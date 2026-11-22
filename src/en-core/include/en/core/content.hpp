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

struct PersonaRow {
  std::string id;    // slug, at most 32 bytes: the memory key
  std::string file;  // "personas/<name>.toml"
  std::optional<std::uint16_t> talk_code;  // metadata only, never used to resolve
  std::vector<NpcParamRow> npc_params;
  std::vector<EntityRow> entity_ids;
  bool enabled = true;
  bool test_only = false;  // never put in the game's index (NPC §2.1): the self-test, the eval and tests only
  std::size_t line = 0;

  friend bool operator==(const PersonaRow&, const PersonaRow&) = default;
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

  // The record for a dialogue prompt at stage `active` (prompt.hpp's DialogueContext::record), exactly what the
  // renderer reads (prompt.hpp's kRecordFields) as the file holds it: `stage` is the active stage with §3.2's
  // inheritance applied (`road`, `at`, `hears`, `knows` from the last stage that set them), and nothing else is kept,
  // so what is never rendered never reaches the renderer: the stages' `forbid` (eval-only, O36, O86), `when`, gate
  // fields, `beckon` and the rest; the record's id, `seed` and `deflections` (shown directly, never prompted); its
  // knowledge and awareness. An index past the last stage is the last stage.
  [[nodiscard]] nlohmann::json resolved(std::size_t active) const;
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

  [[nodiscard]] const Persona* persona(std::string_view id) const noexcept;
  [[nodiscard]] const PersonaRow* row(std::string_view id) const noexcept;
};

// Reads the pack: the overrides replace the shipped files of the same path whole, and a file only in the overrides is
// added. The index must be there; each enabled row's file must be there and hold a persona with the row's id. Files
// outside identity/ and personas/ are kept as they are (`others`); the persona files no row names are left unread. Any error refuses the pack, every error listed (at most 64, then a count).
[[nodiscard]] proto::Expected<ContentPack, std::vector<ContentError>> load_content(std::span<const ContentFile> files);

}  // namespace en::core
