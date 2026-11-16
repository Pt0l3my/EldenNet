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

namespace en::llm {

// Why a request goes out. It decides what happens at the requests-per-hour bound (ARCH §11.4), whether a reply
// gets a backup request (ARCH §11.2), and, with the slot, the priority on a provider (ARCH §11.3).
enum class Purpose {
  Reply,          // the player asked: sent even past the bound
  OwnInitiative,  // an NPC's own line: past the bound its seed or silence (BudgetSpent)
  Decision,       // a decision call: past the bound its rules
  Background,     // pools and memory: past the bound they wait
};

// ARCH §11.3's order on a provider, highest first: replies (5) > own-initiative lines and the decision calls that
// choose them (4) > bark_live (3) > memory (2) > bark_pool (1); selftest and draft rank with own-initiative lines.
// `slot` is the slot as resolved (Config::slot(name).name).
int priority_of(std::string_view slot, Purpose purpose);

struct ChatRequest {
  std::string slot = std::string(kDialogue);  // unknown names fall through to dialogue
  Purpose purpose = Purpose::Reply;
  std::vector<Message> messages;
  std::optional<int> max_tokens;              // at most the slot's budget
  std::optional<double> temperature;          // the slot's when unset
  std::optional<nlohmann::json> json_schema;  // the reply is held to it where the provider takes a schema
  int top_logprobs = 0;                       // where the provider offers logprobs
};

struct AttemptInfo;

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

struct AttemptInfo {
  std::string provider;
  std::string model;
  ErrorClass error = ErrorClass::None;
  int http_status = 0;
  bool backup = false;          // started beside a late reply (ARCH §11.2)
  bool stripped = false;        // sent without the optional fields
  bool minimal_effort = false;  // sent at the lowest reasoning effort
  bool lost_race = false;       // cancelled because the other attempt streamed first: no failure
  Millis sent_at{0};            // from the request's start
  Millis first_token{-1};       // from its own send; -1 when it never streamed
  Millis ended_at{0};           // from the request's start
  std::string message;
};

struct ChatResult {
  ErrorClass error = ErrorClass::None;
  // What went wrong: the client's own words and, for an HTTP error, the server's message cut to 200 bytes with
  // anything shaped like a credential replaced (never for a rejected key). Never a key. The client's own log lines
  // carry none of it.
  std::string message;
  std::string text;          // the reply, <think> stripped; what streamed so far when it broke after starting
  std::string raw_text;      // the content exactly as sent, which the decision reader maps logprobs onto
  std::string finish_reason;
  std::string provider;      // the attempt that answered, or that failed last
  std::string model;
  std::string served_model;  // as the provider named it
  Usage usage;
  std::vector<TokenLogprob> logprobs;
  bool used_schema = false;
  bool used_logprobs = false;
  bool streamed = false;     // some text reached the sink (no retry follows that: the caller shows what came)
  Millis first_token{-1};    // from the request's start to the first visible text
  Millis total{0};
  std::vector<AttemptInfo> attempts;

  bool ok() const { return error == ErrorClass::None; }
};

enum class LogLevel { Debug, Info, Warn };
using LogFn = std::function<void(LogLevel, const std::string&)>;

struct ClientOptions {
  std::shared_ptr<Clock> clock;  // the system's steady clock when null
  LogFn log;                     // log lines name slots, providers, models and classes; never keys or prompt text
  Millis poll_slice{50};         // the longest a waiting request goes without looking at cancellation
};

struct DecisionCall;
struct DecisionPick;

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
