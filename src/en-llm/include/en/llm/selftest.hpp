#pragma once
// The self-test and the model list of EldenNet's AI pages (ARCH §11.7's steps 4a, 4b, 4d and 4e; PAGES §4.6.2), run on
// the handle's workers and driven from the caller's thread without waiting: the caller calls step() after it has
// made the run and after each batch of the handle's events, and hands each event to take().
//
// - 4a, the reachability: each provider a slot in use names (its model or its fallback) lists its models (Service::
//   list_models); it passes when it answers and lists every model of those slots that it serves.
// - 4b, the latency: the selftest slot streams a short reply (32 tokens at most, its budget): the first token's time
//   and the tokens per second.
// - 4d, the JSON check: the memory slot answers a two-field object against a schema, and it parses.
// - 4e, the verdict: the first page's estimated time, the first token's plus 15 tokens at the measured rate: quick
//   (1.6 s or less), acceptable (3.0 s or less) or slow (C5's cloud targets).
// ARCH §11.7's 4c, the voice check through the dialogue pipeline, waits for the personas and the prompts in game.
//
// The report holds numbers, error classes and ids from the configuration only: never a key, an address, the model's
// text or an id a server listed. The two requests are our own fixed words; their answers are read here and dropped.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "en/llm/clock.hpp"
#include "en/llm/config.hpp"
#include "en/llm/service.hpp"

namespace en::llm {

enum class CheckState : std::uint8_t { Waiting, Running, Passed, Failed, Skipped };
enum class Verdict : std::uint8_t { None, Quick, Acceptable, Slow };

[[nodiscard]] const char* to_string(CheckState s) noexcept;
[[nodiscard]] const char* to_string(Verdict v) noexcept;

inline constexpr Millis kQuickPage{1600};
inline constexpr Millis kAcceptablePage{3000};
inline constexpr int kPageTokens = 15;  // ARCH §11.7's first page

[[nodiscard]] Verdict verdict_of(Millis first_page) noexcept;

struct ListCheck {
  std::string provider;
  CheckState state = CheckState::Waiting;
  ErrorClass error = ErrorClass::None;
  int status = 0;
  int listed = 0;
  std::vector<std::string> missing;  // the slots (their names) whose model on this provider it does not list
};

struct SelfTestReport {
  bool lists_only = false;  // the model list alone (Providers' "List models")
  std::vector<ListCheck> lists;
  // 4b
  CheckState latency = CheckState::Waiting;
  ErrorClass latency_error = ErrorClass::None;
  std::string provider;  // the selftest slot's model and its provider, as the configuration names them
  std::string model;
  Millis first_token{-1};
  int tokens = 0;                 // as the provider reported them, else estimated from the text (four bytes a token)
  double tokens_per_second = 0;
  // 4d
  CheckState json = CheckState::Waiting;
  ErrorClass json_error = ErrorClass::None;
  // 4e
  Verdict verdict = Verdict::None;
  Millis first_page{0};

  [[nodiscard]] bool done() const noexcept;
  [[nodiscard]] bool passed() const noexcept;  // every step it ran passed
};

class SelfTest {
 public:
  // The plan, from the configuration the handle runs. Its two requests take the ids `first_id` and `first_id + 1`:
  // the caller keeps them apart from its own.
  SelfTest(const Config& config, bool lists_only, RequestId first_id);

  // Starts what comes next on the handle; never blocks.
  void step(Service& service);
  // One of the handle's events: true when it was this run's, and then taken.
  bool take(const LlmEvent& event);
  // Stops what runs; the report's unfinished steps read Skipped.
  void cancel(Service& service);

  [[nodiscard]] const SelfTestReport& report() const noexcept { return report_; }
  [[nodiscard]] bool done() const noexcept { return report_.done(); }

  // The requests, for the tests: our own words, no player text.
  [[nodiscard]] static ChatRequest latency_request();
  [[nodiscard]] static ChatRequest json_request();
  // Whether a JSON check's answer is the object it asked for.
  [[nodiscard]] static bool json_answer_ok(std::string_view text);

 private:
  struct Want {
    std::string slot;
    std::string model;
  };
  void finish_latency(const ChatResult& r);
  void finish_json(const ChatResult& r);
  void skip_rest();

  std::vector<std::vector<Want>> wants_;  // per list check: the slots' models that provider must list
  std::vector<std::optional<RequestId>> list_ids_;
  std::size_t next_list_ = 0;
  RequestId first_id_ = 0;
  bool latency_sent_ = false;
  bool json_sent_ = false;
  SelfTestReport report_;
};

}  // namespace en::llm
