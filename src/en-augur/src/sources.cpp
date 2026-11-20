// The sources (sources.hpp; ARCH §8.2).
#include "en/augur/sources.hpp"

#include "en/augur/world.hpp"

namespace en::augur {

namespace {

// en-core's lifecycle changes of the step, as facts.
class LifecycleSource final : public EventSource {
 public:
  SourceId id() const noexcept override { return SourceId::Lifecycle; }
  void poll(const AugurInput& input, Mono now, const WorldModel&, Observations& out) override {
    for (const LifeFact& f : input.life) out.facts.push_back(Fact{Fact::Kind::Life, f, 0, now});
  }
};

// The new snapshot against the last processed one, which the world model keeps (it has taken this snapshot in step
// 1): a sample of every signal some rule reads, for every subject present.
class SnapshotDiff final : public EventSource {
 public:
  explicit SnapshotDiff(const SignalSet& used) : used_(used) {}
  SourceId id() const noexcept override { return SourceId::SnapshotDiff; }
  void poll(const AugurInput& input, Mono, const WorldModel& world, Observations& out) override {
    if (input.snapshot == nullptr) return;
    const Mono t = input.snapshot->mono_ms;
    for (const SignalInfo& s : kSignals) {
      if (!used_.test(static_cast<std::size_t>(s.id)) || s.type == SignalType::Fact) continue;
      if ((s.subjects & kWorld) != 0) {
        out.samples.push_back(Sample{SubjectRef{SubjectRef::Kind::World, {}}, s.id, world.sample_world(s.id), t});
      }
      if ((s.subjects & kPlayer) != 0 && world.player().present) {
        out.samples.push_back(Sample{SubjectRef{SubjectRef::Kind::Player, {}}, s.id, world.sample_player(s.id), t});
      }
      if ((s.subjects & kChr) != 0) {
        for (const Tracker& tr : world.trackers()) {
          if (!tr.present) continue;
          out.samples.push_back(Sample{SubjectRef{SubjectRef::Kind::Chr, tr.handle}, s.id, world.sample(s.id, tr), t});
        }
      }
    }
  }

 private:
  SignalSet used_;
};

// The flag frames against the catalogue's watch list (v0.2): a seam. Without a catalogue there is no watch list, so
// every frame's version differs from the core's (none) and is ignored, as §8.2 has it.
class FlagWatch final : public EventSource {
 public:
  SourceId id() const noexcept override { return SourceId::FlagWatch; }
  void poll(const AugurInput&, Mono, const WorldModel&, Observations&) override {}
};

// Conversation and UI outputs (Submit, CloseChat) and the events subscribers emit: a seam until the chat exists.
class Pipeline final : public EventSource {
 public:
  SourceId id() const noexcept override { return SourceId::Pipeline; }
  void poll(const AugurInput&, Mono, const WorldModel&, Observations&) override {}
};

// now crossing each whole second: one fact per second crossed (at most 60 at once, impl).
class Timer final : public EventSource {
 public:
  SourceId id() const noexcept override { return SourceId::Timer; }
  void poll(const AugurInput&, Mono now, const WorldModel&, Observations& out) override {
    const std::uint64_t second = now / 1000;
    if (started_ && second > last_) {
      const std::uint64_t from = second - last_ > 60 ? second - 60 : last_ + 1;
      for (std::uint64_t s = from; s <= second; ++s) out.facts.push_back(Fact{Fact::Kind::TimerFire, {}, s, now});
    }
    if (!started_ || second > last_) last_ = second;
    started_ = true;
  }

 private:
  bool started_ = false;
  std::uint64_t last_ = 0;
};

}  // namespace

std::vector<std::unique_ptr<EventSource>> make_sources(const SignalSet& used) {
  std::vector<std::unique_ptr<EventSource>> out;
  out.push_back(std::make_unique<LifecycleSource>());
  out.push_back(std::make_unique<SnapshotDiff>(used));
  out.push_back(std::make_unique<FlagWatch>());
  out.push_back(std::make_unique<Pipeline>());
  out.push_back(std::make_unique<Timer>());
#if defined(EN_HOOKS)
#error "the hook sources H1 to H3 are reserved (ARCH §8.11): each needs its spike and the owner's decision first"
#endif
  return out;
}

}  // namespace en::augur
