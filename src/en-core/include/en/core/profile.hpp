#pragma once
// The player's own layer over a shipped persona, and the "draft with AI" helper's prompt and reading (NPC §4.5's "What a
// layer may set", NPC §4.7, §4.8; S7.2.b, WHY§377). Pure (ARCH §2.3): the shell reads and writes the files; this
// parses, checks, merges and writes their text.
//
// - **The file.** `layers/persona-<id>.toml` in the player's overrides folder (the brief's S7.2.b: the overrides folder
//   the load already reads, in NPC §4.7's file name), never shipped: `schema` (1), `target` ("persona:<id>", the
//   path's), `rev` (+1 on every save; a save on an older one is a conflict), `base_hash` (the shipped record's hash when
//   it was saved, so that the editor can say the shipped profile changed since), `drafted` ("assisted" once a draft of
//   the helper's was taken, PL12's provenance; "hand" or absent otherwise), and the fields a layer may set:
//   `instructions`, `subjects` (replacing the shipped list), `register` and `reply_words_max`. Nothing else: a layer
//   never sets identity, the public or private fields, stages, knowledge or anything the editor does not show.
// - **The checks** (NPC §4.8's player rules, the same the load runs): PL1's schema and caps are errors (the file is
//   refused and the shipped profile stands alone); PL5's template marks and the safety list are errors; a banned word,
//   a quoted sample line, an effect adjective, a subject that is no noun phrase (PL4) and block 4 past its cap with the
//   player's instructions (WHY§472, as built: a warning for the player's own) are warnings, and the layer still loads.
//   Every finding names a field and an item in our own words, never a value of the file.
// - **The load.** load_content (content.hpp) reads each layer file of the overrides and checks its schema; the core,
//   once the speech parts are loaded, runs the wording and block 4 checks (apply_layers) and merges each layer that
//   holds no error into its persona (`Persona::layer`): the typed register and reply length take its values, and
//   `Persona::resolved` renders its instructions and register. The shipped record (`Persona::record`) stays as shipped,
//   so the shipped content's own lints never see the player's text.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/core/content.hpp"
#include "en/proto/expected.hpp"
#include "en/proto/llm.hpp"

namespace en::core {

class DialoguePrompt;
struct Lexicon;

inline constexpr std::string_view kLayerFolder = "layers/";
inline constexpr std::size_t kLayerFileMaxBytes = 8 * 1024;  // NPC §4.7's cap a file
inline constexpr std::int64_t kLayerSchema = 1;
inline constexpr std::size_t kLayerSubjectsMax = 8;          // NPC §3.1's most on a persona
inline constexpr std::size_t kLayerSubjectWords = 10;
inline constexpr std::size_t kLayerSubjectChars = 50;
inline constexpr std::size_t kLayerInstructionWords = 35;
inline constexpr std::size_t kLayerInstructionChars = 210;
inline constexpr int kLayerReplyWordsMin = 10;
inline constexpr int kLayerReplyWordsMax = 60;

// The fields a layer may set, by the ids the editor names them with (NPC §4.5).
inline constexpr std::array<std::string_view, 4> kLayerFields = {"instructions", "subjects", "register",
                                                                 "reply_words_max"};
// The registers, in NPC §3.3's order, as a layer's `register` names them.
[[nodiscard]] std::span<const std::string_view> register_names() noexcept;

// "layers/persona-<id>.toml", and the persona id such a path names (nothing for any other path).
[[nodiscard]] std::string layer_path(std::string_view persona_id);
[[nodiscard]] std::optional<std::string> layer_persona(std::string_view path);

// A shipped record's hash as a layer's `base_hash` holds it: FNV-1a 64 of the file's text, 16 lower-case hex digits.
[[nodiscard]] std::string record_hash(std::string_view text);

// PersonaLayer, LayerProblem and LayerFinding are content.hpp's, since a persona holds its layer.

// The editor's words for a finding (drafts for the owner's review, British spelling): never a value of the file.
[[nodiscard]] std::string words(const LayerFinding& f);
// The finding as the load lists it among the content's warnings: "<file>:0: <field>[<item>]: <words>", with "refused: "
// before an error's words (the layer was not loaded).
[[nodiscard]] ContentError to_content_error(const LayerFinding& f);

// Reads one layer file: its size, the TOML pre-check, the parse and PL1's schema and caps (errors), with `target` the
// path's persona. Its findings go to `findings`; the layer is returned when none is an error.
[[nodiscard]] std::optional<PersonaLayer> read_layer(const ContentFile& file, std::vector<LayerFinding>& findings);

// A layer file's revision as it states it, whatever else the file holds wrong (0 when it states none, or cannot be read):
// the editor's conflict check over a refused file too.
[[nodiscard]] std::uint32_t layer_rev(std::string_view text);

// The layer as its file's text, which read_layer reads back as it was: the editor's save (NPC §4.7). A prose field's
// line breaks and tabs become spaces (one paragraph, GraceUI's G38 (3)).
[[nodiscard]] std::string layer_text(const PersonaLayer& layer);

// The checks that need the pack's speech parts, then the merge (the core's load, after load_speech): for each layer of
// `pack.layers`, its persona (a layer whose persona the pack lacks is a Target warning and is left out), the wording of
// its instructions and subjects against `lexicon` (template marks and safety errors; banned words, quotes, effect words
// and noun phrases warnings) and block 4 rendered with it (a warning past the cap). A layer with no error is merged into
// its persona; its findings join `findings`.
void apply_layers(ContentPack& pack, const DialoguePrompt& prompt, const Lexicon& lexicon,
                  std::vector<LayerFinding>& findings);

// The editor's check (S7.2.b): the content load run again, as the core runs it, over `files` (the shipped pack and the
// overrides as the shell holds them) with `candidate` in place of the file of its path, and its findings alone. With
// nothing else of the content loading, one ContentDown finding.
struct LayerCheck {
  std::vector<LayerFinding> findings;
  std::optional<PersonaLayer> layer;  // as read and merged, when it holds no error
  [[nodiscard]] bool ok() const noexcept;  // no error among the findings
};
[[nodiscard]] LayerCheck check_layer(std::span<const ContentFile> files, const ContentFile& candidate);

// ---- The "draft with AI" helper (NPC §4.7) ----

inline constexpr std::string_view kDraftPromptPath = "prompts/draft/10_profile.j2";
inline constexpr std::size_t kDraftNotesMax = 400;  // characters of the player's notes
inline constexpr int kDraftsPerSession = 20;
inline constexpr int kDraftMaxTokens = 500;  // the slot's own budget (en-llm's draft slot)

// What a draft of a layer reads: the player's notes, the fixed phrase of the profile's kind (our words), and the
// player's own current text of the fields drafted, when revising. Never the runtime name, game text, a shipped profile's
// private fields, stages, knowledge, memories or journals.
struct DraftInput {
  std::string notes;
  std::string_view kind_phrase = "a named person";
  nlohmann::json current = nlohmann::json::object();  // "instructions" and "subjects", as the player has them
};

// The request's messages, from the pack's prompts/draft/10_profile.j2 (among `others`), or why they could not be made.
[[nodiscard]] proto::Expected<std::vector<proto::Message>, std::string> draft_messages(
    std::span<const ContentFile> others, const DraftInput& in);
// The reply's JSON schema, for providers that take one: the drafted fields alone.
[[nodiscard]] nlohmann::json draft_schema();

// A draft read: each proposal by field id (GraceUI's {draft: {field: value}}), and what was done to it.
struct DraftResult {
  nlohmann::json draft = nlohmann::json::object();
  bool unreadable = false;  // no JSON object came back
  bool discarded = false;   // a word of the safety list: nothing is proposed
  std::size_t dropped = 0;  // sentences and subjects left out for a name neither the notes nor the current text hold
  std::size_t cut = 0;      // fields cut to their caps, at a sentence's end, and subjects past the eighth left out
};
// Reads a reply: its JSON object (unknown fields dropped), each field held to its caps, every sentence or subject naming
// someone (a capitalised word that is not the first) that neither the notes nor the current text hold dropped without a
// mark, since the helper invents no names, and the whole draft discarded on a word of the safety list.
[[nodiscard]] DraftResult read_draft(std::string_view reply, const DraftInput& in, const Lexicon& lexicon);

}  // namespace en::core
