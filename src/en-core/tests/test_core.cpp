// The slice's lifecycle, ladder and Dormant (ARCH §6.8, §9), driven as the shell drives it: C2's messages, then C1's
// snapshots, at 4 Hz.
#include <doctest.h>

#include <algorithm>
#include <string>

#include "en/core/core.hpp"
#include "snapshots.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

namespace {

bool has_phase_change(const Outputs& o, Phase from, Phase to) {
  return std::any_of(o.changes.begin(), o.changes.end(), [&](const Change& c) {
    return c.what == Change::What::Phase && c.from == static_cast<std::uint8_t>(from) &&
           c.to == static_cast<std::uint8_t>(to);
  });
}

bool tracks_none(const Outputs& o) {
  return std::any_of(o.game.begin(), o.game.end(), [](const proto::GameCommand& c) {
    const auto* t = std::get_if<proto::GameCommand::SetTracked>(&c.payload);
    return t != nullptr && t->handles.empty();
  });
}

// From boot to the title, then into a world: the usual start.
struct Session {
  Core core;
  Mono t = 0;

  Outputs snap(const proto::WorldSnapshot& s) {
    t = s.mono_ms;
    return core.step(in(s), t);
  }
  Outputs life(const proto::Lifecycle& l) { return core.step(in(l), t); }
  void to_title() {
    life(data_ready());
    for (int i = 0; i < 8; ++i) snap(title(t + 250));
  }
  // Enters the world: the PlaythroughReady first (C2 before C1), then snapshots until the play time has risen.
  Outputs to_world(const char* name = "Vessel") {
    life(playthrough_ready(3, name));
    snap(world(t + 250));
    return snap(world(t + 250));
  }
};

}  // namespace

TEST_CASE("boot: Gate, then DataReady makes Ready and L2; a quiet title opens the gate after 5 s but stays Dormant") {
  Core core;
  CHECK(core.state().phase == Phase::Gate);
  CHECK(core.state().level == Level::Starting);
  auto o = core.step(in(data_ready()), 100);
  CHECK(o.state.phase == Phase::Ready);
  CHECK(o.state.level == Level::L2);
  CHECK(core.caps().groups == proto::FlagSetTraits<proto::GroupSet>::all);
  CHECK_FALSE(o.ai_allowed);

  o = core.step(in(title(250)), 250);
  CHECK(o.state.phase == Phase::Title);
  CHECK(has_phase_change(o, Phase::Ready, Phase::Title));
  for (Mono t = 500; t < 5250; t += 250) o = core.step(in(title(t)), t);
  CHECK_FALSE(o.state.gate_open);
  o = core.step(in(title(5250)), 5250);
  CHECK(o.state.gate_open);
  CHECK(o.ai_allowed);     // the title clause: the wizard's and the self-test's calls
  CHECK(o.state.dormant);  // but no speech at the title (§6.8)
  CHECK_FALSE(o.state.badge);
}

TEST_CASE("Title → Playthrough → InWorld: needs the identity, phase B, a slot 0–9 and the play time rising") {
  Session s;
  s.to_title();
  // No PlaythroughReady yet: the world alone is not enough.
  auto o = s.snap(world(s.t + 250));
  o = s.snap(world(s.t + 250));
  CHECK(o.state.phase == Phase::Title);

  s.life(playthrough_ready(3, "Vessel"));
  // Phase B not yet passed.
  auto w = world(s.t + 250, 700000);
  w.health = healthy(false);
  o = s.snap(w);
  CHECK(o.state.phase == Phase::Title);
  // The play time not rising (the same on two snapshots).
  o = s.snap(world(s.t + 250, 700000));
  CHECK(o.state.phase == Phase::Title);
  // A slot the identity does not name.
  w = world(s.t + 250);
  w.player->save_slot = 4;
  o = s.snap(w);
  CHECK(o.state.phase == Phase::Title);
  // All present (the play time rose since the last snapshot): the playthrough starts, and with no store the world
  // follows on the same step.
  o = s.snap(world(s.t + 250));
  CHECK(o.state.phase == Phase::InWorld);
  CHECK(has_phase_change(o, Phase::Title, Phase::Playthrough));
  CHECK(has_phase_change(o, Phase::Playthrough, Phase::InWorld));
  CHECK(o.state.playthroughs == 1);
  REQUIRE(s.core.playthrough().has_value());
  CHECK(s.core.playthrough()->slot == 3);
  CHECK(s.core.playthrough()->name == std::string_view("Vessel"));
}

TEST_CASE("a slot outside 0–9 or an empty name never starts a playthrough") {
  Session s;
  s.to_title();
  s.life(playthrough_ready(12, "Vessel"));
  auto w = world(s.t + 250);
  w.player->save_slot = 12;
  s.snap(w);
  w = world(s.t + 250);
  w.player->save_slot = 12;
  CHECK(s.snap(w).state.phase == Phase::Title);

  Session e;
  e.to_title();
  e.life(playthrough_ready(3, ""));
  e.snap(world(e.t + 250));
  CHECK(e.snap(world(e.t + 250)).state.phase == Phase::Title);
}

TEST_CASE("in the world: Dormant until the gate has held 5 s; then speech; co-op closes it at once with the badge") {
  Session s;
  s.to_title();
  auto o = s.to_world();
  REQUIRE(o.state.phase == Phase::InWorld);
  CHECK(o.state.dormant);
  for (int i = 0; i < 24; ++i) o = s.snap(world(s.t + 250));
  CHECK(o.state.gate_open);
  CHECK_FALSE(o.state.dormant);
  CHECK(o.ai_allowed);

  auto coop = world(s.t + 250);
  coop.session.player_set_len = 2;
  o = s.snap(coop);
  CHECK_FALSE(o.state.gate_open);
  CHECK(o.state.dormant);
  CHECK_FALSE(o.ai_allowed);
  CHECK(o.state.badge);
  CHECK(tracks_none(o));
  CHECK(std::count_if(o.game.begin(), o.game.end(), [](const auto&) { return true; }) == 1);
  CHECK(std::any_of(o.changes.begin(), o.changes.end(), [](const Change& c) {
    return c.what == Change::What::GateClosed && c.from == static_cast<std::uint8_t>(Unclean::NotSolo);
  }));

  // Alone again: the badge stays until the 5 s window completes.
  for (int i = 0; i < 20; ++i) {
    o = s.snap(world(s.t + 250));
    CHECK(o.state.badge);
  }
  o = s.snap(world(s.t + 250));
  CHECK(o.state.gate_open);
  CHECK_FALSE(o.state.badge);
  CHECK_FALSE(o.state.dormant);
}

TEST_CASE("a loading screen: Loading at once (SetTracked{}), back in the same world once the counter holds") {
  Session s;
  s.to_title();
  s.to_world();
  for (int i = 0; i < 24; ++i) s.snap(world(s.t + 250));
  REQUIRE(s.core.state().gate_open);

  auto o = s.snap(loading(s.t + 250, 8));
  CHECK(o.state.phase == Phase::Loading);
  CHECK(tracks_none(o));
  CHECK(o.game.size() == 1);  // one SetTracked{} for the world left and the gate closed together
  CHECK(o.state.dormant);
  CHECK_FALSE(o.state.badge);  // a loading screen raises no multiplayer badge
  // The new world: the counter must hold over two snapshots, and the same character returns.
  s.life(playthrough_ready(3, "Vessel"));
  o = s.snap(world(s.t + 250, 0, 8));
  CHECK(o.state.phase == Phase::Loading);
  o = s.snap(world(s.t + 250, 0, 8));
  CHECK(o.state.phase == Phase::InWorld);
  CHECK(o.state.playthroughs == 1);
  CHECK_FALSE(o.state.badge);
  CHECK(o.state.dormant);  // the gate's hold again after the loading screen
}

TEST_CASE("InWorld → Loading on each of its signs: no main player, the counter moved, the screen not InGame") {
  for (int sign = 0; sign < 3; ++sign) {
    Session s;
    s.to_title();
    s.to_world();
    auto w = world(s.t + 250);
    if (sign == 0) w.life.main_player = false;
    if (sign == 1) w.life.load_counter = 99;
    if (sign == 2) w.life.screen = proto::Screen::Loading;
    CHECK(s.snap(w).state.phase == Phase::Loading);
  }
}

TEST_CASE("Loading → Playthrough: another character comes back from the loading screen") {
  Session s;
  s.to_title();
  s.to_world("Vessel");
  s.snap(loading(s.t + 250, 9));
  s.life(playthrough_ready(3, "Wanderer"));
  s.snap(world(s.t + 250, 0, 9));
  auto o = s.snap(world(s.t + 250, 0, 9));
  CHECK(has_phase_change(o, Phase::Loading, Phase::Playthrough));
  CHECK(o.state.phase == Phase::InWorld);
  CHECK(o.state.playthroughs == 2);
  CHECK(s.core.playthrough()->name == std::string_view("Wanderer"));
}

TEST_CASE("Loading with no new identity: the same slot and class return to the world; another waits for one") {
  Session s;
  s.to_title();
  s.to_world();
  s.snap(loading(s.t + 250, 9));
  s.snap(world(s.t + 250, 0, 9));
  CHECK(s.snap(world(s.t + 250, 0, 9)).state.phase == Phase::InWorld);

  Session d;
  d.to_title();
  d.to_world();
  d.snap(loading(d.t + 250, 9));
  auto w = world(d.t + 250, 0, 9);
  w.player->archetype = 9;
  d.snap(w);
  w.mono_ms = d.t + 250;
  CHECK(d.snap(w).state.phase == Phase::Loading);
}

TEST_CASE("back to the title: the playthrough ends, and the next is a new one") {
  Session s;
  s.to_title();
  s.to_world();
  auto o = s.snap(title(s.t + 250));
  CHECK(o.state.phase == Phase::Title);
  CHECK(tracks_none(o));
  CHECK_FALSE(s.core.playthrough().has_value());
  // The same character again counts as a new playthrough: the store would open its database again.
  o = s.to_world();
  CHECK(o.state.phase == Phase::InWorld);
  CHECK(o.state.playthroughs == 2);

  // And from a loading screen too.
  s.snap(loading(s.t + 250));
  CHECK(s.snap(title(s.t + 250)).state.phase == Phase::Title);
}

TEST_CASE("very late load: Ready with the main player present goes to the world; during a loading screen to Loading") {
  Session s;
  s.life(data_ready());
  s.life(playthrough_ready());
  s.snap(world(250));
  auto o = s.snap(world(500));
  CHECK(has_phase_change(o, Phase::Ready, Phase::Playthrough));
  CHECK(o.state.phase == Phase::InWorld);

  Session l;
  l.life(data_ready());
  CHECK(l.snap(loading(250)).state.phase == Phase::Loading);
  l.life(playthrough_ready());
  l.snap(world(500, 0, 8));
  CHECK(l.snap(world(750, 0, 8)).state.phase == Phase::InWorld);
}

TEST_CASE("T1's banners: L1, the banner kept, the gate's effects off; the texts") {
  Core core;
  auto o = core.step(CoreInput{BootBanner{BannerReason::DiscoveryFailed, "a required item was not found"}}, 10);
  CHECK(o.state.phase == Phase::Banner);
  CHECK(o.state.level == Level::L1);
  CHECK(o.state.banner == BannerReason::DiscoveryFailed);
  CHECK(core.banner_detail() == "a required item was not found");
  // Nothing more happens: snapshots (should any arrive) change neither the phase nor the level.
  for (Mono t = 250; t <= 8000; t += 250) o = core.step(in(world(t)), t);
  CHECK(o.state.phase == Phase::Banner);
  CHECK(o.state.level == Level::L1);
  CHECK_FALSE(o.ai_allowed);

  CHECK(banner_text(BannerReason::UnknownBuild, "2.0.0.1", "") ==
        "EldenNet inactive: game build 2.0.0.1 not supported yet");
  CHECK(banner_text(BannerReason::DiscoveryFailed, "2.0.0.1", "a required item was not found") ==
        "EldenNet inactive: game build 2.0.0.1 not supported yet (reason: a required item was not found)");
  CHECK(banner_text(BannerReason::SessionFailed, "2.0.0.1", "") ==
        "EldenNet inactive: it could not confirm this is an offline solo session (game build 2.0.0.1).");
  CHECK(banner_text(BannerReason::AntiCheat, "", "").rfind("EldenNet inactive: Easy Anti-Cheat is running.", 0) == 0);
  CHECK(banner_text(BannerReason::TaskTripped, "", "") ==
        "EldenNet stopped reading the game after an error. Your game is not affected.");
  CHECK(banner_text(BannerReason::NotStarted, "", "") == "EldenNet inactive: the game did not finish starting");
  CHECK(banner_text(BannerReason::SpikeRun, "2.0.0.1", "s0e") == "Spike run: EldenNet makes no call into the game");
  CHECK(std::string(to_string(BannerReason::SpikeRun)) == "spike_run");
}

TEST_CASE("a spike run's banner (S0-E, S4): L1 at once, as any of T1's banners") {
  Core core;
  auto o = core.step(CoreInput{BootBanner{BannerReason::SpikeRun, "s4"}}, 10);
  CHECK(o.state.phase == Phase::Banner);
  CHECK(o.state.level == Level::L1);
  CHECK(o.state.banner == BannerReason::SpikeRun);
  CHECK(core.banner_detail() == "s4");
  CHECK_FALSE(o.ai_allowed);
}

TEST_CASE("a structural trip is L1 for the session; an optional one changes neither level nor gate") {
  Session s;
  s.to_title();
  s.to_world();
  for (int i = 0; i < 24; ++i) s.snap(world(s.t + 250));
  REQUIRE(s.core.state().gate_open);

  auto o = s.life(tripped(proto::Group::Camera));
  CHECK(o.state.level == Level::L2);
  CHECK(o.ai_allowed);
  CHECK_FALSE(o.state.banner.has_value());

  o = s.life(tripped(proto::Group::World));
  CHECK(o.state.level == Level::L1);
  CHECK(o.state.banner == BannerReason::TaskTripped);
  CHECK_FALSE(o.ai_allowed);
  // The gate may still read clean, but L1 keeps every network call off.
  o = s.snap(world(s.t + 250));
  CHECK(o.state.gate_open);
  CHECK_FALSE(o.ai_allowed);
}

TEST_CASE("the session group tripping closes the gate for good, with its banner") {
  Session s;
  s.to_title();
  s.to_world();
  for (int i = 0; i < 24; ++i) s.snap(world(s.t + 250));
  auto o = s.life(tripped(proto::Group::Session));
  CHECK_FALSE(o.state.gate_open);
  CHECK(o.state.banner == BannerReason::SessionFailed);
  CHECK(tracks_none(o));
  for (int i = 0; i < 40; ++i) o = s.snap(world(s.t + 250));
  CHECK_FALSE(o.state.gate_open);
  CHECK(o.state.gate_why == Unclean::SessionFailed);
}

TEST_CASE("C2 lost: DataReady and a trip are rebuilt from the snapshots' health") {
  Core core;
  auto o = core.step(in(title(250)), 250);
  CHECK(o.state.phase == Phase::Title);  // Gate → Ready → Title on one snapshot
  CHECK(o.state.level == Level::L2);
  CHECK(has_phase_change(o, Phase::Gate, Phase::Ready));

  auto t = title(500);
  t.health.of(proto::Group::Player).state = proto::GroupState::Tripped;
  o = core.step(in(t), 500);
  CHECK(o.state.level == Level::L1);
  CHECK(o.state.banner == BannerReason::TaskTripped);

  // Before phase A has passed nothing is rebuilt.
  Core early;
  auto u = title(250);
  u.health.of(proto::Group::World).state = proto::GroupState::Untested;
  CHECK(early.step(in(u), 250).state.phase == Phase::Gate);
}

TEST_CASE("a later anti-cheat hit: banner, L1, the gate closed") {
  Session s;
  s.to_title();
  s.to_world();
  for (int i = 0; i < 24; ++i) s.snap(world(s.t + 250));
  auto w = world(s.t + 250);
  w.session.anticheat = true;
  auto o = s.snap(w);
  CHECK(o.state.banner == BannerReason::AntiCheat);
  CHECK(o.state.level == Level::L1);
  CHECK_FALSE(o.state.gate_open);
  CHECK_FALSE(o.state.badge);  // the banner says it; no badge besides
}

TEST_CASE("a null snapshot and a tick change nothing") {
  Session s;
  s.to_title();
  const State before = s.core.state();
  auto o = s.core.step(CoreInput{std::unique_ptr<proto::WorldSnapshot>{}}, 99999);
  CHECK(o.state == before);
  o = s.core.step(CoreInput{CoreInput::Tick{}}, 99999);
  CHECK(o.state == before);
  CHECK(o.changes.empty());
}

TEST_CASE("names") {
  CHECK(std::string(to_string(Phase::InWorld)) == "in_world");
  CHECK(std::string(to_string(Level::L2)) == "L2");
  CHECK(std::string(to_string(BannerReason::SessionFailed)) == "session_failed");
  CHECK(std::string(to_string(Change::What::GateClosed)) == "gate_closed");
}
