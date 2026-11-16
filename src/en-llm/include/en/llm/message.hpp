#pragma once
// Chat messages as en-prompt hands them over (NPC §7.1's markers), with their cache marks.
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace en::llm {

enum class Role { System, User, Assistant };

const char* to_string(Role role);

// A piece of a message's text. `cache_break` marks a cache breakpoint after it: `[system cache=A]` ends with one,
// and so do block 5 and block 5b (NPC §7.1). Providers with explicit cache control get the marks; the rest get the
// same text, in the same order, and nothing else.
struct Part {
  std::string text;
  bool cache_break = false;
};

struct Message {
  Role role = Role::User;
  std::vector<Part> parts;

  static Message system(std::string text, bool cache_break = false);
  static Message user(std::string text);
  static Message assistant(std::string text);

  // The parts joined exactly as they are. A part that starts a new paragraph carries its own blank line, as
  // en-prompt's parts do (NPC §7.1), so the model reads the same words whether the parts travel apart or joined.
  std::string joined() const;
  bool has_cache_break() const;
};

// Drops empty parts, moving an empty part's mark to the part before it, since providers refuse empty text blocks.
void normalise(Message& message);

// Reads messages from JSON. Accepted shapes: an array of messages, or an object with a "messages" array. A message
// is {"role": "system" | "developer" | "user" | "assistant", "content": text or parts}, optionally with a mark for
// its end: "cache": "A" (any label), "cache": true, or "cache_break": true. Parts are {"text": ...} or
// {"type": "text", "text": ...}, each optionally marked the same way or with an OpenAI-style "cache_control", and
// they join exactly. en-prompt's own form (render-prompt --json) also has "parts": [{"text", "cache"}], which then
// hold the message and its marks, and must join to its "content".
// Throws std::invalid_argument naming the message index; it never quotes the content.
std::vector<Message> parse_messages(const nlohmann::json& document);

}  // namespace en::llm
