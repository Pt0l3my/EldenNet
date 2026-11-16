// The JSON forms of the model client's messages: a round trip for every type, through JSON values and through text,
// with every field away from its default; a field left out keeps its default; and malformed JSON is refused with a
// message that names the path and the problem, never a value. All data is invented.
#include <doctest.h>

#include <string>

#include "en/proto/json.hpp"
#include "en/proto/llm.hpp"

using namespace en::proto;
using nlohmann::json;
using std::chrono::milliseconds;

namespace {

// Ids past 2^53, where a reader holding numbers as doubles would lose them (ARCH §2.2's store rules).
constexpr RequestId kBigId = (RequestId{1} << 60) + 7;
constexpr Epoch kBigEpoch = (Epoch{1} << 53) + 1;

template <class T>
void round_trip(const T& value) {
  const json j = value;
  CHECK(j.get<T>() == value);
  CHECK(read_json<T>(j.dump()) == value);
}

template <class T>
std::string refusal(const json& j) {
  try {
    (void)j.get<T>();
  } catch (const ProtoError& e) {
    return e.what();
  }
  return "";
}

Message sample_message() {
  Message m;
  m.role = Role::Assistant;
  m.parts = {{"An invented opening line.\n\n", true}, {"And its second part.", false}};
  return m;
}

ChatRequest sample_chat() {
  ChatRequest r;
  r.slot = std::string(kMemory);
  r.purpose = Purpose::Background;
  r.messages = {Message::system("You keep an invented journal.", true), sample_message(), Message::user("Go on.")};
  r.max_tokens = 321;
  r.temperature = 0.375;
  r.json_schema = json{{"type", "object"}, {"properties", {{"answer", {{"type", "string"}}}}}};
  r.top_logprobs = 5;
  return r;
}

AttemptInfo sample_attempt(int n) {
  AttemptInfo a;
  a.provider = "invented-cloud-" + std::to_string(n);
  a.model = "invented-model";
  a.error = n == 0 ? ErrorClass::Server : ErrorClass::None;
  a.http_status = n == 0 ? 503 : 200;
  a.backup = n == 1;
  a.stripped = true;
  a.minimal_effort = true;
  a.lost_race = n == 1;
  a.sent_at = milliseconds(10 + n);
  a.first_token = milliseconds(n == 0 ? -1 : 240);
  a.ended_at = milliseconds(900 + n);
  a.message = n == 0 ? "HTTP 503: an invented outage" : "";
  return a;
}

ChatResult sample_result() {
  ChatResult r;
  r.error = ErrorClass::Truncated;
  r.message = "cut at max_tokens (140) before a full sentence";
  r.text = "The lantern swings. And then";
  r.raw_text = "<think>a plan</think>The lantern swings. And then";
  r.finish_reason = "length";
  r.provider = "invented-cloud-1";
  r.model = "invented-model";
  r.served_model = "invented-model-2026";
  r.usage = Usage{1200, 140, 1024};
  r.logprobs = {TokenLogprob{"The", -0.125, {{"The", -0.125}, {"A", -2.5}}}, TokenLogprob{" lantern", -0.0625, {}}};
  r.used_schema = true;
  r.used_logprobs = true;
  r.streamed = true;
  r.first_token = milliseconds(250);
  r.total = milliseconds(1234);
  r.attempts = {sample_attempt(0), sample_attempt(1)};
  return r;
}

DecisionCall sample_call() {
  DecisionCall c;
  c.task = "next_speaker";
  c.kind = DecisionKind::Choice;
  c.subject = "persona:invented_smith";
  c.question = "Who answers the line just heard?";
  c.state = {{"heard_line", "Has anyone seen the lantern-bearer?"}, {"place", "a shared hall"}};
  c.options = {{"A", "a weary smith"}, {"B", "a wandering scholar"}, {"none", "silence"}};
  c.rule = "B";
  c.messages = {Message::system("Rendered by the template engine.", true), Message::user("Pick A, B or none.")};
  c.slot = "decisions_custom";
  return c;
}

DecisionPick sample_pick() {
  DecisionPick p;
  p.key = "A";
  p.by = DecidedBy::Model;
  p.read = ReadAs::Schema;
  p.probabilities = {{"A", 0.75}, {"none", 0.25}};
  p.fallback = true;
  p.miss = true;
  p.error = ErrorClass::FirstTokenTimeout;
  p.latency = milliseconds(1500);
  p.prompt_hash = "0123456789abcdef";
  p.provider = "invented-cloud-0";
  p.model = "invented-model";
  p.task = "next_speaker";
  p.subject = "persona:invented_smith";
  p.option_keys = {"A", "B", "none"};
  return p;
}

}  // namespace

TEST_CASE("every enum's names travel both ways") {
  for (const Role v : {Role::System, Role::User, Role::Assistant}) round_trip(v);
  for (const Purpose v : {Purpose::Reply, Purpose::OwnInitiative, Purpose::Decision, Purpose::Background}) round_trip(v);
  for (int i = 0; i <= static_cast<int>(ErrorClass::Dropped); ++i) round_trip(static_cast<ErrorClass>(i));
  for (const DecisionKind v : {DecisionKind::Choice, DecisionKind::YesNo}) round_trip(v);
  for (const DecidedBy v : {DecidedBy::Model, DecidedBy::Rules}) round_trip(v);
  for (const ReadAs v : {ReadAs::Schema, ReadAs::Meaning, ReadAs::YesNo, ReadAs::Rules}) round_trip(v);
  for (const RequestKind v : {RequestKind::Chat, RequestKind::Decision}) round_trip(v);
  for (const EventKind v : {EventKind::Started, EventKind::Delta, EventKind::Reset, EventKind::Ended,
                            EventKind::WarmUpEnded}) {
    round_trip(v);
  }
  CHECK(json(Purpose::OwnInitiative) == "own_initiative");
  CHECK(json(ErrorClass::GateClosed) == "GateClosed");
  CHECK(json(DecisionKind::YesNo) == "yes_no");
}

TEST_CASE("messages, chat requests and their results round-trip, every field included") {
  round_trip(Part{"An invented line.", true});
  round_trip(sample_message());
  round_trip(sample_chat());
  round_trip(Usage{12, 34, 5});
  round_trip(TopLogprob{" B", -1.75});
  round_trip(TokenLogprob{"B", -0.5, {{"B", -0.5}, {"A", -1.25}}});
  round_trip(sample_attempt(0));
  round_trip(sample_result());

  // The form itself: names in snake_case, durations in milliseconds, an absent optional as null.
  const json chat = sample_chat();
  CHECK(chat["purpose"] == "background");
  CHECK(chat["messages"][0]["parts"][0]["cache_break"] == true);
  CHECK(json(ChatRequest{})["max_tokens"].is_null());
  CHECK(json(sample_result())["attempts"][0]["first_token_ms"] == -1);
  CHECK(json(sample_result())["total_ms"] == 1234);
}

TEST_CASE("decision calls and picks round-trip; the rules fallback is the key the rules picked") {
  round_trip(DecisionOption{"A", "a weary smith"});
  round_trip(DecisionRecord{"place", "a shared hall"});
  round_trip(sample_call());
  DecisionCall yes_no = sample_call();
  yes_no.kind = DecisionKind::YesNo;
  yes_no.subject.reset();
  yes_no.options.clear();
  yes_no.rule = "no";
  round_trip(yes_no);
  round_trip(KeyProbability{"none", 0.125});
  round_trip(sample_pick());
  CHECK(json(sample_call())["rule"] == "B");
  CHECK(json(yes_no)["subject"].is_null());
}

TEST_CASE("the handle's requests and events round-trip, with ids past 2^53") {
  round_trip(LlmRequest::chat(kBigId, sample_chat()));
  round_trip(LlmRequest::decision(kBigId - 1, sample_call(), kBigEpoch));
  const json request = LlmRequest::decision(kBigId, sample_call(), kBigEpoch);
  CHECK(request["id"].get<RequestId>() == kBigId);
  CHECK(request.dump().find(std::to_string(kBigId)) != std::string::npos);
  CHECK(request.contains("decision"));
  CHECK_FALSE(request.contains("chat"));

  round_trip(WarmUpOutcome{true, 200, milliseconds(4200), ErrorClass::None});
  round_trip(WarmUpOutcome{false, 404, milliseconds(12), ErrorClass::BadRequest});

  LlmEvent started;
  started.kind = EventKind::Started;
  started.id = kBigId;
  started.provider = "invented-cloud-0";
  started.model = "invented-model";
  round_trip(started);
  LlmEvent delta;
  delta.id = 3;
  delta.text = "A piece of an invented reply, with UTF-8: caf\xC3\xA9 \xE2\x80\x94 \xE2\x80\x9Cquoted\xE2\x80\x9D.";
  round_trip(delta);
  LlmEvent reset;
  reset.kind = EventKind::Reset;
  reset.id = 3;
  round_trip(reset);
  LlmEvent ended_chat;
  ended_chat.kind = EventKind::Ended;
  ended_chat.id = 3;
  ended_chat.chat = sample_result();
  round_trip(ended_chat);
  LlmEvent ended_pick;
  ended_pick.kind = EventKind::Ended;
  ended_pick.id = 4;
  ended_pick.pick = sample_pick();
  round_trip(ended_pick);
  LlmEvent warmed;
  warmed.kind = EventKind::WarmUpEnded;
  warmed.id = 1;
  warmed.provider = "invented-local";
  warmed.model = "invented-local-model";
  warmed.warm_up = WarmUpOutcome{true, 200, milliseconds(800), ErrorClass::None};
  round_trip(warmed);
  CHECK(ended_pick.error() == ErrorClass::FirstTokenTimeout);
  CHECK(json(delta)["chat"].is_null());
}

TEST_CASE("a field left out keeps its default; defaults round-trip too") {
  round_trip(Part{});
  round_trip(Message{});
  round_trip(ChatRequest{});
  round_trip(ChatResult{});
  round_trip(DecisionCall{});
  round_trip(DecisionPick{});
  round_trip(LlmEvent{});
  round_trip(LlmRequest{});
  CHECK(json::object().get<ChatRequest>() == ChatRequest{});
  CHECK(json::object().get<ChatRequest>().slot == "dialogue");
  CHECK(json::object().get<DecisionCall>().slot == "decisions");
  CHECK(json::object().get<ChatResult>().first_token == milliseconds(-1));
  CHECK(json::object().get<Usage>().prompt_tokens == -1);
  const auto partial = json{{"kind", "Delta"}, {"id", 9}, {"text", "Only these."}}.get<LlmEvent>();
  CHECK(partial.text == "Only these.");
  CHECK_FALSE(partial.chat.has_value());
  CHECK(partial.warm_up == WarmUpOutcome{});
}

TEST_CASE("a request may name a reserved slot, and its JSON form keeps the name") {
  // The names are en-proto's, beside the slots in use (O88); where such a request goes, for now dialogue, is en-llm's.
  ChatRequest request;
  request.slot = std::string(kActions);
  round_trip(request);
  CHECK(json(request)["slot"] == "actions");
  DecisionCall call;
  call.slot = std::string(kVision);
  round_trip(call);
  CHECK(json(call)["slot"] == "vision");
}

TEST_CASE("malformed JSON is refused, naming the path and the problem") {
  CHECK(refusal<Message>(json::array()) == "Message: must be an object");
  CHECK(refusal<Message>(json{{"role", "narrator"}}) == "Message.role: must be one of system, user, assistant");
  CHECK(refusal<Message>(json{{"role", 1}}) == "Message.role: must be one of system, user, assistant");
  CHECK(refusal<Message>(json{{"parts", json::object()}}) == "Message.parts: must be an array");
  CHECK(refusal<Message>(json{{"parts", {{{"text", 5}}}}}) == "Message.parts[0].text: must be a string");
  CHECK(refusal<Part>(json{{"text", "x"}, {"cache", true}}) == "Part: has an unknown field \"cache\"");
  CHECK(refusal<Part>(json{{std::string(80, 'k'), 1}}) == "Part: has an unknown field with a long or unprintable name");
  CHECK(refusal<Part>(json{{"cache_break", "yes"}}) == "Part.cache_break: must be true or false");
  CHECK(refusal<ChatRequest>(json{{"max_tokens", 1.5}}) == "ChatRequest.max_tokens: must be a whole number");
  CHECK(refusal<ChatRequest>(json{{"max_tokens", 1LL << 40}}) == "ChatRequest.max_tokens: is out of range");
  CHECK(refusal<ChatRequest>(json{{"temperature", "warm"}}) == "ChatRequest.temperature: must be a number");
  CHECK(refusal<ChatRequest>(json{{"purpose", "own"}}) ==
        "ChatRequest.purpose: must be one of reply, own_initiative, decision, background");
  CHECK(refusal<ChatRequest>(json{{"slot", nullptr}}) == "ChatRequest.slot: must be a string");
  CHECK(refusal<LlmEvent>(json{{"id", -1}}) == "LlmEvent.id: must be a whole number of 0 or more");
  CHECK(refusal<LlmEvent>(json{{"chat", {{"attempts", {json::object(), {{"error", "Teapot"}}}}}}}).rfind(
            "LlmEvent.chat.attempts[1].error: must be one of None, FirstTokenTimeout, ", 0) == 0);
  CHECK(refusal<LlmEvent>(json{{"warm_up", {{"time_ms", "soon"}}}}) ==
        "LlmEvent.warm_up.time_ms: must be a whole number of milliseconds");
  CHECK(refusal<LlmRequest>(json{{"id", 1}}) == "LlmRequest: needs exactly one of \"chat\" and \"decision\"");
  CHECK(refusal<LlmRequest>(json{{"id", 1}, {"chat", json::object()}, {"decision", json::object()}}) ==
        "LlmRequest: needs exactly one of \"chat\" and \"decision\"");
  CHECK(refusal<LlmRequest>(json{{"id", 1}, {"decision", {{"options", {{{"key", "A"}, {"txt", "x"}}}}}}}) ==
        "LlmRequest.decision.options[0]: has an unknown field \"txt\"");
  CHECK(refusal<DecisionPick>(json{{"by", "oracle"}}) == "DecisionPick.by: must be one of model, rules");

  // A value is never quoted: a refused field that held text names only where it was.
  const std::string secret_like = "an-invented-value-that-must-not-be-echoed";
  CHECK(refusal<ChatResult>(json{{"usage", secret_like}}).find(secret_like) == std::string::npos);
  CHECK(refusal<ChatResult>(json{{"error", secret_like}}).find(secret_like) == std::string::npos);

  // From text: past the caps or not JSON at all, before any field is read.
  CHECK_THROWS_WITH_AS(read_json<LlmEvent>("{\"kind\": "), "the text is not valid JSON", ProtoError);
  CHECK_THROWS_AS(read_json<LlmEvent>(R"({"kind": "Delta", "text": 5})"), ProtoError);
}
