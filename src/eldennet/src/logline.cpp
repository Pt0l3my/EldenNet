// The scrubbed logger's lines (logline.hpp).
#include "en/shell/logline.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace en::shell {

bool is_word(std::string_view s) noexcept {
  if (s.empty() || s.size() > kMaxWord) return false;
  return std::all_of(s.begin(), s.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
           c == '+' || c == '-';
  });
}

Line::Line(std::string_view event) noexcept {
  const std::string_view e = is_word(event) ? event : std::string_view("redacted");
  std::memcpy(buf_.data(), e.data(), e.size());
  n_ = e.size();
}

void Line::pair(std::string_view key, std::string_view value) noexcept {
  const std::string_view k = is_word(key) ? key : std::string_view("redacted");
  const std::size_t need = 1 + k.size() + 1 + value.size();
  if (cut_ || n_ + need > buf_.size()) {
    cut_ = true;
    return;
  }
  buf_[n_++] = ' ';
  std::memcpy(buf_.data() + n_, k.data(), k.size());
  n_ += k.size();
  buf_[n_++] = '=';
  std::memcpy(buf_.data() + n_, value.data(), value.size());
  n_ += value.size();
}

Line& Line::num(std::string_view key, std::uint64_t v) noexcept {
  char b[24];
  const int n = std::snprintf(b, sizeof b, "%" PRIu64, v);
  pair(key, std::string_view(b, n > 0 ? static_cast<std::size_t>(n) : 0));
  return *this;
}

Line& Line::sig(std::string_view key, std::int64_t v) noexcept {
  char b[24];
  const int n = std::snprintf(b, sizeof b, "%" PRId64, v);
  pair(key, std::string_view(b, n > 0 ? static_cast<std::size_t>(n) : 0));
  return *this;
}

Line& Line::hex(std::string_view key, std::uint64_t v) noexcept {
  char b[24];
  const int n = std::snprintf(b, sizeof b, "0x%" PRIX64, v);
  pair(key, std::string_view(b, n > 0 ? static_cast<std::size_t>(n) : 0));
  return *this;
}

Line& Line::word(std::string_view key, std::string_view w) noexcept {
  pair(key, is_word(w) ? w : std::string_view("redacted"));
  return *this;
}

}  // namespace en::shell
