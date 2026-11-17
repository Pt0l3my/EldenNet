#pragma once
// eldennet-launcher's Windows part: what its files share. The portable logic lives in en_launcher_core; here are the
// processes, the injection, the watch block's mapping, the dialogs and the program's flow (ARCH §4).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/launcher/config.hpp"
#include "en/launcher/options.hpp"
#include "en/launcher/record.hpp"
#include "en/launcher/texts.hpp"
#include "en/proto/watch.hpp"

namespace en::launcher::win {

namespace fs = std::filesystem;

// ---- Text and time (out.cpp) ----

[[nodiscard]] std::wstring widen(std::string_view utf8);
[[nodiscard]] std::string narrow(std::wstring_view wide);
[[nodiscard]] std::uint64_t mono_ms();    // QPC in ms: the snapshots' clock (watch.hpp's mono_ms)
[[nodiscard]] std::string local_stamp();  // YYYYMMDD-HHMMSS, local time

// The launcher's lines: "LAUNCHER <event> key=value … t=<ms>", to standard output when there is one (a console, or a
// pipe as under the tests) and to launcher.log. They hold numbers and fixed words only: never a path, a user name or
// an id (PV rules).
void open_output(bool console);
void open_log(const fs::path& logs_dir);
void say(const char* fmt, ...);
// A plain sentence for the owner, on the console only (R1's steps, --probe): never in the log, which keeps to say()'s
// numbers and words.
void console_line(std::string_view text);

// ---- The context of one launcher run (main.cpp) ----

struct Paths {
  fs::path exe_dir;  // the launcher's folder
  fs::path appdata;  // %APPDATA%, or --appdata
  fs::path en;       // %APPDATA%\EldenNet
  fs::path config;   // …\config
  fs::path logs;     // …\logs
  fs::path crashes;  // …\crashes
  fs::path spikes;   // …\spikes
  fs::path backups;  // …\save-backups
  fs::path saves;    // %APPDATA%\EldenRing
  fs::path gal;      // gal/ beside the launcher, or --gal
};

// ---- The system (sys.cpp) ----

[[nodiscard]] std::optional<fs::path> appdata_folder();
[[nodiscard]] std::optional<fs::path> steam_folder();  // the registry's Steam path
[[nodiscard]] bool steam_running(const std::vector<ProcessEntry>& processes);
[[nodiscard]] std::vector<ProcessEntry> list_processes();
// The game's modules by file name; empty when the process cannot be listed (yet).
[[nodiscard]] std::vector<std::string> list_modules(DWORD pid);
[[nodiscard]] bool module_loaded(DWORD pid, const fs::path& dll);
// A visible top-level window of the process (its main window), or null.
[[nodiscard]] HWND main_window(DWORD pid);
// WM_CLOSE to each of the process's top-level windows; answers how many there were.
std::size_t close_windows(DWORD pid);
// The game's TCP connections to anywhere but this PC (s0c): a count, no address kept.
[[nodiscard]] std::optional<std::uint32_t> remote_connections(DWORD pid);
[[nodiscard]] std::vector<std::string> files_in(const fs::path& dir);

// ---- Processes (inject.cpp) ----

struct Started {
  PROCESS_INFORMATION pi{};
  DWORD error = 0;
  [[nodiscard]] bool ok() const { return pi.hProcess != nullptr; }
};
// The game, suspended, in its own folder; with the appid in its environment for the env method. No handles inherited.
[[nodiscard]] Started start_game(const fs::path& exe, const std::vector<std::string>& args, bool env_appid,
                                 std::uint32_t appid);

struct Injected {
  bool ok = false;
  std::string_view step;  // the step that failed (texts.hpp)
  DWORD code = 0;
};
// LoadLibraryW in the process on a thread of its own, the DLL by its absolute path (ARCH §4.6).
[[nodiscard]] Injected inject(HANDLE process, DWORD pid, const fs::path& dll);

// ---- The watch block (watch.cpp) ----

class WatchBlock {
 public:
  // Creates the block for `pid` or opens the one EldenNet created; null on failure.
  static std::optional<WatchBlock> open(DWORD pid);
  WatchBlock(WatchBlock&& o) noexcept;
  WatchBlock& operator=(WatchBlock&&) = delete;
  WatchBlock(const WatchBlock&) = delete;
  ~WatchBlock();
  [[nodiscard]] proto::watch::Block& block() const { return *block_; }
  [[nodiscard]] HANDLE crash_event() const { return crash_; }
  void ack_crash() const;
  [[nodiscard]] bool created() const { return created_; }

 private:
  WatchBlock() = default;
  HANDLE mapping_ = nullptr;
  HANDLE crash_ = nullptr;
  HANDLE ack_ = nullptr;
  proto::watch::Block* block_ = nullptr;
  bool created_ = false;
};

// What the watch loop is asked to do beyond watching (a spike's run).
struct WatchPlan {
  bool console = false;  // status lines and the Mark
  bool probe = false;    // --probe: R1's steps in plain words (the block's probe request was written before loading)
  proto::watch::Spike spike = proto::watch::Spike::None;
  std::uint32_t run_timeout_s = 0;          // > 0: end the run once T1 stops, or at this limit
  bool crash_when_ready = false;            // s0d-crash: ask for a crash once T1 is Ready
  bool count_connections = false;           // s0c
  std::optional<RemoveAfter> remove_appid;  // Window: remove our appid file once the window shows
  std::optional<AppidRecord> appid;
  std::uint32_t appid_value = 0;
};

struct WatchResult {
  DWORD exit_code = 0;
  bool crash_signalled = false;
  bool closed_by_us = false;
  bool timed_out = false;
  proto::watch::InitState last_init = proto::watch::InitState::None;
  std::uint64_t ms = 0;
  SessionRecord record;
};

// Watches the game until it ends (O71): the crash event, EldenNet's status, the Marks, the module lists.
[[nodiscard]] WatchResult watch_game(HANDLE process, DWORD pid, const WatchBlock& wb, const WatchPlan& plan);
// Closes the game: WM_CLOSE first, then TerminateProcess with kClosedByLauncher.
void close_game(HANDLE process, DWORD pid, DWORD grace_ms);

// ---- Dialogs (dialogs.cpp) ----

void set_dialogs(bool enabled, bool answer_yes);
// An error with an action (E01, E05, E07, E10): true when the player chose the action.
[[nodiscard]] bool ask(ErrorCode e, const std::string& text);
void tell_error(ErrorCode e, const std::string& text);
void tell(const std::string& title, const std::string& text);
// The first-screen notices; true when "Don't show again" was ticked.
[[nodiscard]] bool show_notices();
// E01's "Open eldenring.exe…".
[[nodiscard]] std::optional<fs::path> pick_game(const std::string& exe_name);

}  // namespace en::launcher::win
