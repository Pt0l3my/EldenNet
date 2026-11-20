#pragma once
// The sources (ARCH §8.2, DESIGN §6.2): one interface, EventSource, polled in a fixed order on every step. What v0.1
// has data for is built: SnapshotDiff (the read groups' snapshot against the last processed one, through the world
// model), Lifecycle (what en-core's lifecycle slice changed on the step) and Timer (now crossing each whole second).
// FlagWatch and Pipeline are the seams §8.2 describes, polled in their place and observing nothing until their data
// exists (the flag catalogue and its watch list, v0.2; conversations, v0.2 on); the hook sources H1 to H3 are compiled
// out (EN_HOOKS, §8.11).
#include <bitset>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "en/augur/signals.hpp"
#include "en/augur/types.hpp"
#include "en/proto/snapshot.hpp"

namespace en::augur {

class WorldModel;

// A lifecycle change en-core's slice made on the step (ARCH §9), or a read group that tripped.
struct LifeFact {
  FactKind kind = FactKind::DataReady;
  std::uint8_t value = 0;  // GateChanged: 1 open; GroupTripped: the Group; LadderChanged: the LadderLevel
  PlaythroughKey key;      // PlaythroughReady
  friend constexpr bool operator==(const LifeFact&, const LifeFact&) = default;
};

// One step's input: a processed snapshot (or none), and the lifecycle changes of the same step.
struct AugurInput {
  const proto::WorldSnapshot* snapshot = nullptr;
  std::span<const LifeFact> life;
};

struct SubjectRef {
  enum class Kind : std::uint8_t { Chr, Player, World };
  Kind kind = Kind::World;
  ChrHandle handle;  // Chr
  friend constexpr bool operator==(const SubjectRef&, const SubjectRef&) = default;
};

struct Sample {
  SubjectRef subject;
  SignalId signal = SignalId::HpFrac;
  Value value;
  Mono t = 0;
};

struct Fact {
  enum class Kind : std::uint8_t { Life, TimerFire };
  Kind kind = Kind::Life;
  LifeFact life;
  std::uint64_t second = 0;  // TimerFire: the whole second crossed
  Mono t = 0;
};

struct Observations {
  std::vector<Sample> samples;
  std::vector<Fact> facts;
  void clear() {
    samples.clear();
    facts.clear();
  }
};

class EventSource {
 public:
  virtual ~EventSource() = default;
  [[nodiscard]] virtual SourceId id() const noexcept = 0;
  virtual void poll(const AugurInput& input, Mono now, const WorldModel& world, Observations& out) = 0;
};

// The signals some rule reads: SnapshotDiff samples only those.
using SignalSet = std::bitset<kSignalCount>;

// The sources in their polling order (§8.9 step 2).
[[nodiscard]] std::vector<std::unique_ptr<EventSource>> make_sources(const SignalSet& used);

}  // namespace en::augur
