#pragma once
// The Characters pages' invented facts for the fixture states (PAGES §5; tests/pages/<state>/topics.json): two invented
// characters, one with the player's own layer. No game text: every name and line is ours, written for the tests.
#include <string>
#include <string_view>
#include <vector>

#include "en/overlay/characters.hpp"

namespace en::overlay::test {

inline ProfileRow ferryman() {
  ProfileRow r;
  r.key = "persona.ferryman";
  r.name = "Old Bram";
  r.role = "a ferryman on the grey river";
  r.summary = "A stooped man in an oilskin coat who poles a flat boat across the river for a coin.";
  r.subjects = {"the river's moods", "coins and what they buy", "the weather over the water"};
  r.register_id = "colloquial";
  r.reply_words_max = 30;
  r.news = "plain";
  return r;
}

inline ProfileRow weaver() {
  ProfileRow r;
  r.key = "persona.weaver";
  r.name = "Ysolde";
  r.role = "a weaver of fishing nets";
  r.summary = "A quick-handed woman mending nets on a jetty, who looks up only when a stranger stops.";
  r.subjects = {"knots and the hands that tie them", "the catch this season"};
  r.register_id = "formal";
  r.reply_words_max = 40;
  r.layer = true;
  r.rev = 3;
  r.fields = {{"instructions", "Counts under her breath while she works, and loses count when someone interrupts."},
              {"subjects", {"knots and the hands that tie them", "old songs of the jetty"}}};
  r.warnings = 1;
  return r;
}

// The facts each fixture state points at: none before the content loads (starting); the two characters otherwise, the
// weaver's layer saved over an older shipped profile in the world.
inline const CharacterFacts* characters_fixture(std::string_view state) {
  static const CharacterFacts title = [] {
    CharacterFacts f;
    f.revision = 1;
    f.rows = {ferryman(), weaver()};
    f.draft_provider = "cloud-one";
    return f;
  }();
  static const CharacterFacts world = [] {
    CharacterFacts f = title;
    f.revision = 2;
    f.rows[1].updated = true;
    f.draft_blocked = "Drafts wait until EldenNet may use the network.";
    return f;
  }();
  if (state == "starting") return nullptr;
  if (state == "world" || state == "multiplayer") return &world;
  return &title;
}

// Every topic of the Characters pages the fixtures hold, for a state's facts: the list, then each row's two members.
inline std::vector<std::string> characters_topics(const CharacterFacts* f) {
  std::vector<std::string> out = {std::string(kNamedList)};
  if (f == nullptr) return out;
  for (const ProfileRow& r : f->rows) {
    out.push_back(std::string(kNamedFamily) + "/" + r.key);
    out.push_back(std::string(kProfilesFamily) + "/" + r.key);
  }
  return out;
}

}  // namespace en::overlay::test
