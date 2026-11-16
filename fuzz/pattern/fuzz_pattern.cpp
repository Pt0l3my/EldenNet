// libFuzzer target: en-gal's pattern grammar (parse_pattern) and multi-pattern matcher (MultiMatcher). The input is a
// pattern's text, a NUL, a byte for the chunk size, and the bytes to search. Besides any sanitizer report, a broken
// property is a finding:
// - what the grammar accepts keeps to its limits (1 to 256 bytes, at least 4 fixed, value within mask), and its
//   canonical text reads back to the same pattern;
// - what it refuses names a problem at a character within the text;
// - the matcher, fed the bytes in overlapping chunks of the chosen size, finds exactly the starts a plain search finds,
//   for the pattern and for a copy of it with one fixed byte made a wildcard; and the pattern planted in the bytes is
//   found where it was planted.
// An exception that escapes is a finding too, so this entry has no ffi_guard (R3 is for the engine's entries).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string_view>
#include <vector>

#include "en/gal/patterns.hpp"

namespace {

using namespace en::gal;

void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

std::vector<std::uint64_t> naive(const BytePattern& p, std::span<const std::byte> bytes) {
  std::vector<std::uint64_t> out;
  for (std::size_t i = 0; i + p.size() <= bytes.size(); ++i) {
    if (p.matches(bytes.subspan(i))) out.push_back(i);
  }
  return out;
}

void check_matcher(const std::vector<BytePattern>& patterns, std::span<const std::byte> bytes, std::size_t chunk) {
  MultiMatcher m(patterns, 1u << 16);
  chunk = std::max(chunk, m.longest() + 1);
  const std::size_t step = chunk - m.overlap();
  m.begin(0);
  for (std::size_t pos = 0; pos < bytes.size(); pos += step) {
    const std::size_t len = std::min(chunk, bytes.size() - pos);
    const bool last = pos + len >= bytes.size();
    m.feed(pos, bytes.subspan(pos, len), last);
    if (last) break;
  }
  m.finish(bytes.size());
  require(m.unscanned() == 0, "nothing unscanned without a gap");
  for (std::size_t i = 0; i < patterns.size(); ++i) {
    const std::vector<std::uint64_t> expected = naive(patterns[i], bytes);
    require(m.count(i) == expected.size(), "the matcher counts what a plain search finds");
    const std::span<const std::uint64_t> found = m.hits(i);
    require(std::equal(found.begin(), found.end(), expected.begin(), expected.end()), "at the same starts");
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  // SAFETY: libFuzzer hands over `size` readable bytes, which live until this call returns; the views read them as
  // the characters and bytes they are, and end where they end.
  const std::string_view input(reinterpret_cast<const char*>(data), size);
  const std::span<const std::byte> raw(reinterpret_cast<const std::byte*>(data), size);
  const std::size_t nul = input.find('\0');
  const std::string_view text = input.substr(0, nul);

  const auto parsed = parse_pattern(text);
  if (!parsed) {
    require(parsed.error().at <= text.size(), "a refusal points into the text");
    require(!describe(parsed.error()).empty(), "a refusal is worded");
    return 0;
  }
  const BytePattern& p = *parsed;
  require(p.size() >= 1 && p.size() <= 256 && p.fixed() >= 4 && p.mask.size() == p.size(), "within the limits");
  for (std::size_t i = 0; i < p.size(); ++i) {
    require((p.value[i] & p.mask[i]) == p.value[i] && (p.mask[i] == 0 || p.mask[i] == 0xFF), "value within mask");
  }
  const auto again = parse_pattern(p.text());
  require(again.has_value() && *again == p, "the canonical text reads back to the same pattern");
  require(p.text().size() == 3 * p.size() - 1, "two characters a byte, a space between");

  if (nul == std::string_view::npos || nul + 1 >= size) return 0;
  const std::size_t chunk = 1 + std::to_integer<std::size_t>(raw[nul + 1]) * 3;
  std::vector<std::byte> bytes(raw.begin() + static_cast<std::ptrdiff_t>(nul + 2), raw.end());

  BytePattern looser = p;  // one fixed byte made a wildcard, where that still leaves four
  for (std::size_t i = 0; i < looser.size() && looser.fixed() > 4; ++i) {
    if (looser.mask[i] == 0xFF) {
      looser.mask[i] = 0;
      looser.value[i] = 0;
      break;
    }
  }
  check_matcher({p, looser}, bytes, chunk);

  if (bytes.size() >= p.size()) {  // planted where the bytes say, it is found there
    const std::size_t at = bytes.empty() ? 0 : std::to_integer<std::size_t>(bytes[0]) % (bytes.size() - p.size() + 1);
    for (std::size_t i = 0; i < p.size(); ++i) {
      if (p.mask[i] != 0) bytes[at + i] = std::byte{p.value[i]};
    }
    MultiMatcher m(std::vector<BytePattern>{p});
    m.begin(0);
    m.feed(0, bytes, true);
    const std::span<const std::uint64_t> hits = m.hits(0);
    require(std::find(hits.begin(), hits.end(), at) != hits.end() || m.count(0) > hits.size(),
            "a planted pattern is found");
  }
  return 0;
}
