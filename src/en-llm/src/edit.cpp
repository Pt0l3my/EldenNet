// Changes to providers.toml and secrets.toml from EldenNet's AI pages (edit.hpp).
#include "en/llm/edit.hpp"

#include <algorithm>
#include <set>
#include <sstream>

#include <toml.hpp>

#include "en/proto/toml_check.hpp"

namespace en::llm {

namespace {

constexpr std::size_t kMaxKeyBytes = 1024;

bool valid_name(std::string_view id) {
  if (id.empty() || id.size() > 64) return false;
  return std::all_of(id.begin(), id.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '.';
  });
}

// A player's text as toml++ reads it, once it has passed the engine's own check (en-proto's toml_check.hpp).
std::optional<toml::table> parse_player(std::string_view text, std::string_view file) {
  if (proto::unsafe_for_toml(text, proto::TomlText::Player)) return std::nullopt;
  toml::parse_result parsed = toml::parse(text, file);
  if (!parsed) return std::nullopt;
  return std::move(parsed).table();
}

const char* json_word(JsonMode m) {
  switch (m) {
    case JsonMode::None: return "none";
    case JsonMode::JsonObject: return "json_object";
    case JsonMode::JsonSchema: return "json_schema";
  }
  return "none";
}

std::string normalised(std::string url) {
  while (!url.empty() && (url.back() == '/' || url.back() == ' ')) url.pop_back();
  while (!url.empty() && url.front() == ' ') url.erase(url.begin());
  return url;
}

std::optional<Flavor> flavor_of(std::string_view word) {
  if (word == "openai") return Flavor::OpenAI;
  if (word == "ollama") return Flavor::Ollama;
  if (word == "llamacpp") return Flavor::LlamaCpp;
  if (word == "lmstudio") return Flavor::LmStudio;
  if (word == "koboldcpp") return Flavor::KoboldCpp;
  return std::nullopt;
}

std::string text_of(const toml::table& table) {
  std::ostringstream out;
  out << table << "\n";
  return out.str();
}

}  // namespace

EditedText edit_providers(std::string_view text, const ProvidersEdit& edit) {
  EditedText out;
  toml::table root;
  if (!text.empty()) {
    std::optional<toml::table> parsed = parse_player(text, "providers.toml");
    if (!parsed) {
      out.why = "refused";
      return out;
    }
    root = std::move(*parsed);
  } else {
    root.insert_or_assign("schema", 1);
  }
  if (edit.preset) root.insert_or_assign("preset", *edit.preset);

  toml::node* node = root.get("provider");
  if (node == nullptr && !edit.add.empty()) {
    root.insert_or_assign("provider", toml::array{});
    node = root.get("provider");
  }
  toml::array* list = node != nullptr ? node->as_array() : nullptr;
  if (node != nullptr && list == nullptr) {
    out.why = "not_a_table";
    return out;
  }
  auto id_of = [](const toml::node& n) -> std::string {
    const toml::table* t = n.as_table();
    const toml::node* id = t != nullptr ? t->get("id") : nullptr;
    return id != nullptr && id->is_string() ? id->as_string()->get() : std::string();
  };
  for (const std::string& id : edit.remove) {
    if (list == nullptr) {
      out.why = "unknown_id";
      return out;
    }
    const auto at = std::find_if(list->begin(), list->end(), [&](const toml::node& n) { return id_of(n) == id; });
    if (at == list->end()) {
      out.why = "unknown_id";
      return out;
    }
    list->erase(at);
  }
  for (const ProviderEntry& e : edit.add) {
    if (!valid_name(e.id) || (e.base_url.rfind("https://", 0) != 0 && e.base_url.rfind("http://", 0) != 0) ||
        (!e.key_ref.empty() && e.key_ref.rfind("secrets:", 0) != 0 && e.key_ref.rfind("env:", 0) != 0)) {
      out.why = "bad_entry";
      return out;
    }
    if (std::any_of(list->begin(), list->end(), [&](const toml::node& n) { return id_of(n) == e.id; })) {
      out.why = "duplicate_id";
      return out;
    }
    toml::table t;
    t.insert_or_assign("id", e.id);
    t.insert_or_assign("flavor", std::string(to_string(e.flavor)));
    t.insert_or_assign("kind", std::string(to_string(e.kind)));
    t.insert_or_assign("base_url", normalised(e.base_url));
    if (!e.key_ref.empty()) t.insert_or_assign("key_ref", e.key_ref);
    t.insert_or_assign("json_mode", std::string(json_word(e.json_mode)));
    list->push_back(std::move(t));
  }
  if (list != nullptr && list->empty()) root.erase("provider");

  // A [slots.*] entry that names a removed provider goes with it, so that the file still names only providers it has.
  if (!edit.remove.empty()) {
    if (toml::table* slots = root.get_as<toml::table>("slots")) {
      std::vector<std::string> gone;
      for (auto&& [name, value] : *slots) {
        const toml::table* slot = value.as_table();
        if (slot == nullptr) continue;
        for (const std::string_view key : {std::string_view("provider"), std::string_view("fallback_provider")}) {
          const toml::node* p = slot->get(key);
          if (p != nullptr && p->is_string() &&
              std::find(edit.remove.begin(), edit.remove.end(), p->as_string()->get()) != edit.remove.end()) {
            gone.emplace_back(name.str());
            break;
          }
        }
      }
      for (const std::string& name : gone) slots->erase(name);
      if (slots->empty()) root.erase("slots");
    }
  }
  out.text = text_of(root);
  out.ok = true;
  return out;
}

std::vector<std::string> preset_ids(std::string_view models_text) {
  std::vector<std::string> out;
  const std::optional<toml::table> root = parse_player(models_text, "models.toml");
  if (!root) return out;
  const toml::table* presets = root->get_as<toml::table>("preset");
  if (presets == nullptr) return out;
  for (auto&& [name, value] : *presets) {
    if (value.is_table()) out.emplace_back(name.str());
  }
  return out;
}

std::vector<ProviderEntry> preset_providers(std::string_view models_text, std::string_view preset,
                                            const std::vector<ProviderConfig>& have) {
  const std::optional<toml::table> root = parse_player(models_text, "models.toml");
  if (!root) throw ConfigError("models.toml", "fails its checks");
  const toml::table* presets = root->get_as<toml::table>("preset");
  const toml::table* table = presets != nullptr ? presets->get_as<toml::table>(preset) : nullptr;
  if (table == nullptr) throw ConfigError("models.toml", "has no preset \"" + std::string(preset) + "\"");

  std::vector<std::pair<std::string, const toml::table*>> named;
  if (const toml::table* one = table->get_as<toml::table>("provider")) {
    named.emplace_back(std::string(preset), one);
  } else if (const toml::table* several = table->get_as<toml::table>("providers")) {
    for (auto&& [name, value] : *several) {
      if (const toml::table* spec = value.as_table()) named.emplace_back(std::string(name.str()), spec);
    }
  }
  if (named.empty()) throw ConfigError("models.toml", "preset." + std::string(preset) + " names no provider");

  std::vector<ProviderEntry> out;
  for (const auto& [name, spec] : named) {
    const std::optional<std::string> url = spec->get_as<std::string>("base_url") != nullptr
                                               ? std::optional<std::string>(spec->get_as<std::string>("base_url")->get())
                                               : std::nullopt;
    const std::optional<Flavor> flavor =
        spec->get_as<std::string>("flavor") != nullptr ? flavor_of(spec->get_as<std::string>("flavor")->get())
                                                       : std::nullopt;
    std::optional<ProviderKind> kind;
    if (const toml::value<std::string>* k = spec->get_as<std::string>("kind")) {
      kind = k->get() == "local" ? ProviderKind::Local : ProviderKind::Cloud;
    }
    // Matched as parse_config matches it: by base_url where the preset gives one, else by kind and flavour.
    const bool matched = std::any_of(have.begin(), have.end(), [&](const ProviderConfig& p) {
      if (url) return p.base_url == normalised(*url);
      return (!kind || p.kind == *kind) && (!flavor || p.flavor == *flavor);
    });
    if (matched) continue;
    if (!url) {
      throw ConfigError("models.toml", "preset." + std::string(preset) + "'s provider \"" + name +
                                           "\" has no base_url: the local probe finds it");
    }
    ProviderEntry e;
    e.id = name;
    e.flavor = flavor.value_or(Flavor::OpenAI);
    e.base_url = normalised(*url);
    const bool loopback = e.base_url.find("://127.") != std::string::npos ||
                          e.base_url.find("://localhost") != std::string::npos ||
                          e.base_url.find("://[::1]") != std::string::npos;
    e.kind = kind.value_or(loopback ? ProviderKind::Local : ProviderKind::Cloud);
    if (e.kind == ProviderKind::Cloud) e.key_ref = "secrets:" + name;
    out.push_back(std::move(e));
  }
  return out;
}

std::vector<std::string> unused_providers(const Config& config) {
  std::set<std::string, std::less<>> used;
  for (const auto& [name, slot] : config.slots) {
    for (const ModelRef& m : slot.models) used.insert(m.provider);
  }
  for (const auto& [name, models] : config.reserved) {
    for (const ModelRef& m : models) used.insert(m.provider);
  }
  std::vector<std::string> out;
  for (const ProviderConfig& p : config.providers) {
    if (used.count(p.id) == 0) out.push_back(p.id);
  }
  return out;
}

std::vector<std::string> secret_names(const std::vector<ProviderConfig>& providers) {
  std::vector<std::string> out;
  auto take = [&](const std::string& ref) {
    if (ref.rfind("secrets:", 0) == 0 && ref.size() > 8) out.push_back(ref.substr(8));
  };
  for (const ProviderConfig& p : providers) {
    take(p.key_ref);
    for (const std::string& ref : p.unresolved_refs) take(ref);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

EditedSecrets edit_secrets(std::string_view text, std::string_view name, const Secret* value) {
  EditedSecrets out;
  if (!valid_name(name)) {
    out.why = "bad_name";
    return out;
  }
  toml::table root;
  if (!text.empty()) {
    std::optional<toml::table> parsed = parse_player(text, "secrets.toml");
    if (!parsed) {
      out.why = "refused";
      return out;
    }
    root = std::move(*parsed);
  }
  toml::node* node = root.get("keys");
  if (node == nullptr) {
    root.insert_or_assign("keys", toml::table{});
    node = root.get("keys");
  }
  toml::table* keys = node->as_table();
  if (keys == nullptr) {
    out.why = "not_a_table";
    return out;
  }
  if (value != nullptr) {
    keys->insert_or_assign(name, value->reveal());
  } else {
    keys->erase(name);
  }
  std::ostringstream written;
  written << root << "\n";
  std::string made = written.str();
  out.text = Secret(std::move(made));
  // The stream's own buffer held the keys too: overwrite it before it goes.
  std::string left = written.str();
  std::fill(left.begin(), left.end(), '\0');
  written.str(left);
  std::fill(left.begin(), left.end(), '\0');
  // toml++'s nodes hold the keys as well: overwritten before `root` goes at the end of this function.
  for (auto&& [key, held] : *keys) {
    if (toml::value<std::string>* s = held.as_string()) std::fill(s->get().begin(), s->get().end(), '\0');
  }
  out.ok = true;
  return out;
}

std::optional<Secret> clean_key(std::string_view typed) {
  while (!typed.empty() && (typed.front() == ' ' || typed.front() == '\t' || typed.front() == '\r' ||
                            typed.front() == '\n')) {
    typed.remove_prefix(1);
  }
  while (!typed.empty() && (typed.back() == ' ' || typed.back() == '\t' || typed.back() == '\r' ||
                            typed.back() == '\n')) {
    typed.remove_suffix(1);
  }
  if (typed.empty() || typed.size() > kMaxKeyBytes) return std::nullopt;
  for (const char c : typed) {
    const auto u = static_cast<unsigned char>(c);
    if (u <= 0x20 || u == 0x7F) return std::nullopt;
  }
  return Secret(std::string(typed));
}

}  // namespace en::llm
