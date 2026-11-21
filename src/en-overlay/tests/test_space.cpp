// EldenNet's space (space.hpp), in every build: its page data as generated, its topics from invented facts, its one
// command path (settings.set and settings.reset, each refusal named), what it publishes and when (nothing unwatched, a
// state when watched and when it changes, at its rate; the logs stream from its oldest row, in batches), its words
// (no lamp while starting, the banner's own words, absence silent) and, in the Linux builds, the fixture states'
// topics.json, which grace-dev previews (tests/pages; EN_UPDATE_FIXTURES=1 writes them anew).
#include <doctest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "en/overlay/space.hpp"
#include "graceui.h"

using namespace en::overlay;
using nlohmann::json;
namespace pw = en::proto::watch;

namespace {

// ---- Invented facts: the fixture states (PAGES §5), with our own numbers and no game text.

SpaceFacts base() {
  SpaceFacts f;
  f.version = {0, 1, 0};
  f.graceui = GRACEUI_VERSION_PACK(0, 2, 8);  // the GraceUI vendored (third_party/graceui-sys)
  f.floor = {0, 2, 8};
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
  return f;
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

// A first run: no providers.toml yet, so nothing set up.
AiFacts ai_first_run() {
  AiFacts ai;
  ai.files = AiFiles::Missing;
  ai.presets = {"cloud-budget", "cloud-nanogpt"};
  ai.preset_missing = {"openrouter"};
  return ai;
}

// What the fixture states point at (the facts hold a pointer).
const AiFacts& ai_fixture(std::string_view state) {
  static const AiFacts ready = ai_ready();
  static const AiFacts first = ai_first_run();
  static const AiFacts resting = [] {
    AiFacts ai = ai_ready();
    ai.providers[0].cooling_s = 42;
    ai.providers[0].pairs_cooling = 1;
    ai.slots[2].healthy = false;
    return ai;
  }();
  if (state == "title") return ready;
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
// The log lines a state's session wrote, invented in the log's own vocabulary (the fixtures', compared in the Linux
// builds alone).
void feed(Space& s, std::string_view state) {
  s.log_line("boot pid=4242 dll=eldennet.dll probe_build=0 version_minor=1 version=dev", 3);
  s.log_line("gate result=" + std::string(state == "banner" ? "unknown_build" : "passed") + " ms=11", 11);
  s.log_line("init state=settings", 14);
  if (state == "banner") {
    s.log_line("settings_problem where=debug.log_level", 15);
    s.log_line("banner reason=unknown_build detail=unknown_build", 1210);
    return;
  }
  s.log_line("connect ok=1 outcome=connected path=main how=path looks=1 installed=1 graceui=0.2.8", 1830);
  if (state == "starting") return;
  s.log_line("init state=ready", 6120);
  s.log_line("change what=phase to=title", 6140);
  if (state == "world" || state == "multiplayer") {
    s.log_line("change what=phase to=in_world", 61200);
    s.log_line("tripped group=camera", 1200400);
  }
  if (state == "multiplayer") s.log_line("change what=badge to=up", 1300800);
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
  return f;
}

const char* const kAiTopics[] = {"ai.status",           "providers",           "health",
                                 "ai.slots",            "selftest",            "space.state",
                                 "ai.choices.dialogue", "ai.choices.bark_live", "ai.choices.bark_pool",
                                 "ai.choices.memory",   "ai.choices.selftest"};

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
  CHECK(ids == std::vector<std::string>{"home", "system.status", "system.logs", "settings.developer", "ai.providers",
                                        "ai.models", "ai.selftest"});
  // The rail holds PAGES §4.2.1's categories in their order, the slice's pages in theirs.
  std::vector<std::string> rail;
  for (const json& c : pages["categories"]) rail.push_back(c["id"].get<std::string>());
  CHECK(rail == std::vector<std::string>{"home", "ai", "voice", "characters", "map", "history", "behaviour", "system",
                                         "tools", "settings"});
  const Space s;
  REQUIRE(s.valid());
  for (const json& t : pages["topics"]) {
    const std::string id = t["id"].get<std::string>();
    if (t["shape"] == "stream") {
      CHECK(id == "logs");
    } else {
      CHECK(s.body(id, facts_of("title")).is_object());
      CHECK_FALSE(s.body(id, facts_of("title")).empty());
    }
  }
  CHECK(pages["commands"] == json::array({"settings.set", "settings.reset", "selftest.run", "models.list",
                                          "providers.add", "providers.remove", "providers.key"}));
  CHECK(pages["minor"] == 2);  // row actions, a row secret, a form and pickers: protocol 1.2's (GraceUI D196)
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
          CHECK(b["command"] == "providers.add");
        }
      }
    }
  }
  CHECK(row_actions == 1);
  CHECK(forms == 1);
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
  CHECK(s.values() == json{{"debug.panel", false},
                           {"debug.record_sessions", false},
                           {"debug.log_level", "info"},
                           {"ai.preset", "cloud-budget"},
                           {"slots.dialogue.model", ""},
                           {"slots.bark_live.model", ""},
                           {"slots.bark_pool.model", ""},
                           {"slots.memory.model", ""},
                           {"slots.selftest.model", ""}});
  s.set_value("debug.panel", true);  // as settings.toml had it at the start
  s.set_value("debug.unknown", 3);   // not one it describes: ignored
  CHECK(s.values().size() == 9);
  json body = s.settings_body();
  CHECK(body["values"]["debug.panel"] == true);
  CHECK(body["changed"] == json::array({"debug.panel"}));
  CHECK(body["description"].size() == 9);
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
  for (int i = 0; i < 520; ++i) s.log_line("line n=" + std::to_string(i), 1000u * i);
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
  CHECK(out[4]["from_seq"] == 421);
  CHECK(out[4]["dropped"] == 0);
  CHECK(out[4]["rows"].back()["seq"] == 520);
  s.log_line("line n=520", 700'000);
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
  for (const json& i : s.body("status", bare)["items"]) {
    CHECK(i["label"] != "GraceUI");
    CHECK(i["label"] != "Game build");
  }
  CHECK(s.body("system.status", bare)["rows"].empty());
  for (const json& c : s.body("system.status", title())["checks"]) CHECK(c["label"] != "Recording");
  CHECK(s.body("errors", title())["checks"].empty());
  CHECK(s.body("system.status", world())["rows"].size() == en::proto::kGroupCount);

  for (const char* state : kStates) {
    for (const char* topic : {"status", "errors", "system.status", "system.compatibility"}) {
      CHECK(clean(s.body(topic, facts_of(state))));
    }
  }
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
  REQUIRE(providers["rows"].size() == 1);
  const json& row = providers["rows"][0];
  CHECK(row["id"] == "openrouter");
  CHECK(row["kind"] == "Cloud");
  CHECK(row["type"] == "OpenAI-compatible");
  CHECK(row["address"] == "https://router.example.invalid/api/v1");  // on this page alone (PV2)
  CHECK(row["key"] == "set");  // the row's secret (GraceUI D194): its status alone
  CHECK(row["requests"] == "2");
  CHECK(row["json"] == "JSON schema");
  REQUIRE(providers["checks"].size() == 1);
  CHECK(providers["checks"][0]["state"] == "pass");
  CHECK(providers["checks"][0]["detail"] == "Lists 214 models, every job's model among them.");
  for (const char* topic : {"ai.status", "health", "ai.slots", "selftest", "ai.choices.dialogue", "ai.choices.memory"}) {
    CHECK_FALSE(holds(s.body(topic, f), "example.invalid"));
    CHECK_FALSE(holds(s.body(topic, f), "://"));
  }
  // A job's picker: what its provider listed, in its order, with nothing shaped like a path (PV5); a job that borrows
  // another's models picks from the same provider's list.
  const json choices = s.body("ai.choices.memory", f)["choices"];
  REQUIRE(choices.size() == 5);
  CHECK(choices[0] == json{{"value", "talker-small"}, {"label", "talker-small"}});
  CHECK(choices[4]["value"] == "keeper-small");
  CHECK_FALSE(holds(choices, "Users"));
  CHECK(s.body("ai.choices.selftest", f)["choices"].size() == 5);
  CHECK(s.body("ai.choices.dialogue", facts_of("starting")) == json{{"choices", json::array()}});  // nothing listed
  CHECK(s.body("ai.choices.dialogue", title()) == json{{"choices", json::array()}});
  CHECK(s.body("ai.status", f)["sentence"].get<std::string>().find("only observes in this version") !=
        std::string::npos);
  const json slots = s.body("ai.slots", f);
  REQUIRE(slots["rows"].size() == 5);
  CHECK(slots["rows"][0]["job"] == "Conversation");
  CHECK(slots["rows"][0]["model"] == "talker-small");
  CHECK(slots["rows"][0]["fallback"] == "talker-large");
  CHECK(slots["rows"][4]["job"] == "Self-test");
  CHECK(slots["rows"][4]["model"] == "Same as Conversation");
  const json health = s.body("health", f);
  CHECK(health["rows"][0]["state"] == "Healthy");
  CHECK(health["rows"][0]["answered"] == "0:11:40");
  CHECK(health["rows"][0]["jobs"] == "Conversation, Quick lines, Prepared lines, Memory, Self-test");
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
  CHECK(greyed.size() == 3);
  CHECK(greyed["ai.selftest"] == "EldenNet inactive: game build 3.14.15.0 not supported yet");
  CHECK(greyed.contains("ai.providers"));
  CHECK_FALSE(greyed.contains("home"));
  CHECK_FALSE(greyed.contains("system.status"));
  SpaceFacts long_banner = facts_of("banner");
  const std::string words(300, 'w');
  long_banner.stop_text = words;
  CHECK(s.body("space.state", long_banner)["greyed"]["ai.models"].get<std::string>().size() == 200);

  // Resting models, the gate closed online, a first run, the banner.
  CHECK(s.body("health", facts_of("world"))["rows"][0]["state"] == "Cooling down, 42 s left");
  CHECK(s.body("ai.slots", facts_of("world"))["rows"][2]["state"] == "Resting after failures");
  CHECK(s.body("ai.status", facts_of("multiplayer"))["sentence"] ==
        "Model calls wait while you play with others online.");
  CHECK(s.body("ai.status", facts_of("starting"))["sentence"].get<std::string>().rfind("No provider yet", 0) == 0);
  // At L1 the lead takes the banner's own words, the greyed page's one form (O95).
  CHECK(s.body("ai.status", facts_of("banner"))["sentence"] ==
        "EldenNet inactive: game build 3.14.15.0 not supported yet");
  // Absence is silent: no table rows for providers there are not, no self-test before one runs, nothing without facts.
  CHECK(s.body("providers", facts_of("starting"))["rows"].empty());
  CHECK(s.body("selftest", facts_of("starting")).empty());
  for (const char* topic : {"ai.status", "providers", "health", "ai.slots", "selftest"}) {
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
  CHECK(s.body("providers", m)["rows"][0]["key"] == "set");  // kept, though refused: Health and the lead say so
  CHECK(s.body("ai.status", m)["sentence"] == "openrouter refused its key: paste it again on its row on Providers.");
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
  CHECK(taken[0].loopback);
  r = add("far.cloud", "https://models.example.invalid/api/v1");
  CHECK(r["ok"] == true);
  taken = s.take_requests();
  REQUIRE(taken.size() == 1);
  CHECK_FALSE(taken[0].loopback);

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
  CHECK(refusal("box", "http://[fd00::20]:1234/v1") ==
        "Only a server on this PC may use http://: give this server's https:// address.");
  CHECK(refusal("box", "http://127.example.invalid/v1").rfind("Only a server on this PC", 0) == 0);
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
  // A provider with no key's place yet (a server of one's own on this PC) takes one; clearing none is no request.
  ai.providers[0].key_place = AiKeyPlace::None;
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
  CHECK(frames.size() == 12);
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
  CHECK(set("slots.memory.model", "")["error"] == "invalid_value");
  CHECK(set("slots.memory.model", 4)["error"] == "invalid_value");
  // Before a list has answered, not now.
  ai.models.clear();
  CHECK(set("slots.dialogue.model", "talker-large")["detail"].get<std::string>().rfind("Press List models", 0) == 0);
  ai = ai_ready();
  ai.slots[1].provider.clear();
  CHECK(set("slots.bark_live.model", "talker-large")["detail"].get<std::string>().rfind("Quick lines has no provider", 0) ==
        0);
  CHECK(s.take_changes().empty());
  // A reset: the preset's model again (the DLL clears the job's [slots.*]).
  ai = ai_ready();
  CHECK(frame(s.command(cmd({{"scope", "key"}, {"target", "slots.memory.model"}}, "settings.reset"), f))["ok"] == true);
  changes = s.take_changes();
  REQUIRE(changes.size() == 1);
  CHECK(changes[0].value == "");
  // Once the DLL takes the edited configuration (its revision), the rows follow it again.
  ai.revision = 4;
  ai.slots[3].model = "keeper-large";
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

TEST_CASE("space: the log's rows are scrubbed of a SteamID64's shape; the clock") {
  CHECK(scrubbed("steam id=76561197960287930 n=12") == "steam id=redacted n=12");
  CHECK(scrubbed("n=765611979602879301") == "n=765611979602879301");  // 18 digits: not the shape
  CHECK(scrubbed("frames=12345678901234567") == "frames=12345678901234567");
  CHECK(clock_of(0) == "0:00:00");
  CHECK(clock_of(3'723'000) == "1:02:03");
  Space s;
  s.log_line("x id=76561197960287930", 5);
  CHECK(s.log_rows()[0]["line"] == "x id=redacted");
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
    for (const char* topic : {"status", "errors", "system.status", "system.compatibility"}) {
      topics[topic] = s.body(topic, f);
    }
    for (const char* topic : kAiTopics) topics[topic] = s.body(topic, f);
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
