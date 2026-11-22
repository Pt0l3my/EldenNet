// The pager (NPC §8.5, §8.6): speech_pager.hpp has the rules.
#include <algorithm>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "en/core/speech_pager.hpp"
#include "text.hpp"

namespace en::core {
namespace {

std::size_t chars(std::string_view s) { return speech::utf8_chars(s); }

std::string_view trimmed(std::string_view s) {
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
  return s;
}

std::string joined(std::string_view a, std::string_view b) {
  if (a.empty()) return std::string(b);
  if (b.empty()) return std::string(a);
  std::string out(a);
  out += ' ';
  out += b;
  return out;
}

bool ends_with(std::string_view s, std::string_view tail) {
  return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

bool has_word_char(std::string_view token) {
  std::vector<speech::Cp> cps;
  speech::decode_utf8(token, true, cps);
  return std::any_of(cps.begin(), cps.end(),
                     [](const speech::Cp& c) { return speech::is_letter(c.cp) || speech::is_digit(c.cp); });
}

// A text ending at a cut: its trailing punctuation and spaces dropped, then "...".
std::string with_ellipsis(std::string_view s) {
  s = trimmed(s);
  for (;;) {
    if (!s.empty() && (s.back() == ',' || s.back() == ';' || s.back() == ':' || s.back() == '-' || s.back() == '.' ||
                       s.back() == ' ')) {
      s.remove_suffix(1);
    } else if (ends_with(s, "\xE2\x80\x94") || ends_with(s, "\xE2\x80\x93")) {
      s.remove_suffix(3);
    } else {
      break;
    }
  }
  return std::string(s) + "...";
}

// The byte offset just past the n-th word (counting as reply_words does), or the text's size.
std::size_t past_word(std::string_view text, std::size_t n) {
  std::size_t words = 0;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && text[i] == ' ') ++i;
    std::size_t j = i;
    while (j < text.size() && text[j] != ' ') ++j;
    if (j > i && has_word_char(text.substr(i, j - i)) && ++words == n) return j;
    i = j;
  }
  return text.size();
}

// The reply's first sentence, past the cap + 10 words: cut at its last ';', '—' or ',' within them, else at the word.
std::string cut_first(std::string_view text, std::size_t limit_words) {
  const std::size_t end = past_word(text, limit_words);
  const std::string_view head = text.substr(0, end);
  std::size_t mark = std::string_view::npos;
  for (const std::string_view m : {std::string_view(";"), std::string_view("\xE2\x80\x94"), std::string_view(",")}) {
    const std::size_t at = head.rfind(m);
    if (at != std::string_view::npos && at > 0 && (mark == std::string_view::npos || at > mark)) mark = at;
  }
  if (mark != std::string_view::npos && has_word_char(head.substr(0, mark))) return with_ellipsis(head.substr(0, mark));
  return with_ellipsis(head);
}

// The longest first part of whole words, ending in "...", within `max_bytes`.
std::string cut_bytes(std::string_view text, std::size_t max_bytes) {
  std::size_t cut = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == ' ' && i + 3 <= max_bytes) cut = i;
  }
  if (cut == 0) {  // one very long word: at a character boundary
    cut = std::min(text.size(), max_bytes - 3);
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;
  }
  std::string out = with_ellipsis(text.substr(0, cut));
  while (out.size() > max_bytes) out = with_ellipsis(std::string_view(out).substr(0, out.size() - 4));
  return out;
}

// The longest first part that fits a page alone, cut at a character (a last resort for a text with no space: the
// sanitizer's tokens are at most 24 characters, so it never needs this).
std::size_t hard_cut(std::string_view before, std::string_view s) {
  std::size_t best = 0;
  std::size_t i = 0;
  while (i < s.size()) {
    std::size_t j = i + 1;
    while (j < s.size() && (static_cast<unsigned char>(s[j]) & 0xC0) == 0x80) ++j;
    if (!page_fits(joined(before, s.substr(0, j)))) break;
    best = j;
    i = j;
  }
  // Not even one character fits: text that is no UTF-8 (a run of continuation bytes), cut at a byte.
  if (best == 0 && before.empty()) {
    while (best < s.size() && page_fits(s.substr(0, best + 1))) ++best;
  }
  return best;
}

class Reply final : public ReplyPager {
 public:
  explicit Reply(std::uint16_t words_max) : cap_(words_max == 0 ? kWordsDefault : words_max) {}

  Take push(Sentence sentence) override {
    if (full_) return Take::Over;
    std::string text(trimmed(sentence.text));
    if (text.empty()) return Take::Kept;
    const std::size_t limit = static_cast<std::size_t>(cap_) + kWordsOver;
    std::size_t w = reply_words(text);
    if (sentences_ == 0 && w > limit) {
      text = cut_first(text, limit);
      w = reply_words(text);
      full_ = true;
    } else if (words_ + w > limit) {
      full_ = true;
      return Take::Over;
    }
    const std::size_t bytes = bytes_ + (sentences_ > 0 ? 1 : 0) + text.size();
    if (bytes > kReplyBytes) {
      if (sentences_ > 0) {
        full_ = true;
        return Take::Over;
      }
      text = cut_bytes(text, kReplyBytes);
      full_ = true;
    }
    bytes_ += (sentences_ > 0 ? 1 : 0) + text.size();
    words_ += w;
    ++sentences_;
    queue_.push_back(std::move(text));
    if (words_ > cap_ || sentences_ >= kReplySentences) full_ = true;
    return full_ ? Take::Full : Take::Kept;
  }

  void end() override { ended_ = true; }

  Sealing next(Mono now) override {
    Sealing s;
    if (queue_.empty()) {
      s.kind = (ended_ || full_) ? Sealing::Kind::Done : Sealing::Kind::Wait;  // full: no sentence will come
      return s;
    }
    const bool closed = ended_ || full_;  // no sentence will join the queue
    // Pack the page: whole pieces while they fit; a first piece too long alone, or one after short sentences only, is
    // split.
    std::string page;
    std::size_t used = 0;       // pieces placed whole
    std::size_t last_long = 0;  // pieces placed whole up to the last that is not short
    bool only_short = true;
    std::optional<std::pair<std::string, std::string>> split;  // the head placed and the rest left
    for (std::size_t i = 0; i < queue_.size(); ++i) {
      const std::string& piece = queue_[i];
      const bool short_piece = chars(piece) < kShortSentenceChars;
      std::string candidate = joined(page, piece);
      if (page_fits(candidate)) {
        page = std::move(candidate);
        used = i + 1;
        if (!short_piece) last_long = used;
        only_short = only_short && short_piece;
        continue;
      }
      if (page.empty() || only_short) {
        std::size_t cut = page_split(page, piece);
        if (cut == 0 && page.empty()) cut = hard_cut(page, piece);
        if (cut > 0) {
          std::string head(trimmed(std::string_view(piece).substr(0, cut)));
          std::string rest(trimmed(std::string_view(piece).substr(cut)));
          page = joined(page, head);
          split = std::make_pair(std::move(head), std::move(rest));
          only_short = false;
        }
      }
      break;
    }
    if (!split) {
      // Short sentences at the page's end join the next: they leave this page for it unless nothing more can come.
      const bool more = used < queue_.size() || !closed;
      if (more && used > last_long && last_long > 0) {
        used = last_long;
        page.clear();
        for (std::size_t i = 0; i < used; ++i) page = joined(page, queue_[i]);
      } else if (only_short && used == queue_.size() && !closed) {
        s.kind = Sealing::Kind::Wait;  // short ones alone, waiting for the sentence they join
        return s;
      }
      // Page 1 waits up to 250 ms for another sentence while what it holds is shorter than a line.
      if (!sealed_any_ && !closed && used == queue_.size() && chars(page) < kPageLineChars) {
        if (!needed_at_) needed_at_ = now;
        const Mono until = *needed_at_ + kFirstPageWaitMs;
        if (now < until) {
          s.kind = Sealing::Kind::Wait;
          s.until = until;
          return s;
        }
      }
    }
    // Seal it.
    queue_.erase(queue_.begin(), queue_.begin() + static_cast<std::ptrdiff_t>(used));
    if (split) {
      if (split->second.empty()) {
        queue_.pop_front();
      } else {
        queue_.front() = std::move(split->second);
      }
    }
    sealed_any_ = true;
    needed_at_.reset();
    s.kind = Sealing::Kind::Page;
    s.page.text = std::move(page);
    s.page.hold_ms = page_hold_ms(s.page.text);
    s.page.last = closed && queue_.empty();
    return s;
  }

 private:
  std::uint16_t cap_;
  std::deque<std::string> queue_;  // sentences, or the rest of one, waiting for a page
  std::size_t sentences_ = 0;
  std::size_t words_ = 0;
  std::size_t bytes_ = 0;
  bool full_ = false;
  bool ended_ = false;
  bool sealed_any_ = false;
  std::optional<Mono> needed_at_;  // page 1, first asked for
};

}  // namespace

std::unique_ptr<ReplyPager> SpeechPager::open(const PageContext& context) const {
  return std::make_unique<Reply>(context.words_max);
}

std::uint32_t page_hold_ms(std::string_view text) {
  const std::size_t n = chars(text);
  const std::size_t raw = 1200 + 55 * std::min<std::size_t>(n, 1000);
  std::uint32_t hold = static_cast<std::uint32_t>(std::clamp<std::size_t>(raw, 2000, 6500));
  if (ends_with(text, "...")) hold += 400;
  return hold;
}

Mono hold_ends(Mono shown, std::uint32_t hold_ms, std::optional<Mono> skipped) {
  const Mono end = shown + hold_ms;
  if (!skipped) return end;
  return std::min(end, std::max(*skipped, shown + kSkipFloorMs));
}

bool page_fits(std::string_view text) {
  if (text.size() > kPageBytes) return false;
  std::size_t lines = 1;
  std::size_t line = 0;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && text[i] == ' ') ++i;
    std::size_t j = i;
    while (j < text.size() && text[j] != ' ') ++j;
    if (j == i) break;
    const std::size_t len = chars(text.substr(i, j - i));
    if (len > kPageLineChars) return false;
    if (line == 0) {
      line = len;
    } else if (line + 1 + len <= kPageLineChars) {
      line += 1 + len;
    } else if (++lines > kPageLines) {
      return false;
    } else {
      line = len;
    }
    i = j;
  }
  return true;
}

std::size_t page_split(std::string_view before, std::string_view sentence) {
  std::vector<speech::Cp> cps;
  speech::decode_utf8(sentence, true, cps);
  const auto fits = [&](std::size_t cut) {
    const std::string_view head = trimmed(sentence.substr(0, cut));
    return !head.empty() && page_fits(joined(before, head));
  };
  std::size_t best = 0;
  // At a mark among characters 40 to 116 (the mark the 40th to the 116th character), after it.
  for (std::size_t k = 0; k < cps.size(); ++k) {
    const char32_t c = cps[k].cp;
    const bool mark = c == ';' || c == U'\u2014' || c == ':' || c == ',';
    if (!mark || k + 1 < 40 || k + 1 > 116) continue;
    if (k + 1 >= cps.size() || cps[k + 1].cp != ' ') continue;  // never inside a word
    const std::size_t cut = cps[k].at + cps[k].bytes;
    if (cut < sentence.size() && fits(cut)) best = cut;
  }
  if (best > 0) return best;
  // At a space within the first 116 characters.
  for (std::size_t k = 0; k < cps.size() && k <= 116; ++k) {
    if (cps[k].cp == ' ' && k > 0 && fits(cps[k].at)) best = cps[k].at;
  }
  return best;
}

std::size_t reply_words(std::string_view text) {
  std::size_t n = 0;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && text[i] == ' ') ++i;
    std::size_t j = i;
    while (j < text.size() && text[j] != ' ') ++j;
    if (j > i && has_word_char(text.substr(i, j - i))) ++n;
    i = j;
  }
  return n;
}

}  // namespace en::core
