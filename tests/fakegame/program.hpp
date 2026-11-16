#pragma once
// The fake game's own parts, shared between its files: the options, the status lines, the task hub, the world, the
// faults on request, the self-test and --describe (README.md).
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
  std::wstring status_file;
  std::uint32_t exit_code = 0;
  bool describe = false;
  bool verbose = false;
};

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
void hub_register_builtins(FG::TaskUnit* const tasks[4], const std::uint32_t groups[4]);
void hub_run_frame(std::uint64_t frame, float dt);

struct HubStats {
  std::uint64_t builtin_runs = 0;
  std::uint32_t foreign_tasks = 0;
  std::uint64_t foreign_runs = 0;
  std::uint64_t foreign_us_max = 0;
  std::uint32_t rejected = 0;
  std::uint32_t ignored = 0;
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
