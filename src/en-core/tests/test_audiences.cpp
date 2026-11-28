// Personas in Augur and the conversation's events with frozen audiences (MEM §1.1 to §1.3; v0.3): the Lantern-Keeper
// talked with at 2 m, the pilot 12 m off (overhearing) or 17 m off (hearing nothing), from the pilot's overrides in
// the eval's realm, with the stubs' speech. Invented data throughout.
#include <doctest.h>

#include <map>
#include <string>
#include <vector>

#include "en/augur/audience.hpp"
#include "en/core/core.hpp"
#include "en/core/stubs.hpp"
#include "snapshots.hpp"
#include "test_files.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

namespace {

std::vector<ContentFile> pilot_content() {
  std::vector<ContentFile> files = builtin_content();
  for (ContentFile& f : files_under("tests/scenarios/talk/pilot-overrides/", ContentOrigin::Override)) {
    files.push_back(std::move(f));
  }
  return files;
}

proto::Chr npc(std::uint64_t handle, std::uint32_t param, std::array<float, 3> pos) {
  proto::Chr c;
  c.handle = proto::ChrHandle{handle};
  c.param = param;
  c.pos = pos;
  c.dist_m = std::sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
  c.bearing_deg = 3.0f;
  c.hp = proto::Meter{100, 100};
  c.team = 1;
  return c;
}

struct Session {
  stubs::StubSpeech speech;
  Core core{speech.parts()};
  Mono t = 0;
  std::array<float, 3> kale_at{12, 0, 0};
  std::vector<augur::Event> events;

  Outputs feed(CoreInput in) {
    Outputs o = core.step(std::move(in), t);
    events.insert(events.end(), o.events.begin(), o.events.end());
    return o;
  }
  void world(int snapshots) {
    for (int i = 0; i < snapshots; ++i) {
      t += 250;
      proto::WorldSnapshot s = test::world(t);
      s.player->handle = proto::ChrHandle{1};
      s.player->hp = proto::Meter{400, 400};
      s.ui.hud = proto::HudState::Default;
      s.ui.subtitles_on = true;
      (void)s.chars.try_push_back(npc(0x500, 81100, {2, 0, 0}));  // the Lantern-Keeper
      (void)s.chars.try_push_back(npc(0x600, 81200, kale_at));    // the pilot
      const Outputs o = feed(in(s));
      if (o.ui.page && o.ui.page->kind == proto::PageKind::Speech) {
        t += 10;
        (void)feed(CoreInput{proto::UiEvent{proto::UiEvent::PageShown{o.ui.page->id, t}}});
      }
    }
  }
  // A whole line and its reply, committed on display.
  void talk() {
    REQUIRE(core.load_content(pilot_content(), Realm::Eval).empty());
    (void)feed(in(data_ready()));
    (void)feed(CoreInput{proto::NetHealth{true}});
    (void)feed(in(playthrough_ready()));
    world(30);
    REQUIRE(core.state().phase == Phase::InWorld);
    (void)feed(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
    const Outputs o = feed(CoreInput{proto::UiEvent{proto::UiEvent::Submit{"Is the lamp yours?"}}});
    REQUIRE(o.requests.size() == 1);
    proto::LlmEvent d;
    d.kind = proto::EventKind::Delta;
    d.id = o.requests[0].id;
    d.text = "Hm. It keeps me as I keep it. ";
    (void)feed(CoreInput{d});
    proto::LlmEvent e;
    e.kind = proto::EventKind::Ended;
    e.id = o.requests[0].id;
    e.chat = proto::ChatResult{};
    (void)feed(CoreInput{e});
    world(20);
    t += 250;
    (void)feed(in(loading(t)));  // ends it: ConversationEnded
  }
  template <class K>
  std::vector<augur::Event> of() const {
    std::vector<augur::Event> v;
    for (const augur::Event& e : events) {
      if (std::holds_alternative<K>(e.kind)) v.push_back(e);
    }
    return v;
  }
  std::optional<std::size_t> index_of(std::string_view id) const { return core.directory().find(id); }
  // Who perceived the event, by profile id, as the store will keep it.
  std::map<std::string, proto::How> heard(const augur::Event& e) const {
    std::map<std::string, proto::How> m;
    const auto rows = augur::stored_audience(e, [&](augur::PersonaId p) -> std::optional<std::string> {
      if (p.index >= core.directory().size()) return std::nullopt;
      return core.directory().persona(p.index).id;
    });
    for (const proto::AudienceRow& r : rows) m[r.persona] = r.how;
    return m;
  }
};

}  // namespace

TEST_CASE("audiences: a persona is classified, and its line is heard 16 m round, direct by the one spoken to") {
  Session s;
  s.talk();
  // The Lantern-Keeper, within 10 m, was seen, by itself alone; the pilot, 12 m off, was not.
  const auto seen = s.of<augur::PersonaSeen>();
  REQUIRE(seen.size() == 1);
  CHECK(s.heard(seen[0]) == std::map<std::string, proto::How>{{"lantern_keeper", proto::How::Direct}});
  // The Tarnished's line: direct for the Lantern-Keeper, overheard by the pilot 12 m off.
  const auto said = s.of<augur::PlayerSaid>();
  REQUIRE(said.size() == 1);
  CHECK(said[0].event_class == augur::Class::P);
  CHECK(s.heard(said[0]) ==
        std::map<std::string, proto::How>{{"lantern_keeper", proto::How::Direct}, {"kale", proto::How::Overheard}});
  // The reply: direct for its speaker, overheard by the pilot.
  const auto reply = s.of<augur::NpcSaid>();
  REQUIRE(reply.size() == 1);
  CHECK(s.heard(reply[0]) ==
        std::map<std::string, proto::How>{{"lantern_keeper", proto::How::Direct}, {"kale", proto::How::Overheard}});
  // The end: the persona only.
  const auto ended = s.of<augur::ConversationEnded>();
  REQUIRE(ended.size() == 1);
  CHECK(s.heard(ended[0]) == std::map<std::string, proto::How>{{"lantern_keeper", proto::How::Direct}});
  CHECK(s.of<augur::ConversationStarted>().size() == 1);
  // What each may know, through the one reader.
  const auto keeper = s.index_of("lantern_keeper");
  const auto kale = s.index_of("kale");
  REQUIRE(keeper);
  REQUIRE(kale);
  const augur::VisCtx ctx{1};
  CHECK(augur::visible_to(said[0], augur::PersonaId{static_cast<std::uint32_t>(*keeper)}, ctx)->how ==
        proto::How::Direct);
  CHECK(augur::visible_to(ended[0], augur::PersonaId{static_cast<std::uint32_t>(*kale)}, ctx) == std::nullopt);
}

TEST_CASE("audiences: a persona 17 m off hears nothing of the conversation, and nothing leaks to it") {
  Session s;
  s.kale_at = {0, 17, 0};  // 17 m from the Tarnished and 17.1 m from the Lantern-Keeper
  s.talk();
  const auto kale = s.index_of("kale");
  REQUIRE(kale);
  const augur::VisCtx ctx{1};
  std::size_t speech = 0;
  for (const augur::Event& e : s.events) {
    const bool line =
        std::holds_alternative<augur::PlayerSaid>(e.kind) || std::holds_alternative<augur::NpcSaid>(e.kind);
    if (line) ++speech;
    if (line || std::holds_alternative<augur::ConversationEnded>(e.kind)) {
      CHECK_FALSE(augur::visible_to(e, augur::PersonaId{static_cast<std::uint32_t>(*kale)}, ctx).has_value());
      CHECK(s.heard(e).count("kale") == 0);
    }
  }
  CHECK(speech == 2);
}

TEST_CASE("audiences: before any content load every character is Mute, and no audience has a row") {
  stubs::StubSpeech speech;
  Core core{speech.parts()};
  (void)core.step(in(data_ready()), 0);
  (void)core.step(in(playthrough_ready()), 0);
  for (Mono t = 250; t <= 3000; t += 250) {
    proto::WorldSnapshot s = test::world(t);
    s.player->handle = proto::ChrHandle{1};
    (void)s.chars.try_push_back(npc(0x500, 81100, {2, 0, 0}));
    const Outputs o = core.step(in(s), t);
    for (const augur::Event& e : o.events) {
      if (e.subject && e.subject->kind != augur::ActorRef::Kind::Player) {
        CHECK(e.subject->kind == augur::ActorRef::Kind::Mute);
      }
      CHECK(augur::audience_size(e) == 0);
    }
  }
}
