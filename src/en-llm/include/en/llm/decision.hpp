#pragma once
// Decision calls (ARCH §11.8): code builds the legal options, the model picks one, and a rules fallback always
// stands behind it. Our own format over chat completions, on the `decisions` slot.
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
#include "en/proto/llm.hpp"

namespace en::llm {

// The call and its pick are en-proto's (llm.hpp), shared with the core. A call's rules fallback is the key the
// caller's rules picked from the same state (`rule`), which is checked here like everything else in the call.
using en::proto::DecidedBy;
using en::proto::DecisionCall;
using en::proto::DecisionKind;
using en::proto::DecisionOption;
using en::proto::DecisionPick;
using en::proto::DecisionRecord;
using en::proto::KeyProbability;
using en::proto::ReadAs;
using en::proto::to_string;

inline constexpr std::size_t kMaxOptions = 12;   // EST, as MEM §4.5's gate keeps
inline constexpr std::size_t kMaxKeyLength = 16;

// Throws std::invalid_argument naming what is wrong: no task, no rule, a question missing, 2 to 12 options with
// unique short keys of letters, digits, '_' or '-' for a choice, none for a yes-or-no question, or a rule that is
// not one of the call's keys.
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
