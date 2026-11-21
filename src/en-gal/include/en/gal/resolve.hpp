#pragma once
// Lodestar's resolver (proposal 0005 §7.1, §9.1; O100): the checks every start makes on the build's table, and the one
// way from an id to its place. A start searches nothing: it takes each item of the table the gate's build names
// (layout.hpp, schema 2) and re-finds it in the image as loaded.
//
// - placement: the value lies where its kind must. A static in a section that is read and written and not run, with
//   room for a pointer; a function at a start the image's own function table lists, of the table's length; a table
//   whole within one section that is read and not run; a class's tables of methods in read-only data, aligned.
// - witness: each place the table records as using the id still does. The instruction there is decoded with our own
//   decoder (x86.hpp) and must refer to the id: a static or a table by a RIP-relative operand, a function by a relative
//   call or jump or by a RIP-relative operand. One witness that refers elsewhere refuses the id.
//
// An item that fails is refused alone: nothing is looked for nearby, nothing is retried with a looser rule, and its
// place is never handed out. The other checks of §7.1 (digest, class, self-test, verdict) are later steps'.
//
// It reads through a ByteSource (offline the executable file, in game the guarded view of the mapped image), a few
// bytes at a time, and copies what it reads: nothing points into the image, and nothing here knows the game.
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "en/gal/ids.hpp"
#include "en/gal/layout.hpp"
#include "en/gal/pe.hpp"
#include "en/proto/expected.hpp"

namespace en::gal {

// Why an id is refused. The words are fixed, for logs and reports.
enum class ItemProblem : std::uint8_t {
  None,
  Unknown,             // the build's table holds no entry for the id
  Kind,                // its entry is of another kind than the one asked for
  Section,             // placement: in no section of the image, or in one that cannot hold its kind
  Alignment,           // placement: a table of methods that is not aligned to 8
  NotFunctionStart,    // placement: no function of the image's function table starts there
  Length,              // placement: the function that starts there is not of the table's length
  Extent,              // placement: it runs past the end of its section
  WitnessOutsideCode,  // a witness lies in no section that is run
  WitnessUnreadable,   // a witness's bytes could not be read
  WitnessUndecodable,  // nothing decodes at a witness
  WitnessElsewhere,    // the instruction at a witness does not refer to the id
};
[[nodiscard]] const char* to_string(ItemProblem problem) noexcept;  // "none", "section", "witness_elsewhere", ...

// The check a problem belongs to (proposal 0005 §7.1); placement for the ones that are no check's.
[[nodiscard]] ItemCheck check_of(ItemProblem problem) noexcept;

struct ItemState {
  std::uint32_t id = 0;
  ItemProblem problem = ItemProblem::None;
  std::uint8_t witness = 0;    // for a witness's problem, which one (its place in the entry's list)
  std::uint8_t witnesses = 0;  // how many witnesses referred to the id

  [[nodiscard]] bool ok() const noexcept { return problem == ItemProblem::None; }
  friend constexpr bool operator==(const ItemState&, const ItemState&) = default;
};

// The checks a start makes on one item, against the image the gate matched.
[[nodiscard]] ItemState check_item(const ImageInfo& image, const ByteSource& source, const ItemEntry& item) noexcept;

// A build's table, checked once against the image. Made on the init thread, before anything reads the game; what it
// hands out are offsets from the start of the image, and only of items that passed.
class Library {
 public:
  Library(const Layout& layout, const ImageInfo& image, const ByteSource& source);

  [[nodiscard]] std::span<const ItemState> states() const noexcept { return states_; }
  [[nodiscard]] const ItemState* state(std::uint32_t id) const noexcept;
  [[nodiscard]] std::size_t refused() const noexcept;

  // The place of a static, a function or a table that passed its checks, or why there is none.
  [[nodiscard]] proto::Expected<std::uint32_t, ItemProblem> place(std::uint32_t id, IdKind kind) const noexcept;
  // The tables of methods of a class that passed its checks; `problem`, when given, says why there are none.
  [[nodiscard]] std::span<const std::uint32_t> vtables(std::uint32_t id, ItemProblem* problem = nullptr) const noexcept;

 private:
  struct Slot {
    IdKind kind = IdKind::Static;
    std::uint32_t rva = 0;
    std::vector<std::uint32_t> vtables;
  };
  [[nodiscard]] std::size_t index_of(std::uint32_t id) const noexcept;  // states_.size() when absent

  std::vector<ItemState> states_;  // in the table's order
  std::vector<Slot> slots_;        // beside states_
};

}  // namespace en::gal
