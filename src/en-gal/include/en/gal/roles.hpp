#pragma once
// What the game-access layer reads, by roles of our own (ARCH §6.1): the items discovery finds, the classes of the
// layout the read groups use and the fields they read, each under a label of ours. No name of the game is here: the
// game's names (the singletons' runtime-class names, the RTTI classes, the task group's name) and every offset and
// pattern come from gal/'s files, keyed by these labels (O27, O48, O89; WHY§123):
// - patterns.toml: the function and the statics by item label (`id = "register_task"`, `id = "save_data"`), and the
//   FD4 idiom as `id = "fd4_idiom"`, with its `path_suffix`;
// - the layout: a class per label (`[class.characters]`), whose `singleton` key names the FD4 singleton behind an item
//   and whose `rtti` key names the class its objects' vtables must have, and the fields below by name.
// A field is checked by its kind (a whole number of any width, a pointer, a bit, ...), not one exact type, so a layout
// says what the game holds and the reader widens it.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "en/gal/layout.hpp"
#include "en/proto/snapshot.hpp"

namespace en::gal::roles {

// ---- Items: what discovery finds (ARCH §6.1: 5 AOB items, 11 FD4 singletons) ----

enum class Item : std::uint8_t {
  RegisterTask,  // a function, by pattern: called once, to register our task (boot)
  ModuleHandle,  // a static, by pattern: non-null once the game is up (readiness)
  SaveData,      // a static, by pattern: play time, deaths, the player's sheet and the settings
  GameState,     // a static, by pattern: the save slot and the talk camera
  FieldArea,     // a static, by pattern: the place's id
  TaskHub,       // FD4 singletons from here on: the task hub, whose instance register_task takes
  Characters,    // the characters near the player, the main player and the player set
  Session,       // the online session's state
  EventFlags,    // the event flags
  Hud,           // the HUD's state, the banner and the boss bars
  Menus,         // the screen state and the talk popup
  Loading,       // the loading screens' counter (optional within ui)
  WorldClock,    // the time of day
  Camera,        // the view
  ParamStore,    // the param tables
  Messages,      // the text tables
};
inline constexpr std::size_t kItemCount = 16;

enum class ItemKind : std::uint8_t { Function, Static, Singleton };

struct ItemInfo {
  std::string_view label;
  ItemKind kind;
  bool required;  // ARCH §3.2: missing after the last retry → banner; an optional item only turns its group off
};

[[nodiscard]] const ItemInfo& info(Item item) noexcept;
[[nodiscard]] constexpr std::size_t index(Item item) noexcept { return static_cast<std::size_t>(item); }

inline constexpr std::string_view kFd4IdiomId = "fd4_idiom";

// ---- Classes of the layout, by label ----

enum class Cls : std::uint8_t {
  TaskHub, TaskGroup, Characters, NearEntry, PlayerSlot, Character, PlayerCharacter, Parts, Vitals, Motion,
  Reaction, Grip, SaveData, Settings, PlayerData, ItemId, GameState, FieldArea, Session, Hud, BossBar, Menus,
  Popup, Loading, WorldClock, Camera, View, EventFlags, FlagNode, Messages, Fmg, FmgGroup, ParamStore, ParamTable,
  ParamRow,
};
inline constexpr std::size_t kClsCount = static_cast<std::size_t>(Cls::ParamRow) + 1;

[[nodiscard]] std::string_view label(Cls c) noexcept;
// The class an item's instance has (the singletons' and the statics'); nothing for the function.
[[nodiscard]] std::optional<Cls> class_of(Item item) noexcept;

// ---- Fields ----

// What a field may be: a whole number of any width (u8 to u64, i32, bool8, handle), a pointer, a bit, a float, three
// floats, sixteen floats, UTF-16 text, an array, or a DL vector or map header.
enum class Kind : std::uint8_t { Int, Ptr, Bit, F32, Vec3, Mat4, Text, Array, Vector, Map };

enum class Fld : std::uint8_t {
  HubGroups, HubGroupCount, GroupName,
  MainPlayer, ByDistance, PlayerSet, PlayerSetLen, NearChr, NearDistance, SlotChr,
  ChrHandle, ChrParam, ChrModel, ChrEntity, ChrType, ChrTeam, ChrBlock, ChrOnScreen, ChrDead, ChrLastHit, ChrParts,
  PlayerLockedOn, PlayerRide,
  PartsVitals, PartsMotion, PartsReaction, PartsGrip,
  Hp, MaxHp, Fp, MaxFp, Stamina, MaxStamina, Pos, RecvDmg, Throw,
  PlayTime, Deaths, NgLevel, BossFight, SavePlayerData, SaveSettings, Subtitles,
  Level, Archetype, Body, Keepsake, Runes, Name, Role, MyWorld, Gear, ItemValue,
  SaveSlot, TalkCam, PlaceId, Lobby,
  HudState, Banner, BossBars, BarHandle, BarNameId, BarDamage,
  Screen, Popup, PopupTalk, LoadCounter, Hour, Minute,
  View, ViewMatrix, ViewFov, ViewAspect,
  Divisor, FlagMap, HolderBase, NodeLeft, NodeParent, NodeRight, NodeKey, NodeMode, NodeValue,
  MsgTables, MsgCount, FmgGroupCount, FmgGroupsOff, FmgOffsetsOff, FgOffsetIndex, FgFirst, FgLast,
  PsTables, PsCount, PtRows, PtRowCount, PtRowSize, PrId, PrData,
};
inline constexpr std::size_t kFldCount = static_cast<std::size_t>(Fld::PrData) + 1;

// The read group a field serves; `boot` for the task's own check.
struct Use {
  bool boot = false;
  proto::Group group = proto::Group::Session;
};

struct FieldSpec {
  Cls cls;
  std::string_view name;
  Kind kind;
  Use use;
  bool optional;  // its group runs without it (the field reads as absent)
};

[[nodiscard]] const FieldSpec& spec(Fld f) noexcept;
[[nodiscard]] constexpr std::size_t index(Fld f) noexcept { return static_cast<std::size_t>(f); }
[[nodiscard]] bool kind_matches(Kind kind, FieldType type) noexcept;

// The enum tables the groups map raw values through (ARCH §6.3), each with the names it may give.
enum class Enum : std::uint8_t { Lobby, Role, Screen, Hud, Body, Ride };
inline constexpr std::size_t kEnumCount = 6;
struct EnumSpec {
  std::string_view table;
  Fld field;
  std::span<const std::string_view> names;
};
[[nodiscard]] const EnumSpec& spec(Enum e) noexcept;

// ---- Groups ----

// The items a group needs (besides the classes and fields of its own); `Loading` is optional within ui.
[[nodiscard]] std::span<const Item> items_of(proto::Group g) noexcept;

// The layout bound to the roles: each class and field found once, at discovery, so that nothing on the game thread
// looks a name up.
class Bound {
 public:
  explicit Bound(const Layout& layout) noexcept;
  [[nodiscard]] const ClassLayout* cls(Cls c) const noexcept { return cls_[static_cast<std::size_t>(c)]; }
  [[nodiscard]] const FieldLayout* field(Fld f) const noexcept { return fld_[index(f)]; }
  [[nodiscard]] const EnumTable* table(Enum e) const noexcept { return enum_[static_cast<std::size_t>(e)]; }
  // A class's copy size (or stride, for an element class): what one guarded copy of its object takes.
  [[nodiscard]] std::uint32_t size_of(Cls c) const noexcept;
  [[nodiscard]] std::uint32_t stride_of(Cls c) const noexcept;
  // Why the group cannot run on this layout: a field missing or of the wrong kind, a class without a size, an enum
  // table missing or naming a value the snapshot does not know. Nothing when it can.
  struct Gap {
    std::string_view where;  // a class's or field's label
    const char* problem;
  };
  [[nodiscard]] std::optional<Gap> gap(proto::Group g) const noexcept;
  [[nodiscard]] std::optional<Gap> boot_gap() const noexcept;

 private:
  std::array<const ClassLayout*, kClsCount> cls_{};
  std::array<const FieldLayout*, kFldCount> fld_{};
  std::array<const EnumTable*, kEnumCount> enum_{};
};

}  // namespace en::gal::roles
