#pragma once
// What the game task reads, as plain data (ARCH §7.1, DESIGN §5.2): the WorldSnapshot it builds every 250 ms of game
// time and the parts it is made of, with the read groups' health (ARCH §6.7) and the capabilities discovery found
// (ARCH §6.8). Every field is fixed-capacity and heap-free, so the task fills a preallocated snapshot on the game
// thread (R6) and the core copies it out of the slot (C1); the whole snapshot is trivially copyable.
//
// The types are our own, and so are the enums' names: the raw values the game holds reach them only through the
// layout's enum tables (ARCH §6.3), and an enum's Unknown means "not read". Each type has a JSON form (to_json and
// from_json, below; game_json.cpp) for recordings and replays (ARCH §7.5).
#include <array>
#include <bitset>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <nlohmann/json.hpp>

#include "en/proto/fixed.hpp"

namespace en::proto {

// ---- Identities ----

// A character's handle (the game's packed field-instance handle, read as 64 bits): what crosses the boundary in place
// of a pointer (DESIGN §2.3). It names a character only while that character is in the snapshot.
struct ChrHandle {
  std::uint64_t raw = 0;

  friend constexpr auto operator<=>(const ChrHandle&, const ChrHandle&) = default;
};

// A map block, mAA_BB_CC_DD, packed by us as 0xAABBCCDD: the area in the top byte, then the block, the region and the
// index.
struct BlockId {
  std::uint32_t packed = 0;

  [[nodiscard]] static constexpr BlockId of(std::uint8_t area, std::uint8_t block, std::uint8_t region,
                                            std::uint8_t index) noexcept {
    return BlockId{(std::uint32_t{area} << 24) | (std::uint32_t{block} << 16) | (std::uint32_t{region} << 8) |
                   std::uint32_t{index}};
  }
  [[nodiscard]] constexpr std::uint8_t area() const noexcept { return static_cast<std::uint8_t>(packed >> 24); }
  [[nodiscard]] constexpr std::uint8_t block() const noexcept { return static_cast<std::uint8_t>(packed >> 16); }
  [[nodiscard]] constexpr std::uint8_t region() const noexcept { return static_cast<std::uint8_t>(packed >> 8); }
  [[nodiscard]] constexpr std::uint8_t index() const noexcept { return static_cast<std::uint8_t>(packed); }

  friend constexpr auto operator<=>(const BlockId&, const BlockId&) = default;
};

// A build's place in builds.toml (ARCH §6.3): the index of its [[build]] entry.
struct BuildKey {
  std::uint16_t index = 0;

  friend constexpr auto operator<=>(const BuildKey&, const BuildKey&) = default;
};

// A key of regions.toml (NPC §9.5), such as "limgrave": lower-case ASCII letters, digits and '_', at most 32 bytes.
// The game knows no regions: the core finds the key from the block and the place (regions.toml maps both), so the
// task leaves it empty.
using RegionKey = FixedString<32>;

// ---- Enums: our own names; the layout maps the game's raw values to them (ARCH §6.3) ----

enum class MultiplayRole : std::uint8_t { Unknown, Host, Cooperator, Invader, Other };
enum class LobbyState : std::uint8_t { Unknown, None, Hosting, Joined, Other };
enum class Screen : std::uint8_t { Unknown, InGame, Loading, Title };
enum class HudState : std::uint8_t { Unknown, HideAll, ShowAll, PopupMenu, Default };
enum class BodyType : std::uint8_t { Unknown, A, B };

// DESIGN §9.9's seven statuses, and their flag set (bit i is the status of value i). Empty until S8 verifies them.
enum class Status : std::uint8_t { Frostbite, Poison, ScarletRot, Sleep, Hemorrhage, Madness, DeathBlight };
inline constexpr std::size_t kStatusCount = 7;
enum class StatusSet : std::uint8_t { None = 0 };
template <>
struct FlagSetTraits<StatusSet> {
  static constexpr StatusSet all = static_cast<StatusSet>((1u << kStatusCount) - 1);
};
[[nodiscard]] constexpr StatusSet bit_of(Status s) noexcept {
  return static_cast<StatusSet>(1u << static_cast<unsigned>(s));
}

// The params the task reads rows of (ARCH §6.1, §6.9).
enum class ParamKind : std::uint8_t { NpcParam, BonfireWarpParam, WorldMapPlaceNameParam, SpEffectParam };

// ---- Read groups and their health (ARCH §6.7, DESIGN §4.3) ----

// The eleven breaker groups. Session, world and player are structural: one that fails or trips takes the ladder to L1.
enum class Group : std::uint8_t { Session, World, Combat, Player, Flags, Text, Ui, Place, Camera, Params, Status };
inline constexpr std::size_t kGroupCount = 11;
[[nodiscard]] constexpr bool structural(Group g) noexcept {
  return g == Group::Session || g == Group::World || g == Group::Player;
}

enum class GroupSet : std::uint16_t { None = 0 };
template <>
struct FlagSetTraits<GroupSet> {
  static constexpr GroupSet all = static_cast<GroupSet>((1u << kGroupCount) - 1);
};
[[nodiscard]] constexpr GroupSet bit_of(Group g) noexcept {
  return static_cast<GroupSet>(1u << static_cast<unsigned>(g));
}

// Untested until phase A has run; Ok once it passed; Tripped when a self-test failed or the breaker tripped (3 hard
// faults in 60 s), which is for the session.
enum class GroupState : std::uint8_t { Untested, Ok, Tripped };

struct GroupHealth {
  GroupState state = GroupState::Untested;
  bool phase_b = false;     // phase B has passed in this playthrough (its first in-world snapshots)
  std::uint8_t faults = 0;  // hard faults this tick, saturating at 255

  friend constexpr bool operator==(const GroupHealth&, const GroupHealth&) = default;
};

struct ReadHealth {
  std::array<GroupHealth, kGroupCount> groups{};  // by Group's value
  std::uint32_t task_us_last = 0;                 // the last task invocation's time, µs
  std::uint32_t task_us_p99 = 0;                  // the 99th percentile of the C13 histogram, µs

  [[nodiscard]] constexpr GroupHealth& of(Group g) noexcept { return groups[static_cast<std::size_t>(g)]; }
  [[nodiscard]] constexpr const GroupHealth& of(Group g) const noexcept { return groups[static_cast<std::size_t>(g)]; }

  friend constexpr bool operator==(const ReadHealth&, const ReadHealth&) = default;
};

// What the game backend can read (ARCH §6.8): the groups whose items discovery resolved and whose phase-A self-tests
// passed. The ladder level is the core's, from these, ReadHealth and the provider's health.
struct Capabilities {
  GroupSet groups = GroupSet::None;

  friend constexpr bool operator==(const Capabilities&, const Capabilities&) = default;
};

// ---- The snapshot's parts (ARCH §7.1: units and sources) ----

struct Meter {
  std::uint32_t cur = 0;
  std::uint32_t max = 0;

  friend constexpr bool operator==(const Meter&, const Meter&) = default;
};

struct Session {
  MultiplayRole role = MultiplayRole::Unknown;  // Unknown at the title, where none can be read
  bool is_my_world = false;
  LobbyState lobby = LobbyState::Unknown;
  std::uint8_t player_set_len = 0;  // the player set's entries that are not of an NPC type (DESIGN §3.4)
  bool anticheat = false;           // C14
  bool ersc_loaded = false;         // C14

  friend constexpr bool operator==(const Session&, const Session&) = default;
};

struct Life {
  bool main_player = false;
  Screen screen = Screen::Unknown;
  std::optional<std::uint32_t> load_counter;  // from an optional item: empty without it

  friend constexpr bool operator==(const Life&, const Life&) = default;
};

struct Ui {
  HudState hud = HudState::Unknown;
  std::optional<std::uint16_t> banner;         // empty when 0 or -1
  std::optional<std::uint32_t> popup_talk_id;  // the popup's; empty when negative
  bool talk_cam = false;                       // the talk camera's id is not -1
  bool subtitles_on = false;                   // the game's own setting

  friend constexpr bool operator==(const Ui&, const Ui&) = default;
};

// The six pieces worn, by their equipment row ids; names come through ResolveText (DESIGN §4.5).
struct Gear {
  std::uint32_t right_weapon = 0;
  std::uint32_t left_weapon = 0;
  std::uint32_t head = 0;
  std::uint32_t chest = 0;
  std::uint32_t hands = 0;
  std::uint32_t legs = 0;

  friend constexpr bool operator==(const Gear&, const Gear&) = default;
};

struct Player {
  ChrHandle handle;
  std::uint16_t level = 0;
  std::uint8_t archetype = 0;
  BodyType body = BodyType::Unknown;
  Meter hp, fp, stamina;  // the data module
  std::uint32_t runes_held = 0;
  std::uint32_t deaths = 0;
  std::uint64_t play_time_ms = 0;  // widened
  std::uint8_t ng_lvl = 0;
  std::uint8_t save_slot = 0;
  bool dead = false;    // the dead bit
  bool riding = false;  // ride state 5
  std::optional<ChrHandle> locked_on;
  std::optional<ChrHandle> last_hit_by;
  std::uint8_t throw_state = 0;  // the throw module
  bool roundtable = false;       // the Roundtable's effect in the list (ARCH §7.1)
  StatusSet statuses = StatusSet::None;
  BlockId block;
  std::array<float, 3> pos{};  // the physics position, m
  Gear gear;
  bool boss_flag = false;  // the game's boss-fight flag, the cross-check

  friend constexpr bool operator==(const Player&, const Player&) = default;
};

struct Place {
  std::optional<std::uint32_t> place_id;  // empty when 0 or less
  RegionKey region;                       // regions.toml's key; the core's (see RegionKey)
  bool legacy = false;                    // area < 30
  bool overworld = false;                 // area 60 or 61

  friend constexpr bool operator==(const Place&, const Place&) = default;
};

struct Clock {
  std::uint8_t h = 0;
  std::uint8_t m = 0;

  friend constexpr bool operator==(const Clock&, const Clock&) = default;
};

struct BossBar {
  ChrHandle handle;
  std::int32_t name_id = 0;
  std::int32_t damage_taken = 0;

  friend constexpr bool operator==(const BossBar&, const BossBar&) = default;
};

struct Boss {
  bool active = false;  // bars non-empty, or the player's boss_flag
  FixedVector<BossBar, 3> bars;

  friend constexpr bool operator==(const Boss&, const Boss&) = default;
};

struct Chr {
  ChrHandle handle;
  std::uint32_t param = 0;  // its NpcParam row
  std::uint32_t model = 0;
  std::uint32_t entity = 0;
  std::uint8_t kind = 0;  // the character's type, raw
  std::uint8_t team = 0;  // the live team
  Meter hp;
  bool dead = false;
  std::optional<ChrHandle> last_hit_by;
  std::int32_t recv_dmg_type = 0;
  std::uint8_t throw_state = 0;
  float dist_m = 0;       // |pos - player.pos|, computed by us
  float bearing_deg = 0;  // against the camera's forward; NaN without the camera
  bool on_screen = false;
  StatusSet statuses = StatusSet::None;
  std::optional<bool> asleep;  // empty until S8
  BlockId block;
  std::array<float, 3> pos{};

  friend constexpr bool operator==(const Chr&, const Chr&) = default;
};

// The watched event flags (MEM §6.2): bit i answers the watch list's id i; `valid` is 0 where the id's group does not
// exist, and the id then reads false (ARCH §6.9).
inline constexpr std::size_t kMaxWatchFlags = 2048;
struct FlagFrame {
  std::uint32_t watch_version = 0;
  std::bitset<kMaxWatchFlags> bits;
  std::bitset<kMaxWatchFlags> valid;

  friend bool operator==(const FlagFrame&, const FlagFrame&) = default;
};

// An answer to ResolveText: the sanitised UTF-8 text, or empty when the id has none.
struct TextEntry {
  std::uint16_t fmg = 0;
  std::uint32_t id = 0;
  std::optional<FixedString<256>> text;

  friend constexpr bool operator==(const TextEntry&, const TextEntry&) = default;
};

// One field of a param row: its index among the layout's fields for that param, and its value, widened.
struct ParamField {
  std::uint8_t index = 0;
  std::int64_t value = 0;

  friend constexpr bool operator==(const ParamField&, const ParamField&) = default;
};

struct ParamRowInfo {
  ParamKind param = ParamKind::NpcParam;
  std::uint32_t id = 0;
  FixedVector<ParamField, 8> fields;

  friend constexpr bool operator==(const ParamRowInfo&, const ParamRowInfo&) = default;
};

// ---- The snapshot ----

inline constexpr std::size_t kMaxSnapshotChars = 32;
inline constexpr std::size_t kMaxNewText = 16;
inline constexpr std::size_t kMaxNewParams = 32;

struct WorldSnapshot {
  std::uint64_t seq = 0;      // +1 per snapshot the task builds
  std::uint64_t mono_ms = 0;  // QPC at the start of the build
  bool gap = false;           // an unread snapshot was overwritten, or the budget ran out (ARCH §6.5)
  BuildKey build;
  ReadHealth health;
  Session session;
  Life life;
  Ui ui;
  std::optional<Player> player;  // empty when main_player is null
  std::optional<Place> place;
  std::optional<Clock> clock;
  Boss boss;
  FixedVector<Chr, kMaxSnapshotChars> chars;            // nearest first
  std::optional<FlagFrame> flags;                       // every 4th snapshot
  FixedVector<TextEntry, kMaxNewText> new_text;         // answers to ResolveText
  FixedVector<ParamRowInfo, kMaxNewParams> new_params;  // answers to ResolveParams

  friend bool operator==(const WorldSnapshot&, const WorldSnapshot&) = default;
};

// ---- Names, as the JSON forms write them ----

const char* to_string(MultiplayRole v) noexcept;
const char* to_string(LobbyState v) noexcept;
const char* to_string(Screen v) noexcept;
const char* to_string(HudState v) noexcept;
const char* to_string(BodyType v) noexcept;
const char* to_string(Status v) noexcept;
const char* to_string(ParamKind v) noexcept;
const char* to_string(Group v) noexcept;  // lower case, as the design writes them: "session", "world", ...
const char* to_string(GroupState v) noexcept;

// ---- JSON forms (game_json.cpp) ----
// Every field under its own name; an absent optional is null; a handle, a block and a build key are integers; a flag
// set is an array of names; a float that is not finite is null, and null reads back as NaN; a flag frame's bit sets
// are hex strings (bit i is bit i % 8 of byte i / 8, trailing zero bytes left out). Reading refuses unknown fields, a
// value of the wrong type or range, and a container past its capacity, with the path and the problem (ProtoError).

void to_json(nlohmann::json& j, ChrHandle v);
void from_json(const nlohmann::json& j, ChrHandle& v);
void to_json(nlohmann::json& j, BlockId v);
void from_json(const nlohmann::json& j, BlockId& v);
void to_json(nlohmann::json& j, BuildKey v);
void from_json(const nlohmann::json& j, BuildKey& v);
void to_json(nlohmann::json& j, MultiplayRole v);
void from_json(const nlohmann::json& j, MultiplayRole& v);
void to_json(nlohmann::json& j, LobbyState v);
void from_json(const nlohmann::json& j, LobbyState& v);
void to_json(nlohmann::json& j, Screen v);
void from_json(const nlohmann::json& j, Screen& v);
void to_json(nlohmann::json& j, HudState v);
void from_json(const nlohmann::json& j, HudState& v);
void to_json(nlohmann::json& j, BodyType v);
void from_json(const nlohmann::json& j, BodyType& v);
void to_json(nlohmann::json& j, Status v);
void from_json(const nlohmann::json& j, Status& v);
void to_json(nlohmann::json& j, StatusSet v);
void from_json(const nlohmann::json& j, StatusSet& v);
void to_json(nlohmann::json& j, ParamKind v);
void from_json(const nlohmann::json& j, ParamKind& v);
void to_json(nlohmann::json& j, Group v);
void from_json(const nlohmann::json& j, Group& v);
void to_json(nlohmann::json& j, GroupSet v);
void from_json(const nlohmann::json& j, GroupSet& v);
void to_json(nlohmann::json& j, GroupState v);
void from_json(const nlohmann::json& j, GroupState& v);

void to_json(nlohmann::json& j, const GroupHealth& v);
void from_json(const nlohmann::json& j, GroupHealth& v);
void to_json(nlohmann::json& j, const ReadHealth& v);
void from_json(const nlohmann::json& j, ReadHealth& v);
void to_json(nlohmann::json& j, const Capabilities& v);
void from_json(const nlohmann::json& j, Capabilities& v);
void to_json(nlohmann::json& j, const Meter& v);
void from_json(const nlohmann::json& j, Meter& v);
void to_json(nlohmann::json& j, const Session& v);
void from_json(const nlohmann::json& j, Session& v);
void to_json(nlohmann::json& j, const Life& v);
void from_json(const nlohmann::json& j, Life& v);
void to_json(nlohmann::json& j, const Ui& v);
void from_json(const nlohmann::json& j, Ui& v);
void to_json(nlohmann::json& j, const Gear& v);
void from_json(const nlohmann::json& j, Gear& v);
void to_json(nlohmann::json& j, const Player& v);
void from_json(const nlohmann::json& j, Player& v);
void to_json(nlohmann::json& j, const Place& v);
void from_json(const nlohmann::json& j, Place& v);
void to_json(nlohmann::json& j, const Clock& v);
void from_json(const nlohmann::json& j, Clock& v);
void to_json(nlohmann::json& j, const BossBar& v);
void from_json(const nlohmann::json& j, BossBar& v);
void to_json(nlohmann::json& j, const Boss& v);
void from_json(const nlohmann::json& j, Boss& v);
void to_json(nlohmann::json& j, const Chr& v);
void from_json(const nlohmann::json& j, Chr& v);
void to_json(nlohmann::json& j, const FlagFrame& v);
void from_json(const nlohmann::json& j, FlagFrame& v);
void to_json(nlohmann::json& j, const TextEntry& v);
void from_json(const nlohmann::json& j, TextEntry& v);
void to_json(nlohmann::json& j, const ParamField& v);
void from_json(const nlohmann::json& j, ParamField& v);
void to_json(nlohmann::json& j, const ParamRowInfo& v);
void from_json(const nlohmann::json& j, ParamRowInfo& v);
void to_json(nlohmann::json& j, const WorldSnapshot& v);
void from_json(const nlohmann::json& j, WorldSnapshot& v);

}  // namespace en::proto
