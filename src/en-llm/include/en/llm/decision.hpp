#pragma once
// Decision calls (ARCH §11.8): code builds the legal options, the model picks one, and a rules fallback always
// stands behind it. Our own format over chat completions, on the `decisions` slot.
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/llm/client.hpp"
#include "en/llm/clock.hpp"
#include "en/llm/errors.hpp"
#include "en/llm/message.hpp"
#include "en/llm/stream.hpp"

namespace en::llm {

enum class DecisionKind { Choice, YesNo };

inline constexpr std::size_t kMaxOptions = 12;   // EST, as MEM §4.5's gate keeps
inline constexpr std::size_t kMaxKeyLength = 16;

struct DecisionOption {
  std::string key;   // short: A, B, … or a neutral label; "none" where picking nothing is legal
  std::string text;  // one line code wrote from records
};

// A record the question needs, gathered by code by id; it enters the prompt as quoted data.
struct DecisionRecord {
  std::string name;
  std::string text;
};

struct DecisionCall {
  std::string task;                     // "next_speaker" in v1; the recall gate after v1
  DecisionKind kind = DecisionKind::Choice;
  std::optional<std::string> subject;   // the profile it is for, if one
  std::string question;                 // one plain question
  std::vector<DecisionRecord> state;
  std::vector<DecisionOption> options;  // Choice only: 2 to 12, unique keys
  std::function<std::string()> rule;    // the rules fallback: a pure function of the same state, returning a legal key
  std::vector<Message> messages;        // rendered by en-prompt from prompts/decisions/; empty: the built-in layout
  std::string slot = std::string(kDecisions);
};

enum class DecidedBy { Model, Rules };
enum class ReadAs { Schema, Meaning, YesNo, Rules };

const char* to_string(DecidedBy by);
const char* to_string(ReadAs read);

struct KeyProbability {
  std::string key;
  double probability = 0.0;
};

struct DecisionPick {
  std::string key;
  DecidedBy by = DecidedBy::Rules;
  ReadAs read = ReadAs::Rules;
  std::vector<KeyProbability> probabilities;  // from the top-5 logprobs where offered; recorded, never a cut-off
  bool fallback = false;                      // the rules decided although a model call was wanted
  bool miss = false;                          // the model answered, but no option or more than one
  ErrorClass error = ErrorClass::None;        // the call's failure, when that is why the rules decided
  Millis latency{0};
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
};

// Throws std::invalid_argument naming what is wrong: no task, no rule, a question missing, 2 to 12 options with
// unique short keys of letters, digits, '_' or '-' for a choice, none for a yes-or-no question.
void validate(const DecisionCall& call);

// The keys a call may answer: its options' keys, or "yes" and "no".
std::vector<std::string> legal_keys(const DecisionCall& call);

// The built-in layout (our own words): a system line, then the records as quoted data, the question and the options.
std::vector<Message> build_decision_messages(const DecisionCall& call);

// {"answer": <one of the keys>} as a JSON schema, for providers that take one.
nlohmann::json decision_schema(const DecisionCall& call);

// A JSON answer held to the schema: {"answer": "B"}, also inside a code fence. Empty when it is not one.
std::optional<std::string> read_schema_answer(std::string_view answer, const DecisionCall& call);

struct AnswerRead {
  std::optional<std::string> key;
  bool ambiguous = false;  // it named more than one option
};

// A plain answer read by meaning: the key, the option's own words, or a plain paraphrase ("no one", "silence" for
// none), whatever the case, quotes, markup or words around it. More than one option, or none, is a miss.
AnswerRead read_choice(std::string_view answer, const std::vector<DecisionOption>& options);
AnswerRead read_yes_no(std::string_view answer);
AnswerRead read_by_meaning(std::string_view answer, const DecisionCall& call);

// The probabilities of the keys at the answer's token, from its top logprobs; empty when the token cannot be found
// or does not name the pick.
std::vector<KeyProbability> key_probabilities(const std::vector<TokenLogprob>& logprobs, const DecisionCall& call,
                                              const std::string& picked);

std::string hash_messages(const std::vector<Message>& messages);

}  // namespace en::llm
