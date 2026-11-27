// The store's messages (store.hpp): their words, which requests take the speech reserve, and ids that keep every bit.
#include <doctest.h>

#include <cstdint>
#include <limits>
#include <string>

#include "en/proto/store.hpp"

using namespace en::proto;

TEST_CASE("store messages: the audience's words read back, and nothing else reads") {
  for (const How h : {How::Direct, How::Overheard, How::Witnessed}) CHECK(how_from(to_string(h)) == h);
  CHECK_FALSE(how_from("Direct").has_value());
  CHECK_FALSE(how_from("").has_value());
  CHECK(std::string(to_string(JobState::Parked)) == "parked");
  CHECK(std::string(to_string(JobCause::None)).empty());
  CHECK(std::string(db::to_string(db::Fault::TooNew)) == "too_new");
}

TEST_CASE("store messages: speech takes the reserve, other writes and queries do not") {
  DbRequest r;
  r.op = db::AppendPage{};
  CHECK(r.speech());
  db::Append said;
  said.event.kind = "PlayerSaid";
  r.op = said;
  CHECK(r.speech());
  said.event.kind = "NpcSaid";
  r.op = said;
  CHECK(r.speech());
  said.event.kind = "PersonaAttacked";
  r.op = said;
  CHECK_FALSE(r.speech());
  r.op = db::Bundle{};
  CHECK_FALSE(r.speech());
}

TEST_CASE("store messages: event ids above 2^53 keep every bit") {
  StoredEvent e;
  e.id = (std::int64_t{1} << 53) + 1;
  const StoredEvent copy = e;
  CHECK(copy.id == (std::int64_t{1} << 53) + 1);
  e.id = std::numeric_limits<std::int64_t>::max();
  CHECK(e.id - 1 == std::numeric_limits<std::int64_t>::max() - 1);
}
