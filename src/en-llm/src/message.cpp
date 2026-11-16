#include "en/llm/message.hpp"

#include <stdexcept>

namespace en::llm {

using nlohmann::json;

namespace {

[[noreturn]] void bad(std::size_t index, const std::string& what) {
  throw std::invalid_argument("message " + std::to_string(index) + ": " + what);
}

Role parse_role(const json& value, std::size_t index) {
  if (!value.is_string()) bad(index, "\"role\" must be a string");
  const auto role = value.get<std::string>();
  if (role == "system" || role == "developer") return Role::System;
  if (role == "user") return Role::User;
  if (role == "assistant") return Role::Assistant;
  bad(index, "unknown role (expected system, user or assistant)");
}

// A mark is any non-empty cache label, true, or an OpenAI-style cache_control object.
bool marked(const json& object) {
  if (auto it = object.find("cache"); it != object.end()) {
    if (it->is_boolean()) return it->get<bool>();
    if (it->is_string()) return !it->get<std::string>().empty();
  }
  if (auto it = object.find("cache_break"); it != object.end() && it->is_boolean()) return it->get<bool>();
  if (auto it = object.find("cache_control"); it != object.end()) return it->is_object();
  return false;
}

Part parse_part(const json& value, std::size_t index) {
  if (value.is_string()) return Part{value.get<std::string>(), false};
  if (!value.is_object()) bad(index, "a content part must be text or an object");
  auto text = value.find("text");
  if (text == value.end() || !text->is_string()) bad(index, "a content part needs a \"text\" string");
  if (auto type = value.find("type"); type != value.end() && (!type->is_string() || type->get<std::string>() != "text")) {
    bad(index, "only text parts are supported");
  }
  return Part{text->get<std::string>(), marked(value)};
}

Message parse_message(const json& value, std::size_t index) {
  if (!value.is_object()) bad(index, "must be an object");
  auto role = value.find("role");
  if (role == value.end()) bad(index, "needs a \"role\"");
  Message message;
  message.role = parse_role(*role, index);
  auto content = value.find("content");
  if (content == value.end()) bad(index, "needs a \"content\"");
  if (content->is_string()) {
    message.parts.push_back(Part{content->get<std::string>(), false});
  } else if (content->is_array()) {
    for (const json& part : *content) message.parts.push_back(parse_part(part, index));
  } else {
    bad(index, "\"content\" must be text or an array of parts");
  }
  if (auto parts = value.find("parts"); parts != value.end()) {
    // en-prompt's own form: its parts carry the breakpoints inside a merged message, which "content" cannot.
    if (!parts->is_array()) bad(index, "\"parts\" must be an array");
    std::vector<Part> listed;
    for (const json& part : *parts) listed.push_back(parse_part(part, index));
    std::string text;
    for (const Part& part : listed) text.append(part.text);
    if (text != message.joined()) bad(index, "its \"parts\" do not join to its \"content\"");
    message.parts = std::move(listed);
  }
  if (marked(value) && !message.parts.empty()) message.parts.back().cache_break = true;
  normalise(message);
  return message;
}

}  // namespace

std::vector<Message> parse_messages(const json& document) {
  const json* list = &document;
  if (document.is_object()) {
    auto it = document.find("messages");
    if (it == document.end()) throw std::invalid_argument("expected an array of messages or {\"messages\": [...]}");
    list = &*it;
  }
  if (!list->is_array()) throw std::invalid_argument("\"messages\" must be an array");
  std::vector<Message> messages;
  messages.reserve(list->size());
  for (std::size_t i = 0; i < list->size(); ++i) messages.push_back(parse_message((*list)[i], i));
  if (messages.empty()) throw std::invalid_argument("there are no messages");
  return messages;
}

}  // namespace en::llm
