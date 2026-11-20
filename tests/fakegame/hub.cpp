// The task hub (CSTask's role; ARCH §6.4): a group table, the queue that the registration front feeds, and each
// frame's run of every active task, group by group. The hub treats every task as its ABI says, as raw bytes with a
// vtable pointer first, because a task registered from outside (EldenNet's trampoline, or the self-test) is no C++
// object of ours.
#include <cstddef>
#include <cstring>
#include <new>

#include "program.hpp"

namespace fakegame {
namespace {

// The group table's names (invented), in a static table of 16-byte entries { index, flags, name }, as a game's own
// table of groups might be laid out (the derivation finds such a table at its stride, R0c). The stand-in for the
// group where positions are final is Fg_ActorSettle.
struct GroupRow {
  std::uint32_t index;
  std::uint32_t flags;
  const char16_t* name;
};
static_assert(sizeof(GroupRow) == 16);
constexpr GroupRow kGroupRows[kBaseGroups] = {
    {0, 1, u"Fg_FrameOpen"},   {1, 1, u"Fg_InputPoll"}, {2, 1, u"Fg_ScriptStep"}, {3, 1, u"Fg_ActorThink"},
    {4, 1, u"Fg_ActorMove"},   {5, 1, u"Fg_ActorSettle"}, {6, 1, u"Fg_CameraAim"}, {7, 1, u"Fg_MenuStep"},
    {8, 1, u"Fg_DrawSubmit"}, {9, 1, u"Fg_FrameClose"}};
constexpr const char16_t* kPreludeNames[] = {u"Fg_Prelude0", u"Fg_Prelude1", u"Fg_Prelude2", u"Fg_Prelude3",
                                             u"Fg_Prelude4", u"Fg_Prelude5", u"Fg_Prelude6", u"Fg_Prelude7"};

struct Slot {
  void* task;
  bool foreign;
};

struct Hub {
  SRWLOCK lock = SRWLOCK_INIT;
  std::vector<Slot> pending;
  std::vector<std::vector<Slot>> active;
  FG::GroupEntry* groups = nullptr;
  std::uint32_t count = 0;
  bool registering_builtins = false;
  HubStats stats;
};

Hub& hub() {
  static Hub h;
  return h;
}

using AttachFn = void (*)(void* self, void* hub);
using RunFn = void (*)(void* self, const FG::FrameInfo* frame);
using QueryFn = std::uint32_t (*)(void* self);

// SAFETY: `task` is a registered task, whose first field is its vtable pointer (the ABI the hub publishes); the slot
// index is below FG::kTaskSlots, and the entry is a function of that slot's signature.
template <class Fn>
Fn slot_of(void* task, std::uint32_t index) {
  void* const* vtable = nullptr;
  std::memcpy(&vtable, task, sizeof vtable);
  void* entry = vtable[index];
  Fn fn = nullptr;
  std::memcpy(&fn, &entry, sizeof fn);
  return fn;
}

// SAFETY: as slot_of: the run slot's entry, as the vtable holds it.
void* run_entry(void* task) {
  void* const* vtable = nullptr;
  std::memcpy(&vtable, task, sizeof vtable);
  return vtable[FG::kRunSlot];
}

// SAFETY: the hub's own fields of a registered task, at +0x08 onwards (FG::TaskFields), which every task leaves to it.
void write_field(void* task, std::size_t offset, const void* value, std::size_t size) {
  std::memcpy(static_cast<std::byte*>(task) + offset, value, size);
}
std::uint64_t read_runs(void* task) {
  std::uint64_t runs = 0;
  std::memcpy(&runs, static_cast<const std::byte*>(task) + 8 + offsetof(FG::TaskFields, runs), sizeof runs);
  return runs;
}

std::uint64_t now_us() {
  LARGE_INTEGER f{}, c{};
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return static_cast<std::uint64_t>(c.QuadPart) * 1000000ULL / static_cast<std::uint64_t>(f.QuadPart);
}

}  // namespace

std::uint32_t standin_group() { return kStandinBase + options().shift_groups; }
std::uint32_t group_count() { return hub().count; }
const char16_t* group_name(std::uint32_t index) {
  Hub& h = hub();
  return index < h.count ? h.groups[index].name : u"";
}

void hub_create() {
  Hub& h = hub();
  const std::uint32_t shift = options().shift_groups;
  h.count = kBaseGroups + shift;
  h.groups = static_cast<FG::GroupEntry*>(
      VirtualAlloc(nullptr, sizeof(FG::GroupEntry) * h.count, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  for (std::uint32_t i = 0; i < h.count; ++i) {
    FG::GroupEntry& e = h.groups[i];
    e.index = i;
    e.flags = 1;
    const char16_t* name = i < shift ? kPreludeNames[i] : kGroupRows[i - shift].name;
    for (std::size_t c = 0; c < 63 && name[c] != u'\0'; ++c) e.name[c] = name[c];
  }
  h.active.assign(h.count, {});

  void* mem = VirtualAlloc(nullptr, sizeof(FG::TaskLoomImp), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  auto* loom = new (mem) FG::TaskLoomImp();
  loom->f.group_count = h.count;
  loom->f.groups = h.groups;
  // The hub's live tasks as a table the hub's instance points at, as a game's hub holds its lists: R1's census of the
  // hub (part 6) finds the tasks' classes, and their base, from here.
  loom->f.live = static_cast<void**>(
      VirtualAlloc(nullptr, sizeof(void*) * FG::kMaxLiveTasks, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  // And each one's run slot, as a hub might cache the function it calls every frame: R1's whole-memory search finds a
  // copy of our slot here (spike.7).
  loom->f.run_fns = static_cast<void**>(
      VirtualAlloc(nullptr, sizeof(void*) * FG::kMaxLiveTasks, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  fg_static_taskloom = loom;
}

void hub_register_builtins(FG::TaskUnit* const tasks[4], const std::uint32_t groups[4]) {
  FgBootTask rows[4]{};
  for (int i = 0; i < 4; ++i) rows[i] = FgBootTask{tasks[i], groups[i], 0};
  hub().registering_builtins = true;
  fg_boot_register_tasks(rows);
  hub().registering_builtins = false;
}

HubStats hub_stats() {
  Hub& h = hub();
  AcquireSRWLockShared(&h.lock);
  HubStats s = h.stats;
  ReleaseSRWLockShared(&h.lock);
  return s;
}

void hub_run_frame(std::uint64_t frame, float dt) {
  Hub& h = hub();
  // Activate what was registered since the last frame: slot 0 once, on this thread.
  std::vector<Slot> fresh;
  AcquireSRWLockExclusive(&h.lock);
  fresh.swap(h.pending);
  ReleaseSRWLockExclusive(&h.lock);
  for (const Slot& s : fresh) {
    std::uint32_t group = 0;
    std::memcpy(&group, static_cast<const std::byte*>(s.task) + 8, sizeof group);
    const std::uint32_t state = 2;
    write_field(s.task, 8 + offsetof(FG::TaskFields, state), &state, sizeof state);
    slot_of<AttachFn>(s.task, FG::kAttachSlot)(s.task, fg_static_taskloom);
    h.active[group].push_back(s);
    ++h.groups[group].task_count;
    if (auto* loom = static_cast<FG::TaskLoomImp*>(fg_get_taskloom())) {
      if (loom->f.live != nullptr && loom->f.tasks_live < FG::kMaxLiveTasks) loom->f.live[loom->f.tasks_live] = s.task;
      if (loom->f.run_fns != nullptr && loom->f.tasks_live < FG::kMaxLiveTasks) {
        loom->f.run_fns[loom->f.tasks_live] = run_entry(s.task);
      }
      ++loom->f.tasks_live;
    }
    if (s.foreign) {
      // SAFETY: the group's UTF-16 name, read as Windows' 16-bit wchar_t for printing.
      status("task-active group=%u name=%ls foreign=1 frame=%llu", group,
             reinterpret_cast<const wchar_t*>(h.groups[group].name), static_cast<unsigned long long>(frame));
    }
  }
  if (auto* loom = static_cast<FG::TaskLoomImp*>(fg_get_taskloom())) loom->f.frame = frame;

  for (std::uint32_t g = 0; g < h.count; ++g) {
    for (const Slot& s : h.active[g]) {
      const FG::FrameInfo info{frame, dt, g};
      const std::uint64_t t0 = s.foreign ? now_us() : 0;
      slot_of<RunFn>(s.task, FG::kRunSlot)(s.task, &info);
      const std::uint64_t runs = read_runs(s.task) + 1;
      write_field(s.task, 8 + offsetof(FG::TaskFields, runs), &runs, sizeof runs);
      if (frame % FG::kQueryEvery == 0) (void)slot_of<QueryFn>(s.task, FG::kQuerySlot)(s.task);
      AcquireSRWLockExclusive(&h.lock);
      if (s.foreign) {
        ++h.stats.foreign_runs;
        const std::uint64_t us = now_us() - t0;
        if (us > h.stats.foreign_us_max) h.stats.foreign_us_max = us;
      } else {
        ++h.stats.builtin_runs;
      }
      ReleaseSRWLockExclusive(&h.lock);
    }
  }
}

}  // namespace fakegame

using fakegame::hub;
using fakegame::options;
using fakegame::RegisterMode;
using fakegame::status;

namespace {
// SAFETY: a read of an address in the first page, which is never mapped: the fault a game given bad input would meet.
__declspec(noinline) void fault_in_game() {
  volatile const std::uint32_t* bad = reinterpret_cast<volatile const std::uint32_t*>(std::uintptr_t{0x10});
  (void)*bad;
}
}  // namespace

extern "C" void fg_hub_enqueue(FG::TaskLoomImp* loom, std::uint32_t group, FG::TaskUnit* task) {
  auto& h = hub();
  const bool builtin = h.registering_builtins;
  if (!builtin && options().register_mode == RegisterMode::Fault) {
    status("register fault-on-request group=%u", group);
    fakegame::status_flush();
    fault_in_game();
  }
  if (!builtin && options().register_mode == RegisterMode::Ignore) {
    AcquireSRWLockExclusive(&h.lock);
    ++h.stats.ignored;
    ReleaseSRWLockExclusive(&h.lock);
    status("register ignored group=%u", group);
    return;
  }
  // The hub's fields of the task: its group, its state and the hub (FG::TaskFields).
  const std::uint32_t state = 1;
  void* hub_ptr = loom;
  const std::uint64_t runs = 0;
  fakegame::write_field(task, 8 + offsetof(FG::TaskFields, group), &group, sizeof group);
  fakegame::write_field(task, 8 + offsetof(FG::TaskFields, state), &state, sizeof state);
  fakegame::write_field(task, 8 + offsetof(FG::TaskFields, hub), &hub_ptr, sizeof hub_ptr);
  fakegame::write_field(task, 8 + offsetof(FG::TaskFields, runs), &runs, sizeof runs);
  AcquireSRWLockExclusive(&h.lock);
  h.pending.push_back(fakegame::Slot{task, !builtin});
  if (!builtin) ++h.stats.foreign_tasks;
  ReleaseSRWLockExclusive(&h.lock);
  if (!builtin) {
    status("register accepted group=%u thread=%s", group,
           GetCurrentThreadId() == fakegame::game_thread_id() ? "game" : "other");
  }
}

// The wrapper's other way (fg_register_task's tail jump while the hub is closed): the task is counted as ignored and
// never queued, so the call answers as if it had registered.
extern "C" void fg_register_closed(FG::TaskUnit* task, FG::TaskLoomImp* hub, std::uint32_t group) {
  (void)task;
  (void)hub;
  auto& h = fakegame::hub();
  AcquireSRWLockExclusive(&h.lock);
  ++h.stats.ignored;
  ReleaseSRWLockExclusive(&h.lock);
  status("register closed group=%u", group);
}

extern "C" void fg_register_rejected(std::uint32_t reason) {
  auto& h = hub();
  AcquireSRWLockExclusive(&h.lock);
  ++h.stats.rejected;
  ReleaseSRWLockExclusive(&h.lock);
  static const char* const kReasons[] = {"?", "hub", "group", "task"};
  status("register rejected reason=%s", kReasons[reason < 4 ? reason : 0]);
  fakegame::status_flush();
  fault_in_game();
}
