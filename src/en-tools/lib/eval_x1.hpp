#pragma once
// X1, "voice in a bottle", outside the game (NPC §12.2, §12.1; DESIGN §16.1; the owner's O139 to O142): the set's turns
// exported as jobpacks for LLM Jobrunner, which runs them on the owner's own model servers, and Jobrunner's results
// read back through the speech and measured per server and model.
//
// Jobrunner cannot chain turns, so every turn is an item of its own. Its messages are the prompt the core renders for
// that line when the conversation before it went as we wrote it: each scenario of the set runs through the real core
// with its scripted model (mock), whose replies are our own lines, and each line's first request is captured, as
// en-llm would send it to a server that takes no cache marks (render_messages, CacheMarks::None), with the dialogue
// slot's sampling (max_tokens and temperature from its budget, the dialogue's stop sequences, thinking off). A scripted
// run that regenerates, deflects, fails, is cut off or misses an expectation is refused: its history would not be ours.
//
// The set (tests/x1/x1.toml, ours):
//
//   schema = 1
//   name = "x1"
//   [arm.former]                        # an arm: an overrides folder from this file's folder, after the scenario's
//   overrides = "arms/former"           # ("positive", with no folder, is the content as it ships)
//   [[run]]
//   pack = "x1-kale"                    # the jobpack it goes in: a slug of at most 40 characters
//   code = "voice"                      # in each item's id: 1 to 8 of a-z and 0-9
//   kind = "voice"                      # voice | test_question
//   scenario = "../scenarios/talk/x1-voice-kale.toml"   # from this file's folder
//   arms = ["positive", "former"]
//   repeat = 1                          # Jobrunner's runs of each item on each server, 1 to 10
//
// An item's id is `<arm>-<persona>-<code>-t<NN>`, with `-x<repeat>` when it repeats; Jobrunner's result lines name it
// and their run. The import reads results.jsonl as untrusted data: each line strictly (its keys, types and sizes), at
// most 64 MB in all, matched to its item by id, its messages (when Jobrunner kept them) the item's to the byte; a line
// that fails is refused by its number and reason, never quoting it. A server's name outside Jobrunner's own form and a
// model's name are written as given only within bounds (a model's file name alone, never a folder), else as
// `server-N` or `model-N`.
//
// What the import writes:
//   - the local report (`x1-local`, schema 1): every result with the reply, what would show, its speech counts and
//     timings: model text, so written only where --report says, never committed or uploaded (PV rules);
//   - the summary (`x1-summary`, schema 1): numbers per server and model, persona, arm and kind, C10 on the
//     non-archaic personas, the arms side by side, the "Name:" prefixes by class; no line of text;
//   - the test questions (`x1-questions`, schema 1): each test question's reply with the prompt it answered, for the
//     Engine session's scoring (O142): local-only, as the report.
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/proto/expected.hpp"
#include "speech_tap.hpp"

namespace en::tools {

// Jobrunner 1.4's jobpack/1 caps (its README, "Jobpack format 1"; llm_jobrunner.py's defaults and checks).
struct JobpackCaps {
  static constexpr std::size_t kFileBytes = std::size_t{2} << 20;
  static constexpr std::size_t kItems = 2000;
  static constexpr std::size_t kRuns = 5000;          // items × repeats × servers; checked for kServers servers up
  static constexpr std::size_t kServers = 8;
  static constexpr std::size_t kIdChars = 64;
  static constexpr std::size_t kMessages = 64;
  static constexpr std::size_t kDescriptionChars = 600;
  static constexpr std::size_t kNameChars = 100;
  static constexpr int kMaxTokens = 2048;
  static constexpr std::size_t kStops = 4;
  static constexpr std::size_t kStopChars = 64;
};

struct X1Arm {
  std::string name;
  std::optional<std::filesystem::path> overrides;  // none: the content as it ships
};

struct X1Run {
  std::string pack, code, kind;  // kind: "voice" or "test_question"
  std::filesystem::path scenario;
  std::vector<std::string> arms;
  int repeat = 1;
  std::size_t line = 0;
};

struct X1Set {
  std::string name;
  std::filesystem::path folder;
  std::vector<X1Arm> arms;
  std::vector<X1Run> runs;
};

// The set, or what is wrong with it ("line 9: run[2].arms: ..."), never quoting a value.
[[nodiscard]] proto::Expected<X1Set, std::string> parse_x1_set(std::string_view text,
                                                               const std::filesystem::path& folder);

// One turn of the set in one arm: a jobpack item, and what the import needs to read its replies.
struct X1Item {
  std::string id, pack, kind, arm, persona, register_name, scenario, code, state;
  std::size_t turn = 0;  // from 1, in its scenario
  int repeat = 1;
  std::string player;            // the Tarnished's line, ours
  nlohmann::json messages;       // as a server is sent them
  ReplyContext context;          // the line's first try's, from the scripted run
  std::vector<std::string> topics;  // the persona's `forbid` topics (the test questions')
};

struct X1Sampling {
  int max_tokens = 0;
  double temperature = 0;
  std::vector<std::string> stop;
};
// The dialogue slot's, as the game sends a reply's request (en-llm's default_budget, en-core's kDialogueStops).
[[nodiscard]] X1Sampling x1_sampling();

// Every item of the set, in its runs' order, each arm's after the other for each scenario; or the first problem.
[[nodiscard]] proto::Expected<std::vector<X1Item>, std::string> build_x1_items(const X1Set& set);

// The jobpacks, one per `pack`, in the order the set first names them: {name, JSON}. Each is checked against
// JobpackCaps; one past a cap is an error naming the pack and the cap.
struct X1Jobpack {
  std::string name;
  nlohmann::json json;
  std::size_t items = 0, runs = 0;  // runs per server
};
[[nodiscard]] proto::Expected<std::vector<X1Jobpack>, std::string> x1_jobpacks(const X1Set& set,
                                                                              const std::vector<X1Item>& items);
// The bytes Jobrunner reads: the JSON in UTF-8, with a final newline.
[[nodiscard]] std::string jobpack_text(const X1Jobpack& pack);

// ---- The import ----

struct X1Line {
  std::size_t line = 0;  // in its file, from 1
  std::string id;
  int run = 1;
  bool ok = false;
  std::string server, model;  // as bounded (above)
  std::string reply;
  std::optional<std::string> finish_reason;
  std::optional<double> first_token_s, total_s, tokens_per_s;
  std::optional<std::int64_t> completion_tokens;
  bool had_messages = false;
};

struct X1Refusal {
  std::size_t file = 0;  // which results file, from 1
  std::size_t line = 0;
  std::string why;       // fixed words, never a value
};

struct X1Import {
  std::vector<X1Line> lines;
  std::vector<X1Refusal> refused;
  // Each name as a result line gave it, with its label: held in memory to keep labels the same across files, and
  // never written.
  std::vector<std::pair<std::string, std::string>> server_labels, model_labels;
};

inline constexpr std::size_t kResultsFileMax = std::size_t{64} << 20;
inline constexpr std::size_t kResultLineMax = std::size_t{1} << 20;
inline constexpr std::size_t kReplyMax = 64 * 1024;

// Reads one results.jsonl's text against the items (file: its number among the files, for the refusals), appending to
// `into`. Labels for servers and models are shared across files through `into`.
void read_jobrunner_results(std::string_view text, std::size_t file, const std::vector<X1Item>& items,
                            X1Import& into);

// The bounded forms: a server's name as Jobrunner names it (A-Z a-z 0-9 _ -, 1 to 32), else none; a model's file name
// (the part after the last '/' or '\', printable, at most 128 characters, no address), else none.
[[nodiscard]] std::optional<std::string> bounded_server(std::string_view name);
[[nodiscard]] std::optional<std::string> bounded_model(std::string_view name);

// One result read through the speech, with its item's context.
struct X1Measured {
  const X1Line* line = nullptr;
  const X1Item* item = nullptr;
  SpeechPass pass;
  std::uint32_t shown_contract_breaks = 0;  // markup, a list, a line break or a "Name:" left in what would show
};

struct X1Outputs {
  nlohmann::json report;     // x1-local
  nlohmann::json summary;    // x1-summary
  nlohmann::json questions;  // x1-questions
  std::string printed;       // for standard output: numbers and labels only
};

[[nodiscard]] X1Outputs x1_measure(const X1Set& set, const std::vector<X1Item>& items, const X1Import& import,
                                   const SpeechTap& tap);

}  // namespace en::tools
