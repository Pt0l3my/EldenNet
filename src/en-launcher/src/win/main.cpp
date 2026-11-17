// eldennet-launcher: finds the game, checks before it starts, backs up the saves, starts it without EAC, loads EldenNet
// and stays as the crash watcher until the game ends (ARCH §4, O5, O71); with the development options, the S0 spikes'
// loops and the dev console with the Mark (O90). Its exit codes are options.cpp's usage text's.
#include "win.hpp"  // windows.h first

#include <shellapi.h>

#include <algorithm>
#include <fstream>
#include <system_error>

#include "en/launcher/appid.hpp"
#include "en/launcher/backup.hpp"
#include "en/launcher/exe.hpp"
#include "en/launcher/steam.hpp"
#include "en/gal/derive.hpp"
#include "en/gal/report.hpp"
#include "en/proto/ffi_guard.hpp"
#include "en/shell/probe_report.hpp"

#ifndef EN_LAUNCHER_VERSION
#define EN_LAUNCHER_VERSION "0.0.0"
#endif

namespace en::launcher::win {

namespace {

namespace w = proto::watch;

constexpr int kExitDone = 0;
constexpr int kExitSpikeFailed = 1;
constexpr int kExitUsage = 2;
constexpr int kExitCannot = 3;
constexpr std::size_t kCrashRecordsKept = 20;

struct Ctx {
  Options o;
  Paths p;
  LauncherConfig cfg;
  GameSpec game = elden_ring();
};

// ---- Files of the launcher's own ----

void write_text(const fs::path& file, const std::string& text) {
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  const fs::path tmp = file.wstring() + L".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out << text;
  }
  fs::rename(tmp, file, ec);
}

void append_line(const fs::path& file, const std::string& line) {
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  std::ofstream(file, std::ios::binary | std::ios::app) << line << "\n";
}

LauncherConfig load_config(const Paths& p) {
  const auto text = read_text(p.config / "launcher.toml", 64u << 10);
  if (!text) return {};
  ConfigRead r = read_config(*text);
  for (const auto& problem : r.problems) say("config problem=\"%s\"", problem.c_str());
  return r.config;
}

void save_config(const Paths& p, const LauncherConfig& c) { write_text(p.config / "launcher.toml", config_text(c)); }

std::optional<AppidRecord> load_state(const Paths& p) {
  const auto text = read_text(p.config / "launcher-state.toml", 64u << 10);
  return text ? read_state(*text) : std::nullopt;
}

void save_state(const Paths& p, const std::optional<AppidRecord>& r) {
  write_text(p.config / "launcher-state.toml", state_text(r));
}

std::vector<fs::path> crash_records(const Paths& p) {
  std::vector<fs::path> out;
  std::error_code ec;
  for (fs::directory_iterator it(p.crashes, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = narrow(it->path().filename().wstring());
    if (name.rfind("crash-", 0) == 0 && it->path().extension() == ".json") out.push_back(it->path());
  }
  std::sort(out.begin(), out.end());
  return out;
}

void keep_record(const Paths& p, const SessionRecord& r, DWORD pid) {
  write_text(p.crashes / widen("crash-" + r.ended + "-" + std::to_string(pid) + ".json"), to_json(r).dump(1) + "\n");
  auto all = crash_records(p);
  std::error_code ec;
  while (all.size() > kCrashRecordsKept) {
    fs::remove(all.front(), ec);
    all.erase(all.begin());
  }
  say("record kept=1 kind=%s", to_string(r.kind));
}

// The next start explains what the watcher kept since the last (O71); every record shown is marked explained.
void explain_last_crash(const Paths& p) {
  std::optional<std::string> newest;
  std::size_t count = 0;
  for (const fs::path& f : crash_records(p)) {
    const auto text = read_text(f, 1u << 20);
    if (!text) continue;
    nlohmann::json j = nlohmann::json::parse(*text, nullptr, false);
    const auto e = explanation(j);
    if (!e) continue;
    newest = e;
    ++count;
    j["explained"] = true;
    write_text(f, j.dump(1) + "\n");
  }
  if (!newest) return;
  say("last_crash explained=%zu", count);
  tell("The last session ended with an error", *newest);
}

// ---- Steps ----

int fail(ErrorCode e, const std::string& shown_detail = {}, std::uint32_t code = 0) {
  tell_error(e, error_text(e, shown_detail, code));
  return exit_code(e);
}

std::string game_exe_name(const Ctx& c) {
  if (c.o.game) {
    if (const auto p = resolve_game_path(path_from_utf8(*c.o.game), c.game)) return narrow(p->filename().wstring());
  }
  if (c.cfg.game) {
    if (const auto p = resolve_game_path(*c.cfg.game, c.game)) return narrow(p->filename().wstring());
  }
  return c.game.exe_name();
}

// A steam_appid.txt this launcher created and has not removed yet (a crash of the launcher, a power cut): removed while
// the game is not running, at every start and with --cleanup (ARCH §4.5).
enum class Stale : std::uint8_t { None, Removed, GameRunning, Kept };
Stale clean_stale_appid(const Ctx& c, const std::vector<ProcessEntry>& processes) {
  const auto rec = load_state(c.p);
  if (!rec) return Stale::None;
  if (running(processes, game_exe_name(c))) return Stale::GameRunning;
  const AppidRemoval r = remove_if_ours(*rec, c.game.appid);
  say("appid stale=1 removal=%s", to_string(r));
  if (r == AppidRemoval::Failed) return Stale::Kept;
  save_state(c.p, std::nullopt);
  return Stale::Removed;
}

struct Found {
  std::optional<fs::path> exe;
  const char* how = "none";
  std::size_t libraries = 0, manifests = 0;
};

Found find_game(Ctx& c, bool remember) {
  Found f;
  if (c.o.game) {
    f.how = "game_option";
    if (const auto p = resolve_game_path(path_from_utf8(*c.o.game), c.game)) {
      f.exe = fs::absolute(*p);
      if (remember && c.cfg.game != f.exe) {
        c.cfg.game = f.exe;
        save_config(c.p, c.cfg);
      }
    }
    return f;
  }
  if (c.cfg.game) {
    f.how = "remembered";
    if (const auto p = resolve_game_path(*c.cfg.game, c.game)) {
      f.exe = *p;
      return f;
    }
  }
  const auto steam = c.o.steam ? std::optional<fs::path>(path_from_utf8(*c.o.steam)) : steam_folder();
  if (steam) {
    f.how = "steam";
    const SteamScan scan = find_in_steam(*steam, c.game);
    f.libraries = scan.libraries.size();
    f.manifests = scan.manifests;
    for (const auto& note : scan.notes) say("discovery note=\"%s\"", note.c_str());
    f.exe = scan.exe;
  }
  return f;
}

fs::path dll_path(const Ctx& c) {
  return c.o.dll ? fs::absolute(path_from_utf8(*c.o.dll)) : c.p.exe_dir / L"eldennet.dll";
}

std::string dll_label(const fs::path& dll) { return safe_module_name(narrow(dll.filename().wstring())); }

struct Gate {
  GateOutcome outcome;
  std::string build = "unknown";
};

Gate gate(const Ctx& c, const fs::path& exe) {
  Gate g;
  g.outcome = gate_game(exe, c.p.gal / L"builds.toml");
  if (g.outcome.kind == GateOutcome::Kind::Passed) g.build = g.outcome.key;
  say("gate result=%s build=%s version=%s why=\"%s\"", to_string(g.outcome.kind), g.build.c_str(),
      g.outcome.version.empty() ? "none" : g.outcome.version.c_str(), g.outcome.why.c_str());
  return g;
}

// ---- R1's reports (--probe) ----

// After an R1 session: the two reports EldenNet wrote, checked by the lint they must pass before they leave the PC
// (`eldennet-engine lint --report`, the same vocabularies), each told as a line, and the result to the owner.
void check_r1_reports(const Paths& p) {
  const fs::path folder = p.en / L"probe";
  const gal::report::Vocabulary* kinds[] = {&gal::derive::vocabulary(), &shell::probe_vocabulary()};
  bool missing = false;
  bool refused = false;
  for (const char* name : {"r1-derive.json", "r1-probe.json"}) {
    const auto text = read_text(folder / name, gal::report::kMaxReportBytes);
    if (!text) {
      missing = true;
      say("r1 report=%s lint=missing", name);
      continue;
    }
    const auto result = gal::report::lint(*text, kinds);
    refused = refused || !result;
    say("r1 report=%s lint=%s problems=%zu bytes=%zu", name, result ? "clean" : "refused",
        result ? std::size_t{0} : result.error().size(), text->size());
  }
  const R1Step step = missing ? R1Step::ReportsMissing : refused ? R1Step::ReportsRefused : R1Step::ReportsClean;
  say("r1 step=%s", r1_word(step));
  console_line(r1_text(step));
}

// ---- One start, watched to its end ----

struct Run {
  RunResult result;
  int error_exit = 0;          // an E-code's exit when the run could not start or load
  bool start_vanilla = false;  // E10's "Start without EldenNet" was chosen
};

Run run_once(const Ctx& c, std::uint32_t run, const fs::path& exe, const std::string& build, bool inject_dll,
             AppidMethod method, RemoveAfter remove_after) {
  Run out;
  out.result.run = run;
  const std::string started_stamp = local_stamp();
  const std::uint64_t t0 = mono_ms();

  // Starting without EAC (ARCH §4.5).
  bool env_appid = method == AppidMethod::Env;
  std::optional<AppidRecord> created;
  if (method == AppidMethod::File) {
    const AppidPrepared prep = prepare_appid_file(exe.parent_path(), c.game.appid, started_stamp);
    say("appid method=file step=%s", to_string(prep.step));
    if (prep.step == AppidStep::Unwritable) {
      out.result.outcome = RunOutcome::StartFailed;
      out.error_exit = fail(ErrorCode::E06, narrow(exe.parent_path().wstring()));
      return out;
    }
    if (prep.step == AppidStep::Foreign) env_appid = true;  // someone else's file stays as it is
    if (prep.record) {
      created = prep.record;
      save_state(c.p, created);
    }
  } else {
    say("appid method=env");
  }
  const auto drop_appid = [&] {
    if (!created) return;
    say("appid removal=%s at=failure", to_string(remove_if_ours(*created, c.game.appid)));
    save_state(c.p, std::nullopt);
  };

  const Started s = start_game(exe, c.o.game_args, env_appid, c.game.appid);
  if (!s.ok()) {
    say("started ok=0 error=%lu", s.error);
    drop_appid();
    out.result.outcome = RunOutcome::StartFailed;
    out.error_exit = fail(ErrorCode::E09, "CreateProcessW", s.error);
    return out;
  }
  const DWORD pid = s.pi.dwProcessId;
  const InjectAt at = inject_dll ? c.o.inject_at : InjectAt::Suspended;
  say("started ok=1 run=%u mode=%s inject=%d also=%zu", run, inject_dll ? to_string(at) : "vanilla", inject_dll ? 1 : 0,
      c.o.also.size());
  auto wb = WatchBlock::open(pid);
  if (!wb) say("watch_block ok=0 error=%lu", GetLastError());
  if (wb && inject_dll) {
    // What EldenNet reads at its start, before anything is loaded: the spike, and R1 (--probe), which it answers.
    wb->block().control.spike.store(static_cast<std::uint32_t>(c.o.spike), std::memory_order_release);
    if (c.o.probe) {
      wb->block().control.probe.store(static_cast<std::uint32_t>(w::ProbeRequest::R1), std::memory_order_release);
    }
  }

  std::vector<fs::path> dlls;
  for (const auto& a : c.o.also) dlls.push_back(fs::absolute(path_from_utf8(a)));
  if (inject_dll) dlls.push_back(dll_path(c));
  const auto load_all = [&]() -> std::optional<Injected> {
    for (const fs::path& d : dlls) {
      const Injected inj = inject(s.pi.hProcess, pid, d);
      say("inject dll=%s ok=%d step=\"%.*s\" code=%lu", dll_label(d).c_str(), inj.ok ? 1 : 0,
          static_cast<int>(inj.step.size()), inj.step.data(), inj.code);
      if (!inj.ok) return inj;
    }
    return std::nullopt;
  };
  const auto abandon = [&](std::string_view step, DWORD code) {
    // A half-loaded game never runs on (ARCH §4.6): it is ended, and E10 says which step failed.
    TerminateProcess(s.pi.hProcess, kClosedByLauncher);
    WaitForSingleObject(s.pi.hProcess, 10000);
    CloseHandle(s.pi.hThread);
    CloseHandle(s.pi.hProcess);
    drop_appid();
    out.result.outcome = RunOutcome::InjectFailed;
    out.result.end = EndKind::Closed;
    out.result.exit_code = kClosedByLauncher;
    out.start_vanilla = ask(ErrorCode::E10, error_text(ErrorCode::E10, step, code));
    out.error_exit = exit_code(ErrorCode::E10);
  };

  if (!wb) {
    abandon("watch block", GetLastError());
    return out;
  }
  if (at == InjectAt::Suspended) {
    if (const auto failed = load_all()) {
      abandon(failed->step, failed->code);
      return out;
    }
  }
  if (ResumeThread(s.pi.hThread) == static_cast<DWORD>(-1)) {
    abandon(kStepResume, GetLastError());
    return out;
  }
  if (at != InjectAt::Suspended) {
    // A late load (ARCH §3.3): once the window shows, or a while after it, about when the title screen does.
    bool window = false;
    for (int i = 0; i < 1200 && !window && WaitForSingleObject(s.pi.hProcess, 100) == WAIT_TIMEOUT; ++i) {
      window = main_window(pid) != nullptr;
    }
    say("late_load window=%d", window ? 1 : 0);
    if (window && at == InjectAt::Title) WaitForSingleObject(s.pi.hProcess, c.o.title_wait_s * 1000);
    if (WaitForSingleObject(s.pi.hProcess, 0) == WAIT_TIMEOUT) {
      if (!window) {
        abandon("window", WAIT_TIMEOUT);
        return out;
      }
      if (const auto failed = load_all()) {
        abandon(failed->step, failed->code);
        return out;
      }
    }
  }

  WatchPlan plan;
  plan.console = c.o.console;
  plan.probe = c.o.probe && inject_dll;
  plan.spike = c.o.spike;
  if (c.o.spike == w::Spike::S0a || c.o.spike == w::Spike::S0dCrash) plan.run_timeout_s = c.o.run_timeout_s;
  plan.crash_when_ready = c.o.spike == w::Spike::S0dCrash;
  plan.count_connections = c.o.spike == w::Spike::S0c;
  plan.remove_appid = remove_after;
  plan.appid = created;
  plan.appid_value = c.game.appid;
  WatchResult res = watch_game(s.pi.hProcess, pid, *wb, plan);
  if (created && remove_after != RemoveAfter::Never) save_state(c.p, std::nullopt);
  if (plan.probe) check_r1_reports(c.p);

  SessionRecord& rec = res.record;
  rec.launcher_version = EN_LAUNCHER_VERSION;
  rec.build = build;
  rec.started = started_stamp;
  rec.ended = local_stamp();
  rec.mode = inject_dll ? to_string(at) : "vanilla";
  rec.spike = c.o.spike;
  const bool kept = worth_keeping(rec);
  if (kept) keep_record(c.p, rec, pid);
  CloseHandle(s.pi.hThread);
  CloseHandle(s.pi.hProcess);

  RunResult& r = out.result;
  if (res.timed_out) {
    r.outcome = RunOutcome::Timeout;
  } else if (w::terminal(res.last_init)) {
    r.outcome = outcome_of(res.last_init);
  } else {
    r.outcome = res.last_init == w::InitState::None && inject_dll ? RunOutcome::NoDll : RunOutcome::GameEnded;
  }
  r.status = rec.status;
  r.end = rec.kind;
  r.exit_code = rec.exit_code;
  r.record_kept = kept;
  r.crash_filter = rec.crash.has_value();
  r.run_ms = mono_ms() - t0;
  r.connections_max = rec.connections_max;
  return out;
}

// ---- The actions ----

int do_start(Ctx& c) {
  explain_last_crash(c.p);
  const Found found = find_game(c, true);
  say("discovery how=%s found=%d libraries=%zu manifests=%zu", found.how, found.exe ? 1 : 0, found.libraries,
      found.manifests);
  std::optional<fs::path> exe = found.exe;
  if (!exe) {
    if (ask(ErrorCode::E01, error_text(ErrorCode::E01))) {
      exe = pick_game(c.game.exe_name());
      if (exe) {
        c.cfg.game = *exe;
        save_config(c.p, c.cfg);
      }
    }
    if (!exe) return exit_code(ErrorCode::E01);
  }
  const auto processes = list_processes();
  clean_stale_appid(c, processes);
  if (c.cfg.notices_seen < kNoticesVersion && show_notices()) {
    c.cfg.notices_seen = kNoticesVersion;
    save_config(c.p, c.cfg);
  }
  bool inject_dll = !c.o.vanilla;
  const Gate g = gate(c, *exe);
  if (inject_dll && g.outcome.kind != GateOutcome::Kind::Passed) {
    // E05: launch anyway, and EldenNet stays inactive (its own gate stops it at L1).
    if (!ask(ErrorCode::E05, error_text(ErrorCode::E05, g.outcome.version.empty() ? "unknown" : g.outcome.version))) {
      return exit_code(ErrorCode::E05);
    }
  }

  // Pre-flight (ARCH §4.3).
  PreflightFacts facts;
  facts.steam_running = c.o.no_steam_check || steam_running(processes);
  facts.eac_running = running(processes, "start_protected_game.exe");
  facts.game_running = running(processes, narrow(exe->filename().wstring()));
  const fs::path dll = dll_path(c);
  facts.dll_ours = !inject_dll || is_ours(dll);
  const auto proxies = proxy_dlls_beside(files_in(exe->parent_path()));
  std::string proxy_list;
  for (const auto& px : proxies) proxy_list += (proxy_list.empty() ? "" : ",") + px;
  say("preflight steam=%d steam_checked=%d eac=%d game_running=%d dll_ours=%d dll=%s other_mods=%s",
      facts.steam_running ? 1 : 0, c.o.no_steam_check ? 0 : 1, facts.eac_running ? 1 : 0, facts.game_running ? 1 : 0,
      facts.dll_ours ? 1 : 0, inject_dll ? dll_label(dll).c_str() : "none",
      proxy_list.empty() ? "none" : proxy_list.c_str());
  if (const auto e = preflight_failure(facts, !c.o.no_steam_check, inject_dll)) return fail(*e);
  if (!proxies.empty()) tell("Other mods detected", "Other mods detected: " + proxy_list + ".");
  for (const auto& a : c.o.also) {
    std::error_code ec;
    if (!fs::is_regular_file(path_from_utf8(a), ec)) {
      say("usage error=\"an --also DLL does not exist\"");
      return kExitUsage;
    }
  }

  // Save backups (ARCH §4.4).
  if (c.o.no_backup) {
    say("backup skipped=1");
  } else {
    const BackupReport b =
        back_up_saves(BackupSpec{c.p.saves, c.p.backups, elden_ring_save_files(), 10, local_stamp()});
    say("backup folders=%zu made=%zu unchanged=%zu empty=%zu removed=%zu problems=%zu", b.folders, b.made, b.unchanged,
        b.empty, b.removed, b.problems.size());
    // R1 (--probe) never goes on without a backup: its one call into the game rests on one (O89).
    if (!b.ok() && (c.o.probe || !ask(ErrorCode::E07, error_text(ErrorCode::E07, b.problems.front())))) {
      if (c.o.probe) say("r1 backup=failed");
      return c.o.probe ? fail(ErrorCode::E07, b.problems.front()) : exit_code(ErrorCode::E07);
    }
  }

  const AppidMethod method = c.o.appid_method.value_or(c.cfg.appid_method);
  const RemoveAfter remove_after = c.o.remove_after.value_or(c.cfg.remove_after);
  const std::uint32_t runs = (c.o.spike == w::Spike::S0a || c.o.spike == w::Spike::S0dCrash) ? c.o.runs : 1;
  const std::string spike_log = c.o.spike == w::Spike::None
                                    ? std::string{}
                                    : std::string(w::to_string(c.o.spike)) + "-" + local_stamp() + ".jsonl";
  std::vector<RunResult> results;
  for (std::uint32_t run = 1; run <= runs; ++run) {
    Run r = run_once(c, run, *exe, g.build, inject_dll, method, remove_after);
    if (r.start_vanilla && c.o.spike == w::Spike::None) {
      say("start_without_eldennet=1");
      r = run_once(c, run, *exe, g.build, false, method, remove_after);
    }
    say("run n=%u outcome=%s end=%s exit_code=0x%08X record=%d crash_filter=%d run_ms=%llu", run,
        to_string(r.result.outcome), to_string(r.result.end), r.result.exit_code, r.result.record_kept ? 1 : 0,
        r.result.crash_filter ? 1 : 0, static_cast<unsigned long long>(r.result.run_ms));
    results.push_back(r.result);
    if (!spike_log.empty()) append_line(c.p.spikes / widen(spike_log), to_json(r.result, c.o.spike).dump());
    if (c.o.spike == w::Spike::None) return r.error_exit != 0 ? r.error_exit : kExitDone;
    if (run < runs) Sleep(2000);  // the game's own processes and files settle between runs
  }
  const Tally t = tally(results);
  const bool ok = passed(c.o.spike, t);
  append_line(c.p.spikes / widen(spike_log), to_json(t, c.o.spike).dump());
  say("tally spike=%s %s passed=%d", w::to_string(c.o.spike), tally_line(t).c_str(), ok ? 1 : 0);
  return ok ? kExitDone : kExitSpikeFailed;
}

int do_watch(const Ctx& c) {
  HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, c.o.watch_pid);
  if (h == nullptr) {
    say("watch ok=0 error=%lu", GetLastError());
    return kExitCannot;
  }
  auto wb = WatchBlock::open(c.o.watch_pid);
  if (!wb) {
    say("watch ok=0 block=0");
    CloseHandle(h);
    return kExitCannot;
  }
  say("watch ok=1 block_created=%d", wb->created() ? 1 : 0);
  WatchPlan plan;
  plan.console = c.o.console;
  WatchResult res = watch_game(h, c.o.watch_pid, *wb, plan);
  CloseHandle(h);
  SessionRecord& rec = res.record;
  rec.launcher_version = EN_LAUNCHER_VERSION;
  rec.ended = local_stamp();
  rec.started = rec.ended;
  rec.build = "unknown";
  rec.mode = "watch";
  if (worth_keeping(rec)) keep_record(c.p, rec, c.o.watch_pid);
  return kExitDone;
}

int do_backup_only(const Ctx& c) {
  const BackupReport b = back_up_saves(BackupSpec{c.p.saves, c.p.backups, elden_ring_save_files(), 10, local_stamp()});
  say("backup folders=%zu made=%zu unchanged=%zu empty=%zu removed=%zu problems=%zu", b.folders, b.made, b.unchanged,
      b.empty, b.removed, b.problems.size());
  if (!b.ok()) return fail(ErrorCode::E07, b.problems.front());
  return kExitDone;
}

int do_cleanup(const Ctx& c) {
  switch (clean_stale_appid(c, list_processes())) {
    case Stale::None:
      say("cleanup appid=none");
      return kExitDone;
    case Stale::GameRunning:
      say("cleanup game_running=1");
      return fail(ErrorCode::E03);
    case Stale::Removed:
      say("cleanup appid=done");
      return kExitDone;
    case Stale::Kept:
      say("cleanup appid=failed");
      return kExitCannot;
  }
  return kExitDone;
}

int do_diagnostics(Ctx& c) {
  const Found found = find_game(c, false);
  say("diagnostics discovery how=%s found=%d libraries=%zu manifests=%zu", found.how, found.exe ? 1 : 0,
      found.libraries, found.manifests);
  const auto processes = list_processes();
  if (found.exe) {
    const Gate g = gate(c, *found.exe);
    (void)g;
    const auto content = read_text(found.exe->parent_path() / path_from_utf8(kAppidFileName), 4096);
    const AppidFile f = classify(content, c.game.appid);
    say("diagnostics appid_file=%s", f == AppidFile::Absent ? "absent" : f == AppidFile::Ours ? "appid" : "other");
    std::string proxy_list;
    for (const auto& px : proxy_dlls_beside(files_in(found.exe->parent_path()))) {
      proxy_list += (proxy_list.empty() ? "" : ",") + px;
    }
    say("diagnostics other_mods=%s", proxy_list.empty() ? "none" : proxy_list.c_str());
  }
  const fs::path dll = dll_path(c);
  std::error_code ec;
  say("diagnostics steam=%d eac=%d game_running=%d dll_exists=%d dll_ours=%d state_record=%d",
      steam_running(processes) ? 1 : 0, running(processes, "start_protected_game.exe") ? 1 : 0,
      running(processes, game_exe_name(c)) ? 1 : 0, fs::exists(dll, ec) ? 1 : 0, is_ours(dll) ? 1 : 0,
      load_state(c.p) ? 1 : 0);
  std::size_t ids = 0;
  for (fs::directory_iterator it(c.p.saves, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_directory(ec) && is_steam_id_folder(narrow(it->path().filename().wstring()))) ++ids;
  }
  say("diagnostics save_folders=%zu crash_records=%zu appid_method=%s remove_after=%s", ids, crash_records(c.p).size(),
      to_string(c.cfg.appid_method), to_string(c.cfg.remove_after));
  return kExitDone;
}

int run_launcher() {
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::vector<std::string> args;
  for (int i = 1; argv != nullptr && i < argc; ++i) args.push_back(narrow(argv[i]));
  if (argv != nullptr) LocalFree(argv);
  const auto parsed = parse_options(args);
  if (!parsed) {
    open_output(false);
    say("usage error=\"%s\"", parsed.error().c_str());
    return kExitUsage;
  }
  Ctx c;
  c.o = *parsed;
  open_output(c.o.console);
  set_dialogs(!c.o.no_dialogs, c.o.yes);
  if (c.o.action == Action::Help) {
    const std::string text = usage_text();
    if (GetStdHandle(STD_OUTPUT_HANDLE) != nullptr) {
      DWORD written = 0;
      WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    }
    tell("eldennet-launcher", text);
    return kExitDone;
  }

  wchar_t self[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  c.p.exe_dir = fs::path(self).parent_path();
  const auto appdata =
      c.o.appdata ? std::optional<fs::path>(fs::absolute(path_from_utf8(*c.o.appdata))) : appdata_folder();
  if (!appdata) {
    say("start error=\"no AppData folder\"");
    return kExitCannot;
  }
  c.p.appdata = *appdata;
  c.p.en = c.p.appdata / L"EldenNet";
  c.p.config = c.p.en / L"config";
  c.p.logs = c.p.en / L"logs";
  c.p.crashes = c.p.en / L"crashes";
  c.p.spikes = c.p.en / L"spikes";
  c.p.backups = c.p.en / L"save-backups";
  c.p.saves = c.p.appdata / L"EldenRing";
  c.p.gal = c.o.gal ? fs::absolute(path_from_utf8(*c.o.gal)) : c.p.exe_dir / L"gal";
  open_log(c.p.logs);
  static constexpr const char* kActions[] = {"start", "backup_only", "cleanup", "diagnostics", "watch", "help"};
  say("begin version=%s action=%s spike=%s runs=%u console=%d ignored_args=%zu", EN_LAUNCHER_VERSION,
      kActions[static_cast<int>(c.o.action)], w::to_string(c.o.spike), c.o.runs, c.o.console ? 1 : 0, c.o.ignored);
  c.cfg = load_config(c.p);
  int code = kExitDone;
  switch (c.o.action) {
    case Action::Start:
      code = do_start(c);
      break;
    case Action::BackupOnly:
      code = do_backup_only(c);
      break;
    case Action::Cleanup:
      code = do_cleanup(c);
      break;
    case Action::Diagnostics:
      code = do_diagnostics(c);
      break;
    case Action::Watch:
      code = do_watch(c);
      break;
    case Action::Help:
      break;
  }
  say("finish exit=%d", code);
  return code;
}

}  // namespace

}  // namespace en::launcher::win

// R3: the entry starts with ffi_guard; an exception that escapes the flow ends the launcher with exit code 3. (The
// SDK declares wWinMain without noexcept, so the definition cannot add it; ffi_guard is noexcept itself.)
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  return en::proto::ffi_guard(en::proto::Component::Launcher, 3, [] { return en::launcher::win::run_launcher(); });
}
