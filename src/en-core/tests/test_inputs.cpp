// The core's widened inputs and outputs (the first conversation, WHY§364): the surfaces' events and the model client's
// events are accepted, and with no content loaded (no persona) each is a tick, so v0.1's behaviour is unchanged and the
// outputs' UI model is a closed box with no request; the model client's health moves the ladder to L3 and L4 (ARCH
// §6.8; part B), and nothing else. All data is invented.
#include <doctest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "en/core/core.hpp"
#include "snapshots.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::test;

namespace {

std::vector<std::string> kinds_of(const Outputs& o) {
  std::vector<std::string> k;
  for (const augur::Event& e : o.events) k.emplace_back(augur::kind_name(e.kind));
  return k;
}

// The new inputs, one of each kind and each event kind.
std::vector<CoreInput> new_inputs() {
  std::vector<CoreInput> list;
  list.push_back(CoreInput{proto::UiEvent{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}}});
  list.push_back(CoreInput{proto::UiEvent{proto::UiEvent::Submit{"Where does the road go?"}}});
  list.push_back(CoreInput{proto::UiEvent{proto::UiEvent::PageShown{proto::PageId{1, 1, 0}, 5000}}});
  list.push_back(CoreInput{proto::UiEvent{proto::UiEvent::Skip{proto::PageId{1, 1, 0}}}});
  list.push_back(CoreInput{proto::UiEvent{proto::UiEvent::Close{proto::CloseReason::Escape, "half a line", ""}}});
  proto::LlmEvent delta;
  delta.kind = proto::EventKind::Delta;
  delta.id = 9;
  delta.text = "Down, mostly.";
  list.push_back(CoreInput{delta});
  proto::LlmEvent ended;
  ended.kind = proto::EventKind::Ended;
  ended.id = 9;
  ended.chat = proto::ChatResult{};
  list.push_back(CoreInput{ended});
  list.push_back(CoreInput{proto::NetHealth{false}});  // an unhealthy model leaves the ladder at L2
  return list;
}

void check_same(const Outputs& a, const Outputs& b) {
  CHECK(a.state == b.state);
  CHECK(a.changes == b.changes);
  CHECK(a.game.size() == b.game.size());
  CHECK(a.ai_allowed == b.ai_allowed);
  CHECK(kinds_of(a) == kinds_of(b));
}

void check_quiet(const Outputs& o) {
  CHECK(o.ui == proto::UiModel{});
  CHECK(o.ui.chat.view == proto::BoxView::Closed);
  CHECK(o.requests.empty());
  CHECK(o.cancels.empty());
}

}  // namespace

TEST_CASE("the new inputs are each a tick: a session with them in place of ticks runs exactly as with ticks") {
  Core ticked;
  Core fed;
  const std::vector<CoreInput> kinds = new_inputs();
  std::size_t next = 0;
  Mono t = 0;
  // Each snapshot is followed by a tick in one core and by the next of the new inputs in the other, at the same time.
  const auto both = [&](const proto::WorldSnapshot& s) {
    t = s.mono_ms;
    check_same(ticked.step(in(s), t), fed.step(in(s), t));
    t += 100;
    const Outputs a = ticked.step(CoreInput{CoreInput::Tick{}}, t);
    std::vector<CoreInput> again = new_inputs();  // a fresh copy: a step takes its input by value
    const Outputs b = fed.step(std::move(again[next % again.size()]), t);
    ++next;
    check_same(a, b);
    check_quiet(a);
    check_quiet(b);
  };
  check_same(ticked.step(in(data_ready()), 0), fed.step(in(data_ready()), 0));
  for (int i = 0; i < 24; ++i) both(title(t + 250));  // the title's gate opens after its 5 s
  CHECK(fed.state().gate_open);
  check_same(ticked.step(in(playthrough_ready()), t), fed.step(in(playthrough_ready()), t));
  for (int i = 0; i < 24; ++i) both(world(t + 250));  // into the world, and on
  CHECK(fed.state().phase == Phase::InWorld);
  CHECK(fed.state() == ticked.state());
  CHECK(next >= kinds.size());  // every kind went in at least once
}

TEST_CASE("before the first snapshot and after the last, a new input changes nothing and asks for nothing") {
  Core core;
  for (CoreInput& input : new_inputs()) {
    const State before = core.state();
    const Outputs o = core.step(std::move(input), 1000);
    CHECK(o.state == before);
    CHECK(o.changes.empty());
    CHECK(o.game.empty());
    CHECK_FALSE(o.ai_allowed);
    check_quiet(o);
  }
}

TEST_CASE("the model client's health takes the ladder from L2 to L4 and back, to L3 with an optional group tripped") {
  Core core;
  Mono t = 0;
  (void)core.step(in(data_ready()), t);
  CHECK(core.state().level == Level::L2);
  Outputs o = core.step(CoreInput{proto::NetHealth{true}}, t += 100);
  CHECK(core.state().level == Level::L4);
  REQUIRE(o.changes.size() == 1);
  CHECK(o.changes[0].what == Change::What::Level);
  CHECK(o.changes[0].from == static_cast<std::uint8_t>(Level::L2));
  CHECK(o.changes[0].to == static_cast<std::uint8_t>(Level::L4));
  CHECK(std::string(to_string(Level::L4)) == "L4");
  (void)core.step(CoreInput{proto::NetHealth{false}}, t += 100);
  CHECK(core.state().level == Level::L2);
  (void)core.step(in(tripped(proto::Group::Camera)), t += 100);  // optional: L3 at most from now on
  CHECK(core.state().level == Level::L2);
  (void)core.step(CoreInput{proto::NetHealth{true}}, t += 100);
  CHECK(core.state().level == Level::L3);
  CHECK(std::string(to_string(Level::L3)) == "L3");
  // A structural trip is L1 for the session, whatever the model's health.
  (void)core.step(in(tripped(proto::Group::World)), t += 100);
  CHECK(core.state().level == Level::L1);
  (void)core.step(CoreInput{proto::NetHealth{true}}, t += 100);
  CHECK(core.state().level == Level::L1);
}

TEST_CASE("before T1 is ready the model client's health changes nothing") {
  Core core;
  const Outputs o = core.step(CoreInput{proto::NetHealth{true}}, 100);
  CHECK(core.state().level == Level::Starting);
  CHECK(o.changes.empty());
  CHECK_FALSE(o.ai_allowed);
}
