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
//   before the call and compared after it.
// Every read is a guarded copy on the game thread within the tick's budget, never into a known stack (WHY§111); the
// candidates come from the image's own data section, read on T1.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "en/gal/discovery.hpp"
#include "en/gal/guarded.hpp"
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

inline constexpr std::uint32_t kCalleeBound = 4096;  // bytes of the callee decoded at most

// What rcx, rdx, r8 and r9 hold at a function's entry: one of the summary's arguments (`from`: 1, 2, 8 or 9; 0 none),
// or the first word of that argument's object. A function summarised on its own holds its own arguments.
struct EntryArg {
  std::uint8_t from = 0;
  bool first_word = false;
};
using EntryState = std::array<EntryArg, 4>;
inline constexpr EntryState kOwnArguments = {{{1, false}, {2, false}, {8, false}, {9, false}}};
// The entry state of the function a call (or a tail jump) reaches: its rcx, rdx, r8 and r9 as the call's `pass` says.
[[nodiscard]] EntryState entry_at(const CalleeCall& call) noexcept;

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
[[nodiscard]] CalleeCode summarise_callee(std::span<const std::byte> code, std::uint32_t rva, std::uint32_t extent,
                                          const EntryState& entry = kOwnArguments);

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

// A word that changed across the call: the block of the walk before it, the word's offset in it, its new value.
struct ChangedWord {
  std::uint32_t block = 0;
  std::uint32_t offset = 0;
  std::uint64_t value = 0;
};
struct SectionSpan {
  std::uint32_t rva = 0;
  std::uint32_t size = 0;
  SectionKind kind = SectionKind::Other;
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
// those that held the same new value at the earlier diff), each value's kind classified by `spec`.
[[nodiscard]] DiffResult diff_hub(const HubWalk& before, const HubWalk& after, const DiffSpec& spec);
// The kind of section characteristics name (code, read-only data, writable data, other), as section_kind reads them.
[[nodiscard]] SectionKind section_kind_of(std::uint32_t characteristics) noexcept;

#endif

}  // namespace en::gal::probe
