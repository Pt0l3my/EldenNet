// The store's spill ring (spill.hpp) against a C8 of the test's own: order kept, nothing jumping the ring, the
// speech reserve, events paused past the non-speech part with the toast's state, resumed only once the ring is empty
// and C8 below 512, and speech never lost.
#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "en/shell/spill.hpp"

using namespace en::shell;
namespace db = en::proto::db;

namespace {

struct FakeC8 {
  std::size_t capacity = 0;
  std::vector<std::uint64_t> taken;  // the sequence numbers, in order
  bool full() const { return taken.size() >= capacity; }
  SpillRing::Send send() {
    return [this](en::proto::DbRequest& r) {
      if (full()) return false;
      taken.push_back(r.seq);
      return true;
    };
  }
};

en::proto::DbRequest event(std::uint64_t seq) {
  db::Append a;
  a.event.kind = "PersonaSeen";
  return en::proto::DbRequest{seq, a};
}
en::proto::DbRequest speech(std::uint64_t seq) {
  db::Append a;
  a.event.kind = "PlayerSaid";
  return en::proto::DbRequest{seq, a};
}
en::proto::DbRequest page(std::uint64_t seq) { return en::proto::DbRequest{seq, db::AppendPage{1, "", 0}}; }
en::proto::DbRequest conv(std::uint64_t seq) { return en::proto::DbRequest{seq, db::ConvOpen{}}; }

}  // namespace

TEST_CASE("spill: what C8 takes goes at once; once one spills, every later one queues behind it, in order") {
  FakeC8 c8{2, {}};
  SpillRing ring({8, 2, 3});
  CHECK(ring.push(event(1), c8.send()) == SpillRing::Push::Sent);
  CHECK(ring.push(conv(2), c8.send()) == SpillRing::Push::Sent);
  CHECK(ring.push(event(3), c8.send()) == SpillRing::Push::Spilled);
  c8.capacity = 10;
  CHECK(ring.push(speech(4), c8.send()) == SpillRing::Push::Spilled);  // C8 has room, but the ring is not empty
  CHECK(ring.spilled() == 2);
  ring.drain(c8.send(), c8.taken.size());
  CHECK(ring.spilled() == 0);
  CHECK(c8.taken == std::vector<std::uint64_t>{1, 2, 3, 4});
}

TEST_CASE("spill: past the non-speech part events are not saved, and the toast's state stays until C8 drains") {
  FakeC8 c8{0, {}};
  SpillRing ring({8, 2, 3});
  for (std::uint64_t i = 1; i <= 5; ++i) CHECK(ring.push(event(i), c8.send()) == SpillRing::Push::Spilled);
  CHECK(ring.push(conv(6), c8.send()) == SpillRing::Push::Spilled);  // the store's own request: kept
  CHECK_FALSE(ring.behind());
  CHECK(ring.push(event(7), c8.send()) == SpillRing::Push::NotSaved);  // 6 of 8, 2 reserved
  CHECK(ring.behind());
  // Speech takes the reserve, and is kept past a full ring; the chat holds its pages while the ring is full.
  CHECK(ring.push(speech(8), c8.send()) == SpillRing::Push::Spilled);
  CHECK(ring.push(page(9), c8.send()) == SpillRing::Push::Spilled);
  CHECK(ring.pages_held());
  CHECK(ring.push(page(10), c8.send()) == SpillRing::Push::Spilled);
  CHECK(ring.push(event(11), c8.send()) == SpillRing::Push::NotSaved);  // paused: not saved, counted
  CHECK(ring.not_saved() == 2);
  // C8 takes part: still behind while the ring holds anything.
  c8.capacity = 4;
  ring.drain(c8.send(), 0);
  CHECK(ring.behind());
  CHECK_FALSE(ring.pages_held());
  CHECK(ring.push(event(12), c8.send()) == SpillRing::Push::NotSaved);
  // All of it taken, but C8 holds 9, not below 3: still behind; then the store drains C8.
  c8.capacity = 100;
  ring.drain(c8.send(), 4);
  CHECK(ring.spilled() == 0);
  CHECK(ring.behind());
  ring.drain(c8.send(), 2);
  CHECK_FALSE(ring.behind());
  CHECK(c8.taken == std::vector<std::uint64_t>{1, 2, 3, 4, 5, 6, 8, 9, 10});
  CHECK(ring.push(event(13), c8.send()) == SpillRing::Push::Sent);
  CHECK(ring.not_saved() == 3);
}

TEST_CASE("spill: only an event's append may go unsaved") {
  CHECK(SpillRing::droppable(event(1)));
  CHECK_FALSE(SpillRing::droppable(speech(1)));
  CHECK_FALSE(SpillRing::droppable(page(1)));
  CHECK_FALSE(SpillRing::droppable(conv(1)));
  CHECK_FALSE(SpillRing::droppable(en::proto::DbRequest{1, db::Close{}}));
}

TEST_CASE("spill: the default limits are MEM 8.6's") {
  const SpillRing::Limits l;
  CHECK(l.ring == 4096);
  CHECK(l.speech_reserve == 256);
  CHECK(l.resume_below == 512);
}
