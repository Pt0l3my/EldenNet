// Augur's words (words.hpp): the golden form's vocabulary, and an event's reason at a glance.
#include "en/augur/words.hpp"

#include <array>
#include <cstddef>

namespace en::augur {

namespace {

template <std::size_t N>
const char* word(const std::array<const char*, N>& words, std::size_t i) noexcept {
  return i < N ? words[i] : "?";
}

template <class E, std::size_t N>
const char* word_of(const std::array<const char*, N>& words, E v) noexcept {
  return word(words, static_cast<std::size_t>(v));
}

constexpr std::array<const char*, 3> kClasses = {"P", "K", "T"};
constexpr std::array<const char*, 3> kHows = {"direct", "overheard", "witnessed"};
constexpr std::array<const char*, 5> kRadii = {"speech", "witness", "death", "combat", "none"};
constexpr std::array<const char*, 7> kSources = {"lifecycle", "snapshot_diff", "flag_watch", "pipeline",
                                                 "timer",     "hook",          "subscriber"};
constexpr std::array<const char*, 4> kActors = {"player", "persona", "card", "mute"};
constexpr std::array<const char*, 3> kSides = {"tarnished", "card", "team"};
constexpr std::array<const char*, 3> kOutcomes = {"Victory", "Death", "Left"};
constexpr std::array<const char*, 4> kVias = {"Hit", "LockClosing", "Near", "LockOnly"};
constexpr std::array<const char*, 2> kWhys = {"Timeout", "Distance"};
constexpr std::array<const char*, 5> kLevels = {"Starting", "L1", "L2", "L3", "L4"};

static_assert(kSources.size() == static_cast<std::size_t>(SourceId::Subscriber) + 1);
static_assert(kRadii.size() == static_cast<std::size_t>(Radius::None) + 1);
static_assert(kLevels.size() == static_cast<std::size_t>(LadderLevel::L4) + 1);

template <class... F>
struct Overloaded : F... {
  using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>;

constexpr Reason said(const char* w) noexcept { return Reason{w, false, 0}; }
constexpr Reason counted(const char* w, std::uint64_t n) noexcept { return Reason{w, true, n}; }

}  // namespace

const char* to_string(Class v) noexcept { return word_of(kClasses, v); }
const char* to_string(How v) noexcept { return word_of(kHows, v); }
const char* to_string(Radius v) noexcept { return word_of(kRadii, v); }
const char* to_string(SourceId v) noexcept { return word_of(kSources, v); }
const char* to_string(ActorRef::Kind v) noexcept { return word_of(kActors, v); }
const char* to_string(SideId::Kind v) noexcept { return word_of(kSides, v); }
const char* to_string(BossOutcome v) noexcept { return word_of(kOutcomes, v); }
const char* to_string(EngageVia v) noexcept { return word_of(kVias, v); }
const char* to_string(DisengageWhy v) noexcept { return word_of(kWhys, v); }
const char* to_string(LadderLevel v) noexcept { return word_of(kLevels, v); }

const char* to_string(DetectorKind v) noexcept {
  switch (v) {
    case DetectorKind::RisingEdge: return "rising_edge";
    case DetectorKind::FallingEdge: return "falling_edge";
    case DetectorKind::ChangedStable: return "changed_stable";
    case DetectorKind::CrossesBelow: return "crosses_below";
    case DetectorKind::CrossesAbove: return "crosses_above";
    case DetectorKind::DropOverWindow: return "drop_over_window";
    case DetectorKind::RiseOverWindow: return "rise_over_window";
    case DetectorKind::EnterRadius: return "enter_radius";
    case DetectorKind::CountIncrement: return "count_increment";
    case DetectorKind::Engagement: return "engagement";
    case DetectorKind::Fact: return "fact";
  }
  return "?";
}

const char* kind_word(const EventKind& k) noexcept {
  // kKindNames holds views of string literals, so each name's data is NUL-terminated.
  return k.index() < kKindNames.size() ? kKindNames[k.index()].data() : "?";
}

Reason reason(const Event& ev) noexcept {
  return std::visit(
      Overloaded{
          [](const PlaceEntered& e) { return counted("place", e.place_id); },
          [](const BossFightStarted& e) { return counted("bars", e.bars.size()); },
          [](const BossFightEnded& e) { return said(to_string(e.outcome)); },
          [](const NativeTalkStarted& e) {
            if (e.signals.popup) return said("popup");
            if (e.signals.talk_cam) return said("talk_cam");
            return said(e.signals.popup_menu ? "popup_menu" : nullptr);
          },
          [](const NativeTalkEnded& e) { return counted("held_ms", e.held_ms); },
          [](const PlayerDied& e) { return said(e.in_boss ? "in_boss" : e.killer ? "killed" : "died"); },
          [](const MountChanged& e) { return said(e.riding ? "mounted" : "dismounted"); },
          [](const Struck& e) { return said(e.unseen ? "unseen" : e.hard ? "hard" : "hit"); },
          [](const Engaged& e) { return said(to_string(e.via)); },
          [](const Disengaged& e) { return said(e.fled ? "fled" : to_string(e.why)); },
          [](const AllyDied& e) { return counted("witnesses", e.witnesses.size()); },
          [](const PlaythroughReady& e) { return counted("slot", e.key.slot); },
          [](const JourneyChanged& e) { return counted("ng", e.ng_lvl); },
          [](const GateChanged& e) { return said(e.active ? "active" : "inactive"); },
          [](const GroupTripped& e) { return said(proto::to_string(e.group)); },
          [](const LadderChanged& e) { return said(to_string(e.to)); },
          [](const auto&) { return Reason{}; },
      },
      ev.kind);
}

}  // namespace en::augur
