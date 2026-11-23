#pragma once
// The content pack as the conversation reads it (first conversation, part B; WHY§364): the persona index
// (`identity/personas.toml`, NPC §2.1) and each persona's record (`personas/<id>.toml`, NPC §3.1), from the shipped
// content and the player's overrides folder (DESIGN §11.4: the same paths, each file there replacing the shipped one
// whole). Pure (ARCH §2.3): the shell reads the files and hands their texts over; this parses and checks them.
//
// Every file is checked before toml++ reads it (en-proto's unsafe_for_toml), held to its cap (64 KB, a persona's
// 16 KB), read strictly (an unknown key is an error) and held to NPC §3.1's types and caps. An error names the file,
// the line and the key, in fixed words: never a value of the file. A record is kept as JSON, as the content pack holds
// it, for part A's renderer (prompt.hpp's DialogueContext::record), and the few fields the conversation and the lint
// act on are typed beside it.
//
// What v0.2 leaves for later (src/en-core/README.md): `overrides/disable.toml` (P3 §8.4), the seed's combat keys
// (NPC §9.11), the alias table's use (§2.4) and the lints of NPC §4.8 and MEM §12.10 beyond these schemas.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/proto/expected.hpp"

namespace en::core {

// Where a file came from: the content pack shipped with EldenNet, or the player's overrides folder.
enum class ContentOrigin : std::uint8_t { Shipped, Override };

struct ContentFile {
  // Relative to its root, '/'-separated, as both roots hold it: "identity/personas.toml", "personas/kale.toml".
  std::string path;
  std::string text;
  ContentOrigin origin = ContentOrigin::Shipped;
};

struct ContentError {
  std::string file;      // the file's path, as above; empty for the pack as a whole
  std::size_t line = 0;  // 1-based; 0 for the whole file
  std::string problem;   // fixed words and the key's path, never a value of the file

  friend bool operator==(const ContentError&, const ContentError&) = default;
};
[[nodiscard]] std::string describe(const ContentError& e);  // "personas/x.toml:12: voice.openers: ..."

inline constexpr std::size_t kContentFileMaxBytes = 64 * 1024;  // DESIGN §11.4
inline constexpr std::size_t kPersonaFileMaxBytes = 16 * 1024;
inline constexpr std::string_view kPersonaIndexPath = "identity/personas.toml";

// ---- identity/personas.toml (NPC §2.1) ----

struct NpcParamRow {
  std::uint32_t param = 0;           // the NpcParam row
  std::string form;                  // default, summon, boss, hostile, later, ...
  std::uint32_t expect_name_id = 0;  // 0: not filled yet
  bool verified = false;
  std::string reveal_stage;          // empty: none

  friend bool operator==(const NpcParamRow&, const NpcParamRow&) = default;
};

struct EntityRow {
  std::uint32_t id = 0;  // never 0
  bool verified = false;

  friend bool operator==(const EntityRow&, const EntityRow&) = default;
};

// A persona's flag block in the catalogue (NPC §2.1; MEM §6.1): its alias prefix and base, which must agree with the
// catalogue's `[[block]]` (knowledge.hpp checks it).
struct FlagBlockRef {
  std::string alias;
  std::uint32_t base = 0;  // 0: unknown
  bool verified = false;

  friend bool operator==(const FlagBlockRef&, const FlagBlockRef&) = default;
};

struct PersonaRow {
  std::string id;    // slug, at most 32 bytes: the memory key
  std::string file;  // "personas/<name>.toml"
  std::optional<std::uint16_t> talk_code;  // metadata only, never used to resolve
  std::optional<FlagBlockRef> flag_block;
  std::vector<NpcParamRow> npc_params;
  std::vector<EntityRow> entity_ids;
  bool enabled = true;
  bool test_only = false;  // never put in the game's index (NPC §2.1): the self-test, the eval and tests only
  std::size_t line = 0;

  friend bool operator==(const PersonaRow&, const PersonaRow&) = default;
};

// ---- The player's own layer over a shipped persona (NPC §4.5, §4.7; profile.hpp reads, checks and writes it) ----

struct PersonaLayer {
  std::string file;       // its path, "layers/persona-<id>.toml"
  std::string persona;    // the persona's id
  std::uint32_t rev = 0;  // +1 on every save
  std::string base_hash;  // the shipped record's hash when it was saved; empty: not known
  bool assisted = false;  // drafted = "assisted": a draft of the helper's was taken (PL12)
  // What it sets, by kLayerFields' ids: "instructions" a string, "subjects" an array of strings, "register" one of
  // register_names(), "reply_words_max" a whole number. A field it does not set is absent.
  nlohmann::json fields = nlohmann::json::object();

  bool operator==(const PersonaLayer&) const = default;
};

// What a check found, in the editor's form (GraceUI's {field, item, level, words}, D217).
enum class LayerProblem : std::uint8_t {
  NotRead,       // the file is past its cap, refused before it is parsed, or no valid TOML
  NewerSchema,   // written by a newer EldenNet: read-only here
  BadSchema,     // no schema, or one this EldenNet does not read
  Target,        // its target is not the path's persona, or names no persona of the pack
  UnknownKey,    // a key a layer may not set
  WrongType,     // a value of the wrong kind
  TooManyWords,  // past `limit` words
  TooManyChars,  // past `limit` characters
  TooManyItems,  // past `limit` items
  OutOfRange,    // reply_words_max outside 10 to 60
  NotAChoice,    // a register that is not one of the five
  Empty,         // an empty text or item
  Template,      // {{, {% or {#: an error (PL5)
  Safety,        // a word of the safety list: an error (PL5; O107)
  Banned,        // a banned word (PL5): a warning
  Quote,         // a quoted sample line (PL5; NPC §3.3): a warning
  Effect,        // an effect adjective (PL5; NPC §3.3): a warning
  NounPhrase,    // a subject that is no noun phrase (PL4): a warning
  Block4,        // block 4 with the layer's instructions past its cap (WHY§472): a warning; `value` the estimate
  ContentDown,   // the content itself does not load, so nothing can be checked now
};

struct LayerFinding {
  std::string file;  // the layer's path
  std::string field;  // one of kLayerFields, or empty for the whole file
  int item = -1;      // a list's item, from 0; -1 for the field as a whole
  bool error = false;
  LayerProblem problem = LayerProblem::NotRead;
  std::uint32_t limit = 0;  // the cap the words name: words, characters, items or tokens
  std::uint32_t value = 0;  // Block4: the estimate

  bool operator==(const LayerFinding&) const = default;
};

// ---- A persona's record (NPC §3.1) ----

enum class Register : std::uint8_t { ArchaicRegal, Formal, Colloquial, Broken, Chant };
enum class ChatMode : std::uint8_t { Allowed, Refuse, Mute };              // stages.chat (§3.2)
enum class Conscious : std::uint8_t { Awake, Asleep, Senseless, Unknown };  // stages.state (§3.2)
enum class Approach : std::uint8_t { None, Native, Unknown };              // stages.approach (§5.8)

[[nodiscard]] const char* to_string(Register v) noexcept;   // "archaic-regal", "formal", ...
[[nodiscard]] const char* to_string(ChatMode v) noexcept;   // "allowed", "refuse", "mute"
[[nodiscard]] const char* to_string(Conscious v) noexcept;  // "awake", "asleep", "senseless", "unknown"

struct Tic {
  std::string text;
  std::uint8_t max_per_conversation = 1;  // 1 or 2

  friend bool operator==(const Tic&, const Tic&) = default;
};

struct PersonaStage {
  std::string id;
  std::string when;  // the predicate's source (§3.2); compiled by the directory (identity.hpp)
  // The stage's own values of the inheriting gate fields (§3.2): unset takes the previous stage's.
  std::optional<Conscious> state;
  std::optional<Approach> approach;
  ChatMode chat = ChatMode::Allowed;  // never inherits; `allowed` unless the stage says otherwise
  bool own_chat = false;              // the stage sets `chat` itself
  // The stage unlocks or shows something of its own (§3.2: `unlock`, `identity`, `beckon`, `gist`, `where`, `mood`,
  // `road`, `at`, `hears`, `knows`, `heard`, `short`, or `chat = allowed`): with an Unknown `when` it is inactive.
  // Otherwise it only restricts, and an Unknown `when` leaves it active.
  bool shows = false;
  std::size_t line = 0;

  friend bool operator==(const PersonaStage&, const PersonaStage&) = default;
};

struct Persona {
  std::string id;
  std::string file;
  ContentOrigin origin = ContentOrigin::Shipped;  // its file's: the shipped pack's, or the player's overrides'
  std::string display_name_fallback;  // the runtime NpcName wins once text is read (§2.7)
  Register reg = Register::Formal;
  std::uint8_t reply_words_max = 40;
  std::vector<std::string> address_forms;
  std::vector<std::string> deflections;  // shown after a double lint failure (§8.4), never prompted
  std::vector<std::string> openers;      // voice.openers
  std::vector<Tic> tics;                 // voice.tics
  std::vector<PersonaStage> stages;      // in file order; the first is the base stage, `when = "true"`
  nlohmann::json record;                 // the file as it is, tables as objects
  std::string hash;                      // its file's hash (profile.hpp's record_hash): a layer's base_hash
  // The player's layer merged into it (profile.hpp's apply_layers): the typed register and reply length above already
  // take its values, and resolved() renders its instructions and register. `record` stays as the file holds it.
  std::optional<PersonaLayer> layer;

  // The record for a dialogue prompt at stage `active` (prompt.hpp's DialogueContext::record), exactly what the
  // renderer reads (prompt.hpp's kRecordFields) as the file holds it: `stage` is the active stage with §3.2's
  // inheritance applied (`road`, `at`, `hears`, `knows` from the last stage that set them), and nothing else is kept,
  // so what is never rendered never reaches the renderer: the stages' `forbid` (eval-only, O36, O86), `when`, gate
  // fields, `beckon` and the rest; the record's id, `seed` and `deflections` (shown directly, never prompted); its
  // knowledge and awareness. An index past the last stage is the last stage.
  // With `with_layer` (the default) the player's layer's instructions, register and reply length are its own; without, the
  // record is the shipped one alone (the shipped content's lints).
  [[nodiscard]] nlohmann::json resolved(std::size_t active, bool with_layer = true) const;
  // The gate fields at stage `active`, inheritance applied: the state (the base stage sets it), the approach (unset
  // reads unknown) and the chat mode.
  [[nodiscard]] Conscious state_at(std::size_t active) const noexcept;
  [[nodiscard]] Approach approach_at(std::size_t active) const noexcept;
  [[nodiscard]] ChatMode chat_at(std::size_t active) const noexcept;
  // The stage's index whose `forbid` topics the eval asks about (NPC §12.1): its own list, never rendered.
  [[nodiscard]] std::vector<std::string> forbid_at(std::size_t active) const;
};

struct ContentPack {
  std::vector<PersonaRow> rows;    // identity/personas.toml's [[persona]], in file order
  std::vector<Persona> personas;   // one per enabled row, in the rows' order
  std::uint64_t hash = 0;          // FNV-1a over the files read, in order: a change of content changes it
  // The pack's other files, the overrides applied, by path (prompts/, lexicon/ and the rest: part A's renderer and lint
  // read them), unread here and not in the hash.
  std::vector<ContentFile> others;
  // The overrides' layer files (`layers/persona-<id>.toml`) that passed their schema (PL1), not yet merged: the core
  // merges them once the speech parts are loaded (profile.hpp's apply_layers). In the hash.
  std::vector<PersonaLayer> layers;
  // What reading them found (PL1's errors refuse a layer, never the pack).
  std::vector<LayerFinding> layer_findings;

  [[nodiscard]] const Persona* persona(std::string_view id) const noexcept;
  [[nodiscard]] const PersonaRow* row(std::string_view id) const noexcept;
};

// Reads the pack: the overrides replace the shipped files of the same path whole, and a file only in the overrides is
// added. The index must be there; each enabled row's file must be there and hold a persona with the row's id. Files
// outside identity/ and personas/ are kept as they are (`others`); the persona files no row names are left unread. Any error refuses the pack, every error listed (at most 64, then a count).
[[nodiscard]] proto::Expected<ContentPack, std::vector<ContentError>> load_content(std::span<const ContentFile> files);

}  // namespace en::core
