#pragma once
// Providers, models, slots, presets, the request budget and keys (ARCH §11.1, §11.4, §11.5): the part of the
// configuration the model client reads.
#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/llm/clock.hpp"
#include "en/llm/secret.hpp"
#include "en/llm/transport.hpp"
#include "en/proto/llm.hpp"

namespace en::llm {

enum class Flavor { OpenAI, Ollama, LlamaCpp, LmStudio, KoboldCpp };
enum class ProviderKind { Cloud, Local };
enum class JsonMode { None, JsonObject, JsonSchema };
// How a provider takes en-prompt's cache marks (NPC §7.1): explicit breakpoints (OpenRouter's cache_control on
// content parts), llama.cpp's cache_prompt flag, or nothing but the order (providers that cache prefixes themselves).
enum class CacheMarks { None, Breakpoints, CachePrompt };

const char* to_string(Flavor f);
const char* to_string(ProviderKind k);

struct ProviderConfig {
  std::string id;
  Flavor flavor = Flavor::OpenAI;
  ProviderKind kind = ProviderKind::Cloud;
  std::string base_url;            // without a trailing slash; requests go to base_url + "/chat/completions"
  std::string key_ref;             // "secrets:NAME" or "env:VAR": names only, safe to show
  std::optional<Secret> key;       // resolved from key_ref at load, held in memory only
  std::vector<HttpHeader> headers;                              // plain headers
  std::vector<std::pair<std::string, Secret>> secret_headers;   // headers whose value was a secrets: or env: ref
  nlohmann::json extra_body = nlohmann::json::object();  // merged into every body; holds the reasoning switch
  int max_concurrency = 2;         // cloud 2, local 1 (impl)
  JsonMode json_mode = JsonMode::None;
  std::optional<std::string> quant_floor;  // where the provider filters upstreams by quantisation (OpenRouter)
  CacheMarks cache_marks = CacheMarks::None;
  bool logprobs = false;           // whether the provider returns logprobs (decision calls ask for the top 5)
  std::vector<std::string> unresolved_refs;  // refs (names only) that found no key: the provider is not used

  bool keys_resolved() const { return unresolved_refs.empty(); }
};

struct ModelRef {
  std::string provider;
  std::string model;
};

// One slot's budgets (ARCH §11.4). The same in every preset, so they are code, not configuration.
struct SlotBudget {
  int max_tokens = 140;
  double temperature = 0.8;
  Millis first_token{6000};
  Millis first_token_local{10000};
  Millis total{20000};
  int retries = 0;      // same-model retries the slot's failure actions may spend
  int concurrency = 1;  // requests of this slot in flight at once
};

struct SlotConfig {
  std::string name;
  SlotBudget budget;
  std::vector<ModelRef> models;  // the slot's model first, then its fallback
};

// The slots' names are en-proto's (llm.hpp): the core names the slot a request goes on.
using en::proto::kActions;
using en::proto::kBarkLive;
using en::proto::kBarkPool;
using en::proto::kDecisions;
using en::proto::kDialogue;
using en::proto::kDraft;
using en::proto::kMemory;
using en::proto::kSelftest;
using en::proto::kVision;

// Every slot, in ARCH §11.4's order: a constant, so no static object is made or destroyed for it (ARCH §3.1, §1.7).
inline constexpr std::array<std::string_view, 7> kKnownSlots = {kDialogue, kBarkLive, kBarkPool, kMemory,
                                                                kSelftest, kDraft,    kDecisions};
constexpr std::span<const std::string_view> known_slots() { return kKnownSlots; }
SlotBudget default_budget(std::string_view slot);

// The slots reserved for later uses (O88; ARCH §11.4): [slots.*] and the presets take them, read and checked as every
// slot is, so that the files keep their shape when the features arrive. They have no budget yet and no request uses
// them: one that names them falls through to dialogue (Config::slot), as an unknown name does.
inline constexpr std::array<std::string_view, 2> kReservedSlots = {kActions, kVision};
constexpr std::span<const std::string_view> reserved_slots() { return kReservedSlots; }
constexpr bool is_reserved_slot(std::string_view slot) {
  for (const std::string_view reserved : kReservedSlots) {
    if (reserved == slot) return true;
  }
  return false;
}

struct Config {
  std::string preset = "custom";
  std::vector<ProviderConfig> providers;
  std::map<std::string, SlotConfig, std::less<>> slots;  // every known slot, with or without a model
  // Each reserved slot's models as the files name them (none where they name none): checked, and used by nothing yet.
  std::map<std::string, std::vector<ModelRef>, std::less<>> reserved;
  int requests_per_hour = 600;  // budget.requests_per_hour, 60 to 3600 (ARCH §11.4, §11.5)
  Millis backup_after{2500};    // a cloud reply's backup request (ARCH §11.2; EST)
  std::vector<std::string> warnings;  // things the loader could not resolve but that do not stop the client

  const ProviderConfig* provider(std::string_view id) const;
  // Unknown slot names fall through to dialogue (ARCH §11.4), and so, until their features exist, do the reserved ones.
  const SlotConfig& slot(std::string_view name) const;
};

// A configuration problem. The message names the file and what is wrong; it never quotes a key.
class ConfigError : public std::runtime_error {
 public:
  ConfigError(const std::string& file, const std::string& what) : std::runtime_error(file + ": " + what) {}
};

using EnvLookup = std::function<std::optional<std::string>(const std::string&)>;
std::optional<std::string> system_env(const std::string& name);

// The files' texts. providers.toml is required; models.toml when providers.toml names a preset other than
// "custom"; settings.toml for the request budget; secrets.toml for keys. A preset of models.toml names the providers
// its slots use, in one of two forms (one `provider = { ... }`, or named [preset.<id>.providers.<name>] tables, each
// slot naming its own; O88), and each is matched to a provider of providers.toml, which holds the endpoints' keys: a
// preset never holds one.
struct ConfigTexts {
  std::string providers;
  std::optional<std::string> models;
  std::optional<std::string> settings;
  std::optional<std::string> secrets;
};

Config parse_config(const ConfigTexts& texts, const EnvLookup& env = system_env);

// Reads providers.toml, and models.toml, settings.toml and secrets.toml where present, from `dir`; `models_file`
// replaces dir/models.toml (the shipped one lives in content/eldenring/). Files over 64 KB are refused (DESIGN §11.4).
Config load_config_dir(const std::filesystem::path& dir, const std::optional<std::filesystem::path>& models_file = {},
                       const EnvLookup& env = system_env);

// The levels a quantisation floor admits, lowest first: the floor and everything above it ("unknown" never).
std::vector<std::string> quantizations_from(std::string_view floor);

}  // namespace en::llm
