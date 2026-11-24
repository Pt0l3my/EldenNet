// The conversation's states and timers (NPC §6.1, §6.2), queue-don't-cancel (§6.3), the endings (§5.7) and the draft
// (§5.4), on a simulated clock with the stubs' speech. All data is invented.
#include <doctest.h>

#include <string>

#include "rig.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;
using proto::ConvState;

TEST_CASE("Closed to Open: the talk key on a conversable persona in a safe context, once the observer allows it") {
  Rig r;
  r.wait(2000);
  r.open();  // within the observer's 3 s: fails closed, silently
  CHECK(r.conv.state() == ConvState::Closed);
  CHECK(r.toasts.empty());
  CHECK_FALSE(r.last.ui.hint.has_value());
  r.wait(1250);
  REQUIRE(r.last.ui.hint.has_value());
  CHECK(r.last.ui.hint->verb == proto::HintVerb::Speak);
  CHECK(r.last.ui.hint->name == "Lantern-Keeper");
  CHECK(r.last.ui.hint->target == kKeeper);
  r.open();
  CHECK(r.conv.state() == ConvState::Open);
  CHECK(r.conv.partner() == "lantern_keeper");
  const proto::ChatBox& box = r.last.ui.chat;
  CHECK(box.view == proto::BoxView::Open);
  CHECK(box.state == ConvState::Open);
  CHECK(box.partner == "Lantern-Keeper");
  CHECK(box.heard_by == std::vector<std::string>{"Lantern-Keeper"});
  CHECK_FALSE(r.last.ui.hint.has_value());
  REQUIRE(r.of(Commit::Kind::Started).size() == 1);
  CHECK(r.of(Commit::Kind::Started)[0].persona == "lantern_keeper");
  CHECK(r.requests.empty());
}

TEST_CASE("Open to Awaiting to Streaming to Paging to Open: a reply's pages, each committed when it shows") {
  Rig r;
  r.open_box();
  r.say("  Where does the\troad go?  ");
  CHECK(r.conv.state() == ConvState::Awaiting);
  REQUIRE(r.of(Commit::Kind::PlayerSaid).size() == 1);
  CHECK(r.of(Commit::Kind::PlayerSaid)[0].text == "Where does the road go?");  // cleaned, as sent
  REQUIRE(r.requests.size() == 1);
  const proto::LlmRequest& req = r.requests[0];
  CHECK(req.id >= kConversationRequests);
  CHECK_FALSE(req.epoch.has_value());  // a reply never carries one
  const auto& chat = std::get<proto::ChatRequest>(req.body);
  CHECK(chat.slot == "dialogue");
  CHECK(chat.stop == std::vector<std::string>{"\nThe Tarnished:", "\nTarnished:"});
  CHECK_FALSE(r.page().has_value());  // nothing yet: the card waits 350 ms
  r.wait(400);
  REQUIRE(r.page().has_value());
  CHECK(r.page()->kind == proto::PageKind::Card);
  CHECK(r.page()->speaker == "Lantern-Keeper");
  CHECK(r.of(Commit::Kind::NpcSaid).empty());  // a card is never committed
  r.delta("Hm. The road runs east. Mind");
  CHECK(r.conv.state() == ConvState::Streaming);
  REQUIRE(r.page().has_value());
  CHECK(r.page()->kind == proto::PageKind::Speech);
  CHECK(r.page()->text == "Hm.");
  REQUIRE(r.of(Commit::Kind::NpcSaid).size() == 1);
  CHECK(r.of(Commit::Kind::NpcSaid)[0].text == "Hm.");
  r.delta(" the dark.");
  r.finish();
  CHECK(r.conv.state() == ConvState::Paging);
  r.wait(2000);  // page 0's hold
  CHECK(r.page()->text == "The road runs east.");
  r.wait(2000);
  CHECK(r.page()->text == "Mind the dark.");
  CHECK(r.page()->last);
  r.wait(2000);
  CHECK(r.conv.state() == ConvState::Open);
  CHECK_FALSE(r.page().has_value());
  CHECK(r.of(Commit::Kind::NpcPage).size() == 2);
  REQUIRE(r.log.events().size() == 3);  // Started, PlayerSaid, NpcSaid (grown by both pages)
  CHECK(r.log.events()[2].text == "Hm. The road runs east. Mind the dark.");
  CHECK(r.log.events()[2].pages == 3);
  const proto::ChatBox& box = r.last.ui.chat;
  REQUIRE(box.history.size() == 2);
  CHECK(box.history[1].who == proto::HistoryLine::Who::Npc);
  CHECK(box.history[1].text == "Hm. The road runs east. Mind the dark.");
}

TEST_CASE("Awaiting: a line waits in the one queued slot, a later one replaces it, and it goes at the last page") {
  Rig r;
  r.open_box();
  r.say("First.");
  r.say("Second, replaced.");
  r.say("Third.");
  CHECK(r.conv.queued() == "Third.");
  REQUIRE(r.requests.size() == 1);
  const auto& box = r.last.ui.chat;
  REQUIRE(box.history.size() == 2);
  CHECK(box.history[1].text == "Third.");
  CHECK(box.history[1].waiting);
  r.delta("Hm.");
  r.finish();  // "Hm." is the reply's one page, and its last
  CHECK(r.page()->last);
  CHECK(r.requests.size() == 2);  // dispatched when the last page started showing
  CHECK_FALSE(r.conv.queued().has_value());
  const auto said = r.of(Commit::Kind::PlayerSaid);
  REQUIRE(said.size() == 2);
  CHECK(said[1].text == "Third.");  // the replaced line was never committed
  // The new reply's first page waits for the last page's hold.
  r.delta("Yes. ");
  CHECK(r.page()->text == "Hm.");
  r.wait(2050);
  CHECK(r.page()->text == "Yes.");
}

TEST_CASE("a one-page reply sealed while it streamed: the queued line goes when the stream ends, not at the hold's end") {
  // NPC §6.3, WHY§432: the page was sealed before the reply ended, so it was not marked last when it showed.
  SUBCASE("the page showed before the stream ended") {
    Rig r;
    r.open_box();
    r.say("First.");
    r.say("Then this.");
    r.delta("Hm. ");
    REQUIRE(r.page().has_value());
    CHECK_FALSE(r.page()->last);
    r.wait(500);
    CHECK(r.requests.size() == 1);  // the stream goes on: the page up may not be the last
    r.finish();
    CHECK(r.requests.size() == 2);  // the stream ended with the last page showing: the line goes at once
    CHECK(r.page()->last);
    CHECK_FALSE(r.conv.queued().has_value());
    REQUIRE(r.of(Commit::Kind::PlayerSaid).size() == 2);
    // The new reply's first page still waits for the page up to finish its hold.
    r.delta("Yes. ");
    CHECK(r.page()->text == "Hm.");
    r.wait(1600);
    CHECK(r.page()->text == "Yes.");
  }
  SUBCASE("the stream ended before the page showed: the line goes when it shows") {
    Rig r;
    r.auto_ack = false;
    r.open_box();
    r.say("First.");
    r.say("Then this.");
    r.delta("Hm. ");
    REQUIRE(r.page().has_value());
    const proto::PageId id = r.page()->id;
    r.finish();
    r.wait(100);
    CHECK(r.requests.size() == 1);  // not shown yet
    CHECK(r.page()->last);
    r.shown(id);
    CHECK(r.requests.size() == 2);
  }
  SUBCASE("a later page sealed while it streamed: the same when the stream ends during its hold") {
    Rig r;
    r.open_box();
    r.say("First.");
    r.say("Then this.");  // queued while nothing has shown
    r.delta("One. ");
    r.delta("Two. ");
    CHECK(r.page()->text == "One.");
    r.wait(2050);  // page 1's hold ends while the reply streams: page 2 is sealed then, not marked last
    REQUIRE(r.page()->text == "Two.");
    CHECK_FALSE(r.page()->last);
    CHECK(r.requests.size() == 1);
    r.finish();
    CHECK(r.requests.size() == 2);
    CHECK(r.of(Commit::Kind::CutOff).empty());
  }
  SUBCASE("with more left to page when the stream ends, the line waits for the last page to show") {
    Rig r;
    r.open_box();
    r.say("First.");
    r.say("Then this.");
    r.delta("One. Two. ");
    CHECK(r.page()->text == "One.");
    r.finish();
    CHECK(r.requests.size() == 1);  // "Two." is still to come
    r.wait(2050);
    CHECK(r.page()->text == "Two.");
    CHECK(r.page()->last);
    CHECK(r.requests.size() == 2);
  }
}

TEST_CASE("Streaming: a line cuts the reply off after the page up has shown 2 s, unshown pages dropped") {
  Rig r;
  r.open_box();
  r.say("Tell me of the lamp.");
  r.delta("It burns. It burns all night. It burns for you. ");
  CHECK(r.page()->text == "It burns.");
  r.wait(500);
  const proto::RequestId first = r.requests.back().id;
  r.say("Enough.");
  CHECK(std::find(r.cancels.begin(), r.cancels.end(), first) != r.cancels.end());
  REQUIRE(r.of(Commit::Kind::CutOff).size() == 1);
  CHECK(r.requests.size() == 2);
  CHECK(r.conv.state() == ConvState::Awaiting);
  r.delta("As you wish. ");
  CHECK(r.page()->text == "It burns.");  // the page up stays its 2 s (§6.3 step 1)
  r.wait(1550);
  CHECK(r.page()->text == "As you wish.");
  for (const Commit& c : r.commits) CHECK(c.text.find("all night") == std::string::npos);  // never shown, never kept
  CHECK(r.log.events()[2].cut_off);
  CHECK(r.log.events()[2].text == "It burns.");
}

TEST_CASE("Awaiting to Open: no answer in 20 s gives the uncommitted ... page and a toast") {
  Rig r;
  r.open_box();
  r.say("Hello?");
  r.wait(19900);
  CHECK(r.conv.state() == ConvState::Awaiting);
  r.wait(200);
  CHECK(r.conv.state() == ConvState::Open);
  CHECK(r.cancels.size() == 1);
  REQUIRE(r.page().has_value());
  CHECK(r.page()->kind == proto::PageKind::Failure);
  CHECK(r.page()->text == "\xE2\x80\xA6");
  CHECK(r.toasted("No answer: provider timed out"));
  CHECK(r.of(Commit::Kind::NpcSaid).empty());
  r.wait(2500);
  CHECK_FALSE(r.page().has_value());
}

TEST_CASE("a failed request gives the ... page with its class's toast") {
  const auto run = [](proto::ErrorClass e) {
    Rig r;
    r.open_box();
    r.say("Hello?");
    r.finish(e);
    CHECK(r.conv.state() == ConvState::Open);
    CHECK(r.page()->kind == proto::PageKind::Failure);
    CHECK(r.of(Commit::Kind::NpcSaid).empty());
    return r.toasts;
  };
  CHECK(run(proto::ErrorClass::Auth) == std::vector<std::string>{"API key rejected: open settings (F10)"});
  CHECK(run(proto::ErrorClass::RateLimited) == std::vector<std::string>{"Provider rate limit, try again shortly"});
  CHECK(run(proto::ErrorClass::TotalTimeout) == std::vector<std::string>{"No answer: provider timed out"});
  CHECK(run(proto::ErrorClass::Server) == std::vector<std::string>{"No answer: the provider failed"});
}

TEST_CASE("a failed render sends nothing and takes the failed request's path") {
  Rig r;
  r.stub.prompt.refuse = PromptRefusal::OverCap;
  r.open_box();
  r.say("Hello?");
  CHECK(r.requests.empty());
  CHECK(r.page()->kind == proto::PageKind::Failure);
  CHECK(r.of(Commit::Kind::PlayerSaid).size() == 1);
}

TEST_CASE("a hard lint hit regenerates once, then the persona deflects") {
  Rig r;
  r.open_box();
  r.say("Say the word.");
  r.delta("BANNED word. ");
  CHECK(r.cancels.size() == 1);
  CHECK(r.requests.size() == 2);  // the regeneration
  CHECK_FALSE(r.page().has_value());
  r.delta("BANNED again. ");
  CHECK(r.cancels.size() == 2);
  REQUIRE(r.page().has_value());
  const std::vector<std::string> deflections = {"The lamp shows the road, not the whole of it.",
                                                "Ask again when the wick is longer.",
                                                "Some roads are better walked than described."};
  CHECK(std::find(deflections.begin(), deflections.end(), r.page()->text) != deflections.end());
  CHECK(r.page()->last);
  for (const Commit& c : r.commits) CHECK(c.text.find("BANNED") == std::string::npos);
  // A hit in a later sentence ends the reply at the previous one.
  r.wait(2100);
  r.say("Again.");
  r.delta("Fine. Then BANNED. ");
  CHECK(r.page()->text == "Fine.");
  CHECK(r.page()->last);
}

TEST_CASE("Held: native talk nearby holds what is not shown, resumes 1.5 s after, and drops at 10 s") {
  Rig r;
  r.open_box();
  r.keeper(4.0f);  // the partner beyond 3 m: native talk is someone else's
  r.say("Go on.");
  r.delta("One. Two. ");
  CHECK(r.page()->text == "One.");
  r.snap.ui.talk_cam = true;
  r.wait(250);
  CHECK(r.conv.state() == ConvState::Held);
  CHECK_FALSE(r.page().has_value());  // nothing over the game's own talk
  r.wait(3000);
  r.snap.ui.talk_cam = false;
  r.wait(1250);
  CHECK(r.conv.state() == ConvState::Held);
  r.wait(500);
  CHECK(r.conv.state() != ConvState::Held);
  CHECK(r.page()->text == "One.");  // its hold resumes where it paused
  r.wait(1900);
  CHECK(r.page()->text == "Two.");
  // Held again, past 10 s: the rest is dropped, never committed.
  r.delta("Three. ");
  r.snap.ui.talk_cam = true;
  r.wait(10250);
  CHECK(r.conv.state() == ConvState::Open);
  for (const Commit& c : r.commits) CHECK(c.text.find("Three") == std::string::npos);
}

TEST_CASE("Held: the overlay hidden holds the pages until it shows again, and drops them at 30 s") {
  Rig r;
  r.open_box();
  r.say("Go on.");
  r.delta("One. Two. ");
  r.snap.ui.hud = proto::HudState::HideAll;
  r.wait(250);
  CHECK(r.conv.state() == ConvState::Held);
  r.snap.ui.hud = proto::HudState::Default;
  r.wait(250);
  CHECK(r.conv.state() == ConvState::Streaming);
  r.snap.ui.hud = proto::HudState::HideAll;
  r.wait(29500);
  CHECK(r.conv.state() == ConvState::Held);
  r.wait(750);
  CHECK(r.conv.state() == ConvState::Open);
}

TEST_CASE("timers: walking away, idle, an unresolvable partner") {
  SUBCASE("more than 12 m for 2 s") {
    Rig r;
    r.open_box();
    r.keeper(13.0f);
    r.wait(1750);
    CHECK(r.conv.state() == ConvState::Open);
    r.wait(500);
    CHECK(r.conv.state() == ConvState::Closed);
    CHECK(r.ended == std::vector<EndReason>{EndReason::WalkedAway});
  }
  SUBCASE("120 s without a player line") {
    Rig r;
    r.open_box();
    r.wait(119900);
    CHECK(r.conv.state() == ConvState::Open);
    r.wait(200);
    CHECK(r.ended == std::vector<EndReason>{EndReason::Idle});
  }
  SUBCASE("the partner gone from the snapshot for 2 s") {
    Rig r;
    r.open_box();
    r.remove(kKeeper);
    r.wait(1750);
    CHECK(r.conv.state() == ConvState::Open);
    r.wait(500);
    CHECK(r.ended == std::vector<EndReason>{EndReason::NpcState});
  }
}

TEST_CASE("the endings at once: travel, dormant, death, the partner's state, native talk, a degraded group, a fault") {
  const auto ends = [](auto change) {
    Rig r;
    r.open_box();
    change(r);
    r.wait(250);
    CHECK(r.conv.state() == ConvState::Closed);
    REQUIRE(r.ended.size() == 1);
    const auto ended = r.of(Commit::Kind::Ended);
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].reason == r.ended[0]);
    return r.ended[0];
  };
  CHECK(ends([](Rig& r) { r.loading = true; }) == EndReason::Travel);
  CHECK(ends([](Rig& r) { r.dormant = true; }) == EndReason::Dormant);
  CHECK(ends([](Rig& r) { r.snap.player->dead = true; }) == EndReason::Death);
  CHECK(ends([](Rig& r) { r.keeper(2.5f).dead = true; }) == EndReason::NpcState);
  CHECK(ends([](Rig& r) { r.keeper(2.5f).team = 6; }) == EndReason::NpcState);
  CHECK(ends([](Rig& r) { r.snap.ui.talk_cam = true; }) == EndReason::NativeTalk);  // the partner within 3 m
  CHECK(ends([](Rig& r) { r.snap.health.of(proto::Group::Flags).state = proto::GroupState::Tripped; }) ==
        EndReason::Degraded);
  Rig f;
  f.open_box();
  f.close(proto::CloseReason::Fault);
  CHECK(f.ended == std::vector<EndReason>{EndReason::Fault});
  CHECK(f.toasted("Conversation closed after a fault in EldenNet."));
}

TEST_CASE("a refused box names the holder GraceUI gives, a not-now one its reason; anything else in generic words") {
  // WHY§442: the Close's detail is the holder's mod id (GraceUI's menu is "graceui") or the overlay's word for why.
  const auto toast_for = [](proto::CloseReason why, const std::string& detail) {
    Rig r;
    r.open_box();
    r.ui(proto::UiEvent{proto::UiEvent::Close{why, "", detail}});
    CHECK(r.ended == std::vector<EndReason>{EndReason::Closed});  // no line yet: the conversation never began
    REQUIRE(r.toasts.size() == 1);
    return r.toasts.front();
  };
  using proto::CloseReason;
  CHECK(toast_for(CloseReason::Refused, "graceui") == "Chat cannot take the keyboard: GraceUI's menu has the controls.");
  CHECK(toast_for(CloseReason::Refused, "another-mod") ==
        "Chat cannot take the keyboard: another mod (another-mod) has the controls.");
  // Not an id as GraceUI checks one (GraceUI's own fallback, a quote, a space, too long, empty): never repeated.
  for (const char* odd : {"a mod", "Mod\"X", "x", "", "abcdefghijklmnopqrstuvwxyz0123456789"}) {
    CAPTURE(odd);
    CHECK(toast_for(CloseReason::Refused, odd) == "Chat cannot take the keyboard: another mod has the controls.");
  }
  CHECK(toast_for(CloseReason::Refused, "eldennet") == "Chat cannot take the keyboard just now.");
  CHECK(toast_for(CloseReason::NotNow, "danger") == "Chat cannot take the keyboard: danger is near.");
  CHECK(toast_for(CloseReason::NotNow, "loading") == "Chat cannot take the keyboard while the game loads.");
  CHECK(toast_for(CloseReason::NotNow, "game_menu") == "Chat cannot take the keyboard while a game menu is open.");
  CHECK(toast_for(CloseReason::NotNow, "not_ready") == "Chat cannot take the keyboard just now.");
  CHECK(toast_for(CloseReason::NotNow, "another-mod") == "Chat cannot take the keyboard just now.");
}

TEST_CASE("Esc: at once with nothing pending; with a reply pending it closes the box, and no press ends it sooner") {
  SUBCASE("nothing pending") {
    Rig r;
    r.open_box();
    r.close(proto::CloseReason::Escape, "a half line");
    CHECK(r.ended == std::vector<EndReason>{EndReason::Closed});
  }
  SUBCASE("the reply paces out, then the conversation ends") {
    Rig r;
    r.open_box();
    r.say("Go on.");
    r.delta("One. Two. ");
    r.close(proto::CloseReason::Escape);
    CHECK_FALSE(r.conv.box_open());
    CHECK(r.conv.state() == ConvState::Streaming);
    CHECK(r.page()->text == "One.");
    r.finish();
    r.wait(4100);
    CHECK(r.ended == std::vector<EndReason>{EndReason::Closed});
    CHECK(r.of(Commit::Kind::NpcPage).size() == 1);  // "Two." showed
  }
  SUBCASE("there is no second Esc: one after the box has closed changes nothing, and the reply paces out") {
    // WHY§461 (the owner, GraceUI's G39 (c)): with the box closed Esc is the game's; were one to reach the core all
    // the same, it neither ends the conversation nor stops the reply.
    Rig r;
    r.open_box();
    r.say("Go on.");
    r.delta("One. Two. ");
    r.close(proto::CloseReason::Escape);
    r.close(proto::CloseReason::Escape, "never typed");
    CHECK(r.ended.empty());
    CHECK(r.cancels.empty());
    CHECK(r.conv.state() == ConvState::Streaming);
    r.finish();
    r.wait(4100);
    CHECK(r.ended == std::vector<EndReason>{EndReason::Closed});
    CHECK(r.of(Commit::Kind::NpcPage).size() == 1);  // "Two." showed all the same
    CHECK(r.cancels.empty());
  }
  SUBCASE("the box opened again while the reply paces: the conversation goes on after it") {
    Rig r;
    r.open_box();
    r.say("Go on.");
    r.delta("One. Two. ");
    r.close(proto::CloseReason::Escape);
    r.wait(400);
    r.open();
    CHECK(r.conv.box_open());
    r.finish();
    r.wait(4100);
    CHECK(r.ended.empty());
    CHECK(r.conv.box_open());
    r.close(proto::CloseReason::Escape);  // nothing pending now: at once
    CHECK(r.ended == std::vector<EndReason>{EndReason::Closed});
  }
}

TEST_CASE("danger closes the box at once and keeps the draft; pages pace on; 10 s of it ends the conversation") {
  Rig r;
  r.open_box();
  r.say("Go on.");
  r.delta("One. Two. ");
  proto::Chr wolf = character(kWolf, 9900, 10.0f);
  wolf.team = 6;
  r.add(wolf);
  r.wait(250);
  CHECK_FALSE(r.conv.box_open());
  CHECK(r.last.ui.chat.view == proto::BoxView::Closed);
  CHECK(r.toasted("The moment has passed."));
  CHECK(r.page()->text == "One.");  // shown pages keep pacing
  r.close(proto::CloseReason::Shut, "my half line");
  // The danger clears in time: the talk key reopens the box on the same target, with the draft.
  r.remove(kWolf);
  r.wait(1000);
  CHECK(r.conv.state() != ConvState::Closed);
  r.open();
  CHECK(r.conv.box_open());
  CHECK(r.last.ui.chat.draft == "my half line");
  // Danger again, for 10 s.
  r.add(wolf);
  r.wait(9900);
  CHECK(r.conv.state() != ConvState::Closed);
  r.wait(500);
  CHECK(r.ended == std::vector<EndReason>{EndReason::Danger});
}

TEST_CASE("the draft is kept 60 s for its target") {
  Rig r;
  r.open_box();
  r.close(proto::CloseReason::FocusLost, "kept for later");
  r.open();
  CHECK(r.last.ui.chat.draft == "kept for later");
  r.close(proto::CloseReason::Escape, "after the end");
  CHECK(r.conv.state() == ConvState::Closed);
  r.wait(30000);
  r.open();
  CHECK(r.last.ui.chat.draft == "after the end");
  r.close(proto::CloseReason::Escape, "too old");
  r.wait(60100);
  r.open();
  CHECK(r.last.ui.chat.draft.empty());
}

TEST_CASE("a box GraceUI refuses at its opening keeps the draft for the next, within the first 60 s") {
  // WHY§586: GraceUI's danger close keeps the draft; the talk key during that danger, which the core does not count,
  // reopens the box, GraceUI answers not now (the close carries no draft: the box never held input), and the
  // conversation ends with no line said. The draft comes back at the next opening, its 60 s from the first close.
  for (const proto::CloseReason why : {proto::CloseReason::NotNow, proto::CloseReason::Refused}) {
    CAPTURE(static_cast<int>(why));
    Rig r;
    r.open_box();
    r.close(proto::CloseReason::InputLost, "my half line");
    r.wait(1000);
    r.open();
    CHECK(r.last.ui.chat.draft == "my half line");
    r.ui(proto::UiEvent{proto::UiEvent::Close{why, "", why == proto::CloseReason::NotNow ? "danger" : "graceui"}});
    CHECK(r.ended == std::vector<EndReason>{EndReason::Closed});
    r.wait(1000);
    r.open();
    REQUIRE(r.conv.box_open());
    CHECK(r.last.ui.chat.draft == "my half line");
    // Refused again, then 60 s from the first close: gone.
    r.ui(proto::UiEvent{proto::UiEvent::Close{why, "", "danger"}});
    r.wait(58100);
    r.open();
    CHECK(r.last.ui.chat.draft.empty());
  }
  // A line sent from the box: the store's copy goes with it, so that no later box offers what was sent, even where
  // no close of the box's came after the line (here the core's own danger close, which the rig never answers).
  Rig r;
  r.open_box();
  r.close(proto::CloseReason::FocusLost, "sent at once");
  r.open();
  REQUIRE(r.last.ui.chat.draft == "sent at once");
  r.say("sent at once");
  proto::Chr wolf = character(kWolf, 9900, 10.0f);
  wolf.team = 6;
  r.add(wolf);
  r.wait(10500);
  CHECK(r.ended == std::vector<EndReason>{EndReason::Danger});
  r.remove(kWolf);
  r.wait(1000);
  r.open();
  REQUIRE(r.conv.box_open());
  CHECK(r.last.ui.chat.draft.empty());
}

TEST_CASE("Retarget: the talk key on another conversable persona ends this one after its page and opens that one") {
  Rig r;
  r.add(character(kWren, 4100, 6.0f, 40.0f));
  r.open_box();
  CHECK(r.conv.partner() == "lantern_keeper");
  r.close(proto::CloseReason::FocusLost);
  r.snap.player->locked_on = kWren;  // rule 3, the lock-on target
  r.wait(750);
  r.open();
  CHECK(r.ended == std::vector<EndReason>{EndReason::Retarget});
  CHECK(r.conv.partner() == "wren");
  CHECK(r.conv.box_open());
  CHECK(r.of(Commit::Kind::Started).size() == 2);
}

TEST_CASE("on an end the input epoch moves and own-initiative lines stay quiet for 3 s") {
  Rig r;
  const auto e0 = r.conv.epoch();
  r.open_box();
  CHECK(r.conv.epoch() == e0 + 1);
  r.close(proto::CloseReason::Escape);
  CHECK(r.conv.epoch() == e0 + 2);
  CHECK(r.conv.barks_quiet_until() == r.now + 3000);
}

TEST_CASE("a game banner stops the box opening but never closes an open one (O108)") {
  Rig r;
  r.open_box();
  r.snap.ui.banner = 3;
  r.wait(500);
  CHECK(r.conv.box_open());
  CHECK_FALSE(r.toasted("The moment has passed."));
  r.close(proto::CloseReason::Escape, "a half line");
  REQUIRE_FALSE(r.conv.box_open());
  r.open();  // the banner still shows: the box does not open
  CHECK_FALSE(r.conv.box_open());
  r.snap.ui.banner.reset();
  r.wait(500);
  r.open();
  CHECK(r.conv.box_open());
}
