#pragma once
// The sanitizer, part A's (NPC §8.2 steps 0 to 7; DESIGN §8.4): speech.hpp's Sanitizer, for real. Each reply's
// SentenceStream takes the stream's deltas as they come, split anywhere, even inside a character, and gives out each
// sentence once it is final: once no later text could change it. Giving them out does not wait for the stream to end.
//
// The steps, in order, on the raw stream:
//   0  Decode: lossy UTF-8 (a broken sequence is one U+FFFD), NFC within Latin-1 and Latin Extended-A, the raw stream
//      capped at 4 KB (what is past it is OverLimit, and the stream stops: Flow::Stop).
//   1  Reasoning: <think>, <thinking> and <reasoning> (any case) to their own close; an unclosed one to the end.
//   2  Trailer: a line starting (after spaces, tabs, '*', '`', '>' or '~') with ACT: or ACTION: (any case) ends the
//      reply's speech; the rest of the stream, after the colon, is the intent (Flow::IntentOnly).
//   3  Run-on: a line starting so with "The Tarnished:" or "Tarnished:" (any case) ends the reply (Flow::Stop).
//   4  Markup: <tags> and <?...?> on one line; [...] on one line, a link [text](target) keeping its text; (...) of at
//      most 60 characters on one line; *...* and _..._ on one line (a run of two or more is a bold marker, taken out
//      alone; a lone one is taken out alone); a heading's '#', a bullet's '-', '+', '*' or '•' and a quote's '>' at a
//      line's start; backticks; emoji, variation selectors and the joiner (Emoji); C0 and C1 controls (Control), where
//      a newline, a carriage return or a tab becomes a space and counts nothing.
//   7  Split (NPC §8.6): '.', '!', '?', "..." or "…" (a run of them), then closing quotes, then white space or the
//      stream's end; "..." before a lower-case word is no boundary. The open fragment is held.
//   5  Frame, per sentence: a leading "Name:" of the reply's first sentence (one of the context's names, any case, or
//      an upper-case letter, at most 24 letters, spaces, apostrophes or hyphens, a colon and white space); quotation
//      marks wrapping a sentence, a reply's opening and closing double quotes, and a single quote opening a sentence
//      (not 'tis, 'twas, 'twere, 'twill, 'em) or closing it after its terminator (Quotes).
//   6  Whitelist, per sentence: letters (ASCII, Latin-1, Latin Extended-A), digits, the space and . , ; : ! ? ' - — –;
//      ’ ‘ ʼ become ', "…" becomes "...", every other character is Character; spaces collapsed and trimmed; a token of
//      more than 24 characters is LongToken. A sentence left empty is not given out; its removals go to the next.
//
// Removals are counted per sentence, by reason, in bytes of the raw stream, never with their text: what lies between
// two sentences is counted with the later one, and what follows the last sentence of a reply that ended is counted
// with none (the flow and intent() tell of a trailer or a cut). Pure (ARCH §2.3); each feed reads the reply's raw text
// again from its start (at most 4 KB), so a reply costs at most 4 KB of work per delta.
#include <cstddef>
#include <memory>

#include "en/core/speech.hpp"

namespace en::core {

inline constexpr std::size_t kRawStreamMax = 4096;  // NPC §8.5: the raw stream's cap
inline constexpr std::size_t kTokenCharsMax = 24;   // step 6: a longer token is dropped
inline constexpr std::size_t kParenSpanMax = 60;    // step 4: (...) spans of at most this many characters inside

class SpeechSanitizer final : public Sanitizer {
 public:
  [[nodiscard]] std::unique_ptr<SentenceStream> open(const SanitizeContext& context) const override;
};

}  // namespace en::core
