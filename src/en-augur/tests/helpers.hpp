#pragma once
// en-augur's tests: a context of the tests' own, the shipped rule files, a seeded generator, and snapshots with
// invented values (no game data: the handles, teams, places and numbers are ours).
#include <algorithm>
#include <cmath>
#include <span>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "en/augur/augur.hpp"
#include "en/augur/bus.hpp"
#include "en/augur/golden.hpp"
#include "en/augur/rules.hpp"

namespace en::augur::test {

// splitmix64: the property tests' seeded generator; a failure prints its seed.
struct Rng {
  std::uint64_t s;
  explicit Rng(std::uint64_t seed) : s(seed) {}
  std::uint64_t next() {
    std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
  std::uint64_t below(std::uint64_t n) { return n == 0 ? 0 : next() % n; }
  double unit() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }
  bool chance(double p) { return unit() < p; }
};

// Predicates the tests' own: "always" → 1 (true), "never" → 2 (false); anything else refused.
class TestCompiler final : public PredicateCompiler {
 public:
  proto::Expected<PredicateId, std::string> compile(std::string_view expr) const override {
    if (expr == "always") return PredicateId{1};
    if (expr == "never") return PredicateId{2};
    return proto::Unexpected{std::string("not a test predicate")};
  }
};

class TestCtx final : public AugurCtx {
 public:
  Subject classify(const proto::Chr& c) const override {
    const auto it = classes.find(c.handle.raw);
    return it != classes.end() ? it->second : Subject{};
  }
  bool eval(PredicateId id, const EvalScope&) const override { return id != 2; }
  bool persona_may_perceive(PersonaId id) const override { return perceivers.count(id.index) != 0; }
  std::uint64_t rng(EventId ev, std::uint32_t salt) const override { return ev.step * 31 + ev.index * 7 + salt; }

  void persona(std::uint64_t handle, std::uint32_t id, bool perceives = true) {
    Subject s;
    s.kind = Subject::Kind::Persona;
    s.persona.index = id;
    classes[handle] = s;
    if (perceives) perceivers.insert(id);
  }

  std::map<std::uint64_t, Subject> classes;
  std::set<std::uint32_t> perceivers;
};

inline WorldFacts facts() {
  WorldFacts f;
  f.hostile_teams = {6, 9};
  f.victory_banners = {1, 2};
  f.crit_player_throw = 3;
  f.crit_victim_throw = 4;
  f.open_world_areas = {60};
  return f;
}

inline std::vector<RuleFile> shipped_files() {
  std::vector<RuleFile> out;
  std::vector<std::filesystem::path> paths;
  for (const auto& e : std::filesystem::directory_iterator(EN_AUGUR_CONTENT)) {
    if (e.path().extension() == ".toml") paths.push_back(e.path());
  }
  std::sort(paths.begin(), paths.end());
  for (const auto& p : paths) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    out.push_back(RuleFile{p.filename().string(), ss.str(), false});
  }
  return out;
}

// The shipped rule files, compiled with the tests' predicates (they hold no `requires` but "true").
inline std::shared_ptr<const RuleSet> shipped_rules() {
  static const TestCompiler compiler;
  const auto files = shipped_files();
  auto set = RuleSet::load(files, compiler);
  return set ? *set : nullptr;
}

inline std::shared_ptr<const RuleSet> rules_of(const std::string& toml) {
  static const TestCompiler compiler;
  const RuleFile f{"test.toml", toml, false};
  auto set = RuleSet::load(std::span<const RuleFile>(&f, 1), compiler);
  return set ? *set : nullptr;
}

inline constexpr std::uint64_t kPlayer = 0x100;

inline proto::Chr chr(std::uint64_t handle, std::array<float, 3> pos, std::uint8_t team = 6,
                      std::uint32_t hp = 1000, std::uint32_t max = 1000) {
  proto::Chr c;
  c.handle.raw = handle;
  c.team = team;
  c.hp = {hp, max};
  c.pos = pos;
  c.dist_m = std::sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
  c.block = proto::BlockId::of(60, 1, 2, 0);
  c.on_screen = true;
  return c;
}

// A snapshot at t with the Tarnished at the origin.
inline proto::WorldSnapshot snap(Mono t, std::uint32_t hp = 1000) {
  proto::WorldSnapshot s;
  s.seq = t / 250;
  s.mono_ms = t;
  s.life.main_player = true;
  s.life.screen = proto::Screen::InGame;
  proto::Player p;
  p.handle.raw = kPlayer;
  p.hp = {hp, 1000};
  p.play_time_ms = 100000 + t;
  p.block = proto::BlockId::of(60, 1, 2, 0);
  s.player = p;
  proto::Place place;
  place.place_id = 7001;
  s.place = place;
  return s;
}

// One Augur and its context, stepped through snapshots.
struct Rig {
  explicit Rig(std::shared_ptr<const RuleSet> rules = shipped_rules()) : augur(facts()) { augur.set_rules(std::move(rules)); }
  EventBatch step(const proto::WorldSnapshot& s, std::vector<LifeFact> life = {}) {
    EventBatch out;
    augur.step(AugurInput{&s, life}, s.mono_ms, ctx, out);
    all.insert(all.end(), out.begin(), out.end());
    return out;
  }
  EventBatch tick(Mono now, std::vector<LifeFact> life = {}) {
    EventBatch out;
    augur.step(AugurInput{nullptr, life}, now, ctx, out);
    all.insert(all.end(), out.begin(), out.end());
    return out;
  }
  template <class K>
  std::vector<Event> of() const {
    std::vector<Event> v;
    for (const Event& e : all) {
      if (std::holds_alternative<K>(e.kind)) v.push_back(e);
    }
    return v;
  }
  std::string golden() const {
    GoldenNames names;
    std::string out;
    for (const Event& e : all) out += golden_line(e, [&](ChrHandle h) { return names(h); }) + "\n";
    return out;
  }
  Augur augur;
  TestCtx ctx;
  EventBatch all;
};

}  // namespace en::augur::test
