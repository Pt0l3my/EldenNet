// Augur in en-core's step (core.hpp; ARCH §8 opening, part 11): the built-in rule set loads, Augur sees the slice's
// lifecycle changes on the same step, every character is Mute until personas exist, and v0.1's predicates.
#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "en/augur/golden.hpp"
#include "en/core/core.hpp"
#include "snapshots.hpp"

namespace en::core {
namespace {

using test::in;

std::vector<std::string> kinds(const Outputs& out) {
  std::vector<std::string> k;
  for (const augur::Event& e : out.events) k.emplace_back(augur::kind_name(e.kind));
  return k;
}

bool has(const std::vector<std::string>& v, const char* k) { return std::find(v.begin(), v.end(), k) != v.end(); }

proto::Chr soldier(std::uint64_t h, float dist, std::uint32_t hp = 400) {
  proto::Chr c;
  c.handle.raw = h;
  c.team = 6;  // in the hostile set
  c.hp = {hp, 400};
  c.dist_m = dist;
  c.pos = {dist, 0, 0};
  c.block = proto::BlockId::of(60, 40, 36, 0);
  return c;
}

TEST_CASE("the built-in rule set loads, and v0.1's rule files are all of it") {
  Core core;
  CHECK(core.rule_errors().empty());
  REQUIRE(core.augur().rules());
  CHECK(core.augur().rules()->rules().size() >= 20);
  CHECK(builtin_rule_files().size() == 3);
}

TEST_CASE("Augur reads the slice's lifecycle changes of the same step") {
  Core core;
  Outputs a = core.step(in(test::data_ready()), 0);
  CHECK(has(kinds(a), "DataReady"));
  CHECK(has(kinds(a), "LadderChanged"));
  (void)core.step(in(test::playthrough_ready()), 10);
  (void)core.step(in(test::world(250)), 250);
  Outputs b = core.step(in(test::world(500)), 500);
  CHECK(has(kinds(b), "PlaythroughReady"));
  // The gate opens after 5 s clean: GateChanged{active} on that very step.
  bool opened = false;
  for (Mono t = 750; t <= 6000 && !opened; t += 250) {
    const Outputs o = core.step(in(test::world(t)), t);
    for (const augur::Event& e : o.events) {
      if (const auto* g = std::get_if<augur::GateChanged>(&e.kind)) opened = g->active;
    }
  }
  CHECK(opened);
  Outputs l = core.step(in(test::loading(6250)), 6250);
  CHECK(has(kinds(l), "LoadingStarted"));
  CHECK(has(kinds(l), "GateChanged"));
}

TEST_CASE("every character is Mute and the Tarnished is Player until personas exist") {
  Core core;
  (void)core.step(in(test::data_ready()), 0);
  (void)core.step(in(test::playthrough_ready()), 0);
  std::vector<augur::Event> all;
  for (Mono t = 250; t <= 3000; t += 250) {
    proto::WorldSnapshot s = test::world(t);
    s.player->handle.raw = 0x10;
    s.player->hp = {500, 500};
    proto::Chr c = soldier(0x20, 5.0f, t >= 1500 ? 300 : 400);
    if (t >= 1500) c.last_hit_by = proto::ChrHandle{0x10};
    (void)s.chars.try_push_back(c);
    Outputs o = core.step(in(s), t);
    all.insert(all.end(), o.events.begin(), o.events.end());
  }
  bool engaged = false, struck = false;
  for (const augur::Event& e : all) {
    if (e.subject && e.subject->kind != augur::ActorRef::Kind::Player) {
      CHECK(e.subject->kind == augur::ActorRef::Kind::Mute);
    }
    CHECK(e.audience.empty());  // no persona perceives yet
    if (const auto* g = std::get_if<augur::Engaged>(&e.kind)) {
      engaged = true;
      CHECK(g->via == augur::EngageVia::Near);  // within 8 m before the hit
    }
    if (const auto* s = std::get_if<augur::Struck>(&e.kind)) {
      struck = true;
      REQUIRE(s->att);
      CHECK(s->att->kind == augur::ActorRef::Kind::Player);
    }
  }
  CHECK(engaged);
  CHECK(struck);
}

TEST_CASE("v0.1's predicates: runes and journey, and every call refused") {
  Predicates p;
  const auto a = p.compile("runes >= 1000 && !(journey > 0)");
  REQUIRE(a);
  CHECK(p.eval(*a, PredicateFacts{1000, 0}));
  CHECK_FALSE(p.eval(*a, PredicateFacts{999, 0}));
  CHECK_FALSE(p.eval(*a, PredicateFacts{5000, 1}));
  const auto b = p.compile("false || (true && journey == 2)");
  REQUIRE(b);
  CHECK(p.eval(*b, PredicateFacts{0, 2}));
  CHECK_FALSE(p.eval(*b, PredicateFacts{0, 3}));
  CHECK(p.eval(augur::kAlwaysTrue, PredicateFacts{}));
  for (const char* bad : {"world(\"ash\")", "knows(\"x\")", "flag(\"rest\")", "boss(\"x\")", "stage(\"k\", 2)",
                          "region(\"limgrave\")", "runes >= ", "runes", "(true", "true &&", "frobnicate",
                          "runes >= 1234567890123"}) {
    CAPTURE(bad);
    CHECK_FALSE(p.compile(bad));
  }
  std::string deep;
  for (int i = 0; i < 40; ++i) deep += "(";
  deep += "true";
  for (int i = 0; i < 40; ++i) deep += ")";
  CHECK_FALSE(p.compile(deep));
  CHECK_FALSE(p.compile(std::string(1100, ' ') + "true"));
  const auto err = p.compile("world(\"secret_alias\")");
  REQUIRE_FALSE(err);
  CHECK(err.error().find("secret_alias") == std::string::npos);  // never a quote of the expression
}

TEST_CASE("GroupTripped: from C2's message or a snapshot's health, once per group for the session") {
  Core core;
  (void)core.step(in(test::data_ready()), 0);
  const auto count = [](const Outputs& o, proto::Group g) {
    int n = 0;
    for (const augur::Event& e : o.events) {
      if (const auto* t = std::get_if<augur::GroupTripped>(&e.kind)) n += t->group == g ? 1 : 0;
    }
    return n;
  };
  CHECK(count(core.step(in(test::tripped(proto::Group::Text)), 10), proto::Group::Text) == 1);
  proto::WorldSnapshot s = test::world(250);
  s.health.of(proto::Group::Text).state = proto::GroupState::Tripped;
  s.health.of(proto::Group::Camera).state = proto::GroupState::Tripped;
  const Outputs o = core.step(in(s), 250);
  CHECK(count(o, proto::Group::Text) == 0);  // already had
  CHECK(count(o, proto::Group::Camera) == 1);
  s.mono_ms = 500;
  CHECK(count(core.step(in(s), 500), proto::Group::Camera) == 0);
}

TEST_CASE("the golden form: handles renamed in order of first appearance, keys included") {
  augur::GoldenNames names;
  const augur::HandleNamer n = [&](augur::ChrHandle h) { return names(h); };
  CHECK(augur::key_names("hit:#1a2b:#ff", n) == "hit:h1:h2");
  CHECK(augur::key_names("hit:#ff:player", n) == "hit:h2:player");
  CHECK(augur::key_names("place:#", n) == "place:#");
  CHECK(augur::actor_text(augur::ActorRef::mute(augur::ChrHandle{0x1a2b}), n) == "mute#h1");
}

TEST_CASE("a rule set that fails to load keeps the old one") {
  Core core;
  const std::size_t before = core.augur().rules()->rules().size();
  const augur::RuleFile bad{"bad.toml", "[[rule]]\nid = \"x\"\nemits = \"NoSuchKind\"\n", false};
  const auto errors = core.load_rules(std::span<const augur::RuleFile>(&bad, 1));
  CHECK_FALSE(errors.empty());
  CHECK(core.augur().rules()->rules().size() == before);
}

}  // namespace
}  // namespace en::core
