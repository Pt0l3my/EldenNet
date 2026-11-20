#pragma once
// eldennet.dll's own parts (win/), shared between its files: the process-wide state T1 makes once and never destroys
// (R14: the DLL holds no object with a destructor that runs at detach, and no static with a dynamic initialiser), the
// log, the paths, and the threads' entries. Windows and clang-cl only.
//
// Threads (ARCH §1.2): T1 init (boot.cpp), T2 the game thread (the task, through en-gal's trampoline), T6 the core
// (shell.cpp), T10 the watcher (watcher.cpp: the anti-cheat and ersc.dll checks every 60 s) and T11 the log writer
// (logger.cpp). No other thread is made. T1 connects to GraceUI (overlay.cpp, en-overlay) and T6 alone calls it after.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/core.hpp"
#include "en/gal/discovery.hpp"
#include "en/gal/native_sys.hpp"
#include "en/gal/task.hpp"
#include "en/proto/command.hpp"
#include "en/proto/lifecycle.hpp"
#include "en/proto/watch.hpp"
#include "en/shell/logline.hpp"
#include "en/shell/rings.hpp"
#include "en/shell/settings.hpp"

namespace eldennet {

inline constexpr std::array<std::uint16_t, 3> kVersion = {0, 1, 0};
#ifdef EN_DLL_VERSION_LABEL
inline constexpr std::string_view kVersionLabel = EN_DLL_VERSION_LABEL;  // EN_VERSION_LABEL: "0.1.0-spike.1"
#else
inline constexpr std::string_view kVersionLabel = "dev";
#endif
#if EN_PROBE
inline constexpr bool kProbeBuild = true;
#else
inline constexpr bool kProbeBuild = false;
#endif

// ---- Time: QPC, the snapshots' own clock ----
[[nodiscard]] std::uint64_t mono_ms() noexcept;   // since the machine's start, as the snapshots and the Marks
[[nodiscard]] std::uint64_t since_ms() noexcept;  // since T1's start (the log's t=)
void set_start() noexcept;

// ---- The log (logger.cpp): any thread pushes, T11 writes. Lines before Safety are kept and written once the
// logger starts, so a second copy never writes to the shared log (ARCH §3.2). ----
void log(const en::shell::Line& line, en::shell::LogLevel level = en::shell::LogLevel::Info) noexcept;
void set_log_level(en::shell::LogLevel level) noexcept;
// Starts T11 on `folder`\eldennet.log (rotated past 1 MiB, one older kept).
bool start_logger(const std::wstring& folder) noexcept;
[[nodiscard]] std::uint64_t log_dropped() noexcept;

// ---- Paths and the process (platform.cpp) ----
struct Paths {
  std::wstring dll;       // the DLL's own file
  std::wstring dll_dir;   // its folder: gal\ lies beside it (ARCH §3.1)
  std::wstring gal;
  std::wstring data;      // %APPDATA%\EldenNet, or ELDENNET_APPDATA\EldenNet in development and tests
  bool data_overridden = false;
  std::wstring data_root;  // what `data` was made from (the launcher's --appdata for the watcher)
};
[[nodiscard]] Paths find_paths(HMODULE module);
[[nodiscard]] std::optional<std::string> read_text(const std::wstring& path, std::size_t max_bytes);
bool write_text(const std::wstring& path, std::string_view text);  // to path.part, then renamed over path
bool make_dirs(const std::wstring& path);
// A visible top-level window of this process (ARCH §3.2's WaitWindow).
[[nodiscard]] bool main_window() noexcept;
// The process's modules' file names, and whether one is ours by its marker (another module exporting
// eldennet_instance_marker: R16's second copy).
[[nodiscard]] std::vector<std::wstring> module_names();
[[nodiscard]] std::size_t other_copies(HMODULE ours);
[[nodiscard]] std::wstring parent_name();
bool start_process(const std::wstring& exe, std::wstring command_line);

// ---- The watch block (block.cpp): the launcher's crash watcher and its dev console (O71, O90) ----
namespace block {
bool attach() noexcept;                // the launcher's block for this process, if any
bool start_watcher(const Paths& paths) noexcept;  // under another loader: eldennet-launcher.exe --watch <pid>
[[nodiscard]] bool attached() noexcept;
void install_filter() noexcept;        // once the game has started (WHY§172)
void state(en::proto::watch::InitState s) noexcept;
void ready_ms(std::uint32_t ms) noexcept;
void items(std::uint32_t resolved, std::uint32_t total, std::uint32_t passes) noexcept;
void counts(std::uint64_t snapshots, std::uint64_t faults, std::uint64_t tripped, std::uint32_t p99) noexcept;
void core(std::uint32_t phase, std::uint32_t level, std::uint32_t banner, std::uint32_t flags) noexcept;
void game_thread(std::uint32_t tid) noexcept;
void init_thread(std::uint32_t tid) noexcept;
[[nodiscard]] en::proto::watch::Spike spike() noexcept;
[[nodiscard]] bool crash_requested() noexcept;
// The launcher's --probe: what it asked (R1, before it loaded us), and EldenNet's answer in the same field.
[[nodiscard]] en::proto::watch::ProbeRequest probe_request() noexcept;
void probe_answer(en::proto::watch::ProbeRequest answer) noexcept;
// The Marks: the block's ring, or EldenNet's own with no block. The launcher's Pause stamps them until EldenNet's own
// Mark, a GraceUI hotkey on the same key, takes the ring over (watch.hpp's MarkSource).
[[nodiscard]] std::uint32_t marks() noexcept;
[[nodiscard]] std::uint64_t mark_ms(std::uint32_t n) noexcept;  // mark n (from 1), while the ring holds it
[[nodiscard]] en::proto::watch::MarkSource mark_source() noexcept;
// EldenNet's GraceUI Mark is registered: asks for the ring; true once it is EldenNet's (at once when nobody held the
// key; after the launcher's hand-over otherwise).
bool take_marks() noexcept;
// A Mark of EldenNet's own: its number, or 0 while the ring is not EldenNet's.
std::uint32_t add_own_mark(std::uint64_t at_ms) noexcept;
// What T1 last reported (kept with no block too).
[[nodiscard]] en::proto::watch::InitState init_state() noexcept;
struct Items {
  std::uint32_t resolved = 0;
  std::uint32_t total = 0;
  std::uint32_t passes = 0;
};
[[nodiscard]] Items items_seen() noexcept;
}  // namespace block

// ---- The task's channels (shell.cpp): C1 to C5, and the core thread ----
class Io;
struct OverlayLink;  // overlay.cpp: en-overlay's connection and what T6 last showed through it
struct Shell {
  en::shell::Settings settings;
  Paths paths;
  HMODULE module = nullptr;
  en::gal::NativeSys* sys = nullptr;
  en::gal::GalTask* task = nullptr;  // set once registered
  std::atomic<bool> task_set{false};
  Io* io = nullptr;
  HANDLE wake = nullptr;              // the core's one wake signal
  std::atomic<bool> anticheat{false};  // C14
  std::atomic<bool> ersc{false};
  en::shell::SpscRing<en::core::BootBanner, 4> banners;  // T1 → T6
  std::array<std::uint16_t, 4> game_version{};
  bool game_version_known = false;  // from the gate, or the fingerprint of a build the gate refused
  std::string_view region = "none";
  std::atomic<OverlayLink*> overlay{nullptr};  // T1 publishes it once connected; T6 alone uses it after
};
// The one Shell, made on T1 (never destroyed), or null before.
[[nodiscard]] Shell* shell() noexcept;
Shell& make_shell();

// The task's IO over C1 to C5 (TaskIo), made on T1.
Io* make_io(Shell& s);
en::gal::TaskIo& task_io(Io& io) noexcept;
bool start_core(Shell& s) noexcept;     // T6
bool start_watcher_thread(Shell& s) noexcept;  // T10
void push_banner(Shell& s, en::core::BannerReason reason, std::string_view detail) noexcept;

// ---- GraceUI (overlay.cpp; en-overlay): the connect on T1, then each wake of T6 ----
// Connect or ConnectB (ARCH §3.2): find GraceUI, connect, and, connected, publish the link for T6. One log line; a
// failed connect leaves EldenNet headless (ARCH §10.3). `banner_path`: ConnectB, after a refused gate or anti-cheat.
void connect_overlay(Shell& s, bool banner_path) noexcept;
// T6, after each step: the banner and the badge as notices, the world-state claims, the Mark's hotkey and its
// presses, and the F11 view. `latest` is the newest snapshot the core judged, or null before the first.
void overlay_tick(Shell& s, const en::core::Core& core, const en::proto::WorldSnapshot* latest, std::uint64_t tripped,
                  std::uint64_t now) noexcept;
// T6: one of the core's changes, by its words (static ones: an enum's name), for the view's list.
void overlay_change(const char* what, const char* to, std::uint64_t at_ms) noexcept;

// ---- T1 (boot.cpp) and R1 (probe_r1.cpp) ----
unsigned long init_main(void* module) noexcept;
// R1's round on T1, in a probe build (probe_r1.cpp): T1 stays its reporter for the session. A release build has none.
void run_r1(Shell& s, const en::proto::Expected<en::gal::Gated, en::gal::GateFailure>* gated) noexcept;

// The file a log, a record or a report may name: the file name only, made a word (anything else is "redacted").
[[nodiscard]] std::string word_of(std::wstring_view name);

}  // namespace eldennet
