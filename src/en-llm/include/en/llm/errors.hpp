#pragma once
// Error classes (ARCH §11.2) and the client's own outcomes.
#include <cstdint>
#include <string_view>

namespace en::llm {

enum class ErrorClass : std::uint8_t {
  None,               // success
  // ARCH §11.2's classes
  FirstTokenTimeout,  // no content delta by the slot's first-token time
  TotalTimeout,       // the slot's total time passed
  RateLimited,        // HTTP 429, or a rate-limit error inside the stream
  Auth,               // HTTP 401 or 403, or a provider with no key
  BadRequest,         // HTTP 400, 404, 422 and other 4xx: our request
  Server,             // HTTP 5xx, a connection error, a broken stream
  Empty,              // the reply ended with no content
  Truncated,          // finish_reason = length before a full sentence
  Refused,            // a content-filter finish, or a refusal pattern
  // the client's own outcomes: nothing was wrong with the provider
  Cancelled,          // the caller cancelled (the input epoch, shutdown)
  Preempted,          // a higher-priority job took the provider's connection (ARCH §11.3); the job requeues
  Busy,               // the slot or every healthy provider is at its concurrency; nothing was sent
  BudgetSpent,        // the requests per hour are spent and this request is not a reply (ARCH §11.4)
  NoHealthyProvider,  // every model of the slot is cooling, disabled or missing
  GateClosed,         // the offline gate is false (ARCH §5.1): nothing goes out
};

const char* to_string(ErrorClass e);

// Whether a failure of this class adds to its (provider, model) pair's count (ARCH §11.2's last column).
bool counts_in_health(ErrorClass e);

// The class an HTTP status gives (2xx gives None).
ErrorClass classify_http_status(int status);

}  // namespace en::llm
