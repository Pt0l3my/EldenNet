// The eval's runner (eval_run.hpp; DESIGN §14, NPC §12.1, ARCH §12.2, §12.3).
#include "eval_run.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <variant>

#include <toml.hpp>

#include "en/augur/audience.hpp"
#include "en/core/core.hpp"
#include "en/core/facts.hpp"
#include "en/core/speech_content.hpp"
#include "en/store/playthrough.hpp"
#include "en/store/store.hpp"
#include "en/proto/toml_check.hpp"
#include "golden_run.hpp"
#include "prompt_fixture.hpp"  // find_flag_state

namespace en::tools {

namespace fs = std::filesystem;
using core::Mono;

namespace {

std::string at(const toml::node& n, const std::string& what) {
  return "line " + std::to_string(n.source().begin.line) + ": " + what;
}

// A plain relative path inside its folder: no root, no "..".
bool inside(const fs::path& p) {
  if (p.empty() || p.is_absolute() || p.has_root_name()) return false;
  for (const fs::path& part : p) {
    if (part == "..") return false;
  }
  return true;
}

constexpr std::array<std::string_view, 17> kStepKinds = {
    "open",        "say",    "wait",    "esc",    "skip", "move", "expect", "flags",    "restart",
    "native_talk", "strike", "ng_plus", "reload", "kill", "load", "choose", "wait_jobs"};

}  // namespace

proto::Expected<EvalScenario, std::string> parse_eval_scenario(std::string_view text, const fs::path& folder) {
  if (const auto unsafe = proto::unsafe_for_toml(text, proto::TomlText::Player)) {
    return proto::Unexpected{"line " + std::to_string(unsafe->line) + ": refused before it is parsed: " + unsafe->problem};
  }
  toml::parse_result parsed = toml::parse(text);
  if (!parsed) {
    return proto::Unexpected{"line " + std::to_string(parsed.error().source().begin.line) +
                             ": not valid TOML: " + std::string(parsed.error().description())};
  }
  const toml::table& root = parsed.table();
  constexpr std::array<std::string_view, 10> kTop = {"schema", "name",     "persona", "overrides", "mock",
                                                     "recorded", "world", "step",    "store",     "character"};
  for (auto&& [k, v] : root) {
    if (std::find(kTop.begin(), kTop.end(), k.str()) == kTop.end()) return proto::Unexpected{at(v, "an unknown key")};
  }
  if (root["schema"].value_or(std::int64_t{0}) != 1) return proto::Unexpected{std::string("schema must be 1")};
  EvalScenario s;
  s.folder = folder;
  s.name = root["name"].value_or(std::string());
  s.persona = root["persona"].value_or(std::string());
  if (s.name.empty() || s.persona.empty()) return proto::Unexpected{std::string("name and persona are needed")};
  s.store = root["store"].value_or(false);
  s.character = root["character"].value_or(std::string("Vessel"));
  if (s.character.empty() || s.character.size() > 16) return proto::Unexpected{std::string("character: 1 to 16 bytes")};
  for (const char* key : {"overrides", "mock", "recorded"}) {
    if (const auto v = root[key].value<std::string>()) {
      if (!inside(fs::path(*v))) return proto::Unexpected{std::string(key) + " must be a path inside the scenario's folder"};
      std::optional<std::string>& slot = std::string_view(key) == "overrides" ? s.overrides
                                         : std::string_view(key) == "mock"    ? s.mock
                                                                              : s.recorded;
      slot = *v;
    }
  }
  if (const toml::table* w = root["world"].as_table()) {
    constexpr std::array<std::string_view, 7> kWorld = {"param", "distance_m", "bearing_deg", "clock",
                                                        "subtitles", "flags", "extra"};
    for (auto&& [k, v] : *w) {
      if (std::find(kWorld.begin(), kWorld.end(), k.str()) == kWorld.end()) return proto::Unexpected{at(v, "world: an unknown key")};
    }
    s.param = static_cast<std::uint32_t>((*w)["param"].value_or(std::int64_t{0}));
    s.distance_m = static_cast<float>((*w)["distance_m"].value_or(2.5));
    s.bearing_deg = static_cast<float>((*w)["bearing_deg"].value_or(0.0));
    s.subtitles = (*w)["subtitles"].value_or(true);
    if (const auto f = (*w)["flags"].value<std::string>()) s.flags = *f;
    if (const toml::array* ex = (*w)["extra"].as_array()) {
      if (!ex->is_array_of_tables() || ex->size() > 8) {
        return proto::Unexpected{std::string("world.extra: at most 8 tables")};
      }
      for (const toml::node& n : *ex) {
        const toml::table& e = *n.as_table();
        EvalScenario::Extra x;
        x.param = static_cast<std::uint32_t>(e["param"].value_or(std::int64_t{0}));
        x.distance_m = static_cast<float>(e["distance_m"].value_or(10.0));
        x.bearing_deg = static_cast<float>(e["bearing_deg"].value_or(0.0));
        if (x.param == 0) return proto::Unexpected{at(e, "world.extra: param is needed")};
        s.extras.push_back(x);
      }
    }
    if (const toml::array* c = (*w)["clock"].as_array(); c != nullptr && c->size() == 2) {
      s.clock_h = static_cast<std::uint8_t>((*c)[0].value_or(std::int64_t{12}) % 24);
      s.clock_m = static_cast<std::uint8_t>((*c)[1].value_or(std::int64_t{0}) % 60);
    }
  }
  if (s.param == 0) return proto::Unexpected{std::string("world.param, the persona's invented row, is needed")};
  const toml::array* steps = root["step"].as_array();
  if (steps == nullptr || !steps->is_array_of_tables()) return proto::Unexpected{std::string("the scenario needs [[step]]s")};
  constexpr std::array<std::string_view, 27> kStep = {
      "do",       "text",          "ms",           "distance_m",  "state",    "box",    "ended",
      "last_npc", "toast",         "npc_lines",    "player_lines", "failures", "deflections",
      "regenerations", "cancels",  "note",         "at",          "flags",    "prompt_has",
      "prompt_lacks",  "name_prefixes", "who",     "pt",          "choice",   "mode",   "fact",
      "audience"};
  for (std::size_t i = 0; i < steps->size(); ++i) {
    const toml::table& t = *steps->get(i)->as_table();
    const std::string where = "step[" + std::to_string(i) + "]";
    for (auto&& [k, v] : t) {
      if (std::find(kStep.begin(), kStep.end(), k.str()) == kStep.end()) return proto::Unexpected{at(v, where + ": an unknown key")};
    }
    EvalStep st;
    st.line = static_cast<std::size_t>(t.source().begin.line);
    const std::string kind = t["do"].value_or(std::string());
    const auto k = std::find(kStepKinds.begin(), kStepKinds.end(), kind);
    if (k == kStepKinds.end()) {
      return proto::Unexpected{
          at(t, where + ".do: must be one of open, say, wait, esc, skip, move, expect, flags, restart, native_talk, "
                        "strike, ng_plus, reload, kill, load, choose, wait_jobs")};
    }
    st.kind = static_cast<EvalStep::Kind>(k - kStepKinds.begin());
    st.text = t["text"].value_or(std::string());
    st.ms = static_cast<Mono>(std::max<std::int64_t>(0, t["ms"].value_or(std::int64_t{0})));
    st.distance_m = static_cast<float>(t["distance_m"].value_or(0.0));
    for (const char* key : {"state", "box", "ended", "last_npc", "toast"}) {
      if (const auto v = t[key].value<std::string>()) {
        const std::string_view kk = key;
        (kk == "state" ? st.state : kk == "box" ? st.box : kk == "ended" ? st.ended : kk == "last_npc" ? st.last_npc : st.toast) = *v;
      }
    }
    for (const char* key : {"npc_lines", "player_lines", "failures", "deflections", "regenerations", "cancels",
                            "name_prefixes"}) {
      if (const auto v = t[key].value<std::int64_t>()) {
        const std::string_view kk = key;
        (kk == "npc_lines"       ? st.npc_lines
         : kk == "player_lines"  ? st.player_lines
         : kk == "failures"      ? st.failures
         : kk == "deflections"   ? st.deflections
         : kk == "regenerations" ? st.regenerations
         : kk == "cancels"       ? st.cancels
                                 : st.name_prefixes) = static_cast<std::size_t>(std::max<std::int64_t>(0, *v));
      }
    }
    if (st.kind == EvalStep::Kind::Say && st.text.empty()) return proto::Unexpected{at(t, where + ": say needs a text")};
    st.who = static_cast<std::size_t>(std::max<std::int64_t>(0, t["who"].value_or(std::int64_t{0})));
    if (st.who > s.extras.size()) return proto::Unexpected{at(t, where + ".who: no such extra")};
    st.pt = std::max<std::int64_t>(0, t["pt"].value_or(std::int64_t{0}));
    if (st.kind == EvalStep::Kind::Load && st.pt == 0) return proto::Unexpected{at(t, where + ": load needs a pt")};
    st.choice = t["choice"].value_or(std::string());
    if (st.kind == EvalStep::Kind::Choose && st.choice != "forget" && st.choice != "keep" && st.choice != "new") {
      return proto::Unexpected{at(t, where + ".choice: must be forget, keep or new")};
    }
    if (st.kind == EvalStep::Kind::NativeTalk && st.ms == 0) st.ms = 3000;
    if (const auto m = t["mode"].value<std::string>()) st.mode = *m;
    if (const toml::table* f = t["fact"].as_table()) {
      st.fact = EvalStep::Fact{(*f)["persona"].value_or(std::string()), (*f)["key"].value_or(std::string()),
                               (*f)["value"].value_or(std::string())};
      if (st.fact->persona.empty() || st.fact->key.empty()) {
        return proto::Unexpected{at(t, where + ".fact: persona and key are needed")};
      }
    }
    if (const toml::table* a = t["audience"].as_table()) {
      st.audience = EvalStep::Audience{(*a)["kind"].value_or(std::string()), (*a)["persona"].value_or(std::string()),
                                       (*a)["how"].value_or(std::string()),
                                       static_cast<std::size_t>(std::max<std::int64_t>(0, (*a)["count"].value_or(
                                                                                          std::int64_t{0})))};
      if (st.audience->kind.empty() || st.audience->persona.empty() || !proto::how_from(st.audience->how)) {
        return proto::Unexpected{at(t, where + ".audience: kind, persona and how (direct, overheard, witnessed)")};
      }
    }
    st.flags = t["flags"].value_or(std::string());
    if (st.kind == EvalStep::Kind::Flags && st.flags.empty()) return proto::Unexpected{at(t, where + ": flags needs a state's name")};
    for (const char* key : {"prompt_has", "prompt_lacks"}) {
      if (!t.contains(key)) continue;
      const toml::array* a = t[key].as_array();
      if (a == nullptr) return proto::Unexpected{at(t, where + "." + key + ": must be an array of strings")};
      for (const toml::node& n : *a) {
        const auto v = n.value<std::string>();
        if (!v || v->empty()) return proto::Unexpected{at(t, where + "." + key + ": must be an array of strings")};
        (std::string_view(key) == "prompt_has" ? st.prompt_has : st.prompt_lacks).push_back(*v);
      }
    }
    s.steps.push_back(std::move(st));
  }
  return s;
}

proto::Expected<std::vector<core::ContentFile>, std::string> read_content_folder(const fs::path& root,
                                                                                 core::ContentOrigin origin) {
  std::vector<core::ContentFile> out;
  std::error_code ec;
  if (!fs::is_directory(root, ec)) return proto::Unexpected{std::string("the content folder cannot be read")};
  std::vector<fs::path> files;
  for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
    const fs::path ext = it->path().extension();  // records and lexicons, and the prompts' templates
    if (it->is_regular_file(ec) && (ext == ".toml" || ext == ".md" || ext == ".j2")) files.push_back(it->path());
  }
  if (ec) return proto::Unexpected{std::string("the content folder cannot be listed")};
  std::sort(files.begin(), files.end());
  for (const fs::path& f : files) {
    const fs::path rel = fs::relative(f, root, ec);
    if (ec || !inside(rel)) return proto::Unexpected{std::string("a content file lies outside its folder")};
    auto text = read_file(f.string(), core::kContentFileMaxBytes);
    if (!text) return proto::Unexpected{"a content file cannot be read, or is larger than 64 KB: " + rel.generic_string()};
    out.push_back(core::ContentFile{rel.generic_string(), std::move(*text), origin});
  }
  return out;
}

namespace {

constexpr proto::ChrHandle kPlayer{1};
constexpr proto::ChrHandle kPartner{0x500};
constexpr std::uint64_t kEvalAccount = 76561190000000009ull;  // invented

const char* mode_word(core::StorePath::Mode m) {
  switch (m) {
    case core::StorePath::Mode::Off:
      return "off";
    case core::StorePath::Mode::Opening:
      return "opening";
    case core::StorePath::Mode::Choice:
      return "choice";
    case core::StorePath::Mode::Open:
      return "open";
    case core::StorePath::Mode::Down:
      return "down";
  }
  return "off";
}

// The run's own folder for the store, removed after it.
class RunFolder {
 public:
  explicit RunFolder(const std::optional<fs::path>& kept) : keep_(kept.has_value()) {
    std::random_device rd;
    path_ = kept ? *kept
                 : fs::temp_directory_path() / ("en-eval-store-" + std::to_string(rd()) + "-" + std::to_string(rd()));
    fs::create_directories(path_);
  }
  ~RunFolder() {
    if (keep_) return;  // the oracle's: the caller reads it, and removes it
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  RunFolder(const RunFolder&) = delete;
  RunFolder& operator=(const RunFolder&) = delete;
  [[nodiscard]] const fs::path& path() const { return path_; }

 private:
  fs::path path_;
  bool keep_ = false;
};

class Runner {
 public:
  Runner(const EvalScenario& s, EvalLlm llm, const EvalOptions& options) : s_(s), llm_(llm), options_(options) {}

  proto::Expected<EvalRun, std::string> run() {
    run_.scenario = s_.name;
    run_.persona = s_.persona;
    run_.llm = llm_ == EvalLlm::Mock ? "mock" : "recorded";
    files_ = core::builtin_content();
    if (s_.overrides) {
      auto extra = read_content_folder(s_.folder / *s_.overrides, core::ContentOrigin::Override);
      if (!extra) return proto::Unexpected{"overrides: " + extra.error()};
      for (core::ContentFile& f : *extra) files_.push_back(std::move(f));
    }
    if (options_.arm) {
      auto extra = read_content_folder(*options_.arm, core::ContentOrigin::Override);
      if (!extra) return proto::Unexpected{"the arm: " + extra.error()};
      for (core::ContentFile& f : *extra) files_.push_back(std::move(f));
    }
    // The speech the core will run, loaded from the same pack, so that the tap can wrap it (Core(SpeechParts)).
    auto pack = core::load_content(files_);
    if (!pack) return proto::Unexpected{"the content does not load: " + core::describe(pack.error().front())};
    auto speech = core::load_speech(*pack);
    if (!speech) return proto::Unexpected{"the content does not load: " + core::describe(speech.error().front())};
    tap_ = std::make_unique<SpeechTap>(speech->lint, speech->prompt);
    if (s_.store) {
      folder_ = std::make_unique<RunFolder>(options_.store_dir);
      new_store();
    }
    extra_dist_.clear();
    for (const EvalScenario::Extra& x : s_.extras) extra_dist_.push_back(x.distance_m);
    if (auto loaded = fresh_core(); !loaded.empty()) return proto::Unexpected{std::move(loaded)};
    if (s_.flags) {
      auto f = flag_state(*s_.flags);
      if (!f) return proto::Unexpected{f.error()};
      flags_ = std::move(*f);
    }
    const auto persona = core_->directory().find(s_.persona);
    if (!persona) return proto::Unexpected{std::string("the scenario's persona is not in the content")};
    persona_ = &core_->directory().persona(*persona);
    run_.words_max = persona_->reply_words_max;
    run_.register_name = core::to_string(persona_->reg);
    if (const auto st = persona_->record.find("stages"); st != persona_->record.end() && st->is_array()) {
      for (const nlohmann::json& stage : *st) {
        const auto f = stage.find("forbid");
        if (f == stage.end() || !f->is_array()) continue;
        for (const nlohmann::json& topic : *f) {
          if (topic.is_string() && std::find(run_.topics.begin(), run_.topics.end(), topic.get<std::string>()) ==
                                       run_.topics.end()) {
            run_.topics.push_back(topic.get<std::string>());
          }
        }
      }
    }
    flags_name_ = s_.flags.value_or(std::string());
    if (llm_ == EvalLlm::Mock) {
      if (!s_.mock) return proto::Unexpected{std::string("the scenario names no mock script")};
      const auto text = read_file((s_.folder / *s_.mock).string(), 256 * 1024);
      if (!text) return proto::Unexpected{std::string("the mock script cannot be read")};
      auto script = parse_mock_script(*text);
      if (!script) return proto::Unexpected{"the mock script: " + script.error()};
      model_.emplace<MockModel>(std::move(*script));
    } else {
      if (!s_.recorded) return proto::Unexpected{std::string("the scenario names no recorded events")};
      const auto text = read_file((s_.folder / *s_.recorded).string(), 16u << 20);
      if (!text) return proto::Unexpected{std::string("the recorded events cannot be read")};
      auto events = parse_recorded(*text);
      if (!events) return proto::Unexpected{"the recorded events: " + events.error()};
      model_.emplace<RecordedModel>(std::move(*events));
    }
    if (options_.start_pt) pt_base_ = static_cast<proto::Pt>(*options_.start_pt);
    if (auto booted = boot(); !booted.empty()) return proto::Unexpected{std::move(booted)};
    for (const EvalStep& st : s_.steps) {
      run_.step_pt.push_back(static_cast<std::int64_t>(play_ms()));
      if (st.kind == EvalStep::Kind::Restart) {
        if (auto loaded = fresh_core(); !loaded.empty()) return proto::Unexpected{std::move(loaded)};
        if (auto booted = boot(); !booted.empty()) return proto::Unexpected{std::move(booted)};
        continue;
      }
      if (st.kind == EvalStep::Kind::Reload || st.kind == EvalStep::Kind::Kill || st.kind == EvalStep::Kind::Load) {
        if (!store_) {
          run_.failed.push_back("line " + std::to_string(st.line) + ": needs store = true");
          continue;
        }
        if (st.kind != EvalStep::Kind::Kill) to_title();  // a kill closes nothing: the file is left as it was
        if (st.kind == EvalStep::Kind::Load) {
          pt_base_ = st.pt;  // the same character, an older save: the core opens the same file
          t_base_ = t_;
          if (!st.flags.empty()) {  // and its world as that save holds it
            auto f = flag_state(st.flags);
            if (!f) return proto::Unexpected{f.error()};
            flags_ = std::move(*f);
            flags_name_ = st.flags;
          }
        } else {
          new_store();
          if (auto loaded = fresh_core(); !loaded.empty()) return proto::Unexpected{std::move(loaded)};
          pt_base_ = static_cast<proto::Pt>(play_ms());  // the save as it was left
          t_base_ = t_;
        }
        if (auto booted = boot(); !booted.empty()) return proto::Unexpected{std::move(booted)};
        continue;
      }
      do_step(st);
    }
    finish();
    return std::move(run_);
  }

 private:
  // A core of its own over the tap's parts, the content loaded (the first, or a restart's: a first meeting again).
  std::string fresh_core() {
    core_.reset();
    core_ = std::make_unique<core::Core>(tap_->parts());
    if (const auto errors = core_->load_content(files_, core::Realm::Eval); !errors.empty()) {
      return "the content does not load: " + core::describe(errors.front());
    }
    watch_.reset();
    replies_.clear();
    ack_.reset();
    acked_.reset();
    failure_page_.reset();
    box_open_ = shut_ = closing_ = false;
    ui_ = proto::UiModel{};
    if (persona_ != nullptr) {
      const auto persona = core_->directory().find(s_.persona);
      persona_ = persona ? &core_->directory().persona(*persona) : nullptr;
    }
    return {};
  }

  // A store thread of the session's own on the run's folder (T9 is the process's: a reload or a kill starts another).
  void new_store() {
    store_.reset();
    store::StoreCfg c;
    c.saves = folder_->path() / "saves";
    c.build = "eval";
    store_ = std::make_unique<store::StoreCore>(std::move(c));
  }

  // The play time the snapshots carry: the save's, from when it was loaded.
  [[nodiscard]] std::uint64_t play_ms() const { return static_cast<std::uint64_t>(pt_base_ + (t_ - t_base_)); }

  // Back to the title, as the game shows it: the core closes the playthrough's file.
  void to_title() {
    t_ += 250;
    proto::WorldSnapshot s = snapshot();
    s.life.main_player = false;
    s.life.screen = proto::Screen::Title;
    s.player.reset();
    s.chars.clear();
    feed(core::CoreInput{std::make_unique<proto::WorldSnapshot>(std::move(s))});
  }

  // Boot: T1 ready, a healthy model, a playthrough, and the world until the gate has held and the observer allows.
  std::string boot() {
    const Mono from = t_;
    feed(core::CoreInput{proto::Lifecycle{proto::Lifecycle::DataReady{proto::BuildKey{0}, all_caps()}}});
    feed(core::CoreInput{proto::NetHealth{true}});
    if (store_) feed(core::CoreInput{core::CoreInput::Account{kEvalAccount}});
    proto::Lifecycle::PlaythroughReady p;
    p.slot = 2;
    (void)p.name.try_assign(s_.character);  // invented; it stays in the core
    p.play_time_ms = static_cast<std::uint64_t>(pt_base_);
    feed(core::CoreInput{proto::Lifecycle{p}});
    while (!(core_->state().phase == core::Phase::InWorld && core_->state().gate_open)) {
      if (t_ - from > 30000) return "the world never became ready";
      advance(250);
    }
    advance(3500);
    return {};
  }
  static proto::Capabilities all_caps() {
    proto::Capabilities c;
    c.groups = proto::FlagSetTraits<proto::GroupSet>::all;
    return c;
  }

  // A fixture flag state by its name, through the loaded content's catalogue (MEM §7.8).
  proto::Expected<core::FlagState, std::string> flag_state(const std::string& name) const {
    const auto path = find_flag_state(s_.folder, name);
    if (!path) return proto::Unexpected{std::string("no fixture flag state of that name above the scenario")};
    const auto text = read_file(path->string(), 64 * 1024);
    if (!text) return proto::Unexpected{std::string("the fixture flag state cannot be read")};
    auto fs = core_->knowledge().fixture(*text);
    if (!fs) return proto::Unexpected{"the fixture flag state: " + fs.error()};
    return std::move(*fs);
  }

  // The game task's answer to the core's watch list: each watched id's bit as the state has its alias, every id valid.
  std::optional<proto::FlagFrame> frame() const {
    if (!watch_ || !flags_) return std::nullopt;
    const core::WatchList& list = core_->watch_list();
    if (list.version != watch_->version) return std::nullopt;
    proto::FlagFrame f;
    f.watch_version = list.version;
    for (std::size_t b = 0; b < list.ids.size() && b < proto::kMaxWatchFlags; ++b) {
      f.valid[b] = true;
      f.bits[b] = list.flags[b] < flags_->bits.size() && flags_->bits[list.flags[b]] != 0;
    }
    return f;
  }

  proto::WorldSnapshot snapshot() const {
    proto::WorldSnapshot s;
    if (seq_ % 4 == 0) s.flags = frame();  // every fourth snapshot, as the task reads them (ARCH §6)
    s.seq = seq_;
    s.mono_ms = t_;
    for (auto& g : s.health.groups) {
      g.state = proto::GroupState::Ok;
      g.phase_b = true;
    }
    s.session.role = proto::MultiplayRole::Host;
    s.session.is_my_world = true;
    s.session.lobby = proto::LobbyState::None;
    s.session.player_set_len = 1;
    s.life.main_player = true;
    s.life.screen = proto::Screen::InGame;
    s.life.load_counter = 7;
    s.ui.hud = proto::HudState::Default;
    s.ui.subtitles_on = s_.subtitles;
    proto::Player p;
    p.handle = kPlayer;
    p.save_slot = 2;
    p.play_time_ms = play_ms();
    p.ng_lvl = ng_;
    p.hp = proto::Meter{420, 420};
    s.player = p;
    if (talk_until_ && t_ < *talk_until_) s.ui.popup_talk_id = 1;
    s.clock = proto::Clock{s_.clock_h, s_.clock_m};
    proto::Chr c;
    c.handle = kPartner;
    c.param = s_.param;
    c.dist_m = distance_;
    c.bearing_deg = s_.bearing_deg;
    c.hp = proto::Meter{partner_hp_, 100};
    c.on_screen = true;
    if (strike_ && *strike_ == seq_) c.last_hit_by = kPlayer;
    (void)s.chars.try_push_back(c);
    for (std::size_t i = 0; i < s_.extras.size(); ++i) {
      proto::Chr x;
      x.handle = proto::ChrHandle{0x601 + i};
      x.param = s_.extras[i].param;
      x.dist_m = extra_dist_[i];
      x.bearing_deg = s_.extras[i].bearing_deg;
      x.hp = proto::Meter{100, 100};
      x.on_screen = true;
      (void)s.chars.try_push_back(x);
    }
    return s;
  }

  void feed(core::CoreInput in) {
    const core::Outputs o = core_->step(std::move(in), t_);
    absorb(o);
    // The store's thread, answering each request in order as it arrives (T9); each answer is the core's next input.
    if (store_ && !o.db.empty()) {
      std::vector<proto::DbResult> answers;
      for (const proto::DbRequest& r : o.db) answers.push_back(store_->handle(r));
      for (proto::DbResult& r : answers) feed(core::CoreInput{std::move(r)});
    }
    // The attempts the conversation opened in this step belong to the line in flight: the newest, its PlayerSaid
    // committed in this step or before.
    for (; seen_attempts_ < tap_->attempts().size(); ++seen_attempts_) {
      if (!run_.turns.empty()) run_.turns.back().attempts.push_back(seen_attempts_);
    }
  }

  template <class F>
  void with_model(F&& f) {
    std::visit([&](auto& m) {
      if constexpr (!std::is_same_v<std::decay_t<decltype(m)>, std::monostate>) f(m);
    }, model_);
  }

  std::optional<Mono> model_next() {
    std::optional<Mono> at;
    with_model([&](auto& m) { at = m.next_at(); });
    return at;
  }

  void absorb(const core::Outputs& o) {
    for (const proto::GameCommand& c : o.game) {
      if (const auto* w = std::get_if<proto::GameCommand::SetWatchFlags>(&c.payload)) watch_ = *w;
    }
    for (const core::Commit& c : o.commits) {
      switch (c.kind) {
        case core::Commit::Kind::PlayerSaid:
          run_.turns.push_back(EvalTurn{c.text, "", 0, false, false, false, 0, 0, std::nullopt, {}, {}, flags_name_});
          said_at_.push_back(c.at);
          break;
        case core::Commit::Kind::NpcSaid:
          for (std::size_t i = run_.turns.size(); i-- > 0;) {
            if (run_.turns[i].pages == 0) {
              run_.turns[i].reply = c.text;
              run_.turns[i].pages = 1;
              run_.turns[i].first_page_ms = c.at - said_at_[i];
              replies_[{c.conv, c.reply}] = i;
              break;
            }
          }
          break;
        case core::Commit::Kind::NpcPage:
          if (const auto it = replies_.find({c.conv, c.reply}); it != replies_.end()) {
            run_.turns[it->second].reply += " " + c.text;
            ++run_.turns[it->second].pages;
          }
          break;
        case core::Commit::Kind::CutOff:
          if (const auto it = replies_.find({c.conv, c.reply}); it != replies_.end()) run_.turns[it->second].cut_off = true;
          break;
        default:
          break;
      }
    }
    for (const proto::LlmRequest& r : o.requests) {
      const auto* chat = std::get_if<proto::ChatRequest>(&r.body);
      // The memory job's: the model answers it, and no turn counts it.
      if (chat != nullptr && chat->slot != proto::kDialogue) {
        with_model([&](auto& m) { m.submit(r, t_); });
        ++run_.background;
        continue;
      }
      if (chat != nullptr) {
        last_prompt_.clear();
        for (const proto::Message& m : chat->messages) last_prompt_ += m.joined() + "\n";
        if (!run_.turns.empty() && run_.turns.back().prompt.empty()) run_.turns.back().prompt = chat->messages;
      }
      with_model([&](auto& m) { m.submit(r, t_); });
      ++run_.requests;
      if (!run_.turns.empty()) ++run_.turns.back().requests;
    }
    for (const proto::RequestId id : o.cancels) {
      with_model([&](auto& m) { m.cancel(id, t_); });
      ++run_.cancels;
    }
    // A "…" page of its own for each failure, by the reply it stands for.
    if (o.ui.page && o.ui.page->kind == proto::PageKind::Failure && o.ui.page->id != failure_page_) {
      failure_page_ = o.ui.page->id;
      if (!run_.turns.empty()) run_.turns.back().failure = true;
    }
    for (const proto::Toast& toast : o.ui.toasts) {
      if (std::find(run_.toasts.begin(), run_.toasts.end(), toast.text) == run_.toasts.end()) run_.toasts.push_back(toast.text);
    }
    if (o.ended) run_.endings.emplace_back(core::to_string(*o.ended));
    // The surfaces' stand-in: a speech page shows on the next frame; a box the core closes answers Shut.
    if (o.ui.page && o.ui.page->kind == proto::PageKind::Speech && o.ui.page->id != acked_ &&
        (!ack_ || ack_->first != o.ui.page->id)) {
      ack_ = std::make_pair(o.ui.page->id, t_ + 16);
    }
    if (box_open_ && o.ui.chat.view != proto::BoxView::Open && !closing_) shut_ = true;
    box_open_ = o.ui.chat.view == proto::BoxView::Open;
    ui_ = o.ui;
  }

  // Time passes to t + ms: snapshots every 250 ms, ticks every 100 ms, the model's events and the surfaces' acks at
  // their own times.
  void advance(Mono ms) {
    const Mono until = t_ + ms;
    for (;;) {
      Mono next = std::min(next_snapshot_, next_tick_);
      if (const auto m = model_next()) next = std::min(next, *m);
      if (ack_) next = std::min(next, ack_->second);
      if (next > until) break;
      t_ = std::max(t_, next);
      if (next_snapshot_ <= t_) {
        next_snapshot_ += 250;
        ++seq_;
        feed(core::CoreInput{std::make_unique<proto::WorldSnapshot>(snapshot())});
      }
      std::vector<proto::LlmEvent> due;
      with_model([&](auto& m) { due = m.due(t_); });
      for (proto::LlmEvent& e : due) feed(core::CoreInput{std::move(e)});
      if (ack_ && ack_->second <= t_) {
        const proto::PageId id = ack_->first;
        ack_.reset();
        acked_ = id;
        feed(core::CoreInput{proto::UiEvent{proto::UiEvent::PageShown{id, t_}}});
      }
      if (shut_) {
        shut_ = false;
        feed(core::CoreInput{proto::UiEvent{proto::UiEvent::Close{proto::CloseReason::Shut, "", ""}}});
      }
      if (next_tick_ <= t_) {
        next_tick_ += 100;
        feed(core::CoreInput{core::CoreInput::Tick{}});
      }
    }
    t_ = until;
  }

  void ui(proto::UiEvent e) { feed(core::CoreInput{std::move(e)}); }

  void do_step(const EvalStep& st) {
    switch (st.kind) {
      case EvalStep::Kind::Open:
        ui(proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}});
        return;
      case EvalStep::Kind::Say:
        ui(proto::UiEvent{proto::UiEvent::Submit{st.text}});
        return;
      case EvalStep::Kind::Wait:
        advance(st.ms);
        return;
      case EvalStep::Kind::Esc:
        closing_ = true;
        ui(proto::UiEvent{proto::UiEvent::Close{proto::CloseReason::Escape, "", ""}});
        closing_ = false;
        return;
      case EvalStep::Kind::Skip:
        if (ui_.page && ui_.page->kind == proto::PageKind::Speech) ui(proto::UiEvent{proto::UiEvent::Skip{ui_.page->id}});
        return;
      case EvalStep::Kind::Move:
        if (st.who == 0) {
          distance_ = st.distance_m;
        } else {
          extra_dist_[st.who - 1] = st.distance_m;
        }
        return;
      case EvalStep::Kind::NativeTalk:
        talk_until_ = t_ + st.ms;
        advance(st.ms + 1000);
        talk_until_.reset();
        return;
      case EvalStep::Kind::Strike:
        partner_hp_ = partner_hp_ > 10 ? partner_hp_ - 10 : 1;
        strike_ = seq_ + 1;
        advance(1000);
        return;
      case EvalStep::Kind::NgPlus:
        ++ng_;
        advance(1000);
        return;
      case EvalStep::Kind::Reload:
      case EvalStep::Kind::Kill:
      case EvalStep::Kind::Load:  // run() starts what follows: the title, a fresh core or store, the boot
        return;
      case EvalStep::Kind::Choose:
        feed(core::CoreInput{core::CoreInput::MemoryChoice{st.choice == "keep"  ? proto::db::Choice::Keep
                                                           : st.choice == "new" ? proto::db::Choice::NewCharacter
                                                                                : proto::db::Choice::Forget}});
        advance(500);
        return;
      case EvalStep::Kind::WaitJobs: {  // until no memory job is queued or in flight, two minutes at most
        const Mono until = t_ + 120000;
        advance(250);
        const auto busy = [&] {
          return core_->memory_busy() || !core_->store().runnable().empty() || core_->lived_waiting();
        };
        while (t_ < until && busy()) advance(250);
        if (busy()) {
          run_.failed.push_back("line " + std::to_string(st.line) + ": the memory jobs did not finish in 2 min");
        }
        return;
      }
      case EvalStep::Kind::Expect:
        expect(st);
        return;
      case EvalStep::Kind::Restart:  // run() starts the fresh core
        return;
      case EvalStep::Kind::Flags:
        if (auto f = flag_state(st.flags)) {
          flags_ = std::move(*f);
          flags_name_ = st.flags;
        } else {
          run_.failed.push_back("line " + std::to_string(st.line) + ": " + f.error());
        }
        return;
    }
  }

  void check(const EvalStep& st, const char* what, const std::string& got, const std::string& want) {
    if (got != want) {
      run_.failed.push_back("line " + std::to_string(st.line) + ": " + what + " is " + got + ", expected " + want);
    }
  }

  void expect(const EvalStep& st) {
    settle();
    if (st.state) check(st, "state", proto::to_string(ui_.chat.state), *st.state);
    if (st.box) check(st, "box", proto::to_string(ui_.chat.view), *st.box);
    if (st.ended) check(st, "ended", run_.endings.empty() ? "none" : run_.endings.back(), *st.ended);
    if (st.last_npc) {
      std::string last;
      for (const EvalTurn& t : run_.turns) {
        if (!t.reply.empty()) last = t.reply;
      }
      if (last != *st.last_npc) run_.failed.push_back("line " + std::to_string(st.line) + ": last_npc differs");  // no text
    }
    if (st.toast && std::find(run_.toasts.begin(), run_.toasts.end(), *st.toast) == run_.toasts.end()) {
      run_.failed.push_back("line " + std::to_string(st.line) + ": the toast was never up");
    }
    const auto count = [&](auto pred) {
      return std::to_string(std::count_if(run_.turns.begin(), run_.turns.end(), pred));
    };
    if (st.npc_lines) check(st, "npc_lines", count([](const EvalTurn& t) { return !t.reply.empty(); }), std::to_string(*st.npc_lines));
    if (st.player_lines) check(st, "player_lines", std::to_string(run_.turns.size()), std::to_string(*st.player_lines));
    if (st.failures) check(st, "failures", count([](const EvalTurn& t) { return t.failure; }), std::to_string(*st.failures));
    if (st.deflections) check(st, "deflections", count([](const EvalTurn& t) { return t.deflection; }), std::to_string(*st.deflections));
    if (st.regenerations) {
      std::size_t n = 0;
      for (const EvalTurn& t : run_.turns) n += t.requests > 1 ? t.requests - 1 : 0;
      check(st, "regenerations", std::to_string(n), std::to_string(*st.regenerations));
    }
    if (st.cancels) check(st, "cancels", std::to_string(run_.cancels), std::to_string(*st.cancels));
    if (st.mode) check(st, "mode", mode_word(core_->store().mode()), *st.mode);
    if (st.fact) {
      const auto& facts = core_->store().facts();
      const auto it = facts.find(st.fact->persona);
      const proto::FactRow* row = it != facts.end() ? core::find_fact(it->second, st.fact->key) : nullptr;
      check(st, ("fact " + st.fact->persona + "." + st.fact->key).c_str(), row != nullptr ? row->value : "<none>",
            st.fact->value);
    }
    if (st.audience) {
      const store::Playthrough* pt = store_ ? store_->playthrough() : nullptr;
      if (pt == nullptr) {
        run_.failed.push_back("line " + std::to_string(st.line) + ": audience: no file is open");
      } else {
        const std::uint16_t journey = core_->store().journey();
        std::size_t n = 0;
        if (auto seen = store::events::seen_by(pt->db, journey, st.audience->persona, 0, std::nullopt)) {
          for (const proto::StoredEvent& e : *seen) {
            const auto v = augur::visible_to(e, st.audience->persona, augur::VisCtx{journey});
            if (e.kind == st.audience->kind && v && proto::to_string(v->how) == st.audience->how) ++n;
          }
        }
        check(st, ("audience " + st.audience->kind + " " + st.audience->persona).c_str(), std::to_string(n),
              std::to_string(st.audience->count));
      }
    }
    if (st.name_prefixes) {
      std::size_t n = 0;
      for (const TapAttempt& a : tap_->attempts()) n += a.counts.name_prefixes();
      check(st, "name_prefixes", std::to_string(n), std::to_string(*st.name_prefixes));
    }
    for (std::size_t i = 0; i < st.prompt_has.size(); ++i) {
      if (last_prompt_.find(st.prompt_has[i]) == std::string::npos) {
        run_.failed.push_back("line " + std::to_string(st.line) + ": the prompt lacks prompt_has[" + std::to_string(i) + "]");
      }
    }
    for (std::size_t i = 0; i < st.prompt_lacks.size(); ++i) {
      if (last_prompt_.find(st.prompt_lacks[i]) != std::string::npos) {
        run_.failed.push_back("line " + std::to_string(st.line) + ": the prompt holds prompt_lacks[" + std::to_string(i) + "]");
      }
    }
  }

  void settle() {
    for (EvalTurn& t : run_.turns) {
      t.words = static_cast<std::uint32_t>(words_of(t.reply));
      t.deflection = std::find(persona_->deflections.begin(), persona_->deflections.end(), t.reply) != persona_->deflections.end();
    }
  }

  void finish() {
    settle();
    run_.attempts.assign(tap_->attempts().begin(), tap_->attempts().end());
  }

  const EvalScenario& s_;
  EvalLlm llm_;
  const EvalOptions& options_;
  std::vector<core::ContentFile> files_;
  std::unique_ptr<SpeechTap> tap_;   // before the core, which holds its parts
  std::unique_ptr<core::Core> core_;
  std::size_t seen_attempts_ = 0;
  const core::Persona* persona_ = nullptr;
  std::variant<std::monostate, MockModel, RecordedModel> model_;
  EvalRun run_;
  Mono t_ = 0;
  Mono next_snapshot_ = 250;
  Mono next_tick_ = 100;
  std::uint64_t seq_ = 0;
  float distance_ = s_.distance_m;
  std::optional<std::pair<proto::PageId, Mono>> ack_;
  std::optional<proto::PageId> acked_;
  bool box_open_ = false, shut_ = false, closing_ = false;
  std::optional<proto::PageId> failure_page_;
  proto::UiModel ui_;
  std::vector<Mono> said_at_;
  std::optional<proto::GameCommand::SetWatchFlags> watch_;  // the core's last watch list
  std::optional<core::FlagState> flags_;                    // the world's flag state, by the catalogue's aliases
  std::string flags_name_;                                  // its fixture's name
  std::string last_prompt_;                                 // the last dialogue request's messages, for prompt_has
  std::map<std::pair<proto::ConvId, proto::ReplyId>, std::size_t> replies_;
  // Memory (store = true): the run's folder, before the store that keeps its file open.
  std::unique_ptr<RunFolder> folder_;
  std::unique_ptr<store::StoreCore> store_;
  proto::Pt pt_base_ = 600000;  // the save's play time when loaded
  Mono t_base_ = 0;
  std::uint8_t ng_ = 0;
  std::optional<Mono> talk_until_;      // a native talk with the partner until then
  std::optional<std::uint64_t> strike_;  // the snapshot on which the Tarnished's blow lands
  std::uint32_t partner_hp_ = 100;
  std::vector<float> extra_dist_;
};

}  // namespace

proto::Expected<EvalRun, std::string> run_eval(const EvalScenario& scenario, EvalLlm llm,
                                               const EvalOptions& options) {
  Runner r(scenario, llm, options);
  return r.run();
}

std::optional<std::size_t> EvalTurn::first_try(const std::vector<TapAttempt>& all) const {
  std::optional<std::size_t> out;
  for (const std::size_t i : attempts) {
    if (i < all.size() && !all[i].context.regeneration) out = i;
  }
  return out;
}

std::optional<std::size_t> EvalTurn::regeneration(const std::vector<TapAttempt>& all) const {
  for (const std::size_t i : attempts) {
    if (i < all.size() && all[i].context.regeneration) return i;
  }
  return std::nullopt;
}

nlohmann::json counts_json(const SpeechCounts& c) {
  nlohmann::json j = nlohmann::json::object();
  nlohmann::json removed = nlohmann::json::object(), soft = nlohmann::json::object(), hard = nlohmann::json::object();
  for (std::size_t k = 0; k < c.removed.size(); ++k) {
    if (c.removed[k] != 0) removed[removed_name(k)] = c.removed[k];
  }
  for (std::size_t k = 0; k < c.soft.size(); ++k) {
    if (c.soft[k] != 0) soft[soft_name(k)] = c.soft[k];
  }
  for (std::size_t k = 0; k < c.hard.size(); ++k) {
    if (c.hard[k] != 0) hard[hard_name(k)] = c.hard[k];
  }
  j["removed"] = std::move(removed);
  j["soft"] = std::move(soft);
  j["hard"] = std::move(hard);
  j["sentences"] = c.sentences;
  j["archaic_sentences"] = c.archaic_sentences;
  j["full"] = c.full;
  j["over"] = c.over;
  j["trailers"] = c.trailers;
  j["stream_stops"] = c.stream_stops;
  if (c.name_class) j["name_prefix"] = {{"class", to_string(*c.name_class)}, {"text", c.name_prefix}};
  return j;
}

nlohmann::json eval_report(const EvalRun& run) {
  nlohmann::json j;
  j["schema"] = 2;
  j["kind"] = "eval-local";  // holds the conversation's text: local only, never committed or uploaded
  j["scenario"] = run.scenario;
  j["persona"] = run.persona;
  j["register"] = run.register_name;
  j["llm"] = run.llm;
  j["words_max"] = run.words_max;
  nlohmann::json turns = nlohmann::json::array();
  std::size_t shown = 0, within = 0, failures = 0, deflections = 0, cut = 0, regenerations = 0;
  std::vector<Mono> latency;
  SpeechCounts speech;  // the first tries'
  std::array<std::size_t, 3> classes{};
  for (const EvalTurn& t : run.turns) {
    nlohmann::json tj;
    tj["player"] = t.player;
    tj["reply"] = t.reply;
    tj["pages"] = t.pages;
    tj["words"] = t.words;
    tj["cut_off"] = t.cut_off;
    tj["failure"] = t.failure;
    tj["deflection"] = t.deflection;
    tj["requests"] = t.requests;
    tj["first_page_ms"] = t.first_page_ms ? nlohmann::json(*t.first_page_ms) : nlohmann::json(nullptr);
    if (const auto a = t.first_try(run.attempts)) {
      const SpeechCounts& c = run.attempts[*a].counts;
      tj["speech"] = counts_json(c);
      speech.add(c);
      if (c.name_class) ++classes[static_cast<std::size_t>(*c.name_class)];
    }
    if (const auto a = t.regeneration(run.attempts)) tj["regeneration_speech"] = counts_json(run.attempts[*a].counts);
    turns.push_back(std::move(tj));
    if (!t.reply.empty()) {
      ++shown;
      if (t.words <= run.words_max) ++within;
    }
    failures += t.failure ? 1 : 0;
    deflections += t.deflection ? 1 : 0;
    cut += t.cut_off ? 1 : 0;
    regenerations += t.requests > 1 ? t.requests - 1 : 0;
    if (t.first_page_ms) latency.push_back(*t.first_page_ms);
  }
  j["turns"] = std::move(turns);
  std::sort(latency.begin(), latency.end());
  nlohmann::json m;
  m["player_lines"] = run.turns.size();
  m["replies_shown"] = shown;
  m["within_words_max"] = shown == 0 ? 1.0 : static_cast<double>(within) / static_cast<double>(shown);
  m["failure_pages"] = failures;
  m["deflections"] = deflections;
  m["cut_offs"] = cut;
  m["regenerations"] = regenerations;
  m["requests"] = run.requests;
  m["cancels"] = run.cancels;
  m["first_page_ms_median"] = latency.empty() ? nlohmann::json(nullptr) : nlohmann::json(latency[latency.size() / 2]);
  m["first_page_ms_max"] = latency.empty() ? nlohmann::json(nullptr) : nlohmann::json(latency.back());
  // The first tries' speech (WHY§410: the "Name:" prefixes by class, so that a narrower rule can be judged on data).
  m["speech"] = counts_json(speech);
  m["name_prefixes"] = {{"name", classes[0]}, {"short", classes[1]}, {"long", classes[2]}};
  j["metrics"] = std::move(m);
  j["toasts"] = run.toasts;
  j["endings"] = run.endings;
  j["expectations_failed"] = run.failed;
  return j;
}

std::string eval_summary(const EvalRun& run) {
  const nlohmann::json r = eval_report(run);
  const nlohmann::json& m = r["metrics"];
  std::ostringstream o;
  o << "eval " << run.scenario << " (" << run.llm << "): " << m["player_lines"].get<std::size_t>() << " lines, "
    << m["replies_shown"].get<std::size_t>() << " replies shown, " << m["failure_pages"].get<std::size_t>()
    << " failure pages, " << m["deflections"].get<std::size_t>() << " deflections, " << m["regenerations"].get<std::size_t>()
    << " regenerations, within the word cap " << static_cast<int>(m["within_words_max"].get<double>() * 100.0 + 0.5)
    << "%, " << m["name_prefixes"]["name"].get<std::size_t>() + m["name_prefixes"]["short"].get<std::size_t>() +
                    m["name_prefixes"]["long"].get<std::size_t>()
    << " name prefixes, " << run.failed.size() << " expectations failed\n";
  return o.str();
}

}  // namespace en::tools
