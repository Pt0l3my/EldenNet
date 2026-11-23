// The watcher's records, the spikes' runs and the pre-flight's decisions (record.hpp; O71, ARCH §4.3).
#include "en/launcher/record.hpp"

#include <algorithm>
#include <cstdio>

namespace en::launcher {

namespace w = proto::watch;

const char* to_string(EndKind k) noexcept {
  switch (k) {
    case EndKind::Clean:
      return "clean";
    case EndKind::Exit:
      return "exit";
    case EndKind::Crash:
      return "crash";
    case EndKind::Closed:
      return "closed";
  }
  return "exit";
}

EndKind classify_end(std::uint32_t exit_code, bool crash_signalled) noexcept {
  if (exit_code == kClosedByLauncher) return EndKind::Closed;
  if (crash_signalled) return EndKind::Crash;
  if (exit_code == 0) return EndKind::Clean;
  // An exception's status: severity "error" (0xC…), or a breakpoint or single step that no handler took.
  if ((exit_code & 0xF0000000u) == 0xC0000000u || exit_code == 0x80000003u || exit_code == 0x80000004u) {
    return EndKind::Crash;
  }
  return EndKind::Exit;
}

DllStatus copy_status(const w::Block& b) noexcept {
  const w::Status& s = b.status;
  DllStatus d;
  const std::uint32_t init = s.init.load(std::memory_order_acquire);
  d.init = init < w::kInitStateCount ? static_cast<w::InitState>(init) : w::InitState::None;
  d.phase = s.phase.load(std::memory_order_relaxed);
  d.level = s.level.load(std::memory_order_relaxed);
  d.banner = s.banner.load(std::memory_order_relaxed);
  d.flags = s.flags.load(std::memory_order_relaxed);
  d.items_resolved = s.items_resolved.load(std::memory_order_relaxed);
  d.items_total = s.items_total.load(std::memory_order_relaxed);
  d.discovery_passes = s.discovery_passes.load(std::memory_order_relaxed);
  d.ready_ms = s.ready_ms.load(std::memory_order_relaxed);
  d.task_us_p99 = s.task_us_p99.load(std::memory_order_relaxed);
  d.snapshots = s.snapshots.load(std::memory_order_relaxed);
  d.faults = s.faults.load(std::memory_order_relaxed);
  d.tripped = s.tripped.load(std::memory_order_relaxed);
  return d;
}

std::string safe_module_name(std::string_view name) {
  if (const auto cut = name.find_last_of("\\/"); cut != std::string_view::npos) name.remove_prefix(cut + 1);
  std::string out;
  for (const char c : name) {
    if (out.size() >= 64) break;
    const bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                      c == '_' || c == '-' || c == '+' || c == ' ';
    out.push_back(keep ? c : '?');
  }
  return out;
}

std::optional<CrashInfo> copy_crash(const w::Block& b) {
  const auto state = static_cast<w::CrashState>(b.crash.state.load(std::memory_order_acquire));
  if (state != w::CrashState::Written && state != w::CrashState::Taken) return std::nullopt;
  CrashInfo c;
  c.code = b.crash.code;
  c.thread = b.crash.thread <= static_cast<std::uint32_t>(w::ThreadKind::Reader)
                 ? static_cast<w::ThreadKind>(b.crash.thread)
                 : w::ThreadKind::Unknown;
  c.chained = b.crash.chained != 0;
  c.offset = b.crash.offset;
  c.module = safe_module_name(w::module_name(b.crash));
  return c;
}

bool worth_keeping(const SessionRecord& r) noexcept { return r.kind == EndKind::Crash || r.kind == EndKind::Exit; }

namespace {

std::string hex32(std::uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "0x%08X", v);
  return b;
}

nlohmann::json status_json(const DllStatus& s) {
  return nlohmann::json{{"init", w::to_string(s.init)},
                        {"phase", s.phase},
                        {"level", s.level},
                        {"banner", s.banner},
                        {"flags", s.flags},
                        {"items_resolved", s.items_resolved},
                        {"items_total", s.items_total},
                        {"discovery_passes", s.discovery_passes},
                        {"ready_ms", s.ready_ms},
                        {"task_us_p99", s.task_us_p99},
                        {"snapshots", s.snapshots},
                        {"faults", s.faults},
                        {"tripped", s.tripped}};
}

}  // namespace

nlohmann::json to_json(const SessionRecord& r) {
  nlohmann::json j{{"record", 1},
                   {"launcher", r.launcher_version},
                   {"build", r.build},
                   {"started", r.started},
                   {"ended", r.ended},
                   {"mode", r.mode},
                   {"spike", w::to_string(r.spike)},
                   {"kind", to_string(r.kind)},
                   {"exit_code", r.exit_code},
                   {"exit_code_hex", hex32(r.exit_code)},
                   {"session_ms", r.session_ms},
                   {"dll_attached", r.dll_attached},
                   {"status", status_json(r.status)},
                   {"modules", r.modules},
                   {"modules_at_crash", r.modules_at_crash},
                   {"modules_age_ms", r.modules_age_ms},
                   {"marks", r.marks},
                   {"connections_max", r.connections_max},
                   {"panel", r.panel},
                   {"explained", r.explained}};
  if (r.crash) {
    j["crash"] = nlohmann::json{
        {"code", r.crash->code},       {"code_hex", hex32(r.crash->code)}, {"thread", w::to_string(r.crash->thread)},
        {"chained", r.crash->chained}, {"module", r.crash->module},        {"offset", r.crash->offset}};
  } else {
    j["crash"] = nullptr;
  }
  return j;
}

std::optional<std::string> explanation(const nlohmann::json& record) {
  if (!record.is_object() || record.value("explained", true)) return std::nullopt;
  const std::string kind = record.value("kind", std::string{});
  const auto code = record.value("exit_code", std::uint32_t{0});
  if (kind == "crash") {
    std::string where;
    if (const auto it = record.find("crash"); it != record.end() && it->is_object()) {
      where = safe_module_name(it->value("module", std::string{}));
    }
    std::string text = "The game stopped with an error during the last session (" + hex32(code) +
                       (where.empty() ? std::string{} : ", in " + where) + ").";
    text +=
        " The launcher kept a short record of it in %APPDATA%\\EldenNet\\crashes: numbers and module names only, "
        "no save, chat or key. Include it if you report the problem.";
    return text;
  }
  if (kind == "exit") {
    return "The game ended with exit code " + std::to_string(code) +
           " during the last session. The launcher kept a short record of it in %APPDATA%\\EldenNet\\crashes.";
  }
  return std::nullopt;
}

// ---- Runs ----

const char* to_string(RunOutcome o) noexcept {
  switch (o) {
    case RunOutcome::Ready:
      return "ready";
    case RunOutcome::Banner:
      return "banner";
    case RunOutcome::Inert:
      return "inert";
    case RunOutcome::Exit:
      return "exit";
    case RunOutcome::GameEnded:
      return "game_ended";
    case RunOutcome::Timeout:
      return "timeout";
    case RunOutcome::NoDll:
      return "no_dll";
    case RunOutcome::StartFailed:
      return "start_failed";
    case RunOutcome::InjectFailed:
      return "inject_failed";
  }
  return "no_dll";
}

RunOutcome outcome_of(w::InitState last) noexcept {
  switch (last) {
    case w::InitState::Ready:
      return RunOutcome::Ready;
    case w::InitState::Banner:
      return RunOutcome::Banner;
    case w::InitState::Inert:
      return RunOutcome::Inert;
    case w::InitState::Exit:
      return RunOutcome::Exit;
    case w::InitState::None:
      return RunOutcome::NoDll;
    default:
      return RunOutcome::Timeout;
  }
}

Tally tally(std::span<const RunResult> runs) noexcept {
  Tally t;
  for (const RunResult& r : runs) {
    ++t.runs;
    switch (r.outcome) {
      case RunOutcome::Ready:
        ++t.ready;
        break;
      case RunOutcome::Banner:
        ++t.banner;
        break;
      case RunOutcome::Inert:
        ++t.inert;
        break;
      case RunOutcome::Exit:
        ++t.exit;
        break;
      case RunOutcome::GameEnded:
        ++t.game_ended;
        break;
      case RunOutcome::Timeout:
        ++t.timeout;
        break;
      case RunOutcome::NoDll:
        ++t.no_dll;
        break;
      case RunOutcome::StartFailed:
        ++t.start_failed;
        break;
      case RunOutcome::InjectFailed:
        ++t.inject_failed;
        break;
    }
    if (r.end == EndKind::Crash) ++t.crashes;
    if (r.record_kept) ++t.records;
    if (r.crash_filter) ++t.filter_records;
    if (r.report) ++t.reports;
    if (r.waits != "none") ++t.waiting;
    if (!r.table_ok) ++t.table_failed;
  }
  return t;
}

bool passed(w::Spike spike, const Tally& t) noexcept {
  if (t.runs == 0) return false;
  switch (spike) {
    case w::Spike::S0a:
      return t.ready + t.banner + t.inert == t.runs && t.crashes == 0 && t.table_failed == 0;
    case w::Spike::S0dCrash:
      return t.records == t.runs && t.crashes == t.runs;
    case w::Spike::S0b:
    case w::Spike::S0c:
    case w::Spike::S0d:
    case w::Spike::S0e:
    case w::Spike::S0eAlone:
    case w::Spike::S4:
    case w::Spike::None:
      return t.crashes == 0 && t.start_failed == 0 && t.inject_failed == 0;
  }
  return false;
}

nlohmann::json to_json(const RunResult& r, w::Spike spike) {
  nlohmann::json j{{"spike", w::to_string(spike)},
                        {"run", r.run},
                        {"mode", r.mode.empty() ? std::string("unknown") : r.mode},
                        {"outcome", to_string(r.outcome)},
                        {"status", status_json(r.status)},
                        {"end", to_string(r.end)},
                        {"exit_code", r.exit_code},
                        {"record_kept", r.record_kept},
                        {"crash_filter", r.crash_filter},
                        {"run_ms", r.run_ms},
                        {"connections_max", r.connections_max},
                        {"panel", r.panel}};
  if (r.folder != 0) {
    // An S0 run's or an observer's folder, spikes\<word>-<folder>; for an S0 run, what eldennet.dll's report said.
    j["folder"] = r.folder;
    j["report"] = r.report;
    j["waits"] = r.waits;
    j["table_ok"] = r.table_ok;
  }
  return j;
}

nlohmann::json to_json(const Tally& t, w::Spike spike) {
  return nlohmann::json{{"spike", w::to_string(spike)},
                        {"tally", 1},
                        {"runs", t.runs},
                        {"ready", t.ready},
                        {"banner", t.banner},
                        {"inert", t.inert},
                        {"exit", t.exit},
                        {"game_ended", t.game_ended},
                        {"timeout", t.timeout},
                        {"no_dll", t.no_dll},
                        {"start_failed", t.start_failed},
                        {"inject_failed", t.inject_failed},
                        {"crashes", t.crashes},
                        {"records", t.records},
                        {"filter_records", t.filter_records},
                        {"reports", t.reports},
                        {"waiting", t.waiting},
                        {"table_failed", t.table_failed},
                        {"passed", passed(spike, t)}};
}

std::string tally_line(const Tally& t) {
  char b[384];
  std::snprintf(b, sizeof b,
                "runs=%u ready=%u banner=%u inert=%u exit=%u game_ended=%u timeout=%u no_dll=%u start_failed=%u "
                "inject_failed=%u crashes=%u records=%u filter_records=%u reports=%u waiting=%u table_failed=%u",
                t.runs, t.ready, t.banner, t.inert, t.exit, t.game_ended, t.timeout, t.no_dll, t.start_failed,
                t.inject_failed, t.crashes, t.records, t.filter_records, t.reports, t.waiting, t.table_failed);
  return b;
}

// ---- Pre-flight ----

namespace {
bool same_name(std::string_view a, std::string_view b) noexcept {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
    if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
    if (x != y) return false;
  }
  return true;
}
}  // namespace

bool running(std::span<const ProcessEntry> list, std::string_view exe_name) noexcept {
  return std::any_of(list.begin(), list.end(), [&](const ProcessEntry& p) { return same_name(p.name, exe_name); });
}

std::optional<ErrorCode> preflight_failure(const PreflightFacts& f, bool check_steam, bool inject) {
  if (check_steam && !f.steam_running) return ErrorCode::E02;
  if (f.eac_running) return ErrorCode::E04;
  if (f.game_running) return ErrorCode::E03;
  if (inject && !f.dll_ours) return ErrorCode::E08;
  return std::nullopt;
}

std::vector<std::string> proxy_dlls_beside(const std::vector<std::string>& files_in_game_dir) {
  std::vector<std::string> out;
  for (const char* proxy : {"dinput8.dll", "winhttp.dll", "dxgi.dll"}) {
    for (const std::string& f : files_in_game_dir) {
      if (same_name(f, proxy)) {
        out.emplace_back(proxy);
        break;
      }
    }
  }
  return out;
}

}  // namespace en::launcher
