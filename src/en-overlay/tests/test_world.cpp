// EldenNet's claims as a world-state provider (world.hpp).
#include <doctest.h>

#include "en/overlay/world.hpp"
#include "graceui.h"

using namespace en::overlay;
using Where = WorldInputs::Where;

TEST_CASE("world: the title, loading, multiplayer and anti-cheat claims") {
  CHECK(world_flags(WorldInputs{Where::Starting}) == 0);
  CHECK(world_flags(WorldInputs{Where::Stopped}) == 0);
  CHECK(world_flags(WorldInputs{Where::Title}) == GRACEUI_WORLD_TITLE);
  CHECK(world_flags(WorldInputs{Where::Loading}) == GRACEUI_WORLD_LOADING);
  CHECK(world_flags(WorldInputs{Where::World}) == 0);  // no snapshot yet: nothing claimed
  WorldInputs in{Where::World, true, true, nullptr};
  CHECK(world_flags(in) == (GRACEUI_WORLD_MULTIPLAYER | GRACEUI_WORLD_ANTI_CHEAT));
  in.where = Where::Stopped;
  CHECK(world_flags(in) == (GRACEUI_WORLD_MULTIPLAYER | GRACEUI_WORLD_ANTI_CHEAT));
}

TEST_CASE("world: v0.1's danger stand-in and the game menu's, in a world only") {
  en::proto::WorldSnapshot s;
  WorldInputs in{Where::World, false, false, &s};
  CHECK(world_flags(in) == 0);
  CHECK_FALSE(danger_stand_in(s));
  s.boss.active = true;
  CHECK(danger_stand_in(s));
  CHECK(world_flags(in) == GRACEUI_WORLD_DANGER);
  s.boss.active = false;
  s.player.emplace();
  CHECK_FALSE(danger_stand_in(s));
  s.player->locked_on = en::proto::ChrHandle{};
  CHECK(danger_stand_in(s));
  s.player->locked_on.reset();
  s.player->dead = true;
  CHECK(danger_stand_in(s));
  s.player->dead = false;
  s.ui.hud = en::proto::HudState::PopupMenu;
  CHECK(world_flags(in) == GRACEUI_WORLD_GAME_MENU);
  in.where = Where::Title;  // at the title neither is claimed, whatever the last world snapshot said
  s.boss.active = true;
  CHECK(world_flags(in) == GRACEUI_WORLD_TITLE);
}
