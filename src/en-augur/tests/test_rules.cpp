// The rule files' schema and load-time validation (ARCH §8.5): the shipped set loads; each refusal names its file and
// line; unknown keys are refused; any error refuses the whole set.
#include <doctest.h>

#include <string>

#include "helpers.hpp"

namespace en::augur {
namespace {

const test::TestCompiler kCompiler;

std::vector<RuleError> errors_of(const std::string& toml, RuleLimits limits = {}, bool user = false) {
  const RuleFile f{"t.toml", toml, user};
  auto r = RuleSet::load(std::span<const RuleFile>(&f, 1), kCompiler, limits);
  if (r) return {};
  return r.error();
}

bool refused_with(const std::string& toml, const std::string& words) {
  const auto e = errors_of(toml);
  for (const RuleError& x : e) {
    if (describe(x).find(words) != std::string::npos) return true;
  }
  if (e.empty()) MESSAGE("accepted: " << toml);
  for (const RuleError& x : e) MESSAGE(describe(x));
  return false;
}

const char* kGood = R"(
[[rule]]
id = "low"
emits = "LowHp"
subjects = { kinds = ["generic"], alive = true }
signal = "hp_frac"
detect = { kind = "crosses_below", value = 0.25, rearm_above = 0.35 }
class = "K"
key = "low_hp:{subject}"
ttl_s = 20
confidence = 0.95
audience = { radius = "witness" }
interrupts = ["ambient"]
requires = "always"
priority = 30
)";

std::string with(const std::string& from, const std::string& to) {
  std::string s = kGood;
  const auto at = s.find(from);
  REQUIRE(at != std::string::npos);
  s.replace(at, from.size(), to);
  return s;
}

TEST_CASE("the shipped rule files load, and hold v0.1's catalogue and v0.3's persona rules") {
  const auto files = test::shipped_files();
  REQUIRE(files.size() == 4);
  auto set = RuleSet::load(files, kCompiler);
  for (const RuleError& e : set ? std::vector<RuleError>{} : set.error()) MESSAGE(describe(e));
  REQUIRE(set);
  std::set<std::string> kinds;
  for (const Rule& r : (*set)->rules()) kinds.insert(std::string(kind_name(r.kind)));
  for (const char* k : {"PlaceEntered", "BossFightStarted", "BossFightEnded", "NativeTalkStarted", "NativeTalkEnded",
                        "PlayerDied", "MountChanged", "Struck", "LowHp", "Healed", "Engaged", "Disengaged",
                        "AllyDied", "CritLanded", "JourneyChanged", "DataReady", "PlaythroughReady", "LoadingStarted",
                        "LoadingEnded", "TitleReached", "GateChanged", "GroupTripped", "LadderChanged",
                        "PersonaSeen", "PersonaApproached", "PersonaAttacked", "PersonaDied", "FoughtBeside",
                        "KilledNear"}) {
    CAPTURE(k);
    CHECK(kinds.count(k) == 1);
  }
  CHECK(kinds.size() == 29);  // NativeTalkStarted and Ended are the world's and a persona's: one kind each
  // O147: the pacing core is v0.4's, so the rules that only arm own-initiative lines wait for it.
  CHECK(kinds.count("CameNear") == 0);
  CHECK(kinds.count("AutonomyTick") == 0);
  CHECK((*set)->hash() != 0);
  CHECK(hash_text((*set)->hash()).rfind("fnv1a64:", 0) == 0);
}

TEST_CASE("a good rule loads with every field read") {
  const auto e = errors_of(kGood);
  for (const RuleError& x : e) MESSAGE(describe(x));
  REQUIRE(e.empty());
  const RuleFile f{"t.toml", kGood, false};
  auto set = RuleSet::load(std::span<const RuleFile>(&f, 1), kCompiler);
  REQUIRE(set);
  const Rule& r = (*set)->rules()[0];
  CHECK(*r.id == "low");
  CHECK(r.cls == Class::K);
  CHECK(r.ttl_s == 20);
  CHECK(r.alive);
  CHECK(r.radius == Radius::Witness);
  CHECK(r.interrupts == InterruptSet::Ambient);
  CHECK(r.requires_id == 1);
  CHECK(r.priority == 30);
  CHECK(r.line == 2);
}

TEST_CASE("each refusal, with its file and line") {
  CHECK(refused_with(with("priority = 30", "priority = 30\ncolour = 1"), "t.toml:16: rule[0]: has an unknown key \"colour\""));
  CHECK(refused_with(with("alive = true", "alive = true, colour = 1"), "unknown key \"colour\""));
  CHECK(refused_with(with("rearm_above = 0.35", "rearm_above = 0.35, step = 2"), "unknown key \"step\""));
  CHECK(refused_with("[x]\na = 1\n", "has an unknown key \"x\""));
  CHECK(refused_with(with("\"LowHp\"", "\"Frobbed\""), "t.toml:4: rule[0].emits: is not in the event catalogue"));
  CHECK(refused_with(with("\"hp_frac\"", "\"mood\""), "is not in the signal registry"));
  CHECK(refused_with(with("\"hp_frac\"", "\"flag:rest\""), "flag signals wait for the flag catalogue"));
  CHECK(refused_with(with("\"hp_frac\"", "\"dead\""), "does not take the signal \"dead\""));
  CHECK(refused_with(with("\"crosses_below\"", "\"wobble\""), "is not a detector"));
  CHECK(refused_with(with("rearm_above = 0.35", "rearm_above = 0.2"), "must be at least value (hysteresis)"));
  CHECK(refused_with(with("[\"generic\"]", "[\"world\"]"), "is not sampled for"));
  CHECK(refused_with(with("[\"generic\"]", "[\"crowd\"]"), "holds a kind that is not"));
  CHECK(refused_with(with("ttl_s = 20", "ttl_s = 0"), "must be at least 1 for a K rule"));
  CHECK(refused_with(with("key = \"low_hp:{subject}\"\n", ""), "a K rule needs its key"));
  CHECK(refused_with(with("ttl_s = 20", "ttl_s = 20\ndedup_s = 5"), "is taken by P rules only"));
  CHECK(refused_with(with("class = \"K\"", "class = \"T\""), "a T rule takes no ttl_s, merge_ms or dedup_s"));
  CHECK(refused_with(with("class = \"K\"", "class = \"Q\""), "must be P, K or T"));
  CHECK(refused_with(with("{subject}", "{att}"), "names a placeholder its payload does not offer"));
  CHECK(refused_with(with("{subject}", "{subject"), "has a '{' without its '}'"));
  CHECK(refused_with(with("\"witness\"", "\"far\""), "must be speech, witness, death, combat, none or subject"));
  CHECK(refused_with(with("[\"ambient\"]", "[\"thunder\"]"), "holds a name that is not an interrupt"));
  CHECK(refused_with(with("\"always\"", "\"world(x)\""), "does not compile: not a test predicate"));
  CHECK(refused_with(with("priority = 30", "priority = 101"), "must be a whole number from 0 to 100"));
  CHECK(refused_with(with("confidence = 0.95", "confidence = 0"), "must be above 0 and at most 1"));
  CHECK(refused_with(with("ttl_s = 20", "ttl_s = 601"), "from 0 to 600"));
  CHECK(refused_with(with("ttl_s = 20", "ttl_s = 20\ncooldown_s = 3601"), "from 0 to 3600"));
  CHECK(refused_with(with("ttl_s = 20", "ttl_s = 20\ndebounce_ms = 60001"), "from 0 to 60000"));
  CHECK(refused_with(with("ttl_s = 20", "ttl_s = 20\nmerge_ms = 30001"), "from 0 to 30000"));
  CHECK(refused_with(with("id = \"low\"", "id = \"Low Hp\""), "must be a lower-case word"));
  CHECK(refused_with(with("emits = \"LowHp\"", "emits = \"CameNear\""), "it waits for speakers to greet"));
  CHECK(refused_with(with("emits = \"LowHp\"", "emits = \"StageChanged\""), "it waits for the knowledge subscriber"));
  CHECK(refused_with(with("emits = \"LowHp\"", "emits = \"FlagChanged\""), "it waits for flag edges"));
  CHECK(refused_with(with("emits = \"LowHp\"", "emits = \"LowHp\"\npayload = \"struck\""), "builds Struck, not LowHp"));
  CHECK(refused_with("[[rule]]\nid = \"e\"\nemits = \"Engaged\"\nsubjects = { kinds = [\"generic\"] }\nsignal = \"engaged\"\n"
                     "detect = { kind = \"engagement\", on = \"engaged\" }\ndebounce_ms = 100\nclass = \"T\"\n",
                     "is not taken by the engagement machine or a fact"));
  CHECK(refused_with("[[rule]]\nid = \"d\"\nemits = \"DataReady\"\nsubjects = { kinds = [\"world\"] }\nsignal = \"lifecycle\"\n"
                     "detect = { kind = \"fact\", value = \"sunrise\" }\nclass = \"T\"\n",
                     "is not a lifecycle fact"));
  CHECK(refused_with(std::string(kGood) + kGood, "rule id \"low\" is not unique"));
  CHECK(refused_with("rule = 3\n", "rule must be an array of tables"));
  CHECK(refused_with("[[rule]]\nid = \"x\"\n", "is missing"));
  CHECK(refused_with("[!]\n", "is refused before it is parsed"));
  CHECK(refused_with("a = 1\na = 2\n", "is not valid TOML"));
}

TEST_CASE("bounds on files and sets, and user files only when allowed") {
  RuleLimits small;
  small.max_file_bytes = 64;
  CHECK_FALSE(errors_of(kGood, small).empty());
  RuleLimits few;
  few.max_rules = 0;
  CHECK_FALSE(errors_of(kGood, few).empty());
  CHECK_FALSE(errors_of(kGood, {}, true).empty());  // augur.allow_custom_rules is off
  RuleLimits custom;
  custom.allow_custom = true;
  CHECK(errors_of(kGood, custom, true).empty());
  // Many errors are capped, with a line that says how many more.
  std::string many;
  for (int i = 0; i < 100; ++i) many += "[[rule]]\nid = \"r" + std::to_string(i) + "\"\n";
  const auto e = errors_of(many);
  CHECK(e.size() == 65);
  CHECK(e.back().problem.find("more problems") != std::string::npos);
}

TEST_CASE("an error anywhere refuses the whole set") {
  const RuleFile files[] = {{"a.toml", kGood, false}, {"b.toml", "[[rule]]\nid = \"bad\"\n", false}};
  CHECK_FALSE(RuleSet::load(files, kCompiler));
}

}  // namespace
}  // namespace en::augur
