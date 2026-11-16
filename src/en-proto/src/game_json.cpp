// The JSON forms of the game messages (snapshot.hpp, lifecycle.hpp, command.hpp, render.hpp): the records of a
// recording (ARCH §7.5) and what a replay reads back. Written with every field under its own name; read strictly, with
// the path and the problem in every refusal (json_read.hpp).
#include <array>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "en/proto/command.hpp"
#include "en/proto/lifecycle.hpp"
#include "en/proto/render.hpp"
#include "en/proto/snapshot.hpp"
#include "json_read.hpp"

namespace en::proto {

using nlohmann::json;

namespace detail {

// ---- Readers of plain values the model client's messages do not use ----

// Every integer type but bool and the two json.cpp reads (int, std::uint64_t), refusing what does not fit.
template <class I>
  requires(std::is_integral_v<I> && !std::is_same_v<I, bool> && !std::is_same_v<I, int> &&
           !std::is_same_v<I, std::uint64_t>)
void read(const json& j, const At& at, I& out) {
  if constexpr (std::is_unsigned_v<I>) {
    std::uint64_t value = 0;
    if (j.is_number_unsigned()) {
      value = j.get<std::uint64_t>();
    } else if (j.is_number_integer() && j.get<std::int64_t>() >= 0) {
      value = static_cast<std::uint64_t>(j.get<std::int64_t>());
    } else {
      fail(at, "must be a whole number of 0 or more");
    }
    if (!std::in_range<I>(value)) fail(at, "is out of range");
    out = static_cast<I>(value);
  } else {
    std::int64_t value = 0;
    if (j.is_number_unsigned()) {
      const auto u = j.get<std::uint64_t>();
      if (!std::in_range<std::int64_t>(u)) fail(at, "is out of range");
      value = static_cast<std::int64_t>(u);
    } else if (j.is_number_integer()) {
      value = j.get<std::int64_t>();
    } else {
      fail(at, "must be a whole number");
    }
    if (!std::in_range<I>(value)) fail(at, "is out of range");
    out = static_cast<I>(value);
  }
}

// A float: a number within float's range, or null for NaN (a float that is not finite is written as null).
void read(const json& j, const At& at, float& out) {
  if (j.is_null()) {
    out = std::numeric_limits<float>::quiet_NaN();
    return;
  }
  if (!j.is_number()) fail(at, "must be a number or null");
  const double value = j.get<double>();
  if (!(std::fabs(value) <= static_cast<double>(std::numeric_limits<float>::max()))) fail(at, "is out of range");
  out = static_cast<float>(value);
}

template <std::size_t N>
void read(const json& j, const At& at, std::array<float, N>& out) {
  if (!j.is_array() || j.size() != N) fail(at, "must be an array of " + std::to_string(N) + " numbers");
  for (std::size_t i = 0; i < N; ++i) read(j[i], At{&at, {}, i}, out[i]);
}

template <class T, std::size_t N>
void read(const json& j, const At& at, FixedVector<T, N>& out) {
  if (!j.is_array()) fail(at, "must be an array");
  if (j.size() > N) fail(at, "has more than " + std::to_string(N) + " items");
  out.clear();
  for (std::size_t i = 0; i < j.size(); ++i) {
    T value{};
    read(j[i], At{&at, {}, i}, value);
    (void)out.try_push_back(std::move(value));  // within the capacity: the size was checked
  }
}

template <std::size_t N>
void read(const json& j, const At& at, FixedString<N>& out) {
  if (!j.is_string()) fail(at, "must be a string");
  const std::string& text = j.get_ref<const std::string&>();
  if (text.size() > N) fail(at, "is longer than " + std::to_string(N) + " bytes");
  if (text.find('\0') != std::string::npos) fail(at, "holds a NUL");
  if (!valid_utf8(text)) fail(at, "is not valid UTF-8");
  (void)out.try_assign(text);  // within the capacity, UTF-8 and with no NUL: checked above
}

// A bit set as hex: bit i is bit i % 8 of byte i / 8, each byte two digits, the high one first.
void read(const json& j, const At& at, std::bitset<kMaxWatchFlags>& out) {
  constexpr std::size_t kMaxDigits = kMaxWatchFlags / 4;
  if (!j.is_string()) fail(at, "must be a string of hex digits");
  const std::string& text = j.get_ref<const std::string&>();
  if (text.size() % 2 != 0 || text.size() > kMaxDigits) {
    fail(at, "must be an even number of hex digits, at most " + std::to_string(kMaxDigits));
  }
  out.reset();
  for (std::size_t d = 0; d < text.size(); ++d) {
    const char c = text[d];
    unsigned nibble = 0;
    if (c >= '0' && c <= '9') {
      nibble = static_cast<unsigned>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      nibble = static_cast<unsigned>(c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      nibble = static_cast<unsigned>(c - 'A' + 10);
    } else {
      fail(at, "must hold hex digits only");
    }
    const std::size_t byte = d / 2;
    const unsigned shift = d % 2 == 0 ? 4u : 0u;  // the high digit first
    for (unsigned b = 0; b < 4; ++b) {
      if ((nibble >> b) & 1u) out.set(byte * 8 + shift + b);
    }
  }
}

// ---- Enums and flag sets ----

constexpr std::array kRoles = {MultiplayRole::Unknown, MultiplayRole::Host, MultiplayRole::Cooperator,
                               MultiplayRole::Invader, MultiplayRole::Other};
constexpr std::array kLobbies = {LobbyState::Unknown, LobbyState::None, LobbyState::Hosting, LobbyState::Joined,
                                 LobbyState::Other};
constexpr std::array kScreens = {Screen::Unknown, Screen::InGame, Screen::Loading, Screen::Title};
constexpr std::array kHuds = {HudState::Unknown, HudState::HideAll, HudState::ShowAll, HudState::PopupMenu,
                              HudState::Default};
constexpr std::array kBodies = {BodyType::Unknown, BodyType::A, BodyType::B};
constexpr std::array kStatuses = {Status::Frostbite, Status::Poison,  Status::ScarletRot, Status::Sleep,
                                  Status::Hemorrhage, Status::Madness, Status::DeathBlight};
constexpr std::array kParamKinds = {ParamKind::NpcParam, ParamKind::BonfireWarpParam,
                                    ParamKind::WorldMapPlaceNameParam, ParamKind::SpEffectParam};
constexpr std::array kGroups = {Group::Session, Group::World,  Group::Combat, Group::Player,
                                Group::Flags,   Group::Text,   Group::Ui,     Group::Place,
                                Group::Camera,  Group::Params, Group::Status};
constexpr std::array kGroupStates = {GroupState::Untested, GroupState::Ok, GroupState::Tripped};
static_assert(kStatuses.size() == kStatusCount && kGroups.size() == kGroupCount);

void read(const json& j, const At& at, MultiplayRole& out) { read_enum(j, at, kRoles, out); }
void read(const json& j, const At& at, LobbyState& out) { read_enum(j, at, kLobbies, out); }
void read(const json& j, const At& at, Screen& out) { read_enum(j, at, kScreens, out); }
void read(const json& j, const At& at, HudState& out) { read_enum(j, at, kHuds, out); }
void read(const json& j, const At& at, BodyType& out) { read_enum(j, at, kBodies, out); }
void read(const json& j, const At& at, Status& out) { read_enum(j, at, kStatuses, out); }
void read(const json& j, const At& at, ParamKind& out) { read_enum(j, at, kParamKinds, out); }
void read(const json& j, const At& at, Group& out) { read_enum(j, at, kGroups, out); }
void read(const json& j, const At& at, GroupState& out) { read_enum(j, at, kGroupStates, out); }

// A flag set as the array of its members' names, each at most once.
template <class Set, class One, std::size_t N>
void read_set(const json& j, const At& at, const std::array<One, N>& all, Set& out) {
  if (!j.is_array()) fail(at, "must be an array of names");
  out = Set{};
  for (std::size_t i = 0; i < j.size(); ++i) {
    One one{};
    read_enum(j[i], At{&at, {}, i}, all, one);
    if (has_all(out, bit_of(one))) fail(At{&at, {}, i}, "names a member twice");
    out |= bit_of(one);
  }
}

template <class Set, class One, std::size_t N>
json write_set(Set set, const std::array<One, N>& all) {
  json out = json::array();
  for (const One one : all) {
    if (has_all(set, bit_of(one))) out.push_back(to_string(one));
  }
  return out;
}

void read(const json& j, const At& at, StatusSet& out) { read_set(j, at, kStatuses, out); }
void read(const json& j, const At& at, GroupSet& out) { read_set(j, at, kGroups, out); }

// ---- Identities ----

void read(const json& j, const At& at, ChrHandle& out) { read(j, at, out.raw); }
void read(const json& j, const At& at, BlockId& out) { read(j, at, out.packed); }
void read(const json& j, const At& at, BuildKey& out) { read(j, at, out.index); }

// ---- Health and capabilities ----

void read(const json& j, const At& at, GroupHealth& out) {
  out = GroupHealth{};
  Fields(j, at)("state", out.state)("phase_b", out.phase_b)("faults", out.faults).done();
}

// The groups as an object keyed by their names; a group left out keeps its default.
void read(const json& j, const At& at, std::array<GroupHealth, kGroupCount>& out) {
  if (!j.is_object()) fail(at, "must be an object");
  out = {};
  for (auto it = j.begin(); it != j.end(); ++it) {
    const Group* found = nullptr;
    for (const Group& g : kGroups) {
      if (it.key() == to_string(g)) found = &g;
    }
    if (found == nullptr) fail(at, "has an unknown group " + shown_name(it.key()));
    read(*it, At{&at, to_string(*found), 0}, out[static_cast<std::size_t>(*found)]);
  }
}

void read(const json& j, const At& at, ReadHealth& out) {
  out = ReadHealth{};
  Fields(j, at)("groups", out.groups)("task_us_last", out.task_us_last)("task_us_p99", out.task_us_p99).done();
}

void read(const json& j, const At& at, Capabilities& out) {
  out = Capabilities{};
  Fields(j, at)("groups", out.groups).done();
}

// ---- The snapshot's parts ----

void read(const json& j, const At& at, Meter& out) {
  out = Meter{};
  Fields(j, at)("cur", out.cur)("max", out.max).done();
}

void read(const json& j, const At& at, Session& out) {
  out = Session{};
  Fields(j, at)("role", out.role)("is_my_world", out.is_my_world)("lobby", out.lobby)("player_set_len",
                                                                                       out.player_set_len)(
      "anticheat", out.anticheat)("ersc_loaded", out.ersc_loaded)
      .done();
}

void read(const json& j, const At& at, Life& out) {
  out = Life{};
  Fields(j, at)("main_player", out.main_player)("screen", out.screen)("load_counter", out.load_counter).done();
}

void read(const json& j, const At& at, Ui& out) {
  out = Ui{};
  Fields(j, at)("hud", out.hud)("banner", out.banner)("popup_talk_id", out.popup_talk_id)("talk_cam", out.talk_cam)(
      "subtitles_on", out.subtitles_on)
      .done();
}

void read(const json& j, const At& at, Gear& out) {
  out = Gear{};
  Fields(j, at)("right_weapon", out.right_weapon)("left_weapon", out.left_weapon)("head", out.head)(
      "chest", out.chest)("hands", out.hands)("legs", out.legs)
      .done();
}

void read(const json& j, const At& at, Player& out) {
  out = Player{};
  Fields(j, at)("handle", out.handle)("level", out.level)("archetype", out.archetype)("body", out.body)("hp", out.hp)(
      "fp", out.fp)("stamina", out.stamina)("runes_held", out.runes_held)("deaths", out.deaths)(
      "play_time_ms", out.play_time_ms)("ng_lvl", out.ng_lvl)("save_slot", out.save_slot)("dead", out.dead)(
      "riding", out.riding)("locked_on", out.locked_on)("last_hit_by", out.last_hit_by)("throw_state",
                                                                                        out.throw_state)(
      "roundtable", out.roundtable)("statuses", out.statuses)("block", out.block)("pos", out.pos)("gear", out.gear)(
      "boss_flag", out.boss_flag)
      .done();
}

bool region_key_ok(std::string_view key) {
  for (const char c : key) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

void read(const json& j, const At& at, Place& out) {
  out = Place{};
  Fields(j, at)("place_id", out.place_id)("region", out.region)("legacy", out.legacy)("overworld", out.overworld)
      .done();
  if (!region_key_ok(out.region.view())) {
    fail(At{&at, "region", 0}, "must hold lower-case letters, digits and _ only");
  }
}

void read(const json& j, const At& at, Clock& out) {
  out = Clock{};
  Fields(j, at)("h", out.h)("m", out.m).done();
  if (out.h > 23) fail(At{&at, "h", 0}, "is out of range");
  if (out.m > 59) fail(At{&at, "m", 0}, "is out of range");
}

void read(const json& j, const At& at, BossBar& out) {
  out = BossBar{};
  Fields(j, at)("handle", out.handle)("name_id", out.name_id)("damage_taken", out.damage_taken).done();
}

void read(const json& j, const At& at, Boss& out) {
  out = Boss{};
  Fields(j, at)("active", out.active)("bars", out.bars).done();
}

void read(const json& j, const At& at, Chr& out) {
  out = Chr{};
  Fields(j, at)("handle", out.handle)("param", out.param)("model", out.model)("entity", out.entity)("kind", out.kind)(
      "team", out.team)("hp", out.hp)("dead", out.dead)("last_hit_by", out.last_hit_by)("recv_dmg_type",
                                                                                         out.recv_dmg_type)(
      "throw_state", out.throw_state)("dist_m", out.dist_m)("bearing_deg", out.bearing_deg)("on_screen",
                                                                                            out.on_screen)(
      "statuses", out.statuses)("asleep", out.asleep)("block", out.block)("pos", out.pos)
      .done();
}

void read(const json& j, const At& at, FlagFrame& out) {
  out = FlagFrame{};
  Fields(j, at)("watch_version", out.watch_version)("bits", out.bits)("valid", out.valid).done();
}

void read(const json& j, const At& at, TextEntry& out) {
  out = TextEntry{};
  Fields(j, at)("fmg", out.fmg)("id", out.id)("text", out.text).done();
}

// A param field as the pair [index, value].
void read(const json& j, const At& at, ParamField& out) {
  if (!j.is_array() || j.size() != 2) fail(at, "must be an array of a field's index and its value");
  out = ParamField{};
  read(j[0], At{&at, {}, 0}, out.index);
  read(j[1], At{&at, {}, 1}, out.value);
}

void read(const json& j, const At& at, ParamRowInfo& out) {
  out = ParamRowInfo{};
  Fields(j, at)("param", out.param)("id", out.id)("fields", out.fields).done();
}

void read(const json& j, const At& at, WorldSnapshot& out) {
  out = WorldSnapshot{};
  Fields(j, at)("seq", out.seq)("mono_ms", out.mono_ms)("gap", out.gap)("build", out.build)("health", out.health)(
      "session", out.session)("life", out.life)("ui", out.ui)("player", out.player)("place", out.place)(
      "clock", out.clock)("boss", out.boss)("chars", out.chars)("flags", out.flags)("new_text", out.new_text)(
      "new_params", out.new_params)
      .done();
}

// ---- The render feed ----

void read(const json& j, const At& at, Head& out) {
  out = Head{};
  Fields(j, at)("handle", out.handle)("pos", out.pos)("present", out.present).done();
}

void read(const json& j, const At& at, RenderFeed& out) {
  out = RenderFeed{};
  Fields(j, at)("seq", out.seq)("qpc_us", out.qpc_us)("view", out.view)("fov_rad", out.fov_rad)("aspect", out.aspect)(
      "camera_ok", out.camera_ok)("heads", out.heads)
      .done();
}

// ---- Lifecycle ----

void read(const json& j, const At& at, Lifecycle::DataReady& out) {
  out = Lifecycle::DataReady{};
  Fields(j, at)("build", out.build)("caps", out.caps).done();
}

void read(const json& j, const At& at, Lifecycle::PlaythroughReady& out) {
  out = Lifecycle::PlaythroughReady{};
  Fields(j, at)("slot", out.slot)("name", out.name)("archetype", out.archetype)("keepsake", out.keepsake)(
      "play_time_ms", out.play_time_ms)
      .done();
}

void read(const json& j, const At& at, Lifecycle::GroupTripped& out) {
  out = Lifecycle::GroupTripped{};
  Fields(j, at)("group", out.group).done();
}

void read(const json& j, const At& at, Lifecycle& out) {
  std::optional<Lifecycle::DataReady> data_ready;
  std::optional<Lifecycle::PlaythroughReady> playthrough_ready;
  std::optional<Lifecycle::GroupTripped> group_tripped;
  Fields(j, at)("data_ready", data_ready)("playthrough_ready", playthrough_ready)("group_tripped", group_tripped)
      .done();
  const int kinds = int{data_ready.has_value()} + int{playthrough_ready.has_value()} + int{group_tripped.has_value()};
  if (kinds != 1) fail(at, "needs exactly one of \"data_ready\", \"playthrough_ready\" and \"group_tripped\"");
  if (data_ready) {
    out.payload = *data_ready;
  } else if (playthrough_ready) {
    out.payload = *playthrough_ready;
  } else {
    out.payload = *group_tripped;
  }
}

// ---- Commands ----

void read(const json& j, const At& at, GameCommand::SetWatchFlags& out) {
  out = GameCommand::SetWatchFlags{};
  std::optional<std::vector<std::uint32_t>> ids;
  Fields(j, at)("version", out.version)("ids", ids).done();
  if (ids) {
    if (ids->size() > kMaxWatchFlags) {
      fail(At{&at, "ids", 0}, "has more than " + std::to_string(kMaxWatchFlags) + " ids");
    }
    out.ids = std::make_shared<const std::vector<std::uint32_t>>(std::move(*ids));
  }
}

void read(const json& j, const At& at, GameCommand::SetTracked& out) {
  out = GameCommand::SetTracked{};
  Fields(j, at)("handles", out.handles).done();
}

void read(const json& j, const At& at, GameCommand::ResolveText& out) {
  out = GameCommand::ResolveText{};
  Fields(j, at)("fmg", out.fmg)("ids", out.ids).done();
}

void read(const json& j, const At& at, GameCommand::ResolveParams& out) {
  out = GameCommand::ResolveParams{};
  Fields(j, at)("param", out.param)("ids", out.ids).done();
}

void read(const json& j, const At& at, GameCommand::Noop& out) {
  out = GameCommand::Noop{};
  Fields(j, at).done();
}

void read(const json& j, const At& at, GameCommand& out) {
  std::optional<GameCommand::SetWatchFlags> set_watch_flags;
  std::optional<GameCommand::SetTracked> set_tracked;
  std::optional<GameCommand::ResolveText> resolve_text;
  std::optional<GameCommand::ResolveParams> resolve_params;
  std::optional<GameCommand::Noop> noop;
  Fields(j, at)("set_watch_flags", set_watch_flags)("set_tracked", set_tracked)("resolve_text", resolve_text)(
      "resolve_params", resolve_params)("noop", noop)
      .done();
  const int kinds = int{set_watch_flags.has_value()} + int{set_tracked.has_value()} +
                    int{resolve_text.has_value()} + int{resolve_params.has_value()} + int{noop.has_value()};
  if (kinds != 1) {
    fail(at,
         "needs exactly one of \"set_watch_flags\", \"set_tracked\", \"resolve_text\", \"resolve_params\" and "
         "\"noop\"");
  }
  if (set_watch_flags) {
    out.payload = std::move(*set_watch_flags);
  } else if (set_tracked) {
    out.payload = *set_tracked;
  } else if (resolve_text) {
    out.payload = *resolve_text;
  } else if (resolve_params) {
    out.payload = *resolve_params;
  } else {
    out.payload = *noop;
  }
}

}  // namespace detail

namespace {

// ---- Writers: every field; an absent optional, and a float that is not finite, as null ----

json number(float v) { return std::isfinite(v) ? json(v) : json(nullptr); }

template <std::size_t N>
json numbers(const std::array<float, N>& values) {
  json out = json::array();
  for (const float v : values) out.push_back(number(v));
  return out;
}

template <std::size_t N>
json text(const FixedString<N>& s) {
  return json(std::string(s.view()));
}

template <class T>
json or_null(const std::optional<T>& value) {
  return value ? json(*value) : json(nullptr);
}

template <class T, std::size_t N>
json list(const FixedVector<T, N>& items) {
  json out = json::array();
  for (const T& item : items) out.push_back(json(item));
  return out;
}

json hex(const std::bitset<kMaxWatchFlags>& bits) {
  constexpr char kDigits[] = "0123456789abcdef";
  std::array<unsigned, kMaxWatchFlags / 8> bytes{};
  std::size_t used = 0;  // bytes up to the last that is not zero
  for (std::size_t byte = 0; byte < bytes.size(); ++byte) {
    for (std::size_t b = 0; b < 8; ++b) {
      if (bits.test(byte * 8 + b)) bytes[byte] |= 1u << b;
    }
    if (bytes[byte] != 0) used = byte + 1;
  }
  std::string out;
  out.reserve(used * 2);
  for (std::size_t byte = 0; byte < used; ++byte) {
    out += kDigits[(bytes[byte] >> 4) & 0xFu];
    out += kDigits[bytes[byte] & 0xFu];
  }
  return out;
}

json health_groups(const std::array<GroupHealth, kGroupCount>& groups) {
  json out = json::object();
  for (const Group g : detail::kGroups) out[to_string(g)] = json(groups[static_cast<std::size_t>(g)]);
  return out;
}

}  // namespace

// ---- to_json and from_json ----

void to_json(json& j, ChrHandle v) { j = v.raw; }
void from_json(const json& j, ChrHandle& v) { detail::read_root(j, "ChrHandle", v); }
void to_json(json& j, BlockId v) { j = v.packed; }
void from_json(const json& j, BlockId& v) { detail::read_root(j, "BlockId", v); }
void to_json(json& j, BuildKey v) { j = v.index; }
void from_json(const json& j, BuildKey& v) { detail::read_root(j, "BuildKey", v); }

void to_json(json& j, MultiplayRole v) { j = to_string(v); }
void from_json(const json& j, MultiplayRole& v) { detail::read_root(j, "MultiplayRole", v); }
void to_json(json& j, LobbyState v) { j = to_string(v); }
void from_json(const json& j, LobbyState& v) { detail::read_root(j, "LobbyState", v); }
void to_json(json& j, Screen v) { j = to_string(v); }
void from_json(const json& j, Screen& v) { detail::read_root(j, "Screen", v); }
void to_json(json& j, HudState v) { j = to_string(v); }
void from_json(const json& j, HudState& v) { detail::read_root(j, "HudState", v); }
void to_json(json& j, BodyType v) { j = to_string(v); }
void from_json(const json& j, BodyType& v) { detail::read_root(j, "BodyType", v); }
void to_json(json& j, Status v) { j = to_string(v); }
void from_json(const json& j, Status& v) { detail::read_root(j, "Status", v); }
void to_json(json& j, StatusSet v) { j = detail::write_set(v, detail::kStatuses); }
void from_json(const json& j, StatusSet& v) { detail::read_root(j, "StatusSet", v); }
void to_json(json& j, ParamKind v) { j = to_string(v); }
void from_json(const json& j, ParamKind& v) { detail::read_root(j, "ParamKind", v); }
void to_json(json& j, Group v) { j = to_string(v); }
void from_json(const json& j, Group& v) { detail::read_root(j, "Group", v); }
void to_json(json& j, GroupSet v) { j = detail::write_set(v, detail::kGroups); }
void from_json(const json& j, GroupSet& v) { detail::read_root(j, "GroupSet", v); }
void to_json(json& j, GroupState v) { j = to_string(v); }
void from_json(const json& j, GroupState& v) { detail::read_root(j, "GroupState", v); }

void to_json(json& j, const GroupHealth& v) {
  j = {{"state", to_string(v.state)}, {"phase_b", v.phase_b}, {"faults", v.faults}};
}
void from_json(const json& j, GroupHealth& v) { detail::read_root(j, "GroupHealth", v); }

void to_json(json& j, const ReadHealth& v) {
  j = {{"groups", health_groups(v.groups)}, {"task_us_last", v.task_us_last}, {"task_us_p99", v.task_us_p99}};
}
void from_json(const json& j, ReadHealth& v) { detail::read_root(j, "ReadHealth", v); }

void to_json(json& j, const Capabilities& v) { j = {{"groups", json(v.groups)}}; }
void from_json(const json& j, Capabilities& v) { detail::read_root(j, "Capabilities", v); }

void to_json(json& j, const Meter& v) { j = {{"cur", v.cur}, {"max", v.max}}; }
void from_json(const json& j, Meter& v) { detail::read_root(j, "Meter", v); }

void to_json(json& j, const Session& v) {
  j = {{"role", to_string(v.role)},           {"is_my_world", v.is_my_world}, {"lobby", to_string(v.lobby)},
       {"player_set_len", v.player_set_len}, {"anticheat", v.anticheat},     {"ersc_loaded", v.ersc_loaded}};
}
void from_json(const json& j, Session& v) { detail::read_root(j, "Session", v); }

void to_json(json& j, const Life& v) {
  j = {{"main_player", v.main_player}, {"screen", to_string(v.screen)}, {"load_counter", or_null(v.load_counter)}};
}
void from_json(const json& j, Life& v) { detail::read_root(j, "Life", v); }

void to_json(json& j, const Ui& v) {
  j = {{"hud", to_string(v.hud)},
       {"banner", or_null(v.banner)},
       {"popup_talk_id", or_null(v.popup_talk_id)},
       {"talk_cam", v.talk_cam},
       {"subtitles_on", v.subtitles_on}};
}
void from_json(const json& j, Ui& v) { detail::read_root(j, "Ui", v); }

void to_json(json& j, const Gear& v) {
  j = {{"right_weapon", v.right_weapon}, {"left_weapon", v.left_weapon}, {"head", v.head},
       {"chest", v.chest},               {"hands", v.hands},             {"legs", v.legs}};
}
void from_json(const json& j, Gear& v) { detail::read_root(j, "Gear", v); }

void to_json(json& j, const Player& v) {
  j = {{"handle", json(v.handle)},
       {"level", v.level},
       {"archetype", v.archetype},
       {"body", to_string(v.body)},
       {"hp", json(v.hp)},
       {"fp", json(v.fp)},
       {"stamina", json(v.stamina)},
       {"runes_held", v.runes_held},
       {"deaths", v.deaths},
       {"play_time_ms", v.play_time_ms},
       {"ng_lvl", v.ng_lvl},
       {"save_slot", v.save_slot},
       {"dead", v.dead},
       {"riding", v.riding},
       {"locked_on", or_null(v.locked_on)},
       {"last_hit_by", or_null(v.last_hit_by)},
       {"throw_state", v.throw_state},
       {"roundtable", v.roundtable},
       {"statuses", json(v.statuses)},
       {"block", json(v.block)},
       {"pos", numbers(v.pos)},
       {"gear", json(v.gear)},
       {"boss_flag", v.boss_flag}};
}
void from_json(const json& j, Player& v) { detail::read_root(j, "Player", v); }

void to_json(json& j, const Place& v) {
  j = {{"place_id", or_null(v.place_id)},
       {"region", text(v.region)},
       {"legacy", v.legacy},
       {"overworld", v.overworld}};
}
void from_json(const json& j, Place& v) { detail::read_root(j, "Place", v); }

void to_json(json& j, const Clock& v) { j = {{"h", v.h}, {"m", v.m}}; }
void from_json(const json& j, Clock& v) { detail::read_root(j, "Clock", v); }

void to_json(json& j, const BossBar& v) {
  j = {{"handle", json(v.handle)}, {"name_id", v.name_id}, {"damage_taken", v.damage_taken}};
}
void from_json(const json& j, BossBar& v) { detail::read_root(j, "BossBar", v); }

void to_json(json& j, const Boss& v) { j = {{"active", v.active}, {"bars", list(v.bars)}}; }
void from_json(const json& j, Boss& v) { detail::read_root(j, "Boss", v); }

void to_json(json& j, const Chr& v) {
  j = {{"handle", json(v.handle)},
       {"param", v.param},
       {"model", v.model},
       {"entity", v.entity},
       {"kind", v.kind},
       {"team", v.team},
       {"hp", json(v.hp)},
       {"dead", v.dead},
       {"last_hit_by", or_null(v.last_hit_by)},
       {"recv_dmg_type", v.recv_dmg_type},
       {"throw_state", v.throw_state},
       {"dist_m", number(v.dist_m)},
       {"bearing_deg", number(v.bearing_deg)},
       {"on_screen", v.on_screen},
       {"statuses", json(v.statuses)},
       {"asleep", or_null(v.asleep)},
       {"block", json(v.block)},
       {"pos", numbers(v.pos)}};
}
void from_json(const json& j, Chr& v) { detail::read_root(j, "Chr", v); }

void to_json(json& j, const FlagFrame& v) {
  j = {{"watch_version", v.watch_version}, {"bits", hex(v.bits)}, {"valid", hex(v.valid)}};
}
void from_json(const json& j, FlagFrame& v) { detail::read_root(j, "FlagFrame", v); }

void to_json(json& j, const TextEntry& v) {
  j = {{"fmg", v.fmg}, {"id", v.id}, {"text", v.text ? text(*v.text) : json(nullptr)}};
}
void from_json(const json& j, TextEntry& v) { detail::read_root(j, "TextEntry", v); }

void to_json(json& j, const ParamField& v) { j = json::array({v.index, v.value}); }
void from_json(const json& j, ParamField& v) { detail::read_root(j, "ParamField", v); }

void to_json(json& j, const ParamRowInfo& v) {
  j = {{"param", to_string(v.param)}, {"id", v.id}, {"fields", list(v.fields)}};
}
void from_json(const json& j, ParamRowInfo& v) { detail::read_root(j, "ParamRowInfo", v); }

void to_json(json& j, const WorldSnapshot& v) {
  j = {{"seq", v.seq},
       {"mono_ms", v.mono_ms},
       {"gap", v.gap},
       {"build", json(v.build)},
       {"health", json(v.health)},
       {"session", json(v.session)},
       {"life", json(v.life)},
       {"ui", json(v.ui)},
       {"player", or_null(v.player)},
       {"place", or_null(v.place)},
       {"clock", or_null(v.clock)},
       {"boss", json(v.boss)},
       {"chars", list(v.chars)},
       {"flags", or_null(v.flags)},
       {"new_text", list(v.new_text)},
       {"new_params", list(v.new_params)}};
}
void from_json(const json& j, WorldSnapshot& v) { detail::read_root(j, "WorldSnapshot", v); }

void to_json(json& j, const Head& v) {
  j = {{"handle", json(v.handle)}, {"pos", numbers(v.pos)}, {"present", v.present}};
}
void from_json(const json& j, Head& v) { detail::read_root(j, "Head", v); }

void to_json(json& j, const RenderFeed& v) {
  j = {{"seq", v.seq},
       {"qpc_us", v.qpc_us},
       {"view", numbers(v.view)},
       {"fov_rad", number(v.fov_rad)},
       {"aspect", number(v.aspect)},
       {"camera_ok", v.camera_ok},
       {"heads", list(v.heads)}};
}
void from_json(const json& j, RenderFeed& v) { detail::read_root(j, "RenderFeed", v); }

void to_json(json& j, const Lifecycle::DataReady& v) { j = {{"build", json(v.build)}, {"caps", json(v.caps)}}; }
void from_json(const json& j, Lifecycle::DataReady& v) { detail::read_root(j, "DataReady", v); }

void to_json(json& j, const Lifecycle::PlaythroughReady& v) {
  j = {{"slot", v.slot},
       {"name", text(v.name)},
       {"archetype", v.archetype},
       {"keepsake", v.keepsake},
       {"play_time_ms", v.play_time_ms}};
}
void from_json(const json& j, Lifecycle::PlaythroughReady& v) { detail::read_root(j, "PlaythroughReady", v); }

void to_json(json& j, const Lifecycle::GroupTripped& v) { j = {{"group", to_string(v.group)}}; }
void from_json(const json& j, Lifecycle::GroupTripped& v) { detail::read_root(j, "GroupTripped", v); }

void to_json(json& j, const Lifecycle& v) {
  if (const auto* data_ready = std::get_if<Lifecycle::DataReady>(&v.payload)) {
    j = {{"data_ready", json(*data_ready)}};
  } else if (const auto* playthrough_ready = std::get_if<Lifecycle::PlaythroughReady>(&v.payload)) {
    j = {{"playthrough_ready", json(*playthrough_ready)}};
  } else if (const auto* group_tripped = std::get_if<Lifecycle::GroupTripped>(&v.payload)) {
    j = {{"group_tripped", json(*group_tripped)}};
  }
}
void from_json(const json& j, Lifecycle& v) { detail::read_root(j, "Lifecycle", v); }

void to_json(json& j, const GameCommand::SetWatchFlags& v) {
  json ids = nullptr;
  if (v.ids) ids = *v.ids;
  j = {{"version", v.version}, {"ids", std::move(ids)}};
}
void from_json(const json& j, GameCommand::SetWatchFlags& v) { detail::read_root(j, "SetWatchFlags", v); }

void to_json(json& j, const GameCommand::SetTracked& v) { j = {{"handles", list(v.handles)}}; }
void from_json(const json& j, GameCommand::SetTracked& v) { detail::read_root(j, "SetTracked", v); }

void to_json(json& j, const GameCommand::ResolveText& v) { j = {{"fmg", v.fmg}, {"ids", list(v.ids)}}; }
void from_json(const json& j, GameCommand::ResolveText& v) { detail::read_root(j, "ResolveText", v); }

void to_json(json& j, const GameCommand::ResolveParams& v) {
  j = {{"param", to_string(v.param)}, {"ids", list(v.ids)}};
}
void from_json(const json& j, GameCommand::ResolveParams& v) { detail::read_root(j, "ResolveParams", v); }

void to_json(json& j, const GameCommand& v) {
  if (const auto* set_watch_flags = std::get_if<GameCommand::SetWatchFlags>(&v.payload)) {
    j = {{"set_watch_flags", json(*set_watch_flags)}};
  } else if (const auto* set_tracked = std::get_if<GameCommand::SetTracked>(&v.payload)) {
    j = {{"set_tracked", json(*set_tracked)}};
  } else if (const auto* resolve_text = std::get_if<GameCommand::ResolveText>(&v.payload)) {
    j = {{"resolve_text", json(*resolve_text)}};
  } else if (const auto* resolve_params = std::get_if<GameCommand::ResolveParams>(&v.payload)) {
    j = {{"resolve_params", json(*resolve_params)}};
  } else {
    j = {{"noop", json::object()}};
  }
}
void from_json(const json& j, GameCommand& v) { detail::read_root(j, "GameCommand", v); }

}  // namespace en::proto
