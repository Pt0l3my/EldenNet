// The gates (NPC §5.2 to §5.4, §5.8; DESIGN §8.7; ARCH §6.8): each clause alone, the target order, the fail-closed
// observer, the refusals' toasts, and no request while gated (C4). All data is invented.
#include <doctest.h>

#include <cmath>
#include <string>

#include "rig.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

namespace {

Surroundings safe() {
  Surroundings w;
  w.active = true;
  w.caps_conversation = true;
  return w;
}

}  // namespace

TEST_CASE("safe_context: each clause alone, in order") {
  CHECK_FALSE(safe_context(safe()).has_value());
  auto w = safe();
  w.active = false;
  CHECK(safe_context(w) == Unsafe::NotActive);
  w = safe();
  w.player_dead = true;
  CHECK(safe_context(w) == Unsafe::Player);
  w = safe();
  w.riding = true;
  CHECK(safe_context(w) == Unsafe::Player);
  w = safe();
  w.boss = true;
  CHECK(safe_context(w) == Unsafe::Boss);
  w = safe();
  w.combat = true;
  w.sheltered = true;
  CHECK(safe_context(w) == Unsafe::Combat);  // never waived
  w = safe();
  w.engaged_hostile_m = 24.0f;
  CHECK(safe_context(w) == Unsafe::Hostiles);
  w.engaged_hostile_m = 26.0f;
  CHECK_FALSE(safe_context(w).has_value());
  w = safe();
  w.hostile_m = 12.0f;
  CHECK(safe_context(w) == Unsafe::Hostiles);
  w.hostile_m = 20.0f;
  CHECK_FALSE(safe_context(w).has_value());
  w.caps_combat = false;
  CHECK(safe_context(w) == Unsafe::Hostiles);  // stricter without the combat group
  w.sheltered = true;
  CHECK_FALSE(safe_context(w).has_value());  // sheltered waives the distance checks
  w = safe();
  w.menu = true;
  CHECK(safe_context(w) == Unsafe::Menu);
  w.menu = false;
  w.banner = true;  // O108: a banner fails the opening too
  CHECK(safe_context(w) == Unsafe::Menu);
  w.active = false;
  CHECK(safe_context(w) == Unsafe::NotActive);  // the first that fails
  CHECK(std::string(to_string(Unsafe::Hostiles)) == "hostiles");
}

TEST_CASE("conversable: each clause alone, in order") {
  auto pack = load_content(rig_content());
  REQUIRE(pack.has_value());
  const Persona& keeper = *pack->persona("lantern_keeper");
  proto::Chr c = character(kKeeper, 81100, 2.0f);
  PartnerFacts p;
  p.persona = &keeper;
  p.chr = &c;
  const Surroundings w = safe();
  CHECK_FALSE(conversable(p, w, 100000).has_value());
  c.dead = true;
  CHECK(conversable(p, w, 100000) == NotConversable::Dead);
  c.dead = false;
  p.hostile = true;
  CHECK(conversable(p, w, 100000) == NotConversable::Hostile);
  p.hostile = false;
  p.attacked_at = 80000;
  CHECK(conversable(p, w, 100000) == NotConversable::RecentlyAttacked);
  CHECK_FALSE(conversable(p, w, 110000).has_value());  // 30 s on
  p.attacked_at.reset();
  auto native = w;
  native.native = true;
  CHECK(conversable(p, native, 100000) == NotConversable::NativeTalk);
  p.summon = true;
  CHECK(conversable(p, w, 100000) == NotConversable::Busy);
  p.summon = false;
  p.holds_boss_bar = true;
  CHECK(conversable(p, w, 100000) == NotConversable::Busy);
  p.holds_boss_bar = false;
  auto blind = w;
  blind.caps_conversation = false;
  CHECK(conversable(p, blind, 100000) == NotConversable::Unavailable);
}

TEST_CASE("conversable: the stage's chat and state") {
  const std::string asleep = std::string(kWrenRecord) + "[[stages]]\nid = \"night\"\nwhen = \"runes >= 1\"\nstate = \"asleep\"\n";
  auto pack = load_content(rig_content(asleep));
  REQUIRE(pack.has_value());
  const Persona& wren = *pack->persona("wren");
  proto::Chr c = character(kWren, 4100, 2.0f);
  PartnerFacts p;
  p.persona = &wren;
  p.chr = &c;
  p.stage = 1;
  CHECK(conversable(p, safe(), 0) == NotConversable::Asleep);
  const std::string refusing = std::string(kWrenRecord) + "[[stages]]\nid = \"sulk\"\nwhen = \"runes >= 1\"\nchat = \"refuse\"\n";
  auto pack2 = load_content(rig_content(refusing));
  REQUIRE(pack2.has_value());
  p.persona = pack2->persona("wren");
  CHECK(conversable(p, safe(), 0) == NotConversable::Stage);
}

TEST_CASE("the refusals at the talk key give their toasts, or none") {
  SUBCASE("a refusing stage") {
    Rig r(std::string(kWrenRecord) + "[[stages]]\nid = \"sulk\"\nwhen = \"runes >= 1\"\nchat = \"refuse\"\n");
    r.facts.runes = 5;
    r.remove(kKeeper);
    r.add(character(kWren, 4100, 2.0f));
    r.wait(3250);
    r.open();
    CHECK(r.conv.state() == proto::ConvState::Closed);
    CHECK(r.toasted("(Wren will not speak with you)"));
  }
  SUBCASE("asleep") {
    Rig r(std::string(kWrenRecord) + "[[stages]]\nid = \"night\"\nwhen = \"runes >= 1\"\nstate = \"asleep\"\n");
    r.facts.runes = 5;
    r.remove(kKeeper);
    r.add(character(kWren, 4100, 2.0f));
    r.wait(3250);
    r.open();
    CHECK(r.toasted("(Wren is asleep.)"));
  }
  SUBCASE("danger near") {
    Rig r;
    proto::Chr wolf = character(kWolf, 9900, 8.0f);
    wolf.team = 6;
    r.add(wolf);
    r.wait(3250);
    CHECK_FALSE(r.last.ui.hint.has_value());  // the key would not open chat: no hint
    r.open();
    CHECK(r.conv.state() == proto::ConvState::Closed);
    CHECK(r.toasted("Not now: danger is near."));
  }
  SUBCASE("nothing in range: no response") {
    Rig r;
    r.keeper(9.0f, 60.0f);
    r.wait(3250);
    r.open();
    CHECK(r.conv.state() == proto::ConvState::Closed);
    CHECK(r.toasts.empty());
  }
  SUBCASE("struck by the Tarnished within 30 s: no generated speech, and no word") {
    Rig r;
    r.keeper(2.5f).last_hit_by = kPlayer;
    r.wait(3250);
    r.open();
    CHECK(r.conv.state() == proto::ConvState::Closed);
    CHECK(r.toasts.empty());
  }
}

TEST_CASE("the fail-closed observer: the HUD at Default, 3 s free of native talk, 3 s since the persona came near") {
  SUBCASE("the HUD") {
    Rig r;
    r.snap.ui.hud = proto::HudState::ShowAll;
    r.wait(4000);
    r.open();
    CHECK(r.conv.state() == proto::ConvState::Closed);
  }
  SUBCASE("native talk within the last 3 s") {
    Rig r;
    r.wait(3250);
    r.snap.ui.popup_talk_id = 42;
    r.wait(250);
    r.snap.ui.popup_talk_id.reset();
    r.wait(2500);
    r.open();
    CHECK(r.conv.state() == proto::ConvState::Closed);
    r.wait(750);
    r.open();
    CHECK(r.conv.state() == proto::ConvState::Open);
  }
  SUBCASE("the persona came within reach less than 3 s ago") {
    Rig r;
    r.keeper(20.0f);
    r.wait(5000);
    r.keeper(2.0f);
    r.wait(2750);
    r.open();
    CHECK(r.conv.state() == proto::ConvState::Closed);
    r.wait(500);
    r.open();
    CHECK(r.conv.state() == proto::ConvState::Open);
  }
}

TEST_CASE("the target order: the lock-on, then the cone, then the nearest; a new best leads 500 ms in the hint") {
  Rig r;
  r.keeper(4.0f, 40.0f);                       // outside the cone, within 5 m
  r.add(character(kWren, 4100, 7.0f, 10.0f));  // in the cone
  r.wait(3500);
  REQUIRE(r.last.ui.hint.has_value());
  CHECK(r.last.ui.hint->target == kWren);  // rule 4 before rule 5
  r.snap.health.of(proto::Group::Camera).state = proto::GroupState::Tripped;  // no cone without the camera
  r.wait(250);
  CHECK(r.last.ui.hint->target == kWren);  // the new best has not led 500 ms yet
  r.wait(500);
  CHECK(r.last.ui.hint->target == kKeeper);
  r.snap.player->locked_on = kWren;  // rule 3
  r.wait(750);
  CHECK(r.last.ui.hint->target == kWren);
  r.open();
  CHECK(r.conv.partner() == "wren");
}

TEST_CASE("C4: no request while gated, and none in a session whose gate never opens") {
  Rig r;
  r.open_box();
  r.speech = false;  // the ladder below L3: the box closes, and a line sends nothing
  r.wait(250);
  CHECK_FALSE(r.conv.box_open());
  r.speech = true;
  r.wait(250);
  r.open();
  REQUIRE(r.conv.box_open());
  r.speech = false;
  r.say("Is anyone there?");  // the step's own check, before the snapshot has closed the box
  CHECK(r.requests.empty());
  CHECK(r.toasted("No answer: no model is ready"));
}

TEST_CASE("the cancels: a cut-off, a regeneration and an ending each cancel the live request") {
  Rig r;
  r.open_box();
  r.say("One.");
  r.delta("Yes. And more. ");
  r.say("Two.");  // a cut-off
  REQUIRE(r.cancels.size() == 1);
  CHECK(r.cancels[0] == r.requests[0].id);
  r.delta("BANNED. ");  // a regeneration
  REQUIRE(r.cancels.size() == 2);
  CHECK(r.cancels[1] == r.requests[1].id);
  r.dormant = true;  // an ending, at once
  r.step();
  REQUIRE(r.cancels.size() == 3);
  CHECK(r.cancels[2] == r.requests[2].id);
  for (std::size_t i = 1; i < r.requests.size(); ++i) CHECK(r.requests[i].id == r.requests[i - 1].id + 1);
}
