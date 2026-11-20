#pragma once
// R1's probe report (O89; plan §2; the owner's offline round R0c): what the probe build of eldennet.dll finds in game,
// in numbers and the leads' labels only, for the reviewing agent to turn into gal/ data. Written through en-gal's
// closed-vocabulary writer (report.hpp), kind "probe", schema 1, and checked by `eldennet-engine lint --report`. Its
// companion, r1-derive.json, is the derivation run over the live image (derive.json's schema 1, the vocabulary
// DataPiler's guard already holds), so this report holds only what the derivation does not:
// - `checks`: what R1 confirmed before its one call into the game, each item `confirmed`, `differs`, `missing` or
//   `not_run`, with the number found and the number expected (R0's or the draft layout's) where there is one: the
//   gate, the FD4 idiom against gal/patterns.toml's (its value the live hits, its expected R0's count), the name
//   layout, the registration and its registers, the group's index against the layout's, the task's shape, and each
//   singleton;
// - `hinstance`: the data slots holding the module's base (global_hinstance's slot);
// - `register`: the call, made only once every check passed and at the title (the first Mark), its registers and group;
// - `slots`: how often the game called each slot of our task's fake vtable (the run slot is the one called every
//   frame), and how much of our object the game's hub wrote;
// - `census`: the vtables the hub's own objects have, and the base classes they share, each with its derived classes'
//   slot counts and run slot (the task's base class);
// - `rising`: the data slots whose instance holds a 32-bit field rising with the clock (the play time's static, which
//   has no RTTI), with how often it rose and stood still;
// - `statics`: each RTTI-classed static's slot, found as the data slot whose instance has the class's vtable;
// - `hub_search` (spike.5): where in the hub's reachable memory our task object's address was found;
// - `callee_code` (spike.6): the registration's callee as its code reads: its calls, its branches before the first
//   call, and what it does with each argument register before it; since spike.7 the arguments in each call (`pass`),
//   each branch's flag test (`guard`), and each callee of it summarised with the arguments followed in (`inner`);
// - `memory_search` (spike.6): every word of the process equal to our task's address, or pointing into it, after the
//   call, ours counted apart; since spike.7 also those equal to our vtable's and its slots' addresses, and the
//   process's committed total;
// - `hub_diff` (spike.7): the hub's reachable memory before the call against the same walk 2 and 20 s after it.
// It is in en-gal's offline part, beside derive.json's report, so that the launcher lints R1's two reports after an R1
// session (--probe) without en-core (ARCH §2.3). The probe build of eldennet.dll fills `Findings` in game
// (win/probe_r1.cpp); probe.hpp's searches, which find what it holds, are en_gal_game_probe's.
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/gal/report.hpp"
#include "en/proto/expected.hpp"

namespace en::gal::probe {

enum class CheckStatus : std::uint8_t { Confirmed, Differs, Missing, NotRun };

struct Check {
  std::string_view item;                   // gate, fd4_idiom, name_layout, register, register_abi, group, task_shape,
                                           // singleton
  std::string label;                       // the leads' label, where the item takes one
  CheckStatus status = CheckStatus::NotRun;
  std::optional<std::uint64_t> value;      // what R1 found
  std::optional<std::uint64_t> expected;   // what R0 or the draft layout says
};

struct Base {
  std::uint32_t type_descriptor = 0;  // RVA
  std::uint32_t found = 0;            // of the census's classes, how many list it among their bases
  std::uint32_t derived = 0;          // classes deriving from it in the image
  std::uint32_t slots_min = 0;
  std::uint32_t slots_max = 0;
  std::optional<std::uint32_t> run_slot;
};

struct Rising {
  std::uint32_t slot = 0;
  std::uint32_t offset = 0;
  std::uint32_t rose = 0;
  std::uint32_t still = 0;
  std::uint32_t other = 0;
};

struct Static {
  std::string label;
  std::uint64_t candidates = 0;
  std::uint64_t checked = 0;
  std::vector<std::uint32_t> found;  // slots (RVAs)
};

// The class objects' name search, as numbers only (derive.hpp's NameSlot): R1 run 1 named 9 of 217 (WHY§206).
struct NameSlot {
  std::uint32_t offset = 0;
  std::uint64_t null_ptr = 0, to_rdata = 0, to_data = 0, to_code = 0, to_other = 0, outside = 0;
  std::uint64_t plain = 0, qualified = 0, other_text = 0, inline_text = 0;
};

struct Names {
  std::uint64_t of = 0;         // class objects searched
  std::uint64_t agree = 0;      // named by the best layout
  std::uint64_t qualified = 0;  // of those, names joined by :: to a namespace
  std::optional<std::uint32_t> offset;  // the best layout's offset
  // Naming by the instances' RTTI (O92): statics tried, null, unread, not a known vtable, classed, leads named.
  std::uint64_t inst_statics = 0, inst_null = 0, inst_unread = 0, inst_unclassed = 0, inst_classed = 0,
                inst_matched = 0;
  std::vector<NameSlot> slots;
};

// R1 run 4 (WHY§209): register_task answered ok and the task never ran. After the call, on T1, the hub's reachable
// memory is searched for our task object's address, read-only and bounded: where it was stored, if anywhere.
struct HubHit {
  std::uint32_t depth = 0;            // pointers followed from the hub's instance
  std::vector<std::uint32_t> path;    // the offset read at each level, the last the word that held our address
  std::uint32_t near_group = 0;       // 32-bit words equal to the group's index within 64 bytes of it
};

struct HubSearch {
  std::uint64_t at_ms = 0;            // after the call
  std::uint64_t nodes = 0;            // blocks read
  std::uint64_t read_bytes = 0;
  std::uint64_t faults = 0;           // copies that faulted
  std::uint64_t refused_reads = 0;    // addresses the plausibility rules refused
  std::uint64_t found = 0;            // words equal to our object's address
  std::uint64_t inside = 0;           // words pointing inside our object, past its start
  std::vector<HubHit> hits;      // the first eight
};

// R1 run 5 (WHY§227): register_task answered ok, the task never ran, and the hub's memory held nowhere our address. So
// spike.6 summarises the callee from the live image (probe.hpp's summarise_callee: the code the call runs, decoded from
// its RVA within its function or 4 KiB, no call followed) and searches the whole process for our address after the call
// (probe.hpp's search_memory). Offsets are from the callee's entry; registers by number, 0 rax to 15 r15.
// R1 run 6 (WHY§235): the callee is a wrapper of one branch and two calls. Spike.7 maps the arguments into each call
// (`pass`), reads the flag test before each branch (`guards`), and summarises each callee of the wrapper in turn with
// the wrapper's arguments followed into it (InnerCallee): an inner summary's `args` are the wrapper's.
enum class CallForm : std::uint8_t { Direct, Register, Memory, Rip, Tail };  // Tail: a direct jump out of the function
// A register holding one of the summarised function's arguments at a call (or a tail jump).
struct PassedArg {
  std::uint8_t reg = 0;     // 1 rcx, 2 rdx, 8 r8, 9 r9: the register at the call
  std::uint8_t from = 0;    // the argument it holds, by the same numbers
  bool first_word = false;  // it holds the first word of that argument's object (its vtable), not the argument
};
struct CalleeCall {
  std::uint32_t offset = 0;
  CallForm form = CallForm::Direct;
  std::optional<std::uint32_t> target;    // a direct call's or tail jump's RVA, or the slot a RIP-relative one reads
  std::optional<std::uint8_t> base;       // a register call's register, or a memory call's base
  std::optional<std::uint8_t> index;      // a memory call's index register
  std::int64_t disp = 0;                  // a memory call's displacement
  std::optional<std::uint8_t> vtable_of;  // the argument whose object's first word the base holds (a virtual call on it)
  bool reached = false;                   // the flow from the entry reaches it, so `pass` is known
  std::vector<PassedArg> pass;            // rcx, rdx, r8 and r9 at the call, those that hold an argument
};
struct CalleeBranch {
  std::uint32_t offset = 0;
  std::int64_t to = 0;  // the target, from the entry
};
// An operand of the instruction that sets a branch's flags.
enum class OperandForm : std::uint8_t { Register, Memory, Rip, Immediate };
struct Operand {
  OperandForm form = OperandForm::Register;
  std::optional<std::uint8_t> reg;       // a register operand
  std::optional<std::uint8_t> from;      // the argument that register holds
  bool first_word = false;               // it holds the first word of that argument's object
  std::optional<std::uint8_t> base;      // a memory operand's base register
  std::optional<std::uint8_t> base_arg;  // the argument the base holds
  std::optional<std::uint8_t> index;
  std::int64_t disp = 0;
  std::optional<std::uint32_t> target;   // a RIP-relative operand's slot (RVA)
  std::optional<std::uint64_t> imm;      // an immediate, as the operand's width reads it, kept from 0 to 65,536
};
enum class GuardOp : std::uint8_t { Compare, Test, Other, None };
// A conditional branch before the first call, with the instruction that set its flags (the nearest before it, past
// moves and loads, which set none).
struct CalleeGuard {
  std::uint32_t offset = 0;              // the branch
  std::int64_t to = 0;                   // its target, from the entry
  std::uint8_t cond = 16;                // a Jcc's condition (0 o to 15 g); 16 another branch (loop, jrcxz, xbegin)
  GuardOp op = GuardOp::None;            // None: no instruction before it in the decoded code
  std::optional<std::uint32_t> op_at;
  std::uint8_t width = 0;                // the operands' size in bytes
  std::vector<Operand> operands;         // a compare's or test's, the first as the instruction names it
};
// A store of an argument (or of a register holding it) to memory, before the first call.
struct ArgStore {
  std::uint32_t offset = 0;
  std::uint8_t via = 0;                  // the register stored
  std::optional<std::uint8_t> base;      // the address's base register (none: RIP-relative or absolute)
  std::optional<std::uint8_t> base_arg;  // the argument the base register holds, if it holds one
  std::optional<std::uint8_t> index;
  std::int64_t disp = 0;                 // the displacement; RIP-relative: the slot's RVA
  bool rip = false;
  std::uint8_t width = 0;                // bytes stored
  bool first_word = false;               // the register held the first word of the argument's object (its vtable)
};
// A compare of an argument with an immediate, before the first call: the value kept only if 0 to 65,536.
struct ArgCompare {
  std::uint32_t offset = 0;
  std::uint8_t via = 0;
  std::optional<std::uint64_t> imm;
};
struct ArgUse {
  std::uint8_t reg = 0;                // 1 rcx, 2 rdx, 8 r8, 9 r9 (and their 32-bit forms)
  std::vector<ArgStore> stores;        // the first 8
  std::vector<std::uint8_t> saved;     // the callee-saved registers it was moved to
  std::vector<ArgCompare> compares;    // with an immediate, the first 8
  std::uint32_t compared_other = 0;    // compares with memory, a register, or an immediate past the bound
  std::uint32_t tested = 0;            // TEST with itself or an immediate (a null or flag check)
};
struct CalleeCode {
  bool read = false;                    // the image gave the bytes
  std::uint32_t extent = 0;             // the function's length in the exception directory, or 0
  std::uint32_t bytes = 0;              // decoded
  std::uint32_t instructions = 0;
  bool cut = false;                     // the 4 KiB bound ended it before the function's end
  bool undecodable = false;             // decoding stopped at bytes that do not decode
  std::optional<std::uint32_t> first_call;
  std::optional<std::uint32_t> first_ret;
  bool returns_early = false;           // a path from the entry reaches a return through no call
  std::optional<std::uint32_t> modelled_to;  // the first instruction before the first call whose effect on the
                                             // registers the summary does not model: tracking stops there
  std::uint32_t call_count = 0;
  std::vector<CalleeCall> calls;        // the first 32
  std::uint32_t branch_count = 0;       // conditional branches before the first call
  std::vector<CalleeBranch> branches;   // the first 32
  std::vector<CalleeGuard> guards;      // the first 8 of those branches, each with its flags' instruction
  std::vector<ArgUse> args;             // rcx, rdx, r8, r9 (of the wrapper, for an inner summary)
};
// A callee of the summarised function (a direct call's target, or a tail jump's), summarised in turn with the
// arguments its call site passes it: its `args` are the wrapper's (spike.7, WHY§235).
struct InnerCallee {
  std::uint32_t rva = 0;
  std::uint32_t site = 0;  // the call's (or jump's) offset in the wrapper
  bool tail = false;
  CalleeCode code;
};

enum class RegionKind : std::uint8_t { Private, Mapped, Image, Other };
enum class ModuleKind : std::uint8_t { None, Ours, Game, Other };
enum class SectionKind : std::uint8_t { None, Header, Code, Rdata, Data, Other };
// What a word the search counts points at (spike.7): our object (or into it), our fake vtable, or one of its slots.
enum class PointsTo : std::uint8_t { Object, Vtable, Slot };
struct MemoryHit {
  bool own = false;                     // in our module, our heap, our task's own block or our vtable's
  std::uint64_t offset = 0;             // 0: the word is our object's address; else how far into it the word points
  RegionKind region = RegionKind::Other;
  std::uint32_t protect = 0;            // the region's protection, as VirtualQuery gives it
  ModuleKind module = ModuleKind::None; // an image's
  SectionKind section = SectionKind::None;
  std::optional<std::uint32_t> rva;     // in our module or the game's
  std::uint32_t near_group = 0;         // 32-bit words equal to the group's index within 64 bytes of it
  PointsTo points_to = PointsTo::Object;
  std::optional<std::uint32_t> slot;    // a slot's index in our vtable
};
enum class SearchStop : std::uint8_t { Done, Bytes, Time };
struct MemorySearch {
  std::uint64_t at_ms = 0;        // after the call
  std::uint64_t ms = 0;           // how long it took
  std::uint64_t regions = 0;      // committed regions, past a cap too
  std::uint64_t committed_bytes = 0;  // the process's committed total (spike.7), past a cap too
  std::uint64_t read_regions = 0;
  std::uint64_t read_bytes = 0;
  std::uint64_t unread_bytes = 0; // committed but not read: no access, guard, uncached or write-combined, past user space
  std::uint64_t stack_bytes = 0;  // in a known thread stack, or kept out (the search's own buffer)
  std::uint64_t faults = 0;       // pages whose copy faulted
  SearchStop stopped = SearchStop::Done;
  bool own_known = false;         // our heap's regions were known, so its copies count as ours
  std::uint64_t found = 0;        // words equal to our object's address, outside ours
  std::uint64_t inside = 0;       // words pointing inside it, past its start, outside ours
  std::uint64_t own_found = 0;
  std::uint64_t own_inside = 0;
  // Spike.7 (WHY§235): words equal to our fake vtable's address, and to one of its slots' (thunks in our module).
  std::uint64_t vtable_found = 0;
  std::uint64_t own_vtable_found = 0;
  std::uint64_t slot_found = 0;
  std::uint64_t own_slot_found = 0;
  std::uint32_t slot_distinct = 0;      // how many different slots the game's words name
  std::uint32_t own_slot_distinct = 0;
  // The game's first, then ours; each side's words equal to our object's address (or inside it) first, then our
  // vtable's, then a slot's: at most 8 of each.
  std::vector<MemoryHit> hits;
};

// Spike.7 (WHY§235): the hub's reachable memory just before the call, kept, and compared with the same walk 2 and 20 s
// after it, block by block by path (probe.hpp's walk_hub and diff_hub). Numbers and kinds only.
enum class ValueKind : std::uint8_t { Zero, Small, Group, Task, Vtable, Ours, Game, Heap, Other };
struct WordValue {
  ValueKind kind = ValueKind::Other;
  std::optional<std::uint64_t> value;   // Small, Group: the value, 1 to 65,536
  std::optional<std::uint64_t> offset;  // Task, Vtable: how far into ours it points, when not at the start
  SectionKind section = SectionKind::None;  // Game: the section's kind, by its characteristics
  std::optional<std::uint32_t> rva;     // Game, Ours
  std::optional<bool> fresh;            // a new pointer into memory: the walk before the call reached no block there
  bool group_half = false;              // a 32-bit half of the word equals the group's index
};
struct DiffWord {
  std::uint32_t depth = 0;              // pointers followed from the hub's instance
  std::vector<std::uint32_t> path;      // the offset read at each level, the last the word's own
  WordValue before;
  WordValue after;
  std::optional<bool> steady;           // the later diff: the word held the same new value at the earlier one
};
struct HubDiff {
  std::uint64_t at_ms = 0;
  std::uint64_t before_blocks = 0;      // read before the call
  std::uint64_t after_blocks = 0;       // read now
  std::uint64_t compared = 0;           // blocks found again, by path (or at the same address by another path)
  std::uint64_t moved = 0;              // paths that now lead to another address
  std::uint64_t gone = 0;               // blocks read before whose address the walk no longer reaches
  std::uint64_t new_blocks = 0;         // reached only after the call
  std::uint64_t new_bytes = 0;
  std::uint64_t changed = 0;            // 8-byte words that differ
  std::optional<std::uint64_t> steady_words;  // the later diff: changed words holding the earlier diff's new value
  bool capped = false;                  // a walk met its cap of blocks or bytes
  std::uint64_t kept_bytes = 0;         // the bytes kept from before the call
  std::vector<DiffWord> words;          // the first 32 (in the later diff, the steady ones first)
};

struct Findings {
  std::string_view stage = "start";  // start, derived, waiting, registered, running, refused, failed
  std::uint64_t elapsed_ms = 0;      // since T1's start
  bool gate_passed = false;
  std::uint32_t derive_passes = 0;
  std::uint64_t derive_ms = 0;
  bool derive_written = false;
  std::vector<Check> checks;
  std::vector<std::uint32_t> hinstance;
  std::uint64_t hinstance_count = 0;
  // The call.
  std::string_view register_status = "not_called";  // not_called, waiting_for_mark, refused, ok, faulted
  std::uint32_t callee = 0;                          // RVA
  std::array<std::uint8_t, 3> registers{};           // task, instance, group: 1 rcx, 2 rdx, 8 r8, 9 r9
  std::uint32_t group_index = 0;
  std::uint32_t object_size = 0;
  std::uint32_t vtable_slots = 0;
  // On the game thread.
  std::uint64_t first_tick_ms = 0;  // from the call to the first tick
  std::uint64_t invocations = 0;
  std::vector<std::pair<std::uint32_t, std::uint64_t>> slot_calls;  // slot, calls (those called)
  std::uint32_t object_written = 0;   // one past the last byte of our object the hub wrote
  std::uint32_t object_nonzero = 0;   // bytes it holds that are not zero
  bool census_done = false;
  std::uint64_t census_copies = 0;
  std::uint64_t census_faults = 0;
  std::uint64_t census_objects = 0;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> census_vtables;  // RVA, objects
  std::vector<Base> bases;
  std::uint64_t rising_candidates = 0;
  std::uint32_t rising_passes = 0;
  std::vector<Rising> rising;
  std::vector<Static> statics;
  std::uint64_t copy_faults = 0;
  std::uint64_t stack_refused = 0;
  std::uint64_t allocations = 0;  // on the game thread after its first tick (R6)
  Names names;
  std::vector<HubSearch> hub_search;
  std::optional<CalleeCode> callee_code;  // written as `callee_code`: `register.callee` already holds its RVA
  std::vector<InnerCallee> callee_inner;  // written as `callee_code.inner`
  std::vector<MemorySearch> memory_search;
  std::vector<HubDiff> hub_diff;
};

[[nodiscard]] const report::Vocabulary& vocabulary() noexcept;
[[nodiscard]] proto::Expected<std::string, report::Problem> write_report(const Findings& f);

}  // namespace en::gal::probe
