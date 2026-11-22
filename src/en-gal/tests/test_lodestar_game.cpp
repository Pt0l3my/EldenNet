// Lodestar's in-game round on the invented world (lodestar.hpp; proposal 0005 §6.1's instance_class; O92): the
// managers named by their instances' classes under the catalogue's labels, the report that carries no name, and the
// draft table a start can then take its places from. The world is fake_world.hpp's; the catalogue is the repository's.
#include <doctest.h>

#include <algorithm>
#include <ostream>
#include <string>

#include "en/gal/lodestar.hpp"
#include "en/gal/recipes.hpp"
#include "gal_ids_text.hpp"
#include "rig_gal.hpp"

using namespace en::gal;
using namespace en::gal::test;
using derive::Status;
using roles::Item;

namespace {

constexpr std::string_view kRecipes =
    "schema = 1\n[[recipe]]\nid = \"accessor\"\nkind = \"accessor\"\n"
    "steps = [\"load_slot\", \"test_loaded\", \"branch_set_short\", \"address_arg1\", \"call_relative\"]\n"
    "gives = { static = 0, class = 3, getter = 4 }\npath_suffix = \"One.h\"\n"
    "provenance = { method = \"own-derivation\", evidence = \"docs/ops/probes/invented.md\", date = \"2026-01-02\" }\n";

// The world's managers under the catalogue's labels, each by the class its instance has here; `task_hub` under the
// label it had before (the catalogue's `was`), and one lead the catalogue does not hold.
constexpr const char* kLeads = R"(schema = 1
product_name = "INVENTED GAME"

[fd4]
path_suffix = "One.h"

[singletons]
cs_task = "TaskHub"
characters = "Roster"
session = "Lobby"
event_flags = "Marks"
hud = "Hud"
menus = "Menus"
loading = "Veil"
world_clock = "Clock"
camera = "Eye"
param_store = "Tables"
messages = "Words"
no_such_thing = "Nowhere"

[register]
instance = "cs_task"

[task]
group = "W_Settle"
base = "W::NoTaskBase"

[vtable_sets]
character = "W::Chr"
player_character = "W::Hero"

[statics]
)";

struct Round {
  Gated gated;
  Catalogue ids;
  derive::Leads leads;
  lodestar::Found found;
};

lodestar::ReadWord reader_of(const FakeWorld& w, std::uint64_t* reads = nullptr) {
  return [&w, reads](std::uint64_t address) -> std::optional<std::uint64_t> {
    if (reads != nullptr) ++*reads;
    return w.mem.get<std::uint64_t>(address);
  };
}

Round round_of(FakeWorld& w, std::uint64_t* reads = nullptr) {
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  auto ids = parse_ids(kRepositoryIdsToml);
  REQUIRE(ids.has_value());
  auto leads = derive::parse_leads(kLeads);
  if (!leads) {
    for (const auto& e : leads.error()) MESSAGE(describe(e));
  }
  REQUIRE(leads.has_value());
  const auto recipes = parse_recipes(kRecipes, {});
  REQUIRE(recipes.has_value());
  Round r{std::move(*gated), std::move(*ids), std::move(*leads), {}};
  r.found = lodestar::find(r.gated.image, w.sys->image(), 0, r.leads, *recipes, nullptr);
  r.found.instances =
      lodestar::name_instances(r.gated.image, w.sys->image(), r.found.accessor, r.leads, r.ids, reader_of(w, reads));
  return r;
}

const lodestar::ManagerFound& manager(const Round& r, std::string_view label) {
  const auto& all = r.found.instances->managers;
  const auto it = std::find_if(all.begin(), all.end(), [&](const auto& m) { return m.label == label; });
  REQUIRE(it != all.end());
  return *it;
}

}  // namespace

TEST_CASE("in game: each manager is named by its instance's class, under the catalogue's label and number") {
  FakeWorld w;
  std::uint64_t reads = 0;
  const Round r = round_of(w, &reads);
  const lodestar::InstancesFound& n = *r.found.instances;
  CHECK(r.found.accessor.statics_found == 11);
  CHECK(n.tried == 11);
  CHECK(n.null == 0);
  CHECK(n.unread == 0);
  CHECK(n.unclassed == 0);
  CHECK(n.classed == 11);
  CHECK(reads == 11);  // one word at each instance, no more
  CHECK(n.leads == 12);
  CHECK(n.unlisted == 1);
  CHECK(n.named == 11);
  CHECK(n.ambiguous == 0);
  REQUIRE(n.managers.size() == 11);
  CHECK(std::is_sorted(n.managers.begin(), n.managers.end(), [](const auto& a, const auto& b) { return a.id < b.id; }));
  for (const Item item : {Item::TaskHub, Item::Characters, Item::Session, Item::EventFlags, Item::Hud, Item::Menus,
                          Item::Loading, Item::WorldClock, Item::Camera, Item::ParamStore, Item::Messages}) {
    const roles::ItemInfo& info = roles::info(item);
    CAPTURE(info.label);
    const lodestar::ManagerFound& m = manager(r, info.label);
    CHECK(m.id == info.id);
    CHECK(m.status == Status::Derived);
    CHECK(m.rva == w.at("st_" + std::string(info.label)) - w.base);
    CHECK(m.vtable == w.mem.get<std::uint64_t>(w.instance(std::string(info.label))) - w.base);
    CHECK(m.sites == 1);
    CHECK_FALSE(m.imp);
    REQUIRE(m.witnesses.size() == 1);
    CHECK(m.witnesses[0] == w.at("get_" + std::string(info.label)) + 5 - w.base);  // the load itself
  }

  // The report: numbers and the catalogue's labels. No class's name, as a lead gives it or as the image holds it.
  const auto report = lodestar::write_report(r.found);
  if (!report) MESSAGE(report::describe(report.error()));
  REQUIRE(report.has_value());
  CHECK(report->find("\"instances\"") != std::string::npos);
  CHECK(report->find("\"task_hub\"") != std::string::npos);
  for (const char* name : {"Roster", "TaskHub", "Lobby", "Nowhere", "cs_task", "W_Settle", ".?AV"}) {
    CAPTURE(name);
    CHECK(report->find(name) == std::string::npos);
  }
}

TEST_CASE("in game: a static with no instance yet names no one; two statics of one class leave the lead ambiguous") {
  FakeWorld w;
  const std::uint64_t eye = w.instance("camera");
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);  // not made yet (the title, say)
  {
    const Round r = round_of(w);
    CHECK(r.found.instances->null == 1);
    CHECK(r.found.instances->classed == 10);
    CHECK(r.found.instances->named == 10);
    CHECK(manager(r, "camera").status == Status::Missing);
    CHECK(manager(r, "camera").rva == 0);
    CHECK(manager(r, "hud").status == Status::Derived);
  }
  // The camera's static holds an object of the HUD's class: neither static is the HUD's for sure.
  w.mem.put<std::uint64_t>(w.at("st_camera"), eye);
  w.set_vtable(eye, "hud");
  {
    const Round r = round_of(w);
    CHECK(r.found.instances->classed == 11);
    CHECK(r.found.instances->ambiguous == 1);
    CHECK(r.found.instances->named == 9);
    CHECK(manager(r, "hud").status == Status::Ambiguous);
    CHECK(manager(r, "camera").status == Status::Missing);
  }
  // An instance whose first word is no table of methods the image knows, and one that cannot be read.
  w.mem.put<std::uint64_t>(eye, 0x1234);
  {
    const Round r = round_of(w);
    CHECK(r.found.instances->unclassed == 1);
    CHECK(manager(r, "hud").status == Status::Derived);
  }
  {
    auto gated = gate_image(*w.sys, w.builds);
    REQUIRE(gated.has_value());
    Round r = round_of(w);
    const auto refused = lodestar::name_instances(r.gated.image, w.sys->image(), r.found.accessor, r.leads, r.ids,
                                                  [](std::uint64_t) { return std::optional<std::uint64_t>{}; });
    CHECK(refused.unread == 11);
    CHECK(refused.named == 0);
    // A static that points into the image holds no instance: it is not read at all.
    w.mem.put<std::uint64_t>(w.at("st_camera"), w.at("st_hud"));
    std::uint64_t reads = 0;
    const auto inside = lodestar::name_instances(r.gated.image, w.sys->image(), r.found.accessor, r.leads, r.ids,
                                                 reader_of(w, &reads));
    CHECK(inside.unread == 1);
    CHECK(reads == 10);
  }
}

TEST_CASE("in game: a lead's name with Imp names the class too, and the report says so") {
  FakeWorld w;
  // The marks' class renamed in place, in the image as loaded: Marks becomes MaImp, and the lead asks for Ma.
  const std::uint64_t name = w.at("td_marks") + 16;
  for (std::size_t i = 0; i < 5; ++i) REQUIRE(w.mem.get<char>(name + 4 + i) == "Marks"[i]);
  for (std::size_t i = 0; i < 5; ++i) w.mem.put<char>(name + 4 + i, "MaImp"[i]);
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  auto ids = parse_ids(kRepositoryIdsToml);
  REQUIRE(ids.has_value());
  std::string text = kLeads;
  const std::size_t at = text.find("\"Marks\"");
  REQUIRE(at != std::string::npos);
  text.replace(at, 7, "\"Ma\"");
  const auto leads = derive::parse_leads(text);
  REQUIRE(leads.has_value());
  const auto recipes = parse_recipes(kRecipes, {});
  REQUIRE(recipes.has_value());
  const lodestar::Found found = lodestar::find(gated->image, w.sys->image(), 0, *leads, *recipes, nullptr);
  const auto n = lodestar::name_instances(gated->image, w.sys->image(), found.accessor, *leads, *ids, reader_of(w));
  CHECK(n.named == 11);
  const auto it = std::find_if(n.managers.begin(), n.managers.end(), [](const auto& m) { return m.label == "event_flags"; });
  REQUIRE(it != n.managers.end());
  CHECK(it->status == Status::Derived);
  CHECK(it->imp);
  CHECK(it->rva == w.at("st_event_flags") - w.base);
}

TEST_CASE("in game: the draft table is one a start takes its places from, each entry checked and a lead") {
  FakeWorld w;
  const Round r = round_of(w);
  const TableProvenance provenance{"0.1.0-test.1", std::string(64, 'a'), "l5", "2026-01-02", {"our own notes"}};
  const lodestar::DraftTable draft = lodestar::draft_table(r.found, r.ids, provenance, r.gated.image, w.sys->image());
  CHECK(draft.entries == 13);  // eleven managers and the two classes; this world has no groups' table to find
  CHECK(draft.left_out == 0);
  CHECK(draft.witnesses_dropped == 0);
  CHECK(draft.text.find("[item.task_hub]") != std::string::npos);
  CHECK(draft.text.find("[item.character]") != std::string::npos);
  CHECK(draft.text.find("status = \"confirmed\"") == std::string::npos);
  for (const char* name : {"Roster", "TaskHub", "W::Chr", "cs_task"}) CHECK(draft.text.find(name) == std::string::npos);

  // The world's layout with the draft as its table. A release build refuses a lead, as it must; once the review has
  // confirmed the entries, a start on a thread of our own takes every manager and both classes from the table.
  std::string layout = w.layout;
  REQUIRE(layout.rfind("schema = 1", 0) == 0);
  layout.replace(0, 10, "schema = 2");
  layout += "\n" + draft.text;
  DiscoveryOptions options;
  options.ids = std::string(kRepositoryIdsToml);
  options.frame = FrameProvider::OwnThread;
  CHECK_FALSE(discover(*w.sys, r.gated, w.patterns, layout, options).has_value());
  DiscoveryOptions dev = options;
  dev.mode = LayoutMode::Dev;
  const auto as_leads = discover(*w.sys, r.gated, w.patterns, layout, dev);
  if (!as_leads) {
    for (const auto& e : as_leads.error()) MESSAGE(describe(e));
  }
  REQUIRE(as_leads.has_value());
  CHECK(as_leads->ready());
  CHECK(as_leads->report().table_items == 13);
  CHECK(as_leads->report().table_refused == 0);
  CHECK(as_leads->report().fd4_hits == 0);  // every manager came from the table: the accessors were not looked for
  for (const Item item : {Item::TaskHub, Item::Characters, Item::Camera, Item::Messages}) {
    const ItemResult& got = as_leads->report().items[roles::index(item)];
    CAPTURE(roles::info(item).label);
    CHECK(got.from_table);
    CHECK(got.address == w.at("st_" + std::string(roles::info(item).label)));
  }
  CHECK(as_leads->vtables(roles::Cls::Character).all().size() == 4);
  CHECK(as_leads->vtables(roles::Cls::PlayerCharacter).contains(w.vtable("hero")));
}

TEST_CASE("in game: an entry the image does not bear out is left out of the draft, and a stale witness dropped") {
  FakeWorld w;
  Round r = round_of(w);
  // A manager "found" in the code section, and one whose witness is a place that refers to another static.
  for (lodestar::ManagerFound& m : r.found.instances->managers) {
    if (m.label == "hud") m.rva = static_cast<std::uint32_t>(w.at("get_hud") - w.base) & ~7u;
    if (m.label == "menus") m.witnesses.push_back(static_cast<std::uint32_t>(w.at("get_camera") + 5 - w.base));
  }
  const TableProvenance provenance{"0.1.0-test.1", std::string(64, 'a'), "l5", "2026-01-02", {}};
  const lodestar::DraftTable draft = lodestar::draft_table(r.found, r.ids, provenance, r.gated.image, w.sys->image());
  CHECK(draft.entries == 12);
  CHECK(draft.left_out == 1);
  CHECK(draft.witnesses_dropped == 1);
  CHECK(draft.text.find("[item.hud]") == std::string::npos);
  CHECK(draft.text.find("[item.menus]") != std::string::npos);
}
