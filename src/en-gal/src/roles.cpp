// The roles the game-access layer reads by (roles.hpp): our own labels for items, classes and fields, and their binding
// to a layout. Every name here is ours; the game's arrive through gal/'s files.
#include "en/gal/roles.hpp"

#include <algorithm>

namespace en::gal::roles {

namespace {

using proto::Group;

// Each item's id is its number in Lodestar's catalogue, gal/ids.toml (proposal 0005 §4): the catalogue's label is the
// item's, or, for the function, its earlier label (`was`), which schema 1's reports and patterns.toml keep. A test
// holds the two together.
constexpr std::array<ItemInfo, kItemCount> kItems = {{
    {"register_task", ItemKind::Function, true, 2001},
    {"module_handle", ItemKind::Static, false, 1001},
    {"save_data", ItemKind::Static, true, 1002},
    {"game_state", ItemKind::Static, true, 1003},
    {"field_area", ItemKind::Static, false, 1004},
    {"task_hub", ItemKind::Singleton, true, 1005},
    {"characters", ItemKind::Singleton, true, 1006},
    {"session", ItemKind::Singleton, true, 1007},
    {"event_flags", ItemKind::Singleton, false, 1008},
    {"hud", ItemKind::Singleton, false, 1009},
    {"menus", ItemKind::Singleton, false, 1010},
    {"loading", ItemKind::Singleton, false, 1011},
    {"world_clock", ItemKind::Singleton, false, 1012},
    {"camera", ItemKind::Singleton, false, 1013},
    {"param_store", ItemKind::Singleton, false, 1014},
    {"messages", ItemKind::Singleton, false, 1015},
}};

constexpr std::array<std::string_view, kClsCount> kClasses = {
    "task_hub",    "task_group", "characters", "near_entry",  "player_slot", "character",  "player_character",
    "character_parts", "vitals", "motion",     "reaction",    "grip",        "save_data",  "settings",
    "player_data", "item_id",    "game_state", "field_area",  "session",     "hud",        "boss_bar",
    "menus",       "popup",      "loading",    "world_clock", "camera",      "view",       "event_flags",
    "flag_node",   "messages",   "fmg",        "fmg_group",   "param_store", "param_table", "param_row"};

constexpr Use B{true, Group::Session};
constexpr Use use(Group g) { return Use{false, g}; }
constexpr Use kSession = use(Group::Session), kWorld = use(Group::World), kCombat = use(Group::Combat),
              kPlayer = use(Group::Player), kFlags = use(Group::Flags), kText = use(Group::Text), kUi = use(Group::Ui),
              kPlace = use(Group::Place), kCamera = use(Group::Camera), kParams = use(Group::Params);

constexpr std::array<FieldSpec, kFldCount> kFields = {{
    {Cls::TaskHub, "groups", Kind::Ptr, B, false},
    {Cls::TaskHub, "group_count", Kind::Int, B, false},
    {Cls::TaskGroup, "name", Kind::Text, B, false},
    {Cls::Characters, "main_player", Kind::Ptr, kWorld, false},
    {Cls::Characters, "by_distance", Kind::Vector, kWorld, false},
    {Cls::Characters, "player_set", Kind::Ptr, kSession, false},
    {Cls::Characters, "player_set_len", Kind::Int, kSession, false},
    {Cls::NearEntry, "chr", Kind::Ptr, kWorld, false},
    {Cls::NearEntry, "distance", Kind::F32, kWorld, false},
    {Cls::PlayerSlot, "chr", Kind::Ptr, kSession, false},
    {Cls::Character, "handle", Kind::Int, kWorld, false},
    {Cls::Character, "npc_param_id", Kind::Int, kWorld, false},
    {Cls::Character, "model", Kind::Int, kWorld, false},
    {Cls::Character, "entity_id", Kind::Int, kWorld, false},
    {Cls::Character, "chr_type", Kind::Int, kWorld, false},
    {Cls::Character, "team", Kind::Int, kWorld, false},
    {Cls::Character, "block_id", Kind::Int, kWorld, false},
    {Cls::Character, "on_screen", Kind::Bit, kWorld, false},
    {Cls::Character, "dead", Kind::Bit, kWorld, false},
    {Cls::Character, "last_hit_by", Kind::Int, kCombat, false},
    {Cls::Character, "parts", Kind::Ptr, kWorld, false},
    {Cls::PlayerCharacter, "locked_on", Kind::Int, kCombat, true},
    {Cls::PlayerCharacter, "ride_state", Kind::Int, kPlayer, true},
    {Cls::Parts, "vitals", Kind::Ptr, kWorld, false},
    {Cls::Parts, "motion", Kind::Ptr, kWorld, false},
    {Cls::Parts, "reaction", Kind::Ptr, kCombat, false},
    {Cls::Parts, "grip", Kind::Ptr, kCombat, false},
    {Cls::Vitals, "hp", Kind::Int, kWorld, false},
    {Cls::Vitals, "max_hp", Kind::Int, kWorld, false},
    {Cls::Vitals, "fp", Kind::Int, kWorld, true},
    {Cls::Vitals, "max_fp", Kind::Int, kWorld, true},
    {Cls::Vitals, "stamina", Kind::Int, kWorld, true},
    {Cls::Vitals, "max_stamina", Kind::Int, kWorld, true},
    {Cls::Motion, "pos", Kind::Vec3, kWorld, false},
    {Cls::Reaction, "recv_dmg_type", Kind::Int, kCombat, false},
    {Cls::Grip, "throw_state", Kind::Int, kCombat, false},
    {Cls::SaveData, "play_time", Kind::Int, kPlayer, false},
    {Cls::SaveData, "death_count", Kind::Int, kPlayer, false},
    {Cls::SaveData, "ng_level", Kind::Int, kPlayer, false},
    {Cls::SaveData, "boss_fight", Kind::Int, kPlayer, true},
    {Cls::SaveData, "player_data", Kind::Ptr, kPlayer, false},
    {Cls::SaveData, "settings", Kind::Ptr, kUi, true},
    {Cls::Settings, "show_subtitles", Kind::Int, kUi, true},
    {Cls::PlayerData, "level", Kind::Int, kPlayer, false},
    {Cls::PlayerData, "archetype", Kind::Int, kPlayer, false},
    {Cls::PlayerData, "body", Kind::Int, kPlayer, false},
    {Cls::PlayerData, "keepsake", Kind::Int, kPlayer, false},
    {Cls::PlayerData, "runes", Kind::Int, kPlayer, false},
    {Cls::PlayerData, "name", Kind::Text, kPlayer, false},
    {Cls::PlayerData, "multiplay_role", Kind::Int, kSession, false},
    {Cls::PlayerData, "is_my_world", Kind::Int, kSession, false},
    {Cls::PlayerData, "gear", Kind::Array, kPlayer, true},
    {Cls::ItemId, "id", Kind::Int, kPlayer, true},
    {Cls::GameState, "save_slot", Kind::Int, kPlayer, false},
    {Cls::GameState, "talk_cam_id", Kind::Int, kUi, true},
    {Cls::FieldArea, "place_id", Kind::Int, kPlace, false},
    {Cls::Session, "lobby_state", Kind::Int, kSession, false},
    {Cls::Hud, "hud_state", Kind::Int, kUi, false},
    {Cls::Hud, "banner_id", Kind::Int, kUi, false},
    {Cls::Hud, "boss_bars", Kind::Array, kUi, false},
    {Cls::BossBar, "handle", Kind::Int, kUi, false},
    {Cls::BossBar, "name_id", Kind::Int, kUi, false},
    {Cls::BossBar, "damage_taken", Kind::Int, kUi, false},
    {Cls::Menus, "screen_state", Kind::Int, kUi, false},
    {Cls::Menus, "popup", Kind::Ptr, kUi, true},
    {Cls::Popup, "talk_id", Kind::Int, kUi, true},
    {Cls::Loading, "load_counter", Kind::Int, kUi, true},
    {Cls::WorldClock, "hour", Kind::Int, kPlace, false},
    {Cls::WorldClock, "minute", Kind::Int, kPlace, false},
    {Cls::Camera, "view", Kind::Ptr, kCamera, false},
    {Cls::View, "matrix", Kind::Mat4, kCamera, false},
    {Cls::View, "fov", Kind::F32, kCamera, false},
    {Cls::View, "aspect", Kind::F32, kCamera, true},
    {Cls::EventFlags, "divisor", Kind::Int, kFlags, false},
    {Cls::EventFlags, "map", Kind::Map, kFlags, false},
    {Cls::EventFlags, "holder_base", Kind::Ptr, kFlags, false},
    {Cls::FlagNode, "left", Kind::Ptr, kFlags, false},
    {Cls::FlagNode, "parent", Kind::Ptr, kFlags, false},
    {Cls::FlagNode, "right", Kind::Ptr, kFlags, false},
    {Cls::FlagNode, "key", Kind::Int, kFlags, false},
    {Cls::FlagNode, "mode", Kind::Int, kFlags, false},
    {Cls::FlagNode, "value", Kind::Int, kFlags, false},
    {Cls::Messages, "tables", Kind::Ptr, kText, false},
    {Cls::Messages, "count", Kind::Int, kText, false},
    {Cls::Fmg, "group_count", Kind::Int, kText, false},
    {Cls::Fmg, "groups_off", Kind::Int, kText, false},
    {Cls::Fmg, "offsets_off", Kind::Int, kText, false},
    {Cls::FmgGroup, "offset_index", Kind::Int, kText, false},
    {Cls::FmgGroup, "first_id", Kind::Int, kText, false},
    {Cls::FmgGroup, "last_id", Kind::Int, kText, false},
    {Cls::ParamStore, "tables", Kind::Ptr, kParams, false},
    {Cls::ParamStore, "count", Kind::Int, kParams, false},
    {Cls::ParamTable, "rows", Kind::Ptr, kParams, false},
    {Cls::ParamTable, "row_count", Kind::Int, kParams, false},
    {Cls::ParamTable, "row_size", Kind::Int, kParams, true},
    {Cls::ParamRow, "id", Kind::Int, kParams, false},
    {Cls::ParamRow, "data", Kind::Ptr, kParams, false},
}};

constexpr std::string_view kLobbyNames[] = {"None", "Hosting", "Joined", "Other"};
constexpr std::string_view kRoleNames[] = {"Host", "Cooperator", "Invader", "Other"};
constexpr std::string_view kScreenNames[] = {"InGame", "Loading", "Title"};
constexpr std::string_view kHudNames[] = {"HideAll", "ShowAll", "PopupMenu", "Default"};
constexpr std::string_view kBodyNames[] = {"A", "B"};
constexpr std::string_view kRideNames[] = {"OnFoot", "Riding"};
constexpr std::array<EnumSpec, kEnumCount> kEnums = {{
    {"lobby", Fld::Lobby, kLobbyNames},
    {"multiplay_role", Fld::Role, kRoleNames},
    {"screen", Fld::Screen, kScreenNames},
    {"hud", Fld::HudState, kHudNames},
    {"body", Fld::Body, kBodyNames},
    {"ride", Fld::PlayerRide, kRideNames},
}};

constexpr Item kSessionItems[] = {Item::Session, Item::SaveData, Item::Characters};
constexpr Item kWorldItems[] = {Item::Characters};
constexpr Item kPlayerItems[] = {Item::SaveData, Item::GameState, Item::Characters};
constexpr Item kFlagsItems[] = {Item::EventFlags};
constexpr Item kTextItems[] = {Item::Messages};
constexpr Item kUiItems[] = {Item::Hud, Item::Menus};
constexpr Item kPlaceItems[] = {Item::FieldArea, Item::WorldClock};
constexpr Item kCameraItems[] = {Item::Camera};
constexpr Item kParamsItems[] = {Item::ParamStore};

// Fields of another group's that a group's reads go through (combat's characters are world's walk; session's sheet is
// reached through the save data).
constexpr Fld kSessionAlso[] = {Fld::SavePlayerData};
constexpr Fld kCombatAlso[] = {Fld::MainPlayer, Fld::ByDistance, Fld::NearChr, Fld::NearDistance, Fld::ChrHandle,
                               Fld::ChrParts};
constexpr Fld kPlayerAlso[] = {Fld::MainPlayer};
std::span<const Fld> also_of(Group g) {
  switch (g) {
    case Group::Session: return kSessionAlso;
    case Group::Combat: return kCombatAlso;
    case Group::Player: return kPlayerAlso;
    default: return {};
  }
}

bool is_int(FieldType t) {
  switch (t) {
    case FieldType::U8:
    case FieldType::U16:
    case FieldType::U32:
    case FieldType::U64:
    case FieldType::I32:
    case FieldType::Bool8:
    case FieldType::Handle: return true;
    default: return false;
  }
}

}  // namespace

const ItemInfo& info(Item item) noexcept { return kItems[index(item)]; }
std::string_view label(Cls c) noexcept { return kClasses[static_cast<std::size_t>(c)]; }
const FieldSpec& spec(Fld f) noexcept { return kFields[index(f)]; }
const EnumSpec& spec(Enum e) noexcept { return kEnums[static_cast<std::size_t>(e)]; }

std::optional<Cls> class_of(Item item) noexcept {
  switch (item) {
    case Item::RegisterTask:
    case Item::ModuleHandle: return std::nullopt;
    case Item::SaveData: return Cls::SaveData;
    case Item::GameState: return Cls::GameState;
    case Item::FieldArea: return Cls::FieldArea;
    case Item::TaskHub: return Cls::TaskHub;
    case Item::Characters: return Cls::Characters;
    case Item::Session: return Cls::Session;
    case Item::EventFlags: return Cls::EventFlags;
    case Item::Hud: return Cls::Hud;
    case Item::Menus: return Cls::Menus;
    case Item::Loading: return Cls::Loading;
    case Item::WorldClock: return Cls::WorldClock;
    case Item::Camera: return Cls::Camera;
    case Item::ParamStore: return Cls::ParamStore;
    case Item::Messages: return Cls::Messages;
  }
  return std::nullopt;
}

bool kind_matches(Kind kind, FieldType type) noexcept {
  switch (kind) {
    case Kind::Int: return is_int(type);
    case Kind::Ptr: return type == FieldType::Ptr;
    case Kind::Bit: return type == FieldType::Bit || type == FieldType::Bool8;
    case Kind::F32: return type == FieldType::F32;
    case Kind::Vec3: return type == FieldType::Vec3f;
    case Kind::Mat4: return type == FieldType::Mat4f;
    case Kind::Text: return type == FieldType::Utf16;
    case Kind::Array: return type == FieldType::Array;
    case Kind::Vector: return type == FieldType::DlVector;
    case Kind::Map: return type == FieldType::DlMap;
  }
  return false;
}

std::span<const Item> items_of(Group g) noexcept {
  switch (g) {
    case Group::Session: return kSessionItems;
    case Group::World:
    case Group::Combat: return kWorldItems;
    case Group::Player: return kPlayerItems;
    case Group::Flags: return kFlagsItems;
    case Group::Text: return kTextItems;
    case Group::Ui: return kUiItems;
    case Group::Place: return kPlaceItems;
    case Group::Camera: return kCameraItems;
    case Group::Params: return kParamsItems;
    case Group::Status: return {};
  }
  return {};
}

Bound::Bound(const Layout& layout) noexcept {
  for (std::size_t c = 0; c < kClsCount; ++c) cls_[c] = layout.find_class(kClasses[c]);
  for (std::size_t f = 0; f < kFldCount; ++f) {
    const ClassLayout* c = cls_[static_cast<std::size_t>(kFields[f].cls)];
    const FieldLayout* field = c != nullptr ? c->field(kFields[f].name) : nullptr;
    fld_[f] = field != nullptr && kind_matches(kFields[f].kind, field->type) ? field : nullptr;
  }
  for (std::size_t e = 0; e < kEnumCount; ++e) {
    const FieldLayout* field = fld_[index(kEnums[e].field)];
    const EnumTable* t = field != nullptr && !field->enum_table.empty() ? layout.find_enum(field->enum_table) : nullptr;
    // Every value it gives must be a name the snapshot knows.
    if (t != nullptr) {
      for (const auto& [raw, name] : t->values) {
        if (std::find(kEnums[e].names.begin(), kEnums[e].names.end(), name) == kEnums[e].names.end()) t = nullptr;
        if (t == nullptr) break;
      }
    }
    enum_[e] = t;
  }
}

std::uint32_t Bound::size_of(Cls c) const noexcept {
  const ClassLayout* k = cls(c);
  if (k == nullptr) return 0;
  return k->copy_size ? *k->copy_size : k->stride.value_or(0);
}

std::uint32_t Bound::stride_of(Cls c) const noexcept {
  const ClassLayout* k = cls(c);
  if (k == nullptr) return 0;
  return k->stride ? *k->stride : k->copy_size.value_or(0);
}

namespace {
std::optional<Bound::Gap> check_field(const Bound& b, Fld f) {
  const FieldSpec& s = kFields[index(f)];
  if (b.cls(s.cls) == nullptr) return Bound::Gap{label(s.cls), "is not a class of the layout"};
  if (b.size_of(s.cls) == 0) return Bound::Gap{label(s.cls), "has no copy size or stride"};
  if (b.field(f) == nullptr) return Bound::Gap{s.name, "is missing from its class, or of another kind"};
  return std::nullopt;
}
}  // namespace

std::optional<Bound::Gap> Bound::gap(Group g) const noexcept {
  if (g == Group::Status) return Gap{"status", "is off until S8 verifies it (O40)"};
  for (std::size_t f = 0; f < kFldCount; ++f) {
    const FieldSpec& s = kFields[f];
    if (s.use.boot || s.use.group != g || s.optional) continue;
    if (auto gap = check_field(*this, static_cast<Fld>(f))) return gap;
  }
  for (const Fld f : also_of(g)) {
    if (auto gap = check_field(*this, f)) return gap;
  }
  for (std::size_t e = 0; e < kEnumCount; ++e) {
    const FieldSpec& s = kFields[index(kEnums[e].field)];
    if (s.use.boot || s.use.group != g || fld_[index(kEnums[e].field)] == nullptr) continue;
    if (enum_[e] == nullptr) return Gap{kEnums[e].table, "is missing, or names a value the snapshot does not know"};
  }
  return std::nullopt;
}

std::optional<Bound::Gap> Bound::boot_gap() const noexcept {
  for (std::size_t f = 0; f < kFldCount; ++f) {
    if (!kFields[f].use.boot) continue;
    if (auto gap = check_field(*this, static_cast<Fld>(f))) return gap;
  }
  return std::nullopt;
}

}  // namespace en::gal::roles
