#pragma once
// From a chat completion's events to the reply: content deltas, `<think>` stripped, the finish reason, usage and
// logprobs (ARCH §11.1), and the end-of-reply checks behind Empty, Truncated and Refused (ARCH §11.2).
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/llm/errors.hpp"
#include "en/proto/llm.hpp"

namespace en::llm {

using en::proto::TokenLogprob;
using en::proto::TopLogprob;
using en::proto::Usage;

// ARCH §10.2's caps on a reply, beside the stream's 64 KB lines (sse.hpp): a whole response at most 1 MB, and a stream
// that passes 16 KB of display text is cancelled. Each event's JSON is held to en-proto's caps (256 KB, 32 deep)
// before it is parsed (en/proto/json.hpp).
inline constexpr std::size_t kMaxResponse = std::size_t{1} << 20;
inline constexpr std::size_t kMaxReplyText = std::size_t{16} << 10;

// Removes `<think>…</think>` from streamed text, tags split across chunks included. A reply that opens with
// reasoning and only closes it (the template put the opening tag in the prompt) is caught too: the closing tag
// reports `reset`, and everything before it is dropped.
class ThinkFilter {
 public:
  struct Output {
    std::string text;
    bool reset = false;  // the text passed on before this chunk was reasoning
  };
  Output feed(std::string_view chunk);
  // Releases what was held back for a possible tag; an unclosed `<think>` drops the rest.
  std::string finish();

 private:
  bool inside_ = false;
  bool seen_open_ = false;
  std::string held_;
};

class ReplyAssembler {
 public:
  struct Step {
    std::string delta;   // visible text this event added
    bool reset = false;  // the visible text so far was reasoning and is withdrawn
  };

  // `max_text`: the visible text it keeps (kMaxReplyText unless given). What would take the text past it is cut there,
  // at a character's boundary, and the reply is over: overflowed() says so, and nothing after it is read.
  ReplyAssembler() = default;
  explicit ReplyAssembler(std::size_t max_text) : max_text_(max_text) {}

  // One SSE event's data: "[DONE]" or a JSON chunk. A chunk past en-proto's JSON caps is not parsed: refused() says why.
  Step on_event_data(std::string_view data);
  // A reply that came as one JSON body (a server that ignored stream: true), held to the same caps.
  Step on_whole_body(std::string_view body);
  // The end of the stream: releases held-back text and trims the reply.
  Step finish();

  const std::string& text() const { return text_; }      // visible: think stripped, outer whitespace trimmed
  const std::string& raw_text() const { return raw_; }   // the content exactly as sent, for the logprobs
  bool done_seen() const { return done_; }
  const std::string& finish_reason() const { return finish_reason_; }
  const Usage& usage() const { return usage_; }
  const std::vector<TokenLogprob>& logprobs() const { return logprobs_; }
  const std::string& served_model() const { return served_model_; }
  // An error object inside the stream (OpenRouter sends them mid-stream), with its class and a safe message.
  std::optional<ErrorClass> stream_error() const { return stream_error_; }
  const std::string& stream_error_message() const { return stream_error_message_; }
  int bad_chunks() const { return bad_chunks_; }
  bool saw_reasoning() const { return reasoning_chars_ > 0; }
  // The visible text passed max_text: the stream is to be cancelled, and the text is what fitted.
  bool overflowed() const { return overflowed_; }
  // Why a chunk or body was not parsed (past the JSON caps), in our own words; empty when none was refused.
  const std::string& refused() const { return refused_; }

 private:
  Step on_chunk(const nlohmann::json& chunk, bool whole);
  Step add_content(std::string_view content);
  void append_visible(std::string_view text, Step& step);

  ThinkFilter think_;
  std::string text_;
  std::string raw_;
  bool done_ = false;
  std::string finish_reason_;
  Usage usage_;
  std::vector<TokenLogprob> logprobs_;
  std::string served_model_;
  std::optional<ErrorClass> stream_error_;
  std::string stream_error_message_;
  int bad_chunks_ = 0;
  long reasoning_chars_ = 0;
  std::size_t max_text_ = kMaxReplyText;
  bool overflowed_ = false;
  std::string refused_;
};

// The class an error object gives: {"error": {"code": 429, ...}} and the like.
ErrorClass classify_error_object(const nlohmann::json& error);

// Whether the text ends a sentence (., !, ? or an ellipsis, before closing quotes or brackets).
bool ends_at_sentence(std::string_view text);

// The text up to its last complete sentence (empty when there is none), for a reply cut by its token budget.
std::string complete_sentences(std::string_view text);

// Whether a reply opens the way an assistant's refusal does ("As an AI…", "I can't help with that…"). Only phrases
// no Lands Between speaker would open with are listed, so an NPC's own "I can't let you pass" is never one.
bool looks_like_refusal(std::string_view text);

// A short, safe description of a server's error body: its message, cut to 200 bytes, with anything shaped like a
// key replaced. Auth errors never carry the server's text (some echo part of the key).
std::string describe_error_body(std::string_view body, int status);

// Whether an error body names reasoning or thinking: the sign that the model refuses reasoning off (ARCH §11.2).
bool names_reasoning(std::string_view body);

}  // namespace en::llm
