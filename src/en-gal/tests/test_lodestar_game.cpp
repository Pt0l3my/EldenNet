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
  w.mem.put<std::uint64_t>(w.at("st_hud"), 0);
  w.mem.put<std::uint64_t>(w.at("st_camera"), 0);
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  CHECK(held.named == 9);
  const auto of = [&](std::string_view label) {
    const auto it = std::find_if(held.managers.begin(), held.managers.end(), [&](const auto& m) { return m.label == label; });
    REQUIRE(it != held.managers.end());
    return *it;
  };
  lodestar::set_by_search(held, hud, 0x1238, tables[0], 1);
  CHECK(held.named == 10);
  CHECK(of("hud").status == Status::Derived);
  CHECK(of("hud").by_search);
  CHECK(of("hud").rva == 0x1238);
  CHECK(of("hud").sites == 0);
  CHECK(of("hud").witnesses.empty());
  CHECK(of("hud").pass == 1);
  lodestar::set_by_search(held, roles::info(Item::Camera).id, 0x2000, 0x3000, 2);
  CHECK(of("camera").status == Status::Ambiguous);
  CHECK(of("camera").search_found == 2);
  CHECK(held.ambiguous == 1);
  // What the search named stays named through a later pass, and a manager a pass named is not touched by a search.
  lodestar::merge(held, namer.pass(w.sys->image(), r.found.accessor, reader_of(w)));
  CHECK(of("hud").by_search);
  CHECK(of("hud").rva == 0x1238);
  lodestar::set_by_search(held, roles::info(Item::Session).id, 0x4000, 0x5000, 1);
  CHECK_FALSE(of("session").by_search);
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
  r.found.rising = lodestar::RisingFound{12, 3, 0, {lodestar::RisingHit{0x1000, 0x20, 5, 2, 0}}};
  const auto report = lodestar::write_report(r.found);
  if (!report) MESSAGE(report::describe(report.error()));
  REQUIRE(report.has_value());
  CHECK(report->find("\"clock_hits\"") != std::string::npos);
  CHECK(report->find("\"rising_hits\"") != std::string::npos);
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
