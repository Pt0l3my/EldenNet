// gal/ids.toml (ids.hpp; proposal 0005 §4): Lodestar's catalogue reads with every problem reported; numbers keep to
// their kind's block and ascend; labels are ours in form and never used twice; and the repository's own catalogue
// agrees with the roles the code reads. Every id in the invented catalogues is invented.
#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "en/gal/ids.hpp"
#include "en/gal/roles.hpp"
#include "gal_ids_text.hpp"

using namespace en::gal;

namespace {

std::string id(std::uint32_t n, std::string_view label, std::string_view kind, std::string_view extra = "") {
  return "[[id]]\nn = " + std::to_string(n) + "\nlabel = \"" + std::string(label) + "\"\nkind = \"" +
         std::string(kind) + "\"\nmeans = \"An invented thing.\"\nneeds = \"world\"\nrequired = false\n" +
         std::string(extra) + "\n";
}

SchemaErrors refusal(const std::string& body) {
  auto catalogue = parse_ids("schema = 1\n" + body);
  REQUIRE_FALSE(catalogue.has_value());
  return catalogue.error();
}

bool has_error(const SchemaErrors& errors, std::string_view where, std::string_view problem) {
  for (const SchemaError& e : errors) {
    if (e.where == where && e.problem.find(problem) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("ids.toml: every id with its number, label, kind, meaning and need") {
  const std::string text = "schema = 1\n" + id(1001, "lantern", "static") +
                           id(1002, "ledger", "static", "was = \"old_ledger\"\nstatus = \"retired\"") +
                           id(2001, "ring_bell", "function") + id(3001, "walker", "class") +
                           id(4001, "rota", "table");
  const auto catalogue = parse_ids(text);
  REQUIRE(catalogue.has_value());
  REQUIRE(catalogue->entries().size() == 5);
  const IdEntry* ledger = catalogue->find(1002);
  REQUIRE(ledger != nullptr);
  CHECK(ledger->label == "ledger");
  CHECK(ledger->was == "old_ledger");
  CHECK(ledger->kind == IdKind::Static);
  CHECK(ledger->status == IdStatus::Retired);
  CHECK(ledger->needs == "world");
  CHECK_FALSE(ledger->required);
  CHECK(catalogue->find(1001)->status == IdStatus::Active);
  CHECK(catalogue->find("ring_bell") == catalogue->find(2001));
  CHECK(catalogue->find("old_ledger") == nullptr);        // an earlier label is no label
  CHECK(catalogue->find("old_ledger", true) == ledger);   // unless asked for
  CHECK(catalogue->find(1003) == nullptr);
  CHECK(catalogue->find("nothing") == nullptr);
  CHECK(catalogue->find(4001)->kind == IdKind::Table);
}

TEST_CASE("ids.toml: the kinds' blocks") {
  CHECK(block_of(IdKind::Static).first == 1000);
  CHECK(block_of(IdKind::Function).first == 2000);
  CHECK(block_of(IdKind::Class).first == 3000);
  CHECK(block_of(IdKind::Table).last == 4999);
  CHECK(kind_of_number(1000) == IdKind::Static);
  CHECK(kind_of_number(1999) == IdKind::Static);
  CHECK(kind_of_number(2000) == IdKind::Function);
  CHECK(kind_of_number(3999) == IdKind::Class);
  CHECK(kind_of_number(4999) == IdKind::Table);
  CHECK_FALSE(kind_of_number(999).has_value());
  CHECK_FALSE(kind_of_number(5000).has_value());
  for (const IdKind kind : {IdKind::Static, IdKind::Function, IdKind::Class, IdKind::Table}) {
    CHECK(kind_of_number(block_of(kind).first) == kind);
    CHECK(kind_of_number(block_of(kind).last) == kind);
  }
}

TEST_CASE("ids.toml: a number keeps to its kind's block, and numbers ascend, none twice") {
  CHECK(has_error(refusal(id(2001, "lantern", "static")), "ids.toml: id[0].n", "outside its kind's block"));
  CHECK(has_error(refusal(id(999, "lantern", "static")), "ids.toml: id[0].n", ""));
  CHECK(has_error(refusal(id(5000, "rota", "table")), "ids.toml: id[0].n", ""));
  CHECK(has_error(refusal(id(1002, "lantern", "static") + id(1001, "ledger", "static")), "ids.toml: id[1].n",
                  "must be above the id before it"));
  CHECK(has_error(refusal(id(1001, "lantern", "static") + id(1001, "ledger", "static")), "ids.toml: id[1].n",
                  "must be above the id before it"));
  CHECK(has_error(refusal(id(1001, "lantern", "gadget")), "ids.toml: id[0].kind", "must be one of"));
}

TEST_CASE("ids.toml: a label is ours in form, and no label or earlier label is used twice") {
  for (const std::string_view bad : {"Lantern", "1lantern", "lan-tern", "lan tern", "_lantern", ""}) {
    CAPTURE(bad);
    CHECK(has_error(refusal(id(1001, bad, "static")), "ids.toml: id[0].label", "must be lower-case"));
  }
  CHECK(has_error(refusal(id(1001, std::string(49, 'a'), "static")), "ids.toml: id[0].label", "must be lower-case"));
  CHECK(parse_ids("schema = 1\n" + id(1001, std::string(48, 'a'), "static")).has_value());
  CHECK(has_error(refusal(id(1001, "lantern", "static") + id(1002, "lantern", "static")), "ids.toml: id[1].label",
                  "is an earlier id's label"));
  CHECK(has_error(refusal(id(1001, "lantern", "static", "was = \"lamp\"") + id(1002, "lamp", "static")),
                  "ids.toml: id[1].label", "is an earlier id's label"));
  CHECK(has_error(refusal(id(1001, "lantern", "static") + id(1002, "ledger", "static", "was = \"lantern\"")),
                  "ids.toml: id[1].was", "is an earlier id's label"));
  CHECK(has_error(refusal(id(1001, "lantern", "static", "was = \"lantern\"")), "ids.toml: id[0].was",
                  "is the label itself"));
  CHECK(has_error(refusal(id(1001, "lantern", "static", "was = \"Lamp\"")), "ids.toml: id[0].was",
                  "must be lower-case"));
}

TEST_CASE("ids.toml: the meaning, the need, the status and unknown keys") {
  const auto with = [](std::string_view means, std::string_view needs, std::string_view extra) {
    return "[[id]]\nn = 1001\nlabel = \"lantern\"\nkind = \"static\"\nmeans = \"" + std::string(means) +
           "\"\nneeds = \"" + std::string(needs) + "\"\n" + std::string(extra) + "\n";
  };
  CHECK(has_error(refusal(with("", "world", "required = false")), "ids.toml: id[0].means", "1 to 400 bytes"));
  CHECK(has_error(refusal(with(std::string(401, 'a'), "world", "required = false")), "ids.toml: id[0].means",
                  "1 to 400 bytes"));
  CHECK(has_error(refusal(with("caf\\u00E9", "world", "required = false")), "ids.toml: id[0].means",
                  "printable ASCII"));
  CHECK(has_error(refusal(with("A thing.", "weather", "required = false")), "ids.toml: id[0].needs",
                  "must be boot, a read group's name or task_service"));
  for (const std::string_view needs : {"boot", "session", "world", "combat", "player", "flags", "text", "ui", "place",
                                       "camera", "params", "status", "task_service"}) {
    CAPTURE(needs);
    CHECK(parse_ids("schema = 1\n" + with("A thing.", needs, "required = true")).has_value());
  }
  CHECK(has_error(refusal(with("A thing.", "world", "")), "ids.toml: id[0].required", ""));
  CHECK(has_error(refusal(with("A thing.", "world", "required = false\nstatus = \"asleep\"")),
                  "ids.toml: id[0].status", "must be one of active, retired"));
  CHECK(has_error(refusal(with("A thing.", "world", "required = true\nstatus = \"retired\"")),
                  "ids.toml: id[0].required", "must be false for a retired id"));
  CHECK(has_error(refusal(with("A thing.", "world", "required = false\nrva = 4096")), "ids.toml: id[0]",
                  "has an unknown key \"rva\""));  // the catalogue holds no fact about any build
  // The file itself: its schema, its list, and every problem at once.
  CHECK_FALSE(parse_ids("schema = 2\n" + id(1001, "lantern", "static")).has_value());
  CHECK_FALSE(parse_ids("schema = 1\n").has_value());
  CHECK(has_error(refusal("id = []\n"), "ids.toml: id", "must list 1 to 4000 ids"));
  CHECK(has_error(refusal("id = [1]\n"), "ids.toml: id[0]", "must be a table"));
  const SchemaErrors many = refusal(id(2001, "Lantern", "static") + id(1001, "ledger", "gadget"));
  CHECK(many.size() >= 3);
}

TEST_CASE("the repository's catalogue reads, and every role of the code is one of its ids") {
  const auto catalogue = parse_ids(test::kRepositoryIdsToml);
  if (!catalogue.has_value()) {
    for (const SchemaError& e : catalogue.error()) MESSAGE(describe(e));
  }
  REQUIRE(catalogue.has_value());
  for (std::size_t i = 0; i < roles::kItemCount; ++i) {
    const roles::ItemInfo& item = roles::info(static_cast<roles::Item>(i));
    CAPTURE(item.label);
    const IdEntry* id = catalogue->find(item.id);
    REQUIRE(id != nullptr);
    // The catalogue's label is the role's, or its earlier label where schema 1's files still say so.
    CHECK((id->label == item.label || id->was == item.label));
    CHECK(catalogue->find(item.label, true) == id);
    CHECK(id->kind == (item.kind == roles::ItemKind::Function ? IdKind::Function : IdKind::Static));
    CHECK(id->required == item.required);
    CHECK(id->status == IdStatus::Active);
  }
}

TEST_CASE("the catalogue's first ids are permanent: a number never changes its label or its kind") {
  // Append here when an id is added; never edit a line. A changed line is a renumbering, which the catalogue forbids.
  constexpr std::array<std::pair<std::uint32_t, std::string_view>, 20> kPinned = {{
      {1001, "module_handle"}, {1002, "save_data"},   {1003, "game_state"},       {1004, "field_area"},
      {1005, "task_hub"},      {1006, "characters"},  {1007, "session"},          {1008, "event_flags"},
      {1009, "hud"},           {1010, "menus"},       {1011, "loading"},          {1012, "world_clock"},
      {1013, "camera"},        {1014, "param_store"}, {1015, "messages"},         {2001, "task_add"},
      {2002, "task_remove"},   {3001, "character"},   {3002, "player_character"}, {4001, "task_groups"},
  }};
  const auto catalogue = parse_ids(test::kRepositoryIdsToml);
  REQUIRE(catalogue.has_value());
  CHECK(catalogue->entries().size() >= kPinned.size());
  for (const auto& [number, label] : kPinned) {
    CAPTURE(number);
    const IdEntry* id = catalogue->find(number);
    REQUIRE(id != nullptr);
    CHECK(id->label == label);
    CHECK(kind_of_number(number) == id->kind);
  }
}
