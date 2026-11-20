#pragma once
// Augur, the event engine (ARCH §8, DESIGN §6): snapshot diffs, lifecycle changes and time in, typed events with frozen
// audiences out. Pure: no I/O, no clock (time comes in as Mono), no thread, no exception of its own; it runs first in
// every Core::step. It is below en-core in the graph and game-neutral: what en-core knows (who a character is, the
// predicates, who may perceive, the seed) comes through AugurCtx, and what the game's values mean through WorldFacts.
//
// Augur::step follows §8.9's order: (1) the world model takes the input; (2) the sources are polled in their fixed
// order, Lifecycle, SnapshotDiff, FlagWatch, Pipeline, Timer; (3) each source's fires are sorted by (priority, subject
// distance, handle) and go through the modifiers (§8.4: debounce in the detector, then requires, cooldown, class
// handling, confidence); (4) the audiences are resolved and frozen. Step 5, the dispatch to the subscribers, is the
// bus's (bus.hpp).
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "en/augur/events.hpp"
#include "en/augur/kstore.hpp"
#include "en/augur/rules.hpp"
#include "en/augur/sources.hpp"
#include "en/augur/types.hpp"
#include "en/augur/world.hpp"
#include "en/proto/snapshot.hpp"

namespace en::augur {

// Where a `requires` is evaluated: the event being emitted.
struct EvalScope {
  const Event* event = nullptr;
};

// What en-core supplies (ARCH §8.1).
class AugurCtx {
 public:
  // Persona{id, stage_muted, chat}, Card{id, variant}, Mute or Player.
  [[nodiscard]] virtual Subject classify(const proto::Chr& chr) const = 0;
  // A compiled `requires` (DESIGN §10.4), evaluated at emission.
  [[nodiscard]] virtual bool eval(PredicateId pred, const EvalScope& at) const = 0;
  // Alive and not muted at the current stage (and conscious, MEM §1.2).
  [[nodiscard]] virtual bool persona_may_perceive(PersonaId id) const = 0;
  // Deterministic: the same event and salt give the same number for the same seed.
  [[nodiscard]] virtual std::uint64_t rng(EventId ev, std::uint32_t salt) const = 0;

 protected:
  ~AugurCtx() = default;
};

// What the counters say about the last steps (the debug view, the tests).
struct AugurStats {
  std::uint64_t steps = 0;
  std::uint64_t fires = 0;          // fires that matured
  std::uint64_t refused = 0;        // a payload builder refused (no witnesses, no place)
  std::uint64_t by_requires = 0;    // suppressed by `requires`
  std::uint64_t by_cooldown = 0;    // suppressed by the cooldown
  std::uint64_t by_dedup = 0;       // a P event within its dedup window
  std::uint64_t merged = 0;         // K fires coalesced into an entry
  std::uint64_t emitted = 0;
};

class Augur {
 public:
  explicit Augur(WorldFacts facts = {});

  // Replaces the rule set. RuleSet::load refuses any set with an error, so the caller keeps the old one then (§8.5);
  // here every detector, cooldown and deferral starts again.
  void set_rules(std::shared_ptr<const RuleSet> rules);
  [[nodiscard]] const std::shared_ptr<const RuleSet>& rules() const noexcept { return rules_; }

  // One step. The events are appended to `out` in dispatch order, with their ids, audiences and interrupts.
  void step(const AugurInput& input, Mono now, const AugurCtx& ctx, EventBatch& out);

  // For events subscribers emit (BusCtx::emit): the same resolution, against the world as it stands.
  void resolve_audience(Event& ev, const AugurCtx& ctx) const;
  // The next id of this step, for an event a subscriber emits.
  [[nodiscard]] EventId next_id() noexcept { return EventId{step_, next_index_++}; }

  [[nodiscard]] const KStore& kstore() const noexcept { return kstore_; }
  [[nodiscard]] const WorldModel& world() const noexcept { return world_; }
  [[nodiscard]] const AugurStats& stats() const noexcept { return stats_; }

 private:
  // A fire that matured, before the modifiers that follow debounce.
  struct Pending {
    std::size_t rule = 0;
    SourceId source = SourceId::SnapshotDiff;
    SubjectRef subject;
    Fire fire;
    float distance = 0;
    std::uint64_t handle = 0;
    std::optional<EngTransition> transition;
    std::optional<LifeFact> fact;
    std::optional<BossOutcome> outcome;  // a deferred boss outcome, once resolved
    std::uint32_t fight = 0;
  };
  // A boss outcome waiting for its evidence, or for its window to end (§8.5).
  struct Deferred {
    Pending pending;
    Mono deadline = 0;
  };
  void evaluate(const Observations& obs, Mono now, const AugurCtx& ctx, std::vector<Pending>& fires);
  void emit_batch(std::vector<Pending>& fires, Mono now, const AugurCtx& ctx, EventBatch& out);
  bool finish(Pending& p, Mono now, const AugurCtx& ctx, EventBatch& out);
  void resolve_deferred(Mono now, bool timer, const AugurCtx& ctx, EventBatch& out);
  Event start_event(const Rule& rule, const Pending& p, Mono now) const;

  std::shared_ptr<const RuleSet> rules_;
  std::array<std::vector<std::size_t>, kSignalCount> by_signal_{};
  WorldModel world_;
  KStore kstore_;
  CooldownTable cooldowns_;
  DedupTable dedup_;
  std::vector<std::unique_ptr<EventSource>> sources_;
  std::vector<Deferred> deferred_;
  AugurStats stats_;
  std::uint64_t step_ = 0;
  std::uint16_t next_index_ = 0;
};

}  // namespace en::augur
