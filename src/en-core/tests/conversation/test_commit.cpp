// Commit on display (DESIGN §8.5, NPC §8.7): the first PageShown makes the NpcSaid and each later one grows it, a page
// never acknowledged is never committed, acks are taken 500 ms after an end, a duplicate is ignored, and a late skip
// never ends the page after the one it was meant for. All data is invented.
#include <doctest.h>

#include <string>

#include "rig.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

TEST_CASE("a page that is sealed and put up but never acknowledged is never committed") {
  Rig r;
  r.auto_ack = false;
  r.open_box();
  r.say("Speak.");
  r.delta("Unseen words. ");
  REQUIRE(r.page().has_value());
  CHECK(r.page()->kind == proto::PageKind::Speech);
  r.wait(5000);  // no PageShown: the page holds its place, unacknowledged, and nothing is committed
  CHECK(r.of(Commit::Kind::NpcSaid).empty());
  r.say("Never mind.");  // queued: nothing of the reply has shown
  r.close(proto::CloseReason::Escape);
  r.close(proto::CloseReason::Escape);  // the second Esc: at once
  r.wait(1000);
  CHECK(r.of(Commit::Kind::NpcSaid).empty());
  CHECK(r.of(Commit::Kind::PlayerSaid).size() == 1);  // the queued line was never sent, never committed
  for (const LoggedEvent& e : r.log.events()) CHECK(e.text.find("Unseen") == std::string::npos);
}

TEST_CASE("the ack of the first page creates the NpcSaid, later acks grow it, duplicates are ignored") {
  Rig r;
  r.auto_ack = false;
  r.open_box();
  r.say("Speak.");
  r.delta("One. Two. ");
  r.finish();
  const proto::PageId first = r.page()->id;
  CHECK(first.index == 0);
  r.shown(first);
  r.shown(first);  // a duplicate
  REQUIRE(r.of(Commit::Kind::NpcSaid).size() == 1);
  r.wait(2100);
  REQUIRE(r.page().has_value());
  CHECK(r.page()->id.index == 1);
  CHECK(r.of(Commit::Kind::NpcPage).empty());  // put up, not yet shown
  r.shown(r.page()->id);
  REQUIRE(r.of(Commit::Kind::NpcPage).size() == 1);
  CHECK(r.of(Commit::Kind::NpcPage)[0].page == 1);
  r.shown(proto::PageId{first.conv, first.reply, 7});  // a page that was never put up
  CHECK(r.of(Commit::Kind::NpcPage).size() == 1);
  CHECK(r.log.events().back().text == "One. Two.");
}

TEST_CASE("after an end, the last reply's acks are taken for 500 ms, and no later") {
  for (const Mono late : {Mono{400}, Mono{600}}) {
    CAPTURE(late);
    Rig r;
    r.auto_ack = false;
    r.open_box();
    r.say("Speak.");
    r.delta("Seen at the last moment. ");
    const proto::PageId id = r.page()->id;
    r.loading = true;  // a loading screen: the conversation ends at once
    r.step();
    REQUIRE(r.ended == std::vector<EndReason>{EndReason::Travel});
    r.now += late;
    r.loading = false;
    r.shown(id);
    CHECK(r.of(Commit::Kind::NpcSaid).size() == (late <= 500 ? 1u : 0u));
  }
}

TEST_CASE("Tab ends a page's hold, no sooner than 300 ms after it showed; a late skip never ends the next page") {
  Rig r;
  r.open_box();
  r.say("Speak.");
  r.delta("One. Two. Three. ");
  r.finish();
  const proto::PageId first = r.page()->id;
  r.wait(100);
  r.skip(first);
  r.wait(150);
  CHECK(r.page()->id == first);  // 250 ms: not yet
  r.wait(100);
  CHECK(r.page()->id.index == 1);  // 350 ms: the skip took effect at 300
  r.skip(first);  // late: Tab went down while the first page showed, the event came after it went
  r.wait(1000);
  CHECK(r.page()->id.index == 1);  // the second page holds its full 2 s
  r.wait(1100);
  CHECK(r.page()->id.index == 2);
}

TEST_CASE("a cut-off finalises the NpcSaid with the pages shown, marked cut off") {
  Rig r;
  r.open_box();
  r.say("Speak.");
  r.delta("One. Two. Three. ");
  r.wait(2100);
  CHECK(r.page()->text == "Two.");
  r.say("Stop.");
  REQUIRE(r.log.events().size() >= 4);
  const LoggedEvent& said = r.log.events()[2];
  CHECK(said.kind == Commit::Kind::NpcSaid);
  CHECK(said.text == "One. Two.");
  CHECK(said.cut_off);
  CHECK(said.pages == 2);
  CHECK(r.log.events()[3].text == "Stop.");
}

TEST_CASE("the history strip and the next prompt read only committed text") {
  Rig r;
  r.auto_ack = false;
  r.open_box();
  r.say("Speak.");
  r.delta("Shown. Not shown. ");
  r.shown(r.page()->id);
  r.say("And then?");  // cuts off after "Shown."
  const auto& box = r.last.ui.chat;
  REQUIRE(box.history.size() == 3);
  CHECK(box.history[1].text == "Shown.");
  CHECK(box.history[2].text == "And then?");
  for (const proto::HistoryLine& l : box.history) CHECK(l.text.find("Not shown") == std::string::npos);
}
