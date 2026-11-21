// The bus (ARCH §8.9) and the K store (§8.4 item 4): dispatch to every subscriber in order before the next event,
// emitted events at the FIFO's tail, the FIFO's overflow (T before K, never P), the exception policy, the store
// requests' queue with a stalled store, and the merge property of §8.12.
#include <doctest.h>

#include <string>

#include "helpers.hpp"

namespace en::augur {
namespace {

using test::Rig;
using test::snap;

Event ev(Class c, std::uint64_t n) {
  Event e;
  e.event_class = c;
  e.kind = DataReady{};
  e.t_mono_ms = n;
  return e;
}

struct Recorder final : Subscriber {
  explicit Recorder(SubsystemId s, std::vector<std::string>& log) : sub(s), seen(log) {}
  SubsystemId id() const override { return sub; }
  void on_event(const Event& e, BusCtx& ctx) override {
    seen.push_back(std::to_string(static_cast<int>(sub)) + ":" + std::string(kind_name(e.kind)));
    if (throws > 0) {
      --throws;
      throw 1;
    }
    if (sub == SubsystemId::Barks && std::holds_alternative<LoadingStarted>(e.kind)) {
      Event follow;
      follow.kind = TitleReached{};
      follow.event_class = Class::T;
      ctx.emit(follow);
    }
  }
  void reset() override { ++resets; }
  SubsystemId sub;
  std::vector<std::string>& seen;
  int throws = 0;
  int resets = 0;
};

TEST_CASE("dispatch: facts, log, conversation, barks, memory, UI, each event to all before the next") {
  Rig r;
  std::vector<std::string> log;
  Recorder ui(SubsystemId::Ui, log), facts(SubsystemId::Facts, log), barks(SubsystemId::Barks, log);
  Bus bus;
  bus.add(&ui);
  bus.add(&barks);
  bus.add(&facts);
  EventBatch batch;
  batch.push_back(ev(Class::T, 1));
  Event l = ev(Class::T, 2);
  l.kind = LoadingStarted{};
  batch.push_back(l);
  EventBatch out;
  bus.run(std::move(batch), 0, r.augur, r.ctx, out);
  const std::vector<std::string> want = {"0:DataReady",      "3:DataReady",      "5:DataReady",
                                         "0:LoadingStarted", "3:LoadingStarted", "5:LoadingStarted",
                                         "0:TitleReached",   "3:TitleReached",   "5:TitleReached"};
  CHECK(log == want);
  REQUIRE(out.size() == 3);
  CHECK(out[2].source == SourceId::Subscriber);  // emitted, at the FIFO's tail
}

TEST_CASE("the FIFO: past 256, T events go first, then K; P never") {
  BusStats stats;
  std::deque<Event> fifo;
  for (std::uint64_t i = 0; i < 100; ++i) Bus::admit(fifo, ev(Class::T, i), stats);
  for (std::uint64_t i = 0; i < 100; ++i) Bus::admit(fifo, ev(Class::K, i), stats);
  for (std::uint64_t i = 0; i < 56; ++i) Bus::admit(fifo, ev(Class::P, i), stats);
  CHECK(fifo.size() == kFifoCap);
  for (std::uint64_t i = 0; i < 150; ++i) Bus::admit(fifo, ev(Class::P, 1000 + i), stats);
  CHECK(fifo.size() == kFifoCap);
  CHECK(stats.dropped_t == 100);
  CHECK(stats.dropped_k == 50);
  Bus::admit(fifo, ev(Class::T, 9), stats);  // a T at a full FIFO is dropped itself
  CHECK(stats.dropped_t == 101);
  for (std::uint64_t i = 0; i < 60; ++i) Bus::admit(fifo, ev(Class::P, 2000 + i), stats);
  CHECK(stats.dropped_k == 100);
  CHECK(stats.over_cap_p == 10);
  std::size_t p = 0;
  for (const Event& e : fifo) p += e.event_class == Class::P ? 1 : 0;
  CHECK(p == 56 + 150 + 60);  // every P kept
}

TEST_CASE("a subscriber's first exception resets it, the second disables it for the session") {
  Rig r;
  std::vector<std::string> log;
  Recorder memory(SubsystemId::Memory, log), ui(SubsystemId::Ui, log);
  memory.throws = 2;
  Bus bus;
  bus.add(&memory);
  bus.add(&ui);
  for (int i = 0; i < 3; ++i) {
    EventBatch b;
    b.push_back(ev(Class::T, static_cast<std::uint64_t>(i)));
    EventBatch out;
    bus.run(std::move(b), 0, r.augur, r.ctx, out);
    CHECK(out.size() == 1);
  }
  CHECK(memory.resets == 1);
  CHECK(bus.stats().disabled[static_cast<std::size_t>(SubsystemId::Memory)]);
  CHECK_FALSE(bus.stats().disabled[static_cast<std::size_t>(SubsystemId::Ui)]);
  CHECK(std::count(log.begin(), log.end(), "5:DataReady") == 3);  // the others carry on
  CHECK(std::count(log.begin(), log.end(), "4:DataReady") == 2);
}

TEST_CASE("a stalled store: no P event lost up to the ring, the toast, the speech reserve, recovery below 512") {
  PersistQueue q;
  std::uint64_t id = 0;
  for (int i = 0; i < 200; ++i) CHECK(q.push({PersistQueue::Kind::Diagnostic, id++}) == PersistQueue::Push::Queued);
  for (int i = 0; i < 1024 - 200; ++i) CHECK(q.push({PersistQueue::Kind::Event, id++}) == PersistQueue::Push::Queued);
  // Full: each P event takes a diagnostic's place first.
  for (int i = 0; i < 200; ++i) CHECK(q.push({PersistQueue::Kind::Event, id++}) == PersistQueue::Push::Queued);
  CHECK(q.push({PersistQueue::Kind::Diagnostic, id++}) == PersistQueue::Push::DroppedDiagnostic);
  // Then they spill into the RAM ring, up to its non-speech part (4,096 − 256).
  for (int i = 0; i < 3840; ++i) CHECK(q.push({PersistQueue::Kind::Event, id++}) == PersistQueue::Push::Spilled);
  CHECK_FALSE(q.toast());
  CHECK(q.push({PersistQueue::Kind::Event, id++}) == PersistQueue::Push::Paused);
  CHECK(q.toast());
  CHECK(q.not_saved() == 1);
  // Speech still has its reserve; when that is full too, the chat must stop showing pages.
  for (int i = 0; i < 256; ++i) CHECK(q.push({PersistQueue::Kind::Speech, id++}) == PersistQueue::Push::Spilled);
  CHECK(q.speech_blocked());
  CHECK(q.push({PersistQueue::Kind::Speech, id++}) == PersistQueue::Push::Spilled);  // a shown line is never lost
  // The store drains in order; persistence resumes only once the ring is empty and the queue below 512.
  std::uint64_t last = 0;
  std::size_t taken = 0;
  PersistQueue::Item it;
  while (q.toast() && q.pop(it)) {
    CHECK(it.id >= last);
    last = it.id;
    ++taken;
  }
  CHECK(q.queued() < 512);
  CHECK(q.spilled() == 0);
  CHECK(taken == 1024 + 3840 + 257 - q.queued());
  CHECK(q.push({PersistQueue::Kind::Event, id++}) == PersistQueue::Push::Queued);
}

TEST_CASE("property: merge: same-key fires within merge_ms give one entry, count the number of inputs") {
  for (std::uint64_t seed = 1; seed <= 100; ++seed) {
    CAPTURE(seed);
    test::Rng rng(seed);
    KStore k;
    const std::uint32_t merge = 500 + static_cast<std::uint32_t>(rng.below(3000));
    const std::size_t n = 1 + rng.below(12);
    Mono t = 1000;
    std::size_t emitted = 0, salient = 0;
    bool hard = false;
    for (std::size_t i = 0; i < n; ++i) {
      Event e;
      e.event_class = Class::K;
      e.key = std::make_shared<const std::string>("hit:player:#9");
      Struck s;
      s.hard = rng.chance(0.2);
      s.frac_total = 0.01f;
      e.kind = s;
      const bool turns = s.hard && !hard;
      hard = hard || s.hard;
      const KStore::Result r = k.put(e, 3, t, merge, 10);
      if (r.emit) ++emitted;
      if (r.emit && r.merged) {
        ++salient;
        CHECK(turns);
      }
      if (i > 0 && !turns) CHECK_FALSE(r.emit);
      t += rng.below(merge / static_cast<Mono>(n) + 1);
    }
    REQUIRE(k.entries().size() == 1);
    CHECK(k.entries()[0].event.count == n);
    CHECK(std::get<Struck>(k.entries()[0].event.kind).count == n);
    CHECK(emitted == 1 + salient);
    k.expire(t + 10000);
    CHECK(k.entries().empty());
  }
}

TEST_CASE("the K store's cap: the earliest-expiring entry goes first; dedup and cooldown tables") {
  KStore k;
  for (std::size_t i = 0; i < kMaxKEntries + 5; ++i) {
    Event e;
    e.event_class = Class::K;
    e.key = std::make_shared<const std::string>("k" + std::to_string(i));
    e.kind = LowHp{};
    (void)k.put(e, 0, 1000 + i, 0, static_cast<std::uint32_t>(1 + (i % 7)));
  }
  CHECK(k.entries().size() == kMaxKEntries);
  DedupTable d;
  CHECK_FALSE(d.duplicate("p", 1000, 5));
  CHECK(d.duplicate("p", 5999, 5));
  CHECK_FALSE(d.duplicate("p", 6000, 5));
  CooldownTable c;
  CHECK_FALSE(c.cooling(1, 7, 1000, 2));
  CHECK(c.cooling(1, 7, 2999, 2));
  CHECK_FALSE(c.cooling(1, 8, 2999, 2));
  CHECK_FALSE(c.cooling(1, 7, 3000, 2));
}

}  // namespace
}  // namespace en::augur
