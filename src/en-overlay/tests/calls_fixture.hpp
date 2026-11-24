#pragma once
// AI › Requests' and Home's Model calls card's invented facts for the fixture states (PAGES §4.6.1, §4.6.2;
// tests/pages/<state>/topics.json): the model calls' record as the DLL keeps it (en-proto's calls.hpp). No game text:
// every speaker is characters_fixture.hpp's, every line shown is ours, written for the tests, and every provider and
// model is the AI fixture's. A row's text is a shown reply's alone, as the record keeps it (PV4).
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "en/proto/calls.hpp"

namespace en::overlay::test {

namespace pr = en::proto;

inline pr::CallRow call(std::uint64_t id, std::uint64_t at_ms, std::string_view slot, pr::CallPurpose purpose,
                        std::string speaker, std::string provider, std::string model) {
  pr::CallRow r;
  r.id = id;
  r.at_ms = at_ms;
  r.slot = std::string(slot);
  r.purpose = purpose;
  r.speaker = std::move(speaker);
  r.provider = std::move(provider);
  r.model = std::move(model);
  r.attempts = 1;
  return r;
}

inline pr::CallRow ended(pr::CallRow r, pr::CallOutcome outcome, std::int64_t first_ms, std::int64_t total_ms,
                         std::int32_t in, std::int32_t out, std::string text = {}) {
  r.outcome = outcome;
  r.first_token_ms = first_ms;
  r.total_ms = total_ms;
  r.tokens_in = in;
  r.tokens_out = out;
  r.text = std::move(text);
  return r;
}

// While starting and at L1 the record with no row yet (the DLL hands it over from the start); at the title the
// self-test's run (its model list and its two
// requests) and AI › Providers' List models; in the world the replies of a conversation before AI was paused, shown, by
// the fallback, not shown, timed out, failed on the provider's side and one still waiting, and a profile draft, some of
// them with a cost reported. Newest first, as the record keeps them.
inline const pr::CallsView* calls_fixture(std::string_view state) {
  static const pr::CallsView title = [] {
    pr::CallsView v;
    v.revision = 5;
    const auto list = [](std::uint64_t id, std::uint64_t at, pr::CallPurpose why, std::string provider,
                         std::int64_t total) {
      return ended(call(id, at, "", why, "", std::move(provider), ""), pr::CallOutcome::Answered, -1, total, -1, -1);
    };
    v.rows = {list(5, 705'000, pr::CallPurpose::ModelList, "lantern-box", 140),
              list(4, 705'000, pr::CallPurpose::ModelList, "openrouter", 610),
              ended(call(3, 690'900, pr::kSelftest, pr::CallPurpose::SelfTest, "", "openrouter", "talker-small"),
                    pr::CallOutcome::Answered, 380, 1'020, 96, 24),
              ended(call(2, 690'300, pr::kSelftest, pr::CallPurpose::SelfTest, "", "openrouter", "talker-small"),
                    pr::CallOutcome::Answered, 420, 590, 88, 18),
              list(1, 690'000, pr::CallPurpose::SelfTest, "openrouter", 300)};
    v.usage.calls = 5;
    v.usage.tokens_in = 184;
    v.usage.tokens_out = 42;
    v.usage.requests_hour = 6;
    v.usage.bound = 600;
    return v;
  }();
  static const pr::CallsView world = [] {
    pr::CallsView v;
    v.revision = 41;
    using O = pr::CallOutcome;
    const auto reply = [](std::uint64_t id, std::uint64_t at, std::string who, std::string model) {
      return call(id, at, pr::kDialogue, pr::CallPurpose::Reply, std::move(who), "openrouter", std::move(model));
    };
    pr::CallRow waiting = reply(19, 3'905'000, "Ysolde", "talker-small");
    pr::CallRow shown = ended(reply(18, 3'655'000, "Old Bram", "talker-small"), O::Shown, 640, 1'720, 812, 46,
                              "Two coins for the crossing, friend, and the river decides how dry you land.");
    shown.cost = 0.00031;
    pr::CallRow fallback = ended(reply(17, 3'642'000, "Old Bram", "talker-large"), O::FallbackModel, 2'950, 4'100, 790,
                                 52, "Ask me again when the lamps are lit. I count better by their light.");
    fallback.attempts = 2;
    fallback.fallback = true;
    fallback.error = pr::ErrorClass::FirstTokenTimeout;
    fallback.cost = 0.00118;
    pr::CallRow withheld = ended(reply(16, 3'610'000, "Ysolde", "talker-small"), O::NotShown, 700, 1'300, 760, 40);
    pr::CallRow slow = ended(reply(15, 3'590'000, "Ysolde", "talker-small"), O::TimedOut, -1, 6'000, -1, -1);
    slow.attempts = 2;
    slow.error = pr::ErrorClass::FirstTokenTimeout;
    pr::CallRow draft = ended(call(14, 3'560'000, pr::kDraft, pr::CallPurpose::ProfileDraft, "", "openrouter",
                                   "talker-small"),
                              O::Answered, 900, 3'400, 420, 180);
    draft.cost = 0.00052;
    pr::CallRow failed = ended(reply(13, 3'500'000, "Old Bram", "talker-small"), O::Failed, -1, 2'200, -1, -1);
    failed.attempts = 3;
    failed.error = pr::ErrorClass::Server;
    failed.http_status = 503;
    pr::CallRow first = ended(reply(12, 3'480'000, "Old Bram", "talker-small"), O::Shown, 610, 1'540, 702, 38,
                              "The river is low today. Mind the third plank; it remembers every boot.");
    first.cost = 0.00028;
    v.rows = {waiting, shown, fallback, withheld, slow, draft, failed, first};
    v.usage.calls = 31;
    v.usage.tokens_in = 21'480;
    v.usage.tokens_out = 1'322;
    v.usage.failures = 3;
    v.usage.requests_hour = 24;
    v.usage.bound = 600;
    v.usage.cost = 0.0187;
    v.usage.cost_calls = 19;
    return v;
  }();
  static const pr::CallsView none;
  if (state == "title") return &title;
  if (state == "world" || state == "multiplayer") return &world;
  return &none;
}

}  // namespace en::overlay::test
