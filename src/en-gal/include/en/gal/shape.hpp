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
  SkipOne,         // any one instruction that falls through and does not load the remembered register anew: the
                   // compiler now and then puts one between a load and its test
  CompareSlot,     // a 64-bit compare of a RIP-relative slot with zero, in memory: where the slot's value is not
                   // needed in a register, the compiler tests it there; gives the slot. A branch_set step may follow
};
inline constexpr std::size_t kFormCount = 8;

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

// Why a shape does not stand at a place: the first step whose instruction is not its form, and how it differs. The
// finder uses it to say, in counts, why a site another method accepts is none of the shape's.
enum class Differs : std::uint8_t {
  Narrow,         // the form in all but its width: a load or a test that is not 64-bit
  OtherRegister,  // a 64-bit test, but not of the register just loaded
  Other,          // another instruction altogether
  Undecodable,    // nothing decodes there, or the section ends
};
[[nodiscard]] const char* to_string(Differs differs) noexcept;  // "narrow", "other_register", "other", "undecodable"

struct Mismatch {
  std::uint8_t step = 0;
  Differs differs = Differs::Other;
  friend constexpr bool operator==(const Mismatch&, const Mismatch&) = default;
};

// The first mismatch of the shape at `rva`; nothing when the shape stands there, or is not valid.
[[nodiscard]] std::optional<Mismatch> mismatch_at(const ImageBytes& img, const SectionCopy& text, const Shape& shape,
                                                  std::uint32_t rva);

// Every place of the section where the shape stands.
[[nodiscard]] Matches find(const ImageBytes& img, const SectionCopy& text, const Shape& shape, std::size_t max);

}  // namespace en::gal::shape
