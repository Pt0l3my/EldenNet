// Lodestar's in-game round on the invented world (lodestar.hpp; proposal 0005 §6.1's instance_class; O92): the
// managers named by their instances' classes under the catalogue's labels, the report that carries no name, and the
// draft table a start can then take its places from. The world is fake_world.hpp's; the catalogue is the repository's.
#include <doctest.h>

#include <algorithm>
#include <ostream>
#include <string>

#include "en/gal/lodestar.hpp"
#include "en/gal/lodestar_trial.hpp"
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

TEST_CASE("in game: the reader's trial counts what each manager's readings saw, on the reader's own thread") {
  FakeWorld w;
  Round r = round_of(w);
  lodestar::Trial trial(*w.sys, w.base);
  for (const lodestar::ManagerFound& m : r.found.instances->managers) CHECK(trial.add(m));
  CHECK_FALSE(trial.add(manager(r, "hud")));  // once only
  lodestar::ManagerFound missing = manager(r, "hud");
  missing.id = 1999;
  missing.status = Status::Missing;
  CHECK_FALSE(trial.add(missing));  // only what the round named

  ReaderLoop loop(*w.sys, trial);
  std::uint64_t now = 2'000'000;
  const auto run = [&](int steps) {
    for (int i = 0; i < steps; ++i) {
      now += 33'333;
      w.sys->set_now_us(now);
      (void)loop.step();
    }
  };
  run(30);  // a second: about ten readings
  {
    const lodestar::TrialFound t = trial.found();
    CHECK(t.steps == 30);
    CHECK(t.readings >= 9);
    CHECK(t.readings <= 11);
    REQUIRE(t.managers.size() == 11);
    for (const lodestar::TrialManager& m : t.managers) {
      CAPTURE(m.label);
      CHECK(m.same == t.readings);
      CHECK(m.null + m.other + m.fault + m.torn == 0);
    }
    CHECK(t.copies == t.readings * 11 * 3);
    for (std::size_t k = 0; k < t.faults.size(); ++k) {
      CAPTURE(k);
      CHECK(t.faults[k] == 0);  // by guarded.hpp's FaultKind
    }
  }
  const std::uint64_t before = trial.readings();
  // The camera's static emptied; the HUD's instance turned into an object of another class; the clock's instance gone
  // (its static points at memory that cannot be read).
  const std::uint64_t eye = w.instance("camera");
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);
  w.set_vtable(w.instance("hud"), "tomb");
  w.mem.put<std::uint64_t>(w.at("st_world_clock"), 0x7000'0000'1000);
  run(30);
  const lodestar::TrialFound t = trial.found();
  const std::uint64_t later = t.readings - before;
  CHECK(later >= 9);
  const auto of = [&](std::string_view label) {
    const auto it = std::find_if(t.managers.begin(), t.managers.end(), [&](const auto& m) { return m.label == label; });
    REQUIRE(it != t.managers.end());
    return *it;
  };
  CHECK(of("camera").null == later);
  CHECK(of("hud").other == later);
  CHECK(of("world_clock").fault == later);
  CHECK(of("session").same == t.readings);
  CHECK(t.faults[static_cast<std::size_t>(FaultKind::Copy)] == later);
  w.mem.put<std::uint64_t>(w.at("st_camera"), eye);

  // The trial in the report: numbers and labels.
  r.found.trial = t;
  const auto report = lodestar::write_report(r.found);
  if (!report) MESSAGE(report::describe(report.error()));
  REQUIRE(report.has_value());
  CHECK(report->find("\"trial\"") != std::string::npos);
  CHECK(report->find("\"fault_copy\"") != std::string::npos);
}

TEST_CASE("in game: a slot that changes between its two reads is counted as torn, and judges nothing") {
  FakeWorld w;
  Round r = round_of(w);
  lodestar::Trial trial(*w.sys, w.base);
  REQUIRE(trial.add(manager(r, "camera")));
  const std::uint64_t slot = w.at("st_camera");
  const std::uint64_t eye = w.instance("camera");
  const std::uint64_t other = w.instance("hud");
  // The game moves the camera between the reader's read of the instance and its second read of the slot.
  int copies = 0;
  w.sys->fake_copier().before_copy = [&](std::uint64_t address) {
    if (address == eye && ++copies == 1) w.mem.put<std::uint64_t>(slot, other);
  };
  ReaderLoop loop(*w.sys, trial);
  (void)loop.step();
  const lodestar::TrialFound t = trial.found();
  REQUIRE(t.managers.size() == 1);
  CHECK(t.readings == 1);
  CHECK(t.managers[0].torn == 1);
  CHECK(t.managers[0].same + t.managers[0].other + t.managers[0].fault + t.managers[0].null == 0);
}

TEST_CASE("in game: a later pass names what was null at the first, and what was named stays named") {
  FakeWorld w;
  const std::uint64_t eye = w.instance("camera");
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);  // the title: no camera yet
  Round first = round_of(w);
  lodestar::InstancesFound held;
  lodestar::merge(held, *first.found.instances);
  CHECK(held.passes == 1);
  CHECK(held.named == 10);
  const auto of = [&](std::string_view label) {
    const auto it = std::find_if(held.managers.begin(), held.managers.end(), [&](const auto& m) { return m.label == label; });
    REQUIRE(it != held.managers.end());
    return *it;
  };
  CHECK(of("hud").pass == 1);
  CHECK(of("camera").pass == 0);
  // The world: the camera is made, and the HUD's slot is empty for a moment.
  w.mem.put<std::uint64_t>(w.at("st_camera"), eye);
  const std::uint32_t hud = of("hud").rva;
  w.mem.put<std::uint64_t>(w.at("st_hud"), 0);
  Round second = round_of(w);
  CHECK(second.found.instances->named == 10);  // this pass alone: the camera, not the HUD
  lodestar::merge(held, *second.found.instances);
  CHECK(held.passes == 2);
  CHECK(held.named == 11);
  CHECK(held.null == 1);  // the later pass's counts
  CHECK(of("camera").status == Status::Derived);
  CHECK(of("camera").pass == 2);
  CHECK(of("hud").status == Status::Derived);
  CHECK(of("hud").pass == 1);
  CHECK(of("hud").rva == hud);
}
