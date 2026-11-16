// The caps on reading JSON (json.hpp) and the JSON forms of the model client's messages (llm.hpp).
#include "en/proto/json.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

#include "en/proto/llm.hpp"
#include "json_read.hpp"

namespace en::proto {

using nlohmann::json;

// ---- Reading within the caps ----

namespace {

std::string bytes_text(std::size_t n) {
  constexpr std::size_t kKb = std::size_t{1} << 10;
  constexpr std::size_t kMb = std::size_t{1} << 20;
  if (n >= kMb && n % kMb == 0) return std::to_string(n / kMb) + " MB";
  if (n >= kKb && n % kKb == 0) return std::to_string(n / kKb) + " KB";
  return std::to_string(n) + " bytes";
}

}  // namespace

std::string describe(JsonRefusal refusal, const JsonLimits& limits) {
  switch (refusal) {
    case JsonRefusal::None: return {};
    case JsonRefusal::TooLarge: return "larger than " + bytes_text(limits.max_bytes);
    case JsonRefusal::TooDeep: return "nested deeper than " + std::to_string(limits.max_depth);
    case JsonRefusal::Malformed: return "not valid JSON";
  }
  return "not valid JSON";
}

bool within_depth(std::string_view text, std::size_t max_depth) noexcept {
  std::size_t depth = 0;
  bool in_string = false;
  bool escaped = false;
  for (const char c : text) {
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }
    if (c == '"') {
      in_string = true;
    } else if (c == '[' || c == '{') {
      if (++depth > max_depth) return false;
    } else if ((c == ']' || c == '}') && depth > 0) {
      --depth;  // a stray closer is the parser's to refuse
    }
  }
  return true;
}

ParsedJson parse_json(std::string_view text, const JsonLimits& limits) {
  ParsedJson out;
  if (text.size() > limits.max_bytes) {
    out.refusal = JsonRefusal::TooLarge;
  } else if (!within_depth(text, limits.max_depth)) {
    out.refusal = JsonRefusal::TooDeep;
  } else {
    out.value = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (out.value.is_discarded()) out.refusal = JsonRefusal::Malformed;
  }
  if (!out.ok()) out.value = json(json::value_t::discarded);
  return out;
}

// ---- JSON forms ----

namespace detail {

std::string path_of(const At& at) {
  std::vector<const At*> chain;
  for (const At* node = &at; node != nullptr; node = node->parent) chain.push_back(node);
  std::string out;
  for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
    const At& node = **it;
    if (node.parent == nullptr) {
      out.append(node.name);
    } else if (node.name.empty()) {
      out += "[" + std::to_string(node.index) + "]";
    } else {
      out += ".";
      out.append(node.name);
    }
  }
  return out;
}

void fail(const At& at, const std::string& problem) { throw ProtoError(path_of(at) + ": " + problem); }

std::string shown_name(const std::string& name) {
  const bool plain = name.size() <= 40 && std::all_of(name.begin(), name.end(), [](char c) {
                       return c >= 0x20 && c < 0x7F && c != '"' && c != '\\';
                     });
  return plain ? "\"" + name + "\"" : "with a long or unprintable name";
}

// The model client's messages' readers, declared first so that the templates find each other's.
void read(const json& j, const At& at, Role& out);
void read(const json& j, const At& at, Purpose& out);
void read(const json& j, const At& at, ErrorClass& out);
void read(const json& j, const At& at, DecisionKind& out);
void read(const json& j, const At& at, DecidedBy& out);
void read(const json& j, const At& at, ReadAs& out);
void read(const json& j, const At& at, RequestKind& out);
void read(const json& j, const At& at, EventKind& out);
void read(const json& j, const At& at, Part& out);
void read(const json& j, const At& at, Message& out);
void read(const json& j, const At& at, ChatRequest& out);
void read(const json& j, const At& at, Usage& out);
void read(const json& j, const At& at, TopLogprob& out);
void read(const json& j, const At& at, TokenLogprob& out);
void read(const json& j, const At& at, AttemptInfo& out);
void read(const json& j, const At& at, ChatResult& out);
void read(const json& j, const At& at, DecisionOption& out);
void read(const json& j, const At& at, DecisionRecord& out);
void read(const json& j, const At& at, DecisionCall& out);
void read(const json& j, const At& at, KeyProbability& out);
void read(const json& j, const At& at, DecisionPick& out);
void read(const json& j, const At& at, LlmRequest& out);
void read(const json& j, const At& at, WarmUpOutcome& out);
void read(const json& j, const At& at, LlmEvent& out);

void read(const json& j, const At& at, bool& out) {
  if (!j.is_boolean()) fail(at, "must be true or false");
  out = j.get<bool>();
}

void read(const json& j, const At& at, int& out) {
  constexpr auto kMin = std::numeric_limits<int>::min();
  constexpr auto kMax = std::numeric_limits<int>::max();
  if (j.is_number_unsigned()) {
    const auto value = j.get<std::uint64_t>();
    if (value > static_cast<std::uint64_t>(kMax)) fail(at, "is out of range");
    out = static_cast<int>(value);
    return;
  }
  if (!j.is_number_integer()) fail(at, "must be a whole number");
  const auto value = j.get<std::int64_t>();
  if (value < kMin || value > kMax) fail(at, "is out of range");
  out = static_cast<int>(value);
}

void read(const json& j, const At& at, std::uint64_t& out) {
  if (j.is_number_unsigned()) {
    out = j.get<std::uint64_t>();
    return;
  }
  if (j.is_number_integer() && j.get<std::int64_t>() >= 0) {
    out = static_cast<std::uint64_t>(j.get<std::int64_t>());
    return;
  }
  fail(at, "must be a whole number of 0 or more");
}

void read(const json& j, const At& at, double& out) {
  if (!j.is_number()) fail(at, "must be a number");
  out = j.get<double>();
}

void read(const json& j, const At& at, std::string& out) {
  if (!j.is_string()) fail(at, "must be a string");
  out = j.get<std::string>();
}

void read(const json& j, const At& at, std::chrono::milliseconds& out) {
  std::int64_t value = 0;
  if (j.is_number_unsigned()) {
    const auto u = j.get<std::uint64_t>();
    if (u > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) fail(at, "is out of range");
    value = static_cast<std::int64_t>(u);
  } else if (j.is_number_integer()) {
    value = j.get<std::int64_t>();
  } else {
    fail(at, "must be a whole number of milliseconds");
  }
  out = std::chrono::milliseconds(value);
}

void read(const json& j, const At&, json& out) { out = j; }

constexpr std::array kRoles = {Role::System, Role::User, Role::Assistant};
constexpr std::array kPurposes = {Purpose::Reply, Purpose::OwnInitiative, Purpose::Decision, Purpose::Background};
constexpr std::array kErrorClasses = {
    ErrorClass::None,        ErrorClass::FirstTokenTimeout, ErrorClass::TotalTimeout, ErrorClass::RateLimited,
    ErrorClass::Auth,        ErrorClass::BadRequest,        ErrorClass::Server,       ErrorClass::Empty,
    ErrorClass::Truncated,   ErrorClass::Refused,           ErrorClass::Cancelled,    ErrorClass::Preempted,
    ErrorClass::Busy,        ErrorClass::BudgetSpent,       ErrorClass::NoHealthyProvider,
    ErrorClass::GateClosed,  ErrorClass::Dropped};
constexpr std::array kDecisionKinds = {DecisionKind::Choice, DecisionKind::YesNo};
constexpr std::array kDecidedBy = {DecidedBy::Model, DecidedBy::Rules};
constexpr std::array kReadAs = {ReadAs::Schema, ReadAs::Meaning, ReadAs::YesNo, ReadAs::Rules};
constexpr std::array kRequestKinds = {RequestKind::Chat, RequestKind::Decision};
constexpr std::array kEventKinds = {EventKind::Started, EventKind::Delta, EventKind::Reset, EventKind::Ended,
                                    EventKind::WarmUpEnded};

void read(const json& j, const At& at, Role& out) { read_enum(j, at, kRoles, out); }
void read(const json& j, const At& at, Purpose& out) { read_enum(j, at, kPurposes, out); }
void read(const json& j, const At& at, ErrorClass& out) { read_enum(j, at, kErrorClasses, out); }
void read(const json& j, const At& at, DecisionKind& out) { read_enum(j, at, kDecisionKinds, out); }
void read(const json& j, const At& at, DecidedBy& out) { read_enum(j, at, kDecidedBy, out); }
void read(const json& j, const At& at, ReadAs& out) { read_enum(j, at, kReadAs, out); }
void read(const json& j, const At& at, RequestKind& out) { read_enum(j, at, kRequestKinds, out); }
void read(const json& j, const At& at, EventKind& out) { read_enum(j, at, kEventKinds, out); }

void read(const json& j, const At& at, Part& out) {
  out = Part{};
  Fields(j, at)("text", out.text)("cache_break", out.cache_break).done();
}

void read(const json& j, const At& at, Message& out) {
  out = Message{};
  Fields(j, at)("role", out.role)("parts", out.parts).done();
}

void read(const json& j, const At& at, ChatRequest& out) {
  out = ChatRequest{};
  Fields(j, at)("slot", out.slot)("purpose", out.purpose)("messages", out.messages)("max_tokens", out.max_tokens)(
      "temperature", out.temperature)("json_schema", out.json_schema)("top_logprobs", out.top_logprobs)
      .done();
}

void read(const json& j, const At& at, Usage& out) {
  out = Usage{};
  Fields(j, at)("prompt_tokens", out.prompt_tokens)("completion_tokens", out.completion_tokens)(
      "cached_tokens", out.cached_tokens)
      .done();
}

void read(const json& j, const At& at, TopLogprob& out) {
  out = TopLogprob{};
  Fields(j, at)("token", out.token)("logprob", out.logprob).done();
}

void read(const json& j, const At& at, TokenLogprob& out) {
  out = TokenLogprob{};
  Fields(j, at)("token", out.token)("logprob", out.logprob)("top", out.top).done();
}

void read(const json& j, const At& at, AttemptInfo& out) {
  out = AttemptInfo{};
  Fields(j, at)("provider", out.provider)("model", out.model)("error", out.error)("http_status", out.http_status)(
      "backup", out.backup)("stripped", out.stripped)("minimal_effort", out.minimal_effort)("lost_race", out.lost_race)(
      "sent_at_ms", out.sent_at)("first_token_ms", out.first_token)("ended_at_ms", out.ended_at)("message", out.message)
      .done();
}

void read(const json& j, const At& at, ChatResult& out) {
  out = ChatResult{};
  Fields(j, at)("error", out.error)("message", out.message)("text", out.text)("raw_text", out.raw_text)(
      "finish_reason", out.finish_reason)("provider", out.provider)("model", out.model)("served_model",
                                                                                         out.served_model)(
      "usage", out.usage)("logprobs", out.logprobs)("used_schema", out.used_schema)("used_logprobs", out.used_logprobs)(
      "streamed", out.streamed)("first_token_ms", out.first_token)("total_ms", out.total)("attempts", out.attempts)
      .done();
}

void read(const json& j, const At& at, DecisionOption& out) {
  out = DecisionOption{};
  Fields(j, at)("key", out.key)("text", out.text).done();
}

void read(const json& j, const At& at, DecisionRecord& out) {
  out = DecisionRecord{};
  Fields(j, at)("name", out.name)("text", out.text).done();
}

void read(const json& j, const At& at, DecisionCall& out) {
  out = DecisionCall{};
  Fields(j, at)("task", out.task)("kind", out.kind)("subject", out.subject)("question", out.question)(
      "state", out.state)("options", out.options)("rule", out.rule)("messages", out.messages)("slot", out.slot)
      .done();
}

void read(const json& j, const At& at, KeyProbability& out) {
  out = KeyProbability{};
  Fields(j, at)("key", out.key)("probability", out.probability).done();
}

void read(const json& j, const At& at, DecisionPick& out) {
  out = DecisionPick{};
  Fields(j, at)("key", out.key)("by", out.by)("read", out.read)("probabilities", out.probabilities)(
      "fallback", out.fallback)("miss", out.miss)("error", out.error)("latency_ms", out.latency)(
      "prompt_hash", out.prompt_hash)("provider", out.provider)("model", out.model)("task", out.task)(
      "subject", out.subject)("option_keys", out.option_keys)
      .done();
}

void read(const json& j, const At& at, LlmRequest& out) {
  out = LlmRequest{};
  std::optional<ChatRequest> chat;
  std::optional<DecisionCall> decision;
  Fields(j, at)("id", out.id)("epoch", out.epoch)("chat", chat)("decision", decision).done();
  if (chat.has_value() == decision.has_value()) fail(at, "needs exactly one of \"chat\" and \"decision\"");
  if (chat) {
    out.body = std::move(*chat);
  } else {
    out.body = std::move(*decision);
  }
}

void read(const json& j, const At& at, WarmUpOutcome& out) {
  out = WarmUpOutcome{};
  Fields(j, at)("ok", out.ok)("status", out.status)("time_ms", out.time)("error", out.error).done();
}

void read(const json& j, const At& at, LlmEvent& out) {
  out = LlmEvent{};
  Fields(j, at)("kind", out.kind)("id", out.id)("provider", out.provider)("model", out.model)("text", out.text)(
      "chat", out.chat)("pick", out.pick)("warm_up", out.warm_up)
      .done();
}

}  // namespace detail

namespace {

// Writers: every field, an absent optional as null.
json write(const Part& v);
json write(const Message& v);
json write(const ChatRequest& v);
json write(const Usage& v);
json write(const TopLogprob& v);
json write(const TokenLogprob& v);
json write(const AttemptInfo& v);
json write(const ChatResult& v);
json write(const DecisionOption& v);
json write(const DecisionRecord& v);
json write(const DecisionCall& v);
json write(const KeyProbability& v);
json write(const DecisionPick& v);
json write(const LlmRequest& v);
json write(const WarmUpOutcome& v);
json write(const LlmEvent& v);

template <class T>
json list(const std::vector<T>& items) {
  json out = json::array();
  for (const T& item : items) out.push_back(write(item));
  return out;
}

template <class T>
json or_null(const std::optional<T>& value) {
  return value ? json(*value) : json(nullptr);
}

json write(const Part& v) { return {{"text", v.text}, {"cache_break", v.cache_break}}; }

json write(const Message& v) { return {{"role", to_string(v.role)}, {"parts", list(v.parts)}}; }

json write(const ChatRequest& v) {
  return {{"slot", v.slot},
          {"purpose", to_string(v.purpose)},
          {"messages", list(v.messages)},
          {"max_tokens", or_null(v.max_tokens)},
          {"temperature", or_null(v.temperature)},
          {"json_schema", v.json_schema ? *v.json_schema : json(nullptr)},
          {"top_logprobs", v.top_logprobs}};
}

json write(const Usage& v) {
  return {{"prompt_tokens", v.prompt_tokens},
          {"completion_tokens", v.completion_tokens},
          {"cached_tokens", v.cached_tokens}};
}

json write(const TopLogprob& v) { return {{"token", v.token}, {"logprob", v.logprob}}; }

json write(const TokenLogprob& v) { return {{"token", v.token}, {"logprob", v.logprob}, {"top", list(v.top)}}; }

json write(const AttemptInfo& v) {
  return {{"provider", v.provider},
          {"model", v.model},
          {"error", to_string(v.error)},
          {"http_status", v.http_status},
          {"backup", v.backup},
          {"stripped", v.stripped},
          {"minimal_effort", v.minimal_effort},
          {"lost_race", v.lost_race},
          {"sent_at_ms", v.sent_at.count()},
          {"first_token_ms", v.first_token.count()},
          {"ended_at_ms", v.ended_at.count()},
          {"message", v.message}};
}

json write(const ChatResult& v) {
  return {{"error", to_string(v.error)},
          {"message", v.message},
          {"text", v.text},
          {"raw_text", v.raw_text},
          {"finish_reason", v.finish_reason},
          {"provider", v.provider},
          {"model", v.model},
          {"served_model", v.served_model},
          {"usage", write(v.usage)},
          {"logprobs", list(v.logprobs)},
          {"used_schema", v.used_schema},
          {"used_logprobs", v.used_logprobs},
          {"streamed", v.streamed},
          {"first_token_ms", v.first_token.count()},
          {"total_ms", v.total.count()},
          {"attempts", list(v.attempts)}};
}

json write(const DecisionOption& v) { return {{"key", v.key}, {"text", v.text}}; }

json write(const DecisionRecord& v) { return {{"name", v.name}, {"text", v.text}}; }

json write(const DecisionCall& v) {
  return {{"task", v.task},
          {"kind", to_string(v.kind)},
          {"subject", or_null(v.subject)},
          {"question", v.question},
          {"state", list(v.state)},
          {"options", list(v.options)},
          {"rule", v.rule},
          {"messages", list(v.messages)},
          {"slot", v.slot}};
}

json write(const KeyProbability& v) { return {{"key", v.key}, {"probability", v.probability}}; }

json write(const DecisionPick& v) {
  return {{"key", v.key},
          {"by", to_string(v.by)},
          {"read", to_string(v.read)},
          {"probabilities", list(v.probabilities)},
          {"fallback", v.fallback},
          {"miss", v.miss},
          {"error", to_string(v.error)},
          {"latency_ms", v.latency.count()},
          {"prompt_hash", v.prompt_hash},
          {"provider", v.provider},
          {"model", v.model},
          {"task", v.task},
          {"subject", or_null(v.subject)},
          {"option_keys", v.option_keys}};
}

json write(const LlmRequest& v) {
  json out = {{"id", v.id}, {"epoch", or_null(v.epoch)}};
  if (const auto* chat = std::get_if<ChatRequest>(&v.body)) {
    out["chat"] = write(*chat);
  } else {
    out["decision"] = write(std::get<DecisionCall>(v.body));
  }
  return out;
}

json write(const WarmUpOutcome& v) {
  return {{"ok", v.ok}, {"status", v.status}, {"time_ms", v.time.count()}, {"error", to_string(v.error)}};
}

json write(const LlmEvent& v) {
  return {{"kind", to_string(v.kind)},
          {"id", v.id},
          {"provider", v.provider},
          {"model", v.model},
          {"text", v.text},
          {"chat", v.chat ? write(*v.chat) : json(nullptr)},
          {"pick", v.pick ? write(*v.pick) : json(nullptr)},
          {"warm_up", write(v.warm_up)}};
}

}  // namespace

void to_json(json& j, Role v) { j = to_string(v); }
void from_json(const json& j, Role& v) { detail::read_root(j, "Role", v); }
void to_json(json& j, Purpose v) { j = to_string(v); }
void from_json(const json& j, Purpose& v) { detail::read_root(j, "Purpose", v); }
void to_json(json& j, ErrorClass v) { j = to_string(v); }
void from_json(const json& j, ErrorClass& v) { detail::read_root(j, "ErrorClass", v); }
void to_json(json& j, DecisionKind v) { j = to_string(v); }
void from_json(const json& j, DecisionKind& v) { detail::read_root(j, "DecisionKind", v); }
void to_json(json& j, DecidedBy v) { j = to_string(v); }
void from_json(const json& j, DecidedBy& v) { detail::read_root(j, "DecidedBy", v); }
void to_json(json& j, ReadAs v) { j = to_string(v); }
void from_json(const json& j, ReadAs& v) { detail::read_root(j, "ReadAs", v); }
void to_json(json& j, RequestKind v) { j = to_string(v); }
void from_json(const json& j, RequestKind& v) { detail::read_root(j, "RequestKind", v); }
void to_json(json& j, EventKind v) { j = to_string(v); }
void from_json(const json& j, EventKind& v) { detail::read_root(j, "EventKind", v); }

void to_json(json& j, const Part& v) { j = write(v); }
void from_json(const json& j, Part& v) { detail::read_root(j, "Part", v); }
void to_json(json& j, const Message& v) { j = write(v); }
void from_json(const json& j, Message& v) { detail::read_root(j, "Message", v); }
void to_json(json& j, const ChatRequest& v) { j = write(v); }
void from_json(const json& j, ChatRequest& v) { detail::read_root(j, "ChatRequest", v); }
void to_json(json& j, const Usage& v) { j = write(v); }
void from_json(const json& j, Usage& v) { detail::read_root(j, "Usage", v); }
void to_json(json& j, const TopLogprob& v) { j = write(v); }
void from_json(const json& j, TopLogprob& v) { detail::read_root(j, "TopLogprob", v); }
void to_json(json& j, const TokenLogprob& v) { j = write(v); }
void from_json(const json& j, TokenLogprob& v) { detail::read_root(j, "TokenLogprob", v); }
void to_json(json& j, const AttemptInfo& v) { j = write(v); }
void from_json(const json& j, AttemptInfo& v) { detail::read_root(j, "AttemptInfo", v); }
void to_json(json& j, const ChatResult& v) { j = write(v); }
void from_json(const json& j, ChatResult& v) { detail::read_root(j, "ChatResult", v); }
void to_json(json& j, const DecisionOption& v) { j = write(v); }
void from_json(const json& j, DecisionOption& v) { detail::read_root(j, "DecisionOption", v); }
void to_json(json& j, const DecisionRecord& v) { j = write(v); }
void from_json(const json& j, DecisionRecord& v) { detail::read_root(j, "DecisionRecord", v); }
void to_json(json& j, const DecisionCall& v) { j = write(v); }
void from_json(const json& j, DecisionCall& v) { detail::read_root(j, "DecisionCall", v); }
void to_json(json& j, const KeyProbability& v) { j = write(v); }
void from_json(const json& j, KeyProbability& v) { detail::read_root(j, "KeyProbability", v); }
void to_json(json& j, const DecisionPick& v) { j = write(v); }
void from_json(const json& j, DecisionPick& v) { detail::read_root(j, "DecisionPick", v); }
void to_json(json& j, const LlmRequest& v) { j = write(v); }
void from_json(const json& j, LlmRequest& v) { detail::read_root(j, "LlmRequest", v); }
void to_json(json& j, const WarmUpOutcome& v) { j = write(v); }
void from_json(const json& j, WarmUpOutcome& v) { detail::read_root(j, "WarmUpOutcome", v); }
void to_json(json& j, const LlmEvent& v) { j = write(v); }
void from_json(const json& j, LlmEvent& v) { detail::read_root(j, "LlmEvent", v); }

}  // namespace en::proto
