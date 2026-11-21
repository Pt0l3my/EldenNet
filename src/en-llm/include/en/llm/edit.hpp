#pragma once
// Changes to providers.toml and secrets.toml (ARCH §11.1, §11.4, §11.5) made from EldenNet's AI pages: the preset in
// use, the providers a preset names, the providers no job uses, and the keys. Each edit takes the file's text and
// gives the new one, or why not in our own words: never a key, never the file's text. Every other key and table of the
// file is kept with its value; comments and the layout are not, since toml++ writes the tables again (as the pages'
// settings writes do). The caller checks the result with parse_config before it relies on it, and writes it beside
// the file and renames it over.
//
// A preset never holds a key (O88): the providers it names are written to providers.toml with a key_ref of their
// own, `secrets:<id>`, and the key goes to secrets.toml under that name, written only through edit_secrets.
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/llm/config.hpp"
#include "en/llm/secret.hpp"

namespace en::llm {

// One provider of providers.toml as the pages write it: the endpoint and where its key is, with the defaults of
// ARCH §11.1 for the rest (requests at once by kind, the reasoning switch and headers left to the player's file).
struct ProviderEntry {
  std::string id;
  Flavor flavor = Flavor::OpenAI;
  ProviderKind kind = ProviderKind::Cloud;
  std::string base_url;
  std::string key_ref;                       // "secrets:<id>" for a cloud provider; empty for none
  JsonMode json_mode = JsonMode::JsonSchema;  // PAGES §4.6.2's default

  bool operator==(const ProviderEntry&) const = default;
};

// A job's model as the pages choose it (PAGES §4.5's `[slots.<slot>]` row): the slot's `provider` and `model` written,
// any fallback the player's file gives it kept; an empty model removes the slot's table, so the preset's models hold
// again ("Reset to preset").
struct SlotEdit {
  std::string slot;      // a slot in use (known_slots())
  std::string provider;  // with a model: the provider's id, which providers.toml has
  std::string model;     // empty: the slot's [slots.<slot>] removed

  bool operator==(const SlotEdit&) const = default;
};

// Where a provider's key is kept, set on its entry: `secrets:<name>`, for a key the pages set on a provider whose entry
// had none (a server of the player's own on this PC).
struct KeyRefEdit {
  std::string id;
  std::string key_ref;

  bool operator==(const KeyRefEdit&) const = default;
};

struct ProvidersEdit {
  std::optional<std::string> preset;  // providers.toml's preset ("custom" or a preset of models.toml)
  std::vector<ProviderEntry> add;     // appended, each once: an id providers.toml has already is refused
  std::vector<std::string> remove;    // by id, with every [slots.*] entry that names one of them
  std::vector<SlotEdit> slots;        // made after the rest, in order
  std::vector<KeyRefEdit> key_refs;   // likewise
};

struct EditedText {
  bool ok = false;
  std::string text;
  // When not ok: "refused" (the file's text fails its checks: it is never written over), "not_a_table",
  // "duplicate_id", "unknown_id", "bad_entry", "unknown_slot".
  std::string why;
};

// A provider of the player's own, from AI › Providers' form: an OpenAI-compatible endpoint (ARCH §11.1's default
// flavour), local when its host is this PC's (one request at once, no key until one is set on its row) and cloud
// otherwise, its key then in secrets:<id>. `base_url` is as en-proto's check_base_url wrote it.
[[nodiscard]] ProviderEntry own_provider(std::string id, std::string base_url, bool loopback);

// providers.toml's text with `edit` made; an empty text makes a new file (`schema = 1` and the edit).
[[nodiscard]] EditedText edit_providers(std::string_view text, const ProvidersEdit& edit);

// The presets models.toml defines, by id (toml++ keeps a table's keys in order); none when it has none or fails its
// checks.
[[nodiscard]] std::vector<std::string> preset_ids(std::string_view models_text);

// The providers the preset names that none of `have` matches (as parse_config matches them: by base_url where the
// preset gives one, else by kind and flavour), as entries to add: each named as the preset names it (its one
// `provider = { ... }` by the preset's id), with its key in secrets:<id> when it is a cloud one. Throws ConfigError
// for a preset models.toml lacks, and for a provider the preset gives no base_url and `have` does not match (a local
// preset's, which the local probe fills: later work).
[[nodiscard]] std::vector<ProviderEntry> preset_providers(std::string_view models_text, std::string_view preset,
                                                          const std::vector<ProviderConfig>& have);

// The providers no slot names, for its model or its fallback, the reserved slots included: in providers.toml's order.
[[nodiscard]] std::vector<std::string> unused_providers(const Config& config);

// The secrets.toml names `providers` refer to (key_ref and header values "secrets:NAME").
[[nodiscard]] std::vector<std::string> secret_names(const std::vector<ProviderConfig>& providers);

struct EditedSecrets {
  bool ok = false;
  Secret text;      // the new file's text, which holds every key: never logged, shown or kept in a std::string
  std::string why;  // when not ok: "refused", "not_a_table", "bad_name"
};

// secrets.toml's text with the key `name` set to `value`, or removed when `value` is null; an empty text makes a new
// file. A name is a provider id's form (1 to 64 letters, digits, '_', '-' or '.').
[[nodiscard]] EditedSecrets edit_secrets(std::string_view text, std::string_view name, const Secret* value);

// The names secrets.toml holds a key under (a non-empty string), by name; none when it fails its checks. For the
// pages' key rows, which say only whether a key is set.
[[nodiscard]] std::vector<std::string> keys_set(std::string_view secrets_text);

// A key as the player typed it: surrounding spaces and line ends taken off. Nothing when what is left is empty, longer
// than 1024 bytes, or holds a control character or a space.
[[nodiscard]] std::optional<Secret> clean_key(std::string_view typed);

}  // namespace en::llm
