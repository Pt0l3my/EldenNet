// T1, the init thread (dll.hpp; ARCH §1.4, §3.2): EldenNet's start, from Pin to Ready or to a banner, then T1 ends.
// Each state is logged and written to the watch block for the launcher's dev console, and a terminal one names how
// long the start took.
//
//   Pin (and R16's mutex) → Gate → Safety (the anti-cheat check; the log starts here) → Settings (enabled = false: Exit)
//   → T6, T10 and T11 → WaitWindow (≤ 120 s from T1's start) → Connect (GraceUI, overlay.cpp: ≤ 30 s, at once when
//   none is installed; a failed connect goes on headless)
//   → Discover (the AOB items, the FD4 singletons, the RTTI sets; retried at +5, +15 and +30 s)
//   → WaitEngine (the task hub's instance non-null, ≤ 120 s) → the crash filter → Register (the one call)
//   → FirstTick (≤ 30 s: the group check and phase A on the game thread) → Ready (R16's marker scan) → end.
//   An unknown build, or anti-cheat, takes the banner path: WaitWindow, the crash filter, ConnectB (GraceUI's notice-only
//   mode with anti-cheat), the banner, end.
//   A probe build with probe.round = "r1" runs R1 after WaitWindow instead (probe_r1.cpp).
#include <cstring>

#include "dll.hpp"
#include "en/gal/discovery.hpp"
#include "en/gal/sys_boot.hpp"
#include "en/proto/ffi_guard.hpp"
#include "en/shell/names.hpp"

#if EN_PROBE
#include "en/gal/probe.hpp"
#endif

namespace eldennet {

namespace w = en::proto::watch;
using en::core::BannerReason;
using en::shell::Line;
using en::shell::LogLevel;

#if EN_PROBE
// The probe build's allocation watch (R6, ARCH §10.1): alloc_count.cpp's counting operator new, armed on the game thread.
void arm_allocation_watch(bool armed) noexcept;
std::uint64_t allocations_watched() noexcept;
#endif

namespace {

constexpr std::uint64_t kStartLimitMs = 120'000;  // WaitWindow and WaitEngine, from T1's start
constexpr std::uint64_t kFirstTickMs = 30'000;

void enter(w::InitState s) {
  block::state(s);
  log(Line("init").word("state", w::to_string(s)));
}

void finish(w::InitState s) {
  block::ready_ms(static_cast<std::uint32_t>(since_ms()));
  enter(s);
}

// The task's body on the game thread: the thread named for the crash filter, and in a probe build the allocation
// watch and S0-D's fault injection (part 4's probe option), then the task.
class Body final : public en::gal::TaskBody {
 public:
  Body(en::gal::GalTask& task, Shell& s) : task_(task), s_(s) {}

  void run(const en::proto::GameThread& gt, const en::gal::FrameCtx& frame) override {
    if (!named_) {
      named_ = true;
      block::game_thread(GetCurrentThreadId());
    }
#if EN_PROBE
    const auto& st = task_.status();
    arm_allocation_watch(st.snapshots.load(std::memory_order_relaxed) >= 3);
    const en::shell::ProbeSettings& p = s_.settings.probe;
    if (p.fault_every != 0 && !injected_ && st.state.load(std::memory_order_relaxed) == en::gal::TaskState::Running) {
      if (task_.backend().last_mode() == en::gal::ReadMode::Full && world_since_us_ == 0) world_since_us_ = frame.qpc_now_us;
      if (world_since_us_ != 0 && frame.qpc_now_us - world_since_us_ >= std::uint64_t{p.fault_after_s} * 1'000'000) {
        injected_ = true;
        const std::uint64_t at = s_.settings.copy == en::shell::CopyMode::Rpm ? 0 : s_.sys->no_access_page();
        task_.backend().reader().inject(en::gal::Injection{p.fault_group, p.fault_every, at});
      }
    }
#endif
    task_.run(gt, frame);
#if EN_PROBE
    arm_allocation_watch(false);
#endif
  }

 private:
  en::gal::GalTask& task_;
  [[maybe_unused]] Shell& s_;  // the probe build's settings
  bool named_ = false;
#if EN_PROBE
  bool injected_ = false;
  std::uint64_t world_since_us_ = 0;
#endif
};

std::optional<std::string> gal_text(const Shell& s, const std::string& name) {
  std::wstring wide(name.begin(), name.end());  // gal/'s names are ASCII (builds.toml's layout path is checked)
  for (wchar_t& c : wide) {
    if (c == L'/') c = L'\\';
  }
  return read_text(s.paths.gal + L"\\" + wide, 256u << 10);
}

bool wait_window(std::uint64_t limit_ms) {
  while (!main_window()) {
    if (since_ms() >= limit_ms) return false;
    Sleep(100);
  }
  return true;
}

// The banner path (ARCH §3.2's WaitWindow_B and ConnectB): the window waited for, the crash filter, the banner, end.
void banner_path(Shell& s, BannerReason reason, const char* detail) {
  enter(w::InitState::WaitWindow);
  const bool window = wait_window(kStartLimitMs);
  log(Line("window").flag("found", window).num("ms", since_ms()));
  if (window) block::install_filter();
  connect_overlay(s, true);  // ConnectB: the banner reaches GraceUI's notice through T6 (overlay.cpp)
  log(Line("banner").word("reason", en::core::to_string(reason)).word("detail", detail), LogLevel::Warn);
  push_banner(s, reason, detail);
  finish(w::InitState::Banner);
}

void stop_at(Shell& s, BannerReason reason, const char* detail) {
  log(Line("banner").word("reason", en::core::to_string(reason)).word("detail", detail), LogLevel::Warn);
  push_banner(s, reason, detail);
  finish(w::InitState::Banner);
}

void run(HMODULE module) {
  set_start();
  // ---- Pin, and R16's mutex (ARCH §1.6), taken here rather than in Safety so that a second copy loaded a moment
  // later can never overtake the first while it gates ----
  (void)en::gal::sys::boot::pin(module);
  const std::wstring mutex_name = L"Local\\EldenNet.Instance." + std::to_wstring(GetCurrentProcessId());
  HANDLE mutex = CreateMutexW(nullptr, FALSE, mutex_name.c_str());
  if (mutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
    if (mutex != nullptr) CloseHandle(mutex);
    OutputDebugStringW(L"EldenNet: a second copy was loaded into this process; it stays idle\n");
    return;  // no log, no block, no thread, no task, no gate (R16)
  }
  // The first copy holds the mutex for the life of the process and never closes it.
  block::init_thread(GetCurrentThreadId());
  Shell& s = make_shell();
  s.module = module;
  s.paths = find_paths(module);
  log(Line("boot").num("pid", GetCurrentProcessId()).word("dll", word_of(en::shell::file_name_of(s.paths.dll)))
          .flag("probe_build", kProbeBuild).num("version_minor", kVersion[1]).word("version", kVersionLabel));

  // ---- Gate (ARCH §3.2): the mapped headers against gal/builds.toml; the result waits for Safety ----
  s.sys = new en::gal::NativeSys(en::gal::CopyKind::Seh);  // lives for the process (R14)
  s.sys->note_own_stack();
  const std::optional<std::string> builds = gal_text(s, "builds.toml");
  auto* gated = new en::proto::Expected<en::gal::Gated, en::gal::GateFailure>(
      builds ? en::gal::gate_image(*s.sys, *builds)
             : en::proto::Expected<en::gal::Gated, en::gal::GateFailure>(en::proto::Unexpected{en::gal::GateFailure{
                   en::gal::GateFailure::What::BuildsFile, "no builds.toml"}}));
  const char* gate_word = "passed";
  if (!*gated) {
    switch (gated->error().what) {
      case en::gal::GateFailure::What::Image: gate_word = "image"; break;
      case en::gal::GateFailure::What::BuildsFile: gate_word = builds ? "builds_file" : "no_builds_file"; break;
      case en::gal::GateFailure::What::Gate: gate_word = "unknown_build"; break;
    }
  } else {
    const en::gal::BuildEntry& e = (*gated)->profile.entry();
    s.game_version = e.file_version;
    s.game_version_known = true;
    s.region = e.region == en::gal::Region::Ww ? "ww" : "jp";
  }
  if (!s.game_version_known) {
    // A build the gate refused: its file version all the same, for the banner's "game build 2.x.y.z" (ARCH §10.3).
    if (const auto image = en::gal::read_image(s.sys->image(), en::gal::ImageLayout::Memory)) {
      if (const auto fp = en::gal::fingerprint(*image)) {
        s.game_version = fp->file_version;
        s.game_version_known = true;
      }
    }
  }
  log(Line("gate").word("result", gate_word).num("ms", since_ms()));

  // ---- Safety: the anti-cheat check (ARCH §5.3); R16's mutex was taken at Pin ----
  bool anticheat = false;
  for (const std::wstring& m : module_names()) anticheat = anticheat || en::shell::anticheat_module(m);
  const std::wstring parent = parent_name();
  const bool protected_parent = en::shell::protected_launcher(parent);
  anticheat = anticheat || protected_parent;
  s.anticheat.store(anticheat, std::memory_order_relaxed);
  const bool logging = !s.paths.data.empty() && start_logger(s.paths.data + L"\\logs");
  bool watched = block::attach();
  bool watcher_started = false;
  if (!watched) {
    watcher_started = block::start_watcher(s.paths);  // another loader: EldenNet starts the watcher (O71)
    watched = watcher_started && block::attach();
  }
  enter(w::InitState::Pin);
  enter(w::InitState::Gate);
  enter(w::InitState::Safety);
  log(Line("safety").flag("first_copy", true).flag("anticheat", anticheat).flag("protected_parent", protected_parent)
          .flag("logging", logging).flag("watched", watched).flag("watcher_started", watcher_started)
          .word("spike", w::to_string(block::spike())));

  // ---- Settings (ARCH §11.5) ----
  enter(w::InitState::Settings);
  const std::optional<std::string> text = read_text(s.paths.data + L"\\config\\settings.toml",
                                                    en::shell::kMaxSettingsBytes);
  const en::shell::LoadedSettings loaded = en::shell::parse_settings(text.value_or(""), kProbeBuild);
  s.settings = loaded.settings;
  set_log_level(s.settings.log_level);
  log(Line("settings").flag("file", text.has_value()).flag("refused", loaded.refused).flag("enabled", s.settings.enabled)
          .flag("record", s.settings.record_sessions).word("copy", en::shell::to_string(s.settings.copy))
          .word("log_level", en::shell::to_string(s.settings.log_level))
          .word("probe", en::shell::to_string(s.settings.probe.round)));
  for (const en::shell::SettingsProblem& p : loaded.problems) {
    log(Line("settings_problem").word("where", p.where.empty() ? "file" : p.where), LogLevel::Warn);
  }
  // The launcher's --probe (O89): R1, in a probe build only, with the file's other probe settings, and always at the
  // Mark (the owner's Pause at the title) unless the file says never. A release build, or EldenNet turned off, refuses.
  if (block::probe_request() == w::ProbeRequest::R1) {
    const bool accept = kProbeBuild && s.settings.enabled;
    if (accept) {
      s.settings.probe.round = en::shell::ProbeRound::R1;
      if (s.settings.probe.register_when == en::shell::ProbeRegister::Auto) {
        s.settings.probe.register_when = en::shell::ProbeRegister::Mark;
      }
    }
    block::probe_answer(accept ? w::ProbeRequest::Accepted : w::ProbeRequest::Refused);
    log(Line("probe").word("asked", "r1").flag("accepted", accept).flag("probe_build", kProbeBuild)
            .flag("enabled", s.settings.enabled),
        accept ? LogLevel::Info : LogLevel::Warn);
    if (!accept) {
      finish(w::InitState::Exit);  // the owner asked for R1 alone: no other start, and nothing called
      return;
    }
  }
  if (!s.settings.enabled) {
    finish(w::InitState::Exit);  // Off: nothing more this session (ARCH §1.5)
    return;
  }
  s.sys->set_copy_kind(s.settings.copy == en::shell::CopyMode::Rpm ? en::gal::CopyKind::Rpm : en::gal::CopyKind::Seh);
  s.io = make_io(s);
  const bool core = start_core(s);
  const bool watcher = start_watcher_thread(s);
  log(Line("threads").flag("core", core).flag("watcher", watcher).flag("logger", logging));

  if (!*gated) return banner_path(s, BannerReason::UnknownBuild, gate_word);
  if (anticheat) return banner_path(s, BannerReason::AntiCheat, protected_parent ? "parent" : "module");

  // ---- WaitWindow, Connect ----
  enter(w::InitState::WaitWindow);
  if (!wait_window(kStartLimitMs)) return stop_at(s, BannerReason::NotStarted, "no_window");
  log(Line("window").flag("found", true).num("ms", since_ms()));
  enter(w::InitState::Connect);
  connect_overlay(s, false);

#if EN_PROBE
  if (s.settings.probe.round == en::shell::ProbeRound::R1) {
    run_r1(s, gated);
    return;
  }
#endif

  // ---- Discover (ARCH §3.2's ReadyScan and Discover; the retries at +5, +15 and +30 s) ----
  enter(w::InitState::Discover);
  const en::gal::Gated& g = **gated;
  const std::optional<std::string> patterns = gal_text(s, "patterns.toml");
  const std::optional<std::string> layout = gal_text(s, g.profile.entry().layout);
  if (!patterns || !layout) return stop_at(s, BannerReason::DiscoveryFailed, "files");
  en::gal::DiscoveryOptions options;
  options.mode = kProbeBuild ? en::gal::LayoutMode::Dev : en::gal::LayoutMode::Release;
  const std::uint64_t pass0 = since_ms();
  en::gal::Retried found = en::gal::discover_with_retries(*s.sys, g, *patterns, *layout, options,
                                                          [&](std::uint64_t at_ms) {
                                                            while (since_ms() - pass0 < at_ms) Sleep(100);
                                                          });
  if (!found.last) {
    log(Line("discovery").flag("refused", true).num("problems", found.last.error().size()), LogLevel::Error);
    return stop_at(s, BannerReason::DiscoveryFailed, "gal_files");
  }
  auto* d = new en::gal::Discovery(std::move(*found.last));  // lives for the process
  const en::gal::DiscoveryReport& r = d->report();
  std::uint32_t resolved = 0;
  for (const en::gal::ItemResult& item : r.items) resolved += item.status == en::gal::ItemStatus::Resolved ? 1 : 0;
  block::items(resolved, static_cast<std::uint32_t>(r.items.size()), found.passes);
  log(Line("discovery").num("passes", found.passes).flag("ready", r.ready).num("items", resolved)
          .num("of", r.items.size()).num("fd4_names", r.fd4_names).num("rtti_classes", r.rtti_classes)
          .num("pass_us", r.pass_us).num("ms", since_ms()));
  if (!r.ready) return stop_at(s, BannerReason::DiscoveryFailed, r.failure);

  // ---- WaitEngine: the task hub's instance non-null (ARCH §3.2), ≤ 120 s from T1's start ----
  enter(w::InitState::WaitEngine);
  const std::uint64_t hub_rva = *d->address(en::gal::roles::Item::TaskHub) - d->module_base();
  bool engine = false;
  while (!engine) {
    std::array<std::byte, 8> v{};
    std::uint64_t inst = 0;
    if (s.sys->image().read(hub_rva, v)) std::memcpy(&inst, v.data(), 8);
    engine = inst != 0;
    if (!engine && since_ms() >= kStartLimitMs) return stop_at(s, BannerReason::NotStarted, "engine");
    if (!engine) Sleep(100);
  }
  log(Line("engine").flag("ready", true).num("ms", since_ms()));
  block::install_filter();  // the game has started: its C runtime's filter is in, ours goes over it (WHY§172)

  // ---- Register: the one call into the game ----
  enter(w::InitState::Register);
  // The stack table made again, without T1's own stack: threads that ended since the first scan (the launcher's
  // loading thread), and T1 itself once it ends at Ready, leave memory the game reuses for its own objects, which a
  // stale range would refuse as a stack (WHY§190).
  const std::size_t stacks = s.sys->rescan_stacks();
  auto* task = new en::gal::GalTask(*s.sys, *d, task_io(*s.io));  // lives for the process
  auto* body = new Body(*task, s);
  const auto registered = en::gal::install_task(*s.sys, *d, *body);
  log(Line("register").flag("ok", registered.has_value()).num("stacks", stacks)
          .word("error", registered ? "none" : en::gal::to_string(registered.error())));
  if (!registered) return stop_at(s, BannerReason::RegisterFaulted, en::gal::to_string(registered.error()));
  s.task = task;
  s.task_set.store(true, std::memory_order_release);

  // ---- FirstTick (≤ 30 s): the group check and phase A on the game thread ----
  enter(w::InitState::FirstTick);
  const std::uint64_t asked = since_ms();
  for (;;) {
    const en::gal::TaskStatus& st = task->status();
    const en::gal::TaskState ts = st.state.load(std::memory_order_acquire);
    if (ts == en::gal::TaskState::Inert) {
      log(Line("first_tick").word("state", "inert").word("why", en::gal::to_string(st.inert_why.load()))
              .sig("group_found_at", st.group_found_at.load()), LogLevel::Error);
      push_banner(s, BannerReason::Inert, en::gal::to_string(st.inert_why.load()));
      finish(w::InitState::Inert);
      return;
    }
    if (ts == en::gal::TaskState::Running && st.snapshots.load(std::memory_order_acquire) >= 1) break;
    if (since_ms() - asked >= kFirstTickMs) {
      log(Line("first_tick").word("state", en::gal::to_string(ts)).word("why", "no_tick"), LogLevel::Error);
      task->backend().make_inert();
      push_banner(s, BannerReason::Inert, "no_tick");
      finish(w::InitState::Inert);
      return;
    }
    Sleep(50);
  }
  log(Line("first_tick").word("state", "running").num("ms", since_ms() - asked));

  // ---- Ready (DataReady went to the core on C2 from the task) ----
  finish(w::InitState::Ready);
  const std::size_t others = other_copies(module);
  if (others != 0) log(Line("second_copy").num("idle", others), LogLevel::Warn);  // a toast with GraceUI (§1.6)
}

}  // namespace

unsigned long init_main(void* module) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, 1ul, [&] {
    run(static_cast<HMODULE>(module));
    return 0ul;
  });
}

}  // namespace eldennet
