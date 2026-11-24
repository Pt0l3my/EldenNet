#pragma once
// The Content packs page's invented facts for the fixture states (PAGES §4.6.9; tests/pages/<state>/topics.json): the
// characters_fixture.hpp's two invented characters, their layers and invented cards. No game text: every id, path and
// count is ours, and each problem is in the content layer's own words.
#include <string_view>

#include "en/proto/content_view.hpp"

namespace en::overlay::test {

namespace pr = en::proto;

// Before the first load nothing (starting); at the title the start's load, which passed: the player's disable.toml
// (a warning: an id that names nothing), the weaver's layer loaded and the ferryman's refused alone, a persona and a
// card turned off; in the world a later reload refused by an override of the ferryman's record (the content of the
// start stays in use, the other files wait), and a file the read left out.
inline const pr::ContentView* content_fixture(std::string_view state) {
  static const pr::ContentView title = [] {
    pr::ContentView v;
    v.revision = 2;
    v.loaded = true;
    v.personas = 2;
    v.cards = 3;
    v.knowledge = true;
    v.rules = true;
    v.overrides = {
        pr::OverrideFile{"disable.toml",
                         pr::OverrideState::Loaded,
                         {pr::ContentProblem{3, false, "cards[1]: names no shipped card, so nothing is turned off"}}},
        pr::OverrideFile{
            "layers/persona-ferryman.toml",
            pr::OverrideState::Refused,
            {pr::ContentProblem{0, true, "reply_words_max: refused: Must be a whole number from 10 to 60."}}},
        pr::OverrideFile{"layers/persona-weaver.toml", pr::OverrideState::Loaded, {}},
    };
    v.off_personas = {"heron_watcher"};
    v.off_cards = {"salt_crier"};
    v.reloads = {pr::ContentReload{6'200, pr::ReloadLayer::All, true, false, 0, 2}};
    return v;
  }();
  static const pr::ContentView world = [] {
    pr::ContentView v = title;
    v.revision = 4;
    v.overrides = {
        pr::OverrideFile{"disable.toml", pr::OverrideState::Waiting, {}},
        pr::OverrideFile{"layers/persona-ferryman.toml", pr::OverrideState::Waiting, {}},
        pr::OverrideFile{"layers/persona-weaver.toml", pr::OverrideState::Waiting, {}},
        pr::OverrideFile{"personas/ferryman.toml",
                         pr::OverrideState::Refused,
                         {pr::ContentProblem{12, true, "voice.cadence: is longer than 60 words"}}},
    };
    v.left_out = {pr::OverrideFile{"drafts/old/a/b/c",
                                   pr::OverrideState::LeftOut,
                                   {pr::ContentProblem{0, true, "is a folder nested too deep: four levels are read"}}}};
    v.reloads.insert(v.reloads.begin(), pr::ContentReload{1'275'400, pr::ReloadLayer::Overrides, false, true, 1, 0});
    return v;
  }();
  if (state == "starting") return nullptr;
  if (state == "world" || state == "multiplayer") return &world;
  return &title;
}

}  // namespace en::overlay::test
