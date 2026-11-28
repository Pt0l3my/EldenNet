// The lived layer (lived.hpp; MEM §13.2, §13.4 to §13.6, §13.9): the due count, the record's form, the prompt on the
// shipped templates, and each rule of MEM §13.5 with a fixture that just fails it: a self change on 2 memories, a
// manner from 2 meetings, an entry citing an unknown keepsake, a learned line on a claim alone, a warm feeling towards
// the Tarnished at disposition −1; and block 8's lines. Invented data throughout.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/core/core.hpp"
#include "en/core/lived.hpp"

using namespace en;
using namespace en::core;
using nlohmann::json;

namespace {

constexpr proto::Pt kHour = 3600000;

Keepsake keep(std::string id, Keepsake::Basis basis, double weight, proto::Pt since, std::string text = "x.") {
  Keepsake k;
  k.id = std::move(id);
  k.text = std::move(text);
  k.basis = basis;
  k.weight = weight;
  k.since_pt = since;
  k.updated_pt = since;
  return k;
}

LivedJobInput input(std::int8_t disposition = -1) {
  LivedJobInput in;
  in.persona = "kale";
  in.name = "Kalé";
  in.journal.version = 3;
  in.journal.disposition = disposition;
  in.journal.as_of_pt = 3 * kHour;
  in.journal.keepsakes = {keep("k1", Keepsake::Basis::Said, 0.6, 1 * kHour, "They mean to take the castle."),
                          keep("k2", Keepsake::Basis::Said, 0.6, 2 * kHour, "They claim to have killed its lord."),
                          keep("k3", Keepsake::Basis::Seen, 0.75, 3 * kHour, "They struck me in my own shop."),
                          keep("k4", Keepsake::Basis::Seen, 0.8, 4 * kHour, "They came back to pay for it."),
                          keep("k5", Keepsake::Basis::Seen, 0.9, 5 * kHour, "They warned me of the soldiers.")};
  in.journal.keepsakes[1].check_key = "slew:godrick";
  in.seeks = {"Make a living without dying for it"};
  in.people = {{"P1", "the Tarnished"}};
  return in;
}

std::string reply(json towards = json::array(), json aims = json::array(), json learned = json::array(),
                  json self = nullptr, json manner = nullptr) {
  return json{{"towards", towards}, {"aims", aims}, {"learned", learned}, {"self", self}, {"manner", manner}}.dump();
}

json toward(std::string feeling, json refs, std::string text = "The Tarnished is quick to strike.") {
  return {{"id", nullptr},
          {"who", "P1"},
          {"feeling", std::move(feeling)},
          {"trust_delta", 0},
          {"text", std::move(text)},
          {"refs", std::move(refs)}};
}

}  // namespace

TEST_CASE("lived: due after 3 new or rewritten keepsakes, a new protected keepsake or a moved feeling") {
  LivedDue due;
  Journal before;
  Journal after;
  after.keepsakes = {keep("k1", Keepsake::Basis::Said, 0.5, 0), keep("k2", Keepsake::Basis::Said, 0.5, 0)};
  count_journal_change(due, before, after);
  CHECK(due.changed == 2);
  CHECK_FALSE(due.due());
  Journal third = after;
  third.keepsakes.push_back(keep("k3", Keepsake::Basis::Said, 0.5, 0));
  count_journal_change(due, after, third);
  CHECK(due.due());  // 3 since
  LivedDue moved;
  Journal warmer = third;
  warmer.disposition = 1;
  count_journal_change(moved, third, warmer);
  CHECK(moved.disposition_moved);
  CHECK(moved.due());
}

TEST_CASE("lived: the record round-trips through the store's form; the prompt renders on the shipped templates") {
  Lived l;
  l.version = 2;
  l.towards = {LivedEntry{"t1", "tarnished", "wary", -1, {}, "The Tarnished is quick to strike.", {"k3"}}};
  l.self = LivedEntry{"s", {}, {}, 0, {}, "I trust my own eyes more than words.", {"k3", "k4", "k5"}};
  const auto back = lived_of(lived_version(l, 1, "kale"));
  REQUIRE(back.has_value());
  CHECK(*back == l);
  auto pack = load_content(builtin_content());
  REQUIRE(pack.has_value());
  LivedJobInput in = input();
  in.lived = l;
  const auto m = lived_messages(pack->others, in);
  REQUIRE(m.has_value());
  std::string all;
  for (const proto::Message& x : *m) all += x.joined() + "\n";
  CHECK(all.find("You keep a record of how things have turned for Kalé") != std::string::npos);
  CHECK(all.find("k3 [0.75, seen, with P1] \"They struck me in my own shop.\"") != std::string::npos);
  CHECK(all.find("k5 [0.90, seen, with P1, keep]") != std::string::npos);  // the protected one
  CHECK(all.find("t1 [towards P1, wary; from k3] \"The Tarnished is quick to strike.\"") != std::string::npos);
  CHECK(all.find("P1 the Tarnished") != std::string::npos);
  CHECK(lived_schema()["required"].size() == 5);
}

TEST_CASE("lived: MEM §13.5's rules, each with a fixture that just fails it") {
  LivedCheck check;
  check.now = 30 * kHour;
  LivedJobInput in = input(-1);
  for (Keepsake& k : in.journal.keepsakes) k.about = "tarnished";
  // A feeling that keeps to the Tarnished's trust (−1) passes; a warm one is dropped.
  auto ok = validate_lived(reply(json::array({toward("wary", {"k3"})})), "stop", in, check);
  REQUIRE(ok.has_value());
  REQUIRE(ok->towards.size() == 1);
  CHECK(ok->towards[0].who == "tarnished");
  auto warm = validate_lived(reply(json::array({toward("fond", {"k3"})})), "stop", in, check);
  REQUIRE(warm.has_value());
  CHECK(warm->towards.empty());
  // An entry citing only an unknown keepsake is dropped.
  auto unknown = validate_lived(reply(json::array({toward("wary", {"k9"})})), "stop", in, check);
  REQUIRE(unknown.has_value());
  CHECK(unknown->towards.empty());
  // A learned line on a claim alone teaches nothing; with a seen memory it holds (2 cited).
  const json claim_only = {{"id", nullptr}, {"text", "Big talk is cheap."}, {"refs", {"k1", "k2"}}};
  CHECK(validate_lived(reply(json::array(), json::array(), json::array({claim_only})), "stop", in, check)
            ->learned.empty());
  const json grounded = {{"id", nullptr}, {"text", "A blow can be paid for."}, {"refs", {"k3", "k4"}}};
  CHECK(
      validate_lived(reply(json::array(), json::array(), json::array({grounded})), "stop", in, check)->learned.size() ==
      1);
  // self, the rare tier: 3 memories of weight 0.7 or more, from 2 scenes; on 2 it is dropped.
  const json self2 = {{"text", "I trust my own eyes."}, {"refs", {"k3", "k4"}}};
  CHECK_FALSE(validate_lived(reply(json::array(), json::array(), json::array(), self2), "stop", in, check)->self);
  const json self3 = {{"text", "I trust my own eyes."}, {"refs", {"k3", "k4", "k5"}}};
  CHECK(validate_lived(reply(json::array(), json::array(), json::array(), self3), "stop", in, check)->self);
  // manner, the sustained pattern: 3 memories with the Tarnished from 3 meetings; from 2 it is dropped.
  const json manner2 = {{"text", "I keep my words short with them."}, {"refs", {"k3", "k4"}}};
  CHECK_FALSE(
      validate_lived(reply(json::array(), json::array(), json::array(), nullptr, manner2), "stop", in, check)->manner);
  const json manner3 = {{"text", "I keep my words short with them."}, {"refs", {"k3", "k4", "k5"}}};
  CHECK(
      validate_lived(reply(json::array(), json::array(), json::array(), nullptr, manner3), "stop", in, check)->manner);
  // A gendered pronoun about the Tarnished drops the entry; a reply that is no JSON is a failure.
  CHECK(validate_lived(reply(json::array({toward("wary", {"k3"}, "He is quick to strike.")})), "stop", in, check)
            ->towards.empty());
  CHECK(validate_lived("no", "stop", in, check).error() == JobFailure::Parse);
}

TEST_CASE("lived: block 8's lines, the Tarnished's first, within 100 tokens") {
  Lived l;
  l.towards = {LivedEntry{"t1", "tarnished", "wary", -1, {}, "The Tarnished is quick to strike.", {"k3"}}};
  l.learned = {LivedEntry{"l1", {}, {}, 0, {}, "A blow can be paid for.", {"k3", "k4"}}};
  const std::vector<std::string> lines = lived_lines(l);
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "Of the Tarnished: \"The Tarnished is quick to strike.\"");
  for (int i = 0; i < 20; ++i) {
    l.aims.push_back(LivedEntry{"a" + std::to_string(i), {}, {}, 0, {}, std::string(60, 'a'), {"k1"}});
  }
  std::size_t bytes = 0;
  for (const std::string& s : lived_lines(l)) bytes += s.size() + 1;
  CHECK(estimate_tokens(bytes) <= kLivedTokens);
}
