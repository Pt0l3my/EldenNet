#pragma once
// The player's behaviour settings the core reads (ARCH §11.5; DESIGN §11.4; S11.3.d): `settings.toml`'s `features`
// switches and `range` table, with their defaults and bounds. The shell parses the file (strictly: an unknown key, a
// wrong type or a value out of its bounds refuses the whole file, and the defaults are used) and hands the core what it
// read; `Core::set_behaviour` takes effect at the next step (ARCH §11.6). Pure: no I/O.
//
// What v0.2 acts on, as far as the design gives it for the first conversation:
// - `features.chat` (default on): off, the talk key opens nothing and shows no hint, and an open conversation ends at
//   the next step (`closed`);
// - `features.self_awareness` (default on): off, block 9 has no "you, now" line (NPC §7.3; O35). With no statuses
//   before S8, `status_awareness` adds nothing yet (DESIGN §9.9).
// The other switches (barks, speak_up, callout, memory, labels, npc_to_npc) and the ranges belong to lines an NPC starts
// itself, the call-out, memory and the labels (v0.3 to v0.6): they are carried, checked and not read yet. The
// conversation's own reach is NPC §5.2's, fixed (a 25° cone within 8 m, else the nearest within 5 m), not `talk_m`,
// which bounds a line an NPC starts towards the Tarnished (DESIGN §8.9).
#include <optional>
#include <string>

namespace en::core {

struct Features {
  bool chat = true;
  bool barks = true;
  bool speak_up = true;
  bool callout = true;
  bool memory = true;
  bool labels = true;
  bool npc_to_npc = true;
  bool self_awareness = true;
  bool status_awareness = true;

  friend constexpr bool operator==(const Features&, const Features&) = default;
};

struct Bounds {
  float lo = 0;
  float hi = 0;
};

// `range` (metres): who may start a line at all (NPC §9.4): towards the Tarnished, overheard, and in a fight.
struct Ranges {
  float talk_m = 8.0f;
  float hear_m = 20.0f;
  float fight_m = 40.0f;

  static constexpr Bounds kTalk{3.0f, 16.0f};
  static constexpr Bounds kHear{8.0f, 30.0f};
  static constexpr Bounds kFight{10.0f, 40.0f};

  friend constexpr bool operator==(const Ranges&, const Ranges&) = default;
};

struct Behaviour {
  Features features;
  Ranges range;

  friend constexpr bool operator==(const Behaviour&, const Behaviour&) = default;
};

// The first value out of its bounds, by its key ("range.talk_m"), or nothing when all are in bounds (a value that is
// not a number is out of bounds). Never the value itself.
[[nodiscard]] std::optional<std::string> check_behaviour(const Behaviour& b);

}  // namespace en::core
