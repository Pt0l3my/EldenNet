#pragma once
// The scrubbed logger's lines (ARCH §1.2's T11; DESIGN §14's rotating logs; PV1 to PV8): `<event> key=value ...`, built
// in a fixed buffer with no allocation, so any thread may make one, the crash filter excepted. A line holds numbers and
// words only: a key or a word value that is not [A-Za-z0-9_.+-] (at most 48 characters) is written as "redacted", so
// no path, user name, address book entry or text from the game can reach a log, whatever a caller passes.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace en::shell {

inline constexpr std::size_t kMaxWord = 48;
inline constexpr std::size_t kLineBytes = 320;

// A word a line may hold: 1 to 48 characters of [A-Za-z0-9_.+-].
[[nodiscard]] bool is_word(std::string_view s) noexcept;

class Line {
 public:
  explicit Line(std::string_view event) noexcept;

  Line& num(std::string_view key, std::uint64_t v) noexcept;
  Line& sig(std::string_view key, std::int64_t v) noexcept;
  Line& hex(std::string_view key, std::uint64_t v) noexcept;
  Line& flag(std::string_view key, bool v) noexcept { return num(key, v ? 1 : 0); }
  // A word, scrubbed: anything that is not a word is written as "redacted".
  Line& word(std::string_view key, std::string_view w) noexcept;

  [[nodiscard]] std::string_view text() const noexcept { return {buf_.data(), n_}; }
  // The line was cut at its capacity (its last pairs dropped whole).
  [[nodiscard]] bool cut() const noexcept { return cut_; }

 private:
  void pair(std::string_view key, std::string_view value) noexcept;

  std::array<char, kLineBytes> buf_{};
  std::size_t n_ = 0;
  bool cut_ = false;
};

}  // namespace en::shell
