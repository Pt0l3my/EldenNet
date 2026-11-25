#pragma once
// Augur's words: one closed vocabulary for every reader of an event (the golden files, golden.hpp; the F11 view,
// through the DLL). Each is a NUL-terminated ASCII word of ours with static storage, never a name or a game's text; a
// value outside its enum is "?".
//
// `reason` is an event's one telling fact for a glance: a word of this vocabulary, and a number where the payload's
// telling field is one (a place's id, a count, a slot). Never an HP fraction or a distance, and never a handle.
#include <cstdint>

#include "en/augur/detectors.hpp"
#include "en/augur/events.hpp"
#include "en/augur/types.hpp"

namespace en::augur {

[[nodiscard]] const char* to_string(Class v) noexcept;          // "P", "K", "T"
[[nodiscard]] const char* to_string(How v) noexcept;            // "direct", "overheard", "witnessed"
[[nodiscard]] const char* to_string(Radius v) noexcept;         // "speech", "witness", "death", "combat", "none"
[[nodiscard]] const char* to_string(SourceId v) noexcept;       // "lifecycle", "snapshot_diff", ..., "subscriber"
[[nodiscard]] const char* to_string(ActorRef::Kind v) noexcept; // "player", "persona", "card", "mute"
[[nodiscard]] const char* to_string(SideId::Kind v) noexcept;   // "tarnished", "card", "team"
[[nodiscard]] const char* to_string(BossOutcome v) noexcept;    // "Victory", "Death", "Left"
[[nodiscard]] const char* to_string(EngageVia v) noexcept;      // "Hit", "LockClosing", "Near", "LockOnly"
[[nodiscard]] const char* to_string(DisengageWhy v) noexcept;   // "Timeout", "Distance"
[[nodiscard]] const char* to_string(LadderLevel v) noexcept;    // "Starting", "L1", ..., "L4"
[[nodiscard]] const char* to_string(DetectorKind v) noexcept;   // "rising_edge", ..., "fact": rule files' words

// The kind's name, NUL-terminated (kKindNames' own literal); "?" for an index outside the catalogue.
[[nodiscard]] const char* kind_word(const EventKind& k) noexcept;

struct Reason {
  const char* word = nullptr;  // null: the kind has nothing to add at a glance
  bool has_number = false;
  std::uint64_t number = 0;
};

// The event's reason, by its kind:
// - PlaceEntered "place" and the place's id; BossFightStarted "bars" and their count; BossFightEnded its outcome;
// - NativeTalkStarted the first signal up ("popup", "talk_cam", "popup_menu"); NativeTalkEnded "held_ms" and its ms;
// - PlayerDied "in_boss", "killed" (a killer named) or "died"; MountChanged "mounted" or "dismounted";
// - Struck "unseen", "hard" or "hit"; Engaged its start condition; Disengaged "fled" or its why;
// - AllyDied "witnesses" and their count; PlaythroughReady "slot" and the slot; JourneyChanged "ng" and the level;
// - GateChanged "active" or "inactive"; GroupTripped the group (en-proto's word); LadderChanged the level reached;
// - every other kind nothing (LowHp and Healed among them: their payload is an HP fraction).
[[nodiscard]] Reason reason(const Event& ev) noexcept;

}  // namespace en::augur
