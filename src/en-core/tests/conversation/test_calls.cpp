// The model calls' record (calls.hpp; PAGES §4.6.2's AI › Requests, §4.6.1's Model calls card) and Pause AI (D15;
// core.hpp's CoreInput::Pause): the ring's bounds and order, each purpose and outcome in PAGES' words, a reply's
// regeneration and fallback as one row, the usage, PV4 (no unshown text, prompt, key or address in any row), and the
// pause's L2, its refusal of every request, and the ladder's return. All data is invented.
#include <doctest.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "en/core/calls.hpp"
#include "en/core/core.hpp"
#include "en/core/stubs.hpp"
#include "../snapshots.hpp"
#include "../test_files.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

namespace {

using proto::CallOutcome;
using proto::CallPurpose;
using proto::ErrorClass;

proto::LlmEvent started(proto::RequestId id, std::string provider, std::string model) {
  proto::LlmEvent e;
  e.kind = proto::EventKind::Started;
  e.id = id;
  e.provider = std::move(provider);
  e.model = std::move(model);
  return e;
}

proto::AttemptInfo attempt(std::string provider, std::string model, ErrorClass error = ErrorClass::None,
                           int status = 0) {
  proto::AttemptInfo a;
  a.provider = std::move(provider);
  a.model = std::move(model);
  a.error = error;
  a.http_status = status;
  return a;
}

// A request's end: the answer from `provider`/`model` after the attempts given (one ok attempt when none is).
proto::LlmEvent ended(proto::RequestId id, ErrorClass error = ErrorClass::None,
                      std::vector<proto::AttemptInfo> tries = {}, int in = -1, int out = -1, double cost = -1) {
  proto::ChatResult r;
  r.error = error;
  r.provider = tries.empty() ? "invented-cloud" : tries.back().provider;
  r.model = tries.empty() ? "invented-model-a" : tries.back().model;
  r.attempts = tries.empty() ? std::vector<proto::AttemptInfo>{attempt(r.provider, r.model, error)} : std::move(tries);
  r.usage.prompt_tokens = in;
  r.usage.completion_tokens = out;
  r.usage.cost = cost;
  r.first_token = std::chrono::milliseconds(error == ErrorClass::None ? 400 : -1);
  r.total = std::chrono::milliseconds(900);
  r.text = "An answer nobody saw.";
  r.message = "the server said: key sk-invented-0000 for the base invented-host-base";
  proto::LlmEvent e;
  e.kind = proto::EventKind::Ended;
  e.id = id;
  e.chat = std::move(r);
  return e;
}

proto::LlmEvent listed(std::uint64_t id, std::string provider, ErrorClass error = ErrorClass::None, int status = 200) {
  proto::LlmEvent e;
  e.kind = proto::EventKind::ModelsEnded;
  e.id = id;
  e.provider = std::move(provider);
  e.models.error = error;
  e.models.status = status;
  e.models.models = {"invented-model-a", "C:\\Users\\someone\\models\\invented.gguf"};
  e.models.time = std::chrono::milliseconds(120);
  return e;
}

CallNote sent_note(proto::RequestId id, proto::ConvId conv, proto::ReplyId reply) {
  return CallNote{CallNote::What::Sent, id, conv, reply, "dialogue", "Lantern-Keeper"};
}

const proto::CallRow& only(const CallRecord& calls) {
  REQUIRE(calls.view().rows.size() == 1);
  return calls.view().rows.front();
}

// Every text a row holds, for PV4's checks.
std::string texts_of(const proto::CallsView& v) {
  std::string all;
  for (const proto::CallRow& r : v.rows) {
    all += r.slot + "|" + r.speaker + "|" + r.provider + "|" + r.model + "|" + r.text + "\n";
  }
  return all;
}

}  // namespace

TEST_CASE("the record keeps the newest 20, 200 in developer mode, newest first, and counts every call") {
  CallRecord calls;
  for (proto::RequestId id = 1; id <= 25; ++id) {
    calls.sent(id, CallStart{"draft", CallPurpose::ProfileDraft, ""}, 1000 * id);
  }
  const proto::CallsView& v = calls.view();
  CHECK(v.kept == proto::kCallsKept);
  REQUIRE(v.rows.size() == 20);
  CHECK(v.rows.front().at_ms == 25'000);
  CHECK(v.rows.back().at_ms == 6'000);
  for (std::size_t i = 1; i < v.rows.size(); ++i) {
    CHECK(v.rows[i - 1].at_ms > v.rows[i].at_ms);
    CHECK(v.rows[i - 1].id > v.rows[i].id);
  }
  CHECK(v.usage.calls == 25);
  calls.set_kept(proto::kCallsKeptDeveloper);
  CHECK(calls.view().kept == 200);
  CHECK(calls.view().rows.size() == 25);  // the record held them: none was lost to the smaller view
  for (proto::RequestId id = 26; id <= 300; ++id) {
    calls.sent(id, CallStart{"draft", CallPurpose::ProfileDraft, ""}, 1000 * id);
  }
  CHECK(calls.view().rows.size() == 200);
  CHECK(calls.view().rows.front().at_ms == 300'000);
  CHECK(calls.view().usage.calls == 300);
  calls.set_kept(proto::kCallsKept);
  CHECK(calls.view().rows.size() == 20);
  CHECK(calls.view().rows.back().at_ms == 281'000);
  // A row made at its end (a self-test's, its request never seen sent) goes where it was sent, among the others.
  const CallStart test{"selftest", CallPurpose::SelfTest, ""};
  calls.event(ended(9001), 295'500, &test);  // sent 900 ms before
  const auto& rows = calls.view().rows;
  REQUIRE(rows.size() == 20);
  CHECK(rows[5].at_ms == 295'000);
  CHECK(rows[6].purpose == CallPurpose::SelfTest);
  CHECK(rows[6].at_ms == 294'600);
  CHECK(rows[7].at_ms == 294'000);
}

TEST_CASE("each purpose and outcome has PAGES' words, and each its own name") {
  std::set<std::string> words;
  std::set<std::string> names;
  for (std::size_t i = 0; i < proto::kCallPurposes; ++i) {
    words.insert(proto::words(static_cast<CallPurpose>(i)));
    names.insert(proto::to_string(static_cast<CallPurpose>(i)));
  }
  CHECK(words == std::set<std::string>{"reply", "own-initiative line", "call-out reply", "prepared-line batch",
                                       "memory work", "self-test", "model list", "profile draft"});
  CHECK(names.size() == proto::kCallPurposes);
  words.clear();
  names.clear();
  for (std::size_t i = 0; i < proto::kCallOutcomes; ++i) {
    words.insert(proto::words(static_cast<CallOutcome>(i)));
    names.insert(proto::to_string(static_cast<CallOutcome>(i)));
  }
  CHECK(words == std::set<std::string>{"waiting", "shown", "not shown", "fallback model", "timed out", "refused",
                                       "failed", "pool filled", "answered"});
  CHECK(names.size() == proto::kCallOutcomes);
  // Every error class but None in words of a sentence; a server that answered is told from one that was not reached.
  for (int e = 1; e <= static_cast<int>(ErrorClass::Dropped); ++e) {
    const std::string w = proto::error_words(static_cast<ErrorClass>(e), 0);
    CHECK(w.size() > 10);
    CHECK(w.back() == '.');
  }
  CHECK(std::string(proto::error_words(ErrorClass::None, 0)).empty());
  CHECK(std::string(proto::error_words(ErrorClass::Server, 0)) != proto::error_words(ErrorClass::Server, 503));
}

TEST_CASE("a reply: waiting, then shown with its displayed text alone, once a page reached the screen") {
  CallRecord calls;
  calls.note(sent_note(7, 1, 1), 5000);
  CHECK(only(calls).outcome == CallOutcome::Waiting);
  CHECK(only(calls).speaker == "Lantern-Keeper");
  CHECK(only(calls).slot == "dialogue");
  CHECK(only(calls).purpose == CallPurpose::Reply);
  CHECK(only(calls).total_ms == -1);
  calls.event(started(7, "invented-cloud", "invented-model-a"), 5050);
  CHECK(only(calls).provider == "invented-cloud");
  calls.shown(1, 1, "The lamp burns low.");  // the first page shows while the answer streams
  CHECK(only(calls).outcome == CallOutcome::Shown);
  calls.event(ended(7, ErrorClass::None, {}, 812, 9, 0.0004), 5950);
  calls.shown(1, 1, "It still burns.");
  const proto::CallRow& r = only(calls);
  CHECK(r.outcome == CallOutcome::Shown);
  CHECK(r.text == "The lamp burns low. It still burns.");
  CHECK(r.first_token_ms == 450);  // the client began 50 ms after it was sent (its end less its 900), and 400 after
  CHECK(r.total_ms == 950);
  CHECK(r.tokens_in == 812);
  CHECK(r.tokens_out == 9);
  CHECK(r.tokens() == 821);
  CHECK(r.attempts == 1);
  CHECK(r.model == "invented-model-a");
  CHECK(r.error == ErrorClass::None);
  const proto::CallUsage& u = calls.view().usage;
  CHECK(u.calls == 1);
  CHECK(u.tokens_in == 812);
  CHECK(u.tokens_out == 9);
  CHECK(u.failures == 0);
  CHECK(u.cost_calls == 1);
  CHECK(u.cost == doctest::Approx(0.0004));
}

TEST_CASE("a reply answered and never shown reads not shown, and turns shown when a page comes after its end") {
  CallRecord calls;
  calls.note(sent_note(7, 1, 1), 0);
  calls.event(ended(7), 900);
  CHECK(only(calls).outcome == CallOutcome::NotShown);
  CHECK(only(calls).text.empty());
  calls.shown(1, 1, "Late, but shown.");  // pacing: the page shows after the stream ended
  CHECK(only(calls).outcome == CallOutcome::Shown);
  // Stopped before anything showed (its conversation closed): not shown, the class kept for the drawer.
  calls.note(sent_note(8, 1, 2), 2000);
  calls.event(ended(8, ErrorClass::Cancelled), 2100);
  CHECK(calls.view().rows.front().outcome == CallOutcome::NotShown);
  CHECK(calls.view().rows.front().error == ErrorClass::Cancelled);
  CHECK(calls.view().usage.failures == 0);
}

TEST_CASE("each failure in PAGES' words, counted while it stands") {
  CallRecord calls;
  const auto end_as = [&](proto::RequestId id, ErrorClass e, int status = 0) {
    calls.note(sent_note(id, 1, id), id * 1000);
    calls.event(ended(id, e, {attempt("invented-cloud", "invented-model-a", e, status)}), id * 1000 + 900);
    return calls.view().rows.front();
  };
  CHECK(end_as(1, ErrorClass::FirstTokenTimeout).outcome == CallOutcome::TimedOut);
  CHECK(end_as(2, ErrorClass::TotalTimeout).outcome == CallOutcome::TimedOut);
  CHECK(end_as(3, ErrorClass::Refused).outcome == CallOutcome::Refused);
  const proto::CallRow auth = end_as(4, ErrorClass::Auth, 401);
  CHECK(auth.outcome == CallOutcome::Failed);
  CHECK(auth.http_status == 401);
  CHECK(std::string(proto::error_words(auth.error, auth.http_status)) == "Refused the key, or was sent none.");
  CHECK(end_as(5, ErrorClass::Server, 503).outcome == CallOutcome::Failed);
  CHECK(end_as(6, ErrorClass::GateClosed).outcome == CallOutcome::Failed);
  CHECK(end_as(7, ErrorClass::Preempted).outcome == CallOutcome::NotShown);
  CHECK(calls.view().usage.failures == 6);
  // A reply that broke after its text came keeps what came: shown once its page is, and no longer a failure.
  CHECK(end_as(8, ErrorClass::TotalTimeout).outcome == CallOutcome::TimedOut);
  CHECK(calls.view().usage.failures == 7);
  calls.shown(1, 8, "Half a thought.");
  CHECK(calls.view().rows.front().outcome == CallOutcome::Shown);
  CHECK(calls.view().rows.front().error == ErrorClass::TotalTimeout);
  CHECK(calls.view().usage.failures == 6);
  // A request the model client did not take.
  calls.note(sent_note(9, 1, 9), 9000);
  calls.not_sent(9, ErrorClass::NoHealthyProvider, 9001);
  CHECK(calls.view().rows.front().outcome == CallOutcome::Failed);
  CHECK(calls.view().rows.front().attempts == 0);
  CHECK(std::string(proto::error_words(ErrorClass::NoHealthyProvider, 0)).rfind("Not sent:", 0) == 0);
  CHECK(calls.view().usage.failures == 7);
  CHECK(calls.view().usage.calls == 9);
}

TEST_CASE("a fallback model's answer, shown: fallback model, with why the first did not answer") {
  CallRecord calls;
  calls.note(sent_note(7, 1, 1), 0);
  calls.event(ended(7, ErrorClass::None,
                    {attempt("invented-cloud", "invented-model-a", ErrorClass::FirstTokenTimeout),
                     attempt("invented-cloud", "invented-model-b")}),
              3000);
  CHECK(only(calls).outcome == CallOutcome::NotShown);  // until a page shows
  calls.shown(1, 1, "From the second model.");
  const proto::CallRow& r = only(calls);
  CHECK(r.outcome == CallOutcome::FallbackModel);
  CHECK(r.fallback);
  CHECK(r.model == "invented-model-b");
  CHECK(r.attempts == 2);
  CHECK(r.error == ErrorClass::FirstTokenTimeout);
  CHECK(calls.view().usage.failures == 0);
}

TEST_CASE("a regeneration joins its reply's row: the attempts and tokens summed, the second answer counting") {
  CallRecord calls;
  calls.note(sent_note(7, 1, 1), 1000);
  calls.note(sent_note(8, 1, 1), 1400);                              // the lint's regeneration, same reply
  calls.event(ended(7, ErrorClass::Cancelled, {}, 300, 12), 1450);    // the first, cancelled for it
  CHECK(only(calls).outcome == CallOutcome::Waiting);
  calls.event(ended(8, ErrorClass::None, {}, 340, 20), 2400);
  calls.shown(1, 1, "A better answer.");
  const proto::CallRow& r = only(calls);
  CHECK(r.outcome == CallOutcome::Shown);
  CHECK(r.attempts == 2);
  CHECK(r.tokens_in == 640);
  CHECK(r.tokens_out == 32);
  CHECK(r.total_ms == 1400);
  CHECK(r.first_token_ms == 900);  // the second request's: sent at 1500 (its end less its 900), its first after 400
  CHECK(calls.view().usage.calls == 1);
  CHECK(calls.view().usage.tokens_in == 640);
}

TEST_CASE("an answer a check withheld reads not shown, and the persona's own line in its place is no text of it") {
  CallRecord calls;
  calls.note(sent_note(7, 1, 1), 0);
  calls.note(CallNote{CallNote::What::Withheld, 7, 1, 1, {}, {}}, 300);  // the lint's deflection
  calls.event(ended(7, ErrorClass::Cancelled), 320);
  calls.shown(1, 1, "The persona's own deflection.");
  CHECK(only(calls).outcome == CallOutcome::NotShown);
  CHECK(only(calls).text.empty());
}

TEST_CASE("the self-test, the model lists and a profile draft: answered, each by its purpose") {
  CallRecord calls;
  const CallStart latency{"selftest", CallPurpose::SelfTest, ""};
  const CallStart list_of_test{"", CallPurpose::SelfTest, ""};
  const CallStart list{"", CallPurpose::ModelList, ""};
  calls.event(listed(1, "invented-cloud"), 1120, &list_of_test);
  calls.event(started(11, "invented-cloud", "invented-model-a"), 1200, &latency);
  calls.event(ended(11), 2100, &latency);
  calls.event(listed(2, "invented-local", ErrorClass::GateClosed, 0), 3000, &list);
  calls.event(listed(3, "invented-cloud", ErrorClass::Auth, 401), 4000, &list);
  calls.sent(1ull << 44, CallStart{"draft", CallPurpose::ProfileDraft, ""}, 5000);
  calls.event(ended(1ull << 44), 6000);
  const auto& rows = calls.view().rows;
  REQUIRE(rows.size() == 5);
  CHECK(rows[0].purpose == CallPurpose::ProfileDraft);
  CHECK(rows[0].outcome == CallOutcome::Answered);
  CHECK(rows[0].text.empty());
  CHECK(rows[1].purpose == CallPurpose::ModelList);
  CHECK(rows[1].outcome == CallOutcome::Failed);
  CHECK(rows[1].http_status == 401);
  CHECK(rows[1].attempts == 1);
  CHECK(rows[2].outcome == CallOutcome::Failed);
  CHECK(rows[2].error == ErrorClass::GateClosed);
  CHECK(rows[2].attempts == 0);  // never left
  CHECK(rows[3].purpose == CallPurpose::SelfTest);
  CHECK(rows[3].slot == "selftest");
  CHECK(rows[3].at_ms == 1200);
  CHECK(rows[3].outcome == CallOutcome::Answered);
  CHECK(rows[4].purpose == CallPurpose::SelfTest);
  CHECK(rows[4].slot.empty());
  CHECK(rows[4].at_ms == 1000);
  CHECK(rows[4].total_ms == 120);
  CHECK(rows[4].model.empty());
  CHECK(calls.view().usage.calls == 5);
  CHECK(calls.view().usage.failures == 2);
  // An event of a call the record was not told of, with nothing to say what it is: left alone.
  calls.event(ended(99), 7000);
  calls.event(listed(4, "invented-cloud"), 7000);
  CHECK(calls.view().rows.size() == 5);
}

TEST_CASE("PV4 and PV5: no unshown answer, prompt, key, address or path in any row") {
  CallRecord calls;
  calls.note(sent_note(7, 1, 1), 0);
  calls.event(ended(7, ErrorClass::Server, {attempt("invented-cloud", "invented-model-a", ErrorClass::Server, 502)}),
              900);
  calls.note(sent_note(8, 1, 2), 1000);
  calls.event(started(8, "invented-local", "/home/someone/models/invented.gguf"), 1010);
  CHECK(calls.view().rows.front().model.empty());
  calls.event(ended(8, ErrorClass::None, {attempt("invented-local", "C:\\Users\\someone\\invented.gguf")}), 1900);
  const std::string all = texts_of(calls.view());
  for (const char* never : {"nobody saw", "sk-invented", "invented-host", "http", "Users", "home", "gguf"}) {
    CHECK_MESSAGE(all.find(never) == std::string::npos, never);
  }
  CHECK(proto::shown_model("invented-model-a"));
  CHECK(proto::shown_model("org/invented-model:free"));
  for (const char* path : {"/models/x.gguf", "~/x.gguf", "C:\\x.gguf", "c:x", "a/home/b", "x\\y", ""}) {
    CHECK_FALSE_MESSAGE(proto::shown_model(path), path);
  }
}

TEST_CASE("a page's text is cut within its bound, at a character's start") {
  CallRecord calls;
  calls.note(sent_note(7, 1, 1), 0);
  calls.shown(1, 1, std::string(1500, 'a'));
  // The second page, after its space, has 547 bytes' room: its two-byte character at bytes 546 and 547 does not fit.
  calls.shown(1, 1, std::string(546, 'b') + "\xC3\xA9" + "cc");
  const std::string& text = only(calls).text;
  CHECK(text.size() == proto::kCallTextMaxBytes - 1);
  CHECK(text.back() == 'b');
  calls.shown(1, 1, "More.");  // full: nothing more is added
  CHECK(only(calls).text.size() == proto::kCallTextMaxBytes - 1);
}

TEST_CASE("the usage: tokens and costs where reported, own-initiative calls in the last hour, the bound") {
  CallRecord calls;
  calls.sent(1, CallStart{"bark_live", CallPurpose::OwnInitiative, "Soldier"}, 10'000);
  calls.sent(2, CallStart{"bark_live", CallPurpose::OwnInitiative, "Soldier"}, 2'000'000);
  calls.sent(3, CallStart{"draft", CallPurpose::ProfileDraft, ""}, 2'100'000);
  calls.event(ended(1, ErrorClass::None, {}, 100, -1), 11'000);
  calls.event(ended(2, ErrorClass::None, {}, -1, -1, 0.002), 2'001'000);
  calls.event(ended(3, ErrorClass::None, {}, 50, 25, 0.001), 2'101'000);
  calls.hour(37, 600, 2'200'000);
  proto::CallUsage u = calls.view().usage;
  CHECK(u.calls == 3);
  CHECK(u.tokens_in == 150);
  CHECK(u.tokens_out == 25);
  CHECK(u.cost_calls == 2);
  CHECK(u.cost == doctest::Approx(0.003));
  CHECK(u.own_initiative_hour == 2);
  CHECK(u.requests_hour == 37);
  CHECK(u.bound == 600);
  CHECK(calls.view().rows.back().tokens() == 100);
  CHECK(calls.view().rows[1].tokens() == -1);  // none reported
  const std::uint64_t rev = calls.view().revision;
  calls.hour(37, 600, 2'200'500);
  CHECK(calls.view().revision == rev);  // nothing changed
  calls.hour(37, 600, 3'610'001);       // an hour after the first: it no longer counts
  CHECK(calls.view().usage.own_initiative_hour == 1);
  calls.hour(5, 600, 5'600'001);
  CHECK(calls.view().usage.own_initiative_hour == 0);
  CHECK(calls.view().usage.calls == 3);  // the session's, whatever the hour
}

TEST_CASE("a request refused before it left (Pause AI) is no call: its row and what it added go") {
  CallRecord calls;
  calls.sent(1, CallStart{"bark_live", CallPurpose::OwnInitiative, "Soldier"}, 1000);
  calls.withdraw(1);
  CHECK(calls.view().rows.empty());
  CHECK(calls.view().usage.calls == 0);
  CHECK(calls.view().usage.own_initiative_hour == 0);
  // A regeneration's: the reply's row stays, its earlier request stopped for it.
  calls.note(sent_note(7, 1, 1), 2000);
  calls.note(sent_note(8, 1, 1), 2400);
  calls.withdraw(8);
  CHECK(only(calls).outcome == CallOutcome::NotShown);
  CHECK(calls.view().usage.calls == 1);
}

// ---- Through the core: the conversation's notes and commits, and Pause AI ----

namespace {

proto::WorldSnapshot with_keeper(proto::WorldSnapshot s) {
  s.player->handle = proto::ChrHandle{1};
  s.player->hp = proto::Meter{400, 400};
  s.ui.hud = proto::HudState::Default;
  s.ui.subtitles_on = true;
  proto::Chr c;
  c.handle = proto::ChrHandle{0x500};
  c.param = 81100;
  c.dist_m = 2.0f;
  c.bearing_deg = 3.0f;
  c.hp = proto::Meter{100, 100};
  (void)s.chars.try_push_back(c);
  return s;
}

// A session on the stubs' speech, every step's notes and commits into a record, as the DLL's T6 does.
struct Session {
  stubs::StubSpeech speech;
  Core core{speech.parts()};
  CallRecord calls;
  Mono t = 0;
  std::vector<proto::LlmRequest> requests;
  Outputs last;

  Outputs feed(CoreInput in) {
    Outputs o = core.step(std::move(in), t);
    record_step(calls, o, t);
    for (auto& r : o.requests) requests.push_back(r);
    last = o;
    return o;
  }
  void world(int snapshots) {
    for (int i = 0; i < snapshots; ++i) {
      t += 250;
      const Outputs o = feed(in(with_keeper(test::world(t))));
      if (o.ui.page && o.ui.page->kind == proto::PageKind::Speech) {
        t += 10;
        (void)feed(CoreInput{proto::UiEvent{proto::UiEvent::PageShown{o.ui.page->id, t}}});
      }
    }
  }
  void open() {
    REQUIRE(core.load_content(content_with_overrides(), Realm::Eval).empty());
    (void)feed(in(data_ready()));
    (void)feed(CoreInput{proto::NetHealth{true}});
    (void)feed(in(playthrough_ready()));
    world(30);
    REQUIRE(core.state().level == Level::L4);
    REQUIRE(feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}}).ui.chat.view ==
            proto::BoxView::Open);
  }
  void reply(proto::RequestId id, const std::string& text) {
    proto::LlmEvent d;
    d.kind = proto::EventKind::Delta;
    d.id = id;
    d.text = text;
    calls.event(d, t);
    (void)feed(CoreInput{d});
    proto::LlmEvent e = ended(id);
    calls.event(e, t);
    (void)feed(CoreInput{e});
  }
  Outputs pause(bool on) {
    t += 10;
    return feed(CoreInput{CoreInput::Pause{on}});
  }
};

}  // namespace

TEST_CASE("a reply through the core: its row from the note, the speaker as shown, the pages shown its only text") {
  Session s;
  s.open();
  const Outputs said = s.feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{"Is the lamp yours?"}}});
  REQUIRE(said.requests.size() == 1);
  REQUIRE(said.calls.size() == 1);
  CHECK(said.calls[0].request == said.requests[0].id);
  CHECK(only(s.calls).speaker == "Lantern-Keeper");
  CHECK(only(s.calls).outcome == CallOutcome::Waiting);
  s.reply(said.requests[0].id, "Hm. It keeps me as I keep it. ");
  s.world(20);
  const proto::CallRow& r = only(s.calls);
  CHECK(r.outcome == CallOutcome::Shown);
  CHECK(r.text == s.core.log().events().back().text);
  CHECK(r.text == "Hm. It keeps me as I keep it.");
  const std::string all = texts_of(s.calls.view());
  CHECK(all.find("lamp yours") == std::string::npos);    // the player's line, which the prompt carries
  CHECK(all.find("invented frame") == std::string::npos);  // the prompt's own words
  CHECK(all.find("nobody saw") == std::string::npos);      // the result's text, never read
}

TEST_CASE("a banned word: the regeneration joins the reply's row, and the deflection after it reads not shown") {
  Session s;
  s.open();
  const Outputs said = s.feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{"Tell me."}}});
  REQUIRE(said.requests.size() == 1);
  const proto::RequestId first = said.requests[0].id;
  proto::LlmEvent d;
  d.kind = proto::EventKind::Delta;
  d.id = first;
  d.text = "A BANNED word. ";
  const Outputs regen = s.feed(CoreInput{d});
  REQUIRE(regen.requests.size() == 1);  // the regeneration
  CHECK(regen.calls.size() == 1);
  CHECK(only(s.calls).outcome == CallOutcome::Waiting);
  d.id = regen.requests[0].id;
  const Outputs deflected = s.feed(CoreInput{d});  // again in the regeneration: the persona's own deflection
  REQUIRE(deflected.calls.size() == 1);
  CHECK(deflected.calls[0].what == CallNote::What::Withheld);
  s.calls.event(ended(first, ErrorClass::Cancelled), s.t);
  s.calls.event(ended(regen.requests[0].id, ErrorClass::Cancelled), s.t);
  s.world(20);
  CHECK(s.core.log().events().back().kind == Commit::Kind::NpcSaid);  // the deflection was shown and committed
  const proto::CallRow& r = only(s.calls);
  CHECK(r.outcome == CallOutcome::NotShown);
  CHECK(r.text.empty());
  CHECK(r.attempts == 2);
}

TEST_CASE("Pause AI: L2 with the flag, the box closed, nothing sent; off, the ladder back to L4, or L3") {
  Session s;
  s.open();
  Outputs o = s.pause(true);
  CHECK(s.core.state().ai_paused);
  CHECK(s.core.state().level == Level::L2);
  REQUIRE(o.changes.size() == 1);
  CHECK(o.changes[0].what == Change::What::Level);
  CHECK(o.changes[0].to == static_cast<std::uint8_t>(Level::L2));
  CHECK(o.ai_allowed);  // the gate is not the pause: the handle's gate stays open, and EldenNet refuses its own calls
  s.world(2);
  CHECK(s.last.ui.chat.view == proto::BoxView::Closed);  // as when the ladder falls
  // Whatever the player does, nothing goes out; the health changing keeps L2.
  for (int i = 0; i < 4; ++i) {
    (void)s.feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
    (void)s.feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{"Hello?"}}});
    (void)s.feed(CoreInput{proto::NetHealth{true}});
    s.world(2);
  }
  CHECK(s.requests.empty());
  CHECK(s.calls.view().rows.empty());
  CHECK(s.core.state().level == Level::L2);
  o = s.pause(false);
  CHECK_FALSE(s.core.state().ai_paused);
  CHECK(s.core.state().level == Level::L4);
  REQUIRE(o.changes.size() == 1);
  CHECK(o.changes[0].to == static_cast<std::uint8_t>(Level::L4));
  // With an optional group tripped, back to L3; with no healthy model, L2 stays.
  (void)s.pause(true);
  (void)s.feed(in(tripped(proto::Group::Camera)));
  (void)s.pause(false);
  CHECK(s.core.state().level == Level::L3);
  (void)s.pause(true);
  (void)s.feed(CoreInput{proto::NetHealth{false}});
  (void)s.pause(false);
  CHECK(s.core.state().level == Level::L2);
}

TEST_CASE("Pause AI while a reply streams: the stream already sent ends as it ends, a regeneration is not sent") {
  Session s;
  s.open();
  const Outputs said = s.feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{"Tell me."}}});
  REQUIRE(said.requests.size() == 1);
  (void)s.pause(true);
  proto::LlmEvent d;
  d.kind = proto::EventKind::Delta;
  d.id = said.requests[0].id;
  d.text = "A BANNED word. ";
  const Outputs o = s.feed(CoreInput{d});
  CHECK(o.requests.empty());  // the lint would regenerate: refused, the "…" page and its toast instead
  CHECK(o.calls.empty());
  CHECK(std::any_of(o.ui.toasts.begin(), o.ui.toasts.end(),
                    [](const proto::Toast& t) { return t.text == "No answer: no model is ready"; }));
  REQUIRE(o.cancels.size() == 1);
  CHECK(o.cancels[0] == said.requests[0].id);
  CHECK(s.requests.size() == 1);
}

TEST_CASE("Pause AI before T1 is ready: L2 once ready, never L4, until it is off") {
  stubs::StubSpeech speech;
  Core core{speech.parts()};
  Mono t = 0;
  (void)core.step(CoreInput{CoreInput::Pause{true}}, t);
  CHECK(core.state().level == Level::Starting);
  CHECK(core.state().ai_paused);
  (void)core.step(in(data_ready()), t += 100);
  (void)core.step(CoreInput{proto::NetHealth{true}}, t += 100);
  CHECK(core.state().level == Level::L2);
  (void)core.step(CoreInput{CoreInput::Pause{false}}, t += 100);
  CHECK(core.state().level == Level::L4);
  // A pause at L1 changes no level: L1 is for the session.
  (void)core.step(in(tripped(proto::Group::World)), t += 100);
  (void)core.step(CoreInput{CoreInput::Pause{true}}, t += 100);
  (void)core.step(CoreInput{CoreInput::Pause{false}}, t += 100);
  CHECK(core.state().level == Level::L1);
}
