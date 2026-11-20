#pragma once
// The fake game: our own Windows program that stands in for a game in EldenNet's tests (README.md). Every name, class,
// path, string, byte pattern, offset and number in it is invented for these tests; nothing is the game's.
//
// Two namespaces: `fakegame` holds the program's own code, and `FG` the invented classes whose run-time type
// information (RTTI) the walk of ARCH §6.2 is to find, as `.?AV<Class>@FG@@`. The `extern "C"` names are the symbols
// that asm.cpp's assembly defines or calls.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>

// --- Offsets that asm.cpp's assembly uses (each checked against the C++ layout with a static_assert) -------------------
#define FG_LOOM_GROUP_COUNT_OFF 0x10  // FG::TaskLoomImp: the number of task groups (the registration front's bound)
#define FG_LOOM_LAST_TASK_OFF 0x30    // FG::TaskLoomImp: the last task registered, which the registration keeps there
#define FG_GROUP_BOUND 0x200          // the registration refuses a group index at or past it, before the count
#define FG_WARDEN_SAVE_SLOT_OFF 0x14  // FG::RunWardenImp: the save slot
#define FG_PLOT_PLACE_OFF 0x1C        // FG::FieldPlotImp: the place id
#define FG_RTCLASS_NAME_OFF 0x18      // a runtime-class object: its name pointer (DLRF's role; ARCH §6.2 step 3)

namespace FG {
class TaskUnit;
class TaskLoomImp;
}  // namespace FG

extern "C" {
// Statics in .data, each 8-aligned (asm.cpp). The FD4-style ones are read through the accessors below.
extern void* fg_static_taskloom;      // FG::TaskLoomImp*: the task hub (CSTask's role)
extern void* fg_static_roster;        // FG::ActorRosterImp* (WorldChrMan's role)
extern void* fg_static_lobby;         // FG::LobbyWardenImp* (CSSessionManager's role)
extern void* fg_static_marks;         // FG::MarkLedgerImp* (CSEventFlagMan's role)
extern void* fg_static_gauges;        // FG::GaugeBoardImp* (CSFeMan's role)
extern void* fg_static_menu;          // FG::MenuDeskImp* (CSMenuMan's role)
extern void* fg_static_veil;          // FG::VeilKeeperImp* (CSNowLoadingHelper's role)
extern void* fg_static_sky;           // FG::SkyClockImp* (WorldAreaTime's role)
extern void* fg_static_eye;           // FG::EyeRigImp* (CSCamera's role)
extern void* fg_static_tables;        // FG::TableVaultImp* (SoloParamRepository's role)
extern void* fg_static_phrases;       // FG::PhraseBookImp* (MsgRepository's role)
extern void* fg_static_duplicata_a;   // decoys: two statics behind one name, so that name is ambiguous
extern void* fg_static_duplicata_b;
extern void* fg_static_strayling;     // decoy: its site's path does not end in FD4Singleton.h
extern void* fg_static_save_ledger;   // AOB statics: FG::SaveLedgerImp* (GameDataMan's role)
extern void* fg_static_run_warden;    // FG::RunWardenImp* (GameMan's role)
extern void* fg_static_field_plot;    // FG::FieldPlotImp* (FieldArea's role)
extern void* fg_static_hinstance;     // the module handle, non-null once the program is up (global_hinstance's role)
extern std::uint64_t fg_hinstance_check;
extern std::uint8_t fg_save_ledger_ready;
extern const void* const fg_static_rockbound;  // decoy: a "static" in .rdata, which the placement check refuses

// Runtime-class objects in .data, one per name (asm.cpp), and the code that the finder of ARCH §6.2 is to find.
extern std::uint8_t fg_rtclass_taskloom[];
extern const char fg_fd4_path[];        // ends in FD4Singleton.h
extern const char fg_fd4_path_decoy[];  // ends in FD4Singleton.hpp

const char* fg_rtclass_name(const void* rtclass);  // the name getter: the start of a .pdata function
void* fg_get_taskloom();
void* fg_get_roster();
void* fg_get_roster_near();  // a second site for the same name and static
void* fg_get_lobby();
void* fg_get_marks();
void* fg_get_gauges();
void* fg_get_menu();
void* fg_get_menu_popup_site();  // a second site
void* fg_get_veil();
void* fg_get_sky();
void* fg_get_eye();
void* fg_get_tables();
void* fg_get_phrases();
void* fg_get_duplicata_a();
void* fg_get_duplicata_b();
void* fg_get_strayling();
void* fg_get_rockbound();

// The AOB items' code (asm.cpp).
void* fg_boot_publish_instance(void* hinstance);  // stores the module handle (global_hinstance)
void* fg_save_ledger_swap(void* instance);        // swaps the SaveLedger static, answers the old value
std::int32_t fg_run_warden_slot();                // the save slot through the RunWarden static, or -1
std::int64_t fg_field_plot_place();               // the place id through the FieldPlot static, or 0

// The task registration: our own invented ABI, (task in rcx, the hub instance in rdx, the group index in r8d).
void fg_register_task(FG::TaskUnit* task, FG::TaskLoomImp* hub, std::uint32_t group);
// The program's own call sites into it: four tasks, each {task, group}, registered in order.
struct FgBootTask {
  FG::TaskUnit* task;
  std::uint32_t group;
  std::uint32_t spare;
};
void fg_boot_register_tasks(const FgBootTask* tasks);  // exactly four entries

// Called from the assembly (defined in C++).
void fg_fd4_unset(const char* path, const char* name, std::uint32_t site);  // a singleton used before it was set
void fg_hub_enqueue(FG::TaskLoomImp* hub, std::uint32_t group, FG::TaskUnit* task);
void fg_register_rejected(std::uint32_t reason);  // 1 hub, 2 group, 3 task; it faults, as a game given bad input would
}

namespace FG {

// --- The task interface (the game's own task base; ARCH §6.4's "task object shape") -----------------------------------
// A task is an object whose first field is a vtable pointer. The hub calls slot 0 once, on the game thread, when it
// activates the task; slot 1 every frame of the task's group; slot 2 every 64th frame. It writes the fields below
// into every task it holds, so a task object is at least 0x20 bytes: the vtable pointer, then 0x18 bytes the hub owns.
struct FrameInfo {
  std::uint64_t frame;  // 0x00: the frame's number
  float dt;             // 0x08: seconds since the previous frame
  std::uint32_t group;  // 0x0C: the group being run
};
struct TaskFields {
  std::uint32_t group;   // +0x08: written at registration
  std::uint32_t state;   // +0x0C: 1 queued, 2 active
  void* hub;             // +0x10: written at registration
  std::uint64_t runs;    // +0x18: incremented after every run
};
inline constexpr std::uint32_t kTaskSlots = 3;
inline constexpr std::uint32_t kAttachSlot = 0;
inline constexpr std::uint32_t kRunSlot = 1;
inline constexpr std::uint32_t kQuerySlot = 2;
inline constexpr std::uint32_t kQueryEvery = 64;
inline constexpr std::size_t kTaskFieldsSize = sizeof(TaskFields);  // the bytes after the vtable pointer: 0x18

class TaskUnit {
 public:
  TaskUnit() = default;
  TaskUnit(const TaskUnit&) = delete;
  TaskUnit& operator=(const TaskUnit&) = delete;
  virtual void attach(void* hub);                // slot 0
  virtual void run(const FrameInfo* frame) = 0;  // slot 1
  virtual std::uint32_t query();                 // slot 2
  TaskFields t{};

 protected:
  ~TaskUnit() = default;  // tasks live as long as the program
};

// --- The task hub (CSTask's role) ---------------------------------------------------------------------------------------
struct GroupEntry {           // stride 0x90
  std::uint32_t index;        // 0x00
  std::uint32_t flags;        // 0x04: bit 0 active
  char16_t name[64];          // 0x08: UTF-16, NUL-terminated
  std::uint32_t task_count;   // 0x88
  std::uint32_t spare;        // 0x8C
};
struct TaskLoomFields {       // from +0x08
  std::uint64_t frame;        // 0x08
  std::uint32_t group_count;  // 0x10 (FG_LOOM_GROUP_COUNT_OFF)
  std::uint32_t flags;        // 0x14
  GroupEntry* groups;         // 0x18
  std::uint64_t tasks_live;   // 0x20
  void** live;                // 0x28: every active task, in the order the hub activated it (kMaxLiveTasks)
  void* last_task;            // 0x30 (FG_LOOM_LAST_TASK_OFF): written by the registration itself (R1's callee summary)
  std::uint64_t spare;        // 0x38
};
inline constexpr std::uint32_t kMaxLiveTasks = 256;

// Every singleton class has a virtual function of its own, so each has a vtable and RTTI.
class TaskLoomImp {
 public:
  virtual std::uint32_t loom_kind() const;
  TaskLoomFields f{};
};

// --- Actors (ChrIns's role) and their parts -----------------------------------------------------------------------------
struct VitalsPart {           // health and the like
  std::int32_t max_hp;        // 0x00
  std::int32_t hp;            // 0x04
  std::int32_t max_fp;        // 0x08
  std::int32_t fp;            // 0x0C
  std::int32_t max_stamina;   // 0x10
  std::int32_t stamina;       // 0x14
};
struct MotionPart {
  std::uint8_t spare[0x20];
  float pos[4];               // 0x20: x, y, z, 1
};
struct ReactPart {
  std::int32_t spare;
  std::int32_t recv_dmg_type; // 0x04
};
struct GripPart {
  std::uint8_t spare[3];
  std::uint8_t throw_state;   // 0x03
};
struct ActorParts {           // the modules block
  VitalsPart* vitals;         // 0x00
  std::uint64_t spare;        // 0x08
  MotionPart* motion;         // 0x10
  ReactPart* react;           // 0x18
  GripPart* grip;             // 0x20
};
inline constexpr std::uint8_t kActorOnScreenBit = 2;  // ActorFields::flags
inline constexpr std::uint8_t kActorDeadBit = 6;

struct ActorFields {          // from +0x08
  std::uint32_t entity_id;    // 0x08
  std::uint32_t model;        // 0x0C
  std::uint64_t handle;       // 0x10
  std::uint32_t npc_param_id; // 0x18
  std::int32_t chr_type;      // 0x1C
  std::uint32_t block_id;     // 0x20
  std::uint8_t team;          // 0x24
  std::uint8_t flags;         // 0x25: bit 2 on screen, bit 6 dead
  std::uint16_t spare0;       // 0x26
  std::uint64_t last_hit_by;  // 0x28
  ActorParts* parts;          // 0x30
  std::uint8_t spare1[0x28];  // 0x38
};

class Actor {
 public:
  Actor() = default;
  Actor(const Actor&) = default;
  Actor& operator=(const Actor&) = delete;
  virtual ~Actor() = default;
  virtual void think(float dt);
  virtual std::uint32_t actor_kind() const;
  ActorFields a{};
};

struct PlayerFields {         // after Actor
  std::uint64_t locked_on;    // a handle, or 0
  std::uint8_t mount_state;   // 3 mounted, 0 on foot
  std::uint8_t spare[7];
};
class PlayerActor : public Actor {
 public:
  void think(float dt) override;
  std::uint32_t actor_kind() const override;
  PlayerFields p{};
};

struct NpcFields {
  std::uint32_t think_id;
  std::uint32_t spare;
};
class NpcActor : public Actor {
 public:
  std::uint32_t actor_kind() const override;
  NpcFields n{};
};

class BeastActor : public NpcActor {  // two levels below Actor
 public:
  void think(float dt) override;
  std::uint32_t actor_kind() const override;
  std::uint64_t pack = 0;
};

class Saddle {  // a second base with a vtable of its own, so MountActor has two vtables (and two locators)
 public:
  Saddle() = default;
  Saddle(const Saddle&) = default;
  Saddle& operator=(const Saddle&) = delete;
  virtual ~Saddle() = default;
  virtual std::uint32_t seats() const;
  std::uint32_t rider_count = 0;
  std::uint32_t spare = 0;
};
class MountActor : public NpcActor, public Saddle {
 public:
  std::uint32_t actor_kind() const override;
  std::uint32_t seats() const override;
  std::uint64_t gait = 0;
};

class Prop {  // not an actor: the actor set must leave it out
 public:
  Prop() = default;
  Prop(const Prop&) = default;
  Prop& operator=(const Prop&) = delete;
  virtual ~Prop() = default;
  virtual std::uint32_t prop_kind() const;
  std::uint64_t id = 0;
};

class Tombstone {  // what a moved object's old place holds once it is poisoned
 public:
  virtual std::uint32_t tomb_kind() const;
  std::uint64_t moved_to = 0;
};

// --- The player's sheet (PlayerGameData's role) and the save ledger (GameDataMan's role) --------------------------------
struct HeroFields {           // from +0x08
  std::uint32_t level;        // 0x08
  std::uint32_t runes;        // 0x0C
  std::uint8_t archetype;     // 0x10
  std::uint8_t body;          // 0x11
  std::uint8_t keepsake;      // 0x12
  std::uint8_t multiplay_role;// 0x13: 2 host, 3 guest
  std::uint8_t is_my_world;   // 0x14
  std::uint8_t spare0[3];     // 0x15
  char16_t name[17];          // 0x18: UTF-16, NUL-terminated
  std::uint16_t spare1[3];    // 0x3A
  std::uint32_t gear[6];      // 0x40
};
class HeroSheet {
 public:
  virtual std::uint32_t sheet_kind() const;
  HeroFields h{};
};

struct SettingsBlock {
  std::uint8_t spare[5];
  std::uint8_t show_subtitles;  // 0x05
  std::uint8_t spare1[10];
};
struct SaveLedgerFields {       // from +0x08
  HeroSheet* hero;              // 0x08
  SettingsBlock* settings;      // 0x10
  std::uint32_t death_count;    // 0x18
  std::uint32_t ng_level;       // 0x1C
  std::uint32_t play_time_ms;   // 0x20
  std::uint8_t boss_fight;      // 0x24
  std::uint8_t spare[3];
};
class SaveLedgerImp {
 public:
  virtual std::uint32_t ledger_kind() const;
  SaveLedgerFields f{};
};

struct RunWardenFields {           // from +0x08
  std::uint64_t spare;             // 0x08
  std::int32_t talk_cam_param_id;  // 0x10
  std::int32_t save_slot;          // 0x14 (FG_WARDEN_SAVE_SLOT_OFF)
};
class RunWardenImp {
 public:
  virtual std::uint32_t warden_kind() const;
  RunWardenFields f{};
};

struct FieldPlotFields {      // from +0x08
  std::uint32_t spare[5];     // 0x08
  std::int32_t place_id;      // 0x1C (FG_PLOT_PLACE_OFF)
};
class FieldPlotImp {
 public:
  virtual std::uint32_t plot_kind() const;
  FieldPlotFields f{};
};

// --- The roster (WorldChrMan's role) ------------------------------------------------------------------------------------
struct NearEntry {            // stride 0x10
  Actor* actor;               // 0x00
  float distance;             // 0x08: metres from the main player
  std::uint32_t spare;        // 0x0C
};
struct PlayerSlot {           // stride 0x10
  Actor* actor;
  std::uint64_t spare;
};
inline constexpr std::uint32_t kMaxNear = 48;
struct ActorRosterFields {    // from +0x08
  std::uint64_t spare0;       // 0x08
  NearEntry* near_begin;      // 0x10: nearest first
  NearEntry* near_end;        // 0x18
  NearEntry* near_cap;        // 0x20
  std::uint64_t spare1;       // 0x28
  PlayerSlot* players;        // 0x30
  std::uint32_t player_count; // 0x38
  std::uint32_t spare2;       // 0x3C
  Actor* main_player;         // 0x40: null at the title and while loading
};
class ActorRosterImp {
 public:
  virtual std::uint32_t roster_kind() const;
  ActorRosterFields f{};
};

// --- The other singletons ------------------------------------------------------------------------------------------------
class LobbyWardenImp {  // CSSessionManager's role
 public:
  virtual std::uint32_t lobby_kind() const;
  std::uint64_t spare = 0;        // 0x08
  std::int32_t lobby_state = 10;  // 0x10: 10 solo, 11 hosting, 12 visiting
  std::int32_t spare1 = 0;
};

struct PopupState {
  std::int32_t spare;
  std::int32_t talk_id;  // 0x04
};
class MenuDeskImp {  // CSMenuMan's role
 public:
  virtual std::uint32_t menu_kind() const;
  std::uint32_t spare = 0;        // 0x08
  std::int32_t screen_state = 1;  // 0x0C: 1 title, 2 loading, 4 in game
  PopupState* popup = nullptr;    // 0x10
};

struct GaugeBar {
  std::uint64_t handle;
  std::int32_t name_id;
  std::int32_t damage;
};
class GaugeBoardImp {  // CSFeMan's role
 public:
  virtual std::uint32_t gauge_kind() const;
  std::int32_t hud_state = 3;  // 0x08: 0 to 3
  std::int32_t banner_id = 0;  // 0x0C
  GaugeBar bars[3]{};          // 0x10, stride 0x10
};

class VeilKeeperImp {  // CSNowLoadingHelper's role
 public:
  virtual std::uint32_t veil_kind() const;
  std::uint32_t load_counter = 0;  // 0x08
  std::uint32_t spare = 0;
};

class SkyClockImp {  // WorldAreaTime's role
 public:
  virtual std::uint32_t sky_kind() const;
  std::uint64_t spare = 0;  // 0x08
  std::uint64_t clock = 0;  // 0x10: hours << 32 | minutes << 16 | seconds
};

struct ViewData {
  float matrix[16];  // 0x00: row-major, rotation in the upper 3x3
  float fov;         // 0x40: radians
  float aspect;      // 0x44
};
class EyeRigImp {  // CSCamera's role
 public:
  virtual std::uint32_t eye_kind() const;
  ViewData* view = nullptr;  // 0x08
};

// The flag store (ARCH §6.9's shape, our own layout): a binary tree of groups (an id's group is id / 1000) under a
// sentinel head whose parent is the root and to which the leaves point, each node naming its group's block of 125
// bytes (1,000 flags, the first flag of a byte in its top bit) by an offset from the holder (mode 1) or by address
// (mode 2).
struct FlagNode {
  FlagNode* left;       // 0x00
  FlagNode* parent;     // 0x08
  FlagNode* right;      // 0x10
  std::uint32_t key;    // 0x18: the group
  std::uint32_t mode;   // 0x1C
  std::uint64_t value;  // 0x20
};
class MarkLedgerImp {  // CSEventFlagMan's role
 public:
  virtual std::uint32_t mark_kind() const;
  std::uint32_t divisor = 1000;     // 0x08
  std::uint32_t spare = 0;
  FlagNode* map_head = nullptr;     // 0x10: the sentinel head
  std::uint64_t map_count = 0;      // 0x18
  std::uint8_t* holder = nullptr;   // 0x20: the blocks of mode 1
};

// The param tables (our own layout): a table per slot, each a sorted list of row references, each row's bytes.
struct ParamRowRef {   // stride 0x10
  std::uint32_t id;    // 0x00
  std::uint32_t spare;
  const void* data;    // 0x08
};
struct ParamSheet {
  const ParamRowRef* rows;   // 0x00
  std::uint16_t row_count;   // 0x08
  std::uint16_t row_size;    // 0x0A
  std::uint32_t spare;
  std::uint64_t spare1;      // to 0x18
};
struct NpcRow {              // 0x10 bytes
  std::int32_t name_id;      // 0x00
  std::uint8_t team;         // 0x04
  std::uint8_t spare[11];
};
inline constexpr std::uint32_t kNpcSheetSlot = 6;
class TableVaultImp {  // SoloParamRepository's role
 public:
  virtual std::uint32_t vault_kind() const;
  std::uint32_t table_count = 0;  // 0x08
  std::uint32_t spare = 0;
  ParamSheet** tables = nullptr;  // 0x10
};

// The text tables (our own layout): a header, its groups of consecutive ids, an offset per id (0: none), and the
// UTF-16 strings, all in one block.
struct PhraseTable {
  std::uint32_t spare;        // 0x00
  std::uint32_t group_count;  // 0x04
  std::uint64_t groups_off;   // 0x08
  std::uint64_t offsets_off;  // 0x10
  std::uint64_t spare1;       // to 0x20
};
struct PhraseGroup {          // stride 12
  std::int32_t offset_index;
  std::int32_t first_id;
  std::int32_t last_id;
};
inline constexpr std::uint32_t kPhraseTables = 24;
inline constexpr std::uint32_t kNpcNameTable = 7;
inline constexpr std::uint32_t kPlaceNameTable = 9;
class PhraseBookImp {  // MsgRepository's role
 public:
  virtual std::uint32_t phrase_kind() const;
  std::uint64_t spare = 0;    // 0x08
  void* tables = nullptr;     // 0x10: kPhraseTables pointers to PhraseTables
  std::int32_t count = 0;     // 0x18
  std::int32_t spare1 = 0;
};

}  // namespace FG

namespace Decoy {
class Actor {  // the same short name as FG::Actor in another namespace: the walk matches the whole mangled name
 public:
  virtual std::uint32_t decoy_kind() const;
  std::uint64_t id = 0;
};
}  // namespace Decoy
