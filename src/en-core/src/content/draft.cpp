// The "draft with AI" helper's prompt and its reply read (profile.hpp; NPC §4.7, §4.8's draft tests): the request's
// messages from the pack's own template, and the reply held to the layer's fields and caps, its invented names dropped
// and the whole draft discarded on a word of the safety list.
#include <algorithm>
#include <map>
#include <memory>

#include "en/core/profile.hpp"
#include "en/core/speech_lint.hpp"
#include "en/prompt/environment.hpp"
#include "schema.hpp"
#include "wording.hpp"

namespace en::core {

namespace {

proto::Message message_of(const prompt::Message& m) {
  proto::Message out;
  switch (m.role) {
    case prompt::Role::system: out.role = proto::Role::System; break;
    case prompt::Role::user: out.role = proto::Role::User; break;
    case prompt::Role::assistant: out.role = proto::Role::Assistant; break;
  }
  for (const prompt::Part& p : m.parts) out.parts.push_back(proto::Part{p.text, p.cache});
  proto::normalise(out);
  return out;
}

// A word that starts with a capital: ASCII A to Z, or a Latin-1 capital in UTF-8 (C3 80 to C3 9E, but for x).
bool capitalised(std::string_view w) {
  if (w.empty()) return false;
  const auto c0 = static_cast<unsigned char>(w[0]);
  if (c0 >= 'A' && c0 <= 'Z') return true;
  if (c0 == 0xC3 && w.size() > 1) {
    const auto c1 = static_cast<unsigned char>(w[1]);
    return c1 >= 0x80 && c1 <= 0x9E && c1 != 0x97;
  }
  return false;
}

// The word without the punctuation round it (an apostrophe inside stays).
std::string_view bare(std::string_view w) {
  const auto punct = [](char c) {
    return c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '?' || c == '"' || c == '(' || c == ')' ||
           c == '\'' || c == '[' || c == ']';
  };
  while (!w.empty() && punct(w.front())) w.remove_prefix(1);
  while (!w.empty() && punct(w.back())) w.remove_suffix(1);
  return w;
}

std::vector<std::string_view> words_in(std::string_view s) {
  std::vector<std::string_view> out;
  std::size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    const std::size_t start = i;
    while (i < s.size() && !(s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    if (i > start) out.push_back(s.substr(start, i - start));
  }
  return out;
}

// Whether a sentence or a subject names someone or somewhere the writer did not (PL6's rule for a name: a capitalised
// word that is not the first). "I" and its contractions are no names.
bool invents_a_name(std::string_view text, std::string_view given) {
  const std::vector<std::string_view> ws = words_in(text);
  for (std::size_t i = 1; i < ws.size(); ++i) {
    const std::string_view w = bare(ws[i]);
    if (!capitalised(w)) continue;
    if (w == "I" || w.rfind("I'", 0) == 0 || w.rfind("I\xE2\x80\x99", 0) == 0) continue;
    if (given.find(w) == std::string_view::npos) return true;
  }
  return false;
}

// The text cut into sentences, each with its end and the spaces after it.
std::vector<std::string> sentences_of(std::string_view s) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (std::size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if ((c == '.' || c == '!' || c == '?') && (i + 1 == s.size() || s[i + 1] == ' ')) {
      std::size_t end = i + 1;
      while (end < s.size() && s[end] == ' ') ++end;
      out.emplace_back(s.substr(start, end - start));
      start = end;
      i = end - 1;
    }
  }
  if (start < s.size()) out.emplace_back(s.substr(start));
  return out;
}

std::string trimmed(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\t')) s.pop_back();
  std::size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t')) ++i;
  return s.substr(i);
}

bool within(std::string_view s, std::size_t words, std::size_t chars) {
  return schema::words_of(s) <= words && schema::chars_of(s) <= chars;
}

}  // namespace

proto::Expected<std::vector<proto::Message>, std::string> draft_messages(std::span<const ContentFile> others,
                                                                         const DraftInput& in) {
  const auto file = std::find_if(others.begin(), others.end(), [](const ContentFile& f) { return f.path == kDraftPromptPath; });
  if (file == others.end()) return proto::Unexpected{std::string("the pack has no ") + std::string(kDraftPromptPath)};
  try {
    auto source = std::make_shared<const prompt::MemorySource>(
        std::map<std::string, std::string>{{std::string(kDraftPromptPath), file->text}});
    const prompt::Environment env(source);
    nlohmann::json current = nlohmann::json::object();
    for (const char* k : {"instructions", "subjects"}) {
      if (in.current.contains(k)) current[k] = in.current[k];
    }
    const nlohmann::json context = {
        {"kind_phrase", std::string(in.kind_phrase)},
        {"field_spec",
         "instructions: one paragraph of at most " + std::to_string(kLayerInstructionWords) +
             " words, how this person speaks and behaves; subjects: three to " + std::to_string(kLayerSubjectsMax) +
             " short phrases of at most " + std::to_string(kLayerSubjectWords) +
             " words each, what this person likes to talk about, never a question or an order."},
        {"notes", in.notes.substr(0, kDraftNotesMax)},
        {"current_json", current.empty() ? std::string() : current.dump()},
    };
    std::vector<proto::Message> out;
    for (const prompt::Message& m : env.render(kDraftPromptPath, context)) out.push_back(message_of(m));
    return out;
  } catch (const std::exception& e) {
    return proto::Unexpected{std::string("the draft's prompt could not be made: ") + e.what()};
  }
}

nlohmann::json draft_schema() {
  return {{"type", "object"},
          {"properties",
           {{"instructions", {{"type", "string"}}},
            {"subjects", {{"type", "array"}, {"items", {{"type", "string"}}}}}}},
          {"additionalProperties", false}};
}

DraftResult read_draft(std::string_view reply, const DraftInput& in, const Lexicon& lexicon) {
  DraftResult out;
  const std::size_t open = reply.find('{');
  const std::size_t close = reply.rfind('}');
  if (open == std::string_view::npos || close == std::string_view::npos || close < open) {
    out.unreadable = true;
    return out;
  }
  const nlohmann::json j = nlohmann::json::parse(reply.substr(open, close - open + 1), nullptr, false);
  if (!j.is_object()) {
    out.unreadable = true;
    return out;
  }
  // What the writer gave: their notes and their own current text. A name the reply holds must come from them.
  std::string given = in.notes;
  given += ' ';
  given += in.current.dump();
  // The safety list first: one hit discards the whole draft (NPC §4.7).
  const auto texts = [&](const nlohmann::json& v, auto&& each) {
    if (v.is_string()) each(v.get<std::string>());
    if (v.is_array()) {
      for (const auto& x : v) {
        if (x.is_string()) each(x.get<std::string>());
      }
    }
  };
  bool unsafe = false;
  for (const char* k : {"instructions", "subjects"}) {
    if (j.contains(k)) texts(j[k], [&](const std::string& t) { unsafe = unsafe || wording::safety_hit(lexicon, t); });
  }
  if (unsafe) {
    out.discarded = true;
    return out;
  }
  if (const auto it = j.find("instructions"); it != j.end() && it->is_string()) {
    std::string kept;
    for (const std::string& s : sentences_of(it->get<std::string>())) {
      if (invents_a_name(s, given)) {
        ++out.dropped;
        continue;
      }
      const std::string next = kept + s;
      if (!within(trimmed(next), kLayerInstructionWords, kLayerInstructionChars)) {
        ++out.cut;  // the rest is cut at this sentence's start
        if (kept.empty()) {
          // One sentence past the caps alone: cut at the last word within them.
          std::string part;
          for (const std::string_view w : words_in(s)) {
            const std::string longer = part.empty() ? std::string(w) : part + " " + std::string(w);
            if (!within(longer, kLayerInstructionWords, kLayerInstructionChars)) break;
            part = longer;
          }
          kept = part;
        }
        break;
      }
      kept = next;
    }
    kept = trimmed(kept);
    for (char& c : kept) {
      if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    }
    if (!kept.empty()) out.draft["instructions"] = kept;
  }
  if (const auto it = j.find("subjects"); it != j.end() && it->is_array()) {
    nlohmann::json subjects = nlohmann::json::array();
    for (const auto& v : *it) {
      if (!v.is_string()) continue;
      const std::string s = trimmed(v.get<std::string>());
      if (s.empty()) continue;
      if (invents_a_name(s, given)) {
        ++out.dropped;
        continue;
      }
      if (!within(s, kLayerSubjectWords, kLayerSubjectChars) || subjects.size() >= kLayerSubjectsMax) {
        ++out.cut;
        continue;
      }
      subjects.push_back(s);
    }
    if (!subjects.empty()) out.draft["subjects"] = std::move(subjects);
  }
  return out;
}

}  // namespace en::core
