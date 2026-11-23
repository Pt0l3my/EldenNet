#pragma once
// The lint, part A's (NPC §8.3, §8.4; DESIGN §8.4): speech.hpp's Lint, for real, over the content pack's lexicon
// (content/eldenring/lexicon/: banned.toml, our own lists, and safety.toml, fingerprints only, O107).
//
// Each sentence, as the sanitizer gave it, is checked once, in order:
//   - hard (NPC §8.3): the safety list's fingerprints (at both levels; at `gentle` its gore list too), the
//     high-confidence promise patterns, the banned words (game mechanics, meta, modern idiom, modern objects and the
//     real world, assistant-speak). The worst hits first: safety, then promise, then banned. A hit decides NPC §8.4's
//     action: in the reply's first sentence (none passed yet) Regenerate on a first try and Deflect on the
//     regeneration; later, Stop at the previous sentence. After an action that ends the reply, every later sentence
//     answers Stop with the same list.
//   - soft: openers (the reply's leading interjections: one from the persona's openers at most, none from the global
//     set that the persona lacks, and not the last reply's: the others are stripped, the sentence fixed in place, and a
//     sentence left with no word is dropped), a duplicate (dropped), a tic past its cap and the told name's second use
//     (logged; the tally names the tic that reached its cap most recently, block 11's one note, WHY§388), the wider
//     promise list, an unknown proper noun, archaic forms in a register that has none, and third-person self-reference
//     (logged).
// Banned and promise entries match whole words, any case, accents ignored, in order; a last word ending in '*'
// matches any word it begins. Pure (ARCH §2.3); the lint never keeps or reports the text that hit.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/speech.hpp"
#include "en/proto/expected.hpp"

namespace en::core {

// ---- The safety list's fingerprints (O107) ----
//
// A fingerprint is SHA-256, in lower-case hex, of kSafetyDomain followed by a normalised word or phrase of at most
// three words, the words joined by one space. The normalisation, fixed here and the same for the list's words and a
// reply's: text is cut into chunks at white space and at any character that is not a letter, a digit, a look-alike
// (@ $ ! |) or a separator (. , ; : - – — … _ * ` ~ and the apostrophes); a chunk's separators and its '!' at either
// end are dropped; letters lose their accents and their case (Latin-1 and Latin Extended-A: "é" is "e", "Æ" is "ae",
// "ß" is "ss"); in a chunk with a letter, separators inside are dropped and look-alikes become letters (0 o, 1 i, 3 e,
// 4 a, 5 s, 7 t, 8 b, @ a, $ s, ! i, | i), while a chunk with no letter keeps its digits alone; three or more
// one-letter words in a row become one word; and a run of one letter repeated becomes one ("fooo" is "fo"). A reply's
// words, and each run of two and three of them, are looked up.
inline constexpr std::string_view kSafetyDomain = "eldennet.safety.v1:";
inline constexpr std::size_t kSafetyPhraseWords = 3;

[[nodiscard]] std::vector<std::string> safety_words(std::string_view text);
// The fingerprint of a word or phrase; nothing when it normalises to no word or to more than three.
[[nodiscard]] std::optional<std::string> safety_fingerprint(std::string_view phrase);

// ---- The lexicon ----

struct LexiconEntry {
  std::vector<std::string> words;  // lower case, accents stripped
  bool prefix = false;             // the last word matches any word it begins
};

struct Lexicon {
  std::vector<LexiconEntry> banned;        // hard: [hard]'s lists
  std::vector<LexiconEntry> promise;       // hard: [promise].hard
  std::vector<LexiconEntry> wider;         // soft: [promise].wider
  std::vector<LexiconEntry> archaic;       // soft: [soft].archaic
  std::vector<std::string> interjections;  // soft: [soft].interjections, the global opener set
  std::vector<std::string> safety_always;  // fingerprints, sorted: both safety levels
  std::vector<std::string> safety_gentle;  // fingerprints, sorted: `gentle` only
};

struct LexiconError {
  std::string file;      // "banned.toml" or "safety.toml"
  std::size_t line = 0;  // 0 when not known
  std::string problem;   // names a key and an index, never an entry's text
};

inline constexpr std::int64_t kLexiconFormat = 1;

// Reads banned.toml's and safety.toml's texts (the content loader reads the files: en-core does no I/O).
[[nodiscard]] proto::Expected<Lexicon, LexiconError> parse_lexicon(std::string_view banned_toml,
                                                                   std::string_view safety_toml);

// ---- The lint ----

class SpeechLint final : public Lint {
 public:
  explicit SpeechLint(Lexicon lexicon);
  [[nodiscard]] std::unique_ptr<ReplyLint> open(const LintContext& context) const override;
  [[nodiscard]] const Lexicon& lexicon() const noexcept { return *lexicon_; }

 private:
  std::shared_ptr<const Lexicon> lexicon_;
};

// The words a lint list matches against: lower case, accents stripped, cut at anything but letters, digits and an
// apostrophe inside a word (’ ‘ ʼ read as '). For tests and the tools.
[[nodiscard]] std::vector<std::string> lint_words(std::string_view text);

// ---- The deflection ring (NPC §8.4) ----
//
// The persona's least recently used deflection in this journey: never-used lines first, in their file's order, then
// the one used longest ago. The conversation (part B) holds one per persona and journey, and the store keeps its order
// from v0.3 (order() and the constructor that takes one).
class DeflectionRing {
 public:
  explicit DeflectionRing(std::size_t count = 0);
  // From a saved order; an order that is not a permutation of 0..count-1 is replaced by the file's order.
  DeflectionRing(std::size_t count, std::vector<std::uint8_t> order);
  // The next line's index, which is then the most recently used; nothing when the persona has none.
  std::optional<std::size_t> next();
  [[nodiscard]] const std::vector<std::uint8_t>& order() const noexcept { return order_; }

 private:
  std::vector<std::uint8_t> order_;  // least recently used first
};

}  // namespace en::core
