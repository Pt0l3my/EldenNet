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
#define FG_LOOM_FLAGS_OFF 0x14        // FG::TaskLoomImp: its flags (none used)
#define FG_LOOM_LAST_TASK_OFF 0x30    // FG::TaskLoomImp: the last task registered, which the registration keeps there
#define FG_LOOM_LAST_VTABLE_OFF 0x38  // FG::TaskLoomImp: that task's vtable, which the registration keeps too
#define FG_GROUP_BOUND 0x200          // the registration refuses a group index at or past it, before the count
#define FG_RACK_SHELVES_OFF 0x10      // FG::TaskRackImp: its table of shelves, one per group (spike.8's shape, WHY§240)
#define FG_RACK_BOUND 31              // the tail function answers 0 for a group past it (entries 0 to 31)
// A shelf (the group's entry; since spike.10 R1 run 9's shape, WHY§289): its word, and the object it holds where run 9's
// entries held one.
#define FG_SHELF_WORD_OFF 0x54        // FG::Rack::TaskLoomImp: its word, the group's own index, passed on in r8
#define FG_SHELF_SHARED_OFF 0x58      // the rack
// The ledger (the second singleton G; since spike.11 R1 run 10's shape, WHY§293): its fields, at offsets of our own.
#define FG_LEDGER_GROUPS_OFF 0x08     // FG::TaskLedgerImp: the hub's groups
#define FG_LEDGER_QUEUE_OFF 0x10      // its queue (FG::TaskQueueImp), where the game's own requests wait
#define FG_LEDGER_OPEN_OFF 0x30       // non-zero once it takes requests: zero, its method answers 0
#define FG_LEDGER_MANAGER_OFF 0x38    // the manager (FG::TaskManagerImp) of the game's own tasks
// The request its method builds (FG::TaskRequest, R1 run 10's shape): 32 bytes at [rsp + FG_REQUEST_DISP] of the
// method's frame.
#define FG_REQUEST_DISP 0x20
#define FG_REQUEST_TASK_OFF 0x08      // the task
#define FG_REQUEST_MANAGER_OFF 0x10   // the ledger's manager
#define FG_REQUEST_WORD_OFF 0x18      // the shelf's word
#define FG_REQUEST_FLAG_OFF 0x1C      // r9's low byte (zero); the game's own requests that wait for ever hold 1
#define FG_REQUEST_BYTES 0x20
// Since spike.12 (R1 run 11's shape, WHY§296) the manager's map, keyed by the shelves' words, at offsets of our own:
// its head (an MSVC-shaped tree's: the node's left at +0, parent +8, right +16, its nil byte at +25, a 4-byte key at
// +32, the value at +40), its count beside it; and its list (begin, end, capacity), the bins a word of 0 goes to.
#define FG_MANAGER_MAP_OFF 0x30
#define FG_MANAGER_LIST_OFF 0x40
#define FG_NODE_NIL_OFF 25
#define FG_NODE_KEY_OFF 32
#define FG_NODE_VALUE_OFF 40
#define FG_BIN_GROUP_OFF 0x08         // FG::TaskBin: its group, which a match hands the task to
#define FG_BIN_KEPT_OFF 0x0C          // and the tasks handed to it
#define FG_TASK_HUB_OFF 0x10          // a task's hub field (FG::TaskFields::hub, at +0x08 + 0x08)
#define FG_TASK_STATE_OFF 0x0C        // a task's state (FG::TaskFields::state): 0 before the hub takes it
#define FG_TASK_KIND_OFF 0x20         // a task's kind (FG::TaskFields::kind): the game's own tasks hold FG_TASK_KIND
#define FG_TASK_KIND 0x51             // the kind of the game's own tasks
// The hub's desk (spike.10, R1 run 9's walks): at +0x3B0 of the hub, at +0x3D0 of the desk its index, whose four
// pointers 32 bytes apart from +200 lead to rows of the ledger's notes 240 bytes apart (four views of one array).
#define FG_LOOM_DESK_OFF 0x3B0
#define FG_DESK_INDEX_OFF 0x3D0
#define FG_INDEX_VIEWS_OFF 200
#define FG_ROW_BYTES 240
#define FG_NOTE_STAMP_MS 120          // the game thread stamps a note with its thread id this long after it is made
// The program's own call sites: the second's task a member of a bench at this offset (spike.10's call-site forms).
#define FG_BENCH_TASK_OFF 0x10
#define FG_WARDEN_SAVE_SLOT_OFF 0x14  // FG::RunWardenImp: the save slot
#define FG_PLOT_PLACE_OFF 0x1C        // FG::FieldPlotImp: the place id
#define FG_RTCLASS_NAME_OFF 0x18      // a runtime-class object: its name pointer (DLRF's role; ARCH §6.2 step 3)

namespace FG {
class TaskUnit;
class TaskLoomImp;
class TaskRequest;
class TaskQueueImp;
class TaskManagerImp;
class TaskBin;
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
extern void* fg_field_plot_mirror;    // with --field-mirror: the same address again, in a second word of the data
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

// The task registration: our own invented ABI, (task in rcx, the hub instance in rdx, the group index in r8d), answering
// 1 when the task was kept and 0 when not. A wrapper of R1 run 7's shape (WHY§240): it loads its own singleton (the
// rack) from fg_static_rack and, while that is null, makes it with two calls and stores it there; then it tail-jumps to
// fg_register_rack with the rack in rcx, the group in edx, the task in r8 and the hub it was given in r9.
std::uint64_t fg_register_task(FG::TaskUnit* task, FG::TaskLoomImp* hub, std::uint32_t group);
// The tail function, a leaf of no unwind data: past FG_RACK_BOUND, or where the rack's shelf for the group is null, it
// answers 0; else (R1 run 8's shape, WHY§248) it moves the task into rdx and jumps directly to fg_shelf_take, the
// shelf in rcx and the hub still in r9.
std::uint64_t fg_register_rack(void* rack, std::uint32_t group, FG::TaskUnit* task, FG::TaskLoomImp* hub);
// The group's own function (since spike.10 R1 run 9's shape, WHY§289): null for a null task; else it loads the ledger
// from fg_static_ledger (while that is null, fg_ledger_missing makes it), puts the shelf's word in r8 and tail-jumps
// through fg_ledger_stub (which zeroes r9) to the ledger's method, the ledger in rcx and the task in rdx.
std::uint64_t fg_shelf_take(void* shelf, FG::TaskUnit* task, FG::TaskUnit* same_task, FG::TaskLoomImp* hub);
extern void* fg_static_rack;                  // FG::TaskRackImp*: the registration's own singleton, made lazily
extern void* fg_static_ledger;                // FG::TaskLedgerImp*: the second singleton G, made with the hub
extern std::uint64_t fg_ledger_blank;         // a static word the hub's rows point at before their first note
extern void* fg_ledger_given;                 // the hub the last registration was given, for the work
std::uint64_t fg_ledger_stub(void* ledger, FG::TaskUnit* task, std::uint32_t word, std::uint64_t zero);
// The ledger's method (G's; since spike.11 R1 run 10's shape, WHY§293): a null task, or a ledger not open, answers 0.
// Else it builds a request on its stack (FG::TaskRequest: its vtable, the task, the ledger's manager, the word, r9's
// low byte), calls fg_request_now with the request's address and answers what it answers. It never reads the task.
std::uint64_t fg_ledger_admit(void* ledger, FG::TaskUnit* task, std::uint32_t word, std::uint64_t zero);
// The request's work, now (the request's run slot too): it notes the task in the hub's rows (fg_ledger_note), hands
// the task to the work (fg_ledger_keep) and answers 1.
std::uint64_t fg_request_now(FG::TaskRequest* request);
std::uint32_t fg_request_cancel(FG::TaskRequest* request);  // the request's other slots: its task's state reset,
std::uint32_t fg_request_word(FG::TaskRequest* request);    // and its word
// Since spike.12 (R1 run 11's shape, WHY§296) the hand-over is the manager's: with a word of 0 the now path's function
// calls fg_map_take for each bin of the manager's list; with any other it walks the manager's map for the word and, on
// a match, calls fg_map_take with the node's bin, the task, the word and the manager, which hands the task to the work
// with the bin's group; no match drops the task (its word kept in fg_missed_word). Then, where the request's flag is
// set (the game's own requests' that wait for ever), it calls fg_task_other with the task and the task's slot 1.
std::uint64_t fg_map_take(FG::TaskBin* bin, FG::TaskUnit* task, std::uint32_t word, FG::TaskManagerImp* manager);
std::uint32_t fg_task_other(FG::TaskUnit* task);
extern std::uint8_t fg_missed;
extern std::uint32_t fg_missed_word;
// The work: it checks its arguments, keeps the task and its vtable in the hub, and queues the task.
void fg_register_keep(FG::TaskLoomImp* hub, std::uint32_t group, FG::TaskUnit* task);
// The program's own call sites into it: four tasks, each {task, group}, registered in order, each passed as a call
// site of a game might pass it (spike.10): a load from the rows, the bench's member (FG_BENCH_TASK_OFF), an allocator's
// result (fg_task_make), a load.
struct FgBootTask {
  FG::TaskUnit* task;
  std::uint32_t group;
  std::uint32_t spare;
};
struct FgTaskBench {
  std::uint64_t id;
  std::uint64_t spare;
  alignas(8) unsigned char task[0x30];  // FG_BENCH_TASK_OFF: a task object made in place
};
void fg_boot_register_tasks(const FgBootTask* tasks, FgTaskBench* bench);  // exactly four entries
FG::TaskUnit* fg_task_make(std::uint32_t which);  // the boot task `which`, as an allocator would answer it
// The registration's sibling of the same argument shape (spike.10): it hands the task to the ledger to forget; its two
// call sites (fg_boot_forget_tasks) are never run, only there to be found.
std::uint64_t fg_unregister_task(FG::TaskUnit* task, FG::TaskLoomImp* hub, std::uint32_t group);
void fg_boot_forget_tasks(const FgBootTask* tasks);

// Called from the assembly (defined in C++).
void fg_fd4_unset(const char* path, const char* name, std::uint32_t site);  // a singleton used before it was set
void fg_hub_enqueue(FG::TaskLoomImp* hub, std::uint32_t group, FG::TaskUnit* task);
void* fg_rack_alloc();                               // the rack's memory, zeroed
void* fg_rack_build(void* memory, FG::TaskLoomImp* hub);  // a shelf for each group of the hub's (but the gap's)
// The ledger's side (spike.10): made while its static is null; a note of each task a request carries in the hub's rows
// (an object of FG::TaskNote, a fresh record, stamped with the game thread's id FG_NOTE_STAMP_MS later); the work, with
// the hub; and the sibling's forgetting, which keeps nothing.
void fg_ledger_missing();
void fg_ledger_note(FG::TaskUnit* task, std::uint32_t word);
void fg_ledger_keep(void* ledger, std::uint32_t group, FG::TaskUnit* task);
std::uint64_t fg_ledger_forget(void* ledger, FG::TaskUnit* task, std::uint32_t group, FG::TaskLoomImp* hub);
void fg_register_rejected(std::uint32_t reason);  // 1 hub, 2 group, 3 task; it faults, as a game given bad input would
}

namespace FG {

// --- The task interface (the game's own task base; ARCH §6.4's "task object shape") -----------------------------------
// A task is an object whose first field is a vtable pointer. The hub calls slot 0 once, on the game thread, when it
// activates the task; slot 1 every frame of the task's group; slot 2 every 64th frame. It writes the fields below
// into every task it holds (but the kind, which the game's own tasks hold from when they are made), so a task object
// is at least 0x28 bytes: the vtable pointer, then 0x20 bytes the hub owns.
struct FrameInfo {
  std::uint64_t frame;  // 0x00: the frame's number
  float dt;             // 0x08: seconds since the previous frame
  std::uint32_t group;  // 0x0C: the group being run
};
struct TaskFields {
  std::uint32_t group;   // +0x08: written at registration
  std::uint32_t state;   // +0x0C: 1 queued, 2 active
  void* hub;             // +0x10: written at registration (FG_TASK_HUB_OFF)
  std::uint64_t runs;    // +0x18: incremented after every run
  std::uint32_t kind;    // +0x20: FG_TASK_KIND in the game's own tasks; a shelf compares it (FG_TASK_KIND_OFF)
  std::uint32_t spare;   // +0x24
};
inline constexpr std::uint32_t kTaskSlots = 3;
inline constexpr std::uint32_t kAttachSlot = 0;
inline constexpr std::uint32_t kRunSlot = 1;
inline constexpr std::uint32_t kQuerySlot = 2;
inline constexpr std::uint32_t kQueryEvery = 64;
inline constexpr std::size_t kTaskFieldsSize = sizeof(TaskFields);  // the bytes after the vtable pointer: 0x20

class TaskUnit {
 public:
  TaskUnit() = default;
  TaskUnit(const TaskUnit&) = delete;
  TaskUnit& operator=(const TaskUnit&) = delete;
  virtual void attach(void* hub);                // slot 0
  virtual void run(const FrameInfo* frame) = 0;  // slot 1
  virtual std::uint32_t query();                 // slot 2
  TaskFields t{0, 0, nullptr, 0, FG_TASK_KIND, 0};  // the game's own: of the game's kind from the start

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
  std::uint32_t flags;        // 0x14 (FG_LOOM_FLAGS_OFF): none set
  GroupEntry* groups;         // 0x18
  std::uint64_t tasks_live;   // 0x20
  void** live;                // 0x28: every active task, in the order the hub activated it (kMaxLiveTasks)
  void* last_task;            // 0x30 (FG_LOOM_LAST_TASK_OFF): written by the registration itself (R1's callee summary)
  void* last_vtable;          // 0x38 (FG_LOOM_LAST_VTABLE_OFF): that task's vtable, as a hub might cache it
  void** run_fns;             // 0x40: each active task's run slot, in the order of `live`, as a hub might cache them
  std::uint64_t spare;        // 0x48
  std::uint8_t spare_desk[FG_LOOM_DESK_OFF - 0x50];  // 0x50
  void* desk;                 // 0x3B0 (FG_LOOM_DESK_OFF): the desk, its index and the rows (spike.10)
};
inline constexpr std::uint32_t kMaxLiveTasks = 256;

// The registration's own singleton (spike.8's shape, WHY§240): a shelf per group. Made by the registration's first
// call. Since spike.9 (WHY§248) each shelf is an object of a class of the rack's own; its class's innermost name is the
// hub's (FG::Rack::TaskLoomImp), so R1, naming classes as O92 does, matches it to the hub's lead; the rack's matches
// none. Since spike.10 (WHY§289, R1 run 9's shape) a shelf holds a word, its group's own index, which the group's
// function passes to the ledger's method, and the rack where run 9's entries held one object.
inline constexpr std::uint32_t kRackGroups = FG_RACK_BOUND + 1;
struct ShelfFields {                      // from +0x08
  std::uint32_t group;                    // 0x08
  std::uint32_t spare0;                   // 0x0C
  std::uint64_t spare[8];                 // 0x10
  std::uint32_t spare1;                   // 0x50
  std::uint32_t word;                     // 0x54 (FG_SHELF_WORD_OFF)
  void* shared;                           // 0x58 (FG_SHELF_SHARED_OFF)
};
namespace Rack {
class TaskLoomImp {
 public:
  virtual std::uint32_t shelf_kind() const;
  ShelfFields s{};
};
}  // namespace Rack
using TaskShelf = Rack::TaskLoomImp;
struct RackFields {                       // from +0x08
  std::uint64_t shelves_made;             // 0x08
  TaskShelf* shelves[kRackGroups];        // 0x10 (FG_RACK_SHELVES_OFF)
};
class TaskRackImp {
 public:
  virtual std::uint32_t rack_kind() const;
  RackFields r{};
};

// The game's own tasks the ledger's manager holds (spike.11, WHY§293): a base of their own and two derived classes, 128
// bytes each, of no lead's names.
struct ChoreFields {                      // from +0x08
  std::uint32_t id;                       // 0x08
  std::uint32_t kind;                     // 0x0C
  std::uint64_t runs;                     // 0x10
  void* manager;                          // 0x18
  std::uint64_t spare[12];                // 0x20
};
class Chore {
 public:
  Chore() = default;
  Chore(const Chore&) = delete;
  Chore& operator=(const Chore&) = delete;
  virtual ~Chore() = default;
  virtual std::uint32_t chore_kind() const = 0;
  virtual void step();
  ChoreFields c{};
};
class SweepChore final : public Chore {
 public:
  std::uint32_t chore_kind() const override;
};
class TallyChore final : public Chore {
 public:
  std::uint32_t chore_kind() const override;
  void step() override;
};
class TaskManagerImp;
// Spike.12 (WHY§296): a group's bin, the map's value; the bins of the groups the world adds are of a class of their
// own.
struct BinFields {                        // from +0x08
  std::uint32_t group;                    // 0x08 (FG_BIN_GROUP_OFF)
  std::uint32_t kept;                     // 0x0C (FG_BIN_KEPT_OFF)
};
class TaskBin {
 public:
  TaskBin() = default;
  TaskBin(const TaskBin&) = delete;
  TaskBin& operator=(const TaskBin&) = delete;
  virtual ~TaskBin() = default;
  virtual std::uint32_t bin_kind() const;
  BinFields b{};
};
class WorldBin final : public TaskBin {
 public:
  std::uint32_t bin_kind() const override;
};
// A node of the manager's map, MSVC's tree's shape (the head's nil byte 1, every node's 0).
struct MapNode {
  MapNode* left;                          // 0x00
  MapNode* parent;                        // 0x08 (the head's: the root)
  MapNode* right;                         // 0x10
  std::uint8_t colour;                    // 0x18
  std::uint8_t nil;                       // 0x19 (FG_NODE_NIL_OFF)
  std::uint8_t spare[6];
  std::uint32_t key;                      // 0x20 (FG_NODE_KEY_OFF): a shelf's word
  std::uint32_t spare1;
  TaskBin* value;                         // 0x28 (FG_NODE_VALUE_OFF)
};
struct ManagerFields {                    // from +0x08
  std::uint32_t spare0;                   // 0x08
  std::uint32_t count;                    // 0x0C
  Chore* chores[4];                       // 0x10
  MapNode* map_head;                      // 0x30 (FG_MANAGER_MAP_OFF)
  std::uint64_t map_count;                // 0x38
  TaskBin** list_begin;                   // 0x40 (FG_MANAGER_LIST_OFF)
  TaskBin** list_end;                     // 0x48
  TaskBin** list_cap;                     // 0x50
};
class TaskManagerImp {
 public:
  virtual std::uint32_t manager_kind() const;
  ManagerFields m{};
};
// The request the ledger's method builds (spike.11, R1 run 10's shape): a base of the fake game's own for its orders,
// and the request, whose slots are its run (the now path's function), its cancel and its word. Its layout is the one
// the method's assembly writes.
class WorkOrder {
 public:
  virtual std::uint32_t run() = 0;
  virtual std::uint32_t cancel() = 0;
};
struct RequestFields {                     // from +0x08
  TaskUnit* task;                         // 0x08 (FG_REQUEST_TASK_OFF)
  TaskManagerImp* manager;                // 0x10 (FG_REQUEST_MANAGER_OFF)
  std::uint32_t word;                     // 0x18 (FG_REQUEST_WORD_OFF)
  std::uint8_t flag;                      // 0x1C (FG_REQUEST_FLAG_OFF)
  std::uint8_t spare[3];
};
class TaskRequest final : public WorkOrder {
 public:
  std::uint32_t run() override;     // a jump to fg_request_now
  std::uint32_t cancel() override;  // a jump to fg_request_cancel
  virtual std::uint32_t word_of();  // a jump to fg_request_word
  RequestFields r{};
};
// The ledger's queue: a lock, and its list of requests waiting.
struct RequestList {
  TaskRequest** items;
  std::uint32_t count;
  std::uint32_t cap;
};
class TaskQueueImp {
 public:
  virtual std::uint32_t queue_kind() const;
  SRWLOCK lock = SRWLOCK_INIT;            // +0x08
  RequestList* list = nullptr;            // +0x10
};

// The second singleton G (spike.10, WHY§289; since spike.11 R1 run 10's shape, WHY§293): the ledger the group's
// function hands each task to. Its class names no lead.
struct LedgerFields {                     // from +0x08
  std::uint32_t groups;                   // 0x08 (FG_LEDGER_GROUPS_OFF)
  std::uint32_t spare0;                   // 0x0C
  TaskQueueImp* queue;                    // 0x10 (FG_LEDGER_QUEUE_OFF)
  std::uint64_t spare1[3];                // 0x18
  std::uint64_t open;                     // 0x30 (FG_LEDGER_OPEN_OFF)
  TaskManagerImp* manager;                // 0x38 (FG_LEDGER_MANAGER_OFF)
};
class TaskLedgerImp {
 public:
  virtual std::uint32_t ledger_kind() const;
  LedgerFields l{};
};
// A note in the hub's rows (spike.10): its vtable in the game's rdata, a fresh record, the game thread's stamp.
class TaskNote {
 public:
  virtual std::uint32_t note_kind() const;
  void* record = nullptr;                 // +0x08: a fresh heap block (before the first note: fg_ledger_blank)
  std::uint32_t stamp = 0;                // +0x10: the game thread's id, FG_NOTE_STAMP_MS after the note
  std::uint32_t spare = 0;
};

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
  float hours = 0.0f;       // 0x18: the hour of the day, as a fraction
  float spare1 = 0.0f;
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
// bytes (1,000 flags, the first flag of a byte in its top bit) by its index in the holder (mode 1: the block lies
// index times 125 bytes in, as the game's does) or by address (mode 2).
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
