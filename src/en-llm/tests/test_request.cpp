// Request bodies, cache marks and HTTP requests (ARCH §11.1, §11.2; NPC §7.1).
#include <doctest.h>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/llm/message.hpp"
#include "en/llm/request.hpp"
#include "rig.hpp"

using namespace en::llm;
using nlohmann::json;
using en::llm::test::make_config;

namespace {

const std::vector<Message> kMessages = {
    Message{Role::System, {Part{"World frame.", true}, Part{"\n\nIdentity.", false}, Part{"\n\nVoice.", true}}},
    Message::user("A question."),
};

json body_for(const ProviderConfig& p, BodyMode mode = BodyMode::Full, const json* schema = nullptr, int top = 0,
              bool minimal = false) {
  BodyInputs in;
  in.provider = &p;
  in.model = "model-a";
  in.messages = &kMessages;
  in.max_tokens = 140;
  in.temperature = 0.8;
  in.json_schema = schema;
  in.top_logprobs = top;
  in.mode = mode;
  in.minimal_effort = minimal;
  return build_chat_body(in);
}

}  // namespace

TEST_CASE("a full body: the client's fields, extra_body merged, the floor in the flavour's own field") {
  const Config cfg = make_config(en::llm::test::kCloud);
  const ProviderConfig& p = cfg.providers[0];
  const json schema = {{"type", "object"}};
  const json body = body_for(p, BodyMode::Full, &schema, 5);
  CHECK(body["model"] == "model-a");
  CHECK(body["max_tokens"] == 140);
  CHECK(body["temperature"].get<double>() == doctest::Approx(0.8));
  CHECK(body["stream"] == true);
  CHECK(body["stream_options"]["include_usage"] == true);
  CHECK(body["reasoning"]["enabled"] == false);
  CHECK(body["provider"]["sort"] == "latency");
  CHECK(body["provider"]["quantizations"] == json({"fp8", "fp16", "bf16", "fp32"}));
  CHECK(body["response_format"]["type"] == "json_schema");
  CHECK(body["response_format"]["json_schema"]["strict"] == true);
  CHECK(body["response_format"]["json_schema"]["schema"] == schema);
  CHECK(body["logprobs"] == true);
  CHECK(body["top_logprobs"] == 5);
  CHECK_FALSE(body.contains("cache_prompt"));
  // No explicit cache control here: each message's parts are joined into one string.
  CHECK(body["messages"][0]["content"] == "World frame.\n\nIdentity.\n\nVoice.");
  CHECK(body["messages"][1]["role"] == "user");
}

TEST_CASE("a stripped body keeps the reasoning switch and the floor, and drops the rest") {
  const Config cfg = make_config(en::llm::test::kCloud);
  const json schema = {{"type", "object"}};
  const json body = body_for(cfg.providers[0], BodyMode::Stripped, &schema, 5);
  CHECK(body["reasoning"]["enabled"] == false);
  CHECK(body["provider"]["quantizations"].size() == 4);
  CHECK_FALSE(body["provider"].contains("sort"));
  CHECK_FALSE(body.contains("stream_options"));
  CHECK_FALSE(body.contains("response_format"));
  CHECK_FALSE(body.contains("logprobs"));
  CHECK_FALSE(body.contains("top_logprobs"));
  CHECK(body["max_tokens"] == 140);

  const Config local = make_config(en::llm::test::kLocal);
  const json local_body = body_for(local.providers[0], BodyMode::Stripped);
  CHECK(local_body["chat_template_kwargs"]["enable_thinking"] == false);
  CHECK(local_body["cache_prompt"] == true);
}

TEST_CASE("stop sequences: every flavour takes them in a full body, at most four and none empty; a stripped body none") {
  // A dialogue's two (NPC §7.3), in invented words.
  const std::vector<std::string> two = {"\nThe Wanderer:", "\nWanderer:"};
  const auto body_with = [](const ProviderConfig& p, const std::vector<std::string>* stop, BodyMode mode) {
    BodyInputs in;
    in.provider = &p;
    in.model = "model-a";
    in.messages = &kMessages;
    in.stop = stop;
    in.mode = mode;
    return build_chat_body(in);
  };
  // Each flavour's chat completions take `stop`: OpenAI's API (and OpenRouter's, the cloud preset's), and the
  // OpenAI-compatible endpoints of Ollama, llama.cpp, LM Studio and KoboldCpp. None is left out.
  for (const char* providers : {en::llm::test::kCloud, en::llm::test::kLocal}) {
    for (const Flavor flavor : {Flavor::OpenAI, Flavor::Ollama, Flavor::LlamaCpp, Flavor::LmStudio, Flavor::KoboldCpp}) {
      Config cfg = make_config(providers);
      cfg.providers[0].flavor = flavor;
      INFO(to_string(flavor) << " " << to_string(cfg.providers[0].kind));
      CHECK(body_with(cfg.providers[0], &two, BodyMode::Full)["stop"] == json(two));
      // The stripped retry after a 400 sends none: a model that refuses the field (OpenAI's reasoning models) still
      // answers, and the sanitizer cuts the same run-on (NPC §8.2, step 3).
      CHECK_FALSE(body_with(cfg.providers[0], &two, BodyMode::Stripped).contains("stop"));
    }
  }
  const Config cfg = make_config(en::llm::test::kCloud);
  const ProviderConfig& p = cfg.providers[0];
  CHECK_FALSE(body_with(p, nullptr, BodyMode::Full).contains("stop"));
  const std::vector<std::string> none;
  CHECK_FALSE(body_with(p, &none, BodyMode::Full).contains("stop"));
  const std::vector<std::string> empties = {"", ""};
  CHECK_FALSE(body_with(p, &empties, BodyMode::Full).contains("stop"));
  const std::vector<std::string> six = {"a", "", "b", "c", "d", "e"};
  CHECK(body_with(p, &six, BodyMode::Full)["stop"] == json({"a", "b", "c", "d"}));
  CHECK(kMaxStops == 4);
}

TEST_CASE("a local provider gets no quantisation floor") {
  Config cfg = make_config(en::llm::test::kLocal);
  cfg.providers[0].quant_floor = "fp8";
  CHECK_FALSE(body_for(cfg.providers[0]).contains("provider"));
}

TEST_CASE("the lowest reasoning effort, in the form the switch was written in") {
  json openrouter = {{"reasoning", {{"enabled", false}, {"exclude", true}}}};
  apply_minimal_effort(openrouter, Flavor::OpenAI);
  CHECK(openrouter["reasoning"] == json({{"effort", "minimal"}, {"exclude", true}}));

  json effort = {{"reasoning_effort", "none"}};
  apply_minimal_effort(effort, Flavor::Ollama);
  CHECK(effort["reasoning_effort"] == "low");

  json think = {{"think", false}};
  apply_minimal_effort(think, Flavor::Ollama);
  CHECK(think["think"] == "low");

  json kwargs = {{"chat_template_kwargs", {{"enable_thinking", false}}}};
  apply_minimal_effort(kwargs, Flavor::LlamaCpp);
  CHECK_FALSE(kwargs.contains("chat_template_kwargs"));
  CHECK(kwargs["reasoning_effort"] == "low");

  json none = json::object();
  apply_minimal_effort(none, Flavor::OpenAI);
  CHECK(none["reasoning_effort"] == "minimal");

  const Config cfg = make_config(en::llm::test::kCloud);
  const json body = body_for(cfg.providers[0], BodyMode::Full, nullptr, 0, /*minimal=*/true);
  CHECK(body["reasoning"] == json({{"effort", "minimal"}}));
  CHECK(body["provider"]["sort"] == "latency");  // only the switch changes
}

TEST_CASE("cache marks: breakpoints as content parts, llama.cpp's flag, or nothing but the order") {
  const json breakpoints = render_messages(kMessages, CacheMarks::Breakpoints);
  const json& parts = breakpoints[0]["content"];
  REQUIRE(parts.is_array());
  REQUIRE(parts.size() == 3);
  CHECK(parts[0]["text"] == "World frame.");  // the parts as they are: their blank lines travel with them
  CHECK(parts[0]["cache_control"]["type"] == "ephemeral");
  CHECK(parts[1]["text"] == "\n\nIdentity.");
  CHECK_FALSE(parts[1].contains("cache_control"));
  CHECK(parts[2]["text"] == "\n\nVoice.");
  CHECK(parts[2].contains("cache_control"));
  CHECK(breakpoints[1]["content"] == "A question.");  // an unmarked message stays a string

  // At most four breakpoints, the earliest kept.
  std::vector<Message> many;
  for (int i = 0; i < 6; ++i) many.push_back(Message::system("block " + std::to_string(i), true));
  const json capped = render_messages(many, CacheMarks::Breakpoints);
  int marks = 0;
  for (const auto& m : capped) marks += m["content"].is_array() && m["content"][0].contains("cache_control") ? 1 : 0;
  CHECK(marks == 4);
  CHECK(capped[5]["content"] == "block 5");

  const json plain = render_messages(kMessages, CacheMarks::CachePrompt);
  CHECK(plain[0]["content"] == "World frame.\n\nIdentity.\n\nVoice.");

  // An empty part disappears, and its mark moves to the part before it.
  Message m{Role::System, {Part{"Kept.", false}, Part{"", true}}};
  normalise(m);
  REQUIRE(m.parts.size() == 1);
  CHECK(m.parts[0].cache_break);
}

TEST_CASE("the HTTP request: the URL, the headers, and the key only in a secret header") {
  const Config cfg = make_config(en::llm::test::kCloud);
  const HttpRequest r = build_chat_request(cfg.providers[0], json{{"stream", true}, {"note", "bad \xFF byte"}});
  CHECK(r.url == "https://llm.example.invalid/api/v1/chat/completions");
  CHECK(r.method == "POST");
  bool accept = false;
  for (const auto& h : r.headers) {
    CHECK(h.value.find("invented-test-key") == std::string::npos);
    accept = accept || (h.name == "Accept" && h.value == "text/event-stream");
  }
  CHECK(accept);
  REQUIRE(r.secret_headers.size() == 1);
  CHECK(r.secret_headers[0].first == "Authorization");
  CHECK(r.secret_headers[0].second.reveal() == "Bearer invented-test-key");
  CHECK(json::parse(r.body)["note"] == "bad \xEF\xBF\xBD byte");  // invalid UTF-8 replaced, not fatal

  const Config local = make_config(en::llm::test::kLocal);
  CHECK(build_chat_request(local.providers[0], json::object()).secret_headers.empty());  // no key, no header
}

TEST_CASE("messages from JSON: shapes, roles and cache marks") {
  const json doc = json::parse(R"({"messages": [
      {"role": "system", "content": "Frame.", "cache": "A"},
      {"role": "developer", "content": [{"text": "Identity."}, {"type": "text", "text": "Voice.", "cache_control": {"type": "ephemeral"}}]},
      {"role": "user", "content": "Hello.", "cache_break": false},
      {"role": "assistant", "content": [{"text": "Hm.", "cache": true}]}
  ]})");
  const auto messages = parse_messages(doc);
  REQUIRE(messages.size() == 4);
  CHECK(messages[0].role == Role::System);
  CHECK(messages[0].parts[0].cache_break);
  CHECK(messages[1].role == Role::System);
  CHECK_FALSE(messages[1].parts[0].cache_break);
  CHECK(messages[1].parts[1].cache_break);
  CHECK_FALSE(messages[2].has_cache_break());
  CHECK(messages[3].parts[0].cache_break);
  CHECK(parse_messages(json::parse(R"([{"role": "user", "content": "Only."}])")).size() == 1);

  auto error = [](const char* text) -> std::string {
    try {
      parse_messages(json::parse(text));
    } catch (const std::invalid_argument& e) {
      return e.what();
    }
    return "";
  };
  CHECK(error(R"([{"role": "narrator", "content": "secret words"}])").find("unknown role") != std::string::npos);
  CHECK(error(R"([{"role": "narrator", "content": "secret words"}])").find("secret words") == std::string::npos);
  CHECK(error(R"([{"role": "user"}])").find("message 0") != std::string::npos);
  CHECK(error(R"([{"role": "user", "content": [{"type": "image_url"}]}])").find("text") != std::string::npos);
  CHECK(error(R"([])").find("no messages") != std::string::npos);
  CHECK(error(R"({"prompt": "x"})").find("messages") != std::string::npos);
}

TEST_CASE("messages in en-prompt's own form keep the breakpoints inside a merged message") {
  // As render-prompt --json writes it: two system parts merged with a blank line, each with its breakpoint.
  const json doc = json::parse(R"({"template": "dialogue", "cache_breakpoints": 2, "messages": [
      {"role": "system", "content": "Frame.\n\nIdentity.", "cache": true,
       "parts": [{"text": "Frame.", "cache": true}, {"text": "\n\nIdentity.", "cache": true}]},
      {"role": "user", "content": "Hello.", "cache": false, "parts": [{"text": "Hello.", "cache": false}]}
  ]})");
  const auto messages = parse_messages(doc);
  REQUIRE(messages.size() == 2);
  REQUIRE(messages[0].parts.size() == 2);
  CHECK(messages[0].parts[0].cache_break);
  CHECK(messages[0].parts[1].cache_break);
  CHECK(messages[0].joined() == "Frame.\n\nIdentity.");
  const json sent = render_messages(messages, CacheMarks::Breakpoints);
  CHECK(sent[0]["content"][0]["text"] == "Frame.");
  CHECK(sent[0]["content"][1]["text"] == "\n\nIdentity.");
  CHECK(sent[0]["content"][1]["cache_control"]["type"] == "ephemeral");
  CHECK(render_messages(messages, CacheMarks::None)[0]["content"] == "Frame.\n\nIdentity.");

  // Parts that do not join to the content mean the file was edited by hand: refused, not guessed at.
  const json edited = json::parse(R"([{"role": "user", "content": "One.", "parts": [{"text": "Two."}]}])");
  CHECK_THROWS_AS(parse_messages(edited), std::invalid_argument);
}
