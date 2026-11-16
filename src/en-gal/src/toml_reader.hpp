#pragma once
// Strict reading of gal/'s TOML files (builds.toml, patterns.toml, layouts/*.toml) with toml++, which en-gal builds
// without exceptions (TOML_EXCEPTIONS=0: en-gal may not throw, ARCH §2.4). Each table is read key by key with typed
// getters; a key that no getter names is an error, and so is a value of the wrong type or out of its range. Every
// problem is recorded, up to kMaxSchemaErrors, with its path; values are never quoted. Internal to en-gal.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <toml.hpp>

#include "en/gal/schema.hpp"

namespace en::gal::toml_read {

// The problems found in one file.
class Errors {
 public:
  explicit Errors(std::string file) : file_(std::move(file)) {}
  void add(const std::string& path, std::string problem);
  [[nodiscard]] bool empty() const noexcept { return errors_.empty(); }
  [[nodiscard]] const std::string& file() const noexcept { return file_; }
  [[nodiscard]] SchemaErrors take();

 private:
  std::string file_;
  SchemaErrors errors_;
  std::size_t dropped_ = 0;
};

// The document, or nothing with the parser's error recorded (its line and column, and what it expected).
[[nodiscard]] std::optional<toml::table> parse(std::string_view text, Errors& errors);

// toml++ 3.4.0 has four traps that en_fuzz_galtoml found (in five inputs), each an assertion in a debug build (which
// aborts) and an assumption or __builtin_unreachable in a release one (undefined behaviour):
// - a table header's first key is read without a check that one starts there ("[!]", "[ \x01]", "[[ \xC3\xA9 ]]");
// - its test for non-ASCII whitespace reaches "unreachable" for many characters (U+00A1 to U+0499 among them), and it
//   runs on any character where whitespace may stand: after a key or a value, after a backslash in a multi-line
//   string, and so on;
// - an array reads a value wherever it does not find ',' or ']', and a value assumes it does not start with '}'
//   ("[}]", "[1, }]");
// - a date followed by a space and one digit makes it scan on from a terminator ("1979-05-27 7,").
// So before toml++ sees a gal/ file, five rules of our own are checked, which refuse no file of gal/'s own:
// - a line whose first character after spaces and tabs is '[' goes on, after "[" or "[[" and spaces or tabs, with a
//   key's first character (a letter, a digit, '_', '-' or a quote), ']' or the end;
// - a character outside ASCII stands only in a comment (a string writes one as \uXXXX): toml++ scans a bare value up to
//   a space, ']', '}', ',' or '#', quotes and all, so "x = a\"é\"" reaches the trap through what looks like a string;
// - there is no multi-line string;
// - a '}' never follows '[' or ',' (spaces, tabs, line breaks and comments between them aside), which is never valid;
// - a bare key or value that starts with a digit has no '-' or ':' after it: gal/'s files hold no dates or times.
// The first rule also refuses such a line inside a multi-line array, which gal/'s files never need.
struct UnsafeText {
  std::size_t line = 0;
  const char* problem = "";
};
[[nodiscard]] std::optional<UnsafeText> unsafe_for_toml(std::string_view text) noexcept;

// "build[3]".
[[nodiscard]] std::string indexed(std::string_view base, std::size_t i);

// A name as an error may show it: short, printable ASCII only.
[[nodiscard]] std::string shown(std::string_view name);

// One table, read strictly.
class Table {
 public:
  Table(const toml::table& table, std::string path, Errors& errors)
      : t_(table), path_(std::move(path)), errors_(errors) {}

  // The path of one of its keys: "build[1].region".
  [[nodiscard]] std::string where(std::string_view key) const;
  [[nodiscard]] const std::string& path() const noexcept { return path_; }
  [[nodiscard]] Errors& errors() noexcept { return errors_; }
  [[nodiscard]] bool has(std::string_view key) const noexcept { return t_.contains(key); }

  // Each getter names its key as known, records what is wrong, and answers nothing then (or when an optional key is
  // absent).
  std::optional<std::string> text(std::string_view key, bool required);
  std::optional<std::int64_t> integer(std::string_view key, bool required, std::int64_t lo, std::int64_t hi);
  std::optional<bool> boolean(std::string_view key, bool required);
  const toml::table* table(std::string_view key, bool required);
  const toml::array* array(std::string_view key, bool required);
  // The node, of any type, for the caller to read.
  const toml::node* node(std::string_view key, bool required);

  // Records every key that no getter named.
  void done();

 private:
  const toml::node* find(std::string_view key, bool required);

  const toml::table& t_;
  std::string path_;
  Errors& errors_;
  std::vector<std::string> known_;
};

// A whole number of `node`, within [lo, hi].
std::optional<std::int64_t> integer_of(const toml::node& node, const std::string& path, Errors& errors, std::int64_t lo,
                                       std::int64_t hi);

// [lo, hi]: two whole numbers within [min, max], the first no greater than the second.
std::optional<std::pair<std::int64_t, std::int64_t>> range_of(const toml::node& node, const std::string& path,
                                                              Errors& errors, std::int64_t min, std::int64_t max);

// An array of at most `max_items` strings, each of 1 to `max_bytes` bytes.
std::optional<std::vector<std::string>> strings_of(const toml::array& array, const std::string& path, Errors& errors,
                                                   std::size_t max_items, std::size_t max_bytes);

// Checks on the shapes of strings.
[[nodiscard]] bool is_identifier(std::string_view s, std::size_t max_bytes) noexcept;  // [A-Za-z0-9_], 1 to max
[[nodiscard]] bool is_date(std::string_view s) noexcept;                               // YYYY-MM-DD
// A relative path of the repository or of gal/: '/'-separated parts of [A-Za-z0-9._-], none empty, "." or "..", none
// starting with '.', no drive, no backslash, at most 200 bytes.
[[nodiscard]] bool is_relative_path(std::string_view s) noexcept;
// "2.7.2.0-ww": four dot-separated numbers of at most 5 digits, '-', and "ww" or "jp".
[[nodiscard]] bool is_build_key(std::string_view s) noexcept;

}  // namespace en::gal::toml_read
