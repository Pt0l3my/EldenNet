// The content pack's speech: its prompts and lexicon, and block 2's content lint (speech_content.hpp; WHY§388).
#include "en/core/speech_content.hpp"

#include <map>
#include <string>
#include <utility>

namespace en::core {

namespace {

constexpr std::size_t kIdentity = static_cast<std::size_t>(Block::Identity);

const ContentFile* other(const ContentPack& pack, std::string_view path) {
  for (const ContentFile& f : pack.others) {
    if (f.path == path) return &f;
  }
  return nullptr;
}

}  // namespace

void lint_identity_blocks(const ContentPack& pack, const DialoguePrompt& prompt, std::vector<ContentError>& errors,
                          std::vector<ContentError>& warnings) {
  const std::uint16_t cap = kBlocks[kIdentity].cap;
  for (const Persona& p : pack.personas) {
    const PersonaRow* row = pack.row(p.id);
    // Shipped content and test personas are held to the cap; the player's own records are warned of.
    const bool strict = p.origin == ContentOrigin::Shipped || (row != nullptr && row->test_only);
    for (std::size_t i = 0; i < p.stages.size(); ++i) {
      const nlohmann::json record = p.resolved(i);
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
      if (*tokens <= cap) continue;
      ContentError e{p.file, p.stages[i].line,
                     at + ": block 2 (the stage's identity, the role, summary and appearance) comes to about " +
                         std::to_string(*tokens) + " tokens, past its cap of " + std::to_string(cap)};
      (strict ? errors : warnings).push_back(std::move(e));
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
  if (!errors.empty()) return proto::Unexpected{std::move(errors)};
  return out;
}

}  // namespace en::core
