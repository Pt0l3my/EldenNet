#pragma once
// eldennet.dll's own parts (win/), shared between its files: the process-wide state T1 makes once and never destroys
// (R14: the DLL holds no object with a destructor that runs at detach, and no static with a dynamic initialiser), the
// log, the paths, and the threads' entries. Windows and clang-cl only.
//
// Threads (ARCH §1.2): T1 init (boot.cpp), T2 the reader (boot.cpp: a thread of our own that steps the task; the game's
// own thread never runs our code in v1, O101), T6 the core
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
#include "en/llm/edit.hpp"
#include "en/llm/secret.hpp"
#include "en/overlay/space.hpp"
#include "en/gal/discovery.hpp"
#include "en/gal/native_sys.hpp"
#include "en/gal/task.hpp"
#include "en/proto/command.hpp"
#include "en/proto/content_view.hpp"
#include "en/proto/lifecycle.hpp"
#include "en/proto/llm.hpp"
#include "en/proto/ui.hpp"
#include "en/proto/watch.hpp"
#include "en/shell/logline.hpp"
#include "en/shell/rings.hpp"
#include "en/shell/settings.hpp"
#include "en/shell/spikerun.hpp"

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
// T6: the next line T11 wrote, for the pages' logs stream (System › Logs); false when none waits.
bool take_log_tail(en::shell::LogEntry& out) noexcept;
// T1, before the core starts: where the pages' settings edits go (settings.toml), and whether `probe` is allowed there.
void set_settings_file(const std::wstring& path, bool probe_build) noexcept;
// T6: every edit the pages made this session, for T11 to write to settings.toml (an older batch not yet written goes).
void post_settings_write(std::vector<en::shell::SettingsEdit> edits) noexcept;
// One change of the AI pages to providers.toml or secrets.toml, for T11 to make on the file as it is then (read again,
// edited by en-llm's edit.hpp, written beside it and renamed over; secrets.toml with an access list for this user
// alone, ARCH §11.5). A key travels as a Secret and is never logged.
struct AiWrite {
  enum class File : std::uint8_t { Providers, Secrets };
  File file = File::Providers;
  en::llm::ProvidersEdit edit;            // Providers
  std::string name;                       // Secrets: the key's name
  std::optional<en::llm::Secret> key;     // Secrets: the key, or none to remove it
  static AiWrite providers(en::llm::ProvidersEdit edit);
  static AiWrite secret(std::string name, std::optional<en::llm::Secret> key);
};
// T1: the folder providers.toml and secrets.toml live in (%APPDATA%\EldenNet\config).
void set_ai_folder(const std::wstring& config) noexcept;
// T6: queued in order; T11 makes them in that order.
void post_ai_write(AiWrite write) noexcept;
// One of the profile editor's file changes (profiles.cpp; NPC §4.7's save path, C17): T11 writes a player's layer
// beside it and renames it over it, the previous version kept as <file>.bak; removes it (the file becomes the .bak); or
// reads the .bak back. Relative paths under the overrides folder alone, never a link out of it.
struct ProfileWrite {
  enum class Op : std::uint8_t { Write, Remove, ReadBak };
  Op op = Op::Write;
  std::wstring folder;  // the overrides folder
  std::string rel;      // "layers/persona-<id>.toml"
  std::string text;     // Write: the file's new text
  std::uint64_t token = 0;
};
struct ProfileWritten {
  std::uint64_t token = 0;
  bool ok = false;
  std::string text;  // ReadBak: the .bak's text
};
// T6: false when eight wait already (C17's capacity: the command is answered busy).
bool post_profile_write(ProfileWrite& write) noexcept;
// T6: the next write T11 made; false when none waits.
bool take_profile_written(ProfileWritten& out) noexcept;

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
// The same with an access list that grants this user alone (no entry inherited from the folder): secrets.toml.
bool write_private(const std::wstring& path, std::string_view text);
bool make_dirs(const std::wstring& path);
// A visible top-level window of this process (ARCH §3.2's WaitWindow).
[[nodiscard]] bool main_window() noexcept;
// The thread that owns that window (the game's main thread), or 0 (spike.10, WHY§289).
[[nodiscard]] DWORD main_window_thread() noexcept;
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
void game_thread(std::uint32_t tid) noexcept;    // the game's own: its window's
void reader_thread(std::uint32_t tid) noexcept;  // T2, EldenNet's reader
void init_thread(std::uint32_t tid) noexcept;
[[nodiscard]] en::proto::watch::Spike spike() noexcept;
[[nodiscard]] bool crash_requested() noexcept;
// The launcher's --read: what it asked (before it loaded us), and EldenNet's answer in the same field.
[[nodiscard]] en::proto::watch::ProbeRequest probe_request() noexcept;
void probe_answer(en::proto::watch::ProbeRequest answer) noexcept;
// The read round's plan as the launcher wrote it (watch.hpp's kPlan*); 0 with no block. The round reads it once.
[[nodiscard]] std::uint32_t probe_plan() noexcept;
// What the round gathered, task by task, for the launcher's panel (watch.hpp's kGather*); nothing with no block.
void probe_gathered(std::uint32_t word) noexcept;
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
struct AiFilesRead;  // ai.cpp: providers.toml, models.toml and secrets.toml as T1 read them, and the transport
struct ContentRead;  // conversation.cpp: the overrides folder and the realm, as T1 read them
struct SpikeState;   // spike.cpp: an S0 run's report, made by T1 at Safety in such a run
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
  bool build_supported = false;     // gal/builds.toml knows the build (the gate passed)
  bool settings_refused = false;    // settings.toml was refused at Settings: the defaults are in use
  bool logger_started = false;      // T11
  std::atomic<bool> watcher_started{false};  // T10, set by T1 once it is made
  std::atomic<bool> recording{false};          // T6's recorder writes (debug.record_sessions)
  std::atomic<std::uint64_t> recorded{0};      // its lines
  std::atomic<OverlayLink*> overlay{nullptr};  // T1 publishes it once connected; T6 alone uses it after
  std::atomic<AiFilesRead*> ai_files{nullptr};  // T1 publishes it on the main path; T6 alone uses it after
  std::atomic<ContentRead*> content{nullptr};   // T1 publishes it on the main path; T6 takes it once
  std::string content_notice;                    // T6's: the content's first problem in words, for one notice
  en::proto::ContentView content_view;           // T6's: Content packs' projection, each load recorded (en-core's)
  std::atomic<SpikeState*> spike{nullptr};       // an S0 run's (spike.cpp); null in every other start
  // T6's: the pages' changes of a behaviour key (features.*, range.*; WHY§476), for the core after overlay_tick.
  std::vector<en::shell::SettingsEdit> behaviour_edits;
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
// presses, the F11 view, and what the player did on the conversation's surfaces (the talk key, Tab, the box's input),
// appended to `ui` for the core. `latest` is the newest snapshot the core judged, or null before the first.
void overlay_tick(Shell& s, const en::core::Core& core, const en::proto::WorldSnapshot* latest, std::uint64_t tripped,
                  std::uint64_t now, std::vector<en::proto::UiEvent>& ui) noexcept;
// T6, after the core took those events: the newest model drawn by the conversation's surfaces, and the events that
// raised (PageShown, Close) for the core; nothing without GraceUI.
std::vector<en::proto::UiEvent> overlay_show(Shell& s, const en::proto::UiModel& model, std::uint64_t now) noexcept;
// T6: GraceUI's wait handle for EldenNet's events (ABI 1.5), or null before the connect or without it.
[[nodiscard]] HANDLE overlay_wake(Shell& s) noexcept;
// T6: one of the core's changes, by its words (static ones: an enum's name), for the view's list.
void overlay_change(const char* what, const char* to, std::uint64_t at_ms) noexcept;
// T6: one of Augur's events of the step, for the view's list, as Augur's words write it (no name, no handle).
void overlay_event(const en::augur::Event& ev) noexcept;
// T6: how many of Augur's events the view's list has taken, for the status line.
std::uint64_t overlay_events_seen() noexcept;

// ---- The AI pages (ai.cpp; PAGES §4.6.2) ----
// T1, on the main path after Safety: providers.toml and secrets.toml from the config folder, models.toml from the
// DLL's content\eldenring, settings.toml's text (its budget), and the WinHTTP transport, handed to T6.
void read_ai_files(Shell& s, const std::optional<std::string>& settings_text) noexcept;
// T6, each wake before the space's step: the handle's events taken, the runs advanced, the AI pages' facts (null
// until T1 has read the files, and on the banner path).
const en::overlay::AiFacts* ai_tick(Shell& s, bool gate_open, en::overlay::Space& space) noexcept;
// T6, each wake: the handle's gate (C15), its events taken, the self-test's and the model list's runs advanced, and
// the conversation's events (its ids from en-core's kConversationRequests) appended to `conversation` for the core.
void ai_pump(Shell& s, bool allowed, std::vector<en::proto::LlmEvent>& conversation) noexcept;
// T6: a request of the core's to the handle (made at the first that needs it); false if it was not accepted (the
// core's own timers then end the reply). Logged by its id and the answer alone.
bool ai_submit(Shell& s, en::proto::LlmRequest request) noexcept;
void ai_cancel(Shell& s, en::proto::RequestId id) noexcept;
// T6: the provider a slot's first model is on (a slot that borrows, its lender's), or empty with none: the draft
// helper's, which its button names (NPC §4.7).
[[nodiscard]] std::string ai_slot_provider(Shell& s, std::string_view slot) noexcept;
// T6: the model client's health as the core's ladder reads it (L3 and L4 need the dialogue slot usable).
[[nodiscard]] en::proto::NetHealth ai_health(Shell& s) noexcept;
// T6: a change of the space's `ai.preset` or a job's model (`slots.<slot>.model`), made and written to providers.toml.
void ai_change(Shell& s, en::overlay::Space& space, en::overlay::Space::Change& change) noexcept;
// T6: a request the space took (selftest.run, models.list, providers.add, providers.remove, providers.key); a key's
// value is taken out of the request and the request's copy overwritten.
void ai_request(Shell& s, en::overlay::Space& space, en::overlay::AiRequest& request) noexcept;

// ---- The conversation (conversation.cpp; WHY§364, part C) ----
// T1, on the main path: the overrides folder (%APPDATA%\EldenNet\overrides\) and the realm (the game's, or the
// tests' Eval in a probe build alone), handed to T6.
void read_content(Shell& s) noexcept;
// T6, each wake: the content loaded into the core once T1 has read it; true on the wake that did.
bool content_tick(Shell& s, en::core::Core& core) noexcept;
// T6, after each step of the core: its requests submitted, its cancels made, its commits and its ending logged as
// words and numbers.
void conversation_out(Shell& s, en::core::Outputs& out) noexcept;

// ---- The profile editor (profiles.cpp; S7.2.b, NPC §4.7) ----
// The draft helper's request ids, below the conversation's (en-core's kConversationRequests): ai_pump hands their
// events to profiles_llm_event.
inline constexpr en::proto::RequestId kDraftRequests = en::proto::RequestId{1} << 44;
// T6, once the content is loaded (content_tick): the files it was loaded from, the shipped pack's then the overrides
// folder's, and the realm, kept for the editor's checks and the loads after a write.
void profiles_loaded(Shell& s, const en::core::Core& core, std::vector<en::core::ContentFile> files,
                     en::core::Realm realm) noexcept;
// T6, each wake before the space's step: the Characters pages' facts (null before the content is loaded).
const en::overlay::CharacterFacts* profiles_facts(Shell& s, const en::core::Core& core, bool gate_open) noexcept;
// T6: a command of the editor's the space took: made (a check answered at once, a write or a draft later).
void profiles_request(Shell& s, const en::core::Core& core, en::overlay::Space& space,
                      en::overlay::ProfileRequest request) noexcept;
// T6 (ai_pump): an event of a draft's request.
void profiles_llm_event(Shell& s, en::proto::LlmEvent event) noexcept;
// T6, each wake after the overlay's: T11's writes answered, a draft whose page was left cancelled, and the content
// loaded again after a write once no conversation is open.
void profiles_tick(Shell& s, en::core::Core& core) noexcept;
// T6: EldenNet's space (overlay.cpp's), or null before its first wake.
[[nodiscard]] en::overlay::Space* overlay_space() noexcept;

// ---- The S0 runs (spike.cpp; DESIGN §16.1's S0-A to S0-D, roadmap S10.1.b) ----
// The report of an S0 run, eldennet-spike.json in the run's folder (en_shell's spikerun.hpp), and what T1 measures for
// it. Each does nothing outside an S0 run whose folder the launcher named.
// T1 at Safety: the run's folder (ELDENNET_SPIKE_DIR, only the launcher's folder for this spike in our spikes folder).
void spike_begin(Shell& s) noexcept;
// T1 after Discover: its counts and the build's table as it checked it; `d` null when gal/'s files were refused.
void spike_discovered(Shell& s, const en::gal::Discovery* d, std::uint32_t passes) noexcept;
// T1, S0-D in a probe build, before the reader or where the start stops short of it: guarded copies forced to fault,
// by each copy primitive (the SEH copy at a page of our own with no access, ReadProcessMemory at the null page).
void spike_trial(Shell& s) noexcept;
// T1, S0-D in a probe build, before the reader: the group the reader's faults are injected into (settings.toml's,
// else the first optional group this start reads).
void spike_inject(Shell& s, const en::gal::Discovery& d) noexcept;
// T1 at the reader's first reading.
void spike_reading(Shell& s) noexcept;
// T1 at its terminal state, before it publishes it: the table checked again, then the report written.
void spike_stopped(Shell& s, en::proto::watch::InitState state, std::optional<en::core::BannerReason> banner,
                   en::shell::SpikeWaits waits) noexcept;
// T6, every 10 s once T1 has stopped: the reader's counts, and the report again.
void spike_tick(Shell& s, std::uint64_t tripped) noexcept;

// ---- T1 (boot.cpp) and the read-only round (probe_read.cpp) ----
unsigned long init_main(void* module) noexcept;
// The read-only round (probe_read.cpp; Lodestar's step L5): on T1 after Connect, in a probe build with
// `probe = { round = "read" }`. It calls nothing of the game and registers nothing; T1 stays as its reporter. A release
// build has none.
void run_read(Shell& s, const en::proto::Expected<en::gal::Gated, en::gal::GateFailure>* gated) noexcept;

// The file a log, a record or a report may name: the file name only, made a word (anything else is "redacted").
[[nodiscard]] std::string word_of(std::wstring_view name);

}  // namespace eldennet
