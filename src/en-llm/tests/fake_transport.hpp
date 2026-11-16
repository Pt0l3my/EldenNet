#pragma once
// A scripted transport for the tests: each request plays a script of timed events on a manual clock, so timeouts,
// backups and cooldowns run in no time. Scripts are queued per model name ("*" for any).
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/llm/clock.hpp"
#include "en/llm/transport.hpp"

namespace en::llm::test {

struct FakeStep {
  Millis at{0};  // after the request was sent
  TransportEventKind kind = TransportEventKind::Data;
  int status = 200;
  std::vector<HttpHeader> headers;
  std::string data;
  std::string error;
};
using FakeScript = std::vector<FakeStep>;

// "data: <chunk>\n\n"
std::string sse_data(const nlohmann::json& chunk);
// A chat-completion chunk with one content delta.
nlohmann::json delta_chunk(const std::string& content);

// A 200 event stream: the deltas (the first at `first`, then one each `gap`), a chunk with the finish reason and,
// where given, usage, then [DONE].
FakeScript sse_reply(const std::vector<std::string>& deltas, const std::string& finish = "stop",
                     Millis first = Millis(100), Millis gap = Millis(20), const nlohmann::json& usage = nullptr);
// A 200 event stream of the given raw pieces at their times, then the end of the body.
FakeScript sse_raw(const std::vector<std::pair<Millis, std::string>>& pieces, Millis end_at);
FakeScript http_error(int status, const std::string& body, std::vector<HttpHeader> headers = {}, Millis at = Millis(50));
// A 200 and then nothing, until the client gives up or cancels.
FakeScript hang(Millis head_at = Millis(10));
FakeScript connection_failure(Millis at = Millis(20), const std::string& error = "Connection reset by peer");

struct Recorded {
  int exchange = 0;
  std::string url;
  std::vector<HttpHeader> headers;
  std::string authorization;  // read back from the secret header, for the tests' checks only
  nlohmann::json body;
  std::string model;
  TimePoint started;
  bool cancelled = false;
  bool finished = false;
  TimePoint ended;
};

class FakeServer final : public TransportFactory {
 public:
  explicit FakeServer(std::shared_ptr<ManualClock> clock);

  void script(const std::string& model, FakeScript script);
  std::unique_ptr<Transport> open() override;

  std::vector<Recorded> requests() const;
  std::size_t active() const;
  // On (the default), a waiting request moves the clock to its next event. Off, only the test moves it, for tests
  // that run requests on several threads.
  void set_auto_advance(bool on);

  struct State;

 private:
  std::shared_ptr<State> state_;
};

}  // namespace en::llm::test
