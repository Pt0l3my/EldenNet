#include "en/llm/errors.hpp"

namespace en::llm {

bool counts_in_health(ErrorClass e) {
  switch (e) {
    case ErrorClass::FirstTokenTimeout:
    case ErrorClass::TotalTimeout:
    case ErrorClass::Server:
    case ErrorClass::Empty:
    case ErrorClass::Truncated:
      return true;
    default:
      // RateLimited and Auth act on the whole provider instead; BadRequest is our request; Refused is the content.
      return false;
  }
}

ErrorClass classify_http_status(int status) {
  if (status >= 200 && status < 300) return ErrorClass::None;
  if (status == 429) return ErrorClass::RateLimited;
  if (status == 401 || status == 403) return ErrorClass::Auth;
  // A timeout or conflict on the server's side is not something a stripped request would fix.
  if (status == 408 || status == 409 || status == 425) return ErrorClass::Server;
  if (status >= 400 && status < 500) return ErrorClass::BadRequest;
  return ErrorClass::Server;  // 5xx, and anything a chat endpoint should never answer (1xx, 3xx)
}

}  // namespace en::llm
