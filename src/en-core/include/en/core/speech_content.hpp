#pragma once
// The content pack's speech (the first conversation, parts A and B joined; WHY§364, WHY§388): the dialogue prompt's
// templates (`prompts/`) and the lint's lexicon (`lexicon/banned.toml` and `lexicon/safety.toml`), read from the pack's
// other files (ContentPack::others, the overrides applied) into part A's DialoguePrompt and SpeechLint, and the content
// lint of block 2. Pure (ARCH §2.3): the shell read the files; the core's load_content calls this after the pack's own
// checks, and a pack whose prompts or lexicon fail is refused as any content error refuses it (the old content stays).
//
// Block 2 (identity: `stage.identity`, `public.role`, `.summary` and `.appearance`) is never trimmed and stays at its
// 120 tokens though its fields at their schema caps come to about 300 (WHY§388): each enabled persona's block 2 is
// rendered alone at each of its stages, under its fallback name, and one past its cap is an error for shipped content
// and a test persona (`test_only`), and a warning for the player's own files (an override's record), which still
// render, the hard cap guarding the whole prompt. The words are fixed and name the file, the stage's line and the
// estimate, never a value of the file.
#include <memory>
#include <string_view>
#include <vector>

#include "en/core/content.hpp"
#include "en/core/dialogue_prompt.hpp"
#include "en/core/speech_lint.hpp"
#include "en/proto/expected.hpp"

namespace en::core {

inline constexpr std::string_view kBannedPath = "lexicon/banned.toml";
inline constexpr std::string_view kSafetyPath = "lexicon/safety.toml";

struct SpeechContent {
  std::shared_ptr<const DialoguePrompt> prompt;
  std::shared_ptr<const SpeechLint> lint;
  std::vector<ContentError> warnings;  // the player's own records whose block 2 passes its cap: they still render
};

[[nodiscard]] proto::Expected<SpeechContent, std::vector<ContentError>> load_speech(const ContentPack& pack);

// The content lint of block 2 alone, as load_speech runs it (for the tools and the tests): each persona's stage whose
// block 2 passes its cap, an error or a warning as above; a stage whose block 2 does not render is an error. Block 4
// (the voice, never trimmed either) is held to its 230 the same way, once per persona (S11.3.c: the pilot's draft voice
// came to 273). PL9's headroom for the player's longest instructions waits for the profile lints (v0.3).
void lint_identity_blocks(const ContentPack& pack, const DialoguePrompt& prompt, std::vector<ContentError>& errors,
                          std::vector<ContentError>& warnings);

// The wording lints that need the lexicon (NPC §4.8, the part of PL4 and PL5 the first conversation can check), as
// load_speech runs them: a banned word (the hard lists) in a prompt-facing field or a stage's rendered text, a quoted
// sample line in the voice, the claims or the instructions, and a subject or stir that is no noun phrase, each an error
// for shipped content and a test persona and a warning for the player's own; an effect adjective (NPC §3.3) a warning.
// Not yet: the safety list's and the lexicon's checks (PL6, `lexicon/public.toml`), the 4-word overlap and the seeds'.
void lint_persona_wording(const ContentPack& pack, const Lexicon& lexicon, std::vector<ContentError>& errors,
                          std::vector<ContentError>& warnings);

}  // namespace en::core
