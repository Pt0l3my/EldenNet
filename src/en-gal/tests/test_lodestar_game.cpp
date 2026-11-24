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
#include "en/gal/resolve.hpp"
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
  r.found = lodestar::find(r.gated.image, w.sys->image(), 0, r.leads, *recipes, nullptr, &r.ids);
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

  // What the image holds under each lead's name, counted: a class of that name with one table of methods for each
  // manager here, and nothing at all for the lead that names no class of this world.
  REQUIRE(r.found.lead_classes.size() == 12);
  for (const lodestar::LeadClasses& c : r.found.lead_classes) {
    CAPTURE(c.label);
    if (c.label == "no_such_thing") {
      CHECK(c.id == 0);
      CHECK(c.named + c.named_imp + c.vtables + c.texts == 0);
    } else {
      CHECK(c.id >= 1005);
      CHECK(c.named == 1);
      CHECK(c.named_imp == 0);
      CHECK(c.vtables == 1);
      CHECK(c.texts == 1);
    }
  }

  // The report: numbers and the catalogue's labels. No class's name, as a lead gives it or as the image holds it.
  const auto report = lodestar::write_report(r.found);
  if (!report) MESSAGE(report::describe(report.error()));
  REQUIRE(report.has_value());
  CHECK(report->find("\"instances\"") != std::string::npos);
  CHECK(report->find("\"lead_classes\"") != std::string::npos);
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
    // A static that points into the image: the object there is read through the image's view, not as a heap object.
    // Here it is another static's slot, whose first word is a heap address and no table of methods.
    w.mem.put<std::uint64_t>(w.at("st_camera"), w.at("st_hud"));
    std::uint64_t reads = 0;
    const auto inside = lodestar::name_instances(r.gated.image, w.sys->image(), r.found.accessor, r.leads, r.ids,
                                                 reader_of(w, &reads));
    CHECK(inside.in_image == 1);
    CHECK(inside.unread == 0);
    CHECK(inside.unclassed == 1);  // a heap address is no table of methods
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
  CHECK(of("camera").null_runs == 1);  // one stretch of empty readings, as long as it lasted
  CHECK(of("camera").null_longest == later);
  CHECK(of("session").null_runs == 0);
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

TEST_CASE("in game: where no static holds the lead's own class, one that holds a class deriving from it names it") {
  FakeWorld w;
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  auto ids = parse_ids(kRepositoryIdsToml);
  REQUIRE(ids.has_value());
  // The camera's lead asks for the characters' base class, which no manager is; the camera's static is given a
  // character object of a class that derives from it (the hero's), and the HUD's lead stays as it was.
  std::string text = kLeads;
  const std::size_t at = text.find("camera = \"Eye\"");
  REQUIRE(at != std::string::npos);
  text.replace(at, 14, "camera = \"Chr\"");
  const auto leads = derive::parse_leads(text);
  REQUIRE(leads.has_value());
  const auto recipes = parse_recipes(kRecipes, {});
  REQUIRE(recipes.has_value());
  const lodestar::Found found = lodestar::find(gated->image, w.sys->image(), 0, *leads, *recipes, nullptr, &*ids);
  w.set_vtable(w.instance("camera"), "hero");
  const lodestar::Namer namer(gated->image, w.sys->image(), *leads, *ids);
  const auto of = [](const lodestar::InstancesFound& n, std::string_view label) {
    const auto it = std::find_if(n.managers.begin(), n.managers.end(), [&](const auto& m) { return m.label == label; });
    REQUIRE(it != n.managers.end());
    return *it;
  };
  std::uint64_t reads = 0;
  const lodestar::InstancesFound n = namer.pass(w.sys->image(), found.accessor, reader_of(w, &reads));
  CHECK(of(n, "camera").status == Status::Derived);
  CHECK(of(n, "camera").by_base);
  CHECK_FALSE(of(n, "camera").imp);
  CHECK(of(n, "camera").rva == w.at("st_camera") - w.base);
  CHECK(of(n, "hud").status == Status::Derived);
  CHECK_FALSE(of(n, "hud").by_base);
  CHECK(n.named == 11);
  // The lead's class has more tables of methods than its own: every class deriving from it counts.
  const auto cls = std::find_if(found.lead_classes.begin(), found.lead_classes.end(),
                                [](const auto& c) { return c.label == "camera"; });
  REQUIRE(cls != found.lead_classes.end());
  CHECK(cls->named == 1);
  CHECK(cls->vtables == 4);  // the base, and the three that derive from it here
  // An object of the class itself takes precedence over one of a class that derives from it.
  w.set_vtable(w.instance("hud"), "chr");
  const lodestar::InstancesFound both = namer.pass(w.sys->image(), found.accessor, reader_of(w));
  CHECK(of(both, "camera").status == Status::Derived);
  CHECK_FALSE(of(both, "camera").by_base);
  CHECK(of(both, "camera").rva == w.at("st_hud") - w.base);
  // A second pass costs one word an instance, and reads the image no more than its slots: the namer was made once.
  reads = 0;
  (void)namer.pass(w.sys->image(), found.accessor, reader_of(w, &reads));
  CHECK(reads == 11);
}

TEST_CASE("in game: the trial counts its readings by stage, and each manager's empty stretches") {
  FakeWorld w;
  Round r = round_of(w);
  lodestar::Trial trial(*w.sys, w.base);
  for (const lodestar::ManagerFound& m : r.found.instances->managers) REQUIRE(trial.add(m));
  trial.set_stage(roles::info(Item::Characters).id);
  ReaderLoop loop(*w.sys, trial);
  std::uint64_t now = 2'000'000;
  const auto read = [&](int readings) {
    for (int i = 0; i < readings * 3; ++i) {
      now += 33'334;
      w.sys->set_now_us(now);
      (void)loop.step();
    }
    return trial.found();
  };
  const auto of = [](const lodestar::TrialFound& t, std::string_view label) {
    const auto it = std::find_if(t.managers.begin(), t.managers.end(), [&](const auto& m) { return m.label == label; });
    REQUIRE(it != t.managers.end());
    return *it;
  };
  // The title: no characters' manager yet, and the camera not made either.
  const std::uint64_t roster = w.instance("characters");
  const std::uint64_t eye = w.instance("camera");
  w.mem.put<std::uint64_t>(w.at("st_characters"), 0);
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);
  const lodestar::TrialFound title = read(10);
  CHECK(title.world_readings == 0);
  CHECK(title.stage_changes == 0);
  CHECK(of(title, "camera").null == title.readings);
  CHECK(of(title, "camera").null_world == 0);
  // The world: both made. Then a loading screen takes the camera away twice, for a while each time.
  w.mem.put<std::uint64_t>(w.at("st_characters"), roster);
  w.mem.put<std::uint64_t>(w.at("st_camera"), eye);
  (void)read(10);
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);
  (void)read(4);
  w.mem.put<std::uint64_t>(w.at("st_camera"), eye);
  (void)read(5);
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);
  (void)read(7);
  w.mem.put<std::uint64_t>(w.at("st_camera"), eye);
  const lodestar::TrialFound world = read(5);
  CHECK(world.stage_changes == 1);
  CHECK(world.world_readings == world.readings - title.readings);
  CHECK(of(world, "camera").null_runs == 3);  // the title's stretch, then the two in the world
  CHECK(of(world, "camera").null_longest == title.readings);
  CHECK(of(world, "camera").null_world >= 9);
  CHECK(of(world, "camera").null_world <= 13);
  CHECK(of(world, "camera").null == title.readings + of(world, "camera").null_world);
  CHECK(of(world, "characters").null == title.readings);
  CHECK(of(world, "characters").null_world == 0);
  CHECK(of(world, "session").null == 0);
  // Back to the title: the stage changes again, and its readings are no longer the world's.
  w.mem.put<std::uint64_t>(w.at("st_characters"), 0);
  const lodestar::TrialFound back = read(5);
  CHECK(back.stage_changes == 2);
  CHECK(back.world_readings == world.world_readings);
}

TEST_CASE("in game: a manager whose object lies inside the image is named through the image's own view") {
  FakeWorld w;
  // The messages' manager kept in the image's own data: its static points at a place in the image whose first word
  // is the class's table of methods (here the menus' static's slot, lent for the test).
  const std::uint64_t words = w.instance("messages");
  const std::uint64_t vtable = w.mem.get<std::uint64_t>(words);
  const std::uint64_t menus = w.instance("menus");
  const std::uint64_t place = w.at("st_menus");
  w.mem.put<std::uint64_t>(place, vtable);
  w.mem.put<std::uint64_t>(w.at("st_messages"), place);
  std::uint64_t reads = 0;
  const Round r = round_of(w, &reads);
  // Two statics now point into the image: the messages' (at the lent place) and the menus' own, which holds a table
  // of methods' address. Neither cost a guarded read outside the image.
  CHECK(r.found.instances->in_image == 2);
  CHECK(reads == 9);
  CHECK(manager(r, "menus").status == Status::Missing);  // a table of methods is no object of the menus' class
  CHECK(manager(r, "messages").status == Status::Derived);
  CHECK(manager(r, "messages").rva == w.at("st_messages") - w.base);
  CHECK(manager(r, "messages").vtable == vtable - w.base);
  (void)menus;
}

TEST_CASE("in game: a lead's tables of methods, and a manager set by the search of the data's slots") {
  FakeWorld w;
  Round r = round_of(w);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  const std::uint32_t hud = roles::info(Item::Hud).id;
  const std::vector<std::uint32_t> tables = namer.vtables(hud);
  REQUIRE(tables.size() == 1);
  CHECK(tables[0] == w.vtable("hud") - w.base);
  CHECK(namer.vtables(1999).empty());
  // The HUD and the camera not named by a pass (their statics empty); the search then finds the HUD's object behind
  // one slot, and the camera's behind two.
  lodestar::InstancesFound held;
  const std::uint64_t eye = w.instance("camera");
  w.mem.put<std::uint64_t>(w.at("st_hud"), 0);
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  CHECK(held.named == 9);
  const auto of = [&](std::string_view label) {
    const auto it = std::find_if(held.managers.begin(), held.managers.end(), [&](const auto& m) { return m.label == label; });
    REQUIRE(it != held.managers.end());
    return *it;
  };
  const auto searched = [](std::vector<std::uint32_t> slots, std::uint32_t objects, std::uint32_t vtable) {
    lodestar::SlotSearchResult out;
    out.slots = std::move(slots);
    out.objects = objects;
    out.vtable = vtable;
    out.candidates = 400;
    out.checked = 400;
    out.unreadable = 7;
    return out;
  };
  // A search that finds nothing leaves the manager as it was, and its own numbers with it.
  lodestar::set_by_search(held, hud, searched({}, 0, 0));
  CHECK(held.named == 9);
  CHECK(of("hud").status == Status::Missing);
  CHECK(of("hud").search_runs == 1);
  CHECK(of("hud").search_candidates == 400);
  CHECK(of("hud").search_checked == 400);
  CHECK(of("hud").search_unreadable == 7);
  lodestar::set_by_search(held, hud, searched({0x1238}, 1, tables[0]));
  CHECK(of("hud").search_runs == 2);
  CHECK(of("hud").search_objects == 1);
  CHECK(of("hud").search_slots == std::vector<std::uint32_t>{0x1238});
  CHECK(held.named == 10);
  CHECK(of("hud").status == Status::Derived);
  CHECK(of("hud").by_search);
  CHECK(of("hud").rva == 0x1238);
  CHECK(of("hud").sites == 0);
  CHECK(of("hud").witnesses.empty());
  CHECK(of("hud").pass == 1);
  // Two slots that hold two objects of the class leave it ambiguous.
  const std::uint32_t camera = roles::info(Item::Camera).id;
  lodestar::set_by_search(held, camera, searched({0x2008, 0x2000}, 2, 0x3000));
  CHECK(of("camera").status == Status::Ambiguous);
  CHECK(of("camera").search_found == 2);
  CHECK(of("camera").rva == 0x2000);  // the first by offset
  CHECK(held.ambiguous == 1);
  // What the search found stays through a later pass: the named one named, the ambiguous one ambiguous, and the
  // search's numbers with both. (The second round in game lost them here: docs/ops/probes/2026-11-22-l5.md.)
  lodestar::set_in_data(held, camera, 3, 0x7000);
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  CHECK(of("hud").by_search);
  CHECK(of("hud").rva == 0x1238);
  CHECK(of("hud").search_runs == 2);
  CHECK(of("camera").status == Status::Ambiguous);
  CHECK(of("camera").by_search);
  CHECK(of("camera").search_found == 2);
  CHECK(of("camera").search_slots == std::vector<std::uint32_t>{0x2000, 0x2008});
  CHECK(of("camera").in_data == 3);
  CHECK(of("camera").in_data_rva == 0x7000);
  CHECK(held.named == 10);
  CHECK(held.ambiguous == 1);
  // A later search that finds nothing does not undo an earlier finding.
  lodestar::set_by_search(held, camera, searched({}, 0, 0));
  CHECK(of("camera").search_found == 2);
  CHECK(of("camera").search_runs == 2);
  // Two slots that hold one object are copies of one address: the manager is named, by the first, and both are kept.
  lodestar::set_by_search(held, camera, searched({0x2008, 0x2000}, 1, 0x3000));
  CHECK(of("camera").status == Status::Derived);
  CHECK(of("camera").rva == 0x2000);
  CHECK(of("camera").search_objects == 1);
  CHECK(of("camera").search_found == 2);
  CHECK(held.named == 11);
  CHECK(held.ambiguous == 0);
  // A manager a pass named is not renamed by a search; the search's numbers are kept all the same.
  lodestar::set_by_search(held, roles::info(Item::Session).id, searched({0x4000}, 1, 0x5000));
  CHECK_FALSE(of("session").by_search);
  CHECK(of("session").search_found == 1);
  // A pass that names a manager the search left ambiguous takes it over, and the search's numbers stay.
  {
    lodestar::InstancesFound two;
    lodestar::merge(two, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
    lodestar::set_by_search(two, camera, searched({0x2008, 0x2000}, 2, 0x3000));
    w.mem.put<std::uint64_t>(w.at("st_camera"), eye);
    lodestar::merge(two, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
    const auto it = std::find_if(two.managers.begin(), two.managers.end(), [&](const auto& m) { return m.id == camera; });
    REQUIRE(it != two.managers.end());
    CHECK(it->status == Status::Derived);
    CHECK_FALSE(it->by_search);
    CHECK(it->rva == w.at("st_camera") - w.base);
    CHECK(it->pass == 2);
    CHECK(it->search_found == 2);
    w.mem.put<std::uint64_t>(w.at("st_camera"), 0);
  }
  // The draft table: a static the search found is given by its instance's class alone, with no agreement claimed.
  r.found.instances = held;
  for (lodestar::ManagerFound& m : r.found.instances->managers) {
    if (m.label == "hud") m.rva = static_cast<std::uint32_t>(w.at("st_hud") - w.base);  // a place a static may lie
  }
  const TableProvenance provenance{"0.1.0-test.1", std::string(64, 'a'), "l5", "2026-01-02", {}};
  const lodestar::DraftTable draft = lodestar::draft_table(r.found, r.ids, provenance, r.gated.image, w.sys->image());
  const std::size_t at = draft.text.find("[item.hud]");
  REQUIRE(at != std::string::npos);
  const std::string entry = draft.text.substr(at, draft.text.find("[item.", at + 5) - at);
  CHECK(entry.find("found_by = [\"instance_class\"]") != std::string::npos);
  CHECK(entry.find("agree") == std::string::npos);
  CHECK(entry.find("witnesses") == std::string::npos);
}

TEST_CASE("in game: the clock search knows the world's clock by how its minutes and hours behave") {
  FakeWorld w;
  Round r = round_of(w);
  std::vector<std::uint32_t> statics;
  for (const lodestar::StaticFound& s : r.found.accessor.statics) statics.push_back(s.rva);
  lodestar::ClockSearch search(statics, w.base, r.gated.image.size_of_image);
  GuardedReader reader(*w.sys);

  struct Body final : TaskBody {
    Body(lodestar::ClockSearch& s, GuardedReader& r) : search(s), reader(r) {}
    void run(const en::proto::GameThread& gt, const FrameCtx& frame) override {
      reader.begin_tick();
      (void)search.step(gt, reader, 64, frame.qpc_now_us);
    }
    lodestar::ClockSearch& search;
    GuardedReader& reader;
  } body(search, reader);
  ReaderLoop loop(*w.sys, body);

  // The world's clock here: the hour one byte at 0x10, the minute at 0x11. A game minute a second; two and a half
  // hours go by. The HUD's object holds a counter that jumps about, the menus' one that counts down.
  const std::uint64_t clock = w.instance("world_clock");
  const std::uint64_t hud = w.instance("hud");
  const std::uint64_t menus = w.instance("menus");
  std::uint8_t hour = 9, minute = 30;
  std::uint64_t now = 5'000'000;
  for (int second = 0; second < 150; ++second) {
    now += 1'000'000;
    w.sys->set_now_us(now);
    (void)loop.step();
    minute = static_cast<std::uint8_t>((minute + 1) % 60);
    if (minute == 0) hour = static_cast<std::uint8_t>((hour + 1) % 24);
    w.mem.put<std::uint8_t>(clock + 0x10, hour);
    w.mem.put<std::uint8_t>(clock + 0x11, minute);
    w.mem.put<std::uint8_t>(hud + 0x08, static_cast<std::uint8_t>((second * 17) % 60));
    w.mem.put<std::uint8_t>(menus + 0x08, static_cast<std::uint8_t>(59 - second % 60));
  }
  const lodestar::ClockFound found = search.found();
  CHECK(found.candidates == 11);
  CHECK(found.passes >= 140);
  CHECK(found.unreadable == 0);
  REQUIRE(found.hits.size() == 1);  // the jumping counter and the one that counts down are no minutes
  const lodestar::ClockHit& hit = found.hits[0];
  CHECK(hit.rva == w.at("st_world_clock") - w.base);
  CHECK(hit.offset == 0x11);
  CHECK(hit.width == 1);
  CHECK(hit.ticks >= 140);
  CHECK(hit.wraps == 2);
  CHECK(hit.has_hour);
  CHECK(hit.hour_offset == 0x10);
  CHECK(hit.hour_steps == 2);

  // In the report: numbers only.
  r.found.clock = found;
  r.found.rising = lodestar::RisingFound{12, 3, 0, 1, {lodestar::RisingHit{0x1000, 0x20, 5, 2, 0}}};
  r.found.search = lodestar::SearchFound{120, 20, 95, 5, 300, 4, 1, 0, 50, 2};
  r.found.members = lodestar::MembersFound{11, 0, 40, 1, {lodestar::MemberHit{0, "messages", 0x1000, 0x18, false, 0x2000}}};
  const auto report = lodestar::write_report(r.found);
  if (!report) MESSAGE(report::describe(report.error()));
  REQUIRE(report.has_value());
  CHECK(report->find("\"clock_hits\"") != std::string::npos);
  CHECK(report->find("\"rising_hits\"") != std::string::npos);
  CHECK(report->find("\"pointer_like\"") != std::string::npos);
  CHECK(report->find("\"member_hits\"") != std::string::npos);
}

TEST_CASE("in game: the clock search takes a new object as new, skips an empty slot, and gives up on what it cannot read") {
  FakeWorld w;
  Round r = round_of(w);
  const std::uint32_t clock_slot = static_cast<std::uint32_t>(w.at("st_world_clock") - w.base);
  const std::uint32_t eye_slot = static_cast<std::uint32_t>(w.at("st_camera") - w.base);
  lodestar::ClockSearch search({clock_slot, eye_slot}, w.base, r.gated.image.size_of_image);
  GuardedReader reader(*w.sys);
  struct Body final : TaskBody {
    Body(lodestar::ClockSearch& s, GuardedReader& r) : search(s), reader(r) {}
    void run(const en::proto::GameThread& gt, const FrameCtx& frame) override {
      reader.begin_tick();
      (void)search.step(gt, reader, 8, frame.qpc_now_us);
    }
    lodestar::ClockSearch& search;
    GuardedReader& reader;
  } body(search, reader);
  ReaderLoop loop(*w.sys, body);
  // The camera's static points at memory that cannot be read: counted once, and not tried again.
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0x7000'0000'1000);
  const std::uint64_t first = w.instance("world_clock");
  std::uint64_t now = 5'000'000;
  std::uint8_t minute = 30;  // where this world's clock stands
  const auto second = [&](std::uint64_t object) {
    now += 1'000'000;
    w.sys->set_now_us(now);
    (void)loop.step();
    minute = static_cast<std::uint8_t>((minute + 1) % 60);
    if (object != 0) w.mem.put<std::uint8_t>(object + 0x11, minute);
  };
  for (int i = 0; i < 8; ++i) second(first);
  // A loading screen: the slot empty for a while, then a new clock object whose minutes start elsewhere. The jump
  // between the two objects is no jump of a field: the new object's first copy is compared with nothing.
  w.mem.put<std::uint64_t>(w.at("st_world_clock"), 0);
  for (int i = 0; i < 3; ++i) second(0);
  const std::uint64_t other = w.instance("hud");  // any other readable object stands in for the new clock
  w.mem.put<std::uint8_t>(other + 0x10, 3);
  minute = 40;
  w.mem.put<std::uint8_t>(other + 0x11, minute);
  w.mem.put<std::uint64_t>(w.at("st_world_clock"), other);
  for (int i = 0; i < 8; ++i) second(other);
  const lodestar::ClockFound found = search.found();
  CHECK(found.candidates == 2);
  CHECK(found.unreadable == 1);
  REQUIRE(found.hits.size() == 1);
  CHECK(found.hits[0].rva == clock_slot);
  CHECK(found.hits[0].offset == 0x11);
  CHECK(found.hits[0].ticks >= 12);
  CHECK(found.hits[0].ticks <= 15);
}

TEST_CASE("in game: the member search finds a lead's object inside another object, or behind a pointer in it") {
  FakeWorld w;
  Round r = round_of(w);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  const std::uint32_t messages = roles::info(Item::Messages).id;
  // What the search looks for: the tables of methods of the messages' class (a lead not named, say).
  std::vector<lodestar::Namer::LeadTable> tables;
  for (lodestar::Namer::LeadTable& t : namer.lead_tables()) {
    if (t.id == messages) tables.push_back(std::move(t));
  }
  REQUIRE(tables.size() == 1);
  CHECK(tables[0].label == "messages");
  CHECK(tables[0].vtable == w.vtable("words") - w.base);
  // The messages' static is empty; the session's object points at the messages' object from its 0x18, and the HUD's
  // object holds a table of methods of that class at its 0x38, as an object lying inside it would.
  const std::uint64_t words = w.instance("messages");
  w.mem.put<std::uint64_t>(w.at("st_messages"), 0);
  w.mem.put<std::uint64_t>(w.instance("session") + 0x18, words);
  w.mem.put<std::uint64_t>(w.instance("hud") + 0x38, w.vtable("words"));
  std::vector<std::uint32_t> statics;
  for (const lodestar::StaticFound& st : lodestar::all_statics(r.found.accessor)) statics.push_back(st.rva);
  lodestar::MemberSearch search(statics, tables, w.base, r.gated.image.size_of_image);
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t now = 1'000'000;
  bool ended = false;
  int steps = 0;
  for (; steps < 1000 && !ended; ++steps) {
    reader.begin_tick();
    ended = search.step(gt, reader, 4, now);  // a few reads a step: an object is gone through over several steps
    now += 33'000;
  }
  REQUIRE(ended);
  CHECK(steps >= 3);
  lodestar::MembersFound found = search.found();
  CHECK(found.passes == 1);
  CHECK(found.objects == 10);  // eleven statics, one of them empty
  CHECK(found.unreadable == 0);
  CHECK(found.pointers > 0);
  REQUIRE(found.hits.size() == 2);
  const auto hit_at = [&](std::uint64_t slot) {
    const auto it = std::find_if(found.hits.begin(), found.hits.end(),
                                 [&](const lodestar::MemberHit& h) { return h.rva == slot - w.base; });
    REQUIRE(it != found.hits.end());
    return *it;
  };
  const lodestar::MemberHit behind = hit_at(w.at("st_session"));
  CHECK(behind.id == messages);
  CHECK(behind.label == "messages");
  CHECK(behind.offset == 0x18);
  CHECK_FALSE(behind.embedded);
  CHECK(behind.vtable == w.vtable("words") - w.base);
  const lodestar::MemberHit inside = hit_at(w.at("st_hud"));
  CHECK(inside.offset == 0x38);
  CHECK(inside.embedded);
  // It rests between passes: nothing is read until the next is due, and a second pass finds the same two, once.
  const std::uint64_t pointers = found.pointers;
  reader.begin_tick();
  CHECK_FALSE(search.step(gt, reader, 64, now + 1'000'000));
  CHECK(search.found().pointers == pointers);
  now += lodestar::MemberSearch::kPassEveryUs + 1'000'000;
  ended = false;
  for (int k = 0; k < 1000 && !ended; ++k) {
    reader.begin_tick();
    ended = search.step(gt, reader, 64, now);
  }
  REQUIRE(ended);
  found = search.found();
  CHECK(found.passes == 2);
  CHECK(found.hits.size() == 2);
  // The object's own first word is not a member: a static that holds the lead's object itself is no hit.
  w.mem.put<std::uint64_t>(w.at("st_messages"), words);
  lodestar::MemberSearch again(statics, tables, w.base, r.gated.image.size_of_image);
  ended = false;
  for (int k = 0; k < 1000 && !ended; ++k) {
    reader.begin_tick();
    ended = again.step(gt, reader, 64, now);
  }
  REQUIRE(ended);
  for (const lodestar::MemberHit& h : again.found().hits) CHECK(h.rva != w.at("st_messages") - w.base);
  // Nothing to look for, or nowhere to look: it does nothing.
  lodestar::MemberSearch none(statics, {}, w.base, r.gated.image.size_of_image);
  CHECK_FALSE(none.step(gt, reader, 64, now));
  CHECK(none.found().objects == 0);
}

TEST_CASE("in game: a lead the catalogue does not hold is counted by the statics that hold its class") {
  FakeWorld w;
  Round r = round_of(w);
  // The lead `no_such_thing` has no class here: counted, with nothing held.
  REQUIRE(r.found.instances->others.size() == 1);
  CHECK(r.found.instances->others[0].label == "no_such_thing");
  CHECK(r.found.instances->others[0].held == 0);
  // With a class one static's object has, it is counted with that static and its table of methods; never named.
  std::string leads_text(kLeads);
  const std::size_t at = leads_text.find("\"Nowhere\"");
  REQUIRE(at != std::string::npos);
  leads_text.replace(at, 9, "\"Save\"");
  auto leads = derive::parse_leads(leads_text);
  REQUIRE(leads.has_value());
  const lodestar::Namer namer(r.gated.image, w.sys->image(), *leads, r.ids);
  lodestar::AccessorFound accessor = r.found.accessor;
  // The save's static is none of the accessor's here: it stands in as a variant's new static.
  lodestar::VariantFound variant;
  variant.label = "accessor_compare";
  variant.statics.push_back(lodestar::StaticFound{static_cast<std::uint32_t>(w.at("st_save_data") - w.base), 0, 3, {}});
  accessor.variants.push_back(variant);
  const lodestar::InstancesFound pass = namer.pass(w.sys->image(), accessor, reader_of(w));
  CHECK(pass.tried == 12);
  CHECK(pass.named == 11);
  REQUIRE(pass.others.size() == 1);
  CHECK(pass.others[0].held == 1);
  CHECK(pass.others[0].rva == w.at("st_save_data") - w.base);
  CHECK(pass.others[0].vtable == w.vtable("save") - w.base);
  const auto text = [&] {
    lodestar::Found f = r.found;
    f.instances = pass;
    return lodestar::write_report(f);
  }();
  REQUIRE(text.has_value());
  CHECK(text->find("\"others\"") != std::string::npos);
  CHECK(text->find("\"held\": 1") != std::string::npos);
}

TEST_CASE("in game: a manager behind a static only a variant of the accessor's shape reaches is named, and marked") {
  FakeWorld w;
  Round r = round_of(w);
  // The camera's static taken out of the accessor's own and handed back as a variant's new static.
  lodestar::AccessorFound accessor = r.found.accessor;
  const std::uint32_t slot = static_cast<std::uint32_t>(w.at("st_camera") - w.base);
  const auto it = std::find_if(accessor.statics.begin(), accessor.statics.end(),
                               [&](const lodestar::StaticFound& s) { return s.rva == slot; });
  REQUIRE(it != accessor.statics.end());
  lodestar::VariantFound variant;
  variant.label = "accessor_gap";
  variant.statics.push_back(*it);
  accessor.statics.erase(it);
  accessor.variants.push_back(variant);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  const lodestar::InstancesFound pass = namer.pass(w.sys->image(), accessor, reader_of(w));
  CHECK(pass.tried == 11);
  CHECK(pass.named == 11);
  for (const lodestar::ManagerFound& m : pass.managers) {
    CAPTURE(m.label);
    CHECK(m.variant == (m.label == "camera"));
    if (m.label == "camera") {
      CHECK(m.rva == slot);
      CHECK(m.sites == variant.statics[0].sites);
      CHECK(m.witnesses == variant.statics[0].witnesses);
    }
  }
  // In the draft table it is an accessor's static like the others: two recipes agree, and its witnesses stand.
  lodestar::Found f = r.found;
  f.accessor = accessor;
  f.instances = pass;
  const TableProvenance provenance{"0.1.0-test.1", std::string(64, 'a'), "l5", "2026-01-02", {}};
  const lodestar::DraftTable draft = lodestar::draft_table(f, r.ids, provenance, r.gated.image, w.sys->image());
  const std::size_t at = draft.text.find("[item.camera]");
  REQUIRE(at != std::string::npos);
  const std::string entry = draft.text.substr(at, draft.text.find("[item.", at + 5) - at);
  CHECK(entry.find("found_by = [\"accessor\", \"instance_class\"]") != std::string::npos);
  CHECK(entry.find("witnesses = [") != std::string::npos);
  const auto text = lodestar::write_report(f);
  REQUIRE(text.has_value());
  CHECK(text->find("\"variant\": true") != std::string::npos);
}

TEST_CASE("in game: a lead with no class of its name is named by a class whose name holds it, and marked") {
  FakeWorld w;
  Round r = round_of(w);
  // The clock's lead by a shorter name than its class has ("Clo" for Clock): no class of that name, with or without
  // Imp, and one whose name holds it. The session's lead keeps its own class, though "Lobby" holds other names too.
  std::string leads_text(kLeads);
  const std::size_t at = leads_text.find("\"Clock\"");
  REQUIRE(at != std::string::npos);
  leads_text.replace(at, 7, "\"Cloc\"");
  auto leads = derive::parse_leads(leads_text);
  REQUIRE(leads.has_value());
  const lodestar::Namer namer(r.gated.image, w.sys->image(), *leads, r.ids);
  const auto classes = namer.lead_classes();
  const auto clock_classes = std::find_if(classes.begin(), classes.end(), [](const auto& c) { return c.label == "world_clock"; });
  REQUIRE(clock_classes != classes.end());
  CHECK(clock_classes->named == 0);
  CHECK(clock_classes->named_imp == 0);
  CHECK(clock_classes->vtables == 0);  // its own tables of methods: none
  CHECK(clock_classes->related == 1);
  const lodestar::InstancesFound pass = namer.pass(w.sys->image(), r.found.accessor, reader_of(w));
  CHECK(pass.named == 11);
  for (const lodestar::ManagerFound& m : pass.managers) {
    CAPTURE(m.label);
    CHECK(m.by_related == (m.label == "world_clock"));
    if (m.label == "world_clock") {
      CHECK(m.status == Status::Derived);
      CHECK(m.rva == w.at("st_world_clock") - w.base);
      CHECK(m.vtable == w.vtable("clock") - w.base);
      CHECK_FALSE(m.imp);
      CHECK_FALSE(m.by_base);
    }
  }
  // What a search looks for, for such a lead: the related class's table of methods.
  const std::uint32_t clock = roles::info(Item::WorldClock).id;
  CHECK(namer.vtables(clock) == std::vector<std::uint32_t>{static_cast<std::uint32_t>(w.vtable("clock") - w.base)});
  // Every class's primary table of methods: ascending, and the clock's among them.
  const std::vector<std::uint32_t> primary = namer.primary_tables();
  CHECK(std::is_sorted(primary.begin(), primary.end()));
  CHECK(std::binary_search(primary.begin(), primary.end(), static_cast<std::uint32_t>(w.vtable("clock") - w.base)));
  lodestar::Found f = r.found;
  f.instances = pass;
  const auto text = lodestar::write_report(f);
  REQUIRE(text.has_value());
  CHECK(text->find("\"by_related\": true") != std::string::npos);
}

TEST_CASE("in game: what a static only a variant reaches holds is said, whatever it is") {
  FakeWorld w;
  Round r = round_of(w);
  lodestar::AccessorFound accessor = r.found.accessor;
  lodestar::VariantFound variant;
  variant.label = "accessor_gap";
  const auto slot = [&](const char* label) { return static_cast<std::uint32_t>(w.at(label) - w.base); };
  // Four statics that are none of the accessor's here: the save's (a class no lead asks for), the game state's
  // (emptied), the field area's (its object's first word made no table of methods), and the module's handle.
  w.mem.put<std::uint64_t>(w.at("st_game_state"), 0);
  w.mem.put<std::uint64_t>(w.instance("field_area"), 0x1234);
  for (const char* label : {"st_save_data", "st_game_state", "st_field_area"}) {
    variant.statics.push_back(lodestar::StaticFound{slot(label), 0, 1, {}});
  }
  accessor.variants.push_back(variant);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  const lodestar::InstancesFound pass = namer.pass(w.sys->image(), accessor, reader_of(w));
  REQUIRE(pass.new_statics.size() == 3);
  CHECK(pass.new_statics[0].rva == slot("st_save_data"));
  CHECK(pass.new_statics[0].holds == lodestar::Holds::Classed);
  CHECK(pass.new_statics[0].vtable == w.vtable("save") - w.base);
  CHECK_FALSE(pass.new_statics[0].lead);
  CHECK(pass.new_statics[1].holds == lodestar::Holds::Nothing);
  CHECK(pass.new_statics[2].holds == lodestar::Holds::Unclassed);
  lodestar::Found f = r.found;
  f.accessor = accessor;
  f.instances = pass;
  const auto text = lodestar::write_report(f);
  REQUIRE(text.has_value());
  CHECK(text->find("\"holds\": \"classed\"") != std::string::npos);
  CHECK(text->find("\"holds\": \"unclassed\"") != std::string::npos);
}

TEST_CASE("in game: the code's references to a slot, and a manager the search named taking the slot the code uses") {
  FakeWorld w;
  Round r = round_of(w);
  // The accessor's own count of a static's sites is a count of references to it: the reference count finds those
  // places, each an instruction that refers to the slot, and no fewer.
  const lodestar::StaticFound& camera = *std::find_if(
      r.found.accessor.statics.begin(), r.found.accessor.statics.end(),
      [&](const lodestar::StaticFound& s) { return s.rva == w.at("st_camera") - w.base; });
  const std::uint32_t mirror = 0x10;  // a place nothing refers to
  const std::vector<std::uint32_t> slots = {mirror, camera.rva};
  std::vector<lodestar::References> refs = lodestar::references(r.gated.image, w.sys->image(), ".text", slots, 64);
  REQUIRE(refs.size() == 2);
  CHECK(refs[1].places.size() == refs[1].count);
  CHECK(refs[0].count == 0);
  CHECK(refs[0].places.empty());
  CHECK(refs[1].count >= camera.sites);
  CHECK(std::is_sorted(refs[1].places.begin(), refs[1].places.end()));
  for (const std::uint32_t site : camera.witnesses) {
    CAPTURE(site);
    std::string listed;
    for (const std::uint32_t at : refs[1].places) listed += std::to_string(at) + " ";
    CAPTURE(listed);
    CHECK(std::find(refs[1].places.begin(), refs[1].places.end(), site) != refs[1].places.end());
  }
  // The first `max` are kept; all are counted.
  const std::vector<lodestar::References> few = lodestar::references(r.gated.image, w.sys->image(), ".text", slots, 1);
  CHECK(few[1].count == refs[1].count);
  CHECK(few[1].places == std::vector<std::uint32_t>{refs[1].places.front()});
  if (refs[1].places.size() > 8) refs[1].places.resize(8);
  // Each place passes the resolver's own check of a witness.
  ItemEntry e;
  e.label = "camera";
  e.id = roles::info(Item::Camera).id;
  e.kind = IdKind::Static;
  e.rva = camera.rva;
  e.found_by = {Recipe::InstanceClass};
  e.witnesses = refs[1].places;
  CHECK(check_item(r.gated.image, w.sys->image(), e).ok());
  CHECK(lodestar::references(r.gated.image, w.sys->image(), ".text", {}, 8).empty());
  CHECK(lodestar::references(r.gated.image, w.sys->image(), ".none", slots, 8)[1].count == 0);

  // A manager the search named behind two slots with one object: the slot the code refers to is the manager's,
  // though the other comes first by offset, and its references are its sites and witnesses.
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  lodestar::InstancesFound held;
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  lodestar::SlotSearchResult result;
  result.slots = {mirror, camera.rva};
  result.objects = 1;
  result.vtable = 0x3000;
  lodestar::set_by_search(held, e.id, result);
  const auto of = [&] {
    return *std::find_if(held.managers.begin(), held.managers.end(), [&](const auto& m) { return m.id == e.id; });
  };
  CHECK(of().rva == mirror);
  lodestar::set_search_refs(held, e.id, {static_cast<std::uint32_t>(refs[0].count), static_cast<std::uint32_t>(refs[1].count)},
                            {refs[0].places, refs[1].places});
  CHECK(of().rva == camera.rva);
  CHECK(of().sites == refs[1].count);
  CHECK(of().witnesses == refs[1].places);
  CHECK(of().by_search);
  CHECK(of().search_refs == std::vector<std::uint32_t>{0, static_cast<std::uint32_t>(refs[1].count)});
  // Through a later pass, and into the draft table: by its instance's class, with witnesses that stand.
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  CHECK(of().rva == camera.rva);
  CHECK(of().search_refs.size() == 2);
  r.found.instances = held;
  const TableProvenance provenance{"0.1.0-test.1", std::string(64, 'a'), "l5", "2026-01-02", {}};
  const lodestar::DraftTable draft = lodestar::draft_table(r.found, r.ids, provenance, r.gated.image, w.sys->image());
  const std::size_t at = draft.text.find("[item.camera]");
  REQUIRE(at != std::string::npos);
  const std::string entry = draft.text.substr(at, draft.text.find("[item.", at + 5) - at);
  CHECK(entry.find("found_by = [\"instance_class\"]") != std::string::npos);
  CHECK(entry.find("witnesses = [") != std::string::npos);
  CHECK(entry.find("\"witness\"") != std::string::npos);
  CHECK(draft.witnesses_dropped == 0);
  // Counts that do not go with the slots change nothing.
  lodestar::set_search_refs(held, e.id, {1}, {{}});
  CHECK(of().search_refs.size() == 2);
}

TEST_CASE("in game: the watch follows a way to an object and counts how its words behave") {
  FakeWorld w;
  Round r = round_of(w);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  const auto rva = [&](std::uint64_t va) { return static_cast<std::uint32_t>(va - w.base); };
  std::uint64_t asked = 0;
  lodestar::Watch watch(w.base, r.gated.image.size_of_image, namer.primary_tables(), [&](std::uint64_t at) {
    ++asked;
    std::array<std::byte, 8> b{};
    return w.mem.read(at, b);
  });
  // The session's object (0x20 bytes): a counter that rises at 0x08, one that falls at 0x0C, a small number at 0x10,
  // and at 0x18 the address of the messages' object. Two ways to the messages' object: its own static, and the
  // pointer in the session's object.
  const std::uint64_t session = w.instance("session"), words = w.instance("messages");
  w.mem.put<std::uint32_t>(session + 0x08, 100);
  w.mem.put<std::uint32_t>(session + 0x0C, 5000);
  w.mem.put<std::uint64_t>(session + 0x10, 7);
  w.mem.put<std::uint64_t>(session + 0x18, words);
  using Way = lodestar::Watch::Way;
  REQUIRE(watch.add(Way{"session", 1007, rva(w.at("st_session")), 0, false, rva(w.vtable("lobby")), false}));
  REQUIRE(watch.add(Way{"messages", 1015, rva(w.at("st_messages")), 0, false, rva(w.vtable("words")), false}));
  REQUIRE(watch.add(Way{"messages", 1015, rva(w.at("st_session")), 0x18, true, rva(w.vtable("words")), false}));
  CHECK_FALSE(watch.add(Way{"again", 0, rva(w.at("st_session")), 0x18, true, 0, false}));  // the same way
  CHECK(watch.ways() == 3);
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t now = 1'000'000;
  const auto second = [&] {
    now += 1'000'000;
    bool ended = false;
    for (int k = 0; k < 100 && !ended; ++k) {
      reader.begin_tick();
      ended = watch.step(gt, reader, 2, now);  // two ways a step: a pass takes two
    }
    REQUIRE(ended);
  };
  second();
  for (int i = 0; i < 20; ++i) {
    w.mem.put<std::uint32_t>(session + 0x08, 100 + 3 * (i + 1));
    w.mem.put<std::uint32_t>(session + 0x0C, 5000 - (i + 1));
    second();
  }
  // Nothing is read twice within a second.
  reader.begin_tick();
  CHECK_FALSE(watch.step(gt, reader, 8, now + 1000));
  lodestar::WatchFound found = watch.found();
  CHECK(found.passes == 21);
  REQUIRE(found.objects.size() == 3);
  const lodestar::WatchObject& s = found.objects[0];
  CHECK(s.label == "session");
  CHECK(s.id == 1007);
  CHECK(s.readings == 21);
  CHECK(s.empty == 0);
  CHECK(s.unreadable == 0);
  CHECK(s.moved == 0);
  CHECK(s.vtable_same == 21);
  CHECK(s.span == 0x20);  // the object's memory ends there
  CHECK(s.vtable == rva(w.vtable("lobby")));
  CHECK(s.bits.empty());
  const auto word = [](const lodestar::WatchObject& o, std::uint32_t offset) {
    const auto it = std::find_if(o.words.begin(), o.words.end(), [&](const auto& x) { return x.offset == offset; });
    REQUIRE(it != o.words.end());
    return *it;
  };
  CHECK(word(s, 0).kind == lodestar::WordKind::Vtable);
  CHECK(word(s, 0).changes_low == 0);
  CHECK(word(s, 8).changes_low == 20);
  CHECK(word(s, 8).rises_low == 20);
  CHECK(word(s, 8).changes_high == 20);  // the falling counter is the word's high half
  CHECK(word(s, 8).rises_high == 0);
  CHECK(word(s, 0x10).kind == lodestar::WordKind::Small);
  CHECK(word(s, 0x18).kind == lodestar::WordKind::Heap);
  CHECK(word(s, 0x18).flips == 0);
  CHECK(asked > 0);
  // Both ways to the messages' object arrive at the one object, every time.
  const lodestar::WatchObject& direct = found.objects[1];
  const lodestar::WatchObject& through = found.objects[2];
  CHECK(direct.same_address == 0);  // the first way of its label has none before it
  CHECK(through.deref);
  CHECK(through.offset == 0x18);
  CHECK(through.readings == 21);
  CHECK(through.same_address == 21);
  CHECK(through.vtable_same == 21);

  // The session's static emptied: nothing there. A new object behind it: moved, and compared with nothing.
  w.mem.put<std::uint64_t>(w.at("st_session"), 0);
  second();
  const std::uint64_t other = w.mem.alloc(0x20);
  w.set_vtable(other, "lobby");
  w.mem.put<std::uint32_t>(other + 0x08, 1);
  w.mem.put<std::uint64_t>(w.at("st_session"), other);
  second();
  second();
  found = watch.found();
  CHECK(found.objects[0].readings == 23);
  CHECK(found.objects[0].empty == 1);
  CHECK(found.objects[0].moved == 0);  // after an empty reading a new object is simply the first again
  CHECK(word(found.objects[0], 8).changes_low == 20);  // the other object's counter was not compared with the old one's
  CHECK(found.objects[2].empty == 3);  // the way through the session's object: no object, then one that keeps no
  CHECK(found.objects[2].unreadable == 0);  // address at 0x18
  // One object swapped for another between two readings: moved.
  w.mem.put<std::uint64_t>(w.at("st_messages"), w.instance("hud"));
  second();
  CHECK(watch.found().objects[1].moved == 1);
  // The report carries a watch as counts and fixed words, and lints clean.
  lodestar::Found f = r.found;
  f.watch = watch.found();
  const auto text = lodestar::write_report(f);
  if (!text) MESSAGE(report::describe(text.error()));
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"kind\": \"heap\"") != std::string::npos);
  CHECK(text->find("\"same_address\": 21") != std::string::npos);
}

TEST_CASE("in game: the watch knows a clock packed into bits, and one kept as a fraction") {
  FakeWorld w;
  Round r = round_of(w);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  lodestar::Watch watch(w.base, r.gated.image.size_of_image, namer.primary_tables(), nullptr);
  // The clock's object (0x20 bytes). At 0x10 one word packs the time: seconds in bits 0 to 5, minutes in 6 to 11,
  // hours in 12 to 16, the day above. At 0x18 the hour of the day as a fraction. A game minute a second; two and a
  // half hours go by.
  const std::uint64_t clock = w.instance("world_clock");
  const auto rva = [&](std::uint64_t va) { return static_cast<std::uint32_t>(va - w.base); };
  REQUIRE(watch.add(lodestar::Watch::Way{"world_clock", 1012, rva(w.at("st_world_clock")), 0, false, 0, true}));
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint32_t hour = 9, minute = 30;
  std::uint64_t now = 5'000'000;
  for (int second = 0; second < 150; ++second) {
    const std::uint64_t packed = std::uint64_t{17} | (std::uint64_t{minute} << 6) | (std::uint64_t{hour} << 12) |
                                 (std::uint64_t{3} << 17);
    w.mem.put<std::uint64_t>(clock + 0x10, packed);
    w.mem.put<float>(clock + 0x18, static_cast<float>(hour) + static_cast<float>(minute) / 60.0f);
    now += 1'000'000;
    reader.begin_tick();
    (void)watch.step(gt, reader, 4, now);
    minute = (minute + 1) % 60;
    if (minute == 0) hour = (hour + 1) % 24;
  }
  const lodestar::WatchFound found = watch.found();
  REQUIRE(found.objects.size() == 1);
  const lodestar::WatchObject& c = found.objects[0];
  CHECK(c.clock);
  CHECK(c.readings == 150);
  REQUIRE_FALSE(c.bits.empty());
  const lodestar::BitHit& best = c.bits[0];
  CHECK(best.offset == 0x10);
  CHECK(best.bit == 6);
  CHECK(best.ticks == 149);
  CHECK(best.wraps == 2);
  CHECK(best.jumps == 0);
  CHECK(best.has_hour);
  CHECK(best.hour_offset == 0x10);
  CHECK(best.hour_bit == 12);
  CHECK(best.hour_steps == 2);
  // Other runs of bits step and go round too (parts of the fraction beside it, which a review tells apart by where
  // they lie); in the packed word itself none has hours beside it and as many steps.
  for (std::size_t i = 1; i < c.bits.size(); ++i) {
    CHECK((c.bits[i].offset != 0x10 || !c.bits[i].has_hour || c.bits[i].ticks < best.ticks));
  }
  // The fraction: it rose at all but the two readings where the hour's digits carried... and never fell far: a
  // steady rise of a sixtieth of an hour a second, about 16 thousandths.
  const auto fraction = std::find_if(c.rises.begin(), c.rises.end(), [](const auto& h) { return h.offset == 0x18; });
  REQUIRE(fraction != c.rises.end());
  CHECK_FALSE(fraction->whole);
  CHECK(fraction->rises == 149);
  CHECK(fraction->jumps == 0);
  CHECK(fraction->rate_micro >= 15'000);
  CHECK(fraction->rate_micro <= 18'000);
  // The fraction never jumped: time was not let pass. (The packed word, read as a whole number, rises by 64 a
  // minute and by 320 when the hour carries: two such carries count as its jumps.)
  CHECK(std::none_of(c.jumps.begin(), c.jumps.end(), [](const auto& e) { return e.offset == 0x18; }));
  CHECK(std::all_of(c.jumps.begin(), c.jumps.end(), [](const auto& e) { return e.offset == 0x10; }));
}

TEST_CASE("in game: the counts and what each static held are the fullest pass's, not the last one's") {
  FakeWorld w;
  Round r = round_of(w);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  lodestar::InstancesFound held;
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  CHECK(held.null == 0);
  CHECK(held.classed == 11);
  // What every static held: one entry a static, each classed, each a class a lead asks for.
  REQUIRE(held.held.size() == 11);
  CHECK(std::all_of(held.held.begin(), held.held.end(),
                    [](const auto& x) { return x.holds == lodestar::Holds::Classed && x.lead && x.vtable != 0; }));
  // The game closes: every static empty at the last pass. The round keeps what it saw in play.
  std::vector<std::pair<std::uint64_t, std::uint64_t>> kept;
  for (const lodestar::StaticFound& st : r.found.accessor.statics) {
    kept.emplace_back(w.base + st.rva, w.mem.get<std::uint64_t>(w.base + st.rva));
    w.mem.put<std::uint64_t>(w.base + st.rva, 0);
  }
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  CHECK(held.passes == 2);
  CHECK(held.null == 0);
  CHECK(held.classed == 11);
  CHECK(held.named == 11);
  CHECK(held.held.size() == 11);
  CHECK(held.held[0].holds == lodestar::Holds::Classed);
  // A pass as full as the fullest replaces it: the latest of equals.
  for (const auto& [at, value] : kept) w.mem.put<std::uint64_t>(at, value);
  w.mem.put<std::uint64_t>(w.instance("hud"), 0x1234);  // one object's first word is no table of methods now
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  CHECK(held.null == 0);
  CHECK(held.classed == 10);
  CHECK(held.unclassed == 1);
  CHECK(std::count_if(held.held.begin(), held.held.end(),
                      [](const auto& x) { return x.holds == lodestar::Holds::Unclassed; }) == 1);
  CHECK(held.named == 11);  // what was named stays named
  lodestar::Found f = r.found;
  f.instances = held;
  const auto text = lodestar::write_report(f);
  REQUIRE(text.has_value());
  CHECK(text->find("\"held_statics\": [") != std::string::npos);
}

TEST_CASE("in game: a way may ask for a longer span and a slower pace, and wide ways are listed only with a find") {
  FakeWorld w;
  Round r = round_of(w);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  lodestar::Watch watch(w.base, r.gated.image.size_of_image, namer.primary_tables(), nullptr);
  const auto rva = [&](std::uint64_t va) { return static_cast<std::uint32_t>(va - w.base); };
  using Way = lodestar::Watch::Way;
  // A large object behind the HUD's static, with a packed clock 0x1000 bytes in; watched over its first 0x2000 bytes.
  const std::uint64_t big = w.mem.alloc(0x2000);
  w.set_vtable(big, "hud");
  w.mem.put<std::uint64_t>(w.at("st_hud"), big);
  Way deep{"hud", 1009, rva(w.at("st_hud")), 0, false, 0, true};
  deep.span = 0x2000;
  REQUIRE(watch.add(deep));
  // Every static's object as a wide way, followed every three seconds: the world clock's holds a clock, the
  // rest nothing that behaves as one.
  std::size_t wide = 0;
  for (const lodestar::StaticFound& st : r.found.accessor.statics) {
    Way way{"static", 0, st.rva, 0, false, 0, true};
    way.wide = true;
    way.every_us = 3'000'000;
    wide += watch.add(way) ? 1 : 0;
  }
  CHECK(wide == 11);  // the HUD's static too: a wide way and a plain one to the same object stand side by side
  const std::uint64_t clock = w.instance("world_clock");
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint32_t hour = 9, minute = 30;
  std::uint64_t now = 5'000'000;
  for (int second = 0; second < 450; ++second) {
    const std::uint64_t packed = (std::uint64_t{minute} << 6) | (std::uint64_t{hour} << 12);
    w.mem.put<std::uint64_t>(big + 0x1000, packed);
    w.mem.put<std::uint64_t>(clock + 0x10, packed);
    now += 1'000'000;
    for (int k = 0; k < 8; ++k) {  // one way a call, as the round drives it; eight calls a second
      reader.begin_tick();
      (void)watch.step(gt, reader, 1, now);
    }
    if (second % 3 == 2) {  // a game minute every three seconds
      minute = (minute + 1) % 60;
      if (minute == 0) hour = (hour + 1) % 24;
    }
  }
  const lodestar::WatchFound found = watch.found();
  CHECK(found.wide_ways == 11);
  // Listed: the deep way, and of the eleven wide ones only the world clock's (the HUD's wide way sees 512 bytes, and
  // its clock lies further in).
  REQUIRE(found.objects.size() == 2);
  const lodestar::WatchObject& d = found.objects[0];
  CHECK(d.label == "hud");
  CHECK(d.span == 0x2000);
  CHECK(d.readings == 450);
  CHECK_FALSE(d.wide);
  REQUIRE_FALSE(d.bits.empty());
  CHECK(d.bits[0].offset == 0x1000);
  CHECK(d.bits[0].bit == 6);
  CHECK(d.bits[0].wraps == 2);
  CHECK(d.bits[0].has_hour);
  CHECK(d.bits[0].hour_bit == 12);
  // Its words: the first 512 bytes' as ever (those that were once not zero), past them only the ones that changed.
  REQUIRE_FALSE(d.words.empty());
  CHECK(d.words[0].offset == 0);
  CHECK(std::count_if(d.words.begin(), d.words.end(), [](const auto& x) { return x.offset >= 0x200; }) == 1);
  CHECK(d.words.back().offset == 0x1000);
  CHECK(d.words.back().changes_low >= 100);
  // The ten statics whose objects never changed were followed more seldom after their first comparisons: far fewer
  // copies than every way at its own pace would have taken (900 the deep way's, 300 the clock's, 3,000 the rest's).
  CHECK(reader.total_copies() < 2'200);
  const lodestar::WatchObject& c = found.objects[1];
  CHECK(c.wide);
  CHECK(c.rva == rva(w.at("st_world_clock")));
  CHECK(c.readings == 150);  // every three seconds
  CHECK(c.words.empty());    // a wide way's words are not written
  REQUIRE_FALSE(c.bits.empty());
  CHECK(c.bits[0].offset == 0x10);
  CHECK(c.bits[0].bit == 6);
  CHECK(c.bits[0].wraps == 2);
  CHECK(c.bits.size() <= 4);
  // The report: a word's counts of nothing are left out, and it lints clean.
  lodestar::Found f = r.found;
  f.watch = found;
  const auto text = lodestar::write_report(f);
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"wide_ways\": 11") != std::string::npos);
  CHECK(text->find("\"flips\": 0") == std::string::npos);
  CHECK(text->find("\"changes_low\": 0") == std::string::npos);
}

TEST_CASE("in game: the watch tells a clock by its jumps when time is let pass, however fast its minutes go") {
  FakeWorld w;
  Round r = round_of(w);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  lodestar::Watch watch(w.base, r.gated.image.size_of_image, namer.primary_tables(), nullptr);
  const auto rva = [&](std::uint64_t va) { return static_cast<std::uint32_t>(va - w.base); };
  // An object of 0x40 bytes behind the HUD's static. At 0x10 the time packed (seconds, minutes from bit 6, hours
  // from bit 12); at 0x18 the hour of the day as a fraction; at 0x20 the second of the day as a whole number; at
  // 0x28 a counter that only counts. Ten game minutes a second: the minutes go too fast to be known as minutes.
  // Time is let pass twice: five hours at the 40th second, three at the 80th; midnight comes ten seconds later.
  const std::uint64_t obj = w.mem.alloc(0x40);
  w.mem.put<std::uint64_t>(w.at("st_hud"), obj);
  lodestar::Watch::Way way{"holder", 0, rva(w.at("st_hud")), 0, false, 0, true};
  REQUIRE(watch.add(way));
  // The same object as a wide way: a wide way is listed only for minutes that go round with hours beside them, and
  // these minutes go too fast for that.
  lodestar::Watch::Way wide = way;
  wide.label = "static";
  wide.wide = true;
  REQUIRE(watch.add(wide));
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t day_s = 3600;  // one o'clock
  std::uint64_t now = 5'000'000;
  for (int second = 0; second < 120; ++second) {
    if (second == 40) day_s += 5 * 3600;
    if (second == 80) day_s += 3 * 3600;
    day_s %= 86400;
    const std::uint64_t packed = (day_s % 60) | ((day_s / 60 % 60) << 6) | ((day_s / 3600) << 12);
    w.mem.put<std::uint64_t>(obj + 0x10, packed);
    w.mem.put<float>(obj + 0x18, static_cast<float>(day_s) / 3600.0f);
    w.mem.put<std::uint32_t>(obj + 0x20, static_cast<std::uint32_t>(day_s));
    w.mem.put<std::uint32_t>(obj + 0x28, static_cast<std::uint32_t>(second * 7));
    now += 1'000'000;
    reader.begin_tick();
    (void)watch.step(gt, reader, 2, now);
    day_s += 600;
  }
  const lodestar::WatchFound found = watch.found();
  CHECK(found.wide_ways == 1);
  REQUIRE(found.objects.size() == 1);  // the plain way; the wide one found no clock by its minutes
  const lodestar::WatchObject& c = found.objects[0];
  CHECK_FALSE(c.wide);
  CHECK(c.address == obj);
  CHECK(c.readings == 120);
  // No run of bits passed as minutes with hours beside it.
  CHECK(std::none_of(c.bits.begin(), c.bits.end(), [](const auto& h) { return h.wraps != 0 && h.has_hour; }));
  // The fraction and the whole number: both rose at their pace and jumped three times (twice when time was let
  // pass, once at midnight, a fall); so did the packed word, read as a whole number. The counter never jumped: it
  // comes last. The fraction comes first.
  REQUIRE(c.rises.size() == 4);
  CHECK(c.rises[3].offset == 0x28);
  CHECK(c.rises[3].jumps == 0);
  CHECK(c.rises[0].offset == 0x18);
  CHECK_FALSE(c.rises[0].whole);
  CHECK(c.rises[0].jumps == 3);
  CHECK(c.rises[0].rises == 116);
  CHECK(c.rises[0].rate_micro >= 160'000);
  CHECK(c.rises[0].rate_micro <= 173'000);
  CHECK(c.rises[1].offset == 0x10);
  CHECK(c.rises[1].whole);
  CHECK(c.rises[2].offset == 0x20);
  CHECK(c.rises[2].whole);
  CHECK(c.rises[2].jumps == 3);
  CHECK(c.rises[2].rate_micro >= 590'000'000);
  CHECK(c.rises[2].rate_micro <= 610'000'000);
  // The hours, standing alone: the run at bit 12 of the packed word stepped by one 19 times and changed otherwise
  // twice.
  REQUIRE_FALSE(c.hours.empty());
  CHECK(c.hours[0].offset == 0x10);
  CHECK(c.hours[0].bit == 12);
  CHECK(c.hours[0].steps >= 18);
  CHECK(c.hours[0].jumps == 2);
  // The jumps, in the order they happened: each field's at the 40th and the 80th second, from what to what.
  const auto jump = [&](lodestar::JumpAs as, std::uint32_t offset, std::uint32_t second) {
    const auto at = std::find_if(c.jumps.begin(), c.jumps.end(), [&](const lodestar::JumpEvent& e) {
      return e.as == as && e.offset == offset && e.second == second;
    });
    return at == c.jumps.end() ? lodestar::JumpEvent{} : *at;
  };
  CHECK(std::is_sorted(c.jumps.begin(), c.jumps.end(), [](const auto& a, const auto& b) { return a.second < b.second; }));
  const lodestar::JumpEvent f40 = jump(lodestar::JumpAs::Fraction, 0x18, 40);
  CHECK(f40.to >= f40.from + 5'000);  // five hours and a sixth, in thousandths
  CHECK(f40.to <= f40.from + 5'400);
  const lodestar::JumpEvent w80 = jump(lodestar::JumpAs::Whole, 0x20, 80);
  CHECK(w80.to == w80.from + 3 * 3600 + 600);
  const lodestar::JumpEvent h40 = jump(lodestar::JumpAs::Hour, 0x10, 40);
  CHECK(h40.bit == 12);
  CHECK(h40.to == (h40.from + 5) % 24);
  // The report carries them, and lints clean.
  lodestar::Found f = r.found;
  f.watch = found;
  const auto text = lodestar::write_report(f);
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"jump_events\": [") != std::string::npos);
  CHECK(text->find("\"as\": \"hour\"") != std::string::npos);
  CHECK(text->find("\"rate_micro\": ") != std::string::npos);
}

TEST_CASE("in game: a way may go one step deeper, and the sweep finds the bits that flip once") {
  FakeWorld w;
  Round r = round_of(w);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  const auto rva = [&](std::uint64_t va) { return static_cast<std::uint32_t>(va - w.base); };
  // The store behind the HUD's static keeps, 0x20 bytes in, the address of a stretch of 40,000 bytes (the blocks);
  // the stretch keeps, 0x10 bytes in, the address of a small object.
  const std::uint64_t store = w.mem.alloc(0x40);
  const std::uint64_t blocks = w.mem.alloc(40'000);
  const std::uint64_t small = w.mem.alloc(0x80);
  w.mem.put<std::uint64_t>(w.at("st_hud"), store);
  w.mem.put<std::uint64_t>(store + 0x20, blocks);
  w.mem.put<std::uint64_t>(blocks + 0x10, small);
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();

  // The watch's deeper way: the static, 0x20 bytes in, the address kept there, 0x10 bytes in, the address kept there.
  lodestar::Watch watch(w.base, r.gated.image.size_of_image, namer.primary_tables(), nullptr);
  lodestar::Watch::Way deeper{"event_flags", 1008, rva(w.at("st_hud")), 0x20, true, 0, false};
  deeper.deeper = true;
  deeper.via = 0x10;
  REQUIRE(watch.add(deeper));
  CHECK_FALSE(watch.add(deeper));
  lodestar::Watch::Way other = deeper;
  other.via = 0x18;  // another way: nothing is kept there
  REQUIRE(watch.add(other));

  lodestar::Sweep sweep(w.base);
  REQUIRE(sweep.add(lodestar::Sweep::Target{"event_flags", 1008, rva(w.at("st_hud")), 0x20, false, 0, 4}));
  CHECK_FALSE(sweep.add(lodestar::Sweep::Target{"event_flags", 1008, rva(w.at("st_hud")), 0x20, false, 0, 8}));
  REQUIRE(sweep.add(lodestar::Sweep::Target{"event_flags", 1008, rva(w.at("st_hud")), 0x20, true, 0x10, 1}));
  CHECK(sweep.targets() == 2);

  std::uint64_t now = 1'000'000;
  std::uint64_t passes = 0;
  std::uint32_t counter = 0;
  for (int second = 0; second < 200; ++second) {
    // A flag is set at the 70th second, 30,001 bytes in (bit 5), and cleared at the 150th; another 133 bytes in at
    // the 110th (bit 0). A counter 0x400 bytes in changes all the time; so does a word of the small object.
    if (second == 70) w.mem.put<std::uint8_t>(blocks + 30'001, 0x20);
    if (second == 150) w.mem.put<std::uint8_t>(blocks + 30'001, 0x00);
    if (second == 110) w.mem.put<std::uint8_t>(blocks + 133, 0x01);
    w.mem.put<std::uint32_t>(blocks + 0x400, ++counter);
    w.mem.put<std::uint32_t>(small + 0x40, counter * 3);
    now += 1'000'000;
    // The tester marks a task done two seconds after the first flag was set, and one second before the first flag
    // is cleared.
    if (second == 72 || second == 149) sweep.mark(now);
    reader.begin_tick();
    (void)watch.step(gt, reader, 1, now);
    for (int k = 0; k < 6; ++k) passes += sweep.step(gt, reader, now) ? 1 : 0;
  }
  const lodestar::WatchFound seen = watch.found();
  REQUIRE(seen.objects.size() == 2);
  CHECK(seen.objects[0].deeper);
  CHECK(seen.objects[0].via == 0x10);
  CHECK(seen.objects[0].readings >= 90);
  CHECK(seen.objects[0].span == 0x80);  // the small object's end is nearer than 512 bytes
  CHECK(std::any_of(seen.objects[0].words.begin(), seen.objects[0].words.end(),
                    [](const auto& x) { return x.offset == 0x40 && x.changes_low >= 80; }));
  CHECK(seen.objects[1].readings == 0);
  CHECK(seen.objects[1].empty >= 90);

  const lodestar::SweepFound found = sweep.found();
  CHECK(found.passes == passes);
  CHECK(found.passes >= 6);  // a pass every thirty seconds, and one at once after each mark
  CHECK(found.passes <= 11);
  REQUIRE(found.mark_seconds.size() == 2);
  CHECK(found.mark_seconds[0] == 72);
  CHECK(found.mark_seconds[1] == 149);
  REQUIRE(found.targets.size() == 2);
  const lodestar::SweepTarget& t = found.targets[0];
  CHECK(t.rva == rva(w.at("st_hud")));
  CHECK(t.offset == 0x20);
  CHECK_FALSE(t.deeper);
  // How far it could be read: two whole pieces, and of the third what stands before the last page's start.
  CHECK(t.bytes >= 40'000 - 0x1000);
  CHECK(t.bytes <= 40'000);
  CHECK(t.readings == found.passes);
  CHECK(t.unreadable == found.passes);  // the piece past its end, at every pass
  CHECK(t.moved == 0);
  CHECK(t.changed_words == 3);
  CHECK(t.busy_words == 1);  // the counter
  CHECK(t.changed_pages == 2);  // the counter and one flag share the first page
  CHECK(t.quiet_pages == 2);
  // The three flips of the two flags, in order, with the second they were seen at (the pass after they happened).
  REQUIRE(t.flips.size() == 3);
  CHECK(t.flips[0].offset == 30'001);
  CHECK(t.flips[0].bit == 5);
  CHECK(t.flips[0].set);
  CHECK(t.flips[0].second >= 70);
  CHECK(t.flips[0].second <= 105);
  CHECK(t.flips[1].offset == 133);
  CHECK(t.flips[1].bit == 0);
  CHECK(t.flips[1].set);
  CHECK(t.flips[1].second >= 110);
  CHECK(t.flips[2].offset == 30'001);
  CHECK_FALSE(t.flips[2].set);
  CHECK(t.flips[2].second >= 150);
  // The flips around the marks: the first flag's setting was seen in the pass the first mark started (and the
  // second flag's, 38 s after the mark, within its minute); the first flag's clearing under the second mark.
  CHECK_FALSE(t.retired);
  REQUIRE(t.mark_flips.size() == 3);
  CHECK(t.mark_flips[0].mark == 1);
  CHECK(t.mark_flips[0].flip.offset == 30'001);
  CHECK(t.mark_flips[0].flip.bit == 5);
  CHECK(t.mark_flips[0].flip.set);
  CHECK(t.mark_flips[0].flip.second >= 72);
  CHECK(t.mark_flips[0].flip.second <= 76);  // the pass began at the mark, not at its usual time
  CHECK(t.mark_flips[1].mark == 1);
  CHECK(t.mark_flips[1].flip.offset == 133);
  CHECK(t.mark_flips[2].mark == 2);
  CHECK(t.mark_flips[2].flip.offset == 30'001);
  CHECK_FALSE(t.mark_flips[2].flip.set);
  // The deeper target: the small object, read to its end; its counter is busy, and no flip of it is listed.
  const lodestar::SweepTarget& d = found.targets[1];
  CHECK(d.deeper);
  CHECK(d.via == 0x10);
  CHECK(d.bytes == 0);  // less than a page stands there: nothing could be read whole
  CHECK(d.flips.empty());
  // The store moves its blocks: the sweep starts its copy again, and counts the move.
  const std::uint64_t again = w.mem.alloc(40'000);
  w.mem.put<std::uint64_t>(store + 0x20, again);
  for (int second = 0; second < 60; ++second) {
    now += 1'000'000;
    reader.begin_tick();
    for (int k = 0; k < 6; ++k) (void)sweep.step(gt, reader, now);
  }
  const lodestar::SweepFound later = sweep.found();
  CHECK(later.targets[0].moved == 1);
  CHECK(later.targets[0].flips.size() == 3);  // the new stretch was compared with nothing at first
  // The report carries the sweep as counts and places, and lints clean.
  lodestar::Found f = r.found;
  f.watch = seen;
  f.sweep = later;
  const auto text = lodestar::write_report(f);
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"sweep\": {") != std::string::npos);
  CHECK(text->find("\"flip_events\": [") != std::string::npos);
  CHECK(text->find("\"mark_seconds\": [72, 149]") != std::string::npos);
  CHECK(text->find("\"mark_flips\": [") != std::string::npos);
  CHECK(text->find("\"mark\": 2") != std::string::npos);
  CHECK(text->find("\"deeper\": true") != std::string::npos);
}

TEST_CASE("in game: the sweep reads no more of a target of which too much changes all the time") {
  FakeWorld w;
  Round r = round_of(w);
  const auto rva = [&](std::uint64_t va) { return static_cast<std::uint32_t>(va - w.base); };
  // Two stretches behind the HUD's static's object: one in which 2,000 words change at every look, one that keeps
  // still but for a flag.
  const std::uint64_t store = w.mem.alloc(0x40);
  const std::uint64_t busy = w.mem.alloc(0x8000);
  const std::uint64_t quiet = w.mem.alloc(0x8000);
  w.mem.put<std::uint64_t>(w.at("st_hud"), store);
  w.mem.put<std::uint64_t>(store + 0x10, busy);
  w.mem.put<std::uint64_t>(store + 0x18, quiet);
  lodestar::Sweep sweep(w.base);
  REQUIRE(sweep.add(lodestar::Sweep::Target{"event_flags", 1008, rva(w.at("st_hud")), 0x10, false, 0, 2}));
  REQUIRE(sweep.add(lodestar::Sweep::Target{"event_flags", 1008, rva(w.at("st_hud")), 0x18, false, 0, 2}));
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t now = 1'000'000;
  for (int second = 0; second < 400; ++second) {
    for (std::uint32_t i = 0; i < 2000; ++i) {
      w.mem.put<std::uint32_t>(busy + i * 8, static_cast<std::uint32_t>(second * 7) + i);
    }
    if (second == 300) w.mem.put<std::uint8_t>(quiet + 0x1234, 0x04);
    now += 1'000'000;
    reader.begin_tick();
    for (int k = 0; k < 6; ++k) (void)sweep.step(gt, reader, now);
  }
  const lodestar::SweepFound found = sweep.found();
  REQUIRE(found.targets.size() == 2);
  CHECK(found.passes >= 12);
  // The busy one: read until its busy words showed (four changes a word take five readings), then no more.
  CHECK(found.targets[0].retired);
  CHECK(found.targets[0].busy_words == 2000);
  CHECK(found.targets[0].readings >= 5);
  CHECK(found.targets[0].readings <= 6);
  CHECK(found.targets[0].flips.empty());
  CHECK(found.targets[0].mark_flips.empty());  // a target given up lists nothing
  // The quiet one: read at every pass, its flag seen.
  CHECK_FALSE(found.targets[1].retired);
  CHECK(found.targets[1].readings == found.passes);
  REQUIRE(found.targets[1].flips.size() == 1);
  CHECK(found.targets[1].flips[0].offset == 0x1234);
  CHECK(found.targets[1].flips[0].bit == 2);
  lodestar::Found f = r.found;
  f.sweep = found;
  const auto text = lodestar::write_report(f);
  REQUIRE(text.has_value());
  CHECK(text->find("\"retired\": true") != std::string::npos);
}

TEST_CASE("in game: the tree beside the flags is walked once, its key told by its order, and a flip joined to its record") {
  FakeWorld w;
  Round r = round_of(w);
  const auto rva = [&](std::uint64_t va) { return static_cast<std::uint32_t>(va - w.base); };
  // The store behind the HUD's static keeps, 0x38 bytes in, the address of the tree's head. A record is 64 bytes:
  // left, parent, right, two small bytes, then the key (the group) and a mode in one word, then the block: its index
  // in the stretch (mode 1, as the game's records keep it) or its address (mode 2). Fifteen groups, a balanced tree; the head's links are the
  // leftmost record, the root and the rightmost, and the leaves point back at the head.
  const std::uint64_t store = w.mem.alloc(0x80);
  const std::uint64_t blocks = w.mem.alloc(125 * 16);
  const std::uint64_t own = w.mem.alloc(125);
  w.mem.put<std::uint64_t>(w.at("st_hud"), store);
  const std::uint64_t head = w.mem.alloc(64);
  w.mem.put<std::uint64_t>(store + 0x38, head);
  const std::uint32_t groups[15] = {3, 7, 10, 11, 60, 61, 62, 1040, 1041, 1042, 1043, 3000, 3001, 9000, 9100};
  std::uint64_t nodes[15];
  for (std::uint64_t& n : nodes) n = w.mem.alloc(64);
  const auto build = [&](auto&& self, int lo, int hi, std::uint64_t parent) -> std::uint64_t {
    if (lo > hi) return head;
    const int mid = (lo + hi) / 2;
    const std::uint64_t at = nodes[mid];
    w.mem.put<std::uint64_t>(at + 0, self(self, lo, mid - 1, at));
    w.mem.put<std::uint64_t>(at + 8, parent);
    w.mem.put<std::uint64_t>(at + 16, self(self, mid + 1, hi, at));
    w.mem.put<std::uint16_t>(at + 24, static_cast<std::uint16_t>(mid % 2));
    w.mem.put<std::uint32_t>(at + 32, groups[mid]);
    const bool by_address = mid == 13;  // one group's block lies by itself
    w.mem.put<std::uint32_t>(at + 36, by_address ? 2u : 1u);
    // The indices are in no order of the keys' (as in the game): the record of the sixth group keeps index 5.
    w.mem.put<std::uint64_t>(at + 40, by_address ? own : static_cast<std::uint64_t>((mid * 7) % 15));
    return at;
  };
  const std::uint64_t root = build(build, 0, 14, head);
  w.mem.put<std::uint64_t>(head + 0, nodes[0]);
  w.mem.put<std::uint64_t>(head + 8, root);
  w.mem.put<std::uint64_t>(head + 16, nodes[14]);
  w.mem.put<std::uint16_t>(head + 24, 0x0101);

  lodestar::TreeRead tree(w.base, r.gated.image.size_of_image, rva(w.at("st_hud")), 0x38);
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  CHECK_FALSE(tree.done());
  CHECK_FALSE(tree.found({}).done);
  int steps = 0;
  for (; steps < 100 && !tree.done(); ++steps) {
    reader.begin_tick();
    (void)tree.step(gt, reader);
  }
  REQUIRE(tree.done());
  CHECK(steps <= 20);  // the way, sixteen records, and the step that finds nothing left
  // Before it is settled nothing is counted. Settling asks what stands behind a word that looks like an address:
  // here, whether the fake memory holds something there.
  CHECK(tree.found({}).fields.empty());
  tree.settle([&](std::uint64_t at) {
    std::array<std::byte, 8> b{};
    return w.mem.read(at, b);
  });
  CHECK(tree.settled());
  // Two flips to join: one 3 bytes into the sixth block of the stretch (a record's index, 5), one in the block that
  // lies by itself (a record's address); and one in a block no record names.
  const lodestar::TreeRead::Flip flips[3] = {{1, 0x28, 125 * 5 + 3, 4, blocks},
                                             {2, 0x30, 17, 0, own},
                                             {2, 0x28, 125 * 15 + 1, 2, blocks}};
  const lodestar::TreeFound found = tree.found(flips, 40);  // the layout says the index is kept 40 bytes in
  CHECK(found.done);
  CHECK_FALSE(found.capped);
  CHECK(found.rva == rva(w.at("st_hud")));
  CHECK(found.offset == 0x38);
  CHECK(found.records == 16);  // the head and the fifteen
  CHECK(found.off_shape == 0);
  CHECK(found.unreadable == 0);
  REQUIRE(found.fields.size() == 16);
  // The key: 32 bytes in, fifteen different numbers, and the one field the tree is ordered by, the first word left.
  CHECK(found.order == 1);
  CHECK(found.fields[8].offset == 32);
  CHECK(found.fields[8].nonzero == 15);
  CHECK(found.fields[8].distinct == 15);
  CHECK(found.fields[8].lowest == 3);
  CHECK(found.fields[8].highest == 9100);
  CHECK(found.fields[8].rising);
  CHECK(std::count_if(found.fields.begin(), found.fields.end(), [](const auto& f) { return f.rising; }) == 1);
  CHECK(found.key_field == 32);
  // The mode: two numbers. The block: an index in thirteen records (the first record's is nothing), none of them a
  // multiple of a block's size, and one address.
  CHECK(found.fields[9].distinct == 2);
  CHECK(found.fields[10].nonzero == 13);
  CHECK(found.fields[10].distinct == 13);
  CHECK(found.fields[10].multiples == 0);
  CHECK(found.fields[10].addresses == 1);
  CHECK(found.fields[0].addresses == 16);
  // The joins: the first flip's block by its index, the second's by its address, the third's by nothing. For each,
  // the reader's way (from the root down by the key) arrives at the same record.
  REQUIRE(found.joins.size() == 2);
  CHECK(found.joins[0].mark == 1);
  CHECK(found.joins[0].target == 0x28);
  CHECK(found.joins[0].flip == 125 * 5 + 3);
  CHECK(found.joins[0].bit == 4);
  CHECK(found.joins[0].match == lodestar::TreeMatch::Index);
  CHECK(found.joins[0].field == 40);
  REQUIRE(found.joins[0].values.size() == 16);
  CHECK(found.joins[0].values[8] == 61);   // the group
  CHECK(found.joins[0].values[9] == 1);    // its mode
  CHECK(found.joins[0].values[10] == 5);   // its block's index
  CHECK(found.joins[0].reached);
  CHECK(found.joins[0].depth >= 1);
  CHECK(found.joins[0].depth <= 4);
  CHECK(found.joins[0].values[0] == 0);    // an address is never written
  CHECK(found.joins[0].values[2] == 0);
  CHECK(found.joins[1].mark == 2);
  CHECK(found.joins[1].match == lodestar::TreeMatch::Address);
  CHECK(found.joins[1].field == 40);
  CHECK(found.joins[1].values[8] == 9000);
  CHECK(found.joins[1].values[9] == 2);
  CHECK(found.joins[1].values[10] == 0);   // the block's address is not written either
  CHECK(found.joins[1].reached);
  // The report carries the tree as numbers, and lints clean.
  lodestar::Found f = r.found;
  f.tree = found;
  const auto text = lodestar::write_report(f);
  if (!text.has_value()) MESSAGE(report::describe(text.error()));
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"tree\": {") != std::string::npos);
  CHECK(text->find("\"match\": \"address\"") != std::string::npos);
  CHECK(text->find("\"ordered\": true") != std::string::npos);
  // A way that leads nowhere ends at once, with nothing read.
  lodestar::TreeRead none(w.base, r.gated.image.size_of_image, rva(w.at("st_hud")), 0x60);
  reader.begin_tick();
  CHECK(none.step(gt, reader));
  none.settle(nullptr);
  CHECK(none.found({}).records == 0);
  CHECK(none.found({}).done);
}

TEST_CASE("in game: the state search finds the field that reads -1 at the title and the same small number in the world") {
  FakeWorld w;
  Round r = round_of(w);
  const auto rva = [&](std::uint64_t va) { return static_cast<std::uint32_t>(va - w.base); };
  // Four slots of the data, each pointing at an object. The first keeps the game's state: 0x14 bytes in, -1 at the
  // title and the save's slot (2) in the world. The second has a field that is -1 always, and one that is -1 at the
  // title and another small number each time in the world. The third's field does not go back to -1. The fourth is
  // another object at the third pass.
  const std::uint64_t state = w.mem.alloc(0x200);
  const std::uint64_t other = w.mem.alloc(0x40);
  const std::uint64_t stuck = w.mem.alloc(0x40);
  const std::uint64_t moved = w.mem.alloc(0x40);
  const std::uint64_t moved_to = w.mem.alloc(0x40);
  w.mem.put<std::uint64_t>(w.at("st_hud"), state);
  w.mem.put<std::uint64_t>(w.at("st_session"), other);
  w.mem.put<std::uint64_t>(w.at("st_messages"), stuck);
  w.mem.put<std::uint64_t>(w.at("st_menus"), moved);
  const auto title = [&] {
    w.mem.put<std::int32_t>(state + 0x14, -1);
    w.mem.put<std::int32_t>(state + 0x1F0, -1);  // a second field of the same object that behaves so
    w.mem.put<std::int32_t>(other + 0x10, -1);
    w.mem.put<std::int32_t>(other + 0x18, -1);
    w.mem.put<std::int32_t>(stuck + 0x20, -1);
    w.mem.put<std::int32_t>(moved + 0x08, -1);
    w.mem.put<std::int32_t>(moved_to + 0x08, -1);
  };
  const auto world = [&](std::int32_t slot, std::int32_t elsewhere) {
    w.mem.put<std::int32_t>(state + 0x14, slot);
    w.mem.put<std::int32_t>(state + 0x1F0, slot);
    w.mem.put<std::int32_t>(other + 0x18, elsewhere);
    w.mem.put<std::int32_t>(stuck + 0x20, 3);
    w.mem.put<std::int32_t>(moved + 0x08, 5);
    w.mem.put<std::int32_t>(moved_to + 0x08, 5);
  };
  lodestar::StateSearch search(
      w.base, {rva(w.at("st_hud")), rva(w.at("st_session")), rva(w.at("st_messages")), rva(w.at("st_menus"))});
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  const auto pass = [&] {
    int steps = 0;
    for (; steps < 100; ++steps) {
      reader.begin_tick();
      if (search.step(gt, reader)) break;
    }
    return steps;
  };
  // Nothing is read before a pass is asked for.
  CHECK_FALSE(search.due());
  reader.begin_tick();
  CHECK_FALSE(search.step(gt, reader));
  CHECK(reader.copies() == 0);
  title();
  search.ask();
  CHECK(search.due());
  CHECK(pass() <= 8);
  CHECK(search.passes() == 1);
  CHECK_FALSE(search.due());
  CHECK(search.found().hits.empty());  // nothing is told before the fourth pass
  world(2, 7);
  search.ask();
  (void)pass();
  title();
  w.mem.put<std::int32_t>(stuck + 0x20, 0);           // not -1 again
  w.mem.put<std::uint64_t>(w.at("st_menus"), moved_to);  // another object behind the slot
  search.ask();
  (void)pass();
  world(2, 9);
  search.ask();
  (void)pass();
  search.ask();  // a fifth is not taken
  CHECK(search.asked() == 4);
  const lodestar::StateFound found = search.found();
  CHECK(found.candidates == 4);
  CHECK(found.passes == 4);
  REQUIRE(found.alive.size() == 4);
  CHECK(found.alive[0] == 4);
  CHECK(found.alive[1] == 4);
  CHECK(found.alive[2] == 2);  // the stuck one and the moved one are out
  CHECK(found.alive[3] == 2);  // a field that reads another small number the second time stays: another save's
  CHECK(found.fields_after_title == 6);
  CHECK(found.fields_after_world == 5);
  CHECK(found.fields_after_again == 3);
  CHECK(found.slots_hit == 2);
  CHECK(found.slots_differ == 1);
  CHECK(found.fields_differ == 1);
  REQUIRE(found.hits.size() == 3);
  CHECK(found.hits[0].rva == rva(w.at("st_hud")));
  CHECK(found.hits[0].offset == 0x14);
  CHECK(found.hits[0].value == 2);
  CHECK(found.hits[0].again == 2);
  CHECK(found.hits[0].same_as == 0);
  CHECK(found.hits[1].rva == rva(w.at("st_hud")));
  CHECK(found.hits[1].offset == 0x1F0);
  CHECK(found.hits[1].same_as == 1);
  CHECK(found.hits[2].rva == rva(w.at("st_session")));
  CHECK(found.hits[2].offset == 0x18);
  CHECK(found.hits[2].value == 7);
  CHECK(found.hits[2].again == 9);
  CHECK(found.hits[2].same_as == 2);
  // Each pass was through before the next was asked for.
  REQUIRE(found.looks.size() == 4);
  CHECK_FALSE(found.overrun);
  for (const lodestar::StateLook& l : found.looks) {
    CHECK(l.through);
    CHECK_FALSE(l.overrun);
  }
  // The report carries the search as counts, places and the small number, and lints clean.
  lodestar::Found f = r.found;
  f.state = found;
  const auto text = lodestar::write_report(f);
  if (!text.has_value()) MESSAGE(report::describe(text.error()));
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"state_hits\": [") != std::string::npos);
  CHECK(text->find("\"alive\": [4, 4, 2, 2]") != std::string::npos);
  CHECK(text->find("\"again\": 9") != std::string::npos);
}

TEST_CASE("in game: two slots that keep one object's address hit one field, and the hits say so") {
  FakeWorld w;
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  const std::uint64_t state = w.mem.alloc(0x400);
  w.mem.put<std::uint64_t>(w.at("st_hud"), state);
  w.mem.put<std::uint64_t>(w.at("st_menus"), state);
  lodestar::StateSearch search(w.base, {rva(w.at("st_hud")), rva(w.at("st_menus"))});
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  const auto look = [&](std::int32_t value) {
    w.mem.put<std::int32_t>(state + 0x20, value);
    search.ask();
    for (int steps = 0; steps < 100; ++steps) {
      reader.begin_tick();
      if (search.step(gt, reader)) return;
    }
  };
  look(-1);
  look(0);  // the first save
  look(-1);
  look(2);  // another
  const lodestar::StateFound found = search.found();
  REQUIRE(found.hits.size() == 2);
  CHECK(found.slots_hit == 2);
  CHECK(found.slots_differ == 2);
  CHECK(found.fields_differ == 1);  // one field, seen through two slots
  CHECK(found.hits[0].same_as == 0);
  CHECK(found.hits[1].same_as == 0);
  CHECK(found.hits[0].value == 0);
  CHECK(found.hits[0].again == 2);
}

TEST_CASE("in game: the character search finds the player's word, the near list and the player set in the manager") {
  FakeWorld w;
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  lodestar::CharSearch::Options o;
  o.base = w.base;
  o.image_size = w.sys->image().size();
  o.manager_slot = rva(w.at("st_characters"));
  o.save_slot = rva(w.at("st_save_data"));
  o.character_vtables = {rva(w.vtable("chr")), rva(w.vtable("hero")), rva(w.vtable("npc")), rva(w.vtable("beast"))};
  o.player_vtables = {rva(w.vtable("hero"))};
  o.image = &w.sys->image();
  lodestar::CharSearch search(std::move(o));
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t now = 0;
  const auto look = [&] {
    for (int steps = 0; steps < 2000; ++steps) {
      reader.begin_tick();
      now += 1000;
      if (search.step(gt, reader, now)) return true;
    }
    return false;
  };
  // Nothing is read before a look is asked for.
  CHECK_FALSE(search.due());
  reader.begin_tick();
  CHECK_FALSE(search.step(gt, reader, now));
  CHECK(reader.copies() == 0);
  // At the title: the manager is there, and no character.
  search.ask();
  REQUIRE(look());
  // In the world.
  w.world();
  search.ask();
  REQUIRE(look());
  const lodestar::CharFound found = search.found();
  CHECK(found.rva == rva(w.at("st_characters")));
  CHECK(found.asked == 2);
  CHECK(found.passes == 2);
  CHECK_FALSE(found.overrun);
  REQUIRE(found.looks.size() == 2);
  const lodestar::CharLook& title = found.looks[0];
  CHECK(title.through);
  CHECK(title.manager_bytes == 0x40);  // the test world's objects end where they end: the piece is halved to fit
  CHECK(title.characters == 0);
  CHECK(title.players == 0);
  CHECK(title.pointers.empty());
  CHECK(title.lists.empty());  // the two addresses are the same: an empty run
  CHECK(title.sets.empty());
  CHECK(title.player_bytes == 0);
  const lodestar::CharLook& world = found.looks[1];
  CHECK(world.through);
  CHECK_FALSE(world.cut);
  CHECK(world.took_ms > 0);
  CHECK(world.reads > 0);
  CHECK(world.characters == 1);
  CHECK(world.players == 1);
  REQUIRE(world.pointers.size() == 1);
  CHECK(world.pointers[0].offset == 0x10);
  CHECK(world.pointers[0].player);
  REQUIRE(world.lists.size() == 1);
  CHECK(world.lists[0].offset == 0x20);
  CHECK(world.lists[0].bytes == 0x10 * w.npcs.size());
  CHECK(world.lists[0].stride == 0x10);
  CHECK(world.lists[0].chr_at == 0);
  CHECK(world.lists[0].elements == w.npcs.size());
  CHECK(world.lists[0].characters == w.npcs.size());
  CHECK_FALSE(world.lists[0].has_player);
  CHECK(world.lists[0].has_distance);
  CHECK(world.lists[0].distance_at == 8);
  REQUIRE(world.sets.size() == 1);
  CHECK(world.sets[0].offset == 0x38);
  CHECK(world.sets[0].chr_at == 0);
  CHECK(world.sets[0].vtable == 0);
  CHECK(world.player_bytes == 0x80);
  REQUIRE(world.members.size() == 1);  // the parts
  CHECK(world.members[0].offset == 0x30);
  CHECK(world.members[0].vtable == 0);
  CHECK(world.members_total == 1);
  // A look the tester overran, and one never begun.
  search.ask();
  search.ask();
  const lodestar::CharFound hurried = search.found();
  CHECK(hurried.asked == 4);
  CHECK(hurried.passes == 2);
  CHECK(hurried.overrun);
  // The report carries it as offsets and counts, and lints clean.
  lodestar::Found f = round_of(w).found;
  f.chars = found;
  const auto text = lodestar::write_report(f);
  if (!text.has_value()) MESSAGE(report::describe(text.error()));
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"chars\": {") != std::string::npos);
  CHECK(text->find("\"char_pointers\": [") != std::string::npos);
  CHECK(text->find("\"player\": true") != std::string::npos);
  CHECK(text->find("\"has_distance\": true") != std::string::npos);
}

TEST_CASE("in game: with several objects of the player's class, the main player is the one at the layout's word") {
  FakeWorld w;
  w.world();
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  // Another object of the player's class, at a word of the roster before the main player's.
  const std::uint64_t other = w.mem.alloc(0x98);
  w.set_vtable(other, "hero");
  w.mem.put<std::uint64_t>(w.instance("characters") + 0x8, other);
  const auto look_with = [&](std::uint32_t word) {
    lodestar::CharSearch::Options o;
    o.base = w.base;
    o.image_size = w.sys->image().size();
    o.manager_slot = rva(w.at("st_characters"));
    o.character_vtables = {rva(w.vtable("chr")), rva(w.vtable("hero")), rva(w.vtable("npc")), rva(w.vtable("beast"))};
    o.player_vtables = {rva(w.vtable("hero"))};
    o.image = &w.sys->image();
    lodestar::CharSearch search(std::move(o));
    search.set_main_player_word(word);
    GuardedReader reader(*w.sys);
    const auto gt = en::proto::GameThread::issue();
    search.ask();
    bool through = false;
    for (int steps = 0; steps < 2000 && !through; ++steps) {
      reader.begin_tick();
      through = search.step(gt, reader, static_cast<std::uint64_t>(steps) * 1000);
    }
    REQUIRE(through);
    return search.found().looks.at(0);
  };
  // Told the main player's word, the search takes the object there: its set and its parts are the main player's.
  const lodestar::CharLook told = look_with(0x10);
  CHECK(told.players == 2);
  REQUIRE(told.sets.size() == 1);
  CHECK(told.sets[0].offset == 0x38);
  REQUIRE(told.members.size() == 1);
  CHECK(told.members[0].offset == 0x30);
  // Not told, it takes the first object of the player's class, which is the other one: no set holds its address,
  // and it points at nothing.
  const lodestar::CharLook untold = look_with(0);
  CHECK(untold.players == 2);
  CHECK(untold.sets.empty());
  CHECK(untold.members.empty());
}

TEST_CASE("in game: the character search tells a holder of parts in the player's object by what its words point at") {
  FakeWorld w;
  w.world();
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  // A holder: 0x100 bytes whose first sixteen words hold ten addresses, seven of objects with a table of methods.
  const std::uint64_t holder = w.mem.alloc(0x100);
  for (std::uint64_t i = 0; i < 10; ++i) {
    const std::uint64_t part = w.mem.alloc(0x100);
    if (i < 7) w.set_vtable(part, "hud");
    w.mem.put<std::uint64_t>(holder + 8 * i, part);
  }
  w.mem.put<std::uint64_t>(w.player + 0x38, holder);
  lodestar::CharSearch::Options o;
  o.base = w.base;
  o.image_size = w.sys->image().size();
  o.manager_slot = rva(w.at("st_characters"));
  o.character_vtables = {rva(w.vtable("chr")), rva(w.vtable("hero")), rva(w.vtable("npc")), rva(w.vtable("beast"))};
  o.player_vtables = {rva(w.vtable("hero"))};
  o.image = &w.sys->image();
  lodestar::CharSearch search(std::move(o));
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  search.ask();
  bool through = false;
  for (int steps = 0; steps < 2000 && !through; ++steps) {
    reader.begin_tick();
    through = search.step(gt, reader, static_cast<std::uint64_t>(steps) * 1000);
  }
  REQUIRE(through);
  const lodestar::CharFound found = search.found();
  REQUIRE(found.looks.size() == 1);
  const lodestar::CharLook& look = found.looks[0];
  REQUIRE(look.members.size() == 2);
  CHECK(look.members[0].offset == 0x30);
  CHECK(look.members[0].classed == 0);
  CHECK(look.members[1].offset == 0x38);
  CHECK(look.members[1].vtable == 0);
  CHECK(look.members[1].pointers == 10);
  CHECK(look.members[1].classed == 7);
  CHECK(look.save_bytes == 0);  // no slot was given for the save data
}

TEST_CASE("in game: a look of the state search that the tester overran is told, with each look's time") {
  FakeWorld w;
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  lodestar::StateSearch search(w.base, {rva(w.at("st_hud")), rva(w.at("st_session")), rva(w.at("st_menus"))});
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t now = 1'000'000;
  const auto pass = [&] {
    for (int steps = 0; steps < 100; ++steps) {
      reader.begin_tick();
      now += 250'000;  // a quarter of a second a step
      if (search.step(gt, reader, now)) return true;
    }
    return false;
  };
  search.ask();
  REQUIRE(pass());
  // The second look is asked for and one candidate read; then the tester confirms the next state already.
  search.ask();
  reader.begin_tick();
  now += 250'000;
  CHECK_FALSE(search.step(gt, reader, now));
  search.ask();
  REQUIRE(pass());  // the second ends late
  REQUIRE(pass());  // the third in its own time
  // The fourth is asked for and the session ends before it is through.
  search.ask();
  const lodestar::StateFound found = search.found();
  CHECK(found.asked == 4);
  CHECK(found.passes == 3);
  CHECK(found.overrun);
  REQUIRE(found.looks.size() == 4);
  CHECK(found.looks[0].through);
  CHECK_FALSE(found.looks[0].overrun);
  CHECK(found.looks[0].took_ms >= 500);  // three candidates and the count, a quarter of a second each
  CHECK(found.looks[0].took_ms <= 1250);
  CHECK(found.looks[1].through);
  CHECK(found.looks[1].overrun);  // the one the tester moved the game on under
  CHECK(found.looks[2].through);
  CHECK_FALSE(found.looks[2].overrun);
  CHECK_FALSE(found.looks[3].through);
  CHECK_FALSE(found.looks[3].overrun);
  CHECK(found.looks[3].took_ms == 0);
  CHECK(found.hits.empty());
  // A look that was never begun when the next was asked for is overrun too.
  lodestar::StateSearch hurried(w.base, {rva(w.at("st_hud"))});
  hurried.ask();
  hurried.ask();
  CHECK(hurried.found().looks.size() == 2);
  CHECK(hurried.found().looks[0].overrun);
  CHECK(hurried.found().overrun);
  // The report tells it.
  lodestar::Found f = round_of(w).found;
  f.state = found;
  const auto text = lodestar::write_report(f);
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"overrun\": true") != std::string::npos);
  CHECK(text->find("\"looks\": [") != std::string::npos);
}

TEST_CASE("in game: the pair search finds the field that reads one number in the first world and another in the second") {
  FakeWorld w;
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  const std::uint64_t state = w.mem.alloc(0x400), other = w.mem.alloc(0x400);
  w.mem.put<std::uint64_t>(w.at("st_hud"), state);
  w.mem.put<std::uint64_t>(w.at("st_session"), other);
  w.mem.put<std::uint64_t>(w.at("st_menus"), 0);  // nothing behind it
  w.mem.put<std::uint32_t>(state + 0x14, 6);      // the slot: 6, then 2
  w.mem.put<std::uint32_t>(state + 0x20, 0x0600); // a byte that reads 6 in a field that does not
  w.mem.put<std::uint32_t>(other + 0x10, 6);      // 6, then 7: no hit
  lodestar::PairSearch search({rva(w.at("st_hud")), rva(w.at("st_session")), rva(w.at("st_menus"))}, 6, 2,
                              &w.sys->image());
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t now = 0;
  const auto look = [&] {
    for (int steps = 0; steps < 200; ++steps) {
      reader.begin_tick();
      now += 100'000;
      if (search.step(gt, reader, now)) return true;
    }
    return false;
  };
  CHECK_FALSE(search.due());
  search.ask();
  REQUIRE(look());
  lodestar::PairFound found = search.found();
  CHECK(found.passes == 1);
  CHECK(found.objects_first == 2);
  CHECK(found.words_first == 2);
  CHECK(found.bytes_first == 3);  // the two fields' own low bytes, and the lone byte
  CHECK(found.hits.empty());      // nothing is told before the second look
  // The second world: another object behind the first slot (the game made it anew), and the numbers change.
  const std::uint64_t again = w.mem.alloc(0x400);
  w.mem.put<std::uint64_t>(w.at("st_hud"), again);
  w.mem.put<std::uint32_t>(again + 0x14, 2);
  w.mem.put<std::uint32_t>(again + 0x20, 0x0200);
  w.mem.put<std::uint32_t>(other + 0x10, 7);
  search.ask();
  REQUIRE(look());
  search.ask();  // a third is not taken
  found = search.found();
  CHECK(found.first == 6);
  CHECK(found.second == 2);
  CHECK(found.candidates == 3);
  CHECK(found.asked == 2);
  CHECK(found.passes == 2);
  CHECK_FALSE(found.overrun);
  CHECK(found.objects_second == 2);  // both that read 6 are read again; the empty slot never was
  CHECK(found.word_hits == 1);
  CHECK(found.byte_hits == 1);
  REQUIRE(found.hits.size() == 2);
  CHECK(found.hits[0].rva == rva(w.at("st_hud")));
  CHECK(found.hits[0].offset == 0x14);
  CHECK(found.hits[0].width == 4);
  CHECK(found.hits[1].offset == 0x21);
  CHECK(found.hits[1].width == 1);
  lodestar::Found f = round_of(w).found;
  f.pair = found;
  const auto text = lodestar::write_report(f);
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"pair_hits\": [") != std::string::npos);
}

TEST_CASE("in game: the parts watch tells a position by its moving while the tester walks, and health by a hit and a heal") {
  FakeWorld w;
  w.world();
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  // The holder: three addresses. A classed part with three fractions (two of them move) and a timer; a plain part
  // with a maximum and a number below it; and an address of nothing.
  const std::uint64_t holder = w.mem.alloc(0x100), body = w.mem.alloc(0x100), vitals = w.mem.alloc(0x40);
  w.set_vtable(body, "hud");
  w.mem.put<std::uint64_t>(holder + 0x0, body);
  w.mem.put<std::uint64_t>(holder + 0x8, vitals);
  w.mem.put<std::uint64_t>(holder + 0x10, 0x7000'0000'0000ull);
  w.mem.put<std::uint64_t>(w.player + 0x38, holder);
  w.mem.put<float>(body + 0x20, 100.0f);
  w.mem.put<float>(body + 0x24, 5.0f);
  w.mem.put<float>(body + 0x28, 200.0f);
  w.mem.put<float>(body + 0x2C, 0.5f);          // a direction: it moves in the walk, within 2
  w.mem.put<std::int32_t>(body + 0x50, 50);     // a number of the earlier part that falls at the hit and stays
  w.mem.put<std::int32_t>(vitals + 0x0, 1200);
  w.mem.put<std::int32_t>(vitals + 0x4, 1100);
  lodestar::PartsWatch::Options o;
  o.base = w.base;
  o.image_size = w.sys->image().size();
  o.manager_slot = rva(w.at("st_characters"));
  o.player_word = 0x10;
  o.holder_at = 0x38;
  o.image = &w.sys->image();
  lodestar::PartsWatch watch(o);
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t now = 10'000'000;
  std::uint32_t timer = 0;
  float x = 100.0f;
  const auto pass = [&](bool walking) {
    now += 1'000'000;
    w.mem.put<std::uint32_t>(body + 0x40, ++timer);  // something that changes all the time
    if (walking) {
      x += 1.5f;
      w.mem.put<float>(body + 0x20, x);
      w.mem.put<float>(body + 0x28, 2.0f * x);
      w.mem.put<float>(body + 0x2C, timer % 2 == 0 ? 0.5f : -0.5f);
    }
    for (int steps = 0; steps < 200; ++steps) {
      reader.begin_tick();
      if (watch.step(gt, reader, now)) return;
    }
    FAIL("a pass did not end");
  };
  CHECK(watch.due(now));
  for (int i = 0; i < 5; ++i) pass(false);  // standing
  CHECK_FALSE(watch.due(now));              // the next pass is a second away
  watch.mark(now);
  for (int i = 0; i < 10; ++i) pass(true);  // walking
  watch.mark(now);
  pass(true);  // still coming to a stop: within the phase's first seconds, not counted
  for (int i = 0; i < 7; ++i) pass(false);
  watch.mark(now);  // the hit's phase
  pass(false);
  w.mem.put<std::int32_t>(vitals + 0x4, 900);
  w.mem.put<std::int32_t>(body + 0x50, 40);
  pass(false);
  pass(false);
  watch.mark(now);  // the heal's
  w.mem.put<std::int32_t>(vitals + 0x4, 1100);
  pass(false);
  pass(false);
  watch.mark(now);
  pass(false);
  const lodestar::PartsFound found = watch.found();
  CHECK(found.player_word == 0x10);
  CHECK(found.holder_at == 0x38);
  CHECK(found.passes == 29);
  CHECK(found.absent == 0);
  CHECK(found.restarts == 0);
  CHECK(found.phase == 5);
  REQUIRE(found.phase_passes.size() == 6);
  CHECK(found.phase_passes[0] == 5);
  CHECK(found.phase_passes[1] == 7);  // ten, less the three within the phase's first four seconds
  CHECK(found.phase_passes[2] == 5);
  CHECK(found.phase_passes[3] == 3);  // the hit's and the heal's phases count from their start
  CHECK(found.phase_passes[4] == 2);
  REQUIRE(found.parts.size() == 2);   // the address of nothing is no part
  CHECK(found.parts[0].index == 0);
  CHECK(found.parts[0].vtable == rva(w.vtable("hud")));
  CHECK(found.parts[0].bytes == 0x100);
  CHECK(found.parts[1].index == 1);
  CHECK(found.parts[1].vtable == 0);
  CHECK(found.parts[1].bytes == 0x40);
  CHECK(found.moves_total == 3);
  REQUIRE(found.moves.size() == 3);   // the timer changed while standing too: no move; the middle fraction never moved
  CHECK(found.moves[0].part == 0);
  CHECK(found.moves[0].offset == 0x20);
  CHECK(found.moves[0].fraction);
  CHECK(found.moves[0].span == 3);    // it ranged over metres: a place
  CHECK(found.moves[1].span == 3);
  CHECK(found.moves[2].offset == 0x2C);
  CHECK(found.moves[2].span == 1);    // within 2: a direction, listed after the places
  REQUIRE(found.moves[0].changed.size() == 6);
  CHECK(found.moves[0].changed[0] == 0);
  CHECK(found.moves[0].changed[1] == 7);
  CHECK(found.moves[0].changed[2] == 0);
  CHECK(found.moves[1].offset == 0x28);
  CHECK(found.falls_total == 2);
  REQUIRE(found.falls.size() == 2);   // the one that fell once and rose once under a bound comes first, whatever its part
  CHECK(found.falls[1].part == 0);
  CHECK(found.falls[1].offset == 0x50);
  CHECK(found.falls[1].rose == 0);
  CHECK(found.falls[0].part == 1);
  CHECK(found.falls[0].offset == 0x4);
  CHECK(found.falls[0].fell == 1);
  CHECK(found.falls[0].rose == 1);
  CHECK(found.falls[0].has_max);
  CHECK(found.falls[0].max_at == 0x0);
  // No player (the title): a pass that reads nothing of the parts, counted as absent.
  w.title();
  pass(false);
  CHECK(watch.found().absent == 1);
  // The report carries where the numbers lie and how they behaved, and lints clean.
  lodestar::Found f = round_of(w).found;
  f.parts = found;
  const auto text = lodestar::write_report(f);
  if (!text.has_value()) MESSAGE(report::describe(text.error()));
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"moves\": [") != std::string::npos);
  CHECK(text->find("\"has_max\": true") != std::string::npos);
  CHECK(text->find("\"span\": 3") != std::string::npos);
  CHECK(text->find("1100") == std::string::npos);  // what a number held is never written
}

TEST_CASE("in game: a lead of the leads' statics names a class by its innermost name, for an object no accessor reaches") {
  // The game's state: no static of the accessor's holds it and the design gives only a name. The namer answers
  // whether a table of methods is that class's, from the image's type information; the name is kept nowhere.
  FakeWorld w;
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  auto ids = parse_ids(kRepositoryIdsToml);
  REQUIRE(ids.has_value());
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  std::string text = kLeads;
  text += "game_state = { rtti = \"Hud\" }\nfield_area = { rtti = \"Elsewhere::Veil\" }\n"
          "save_data = { rtti = \"W::NoSuchClass\" }\nglobal_hinstance = { module = true }\n";
  const auto leads = derive::parse_leads(text);
  REQUIRE(leads.has_value());
  const lodestar::Namer namer(gated->image, w.sys->image(), *leads, *ids);
  // By the innermost name, whatever namespace the lead or the class gives.
  CHECK(namer.static_lead_tables("game_state") == 1);
  CHECK(namer.static_lead_class("game_state", rva(w.vtable("hud"))) == 1);
  CHECK(namer.static_lead_class("game_state", rva(w.vtable("veil"))) == 0);
  CHECK(namer.static_lead_class("game_state", 0) == 0);
  CHECK(namer.static_lead_class("field_area", rva(w.vtable("veil"))) == 1);
  // A lead whose class the image does not hold answers for nothing; so does a label the leads do not hold, and the
  // module's lead, which names no class.
  CHECK(namer.static_lead_tables("save_data") == 0);
  CHECK(namer.static_lead_class("save_data", rva(w.vtable("hud"))) == 0);
  CHECK(namer.static_lead_tables("camera") == 0);
  CHECK(namer.static_lead_class("camera", rva(w.vtable("eye"))) == 0);
  CHECK(namer.static_lead_tables("global_hinstance") == 0);
}

TEST_CASE("in game: a long run of rising addresses does not use up the lists' candidates, and the layout's list comes first") {
  // The fifteenth round's manager: runs of neighbouring addresses, each pair of which looks like a run's bounds,
  // filled every candidate before the lists' own places were reached. Here 200 rising addresses stand before two
  // lists; the pairs inside the run are tried last, and the list at the place the layout gives is judged first.
  FakeWorld w;
  w.world();
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  std::vector<std::uint64_t> chars;
  for (int i = 0; i < 12; ++i) {
    chars.push_back(w.mem.alloc(0x100));
    w.set_vtable(chars.back(), i == 0 ? "hero" : "npc");
  }
  const std::uint64_t manager = w.mem.alloc(0x2000);
  w.mem.put<std::uint64_t>(w.at("st_characters"), manager);
  w.mem.put<std::uint64_t>(manager + 0x10, chars[0]);
  std::uint64_t last = 0;
  for (std::uint32_t i = 0; i < 200; ++i) {
    const std::uint64_t block = w.mem.alloc(0x40);
    REQUIRE(block > last);
    REQUIRE((last == 0 || block - last <= lodestar::CharSearch::kListSpan));
    last = block;
    w.mem.put<std::uint64_t>(manager + 0x100 + 8 * i, block);
  }
  const auto list = [&](std::uint32_t at, std::uint32_t elements) {
    const std::uint64_t run = w.mem.alloc(0x400);
    for (std::uint32_t k = 0; k < elements; ++k) {
      w.mem.put<std::uint64_t>(run + k * 16, chars[1 + k % 11]);
      w.mem.put<float>(run + k * 16 + 8, 1.0f + static_cast<float>((k * 7) % 5));
    }
    w.mem.put<std::uint64_t>(manager + at, run);
    w.mem.put<std::uint64_t>(manager + at + 8, run + elements * 16);
    w.mem.put<std::uint64_t>(manager + at + 16, run + 0x400);
  };
  list(0x1000, 9);
  list(0x1800, 6);
  const auto run = [&](std::uint32_t list_word) {
    lodestar::CharSearch::Options o;
    o.base = w.base;
    o.image_size = w.sys->image().size();
    o.manager_slot = rva(w.at("st_characters"));
    o.main_player_word = 0x10;
    o.list_word = list_word;
    o.character_vtables = {rva(w.vtable("chr")), rva(w.vtable("hero")), rva(w.vtable("npc")), rva(w.vtable("beast"))};
    o.player_vtables = {rva(w.vtable("hero"))};
    o.image = &w.sys->image();
    lodestar::CharSearch search(std::move(o));
    GuardedReader reader(*w.sys);
    const auto gt = en::proto::GameThread::issue();
    search.ask(3);
    bool through = false;
    for (int steps = 0; steps < 100'000 && !through; ++steps) {
      reader.begin_tick();
      through = search.step(gt, reader, static_cast<std::uint64_t>(steps) * 1000);
    }
    REQUIRE(through);
    lodestar::CharFound found = search.found();
    REQUIRE(found.looks.size() == 1);
    return found.looks[0].lists;
  };
  // Without the layout's word: both lists are found behind the run, neither marked.
  const std::vector<lodestar::CharList> plain = run(0);
  REQUIRE(plain.size() == 2);
  CHECK(plain[0].offset == 0x1000);
  CHECK(plain[0].stride == 16);
  CHECK(plain[0].elements == 9);
  CHECK(plain[0].characters == 9);
  CHECK(plain[0].has_fraction);
  CHECK_FALSE(plain[0].expected);
  CHECK(plain[1].offset == 0x1800);
  // With it: that list first, and marked.
  const std::vector<lodestar::CharList> given = run(0x1800);
  REQUIRE(given.size() == 2);
  CHECK(given[0].offset == 0x1800);
  CHECK(given[0].expected);
  CHECK(given[0].elements == 6);
  CHECK(given[1].offset == 0x1000);
  CHECK_FALSE(given[1].expected);
  // A word of the layout's that holds no bounds now is no candidate.
  w.mem.put<std::uint64_t>(manager + 0x1808, 0);
  const std::vector<lodestar::CharList> gone = run(0x1800);
  REQUIRE(gone.size() == 1);
  CHECK(gone[0].offset == 0x1000);
}

TEST_CASE("in game: the character survey tells a character's fields by how they stand among them and over time") {
  // An invented world: a manager with the main player's word and a near list; characters of 2 KiB each with a
  // handle, a kind's number and that number divided by 10,000, an id of their own, a type, a team, a block's id, a
  // byte of flags (on screen, dead), a counter that never keeps still, and an address of their own; each with a holder
  // of parts and a health. The tester stands, looks around (the on-screen bit flips), and one character dies.
  FakeWorld w;
  w.world();
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  const std::uint64_t manager = w.mem.alloc(0x100), run = w.mem.alloc(16 * 0x10);
  w.mem.put<std::uint64_t>(w.at("st_characters"), manager);
  struct Made {
    std::uint64_t at = 0, vitals = 0;
  };
  std::vector<Made> chars;
  const auto make = [&](bool hero, std::uint32_t index) {
    Made c;
    c.at = w.mem.alloc(0x800);
    w.set_vtable(c.at, hero ? "hero" : "npc");
    const std::uint32_t kind = hero ? 0 : 43'400'000 + (index % 3) * 10'000;
    w.mem.put<std::uint64_t>(c.at + 0x08, 0xFFF0'0015'0000'0001ull + index * 3);  // a handle: no address
    w.mem.put<std::uint32_t>(c.at + 0x10, kind);
    w.mem.put<std::uint32_t>(c.at + 0x14, kind / 10'000);
    w.mem.put<std::uint32_t>(c.at + 0x18, hero ? 0 : (index % 4 == 0 ? 0 : 1'044'360'000 + index));
    w.mem.put<std::int32_t>(c.at + 0x1C, hero ? 0 : 3);
    w.mem.put<std::uint8_t>(c.at + 0x20, hero ? 1 : 6);
    w.mem.put<std::uint8_t>(c.at + 0x21, 0x02);  // on screen
    w.mem.put<std::uint32_t>(c.at + 0x24, 0x3C2C'2400u);
    const std::uint64_t holder = w.mem.alloc(0x40);
    c.vitals = w.mem.alloc(0x20);
    w.mem.put<std::uint64_t>(c.at + 0x30, holder);
    w.mem.put<std::uint64_t>(holder + 0x0, c.vitals);
    w.mem.put<std::int32_t>(c.vitals + 0x8, 500);
    w.mem.put<std::int32_t>(c.vitals + 0xC, 500);
    w.mem.put<std::uint64_t>(c.at + 0x100, w.mem.alloc(0x10));  // an address of its own
    w.mem.put<std::uint32_t>(c.at + 0x200, 0x3F80'0000u);       // the same in all: 1.0
    chars.push_back(c);
    return c.at;
  };
  w.mem.put<std::uint64_t>(manager + 0x10, make(true, 0));
  for (std::uint32_t i = 1; i <= 9; ++i) w.mem.put<std::uint64_t>(run + 0x10 * (i - 1), make(false, i));
  w.mem.put<std::uint64_t>(manager + 0x20, run);
  w.mem.put<std::uint64_t>(manager + 0x28, run + 9 * 0x10);
  lodestar::CharSurvey::Options o;
  o.base = w.base;
  o.image_size = w.sys->image().size();
  o.manager_slot = rva(w.at("st_characters"));
  o.main_player_word = 0x10;
  o.list_word = 0x20;
  o.parts_at = 0x30;
  o.vitals_at = 0x0;
  o.hp_at = 0x8;
  o.character_vtables = {rva(w.vtable("chr")), rva(w.vtable("hero")), rva(w.vtable("npc")), rva(w.vtable("beast"))};
  o.image = &w.sys->image();
  lodestar::CharSurvey survey(o);
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t now = 10'000'000;
  std::uint32_t tick = 0;
  const auto pass = [&](bool looking) {
    now += lodestar::CharSurvey::kEveryUs;
    ++tick;
    for (std::size_t i = 0; i < chars.size(); ++i) {
      w.mem.put<std::uint32_t>(chars[i].at + 0x40, tick);  // a counter: some of its bits flip in one phase only
      if (looking && i >= 1 && i <= 6) w.mem.put<std::uint8_t>(chars[i].at + 0x21, tick % 2 == 0 ? 0x02 : 0x00);
    }
    for (int steps = 0; steps < 2000; ++steps) {
      reader.begin_tick();
      if (survey.step(gt, reader, now)) return;
    }
    FAIL("a pass did not end");
  };
  CHECK(survey.due(now));
  for (int i = 0; i < 6; ++i) pass(false);  // standing
  CHECK_FALSE(survey.due(now));
  survey.mark(now);
  for (int i = 0; i < 8; ++i) pass(true);   // looking around
  survey.mark(now);
  for (std::size_t i = 1; i <= 6; ++i) w.mem.put<std::uint8_t>(chars[i].at + 0x21, 0x02);
  pass(false);
  // One dies: its health goes to nothing, its dead bit is set, and it leaves the screen's count as it was.
  w.mem.put<std::int32_t>(chars[4].vitals + 0x8, 0);
  w.mem.put<std::uint8_t>(chars[4].at + 0x21, 0x22);
  pass(false);
  survey.mark(now);
  for (int i = 0; i < 4; ++i) pass(false);
  const lodestar::SurveyFound f = survey.found();
  CHECK(f.passes == 20);
  CHECK(f.absent == 0);
  CHECK(f.tracked == 10);
  CHECK(f.most_listed == 10);
  CHECK(f.phase == 3);
  REQUIRE(f.phase_passes.size() == 4);
  CHECK(f.phase_passes[0] == 6);
  CHECK(f.phase_passes[1] == 8);
  CHECK(f.phase_passes[2] == 2);
  CHECK(f.phase_passes[3] == 4);
  CHECK(f.deaths == 1);
  const auto field = [&](std::uint32_t offset) -> const lodestar::SurveyField* {
    for (const lodestar::SurveyField& r : f.fields) {
      if (r.offset == offset) return &r;
    }
    return nullptr;
  };
  // The kind's number: three kinds among nine, numbers of an id's size, and the player's (none) stands alone.
  REQUIRE(field(0x10) != nullptr);
  CHECK(field(0x10)->seen == 10);
  CHECK(field(0x10)->changed_for == 0);
  CHECK(field(0x10)->distinct == 4);
  CHECK(field(0x10)->most == 3);
  CHECK(field(0x10)->id_like == 9);
  CHECK(field(0x10)->player_alone);
  REQUIRE(field(0x14) != nullptr);
  CHECK(field(0x14)->distinct == 4);
  // The type: small, the same in every other character, and the player's alone.
  REQUIRE(field(0x1C) != nullptr);
  CHECK(field(0x1C)->small == 9);
  CHECK(field(0x1C)->distinct == 2);
  CHECK(field(0x1C)->player_alone);
  // The block's id: the same in all, and listed all the same for its shape; a plain 1.0 in all is not.
  REQUIRE(field(0x24) != nullptr);
  CHECK(field(0x24)->block_like == 10);
  CHECK(field(0x24)->distinct == 1);
  CHECK(field(0x200) == nullptr);
  // The timer changes in every character, and an address is no field: neither is listed.
  CHECK(field(0x40) == nullptr);
  CHECK(field(0x100) == nullptr);
  CHECK(field(0x30) == nullptr);
  // The handle: eight bytes, different in every character, never changed, no address. The address of its own
  // differs in every character too, and is left out for being one.
  REQUIRE(f.handles.size() == 1);
  CHECK(f.handles[0] == 0x08);
  // The kind's number divided by 10,000 is its neighbour, for every character but the player.
  REQUIRE_FALSE(f.pairs.empty());
  CHECK(f.pairs[0].a == 0x10);
  CHECK(f.pairs[0].b == 0x14);
  CHECK(f.pairs[0].holds == 9);
  CHECK(f.pairs[0].of == 9);
  // The dead bit: it was set as the one died, clear in every living character at every sighting since, and set
  // in the dead one. The on-screen bit flipped before in the looking phase, so it is no candidate.
  REQUIRE(f.dead_bits.size() == 1);
  CHECK(f.dead_total == 1);
  CHECK(f.dead_bits[0].offset == 0x21);
  CHECK(f.dead_bits[0].bit == 5);
  CHECK(f.dead_bits[0].set);
  CHECK(f.dead_bits[0].against == 0);
  CHECK(f.dead_bits[0].agree_dead == 5);
  CHECK(f.dead_bits[0].agree_alive == 9 * 5 - 4);  // the four read before it at its death's pass are not counted
  // The on-screen bit: flipped in six characters while the tester looked around, never while they stood.
  REQUIRE_FALSE(f.screen_bits.empty());
  CHECK(f.screen_bits[0].offset == 0x21);
  CHECK(f.screen_bits[0].bit == 1);
  CHECK(f.screen_bits[0].still == 0);
  CHECK(f.screen_bits[0].look >= 6 * 7);
  CHECK(f.screen_bits[0].flipped_in >= 6);
  CHECK(f.screen_bits[0].passes == 8);
  CHECK(f.screen_total == 1);
  // At the title: no main player and an empty list. A pass that reads no character is counted as absent.
  w.mem.put<std::uint64_t>(manager + 0x10, 0);
  w.mem.put<std::uint64_t>(manager + 0x28, run);
  pass(false);
  pass(false);
  const lodestar::SurveyFound gone = survey.found();
  CHECK(gone.absent == 2);
  // What the last pass with characters told is kept: an empty pass at the end takes nothing away. (The eighteenth
  // round's first run in the game ended with five of them, and its report held no field.)
  CHECK(gone.fields.size() == f.fields.size());
  CHECK(gone.fields_total == f.fields_total);
  CHECK(gone.handles == f.handles);
  CHECK(gone.pairs.size() == f.pairs.size());
  CHECK(gone.dead_bits.size() == 1);
  CHECK_FALSE(gone.screen_bits.empty());
  // The report carries where the fields lie and how they behaved, and lints clean; no value is written.
  lodestar::Found found = round_of(w).found;
  found.survey = f;
  const auto text = lodestar::write_report(found);
  if (!text.has_value()) MESSAGE(report::describe(text.error()));
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"survey_fields\": [") != std::string::npos);
  CHECK(text->find("43400000") == std::string::npos);
  CHECK(text->find("1044360") == std::string::npos);
}

TEST_CASE("in game: the round's searches at the game's sizes keep within their buffers (a large manager, full parts)") {
  // What the unit cases above do not reach: a manager of 256 KiB full of addresses (some of nothing, some of runs,
  // many of characters), a holder with all 32 parts of 4 KiB each changing all over, and thousands of slots for the
  // pair search. The numbers are invented; the point is that every limit is met and nothing is written past its
  // buffer (the sanitiser build runs this case too).
  FakeWorld w;
  w.world();
  const auto rva = [&](std::uint64_t at) { return static_cast<std::uint32_t>(at - w.base); };
  std::uint32_t seed = 12345;
  const auto rnd = [&] {
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
  };
  // A pool of objects: characters, players, plain blocks, runs of elements.
  std::vector<std::uint64_t> chars, players, plain;
  for (int i = 0; i < 300; ++i) {
    const std::uint64_t c = w.mem.alloc(0x200);
    w.set_vtable(c, i % 7 == 0 ? "hero" : (i % 3 == 0 ? "npc" : "chr"));
    (i % 7 == 0 ? players : chars).push_back(c);
  }
  for (int i = 0; i < 200; ++i) plain.push_back(w.mem.alloc(0x400));
  const std::uint64_t manager = w.mem.alloc(0x40000);
  w.mem.put<std::uint64_t>(w.at("st_characters"), manager);
  for (std::uint32_t off = 8; off + 8 <= 0x40000; off += 8) {
    const std::uint32_t r = rnd() % 16;
    std::uint64_t v = 0;
    if (r < 4) v = chars[rnd() % chars.size()];
    else if (r == 4) v = players[rnd() % players.size()];
    else if (r < 9) v = plain[rnd() % plain.size()] + (rnd() % 8) * 8;
    else if (r == 9) v = 0x7000'0000'0000ull + (rnd() % 4096) * 8;  // an address of nothing
    w.mem.put<std::uint64_t>(manager + off, v);
  }
  // Runs: a start and an end side by side, some followed by a third, the elements holding characters.
  for (int i = 0; i < 100; ++i) {
    const std::uint64_t run = plain[rnd() % plain.size()];
    const std::uint32_t n = 1 + rnd() % 60;
    for (std::uint32_t k = 0; k < n && k * 16 + 16 <= 0x400; ++k) {
      w.mem.put<std::uint64_t>(run + k * 16, chars[rnd() % chars.size()]);
      w.mem.put<float>(run + k * 16 + 8, static_cast<float>(k));
    }
    const std::uint32_t at = 0x1000 + static_cast<std::uint32_t>(i) * 0x200;
    w.mem.put<std::uint64_t>(manager + at, run);
    w.mem.put<std::uint64_t>(manager + at + 8, run + std::min<std::uint32_t>(n * 16, 0x400));
    w.mem.put<std::uint64_t>(manager + at + 16, run + 0x400);
  }
  // The main player, with a full holder of parts, at the word the layout would name.
  const std::uint64_t hero = w.mem.alloc(0x4000), holder = w.mem.alloc(0x100);
  w.set_vtable(hero, "hero");
  std::vector<std::uint64_t> parts;
  for (std::uint64_t i = 0; i < 32; ++i) {
    parts.push_back(w.mem.alloc(0x1000));
    if (i % 2 == 0) w.set_vtable(parts.back(), "hud");
    w.mem.put<std::uint64_t>(holder + 8 * i, parts.back());
  }
  for (std::uint32_t off = 8; off + 8 <= 0x4000; off += 8) {
    w.mem.put<std::uint64_t>(hero + off, rnd() % 3 == 0 ? plain[rnd() % plain.size()] : std::uint64_t{rnd()});
  }
  w.mem.put<std::uint64_t>(hero + 0x190, holder);
  w.mem.put<std::uint64_t>(manager + 0x1E508, hero);
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  // The character search, to its end.
  lodestar::CharSearch::Options o;
  o.base = w.base;
  o.image_size = w.sys->image().size();
  o.manager_slot = rva(w.at("st_characters"));
  o.save_slot = rva(w.at("st_save_data"));
  o.main_player_word = 0x1E508;
  o.character_vtables = {rva(w.vtable("chr")), rva(w.vtable("hero")), rva(w.vtable("npc")), rva(w.vtable("beast"))};
  o.player_vtables = {rva(w.vtable("hero"))};
  o.image = &w.sys->image();
  lodestar::CharSearch chars_search(std::move(o));
  for (int look = 0; look < 4; ++look) {
    chars_search.ask(static_cast<std::uint32_t>(look));
    bool through = false;
    for (int steps = 0; steps < 400'000 && !through; ++steps) {
      reader.begin_tick();
      through = chars_search.step(gt, reader, static_cast<std::uint64_t>(steps) * 1000);
    }
    REQUIRE(through);
  }
  const lodestar::CharFound cf = chars_search.found();
  REQUIRE(cf.looks.size() == 4);
  CHECK(cf.looks[0].manager_bytes == 0x40000);
  CHECK(cf.looks[0].pointers.size() == lodestar::CharSearch::kListedPointers);
  CHECK(cf.looks[0].lists.size() <= lodestar::CharSearch::kListedLists);
  CHECK(cf.looks[0].members.size() <= lodestar::CharSearch::kListedMembers);
  CHECK(cf.looks[0].player_bytes == 0x4000);
  // The parts watch, through its phases, every field of every part changing.
  lodestar::PartsWatch::Options po;
  po.base = w.base;
  po.image_size = w.sys->image().size();
  po.manager_slot = rva(w.at("st_characters"));
  po.player_word = 0x1E508;
  po.holder_at = 0x190;
  po.image = &w.sys->image();
  lodestar::PartsWatch watch(po);
  std::uint64_t now = 1'000'000;
  for (int pass = 0; pass < 60; ++pass) {
    now += 1'000'000;
    if (pass % 8 == 7) watch.mark(now);  // more marks than phases
    for (std::uint64_t part : parts) {
      for (std::uint32_t off = 8; off + 4 <= 0x1000; off += 4) {
        if ((off / 4 + static_cast<std::uint32_t>(pass)) % 3 != 0) w.mem.put<std::uint32_t>(part + off, rnd() % 1000);
      }
    }
    if (pass == 40) w.mem.put<std::uint64_t>(holder + 8 * 5, plain[0]);  // a part replaced
    if (pass == 45) w.mem.put<std::uint64_t>(holder + 8 * 6, 0);         // and one gone
    bool ended = false;
    for (int steps = 0; steps < 2000 && !ended; ++steps) {
      reader.begin_tick();
      ended = watch.step(gt, reader, now);
    }
    REQUIRE(ended);
    (void)watch.found();
  }
  const lodestar::PartsFound pf = watch.found();
  CHECK(pf.passes == 60);
  CHECK(pf.parts.size() >= 30);
  CHECK(pf.moves.size() <= lodestar::PartsWatch::kMaxMoves);
  CHECK(pf.falls.size() <= lodestar::PartsWatch::kMaxFalls);
  CHECK(pf.phase == lodestar::PartsWatch::kPhases - 1);
  // The pair search over more slots than it keeps, every object full of the first number.
  std::vector<std::uint32_t> slots;
  std::vector<std::uint64_t> objects;
  for (int i = 0; i < 64; ++i) {
    objects.push_back(w.mem.alloc(i % 3 == 0 ? 0x2000 : 0x800));
    for (std::uint32_t off = 0; off + 4 <= (i % 3 == 0 ? 0x2000u : 0x800u); off += 4) {
      w.mem.put<std::uint32_t>(objects.back() + off, off % 16 == 0 ? 6u : 0x06060606u);
    }
  }
  const std::uint64_t table = w.mem.alloc(8 * 9000);
  for (std::uint32_t i = 0; i < 9000; ++i) {
    w.mem.put<std::uint64_t>(table + 8ull * i, objects[i % objects.size()]);
  }
  // Slots must be read from the image: use the image's own statics for a few, and check the cap with many copies.
  for (std::uint32_t i = 0; i < 9000; ++i) slots.push_back(rva(w.at(i % 2 == 0 ? "st_hud" : "st_menus")));
  w.mem.put<std::uint64_t>(w.at("st_hud"), objects[0]);
  w.mem.put<std::uint64_t>(w.at("st_menus"), objects[1]);
  lodestar::PairSearch pair(std::move(slots), 6, 2, &w.sys->image());
  for (int look = 0; look < 2; ++look) {
    pair.ask();
    if (look == 1) {
      for (std::uint64_t object : {objects[0], objects[1]}) {
        for (std::uint32_t off = 0; off + 4 <= 0x800; off += 4) w.mem.put<std::uint32_t>(object + off, 2u);
      }
    }
    bool through = false;
    for (int steps = 0; steps < 400'000 && !through; ++steps) {
      reader.begin_tick();
      through = pair.step(gt, reader, static_cast<std::uint64_t>(steps) * 1000);
    }
    REQUIRE(through);
  }
  const lodestar::PairFound found = pair.found();
  CHECK(found.candidates == lodestar::PairSearch::kMaxCandidates);
  CHECK(found.hits.size() == lodestar::PairSearch::kMaxHits);
  CHECK(found.word_hits > 1000);
}

TEST_CASE("in game: a pass takes over a manager the search named first, and an invariant's static enters the table") {
  FakeWorld w;
  Round r = round_of(w);
  const lodestar::Namer namer(r.gated.image, w.sys->image(), r.leads, r.ids);
  const std::uint32_t camera = roles::info(Item::Camera).id;
  const std::uint64_t eye = w.instance("camera");
  const std::uint32_t slot = static_cast<std::uint32_t>(w.at("st_camera") - w.base);
  // The camera's static is empty at the first pass; the search then names it by that very slot; the next pass finds
  // the object there: the pass's finding (the accessor's static, its sites and witnesses) takes the entry over.
  lodestar::InstancesFound held;
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  lodestar::SlotSearchResult result;
  result.slots = {slot};
  result.objects = 1;
  result.vtable = static_cast<std::uint32_t>(w.vtable("eye") - w.base);
  result.candidates = 50;
  result.checked = 50;
  lodestar::set_by_search(held, camera, result);
  const auto of = [&] {
    return *std::find_if(held.managers.begin(), held.managers.end(), [&](const auto& m) { return m.id == camera; });
  };
  CHECK(of().by_search);
  CHECK(of().witnesses.empty());
  w.mem.put<std::uint64_t>(w.at("st_camera"), eye);
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  CHECK(of().status == Status::Derived);
  CHECK_FALSE(of().by_search);
  CHECK(of().rva == slot);
  CHECK_FALSE(of().witnesses.empty());
  CHECK(of().pass == 2);
  CHECK(of().search_found == 1);  // the search's numbers stay
  CHECK(held.named == 11);
  // Where a later pass does not find it (the game closing), what the search named stays.
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);
  lodestar::InstancesFound other;
  lodestar::merge(other, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  lodestar::set_by_search(other, camera, result);
  lodestar::merge(other, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  const auto kept = std::find_if(other.managers.begin(), other.managers.end(), [&](const auto& m) { return m.id == camera; });
  CHECK(kept->by_search);
  CHECK(kept->status == Status::Derived);

  // An invariant's static: the save's slot, by the field that rose by the clock, with the code's references to it.
  const std::uint32_t save = static_cast<std::uint32_t>(w.at("st_save_data") - w.base);
  const std::vector<std::uint32_t> slots = {save};
  const std::vector<lodestar::References> refs = lodestar::references(r.gated.image, w.sys->image(), ".text", slots, 8);
  REQUIRE(refs[0].count >= 1);
  lodestar::InvariantFound v;
  v.label = "save_data";
  v.id = roles::info(Item::SaveData).id;
  v.rva = save;
  v.offset = 0x10;
  v.rose = 30;
  v.passes = 32;
  v.sites = static_cast<std::uint32_t>(refs[0].count);
  v.witnesses = refs[0].places;
  r.found.instances = held;
  r.found.invariants.push_back(v);
  const TableProvenance provenance{"0.1.0-test.1", std::string(64, 'a'), "l5", "2026-01-02", {}};
  const lodestar::DraftTable draft = lodestar::draft_table(r.found, r.ids, provenance, r.gated.image, w.sys->image());
  const std::size_t at = draft.text.find("[item.save_data]");
  REQUIRE(at != std::string::npos);
  const std::string entry = draft.text.substr(at, draft.text.find("[item.", at + 5) - at);
  CHECK(entry.find("found_by = [\"invariant\"]") != std::string::npos);
  CHECK(entry.find("witnesses = [") != std::string::npos);
  CHECK(entry.find("status = \"lead\"") != std::string::npos);
  CHECK(draft.left_out == 0);
  const auto text = lodestar::write_report(r.found);
  REQUIRE(text.has_value());
  CHECK(text->find("\"invariants\": [") != std::string::npos);
  // One the catalogue does not hold under that number is left out of the table.
  r.found.invariants[0].id = 4242;
  CHECK(lodestar::draft_table(r.found, r.ids, provenance, r.gated.image, w.sys->image()).text.find("[item.save_data]") ==
        std::string::npos);
}
