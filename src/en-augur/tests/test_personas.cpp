// v0.3's persona rules and frozen audiences (MEM §1.1 to §1.3; ARCH §8.6, §8.8): who perceived each event, and how,
// on invented characters around the Tarnished at the origin. Personas are the tests' own ids; every number is ours.
#include <doctest.h>

#include <algorithm>
#include <string>

#include "en/augur/audience.hpp"
#include "helpers.hpp"

using namespace en::augur;
using namespace en::augur::test;
namespace proto = en::proto;

namespace {

constexpr std::uint64_t kKale = 0x200;     // persona 0
constexpr std::uint64_t kKeeper = 0x201;   // persona 1
constexpr std::uint64_t kFar = 0x202;      // persona 2
constexpr std::uint64_t kSoldier = 0x300;  // a generic

std::vector<std::pair<std::uint32_t, How>> audience_of(const Event& e) {
  std::vector<std::pair<std::uint32_t, How>> v;
  for (const AudienceEntry& a : e.audience) v.emplace_back(a.persona.index, a.how);
  return v;
}

proto::WorldSnapshot with(Mono t, std::vector<proto::Chr> chars) {
  proto::WorldSnapshot s = snap(t);
  for (const proto::Chr& c : chars) (void)s.chars.try_push_back(c);
  return s;
}

}  // namespace

TEST_CASE("personas: seen within 10 m for 2 s, once, by the persona only, and again only after 30 m") {
  Rig rig;
  rig.ctx.persona(kKale, 0);
  rig.ctx.persona(kKeeper, 1);
  Mono t = 0;
  for (; t <= 4000; t += 250) rig.step(with(t, {chr(kKale, {8, 0, 0}, 1), chr(kKeeper, {4, 0, 0}, 1)}));
  auto seen = rig.of<PersonaSeen>();
  REQUIRE(seen.size() == 2);
  for (const Event& e : seen) {
    CHECK(e.event_class == Class::P);
    const auto who = std::get<PersonaSeen>(e.kind).persona.index;
    CHECK(audience_of(e) == std::vector<std::pair<std::uint32_t, How>>{{who, How::Direct}});
  }
  // Away to 20 m and back: no re-arm (30 m); then away to 35 m and back: seen again.
  for (; t <= 8000; t += 250) rig.step(with(t, {chr(kKale, {20, 0, 0}, 1)}));
  for (; t <= 12000; t += 250) rig.step(with(t, {chr(kKale, {5, 0, 0}, 1)}));
  CHECK(rig.of<PersonaSeen>().size() == 2);
  for (; t <= 16000; t += 250) rig.step(with(t, {chr(kKale, {35, 0, 0}, 1)}));
  for (; t <= 20000; t += 250) rig.step(with(t, {chr(kKale, {5, 0, 0}, 1)}));
  CHECK(rig.of<PersonaSeen>().size() == 3);
}

TEST_CASE("personas: approached within 6 m for 1 s, a T event with no audience") {
  Rig rig;
  rig.ctx.persona(kKale, 0);
  for (Mono t = 0; t <= 3000; t += 250) rig.step(with(t, {chr(kKale, {5, 0, 0}, 1)}));
  const auto near = rig.of<PersonaApproached>();
  REQUIRE(near.size() == 1);
  CHECK(near[0].event_class == Class::T);
  CHECK(near[0].audience.empty());
}

TEST_CASE("personas: an attack is direct for the victim, witnessed within 20 m, unseen beyond") {
  Rig rig;
  rig.ctx.persona(kKale, 0);
  rig.ctx.persona(kKeeper, 1);
  rig.ctx.persona(kFar, 2);
  auto kale = chr(kKale, {3, 0, 0}, 1);
  const auto keeper = chr(kKeeper, {3, 12, 0}, 1);   // 12 m from Kalé
  const auto far = chr(kFar, {3, 25, 0}, 1);         // 25 m from Kalé
  Mono t = 0;
  for (; t <= 1000; t += 250) rig.step(with(t, {kale, keeper, far}));
  kale.hp = {800, 1000};
  kale.last_hit_by = ChrHandle{test::kPlayer};
  rig.step(with(t, {kale, keeper, far}));
  const auto hits = rig.of<PersonaAttacked>();
  REQUIRE(hits.size() == 1);
  CHECK(hits[0].event_class == Class::P);
  CHECK(hits[0].confidence == doctest::Approx(0.95));
  CHECK(audience_of(hits[0]) ==
        std::vector<std::pair<std::uint32_t, How>>{{0, How::Direct}, {1, How::Witnessed}});
}

TEST_CASE("personas: a death is witnessed within 30 m, the dead persona never perceives, and the Tarnished's hand") {
  Rig rig;
  rig.ctx.persona(kKale, 0);
  rig.ctx.persona(kKeeper, 1);
  auto kale = chr(kKale, {3, 0, 0}, 1);
  const auto keeper = chr(kKeeper, {3, 25, 0}, 1);
  Mono t = 0;
  for (; t <= 1000; t += 250) rig.step(with(t, {kale, keeper}));
  kale.dead = true;
  kale.hp = {0, 1000};
  kale.last_hit_by = ChrHandle{test::kPlayer};
  rig.step(with(t, {kale, keeper}));
  const auto died = rig.of<PersonaDied>();
  REQUIRE(died.size() == 1);
  CHECK(std::get<PersonaDied>(died[0].kind).by_player);
  CHECK(audience_of(died[0]) == std::vector<std::pair<std::uint32_t, How>>{{1, How::Witnessed}});
}

TEST_CASE("personas: one that may not perceive (asleep, muted) is in no audience") {
  Rig rig;
  rig.ctx.persona(kKale, 0);
  rig.ctx.persona(kKeeper, 1, false);
  auto kale = chr(kKale, {3, 0, 0}, 1);
  const auto keeper = chr(kKeeper, {3, 5, 0}, 1);
  Mono t = 0;
  for (; t <= 1000; t += 250) rig.step(with(t, {kale, keeper}));
  kale.hp = {700, 1000};
  kale.last_hit_by = ChrHandle{test::kPlayer};
  rig.step(with(t, {kale, keeper}));
  const auto hits = rig.of<PersonaAttacked>();
  REQUIRE(hits.size() == 1);
  CHECK(audience_of(hits[0]) == std::vector<std::pair<std::uint32_t, How>>{{0, How::Direct}});
}

TEST_CASE("personas: a kill near personas is a K event for its witnesses; none when no persona saw it") {
  Rig rig;
  rig.ctx.persona(kKeeper, 1);
  auto soldier = chr(kSoldier, {6, 0, 0}, 6);
  const auto keeper = chr(kKeeper, {6, 10, 0}, 1);
  Mono t = 0;
  for (; t <= 1000; t += 250) rig.step(with(t, {soldier, keeper}));
  soldier.dead = true;
  soldier.hp = {0, 1000};
  soldier.last_hit_by = ChrHandle{test::kPlayer};
  rig.step(with(t, {soldier, keeper}));
  const auto kills = rig.of<KilledNear>();
  REQUIRE(kills.size() == 1);
  CHECK(kills[0].event_class == Class::K);
  CHECK(std::get<KilledNear>(kills[0].kind).witnesses == std::vector<PersonaId>{PersonaId{1}});
  CHECK(audience_of(kills[0]) == std::vector<std::pair<std::uint32_t, How>>{{1, How::Witnessed}});

  Rig alone;
  auto other = chr(kSoldier, {6, 0, 0}, 6);
  for (t = 0; t <= 1000; t += 250) alone.step(with(t, {other}));
  other.dead = true;
  other.last_hit_by = ChrHandle{test::kPlayer};
  alone.step(with(t, {other}));
  CHECK(alone.of<KilledNear>().empty());
}

TEST_CASE("personas: a boss victory with a persona beside the Tarnished is FoughtBeside, by the persona only") {
  Rig rig;
  rig.ctx.persona(kKale, 0);
  rig.ctx.persona(kFar, 2);
  const auto boss = chr(0x400, {15, 0, 0}, 6);
  const auto kale = chr(kKale, {10, 0, 0}, 1);
  const auto far = chr(kFar, {60, 0, 0}, 1);
  proto::BossBar bar;
  bar.handle = boss.handle;
  bar.name_id = 7777;
  Mono t = 0;
  for (; t <= 2000; t += 250) {
    auto s = with(t, {boss, kale, far});
    s.boss.active = true;
    (void)s.boss.bars.try_push_back(bar);
    rig.step(s);
  }
  auto s = with(t, {kale, far});
  s.ui.banner = 1;  // a victory banner of the tests' facts
  rig.step(s);
  for (t += 250; t <= 6000; t += 250) rig.step(with(t, {kale, far}));
  const auto fought = rig.of<FoughtBeside>();
  REQUIRE(fought.size() == 1);
  CHECK(std::get<FoughtBeside>(fought[0].kind).persona.index == 0);
  CHECK(std::get<FoughtBeside>(fought[0].kind).boss == 7777);
  CHECK(audience_of(fought[0]) == std::vector<std::pair<std::uint32_t, How>>{{0, How::Direct}});
  CHECK(rig.of<BossFightEnded>().size() == 1);
}

TEST_CASE("personas: a native talk with a persona within 3 m is P and the persona's; the world's stays T") {
  Rig rig;
  rig.ctx.persona(kKale, 0);
  rig.ctx.persona(kKeeper, 1);
  const auto kale = chr(kKale, {2, 0, 0}, 1);
  const auto keeper = chr(kKeeper, {2.5f, 0, 0}, 1);
  Mono t = 0;
  for (; t <= 1000; t += 250) rig.step(with(t, {kale, keeper}));
  for (; t <= 4000; t += 250) {
    auto s = with(t, {kale, keeper});
    s.ui.popup_talk_id = 12;
    rig.step(s);
  }
  for (; t <= 5000; t += 250) rig.step(with(t, {kale, keeper}));
  const auto started = rig.of<NativeTalkStarted>();
  const auto ended = rig.of<NativeTalkEnded>();
  REQUIRE(started.size() == 2);
  REQUIRE(ended.size() == 2);
  const auto with_persona = [](const auto& v) {
    return std::count_if(v.begin(), v.end(), [](const Event& e) {
      return e.event_class == Class::P;
    });
  };
  CHECK(with_persona(started) == 1);
  CHECK(with_persona(ended) == 1);
  for (const Event& e : started) {
    if (e.event_class != Class::P) {
      CHECK_FALSE(std::get<NativeTalkStarted>(e.kind).persona.has_value());
      continue;
    }
    CHECK(std::get<NativeTalkStarted>(e.kind).persona == PersonaId{0});  // the nearer of the two
    CHECK(audience_of(e) == std::vector<std::pair<std::uint32_t, How>>{{0, How::Direct}});
  }
}

TEST_CASE("visible_to: the journey in view, the persona's own row, and nothing for the reserved kinds") {
  proto::StoredEvent ev;
  ev.id = (std::int64_t{1} << 33) + 5;
  ev.journey = 2;
  ev.kind = "PlayerSaid";
  ev.audience = {{"kale", How::Direct}, {"lantern_keeper", How::Overheard}};
  const auto kale = visible_to(ev, "kale", VisCtx{2});
  REQUIRE(kale);
  CHECK(kale->how == How::Direct);
  CHECK(kale->ev().id == ev.id);
  CHECK(visible_to(ev, "lantern_keeper", VisCtx{2})->how == How::Overheard);
  CHECK_FALSE(visible_to(ev, "boc", VisCtx{2}));
  CHECK_FALSE(visible_to(ev, "kale", VisCtx{1}));  // an earlier journey is never visible, nor a later one
  CHECK_FALSE(visible_to(ev, "kale", VisCtx{3}));
  ev.kind = "DecisionMade";
  CHECK_FALSE(visible_to(ev, "kale", VisCtx{2}));

  Event ram;
  ram.journey = 1;  // ng_lvl 1: journey 2
  ram.audience = {AudienceEntry{PersonaId{4}, How::Witnessed}};
  CHECK(visible_to(ram, PersonaId{4}, VisCtx{2})->how == How::Witnessed);
  CHECK_FALSE(visible_to(ram, PersonaId{4}, VisCtx{1}));
  CHECK_FALSE(visible_to(ram, PersonaId{5}, VisCtx{2}));
  ram.kind = NpcThought{};
  CHECK_FALSE(visible_to(ram, PersonaId{4}, VisCtx{2}));
}

TEST_CASE("stored_audience: each row by its profile id, a row with no name left out, nearest first kept") {
  Event ev;
  ev.audience = {AudienceEntry{PersonaId{1}, How::Direct}, AudienceEntry{PersonaId{9}, How::Overheard},
                 AudienceEntry{PersonaId{0}, How::Witnessed}};
  const auto rows = stored_audience(ev, [](PersonaId p) -> std::optional<std::string> {
    if (p.index == 0) return std::string("kale");
    if (p.index == 1) return std::string("lantern_keeper");
    return std::nullopt;
  });
  CHECK(rows == std::vector<proto::AudienceRow>{{"lantern_keeper", How::Direct}, {"kale", How::Witnessed}});
  CHECK(audience_size(ev) == 3);
}
