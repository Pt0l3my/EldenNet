// The sanitizer (NPC §8.2 steps 0 to 7): speech_sanitizer.hpp says what each step does. Each feed analyses the reply's
// raw text again from its start, in passes that each stop where more text could still change their answer; a sentence
// is given out once it is final, so a stream split anywhere gives the same sentences as the whole stream at once.
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/core/speech_sanitizer.hpp"
#include "text.hpp"

namespace en::core {
namespace {

using speech::Cp;

struct Found {
  Removed what = Removed::Markup;
  std::uint32_t at = 0;  // raw offset where the removed text started
  std::uint32_t bytes = 0;
};

struct Analysis {
  std::vector<Sentence> sentences;  // the final ones, indexed from 0
  Flow flow = Flow::More;
  std::optional<std::string> intent;
};

char32_t lower(char32_t c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
bool is_terminator(char32_t c) { return c == '.' || c == '!' || c == '?' || c == U'\u2026'; }
bool is_double_quote(char32_t c) {
  return c == '"' || c == U'\u201C' || c == U'\u201D' || c == U'\u00AB' || c == U'\u00BB' || c == U'\u201E';
}
bool is_single_quote(char32_t c) { return c == '\'' || c == U'\u2018' || c == U'\u2019' || c == U'\u02BC'; }
bool is_quote(char32_t c) { return is_double_quote(c) || is_single_quote(c); }
bool is_alnum(char32_t c) { return speech::is_letter(c) || speech::is_digit(c); }

std::uint32_t bytes_of(std::span<const Cp> cps) {
  std::uint32_t n = 0;
  for (const Cp& c : cps) n += c.bytes;
  return n;
}

enum class Match : std::uint8_t { No, Yes, Short };

// Whether `word` (ASCII, lower case) is at cps[from...], any case; Short when the text ends inside it.
Match match_ci(std::span<const Cp> cps, std::size_t from, std::string_view word) {
  for (std::size_t k = 0; k < word.size(); ++k) {
    if (from + k >= cps.size()) return Match::Short;
    if (lower(cps[from + k].cp) != static_cast<unsigned char>(word[k])) return Match::No;
  }
  return Match::Yes;
}

class Analyser {
 public:
  Analyser(std::string_view raw, std::size_t over, bool ended, std::span<const std::string> names)
      : raw_(raw), over_(over), ended_(ended || over > 0), names_(names) {}

  Analysis run() {
    std::vector<Cp> cps = decode();
    std::vector<Cp> p1 = reasoning(cps);
    std::vector<Cp> p2 = lines(p1);
    std::vector<Cp> p4 = markup(p2);
    split(p4);
    if (over_ > 0) out_.flow = Flow::Stop;
    return std::move(out_);
  }

 private:
  // ---- Step 0: decode, NFC and the cap ----
  std::vector<Cp> decode() {
    std::vector<Cp> raw_cps;
    speech::decode_utf8(raw_, ended_, raw_cps);
    open_ = !ended_;
    if (over_ > 0) {
      found_.push_back({Removed::OverLimit, static_cast<std::uint32_t>(raw_.size()), static_cast<std::uint32_t>(over_)});
    }
    std::vector<Cp> out;
    out.reserve(raw_cps.size());
    for (std::size_t i = 0; i < raw_cps.size(); ++i) {
      Cp c = raw_cps[i];
      while (i + 1 < raw_cps.size()) {
        const char32_t composed = speech::compose(c.cp, raw_cps[i + 1].cp);
        if (composed == 0) break;
        c.cp = composed;
        c.bytes = static_cast<std::uint8_t>(c.bytes + raw_cps[i + 1].bytes);
        ++i;
      }
      // An ASCII letter at the end may yet take a combining mark: it waits for more text.
      if (i + 1 == raw_cps.size() && open_ && c.cp < 0x80 && speech::is_letter(c.cp)) break;
      out.push_back(c);
    }
    return out;
  }

  // ---- Step 1: reasoning ----
  std::vector<Cp> reasoning(const std::vector<Cp>& c) {
    static constexpr std::array<std::string_view, 3> kTags = {"think", "thinking", "reasoning"};
    std::vector<Cp> out;
    out.reserve(c.size());
    std::size_t i = 0;
    while (i < c.size()) {
      if (c[i].cp != '<') {
        out.push_back(c[i++]);
        continue;
      }
      bool short_any = false;
      std::size_t body = 0;
      std::string_view tag;
      for (const std::string_view t : kTags) {
        const Match m = match_ci(c, i + 1, t);
        if (m == Match::Short) short_any = true;
        if (m != Match::Yes) continue;
        const std::size_t k = i + 1 + t.size();
        if (k >= c.size()) {
          short_any = true;
        } else if (c[k].cp == '>') {
          body = k + 1;
          tag = t;
          break;
        }
      }
      if (tag.empty()) {
        if (short_any && open_) return stop(out);
        out.push_back(c[i++]);
        continue;
      }
      // Its own close, any case.
      std::size_t end = 0;
      for (std::size_t k = body; k < c.size(); ++k) {
        if (c[k].cp != '<' || match_ci(c, k + 1, "/") != Match::Yes) continue;
        if (match_ci(c, k + 2, tag) != Match::Yes) continue;
        const std::size_t gt = k + 2 + tag.size();
        if (gt < c.size() && c[gt].cp == '>') {
          end = gt + 1;
          break;
        }
      }
      if (end == 0) {
        if (open_) return stop(out);  // still inside it: nothing after it is known yet
        end = c.size();               // unterminated: it swallows the rest
      }
      found_.push_back({Removed::Reasoning, c[i].at, bytes_of(std::span(c).subspan(i, end - i))});
      i = end;
    }
    return out;
  }

  // ---- Steps 2 and 3: the trailer and the run-on, at a line's start ----
  std::vector<Cp> lines(const std::vector<Cp>& c) {
    std::vector<Cp> out;
    out.reserve(c.size());
    bool line_start = true;
    for (std::size_t i = 0; i < c.size(); ++i) {
      if (line_start) {
        std::size_t k = i;
        while (k < c.size() && (c[k].cp == ' ' || c[k].cp == '\t' || c[k].cp == '*' || c[k].cp == '`' ||
                                c[k].cp == '>' || c[k].cp == '~')) {
          ++k;
        }
        if (k >= c.size()) {
          if (open_) return stop(out);
        } else {
          static constexpr std::array<std::string_view, 4> kWords = {"act:", "action:", "the tarnished:", "tarnished:"};
          bool short_any = false;
          for (std::size_t w = 0; w < kWords.size(); ++w) {
            const Match m = match_ci(c, k, kWords[w]);
            if (m == Match::Short) short_any = true;
            if (m != Match::Yes) continue;
            const std::size_t colon_end = k + kWords[w].size();
            const bool trailer = w < 2;
            // What follows the cut is no speech: nothing found there counts (it may never be read).
            std::erase_if(found_, [&](const Found& f) { return f.at >= c[i].at; });
            found_.push_back({trailer ? Removed::Trailer : Removed::RunOn, c[i].at,
                              bytes_of(std::span(c).subspan(i, colon_end - i))});
            if (trailer) {
              out_.flow = Flow::IntentOnly;
              out_.intent = intent_text(std::span(c).subspan(colon_end));
            } else {
              out_.flow = Flow::Stop;
            }
            open_ = false;  // no sentence follows: what came before is complete
            return out;
          }
          if (short_any && open_) return stop(out);
        }
      }
      line_start = c[i].cp == '\n' || c[i].cp == '\r';
      out.push_back(c[i]);
    }
    return out;
  }

  static std::string intent_text(std::span<const Cp> c) {
    std::string out;
    bool space = false;
    for (const Cp& x : c) {
      if (x.cp < 0x20 || x.cp == 0x7F || (x.cp >= 0x80 && x.cp <= 0x9F) || x.cp == ' ') {
        space = !out.empty();
        continue;
      }
      if (space) out += ' ';
      space = false;
      speech::append_utf8(out, x.cp);
    }
    return out;
  }

  // ---- Step 4: markup, emoji and controls ----
  std::vector<Cp> markup(const std::vector<Cp>& c) {
    std::vector<Cp> out;
    out.reserve(c.size());
    const std::size_t n = c.size();
    std::size_t link_close = n;  // the ']' of a link whose text is being kept, and the end of its target
    std::size_t link_end = n;
    // Where a span's close may be looked for: to the line's end (n when the line has not ended in the text so far),
    // and inside a link's text, to its ']'.
    const auto line_end = [&](std::size_t from) {
      std::size_t k = from;
      while (k < n && k < link_close && c[k].cp != '\n' && c[k].cp != '\r') ++k;
      return k;
    };
    const auto remove = [&](Removed why, std::size_t a, std::size_t b) {
      if (b > a) found_.push_back({why, c[a].at, bytes_of(std::span(c).subspan(a, b - a))});
    };
    bool line_start = true;
    for (std::size_t i = 0; i < n;) {
      const char32_t ch = c[i].cp;
      if (i == link_close) {
        remove(Removed::Markup, i, link_end);
        i = link_end;
        link_close = link_end = n;
        continue;
      }
      if (ch == '\n' || ch == '\r' || ch == '\t') {
        out.push_back(Cp{' ', c[i].at, c[i].bytes});
        if (ch != '\t') line_start = true;
        ++i;
        continue;
      }
      if (ch == ' ') {
        out.push_back(c[i++]);
        continue;
      }
      if (line_start) {
        line_start = false;
        // A heading's '#'s, a bullet or a quote's '>'s, before a space.
        std::size_t k = i;
        if (ch == '#' || ch == '>') {
          while (k < n && c[k].cp == ch) ++k;
        } else if (ch == '-' || ch == '+' || ch == '*' || ch == U'\u2022') {
          k = i + 1;
        }
        if (k > i) {
          if (k >= n && open_) return stop(out);
          if (k >= n || c[k].cp == ' ' || (ch == '>' && k < n)) {
            remove(Removed::Markup, i, k);
            i = k;
            line_start = ch == '>';  // a quoted line may hold a bullet or a heading
            continue;
          }
        }
      }
      if (ch < 0x20 || ch == 0x7F || (ch >= 0x80 && ch <= 0x9F)) {
        remove(Removed::Control, i, i + 1);
        ++i;
        continue;
      }
      if (speech::is_emoji(ch)) {
        remove(Removed::Emoji, i, i + 1);
        ++i;
        continue;
      }
      if (ch == '`') {
        remove(Removed::Markup, i, i + 1);
        ++i;
        continue;
      }
      if (ch == '<') {
        // <?...?> or a tag, <name ...> or </name>, closed on the same line within 200 characters.
        const std::size_t le = line_end(i);
        const std::size_t limit = std::min(le, i + 200);
        std::size_t k = i + 1;
        const bool pi = k < n && c[k].cp == '?';
        if (k < n && c[k].cp == '/') ++k;
        const bool tag = pi || (k < n && speech::is_letter(c[k].cp) && c[k].cp < 0x80);
        if (k >= n && open_) return stop(out);
        if (tag) {
          std::size_t close = 0;
          for (std::size_t j = k; j < limit; ++j) {
            if (c[j].cp == '>' && (!pi || (j > i + 2 && c[j - 1].cp == '?'))) {
              close = j + 1;
              break;
            }
          }
          if (close == 0 && limit == n && open_) return stop(out);
          if (close != 0) {
            remove(Removed::Markup, i, close);
            i = close;
            continue;
          }
        }
        out.push_back(c[i++]);  // a lone '<': the whitelist takes it
        continue;
      }
      if (ch == '[') {
        const std::size_t le = line_end(i);
        const std::size_t limit = std::min(le, i + 200);
        std::size_t close = 0;
        for (std::size_t j = i + 1; j < limit; ++j) {
          if (c[j].cp == ']') {
            close = j;
            break;
          }
        }
        if (close == 0) {
          if (limit == n && open_) return stop(out);
          out.push_back(c[i++]);
          continue;
        }
        if (close + 1 >= n && open_) return stop(out);  // a link's target may follow
        if (close + 1 < n && c[close + 1].cp == '(') {
          std::size_t target_end = 0;
          for (std::size_t j = close + 2; j < le; ++j) {
            if (c[j].cp == ')') {
              target_end = j + 1;
              break;
            }
          }
          if (target_end == 0 && le == n && open_) return stop(out);
          if (target_end != 0) {  // a link: its text stays
            remove(Removed::Markup, i, i + 1);
            link_close = close;
            link_end = target_end;
            ++i;
            continue;
          }
        }
        remove(Removed::Markup, i, close + 1);
        i = close + 1;
        continue;
      }
      if (ch == '(') {
        const std::size_t le = line_end(i);
        std::size_t close = 0;
        std::size_t j = i + 1;
        for (; j < le && j <= i + 1 + kParenSpanMax; ++j) {
          if (c[j].cp == ')') {
            close = j + 1;
            break;
          }
        }
        if (close == 0 && j == n && open_) return stop(out);
        if (close != 0) {
          remove(Removed::Markup, i, close);
          i = close;
          continue;
        }
        out.push_back(c[i++]);
        continue;
      }
      if (ch == '*' || ch == '_') {
        std::size_t run = i;
        while (run < n && c[run].cp == ch) ++run;
        if (run >= n && open_) return stop(out);  // the run may grow
        if (run - i >= 2) {  // a bold marker: taken out alone
          remove(Removed::Markup, i, run);
          i = run;
          continue;
        }
        // A single one: a span to the next single one on the line ('_' only at a word's edges).
        const bool word_start = i == 0 || !is_alnum(c[i - 1].cp);
        if (ch == '_' && !word_start) {
          out.push_back(c[i++]);
          continue;
        }
        const std::size_t le = line_end(i);
        const std::size_t limit = ch == '_' ? std::min(le, i + 200) : le;
        std::size_t close = 0;
        bool unsure = false;
        for (std::size_t j = i + 1; j < limit; ++j) {
          if (c[j].cp != ch) continue;
          const bool single = (j + 1 >= n || c[j + 1].cp != ch) && c[j - 1].cp != ch;
          if (j + 1 >= n && open_) {
            unsure = true;  // a run may follow it
            break;
          }
          if (!single) {
            while (j + 1 < limit && c[j + 1].cp == ch) ++j;
            continue;
          }
          if (ch == '_' && j + 1 < n && is_alnum(c[j + 1].cp)) continue;
          if (ch == '_' && j + 1 >= n && open_) {
            unsure = true;
            break;
          }
          close = j + 1;
          break;
        }
        if (close == 0 && (unsure || (limit == n && open_))) return stop(out);
        if (close != 0) {
          remove(Removed::Markup, i, close);
          i = close;
          continue;
        }
        if (ch == '*') {
          remove(Removed::Markup, i, i + 1);
        } else {
          out.push_back(c[i]);  // a lone '_': the whitelist takes it
        }
        ++i;
        continue;
      }
      out.push_back(c[i++]);
    }
    return out;
  }

  // ---- Step 7: the splitter, then steps 5 and 6 per sentence ----
  struct Candidate {
    std::size_t first = 0;  // its first character that is not a space, in the clean text
    std::size_t end = 0;    // one past its boundary (the terminators and any closing quotes)
  };

  void split(const std::vector<Cp>& c) {
    const std::size_t n = c.size();
    const auto skip_spaces = [&](std::size_t k) {
      while (k < n && c[k].cp == ' ') ++k;
      return k;
    };
    std::vector<Candidate> cands;
    std::size_t start = skip_spaces(0);  // where the open fragment begins
    std::size_t i = start;
    while (i < n) {
      if (!is_terminator(c[i].cp)) {
        ++i;
        continue;
      }
      std::size_t j = i;
      while (j < n && is_terminator(c[j].cp)) ++j;
      const bool ellipsis = c[j - 1].cp == U'\u2026' ||
                            (j - i >= 3 && c[j - 1].cp == '.' && c[j - 2].cp == '.' && c[j - 3].cp == '.');
      while (j < n && is_quote(c[j].cp)) ++j;
      if (j == n) break;  // the stream's end is a boundary only once the stream has ended
      if (c[j].cp != ' ') {
        i = j;
        continue;
      }
      const std::size_t next = skip_spaces(j);
      if (ellipsis && next == n && open_) break;  // "..." before a word not yet known
      if (ellipsis && next < n && speech::is_lower(c[next].cp)) {
        i = next;
        continue;
      }
      cands.push_back(Candidate{start, j});
      start = next;
      i = next;
    }
    if (start < n && !open_) {
      cands.push_back(Candidate{start, n});
      start = n;
    }
    // A sentence is final once the next one has begun, or the text is complete. It carries the removals from where the
    // last sentence given out stopped to where the next begins (to the end, for the last of a complete text).
    std::uint32_t carry_from = 0;
    std::vector<Removal> pending;  // steps 5 and 6's removals from sentences left empty
    for (std::size_t k = 0; k < cands.size(); ++k) {
      const bool has_next = k + 1 < cands.size() || start < n;
      if (!has_next && open_) break;
      const std::uint32_t until =
          !has_next ? UINT32_MAX : c[k + 1 < cands.size() ? cands[k + 1].first : start].at;
      const bool last = !has_next;
      std::string text = clean(std::span(c).subspan(cands[k].first, cands[k].end - cands[k].first), k == 0, last,
                               pending);
      if (text.empty()) continue;  // its removals go to the next sentence
      std::vector<Removal> removed = std::move(pending);
      pending.clear();
      for (const Found& f : found_) {
        if (f.at >= carry_from && f.at < until) add(removed, f.what, f.bytes);
      }
      carry_from = until;
      std::sort(removed.begin(), removed.end(), [](const Removal& x, const Removal& y) { return x.what < y.what; });
      out_.sentences.push_back(Sentence{std::move(text), 0, std::move(removed)});
    }
  }

  static void add(std::vector<Removal>& removed, Removed what, std::uint32_t bytes) {
    if (bytes == 0) return;
    for (Removal& r : removed) {
      if (r.what == what) {
        r.bytes += bytes;
        return;
      }
    }
    removed.push_back(Removal{what, bytes});
  }

  // Steps 5 and 6 on one sentence's clean text. `first`: the reply's first; `last`: the reply's last, complete.
  std::string clean(std::span<const Cp> s, bool first, bool last, std::vector<Removal>& removed) const {
    std::size_t a = 0;
    std::size_t b = s.size();
    while (b > a && s[b - 1].cp == ' ') --b;
    // Step 5: a leading "Name:".
    if (first) {
      const std::size_t name_end = name_prefix(s.subspan(a, b - a));
      if (name_end > 0) {
        add(removed, Removed::NamePrefix, bytes_of(s.subspan(a, name_end)));
        a += name_end;
        while (a < b && s[a].cp == ' ') ++a;
      }
    }
    // Step 5: quotation marks at either end.
    while (a < b && is_quote(s[a].cp)) {
      if (is_single_quote(s[a].cp) && !opens_quote(s.subspan(a, b - a))) break;
      add(removed, Removed::Quotes, s[a].bytes);
      ++a;
      while (a < b && s[a].cp == ' ') ++a;
    }
    while (b > a && is_quote(s[b - 1].cp)) {
      const bool after_terminator = b - 1 > a && (is_terminator(s[b - 2].cp) || is_quote(s[b - 2].cp));
      if (!is_double_quote(s[b - 1].cp) && !after_terminator) break;
      if (is_double_quote(s[b - 1].cp) && !after_terminator && !last) break;
      add(removed, Removed::Quotes, s[b - 1].bytes);
      --b;
      while (b > a && s[b - 1].cp == ' ') --b;
    }
    // Step 6: the whitelist, spaces collapsed, long tokens dropped.
    struct Kept {
      char32_t cp;
      std::uint32_t bytes;
    };
    std::vector<Kept> kept;
    kept.reserve(b - a);
    for (std::size_t k = a; k < b; ++k) {
      char32_t ch = s[k].cp;
      if (is_single_quote(ch)) ch = '\'';
      const bool ok = speech::is_letter(ch) || speech::is_digit(ch) || ch == ' ' || ch == '.' || ch == ',' ||
                      ch == ';' || ch == ':' || ch == '!' || ch == '?' || ch == '\'' || ch == '-' ||
                      ch == U'\u2014' || ch == U'\u2013' || ch == U'\u2026';
      if (!ok) {
        add(removed, Removed::Character, s[k].bytes);
        continue;
      }
      kept.push_back(Kept{ch, s[k].bytes});
    }
    std::string text;
    bool any = false;  // a letter or a digit: a sentence with neither is no sentence
    std::size_t k = 0;
    while (k < kept.size()) {
      if (kept[k].cp == ' ') {
        ++k;
        continue;
      }
      std::size_t e = k;
      while (e < kept.size() && kept[e].cp != ' ') ++e;
      if (e - k > kTokenCharsMax) {
        std::uint32_t bytes = 0;
        for (std::size_t x = k; x < e; ++x) bytes += kept[x].bytes;
        add(removed, Removed::LongToken, bytes);
      } else {
        if (!text.empty()) text += ' ';
        for (std::size_t x = k; x < e; ++x) {
          if (kept[x].cp == U'\u2026') {
            text += "...";
          } else {
            speech::append_utf8(text, kept[x].cp);
            any = any || is_alnum(kept[x].cp);
          }
        }
      }
      k = e;
    }
    if (!any) text.clear();
    return text;
  }

  // A single quote that opens a quotation, not an elision such as 'tis.
  static bool opens_quote(std::span<const Cp> s) {
    if (s.size() < 2 || !speech::is_letter(s[1].cp)) return false;
    static constexpr std::array<std::string_view, 6> kElisions = {"tis", "twas", "twere", "twill", "em", "ere"};
    for (const std::string_view e : kElisions) {
      if (match_ci(s, 1, e) != Match::Yes) continue;
      if (1 + e.size() >= s.size() || !speech::is_letter(s[1 + e.size()].cp)) return false;
    }
    return true;
  }

  // The length of a leading "Name:" and the white space after it, or 0.
  std::size_t name_prefix(std::span<const Cp> s) const {
    const auto colon_then_space = [&](std::size_t k) {
      return k + 1 < s.size() && s[k].cp == ':' && s[k + 1].cp == ' ';
    };
    for (const std::string& name : names_) {
      std::vector<Cp> want;
      speech::decode_utf8(name, true, want);
      if (want.empty() || want.size() >= s.size()) continue;
      bool same = true;
      for (std::size_t k = 0; k < want.size() && same; ++k) same = lower(s[k].cp) == lower(want[k].cp);
      if (same && colon_then_space(want.size())) return want.size() + 1;
    }
    if (s.empty() || !speech::is_upper(s[0].cp)) return 0;
    for (std::size_t k = 1; k < s.size() && k <= 25; ++k) {
      const char32_t ch = s[k].cp;
      if (ch == ':') return colon_then_space(k) ? k + 1 : 0;
      if (!speech::is_letter(ch) && ch != '\'' && ch != ' ' && ch != '-') return 0;
    }
    return 0;
  }

  // The text from here on is not decided: what came before it is all a pass hands on, and more is to come.
  std::vector<Cp> stop(std::vector<Cp>& out) {
    open_ = true;
    return std::move(out);
  }

  std::string_view raw_;
  std::size_t over_;
  bool ended_;
  std::span<const std::string> names_;
  bool open_ = true;
  std::vector<Found> found_;
  Analysis out_;
};

class Stream final : public SentenceStream {
 public:
  explicit Stream(const SanitizeContext& context) : names_(context.names.begin(), context.names.end()) {}

  Flow feed(std::string_view delta, std::vector<Sentence>& out) override {
    if (finished_ || flow_ == Flow::Stop) return flow_;
    const std::size_t room = kRawStreamMax - std::min(raw_.size(), kRawStreamMax);
    raw_.append(delta.substr(0, room));
    over_ += delta.size() - std::min(delta.size(), room);
    give(Analyser(raw_, over_, false, names_).run(), out);
    return flow_;
  }

  void withdraw() override {
    raw_.clear();
    over_ = 0;
    base_ = static_cast<std::uint8_t>(std::min<std::size_t>(255, base_ + given_));
    given_ = 0;
    flow_ = Flow::More;
    intent_.reset();
  }

  void finish(std::vector<Sentence>& out) override {
    if (finished_) return;
    give(Analyser(raw_, over_, true, names_).run(), out);
    finished_ = true;
  }

  [[nodiscard]] std::optional<std::string> intent() const override { return intent_; }

 private:
  void give(Analysis a, std::vector<Sentence>& out) {
    for (std::size_t k = given_; k < a.sentences.size(); ++k) {
      Sentence s = std::move(a.sentences[k]);
      s.index = static_cast<std::uint8_t>(std::min<std::size_t>(255, base_ + k));
      out.push_back(std::move(s));
    }
    given_ = std::max(given_, a.sentences.size());
    if (flow_ != Flow::Stop) flow_ = a.flow;
    if (a.intent) intent_ = std::move(a.intent);
  }

  std::vector<std::string> names_;
  std::string raw_;
  std::size_t over_ = 0;
  std::size_t given_ = 0;  // sentences of raw_ given out
  std::uint8_t base_ = 0;  // sentences given out before the last withdraw()
  Flow flow_ = Flow::More;
  bool finished_ = false;
  std::optional<std::string> intent_;
};

}  // namespace

std::unique_ptr<SentenceStream> SpeechSanitizer::open(const SanitizeContext& context) const {
  return std::make_unique<Stream>(context);
}

}  // namespace en::core
