#pragma once
// What the core asks of the model client and what comes back (ARCH §2.2's LlmRequest and LlmEvent, with the request,
// decision and result types they carry), as plain data: they cross the core's thread and en-llm's (C9, C10), and each
// has a JSON form (to_json and from_json, below; json.cpp) for recordings and replays. Everything that acts on them
// is en-llm's: slots, budgets, retries, health, how a decision's answer is read.
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace en::proto {

// ---- Slots (ARCH §11.4): the core names the one a request goes on; unknown names fall through to dialogue ----

inline constexpr std::string_view kDialogue = "dialogue";
inline constexpr std::string_view kBarkLive = "bark_live";
inline constexpr std::string_view kBarkPool = "bark_pool";
inline constexpr std::string_view kMemory = "memory";
inline constexpr std::string_view kSelftest = "selftest";
inline constexpr std::string_view kDraft = "draft";
inline constexpr std::string_view kDecisions = "decisions";
// Reserved for later uses (O88; ARCH §11.4): the configuration takes them, so that its files keep their shape when the
// features arrive, but no request uses them yet; one that names them falls through to dialogue, as an unknown name
// does.
inline constexpr std::string_view kActions = "actions";  // NPC actions, after v1 (O31 to O42)
inline constexpr std::string_view kVision = "vision";    // letting an NPC see the screen; not designed yet

// ---- Messages, as en-prompt hands them over (NPC §7.1's markers), with their cache marks ----

enum class Role { System, User, Assistant };

// A piece of a message's text. `cache_break` marks a cache breakpoint after it: `[system cache=A]` ends with one,
// and so do block 5 and block 5b (NPC §7.1). Providers with explicit cache control get the marks; the rest get the
// same text, in the same order, and nothing else.
struct Part {
  std::string text;
  bool cache_break = false;

  bool operator==(const Part&) const = default;
};

struct Message {
  Role role = Role::User;
  std::vector<Part> parts;

  static Message system(std::string text, bool cache_break = false);
  static Message user(std::string text);
  static Message assistant(std::string text);

  // The parts joined exactly as they are. A part that starts a new paragraph carries its own blank line, as
  // en-prompt's parts do (NPC §7.1), so the model reads the same words whether the parts travel apart or joined.
  std::string joined() const;
  bool has_cache_break() const;

  bool operator==(const Message&) const = default;
};

// Drops empty parts, moving an empty part's mark to the part before it, since providers refuse empty text blocks.
void normalise(Message& message);

// ---- Chat requests and their results ----

// Why a request goes out. It decides what happens at the requests-per-hour bound (ARCH §11.4), whether a reply
// gets a backup request (ARCH §11.2), and, with the slot, the priority on a provider (ARCH §11.3).
enum class Purpose {
  Reply,          // the player asked: sent even past the bound
  OwnInitiative,  // an NPC's own line: past the bound its seed or silence (BudgetSpent)
  Decision,       // a decision call: past the bound its rules
  Background,     // pools and memory: past the bound they wait
};

struct ChatRequest {
  std::string slot = std::string(kDialogue);  // unknown names fall through to dialogue
  Purpose purpose = Purpose::Reply;
  std::vector<Message> messages;
  std::optional<int> max_tokens;              // at most the slot's budget
  std::optional<double> temperature;          // the slot's when unset
  std::optional<nlohmann::json> json_schema;  // the reply is held to it where the provider takes a schema
  int top_logprobs = 0;                       // where the provider offers logprobs
  // Stop sequences: the reply ends before the first of them the model writes (a dialogue's two, NPC §7.3). en-llm sends
  // the first four that are not empty, as `stop`, to every flavour, and none in a stripped retry, so a model that
  // refuses the field (OpenAI's reasoning models) is still answered; the sanitizer cuts the same run-on in any case.
  std::vector<std::string> stop;

  bool operator==(const ChatRequest&) const = default;
};

// Error classes (ARCH §11.2) and the client's own outcomes.
enum class ErrorClass : std::uint8_t {
  None,               // success
  // ARCH §11.2's classes
  FirstTokenTimeout,  // no content delta by the slot's first-token time
  TotalTimeout,       // the slot's total time passed
  RateLimited,        // HTTP 429, or a rate-limit error inside the stream
  Auth,               // HTTP 401 or 403, or a provider with no key
  BadRequest,         // HTTP 400, 404, 422 and other 4xx: our request
  Server,             // HTTP 5xx, a connection error, a broken stream
  Empty,              // the reply ended with no content
  Truncated,          // finish_reason = length before a full sentence, or a reply past 16 KB of text (ARCH §10.2)
  Refused,            // a content-filter finish, or a refusal pattern
  // the client's own outcomes: nothing was wrong with the provider
  Cancelled,          // the caller cancelled (the input epoch, shutdown)
  Preempted,          // a higher-priority job took the provider's connection (ARCH §11.3); the job requeues
  Busy,               // the slot or every healthy provider is at its concurrency; nothing was sent
  BudgetSpent,        // the requests per hour are spent and this request is not a reply (ARCH §11.4)
  NoHealthyProvider,  // every model of the slot is cooling, disabled or missing
  GateClosed,         // the offline gate is false (ARCH §5.1): nothing goes out
  Dropped,            // the request queue was full and a higher-priority request took a bark_pool job's place (ARCH
                      // §1.3, C9): nothing was sent, and the job is not requeued
};

struct Usage {
  int prompt_tokens = -1;  // -1: not reported
  int completion_tokens = -1;
  int cached_tokens = -1;
  // What the call cost, where the provider's reply says (its usage's `cost`, as OpenRouter reports it, in its own
  // unit): never worked out from a price table (D18). -1: not reported.
  double cost = -1;

  bool operator==(const Usage&) const = default;
};

struct TopLogprob {
  std::string token;
  double logprob = 0.0;

  bool operator==(const TopLogprob&) const = default;
};

struct TokenLogprob {
  std::string token;
  double logprob = 0.0;
  std::vector<TopLogprob> top;

  bool operator==(const TokenLogprob&) const = default;
};

// One attempt of a request: the request itself, a retry, the fallback or a backup.
struct AttemptInfo {
  std::string provider;
  std::string model;
  ErrorClass error = ErrorClass::None;
  int http_status = 0;
  bool backup = false;                          // started beside a late reply (ARCH §11.2)
  bool stripped = false;                        // sent without the optional fields
  bool minimal_effort = false;                  // sent at the lowest reasoning effort
  bool lost_race = false;                       // cancelled because the other attempt streamed first: no failure
  std::chrono::milliseconds sent_at{0};         // from the request's start
  std::chrono::milliseconds first_token{-1};    // from its own send; -1 when it never streamed
  std::chrono::milliseconds ended_at{0};        // from the request's start
  std::string message;

  bool operator==(const AttemptInfo&) const = default;
};

struct ChatResult {
  ErrorClass error = ErrorClass::None;
  // What went wrong: the client's own words and, for an HTTP error, the server's message cut to 200 bytes with
  // anything shaped like a credential replaced (never for a rejected key). Never a key.
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
  bool streamed = false;                       // some text reached the sink (no retry follows that)
  std::chrono::milliseconds first_token{-1};   // from the request's start to the first visible text
  std::chrono::milliseconds total{0};
  std::vector<AttemptInfo> attempts;

  bool ok() const { return error == ErrorClass::None; }
  bool operator==(const ChatResult&) const = default;
};

// ---- Decision calls (ARCH §11.8): code builds the legal options, the model picks one, and the rules stand behind ----

enum class DecisionKind { Choice, YesNo };

struct DecisionOption {
  std::string key;   // short: A, B, … or a neutral label; "none" where picking nothing is legal
  std::string text;  // one line code wrote from records

  bool operator==(const DecisionOption&) const = default;
};

// A record the question needs, gathered by code by id; it enters the prompt as quoted data.
struct DecisionRecord {
  std::string name;
  std::string text;

  bool operator==(const DecisionRecord&) const = default;
};

struct DecisionCall {
  std::string task;                     // "next_speaker" in v1; the recall gate after v1
  DecisionKind kind = DecisionKind::Choice;
  std::optional<std::string> subject;   // the profile it is for, if one
  std::string question;                 // one plain question
  std::vector<DecisionRecord> state;
  std::vector<DecisionOption> options;  // Choice only: 2 to 12, unique keys
  // The rules fallback: the key the task's rules picked, a pure function of the same state that the caller works out
  // before the call goes. It must be one of the call's keys (an option's, or yes or no); en-llm checks that first.
  std::string rule;
  std::vector<Message> messages;        // rendered by en-prompt from prompts/decisions/; empty: the built-in layout
  std::string slot = std::string(kDecisions);

  bool operator==(const DecisionCall&) const = default;
};

enum class DecidedBy { Model, Rules };
enum class ReadAs { Schema, Meaning, YesNo, Rules };

struct KeyProbability {
  std::string key;
  double probability = 0.0;

  bool operator==(const KeyProbability&) const = default;
};

struct DecisionPick {
  std::string key;
  DecidedBy by = DecidedBy::Rules;
  ReadAs read = ReadAs::Rules;
  std::vector<KeyProbability> probabilities;  // from the top-5 logprobs where offered; recorded, never a cut-off
  bool fallback = false;                      // the rules decided although a model call was wanted
  bool miss = false;                          // the model answered, but no option or more than one
  ErrorClass error = ErrorClass::None;        // the call's failure, when that is why the rules decided
  std::chrono::milliseconds latency{0};
  std::string prompt_hash;                    // FNV-1a of the messages: the row keeps it, never the text
  std::string provider;
  std::string model;
  std::string task;
  std::optional<std::string> subject;
  std::vector<std::string> option_keys;

  // The pick's probability where logprobs gave one.
  std::optional<double> probability() const;
  // The call's one row for the playthrough's decisions table (MEM §8.7): task, subject, option keys, pick, its
  // probability, model or rules, latency and prompt hash; never the prompt's text or a name.
  nlohmann::json row() const;

  bool operator==(const DecisionPick&) const = default;
};

// ---- The handle's requests and events (C9, C10) ----

using RequestId = std::uint64_t;
using Epoch = std::uint64_t;

enum class RequestKind { Chat, Decision };

// One of the core's requests.
struct LlmRequest {
  // The caller's: non-zero, and unique among live requests (accepted, Ended event not yet queued). Reuse an id only
  // after reading its Ended event.
  RequestId id = 0;
  std::variant<ChatRequest, DecisionCall> body;
  // The player-input epoch own-initiative work was made at (DESIGN §8.9). Own-initiative dialogue must carry it, the
  // decision call that chooses a line may (and is then own-initiative work too), a reply never does.
  std::optional<Epoch> epoch;

  RequestKind kind() const;
  static LlmRequest chat(RequestId id, ChatRequest request, std::optional<Epoch> epoch = std::nullopt);
  static LlmRequest decision(RequestId id, DecisionCall call, std::optional<Epoch> epoch = std::nullopt);

  bool operator==(const LlmRequest&) const = default;
};

enum class EventKind {
  Started,      // the request's first attempt was sent: `provider` and `model`
  Delta,        // the next piece of a chat reply's text: `text`
  Reset,        // the text passed on so far was reasoning after all: withdraw it
  Ended,        // the request's one terminal event: `chat` for a chat request, `pick` for a decision call
  WarmUpEnded,  // a warm-up's one event: `provider`, `model` and `warm_up`
  ModelsEnded,  // a model list's one event: `provider` and `models` (the AI pages' self-test and model list)
};

struct WarmUpOutcome {
  bool ok = false;                     // the server answered 2xx
  int status = 0;                      // its HTTP status; 0 when none came
  std::chrono::milliseconds time{0};   // from the send to the end
  // None, or why not: Server (no connection, a 5xx), BadRequest (a 4xx, such as a model the server lacks),
  // TotalTimeout, GateClosed (it closed meanwhile), Cancelled (shutdown), NoHealthyProvider (no transport).
  ErrorClass error = ErrorClass::None;

  bool operator==(const WarmUpOutcome&) const = default;
};

// A provider's model list (GET <base_url>/models, ARCH §11.4 and §11.7's step 4a), asked on the handle's workers.
struct ModelsOutcome {
  // None, or why not, as en-llm's list_models names it: Auth (401 or 403, or no key), RateLimited, BadRequest, Server (no
  // connection, a 5xx, an answer that is no model list), TotalTimeout, GateClosed, Cancelled, NoHealthyProvider (no
  // transport, or no such provider).
  ErrorClass error = ErrorClass::None;
  int status = 0;                     // the HTTP status; 0 when none came
  std::vector<std::string> models;    // the ids, within en-llm's limits: never shown as they are (a local server's
                                      // ids can be file paths), only counted and compared with the slots' models
  int omitted = 0;                    // entries left out
  std::chrono::milliseconds time{0};  // from the send to the end

  bool operator==(const ModelsOutcome&) const = default;
};

struct LlmEvent {
  EventKind kind = EventKind::Delta;
  RequestId id = 0;                  // the request's; for WarmUpEnded and ModelsEnded their own, not a request id
  std::string provider;              // Started, WarmUpEnded, ModelsEnded
  std::string model;                 // Started, WarmUpEnded
  std::string text;                  // Delta
  std::optional<ChatResult> chat;    // Ended, for a chat request
  std::optional<DecisionPick> pick;  // Ended, for a decision call
  WarmUpOutcome warm_up;             // WarmUpEnded
  ModelsOutcome models;              // ModelsEnded

  bool terminal() const {
    return kind == EventKind::Ended || kind == EventKind::WarmUpEnded || kind == EventKind::ModelsEnded;
  }
  // The outcome's class: the chat result's, the pick's or the warm-up's; None for the other kinds.
  ErrorClass error() const;

  bool operator==(const LlmEvent&) const = default;
};

// ---- The model client's health, as the core's ladder reads it ----

// What the shell reads from en-llm's handle (its health report and configuration) and passes to the core when it
// changes, for ARCH §6.8's ladder: L3 and L4 need a healthy provider. One flag per slot the core uses; the slots of
// barks and memory join as their users do. It names no provider, model or key.
struct NetHealth {
  // The dialogue slot has a model it can use now: a configured provider whose keys resolved, neither cooling nor
  // disabled, and a pair that is not cooling.
  bool dialogue = false;

  bool operator==(const NetHealth&) const = default;
};

// ---- Names: the JSON forms' words for each enum ----

const char* to_string(Role role);                // system, user, assistant
const char* to_string(Purpose purpose);          // reply, own_initiative, decision, background
const char* to_string(ErrorClass e);             // as the enum: None, FirstTokenTimeout, …
const char* to_string(DecisionKind kind);        // choice, yes_no
const char* to_string(DecidedBy by);             // model, rules
const char* to_string(ReadAs read);              // schema, meaning, yes_no, rules
const char* to_string(RequestKind kind);         // chat, decision
const char* to_string(EventKind kind);           // as the enum: Started, Delta, …

// ---- JSON forms (json.cpp) ----
//
// Every field is written, under its own name in snake_case (a duration as "<name>_ms", in milliseconds), an absent
// optional as null. Reading is strict about what is there: an object with only known fields, each of its own type, and
// enums by the names above; a field left out keeps its default. LlmRequest holds exactly one of "chat" and
// "decision". Anything else throws ProtoError (json.hpp), naming the path and the problem. Numbers are finite: a
// NaN or an infinity has no JSON form.

void to_json(nlohmann::json& j, Role v);
void from_json(const nlohmann::json& j, Role& v);
void to_json(nlohmann::json& j, Purpose v);
void from_json(const nlohmann::json& j, Purpose& v);
void to_json(nlohmann::json& j, ErrorClass v);
void from_json(const nlohmann::json& j, ErrorClass& v);
void to_json(nlohmann::json& j, DecisionKind v);
void from_json(const nlohmann::json& j, DecisionKind& v);
void to_json(nlohmann::json& j, DecidedBy v);
void from_json(const nlohmann::json& j, DecidedBy& v);
void to_json(nlohmann::json& j, ReadAs v);
void from_json(const nlohmann::json& j, ReadAs& v);
void to_json(nlohmann::json& j, RequestKind v);
void from_json(const nlohmann::json& j, RequestKind& v);
void to_json(nlohmann::json& j, EventKind v);
void from_json(const nlohmann::json& j, EventKind& v);

void to_json(nlohmann::json& j, const Part& v);
void from_json(const nlohmann::json& j, Part& v);
void to_json(nlohmann::json& j, const Message& v);
void from_json(const nlohmann::json& j, Message& v);
void to_json(nlohmann::json& j, const ChatRequest& v);
void from_json(const nlohmann::json& j, ChatRequest& v);
void to_json(nlohmann::json& j, const Usage& v);
void from_json(const nlohmann::json& j, Usage& v);
void to_json(nlohmann::json& j, const TopLogprob& v);
void from_json(const nlohmann::json& j, TopLogprob& v);
void to_json(nlohmann::json& j, const TokenLogprob& v);
void from_json(const nlohmann::json& j, TokenLogprob& v);
void to_json(nlohmann::json& j, const AttemptInfo& v);
void from_json(const nlohmann::json& j, AttemptInfo& v);
void to_json(nlohmann::json& j, const ChatResult& v);
void from_json(const nlohmann::json& j, ChatResult& v);
void to_json(nlohmann::json& j, const DecisionOption& v);
void from_json(const nlohmann::json& j, DecisionOption& v);
void to_json(nlohmann::json& j, const DecisionRecord& v);
void from_json(const nlohmann::json& j, DecisionRecord& v);
void to_json(nlohmann::json& j, const DecisionCall& v);
void from_json(const nlohmann::json& j, DecisionCall& v);
void to_json(nlohmann::json& j, const KeyProbability& v);
void from_json(const nlohmann::json& j, KeyProbability& v);
void to_json(nlohmann::json& j, const DecisionPick& v);
void from_json(const nlohmann::json& j, DecisionPick& v);
void to_json(nlohmann::json& j, const LlmRequest& v);
void from_json(const nlohmann::json& j, LlmRequest& v);
void to_json(nlohmann::json& j, const WarmUpOutcome& v);
void from_json(const nlohmann::json& j, WarmUpOutcome& v);
void to_json(nlohmann::json& j, const ModelsOutcome& v);
void from_json(const nlohmann::json& j, ModelsOutcome& v);
void to_json(nlohmann::json& j, const LlmEvent& v);
void from_json(const nlohmann::json& j, LlmEvent& v);
void to_json(nlohmann::json& j, const NetHealth& v);
void from_json(const nlohmann::json& j, NetHealth& v);

}  // namespace en::proto
