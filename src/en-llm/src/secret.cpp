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

}  // namespace en::llm
