// libFuzzer target: en-llm's SSE line parser and stream assembler, driven as the model client drives them
// (client.cpp), with ARCH §10.2's caps. The input is a streamed reply's body after a header of three bytes: how the
// transport cuts the body into chunks, the line cap and the visible text's cap (each the built cap below 0x80, else a
// small one, to reach the cap's edge). Besides any sanitizer report, a broken property is a finding:
// - the parser finds the same events, and fails or not alike, however the body is cut;
// - the visible text is what the steps delivered (a reset withdrawing it), at most its cap, in whole UTF-8 characters;
// - what complete_sentences keeps ends at a sentence;
// - a server's error text, in the stream or as an error body, keeps nothing shaped like a key.
// An exception that escapes is a finding too, so this entry has no ffi_guard (R3 is for the engine's entries).
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "en/llm/sse.hpp"
#include "en/llm/stream.hpp"

namespace {

using en::llm::ReplyAssembler;
using en::llm::SseEvent;
using en::llm::SseParser;

// A broken property ends the run as a crash does, so that libFuzzer keeps the input.
void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

// The chunk sizes a transport might deliver, from 1 to `max` bytes: a small generator of our own (xorshift64).
class Cuts {
 public:
  Cuts(std::uint8_t byte, std::size_t size)
      : max_(std::size_t{1} + (byte & 0x3Fu)), state_(0x9E3779B97F4A7C15u ^ (std::uint64_t{byte} << 32) ^ size) {}

  std::size_t next() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 7;
    state_ ^= state_ << 17;
    return 1 + static_cast<std::size_t>(state_ % max_);
  }

 private:
  std::size_t max_;
  std::uint64_t state_;
};

// The parser fed in the chunks `cuts` gives, each copied into a buffer of its own size, so that reading past a chunk's
// end is a sanitizer report. It stops at the first chunk it refuses, as the client does.
SseParser parse_in_chunks(std::string_view body, std::size_t max_line, Cuts cuts, std::vector<SseEvent>& events) {
  SseParser parser(max_line);
  while (!body.empty()) {
    const std::string_view piece = body.substr(0, cuts.next());
    body.remove_prefix(piece.size());
    const std::vector<char> chunk(piece.begin(), piece.end());
    if (!parser.feed(std::string_view(chunk.data(), chunk.size()), events)) break;
  }
  parser.finish(events);
  return parser;
}

bool same_events(const std::vector<SseEvent>& a, const std::vector<SseEvent>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].type != b[i].type || a[i].data != b[i].data || a[i].id != b[i].id) return false;
  }
  return true;
}

// Well-formed UTF-8 (RFC 3629): no stray continuation byte, no cut sequence, no overlong form, no surrogate.
bool whole_utf8(std::string_view s) {
  std::size_t i = 0;
  while (i < s.size()) {
    const auto lead = static_cast<unsigned char>(s[i]);
    std::size_t more = 0;
    std::uint32_t code = 0;
    if (lead < 0x80) {
      ++i;
      continue;
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
      more = 1;
      code = lead & 0x1Fu;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      more = 2;
      code = lead & 0x0Fu;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      more = 3;
      code = lead & 0x07u;
    } else {
      return false;
    }
    if (s.size() - i <= more) return false;
    for (std::size_t k = 1; k <= more; ++k) {
      const auto next = static_cast<unsigned char>(s[i + k]);
      if ((next & 0xC0u) != 0x80u) return false;
      code = (code << 6) | (next & 0x3Fu);
    }
    if ((more == 2 && code < 0x800) || (more == 3 && (code < 0x10000 || code > 0x10FFFF)) ||
        (code >= 0xD800 && code <= 0xDFFF)) {
      return false;
    }
    i += more + 1;
  }
  return true;
}

bool token_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

// Nothing shaped like a key, as stream.cpp's scrub promises: no run of 24 or more token characters, none that opens
// with "sk-" or "sk_", and no token right after "Bearer ".
bool keyless(std::string_view text) {
  std::size_t i = 0;
  while (i < text.size()) {
    if (!token_char(text[i])) {
      ++i;
      continue;
    }
    std::size_t end = i;
    while (end < text.size() && token_char(text[end])) ++end;
    const std::string_view word = text.substr(i, end - i);
    if (word.size() >= 24 || word.starts_with("sk-") || word.starts_with("sk_")) return false;
    i = end;
  }
  constexpr std::string_view kBearer = "Bearer ";
  for (std::size_t at = text.find(kBearer); at != std::string_view::npos; at = text.find(kBearer, at + 1)) {
    const std::size_t after = at + kBearer.size();
    if (after < text.size() && token_char(text[after])) return false;
  }
  return true;
}

std::string trimmed_end(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
  return s;
}

// What the client's sink was shown: each step's delta, after a reset withdrew what came before (client.cpp's apply).
void show(const ReplyAssembler::Step& step, std::string& shown) {
  if (step.reset) shown.clear();
  shown += step.delta;
}

bool stopped(const ReplyAssembler& reply) { return !reply.refused().empty() || reply.overflowed(); }

void check_reply(const ReplyAssembler& reply, const std::string& shown, bool finished, std::size_t max_text) {
  const std::string& text = reply.text();
  require(text == (finished ? trimmed_end(shown) : shown), "the visible text is what the steps delivered");
  require(text.size() <= max_text, "the visible text stays within its cap");
  require(whole_utf8(text), "the visible text holds whole UTF-8 characters");
  require(keyless(reply.stream_error_message()), "an error in the stream keeps nothing shaped like a key");
  const std::string kept = en::llm::complete_sentences(text);
  require(kept.empty() || en::llm::ends_at_sentence(kept), "complete_sentences keeps whole sentences");
  static_cast<void>(en::llm::looks_like_refusal(text));
}

// The events as the client applies them: it stops at a refused chunk or at the text's cap, and finishes otherwise.
void assemble_stream(const std::vector<SseEvent>& events, std::size_t max_text) {
  ReplyAssembler reply(max_text);
  std::string shown;
  for (const SseEvent& event : events) {
    show(reply.on_event_data(event.data), shown);
    if (stopped(reply)) {
      check_reply(reply, shown, false, max_text);
      return;
    }
  }
  show(reply.finish(), shown);
  check_reply(reply, shown, true, max_text);
}

// The body as one JSON reply, from a server that ignored stream: true.
void assemble_whole(std::string_view body, std::size_t max_text) {
  ReplyAssembler reply(max_text);
  std::string shown;
  show(reply.on_whole_body(body), shown);
  if (stopped(reply)) {
    check_reply(reply, shown, false, max_text);
    return;
  }
  show(reply.finish(), shown);
  check_reply(reply, shown, true, max_text);
}

constexpr std::array<int, 6> kStatuses = {400, 401, 404, 429, 500, 503};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size < 3) return 0;
  // SAFETY: libFuzzer hands over `size` readable bytes, which live until this call returns; the view reads them as
  // the characters they are, and ends where they end, so a read past the body's end is a sanitizer report.
  const std::string_view input(reinterpret_cast<const char*>(data), size);
  const auto header = [&](std::size_t i) { return static_cast<std::uint8_t>(input[i]); };
  const std::size_t max_line = header(1) < 0x80 ? en::llm::kMaxSseLine : std::size_t{1} + (header(1) & 0x7Fu);
  const std::size_t max_text = header(2) < 0x80 ? en::llm::kMaxReplyText : std::size_t{1} + (header(2) & 0x7Fu);
  const std::string_view body = input.substr(3);

  std::vector<SseEvent> whole_events;
  SseParser whole(max_line);
  whole.feed(body, whole_events);
  whole.finish(whole_events);
  std::vector<SseEvent> events;
  const SseParser cut = parse_in_chunks(body, max_line, Cuts(header(0), body.size()), events);
  require(cut.failed() == whole.failed(), "the parser fails alike however the body is cut");
  require(same_events(events, whole_events), "the parser finds the same events however the body is cut");
  require(cut.retry_ms() == whole.retry_ms(), "the parser reads the same retry however the body is cut");

  assemble_stream(events, max_text);
  assemble_whole(body, max_text);
  const int status = kStatuses[header(1) % kStatuses.size()];
  require(keyless(en::llm::describe_error_body(body, status)), "an error body keeps nothing shaped like a key");
  return 0;
}
