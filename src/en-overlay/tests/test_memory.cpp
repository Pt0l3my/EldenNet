// The memory pages (memory.hpp; PAGES §4.6.1, §4.6.4, §4.6.5; SP1 to SP9): the Journey's choices, each page's bodies by
// journey, the keys, what is never shown, the transcripts asked for, and the forgets through the one command path.
// All data is invented (memory_fixture.hpp).
#include <doctest.h>

#include <set>
#include <string>

#include "en/overlay/memory.hpp"
#include "en/overlay/space.hpp"
#include "memory_fixture.hpp"

using namespace en::overlay;
using nlohmann::json;

namespace {

const MemoryFacts* world() { return test::memory_fixture("world"); }

}  // namespace

TEST_CASE("memory pages: the Journey lists every journey, the earlier read only, the current one first in view") {
  const json j = MemoryPages::body(kJourneysTopic, world());
  REQUIRE(j["choices"].size() == 2);
  CHECK(j["choices"][0] == json{{"id", "1"}, {"label", "Journey 1"}, {"read_only", true}});
  CHECK(j["choices"][1] == json{{"id", "2"}, {"label", "Journey 2"}});
  CHECK(j["current"] == "2");
  // Before a playthrough: no choice, and so no chooser (GraceUI D254).
  CHECK(MemoryPages::body(kJourneysTopic, nullptr) == json{{"choices", json::array()}});
}

TEST_CASE("memory pages: Playthrough lists the people met on the journey in view, and nobody met only on another") {
  const json second = MemoryPages::body("playthrough/2", world());
  REQUIRE(second["rows"].size() == 1);
  CHECK(second["rows"][0]["id"] == "2.lantern_keeper");
  CHECK(second["rows"][0]["status"] == "Here now");
  CHECK(second["rows"][0]["last_met"] == "0:12");
  const json first = MemoryPages::body("playthrough/1", world());
  REQUIRE(first["rows"].size() == 2);
  CHECK(first["rows"][0]["name"] == "Merrit of the Ferry");
  CHECK(first["rows"][1]["status"] == "Alive when last met");  // never from a flag: perceived alone (SP7)
  CHECK(MemoryPages::body("playthrough/3", world()).empty());  // no such journey: nothing
  CHECK(MemoryPages::body("playthrough/2", nullptr).empty());
  CHECK(MemoryPages::body("met/2.ferry_warden", world()).empty());  // not met on journey 2: absent (SP4)
  const json met = MemoryPages::body("met/1.lantern_keeper", world());
  bool fact = false;
  for (const json& i : met["items"]) {
    if (i["label"] == "Seen you pass by") fact = i["value"] == "2 times";
    CHECK(i["label"] != "Stage");
  }
  CHECK(fact);
}

TEST_CASE("memory pages: Conversations by journey, keys as strings of digits, and the transcript as it was shown") {
  const json list = MemoryPages::body("conversations/2", world());
  REQUIRE(list["rows"].size() == 1);
  CHECK(list["rows"][0]["id"] == "12");
  CHECK(list["rows"][0]["ended"] == "Still open");
  CHECK(MemoryPages::body("conversations/1", world())["rows"][0]["ended"] == "You travelled");
  const json t = MemoryPages::body("conversation/12", world());
  REQUIRE(t["rows"].size() == 2);
  CHECK(t["rows"][0]["who"] == "You");
  CHECK(t["rows"][1]["line"] == "It keeps me as I keep it.");
  CHECK(MemoryPages::body("conversation/7", world()).empty());  // not read yet: absence is silent
  CHECK(MemoryPages::body("conversation/x7", world()).empty());
  // A watched transcript the facts lack is asked of the DLL once.
  MemoryPages pages;
  pages.want({"conversation/7", "conversation/12", "conversations/1"}, world());
  pages.want({"conversation/7"}, world());
  const auto asked = pages.take_requests();
  REQUIRE(asked.size() == 1);
  CHECK(asked[0].kind == MemoryRequest::Kind::Transcript);
  CHECK(asked[0].conv == 7);
}

TEST_CASE("memory pages: Memories' card, keepsakes and work, from MEM 2.2's rendered facts alone") {
  const json list = MemoryPages::body("memories/1", world());
  REQUIRE(list["rows"].size() == 2);
  CHECK(list["rows"][1]["keepsakes"] == "1");
  const json j = MemoryPages::body("journal/1.lantern_keeper", world());
  CHECK(j["card"]["items"][0] == json{{"label", "Their journal"},
                             {"value", "A traveller who asks after the lamp, and listens to the answer."}});
  CHECK(j["keepsakes"]["rows"][0]["kept"] == "Kept");
  CHECK(j["keepsakes"]["rows"][0]["weight"] == "0.8");
  CHECK(j["keepsakes"]["rows"][0]["basis"] == "Seen");
  CHECK(j["forget"]["rows"][0]["id"] == "1.lantern_keeper");
  const std::string text = j.dump();
  for (const char* never : {"stage_seen", "world_seen", "spoke_pt", "verified:", "world_cued"}) {
    CHECK(text.find(never) == std::string::npos);
  }
  CHECK(MemoryPages::body("journal/2.lantern_keeper", world())["work"]["items"][0]["value"] == "One waiting");
  // Backups by count, no path; and what a backup keeps.
  const json st = MemoryPages::body(kMemoryStateTopic, world());
  CHECK(st["items"][0] == json{{"label", "Backups"}, {"value", "3"}});
  CHECK(st["items"][1] == json{{"label", "Newest"}, {"value", "2025-11-27 00:00 UTC"}});
  CHECK(st.dump().find("three newer backups") != std::string::npos);
  CHECK(st.dump().find(".sqlite") == std::string::npos);
}

TEST_CASE("memory pages: Nearby shows the met NPCs perceived now, with no number") {
  const json n = MemoryPages::body(kNearbyTopic, world());
  REQUIRE(n["rows"].size() == 1);
  CHECK(n["rows"][0]["name"] == "Oswin the Lamplighter");
  CHECK(n["rows"][0]["state"] == "Here");
  CHECK_FALSE(n["rows"][0].contains("disposition"));  // no journal on this journey yet
  CHECK(MemoryPages::body(kNearbyTopic, nullptr)["rows"].empty());
}

TEST_CASE("memory pages: the forgets, confirmed, on the open journey only, through the one command path") {
  MemoryPages pages;
  const json yes = {{"confirm", true}};
  CHECK(pages.command(kForgetNpc, json{{"key", "2.lantern_keeper"}}, world()) == "confirm_required");
  CHECK(pages.command(kForgetNpc, json{{"key", "1.lantern_keeper"}, {"confirm", true}}, world()) == "read_only");
  CHECK(pages.command(kForgetNpc, json{{"key", "2.ferry_warden"}, {"confirm", true}}, world()) == "not_found");
  CHECK(pages.command(kForgetNpc, json{{"key", "2/lantern_keeper"}, {"confirm", true}}, world()) == "invalid_value");
  CHECK(pages.command(kForgetJourney, yes, nullptr) == "not_now");
  MemoryFacts busy = *world();
  busy.can_forget = false;
  CHECK(pages.command(kForgetJourney, yes, &busy) == "busy");
  CHECK(pages.take_requests().empty());
  CHECK(pages.command(kForgetNpc, json{{"key", "2.lantern_keeper"}, {"confirm", true}}, world()).empty());
  CHECK(pages.command(kForgetJourney, yes, world()).empty());
  const auto r = pages.take_requests();
  REQUIRE(r.size() == 2);
  CHECK(r[0] == MemoryRequest{MemoryRequest::Kind::ForgetNpc, 2, "lantern_keeper", 0});
  CHECK(r[1] == MemoryRequest{MemoryRequest::Kind::ForgetJourney, 2, "", 0});
}

TEST_CASE("memory pages: through the space: greyed at the title, and the forget answered") {
  Space s;
  SpaceFacts title;
  CHECK(s.body("space.state", title)["greyed"]["history.memories"] == "Load a character to see this.");
  SpaceFacts f;
  f.memory = world();
  CHECK_FALSE(s.body("space.state", f)["greyed"].contains("history.memories"));
  const json args = {{"confirm", true}};
  const std::string frame =
      json{{"v", 1}, {"t", "cmd"}, {"space", "eldennet"}, {"id", 9}, {"name", kForgetJourney}, {"args", args}}.dump();
  const json answer = json::parse(s.command(frame, f));
  CHECK(answer["ok"] == true);
  CHECK(s.take_memory_requests().size() == 1);
  const json refused = json::parse(s.command(frame, title));
  CHECK(refused["error"] == "not_now");
}

TEST_CASE("memory pages: the member keys") {
  CHECK(MemoryPages::member_key("2.lantern_keeper") == std::pair<std::uint16_t, std::string>{2, "lantern_keeper"});
  CHECK_FALSE(MemoryPages::member_key("lantern_keeper"));
  CHECK_FALSE(MemoryPages::member_key("0.lantern_keeper"));
  CHECK_FALSE(MemoryPages::member_key("2."));
  CHECK_FALSE(MemoryPages::member_key("2.Lantern"));
  CHECK(play_clock(3600000 + 2 * 60000) == "1:02");
  CHECK(disposition_word(-2) == "Hostile");
  CHECK(MemoryPages::owns("journal/2.x"));
  CHECK_FALSE(MemoryPages::owns("journals/2"));
}
