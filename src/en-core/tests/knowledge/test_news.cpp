// News since the last meeting (knowledge.hpp; MEM §12.6, §12.13, §12.11): T12, the reaction cue equal to
// tests/knowledge/cues.toml for the pilot and the CI fixture persona test_stake at every row; T4's since-line goldens
// for the pilot's three transitions; and W5's and W7's cue clauses over the four strings, each with a failing fixture,
// the strings held to block 7's template word for word. Invented data: the pilot's test catalogue and fixture states.
#include <doctest.h>

#include <string>
#include <toml.hpp>
#include <vector>

#include "../test_files.hpp"
#include "en/core/core.hpp"
#include "en/core/knowledge.hpp"
#include "en/core/news_lint.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

namespace {

struct World {
  ContentPack pack;
  Knowledge knowledge;
  [[nodiscard]] std::size_t persona(std::string_view id) const {
    for (std::size_t i = 0; i < pack.personas.size(); ++i) {
      if (pack.personas[i].id == id) return i;
    }
    FAIL("no persona " << id);
    return 0;
  }
};

// The pilot's overrides with test_stake bound beside him, test_only, at an invented row.
World world() {
  std::vector<ContentFile> files = builtin_content();
  for (ContentFile& f : files_under("tests/scenarios/talk/pilot-overrides/", ContentOrigin::Override)) {
    if (f.path == "identity/personas.toml") {
      f.text +=
          "\n[[persona]]\nid = \"test_stake\"\nfile = \"personas/test_stake.toml\"\n"
          "npc_params = [ { param = 81900, form = \"default\", expect_name_id = 0, verified = true } ]\n"
          "test_only = true\n";
    }
    files.push_back(std::move(f));
  }
  files.push_back(ContentFile{"personas/test_stake.toml",
                              std::string(test_file("tests/fixtures/personas/test_stake.toml")),
                              ContentOrigin::Override});
  auto pack = load_content(files);
  REQUIRE_MESSAGE(pack.has_value(), (pack ? std::string() : describe(pack.error().front())));
  auto k = Knowledge::build(*pack);
  REQUIRE_MESSAGE(k.has_value(), (k ? std::string() : describe(k.error().front())));
  return World{std::move(*pack), std::move(*k)};
}

FlagState state(const World& w, const std::string& name) {
  auto fs = w.knowledge.fixture(test_file("tests/fixtures/flagstates/" + name + ".toml"));
  REQUIRE_MESSAGE(fs.has_value(), name);
  return *fs;
}

constexpr std::int64_t kNow = 100LL * 3600000;

// The record of one earlier meeting at `prior`, `met_h` hours ago.
NewsRecord record(const World& w, std::size_t p, const std::string& prior, double met_h) {
  NewsRecord r;
  r.pt_now = kNow;
  if (prior == "none") return r;
  const FlagState fs = state(w, prior);
  const WorldView view = w.knowledge.derive(fs);
  for (const Known& k : w.knowledge.known(p, fs, view)) r.seen.push_back(Seen{k.item, k.certainty});
  r.seen_pt = kNow - static_cast<std::int64_t>(met_h * 3600000);
  return r;
}

}  // namespace

TEST_CASE("T12: the reaction cue equals the cue table, for the pilot and test_stake at every row") {
  const World w = world();
  auto parsed = toml::parse(test_file("tests/knowledge/cues.toml"));
  REQUIRE(parsed);
  const toml::table& table = parsed.table();
  std::size_t rows = 0;
  for (auto&& [state_name, by_persona] : table) {
    if (!by_persona.is_table()) continue;
    for (auto&& [persona_id, variants] : *by_persona.as_table()) {
      for (auto&& [variant, node] : *variants.as_table()) {
        const toml::table& row = *node.as_table();
        const std::string name =
            std::string(state_name.str()) + "." + std::string(persona_id.str()) + "." + std::string(variant.str());
        CAPTURE(name);
        const std::size_t p = w.persona(persona_id.str());
        const FlagState fs = state(w, std::string(state_name.str()));
        const WorldView view = w.knowledge.derive(fs);
        NewsRecord r = record(w, p, row["prior"].value_or(std::string("none")), row["met_h"].value_or(0.0));
        if (const auto aw = w.pack.personas[p].record.find("awareness"); aw != w.pack.personas[p].record.end()) {
          r.temperament = aw->value("news", std::string("plain"));
        }
        // A claim is proved when the boss's flag holds and the persona knows the world stage of that boss (MEM §12.7).
        if (const toml::array* claims = row["claims"].as_array()) {
          for (const toml::node& c : *claims) {
            const std::string key = c.value_or(std::string());
            const std::string boss = key.substr(5);
            const bool fell = w.knowledge.eval("boss('" + boss + "')", fs, view) == Tri::T;
            bool knows = false;
            for (const Known& k : w.knowledge.known(p, fs, view, r.seen)) {
              for (const WorldStage& ws : w.knowledge.stages()) {
                if (k.item == "w:" + ws.id && ws.boss == boss) knows = true;
              }
            }
            if (fell && knows) r.told_bosses.push_back(boss);
          }
        }
        if (const toml::array* cued = row["cued"].as_array()) {
          for (const toml::node& c : *cued) {
            const toml::array& pair = *c.as_array();
            r.cued.emplace_back(pair[0].value_or(std::string()), kNow - pair[1].value_or(std::int64_t{0}) * 1000);
          }
        }
        const std::optional<Cue> cue = w.knowledge.cue(p, fs, view, r);
        const std::string got = cue ? cue->item + ":" + to_string(cue->why) : "none";
        CHECK(got == row["expect"].value_or(std::string("?")));
        ++rows;
      }
    }
  }
  CHECK(rows >= 11);
}

TEST_CASE("T4: the pilot's since-line for three transitions, and none when nothing is new or he never met") {
  const World w = world();
  const std::size_t kale = w.persona("kale");
  const auto since = [&](const char* prior, const char* now) {
    const FlagState fs = state(w, now);
    return w.knowledge.since_line(kale, fs, w.knowledge.derive(fs), record(w, kale, prior, 2.0));
  };
  const std::string first = since("fresh", "after_godrick");
  CHECK(first.rfind("Since you last spoke with the Tarnished, you have learned of ", 0) == 0);
  CHECK(first.back() == '.');
  CHECK(first.find(w.knowledge.short_of("w:castle_fallen")) != std::string::npos);
  const std::string second = since("after_godrick", "after_radahn");
  CHECK(second.find(w.knowledge.short_of("w:castle_fallen")) == std::string::npos);  // held at the meeting
  CHECK_FALSE(second.empty());
  const std::string third = since("after_morgott", "erdtree_burned");
  CHECK_FALSE(third.empty());
  for (const std::string& s : {first, second, third}) {
    CHECK(static_cast<double>(s.size()) / 3.6 * 1.15 <= 55.0);  // MEM §12.6: at most 55 tokens
  }
  CHECK(since("after_radahn", "after_radahn").empty());  // nothing new
  CHECK(since("none", "after_godrick").empty());         // never met: no record of what he knew
}

TEST_CASE("W5 and W7: the cue's four strings with every short, each clause with a failing fixture") {
  const World w = world();
  std::vector<std::string> shorts;
  for (const WorldStage& s : w.knowledge.stages()) shorts.push_back(s.short_text);
  CHECK(lint_cue_strings(kCueStrings, shorts).empty());
  // Block 7's template holds the four strings word for word.
  std::string tpl;
  for (const ContentFile& f : builtin_content()) {
    if (f.path == "prompts/dialogue/07_knowledge.j2") tpl = f.text;
  }
  REQUIRE_FALSE(tpl.empty());
  for (const std::string_view s : kCueStrings) {
    std::string t(s);
    t.replace(t.find("{short}"), 7, "{{ cue.short }}");
    CHECK_MESSAGE(tpl.find(t) != std::string::npos, t);
  }
  // W5: one short long enough that a string passes 35 tokens.
  const std::vector<std::string> long_short = {std::string(120, 'a')};
  const auto w5 = lint_cue_strings(kCueStrings, long_short);
  REQUIRE_FALSE(w5.empty());
  CHECK(w5.front().find("W5") != std::string::npos);
  // W7: an imperative to speak, a capitalised word, the Tarnished outside the Told string.
  std::array<std::string_view, 4> bad = kCueStrings;
  bad[0] = "On your mind: {short}, so mention it.";
  CHECK(lint_cue_strings(bad, shorts).front().find("W7") != std::string::npos);
  bad = kCueStrings;
  bad[2] = "On your mind: {short}, the talk of Limgrave.";
  CHECK(lint_cue_strings(bad, shorts).front().find("W7") != std::string::npos);
  bad = kCueStrings;
  bad[3] = "On your mind: {short}, which the Tarnished brought.";
  CHECK(lint_cue_strings(bad, shorts).front().find("W7") != std::string::npos);
}
