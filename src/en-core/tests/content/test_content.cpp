// The content pack (content.hpp; NPC §2.1, §3.1, §3.2; DESIGN §11.4): the shipped test persona, the overrides, the
// resolved record, and the errors, which name the file, the line and the key and never quote a value. Every persona
// written here is invented.
#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "en/core/content.hpp"
#include "en/core/prompt.hpp"  // kRecordFields
#include "../test_files.hpp"

using namespace en::core;
using namespace en::core::test;

namespace {

constexpr const char* kIndex = R"toml(schema = 1
[[persona]]
id = "wren"
file = "personas/wren.toml"
npc_params = [ { param = 4100, form = "default" } ]
)toml";

// An invented persona with two stages: the base, and one behind a flag v0.1's predicates cannot read yet.
constexpr const char* kWren = R"toml(id = "wren"
schema = 1
drafted = "hand"
review = "ci"
display_name_fallback = "Wren"
register = "colloquial"
pronouns = "she"
deflections = ["Not today.", "Ask the wind.", "That is mine to keep."]
[voice]
openers = ["So,", "Well,"]
tics = [ { text = "Mark my words.", max_per_conversation = 2 } ]
[[stages]]
id = "base"
when = "true"
road = "You mend nets by the ford."
at = "limgrave"
state = "awake"
where = "at the ford"
forbid = ["where the boats went"]
[[stages]]
id = "later"
when = "runes >= 2"
gist = "You have met before."
state = "asleep"
)toml";

std::vector<ContentFile> pack_of(const char* index, const char* persona) {
  return {ContentFile{"identity/personas.toml", index, ContentOrigin::Shipped},
          ContentFile{"personas/wren.toml", persona, ContentOrigin::Shipped}};
}

std::vector<ContentError> errors_of(const char* index, const char* persona) {
  const auto files = pack_of(index, persona);
  auto pack = load_content(files);
  REQUIRE_FALSE(pack.has_value());
  return pack.error();
}

bool has(const std::vector<ContentError>& errors, const std::string& fragment) {
  return std::any_of(errors.begin(), errors.end(),
                     [&](const ContentError& e) { return describe(e).find(fragment) != std::string::npos; });
}

std::string with(std::string text, const std::string& from, const std::string& to) {
  const auto at = text.find(from);
  REQUIRE(at != std::string::npos);
  return text.replace(at, from.size(), to);
}

}  // namespace

TEST_CASE("the shipped pack loads: the pilot with its lead row, and the test persona, test_only, with its typed fields") {
  const auto files = builtin_content();
  auto pack = load_content(files);
  REQUIRE(pack.has_value());
  REQUIRE(pack->rows.size() == 2);
  const PersonaRow* keeper = pack->row("lantern_keeper");
  REQUIRE(keeper != nullptr);
  CHECK(keeper->test_only);
  CHECK(keeper->npc_params.empty());
  // The pilot (NPC §2.1, §3.4): one NpcParam row and its flag block, both leads until S1 and S6.
  const PersonaRow* kale = pack->row("kale");
  REQUIRE(kale != nullptr);
  CHECK_FALSE(kale->test_only);
  REQUIRE(kale->npc_params.size() == 1);
  CHECK_FALSE(kale->npc_params[0].verified);
  CHECK(kale->entity_ids.empty());
  REQUIRE(kale->flag_block.has_value());
  CHECK(kale->flag_block->alias == "kale");
  CHECK_FALSE(kale->flag_block->verified);
  const Persona* p = pack->persona("lantern_keeper");
  REQUIRE(p != nullptr);
  CHECK(p->display_name_fallback == "Lantern-Keeper");
  CHECK(p->reg == Register::Formal);
  CHECK(p->reply_words_max == 30);
  CHECK(p->openers == std::vector<std::string>{"Hm."});
  CHECK(p->deflections.size() == 3);
  REQUIRE(p->tics.size() == 1);
  CHECK(p->tics[0].max_per_conversation == 1);
  REQUIRE(p->stages.size() == 1);
  CHECK(p->stages[0].when == "true");
  CHECK(p->state_at(0) == Conscious::Awake);
  CHECK(p->approach_at(0) == Approach::None);
  CHECK(p->chat_at(0) == ChatMode::Allowed);
  CHECK(p->forbid_at(0) == std::vector<std::string>{"what lies at the end of the road"});
  CHECK(pack->hash != 0);
}

TEST_CASE("the resolved record holds what may be rendered, and none of what never is") {
  const auto files = builtin_content();
  auto pack = load_content(files);
  REQUIRE(pack.has_value());
  const nlohmann::json r = pack->persona("lantern_keeper")->resolved(0);
  CHECK(r["public"]["role"] == "keeper of the crossroads lamp");
  CHECK(r["voice"]["openers"][0] == "Hm.");
  for (const char* never : {"id", "stages", "seed", "deflections", "knowledge", "subjects", "stirs"}) {
    CAPTURE(never);
    CHECK_FALSE(r.contains(never));  // the seed and deflections are shown directly, never prompted
  }
  REQUIRE(r.contains("stage"));
  const nlohmann::json& s = r["stage"];
  CHECK(s["where"] == "beside the lamp at the crossroads");
  CHECK(s["road"].get<std::string>().rfind("You tend one lamp", 0) == 0);
  for (const char* never : {"id", "at", "forbid", "when", "beckon", "beckon_src", "approach", "state", "chat"}) {
    CAPTURE(never);
    CHECK_FALSE(s.contains(never));
  }
  CHECK(r.dump().find("end of the road") == std::string::npos);  // the forbid topic is nowhere
}

namespace {

// Every leaf's dotted path (a list is a leaf), in the record's order.
void leaves(const nlohmann::json& j, const std::string& at, std::vector<std::string>& out) {
  if (!j.is_object()) {
    out.push_back(at);
    return;
  }
  for (const auto& [k, v] : j.items()) leaves(v, at.empty() ? k : at + "." + k, out);
}

}  // namespace

TEST_CASE("the resolved record is exactly what the renderer reads: kRecordFields, the file's own values") {
  // An invented record with every field the dialogue prompt reads, and fields it does not.
  constexpr const char* kFull = R"toml(id = "wren"
schema = 1
drafted = "hand"
review = "ci"
display_name_fallback = "Wren"
register = "colloquial"
reply_words_max = 25
address_forms = ["friend"]
pronouns = "she"
instructions = "Speak of the ford and the nets, and of little else, whoever asks, and keep it short."
deflections = ["Not today.", "Ask the wind.", "That is mine to keep."]
subjects = ["the ford"]
[public]
summary = "A net-mender at the ford."
appearance = "A small woman with tar on her hands."
role = "net-mender"
[private]
personality = "Quick and wary."
honesty = "guarded: says little of the ferry"
claims = "Says the ford was hers first."
seeks = ["a dry spring"]
ties = ["The ferryman: an old quarrel"]
[voice]
cadence = "Short, quick sentences."
diction = "River words."
deflection = "Changes the subject to the weather."
openers = ["So,"]
tics = [ { text = "Mark my words.", max_per_conversation = 2 } ]
[knowledge]
always = ["kb.test.ford"]
[[stages]]
id = "base"
when = "true"
identity = "You are Wren, who mends nets."
road = "You mend nets by the ford."
at = "limgrave"
state = "awake"
where = "at the ford"
mood = "busy"
gist = "You have not met."
forbid = ["where the boats went"]
[[stages]]
id = "later"
when = "runes >= 2"
gist = "You have met before."
unlock = { text = "You owe the traveller a favour now, for the nets.", kb = ["kb.test.nets"] }
)toml";
  const auto files = pack_of(kIndex, kFull);
  auto pack = load_content(files);
  REQUIRE_MESSAGE(pack.has_value(), (pack.has_value() ? std::string() : describe(pack.error().front())));
  const Persona& p = pack->personas.at(0);
  std::vector<std::string> got;
  leaves(p.resolved(1), "", got);
  std::vector<std::string> want(kRecordFields.begin(), kRecordFields.end());
  want.erase(std::remove_if(want.begin(), want.end(),
                            [](const std::string& f) { return f == "stage.identity" || f == "stage.where" ||
                                                              f == "stage.mood"; }),
             want.end());  // the base stage's own, never inherited
  std::sort(got.begin(), got.end());
  std::sort(want.begin(), want.end());
  CHECK(got == want);
  CHECK(p.resolved(1)["stage"]["road"] == "You mend nets by the ford.");  // inherited
  CHECK_FALSE(p.resolved(1)["stage"]["unlock"].contains("kb"));
  got.clear();
  leaves(p.resolved(0), "", got);
  CHECK(std::find(got.begin(), got.end(), "stage.identity") != got.end());
  CHECK(std::find(got.begin(), got.end(), "stage.unlock.text") == got.end());
}

TEST_CASE("a stage inherits road, at, hears and knows; chat and the rest never") {
  const auto files = pack_of(kIndex, kWren);
  auto pack = load_content(files);
  REQUIRE(pack.has_value());
  const Persona& p = pack->personas.at(0);
  const nlohmann::json later = p.resolved(1);
  CHECK(later["stage"]["gist"] == "You have met before.");
  CHECK(later["stage"]["road"] == "You mend nets by the ford.");  // inherited
  CHECK_FALSE(later["stage"].contains("where"));  // not inherited
  CHECK(p.state_at(1) == Conscious::Asleep);
  CHECK(p.state_at(0) == Conscious::Awake);
  CHECK(p.approach_at(1) == Approach::Unknown);  // unset reads unknown
  CHECK(p.stages[1].shows);                      // a gist of its own
  CHECK(p.resolved(9)["stage"]["gist"] == "You have met before.");  // past the last: the last
  CHECK(p.forbid_at(1).empty());                   // forbid never inherits
  CHECK(p.origin == ContentOrigin::Shipped);
}

TEST_CASE("the overrides replace a shipped file whole and add their own") {
  auto files = content_with_overrides();
  auto pack = load_content(files);
  REQUIRE(pack.has_value());
  REQUIRE(pack->rows.size() == 1);
  REQUIRE(pack->rows[0].npc_params.size() == 1);
  CHECK(pack->rows[0].npc_params[0].param == 81100);
  CHECK(pack->rows[0].test_only);  // the binding is the tests', and the persona stays a test persona
  // A file of the overrides' own, beside the shipped ones.
  files.push_back(ContentFile{"prompts/dialogue/00_frame.md", "invented", ContentOrigin::Override});
  auto with_other = load_content(files);
  REQUIRE(with_other.has_value());
  // The shipped prompts and lexicon are others too, and the override's file is added beside them.
  REQUIRE(with_other->others.size() == pack->others.size() + 1);
  const auto added = std::find_if(with_other->others.begin(), with_other->others.end(),
                                  [](const ContentFile& f) { return f.path == "prompts/dialogue/00_frame.md"; });
  REQUIRE(added != with_other->others.end());
  CHECK(added->origin == ContentOrigin::Override);
  CHECK(std::any_of(pack->others.begin(), pack->others.end(),
                    [](const ContentFile& f) { return f.path == "lexicon/banned.toml"; }));
  CHECK(with_other->hash == pack->hash);  // other files are A's, outside the hash
}

TEST_CASE("errors name the file, the line and the key, and never quote a value") {
  SUBCASE("no index") {
    const std::vector<ContentFile> none;
    auto pack = load_content(none);
    REQUIRE_FALSE(pack.has_value());
    CHECK(has(pack.error(), "identity/personas.toml:0: is missing"));
  }
  SUBCASE("an unknown key") {
    const std::string bad = with(kWren, "[voice]", "secret_lore = \"the hidden boats\"\n[voice]");
    const auto e = errors_of(kIndex, bad.c_str());
    CHECK(has(e, "personas/wren.toml:9: (top): has an unknown key \"secret_lore\""));
    CHECK_FALSE(has(e, "hidden boats"));
  }
  SUBCASE("over a cap in words") {
    const std::string bad = with(kWren, "id = \"base\"", "id = \"base\"\nmood = \"one two three four five six seven eight nine\"");
    const auto e = errors_of(kIndex, bad.c_str());
    CHECK(has(e, "stages[0].mood: is longer than 8 words"));
    CHECK_FALSE(has(e, "seven eight"));
  }
  SUBCASE("too many openers, and an opener of two words") {
    const std::string bad = with(kWren, "openers = [\"So,\", \"Well,\"]", "openers = [\"A\", \"B\", \"C\", \"D\", \"E\"]");
    CHECK(has(errors_of(kIndex, bad.c_str()), "voice.openers: must hold 0 to 4 entries"));
    const std::string two = with(kWren, "openers = [\"So,\", \"Well,\"]", "openers = [\"Now then\"]");
    CHECK(has(errors_of(kIndex, two.c_str()), "voice.openers[0]: must be one word"));
  }
  SUBCASE("a value of the wrong type, and one outside its closed set") {
    const std::string bad = with(kWren, "pronouns = \"she\"", "pronouns = 3");
    CHECK(has(errors_of(kIndex, bad.c_str()), "pronouns: must be a string"));
    const std::string reg = with(kWren, "register = \"colloquial\"", "register = \"shouty\"");
    CHECK(has(errors_of(kIndex, reg.c_str()), "register: must be one of archaic-regal, formal, colloquial, broken, chant"));
  }
  SUBCASE("a shipped persona records its provenance (PL12)") {
    const std::string bad = with(kWren, "review = \"ci\"\n", "");
    CHECK(has(errors_of(kIndex, bad.c_str()), "drafted, review: a shipped persona must set both (PL12)"));
    const std::string drafted = with(kWren, "drafted = \"hand\"", "drafted = \"dreamt\"");
    CHECK(has(errors_of(kIndex, drafted.c_str()), "drafted: must be one of hand, assisted"));
  }
  SUBCASE("the base stage's rules") {
    const std::string bad = with(with(kWren, "when = \"true\"", "when = \"runes >= 1\""), "road = \"You mend nets by the ford.\"\n", "");
    const auto e = errors_of(kIndex, bad.c_str());
    CHECK(has(e, "stages[0].when: the base stage's must be \"true\""));
    CHECK(has(e, "stages[0].road: the base stage must set it"));
  }
  SUBCASE("a ci: beckon only on a test persona") {
    const std::string bad =
        with(kWren, "where = \"at the ford\"", "where = \"at the ford\"\nbeckon = \"a word\"\nbeckon_src = \"ci:x\"");
    CHECK(has(errors_of(kIndex, bad.c_str()), "stages[0].beckon_src: a ci: record only on a test_only persona"));
  }
  SUBCASE("the record's id against its row's, and a missing record") {
    const std::string bad = with(kWren, "id = \"wren\"", "id = \"other\"");
    CHECK(has(errors_of(kIndex, bad.c_str()), "id: must be the id its row in identity/personas.toml gives"));
    const std::vector<ContentFile> only_index = {ContentFile{"identity/personas.toml", kIndex, ContentOrigin::Shipped}};
    auto pack = load_content(only_index);
    REQUIRE_FALSE(pack.has_value());
    CHECK(has(pack.error(), "persona.file: \"personas/wren.toml\" is missing"));
  }
  SUBCASE("the index's own rules") {
    const std::string twice = std::string(kIndex) + "[[persona]]\nid = \"wren\"\nfile = \"personas/wren.toml\"\n";
    CHECK(has(errors_of(twice.c_str(), kWren), "persona.id: \"wren\" is not unique"));
    const std::string path = with(kIndex, "personas/wren.toml", "../wren.toml");
    CHECK(has(errors_of(path.c_str(), kWren), "persona.file: must be personas/<slug>.toml"));
    const std::string shared = std::string(kIndex) +
                               "[[persona]]\nid = \"tam\"\nfile = \"personas/tam.toml\"\nenabled = false\n"
                               "npc_params = [ { param = 4100, form = \"default\" } ]\n";
    CHECK(has(errors_of(shared.c_str(), kWren), "persona.npc_params: a param row is bound to two personas"));
  }
  SUBCASE("refused before it is parsed, and too large") {
    const std::string bad = with(kWren, "schema = 1", "schema = 1\n\x01");
    CHECK(has(errors_of(kIndex, bad.c_str()), "is refused before it is parsed"));
    std::string big = kWren;
    big += "# " + std::string(17 * 1024, 'x') + "\n";
    CHECK(has(errors_of(kIndex, big.c_str()), "personas/wren.toml:0: is larger than 16 KB"));
  }
  SUBCASE("a disabled row's record is not read") {
    const std::string off = with(kIndex, "npc_params", "enabled = false\nnpc_params");
    const std::vector<ContentFile> only_index = {ContentFile{"identity/personas.toml", off, ContentOrigin::Shipped}};
    auto pack = load_content(only_index);
    REQUIRE(pack.has_value());
    CHECK(pack->personas.empty());
  }
}
