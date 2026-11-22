#pragma once
// The chat-completion bodies and HTTP requests the client sends (ARCH §11.1, §11.2).
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/llm/config.hpp"
#include "en/llm/message.hpp"
#include "en/llm/transport.hpp"

namespace en::llm {

// Full sends every optional field. Stripped is the retry after a BadRequest: no json_schema, no stream_options, no
// logprobs, no stop sequences and none of extra_body but the reasoning switch, and still the quantisation floor (ARCH
// §11.2; WHY§98).
enum class BodyMode { Full, Stripped };

struct BodyInputs {
  const ProviderConfig* provider = nullptr;
  std::string model;
  const std::vector<Message>* messages = nullptr;
  int max_tokens = 140;
  double temperature = 0.8;
  bool stream = true;
  const nlohmann::json* json_schema = nullptr;  // only where the provider takes one; the client decides
  int top_logprobs = 0;                         // 0: none
  // The request's stop sequences: sent as `stop`, the first kMaxStops that are not empty, in a full body only. Every
  // flavour takes the field (OpenAI's and OpenRouter's API, Ollama's, llama.cpp's, LM Studio's and KoboldCpp's
  // OpenAI-compatible endpoints); a model that refuses it (OpenAI's reasoning models answer 400) gets the stripped retry.
  const std::vector<std::string>* stop = nullptr;
  BodyMode mode = BodyMode::Full;
  bool minimal_effort = false;                  // the model refused reasoning off: ask for the lowest effort instead
};

// At most four stop sequences: OpenAI's limit, and more than a dialogue's two.
inline constexpr std::size_t kMaxStops = 4;

nlohmann::json build_chat_body(const BodyInputs& in);

// Messages as the provider takes cache marks: content parts with cache_control where it takes breakpoints (at most
// four, the earliest kept), otherwise each message's parts joined into one string.
nlohmann::json render_messages(const std::vector<Message>& messages, CacheMarks marks);

// POST base_url/chat/completions with the body; the key goes into a secret header that only the transport reads.
HttpRequest build_chat_request(const ProviderConfig& provider, const nlohmann::json& body);

// GET base_url/models (ARCH §11.1) with the provider's headers; the key goes into a secret header, as for chat.
HttpRequest build_models_request(const ProviderConfig& provider);

// Whether a top-level extra_body key carries the reasoning switch, which no retry drops.
bool is_reasoning_key(std::string_view key);

// The lowest reasoning effort the flavour offers: "minimal" on OpenAI-style providers, "low" elsewhere.
const char* minimal_effort_level(Flavor flavor);

// Rewrites the reasoning switch in a body to that lowest effort, in the form the switch was written in.
void apply_minimal_effort(nlohmann::json& body, Flavor flavor);

}  // namespace en::llm
