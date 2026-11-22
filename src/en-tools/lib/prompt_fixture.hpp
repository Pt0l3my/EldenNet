#pragma once
// The dialogue prompt outside the game (part A of the first conversation, WHY§364): the content pack's prompts and
// lexicon read from disk, as the content loader hands them to en-core, a prompt fixture (tests/fixtures/prompt/), and
// the prompt goldens' text, which render-prompt --fixture and golden share.
//
// A fixture (format 1) is JSON: {"fixture": 1, "persona": id, "name": the name as the game would show it, "record": the
// persona's record as the content pack holds it with its stage resolved (NPC §3.1, under "stage"), and, each optional,
// "earlier" and "history" ([{"who": "tarnished" | "persona", "text"}]), "scene" (Scene's fields), "recap" ([text]),
// "reminder" ("banned" | "promise" | "safety"), "tally" ({"tics": [n, n, n]}), "tied" and "land" ({"known", "heard",
// "believed"}: [text])}. Every name and line in one is invented.
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/core/dialogue_prompt.hpp"
#include "en/core/speech_lint.hpp"
#include "en/proto/expected.hpp"

namespace en::tools {

// Every file under <pack>/prompts/ with a template's extension, by name from the pack's root ("prompts/dialogue/…"),
// into `out`; false, with `problem` set, when the pack cannot be read. (Not an Expected: Microsoft's std::map may
// throw when it moves, which Expected refuses.)
[[nodiscard]] bool read_pack_prompts(const std::filesystem::path& pack, std::map<std::string, std::string>& out,
                                     std::string& problem);
// <pack>/lexicon/banned.toml and safety.toml, read by en-core's parse_lexicon.
[[nodiscard]] proto::Expected<core::Lexicon, std::string> read_pack_lexicon(const std::filesystem::path& pack);

struct PromptFixture {
  std::string persona;
  std::string name;
  nlohmann::json record;
  std::vector<core::Turn> earlier;
  std::vector<core::Turn> history;
  core::Scene scene;
  std::vector<std::string> recap;
  std::optional<core::HardList> reminder;
  core::LintTally tally;
  core::Standing tied;
  core::Standing land;

  // Points into this fixture: keep it alive while the context is used.
  [[nodiscard]] core::DialogueContext context() const;
};

[[nodiscard]] proto::Expected<PromptFixture, std::string> parse_prompt_fixture(const std::string& text);

// The golden form of a render: header lines ("# …": the tokens against the budget, the breakpoints, each block's
// estimate against its cap, '!' past it, and what the trim took), then the messages in marker form; or the refusal.
[[nodiscard]] std::string prompt_golden_text(const std::string& name,
                                             const proto::Expected<core::PromptReport, core::PromptError>& result);

}  // namespace en::tools
