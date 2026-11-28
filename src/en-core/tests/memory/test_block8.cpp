// Block 8 and block 6 (a) from memory (memory.hpp; MEM §2.2, §2.3, §2.4, §4.2 to §4.4): the journal's form and its
// protected keepsake, MEM §4.3's example rendered on the shipped templates word for word, the facts' words, the first
// meeting's single line, the cap's trim order, block 6 (a)'s header when things have moved on, and T5: blocks 1 to 5
// byte for byte the same with block 8 or without. Invented data throughout.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/core/content.hpp"
#include "en/core/core.hpp"  // builtin_content
#include "en/core/dialogue_prompt.hpp"
#include "en/core/facts.hpp"
#include "en/core/memory.hpp"
#include "en/core/speech_content.hpp"

using namespace en;
using namespace en::core;

namespace {

constexpr proto::Pt kMin = 60000;

proto::PersonaFacts facts(std::vector<std::pair<std::string, std::string>> kv) {
  proto::PersonaFacts f;
  f.persona = "kale";
  std::sort(kv.begin(), kv.end());
  for (auto& [k, v] : kv) f.facts.push_back(proto::FactRow{k, v, 1, 0});
  return f;
}

Keepsake keep(std::string id, std::string text, Keepsake::Basis basis, double weight, proto::Pt updated,
              std::optional<std::string> check = std::nullopt) {
  Keepsake k;
  k.id = std::move(id);
  k.text = std::move(text);
  k.basis = basis;
  k.weight = weight;
  k.check_key = std::move(check);
  k.since_pt = updated;
  k.updated_pt = updated;
  return k;
}

// MEM §4.3's example: Wren, three meetings, one blow a moment ago, wary, three keepsakes in their three groups.
Journal example() {
  Journal j;
  j.version = 3;
  j.summary = "The newcomer called Wren came in from the grassland and asked after the castle.";
  j.disposition = -1;
  j.told_name = "Wren";
  j.told_name_verified = true;
  j.keepsakes = {
      keep("k1", "They mean to take the castle on the cliffs, or so they say.", Keepsake::Basis::Said, 0.6, 20 * kMin),
      keep("k2", "They claim to have killed the lord of the castle.", Keepsake::Basis::Said, 0.6, 70 * kMin,
           "slew:godrick"),
      keep("k3", "The Tarnished struck me in my own shop and called it an accident.", Keepsake::Basis::Seen, 0.75,
           202 * kMin),
  };
  return j;
}

proto::PersonaFacts example_facts() {
  return facts({{"met_count", "3"},
                {"times_attacked", "1"},
                {"last_attacked_pt", std::to_string(200 * kMin)},
                {"last_met_pt", std::to_string(202 * kMin)},
                {"verified:slew:godrick", "1"}});
}

struct Shipped {
  ContentPack pack;
  std::shared_ptr<const DialoguePrompt> prompt;
  Shipped() {
    auto p = load_content(builtin_content());
    REQUIRE(p.has_value());
    pack = std::move(*p);
    auto speech = load_speech(pack);
    REQUIRE(speech.has_value());
    prompt = speech->prompt;
  }
  std::vector<proto::Message> render(const TarnishedBlock* block, std::span<const Turn> earlier = {},
                                     bool moved_on = false) {
    const auto idx =
        std::find_if(pack.personas.begin(), pack.personas.end(), [](const Persona& p) { return p.id == "kale"; });
    REQUIRE(idx != pack.personas.end());
    const nlohmann::json record = idx->resolved(0);
    const std::vector<std::string> recap = {"Anything new?"};
    DialogueContext c;
    c.persona = "kale";
    c.record = &record;
    c.name = "Kalé";
    c.recap = recap;
    c.earlier = earlier;
    c.earlier_moved_on = moved_on;
    c.tarnished = block;
    auto r = prompt->render(c);
    REQUIRE(r.has_value());
    return r->messages;
  }
};

std::string all_text(const std::vector<proto::Message>& m) {
  std::string out;
  for (const proto::Message& x : m) out += x.joined() + "\n";
  return out;
}

}  // namespace

TEST_CASE("block 8: a journal round-trips through the store's form; the protected keepsake is MEM §2.3's") {
  Journal j = example();
  proto::JournalVersion v;
  v.version = j.version;
  v.summary = j.summary;
  v.keepsakes = keepsakes_json(j.keepsakes);
  v.disposition = j.disposition;
  v.told_name = j.told_name;
  v.told_name_verified = true;
  const auto back = journal_of(v);
  REQUIRE(back.has_value());
  CHECK(back->keepsakes == j.keepsakes);
  CHECK(back->told_name == j.told_name);
  REQUIRE(protected_keepsake(j).has_value());
  CHECK(j.keepsakes[*protected_keepsake(j)].id == "k3");  // the heaviest about the Tarnished, at least 0.7
  j.keepsakes[2].weight = 0.65;
  CHECK_FALSE(protected_keepsake(j).has_value());
  j.keepsakes[0].weight = 0.8;
  j.keepsakes[1].weight = 0.8;
  CHECK(j.keepsakes[*protected_keepsake(j)].id == "k1");  // a tie: the older since_pt
  v.keepsakes = "not json";
  CHECK_FALSE(journal_of(v).has_value());
}

TEST_CASE("block 8: MEM §4.3's example, word for word on the shipped templates") {
  Shipped s;
  const proto::PersonaFacts f = example_facts();
  const Journal j = example();
  BlockNow now;
  now.pt = 205 * kMin;
  const TarnishedBlock b = tarnished_block(&f, &j, now);
  const std::string text = all_text(s.render(&b));
  CHECK(text.find("[The Tarnished, as you know them]\n"
                  "They told you their name is Wren. You have spoken with the Tarnished a few times. The Tarnished "
                  "struck you once, only a short while ago.\n"
                  "You are wary of the Tarnished.\n"
                  "In your own words: \"The newcomer called Wren came in from the grassland and asked after the "
                  "castle.\"\n"
                  "You saw for yourself:\n"
                  "- The Tarnished struck me in my own shop and called it an accident.\n"
                  "The Tarnished told you, and it has proved true:\n"
                  "- They claim to have killed the lord of the castle.\n"
                  "The Tarnished told you (their word only, not proof):\n"
                  "- They mean to take the castle on the cliffs, or so they say.") != std::string::npos);
}

TEST_CASE("block 8: the facts' words, the first meeting's line and the sightings") {
  BlockNow now;
  now.pt = 10 * 3600000;
  const TarnishedBlock none = tarnished_block(nullptr, nullptr, now);
  CHECK(none.facts == std::vector<std::string>{"You have never spoken with this Tarnished."});
  CHECK(none.disposition.empty());
  const proto::PersonaFacts seen = facts({{"seen_count", "2"}});
  CHECK(tarnished_block(&seen, nullptr, now).facts ==
        std::vector<std::string>{"You have never spoken with this Tarnished.",
                                 "You have seen this Tarnished pass by, but you have never spoken."});
  const proto::PersonaFacts some = facts({{"met_count", "1"},
                                          {"last_met_pt", std::to_string(now.pt - 2 * 3600000)},
                                          {"times_attacked", "2"},
                                          {"last_attacked_pt", "0"},
                                          {"fought_beside", "2"},
                                          {"fought_last_boss", "904"},
                                          {"seen_player_die", "1"}});
  now.boss_names[904] = "an invented warden";
  CHECK(tarnished_block(&some, nullptr, now).facts ==
        std::vector<std::string>{"You have spoken with the Tarnished once before.",
                                 "The Tarnished struck you more than once.",
                                 "Some time has passed since you two last talked.",
                                 "You fought beside the Tarnished more than once, most recently against an invented "
                                 "warden.",
                                 "You have seen the Tarnished fall once, and come back."});
  const proto::PersonaFacts long_ago = facts({{"met_count", "6"}, {"last_met_pt", "0"}});
  CHECK(tarnished_block(&long_ago, nullptr, now).facts ==
        std::vector<std::string>{"You have spoken with the Tarnished many times.",
                                 "A long while has passed since you two last talked."});
  Journal j;
  j.told_name = "Wren";
  CHECK(tarnished_block(&long_ago, &j, now).facts.front() == "They asked you to call them Wren.");
  CHECK(tarnished_block(&long_ago, &j, now).disposition == "You have not made up your mind about the Tarnished.");
  j.disposition = 2;
  CHECK(tarnished_block(&long_ago, &j, now).disposition == "You trust the Tarnished, as far as you trust anyone.");
}

TEST_CASE("block 8: within 450 tokens, the lightest keepsakes go first, the protected one never, then the facts") {
  Journal j;
  const std::string long_text(190, 'x');
  for (int i = 0; i < 8; ++i) {
    j.keepsakes.push_back(keep("k" + std::to_string(i + 1), long_text + std::to_string(i), Keepsake::Basis::Said,
                               0.1 * (i + 1) > 1.0 ? 1.0 : 0.05 * (i + 4), i * kMin));
  }
  j.keepsakes[0].weight = 0.95;  // the protected one: about the Tarnished, the heaviest
  j.keepsakes[0].basis = Keepsake::Basis::Seen;
  j.summary = std::string(600, 's');
  const proto::PersonaFacts f = facts({{"met_count", "2"}, {"seen_player_die", "3"}, {"last_met_pt", "0"}});
  BlockNow now;
  now.pt = 20 * 3600000;
  const TarnishedBlock b = tarnished_block(&f, &j, now);
  CHECK(block_tokens(b) <= kBlock8Cap);
  REQUIRE(b.seen.size() == 1);
  CHECK(b.seen[0].protect);
  // At most 7 kept; those left are the heaviest.
  CHECK(b.seen.size() + b.said.size() <= 7);
  for (const auto& l : b.said) CHECK(l.weight >= b.said.back().weight);
  // The name, the meetings and the attacks are never trimmed.
  CHECK(b.facts.front() == "You have spoken with the Tarnished a few times.");
}

TEST_CASE("block 6 (a): the header says things have moved on; T5: blocks 1 to 5 the same with block 8 or without") {
  Shipped s;
  const std::vector<Turn> earlier = {Turn{Turn::Who::Tarnished, "What do you sell?"},
                                     Turn{Turn::Who::Persona, "Rope and lamp oil."}};
  const std::string plain = all_text(s.render(nullptr, earlier));
  CHECK(plain.find("Earlier, when you last spoke with the Tarnished:\n") != std::string::npos);
  CHECK(plain.find("[The Tarnished, as you know them]") == std::string::npos);
  const std::string moved = all_text(s.render(nullptr, earlier, true));
  CHECK(moved.find("Earlier, when you last spoke with the Tarnished (things have moved on since):\n") !=
        std::string::npos);
  const proto::PersonaFacts f = example_facts();
  const Journal j = example();
  BlockNow now;
  now.pt = 205 * kMin;
  const TarnishedBlock b = tarnished_block(&f, &j, now);
  const std::vector<proto::Message> with = s.render(&b, earlier);
  const std::vector<proto::Message> without = s.render(nullptr, earlier);
  std::size_t systems = 0;
  for (std::size_t i = 0; i < without.size() && without[i].role == proto::Role::System; ++i) {
    REQUIRE(i < with.size());
    CHECK(with[i] == without[i]);  // the cached prefix: every system part, byte for byte
    ++systems;
  }
  CHECK(systems >= 1);
}
