#include "fake_transport.hpp"

#include <algorithm>
#include <thread>

namespace en::llm::test {

using nlohmann::json;

std::string sse_data(const json& chunk) { return "data: " + chunk.dump() + "\n\n"; }

json delta_chunk(const std::string& content) {
  return json{{"id", "fake"}, {"model", "served"}, {"choices", json::array({{{"index", 0}, {"delta", {{"content", content}}}}})}};
}

FakeScript sse_reply(const std::vector<std::string>& deltas, const std::string& finish, Millis first, Millis gap,
                     const json& usage) {
  FakeScript s;
  s.push_back(FakeStep{Millis(0), TransportEventKind::Head, 200, {{"content-type", "text/event-stream"}}, "", ""});
  Millis at = first;
  for (const std::string& d : deltas) {
    s.push_back(FakeStep{at, TransportEventKind::Data, 200, {}, sse_data(delta_chunk(d)), ""});
    at += gap;
  }
  json last = {{"choices", json::array({{{"index", 0}, {"delta", json::object()}, {"finish_reason", finish}}})}};
  if (!usage.is_null()) last["usage"] = usage;
  s.push_back(FakeStep{at, TransportEventKind::Data, 200, {}, sse_data(last) + "data: [DONE]\n\n", ""});
  s.push_back(FakeStep{at, TransportEventKind::Done, 200, {}, "", ""});
  return s;
}

FakeScript sse_raw(const std::vector<std::pair<Millis, std::string>>& pieces, Millis end_at) {
  FakeScript s;
  s.push_back(FakeStep{Millis(0), TransportEventKind::Head, 200, {{"content-type", "text/event-stream"}}, "", ""});
  for (const auto& [at, text] : pieces) s.push_back(FakeStep{at, TransportEventKind::Data, 200, {}, text, ""});
  s.push_back(FakeStep{end_at, TransportEventKind::Done, 200, {}, "", ""});
  return s;
}

FakeScript http_error(int status, const std::string& body, std::vector<HttpHeader> headers, Millis at) {
  headers.push_back({"content-type", "application/json"});
  return {FakeStep{at, TransportEventKind::Head, status, headers, "", ""},
          FakeStep{at, TransportEventKind::Data, status, {}, body, ""},
          FakeStep{at, TransportEventKind::Done, status, {}, "", ""}};
}

FakeScript hang(Millis head_at) {
  return {FakeStep{head_at, TransportEventKind::Head, 200, {{"content-type", "text/event-stream"}}, "", ""}};
}

FakeScript connection_failure(Millis at, const std::string& error) {
  return {FakeStep{at, TransportEventKind::Failed, 0, {}, "", error}};
}

struct FakeServer::State {
  std::shared_ptr<ManualClock> clock;
  mutable std::mutex mutex;
  std::map<std::string, std::deque<FakeScript>> scripts;
  std::vector<Recorded> recorded;
  int next_exchange = 1;
  bool auto_advance = true;
  std::size_t active = 0;
};

namespace {

class FakeSession final : public Transport {
 public:
  explicit FakeSession(std::shared_ptr<FakeServer::State> state) : s_(std::move(state)) {}

  ~FakeSession() override {
    std::lock_guard lock(s_->mutex);
    for (const Running& r : running_) finish(r, true);
  }

  int start(const HttpRequest& request) override {
    std::lock_guard lock(s_->mutex);
    Recorded rec;
    rec.exchange = s_->next_exchange++;
    rec.method = request.method;
    rec.url = request.url;
    rec.headers = request.headers;
    for (const auto& [name, value] : request.secret_headers) {
      if (name == "Authorization") rec.authorization = value.reveal();
    }
    rec.body = json::parse(request.body, nullptr, false);
    if (rec.body.is_object()) rec.model = rec.body.value("model", "");
    rec.started = s_->clock->now();
    FakeScript script;
    if (!take(rec.model, script) && !take("*", script)) {
      script = {FakeStep{Millis(0), TransportEventKind::Failed, 0, {}, "", "no script for model " + rec.model}};
    }
    running_.push_back(Running{rec.exchange, std::move(script), 0, rec.started, s_->recorded.size()});
    s_->recorded.push_back(std::move(rec));
    ++s_->active;
    return running_.back().id;
  }

  void cancel(int exchange) override {
    std::lock_guard lock(s_->mutex);
    auto it = std::find_if(running_.begin(), running_.end(), [&](const Running& r) { return r.id == exchange; });
    if (it == running_.end()) return;
    finish(*it, true);
    running_.erase(it);
  }

  void wait(Millis max_wait, std::vector<TransportEvent>& out) override {
    std::unique_lock lock(s_->mutex);
    if (collect(out)) return;
    if (s_->auto_advance) {
      TimePoint target = s_->clock->now() + max_wait;
      if (auto next = next_due()) target = std::min(target, *next);
      s_->clock->advance_to(target);
    } else {
      lock.unlock();
      std::this_thread::sleep_for(std::min(max_wait, Millis(2)));
      lock.lock();
    }
    collect(out);
  }

 private:
  struct Running {
    int id = 0;
    FakeScript script;
    std::size_t next = 0;
    TimePoint start;
    std::size_t record = 0;
  };

  bool take(const std::string& model, FakeScript& script) {
    auto it = s_->scripts.find(model);
    if (it == s_->scripts.end() || it->second.empty()) return false;
    script = std::move(it->second.front());
    it->second.pop_front();
    return true;
  }

  std::optional<TimePoint> next_due() const {
    std::optional<TimePoint> next;
    for (const Running& r : running_) {
      if (r.next < r.script.size()) {
        const TimePoint at = r.start + r.script[r.next].at;
        if (!next || at < *next) next = at;
      }
    }
    return next;
  }

  void finish(const Running& r, bool cancelled) {
    Recorded& rec = s_->recorded[r.record];
    rec.cancelled = cancelled;
    rec.finished = !cancelled;
    rec.ended = s_->clock->now();
    --s_->active;
  }

  // Delivers every step that is due, in time order; returns whether there were any.
  bool collect(std::vector<TransportEvent>& out) {
    const TimePoint now = s_->clock->now();
    bool any = false;
    while (true) {
      Running* earliest = nullptr;
      for (Running& r : running_) {
        if (r.next >= r.script.size() || r.start + r.script[r.next].at > now) continue;
        if (earliest == nullptr || r.start + r.script[r.next].at < earliest->start + earliest->script[earliest->next].at) {
          earliest = &r;
        }
      }
      if (earliest == nullptr) return any;
      const FakeStep& step = earliest->script[earliest->next++];
      TransportEvent event;
      event.exchange = earliest->id;
      event.kind = step.kind;
      event.status = step.status;
      event.headers = step.headers;
      event.data = step.data;
      event.error = step.error;
      out.push_back(std::move(event));
      any = true;
      if (step.kind == TransportEventKind::Done || step.kind == TransportEventKind::Failed) {
        finish(*earliest, false);
        const int id = earliest->id;
        running_.erase(std::find_if(running_.begin(), running_.end(), [&](const Running& r) { return r.id == id; }));
      }
    }
  }

  std::shared_ptr<FakeServer::State> s_;
  std::vector<Running> running_;
};

}  // namespace

FakeServer::FakeServer(std::shared_ptr<ManualClock> clock) : state_(std::make_shared<State>()) {
  state_->clock = std::move(clock);
}

void FakeServer::script(const std::string& model, FakeScript script) {
  std::lock_guard lock(state_->mutex);
  state_->scripts[model].push_back(std::move(script));
}

std::unique_ptr<Transport> FakeServer::open() { return std::make_unique<FakeSession>(state_); }

std::vector<Recorded> FakeServer::requests() const {
  std::lock_guard lock(state_->mutex);
  return state_->recorded;
}

std::size_t FakeServer::active() const {
  std::lock_guard lock(state_->mutex);
  return state_->active;
}

void FakeServer::set_auto_advance(bool on) {
  std::lock_guard lock(state_->mutex);
  state_->auto_advance = on;
}

}  // namespace en::llm::test
