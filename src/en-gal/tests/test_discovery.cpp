// Discovery in game over FakeSys (discovery.hpp; ARCH §3.2, §6.2): the gate, the AOB items, the FD4 singletons by
// their class objects or by the getter, the RTTI sets, the probe round's slots, the groups, and the retries. The world
// and its gal/ texts are invented (fake_world.hpp).
#include <doctest.h>

#include <algorithm>

#include "rig_gal.hpp"

using namespace en::gal;
using namespace en::gal::test;
using en::proto::Group;
using roles::Cls;
using roles::Item;

namespace {

std::string replaced(std::string text, std::string_view from, std::string_view to) {
  const std::size_t at = text.find(from);
  REQUIRE(at != std::string::npos);
  text.replace(at, from.size(), to);
  return text;
}

}  // namespace

TEST_CASE("discovery: the gate passes only the build's exact fingerprint") {
  FakeWorld w;
  CHECK(gate_image(*w.sys, w.builds).has_value());
  const auto other = gate_image(*w.sys, replaced(w.builds, "time_date_stamp = 0x12345678", "time_date_stamp = 0x1"));
  REQUIRE_FALSE(other.has_value());
  CHECK(other.error().what == GateFailure::What::Gate);
  CHECK(other.error().text.find("fingerprint differs") != std::string::npos);
  CHECK(gate_image(*w.sys, "schema = 1\n[!]\n").error().what == GateFailure::What::BuildsFile);
}

TEST_CASE("discovery: every item, the singletons by name, the RTTI sets and every group of the world") {
  FakeWorld w;
  const Discovery d = discovered(w);
  CHECK(d.ready());
  const DiscoveryReport& r = d.report();
  for (std::size_t i = 0; i < roles::kItemCount; ++i) {
    INFO(roles::info(static_cast<Item>(i)).label);
    CHECK(r.items[i].status == ItemStatus::Resolved);
  }
  CHECK(d.address(Item::RegisterTask) == w.at("register_task"));
  for (const char* item : {"task_hub", "characters", "session", "event_flags", "hud", "menus", "loading", "world_clock",
                           "camera", "param_store", "messages", "save_data", "game_state", "field_area",
                           "module_handle"}) {
    INFO(item);
    const auto it = std::find_if(r.items.begin(), r.items.end(), [&](const ItemResult& x) {
      return x.address == w.at(std::string("st_") + item);
    });
    CHECK(it != r.items.end());
  }
  CHECK(r.fd4_names == 11);
  CHECK(r.getter_calls == 0);  // the name layout is confirmed: no game function is called
  // The characters' set holds the base and every class deriving from it; the player's its own class alone.
  const VtableSet& chr = d.vtables(Cls::Character);
  for (const char* c : {"chr", "hero", "npc", "beast"}) CHECK(chr.contains(w.vtable(c)));
  CHECK_FALSE(chr.contains(w.vtable("prop")));
  CHECK_FALSE(chr.contains(w.vtable("tomb")));
  CHECK(d.vtables(Cls::PlayerCharacter).all().size() == 1);
  CHECK(d.vtables(Cls::PlayerCharacter).contains(w.vtable("hero")));
  CHECK(d.vtables(Cls::Hud).contains(w.vtable("hud")));
  CHECK(d.vtables(Cls::Vitals).empty());  // no RTTI declared: no check
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
    INFO(en::proto::to_string(static_cast<Group>(g)));
    CHECK(r.groups[g] == (static_cast<Group>(g) == Group::Status ? GroupWhy::Off : GroupWhy::None));
  }
}

TEST_CASE("discovery: without a confirmed name layout, the getter is called once for each candidate, at init only") {
  FakeWorld w(WorldOptions{false});
  const Discovery d = discovered(w);
  CHECK(d.ready());
  CHECK(d.report().getter_calls == 11);
  CHECK(w.sys->getter_calls == 11);
  CHECK(d.address(Item::Camera) == w.at("st_camera"));
  // After registration the fallback is refused (ARCH §6.2: init only).
  FakeIo io;
  GalTask task(*w.sys, d, io);
  REQUIRE(install_task(*w.sys, d, task).has_value());
  CHECK(w.sys->call_getter(1, 2).error() == SysError::NotInit);
}

TEST_CASE("discovery: the probe round's slot stands for a static that has no pattern yet") {
  FakeWorld w;
  const std::size_t a = w.patterns.find("[[pattern]]\nid = \"save_data\"");
  REQUIRE(a != std::string::npos);
  const std::size_t b = w.patterns.find("[[pattern]]", a + 5);
  std::string no_save = w.patterns;
  no_save.erase(a, b - a);
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  // Without the pattern or a slot, save_data is required and missing: patterns.toml itself is refused.
  CHECK_FALSE(discover(*w.sys, *gated, no_save, w.layout).has_value());
  DiscoveryOptions with_slot;
  with_slot.static_slots.emplace_back("save_data", static_cast<std::uint32_t>(w.at("st_save_data") - w.base));
  const auto d = discover(*w.sys, *gated, no_save, w.layout, with_slot);
  REQUIRE(d.has_value());
  CHECK(d->ready());
  CHECK(d->report().items[roles::index(Item::SaveData)].from_slot);
  CHECK(d->address(Item::SaveData) == w.at("st_save_data"));
  DiscoveryOptions in_code;
  in_code.static_slots.emplace_back("save_data", static_cast<std::uint32_t>(w.at("register_task") - w.base));
  const auto refused = discover(*w.sys, *gated, no_save, w.layout, in_code);
  REQUIRE(refused.has_value());
  CHECK(refused->report().items[roles::index(Item::SaveData)].status == ItemStatus::Refused);
  CHECK_FALSE(refused->ready());
}

TEST_CASE("discovery: a required item missing is retried at +5, +15 and +30 s, then fails") {
  FakeWorld w;
  const std::string broken =
      replaced(w.patterns, "48 89 CB 89 D6 41 89 70 08 49 89 58 10 31 C0", "48 89 CB 89 D6 41 89 70 08 49 89 58 10 31 C1");
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  std::vector<std::uint64_t> waits;
  const Retried r = discover_with_retries(*w.sys, *gated, broken, w.layout, {},
                                          [&](std::uint64_t t) { waits.push_back(t); });
  REQUIRE(r.last.has_value());
  CHECK(r.passes == 4);
  CHECK(waits == std::vector<std::uint64_t>{5'000, 15'000, 30'000});
  CHECK_FALSE(r.last->ready());
  CHECK(std::string(r.last->report().failure) == "a required item is missing");
  CHECK(r.last->report().items[roles::index(Item::RegisterTask)].status == ItemStatus::Missing);
  // Found at once: one pass.
  waits.clear();
  const Retried ok = discover_with_retries(*w.sys, *gated, w.patterns, w.layout, {}, [&](std::uint64_t t) { waits.push_back(t); });
  CHECK(ok.passes == 1);
  CHECK(waits.empty());
}

TEST_CASE("discovery: an optional item missing turns its group off; a structural one stops it") {
  FakeWorld w;
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  const auto no_camera = discover(*w.sys, *gated, w.patterns, replaced(w.layout, "singleton = \"Eye\"\n", ""));
  REQUIRE(no_camera.has_value());
  CHECK(no_camera->ready());
  CHECK(no_camera->report().groups[static_cast<std::size_t>(Group::Camera)] == GroupWhy::NotDiscovered);
  // A field of the wrong kind: the group cannot run.
  const auto bad_field = discover(*w.sys, *gated, w.patterns,
                                  replaced(w.layout, "hour = { off = 0x10, ty = \"u8\"", "hour = { off = 0x10, ty = \"f32\""));
  REQUIRE(bad_field.has_value());
  CHECK(bad_field->report().groups[static_cast<std::size_t>(Group::Place)] == GroupWhy::NotDiscovered);
  // The session's singleton missing: required, so nothing is registered.
  const auto no_session = discover(*w.sys, *gated, w.patterns, replaced(w.layout, "singleton = \"Lobby\"\n", ""));
  REQUIRE(no_session.has_value());
  CHECK_FALSE(no_session->ready());
  // A class whose RTTI the image does not hold: its group is off.
  const auto no_rtti = discover(*w.sys, *gated, w.patterns, replaced(w.layout, "rtti = \"W::Clock\"", "rtti = \"W::Nowhere\""));
  REQUIRE(no_rtti.has_value());
  CHECK(no_rtti->report().groups[static_cast<std::size_t>(Group::Place)] == GroupWhy::NotDiscovered);
  CHECK(no_rtti->report().rtti_missing == 1);
  // A layout refused (a release build refuses its leads) is an error, not a retry.
  CHECK_FALSE(discover(*w.sys, *gated, w.patterns, replaced(w.layout, "[class.grip]\ncopy_size = 0x4\nstatus = \"confirmed\"",
                                                            "[class.grip]\ncopy_size = 0x4\nstatus = \"lead\""))
                  .has_value());
  DiscoveryOptions dev;
  dev.mode = LayoutMode::Dev;
  const auto lead = discover(*w.sys, *gated, w.patterns,
                             replaced(w.layout, "[class.grip]\ncopy_size = 0x4\nstatus = \"confirmed\"",
                                      "[class.grip]\ncopy_size = 0x4\nstatus = \"lead\""),
                             dev);
  REQUIRE(lead.has_value());
  CHECK(lead->report().layout_has_leads);
}
