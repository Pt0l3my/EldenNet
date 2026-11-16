#pragma once
// Chat messages as en-prompt hands them over (NPC §7.1's markers), with their cache marks. The types are en-proto's
// (llm.hpp), shared with the core; this header keeps their en-llm names and adds the reader of the tools' files.
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/proto/llm.hpp"

namespace en::llm {

using en::proto::Message;
using en::proto::normalise;
using en::proto::Part;
using en::proto::Role;
using en::proto::to_string;

// Reads messages from JSON. Accepted shapes: an array of messages, or an object with a "messages" array. A message
// is {"role": "system" | "developer" | "user" | "assistant", "content": text or parts}, optionally with a mark for
// its end: "cache": "A" (any label), "cache": true, or "cache_break": true. Parts are {"text": ...} or
// {"type": "text", "text": ...}, each optionally marked the same way or with an OpenAI-style "cache_control", and
// they join exactly. en-prompt's own form (render-prompt --json) also has "parts": [{"text", "cache"}], which then
// hold the message and its marks, and must join to its "content".
// Throws std::invalid_argument naming the message index; it never quotes the content.
std::vector<Message> parse_messages(const nlohmann::json& document);

}  // namespace en::llm
