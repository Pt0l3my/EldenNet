#pragma once
// The probe option (EN_PROBE; ARCH §2.5, §6.10 step 3; O89's R1): what a dev build adds for the in-game round that
// confirms R0's derivation. Compiled only with EN_PROBE (en_gal_game_probe); a release build has none of it.
// - Slot counts: how often the game called each slot of our task's fake vtable (SysLayer::slot_calls), so the run slot
//   is confirmed by the game's own calls.
// - The group check's search: where the layout's group name really is (ErBackend::first_tick's found_at).
// - Fault injection: GuardedReader::inject, a real fault on every n-th read of a group (S0-D's breakers).
// - Searches by invariant, here: a static's slot found as the .data slot whose instance has its class's vtable (plan
//   §2: the AOB statics' slots, which R0 cannot find offline and `probe-exe --static` then cuts patterns at), and pure
//   searches over one copied object's bytes (offsets of a vtable, of a (max, current) pair, of a known UTF-16 text, of
//   a value), whose answers are offsets only: never the bytes, and never a name.
// - On T1, for R1's spike.6 (WHY§227): the registration's callee summarised from its code, and the whole process
//   searched for our task object's address, numbers and kinds only; for spike.7 (WHY§235), the arguments followed into
//   the callee's own callees, our vtable's and slots' addresses searched too, and the hub's reachable memory kept from
//   before the call and compared after it; for spike.8 (WHY§240), the instructions as forms, the values loaded from
//   memory followed, jumps through memory or a register as tail calls, the tail function's table matched and read, and
//   a control diff of the hub before the call; for spike.9 (WHY§248), the group's own function and its callees with
//   our task's and the entry's loads followed to their uses, the walk from two more roots, tagged pointers, and the
//   RTTI classes of the singleton and the entry.
// Every read is a guarded copy on the game thread within the tick's budget, never into a known stack (WHY§111); the
// candidates come from the image's own data section, read on T1.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/discovery.hpp"
#include "en/gal/guarded.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/probe_report.hpp"
#include "en/gal/sys.hpp"

namespace en::gal::probe {

#if EN_PROBE

// R1's candidates for a static's slot, read on T1 from the image: every 8-aligned slot of the writable data sections
// whose value lies in user space outside the image (an instance on the heap), at most `max`, by RVA.
struct HeapSlots {
  std::vector<std::uint32_t> slots;
  std::vector<std::uint64_t> values;  // what each slot held when it was read
  std::uint64_t unreadable = 0;       // bytes of those sections that could not be read
};
[[nodiscard]] HeapSlots heap_slots(const SysLayer& sys, const ImageInfo& image, std::size_t max = 1u << 16);

// The 8-aligned slots of the writable data sections that hold `value` (R1: global_hinstance, the module's base), by
// RVA, at most `max`; `count` is how many there were.
struct Holding {
  std::vector<std::uint32_t> slots;
  std::uint64_t count = 0;
};
[[nodiscard]] Holding slots_holding(const SysLayer& sys, const ImageInfo& image, std::uint64_t value,
                                    std::size_t max = 16);

// The static-slot search for one class's instance.
class SlotSearch {
 public:
  // Collects the candidates on T1: every 8-aligned slot of the image's writable data sections whose value lies in user
  // space outside the image (an instance on the heap), at most `max_candidates`.
  SlotSearch(const SysLayer& sys, const Discovery& discovery, roles::Cls cls, std::size_t max_candidates = 1u << 16);
  // The same for a vtable set of the caller's (R1, before any discovery: a class's vtables from the live derivation).
  SlotSearch(const SysLayer& sys, const ImageInfo& image, const VtableSet& set, std::size_t max_candidates = 1u << 16);
  // On the game thread: checks up to `copies` candidates (a guarded copy of each one's first 8 bytes, compared with the
  // class's vtables). Answers whether every candidate has been checked.
  // Nothing it does on the game thread allocates (R6). Its progress and finds may be read from any thread.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies);
  [[nodiscard]] std::size_t candidates() const noexcept { return slots_.size(); }
  [[nodiscard]] std::size_t checked() const noexcept { return next_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint64_t unreadable() const noexcept { return unreadable_.load(std::memory_order_relaxed); }
  // The slots (RVAs) whose instance has one of the class's vtables, the first kMaxFound.
  static constexpr std::size_t kMaxFound = 16;
  [[nodiscard]] std::span<const std::uint32_t> found() const noexcept {
    return std::span<const std::uint32_t>(found_.data(), found_count_.load(std::memory_order_acquire));
  }

 private:
  const VtableSet& set_;
  std::vector<std::uint32_t> slots_;
  std::vector<std::uint64_t> values_;
  std::array<std::uint32_t, kMaxFound> found_{};
  std::atomic<std::size_t> found_count_{0};
  std::atomic<std::size_t> next_{0};
  std::atomic<std::uint64_t> unreadable_{0};
};

// The rising-counter search (R1: a static whose class has no RTTI, such as the one that holds the play time): each
// candidate's instance's first `span` bytes, copied pass after pass on the game thread (each at least a second after
// its last copy); a 4-aligned 32-bit field that rose between two copies by the time between them in milliseconds,
// within a fifth, is a hit. Each hit keeps
// how many passes it rose in and how many it stood still in (a play time stands still at the title and in menus, a
// clock of the process never does). Everything is allocated on T1; the game thread allocates nothing (R6).
class RisingSearch {
 public:
  struct Hit {
    std::uint32_t slot = 0;    // the static's RVA
    std::uint32_t offset = 0;  // the field's offset in the instance
    std::uint32_t rose = 0;    // passes it rose in by the elapsed milliseconds
    std::uint32_t still = 0;   // passes it held its value in
    std::uint32_t other = 0;   // passes it changed otherwise
  };
  static constexpr std::size_t kMaxHits = 32;
  static constexpr std::uint64_t kMinIntervalUs = 1'000'000;  // a candidate is copied again a second later at least

  RisingSearch(HeapSlots candidates, std::uint32_t span = 0x100, std::size_t max_candidates = 1u << 14);
  // Copies up to `copies` candidates at `now_us` (the tick's clock); answers whether a pass ended on this call.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies, std::uint64_t now_us);
  [[nodiscard]] std::size_t candidates() const noexcept { return slots_.size(); }
  [[nodiscard]] std::uint32_t passes() const noexcept { return passes_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint64_t unreadable() const noexcept { return unreadable_.load(std::memory_order_relaxed); }
  // The hits so far, at most kMaxHits; read from any thread once a pass has ended (they change only as one ends).
  [[nodiscard]] std::span<const Hit> hits() const noexcept {
    return std::span<const Hit>(hits_.data(), hit_count_.load(std::memory_order_acquire));
  }

 private:
  void compare(std::size_t i, std::span<const std::byte> now, std::uint64_t now_us) noexcept;

  std::uint32_t span_;
  std::vector<std::uint32_t> slots_;
  std::vector<std::uint64_t> values_;
  std::vector<std::byte> last_;           // each candidate's previous copy, span_ bytes each
  std::vector<std::uint8_t> dead_;        // a candidate whose copy faulted is not copied again
  std::vector<std::uint64_t> last_us_;    // and when it was made; 0 before the first
  std::vector<std::byte> scratch_;
  std::size_t next_ = 0;
  std::array<Hit, kMaxHits> hits_{};
  std::atomic<std::size_t> hit_count_{0};
  std::atomic<std::uint32_t> passes_{0};
  std::atomic<std::uint64_t> unreadable_{0};
};

// The hub census (R1: the task's base class): the objects the task hub's instance points at, to a depth of three, whose
// first 8 bytes are one of the image's vtables. The hub's own tasks are among them, so the class they share is the
// task's base. Each level's pointers are followed within a cap; every read is a guarded copy on the game thread within
// the tick's budget, and a copy that faults is made again at half its length, down to 16 bytes (a small object near
// the end of its allocation). Everything is allocated on T1.
class Census {
 public:
  static constexpr std::size_t kMaxFound = 256;
  // `vtables`: every vtable of the image (absolute, sorted), from T1's RTTI walk; `hub_static`: the hub's static
  // (absolute); `module_base`, `image_size`: pointers into the image are not followed, but for those into `data`, the
  // image's writable sections (absolute), where a game may keep objects of its own, its tasks among them.
  Census(std::vector<std::uint64_t> vtables, std::uint64_t hub_static, std::uint64_t module_base,
         std::uint64_t image_size, std::vector<AddressRange> data = {});
  // Up to `copies` copies; answers whether the census is complete.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies);
  [[nodiscard]] bool done() const noexcept { return done_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint64_t copies() const noexcept { return copies_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t faults() const noexcept { return faults_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t objects() const noexcept { return objects_.load(std::memory_order_relaxed); }
  // The distinct vtables found (absolute), and how many objects had each; complete once done().
  [[nodiscard]] std::span<const std::uint64_t> vtables() const noexcept {
    return std::span<const std::uint64_t>(found_.data(), found_count_.load(std::memory_order_acquire));
  }
  [[nodiscard]] std::span<const std::uint32_t> counts() const noexcept {
    return std::span<const std::uint32_t>(found_n_.data(), found_count_.load(std::memory_order_acquire));
  }

 private:
  struct Item {
    std::uint64_t address = 0;
    std::uint32_t span = 0;  // the next copy's length: the level's, halved after each fault down to 16 bytes
    std::uint8_t level = 0;
  };
  static constexpr std::array<std::uint32_t, 4> kSpan = {0x400, 0x200, 0x100, 0x8};
  static constexpr std::array<std::size_t, 4> kCap = {1, 256, 4096, 16384};
  void found(std::uint64_t vtable) noexcept;
  bool follow(std::uint64_t v) const noexcept;

  std::vector<std::uint64_t> vtables_;
  std::vector<AddressRange> data_;
  std::uint64_t hub_static_;
  std::uint64_t base_;
  std::uint64_t image_end_;
  std::vector<Item> queue_;  // breadth first; reserved for every level's cap
  std::array<std::size_t, 4> queued_{};
  std::size_t next_ = 0;
  std::vector<std::byte> scratch_;
  std::vector<std::uint64_t> seen_;  // the addresses queued: an open-addressed set of a fixed size, never grown
  std::array<std::uint64_t, kMaxFound> found_{};
  std::array<std::uint32_t, kMaxFound> found_n_{};
  std::atomic<std::size_t> found_count_{0};
  std::atomic<std::uint64_t> copies_{0};
  std::atomic<std::uint64_t> faults_{0};
  std::atomic<std::uint64_t> objects_{0};
  std::atomic<bool> done_{false};
  bool started_ = false;
};

// Searches over one copied object's bytes: the offsets (ascending) where each invariant holds.
// - An 8-aligned pointer that is one of a set's vtables (RTTI vtables at object starts, or at subobjects).
[[nodiscard]] std::vector<std::uint32_t> vtable_offsets(std::span<const std::byte> obj, const VtableSet& set);
// - Two adjacent signed 32-bit values (max, current) with 0 < max <= 100000 and 0 <= current <= max: hp beside max_hp.
//   `max_first` says which comes first.
[[nodiscard]] std::vector<std::uint32_t> meter_offsets(std::span<const std::byte> obj, bool max_first);
// - A UTF-16 text the caller knows (such as the character's own name, which is never recorded), NUL-terminated.
[[nodiscard]] std::vector<std::uint32_t> utf16_offsets(std::span<const std::byte> obj, std::u16string_view text);
// - A 32-bit value (such as the flag divisor), at 4-aligned offsets.
[[nodiscard]] std::vector<std::uint32_t> u32_offsets(std::span<const std::byte> obj, std::uint32_t value);

// ---- Spike.6 (WHY§227): the callee as its code reads, and the whole process searched for our task ----

// One section of the image: its RVA, size and kind (by its characteristics).
struct SectionSpan {
  std::uint32_t rva = 0;
  std::uint32_t size = 0;
  SectionKind kind = SectionKind::Other;
};

inline constexpr std::uint32_t kCalleeBound = 4096;  // bytes of the callee decoded at most

// What rcx, rdx, r8 and r9 hold at a function's entry: one of the summary's arguments (`from`: 1, 2, 8 or 9; 0 none),
// or the first word of that argument's object. A function summarised on its own holds its own arguments; in an inner
// summary a register that holds none holds the value it was entered with (Source's Entry, spike.8). Spike.9: `entry`,
// a register holding no argument holds the group's entry (its loads are `entry_reads`).
// Spike.10 (WHY§289): `g`, a register holding no argument holds the second singleton G (its loads are `g_reads`);
// `word`, it holds the entry's word (its uses are `word_uses`); `zero`, a stub left it zero (reported only).
// Spike.11 (WHY§293): `request`, it holds the request G's method builds (its loads are `request_reads`, and a load of
// the request's task field holds the task); `manager`, it holds G's manager, the request's field of G's (its loads are
// `manager_reads`).
// Spike.12 (WHY§296): `value`, it holds the value of the manager's map that the now path's function passes on a match
// (its loads are `value_reads`).
struct EntryArg {
  std::uint8_t from = 0;
  bool first_word = false;
  bool entry = false;
  bool g = false;
  bool word = false;
  bool zero = false;
  bool request = false;
  bool manager = false;
  bool value = false;
};
using EntryState = std::array<EntryArg, 4>;
inline constexpr EntryState kOwnArguments = {{{1, false, false}, {2, false, false}, {8, false, false},
                                              {9, false, false}}};
// The entry state of the function a call (or a tail jump) reaches: its rcx, rdx, r8 and r9 as the call's `pass` says.
[[nodiscard]] EntryState entry_at(const CalleeCall& call) noexcept;
// The same from a function entered with `parent` (spike.9): a register the call's `pass_src` gives as one of the
// function's entry values that held the group's entry holds it in the callee too; since spike.10 the same for G, the
// entry's word and a zero.
[[nodiscard]] EntryState entry_at(const CalleeCall& call, const EntryState& parent) noexcept;
// Spike.10 (WHY§289): the entry state of G's method, reached from the group's function (entered with `parent`) by
// `site`: as entry_at gives it, and rcx, loaded from a RIP-relative slot 64 bits wide, holds G; a register loaded from
// the group's entry (a value read from [entry + d]) holds the entry's word.
[[nodiscard]] EntryState method_entry_at(const CalleeCall& site, const EntryState& parent) noexcept;
// The site in the group's function that reaches G's method: the first reached tail jump (else call) with a target that
// passes our task (`task`, the wrapper's argument) with rcx loaded from a RIP-relative slot 64 bits wide.
[[nodiscard]] const CalleeCall* method_site(const CalleeCode& group, std::uint8_t task) noexcept;
// A stub: a summary of 16 bytes or fewer, with no call, no branch and no return, whose last instruction is a reached
// direct tail jump (that jump; else null). Followed through without spending a depth level (spike.10).
inline constexpr std::uint32_t kMaxStubBytes = 16;
[[nodiscard]] const CalleeCall* stub_jump(const CalleeCode& stub) noexcept;
// The entry state at a stub's target, the stub entered with `stub_entry`: entry_at through its jump, and `zero` where
// the stub zeroed the register (xor r, r, or mov r, 0) and wrote it no more.
[[nodiscard]] EntryState through_stub(const CalleeCode& stub, const EntryState& stub_entry) noexcept;
// Spike.10: what a call site passes in `reg` (the task's register), read from the code of the function holding it,
// decoded from its start (`fn_rva`, `code` its bytes from there on) onto the call at `call_rva` (probe_report.hpp's
// SiteArg).
[[nodiscard]] SiteArg task_argument(std::span<const std::byte> code, std::uint32_t fn_rva, std::uint32_t call_rva,
                                    std::uint8_t reg);

// What the code at `rva` does, decoded with en-gal's decoder from its first byte and following no call: within its
// function (`extent`, its length in the exception directory, at most kCalleeBound), or, with no extent, up to its first
// return. `code` holds its bytes from `rva` on, as the image holds them. The arguments rcx, rdx, r8 and r9 (as `entry`
// gives them) are followed through the moves between registers (64- and 32-bit), and a register loaded from
// [argument + 0] holds that argument's object's first word (its vtable, for a virtual call). What each register holds
// is known along the flow (spike.7): where two paths meet, a register keeps what it holds only when both agree, and
// code no known path reaches holds nothing. Before the first call (in address order) each argument's stores to memory,
// its moves to callee-saved registers, and its compares and tests are kept, and each conditional branch's flag test
// (`guards`); at every call and tail jump, which argument each of rcx, rdx, r8 and r9 holds (`pass`). An instruction
// whose effect on the registers the summary does not model clears them all on its path, and the first before the first
// call is reported (`modelled_to`): nothing is guessed past it.
// Spike.8 (WHY§240): a register also holds a value loaded from memory (a 64- or 32-bit mov, movzx, movsx or movsxd
// from a RIP-relative slot or from [base + index*scale + disp], with what base and index hold, at most 3 loads deep),
// moved as an argument is; a call's rax holds its result, and a 64-bit store of a register holding a result (or
// nothing known) to a RIP-relative slot makes it, and every register holding that result, hold the slot's value.
// Every other write clears it. With no extent, decoding also ends at the first jump (direct, or through memory or a
// register) that no branch or jump decoded before it goes past: a tail jump's padding is the next function's. A jump
// through memory or a register is a tail call site (TailMemory, TailRegister); each
// guard's register operand that holds no argument gives its `source`; the first 64 instructions are given as forms
// (`forms`), those no known path reaches marked `unreached`.
// Spike.9 (WHY§248): with no extent, a direct jump past the decoded code (or before the entry) is a tail jump too. With
// `task` (the argument, 1, 2, 8 or 9, that holds our task), every load whose base holds the task or a value loaded from
// it is a task read, and every load whose base holds the group's entry (EntryArg's `entry`) or a value loaded from it an
// entry read, each with what its value is used for along the flow (compared with an immediate or otherwise, tested,
// stored, passed into a call, called through, used as a base, changed); a compare, test or other use of such a field in
// place is a read too. A followed value changed by add, or, xor, and, sub, a shift, a bit set or a lea stays followed
// (Source's Modified), and each store of the task, of a value read from it, or of either changed is a task store.
// Spike.10: with no extent, a direct jump to one of `starts` (the RVAs where the exception directory's functions begin,
// ascending) goes past nothing: a forward tail jump (a stub's, into the function after it) ends the decoding too.
[[nodiscard]] CalleeCode summarise_callee(std::span<const std::byte> code, std::uint32_t rva, std::uint32_t extent,
                                          const EntryState& entry = kOwnArguments, std::uint8_t task = 0,
                                          std::span<const std::uint32_t> starts = {});
inline constexpr std::size_t kMaxForms = 64;  // instructions given as forms, per summary (with parts, 128)
inline constexpr std::size_t kMaxFieldReads = 16, kMaxFieldUses = 8, kMaxTaskStores = 16;

// ---- Spike.11 (WHY§293): a function in parts, the request G's method builds, and the request's methods ----

// An entry of the exception directory chained to another (UNW_FLAG_CHAININFO, or an entry that points at another),
// with the primary entry its chain leads back to.
struct ChainedEntry {
  std::uint32_t begin = 0, end = 0;
  std::uint32_t primary = 0;  // the primary entry's begin
};
// Every chained entry of `functions` (ImageInfo's, sorted), read from the image's unwind information, by primary then
// begin; a chain of more than 16 links, or one that cannot be read, is left out.
[[nodiscard]] std::vector<ChainedEntry> chained_entries(const ImageBytes& img,
                                                        std::span<const RuntimeFunction> functions);
// What a summary knows of the image around it: the exception directory's starts (spike.10's), its entries and the
// chained ones (for the parts), and the request whose fields it follows (G's method's, once found).
struct SummaryOptions {
  std::span<const std::uint32_t> starts;
  std::span<const RuntimeFunction> functions;
  std::span<const ChainedEntry> chained;
  const RequestShape* request = nullptr;
};
// A function with an extent is decoded in parts (`parts`) where its code goes past it: its primary entry, then each
// entry chained to it that lies after its start within `code`, then, at each conditional branch's target past what is
// decoded that lies in no entry, linear decoding from there (unlisted) up to kPartBound bytes, until a return, a trap,
// a jump through memory or a register, a direct jump not back into a part, another entry's start or a part already
// decoded; at most kMaxParts parts. A jump into a part is no tail jump. `code` holds the bytes from `rva` on (the
// parts' bytes among them, at most kPartSpan). Its forms are its first kMaxForms instructions, or with parts 128.
inline constexpr std::uint32_t kPartBound = 4096, kPartSpan = 64u << 10;
inline constexpr std::size_t kMaxParts = 8;
[[nodiscard]] CalleeCode summarise_callee(std::span<const std::byte> code, std::uint32_t rva, std::uint32_t extent,
                                          const EntryState& entry, std::uint8_t task, const SummaryOptions& options);
// The request G's method builds (probe_report.hpp's RequestShape), from its summary entered with `entry`: the first
// 8-byte store to [rsp + d] of a RIP-relative address in rdata (by `sections`) with an 8-byte store of the task (`task`,
// the wrapper's argument) at [rsp + d + 8]; its fields every store to [rsp + d] to [rsp + d + 64), the first at each
// place; `now` the target of the first reached direct call with rcx [rsp + d]. Nothing when there is none. T1 fills its
// class and slots.
[[nodiscard]] std::optional<RequestShape> request_shape(const CalleeCode& method, const EntryState& entry,
                                                        std::uint8_t task, std::span<const SectionSpan> sections);
// The entry state at a call in a function entered with `parent`, the request known: as entry_at gives it, and a
// register holding the request's stack address (G's method's), or a register the parent's request holds (carried by
// entry_at), holds the request; one loaded from the request's task field holds the task (`task`), from its manager
// field G's manager, from its word field the entry's word.
[[nodiscard]] EntryState request_entry_at(const CalleeCall& call, const EntryState& parent,
                                          const RequestShape& request, std::uint8_t task) noexcept;

// One region of the process's address space, as VirtualQuery describes it (NativeSys::region_at in game; a test's
// list otherwise).
struct MemoryRegion {
  std::uint64_t base = 0;
  std::uint64_t size = 0;
  std::uint64_t allocation_base = 0;
  std::uint32_t protect = 0;  // the PAGE_* value
  bool committed = false;
  RegionKind kind = RegionKind::Other;
};
// Windows's page protections, as VirtualQuery gives them.
inline constexpr std::uint32_t kPageNoAccess = 0x01, kPageReadOnly = 0x02, kPageReadWrite = 0x04,
                               kPageWriteCopy = 0x08, kPageExecute = 0x10, kPageExecuteRead = 0x20,
                               kPageExecuteReadWrite = 0x40, kPageExecuteWriteCopy = 0x80, kPageGuard = 0x100,
                               kPageNoCache = 0x200, kPageWriteCombine = 0x400;
// Whether the search reads a region: committed, readable, and neither a guard page, no-access, uncached nor
// write-combined (device memory, slow to read, never where a pointer of ours is kept).
[[nodiscard]] bool searchable(const MemoryRegion& r) noexcept;
// The region at `at` or the first after it; nothing past the last.
using RegionQuery = std::function<std::optional<MemoryRegion>(std::uint64_t at)>;

// The kind of the section holding `rva` in an image whose first page (its headers, as a copy holds them) is `headers`:
// read by its characteristics alone, never by its name.
[[nodiscard]] SectionKind section_kind(std::span<const std::byte> headers, std::uint64_t rva) noexcept;

struct MemorySearchSpec {
  std::uint64_t target = 0;        // our task object's address
  std::uint64_t object_bytes = 0;  // its length: a word in (target, target + object_bytes) points inside it
  std::uint64_t vtable = 0;        // our fake vtable's address (0: not searched)
  std::vector<std::uint64_t> slots;  // its slots' addresses, by index (our module's thunks)
  std::uint32_t group_index = 0;
  std::uint64_t game_base = 0;     // the game's module (its allocation base)
  std::uint64_t own_base = 0;      // ours
  // The allocation bases of our own memory besides our module: our heap's regions and large blocks, our task's block,
  // our vtable's.
  std::vector<std::uint64_t> own_bases;
  bool own_known = false;          // whether `own_bases` holds our heap's
  std::vector<AddressRange> skip;  // never read (the calling thread's stack)
  std::uint64_t request_vtable = 0;  // spike.11: the request's vtable address (0: not searched)
  std::uint64_t max_bytes = 64ull << 30;  // far past a game's size (run 6: 10.6 GiB): the time cap is the bound
  std::uint64_t max_us = 30'000'000;
};

// Every committed, searchable region from the lowest user address up, read through the crash-guarded copy in chunks of
// at most kMaxCopyBytes (a chunk that faults, or that a known stack or `skip` overlaps, is read again page by page,
// skipping those pages), never past the caps, counting the 8-aligned words equal to our object's address and those
// pointing inside it, equal to our vtable's and equal to one of its slots', ours apart (a region is ours when its
// allocation base is our module's or one of `own_bases`). Past a cap the regions are still walked, unread, for the
// process's committed total. The search's own buffer is never read. Run on T1; it allocates up front only.
[[nodiscard]] MemorySearch search_memory(SysLayer& sys, const RegionQuery& query, const MemorySearchSpec& spec);

// ---- Spike.7 (WHY§235): the hub's reachable memory before and after the call ----

// One block the walk read: `len` bytes at `address`, reached from the hub's instance through `depth` pointers, the
// offset of each in its block in `path`; its bytes at `at` in the walk's buffer when `read`.
struct HubBlock {
  std::uint64_t address = 0;
  std::uint32_t len = 0;
  std::uint32_t depth = 0;
  std::array<std::uint32_t, 3> path{};
  std::uint64_t at = 0;
  bool read = false;
};
struct HubWalkSpec {
  std::uint64_t instance = 0;
  std::uint64_t image_base = 0, image_end = 0;  // pointers into the game's image are not followed
  AddressRange own_image{};                     // nor into our module's
  AddressRange keep{};                          // nor into this range (our task object), which the search counts
  std::uint32_t max_depth = 3;
  std::uint64_t max_blocks = 6000;
  std::uint64_t max_bytes = 6u << 20;
};
struct HubWalk {
  std::vector<HubBlock> blocks;  // breadth first, each address once
  std::vector<std::byte> bytes;  // every read block's bytes, kept in the caller's heap
  std::uint64_t read_bytes = 0;
  std::uint64_t faults = 0;
  std::uint64_t refused = 0;     // addresses the plausibility rules refused
  bool capped = false;           // the cap of blocks or bytes stopped it
};
// Spike.5's walk (WHY§209), its blocks kept: breadth first from the hub's instance (a page, then 1 KiB per block, never
// across a page), every 8-aligned word in user space outside both images and `keep` followed, to `max_depth` pointers,
// within the caps; every read a guarded copy under the reader's plausibility rules. Run on T1.
[[nodiscard]] HubWalk walk_hub(SysLayer& sys, const HubWalkSpec& spec);
// The walk's words equal to `target` and pointing into [target, end), as the hub search reports them (spike.5).
[[nodiscard]] HubSearch search_walk(const HubWalk& walk, std::uint64_t target, std::uint64_t end,
                                    std::uint32_t group_index);

// A word that changed across the call: the block of the walk before it, the word's offset in it, its new value; and
// the block of the walk after it (spike.8: the control diff's after-walk is the call's before-walk).
struct ChangedWord {
  std::uint32_t block = 0;
  std::uint32_t offset = 0;
  std::uint64_t value = 0;
  std::uint32_t after_block = 0;
};
struct DiffSpec {
  std::uint32_t group_index = 0;
  AddressRange task{};    // our task object's block
  AddressRange vtable{};  // our fake vtable
  std::uint64_t own_base = 0;   // our module (its allocation base)
  std::uint64_t game_base = 0;  // the game's
  std::vector<SectionSpan> game_sections;  // the game's sections, by RVA, of the kinds their characteristics give
  RegionQuery query;                       // what a pointer of no other kind points into (heap or other)
  const std::vector<ChangedWord>* earlier = nullptr;  // an earlier diff's changes: each word's steadiness is read
  // Spike.8: the control diff's changes (its after-walk this diff's before-walk): a word whose before-block and offset
  // changed there too is `in_control`.
  const std::vector<ChangedWord>* control = nullptr;
};
struct DiffResult {
  HubDiff diff;
  std::vector<ChangedWord> changed;  // every changed word, at most kMaxChangedWords, for a later diff's steadiness
};
inline constexpr std::size_t kMaxChangedWords = 1u << 16;
inline constexpr std::size_t kMaxDiffWords = 32;
// The two walks compared block by block: an after-block is the before-block of its path when the path still leads to
// the same address, else the before-block at its address (reached by another path), else new. Words are compared over
// the shorter of the two; the first kMaxDiffWords changed words are described (with `earlier`, the steady ones first:
// those that held the same new value at the earlier diff; with `control`, those not in the control before those in
// it), each value's kind classified by `spec`.
[[nodiscard]] DiffResult diff_hub(const HubWalk& before, const HubWalk& after, const DiffSpec& spec);
// The kind of section characteristics name (code, read-only data, writable data, other), as section_kind reads them.
[[nodiscard]] SectionKind section_kind_of(std::uint32_t characteristics) noexcept;
// One value's kind, as the diff classifies it (zero, the group's index when `spec` gives one, small, our task, our
// vtable, ours, the game's with its section and RVA, heap, other), with no freshness (spike.8: the call's return value,
// the table's words).
[[nodiscard]] WordValue classify_value(std::uint64_t v, const DiffSpec& spec);
// Spike.10: one view of the array, as the walk kept it before the call and as read again after it: the changed 8-byte
// words (the first kMaxViewWords described, each value's kind by `kinds`), and whether a changed word's low or high
// 32 bits equal `main_thread` or `caller_thread` (0: not compared).
[[nodiscard]] ViewRead diff_view(std::span<const std::byte> before, std::span<const std::byte> after,
                                 const DiffSpec& kinds, std::uint32_t main_thread, std::uint32_t caller_thread);

// ---- Spike.8 (WHY§240): the tail function's table ----

// Each RIP-relative operand's section in `code`'s forms, guards and its loads' sources, by `sections` (the image's).
void mark_sections(CalleeCode& code, std::span<const SectionSpan> sections) noexcept;
// Whether the wrapper and its tail function have run 7's shapes (probe_report.hpp's Table): the wrapper's rcx at its
// tail jump (a reached one, whose target `inner` summarises) loaded from a RIP-relative slot S, directly or through one
// [reg + d] load (or, where the paths into the jump disagree on rcx, so loaded and null-tested by a jne whose target
// reaches the jump in straight-line code that leaves rcx as it is: `via_guard`); in the tail function a compare of the group (`group_reg`, the wrapper's argument) with an immediate,
// and a null test (a register tested with itself, 64-bit) of a value loaded from [base + index*8 + disp], its base the
// entry rcx (or loaded from [entry rcx + d0], one level deeper) and its index the group; and, where there is one, a
// jump through [that value's first word + d2], or (spike.9, run 8's) a direct tail jump with that value in rcx
// (`target`). The first shape that fails is named and nothing more is filled.
[[nodiscard]] Table match_table(const CalleeCode& wrapper, std::span<const InnerCallee> inner, std::uint8_t group_reg);
// The entry state of the group's function, entered by the tail function's jump at `site` (spike.9): as entry_at gives
// it, rcx holding the group's entry.
[[nodiscard]] EntryState group_entry_at(const CalleeCall& site) noexcept;
// A guarded read of `out.size()` bytes at an address (T1's crash-guarded copy under the reader's plausibility rules),
// false where it was refused or faulted.
using BlockReader = std::function<bool(std::uint64_t address, std::span<std::byte> out)>;
struct TableReadSpec {
  std::uint64_t module_base = 0;   // the game's
  std::uint32_t group_index = 0;
  std::uint64_t instance = 0;      // CSTask's, as R1 passes it
  std::vector<std::pair<std::uint32_t, std::uint64_t>> leads;  // each named singleton's index in the leads, its instance
  DiffSpec kinds;                  // how values are classified (its group_index 0: a value is never "group")
};
inline constexpr std::uint32_t kMaxTableEntries = 513;  // entries 0 to 512
// What the reads found, as addresses, for T1's own use (spike.9: the walks' roots, the classes): never reported.
struct TableAddresses {
  std::uint64_t singleton = 0, singleton_first = 0;  // *S (through the hop), and its first word
  std::uint64_t entry = 0, entry_first = 0;          // the group's entry, and its first word
  std::vector<std::uint64_t> entries;                // spike.10: every entry read, by index (0 where null)
};
// The reads, for a matched shape: *S (through the hop), the entries from 0 to the bound (at most 512) at
// table + disp + i*8, and for the group's entry when it is not null its first word and the word at [first word +
// slot_disp]. Kinds and indices only; with `found`, the addresses too.
[[nodiscard]] TableRead read_table(const Table& shape, const TableReadSpec& spec, const BlockReader& read,
                                   TableAddresses* found = nullptr);
// Spike.9: a class's innermost name (O92's rule) against the leads' singletons' names: the first lead whose name it is,
// or whose name with Imp it is.
void match_lead(ClassMatch& m, std::string_view cls, std::span<const std::string> names);

// ---- Spike.12 (WHY§296): the manager's map, as the now path's function walks it ----

// The map's layout from the now path's function's summary (`now`, entered with `entry`: the request in a register),
// the request's shape and the wrapper's task argument (probe_report.hpp's MapShape): the manager is the request's
// manager field; the head the manager's 8-byte field whose value is a base, the root the head's, the nil byte the
// root's 1-byte field compared in place with 0; the key the memory operand compared with the word; the call the
// reached direct call with the word in r8 and the manager in r9, an 8-byte load in rcx its displacement the value's
// (where the summary knows nothing in rcx, the paths into the call disagreeing on its base, 8 bytes past the key);
// the list the two manager fields one of which a sub takes from the other (begin, end); the other call the last direct
// call with the task in rcx before the first call through the task's first word. The children are the tree's own, at
// +0 and at the root's displacement + 8 (MSVC's node: left, parent, right). The first part not found is `failed`.
[[nodiscard]] MapShape map_shape(const CalleeCode& now, const EntryState& entry, const RequestShape& request,
                                 std::uint8_t task);
// One walk of the map and of the list, at the manager `manager`, by a derived shape: the head on the heap (`on_heap`)
// with its nil byte not zero; from the root, in order, each node on the heap, read whole (to its value's end), its nil
// byte zero, never met twice, at most kMaxMapNodes; a child equal to the head is a leaf's. The keys (`width` bytes) in
// order; whether `word` is among them, else the nearest below and above it; the values, in key order, for T1 to
// classify (never reported). The list's count, (end - begin) / 8, at most kMaxListEntries, and its entries. A check
// that fails is named (`failed`, `list_failed`) and nothing more of that part is read.
inline constexpr std::size_t kMaxMapNodes = 4096, kMaxMapKeys = 256, kMaxListEntries = 4096, kMaxValueClasses = 16;
struct MapWalk {
  MapRead read;                        // its map and list parts (without the time, the stage and the classes)
  std::optional<std::uint64_t> count_word;  // the manager's word beside the head, as read
  std::vector<std::uint64_t> values;   // each node's value, in key order
  std::vector<std::uint64_t> list;     // the list's entries
};
using HeapTest = std::function<bool(std::uint64_t address)>;
[[nodiscard]] MapWalk walk_map(const BlockReader& read, const HeapTest& on_heap, std::uint64_t manager,
                               const MapShape& shape, std::uint32_t word);

#endif

}  // namespace en::gal::probe
