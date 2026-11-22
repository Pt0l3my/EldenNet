// The lint (NPC §8.3, §8.4) and the deflection ring: speech_lint.hpp has the rules.
#include <algorithm>
#include <array>
#include <numeric>
#include <set>
#include <string>
#include <utility>

#include "en/core/speech_lint.hpp"
#include "sha256.hpp"
#include "text.hpp"

namespace en::core {
namespace {

// How many times an entry's words stand in `words`, in order and whole.
std::size_t occurrences(const std::vector<std::string>& words, const LexiconEntry& e) {
  if (e.words.empty() || words.size() < e.words.size()) return 0;
  std::size_t n = 0;
  for (std::size_t i = 0; i + e.words.size() <= words.size(); ++i) {
    bool same = true;
    for (std::size_t k = 0; k < e.words.size() && same; ++k) {
      const std::string& w = words[i + k];
      const std::string& want = e.words[k];
      const bool last = k + 1 == e.words.size();
      same = (last && e.prefix) ? w.compare(0, want.size(), want) == 0 : w == want;
    }
    n += same ? 1 : 0;
  }
  return n;
}

bool any_of(const std::vector<std::string>& words, const std::vector<LexiconEntry>& list) {
  return std::any_of(list.begin(), list.end(), [&](const LexiconEntry& e) { return occurrences(words, e) > 0; });
}

LexiconEntry entry_of(std::string_view text) { return LexiconEntry{lint_words(text), false}; }

bool punctuation(char c) { return c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':' || c == '-'; }

// A token of the sanitized text without its punctuation at either end, and whether it had some after it. The em and en
// dashes are three bytes each in UTF-8, so they are trimmed as strings.
std::string_view core_of(std::string_view token, bool& trailing) {
  trailing = false;
  for (;;) {
    if (!token.empty() && punctuation(token.back())) {
      token.remove_suffix(1);
      trailing = true;
    } else if (token.size() >= 3 &&
               (token.substr(token.size() - 3) == "\xE2\x80\x94" || token.substr(token.size() - 3) == "\xE2\x80\x93")) {
      token.remove_suffix(3);
      trailing = true;
    } else {
      break;
    }
  }
  while (!token.empty() && (punctuation(token.front()) || token.front() == '\'')) token.remove_prefix(1);
  return token;
}

std::vector<std::string_view> tokens_of(std::string_view text) {
  std::vector<std::string_view> out;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && text[i] == ' ') ++i;
    std::size_t j = i;
    while (j < text.size() && text[j] != ' ') ++j;
    if (j > i) out.push_back(text.substr(i, j - i));
    i = j;
  }
  return out;
}

std::string lowered(std::string_view s) {
  const std::vector<std::string> w = lint_words(s);
  std::string out;
  for (const std::string& x : w) out += x;
  return out;
}

std::string capitalised(std::string text) {
  if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') text[0] = static_cast<char>(text[0] - 32);
  return text;
}

class Reply final : public ReplyLint {
 public:
  Reply(std::shared_ptr<const Lexicon> lex, const LintContext& c)
      : lex_(std::move(lex)),
        regeneration_(c.regeneration),
        gentle_(c.safety == Safety::Gentle),
        archaic_(c.archaic),
        last_opener_(c.tally.opener),
        tics_used_(c.tally.tics),
        name_uses_(c.tally.name_uses),
        spent_(c.tally.spent) {
    for (const std::string& o : c.openers) openers_.push_back(lowered(o));
    for (std::size_t i = 0; i < c.tics.size() && i < tics_used_.size(); ++i) {
      tics_.push_back(entry_of(c.tics[i]));
      tic_max_.push_back(i < c.tic_max.size() && c.tic_max[i] > 0 ? c.tic_max[i] : 1);
    }
    for (const std::string& n : c.names) {
      for (std::string& w : lint_words(n)) known_.insert(std::move(w));
    }
    for (const std::string& n : c.self_names) {
      LexiconEntry e = entry_of(n);
      for (const std::string& w : e.words) known_.insert(w);
      if (!e.words.empty()) self_.push_back(std::move(e));
    }
    if (!c.told_name.empty()) {
      told_ = entry_of(c.told_name);
      for (const std::string& w : told_.words) known_.insert(w);
    }
  }

  LintVerdict check(Sentence& sentence) override {
    if (terminal_) return *terminal_;
    LintVerdict v;
    const std::vector<std::string> words = lint_words(sentence.text);
    if (const std::optional<HardList> hit = hard(sentence.text, words)) {
      v.hit = hit;
      v.action = passed_ == 0 ? (regeneration_ ? LintAction::Deflect : LintAction::Regenerate) : LintAction::Stop;
      terminal_ = LintVerdict{LintAction::Stop, hit, {}};  // the reply has ended: what follows is not shown
      return v;
    }
    if (leading_) openers(sentence, v);
    if (lint_words(sentence.text).empty()) {  // nothing left but the stripped interjections
      v.action = LintAction::Drop;
      return v;
    }
    const std::string key = lowered(sentence.text);
    if (std::find(seen_.begin(), seen_.end(), key) != seen_.end()) {
      v.soft.push_back(SoftHit::Duplicate);
      v.action = LintAction::Drop;
      return v;
    }
    seen_.push_back(key);
    soft(sentence.text, lint_words(sentence.text), v);
    ++passed_;
    return v;
  }

  [[nodiscard]] LintTally tally() const override {
    LintTally t;
    t.opener = opener_used_;
    t.tics = tics_used_;
    t.name_uses = name_uses_;
    t.spent = spent_;
    return t;
  }

 private:
  std::optional<HardList> hard(std::string_view text, const std::vector<std::string>& words) const {
    const std::vector<std::string> sw = safety_words(text);
    for (std::size_t i = 0; i < sw.size(); ++i) {
      std::string phrase(kSafetyDomain);
      for (std::size_t k = 0; k < kSafetyPhraseWords && i + k < sw.size(); ++k) {
        if (k > 0) phrase += ' ';
        phrase += sw[i + k];
        const std::string fp = speech::sha256_hex(phrase);
        if (std::binary_search(lex_->safety_always.begin(), lex_->safety_always.end(), fp)) return HardList::Safety;
        if (gentle_ && std::binary_search(lex_->safety_gentle.begin(), lex_->safety_gentle.end(), fp)) {
          return HardList::Safety;
        }
      }
    }
    if (any_of(words, lex_->promise)) return HardList::Promise;
    if (any_of(words, lex_->banned)) return HardList::Banned;
    return std::nullopt;
  }

  // The reply's leading interjections (NPC §3.3's opener rule): at most one, from the persona's openers, not the last
  // reply's; the others are stripped. A sentence of interjections alone keeps the next one leading.
  void openers(Sentence& sentence, LintVerdict& v) {
    const std::vector<std::string_view> tokens = tokens_of(sentence.text);
    std::size_t k = 0;
    std::vector<bool> strip;
    for (; k < tokens.size(); ++k) {
      bool trailing = false;
      const std::string w = lowered(core_of(tokens[k], trailing));
      if (w.empty() || (!trailing && k + 1 < tokens.size())) break;
      const auto own = std::find(openers_.begin(), openers_.end(), w);
      const bool global =
          std::find(lex_->interjections.begin(), lex_->interjections.end(), w) != lex_->interjections.end();
      if (own == openers_.end() && !global) break;
      if (own == openers_.end()) {
        v.soft.push_back(SoftHit::Opener);  // the global set's, not the persona's
        strip.push_back(true);
        continue;
      }
      const auto index = static_cast<std::uint8_t>(own - openers_.begin());
      if (used_any_) {
        v.soft.push_back(SoftHit::Opener);  // a second one
        strip.push_back(true);
      } else if (last_opener_ && *last_opener_ == index) {
        v.soft.push_back(SoftHit::RepeatedOpener);
        strip.push_back(true);
        used_any_ = true;  // the reply began with one, though it was stripped
      } else {
        opener_used_ = index;
        used_any_ = true;
        strip.push_back(false);
      }
    }
    leading_ = k == tokens.size();  // every word an interjection: the next sentence still leads
    if (std::none_of(strip.begin(), strip.end(), [](bool s) { return s; })) return;
    std::string text;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
      if (i < strip.size() && strip[i]) continue;
      if (!text.empty()) text += ' ';
      text += tokens[i];
    }
    sentence.text = capitalised(std::move(text));
  }

  void soft(std::string_view text, const std::vector<std::string>& words, LintVerdict& v) {
    for (std::size_t i = 0; i < tics_.size(); ++i) {
      const std::size_t n = occurrences(words, tics_[i]);
      if (n == 0) continue;
      const bool was_spent = tics_used_[i] >= tic_max_[i];
      tics_used_[i] = static_cast<std::uint8_t>(std::min<std::size_t>(255, tics_used_[i] + n));
      if (!was_spent && tics_used_[i] >= tic_max_[i]) spent_ = static_cast<std::uint8_t>(i);  // the newest spent
      if (tics_used_[i] > tic_max_[i]) v.soft.push_back(SoftHit::TicOverCap);
    }
    if (!told_.words.empty()) {
      const std::size_t n = occurrences(words, told_);
      if (n > 0) {
        name_uses_ = static_cast<std::uint8_t>(std::min<std::size_t>(255, name_uses_ + n));
        if (name_uses_ > 1) v.soft.push_back(SoftHit::NameTwice);
      }
    }
    if (any_of(words, lex_->wider)) v.soft.push_back(SoftHit::WiderPromise);
    if (unknown_noun(text)) v.soft.push_back(SoftHit::UnknownNoun);
    if (!archaic_ && any_of(words, lex_->archaic)) v.soft.push_back(SoftHit::Archaic);
    if (third_person(words)) v.soft.push_back(SoftHit::ThirdPerson);
  }

  // A capitalised word that is not the sentence's first, not I or O, not in capitals throughout, and in none of the
  // names this turn shows.
  bool unknown_noun(std::string_view text) const {
    const std::vector<std::string_view> tokens = tokens_of(text);
    for (std::size_t k = 1; k < tokens.size(); ++k) {
      bool trailing = false;
      const std::string_view t = core_of(tokens[k], trailing);
      if (t.empty()) continue;
      std::vector<speech::Cp> cps;
      speech::decode_utf8(t, true, cps);
      if (cps.empty() || !speech::is_upper(cps[0].cp)) continue;
      const bool capitals = cps.size() > 1 && std::all_of(cps.begin(), cps.end(),
                                                          [](const speech::Cp& c) { return !speech::is_lower(c.cp); });
      if (capitals) continue;
      const std::vector<std::string> parts = lint_words(t);
      if (parts.empty()) continue;
      const std::string& w = parts.front();
      if (w == "i" || w == "i'm" || w == "i'll" || w == "i've" || w == "i'd" || w == "o") continue;
      if (std::all_of(parts.begin(), parts.end(), [&](const std::string& p) { return known_.count(p) > 0; })) continue;
      return true;
    }
    return false;
  }

  // One of the persona's own names, not where it names itself ("I am", "call me", "my name is").
  bool third_person(const std::vector<std::string>& words) const {
    static constexpr std::array<std::string_view, 8> kNaming = {"am",     "i'm",   "me", "is",
                                                                "called", "named", "as", "myself"};
    for (const LexiconEntry& self : self_) {
      for (std::size_t i = 0; i + self.words.size() <= words.size(); ++i) {
        if (!std::equal(self.words.begin(), self.words.end(), words.begin() + static_cast<std::ptrdiff_t>(i))) continue;
        const bool naming =
            i > 0 && std::find(kNaming.begin(), kNaming.end(), std::string_view(words[i - 1])) != kNaming.end();
        if (!naming) return true;
      }
    }
    return false;
  }

  std::shared_ptr<const Lexicon> lex_;
  bool regeneration_;
  bool gentle_;
  bool archaic_;
  std::optional<std::uint8_t> last_opener_;
  std::array<std::uint8_t, 3> tics_used_;
  std::uint8_t name_uses_;
  std::optional<std::uint8_t> spent_;
  std::vector<std::string> openers_;
  std::vector<LexiconEntry> tics_;
  std::vector<std::uint8_t> tic_max_;
  std::set<std::string> known_;
  std::vector<LexiconEntry> self_;
  LexiconEntry told_;
  std::optional<LintVerdict> terminal_;
  std::vector<std::string> seen_;
  std::size_t passed_ = 0;
  bool leading_ = true;
  bool used_any_ = false;
  std::optional<std::uint8_t> opener_used_;
};

}  // namespace

SpeechLint::SpeechLint(Lexicon lexicon) : lexicon_(std::make_shared<const Lexicon>(std::move(lexicon))) {}

std::unique_ptr<ReplyLint> SpeechLint::open(const LintContext& context) const {
  return std::make_unique<Reply>(lexicon_, context);
}

DeflectionRing::DeflectionRing(std::size_t count) {
  count = std::min<std::size_t>(count, 255);
  order_.resize(count);
  std::iota(order_.begin(), order_.end(), std::uint8_t{0});
}

DeflectionRing::DeflectionRing(std::size_t count, std::vector<std::uint8_t> order) : DeflectionRing(count) {
  std::vector<std::uint8_t> sorted = order;
  std::sort(sorted.begin(), sorted.end());
  if (sorted == order_) order_ = std::move(order);
}

std::optional<std::size_t> DeflectionRing::next() {
  if (order_.empty()) return std::nullopt;
  const std::uint8_t i = order_.front();
  order_.erase(order_.begin());
  order_.push_back(i);
  return i;
}

}  // namespace en::core
