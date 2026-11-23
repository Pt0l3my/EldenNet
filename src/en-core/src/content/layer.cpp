// The player's own layer over a shipped persona: read, checked, written and merged (profile.hpp; NPC §4.5, §4.7, §4.8;
// S7.2.b). Every finding is in our own words and names a field and an item, never a value of the file.
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>

#include <toml.hpp>

#include "en/core/dialogue_prompt.hpp"
#include "en/core/profile.hpp"
#include "en/core/speech_content.hpp"
#include "en/core/speech_lint.hpp"
#include "schema.hpp"
#include "wording.hpp"

namespace en::core {

namespace {

constexpr std::array<std::string_view, 5> kRegisterNames = {"archaic-regal", "formal", "colloquial", "broken", "chant"};
constexpr std::string_view kLayerPrefix = "layers/persona-";
constexpr std::string_view kTomlSuffix = ".toml";

bool is_hex16(std::string_view s) {
  return s.size() == 16 && std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

// One value written as a TOML basic string: quotes, backslashes and control characters escaped, a line break or a tab
// a space (a layer's text is one paragraph).
std::string toml_string(std::string_view s) {
  std::string out = "\"";
  for (const char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n':
      case '\r':
      case '\t': out += ' '; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04X", static_cast<unsigned>(static_cast<unsigned char>(c)));
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out + "\"";
}

class Findings {
 public:
  Findings(std::string file, std::vector<LayerFinding>& out) : file_(std::move(file)), out_(out) {}
  void add(std::string_view field, int item, bool error, LayerProblem problem, std::uint32_t limit = 0,
           std::uint32_t value = 0) {
    errors_ = errors_ || error;
    out_.push_back(LayerFinding{file_, std::string(field), item, error, problem, limit, value});
  }
  [[nodiscard]] bool errors() const noexcept { return errors_; }

 private:
  std::string file_;
  std::vector<LayerFinding>& out_;
  bool errors_ = false;
};

// PL1: a text field's caps.
void text_caps(Findings& f, std::string_view field, int item, std::string_view s, std::size_t words, std::size_t chars) {
  if (s.empty()) {
    f.add(field, item, true, LayerProblem::Empty);
    return;
  }
  if (schema::words_of(s) > words) f.add(field, item, true, LayerProblem::TooManyWords, static_cast<std::uint32_t>(words));
  if (schema::chars_of(s) > chars) f.add(field, item, true, LayerProblem::TooManyChars, static_cast<std::uint32_t>(chars));
}

// The wording of the player's text (PL4, PL5): errors for template marks and the safety list, warnings for the rest.
void wording_of(Findings& f, const Lexicon& lexicon, std::string_view field, int item, std::string_view text, bool subject) {
  if (wording::has_template_mark(text)) f.add(field, item, true, LayerProblem::Template);
  if (wording::safety_hit(lexicon, text)) f.add(field, item, true, LayerProblem::Safety);
  if (wording::banned(lexicon, text)) f.add(field, item, false, LayerProblem::Banned);
  if (subject) {
    if (!wording::noun_phrase(text)) f.add(field, item, false, LayerProblem::NounPhrase);
    return;
  }
  if (wording::has_quote(text)) f.add(field, item, false, LayerProblem::Quote);
  if (wording::has_effect_word(text)) f.add(field, item, false, LayerProblem::Effect);
}

std::string field_place(const LayerFinding& f) {
  if (f.field.empty()) return "(file)";
  return f.item < 0 ? f.field : f.field + "[" + std::to_string(f.item) + "]";
}

}  // namespace

std::span<const std::string_view> register_names() noexcept { return kRegisterNames; }

std::string layer_path(std::string_view persona_id) {
  return std::string(kLayerPrefix) + std::string(persona_id) + std::string(kTomlSuffix);
}

std::optional<std::string> layer_persona(std::string_view path) {
  if (path.size() <= kLayerPrefix.size() + kTomlSuffix.size() || path.substr(0, kLayerPrefix.size()) != kLayerPrefix ||
      path.substr(path.size() - kTomlSuffix.size()) != kTomlSuffix) {
    return std::nullopt;
  }
  const std::string_view id = path.substr(kLayerPrefix.size(), path.size() - kLayerPrefix.size() - kTomlSuffix.size());
  if (!schema::is_slug(id, 32)) return std::nullopt;
  return std::string(id);
}

std::string record_hash(std::string_view text) {
  std::uint64_t h = 0xcbf29ce484222325ull;
  for (const char c : text) {
    h ^= static_cast<unsigned char>(c);
    h *= 0x100000001b3ull;
  }
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h));
  return buf;
}

std::string words(const LayerFinding& f) {
  const std::string n = std::to_string(f.limit);
  switch (f.problem) {
    case LayerProblem::NotRead: return "This file could not be read, so it was not loaded.";
    case LayerProblem::NewerSchema: return "This file was written by a newer EldenNet, so it is shown here read-only.";
    case LayerProblem::BadSchema: return "This file does not say which version of the format it follows.";
    case LayerProblem::Target: return "This file names no profile of EldenNet's, so it was not loaded.";
    case LayerProblem::UnknownKey:
      return "This file sets something a layer may not: only your notes, the subjects, the way of speaking and the "
             "longest reply.";
    case LayerProblem::WrongType: return "This value is of the wrong kind.";
    case LayerProblem::TooManyWords: return "Too long: at most " + n + " words.";
    case LayerProblem::TooManyChars: return "Too long: at most " + n + " characters.";
    case LayerProblem::TooManyItems: return "Too many: at most " + n + ".";
    case LayerProblem::OutOfRange:
      return "Must be a whole number from " + std::to_string(kLayerReplyWordsMin) + " to " +
             std::to_string(kLayerReplyWordsMax) + ".";
    case LayerProblem::NotAChoice: return "Not one of the ways of speaking EldenNet knows.";
    case LayerProblem::Empty: return "Empty: write something here, or leave it to the shipped profile.";
    case LayerProblem::Template: return "Holds {{, {% or {#, which EldenNet's prompts would read as their own: take it out.";
    case LayerProblem::Safety: return "Holds a word EldenNet never sends to a model: take it out.";
    case LayerProblem::Banned:
      return "Holds a word EldenNet keeps out of its characters' speech, such as a game term or a modern phrase: the "
             "model may not follow it.";
    case LayerProblem::Quote:
      return "Holds a quoted line, which a model tends to repeat word for word: describe the manner instead.";
    case LayerProblem::Effect:
      return "Names an effect, such as mysterious or gruff: say what they do, and the effect follows.";
    case LayerProblem::NounPhrase: return "Write a subject as a short phrase: not a sentence, a question or an instruction.";
    case LayerProblem::Block4:
      return "With your notes, the voice part of their prompt comes to about " + std::to_string(f.value) +
             " tokens, past its " + n + ": keep your notes short.";
    case LayerProblem::ContentDown: return "EldenNet's content does not load just now, so your notes cannot be checked.";
  }
  return "This file could not be read.";
}

ContentError to_content_error(const LayerFinding& f) {
  return ContentError{f.file, 0, field_place(f) + ": " + (f.error ? "refused: " : "") + words(f)};
}

std::optional<PersonaLayer> read_layer(const ContentFile& file, std::vector<LayerFinding>& findings) {
  Findings f(file.path, findings);
  const std::optional<std::string> persona = layer_persona(file.path);
  if (!persona) {
    f.add("", -1, true, LayerProblem::Target);
    return std::nullopt;
  }
  if (file.text.size() > kLayerFileMaxBytes) {
    f.add("", -1, true, LayerProblem::NotRead, static_cast<std::uint32_t>(kLayerFileMaxBytes >> 10));
    return std::nullopt;
  }
  if (proto::unsafe_for_toml(file.text, proto::TomlText::Player)) {
    f.add("", -1, true, LayerProblem::NotRead);
    return std::nullopt;
  }
  toml::parse_result parsed = toml::parse(file.text);
  if (!parsed) {
    f.add("", -1, true, LayerProblem::NotRead);
    return std::nullopt;
  }
  const toml::table& t = parsed.table();
  PersonaLayer layer;
  layer.file = file.path;
  layer.persona = *persona;
  // The file's own keys: its version, target, revision, the shipped record's hash and its provenance.
  const auto s = t["schema"].value<std::int64_t>();
  if (!s || *s < kLayerSchema) {
    f.add("", -1, true, LayerProblem::BadSchema);
  } else if (*s > kLayerSchema) {
    f.add("", -1, true, LayerProblem::NewerSchema);
  }
  if (t["target"].value<std::string>() != "persona:" + *persona) f.add("", -1, true, LayerProblem::Target);
  if (const toml::node* n = t.get("rev")) {
    const auto rev = n->value<std::int64_t>();
    if (!n->is_integer() || !rev || *rev < 0 || *rev > std::numeric_limits<std::uint32_t>::max()) {
      f.add("", -1, true, LayerProblem::WrongType);
    } else {
      layer.rev = static_cast<std::uint32_t>(*rev);
    }
  }
  if (const toml::node* n = t.get("base_hash")) {
    if (const auto* h = n->as_string(); h != nullptr && is_hex16(h->get())) {
      layer.base_hash = h->get();
    } else {
      f.add("", -1, true, LayerProblem::WrongType);
    }
  }
  if (const toml::node* n = t.get("drafted")) {
    const auto d = n->value<std::string>();
    if (!n->is_string() || (d != "hand" && d != "assisted")) {
      f.add("", -1, true, LayerProblem::WrongType);
    } else {
      layer.assisted = d == "assisted";
    }
  }
  // The fields a layer may set (NPC §4.5), each held to NPC §3.1's caps.
  for (auto&& [key, node] : t) {
    const std::string_view k = key.str();
    if (k == "schema" || k == "target" || k == "rev" || k == "base_hash" || k == "drafted") continue;
    if (std::find(kLayerFields.begin(), kLayerFields.end(), k) == kLayerFields.end()) {
      f.add("", -1, true, LayerProblem::UnknownKey);
      continue;
    }
    if (k == "instructions") {
      if (const auto* v = node.as_string()) {
        text_caps(f, k, -1, v->get(), kLayerInstructionWords, kLayerInstructionChars);
        layer.fields["instructions"] = v->get();
      } else {
        f.add(k, -1, true, LayerProblem::WrongType);
      }
    } else if (k == "subjects") {
      const auto* a = node.as_array();
      if (a == nullptr) {
        f.add(k, -1, true, LayerProblem::WrongType);
        continue;
      }
      if (a->size() > kLayerSubjectsMax) {
        f.add(k, -1, true, LayerProblem::TooManyItems, static_cast<std::uint32_t>(kLayerSubjectsMax));
      }
      nlohmann::json items = nlohmann::json::array();
      for (std::size_t i = 0; i < a->size(); ++i) {
        const int item = static_cast<int>(i);
        if (const auto* v = a->get(i)->as_string()) {
          text_caps(f, k, item, v->get(), kLayerSubjectWords, kLayerSubjectChars);
          items.push_back(v->get());
        } else {
          f.add(k, item, true, LayerProblem::WrongType);
        }
      }
      layer.fields["subjects"] = std::move(items);
    } else if (k == "register") {
      const auto v = node.value<std::string>();
      if (!node.is_string() || !v ||
          std::find(kRegisterNames.begin(), kRegisterNames.end(), *v) == kRegisterNames.end()) {
        f.add(k, -1, true, node.is_string() ? LayerProblem::NotAChoice : LayerProblem::WrongType);
      } else {
        layer.fields["register"] = *v;
      }
    } else {  // reply_words_max
      const auto v = node.value<std::int64_t>();
      if (!node.is_integer() || !v) {
        f.add(k, -1, true, LayerProblem::WrongType);
      } else if (*v < kLayerReplyWordsMin || *v > kLayerReplyWordsMax) {
        f.add(k, -1, true, LayerProblem::OutOfRange);
      } else {
        layer.fields["reply_words_max"] = *v;
      }
    }
  }
  if (f.errors()) return std::nullopt;
  return layer;
}

std::string layer_text(const PersonaLayer& layer) {
  std::string out;
  out += "# Your own layer over EldenNet's profile \"" + layer.persona +
         "\", written by EldenNet's profile editor (NPC §4.7). EldenNet's own file is never changed.\n";
  out += "schema = " + std::to_string(kLayerSchema) + "\n";
  out += "target = " + toml_string("persona:" + layer.persona) + "\n";
  out += "rev = " + std::to_string(layer.rev) + "\n";
  if (!layer.base_hash.empty()) out += "base_hash = " + toml_string(layer.base_hash) + "\n";
  if (layer.assisted) out += "drafted = \"assisted\"\n";
  for (const std::string_view key : kLayerFields) {
    const auto it = layer.fields.find(std::string(key));
    if (it == layer.fields.end()) continue;
    out += std::string(key) + " = ";
    if (it->is_string()) {
      out += toml_string(it->get<std::string>());
    } else if (it->is_number_integer()) {
      out += std::to_string(it->get<std::int64_t>());
    } else if (it->is_array()) {
      out += "[";
      bool first = true;
      for (const auto& v : *it) {
        if (!v.is_string()) continue;
        out += (first ? "" : ", ") + toml_string(v.get<std::string>());
        first = false;
      }
      out += "]";
    } else {
      out += "\"\"";  // never: the editor's glue writes the fields' own kinds
    }
    out += "\n";
  }
  return out;
}

void apply_layers(ContentPack& pack, const DialoguePrompt& prompt, const Lexicon& lexicon,
                  std::vector<LayerFinding>& findings) {
  constexpr std::size_t kVoice = static_cast<std::size_t>(Block::Voice);
  for (const PersonaLayer& layer : pack.layers) {
    Findings f(layer.file, findings);
    const auto persona = std::find_if(pack.personas.begin(), pack.personas.end(),
                                      [&](const Persona& p) { return p.id == layer.persona; });
    if (persona == pack.personas.end()) {
      f.add("", -1, true, LayerProblem::Target);  // a persona the index lacks, or one turned off: never merged
      continue;
    }
    if (const auto it = layer.fields.find("instructions"); it != layer.fields.end() && it->is_string()) {
      wording_of(f, lexicon, "instructions", -1, it->get<std::string>(), false);
    }
    if (const auto it = layer.fields.find("subjects"); it != layer.fields.end() && it->is_array()) {
      for (std::size_t i = 0; i < it->size(); ++i) {
        if ((*it)[i].is_string()) wording_of(f, lexicon, "subjects", static_cast<int>(i), (*it)[i].get<std::string>(), true);
      }
    }
    if (f.errors()) continue;
    Persona merged = *persona;
    merged.layer = layer;
    if (const auto it = layer.fields.find("register"); it != layer.fields.end() && it->is_string()) {
      merged.reg = schema::enum_of<Register>(it->get<std::string>(), kRegisterNames, merged.reg);
    }
    if (const auto it = layer.fields.find("reply_words_max"); it != layer.fields.end() && it->is_number_integer()) {
      merged.reply_words_max = static_cast<std::uint8_t>(it->get<std::int64_t>());
    }
    // Block 4 with the player's instructions and register, measured as the shipped one is (WHY§472: a warning for the
    // player's own, since the shipped voice leaves little room; PL9's headroom is the owner's to settle).
    if (layer.fields.contains("instructions") || layer.fields.contains("register")) {
      const nlohmann::json record = merged.resolved(0);
      DialogueContext c;
      c.persona = merged.id;
      c.record = &record;
      c.name = merged.display_name_fallback;
      if (const auto tokens = prompt.block_tokens(c, Block::Voice); tokens && *tokens > kBlocks[kVoice].cap) {
        f.add("instructions", -1, false, LayerProblem::Block4, kBlocks[kVoice].cap, *tokens);
      }
    }
    *persona = std::move(merged);
  }
}

bool LayerCheck::ok() const noexcept {
  return std::none_of(findings.begin(), findings.end(), [](const LayerFinding& f) { return f.error; });
}

LayerCheck check_layer(std::span<const ContentFile> files, const ContentFile& candidate) {
  LayerCheck out;
  std::vector<ContentFile> all;
  all.reserve(files.size() + 1);
  for (const ContentFile& f : files) {
    if (f.path != candidate.path) all.push_back(f);
  }
  all.push_back(ContentFile{candidate.path, candidate.text, ContentOrigin::Override});
  const auto down = [&] {
    out.findings.push_back(LayerFinding{candidate.path, "", -1, true, LayerProblem::ContentDown, 0, 0});
    return out;
  };
  auto pack = load_content(all);
  if (!pack) return down();
  auto speech = load_speech(*pack);
  if (!speech || !speech->prompt || !speech->lint) return down();
  std::vector<LayerFinding> found = pack->layer_findings;
  apply_layers(*pack, *speech->prompt, speech->lint->lexicon(), found);
  for (LayerFinding& f : found) {
    if (f.file == candidate.path) out.findings.push_back(std::move(f));
  }
  for (const Persona& p : pack->personas) {
    if (p.layer && p.layer->file == candidate.path) out.layer = p.layer;
  }
  return out;
}

}  // namespace en::core
