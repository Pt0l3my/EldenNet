// T9's loop (loop.hpp; ARCH §1.3's C8; MEM §8.6): results come back in the order the requests ran; a stalled store
// fills C8 to its 1,024 and the next send answers Full, never blocking, with nothing lost once it drains; a burst
// shares one transaction and a bad append in it fails alone; a quiet moment after writes runs a passive checkpoint.
#include <doctest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "en/store/loop.hpp"
#include "support.hpp"

using namespace en::store;
using namespace en::store::test;
namespace proto = en::proto;
namespace db = en::proto::db;
namespace fs = std::filesystem;

namespace {

// The test's hold on T9: while held, the loop waits before the request it took.
struct Stall {
  std::mutex m;
  std::condition_variable cv;
  bool held = false;
  std::atomic<int> entered{0};
  void hold() {
    std::lock_guard<std::mutex> l(m);
    held = true;
  }
  void release() {
    {
      std::lock_guard<std::mutex> l(m);
      held = false;
    }
    cv.notify_all();
  }
  void before() {
    ++entered;
    std::unique_lock<std::mutex> l(m);
    cv.wait(l, [&] { return !held; });
  }
};

proto::DbRequest request(std::uint64_t seq, proto::DbRequest::Op op) {
  proto::DbRequest r;
  r.seq = seq;
  r.op = std::move(op);
  return r;
}

proto::DbRequest append(std::uint64_t seq, proto::EventId id, proto::Pt t) {
  db::Append a;
  a.event = event(id, "PlaceEntered", t, {{"kale", proto::How::Direct}});
  return request(seq, a);
}

// The next `n` results, waiting up to 30 s in all.
std::vector<proto::DbResult> collect(StoreHandle& h, std::size_t n) {
  std::vector<proto::DbResult> out;
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (out.size() < n && std::chrono::steady_clock::now() < until) {
    proto::DbResult r;
    if (h.poll(r)) {
      out.push_back(std::move(r));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  return out;
}

bool wait_for(const std::function<bool()>& cond) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!cond()) {
    if (std::chrono::steady_clock::now() >= until) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return true;
}

fs::path live_of(const fs::path& saves) {
  const proto::PlaythroughKey k = invented_key();
  return save_folder(saves, k.steamid64).live(k.slot, k.fp8);
}

}  // namespace

TEST_CASE("store loop: a stalled store fills C8 to 1,024; the next send answers Full; nothing is lost after") {
  TempDir dir;
  Stall stall;
  std::atomic<int> woken{0};
  StoreCfg cfg = store_config(dir.path() / "saves");
  cfg.before_request = [&] { stall.before(); };
  auto h = Store::spawn(cfg, [&] { ++woken; });
  REQUIRE(h->send(request(1, db::Open{invented_key(), 60000, 1})).has_value());
  auto opened = collect(*h, 1);
  REQUIRE(opened.size() == 1);
  REQUIRE(std::holds_alternative<db::Opened>(opened[0].result));
  CHECK(wait_for([&] { return woken.load() >= 1; }));  // after the batch it drained
  const proto::EventId first = std::get<db::Opened>(opened[0].result).next_event_id;

  stall.hold();
  const int entered = stall.entered.load();
  REQUIRE(h->send(append(2, first, 61000)).has_value());
  REQUIRE(wait_for([&] { return stall.entered.load() > entered; }));  // T9 holds the first, C8 is empty
  for (std::uint64_t i = 1; i <= kC8Capacity; ++i) {
    REQUIRE(h->send(append(2 + i, first + static_cast<proto::EventId>(i), 61000)).has_value());
  }
  CHECK(h->waiting() == kC8Capacity);
  const auto t0 = std::chrono::steady_clock::now();
  const auto full = h->send(append(9999, first + 5000, 61000));
  CHECK_FALSE(full.has_value());  // Full, at once: the core keeps it in its spill ring
  CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(100));

  stall.release();
  const auto results = collect(*h, kC8Capacity + 1);
  REQUIRE(results.size() == kC8Capacity + 1);
  for (std::size_t i = 0; i < results.size(); ++i) {
    CHECK(results[i].seq == 2 + i);
    CHECK(std::holds_alternative<db::Done>(results[i].result));
  }
  h.reset();
  auto d = Db::open(live_of(dir.path() / "saves"), FileKind::Playthrough, false);
  REQUIRE(d.has_value());
  CHECK(count(*d, "events") == static_cast<std::int64_t>(kC8Capacity + 1));
}

TEST_CASE("store loop: a burst shares a transaction; a bad append fails alone; other requests keep their order") {
  TempDir dir;
  Stall stall;
  StoreCfg cfg = store_config(dir.path() / "saves");
  cfg.before_request = [&] { stall.before(); };
  auto h = Store::spawn(cfg, {});
  REQUIRE(h->send(request(1, db::Open{invented_key(), 60000, 1})).has_value());
  auto opened = collect(*h, 1);
  REQUIRE(opened.size() == 1);
  const proto::EventId first = std::get<db::Opened>(opened[0].result).next_event_id;

  stall.hold();
  const int entered = stall.entered.load();
  std::uint64_t seq = 1;
  REQUIRE(h->send(append(++seq, first, 61000)).has_value());
  REQUIRE(wait_for([&] { return stall.entered.load() > entered; }));
  // 24 appends with one duplicate id among them, then a query, then 3 appends (a run of 16 or fewer: one by one).
  for (proto::EventId i = 1; i < 24; ++i) {
    const proto::EventId id = i == 12 ? first + 5 : first + i;
    REQUIRE(h->send(append(++seq, id, 61000 + i)).has_value());
  }
  REQUIRE(h->send(request(++seq, db::Bundle{1, "kale"})).has_value());
  const std::uint64_t query_seq = seq;
  for (proto::EventId i = 24; i < 27; ++i) REQUIRE(h->send(append(++seq, first + i, 62000 + i)).has_value());
  stall.release();
  const auto results = collect(*h, static_cast<std::size_t>(seq - 1));
  REQUIRE(results.size() == seq - 1);
  int failed = 0;
  for (std::size_t i = 0; i < results.size(); ++i) {
    CHECK(results[i].seq == 2 + i);
    if (const auto* f = std::get_if<db::Failed>(&results[i].result)) {
      ++failed;
      CHECK(results[i].seq == 14);  // the duplicate, sent 13th after the first
      CHECK(f->fault == Fault::Constraint);
    } else if (results[i].seq == query_seq) {
      CHECK(std::holds_alternative<db::BundleResult>(results[i].result));
    } else {
      CHECK(std::holds_alternative<db::Done>(results[i].result));
    }
  }
  CHECK(failed == 1);
  h.reset();
  auto d = Db::open(live_of(dir.path() / "saves"), FileKind::Playthrough, false);
  REQUIRE(d.has_value());
  CHECK(count(*d, "events") == 26);
  // The run that failed together left no counter behind: the one-by-one inserts still wrote it.
  CHECK(value(*d, "SELECT v FROM meta WHERE k = 'next_event_id'") == std::to_string(first + 27));
}

TEST_CASE("store loop: the first quiet 250 ms after writes runs a passive checkpoint, at most once per 10 s") {
  TempDir dir;
  std::atomic<std::uint64_t> mono{0};
  auto h = Store::spawn(store_config(dir.path() / "saves", [&] { return mono.load(); }), {});
  REQUIRE(h->send(request(1, db::Open{invented_key(), 60000, 1})).has_value());
  auto opened = collect(*h, 1);
  REQUIRE(opened.size() == 1);
  const proto::EventId first = std::get<db::Opened>(opened[0].result).next_event_id;
  const fs::path live = live_of(dir.path() / "saves");
  // The first checkpoint is due at once, after the open's writes.
  REQUIRE(wait_for([&] { return h->checkpoints() == 1; }));
  const auto after_first = fs::file_size(live);
  // Few enough frames that SQLite's own checkpoint (at 1,000 pages) stays away, and rows long enough to need pages.
  for (proto::EventId i = 0; i < 40; ++i) {
    proto::DbRequest r = append(2 + static_cast<std::uint64_t>(i), first + i, 61000 + i);
    std::get<db::Append>(r.op).event.text = std::string(500, 'x');
    REQUIRE(h->send(std::move(r)).has_value());
  }
  const auto done = collect(*h, 40);
  REQUIRE(done.size() == 40);
  for (const proto::DbResult& r : done) CHECK(std::holds_alternative<db::Done>(r.result));
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  CHECK(h->checkpoints() == 1);  // within 10 s of the last: not yet
  CHECK(fs::file_size(live) == after_first);
  mono = 10000;
  REQUIRE(wait_for([&] { return h->checkpoints() == 2; }));
  CHECK(fs::file_size(live) > after_first);  // the WAL's pages are in the file
  proto::DbResult none;
  CHECK_FALSE(h->poll(none));  // a checkpoint that went well answers nothing
}
