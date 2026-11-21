#pragma once
// The bus (ARCH §8.9, DESIGN §6.6): step 5 of the order inside one step, the step FIFO, and the backpressure policy of
// the queues Augur's events feed.
//
// - Dispatch: each event goes to every subscriber, facts → log → conversation → barks → memory → UI/debug, before the
//   next event starts. Events a subscriber emits (BusCtx::emit, which resolves their audience) join the tail of the
//   same step's FIFO, so every event of snapshot n is delivered before snapshot n+1 is processed.
// - The FIFO holds at most 256 events: past it, T events are dropped first, then K events, and the drops are counted
//   (en-core logs them); P events are never dropped.
// - Each subscriber call is wrapped in a catch-all handler: the first exception resets that subsystem, the second
//   disables it for the session (R8). Augur's own code throws nothing.
// - PersistQueue is the store requests' policy (§8.9's table): what the memory subscriber hands the store, once the
//   store exists (v0.3), with its RAM ring for P events and its speech reserve.
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "en/augur/events.hpp"
#include "en/augur/types.hpp"

namespace en::augur {

class Augur;
class AugurCtx;

enum class SubsystemId : std::uint8_t { Facts, Log, Conversation, Barks, Memory, Ui };
inline constexpr std::size_t kSubsystemCount = 6;
inline constexpr std::size_t kFifoCap = 256;

class BusCtx {
 public:
  BusCtx(Augur& augur, const AugurCtx& ctx, Mono now) : augur_(augur), ctx_(ctx), now_(now) {}
  // An event a subscriber emits: its id and audience are given here, and it joins the step's FIFO.
  void emit(Event ev);
  [[nodiscard]] Mono now() const noexcept { return now_; }
  [[nodiscard]] const AugurCtx& ctx() const noexcept { return ctx_; }
  [[nodiscard]] std::vector<Event>& emitted() noexcept { return emitted_; }

 private:
  Augur& augur_;
  const AugurCtx& ctx_;
  Mono now_;
  std::vector<Event> emitted_;
};

class Subscriber {
 public:
  virtual ~Subscriber() = default;
  [[nodiscard]] virtual SubsystemId id() const = 0;
  virtual void on_event(const Event& ev, BusCtx& ctx) = 0;  // may emit events (and push the core's outputs)
  virtual void on_step_end(Mono /*now*/, BusCtx& /*ctx*/) {}
  virtual void reset() {}  // the first exception: start the subsystem again
};

struct BusStats {
  std::uint64_t dropped_t = 0;
  std::uint64_t dropped_k = 0;
  std::uint64_t over_cap_p = 0;  // P events kept past the cap (they are never dropped)
  std::array<std::uint8_t, kSubsystemCount> exceptions{};
  std::array<bool, kSubsystemCount> disabled{};
};

class Bus {
 public:
  // Subscribers are called in SubsystemId's order, whatever order they were added in. Not owned.
  void add(Subscriber* s);
  // Dispatches the step's batch, and what the subscribers emit, through the FIFO. `dispatched` receives every event
  // delivered, in order.
  void run(EventBatch&& batch, Mono now, Augur& augur, const AugurCtx& ctx, EventBatch& dispatched);
  [[nodiscard]] const BusStats& stats() const noexcept { return stats_; }

  // The FIFO's admission rule, alone (for tests and run): adds `ev` to `fifo` (from `next`, the first not yet
  // dispatched), dropping a T, else a K event past the cap, never a P one.
  static void admit(std::deque<Event>& fifo, Event ev, BusStats& stats);

 private:
  std::vector<Subscriber*> subs_;
  BusStats stats_;
};

// The store requests' queue (§8.9's table; DESIGN §6.6). Bound 1,024: diagnostics are dropped first. P events past it
// spill into a RAM ring of 4,096, 256 of them reserved for speech. When the ring's non-speech part is full, persisting
// non-speech P events pauses, with the toast "Memory is behind; recent events are not being saved", until the queue
// drains below 512 (impl); when the speech reserve is full, the chat stops showing new pages (they wait), so a shown
// line can never be lost (impl).
class PersistQueue {
 public:
  enum class Kind : std::uint8_t { Diagnostic, Event, Speech };
  struct Item {
    Kind kind = Kind::Event;
    std::uint64_t id = 0;
  };
  struct Limits {
    std::size_t queue = 1024;
    std::size_t ring = 4096;
    std::size_t speech_reserve = 256;
    std::size_t resume_below = 512;
  };
  enum class Push : std::uint8_t { Queued, Spilled, DroppedDiagnostic, Paused };

  PersistQueue() = default;
  explicit PersistQueue(Limits l) : limits_(l) {}

  // What became of the item: queued, spilled into the ring, a diagnostic dropped, or (non-speech P past the ring, or
  // any non-speech P while paused) not saved, with the toast up.
  Push push(Item item);
  // The store takes the oldest item: the queue first, then the ring refills it in order.
  [[nodiscard]] bool pop(Item& out);

  [[nodiscard]] bool toast() const noexcept { return paused_; }
  // The chat must not show a new page: the ring, its speech reserve included, is full.
  [[nodiscard]] bool speech_blocked() const noexcept { return ring_.size() >= limits_.ring; }
  [[nodiscard]] std::size_t queued() const noexcept { return queue_.size(); }
  [[nodiscard]] std::size_t spilled() const noexcept { return ring_.size(); }
  [[nodiscard]] std::uint64_t not_saved() const noexcept { return not_saved_; }

 private:
  void refill();
  Limits limits_;
  std::deque<Item> queue_;
  std::deque<Item> ring_;
  std::size_t ring_speech_ = 0;
  bool paused_ = false;
  std::uint64_t not_saved_ = 0;
};

}  // namespace en::augur
