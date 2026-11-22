// The core's built-in speech (core.cpp's BuiltinSpeech; WHY§364): part A's sanitizer, lint, pager and prompt over the
// loaded content's templates and lexicon, run by part B's conversation. A reply's markup and reasoning taken out before
// it shows, the lint regenerating once with block 11's reminder and then deflecting, the pager's cap cancelling the
// stream, the tic note carried to the next prompt, and the persona's own name stripped from a reply's start. The
// shipped test persona, bound by the tests' overrides; every line is invented.
#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "en/core/core.hpp"
#include "../snapshots.hpp"
#include "../test_files.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

namespace {

proto::WorldSnapshot with_keeper(proto::WorldSnapshot s) {
  s.player->handle = proto::ChrHandle{1};
  s.player->hp = proto::Meter{400, 400};
  s.ui.hud = proto::HudState::Default;
  s.ui.subtitles_on = true;
  proto::Chr c;
  c.handle = proto::ChrHandle{0x500};
  c.param = 81100;
  c.dist_m = 2.0f;
  c.bearing_deg = 3.0f;
  c.hp = proto::Meter{100, 100};
  (void)s.chars.try_push_back(c);
  return s;
}

// A session on the core's own speech: boot, the world, the box open on the test persona.
struct Talk {
  Core core;
  Mono t = 0;
  std::vector<proto::LlmRequest> requests;
  std::vector<proto::RequestId> cancels;
  Outputs last;
  std::optional<proto::PageId> acked;

  Talk() {
    REQUIRE(core.load_content(content_with_overrides(), Realm::Eval).empty());
    (void)feed(in(data_ready()));
    (void)feed(CoreInput{proto::NetHealth{true}});
    (void)feed(in(playthrough_ready()));
    world(30);
    (void)feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
    REQUIRE(last.ui.chat.view == proto::BoxView::Open);
  }
  Outputs feed(CoreInput input) {
    last = core.step(std::move(input), t);
    for (auto& r : last.requests) requests.push_back(r);
    for (auto id : last.cancels) cancels.push_back(id);
    return last;
  }
  // Time passes in snapshots of 250 ms; each speech page is acknowledged on its first frame.
  void world(int snapshots) {
    for (int i = 0; i < snapshots; ++i) {
      t += 250;
      const Outputs o = feed(in(with_keeper(test::world(t))));
      if (o.ui.page && o.ui.page->kind == proto::PageKind::Speech && o.ui.page->id != acked) {
        acked = o.ui.page->id;
        t += 10;
        (void)feed(CoreInput{proto::UiEvent{proto::UiEvent::PageShown{o.ui.page->id, t}}});
      }
    }
  }
  void say(const std::string& line) { (void)feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{line}}}); }
  void delta(const std::string& text) {
    proto::LlmEvent d;
    d.kind = proto::EventKind::Delta;
    d.id = requests.back().id;
    d.text = text;
    (void)feed(CoreInput{d});
  }
  void ended() {
    proto::LlmEvent e;
    e.kind = proto::EventKind::Ended;
    e.id = requests.back().id;
    e.chat = proto::ChatResult{};
    (void)feed(CoreInput{e});
  }
  // The NpcSaid lines in the log, each with its later pages joined.
  [[nodiscard]] std::vector<std::string> shown() const {
    std::vector<std::string> out;
    for (const LoggedEvent& e : core.log().events()) {
      if (e.kind == Commit::Kind::NpcSaid) out.push_back(e.text);
    }
    return out;
  }
  [[nodiscard]] std::string prompt_of(std::size_t i) const {
    std::string all;
    for (const proto::Message& m : std::get<proto::ChatRequest>(requests.at(i).body).messages) all += m.joined() + "\n";
    return all;
  }
};

constexpr const char* kBannedReminder = "Use only words a person of this land would use.";

}  // namespace

TEST_CASE("the core's own speech: reasoning, a stage direction and the speaker's name are taken out before it shows") {
  Talk s;
  s.say("Is the lamp yours?");
  REQUIRE(s.requests.size() == 1);
  // The real prompt: the world frame, the persona's record, block 11's ask in its own name.
  CHECK(s.prompt_of(0).find("Lantern-Keeper") != std::string::npos);
  CHECK(s.prompt_of(0).find("end of the road") == std::string::npos);  // the stage's forbid: never rendered
  s.delta("<think>the lamp</think>Lantern-Keeper: *nods* Hm. It ");
  s.delta("keeps me as I keep it.");
  s.ended();
  s.world(20);
  REQUIRE(s.shown().size() == 1);
  CHECK(s.shown()[0] == "Hm. It keeps me as I keep it.");
  CHECK(s.core.log().events().back().pages == 1);  // "Hm." joins the next sentence on its page
  CHECK(s.cancels.empty());
}

TEST_CASE("the core's own speech: a banned word regenerates once with block 11's reminder, then deflects") {
  Talk s;
  s.say("Where am I?");
  REQUIRE(s.requests.size() == 1);
  CHECK(s.prompt_of(0).find(kBannedReminder) == std::string::npos);
  s.delta("This is a video game, traveller. ");
  s.delta("The lamp ");
  // The hard hit in the reply's first sentence: cancelled, and sent again with the reminder.
  REQUIRE(s.requests.size() == 2);
  REQUIRE(s.cancels.size() == 1);
  CHECK(s.cancels[0] == s.requests[0].id);
  CHECK(s.prompt_of(1).find(kBannedReminder) != std::string::npos);
  s.delta("A crossroads, and a lamp to see it by.");
  s.ended();
  s.world(20);
  REQUIRE(s.shown().size() == 1);
  CHECK(s.shown()[0] == "A crossroads, and a lamp to see it by.");

  // Again, and the regeneration hits too: the persona's deflection, the least recently used (the file's first).
  s.say("Truly?");
  REQUIRE(s.requests.size() == 3);
  s.delta("You are in a video game. ");
  s.delta("And");
  REQUIRE(s.requests.size() == 4);
  s.delta("Press the menu button. ");
  s.delta("And");
  CHECK(s.requests.size() == 4);  // one regeneration only
  s.world(20);
  REQUIRE(s.shown().size() == 2);
  CHECK(s.shown()[1] == "The lamp shows the road, not the whole of it.");
}

TEST_CASE("the core's own speech: the pager's cap fills the reply and the stream is cancelled") {
  Talk s;
  s.say("Tell me everything.");
  REQUIRE(s.requests.size() == 1);
  // The test persona's cap is 30 words: the second sentence crosses it within the cap and 10, and fills the reply.
  s.delta("The lamp burns all night, and the oil runs low by dawn most days. ");
  s.delta("I trim the wick at dusk and again near midnight, when the wind turns cold off the hills. ");
  s.delta("Then I wait. ");
  REQUIRE(s.cancels.size() == 1);
  CHECK(s.cancels[0] == s.requests[0].id);
  s.delta("Nothing more comes.");  // a cancelled stream's late delta is ignored
  s.ended();
  s.world(40);
  REQUIRE(s.shown().size() == 1);
  CHECK(s.shown()[0] ==
        "The lamp burns all night, and the oil runs low by dawn most days. I trim the wick at dusk and again near "
        "midnight, when the wind turns cold off the hills.");
  CHECK(s.shown()[0].find("Then I wait") == std::string::npos);
}

TEST_CASE("the core's own speech: a tic spent in one reply is noted in the next prompt's block 11") {
  Talk s;
  s.say("Any advice?");
  s.delta("Mind the dark between lamps.");
  s.ended();
  s.world(20);
  REQUIRE(s.shown().size() == 1);
  s.say("Anything else?");
  REQUIRE(s.requests.size() == 2);
  CHECK(s.prompt_of(0).find("has had its turn") == std::string::npos);
  CHECK(s.prompt_of(1).find("\"Mind the dark between lamps.\" has had its turn") != std::string::npos);
}
