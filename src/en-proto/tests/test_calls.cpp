// The model calls' record's plain data and words (calls.hpp): PAGES' words for every purpose and outcome, ours for
// every error class, a model id a row may show (PV5), and the cost a provider's usage reports in its JSON form (D18).
// All data is invented.
#include <doctest.h>

#include <set>
#include <string>

#include "en/proto/calls.hpp"
#include "en/proto/json.hpp"

using namespace en::proto;
using nlohmann::json;

TEST_CASE("every purpose, outcome and error class has its words, and the pause its own") {
  std::set<std::string> seen;
  for (std::size_t i = 0; i < kCallPurposes; ++i) seen.insert(words(static_cast<CallPurpose>(i)));
  CHECK(seen.size() == kCallPurposes);
  CHECK(std::string(words(CallPurpose::OwnInitiative)) == "own-initiative line");
  CHECK(std::string(words(CallPurpose::PreparedBatch)) == "prepared-line batch");
  seen.clear();
  for (std::size_t i = 0; i < kCallOutcomes; ++i) seen.insert(words(static_cast<CallOutcome>(i)));
  CHECK(seen.size() == kCallOutcomes);
  CHECK(std::string(words(CallOutcome::FallbackModel)) == "fallback model");
  CHECK(std::string(words(CallOutcome::NotShown)) == "not shown");
  seen.clear();
  for (int e = 1; e <= static_cast<int>(ErrorClass::Dropped); ++e) {
    seen.insert(error_words(static_cast<ErrorClass>(e), 200));
  }
  CHECK(seen.size() == static_cast<std::size_t>(ErrorClass::Dropped));  // one sentence each
  CHECK(std::string(error_words(ErrorClass::Server, 0)) == "Could not be reached.");
  for (const std::string_view w :
       {kAiPausedStatus, kAiPausedSelfTest, kAiPausedModelList, kAiPausedDraft, kAiPausedCall}) {
    CHECK(w.find("paused for this session") != std::string_view::npos);
    CHECK(w.back() == '.');
  }
  CHECK(kAiPausedReason == "AI paused");
}

TEST_CASE("a model id is shown unless it looks like a path (PV5)") {
  for (const char* id : {"invented-model-a", "org/invented-model:free", "invented:7b-q4"}) {
    CHECK_MESSAGE(shown_model(id), id);
  }
  for (const char* id : {"", "/models/x.gguf", "~/x.gguf", "C:\\x.gguf", "d:x", "a\\b", "x/Users/y", "x/home/y",
                         "tab\there"}) {
    CHECK_FALSE_MESSAGE(shown_model(id), id);
  }
  CHECK_FALSE(shown_model(std::string(201, 'a')));
}

TEST_CASE("a usage's cost travels in its JSON form, and an older form without it reads none") {
  const Usage u{812, 9, 640, 0.00042};
  const json j = u;
  CHECK(j.at("cost").get<double>() == doctest::Approx(0.00042));
  CHECK(j.get<Usage>() == u);
  const Usage old = json{{"prompt_tokens", 5}, {"completion_tokens", 2}, {"cached_tokens", -1}}.get<Usage>();
  CHECK(old.cost == -1);
  CHECK(Usage{}.cost == -1);
}

TEST_CASE("a row's tokens: in and out where reported, -1 when neither was") {
  CallRow r;
  CHECK(r.tokens() == -1);
  r.tokens_out = 12;
  CHECK(r.tokens() == 12);
  r.tokens_in = 800;
  CHECK(r.tokens() == 812);
  CHECK(CallsView{}.kept == kCallsKept);
  CHECK(kCallsKeptDeveloper == 200);
}
