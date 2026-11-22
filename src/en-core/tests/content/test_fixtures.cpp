// The fixture flag states (MEM §7.8, §12.11: eight plus `ashen`) and the who-knows-what truth tables
// (tests/knowledge/expect.toml, MEM §12.11 T2): their shape, and their agreement with each other. The tables'
// comparison with known(P) is gated until S11.3.c builds it (WHY§364 (11)).
#include <doctest.h>

#include <array>
#include <regex>
#include <set>
#include <string>

#include <toml.hpp>

#include "en/proto/toml_check.hpp"
#include "../test_files.hpp"

using namespace en::core;
using namespace en::core::test;

namespace {

constexpr std::array<const char*, 8> kStates = {"fresh",        "after_godrick", "two_runes",  "after_morgott",
                                                "after_mohg",   "erdtree_burned", "after_radahn", "after_rykard"};
constexpr std::array<const char*, 2> kPersonas = {"kale", "lantern_keeper"};

toml::table parse(std::string_view path) {
  const std::string_view text = test_file(path);
  REQUIRE_MESSAGE(!text.empty(), std::string(path));
  REQUIRE_FALSE(en::proto::unsafe_for_toml(text, en::proto::TomlText::Player).has_value());
  toml::parse_result r = toml::parse(text);
  REQUIRE(r);
  return std::move(r).table();
}

// Whether a world stage holds at a fixture state (MEM §12.4's `when`, over the state's aliases).
bool holds(const std::string& stage, const toml::table& flags) {
  const auto on = [&](const char* alias) { return flags[alias].value_or(false); };
  if (stage == "castle_fallen") return on("boss.godrick");
  if (stage == "queen_fallen") return on("boss.rennala");
  if (stage == "general_fallen") return on("boss.radahn");
  if (stage == "star_fallen") return on("story.star_fell") && on("boss.radahn");
  if (stage == "manor_lord_fallen") return on("boss.rykard");
  if (stage == "capital_king_fallen") return on("boss.morgott");
  if (stage == "blood_lord_fallen") return on("boss.mohg_lord");
  if (stage == "erdtree_burned") return on("story.erdtree_burned");
  if (stage == "capital_ashen") return on("boss.maliketh") && on("story.erdtree_burned");
  FAIL("an unknown world stage: " << stage);
  return false;
}

}  // namespace

TEST_CASE("the fixture flag states: eight and ashen, aliases only, every one verified") {
  const std::regex alias("^[a-z][a-z0-9_]*(\\.[a-z0-9_]+){1,3}$");
  std::vector<std::string> names(kStates.begin(), kStates.end());
  names.emplace_back("ashen");
  for (const std::string& name : names) {
    CAPTURE(name);
    const toml::table t = parse("tests/fixtures/flagstates/" + name + ".toml");
    CHECK(t["schema"].value_or(0) == 1);
    CHECK(t["state"].value_or(std::string()) == name);
    const toml::table* flags = t["flags"].as_table();
    REQUIRE(flags != nullptr);
    for (auto&& [k, v] : *flags) {
      CHECK(std::regex_match(std::string(k.str()), alias));
      CHECK(v.value_or(false));  // listed means true; an alias not listed reads false
    }
  }
  CHECK(parse("tests/fixtures/flagstates/fresh.toml")["flags"].as_table()->empty());
  const toml::table ashen = parse("tests/fixtures/flagstates/ashen.toml");
  for (const char* w : {"castle_fallen", "queen_fallen", "general_fallen", "star_fallen", "manor_lord_fallen",
                        "capital_king_fallen", "blood_lord_fallen", "erdtree_burned", "capital_ashen"}) {
    CHECK_MESSAGE(holds(w, *ashen["flags"].as_table()), w);  // all nine world stages
  }
}

TEST_CASE("the truth tables: every persona at every state, every item, and none where the item does not hold") {
  const toml::table expect = parse("tests/knowledge/expect.toml");
  const std::regex value("^(none|(fact|rumour|belief):(own|kept|tie|sky|near|word))$");
  for (const char* state : kStates) {
    const toml::table fixture = parse(std::string("tests/fixtures/flagstates/") + state + ".toml");
    const toml::table& flags = *fixture["flags"].as_table();
    for (const char* persona : kPersonas) {
      CAPTURE(state);
      CAPTURE(persona);
      const toml::table* table = expect[state][persona].as_table();
      REQUIRE(table != nullptr);
      CHECK(table->size() == 11);  // the nine world stages and Tanith's two steps
      for (auto&& [k, v] : *table) {
        const std::string item(k.str());
        const std::string got = v.value_or(std::string());
        CHECK(std::regex_match(got, value));
        if (item.rfind("w:", 0) == 0 && !holds(item.substr(2), flags)) CHECK(got == "none");
      }
    }
  }
}

// MEM §12.11 T2: known(P) equals the tables exactly. known(P) comes with S11.3.c; until then the tables are written and
// their shape is checked above, and this case is skipped.
TEST_CASE("T2: known(P) equals tests/knowledge/expect.toml for every persona and fixture state" * doctest::skip()) {
  FAIL("known(P) is not built yet (S11.3.c)");
}
