// The client on the fake transport and a manual clock: error classes and their actions, the stripped and
// minimal-effort retries, failover, health, budgets, the backup request, preemption and cancellation (ARCH §11.2-11.4).
#include <doctest.h>

#include <chrono>
#include <thread>

#include "rig.hpp"

using namespace en::llm;
using namespace en::llm::test;
using nlohmann::json;
using std::chrono::minutes;
using std::chrono::seconds;

namespace {

const HealthReport::Pair* pair_of(const HealthReport& report, const std::string& model) {
  for (const auto& p : report.pairs) {
    if (p.model == model) return &p;
  }
  return nullptr;
}

const HealthReport::Provider* provider_of(const HealthReport& report, const std::string& id) {
  for (const auto& p : report.providers) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

int failures_of(Rig& rig, const std::string& model) {
  const HealthReport report = rig.client->health();
  const auto* pair = pair_of(report, model);
  return pair == nullptr ? 0 : pair->failures;
}

bool wait_for_active(FakeServer& server, std::size_t n) {
  for (int i = 0; i < 2000 && server.active() != n; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  return server.active() == n;
}

FakeScript broken_after(const std::string& text) {
  return {FakeStep{Millis(0), TransportEventKind::Head, 200, {{"content-type", "text/event-stream"}}, "", ""},
          FakeStep{Millis(100), TransportEventKind::Data, 200, {}, sse_data(delta_chunk(text)), ""},
          FakeStep{Millis(150), TransportEventKind::Failed, 0, {}, "", "Connection reset by peer"}};
}

}  // namespace

TEST_CASE("a reply streams, with its usage, timings and the provider's request") {
  Rig rig;
  rig.server->script("model-a", sse_reply({"Well met", ", traveller."}, "stop", Millis(300), Millis(50),
                                          json{{"prompt_tokens", 20}, {"completion_tokens", 4}}));
  CollectSink sink;
  const ChatResult r = rig.chat("dialogue", Purpose::Reply, &sink);
  REQUIRE(r.ok());
  CHECK(r.text == "Well met, traveller.");
  CHECK(sink.text() == "Well met, traveller.");
  CHECK(r.streamed);
  CHECK(r.first_token == Millis(300));
  CHECK(r.total == Millis(400));  // the finishing chunk comes one gap after the last delta
  CHECK(r.usage.prompt_tokens == 20);
  CHECK(r.usage.completion_tokens == 4);
  CHECK(r.provider == "cloud");
  CHECK(r.model == "model-a");
  CHECK(r.served_model == "served");
  CHECK(r.finish_reason == "stop");
  CHECK(r.attempts.size() == 1);

  const auto requests = rig.server->requests();
  REQUIRE(requests.size() == 1);
  CHECK(requests[0].url == "https://llm.example.invalid/api/v1/chat/completions");
  CHECK(requests[0].authorization == "Bearer invented-test-key");
  CHECK(requests[0].body["max_tokens"] == 140);
  CHECK(requests[0].body["temperature"].get<double>() == doctest::Approx(0.8));
  CHECK(requests[0].body["reasoning"]["enabled"] == false);
  CHECK(requests[0].body["messages"][0]["content"] == "You are an invented test speaker.");
  CHECK_FALSE(requests[0].body.contains("response_format"));  // a reply asks for no schema
  CHECK(rig.client->requests_last_hour() == 1);
  for (const auto& line : rig.log()) CHECK(line.find("invented-test-key") == std::string::npos);
}

TEST_CASE("dialogue: a first-token timeout goes to the fallback model once") {
  Rig rig;
  rig.server->script("model-a", hang());
  rig.server->script("model-b", sse_reply({"Late, but here."}));
  const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);  // an own line gets no backup request
  REQUIRE(r.ok());
  CHECK(r.model == "model-b");
  REQUIRE(r.attempts.size() == 2);
  CHECK(r.attempts[0].error == ErrorClass::FirstTokenTimeout);
  CHECK(r.attempts[0].ended_at == Millis(6000));
  CHECK(r.attempts[1].sent_at == Millis(6000));
  CHECK_FALSE(r.attempts[1].backup);
  CHECK(rig.server->requests()[0].cancelled);
  CHECK(failures_of(rig, "model-a") == 1);

  Rig twice;
  twice.server->script("model-a", hang());
  twice.server->script("model-b", hang());
  const ChatResult r2 = twice.chat("dialogue", Purpose::OwnInitiative);
  CHECK(r2.error == ErrorClass::FirstTokenTimeout);
  CHECK(r2.attempts.size() == 2);
  CHECK(r2.total == Millis(12000));
}

TEST_CASE("a late cloud reply gets a backup at 2.5 s, and the first to stream is kept") {
  Rig rig;
  rig.server->script("model-a", hang());
  rig.server->script("model-b", sse_reply({"From the backup."}, "stop", Millis(200)));
  CollectSink sink;
  const ChatResult r = rig.chat("dialogue", Purpose::Reply, &sink);
  REQUIRE(r.ok());
  CHECK(r.model == "model-b");
  CHECK(sink.text() == "From the backup.");
  REQUIRE(r.attempts.size() == 2);
  CHECK(r.attempts[1].backup);
  CHECK(r.attempts[1].sent_at == Millis(2500));
  CHECK(r.first_token == Millis(2700));
  CHECK(r.attempts[0].lost_race);
  CHECK(r.attempts[0].error == ErrorClass::Cancelled);
  const auto requests = rig.server->requests();
  CHECK(requests[0].cancelled);
  CHECK(requests[0].ended - requests[0].started == Millis(2700));
  CHECK(failures_of(rig, "model-a") == 0);         // the loser counts as no failure
  CHECK(rig.client->requests_last_hour() == 2);    // both count towards the hour

  Rig primary_wins;
  primary_wins.server->script("model-a", sse_reply({"Primary wins."}, "stop", Millis(3000)));
  primary_wins.server->script("model-b", sse_reply({"Too late."}, "stop", Millis(1000)));  // would stream at 3.5 s
  const ChatResult r2 = primary_wins.chat();
  CHECK(r2.model == "model-a");
  CHECK(r2.text == "Primary wins.");
  REQUIRE(r2.attempts.size() == 2);
  CHECK(r2.attempts[1].lost_race);
  CHECK(primary_wins.server->requests()[1].cancelled);
}

TEST_CASE("no backup for own-initiative lines, local providers, or a fallback that is not healthy") {
  {
    Rig rig;
    rig.server->script("model-a", sse_reply({"Slow."}, "stop", Millis(4000)));
    CHECK(rig.chat("dialogue", Purpose::OwnInitiative).ok());
    CHECK(rig.server->requests().size() == 1);
  }
  {
    Rig rig(kLocal);
    rig.server->script("local-model", sse_reply({"Slow, locally."}, "stop", Millis(4000)));
    CHECK(rig.chat().ok());
    CHECK(rig.server->requests().size() == 1);
  }
  {
    Rig rig(kTwoClouds, "[keys]\ncloud = \"invented-test-key\"\n");  // the fallback's provider has no key
    rig.server->script("model-a", sse_reply({"Slow."}, "stop", Millis(4000)));
    CHECK(rig.chat().ok());
    CHECK(rig.server->requests().size() == 1);
  }
}

TEST_CASE("RateLimited cools the whole provider, honouring Retry-After") {
  Rig rig;
  rig.server->script("model-a", http_error(429, R"({"error": {"message": "slow down"}})", {{"retry-after", "120"}}));
  const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);
  CHECK(r.error == ErrorClass::RateLimited);
  CHECK(r.attempts.size() == 1);  // the fallback is on the same provider, which is cooling too
  CHECK(r.message.find("slow down") != std::string::npos);
  const HealthReport report = rig.client->health();
  const auto* cloud = provider_of(report, "cloud");
  REQUIRE(cloud != nullptr);
  CHECK(cloud->cooling_left == Millis(120000));
  CHECK(rig.chat().error == ErrorClass::NoHealthyProvider);
  CHECK(rig.server->requests().size() == 1);
  rig.clock->advance(seconds(121));
  rig.server->script("model-a", sse_reply({"Back again."}));
  CHECK(rig.chat("dialogue", Purpose::OwnInitiative).ok());

  Rig two(kTwoClouds);
  two.server->script("model-a", http_error(429, "{}"));
  two.server->script("model-b", sse_reply({"From the other provider."}));
  const ChatResult r2 = two.chat("dialogue", Purpose::OwnInitiative);
  CHECK(r2.ok());
  CHECK(r2.provider == "other");
  CHECK(two.server->requests()[1].authorization == "Bearer invented-other-key");
  CHECK(two.server->requests()[1].body["reasoning_effort"] == "none");  // each provider's own switch
  // 60 s from the 429, read after the fallback's 120 ms.
  CHECK(provider_of(two.client->health(), "cloud")->cooling_left == Millis(60000 - 120));
}

TEST_CASE("Auth disables the provider until the configuration changes, and never repeats the server's text") {
  Rig rig;
  rig.server->script("model-a", http_error(401, R"({"error": {"message": "Incorrect API key provided: inv***key"}})"));
  const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);
  CHECK(r.error == ErrorClass::Auth);
  CHECK(r.attempts.size() == 1);
  CHECK(r.message.find("inv***") == std::string::npos);
  CHECK(rig.chat().error == ErrorClass::NoHealthyProvider);
  CHECK(rig.log_count("rejected its key") == 1);
  rig.client->reload(make_config(kCloud));
  rig.server->script("model-a", sse_reply({"The key works now."}));
  CHECK(rig.chat().ok());
}

TEST_CASE("BadRequest: one retry without the optional fields, never without the reasoning switch or the floor") {
  Rig rig;
  rig.server->script("model-a", http_error(400, R"({"error": {"message": "unknown field: stream_options"}})"));
  rig.server->script("model-a", sse_reply({"The stripped body works."}));
  const ChatResult r = rig.chat();
  REQUIRE(r.ok());
  REQUIRE(r.attempts.size() == 2);
  CHECK(r.attempts[0].error == ErrorClass::BadRequest);
  CHECK(r.attempts[0].http_status == 400);
  CHECK(r.attempts[1].stripped);
  auto requests = rig.server->requests();
  CHECK(requests[0].body.contains("stream_options"));
  CHECK_FALSE(requests[1].body.contains("stream_options"));
  CHECK(requests[1].body["reasoning"]["enabled"] == false);
  CHECK(requests[1].body["provider"]["quantizations"].size() == 4);
  CHECK_FALSE(requests[1].body["provider"].contains("sort"));
  CHECK(failures_of(rig, "model-a") == 0);  // our request, not counted

  // The pair has shown it takes only the stripped body, so it starts there now.
  rig.server->script("model-a", sse_reply({"Again."}));
  CHECK(rig.chat().ok());
  CHECK_FALSE(rig.server->requests()[2].body.contains("stream_options"));
  CHECK(rig.log_count("stripped body") == 1);

  Rig twice;
  twice.server->script("model-a", http_error(400, "{}"));
  twice.server->script("model-a", http_error(422, "{}"));
  const ChatResult r2 = twice.chat();
  CHECK(r2.error == ErrorClass::BadRequest);
  CHECK(r2.attempts.size() == 2);
}

TEST_CASE("a reply's stop sequences reach the server; a 400 is retried without them, and they stay off for the pair") {
  Rig rig;
  ChatRequest request;
  request.messages = {Message::system("An invented frame.", true), Message::user("Where does the road go?")};
  request.stop = {"\nThe Wanderer:", "\nWanderer:"};
  rig.server->script("model-a", sse_reply({"Down, mostly."}));
  REQUIRE(rig.client->chat(request, nullptr, {}).ok());
  CHECK(rig.server->requests()[0].body["stop"] == json({"\nThe Wanderer:", "\nWanderer:"}));

  // A model that refuses the field: the stripped retry sends none, and the pair starts stripped from then on.
  Rig refuses;
  refuses.server->script("model-a", http_error(400, R"({"error": {"message": "Unsupported parameter: 'stop'"}})"));
  refuses.server->script("model-a", sse_reply({"Down, mostly."}));
  const ChatResult r = refuses.client->chat(request, nullptr, {});
  REQUIRE(r.ok());
  REQUIRE(r.attempts.size() == 2);
  CHECK(r.attempts[1].stripped);
  auto seen = refuses.server->requests();
  CHECK(seen[0].body.contains("stop"));
  CHECK_FALSE(seen[1].body.contains("stop"));
  refuses.server->script("model-a", sse_reply({"Again."}));
  CHECK(refuses.client->chat(request, nullptr, {}).ok());
  CHECK_FALSE(refuses.server->requests()[2].body.contains("stop"));

  // A request without any sends no field.
  rig.server->script("model-a", sse_reply({"Fine."}));
  CHECK(rig.chat().ok());
  CHECK_FALSE(rig.server->requests()[1].body.contains("stop"));
}

TEST_CASE("a model that refuses reasoning off gets one retry at minimal effort, remembered for the session") {
  Rig rig;
  rig.server->script("model-a", http_error(400, R"({"error": {"message": "Reasoning is mandatory for this model and cannot be disabled"}})"));
  rig.server->script("model-a", sse_reply({"Thought briefly."}));
  const ChatResult r = rig.chat();
  REQUIRE(r.ok());
  REQUIRE(r.attempts.size() == 2);
  CHECK(r.attempts[1].minimal_effort);
  CHECK_FALSE(r.attempts[1].stripped);
  const auto requests = rig.server->requests();
  CHECK(requests[1].body["reasoning"] == json({{"effort", "minimal"}}));
  CHECK(requests[1].body.contains("stream_options"));  // the error named reasoning, so the optional fields stay
  CHECK(requests[1].body["provider"]["quantizations"].size() == 4);
  CHECK(rig.log_count("refuses reasoning off") == 1);

  rig.server->script("model-a", sse_reply({"Still brief."}));
  CHECK(rig.chat().ok());
  CHECK(rig.server->requests()[2].body["reasoning"] == json({{"effort", "minimal"}}));
  CHECK(rig.log_count("refuses reasoning off") == 1);  // said once
  CHECK(pair_of(rig.client->health(), "model-a")->minimal_effort);

  Rig local(kLocal);  // llama.cpp's switch is lifted and the low effort asked for instead
  local.server->script("local-model", http_error(422, R"({"error": "thinking cannot be turned off"})"));
  local.server->script("local-model", sse_reply({"Done."}));
  REQUIRE(local.chat().ok());
  const auto body = local.server->requests()[1].body;
  CHECK_FALSE(body.contains("chat_template_kwargs"));
  CHECK(body["reasoning_effort"] == "low");
}

TEST_CASE("server errors fall back once; three in a row cool the pair for 60 s; a success resets the count") {
  Rig rig;
  for (int i = 0; i < 3; ++i) {
    if (i == 1) {
      rig.server->script("model-a", connection_failure());  // a reset is a Server error too
    } else {
      rig.server->script("model-a", http_error(503, "{}"));
    }
    rig.server->script("model-b", sse_reply({"From the fallback."}));
    const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);
    CHECK(r.ok());
    CHECK(r.model == "model-b");
  }
  CHECK(rig.log_count("three failures") == 1);
  CHECK(pair_of(rig.client->health(), "model-a")->cooling_left == Millis(60000 - 120));

  // While it cools, the slot goes straight to its fallback.
  rig.server->script("model-b", sse_reply({"Directly."}));
  const ChatResult cooling = rig.chat("dialogue", Purpose::OwnInitiative);
  CHECK(cooling.attempts.size() == 1);
  CHECK(cooling.model == "model-b");
  rig.clock->advance(seconds(61));
  rig.server->script("model-a", sse_reply({"Recovered."}));
  CHECK(rig.chat("dialogue", Purpose::OwnInitiative).model == "model-a");

  Rig reset;
  auto fail_once = [&] {
    reset.server->script("model-a", http_error(500, "{}"));
    reset.server->script("model-b", sse_reply({"b"}));
    reset.chat("dialogue", Purpose::OwnInitiative);
  };
  fail_once();
  fail_once();
  reset.server->script("model-a", sse_reply({"a"}));
  reset.chat("dialogue", Purpose::OwnInitiative);
  fail_once();
  fail_once();
  CHECK(failures_of(reset, "model-a") == 2);
  CHECK(reset.log_count("three failures") == 0);

  Rig all_cooling(kCloud);
  for (int i = 0; i < 3; ++i) {
    all_cooling.server->script("model-a", http_error(502, "{}"));
    all_cooling.server->script("model-b", http_error(502, "{}"));
    CHECK(all_cooling.chat("dialogue", Purpose::OwnInitiative).error == ErrorClass::Server);
  }
  const ChatResult none = all_cooling.chat();
  CHECK(none.error == ErrorClass::NoHealthyProvider);
  CHECK(none.message.find("cooling") != std::string::npos);
}

TEST_CASE("Empty falls back; Truncated and Refused return with their text and are handled by the caller") {
  {
    Rig rig;
    rig.server->script("model-a", sse_raw({{Millis(100), "data: [DONE]\n\n"}}, Millis(100)));
    rig.server->script("model-b", sse_reply({"Not empty."}));
    const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);
    CHECK(r.ok());
    CHECK(r.attempts[0].error == ErrorClass::Empty);
  }
  {
    Rig rig;
    rig.server->script("model-a", sse_reply({"The road goes on and"}, "length"));
    const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);
    CHECK(r.error == ErrorClass::Truncated);
    CHECK(r.text == "The road goes on and");
    CHECK(r.attempts.size() == 1);
  }
  {
    Rig rig;
    rig.server->script("model-a", sse_reply({"The road goes on."}, "length"));
    CHECK(rig.chat("dialogue", Purpose::OwnInitiative).ok());  // cut at a full sentence: nothing lost
  }
  {
    Rig rig;
    rig.server->script("model-a", sse_reply({"Partial"}, "content_filter"));
    const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);
    CHECK(r.error == ErrorClass::Refused);
    CHECK(r.attempts.size() == 1);
  }
  {
    Rig rig;
    rig.server->script("model-a", sse_reply({"As an AI, I cannot take part in this."}));
    const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);
    CHECK(r.error == ErrorClass::Refused);
    CHECK(failures_of(rig, "model-a") == 0);  // not counted
  }
  {
    Rig rig;  // an error object inside the stream
    rig.server->script("model-a", sse_raw({{Millis(100), "data: {\"error\": {\"code\": 502, \"message\": \"upstream failed\"}}\n\n"}},
                                          Millis(100)));
    rig.server->script("model-b", sse_reply({"From the fallback."}));
    const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);
    CHECK(r.ok());
    CHECK(r.attempts[0].error == ErrorClass::Server);
  }
}

TEST_CASE("a stream that breaks after its text was shown keeps the text, and no second reply follows") {
  Rig rig;
  rig.server->script("model-a", broken_after("Half a"));
  CollectSink sink;
  const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative, &sink);
  CHECK(r.error == ErrorClass::Server);
  CHECK(r.text == "Half a");
  CHECK(r.streamed);
  CHECK(sink.text() == "Half a");
  CHECK(r.attempts.size() == 1);

  Rig quiet;  // with nothing shown, the fallback may still answer
  quiet.server->script("model-a", broken_after("Half a"));
  quiet.server->script("model-b", sse_reply({"Whole."}));
  CHECK(quiet.chat("dialogue", Purpose::OwnInitiative).text == "Whole.");
}

TEST_CASE("a total timeout keeps what streamed") {
  Rig rig;
  rig.server->script("model-a", sse_raw({{Millis(100), sse_data(delta_chunk("Slowly, "))},
                                         {Millis(15000), sse_data(delta_chunk("very slowly"))}},
                                        Millis(25000)));
  CollectSink sink;
  const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative, &sink);
  CHECK(r.error == ErrorClass::TotalTimeout);
  CHECK(r.text == "Slowly, very slowly");
  CHECK(sink.text() == "Slowly, very slowly");
  CHECK(r.total == Millis(20000));
  CHECK(r.attempts.size() == 1);
}

TEST_CASE("each slot's retries: bark_pool once, memory twice, bark_live never") {
  {
    Rig rig;
    rig.server->script("*", hang());
    rig.server->script("*", hang());
    const ChatResult r = rig.chat("bark_pool", Purpose::Background);
    CHECK(r.error == ErrorClass::FirstTokenTimeout);
    CHECK(r.attempts.size() == 2);
    CHECK(r.total == Millis(30000));
  }
  {
    Rig rig;
    rig.server->script("model-a", http_error(503, "{}"));
    rig.server->script("model-a", sse_reply({"A pool batch."}));
    const ChatResult r = rig.chat("bark_pool", Purpose::Background);
    CHECK(r.ok());
    CHECK(r.attempts.size() == 2);
  }
  {
    Rig rig;
    for (int i = 0; i < 3; ++i) rig.server->script("*", hang());
    const ChatResult r = rig.chat("memory", Purpose::Background);
    CHECK(r.error == ErrorClass::FirstTokenTimeout);
    CHECK(r.attempts.size() == 3);
    CHECK(r.total == Millis(60000));
  }
  {
    Rig rig;
    rig.server->script("model-a", sse_reply({"The journal says"}, "length"));
    rig.server->script("model-a", sse_reply({"The journal says enough."}, "length"));
    const ChatResult r = rig.chat("memory", Purpose::Background);
    CHECK(r.ok());
    CHECK(r.attempts.size() == 2);
  }
  {
    Rig rig;
    rig.server->script("*", hang());
    const ChatResult r = rig.chat("bark_live", Purpose::OwnInitiative);
    CHECK(r.error == ErrorClass::FirstTokenTimeout);
    CHECK(r.attempts.size() == 1);
    CHECK(r.total == Millis(1500));
  }
}

TEST_CASE("the requests-per-hour bound: replies still go out, everything else stops, and the hour rolls") {
  Rig rig(kCloud, kSecrets, "budget = { requests_per_hour = 60 }\n");
  for (int i = 0; i < 60; ++i) {
    rig.server->script("*", sse_reply({"ok."}));
    REQUIRE(rig.chat("dialogue", Purpose::OwnInitiative).ok());
  }
  CHECK(rig.chat("dialogue", Purpose::OwnInitiative).error == ErrorClass::BudgetSpent);
  CHECK(rig.chat("bark_pool", Purpose::Background).error == ErrorClass::BudgetSpent);
  CHECK(rig.chat("decisions", Purpose::Decision).error == ErrorClass::BudgetSpent);
  rig.server->script("*", sse_reply({"The player asked."}));
  CHECK(rig.chat("dialogue", Purpose::Reply).ok());
  CHECK(rig.client->requests_last_hour() == 61);
  CHECK(rig.server->requests().size() == 61);
  rig.clock->advance(minutes(61));
  rig.server->script("*", sse_reply({"A new hour."}));
  CHECK(rig.chat("dialogue", Purpose::OwnInitiative).ok());
}

TEST_CASE("concurrency: one dialogue request at a time; a reply takes a busy local provider from a background job") {
  {
    Rig rig;
    rig.server->set_auto_advance(false);
    rig.server->script("model-a", hang());
    std::stop_source stop_first;
    ChatResult first;
    std::thread t([&] { first = rig.chat("dialogue", Purpose::OwnInitiative, nullptr, stop_first.get_token()); });
    REQUIRE(wait_for_active(*rig.server, 1));
    CHECK(rig.chat("dialogue", Purpose::Reply).error == ErrorClass::Busy);
    stop_first.request_stop();
    t.join();
    CHECK(first.error == ErrorClass::Cancelled);
    CHECK(rig.server->requests()[0].cancelled);
  }
  {
    Rig rig(kLocal);
    rig.server->set_auto_advance(false);
    rig.server->script("local-model", hang());
    ChatResult memory;
    std::thread t([&] { memory = rig.chat("memory", Purpose::Background); });
    REQUIRE(wait_for_active(*rig.server, 1));
    rig.server->script("local-model", sse_reply({"The reply comes first."}, "stop", Millis(0), Millis(0)));
    const ChatResult reply = rig.chat("dialogue", Purpose::Reply);
    t.join();
    CHECK(reply.ok());
    CHECK(memory.error == ErrorClass::Preempted);
    CHECK(rig.log_count("gives way") == 1);
  }
  {
    Rig rig(kLocal);  // nothing takes the provider from dialogue
    rig.server->set_auto_advance(false);
    rig.server->script("local-model", hang());
    std::stop_source stop_line;
    ChatResult line;
    std::thread t([&] { line = rig.chat("dialogue", Purpose::OwnInitiative, nullptr, stop_line.get_token()); });
    REQUIRE(wait_for_active(*rig.server, 1));
    const ChatResult callout = rig.chat("bark_live", Purpose::OwnInitiative);
    CHECK(callout.error == ErrorClass::Busy);
    stop_line.request_stop();
    t.join();
    CHECK(line.error == ErrorClass::Cancelled);
  }
}

TEST_CASE("cancellation: a cancelled token sends nothing, and a cancel mid-stream keeps what came") {
  Rig rig;
  std::stop_source early;
  early.request_stop();
  CHECK(rig.chat("dialogue", Purpose::Reply, nullptr, early.get_token()).error == ErrorClass::Cancelled);
  CHECK(rig.server->requests().empty());

  // The input epoch moves on while the reply streams.
  struct StopSink final : StreamSink {
    std::stop_source* stop = nullptr;
    std::string text;
    void on_delta(std::string_view t) override {
      text += t;
      stop->request_stop();
    }
  };
  std::stop_source mid;
  StopSink sink;
  sink.stop = &mid;
  rig.server->script("model-a", sse_reply({"First words", " and more"}, "stop", Millis(100), Millis(500)));
  const ChatResult r = rig.chat("dialogue", Purpose::Reply, &sink, mid.get_token());
  CHECK(r.error == ErrorClass::Cancelled);
  CHECK(r.text == "First words");
  CHECK(sink.text == "First words");
  CHECK(rig.server->requests().back().cancelled);
  CHECK(failures_of(rig, "model-a") == 0);
}

TEST_CASE("the offline gate: nothing goes out while it is closed") {
  Rig rig;
  rig.client->set_gate_open(false);
  CHECK(rig.chat().error == ErrorClass::GateClosed);
  CHECK(rig.server->requests().empty());
  rig.client->set_gate_open(true);
  rig.server->script("model-a", sse_reply({"Open again."}));
  CHECK(rig.chat().ok());
}

TEST_CASE("unknown slots fall through to dialogue, and a request never exceeds its slot's budget") {
  Rig rig;
  ChatRequest request;
  request.messages = {Message::user("Hello.")};
  request.slot = "banter";
  request.max_tokens = 1000;
  rig.server->script("*", sse_reply({"ok."}));
  CHECK(rig.client->chat(request).ok());
  CHECK(rig.server->requests()[0].body["max_tokens"] == 140);

  request.slot = "bark_live";
  request.max_tokens = 10;
  request.temperature = 0.2;
  rig.server->script("*", sse_reply({"Hah!"}));
  CHECK(rig.client->chat(request).ok());
  CHECK(rig.server->requests()[1].body["max_tokens"] == 10);
  CHECK(rig.server->requests()[1].body["temperature"].get<double>() == doctest::Approx(0.2));

  request.messages.clear();
  CHECK_THROWS_AS(rig.client->chat(request), std::invalid_argument);
}

TEST_CASE("a server that ignores stream: true, and reasoning that never counts as the first token") {
  {
    Rig rig;
    rig.server->script("model-a",
                       {FakeStep{Millis(0), TransportEventKind::Head, 200, {{"content-type", "application/json"}}, "", ""},
                        FakeStep{Millis(400), TransportEventKind::Data, 200, {},
                                 R"({"choices": [{"message": {"content": "All at once."}, "finish_reason": "stop"}]})", ""},
                        FakeStep{Millis(400), TransportEventKind::Done, 200, {}, "", ""}});
    CollectSink sink;
    const ChatResult r = rig.chat("dialogue", Purpose::Reply, &sink);
    CHECK(r.ok());
    CHECK(r.text == "All at once.");
    CHECK(sink.text() == "All at once.");
  }
  {
    Rig rig;
    rig.server->script("model-a", sse_raw({{Millis(100), sse_data(delta_chunk("<think>pondering"))},
                                           {Millis(7000), sse_data(delta_chunk("</think>Answer."))}},
                                          Millis(7000)));
    rig.server->script("model-b", sse_reply({"Plain."}));
    const ChatResult r = rig.chat("dialogue", Purpose::OwnInitiative);
    CHECK(r.attempts[0].error == ErrorClass::FirstTokenTimeout);
    CHECK(r.model == "model-b");
  }
  {
    Rig rig;
    rig.server->script("model-a", sse_reply({"<think>brief</think>", "Hello."}));
    CollectSink sink;
    const ChatResult r = rig.chat("dialogue", Purpose::Reply, &sink);
    CHECK(r.text == "Hello.");
    CHECK(sink.text() == "Hello.");
  }
}

TEST_CASE("nothing to send: no transport, no model, or no key") {
  {
    Client client(make_config(kCloud), nullptr);
    ChatRequest request;
    request.messages = {Message::user("Hello.")};
    CHECK(client.chat(request).error == ErrorClass::NoHealthyProvider);
  }
  {
    Rig rig("schema = 1\n[[provider]]\nid = \"x\"\nbase_url = \"https://x.example.invalid\"\n");
    const ChatResult r = rig.chat();
    CHECK(r.error == ErrorClass::NoHealthyProvider);
    CHECK(r.message.find("has no model") != std::string::npos);
  }
  {
    Rig rig(kCloud, "");
    const ChatResult r = rig.chat();
    CHECK(r.error == ErrorClass::NoHealthyProvider);
    CHECK(r.message.find("key") != std::string::npos);
    CHECK(rig.server->requests().empty());
  }
}

TEST_CASE("the rest of the table: memory retries stripped and on Empty; pools drop; memory requeues on a rate limit") {
  {
    Rig rig;
    rig.server->script("model-a", http_error(400, R"({"error": {"message": "unknown field"}})"));
    rig.server->script("model-a", sse_reply({"A journal entry."}));
    const ChatResult r = rig.chat("memory", Purpose::Background);
    CHECK(r.ok());
    REQUIRE(r.attempts.size() == 2);
    CHECK(r.attempts[1].stripped);
  }
  {
    Rig rig;
    rig.server->script("model-a", sse_raw({{Millis(100), "data: [DONE]\n\n"}}, Millis(100)));
    rig.server->script("model-a", sse_reply({"A journal entry."}));
    const ChatResult r = rig.chat("memory", Purpose::Background);
    CHECK(r.ok());
    CHECK(r.attempts.size() == 2);
  }
  for (const FakeScript& script : {http_error(429, "{}"), sse_raw({{Millis(100), "data: [DONE]\n\n"}}, Millis(100)),
                                   http_error(400, "{}"), sse_reply({"Cut off in the"}, "length")}) {
    Rig rig;
    rig.server->script("model-a", script);
    const ChatResult r = rig.chat("bark_pool", Purpose::Background);
    CHECK_FALSE(r.ok());
    CHECK(r.attempts.size() == 1);  // the batch is dropped, rejected or kept as far as it is valid: the caller's
  }
  {
    Rig rig;
    rig.server->script("model-a", http_error(429, "{}"));
    const ChatResult r = rig.chat("memory", Purpose::Background);
    CHECK(r.error == ErrorClass::RateLimited);  // the caller requeues
    CHECK(r.attempts.size() == 1);
  }
}
