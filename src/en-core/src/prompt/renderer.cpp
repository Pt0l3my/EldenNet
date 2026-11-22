// The dialogue prompt's renderer (NPC §7.1 to §7.3): dialogue_prompt.hpp has the rules, README.md here the data's
// names. Every en-prompt error is caught here and answered as a PromptError: a failed render sends nothing.
#include <algorithm>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "en/core/dialogue_prompt.hpp"
#include "en/prompt/environment.hpp"
#include "en/prompt/messages.hpp"
#include "en/prompt/source.hpp"

namespace en::core {

using nlohmann::json;

namespace {

constexpr std::string_view kDialogueFolder = "prompts/dialogue";
constexpr std::string_view kRegisterFolder = "prompts/registers";

PromptError refusal(PromptRefusal why, std::string detail) { return PromptError{why, std::move(detail)}; }

// ---- The record's fields, their types checked ----

class Reader {
 public:
  Reader(const json& record, std::string_view persona) : record_(record), persona_(persona) {}

  // A text field at a dotted path: null when absent or null; an error when it is anything but text.
  json text(std::string_view path, bool phrase = false) {
    const json* v = at(path);
    if (v == nullptr || v->is_null()) return nullptr;
    if (!v->is_string()) return fail(path, "is not text");
    std::string s = v->get<std::string>();
    while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || (phrase && s.back() == '.'))) s.pop_back();
    while (!s.empty() && (s.front() == ' ' || s.front() == '\n')) s.erase(s.begin());
    return s.empty() ? json(nullptr) : json(std::move(s));
  }

  // A list of texts: [] when absent or null.
  json texts(std::string_view path) {
    const json* v = at(path);
    json out = json::array();
    if (v == nullptr || v->is_null()) return out;
    if (!v->is_array()) return fail(path, "is not a list");
    for (const json& e : *v) {
      if (!e.is_string()) return fail(path, "holds an entry that is not text");
      if (!e.get_ref<const std::string&>().empty()) out.push_back(e);
    }
    return out;
  }

  std::optional<std::int64_t> integer(std::string_view path) {
    const json* v = at(path);
    if (v == nullptr || v->is_null()) return std::nullopt;
    if (!v->is_number_integer()) {
      fail(path, "is not a whole number");
      return std::nullopt;
    }
    return v->get<std::int64_t>();
  }

  // voice.tics: [{text, max_per_conversation}] (or a bare text, its cap 1).
  void tics(json& texts, std::vector<std::uint8_t>& caps) {
    texts = json::array();
    const json* v = at("voice.tics");
    if (v == nullptr || v->is_null()) return;
    if (!v->is_array()) {
      fail("voice.tics", "is not a list");
      return;
    }
    for (const json& e : *v) {
      if (e.is_string()) {
        texts.push_back(e);
        caps.push_back(1);
        continue;
      }
      const auto t = e.is_object() ? e.find("text") : e.end();
      if (!e.is_object() || t == e.end() || !t->is_string()) {
        fail("voice.tics", "holds an entry with no text");
        return;
      }
      std::uint8_t cap = 1;
      if (const auto m = e.find("max_per_conversation"); m != e.end()) {
        if (!m->is_number_integer() || m->get<std::int64_t>() < 1 || m->get<std::int64_t>() > 255) {
          fail("voice.tics", "holds a max_per_conversation that is not a whole number from 1");
          return;
        }
        cap = static_cast<std::uint8_t>(m->get<std::int64_t>());
      }
      texts.push_back(*t);
      caps.push_back(cap);
    }
  }

  [[nodiscard]] const std::optional<PromptError>& error() const { return error_; }

 private:
  const json* at(std::string_view path) const {
    const json* v = &record_;
    std::size_t start = 0;
    while (start <= path.size()) {
      const std::size_t dot = path.find('.', start);
      const std::string key(path.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start));
      if (!v->is_object()) return nullptr;
      const auto it = v->find(key);
      if (it == v->end()) return nullptr;
      v = &*it;
      if (dot == std::string_view::npos) break;
      start = dot + 1;
    }
    return v;
  }

  json fail(std::string_view path, const char* problem) {
    if (!error_) {
      error_ = refusal(PromptRefusal::Template,
                       "persona " + std::string(persona_) + ": the record's " + std::string(path) + " " + problem);
    }
    return nullptr;
  }

  const json& record_;
  std::string_view persona_;
  std::optional<PromptError> error_;
};

json turns(std::span<const Turn> ts) {
  json out = json::array();
  for (const Turn& t : ts) {
    if (t.text.empty()) continue;
    out.push_back({{"tarnished", t.who == Turn::Who::Tarnished}, {"text", t.text}});
  }
  return out;
}

json strings(const std::vector<std::string>& v) {
  json out = json::array();
  for (const std::string& s : v) {
    if (!s.empty()) out.push_back(s);
  }
  return out;
}

json standing(const Standing& s) {
  return {{"known", strings(s.known)}, {"heard", strings(s.heard)}, {"believed", strings(s.believed)}};
}

std::string_view reminder_name(const std::optional<HardList>& r) {
  if (!r) return "";
  switch (*r) {
    case HardList::Banned:
      return "banned";
    case HardList::Promise:
      return "promise";
    case HardList::Safety:
      return "safety";
  }
  return "";
}

// ---- Trimming ----

// Where each exchange of the history starts: at each of the Tarnished's lines (persona lines before the first form
// one).
std::vector<std::size_t> exchange_starts(const json& history) {
  std::vector<std::size_t> starts;
  for (std::size_t i = 0; i < history.size(); ++i) {
    if (i == 0 || history[i]["tarnished"].get<bool>()) starts.push_back(i);
  }
  return starts;
}

proto::Message to_proto(const prompt::Message& m) {
  proto::Message out;
  switch (m.role) {
    case prompt::Role::system:
      out.role = proto::Role::System;
      break;
    case prompt::Role::user:
      out.role = proto::Role::User;
      break;
    case prompt::Role::assistant:
      out.role = proto::Role::Assistant;
      break;
  }
  for (const prompt::Part& p : m.parts) out.parts.push_back(proto::Part{p.text, p.cache});
  proto::normalise(out);
  return out;
}

std::uint32_t tokens_of(const std::vector<prompt::Message>& messages) {
  return static_cast<std::uint32_t>(prompt::estimate_tokens(messages));
}

}  // namespace

proto::Expected<json, PromptError> dialogue_data(const DialogueContext& context) {
  const std::string persona = context.persona.empty() ? std::string("(unnamed)") : std::string(context.persona);
  if (context.record == nullptr)
    return proto::Unexpected{refusal(PromptRefusal::NoRecord, "persona " + persona + ": no record")};
  if (!context.record->is_object()) {
    return proto::Unexpected{refusal(PromptRefusal::Template, "persona " + persona + ": the record is not an object")};
  }
  Reader r(*context.record, persona);
  json p;
  p["identity"] = r.text("stage.identity");
  p["role"] = r.text("public.role", true);
  p["summary"] = r.text("public.summary");
  p["appearance"] = r.text("public.appearance");
  p["personality"] = r.text("private.personality");
  p["honesty"] = r.text("private.honesty", true);
  p["claims"] = r.text("private.claims");
  p["seeks"] = r.texts("private.seeks");
  p["ties"] = r.texts("private.ties");
  p["unlock"] = r.text("stage.unlock.text");
  p["cadence"] = r.text("voice.cadence");
  p["diction"] = r.text("voice.diction");
  p["deflection"] = r.text("voice.deflection");
  p["openers"] = r.texts("voice.openers");
  std::vector<std::uint8_t> tic_caps;
  json tics;
  r.tics(tics, tic_caps);
  p["tics"] = tics;
  p["instructions"] = r.text("instructions");
  const json address = r.texts("address_forms");
  p["address"] = address.empty() ? json(nullptr) : address.front();
  json also = json::array();
  for (std::size_t i = 1; i < address.size(); ++i) also.push_back(address[i]);
  p["address_also"] = also;
  json stage;
  stage["road"] = r.text("stage.road");
  stage["where"] = r.text("stage.where", true);
  stage["mood"] = r.text("stage.mood", true);
  stage["gist"] = r.text("stage.gist");
  const json register_name = r.text("register");
  const json pronoun = r.text("pronouns");
  const std::optional<std::int64_t> words = r.integer("reply_words_max");
  if (r.error()) return proto::Unexpected{*r.error()};

  json d;
  d["persona"] = std::move(p);
  d["stage"] = std::move(stage);
  d["name"] = std::string(context.name);
  std::string reg = register_name.is_null() ? std::string("formal") : register_name.get<std::string>();
  if (std::find(kRegisters.begin(), kRegisters.end(), reg) == kRegisters.end()) {
    return proto::Unexpected{
        refusal(PromptRefusal::Template, "persona " + persona + ": the record's register is not one of the five")};
  }
  d["register"] = reg;
  const std::string pr = pronoun.is_null() ? std::string("they") : pronoun.get<std::string>();
  const char* possessive = pr == "he"     ? "his"
                           : pr == "she"  ? "her"
                           : pr == "it"   ? "its"
                           : pr == "they" ? "their"
                                          : nullptr;
  if (possessive == nullptr) {
    return proto::Unexpected{
        refusal(PromptRefusal::Template, "persona " + persona + ": the record's pronouns is not he, she, they or it")};
  }
  d["pronoun"] = pr;
  d["possessive"] = possessive;
  d["words_max"] = std::clamp<std::int64_t>(words.value_or(40), 10, 60);
  d["standing"] = {{"tied", standing(context.tied)}, {"land", standing(context.land)}};
  d["earlier"] = turns(context.earlier);
  d["history"] = turns(context.history);
  const Scene& s = context.scene;
  json nearby = json::array();
  for (const std::string& n : s.nearby) {
    if (!n.empty() && nearby.size() < 4) nearby.push_back(n);
  }
  json just_now = json::array();
  for (const std::string& n : s.just_now) {
    if (!n.empty() && just_now.size() < 3) just_now.push_back(n);
  }
  d["scene"] = {{"place", s.place},    {"time_word", s.time_word}, {"weather_word", s.weather_word},
                {"nearby", nearby},    {"tarnished", s.tarnished}, {"just_now", just_now},
                {"you_now", s.you_now}};
  json recap = json::array();
  const std::size_t from = context.recap.size() > 3 ? context.recap.size() - 3 : 0;
  for (std::size_t i = from; i < context.recap.size(); ++i) {
    if (!context.recap[i].empty()) recap.push_back(context.recap[i]);
  }
  d["recap"] = recap;
  json spent = json::array();
  for (std::size_t i = 0; i < tics.size() && i < context.tally.tics.size(); ++i) {
    if (context.tally.tics[i] >= tic_caps[i]) spent.push_back(tics[i]);
  }
  d["spent_tics"] = spent;
  d["reminder"] = std::string(reminder_name(context.reminder));
  d["actions_block"] = "";  // the seams for actions (DESIGN §13): empty in v1
  d["actions_tail"] = "";
  return d;
}

struct DialoguePrompt::Impl {
  explicit Impl(std::shared_ptr<const prompt::TemplateSource> source) : env(std::move(source)) {}
  prompt::Environment env;
  std::array<std::string, kBlockCount> parts;  // each block's template name

  // A block's messages; none for a block that renders nothing (a clause left out whole, §7.1), which en-prompt answers
  // as an error of its own, after rendering, for a prompt that it is not here.
  std::vector<prompt::Message> block(std::size_t i, const json& data) const {
    try {
      return env.render(parts[i], data);
    } catch (const prompt::TemplateError& e) {
      if (e.message() == "the prompt rendered no messages" && e.trace().empty()) return {};
      throw;
    }
  }
};

DialoguePrompt::DialoguePrompt(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
DialoguePrompt::~DialoguePrompt() = default;

proto::Expected<std::unique_ptr<DialoguePrompt>, PromptError> DialoguePrompt::load(
    const std::map<std::string, std::string>& templates) {
  try {
    std::map<std::string, std::string> mine;
    for (const auto& [name, text] : templates) {
      if (name.rfind("prompts/", 0) == 0 || name.rfind("prompts\\", 0) == 0) mine.emplace(name, text);
    }
    auto source = std::make_shared<const prompt::MemorySource>(std::move(mine));
    auto impl = std::make_unique<Impl>(source);
    const std::optional<std::vector<std::string>> files = source->files(kDialogueFolder);
    if (!files) return proto::Unexpected{refusal(PromptRefusal::Template, "the pack has no prompts/dialogue/")};
    for (const std::string& f : *files) {
      const std::string_view base = std::string_view(f).substr(f.rfind('/') + 1);
      const std::size_t underscore = base.find('_');
      const std::string_view number = base.substr(0, underscore == std::string_view::npos ? 0 : underscore);
      bool known = false;
      for (std::size_t i = 0; i < kBlockCount; ++i) {
        if (kBlocks[i].number != number) continue;
        if (!impl->parts[i].empty()) {
          return proto::Unexpected{
              refusal(PromptRefusal::Template, "prompts/dialogue/ has two parts " + std::string(number))};
        }
        impl->parts[i] = f.find('/') == std::string::npos ? std::string(kDialogueFolder) + "/" + f : f;
        known = true;
      }
      const bool numbered = !number.empty() && number.front() >= '0' && number.front() <= '9';
      if (!known && numbered) {
        return proto::Unexpected{refusal(
            PromptRefusal::Template,
            "prompts/dialogue/ has a part " + std::string(number) + " that is no block of the dialogue prompt")};
      }
    }
    for (std::size_t i = 0; i < kBlockCount; ++i) {
      if (impl->parts[i].empty()) {
        return proto::Unexpected{
            refusal(PromptRefusal::Template, "prompts/dialogue/ has no part " + std::string(kBlocks[i].number))};
      }
    }
    for (const std::string_view r : kRegisters) {
      const std::string name = std::string(kRegisterFolder) + "/" + std::string(r);
      if (source->find(name + ".md") != prompt::TemplateSource::Entry::file) {
        return proto::Unexpected{refusal(PromptRefusal::Template, "the pack has no " + name + ".md")};
      }
    }
    // The dialogue folder against the data's names, every branch: an unknown name is an error at load.
    const json sample = {{"stage", json::object()}};
    DialogueContext probe;
    probe.persona = "lint";
    probe.record = &sample;
    auto data = dialogue_data(probe);
    if (!data) return proto::Unexpected{data.error()};
    std::vector<prompt::Finding> findings = impl->env.lint(kDialogueFolder, &*data);
    for (const std::string_view r : kRegisters) {  // named by a computed name, so the folder's lint does not reach them
      if (!findings.empty()) break;
      findings = impl->env.lint(std::string(kRegisterFolder) + "/" + std::string(r) + ".md", &*data);
    }
    if (!findings.empty()) {
      return proto::Unexpected{refusal(PromptRefusal::Template, prompt::to_string(findings.front()))};
    }
    return std::unique_ptr<DialoguePrompt>(new DialoguePrompt(std::move(impl)));
  } catch (const prompt::TemplateError& e) {
    return proto::Unexpected{refusal(PromptRefusal::Template, e.what())};
  } catch (const std::exception& e) {
    return proto::Unexpected{
        refusal(PromptRefusal::Template, std::string("the templates could not load: ") + e.what())};
  }
}

proto::Expected<RenderedPrompt, PromptError> DialoguePrompt::render(const DialogueContext& context) const {
  auto r = report(context);
  if (!r) return proto::Unexpected{std::move(r.error())};
  return std::move(r->prompt);
}

proto::Expected<PromptReport, PromptError> DialoguePrompt::report(const DialogueContext& context) const {
  auto data_or = dialogue_data(context);
  if (!data_or) return proto::Unexpected{std::move(data_or.error())};
  json data = std::move(*data_or);
  const std::string persona = context.persona.empty() ? std::string("(unnamed)") : std::string(context.persona);
  try {
    std::array<std::vector<prompt::Message>, kBlockCount> blocks;
    std::array<std::uint32_t, kBlockCount> tokens{};
    const auto render_block = [&](std::size_t i) {
      blocks[i] = impl_->block(i, data);
      tokens[i] = tokens_of(blocks[i]);
    };
    for (std::size_t i = 0; i < kBlockCount; ++i) render_block(i);
    const auto assembled = [&] {
      std::vector<prompt::Message> all;
      for (const auto& b : blocks) all.insert(all.end(), b.begin(), b.end());
      return prompt::merge_same_role(std::move(all));
    };
    PromptReport report;
    constexpr std::size_t kHistory = static_cast<std::size_t>(Block::History);
    constexpr std::size_t kScene = static_cast<std::size_t>(Block::Scene);
    // One step of block 6's trim: the previous conversation's oldest line, else this one's oldest exchange.
    const auto trim_history = [&]() -> bool {
      json& earlier = data["earlier"];
      if (!earlier.empty()) {
        earlier.erase(earlier.begin());
        ++report.earlier_trimmed;
        return true;
      }
      json& history = data["history"];
      const std::vector<std::size_t> starts = exchange_starts(history);
      if (starts.size() <= kKeptExchanges) return false;
      history.erase(history.begin(), history.begin() + static_cast<std::ptrdiff_t>(starts[1]));
      ++report.exchanges_trimmed;
      return true;
    };
    const auto trim_nearby = [&]() -> bool {
      json& nearby = data["scene"]["nearby"];
      if (nearby.empty()) return false;
      nearby.erase(nearby.end() - 1);
      ++report.nearby_trimmed;
      return true;
    };
    while (tokens[kHistory] > kBlocks[kHistory].cap && trim_history()) render_block(kHistory);
    while (tokens[kScene] > kBlocks[kScene].cap && trim_nearby()) render_block(kScene);
    std::vector<prompt::Message> all = assembled();
    while (tokens_of(all) > kPromptTarget) {
      if (trim_history()) {
        render_block(kHistory);
      } else if (trim_nearby()) {
        render_block(kScene);
      } else {
        break;
      }
      all = assembled();
    }
    const std::uint32_t total = tokens_of(all);
    if (total > kPromptHardCap) {
      return proto::Unexpected{refusal(PromptRefusal::OverCap, "persona " + persona + ": about " +
                                                                   std::to_string(total) + " tokens after every trim")};
    }
    for (std::size_t i = 0; i < kBlockCount; ++i) {
      report.blocks[i] = BlockReport{kBlocks[i].block, tokens[i], tokens[i] > kBlocks[i].cap};
    }
    for (const prompt::Message& m : all) report.prompt.messages.push_back(to_proto(m));
    report.prompt.tokens = total;
    return report;
  } catch (const prompt::TemplateError& e) {
    return proto::Unexpected{refusal(PromptRefusal::Template, "persona " + persona + ": " + e.what())};
  } catch (const std::exception& e) {
    return proto::Unexpected{refusal(PromptRefusal::Template, "persona " + persona + ": " + e.what())};
  }
}

}  // namespace en::core
