#pragma once
// A conversation on a simulated clock, for the conversation's tests: the stubs' speech (stubs.hpp), the shipped test
// persona bound to an invented character by the tests' overrides, and an invented second persona; a world of one
// snapshot the tests change, stepped as the core steps it (a snapshot every 250 ms, ticks between). All data is
// invented.
#include <doctest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "en/core/conversation.hpp"
#include "en/core/stubs.hpp"
#include "../test_files.hpp"

namespace en::core::test {

inline constexpr proto::ChrHandle kPlayer{1};
inline constexpr proto::ChrHandle kKeeper{0x500};  // the test persona, row 81100
inline constexpr proto::ChrHandle kWren{0x600};    // an invented second persona, row 4100
inline constexpr proto::ChrHandle kWolf{0x700};    // an invented hostile

inline constexpr const char* kWrenRecord = R"toml(id = "wren"
schema = 1
display_name_fallback = "Wren"
register = "colloquial"
pronouns = "she"
deflections = ["Not today.", "Ask the wind.", "That is mine to keep."]
[[stages]]
id = "base"
when = "true"
road = "You mend nets by the ford."
at = "limgrave"
state = "awake"
)toml";

inline std::vector<ContentFile> rig_content(const std::string& wren = kWrenRecord) {
  std::vector<ContentFile> files = builtin_content();
  files.push_back(ContentFile{"identity/personas.toml", R"toml(schema = 1
[[persona]]
id = "lantern_keeper"
file = "personas/lantern_keeper.toml"
npc_params = [ { param = 81100, form = "default", verified = true } ]
test_only = true
[[persona]]
id = "wren"
file = "personas/wren.toml"
npc_params = [ { param = 4100, form = "default", verified = true } ]
)toml",
                              ContentOrigin::Override});
  files.push_back(ContentFile{"personas/wren.toml", wren, ContentOrigin::Override});
  return files;
}

inline proto::Chr character(proto::ChrHandle h, std::uint32_t param, float dist, float bearing = 0.0f) {
  proto::Chr c;
  c.handle = h;
  c.param = param;
  c.dist_m = dist;
  c.bearing_deg = bearing;
  c.hp = proto::Meter{100, 100};
  c.on_screen = true;
  return c;
}

class Rig {
 public:
  explicit Rig(const std::string& wren = kWrenRecord) {
    auto pack = load_content(rig_content(wren));
    REQUIRE(pack.has_value());
    cast_ = Directory::build(std::move(*pack), Realm::Eval, predicates_);
    snap.mono_ms = 0;
    for (auto& g : snap.health.groups) g.state = proto::GroupState::Ok;
    snap.life.main_player = true;
    snap.life.screen = proto::Screen::InGame;
    snap.ui.hud = proto::HudState::Default;
    snap.ui.subtitles_on = true;
    proto::Player p;
    p.handle = kPlayer;
    p.hp = proto::Meter{500, 500};
    snap.player = p;
    snap.clock = proto::Clock{18, 30};
    keeper(2.5f, 5.0f);
  }

  // The keeper at a distance and a bearing (it is put back if it was taken away).
  proto::Chr& keeper(float dist, float bearing = 5.0f) {
    for (proto::Chr& c : snap.chars) {
      if (c.handle == kKeeper) {
        c.dist_m = dist;
        c.bearing_deg = bearing;
        return c;
      }
    }
    (void)snap.chars.try_push_back(character(kKeeper, 81100, dist, bearing));
    return snap.chars[snap.chars.size() - 1];
  }
  proto::Chr* find(proto::ChrHandle h) {
    for (proto::Chr& c : snap.chars) {
      if (c.handle == h) return &c;
    }
    return nullptr;
  }
  void remove(proto::ChrHandle h) {
    decltype(snap.chars) kept;
    for (const proto::Chr& c : snap.chars) {
      if (c.handle != h) (void)kept.try_push_back(c);
    }
    snap.chars = kept;
  }
  void add(const proto::Chr& c) { (void)snap.chars.try_push_back(c); }

  ConversationWorld world(bool fresh) {
    ConversationWorld w;
    w.snapshot = &snap;
    w.fresh = fresh;
    w.in_world = in_world;
    w.loading = loading;
    w.dormant = dormant;
    w.speech = speech;
    w.combat = combat;
    w.player_died = died;
    w.cast = &cast_;
    w.predicates = &predicates_;
    w.facts = facts;
    w.hostile_teams = hostile_;
    w.log = &log;
    return w;
  }

  // One step at the rig's time: a snapshot (fresh), or the event given.
  const ConversationOutput& step(const proto::UiEvent* ui = nullptr, const proto::LlmEvent* llm = nullptr,
                                 bool fresh = false) {
    snap.mono_ms = now;
    ConversationInput in;
    in.now = now;
    in.world = world(fresh);
    in.ui = ui;
    in.llm = llm;
    last = conv.step(in);
    for (const Commit& c : last.commits) {
      log.apply(c);
      commits.push_back(c);
    }
    for (const auto& r : last.requests) requests.push_back(r);
    for (const auto id : last.cancels) cancels.push_back(id);
    if (last.ended) ended.push_back(*last.ended);
    for (const proto::Toast& t : last.ui.toasts) {
      if (std::find(toasts.begin(), toasts.end(), t.text) == toasts.end()) toasts.push_back(t.text);
    }
    if (auto_ack && last.ui.page && last.ui.page->kind == proto::PageKind::Speech && !(acked && *acked == last.ui.page->id)) {
      const proto::UiEvent shown{proto::UiEvent::PageShown{last.ui.page->id, now}};
      acked = last.ui.page->id;
      return step(&shown);
    }
    return last;
  }
  const ConversationOutput& snapshot() { return step(nullptr, nullptr, true); }
  // Time passes: a snapshot every 250 ms, a tick every 50 ms between.
  void wait(Mono ms) {
    const Mono until = now + ms;
    while (now < until) {
      now = std::min<Mono>(until, now + 50);
      if (now % 250 == 0) {
        snapshot();
      } else {
        step();
      }
    }
  }
  const ConversationOutput& ui(const proto::UiEvent& e) { return step(&e); }
  const ConversationOutput& open() { return ui(proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}); }
  const ConversationOutput& say(const std::string& text) { return ui(proto::UiEvent{proto::UiEvent::Submit{text}}); }
  const ConversationOutput& close(proto::CloseReason why, const std::string& draft = "") {
    return ui(proto::UiEvent{proto::UiEvent::Close{why, draft, ""}});
  }
  const ConversationOutput& shown(proto::PageId id) { return ui(proto::UiEvent{proto::UiEvent::PageShown{id, now}}); }
  const ConversationOutput& skip(proto::PageId id) { return ui(proto::UiEvent{proto::UiEvent::Skip{id}}); }

  const ConversationOutput& llm(proto::EventKind kind, std::string text = "",
                                proto::ErrorClass error = proto::ErrorClass::None) {
    proto::LlmEvent e;
    e.kind = kind;
    e.id = requests.empty() ? 0 : requests.back().id;
    e.text = std::move(text);
    if (kind == proto::EventKind::Ended) {
      proto::ChatResult r;
      r.error = error;
      e.chat = r;
    }
    return step(nullptr, &e);
  }
  const ConversationOutput& delta(const std::string& text) { return llm(proto::EventKind::Delta, text); }
  const ConversationOutput& finish(proto::ErrorClass error = proto::ErrorClass::None) {
    return llm(proto::EventKind::Ended, "", error);
  }

  // Warms the observer (3 s free of native talk, 3 s since the keeper came within reach) and opens the box.
  void open_box() {
    wait(3250);
    open();
    REQUIRE(conv.box_open());
  }

  [[nodiscard]] std::vector<Commit> of(Commit::Kind k) const {
    std::vector<Commit> out;
    for (const Commit& c : commits) {
      if (c.kind == k) out.push_back(c);
    }
    return out;
  }
  [[nodiscard]] std::optional<proto::PageView> page() const { return last.ui.page; }
  [[nodiscard]] bool toasted(const std::string& text) const {
    return std::find(toasts.begin(), toasts.end(), text) != toasts.end();
  }

  stubs::StubSpeech stub;
  Conversation conv{stub.parts()};
  MemoryLog log;
  proto::WorldSnapshot snap;
  Mono now = 0;
  bool in_world = true, loading = false, dormant = false, speech = true, combat = false, died = false;
  PredicateFacts facts;
  bool auto_ack = true;  // the fake surface acknowledges each speech page on the step it appears
  std::optional<proto::PageId> acked;

  ConversationOutput last;
  std::vector<Commit> commits;
  std::vector<proto::LlmRequest> requests;
  std::vector<proto::RequestId> cancels;
  std::vector<EndReason> ended;
  std::vector<std::string> toasts;  // every text that has been up

 private:
  Predicates predicates_;
  Directory cast_;
  std::vector<std::uint8_t> hostile_{6, 7, 27, 29};
};

}  // namespace en::core::test
