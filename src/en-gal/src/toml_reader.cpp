// Strict reading of gal/'s TOML files (toml_reader.hpp), and describe() for their problems (schema.hpp).
#include "toml_reader.hpp"

#include <algorithm>

#include "en/gal/schema.hpp"

namespace en::gal {

std::string describe(const SchemaError& error) { return error.where + ": " + error.problem; }

namespace toml_read {

void Errors::add(const std::string& path, std::string problem) {
  if (errors_.size() >= kMaxSchemaErrors) {
    ++dropped_;
    return;
  }
  errors_.push_back(SchemaError{path.empty() ? file_ : file_ + ": " + path, std::move(problem)});
}

SchemaErrors Errors::take() {
  if (dropped_ > 0) {
    errors_.push_back(SchemaError{file_, "and " + std::to_string(dropped_) + " more problems"});
    dropped_ = 0;
  }
  return std::move(errors_);
}

namespace {
constexpr const char* kOutsideAscii =
    "a character outside ASCII may stand only in a comment; in a string, write it as \\uXXXX";
}  // namespace

std::optional<UnsafeText> unsafe_for_toml(std::string_view text) noexcept {
  const auto blank = [](char c) { return c == ' ' || c == '\t'; };
  const auto key_start = [](char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '"' || c == '\'';
  };
  enum class In { Text, Basic, Literal, Comment };
  In in = In::Text;
  char last = '\n';  // the last character outside strings and comments that is not a space, a tab or a line break
  const auto bare = [](char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '+' || c == '.' || c == ':';
  };
  bool in_bare = false;                                      // within a bare key or value
  bool digit_bare = false;                                   // and it began with a digit
  std::size_t i = text.starts_with("\xEF\xBB\xBF") ? 3 : 0;  // toml++ skips a byte-order mark
  std::size_t line = 1;
  bool line_start = true;
  for (; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\n' || c == '\r') {  // no string or comment of a gal/ file goes past its line
      if (c == '\n') ++line;
      in = In::Text;
      line_start = true;
      in_bare = false;
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
        if (k < text.size() && !key_start(text[k]) && text[k] != ']') {
          return UnsafeText{line, "a line that starts with [ must open a table header with a key"};
        }
      }
    }
    switch (in) {
      case In::Text:
        if (bare(c)) {
          if (!in_bare) {
            in_bare = true;
            digit_bare = c >= '0' && c <= '9';
          } else if (digit_bare && (c == '-' || c == ':')) {
            return UnsafeText{line,
                              "a number with - or : after its first digit (a date or a time) is not used in "
                              "gal/'s files"};
          }
        } else {
          in_bare = false;
        }
        if (c == '#') {
          in = In::Comment;
        } else if (c == '"' || c == '\'') {
          if (text.substr(i, 3) == std::string_view(c == '"' ? "\"\"\"" : "\'\'\'")) {
            return UnsafeText{line, "multi-line strings are not used in gal/'s files"};
          }
          in = c == '"' ? In::Basic : In::Literal;
          last = c;
        } else if (static_cast<unsigned char>(c) >= 0x80) {
          return UnsafeText{line, kOutsideAscii};
        } else if (c == '}' && (last == '[' || last == ',')) {
          return UnsafeText{line, "a } stands where a value must"};
        } else if (!blank(c)) {
          last = c;
        }
        break;
      case In::Basic:
        if (static_cast<unsigned char>(c) >= 0x80) {
          return UnsafeText{line, kOutsideAscii};
        } else if (c == '\\' && i + 1 < text.size() && text[i + 1] != '\n' && text[i + 1] != '\r') {
          ++i;  // the escaped character, whatever it is, does not end the string
        } else if (c == '"') {
          in = In::Text;
        }
        break;
      case In::Literal:
        if (static_cast<unsigned char>(c) >= 0x80) return UnsafeText{line, kOutsideAscii};
        if (c == '\'') in = In::Text;
        break;
      case In::Comment: break;
    }
  }
  return std::nullopt;
}

std::optional<toml::table> parse(std::string_view text, Errors& errors) {
  if (text.size() > kMaxGalFileBytes) {
    errors.add("", "is larger than " + std::to_string(kMaxGalFileBytes >> 10) + " KiB");
    return std::nullopt;
  }
  if (const std::optional<UnsafeText> unsafe = unsafe_for_toml(text)) {
    errors.add("", "is refused before it is parsed: line " + std::to_string(unsafe->line) + ": " + unsafe->problem);
    return std::nullopt;
  }
  toml::parse_result result = toml::parse(text);
  if (!result) {
    const toml::parse_error& e = result.error();
    errors.add("", "is not valid TOML: line " + std::to_string(e.source().begin.line) + ", column " +
                       std::to_string(e.source().begin.column) + ": " + std::string(e.description()));
    return std::nullopt;
  }
  return std::move(result).table();
}

std::string indexed(std::string_view base, std::size_t i) { return std::string(base) + "[" + std::to_string(i) + "]"; }

std::string shown(std::string_view name) {
  const bool plain = !name.empty() && name.size() <= 40 && std::all_of(name.begin(), name.end(), [](char c) {
    return c >= 0x20 && c < 0x7F && c != '"' && c != '\\';
  });
  return plain ? "\"" + std::string(name) + "\"" : "with a long or unprintable name";
}

std::string Table::where(std::string_view key) const {
  return path_.empty() ? std::string(key) : path_ + "." + std::string(key);
}

const toml::node* Table::find(std::string_view key, bool required) {
  known_.emplace_back(key);
  const toml::node* n = t_.get(key);
  if (n == nullptr && required) errors_.add(where(key), "is missing");
  return n;
}

std::optional<std::string> Table::text(std::string_view key, bool required) {
  const toml::node* n = find(key, required);
  if (n == nullptr) return std::nullopt;
  if (const auto* s = n->as_string()) return s->get();
  errors_.add(where(key), "must be a string");
  return std::nullopt;
}

std::optional<std::int64_t> Table::integer(std::string_view key, bool required, std::int64_t lo, std::int64_t hi) {
  const toml::node* n = find(key, required);
  if (n == nullptr) return std::nullopt;
  return integer_of(*n, where(key), errors_, lo, hi);
}

std::optional<bool> Table::boolean(std::string_view key, bool required) {
  const toml::node* n = find(key, required);
  if (n == nullptr) return std::nullopt;
  if (const auto* b = n->as_boolean()) return b->get();
  errors_.add(where(key), "must be true or false");
  return std::nullopt;
}

const toml::table* Table::table(std::string_view key, bool required) {
  const toml::node* n = find(key, required);
  if (n == nullptr) return nullptr;
  if (const auto* t = n->as_table()) return t;
  errors_.add(where(key), "must be a table");
  return nullptr;
}

const toml::array* Table::array(std::string_view key, bool required) {
  const toml::node* n = find(key, required);
  if (n == nullptr) return nullptr;
  if (const auto* a = n->as_array()) return a;
  errors_.add(where(key), "must be an array");
  return nullptr;
}

const toml::node* Table::node(std::string_view key, bool required) { return find(key, required); }

void Table::done() {
  for (auto&& [key, value] : t_) {
    (void)value;
    if (std::find(known_.begin(), known_.end(), key.str()) == known_.end()) {
      errors_.add(path_, "has an unknown key " + shown(key.str()));
    }
  }
}

std::optional<std::int64_t> integer_of(const toml::node& node, const std::string& path, Errors& errors, std::int64_t lo,
                                       std::int64_t hi) {
  const auto* i = node.as_integer();
  if (i == nullptr) {
    errors.add(path, "must be a whole number");
    return std::nullopt;
  }
  const std::int64_t v = i->get();
  if (v < lo || v > hi) {
    errors.add(path, "must be between " + std::to_string(lo) + " and " + std::to_string(hi));
    return std::nullopt;
  }
  return v;
}

std::optional<std::pair<std::int64_t, std::int64_t>> range_of(const toml::node& node, const std::string& path,
                                                              Errors& errors, std::int64_t min, std::int64_t max) {
  const auto* a = node.as_array();
  if (a == nullptr || a->size() != 2) {
    errors.add(path, "must be an array of two whole numbers, [lowest, highest]");
    return std::nullopt;
  }
  const auto lo = integer_of(*a->get(0), indexed(path, 0), errors, min, max);
  const auto hi = integer_of(*a->get(1), indexed(path, 1), errors, min, max);
  if (!lo || !hi) return std::nullopt;
  if (*lo > *hi) {
    errors.add(path, "has its lowest above its highest");
    return std::nullopt;
  }
  return std::pair{*lo, *hi};
}

std::optional<std::vector<std::string>> strings_of(const toml::array& array, const std::string& path, Errors& errors,
                                                   std::size_t max_items, std::size_t max_bytes) {
  if (array.size() > max_items) {
    errors.add(path, "has more than " + std::to_string(max_items) + " items");
    return std::nullopt;
  }
  std::vector<std::string> out;
  bool ok = true;
  for (std::size_t i = 0; i < array.size(); ++i) {
    const auto* s = array.get(i)->as_string();
    if (s == nullptr || s->get().empty() || s->get().size() > max_bytes) {
      errors.add(indexed(path, i), "must be a string of 1 to " + std::to_string(max_bytes) + " bytes");
      ok = false;
      continue;
    }
    out.push_back(s->get());
  }
  if (!ok) return std::nullopt;
  return out;
}

bool is_identifier(std::string_view s, std::size_t max_bytes) noexcept {
  if (s.empty() || s.size() > max_bytes) return false;
  return std::all_of(s.begin(), s.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
  });
}

bool is_date(std::string_view s) noexcept {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (i == 4 || i == 7) continue;
    if (s[i] < '0' || s[i] > '9') return false;
  }
  const int month = (s[5] - '0') * 10 + (s[6] - '0');
  const int day = (s[8] - '0') * 10 + (s[9] - '0');
  return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

bool is_relative_path(std::string_view s) noexcept {
  if (s.empty() || s.size() > 200) return false;
  std::size_t start = 0;
  while (start <= s.size()) {
    const std::size_t slash = s.find('/', start);
    const std::string_view part = s.substr(start, slash == std::string_view::npos ? s.npos : slash - start);
    if (part.empty() || part.front() == '.') return false;  // also "." and ".."
    for (const char c : part) {
      const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                      c == '_' || c == '-';
      if (!ok) return false;
    }
    if (slash == std::string_view::npos) break;
    start = slash + 1;
  }
  return true;
}

bool is_build_key(std::string_view s) noexcept {
  const std::size_t dash = s.rfind('-');
  if (dash == std::string_view::npos) return false;
  const std::string_view region = s.substr(dash + 1);
  if (region != "ww" && region != "jp") return false;
  const std::string_view version = s.substr(0, dash);
  int fields = 0;
  std::size_t digits = 0;
  for (const char c : version) {
    if (c == '.') {
      if (digits == 0) return false;
      ++fields;
      digits = 0;
    } else if (c >= '0' && c <= '9') {
      if (++digits > 5) return false;
    } else {
      return false;
    }
  }
  return digits > 0 && fields == 3;
}

}  // namespace toml_read
}  // namespace en::gal
