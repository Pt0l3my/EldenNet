// The conversation wired into the core (core.hpp; WHY§364): a whole session from boot, with the stubs' speech, the
// tests' overrides binding the test persona and the model client's events played back, ending in the core's event log;
// and C4: a session whose gate never opens sends nothing, whatever the player does. All data is invented.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/core/core.hpp"
#include "en/core/stubs.hpp"
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

struct Session {
  stubs::StubSpeech speech;
  Core core{speech.parts()};
  Mono t = 0;
  std::vector<proto::LlmRequest> requests;
  std::vector<proto::RequestId> cancels;
  Outputs last;

  Outputs feed(CoreInput in) {
    last = core.step(std::move(in), t);
    for (auto& r : last.requests) requests.push_back(r);
    for (auto id : last.cancels) cancels.push_back(id);
    return std::move(last);
  }
  void world(int snapshots, bool solo = true) {
    for (int i = 0; i < snapshots; ++i) {
      t += 250;
      proto::WorldSnapshot s = with_keeper(test::world(t));
      if (!solo) s.session.player_set_len = 2;  // another player in the world
      const Outputs o = feed(in(s));
      last = o;
      if (o.ui.page && o.ui.page->kind == proto::PageKind::Speech) {
        t += 10;
        last = feed(CoreInput{proto::UiEvent{proto::UiEvent::PageShown{o.ui.page->id, t}}});
      }
    }
  }
};

}  // namespace

TEST_CASE("a whole session: boot, the world, the talk key, a line, a reply shown and committed") {
  Session s;
  CHECK(s.core.load_content(content_with_overrides(), Realm::Eval).empty());
  CHECK(s.core.directory().size() == 1);
  (void)s.feed(in(data_ready()));
  (void)s.feed(CoreInput{proto::NetHealth{true}});
  CHECK(s.core.state().level == Level::L4);
  (void)s.feed(in(playthrough_ready()));
  s.world(30);  // into the world, the gate's 5 s and the observer's 3 s
  REQUIRE(s.core.state().phase == Phase::InWorld);
  REQUIRE(s.core.state().gate_open);
  REQUIRE(s.last.ui.hint.has_value());
  CHECK(s.last.ui.hint->name == "Lantern-Keeper");
  Outputs o = s.feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
  CHECK(o.ui.chat.view == proto::BoxView::Open);
  REQUIRE(o.commits.size() == 1);
  CHECK(o.commits[0].kind == Commit::Kind::Started);
  o = s.feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{"Is the lamp yours?"}}});
  REQUIRE(o.requests.size() == 1);
  CHECK(o.ai_allowed);
  const proto::RequestId id = o.requests[0].id;
  CHECK(id >= kConversationRequests);
  proto::LlmEvent d;
  d.kind = proto::EventKind::Delta;
  d.id = id;
  d.text = "Hm. It keeps me as I keep it. ";
  (void)s.feed(CoreInput{d});
  proto::LlmEvent e;
  e.kind = proto::EventKind::Ended;
  e.id = id;
  e.chat = proto::ChatResult{};
  (void)s.feed(CoreInput{e});
  s.world(20);
  const auto& log = s.core.log().events();
  REQUIRE(log.size() == 3);
  CHECK(log[0].kind == Commit::Kind::Started);
  CHECK(log[1].kind == Commit::Kind::PlayerSaid);
  CHECK(log[1].text == "Is the lamp yours?");
  CHECK(log[2].kind == Commit::Kind::NpcSaid);
  CHECK(log[2].text == "Hm. It keeps me as I keep it.");
  CHECK(log[2].pages == 2);
  CHECK(s.core.conversation().state() == proto::ConvState::Open);
  // A loading screen ends it at once, and the end is committed in the same step.
  s.t += 250;
  o = s.feed(in(loading(s.t)));
  CHECK(o.ended == EndReason::Travel);
  CHECK(s.core.log().events().back().kind == Commit::Kind::Ended);
  CHECK(s.core.log().events().back().reason == EndReason::Travel);
}

TEST_CASE("in the game's realm the test persona never resolves, so the talk key opens nothing") {
  Session s;
  CHECK(s.core.load_content(content_with_overrides(), Realm::Game).empty());
  (void)s.feed(in(data_ready()));
  (void)s.feed(CoreInput{proto::NetHealth{true}});
  (void)s.feed(in(playthrough_ready()));
  s.world(30);
  CHECK_FALSE(s.last.ui.hint.has_value());
  const Outputs o = s.feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
  CHECK(o.ui.chat.view == proto::BoxView::Closed);
  CHECK(o.commits.empty());
}

TEST_CASE("C4: with the gate closed nothing is ever sent, whatever the player does") {
  Session s;
  CHECK(s.core.load_content(content_with_overrides(), Realm::Eval).empty());
  (void)s.feed(in(data_ready()));
  (void)s.feed(CoreInput{proto::NetHealth{true}});
  (void)s.feed(in(playthrough_ready()));
  for (int i = 0; i < 40; ++i) {
    s.world(1, false);  // co-op: the gate never opens
    (void)s.feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
    (void)s.feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{"Hello?"}}});
    CHECK_FALSE(s.last.ai_allowed);
  }
  CHECK(s.requests.empty());
  CHECK(s.core.log().events().empty());
}

TEST_CASE("the built-in content loads in the game's realm: the pilot's lead row binds, the test persona's none") {
  Core core;
  CHECK(core.load_content(builtin_content(), Realm::Game).empty());
  CHECK(core.directory().size() == 2);
  CHECK(core.directory().index().rows() == 1);
  std::vector<ContentFile> broken = builtin_content();
  broken[0].text += "\nunknown_key = 1\n";
  CHECK_FALSE(core.load_content(broken, Realm::Game).empty());
  CHECK(core.directory().size() == 2);  // the old content stays
}

TEST_CASE("content loaded anew while a conversation is open ends it, its request cancelled") {
  Session s;
  CHECK(s.core.load_content(content_with_overrides(), Realm::Eval).empty());
  (void)s.feed(in(data_ready()));
  (void)s.feed(CoreInput{proto::NetHealth{true}});
  (void)s.feed(in(playthrough_ready()));
  s.world(30);
  (void)s.feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
  const Outputs said = s.feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{"Still there?"}}});
  REQUIRE(said.requests.size() == 1);
  std::vector<ContentFile> files = content_with_overrides();
  files.push_back(ContentFile{"prompts/invented.md", "changed", ContentOrigin::Override});  // not in the hash
  for (ContentFile& f : files) {
    if (f.path == "identity/personas.toml" && f.origin == ContentOrigin::Override) f.text += "\n# the hash changes\n";
  }
  CHECK(s.core.load_content(files, Realm::Eval).empty());
  const Outputs o = s.feed(CoreInput{CoreInput::Tick{}});
  CHECK(o.ended == EndReason::Fault);
  REQUIRE(o.cancels.size() == 1);
  CHECK(o.cancels[0] == said.requests[0].id);
  s.world(20);  // the targets come back from the new content
  CHECK(s.last.ui.hint.has_value());
}
