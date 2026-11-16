// galtest.dll (CMakeLists.txt): EldenNet's T1 and task in miniature, for the game-access layer's tests in the fake
// game. DllMain only starts a thread (ARCH §3.1); that thread gates the image, runs discovery with its retries,
// registers the task once and then reports, every 500 ms, numbers only: what was found, what the snapshots hold, the
// groups' states, the task's cost. Its settings come from the environment, which the harness sets:
//   EN_GALTEST_DIR      the gal/ folder (builds.toml, patterns.toml, layouts/)          (required)
//   EN_GALTEST_STATUS   a file the lines are appended to as well (a run with no console, on Windows)
//   EN_GALTEST_COPY     seh (default) or rpm: the copy primitive (ARCH §6.5, S0-D)
//   EN_GALTEST_FAULT    <group>:<every>: the probe's fault injection, from EN_GALTEST_FAULT_AFTER s in the world
//   EN_GALTEST_FAULT_AFTER  seconds (default 2)
// Everything it reads and prints belongs to the fake game, whose every fact is invented.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "en/gal/discovery.hpp"
#include "en/gal/native_sys.hpp"
#include "en/gal/probe.hpp"
#include "en/gal/task.hpp"
#include "en/proto/ffi_guard.hpp"

using namespace en::gal;
using en::proto::GameCommand;
using en::proto::Group;

// ---- The allocation watch (R6): this DLL's own operator new and delete, counted on the game thread when armed ----
namespace {
thread_local bool t_armed = false;
constinit std::atomic<std::uint64_t> g_news{0};
constinit std::atomic<std::uint64_t> g_deletes{0};
}  // namespace

namespace {

void count_new() noexcept {
  if (t_armed) g_news.fetch_add(1, std::memory_order_relaxed);
}
void count_delete(void* p) noexcept {
  if (p != nullptr && t_armed) g_deletes.fetch_add(1, std::memory_order_relaxed);
}

void* allocate(std::size_t size) noexcept {
  count_new();
  return std::malloc(size == 0 ? 1 : size);
}

void* allocate(std::size_t size, std::align_val_t alignment) noexcept {
  count_new();
  const auto align = static_cast<std::size_t>(alignment);
#ifdef _WIN32
  return _aligned_malloc(size == 0 ? 1 : size, align);
#else
  void* p = nullptr;
  return posix_memalign(&p, align, size == 0 ? 1 : size) == 0 ? p : nullptr;
#endif
}

void release(void* p) noexcept {
  count_delete(p);
  std::free(p);
}

void release_aligned(void* p) noexcept {
  count_delete(p);
#ifdef _WIN32
  _aligned_free(p);
#else
  std::free(p);
#endif
}

template <class P>
P* or_throw(P* p) {
  if (p == nullptr) throw std::bad_alloc();
  return p;
}

}  // namespace

// All twenty forms, so that no memory passes between this program's allocator and a runtime's (a sanitizer's nothrow
// or aligned new freed here by free() would be a mismatch).
void* operator new(std::size_t size) { return or_throw(allocate(size)); }
void* operator new[](std::size_t size) { return or_throw(allocate(size)); }
void* operator new(std::size_t size, std::align_val_t a) { return or_throw(allocate(size, a)); }
void* operator new[](std::size_t size, std::align_val_t a) { return or_throw(allocate(size, a)); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void* operator new(std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept { return allocate(size, a); }
void* operator new[](std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept { return allocate(size, a); }
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, std::size_t) noexcept { release(p); }
void operator delete[](void* p, std::size_t) noexcept { release(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete(void* p, std::align_val_t) noexcept { release_aligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { release_aligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { release_aligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { release_aligned(p); }

namespace {

// ---- Output ----

char g_status_file[1024] = {};  // set once on T1, before any line is printed

void line(const char* fmt, ...) {
  char buf[2048];
  int n = std::snprintf(buf, sizeof buf, "GALTEST ");
  va_list args;
  va_start(args, fmt);
  const int m = std::vsnprintf(buf + n, sizeof buf - static_cast<std::size_t>(n) - 2, fmt, args);
  va_end(args);
  n += m < 0 ? 0 : std::min(m, static_cast<int>(sizeof buf) - n - 2);
  buf[n++] = '\n';
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(n), &written, nullptr);
  if (g_status_file[0] != '\0') {
    std::ofstream f(g_status_file, std::ios::app | std::ios::binary);
    f.write(buf, n);
  }
}

std::string env(const char* name) {
  char* value = nullptr;
  std::size_t len = 0;
  std::string out;
  if (_dupenv_s(&value, &len, name) == 0 && value != nullptr) out = value;
  std::free(value);
  return out;
}

std::optional<std::string> read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return std::nullopt;
  std::ostringstream s;
  s << f.rdbuf();
  return s.str();
}

// ---- The core's side, in miniature: commands in, snapshots summarised into atomics ----

struct Summary {
  std::atomic<std::int64_t> chars{-1}, level{-1}, runes{-1}, hp{-1}, max_hp{-1}, slot{-1}, place{-1}, hour{-1},
      minute{-1}, lobby{-1}, role{-1}, screen{-1}, hud{-1}, mode{-1}, near_handle{0}, near_param{-1}, dead_chars{-1},
      boss_bars{-1}, subtitles{-1}, player_set{-1}, play_ms{-1}, gap{0};
  std::atomic<std::uint64_t> flag_frames{0}, flag_bits{0}, flag_valid{0}, flag_version{0};
  std::atomic<std::uint64_t> texts{0}, text_named{0}, text_empty{0}, params{0}, param_name_id{0};
  std::atomic<std::uint64_t> renders{0}, render_present{0};
  std::atomic<std::uint64_t> data_ready{0}, playthroughs{0}, trips{0}, trip_groups{0}, name_len{0};
  std::atomic<std::uint64_t> published{0}, dropped{0};
};

class Io final : public TaskIo {
 public:
  explicit Io(Summary& s) : s_(s), building_(std::make_unique<en::proto::WorldSnapshot>()) {}

  bool pop_command(GameCommand& out) noexcept override {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    if (head == tail_.load(std::memory_order_acquire)) return false;
    out = std::move(ring_[head % ring_.size()]);
    head_.store(head + 1, std::memory_order_release);
    return true;
  }
  // From the reporter thread (the only producer).
  bool push_command(GameCommand c) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail - head_.load(std::memory_order_acquire) >= ring_.size()) return false;
    ring_[tail % ring_.size()] = std::move(c);
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }
  en::proto::WorldSnapshot* begin_snapshot() noexcept override { return building_.get(); }
  void end_snapshot(bool built) noexcept override {
    if (!built) {
      s_.dropped.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    s_.published.fetch_add(1, std::memory_order_relaxed);
    const en::proto::WorldSnapshot& w = *building_;
    s_.gap.fetch_add(w.gap ? 1 : 0, std::memory_order_relaxed);
    s_.screen.store(static_cast<std::int64_t>(w.life.screen));
    s_.lobby.store(static_cast<std::int64_t>(w.session.lobby));
    s_.role.store(static_cast<std::int64_t>(w.session.role));
    s_.player_set.store(w.session.player_set_len);
    s_.chars.store(static_cast<std::int64_t>(w.chars.size()));
    std::int64_t dead = 0;
    for (const auto& c : w.chars) dead += c.dead ? 1 : 0;
    s_.dead_chars.store(dead);
    if (!w.chars.empty()) {
      s_.near_handle.store(static_cast<std::int64_t>(w.chars[0].handle.raw));
      s_.near_param.store(w.chars[0].param);
    }
    if (w.player) {
      s_.level.store(w.player->level);
      s_.runes.store(w.player->runes_held);
      s_.hp.store(w.player->hp.cur);
      s_.max_hp.store(w.player->hp.max);
      s_.slot.store(w.player->save_slot);
      s_.play_ms.store(static_cast<std::int64_t>(w.player->play_time_ms));
    }
    if (w.place && w.place->place_id) s_.place.store(*w.place->place_id);
    if (w.clock) {
      s_.hour.store(w.clock->h);
      s_.minute.store(w.clock->m);
    }
    s_.hud.store(static_cast<std::int64_t>(w.ui.hud));
    s_.subtitles.store(w.ui.subtitles_on ? 1 : 0);
    s_.boss_bars.store(static_cast<std::int64_t>(w.boss.bars.size()));
    if (w.flags) {
      s_.flag_frames.fetch_add(1, std::memory_order_relaxed);
      std::uint64_t bits = 0, valid = 0;
      for (std::size_t i = 0; i < 8; ++i) {
        bits |= static_cast<std::uint64_t>(w.flags->bits[i]) << i;
        valid |= static_cast<std::uint64_t>(w.flags->valid[i]) << i;
      }
      s_.flag_bits.store(bits);
      s_.flag_valid.store(valid);
      s_.flag_version.store(w.flags->watch_version);
    }
    for (const auto& t : w.new_text) {
      s_.texts.fetch_add(1, std::memory_order_relaxed);
      (t.text ? s_.text_named : s_.text_empty).fetch_add(1, std::memory_order_relaxed);
    }
    for (const auto& p : w.new_params) {
      s_.params.fetch_add(1, std::memory_order_relaxed);
      if (!p.fields.empty()) s_.param_name_id.store(static_cast<std::uint64_t>(p.fields[0].value));
    }
  }
  bool push_lifecycle(const en::proto::Lifecycle& l) noexcept override {
    if (std::holds_alternative<en::proto::Lifecycle::DataReady>(l.payload)) s_.data_ready.fetch_add(1);
    if (const auto* p = std::get_if<en::proto::Lifecycle::PlaythroughReady>(&l.payload)) {
      s_.playthroughs.fetch_add(1);
      s_.name_len.store(p->name.size());  // its length only: the name is never printed
    }
    if (const auto* g = std::get_if<en::proto::Lifecycle::GroupTripped>(&l.payload)) {
      s_.trips.fetch_add(1);
      s_.trip_groups.fetch_or(1ull << static_cast<unsigned>(g->group));
    }
    return true;
  }
  bool retire(std::shared_ptr<const std::vector<std::uint32_t>>& list) noexcept override {
    // Kept until the reporter thread drops it: never freed on the game thread (C5).
    for (auto& r : retired_) {
      if (!r) {
        r = std::move(list);
        return true;
      }
    }
    return false;
  }
  void push_render(const en::proto::RenderFeed& f) noexcept override {
    s_.renders.fetch_add(1, std::memory_order_relaxed);
    if (!f.heads.empty() && f.heads[0].present) s_.render_present.fetch_add(1, std::memory_order_relaxed);
  }

 private:
  Summary& s_;
  std::unique_ptr<en::proto::WorldSnapshot> building_;
  std::array<GameCommand, 16> ring_{};
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
  std::array<std::shared_ptr<const std::vector<std::uint32_t>>, 4> retired_{};
};

// ---- The task, wrapped: the probe's searches and injection, and the allocation watch ----

struct Probe final : TaskBody {
  Probe(GalTask& t, NativeSys& sys, const Discovery& d) : task(t), sys(sys) {
    for (const roles::Cls c : {roles::Cls::SaveData, roles::Cls::GameState, roles::Cls::FieldArea}) {
      searches.push_back(std::make_unique<probe::SlotSearch>(sys, d, c));
    }
  }
  void run(const en::proto::GameThread& gt, const FrameCtx& frame) override {
    const bool warm = task.status().snapshots.load(std::memory_order_relaxed) >= 3;
    t_armed = warm;
    task.run(gt, frame);
    if (task.status().state.load(std::memory_order_relaxed) == TaskState::Running) {
      if (inject_now.load(std::memory_order_acquire) && !injected) {
        injected = true;
        task.backend().reader().inject(injection);
      }
      // The static-slot searches, within what the tick's budget has left: watched for allocations too.
      for (auto& s : searches) {
        if (!s->step(gt, task.backend().reader(), 32)) break;
      }
    }
    t_armed = false;
  }
  GalTask& task;
  NativeSys& sys;
  std::vector<std::unique_ptr<probe::SlotSearch>> searches;
  std::atomic<bool> inject_now{false};
  Injection injection;
  bool injected = false;
};

std::string groups_text(const Discovery& d) {
  std::string out;
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
    if (!out.empty()) out += ",";
    out += en::proto::to_string(static_cast<Group>(g));
    out += ":";
    out += to_string(d.report().groups[g]);
  }
  return out;
}

std::string groups_text(const TaskStatus& st) {
  std::string out;
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
    if (!out.empty()) out += ",";
    out += en::proto::to_string(static_cast<Group>(g));
    out += ":";
    out += en::proto::to_string(st.groups[g].load(std::memory_order_relaxed));
  }
  return out;
}

std::optional<Group> group_named(const std::string& name) {
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
    if (name == en::proto::to_string(static_cast<Group>(g))) return static_cast<Group>(g);
  }
  return std::nullopt;
}

DWORD WINAPI t1(void*) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, DWORD{1}, [] {
    const std::string status_file = env("EN_GALTEST_STATUS");
    if (status_file.size() < sizeof g_status_file) std::memcpy(g_status_file, status_file.c_str(), status_file.size() + 1);
    const std::string dir = env("EN_GALTEST_DIR");
    const bool rpm = env("EN_GALTEST_COPY") == "rpm";
    line("boot pid=%lu copy=%s", GetCurrentProcessId(), rpm ? "rpm" : "seh");
    // NativeSys and its task live for the life of the process (R14): made here, never destroyed.
    auto* sys = new NativeSys(rpm ? CopyKind::Rpm : CopyKind::Seh);
    line("stacks n=%zu", sys->stacks().size());
    const auto builds = read_file(dir + "\\builds.toml");
    const auto patterns = read_file(dir + "\\patterns.toml");
    if (!builds || !patterns) {
      line("error files");
      return DWORD{2};
    }
    // The gate, at once; discovery and readiness wait for the game's statics (ARCH §3.2's WaitEngine).
    const std::uint64_t t0 = sys->now_us();
    const auto gated = gate_image(*sys, *builds);
    if (!gated) {
      line("gate failed what=%d", static_cast<int>(gated.error().what));
      return DWORD{3};
    }
    line("gate ok key=%s", gated->profile.entry().key.c_str());
    const auto layout = read_file(dir + "\\" + gated->profile.entry().layout);
    if (!layout) {
      line("error layout");
      return DWORD{2};
    }
    DiscoveryOptions options;
    options.mode = LayoutMode::Release;
    Retried found = discover_with_retries(*sys, *gated, *patterns, *layout, options, [&](std::uint64_t at_ms) {
      while (sys->now_us() - t0 < at_ms * 1000) Sleep(100);
    });
    if (!found.last) {
      line("discovery refused problems=%zu", found.last.error().size());
      for (const auto& e : found.last.error()) line("problem %s", describe(e).c_str());
      return DWORD{4};
    }
    auto* d = new Discovery(std::move(*found.last));
    const DiscoveryReport& r = d->report();
    std::size_t resolved = 0;
    for (const ItemResult& item : r.items) resolved += item.status == ItemStatus::Resolved ? 1 : 0;
    line("discovery passes=%u ready=%d items=%zu/%zu fd4_hits=%llu fd4_names=%llu getter_calls=%llu rtti_classes=%u "
         "rtti_missing=%u unscanned=%llu unreadable=%llu pass_us=%llu groups=%s",
         found.passes, r.ready ? 1 : 0, resolved, r.items.size(), static_cast<unsigned long long>(r.fd4_hits),
         static_cast<unsigned long long>(r.fd4_names), static_cast<unsigned long long>(r.getter_calls), r.rtti_classes,
         r.rtti_missing, static_cast<unsigned long long>(r.unscanned), static_cast<unsigned long long>(r.image_unreadable),
         static_cast<unsigned long long>(r.pass_us), groups_text(*d).c_str());
    for (std::size_t i = 0; i < r.items.size(); ++i) {
      line("item label=%s status=%s rva=0x%X sites=%u", roles::info(static_cast<roles::Item>(i)).label.data(),
           to_string(r.items[i].status), r.items[i].rva, r.items[i].sites);
    }
    if (!r.ready) {
      line("not-ready failure=%s", r.failure);
      return DWORD{5};
    }
    // Readiness: the task hub's instance non-null (polled every 100 ms for at most 30 s).
    const std::uint64_t hub_rva = *d->address(roles::Item::TaskHub) - d->module_base();
    for (int i = 0; i < 300; ++i) {
      std::array<std::byte, 8> v{};
      std::uint64_t inst = 0;
      if (sys->image().read(hub_rva, v)) std::memcpy(&inst, v.data(), 8);
      if (inst != 0) break;
      Sleep(100);
    }
    static Summary summary;  // constant-initialised atomics; lives for the process
    auto* io = new Io(summary);
    auto* task = new GalTask(*sys, *d, *io);
    auto* body = new Probe(*task, *sys, *d);
    const std::string fault = env("EN_GALTEST_FAULT");
    if (const std::size_t colon = fault.find(':'); colon != std::string::npos) {
      if (const auto g = group_named(fault.substr(0, colon))) {
        body->injection = Injection{*g, static_cast<std::uint32_t>(std::atoi(fault.c_str() + colon + 1)),
                                    rpm ? 0 : sys->no_access_page()};
      }
    }
    const auto registered = install_task(*sys, *d, *body);
    line("registered ok=%d error=%s", registered ? 1 : 0, registered ? "none" : to_string(registered.error()));
    if (!registered) return DWORD{6};
    // The core's first commands: a watch list, look-ups of text and rows.
    io->push_command(GameCommand{GameCommand::SetWatchFlags{
        9, std::make_shared<const std::vector<std::uint32_t>>(std::vector<std::uint32_t>{100, 7005, 4321010, 999000})}});
    GameCommand::ResolveText npc;
    npc.fmg = 7;
    (void)npc.ids.try_push_back(1100);
    (void)npc.ids.try_push_back(1103);
    io->push_command(GameCommand{npc});
    GameCommand::ResolveText place;
    place.fmg = 9;
    (void)place.ids.try_push_back(6100100);
    io->push_command(GameCommand{place});
    GameCommand::ResolveParams rows;
    rows.param = en::proto::ParamKind::NpcParam;
    (void)rows.ids.try_push_back(81100);
    (void)rows.ids.try_push_back(81999);
    io->push_command(GameCommand{rows});

    // Report every 500 ms, for ever (the fake game ends the process).
    const double fault_after = env("EN_GALTEST_FAULT_AFTER").empty() ? 2.0 : std::atof(env("EN_GALTEST_FAULT_AFTER").c_str());
    std::uint64_t world_since = 0;
    bool tracked = false;
    for (;;) {
      Sleep(500);
      const TaskStatus& st = task->status();
      const auto now = sys->now_us();
      if (summary.screen.load() == static_cast<std::int64_t>(en::proto::Screen::InGame)) {
        if (world_since == 0) world_since = now;
      }
      if (!tracked && summary.near_handle.load() != 0) {
        GameCommand::SetTracked t;
        (void)t.handles.try_push_back(en::proto::ChrHandle{static_cast<std::uint64_t>(summary.near_handle.load())});
        tracked = io->push_command(GameCommand{t});
      }
      if (world_since != 0 && body->injection.every != 0 && static_cast<double>(now - world_since) / 1e6 >= fault_after) {
        body->inject_now.store(true, std::memory_order_release);
      }
      std::string slots;
      const char* labels[] = {"save_data", "game_state", "field_area"};
      for (std::size_t i = 0; i < body->searches.size(); ++i) {
        const auto& s = *body->searches[i];
        char b[96];
        std::snprintf(b, sizeof b, "%s%s:%zu/%zu:0x%X", slots.empty() ? "" : ",", labels[i], s.checked(), s.candidates(),
                      s.found().empty() ? 0u : s.found()[0]);
        slots += b;
      }
      const GuardedReader& reader = task->backend().reader();
      line("status state=%s inert_why=%s group_ok=%d group_found_at=%d invocations=%llu snapshots=%llu abandoned=%llu "
           "flag_frames=%llu loading_only=%llu dormant=%llu commands=%llu renders=%llu render_present=%llu "
           "throttled=%d task_p50=%llu task_p99=%llu task_max=%llu build_p99=%llu build_max=%llu copies=%llu "
           "fault_copy=%llu fault_injected=%llu fault_stack=%llu fault_budget=%llu vtable_skips=%llu "
           "slot_calls=%llu,%llu,%llu allocs=%llu frees=%llu groups=%s mode=%s",
           to_string(st.state.load()), to_string(st.inert_why.load()), st.group_ok.load() ? 1 : 0,
           st.group_found_at.load(), static_cast<unsigned long long>(st.invocations.load()),
           static_cast<unsigned long long>(st.snapshots.load()), static_cast<unsigned long long>(st.abandoned.load()),
           static_cast<unsigned long long>(st.flag_frames.load()), static_cast<unsigned long long>(st.loading_only.load()),
           static_cast<unsigned long long>(st.dormant.load()), static_cast<unsigned long long>(st.commands.load()),
           static_cast<unsigned long long>(summary.renders.load()),
           static_cast<unsigned long long>(summary.render_present.load()), st.throttled.load() ? 1 : 0,
           static_cast<unsigned long long>(st.task_us.percentile_us(500)),
           static_cast<unsigned long long>(st.task_us.percentile_us(990)),
           static_cast<unsigned long long>(st.task_us.max_us()),
           static_cast<unsigned long long>(st.build_us.percentile_us(990)),
           static_cast<unsigned long long>(st.build_us.max_us()),
           static_cast<unsigned long long>(reader.total_copies()),
           static_cast<unsigned long long>(reader.faults(FaultKind::Copy)),
           static_cast<unsigned long long>(reader.faults(FaultKind::Injected)),
           static_cast<unsigned long long>(reader.faults(FaultKind::Stack)),
           static_cast<unsigned long long>(reader.faults(FaultKind::Budget)),
           static_cast<unsigned long long>(st.vtable_skips.load()),
           static_cast<unsigned long long>(sys->slot_calls(0)), static_cast<unsigned long long>(sys->slot_calls(1)),
           static_cast<unsigned long long>(sys->slot_calls(2)), static_cast<unsigned long long>(g_news.load()),
           static_cast<unsigned long long>(g_deletes.load()), groups_text(st).c_str(),
           to_string(st.mode.load()));
      line("world screen=%lld lobby=%lld role=%lld player_set=%lld chars=%lld dead=%lld near_param=%lld level=%lld "
           "runes=%lld hp=%lld/%lld slot=%lld play_ms=%lld place=%lld hour=%lld minute=%lld hud=%lld subtitles=%lld "
           "boss_bars=%lld gaps=%lld published=%llu dropped=%llu",
           static_cast<long long>(summary.screen.load()), static_cast<long long>(summary.lobby.load()),
           static_cast<long long>(summary.role.load()), static_cast<long long>(summary.player_set.load()),
           static_cast<long long>(summary.chars.load()), static_cast<long long>(summary.dead_chars.load()),
           static_cast<long long>(summary.near_param.load()), static_cast<long long>(summary.level.load()),
           static_cast<long long>(summary.runes.load()), static_cast<long long>(summary.hp.load()),
           static_cast<long long>(summary.max_hp.load()), static_cast<long long>(summary.slot.load()),
           static_cast<long long>(summary.play_ms.load()), static_cast<long long>(summary.place.load()),
           static_cast<long long>(summary.hour.load()), static_cast<long long>(summary.minute.load()),
           static_cast<long long>(summary.hud.load()), static_cast<long long>(summary.subtitles.load()),
           static_cast<long long>(summary.boss_bars.load()), static_cast<long long>(summary.gap.load()),
           static_cast<unsigned long long>(summary.published.load()),
           static_cast<unsigned long long>(summary.dropped.load()));
      line("stores flag_version=%llu flag_bits=0x%llX flag_valid=0x%llX texts=%llu text_named=%llu text_empty=%llu "
           "params=%llu param_name_id=%llu data_ready=%llu playthroughs=%llu name_len=%llu trips=%llu trip_groups=0x%llX "
           "slots=%s",
           static_cast<unsigned long long>(summary.flag_version.load()),
           static_cast<unsigned long long>(summary.flag_bits.load()),
           static_cast<unsigned long long>(summary.flag_valid.load()),
           static_cast<unsigned long long>(summary.texts.load()), static_cast<unsigned long long>(summary.text_named.load()),
           static_cast<unsigned long long>(summary.text_empty.load()), static_cast<unsigned long long>(summary.params.load()),
           static_cast<unsigned long long>(summary.param_name_id.load()),
           static_cast<unsigned long long>(summary.data_ready.load()),
           static_cast<unsigned long long>(summary.playthroughs.load()),
           static_cast<unsigned long long>(summary.name_len.load()), static_cast<unsigned long long>(summary.trips.load()),
           static_cast<unsigned long long>(summary.trip_groups.load()), slots.c_str());
    }
    return DWORD{0};
  });
}

}  // namespace

// The loader's call (ARCH §3.1): one thread started, nothing else under the loader lock; detach does nothing (R14).
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, FALSE, [&] {
    if (reason == DLL_PROCESS_ATTACH) {
      DisableThreadLibraryCalls(instance);
      HANDLE t = CreateThread(nullptr, 0, t1, nullptr, 0, nullptr);
      if (t != nullptr) CloseHandle(t);
    }
    return TRUE;
  });
}
