#pragma once
// The event catalogue (ARCH §8.6, DESIGN §6.4, §6.5): every v1 kind as a struct of its own, and EventKind, the variant
// of them all in the catalogue's order. Since O45 every PersonaId here reads "profile id", except in StageChanged.
//
// v0.1 emits only what its sources can produce (the module's README lists which, and why the others wait). The kinds a
// later source emits are here already, so that a rule file naming one is checked against the same catalogue; their
// helper types are the smallest the catalogue names (a later part widens them, never renames them).
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "en/augur/types.hpp"
#include "en/proto/fixed.hpp"
#include "en/proto/snapshot.hpp"

namespace en::augur {

// ---- Helper types the catalogue names ----

enum class BossOutcome : std::uint8_t { Victory, Death, Left };
enum class AttackVia : std::uint8_t { HpDrop, HostilityBit };
enum class EngageVia : std::uint8_t { Hit, LockClosing, Near, LockOnly };
enum class DisengageWhy : std::uint8_t { Timeout, Distance };
enum class LinePhase : std::uint8_t { Started, Ended };
// Which talk fields were up when a native talk started (S3 picks the signals).
struct TalkSignals {
  bool popup = false;     // ui.popup_talk_id is set
  bool talk_cam = false;  // ui.talk_cam
  bool popup_menu = false;  // hud == PopupMenu with a persona within 3 m (needs personas: never at v0.1)
  friend constexpr bool operator==(const TalkSignals&, const TalkSignals&) = default;
};
// The ladder's levels as events name them (ARCH §6.8).
enum class LadderLevel : std::uint8_t { Starting, L1, L2, L3, L4 };
// The pipeline's helper types (v0.3 and later; never made at v0.1). The text is the sanitised text R13's SafeText will
// carry; until the sanitizer exists nothing makes one.
using TextRef = std::shared_ptr<const std::string>;
using ConvId = std::uint32_t;
using ItemId = std::uint32_t;
using LineId = std::uint32_t;
enum class InputSource : std::uint8_t { Typed, Voice };
enum class Motive : std::uint8_t { Reply };  // NPC §9.1's reaction types join when barks exist (v0.4)
struct Opener {
  enum class Kind : std::uint8_t { Player, Npc };
  Kind kind = Kind::Player;
  LineId line = 0;
  friend constexpr bool operator==(const Opener&, const Opener&) = default;
};
enum class EndReason : std::uint8_t { Player, Travel, Death, Dormant, Timeout };
enum class Certainty : std::uint8_t { Fact, Rumour, Belief };

// ---- The catalogue, in its order ----

// world (P unless noted)
struct PlaceEntered {
  std::uint32_t place_id = 0;
  std::optional<std::uint32_t> prev;
  friend bool operator==(const PlaceEntered&, const PlaceEntered&) = default;
};
struct GraceRested {
  std::optional<std::uint32_t> grace;
  friend bool operator==(const GraceRested&, const GraceRested&) = default;
};
struct RestEnded {
  friend bool operator==(const RestEnded&, const RestEnded&) = default;
};
struct GraceLit {
  std::uint32_t flag = 0, grace = 0;
  friend bool operator==(const GraceLit&, const GraceLit&) = default;
};
struct FlagChanged {
  std::shared_ptr<const std::string> alias;
  bool value = false, verified = false;
  friend bool operator==(const FlagChanged&, const FlagChanged&) = default;
};
struct BossFightStarted {
  proto::FixedVector<BossRef, 3> bars;
  friend bool operator==(const BossFightStarted&, const BossFightStarted&) = default;
};
struct BossFightEnded {
  BossOutcome outcome = BossOutcome::Left;
  proto::FixedVector<BossRef, 3> bars;
  friend bool operator==(const BossFightEnded&, const BossFightEnded&) = default;
};
// personas
struct PersonaSeen {
  PersonaId persona;
  friend bool operator==(const PersonaSeen&, const PersonaSeen&) = default;
};
struct PersonaApproached {
  PersonaId persona;
  friend bool operator==(const PersonaApproached&, const PersonaApproached&) = default;
};
struct PersonaAttacked {
  PersonaId persona;
  AttackVia via = AttackVia::HpDrop;
  friend bool operator==(const PersonaAttacked&, const PersonaAttacked&) = default;
};
struct PersonaDied {
  PersonaId persona;
  bool by_player = false;
  friend bool operator==(const PersonaDied&, const PersonaDied&) = default;
};
struct FoughtBeside {
  PersonaId persona;
  std::int32_t boss = 0;
  friend bool operator==(const FoughtBeside&, const FoughtBeside&) = default;
};
struct NativeTalkStarted {
  std::optional<PersonaId> persona;
  TalkSignals signals;
  friend bool operator==(const NativeTalkStarted&, const NativeTalkStarted&) = default;
};
struct NativeTalkEnded {
  std::optional<PersonaId> persona;
  std::uint32_t held_ms = 0;
  friend bool operator==(const NativeTalkEnded&, const NativeTalkEnded&) = default;
};
struct StageChanged {
  PersonaId persona;
  StageId from = 0, to = 0;
  friend bool operator==(const StageChanged&, const StageChanged&) = default;
};
// player
struct PlayerDied {
  std::optional<ActorRef> killer;
  bool in_boss = false;
  friend bool operator==(const PlayerDied&, const PlayerDied&) = default;
};
struct MountChanged {
  bool riding = false;
  friend bool operator==(const MountChanged&, const MountChanged&) = default;
};
// combat (K)
struct Struck {
  std::optional<ActorRef> att;
  ActorRef vic;
  bool hard = false, unseen = false;
  std::uint16_t count = 1;
  float frac_total = 0;
  std::int32_t dmg_type = 0;
  friend bool operator==(const Struck&, const Struck&) = default;
};
struct LowHp {
  ActorRef subject;
  float frac = 0;
  friend bool operator==(const LowHp&, const LowHp&) = default;
};
struct Healed {
  ActorRef subject;
  float frac = 0;
  friend bool operator==(const Healed&, const Healed&) = default;
};
struct Engaged {
  ActorRef subject, opponent;
  EngageVia via = EngageVia::Hit;
  SquadId squad = 0;
  friend bool operator==(const Engaged&, const Engaged&) = default;
};
struct Disengaged {
  ActorRef subject, opponent;
  bool fled = false;
  DisengageWhy why = DisengageWhy::Timeout;
  friend bool operator==(const Disengaged&, const Disengaged&) = default;
};
struct AllyDied {
  ActorRef victim;
  SideId side;
  std::vector<ChrHandle> witnesses;
  friend bool operator==(const AllyDied&, const AllyDied&) = default;
};
struct CritLanded {
  ActorRef victim;
  friend bool operator==(const CritLanded&, const CritLanded&) = default;
};
struct KilledNear {
  CardId victim_card;
  std::vector<PersonaId> witnesses;
  friend bool operator==(const KilledNear&, const KilledNear&) = default;
};
struct IdleNear {
  ActorRef subject;
  friend bool operator==(const IdleNear&, const IdleNear&) = default;
};
struct FoeKilled {
  ActorRef victim, by;
  friend bool operator==(const FoeKilled&, const FoeKilled&) = default;
};
struct StatusStarted {
  ActorRef subject;
  proto::Status status = proto::Status::Frostbite;
  friend bool operator==(const StatusStarted&, const StatusStarted&) = default;
};
struct StatusEnded {
  ActorRef subject;
  proto::Status status = proto::Status::Frostbite;
  friend bool operator==(const StatusEnded&, const StatusEnded&) = default;
};
struct StatusBurst {
  ActorRef subject;
  proto::Status status = proto::Status::Frostbite;
  friend bool operator==(const StatusBurst&, const StatusBurst&) = default;
};
// proximity and timers (T)
struct CameNear {
  ActorRef subject, other;
  friend bool operator==(const CameNear&, const CameNear&) = default;
};
struct AutonomyTick {
  ActorRef subject;
  friend bool operator==(const AutonomyTick&, const AutonomyTick&) = default;
};
// pipeline
struct PlayerSaid {
  std::optional<ActorRef> to;
  TextRef text;
  std::optional<ConvId> conv;
  InputSource input = InputSource::Typed;
  friend bool operator==(const PlayerSaid&, const PlayerSaid&) = default;
};
struct NpcSaid {
  ActorRef speaker;
  std::optional<ActorRef> to;
  TextRef text;
  std::optional<ConvId> conv;
  std::uint8_t pages = 0;
  Motive motive = Motive::Reply;
  std::optional<ItemId> cue;
  friend bool operator==(const NpcSaid&, const NpcSaid&) = default;
};
struct CalloutSent {
  ActorRef target;
  friend bool operator==(const CalloutSent&, const CalloutSent&) = default;
};
struct CalloutReplied {
  ActorRef target;
  bool from_seed = false;
  friend bool operator==(const CalloutReplied&, const CalloutReplied&) = default;
};
struct ConversationStarted {
  PersonaId persona;
  ConvId conv = 0;
  Opener opened_by;
  friend bool operator==(const ConversationStarted&, const ConversationStarted&) = default;
};
struct ConversationEnded {
  PersonaId persona;
  ConvId conv = 0;
  EndReason reason = EndReason::Player;
  std::uint16_t player_lines = 0, committed = 0;
  std::optional<std::vector<std::pair<ItemId, Certainty>>> known;
  friend bool operator==(const ConversationEnded&, const ConversationEnded&) = default;
};
// lifecycle (P for PlaythroughReady and JourneyChanged, T otherwise)
struct DataReady {
  friend bool operator==(const DataReady&, const DataReady&) = default;
};
struct PlaythroughReady {
  PlaythroughKey key;
  friend bool operator==(const PlaythroughReady&, const PlaythroughReady&) = default;
};
struct LoadingStarted {
  friend bool operator==(const LoadingStarted&, const LoadingStarted&) = default;
};
struct LoadingEnded {
  friend bool operator==(const LoadingEnded&, const LoadingEnded&) = default;
};
struct TitleReached {
  friend bool operator==(const TitleReached&, const TitleReached&) = default;
};
struct JourneyChanged {
  std::uint8_t ng_lvl = 0;
  friend bool operator==(const JourneyChanged&, const JourneyChanged&) = default;
};
struct GateChanged {
  bool active = false;
  friend bool operator==(const GateChanged&, const GateChanged&) = default;
};
struct GroupTripped {
  proto::Group group = proto::Group::Session;
  friend bool operator==(const GroupTripped&, const GroupTripped&) = default;
};
struct LadderChanged {
  LadderLevel to = LadderLevel::Starting;
  friend bool operator==(const LadderChanged&, const LadderChanged&) = default;
};
// scripted lines (T)
struct NativeLineHeard {
  std::optional<ActorRef> speaker;
  LinePhase phase = LinePhase::Started;
  std::optional<std::uint32_t> talk_id;
  friend bool operator==(const NativeLineHeard&, const NativeLineHeard&) = default;
};
// reserved seams (never emitted in v1)
struct NpcThought {
  friend bool operator==(const NpcThought&, const NpcThought&) = default;
};
struct DecisionMade {
  friend bool operator==(const DecisionMade&, const DecisionMade&) = default;
};

using EventKind =
    std::variant<PlaceEntered, GraceRested, RestEnded, GraceLit, FlagChanged, BossFightStarted, BossFightEnded,
                 PersonaSeen, PersonaApproached, PersonaAttacked, PersonaDied, FoughtBeside, NativeTalkStarted,
                 NativeTalkEnded, StageChanged, PlayerDied, MountChanged, Struck, LowHp, Healed, Engaged, Disengaged,
                 AllyDied, CritLanded, KilledNear, IdleNear, FoeKilled, StatusStarted, StatusEnded, StatusBurst,
                 CameNear, AutonomyTick, PlayerSaid, NpcSaid, CalloutSent, CalloutReplied, ConversationStarted,
                 ConversationEnded, DataReady, PlaythroughReady, LoadingStarted, LoadingEnded, TitleReached,
                 JourneyChanged, GateChanged, GroupTripped, LadderChanged, NativeLineHeard, NpcThought, DecisionMade>;

inline constexpr std::size_t kKindCount = std::variant_size_v<EventKind>;

// The kinds' names, as rule files and golden files write them ("PlaceEntered", ...), by the variant's index.
[[nodiscard]] std::string_view kind_name(std::size_t index) noexcept;
[[nodiscard]] inline std::string_view kind_name(const EventKind& k) noexcept { return kind_name(k.index()); }
// The index of a kind by its name, or nothing.
[[nodiscard]] std::optional<std::size_t> kind_index(std::string_view name) noexcept;

// ---- The event (DESIGN §6.4) ----

struct Event {
  EventId id;
  EventKind kind;
  Class event_class = Class::T;
  Mono t_play_ms = 0;  // the Tarnished's play time at the last processed snapshot
  Mono t_mono_ms = 0;  // the step's time
  std::uint16_t journey = 0;  // ng_lvl
  PlaceRef place;
  std::optional<ActorRef> subject, object;
  std::vector<AudienceEntry> audience;  // frozen once resolved
  float confidence = 1.0f;
  SourceId source = SourceId::SnapshotDiff;
  std::shared_ptr<const std::string> rule;  // the rule's id
  std::shared_ptr<const std::string> key;
  std::optional<std::uint32_t> ttl_ms;
  std::uint16_t count = 1;
  InterruptSet interrupts = InterruptSet::None;
  bool merged = false;  // a K event re-emitted by a merge on a salient change (ARCH §8.4 item 4)
  // For the bus's order (§8.9 step 3): the rule's priority, the subject's distance from the Tarnished, its handle.
  std::uint8_t priority = 50;
  float distance_m = 0;
  std::uint64_t order_handle = 0;
  Radius radius = Radius::None;
};

using EventBatch = std::vector<Event>;

// Whether the persona perceived the event: one function for every reader (ARCH §8.8; WHY§99).
[[nodiscard]] bool visible_to(const Event& ev, PersonaId persona) noexcept;

// The event kind's own interrupt set (ARCH §8.8's table): Engaged with confidence ≥ 0.6 within 25 m, PlayerDied,
// LoadingStarted, NativeTalkStarted and NativeLineHeard{Started}.
[[nodiscard]] InterruptSet kind_interrupts(const Event& ev) noexcept;

}  // namespace en::augur
