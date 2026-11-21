#pragma once
// gal/recipes.toml: Lodestar's recipes (proposal 0005 §6; O102). A recipe says how our own tools find an id in a build
// nobody has seen, as data, with no byte of the game: gal/ holds numbers, our labels and shapes in our own grammar
// (shape.hpp), and never a pattern cut from the game's code. Each recipe has its note, gal/derivations/<id>.md, in our
// own words (O27), and its provenance is always our own derivation (O4, O48).
//
// One kind so far, `accessor`: the short sequence the compiler writes wherever the game fetches a manager, as a shape,
// with which of its steps gives the static (a load of a slot), the class object (an address into the first argument's
// register) and the getter (a relative call), and the suffix the path every site loads ends with. The FD4 finder runs
// on it (fd4.hpp). The file replaces patterns.toml's idiom; patterns.toml goes when nothing reads it.
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/fd4.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/schema.hpp"
#include "en/gal/shape.hpp"
#include "en/proto/expected.hpp"

namespace en::gal {

enum class RecipeKind : std::uint8_t { Accessor };

struct RecipeSpec {
  std::string id;  // a label of ours
  RecipeKind kind = RecipeKind::Accessor;
  std::string section = ".text";  // where it is matched
  shape::Shape shape;
  // An accessor's: the steps that give its three places, and the path's suffix.
  std::uint8_t static_step = 0;
  std::uint8_t class_step = 0;
  std::uint8_t getter_step = 0;
  std::string path_suffix;
  std::uint32_t expect_min = 1;  // expect_matches: the fewest and the most places the shape may stand at
  std::uint32_t expect_max = 1u << 16;
  std::vector<std::string> builds;  // where it was confirmed; empty: a lead, confirmed nowhere yet
  Provenance provenance;

  [[nodiscard]] fd4::Accessor accessor() const { return fd4::Accessor{shape, static_step, class_step, getter_step}; }
};

struct RecipeSet {
  std::vector<RecipeSpec> recipes;

  [[nodiscard]] const RecipeSpec* find(std::string_view id) const noexcept;
};

inline constexpr std::uint32_t kMaxRecipeMatches = 1u << 16;

// Reads recipes.toml (schema 1) and reports every problem: an id that is no label of ours or is used twice; a kind
// other than accessor; steps that are not 1 to 16 forms of the grammar, or test a register before any load; a place
// given by a step that is not there or is of a form that does not give it; a path suffix that is not 1 to 64 printable
// ASCII characters; counts out of range; a build key out of form; a provenance that is not our own derivation; and
// each of `required_ids` absent. No key of a recipe can hold bytes: `bytes`, `pattern` and the like are unknown keys.
[[nodiscard]] proto::Expected<RecipeSet, SchemaErrors> parse_recipes(std::string_view text,
                                                                     std::span<const std::string_view> required_ids,
                                                                     std::string_view file = "recipes.toml");

}  // namespace en::gal
