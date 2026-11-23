// The content pack's speech: its prompts and lexicon, and block 2's content lint (speech_content.hpp; WHY§388).
#include "en/core/speech_content.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <utility>

#include "wording.hpp"

namespace en::core {

namespace {

constexpr std::size_t kIdentity = static_cast<std::size_t>(Block::Identity);
constexpr std::size_t kVoice = static_cast<std::size_t>(Block::Voice);

const ContentFile* other(const ContentPack& pack, std::string_view path) {
  for (const ContentFile& f : pack.others) {
    if (f.path == path) return &f;
  }
  return nullptr;
}

using wording::has_quote;
using wording::kEffectWords;
using wording::kImperatives;
using wording::matches;

// A record's text at a dotted path: a string, or each string of a list.
std::vector<std::string> texts_at(const nlohmann::json& record, std::string_view path) {
  const nlohmann::json* v = &record;
  for (std::size_t start = 0; v != nullptr && start <= path.size();) {
    const std::size_t dot = std::min(path.find('.', start), path.size());
    const auto it = v->is_object() ? v->find(std::string(path.substr(start, dot - start))) : v->end();
    v = v->is_object() && it != v->end() ? &*it : nullptr;
    start = dot + 1;
  }
  std::vector<std::string> out;
  if (v == nullptr) return out;
  if (v->is_string()) out.push_back(v->get<std::string>());
  if (v->is_array()) {
    for (const auto& x : *v) {
      if (x.is_string()) out.push_back(x.get<std::string>());
      if (x.is_object() && x.contains("text") && x["text"].is_string()) out.push_back(x["text"].get<std::string>());
    }
  }
  return out;
}

}  // namespace

void lint_persona_wording(const ContentPack& pack, const Lexicon& lexicon, std::vector<ContentError>& errors,
                          std::vector<ContentError>& warnings) {
  // PL5's prompt-facing fields, and the stages' texts that render (NPC §3.1, §7.3).
  constexpr std::array<std::string_view, 16> kFacing = {
      "public.summary",   "public.appearance", "public.role",      "private.personality", "private.honesty",
      "private.claims",   "private.seeks",     "private.ties",     "voice.cadence",       "voice.diction",
      "voice.deflection", "voice.tics",        "subjects",         "stirs",               "instructions",
      "address_forms"};
  constexpr std::array<std::string_view, 6> kStageFacing = {"identity", "where", "mood", "gist", "road", "unlock.text"};
  constexpr std::array<std::string_view, 5> kNoQuotes = {"voice.cadence", "voice.diction", "voice.deflection",
                                                         "private.claims", "instructions"};
  constexpr std::array<std::string_view, 7> kEffectFields = {"voice.cadence",      "voice.diction",  "voice.deflection",
                                                             "public.summary",     "public.appearance",
                                                             "private.personality", "instructions"};
  for (const Persona& p : pack.personas) {
    const PersonaRow* row = pack.row(p.id);
    const bool strict = p.origin == ContentOrigin::Shipped || (row != nullptr && row->test_only);
    auto& hard = strict ? errors : warnings;
    const auto banned = [&](const std::string& text) {
      const std::vector<std::string> words = lint_words(text);
      return std::any_of(lexicon.banned.begin(), lexicon.banned.end(), [&](const LexiconEntry& e) { return matches(e, words); });
    };
    for (const std::string_view field : kFacing) {
      for (const std::string& t : texts_at(p.record, field)) {
        if (banned(t)) hard.push_back(ContentError{p.file, 0, std::string(field) + ": holds a banned word (PL5)"});
      }
    }
    if (const auto rs = p.record.find("stages"); rs != p.record.end() && rs->is_array()) {
      for (std::size_t i = 0; i < rs->size(); ++i) {
        for (const std::string_view field : kStageFacing) {
          for (const std::string& t : texts_at((*rs)[i], field)) {
            if (banned(t)) {
              hard.push_back(ContentError{p.file, i < p.stages.size() ? p.stages[i].line : 0,
                                          "stages[" + std::to_string(i) + "]." + std::string(field) +
                                              ": holds a banned word (PL5)"});
            }
          }
        }
      }
    }
    for (const std::string_view field : kNoQuotes) {
      for (const std::string& t : texts_at(p.record, field)) {
        if (has_quote(t)) hard.push_back(ContentError{p.file, 0, std::string(field) + ": a quoted sample line (PL5; NPC §3.3)"});
      }
    }
    for (const std::string_view field : kEffectFields) {
      for (const std::string& t : texts_at(p.record, field)) {
        const std::vector<std::string> words = lint_words(t);
        if (std::any_of(words.begin(), words.end(), [](const std::string& w) {
              return std::find(kEffectWords.begin(), kEffectWords.end(), w) != kEffectWords.end();
            })) {
          warnings.push_back(ContentError{p.file, 0, std::string(field) + ": an effect adjective (PL5; NPC §3.3)"});
        }
      }
    }
    // PL4: subjects and stirs are noun phrases: no sentence's end, no question, no quotation, no imperative.
    for (const std::string_view field : {std::string_view("subjects"), std::string_view("stirs")}) {
      for (const std::string& t : texts_at(p.record, field)) {
        const char last = t.empty() ? ' ' : t.back();
        const std::vector<std::string> words = lint_words(t);
        const bool imperative = std::any_of(words.begin(), words.end(), [](const std::string& w) {
          return std::find(kImperatives.begin(), kImperatives.end(), w) != kImperatives.end();
        });
        if (last == '.' || last == '!' || t.find('?') != std::string::npos || has_quote(t) || imperative) {
          hard.push_back(ContentError{p.file, 0, std::string(field) + ": must be a noun phrase (PL4)"});
        }
      }
    }
  }
}

void lint_identity_blocks(const ContentPack& pack, const DialoguePrompt& prompt, std::vector<ContentError>& errors,
                          std::vector<ContentError>& warnings) {
  const std::uint16_t cap = kBlocks[kIdentity].cap;
  for (const Persona& p : pack.personas) {
    const PersonaRow* row = pack.row(p.id);
    // Shipped content and test personas are held to the cap; the player's own records are warned of.
    const bool strict = p.origin == ContentOrigin::Shipped || (row != nullptr && row->test_only);
    for (std::size_t i = 0; i < p.stages.size(); ++i) {
      // The shipped record alone: a layer of the player's is measured with its own words by apply_layers (profile.hpp).
      const nlohmann::json record = p.resolved(i, false);
      DialogueContext c;
      c.persona = p.id;
      c.record = &record;
      c.name = p.display_name_fallback;
      const std::string at = "stages[" + std::to_string(i) + "]";
      const auto tokens = prompt.block_tokens(c, Block::Identity);
      if (!tokens) {
        errors.push_back(ContentError{p.file, p.stages[i].line, at + ": block 2 does not render"});
        continue;
      }
      if (*tokens > cap) {
        ContentError e{p.file, p.stages[i].line,
                       at + ": block 2 (the stage's identity, the role, summary and appearance) comes to about " +
                           std::to_string(*tokens) + " tokens, past its cap of " + std::to_string(cap)};
        (strict ? errors : warnings).push_back(std::move(e));
      }
      // Block 4, the voice, is never trimmed either (NPC §7.3): held to its cap as block 2 is, once (it has no stage).
      if (i != 0) continue;
      const auto voice = prompt.block_tokens(c, Block::Voice);
      if (voice && *voice > kBlocks[kVoice].cap) {
        ContentError e{p.file, 0,
                       "voice: block 4 (the register, the voice fields and the instructions) comes to about " +
                           std::to_string(*voice) + " tokens, past its cap of " + std::to_string(kBlocks[kVoice].cap)};
        (strict ? errors : warnings).push_back(std::move(e));
      }
    }
  }
}

proto::Expected<SpeechContent, std::vector<ContentError>> load_speech(const ContentPack& pack) {
  std::vector<ContentError> errors;
  SpeechContent out;

  std::map<std::string, std::string> templates;
  for (const ContentFile& f : pack.others) {
    if (f.path.rfind("prompts/", 0) == 0) templates.emplace(f.path, f.text);
  }
  auto prompt = DialoguePrompt::load(templates);
  if (prompt) {
    out.prompt = std::shared_ptr<const DialoguePrompt>(std::move(*prompt));
  } else {
    errors.push_back(ContentError{"prompts/", 0, prompt.error().detail});
  }

  const ContentFile* banned = other(pack, kBannedPath);
  const ContentFile* safety = other(pack, kSafetyPath);
  if (banned == nullptr) errors.push_back(ContentError{std::string(kBannedPath), 0, "is missing"});
  if (safety == nullptr) errors.push_back(ContentError{std::string(kSafetyPath), 0, "is missing"});
  if (banned != nullptr && safety != nullptr) {
    auto lexicon = parse_lexicon(banned->text, safety->text);
    if (lexicon) {
      out.lint = std::make_shared<const SpeechLint>(std::move(*lexicon));
    } else {
      const LexiconError& e = lexicon.error();
      errors.push_back(ContentError{"lexicon/" + e.file, e.line, e.problem});
    }
  }

  if (out.prompt) lint_identity_blocks(pack, *out.prompt, errors, out.warnings);
  if (out.lint) lint_persona_wording(pack, out.lint->lexicon(), errors, out.warnings);
  if (!errors.empty()) return proto::Unexpected{std::move(errors)};
  return out;
}

}  // namespace en::core
