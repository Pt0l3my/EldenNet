// Tools › Workshop (workshop.hpp; PAGES §4.6.8, SP8; GraceUI D258), in every build: the page data's four fields and
// its place, its topics' bodies from invented facts, nothing published while Developer views are off, `{}` and the
// events stream cleared the moment they go off, recording.set at its one command path, the prompt's paths and
// addresses scrubbed, and every body within GraceUI's frame. Every word and number here is invented.
#include <doctest.h>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "en/overlay/space.hpp"
#include "en/overlay/workshop.hpp"
#include "workshop_fixture.hpp"

using namespace en::overlay;
using nlohmann::json;
namespace pr = en::proto;

namespace {

std::string cmd(const json& args, std::string_view name, int id = 7) {
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

const json* page_of(const json& pages, std::string_view id) {
  for (const json& p : pages["pages"]) {
    if (p["id"].get<std::string>() == id) return &p;
  }
  return nullptr;
}

const char* const kStateTopics[] = {"workshop.exchange", "workshop.world", "workshop.known", "workshop.unknown",
                                    "workshop.status", "workshop.kinds"};

SpaceFacts on_facts(const pr::WorkshopView& v) {
  SpaceFacts f;
  f.workshop = &v;
  return f;
}

}  // namespace

TEST_CASE("workshop: the page data: Tools › Workshop in the browser alone, shown with Developer views, gated") {
  const json pages = json::parse(space_pages());
  const json* w = page_of(pages, "tools.workshop");
  REQUIRE(w != nullptr);
  CHECK((*w)["category"] == "tools");
  CHECK((*w)["increment"] == "0.2");
  CHECK((*w)["browser_only"] == true);
  CHECK((*w)["shown_with"] == "debug.panel");
  CHECK((*w)["live"] == true);
  // The owner's words (O129 (a)).
  CHECK((*w)["gate"]["words"] ==
        "Workshop shows EldenNet's raw workings: the text sent to the model and its replies, every event, and what "
        "each character knows. It contains spoilers. Open it for this session?");
  CHECK((*w)["gate"]["confirm"] == "Open Workshop");
  CHECK((*w)["ribbon"] == "Spoilers");
  CHECK((*w)["gate"]["words"].get<std::string>().size() <= 256);
  // Tools holds Bug report then Workshop (PAGES §4.2.1).
  for (const json& c : pages["categories"]) {
    if (c["id"] == "tools") CHECK(c["pages"] == json::array({"tools.report", "tools.workshop"}));
  }
  // Its header switch: Record this session, recording.set on workshop.status.recording_on (GraceUI D256), sensitive.
  REQUIRE((*w)["actions"].size() == 1);
  CHECK((*w)["actions"][0]["command"] == "recording.set");
  CHECK((*w)["actions"][0]["label"] == "Record this session");
  CHECK((*w)["actions"][0]["state"] == json{{"topic", "workshop.status"}, {"field", "recording_on"}});
  const json& sensitive = pages["sensitive_commands"];
  CHECK(std::find(sensitive.begin(), sensitive.end(), "recording.set") != sensitive.end());
  // Every workshop.* topic is drawn on Workshop alone, and no setting lives there (a shortcut is not a home), so that
  // GraceUI 0.2.57's pc_only marks are one line each when the pin takes them (D264).
  for (const json& p : pages["pages"]) {
    const std::string text = p.dump();
    if (p["id"] != "tools.workshop") CHECK(text.find("\"workshop.") == std::string::npos);
  }
  const json settings = json::parse(settings_description());
  for (const json& s : settings["settings"]) CHECK(s["home"] != "tools.workshop");
  std::vector<std::string> topics;
  for (const json& t : pages["topics"]) {
    const std::string id = t["id"].get<std::string>();
    if (id.rfind("workshop.", 0) == 0) topics.push_back(id);
    if (id == "workshop.events") {
      CHECK(t["shape"] == "stream");
      CHECK(t["ring"] == pr::kWorkshopEventRing);
    }
  }
  CHECK(topics.size() == 7);
  for (const std::string& t : topics) CHECK(Workshop::owns_topic(t));
}

TEST_CASE("workshop: while Developer views are off, nothing is published, kept or taken") {
  Space s;
  REQUIRE(s.valid());
  const pr::WorkshopView v = test::workshop_view();
  const SpaceFacts f = on_facts(v);
  CHECK_FALSE(s.workshop_on());
  for (const char* t : kStateTopics) {
    CHECK(s.body(t, f).empty());
    s.watched(sub(t));
  }
  s.watched(sub("workshop.events"));
  s.workshop_event(test::workshop_event_of(1, "Struck", "K", "snapshot_diff", "{}"));
  CHECK(s.workshop_rows().empty());
  for (const json& fr : collect(s, f, 1000)) {
    CHECK(fr["topic"].get<std::string>().rfind("workshop.", 0) != 0);
  }
  const json r = json::parse(s.command(cmd({{"on", true}}, "recording.set"), f));
  CHECK(r["error"] == "not_now");
  CHECK(r["detail"] == "Turn on Developer views in Settings › Developer first.");
  CHECK(s.take_recording_requests().empty());
}

TEST_CASE("workshop: on, each topic from the facts; the moment it goes off, {} and the stream cleared, then silence") {
  Space s;
  const pr::WorkshopView v = test::workshop_view();
  const SpaceFacts f = on_facts(v);
  s.set_value("debug.panel", true);
  REQUIRE(s.workshop_on());
  for (const char* t : kStateTopics) s.watched(sub(t));
  s.watched(sub("workshop.events"));
  for (int i = 0; i < 3; ++i) {
    s.workshop_event(test::workshop_event_of(1000 + i, "Struck", "K", "snapshot_diff", R"({"kind":"Struck"})"));
  }
  std::vector<json> frames = collect(s, f, 1000);
  std::vector<std::string> states;
  json rows;
  for (const json& fr : frames) {
    if (fr["t"] == "state") states.push_back(fr["topic"].get<std::string>());
    if (fr["t"] == "rows" && fr["topic"] == "workshop.events") rows = fr;
  }
  CHECK(states.size() == 6);
  REQUIRE(rows.is_object());
  CHECK(rows["from_seq"] == 1);
  CHECK(rows["rows"].size() == 3);
  CHECK(rows["rows"][0] == json{{"seq", 1}, {"time", "0:00:01"}, {"line", R"({"kind":"Struck"})"}, {"kind", "Struck"},
                                {"class", "K"}, {"source", "snapshot_diff"}});
  // Nothing again until something changes.
  CHECK(collect(s, f, 1100).empty());

  // Developer views off (settings.set, as the browser's switch sends it): {} on each watched state topic, the stream
  // cleared by an empty batch past a gap, the ring emptied; then nothing, whatever the facts hold.
  const json set = json::parse(s.command(cmd({{"key", "debug.panel"}, {"value", false}}, "settings.set"), f));
  CHECK(set["ok"] == true);
  CHECK_FALSE(s.workshop_on());
  frames = collect(s, f, 2000);
  std::size_t cleared = 0;
  bool stream_cleared = false;
  for (const json& fr : frames) {
    if (fr["t"] == "state" && fr["topic"].get<std::string>().rfind("workshop.", 0) == 0) {
      CHECK(fr["body"] == json::object());
      ++cleared;
    }
    if (fr["t"] == "rows" && fr["topic"] == "workshop.events") {
      CHECK(fr["rows"].empty());
      CHECK(fr["from_seq"].get<std::uint64_t>() > 4);  // past the next row a view expects (4): a gap
      stream_cleared = true;
    }
  }
  CHECK(cleared == 6);
  CHECK(stream_cleared);
  CHECK(s.workshop_rows().empty());
  for (const json& fr : collect(s, f, 3000)) CHECK(fr["topic"].get<std::string>().rfind("workshop.", 0) != 0);

  // On again: the bodies come back, and the stream's rows go on past the gap.
  s.set_value("debug.panel", true);
  s.workshop_event(test::workshop_event_of(5000, "LowHp", "K", "snapshot_diff", "{}"));
  frames = collect(s, f, 5000);
  std::size_t again = 0;
  for (const json& fr : frames) {
    if (fr["t"] == "state" && fr["topic"].get<std::string>().rfind("workshop.", 0) == 0) ++again;
    if (fr["t"] == "rows" && fr["topic"] == "workshop.events") CHECK(fr["rows"].size() == 1);
  }
  CHECK(again == 6);
}

TEST_CASE("workshop: the events stream's ring of 500, in batches of 100, from the oldest held for a new watcher") {
  Space s;
  const pr::WorkshopView v;
  const SpaceFacts f = on_facts(v);
  s.set_value("debug.panel", true);
  for (int i = 0; i < 620; ++i) s.workshop_event(test::workshop_event_of(i, "Struck", "K", "snapshot_diff", "{}"));
  CHECK(s.workshop_rows().size() == pr::kWorkshopEventRing);
  CHECK(s.workshop_rows()[0]["seq"] == 121);
  s.watched(sub("workshop.events"));
  std::size_t batches = 0;
  std::size_t total = 0;
  for (const json& fr : collect(s, f, 1000)) {
    if (fr["t"] != "rows") continue;
    if (batches == 0) {
      CHECK(fr["from_seq"] == 121);
      CHECK(fr["dropped"] == 120);
    }
    ++batches;
    total += fr["rows"].size();
  }
  CHECK(batches == 5);
  CHECK(total == 500);
}

TEST_CASE("workshop: the bodies, in the facts' own words and numbers; the prompt's paths and addresses scrubbed") {
  Space s;
  pr::WorkshopView v = test::workshop_view();
  v.exchange.messages.push_back(
      {"user", "Look in C:\\Users\\someone\\notes and at 203.0.113.20:8080, id 76561197960265728.", false});
  const SpaceFacts f = on_facts(v);
  s.set_value("debug.panel", true);

  const json x = s.body("workshop.exchange", f);
  std::map<std::string, std::string> items;
  for (const json& i : x["facts"]["items"]) items[i["label"].get<std::string>()] = i["value"].get<std::string>();
  CHECK(items["Job"] == "dialogue");
  CHECK(items["Purpose"] == "reply");
  CHECK(items["Provider"] == "lantern-box");
  CHECK(items["Model"] == "invented-lantern-a");
  CHECK(items["Answered by"] == "invented-lantern-a-0425");
  CHECK(items["Outcome"] == "Answered");
  CHECK(items["Tokens"] == "812 in, 9 out, 640 cached");
  CHECK(items["First words"] == "0.14 s");
  CHECK(items["Asked for"] == "at most 140 tokens; temperature 0.8; 2 stop sequences");
  const json& parts = x["parts"]["parts"];
  REQUIRE(parts.size() == 4);
  CHECK(parts[0]["label"] == "1. System · a cache mark after it");
  CHECK(parts[0]["fields"][0]["code"] == true);
  CHECK(parts[0]["optional"] == false);
  const std::string scrubbed = parts[2]["fields"][0]["value"].get<std::string>();
  CHECK(scrubbed.find("Users") == std::string::npos);
  CHECK(scrubbed.find("203.0.113") == std::string::npos);
  CHECK(scrubbed.find("76561197960265728") == std::string::npos);
  CHECK(scrubbed.find("Look in") != std::string::npos);
  CHECK(parts[3]["id"] == "reply");
  CHECK(parts[3]["fields"][0]["value"] == "Every one of them, since dusk.");
  CHECK(parts[3]["fields"][1]["label"] == "As the model wrote it");

  const json w = s.body("workshop.world", f);
  CHECK(w["flags"]["rows"].size() == 2);
  CHECK(w["flags"]["rows"][0]["reads"] == "T");
  CHECK(w["flags"]["rows"][1]["block"] == "keeper + 3");
  CHECK(w["stages"]["rows"][1]["value"] == "F");
  CHECK(w["tracks"]["rows"][1]["step"] == "none");
  CHECK(w["frame"]["items"][0]["value"] == "Read");

  const json k = s.body("workshop.known", f);
  CHECK(k["rows"].size() == 2);
  CHECK(k["rows"][0]["block5b"] == "shown");
  CHECK(k["rows"][1]["own"] == "no");

  const json u = s.body("workshop.unknown", f);
  CHECK(u["rows"]["rows"].size() == 2);
  CHECK(u["rows"]["rows"][0]["nearest"] == "6.5 m");
  CHECK(u["toml"]["parts"][1]["label"] == "NpcParam 7700, entity 1043300800");
  CHECK(u["toml"]["parts"][1]["fields"][0]["label"] == "TOML");
  CHECK(u["toml"]["parts"][1]["fields"][0]["code"] == true);

  const json st = s.body("workshop.status", f);
  CHECK(st["recording_on"] == true);
  CHECK(st["recording"]["items"][0]["value"] == "On: Record sessions was on at the start");
  for (const json& i : st["recording"]["items"]) CHECK(i["value"].get<std::string>().find('\\') == std::string::npos);

  const json kinds = s.body("workshop.kinds", f);
  CHECK(kinds["kinds"]["rows"].size() == 2);
  CHECK(kinds["kinds"]["rows"][1].contains("built") == false);
  CHECK(kinds["rules"]["rows"][0]["where"] == "combat.toml:12");

  // Nothing yet since Developer views came on: the exchange and the unknown characters have no body.
  const pr::WorkshopView none;
  CHECK(s.body("workshop.exchange", on_facts(none)).empty());
  CHECK(s.body("workshop.unknown", on_facts(none)).empty());
}

TEST_CASE("workshop: recording.set at the one command path, handed to the DLL; its note names the command alone") {
  Space s;
  const pr::WorkshopView v;
  const SpaceFacts f = on_facts(v);
  s.set_value("debug.panel", true);
  CHECK(json::parse(s.command(cmd({{"on", "yes"}}, "recording.set"), f))["error"] == "invalid_value");
  CHECK(json::parse(s.command(cmd(json::array(), "recording.set"), f))["error"] == "invalid_value");
  CHECK(json::parse(s.command(cmd({{"on", true}}, "recording.set"), f))["ok"] == true);
  CHECK(json::parse(s.command(cmd({{"on", false}}, "recording.set"), f))["ok"] == true);
  CHECK(s.take_recording_requests() == std::vector<RecordingRequest>{{true}, {false}});
  CHECK(s.take_recording_requests().empty());
  bool noted = false;
  for (const SpaceNote& n : s.take_notes()) {
    if (n.what == SpaceNote::What::Command && n.topic == "recording.set" && n.result == "ok") noted = true;
  }
  CHECK(noted);
}

TEST_CASE("workshop: every body stays within GraceUI's frame to a page, whatever the facts hold at their bounds") {
  Space s;
  s.set_value("debug.panel", true);
  pr::WorkshopView v = test::workshop_view();
  v.exchange.messages.clear();
  // A prompt at its bound, of characters JSON escapes (a quote, a backslash and a line break), and the reply's two.
  std::string prompt;
  while (prompt.size() < pr::kWorkshopMessagesMax) prompt += "\"q\"\\\n";
  prompt.resize(pr::kWorkshopMessagesMax);
  v.exchange.messages.push_back({"user", prompt, false});
  v.exchange.text = std::string(pr::kWorkshopReplyMax, '"');
  v.exchange.raw = std::string(pr::kWorkshopReplyMax, '\n');
  v.world.flags.assign(pr::kWorkshopRowsMax, pr::WorkshopFlag{"an.alias.of.thirty.two.characters", 4294967295u,
                                                               "a.block", 19, true, true, true, true, true, "U",
                                                               "outcome", "unknown", 3});
  v.known.rows.assign(pr::kWorkshopRowsMax, pr::WorkshopKnown{"a_persona_id_of_32_characters___", "a_stage_id",
                                                              "q:a_track.a_step_of_its_own", "rumour", "word", true,
                                                              "offered"});
  const SpaceFacts f = on_facts(v);
  for (const char* t : kStateTopics) {
    const std::string text = s.body(t, f).dump();
    INFO(t);
    CHECK(text.size() <= pr::kWorkshopBodyMax);
  }
  // Rows left out are counted, never silently dropped.
  const json w = s.body("workshop.world", f);
  CHECK(w["flags"]["rows"].size() < pr::kWorkshopRowsMax);
  bool counted = false;
  for (const json& i : w["frame"]["items"]) {
    if (i["label"] == "Flags left out") counted = true;
  }
  CHECK(counted);
  const json k = s.body("workshop.known", f);
  CHECK(k["rows"].size() < pr::kWorkshopRowsMax);
  CHECK(k["left_out"].get<std::size_t>() == pr::kWorkshopRowsMax - k["rows"].size());
}
