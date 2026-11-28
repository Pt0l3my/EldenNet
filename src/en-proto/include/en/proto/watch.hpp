#pragma once
// The watch block: the one page of shared memory between EldenNet in the game and the launcher outside it, the crash
// watcher (O71) and its development console with the Mark (O90). Plain data, laid out the same by every compiler (the
// launcher builds with MinGW-w64 and clang-cl, the DLL with clang-cl), with fixed offsets checked below.
//
// - Its creator is whoever comes first: the launcher, before it injects, or EldenNet, started by another loader, which
//   then starts the watcher (`eldennet-launcher --watch <pid>`). Both name it by the game's process id
//   (`Local\EldenNet.Watch.<pid>`); the creator constructs it and writes the header, the magic last.
// - EldenNet writes its status (the dev console shows it; a spike's run reads its outcome there) and, once, from its
//   crash filter, the crash record: allocation-free, then it signals `Local\EldenNet.Crash.<pid>` and waits at most
//   2 s for the watcher's `Local\EldenNet.CrashAck.<pid>`, while the watcher lists the game's modules.
// - The launcher writes its requests (the spike under way; a crash on request in a probe build, for S0-D; the
//   read-only round in a probe build, which EldenNet answers in the same field) and the
//   Marks: each press of the hotkey, stamped in mono milliseconds (QPC, the recording's clock), which EldenNet's
//   recorder writes into the recording. Once EldenNet's own Mark is bound, a GraceUI hotkey on the same key (part 12),
//   the launcher hands the ring over (`Marks::source`): one writer at a time, EldenNet's after the hand-over.
// Numbers only, and one file name with no folder: nothing personal, no path, no text from the game.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace en::proto::watch {

inline constexpr std::uint32_t kMagic = 0x42574E45u;  // "ENWB"
inline constexpr std::uint32_t kVersion = 3;  // 2: probe_plan appended; 3: probe_gathered (both 2026-11-24)
// The read round's plans (Block::probe_plan): which tasks the launcher's panel lists, and so what the tester's marks
// mean to the round. 0: the rounds up to the seventeenth (two characters; a stand, a walk, a hit, a heal). 1: the
// character survey (one character; a stand, a look around, a kill). 2: the sheet (one character; a stand, a ride
// or a run, the tester's own death). 3: the stay (one character; a stand, the tester's own death, then some minutes
// in the world as the tester likes). 4: the tour (one character; the game's menu open and closed, sprints and a
// spell, mounted and on foot, a travel to another grace and back, a lock on an enemy, the tester's own death, two
// minutes in the world). The DLL takes a number it does not know for a refusal of the round, never for plan 0.
inline constexpr std::uint32_t kPlanRounds = 0;
inline constexpr std::uint32_t kPlanSurvey = 1;
inline constexpr std::uint32_t kPlanSheet = 2;
inline constexpr std::uint32_t kPlanStay = 3;
inline constexpr std::uint32_t kPlanTour = 4;
inline constexpr std::uint32_t kPlanCount = 5;
// What the round says of a task that ended with a mark (Block::probe_gathered, the owner's wish of 2026-11-24: "a
// per task feedback that what was needed did gather"): two bits a mark, the first mark's the lowest.
inline constexpr std::uint32_t kGatherUntold = 0;  // the round has not said (or cannot tell)
inline constexpr std::uint32_t kGathered = 1;      // what the task was for was read
inline constexpr std::uint32_t kNotGathered = 2;   // it was not
inline constexpr std::uint32_t kGatherMarks = 15;  // marks the word has room for
// The word's highest bit: the round could not write its report at its last try (too large, or the file refused).
// Whatever the tasks' verdicts say, the files then do not hold the run (the twentieth round's two runs: the panel
// said "usable" while the report stood still).
inline constexpr std::uint32_t kGatherReportFailed = 1u << 31;
// The verdict for the mark `n` (1 the first) in the word; kGatherUntold past the word's room.
[[nodiscard]] constexpr std::uint32_t gathered_of(std::uint32_t word, std::uint32_t n) noexcept {
  return n == 0 || n > kGatherMarks ? kGatherUntold : (word >> ((n - 1) * 2)) & 3u;
}
[[nodiscard]] constexpr std::uint32_t with_gathered(std::uint32_t word, std::uint32_t n, std::uint32_t verdict) noexcept {
  if (n == 0 || n > kGatherMarks) return word;
  const std::uint32_t shift = (n - 1) * 2;
  return (word & ~(3u << shift)) | ((verdict & 3u) << shift);
}
inline constexpr std::size_t kBlockSize = 4096;      // one page: what the creator maps
inline constexpr std::size_t kModuleNameBytes = 64;  // a module's file name, ASCII, NUL-padded
inline constexpr std::size_t kMarkRing = 32;         // the newest marks the block keeps
inline constexpr std::uint32_t kCrashWaitMs = 2000;  // the crash filter waits at most this long for the watcher

// The objects' names, each followed by the game's process id in decimal.
inline constexpr std::wstring_view kBlockPrefix = L"Local\\EldenNet.Watch.";
inline constexpr std::wstring_view kCrashEventPrefix = L"Local\\EldenNet.Crash.";
inline constexpr std::wstring_view kAckEventPrefix = L"Local\\EldenNet.CrashAck.";

// T1's states (ARCH §3.2), as EldenNet reports them; None until it has attached.
enum class InitState : std::uint32_t {
  None,
  Pin,
  Gate,
  Safety,
  Settings,
  WaitWindow,
  Connect,
  ReadyScan,
  WaitEngine,
  Discover,
  Register,   // the game's task: the call that registers it (not a v1 start, O101; R1, retired 2026-11-22, O106)
  FirstTick,  // the game's task: its first call from the game (as Register: nothing enters it now)
  Ready,   // DataReady sent: the reading runs and phase A passed
  Inert,   // the first reading's checks failed, or none came in 30 s
  Banner,  // stopped at a banner (the status's `banner` says which)
  Exit,    // a second copy, or Off
  // Appended (the numbers above never change): a start that reads on a thread of EldenNet's own (O101).
  Reader,        // the reader thread is being started; nothing of the game is called
  FirstReading,  // waiting for its first reading
};
inline constexpr std::uint32_t kInitStateCount = 18;
[[nodiscard]] const char* to_string(InitState s) noexcept;  // "none", "pin", ..., "ready", "inert", "banner", "exit"
// Where T1 stops: a spike's run ends there.
[[nodiscard]] constexpr bool terminal(InitState s) noexcept {
  return s == InitState::Ready || s == InitState::Inert || s == InitState::Banner || s == InitState::Exit;
}

// The spikes (DESIGN §16.1) the launcher runs, told to EldenNet so it can do its part: S0-A to S0-D, then S0-E's
// present-hook half and S4 (input), whose measurements are GraceUI's observer's (roadmap S10.1.g, S11.1.a). Append
// only: the words are numbers in the block.
enum class Spike : std::uint32_t { None, S0a, S0b, S0c, S0d, S0dCrash, S0e, S0eAlone, S4 };
inline constexpr std::uint32_t kSpikeCount = 9;
// "none", "s0a", "s0b", "s0c", "s0d", "s0d-crash", "s0e", "s0e_alone", "s4"
[[nodiscard]] const char* to_string(Spike s) noexcept;
// S0-E and S4: GraceUI's observer measures, the launcher gives the run a folder of its own for the observer's report
// (ELDENNET_SPIKE_DIR), and EldenNet makes no call into the game (it stops at Register). With s0e_alone the launcher
// loads no eldennet.dll at all, only the --also DLLs.
[[nodiscard]] constexpr bool observer_run(Spike s) noexcept {
  return s == Spike::S0e || s == Spike::S0eAlone || s == Spike::S4;
}

// The launcher's --read (Lodestar's read-only round, step L5): `Read` is asked by the launcher before it loads
// anything, and answered by EldenNet at Settings. Only a probe build of eldennet.dll accepts it, and it calls nothing
// of the game at all; a release build, or EldenNet turned off in settings.toml, refuses it.
// Append only: the numbers never change, and a retired word keeps its number. Nothing writes or accepts a retired one.
enum class ProbeRequest : std::uint32_t {
  None,
  RetiredR1,  // retired with R1, 2026-11-22 (O106)
  Accepted,
  Refused,
  RetiredAbsent,  // retired with R1, 2026-11-22 (O106)
  Read,
};
// "none", "retired", "accepted", "refused", "retired", "read"
[[nodiscard]] const char* to_string(ProbeRequest p) noexcept;

enum class CrashState : std::uint32_t { None, Writing, Written, Taken };

// Who stamps the Marks (O90; part 12), one writer at a time: the launcher, with its Pause registered as a system hotkey
// (`--console`), until EldenNet's own Mark, a GraceUI hotkey on the same key, is bound; EldenNet then asks, the launcher
// lets its key go and hands the ring over, and from then on EldenNet alone adds Marks. With nobody holding the key
// (no console), EldenNet takes the ring at once.
enum class MarkSource : std::uint32_t {
  None,      // nobody stamps Marks yet
  Launcher,  // the launcher's Pause, a system hotkey (RegisterHotKey), stamps them
  Asked,     // EldenNet's GraceUI Mark is bound and asks for the ring: the launcher is to let its key go
  EldenNet,  // the launcher let its key go (or never held it): EldenNet's GraceUI Mark stamps them
};
inline constexpr std::uint32_t kMarkSourceCount = 4;
[[nodiscard]] const char* to_string(MarkSource s) noexcept;  // "none", "launcher", "asked", "eldennet"

// The thread that crashed, as EldenNet knows its threads. Game is the game's own thread as far as EldenNet knows it:
// the thread of the game's window. Reader (appended) is EldenNet's own reading thread (O101).
enum class ThreadKind : std::uint32_t { Unknown, Game, Init, Other, Reader };
[[nodiscard]] const char* to_string(ThreadKind t) noexcept;  // "unknown", "game", "init", "other", "reader"

// Status flags (Status::flags).
inline constexpr std::uint32_t kFlagGateOpen = 1u << 0;
inline constexpr std::uint32_t kFlagDormant = 1u << 1;
inline constexpr std::uint32_t kFlagBadge = 1u << 2;
inline constexpr std::uint32_t kFlagCore = 1u << 3;  // phase, level and the gate's flags are the core's

struct Header {
  std::atomic<std::uint32_t> magic;  // kMagic, written last by the creator
  std::uint32_t version;
  std::uint32_t size;
  std::uint32_t game_pid;
};

// The launcher's requests (development).
struct Control {
  std::atomic<std::uint32_t> spike;          // Spike
  std::atomic<std::uint32_t> crash_request;  // 1: crash now (s0d-crash; honoured by probe builds only)
  std::atomic<std::uint32_t> watcher_pid;    // the watcher's process id while it watches, else 0
  std::atomic<std::uint32_t> probe;          // ProbeRequest: the launcher writes Read, EldenNet its answer
};

// EldenNet's status, field by field (a reader may see one update half done; each field is whole).
struct Status {
  std::atomic<std::uint32_t> init;              // InitState
  std::atomic<std::uint32_t> phase;             // en-core's Phase + 1; 0 before the core
  std::atomic<std::uint32_t> level;             // en-core's Level + 1; 0 before the core
  std::atomic<std::uint32_t> banner;            // en-core's BannerReason + 1; 0 none
  std::atomic<std::uint32_t> flags;             // kFlag*
  std::atomic<std::uint32_t> items_resolved;    // discovery's items resolved
  std::atomic<std::uint32_t> items_total;       // and asked for
  std::atomic<std::uint32_t> discovery_passes;  // 1 to 4 (the retries at +5, +15, +30 s)
  std::atomic<std::uint32_t> ready_ms;          // from T1's start to its terminal state
  std::atomic<std::uint32_t> task_us_p99;       // C13
  std::atomic<std::uint64_t> snapshots;         // built by the task
  std::atomic<std::uint64_t> faults;            // guarded reads that faulted, all groups
  std::atomic<std::uint64_t> tripped;           // the groups tripped, bit per proto::Group
  std::atomic<std::uint64_t> updated_ms;        // mono ms of the last update
};

// Written once, by EldenNet's crash filter, with no allocation; published by `state` = Written.
struct Crash {
  std::atomic<std::uint32_t> state;           // CrashState
  std::uint32_t code;                         // the exception's code
  std::uint32_t thread;                       // ThreadKind
  std::uint32_t chained;                      // 1: a filter installed before EldenNet's was called after it
  std::uint64_t offset;                       // the faulting address's offset in its module; 0 outside every module
  std::uint64_t at_ms;                        // mono ms
  std::array<char, kModuleNameBytes> module;  // that module's file name, no folder: ASCII, '?' for anything else
};

// The Marks (O90): mark n (from 1) is at mono_ms[(n - 1) % kMarkRing]; `count` is written after it, by the one writer
// `source` names (MarkSource).
struct Marks {
  std::atomic<std::uint32_t> count;
  std::atomic<std::uint32_t> source;  // MarkSource
  std::array<std::atomic<std::uint64_t>, kMarkRing> mono_ms;
};

struct Block {
  Header header;
  Control control;
  Status status;
  Crash crash;
  Marks marks;
  // Appended (version 2): the read round's plan (kPlan*), written by the launcher with its --read request before the
  // game starts, read once by the round.
  std::atomic<std::uint32_t> probe_plan;
  // Appended (version 3): what the round gathered, task by task (kGather*, two bits a mark). Written by the DLL as
  // the round judges each mark's task, read by the launcher's panel. Nothing else crosses here.
  std::atomic<std::uint32_t> probe_gathered;
};

static_assert(std::atomic<std::uint32_t>::is_always_lock_free && std::atomic<std::uint64_t>::is_always_lock_free,
              "the block's atomics must be lock-free: they live in memory two processes share");
static_assert(sizeof(std::atomic<std::uint64_t>) == 8 && alignof(std::atomic<std::uint64_t>) == 8);
static_assert(std::is_standard_layout_v<Block>);
static_assert(offsetof(Block, control) == 16);
static_assert(offsetof(Block, status) == 32);
static_assert(offsetof(Block, crash) == 104);
static_assert(offsetof(Block, marks) == 200);
static_assert(offsetof(Block, probe_plan) == 464);
static_assert(offsetof(Block, probe_gathered) == 468);
static_assert(sizeof(Block) == 472);
static_assert(sizeof(Block) <= kBlockSize);

// The creator's part, on memory it has just mapped (zero-filled): every field zero, then the header, magic last.
void initialise(Block& b, std::uint32_t game_pid) noexcept;
// The magic, version and size agree: what an opener checks before it reads or writes anything else.
[[nodiscard]] bool valid(const Block& b) noexcept;

// A module's file name (no folder) into the record's fixed field: ASCII letters, digits and `._-+ ` kept, anything
// else '?', cut at kModuleNameBytes - 1. Allocation-free, for a crash filter.
void set_module_name(Crash& c, const wchar_t* name, std::size_t length) noexcept;
[[nodiscard]] std::string_view module_name(const Crash& c) noexcept;

// Mono milliseconds from a performance counter and its frequency, without overflow: the snapshots' clock.
[[nodiscard]] constexpr std::uint64_t mono_ms(std::uint64_t counter, std::uint64_t frequency) noexcept {
  return frequency == 0 ? 0 : (counter / frequency) * 1000 + (counter % frequency) * 1000 / frequency;
}

// Adds a Mark; answers its number (from 1). Only the writer `source` names calls it.
std::uint32_t add_mark(Marks& m, std::uint64_t at_ms) noexcept;

// The hand-over (MarkSource). The launcher, once its hotkey is registered: `launcher_holds`. EldenNet, once its own
// Mark is bound: `eldennet_asks`, which answers true when the ring is EldenNet's at once (nobody held the key), and
// false when the launcher must first let its key go (Asked) or when there is nothing to ask (already Asked or
// EldenNet's). The launcher, seeing Asked: unregister its key, take the presses already queued, then `launcher_hands_over`.
[[nodiscard]] MarkSource mark_source(const Marks& m) noexcept;
void launcher_holds(Marks& m) noexcept;
[[nodiscard]] bool eldennet_asks(Marks& m) noexcept;
void launcher_hands_over(Marks& m) noexcept;

}  // namespace en::proto::watch
