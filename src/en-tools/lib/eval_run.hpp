#pragma once
// The eval's runner (DESIGN §14, NPC §12.1, ARCH §12.2, §12.3; WHY§364 (10)): a scripted conversation through the
// real core, its speech and prompt included, against a model that is scripted (`mock`, mockllm.hpp) or recorded
// (`recorded`), on a simulated clock, with a stand-in for the surfaces that acknowledges each page on its first frame.
// Live models and the judged sets wait for a key and the judge's check (DESIGN §14). The speech runs through the
// eval's tap (speech_tap.hpp), which counts what each reply attempt did: the sanitizer's removals, the "Name:" prefix
// by class (WHY§410), the lint's hits and the pager's takes, each attempt with its context.
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
//   do = "open"                              # open | say | wait | esc | skip | move | flags | expect | restart
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
//   name_prefixes = 0                        # also: the reply attempts so far whose "Name:" step 5 took
//
// Memory (v0.3; MEM §2.8, §9): `store = true` runs the core against en-store's StoreCore on a temporary folder of the
// run's own (removed after; the oracle's, EvalOptions::store_dir, is kept), the account attached, each step's store
// requests answered in order as T9 would. Then:
//
//   character = "Wren"                       # the Tarnished's invented name (default "Vessel")
//   [[world.extra]]                          # more invented characters near the Tarnished, after the partner
//   param = 81100
//   distance_m = 12.0
//   [[step]]
//   do = "native_talk"                       # the game's own talk with the partner for `ms` (default 3000)
//   do = "strike"                            # the Tarnished strikes the partner once
//   do = "ng_plus"                           # the journey's next cycle (ng_lvl + 1)
//   do = "reload"                            # back to the title, then the same save again: a new session's core
//   do = "kill"                              # the process dies: the core and the store's thread are dropped unflushed
//   do = "load"                              # an older save of the same character, at `pt` ms of play
//   do = "choose"                            # the answer to the rollback choice: forget | keep | new
//   do = "wait_jobs"                         # until no memory job is queued or in flight (two minutes at most)
//   do = "move"                              # `who` = 0 the partner (default), n the n-th extra
//   do = "expect"                            # and: mode (the store's: off, opening, choice, open, down),
//   fact = { persona = "kale", key = "met_count", value = "1" }      # "<none>" when unset
//   audience = { kind = "PlayerSaid", persona = "kale", how = "direct", count = 3 }
//
// A `flags` step (`flags = "after_radahn"`) changes the world's flag state from the next snapshot on: the scenario's
// overrides give a catalogue whose aliases a `ci:` record verifies (the eval's realm alone), so the core asks for them
// (SetWatchFlags) and the runner answers each fourth snapshot with a FlagFrame of the state's aliases, as the game task
// would. A failed `prompt_has` names its line and the entry's index, never the text. A `restart` step starts a fresh
// core with the same content and flag state, so that what follows is a first meeting with no earlier lines (the test
// questions, NPC §12.1).
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
#include "en/proto/llm.hpp"
#include "mockllm.hpp"
#include "speech_tap.hpp"

namespace en::tools {

struct EvalStep {
  enum class Kind : std::uint8_t {
    Open,
    Say,
    Wait,
    Esc,
    Skip,
    Move,
    Expect,
    Flags,
    Restart,
    NativeTalk,
    Strike,
    NgPlus,
    Reload,
    Kill,
    Load,
    Choose,
    WaitJobs,
  };
  Kind kind = Kind::Wait;
  std::size_t line = 0;
  std::string text;                 // Say
  core::Mono ms = 0;                // Wait
  float distance_m = 0;             // Move
  // Expect: each set field is checked.
  std::optional<std::string> state, box, ended, last_npc, toast;
  std::optional<std::size_t> npc_lines, player_lines, failures, deflections;
  std::optional<std::size_t> regenerations, cancels;  // the run's so far: lines sent twice, requests cancelled
  std::optional<std::size_t> name_prefixes;           // the run's so far: attempts whose "Name:" step 5 took
  std::vector<std::string> prompt_has, prompt_lacks;  // Expect: in the last dialogue request's messages, or not
  std::string flags;                                  // Flags: a fixture flag state's name
  std::size_t who = 0;                                // Move: 0 the partner, n the n-th extra
  std::int64_t pt = 0;                                // Load: the older save's play time, ms
  std::string choice;                                 // Choose: forget | keep | new
  std::optional<std::string> mode;                    // Expect: the store's mode
  struct Fact {
    std::string persona, key, value;
  };
  struct Audience {
    std::string kind, persona, how;
    std::size_t count = 0;
  };
  std::optional<Fact> fact;          // Expect: the journey's fact, as the core holds it
  std::optional<Audience> audience;  // Expect: the events of a kind the persona perceived so, in the file
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
  bool store = false;                 // the core against en-store on a temporary folder
  std::string character = "Vessel";   // the Tarnished's name, invented
  struct Extra {
    std::uint32_t param = 0;
    float distance_m = 0, bearing_deg = 0;
  };
  std::vector<Extra> extras;
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
  std::vector<std::size_t> attempts;        // its reply attempts, by index into EvalRun::attempts, in order
  std::vector<proto::Message> prompt;       // its first request's messages, as the prompt rendered them
  std::string flags;                        // the fixture flag state the world was in, or "" for none
  // Its reply's first try (the last attempt opened before any regeneration, after a Reset), and its regeneration.
  [[nodiscard]] std::optional<std::size_t> first_try(const std::vector<TapAttempt>& all) const;
  [[nodiscard]] std::optional<std::size_t> regeneration(const std::vector<TapAttempt>& all) const;
};

struct EvalRun {
  std::string scenario;
  std::string persona;
  std::string llm;  // mock | recorded
  std::uint16_t words_max = 0;
  std::string register_name;  // the persona's register: "archaic-regal", "formal", ...
  std::vector<std::string> topics;  // its stages' `forbid` topics, eval-only (the test questions', NPC §12.1)
  std::vector<EvalTurn> turns;
  std::vector<TapAttempt> attempts;  // every reply attempt the conversation opened, with its context and counts
  std::vector<std::string> toasts;  // every toast that went up
  std::vector<std::string> endings;
  std::size_t requests = 0, cancels = 0;
  std::size_t background = 0;  // the memory job's requests (v0.3)
  std::vector<std::string> failed;  // each expectation that failed: "line 30: npc_lines is 0, expected 1"
  std::vector<std::int64_t> step_pt;  // each step's play time as it began, ms (the oracle's cuts, MEM §9.6)
};

enum class EvalLlm : std::uint8_t { Mock, Recorded };

struct EvalOptions {
  // An arm (NPC §12.2): an overrides folder loaded after the scenario's, whose files replace any of the same path
  // (the positive-wording arm's former prompts, tests/x1/arms/).
  std::optional<std::filesystem::path> arm;
  // The oracle's (MEM §9.6): the store's folder, kept after the run (a temporary one, removed, when none); and the
  // save's play time when the world first loads (600,000 ms when none).
  std::optional<std::filesystem::path> store_dir;
  std::optional<std::int64_t> start_pt;
};

// Runs the scenario through a fresh core loaded with the built-in content, the scenario's overrides and the arm's, in
// the eval's realm. An error is a problem with the inputs (a file that cannot be read or parsed, content that does not
// load).
[[nodiscard]] proto::Expected<EvalRun, std::string> run_eval(const EvalScenario& scenario, EvalLlm llm,
                                                             const EvalOptions& options = {});

// The speech counts as the report writes them: each kind that is not 0, by its name.
[[nodiscard]] nlohmann::json counts_json(const SpeechCounts& c);

// The report (schema 2, kind "eval-local"): the turns with their text and their first try's speech counts, the metrics
// and the expectations.
[[nodiscard]] nlohmann::json eval_report(const EvalRun& run);
// The numbers alone, for standard output: no text of the conversation.
[[nodiscard]] std::string eval_summary(const EvalRun& run);

}  // namespace en::tools
