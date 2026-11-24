// The SSE parser, the <think> filter, the reply assembler and the end-of-reply checks (ARCH §11.1, §11.2).
#include <doctest.h>

#include <nlohmann/json.hpp>

#include "en/llm/errors.hpp"
#include "en/llm/health.hpp"
#include "en/llm/sse.hpp"
#include "en/llm/stream.hpp"

using namespace en::llm;
using nlohmann::json;

namespace {

std::vector<SseEvent> parse_all(std::string_view text, std::size_t piece = 0) {
  SseParser parser;
  std::vector<SseEvent> events;
  if (piece == 0) {
    parser.feed(text, events);
  } else {
    for (std::size_t i = 0; i < text.size(); i += piece) parser.feed(text.substr(i, piece), events);
  }
  parser.finish(events);
  return events;
}

std::string filter_all(const std::vector<std::string>& chunks, int* resets = nullptr) {
  ThinkFilter filter;
  std::string out;
  for (const auto& c : chunks) {
    auto o = filter.feed(c);
    if (o.reset) {
      out.clear();
      if (resets != nullptr) ++*resets;
    }
    out += o.text;
  }
  return out + filter.finish();
}

}  // namespace

TEST_CASE("SSE: data lines, multi-line events, comments and every line ending") {
  const std::string stream =
      ": keep-alive comment\n"
      "data: first\n\n"
      "data: two\r\ndata: lines\r\n\r\n"
      "event: notice\rid: 7\rdata:no space\r\r"
      "retry: 1500\n"
      "data: [DONE]\n\n";
  const auto events = parse_all(stream);
  REQUIRE(events.size() == 4);
  CHECK(events[0].data == "first");
  CHECK(events[0].type == "message");
  CHECK(events[1].data == "two\nlines");
  CHECK(events[2].type == "notice");
  CHECK(events[2].id == "7");
  CHECK(events[2].data == "no space");
  CHECK(events[3].data == "[DONE]");
  CHECK(events[3].id == "7");  // the last id carries forward

  SseParser parser;
  std::vector<SseEvent> ignored;
  parser.feed(stream, ignored);
  CHECK(parser.retry_ms() == 1500);
}

TEST_CASE("SSE: bytes split anywhere give the same events") {
  const std::string stream = "\xEF\xBB\xBF" "data: {\"a\":1}\r\n\r\ndata: x\r\ndata: y\n\n: c\ndata: last\n\n";
  const auto whole = parse_all(stream);
  for (std::size_t piece : {1u, 2u, 3u, 7u}) {
    const auto split = parse_all(stream, piece);
    REQUIRE(split.size() == whole.size());
    for (std::size_t i = 0; i < whole.size(); ++i) CHECK(split[i].data == whole[i].data);
  }
  REQUIRE(whole.size() == 3);
  CHECK(whole[0].data == "{\"a\":1}");  // the byte-order mark is gone
}

TEST_CASE("SSE: an empty data line is an event; an event with no data is none; a last event without its blank line counts") {
  const auto events = parse_all("data:\n\nevent: ping\n\ndata: [DONE]\n");
  REQUIRE(events.size() == 2);
  CHECK(events[0].data.empty());
  CHECK(events[1].data == "[DONE]");
}

TEST_CASE("SSE: a line past the cap stops the parse") {
  SseParser parser(16);
  std::vector<SseEvent> events;
  CHECK_FALSE(parser.feed("data: " + std::string(64, 'x'), events));
  CHECK(parser.failed());
  CHECK_FALSE(parser.feed("data: ok\n\n", events));
  CHECK(events.empty());
}

TEST_CASE("think: blocks are stripped, also when the tags are split across chunks") {
  CHECK(filter_all({"<think>plan the reply</think>Hello."}) == "Hello.");
  CHECK(filter_all({"<thi", "nk>plan", " more</th", "ink>Hel", "lo."}) == "Hello.");
  CHECK(filter_all({"A", "<", "think>x</think>", "B"}) == "AB");
  CHECK(filter_all({"a < b, and c <= d"}) == "a < b, and c <= d");
  CHECK(filter_all({"ends with <"}) == "ends with <");  // held back, then released at the end
  CHECK(filter_all({"before<think>never closed"}) == "before");
  CHECK(filter_all({"<think>a</think>b</think>c"}) == "bc");  // a stray closing tag after a block: the tag alone goes
}

TEST_CASE("think: a reply that only closes its reasoning withdraws what came before") {
  int resets = 0;
  CHECK(filter_all({"the model reasons here", " and more", "</think>The reply."}, &resets) == "The reply.");
  CHECK(resets == 1);
}

TEST_CASE("assembler: deltas, the finish reason, usage, logprobs and [DONE]") {
  ReplyAssembler r;
  std::string streamed;
  auto feed = [&](const json& chunk) { streamed += r.on_event_data(chunk.dump()).delta; };
  feed({{"model", "served-model"}, {"choices", {{{"delta", {{"role", "assistant"}, {"content", "\n  "}}}}}}});
  feed({{"choices", {{{"delta", {{"content", "Well met"}}}}}}});
  feed({{"choices", {{{"delta", {{"reasoning", "hidden thoughts"}}}}}}});
  feed({{"choices", {{{"delta", {{"content", ", traveller. "}}}, {"finish_reason", nullptr}}}}});
  feed({{"choices",
         {{{"delta", json::object()},
           {"finish_reason", "stop"},
           {"logprobs", {{"content", {{{"token", "Well"}, {"logprob", -0.1}, {"top_logprobs", {{{"token", "Well"}, {"logprob", -0.1}}, {{"token", "Hail"}, {"logprob", -2.5}}}}}}}}}}}}});
  feed({{"choices", json::array()}, {"usage", {{"prompt_tokens", 812}, {"completion_tokens", 9}, {"prompt_tokens_details", {{"cached_tokens", 640}}}}}});
  CHECK(r.on_event_data(" [DONE] ").delta.empty());
  r.finish();

  CHECK(r.done_seen());
  CHECK(r.text() == "Well met, traveller.");  // outer whitespace trimmed
  CHECK(streamed == "Well met, traveller. ");
  CHECK(r.finish_reason() == "stop");
  CHECK(r.served_model() == "served-model");
  CHECK(r.usage().prompt_tokens == 812);
  CHECK(r.usage().completion_tokens == 9);
  CHECK(r.usage().cached_tokens == 640);
  CHECK(r.saw_reasoning());
  REQUIRE(r.logprobs().size() == 1);
  CHECK(r.logprobs()[0].top.size() == 2);
  CHECK(r.logprobs()[0].top[1].token == "Hail");
}

TEST_CASE("assembler: bad chunks are counted, error objects classified, a whole JSON body read") {
  ReplyAssembler r;
  r.on_event_data("{not json");
  r.on_event_data(R"({"error": {"code": 429, "message": "slow down, key sk-test"}})");
  CHECK(r.bad_chunks() == 1);
  REQUIRE(r.stream_error().has_value());
  CHECK(*r.stream_error() == ErrorClass::RateLimited);
  CHECK(r.stream_error_message().find("sk-test") == std::string::npos);

  ReplyAssembler whole;
  const auto step = whole.on_whole_body(R"({"choices": [{"message": {"content": "<think>x</think>Short answer."}, "finish_reason": "stop"}],
                                           "usage": {"prompt_tokens": 5, "completion_tokens": 2}})");
  whole.finish();
  CHECK(step.delta == "Short answer.");
  CHECK(whole.text() == "Short answer.");
  CHECK(whole.done_seen());
  CHECK(whole.raw_text() == "<think>x</think>Short answer.");
  CHECK(whole.usage().cost == -1);  // no cost reported: none (D18)
}

TEST_CASE("assembler: a cost the provider's usage reports is kept, anything else is none (D18)") {
  const auto cost_of = [](const json& cost) {
    ReplyAssembler r;
    (void)r.on_whole_body(json{{"choices", {{{"message", {{"content", "Short answer."}}}, {"finish_reason", "stop"}}}},
                               {"usage", {{"prompt_tokens", 5}, {"completion_tokens", 2}, {"cost", cost}}}}
                              .dump());
    r.finish();
    return r.usage().cost;
  };
  CHECK(cost_of(0.00042) == doctest::Approx(0.00042));
  CHECK(cost_of(0) == 0);
  CHECK(cost_of(-0.5) == -1);
  CHECK(cost_of("0.1") == -1);
  CHECK(cost_of(nullptr) == -1);
  CHECK(cost_of(1e9) == -1);
}

TEST_CASE("assembler: error objects are classified") {
  CHECK(classify_error_object(json{{"code", 502}}) == ErrorClass::Server);
  CHECK(classify_error_object(json{{"code", 401}}) == ErrorClass::Auth);
  CHECK(classify_error_object(json{{"code", 400}}) == ErrorClass::BadRequest);
  CHECK(classify_error_object(json{{"type", "rate_limit_exceeded"}}) == ErrorClass::RateLimited);
  CHECK(classify_error_object(json("upstream failed")) == ErrorClass::Server);
}

TEST_CASE("HTTP statuses map to the error classes") {
  CHECK(classify_http_status(200) == ErrorClass::None);
  CHECK(classify_http_status(429) == ErrorClass::RateLimited);
  CHECK(classify_http_status(401) == ErrorClass::Auth);
  CHECK(classify_http_status(403) == ErrorClass::Auth);
  for (int s : {400, 404, 422, 413}) CHECK(classify_http_status(s) == ErrorClass::BadRequest);
  for (int s : {500, 502, 503, 504, 408}) CHECK(classify_http_status(s) == ErrorClass::Server);
  CHECK(counts_in_health(ErrorClass::Server));
  CHECK(counts_in_health(ErrorClass::FirstTokenTimeout));
  CHECK_FALSE(counts_in_health(ErrorClass::BadRequest));
  CHECK_FALSE(counts_in_health(ErrorClass::Refused));
  CHECK_FALSE(counts_in_health(ErrorClass::Cancelled));
}

TEST_CASE("sentences, refusals, error bodies and Retry-After") {
  CHECK(ends_at_sentence("It is done."));
  CHECK(ends_at_sentence("Is it? \"Yes!\" "));
  CHECK(ends_at_sentence("And so\xE2\x80\xA6"));
  CHECK(ends_at_sentence("\xE2\x80\x9CGo.\xE2\x80\x9D"));
  CHECK_FALSE(ends_at_sentence("And then the"));
  CHECK_FALSE(ends_at_sentence(""));
  CHECK(complete_sentences("One. Two! Three and") == "One. Two!");
  CHECK(complete_sentences("\"Stay.\" Then") == "\"Stay.\"");
  CHECK(complete_sentences("no end at all").empty());

  CHECK(looks_like_refusal("As an AI language model, I cannot do that."));
  CHECK(looks_like_refusal("  \"I can\xE2\x80\x99t help with that request.\""));
  CHECK(looks_like_refusal("I'm sorry, but I can't help with this."));
  CHECK_FALSE(looks_like_refusal("I can't let you pass, Tarnished."));
  CHECK_FALSE(looks_like_refusal("Sorry, friend, the road is closed."));

  const std::string scrubbed = describe_error_body(R"({"error": {"message": "bad key sk-test used"}})", 400);
  CHECK(scrubbed.find("HTTP 400") == 0);
  CHECK(scrubbed.find("sk-test") == std::string::npos);
  CHECK(scrubbed.find("<redacted>") != std::string::npos);
  CHECK(describe_error_body("token abcdefghijklmnopqrstuvwxyz rejected", 400).find("abcdefghijklmnopqrstuvwxyz") ==
        std::string::npos);  // any long run of token characters
  CHECK(describe_error_body(R"({"error": {"message": "Incorrect API key provided: inv***xyz"}})", 401) ==
        "HTTP 401: the provider rejected the key");
  CHECK(describe_error_body("Bearer abc rejected", 500).find("abc") == std::string::npos);
  std::string long_body;
  for (int i = 0; i < 300; ++i) long_body += "x ";
  CHECK(describe_error_body(long_body, 502).size() < 230);

  CHECK(names_reasoning(R"({"error": "Reasoning is mandatory for this endpoint and cannot be disabled."})"));
  CHECK(names_reasoning("thinking must stay enabled"));
  CHECK_FALSE(names_reasoning(R"({"error": "unknown field stream_options"})"));

  CHECK(rate_limit_cooldown({{"retry-after", "120"}}) == Millis(120000));
  CHECK(rate_limit_cooldown({{"retry-after", "1000000"}}) == Millis(60000));  // too long a number: the default
  CHECK(rate_limit_cooldown({{"retry-after", "900"}}) == Millis(300000));    // capped at 300 s
  CHECK(rate_limit_cooldown({{"retry-after", "Wed, 21 Oct 2026 07:28:00 GMT"}}) == Millis(60000));
  CHECK(rate_limit_cooldown({}) == Millis(60000));
}
