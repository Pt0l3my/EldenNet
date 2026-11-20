#pragma once
// EldenNet as a world-state provider (GraceUI D78, D143; part 12): the claims it makes to GraceUI, from what v0.1 knows,
// replaced whole whenever they change. GraceUI combines every provider's claims; danger and loading end any input holder
// at once (GraceUI's invariant 9), a game menu keeps `request_input` at not now, and an anti-cheat finding closes the
// browser view for the session (GraceUI D81). Portable and pure: graceui.h's flags are plain C.
//
// What v0.1 knows, and the stand-ins until the later parts:
// - title and loading: en-core's lifecycle phase (ARCH §9);
// - multiplayer: the pause badge is up, or the gate closed for a reason of the session (ARCH §5.1, §5.4);
// - anti-cheat: the Safety state's check or T10's (C14);
// - danger: a STAND-IN until en-core's `safe_context` (NPC §5.4), which needs the combat state of en-augur (part 11)
//   and the ladder's L3: in a world, the player dead, a boss bar showing, or the player locked on to a target;
// - a game menu: a STAND-IN until NPC §5.8's menu read: in a world, the HUD in its pop-up menu state. The game's own
//   text entry is not read in v0.1.
#include <cstdint>

#include "en/proto/snapshot.hpp"

namespace en::overlay {

struct WorldInputs {
  enum class Where : std::uint8_t { Starting, Title, Loading, World, Stopped };
  Where where = Where::Starting;  // Stopped: at a banner (L1)
  bool multiplayer = false;
  bool anticheat = false;
  const proto::WorldSnapshot* latest = nullptr;  // the newest snapshot, if one has arrived
};

// The claims, as GRACEUI_WORLD_* flags.
[[nodiscard]] std::uint32_t world_flags(const WorldInputs& in) noexcept;
// v0.1's danger stand-in on one snapshot (above).
[[nodiscard]] bool danger_stand_in(const proto::WorldSnapshot& s) noexcept;

}  // namespace en::overlay
