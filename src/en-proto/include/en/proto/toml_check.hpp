#pragma once
// A check of a TOML text before toml++ 3.4.0 reads it (WHY§122), shared by every reader of TOML in the engine: en-gal's
// gal/ files and en-llm's providers, models, settings and secrets files, which the player edits. toml++ has four traps,
// found by en_fuzz_galtoml in five inputs, each an assertion in a debug build (the program aborts) and an assumption or
// __builtin_unreachable in a release one (undefined behaviour, in the game's process):
// - a table header's first key is read without a check that one starts there ("[!]", "[ \x01]", "[[ \xC3\xA9 ]]");
// - its test for non-ASCII whitespace reaches "unreachable" for many characters (U+00A1 to U+0499 among them), and it
//   runs on any character where whitespace may stand: after a key or a value, after a backslash in a multi-line
//   string, inside a bare value (scanned quotes and all up to a space, ']', '}', ',' or '#');
// - an array reads a value wherever it does not find ',' or ']', and a value assumes it does not start with '}'
//   ("[}]", "[1, }]");
// - a date followed by a space and one digit makes it scan on from a terminator ("1979-05-27 7,").
// So, before toml++ sees a text, these rules of our own are checked; none refuses a file the engine's readers accept:
// - a line whose first character after spaces and tabs is '[' goes on, after "[" or "[[" and spaces or tabs, with a
//   key's first character (a letter, a digit, '_', '-' or a quote), ']' or the end (such a line inside a multi-line
//   array is refused too, which no file of ours needs);
// - a character outside ASCII stands only in a comment, or (in a player's file) inside a quoted string that opens
//   where a value or a key may start; gal/'s files write one in a string as \uXXXX;
// - a quote never follows a bare word's own character (toml++ would read it as part of the word), save after the dot
//   of a dotted key;
// - there is no multi-line string;
// - a '}' never follows '[' or ',' (spaces, tabs, line breaks and comments between them aside), which is never valid;
// - a bare key or value that starts with a digit has no '-' or ':' after it: no file of ours holds a date or a time.
// It needs no toml++, reads the text once, allocates nothing and never throws.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace en::proto {

// Whose file it is: gal/'s (written by us, ASCII outside comments) or the player's (non-ASCII in strings too). The
// problems are worded for each.
enum class TomlText : std::uint8_t { Gal, Player };

struct TomlRefusal {
  std::size_t line = 0;      // 1-based
  const char* problem = "";  // a fixed text: it never quotes the file
};

[[nodiscard]] std::optional<TomlRefusal> unsafe_for_toml(std::string_view text, TomlText whose) noexcept;

}  // namespace en::proto
