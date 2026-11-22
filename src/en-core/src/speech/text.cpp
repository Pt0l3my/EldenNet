// Text helpers for speech (src/speech/, part A's): lossy UTF-8, the Latin letters the whitelist keeps, NFC's
// compositions within them, accents stripped, and the emoji the markup step takes out. Our own code; the two tables
// were generated from the Unicode Character Database's decompositions for U+00C0 to U+017F (the facts of Unicode,
// read through Python's unicodedata module) and checked by the tests. No static object with a dynamic initialiser.
#include "text.hpp"

#include <algorithm>
#include <array>

namespace en::core::speech {
namespace {

struct Composition {
  char32_t mark;
  char base;
  char32_t composed;
};

// Every canonical composition of an ASCII letter and one combining mark into a letter of Latin-1 or Latin
// Extended-A, by mark and then base (the order the lookup's binary search needs).
constexpr std::array<Composition, 161> kCompositions = {{
    {0x0300, 'A', 0x00C0}, {0x0300, 'E', 0x00C8}, {0x0300, 'I', 0x00CC}, {0x0300, 'O', 0x00D2}, {0x0300, 'U', 0x00D9},
    {0x0300, 'a', 0x00E0}, {0x0300, 'e', 0x00E8}, {0x0300, 'i', 0x00EC}, {0x0300, 'o', 0x00F2}, {0x0300, 'u', 0x00F9},
    {0x0301, 'A', 0x00C1}, {0x0301, 'C', 0x0106}, {0x0301, 'E', 0x00C9}, {0x0301, 'I', 0x00CD}, {0x0301, 'L', 0x0139},
    {0x0301, 'N', 0x0143}, {0x0301, 'O', 0x00D3}, {0x0301, 'R', 0x0154}, {0x0301, 'S', 0x015A}, {0x0301, 'U', 0x00DA},
    {0x0301, 'Y', 0x00DD}, {0x0301, 'Z', 0x0179}, {0x0301, 'a', 0x00E1}, {0x0301, 'c', 0x0107}, {0x0301, 'e', 0x00E9},
    {0x0301, 'i', 0x00ED}, {0x0301, 'l', 0x013A}, {0x0301, 'n', 0x0144}, {0x0301, 'o', 0x00F3}, {0x0301, 'r', 0x0155},
    {0x0301, 's', 0x015B}, {0x0301, 'u', 0x00FA}, {0x0301, 'y', 0x00FD}, {0x0301, 'z', 0x017A}, {0x0302, 'A', 0x00C2},
    {0x0302, 'C', 0x0108}, {0x0302, 'E', 0x00CA}, {0x0302, 'G', 0x011C}, {0x0302, 'H', 0x0124}, {0x0302, 'I', 0x00CE},
    {0x0302, 'J', 0x0134}, {0x0302, 'O', 0x00D4}, {0x0302, 'S', 0x015C}, {0x0302, 'U', 0x00DB}, {0x0302, 'W', 0x0174},
    {0x0302, 'Y', 0x0176}, {0x0302, 'a', 0x00E2}, {0x0302, 'c', 0x0109}, {0x0302, 'e', 0x00EA}, {0x0302, 'g', 0x011D},
    {0x0302, 'h', 0x0125}, {0x0302, 'i', 0x00EE}, {0x0302, 'j', 0x0135}, {0x0302, 'o', 0x00F4}, {0x0302, 's', 0x015D},
    {0x0302, 'u', 0x00FB}, {0x0302, 'w', 0x0175}, {0x0302, 'y', 0x0177}, {0x0303, 'A', 0x00C3}, {0x0303, 'I', 0x0128},
    {0x0303, 'N', 0x00D1}, {0x0303, 'O', 0x00D5}, {0x0303, 'U', 0x0168}, {0x0303, 'a', 0x00E3}, {0x0303, 'i', 0x0129},
    {0x0303, 'n', 0x00F1}, {0x0303, 'o', 0x00F5}, {0x0303, 'u', 0x0169}, {0x0304, 'A', 0x0100}, {0x0304, 'E', 0x0112},
    {0x0304, 'I', 0x012A}, {0x0304, 'O', 0x014C}, {0x0304, 'U', 0x016A}, {0x0304, 'a', 0x0101}, {0x0304, 'e', 0x0113},
    {0x0304, 'i', 0x012B}, {0x0304, 'o', 0x014D}, {0x0304, 'u', 0x016B}, {0x0306, 'A', 0x0102}, {0x0306, 'E', 0x0114},
    {0x0306, 'G', 0x011E}, {0x0306, 'I', 0x012C}, {0x0306, 'O', 0x014E}, {0x0306, 'U', 0x016C}, {0x0306, 'a', 0x0103},
    {0x0306, 'e', 0x0115}, {0x0306, 'g', 0x011F}, {0x0306, 'i', 0x012D}, {0x0306, 'o', 0x014F}, {0x0306, 'u', 0x016D},
    {0x0307, 'C', 0x010A}, {0x0307, 'E', 0x0116}, {0x0307, 'G', 0x0120}, {0x0307, 'I', 0x0130}, {0x0307, 'Z', 0x017B},
    {0x0307, 'c', 0x010B}, {0x0307, 'e', 0x0117}, {0x0307, 'g', 0x0121}, {0x0307, 'z', 0x017C}, {0x0308, 'A', 0x00C4},
    {0x0308, 'E', 0x00CB}, {0x0308, 'I', 0x00CF}, {0x0308, 'O', 0x00D6}, {0x0308, 'U', 0x00DC}, {0x0308, 'Y', 0x0178},
    {0x0308, 'a', 0x00E4}, {0x0308, 'e', 0x00EB}, {0x0308, 'i', 0x00EF}, {0x0308, 'o', 0x00F6}, {0x0308, 'u', 0x00FC},
    {0x0308, 'y', 0x00FF}, {0x030A, 'A', 0x00C5}, {0x030A, 'U', 0x016E}, {0x030A, 'a', 0x00E5}, {0x030A, 'u', 0x016F},
    {0x030B, 'O', 0x0150}, {0x030B, 'U', 0x0170}, {0x030B, 'o', 0x0151}, {0x030B, 'u', 0x0171}, {0x030C, 'C', 0x010C},
    {0x030C, 'D', 0x010E}, {0x030C, 'E', 0x011A}, {0x030C, 'L', 0x013D}, {0x030C, 'N', 0x0147}, {0x030C, 'R', 0x0158},
    {0x030C, 'S', 0x0160}, {0x030C, 'T', 0x0164}, {0x030C, 'Z', 0x017D}, {0x030C, 'c', 0x010D}, {0x030C, 'd', 0x010F},
    {0x030C, 'e', 0x011B}, {0x030C, 'l', 0x013E}, {0x030C, 'n', 0x0148}, {0x030C, 'r', 0x0159}, {0x030C, 's', 0x0161},
    {0x030C, 't', 0x0165}, {0x030C, 'z', 0x017E}, {0x0327, 'C', 0x00C7}, {0x0327, 'G', 0x0122}, {0x0327, 'K', 0x0136},
    {0x0327, 'L', 0x013B}, {0x0327, 'N', 0x0145}, {0x0327, 'R', 0x0156}, {0x0327, 'S', 0x015E}, {0x0327, 'T', 0x0162},
    {0x0327, 'c', 0x00E7}, {0x0327, 'g', 0x0123}, {0x0327, 'k', 0x0137}, {0x0327, 'l', 0x013C}, {0x0327, 'n', 0x0146},
    {0x0327, 'r', 0x0157}, {0x0327, 's', 0x015F}, {0x0327, 't', 0x0163}, {0x0328, 'A', 0x0104}, {0x0328, 'E', 0x0118},
    {0x0328, 'I', 0x012E}, {0x0328, 'U', 0x0172}, {0x0328, 'a', 0x0105}, {0x0328, 'e', 0x0119}, {0x0328, 'i', 0x012F},
    {0x0328, 'u', 0x0173},
}};

// The ASCII form of each letter from U+00C0 to U+017F, case kept ("" for U+00D7 and U+00F7, which are not letters).
constexpr std::array<std::string_view, 0x180 - 0xC0> kStripped = {
    "A", "A", "A", "A", "A", "A", "AE", "C", "E", "E", "E", "E",
    "I", "I", "I", "I", "D", "N", "O", "O", "O", "O", "O", "",
    "O", "U", "U", "U", "U", "Y", "TH", "ss", "a", "a", "a", "a",
    "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "o", "", "o", "u", "u", "u",
    "u", "y", "th", "y", "A", "a", "A", "a", "A", "a", "C", "c",
    "C", "c", "C", "c", "C", "c", "D", "d", "D", "d", "E", "e",
    "E", "e", "E", "e", "E", "e", "E", "e", "G", "g", "G", "g",
    "G", "g", "G", "g", "H", "h", "H", "h", "I", "i", "I", "i",
    "I", "i", "I", "i", "I", "i", "IJ", "ij", "J", "j", "K", "k",
    "k", "L", "l", "L", "l", "L", "l", "L", "l", "L", "l", "N",
    "n", "N", "n", "N", "n", "n", "N", "n", "O", "o", "O", "o",
    "O", "o", "OE", "oe", "R", "r", "R", "r", "R", "r", "S", "s",
    "S", "s", "S", "s", "S", "s", "T", "t", "T", "t", "T", "t",
    "U", "u", "U", "u", "U", "u", "U", "u", "U", "u", "U", "u",
    "W", "w", "Y", "y", "Y", "Z", "z", "Z", "z", "Z", "z", "s",
};

struct Range {
  char32_t first;
  char32_t last;
};

// Emoji, for step 4: Extended_Pictographic's blocks (from Unicode's emoji data, the ranges outside the whitelist
// that matter in speech), regional indicators, skin tones, keycaps, tags, variation selectors and the joiner. A
// character in none of these that the whitelist refuses is counted as Character instead: the reason differs, the
// removal does not.
constexpr std::array<Range, 40> kEmoji = {{
    {0x00A9, 0x00A9},   {0x00AE, 0x00AE},   {0x200D, 0x200D},   {0x203C, 0x203C},   {0x2049, 0x2049},
    {0x20E3, 0x20E3},   {0x2122, 0x2122},   {0x2139, 0x2139},   {0x2194, 0x2199},   {0x21A9, 0x21AA},
    {0x231A, 0x231B},   {0x2328, 0x2328},   {0x23CF, 0x23CF},   {0x23E9, 0x23F3},   {0x23F8, 0x23FA},
    {0x24C2, 0x24C2},   {0x25AA, 0x25AB},   {0x25B6, 0x25B6},   {0x25C0, 0x25C0},   {0x25FB, 0x25FE},
    {0x2600, 0x27BF},   {0x2934, 0x2935},   {0x2B05, 0x2B07},   {0x2B1B, 0x2B1C},   {0x2B50, 0x2B50},
    {0x2B55, 0x2B55},   {0x3030, 0x3030},   {0x303D, 0x303D},   {0x3297, 0x3297},   {0x3299, 0x3299},
    {0xFE00, 0xFE0F},   {0x1F000, 0x1F0FF}, {0x1F10D, 0x1F10F}, {0x1F12F, 0x1F12F}, {0x1F16C, 0x1F171},
    {0x1F17E, 0x1F17F}, {0x1F18E, 0x1F18E}, {0x1F191, 0x1F19A}, {0x1F1AD, 0x1FAFF}, {0xE0020, 0xE007F},
}};

bool continuation(unsigned char b) { return (b & 0xC0) == 0x80; }

}  // namespace

std::size_t decode_utf8(std::string_view raw, bool ended, std::vector<Cp>& out, std::uint32_t offset) {
  std::size_t i = 0;
  const std::size_t n = raw.size();
  while (i < n) {
    const auto b0 = static_cast<unsigned char>(raw[i]);
    const auto at = static_cast<std::uint32_t>(offset + i);
    if (b0 < 0x80) {
      out.push_back(Cp{b0, at, 1});
      ++i;
      continue;
    }
    // The length a valid sequence led by b0 has, and the range its second byte must lie in (Unicode's table 3-7).
    std::size_t len = 0;
    unsigned char lo = 0x80;
    unsigned char hi = 0xBF;
    if (b0 >= 0xC2 && b0 <= 0xDF) {
      len = 2;
    } else if (b0 == 0xE0) {
      len = 3;
      lo = 0xA0;
    } else if ((b0 >= 0xE1 && b0 <= 0xEC) || b0 == 0xEE || b0 == 0xEF) {
      len = 3;
    } else if (b0 == 0xED) {
      len = 3;
      hi = 0x9F;
    } else if (b0 == 0xF0) {
      len = 4;
      lo = 0x90;
    } else if (b0 >= 0xF1 && b0 <= 0xF3) {
      len = 4;
    } else if (b0 == 0xF4) {
      len = 4;
      hi = 0x8F;
    }
    if (len == 0) {  // not a lead byte: one replacement for it
      out.push_back(Cp{kReplacement, at, 1});
      ++i;
      continue;
    }
    // The maximal subpart: as many bytes as still make a valid prefix.
    std::size_t got = 1;
    while (got < len && i + got < n) {
      const auto b = static_cast<unsigned char>(raw[i + got]);
      const bool ok = got == 1 ? (b >= lo && b <= hi) : continuation(b);
      if (!ok) break;
      ++got;
    }
    if (got < len && i + got == n && !ended) return i;  // incomplete at the end: more may come
    if (got < len) {
      out.push_back(Cp{kReplacement, at, static_cast<std::uint8_t>(got)});
      i += got;
      continue;
    }
    char32_t cp = b0 & (len == 2 ? 0x1F : len == 3 ? 0x0F : 0x07);
    for (std::size_t k = 1; k < len; ++k) cp = (cp << 6) | (static_cast<unsigned char>(raw[i + k]) & 0x3F);
    out.push_back(Cp{cp, at, static_cast<std::uint8_t>(len)});
    i += len;
  }
  return n;
}

void append_utf8(std::string& out, char32_t cp) {
  if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = kReplacement;
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

std::size_t utf8_chars(std::string_view text) {
  std::size_t n = 0;
  for (const char c : text) n += continuation(static_cast<unsigned char>(c)) ? 0 : 1;
  return n;
}

char32_t compose(char32_t base, char32_t mark) {
  if (base >= 0x80 || !is_combining_mark(mark)) return 0;
  const auto it = std::lower_bound(kCompositions.begin(), kCompositions.end(), Composition{mark, static_cast<char>(base), 0},
                                   [](const Composition& a, const Composition& b) {
                                     return a.mark != b.mark ? a.mark < b.mark : a.base < b.base;
                                   });
  if (it == kCompositions.end() || it->mark != mark || it->base != static_cast<char>(base)) return 0;
  return it->composed;
}

std::string_view stripped(char32_t cp) {
  if (cp < 0xC0 || cp >= 0x180) return {};
  return kStripped[cp - 0xC0];
}

bool is_letter(char32_t cp) {
  if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z')) return true;
  return !stripped(cp).empty();
}

bool is_upper(char32_t cp) {
  if (cp >= 'A' && cp <= 'Z') return true;
  const std::string_view s = stripped(cp);
  return !s.empty() && s.front() >= 'A' && s.front() <= 'Z';
}

bool is_lower(char32_t cp) {
  if (cp >= 'a' && cp <= 'z') return true;
  const std::string_view s = stripped(cp);
  return !s.empty() && s.front() >= 'a' && s.front() <= 'z';
}

bool is_emoji(char32_t cp) {
  for (const Range& r : kEmoji) {
    if (cp < r.first) return false;  // the ranges are in order
    if (cp <= r.last) return true;
  }
  return false;
}

}  // namespace en::core::speech
