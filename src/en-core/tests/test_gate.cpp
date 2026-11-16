// The gate's predicate and hysteresis (ARCH §5.1).
#include <doctest.h>

#include "en/core/gate.hpp"
#include "snapshots.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

TEST_CASE("judge: a solo world of one's own and a quiet title are clean; each clause fails on its own") {
  CHECK(judge(world(0), false) == Verdict{true, Unclean::None});
  CHECK(judge(title(0), false) == Verdict{true, Unclean::None});

  auto s = world(0);
  s.gap = true;
  CHECK(judge(s, false).why == Unclean::Gap);
  CHECK(judge(world(0), true).why == Unclean::SessionFailed);
  s = world(0);
  s.health.of(proto::Group::Session).state = proto::GroupState::Untested;
  CHECK(judge(s, false).why == Unclean::SessionFailed);
  s = world(0);
  s.session.anticheat = true;
  CHECK(judge(s, false).why == Unclean::AntiCheat);

  // Each part of solo_world, alone.
  s = world(0);
  s.session.role = proto::MultiplayRole::Cooperator;
  CHECK(judge(s, false).why == Unclean::NotSolo);
  s = world(0);
  s.session.is_my_world = false;
  CHECK(judge(s, false).why == Unclean::NotSolo);
  s = world(0);
  s.session.lobby = proto::LobbyState::Hosting;
  CHECK(judge(s, false).why == Unclean::NotSolo);
  s = world(0);
  s.session.player_set_len = 2;
  CHECK(judge(s, false).why == Unclean::NotSolo);
  s = world(0);
  s.session.player_set_len = 0;
  CHECK(judge(s, false).why == Unclean::NotSolo);

  // Values not read are never clean.
  s = world(0);
  s.session.role = proto::MultiplayRole::Unknown;
  CHECK(judge(s, false).why == Unclean::NotSolo);
  s = world(0);
  s.session.lobby = proto::LobbyState::Unknown;
  CHECK(judge(s, false).why == Unclean::NotSolo);
  s = title(0);
  s.session.lobby = proto::LobbyState::Unknown;
  CHECK(judge(s, false).why == Unclean::Lobby);
  s = title(0);
  s.session.lobby = proto::LobbyState::Joined;
  CHECK(judge(s, false).why == Unclean::Lobby);
  s = title(0);
  s.life.screen = proto::Screen::Unknown;
  CHECK(judge(s, false).why == Unclean::Loading);
  CHECK(judge(loading(0), false).why == Unclean::Loading);

  // At the title no role is read, and none is asked for.
  s = title(0);
  s.session.role = proto::MultiplayRole::Unknown;
  s.session.is_my_world = false;
  CHECK(judge(s, false).clean);
  // The world's clause, not the title's, once the main player is there, whatever the screen says.
  s = world(0);
  s.life.screen = proto::Screen::Title;
  s.session.player_set_len = 2;
  CHECK(judge(s, false).why == Unclean::NotSolo);
}

TEST_CASE("the gate opens after 5,000 ms of clean snapshots, not before, and closes on the first unclean one") {
  Gate g;
  CHECK_FALSE(g.open());
  for (Mono t = 0; t < 5000; t += 250) {
    CHECK_FALSE(g.step(world(t)));
    CHECK_FALSE(g.open());
  }
  CHECK(g.step(world(5000)));  // held for exactly 5,000 ms
  CHECK(g.open());
  CHECK_FALSE(g.step(world(5250)));

  auto coop = world(5500);
  coop.session.player_set_len = 2;
  CHECK(g.step(coop));  // at once, on the same snapshot (C4)
  CHECK_FALSE(g.open());
  CHECK(g.why() == Unclean::NotSolo);
  CHECK_FALSE(g.clean_since().has_value());

  // Alone again: the badge's wait, 5 s from the first clean snapshot.
  for (Mono t = 5750; t < 10750; t += 250) CHECK_FALSE(g.step(world(t)));
  CHECK(g.clean_since() == Mono{5750});
  CHECK(g.step(world(10750)));
}

TEST_CASE("a gap between snapshots longer than 1,000 ms starts the hold again") {
  Gate g;
  for (Mono t = 0; t <= 4000; t += 250) g.step(world(t));
  CHECK_FALSE(g.open());
  // 1,000 ms exactly is no gap.
  g.step(world(5000));
  CHECK(g.open());

  Gate h;
  for (Mono t = 0; t <= 3000; t += 250) h.step(world(t));
  h.step(world(4001));  // 1,001 ms after the last: the hold starts here
  CHECK(h.clean_since() == Mono{4001});
  for (Mono t = 4250; t < 9001; t += 250) h.step(world(t));
  CHECK_FALSE(h.open());
  h.step(world(9001));
  CHECK(h.open());

  // A clock that goes back starts the hold again too.
  Gate k;
  for (Mono t = 10000; t <= 13000; t += 250) k.step(world(t));
  k.step(world(9000));
  CHECK(k.clean_since() == Mono{9000});
}

TEST_CASE("a gap snapshot closes the gate and restarts the hold") {
  Gate g;
  for (Mono t = 0; t <= 5000; t += 250) g.step(world(t));
  REQUIRE(g.open());
  auto s = world(5250);
  s.gap = true;
  CHECK(g.step(s));
  CHECK(g.why() == Unclean::Gap);
  g.step(world(5500));
  CHECK(g.clean_since() == Mono{5500});
}

TEST_CASE("a failed session is failed for the session, even if a later snapshot reads it Ok") {
  Gate g;
  for (Mono t = 0; t <= 5000; t += 250) g.step(world(t));
  REQUIRE(g.open());
  auto s = world(5250);
  s.health.of(proto::Group::Session).state = proto::GroupState::Tripped;
  CHECK(g.step(s));
  CHECK(g.session_failed());
  for (Mono t = 5500; t <= 20000; t += 250) g.step(world(t));
  CHECK_FALSE(g.open());
  CHECK(g.why() == Unclean::SessionFailed);

  Gate h;
  for (Mono t = 0; t <= 5000; t += 250) h.step(world(t));
  CHECK(h.session_failed_now());
  CHECK_FALSE(h.session_failed_now());  // already closed: no change
  h.step(world(5250));
  CHECK_FALSE(h.open());
}

TEST_CASE("the title clause: clean at a quiet title, closed through a loading screen, and the hold again after it") {
  Gate g;
  for (Mono t = 0; t <= 5000; t += 250) g.step(title(t));
  CHECK(g.open());
  CHECK(g.step(loading(5250)));
  CHECK(g.why() == Unclean::Loading);
  for (Mono t = 5500; t < 10500; t += 250) g.step(world(t));
  CHECK_FALSE(g.open());
  g.step(world(10500));
  CHECK(g.open());
}

TEST_CASE("the reasons' names") {
  CHECK(std::string(to_string(Unclean::None)) == "none");
  CHECK(std::string(to_string(Unclean::NotSolo)) == "not_solo");
  CHECK(std::string(to_string(Unclean::SessionFailed)) == "session_failed");
}
