// EldenNet's claims as a world-state provider (world.hpp).
#include "en/overlay/world.hpp"

#include "graceui.h"

namespace en::overlay {

bool danger_stand_in(const proto::WorldSnapshot& s) noexcept {
  if (s.boss.active) return true;
  if (!s.player) return false;
  return s.player->dead || s.player->locked_on.has_value();
}

std::uint32_t world_flags(const WorldInputs& in) noexcept {
  std::uint32_t flags = 0;
  switch (in.where) {
    case WorldInputs::Where::Title: flags |= GRACEUI_WORLD_TITLE; break;
    case WorldInputs::Where::Loading: flags |= GRACEUI_WORLD_LOADING; break;
    case WorldInputs::Where::World:
      if (in.latest != nullptr) {
        if (danger_stand_in(*in.latest)) flags |= GRACEUI_WORLD_DANGER;
        if (in.latest->ui.hud == proto::HudState::PopupMenu) flags |= GRACEUI_WORLD_GAME_MENU;
      }
      break;
    case WorldInputs::Where::Starting:
    case WorldInputs::Where::Stopped: break;
  }
  if (in.multiplayer) flags |= GRACEUI_WORLD_MULTIPLAYER;
  if (in.anticheat) flags |= GRACEUI_WORLD_ANTI_CHEAT;
  return flags;
}

}  // namespace en::overlay
