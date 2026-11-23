#pragma once
// The fake game's own parts, shared between its files: the options, the status lines, the task hub, the world, the
// faults on request, the self-test and --describe (README.md).
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "fakegame.hpp"

namespace fakegame {

// --- Options (the command line; README.md lists each) -------------------------------------------------------------------
enum class MoveMode : std::uint8_t { Poison, Free, Keep };
enum class RegisterMode : std::uint8_t { Normal, Fault, Ignore };
enum class Crash : std::uint8_t {
  None, AvRead, AvWrite, AvExec, DivZero, Illegal, StackOverflow, Breakpoint, FastFail, Terminate
};
enum class Handled : std::uint8_t { Seh, Cxx };
enum class LoadMoment : std::uint8_t { Boot, Window, Ready, Title, World };

struct DllLoad {
  LoadMoment moment = LoadMoment::Boot;
  std::wstring path;
  bool done = false;
};

struct Options {
  std::uint64_t frames = 600;  // then a clean exit; 0 with `forever`
  bool forever = false;
  std::uint32_t fps = 60;      // 0: as fast as it goes
  bool window = true;
  std::uint32_t boot_delay_ms = 0;     // before the window
  std::uint32_t statics_delay_ms = 0;  // between the window and the statics
  bool field_mirror = false;           // a second word of the data holds the FieldPlot's address (a copy of one address)
  bool slot_by_state = false;          // the run warden's save slot reads -1 outside the world (as the game's does, by
                                       // our notes) and 2 in it; without this it reads 2 always
  std::uint32_t first_slot = 2;        // with slot_by_state: the slot it reads in the world before it goes back to its
                                       // title (another save was loaded first); after that it reads 2
  std::uint32_t retitle_at_s = 0;      // seconds into the world at which the game goes back to its title, once; 0 never
  std::uint32_t retitle_for_s = 10;    // and how long it stays there before it loads the world again
  std::uint32_t clock_rate = 60;       // game seconds a second of real time in the world (a game minute a second)
  bool day_book = false;               // the first decoy static holds a plain block that keeps the clock's packed word
                                       // twice (0x10 and 0x18 bytes in): a clock behind a static no lead names
  bool pass_time = false;              // the clock jumps twice in the world: five hours 14 s in, three hours 26 s in
  bool flag_set = false;               // a flag of the store's second block is set 30 s into the world
  std::uint64_t title_frames = 30;
  std::uint64_t load_frames = 10;
  bool stay_title = false;
  std::uint64_t reload_every = 0;  // world frames between loading screens; 0 never
  std::uint32_t actors = 6;        // besides the main player; at most kMaxNear - 1
  bool online = false;
  std::uint64_t online_at = 0;     // frame at which the session turns online; 0 never
  std::uint64_t hitch_every = 0;
  std::uint32_t hitch_ms = 50;
  std::uint64_t stall_at = 0;      // 0 never
  std::uint32_t stall_ms = 0;      // 0 forever
  std::uint64_t move_every = 0;    // world frames between moves of every singleton and the player; 0 never
  MoveMode move_mode = MoveMode::Poison;
  std::uint32_t shift_groups = 0;
  RegisterMode register_mode = RegisterMode::Normal;
  Crash crash = Crash::None;
  std::uint64_t crash_at = 1;
  bool crash_on_worker = false;
  std::uint64_t handled_every = 0;
  Handled handled_kind = Handled::Seh;
  bool game_filter = false;          // install the program's own unhandled-exception filter at boot
  std::uint64_t game_filter_at = 0;  // install it again (replacing any other) at this frame; 0 never
  bool game_filter_continue = false; // the filter answers "continue search" instead of "execute handler"
  std::vector<DllLoad> loads;
  bool selftest_task = false;
  bool selftest_swapped = false;
  std::uint32_t selftest_object_size = static_cast<std::uint32_t>(FG::kTaskFieldsSize);
  std::int32_t selftest_group = -1;  // -1: the stand-in group
  std::uint64_t press_pause_at = 0;  // the player presses Pause (the launcher's Mark, O90) at this frame; 0 never
  // Part 12: GraceUI in the fake game (present.cpp). --d3d12 presents every frame on a Direct3D 12 swap chain, so that
  // GraceUI's overlay binds it and draws; --press-f11-at presses F11 at each frame listed, the window in front and
  // holding the focus (GraceUI's hotkey, D155); --grace-report reads graceui.dll's test exports at each frame listed
  // and, with --d3d12, counts the pixels of each --grace-probe's colour in its rectangle on the frame just presented
  // (at most three probes, reported as probe, probe2 and probe3).
  // --grace-take-pause stands in for another mod: at boot, before the window, the fake game finds the GraceUI already
  // loaded (the launcher's --also) through graceui-sys, connects and registers Pause as a hotkey of its own, so that
  // EldenNet's Mark, asked for later, registers unbound (GraceUI D164).
  // --press-keys F:KEY,F:KEY... presses one of GraceUI's menu keys at each frame listed, as --press-f11-at does F11
  // (f5, next, prior, up, down, left, right, return, escape): the player in GraceUI's menu (D167, D171).
  bool d3d12 = false;
  bool grace_take_pause = false;
  std::vector<std::uint64_t> press_f11_at;
  struct KeyPress {
    std::uint64_t frame = 0;
    unsigned vk = 0;
    unsigned scan = 0;
  };
  std::vector<KeyPress> press_keys;
  // The first conversation (S11.3, part C): --calm makes the world one a conversation may start in (no boss bar, no
  // strike on the first character, no native talk's pop-up); --type-at F:TEXT,F:TEXT types each text at its frame
  // into the window, a character at a time as Unicode key presses (an IME's or Steam's text arrives so), the window in
  // front and holding the focus.
  bool calm = false;
  struct Typed {
    std::uint64_t frame = 0;
    std::wstring text;
  };
  std::vector<Typed> type_at;
  std::vector<std::uint64_t> grace_report_at;
  struct Probe {
    std::uint32_t rect[4] = {0, 0, 0, 0};  // x0, y0, x1, y1 in the back buffer's pixels
    std::uint32_t colour[3] = {0, 0, 0};
    std::uint32_t tolerance = 3;  // each channel within this of the colour (an eighth value: text's ink, say)
  };
  static constexpr std::size_t kProbes = 3;
  std::vector<Probe> grace_probes;
  std::wstring status_file;
  std::uint32_t exit_code = 0;
  bool describe = false;
  bool verbose = false;
};

// --- GraceUI in the fake game (present.cpp) ------------------------------------------------------------------------------
// The Direct3D 12 device, queue and flip-model swap chain on the window, d3d12.dll and dxgi.dll loaded by name only
// with --d3d12; false (with a status line) if they cannot be made.
bool present_start(HWND window);
// One frame: the back buffer cleared to the fake game's colour, then presented.
void present_frame();
// F11 pressed and released into the window (SendInput, the virtual key and its scan code), the window in front.
void press_key_into(HWND window, unsigned vk, unsigned scan);
// The keys (WM_KEYDOWN, WM_SYSKEYDOWN) and characters (WM_CHAR) that reached the game's own window procedure.
std::uint32_t game_keys() noexcept;
std::uint32_t game_chars() noexcept;
// --type-at: each character of `text` as a Unicode key press (KEYEVENTF_UNICODE), as press_key_into presses a key.
void type_into(HWND window, const std::wstring& text);
// What graceui.dll's test exports say (a test build of GraceUI's, GRACEUI_TEST_EXPORTS), as one status line.
void grace_report(std::uint64_t frame);
// --grace-take-pause: another mod's connection to GraceUI and its hotkey on Pause, with one status line.
void grace_take_pause();

// Parses the command line. On an error, writes the reason to `error` and answers false.
bool parse_options(int argc, wchar_t** argv, Options& out, std::string& error);
const Options& options();
void set_options(const Options& opts);

// --- Status lines: "FAKEGAME <event> key=value ...", on standard output and in the status file ------------------------
void status_open(const std::wstring& file);
void status(const char* fmt, ...);
void status_raw(const char* text, std::size_t size);  // a line already formatted, newline included
void status_flush();

// --- The task hub ---------------------------------------------------------------------------------------------------------
inline constexpr std::uint32_t kBaseGroups = 10;
inline constexpr std::uint32_t kStandinBase = 5;  // Fg_ActorSettle: positions are final there
std::uint32_t standin_group();                    // kStandinBase + the shift
void hub_create();                                // the hub singleton and its group table
// The program's own four tasks, the second made in `bench` (spike.10's call-site forms).
void hub_register_builtins(FG::TaskUnit* const tasks[4], const std::uint32_t groups[4], FgTaskBench* bench);
void hub_run_frame(std::uint64_t frame, float dt);
// Spike.12 (WHY§296): the manager's map's words, at the title and those the world's first load adds, and the call
// that adds them (on the game thread, once).
std::vector<std::uint32_t> map_title_words();
std::vector<std::uint32_t> map_world_words();
void hub_world_loaded();

struct HubStats {
  std::uint64_t builtin_runs = 0;
  std::uint32_t foreign_tasks = 0;
  std::uint64_t foreign_runs = 0;
  std::uint64_t foreign_us_max = 0;
  std::uint32_t rejected = 0;
  std::uint32_t ignored = 0;
  std::uint64_t live = 0;  // the hub's live tasks (TaskLoomFields.tasks_live): the program's own and any registered
};
HubStats hub_stats();
std::uint32_t group_count();
const char16_t* group_name(std::uint32_t index);

// --- The world ----------------------------------------------------------------------------------------------------------
enum class Phase : std::uint8_t { Boot, Title, Loading, World };
const char* phase_name(Phase p);
void world_create_singletons();  // every singleton, published in its static
void world_register_tasks();     // the program's own four tasks, through the program's own call sites
void world_frame_begin(std::uint64_t frame);  // phases, loading screens, moves, the session's turn online
Phase world_phase();
void world_move_all();
void world_publish_hinstance(HINSTANCE instance);
void world_build_for_describe();  // the actors and props, so that each class has a live vtable

struct WorldStats {
  std::uint32_t moves = 0;
  std::uint32_t loads = 0;
  std::uint32_t poisoned = 0;
  std::uint32_t freed = 0;
  std::uint32_t fd4_unset = 0;
};
WorldStats world_stats();

// Instances whose vtables --describe reports (each a live object of its class).
struct ClassSample {
  const char* name;     // e.g. "FG::Actor"
  const char* mangled;  // e.g. ".?AVActor@FG@@"
  const void* object;   // an instance; its first pointer is the class's vtable (at `offset` for a second base)
  std::uint32_t offset; // where that vtable pointer sits in the object
  bool actor;           // has FG::Actor as a base (or is it)
  const char* bases;    // its bases, comma-separated, nearest first
};
std::vector<ClassSample> class_samples();

// --- Faults on request ------------------------------------------------------------------------------------------------
const char* crash_name(Crash c);
bool handled_supported(Handled h);
[[noreturn]] void crash_now(Crash c);
void crash_on_worker(Crash c);
bool handled_exception(Handled h);  // one exception the program raises and handles itself; true if handled
void install_game_filter();
std::uint32_t handled_count();

// --- The foreign-task self-test (a stand-in for EldenNet's T1 and trampoline) -------------------------------------------
void selftest_start(DWORD game_thread_id);
struct SelftestStats {
  bool registered = false;
  bool faulted = false;
  std::uint32_t fault_code = 0;
  std::uint64_t attach = 0;
  std::uint64_t runs = 0;
  std::uint64_t queries = 0;
  std::uint64_t wrong_thread = 0;
  std::uint64_t wrong_group = 0;
  bool body_ok = true;
};
SelftestStats selftest_stats();

// --- --describe -----------------------------------------------------------------------------------------------------------
int describe(HINSTANCE instance);

// The thread that runs frames (the program's main thread).
DWORD game_thread_id();

}  // namespace fakegame
