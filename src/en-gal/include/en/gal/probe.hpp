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

// The static-slot search for one class's instance.
class SlotSearch {
 public:
  // Collects the candidates on T1: every 8-aligned slot of the image's writable data sections whose value lies in user
  // space outside the image (an instance on the heap), at most `max_candidates`.
  SlotSearch(const SysLayer& sys, const Discovery& discovery, roles::Cls cls, std::size_t max_candidates = 1u << 16);
  // On the game thread: checks up to `copies` candidates (a guarded copy of each one's first 8 bytes, compared with the
  // class's vtables). Answers whether every candidate has been checked.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies);
  [[nodiscard]] std::size_t candidates() const noexcept { return slots_.size(); }
  [[nodiscard]] std::size_t checked() const noexcept { return next_; }
  [[nodiscard]] std::uint64_t unreadable() const noexcept { return unreadable_; }
  // The slots (RVAs) whose instance has one of the class's vtables.
  [[nodiscard]] std::span<const std::uint32_t> found() const noexcept { return found_; }

 private:
  const VtableSet& set_;
  std::uint64_t base_ = 0;
  std::vector<std::uint32_t> slots_;
  std::vector<std::uint64_t> values_;
  std::vector<std::uint32_t> found_;
  std::size_t next_ = 0;
  std::uint64_t unreadable_ = 0;
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
