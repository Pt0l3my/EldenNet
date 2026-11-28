// MEM §2.2's fold (facts.hpp): each fact's rule, meetings merged within 10 min, world_seen keeping the higher
// certainty, and the property that the live fold and the recomputation agree (seeded random events). Invented rows.
#include <doctest.h>

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "en/core/facts.hpp"

using namespace en;
using namespace en::core;
using proto::How;

namespace {

constexpr proto::Pt kMin = 60000;

proto::StoredEvent event(std::int64_t id, std::string kind, proto::Pt t, std::string subject = "persona:kale",
                         std::string payload = "{}") {
  proto::StoredEvent e;
  e.id = id;
  e.kind = std::move(kind);
  e.t_play_ms = t;
  e.subject = std::move(subject);
  e.payload = std::move(payload);
  e.place_id = 1001;
  return e;
}

proto::StoredEvent ended(std::int64_t id, proto::Pt t, int player, int committed, std::string known = "null") {
  return event(id, "ConversationEnded", t, "persona:kale",
               R"({"reason":"closed","player_lines":)" + std::to_string(player) + R"(,"committed":)" +
                   std::to_string(committed) + R"(,"known":)" + known + R"(,"stage":"base"})");
}

std::string value(const proto::PersonaFacts& f, std::string_view key) {
  const proto::FactRow* r = find_fact(f, key);
  return r != nullptr ? r->value : std::string("<none>");
}

}  // namespace

TEST_CASE("facts: a meeting needs a line of the persona's; meetings within 10 min merge") {
  proto::PersonaFacts f;
  f.persona = "kale";
  CHECK(fold_fact(f, ended(1, 10 * kMin, 2, 2), How::Direct).empty());  // the Tarnished's lines alone
  CHECK(value(f, fact::kMetCount) == "<none>");
  CHECK_FALSE(fold_fact(f, ended(2, 11 * kMin, 1, 2), How::Direct).empty());
  CHECK(value(f, fact::kMetCount) == "1");
  CHECK(value(f, fact::kFirstMetPt) == std::to_string(11 * kMin));
  CHECK(value(f, fact::kFirstMetPlace) == "1001");
  CHECK(value(f, fact::kStageSeen) == "base");
  (void)fold_fact(f, ended(3, 20 * kMin, 1, 3), How::Direct);  // 9 min later: the same visit
  CHECK(value(f, fact::kMetCount) == "1");
  CHECK(value(f, fact::kLastMetPt) == std::to_string(20 * kMin));
  (void)fold_fact(f, ended(4, 31 * kMin, 1, 2), How::Direct);  // 11 min after the last: a new one
  CHECK(value(f, fact::kMetCount) == "2");
  CHECK(value(f, fact::kFirstMetPt) == std::to_string(11 * kMin));
  // A native talk with the persona is a meeting too; overheard, an end is nobody's meeting.
  (void)fold_fact(f, event(5, "NativeTalkEnded", 60 * kMin, "persona:kale", R"({"held_ms":900})"), How::Direct);
  CHECK(value(f, fact::kMetCount) == "3");
  CHECK(fold_fact(f, ended(6, 90 * kMin, 1, 2), How::Overheard).empty());
}

TEST_CASE("facts: seen, attacked, fought beside, the Tarnished's falls, and another persona's events") {
  proto::PersonaFacts f;
  f.persona = "kale";
  (void)fold_fact(f, event(1, "PersonaSeen", kMin), How::Direct);
  (void)fold_fact(f, event(2, "PersonaSeen", 2 * kMin), How::Direct);
  CHECK(value(f, fact::kSeenCount) == "2");
  proto::StoredEvent hit = event(3, "PersonaAttacked", 3 * kMin, "persona:kale", R"({"via":"HpDrop"})");
  hit.object = "player";
  (void)fold_fact(f, hit, How::Direct);
  CHECK(value(f, fact::kTimesAttacked) == "1");
  CHECK(value(f, fact::kLastAttackedPt) == std::to_string(3 * kMin));
  CHECK(fold_fact(f, hit, How::Witnessed).empty());  // a witness was not struck
  (void)fold_fact(f, event(4, "FoughtBeside", 4 * kMin, "persona:kale", R"({"boss":904})"), How::Direct);
  CHECK(value(f, fact::kFoughtBeside) == "1");
  CHECK(value(f, fact::kFoughtLastBoss) == "904");
  (void)fold_fact(f, event(5, "PlayerDied", 5 * kMin, "player", R"({"killer":null,"in_boss":false})"), How::Witnessed);
  CHECK(value(f, fact::kSeenPlayerDie) == "1");
  // Another persona's seeing is not this one's.
  CHECK(fold_fact(f, event(6, "PersonaSeen", 6 * kMin, "persona:wren"), How::Direct).empty());
  // Each fact's row names the event that last moved it.
  CHECK(find_fact(f, fact::kSeenPlayerDie)->src_event == 5);
}

TEST_CASE("facts: world_seen is the union of the meetings' known, keeping the higher certainty; null changes nothing") {
  proto::PersonaFacts f;
  f.persona = "kale";
  (void)fold_fact(f, ended(1, 10 * kMin, 1, 2, R"([["star_fell","rumour"],["howl","belief"]])"), How::Direct);
  CHECK(value(f, fact::kWorldSeen) == R"([["howl","belief"],["star_fell","rumour"]])");
  CHECK(value(f, fact::kWorldSeenPt) == std::to_string(10 * kMin));
  (void)fold_fact(f, ended(2, 40 * kMin, 1, 2, R"([["star_fell","fact"],["howl","rumour"]])"), How::Direct);
  CHECK(value(f, fact::kWorldSeen) == R"([["howl","rumour"],["star_fell","fact"]])");
  (void)fold_fact(f, ended(3, 80 * kMin, 1, 2, R"([["star_fell","belief"]])"), How::Direct);  // never lower
  CHECK(value(f, fact::kWorldSeen) == R"([["howl","rumour"],["star_fell","fact"]])");
  CHECK(value(f, fact::kWorldSeenPt) == std::to_string(80 * kMin));
  (void)fold_fact(f, ended(4, 120 * kMin, 1, 2), How::Direct);  // known null: a meeting, nothing seen
  CHECK(value(f, fact::kWorldSeenPt) == std::to_string(80 * kMin));
  CHECK(value(f, fact::kMetCount) == "4");
}

TEST_CASE("facts: a cue moves world_cued for its item") {
  proto::PersonaFacts f;
  f.persona = "kale";
  (void)fold_fact(f, event(1, "NpcSaid", 5 * kMin, "persona:kale", R"({"cue":"star_fell"})"), How::Direct);
  (void)fold_fact(f, event(2, "NpcSaid", 7 * kMin, "persona:kale", R"({"cue":null})"), How::Direct);
  CHECK(value(f, fact::kWorldCued) == R"([["star_fell",300000]])");
  (void)fold_fact(f, event(3, "NpcSaid", 9 * kMin, "persona:kale", R"({"cue":"star_fell"})"), How::Direct);
  CHECK(fact_pairs(f, fact::kWorldCued).front().second == std::to_string(9 * kMin));
}

TEST_CASE("facts: the live fold and the recomputation agree (seeded random events, every kind and how)") {
  for (std::uint32_t seed = 1; seed <= 20; ++seed) {
    std::mt19937 rng(seed);
    const std::vector<std::string> personas = {"kale", "wren", "lantern_keeper"};
    const std::vector<std::string> kinds = {"ConversationEnded", "NativeTalkEnded", "PersonaSeen", "PersonaAttacked",
                                            "FoughtBeside",      "PlayerDied",      "NpcSaid",     "PlaceEntered"};
    const std::vector<How> hows = {How::Direct, How::Overheard, How::Witnessed};
    std::vector<proto::StoredEvent> events;
    std::map<std::string, proto::PersonaFacts> live;
    proto::Pt t = 0;
    for (std::int64_t id = (std::int64_t{1} << 33) + 1; events.size() < 200; ++id) {
      t += static_cast<proto::Pt>(rng() % (15 * kMin));
      const std::string& who = personas[rng() % personas.size()];
      const std::string& kind = kinds[rng() % kinds.size()];
      std::string payload = "{}";
      if (kind == "ConversationEnded") {
        payload = R"({"reason":"closed","player_lines":1,"committed":)" + std::to_string(1 + rng() % 3) +
                  R"(,"known":[["item)" + std::to_string(rng() % 4) + R"(",")" + (rng() % 2 ? "fact" : "rumour") +
                  R"("]],"stage":"s)" + std::to_string(rng() % 3) + "\"}";
      } else if (kind == "FoughtBeside") {
        payload = R"({"boss":)" + std::to_string(rng() % 5) + "}";
      } else if (kind == "NpcSaid") {
        payload = rng() % 2 ? R"({"cue":"item1"})" : R"({"cue":null})";
      }
      proto::StoredEvent e = event(id, kind, t, kind == "PlayerDied" ? "player" : "persona:" + who, payload);
      if (kind == "PersonaAttacked") e.object = "player";
      for (const std::string& p : personas) {
        if (rng() % 3 == 0) continue;
        const How how = hows[rng() % hows.size()];
        e.audience.push_back(proto::AudienceRow{p, how});
        proto::PersonaFacts& f = live[p];
        f.persona = p;
        (void)fold_fact(f, e, how);
      }
      events.push_back(std::move(e));
    }
    std::vector<proto::StoredEvent> shuffled = events;  // the recomputation folds in id order, whatever it is given
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    const std::vector<proto::PersonaFacts> again = fold_all(shuffled);
    std::size_t with_facts = 0;
    for (const auto& [p, f] : live) {
      if (f.facts.empty()) continue;
      ++with_facts;
      const auto it =
          std::find_if(again.begin(), again.end(), [&](const proto::PersonaFacts& g) { return g.persona == p; });
      REQUIRE(it != again.end());
      CHECK(*it == f);
    }
    CHECK(again.size() == with_facts);
  }
}
