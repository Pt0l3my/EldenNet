// The check of a TOML text before toml++ reads it (toml_check.hpp; WHY§122). Moved here from en-gal's toml_reader so
// that en-llm's readers of the player's files run it too.
#include "en/proto/toml_check.hpp"

namespace en::proto {

namespace {

struct Wording {
  const char* header;
  const char* outside_ascii;
  const char* quote_in_word;
  const char* multi_line;
  const char* brace;
  const char* date;
};

constexpr Wording kGal = {
    "a line that starts with [ must open a table header with a key",
    "a character outside ASCII may stand only in a comment; in a string, write it as \\uXXXX",
    "a quote may not follow a bare word's own character",
    "multi-line strings are not used in gal/'s files",
    "a } stands where a value must",
    "a number with - or : after its first digit (a date or a time) is not used in gal/'s files",
};

constexpr Wording kPlayer = {
    "a line that starts with [ must name a table, such as [section]",
    "a letter outside plain ASCII may stand only inside quotes or in a comment",
    "a quote must open a string: it may not follow a word's own letter or digit",
    "multi-line strings (three quotes) are not used in these files",
    "a } stands where a value must be",
    "a date or a time is not a value these files take",
};

constexpr bool blank(char c) { return c == ' ' || c == '\t'; }
constexpr bool key_start(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
         c == '"' || c == '\'';
}
constexpr bool bare(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
         c == '+' || c == '.' || c == ':';
}
constexpr bool outside_ascii(char c) { return static_cast<unsigned char>(c) >= 0x80; }

}  // namespace

std::optional<TomlRefusal> unsafe_for_toml(std::string_view text, TomlText whose) noexcept {
  const Wording& w = whose == TomlText::Gal ? kGal : kPlayer;
  const bool strings_may_hold_more = whose == TomlText::Player;
  enum class In { Text, Basic, Literal, Comment };
  In in = In::Text;
  char last = '\n';          // the last character outside strings and comments that is not a space, a tab or a break
  char prev = '\n';          // the character just before, outside strings and comments
  bool in_bare = false;      // within a bare key or value
  bool digit_bare = false;   // and it began with a digit
  std::size_t i = text.starts_with("\xEF\xBB\xBF") ? 3 : 0;  // toml++ skips a byte-order mark
  std::size_t line = 1;
  bool line_start = true;
  for (; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\n' || c == '\r') {  // no string or comment of ours goes past its line
      if (c == '\n') ++line;
      in = In::Text;
      line_start = true;
      in_bare = false;
      prev = c;
      continue;
    }
    if (in == In::Text && line_start) {
      line_start = false;
      std::size_t k = i;
      while (k < text.size() && blank(text[k])) ++k;
      if (k < text.size() && text[k] == '[') {
        ++k;
        while (k < text.size() && blank(text[k])) ++k;
        if (k < text.size() && text[k] == '[') ++k;
        while (k < text.size() && blank(text[k])) ++k;
        if (k < text.size() && !key_start(text[k]) && text[k] != ']') return TomlRefusal{line, w.header};
      }
    }
    switch (in) {
      case In::Text:
        if (c == '"' || c == '\'') {
          // A quote inside a bare word is part of the word to toml++, which then scans on past it; only the dot of a
          // dotted key may stand right before a quoted key part.
          if (in_bare && prev != '.') return TomlRefusal{line, w.quote_in_word};
          if (text.substr(i, 3) == std::string_view(c == '"' ? "\"\"\"" : "\'\'\'")) {
            return TomlRefusal{line, w.multi_line};
          }
          in = c == '"' ? In::Basic : In::Literal;
          in_bare = false;
          last = c;
          break;
        }
        if (bare(c)) {
          if (!in_bare) {
            in_bare = true;
            digit_bare = c >= '0' && c <= '9';
          } else if (digit_bare && (c == '-' || c == ':')) {
            return TomlRefusal{line, w.date};
          }
        } else {
          in_bare = false;
        }
        if (c == '#') {
          in = In::Comment;
        } else if (outside_ascii(c)) {
          return TomlRefusal{line, w.outside_ascii};
        } else if (c == '}' && (last == '[' || last == ',')) {
          return TomlRefusal{line, w.brace};
        } else if (!blank(c)) {
          last = c;
        }
        prev = c;
        break;
      case In::Basic:
        if (outside_ascii(c) && !strings_may_hold_more) {
          return TomlRefusal{line, w.outside_ascii};
        } else if (c == '\\' && i + 1 < text.size() && text[i + 1] != '\n' && text[i + 1] != '\r') {
          if (outside_ascii(text[i + 1])) return TomlRefusal{line, w.outside_ascii};  // never an escape
          ++i;  // the escaped character, whatever it is, does not end the string
        } else if (c == '"') {
          in = In::Text;
          prev = c;
        }
        break;
      case In::Literal:
        if (outside_ascii(c) && !strings_may_hold_more) return TomlRefusal{line, w.outside_ascii};
        if (c == '\'') {
          in = In::Text;
          prev = c;
        }
        break;
      case In::Comment:
        break;
    }
  }
  return std::nullopt;
}

}  // namespace en::proto
