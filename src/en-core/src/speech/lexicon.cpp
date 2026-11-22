// The lexicon (NPC §8.3; O107): the safety list's normalisation and fingerprints, the lint's words, and the reader of
// banned.toml and safety.toml. speech_lint.hpp has the rules; content/eldenring/lexicon/README.md the files' format.
#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <toml.hpp>

#include "en/core/speech_lint.hpp"
#include "en/proto/toml_check.hpp"
#include "sha256.hpp"
#include "text.hpp"

namespace en::core {
namespace {

using speech::Cp;

bool separator(char32_t c) {
  switch (c) {
    case '.': case ',': case ';': case ':': case '-': case '_': case '*': case '`': case '~': case '\'':
    case U'\u2013': case U'\u2014': case U'\u2026': case U'\u2018': case U'\u2019': case U'\u02BC':
      return true;
    default:
      return false;
  }
}

char lookalike(char32_t c) {
  switch (c) {
    case '0': return 'o';
    case '1': return 'i';
    case '3': return 'e';
    case '4': return 'a';
    case '5': return 's';
    case '7': return 't';
    case '8': return 'b';
    case '@': return 'a';
    case '$': return 's';
    case '!': return 'i';
    case '|': return 'i';
    default: return 0;
  }
}

// A letter's lower-case ASCII form ("" for anything that is no letter).
std::string folded(char32_t c) {
  if (c >= 'A' && c <= 'Z') return std::string(1, static_cast<char>(c + 32));
  if (c >= 'a' && c <= 'z') return std::string(1, static_cast<char>(c));
  std::string s(speech::stripped(c));
  for (char& x : s) x = static_cast<char>(x >= 'A' && x <= 'Z' ? x + 32 : x);
  return s;
}

std::string squeezed(const std::string& w) {
  std::string out;
  for (const char c : w) {
    if (out.empty() || out.back() != c) out += c;
  }
  return out;
}

}  // namespace

std::vector<std::string> safety_words(std::string_view text) {
  std::vector<Cp> cps;
  speech::decode_utf8(text, true, cps);
  std::vector<std::string> words;
  std::size_t i = 0;
  const auto chunk_char = [](char32_t c) {
    return speech::is_letter(c) || speech::is_digit(c) || lookalike(c) != 0 || separator(c);
  };
  while (i < cps.size()) {
    if (!chunk_char(cps[i].cp)) {
      ++i;
      continue;
    }
    std::size_t j = i;
    while (j < cps.size() && chunk_char(cps[j].cp)) ++j;
    // The chunk [i, j): its separators and '!' at either end dropped.
    std::size_t a = i;
    std::size_t b = j;
    while (a < b && (separator(cps[a].cp) || cps[a].cp == '!')) ++a;
    while (b > a && (separator(cps[b - 1].cp) || cps[b - 1].cp == '!')) --b;
    bool letter = false;
    for (std::size_t k = a; k < b; ++k) letter = letter || speech::is_letter(cps[k].cp);
    std::string w;
    for (std::size_t k = a; k < b; ++k) {
      const char32_t c = cps[k].cp;
      if (speech::is_letter(c)) {
        w += folded(c);
      } else if (letter && lookalike(c) != 0) {
        w += lookalike(c);
      } else if (!letter && speech::is_digit(c)) {
        w += static_cast<char>(c);
      }
    }
    if (!w.empty()) words.push_back(std::move(w));
    i = j;
  }
  // Three or more one-letter words in a row are one word, spelled out.
  std::vector<std::string> joined;
  for (std::size_t k = 0; k < words.size();) {
    std::size_t e = k;
    while (e < words.size() && words[e].size() == 1) ++e;
    if (e - k >= 3) {
      std::string w;
      for (std::size_t x = k; x < e; ++x) w += words[x];
      joined.push_back(std::move(w));
      k = e;
    } else {
      joined.push_back(std::move(words[k]));
      ++k;
    }
  }
  for (std::string& w : joined) w = squeezed(w);
  return joined;
}

std::optional<std::string> safety_fingerprint(std::string_view phrase) {
  const std::vector<std::string> words = safety_words(phrase);
  if (words.empty() || words.size() > kSafetyPhraseWords) return std::nullopt;
  std::string text(kSafetyDomain);
  for (std::size_t k = 0; k < words.size(); ++k) {
    if (k > 0) text += ' ';
    text += words[k];
  }
  return speech::sha256_hex(text);
}

std::vector<std::string> lint_words(std::string_view text) {
  std::vector<Cp> cps;
  speech::decode_utf8(text, true, cps);
  std::vector<std::string> words;
  std::string w;
  const auto flush = [&] {
    while (!w.empty() && w.back() == '\'') w.pop_back();
    if (!w.empty()) words.push_back(std::move(w));
    w.clear();
  };
  for (const Cp& c : cps) {
    const bool apostrophe = c.cp == '\'' || c.cp == U'\u2018' || c.cp == U'\u2019' || c.cp == U'\u02BC';
    if (speech::is_letter(c.cp)) {
      w += folded(c.cp);
    } else if (speech::is_digit(c.cp)) {
      w += static_cast<char>(c.cp);
    } else if (apostrophe && !w.empty()) {
      w += '\'';
    } else {
      flush();
    }
  }
  flush();
  return words;
}

namespace {

LexiconError error(const char* file, std::size_t line, std::string problem) {
  return LexiconError{file, line, std::move(problem)};
}

std::size_t line_of(const toml::node& n) { return static_cast<std::size_t>(n.source().begin.line); }

// A list of words or phrases, each entry checked: a string, with words, at most one '*' and that at its end.
std::optional<LexiconError> read_entries(const toml::node& node, const char* file, const std::string& key,
                                         std::vector<LexiconEntry>& out) {
  const toml::array* a = node.as_array();
  if (a == nullptr) return error(file, line_of(node), key + " must be a list of strings");
  for (std::size_t i = 0; i < a->size(); ++i) {
    const toml::node& v = *a->get(i);
    const auto* s = v.as_string();
    if (s == nullptr) return error(file, line_of(v), key + "[" + std::to_string(i) + "] is not a string");
    std::string text = s->get();
    LexiconEntry e;
    if (!text.empty() && text.back() == '*') {
      e.prefix = true;
      text.pop_back();
    }
    if (text.find('*') != std::string::npos) {
      return error(file, line_of(v), key + "[" + std::to_string(i) + "] has a '*' before its end");
    }
    e.words = lint_words(text);
    if (e.words.empty()) return error(file, line_of(v), key + "[" + std::to_string(i) + "] has no word");
    out.push_back(std::move(e));
  }
  return std::nullopt;
}

std::optional<LexiconError> read_banned(std::string_view text, Lexicon& lex) {
  const char* file = "banned.toml";
  if (const auto unsafe = proto::unsafe_for_toml(text, proto::TomlText::Gal)) {
    return error(file, unsafe->line, std::string("is refused before it is parsed: ") + unsafe->problem);
  }
  toml::parse_result parsed = toml::parse(text);
  if (!parsed) {
    const toml::parse_error& e = parsed.error();
    return error(file, static_cast<std::size_t>(e.source().begin.line), "is not valid TOML: " + std::string(e.description()));
  }
  const toml::table& root = parsed.table();
  bool format = false;
  for (auto&& [k, v] : root) {
    const std::string key(k.str());
    if (key == "format") {
      const auto* f = v.as_integer();
      if (f == nullptr || f->get() != kLexiconFormat) return error(file, line_of(v), "format must be 1");
      format = true;
    } else if (key == "hard") {
      const toml::table* t = v.as_table();
      if (t == nullptr) return error(file, line_of(v), "hard must be a table of lists");
      for (auto&& [hk, hv] : *t) {
        if (auto e = read_entries(hv, file, "hard." + std::string(hk.str()), lex.banned)) return e;
      }
    } else if (key == "promise" || key == "soft") {
      const toml::table* t = v.as_table();
      if (t == nullptr) return error(file, line_of(v), key + " must be a table of lists");
      for (auto&& [sk, sv] : *t) {
        const std::string sub(sk.str());
        const std::string name = key + "." + sub;
        if (name == "promise.hard") {
          if (auto e = read_entries(sv, file, name, lex.promise)) return e;
        } else if (name == "promise.wider") {
          if (auto e = read_entries(sv, file, name, lex.wider)) return e;
        } else if (name == "soft.archaic") {
          if (auto e = read_entries(sv, file, name, lex.archaic)) return e;
        } else if (name == "soft.interjections") {
          std::vector<LexiconEntry> words;
          if (auto e = read_entries(sv, file, name, words)) return e;
          for (const LexiconEntry& w : words) {
            if (w.words.size() != 1 || w.prefix) return error(file, line_of(sv), name + " takes single words");
            lex.interjections.push_back(w.words.front());
          }
        } else {
          return error(file, line_of(sv), "has an unknown key " + name);
        }
      }
    } else {
      return error(file, line_of(v), "has an unknown key " + key);
    }
  }
  if (!format) return error(file, 0, "has no format");
  return std::nullopt;
}

std::optional<LexiconError> read_safety(std::string_view text, Lexicon& lex) {
  const char* file = "safety.toml";
  if (const auto unsafe = proto::unsafe_for_toml(text, proto::TomlText::Gal)) {
    return error(file, unsafe->line, std::string("is refused before it is parsed: ") + unsafe->problem);
  }
  toml::parse_result parsed = toml::parse(text);
  if (!parsed) {
    const toml::parse_error& e = parsed.error();
    return error(file, static_cast<std::size_t>(e.source().begin.line), "is not valid TOML: " + std::string(e.description()));
  }
  bool format = false;
  for (auto&& [k, v] : parsed.table()) {
    const std::string key(k.str());
    if (key == "format") {
      const auto* f = v.as_integer();
      if (f == nullptr || f->get() != kLexiconFormat) return error(file, line_of(v), "format must be 1");
      format = true;
      continue;
    }
    if (key != "always" && key != "gentle") return error(file, line_of(v), "has an unknown key " + key);
    std::vector<std::string>& into = key == "always" ? lex.safety_always : lex.safety_gentle;
    const toml::array* a = v.as_array();
    if (a == nullptr) return error(file, line_of(v), key + " must be a list of fingerprints");
    for (std::size_t i = 0; i < a->size(); ++i) {
      const toml::node& e = *a->get(i);
      const auto* s = e.as_string();
      const bool hex = s != nullptr && s->get().size() == 64 &&
                       std::all_of(s->get().begin(), s->get().end(),
                                   [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
      if (!hex) {
        return error(file, line_of(e),
                     key + "[" + std::to_string(i) + "] is not a fingerprint (64 lower-case hex digits)");
      }
      into.push_back(s->get());
    }
    std::sort(into.begin(), into.end());
    if (std::adjacent_find(into.begin(), into.end()) != into.end()) {
      return error(file, line_of(v), key + " holds a fingerprint twice");
    }
  }
  if (!format) return error(file, 0, "has no format");
  return std::nullopt;
}

}  // namespace

proto::Expected<Lexicon, LexiconError> parse_lexicon(std::string_view banned_toml, std::string_view safety_toml) {
  Lexicon lex;
  if (auto e = read_banned(banned_toml, lex)) return proto::Unexpected{std::move(*e)};
  if (auto e = read_safety(safety_toml, lex)) return proto::Unexpected{std::move(*e)};
  return lex;
}

}  // namespace en::core
