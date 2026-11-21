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
// - `hub_diff` (spike.7): the hub's reachable memory before the call against the same walk 2 and 20 s after it;
// - since spike.8 (WHY§240): the call's return value (`register.returned`), each summarised function's instructions
//   as forms (`forms`: classes and operand kinds, no bytes), jumps through memory or a register as tail call sites,
//   where a tested register's value came from (`source`), the table the tail function reads, read before and after
//   the call (`table`), and a control diff of the hub over the same interval before the call (`hub_diff_control`);
// - since spike.9 (WHY§248): the group's own function (the tail function's direct jump) and its callees summarised with
//   the group's entry and our task followed in, each load from our task or the entry with what its value is used for
//   (`task_reads`, `entry_reads`) and each store of our task or of a value read from it (`task_stores`); the walk and
//   its diffs from the singleton and the group's entry too (`root`); words holding our object's or vtable's address
//   under a high-bit tag (`task_tagged`, `tagged_found`); and the RTTI class of the singleton and of the entry against
//   the leads' names (`singleton_class`, `entry_class`);
// - since spike.10 (WHY§289): the second singleton's method (G's, reached through a stub) and its callees, with G's
//   loads (`g_reads`) and the uses of the entry's word (`word_uses`); G and the entry's word read before and after the
//   call (`method`); G's walk and its diffs (`root` g_singleton); the array behind CSTask's +0x3B0 and +0x3D0 read
//   again 50 and 250 ms and 2 s after the call (`array_views`); what the game's own call sites pass as the task
//   (`game_sites`); and the registration's sibling summarised the same way (`sibling`).
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
enum class RegionKind : std::uint8_t { Private, Mapped, Image, Other };
enum class ModuleKind : std::uint8_t { None, Ours, Game, Other };
enum class SectionKind : std::uint8_t { None, Header, Code, Rdata, Data, Other };

// R1 run 7 (WHY§240): the wrapper tail-jumps to a function that bounds the group, tests a register loaded from none of
// its arguments, and leaves through a jump spike.7 did not count. Spike.8 models a jump through memory or a register as
// a tail call (TailMemory, TailRegister), and follows what a register holds past the arguments: a value loaded from a
// RIP-relative slot or from [base + index*scale + disp] (Source), at most 3 levels.
enum class CallForm : std::uint8_t { Direct, Register, Memory, Rip, Tail, TailMemory, TailRegister };
// An instruction's class, as R1's forms name it (spike.8): no bytes and no opcode beyond it.
enum class InsnClass : std::uint8_t {
  Mov, MovExt, Lea, Cmp, Test, Jcc, Jmp, Call, Ret, Push, Pop, Xor, Add, Sub, And, Or, Shift, Imul, Nop, Int3, Other
};
// What a register holds, as the report describes it (spike.8): an argument, the first word of its object, the value a
// function was entered with in one of rcx to r9 that holds no argument (an inner summary's), or a value loaded from
// memory, with what the load's base and index held, to at most 3 loads deep. Since spike.9 also a followed value (our
// task, a value read from it or from the group's entry) changed by an instruction: added to, masked, tagged, shifted.
struct Source {
  enum class Kind : std::uint8_t { None, Argument, FirstWord, Entry, Rip, Memory, Modified };
  Kind kind = Kind::None;
  std::uint8_t from = 0;        // Argument, FirstWord: the argument (1, 2, 8, 9); Entry: the register
  std::uint32_t at = 0;         // Rip, Memory: the load's offset (of the loads that made it, the first in address order);
                                // Modified: the changing instruction's
  std::uint32_t target = 0;     // Rip: the slot's RVA
  std::optional<std::uint8_t> base;   // Memory: the registers
  std::optional<std::uint8_t> index;
  std::uint8_t scale = 1;
  std::int64_t disp = 0;
  std::uint8_t width = 8;       // Rip, Memory: bytes loaded; Modified: the operation's size
  std::vector<Source> base_src;   // Memory: what the base held (at most one)
  std::vector<Source> index_src;  // and the index
  // Modified (spike.9): the instruction's class (add, or, xor, and, sub, shift, lea; a bit set, cleared or flipped is
  // other's, by the bit's number), and what it changed the value by: an immediate (kept from 0 to 65,536, else
  // `large`), a register (`by`), or memory (neither).
  InsnClass op = InsnClass::Other;
  std::optional<std::uint64_t> imm{};
  bool large = false;
  std::optional<std::uint8_t> by{};
  std::vector<Source> origin{};   // the value it changed (one)
};
// Whether two descriptions name the same value (the load's offset aside).
[[nodiscard]] bool same_source(const Source& a, const Source& b) noexcept;
// A register at a call holding a known value that is no argument (spike.8); spike.9: where it holds the group's entry
// (an inner summary's `entry_src`); spike.10: where it holds the second singleton G, or the entry's word.
struct PassedSource {
  std::uint8_t reg = 0;
  Source source;
  bool holds_entry = false;
  bool holds_g = false;
  bool holds_word = false;
};
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
  // Spike.8: a tail jump through a RIP-relative slot (`target` its RVA); what a memory or register call's base holds
  // when it is no argument; the object whose first word it holds when that object's value is known (a virtual call on
  // a value loaded from memory); rcx, rdx, r8 and r9 holding known values that are no argument.
  bool rip = false;
  std::optional<Source> base_src;
  std::optional<Source> vtable_src;
  std::vector<PassedSource> pass_src;
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
  // Spike.8, in the forms: a register's or memory operand's width in bytes (0 where the form gives none), whether a
  // byte register is ah, ch, dh or bh (`reg` then names rax to rbx), an index's scale, a RIP-relative slot's section
  // (by its characteristics, filled from the image), and an immediate past 65,536 (`large`, its value not kept).
  std::uint8_t width = 0;
  bool high = false;
  std::uint8_t scale = 0;
  SectionKind section = SectionKind::None;
  bool large = false;
  // A guard's register operand holding no argument: where its value came from; a guard's memory operand whose base holds
  // no argument: what the base holds (spike.8).
  std::optional<Source> source;
  std::optional<Source> base_src;
};
struct Form {
  std::uint32_t offset = 0;
  std::uint8_t length = 0;
  InsnClass cls = InsnClass::Other;
  std::uint8_t cond = 16;          // a Jcc's condition, as CalleeGuard's
  std::optional<std::int64_t> to;  // a relative branch's, jump's or call's target, from the entry
  bool unreached = false;          // no known path from the entry reaches it (padding after a return or a jump)
  std::vector<Operand> operands;   // as the instruction names them (Intel's order), for the classes that have them
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
// Spike.9 (WHY§248): a load from our task (the wrapper's task argument, or a value loaded from it) or from the group's
// entry, anywhere in the function, and what the value it loads is then used for, along the flow.
// Spike.10: Index, a value used as a memory operand's index.
enum class UseKind : std::uint8_t { Compared, ComparedOther, Tested, Stored, Passed, Called, Base, Changed, Other, Index };
struct FieldUse {
  UseKind kind = UseKind::Other;
  std::uint32_t offset = 0;             // the instruction that uses it
  std::optional<std::uint64_t> imm;     // Compared: the immediate, kept from 0 to 65,536
  bool large = false;                   // Compared: an immediate past it
  std::optional<std::uint8_t> reg;      // Passed: the register at the call (or tail jump)
  std::optional<std::uint32_t> target;  // Passed, Called: a direct call's or tail jump's RVA
  std::optional<std::uint8_t> base;     // Stored: the address's base register
  std::optional<Source> base_src;       // Stored: what that base holds
  std::optional<std::uint8_t> index;
  std::int64_t disp = 0;                // Stored: the displacement (RIP-relative: the slot's RVA)
  bool rip = false;
  std::uint8_t width = 0;               // Stored: bytes stored
  std::optional<InsnClass> op;          // Changed: the instruction's class
};
struct FieldRead {
  std::uint32_t offset = 0;             // the reading instruction
  std::int64_t disp = 0;                // the field's offset from the base
  std::uint8_t width = 0;               // bytes read
  std::uint8_t base = 0;                // the address's base register
  Source base_src;                      // what the base holds: the task, the entry, or a value read from one
  std::optional<std::uint8_t> index;
  std::uint8_t scale = 1;
  bool direct = false;                  // compared, tested or otherwise used in place: no register loaded
  std::uint32_t use_count = 0;
  std::vector<FieldUse> uses;           // the first 8
};
// A store of our task, of a value read from it, or of either changed (a tagged pointer), anywhere in the function.
struct TaskStore {
  std::uint32_t offset = 0;
  std::uint8_t via = 0;                 // the register stored
  Source value;                         // what it held
  std::optional<std::uint8_t> base;
  std::optional<Source> base_src;       // what the address's base holds
  std::optional<std::uint8_t> index;
  std::int64_t disp = 0;                // RIP-relative: the slot's RVA
  bool rip = false;
  std::uint8_t width = 0;
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
  std::vector<Form> forms;              // spike.8: the first 64 instructions, as forms
  bool forms_dropped = false;           // left out so that the report stays within its size
  bool trimmed = false;                 // spike.9: its call sites, branches and guards left out for the same reason
  // Spike.9: the loads from our task and from the group's entry (the first 16 of each), and the stores of our task or
  // of a value read from it (the first 16), with how many there were.
  std::uint32_t task_read_count = 0;
  std::vector<FieldRead> task_reads;
  std::uint32_t entry_read_count = 0;
  std::vector<FieldRead> entry_reads;
  std::uint32_t task_store_count = 0;
  std::vector<TaskStore> task_stores;
  // Spike.10 (WHY§289): the loads from the second singleton G (rcx at G's method's entry, or a value loaded from it),
  // the first 16, and the uses of the entry's word (r8 at its entry), the first 16, with how many there were.
  std::uint32_t g_read_count = 0;
  std::vector<FieldRead> g_reads;
  std::uint32_t word_use_count = 0;
  std::vector<FieldUse> word_uses;
};
// A callee of the summarised function (a direct call's target, or a tail jump's), summarised in turn with the
// arguments its call site passes it: its `args` are the wrapper's (spike.7, WHY§235).
struct InnerCallee {
  std::uint32_t rva = 0;
  std::uint32_t site = 0;  // the call's (or jump's) offset in the wrapper
  bool tail = false;
  CalleeCode code;
  // Spike.8: one more level, the function the tail function's table entry leads to (its `site` the tail function's
  // jump): 2; else 1, and not written. Spike.9: the group's own function (reached by the tail function's direct jump,
  // or through the entry's vtable) at 2, and its callees at 3, each `site` in the function at `parent`. Spike.10: G's
  // method at 3 (its site the group's function's, through a stub that spends no level) and its callees at 4.
  std::uint32_t depth = 1;
  std::uint32_t parent = 0;              // the RVA of the function holding the site, past depth 1
  std::vector<PassedSource> entry_src{};  // spike.9: the site's known values that are no argument, past depth 1
  // Spike.10: the stub (16 bytes or fewer, ending in a direct tail jump) the site reaches it through, and the registers
  // of rcx to r9 that stub leaves zero (G's method is entered with r9 0).
  std::optional<std::uint32_t> stub{};
  std::vector<std::uint8_t> zeroed{};
};

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
  std::optional<std::uint32_t> tag{};   // spike.9, a tagged hit: the word's high 16 bits (its low 48 our address)
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
  // Spike.9 (WHY§248): words whose low 48 bits equal our object's (or our vtable's) address while their high 16 bits
  // are not zero, a pointer under a tag; the game's first, then ours, at most 8 of each side and kind.
  std::uint64_t tagged_found = 0;
  std::uint64_t own_tagged_found = 0;
  std::uint64_t vtable_tagged_found = 0;
  std::uint64_t own_vtable_tagged_found = 0;
  std::vector<MemoryHit> tagged_hits;
};

// Spike.7 (WHY§235): the hub's reachable memory just before the call, kept, and compared with the same walk 2 and 20 s
// after it, block by block by path (probe.hpp's walk_hub and diff_hub). Numbers and kinds only.
// Spike.9: TaskTagged, a word whose low 48 bits are our task's address or point inside it, its high 16 bits not zero.
enum class ValueKind : std::uint8_t { Zero, Small, Group, Task, Vtable, Ours, Game, Heap, Other, TaskTagged };
struct WordValue {
  ValueKind kind = ValueKind::Other;
  std::optional<std::uint64_t> value;   // Small, Group: the value, 1 to 65,536
  std::optional<std::uint64_t> offset;  // Task, Vtable, TaskTagged: how far into ours it points, when not at the start
  SectionKind section = SectionKind::None;  // Game: the section's kind, by its characteristics
  std::optional<std::uint32_t> rva;     // Game, Ours
  std::optional<bool> fresh;            // a new pointer into memory: the walk before the call reached no block there
  bool group_half = false;              // a 32-bit half of the word equals the group's index
  std::optional<std::uint32_t> tag{};   // spike.9, TaskTagged: the high 16 bits
};
struct DiffWord {
  std::uint32_t depth = 0;              // pointers followed from the hub's instance
  std::vector<std::uint32_t> path;      // the offset read at each level, the last the word's own
  WordValue before;
  WordValue after;
  std::optional<bool> steady;           // the later diff: the word held the same new value at the earlier one
  std::optional<bool> in_control;       // spike.8: the same path's word changed in the control diff, before the call
};
struct HubDiff {
  // Spike.9: the walk's root: task_hub (CSTask's instance), singleton (the tail function's) or group_entry (the
  // group's entry in its table); empty, not written.
  std::string_view root;
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
  std::optional<std::uint64_t> in_control_words;  // spike.8: changed words whose path's word changed in the control too
  std::vector<DiffWord> words;          // the first 32 (in the later diff, the steady ones first; since spike.8 those
                                        // not in the control before those in it)
};

// Spike.8 (WHY§240): the table the tail function reads, when the wrapper's and the tail function's shapes match run 7's
// reading (probe.hpp's match_table): the wrapper's rcx at its tail jump loaded from a RIP-relative slot (the singleton's
// global), directly or through one [reg + d] load, and the tail function's null-tested register loaded from
// [entry rcx + group*8 + disp], or the same one level deeper. Read before the call and 2 s after it.
struct TableRead {
  std::uint64_t at_ms = 0;               // after the call (before it: 0)
  bool read = false;                     // the singleton and the entries were read
  std::optional<WordValue> singleton;    // *S (through the hop), when it could be read
  bool is_instance = false;              // CSTask's instance, as R1 passes it
  std::optional<std::uint32_t> singleton_lead;  // a named singleton's instance: its index in the leads' singletons
  std::uint32_t entries = 0;             // read: 0 to the bound
  std::uint32_t nonzero = 0;
  std::vector<std::uint32_t> nonzero_at;  // their indices
  std::optional<WordValue> entry;        // the group's
  std::optional<WordValue> entry_vtable; // its first word
  std::optional<WordValue> slot_value;   // the word at [first word + slot_disp]: the function the final jump reaches
};
// Spike.9 (WHY§248): an object's RTTI class through its first word, as O92 names the singletons by their instances,
// against the leads' names: the lead's index where the class's innermost name is the lead's name, or it with Imp.
enum class LeadMatch : std::uint8_t { None, Name, Imp };
struct ClassMatch {
  bool vtable = false;                 // the first word is a vtable of the image's RTTI walk
  bool classed = false;                // its locator names a class (a complete object's, at offset 0)
  std::uint32_t type_descriptor = 0;   // RVA
  LeadMatch match = LeadMatch::None;
  std::optional<std::uint32_t> lead;   // the lead's index in the leads' singletons
};
struct Table {
  bool matched = false;
  std::string_view failed;                // when not matched: wrapper_tail, wrapper_rcx, tail_summary, tail_bound,
                                          // tail_null_test
  std::uint32_t singleton_slot = 0;       // S's RVA
  std::optional<std::int64_t> hop;        // rcx = [*S + hop]
  bool via_guard = false;                 // read on the path past the wrapper's null test of rcx, where the paths
                                          // into the tail jump disagree on rcx
  std::optional<std::int64_t> array;      // the entries at [singleton + array] (one level deeper)
  std::int64_t disp = 0;                  // entry i at table + disp + i*8
  std::uint32_t bound = 0;                // the group's bound, from the tail function's compare
  std::optional<std::int64_t> slot_disp;  // the final jump goes through [entry's first word + slot_disp]
  std::uint32_t rva = 0;                  // the tail function
  std::optional<std::uint32_t> site;      // its final jump
  std::optional<std::uint32_t> target;    // spike.9: where that jump is direct, its target (the group's function)
  std::optional<TableRead> before, after;
  std::optional<ClassMatch> singleton_class, entry_class;  // spike.9: read just before the call
};

// Spike.10 (WHY§289): the second singleton G and its method. The group's function loads G from a data slot and
// tail-jumps (through a stub) to G's method with G in rcx, our task in rdx and a word of the entry in r8. Read just before
// the call and 2 s after it: G (null or heap), its RTTI class against the leads, whether G is the object CSTask's
// instance holds at `hub_off` and the group's entry at `entry_off` (run 9's walks reached one object there from every
// root), and the entry's word: group 110's, and over every entry of the table how many hold their own index there and
// how many distinct values there are. Numbers only.
struct MethodRead {
  std::uint64_t at_ms = 0;              // after the call (before it: 0)
  bool read = false;                    // G's slot was read
  std::optional<WordValue> g;           // what the slot holds
  std::optional<ClassMatch> g_class;
  std::optional<bool> at_hub;           // G is the word CSTask's instance holds at hub_off
  std::optional<bool> at_entry;         // G is the word the group's entry holds at entry_off
  std::optional<std::uint32_t> word;    // the group's entry's word
  std::uint32_t words_read = 0;         // entries whose word was read
  std::uint32_t own_index = 0;          // of those, holding their own index
  std::uint32_t distinct = 0;           // distinct values among them
};
struct Method {
  std::uint32_t site = 0;                 // the jump (or call) in the group's function that reaches G's method
  std::uint32_t slot = 0;                 // G's data slot (RVA)
  std::optional<std::uint32_t> stub;      // the stub the site reaches it through
  std::uint32_t rva = 0;                  // G's method
  std::optional<std::int64_t> word_disp;  // the entry's word: its offset in the entry, and its width
  std::uint8_t word_width = 0;
  std::uint32_t hub_off = 0, entry_off = 0;
  // G as the walk's fourth root: walked; is_root (G is a root's object); held_by_root (G is the object CSTask's instance
  // or the entry holds at those offsets, already in their walks); none (no G to walk).
  std::string_view g_walk = "none";
  std::optional<MethodRead> before, after;
};

// Spike.10 (WHY§289): the array behind CSTask's instance at +0x3B0, then +0x3D0, that changed after the call in runs 7
// to 9, seen by the hub's walk as four views (depth 3, four sibling pointers 32 bytes apart). Each view's bytes as the
// walk before the call kept them, read again about 50 and 250 ms after the call and at 2 s: the words that changed, each
// value's kind, the RTTI class of a new value in the game's rdata (a vtable), and whether a changed word (either 32-bit
// half) equals the game's main thread id (the main window's thread) or our calling thread's (T1). Numbers only.
struct ArrayView {
  std::vector<std::uint32_t> path;
  bool found = false;                   // the walk before the call reached a block by that path
  std::uint32_t bytes = 0;
};
struct ViewWord {
  std::uint32_t offset = 0;
  WordValue before, after;
  std::optional<ClassMatch> after_class;  // a new value in the game's rdata: its class, where it is a vtable
};
struct ViewRead {
  std::uint64_t at_ms = 0;
  std::uint32_t view = 0;               // the view's index
  bool read = false;
  std::uint32_t changed = 0;            // 8-byte words that differ from the walk's
  std::vector<ViewWord> words;          // the first 8
  bool main_thread = false;
  bool caller_thread = false;
};
inline constexpr std::size_t kMaxViewWords = 8;
struct ArrayViews {
  std::vector<ArrayView> views;
  std::vector<ViewRead> reads;
};

// Spike.10 (WHY§289): what the game's own call sites of register_task pass as the task (its register before the call),
// read from the code before each call back to the register's last write: an embedded member (lea reg, [base + disp]),
// a static object (lea reg, [rip + slot]), a load (mov reg, [base + disp] or [rip + slot]), an allocator's result (a
// call's rax), the function's own argument (no write in the function before the call), or other; with what the base
// holds where there is one (one level), how many instructions lie between the write and the call, and whether a branch
// lands between them (another path may set it).
enum class ArgForm : std::uint8_t { Member, Static, Load, Result, Argument, Stack, Other };
struct SiteArg {
  std::uint32_t site = 0;                     // the call instruction (RVA)
  bool decoded = false;                       // the function decoded from its start onto the call
  ArgForm form = ArgForm::Other;
  std::optional<std::uint8_t> base;           // member, load: the address's base register
  std::int64_t disp = 0;                      // member, load: the displacement
  std::optional<std::uint32_t> target;        // static, a RIP-relative load: the RVA; result: a direct call's target
  bool rip = false;
  std::optional<ArgForm> base_form;           // member, load: what the base holds
  std::optional<std::uint32_t> base_target;
  std::optional<std::int64_t> base_disp;
  std::uint32_t distance = 0;                 // instructions from the write to the call
  bool joined = false;
};
// Spike.10: the registration's sibling of the same argument shape (derive's second candidate), summarised with its
// callees down the same depth as the registration's chain, for comparison.
struct Sibling {
  std::uint32_t rva = 0;
  std::uint32_t sites = 0;
  CalleeCode code;
  std::vector<InnerCallee> inner;
  std::uint32_t inner_dropped = 0;
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
  std::optional<WordValue> returned;   // spike.8: rax as the call left it, as a kind
  std::optional<std::uint8_t> returned_low;  // its low byte (a bool's answer sets al alone)
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
  std::optional<HubDiff> hub_diff_control;  // spike.8: the hub's walk at the Mark against the walk 2 s later
  std::optional<Table> table;               // spike.8
  std::vector<HubDiff> hub_diff_controls;   // spike.9: the same for the singleton's and the group entry's walks
  std::uint32_t inner_dropped = 0;          // spike.9: inner summaries left out so that the report stays within its size
  // Spike.10 (WHY§289): G and its method, the array's views across the call, the game's own call sites' tasks, and the
  // registration's sibling.
  std::optional<Method> method;
  std::optional<ArrayViews> views;
  std::vector<SiteArg> game_sites;
  std::optional<Sibling> sibling;
};

[[nodiscard]] const report::Vocabulary& vocabulary() noexcept;
[[nodiscard]] proto::Expected<std::string, report::Problem> write_report(const Findings& f);

}  // namespace en::gal::probe
