#pragma once
// en-gal's reader: GuardedReader's one guarded copy (ARCH §6.5, DESIGN R4). It copies bytes from an address into
// storage the caller owns, under one structured exception handler (SEH: __try/__except), and answers whether every
// byte arrived. It never throws and keeps no state, and no pointer made from the address outlives the copy: a fault is
// an answer, not an event. The plausibility checks, groups, budgets and breakers around it are GuardedReader's (§6.5,
// §6.7), and nothing here knows the game.
//
// SEH is an MSVC-like compiler's: en-gal builds with clang-cl alone (cmake/clang-cl-xwin.cmake; ARCH §2.6).
#include <cstddef>
#include <cstdint>
#include <span>

namespace en::gal {

// What one guarded copy did.
struct CopyOutcome {
  bool copied = false;     // every byte arrived
  std::uint32_t code = 0;  // if not, the Windows exception that stopped the copy; 0 if it copied
};

// Copies dst.size() bytes from `address` into `dst`. A copy that meets an access violation, an in-page error or a
// guard page stops there and answers {false, code}, with `dst` zeroed: nothing of a partial copy survives. Windows
// lifts a guard page's guard as it raises that fault, so a guard page read once is a guard page no more, and its next
// read goes through. Any other exception is not the copy's, and goes on to the next handler. An empty `dst` copies
// nothing and answers {true, 0}.
[[nodiscard]] CopyOutcome seh_copy(std::uint64_t address, std::span<std::byte> dst) noexcept;

}  // namespace en::gal
