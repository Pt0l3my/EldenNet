// The fake game's world: every singleton (published in its static), the actors and their parts, the phases (title,
// loading, world), the program's own four tasks, and the moves of statics on request (ARCH §6.6's R5 has something to
// meet). All values are invented.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <string>
#include <new>
#include <utility>

#include "program.hpp"

// --- The invented classes' virtual functions (each answers a constant of its own) ------------------------------------
namespace FG {
void TaskUnit::attach(void*) {}
std::uint32_t TaskUnit::query() { return 0x51; }
std::uint32_t TaskLoomImp::loom_kind() const { return 0x101; }
void Actor::think(float) {}
std::uint32_t Actor::actor_kind() const { return 0x201; }
void PlayerActor::think(float) {}
std::uint32_t PlayerActor::actor_kind() const { return 0x202; }
std::uint32_t NpcActor::actor_kind() const { return 0x203; }
void BeastActor::think(float) {}
std::uint32_t BeastActor::actor_kind() const { return 0x204; }
std::uint32_t Saddle::seats() const { return 1; }
std::uint32_t MountActor::actor_kind() const { return 0x205; }
std::uint32_t MountActor::seats() const { return 2; }
std::uint32_t Prop::prop_kind() const { return 0x301; }
std::uint32_t Tombstone::tomb_kind() const { return 0xDEAD; }
std::uint32_t HeroSheet::sheet_kind() const { return 0x401; }
std::uint32_t SaveLedgerImp::ledger_kind() const { return 0x402; }
std::uint32_t RunWardenImp::warden_kind() const { return 0x403; }
std::uint32_t FieldPlotImp::plot_kind() const { return 0x404; }
std::uint32_t ActorRosterImp::roster_kind() const { return 0x405; }
std::uint32_t LobbyWardenImp::lobby_kind() const { return 0x406; }
std::uint32_t MenuDeskImp::menu_kind() const { return 0x407; }
std::uint32_t GaugeBoardImp::gauge_kind() const { return 0x408; }
std::uint32_t VeilKeeperImp::veil_kind() const { return 0x409; }
std::uint32_t SkyClockImp::sky_kind() const { return 0x40A; }
std::uint32_t EyeRigImp::eye_kind() const { return 0x40B; }
std::uint32_t MarkLedgerImp::mark_kind() const { return 0x40C; }
std::uint32_t TableVaultImp::vault_kind() const { return 0x40D; }
std::uint32_t PhraseBookImp::phrase_kind() const { return 0x40E; }

// The program's own tasks, one per group it uses.
class ClockTask final : public TaskUnit {
 public:
  void run(const FrameInfo* frame) override;
};
class MotionTask final : public TaskUnit {
 public:
  void run(const FrameInfo* frame) override;
};
class SettleTask final : public TaskUnit {
 public:
  void run(const FrameInfo* frame) override;
};
class MenuTask final : public TaskUnit {
 public:
  void run(const FrameInfo* frame) override;
};
}  // namespace FG

std::uint32_t Decoy::Actor::decoy_kind() const { return 0x501; }

namespace fakegame {
namespace {

constexpr char16_t kHeroName[] = u"Test Wanderer";  // invented
constexpr float kPi = 3.14159265f;

struct NpcMotion {
  float radius;
  float angle;
  float speed;
};

struct WorldState {
  Phase phase = Phase::Boot;
  std::uint64_t phase_frames = 0;
  std::uint64_t world_frames = 0;
  std::uint32_t generation = 0;
  FG::PlayerActor* player = nullptr;
  std::vector<FG::Actor*> npcs;
  std::vector<NpcMotion> motion;
  std::vector<FG::ActorParts*> parts;
  std::vector<FG::Prop*> props;
  Decoy::Actor* decoy = nullptr;
  float player_angle = 0.0f;
  float clock_s = 0.0f;
  float world_s = 0.0f;  // seconds in the world
  bool retitled = false; // the game went back to its title once (--retitle-at-s)
  bool online = false;
  WorldStats stats;
  FG::ClockTask clock_task;
  FgTaskBench motion_bench{};            // spike.10: the motion task is made in the bench, a member of it
  FG::MotionTask* motion_task = nullptr;
  FG::SettleTask settle_task;
  FG::MenuTask menu_task;
  std::uint8_t* flag_blocks[3] = {nullptr, nullptr, nullptr};  // groups 0, 7 and 4321
};

WorldState& state() {
  static WorldState w;
  static_assert(sizeof(FG::MotionTask) <= sizeof(FgTaskBench::task) && alignof(FG::MotionTask) <= 8);
  if (w.motion_task == nullptr) w.motion_task = new (w.motion_bench.task) FG::MotionTask();  // made in the bench
  return w;
}

void* alloc_block(std::size_t size) {
  return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

template <class T, class... A>
T* make(A&&... args) {
  return new (alloc_block(sizeof(T))) T(std::forward<A>(args)...);
}

// What becomes of an object's old place once it has moved (--move-mode): poisoned (a Tombstone, whose vtable no
// layout expects), freed (any read of it faults) or kept as it was. These classes own nothing, so the old object's
// life ends as its storage is reused or released, with no destructor to run.
template <class T>
void retire(T* old, const void* moved_to) {
  WorldState& w = state();
  switch (options().move_mode) {
    case MoveMode::Keep:
      return;
    case MoveMode::Poison: {
      std::memset(static_cast<void*>(old), 0, sizeof(T));
      auto* tomb = new (static_cast<void*>(old)) FG::Tombstone();
      // SAFETY: an address kept as a number, for a reader to see where the object went.
      tomb->moved_to = reinterpret_cast<std::uintptr_t>(moved_to);
      ++w.stats.poisoned;
      return;
    }
    case MoveMode::Free:
      VirtualFree(static_cast<void*>(old), 0, MEM_RELEASE);
      ++w.stats.freed;
      return;
  }
}

template <class T>
T* relocate(T* old) {
  T* moved = make<T>(*old);
  retire(old, moved);
  return moved;
}

template <class T>
void move_static(void*& slot) {
  if (auto* old = static_cast<T*>(slot)) slot = relocate(old);
}

template <class T>
T* get(void* (*accessor)()) {
  return static_cast<T*>(accessor());
}

std::uint64_t make_handle(std::uint32_t index, std::uint32_t generation) {
  return (std::uint64_t{0xFA} << 56) | (std::uint64_t{generation} << 24) | index;
}

FG::ActorParts* make_parts(std::int32_t max_hp, std::int32_t hp) {
  auto* parts = make<FG::ActorParts>();
  parts->vitals = make<FG::VitalsPart>();
  parts->motion = make<FG::MotionPart>();
  parts->react = make<FG::ReactPart>();
  parts->grip = make<FG::GripPart>();
  *parts->vitals = FG::VitalsPart{max_hp, hp, 90, 80, 120, 110};
  parts->motion->pos[3] = 1.0f;
  parts->react->recv_dmg_type = 0;
  parts->grip->throw_state = 0;
  return parts;
}

void free_parts(FG::ActorParts* parts) {
  if (options().move_mode != MoveMode::Free) return;  // kept (or left for the poisoned actor to point at)
  VirtualFree(parts->vitals, 0, MEM_RELEASE);
  VirtualFree(parts->motion, 0, MEM_RELEASE);
  VirtualFree(parts->react, 0, MEM_RELEASE);
  VirtualFree(parts->grip, 0, MEM_RELEASE);
  VirtualFree(parts, 0, MEM_RELEASE);
}

// --- The flag store, the text tables and the param tables (invented contents; fakegame.hpp has their layouts) ---
constexpr std::uint32_t kFlagGroups[3] = {0, 7, 4321};
constexpr std::uint32_t kFlagsAlways[] = {100, 4321010};
constexpr std::uint32_t kFlagWorld = 7005;  // set while in the world

void set_flag(std::uint32_t id, bool on) {
  WorldState& w = state();
  for (int g = 0; g < 3; ++g) {
    if (id / 1000 != kFlagGroups[g] || w.flag_blocks[g] == nullptr) continue;
    std::uint8_t& byte = w.flag_blocks[g][(id % 1000) / 8];
    const auto bit = static_cast<std::uint8_t>(1u << (7 - id % 8));
    byte = on ? static_cast<std::uint8_t>(byte | bit) : static_cast<std::uint8_t>(byte & ~bit);
  }
}

// --flag-set's flag: of the second group, in the holder's fourth block (byte 15 of it, bit 4).
constexpr std::uint32_t kFlagLate = 7123;
void set_late_flag() { set_flag(kFlagLate, true); }

FG::MarkLedgerImp* make_flag_store() {
  WorldState& w = state();
  auto* marks = make<FG::MarkLedgerImp>();
  auto* holder = static_cast<std::uint8_t*>(alloc_block(125 * 4));
  auto* own = static_cast<std::uint8_t*>(alloc_block(125));
  auto* head = make<FG::FlagNode>();
  auto* n0 = make<FG::FlagNode>();
  auto* n7 = make<FG::FlagNode>();
  auto* n4321 = make<FG::FlagNode>();
  // SAFETY: the block's address kept as a number, as a mode-2 descriptor holds it.
  const std::uint64_t own_at = reinterpret_cast<std::uintptr_t>(own);
  *head = FG::FlagNode{n0, n7, n4321, 0, 0, 0};
  *n7 = FG::FlagNode{n0, head, n4321, 7, 1, 3};  // mode 1: the block's index in the holder (the fourth block)
  *n0 = FG::FlagNode{head, n7, head, 0, 1, 0};
  *n4321 = FG::FlagNode{head, n7, head, 4321, 2, own_at};
  marks->map_head = head;
  marks->map_count = 3;
  marks->holder = holder;
  w.flag_blocks[0] = holder;
  w.flag_blocks[1] = holder + 125 * 3;
  w.flag_blocks[2] = own;
  for (const std::uint32_t id : kFlagsAlways) set_flag(id, true);
  return marks;
}

// One text table: a group of consecutive ids from `first`, each with its string, and one more id with none.
FG::PhraseTable* make_phrases(std::int32_t first, std::initializer_list<const char16_t*> names) {
  const auto n = static_cast<std::uint32_t>(names.size());
  std::size_t strings = 0;
  for (const char16_t* t : names) strings += 2 * (std::char_traits<char16_t>::length(t) + 1);
  const std::size_t offsets_off = 0x30;
  const std::size_t strings_off = offsets_off + 8 * (n + 1);
  auto* base = static_cast<std::byte*>(alloc_block(strings_off + strings));
  auto* table = new (base) FG::PhraseTable{0, 1, 0x20, offsets_off, 0};
  const FG::PhraseGroup group{0, first, first + static_cast<std::int32_t>(n)};
  std::memcpy(base + 0x20, &group, sizeof group);
  std::size_t at = strings_off;
  std::uint32_t i = 0;
  for (const char16_t* t : names) {
    const std::int64_t off = static_cast<std::int64_t>(at);
    std::memcpy(base + offsets_off + 8 * i, &off, 8);
    const std::size_t len = std::char_traits<char16_t>::length(t);
    std::memcpy(base + at, t, 2 * len);
    at += 2 * (len + 1);
    ++i;
  }
  return table;  // the last id's offset stays 0: no string
}

FG::PhraseBookImp* make_phrase_book() {
  auto* book = make<FG::PhraseBookImp>();
  auto** tables = static_cast<FG::PhraseTable**>(alloc_block(sizeof(void*) * FG::kPhraseTables));
  tables[FG::kNpcNameTable] = make_phrases(1100, {u"Ash Warden", u"Brine Keeper", u"Cinder Monk"});
  tables[FG::kPlaceNameTable] = make_phrases(6100100, {u"Quiet Vale"});
  tables[3] = make_phrases(500, {u"Lantern Oil"});
  book->tables = tables;
  book->count = static_cast<std::int32_t>(FG::kPhraseTables);
  return book;
}

FG::TableVaultImp* make_table_vault() {
  auto* vault = make<FG::TableVaultImp>();
  constexpr std::uint32_t kRows = 6;
  auto* rows = static_cast<FG::ParamRowRef*>(alloc_block(sizeof(FG::ParamRowRef) * kRows));
  for (std::uint32_t i = 0; i < kRows; ++i) {
    auto* row = make<FG::NpcRow>();
    row->name_id = static_cast<std::int32_t>(1100 + (i + 1) % 3);
    row->team = static_cast<std::uint8_t>(10 + (i + 1) % 3);
    rows[i] = FG::ParamRowRef{81000 + 100 * (i + 1), 0, row};  // the actors' npc_param_ids (fill_actor)
  }
  auto* sheet = make<FG::ParamSheet>();
  *sheet = FG::ParamSheet{rows, static_cast<std::uint16_t>(kRows), static_cast<std::uint16_t>(sizeof(FG::NpcRow)), 0, 0};
  auto** tables = static_cast<FG::ParamSheet**>(alloc_block(sizeof(void*) * 8));
  tables[FG::kNpcSheetSlot] = sheet;
  vault->tables = tables;
  vault->table_count = 8;
  return vault;
}

void set_screen(std::int32_t screen) {
  if (auto* menu = get<FG::MenuDeskImp>(fg_get_menu)) menu->screen_state = screen;
}

void apply_session() {
  WorldState& w = state();
  if (auto* lobby = get<FG::LobbyWardenImp>(fg_get_lobby)) lobby->lobby_state = w.online ? 12 : 10;
  if (auto* ledger = static_cast<FG::SaveLedgerImp*>(fg_static_save_ledger); ledger && ledger->f.hero) {
    ledger->f.hero->h.multiplay_role = w.online ? 3 : 2;
    ledger->f.hero->h.is_my_world = w.online ? 0 : 1;
  }
}

void fill_actor(FG::Actor& a, std::uint32_t index, std::uint32_t generation, FG::ActorParts* parts) {
  a.a.entity_id = 20000 + index;
  a.a.model = 0x1100 + 0x10 * (index % 5);
  a.a.handle = make_handle(index, generation);
  a.a.npc_param_id = index == 0 ? 0 : 81000 + 100 * index;
  a.a.chr_type = index == 0 ? 0 : 2 + static_cast<std::int32_t>(index % 2);
  a.a.block_id = 0x0A1B0000u | index;
  a.a.team = static_cast<std::uint8_t>(index == 0 ? 1 : 10 + index % 3);
  a.a.flags = static_cast<std::uint8_t>(1u << FG::kActorOnScreenBit);
  a.a.parts = parts;
}

void rebuild_near() {
  WorldState& w = state();
  auto* roster = get<FG::ActorRosterImp>(fg_get_roster);
  if (roster == nullptr) return;
  FG::NearEntry* begin = roster->f.near_begin;
  std::size_t n = 0;
  if (w.player != nullptr) {
    const float* pp = w.player->a.parts->motion->pos;
    for (FG::Actor* npc : w.npcs) {
      const float* p = npc->a.parts->motion->pos;
      const float dx = p[0] - pp[0], dy = p[1] - pp[1], dz = p[2] - pp[2];
      const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
      begin[n++] = FG::NearEntry{npc, d, 0};
      npc->a.flags = static_cast<std::uint8_t>(d < 30.0f ? (npc->a.flags | (1u << FG::kActorOnScreenBit))
                                                         : (npc->a.flags & ~(1u << FG::kActorOnScreenBit)));
    }
  }
  std::sort(begin, begin + n, [](const FG::NearEntry& l, const FG::NearEntry& r) { return l.distance < r.distance; });
  roster->f.near_end = begin + n;
}

void spawn() {
  WorldState& w = state();
  ++w.generation;
  w.player = make<FG::PlayerActor>();
  fill_actor(*w.player, 0, w.generation, make_parts(1200, 1100));
  w.player->p.mount_state = 0;
  w.parts.push_back(w.player->a.parts);
  const std::uint32_t count = std::min<std::uint32_t>(options().actors, FG::kMaxNear - 1);
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto max_hp = static_cast<std::int32_t>(400 + 150 * i);
    const std::int32_t hp = i == 3 ? 0 : max_hp - static_cast<std::int32_t>(37 * i);
    FG::ActorParts* parts = make_parts(max_hp, hp);
    FG::Actor* npc = nullptr;
    switch (i % 4) {
      case 0: npc = make<FG::NpcActor>(); break;
      case 1: npc = make<FG::BeastActor>(); break;
      case 2: npc = make<FG::MountActor>(); break;
      default: npc = make<FG::Actor>(); break;
    }
    fill_actor(*npc, i + 1, w.generation, parts);
    if (hp == 0) npc->a.flags = static_cast<std::uint8_t>(npc->a.flags | (1u << FG::kActorDeadBit));
    w.npcs.push_back(npc);
    w.parts.push_back(parts);
    w.motion.push_back(NpcMotion{4.0f + 3.0f * static_cast<float>(i), 0.7f * static_cast<float>(i),
                                 0.3f + 0.05f * static_cast<float>(i)});
  }
  auto* roster = get<FG::ActorRosterImp>(fg_get_roster);
  roster->f.main_player = w.player;
  roster->f.players[0] = FG::PlayerSlot{w.player, 0};
  roster->f.player_count = 1;
  rebuild_near();
}

void despawn() {
  WorldState& w = state();
  if (auto* roster = get<FG::ActorRosterImp>(fg_get_roster)) {
    roster->f.main_player = nullptr;
    roster->f.players[0] = FG::PlayerSlot{nullptr, 0};
    roster->f.player_count = 0;
    roster->f.near_end = roster->f.near_begin;
  }
  for (FG::Actor* npc : w.npcs) retire(npc, nullptr);
  if (w.player != nullptr) retire(w.player, nullptr);
  for (FG::ActorParts* parts : w.parts) free_parts(parts);
  w.npcs.clear();
  w.motion.clear();
  w.parts.clear();
  w.player = nullptr;
}

void enter(Phase phase, std::uint64_t frame) {
  WorldState& w = state();
  if (phase == Phase::Loading || (phase == Phase::Title && w.retitled)) despawn();  // back at the title: no world
  w.phase = phase;
  w.phase_frames = 0;
  // --slot-by-state: the save's slot is known only in the world.
  if (options().slot_by_state && fg_static_run_warden != nullptr) {
    const std::int32_t slot = w.retitled ? 2 : static_cast<std::int32_t>(options().first_slot);
    static_cast<FG::RunWardenImp*>(fg_static_run_warden)->f.save_slot = phase == Phase::World ? slot : -1;
  }
  set_flag(kFlagWorld, phase == Phase::World);
  switch (phase) {
    case Phase::Title: set_screen(1); break;
    case Phase::Loading: set_screen(2); ++w.stats.loads; break;
    case Phase::World:
      spawn();
      hub_world_loaded();  // spike.12: the words the world adds to the manager's map, at its first load
      set_screen(4);
      w.world_frames = 0;
      break;
    case Phase::Boot: break;
  }
  if (phase == Phase::World) {
    status("phase world frame=%llu player=%p actors=%zu", static_cast<unsigned long long>(frame),
           static_cast<const void*>(w.player), w.npcs.size());
  } else {
    status("phase %s frame=%llu", phase_name(phase), static_cast<unsigned long long>(frame));
  }
}

void go_online(std::uint64_t frame) {
  state().online = true;
  apply_session();
  status("session online frame=%llu", static_cast<unsigned long long>(frame));
}

}  // namespace

const char* phase_name(Phase p) {
  switch (p) {
    case Phase::Boot: return "boot";
    case Phase::Title: return "title";
    case Phase::Loading: return "loading";
    case Phase::World: return "world";
  }
  return "?";
}

Phase world_phase() { return state().phase; }
WorldStats world_stats() { return state().stats; }

void world_publish_hinstance(HINSTANCE instance) { (void)fg_boot_publish_instance(instance); }

void world_create_singletons() {
  hub_create();
  auto* roster = make<FG::ActorRosterImp>();
  auto* entries = static_cast<FG::NearEntry*>(alloc_block(sizeof(FG::NearEntry) * FG::kMaxNear));
  roster->f.near_begin = entries;
  roster->f.near_end = entries;
  roster->f.near_cap = entries + FG::kMaxNear;
  roster->f.players = static_cast<FG::PlayerSlot*>(alloc_block(sizeof(FG::PlayerSlot) * 4));
  fg_static_roster = roster;
  fg_static_lobby = make<FG::LobbyWardenImp>();
  fg_static_marks = make_flag_store();
  fg_static_gauges = make<FG::GaugeBoardImp>();
  auto* menu = make<FG::MenuDeskImp>();
  menu->popup = make<FG::PopupState>();
  fg_static_menu = menu;
  fg_static_veil = make<FG::VeilKeeperImp>();
  fg_static_sky = make<FG::SkyClockImp>();
  auto* eye = make<FG::EyeRigImp>();
  eye->view = make<FG::ViewData>();
  for (int i = 0; i < 4; ++i) eye->view->matrix[i * 5] = 1.0f;  // identity rotation
  eye->view->fov = 0.9f;
  eye->view->aspect = 16.0f / 9.0f;
  fg_static_eye = eye;
  fg_static_tables = make_table_vault();
  fg_static_phrases = make_phrase_book();
  // The decoys' statics hold objects of their own, so that each would resolve if its site were believed.
  // --day-book: a plain block in the first decoy's place, which the clock's task keeps the time in.
  fg_static_duplicata_a = options().day_book ? alloc_block(0x40) : static_cast<void*>(make<FG::Prop>());
  fg_static_duplicata_b = make<FG::Prop>();
  fg_static_strayling = make<FG::Prop>();

  // The AOB statics: the ledger (with the hero sheet and the settings), the warden and the plot.
  auto* hero = make<FG::HeroSheet>();
  hero->h.level = 12;
  hero->h.runes = 4321;
  hero->h.archetype = 3;
  hero->h.body = 1;
  hero->h.keepsake = 2;
  for (std::size_t i = 0; i + 1 < sizeof kHeroName / sizeof kHeroName[0]; ++i) hero->h.name[i] = kHeroName[i];
  const std::uint32_t gear[6] = {1100000, 2200000, 3300000, 3301000, 3302000, 3303000};
  std::copy(std::begin(gear), std::end(gear), std::begin(hero->h.gear));
  auto* ledger = make<FG::SaveLedgerImp>();
  ledger->f.hero = hero;
  ledger->f.settings = make<FG::SettingsBlock>();
  ledger->f.settings->show_subtitles = 1;
  ledger->f.ng_level = 0;
  (void)fg_save_ledger_swap(ledger);
  auto* warden = make<FG::RunWardenImp>();
  warden->f.save_slot = options().slot_by_state ? -1 : 2;
  warden->f.talk_cam_param_id = -1;
  fg_static_run_warden = warden;
  auto* plot = make<FG::FieldPlotImp>();
  plot->f.place_id = 6100100;
  fg_static_field_plot = plot;
  if (options().field_mirror) fg_field_plot_mirror = plot;

  WorldState& w = state();
  w.online = options().online;
  apply_session();
  for (int i = 0; i < 2; ++i) {
    w.props.push_back(make<FG::Prop>());
    w.props.back()->id = 900 + static_cast<std::uint64_t>(i);
  }
  w.decoy = make<Decoy::Actor>();
}

void world_register_tasks() {
  WorldState& w = state();
  FG::TaskUnit* const tasks[4] = {&w.clock_task, w.motion_task, &w.settle_task, &w.menu_task};
  const std::uint32_t shift = options().shift_groups;
  const std::uint32_t groups[4] = {0 + shift, 4 + shift, kStandinBase + shift, 7 + shift};
  hub_register_builtins(tasks, groups, &w.motion_bench);
}

void world_frame_begin(std::uint64_t frame) {
  WorldState& w = state();
  const Options& o = options();
  if (w.phase == Phase::Boot) enter(Phase::Title, frame);
  switch (w.phase) {
    case Phase::Title:
      // The first title lasts --title-frames; the second (--retitle-at-s) --retitle-for-s seconds.
      if (!o.stay_title &&
          w.phase_frames >= (w.retitled ? std::uint64_t{o.retitle_for_s} * std::max<std::uint32_t>(o.fps, 1) : o.title_frames)) {
        enter(Phase::Loading, frame);
      }
      break;
    case Phase::Loading:
      if (auto* veil = get<FG::VeilKeeperImp>(fg_get_veil)) ++veil->load_counter;
      if (w.phase_frames >= o.load_frames) enter(Phase::World, frame);
      break;
    case Phase::World:
      ++w.world_frames;
      if (o.retitle_at_s != 0 && !w.retitled && w.world_s >= static_cast<float>(o.retitle_at_s)) {
        w.retitled = true;
        enter(Phase::Title, frame);
      } else if (o.reload_every != 0 && w.world_frames % o.reload_every == 0) {
        enter(Phase::Loading, frame);
      } else if (o.move_every != 0 && w.world_frames % o.move_every == 0) {
        world_move_all();
      }
      break;
    case Phase::Boot:
      break;
  }
  if (o.online_at != 0 && frame == o.online_at && !w.online) go_online(frame);
  ++w.phase_frames;
}

void world_move_all() {
  WorldState& w = state();
  const void* roster_before = fg_static_roster;
  const void* player_before = w.player;
  move_static<FG::TaskLoomImp>(fg_static_taskloom);
  move_static<FG::ActorRosterImp>(fg_static_roster);
  move_static<FG::LobbyWardenImp>(fg_static_lobby);
  move_static<FG::MarkLedgerImp>(fg_static_marks);
  move_static<FG::GaugeBoardImp>(fg_static_gauges);
  move_static<FG::MenuDeskImp>(fg_static_menu);
  move_static<FG::VeilKeeperImp>(fg_static_veil);
  move_static<FG::SkyClockImp>(fg_static_sky);
  move_static<FG::EyeRigImp>(fg_static_eye);
  move_static<FG::TableVaultImp>(fg_static_tables);
  move_static<FG::PhraseBookImp>(fg_static_phrases);
  move_static<FG::RunWardenImp>(fg_static_run_warden);
  move_static<FG::FieldPlotImp>(fg_static_field_plot);
  if (options().field_mirror) fg_field_plot_mirror = fg_static_field_plot;
  if (auto* old = static_cast<FG::SaveLedgerImp*>(fg_static_save_ledger)) {
    auto* moved = make<FG::SaveLedgerImp>(*old);
    if (moved->f.hero != nullptr) moved->f.hero = relocate(moved->f.hero);
    (void)fg_save_ledger_swap(moved);
    retire(old, moved);
  }
  if (w.player != nullptr) {
    w.player = relocate(w.player);
    auto* roster = get<FG::ActorRosterImp>(fg_get_roster);
    roster->f.main_player = w.player;
    roster->f.players[0].actor = w.player;
  }
  ++w.stats.moves;
  status("moved n=%u roster=%p->%p player=%p->%p mode=%s", w.stats.moves, roster_before,
         static_cast<const void*>(fg_static_roster), player_before, static_cast<const void*>(w.player),
         options().move_mode == MoveMode::Poison ? "poison" : options().move_mode == MoveMode::Free ? "free" : "keep");
}

void world_build_for_describe() {
  world_create_singletons();
  spawn();
}

std::vector<ClassSample> class_samples() {
  WorldState& w = state();
  std::vector<ClassSample> out;
  auto add = [&out](const char* name, const char* mangled, const void* object, std::uint32_t offset, bool actor,
                    const char* bases) {
    if (object != nullptr) out.push_back(ClassSample{name, mangled, object, offset, actor, bases});
  };
  add("FG::TaskLoomImp", ".?AVTaskLoomImp@FG@@", fg_static_taskloom, 0, false, "");
  add("FG::ActorRosterImp", ".?AVActorRosterImp@FG@@", fg_static_roster, 0, false, "");
  add("FG::LobbyWardenImp", ".?AVLobbyWardenImp@FG@@", fg_static_lobby, 0, false, "");
  add("FG::MarkLedgerImp", ".?AVMarkLedgerImp@FG@@", fg_static_marks, 0, false, "");
  add("FG::GaugeBoardImp", ".?AVGaugeBoardImp@FG@@", fg_static_gauges, 0, false, "");
  add("FG::MenuDeskImp", ".?AVMenuDeskImp@FG@@", fg_static_menu, 0, false, "");
  add("FG::VeilKeeperImp", ".?AVVeilKeeperImp@FG@@", fg_static_veil, 0, false, "");
  add("FG::SkyClockImp", ".?AVSkyClockImp@FG@@", fg_static_sky, 0, false, "");
  add("FG::EyeRigImp", ".?AVEyeRigImp@FG@@", fg_static_eye, 0, false, "");
  add("FG::TableVaultImp", ".?AVTableVaultImp@FG@@", fg_static_tables, 0, false, "");
  add("FG::PhraseBookImp", ".?AVPhraseBookImp@FG@@", fg_static_phrases, 0, false, "");
  add("FG::SaveLedgerImp", ".?AVSaveLedgerImp@FG@@", fg_static_save_ledger, 0, false, "");
  add("FG::RunWardenImp", ".?AVRunWardenImp@FG@@", fg_static_run_warden, 0, false, "");
  add("FG::FieldPlotImp", ".?AVFieldPlotImp@FG@@", fg_static_field_plot, 0, false, "");
  if (auto* ledger = static_cast<FG::SaveLedgerImp*>(fg_static_save_ledger)) {
    add("FG::HeroSheet", ".?AVHeroSheet@FG@@", ledger->f.hero, 0, false, "");
  }
  add("FG::PlayerActor", ".?AVPlayerActor@FG@@", w.player, 0, true, "FG::Actor");
  for (FG::Actor* npc : w.npcs) {
    switch (npc->actor_kind()) {
      case 0x201: add("FG::Actor", ".?AVActor@FG@@", npc, 0, true, ""); break;
      case 0x203: add("FG::NpcActor", ".?AVNpcActor@FG@@", npc, 0, true, "FG::Actor"); break;
      case 0x204: add("FG::BeastActor", ".?AVBeastActor@FG@@", npc, 0, true, "FG::NpcActor,FG::Actor"); break;
      case 0x205: {
        auto* mount = static_cast<FG::MountActor*>(npc);
        add("FG::MountActor", ".?AVMountActor@FG@@", mount, 0, true, "FG::NpcActor,FG::Actor,FG::Saddle");
        const auto* saddle = static_cast<const FG::Saddle*>(mount);
        // SAFETY: the distance between two subobjects of one live object, as a number.
        const auto offset = static_cast<std::uint32_t>(reinterpret_cast<const std::byte*>(saddle) -
                                                       reinterpret_cast<const std::byte*>(mount));
        add("FG::MountActor@FG::Saddle", ".?AVMountActor@FG@@", mount, offset, true,
            "FG::NpcActor,FG::Actor,FG::Saddle");
        break;
      }
      default: break;
    }
  }
  if (!w.props.empty()) add("FG::Prop", ".?AVProp@FG@@", w.props.front(), 0, false, "");
  add("Decoy::Actor", ".?AVActor@Decoy@@", w.decoy, 0, false, "");
  add("FG::ClockTask", ".?AVClockTask@FG@@", &w.clock_task, 0, false, "FG::TaskUnit");
  add("FG::MotionTask", ".?AVMotionTask@FG@@", w.motion_task, 0, false, "FG::TaskUnit");
  add("FG::SettleTask", ".?AVSettleTask@FG@@", &w.settle_task, 0, false, "FG::TaskUnit");
  add("FG::MenuTask", ".?AVMenuTask@FG@@", &w.menu_task, 0, false, "FG::TaskUnit");
  static FG::Tombstone tomb;
  add("FG::Tombstone", ".?AVTombstone@FG@@", &tomb, 0, false, "");
  // Remove repeats of one class (several actors share theirs), keeping the first.
  std::vector<ClassSample> unique;
  for (const ClassSample& s : out) {
    const bool seen = std::any_of(unique.begin(), unique.end(),
                                  [&s](const ClassSample& u) { return std::strcmp(u.name, s.name) == 0; });
    if (!seen) unique.push_back(s);
  }
  return unique;
}

}  // namespace fakegame

// --- The program's own tasks ------------------------------------------------------------------------------------------------
using fakegame::Phase;

void FG::ClockTask::run(const FrameInfo* frame) {
  auto& w = fakegame::state();
  if (w.phase == Phase::World) {
    if (auto* ledger = static_cast<SaveLedgerImp*>(fg_static_save_ledger)) {
      ledger->f.play_time_ms += static_cast<std::uint32_t>(frame->dt * 1000.0f + 0.5f);
    }
    w.clock_s += frame->dt * static_cast<float>(fakegame::options().clock_rate);  // a game minute a second, unless asked
    const float before = w.world_s;
    w.world_s += frame->dt;
    // --pass-time: time is let pass twice, five hours and then three. --flag-set: one flag is set, once.
    if (fakegame::options().pass_time) {
      if (before < 14.0f && w.world_s >= 14.0f) w.clock_s += 5.0f * 3600.0f;
      if (before < 26.0f && w.world_s >= 26.0f) w.clock_s += 3.0f * 3600.0f;
    }
    if (fakegame::options().flag_set && before < 30.0f && w.world_s >= 30.0f) fakegame::set_late_flag();
  }
  if (auto* sky = static_cast<SkyClockImp*>(fg_get_sky())) {
    const auto total = static_cast<std::uint64_t>(w.clock_s) + 9 * 3600;
    sky->clock = ((total / 3600) % 24) << 32 | ((total / 60) % 60) << 16 | (total % 60);
    sky->hours = static_cast<float>(total % 86400) / 3600.0f;
    if (fakegame::options().day_book && fg_static_duplicata_a != nullptr) {
      auto* book = static_cast<std::uint64_t*>(fg_static_duplicata_a);
      book[2] = sky->clock;
      book[3] = sky->clock;
    }
  }
}

void FG::MotionTask::run(const FrameInfo* frame) {
  auto& w = fakegame::state();
  if (w.phase != Phase::World || w.player == nullptr) return;
  w.player_angle += 0.05f * frame->dt;
  float* pp = w.player->a.parts->motion->pos;
  pp[0] = 100.0f + 20.0f * std::cos(w.player_angle);
  pp[1] = 12.0f;
  pp[2] = 200.0f + 20.0f * std::sin(w.player_angle);
  for (std::size_t i = 0; i < w.npcs.size(); ++i) {
    fakegame::NpcMotion& m = w.motion[i];
    m.angle += m.speed * frame->dt;
    if (m.angle > 2.0f * fakegame::kPi) m.angle -= 2.0f * fakegame::kPi;
    float* p = w.npcs[i]->a.parts->motion->pos;
    p[0] = pp[0] + m.radius * std::cos(m.angle);
    p[1] = pp[1];
    p[2] = pp[2] + m.radius * std::sin(m.angle);
  }
  fakegame::rebuild_near();
}

void FG::SettleTask::run(const FrameInfo* frame) {
  auto& w = fakegame::state();
  auto* gauges = static_cast<GaugeBoardImp*>(fg_get_gauges());
  if (gauges == nullptr) return;
  if (w.phase == Phase::World && !w.npcs.empty()) {
    gauges->bars[0] = GaugeBar{w.npcs.back()->a.handle, 9100, static_cast<std::int32_t>(frame->frame % 500)};
    if (frame->frame % 120 == 0) w.npcs.front()->a.last_hit_by = w.player != nullptr ? w.player->a.handle : 0;
  } else {
    gauges->bars[0] = GaugeBar{0, -1, 0};
  }
}

void FG::MenuTask::run(const FrameInfo* frame) {
  auto& w = fakegame::state();
  if (auto* gauges = static_cast<GaugeBoardImp*>(fg_get_gauges())) gauges->hud_state = w.phase == Phase::World ? 3 : 0;
  if (auto* menu = static_cast<MenuDeskImp*>(fg_get_menu_popup_site()); menu != nullptr && menu->popup != nullptr) {
    menu->popup->talk_id = w.phase == Phase::World && frame->frame % 600 < 60 ? 7100 : -1;
  }
  (void)fg_run_warden_slot();
  (void)fg_field_plot_place();
  (void)fg_get_roster_near();
  (void)fg_get_marks();
  (void)fg_get_tables();
  (void)fg_get_phrases();
  (void)fg_get_eye();
}

// The FD4-style accessors' log: a singleton was used before its static was set (asm.cpp). Counted; with --verbose,
// the first time for each site is a status line.
extern "C" void fg_fd4_unset(const char* path, const char* name, std::uint32_t site) {
  auto& w = fakegame::state();
  ++w.stats.fd4_unset;
  static std::uint32_t seen[8]{};
  const std::uint32_t bit = site % 256;
  if (fakegame::options().verbose && (seen[bit / 32] & (1u << (bit % 32))) == 0) {
    seen[bit / 32] |= 1u << (bit % 32);
    fakegame::status("fd4-unset name=%s site=%u path_ok=%d", name, site,
                     std::strlen(path) >= 14 && std::strcmp(path + std::strlen(path) - 14, "FD4Singleton.h") == 0);
  }
}
