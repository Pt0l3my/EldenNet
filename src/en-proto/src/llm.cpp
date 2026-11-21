// The pure helpers of the model client's messages (llm.hpp): building messages, the request and event accessors, a
// pick's probability and row, and each enum's name.
#include "en/proto/llm.hpp"

#include <utility>

namespace en::proto {

using nlohmann::json;

Message Message::system(std::string text, bool cache_break) {
  return Message{Role::System, {Part{std::move(text), cache_break}}};
}

Message Message::user(std::string text) { return Message{Role::User, {Part{std::move(text), false}}}; }

Message Message::assistant(std::string text) { return Message{Role::Assistant, {Part{std::move(text), false}}}; }

std::string Message::joined() const {
  std::string out;
  for (const Part& part : parts) out.append(part.text);
  return out;
}

bool Message::has_cache_break() const {
  for (const Part& part : parts) {
    if (part.cache_break) return true;
  }
  return false;
}

void normalise(Message& message) {
  std::vector<Part> kept;
  kept.reserve(message.parts.size());
  for (Part& part : message.parts) {
    if (!part.text.empty()) {
      kept.push_back(std::move(part));
    } else if (part.cache_break && !kept.empty()) {
      kept.back().cache_break = true;
    }
  }
  message.parts = std::move(kept);
}

std::optional<double> DecisionPick::probability() const {
  for (const auto& p : probabilities) {
    if (p.key == key) return p.probability;
  }
  return std::nullopt;
}

json DecisionPick::row() const {
  const auto p = probability();
  return json{{"task", task},
              {"subject", subject ? json(*subject) : json(nullptr)},
              {"options", option_keys},
              {"pick", key},
              {"probability", p ? json(*p) : json(nullptr)},
              {"by", to_string(by)},
              {"latency_ms", latency.count()},
              {"prompt_hash", prompt_hash}};
}

RequestKind LlmRequest::kind() const {
  return std::holds_alternative<DecisionCall>(body) ? RequestKind::Decision : RequestKind::Chat;
}

LlmRequest LlmRequest::chat(RequestId id, ChatRequest request, std::optional<Epoch> epoch) {
  LlmRequest r;
  r.id = id;
  r.body = std::move(request);
  r.epoch = epoch;
  return r;
}

LlmRequest LlmRequest::decision(RequestId id, DecisionCall call, std::optional<Epoch> epoch) {
  LlmRequest r;
  r.id = id;
  r.body = std::move(call);
  r.epoch = epoch;
  return r;
}

ErrorClass LlmEvent::error() const {
  if (chat) return chat->error;
  if (pick) return pick->error;
  if (kind == EventKind::ModelsEnded) return models.error;
  return kind == EventKind::WarmUpEnded ? warm_up.error : ErrorClass::None;
}

const char* to_string(Role role) {
  switch (role) {
    case Role::System: return "system";
    case Role::User: return "user";
    case Role::Assistant: return "assistant";
  }
  return "user";
}

const char* to_string(Purpose purpose) {
  switch (purpose) {
    case Purpose::Reply: return "reply";
    case Purpose::OwnInitiative: return "own_initiative";
    case Purpose::Decision: return "decision";
    case Purpose::Background: return "background";
  }
  return "reply";
}

const char* to_string(ErrorClass e) {
  switch (e) {
    case ErrorClass::None: return "None";
    case ErrorClass::FirstTokenTimeout: return "FirstTokenTimeout";
    case ErrorClass::TotalTimeout: return "TotalTimeout";
    case ErrorClass::RateLimited: return "RateLimited";
    case ErrorClass::Auth: return "Auth";
    case ErrorClass::BadRequest: return "BadRequest";
    case ErrorClass::Server: return "Server";
    case ErrorClass::Empty: return "Empty";
    case ErrorClass::Truncated: return "Truncated";
    case ErrorClass::Refused: return "Refused";
    case ErrorClass::Cancelled: return "Cancelled";
    case ErrorClass::Preempted: return "Preempted";
    case ErrorClass::Busy: return "Busy";
    case ErrorClass::BudgetSpent: return "BudgetSpent";
    case ErrorClass::NoHealthyProvider: return "NoHealthyProvider";
    case ErrorClass::GateClosed: return "GateClosed";
    case ErrorClass::Dropped: return "Dropped";
  }
  return "Unknown";
}

const char* to_string(DecisionKind kind) { return kind == DecisionKind::YesNo ? "yes_no" : "choice"; }

const char* to_string(DecidedBy by) { return by == DecidedBy::Model ? "model" : "rules"; }

const char* to_string(ReadAs read) {
  switch (read) {
    case ReadAs::Schema: return "schema";
    case ReadAs::Meaning: return "meaning";
    case ReadAs::YesNo: return "yes_no";
    case ReadAs::Rules: return "rules";
  }
  return "rules";
}

const char* to_string(RequestKind kind) { return kind == RequestKind::Decision ? "decision" : "chat"; }

const char* to_string(EventKind kind) {
  switch (kind) {
    case EventKind::Started: return "Started";
    case EventKind::Delta: return "Delta";
    case EventKind::Reset: return "Reset";
    case EventKind::Ended: return "Ended";
    case EventKind::WarmUpEnded: return "WarmUpEnded";
    case EventKind::ModelsEnded: return "ModelsEnded";
  }
  return "Unknown";
}

}  // namespace en::proto
