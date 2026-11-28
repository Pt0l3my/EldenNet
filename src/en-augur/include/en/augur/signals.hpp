#pragma once
// The signal registry (ARCH §8.3): code-defined, so adding a signal takes code. Each signal has a type (which detectors
// it takes, §8.5), the subjects it is sampled for, and how it is derived (world.cpp). A signal may be "not sampled":
// then its detectors see nothing at that snapshot (hp_frac when hp.max is 0; the signals that wait for a spike).
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace en::augur {

enum class SignalType : std::uint8_t {
  Bool,        // edges, changed_stable
  Number,      // crossings, windows (enter_radius on dist_m alone)
  Integer,     // count_increment, changed_stable
  Id,          // changed_stable: an identity or a level, optional (none is a value of its own)
  Enum,        // changed_stable
  Engagement,  // the engagement machine (§8.7): its value is the confidence
  Fact,        // discrete facts (the Lifecycle source)
};

// Who a signal is sampled for. A rule's subject kinds (generic, persona, player, world) must be among them: generic and
// persona both mean a character (chr).
enum class SubjectSet : std::uint8_t { None = 0, Chr = 1, Player = 2, World = 4 };

enum class SignalId : std::uint8_t {
  HpFrac,
  HpDelta,
  Dead,
  TeamHostile,
  TeamTurned,
  DistM,
  OnScreen,
  LockedByPlayer,
  HitByPlayer,
  HitPlayer,
  Engaged,
  Throw,
  CritByPlayer,
  IdleDue,
  Riding,
  Crouching,
  Invisible,
  Roundtable,
  PlaceId,
  BlockArea,
  Deaths,
  NgLvl,
  InCombat,
  Banner,
  Hud,
  TalkPopup,
  TalkCam,
  NativeTalk,
  BossActive,
  Lifecycle,
  TalkingWith,
};
inline constexpr std::size_t kSignalCount = static_cast<std::size_t>(SignalId::TalkingWith) + 1;

struct SignalInfo {
  SignalId id;
  std::string_view name;
  SignalType type;
  std::uint8_t subjects;  // SubjectSet bits
  bool sampled;           // false: registered but never sampled in this build (it waits for a spike's verification)
};

inline constexpr std::uint8_t kChr = 1, kPlayer = 2, kWorld = 4;

// ARCH §8.3's table, in its order, with the additions of this build (marked in the README): crit_by_player (the
// Tarnished's throw 3 while the character's is 4, for CritLanded), native_talk (talk_popup or talk_cam, for
// NativeTalkStarted and Ended) and lifecycle (the Lifecycle source's facts). clock_bucket and flag:<alias> are not
// registered yet: the seven words' hours are not set down, and flags need the catalogue and FlagWatch (v0.2).
inline constexpr std::array<SignalInfo, kSignalCount> kSignals = {{
    {SignalId::HpFrac, "hp_frac", SignalType::Number, kChr | kPlayer, true},
    {SignalId::HpDelta, "hp_delta", SignalType::Number, kChr | kPlayer, true},
    {SignalId::Dead, "dead", SignalType::Bool, kChr | kPlayer, true},
    {SignalId::TeamHostile, "team_hostile", SignalType::Bool, kChr, true},
    {SignalId::TeamTurned, "team_turned", SignalType::Bool, kChr, false},  // S7
    {SignalId::DistM, "dist_m", SignalType::Number, kChr, true},
    {SignalId::OnScreen, "on_screen", SignalType::Bool, kChr, true},
    {SignalId::LockedByPlayer, "locked_by_player", SignalType::Bool, kChr, true},
    {SignalId::HitByPlayer, "hit_by_player", SignalType::Bool, kChr, true},
    {SignalId::HitPlayer, "hit_player", SignalType::Bool, kChr, true},
    {SignalId::Engaged, "engaged", SignalType::Engagement, kChr, true},
    {SignalId::Throw, "throw", SignalType::Enum, kChr | kPlayer, true},
    {SignalId::CritByPlayer, "crit_by_player", SignalType::Bool, kChr, true},
    {SignalId::IdleDue, "idle_due", SignalType::Bool, kChr, false},  // IdleNear's cards (v0.4)
    {SignalId::Riding, "riding", SignalType::Bool, kPlayer, true},
    {SignalId::Crouching, "crouching", SignalType::Bool, kPlayer, false},  // S7
    {SignalId::Invisible, "invisible", SignalType::Bool, kPlayer, false},  // S8
    {SignalId::Roundtable, "roundtable", SignalType::Bool, kPlayer, true},
    {SignalId::PlaceId, "place_id", SignalType::Id, kPlayer, true},
    {SignalId::BlockArea, "block_area", SignalType::Id, kPlayer, true},
    {SignalId::Deaths, "deaths", SignalType::Integer, kPlayer, true},
    {SignalId::NgLvl, "ng_lvl", SignalType::Id, kPlayer, true},
    {SignalId::InCombat, "in_combat", SignalType::Bool, kPlayer, true},
    {SignalId::Banner, "banner", SignalType::Id, kWorld, true},
    {SignalId::Hud, "hud", SignalType::Enum, kWorld, true},
    {SignalId::TalkPopup, "talk_popup", SignalType::Bool, kWorld, true},
    {SignalId::TalkCam, "talk_cam", SignalType::Bool, kWorld, true},
    {SignalId::NativeTalk, "native_talk", SignalType::Bool, kWorld, true},
    {SignalId::BossActive, "boss_active", SignalType::Bool, kWorld, true},
    {SignalId::Lifecycle, "lifecycle", SignalType::Fact, kWorld, true},
    // v0.3: the persona a native talk is with, the nearest within 3 m while the popup's talk id, the talk camera or the
    // HUD's pop-up menu is up (DESIGN §6.5; S3 picks the signals), for the P NativeTalkStarted and Ended with it.
    {SignalId::TalkingWith, "talking_with", SignalType::Bool, kChr, true},
}};

[[nodiscard]] constexpr const SignalInfo& signal_info(SignalId id) noexcept {
  return kSignals[static_cast<std::size_t>(id)];
}
[[nodiscard]] std::optional<SignalId> signal_by_name(std::string_view name) noexcept;

// One sample of a signal. Unsampled: nothing this snapshot (a detector sees nothing). None: an optional signal that is
// empty (a place with no id, no banner), a value of its own for changed_stable. Some: the value (a bool is 0 or 1).
struct Value {
  enum class State : std::uint8_t { Unsampled, None, Some };
  State state = State::Unsampled;
  double v = 0;

  [[nodiscard]] static constexpr Value some(double x) noexcept { return Value{State::Some, x}; }
  [[nodiscard]] static constexpr Value of(bool b) noexcept { return Value{State::Some, b ? 1.0 : 0.0}; }
  [[nodiscard]] static constexpr Value none() noexcept { return Value{State::None, 0}; }
  template <class T>
  [[nodiscard]] static constexpr Value of_optional(const std::optional<T>& o) noexcept {
    return o ? some(static_cast<double>(*o)) : none();
  }
  [[nodiscard]] constexpr bool sampled() const noexcept { return state != State::Unsampled; }
  [[nodiscard]] constexpr bool present() const noexcept { return state == State::Some; }
  [[nodiscard]] constexpr bool truth() const noexcept { return state == State::Some && v != 0; }
  [[nodiscard]] constexpr bool same(const Value& o) const noexcept {
    return state == o.state && (state != State::Some || v == o.v);
  }
};

// The facts the Lifecycle source turns into samples of `lifecycle` (ARCH §8.2): what en-core's lifecycle slice changed
// on the step, and the read groups that tripped.
enum class FactKind : std::uint8_t {
  DataReady,
  PlaythroughReady,
  LoadingStarted,
  LoadingEnded,
  TitleReached,
  GateChanged,
  GroupTripped,
  LadderChanged,
};
inline constexpr std::array<std::string_view, 8> kFactNames = {
    "data_ready", "playthrough_ready", "loading_started", "loading_ended",
    "title_reached", "gate_changed", "group_tripped", "ladder_changed"};

}  // namespace en::augur
