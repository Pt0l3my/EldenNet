#pragma once
// The world model (ARCH §8.2, §8.7, §8.9 step 1): the last processed snapshot as trackers (one per character, dropped
// after 2 s absent; the Tarnished; the world), each with its detector states and a short history; the engagement
// machine for each pair of the Tarnished and a hostile character, with its squads; the combat state; the stance
// towards each viewer; and what the boss outcome reads. A dropped snapshot never fakes an edge: every derivation
// compares with the last *processed* snapshot.
//
// Game-neutral: what the game's values mean (the hostile teams, the victory banners, the throw states of a critical
// hit, the open world's areas) comes in as WorldFacts, which en-core supplies.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "en/augur/detectors.hpp"
#include "en/augur/events.hpp"
#include "en/augur/rules.hpp"
#include "en/augur/signals.hpp"
#include "en/augur/types.hpp"
#include "en/proto/snapshot.hpp"

namespace en::augur {

class AugurCtx;

// The game's meanings, as en-core knows them (teams.toml, DESIGN §5.3, §6.5, ARCH §8.8).
struct WorldFacts {
  std::vector<std::uint8_t> hostile_teams;    // the live teams that are hostile to the Tarnished
  std::vector<std::uint16_t> victory_banners;  // a boss fight's Victory evidence
  std::uint8_t crit_player_throw = 0;          // CritLanded: the Tarnished's throw state ...
  std::uint8_t crit_victim_throw = 0;          // ... while the victim's is this one
  std::vector<std::uint8_t> open_world_areas;  // the same area means the same area byte here, else the same block
};

// The bounds of §8.7 and §8.10.
inline constexpr std::size_t kMaxTrackers = 256;
inline constexpr Mono kTrackerDropMs = 2000;
inline constexpr Mono kEngageTimeoutMs = 12000;
inline constexpr float kEngageEndM = 30.0f;
inline constexpr float kNearM = 8.0f;
inline constexpr float kLockClosingM = 20.0f;
inline constexpr float kLockClosingShrinkM = 3.0f;
inline constexpr Mono kLockClosingMs = 2000;
inline constexpr Mono kFledWindowMs = 5000;
inline constexpr float kFledGrowM = 15.0f;
inline constexpr float kFledBeyondM = 25.0f;
inline constexpr Mono kSquadRecentMs = 10000;
inline constexpr float kSquadJoinM = 20.0f;
inline constexpr Mono kCombatSignalMs = 8000;
inline constexpr Mono kHardWindowMs = 3000;
inline constexpr float kHardFraction = 0.30f;
inline constexpr float kWitnessM = 20.0f;   // AllyDied's witnesses; an unseen attack's reach; KilledNear's witnesses
inline constexpr float kTalkWithM = 3.0f;   // a native talk's persona (DESIGN §6.5)
inline constexpr float kFoughtBesideM = 40.0f;  // FoughtBeside: a persona within the combat radius at a Victory
inline constexpr float kConfGate = 0.6f;     // gating and barks use only engagements of this confidence or more

// A small ring of the newest N items, oldest first.
template <class T, std::size_t N>
class Ring {
 public:
  void push(const T& v) noexcept {
    items_[(start_ + size_) % N] = v;
    if (size_ < N) {
      ++size_;
    } else {
      start_ = (start_ + 1) % N;
    }
  }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] const T& operator[](std::size_t i) const noexcept { return items_[(start_ + i) % N]; }
  [[nodiscard]] const T& back() const noexcept { return (*this)[size_ - 1]; }
  void clear() noexcept { start_ = size_ = 0; }

 private:
  std::array<T, N> items_{};
  std::size_t start_ = 0;
  std::size_t size_ = 0;
};

struct HistSample {
  Mono t = 0;
  float hp_frac = 0;  // NaN-free: 0 when max is 0
  float dist = 0;     // the Tarnished's distance (a character's); 0 for the Tarnished
};

// One transition of the engagement machine on this step (§8.7).
struct EngTransition {
  bool engaged = true;  // false: disengaged
  EngageVia via = EngageVia::Hit;
  float conf = 0;
  DisengageWhy why = DisengageWhy::Timeout;
  bool fled = false;
  SquadId squad = 0;
};

struct Engagement {
  bool on = false;
  float conf = 0;
  EngageVia via = EngageVia::Hit;
  Mono since = 0;
  Mono last_activity = 0;  // the last exchange or lock-on
  SquadId squad = 0;
  bool rearm = false;  // after a Timeout, Near and lock-on start it again only once neither holds (impl)
};

struct Tracker {
  ChrHandle handle;
  bool present = false;  // in the last processed snapshot
  Mono last_seen = 0;
  proto::Chr chr;  // as last processed
  Subject subject;
  bool has_prev = false;
  std::uint32_t prev_hp = 0;
  bool hp_delta_valid = false;
  float hp_delta = 0;  // (prev hp − hp) / max
  Ring<HistSample, 32> hist;
  Engagement eng;
  std::optional<EngTransition> transition;  // this step's
  std::vector<DetState> det;                // by rule index
};

struct PlayerTrack {
  bool present = false;
  bool ever = false;
  Mono last_seen = 0;
  proto::Player p;
  bool has_prev = false;
  std::uint32_t prev_hp = 0;
  bool hp_delta_valid = false;
  float hp_delta = 0;
  Ring<HistSample, 32> hist;
  std::vector<DetState> det;
};

struct WorldTrack {
  bool has_snapshot = false;
  Mono t = 0;
  proto::Ui ui;
  proto::Boss boss;
  std::optional<proto::Place> place;
  std::optional<Mono> native_talk_since;  // while a native talk is up
  std::optional<Mono> native_talk_started;  // the last one's start, kept after it ends (NativeTalkEnded's held_ms)
  std::vector<DetState> det;
};

struct Squad {
  SquadId id = 0;
  std::uint8_t team = 0;
  std::vector<ChrHandle> members;
};

// A boss fight as the outcome reads it: its ordinal in the session, when it started and ended, its bars, and the
// evidence seen since it started.
struct BossFight {
  std::uint32_t ordinal = 0;
  Mono started = 0;
  proto::FixedVector<proto::BossBar, 3> bars;
  std::optional<Mono> victory_at;  // a victory banner
  std::optional<Mono> died_at;     // a PlayerDied emitted
};

enum class Stance : std::uint8_t { Enemy, Turned, Ally, Bystander };

class WorldModel {
 public:
  explicit WorldModel(WorldFacts facts = {});

  // Fresh detector states for a rule set of n rules (a reload starts every detector again).
  void set_rule_count(std::size_t n);
  // Step 1 of §8.9 for a processed snapshot: trackers, the Tarnished, the world, the engagement machine, squads and
  // the combat state.
  void update(const proto::WorldSnapshot& s, const AugurCtx& ctx);
  // A loading screen (§9: InWorld → Loading): engagements and squads cleared silently, the characters forgotten, every
  // enter_radius re-armed. The Tarnished's and the world's detectors carry on.
  void loading(std::span<const Rule> rules);
  // Records a PlayerDied emitted (the boss outcome's Death evidence).
  void note_player_died(Mono t);

  [[nodiscard]] std::span<Tracker> trackers() noexcept { return trackers_; }
  [[nodiscard]] std::span<const Tracker> trackers() const noexcept { return trackers_; }
  [[nodiscard]] Tracker* find(ChrHandle h) noexcept;
  [[nodiscard]] const Tracker* find(ChrHandle h) const noexcept;
  [[nodiscard]] PlayerTrack& player() noexcept { return player_; }
  [[nodiscard]] const PlayerTrack& player() const noexcept { return player_; }
  [[nodiscard]] WorldTrack& world() noexcept { return world_; }
  [[nodiscard]] const WorldTrack& world() const noexcept { return world_; }
  [[nodiscard]] std::span<const Squad> squads() const noexcept { return squads_; }
  [[nodiscard]] const std::optional<BossFight>& fight() const noexcept { return fight_; }
  [[nodiscard]] const WorldFacts& facts() const noexcept { return facts_; }
  [[nodiscard]] Mono now() const noexcept { return world_.t; }
  // The persona a native talk is with at the last snapshot: the nearest present, living persona within 3 m while the
  // popup's talk id, the talk camera or the HUD's pop-up menu is up (DESIGN §6.5; S3 picks the signals).
  [[nodiscard]] const std::optional<ChrHandle>& talk_with() const noexcept { return talk_with_; }

  // The samples of §8.3's derivations at the last processed snapshot.
  [[nodiscard]] Value sample(SignalId id, const Tracker& t) const noexcept;
  [[nodiscard]] Value sample_player(SignalId id) const noexcept;
  [[nodiscard]] Value sample_world(SignalId id) const noexcept;

  [[nodiscard]] bool hostile(const proto::Chr& c) const noexcept;
  [[nodiscard]] bool in_combat() const noexcept;
  // A character's combat: engaged with the Tarnished at the gate's confidence or more.
  [[nodiscard]] static bool combatant(const Tracker& t) noexcept { return t.eng.on && t.eng.conf >= kConfGate; }
  // The highest HP fraction over the last `window` ms minus the current one: how hard the subject was hit.
  [[nodiscard]] static float drop_within(const Ring<HistSample, 32>& hist, Mono now, Mono window) noexcept;
  // The stance of x towards the viewer (§8.7), from what v0.1 can read: no one reads as turned until S7, and only the
  // Tarnished is on the Tarnished's side.
  [[nodiscard]] Stance stance(const ActorRef& viewer, const ActorRef& x) const noexcept;
  // Whether the Tarnished is concealed from the viewer (§8.7): never, until crouching and invisibility are verified.
  [[nodiscard]] bool concealed_from(const ActorRef&) const noexcept { return false; }
  // An actor's position and block, if it is the Tarnished or a present character.
  [[nodiscard]] std::optional<std::array<float, 3>> position(const ActorRef& a) const noexcept;
  [[nodiscard]] std::optional<BlockId> block(const ActorRef& a) const noexcept;
  [[nodiscard]] bool same_area(BlockId a, BlockId b) const noexcept;

 private:
  void update_player(const proto::WorldSnapshot& s, Mono t);
  void update_world(const proto::WorldSnapshot& s, Mono t);
  void update_trackers(const proto::WorldSnapshot& s, Mono t, const AugurCtx& ctx);
  void update_engagement(Tracker& tr, Mono t);
  void end_engagement(Tracker& tr);
  SquadId join_squad(const Tracker& tr, Mono t);
  void leave_squad(Tracker& tr);
  bool hit_by_player(const Tracker& tr) const noexcept;
  bool hit_player(const Tracker& tr) const noexcept;
  bool crit_by_player(const Tracker& tr) const noexcept;

  WorldFacts facts_;
  std::size_t rule_count_ = 0;
  std::vector<Tracker> trackers_;
  PlayerTrack player_;
  WorldTrack world_;
  std::vector<Squad> squads_;
  SquadId next_squad_ = 1;
  std::optional<Mono> last_combat_signal_;
  std::optional<ChrHandle> talk_with_;  // the persona a native talk is with (talking_with), at the last snapshot
  std::optional<BossFight> fight_;
  std::uint32_t fights_ = 0;
};

}  // namespace en::augur
