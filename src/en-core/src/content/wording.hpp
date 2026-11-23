#pragma once
// The wording checks of NPC §4.8's PL4 and PL5 that need the lexicon, shared by the shipped content's lint
// (speech_content.cpp) and the player's layers (layer.cpp), so that both read one list of effect words and imperatives
// and match a lint entry the same way. Internal to en-core.
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "../speech/sha256.hpp"
#include "en/core/speech_lint.hpp"

namespace en::core::wording {

// NPC §3.3's effect adjectives: a warning in the persona's voice, public and personality fields and the instructions
// (PL5).
inline constexpr std::array<std::string_view, 19> kEffectWords = {
    "cryptic", "enigmatic", "mysterious", "riddling", "terse",     "clipped", "curt",  "blunt", "laconic", "gruff",
    "sardonic", "sarcastic", "snarky",    "witty",    "quirky",    "sassy",   "edgy",  "brooding", "ominous"};
// PL4's imperative words: a subject or a stir is a noun phrase, never an instruction (W7's list).
inline constexpr std::array<std::string_view, 7> kImperatives = {"tell", "mention", "say", "ask", "remind", "must",
                                                                 "should"};

// Whether a lint entry's words occur in order in the text's words (an entry's last word may be a prefix).
inline bool matches(const LexiconEntry& e, const std::vector<std::string>& words) {
  if (e.words.empty() || e.words.size() > words.size()) return false;
  for (std::size_t i = 0; i + e.words.size() <= words.size(); ++i) {
    bool all = true;
    for (std::size_t j = 0; j < e.words.size() && all; ++j) {
      const bool last = j + 1 == e.words.size();
      all = (last && e.prefix) ? words[i + j].rfind(e.words[j], 0) == 0 : words[i + j] == e.words[j];
    }
    if (all) return true;
  }
  return false;
}

inline bool banned(const Lexicon& lexicon, std::string_view text) {
  const std::vector<std::string> words = lint_words(text);
  return std::any_of(lexicon.banned.begin(), lexicon.banned.end(), [&](const LexiconEntry& e) { return matches(e, words); });
}

inline bool has_quote(std::string_view s) {
  return s.find('"') != std::string_view::npos || s.find("\xE2\x80\x9C") != std::string_view::npos;  // " or “
}

inline bool has_effect_word(std::string_view s) {
  const std::vector<std::string> words = lint_words(s);
  return std::any_of(words.begin(), words.end(), [](const std::string& w) {
    return std::find(kEffectWords.begin(), kEffectWords.end(), w) != kEffectWords.end();
  });
}

// PL4: a noun phrase has no sentence's end, no question, no quotation and no imperative.
inline bool noun_phrase(std::string_view t) {
  const char last = t.empty() ? ' ' : t.back();
  const std::vector<std::string> words = lint_words(t);
  const bool imperative = std::any_of(words.begin(), words.end(), [](const std::string& w) {
    return std::find(kImperatives.begin(), kImperatives.end(), w) != kImperatives.end();
  });
  return !(last == '.' || last == '!' || t.find('?') != std::string_view::npos || has_quote(t) || imperative);
}

// PL5's template marks: text a prompt's template would read as its own.
inline bool has_template_mark(std::string_view s) {
  return s.find("{{") != std::string_view::npos || s.find("{%") != std::string_view::npos ||
         s.find("{#") != std::string_view::npos;
}

// A word or a run of two or three of the safety list's `always` fingerprints (O107: refused at both levels): the reply
// lint's own lookup.
inline bool safety_hit(const Lexicon& lexicon, std::string_view text) {
  const std::vector<std::string> sw = safety_words(text);
  for (std::size_t i = 0; i < sw.size(); ++i) {
    std::string phrase(kSafetyDomain);
    for (std::size_t k = 0; k < kSafetyPhraseWords && i + k < sw.size(); ++k) {
      if (k > 0) phrase += ' ';
      phrase += sw[i + k];
      if (std::binary_search(lexicon.safety_always.begin(), lexicon.safety_always.end(), speech::sha256_hex(phrase))) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace en::core::wording
