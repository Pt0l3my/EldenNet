#pragma once
// The probe option (EN_PROBE; ARCH §2.5, §6.10 step 3): what a dev build adds for the in-game rounds. Compiled only
// with EN_PROBE (en_gal_game_probe); a release build has none of it.
// - Slot counts: how often the game called each slot of our task's fake vtable (SysLayer::slot_calls), so the run slot
//   is confirmed by the game's own calls.
// - The group check's search: where the layout's group name really is (ErBackend::first_tick's found_at).
// - Fault injection: GuardedReader::inject, a real fault on every n-th read of a group (S0-D's breakers).
// - Searches by invariant, here: a static's slot found as the .data slot whose instance has its class's vtable (plan
//   §2: the AOB statics' slots, which R0 cannot find offline and `probe-exe --static` then cuts patterns at), the
//   rising-counter search, the object scan, and pure searches over one copied object's bytes (offsets of a vtable, of
//   a (max, current) pair, of a known UTF-16 text, of a value), whose answers are offsets only: never the bytes, and
//   never a name.
// The read-only round uses them (eldennet's win/probe_read.cpp; Lodestar's step L5). What R1 added on T1 around its one
// call into the game (the callee's summary, the whole-memory search, the hub's walks and diffs, the table, the map, the
// hub census) went with it: R1, retired 2026-11-22, O106.
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
#include "en/gal/sys.hpp"

namespace en::gal::probe {

#if EN_PROBE

// The candidates for a static's slot, read on T1 from the image: every 8-aligned slot of the writable data sections
// whose value lies in user space outside the image (an instance on the heap), at most `max`, by RVA.
struct HeapSlots {
  std::vector<std::uint32_t> slots;
  std::vector<std::uint64_t> values;  // what each slot held when it was read
  std::uint64_t unreadable = 0;       // bytes of those sections that could not be read
  // With a `live` question (below): every slot whose value could be a heap address, and what the answer was.
  std::uint64_t pointer_like = 0;
  std::uint64_t not_committed = 0;
  std::uint64_t no_access = 0;
};
// What stands at an address a slot holds, as the caller's system says without reading it (on Windows, VirtualQuery):
// memory that may be read, no memory, or memory that may not be read.
enum class Liveness : std::uint8_t { Live, NotCommitted, NoAccess };
using LiveFn = std::function<Liveness(std::uint64_t address)>;
// With `live`, only the slots whose value it answers Live for are kept (and counted against `max`); most 8-aligned
// values of a game's data that could be a heap address are not one (two small numbers side by side read as one).
[[nodiscard]] HeapSlots heap_slots(const SysLayer& sys, const ImageInfo& image, std::size_t max = 1u << 16,
                                   const LiveFn* live = nullptr);

// The 8-aligned slots of the writable data sections that hold `value` (global_hinstance, the module's base), by
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
  // The same for a vtable set of the caller's (before any discovery: a class's vtables from the live image).
  SlotSearch(const SysLayer& sys, const ImageInfo& image, const VtableSet& set, std::size_t max_candidates = 1u << 16);
  // The same over candidates the caller collected (heap_slots, with its own question of what is live).
  SlotSearch(HeapSlots candidates, const VtableSet& set);
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
  // The object each of those slots held (its address when the candidates were collected), in the same order: two
  // slots with one object are copies of one address.
  [[nodiscard]] std::span<const std::uint64_t> found_objects() const noexcept {
    return std::span<const std::uint64_t>(found_object_.data(), found_count_.load(std::memory_order_acquire));
  }
  // How many slots held one in all (found() keeps the first kMaxFound).
  [[nodiscard]] std::uint64_t found_total() const noexcept { return found_total_.load(std::memory_order_relaxed); }

 private:
  const VtableSet& set_;
  std::vector<std::uint32_t> slots_;
  std::vector<std::uint64_t> values_;
  std::array<std::uint32_t, kMaxFound> found_{};
  std::array<std::uint64_t, kMaxFound> found_object_{};
  std::atomic<std::uint64_t> found_total_{0};
  std::atomic<std::size_t> found_count_{0};
  std::atomic<std::size_t> next_{0};
  std::atomic<std::uint64_t> unreadable_{0};
};

// The rising-counter search (a static whose class has no RTTI, such as the one that holds the play time): each
// candidate's instance's first `span` bytes, copied pass after pass on the game thread (each at least a second after
// its last copy); a 4-aligned 32-bit field that rose between two copies by the time between them in milliseconds,
// within a fifth, has risen. Every field keeps how many passes it rose in, and from its first rise how many it stood
// still in and how many it changed otherwise (a play time stands still at the title and in menus, a clock of the
// process never does). The hits are the fields that rose in the most passes, chosen again as each pass ends: the best,
// not the first met. Everything is allocated on T1; the game thread allocates nothing (R6).
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
  // Copies up to `copies` candidates at `now_us` (the tick's clock); answers whether a pass ended on this call. A
  // pass is a sweep over the candidates in which at least one was copied.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies, std::uint64_t now_us);
  [[nodiscard]] std::size_t candidates() const noexcept { return slots_.size(); }
  [[nodiscard]] std::uint32_t passes() const noexcept { return passes_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint64_t unreadable() const noexcept { return unreadable_.load(std::memory_order_relaxed); }
  // Fields that rose in at least one pass, as the last pass left them.
  [[nodiscard]] std::uint64_t risen() const noexcept { return risen_.load(std::memory_order_relaxed); }
  // The hits as the last pass left them, the most passes risen first, at most kMaxHits; read from any thread. They
  // are written anew as a pass ends, into the table the readers are not looking at, and a pass takes a second at least.
  [[nodiscard]] std::span<const Hit> hits() const noexcept {
    const std::size_t which = shown_.load(std::memory_order_acquire);
    return std::span<const Hit>(hits_[which].data(), hit_count_[which].load(std::memory_order_acquire));
  }

 private:
  void compare(std::size_t i, std::span<const std::byte> now, std::uint64_t now_us) noexcept;
  void publish() noexcept;

  std::uint32_t span_;
  std::vector<std::uint32_t> slots_;
  std::vector<std::uint64_t> values_;
  std::vector<std::byte> last_;           // each candidate's previous copy, span_ bytes each
  std::vector<std::uint8_t> dead_;        // a candidate whose copy faulted is not copied again
  std::vector<std::uint64_t> last_us_;    // and when it was made; 0 before the first
  std::vector<std::byte> scratch_;
  std::vector<std::uint16_t> rose_;       // by candidate and field: passes it rose in, and from then on the others
  std::vector<std::uint16_t> still_;
  std::vector<std::uint16_t> other_;
  std::size_t next_ = 0;
  bool read_in_sweep_ = false;  // a sweep that copied no candidate is no pass
  std::array<std::array<Hit, kMaxHits>, 2> hits_{};
  std::array<std::atomic<std::size_t>, 2> hit_count_{};
  std::atomic<std::size_t> shown_{0};
  std::atomic<std::uint32_t> passes_{0};
  std::atomic<std::uint64_t> unreadable_{0};
  std::atomic<std::uint64_t> risen_{0};
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

// One region of the process's address space, as VirtualQuery describes it (NativeSys::region_at in game; a test's
// list otherwise).
enum class RegionKind : std::uint8_t { Private, Mapped, Image, Other };
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
// Whether a search reads a region: committed, readable, and neither a guard page, no-access, uncached nor
// write-combined (device memory, slow to read, never where a pointer of ours is kept).
[[nodiscard]] bool searchable(const MemoryRegion& r) noexcept;
// The region at `at` or the first after it; nothing past the last.
using RegionQuery = std::function<std::optional<MemoryRegion>(std::uint64_t at)>;

// The object scan (Lodestar's read-only round): where is an object of a class that nothing known points at? The
// process's private, writable memory is gone through once, 8-aligned word by word, for a word that is one of the
// targets' tables of methods: an object of that class starts there (level 0). Then, up to kLevels times, it is gone
// through again for words that hold the address of a node of the level before; for each such holder the nearest word
// before it that is some class's primary table of methods is taken for the start of the object it lies in (within
// kBack bytes, in the same region), and that object is a node of the next level. A level-0 node that nothing holds is
// looked at the same way: the object it lies inside, if any (`embedded`). The caller then compares each node's
// address with the image's own data, where a chain ends.
// It reads only: regions are asked for first (`regions`), and again after every kAskEvery bytes of one, so that the
// reader it is given need not ask before each piece; only committed, private, plainly writable ones outside the
// image and outside the caller's own memory are read, each piece through the guarded reader (never a thread's stack),
// and what it keeps of the memory is the nodes: a class (an offset in the image), an offset into an object, and an
// address it never reports. A found "object" may be a stale one, and an "owner" may be a neighbour: every node is a
// lead for a review. Everything is allocated when it is made; the reader's thread allocates nothing.
class ObjectScan {
 public:
  static constexpr std::size_t kMaxNodes = 64;
  static constexpr std::size_t kMaxObjects = 12;    // level-0 nodes kept
  static constexpr std::size_t kMaxPerParent = 4;   // holders kept for one node; all are counted
  static constexpr std::uint32_t kLevels = 3;
  static constexpr std::uint32_t kChunk = kMaxCopyBytes;
  static constexpr std::uint32_t kBack = kMaxCopyBytes;
  static constexpr std::uint64_t kAskEvery = 1u << 20;  // a region is asked for again after this much of it
  struct Target {
    std::uint32_t vtable = 0;  // a table of methods, as an offset in the image
    std::uint32_t lead = 0;    // the caller's number for what it belongs to
  };
  struct Node {
    std::uint8_t level = 0;
    bool embedded = false;      // level 1: the parent lies inside this object, and nothing was found pointing at it
    std::uint16_t parent = 0;   // level 1 and up: the node whose address it holds
    std::uint32_t lead = 0;
    std::uint32_t vtable = 0;   // the node's own class; 0: the holder lies in no object that could be told
    std::uint32_t offset = 0;   // level 1 and up: where in the node the address (or the enclosed object) lies
    std::uint32_t holders = 0;  // words found that hold this node's address (counted in the sweep after its own)
    std::uint64_t address = 0;
  };

  // `targets`: what to look for; `primary`: every class's primary table of methods, as offsets, ascending;
  // `base`, `size`: the image as loaded; `own`: the allocation bases of the caller's own memory, never read.
  ObjectScan(RegionQuery regions, std::vector<Target> targets, std::vector<std::uint32_t> primary, std::uint64_t base,
             std::uint64_t size, std::vector<std::uint64_t> own);

  // Reads up to `chunks` pieces; answers whether the scan is over.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t chunks);

  [[nodiscard]] bool done() const noexcept { return done_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint64_t regions() const noexcept { return regions_read_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t bytes() const noexcept { return bytes_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t unreadable() const noexcept { return unreadable_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint32_t sweeps() const noexcept { return sweeps_.load(std::memory_order_acquire); }
  // The nodes so far, in the order found (a node's parent comes before it); read from any thread.
  [[nodiscard]] std::span<const Node> nodes() const noexcept {
    return std::span<const Node>(nodes_.data(), node_count_.load(std::memory_order_acquire));
  }

 private:
  void look(std::span<const std::byte> chunk, std::uint64_t at, const proto::GameThread& gt, GuardedReader& reader);
  // The object a place lies in: the nearest word before `at` that is a primary table of methods, no further back
  // than kBack and never before `floor`. Nothing when there is none, or the memory cannot be read.
  struct Owner {
    std::uint64_t address = 0;
    std::uint32_t vtable = 0;
  };
  [[nodiscard]] std::optional<Owner> owner_of(std::uint64_t at, std::uint64_t floor, const proto::GameThread& gt,
                                              GuardedReader& reader);
  bool add(const Node& node) noexcept;
  void end_sweep(const proto::GameThread& gt, GuardedReader& reader);
  [[nodiscard]] bool ours(std::uint64_t allocation_base) const noexcept;

  RegionQuery regions_;
  std::vector<Target> targets_;         // by table
  std::vector<std::uint32_t> primary_;  // ascending
  std::uint64_t base_;
  std::uint64_t size_;
  std::vector<std::uint64_t> own_;
  std::vector<std::byte> chunk_;
  std::vector<std::byte> back_;
  std::uint32_t sweep_ = 0;
  std::uint64_t cursor_ = kLowestAddress;
  std::uint64_t region_base_ = 0;
  std::uint64_t region_end_ = 0;        // 0: the next region is yet to be asked for
  bool in_region_ = false;              // the last region asked for was one the scan reads
  struct Want {
    std::uint64_t address = 0;
    std::uint16_t node = 0;
    std::uint16_t kept = 0;
  };
  std::array<Want, kMaxNodes> want_{};  // the nodes this sweep looks for the holders of
  std::size_t want_count_ = 0;
  std::uint64_t want_lo_ = 0, want_hi_ = 0;
  std::array<Node, kMaxNodes> nodes_{};
  std::atomic<std::size_t> node_count_{0};
  std::atomic<bool> done_{false};
  std::atomic<std::uint64_t> regions_read_{0};
  std::atomic<std::uint64_t> bytes_{0};
  std::atomic<std::uint64_t> unreadable_{0};
  std::atomic<std::uint32_t> sweeps_{0};
};

#endif

}  // namespace en::gal::probe
