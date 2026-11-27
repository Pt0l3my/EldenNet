// X1 outside the game: the set, its jobpacks and Jobrunner's results (eval_x1.hpp; NPC §12.2; O139 to O142).
#include "eval_x1.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#include <toml.hpp>

#include "en/core/core.hpp"
#include "en/core/prompt.hpp"
#include "en/core/speech_content.hpp"
#include "en/llm/config.hpp"
#include "en/llm/request.hpp"
#include "en/proto/json.hpp"
#include "en/proto/toml_check.hpp"
#include "eval_run.hpp"
#include "golden_run.hpp"  // read_file

namespace en::tools {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

std::string at(const toml::node& n, const std::string& what) {
  return "line " + std::to_string(n.source().begin.line) + ": " + what;
}

bool slug(std::string_view s, std::size_t max, bool dash) {
  if (s.empty() || s.size() > max) return false;
  return std::all_of(s.begin(), s.end(), [&](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || (dash && c == '-');
  });
}

// Jobrunner's id: 1 to 64 of A-Z a-z 0-9 _ - (ID_RX).
bool jobrunner_id(std::string_view s) {
  if (s.empty() || s.size() > JobpackCaps::kIdChars) return false;
  return std::all_of(s.begin(), s.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
  });
}

// The same words in the same order, whatever the spaces between them.
bool same_words(std::string_view a, std::string_view b) {
  const auto next = [](std::string_view& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\n' || s.front() == '\t')) s.remove_prefix(1);
    std::size_t n = 0;
    while (n < s.size() && s[n] != ' ' && s[n] != '\n' && s[n] != '\t') ++n;
    const std::string_view w = s.substr(0, n);
    s.remove_prefix(n);
    return w;
  };
  for (;;) {
    const std::string_view x = next(a), y = next(b);
    if (x != y) return false;
    if (x.empty()) return true;
  }
}

std::size_t code_points(std::string_view s) {
  std::size_t n = 0;
  for (const char c : s) n += (static_cast<unsigned char>(c) & 0xC0) != 0x80 ? 1 : 0;
  return n;
}

}  // namespace

// ---- The set ----

proto::Expected<X1Set, std::string> parse_x1_set(std::string_view text, const fs::path& folder) {
  if (const auto unsafe = proto::unsafe_for_toml(text, proto::TomlText::Player)) {
    return proto::Unexpected{"line " + std::to_string(unsafe->line) + ": refused before it is parsed: " + unsafe->problem};
  }
  toml::parse_result parsed = toml::parse(text);
  if (!parsed) {
    return proto::Unexpected{"line " + std::to_string(parsed.error().source().begin.line) +
                             ": not valid TOML: " + std::string(parsed.error().description())};
  }
  const toml::table& root = parsed.table();
  for (auto&& [k, v] : root) {
    if (k.str() != "schema" && k.str() != "name" && k.str() != "arm" && k.str() != "run") {
      return proto::Unexpected{at(v, "an unknown key")};
    }
  }
  if (root["schema"].value_or(std::int64_t{0}) != 1) return proto::Unexpected{std::string("schema must be 1")};
  X1Set set;
  set.folder = folder;
  set.name = root["name"].value_or(std::string());
  if (!slug(set.name, 40, true)) return proto::Unexpected{std::string("name: a slug of at most 40 characters")};
  set.arms.push_back(X1Arm{"positive", std::nullopt});
  if (const toml::node* arms = root.get("arm")) {
    const toml::table* t = arms->as_table();
    if (t == nullptr) return proto::Unexpected{at(*arms, "arm: must be a table of arms")};
    for (auto&& [k, v] : *t) {
      const std::string name(k.str());
      const toml::table* a = v.as_table();
      if (a == nullptr || !slug(name, 16, false) || name == "positive") {
        return proto::Unexpected{at(v, "arm: each is a table named by a slug of at most 16 characters, not positive")};
      }
      for (auto&& [ak, av] : *a) {
        if (ak.str() != "overrides") return proto::Unexpected{at(av, "arm." + name + ": an unknown key")};
      }
      const auto o = (*a)["overrides"].value<std::string>();
      if (!o || o->empty()) return proto::Unexpected{at(v, "arm." + name + ".overrides: a folder is needed")};
      set.arms.push_back(X1Arm{name, folder / *o});
    }
  }
  const toml::array* runs = root["run"].as_array();
  if (runs == nullptr || !runs->is_array_of_tables() || runs->empty()) {
    return proto::Unexpected{std::string("the set needs [[run]]s")};
  }
  constexpr std::array<std::string_view, 6> kRunKeys = {"pack", "code", "kind", "scenario", "arms", "repeat"};
  for (std::size_t i = 0; i < runs->size(); ++i) {
    const toml::table& t = *runs->get(i)->as_table();
    const std::string where = "run[" + std::to_string(i) + "]";
    for (auto&& [k, v] : t) {
      if (std::find(kRunKeys.begin(), kRunKeys.end(), k.str()) == kRunKeys.end()) {
        return proto::Unexpected{at(v, where + ": an unknown key")};
      }
    }
    X1Run r;
    r.line = static_cast<std::size_t>(t.source().begin.line);
    r.pack = t["pack"].value_or(std::string());
    r.code = t["code"].value_or(std::string());
    r.kind = t["kind"].value_or(std::string());
    const std::string scenario = t["scenario"].value_or(std::string());
    if (!slug(r.pack, 40, true)) return proto::Unexpected{at(t, where + ".pack: a slug of at most 40 characters")};
    if (!slug(r.code, 8, false) || r.code.find('_') != std::string::npos) {
      return proto::Unexpected{at(t, where + ".code: 1 to 8 of a-z and 0-9")};
    }
    if (r.kind != "voice" && r.kind != "test_question") {
      return proto::Unexpected{at(t, where + ".kind: must be voice or test_question")};
    }
    if (scenario.empty() || fs::path(scenario).is_absolute() || fs::path(scenario).extension() != ".toml") {
      return proto::Unexpected{at(t, where + ".scenario: a scenario's .toml, from the set's folder")};
    }
    r.scenario = folder / scenario;
    const toml::array* arms = t["arms"].as_array();
    if (arms == nullptr || arms->empty()) return proto::Unexpected{at(t, where + ".arms: a list of the set's arms")};
    for (const toml::node& n : *arms) {
      const auto name = n.value<std::string>();
      const bool known = name && std::any_of(set.arms.begin(), set.arms.end(), [&](const X1Arm& a) { return a.name == *name; });
      if (!known) return proto::Unexpected{at(n, where + ".arms: each must be positive or an arm the set defines")};
      if (std::find(r.arms.begin(), r.arms.end(), *name) != r.arms.end()) {
        return proto::Unexpected{at(n, where + ".arms: an arm named twice")};
      }
      r.arms.push_back(*name);
    }
    const auto repeat = t["repeat"].value_or(std::int64_t{1});
    if (repeat < 1 || repeat > 10) return proto::Unexpected{at(t, where + ".repeat: 1 to 10")};
    r.repeat = static_cast<int>(repeat);
    set.runs.push_back(std::move(r));
  }
  return set;
}

X1Sampling x1_sampling() {
  const llm::SlotBudget b = llm::default_budget(llm::kDialogue);
  X1Sampling s;
  s.max_tokens = b.max_tokens;
  s.temperature = b.temperature;
  for (const std::string_view stop : core::kDialogueStops) s.stop.emplace_back(stop);
  return s;
}

proto::Expected<std::vector<X1Item>, std::string> build_x1_items(const X1Set& set) {
  std::vector<X1Item> items;
  std::set<std::string> ids;
  for (std::size_t ri = 0; ri < set.runs.size(); ++ri) {
    const X1Run& r = set.runs[ri];
    const std::string where = "run[" + std::to_string(ri) + "] (line " + std::to_string(r.line) + ")";
    const auto text = read_file(r.scenario.string(), 256 * 1024);
    if (!text) return proto::Unexpected{where + ": the scenario cannot be read, or is larger than 256 KiB"};
    auto scenario = parse_eval_scenario(*text, r.scenario.parent_path());
    if (!scenario) return proto::Unexpected{where + ": the scenario: " + scenario.error()};
    // Our replies, which its later turns' prompts hold: each must show whole, as written.
    if (!scenario->mock) return proto::Unexpected{where + ": the scenario names no mock script of our replies"};
    const auto script_text = read_file((scenario->folder / *scenario->mock).string(), 256 * 1024);
    if (!script_text) return proto::Unexpected{where + ": the scenario's mock script cannot be read"};
    auto script = parse_mock_script(*script_text);
    if (!script) return proto::Unexpected{where + ": the mock script: " + script.error()};
    for (const std::string& arm : r.arms) {
      const auto a = std::find_if(set.arms.begin(), set.arms.end(), [&](const X1Arm& x) { return x.name == arm; });
      EvalOptions options;
      options.arm = a->overrides;
      auto run = run_eval(*scenario, EvalLlm::Mock, options);
      const std::string here = where + ", arm " + arm;
      if (!run) return proto::Unexpected{here + ": " + run.error()};
      if (!run->failed.empty()) return proto::Unexpected{here + ": the scripted run missed an expectation: " + run->failed.front()};
      for (std::size_t ti = 0; ti < run->turns.size(); ++ti) {
        const EvalTurn& t = run->turns[ti];
        const auto first = t.first_try(run->attempts);
        // Its history must be ours, as written: one request, shown whole, with no regeneration or deflection.
        const bool whole = ti < script->replies.size() && same_words(t.reply, script->replies[ti].text);
        if (t.requests != 1 || t.failure || t.deflection || t.cut_off || t.pages == 0 || t.prompt.empty() || !first ||
            !whole) {
          return proto::Unexpected{here + ": turn " + std::to_string(ti + 1) +
                                   ": a scripted reply of ours did not show whole, as written (regenerated, deflected, "
                                   "failed, cut off, or changed by the lint or the pager's caps), so the turns after it "
                                   "would not hold our history"};
        }
        X1Item item;
        item.pack = r.pack;
        item.kind = r.kind;
        item.arm = arm;
        item.persona = run->persona;
        item.register_name = run->register_name;
        item.scenario = run->scenario;
        item.code = r.code;
        item.state = t.flags;
        item.turn = ti + 1;
        item.repeat = r.repeat;
        item.player = t.player;
        item.messages = llm::render_messages(t.prompt, llm::CacheMarks::None);
        item.context = run->attempts[*first].context;
        item.topics = run->topics;
        const std::string turn = (ti + 1 < 10 ? "t0" : "t") + std::to_string(ti + 1);
        item.id = arm + "-" + run->persona + "-" + r.code + "-" + turn;
        if (r.repeat > 1) item.id += "-x" + std::to_string(r.repeat);
        if (!jobrunner_id(item.id)) return proto::Unexpected{here + ": an item's id is not 1 to 64 of A-Z a-z 0-9 _ -"};
        if (!ids.insert(item.id).second) return proto::Unexpected{here + ": an item's id is the set's twice"};
        if (item.messages.size() > JobpackCaps::kMessages) {
          return proto::Unexpected{here + ": turn " + std::to_string(ti + 1) + ": more messages than Jobrunner takes"};
        }
        items.push_back(std::move(item));
      }
    }
  }
  return items;
}

namespace {

std::string pack_description(const X1Set& set, const std::string& pack, const std::vector<const X1Item*>& items,
                             const X1Sampling& s) {
  std::vector<std::string> personas, arms;
  std::size_t questions = 0;
  for (const X1Item* i : items) {
    if (std::find(personas.begin(), personas.end(), i->persona) == personas.end()) personas.push_back(i->persona);
    if (std::find(arms.begin(), arms.end(), i->arm) == arms.end()) arms.push_back(i->arm);
    questions += i->kind == "test_question" ? 1 : 0;
  }
  const auto join = [](const std::vector<std::string>& v) {
    std::string out;
    for (const std::string& x : v) out += (out.empty() ? "" : ", ") + x;
    return out;
  };
  char sampling[96];
  std::snprintf(sampling, sizeof sampling, "%d tokens, temperature %.1f, thinking off", s.max_tokens, s.temperature);
  std::string d = "EldenNet's X1, voice in a bottle (" + set.name + ", " + pack + "): " + std::to_string(items.size()) +
                  " turns of " + join(personas) + ", arms " + join(arms) + ".";
  if (questions > 0) d += " " + std::to_string(questions) + " are test questions, each run again by its repeat.";
  d += "\nEach item is one turn of a conversation whose earlier lines we wrote, so no item waits on another; each goes "
       "to every server that is up. Sampling as the game's dialogue: " +
       std::string(sampling) + ".\nLoad the candidate models in your servers, then press Start.";
  return d;
}

}  // namespace

proto::Expected<std::vector<X1Jobpack>, std::string> x1_jobpacks(const X1Set& set, const std::vector<X1Item>& items) {
  const X1Sampling s = x1_sampling();
  std::vector<std::string> order;
  for (const X1Run& r : set.runs) {
    if (std::find(order.begin(), order.end(), r.pack) == order.end()) order.push_back(r.pack);
  }
  std::vector<X1Jobpack> out;
  for (const std::string& name : order) {
    std::vector<const X1Item*> in;
    for (const X1Item& i : items) {
      if (i.pack == name) in.push_back(&i);
    }
    X1Jobpack p;
    p.name = name;
    json j = json::object();
    j["format"] = "jobpack/1";
    j["name"] = name;
    j["server"] = "*";
    j["description"] = pack_description(set, name, in, s);
    j["defaults"] = {{"max_tokens", s.max_tokens}, {"temperature", s.temperature}, {"thinking", false}, {"stop", s.stop}};
    json list = json::array();
    for (const X1Item* i : in) {
      json item = {{"id", i->id}, {"messages", i->messages}};
      if (i->repeat > 1) item["repeat"] = i->repeat;
      list.push_back(std::move(item));
      p.runs += static_cast<std::size_t>(i->repeat);
    }
    p.items = in.size();
    j["items"] = std::move(list);
    p.json = std::move(j);
    const std::string desc = p.json["description"].get<std::string>();
    const std::string where = "the jobpack " + name + ": ";
    if (code_points(desc) > JobpackCaps::kDescriptionChars) return proto::Unexpected{where + "its description passes 600 characters"};
    if (p.items == 0) return proto::Unexpected{where + "it holds no item"};
    if (p.items > JobpackCaps::kItems) return proto::Unexpected{where + "it passes Jobrunner's 2,000 items"};
    if (p.runs * JobpackCaps::kServers > JobpackCaps::kRuns) {
      return proto::Unexpected{where + "its runs pass Jobrunner's 5,000 with 8 servers up"};
    }
    if (s.max_tokens < 1 || s.max_tokens > JobpackCaps::kMaxTokens || s.stop.size() > JobpackCaps::kStops ||
        std::any_of(s.stop.begin(), s.stop.end(), [](const std::string& x) { return x.empty() || x.size() > JobpackCaps::kStopChars; })) {
      return proto::Unexpected{where + "its sampling passes Jobrunner's caps"};
    }
    if (jobpack_text(p).size() > JobpackCaps::kFileBytes) return proto::Unexpected{where + "it passes Jobrunner's 2 MB"};
    out.push_back(std::move(p));
  }
  return out;
}

std::string jobpack_text(const X1Jobpack& pack) { return pack.json.dump(1) + "\n"; }

// ---- The import ----

std::optional<std::string> bounded_server(std::string_view name) {
  if (name.empty() || name.size() > 32) return std::nullopt;
  const bool ok = std::all_of(name.begin(), name.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
  });
  return ok ? std::optional<std::string>(name) : std::nullopt;
}

std::optional<std::string> bounded_model(std::string_view name) {
  if (const std::size_t cut = name.find_last_of("/\\"); cut != std::string_view::npos) name.remove_prefix(cut + 1);
  while (!name.empty() && name.front() == ' ') name.remove_prefix(1);
  while (!name.empty() && name.back() == ' ') name.remove_suffix(1);
  if (name.empty() || name.size() > 128) return std::nullopt;
  for (const char c : name) {
    const auto u = static_cast<unsigned char>(c);
    if (u < 0x20 || u >= 0x7F || c == '@' || c == '%') return std::nullopt;  // printable ASCII, no account or escape
  }
  // No address: a colon (a port, a scheme or a drive) or four dotted numbers.
  if (name.find(':') != std::string_view::npos) return std::nullopt;
  int dots = 0, digits_run = 0;
  for (const char c : name) {
    if (c >= '0' && c <= '9') {
      ++digits_run;
    } else if (c == '.' && digits_run > 0 && digits_run <= 3) {
      ++dots;
      digits_run = 0;
      if (dots >= 3) return std::nullopt;
    } else {
      dots = 0;
      digits_run = 0;
    }
  }
  return std::string(name);
}

namespace {

constexpr std::array<std::string_view, 19> kLineKeys = {
    "item",   "id",    "run",    "ok",       "server",        "model",     "reply",
    "reasoning", "finish_reason", "usage", "timings",  "first_token_s", "total_s",   "params",
    "error",  "messages", "provider", "tokens_per_s", "cost_usd"};

std::string label_for(std::vector<std::pair<std::string, std::string>>& labels, const std::string& raw,
                      const std::optional<std::string>& bounded, const char* stem) {
  for (const auto& [r, l] : labels) {
    if (r == raw) return l;
  }
  std::string label = bounded ? *bounded : std::string(stem) + "-" + std::to_string(labels.size() + 1);
  // Two raw names that bound to one label stay apart.
  for (const auto& [r, l] : labels) {
    if (l == label) label += "-" + std::to_string(labels.size() + 1);
  }
  labels.emplace_back(raw, label);
  return label;
}

std::optional<double> number_at(const json& o, std::string_view key) {
  const auto it = o.find(key);
  if (it == o.end() || it->is_null()) return std::nullopt;
  if (!it->is_number()) return std::nullopt;
  const double v = it->get<double>();
  return std::isfinite(v) && v >= 0 && v < 1e7 ? std::optional<double>(v) : std::nullopt;
}

}  // namespace

void read_jobrunner_results(std::string_view text, std::size_t file, const std::vector<X1Item>& items, X1Import& into) {
  std::map<std::string, const X1Item*, std::less<>> by_id;
  for (const X1Item& i : items) by_id.emplace(i.id, &i);
  std::set<std::tuple<std::string, int, std::string>> seen;
  for (const X1Line& l : into.lines) seen.emplace(l.id, l.run, l.server);
  const auto refuse = [&](std::size_t line, std::string why) { into.refused.push_back(X1Refusal{file, line, std::move(why)}); };
  if (text.size() > kResultsFileMax) {
    refuse(0, "the file is larger than 64 MB");
    return;
  }
  std::size_t number = 0, start = 0;
  while (start < text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(start, end - start);
    start = end + 1;
    ++number;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.find_first_not_of(" \t") == std::string_view::npos) continue;
    if (line.size() > kResultLineMax) {
      refuse(number, "the line is larger than 1 MB");
      continue;
    }
    const proto::ParsedJson p = proto::parse_json(line, proto::JsonLimits{kResultLineMax, 32});
    if (!p.ok() || !p.value.is_object()) {
      refuse(number, "not a JSON object");
      continue;
    }
    const json& o = p.value;
    bool unknown = false;
    for (auto it = o.begin(); it != o.end(); ++it) {
      if (std::find(kLineKeys.begin(), kLineKeys.end(), it.key()) == kLineKeys.end()) unknown = true;
    }
    if (unknown) {
      refuse(number, "a key Jobrunner's result lines do not have");
      continue;
    }
    const auto str = [&](const char* k, std::size_t max) -> std::optional<std::string> {
      const auto it = o.find(k);
      if (it == o.end() || !it->is_string() || it->get_ref<const std::string&>().size() > max) return std::nullopt;
      return it->get<std::string>();
    };
    const auto whole = [&](const char* k, std::int64_t lo, std::int64_t hi) -> std::optional<std::int64_t> {
      const auto it = o.find(k);
      if (it == o.end() || !it->is_number_integer()) return std::nullopt;
      const auto v = it->get<std::int64_t>();
      return v >= lo && v <= hi ? std::optional<std::int64_t>(v) : std::nullopt;
    };
    const auto item_no = whole("item", 1, 1000000);
    const auto id = str("id", 64);
    const auto run = whole("run", 1, 1000);
    const auto ok_it = o.find("ok");
    const auto server = str("server", 256);
    const auto model = str("model", 512);
    if (!item_no || !id || !jobrunner_id(*id) || !run || ok_it == o.end() || !ok_it->is_boolean() || !server || !model) {
      refuse(number, "item, id, run, ok, server or model missing or not of Jobrunner's form");
      continue;
    }
    // The optional fields, each of its type and within its bounds when present.
    const auto opt_type = [&](const char* k, bool (json::*is)() const noexcept) {
      const auto it = o.find(k);
      return it == o.end() || it->is_null() || ((*it).*is)();
    };
    if (!opt_type("reply", &json::is_string) || !opt_type("reasoning", &json::is_string) ||
        !opt_type("finish_reason", &json::is_string) || !opt_type("usage", &json::is_object) ||
        !opt_type("timings", &json::is_object) || !opt_type("first_token_s", &json::is_number) ||
        !opt_type("total_s", &json::is_number) || !opt_type("params", &json::is_object) ||
        !opt_type("error", &json::is_string) || !opt_type("messages", &json::is_array) ||
        !opt_type("provider", &json::is_string) || !opt_type("tokens_per_s", &json::is_number) ||
        !opt_type("cost_usd", &json::is_number)) {
      refuse(number, "a field not of Jobrunner's type");
      continue;
    }
    X1Line l;
    l.line = number;
    l.id = *id;
    l.run = static_cast<int>(*run);
    l.ok = ok_it->get<bool>();
    if (const auto it = o.find("reply"); it != o.end() && it->is_string()) {
      if (it->get_ref<const std::string&>().size() > kReplyMax) {
        refuse(number, "the reply is larger than 64 KB");
        continue;
      }
      l.reply = it->get<std::string>();
    }
    if (const auto it = o.find("finish_reason"); it != o.end() && it->is_string()) {
      const std::string& f = it->get_ref<const std::string&>();
      if (f.size() <= 32 && std::all_of(f.begin(), f.end(), [](char c) { return (c >= 'a' && c <= 'z') || c == '_'; })) {
        l.finish_reason = f;
      }
    }
    l.first_token_s = number_at(o, "first_token_s");
    l.total_s = number_at(o, "total_s");
    if (const auto it = o.find("usage"); it != o.end() && it->is_object()) {
      if (const auto c = it->find("completion_tokens"); c != it->end() && c->is_number_integer() &&
                                                       c->get<std::int64_t>() >= 0 && c->get<std::int64_t>() < 1000000) {
        l.completion_tokens = c->get<std::int64_t>();
      }
    }
    if (const auto t = number_at(o, "tokens_per_s")) {
      l.tokens_per_s = t;
    } else if (const auto it = o.find("timings"); it != o.end() && it->is_object()) {
      if (const auto t2 = number_at(*it, "predicted_per_second"); t2 && *t2 > 0) l.tokens_per_s = t2;
    }
    if (!l.tokens_per_s && l.completion_tokens && l.first_token_s && l.total_s && *l.total_s > *l.first_token_s) {
      l.tokens_per_s = static_cast<double>(*l.completion_tokens) / (*l.total_s - *l.first_token_s);
    }
    const auto item = by_id.find(l.id);
    if (item == by_id.end()) {
      refuse(number, "an id this set does not hold");
      continue;
    }
    if (l.run > item->second->repeat) {
      refuse(number, "a run past its item's repeat");
      continue;
    }
    if (const auto it = o.find("messages"); it != o.end() && it->is_array()) {
      l.had_messages = true;
      if (*it != item->second->messages) {
        refuse(number, "its messages are not this set's item's (another version of the set?)");
        continue;
      }
    }
    l.server = label_for(into.server_labels, *server, bounded_server(*server), "server");
    l.model = label_for(into.model_labels, *model, bounded_model(*model), "model");
    if (!seen.emplace(l.id, l.run, l.server).second) {
      refuse(number, "a run read already, from this server");
      continue;
    }
    into.lines.push_back(std::move(l));
  }
}

// ---- The measures ----

namespace {

double share(std::size_t n, std::size_t of) {
  if (of == 0) return 0;
  return std::round(static_cast<double>(n) / static_cast<double>(of) * 10000.0) / 10000.0;
}

json quantiles(std::vector<double> v) {
  json j = {{"n", v.size()}};
  if (v.empty()) return j;
  std::sort(v.begin(), v.end());
  const auto rank = [&](double q) {
    const auto k = static_cast<std::size_t>(std::ceil(q * static_cast<double>(v.size())));
    return std::round(v[std::max<std::size_t>(k, 1) - 1] * 1000.0) / 1000.0;
  };
  j["p50"] = rank(0.5);
  j["p90"] = rank(0.9);
  j["max"] = std::round(v.back() * 1000.0) / 1000.0;
  return j;
}

// What would show and still breaks the contract (NPC §11): markup, a list, a line break or a leading "Name:".
std::uint32_t shown_breaks(const std::vector<std::string>& shown) {
  std::uint32_t n = 0;
  for (std::size_t i = 0; i < shown.size(); ++i) {
    const std::string& s = shown[i];
    bool bad = s.find_first_of("*_#<>[]{}`|~\n\r\t") != std::string::npos;
    if (i == 0) {
      const std::size_t colon = s.find(": ");
      if (colon != std::string::npos && colon <= 26 && s.substr(0, colon).find_first_of(".,;!?") == std::string::npos &&
          words_of(s.substr(0, colon)) <= 3) {
        bad = true;
      }
    }
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) bad = true;
    n += bad ? 1 : 0;
  }
  return n;
}

constexpr std::array<core::Removed, 8> kContract = {
    core::Removed::Reasoning, core::Removed::Trailer, core::Removed::RunOn,      core::Removed::Markup,
    core::Removed::Emoji,     core::Removed::Control, core::Removed::NamePrefix, core::Removed::Quotes};

std::size_t idx(core::Removed r) { return static_cast<std::size_t>(r); }
std::size_t idx(core::SoftHit s) { return static_cast<std::size_t>(s); }
std::size_t idx(core::HardList h) { return static_cast<std::size_t>(h); }

const char* action_name(core::LintAction a) {
  switch (a) {
    case core::LintAction::Regenerate:
      return "regenerate";
    case core::LintAction::Deflect:
      return "deflect";
    case core::LintAction::Stop:
      return "stop";
    default:
      return "pass";
  }
}

struct Tally {
  std::size_t n = 0, failures = 0, replies = 0, within = 0, cut = 0, regenerate = 0, stop = 0, finish_length = 0;
  std::array<std::size_t, kHardKinds> hard{};  // replies with a hit of each list
  std::size_t wider = 0, promise = 0, archaic_lines = 0, openers = 0, repeated_openers = 0, unknown_nouns = 0;
  std::uint64_t sentences = 0, archaic_sentences = 0, tic_over_cap = 0, name_twice = 0, third_person = 0,
                duplicates = 0, shown_breaks = 0;
  std::array<std::size_t, kContract.size()> contract{};  // replies with a removal of each kind
  std::size_t contract_any = 0;
  std::array<std::size_t, 3> names{};

  void add(const X1Measured& m) {
    ++n;
    if (!m.line->ok || m.line->reply.empty()) {
      ++failures;
      return;
    }
    ++replies;
    const SpeechPass& p = m.pass;
    const SpeechCounts& c = p.counts;
    within += p.model_words <= m.item->context.words_max ? 1 : 0;
    cut += p.cut_by_pager ? 1 : 0;
    regenerate += p.stopped == core::LintAction::Regenerate || p.stopped == core::LintAction::Deflect ? 1 : 0;
    stop += p.stopped == core::LintAction::Stop ? 1 : 0;
    finish_length += m.line->finish_reason == "length" ? 1 : 0;
    for (std::size_t k = 0; k < kHardKinds; ++k) hard[k] += c.hard[k] > 0 ? 1 : 0;
    wider += c.soft[idx(core::SoftHit::WiderPromise)] > 0 ? 1 : 0;
    promise += c.soft[idx(core::SoftHit::WiderPromise)] > 0 || c.hard[idx(core::HardList::Promise)] > 0 ? 1 : 0;
    archaic_lines += c.archaic_sentences > 0 ? 1 : 0;
    openers += c.soft[idx(core::SoftHit::Opener)] + c.soft[idx(core::SoftHit::RepeatedOpener)] > 0 ? 1 : 0;
    repeated_openers += c.soft[idx(core::SoftHit::RepeatedOpener)] > 0 ? 1 : 0;
    unknown_nouns += c.soft[idx(core::SoftHit::UnknownNoun)] > 0 ? 1 : 0;
    sentences += c.sentences;
    archaic_sentences += c.archaic_sentences;
    tic_over_cap += c.soft[idx(core::SoftHit::TicOverCap)];
    name_twice += c.soft[idx(core::SoftHit::NameTwice)];
    third_person += c.soft[idx(core::SoftHit::ThirdPerson)];
    duplicates += c.soft[idx(core::SoftHit::Duplicate)];
    shown_breaks += m.shown_contract_breaks;
    bool any = false;
    for (std::size_t k = 0; k < kContract.size(); ++k) {
      const bool hit = kContract[k] == core::Removed::Trailer ? (c.removed[idx(kContract[k])] > 0 || c.trailers > 0)
                       : kContract[k] == core::Removed::RunOn ? (c.removed[idx(kContract[k])] > 0 || c.stream_stops > 0)
                                                              : c.removed[idx(kContract[k])] > 0;
      contract[k] += hit ? 1 : 0;
      any = any || hit;
    }
    contract_any += any ? 1 : 0;
    if (c.name_class) ++names[static_cast<std::size_t>(*c.name_class)];
  }

  [[nodiscard]] json to_json() const {
    json j;
    j["runs"] = n;
    j["failures"] = failures;
    j["replies"] = replies;
    j["within_words_max"] = share(within, replies);
    j["cut_by_pager"] = share(cut, replies);
    j["finish_length"] = finish_length;
    json h = json::object();
    for (std::size_t k = 0; k < kHardKinds; ++k) h[hard_name(k)] = hard[k];
    j["hard_hits"] = std::move(h);
    j["would_regenerate"] = regenerate;
    j["would_stop"] = stop;
    j["wider_promise"] = wider;
    j["promise_rate"] = share(promise, replies);
    j["archaic_lines"] = share(archaic_lines, replies);
    j["archaic_sentences"] = sentences == 0 ? 0.0 : share(archaic_sentences, sentences);
    j["openers_stripped"] = openers;
    j["repeated_openers"] = repeated_openers;
    j["tic_over_cap"] = tic_over_cap;
    j["name_twice"] = name_twice;
    j["third_person"] = third_person;
    j["duplicates"] = duplicates;
    j["unknown_nouns"] = share(unknown_nouns, replies);
    json cj = json::object();
    for (std::size_t k = 0; k < kContract.size(); ++k) cj[removed_name(idx(kContract[k]))] = contract[k];
    j["contract_caught"] = std::move(cj);
    j["contract_caught_share"] = share(contract_any, replies);
    j["contract_shown"] = shown_breaks;
    j["name_prefixes"] = {{"name", names[0]}, {"short", names[1]}, {"long", names[2]}};
    return j;
  }
};

std::string joined(const std::vector<std::string>& v) {
  std::string out;
  for (const std::string& s : v) out += (out.empty() ? "" : " ") + s;
  return out;
}

}  // namespace

X1Outputs x1_measure(const X1Set& set, const std::vector<X1Item>& items, const X1Import& import, const SpeechTap& tap) {
  std::map<std::string, const X1Item*, std::less<>> by_id;
  for (const X1Item& i : items) by_id.emplace(i.id, &i);
  std::vector<X1Measured> measured;
  measured.reserve(import.lines.size());
  for (const X1Line& l : import.lines) {
    const auto it = by_id.find(l.id);
    if (it == by_id.end()) continue;
    X1Measured m;
    m.line = &l;
    m.item = it->second;
    if (l.ok && !l.reply.empty()) {
      m.pass = run_speech(tap, it->second->context, l.reply);
      m.shown_contract_breaks = shown_breaks(m.pass.shown);
    }
    measured.push_back(std::move(m));
  }
  // The models, in the order their first result came.
  std::vector<std::pair<std::string, std::string>> models;
  for (const X1Measured& m : measured) {
    const std::pair<std::string, std::string> k{m.line->server, m.line->model};
    if (std::find(models.begin(), models.end(), k) == models.end()) models.push_back(k);
  }
  X1Outputs out;
  json summary;
  summary["schema"] = 1;
  summary["kind"] = "x1-summary";  // numbers and labels only: no line of a conversation, no prompt
  summary["set"] = set.name;
  summary["lines"] = {{"read", import.lines.size()}, {"refused", import.refused.size()}};
  json refused = json::array();
  for (const X1Refusal& r : import.refused) refused.push_back({{"file", r.file}, {"line", r.line}, {"why", r.why}});
  summary["refused"] = std::move(refused);
  const X1Sampling s = x1_sampling();
  summary["sampling"] = {{"max_tokens", s.max_tokens}, {"temperature", s.temperature}, {"thinking", false}};
  std::ostringstream printed;
  printed << "x1 " << set.name << ": " << import.lines.size() << " results read, " << import.refused.size()
          << " refused, " << models.size() << " models\n";
  json model_list = json::array();
  for (std::size_t mi = 0; mi < models.size(); ++mi) {
    const auto& [server, model] = models[mi];
    std::vector<const X1Measured*> mine;
    for (const X1Measured& m : measured) {
      if (m.line->server == server && m.line->model == model) mine.push_back(&m);
    }
    json mj;
    mj["server"] = server;
    mj["model"] = model;
    mj["runs"] = mine.size();
    std::vector<double> ttft, tps, total;
    for (const X1Measured* m : mine) {
      if (!m->line->ok) continue;
      if (m->line->first_token_s) ttft.push_back(*m->line->first_token_s);
      if (m->line->tokens_per_s) tps.push_back(*m->line->tokens_per_s);
      if (m->line->total_s) total.push_back(*m->line->total_s);
    }
    mj["first_token_s"] = quantiles(ttft);
    mj["tokens_per_s"] = quantiles(tps);
    mj["total_s"] = quantiles(total);
    // Per persona, arm and kind, in the set's order.
    json sets = json::array();
    std::vector<std::tuple<std::string, std::string, std::string, std::string>> keys;  // persona, register, arm, kind
    for (const X1Item& i : items) {
      const auto k = std::make_tuple(i.persona, i.register_name, i.arm, i.kind);
      if (std::find(keys.begin(), keys.end(), k) == keys.end()) keys.push_back(k);
    }
    Tally c10, regal;
    std::size_t questions = 0;
    std::map<std::string, std::map<std::string, json>> arms;  // persona -> arm -> measures (voice)
    for (const auto& [persona, reg, arm, kind] : keys) {
      Tally t;
      for (const X1Measured* m : mine) {
        if (m->item->persona == persona && m->item->arm == arm && m->item->kind == kind) t.add(*m);
      }
      if (t.n == 0) continue;
      json sj = t.to_json();
      sj["persona"] = persona;
      sj["register"] = reg;
      sj["arm"] = arm;
      sj["kind"] = kind;
      if (kind == "voice") arms[persona][arm] = sj;
      if (kind == "test_question") questions += t.replies;
      if (kind == "voice" && arm == "positive") {
        for (const X1Measured* m : mine) {
          if (m->item->persona != persona || m->item->arm != arm || m->item->kind != kind) continue;
          (reg == "archaic-regal" ? regal : c10).add(*m);
        }
      }
      sets.push_back(std::move(sj));
    }
    mj["sets"] = std::move(sets);
    // C10 (DESIGN §1.3) on the non-archaic personas' voice sets, the shipped wording: ≥ 98% within the word cap, ≤ 2%
    // archaic lines, 0 banned words shown (the lint holds them back: the count it caught is beside).
    const json cj = c10.to_json();
    const double within = cj["within_words_max"].get<double>();
    const double archaic = cj["archaic_lines"].get<double>();
    const bool met = c10.replies > 0 && within >= 0.98 && archaic <= 0.02;
    std::size_t expected = 0;  // the runs a whole set gives this model: every non-archaic positive voice item
    for (const X1Item& i : items) {
      if (i.kind == "voice" && i.arm == "positive" && i.register_name != "archaic-regal") {
        expected += static_cast<std::size_t>(i.repeat);
      }
    }
    mj["c10"] = {{"met", met},
                 {"complete", c10.n == expected},
                 {"runs", c10.n},
                 {"expected", expected},
                 {"replies", c10.replies},
                 {"within_words_max", within},
                 {"archaic_lines", archaic},
                 {"banned_shown", 0},
                 {"banned_caught", c10.hard[idx(core::HardList::Banned)]}};
    if (regal.n > 0) {
      const json rj = regal.to_json();
      const double share_lines = rj["archaic_lines"].get<double>();
      mj["archaic_regal"] = {{"replies", regal.replies},
                             {"archaic_lines", share_lines},
                             {"archaic_sentences", rj["archaic_sentences"]},
                             {"in_25_to_60", share_lines >= 0.25 && share_lines <= 0.60}};
    }
    json arm_list = json::array();
    for (const auto& [persona, by_arm] : arms) {
      if (by_arm.size() < 2) continue;
      json aj = {{"persona", persona}};
      for (const auto& [arm, m] : by_arm) {
        aj[arm] = {{"replies", m["replies"]},
                   {"within_words_max", m["within_words_max"]},
                   {"archaic_lines", m["archaic_lines"]},
                   {"promise_rate", m["promise_rate"]},
                   {"contract_caught_share", m["contract_caught_share"]},
                   {"name_prefixes", m["name_prefixes"]}};
      }
      arm_list.push_back(std::move(aj));
    }
    mj["arms"] = std::move(arm_list);
    mj["test_questions"] = {{"replies", questions}, {"to_score", questions}};
    printed << "  model " << mi + 1 << ": " << server << " / " << model << ": " << mine.size() << " runs; C10 "
            << (met ? "met" : "not met") << (c10.n == expected ? "" : " so far") << " (within the cap " << static_cast<int>(std::round(within * 100)) << "%, archaic "
            << static_cast<int>(std::round(archaic * 100)) << "%); " << questions << " test-question replies to score\n";
    model_list.push_back(std::move(mj));
  }
  summary["models"] = std::move(model_list);
  out.summary = std::move(summary);

  // The local report and the test questions, in the set's order of personas and turns, then by server, model, run.
  std::vector<const X1Measured*> order;
  for (const X1Measured& m : measured) order.push_back(&m);
  std::map<std::string, std::size_t> place;
  for (std::size_t i = 0; i < items.size(); ++i) place[items[i].persona + "\n" + items[i].kind + "\n" + std::to_string(items[i].turn)] = i;
  std::stable_sort(order.begin(), order.end(), [&](const X1Measured* a, const X1Measured* b) {
    const auto ka = place[a->item->persona + "\n" + a->item->kind + "\n" + std::to_string(a->item->turn)];
    const auto kb = place[b->item->persona + "\n" + b->item->kind + "\n" + std::to_string(b->item->turn)];
    return std::tie(ka, a->line->server, a->line->model, a->line->run, a->item->arm) <
           std::tie(kb, b->line->server, b->line->model, b->line->run, b->item->arm);
  });
  json report;
  report["schema"] = 1;
  report["kind"] = "x1-local";  // model text: local only, never committed or uploaded
  report["set"] = set.name;
  json rows = json::array();
  json questions;
  questions["schema"] = 1;
  questions["kind"] = "x1-questions";  // model text and the prompts: local only, for the Engine session's scoring
  questions["set"] = set.name;
  json qrows = json::array();
  for (const X1Measured* m : order) {
    const X1Line& l = *m->line;
    const X1Item& i = *m->item;
    json r;
    r["id"] = l.id;
    r["run"] = l.run;
    r["server"] = l.server;
    r["model"] = l.model;
    r["persona"] = i.persona;
    r["register"] = i.register_name;
    r["arm"] = i.arm;
    r["kind"] = i.kind;
    r["scenario"] = i.scenario;
    r["turn"] = i.turn;
    r["state"] = i.state;
    r["player"] = i.player;
    r["ok"] = l.ok;
    r["reply"] = l.reply;
    r["shown"] = joined(m->pass.shown);
    r["words_max"] = i.context.words_max;
    r["model_words"] = m->pass.model_words;
    r["cut_by_pager"] = m->pass.cut_by_pager;
    r["stopped"] = m->pass.stopped ? action_name(*m->pass.stopped) : "none";
    r["hit"] = m->pass.hit ? hard_name(idx(*m->pass.hit)) : "none";
    r["speech"] = counts_json(m->pass.counts);
    r["contract_shown"] = m->shown_contract_breaks;
    r["finish_reason"] = l.finish_reason ? json(*l.finish_reason) : json(nullptr);
    r["first_token_s"] = l.first_token_s ? json(*l.first_token_s) : json(nullptr);
    r["total_s"] = l.total_s ? json(*l.total_s) : json(nullptr);
    r["tokens_per_s"] = l.tokens_per_s ? json(std::round(*l.tokens_per_s * 100.0) / 100.0) : json(nullptr);
    rows.push_back(r);
    if (i.kind == "test_question") {
      json q;
      q["id"] = l.id;
      q["run"] = l.run;
      q["server"] = l.server;
      q["model"] = l.model;
      q["persona"] = i.persona;
      q["state"] = i.state;
      q["topics"] = i.topics;
      q["question"] = i.player;
      q["messages"] = i.messages;
      q["ok"] = l.ok;
      q["reply"] = l.reply;
      q["shown"] = joined(m->pass.shown);
      q["finish_reason"] = r["finish_reason"];
      q["leak"] = nullptr;  // the Engine session's score: true, false, with a note
      qrows.push_back(std::move(q));
    }
  }
  report["results"] = std::move(rows);
  questions["replies"] = std::move(qrows);
  out.report = std::move(report);
  out.questions = std::move(questions);
  for (const X1Refusal& r : import.refused) {
    printed << "  refused: file " << r.file << ", line " << r.line << ": " << r.why << "\n";
  }
  out.printed = printed.str();
  return out;
}

}  // namespace en::tools
