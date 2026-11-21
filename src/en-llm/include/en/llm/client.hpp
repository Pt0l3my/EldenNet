#pragma once
// The model client (ARCH §11): one OpenAI-compatible chat-completions client over the configured providers, with
// each slot's budgets and failure actions, health, the requests-per-hour budget, backup requests, preemption of
// background jobs, and decision calls (decision.hpp).
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/llm/clock.hpp"
#include "en/llm/config.hpp"
#include "en/llm/errors.hpp"
#include "en/llm/health.hpp"
#include "en/llm/message.hpp"
#include "en/llm/stream.hpp"
#include "en/llm/transport.hpp"
#include "en/proto/llm.hpp"

namespace en::llm {

// The request, its attempts and its result are en-proto's (llm.hpp), shared with the core.
using en::proto::AttemptInfo;
using en::proto::ChatRequest;
using en::proto::ChatResult;
using en::proto::Purpose;
using en::proto::to_string;

// ARCH §11.3's order on a provider, highest first: replies (5) > own-initiative lines and the decision calls that
// choose them (4) > bark_live (3) > memory (2) > bark_pool (1); selftest and draft rank with own-initiative lines.
// `slot` is the slot as resolved (Config::slot(name).name).
int priority_of(std::string_view slot, Purpose purpose);

// Receives a reply as it streams, on the thread that called chat().
class StreamSink {
 public:
  virtual ~StreamSink() = default;
  virtual void on_delta(std::string_view text) = 0;
  // The text passed on so far was reasoning after all (a reply that only closed its <think>): withdraw it.
  virtual void on_reset() {}
  // An attempt (the request, a retry, the fallback or a backup) has just been sent; its outcome is not known yet.
  virtual void on_attempt(const AttemptInfo&) {}
};

enum class LogLevel { Debug, Info, Warn };
using LogFn = std::function<void(LogLevel, const std::string&)>;

struct ClientOptions {
  std::shared_ptr<Clock> clock;  // the system's steady clock when null
  LogFn log;                     // log lines name slots, providers, models and classes; never keys or prompt text
  Millis poll_slice{50};         // the longest a waiting request goes without looking at cancellation
};

using en::proto::DecisionCall;
using en::proto::DecisionPick;

class Client {
 public:
  Client(Config config, std::shared_ptr<TransportFactory> transport, ClientOptions options = {});
  ~Client();
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  // Sends a request on its slot and returns when it has ended, after the slot's retries, fallback and backup.
  // Safe to call from several threads at once. Throws std::invalid_argument for a request with no messages.
  ChatResult chat(const ChatRequest& request, StreamSink* sink = nullptr, std::stop_token stop = {});

  // A decision call (ARCH §11.8). A provider's failure never throws: the call's rules decide instead.
  // Throws std::invalid_argument for a malformed call (decision.hpp says what is checked).
  DecisionPick decide(const DecisionCall& call, std::stop_token stop = {});
  // The same, telling `observer` of each attempt as it is sent (on_attempt); the answer's text is not passed on.
  DecisionPick decide(const DecisionCall& call, StreamSink* observer, std::stop_token stop);

  // The offline gate (ARCH §5.1): while it is closed nothing is sent, and running requests are cancelled.
  void set_gate_open(bool open);
  bool gate_open() const;

  // A changed configuration: providers disabled for a rejected key are tried again (ARCH §11.2).
  void reload(Config config);

  HealthReport health() const;
  int requests_last_hour() const;
  std::shared_ptr<const Config> config() const;

  struct Impl;

 private:
  // chat() and decide() both end here: `sink` receives the text, `observer` each attempt as it is sent.
  ChatResult run_chat(const ChatRequest& request, StreamSink* sink, StreamSink* observer, std::stop_token stop);

  std::unique_ptr<Impl> impl_;
};

}  // namespace en::llm
