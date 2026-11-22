#pragma once
// Text helpers for speech, internal to en-core (src/speech/): lossy UTF-8, the Latin letters the sanitizer's whitelist
// keeps (ASCII, Latin-1 and Latin Extended-A, NPC §8.2 step 6), NFC's compositions within them (step 0), their accents
// stripped (the lint's matching and the safety list's normalisation, O107) and the emoji step 4 takes out.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace en::core::speech {

inline constexpr char32_t kReplacement = 0xFFFD;

// A code point of the raw stream: where it started (a byte offset into the raw stream) and how many raw bytes it came
// from (more than its own encoding after NFC joined two, or for a replacement of a broken sequence).
struct Cp {
  char32_t cp = 0;
  std::uint32_t at = 0;
  std::uint8_t bytes = 0;
};

// Decodes UTF-8 lossily, appending to `out`: each maximal invalid subpart becomes one U+FFFD (Unicode's practice). With
// `ended` false, a sequence cut off by the end of `raw` is left for later: the return value is how many bytes were
// decoded. `offset` is added to every Cp's `at`.
std::size_t decode_utf8(std::string_view raw, bool ended, std::vector<Cp>& out, std::uint32_t offset = 0);
void append_utf8(std::string& out, char32_t cp);
// Characters (code points) in valid UTF-8 text.
std::size_t utf8_chars(std::string_view text);

inline bool is_combining_mark(char32_t cp) { return cp >= 0x0300 && cp <= 0x036F; }
// The letter of Latin-1 or Latin Extended-A that an ASCII letter and a combining mark compose to under NFC; 0 if none.
char32_t compose(char32_t base, char32_t mark);
// The ASCII form of a letter of Latin-1 or Latin Extended-A, its case kept ("é" is "e", "Æ" is "AE", "ß" is "ss");
// empty for anything else, ASCII included.
std::string_view stripped(char32_t cp);
// The letters the whitelist keeps: ASCII, Latin-1 and Latin Extended-A.
bool is_letter(char32_t cp);
bool is_upper(char32_t cp);
bool is_lower(char32_t cp);
inline bool is_digit(char32_t cp) { return cp >= '0' && cp <= '9'; }
bool is_emoji(char32_t cp);

}  // namespace en::core::speech
