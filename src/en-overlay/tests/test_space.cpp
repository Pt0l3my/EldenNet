// EldenNet's space (space.hpp), in every build: its page data as generated, its topics from invented facts, its one
// command path (settings.set and settings.reset, each refusal named), what it publishes and when (nothing unwatched, a
// state when watched and when it changes, at its rate; the logs stream from its oldest row, in batches), its words
// (no lamp while starting, the banner's own words, absence silent) and, in the Linux builds, the fixture states'
// topics.json, which grace-dev previews (tests/pages; EN_UPDATE_FIXTURES=1 writes them anew).
#include <doctest.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "calls_fixture.hpp"
#include "characters_fixture.hpp"
#include "content_fixture.hpp"
#include "en/overlay/docs.hpp"
#include "en/overlay/space.hpp"
#include "graceui.h"
#include "system_fixture.hpp"

using namespace en::overlay;
using nlohmann::json;
namespace pw = en::proto::watch;

namespace {

// ---- Invented facts: the fixture states (PAGES §5), with our own numbers and no game text.

SpaceFacts base() {
  SpaceFacts f;
  f.version = {0, 1, 0};
  f.label = "0.1.0";  // the build's label (EN_VERSION_LABEL)
  f.graceui = GRACEUI_VERSION_PACK(0, 2, 12);  // the GraceUI vendored (third_party/graceui-sys)
  f.floor = {0, 2, 12};
  f.build_known = true;
  f.build = {3, 14, 15, 0};
  f.build_supported = true;
  f.watcher = true;
  f.logger = true;
  f.launcher = true;
  f.session_ms = 754'000;
  return f;
}

SpaceFacts starting() {
  SpaceFacts f = base();
  f.init = pw::InitState::Discover;
  f.session_ms = 3'000;
  return f;
}

SpaceFacts title() {
  SpaceFacts f = base();
  f.init = pw::InitState::Ready;
  f.level = Level::Observing;
  f.where = Where::Title;
  f.gate_open = true;
  f.gate_why = Unclean::None;
  f.dormant = true;
  f.reading = Reading::Running;
  f.snapshots = 1200;
  f.task_p99_us = 140;
  f.have_health = true;
  for (auto& g : f.health.groups) g.state = en::proto::GroupState::Ok;
  f.health.of(en::proto::Group::Combat).state = en::proto::GroupState::Untested;
  f.health.of(en::proto::Group::Status).state = en::proto::GroupState::Untested;
  return f;
}

SpaceFacts world() {
  SpaceFacts f = title();
  f.where = Where::World;
  f.dormant = false;
  f.snapshots = 9600;
  f.health.of(en::proto::Group::Combat).state = en::proto::GroupState::Ok;
  f.health.of(en::proto::Group::Camera).state = en::proto::GroupState::Tripped;
  f.tripped = 1ull << static_cast<unsigned>(en::proto::Group::Camera);
  f.recording_asked = true;
  f.recording = true;
  f.recorded = 2400;
  f.session_ms = 3'930'000;
  f.ai_paused = true;  // Pause AI for this session (D15): the ladder at L2 for it, its calls made before it
  return f;
}

SpaceFacts multiplayer() {
  SpaceFacts f = world();
  f.tripped = 0;
  f.health.of(en::proto::Group::Camera).state = en::proto::GroupState::Ok;
  f.gate_open = false;
  f.gate_why = Unclean::NotSolo;
  f.dormant = true;
  f.badge = true;
  f.recording_asked = false;
  f.recording = false;
  f.ai_paused = false;
  return f;
}

// Each job's fixed budgets as the DLL fills them, from en-llm's slot table (ARCH §11.4).
std::vector<AiBudget> budgets() {
  return {AiBudget{en::proto::kDialogue, 140, 0.8, 6000, 10000, 20000, 0},
          AiBudget{en::proto::kBarkLive, 32, 0.9, 1500, 1500, 3000, 0},
          AiBudget{en::proto::kBarkPool, 600, 1.0, 15000, 15000, 60000, 1},
          AiBudget{en::proto::kMemory, 500, 0.3, 20000, 20000, 60000, 2},
          AiBudget{en::proto::kSelftest, 32, 0.0, 10000, 10000, 20000, 0}};
}

// The AI pages' facts (invented ids, models and an invented address; the provider's name is the shipped preset's).
AiFacts ai_ready() {
  AiFacts ai;
  ai.files = AiFiles::Ready;
  ai.preset = "cloud-budget";
  ai.presets = {"cloud-budget", "cloud-nanogpt"};
  ai.keys_set = {"openrouter"};
  AiProvider p;
  p.id = "openrouter";
  p.cloud = true;
  p.flavor = AiFlavor::OpenAI;
  p.base_url = "https://router.example.invalid/api/v1";
  p.key = AiKey::Set;
  p.key_place = AiKeyPlace::Secrets;
  p.key_name = "openrouter";
  p.concurrency = 2;
  p.json = AiJson::Schema;
  p.answered_ms = 700'000;
  p.slots = {en::proto::kDialogue, en::proto::kBarkLive, en::proto::kBarkPool, en::proto::kMemory,
             en::proto::kSelftest};
  ai.providers.push_back(p);
  // As the DLL fills them: a slot that borrows another's models has that slot's provider and models too.
  auto slot = [](std::string_view name, std::string model, std::string fallback, std::string_view same) {
    AiSlot s;
    s.slot = name;
    s.provider = "openrouter";
    s.model = std::move(model);
    if (!fallback.empty()) {
      s.fallback_provider = "openrouter";
      s.fallback = std::move(fallback);
    }
    s.same_as = same;
    return s;
  };
  ai.slots = {slot(en::proto::kDialogue, "talker-small", "talker-large", ""),
              slot(en::proto::kBarkLive, "talker-small", "", ""), slot(en::proto::kBarkPool, "pool-flash", "", ""),
              slot(en::proto::kMemory, "keeper-large", "", ""),
              slot(en::proto::kSelftest, "talker-small", "talker-large", en::proto::kDialogue)};
  ai.budgets = budgets();
  ai.revision = 3;
  ai.unused = {};
  ai.listed = true;
  AiList l;
  l.provider = "openrouter";
  l.state = AiStep::Passed;
  l.listed = 214;
  ai.lists = {l};
  // What the provider listed (invented ids; one shaped like a local server's model file, which no picker shows).
  ai.models = {AiModels{"openrouter", {"talker-small", "talker-large", "pool-flash", "keeper-large", "keeper-small",
                                        "C:\\Users\\someone\\models\\invented.gguf"}}};
  AiSelfTest& st = ai.selftest;
  st.ran = true;
  st.at_ms = 690'000;
  st.lists = {l};
  st.latency = AiStep::Passed;
  st.json = AiStep::Passed;
  st.provider = "openrouter";
  st.model = "talker-small";
  st.first_token_ms = 420;
  st.tokens_per_second = 61;
  st.first_page_ms = 666;
  st.verdict = AiVerdict::Quick;
  st.earlier = {AiRun{610'000, "talker-small", AiVerdict::Acceptable, true}};
  return ai;
}

// A first run: no providers.toml yet, so nothing set up but the budgets, which are code.
AiFacts ai_first_run() {
  AiFacts ai;
  ai.files = AiFiles::Missing;
  ai.budgets = budgets();
  ai.presets = {"cloud-budget", "cloud-nanogpt"};
  ai.preset_missing = {"openrouter"};
  return ai;
}

// A server of the player's own on this PC, with no key's place (its row reads Not needed), listing invented models.
AiProvider lantern() {
  AiProvider p;
  p.id = "lantern-box";
  p.cloud = false;
  p.base_url = "http://127.0.0.1:1234/v1";
  p.key = AiKey::NotNeeded;
  p.json = AiJson::Schema;
  return p;
}

// The title screen's: ai_ready with that server added, Memory's pick on it (providers.toml's [slots.memory]).
AiFacts ai_own() {
  AiFacts ai = ai_ready();
  AiProvider own = lantern();
  own.slots = {en::proto::kMemory};
  own.answered_ms = 705'000;
  ai.providers[0].slots = {en::proto::kDialogue, en::proto::kBarkLive, en::proto::kBarkPool, en::proto::kSelftest};
  ai.providers.push_back(own);
  ai.slots[3].provider = "lantern-box";
  ai.slots[3].model = "lantern-keeper";
  ai.slots[3].picked = true;
  ai.models.push_back(AiModels{"lantern-box", {"lantern-keeper", "lantern-talker"}});
  AiList l = ai.lists[0];
  l.provider = "lantern-box";
  l.listed = 2;
  ai.lists.push_back(l);
  return ai;
}

// What the fixture states point at (the facts hold a pointer).
const AiFacts& ai_fixture(std::string_view state) {
  static const AiFacts own = ai_own();
  static const AiFacts first = ai_first_run();
  static const AiFacts resting = [] {
    AiFacts ai = ai_ready();
    ai.providers[0].cooling_s = 42;
    ai.providers[0].pairs_cooling = 1;
    ai.slots[2].healthy = false;
    // A second cloud provider whose key the model list refused (its row reads Refused, Health says why).
    AiProvider spare;
    spare.id = "spare-cloud";
    spare.base_url = "https://spare.example.invalid/v1";
    spare.key = AiKey::Rejected;
    spare.key_place = AiKeyPlace::Secrets;
    spare.key_name = "spare-cloud";
    spare.concurrency = 2;
    spare.json = AiJson::Schema;
    ai.providers.push_back(spare);
    ai.unused = {"spare-cloud"};
    AiList l;
    l.provider = "spare-cloud";
    l.state = AiStep::Failed;
    l.why = AiFailure::KeyRefused;
    ai.lists.push_back(l);
    return ai;
  }();
  if (state == "title") return own;
  if (state == "world" || state == "multiplayer") return resting;
  return first;
}

SpaceFacts banner() {
  SpaceFacts f = base();
  f.init = pw::InitState::Banner;
  f.level = Level::Inactive;
  f.stop = Stop::UnknownBuild;
  f.stop_text = "EldenNet inactive: game build 3.14.15.0 not supported yet";
  f.where = Where::Stopped;
  f.build_supported = false;
  f.launcher = false;
  f.settings_refused = true;
  f.session_ms = 41'000;
  return f;
}

#if !defined(_WIN32)
// The log lines a state's session wrote, invented in the log's own vocabulary, each at the level the DLL writes it
// (the fixtures', compared in the Linux builds alone).
void feed(Space& s, std::string_view state) {
  constexpr LogLevel info = LogLevel::Info;
  constexpr LogLevel warning = LogLevel::Warning;
  s.log_line("boot pid=4242 dll=eldennet.dll probe_build=0 version_minor=1 version=dev", 3, info);
  s.log_line("gate result=" + std::string(state == "banner" ? "unknown_build" : "passed") + " ms=11", 11, info);
  s.log_line("init state=settings", 14, info);
  if (state == "banner") {
    s.log_line("settings_problem where=debug.log_level", 15, warning);
    s.log_line("banner reason=unknown_build detail=unknown_build", 1210, warning);
    return;
  }
  s.log_line("connect ok=1 outcome=connected path=main how=path looks=1 installed=1 graceui=0.2.12", 1830, info);
  if (state == "starting") return;
  s.log_line("init state=ready", 6120, info);
  s.log_line("change what=phase to=title", 6140, info);
  if (state == "world" || state == "multiplayer") {
    s.log_line("change what=phase to=in_world", 61200, info);
    s.log_line("tripped group=camera", 1200400, warning);
  }
  if (state == "multiplayer") s.log_line("change what=badge to=up", 1300800, info);
}
#endif

const char* const kStates[] = {"starting", "title", "world", "multiplayer", "banner"};

SpaceFacts facts_of(std::string_view state) {
  SpaceFacts f = state == "starting" ? starting()
                 : state == "title"  ? title()
                 : state == "world"  ? world()
                 : state == "multiplayer" ? multiplayer()
                                          : banner();
  f.ai = &ai_fixture(state);
  f.characters = en::overlay::test::characters_fixture(state);  // Characters › Named NPCs (characters_fixture.hpp)
  f.content = en::overlay::test::content_fixture(state);        // Content packs (content_fixture.hpp)
  f.calls = en::overlay::test::calls_fixture(state);            // AI › Requests, Home's Model calls (calls_fixture.hpp)
  f.perf = en::overlay::test::perf_fixture(state);              // System › Performance (system_fixture.hpp)
  f.traces = en::overlay::test::traces_fixture(state);          // System › Traces (system_fixture.hpp)
  return f;
}

const char* const kAiTopics[] = {"ai.status",
                                 "providers",
                                 "health",
                                 "ai.jobs",
                                 "ai.budgets",
                                 "selftest",
                                 "space.state",
                                 "ai.choices.dialogue",
                                 "ai.choices.bark_live",
                                 "ai.choices.bark_pool",
                                 "ai.choices.memory",
                                 "ai.choices.selftest",
                                 "ai.choices.providers",
                                 "ai.choices.dialogue.fallback",
                                 "ai.choices.bark_live.fallback",
                                 "ai.choices.bark_pool.fallback",
                                 "ai.choices.memory.fallback",
                                 "ai.choices.selftest.fallback",
                                 "calls",
                                 "usage"};

json frame(const std::string& text) { return json::parse(text); }

std::string cmd(const json& args, std::string_view name = "settings.set", int id = 7) {
  return json{{"v", 1}, {"t", "cmd"}, {"space", "eldennet"}, {"id", id}, {"name", name}, {"args", args}}.dump();
}

std::string sub(std::string_view topic) {
  return json{{"v", 1}, {"t", "sub"}, {"space", "eldennet"}, {"id", 0}, {"topic", topic}}.dump();
}

std::vector<json> collect(Space& s, const SpaceFacts& f, std::uint64_t now) {
  std::vector<std::string> frames;
  s.collect(f, now, frames);
  std::vector<json> out;
  for (const std::string& t : frames) out.push_back(json::parse(t));
  return out;
}

// Every string in a body: none holds a path's or an address's marks (PV1 to PV8, PAGES §4.9).
bool clean(const json& j) {
  if (j.is_string()) {
    const std::string s = j.get<std::string>();
    return s.find('\\') == std::string::npos && s.find(":/") == std::string::npos &&
           s.find("127.0.0.1") == std::string::npos && s.find("AppData") == std::string::npos;
  }
  if (j.is_array() || j.is_object()) {
    for (const json& v : j) {
      if (!clean(v)) return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("space: the page data as generated: EldenNet's id, the slice's increment, every topic built here") {
  const json pages = json::parse(space_pages());
  const json settings = json::parse(settings_description());
  CHECK(pages["space"] == "eldennet");
  CHECK(settings["schema"]["number"] == 2);
  CHECK(settings["schema"]["hash"].get<std::string>().size() == 16);
  std::vector<std::string> ids;
  for (const json& p : pages["pages"]) {
    ids.push_back(p["id"].get<std::string>());
    // Every page ships in the increment registered or an earlier one (D173): Home and System in 0.1, AI in 0.2.
    CHECK(p["increment"].get<std::string>() <= std::string(kIncrement));
  }
  CHECK(ids == std::vector<std::string>{"home", "system.status", "system.performance", "system.traces", "system.logs",
                                        "settings.developer", "ai.providers", "ai.models", "ai.selftest",
                                        "ai.requests", "characters.named", "content", "tools.report", "docs"});
  // The rail holds PAGES §4.2.1's categories in their order, the slice's pages in theirs.
  std::vector<std::string> rail;
  for (const json& c : pages["categories"]) rail.push_back(c["id"].get<std::string>());
  CHECK(rail == std::vector<std::string>{"home", "ai", "voice", "characters", "map", "history", "behaviour", "system",
                                         "tools", "settings", "docs", "content"});
  // Its foot (protocol 1.10, GraceUI D248): Settings, Docs and Content packs, after every other category.
  std::vector<std::string> foot;
  for (const json& c : pages["categories"]) {
    if (c.value("foot", false)) foot.push_back(c["id"].get<std::string>());
  }
  CHECK(foot == std::vector<std::string>{"settings", "docs", "content"});
  const Space s;
  REQUIRE(s.valid());
  for (const json& t : pages["topics"]) {
    const std::string id = t["id"].get<std::string>();
    if (t["shape"] == "stream") {
      CHECK(id == "logs");
    } else if (t.value("family", false)) {
      CHECK(s.body(id, facts_of("title")).empty());  // a family is never published itself, only its members (D215)
    } else {
      CHECK(s.body(id, facts_of("title")).is_object());
      CHECK_FALSE(s.body(id, facts_of("title")).empty());
    }
  }
  CHECK(pages["commands"] == json::array({"settings.set", "settings.reset", "selftest.run", "models.list",
                                          "providers.add", "providers.remove", "providers.key", "profile.check",
                                          "profile.save", "profile.reset", "profile.restore", "profile.draft",
                                          "report.build", "report.part", "ai.pause"}));
  // A stream's filter bar: protocol 1.16's (GraceUI D257), on 1.15's header switch (D256), 1.13's quick switches and
  // live pages (D252), 1.12's document block, prose field and preview (D251), 1.11's filter bar (D250), 1.10's foot
  // (D248) and 1.8's sensitive marks (D221).
  CHECK(pages["minor"] == 16);
  // Pause AI for this session, the one quick switch (GraceUI §78's form), and not sensitive: a paired device may pause.
  CHECK(pages["quick"] == json::array({{{"id", "pause"}, {"label", "Pause AI for this session"},
                                        {"command", "ai.pause"},
                                        {"state", {{"topic", "status"}, {"field", "ai_paused"}}}, {"home", "home"}}}));
  CHECK(std::find(pages["sensitive_commands"].begin(), pages["sensitive_commands"].end(), "ai.pause") ==
        pages["sensitive_commands"].end());
  // Home, Performance, Traces and Requests are live (D252): their topics move by themselves; no other page is.
  std::vector<std::string> live;
  for (const json& p : pages["pages"]) {
    if (p.value("live", false)) live.push_back(p["id"].get<std::string>());
  }
  CHECK(live == std::vector<std::string>{"home", "system.performance", "system.traces", "ai.requests"});
  // System's pages in PAGES §4.2.1's order, Performance and Traces after Status.
  for (const json& c : pages["categories"]) {
    if (c["id"] == "system") {
      CHECK(c["pages"] == json::array({"system.status", "system.performance", "system.traces", "system.logs"}));
    }
  }
  // GraceUI 0.2.5's checks (D183, eldennet-changes §36), met already: no topic is the server's own "pages", the id is
  // not GraceUI's reserved one, and every destructive action, a row's included, has its confirmation's text.
  CHECK(pages["space"] != "graceui");
  for (const json& t : pages["topics"]) CHECK(t["id"] != "pages");
  int row_actions = 0;
  int forms = 0;
  for (const json& p : pages["pages"]) {
    for (const json& a : p.value("actions", json::array())) {
      if (a["tone"] == "destructive") CHECK_FALSE(a.value("confirm", std::string()).empty());
    }
    for (const json& sec : p.value("sections", json::array())) {
      for (const json& b : sec["blocks"]) {
        for (const json& a : b.value("row_actions", json::array())) {
          ++row_actions;
          CHECK(a["command"] == "providers.remove");
          CHECK(a["tone"] == "destructive");
          CHECK_FALSE(a.value("confirm", std::string()).empty());
        }
        if (b.contains("row_secret")) CHECK(b["row_secret"] == json{{"column", "key"}, {"command", "providers.key"},
                                                                    {"key", "id"}});
        if (b["kind"] == "form") {
          ++forms;
          CHECK((b["command"] == "providers.add" || b["command"] == "report.build"));
        }
      }
    }
  }
  CHECK(row_actions == 1);
  CHECK(forms == 2);
  // Home's and System's blocks on GraceUI 0.2.7's props (D185 to D187): one topic for System › Status, its own words
  // for an empty list, the log newest first.
  std::vector<std::string> topics;
  for (const json& t : pages["topics"]) topics.push_back(t["id"].get<std::string>());
  CHECK(std::find(topics.begin(), topics.end(), "system.compatibility") == topics.end());
  const auto block_of = [&](std::string_view page, std::string_view section) {
    for (const json& p : pages["pages"]) {
      if (p["id"].get<std::string>() != page) continue;
      std::vector<json> all = p.value("sections", json::array());
      for (const json& tab : p.value("tabs", json::array())) {
        for (const json& sec : tab["sections"]) all.push_back(sec);
      }
      for (const json& sec : all) {
        if (sec["id"].get<std::string>() == section) return sec["blocks"][0];
      }
    }
    return json();
  };
  CHECK(block_of("home", "problems")["props"]["empty"] == "No problems.");
  CHECK(block_of("system.logs", "log")["props"]["newest_first"] == true);
  CHECK(block_of("system.status", "versions")["props"]["field"] == "compatibility");
  CHECK(block_of("system.status", "versions")["topic"] == "system.status");
  // Each job's model row is a picker on a topic of the space's own (D193).
  for (const json& s : settings["settings"]) {
    if (s["key"].get<std::string>().rfind("slots.", 0) != 0) continue;
    CHECK(s["type"] == "text");
    CHECK(s["choices_from"].get<std::string>().rfind("ai.choices.", 0) == 0);
    CHECK(s["home"] == "ai.models");
  }
  // The settings topic sends the description and schema as registered, and no other.
  const json body = s.settings_body();
  CHECK(body["schema"] == settings["schema"]);
  CHECK(body["description"] == settings["settings"]);
}

TEST_CASE("space: settings start at their defaults; settings.set validated at its one entry point") {
  Space s;
  REQUIRE(s.valid());
  json defaults{{"debug.panel", false}, {"debug.record_sessions", false}, {"debug.log_level", "info"},
                {"ai.preset", "cloud-budget"}};
  for (const char* slot : {"dialogue", "bark_live", "bark_pool", "memory", "selftest"}) {
    for (const char* row : {"provider", "model", "fallback_provider", "fallback"}) {
      defaults[std::string("slots.") + slot + "." + row] = "";  // each job's four rows: the preset's (GraceUI D199)
    }
  }
  CHECK(s.values() == defaults);
  s.set_value("debug.panel", true);  // as settings.toml had it at the start
  s.set_value("debug.unknown", 3);   // not one it describes: ignored
  CHECK(s.values().size() == 24);
  json body = s.settings_body();
  CHECK(body["values"]["debug.panel"] == true);
  CHECK(body["changed"] == json::array({"debug.panel"}));
  CHECK(body["description"].size() == 24);
  CHECK(body["schema"]["number"] == 2);

  const SpaceFacts f = title();
  json r = frame(s.command(cmd({{"key", "debug.log_level"}, {"value", "debug"}}), f));
  CHECK(r["t"] == "result");
  CHECK(r["space"] == "eldennet");
  CHECK(r["id"] == 7);
  CHECK(r["ok"] == true);
  CHECK_FALSE(r.contains("error"));
  auto changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "debug.log_level");
  CHECK(changes[0].value == "debug");
  CHECK(s.take_changes().empty());
  // The same value again changes nothing.
  CHECK(frame(s.command(cmd({{"key", "debug.log_level"}, {"value", "debug"}}), f))["ok"] == true);
  CHECK(s.take_changes().empty());

  const auto error_of = [&](const json& args, std::string_view name = "settings.set") {
    const json a = frame(s.command(cmd(args, name), f));
    return a.value("error", std::string("none"));
  };
  CHECK(error_of({{"key", "debug.log_level"}, {"value", "trace"}}) == "invalid_value");  // not one of its choices
  CHECK(error_of({{"key", "debug.panel"}, {"value", 1}}) == "invalid_value");           // not true or false
  CHECK(error_of({{"key", "debug.panel"}}) == "invalid_value");
  CHECK(error_of({{"key", "pacing.gap_s"}, {"value", 2.5}}) == "unknown_key");  // a later increment's
  CHECK(error_of({{"key", "debug.panel"}, {"value", true}}, "voice.sample") == "unknown_command");
  CHECK(frame(s.command("not json", f))["error"] == "bad_message");
  CHECK(s.take_changes().empty());
  auto notes = s.take_notes();
  REQUIRE(notes.size() == 8);
  CHECK(notes[0].what == SpaceNote::What::Command);
  CHECK(notes[0].topic == "settings.set");
  CHECK(notes[0].key == "debug.log_level");
  CHECK(notes[0].result == "ok");
  CHECK(notes[2].result == "invalid_value");
  CHECK(notes[5].result == "unknown_key");
  CHECK(notes[6].topic == "voice.sample");
  CHECK(notes[7].result == "bad_message");

  // While the settings file the player wrote is refused, nothing writes over it: not now, in words.
  SpaceFacts refused = f;
  refused.settings_refused = true;
  r = frame(s.command(cmd({{"key", "debug.panel"}, {"value", false}}), refused));
  CHECK(r["error"] == "not_now");
  CHECK(r["detail"].get<std::string>().find("settings file") != std::string::npos);
  CHECK(r["detail"].get<std::string>().size() <= 256);
  CHECK(s.take_changes().empty());
}

TEST_CASE("space: a secret is held and published as its status alone, set or empty (invariant 5, PV2)") {
  const std::string description = R"({"schema":{"number":1,"hash":"0000000000000000"},"settings":[)"
                                   R"({"key":"keys.test","type":"secret","default":"","label":"A key","help":"An )"
                                   R"(invented key.","synonyms":[],"home":"home","effect":"now","availability":"live",)"
                                   R"("locked":false}]})";
  Space s(description);
  REQUIRE(s.valid());
  CHECK(s.values()["keys.test"] == "");
  CHECK(frame(s.command(cmd({{"key", "keys.test"}, {"value", "an-invented-value"}}), facts_of("title")))["ok"] == true);
  CHECK(s.values()["keys.test"] == "set");
  CHECK(s.settings_body().dump().find("an-invented-value") == std::string::npos);
  const auto changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].value == "an-invented-value");  // for the DLL to store, never to publish
  s.set_value("keys.test", "another-invented-value");
  CHECK(s.values()["keys.test"] == "set");
}

TEST_CASE("space: settings.reset: a key at once; a page or every setting only once confirmed (D173)") {
  Space s;
  const SpaceFacts f = title();
  s.set_value("debug.panel", true);
  s.set_value("debug.record_sessions", true);
  s.set_value("debug.log_level", "debug");
  json r = frame(s.command(cmd({{"scope", "key"}, {"target", "debug.panel"}}, "settings.reset"), f));
  CHECK(r["ok"] == true);
  CHECK(s.values()["debug.panel"] == false);
  r = frame(s.command(cmd({{"scope", "page"}, {"target", "settings.developer"}}, "settings.reset"), f));
  CHECK(r["error"] == "confirm_required");
  CHECK(s.values()["debug.record_sessions"] == true);
  r = frame(s.command(cmd({{"scope", "page"}, {"target", "settings.developer"}, {"confirm", true}}, "settings.reset"),
                      f));
  CHECK(r["ok"] == true);
  CHECK(s.values()["debug.record_sessions"] == false);
  CHECK(s.values()["debug.log_level"] == "info");
  CHECK(s.take_changes().size() == 3);
  s.set_value("debug.panel", true);
  CHECK(frame(s.command(cmd({{"scope", "all"}}, "settings.reset"), f))["error"] == "confirm_required");
  CHECK(frame(s.command(cmd({{"scope", "all"}, {"confirm", true}}, "settings.reset"), f))["ok"] == true);
  CHECK(s.values()["debug.panel"] == false);
  CHECK(frame(s.command(cmd({{"scope", "key"}, {"target", "debug.none"}}, "settings.reset"), f))["error"] ==
        "unknown_key");
  CHECK(frame(s.command(cmd({{"scope", "page"}, {"target", "home"}, {"confirm", true}}, "settings.reset"), f))
            ["error"] == "invalid_value");  // a page that homes no setting
  CHECK(frame(s.command(cmd({{"scope", "everything"}}, "settings.reset"), f))["error"] == "invalid_value");
}

TEST_CASE("space: nothing is published unwatched; a state when watched, when it changes, and at its rate") {
  Space s;
  SpaceFacts f = title();
  CHECK(collect(s, f, 1000).empty());
  s.watched(sub("settings"));
  s.watched(sub("status"));
  CHECK(s.watching("status"));
  auto out = collect(s, f, 1000);
  REQUIRE(out.size() == 2);
  CHECK(out[0]["t"] == "state");
  CHECK(out[0]["topic"] == "settings");
  CHECK(out[0]["rev"] == 1);
  CHECK(out[0]["body"]["values"]["debug.log_level"] == "info");
  CHECK(out[1]["topic"] == "status");
  CHECK(out[1]["body"]["word"] == "Dormant");
  CHECK(collect(s, f, 1100).empty());  // nothing changed

  // A command's change: the settings again at once, at the next revision.
  (void)s.command(cmd({{"key", "debug.log_level"}, {"value", "warn"}}), f);
  out = collect(s, f, 1150);
  REQUIRE(out.size() == 1);
  CHECK(out[0]["topic"] == "settings");
  CHECK(out[0]["rev"] == 2);
  CHECK(out[0]["body"]["changed"] == json::array({"debug.log_level"}));

  // The status at most four times a second (GraceUI DESIGN §6.5): a change 100 ms after its last waits.
  f = world();
  CHECK(collect(s, f, 1100).empty());
  out = collect(s, f, 1260);
  REQUIRE(out.size() == 1);
  CHECK(out[0]["body"]["word"] == "Observing");
  CHECK(out[0]["rev"] == 2);

  // Watched again (another view): the newest state at once, changed or not. Unwatched: nothing more.
  s.watched(sub("status"));
  out = collect(s, f, 1300);
  REQUIRE(out.size() == 1);
  CHECK(out[0]["rev"] == 3);
  s.unwatched(json{{"v", 1}, {"t", "unsub"}, {"space", "eldennet"}, {"id", 0}, {"topic", "status"}}.dump());
  CHECK_FALSE(s.watching("status"));
  CHECK(collect(s, banner(), 5000).empty());
  const auto notes = s.take_notes();
  REQUIRE(notes.size() == 5);
  CHECK(notes[0].what == SpaceNote::What::Watched);
  CHECK(notes[4].what == SpaceNote::What::Unwatched);
  CHECK(notes[4].topic == "status");
  s.watched("not a frame");  // a frame with no topic is passed over
  CHECK(s.take_notes().empty());
}

TEST_CASE("space: the logs stream from its oldest row held, in batches of 100, then each new row; its ring of 500") {
  Space s;
  const SpaceFacts f = title();
  for (int i = 0; i < 520; ++i) s.log_line("line n=" + std::to_string(i), 1000u * i, LogLevel::Info);
  CHECK(s.log_rows().size() == 500);
  CHECK(s.log_rows()[0]["seq"] == 21);
  CHECK(collect(s, f, 0).empty());  // unwatched
  s.watched(sub("logs"));
  auto out = collect(s, f, 600'000);
  REQUIRE(out.size() == 5);
  CHECK(out[0]["t"] == "rows");
  CHECK(out[0]["topic"] == "logs");
  CHECK(out[0]["from_seq"] == 21);
  CHECK(out[0]["dropped"] == 20);  // lost to the ring before the view watched
  CHECK(out[0]["rows"].size() == 100);
  CHECK(out[0]["rows"][0]["line"] == "line n=20");
  CHECK(out[0]["rows"][0]["time"] == "0:00:20");
  CHECK(out[0]["rows"][0]["level"] == "info");
  CHECK(out[4]["from_seq"] == 421);
  CHECK(out[4]["dropped"] == 0);
  CHECK(out[4]["rows"].back()["seq"] == 520);
  s.log_line("line n=520", 700'000, LogLevel::Info);
  CHECK(collect(s, f, 600'500).empty());  // within its second
  out = collect(s, f, 601'100);
  REQUIRE(out.size() == 1);
  CHECK(out[0]["from_seq"] == 521);
  CHECK(out[0]["rows"].size() == 1);
  // A view that watches again gets the stream from the oldest row held.
  s.watched(sub("logs"));
  out = collect(s, f, 601'200);
  REQUIRE(out.size() == 5);
  CHECK(out[0]["from_seq"] == 22);
}

TEST_CASE("space: the words: no lamp while starting, the banner's own words, absence silent, nothing personal") {
  Space s;
  json b = s.body("status", starting());
  CHECK_FALSE(b.contains("word"));  // no lamp before EldenNet has a level (GraceUI §24 item 1)
  CHECK(b["items"][0] == json{{"label", "State"}, {"value", "Starting"}});
  CHECK(b["items"][1]["value"] == "Finding what it reads in the game");

  b = s.body("status", banner());
  CHECK(b["word"] == "Inactive");
  CHECK(b["tone"] == "rot");
  CHECK(b["reason"] == "EldenNet inactive: game build 3.14.15.0 not supported yet");
  CHECK(b["page"] == "system.status");
  const json errors = s.body("errors", banner());
  REQUIRE(errors["checks"].size() == 2);  // the failed start, and the settings file
  CHECK(errors["checks"][0]["label"] == "EldenNet inactive: game build 3.14.15.0 not supported yet");
  CHECK(errors["checks"][0]["state"] == "fail");
  CHECK(errors["checks"][1]["state"] == "warn");

  b = s.body("status", multiplayer());
  CHECK(b["word"] == "Dormant");
  CHECK(b["reason"].get<std::string>().find("online") != std::string::npos);
  CHECK(s.body("status", world())["word"] == "Observing");

  // Absence is silent: no GraceUI version before one is known, no build that was not read, no read groups before a
  // snapshot, no recording that was not asked for, no problems when there are none.
  SpaceFacts bare = starting();
  bare.graceui = 0;
  bare.build_known = false;
  const json bare_status = s.body("status", bare);  // held: a range over a temporary's member would dangle
  for (const json& i : bare_status["items"]) {
    CHECK(i["label"] != "GraceUI");
    CHECK(i["label"] != "Game build");
  }
  CHECK(s.body("system.status", bare)["rows"].empty());
  const json title_system = s.body("system.status", title());
  for (const json& c : title_system["checks"]) CHECK(c["label"] != "Recording");
  CHECK(s.body("errors", title())["checks"].empty());
  CHECK(s.body("system.status", world())["rows"].size() == en::proto::kGroupCount);
  // The Compatibility tab reads the same topic's `compatibility` member (GraceUI D185).
  const json compatibility = s.body("system.status", title())["compatibility"]["checks"];
  REQUIRE(compatibility.size() == 3);
  CHECK(compatibility[0]["label"] == "Game build 3.14.15.0");
  CHECK(compatibility[2]["detail"] == "EldenNet needs GraceUI 0.2.12 or later");

  for (const char* state : kStates) {
    for (const char* topic : {"status", "errors", "system.status"}) CHECK(clean(s.body(topic, facts_of(state))));
  }
}

TEST_CASE("space: L3 and L4 show as speaking, in their own words, not as Observing (WHY§422, §442)") {
  Space s;
  AiFacts ai = ai_ready();
  SpaceFacts f = world();
  f.ai = &ai;
  f.ai_paused = false;  // the world fixture's pause holds the ladder at L2: off here, as it must be to speak
  f.level = Level::Speaking;
  json b = s.body("status", f);
  CHECK(b["word"] == "Speaking");
  CHECK(b["tone"] == "moss");
  CHECK(b["reason"] == "EldenNet reads the game, and NPCs speak.");
  CHECK(b["items"][0]["value"] == "Speaking");
  CHECK(s.body("ai.status", f)["sentence"] == "Conversation is ready: NPCs answer when you speak to them.");
  bool solo = false;
  const json system = s.body("system.status", f);  // held: a range over a temporary's member would dangle
  for (const json& i : system["items"]) solo = solo || i["label"] == "Offline and solo";
  CHECK(solo);

  f.level = Level::SpeakingInPart;
  b = s.body("status", f);
  CHECK(b["word"] == "Speaking, in part");
  CHECK(b["tone"] == "ember");
  CHECK(b["reason"] == "NPCs speak, but part of the game EldenNet reads has stopped: System › Status says which.");
  CHECK(s.body("ai.status", f)["sentence"] == "Conversation is ready: NPCs answer when you speak to them.");

  // Dormant over either speaking level, as over L2; the AI pages lead with the gate's cause, not with speech.
  for (const Level l : {Level::SpeakingInPart, Level::Speaking}) {
    SpaceFacts m = multiplayer();
    m.ai = &ai;
    m.level = l;
    CHECK(s.body("status", m)["word"] == "Dormant");
    CHECK(s.body("ai.status", m)["sentence"] == "Model calls wait while you play with others online.");
  }
  // L2 keeps its words.
  f.level = Level::Observing;
  CHECK(s.body("status", f)["word"] == "Observing");
  CHECK(clean(s.body("status", f)));
}

// Every string of a body that holds `needle`, for the checks that a key, an address or a listed id is nowhere.
bool holds(const json& j, std::string_view needle) {
  if (j.is_string()) return j.get<std::string>().find(needle) != std::string::npos;
  if (j.is_array() || j.is_object()) {
    for (const json& v : j) {
      if (holds(v, needle)) return true;
    }
  }
  return false;
}

TEST_CASE("space: the AI pages' topics: projections only, the address on Providers alone, absence silent") {
  const Space s;
  const SpaceFacts f = facts_of("title");
  const json providers = s.body("providers", f);
  REQUIRE(providers["rows"].size() == 2);
  const json& row = providers["rows"][0];
  CHECK(row["id"] == "openrouter");
  CHECK(row["kind"] == "Cloud");
  CHECK(row["type"] == "OpenAI-compatible");
  CHECK(row["address"] == "https://router.example.invalid/api/v1");  // on this page alone (PV2)
  CHECK(row["key"] == "set");  // the row's secret (GraceUI D194): its status alone
  CHECK(row["requests"] == "2");
  CHECK(row["json"] == "JSON schema");
  // A server of one's own on this PC with no key's place: Not needed (GraceUI D197), and no field.
  CHECK(providers["rows"][1]["id"] == "lantern-box");
  CHECK(providers["rows"][1]["kind"] == "Local");
  CHECK(providers["rows"][1]["key"] == "none");
  REQUIRE(providers["checks"].size() == 2);
  CHECK(providers["checks"][0]["state"] == "pass");
  CHECK(providers["checks"][0]["detail"] == "Lists 214 models, every job's model among them.");
  for (const char* topic : {"ai.status", "health", "ai.jobs", "ai.budgets", "selftest", "ai.choices.dialogue",
                            "ai.choices.memory", "ai.choices.providers", "ai.choices.dialogue.fallback"}) {
    CHECK_FALSE(holds(s.body(topic, f), "example.invalid"));
    CHECK_FALSE(holds(s.body(topic, f), "://"));
  }
  // A job's picker: what its provider listed, in its order, with nothing shaped like a path (PV5); a job that borrows
  // another's models picks from the same provider's list; Memory, on the server of one's own, from that one's.
  const json choices = s.body("ai.choices.bark_pool", f)["choices"];
  REQUIRE(choices.size() == 5);
  CHECK(choices[0] == json{{"value", "talker-small"}, {"label", "talker-small"}});
  CHECK(choices[4]["value"] == "keeper-small");
  CHECK_FALSE(holds(choices, "Users"));
  CHECK(s.body("ai.choices.selftest", f)["choices"].size() == 5);
  CHECK(s.body("ai.choices.memory", f)["choices"] ==
        json::array({json{{"value", "lantern-keeper"}, {"label", "lantern-keeper"}},
                     json{{"value", "lantern-talker"}, {"label", "lantern-talker"}}}));
  // Every job's provider picker: the providers, in providers.toml's order.
  CHECK(s.body("ai.choices.providers", f)["choices"] ==
        json::array({json{{"value", "openrouter"}, {"label", "openrouter"}},
                     json{{"value", "lantern-box"}, {"label", "lantern-box"}}}));
  CHECK(s.body("ai.choices.providers", facts_of("starting")) == json{{"choices", json::array()}});
  CHECK(s.body("ai.choices.dialogue", facts_of("starting")) == json{{"choices", json::array()}});  // nothing listed
  CHECK(s.body("ai.choices.dialogue", title()) == json{{"choices", json::array()}});
  CHECK(s.body("ai.status", f)["sentence"].get<std::string>().find("NPCs speak once a model answers") !=
        std::string::npos);
  // Each card's status (GraceUI D202): a member per job, in the lamp's tones; its budgets, a job that borrows saying so.
  const json jobs = s.body("ai.jobs", f);
  CHECK(jobs.size() == 5);
  for (const char* slot : {"dialogue", "bark_live", "bark_pool", "memory", "selftest"}) {
    CHECK(jobs[slot] == json{{"word", "Ready"}, {"tone", "moss"}});
  }
  const json budgets = s.body("ai.budgets", f);
  CHECK(budgets.size() == 5);
  CHECK(budgets["dialogue"]["items"] ==
        json::array({json{{"label", "Longest answer"}, {"value", "140 tokens"}},
                     json{{"label", "Temperature"}, {"value", "0.8"}},
                     json{{"label", "First token within"}, {"value", "6 s, 10 s on a local server"}},
                     json{{"label", "Whole answer within"}, {"value", "20 s"}},
                     json{{"label", "Retries"}, {"value", "None"}}}));
  CHECK(budgets["bark_live"]["items"][2]["value"] == "1.5 s");
  CHECK(budgets["memory"]["items"][4]["value"] == "2");
  CHECK(budgets["selftest"]["items"][0] == json{{"label", "Models"}, {"value", "Same as Conversation"}});
  CHECK(budgets["selftest"]["items"][2]["value"] == "0");
  const json health = s.body("health", f);
  CHECK(health["rows"][0]["state"] == "Healthy");
  CHECK(health["rows"][0]["answered"] == "0:11:40");
  CHECK(health["rows"][0]["jobs"] == "Conversation, Quick lines, Prepared lines, Self-test");
  CHECK(health["rows"][1]["jobs"] == "Memory");
  const json st = s.body("selftest", f);
  CHECK(st["sentence"] == "Quick: the first words of a reply come in about 0.67 s.");
  REQUIRE(st["checks"].size() == 3);
  CHECK(st["checks"][0]["label"] == "Reaches openrouter");
  CHECK(st["checks"][1]["state"] == "pass");
  CHECK(st["checks"][2]["state"] == "pass");
  CHECK(st["rows"].size() == 1);
  CHECK(st["rows"][0]["verdict"] == "Acceptable");

  // GraceUI's space.state (D188): nothing greyed but at L1, where the AI pages carry the banner's words.
  CHECK(s.body("space.state", f) == json{{"greyed", json::object()}});
  const json greyed = s.body("space.state", facts_of("banner"))["greyed"];
  CHECK(greyed.size() == 5);  // Home and AI (PAGES §4.4): System and Settings stay usable
  CHECK(greyed["ai.selftest"] == "EldenNet inactive: game build 3.14.15.0 not supported yet");
  CHECK(greyed["home"] == greyed["ai.selftest"]);
  CHECK(greyed.contains("ai.providers"));
  CHECK(greyed["ai.requests"] == greyed["ai.selftest"]);
  CHECK_FALSE(greyed.contains("system.status"));
  CHECK_FALSE(greyed.contains("settings.developer"));
  SpaceFacts long_banner = facts_of("banner");
  const std::string words(300, 'w');
  long_banner.stop_text = words;
  CHECK(s.body("space.state", long_banner)["greyed"]["ai.models"].get<std::string>().size() == 200);

  // Resting models, a key refused, the gate closed online, a first run, the banner.
  CHECK(s.body("health", facts_of("world"))["rows"][0]["state"] == "Cooling down, 42 s left");
  CHECK(s.body("health", facts_of("world"))["rows"][1]["state"] == "Failing: it refused its key");
  CHECK(s.body("providers", facts_of("world"))["rows"][1]["key"] == "refused");
  CHECK(s.body("ai.choices.providers", facts_of("world"))["choices"].size() == 1);  // no job's choice while refused
  // The world's: the provider rate-limited, so every job on it is cooling down.
  CHECK(s.body("ai.jobs", facts_of("world"))["bark_pool"] == json{{"word", "Cooling down"}, {"tone", "ember"}});
  CHECK(s.body("ai.status", facts_of("multiplayer"))["sentence"] ==
        "Model calls wait while you play with others online.");
  CHECK(s.body("ai.status", facts_of("starting"))["sentence"].get<std::string>().rfind("No provider yet", 0) == 0);
  // At L1 the lead takes the banner's own words, the greyed page's one form (O95).
  CHECK(s.body("ai.status", facts_of("banner"))["sentence"] ==
        "EldenNet inactive: game build 3.14.15.0 not supported yet");
  // Absence is silent: no table rows for providers there are not, no self-test before one runs, nothing without facts.
  CHECK(s.body("providers", facts_of("starting"))["rows"].empty());
  CHECK(s.body("selftest", facts_of("starting")).empty());
  for (const char* topic : {"ai.status", "providers", "health", "ai.jobs", "ai.budgets", "selftest"}) {
    CHECK(s.body(topic, title()).empty());
  }

  // A key's status reads Missing or Rejected, and the lead says what to do.
  AiFacts missing = ai_ready();
  missing.providers[0].key = AiKey::Missing;
  SpaceFacts m = title();
  m.ai = &missing;
  CHECK(s.body("ai.status", m)["sentence"] == "openrouter has no key yet: paste it on its row on Providers.");
  CHECK(s.body("providers", m)["rows"][0]["key"] == "");
  missing.providers[0].key = AiKey::Rejected;
  CHECK(s.body("providers", m)["rows"][0]["key"] == "refused");  // GraceUI D197: Refused, the field kept
  CHECK(s.body("ai.status", m)["sentence"] == "openrouter refused its key: paste it again on its row on Providers.");
  CHECK(s.body("health", m)["rows"][0]["state"] == "Failing: it refused its key");
  CHECK(s.body("ai.choices.providers", m)["choices"].empty());
  // A provider with no key's place whose call was refused for want of one asks for a key; with none asked for, it
  // needs none.
  missing.providers[0].key_place = AiKeyPlace::None;
  CHECK(s.body("ai.status", m)["sentence"] == "openrouter asks for a key: paste it on its row on Providers.");
  CHECK(s.body("health", m)["rows"][0]["state"] == "Failing: it asks for a key");
  CHECK(s.body("providers", m)["rows"][0]["key"] == "refused");
  missing.providers[0].key = AiKey::NotNeeded;
  CHECK(s.body("providers", m)["rows"][0]["key"] == "none");
  CHECK(s.body("health", m)["rows"][0]["state"] == "Healthy");
  missing.providers[0].key = AiKey::Rejected;
  missing.providers[0].key_place = AiKeyPlace::Secrets;
  // A list that missed a job's model, and a self-test that could not run its steps.
  missing.lists[0].state = AiStep::Failed;
  missing.lists[0].missing = {en::proto::kDialogue, en::proto::kMemory};
  CHECK(s.body("providers", m)["checks"][0]["detail"] ==
        "Lists 214 models, but not Conversation's or Memory's model.");
  missing.selftest.lists[0].state = AiStep::Failed;
  missing.selftest.lists[0].why = AiFailure::KeyRefused;
  missing.selftest.latency = AiStep::Skipped;
  missing.selftest.json = AiStep::Skipped;
  missing.selftest.verdict = AiVerdict::None;
  missing.selftest.first_token_ms = -1;
  const json failed = s.body("selftest", m);
  CHECK(failed["checks"][0]["detail"] == "Refused the key.");
  CHECK(failed["checks"][1]["detail"] == "Not run: its provider did not answer.");
  CHECK(failed["sentence"].get<std::string>().find("could not measure") != std::string::npos);
  missing.selftest.running = true;
  CHECK(s.body("selftest", m)["sentence"] == "The self-test is running: it takes a few seconds.");
}

namespace {

AiRequest request(AiRequest::Kind kind, std::string id = {}) {
  AiRequest r;
  r.kind = kind;
  r.id = std::move(id);
  return r;
}

}  // namespace

TEST_CASE("space: a refused providers file says why on System › Status and Home, by its line, never its text") {
  const Space s;
  AiFacts ai = ai_ready();
  ai.files = AiFiles::Refused;
  ai.refusal = AiRefusal::PlainKey;
  ai.refusal_line = 7;
  SpaceFacts f = title();
  f.ai = &ai;
  const auto part = [&]() {
    const json body = s.body("system.status", f);  // bound first: a member of a temporary dangles (WHY§269)
    for (const json& c : body["checks"]) {
      if (c["label"] == "The providers file") return c;
    }
    return json();
  };
  REQUIRE(part().is_object());
  CHECK(part()["state"] == "fail");
  CHECK(part()["detail"] == "A provider with a key uses http:// to another computer, where its key would cross the "
                            "network unencrypted: use https://, or remove its key. Correct providers.toml at line 7, "
                            "then start the game again.");
  const json problems = s.body("errors", f)["checks"];
  REQUIRE(problems.size() == 1);
  CHECK(problems[0]["label"] == "Your providers file has a problem, so EldenNet calls no model.");
  CHECK(problems[0]["detail"] == part()["detail"]);
  CHECK(s.body("ai.status", f)["sentence"] ==
        "Your providers file has a problem, so EldenNet calls no model: System › Status says what.");
  const std::pair<AiRefusal, const char*> words[] = {
      {AiRefusal::Scheme, "starts with neither https:// nor http://"},
      {AiRefusal::Credentials, "holds a name or password"},
      {AiRefusal::QueryOrFragment, "has a ? or # in it"},
      {AiRefusal::PlainPublic, "neither this PC nor a private address"},
      {AiRefusal::PlainCloud, "A cloud provider's base_url uses http://"},
      {AiRefusal::Other, "something EldenNet cannot use"}};
  for (const auto& [refusal, said] : words) {
    ai.refusal = refusal;
    CHECK(part()["detail"].get<std::string>().find(said) != std::string::npos);
  }
  ai.refusal_line = 0;
  CHECK(part()["detail"].get<std::string>().find("Correct providers.toml, then") != std::string::npos);
  // A file read: nothing to say.
  ai.files = AiFiles::Ready;
  CHECK_FALSE(part().is_object());
  CHECK(s.body("errors", f)["checks"].empty());
}

TEST_CASE("space: the AI pages' commands, each answered at once; the long ones handed to the DLL") {
  Space s;
  AiFacts ai = ai_ready();
  SpaceFacts f = title();
  f.ai = &ai;
  const auto answer_of = [&](std::string_view name, const json& args = json::object()) {
    return frame(s.command(cmd(args, name), f));
  };
  using K = AiRequest::Kind;
  CHECK(answer_of("selftest.run")["ok"] == true);
  CHECK(answer_of("models.list")["ok"] == true);
  CHECK(s.take_requests() == std::vector<AiRequest>{request(K::SelfTest), request(K::ListModels)});
  ai.selftest.running = true;
  CHECK(answer_of("selftest.run")["error"] == "busy");
  CHECK(answer_of("models.list")["error"] == "not_now");
  ai.selftest.running = false;
  ai.listing = true;
  CHECK(answer_of("models.list")["error"] == "busy");
  CHECK(answer_of("selftest.run")["detail"] == "The model list is running: try again when it ends.");
  ai.listing = false;
  f.gate_open = false;  // only while the gate allows network (GraceUI DESIGN §6.6)
  CHECK(answer_of("selftest.run")["error"] == "not_now");
  f.gate_open = true;
  ai.providers.clear();
  CHECK(answer_of("selftest.run")["detail"].get<std::string>().rfind("Add a provider first", 0) == 0);
  ai.files = AiFiles::Refused;
  CHECK(answer_of("models.list")["detail"].get<std::string>().find("providers file") != std::string::npos);
  CHECK(s.take_requests().empty());

  // The header's Add the preset's provider: the chosen preset's providers that providers.toml lacks.
  ai = ai_first_run();
  CHECK(answer_of("providers.add")["ok"] == true);
  CHECK(s.take_requests() == std::vector<AiRequest>{request(K::AddPreset)});
  ai.preset_missing.clear();
  CHECK(answer_of("providers.add")["detail"] == "Your providers already include every one this preset uses.");
  ai.preset_needs_probe = true;
  CHECK(answer_of("providers.add")["error"] == "not_now");
  s.set_value("ai.preset", "custom");
  ai.preset_needs_probe = false;
  CHECK(answer_of("providers.add")["detail"].get<std::string>().rfind("Custom keeps your providers file", 0) == 0);

  // Remove on a provider's row: destructive, so confirmed (D173, D192); never one a job uses, and the answer says why.
  ai = ai_ready();
  CHECK(answer_of("providers.remove", {{"id", "openrouter"}})["error"] == "confirm_required");
  CHECK(answer_of("providers.remove", {{"id", "openrouter"}, {"confirm", "yes"}})["error"] == "confirm_required");
  json r = answer_of("providers.remove", {{"id", "openrouter"}, {"confirm", true}});
  CHECK(r["error"] == "not_now");
  CHECK(r["detail"] == "Conversation, Quick lines, Prepared lines, Memory and Self-test use it: on Models, choose another "
                       "preset, or reset a job's model to the preset's.");
  CHECK(answer_of("providers.remove", {{"id", "nobody"}, {"confirm", true}})["error"] == "invalid_value");
  CHECK(answer_of("providers.remove", {{"confirm", true}})["error"] == "invalid_value");
  AiProvider spare;
  spare.id = "spare";
  spare.base_url = "https://spare.example.invalid/v1";
  ai.providers.push_back(spare);
  ai.unused = {"spare"};
  CHECK(answer_of("providers.remove", {{"id", "spare"}, {"confirm", true}})["ok"] == true);
  CHECK(s.take_requests() == std::vector<AiRequest>{request(K::Remove, "spare")});
  ai.listing = true;
  CHECK(answer_of("providers.remove", {{"id", "spare"}, {"confirm", true}})["error"] == "not_now");
  ai.listing = false;
  // A provider one job uses names it alone.
  ai.unused.clear();
  ai.providers[1].slots = {en::proto::kMemory};
  CHECK(answer_of("providers.remove", {{"id", "spare"}, {"confirm", true}})["detail"].get<std::string>().rfind(
            "Memory uses it:", 0) == 0);

  // While EldenNet starts or is inactive, nothing; inactive, the banner's own words.
  f.level = Level::Inactive;
  f.stop_text = "EldenNet inactive: an invented reason";
  CHECK(answer_of("selftest.run")["detail"] == "EldenNet inactive: an invented reason");
  f.ai = nullptr;
  f.level = Level::Observing;
  CHECK(answer_of("providers.add")["error"] == "not_now");
  CHECK(s.take_requests().empty());

  // An argument of the wrong type is refused, and changes nothing.
  f.ai = &ai;
  CHECK(answer_of("settings.set", {{"key", 3}, {"value", "x"}})["error"] == "invalid_value");
  CHECK(answer_of("providers.key", {{"id", 3}, {"value", 4}})["error"] == "invalid_value");
  CHECK(s.take_requests().empty());
  CHECK(s.take_changes().empty());
}

TEST_CASE("space: a provider of one's own from the form: its name and base URL checked here, refusals in words") {
  Space s;
  AiFacts ai = ai_ready();
  SpaceFacts f = title();
  f.ai = &ai;
  const auto add = [&](const json& name, const json& url) {
    return frame(s.command(cmd({{"name", name}, {"base_url", url}}, "providers.add"), f));
  };
  json r = add("lantern-box", " http://127.0.0.1:1234/v1/ ");
  CHECK(r["ok"] == true);
  std::vector<AiRequest> taken = s.take_requests();
  REQUIRE(taken.size() == 1);
  CHECK(taken[0].kind == AiRequest::Kind::AddOwn);
  CHECK(taken[0].id == "lantern-box");
  CHECK(taken[0].base_url == "http://127.0.0.1:1234/v1");
  CHECK(taken[0].local);
  r = add("far.cloud", "https://models.example.invalid/api/v1");
  CHECK(r["ok"] == true);
  taken = s.take_requests();
  REQUIRE(taken.size() == 1);
  CHECK_FALSE(taken[0].local);
  // A server on another PC of the home network, by its private address (O98 (a)): a local one, which takes no key.
  // The IPv4 one is built from its numbers, so that no file holds a home network's address as text (the sweep).
  CHECK(add("other-pc", "http://[fd00::20]:1234/v1")["ok"] == true);
  const std::string home_v4 = "http://" + std::to_string(192) + "." + std::to_string(168) + ".1.20:1234/v1";
  CHECK(add("other-pc-2", home_v4)["ok"] == true);
  taken = s.take_requests();
  REQUIRE(taken.size() == 2);
  CHECK(taken[0].local);
  CHECK(taken[0].base_url == "http://[fd00::20]:1234/v1");
  CHECK(taken[1].local);

  const auto refusal = [&](const json& name, const json& url) {
    const json a = add(name, url);
    CHECK(a["error"] == "invalid_value");
    return a.value("detail", std::string());
  };
  CHECK(refusal("", "https://models.example.invalid/v1") == "Give the provider a name.");
  CHECK(refusal("two words", "https://models.example.invalid/v1").rfind("A provider's name is up to 32", 0) == 0);
  CHECK(refusal(std::string(33, 'a'), "https://models.example.invalid/v1").rfind("A provider's name", 0) == 0);
  CHECK(refusal("OpenRouter", "https://models.example.invalid/v1") ==
        "A provider is already called that: choose another name.");
  ai.keys_set = {"openrouter", "spare-key"};
  CHECK(refusal("Spare-Key", "https://models.example.invalid/v1") ==
        "A key is already kept under that name: choose another name.");
  CHECK(refusal("box", "") == "Give the provider's base URL, such as https://api.example.com/v1.");
  CHECK(refusal("box", "ftp://models.example.invalid/v1").rfind("A base URL starts with https://", 0) == 0);
  CHECK(refusal("box", "https://someone:secret@models.example.invalid/v1").rfind("A base URL holds no name", 0) == 0);
  CHECK(refusal("box", "https://models.example.invalid/v1?key=1") ==
        "A base URL ends at its path, with no ? or # after it.");
  CHECK(refusal("box", "https://models.example.invalid:99999/v1").find("port") != std::string::npos);
  CHECK(refusal("box", "https://models example/v1").find("character") != std::string::npos);
  CHECK(refusal("box", "http://192.0.2.7:1234/v1") ==
        "http:// goes only to this PC, or to another PC of your home network by its private address, not its name: "
        "give this server's https:// address.");
  CHECK(refusal("box", "http://storeroom.invalid:1234/v1").rfind("http:// goes only to this PC", 0) == 0);  // a name
  CHECK(refusal("box", "http://127.example.invalid/v1").rfind("http:// goes only to this PC", 0) == 0);
  CHECK(refusal("box", 12).rfind("Give the provider's base URL", 0) == 0);
  CHECK(refusal(12, "https://models.example.invalid/v1") == "Give the provider a name.");
  CHECK(s.take_requests().empty());
  // No typed name or address reaches a note (the log): only the command and its result.
  for (const SpaceNote& n : s.take_notes()) {
    CHECK(n.key.empty());
    CHECK(n.result.find("example") == std::string::npos);
  }
  // While a run goes on, not now.
  ai.selftest.running = true;
  CHECK(add("box", "https://models.example.invalid/v1")["error"] == "not_now");
}

TEST_CASE("space: a key on a provider's row: write-only, never echoed, never published; where no key may go, words") {
  Space s;
  AiFacts ai = ai_ready();
  SpaceFacts f = title();
  f.ai = &ai;
  const std::string key = "sk-invented-0123456789abcdef";
  const auto put = [&](const std::string& id, const json& value) {
    return frame(s.command(cmd({{"id", id}, {"value", value}}, "providers.key"), f));
  };
  json r = put("openrouter", "  " + key + "\n");
  CHECK(r["ok"] == true);
  CHECK_FALSE(r.contains("body"));
  std::vector<AiRequest> taken = s.take_requests();
  REQUIRE(taken.size() == 1);
  CHECK(taken[0].kind == AiRequest::Kind::SetKey);
  CHECK(taken[0].id == "openrouter");
  CHECK(taken[0].key == key);  // for the DLL to store and overwrite, never to publish
  // Cleared with an empty value.
  CHECK(put("openrouter", "")["ok"] == true);
  CHECK(s.take_requests() == std::vector<AiRequest>{request(AiRequest::Kind::ClearKey, "openrouter")});
  // Refused, in words that never echo it.
  r = put("openrouter", "two words");
  CHECK(r["error"] == "invalid_value");
  CHECK(r["detail"].get<std::string>().find("two words") == std::string::npos);
  CHECK(put("openrouter", std::string(1025, 'k'))["error"] == "invalid_value");
  CHECK(put("nobody", key)["error"] == "invalid_value");
  CHECK(put("openrouter", 5)["error"] == "invalid_value");
  // Never to an address a key would reach in clear text, nor where the file keeps it in the environment.
  ai.providers[0].plain = true;
  r = put("openrouter", key);
  CHECK(r["error"] == "not_now");
  CHECK(r["detail"].get<std::string>().find("no key") != std::string::npos);
  ai.providers[0].plain = false;
  ai.providers[0].key_place = AiKeyPlace::Env;
  CHECK(put("openrouter", key)["detail"].get<std::string>().find("environment variable") != std::string::npos);
  // A provider with no key's place (a server of one's own on this PC) takes none while its row reads Not needed (GraceUI
  // D197), and one once a call was refused for want of it; clearing none is no request.
  ai.providers[0].key_place = AiKeyPlace::None;
  ai.providers[0].key = AiKey::NotNeeded;
  r = put("openrouter", key);
  CHECK(r["error"] == "not_now");
  CHECK(r["detail"] == "This provider takes no key. If it asks for one, its row says so after List models.");
  ai.providers[0].key = AiKey::Rejected;
  CHECK(put("openrouter", key)["ok"] == true);
  CHECK(put("openrouter", "")["ok"] == true);
  taken = s.take_requests();
  REQUIRE(taken.size() == 1);
  CHECK(taken[0].kind == AiRequest::Kind::SetKey);
  ai.files = AiFiles::Refused;
  CHECK(put("openrouter", key)["error"] == "not_now");
  // The key is in no frame published, no answer, no note.
  std::vector<std::string> frames;
  s.watched(sub("settings"));
  for (const char* topic : kAiTopics) s.watched(sub(topic));
  ai.files = AiFiles::Ready;
  s.collect(f, 1000, frames);
  CHECK(frames.size() == 1 + std::size(kAiTopics));
  for (const std::string& text : frames) CHECK(text.find(key) == std::string::npos);
  for (const SpaceNote& n : s.take_notes()) {
    CHECK(n.key.find(key) == std::string::npos);
    CHECK(n.result.find(key) == std::string::npos);
  }
}

TEST_CASE("space: the preset and each job's model through settings.set: a model only from what its provider lists") {
  Space s;
  AiFacts ai = ai_ready();
  SpaceFacts f = title();
  f.ai = &ai;
  // The model rows follow the facts: each job's model now.
  std::vector<std::string> frames;
  s.collect(f, 0, frames);
  CHECK(s.values()["slots.dialogue.model"] == "talker-small");
  CHECK(s.values()["slots.memory.model"] == "keeper-large");
  CHECK(s.values()["slots.selftest.model"] == "talker-small");  // borrowed from Conversation
  const auto set = [&](const std::string& key, const json& value) {
    return frame(s.command(cmd({{"key", key}, {"value", value}}), f));
  };
  json r = set("slots.memory.model", "keeper-small");
  CHECK(r["ok"] == true);
  std::vector<Space::Change> changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "slots.memory.model");
  CHECK(changes[0].value == "keeper-small");
  CHECK(s.values()["slots.memory.model"] == "keeper-small");
  // Not one it listed, or shaped like a path, or nothing: refused, in words.
  r = set("slots.memory.model", "invented-elsewhere");
  CHECK(r["error"] == "invalid_value");
  CHECK(r["detail"] == "openrouter does not list that model: press List models on Providers again.");
  CHECK(set("slots.memory.model", "C:\\Users\\someone\\models\\invented.gguf")["error"] == "invalid_value");
  CHECK(set("slots.memory.model", 4)["error"] == "invalid_value");
  // A picker's Default (GraceUI D199) is the row's reset: with no pick in providers.toml, nothing to write, and the row
  // shows the job's model again.
  CHECK(set("slots.memory.model", "")["ok"] == true);
  CHECK(s.take_changes().empty());
  CHECK(s.values()["slots.memory.model"] == "keeper-large");
  // Before a list has answered, not now.
  ai.models.clear();
  CHECK(set("slots.dialogue.model", "talker-large")["detail"].get<std::string>().rfind("Press List models", 0) == 0);
  ai = ai_ready();
  ai.slots[1].provider.clear();
  CHECK(set("slots.bark_live.model", "talker-large")["detail"] == "Choose Quick lines' provider first.");
  CHECK(s.take_changes().empty());
  // A reset: the preset's model again (the DLL clears the job's [slots.*]), once the DLL has taken the pick.
  ai = ai_ready();
  ai.slots[3].model = "keeper-small";
  ai.slots[3].picked = true;
  ai.revision = 4;
  s.collect(f, 5, frames);
  CHECK(frame(s.command(cmd({{"scope", "key"}, {"target", "slots.memory.model"}}, "settings.reset"), f))["ok"] == true);
  changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].value == "");
  // Once the DLL takes the edited configuration (its revision), the rows follow it again.
  ai.revision = 5;
  ai.slots[3].model = "keeper-large";
  ai.slots[3].picked = false;
  s.collect(f, 10, frames);
  CHECK(s.values()["slots.memory.model"] == "keeper-large");
  // A refused settings.toml does not hold them; a refused providers.toml does.
  f.settings_refused = true;
  CHECK(set("slots.memory.model", "keeper-small")["ok"] == true);
  ai.files = AiFiles::Refused;
  CHECK(set("slots.memory.model", "talker-small")["error"] == "not_now");
  CHECK(frame(s.command(cmd({{"scope", "key"}, {"target", "slots.memory.model"}}, "settings.reset"), f))["error"] ==
        "not_now");
  ai.files = AiFiles::Ready;
  // The preset: one of models.toml's, or custom.
  CHECK(set("ai.preset", "cloud-nanogpt")["ok"] == true);
  CHECK(set("ai.preset", "custom")["ok"] == true);
  CHECK(set("ai.preset", "local-12gb")["error"] == "invalid_value");
  ai.presets = {"cloud-budget"};
  CHECK(set("ai.preset", "cloud-nanogpt")["error"] == "not_now");
  CHECK(s.take_changes().size() == 3);
}

TEST_CASE("space: a job's provider: one of the providers, written with its model at once or waiting for one") {
  Space s;
  AiFacts ai = ai_own();
  SpaceFacts f = title();
  f.ai = &ai;
  std::vector<std::string> frames;
  s.collect(f, 0, frames);
  // The rows follow the facts: each job's provider and model now; the changed ones are Memory's, its pick.
  CHECK(s.values()["slots.dialogue.provider"] == "openrouter");
  CHECK(s.values()["slots.memory.provider"] == "lantern-box");
  CHECK(s.values()["slots.memory.model"] == "lantern-keeper");
  CHECK(s.values()["slots.selftest.provider"] == "openrouter");  // borrowed from Conversation
  CHECK(s.settings_body()["changed"] == json::array({"slots.memory.provider", "slots.memory.model"}));
  const auto set = [&](const std::string& key, const json& value) {
    return frame(s.command(cmd({{"key", key}, {"value", value}}), f));
  };
  const auto choices = [&](std::string_view topic) {
    std::vector<std::string> out;
    const json body = s.body(topic, f);  // held: a range-for over a temporary's member would dangle (WHY§269)
    for (const json& c : body["choices"]) out.push_back(c["value"].get<std::string>());
    return out;
  };
  // Not one of the providers, or one whose key was refused: refused, in words. The reserved jobs have no rows.
  json r = set("slots.bark_pool.provider", "invented-nowhere");
  CHECK(r["error"] == "invalid_value");
  CHECK(r["detail"] == "That provider is not in your providers file: add it on Providers first.");
  CHECK(set("slots.bark_pool.provider", 5)["error"] == "invalid_value");
  ai.providers[1].key = AiKey::Rejected;
  CHECK(set("slots.bark_pool.provider", "lantern-box")["detail"] ==
        "lantern-box asks for a key: paste it on its row on Providers first.");
  ai.providers[1].key = AiKey::NotNeeded;
  CHECK(set("slots.actions.provider", "openrouter")["error"] == "unknown_key");
  CHECK(set("slots.vision.model", "talker-small")["error"] == "unknown_key");
  CHECK(s.take_changes().empty());

  // A provider whose list lacks the job's model waits on its row, nothing written, and the job's models follow it.
  CHECK(set("slots.bark_pool.provider", "lantern-box")["ok"] == true);
  CHECK(s.take_changes().empty());
  CHECK(s.values()["slots.bark_pool.provider"] == "lantern-box");
  CHECK(s.values()["slots.bark_pool.model"] == "pool-flash");
  CHECK(choices("ai.choices.bark_pool") == std::vector<std::string>{"lantern-keeper", "lantern-talker"});
  CHECK(s.settings_body()["changed"] == json::array({"slots.memory.provider", "slots.memory.model"}));
  r = set("slots.bark_pool.model", "pool-flash");
  CHECK(r["error"] == "invalid_value");
  CHECK(r["detail"] == "lantern-box does not list that model: press List models on Providers again.");
  // A model of its list: the pick, written on that provider.
  CHECK(set("slots.bark_pool.model", "lantern-talker")["ok"] == true);
  std::vector<Space::Change> changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "slots.bark_pool.model");
  CHECK(changes[0].value == "lantern-talker");
  CHECK(changes[0].provider == "lantern-box");
  // The DLL takes it (a new revision): the job's rows count as changed now, and only they and Memory's.
  ai.slots[2].provider = "lantern-box";
  ai.slots[2].model = "lantern-talker";
  ai.slots[2].picked = true;
  ai.revision = 4;
  s.collect(f, 10, frames);
  CHECK(s.values()["slots.bark_pool.provider"] == "lantern-box");
  CHECK(s.settings_body()["changed"] == json::array({"slots.bark_pool.provider", "slots.bark_pool.model",
                                                     "slots.memory.provider", "slots.memory.model"}));

  // A provider that lists the job's model takes the job at once, with that model; the job's own again changes nothing.
  ai.models[1].ids.push_back("talker-small");
  CHECK(set("slots.dialogue.provider", "lantern-box")["ok"] == true);
  changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "slots.dialogue.model");
  CHECK(changes[0].value == "talker-small");
  CHECK(changes[0].provider == "lantern-box");
  CHECK(set("slots.dialogue.provider", "openrouter")["ok"] == true);
  CHECK(s.take_changes().empty());
  CHECK(set("slots.dialogue.model", "talker-small")["ok"] == true);  // its model now, on its provider now
  CHECK(s.take_changes().empty());

  // A reset of either row takes the job's pick away: one change, an empty model; the rows count as changed no more once
  // the DLL has taken it.
  CHECK(frame(s.command(cmd({{"scope", "key"}, {"target", "slots.bark_pool.provider"}}, "settings.reset"), f))["ok"] ==
        true);
  changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "slots.bark_pool.model");
  CHECK(changes[0].value == "");
  ai.slots[2] = ai_ready().slots[2];
  ai.revision = 5;
  s.collect(f, 20, frames);
  CHECK(s.values()["slots.bark_pool.provider"] == "openrouter");
  CHECK(s.settings_body()["changed"] == json::array({"slots.memory.provider", "slots.memory.model"}));
  // A job with no pick but a provider waiting: its reset drops the provider, writes nothing, and the rows are the job's.
  ai.models[1].ids.pop_back();
  CHECK(set("slots.selftest.provider", "lantern-box")["ok"] == true);
  CHECK(s.values()["slots.selftest.provider"] == "lantern-box");
  CHECK(frame(s.command(cmd({{"scope", "key"}, {"target", "slots.selftest.model"}}, "settings.reset"), f))["ok"] == true);
  CHECK(s.take_changes().empty());
  CHECK(s.values()["slots.selftest.provider"] == "openrouter");
  CHECK(s.values()["slots.selftest.model"] == "talker-small");
  CHECK(choices("ai.choices.selftest").size() == 5);
  // A provider waiting whose entry goes (the player removed it) is dropped at the next revision.
  CHECK(set("slots.selftest.provider", "lantern-box")["ok"] == true);
  AiFacts gone = ai_ready();
  gone.revision = 6;
  f.ai = &gone;
  s.collect(f, 30, frames);
  CHECK(s.values()["slots.selftest.provider"] == "openrouter");
  CHECK(s.settings_body()["changed"] == json::array());

  // A new preset leaves the picks (PAGES §4.5's rows): Memory's stay changed, and the preset's row is changed too.
  f.ai = &ai;
  ai.revision = 7;
  s.collect(f, 40, frames);
  CHECK(set("ai.preset", "cloud-nanogpt")["ok"] == true);
  ai.preset = "cloud-nanogpt";
  ai.revision = 8;
  s.collect(f, 50, frames);
  CHECK(s.settings_body()["changed"] == json::array({"ai.preset", "slots.memory.provider", "slots.memory.model"}));
  CHECK(s.take_changes().size() == 1);  // the preset's

  // A job with no model yet: its provider first, then a model of its list, written together.
  AiFacts none = ai_own();
  for (AiSlot& slot : none.slots) {
    slot.provider.clear();
    slot.model.clear();
    slot.picked = false;
  }
  none.revision = 9;
  f.ai = &none;
  s.collect(f, 60, frames);
  CHECK(set("slots.dialogue.model", "talker-small")["detail"] == "Choose Conversation's provider first.");
  CHECK(set("slots.dialogue.provider", "lantern-box")["ok"] == true);
  CHECK(s.take_changes().empty());
  CHECK(set("slots.dialogue.model", "lantern-talker")["ok"] == true);
  changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].provider == "lantern-box");
  // With no provider at all, the words say to add one.
  none.providers.clear();
  CHECK(set("slots.memory.model", "lantern-keeper")["detail"].get<std::string>().rfind("Add a provider first", 0) == 0);
}

TEST_CASE("space: AI › Models is one card per job, each with exactly its own rows, its slot as chip, a live status") {
  const json pages = json::parse(space_pages());
  const json settings = json::parse(settings_description());
  json models;
  for (const json& p : pages["pages"]) {
    if (p["id"] == "ai.models") models = p;
  }
  REQUIRE(models.is_object());
  std::vector<std::string> ids;
  for (const json& sec : models["sections"]) ids.push_back(sec["id"].get<std::string>());
  // PAGES §4.6.2's order; Decisions (0.4) and Profile drafts (0.6) are not built, and the Jobs table is gone.
  CHECK(ids == std::vector<std::string>{"now", "preset", "dialogue", "bark_live", "bark_pool", "memory", "selftest"});
  const std::vector<std::pair<std::string, std::string>> cards = {{"dialogue", "Conversation"},
                                                                  {"bark_live", "Quick lines"},
                                                                  {"bark_pool", "Prepared lines"},
                                                                  {"memory", "Memory"},
                                                                  {"selftest", "Self-test"}};
  for (const auto& [slot, title] : cards) {
    const json* card = nullptr;
    for (const json& sec : models["sections"]) {
      if (sec["id"] == slot) card = &sec;
    }
    REQUIRE(card != nullptr);
    CHECK((*card)["title"] == title);
    CHECK((*card)["chip"] == slot);
    CHECK((*card)["status"] == json{{"topic", "ai.jobs"}, {"field", slot}});
    CHECK_FALSE(card->contains("actions"));  // "Prepare lines now" waits for the pool
    REQUIRE((*card)["blocks"].size() == 2);
    const std::string base = "slots." + slot + ".";
    CHECK((*card)["blocks"][0] == json{{"kind", "settings"},
                                       {"keys", json::array({base + "provider", base + "model", base + "fallback_provider",
                                                             base + "fallback"})}});
    CHECK((*card)["blocks"][1] == json{{"kind", "component"}, {"component", "key_value_list"}, {"topic", "ai.budgets"},
                                       {"props", json{{"field", slot}}}});
  }
  // One home per setting: every setting is in exactly one settings block of one page, and that page is its home; a job's
  // four rows are its card's alone.
  std::map<std::string, int> homes;
  std::map<std::string, std::string> section_of;
  for (const json& p : pages["pages"]) {
    std::vector<json> all = p.value("sections", json::array());
    for (const json& tab : p.value("tabs", json::array())) {
      for (const json& sec : tab["sections"]) all.push_back(sec);
    }
    for (const json& sec : all) {
      std::vector<json> blocks = sec["blocks"];
      for (const json& b : sec.value("advanced", json::array())) blocks.push_back(b);
      for (const json& b : blocks) {
        if (b["kind"] != "settings") continue;
        for (const json& k : b["keys"]) {
          ++homes[k.get<std::string>()];
          section_of[k.get<std::string>()] = p["id"].get<std::string>() + "/" + sec["id"].get<std::string>();
        }
      }
    }
  }
  for (const json& s : settings["settings"]) {
    const std::string key = s["key"].get<std::string>();
    CHECK_MESSAGE(homes[key] == 1, key);
    CHECK(section_of[key].rfind(s["home"].get<std::string>() + "/", 0) == 0);
    if (key.rfind("slots.", 0) == 0) {
      const std::string slot = key.substr(6, key.find('.', 6) - 6);
      CHECK(section_of[key] == "ai.models/" + slot);
    }
  }
  CHECK(homes.size() == settings["settings"].size());
  for (const json& t : pages["topics"]) CHECK(t["id"] != "ai.slots");
}

TEST_CASE("space: a job's status follows its provider's health, in the lamp's tones; nothing before EldenNet knows it") {
  Space s;
  s.watched(sub("ai.jobs"));
  const auto jobs_frames = [&](const SpaceFacts& f, std::uint64_t now) {
    std::vector<json> out;
    for (const json& j : collect(s, f, now)) {
      if (j.value("topic", "") == "ai.jobs") out.push_back(j["body"]);
    }
    return out;
  };
  // No facts yet: no frame at all, so no card draws a status (GraceUI D202, invariant 2).
  CHECK(jobs_frames(title(), 0).empty());
  CHECK(s.body("ai.jobs", title()).empty());
  AiFacts ai = ai_own();
  SpaceFacts f = title();
  f.ai = &ai;
  std::vector<json> frames = jobs_frames(f, 10);
  REQUIRE(frames.size() == 1);
  const json ready{{"word", "Ready"}, {"tone", "moss"}};
  CHECK(frames[0] == json{{"dialogue", ready}, {"bark_live", ready}, {"bark_pool", ready}, {"memory", ready},
                          {"selftest", ready}});
  CHECK(jobs_frames(f, 20).empty());  // unchanged: not again
  // The cloud provider rate-limited: its jobs cool down; Memory, on the server of one's own, stays ready.
  ai.providers[0].cooling_s = 30;
  frames = jobs_frames(f, 30);
  REQUIRE(frames.size() == 1);
  CHECK(frames[0]["dialogue"] == json{{"word", "Cooling down"}, {"tone", "ember"}});
  CHECK(frames[0]["selftest"] == frames[0]["dialogue"]);  // it borrows Conversation's models
  CHECK(frames[0]["memory"] == ready);
  ai.providers[0].cooling_s = 29;  // the seconds left are Health's: the card's word does not change with them
  CHECK(jobs_frames(f, 40).empty());
  // Its models resting after failures (every model of the job, its fallback too).
  ai.providers[0].cooling_s = 0;
  ai.slots[2].healthy = false;
  frames = jobs_frames(f, 50);
  REQUIRE(frames.size() == 1);
  CHECK(frames[0]["bark_pool"] == json{{"word", "Resting after failures"}, {"tone", "ember"}});
  CHECK(frames[0]["dialogue"] == ready);
  // Its key refused (the model list's 401, or the health's disabled provider), or asked for: failing.
  ai.slots[2].healthy = true;
  ai.providers[0].key = AiKey::Rejected;
  frames = jobs_frames(f, 60);
  REQUIRE(frames.size() == 1);
  CHECK(frames[0]["dialogue"] == json{{"word", "Failing"}, {"tone", "rot"}});
  CHECK(frames[0]["bark_pool"] == frames[0]["dialogue"]);
  CHECK(frames[0]["memory"] == ready);
  ai.providers[0].key = AiKey::Set;
  ai.providers[0].disabled = true;
  CHECK(s.body("ai.jobs", f)["dialogue"]["word"] == "Failing");
  ai.providers[1].key = AiKey::Rejected;
  ai.providers[1].key_place = AiKeyPlace::None;
  CHECK(s.body("ai.jobs", f)["memory"]["word"] == "Failing");
  // Recovered: the right key again, and the provider answering.
  ai.providers[0].disabled = false;
  ai.providers[1].key = AiKey::NotNeeded;
  frames = jobs_frames(f, 70);
  REQUIRE(frames.size() == 1);
  CHECK(frames[0]["dialogue"] == ready);
  CHECK(frames[0]["memory"] == ready);
  // No key kept yet, and no model at all.
  ai.providers[0].key = AiKey::Missing;
  CHECK(s.body("ai.jobs", f)["dialogue"] == json{{"word", "No key yet"}, {"tone", "glint"}});
  ai.providers[0].key = AiKey::Set;
  ai.slots[1].model.clear();
  CHECK(s.body("ai.jobs", f)["bark_live"] == json{{"word", "No model"}, {"tone", "glint"}});
  // A first run, with no providers file: no job, so no member, and nothing drawn; the budgets are there all the same.
  SpaceFacts first = title();
  first.ai = &ai_fixture("starting");
  CHECK(s.body("ai.jobs", first).empty());
  CHECK(s.body("ai.budgets", first).size() == 5);
}

TEST_CASE("space: a job's fallback: its provider and model from that provider's list, written together or waiting") {
  Space s;
  AiFacts ai = ai_own();
  SpaceFacts f = title();
  f.ai = &ai;
  std::vector<std::string> frames;
  s.collect(f, 0, frames);
  const auto set = [&](const std::string& key, const json& value) {
    return frame(s.command(cmd({{"key", key}, {"value", value}}), f));
  };
  const auto reset = [&](const std::string& key) {
    return frame(s.command(cmd({{"scope", "key"}, {"target", key}}, "settings.reset"), f));
  };
  const auto choices = [&](std::string_view topic) {
    std::vector<std::string> out;
    const json body = s.body(topic, f);  // held: a range-for over a temporary's member would dangle (WHY§269)
    for (const json& c : body["choices"]) out.push_back(c["value"].get<std::string>());
    return out;
  };
  // The rows follow the facts: Conversation's fallback, the preset's; none for Memory; the changed rows Memory's model.
  CHECK(s.values()["slots.dialogue.fallback"] == "talker-large");
  CHECK(s.values()["slots.dialogue.fallback_provider"] == "openrouter");
  CHECK(s.values()["slots.memory.fallback"] == "");
  CHECK(s.values()["slots.memory.fallback_provider"] == "");
  CHECK(s.settings_body()["changed"] == json::array({"slots.memory.provider", "slots.memory.model"}));
  // A fallback's choices: what its provider listed, or, with none yet, what the job's provider listed.
  CHECK(choices("ai.choices.dialogue.fallback").size() == 5);
  CHECK(choices("ai.choices.memory.fallback") == std::vector<std::string>{"lantern-keeper", "lantern-talker"});

  // Memory's fallback from its provider's list: written with that provider; not the job's own model there; not one it
  // did not list.
  json r = set("slots.memory.fallback", "lantern-keeper");
  CHECK(r["error"] == "invalid_value");
  CHECK(r["detail"] == "That is Memory's own model: choose another for its fallback.");
  CHECK(set("slots.memory.fallback", "talker-large")["detail"] ==
        "lantern-box does not list that model: press List models on Providers again.");
  CHECK(set("slots.memory.fallback", "lantern-talker")["ok"] == true);
  std::vector<Space::Change> changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "slots.memory.fallback");
  CHECK(changes[0].value == "lantern-talker");
  CHECK(changes[0].provider == "lantern-box");
  // A job that borrows another's models, or has none, takes no fallback until it has a model of its own.
  r = set("slots.selftest.fallback", "talker-small");
  CHECK(r["error"] == "not_now");
  CHECK(r["detail"] == "Self-test uses Conversation's models: choose a model of its own first.");
  CHECK(set("slots.selftest.fallback_provider", "lantern-box")["detail"] == r["detail"]);
  // A fallback's provider: one of the providers, its key not refused.
  CHECK(set("slots.bark_live.fallback_provider", "invented-nowhere")["detail"] ==
        "That provider is not in your providers file: add it on Providers first.");
  ai.providers[1].key = AiKey::Rejected;
  CHECK(set("slots.bark_live.fallback_provider", "lantern-box")["detail"] ==
        "lantern-box asks for a key: paste it on its row on Providers first.");
  ai.providers[1].key = AiKey::NotNeeded;
  CHECK(s.take_changes().empty());
  // Quick lines has no fallback: its provider chosen waits on its row, its choices that provider's; then a model of it.
  CHECK(set("slots.bark_live.fallback_provider", "lantern-box")["ok"] == true);
  CHECK(s.take_changes().empty());
  CHECK(s.values()["slots.bark_live.fallback_provider"] == "lantern-box");
  CHECK(choices("ai.choices.bark_live.fallback") == std::vector<std::string>{"lantern-keeper", "lantern-talker"});
  CHECK(set("slots.bark_live.fallback", "lantern-keeper")["ok"] == true);
  changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "slots.bark_live.fallback");
  CHECK(changes[0].provider == "lantern-box");
  // A provider that lists the fallback now takes it at once; the fallback's own again changes nothing.
  ai.models[1].ids.push_back("talker-large");
  CHECK(set("slots.dialogue.fallback_provider", "lantern-box")["ok"] == true);
  changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "slots.dialogue.fallback");
  CHECK(changes[0].value == "talker-large");
  CHECK(changes[0].provider == "lantern-box");
  CHECK(set("slots.dialogue.fallback_provider", "openrouter")["ok"] == true);
  CHECK(set("slots.dialogue.fallback", "talker-large")["ok"] == true);
  CHECK(s.take_changes().empty());

  // The DLL takes Conversation's and Memory's fallbacks: their fallback rows count as changed, the rest as before.
  ai.slots[0].fallback_provider = "lantern-box";
  ai.slots[0].fallback_picked = true;
  ai.slots[3].fallback = "lantern-talker";
  ai.slots[3].fallback_provider = "lantern-box";
  ai.slots[3].fallback_picked = true;
  ai.revision = 4;
  s.collect(f, 10, frames);
  CHECK(s.values()["slots.dialogue.fallback_provider"] == "lantern-box");
  CHECK(s.settings_body()["changed"] == json::array({"slots.dialogue.fallback_provider", "slots.dialogue.fallback",
                                                     "slots.memory.provider", "slots.memory.model",
                                                     "slots.memory.fallback_provider", "slots.memory.fallback"}));
  // A fallback row's reset, or its picker's Default, takes the fallback alone away.
  CHECK(reset("slots.dialogue.fallback_provider")["ok"] == true);
  changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "slots.dialogue.fallback");
  CHECK(changes[0].value == "");
  CHECK(set("slots.dialogue.fallback", "")["ok"] == true);
  CHECK(s.take_changes().size() == 1);
  // A reset of the job's own rows takes its whole pick away, its fallback with it: one change.
  CHECK(reset("slots.memory.provider")["ok"] == true);
  changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].key == "slots.memory.model");
  CHECK(changes[0].value == "");
  // A page's reset: each job's table once, the fallbacks that stand alone once each.
  CHECK(frame(s.command(cmd({{"scope", "page"}, {"target", "ai.models"}, {"confirm", true}}, "settings.reset"),
                        f))["ok"] == true);
  changes = s.take_changes();
  REQUIRE(changes.size() == 2);
  CHECK(changes[0].key == "slots.memory.model");
  CHECK(changes[1].key == "slots.dialogue.fallback");
  // A provider waiting for a fallback goes when its entry does; with no model, a job takes no fallback.
  CHECK(set("slots.memory.fallback_provider", "openrouter")["ok"] == true);
  CHECK(s.values()["slots.memory.fallback_provider"] == "openrouter");
  AiFacts none = ai_own();
  none.providers.erase(none.providers.begin());
  none.slots[1].model.clear();
  none.revision = 5;
  f.ai = &none;
  s.collect(f, 20, frames);
  CHECK(s.values()["slots.memory.fallback_provider"] == "");
  CHECK(set("slots.bark_live.fallback", "lantern-talker")["detail"] == "Choose Quick lines' model first.");
}

TEST_CASE("space: Content packs: one topic, a member per section; relative paths, lines and fixed words alone") {
  namespace pr = en::proto;
  Space s;
  // Before the first load nothing (absence is silent): an empty body, so each section shows its own words.
  CHECK(s.body("content", facts_of("starting")).empty());
  s.watched(sub("content"));
  const std::vector<json> first = collect(s, facts_of("starting"), 0);
  REQUIRE(first.size() == 1);
  CHECK(first[0]["topic"] == "content");
  CHECK(first[0]["body"] == json::object());

  const SpaceFacts title = facts_of("title");
  const json b = s.body("content", title);
  CHECK(clean(b));
  // Shipped content: name, version and credit; the cast's counts; knowledge and rules included, never counted (SP5).
  std::vector<std::pair<std::string, std::string>> shipped;
  for (const json& i : b["shipped"]["items"]) shipped.emplace_back(i["label"], i["value"]);
  using Items = std::vector<std::pair<std::string, std::string>>;
  CHECK(shipped == Items{{"Name", "EldenNet's own content"},
                         {"Version", "0.1.0"},
                         {"Credit", "Written for EldenNet, in its own words"},
                         {"Personas", "2"},
                         {"Cards", "3"},
                         {"Knowledge", "Included"},
                         {"Event rules", "Included"}});
  // Your overrides: a row a file, or a row a problem with its line and the content layer's words.
  const json& rows = b["overrides"]["rows"];
  REQUIRE(rows.size() == 3);
  CHECK(rows[0] == json{{"file", "disable.toml"}, {"state", "Loaded"}, {"line", "Line 3"},
                        {"problem", "cards[1]: names no shipped card, so nothing is turned off"}});
  CHECK(rows[1]["file"] == "layers/persona-ferryman.toml");
  CHECK(rows[1]["state"] == "Refused");
  CHECK(rows[1]["line"] == "");  // the whole file
  CHECK(rows[2] == json{{"file", "layers/persona-weaver.toml"}, {"state", "Loaded"}, {"line", ""}, {"problem", ""}});
  CHECK(b["overrides"]["columns"].size() == 4);
  // Turned off: personas and cards by id.
  CHECK(b["off"]["items"] == json::array({json{{"label", "Personas"}, {"value", "heron_watcher"}},
                                          json{{"label", "Cards"}, {"value", "salt_crier"}}}));
  // Reloads: time, what it read, the result.
  CHECK(b["reloads"]["rows"] == json::array({json{{"time", "0:00:06"}, {"layer", "Shipped content and your overrides"},
                                                  {"result", "Loaded, with 2 warnings"}}}));

  // In the world: a reload refused, the content of the start still in use; the files waiting; one left out.
  const json w = s.body("content", facts_of("world"));
  CHECK(w["reloads"]["rows"][0] == json{{"time", "0:21:15"}, {"layer", "Your overrides"},
                                        {"result", "Refused for a problem: the content loaded before stays in use"}});
  const json& wrows = w["overrides"]["rows"];
  REQUIRE(wrows.size() == 5);
  CHECK(wrows[0]["state"] == "Not loaded: another file was refused");
  CHECK(wrows[3] == json{{"file", "personas/ferryman.toml"}, {"state", "Refused"}, {"line", "Line 12"},
                         {"problem", "voice.cadence: is longer than 60 words"}});
  CHECK(wrows[4]["file"] == "drafts/old/a/b/c");
  CHECK(wrows[4]["state"] == "Not read");

  // Absence is silent: no count of nothing, no empty list; a refused start says no content is in use.
  pr::ContentView v;
  v.revision = 1;
  v.reloads = {pr::ContentReload{1'000, pr::ReloadLayer::All, false, false, 3, 0}};
  SpaceFacts f = title;
  f.content = &v;
  const json e = s.body("content", f);
  CHECK(e["shipped"]["items"].size() == 3);  // name, version and credit alone
  CHECK(e["off"]["items"].empty());
  CHECK(e["overrides"]["rows"].empty());
  CHECK(e["reloads"]["rows"][0]["result"] == "Refused for 3 problems: no content is in use");
  // PV5: a path that is not relative to the folder is never shown, on any row.
  for (const char* bad : {"C:/Users/someone/x.toml", "/home/someone/x.toml", "../outside.toml", "a/../../b.toml",
                          "~/x.toml", "a\\b.toml", "personas//x.toml", "./x.toml"}) {
    CAPTURE(bad);
    CHECK_FALSE(pr::shown_path(bad));
    v.overrides = {pr::OverrideFile{bad, pr::OverrideState::Loaded, {}}};
    v.left_out = {pr::OverrideFile{bad, pr::OverrideState::LeftOut, {}}};
    const json r = s.body("content", f);
    CHECK(r["overrides"]["rows"][0]["file"] == "Other files");
    CHECK(r["overrides"]["rows"][1]["file"] == "Other files");
  }
  CHECK(pr::shown_path("layers/persona-weaver.toml"));
  CHECK(pr::shown_path("disable.toml"));
  // Read-only: the page sends no command, and its topic is the only one it reads.
  const json pages = json::parse(space_pages());
  for (const json& p : pages["pages"]) {
    if (p["id"] != "content") continue;
    CHECK_FALSE(p.contains("actions"));
    CHECK(p["category"] == "content");
    CHECK(p["increment"] == "0.2");
    for (const json& sec : p["sections"]) {
      for (const json& blk : sec["blocks"]) {
        CHECK(blk["topic"] == "content");
        CHECK_FALSE(blk.contains("row_actions"));
      }
    }
  }
}

TEST_CASE("space: the log's rows are scrubbed of a SteamID64's shape; the clock") {
  CHECK(scrubbed("steam id=76561197960287930 n=12") == "steam id=redacted n=12");
  CHECK(scrubbed("n=765611979602879301") == "n=765611979602879301");  // 18 digits: not the shape
  CHECK(scrubbed("frames=12345678901234567") == "frames=12345678901234567");
  CHECK(clock_of(0) == "0:00:00");
  CHECK(clock_of(3'723'000) == "1:02:03");
  Space s;
  s.log_line("x id=76561197960287930", 5, LogLevel::Info);
  CHECK(s.log_rows()[0]["line"] == "x id=redacted");
}

TEST_CASE("space: each log row's level is the logger's, in GraceUI D257's words, never read from its text") {
  Space s;
  s.log_line("ai_fail slot=dialogue class=server", 10, LogLevel::Error);
  s.log_line("tripped group=camera", 20, LogLevel::Warning);
  s.log_line("init state=ready", 30, LogLevel::Info);
  s.log_line("watch n=3", 40, LogLevel::Debug);
  s.log_line("step n=9", 50, LogLevel::Trace);
  // A line whose words name a level keeps the level it was written at.
  s.log_line("error warning level=error", 60, LogLevel::Info);
  s.log_line("info only", 70, LogLevel::Error);
  const json rows = s.log_rows();
  REQUIRE(rows.size() == 7);
  std::vector<std::string> levels;
  for (const json& r : rows) levels.push_back(r["level"].get<std::string>());
  CHECK(levels == std::vector<std::string>{"error", "warning", "info", "debug", "trace", "info", "error"});
  // The stream's frames carry it, with each row's seq, time and scrubbed line: what both views filter.
  s.watched(sub("logs"));
  const auto out = collect(s, title(), 1000);
  REQUIRE(out.size() == 1);
  REQUIRE(out[0]["rows"].size() == 7);
  for (std::size_t i = 0; i < 7; ++i) {
    const json& row = out[0]["rows"][i];
    CHECK(row == rows[i]);
    CHECK(row.size() == 4);  // seq, time, line, level
    CHECK(row["level"] == levels[i]);
  }
  // And the page data: Logs' log tail has the bar GraceUI D257 gives, a level filter and a search in the lines, and no
  // sort (the rows come in time's order).
  const json pages = json::parse(space_pages());
  json tail;
  for (const json& page : pages["pages"]) {
    if (page["id"] != "system.logs") continue;
    for (const json& sec : page["sections"]) {
      for (const json& b : sec["blocks"]) {
        if (b.value("component", "") == "log_tail") tail = b;
      }
    }
  }
  CHECK(tail["topic"] == "logs");
  CHECK(tail["props"]["newest_first"] == true);
  CHECK(tail["filter_bar"] == json{{"filters", json::array({{{"column", "level"}, {"label", "Level"}}})},
                                   {"search", {{"columns", json::array({"line"})}, {"label", "Find in the log"}}}});
}

// ---- Docs (PAGES §4.6.9; GraceUI D251): what tools/docs/build.py bundled, checked again as GraceUI's reader checks
// it.

namespace {

std::size_t code_points(const std::string& s) {
  std::size_t n = 0;
  for (const char c : s) n += (static_cast<unsigned char>(c) & 0xC0) != 0x80 ? 1 : 0;
  return n;
}

// A paragraph's, an item's or a note's spans: each a {text} with one mark at most, under 2000 characters in all; each
// link's document one of the list's and its heading one of that document's (GraceUI D251's rules, and the README's).
void check_spans(const json& spans, const std::map<std::string, std::vector<std::string>>& headings) {
  REQUIRE(spans.is_array());
  REQUIRE_FALSE(spans.empty());
  std::size_t total = 0;
  for (const json& s : spans) {
    REQUIRE(s.is_object());
    const std::string text = s.at("text").get<std::string>();
    CHECK_FALSE(text.empty());
    total += code_points(text);
    CHECK(static_cast<int>(s.value("strong", false)) + static_cast<int>(s.value("code", false)) +
              static_cast<int>(s.contains("doc")) <= 1);
    if (s.contains("doc")) {
      const auto d = headings.find(s["doc"].get<std::string>());
      REQUIRE(d != headings.end());
      if (s.contains("heading")) {
        CHECK(std::find(d->second.begin(), d->second.end(), s["heading"].get<std::string>()) != d->second.end());
      }
    }
  }
  CHECK(total < 2000);
}

}  // namespace

TEST_CASE("space: Docs: a master page, its list, and each document bundled within GraceUI's limits") {
  const json pages = json::parse(space_pages());
  const json* page = nullptr;
  for (const json& p : pages["pages"]) {
    if (p["id"] == "docs") page = &p;
  }
  REQUIRE(page != nullptr);
  CHECK((*page)["category"] == "docs");
  CHECK((*page)["increment"] == "0.2");
  CHECK((*page)["master"]["topic"] == std::string(kDocsList));
  CHECK((*page)["master"]["detail"] == std::string(kDocsFamily));
  // The list's own search, on its rows' titles and headings alone (D250): never a document's body.
  CHECK((*page)["master"]["filter_bar"] == json{{"search", {{"columns", {"title", "headings"}},
                                                             {"label", "Find a document"}}}});
  REQUIRE((*page)["sections"].size() == 1);
  CHECK((*page)["sections"][0]["blocks"] == json::array({json{{"kind", "document"}, {"topic", "docs"}}}));

  const Space s;
  const SpaceFacts f = facts_of("title");
  const json list = s.body(kDocsList, f);
  CHECK(list["columns"] == json::array({json{{"id", "title"}, {"label", "Document"}},
                                        json{{"id", "headings"}, {"label", "Covers"}}}));
  std::vector<std::string> ids;
  for (const json& r : list["rows"]) ids.push_back(r["id"].get<std::string>());
  // PAGES §4.6.9's documents, in its order.
  CHECK(ids == std::vector<std::string>{"getting-started", "installing", "provider", "privacy", "spoilers", "keys",
                                        "troubleshooting", "writing-content", "licences", "about"});
  std::map<std::string, std::vector<std::string>> headings;
  for (const BundledDoc& d : bundled_docs()) {
    const json body = json::parse(d.body);
    for (const json& b : body["blocks"]) {
      if (b.contains("heading")) headings[std::string(d.id)].push_back(b["heading"].get<std::string>());
    }
    headings[std::string(d.id)];
  }
  REQUIRE(bundled_docs().size() == list["rows"].size());
  for (std::size_t i = 0; i < bundled_docs().size(); ++i) {
    const BundledDoc& d = bundled_docs()[i];
    INFO("docs/" << d.id);
    const json& row = list["rows"][i];
    CHECK(row["title"] == std::string(d.title));
    CHECK(row["headings"] == std::string(d.headings));
    // The bundled text is the body as the space writes it: nlohmann/json's form, keys sorted.
    const json body = s.body(std::string(kDocsFamily) + "/" + std::string(d.id), f);
    CHECK(body["title"] == std::string(d.title));
    if (d.id != kDocsAbout) CHECK(body.dump() == std::string(d.body));
    CHECK(d.body.size() < 64u * 1024);
    REQUIRE(body["blocks"].is_array());
    CHECK(body["blocks"].size() <= 400);
    std::vector<std::string> joined;
    for (const json& b : body["blocks"]) {
      REQUIRE(b.is_object());
      int kinds = 0;
      for (const char* k : {"heading", "paragraph", "list", "note", "code", "table"}) kinds += b.contains(k) ? 1 : 0;
      CHECK(kinds == 1);
      if (b.contains("heading")) {
        CHECK((b["level"] == 2 || b["level"] == 3));
        CHECK(code_points(b["heading"].get<std::string>()) <= 120);
        joined.push_back(b["heading"].get<std::string>());
      }
      if (b.contains("paragraph")) check_spans(b["paragraph"], headings);
      if (b.contains("note")) check_spans(b["note"], headings);
      if (b.contains("list")) {
        CHECK(b["list"].size() <= 100);
        for (const json& item : b["list"]) check_spans(item, headings);
      }
      if (b.contains("code")) CHECK(b["code"].get<std::string>().size() < 16u * 1024);
      if (b.contains("table")) {
        CHECK(b["table"]["columns"].size() <= 8);
        CHECK(b["table"]["rows"].size() <= 200);
      }
    }
    std::string words;
    for (const std::string& h : joined) words += (words.empty() ? "" : " · ") + h;
    CHECK(words == d.headings);
    // No address of any kind (PV6, invariant 6): a scheme is named ("https://"), never followed by a host; and no
    // slot left unfilled.
    const std::string text = body.dump();
    for (std::size_t at = text.find("://"); at != std::string::npos; at = text.find("://", at + 3)) {
      CHECK(std::isalnum(static_cast<unsigned char>(text[at + 3])) == 0);
    }
    CHECK(text.find("{eldennet_version}") == std::string::npos);
    CHECK(text.find("{graceui_version}") == std::string::npos);
    CHECK(text.find("{build}") == std::string::npos);
  }
  CHECK(s.body("docs/none", f).empty());  // no such document: no body
}

TEST_CASE("space: Docs: About's slots from what EldenNet knows, a row left out without its value; published once") {
  const auto about_rows = [](const json& body) {
    std::map<std::string, std::string> rows;
    for (const json& b : body["blocks"]) {
      if (!b.contains("table")) continue;
      for (const json& r : b["table"]["rows"]) rows[r["part"].get<std::string>()] = r["version"].get<std::string>();
    }
    return rows;
  };
  Space s;
  SpaceFacts f = facts_of("title");
  f.label = "0.1.0-test.1";
  CHECK(about_rows(s.body("docs/about", f)) == std::map<std::string, std::string>{
                                                   {"EldenNet", "0.1.0"}, {"GraceUI", "0.2.12"},
                                                   {"Build", "0.1.0-test.1"}});
  SpaceFacts unknown = f;
  unknown.graceui = 0;
  unknown.label = {};
  CHECK(about_rows(s.body("docs/about", unknown)) == std::map<std::string, std::string>{{"EldenNet", "0.1.0"}});

  // The list and a document when watched, and not again: they are bundled. About again when its slots change.
  s.watched(sub("docs.list"));
  s.watched(sub("docs/keys"));
  s.watched(sub("docs/about"));
  s.watched(sub("docs/none"));
  auto out = collect(s, f, 1000);
  std::vector<std::string> topics;
  for (const json& o : out) topics.push_back(o["topic"].get<std::string>());
  std::sort(topics.begin(), topics.end());
  CHECK(topics == std::vector<std::string>{"docs.list", "docs/about", "docs/keys"});  // never docs/none
  CHECK(collect(s, f, 2000).empty());
  f.graceui = GRACEUI_VERSION_PACK(0, 2, 44);
  out = collect(s, f, 3000);
  REQUIRE(out.size() == 1);
  CHECK(out[0]["topic"] == "docs/about");
  CHECK(out[0]["rev"] == 2);
  CHECK(about_rows(out[0]["body"])["GraceUI"] == "0.2.44");
}

// ---- Tools › Bug report (PAGES §4.6.8; GraceUI D251; space.hpp).

TEST_CASE("space: Bug report: a form with one prose field, the preview with its command; parts in plain data") {
  const json pages = json::parse(space_pages());
  const json* page = nullptr;
  for (const json& p : pages["pages"]) {
    if (p["id"] == "tools.report") page = &p;
  }
  REQUIRE(page != nullptr);
  CHECK((*page)["category"] == "tools");
  CHECK((*page)["increment"] == "0.2");
  REQUIRE((*page)["sections"].size() == 2);
  CHECK_FALSE((*page)["sections"][0]["description"].get<std::string>().empty());  // the caution
  CHECK((*page)["sections"][0]["blocks"][0] ==
        json{{"kind", "form"}, {"command", "report.build"}, {"submit", "Add to the report"},
             {"fields", {{{"id", "what"}, {"label", "What happened"}, {"max", 2000}, {"prose", true}}}}});
  CHECK((*page)["sections"][1]["blocks"][0] == json{{"kind", "component"}, {"component", "report_preview"},
                                                    {"topic", "report"}, {"command", "report.part"}});
  // No copy or save in 0.2 (PAGES §4.6.8): neither command is the space's yet.
  for (const json& c : pages["commands"]) CHECK((c != "report.copy" && c != "report.save"));

  Space s;
  s.log_line("boot pid=4242 dll=eldennet.dll probe_build=0 version_minor=1 version=dev", 3, LogLevel::Info);
  s.log_line("tripped group=camera", 1200400, LogLevel::Warning);
  const SpaceFacts f = facts_of("world");
  const json body = s.body("report", f);
  std::vector<std::string> ids;
  std::map<std::string, json> parts;
  for (const json& p : body["parts"]) {
    ids.push_back(p["id"].get<std::string>());
    parts[p["id"].get<std::string>()] = p;
    CHECK(p["included"] == true);
  }
  CHECK(ids == std::vector<std::string>{"versions", "state", "settings", "content", "log"});  // nothing typed yet
  CHECK(parts["versions"]["optional"] == false);
  CHECK(parts["state"]["optional"] == false);
  for (const char* id : {"settings", "content", "log"}) CHECK(parts[id]["optional"] == true);
  const auto value = [&](const std::string& part, const std::string& label) {
    for (const json& field : parts[part]["fields"]) {
      if (field["label"] == label) return field["value"].get<std::string>();
    }
    return std::string("(none)");
  };
  CHECK(value("versions", "EldenNet") == "0.1.0");
  CHECK(value("versions", "GraceUI") == "0.2.12");
  CHECK(value("versions", "Game build") == "3.14.15.0, supported");
  CHECK(value("state", "State") == "Observing");
  CHECK(value("state", "Stopped after faults") == "The camera");
  CHECK(value("state", "Recording") == "On");
  CHECK(value("settings", "Values").find("debug.log_level = \"info\"") != std::string::npos);
  // A text value shaped like a local address or a path is left out, as the log's are (PV5): an invented one here.
  s.set_value("slots.dialogue.model", "http://192.0.2.7:8080/v1");
  s.set_value("slots.memory.model", "C:\\models\\invented.gguf");
  const json again = s.body("report", f);
  std::string values_now;
  for (const json& p : again["parts"]) {
    if (p["id"] != "settings") continue;
    for (const json& field : p["fields"]) {
      if (field["label"] == "Values") values_now = field["value"].get<std::string>();
    }
  }
  CHECK(values_now.find("slots.dialogue.model = (left out)") != std::string::npos);
  CHECK(values_now.find("slots.memory.model = (left out)") != std::string::npos);
  CHECK(values_now.find("192.0.2.7") == std::string::npos);
  CHECK(values_now.find("invented.gguf") == std::string::npos);
  CHECK(value("content", "Personas") != "(none)");
  // The log's newest lines, its clock before each, in the mono face with its line breaks.
  CHECK(parts["log"]["fields"][0]["code"] == true);
  CHECK(value("log", "Newest lines").rfind("0:00:00  boot pid=4242", 0) == 0);
  CHECK(value("log", "Newest lines").find("\n0:20:00  tripped group=camera") != std::string::npos);
  CHECK(clean(body));
}

TEST_CASE("space: Bug report: what happened is kept for the preview alone; the switches; refusals in words") {
  Space s;
  s.log_line("boot pid=4242 dll=eldennet.dll probe_build=0 version_minor=1 version=dev", 3, LogLevel::Info);
  const SpaceFacts f = facts_of("title");
  for (const std::string_view topic : {"report", "status", "errors", "system.status", "settings", "content"}) {
    s.watched(sub(topic));
  }
  s.watched(sub("logs"));
  (void)collect(s, f, 1000);
  (void)s.take_notes();

  const std::string typed =
      "  The lantern keeper stopped mid-line, and the box stayed open.\r\nPressing Esc closed it. ";
  const std::string answer = s.command(cmd({{"what", typed}}, "report.build"), f);
  CHECK(frame(answer)["ok"] == true);
  CHECK(answer.find("lantern keeper") == std::string::npos);  // the answer never echoes it
  const std::vector<SpaceNote> notes = s.take_notes();
  REQUIRE(notes.size() == 1);
  CHECK(notes[0].topic == "report.build");
  CHECK(notes[0].key.empty());
  CHECK(notes[0].result == "ok");
  // At once, not at the preview's rate: its words, the spaces round them taken off and the line break kept.
  auto out = collect(s, f, 1100);
  bool published = false;
  for (const json& o : out) {
    const std::string text = o.dump();
    if (o["topic"] == "report") {
      published = true;
      CHECK(o["body"]["parts"][0] ==
            json{{"id", "happened"}, {"label", "What happened"}, {"optional", false}, {"included", true},
                 {"fields", {{{"label", "In your words"},
                              {"value", "The lantern keeper stopped mid-line, and the box stayed open.\n"
                                        "Pressing Esc closed it."}}}}});
    } else {
      CHECK(text.find("lantern keeper") == std::string::npos);  // in no other topic
    }
  }
  CHECK(published);
  for (const std::string_view topic : {"status", "errors", "system.status", "settings", "content", "ai.status"}) {
    CHECK(s.body(topic, f).dump().find("lantern keeper") == std::string::npos);
  }
  CHECK(s.log_rows().dump().find("lantern keeper") == std::string::npos);

  // Scrubbed as the log is (PV5): a SteamID64's shape, a path, an address (a documentation range's, invented).
  CHECK(frame(s.command(cmd({{"what", "My id 76561197960287930, my save in C:\\Users\\Someone\\Saves, "
                                      "my server at 192.0.2.20:8080."}},
                            "report.build"),
                        f))["ok"] == true);
  const std::string shown = s.body("report", f)["parts"][0]["fields"][0]["value"].get<std::string>();
  CHECK(shown == "My id redacted, my save in redacted my server at redacted");
  // Refused, in words, the words kept as they were: over 2000 characters, or a control character.
  const json over = frame(s.command(cmd({{"what", std::string(2001, 'a')}}, "report.build"), f));
  CHECK(over["error"] == "invalid_value");
  CHECK(over["detail"] == "That is over 2000 characters: shorten it.");
  CHECK(frame(s.command(cmd({{"what", std::string(2000, 'a')}}, "report.build"), f))["ok"] == true);
  CHECK(frame(s.command(cmd({{"what", std::string("a\x01")}}, "report.build"), f))["error"] == "invalid_value");
  CHECK(frame(s.command(cmd({{"what", 12}}, "report.build"), f))["error"] == "invalid_value");
  CHECK(s.body("report", f)["parts"][0]["fields"][0]["value"] == std::string(2000, 'a'));
  // Every field sent, an empty one as "" (D251): the words cleared, the part gone.
  CHECK(frame(s.command(cmd({{"what", ""}}, "report.build"), f))["ok"] == true);
  CHECK(s.body("report", f)["parts"][0]["id"] == "versions");

  // An optional part's switch (D251): left out, then in again; the others are always in.
  CHECK(frame(s.command(cmd({{"id", "log"}, {"on", false}}, "report.part"), f))["ok"] == true);
  json parts = s.body("report", f)["parts"];
  for (const json& p : parts) CHECK(p["included"] == (p["id"] != "log"));
  const json always = frame(s.command(cmd({{"id", "versions"}, {"on", false}}, "report.part"), f));
  CHECK(always["error"] == "invalid_value");
  CHECK(always["detail"] == "That part is always in the report.");
  CHECK(frame(s.command(cmd({{"id", "log"}, {"on", "no"}}, "report.part"), f))["error"] == "invalid_value");
  CHECK(frame(s.command(cmd({{"id", "log"}, {"on", true}}, "report.part"), f))["ok"] == true);
  for (const json& p : s.body("report", f)["parts"]) CHECK(p["included"] == true);
  for (const SpaceNote& n : s.take_notes()) {
    CHECK(n.topic.rfind("report.", 0) == 0);
    CHECK((n.key.empty() || n.key == "log" || n.key == "versions"));
  }
}

TEST_CASE("space: Bug report: the log's tail scrubbed again, settings without secrets or paths") {
  Space s(R"({"schema": {"number": 1, "hash": "0"}, "settings": [
    {"key": "a.key", "type": "secret", "default": "", "home": "settings.developer"},
    {"key": "a.model", "type": "text", "default": "", "home": "settings.developer"},
    {"key": "a.flag", "type": "bool", "default": true, "home": "settings.developer"}]})");
  REQUIRE(s.valid());
  s.set_value("a.key", "an invented key");
  s.set_value("a.model", "C:\\Users\\Someone\\models\\lamp.gguf");
  for (int i = 0; i < 40; ++i) {
    s.log_line("tick n=" + std::to_string(i), 1000u * static_cast<unsigned>(i), LogLevel::Debug);
  }
  s.log_line("ai_change what=key name=76561197960287930 host=198.51.100.7", 50000, LogLevel::Info);
  const json parts = s.body("report", facts_of("title"))["parts"];
  std::string values, lines;
  for (const json& p : parts) {
    for (const json& field : p["fields"]) {
      if (field["label"] == "Values") values = field["value"].get<std::string>();
      if (field["label"] == "Newest lines") lines = field["value"].get<std::string>();
    }
  }
  CHECK(values == "a.model = (left out)\na.flag = true");  // never a secret, nor its status
  CHECK(std::count(lines.begin(), lines.end(), '\n') == 29);  // the newest 30
  CHECK(lines.rfind("0:00:11  tick n=11", 0) == 0);
  CHECK(lines.substr(lines.rfind('\n') + 1) == "0:00:50  ai_change what=key name=redacted host=redacted");
  CHECK(parts.dump().find("invented key") == std::string::npos);
}

TEST_CASE("space: AI › Requests and Home's Model calls: PAGES' columns and words, the sort keys, PV4 and PV5") {
  const Space s;
  const SpaceFacts f = facts_of("world");
  const json calls = s.body("calls", f);
  const json& requests = calls["requests"];
  const auto ids_of = [](const json& columns) {
    std::vector<std::string> ids;
    for (const json& c : columns) ids.push_back(c["id"].get<std::string>());
    return ids;
  };
  // The page data: Requests' table on `calls`' `requests` member, with its filter bar (GraceUI D250: job, purpose and
  // outcome; sorted on the rows' hidden keys, newest first; no search, by PV4); Home's card on its `home` member.
  const json pages = json::parse(space_pages());
  json table, card;
  for (const json& page : pages["pages"]) {
    for (const json& sec : page.value("sections", json::array())) {
      for (const json& b : sec["blocks"]) {
        if (b.value("topic", "") != "calls") continue;
        (page["id"] == "ai.requests" ? table : card) = b;
      }
    }
  }
  CHECK(table["props"]["field"] == "requests");
  CHECK(card["props"]["field"] == "home");
  CHECK_FALSE(card.contains("filter_bar"));
  CHECK(table["filter_bar"]["filters"] == json::array({{{"column", "slot"}, {"label", "Job"}},
                                                       {{"column", "purpose"}, {"label", "Purpose"}},
                                                       {{"column", "outcome"}, {"label", "Outcome"}}}));
  std::vector<std::string> keys;
  for (const json& o : table["filter_bar"]["sort"]) keys.push_back(o["key"].get<std::string>());
  CHECK(keys == std::vector<std::string>{"at_ms", "first_token_ms", "total_ms", "tokens"});
  CHECK(table["filter_bar"]["sort"][0]["direction"] == "descending");
  CHECK_FALSE(table["filter_bar"].contains("search"));
  // PAGES' nine columns on the row, then why and the text shown, the row's drawer (GraceUI D256): those two alone.
  CHECK(ids_of(requests["columns"]) == std::vector<std::string>{"time", "slot", "purpose", "speaker", "provider_model",
                                                                 "first_token", "total", "tokens", "outcome", "why",
                                                                 "text"});
  const auto drawers_of = [](const json& columns) {
    std::vector<std::string> ids;
    for (const json& c : columns) {
      if (c.value("drawer", false)) ids.push_back(c["id"].get<std::string>());
    }
    return ids;
  };
  CHECK(drawers_of(requests["columns"]) == std::vector<std::string>{"why", "text"});
  CHECK(requests["columns"][9] == json{{"id", "why"}, {"label", "Why"}, {"drawer", true}});
  CHECK(requests["columns"][10] == json{{"id", "text"}, {"label", "Text shown"}, {"drawer", true}});
  REQUIRE(requests["rows"].size() == 8);
  // Each row has an id, the record's, as words (a drawer follows its row by it: GraceUI reads a string id alone), one
  // of its own.
  std::set<std::string> row_ids;
  for (const json& r : requests["rows"]) {
    REQUIRE(r["id"].is_string());
    row_ids.insert(r["id"].get<std::string>());
  }
  CHECK(row_ids.size() == requests["rows"].size());
  // Newest first. A call still waiting has no first token, total or tokens: those keys are left out, so that it sorts
  // last on them (GraceUI D250), and no text.
  const json& waiting = requests["rows"][0];
  CHECK(waiting["id"] == "19");
  CHECK(waiting["at_ms"] == 3'905'000);
  CHECK(waiting["time"] == "1:05:05");
  CHECK(waiting["outcome"] == "waiting");
  for (const char* key : {"first_token_ms", "first_token", "total_ms", "total", "tokens", "text", "why"}) {
    CHECK_FALSE(waiting.contains(key));
  }
  const json& shown = requests["rows"][1];
  CHECK(shown["slot"] == "Conversation");
  CHECK(shown["purpose"] == "reply");
  CHECK(shown["speaker"] == "Old Bram");
  CHECK(shown["provider_model"] == "talker-small on openrouter");
  CHECK(shown["first_token"] == "0.64 s");
  CHECK(shown["first_token_ms"] == 640);
  CHECK(shown["total"] == "1.72 s");
  CHECK(shown["total_ms"] == 1720);
  CHECK(shown["tokens"] == 858);  // in and out, a column and its sort key at once
  CHECK(shown["outcome"] == "shown");
  CHECK(shown["text"] == "Two coins for the crossing, friend, and the river decides how dry you land.");
  CHECK_FALSE(shown.contains("why"));
  // The fallback's answer reached the screen; why the first model did not answer is kept, in words.
  CHECK(requests["rows"][2]["outcome"] == "fallback model");
  CHECK(requests["rows"][2]["provider_model"] == "talker-large on openrouter");
  CHECK(requests["rows"][2]["why"] == "Took too long to start its answer.");
  // Not shown: no text, ever (PV4).
  CHECK(requests["rows"][3]["outcome"] == "not shown");
  CHECK_FALSE(requests["rows"][3].contains("text"));
  CHECK(requests["rows"][4]["outcome"] == "timed out");
  CHECK_FALSE(requests["rows"][4].contains("first_token_ms"));
  CHECK_FALSE(requests["rows"][4].contains("tokens"));
  CHECK(requests["rows"][5]["slot"] == "Profile drafts");
  CHECK(requests["rows"][5]["purpose"] == "profile draft");
  CHECK(requests["rows"][5]["outcome"] == "answered");
  CHECK_FALSE(requests["rows"][5].contains("speaker"));
  CHECK(requests["rows"][6]["outcome"] == "failed");
  CHECK(requests["rows"][6]["why"] == "Failed on the provider's side.");
  // Every purpose and outcome a filter offers is in PAGES' words (en-proto's), every job its player's label.
  std::vector<std::string> purposes, outcomes;
  for (std::size_t i = 0; i < en::proto::kCallPurposes; ++i) {
    purposes.emplace_back(en::proto::words(static_cast<en::proto::CallPurpose>(i)));
  }
  for (std::size_t i = 0; i < en::proto::kCallOutcomes; ++i) {
    outcomes.emplace_back(en::proto::words(static_cast<en::proto::CallOutcome>(i)));
  }
  for (const json& r : requests["rows"]) {
    CHECK(std::find(purposes.begin(), purposes.end(), r["purpose"].get<std::string>()) != purposes.end());
    CHECK(std::find(outcomes.begin(), outcomes.end(), r["outcome"].get<std::string>()) != outcomes.end());
    const std::string job = r["slot"].get<std::string>();
    CHECK((job == "Conversation" || job == "Profile drafts"));
  }
  // Home's card: the newest 20 (8 here), its own columns, the text shown its drawer, no sort key, no why; each row
  // with the id its call has on Requests.
  const json& home = calls["home"];
  CHECK(ids_of(home["columns"]) == std::vector<std::string>{"time", "slot", "purpose", "speaker", "model",
                                                             "first_token", "total", "outcome", "text"});
  CHECK(drawers_of(home["columns"]) == std::vector<std::string>{"text"});
  REQUIRE(home["rows"].size() == 8);
  CHECK(home["rows"][1]["model"] == "talker-small");
  CHECK(home["rows"][1]["text"] == shown["text"]);
  for (std::size_t i = 0; i < home["rows"].size(); ++i) {
    const json& r = home["rows"][i];
    CHECK(r["id"] == requests["rows"][i]["id"]);
    for (const char* key : {"at_ms", "first_token_ms", "total_ms", "tokens", "why", "provider_model"}) {
      CHECK_FALSE(r.contains(key));
    }
  }
  // A model list has no job and no model: its provider alone.
  const json title_rows = s.body("calls", facts_of("title"))["requests"]["rows"];
  REQUIRE(title_rows.size() == 5);
  CHECK(title_rows[0]["purpose"] == "model list");
  CHECK(title_rows[0]["provider_model"] == "lantern-box");
  CHECK_FALSE(title_rows[0].contains("slot"));
  CHECK(title_rows[2]["slot"] == "Self-test");
  CHECK(title_rows[4]["purpose"] == "self-test");

  // The tiles, and the hour's line against the bound; cost only where a provider reported one (D18).
  const json usage = s.body("usage", f);
  REQUIRE(usage["tiles"].size() == 6);
  CHECK(usage["tiles"][0] == json{{"label", "Calls"}, {"value", "31"}});
  CHECK(usage["tiles"][1] == json{{"label", "Tokens in"}, {"value", "21,480"}});
  CHECK(usage["tiles"][2] == json{{"label", "Tokens out"}, {"value", "1,322"}});
  CHECK(usage["tiles"][3] == json{{"label", "Failures"}, {"value", "3"}});
  CHECK(usage["tiles"][4] == json{{"label", "Own-initiative this hour"}, {"value", "0"}});
  CHECK(usage["tiles"][5] == json{{"label", "Cost reported"}, {"value", "0.0187"}, {"unit", "by 19 calls"}});
  CHECK(usage["sentence"] ==
        "In the last hour: no own-initiative calls, and 24 of the 600 requests EldenNet may send in an hour.");
  const json title_usage = s.body("usage", facts_of("title"));
  CHECK(title_usage["tiles"].size() == 5);  // no cost reported: no cost tile
  en::proto::CallsView one;
  one.usage.own_initiative_hour = 1;
  one.usage.requests_hour = 1;
  SpaceFacts g = f;
  g.calls = &one;
  CHECK(s.body("usage", g)["sentence"] == "In the last hour: 1 own-initiative call, and 1 request sent.");  // no bound

  // No row yet (starting, at L1): empty tables, the tiles at nought, the hour's line with no bound before the providers
  // are read; and with no record at all, nothing (absence is silent).
  CHECK(s.body("calls", facts_of("starting"))["requests"]["rows"].empty());
  CHECK(s.body("calls", facts_of("banner"))["home"]["rows"].empty());
  CHECK(s.body("usage", facts_of("banner"))["sentence"] ==
        "In the last hour: no own-initiative calls, and no requests sent.");
  SpaceFacts bare = f;
  bare.calls = nullptr;
  CHECK(s.body("calls", bare).empty());
  CHECK(s.body("usage", bare).empty());

  // PV4 and PV5: no base URL, key, path or address in any frame; a model id shaped like a path left out.
  en::proto::CallsView odd = *f.calls;
  odd.rows[1].model = "C:\\Users\\someone\\models\\invented.gguf";
  odd.rows[2].model = "/home/someone/invented.gguf";
  g.calls = &odd;
  Space watching;
  watching.watched(sub("calls"));
  watching.watched(sub("usage"));
  for (const json& out : collect(watching, g, 0)) {
    CHECK(clean(out["body"]));
    CHECK_FALSE(holds(out["body"], "example.invalid"));
    CHECK_FALSE(holds(out["body"], "someone"));
    CHECK_FALSE(holds(out["body"], "fk-"));
  }
  CHECK(watching.body("calls", g)["requests"]["rows"][1]["provider_model"] == "openrouter");
  CHECK_FALSE(watching.body("calls", g)["home"]["rows"][1].contains("model"));

  // PV4 in the drawer, which shows only what its row holds: no frame of either table holds a text the record did not
  // keep as shown. A row with none (not shown, timed out, failed, waiting, a draft) has no text in any frame, and a
  // shown reply's is the record's own, on that row alone.
  std::map<std::string, std::string> kept;
  for (const en::proto::CallRow& r : f.calls->rows) {
    if (!r.text.empty()) kept[std::to_string(r.id)] = r.text;
  }
  CHECK(kept.size() == 3);
  Space drawn;
  drawn.watched(sub("calls"));
  const std::vector<json> frames = collect(drawn, f, 0);
  REQUIRE(frames.size() == 1);
  for (const char* member : {"requests", "home"}) {
    for (const json& r : frames[0]["body"][member]["rows"]) {
      const std::string id = r["id"].get<std::string>();
      if (r.contains("text")) {
        CHECK(kept.count(id) == 1);
        CHECK(r["text"] == kept[id]);
        CHECK((r["outcome"] == "shown" || r["outcome"] == "fallback model"));
      } else {
        CHECK(kept.count(id) == 0);
      }
    }
  }
}

TEST_CASE("space: Requests' body stays within GraceUI's frame to a page; the oldest rows give up their text first") {
  const Space s;
  en::proto::CallsView big;
  for (std::uint64_t i = 0; i < en::proto::kCallsKeptDeveloper; ++i) {
    en::proto::CallRow r;
    r.id = 1000 - i;
    r.at_ms = 9'000'000 - i * 1000;
    r.slot = en::proto::kDialogue;
    r.speaker = std::string(64, 'b');
    r.provider = std::string(32, 'p');
    r.model = std::string(190, 'm');
    r.first_token_ms = 500;
    r.total_ms = 1500;
    r.tokens_in = 900;
    r.tokens_out = 120;
    r.outcome = en::proto::CallOutcome::Shown;
    r.text = std::string(en::proto::kCallTextMaxBytes, 't');
    big.rows.push_back(r);
  }
  SpaceFacts f = facts_of("world");
  f.calls = &big;
  const json body = s.body("calls", f);
  const std::string text = body.dump();
  CHECK(text.size() <= kCallsBodyMax);
  const json& rows = body["requests"]["rows"];
  REQUIRE_FALSE(rows.empty());
  CHECK(rows[0].contains("text"));  // the newest keep their text
  CHECK_FALSE(rows[rows.size() - 1].contains("text"));
  bool gone = false;  // once a row gave its text up, every older one has too
  for (const json& r : rows) {
    if (!r.contains("text")) gone = true;
    if (gone) CHECK_FALSE(r.contains("text"));
  }
  CHECK(body["home"]["rows"].size() == kCallsHomeRows);
  for (const json& r : body["home"]["rows"]) CHECK(r.contains("text"));
  // A frame of it fits the frame to a page.
  Space watching;
  watching.watched(sub("calls"));
  std::vector<std::string> frames;
  watching.collect(f, 0, frames);
  REQUIRE(frames.size() == 1);
  CHECK(frames[0].size() <= GRACEUI_FRAME_TO_PAGE_MAX);
}

TEST_CASE("space: Pause AI: ai.pause {on} handed to the DLL at once; status.ai_paused; model calls refused while on") {
  Space s;
  AiFacts ai = ai_ready();
  SpaceFacts f = title();
  f.ai = &ai;
  const auto answer_of = [&](std::string_view name, const json& args = json::object()) {
    return frame(s.command(cmd(args, name), f));
  };
  // The quick switch's command (GraceUI D252): {"on": bool}, answered at once; the state follows on `status`.
  CHECK(answer_of("ai.pause", {{"on", true}})["ok"] == true);
  std::vector<AiRequest> taken = s.take_requests();
  REQUIRE(taken.size() == 1);
  CHECK(taken[0].kind == AiRequest::Kind::Pause);
  CHECK(taken[0].on);
  const std::vector<SpaceNote> notes = s.take_notes();
  REQUIRE(notes.size() == 1);
  CHECK(notes[0].topic == "ai.pause");
  CHECK(notes[0].key == "on");
  CHECK(notes[0].result == "ok");
  // Anything but a boolean `on` is refused, {} among them (what a plain header button would send: a GraceUI before
  // 0.2.49 reads Home's header switch as one, D256).
  for (const json& bad : {json::object(), json{{"on", "yes"}}, json{{"on", 1}}, json{{"off", true}}}) {
    CHECK(answer_of("ai.pause", bad)["error"] == "invalid_value");
  }
  CHECK(s.take_requests().empty());

  // Home's header switch (GraceUI D256; eldennet-changes §82, PAGES §4.6.1's chip): the quick switch's command and
  // state, beside the quick switch, which stays; secondary, not destructive, no confirmation. Its state topic is a
  // state topic of the space's own, not a family, and its command a declared one, neither sensitive nor slow.
  const json pages = json::parse(space_pages());
  json home_page;
  for (const json& p : pages["pages"]) {
    if (p["id"] == "home") home_page = p;
  }
  CHECK(home_page["actions"] == json::array({{{"id", "pause"}, {"label", "Pause AI"}, {"command", "ai.pause"},
                                              {"tone", "secondary"},
                                              {"state", {{"topic", "status"}, {"field", "ai_paused"}}}}}));
  CHECK(pages["quick"][0]["command"] == home_page["actions"][0]["command"]);
  CHECK(pages["quick"][0]["state"] == home_page["actions"][0]["state"]);
  for (const json& topic : pages["topics"]) {
    if (topic["id"] == "status") CHECK(topic["shape"] == "state");
  }
  for (const char* list : {"sensitive_commands", "slow_commands"}) {
    const json& names = pages[list];
    CHECK(std::find(names.begin(), names.end(), "ai.pause") == names.end());
  }
  // The switch sends {"on": ...} flipped from what `status` says: the space's answers to both, as the quick switch's.
  CHECK(s.body("status", f)["ai_paused"].is_boolean());

  // Off: the status says so, and its member is false from the first body, starting or not.
  json status = s.body("status", f);
  CHECK(status["ai_paused"] == false);
  CHECK(status["items"].back() == json{{"label", "Pause AI"}, {"value", "Off"}});
  CHECK(s.body("status", starting())["ai_paused"] == false);
  CHECK_FALSE(s.body("status", starting()).contains("word"));

  // On, as the core holds it: the member true; the Status card's line; at the title the dormant words lead still, in
  // the world (L2 for it, not dormant) the pause is the reason, and the AI pages lead with it (PAGES §4.4).
  f.ai_paused = true;
  status = s.body("status", f);
  CHECK(status["ai_paused"] == true);
  CHECK(status["items"].back() == json{{"label", "Pause AI"}, {"value", "On, for this session"}});
  CHECK(status["reason"] == "At the title screen: EldenNet waits until a character is loaded.");
  SpaceFacts w = facts_of("world");
  CHECK(s.body("status", w)["word"] == "Observing");
  CHECK(s.body("status", w)["reason"] == std::string(en::proto::kAiPausedStatus));
  CHECK(s.body("ai.status", w)["sentence"] ==
        "AI is paused for this session, so EldenNet calls no model. Turn Pause AI off in the quick switches to call "
        "one again.");
  CHECK(s.body("ai.status", f)["sentence"] == s.body("ai.status", w)["sentence"]);
  // The self-test and the model list are refused in en-proto's words, and nothing goes to the DLL.
  json r = answer_of("selftest.run");
  CHECK(r["error"] == "not_now");
  CHECK(r["detail"] == std::string(en::proto::kAiPausedSelfTest));
  r = answer_of("models.list");
  CHECK(r["error"] == "not_now");
  CHECK(r["detail"] == std::string(en::proto::kAiPausedModelList));
  CHECK(s.take_requests().empty());
  // The rest of the AI pages' commands are no model call: they stay open.
  CHECK(answer_of("providers.remove", {{"id", "openrouter"}})["error"] == "confirm_required");
  // Off again: handed to the DLL the same way.
  CHECK(answer_of("ai.pause", {{"on", false}})["ok"] == true);
  taken = s.take_requests();
  REQUIRE(taken.size() == 1);
  CHECK(taken[0].kind == AiRequest::Kind::Pause);
  CHECK_FALSE(taken[0].on);

  // Published: the watched status changes with the facts, at its rate.
  Space watching;
  watching.watched(sub("status"));
  f.ai_paused = false;
  std::vector<json> out = collect(watching, f, 0);
  REQUIRE(out.size() == 1);
  CHECK(out[0]["body"]["ai_paused"] == false);
  f.ai_paused = true;
  out = collect(watching, f, 300);
  REQUIRE(out.size() == 1);
  CHECK(out[0]["body"]["ai_paused"] == true);
}

// ---- System › Performance and System › Traces (PAGES §4.6.7)

namespace {

// Every frame a watcher of the topic gets over a session's facts, as text: what the privacy checks read.
std::vector<std::string> frames_of(std::string_view topic, const SpaceFacts& f) {
  Space s;
  s.watched(sub(topic));
  std::vector<std::string> frames;
  s.collect(f, 0, frames);
  return frames;
}

// Nothing of any text a call or a line had, no address, no path and nothing shaped like a SteamID64 (PV1 to PV8).
void check_no_text(const std::string& text) {
  for (const char* never : {"Old Bram", "Ysolde", "river", "lamps are lit", "openrouter", "talker", "router.example",
                            "\\", ":/", "127.0.0.1", "AppData", "Users", "7656119"}) {
    CHECK_MESSAGE(text.find(never) == std::string::npos, never);
  }
}

}  // namespace

TEST_CASE("space: System › Performance: its tiles, C3's limits beside the figures, its tasks, its storage in words") {
  const Space s;
  // The page data: live, in System after Status, its blocks on the topic's members; Related as PAGES gives them.
  const json pages = json::parse(space_pages());
  json page;
  for (const json& p : pages["pages"]) {
    if (p["id"] == "system.performance") page = p;
  }
  REQUIRE(page.is_object());
  CHECK(page["live"] == true);
  CHECK(page["increment"] == "0.2");
  CHECK(page["related"] == json::array({"system.status", "system.traces"}));
  std::vector<std::string> fields;
  for (const json& sec : page["sections"]) {
    for (const json& b : sec["blocks"]) {
      CHECK(b["topic"] == "system.performance");
      fields.push_back(b.contains("props") ? b["props"].value("field", "") : "");
    }
  }
  CHECK(fields == std::vector<std::string>{"", "budgets", "tasks", "storage"});

  // The title: the core's and the reader's p99, each within C3's limits, every kind of step, the log empty.
  json b = s.body("system.performance", facts_of("title"));
  CHECK(b["tiles"] == json::array({{{"label", "The core's steps, p99"}, {"value", "0.06 ms"}},
                                   {{"label", "Reading the game, p99"}, {"value", "0.13 ms"}}}));
  CHECK(b["budgets"]["checks"] ==
        json::array({{{"label", "Reading the game: the mean step"}, {"state", "pass"},
                      {"detail", "0.04 ms, within its 0.15 ms limit"}},
                     {{"label", "Reading the game: 99 steps in 100"}, {"state", "pass"},
                      {"detail", "0.13 ms, within its 0.50 ms limit"}}}));
  const json& tasks = b["tasks"];
  CHECK(tasks["columns"].size() == 5);
  REQUIRE(tasks["rows"].size() == 3);
  CHECK(tasks["rows"][0] == json{{"task", "Reading the game: each step"}, {"steps", "22,410"}, {"mean", "0.04 ms"},
                                 {"p99", "0.13 ms"}, {"longest", "0.91 ms"}});
  CHECK(tasks["rows"][1]["task"] == "Reading the game: each reading built");
  CHECK(tasks["rows"][1]["longest"] == "1.45 ms");
  CHECK(tasks["rows"][2] == json{{"task", "The core: each step"}, {"steps", "15,300"}, {"mean", "0.02 ms"},
                                 {"p99", "0.06 ms"}, {"longest", "2.90 ms"}});
  CHECK(b["storage"]["items"] ==
        json::array({{{"label", "Database"},
                      {"value", "None in this version: what is said stays in memory until the game closes"}},
                     {{"label", "Kept in memory"},
                      {"value", "0 entries from this session's conversations, 4,096 at most"}}}));

  // The world: the reader's p99 over its limit, a warning; the log's entries.
  b = s.body("system.performance", facts_of("world"));
  CHECK(b["budgets"]["checks"][0]["state"] == "pass");
  CHECK(b["budgets"]["checks"][0]["detail"] == "0.12 ms, within its 0.15 ms limit");
  CHECK(b["budgets"]["checks"][1]["state"] == "warn");
  CHECK(b["budgets"]["checks"][1]["detail"] == "0.64 ms, over its 0.50 ms limit");
  CHECK(b["storage"]["items"][1]["value"] == "37 entries from this session's conversations, 4,096 at most");
  b = s.body("system.performance", facts_of("multiplayer"));
  CHECK(b["storage"]["items"][1]["value"] ==
        "4,096 entries from this session's conversations, 4,096 at most; the 212 oldest gave way to newer ones");

  // Absence is silent: before the reader's first step no reader tile, budget or row; the core's alone.
  b = s.body("system.performance", facts_of("starting"));
  CHECK(b["tiles"] == json::array({{{"label", "The core's steps, p99"}, {"value", "0.05 ms"}}}));
  CHECK(b["budgets"]["checks"].empty());
  REQUIRE(b["tasks"]["rows"].size() == 1);
  CHECK(b["tasks"]["rows"][0]["task"] == "The core: each step");
  // Before the DLL fills any, nothing; and no figure of a frame's or of memory, which EldenNet has no source for.
  SpaceFacts none = facts_of("title");
  none.perf = nullptr;
  CHECK(s.body("system.performance", none) == json::object());
  for (const char* state : kStates) {
    const std::string text = s.body("system.performance", facts_of(state)).dump();
    for (const char* never : {"frame", "Frame", "memory use", "MB", "FPS"}) {
      CHECK_MESSAGE(text.find(never) == std::string::npos, never);
    }
  }
  // A long step reads in its unit: tenths of a millisecond from 10 ms, seconds from one.
  en::proto::PerfView slow;
  slow.core = en::proto::StepTimes{1, 25'000, 25'000, 1'500'000};
  none.perf = &slow;
  b = s.body("system.performance", none);
  CHECK(b["tasks"]["rows"][0]["p99"] == "25.0 ms");
  CHECK(b["tasks"]["rows"][0]["longest"] == "1.50 s");
  CHECK(b["storage"]["items"].size() == 1);  // the log not known: its row left out

  // Published when watched and as the figures move, no oftener than once a second; every frame clean.
  Space watching;
  watching.watched(sub("system.performance"));
  SpaceFacts f = facts_of("title");
  std::vector<json> out = collect(watching, f, 0);
  REQUIRE(out.size() == 1);
  CHECK(out[0]["topic"] == "system.performance");
  en::proto::PerfView moved = *f.perf;
  moved.core.steps += 40;
  f.perf = &moved;
  CHECK(collect(watching, f, 500).empty());
  CHECK(collect(watching, f, 1000).size() == 1);
  for (const char* state : kStates) {
    for (const std::string& text : frames_of("system.performance", facts_of(state))) check_no_text(text);
  }
}

TEST_CASE("space: System › Traces: tiles against C5's targets, the table in PAGES' columns, and no text at all") {
  const Space s;
  const json pages = json::parse(space_pages());
  json page;
  for (const json& p : pages["pages"]) {
    if (p["id"] == "system.traces") page = p;
  }
  REQUIRE(page.is_object());
  CHECK(page["live"] == true);
  CHECK(page["related"] == json::array({"ai.requests", "ai.selftest", "settings.developer"}));
  // Requests and Self-test name Traces among their Related now that it exists, as PAGES gives them.
  for (const json& p : pages["pages"]) {
    if (p["id"] == "ai.requests" || p["id"] == "ai.selftest") {
      CHECK(std::find(p["related"].begin(), p["related"].end(), "system.traces") != p["related"].end());
    }
  }

  // The world: the conversation's lines; 21 shown, so the 95th percentile too, over its 3.0 s target.
  const SpaceFacts f = facts_of("world");
  json b = s.body("traces", f);
  CHECK(b["tiles"] == json::array({{{"label", "First page, median"}, {"value", "1.24 s"}},
                                   {{"label", "First page, 95th percentile"}, {"value", "3.18 s"}},
                                   {{"label", "First token, median"}, {"value", "0.66 s"}},
                                   {{"label", "Lines this session"}, {"value", "21"}, {"unit", "of 24 shown"}}}));
  // One line in game (a computed sentence is drawn on one): the figures against C5's targets for a cloud model.
  CHECK(b["sentence"] == "Against the targets for a cloud model, the median first page is within 1.6 s and the 95th "
                         "percentile is over 3.0 s.");
  std::vector<std::string> ids;
  for (const json& c : b["columns"]) ids.push_back(c["id"].get<std::string>());
  CHECK(ids == std::vector<std::string>{"time", "kind", "slot", "queue", "first_token", "first_page", "total",
                                        "verdict"});
  const json& rows = b["rows"];
  REQUIRE(rows.size() == 7);
  CHECK(rows[0] == json{{"time", "1:05:05"}, {"kind", "reply"}, {"slot", "Conversation"}});  // still waiting
  CHECK(rows[1] == json{{"time", "1:00:55"}, {"kind", "reply"}, {"slot", "Conversation"}, {"queue", "0.04 s"},
                        {"first_token", "0.64 s"}, {"first_page", "1.18 s"}, {"total", "1.72 s"},
                        {"verdict", "quick"}});
  CHECK(rows[2]["verdict"] == "slow");
  CHECK(rows[3]["verdict"] == "quick");  // withheld: the persona's own line was its first page
  CHECK_FALSE(rows[4].contains("first_page"));  // timed out: no page, so no verdict
  CHECK_FALSE(rows[4].contains("verdict"));
  CHECK_FALSE(rows[4].contains("first_token"));
  CHECK(rows[4]["total"] == "6.00 s");

  // On a local model C5's local targets: the median within 2.5 s, the 95th within 4.5 s.
  AiFacts local = *f.ai;
  local.providers.push_back(lantern());
  local.slots[0].provider = "lantern-box";
  SpaceFacts on_local = f;
  on_local.ai = &local;
  CHECK(s.body("traces", on_local)["sentence"] == "Against the targets for a local model, the median first page is "
                                                  "within 2.5 s and the 95th percentile is within 4.5 s.");

  // Fewer than 20 lines: no 95th percentile, its tile and its clause left out; one quick line.
  en::proto::TracesView few;
  few.summary.calls = 1;
  few.summary.lines = 1;
  few.summary.first_page_median_ms = 980;
  few.summary.first_token_median_ms = 420;
  SpaceFacts one = f;
  one.traces = &few;
  b = s.body("traces", one);
  CHECK(b["tiles"] == json::array({{{"label", "First page, median"}, {"value", "0.98 s"}},
                                   {{"label", "First token, median"}, {"value", "0.42 s"}},
                                   {{"label", "Lines this session"}, {"value", "1"}}}));
  CHECK(b["sentence"] == "Against the target for a cloud model, the median first page is within 1.6 s.");
  CHECK(b["rows"].empty());

  // No line yet: the lines' tile alone, at 0; and the words for calls that ended with nothing shown.
  b = s.body("traces", facts_of("title"));
  CHECK(b["tiles"] == json::array({{{"label", "Lines this session"}, {"value", "0"}}}));
  CHECK(b["sentence"] == "No line has reached the screen yet this session.");
  en::proto::TracesView failed;
  failed.summary.calls = 1;
  failed.summary.unshown = 1;
  one.traces = &failed;
  b = s.body("traces", one);
  CHECK(b["sentence"] == "No line has reached the screen yet this session: 1 call for a line ended with no page "
                         "shown. AI › Requests says why.");
  CHECK(b["tiles"] == json::array({{{"label", "Lines this session"}, {"value", "0"}, {"unit", "of 1 shown"}}}));
  one.traces = nullptr;
  CHECK(s.body("traces", one) == json::object());

  // Published when watched; no frame in any state holds a line's text, a speaker, a provider, a model or a path.
  for (const char* state : kStates) {
    const std::vector<std::string> frames = frames_of("traces", facts_of(state));
    REQUIRE(frames.size() == 1);
    check_no_text(frames[0]);
  }
}

TEST_CASE("space: the fixture states' topics.json, which grace-dev previews (tests/pages)") {
#if defined(_WIN32)
  // The Windows builds run under Wine, outside the source tree's drive: the Linux builds compare the files.
  CHECK(true);
#else
  const bool update = std::getenv("EN_UPDATE_FIXTURES") != nullptr;
  for (const char* state : kStates) {
    Space s;
    feed(s, state);
    const SpaceFacts f = facts_of(state);
    json topics = json::object();
    for (const char* topic : {"status", "errors", "system.status", "system.performance", "traces"}) {
      topics[topic] = s.body(topic, f);
    }
    for (const char* topic : kAiTopics) topics[topic] = s.body(topic, f);  // calls and usage among them
    for (const std::string& topic : en::overlay::test::characters_topics(f.characters)) topics[topic] = s.body(topic, f);
    topics["content"] = s.body("content", f);
    // Docs (bundled): the list, the first document, which a page's dump shows, and About, its slots filled.
    topics["docs.list"] = s.body("docs.list", f);
    topics["docs/getting-started"] = s.body("docs/getting-started", f);
    topics["docs/about"] = s.body("docs/about", f);
    // Bug report: in the world, words of the player's own (invented) and the content left out.
    if (std::string_view(state) == "world") {
      (void)s.command(cmd({{"what", "The lantern keeper stopped mid-line, and the box stayed open.\nPressing Esc "
                                    "closed it."}},
                          "report.build"),
                      f);
      (void)s.command(cmd({{"id", "content"}, {"on", false}}, "report.part"), f);
    }
    topics["report"] = s.body("report", f);
    topics["logs"] = s.log_rows();
    const std::string text = topics.dump(2) + "\n";
    const std::string path = std::string(EN_PAGES_FIXTURES) + "/" + state + "/topics.json";
    if (update) {
      std::ofstream(path, std::ios::binary) << text;
    }
    std::ifstream in(path, std::ios::binary);
    std::stringstream have;
    have << in.rdbuf();
    INFO("tests/pages/" << state << "/topics.json is not what the space builds: EN_UPDATE_FIXTURES=1 writes it");
    CHECK(have.str() == text);
  }
#endif
}
