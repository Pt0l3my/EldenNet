#pragma once
// Shapes: Lodestar's recipes without bytes (proposal 0005 §6.1; O102). A shape describes a habit of the compiler as a
// short run of instruction forms, in a grammar of our own over our own decoder's fields (x86.hpp): what kind of
// instruction stands at each step and what it refers to, never which bytes spell it. Nothing of any program is in a
// shape, and the same shape matches every build the same compiler made, whatever registers and distances it chose.
//
// A form says what one instruction must be, by the architecture's own encoding, and what it gives: a 64-bit load of a
// RIP-relative slot gives the slot and remembers the register it loads; a test of that register with itself; a branch
// taken when the test found it set; the address of a RIP-relative object into the first argument's register gives the
// object; a relative call gives its target. The steps' instructions must follow one another, each starting where the
// one before ends.
//
// find() looks at every offset of a section, as a pattern scan does, so a site is found wherever it lies, also where
// a straight decode of the section would be out of step; a cheap test on the first step's encoding comes before each
// decode. It reads an image read once (image_bytes.hpp) and calls nothing. Nothing here knows the game.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "en/gal/image_bytes.hpp"

namespace en::gal::shape {

enum class Form : std::uint8_t {
  LoadSlot,        // a 64-bit load of a RIP-relative slot into a register, which the shape remembers; gives the slot
  TestLoaded,      // a 64-bit test of the remembered register with itself
  BranchSetShort,  // a branch forward, taken when the test found the register set, with an 8-bit distance
  BranchSetNear,   // the same with a 32-bit distance
  AddressArg1,     // the address of a RIP-relative object into the first argument's register; gives the object
  CallRelative,    // a relative call; gives its target
};
inline constexpr std::size_t kFormCount = 6;

[[nodiscard]] const char* to_string(Form form) noexcept;  // "load_slot", "test_loaded", "branch_set_short", ...
[[nodiscard]] std::optional<Form> form_of(std::string_view word) noexcept;
// Whether a form gives a place (a slot, an object or a call's target).
[[nodiscard]] bool gives_place(Form form) noexcept;

inline constexpr std::size_t kMaxSteps = 16;

struct Shape {
  std::vector<Form> steps;  // 1 to kMaxSteps

  // A shape find() and match_at() take: 1 to kMaxSteps steps, and no test or branch before a load to speak of.
  [[nodiscard]] bool valid() const noexcept;
  friend bool operator==(const Shape&, const Shape&) = default;
};

struct Hit {
  std::uint32_t rva = 0;                         // where the first step's instruction starts
  std::uint32_t end = 0;                         // where the last step's instruction ends
  std::array<std::uint32_t, kMaxSteps> gives{};  // by step: the place its form gives, as an RVA; 0 for a form that gives none
  // Where the shape's branch goes, when it has one: the end of the block it skips.
  std::optional<std::uint32_t> branch_to;
};

struct Matches {
  std::vector<Hit> hits;    // the first `max`
  std::uint64_t count = 0;  // all of them
};

// The shape at `rva`, when its instructions stand there; nothing otherwise, or for a shape that is not valid.
[[nodiscard]] std::optional<Hit> match_at(const ImageBytes& img, const SectionCopy& text, const Shape& shape,
                                          std::uint32_t rva);

// Every place of the section where the shape stands.
[[nodiscard]] Matches find(const ImageBytes& img, const SectionCopy& text, const Shape& shape, std::size_t max);

}  // namespace en::gal::shape
