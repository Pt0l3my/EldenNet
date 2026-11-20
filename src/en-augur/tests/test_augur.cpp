// Augur::step with the shipped rules (ARCH §8.4 to §8.9): each v0.1 event from the snapshots that make it, the
// modifiers, the audience, the interrupts, and the ordering and determinism properties of §8.12.
#include <doctest.h>

#include <string>

#include "helpers.hpp"

namespace en::augur {
namespace {

using test::chr;
using test::kPlayer;
using test::Rig;
using test::snap;

constexpr std::uint64_t kFoe = 0x200, kFoe2 = 0x201, kFoe3 = 0x202, kAlly = 0x300;

proto::WorldSnapshot with_foe(Mono t, float x, std::uint32_t hp = 1000) {
  proto::WorldSnapshot s = snap(t);
  (void)s.chars.try_push_back(chr(kFoe, {x, 0, 0}, 6, hp));
  return s;
}

TEST_CASE("Struck: the Tarnished's hit, merged per pair over 2.5 s, hard on 30% within 3 s, salient re-emits") {
  Rig r;
  r.step(with_foe(0, 5));
  proto::WorldSnapshot s = with_foe(250, 5, 900);
  s.chars[0].last_hit_by = proto::ChrHandle{kPlayer};
  r.step(s);
  s = with_foe(500, 5, 850);
  s.chars[0].last_hit_by = proto::ChrHandle{kPlayer};
  r.step(s);  // merged into the first: no new event
  s = with_foe(750, 5, 600);
  s.chars[0].last_hit_by = proto::ChrHandle{kPlayer};
  r.step(s);  // 40% within 3 s: hard turns true, a salient change: re-emitted, merged
  const auto struck = r.of<Struck>();
  REQUIRE(struck.size() == 2);
  const auto& a = std::get<Struck>(struck[0].kind);
  REQUIRE(a.att);
  CHECK(a.att->kind == ActorRef::Kind::Player);
  CHECK_FALSE(a.hard);
  CHECK(*struck[0].key == "hit:player:#200");
  CHECK_FALSE(struck[0].merged);
  const auto& b = std::get<Struck>(struck[1].kind);
  CHECK(b.hard);
  CHECK(b.count == 3);
  CHECK(b.frac_total == doctest::Approx(0.4));
  CHECK(struck[1].merged);
  CHECK(struck[1].count == 3);
  REQUIRE(r.augur.kstore().entries().size() >= 1);
}

TEST_CASE("Struck on the Tarnished: its attacker from its own last_hit_by; unseen beyond 20 m; unknown unread") {
  Rig r;
  proto::WorldSnapshot s = with_foe(0, 5);
  (void)s.chars.try_push_back(chr(kFoe2, {30, 0, 0}));
  r.step(s);
  s = with_foe(250, 5);
  (void)s.chars.try_push_back(chr(kFoe2, {30, 0, 0}));
  s.player->hp.cur = 900;
  s.player->last_hit_by = proto::ChrHandle{kFoe};
  r.step(s);
  s = with_foe(3250, 5);
  (void)s.chars.try_push_back(chr(kFoe2, {30, 0, 0}));
  s.player->hp.cur = 900;
  s.player->last_hit_by = proto::ChrHandle{kFoe};
  r.step(s);
  s = with_foe(3500, 5);
  (void)s.chars.try_push_back(chr(kFoe2, {30, 0, 0}));
  s.player->hp.cur = 800;
  s.player->last_hit_by = proto::ChrHandle{kFoe2};  // 30 m away: unseen
  r.step(s);
  // An NPC hitting an NPC: last_hit_by names another NPC, unread until S7: att unknown, not unseen.
  s = with_foe(3750, 5, 900);
  (void)s.chars.try_push_back(chr(kFoe2, {30, 0, 0}));
  s.player->hp.cur = 800;
  s.chars[0].last_hit_by = proto::ChrHandle{kFoe2};
  r.step(s);
  const auto struck = r.of<Struck>();
  REQUIRE(struck.size() == 3);
  const auto& first = std::get<Struck>(struck[0].kind);
  REQUIRE(first.att);
  CHECK(first.att->handle.raw == kFoe);
  CHECK(first.vic.kind == ActorRef::Kind::Player);
  const auto& second = std::get<Struck>(struck[1].kind);
  CHECK(second.unseen);
  CHECK_FALSE(second.att);
  CHECK(*struck[1].key == "hit:unseen:player");
  const auto& third = std::get<Struck>(struck[2].kind);
  CHECK_FALSE(third.unseen);
  CHECK_FALSE(third.att);
}

TEST_CASE("LowHp below 25%, re-armed above 35%") {
  Rig r;
  for (const auto& [t, hp] : std::vector<std::pair<Mono, std::uint32_t>>{
           {0, 1000}, {250, 200}, {500, 300}, {750, 200}, {1000, 400}, {1250, 100}}) {
    r.step(with_foe(t, 40, hp));
  }
  CHECK(r.of<LowHp>().size() == 2);
}

TEST_CASE("PlayerDied: the dead bit, the death count and banner 5 give one event, and in_boss") {
  Rig r;
  proto::WorldSnapshot s = with_foe(0, 5);
  r.step(s);
  s = with_foe(250, 5);
  s.player->dead = true;
  s.player->hp.cur = 0;
  s.player->deaths = 1;
  s.player->last_hit_by = proto::ChrHandle{kFoe};
  s.ui.banner = 5;
  r.step(s);
  s.mono_ms = 500;
  s.player->play_time_ms = 100500;
  r.step(s);
  const auto died = r.of<PlayerDied>();
  REQUIRE(died.size() == 1);
  const auto& d = std::get<PlayerDied>(died[0].kind);
  REQUIRE(d.killer);
  CHECK(d.killer->handle.raw == kFoe);
  CHECK_FALSE(d.in_boss);
  CHECK(any(died[0].interrupts & InterruptSet::OwnLinesExceptPlayerFell));
  CHECK(r.augur.stats().by_dedup >= 2);
}

proto::WorldSnapshot boss(Mono t, bool active, std::optional<std::uint16_t> banner = std::nullopt) {
  proto::WorldSnapshot s = snap(t);
  s.boss.active = active;
  if (active) (void)s.boss.bars.try_push_back(proto::BossBar{proto::ChrHandle{kFoe3}, 904, 0});
  s.ui.banner = banner;
  return s;
}

TEST_CASE("BossFightEnded: Victory on its banner, Death on PlayerDied, Left once 10 s pass") {
  SUBCASE("Victory") {
    Rig r;
    r.step(boss(0, false));
    r.step(boss(250, true));
    r.step(boss(5000, true, 1));
    r.step(boss(5250, false, 1));
    for (Mono t = 5500; t <= 7000; t += 250) r.step(boss(t, false));
    REQUIRE(r.of<BossFightStarted>().size() == 1);
    const auto ended = r.of<BossFightEnded>();
    REQUIRE(ended.size() == 1);
    CHECK(std::get<BossFightEnded>(ended[0].kind).outcome == BossOutcome::Victory);
    CHECK(ended[0].t_mono_ms == 6250);  // the falling edge debounced 1 s
    CHECK(*ended[0].key == "boss_end:1");
    CHECK(std::get<BossFightEnded>(ended[0].kind).bars.size() == 1);
  }
  SUBCASE("Death") {
    Rig r;
    r.step(boss(0, false));
    r.step(boss(250, true));
    proto::WorldSnapshot s = boss(4000, true);
    s.player->dead = true;
    r.step(s);
    s = boss(4250, false);
    s.player->dead = true;
    r.step(s);
    for (Mono t = 4500; t <= 6000; t += 250) {
      s = boss(t, false);
      s.player->dead = true;
      r.step(s);
    }
    REQUIRE(r.of<PlayerDied>().size() == 1);
    CHECK(std::get<PlayerDied>(r.of<PlayerDied>()[0].kind).in_boss);
    const auto ended = r.of<BossFightEnded>();
    REQUIRE(ended.size() == 1);
    CHECK(std::get<BossFightEnded>(ended[0].kind).outcome == BossOutcome::Death);
  }
  SUBCASE("Left, at the first whole second after the window, on the timer") {
    Rig r;
    r.step(boss(0, false));
    r.step(boss(250, true));
    r.step(boss(2000, false));
    for (Mono t = 2250; t <= 11500; t += 250) r.step(boss(t, false));
    CHECK(r.of<BossFightEnded>().empty());
    r.tick(12100);  // 2000 + 10 s, then the whole second 12 crossed
    const auto ended = r.of<BossFightEnded>();
    REQUIRE(ended.size() == 1);
    CHECK(std::get<BossFightEnded>(ended[0].kind).outcome == BossOutcome::Left);
    CHECK(ended[0].source == SourceId::Timer);
  }
}

TEST_CASE("PlaceEntered: stable 2 s, deduplicated per place for 10 min of play") {
  Rig r;
  const auto at = [&](Mono t, std::optional<std::uint32_t> place) {
    proto::WorldSnapshot s = snap(t);
    s.place->place_id = place;
    r.step(s);
  };
  Mono t = 0;
  at(t, 7001);
  for (int i = 0; i < 4; ++i) at(t += 250, 7002);  // 1 s only
  for (int i = 0; i < 12; ++i) at(t += 250, 7001);
  for (int i = 0; i < 12; ++i) at(t += 250, 7003);  // stable: entered
  for (int i = 0; i < 12; ++i) at(t += 250, std::nullopt);  // no place: nothing entered
  for (int i = 0; i < 12; ++i) at(t += 250, 7004);
  for (int i = 0; i < 12; ++i) at(t += 250, 7003);  // back within 10 min: deduplicated
  const auto entered = r.of<PlaceEntered>();
  REQUIRE(entered.size() == 2);
  CHECK(std::get<PlaceEntered>(entered[0].kind).place_id == 7003);
  CHECK(*std::get<PlaceEntered>(entered[0].kind).prev == 7001);
  CHECK(std::get<PlaceEntered>(entered[1].kind).place_id == 7004);
  CHECK(r.augur.stats().by_dedup == 1);
}

TEST_CASE("MountChanged, NativeTalk, JourneyChanged") {
  Rig r;
  proto::WorldSnapshot s = snap(0);
  r.step(s);
  s = snap(250);
  s.player->riding = true;
  s.ui.popup_talk_id = 4400;
  r.step(s);
  s = snap(1250);
  s.player->ng_lvl = 1;
  r.step(s);
  CHECK(r.of<MountChanged>().size() == 2);
  REQUIRE(r.of<NativeTalkStarted>().size() == 1);
  CHECK(std::get<NativeTalkStarted>(r.of<NativeTalkStarted>()[0].kind).signals.popup);
  CHECK(any(r.of<NativeTalkStarted>()[0].interrupts & InterruptSet::OwnLinesWithin20m));
  REQUIRE(r.of<NativeTalkEnded>().size() == 1);
  CHECK(std::get<NativeTalkEnded>(r.of<NativeTalkEnded>()[0].kind).held_ms == 1000);
  REQUIRE(r.of<JourneyChanged>().size() == 1);
  CHECK(*r.of<JourneyChanged>()[0].key == "journey:1");
}

TEST_CASE("CritLanded, AllyDied's witnesses on its side within 20 m, and no witness, no event") {
  Rig r;
  proto::WorldSnapshot s = with_foe(0, 3);
  (void)s.chars.try_push_back(chr(kFoe2, {8, 0, 0}));     // the same team, 5 m from the victim
  (void)s.chars.try_push_back(chr(kAlly, {6, 0, 0}, 2));  // another team
  r.step(s);
  s = with_foe(250, 3);
  (void)s.chars.try_push_back(chr(kFoe2, {8, 0, 0}));
  (void)s.chars.try_push_back(chr(kAlly, {6, 0, 0}, 2));
  s.player->throw_state = 3;
  s.chars[0].throw_state = 4;
  r.step(s);
  s.mono_ms = 500;
  s.chars[0].dead = true;
  s.chars[0].hp.cur = 0;
  s.chars[2].dead = true;  // the other team's: no witness on its side
  r.step(s);
  CHECK(r.of<CritLanded>().size() == 1);
  const auto died = r.of<AllyDied>();
  REQUIRE(died.size() == 1);
  const auto& d = std::get<AllyDied>(died[0].kind);
  CHECK(d.victim.handle.raw == kFoe);
  CHECK(d.side == SideId{SideId::Kind::Team, 6});
  REQUIRE(d.witnesses.size() == 1);
  CHECK(d.witnesses[0].raw == kFoe2);
  CHECK(r.augur.stats().refused >= 1);
}

TEST_CASE("Healed only in combat") {
  Rig r;
  r.step(with_foe(0, 50, 300));
  r.step(with_foe(250, 50, 700));  // no combat: nothing
  r.step(with_foe(500, 5, 300));   // within 8 m: engaged (Near, 0.6), the Tarnished in combat
  r.step(with_foe(750, 5, 300));
  r.step(with_foe(1000, 5, 700));
  const auto healed = r.of<Healed>();
  REQUIRE(healed.size() == 1);
  CHECK(std::get<Healed>(healed[0].kind).frac == doctest::Approx(0.4));
}

TEST_CASE("a `requires` that is false suppresses the fire and consumes its edge; cooldowns suppress") {
  const char* rule = R"(
[[rule]]
id = "talk"
emits = "NativeTalkStarted"
subjects = { kinds = ["world"] }
signal = "native_talk"
detect = { kind = "rising_edge" }
class = "T"
requires = "%s"
cooldown_s = %s
)";
  const auto make = [&](const char* req, const char* cool) {
    char buf[512];
    std::snprintf(buf, sizeof buf, rule, req, cool);
    return test::rules_of(buf);
  };
  const auto talk = [](Rig& r) {
    for (int i = 0; i < 6; ++i) {
      proto::WorldSnapshot s = snap(static_cast<Mono>(i) * 250);
      if (i % 2 == 1) s.ui.talk_cam = true;
      r.step(s);
    }
  };
  Rig never(make("never", "0"));
  talk(never);
  CHECK(never.of<NativeTalkStarted>().empty());
  CHECK(never.augur.stats().by_requires == 3);
  Rig cooled(make("always", "60"));
  talk(cooled);
  CHECK(cooled.of<NativeTalkStarted>().size() == 1);
  CHECK(cooled.augur.stats().by_cooldown == 2);
}

TEST_CASE("the audience: profiled NPCs in range and in the same area, how they perceived it, at most 16") {
  Rig r;
  r.ctx.persona(0x400, 1);              // 4 m from the foe: witnessed
  r.ctx.persona(0x401, 2);              // 30 m: beyond the witness radius
  r.ctx.persona(0x402, 3, false);       // may not perceive (dead, muted)
  r.ctx.persona(0x403, 4);              // another block, in the open world
  for (std::uint64_t i = 0; i < 20; ++i) r.ctx.persona(0x500 + i, 100 + static_cast<std::uint32_t>(i));
  const auto scene = [&](Mono t, std::uint32_t hp) {
    proto::WorldSnapshot s = with_foe(t, 5, hp);
    (void)s.chars.try_push_back(chr(0x400, {9, 0, 0}, 2));
    (void)s.chars.try_push_back(chr(0x401, {35, 0, 0}, 2));
    (void)s.chars.try_push_back(chr(0x402, {6, 0, 0}, 2));
    proto::Chr other = chr(0x403, {7, 0, 0}, 2);
    other.block = proto::BlockId::of(60, 9, 9, 0);
    (void)s.chars.try_push_back(other);
    for (std::uint64_t i = 0; i < 20; ++i) (void)s.chars.try_push_back(chr(0x500 + i, {5, 1.0f + static_cast<float>(i) * 0.5f, 0}, 2));
    if (hp < 1000) s.chars[0].last_hit_by = proto::ChrHandle{kPlayer};
    return s;
  };
  r.step(scene(0, 1000));
  r.step(scene(250, 900));
  const auto struck = r.of<Struck>();
  REQUIRE(struck.size() == 1);
  const auto& aud = struck[0].audience;
  CHECK(aud.size() == kMaxAudience);
  CHECK(visible_to(struck[0], PersonaId{1}));  // 4 m from the foe: among the 16 nearest
  CHECK(visible_to(struck[0], PersonaId{4}));  // another block, the same area byte of the open world
  // Nearest first: the row of twenty starts 1 m from the foe.
  CHECK(aud[0].persona.index == 100);
  CHECK(aud[0].how == How::Witnessed);
  CHECK_FALSE(visible_to(struck[0], PersonaId{2}));
  CHECK_FALSE(visible_to(struck[0], PersonaId{3}));
  // A persona that is the subject perceives directly.
  Rig d;
  d.ctx.persona(kFoe, 9);
  d.step(with_foe(0, 5));
  proto::WorldSnapshot s = with_foe(250, 5, 900);
  s.chars[0].last_hit_by = proto::ChrHandle{kPlayer};
  d.step(s);
  REQUIRE(d.of<Struck>().size() == 1);
  REQUIRE(d.of<Struck>()[0].audience.size() == 1);
  CHECK(d.of<Struck>()[0].audience[0].how == How::Direct);
  CHECK(d.of<Struck>()[0].subject->kind == ActorRef::Kind::Persona);
  // just_now: at most three, the persona's own (its Engaged and its Struck).
  CHECK(d.augur.kstore().just_now(PersonaId{9}).size() == 2);
  CHECK(d.augur.kstore().just_now(PersonaId{8}).empty());
}

TEST_CASE("indoors the same full block counts; in the open world the area byte") {
  Rig r;
  r.ctx.persona(0x400, 1);
  r.ctx.persona(0x401, 2);
  const auto scene = [&](Mono t, std::uint32_t hp) {
    proto::WorldSnapshot s = with_foe(t, 5, hp);
    s.chars[0].block = proto::BlockId::of(12, 1, 0, 0);
    proto::Chr a = chr(0x400, {6, 0, 0}, 2);
    a.block = proto::BlockId::of(12, 1, 0, 0);
    proto::Chr b = chr(0x401, {6, 1, 0}, 2);
    b.block = proto::BlockId::of(12, 2, 0, 0);
    (void)s.chars.try_push_back(a);
    (void)s.chars.try_push_back(b);
    if (hp < 1000) s.chars[0].last_hit_by = proto::ChrHandle{kPlayer};
    return s;
  };
  r.step(scene(0, 1000));
  r.step(scene(250, 900));
  REQUIRE(r.of<Struck>().size() == 1);
  CHECK(visible_to(r.of<Struck>()[0], PersonaId{1}));
  CHECK_FALSE(visible_to(r.of<Struck>()[0], PersonaId{2}));
}

TEST_CASE("a loading screen clears engagements, squads and the K store, silently") {
  Rig r;
  r.step(with_foe(0, 5));
  r.step(with_foe(250, 5));
  REQUIRE(r.of<Engaged>().size() == 1);
  CHECK_FALSE(r.augur.kstore().entries().empty());
  proto::WorldSnapshot load = snap(500);
  load.player.reset();
  load.life.main_player = false;
  load.life.screen = proto::Screen::Loading;
  r.step(load, {LifeFact{FactKind::LoadingStarted, 0, {}}});
  CHECK(r.augur.kstore().entries().empty());
  CHECK(r.augur.world().squads().empty());
  CHECK(r.of<Disengaged>().empty());
  REQUIRE(r.of<LoadingStarted>().size() == 1);
  CHECK(r.of<LoadingStarted>()[0].interrupts == kInterruptAll);
}

TEST_CASE("Engaged within 25 m at 0.6 or more interrupts chat input, ambient and autonomous lines") {
  Rig r;
  r.step(with_foe(0, 5));
  r.step(with_foe(250, 5));
  const auto e = r.of<Engaged>();
  REQUIRE(e.size() == 1);
  CHECK(e[0].interrupts == (InterruptSet::ChatInput | InterruptSet::Ambient | InterruptSet::Autonomous));
  CHECK(e[0].confidence == doctest::Approx(0.6f));
}

// ---- Seeded properties (§8.12) ----

// A busy random fight: n foes moving and trading hits with the Tarnished, some dying.
std::vector<proto::WorldSnapshot> fight(std::uint64_t seed, std::size_t foes, Mono length) {
  test::Rng rng(seed);
  std::vector<proto::WorldSnapshot> out;
  std::vector<std::array<float, 3>> pos(foes);
  std::vector<std::uint32_t> hp(foes, 1000);
  for (std::size_t i = 0; i < foes; ++i) pos[i] = {static_cast<float>(rng.below(40)), static_cast<float>(rng.below(20)), 0};
  std::uint32_t php = 1000;
  for (Mono t = 0; t < length; t += 250) {
    proto::WorldSnapshot s = snap(t, php);
    for (std::size_t i = 0; i < foes; ++i) {
      pos[i][0] = std::clamp(pos[i][0] + static_cast<float>(rng.unit() - 0.5) * 3, -45.0f, 45.0f);
      proto::Chr c = chr(0x1000 + i, pos[i], rng.chance(0.8) ? 6 : 2, hp[i]);
      if (hp[i] > 0 && rng.chance(0.08)) {
        hp[i] = hp[i] > 150 ? hp[i] - static_cast<std::uint32_t>(rng.below(150)) : 0;
        c.hp.cur = hp[i];
        c.last_hit_by = proto::ChrHandle{kPlayer};
      }
      c.dead = hp[i] == 0;
      (void)s.chars.try_push_back(c);
    }
    if (rng.chance(0.05) && php > 100) {
      php -= 60;
      s.player->hp.cur = php;
      s.player->last_hit_by = proto::ChrHandle{0x1000 + rng.below(foes)};
    }
    if (rng.chance(0.03)) s.player->locked_on = proto::ChrHandle{0x1000 + rng.below(foes)};
    out.push_back(s);
  }
  return out;
}

TEST_CASE("property: ordering: dispatch order is the sort by (priority, distance, handle) within each source batch") {
  for (std::uint64_t seed = 1; seed <= 20; ++seed) {
    CAPTURE(seed);
    Rig r;
    for (const auto& s : fight(seed, 12, 20000)) {
      const EventBatch batch = r.step(s);
      for (std::size_t i = 1; i < batch.size(); ++i) {
        const Event& a = batch[i - 1];
        const Event& b = batch[i];
        CHECK(a.id.index + 1 == b.id.index);
        if (a.source != b.source) {
          CHECK(static_cast<int>(a.source) <= static_cast<int>(b.source));
          continue;
        }
        if (std::holds_alternative<BossFightEnded>(b.kind)) continue;  // resolved after its batch
        const bool ordered = a.priority < b.priority ||
                             (a.priority == b.priority && (a.distance_m < b.distance_m ||
                                                           (a.distance_m == b.distance_m && a.order_handle <= b.order_handle)));
        CHECK(ordered);
      }
    }
  }
}

TEST_CASE("property: determinism: the same inputs give byte-identical events over 10 runs") {
  for (std::uint64_t seed = 1; seed <= 5; ++seed) {
    CAPTURE(seed);
    const auto snaps = fight(seed, 16, 30000);
    std::string first;
    for (int run = 0; run < 10; ++run) {
      Rig r;
      for (const auto& s : snaps) r.step(s);
      const std::string g = r.golden();
      if (run == 0) {
        first = g;
        CHECK_FALSE(first.empty());
      } else {
        CHECK(g == first);
      }
    }
  }
}

TEST_CASE("property: hysteresis through Augur: never two LowHp for a subject without a re-arm between") {
  for (std::uint64_t seed = 1; seed <= 20; ++seed) {
    CAPTURE(seed);
    Rig r;
    const auto snaps = fight(seed, 8, 30000);
    for (const auto& s : snaps) r.step(s);
    std::map<std::uint64_t, Mono> last;
    for (const Event& e : r.of<LowHp>()) {
      const std::uint64_t h = e.subject->handle.raw;
      if (last.count(h) != 0) {
        bool rearmed = false;
        for (const auto& s : snaps) {
          if (s.mono_ms <= last[h] || s.mono_ms >= e.t_mono_ms) continue;
          for (const auto& c : s.chars) {
            if (c.handle.raw == h && static_cast<double>(c.hp.cur) / c.hp.max > 0.35) rearmed = true;
          }
        }
        CHECK(rearmed);
      }
      last[h] = e.t_mono_ms;
    }
  }
}

}  // namespace
}  // namespace en::augur
