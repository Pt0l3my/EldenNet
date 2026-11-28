#pragma once
// mockllm: a scripted model for the eval and the tests (DESIGN §14, ARCH §12.3; WHY§364 (10)). It answers the core's
// requests, in the order they come, with the replies of a script on the caller's clock, as the model client's events
// (proto::LlmEvent: Started, the Deltas, Reset, Ended) would arrive from en-llm: no network, no thread, no clock of its
// own, so a run is the same every time. A script scripts delays, the reply's text cut into chunks, a `<think>` leak, a
// trailer, a withdrawal (Reset) and the error classes en-llm ends with (a 429 is RateLimited, a timeout TotalTimeout),
// once en-llm's own retries and fallback are spent. A request cancelled ends with Cancelled at once.
//
// The script (TOML, ours):
//
//   schema = 1
//   [[reply]]                       # one per request, in order; the last repeats when `repeat_last` (default true)
//   text = "Hm. The road runs on."  # what streams; or a list of pieces, joined by one space
//   first_token_ms = 400            # from the request to its first delta
//   chunk_ms = 60                   # between deltas
//   chunk_chars = 12                # each delta's length in bytes (cut on a character's boundary)
//   think = "..."                   # streamed first, inside <think></think>, as a leak past en-llm
//   reset = true                    # a Reset after the think part: what streamed was reasoning after all
//   trailer = "ACT: ..."            # streamed after the text, on a line of its own
//   error = "RateLimited"           # en-llm's error class; with text, after the text has streamed
//   end_ms = 0                      # from the last delta to the Ended event
//
// Text in a script is invented and ours; a script never holds a key.
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/gate.hpp"  // Mono
#include "en/proto/expected.hpp"
#include "en/proto/llm.hpp"

namespace en::tools {

struct MockReply {
  std::string text;
  std::string think;
  std::string trailer;
  bool reset = false;
  core::Mono first_token_ms = 400;
  core::Mono chunk_ms = 60;
  std::size_t chunk_chars = 12;
  core::Mono end_ms = 0;
  std::optional<proto::ErrorClass> error;
  // The request's slot it answers (v0.3): each slot takes its own replies in order, so a memory job's request between
  // two lines never takes a line's reply. Default the dialogue slot.
  std::string slot = "dialogue";

  friend bool operator==(const MockReply&, const MockReply&) = default;
};

struct MockScript {
  std::vector<MockReply> replies;
  bool repeat_last = true;
};

// The script, or what is wrong with it ("line 4: reply[0].error: must be one of ..."), never quoting a value.
[[nodiscard]] proto::Expected<MockScript, std::string> parse_mock_script(std::string_view text);

// One scheduled event.
struct MockEvent {
  core::Mono at = 0;
  proto::LlmEvent event;
};

class MockModel {
 public:
  explicit MockModel(MockScript script) : script_(std::move(script)) {}

  // A request at `now`: its events are scheduled from the next reply of the script (with none left, it ends with
  // NoHealthyProvider). Decision calls are answered by their rules (DecidedBy::Rules), at once.
  void submit(const proto::LlmRequest& request, core::Mono now);
  // A live request cancelled: its events not yet due are dropped and it ends with Cancelled at `now`.
  void cancel(proto::RequestId id, core::Mono now);
  // The events due at or before `now`, in their order.
  [[nodiscard]] std::vector<proto::LlmEvent> due(core::Mono now);
  [[nodiscard]] std::optional<core::Mono> next_at() const;
  [[nodiscard]] std::size_t served() const noexcept { return served_; }
  [[nodiscard]] std::size_t cancelled() const noexcept { return cancelled_; }

 private:
  MockScript script_;
  std::vector<MockEvent> pending_;  // ordered by time, then by when they were scheduled
  std::size_t served_ = 0;
  std::size_t cancelled_ = 0;
  std::map<std::string, std::size_t> next_by_slot_;
};

// The events a script gives its first `requests` requests, each submitted at 0 and run to its end: what `mockllm`
// prints, one JSON line each ({"request": n, "after_ms": t, "event": {...}}), for checking a script.
[[nodiscard]] std::vector<std::string> mock_lines(const MockScript& script, std::size_t requests);

// A recording of a model's events for `--llm recorded`: the same lines, each the nth request's event at its time from
// that request's start. Replayed with the request's live id in place of the recorded one.
struct RecordedEvent {
  std::size_t request = 0;  // from 1
  core::Mono after_ms = 0;
  proto::LlmEvent event;
};
[[nodiscard]] proto::Expected<std::vector<RecordedEvent>, std::string> parse_recorded(std::string_view text);

class RecordedModel {
 public:
  explicit RecordedModel(std::vector<RecordedEvent> events) : events_(std::move(events)) {}
  void submit(const proto::LlmRequest& request, core::Mono now);
  void cancel(proto::RequestId id, core::Mono now);
  [[nodiscard]] std::vector<proto::LlmEvent> due(core::Mono now);
  [[nodiscard]] std::optional<core::Mono> next_at() const;
  [[nodiscard]] std::size_t served() const noexcept { return served_; }

 private:
  std::vector<RecordedEvent> events_;
  std::vector<MockEvent> pending_;
  std::size_t served_ = 0;
};

}  // namespace en::tools
