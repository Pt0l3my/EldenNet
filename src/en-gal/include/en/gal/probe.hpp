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
// Every read is a guarded copy on the game thread within the tick's budget, never into a known stack (WHY§111); the
// candidates come from the image's own data section, read on T1.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "en/gal/discovery.hpp"
#include "en/gal/guarded.hpp"
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

#endif

}  // namespace en::gal::probe
