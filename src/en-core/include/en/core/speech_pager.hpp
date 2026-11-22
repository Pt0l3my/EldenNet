#pragma once
// The pager, part A's (NPC §8.5, §8.6; DESIGN §8.5): speech.hpp's Pager, for real.
//
// The caps (§8.5), as sentences come in: whole sentences are kept while the reply's words stay within the persona's
// reply_words_max; the sentence that crosses it is kept if the total stays within the cap + 10, and the reply is then
// full (Take::Full: cancel the stream); past that it is left out and the reply is full too (Take::Over, which the
// conversation treats as Full when it comes first). The reply's first sentence alone past the cap + 10 is cut at its
// last ';', '—' or ',' within the cap + 10 words (else at that word) and ends in "...", and fills the reply. At most
// three sentences, and 600 bytes: a sentence that would pass 600 bytes is left out (the first is cut at a word, so it
// fits, and ends in "...").
//
// The pages (§8.6), sealed lazily, when the conversation asks: as many complete sentences as fit two lines of 58
// characters, wrapped at spaces, and 180 bytes. A sentence shorter than 12 characters joins the next on its page, and
// waits for it while the reply goes on. Page 1 is sealed once its first sentence is in, but if what fits is shorter
// than 58 characters it waits up to 250 ms from when it was first asked for (Sealing::until) for another that fits. A
// sentence too long for a page alone splits at its last ';', '—', ':' or ',' among characters 40 to 116 that leaves a
// first part that fits, else at its last space that does, never inside a word; the rest starts the next page with no
// extra pause. A page's hold is clamp(1200 + 55 × characters, 2000, 6500) ms, and 400 ms more when it ends in "...";
// Tab ends a hold early, but not before 300 ms (hold_ends). Pure (ARCH §2.3): time is the `now` the conversation
// passes.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "en/core/speech.hpp"

namespace en::core {

inline constexpr std::size_t kPageLineChars = 58;
inline constexpr std::size_t kPageLines = 2;
inline constexpr std::size_t kPageBytes = 180;
inline constexpr std::size_t kReplyBytes = 600;
inline constexpr std::size_t kReplySentences = 3;
inline constexpr std::uint16_t kWordsOver = 10;         // the crossing sentence's allowance past reply_words_max
inline constexpr std::uint16_t kWordsDefault = 40;      // reply_words_max when the context gives 0 (NPC §3.1)
inline constexpr std::size_t kShortSentenceChars = 12;  // shorter joins the next on its page (P1 §5.6)
inline constexpr Mono kFirstPageWaitMs = 250;
inline constexpr Mono kSkipFloorMs = 300;

class SpeechPager final : public Pager {
 public:
  [[nodiscard]] std::unique_ptr<ReplyPager> open(const PageContext& context) const override;
};

// The hold of a page with this text (NPC §8.6).
[[nodiscard]] std::uint32_t page_hold_ms(std::string_view text);
// When a page shown at `shown` stops holding: at its hold's end, or, when Tab went down at `skipped`, then but not
// before 300 ms after it showed (and never after its hold's end).
[[nodiscard]] Mono hold_ends(Mono shown, std::uint32_t hold_ms, std::optional<Mono> skipped = std::nullopt);
// Whether the text wraps, at spaces, into at most two lines of 58 characters, and holds at most 180 bytes.
[[nodiscard]] bool page_fits(std::string_view text);
// Where an over-long sentence splits (NPC §8.6): the length in bytes of its first part, which fits a page with
// `before` in front of it (joined by a space; empty for a page of its own), or 0 when no first part fits.
[[nodiscard]] std::size_t page_split(std::string_view before, std::string_view sentence);
// Words as the caps count them: tokens between spaces holding a letter or a digit.
[[nodiscard]] std::size_t reply_words(std::string_view text);

}  // namespace en::core
