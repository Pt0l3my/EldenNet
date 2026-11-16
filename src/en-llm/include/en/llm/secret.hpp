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

}  // namespace en::llm
