// The dialogue prompt outside the game: prompt_fixture.hpp says what each part does.
#include "prompt_fixture.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <system_error>

#include "en/core/knowledge.hpp"
#include "en/core/speech_content.hpp"
#include "en/prompt/environment.hpp"
#include "eval_run.hpp"  // read_content_folder

namespace en::tools {
namespace {

namespace fs = std::filesystem;
using nlohmann::json;

std::optional<std::string> slurp(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool template_file(const fs::path& p) {
  const std::string ext = p.extension().string();
  for (const std::string_view e : prompt::template_extensions()) {
    if (ext == e) return true;
  }
  return false;
}

std::optional<std::string> text_of(const json& o, const char* key, std::string& problem) {
  const auto it = o.find(key);
  if (it == o.end() || it->is_null()) return std::string();
  if (!it->is_string()) {
    problem = std::string("\"") + key + "\" is not text";
    return std::nullopt;
  }
  return it->get<std::string>();
}

bool texts_of(const json& o, const char* key, std::vector<std::string>& out, std::string& problem) {
  const auto it = o.find(key);
  if (it == o.end() || it->is_null()) return true;
  if (!it->is_array()) {
    problem = std::string("\"") + key + "\" is not a list";
    return false;
  }
  for (const json& e : *it) {
    if (!e.is_string()) {
      problem = std::string("\"") + key + "\" holds an entry that is not text";
      return false;
    }
    out.push_back(e.get<std::string>());
  }
  return true;
}

bool turns_of(const json& o, const char* key, std::vector<core::Turn>& out, std::string& problem) {
  const auto it = o.find(key);
  if (it == o.end() || it->is_null()) return true;
  if (!it->is_array()) {
    problem = std::string("\"") + key + "\" is not a list";
    return false;
  }
  for (const json& e : *it) {
    const auto who = e.is_object() ? e.find("who") : e.end();
    const auto text = e.is_object() ? e.find("text") : e.end();
    if (!e.is_object() || who == e.end() || text == e.end() || !who->is_string() || !text->is_string() ||
        (*who != "tarnished" && *who != "persona")) {
      problem =
          std::string("\"") + key + "\" holds a turn that is not {\"who\": \"tarnished\" | \"persona\", \"text\"}";
      return false;
    }
    out.push_back(core::Turn{*who == "tarnished" ? core::Turn::Who::Tarnished : core::Turn::Who::Persona,
                             text->get<std::string>()});
  }
  return true;
}

bool standing_of(const json& o, const char* key, core::Standing& out, std::string& problem) {
  const auto it = o.find(key);
  if (it == o.end() || it->is_null()) return true;
  if (!it->is_object()) {
    problem = std::string("\"") + key + "\" is not an object";
    return false;
  }
  return texts_of(*it, "known", out.known, problem) && texts_of(*it, "heard", out.heard, problem) &&
         texts_of(*it, "believed", out.believed, problem);
}

const char* block_name(core::Block b) {
  switch (b) {
    case core::Block::Frame:
      return "1";
    case core::Block::Identity:
      return "2";
    case core::Block::Private:
      return "3";
    case core::Block::Voice:
      return "4";
    case core::Block::Contract:
      return "5";
    case core::Block::Standing:
      return "5b";
    case core::Block::History:
      return "6";
    case core::Block::Knowledge:
      return "7";
    case core::Block::Tarnished:
      return "8";
    case core::Block::Scene:
      return "9";
    case core::Block::Recap:
      return "10";
    case core::Block::Task:
      return "11";
  }
  return "?";
}

const char* refusal_name(core::PromptRefusal r) {
  switch (r) {
    case core::PromptRefusal::NoRecord:
      return "NoRecord";
    case core::PromptRefusal::Template:
      return "Template";
    case core::PromptRefusal::OverCap:
      return "OverCap";
  }
  return "?";
}

}  // namespace

bool read_pack_prompts(const fs::path& pack, std::map<std::string, std::string>& out, std::string& problem) {
  out.clear();
  const fs::path root = pack / "prompts";
  std::error_code ec;
  if (!fs::is_directory(root, ec)) {
    problem = "the pack has no prompts folder";
    return false;
  }
  for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
    if (!it->is_regular_file(ec) || !template_file(it->path())) continue;
    const auto text = slurp(it->path());
    if (!text) {
      problem = "a template of the pack cannot be read";
      return false;
    }
    out.emplace(it->path().lexically_relative(pack).generic_string(), *text);
  }
  if (ec) {
    problem = "the pack's prompts folder cannot be read";
    return false;
  }
  return true;
}

proto::Expected<core::Lexicon, std::string> read_pack_lexicon(const fs::path& pack) {
  const auto banned = slurp(pack / "lexicon" / "banned.toml");
  const auto safety = slurp(pack / "lexicon" / "safety.toml");
  if (!banned || !safety)
    return proto::Unexpected{std::string("the pack's lexicon/banned.toml or safety.toml cannot be read")};
  auto lex = core::parse_lexicon(*banned, *safety);
  if (!lex) {
    const core::LexiconError& e = lex.error();
    return proto::Unexpected{e.file + (e.line > 0 ? ":" + std::to_string(e.line) : std::string()) + ": " + e.problem};
  }
  return std::move(*lex);
}

core::DialogueContext PromptFixture::context() const {
  core::DialogueContext c;
  c.persona = persona;
  c.record = &record;
  c.name = name;
  c.earlier = earlier;
  c.history = history;
  c.scene = scene;
  c.recap = recap;
  c.reminder = reminder;
  c.tally = tally;
  c.tied = tied;
  c.land = land;
  return c;
}

proto::Expected<PromptFixture, std::string> parse_prompt_fixture(const std::string& text) {
  const json j = json::parse(text, nullptr, false);
  if (j.is_discarded() || !j.is_object()) return proto::Unexpected{std::string("the fixture is not a JSON object")};
  if (!j.contains("fixture") || j["fixture"] != 1)
    return proto::Unexpected{std::string("the fixture's format is not 1")};
  static constexpr std::array<std::string_view, 13> kKeys = {"fixture",  "persona", "name",  "record",
                                                             "earlier",  "history", "scene", "recap",
                                                             "reminder", "tally",   "tied",  "land", "pack"};
  for (const auto& [k, v] : j.items()) {
    if (std::find(kKeys.begin(), kKeys.end(), k) == kKeys.end()) {
      return proto::Unexpected{"the fixture has an unknown key \"" + k + "\""};
    }
  }
  PromptFixture f;
  std::string problem;
  const auto persona = text_of(j, "persona", problem);
  const auto name = text_of(j, "name", problem);
  if (!persona || !name) return proto::Unexpected{problem};
  f.persona = *persona;
  f.name = *name;
  if (const auto pk = j.find("pack"); pk != j.end()) {
    if (j.contains("record") || j.contains("tied") || j.contains("land"))
      return proto::Unexpected{std::string("\"pack\" takes the place of \"record\", \"tied\" and \"land\"")};
    if (!pk->is_object()) return proto::Unexpected{std::string("\"pack\" is not an object")};
    for (const auto& [k, v] : pk->items()) {
      if (k != "persona" && k != "flags" && k != "set") return proto::Unexpected{"\"pack\" has an unknown key \"" + k + "\""};
    }
    PackPersona pp;
    const auto who = text_of(*pk, "persona", problem);
    const auto flags = text_of(*pk, "flags", problem);
    if (!who || !flags || !texts_of(*pk, "set", pp.set, problem)) return proto::Unexpected{problem};
    if (who->empty() || flags->empty()) return proto::Unexpected{std::string("\"pack\" needs a persona and a flag state")};
    pp.persona = *who;
    pp.flags = *flags;
    f.pack = std::move(pp);
  } else {
    if (!j.contains("record") || !j["record"].is_object())
      return proto::Unexpected{std::string("the fixture has no record")};
    f.record = j["record"];
  }
  if (!turns_of(j, "earlier", f.earlier, problem) || !turns_of(j, "history", f.history, problem) ||
      !texts_of(j, "recap", f.recap, problem) || !standing_of(j, "tied", f.tied, problem) ||
      !standing_of(j, "land", f.land, problem)) {
    return proto::Unexpected{problem};
  }
  if (const auto s = j.find("scene"); s != j.end() && !s->is_null()) {
    if (!s->is_object()) return proto::Unexpected{std::string("\"scene\" is not an object")};
    for (const auto& [k, v] : s->items()) {
      static constexpr std::array<std::string_view, 7> kScene = {"place",     "time_word", "weather_word", "nearby",
                                                                 "tarnished", "just_now",  "you_now"};
      if (std::find(kScene.begin(), kScene.end(), k) == kScene.end()) {
        return proto::Unexpected{"\"scene\" has an unknown key \"" + k + "\""};
      }
    }
    std::optional<std::string> t;
    if (!(t = text_of(*s, "place", problem))) return proto::Unexpected{problem};
    f.scene.place = *t;
    if (!(t = text_of(*s, "time_word", problem))) return proto::Unexpected{problem};
    f.scene.time_word = *t;
    if (!(t = text_of(*s, "weather_word", problem))) return proto::Unexpected{problem};
    f.scene.weather_word = *t;
    if (!(t = text_of(*s, "tarnished", problem))) return proto::Unexpected{problem};
    f.scene.tarnished = *t;
    if (!(t = text_of(*s, "you_now", problem))) return proto::Unexpected{problem};
    f.scene.you_now = *t;
    if (!texts_of(*s, "nearby", f.scene.nearby, problem) || !texts_of(*s, "just_now", f.scene.just_now, problem)) {
      return proto::Unexpected{problem};
    }
  }
  if (const auto r = j.find("reminder"); r != j.end() && !r->is_null()) {
    if (*r == "banned") {
      f.reminder = core::HardList::Banned;
    } else if (*r == "promise") {
      f.reminder = core::HardList::Promise;
    } else if (*r == "safety") {
      f.reminder = core::HardList::Safety;
    } else {
      return proto::Unexpected{std::string("\"reminder\" is not banned, promise or safety")};
    }
  }
  if (const auto t = j.find("tally"); t != j.end() && !t->is_null()) {
    const auto tics = t->is_object() ? t->find("tics") : t->end();
    if (!t->is_object() || (tics != t->end() && (!tics->is_array() || tics->size() > 3))) {
      return proto::Unexpected{std::string("\"tally\" is not {\"tics\": [at most three counts], \"spent\": n}")};
    }
    for (const auto& [k, v] : t->items()) {
      if (k != "tics" && k != "spent") return proto::Unexpected{"\"tally\" has an unknown key \"" + k + "\""};
    }
    if (const auto spent = t->find("spent"); spent != t->end() && !spent->is_null()) {
      if (!spent->is_number_unsigned() || spent->get<unsigned>() > 2) {
        return proto::Unexpected{std::string("\"tally\"'s \"spent\" is not a tic's index, 0 to 2")};
      }
      f.tally.spent = static_cast<std::uint8_t>(spent->get<unsigned>());
    }
    if (tics != t->end()) {
      for (std::size_t i = 0; i < tics->size(); ++i) {
        if (!(*tics)[i].is_number_unsigned() || (*tics)[i].get<unsigned>() > 255) {
          return proto::Unexpected{std::string("\"tally\" holds a count that is not 0 to 255")};
        }
        f.tally.tics[i] = static_cast<std::uint8_t>((*tics)[i].get<unsigned>());
      }
    }
  }
  return f;
}

std::optional<fs::path> find_flag_state(const fs::path& folder, const std::string& name) {
  const bool slug = !name.empty() && name.size() <= 32 &&
                    std::all_of(name.begin(), name.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
  if (!slug) return std::nullopt;
  std::error_code ec;
  fs::path from = fs::absolute(folder, ec);
  for (int up = 0; up < 8 && !from.empty(); ++up) {
    for (const fs::path& sub : {fs::path("flagstates"), fs::path("fixtures") / "flagstates", fs::path("tests") / "fixtures" / "flagstates"}) {
      const fs::path f = from / sub / (name + ".toml");
      if (fs::is_regular_file(f, ec)) return f;
    }
    if (from == from.parent_path()) break;
    from = from.parent_path();
  }
  return std::nullopt;
}

std::string complete_from_pack(PromptFixture& f, const fs::path& pack_root, const fs::path& folder) {
  if (!f.pack) return {};
  auto files = read_content_folder(pack_root, core::ContentOrigin::Shipped);
  if (!files) return "the pack: " + files.error();
  auto pack = core::load_content(*files);
  if (!pack) return "the pack does not load: " + core::describe(pack.error().front());
  if (auto speech = core::load_speech(*pack); !speech) return "the pack does not load: " + core::describe(speech.error().front());
  auto knowledge = core::Knowledge::build(*pack);
  if (!knowledge) return "the pack does not load: " + core::describe(knowledge.error().front());
  std::size_t index = pack->personas.size();
  for (std::size_t i = 0; i < pack->personas.size(); ++i) {
    if (pack->personas[i].id == f.pack->persona) index = i;
  }
  if (index == pack->personas.size()) return "the pack has no persona of that id";
  const auto path = find_flag_state(folder, f.pack->flags);
  if (!path) return "no fixture flag state of that name above the fixture";
  const auto text = slurp(*path);
  if (!text) return "the fixture flag state cannot be read";
  std::string state = *text;
  if (!f.pack->set.empty()) {  // the aliases set on top: a [flags] table's entries, added
    if (state.find("[flags]") == std::string::npos) state += "\n[flags]\n";
    std::string extra;
    for (const std::string& a : f.pack->set) extra += "\"" + a + "\" = true\n";
    state.insert(state.find("[flags]") + 8, extra);
  }
  auto flags = knowledge->fixture(state);
  if (!flags) return "the fixture flag state: " + flags.error();
  const core::WorldView view = knowledge->derive(*flags);
  const core::Grounding g = knowledge->ground(index, *flags, view);
  f.record = pack->personas[index].resolved(g.stage);
  f.tied = g.tied;
  f.land = g.land;
  return {};
}

std::string prompt_golden_text(const std::string& name,
                               const proto::Expected<core::PromptReport, core::PromptError>& result) {
  std::string out = "# prompt golden: " + name + " (format 1)\n";
  if (!result) {
    out += "# refused: " + std::string(refusal_name(result.error().why)) + ": " + result.error().detail + "\n";
    return out;
  }
  const core::PromptReport& r = *result;
  std::size_t breakpoints = 0;
  for (const proto::Message& m : r.prompt.messages) {
    for (const proto::Part& p : m.parts) breakpoints += p.cache_break ? 1 : 0;
  }
  out += "# tokens " + std::to_string(r.prompt.tokens) + " (target " + std::to_string(core::kPromptTarget) +
         ", hard cap " + std::to_string(core::kPromptHardCap) + "); " + std::to_string(r.prompt.messages.size()) +
         " messages, " + std::to_string(breakpoints) + " cache breakpoints\n";
  out += "# blocks (tokens/cap):";
  for (std::size_t i = 0; i < core::kBlockCount; ++i) {
    const core::BlockReport& b = r.blocks[i];
    out += " " + std::string(block_name(b.block)) + "=" + std::to_string(b.tokens) + "/" +
           std::to_string(core::kBlocks[i].cap) + (b.over_cap ? "!" : "");
  }
  out += "\n# trimmed: earlier lines " + std::to_string(r.earlier_trimmed) + ", exchanges " +
         std::to_string(r.exchanges_trimmed) + ", nearby others " + std::to_string(r.nearby_trimmed) + "\n";
  for (const proto::Message& m : r.prompt.messages) {
    out += m.role == proto::Role::System ? "[ system ]\n"
           : m.role == proto::Role::User ? "[ user ]\n"
                                         : "[ assistant ]\n";
    for (std::size_t i = 0; i < m.parts.size(); ++i) {
      std::string_view text = m.parts[i].text;
      if (i > 0 && !text.empty() && text.front() == '\n') text.remove_prefix(1);
      out += text;
      if (m.parts[i].cache_break) out += "\n[ cache ]";
      out += '\n';
    }
  }
  return out;
}

}  // namespace en::tools
