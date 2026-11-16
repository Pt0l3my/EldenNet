// The game thread's token and the backend interface (thread.hpp, backend.hpp). The token is empty, cannot be copied,
// assigned or made but by issue(), and moves; the compile-fail tests (compile_fail/game_thread.cpp, run by CTest) show
// that the forbidden uses do not build. A backend of the test's own, over invented data, is driven through the
// interface with a token, as the task and the replay harness drive theirs.
#include <doctest.h>

#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

#include "en/proto/backend.hpp"

using namespace en::proto;

static_assert(std::is_empty_v<GameThread>);
static_assert(!std::is_default_constructible_v<GameThread>);
static_assert(!std::is_copy_constructible_v<GameThread>);
static_assert(!std::is_copy_assignable_v<GameThread>);
static_assert(!std::is_move_assignable_v<GameThread>);
static_assert(std::is_nothrow_move_constructible_v<GameThread>);
static_assert(std::is_abstract_v<GameBackend>);

namespace {

// A backend over a few invented characters, which counts what it was asked.
class FakeBackend final : public GameBackend {
 public:
  Capabilities probe(const GameThread&) override {
    ++calls;
    return Capabilities{bit_of(Group::Session) | bit_of(Group::World) | bit_of(Group::Player)};
  }

  Expected<void, ReadError> snapshot(const GameThread&, WorldSnapshot& out) override {
    ++calls;
    if (inert) return Unexpected{ReadError::Inert};
    out = WorldSnapshot{};
    out.seq = ++seq;
    out.life.main_player = true;
    for (std::uint64_t i = 0; i < 3; ++i) {
      Chr c;
      c.handle = ChrHandle{0x1000 + i};
      c.dist_m = static_cast<float>(i);
      (void)out.chars.try_push_back(c);
    }
    return {};
  }

  Expected<void, ReadError> render_feed(const GameThread&, std::span<const ChrHandle> want, RenderFeed& out) override {
    ++calls;
    out = RenderFeed{};
    for (const ChrHandle h : want) {
      if (!out.heads.try_push_back(Head{h, {1, 2, 3}, h.raw >= 0x1000 && h.raw < 0x1003})) break;
    }
    return {};
  }

  Expected<void, ApplyError> apply(const GameThread&, const GameCommand& cmd) override {
    ++calls;
    if (std::holds_alternative<GameCommand::Noop>(cmd.payload)) return {};
    if (const auto* tracked = std::get_if<GameCommand::SetTracked>(&cmd.payload)) {
      tracked_count = tracked->handles.size();
      return {};
    }
    return Unexpected{ApplyError::Unsupported};
  }

  bool inert = false;
  std::uint64_t seq = 0;
  std::size_t tracked_count = 0;
  int calls = 0;
};

// Code that reads takes the token by const reference: it cannot keep a copy.
std::uint64_t read_twice(GameBackend& backend, const GameThread& gt) {
  WorldSnapshot s;
  if (!backend.snapshot(gt, s)) return 0;
  if (!backend.snapshot(gt, s)) return 0;
  return s.seq;
}

}  // namespace

TEST_CASE("only issue() makes a token, and it moves") {
  GameThread gt = GameThread::issue();
  GameThread moved = std::move(gt);
  FakeBackend backend;
  CHECK(read_twice(backend, moved) == 2);
}

TEST_CASE("a backend is read through the interface with the game thread's token") {
  const GameThread gt = GameThread::issue();
  FakeBackend fake;
  GameBackend& backend = fake;
  const Capabilities caps = backend.probe(gt);
  CHECK(has_all(caps.groups, bit_of(Group::World)));
  WorldSnapshot s;
  REQUIRE(backend.snapshot(gt, s));
  CHECK(s.chars.size() == 3);
  const ChrHandle want[] = {ChrHandle{0x1001}, ChrHandle{0x2000}};
  RenderFeed feed;
  REQUIRE(backend.render_feed(gt, want, feed));
  REQUIRE(feed.heads.size() == 2);
  CHECK(feed.heads[0].present);
  CHECK_FALSE(feed.heads[1].present);
  GameCommand track;
  GameCommand::SetTracked tracked;
  (void)tracked.handles.try_push_back(ChrHandle{0x1001});
  track.payload = tracked;
  CHECK(backend.apply(gt, track).has_value());
  CHECK(fake.tracked_count == 1);
  GameCommand text;
  text.payload = GameCommand::ResolveText{};
  const auto refused = backend.apply(gt, text);
  REQUIRE_FALSE(refused);
  CHECK(refused.error() == ApplyError::Unsupported);
  fake.inert = true;
  const auto inert = backend.snapshot(gt, s);
  REQUIRE_FALSE(inert);
  CHECK(std::string(to_string(inert.error())) == "Inert");
  CHECK(fake.calls == 6);
}
