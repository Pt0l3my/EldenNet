#pragma once
// The content's schema checks, shared by the persona files (content.cpp) and the world files (knowledge.cpp): errors in
// fixed words with the file, the line and the key, never a value of the file (content.hpp); NPC §3.1's kinds of field,
// their caps in words and characters, closed values, counts and nested tables; the TOML pre-check and parse; TOML as
// JSON. Internal to en-core.
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <toml.hpp>

#include "en/core/content.hpp"
#include "en/proto/toml_check.hpp"

namespace en::core::schema {

constexpr std::size_t kMaxErrors = 64;

class Errors {
 public:
  explicit Errors(std::vector<ContentError>& out) : out_(out) {}
  void add(const std::string& file, std::size_t line, std::string problem) {
    if (out_.size() >= kMaxErrors) {
      ++dropped_;
      return;
    }
    out_.push_back(ContentError{file, line, std::move(problem)});
  }
  void finish() {
    if (dropped_ > 0) out_.push_back(ContentError{"", 0, "and " + std::to_string(dropped_) + " more problems"});
  }
  [[nodiscard]] std::size_t count() const { return out_.size() + dropped_; }

 private:
  std::vector<ContentError>& out_;
  std::size_t dropped_ = 0;
};

inline std::size_t line_of(const toml::node& n) { return static_cast<std::size_t>(n.source().begin.line); }

// UTF-8 code points, and words as runs of characters between spaces.
inline std::size_t chars_of(std::string_view s) {
  std::size_t n = 0;
  for (const char c : s) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
  }
  return n;
}

inline std::size_t words_of(std::string_view s) {
  std::size_t n = 0;
  bool in_word = false;
  for (const char c : s) {
    const bool space = c == ' ' || c == '\t' || c == '\n' || c == '\r';
    if (!space && !in_word) ++n;
    in_word = !space;
  }
  return n;
}

inline bool is_slug(std::string_view s, std::size_t max) {
  if (s.empty() || s.size() > max || !(s[0] >= 'a' && s[0] <= 'z')) return false;
  return std::all_of(s.begin(), s.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

// A word of the file that may be quoted back: short, printable ASCII. Anything else is named by kind only.
inline std::string shown(std::string_view s) {
  const bool plain = !s.empty() && s.size() <= 40 && std::all_of(s.begin(), s.end(), [](char c) {
    return c > 0x20 && c < 0x7F && c != '"' && c != '\\';
  });
  return plain ? "\"" + std::string(s) + "\"" : std::string("(a long or unprintable key)");
}

// ---- The schema ----

enum class Kind : std::uint8_t { Str, StrList, Int, Bool, Num, Table, TableList, Region, RegionList };

struct Spec;

struct Field {
  std::string_view key;
  Kind kind = Kind::Str;
  std::uint16_t max_words = 0;  // 0: no cap (per item for a list)
  std::uint16_t max_chars = 0;
  std::uint8_t min_count = 0;   // a list's, or a table list's
  std::uint8_t max_count = 0;   // 0: no cap
  std::int64_t lo = 0, hi = 0;  // an integer's bounds
  std::span<const std::string_view> one_of{};  // a string's (or each item's) closed values
  const Spec* sub = nullptr;    // a table's, or a table list's
  bool required = false;
  bool one_word = false;        // each item is one word
};

struct Spec {
  std::span<const Field> fields;
};

// Builders, so that every field states what it sets and nothing is left to aggregate order.
constexpr Field text(std::string_view key, std::uint16_t words = 0, std::uint16_t chars = 0) {
  Field f;
  f.key = key;
  f.kind = Kind::Str;
  f.max_words = words;
  f.max_chars = chars;
  return f;
}
constexpr Field choice(std::string_view key, std::span<const std::string_view> values) {
  Field f = text(key);
  f.one_of = values;
  return f;
}
constexpr Field list(std::string_view key, std::uint8_t min, std::uint8_t max, std::uint16_t words = 0,
                     std::uint16_t chars = 0) {
  Field f = text(key, words, chars);
  f.kind = Kind::StrList;
  f.min_count = min;
  f.max_count = max;
  return f;
}
constexpr Field words1(Field f) {
  f.one_word = true;
  return f;
}
constexpr Field integer(std::string_view key, std::int64_t lo, std::int64_t hi) {
  Field f = text(key);
  f.kind = Kind::Int;
  f.lo = lo;
  f.hi = hi;
  return f;
}
constexpr Field boolean(std::string_view key) {
  Field f = text(key);
  f.kind = Kind::Bool;
  return f;
}
constexpr Field number(std::string_view key) {
  Field f = text(key);
  f.kind = Kind::Num;
  return f;
}
constexpr Field region(std::string_view key) {
  Field f = text(key);
  f.kind = Kind::Region;
  return f;
}
constexpr Field regions(std::string_view key, std::uint8_t max) {
  Field f = list(key, 0, max);
  f.kind = Kind::RegionList;
  return f;
}
constexpr Field table(std::string_view key, const Spec* sub) {
  Field f = text(key);
  f.kind = Kind::Table;
  f.sub = sub;
  return f;
}
constexpr Field tables(std::string_view key, std::uint8_t min, std::uint8_t max, const Spec* sub) {
  Field f = table(key, sub);
  f.kind = Kind::TableList;
  f.min_count = min;
  f.max_count = max;
  return f;
}
constexpr Field required(Field f) {
  f.required = true;
  return f;
}

inline bool is_region(std::string_view s) { return is_slug(s, 32); }

class Checker {
 public:
  Checker(std::string file, Errors& errors) : file_(std::move(file)), errors_(errors) {}

  void error(const toml::node& at, const std::string& path, const std::string& problem) {
    errors_.add(file_, line_of(at), path + ": " + problem);
  }

  void string_caps(const toml::node& at, const std::string& path, const Field& f, std::string_view s) {
    if (f.max_words != 0 && words_of(s) > f.max_words) {
      error(at, path, "is longer than " + std::to_string(f.max_words) + (f.max_words == 1 ? " word" : " words"));
    }
    if (f.max_chars != 0 && chars_of(s) > f.max_chars) {
      error(at, path, "is longer than " + std::to_string(f.max_chars) + " characters");
    }
    if (f.one_word && words_of(s) != 1) error(at, path, "must be one word");
    if (!f.one_of.empty() && std::find(f.one_of.begin(), f.one_of.end(), s) == f.one_of.end()) {
      std::string names;
      for (const std::string_view v : f.one_of) names += (names.empty() ? "" : ", ") + std::string(v);
      error(at, path, "must be one of " + names);
    }
    if (s.empty()) error(at, path, "is empty");
  }

  void check(const toml::table& t, const Spec& spec, const std::string& where) {
    for (auto&& [k, v] : t) {
      const auto f = std::find_if(spec.fields.begin(), spec.fields.end(), [&](const Field& x) { return x.key == k.str(); });
      if (f == spec.fields.end()) error(v, where.empty() ? std::string("(top)") : where, "has an unknown key " + shown(k.str()));
    }
    for (const Field& f : spec.fields) {
      const std::string path = where.empty() ? std::string(f.key) : where + "." + std::string(f.key);
      const toml::node* n = t.get(f.key);
      if (n == nullptr) {
        if (f.required) errors_.add(file_, line_of(t), path + ": is missing");
        continue;
      }
      check_value(*n, f, path);
    }
  }

  void check_value(const toml::node& n, const Field& f, const std::string& path) {
    switch (f.kind) {
      case Kind::Str:
        if (const auto* s = n.as_string()) {
          string_caps(n, path, f, s->get());
        } else {
          error(n, path, "must be a string");
        }
        return;
      case Kind::Region:
        if (const auto* s = n.as_string(); s == nullptr || !is_region(s->get())) error(n, path, "must be a region key");
        return;
      case Kind::Int: {
        const auto* i = n.as_integer();
        if (i == nullptr || i->get() < f.lo || i->get() > f.hi) {
          error(n, path, "must be a whole number from " + std::to_string(f.lo) + " to " + std::to_string(f.hi));
        }
        return;
      }
      case Kind::Bool:
        if (!n.is_boolean()) error(n, path, "must be true or false");
        return;
      case Kind::Num:
        if (!n.is_number()) error(n, path, "must be a number");
        return;
      case Kind::Table:
        if (const auto* t = n.as_table()) {
          check(*t, *f.sub, path);
        } else {
          error(n, path, "must be a table");
        }
        return;
      case Kind::StrList:
      case Kind::RegionList:
      case Kind::TableList: {
        const auto* a = n.as_array();
        if (a == nullptr) {
          error(n, path, f.kind == Kind::TableList ? "must be an array of tables" : "must be an array of strings");
          return;
        }
        if (a->size() < f.min_count || (f.max_count != 0 && a->size() > f.max_count)) {
          error(n, path,
                "must hold " + (f.min_count == f.max_count
                                    ? std::to_string(f.min_count)
                                    : (f.max_count == 0 ? "at least " + std::to_string(f.min_count)
                                                        : std::to_string(f.min_count) + " to " + std::to_string(f.max_count))) +
                    " entries");
        }
        for (std::size_t i = 0; i < a->size(); ++i) {
          const toml::node& item = *a->get(i);
          const std::string at = path + "[" + std::to_string(i) + "]";
          if (f.kind == Kind::TableList) {
            if (const auto* t = item.as_table()) {
              check(*t, *f.sub, at);
            } else {
              error(item, at, "must be a table");
            }
          } else if (f.kind == Kind::RegionList) {
            if (const auto* s = item.as_string(); s == nullptr || !is_region(s->get())) error(item, at, "must be a region key");
          } else if (const auto* s = item.as_string()) {
            string_caps(item, at, f, s->get());
          } else {
            error(item, at, "must be a string");
          }
        }
        return;
      }
    }
  }

 private:
  std::string file_;
  Errors& errors_;
};

// ---- TOML to JSON ----

inline nlohmann::json to_json_value(const toml::node& n) {
  if (const auto* t = n.as_table()) {
    nlohmann::json j = nlohmann::json::object();
    for (auto&& [k, v] : *t) j[std::string(k.str())] = to_json_value(v);
    return j;
  }
  if (const auto* a = n.as_array()) {
    nlohmann::json j = nlohmann::json::array();
    for (const toml::node& v : *a) j.push_back(to_json_value(v));
    return j;
  }
  if (const auto* s = n.as_string()) return s->get();
  if (const auto* i = n.as_integer()) return i->get();
  if (const auto* f = n.as_floating_point()) return f->get();
  if (const auto* b = n.as_boolean()) return b->get();
  return nullptr;  // dates and times: refused before they get here (unsafe_for_toml)
}

inline std::optional<toml::table> parse(const ContentFile& file, std::size_t cap, Errors& errors) {
  if (file.text.size() > cap) {
    errors.add(file.path, 0, "is larger than " + std::to_string(cap >> 10) + " KB");
    return std::nullopt;
  }
  if (const auto unsafe = proto::unsafe_for_toml(file.text, proto::TomlText::Player)) {
    errors.add(file.path, unsafe->line, std::string("is refused before it is parsed: ") + unsafe->problem);
    return std::nullopt;
  }
  toml::parse_result parsed = toml::parse(file.text);
  if (!parsed) {
    errors.add(file.path, static_cast<std::size_t>(parsed.error().source().begin.line),
               "is not valid TOML: " + std::string(parsed.error().description()));
    return std::nullopt;
  }
  return std::move(parsed).table();
}

template <class E>
inline E enum_of(std::string_view s, std::span<const std::string_view> names, E fallback) {
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (names[i] == s) return static_cast<E>(i);
  }
  return fallback;
}

inline std::vector<std::string> strings_at(const toml::table& t, std::string_view key) {
  std::vector<std::string> out;
  if (const auto* a = t[key].as_array()) {
    for (const toml::node& v : *a) {
      if (const auto* s = v.as_string()) out.push_back(s->get());
    }
  }
  return out;
}


}  // namespace en::core::schema
