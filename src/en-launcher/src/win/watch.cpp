// The crash watcher (O71) and the development console with the Mark (O90): the watch block's mapping and events, and
// the loop that watches the game until it ends (win.hpp).
#include <algorithm>
#include <new>
#include <string>

#include "en/launcher/appid.hpp"
#include "win.hpp"

namespace en::launcher::win {

namespace w = proto::watch;

namespace {

constexpr int kMarkHotkey = 0x454E;  // the Mark's hotkey id ("EN")

std::wstring object_name(std::wstring_view prefix, DWORD pid) { return std::wstring(prefix) + std::to_wstring(pid); }

}  // namespace

std::optional<WatchBlock> WatchBlock::open(DWORD pid) {
  WatchBlock wb;
  wb.mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(w::kBlockSize),
                                   object_name(w::kBlockPrefix, pid).c_str());
  if (wb.mapping_ == nullptr) return std::nullopt;
  wb.created_ = GetLastError() != ERROR_ALREADY_EXISTS;
  void* view = MapViewOfFile(wb.mapping_, FILE_MAP_ALL_ACCESS, 0, 0, w::kBlockSize);
  if (view == nullptr) return std::nullopt;
  if (wb.created_) {
    wb.block_ = ::new (view) w::Block();  // the creator constructs it in the zeroed page
    w::initialise(*wb.block_, pid);
  } else {
    // SAFETY: EldenNet created the block in this page (watch.hpp); its magic, version and size are checked first.
    wb.block_ = static_cast<w::Block*>(view);
    for (int i = 0; i < 50 && !w::valid(*wb.block_); ++i) Sleep(20);
    if (!w::valid(*wb.block_)) {
      UnmapViewOfFile(view);
      wb.block_ = nullptr;
      return std::nullopt;
    }
  }
  wb.crash_ = CreateEventW(nullptr, FALSE, FALSE, object_name(w::kCrashEventPrefix, pid).c_str());
  wb.ack_ = CreateEventW(nullptr, FALSE, FALSE, object_name(w::kAckEventPrefix, pid).c_str());
  if (wb.crash_ == nullptr || wb.ack_ == nullptr) return std::nullopt;
  return std::optional<WatchBlock>(std::move(wb));
}

WatchBlock::WatchBlock(WatchBlock&& o) noexcept
    : mapping_(std::exchange(o.mapping_, nullptr)),
      crash_(std::exchange(o.crash_, nullptr)),
      ack_(std::exchange(o.ack_, nullptr)),
      block_(std::exchange(o.block_, nullptr)),
      created_(o.created_) {}

WatchBlock::~WatchBlock() {
  if (block_ != nullptr) UnmapViewOfFile(block_);
  for (HANDLE h : {crash_, ack_, mapping_}) {
    if (h != nullptr) CloseHandle(h);
  }
}

void WatchBlock::ack_crash() const {
  block_->crash.state.store(static_cast<std::uint32_t>(w::CrashState::Taken), std::memory_order_release);
  SetEvent(ack_);
}

void close_game(HANDLE process, DWORD pid, DWORD grace_ms) {
  if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) return;
  if (close_windows(pid) > 0 && WaitForSingleObject(process, grace_ms) == WAIT_OBJECT_0) return;
  TerminateProcess(process, kClosedByLauncher);
  WaitForSingleObject(process, 10000);
}

WatchResult watch_game(HANDLE process, DWORD pid, const WatchBlock& wb, const WatchPlan& plan) {
  WatchResult r;
  w::Block& b = wb.block();
  b.control.spike.store(static_cast<std::uint32_t>(plan.spike), std::memory_order_relaxed);
  b.control.watcher_pid.store(GetCurrentProcessId(), std::memory_order_release);
  bool hotkey = false;
  if (plan.console) {
    hotkey = RegisterHotKey(nullptr, kMarkHotkey, MOD_NOREPEAT, VK_PAUSE) != 0;
    say("mark hotkey=pause registered=%d", hotkey ? 1 : 0);
  }
  const std::uint64_t t0 = mono_ms();
  std::uint64_t modules_at = 0;
  std::uint64_t last_print = 0;
  std::uint64_t last_connections = 0;
  bool window_seen = false;
  bool appid_done = !plan.remove_appid || *plan.remove_appid != RemoveAfter::Window || !plan.appid;
  bool crash_requested = false;
  std::uint64_t crash_deadline = 0;
  bool closing = false;
  DllStatus printed;
  w::InitState seen = w::InitState::None;
  // R1's steps (--probe), each told once as it changes.
  std::optional<R1Step> r1_said;
  bool r1_pressed = false;
  std::uint64_t first_tick_at = 0;
  const auto r1_tell = [&](R1Step step) {
    r1_said = step;
    r1_pressed = r1_pressed || step == R1Step::PressMark;
    say("r1 step=%s", r1_word(step));
    console_line(r1_text(step));
  };
  if (plan.probe) r1_tell(R1Step::Asked);
  const auto take_modules = [&](bool at_crash) {
    auto list = list_modules(pid);
    if (list.empty()) return;
    r.record.modules = std::move(list);
    r.record.modules_at_crash = at_crash;
    modules_at = mono_ms();
  };
  const auto close_now = [&](const char* why) {
    say("close why=%s", why);
    closing = true;
    r.closed_by_us = true;
    close_game(process, pid, 10000);
  };

  for (;;) {
    const HANDLE handles[2] = {process, wb.crash_event()};
    const DWORD woke = MsgWaitForMultipleObjects(2, handles, FALSE, 200, QS_ALLINPUT);
    if (woke == WAIT_OBJECT_0) break;
    const std::uint64_t now = mono_ms();
    if (woke == WAIT_OBJECT_0 + 1) {
      // EldenNet's crash filter wrote its record and waits (at most 2 s) while the modules are listed.
      r.crash_signalled = true;
      take_modules(true);
      if (const auto c = copy_crash(b)) {
        say("crash code=0x%08X thread=%s module=%s offset=0x%llX chained=%d", c->code, w::to_string(c->thread),
            c->module.c_str(), static_cast<unsigned long long>(c->offset), c->chained ? 1 : 0);
      }
      wb.ack_crash();
    }
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_HOTKEY && msg.wParam == kMarkHotkey) {
        const std::uint32_t n = w::add_mark(b.marks, now);
        r.record.marks.push_back(now);
        say("mark n=%u mono_ms=%llu", n, static_cast<unsigned long long>(now));
      }
    }
    if (!window_seen && main_window(pid) != nullptr) {
      window_seen = true;
      say("window after_ms=%llu", static_cast<unsigned long long>(now - t0));
      take_modules(false);
    }
    if (!appid_done && (window_seen || now - t0 >= 120000)) {
      appid_done = true;
      say("appid removal=%s at=window", to_string(remove_if_ours(*plan.appid, plan.appid_value)));
    }
    if (!r.crash_signalled && now >= modules_at && now - modules_at >= 5000) take_modules(false);
    if (plan.count_connections && now - last_connections >= 2000) {
      last_connections = now;
      if (const auto n = remote_connections(pid)) r.record.connections_max = std::max(r.record.connections_max, *n);
    }
    const DllStatus st = copy_status(b);
    if (st.init != seen) {
      seen = st.init;
      say("init state=%s after_ms=%llu", w::to_string(st.init), static_cast<unsigned long long>(now - t0));
    }
    if (plan.probe) {
      if (st.init != w::InitState::FirstTick) {
        first_tick_at = 0;
      } else if (first_tick_at == 0) {
        first_tick_at = now;
      }
      const auto answer = static_cast<w::ProbeRequest>(b.control.probe.load(std::memory_order_acquire));
      const auto step = r1_step(answer, st.init, r1_pressed, first_tick_at == 0 ? 0 : now - first_tick_at);
      if (step && step != r1_said) r1_tell(*step);
    }
    if (plan.console && now - last_print >= 1000 &&
        (st.snapshots != printed.snapshots || st.init != printed.init || st.flags != printed.flags ||
         st.faults != printed.faults || now - last_print >= 5000)) {
      last_print = now;
      printed = st;
      say("status init=%s phase=%u level=%u banner=%u flags=0x%X items=%u/%u passes=%u snapshots=%llu faults=%llu "
          "tripped=0x%llX task_p99_us=%u marks=%u",
          w::to_string(st.init), st.phase, st.level, st.banner, st.flags, st.items_resolved, st.items_total,
          st.discovery_passes, static_cast<unsigned long long>(st.snapshots),
          static_cast<unsigned long long>(st.faults), static_cast<unsigned long long>(st.tripped), st.task_us_p99,
          b.marks.count.load(std::memory_order_relaxed));
    }
    if (plan.run_timeout_s > 0 && !closing) {
      if (crash_requested) {
        if (now > crash_deadline) close_now("no_crash");
      } else if (w::terminal(st.init)) {
        if (plan.crash_when_ready && st.init == w::InitState::Ready) {
          b.control.crash_request.store(1, std::memory_order_release);
          crash_requested = true;
          crash_deadline = now + 30000;
          say("crash request=1");
        } else {
          close_now("reported");
        }
      } else if (now - t0 >= std::uint64_t{plan.run_timeout_s} * 1000) {
        r.timed_out = true;
        close_now("timeout");
      }
    }
  }
  const std::uint64_t end = mono_ms();
  DWORD code = 0;
  GetExitCodeProcess(process, &code);
  r.exit_code = code;
  r.ms = end - t0;
  if (hotkey) UnregisterHotKey(nullptr, kMarkHotkey);
  b.control.watcher_pid.store(0, std::memory_order_release);
  r.last_init = copy_status(b).init;
  if (!r.crash_signalled && copy_crash(b)) r.crash_signalled = true;  // written, but the event came too late
  r.record.crash = copy_crash(b);
  r.record.status = copy_status(b);
  r.record.dll_attached = r.record.status.init != w::InitState::None;
  r.record.exit_code = code;
  r.record.kind = classify_end(code, r.crash_signalled);
  r.record.session_ms = r.ms;
  r.record.modules_age_ms = modules_at == 0 ? r.ms : end - modules_at;
  if (!appid_done) say("appid removal=%s at=end", to_string(remove_if_ours(*plan.appid, plan.appid_value)));
  if (plan.remove_appid && *plan.remove_appid == RemoveAfter::Exit && plan.appid) {
    say("appid removal=%s at=exit", to_string(remove_if_ours(*plan.appid, plan.appid_value)));
  }
  say("end exit_code=0x%08X kind=%s crash_filter=%d closed=%d timed_out=%d init=%s modules=%zu at_crash=%d marks=%zu "
      "session_ms=%llu connections_max=%u",
      code, to_string(r.record.kind), r.record.crash ? 1 : 0, r.closed_by_us ? 1 : 0, r.timed_out ? 1 : 0,
      w::to_string(r.last_init), r.record.modules.size(), r.record.modules_at_crash ? 1 : 0, r.record.marks.size(),
      static_cast<unsigned long long>(r.ms), r.record.connections_max);
  return r;
}

}  // namespace en::launcher::win
