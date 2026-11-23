#pragma once
// What the crash watcher keeps of a session (O71): how the game ended (its exit code, and EldenNet's crash record from
// the watch block when its filter ran), EldenNet's last status, the modules loaded, the Marks; and the S0 spikes' runs
// and their tally. Every record is numbers, fixed words and module file names (no folder): no dump, no path, no user
// or account name, no text of the game's or the player's. A record of a crash, or of any other end that was not
// clean, is kept for the next start to explain.
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/launcher/texts.hpp"
#include "en/proto/watch.hpp"

namespace en::launcher {

enum class EndKind : std::uint8_t {
  Clean,   // exit code 0
  Exit,    // another exit code that is no exception's
  Crash,   // an exception's code, or EldenNet's crash filter signalled
  Closed,  // the launcher closed it (a spike's run, or a failed injection)
};
[[nodiscard]] const char* to_string(EndKind k) noexcept;  // "clean", "exit", "crash", "closed"

// The exit code the launcher ends a game with when it closes it: "ENLC".
inline constexpr std::uint32_t kClosedByLauncher = 0x454E4C43u;

[[nodiscard]] EndKind classify_end(std::uint32_t exit_code, bool crash_signalled) noexcept;

// EldenNet's status, copied out of the watch block.
struct DllStatus {
  proto::watch::InitState init = proto::watch::InitState::None;
  std::uint32_t phase = 0, level = 0, banner = 0, flags = 0;
  std::uint32_t items_resolved = 0, items_total = 0, discovery_passes = 0, ready_ms = 0, task_us_p99 = 0;
  std::uint64_t snapshots = 0, faults = 0, tripped = 0;
};
[[nodiscard]] DllStatus copy_status(const proto::watch::Block& b) noexcept;

struct CrashInfo {
  std::uint32_t code = 0;
  proto::watch::ThreadKind thread = proto::watch::ThreadKind::Unknown;
  bool chained = false;
  std::uint64_t offset = 0;
  std::string module;
};
// The crash record, once EldenNet's filter has written it (Written or Taken).
[[nodiscard]] std::optional<CrashInfo> copy_crash(const proto::watch::Block& b);

// A module's file name made safe for a record: no folder, at most 64 bytes, ASCII letters, digits and `._-+ ` only.
[[nodiscard]] std::string safe_module_name(std::string_view name);

struct SessionRecord {
  std::string launcher_version;
  std::string build;           // builds.toml's key, or "unknown"
  std::string started, ended;  // YYYYMMDD-HHMMSS, local time
  std::string mode;            // "suspended", "window", "title", "vanilla" or "watch"
  proto::watch::Spike spike = proto::watch::Spike::None;
  std::uint32_t exit_code = 0;
  EndKind kind = EndKind::Clean;
  std::uint64_t session_ms = 0;
  bool dll_attached = false;
  DllStatus status;
  std::optional<CrashInfo> crash;
  std::vector<std::string> modules;   // file names, as last listed
  bool modules_at_crash = false;      // listed while the crash filter waited, not before
  std::uint64_t modules_age_ms = 0;   // how long before the end they were listed
  std::vector<std::uint64_t> marks;   // the Marks' mono ms
  std::uint32_t connections_max = 0;  // s0c: the game's non-loopback connections, the most at once (no addresses)
  bool explained = false;             // shown at a later start
};

[[nodiscard]] bool worth_keeping(const SessionRecord& r) noexcept;  // a crash, or an exit with a code
[[nodiscard]] nlohmann::json to_json(const SessionRecord& r);
// The next start's explanation of a kept record not yet explained; nothing for one explained already, or another kind.
[[nodiscard]] std::optional<std::string> explanation(const nlohmann::json& record);

// ---- The S0 spikes' runs ----

enum class RunOutcome : std::uint8_t {
  Ready,         // T1 reached Ready
  Banner,        // T1 stopped at a banner (L1)
  Inert,         // the task went inert
  Exit,          // T1 exited (a second copy, or Off)
  GameEnded,     // the game ended before T1 stopped
  Timeout,       // T1 had not stopped within the run's limit
  NoDll,         // EldenNet never wrote its status
  StartFailed,   // the game could not be started (E09)
  InjectFailed,  // the DLL could not be loaded (E10)
};
[[nodiscard]] const char* to_string(RunOutcome o) noexcept;
[[nodiscard]] RunOutcome outcome_of(proto::watch::InitState last) noexcept;  // for a terminal state

struct RunResult {
  std::uint32_t run = 0;
  std::string mode;  // when the DLL was loaded: "suspended", "window", "title", or "vanilla" (spike-report's modes)
  RunOutcome outcome = RunOutcome::NoDll;
  DllStatus status;
  EndKind end = EndKind::Clean;
  std::uint32_t exit_code = 0;
  bool record_kept = false;   // the watcher kept a record of its end
  bool crash_filter = false;  // EldenNet's crash filter wrote its record
  std::uint64_t run_ms = 0;
  std::uint32_t connections_max = 0;
  // An S0 run (s0.hpp): its folder's number (spikes\<word>-<n>; 0 for none), whether eldennet.dll's report of the run
  // was read there, what the start waited for when it stopped short of its reading, and whether the build's table
  // passed S0-A's rule (true where there was no report or no table).
  std::uint32_t folder = 0;
  bool report = false;
  std::string waits = "none";
  bool table_ok = true;
};

struct Tally {
  std::uint32_t runs = 0, ready = 0, banner = 0, inert = 0, exit = 0, game_ended = 0, timeout = 0, no_dll = 0,
                start_failed = 0, inject_failed = 0, crashes = 0, records = 0, filter_records = 0;
  // S0 runs: eldennet.dll's reports read, the runs whose start waited for something, and those whose table failed.
  std::uint32_t reports = 0, waiting = 0, table_failed = 0;
};
[[nodiscard]] Tally tally(std::span<const RunResult> runs) noexcept;
// DESIGN §16.1's rule for the launcher's part of each spike: s0a, every run reaching Ready or L1 (a banner, inert),
// none crashing, and every run's table, where eldennet.dll's report holds one, passing its checks at the start and
// again once the start stopped; s0d-crash, every forced crash recorded; s0b, s0c and s0d, the session not crashing;
// s0e, s0e_alone and s4 likewise (their measurements are GraceUI's observer's report). A start that waits for
// something (an incomplete layout) is never counted against a spike.
[[nodiscard]] bool passed(proto::watch::Spike spike, const Tally& t) noexcept;
[[nodiscard]] nlohmann::json to_json(const RunResult& r, proto::watch::Spike spike);
[[nodiscard]] nlohmann::json to_json(const Tally& t, proto::watch::Spike spike);
[[nodiscard]] std::string tally_line(const Tally& t);  // "runs=3 ready=3 banner=0 ..."

// ---- Pre-flight (ARCH §4.3) ----

struct ProcessEntry {
  std::string name;  // the image's file name
  std::uint32_t pid = 0;
  std::uint32_t parent = 0;
};
[[nodiscard]] bool running(std::span<const ProcessEntry> list, std::string_view exe_name) noexcept;

struct PreflightFacts {
  bool steam_running = false;
  bool eac_running = false;
  bool game_running = false;
  bool dll_ours = false;  // the DLL exists and its version resource reads ProductName = EldenNet
};
// The first check that fails, in ARCH §4.3's order (E02, E04, E03, E08), or nothing.
[[nodiscard]] std::optional<ErrorCode> preflight_failure(const PreflightFacts& f, bool check_steam, bool inject);

// The other proxy DLLs beside the game's executable, by name (information only, never a refusal).
[[nodiscard]] std::vector<std::string> proxy_dlls_beside(const std::vector<std::string>& files_in_game_dir);

}  // namespace en::launcher
