#include "en/llm/request.hpp"

#include <array>

namespace en::llm {

using nlohmann::json;

namespace {

// The keys providers switch reasoning with: OpenRouter's reasoning object, OpenAI's and Ollama's reasoning_effort,
// Ollama's think, llama.cpp's and vLLM's chat_template_kwargs.enable_thinking, and the budget forms.
constexpr std::array<std::string_view, 8> kReasoningKeys = {"reasoning",         "reasoning_effort", "think",
                                                            "thinking",          "enable_thinking",  "chat_template_kwargs",
                                                            "reasoning_budget",  "reasoning_format"};

constexpr int kMaxBreakpoints = 4;  // what providers with explicit cache control accept

void merge_into(json& target, const json& source) {
  for (auto it = source.begin(); it != source.end(); ++it) {
    auto existing = target.find(it.key());
    if (it->is_object() && existing != target.end() && existing->is_object()) {
      merge_into(*existing, *it);
    } else {
      target[it.key()] = *it;
    }
  }
}

// What a stripped retry keeps of extra_body: the reasoning switch, and a floor written there by hand.
json reasoning_only(const json& extra) {
  json kept = json::object();
  for (auto it = extra.begin(); it != extra.end(); ++it) {
    if (is_reasoning_key(it.key())) kept[it.key()] = *it;
  }
  if (auto provider = extra.find("provider"); provider != extra.end() && provider->is_object()) {
    if (auto floor = provider->find("quantizations"); floor != provider->end()) kept["provider"]["quantizations"] = *floor;
  }
  return kept;
}

}  // namespace

bool is_reasoning_key(std::string_view key) {
  for (std::string_view k : kReasoningKeys) {
    if (k == key) return true;
  }
  return false;
}

const char* minimal_effort_level(Flavor flavor) { return flavor == Flavor::OpenAI ? "minimal" : "low"; }

void apply_minimal_effort(json& body, Flavor flavor) {
  const std::string level = minimal_effort_level(flavor);
  if (auto reasoning = body.find("reasoning"); reasoning != body.end() && reasoning->is_object()) {
    json effort = {{"effort", level}};
    if (auto exclude = reasoning->find("exclude"); exclude != reasoning->end()) effort["exclude"] = *exclude;
    *reasoning = std::move(effort);
    return;
  }
  if (body.contains("reasoning_effort")) {
    body["reasoning_effort"] = level;
    return;
  }
  if (body.contains("think")) {  // Ollama's think takes a level as well as a boolean
    body["think"] = level;
    return;
  }
  // The off switches with no level of their own are lifted, and the effort is asked for the common way.
  if (auto kwargs = body.find("chat_template_kwargs"); kwargs != body.end() && kwargs->is_object()) {
    kwargs->erase("enable_thinking");
    if (kwargs->empty()) body.erase("chat_template_kwargs");
  }
  body.erase("enable_thinking");
  body.erase("thinking");
  body.erase("reasoning_budget");
  body["reasoning_effort"] = level;
}

json render_messages(const std::vector<Message>& messages, CacheMarks marks) {
  json list = json::array();
  int breakpoints_left = kMaxBreakpoints;
  for (Message message : messages) {
    normalise(message);
    json entry = {{"role", to_string(message.role)}};
    if (marks == CacheMarks::Breakpoints && message.has_cache_break() && breakpoints_left > 0) {
      json parts = json::array();
      for (std::size_t i = 0; i < message.parts.size(); ++i) {
        // Parts join exactly (Message::joined), so the model reads the same words as from the joined string.
        json part = {{"type", "text"}, {"text", message.parts[i].text}};
        if (message.parts[i].cache_break && breakpoints_left > 0) {
          part["cache_control"] = {{"type", "ephemeral"}};
          --breakpoints_left;
        }
        parts.push_back(std::move(part));
      }
      entry["content"] = std::move(parts);
    } else {
      entry["content"] = message.joined();
    }
    list.push_back(std::move(entry));
  }
  return list;
}

json build_chat_body(const BodyInputs& in) {
  const ProviderConfig& provider = *in.provider;
  json body = json::object();
  // extra_body first: the client's own fields below always win (and load refuses extra_body keys that set them).
  merge_into(body, in.mode == BodyMode::Full ? provider.extra_body : reasoning_only(provider.extra_body));
  if (in.minimal_effort) apply_minimal_effort(body, provider.flavor);
  // A local model's quantisation is the file the player loaded (ARCH §11.1), so the floor is for cloud providers.
  if (provider.quant_floor && provider.kind == ProviderKind::Cloud) {
    json& route = body["provider"];
    if (!route.is_object()) route = json::object();
    route["quantizations"] = quantizations_from(*provider.quant_floor);
  }
  body["model"] = in.model;
  body["messages"] = render_messages(*in.messages, provider.cache_marks);
  body["max_tokens"] = in.max_tokens;
  body["temperature"] = in.temperature;
  body["stream"] = in.stream;
  if (provider.cache_marks == CacheMarks::CachePrompt) body["cache_prompt"] = true;
  if (in.mode == BodyMode::Full) {
    if (in.stream) body["stream_options"] = {{"include_usage", true}};
    if (in.json_schema != nullptr) {
      body["response_format"] = {{"type", "json_schema"},
                                 {"json_schema", {{"name", "answer"}, {"strict", true}, {"schema", *in.json_schema}}}};
    }
    if (in.top_logprobs > 0) {
      body["logprobs"] = true;
      body["top_logprobs"] = in.top_logprobs;
    }
  }
  return body;
}

HttpRequest build_chat_request(const ProviderConfig& provider, const json& body) {
  HttpRequest request;
  request.url = provider.base_url + "/chat/completions";
  const bool stream = body.value("stream", false);
  request.headers.push_back(HttpHeader{"Content-Type", "application/json"});
  request.headers.push_back(HttpHeader{"Accept", stream ? "text/event-stream" : "application/json"});
  for (const HttpHeader& header : provider.headers) request.headers.push_back(header);
  if (provider.key) request.secret_headers.emplace_back("Authorization", Secret("Bearer " + provider.key->reveal()));
  for (const auto& [name, value] : provider.secret_headers) request.secret_headers.emplace_back(name, value);
  // Invalid UTF-8 (a cut stream in the history, say) becomes U+FFFD rather than failing the request.
  request.body = body.dump(-1, ' ', false, json::error_handler_t::replace);
  return request;
}

}  // namespace en::llm
