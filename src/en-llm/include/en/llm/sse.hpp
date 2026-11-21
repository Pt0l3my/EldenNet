#pragma once
// A server-sent-events parser of our own (ARCH §11.1): data lines, events of several data lines, comments, the
// event, id and retry fields, any line ending, bytes split anywhere. `[DONE]` is a payload, read by the assembler.
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace en::llm {

// ARCH §10.2: a stream's line is at most 64 KB.
inline constexpr std::size_t kMaxSseLine = std::size_t{64} << 10;

struct SseEvent {
  std::string type = "message";
  std::string data;  // the event's data lines joined by "\n"
  std::string id;    // the last event id seen, as the spec carries it forward
};

class SseParser {
 public:
  // Lines longer than `max_line` bytes stop the parse (a server sending garbage should not eat memory).
  explicit SseParser(std::size_t max_line = kMaxSseLine) : max_line_(max_line) {}

  // Appends every event the bytes complete. Returns false once a line passed the limit; the parser then stays failed.
  bool feed(std::string_view bytes, std::vector<SseEvent>& out);

  // At the end of the stream: an event left without its closing blank line is still dispatched. The spec drops it,
  // but servers do end streams with "data: [DONE]\n" and a closed connection, and that must not lose the marker.
  void finish(std::vector<SseEvent>& out);

  std::optional<int> retry_ms() const { return retry_ms_; }
  bool failed() const { return failed_; }

 private:
  void line(std::vector<SseEvent>& out);
  void dispatch(std::vector<SseEvent>& out);

  std::size_t max_line_;
  std::string line_;
  bool after_cr_ = false;    // the last byte was CR: a following LF belongs to the same line ending
  bool first_line_ = true;   // a byte-order mark may open the stream
  bool failed_ = false;
  std::string data_;
  bool has_data_ = false;
  std::string type_;
  std::string last_id_;
  std::optional<int> retry_ms_;
};

}  // namespace en::llm
