// The eval's speech tap (speech_tap.hpp; NPC §8.2 to §8.6, §12.2; WHY§410): the "Name:" prefixes by class, an
// attempt's context, the counts of a regeneration, the pager's cap and the archaic share, and one reply's speech on
// its own. All data is invented.
#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "eval_run.hpp"
#include "golden_run.hpp"
#include "speech_tap.hpp"

using namespace en;
using namespace en::tools;

namespace {

const std::filesystem::path kTalk = std::filesystem::path(EN_TOOLS_SCENARIOS) / "talk";

EvalRun run_of(const char* name, const EvalOptions& options = {}) {
  const auto path = kTalk / name;
  const auto text = read_file(path.string(), 256 * 1024);
  REQUIRE(text.has_value());
  auto scenario = parse_eval_scenario(*text, path.parent_path());
  REQUIRE_MESSAGE(scenario.has_value(), (scenario.has_value() ? std::string() : scenario.error()));
  auto run = run_eval(*scenario, EvalLlm::Mock, options);
  REQUIRE_MESSAGE(run.has_value(), (run.has_value() ? std::string() : run.error()));
  return std::move(*run);
}

const SpeechCounts& first_counts(const EvalRun& run, std::size_t turn) {
  const auto a = run.turns.at(turn).first_try(run.attempts);
  REQUIRE(a.has_value());
  return run.attempts[*a].counts;
}

}  // namespace

TEST_CASE("the speech tap: each attempt's \"Name:\" prefix by class, and its context (WHY 410)") {
  const EvalRun run = run_of("lantern-name-prefix.toml");
  CHECK(run.failed.empty());
  REQUIRE(run.turns.size() == 3);
  const SpeechCounts& a = first_counts(run, 0);
  const SpeechCounts& b = first_counts(run, 1);
  const SpeechCounts& c = first_counts(run, 2);
  CHECK(a.name_class == std::optional<NameClass>(NameClass::Name));
  CHECK(a.name_prefix == "Lantern-Keeper");
  CHECK(b.name_class == std::optional<NameClass>(NameClass::Short));
  CHECK(b.name_prefix == "Old Keeper");
  CHECK(c.name_class == std::optional<NameClass>(NameClass::Long));
  CHECK(c.name_prefix == "But know this");
  for (const SpeechCounts* s : {&a, &b, &c}) CHECK(s->name_prefixes() == 1);
  // The context the conversation opened the reply with: the persona's names, its word cap, its opener.
  const ReplyContext& ctx = run.attempts[*run.turns[0].first_try(run.attempts)].context;
  CHECK(ctx.words_max == 30);
  CHECK(std::find(ctx.names.begin(), ctx.names.end(), "Lantern-Keeper") != ctx.names.end());
  CHECK(ctx.openers == std::vector<std::string>{"Hm."});
  CHECK_FALSE(ctx.regeneration);
  // The report counts them by class; the summary line has the number alone.
  const nlohmann::json r = eval_report(run);
  CHECK(r["schema"] == 2);
  CHECK(r["metrics"]["name_prefixes"] == nlohmann::json({{"name", 1}, {"short", 1}, {"long", 1}}));
  CHECK(eval_summary(run).find("3 name prefixes") != std::string::npos);
  CHECK(eval_summary(run).find("Keeper") == std::string::npos);
}

TEST_CASE("the speech tap: a regeneration's attempts and the pager's cap") {
  const EvalRun regen = run_of("lantern-regenerate.toml");
  REQUIRE_FALSE(regen.turns.empty());
  const SpeechCounts& first = first_counts(regen, 0);
  CHECK(first.hard[0] + first.hard[1] + first.hard[2] >= 1);  // the hit that regenerated it
  CHECK(regen.turns[0].regeneration(regen.attempts).has_value());
  const EvalRun long_reply = run_of("lantern-long-reply.toml");
  CHECK(first_counts(long_reply, 0).full + first_counts(long_reply, 0).over >= 1);
}

TEST_CASE("run_speech: one reply on its own, with a turn's context, as the conversation would take it") {
  const EvalRun run = run_of("lantern-name-prefix.toml");
  const ReplyContext ctx = run.attempts[*run.turns[0].first_try(run.attempts)].context;
  auto tap = builtin_speech_tap();
  REQUIRE(tap.has_value());
  SUBCASE("a name prefix and markup, removed and counted") {
    const SpeechPass p = run_speech(**tap, ctx, "Lantern-Keeper: *nods* Hm. The lamp is lit.");
    CHECK(p.counts.name_class == std::optional<NameClass>(NameClass::Name));
    CHECK(p.counts.removed[static_cast<std::size_t>(core::Removed::Markup)] >= 1);
    CHECK_FALSE(p.stopped.has_value());
    REQUIRE_FALSE(p.shown.empty());
    for (const std::string& s : p.shown) CHECK(s.find('*') == std::string::npos);
  }
  SUBCASE("a banned word in the first sentence: the reply would regenerate, and nothing of it shows") {
    const SpeechPass p = run_speech(**tap, ctx, "Okay, traveller. The lamp is lit.");
    CHECK(p.stopped == std::optional<core::LintAction>(core::LintAction::Regenerate));
    CHECK(p.hit == std::optional<core::HardList>(core::HardList::Banned));
    CHECK(p.shown.empty());
    CHECK(p.counts.sentences == 2);  // every sentence is counted, after the hit too
  }
  SUBCASE("past the word cap: the model's words, and the pager's cut") {
    const std::string longer =
        "The lamp burns all night and the oil runs low by dawn, most days of the year. I trim the wick at dusk and "
        "again near midnight. Then I wait for the light, and I count the travellers who pass by the crossroads.";
    const SpeechPass p = run_speech(**tap, ctx, longer);
    CHECK(p.model_words > ctx.words_max);
    CHECK(p.cut_by_pager);
    CHECK(p.model_sentences == 3);
  }
  SUBCASE("a trailer is counted by the flow") {
    const SpeechPass p = run_speech(**tap, ctx, "Hm. The road is quiet.\nACT: trims the wick");
    CHECK(p.counts.trailers == 1);
  }
}
