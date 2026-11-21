// The check of a TOML text before toml++ reads it (toml_check.hpp; WHY§122). Moved here from en-gal's toml_reader so
// that en-llm's readers of the player's files run it too. It follows TOML's own contexts (a key, a table header, the
// start of a value, a bare value, what follows a value, inside arrays and inline tables) far enough to know where
// toml++ would scan a bare value, which it does through anything up to a space, a tab, ']', '}', ',', '#' or a line
// break, quotes and '=' included (en_fuzz_config's two findings, WHY§163).
#include "en/proto/toml_check.hpp"

#include <array>

namespace en::proto {

namespace {

struct Wording {
  const char* control;
  const char* header;
  const char* outside_ascii;
  const char* quote_in_word;
  const char* multi_line;
  const char* brace;
  const char* date;
  const char* after_value;
  const char* depth;
};

constexpr Wording kGal = {
    "a control character (other than a tab) is not allowed",
    "a line that starts with [ must open a table header with a key",
    "a character outside ASCII may stand only in a comment; in a string, write it as \\uXXXX",
    "a quote may not follow a bare word's own character",
    "multi-line strings are not used in gal/'s files",
    "a } stands where a value must",
    "a number with - or : after its first digit (a date or a time) is not used in gal/'s files",
    "a value must be followed by a space, a comma, a closing bracket or brace, or a comment",
    "arrays and inline tables nest more than 32 deep",
};

constexpr Wording kPlayer = {
    "a control character (other than a tab) is not allowed",
    "a line that starts with [ must name a table, such as [section]",
    "a letter outside plain ASCII may stand only inside quotes or in a comment",
    "a quote must open a string: it may not follow a word's own letter or digit",
    "multi-line strings (three quotes) are not used in these files",
    "a } stands where a value must be",
    "a date or a time is not a value these files take",
    "a value must be followed by a space, a comma, a closing bracket or brace, or a comment",
    "lists and tables are nested too deeply",
};

constexpr bool blank(char c) { return c == ' ' || c == '\t'; }
constexpr bool key_start(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
         c == '"' || c == '\'';
}
constexpr bool bare_key(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}
constexpr bool digit(char c) { return c >= '0' && c <= '9'; }
constexpr bool outside_ascii(char c) { return static_cast<unsigned char>(c) >= 0x80; }
constexpr bool control(char c) {
  const auto u = static_cast<unsigned char>(c);
  return (u < 0x20 && c != '\t' && c != '\n' && c != '\r') || u == 0x7F;
}

// Where the scan stands, outside strings and comments.
enum class Ctx : std::uint8_t {
  Key,         // a key (a line's, or an inline table's): bare parts, quoted parts, dots, then '='
  Header,      // inside a table header's brackets
  ValueStart,  // after '=', or in an array after '[' or ','
  BareValue,   // a value that does not start with a quote, '[' or '{': toml++ scans it up to a terminator
  AfterValue,  // after a value or a header: a terminator, a space or a comment
};

}  // namespace

std::optional<TomlRefusal> unsafe_for_toml(std::string_view text, TomlText whose) noexcept {
  const Wording& w = whose == TomlText::Gal ? kGal : kPlayer;
  const bool strings_may_hold_more = whose == TomlText::Player;
  enum class In { Text, Basic, Literal, Comment };
  In in = In::Text;
  Ctx ctx = Ctx::Key;
  std::array<char, 32> stack{};  // 'a' an array, 't' an inline table
  std::size_t depth = 0;
  char last = '\n';          // the last character outside strings and comments that is not a space, a tab or a break
  bool key_word = false;     // the key's current part has begun (a quote may not follow directly)
  bool digit_word = false;   // the current bare key or value began with a digit
  std::size_t i = text.starts_with("\xEF\xBB\xBF") ? 3 : 0;  // toml++ skips a byte-order mark
  std::size_t line = 1;
  bool line_start = true;
  const auto top = [&]() { return depth == 0 ? '\0' : stack[depth - 1]; };
  // A value has ended; what may come next depends on its container.
  const auto end_value = [&]() { ctx = Ctx::AfterValue; };
  for (; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\n' || c == '\r') {  // no string or comment of ours goes past its line
      if (c == '\n') ++line;
      in = In::Text;
      line_start = true;
      if (ctx == Ctx::BareValue) end_value();
      if (depth == 0) {
        ctx = Ctx::Key;
        key_word = false;
      }
      continue;
    }
    if (control(c)) return TomlRefusal{line, w.control};  // TOML allows none, and toml++ scans words through them
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
      case In::Comment:
        break;
      case In::Basic:
        if (outside_ascii(c) && !strings_may_hold_more) {
          return TomlRefusal{line, w.outside_ascii};
        } else if (c == '\\' && i + 1 < text.size() && text[i + 1] != '\n' && text[i + 1] != '\r') {
          if (outside_ascii(text[i + 1]) || control(text[i + 1])) return TomlRefusal{line, w.outside_ascii};
          ++i;  // the escaped character, whatever it is, does not end the string
        } else if (c == '"') {
          in = In::Text;
          if (ctx == Ctx::ValueStart) end_value();
          key_word = true;
          last = c;
        }
        break;
      case In::Literal:
        if (outside_ascii(c) && !strings_may_hold_more) return TomlRefusal{line, w.outside_ascii};
        if (c == '\'') {
          in = In::Text;
          if (ctx == Ctx::ValueStart) end_value();
          key_word = true;
          last = c;
        }
        break;
      case In::Text: {
        if (outside_ascii(c)) return TomlRefusal{line, w.outside_ascii};
        const bool quote = c == '"' || c == '\'';
        if (quote && text.substr(i, 3) == std::string_view(c == '"' ? "\"\"\"" : "\'\'\'")) {
          return TomlRefusal{line, w.multi_line};
        }
        if (c == '#') {
          if (ctx == Ctx::BareValue) end_value();
          in = In::Comment;
          break;
        }
        if (blank(c)) {
          if (ctx == Ctx::BareValue) end_value();
          if (ctx == Ctx::Key || ctx == Ctx::Header) key_word = false;
          break;
        }
        switch (ctx) {
          case Ctx::Key:
          case Ctx::Header:
            if (quote) {
              // A quoted key part opens only where one may: after a dot, a space, '{', ',', '[' or a line's start.
              if (key_word && last != '.') return TomlRefusal{line, w.quote_in_word};
              in = c == '"' ? In::Basic : In::Literal;
            } else if (bare_key(c)) {
              if (!key_word) digit_word = digit(c);
              key_word = true;
            } else if (c == '.') {
              key_word = false;
            } else if (c == '=' && ctx == Ctx::Key) {
              ctx = Ctx::ValueStart;
            } else if (c == '[' && ctx == Ctx::Key && depth == 0) {
              ctx = Ctx::Header;  // a table header: its keys follow
              key_word = false;
            } else if (c == ']' && ctx == Ctx::Header) {
              if (i + 1 < text.size() && text[i + 1] == ']') ++i;
              end_value();
            } else if (c == '}' && ctx == Ctx::Key && top() == 't') {
              if (last != '{') return TomlRefusal{line, w.brace};  // "{ a = 1, }": never valid
              --depth;
              end_value();
            }
            // Anything else in a key is an error toml++ reports itself.
            break;
          case Ctx::ValueStart:
            if (quote) {
              in = c == '"' ? In::Basic : In::Literal;
            } else if (c == '[') {
              if (depth == stack.size()) return TomlRefusal{line, w.depth};
              stack[depth++] = 'a';
            } else if (c == '{') {
              if (depth == stack.size()) return TomlRefusal{line, w.depth};
              stack[depth++] = 't';
              ctx = Ctx::Key;
              key_word = false;
            } else if (c == '}') {
              return TomlRefusal{line, w.brace};  // never valid where a value must stand
            } else if (c == ']' && top() == 'a') {
              --depth;  // an empty array, or a trailing comma
              end_value();
            } else {
              ctx = Ctx::BareValue;
              digit_word = digit(c);
            }
            break;
          case Ctx::BareValue:
            if (quote) return TomlRefusal{line, w.quote_in_word};  // toml++ would scan on past it
            if (digit_word && (c == '-' || c == ':')) return TomlRefusal{line, w.date};
            if (c == ',' || c == ']' || c == '}') {
              ctx = Ctx::AfterValue;
              --i;  // the terminator is the next context's
              continue;
            }
            break;
          case Ctx::AfterValue:
            if (c == ',' && top() == 'a') {
              ctx = Ctx::ValueStart;
            } else if (c == ',' && top() == 't') {
              ctx = Ctx::Key;
              key_word = false;
            } else if (c == ']' && top() == 'a') {
              --depth;
            } else if (c == '}' && top() == 't') {
              --depth;
            } else {
              return TomlRefusal{line, w.after_value};
            }
            break;
        }
        last = c;
        break;
      }
    }
  }
  return std::nullopt;
}

}  // namespace en::proto
