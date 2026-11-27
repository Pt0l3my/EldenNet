// X1 outside the game (eval_x1.hpp; NPC §12.2; O139 to O142): the set and its items, the jobpacks within Jobrunner's
// caps, the bounded names, and the import of Jobrunner's results, on our own invented sample
// (tests/x1/sample-results.jsonl, written by us, never from a real run).
#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "eval_run.hpp"
#include "eval_x1.hpp"
#include "golden_run.hpp"

using namespace en;
using namespace en::tools;

namespace {

const std::filesystem::path kX1 = EN_TOOLS_X1;

X1Set the_set() {
  const auto text = read_file((kX1 / "x1.toml").string(), 256 * 1024);
  REQUIRE(text.has_value());
  auto set = parse_x1_set(*text, kX1);
  REQUIRE_MESSAGE(set.has_value(), (set.has_value() ? std::string() : set.error()));
  return std::move(*set);
}

}  // namespace

TEST_CASE("x1: the archaic-regal fixture's archaic sentences, counted though its register allows them") {
  const auto path = std::filesystem::path(EN_TOOLS_SCENARIOS) / "talk" / "x1-voice-regal.toml";
  auto scenario = parse_eval_scenario(*read_file(path.string(), 256 * 1024), path.parent_path());
  REQUIRE(scenario.has_value());
  auto regal = run_eval(*scenario, EvalLlm::Mock);
  REQUIRE(regal.has_value());
  CHECK(regal->register_name == "archaic-regal");
  std::uint32_t archaic = 0, sentences = 0;
  for (const EvalTurn& t : regal->turns) {
    const auto a = t.first_try(regal->attempts);
    REQUIRE(a.has_value());
    const SpeechCounts& c = regal->attempts[*a].counts;
    archaic += c.archaic_sentences;
    sentences += c.sentences;
    CHECK(c.soft[7] == 0);  // no Archaic soft hit: the register allows them
  }
  CHECK(archaic > 0);
  CHECK(archaic < sentences);
}

TEST_CASE("x1: the set's errors name their line, never a value") {
  const auto bad_kind = parse_x1_set(
      "schema = 1\nname = \"x1\"\n[[run]]\npack = \"p\"\ncode = \"v\"\nkind = \"chat\"\nscenario = \"a.toml\"\n"
      "arms = [\"positive\"]\n",
      ".");
  REQUIRE_FALSE(bad_kind.has_value());
  CHECK(bad_kind.error() == "line 3: run[0].kind: must be voice or test_question");
  const auto bad_arm = parse_x1_set(
      "schema = 1\nname = \"x1\"\n[[run]]\npack = \"p\"\ncode = \"v\"\nkind = \"voice\"\nscenario = \"a.toml\"\n"
      "arms = [\"other\"]\n",
      ".");
  REQUIRE_FALSE(bad_arm.has_value());
  CHECK(bad_arm.error().find("arms: each must be positive or an arm the set defines") != std::string::npos);
  CHECK_FALSE(parse_x1_set("schema = 1\nname = \"x1\"\n", ".").has_value());
  CHECK_FALSE(parse_x1_set("schema = 1\nname = \"x1\"\n[arm.positive]\noverrides = \"a\"\n", ".").has_value());
  const auto repeat = parse_x1_set(
      "schema = 1\nname = \"x1\"\n[[run]]\npack = \"p\"\ncode = \"v\"\nkind = \"voice\"\nscenario = \"a.toml\"\n"
      "arms = [\"positive\"]\nrepeat = 11\n",
      ".");
  REQUIRE_FALSE(repeat.has_value());
  CHECK(repeat.error() == "line 3: run[0].repeat: 1 to 10");
}

TEST_CASE("x1: the set's items, each turn the prompt the core renders after our own history, in Jobrunner's caps") {
  const X1Set set = the_set();
  auto items = build_x1_items(set);
  REQUIRE_MESSAGE(items.has_value(), (items.has_value() ? std::string() : items.error()));
  // Five voice sets of fifty turns in two arms, and twelve test questions in one.
  CHECK(items->size() == 5 * 50 * 2 + 12);
  std::set<std::string> ids;
  std::size_t questions = 0;
  for (const X1Item& i : *items) {
    CHECK(ids.insert(i.id).second);
    REQUIRE(i.messages.is_array());
    REQUIRE(i.messages.size() >= 2);
    CHECK(i.messages.front()["role"] == "system");
    CHECK(i.messages.back()["role"] == "user");
    // The turn's own line is the last thing the prompt holds of the conversation.
    CHECK(i.messages.back()["content"].get<std::string>().find(i.player) != std::string::npos);
    CHECK(i.context.words_max > 0);
    if (i.kind == "test_question") {
      ++questions;
      CHECK(i.repeat == 3);
      CHECK(i.id.size() > 3);
      CHECK(i.id.substr(i.id.size() - 3) == "-x3");
      CHECK_FALSE(i.topics.empty());
      // A first meeting: no earlier line of the conversation in its prompt (a restart before it).
      CHECK(i.messages.size() == 2);
    }
  }
  CHECK(questions == 12);
  // The arms differ in their prompts and in nothing else.
  const auto by_id = [&](const char* id) {
    return std::find_if(items->begin(), items->end(), [&](const X1Item& i) { return i.id == id; });
  };
  const auto pos = by_id("positive-kale-voice-t08");
  const auto old = by_id("former-kale-voice-t08");
  REQUIRE(pos != items->end());
  REQUIRE(old != items->end());
  CHECK(pos->messages != old->messages);
  CHECK(pos->player == old->player);
  CHECK(pos->messages[0]["content"].get<std::string>().find("Of great events not written there") == std::string::npos);
  CHECK(old->messages[0]["content"].get<std::string>().find("Of great events not written there") != std::string::npos);
  // Kalé's second stage holds from his seventh conversation on.
  const auto howl = by_id("positive-kale-voice-t31");
  REQUIRE(howl != items->end());
  CHECK(howl->state == "howl_told");
  auto packs = x1_jobpacks(set, *items);
  REQUIRE_MESSAGE(packs.has_value(), (packs.has_value() ? std::string() : packs.error()));
  REQUIRE(packs->size() == 3);
  for (const X1Jobpack& p : *packs) {
    CHECK(p.json["format"] == "jobpack/1");
    CHECK(p.json["server"] == "*");
    CHECK(jobpack_text(p).size() <= JobpackCaps::kFileBytes);
    CHECK(p.json["description"].get<std::string>().size() <= JobpackCaps::kDescriptionChars);
    CHECK(p.json["defaults"]["max_tokens"] == 140);
    CHECK(p.json["defaults"]["thinking"] == false);
    CHECK(p.json["defaults"]["stop"].size() == 2);
  }
  CHECK((*packs)[2].name == "x1-questions");
  CHECK((*packs)[2].runs == 36);
}

TEST_CASE("x1: a server's and a model's names are written only within bounds") {
  CHECK(bounded_server("slot1") == std::optional<std::string>("slot1"));
  CHECK_FALSE(bounded_server("my server!").has_value());
  CHECK_FALSE(bounded_server(std::string(33, 'a')).has_value());
  CHECK(bounded_model("folder/sub/model-q4.gguf") == std::optional<std::string>("model-q4.gguf"));
  CHECK(bounded_model("C:\\models\\model-q4.gguf") == std::optional<std::string>("model-q4.gguf"));
  CHECK_FALSE(bounded_model("http://example.invalid:1234").has_value());
  CHECK_FALSE(bounded_model("203.0.113.5").has_value());
  CHECK_FALSE(bounded_model("name@host").has_value());
  CHECK(bounded_model("qwen3.5-4b-q4").has_value());
}

TEST_CASE("x1: Jobrunner's results read strictly, measured per server and model, with no text in the summary") {
  const X1Set set = the_set();
  auto items = build_x1_items(set);
  REQUIRE(items.has_value());
  const auto text = read_file((kX1 / "sample-results.jsonl").string(), kResultsFileMax);
  REQUIRE(text.has_value());
  X1Import import;
  read_jobrunner_results(*text, 1, *items, import);
  CHECK(import.lines.size() == 12);
  std::vector<std::size_t> refused;
  for (const X1Refusal& r : import.refused) refused.push_back(r.line);
  CHECK(refused == std::vector<std::size_t>{12, 13, 14, 15, 16, 18});
  for (const X1Refusal& r : import.refused) {
    CHECK(r.why.find("Jobrunner does not write") == std::string::npos);  // never the line's text
    CHECK(r.why.find("broken") == std::string::npos);
  }
  // The odd names became labels; the model's folder went.
  std::set<std::string> servers, models;
  for (const X1Line& l : import.lines) {
    servers.insert(l.server);
    models.insert(l.model);
  }
  CHECK(servers == std::set<std::string>{"slot1", "slot2", "server-3"});
  CHECK(models == std::set<std::string>{"sample-a-q4.gguf", "sample-b-q4.gguf", "model-3"});
  auto tap = builtin_speech_tap();
  REQUIRE(tap.has_value());
  const X1Outputs out = x1_measure(set, *items, import, **tap);
  const nlohmann::json& s = out.summary;
  CHECK(s["kind"] == "x1-summary");
  REQUIRE(s["models"].size() == 3);
  const nlohmann::json& a = s["models"][0];
  CHECK(a["server"] == "slot1");
  CHECK(a["runs"] == 9);
  CHECK(a["first_token_s"]["p50"] == 0.4);
  CHECK(a["tokens_per_s"]["p50"] == 50.0);
  CHECK(a["c10"]["met"] == false);
  CHECK(a["c10"]["complete"] == false);
  CHECK(a["c10"]["expected"] == 200);
  CHECK(a["c10"]["banned_caught"] == 1);
  const auto set_of = [&](const char* persona, const char* arm, const char* kind) {
    for (const nlohmann::json& x : a["sets"]) {
      if (x["persona"] == persona && x["arm"] == arm && x["kind"] == kind) return x;
    }
    FAIL("no set");
    return nlohmann::json();
  };
  const nlohmann::json kale = set_of("kale", "positive", "voice");
  CHECK(kale["runs"] == 4);
  CHECK(kale["within_words_max"] == 0.75);
  CHECK(kale["hard_hits"]["banned"] == 1);
  CHECK(kale["would_regenerate"] == 1);
  CHECK(kale["finish_length"] == 1);
  CHECK(kale["name_prefixes"]["name"] == 1);
  CHECK(set_of("kale", "former", "voice")["contract_caught"]["markup"] == 1);
  CHECK(set_of("x1_broken", "positive", "voice")["name_prefixes"]["long"] == 1);
  CHECK(set_of("x1_regal", "positive", "voice")["archaic_lines"] == 1.0);
  CHECK(a["archaic_regal"]["archaic_lines"] == 1.0);
  CHECK(a["test_questions"]["replies"] == 2);
  CHECK(s["models"][1]["sets"][0]["failures"] == 1);
  // No line of the conversation in the summary, nor in what is printed; the local files hold them.
  const std::string dumped = s.dump();
  for (const X1Line& l : import.lines) {
    if (l.reply.size() > 8) CHECK(dumped.find(l.reply.substr(0, 20)) == std::string::npos);
  }
  CHECK(dumped.find("example.invalid") == std::string::npos);
  CHECK(out.printed.find("come in") == std::string::npos);
  CHECK(out.report["kind"] == "x1-local");
  CHECK(out.report["results"].size() == 12);
  CHECK(out.questions["kind"] == "x1-questions");
  REQUIRE(out.questions["replies"].size() == 2);
  for (const nlohmann::json& q : out.questions["replies"]) {
    CHECK(q["persona"] == "kale");
    CHECK(q["messages"].is_array());
    CHECK(q["topics"] == nlohmann::json::array({"why the merchant caravans scattered"}));
    CHECK(q["leak"].is_null());
  }
}

TEST_CASE("x1: a results file past its bounds is refused whole, by number") {
  X1Import import;
  std::vector<X1Item> none;
  read_jobrunner_results(std::string(kResultsFileMax + 1, ' '), 2, none, import);
  REQUIRE(import.refused.size() == 1);
  CHECK(import.refused[0].file == 2);
  CHECK(import.refused[0].line == 0);
  X1Import lines;
  read_jobrunner_results("\n[1, 2]\n\"text\"\n", 1, none, lines);
  CHECK(lines.refused.size() == 2);
  CHECK(lines.lines.empty());
}
