// gal/recipes.toml (recipes.hpp; proposal 0005 §6; O102): a recipe is a shape in our own grammar with which of its
// steps gives each place; every problem in the file is reported; and no key of a recipe can hold bytes. Every id,
// suffix and build here is invented.
#include <doctest.h>

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/recipes.hpp"

using namespace en::gal;
using shape::Form;

namespace {

constexpr std::string_view kAccessor = R"([[recipe]]
id = "accessor"
kind = "accessor"
section = ".text"
steps = ["load_slot", "test_loaded", "branch_set_short", "address_arg1", "call_relative"]
gives = { static = 0, class = 3, getter = 4 }
path_suffix = "Lone.h"
expect_matches = [1, 4096]
builds = ["9.8.7.0-ww"]
provenance = { method = "own-derivation", leads = ["our own notes"], evidence = "docs/ops/probes/invented.md", date = "2026-01-02" }
)";

std::string file(std::string_view body) { return "schema = 1\n" + std::string(body); }

std::string with(std::string_view text, std::string_view from, std::string_view to) {
  std::string out(text);
  const std::size_t at = out.find(from);
  REQUIRE(at != std::string::npos);
  out.replace(at, from.size(), to);
  return out;
}

bool refused(const std::string& body, std::string_view where, std::string_view problem) {
  const auto set = parse_recipes(file(body), {});
  if (set.has_value()) return false;
  for (const SchemaError& e : set.error()) {
    if (e.where == where && e.problem.find(problem) != std::string::npos) return true;
  }
  for (const SchemaError& e : set.error()) MESSAGE(describe(e));
  return false;
}

}  // namespace

TEST_CASE("recipes.toml: an accessor with its shape, the steps that give its places, and its suffix") {
  const auto set = parse_recipes(file(kAccessor), {});
  if (!set.has_value()) {
    for (const SchemaError& e : set.error()) MESSAGE(describe(e));
  }
  REQUIRE(set.has_value());
  REQUIRE(set->recipes.size() == 1);
  const RecipeSpec* r = set->find("accessor");
  REQUIRE(r != nullptr);
  CHECK(r->kind == RecipeKind::Accessor);
  CHECK(r->section == ".text");
  CHECK(r->shape.steps == std::vector<Form>{Form::LoadSlot, Form::TestLoaded, Form::BranchSetShort, Form::AddressArg1,
                                            Form::CallRelative});
  CHECK(r->static_step == 0);
  CHECK(r->class_step == 3);
  CHECK(r->getter_step == 4);
  CHECK(r->path_suffix == "Lone.h");
  CHECK(r->expect_min == 1);
  CHECK(r->expect_max == 4096);
  CHECK(r->builds == std::vector<std::string>{"9.8.7.0-ww"});
  CHECK(r->provenance.method == "own-derivation");
  CHECK(r->provenance.evidence == "docs/ops/probes/invented.md");
  CHECK(r->accessor().valid());
  CHECK(set->find("nothing") == nullptr);
  // The defaults: the code section, any count up to the most, confirmed nowhere.
  const auto bare = parse_recipes(
      file(with(with(with(kAccessor, "section = \".text\"\n", ""), "expect_matches = [1, 4096]\n", ""),
                "builds = [\"9.8.7.0-ww\"]\n", "")),
      {});
  REQUIRE(bare.has_value());
  CHECK(bare->recipes[0].section == ".text");
  CHECK(bare->recipes[0].expect_max == kMaxRecipeMatches);
  CHECK(bare->recipes[0].builds.empty());
  // A file with no recipe reads, unless one is required.
  CHECK(parse_recipes("schema = 1\n", {}).has_value());
  const std::string_view required[] = {"accessor"};
  const auto lacking = parse_recipes("schema = 1\n", required);
  REQUIRE_FALSE(lacking.has_value());
  CHECK(lacking.error()[0].problem.find("lacks the required recipe") != std::string::npos);
  CHECK(parse_recipes(file(kAccessor), required).has_value());
}

TEST_CASE("recipes.toml: the shape keeps to the grammar, and a place is given by a step of the form that gives it") {
  CHECK(refused(with(kAccessor, "\"test_loaded\"", "\"test\""), "recipes.toml: recipe[0].steps[1]", "must be one of"));
  CHECK(refused(with(kAccessor, "\"test_loaded\"", "7"), "recipes.toml: recipe[0].steps[1]", "must be one of"));
  CHECK(refused(with(kAccessor, "[\"load_slot\", \"test_loaded\"", "[\"test_loaded\", \"load_slot\""),
                "recipes.toml: recipe[0].steps", "tests a register before any load"));
  CHECK(refused(with(kAccessor, "steps = [\"load_slot\", \"test_loaded\", \"branch_set_short\", \"address_arg1\", "
                                "\"call_relative\"]",
                     "steps = []"),
                "recipes.toml: recipe[0].steps", "must list 1 to 16 forms"));
  std::string many = "steps = [";
  for (int i = 0; i < 17; ++i) many += "\"call_relative\", ";
  many += "]";
  CHECK(refused(with(kAccessor, "steps = [\"load_slot\", \"test_loaded\", \"branch_set_short\", \"address_arg1\", "
                                "\"call_relative\"]",
                     many),
                "recipes.toml: recipe[0].steps", "must list 1 to 16 forms"));
  CHECK(refused(with(kAccessor, "static = 0", "static = 1"), "recipes.toml: recipe[0].gives.static",
                "must be a load_slot or a compare_slot step"));
  CHECK(refused(with(kAccessor, "class = 3", "class = 4"), "recipes.toml: recipe[0].gives.class",
                "must be an address_arg1 step"));
  CHECK(refused(with(kAccessor, "getter = 4", "getter = 5"), "recipes.toml: recipe[0].gives.getter",
                "must be a call_relative step"));
  CHECK(refused(with(kAccessor, "getter = 4", "getter = 16"), "recipes.toml: recipe[0].gives.getter", ""));
  CHECK(refused(with(kAccessor, ", getter = 4", ""), "recipes.toml: recipe[0].gives.getter", "is missing"));
  CHECK(refused(with(kAccessor, "getter = 4", "getter = 4, path = 6"), "recipes.toml: recipe[0].gives",
                "has an unknown key \"path\""));
  // The near branch is a form of the grammar too.
  CHECK(parse_recipes(file(with(kAccessor, "branch_set_short", "branch_set_near")), {}).has_value());
}

TEST_CASE("recipes.toml: ids, kinds, the suffix, counts, builds and provenance keep to their forms") {
  CHECK(refused(with(kAccessor, "id = \"accessor\"", "id = \"Accessor\""), "recipes.toml: recipe[0].id",
                "must be lower-case"));
  CHECK(refused(std::string(kAccessor) + std::string(kAccessor), "recipes.toml: recipe[1].id", "is not unique"));
  CHECK(refused(with(kAccessor, "kind = \"accessor\"", "kind = \"idiom\""), "recipes.toml: recipe[0].kind",
                "must be accessor"));
  CHECK(refused(with(kAccessor, "section = \".text\"", "section = \"a very long name\""),
                "recipes.toml: recipe[0].section", "section's name"));
  CHECK(refused(with(kAccessor, "path_suffix = \"Lone.h\"\n", ""), "recipes.toml: recipe[0].path_suffix",
                "is missing"));
  CHECK(refused(with(kAccessor, "\"Lone.h\"", "\"Lone h\""), "recipes.toml: recipe[0].path_suffix", "printable"));
  CHECK(refused(with(kAccessor, "[1, 4096]", "[0, 4096]"), "recipes.toml: recipe[0].expect_matches[0]", ""));
  CHECK(refused(with(kAccessor, "[1, 4096]", "[1, 65537]"), "recipes.toml: recipe[0].expect_matches[1]", ""));
  CHECK(refused(with(kAccessor, "[1, 4096]", "[9, 3]"), "recipes.toml: recipe[0].expect_matches",
                "lowest above its highest"));
  CHECK(refused(with(kAccessor, "\"9.8.7.0-ww\"", "\"version nine\""), "recipes.toml: recipe[0].builds[0]",
                "must be a build key"));
  CHECK(refused(with(kAccessor, "own-derivation", "copied"), "recipes.toml: recipe[0].provenance.method",
                "must be own-derivation"));
  CHECK(refused(with(kAccessor, "date = \"2026-01-02\"", "date = \"yesterday\""),
                "recipes.toml: recipe[0].provenance.date", "YYYY-MM-DD"));
  CHECK(refused(with(kAccessor, "evidence = \"docs/ops/probes/invented.md\"", "evidence = \"/somewhere/else\""),
                "recipes.toml: recipe[0].provenance.evidence", "relative path"));
  CHECK(refused(with(kAccessor, "provenance = {", "note = 1\nprovenance = {"), "recipes.toml: recipe[0]",
                "has an unknown key \"note\""));
  CHECK_FALSE(parse_recipes("schema = 2\n" + std::string(kAccessor), {}).has_value());
  CHECK_FALSE(parse_recipes("recipe = [1]\nschema = 1\n", {}).has_value());
  CHECK_FALSE(parse_recipes("not toml\n", {}).has_value());
}

TEST_CASE("no key of a recipe can carry a byte of the game") {
  for (const std::string_view key : {"bytes", "pattern", "prologue", "code", "captures"}) {
    CAPTURE(key);
    CHECK(refused(with(kAccessor, "path_suffix", std::string(key) + " = \"48 8B ?? ??\"\npath_suffix"),
                  "recipes.toml: recipe[0]", "has an unknown key \"" + std::string(key) + "\""));
  }
}

TEST_CASE("recipes.toml: a second recipe of the accessor's kind, whose static is given by a compare of the slot") {
  const std::string second = std::string(kAccessor) + R"(
[[recipe]]
id = "accessor_compare"
kind = "accessor"
steps = ["compare_slot", "branch_set_short", "address_arg1", "call_relative"]
gives = { static = 0, class = 2, getter = 3 }
path_suffix = "Lone.h"
provenance = { method = "own-derivation", evidence = "docs/ops/probes/invented.md", date = "2026-01-02" }
)";
  const auto set = parse_recipes(file(second), {});
  if (!set) {
    for (const SchemaError& e : set.error()) MESSAGE(e.where << ": " << e.problem);
  }
  REQUIRE(set.has_value());
  REQUIRE(set->recipes.size() == 2);
  const RecipeSpec* r = set->find("accessor_compare");
  REQUIRE(r != nullptr);
  CHECK(r->shape.steps.size() == 4);
  CHECK(r->static_step == 0);
  CHECK(r->class_step == 2);
  CHECK(r->getter_step == 3);
  CHECK(r->accessor().valid());
  CHECK(r->expect_min == 1);  // not counted yet: any number of places is within its range
  CHECK(r->expect_max == kMaxRecipeMatches);
  // The static's step must be a load or a compare of a slot: a skipped instruction gives nothing.
  CHECK(refused(with(second, "steps = [\"compare_slot\", \"branch_set_short\"", "steps = [\"skip_one\", \"branch_set_short\""),
                "recipes.toml: recipe[1].gives.static", "must be a load_slot or a compare_slot step"));
  // A test of a loaded register has nothing to test after a compare in memory.
  CHECK(refused(with(second, "\"compare_slot\", \"branch_set_short\"", "\"compare_slot\", \"test_loaded\""),
                "recipes.toml: recipe[1].steps", "tests a register before any load"));
}
