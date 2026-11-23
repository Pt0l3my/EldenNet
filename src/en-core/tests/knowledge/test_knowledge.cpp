// Quest and world awareness (knowledge.hpp; DESIGN §10.6; MEM §6, §7, §12; S11.3.c, d): the shipped world files, the
// predicates in three values and their load errors, what holds, who knows it, block 5b's lines for the pilot, strict
// mode, the watch list and a frame, T5's byte stability, the developer-label firewall, the "you, now" line and the
// behaviour settings. T2 (the truth tables) is in tests/content/test_fixtures.cpp. Test data is invented.
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <toml.hpp>

#include "en/core/behaviour.hpp"
#include "en/core/core.hpp"
#include "en/core/knowledge.hpp"
#include "en/core/speech_content.hpp"
#include "../snapshots.hpp"
#include "../test_files.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

namespace {

constexpr std::array<const char*, 8> kStates = {"fresh",        "after_godrick", "two_runes",  "after_morgott",
                                                "after_mohg",   "erdtree_burned", "after_radahn", "after_rykard"};

// The NPC §3.6 rendering of Kalé's block 5b at after_radahn: his tie lines, then the world stage's (MEM §12.6).
constexpr const char* kStarTie = "Something came down out of the sky and shook the land. Nobody on the road can say what it means.";
constexpr const char* kCastleTie = "Travellers off the castle road say its lord is dead and his soldiers are left without a master.";
constexpr const char* kGeneral = "The red general who held the eastern wastes has fallen at last.";
constexpr const char* kTree = "The great tree has gone up in flames.";

struct World {
  ContentPack pack;
  Knowledge knowledge;
  std::size_t kale = 0;
};

World load(std::vector<ContentFile> files) {
  auto pack = load_content(files);
  REQUIRE(pack.has_value());
  auto k = Knowledge::build(*pack);
  if (!k) FAIL(describe(k.error().front()));
  World w{std::move(*pack), std::move(*k), 0};
  for (std::size_t i = 0; i < w.pack.personas.size(); ++i) {
    if (w.pack.personas[i].id == "kale") w.kale = i;
  }
  REQUIRE(w.pack.personas[w.kale].id == "kale");
  return w;
}

World shipped() { return load(builtin_content()); }

std::vector<ContentFile> with_pilot_overrides() {
  std::vector<ContentFile> files = builtin_content();
  for (ContentFile& f : files_under("tests/scenarios/talk/pilot-overrides/", ContentOrigin::Override)) files.push_back(std::move(f));
  return files;
}

FlagState state(const World& w, const char* name) {
  auto fs = w.knowledge.fixture(test_file(std::string("tests/fixtures/flagstates/") + name + ".toml"));
  REQUIRE_MESSAGE(fs.has_value(), name);
  return *fs;
}

// The shipped pack with one file's text changed: the errors Knowledge::build gives, as text.
std::vector<std::string> build_errors(const std::string& path, const std::string& from, const std::string& to) {
  std::vector<ContentFile> files = builtin_content();
  bool changed = false;
  for (ContentFile& f : files) {
    if (f.path != path) continue;
    const auto at = f.text.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, from);
    f.text.replace(at, from.size(), to);
    changed = true;
  }
  REQUIRE(changed);
  auto pack = load_content(files);
  REQUIRE(pack.has_value());
  auto k = Knowledge::build(*pack);
  std::vector<std::string> out;
  if (!k) {
    for (const ContentError& e : k.error()) out.push_back(describe(e));
  }
  return out;
}

bool has_part(const std::vector<std::string>& errors, const std::string& part) {
  return std::any_of(errors.begin(), errors.end(), [&](const std::string& e) { return e.find(part) != std::string::npos; });
}

// The pilot's whole dialogue prompt at a flag state, on the shipped templates: its system messages and the rest.
std::string render(const World& w, const DialoguePrompt& prompt, const FlagState& fs) {
  const WorldView view = w.knowledge.derive(fs);
  const Grounding g = w.knowledge.ground(w.kale, fs, view);
  const nlohmann::json record = w.pack.personas[w.kale].resolved(g.stage);
  const std::vector<std::string> recap = {"What news on the road?"};
  DialogueContext c;
  c.persona = "kale";
  c.record = &record;
  c.name = "Kalé";
  c.recap = recap;
  c.tied = g.tied;
  c.land = g.land;
  auto r = prompt.render(c);
  REQUIRE(r.has_value());
  std::string out;
  for (const proto::Message& m : r->messages) out += std::string(m.role == proto::Role::System ? "[system]\n" : "[other]\n") + m.joined() + "\n";
  return out;
}

std::shared_ptr<const DialoguePrompt> shipped_prompt(const World& w) {
  auto speech = load_speech(w.pack);
  REQUIRE(speech.has_value());
  return speech->prompt;
}

}  // namespace

TEST_CASE("the shipped world: nine world stages, Tanith's track, the regions' sky, and every alias a lead") {
  const World w = shipped();
  CHECK(w.knowledge.stages().size() == 9);
  REQUIRE(w.knowledge.tracks().size() == 1);
  CHECK(w.knowledge.tracks()[0].steps.size() == 2);
  const auto sky = [&](const char* key) {
    for (const Region& r : w.knowledge.regions()) {
      if (r.key == key) return r.sky;
    }
    FAIL("no region " << key);
    return false;
  };
  CHECK(sky("limgrave"));
  CHECK_FALSE(sky("roundtable"));
  CHECK_FALSE(sky("underground"));
  for (std::size_t i = 0; i < w.knowledge.catalogue().flags.size(); ++i) {
    CHECK_FALSE(w.knowledge.catalogue().verified(i, Realm::Game));
    CHECK_FALSE(w.knowledge.catalogue().verified(i, Realm::Eval));
  }
  // Nothing is verified before S6, so the core asks the game to read nothing (MEM §6.2).
  CHECK(w.knowledge.watch(Realm::Game, 1).ids.empty());
  CHECK(w.knowledge.watch(Realm::Eval, 1).ids.empty());
  REQUIRE(w.knowledge.catalogue().find("kale.howl_told").has_value());
  CHECK(w.knowledge.catalogue().flags[*w.knowledge.catalogue().find("kale.howl_told")].id == 0);  // unresolved
}

TEST_CASE("before a frame nothing holds: the pilot at his base stage, and block 5b his road alone") {
  const World w = shipped();
  const FlagState fs = w.knowledge.unread();
  const KnowledgeView v = w.knowledge.view(fs);
  for (const Tri t : v.world.stages) CHECK(t == Tri::U);
  CHECK(v.world.personas[w.kale] == 0);
  const Grounding& g = v.personas[w.kale];
  CHECK(g.known.empty());
  CHECK(g.tied == Standing{});
  CHECK(g.land == Standing{});
  // In game today: the same, since no alias is verified (a frame's bits are Unknown unverified).
  proto::FlagFrame frame;
  frame.watch_version = 1;
  frame.bits.set();
  frame.valid.set();
  const WatchList list = w.knowledge.watch(Realm::Game, 1);
  CHECK(w.knowledge.view(w.knowledge.from_frame(list, frame, Realm::Game, 1)).personas[w.kale].known.empty());
}

TEST_CASE("the pilot at after_radahn: block 5b as NPC §3.6 renders it, his tie lines first") {
  const World w = shipped();
  const FlagState fs = state(w, "after_radahn");
  const KnowledgeView v = w.knowledge.view(fs);
  const Grounding& g = v.personas[w.kale];
  CHECK(g.stage == 0);
  CHECK(g.tied == Standing{});  // no tie of his has a certainty: nothing is his own affair
  CHECK(g.land.known == std::vector<std::string>{kStarTie});
  CHECK(g.land.heard == std::vector<std::string>{kCastleTie, kGeneral});
  CHECK(g.shown == std::vector<std::string>{"w:star_fallen", "w:castle_fallen", "w:general_fallen"});
  CHECK(g.offered.empty());
  const auto prompt = shipped_prompt(w);
  const std::string text = render(w, *prompt, fs);
  CHECK(text.find("As things stand now:\nYour own road: You keep shop in the ruined church by the grassland road, trading "
                  "plain goods and listening for whatever news the road brings in.\nIn the land, you know: " +
                  std::string(kStarTie) + "\nIn the land, you have heard: " + kCastleTie + " " + kGeneral + "\n") !=
        std::string::npos);
}

TEST_CASE("the pilot at erdtree_burned: four lines at most within 200 tokens, the rest offered to block 7") {
  const World w = shipped();
  const FlagState fs = state(w, "erdtree_burned");
  const Grounding g = w.knowledge.view(fs).personas[w.kale];
  CHECK(g.known.size() == 5);  // the castle, the queen, the general, the star and the tree
  CHECK(g.shown == std::vector<std::string>{"w:star_fallen", "w:castle_fallen", "w:erdtree_burned", "w:general_fallen"});
  CHECK(g.offered == std::vector<std::string>{"w:queen_fallen"});
  CHECK(g.land.known == std::vector<std::string>{kStarTie, kTree});
  CHECK(g.land.heard == std::vector<std::string>{kCastleTie, kGeneral});
  const auto prompt = shipped_prompt(w);
  const nlohmann::json record = w.pack.personas[w.kale].resolved(g.stage);
  DialogueContext c;
  c.persona = "kale";
  c.record = &record;
  c.name = "Kalé";
  c.tied = g.tied;
  c.land = g.land;
  const auto tokens = prompt->block_tokens(c, Block::Standing);
  REQUIRE(tokens.has_value());
  CHECK(*tokens <= 200);
}

TEST_CASE("strict: an unverified flag keeps its stage Unknown, Unknown is silent, and spoiler 2 needs every atom verified") {
  const World w = shipped();
  FlagState fs = state(w, "after_radahn");
  const auto star = *w.knowledge.catalogue().find("story.star_fell");
  fs.verified[star] = 0;  // MEM §12.11 T9: 310 unverified
  const WorldView view = w.knowledge.derive(fs);
  CHECK(view.stages[3] == Tri::U);  // star_fallen
  std::map<std::string, std::string> known;
  for (const Known& k : w.knowledge.known(w.kale, fs, view)) known[k.item] = to_string(k.certainty);
  CHECK_FALSE(known.contains("w:star_fallen"));
  CHECK(known["w:general_fallen"] == "rumour");
  // A world stage of spoiler 2 whose `when` holds by Kleene's rule while one atom is unverified stays Unknown (MEM §7.4).
  const std::vector<ContentFile> files = [&] {
    std::vector<ContentFile> f = builtin_content();
    const std::string from = "when    = \"flag('story.erdtree_burned')\"";
    for (ContentFile& c : f) {
      if (c.path == "world/stages.toml") {
        REQUIRE(c.text.find(from) != std::string::npos);
        c.text.replace(c.text.find(from), from.size(), "when    = \"flag('story.erdtree_burned') || boss('maliketh')\"");
      }
    }
    return f;
  }();
  const World x = load(files);
  FlagState burned = state(x, "erdtree_burned");
  CHECK(x.knowledge.derive(burned).stages[7] == Tri::T);
  burned.verified[*x.knowledge.catalogue().find("boss.maliketh")] = 0;
  CHECK(x.knowledge.derive(burned).stages[7] == Tri::U);  // the || holds, but an atom is unverified
  CHECK(x.knowledge.eval("flag('story.erdtree_burned') || boss('maliketh')", burned, x.knowledge.derive(burned)) == Tri::T);
}

TEST_CASE("predicates in three values: Kleene's and, or and not; runes, journey, stage and world") {
  const World w = shipped();
  const FlagState none = w.knowledge.unread();
  const FlagState two = state(w, "two_runes");
  const FlagState godrick = state(w, "after_godrick");
  const WorldView vn = w.knowledge.derive(none);
  const WorldView v2 = w.knowledge.derive(two);
  CHECK(w.knowledge.eval("true && !false", none, vn) == Tri::T);
  CHECK(w.knowledge.eval("boss('godrick')", none, vn) == Tri::U);
  CHECK(w.knowledge.eval("!boss('godrick')", none, vn) == Tri::U);  // so a negation never unlocks on unread data
  CHECK(w.knowledge.eval("boss('godrick') && false", none, vn) == Tri::F);
  CHECK(w.knowledge.eval("boss('godrick') || true", none, vn) == Tri::T);
  CHECK(w.knowledge.eval("boss('godrick') && boss('rennala')", two, v2) == Tri::T);
  CHECK(w.knowledge.eval("runes >= 2", two, v2) == Tri::T);
  CHECK(w.knowledge.eval("runes >= 2", godrick, w.knowledge.derive(godrick)) == Tri::F);
  CHECK(w.knowledge.eval("runes >= 0", none, vn) == Tri::T);
  CHECK(w.knowledge.eval("journey >= 1", two, v2) == Tri::T);
  CHECK(w.knowledge.eval("journey >= 2", two, v2) == Tri::F);
  CHECK(w.knowledge.eval("world('castle_fallen')", two, v2) == Tri::T);
  CHECK(w.knowledge.eval("world('star_fallen')", two, v2) == Tri::F);
  CHECK(w.knowledge.eval("stage('kale', 'howl_told')", two, v2) == Tri::F);
  CHECK(w.knowledge.eval("stage('kale', 'howl_told')", none, vn) == Tri::U);
  CHECK(w.knowledge.eval("region('limgrave')", two, v2) == Tri::U);  // the place group maps no region yet
  CHECK(w.knowledge.eval("flag('no.such_alias')", two, v2) == Tri::U);  // does not compile: Unknown
}

TEST_CASE("the predicates' load errors: an unknown alias, a number, knows(), W1, a cycle, never quoting the text") {
  const std::string kale = "personas/kale.toml";
  const std::string howl = "when = \"flag('kale.howl_told')\"";
  // The line of his second stage's table, where the error points.
  std::size_t line = 0;
  for (const ContentFile& f : builtin_content()) {
    if (f.path != kale) continue;
    std::size_t seen = 0;
    for (std::size_t i = 0, n = 1; i < f.text.size(); ++i) {
      if (f.text.compare(i, 11, "[[stages]]\n") == 0 && (i == 0 || f.text[i - 1] == '\n') && ++seen == 2) line = n;
      if (f.text[i] == '\n') ++n;
    }
  }
  REQUIRE(line > 0);
  CHECK(has_part(build_errors(kale, howl, "when = \"flag('kale.secret_word')\""),
                 "personas/kale.toml:" + std::to_string(line) + ": stages[1].when: flag() names no alias of the catalogue"));
  CHECK_FALSE(has_part(build_errors(kale, howl, "when = \"flag('kale.secret_word')\""), "secret_word"));
  CHECK(has_part(build_errors(kale, howl, "when = \"flag(4717)\""), "a call takes a quoted name, never a number"));
  CHECK(has_part(build_errors(kale, howl, "when = \"knows('star_fallen')\""), "knows() belongs to the KB"));
  CHECK(has_part(build_errors(kale, howl, "when = \"runes >= 9\""), "runes takes 0 to 7"));
  CHECK(has_part(build_errors(kale, howl, "when = \"flag('kale.howl_told') &&\""), "a term is expected"));
  CHECK(has_part(build_errors(kale, howl, "when = \"stage('kale', 'howl_told')\""), "reads itself in a cycle"));
  CHECK(has_part(build_errors("world/stages.toml", "when    = \"boss('rennala')\"", "when    = \"!boss('rennala')\""),
                 "world_stage[1].when: a world stage's when uses outcome flags and world() only (W1)"));
  CHECK(has_part(build_errors("world/stages.toml", "when    = \"boss('rennala')\"", "when    = \"world('queen_fallen')\""),
                 "reads itself in a cycle"));
  CHECK(has_part(build_errors(kale, "at = \"limgrave\"", "at = \"the_moon\""), "stages[0].at: names no region"));
  CHECK(has_part(build_errors(kale, "item    = \"w:star_fallen\"", "item    = \"w:sky_fallen\""),
                 "awareness.tie[0].item: names no world stage"));
  CHECK(has_part(build_errors("identity/personas.toml", "base = 4700", "base = 4720"),
                 "persona.flag_block: must agree with the catalogue's block"));
  CHECK(has_part(build_errors("flags/catalog.toml", "verified = false\nspoiler = 1\nmatch = [\"godrick\", \"grafted\"]",
                              "verified = true\nspoiler = 1\nmatch = [\"godrick\", \"grafted\"]"),
                 "flag[0].verified: needs its evidence"));
}

TEST_CASE("the howl stage: Unknown in game, false at every fixture state, and shown once its flag is set") {
  const World w = shipped();
  CHECK(w.knowledge.active_stage(w.kale, w.knowledge.unread()) == 0);
  CHECK(w.knowledge.active_stage(w.kale, state(w, "after_radahn")) == 0);
  auto told = w.knowledge.fixture("schema = 1\nstate = \"howl\"\n[flags]\n\"kale.howl_told\" = true\n");
  REQUIRE(told.has_value());
  CHECK(w.knowledge.active_stage(w.kale, *told) == 1);
  const nlohmann::json r = w.pack.personas[w.kale].resolved(1);
  CHECK(r["stage"]["mood"] == "easygoing, curious how that went");
  CHECK(r["stage"]["road"] == w.pack.personas[w.kale].resolved(0)["stage"]["road"]);  // inherited
  // A fixture names catalogue aliases only.
  CHECK_FALSE(w.knowledge.fixture("schema = 1\n[flags]\n\"kale.secret\" = true\n").has_value());
}

TEST_CASE("Tanith's track at after_rykard: its last step whose flags are set holds, and no v0.2 persona knows it") {
  const World w = shipped();
  const FlagState fs = state(w, "after_rykard");
  const WorldView v = w.knowledge.derive(fs);
  REQUIRE(v.tracks.size() == 1);
  REQUIRE(v.tracks[0].has_value());
  CHECK(*v.tracks[0] == 1);  // vigil
  for (const Known& k : w.knowledge.known(w.kale, fs, v)) CHECK(k.item.rfind("q:", 0) != 0);
}

TEST_CASE("the watch list and a frame: a ci: record verifies in the eval's realm alone") {
  const World w = load(with_pilot_overrides());
  const WatchList eval = w.knowledge.watch(Realm::Eval, 3);
  CHECK(w.knowledge.watch(Realm::Game, 3).ids.empty());
  REQUIRE(eval.ids.size() == w.knowledge.catalogue().flags.size());
  CHECK(std::is_sorted(eval.ids.begin(), eval.ids.end()));
  // A frame of the list's version sets what it reads: the general's fall and the star at after_radahn's aliases.
  proto::FlagFrame frame;
  frame.watch_version = 3;
  frame.valid.set();
  for (std::size_t b = 0; b < eval.ids.size(); ++b) {
    const std::string& alias = w.knowledge.catalogue().flags[eval.flags[b]].alias;
    frame.bits[b] = alias == "boss.godrick" || alias == "boss.radahn" || alias == "story.star_fell";
  }
  const FlagState fs = w.knowledge.from_frame(eval, frame, Realm::Eval, 1);
  CHECK(fs.have_frame);
  CHECK(w.knowledge.view(fs) == w.knowledge.view(state(w, "after_radahn")));
  // In the game's realm the same frame verifies nothing; a frame of an older list reads nothing.
  CHECK(w.knowledge.view(w.knowledge.from_frame(eval, frame, Realm::Game, 1)).personas[w.kale].known.empty());
  frame.watch_version = 2;
  CHECK_FALSE(w.knowledge.from_frame(eval, frame, Realm::Eval, 1).have_frame);
}

TEST_CASE("T5: blocks 1 to 5 never change with the world; block 5b only with what the pilot knows") {
  const World w = shipped();
  const auto prompt = shipped_prompt(w);
  std::map<std::string, std::string> by_state;
  std::string head;
  for (const char* s : kStates) {
    const std::string text = render(w, *prompt, state(w, s));
    CHECK(text == render(w, *prompt, state(w, s)));  // the same inputs, the same bytes
    const std::string before = text.substr(0, text.find("As things stand now:"));
    if (head.empty()) head = before;
    CHECK(before == head);
    by_state[s] = text;
  }
  CHECK(render(w, *prompt, w.knowledge.unread()).substr(0, head.size()) == head);
  // The lord of blood's fall reaches him by no path, so after_mohg's prompt is after_morgott's, byte for byte.
  CHECK(by_state["after_mohg"] == by_state["after_morgott"]);
  CHECK(by_state["fresh"] != by_state["after_godrick"]);
  CHECK(by_state["after_radahn"] != by_state["after_godrick"]);
}

TEST_CASE("the developer-label firewall: no alias and no four-word run of a note in the pilot's prompts (MEM §7.6)") {
  const World w = shipped();
  const auto prompt = shipped_prompt(w);
  std::string catalogue_text;
  for (const ContentFile& f : builtin_content()) {
    if (f.path == "flags/catalog.toml") catalogue_text = f.text;
  }
  auto catalogue = toml::parse(catalogue_text);
  std::vector<std::string> runs;
  std::vector<std::string> aliases;
  if (catalogue) {
    for (const toml::node& n : *catalogue.table()["flag"].as_array()) {
      aliases.push_back((*n.as_table())["alias"].value_or(std::string()));
      std::vector<std::string> words;
      std::string word;
      for (const char c : (*n.as_table())["note"].value_or(std::string()) + " ") {
        if (c == ' ') {
          if (!word.empty()) words.push_back(word);
          word.clear();
        } else {
          word += c;
        }
      }
      for (std::size_t i = 0; i + 4 <= words.size(); ++i) {
        runs.push_back(words[i] + " " + words[i + 1] + " " + words[i + 2] + " " + words[i + 3]);
      }
    }
  }
  REQUIRE(aliases.size() == w.knowledge.catalogue().flags.size());
  REQUIRE_FALSE(runs.empty());
  for (const char* s : kStates) {
    const std::string text = render(w, *prompt, state(w, s));
    for (const std::string& a : aliases) CHECK_MESSAGE(text.find(a) == std::string::npos, a);
    for (const std::string& r : runs) CHECK_MESSAGE(text.find(r) == std::string::npos, r);
  }
}

TEST_CASE("the \"you, now\" line: wounds by DESIGN §5.5's bands, the fight, and nothing when calm or switched off") {
  CHECK(you_now(proto::Meter{100, 100}, false, true).empty());
  CHECK(you_now(proto::Meter{75, 100}, false, true).empty());  // 75 per cent is still unhurt
  CHECK(you_now(proto::Meter{74, 100}, false, true) == "bloodied");
  CHECK(you_now(proto::Meter{50, 100}, false, true) == "bloodied");
  CHECK(you_now(proto::Meter{49, 100}, false, true) == "badly hurt");
  CHECK(you_now(proto::Meter{24, 100}, false, true) == "barely standing");
  CHECK(you_now(proto::Meter{100, 100}, true, true) == "unhurt, in the thick of a fight");
  CHECK(you_now(proto::Meter{30, 100}, true, true) == "badly hurt, in the thick of a fight");
  CHECK(you_now(proto::Meter{0, 0}, false, true).empty());  // HP not read: nothing to say
  CHECK(you_now(proto::Meter{10, 100}, true, false).empty());  // self_awareness off
}

TEST_CASE("the behaviour settings: defaults, bounds, and the core keeping the old on a value out of bounds") {
  CHECK_FALSE(check_behaviour(Behaviour{}).has_value());
  Behaviour b;
  b.range.talk_m = 2.0f;
  CHECK(check_behaviour(b) == std::optional<std::string>("range.talk_m"));
  b.range.talk_m = 16.0f;
  b.range.hear_m = std::numeric_limits<float>::quiet_NaN();
  CHECK(check_behaviour(b) == std::optional<std::string>("range.hear_m"));
  b.range.hear_m = 8.0f;
  b.range.fight_m = 41.0f;
  CHECK(check_behaviour(b) == std::optional<std::string>("range.fight_m"));
  Core core;
  Behaviour off;
  off.features.chat = false;
  CHECK_FALSE(core.set_behaviour(off).has_value());
  CHECK_FALSE(core.behaviour().features.chat);
  CHECK(core.set_behaviour(b) == std::optional<std::string>("range.fight_m"));
  CHECK_FALSE(core.behaviour().features.chat);  // the old stays
}

namespace {

// A session through the core, its own speech, with the pilot bound to the fake game's invented row 81200.
struct PilotSession {
  Core core;
  Mono t = 0;
  Outputs last;
  std::optional<proto::GameCommand::SetWatchFlags> watch;
  std::map<std::string, bool> flags;  // the fixture's aliases, read by the watch list's ids
  proto::Meter hp{100, 100};

  Outputs feed(CoreInput in) {
    last = core.step(std::move(in), t);
    for (const proto::GameCommand& c : last.game) {
      if (const auto* w = std::get_if<proto::GameCommand::SetWatchFlags>(&c.payload)) watch = *w;
    }
    return last;
  }
  proto::WorldSnapshot snapshot() const {
    proto::WorldSnapshot s = test::world(t);
    s.player->handle = proto::ChrHandle{1};
    s.player->hp = proto::Meter{400, 400};
    s.ui.hud = proto::HudState::Default;
    proto::Chr c;
    c.handle = proto::ChrHandle{0x600};
    c.param = 81200;
    c.dist_m = 2.0f;
    c.bearing_deg = 3.0f;
    c.hp = hp;
    (void)s.chars.try_push_back(c);
    if (watch && watch->ids) {
      proto::FlagFrame f;
      f.watch_version = watch->version;
      f.valid.set();
      const Catalogue& cat = core.knowledge().catalogue();
      for (std::size_t b = 0; b < watch->ids->size(); ++b) {
        for (const CatalogueFlag& x : cat.flags) {
          if (x.id == (*watch->ids)[b]) f.bits[b] = flags.contains(x.alias) && flags.at(x.alias);
        }
      }
      s.flags = f;
    }
    return s;
  }
  void world(int n) {
    for (int i = 0; i < n; ++i) {
      t += 250;
      (void)feed(in(snapshot()));
    }
  }
  std::string ask(const std::string& line) {
    Outputs o = feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
    o = feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{line}}});
    REQUIRE(o.requests.size() == 1);
    std::string text;
    const auto* chat = std::get_if<proto::ChatRequest>(&o.requests[0].body);
    REQUIRE(chat != nullptr);
    for (const proto::Message& m : chat->messages) text += m.joined() + "\n";
    return text;
  }
};

}  // namespace

TEST_CASE("through the core: the watch list goes out once, a frame of it grounds the pilot's prompt, the title forgets it") {
  PilotSession s;
  REQUIRE(s.core.load_content(with_pilot_overrides(), Realm::Eval).empty());
  (void)s.feed(in(data_ready()));
  REQUIRE(s.watch.has_value());
  REQUIRE(s.watch->ids != nullptr);
  CHECK(s.watch->ids->size() == s.core.knowledge().catalogue().flags.size());
  s.watch.reset();
  (void)s.feed(CoreInput{proto::NetHealth{true}});
  CHECK_FALSE(s.watch.has_value());  // once per load
  s.watch = proto::GameCommand::SetWatchFlags{s.core.watch_list().version,
                                              std::make_shared<const std::vector<std::uint32_t>>(s.core.watch_list().ids)};
  s.flags = {{"boss.godrick", true}, {"boss.radahn", true}, {"story.star_fell", true}};
  (void)s.feed(in(playthrough_ready()));
  s.world(30);
  REQUIRE(s.core.state().phase == Phase::InWorld);
  CHECK(s.core.flags().have_frame);
  const std::string text = s.ask("What news on the road?");
  CHECK(text.find(std::string("In the land, you know: ") + kStarTie) != std::string::npos);
  CHECK(text.find(std::string("In the land, you have heard: ") + kCastleTie + " " + kGeneral) != std::string::npos);
  CHECK(text.find("You, now:") == std::string::npos);  // unhurt and calm: no line
  // The game's realm verifies none of the test catalogue's ci: records, so it watches nothing and sends nothing.
  Core game;
  REQUIRE(game.load_content(with_pilot_overrides(), Realm::Game).empty());
  CHECK(game.watch_list().ids.empty());
  const Outputs o = game.step(in(data_ready()), 0);
  CHECK(std::none_of(o.game.begin(), o.game.end(), [](const proto::GameCommand& c) {
    return std::holds_alternative<proto::GameCommand::SetWatchFlags>(c.payload);
  }));
}

TEST_CASE("through the core: a bloodied pilot says so in block 9, unless self-awareness is off; chat off opens nothing") {
  const auto boot = [](PilotSession& s, const Behaviour& b) {
    REQUIRE(s.core.load_content(with_pilot_overrides(), Realm::Eval).empty());
    REQUIRE_FALSE(s.core.set_behaviour(b).has_value());
    (void)s.feed(in(data_ready()));
    (void)s.feed(CoreInput{proto::NetHealth{true}});
    (void)s.feed(in(playthrough_ready()));
    s.hp = proto::Meter{60, 100};
    s.world(30);
  };
  PilotSession aware;
  boot(aware, Behaviour{});
  CHECK(aware.ask("Are you hurt?").find("You, now: bloodied.") != std::string::npos);
  PilotSession quiet;
  Behaviour off_self;
  off_self.features.self_awareness = false;
  boot(quiet, off_self);
  CHECK(quiet.ask("Are you hurt?").find("You, now:") == std::string::npos);
  // features.chat off: an open conversation ends at the next step, and the key opens nothing, with no hint.
  Behaviour off;
  off.features.chat = false;
  REQUIRE_FALSE(quiet.core.set_behaviour(off).has_value());
  quiet.world(1);
  CHECK(quiet.core.conversation().state() == proto::ConvState::Closed);
  CHECK(quiet.last.ended == EndReason::Closed);
  quiet.world(20);
  CHECK_FALSE(quiet.last.ui.hint.has_value());
  const Outputs o = quiet.feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
  CHECK(o.ui.chat.view != proto::BoxView::Open);
}
