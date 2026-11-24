// The words of the model calls' record (calls.hpp): PAGES' words for the purposes and outcomes, ours for the rest and
// for the error classes (drafts for the owner's review), and the check on a model id a row may show (PV5).
#include "en/proto/calls.hpp"

#include <algorithm>

namespace en::proto {

const char* words(CallPurpose p) noexcept {
  switch (p) {
    case CallPurpose::Reply: return "reply";
    case CallPurpose::OwnInitiative: return "own-initiative line";
    case CallPurpose::CallOut: return "call-out reply";
    case CallPurpose::PreparedBatch: return "prepared-line batch";
    case CallPurpose::Memory: return "memory work";
    case CallPurpose::SelfTest: return "self-test";
    case CallPurpose::ModelList: return "model list";
    case CallPurpose::ProfileDraft: return "profile draft";
  }
  return "reply";
}

const char* words(CallOutcome o) noexcept {
  switch (o) {
    case CallOutcome::Waiting: return "waiting";
    case CallOutcome::Shown: return "shown";
    case CallOutcome::NotShown: return "not shown";
    case CallOutcome::FallbackModel: return "fallback model";
    case CallOutcome::TimedOut: return "timed out";
    case CallOutcome::Refused: return "refused";
    case CallOutcome::Failed: return "failed";
    case CallOutcome::PoolFilled: return "pool filled";
    case CallOutcome::Answered: return "answered";
  }
  return "waiting";
}

const char* error_words(ErrorClass e, int http_status) noexcept {
  switch (e) {
    case ErrorClass::None: return "";
    case ErrorClass::FirstTokenTimeout: return "Took too long to start its answer.";
    case ErrorClass::TotalTimeout: return "Took too long to finish its answer.";
    case ErrorClass::RateLimited: return "Asked EldenNet to wait: too many requests.";
    case ErrorClass::Auth: return "Refused the key, or was sent none.";
    case ErrorClass::BadRequest: return "Refused the request: the model may not exist there.";
    case ErrorClass::Server: return http_status == 0 ? "Could not be reached." : "Failed on the provider's side.";
    case ErrorClass::Empty: return "Answered with nothing.";
    case ErrorClass::Truncated: return "Stopped before its first sentence ended.";
    case ErrorClass::Refused: return "Declined to answer.";
    case ErrorClass::Cancelled: return "Stopped before it finished.";
    case ErrorClass::Preempted: return "Gave way to a more urgent call.";
    case ErrorClass::Busy: return "Not sent: the provider was busy with other calls.";
    case ErrorClass::BudgetSpent: return "Not sent: this hour's model calls are used up.";
    case ErrorClass::NoHealthyProvider: return "Not sent: no provider answers for it.";
    case ErrorClass::GateClosed: return "Not sent: model calls wait until this session is confirmed offline and solo.";
    case ErrorClass::Dropped: return "Not sent: more urgent calls filled the queue.";
  }
  return "";
}

const char* to_string(CallPurpose p) noexcept {
  switch (p) {
    case CallPurpose::Reply: return "reply";
    case CallPurpose::OwnInitiative: return "own_initiative";
    case CallPurpose::CallOut: return "call_out";
    case CallPurpose::PreparedBatch: return "prepared_batch";
    case CallPurpose::Memory: return "memory";
    case CallPurpose::SelfTest: return "selftest";
    case CallPurpose::ModelList: return "model_list";
    case CallPurpose::ProfileDraft: return "profile_draft";
  }
  return "reply";
}

const char* to_string(CallOutcome o) noexcept {
  switch (o) {
    case CallOutcome::Waiting: return "waiting";
    case CallOutcome::Shown: return "shown";
    case CallOutcome::NotShown: return "not_shown";
    case CallOutcome::FallbackModel: return "fallback_model";
    case CallOutcome::TimedOut: return "timed_out";
    case CallOutcome::Refused: return "refused";
    case CallOutcome::Failed: return "failed";
    case CallOutcome::PoolFilled: return "pool_filled";
    case CallOutcome::Answered: return "answered";
  }
  return "waiting";
}

bool shown_model(std::string_view id) noexcept {
  if (id.empty() || id.size() > 200 || id.front() == '/' || id.front() == '~') return false;
  if (std::any_of(id.begin(), id.end(), [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u < 0x20 || u == 0x7F || c == '\\';
      })) {
    return false;
  }
  const auto letter = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
  if (id.size() >= 2 && letter(id[0]) && id[1] == ':') return false;  // a drive: "C:" and on
  return id.find("/Users/") == std::string_view::npos && id.find("/home/") == std::string_view::npos;
}

}  // namespace en::proto
