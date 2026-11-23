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
  const lodestar::TreeFound found = tree.found(flips);
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
