// The task hub (CSTask's role; ARCH §6.4): a group table, the queue that the registration front feeds, and each
// frame's run of every active task, group by group. The hub treats every task as its ABI says, as raw bytes with a
// vtable pointer first, because a task registered from outside (EldenNet's trampoline, or the self-test) is no C++
// object of ours.
#include <atomic>
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

// The ledger's side (spike.10, R1 run 9's shape, WHY§289): the hub's rows of notes, the note to stamp, the boot tasks
// fg_task_make answers.
constexpr std::uint32_t kRows = 0x1000 / FG_ROW_BYTES;
struct Ledger {
  std::byte* rows = nullptr;
  std::uint32_t notes = 0;
  std::atomic<std::int64_t> pending{-1};  // the row to stamp, or -1
  std::atomic<std::uint64_t> noted_us{0};
  FG::TaskUnit* boot[4] = {};
};
Ledger& ledger() {
  static Ledger l;
  return l;
}

void* alloc_page() { return VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); }

std::uint64_t now_us() {
  LARGE_INTEGER f{}, c{};
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return static_cast<std::uint64_t>(c.QuadPart) * 1000000ULL / static_cast<std::uint64_t>(f.QuadPart);
}


// The ledger's queue (spike.11, R1 run 10's shape): its list's capacity, and the game's own requests that wait in it
// for ever (each of a task of the manager's, its word and its flag 1).
constexpr std::uint32_t kQueueCap = 64;
constexpr std::uint32_t kLingering = 2;
constexpr std::uint32_t kLingeringWords[kLingering] = {3, 7};

// The ledger's objects, in pages of their own (an arena never freed), so that the walks R1 makes from G and its queue
// read nothing that changes on its own.
void* arena(std::size_t bytes) {
  static std::byte* at = nullptr;
  static std::size_t left = 0;
  bytes = (bytes + 15) & ~std::size_t{15};
  if (bytes > left) {
    at = static_cast<std::byte*>(VirtualAlloc(nullptr, 0x10000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    left = at != nullptr ? 0x10000 : 0;
  }
  if (bytes > left) return nullptr;
  void* p = at;
  at += bytes;
  left -= bytes;
  return p;
}
template <class T>
T* in_arena() {
  return new (arena(sizeof(T))) T();
}

// Spike.12 (R1 run 11's shape, WHY§296): the manager's map, a tree of our own making with MSVC's node shape (no
// balancing: a few nodes), keyed by the shelves' words, each value a bin of its group's. Its words: every group's but
// 0's (the list's: a word of 0 takes the list) and the world's own (1, 3, 6 and 8, past the shift), which its first
// load adds.
bool world_word(std::uint32_t word) {
  const std::uint32_t s = options().shift_groups;
  return word == 1 + s || word == 3 + s || word == 6 + s || word == 8 + s;
}

void map_insert(FG::TaskManagerImp& m, std::uint32_t key, FG::TaskBin* value) {
  FG::MapNode* head = m.m.map_head;
  FG::MapNode* parent = head;
  FG::MapNode* at = head->parent;
  bool left = false;
  while (at != head) {
    if (at->key == key) return;
    parent = at;
    left = key < at->key;
    at = left ? at->left : at->right;
  }
  auto* n = in_arena<FG::MapNode>();
  n->left = head;
  n->right = head;
  n->parent = parent;
  n->key = key;
  n->value = value;
  if (parent == head) {  // the node is whole before a reader can reach it
    head->parent = n;
  } else if (left) {
    parent->left = n;
  } else {
    parent->right = n;
  }
  if (head->left == head || key < head->left->key) head->left = n;
  if (head->right == head || key > head->right->key) head->right = n;
  ++m.m.map_count;
}

template <class Bin>
FG::TaskBin* make_bin(std::uint32_t group) {
  auto* bin = in_arena<Bin>();
  bin->b.group = group;
  return bin;
}

void make_map(FG::TaskManagerImp& m) {
  auto* head = in_arena<FG::MapNode>();
  head->left = head->parent = head->right = head;
  head->nil = 1;
  head->colour = 1;
  m.m.map_head = head;
  for (const std::uint32_t w : map_title_words()) map_insert(m, w, make_bin<FG::TaskBin>(w));
  auto** list = static_cast<FG::TaskBin**>(arena(4 * sizeof(FG::TaskBin*)));
  list[0] = make_bin<FG::TaskBin>(0);
  m.m.list_begin = list;
  m.m.list_end = list + 1;
  m.m.list_cap = list + 4;
}

// The ledger, its manager and its three tasks of the game's own, its queue, and the two requests of the game's own that
// wait there (spike.11); since spike.12 the manager's map and list.
void make_ledger() {
  if (fg_static_ledger != nullptr) return;
  auto* g = new (VirtualAlloc(nullptr, sizeof(FG::TaskLedgerImp), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE))
      FG::TaskLedgerImp();
  g->l.groups = hub().count;
  g->l.open = 1;
  auto* manager = in_arena<FG::TaskManagerImp>();
  FG::Chore* const chores[] = {in_arena<FG::SweepChore>(), in_arena<FG::TallyChore>(), in_arena<FG::SweepChore>()};
  for (FG::Chore* c : chores) {
    c->c.id = manager->m.count;
    c->c.kind = FG_TASK_KIND;
    c->c.manager = manager;
    manager->m.chores[manager->m.count++] = c;
  }
  make_map(*manager);
  g->l.manager = manager;
  auto* queue = in_arena<FG::TaskQueueImp>();
  auto* list = in_arena<FG::RequestList>();
  list->items = static_cast<FG::TaskRequest**>(alloc_page());
  list->cap = kQueueCap;
  queue->list = list;
  for (std::uint32_t i = 0; i < kLingering; ++i) {
    auto* r = in_arena<FG::TaskRequest>();
    r->r.task = reinterpret_cast<FG::TaskUnit*>(manager->m.chores[i]);  // SAFETY: a task of the game's, by address
    r->r.manager = manager;
    r->r.word = kLingeringWords[i];
    r->r.flag = 1;
    list->items[list->count++] = r;
  }
  g->l.queue = queue;
  fg_static_ledger = g;
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

}  // namespace

std::uint32_t standin_group() { return kStandinBase + options().shift_groups; }

std::vector<std::uint32_t> map_title_words() {
  std::vector<std::uint32_t> out;
  for (std::uint32_t w = 1; w < kBaseGroups + options().shift_groups; ++w) {
    if (!world_word(w)) out.push_back(w);
  }
  return out;
}
std::vector<std::uint32_t> map_world_words() {
  std::vector<std::uint32_t> out;
  for (std::uint32_t w = 1; w < kBaseGroups + options().shift_groups; ++w) {
    if (world_word(w)) out.push_back(w);
  }
  return out;
}

// The world's first load adds its words to the manager's map, each with a bin of the world's class.
void hub_world_loaded() {
  static bool done = false;
  const auto* g = static_cast<const FG::TaskLedgerImp*>(fg_static_ledger);
  if (done || g == nullptr || g->l.manager == nullptr) return;
  done = true;
  for (const std::uint32_t w : map_world_words()) map_insert(*g->l.manager, w, make_bin<FG::WorldBin>(w));
  status("map world_words=%zu count=%llu", map_world_words().size(),
         static_cast<unsigned long long>(g->l.manager->m.map_count));
}
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
  // Spike.10 (R1 run 9's walks, WHY§289): the desk at +0x3B0, its index at +0x3D0, and the index's four pointers 32
  // bytes apart from +200 into the rows, 240 bytes apart: four overlapping views of one array. Each row points at a
  // static word until the ledger's first note there.
  auto* desk = static_cast<std::byte*>(alloc_page());
  auto* index = static_cast<std::byte*>(alloc_page());
  Ledger& l = ledger();
  l.rows = static_cast<std::byte*>(alloc_page());
  std::memcpy(desk + FG_DESK_INDEX_OFF, &index, sizeof index);
  for (std::uint32_t k = 0; k < 4; ++k) {
    std::byte* view = l.rows + FG_ROW_BYTES * k;
    std::memcpy(index + FG_INDEX_VIEWS_OFF + 32 * k, &view, sizeof view);
  }
  for (std::uint32_t r = 0; r < kRows; ++r) {
    void* blank = &fg_ledger_blank;
    std::memcpy(l.rows + FG_ROW_BYTES * r + 8, &blank, sizeof blank);
  }
  loom->f.desk = desk;
  fg_static_taskloom = loom;
  make_ledger();
}

void hub_register_builtins(FG::TaskUnit* const tasks[4], const std::uint32_t groups[4], FgTaskBench* bench) {
  FgBootTask rows[4]{};
  for (int i = 0; i < 4; ++i) {
    rows[i] = FgBootTask{tasks[i], groups[i], 0};
    ledger().boot[i] = tasks[i];
  }
  hub().registering_builtins = true;
  fg_boot_register_tasks(rows, bench);
  hub().registering_builtins = false;
}

HubStats hub_stats() {
  Hub& h = hub();
  AcquireSRWLockShared(&h.lock);
  HubStats s = h.stats;
  ReleaseSRWLockShared(&h.lock);
  if (const auto* loom = static_cast<const FG::TaskLoomImp*>(fg_get_taskloom())) s.live = loom->f.tasks_live;
  return s;
}

void hub_run_frame(std::uint64_t frame, float dt) {
  Hub& h = hub();
  if (fg_missed != 0) {  // spike.12: a word the manager's map does not hold, its task dropped
    fg_missed = 0;
    status("map missed=%u", fg_missed_word);
  }
  // Spike.10: the ledger's last note, stamped with this thread's id FG_NOTE_STAMP_MS after it was made.
  Ledger& l = ledger();
  if (const std::int64_t row = l.pending.load(std::memory_order_acquire);
      row >= 0 && now_us() - l.noted_us.load(std::memory_order_relaxed) >= FG_NOTE_STAMP_MS * 1000ull) {
    auto* note = reinterpret_cast<FG::TaskNote*>(l.rows + FG_ROW_BYTES * static_cast<std::uint32_t>(row));
    note->stamp = static_cast<std::uint32_t>(GetCurrentThreadId());
    l.pending.store(-1, std::memory_order_release);
  }
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
using fakegame::ledger;
using fakegame::options;
using fakegame::RegisterMode;
using fakegame::status;

namespace {
// SAFETY: a read of an address in the first page, which is never mapped: the fault a game given bad input would meet.
// The address passes through a volatile object, unknown to the optimiser (no -Warray-bounds at -O2), read as written.
__declspec(noinline) void fault_in_game() {
  const volatile std::uintptr_t at = 0x10;
  volatile const std::uint32_t* bad = reinterpret_cast<volatile const std::uint32_t*>(at);
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

std::uint32_t FG::Rack::TaskLoomImp::shelf_kind() const { return 0x501; }
std::uint32_t FG::TaskRackImp::rack_kind() const { return 0x502; }
std::uint32_t FG::TaskLedgerImp::ledger_kind() const { return 0x503; }
std::uint32_t FG::TaskNote::note_kind() const { return 0x504; }
// Spike.11: the manager, its tasks, the queue, and the request's slots (each a jump to its work in asm.cpp, so that its
// code is the same in every build).
std::uint32_t FG::TaskManagerImp::manager_kind() const { return 0x505; }
std::uint32_t FG::TaskQueueImp::queue_kind() const { return 0x506; }
std::uint32_t FG::TaskBin::bin_kind() const { return 0x509; }
std::uint32_t FG::WorldBin::bin_kind() const { return 0x50A; }
void FG::Chore::step() { ++c.runs; }
std::uint32_t FG::SweepChore::chore_kind() const { return 0x507; }
std::uint32_t FG::TallyChore::chore_kind() const { return 0x508; }
void FG::TallyChore::step() { c.runs += 2; }
__attribute__((naked)) std::uint32_t FG::TaskRequest::run() {
  __asm__(".intel_syntax noprefix\n  .byte 0xE9\n  .long fg_request_now - 1f\n1:\n  .att_syntax prefix\n");
}
__attribute__((naked)) std::uint32_t FG::TaskRequest::cancel() {
  __asm__(".intel_syntax noprefix\n  .byte 0xE9\n  .long fg_request_cancel - 1f\n1:\n  .att_syntax prefix\n");
}
__attribute__((naked)) std::uint32_t FG::TaskRequest::word_of() {
  __asm__(".intel_syntax noprefix\n  .byte 0xE9\n  .long fg_request_word - 1f\n1:\n  .att_syntax prefix\n");
}

// The registration's rack (spike.8's shape, WHY§240), made by its first call: the memory, then a shelf for each of the
// hub's groups.
extern "C" void* fg_rack_alloc() {
  return VirtualAlloc(nullptr, sizeof(FG::TaskRackImp), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

extern "C" void* fg_rack_build(void* memory, FG::TaskLoomImp* hub) {
  auto* rack = new (memory) FG::TaskRackImp();
  const std::uint32_t groups = hub != nullptr ? hub->f.group_count : 0;
  static_assert(fakegame::kBaseGroups + 8 <= FG::kRackGroups, "every group of the hub has a shelf's place");
  auto* shelves = static_cast<std::byte*>(
      VirtualAlloc(nullptr, sizeof(FG::TaskShelf) * FG::kRackGroups, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  for (std::uint32_t g = 0; g < groups && g < FG::kRackGroups && shelves != nullptr; ++g) {
    auto* shelf = new (shelves + sizeof(FG::TaskShelf) * g) FG::TaskShelf();
    shelf->s.group = g;
    shelf->s.word = g;  // spike.10: the group's own index, as R1 asks of run 9's entries
    shelf->s.shared = rack;
    rack->r.shelves[g] = shelf;
    ++rack->r.shelves_made;
  }
  status("rack shelves=%llu", static_cast<unsigned long long>(rack->r.shelves_made));
  return rack;
}

// Spike.10 (R1 run 9's shape, WHY§289): the ledger, made while its static is null.
extern "C" void fg_ledger_missing() { fakegame::make_ledger(); }

// The request's work notes each task in the next of the hub's rows: an object of FG::TaskNote (its vtable in rdata), a
// fresh record in place of the static word, and the game thread's stamp FG_NOTE_STAMP_MS later.
extern "C" void fg_ledger_note(FG::TaskUnit*, std::uint32_t word) {
  fakegame::Ledger& l = ledger();
  if (l.rows == nullptr) return;
  const std::uint32_t row = l.notes++ % fakegame::kRows;
  auto* note = new (l.rows + FG_ROW_BYTES * row) FG::TaskNote();
  note->record = new std::uint64_t[2]{word, l.notes};  // never freed: a record the game keeps
  l.noted_us.store(fakegame::now_us(), std::memory_order_relaxed);
  l.pending.store(row, std::memory_order_release);
  if (!hub().registering_builtins) status("ledger note row=%u group=%u", row, word);
}

// The work, with the hub the registration was given (fg_shelf_take keeps it): a swapped call is refused there.
extern "C" void fg_ledger_keep(void*, std::uint32_t group, FG::TaskUnit* task) {
  fg_register_keep(static_cast<FG::TaskLoomImp*>(fg_ledger_given), group, task);
}

extern "C" std::uint64_t fg_ledger_forget(void*, FG::TaskUnit*, std::uint32_t, FG::TaskLoomImp*) { return 0; }

extern "C" FG::TaskUnit* fg_task_make(std::uint32_t which) { return ledger().boot[which & 3u]; }

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
