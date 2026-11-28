// T9, the store's thread (loop.hpp; ARCH §1.2, §1.3's C8; MEM §8.6).
#include "en/store/loop.hpp"

namespace en::store {

namespace db = proto::db;

namespace {

// A request that only reads, or changes nothing in the file: no checkpoint is due after it.
bool writes(const proto::DbRequest& r) {
  return !std::holds_alternative<db::Bundle>(r.op) && !std::holds_alternative<db::JobInput>(r.op) &&
         !std::holds_alternative<db::LivedInput>(r.op) && !std::holds_alternative<db::EventsForFold>(r.op) &&
         !std::holds_alternative<db::Checkpoint>(r.op) && !std::holds_alternative<db::Close>(r.op);
}

}  // namespace

StoreHandle::StoreHandle(StoreCfg cfg, std::function<void()> notify)
    : before_(cfg.before_request), mono_(cfg.mono_ms), core_(std::move(cfg)), notify_(std::move(notify)) {
  thread_ = std::thread([this] { run(); });
}

StoreHandle::~StoreHandle() { stop(); }

void StoreHandle::stop() {
  {
    std::lock_guard<std::mutex> lock(stop_m_);
    stopping_ = true;
  }
  requests_.notify_all();
  results_.notify_all();
  if (thread_.joinable()) thread_.join();
}

bool StoreHandle::stopping() {
  std::lock_guard<std::mutex> lock(stop_m_);
  return stopping_;
}

std::uint64_t StoreHandle::mono() const {
  if (mono_) return mono_();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

proto::Expected<void, Full> StoreHandle::send(proto::DbRequest request) {
  if (!requests_.try_push(request)) return proto::Unexpected{Full{}};
  return {};
}

bool StoreHandle::poll(proto::DbResult& out) { return results_.try_pop(out); }

void StoreHandle::flush_appends(std::vector<proto::DbRequest>& run) {
  const std::function<bool()> stop = [this] { return stopping(); };
  bool together = false;
  if (run.size() > kBurstAppends) {
    std::vector<db::Append> appends;
    appends.reserve(run.size());
    for (const proto::DbRequest& r : run) appends.push_back(std::get<db::Append>(r.op));
    together = core_.append_all(appends).has_value();
  }
  for (proto::DbRequest& r : run) {
    proto::DbResult res = together ? proto::DbResult{r.seq, db::Done{}} : core_.handle(r);
    if (!results_.push_wait(res, stop)) break;
  }
  run.clear();
}

void StoreHandle::housekeeping() {
  const std::uint64_t now = mono();
  if (!dirty_ || !core_.open() || (checkpointed_ && now < *checkpointed_ + kCheckpointGapMs)) return;
  proto::DbRequest req;
  req.seq = 0;
  req.op = db::Checkpoint{};
  proto::DbResult res = core_.handle(req);
  dirty_ = false;
  checkpointed_ = now;
  ++checkpoints_;
  if (std::holds_alternative<db::Tripped>(res.result)) {
    const std::function<bool()> stop = [this] { return stopping(); };
    if (results_.push_wait(res, stop) && notify_) notify_();
  }
}

void StoreHandle::run() {
  const std::function<bool()> stop = [this] { return stopping(); };
  std::vector<proto::DbRequest> appends;
  for (;;) {
    proto::DbRequest req;
    // The timeout paces housekeeping only: every request runs as soon as it arrives.
    if (!requests_.pop_wait(req, std::chrono::milliseconds(250), stop)) {
      if (stop()) return;
      housekeeping();
      continue;
    }
    // Drain what is there: consecutive appends wait for their run's end, everything else runs at once, in order.
    for (;;) {
      if (before_) before_();
      if (writes(req)) dirty_ = true;
      if (std::holds_alternative<db::Append>(req.op)) {
        appends.push_back(std::move(req));
      } else {
        flush_appends(appends);
        proto::DbResult res = core_.handle(req);
        if (!results_.push_wait(res, stop)) return;
      }
      if (!requests_.try_pop(req)) break;
    }
    flush_appends(appends);
    if (notify_) notify_();
    if (stop()) return;
  }
}

std::unique_ptr<StoreHandle> Store::spawn(StoreCfg cfg, std::function<void()> notify) {
  return std::make_unique<StoreHandle>(std::move(cfg), std::move(notify));
}

}  // namespace en::store
