// libFuzzer target: en-core's sanitizer (NPC §8.2 steps 0 to 7; speech_sanitizer.hpp) on a stream fed in pieces. The
// input's first byte chooses how the rest is cut (each later byte value below the first one is a cut, and stays in the
// stream); the stream is also fed whole and, up to 1 KB, one byte at a time. Besides any sanitizer report, a broken property is a
// finding:
// - the three feedings give the same sentences (texts, indexes and removals), flow and intent, for a stream of at most
//   4 KB (past it, what the cap cuts depends on the delta that crossed it: the texts must still agree);
// - every sentence is non-empty valid UTF-8 of the whitelist's characters, trimmed, its spaces collapsed, no token
//   longer than 24 characters, its index one more than the last, and every removal more than 0 bytes.
// An exception that escapes is a finding too.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/speech_sanitizer.hpp"

namespace {

void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

struct Run {
  std::vector<en::core::Sentence> sentences;
  en::core::Flow flow = en::core::Flow::More;
  std::optional<std::string> intent;
};

Run feed(std::string_view text, const std::vector<std::size_t>& cuts) {
  const std::vector<std::string> names = {"Brannoch", "Ysolde of the Weir"};
  const en::core::SpeechSanitizer sanitizer;
  auto stream = sanitizer.open(en::core::SanitizeContext{names});
  Run r;
  std::size_t at = 0;
  for (std::size_t cut : cuts) {
    if (cut <= at || cut > text.size()) continue;
    const en::core::Flow f = stream->feed(text.substr(at, cut - at), r.sentences);
    if (f != en::core::Flow::More) r.flow = f;
    at = cut;
  }
  const en::core::Flow f = stream->feed(text.substr(at), r.sentences);
  if (f != en::core::Flow::More) r.flow = f;
  stream->finish(r.sentences);
  r.intent = stream->intent();
  return r;
}

bool valid_utf8(std::string_view s) {
  std::size_t i = 0;
  while (i < s.size()) {
    const auto b = static_cast<unsigned char>(s[i]);
    const std::size_t len = b < 0x80 ? 1 : (b >> 5) == 0x6 ? 2 : (b >> 4) == 0xE ? 3 : (b >> 3) == 0x1E ? 4 : 0;
    if (len == 0 || i + len > s.size()) return false;
    for (std::size_t k = 1; k < len; ++k) {
      if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
    }
    i += len;
  }
  return true;
}

void check_shape(const Run& r) {
  std::size_t next = 0;
  for (const en::core::Sentence& s : r.sentences) {
    require(!s.text.empty(), "a sentence is never empty");
    require(s.index == next++, "indexes count from 0");
    require(valid_utf8(s.text), "a sentence is valid UTF-8");
    require(s.text.front() != ' ' && s.text.back() != ' ', "a sentence is trimmed");
    require(s.text.find("  ") == std::string::npos, "spaces are collapsed");
    for (const char c : s.text) {
      const auto u = static_cast<unsigned char>(c);
      const bool ascii_ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                            std::string_view(" .,;:!?'-").find(c) != std::string_view::npos;
      require(u >= 0x80 || ascii_ok, "ASCII outside the whitelist is never kept");
    }
    // No token past 24 characters, each "…" counted as the three of "..." it became.
    std::size_t token = 0;
    for (std::size_t i = 0; i <= s.text.size(); ++i) {
      if (i == s.text.size() || s.text[i] == ' ') {
        require(token <= 24 * 3, "no token past 24 characters");
        token = 0;
      } else if ((static_cast<unsigned char>(s.text[i]) & 0xC0) != 0x80) {
        ++token;
      }
    }
    for (const en::core::Removal& m : s.removed) require(m.bytes > 0, "a removal is never of 0 bytes");
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size == 0) return 0;
  // SAFETY: libFuzzer hands over `size` readable bytes, which live until this call returns; the view reads them as
  // the characters they are, and ends where they end.
  const std::string_view text(reinterpret_cast<const char*>(data) + 1, size - 1);
  const auto below = static_cast<unsigned char>(data[0]);
  std::vector<std::size_t> cuts;
  std::vector<std::size_t> bytes;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (static_cast<unsigned char>(text[i]) < below) cuts.push_back(i);
    bytes.push_back(i + 1);
  }
  const Run whole = feed(text, {});
  const Run cut = feed(text, cuts);
  // One byte at a time reads the stream again at each byte: up to 1 KB, to keep the target fast.
  const Run bytewise = text.size() <= 1024 ? feed(text, bytes) : cut;
  check_shape(whole);
  check_shape(cut);
  check_shape(bytewise);
  require(whole.sentences.size() == cut.sentences.size() && whole.sentences.size() == bytewise.sentences.size(),
          "a stream gives as many sentences however it is cut");
  const bool capped = text.size() > en::core::kRawStreamMax;
  for (std::size_t i = 0; i < whole.sentences.size(); ++i) {
    require(whole.sentences[i].text == cut.sentences[i].text && whole.sentences[i].text == bytewise.sentences[i].text,
            "a stream gives the same sentences however it is cut");
    if (!capped) {
      require(whole.sentences[i] == cut.sentences[i] && whole.sentences[i] == bytewise.sentences[i],
              "a stream gives the same removals however it is cut");
    }
  }
  require(whole.flow == cut.flow && whole.flow == bytewise.flow, "the flow is the same however it is cut");
  if (!capped) require(whole.intent == cut.intent && whole.intent == bytewise.intent, "the intent is the same");
  return 0;
}
