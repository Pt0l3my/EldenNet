#pragma once
// gal/ids.toml: the catalogue of Lodestar, EldenNet's own address library (proposal 0005 §4; O100). An id is a number
// and a label, both ours and both permanent: `1005 task_hub` means the same place of the game in every build of the
// game and every version of EldenNet. The catalogue is independent of any build; where an id lies in one build is
// that build's table (gal/layouts/<build>.toml's [item.*], layout.hpp).
//
// The rules the file keeps, in CI and at boot alike (O4):
// - a number is given once, in its kind's block, and the file lists the ids in ascending order, so a new id never
//   takes an old number's place; an id EldenNet stops using stays, marked retired;
// - a label is lower-case words of our own (`[a-z][a-z0-9_]*`), never the game's name for the thing;
// - every entry says in our own words what we mean by it, which read group or service needs it, and whether EldenNet
//   runs without it.
// Nothing here knows the game: no name of its, no offset, no byte.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/schema.hpp"
#include "en/proto/expected.hpp"

namespace en::gal {

// What an id names (proposal 0005 §4.2).
enum class IdKind : std::uint8_t {
  Static,    // a slot in the game's data that holds a pointer or a value
  Function,  // the start of one of the game's functions
  Class,     // a class of the game's objects, known by its run-time type information
  Table,     // a table in the game's data
};
inline constexpr std::size_t kIdKindCount = 4;

[[nodiscard]] const char* to_string(IdKind kind) noexcept;  // "static", "function", "class", "table"

// The numbers a kind's ids take: 1000 onwards for statics, 2000 for functions, 3000 for classes, 4000 for tables.
struct IdBlock {
  std::uint32_t first = 0;
  std::uint32_t last = 0;
};
[[nodiscard]] constexpr IdBlock block_of(IdKind kind) noexcept {
  switch (kind) {
    case IdKind::Static: return {1000, 1999};
    case IdKind::Function: return {2000, 2999};
    case IdKind::Class: return {3000, 3999};
    case IdKind::Table: return {4000, 4999};
  }
  return {0, 0};
}
// The kind whose block holds `number`, or nothing for a number in no block.
[[nodiscard]] constexpr std::optional<IdKind> kind_of_number(std::uint32_t number) noexcept {
  if (number < 1000 || number > 4999) return std::nullopt;
  return static_cast<IdKind>(number / 1000 - 1);
}

enum class IdStatus : std::uint8_t { Active, Retired };

inline constexpr std::size_t kMaxIdLabelBytes = 48;
inline constexpr std::size_t kMaxIdMeansBytes = 400;
inline constexpr std::size_t kMaxIds = 4000;

// A label of ours: `[a-z][a-z0-9_]*`, 1 to kMaxIdLabelBytes bytes.
[[nodiscard]] bool is_id_label(std::string_view s) noexcept;

struct IdEntry {
  std::uint32_t number = 0;
  std::string label;
  std::string was;    // the label older files and schema-1 reports use for it; empty for none
  IdKind kind = IdKind::Static;
  std::string means;  // what we mean by it, in our own words
  std::string needs;  // "boot", a read group's name, or "task_service"
  bool required = false;
  IdStatus status = IdStatus::Active;

  friend bool operator==(const IdEntry&, const IdEntry&) = default;
};

class Catalogue {
 public:
  [[nodiscard]] const std::vector<IdEntry>& entries() const noexcept { return entries_; }
  [[nodiscard]] const IdEntry* find(std::uint32_t number) const noexcept;
  // By its label; `or_was` also answers an entry whose earlier label this is.
  [[nodiscard]] const IdEntry* find(std::string_view label, bool or_was = false) const noexcept;

 private:
  friend proto::Expected<Catalogue, SchemaErrors> parse_ids(std::string_view text, std::string_view file);
  std::vector<IdEntry> entries_;  // in ascending order of number
};

// Reads ids.toml (schema 1) and reports every problem in it: a number outside its kind's block or not above the one
// before it, a label or an earlier label that is not ours in form or is used twice, a meaning that is empty, too long
// or not printable ASCII, a `needs` that names no read group or service, a retired id still required.
[[nodiscard]] proto::Expected<Catalogue, SchemaErrors> parse_ids(std::string_view text,
                                                                 std::string_view file = "ids.toml");

}  // namespace en::gal
