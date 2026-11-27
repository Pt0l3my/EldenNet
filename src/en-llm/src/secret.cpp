#include "en/llm/secret.hpp"

#include <ostream>

namespace en::llm {

Secret& Secret::operator=(const Secret& other) {
  if (this != &other) {
    wipe();
    value_ = other.value_;
  }
  return *this;
}

Secret& Secret::operator=(Secret&& other) noexcept {
  if (this != &other) {
    wipe();
    value_ = std::move(other.value_);
    other.value_.clear();
  }
  return *this;
}

Secret::~Secret() { wipe(); }

// Best effort: overwrite the buffer before it is freed, so a key does not linger in released memory. The volatile
// pointer keeps the compiler from dropping the stores as dead.
void Secret::wipe() noexcept {
  volatile char* p = value_.data();
  for (std::size_t i = 0; i < value_.size(); ++i) p[i] = '\0';
  value_.clear();
}

std::ostream& operator<<(std::ostream& os, const Secret&) { return os << Secret::kRedacted; }

namespace {

bool token_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

bool space_char(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

}  // namespace

std::string redact_credentials(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  std::size_t i = 0;
  while (i < text.size()) {
    if (!token_char(text[i])) {
      out.push_back(text[i++]);
      continue;
    }
    std::size_t j = i;
    while (j < text.size() && token_char(text[j])) ++j;
    const std::string_view word = text.substr(i, j - i);
    const bool keyish = word.size() >= 24 || word.rfind("sk-", 0) == 0 || word.rfind("sk_", 0) == 0;
    out.append(keyish ? std::string(Secret::kRedacted) : std::string(word));
    i = j;
  }
  // "Bearer <token>" with a short token still names a credential.
  for (std::size_t at = out.find("Bearer "); at != std::string::npos; at = out.find("Bearer ", at + 7)) {
    std::size_t end = at + 7;
    while (end < out.size() && !space_char(out[end])) ++end;
    out.replace(at + 7, end - at - 7, Secret::kRedacted);
  }
  return out;
}

}  // namespace en::llm
