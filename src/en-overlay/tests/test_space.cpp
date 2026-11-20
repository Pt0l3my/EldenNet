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
  f.graceui = GRACEUI_VERSION_PACK(0, 2, 3);
  f.floor = {0, 2, 3};
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

// The log lines a state's session wrote, invented in the log's own vocabulary.
void feed(Space& s, std::string_view state) {
  s.log_line("boot pid=4242 dll=eldennet.dll probe_build=0 version_minor=1 version=dev", 3);
  s.log_line("gate result=" + std::string(state == "banner" ? "unknown_build" : "passed") + " ms=11", 11);
  s.log_line("init state=settings", 14);
  if (state == "banner") {
    s.log_line("settings_problem where=debug.log_level", 15);
    s.log_line("banner reason=unknown_build detail=unknown_build", 1210);
    return;
  }
  s.log_line("connect ok=1 outcome=connected path=main how=path looks=1 installed=1 graceui=0.2.3", 1830);
  if (state == "starting") return;
  s.log_line("init state=ready", 6120);
  s.log_line("change what=phase to=title", 6140);
  if (state == "world" || state == "multiplayer") {
    s.log_line("change what=phase to=in_world", 61200);
    s.log_line("tripped group=camera", 1200400);
  }
  if (state == "multiplayer") s.log_line("change what=badge to=up", 1300800);
}

const char* const kStates[] = {"starting", "title", "world", "multiplayer", "banner"};

SpaceFacts facts_of(std::string_view state) {
  if (state == "starting") return starting();
  if (state == "title") return title();
  if (state == "world") return world();
  if (state == "multiplayer") return multiplayer();
  return banner();
}

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
    CHECK(p["increment"] == std::string(kIncrement));  // every page of the slice ships in it (D173)
  }
  CHECK(ids == std::vector<std::string>{"home", "system.status", "system.logs", "settings.developer"});
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
      CHECK(s.body(id, title()).is_object());
      CHECK_FALSE(s.body(id, title()).empty());
    }
  }
  CHECK(pages["commands"] == json::array({"settings.set", "settings.reset"}));
}

TEST_CASE("space: settings start at their defaults; settings.set validated at its one entry point") {
  Space s;
  REQUIRE(s.valid());
  CHECK(s.values() == json{{"debug.panel", false}, {"debug.record_sessions", false}, {"debug.log_level", "info"}});
  s.set_value("debug.panel", true);  // as settings.toml had it at the start
  s.set_value("debug.unknown", 3);   // not one it describes: ignored
  CHECK(s.values().size() == 3);
  json body = s.settings_body();
  CHECK(body["values"]["debug.panel"] == true);
  CHECK(body["changed"] == json::array({"debug.panel"}));
  CHECK(body["description"].size() == 3);
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
  CHECK(error_of({{"key", "debug.panel"}, {"value", true}}, "selftest.run") == "unknown_command");
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
  CHECK(notes[6].topic == "selftest.run");
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
