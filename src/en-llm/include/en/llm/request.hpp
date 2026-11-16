#pragma once
// The chat-completion bodies and HTTP requests the client sends (ARCH §11.1, §11.2).
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/llm/config.hpp"
#include "en/llm/message.hpp"
#include "en/llm/transport.hpp"

namespace en::llm {

// Full sends every optional field. Stripped is the retry after a BadRequest: no json_schema, no stream_options, no
// logprobs and none of extra_body but the reasoning switch, and still the quantisation floor (ARCH §11.2; WHY§98).
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
  BodyMode mode = BodyMode::Full;
  bool minimal_effort = false;                  // the model refused reasoning off: ask for the lowest effort instead
};

nlohmann::json build_chat_body(const BodyInputs& in);

// Messages as the provider takes cache marks: content parts with cache_control where it takes breakpoints (at most
// four, the earliest kept), otherwise each message's parts joined into one string.
nlohmann::json render_messages(const std::vector<Message>& messages, CacheMarks marks);

// POST base_url/chat/completions with the body; the key goes into a secret header that only the transport reads.
HttpRequest build_chat_request(const ProviderConfig& provider, const nlohmann::json& body);

// Whether a top-level extra_body key carries the reasoning switch, which no retry drops.
bool is_reasoning_key(std::string_view key);

// The lowest reasoning effort the flavour offers: "minimal" on OpenAI-style providers, "low" elsewhere.
const char* minimal_effort_level(Flavor flavor);

// Rewrites the reasoning switch in a body to that lowest effort, in the form the switch was written in.
void apply_minimal_effort(nlohmann::json& body, Flavor flavor);

}  // namespace en::llm
