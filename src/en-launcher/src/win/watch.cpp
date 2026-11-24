// The crash watcher (O71) and the development console with the Mark (O90): the watch block's mapping and events, and
// the loop that watches the game until it ends (win.hpp).
#include <algorithm>
#include <new>
#include <string>

#include <fstream>

#include "en/launcher/appid.hpp"
#include "en/launcher/observer_tasks.hpp"
#include "en/launcher/s0.hpp"
#include "win.hpp"

namespace en::launcher::win {

namespace w = proto::watch;

namespace {

constexpr int kMarkHotkey = 0x454E;  // the Mark's hotkey id ("EN")

std::wstring object_name(std::wstring_view prefix, DWORD pid) { return std::wstring(prefix) + std::to_wstring(pid); }

// eldennet.dll's report of an S0 run, from the run's folder (s0.hpp), or nothing.
std::optional<S0Facts> read_report(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::string text(kMaxSpikeReportBytes + 1, '\0');
  in.read(text.data(), static_cast<std::streamsize>(text.size()));
  text.resize(static_cast<std::size_t>(in.gcount()));
  return read_s0_facts(text);
}

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
  bool ring = false;  // the launcher stamps the Marks: by its hotkey, and in the read-only round by the tasks
  if (plan.console) {
    hotkey = RegisterHotKey(nullptr, kMarkHotkey, MOD_NOREPEAT, VK_PAUSE) != 0;
    say("mark hotkey=pause registered=%d", hotkey ? 1 : 0);
    if (hotkey || plan.read) {
      w::launcher_holds(b.marks);
      ring = true;
      if (w::mark_source(b.marks) != w::MarkSource::Launcher) {
        // EldenNet's own Mark took the ring first (GraceUI's hotkey on Pause): the key is its.
        if (hotkey) UnregisterHotKey(nullptr, kMarkHotkey);
        hotkey = false;
        ring = false;
        say("mark hotkey=pause released=1 source=%s", w::to_string(w::mark_source(b.marks)));
      }
    }
  }
  std::uint32_t marks_seen = b.marks.count.load(std::memory_order_acquire);
  const auto own_mark = [&](std::uint64_t at) {
    const std::uint32_t n = w::add_mark(b.marks, at);
    marks_seen = n;
    r.record.marks.push_back(at);
    say("mark n=%u mono_ms=%llu", n, static_cast<unsigned long long>(at));
  };
  const std::uint64_t t0 = mono_ms();
  std::uint64_t modules_at = 0;
  std::uint64_t last_print = 0;
  std::uint64_t last_connections = 0;
  bool window_seen = false;
  std::uint64_t window_last = 0;  // the last poll that found the game's window (O115)
  bool appid_done = !plan.remove_appid || *plan.remove_appid != RemoveAfter::Window || !plan.appid;
  bool crash_requested = false;
  std::uint64_t crash_deadline = 0;
  bool closing = false;
  DllStatus printed;
  w::InitState seen = w::InitState::None;
  // The read-only round's steps (--read), each told once as it changes.
  std::optional<ReadStep> read_said;
  const auto read_tell = [&](ReadStep step, std::uint32_t named, std::uint32_t of) {
    read_said = step;
    say("read step=%s named=%u of=%u", read_word(step), named, of);
    console_line(read_text(step, named, of), "EldenNet read: ");
  };
  if (plan.read) read_tell(ReadStep::Asked, 0, 0);
  // The tester's tasks (--read; an S0 run's, s0.hpp; a run of GraceUI's observer, observer_tasks.hpp): the list, its
  // panel beside the game, and a line for each task as it is done. The Pause key (or a click on the panel's button)
  // confirms the task under way; a marked task's confirmation stamps a Mark, which the round reads and a spike's files
  // keep. Once the ring is EldenNet's (its own Mark on Pause, a GraceUI hotkey), the launcher has no key: in a spike's
  // run each new Mark in the ring confirms the task under way instead, and the launcher stamps none.
  const bool s0 = s0_run(plan.spike) && plan.s0_report.has_value();
  const bool observer = w::observer_run(plan.spike) && plan.run_dir.has_value();
  const std::string who = plan.read ? std::string("EldenNet read: ")
                          : s0      ? s0_who(plan.spike)
                                    : observer_who(plan.spike);
  std::optional<TaskList> tasks;
  TaskPanel panel;
  bool panel_placed = false;
  std::string panel_footer;
  const auto tell_task = [&](std::uint64_t at) {
    if (tasks->finished()) return;
    const TaskRow row = tasks->rows(at)[tasks->current()];
    console_line("Task " + std::to_string(tasks->current() + 1) + " of " + std::to_string(tasks->size()) + ": " +
                     row.title + ". " + row.detail,
                 who);
  };
  const auto task_done = [&](const TaskList::Change& c, const char* by, std::uint64_t at) {
    if (!c.changed) return;
    say("task n=%zu of=%zu by=%s mark=%d", c.task + 1, tasks->size(), by, c.mark ? 1 : 0);
    if (c.mark && ring) own_mark(at);
    tell_task(at);
  };
  if (plan.read) {
    tasks.emplace(read_tasks(plan.read_plan));
    tasks->set_pace(plan.read_pace_s);
    tasks->start(mono_ms());
    const bool opened = panel.open(nullptr);
    say("tasks count=%zu panel=%d pace_s=%u plan=%u", tasks->size(), opened ? 1 : 0, plan.read_pace_s, plan.read_plan);
  } else if (s0 || observer) {
    // The S0 kind's own list (s0.hpp), or the observer's runs' (observer_tasks.hpp); Lodestar's read round keeps its
    // list.
    if (s0) console_line(s0_begin_text(plan.spike, plan.run, plan.runs), who);
    tasks.emplace(s0 ? s0_tasks(plan.spike, plan.run, plan.runs) : observer_tasks(plan.spike));
    tasks->set_pace(plan.spike_pace_s);
    tasks->start(mono_ms());
    const bool opened = panel.open(nullptr);
    say("tasks count=%zu panel=%d pace_s=%u spike=%s run=%u of=%u", tasks->size(), opened ? 1 : 0, plan.spike_pace_s,
        w::to_string(plan.spike), plan.run, plan.runs);
    tell_task(mono_ms());
  }
  r.panel_shown = panel.is_open();
  bool s0_stopped = false;  // T1's start has stopped, and eldennet.dll's report of it was looked for
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
      const bool key = msg.message == WM_HOTKEY && msg.wParam == kMarkHotkey && hotkey;
      const bool click = msg.message == kPanelConfirm && msg.hwnd == nullptr;
      if (tasks && (key || click)) {
        // The tester's confirmation: it does something only for a task that is theirs to confirm.
        const TaskList::Change c = tasks->confirm(now);
        if (c.changed) {
          task_done(c, key ? "key" : "click", now);
        } else {
          say("task key=1 confirmed=0 current=%zu", tasks->current() + 1);
        }
      } else if (key) {
        own_mark(now);
      } else if (msg.hwnd != nullptr) {
        DispatchMessageW(&msg);  // the panel's own messages
      }
    }
    if (tasks) {
      task_done(tasks->tick(now), "time", now);
      if (!panel_placed && panel.is_open()) {
        // Once the game's window shows, the panel moves to its monitor.
        if (const HWND game = main_window(pid)) {
          panel_placed = true;
          panel.close();
          r.panel_shown = panel.open(game) || r.panel_shown;
        }
      }
      panel.show(tasks->rows(now), panel_footer, hotkey);
    }
    // EldenNet's own Mark, GraceUI's hotkey on Pause (part 12), asks for the ring: the key goes, the presses already
    // queued are the launcher's last, and the ring is handed over (watch.hpp's MarkSource). From then on EldenNet alone
    // stamps Marks, and the session's record takes them from the ring.
    if (w::mark_source(b.marks) == w::MarkSource::Asked) {
      if (hotkey) {
        UnregisterHotKey(nullptr, kMarkHotkey);
        hotkey = false;
        while (PeekMessageW(&msg, nullptr, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {
          if (msg.wParam != kMarkHotkey) continue;
          if (tasks && !plan.read) {
            // A spike's tasks: the press confirms the task under way, as any press of the key does.
            const TaskList::Change c = tasks->confirm(now);
            if (c.changed) {
              task_done(c, "key", now);
            } else {
              say("task key=1 confirmed=0 current=%zu", tasks->current() + 1);
            }
          } else {
            own_mark(now);
          }
        }
      }
      w::launcher_hands_over(b.marks);
      ring = false;
      say("mark hotkey=pause released=1 source=%s", w::to_string(w::mark_source(b.marks)));
    }
    if (w::mark_source(b.marks) == w::MarkSource::EldenNet) {
      for (const std::uint32_t n = b.marks.count.load(std::memory_order_acquire); marks_seen < n;) {
        ++marks_seen;
        const std::uint64_t at = b.marks.mono_ms[(marks_seen - 1) % w::kMarkRing].load(std::memory_order_relaxed);
        r.record.marks.push_back(at);
        say("mark n=%u mono_ms=%llu source=eldennet", marks_seen, static_cast<unsigned long long>(at));
        if (tasks && !plan.read) {
          // The owner's Pause, now EldenNet's Mark: it confirms the task under way (the ring already holds the Mark).
          const TaskList::Change c = tasks->confirm(now);
          if (c.changed) {
            task_done(c, "mark", now);
          } else {
            say("task mark=%u confirmed=0 current=%zu", marks_seen, tasks->current() + 1);
          }
        }
      }
    }
    if (!window_seen && main_window(pid) != nullptr) {
      window_seen = true;
      window_last = now;
      say("window after_ms=%llu", static_cast<unsigned long long>(now - t0));
      take_modules(false);
    } else if (window_seen && main_window(pid) != nullptr) {
      window_last = now;
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
    if (plan.read) {
      const auto answer = static_cast<w::ProbeRequest>(b.control.probe.load(std::memory_order_acquire));
      const auto step = read_step(answer, st.init, st.items_resolved, st.items_total, st.discovery_passes);
      if (step && step != read_said) {
        read_tell(*step, st.items_resolved, st.items_total);
        // The round's own task ends when the round says to load the save (or has everything already); a round
        // that will read nothing says so on the panel.
        if (*step == ReadStep::LoadSave || *step == ReadStep::Done) task_done(tasks->round_step(now), "round", now);
        if (*step == ReadStep::Refused || *step == ReadStep::Stopped) panel_footer = read_text(*step);
      }
    }
    if (s0 && !s0_stopped && w::terminal(st.init)) {
      // The start has stopped (Ready, a banner, inert): eldennet.dll wrote its report before it said so. What it did,
      // in plain words, and what waits when it stopped short of its reading for want of something; S0-B's list then
      // ends with quitting the game, every other spike's steps stay. The run's own task (the title's wait) is done.
      s0_stopped = true;
      r.s0 = read_report(*plan.s0_report);
      if (!r.s0) {
        say("s0 report=missing");
        console_line(s0_missing_text(plan.spike), who);
      } else {
        const S0Facts& f = *r.s0;
        say("s0 report=read state=%s waits=%s table=%u refused=%u rechecked=%d again_refused=%u changed=%u "
            "getter_calls=%u forced=%u caught=%u inject=%s",
            f.state.c_str(), f.waits.c_str(), f.table_items, f.table_refused, f.table_rechecked ? 1 : 0,
            f.table_again_refused, f.table_changed, f.getter_calls, f.forced, f.caught, f.inject_group.c_str());
        console_line(s0_stopped_text(plan.spike, f), who);
        if (f.waits != "none") {
          panel_footer = s0_waits_text(plan.spike, f);
          console_line(panel_footer, who);
          if (auto rest = s0_waiting_tasks(plan.spike, f); !rest.empty() && tasks) {
            tasks.emplace(std::move(rest));
            tasks->set_pace(plan.spike_pace_s);
            tasks->start(now);
            say("tasks count=%zu waits=%s", tasks->size(), f.waits.c_str());
            tell_task(now);
          }
        }
      }
      if (tasks) task_done(tasks->round_step(now), "round", now);
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
        // S0-D's crash once T1 has stopped where T6, which honours it, runs: at Ready, or at a banner or inert when
        // the start stopped short of its reading (the start's layout not complete yet).
        if (plan.crash_when_ready && st.init != w::InitState::Exit) {
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
  panel.close();
  if (tasks) {
    task_done(tasks->end(end), "end", end);  // the last task is the game's end itself
    say("tasks done=%zu of=%zu finished=%d", tasks->current(), tasks->size(), tasks->finished() ? 1 : 0);
  }
  r.record.panel = r.panel_shown;
  if (plan.run_dir) {
    // The run's folder says whether the panel was shown, a window always on top of the game's, so that the spike's
    // frame counts are read knowing it; how far the tasks went; and the game's modules as last listed (names only, no
    // path, as a crash record keeps them), so that another program's overlay in the chain is known (WHY§515).
    const nlohmann::json j{{"launcher_run", 1},
                           {"spike", w::to_string(plan.spike)},
                           {"panel", r.panel_shown},
                           {"tasks", tasks ? tasks->size() : 0},
                           {"tasks_done", tasks ? tasks->current() : 0},
                           {"marks", r.record.marks.size()},
                           {"modules", r.record.modules}};
    std::ofstream(*plan.run_dir / L"launcher-run.json", std::ios::binary | std::ios::trunc) << j.dump(1) << "\n";
  }
  if (s0) {
    // The report as T6 last wrote it, with the reader's counts (every 10 s), in place of T1's.
    if (auto f = read_report(*plan.s0_report)) r.s0 = std::move(f);
    if (r.s0) {
      say("s0 end state=%s waits=%s reading=%d reading_ms=%llu", r.s0->state.c_str(), r.s0->waits.c_str(),
          r.s0->reading ? 1 : 0, static_cast<unsigned long long>(r.s0->reading_ms));
    }
  }
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
  // The game's window gone for two polls or more before the end: the player had quit and the game was closing (O115).
  r.record.after_window = window_seen && end > window_last && end - window_last >= 400;
  if (!appid_done) say("appid removal=%s at=end", to_string(remove_if_ours(*plan.appid, plan.appid_value)));
  if (plan.remove_appid && *plan.remove_appid == RemoveAfter::Exit && plan.appid) {
    say("appid removal=%s at=exit", to_string(remove_if_ours(*plan.appid, plan.appid_value)));
  }
  say("end exit_code=0x%08X kind=%s crash_filter=%d closed=%d timed_out=%d init=%s modules=%zu at_crash=%d marks=%zu "
      "session_ms=%llu connections_max=%u after_window=%d",
      code, to_string(r.record.kind), r.record.crash ? 1 : 0, r.closed_by_us ? 1 : 0, r.timed_out ? 1 : 0,
      w::to_string(r.last_init), r.record.modules.size(), r.record.modules_at_crash ? 1 : 0, r.record.marks.size(),
      static_cast<unsigned long long>(r.ms), r.record.connections_max, r.record.after_window ? 1 : 0);
  return r;
}

}  // namespace en::launcher::win
