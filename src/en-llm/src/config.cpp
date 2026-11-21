// Reading providers.toml, models.toml, settings.toml and secrets.toml (ARCH §11.1, §11.4, §11.5).
//
// Every slot names its provider, model and fallback, so that each use can have a provider of its own (O88); a preset
// of models.toml names the providers its slots use, and each is matched to a provider of providers.toml, which alone
// holds keys (by reference, into secrets.toml or the environment).
//
// Every table the client owns is read strictly: an unknown key is an error, as `deny_unknown_fields` is in the design,
// and a newer schema is refused. settings.toml belongs mostly to other modules, so only its `budget` table is read
// here (strictly); the rest of that file is theirs to check. Errors name the file and the line, and never a key's value.
#include "en/llm/config.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <sstream>

#include <toml.hpp>

#include "en/proto/base_url.hpp"
#include "en/proto/toml_check.hpp"

namespace en::llm {

using nlohmann::json;

namespace {

constexpr std::int64_t kProvidersSchema = 1;
constexpr std::int64_t kModelsSchema = 1;
constexpr std::int64_t kSettingsSchema = 2;

// The levels OpenRouter's provider.quantizations takes, lowest first.
constexpr std::array<std::string_view, 8> kQuantLevels = {"int4", "fp4", "fp6", "int8", "fp8", "fp16", "bf16", "fp32"};

// Fields the client writes itself; extra_body may not set them.
constexpr std::array<std::string_view, 11> kClientFields = {
    "model", "messages", "stream", "stream_options", "max_tokens", "max_completion_tokens",
    "temperature", "response_format", "logprobs", "top_logprobs", "n"};

struct Ctx {
  std::string file;
  std::string where;

  [[noreturn]] void fail(const std::string& what, const toml::node* at = nullptr) const {
    fail(ConfigRefusal::Other, what, at);
  }
  [[noreturn]] void fail(ConfigRefusal refusal, const std::string& what, const toml::node* at) const {
    std::string message = where.empty() ? what : where + ": " + what;
    std::uint32_t line = 0;
    if (at != nullptr && at->source().begin.line > 0) {
      line = at->source().begin.line;
      message += " (line " + std::to_string(line) + ")";
    }
    throw ConfigError(file, message, refusal, line);
  }
  Ctx sub(std::string inner) const { return Ctx{file, std::move(inner)}; }
};

// toml++ is built without exceptions in every module (CMakeLists.txt), so its parse answers a result.
// Every text is checked before toml++ reads it (en-proto's toml_check.hpp; WHY§122): toml++ 3.4.0 has traps that are
// undefined behaviour in a release build, and these files are the player's, edited by hand. The refusal names the line
// and the rule, never the text.
toml::table parse_toml(const std::string& file, std::string_view text, bool secret) {
  if (const std::optional<proto::TomlRefusal> unsafe = proto::unsafe_for_toml(text, proto::TomlText::Player)) {
    throw ConfigError(file, "line " + std::to_string(unsafe->line) + ": " + unsafe->problem);
  }
  toml::parse_result result = toml::parse(text, file);
  if (!result) {
    const toml::parse_error& e = result.error();
    const auto line = std::to_string(e.source().begin.line);
    // The parser's description can quote the text it stopped at, which in secrets.toml may be a key.
    if (secret) throw ConfigError(file, "not valid TOML near line " + line);
    throw ConfigError(file, std::string(e.description()) + " (line " + line + ")");
  }
  return std::move(result).table();
}

void check_keys(const toml::table& table, std::initializer_list<std::string_view> allowed, const Ctx& ctx) {
  for (auto&& [key, value] : table) {
    if (std::find(allowed.begin(), allowed.end(), key.str()) == allowed.end()) {
      ctx.fail("unknown key \"" + std::string(key.str()) + "\"", &value);
    }
  }
}

std::optional<std::string> get_string(const toml::table& table, std::string_view key, const Ctx& ctx) {
  const toml::node* node = table.get(key);
  if (node == nullptr) return std::nullopt;
  if (!node->is_string()) ctx.fail("\"" + std::string(key) + "\" must be a string", node);
  return node->as_string()->get();
}

std::string require_string(const toml::table& table, std::string_view key, const Ctx& ctx) {
  auto value = get_string(table, key, ctx);
  if (!value || value->empty()) ctx.fail("needs \"" + std::string(key) + "\"");
  return *value;
}

std::optional<std::int64_t> get_int(const toml::table& table, std::string_view key, const Ctx& ctx) {
  const toml::node* node = table.get(key);
  if (node == nullptr) return std::nullopt;
  if (!node->is_integer()) ctx.fail("\"" + std::string(key) + "\" must be an integer", node);
  return node->as_integer()->get();
}

std::optional<bool> get_bool(const toml::table& table, std::string_view key, const Ctx& ctx) {
  const toml::node* node = table.get(key);
  if (node == nullptr) return std::nullopt;
  if (!node->is_boolean()) ctx.fail("\"" + std::string(key) + "\" must be true or false", node);
  return node->as_boolean()->get();
}

const toml::table* get_table(const toml::table& table, std::string_view key, const Ctx& ctx) {
  const toml::node* node = table.get(key);
  if (node == nullptr) return nullptr;
  if (!node->is_table()) ctx.fail("\"" + std::string(key) + "\" must be a table", node);
  return node->as_table();
}

void check_schema(const toml::table& table, std::int64_t supported, const Ctx& ctx, bool required) {
  auto schema = get_int(table, "schema", ctx);
  if (!schema) {
    if (required) ctx.fail("needs schema = " + std::to_string(supported));
    return;
  }
  if (*schema > supported) {
    ctx.fail("schema " + std::to_string(*schema) + " is newer than this build reads (" + std::to_string(supported) +
             "); update EldenNet");
  }
  if (*schema < 1) ctx.fail("schema must be 1 or more");
}

json to_json(const toml::node& node, const Ctx& ctx) {
  if (const auto* table = node.as_table()) {
    json object = json::object();
    for (auto&& [key, value] : *table) object[std::string(key.str())] = to_json(value, ctx);
    return object;
  }
  if (const auto* array = node.as_array()) {
    json list = json::array();
    for (const auto& value : *array) list.push_back(to_json(value, ctx));
    return list;
  }
  if (const auto* s = node.as_string()) return s->get();
  if (const auto* i = node.as_integer()) return i->get();
  if (const auto* f = node.as_floating_point()) return f->get();
  if (const auto* b = node.as_boolean()) return b->get();
  ctx.fail("dates and times are not request values", &node);
}

using Keys = std::map<std::string, Secret, std::less<>>;

Keys parse_secrets(const std::string& text) {
  const Ctx ctx{"secrets.toml", ""};
  const toml::table root = parse_toml(ctx.file, text, /*secret=*/true);
  check_keys(root, {"keys"}, ctx);
  Keys keys;
  const toml::table* table = get_table(root, "keys", ctx);
  if (table == nullptr) return keys;
  for (auto&& [name, value] : *table) {
    // Names are safe to show; values never are.
    if (!value.is_string()) ctx.fail("keys." + std::string(name.str()) + " must be a string", &value);
    keys.emplace(std::string(name.str()), Secret(value.as_string()->get()));
  }
  return keys;
}

// "secrets:NAME" or "env:VAR". A ref that finds nothing is not an error at load: the provider is simply not used
// until the key is there. Anything else is refused without being echoed, since it may be a key written inline.
std::optional<Secret> resolve_ref(const std::string& ref, const Keys& keys, const EnvLookup& env, const Ctx& ctx,
                                  const std::string& field) {
  if (ref.rfind("secrets:", 0) == 0 && ref.size() > 8) {
    auto it = keys.find(std::string_view(ref).substr(8));
    if (it == keys.end() || it->second.empty()) return std::nullopt;
    return it->second;
  }
  if (ref.rfind("env:", 0) == 0 && ref.size() > 4) {
    auto value = env(ref.substr(4));
    if (!value || value->empty()) return std::nullopt;
    return Secret(std::move(*value));
  }
  ctx.fail(field + " must be \"secrets:NAME\" or \"env:VAR\"; keys are never written inline");
}

bool is_ref(const std::string& value) {
  return (value.rfind("secrets:", 0) == 0 && value.size() > 8) || (value.rfind("env:", 0) == 0 && value.size() > 4);
}

bool valid_id(std::string_view id) {
  if (id.empty() || id.size() > 64) return false;
  return std::all_of(id.begin(), id.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '.';
  });
}

std::string normalise_base_url(std::string url) {
  while (!url.empty() && (url.back() == '/' || url.back() == ' ')) url.pop_back();
  while (!url.empty() && url.front() == ' ') url.erase(url.begin());
  return url;
}

template <typename Enum, std::size_t N>
Enum parse_choice(const std::string& value, const std::array<std::pair<std::string_view, Enum>, N>& choices,
                  const std::string& field, const Ctx& ctx) {
  for (const auto& [name, e] : choices) {
    if (value == name) return e;
  }
  std::string list;
  for (const auto& [name, e] : choices) list += (list.empty() ? "" : " | ") + std::string(name);
  ctx.fail(field + " must be " + list);
}

constexpr std::array<std::pair<std::string_view, Flavor>, 5> kFlavors = {{{"openai", Flavor::OpenAI},
                                                                          {"ollama", Flavor::Ollama},
                                                                          {"llamacpp", Flavor::LlamaCpp},
                                                                          {"lmstudio", Flavor::LmStudio},
                                                                          {"koboldcpp", Flavor::KoboldCpp}}};
constexpr std::array<std::pair<std::string_view, ProviderKind>, 2> kKinds = {
    {{"cloud", ProviderKind::Cloud}, {"local", ProviderKind::Local}}};
constexpr std::array<std::pair<std::string_view, JsonMode>, 3> kJsonModes = {
    {{"none", JsonMode::None}, {"json_object", JsonMode::JsonObject}, {"json_schema", JsonMode::JsonSchema}}};
constexpr std::array<std::pair<std::string_view, CacheMarks>, 3> kCacheMarks = {
    {{"none", CacheMarks::None}, {"breakpoints", CacheMarks::Breakpoints}, {"cache_prompt", CacheMarks::CachePrompt}}};

ProviderConfig parse_provider(const toml::table& table, std::size_t index, const Keys& keys, const EnvLookup& env,
                              const Ctx& file_ctx) {
  const Ctx ctx = file_ctx.sub("provider " + std::to_string(index + 1));
  check_keys(table,
             {"id", "flavor", "kind", "base_url", "key_ref", "headers", "extra_body", "max_concurrency", "json_mode",
              "quant_floor", "cache_marks", "logprobs"},
             ctx);
  ProviderConfig p;
  p.id = require_string(table, "id", ctx);
  if (!valid_id(p.id)) ctx.fail("id must be 1 to 64 letters, digits, '_', '-' or '.'");
  const Ctx pctx = file_ctx.sub("provider \"" + p.id + "\"");

  p.flavor = parse_choice(get_string(table, "flavor", pctx).value_or("openai"), kFlavors, "flavor", pctx);
  p.base_url = normalise_base_url(require_string(table, "base_url", pctx));
  const toml::node* url_node = table.get("base_url");
  if (p.base_url.rfind("https://", 0) != 0 && p.base_url.rfind("http://", 0) != 0) {
    pctx.fail(ConfigRefusal::Scheme, "base_url must start with https:// or http://", url_node);
  }
  // en-proto's check, which AI › Providers' form makes too: a key is never written inline, and the client appends its
  // own paths. Neither message quotes the address.
  switch (proto::url_extras(p.base_url)) {
    case proto::UrlProblem::Credentials:
      pctx.fail(ConfigRefusal::Credentials,
                "base_url must hold no name or password: a key goes in secrets.toml, named by key_ref", url_node);
    case proto::UrlProblem::QueryOrFragment:
      pctx.fail(ConfigRefusal::QueryOrFragment, "base_url must end at its path, with no ? or #", url_node);
    default: break;
  }
  // A key must never cross a network in clear text (ARCH §11.1): http:// goes to this PC, or to a literal private
  // address of the home network as a local server with no key; never to a public host or a name, whatever the
  // provider's kind. en-proto's reach_of, which the pages' form and key rows share.
  const proto::Reach reach = proto::reach_of(p.base_url);
  const bool plain_remote = reach == proto::Reach::HomeNetwork || reach == proto::Reach::Public;
  if (reach == proto::Reach::Public) {
    pctx.fail(ConfigRefusal::PlainPublic,
              "base_url uses http:// to a host that is neither this PC nor a private address: it must use https://",
              url_node);
  }
  if (auto kind = get_string(table, "kind", pctx)) {
    p.kind = parse_choice(*kind, kKinds, "kind", pctx);
  } else {
    p.kind = reach == proto::Reach::ThisPc ? ProviderKind::Local : ProviderKind::Cloud;
  }
  if (plain_remote && p.kind == ProviderKind::Cloud) {
    pctx.fail(ConfigRefusal::PlainCloud, "a cloud provider's base_url must use https://", url_node);
  }
  const auto no_key_in_clear = [&](const char* where, const toml::node* at) {
    if (plain_remote) {
      pctx.fail(ConfigRefusal::PlainKey,
                std::string(where) + " names a key, which would go over http:// to another computer: use https://, "
                                     "or no key",
                at);
    }
  };

  if (auto ref = get_string(table, "key_ref", pctx)) {
    no_key_in_clear("key_ref", table.get("key_ref"));
    p.key_ref = *ref;
    p.key = resolve_ref(*ref, keys, env, pctx, "key_ref");
    if (!p.key) p.unresolved_refs.push_back(*ref);
  }

  if (const toml::table* headers = get_table(table, "headers", pctx)) {
    for (auto&& [name, value] : *headers) {
      if (!value.is_string()) pctx.fail("headers." + std::string(name.str()) + " must be a string", &value);
      std::string text = value.as_string()->get();
      if (is_ref(text)) {
        no_key_in_clear(("headers." + std::string(name.str())).c_str(), &value);
        if (auto secret = resolve_ref(text, keys, env, pctx, "headers." + std::string(name.str()))) {
          p.secret_headers.emplace_back(std::string(name.str()), std::move(*secret));
        } else {
          p.unresolved_refs.push_back(text);
        }
      } else {
        p.headers.push_back(HttpHeader{std::string(name.str()), std::move(text)});
      }
    }
  }

  if (const toml::table* extra = get_table(table, "extra_body", pctx)) {
    p.extra_body = to_json(*extra, pctx);
    for (std::string_view field : kClientFields) {
      if (p.extra_body.contains(field)) pctx.fail("extra_body may not set \"" + std::string(field) + "\": the client sets it");
    }
  }

  const std::int64_t concurrency = get_int(table, "max_concurrency", pctx).value_or(p.kind == ProviderKind::Cloud ? 2 : 1);
  if (concurrency < 1 || concurrency > 16) pctx.fail("max_concurrency must be 1 to 16");
  p.max_concurrency = static_cast<int>(concurrency);

  p.json_mode = parse_choice(get_string(table, "json_mode", pctx).value_or("none"), kJsonModes, "json_mode", pctx);

  if (auto floor = get_string(table, "quant_floor", pctx)) {
    if (std::find(kQuantLevels.begin(), kQuantLevels.end(), *floor) == kQuantLevels.end()) {
      pctx.fail("quant_floor must be one of int4, fp4, fp6, int8, fp8, fp16, bf16, fp32");
    }
    p.quant_floor = *floor;
  }

  if (auto marks = get_string(table, "cache_marks", pctx)) {
    p.cache_marks = parse_choice(*marks, kCacheMarks, "cache_marks", pctx);
  } else {
    p.cache_marks = p.flavor == Flavor::LlamaCpp ? CacheMarks::CachePrompt : CacheMarks::None;
  }

  p.logprobs = get_bool(table, "logprobs", pctx).value_or(p.flavor == Flavor::OpenAI || p.flavor == Flavor::LlamaCpp);
  return p;
}

// A provider as a preset gives it (models.toml): what it expects of a providers.toml provider, which holds the
// endpoint's key. It is matched by base_url where the preset gives one, otherwise as the first provider of its kind
// and flavour (either may be left out). A preset never holds a key (O88).
std::string match_provider(const toml::table& spec, const Config& cfg, const Ctx& ctx) {
  for (const std::string_view secret : {std::string_view("key_ref"), std::string_view("key")}) {
    if (const toml::node* node = spec.get(secret)) {
      ctx.fail("a preset never holds a key: keys stay with providers.toml's providers (key_ref)", node);
    }
  }
  check_keys(spec, {"flavor", "kind", "base_url"}, ctx);
  if (auto url = get_string(spec, "base_url", ctx)) {
    const std::string wanted = normalise_base_url(*url);
    for (const auto& p : cfg.providers) {
      if (p.base_url == wanted) return p.id;
    }
    ctx.fail("needs a provider with base_url = \"" + wanted + "\" in providers.toml", &spec);
  }
  std::optional<ProviderKind> kind;
  std::optional<Flavor> flavor;
  if (auto k = get_string(spec, "kind", ctx)) kind = parse_choice(*k, kKinds, "kind", ctx);
  if (auto f = get_string(spec, "flavor", ctx)) flavor = parse_choice(*f, kFlavors, "flavor", ctx);
  for (const auto& p : cfg.providers) {
    if ((!kind || p.kind == *kind) && (!flavor || p.flavor == *flavor)) return p.id;
  }
  ctx.fail("needs a matching provider in providers.toml", &spec);
}

// One slot's entry in a preset (or its `all`), read before any provider is matched, so that a misspelt key is named
// first.
struct PresetEntry {
  const toml::table* table = nullptr;
  Ctx ctx;
  std::optional<std::string> model;
  std::optional<std::string> fallback;
  std::optional<std::string> provider;           // a name of the preset's providers (O88)
  std::optional<std::string> fallback_provider;  // the same, for the fallback: by default the slot's own
  bool hints = false;                            // model hints only, which the local probe resolves (later work)
};

PresetEntry read_preset_entry(const toml::table& table, const Ctx& ctx) {
  check_keys(table,
             {"model", "fallback", "provider", "fallback_provider", "model_hint", "quant", "background_at_rest_only"},
             ctx);
  PresetEntry e;
  e.table = &table;
  e.ctx = ctx;
  e.model = get_string(table, "model", ctx);
  e.fallback = get_string(table, "fallback", ctx);
  e.provider = get_string(table, "provider", ctx);
  e.fallback_provider = get_string(table, "fallback_provider", ctx);
  get_string(table, "quant", ctx);
  get_bool(table, "background_at_rest_only", ctx);
  if (const toml::node* hint = table.get("model_hint")) {
    if (!hint->is_array()) ctx.fail("model_hint must be a list", hint);
    e.hints = true;
  }
  if (e.fallback_provider && !e.fallback) {
    ctx.fail("names a fallback_provider but no fallback", table.get("fallback_provider"));
  }
  return e;
}

// What a preset gives the slots, which providers.toml's [slots] overrides default to.
struct PresetResult {
  std::optional<std::string> sole;                          // the preset's one provider, where it has one
  std::map<std::string, std::string, std::less<>> by_slot;  // the provider it gives each slot it places
};

// Fills the slots (and the reserved slots' models) from the preset. Its providers come in one of two forms (O88): one
// `provider = { ... }`, or named [preset.<id>.providers.<name>] tables, each slot naming its own; never both, never
// neither.
PresetResult apply_preset(Config& cfg, const std::string& models_text, const std::string& preset_id) {
  const Ctx ctx{"models.toml", ""};
  const toml::table root = parse_toml(ctx.file, models_text, false);
  check_keys(root, {"schema", "preset"}, ctx);
  check_schema(root, kModelsSchema, ctx, /*required=*/true);
  const toml::table* presets = get_table(root, "preset", ctx);
  const toml::node* node = presets != nullptr ? presets->get(preset_id) : nullptr;
  if (node == nullptr || !node->is_table()) ctx.fail("has no preset \"" + preset_id + "\"");
  const toml::table& preset = *node->as_table();
  const Ctx pctx = ctx.sub("preset." + preset_id);
  check_keys(preset,
             {"provider", "providers", "all", "warn_free_tier_for", kDialogue, kBarkLive, kBarkPool, kMemory,
              kSelftest, kDraft, kDecisions, kActions, kVision},
             pctx);
  if (const toml::node* warn = preset.get("warn_free_tier_for"); warn != nullptr && !warn->is_array()) {
    pctx.fail("warn_free_tier_for must be a list", warn);  // the wizard's concern; only checked here
  }

  std::optional<PresetEntry> all;
  if (const toml::table* table = get_table(preset, "all", pctx)) {
    all = read_preset_entry(*table, pctx.sub(pctx.where + ".all"));
  }
  std::map<std::string, PresetEntry, std::less<>> own;
  auto read_own = [&](std::string_view name) {
    const std::string slot(name);
    if (const toml::table* table = get_table(preset, slot, pctx)) {
      own.emplace(slot, read_preset_entry(*table, pctx.sub(pctx.where + "." + slot)));
    }
  };
  for (const std::string_view name : known_slots()) read_own(name);
  for (const std::string_view name : reserved_slots()) read_own(name);

  const toml::node* one = preset.get("provider");
  const toml::node* named = preset.get("providers");
  if (one != nullptr && named != nullptr) pctx.fail("has both provider = { ... } and providers: use one form", one);
  if (one == nullptr && named == nullptr) {
    pctx.fail("names no provider: give it provider = { ... }, or [preset." + preset_id + ".providers.<name>] tables",
              &preset);
  }
  PresetResult result;
  std::map<std::string, std::string, std::less<>> by_name;  // a name of the preset's: the providers.toml id it matched
  if (one != nullptr) {
    if (!one->is_table()) pctx.fail("\"provider\" must be a table", one);
    result.sole = match_provider(*one->as_table(), cfg, pctx.sub(pctx.where + ".provider"));
  } else {
    if (!named->is_table()) {
      pctx.fail("\"providers\" must be [preset." + preset_id + ".providers.<name>] tables", named);
    }
    for (auto&& [key, value] : *named->as_table()) {
      const std::string name(key.str());
      const Ctx nctx = pctx.sub(pctx.where + ".providers." + name);
      if (!valid_id(name)) nctx.fail("a provider's name must be 1 to 64 letters, digits, '_', '-' or '.'", &value);
      if (!value.is_table()) nctx.fail("must be a table", &value);
      const std::string id = match_provider(*value.as_table(), cfg, nctx);
      // One name, one provider. TOML itself refuses a name given twice in a preset; a name that is the id of another
      // provider in providers.toml would stand for two as well.
      if (id != name && cfg.provider(name) != nullptr) {
        nctx.fail("matches providers.toml's \"" + id + "\", but providers.toml has a provider \"" + name +
                      "\" too: one name for two providers",
                  &value);
      }
      by_name.emplace(name, id);
    }
    if (by_name.empty()) pctx.fail("names no provider in its providers table", named);
    if (by_name.size() == 1) result.sole = by_name.begin()->second;
  }

  // The providers.toml id a name of the preset's stands for.
  auto named_provider = [&](const PresetEntry& e, std::string_view key, const std::string& name) -> std::string {
    if (const auto it = by_name.find(name); it != by_name.end()) return it->second;
    e.ctx.fail(std::string(key) + " \"" + name + "\" is not a provider the preset defines" +
                   (one != nullptr ? " (its one provider = { ... } has no name)" : ""),
               e.table->get(key));
  };
  auto primary_provider = [&](const PresetEntry& e) -> std::string {
    if (e.provider) return named_provider(e, "provider", *e.provider);
    if (result.sole) return *result.sole;
    e.ctx.fail("needs provider = \"<name>\": the preset names several providers", e.table);
  };
  // Every name is checked, whether or not its entry ends up used.
  auto check_names = [&](const PresetEntry& e) {
    if (e.provider) named_provider(e, "provider", *e.provider);
    if (e.fallback_provider) named_provider(e, "fallback_provider", *e.fallback_provider);
  };
  if (all) check_names(*all);
  for (const auto& [slot, e] : own) check_names(e);

  auto fill = [&](const std::string& slot, const PresetEntry& e, std::vector<ModelRef>& models, bool warn) {
    const bool has_model = e.model && !e.model->empty();
    if (!has_model && !e.hints && !e.provider) return;  // nothing to place
    const std::string id = primary_provider(e);
    result.by_slot[slot] = id;
    models.clear();
    if (has_model) {
      models.push_back(ModelRef{id, *e.model});
      if (e.fallback && !e.fallback->empty()) {
        const std::string fallback_id =
            e.fallback_provider ? named_provider(e, "fallback_provider", *e.fallback_provider) : id;
        models.push_back(ModelRef{fallback_id, *e.fallback});
      }
    } else if (e.hints && warn) {
      cfg.warnings.push_back("preset " + preset_id + ": slot " + slot +
                             " names model hints only; the local probe resolves them (later work), or name a model in "
                             "providers.toml [slots." + slot + "]");
    }
  };
  for (const std::string_view name : known_slots()) {
    const std::string slot(name);
    const auto it = own.find(slot);
    const PresetEntry* e = it != own.end() ? &it->second : (all ? &*all : nullptr);
    if (e != nullptr) fill(slot, *e, cfg.slots[slot].models, /*warn=*/true);
  }
  // A reserved slot takes its own entry only: `all` is for the slots in use, which it is not yet.
  for (const std::string_view name : reserved_slots()) {
    const std::string slot(name);
    if (const auto it = own.find(slot); it != own.end()) fill(slot, it->second, cfg.reserved[slot], /*warn=*/false);
  }
  return result;
}

// The provider a [slots] override defaults to (ARCH §11.1): the one the preset gives the slot, or gives the slot it
// would borrow a model from (decisions bark_live's, every other slot dialogue's), or the preset's one provider.
std::optional<std::string> preset_default(const PresetResult& preset, std::string_view slot) {
  auto given = [&](std::string_view s) -> std::optional<std::string> {
    if (const auto it = preset.by_slot.find(s); it != preset.by_slot.end()) return it->second;
    return std::nullopt;
  };
  if (auto id = given(slot)) return id;
  if (slot == kDecisions) {
    if (auto id = given(kBarkLive)) return id;
  }
  if (auto id = given(kDialogue)) return id;
  return preset.sole;
}

void apply_slot_overrides(Config& cfg, const toml::table& slots, const PresetResult& preset, const Ctx& file_ctx) {
  for (auto&& [name, value] : slots) {
    const std::string slot(name.str());
    const Ctx ctx = file_ctx.sub("slots." + slot);
    const bool reserved = is_reserved_slot(slot);
    if (!reserved && cfg.slots.find(slot) == cfg.slots.end()) {
      ctx.fail("is not a slot (" + std::string(kDialogue) + ", ...)", &value);
    }
    if (!value.is_table()) ctx.fail("must be a table", &value);
    const toml::table& table = *value.as_table();
    check_keys(table, {"model", "fallback", "provider", "fallback_provider"}, ctx);
    const std::optional<std::string> preset_provider = preset_default(preset, slot);

    auto provider_for = [&](std::string_view key, const std::optional<std::string>& fallback_to) -> std::string {
      if (auto id = get_string(table, key, ctx)) {
        if (cfg.provider(*id) == nullptr) ctx.fail("names no provider \"" + *id + "\"", table.get(key));
        return *id;
      }
      if (fallback_to) return *fallback_to;
      if (cfg.providers.size() == 1) return cfg.providers.front().id;
      ctx.fail("needs " + std::string(key) + " = \"<id>\": providers.toml has several providers");
    };

    auto& models = reserved ? cfg.reserved[slot] : cfg.slots[slot].models;
    std::optional<ModelRef> primary = models.empty() ? std::nullopt : std::optional<ModelRef>(models[0]);
    std::optional<ModelRef> fallback = models.size() < 2 ? std::nullopt : std::optional<ModelRef>(models[1]);
    if (auto model = get_string(table, "model", ctx)) {
      primary = ModelRef{provider_for("provider", preset_provider), *model};
    } else if (table.get("provider") != nullptr) {
      ctx.fail("names a provider but no model");
    }
    if (auto model = get_string(table, "fallback", ctx)) {
      const std::optional<std::string> base = primary ? std::optional<std::string>(primary->provider) : preset_provider;
      fallback = ModelRef{provider_for("fallback_provider", base), *model};
    } else if (table.get("fallback_provider") != nullptr) {
      ctx.fail("names a fallback_provider but no fallback");
    }
    if (!primary && fallback) ctx.fail("has a fallback but no model");
    models.clear();
    if (primary) models.push_back(*primary);
    if (fallback) models.push_back(*fallback);
  }
}

// A slot with no model of its own borrows one (ARCH §11.4): draft and the rest use dialogue's, decisions bark_live's.
void fill_empty_slots(Config& cfg) {
  const auto& dialogue = cfg.slots[std::string(kDialogue)].models;
  auto& bark_live = cfg.slots[std::string(kBarkLive)].models;
  if (bark_live.empty()) {
    bark_live = dialogue;
    cfg.borrowed[std::string(kBarkLive)] = std::string(kDialogue);
  }
  for (auto& [name, slot] : cfg.slots) {
    if (!slot.models.empty() || name == kDialogue) continue;
    slot.models = name == kDecisions ? bark_live : dialogue;
    cfg.borrowed[name] = std::string(name == kDecisions ? kBarkLive : kDialogue);
  }
}

void parse_settings(Config& cfg, const std::string& text) {
  const Ctx ctx{"settings.toml", ""};
  const toml::table root = parse_toml(ctx.file, text, false);
  check_schema(root, kSettingsSchema, ctx, /*required=*/false);
  const toml::table* budget = get_table(root, "budget", ctx);
  if (budget == nullptr) return;
  const Ctx bctx = ctx.sub("budget");
  check_keys(*budget, {"requests_per_hour"}, bctx);
  if (auto per_hour = get_int(*budget, "requests_per_hour", bctx)) {
    if (*per_hour < 60 || *per_hour > 3600) bctx.fail("requests_per_hour must be 60 to 3600");
    cfg.requests_per_hour = static_cast<int>(*per_hour);
  }
}


}  // namespace

const char* to_string(Flavor f) {
  for (const auto& [name, e] : kFlavors) {
    if (e == f) return name.data();
  }
  return "openai";
}

const char* to_string(ProviderKind k) { return k == ProviderKind::Cloud ? "cloud" : "local"; }

SlotBudget default_budget(std::string_view slot) {
  using std::chrono::milliseconds;
  // max_tokens, temperature, first token, first token on a local provider, total, retries, concurrency.
  // ARCH §11.4's table; concurrency is ours (impl): one reply in flight (ARCH §1.3, C9), two call-outs or decisions.
  if (slot == kBarkLive) return {32, 0.9, milliseconds(1500), milliseconds(1500), milliseconds(3000), 0, 2};
  if (slot == kBarkPool) return {600, 1.0, milliseconds(15000), milliseconds(15000), milliseconds(60000), 1, 1};
  if (slot == kMemory) return {500, 0.3, milliseconds(20000), milliseconds(20000), milliseconds(60000), 2, 1};
  if (slot == kSelftest) return {32, 0.0, milliseconds(10000), milliseconds(10000), milliseconds(20000), 0, 1};
  if (slot == kDraft) return {500, 0.7, milliseconds(20000), milliseconds(20000), milliseconds(60000), 0, 1};
  if (slot == kDecisions) return {100, 0.0, milliseconds(1500), milliseconds(3000), milliseconds(5000), 0, 2};
  return {140, 0.8, milliseconds(6000), milliseconds(10000), milliseconds(20000), 0, 1};  // dialogue
}

const ProviderConfig* Config::provider(std::string_view id) const {
  for (const auto& p : providers) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

const SlotConfig& Config::slot(std::string_view name) const {
  auto it = slots.find(name);
  if (it == slots.end()) it = slots.find(kDialogue);
  if (it == slots.end()) throw std::logic_error("a configuration without the dialogue slot");
  return it->second;
}

#ifdef _MSC_VER
namespace {
struct CrtFree {
  void operator()(char* p) const noexcept { std::free(p); }
};
}  // namespace
#endif

std::optional<std::string> system_env(const std::string& name) {
#ifdef _MSC_VER
  // MSVC's CRT marks getenv unsafe, as it is not thread-safe there; _dupenv_s is its thread-safe form, and hands back a
  // copy for us to free.
  char* value = nullptr;
  std::size_t size = 0;
  if (_dupenv_s(&value, &size, name.c_str()) != 0 || value == nullptr) return std::nullopt;
  const std::unique_ptr<char, CrtFree> copy(value);
  return std::string(copy.get());
#else
  const char* value = std::getenv(name.c_str());
  if (value == nullptr) return std::nullopt;
  return std::string(value);
#endif
}

std::vector<std::string> quantizations_from(std::string_view floor) {
  std::vector<std::string> levels;
  bool reached = false;
  for (std::string_view level : kQuantLevels) {
    reached = reached || level == floor;
    if (reached) levels.emplace_back(level);
  }
  return levels;
}

Config parse_config(const ConfigTexts& texts, const EnvLookup& env) {
  Config cfg;
  for (const std::string_view slot : known_slots()) {
    cfg.slots[std::string(slot)] = SlotConfig{std::string(slot), default_budget(slot), {}};
  }
  for (const std::string_view slot : reserved_slots()) cfg.reserved[std::string(slot)] = {};
  const Keys keys = texts.secrets ? parse_secrets(*texts.secrets) : Keys{};
  if (texts.settings) parse_settings(cfg, *texts.settings);

  const Ctx ctx{"providers.toml", ""};
  const toml::table root = parse_toml(ctx.file, texts.providers, false);
  check_keys(root, {"schema", "preset", "provider", "slots"}, ctx);
  check_schema(root, kProvidersSchema, ctx, /*required=*/true);
  cfg.preset = get_string(root, "preset", ctx).value_or("custom");

  if (const toml::node* list = root.get("provider")) {
    if (!list->is_array_of_tables()) ctx.fail("provider must be [[provider]] tables", list);
    std::size_t index = 0;
    for (const auto& entry : *list->as_array()) {
      ProviderConfig p = parse_provider(*entry.as_table(), index++, keys, env, ctx);
      if (cfg.provider(p.id) != nullptr) ctx.fail("two providers have the id \"" + p.id + "\"", &entry);
      cfg.providers.push_back(std::move(p));
    }
  }
  if (cfg.providers.empty()) ctx.fail("names no [[provider]]");

  PresetResult preset;
  if (cfg.preset != "custom") {
    if (!texts.models) throw ConfigError("models.toml", "is needed for preset \"" + cfg.preset + "\"");
    preset = apply_preset(cfg, *texts.models, cfg.preset);
  }
  if (const toml::table* slots = get_table(root, "slots", ctx)) apply_slot_overrides(cfg, *slots, preset, ctx);
  fill_empty_slots(cfg);
  if (cfg.slots[std::string(kDialogue)].models.empty()) {
    cfg.warnings.push_back("no slot has a model: name one in providers.toml [slots.dialogue] or choose a preset");
  }
  for (const auto& p : cfg.providers) {
    for (const auto& ref : p.unresolved_refs) {
      cfg.warnings.push_back("provider " + p.id + ": " + ref + " names no key, so the provider is not used");
    }
  }
  return cfg;
}


}  // namespace en::llm
