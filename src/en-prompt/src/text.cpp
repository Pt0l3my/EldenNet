// Text helpers: data stand-ins, UTF-8, dotted names, source preparation and name suggestions.
#include <algorithm>
#include <string>

#include "detail.hpp"

namespace en::prompt::detail {

std::string escape_data(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (c == '[') c = data_bracket;
    else if (c == '{') c = data_open_brace;
    else if (c == '}') c = data_close_brace;
  }
  return out;
}

void restore_data(std::string& text) {
  for (char& c : text) {
    if (c == data_bracket) c = '[';
    else if (c == data_open_brace) c = '{';
    else if (c == data_close_brace) c = '}';
  }
}

bool is_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::string_view trim(std::string_view text) noexcept {
  while (!text.empty() && is_space(text.front())) text.remove_prefix(1);
  while (!text.empty() && is_space(text.back())) text.remove_suffix(1);
  return text;
}

namespace {

bool is_ascii_letter(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
bool is_identifier_char(char c) noexcept { return is_ascii_letter(c) || is_digit(c) || c == '_'; }
bool is_continuation(unsigned char c) noexcept { return (c & 0xC0u) == 0x80u; }

// The length of the UTF-8 sequence starting at text[i], or 0 when it is not a valid one.
std::size_t utf8_sequence(std::string_view text, std::size_t i) noexcept {
  const auto c = static_cast<unsigned char>(text[i]);
  std::size_t length = 0;
  unsigned min = 0;
  unsigned code = 0;
  if (c < 0x80u) return 1;
  if ((c & 0xE0u) == 0xC0u) {
    length = 2;
    min = 0x80u;
    code = c & 0x1Fu;
  } else if ((c & 0xF0u) == 0xE0u) {
    length = 3;
    min = 0x800u;
    code = c & 0x0Fu;
  } else if ((c & 0xF8u) == 0xF0u) {
    length = 4;
    min = 0x10000u;
    code = c & 0x07u;
  } else {
    return 0;
  }
  if (i + length > text.size()) return 0;
  for (std::size_t k = 1; k < length; ++k) {
    const auto next = static_cast<unsigned char>(text[i + k]);
    if (!is_continuation(next)) return 0;
    code = (code << 6) | (next & 0x3Fu);
  }
  // Overlong forms, surrogates and code points past Unicode's end are not valid UTF-8.
  if (code < min || (code >= 0xD800u && code <= 0xDFFFu) || code > 0x10FFFFu) return 0;
  return length;
}

// Turns "{% elif" into "{% else if": SkyrimNet's engine and Jinja accept elif, stock inja 3.4 only "else if". The
// line count is kept, so errors still point at the right line.
void desugar_elif(std::string& text) {
  for (std::size_t i = text.find("{%"); i != std::string::npos; i = text.find("{%", i + 2)) {
    std::size_t j = i + 2;
    if (j < text.size() && (text[j] == '-' || text[j] == '+')) ++j;
    while (j < text.size() && is_space(text[j])) ++j;
    if (text.compare(j, 4, "elif") == 0 && (j + 4 == text.size() || !is_identifier_char(text[j + 4]))) {
      text.replace(j, 4, "else if");
    }
  }
}

}  // namespace

bool is_identifier(std::string_view text) noexcept {
  if (text.empty() || is_digit(text.front())) return false;
  return std::all_of(text.begin(), text.end(), is_identifier_char);
}

std::size_t utf8_length(std::string_view text) noexcept {
  std::size_t count = 0;
  for (char c : text) {
    if (!is_continuation(static_cast<unsigned char>(c))) ++count;
  }
  return count;
}

std::vector<std::string> utf8_characters(std::string_view text) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < text.size()) {
    std::size_t length = utf8_sequence(text, i);
    if (length == 0) length = 1;  // a stray byte stays one item rather than being lost
    out.emplace_back(text.substr(i, length));
    i += length;
  }
  return out;
}

std::vector<std::string> split_dots(std::string_view dotted) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (;;) {
    const std::size_t dot = dotted.find('.', start);
    out.emplace_back(dotted.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start));
    if (dot == std::string_view::npos) break;
    start = dot + 1;
  }
  return out;
}

std::string dotted_pointer(std::string_view dotted) {
  std::string out;
  for (const std::string& segment : split_dots(dotted)) {
    out += '/';
    for (char c : segment) {
      if (c == '~') out += "~0";
      else if (c == '/') out += "~1";
      else out += c;
    }
  }
  return out;
}

Site site_at(const std::string& name, std::string_view content, std::size_t pos) {
  const inja::SourceLocation location = inja::get_source_location(content, std::min(pos, content.size()));
  return Site{name, location.line, location.column};
}

std::string prepare_source(std::string_view raw, const std::string& name, std::vector<Finding>& findings) {
  if (raw.substr(0, 3) == "\xEF\xBB\xBF") raw.remove_prefix(3);  // a byte-order mark from a Windows editor
  std::string text;
  text.reserve(raw.size());
  for (std::size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n') continue;  // the same bytes on every system
    text.push_back(raw[i]);
  }

  for (std::size_t i = 0; i < text.size();) {
    const std::size_t length = utf8_sequence(text, i);
    if (length == 0) {
      findings.push_back({site_at(name, text, i), "invalid-utf8", "the template is not valid UTF-8 here"});
      break;
    }
    i += length;
  }

  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto c = static_cast<unsigned char>(text[i]);
    if ((c < 0x20u && c != '\t' && c != '\n') || c == 0x7Fu) {
      findings.push_back({site_at(name, text, i), "control-character",
                          "the template holds a control character (code " + std::to_string(c) + ")"});
      break;
    }
  }

  // inja ends an unclosed comment at the end of the text without a word, dropping everything after it.
  for (std::size_t i = text.find("{#"); i != std::string::npos; i = text.find("{#", i + 2)) {
    if (i > 0 && text[i - 1] == '{') continue;  // "{{#" opens an expression, not a comment
    const std::size_t close = text.find("#}", i + 2);
    if (close == std::string::npos) {
      findings.push_back({site_at(name, text, i), "unclosed-comment",
                          "a comment opened with {# never closes with #}: everything after it would be lost"});
      break;
    }
    i = close;
  }

  desugar_elif(text);
  return text;
}

namespace {

std::size_t edit_distance(std::string_view a, std::string_view b) {
  std::vector<std::size_t> row(b.size() + 1);
  for (std::size_t j = 0; j <= b.size(); ++j) row[j] = j;
  for (std::size_t i = 1; i <= a.size(); ++i) {
    std::size_t diagonal = row[0];
    row[0] = i;
    for (std::size_t j = 1; j <= b.size(); ++j) {
      const std::size_t above = row[j];
      const std::size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
      row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + cost});
      diagonal = above;
    }
  }
  return row[b.size()];
}

}  // namespace

std::string closest_name(std::string_view name, const FunctionRegistry& registry) {
  std::string best;
  std::size_t best_distance = 3;  // suggest only close misspellings
  for (const auto& spec : registry.all()) {
    const std::size_t distance = edit_distance(name, spec->name);
    if (distance < best_distance && distance < std::max<std::size_t>(name.size() / 2, 2)) {
      best = spec->name;
      best_distance = distance;
    }
  }
  return best;
}

}  // namespace en::prompt::detail
