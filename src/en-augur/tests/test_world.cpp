// The world model (ARCH §8.7): the engagement machine's start conditions, raises, ends and fled; squads; the combat
// state; stance; trackers dropped after 2 s absent and the cap.
#include <doctest.h>

#include "helpers.hpp"

namespace en::augur {
namespace {

using test::chr;
using test::kPlayer;
using test::Rig;
using test::snap;

constexpr std::uint64_t kA = 0x200, kB = 0x201, kC = 0x202;

proto::WorldSnapshot at(Mono t, std::vector<proto::Chr> chars) {
  proto::WorldSnapshot s = snap(t);
  for (const auto& c : chars) (void)s.chars.try_push_back(c);
  return s;
}

TEST_CASE("each start condition, strongest first: Hit, LockClosing, Near, LockOnly") {
  SUBCASE("Hit, 0.9") {
    Rig r;
    r.step(at(0, {chr(kA, {15, 0, 0})}));
    proto::WorldSnapshot s = at(250, {chr(kA, {15, 0, 0}, 6, 900)});
    s.chars[0].last_hit_by = proto::ChrHandle{kPlayer};
    r.step(s);
    REQUIRE(r.of<Engaged>().size() == 1);
    CHECK(std::get<Engaged>(r.of<Engaged>()[0].kind).via == EngageVia::Hit);
    CHECK(r.of<Engaged>()[0].confidence == doctest::Approx(0.9f));
  }
  SUBCASE("LockOnly, 0.3, then Near raises it across 0.6: re-emitted") {
    Rig r;
    Mono t = 0;
    for (float x = 18; x >= 7; x -= 0.5f, t += 250) {
      proto::WorldSnapshot s = at(t, {chr(kA, {x, 0, 0})});
      if (t >= 250) s.player->locked_on = proto::ChrHandle{kA};
      r.step(s);
    }
    const auto e = r.of<Engaged>();
    REQUIRE(e.size() == 2);
    CHECK(std::get<Engaged>(e[0].kind).via == EngageVia::LockOnly);
    CHECK(e[0].confidence == doctest::Approx(0.3f));
    CHECK(e[0].interrupts == InterruptSet::None);  // below 0.6: no interrupt
    CHECK(std::get<Engaged>(e[1].kind).via == EngageVia::LockClosing);  // 3 m closer within 2 s, within 20 m
    CHECK(e[1].confidence == doctest::Approx(0.6f));
  }
  SUBCASE("a hostile within 8 m, 0.6; a non-hostile never") {
    Rig r;
    r.step(at(0, {chr(kA, {7, 0, 0}), chr(kB, {3, 0, 0}, 2)}));
    REQUIRE(r.of<Engaged>().size() == 1);
    CHECK(r.of<Engaged>()[0].subject->handle.raw == kA);
    CHECK(std::get<Engaged>(r.of<Engaged>()[0].kind).via == EngageVia::Near);
  }
}

TEST_CASE("ends: beyond 30 m (fled), after 12 s with no exchange and no lock-on (re-armed only once apart), on death") {
  SUBCASE("distance, fled") {
    Rig r;
    Mono t = 0;
    r.step(at(t, {chr(kA, {5, 0, 0})}));
    for (float x = 5; x <= 33; x += 4, t += 250) r.step(at(t, {chr(kA, {x, 0, 0})}));
    const auto d = r.of<Disengaged>();
    REQUIRE(d.size() == 1);
    CHECK(std::get<Disengaged>(d[0].kind).why == DisengageWhy::Distance);
    CHECK(std::get<Disengaged>(d[0].kind).fled);
    CHECK(*d[0].key == "eng:#200:player");
  }
  SUBCASE("timeout, then no Near again until it has been apart") {
    Rig r;
    Mono t = 0;
    for (; t <= 13000; t += 250) r.step(at(t, {chr(kA, {6, 0, 0})}));
    REQUIRE(r.of<Disengaged>().size() == 1);
    CHECK(std::get<Disengaged>(r.of<Disengaged>()[0].kind).why == DisengageWhy::Timeout);
    CHECK_FALSE(std::get<Disengaged>(r.of<Disengaged>()[0].kind).fled);
    for (; t <= 16000; t += 250) r.step(at(t, {chr(kA, {6, 0, 0})}));
    CHECK(r.of<Engaged>().size() == 1);
    r.step(at(t += 250, {chr(kA, {12, 0, 0})}));
    r.step(at(t += 250, {chr(kA, {6, 0, 0})}));
    CHECK(r.of<Engaged>().size() == 2);
  }
  SUBCASE("death: no Disengaged") {
    Rig r;
    r.step(at(0, {chr(kA, {5, 0, 0})}));
    proto::Chr dead = chr(kA, {5, 0, 0}, 6, 0);
    dead.dead = true;
    r.step(at(250, {dead}));
    CHECK(r.of<Engaged>().size() == 1);
    CHECK(r.of<Disengaged>().empty());
    CHECK(r.augur.world().squads().empty());
  }
}

TEST_CASE("squads: the same side within 20 m of a member engaged within 10 s joins; another side starts its own") {
  Rig r;
  r.step(at(0, {chr(kA, {6, 0, 0}), chr(kB, {30, 0, 0}), chr(kC, {7, 2, 0}, 9)}));
  r.step(at(250, {chr(kA, {6, 0, 0}), chr(kB, {7, 1, 0}), chr(kC, {7, 2, 0}, 9)}));
  const auto e = r.of<Engaged>();
  REQUIRE(e.size() == 3);
  std::map<std::uint64_t, SquadId> squad;
  for (const Event& ev : e) squad[ev.subject->handle.raw] = std::get<Engaged>(ev.kind).squad;
  CHECK(squad[kA] == squad[kB]);
  CHECK(squad[kC] != squad[kA]);
  CHECK(r.augur.world().squads().size() == 2);
}

TEST_CASE("the combat state: damage, an engagement at 0.6, or a boss; 8 s after the last signal") {
  Rig r;
  r.step(at(0, {chr(kA, {50, 0, 0})}));
  CHECK_FALSE(r.augur.world().in_combat());
  proto::WorldSnapshot s = at(250, {chr(kA, {50, 0, 0})});
  s.player->hp.cur = 900;
  r.step(s);
  CHECK(r.augur.world().in_combat());
  Mono t = 250;
  for (; t <= 8250; t += 250) {
    s = at(t, {chr(kA, {50, 0, 0})});
    s.player->hp.cur = 900;
    r.step(s);
  }
  CHECK_FALSE(r.augur.world().in_combat());
  s.mono_ms = t;
  s.boss.active = true;
  r.step(s);
  CHECK(r.augur.world().in_combat());
}

TEST_CASE("stance towards a viewer, as v0.1 reads it") {
  Rig r;
  r.step(at(0, {chr(kA, {6, 0, 0}), chr(kB, {40, 0, 0}, 2), chr(kC, {42, 0, 0}, 2)}));
  const WorldModel& w = r.augur.world();
  CHECK(w.stance(ActorRef::player(), ActorRef::mute(proto::ChrHandle{kA})) == Stance::Enemy);
  CHECK(w.stance(ActorRef::mute(proto::ChrHandle{kA}), ActorRef::player()) == Stance::Enemy);
  CHECK(w.stance(ActorRef::player(), ActorRef::mute(proto::ChrHandle{kB})) == Stance::Bystander);
  CHECK(w.stance(ActorRef::mute(proto::ChrHandle{kB}), ActorRef::mute(proto::ChrHandle{kC})) == Stance::Ally);
  CHECK(w.stance(ActorRef::mute(proto::ChrHandle{kA}), ActorRef::mute(proto::ChrHandle{kB})) == Stance::Bystander);
  CHECK_FALSE(w.concealed_from(ActorRef::mute(proto::ChrHandle{kA})));  // never, until S7 and S8
}

TEST_CASE("trackers: dropped after 2 s absent, an engagement lost with them silently; at most 256") {
  Rig r;
  r.step(at(0, {chr(kA, {6, 0, 0})}));
  r.step(at(1000, {}));
  CHECK(r.augur.world().find(proto::ChrHandle{kA}) != nullptr);
  r.step(at(2000, {}));
  CHECK(r.augur.world().find(proto::ChrHandle{kA}) == nullptr);
  CHECK(r.of<Disengaged>().empty());
  Mono t = 3000;
  std::size_t most = 0;
  for (std::uint64_t base = 0; base < 32 * 12; base += 32, t += 200) {
    std::vector<proto::Chr> many;
    for (std::uint64_t i = 0; i < 32; ++i) many.push_back(chr(0x5000 + base + i, {60, 0, 0}, 2));
    r.step(at(t, many));
    most = std::max(most, r.augur.world().trackers().size());
    CHECK(r.augur.world().find(proto::ChrHandle{0x5000 + base}) != nullptr);  // the present ones are kept
  }
  CHECK(most == kMaxTrackers);
}

}  // namespace
}  // namespace en::augur
