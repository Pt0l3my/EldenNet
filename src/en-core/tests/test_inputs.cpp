// The core's widened inputs and outputs (part 0 of the first conversation, WHY§364): the surfaces' events, the model
// client's events and its health are accepted, and until the conversation is wired in each is a tick, so v0.1's
// behaviour is unchanged; the outputs' UI model is a closed box and no request goes out. All data is invented.
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
  list.push_back(CoreInput{proto::NetHealth{true}});
  list.push_back(CoreInput{proto::NetHealth{false}});
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
