#pragma once
// A key or other credential, held in memory only (ARCH §11.5).
#include <iosfwd>
#include <string>
#include <string_view>

namespace en::llm {

// Prints as "<redacted>" wherever it is streamed, so a log line or a test failure can never show it. The one way to
// read the value is reveal(), which only the transports call when they write a request, so every use is easy to find.
class Secret {
 public:
  static constexpr std::string_view kRedacted = "<redacted>";

  Secret() = default;
  explicit Secret(std::string value) : value_(std::move(value)) {}
  Secret(const Secret&) = default;
  Secret(Secret&&) noexcept = default;
  Secret& operator=(const Secret& other);
  Secret& operator=(Secret&& other) noexcept;
  ~Secret();

  const std::string& reveal() const { return value_; }
  bool empty() const { return value_.empty(); }

 private:
  void wipe() noexcept;
  std::string value_;
};

std::ostream& operator<<(std::ostream& os, const Secret& secret);

// Anything in `text` shaped like a credential replaced by Secret::kRedacted: a token beginning "sk-" or "sk_", any run
// of 24 or more token characters (letters, digits, '_' and '-'), and the token after "Bearer ". Everything else, its
// spaces and line breaks included, stays as it is. A server's message goes through it (with its spaces made one line,
// stream.cpp), and so do Tools › Workshop's prompt and reply, after the configured keys' own values.
[[nodiscard]] std::string redact_credentials(std::string_view text);

}  // namespace en::llm
