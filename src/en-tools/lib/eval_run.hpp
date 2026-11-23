#pragma once
// The eval's runner (DESIGN §14, NPC §12.1, ARCH §12.2, §12.3; WHY§364 (10)): a scripted conversation through the
// real core, its speech and prompt included, against a model that is scripted (`mock`, mockllm.hpp) or recorded
// (`recorded`), on a simulated clock, with a stand-in for the surfaces that acknowledges each page on its first frame.
// Live models and the judged sets wait for a key and the judge's check (DESIGN §14).
//
// A scenario (tests/scenarios/talk/*.toml, ours):
//
//   schema = 1
//   name = "lantern-first-words"
//   persona = "lantern_keeper"               # whom it talks to
//   overrides = "overrides"                  # an overrides folder, from this file's folder (binds the persona)
//   mock = "lantern-first-words.mock.toml"   # the script for --llm mock, from this file's folder
//   recorded = "lantern-first-words.llm.jsonl"  # the model's events for --llm recorded
//   [world]                                  # one invented character, the persona's, near the Tarnished
//   param = 81100
//   distance_m = 2.5
//   bearing_deg = 5.0
//   clock = [18, 30]
//   subtitles = true
//   flags = "fresh"                          # optional: a fixture flag state (tests/fixtures/flagstates/<name>.toml,
//                                            # found from the scenario's folder up), read through the core's watch list
//   [[step]]                                 # in order, from when the world is ready and the observer allows chat
//   do = "open"                              # open | say | wait | esc | skip | move | flags | expect
//   [[step]]
//   do = "say"
//   text = "Where does the road go?"
//   [[step]]
//   do = "wait"
//   ms = 8000
//   [[step]]
//   do = "expect"                            # each field optional: state, box, npc_lines, player_lines, failures,
//   state = "open"                           # deflections, ended, last_npc, toast, prompt_has, prompt_lacks
//   npc_lines = 1
//   prompt_has = ["As things stand now:"]    # texts the last dialogue request's messages hold, or lack
//
// A `flags` step (`flags = "after_radahn"`) changes the world's flag state from the next snapshot on: the scenario's
// overrides give a catalogue whose aliases a `ci:` record verifies (the eval's realm alone), so the core asks for them
// (SetWatchFlags) and the runner answers each fourth snapshot with a FlagFrame of the state's aliases, as the game task
// would. A failed `prompt_has` names its line and the entry's index, never the text.
//
// The report holds the conversation's text, the model's included: it is a local-only kind, written only where the
// caller says (`--report`), never into the repository (eval-reports/ is ignored by git), and never uploaded (PV rules).
// What the command prints is numbers only.
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/core/content.hpp"
#include "en/core/gate.hpp"
#include "en/proto/expected.hpp"
#include "mockllm.hpp"

namespace en::tools {

struct EvalStep {
  enum class Kind : std::uint8_t { Open, Say, Wait, Esc, Skip, Move, Expect, Flags };
  Kind kind = Kind::Wait;
  std::size_t line = 0;
  std::string text;                 // Say
  core::Mono ms = 0;                // Wait
  float distance_m = 0;             // Move
  // Expect: each set field is checked.
  std::optional<std::string> state, box, ended, last_npc, toast;
  std::optional<std::size_t> npc_lines, player_lines, failures, deflections;
  std::optional<std::size_t> regenerations, cancels;  // the run's so far: lines sent twice, requests cancelled
  std::vector<std::string> prompt_has, prompt_lacks;  // Expect: in the last dialogue request's messages, or not
  std::string flags;                                  // Flags: a fixture flag state's name
};

struct EvalScenario {
  std::string name;
  std::string persona;
  std::filesystem::path folder;  // the scenario's own
  std::optional<std::string> overrides, mock, recorded;
  std::uint32_t param = 0;
  float distance_m = 2.5f;
  float bearing_deg = 0.0f;
  std::uint8_t clock_h = 12, clock_m = 0;
  bool subtitles = true;
  std::optional<std::string> flags;  // the fixture flag state the world starts in
  std::vector<EvalStep> steps;
};

// The scenario, or what is wrong with it ("line 12: step[3].do: ..."), never quoting a value.
[[nodiscard]] proto::Expected<EvalScenario, std::string> parse_eval_scenario(std::string_view text,
                                                                             const std::filesystem::path& folder);

// A folder read as content files: every .toml, .md and .j2 below it (records, lexicons and the prompts' templates), by
// its path from the folder ('/'-separated), as `origin`.
// Files past 64 KB and paths that leave the folder are refused.
[[nodiscard]] proto::Expected<std::vector<core::ContentFile>, std::string> read_content_folder(
    const std::filesystem::path& root, core::ContentOrigin origin);

struct EvalTurn {
  std::string player;        // the Tarnished's line, as committed
  std::string reply;         // what showed, as committed
  std::uint8_t pages = 0;
  bool cut_off = false;
  bool failure = false;      // the "…" page
  bool deflection = false;   // the reply is one of the persona's deflections
  std::uint32_t words = 0;
  std::uint32_t requests = 0;  // this line's requests: 1, or 2 with a regeneration
  std::optional<core::Mono> first_page_ms;  // from the line to its first page's first frame
};

struct EvalRun {
  std::string scenario;
  std::string persona;
  std::string llm;  // mock | recorded
  std::uint16_t words_max = 0;
  std::vector<EvalTurn> turns;
  std::vector<std::string> toasts;  // every toast that went up
  std::vector<std::string> endings;
  std::size_t requests = 0, cancels = 0;
  std::vector<std::string> failed;  // each expectation that failed: "line 30: npc_lines is 0, expected 1"
};

enum class EvalLlm : std::uint8_t { Mock, Recorded };

// Runs the scenario through a fresh core loaded with the built-in content and the scenario's overrides, in the eval's
// realm. An error is a problem with the inputs (a file that cannot be read or parsed, content that does not load).
[[nodiscard]] proto::Expected<EvalRun, std::string> run_eval(const EvalScenario& scenario, EvalLlm llm);

// The report (schema 1, kind "eval-local"): the turns with their text, the metrics and the expectations.
[[nodiscard]] nlohmann::json eval_report(const EvalRun& run);
// The numbers alone, for standard output: no text of the conversation.
[[nodiscard]] std::string eval_summary(const EvalRun& run);

}  // namespace en::tools
