#pragma once
// The task's lifecycle messages to the core (ARCH §3.2, §9; channel C2): DataReady once the task is registered and
// phase A has passed, PlaythroughReady once per playthrough with its identity (read once), and GroupTripped when a
// read group's breaker trips. Everything else about the lifecycle the core derives from snapshot fields, so a lost
// message can always be rebuilt (C2). Each kind is trivially copyable, and so is the message: it travels through a
// fixed-size SPSC ring.
#include <cstdint>
#include <type_traits>
#include <variant>

#include <nlohmann/json.hpp>

#include "en/proto/fixed.hpp"
#include "en/proto/snapshot.hpp"

namespace en::proto {

// The character's name, as UTF-8: 1 to 16 UTF-16 units in the game, so at most 48 bytes.
using PlayerName = FixedString<48>;

struct Lifecycle {
  // The task is registered and ticking, and phase A passed: the ladder leaves Gate (ARCH §3.2's Ready).
  struct DataReady {
    BuildKey build;
    Capabilities caps;

    friend constexpr bool operator==(const DataReady&, const DataReady&) = default;
  };
  // A playthrough's identity, read once when it starts (ARCH §9). The name is the player's character's: a recording
  // replaces it with a keyed hash (ARCH §7.5), and nothing else carries it.
  struct PlaythroughReady {
    std::uint8_t slot = 0;
    PlayerName name;
    std::uint8_t archetype = 0;
    std::uint8_t keepsake = 0;
    std::uint64_t play_time_ms = 0;

    friend constexpr bool operator==(const PlaythroughReady&, const PlaythroughReady&) = default;
  };
  // A group's breaker tripped: off for the session (ARCH §6.7).
  struct GroupTripped {
    Group group = Group::Session;

    friend constexpr bool operator==(const GroupTripped&, const GroupTripped&) = default;
  };

  // DataReady comes first and has no default member initializer of its own: the variant needs its default
  // constructor while this class is still being defined, before the kinds' member initializers are usable.
  std::variant<DataReady, PlaythroughReady, GroupTripped> payload;

  friend constexpr bool operator==(const Lifecycle&, const Lifecycle&) = default;
};

static_assert(std::is_trivially_copyable_v<Lifecycle>, "Lifecycle travels through a fixed-size ring (C2)");

// JSON forms (game_json.cpp): {"data_ready": {...}}, {"playthrough_ready": {...}} or {"group_tripped": {...}}, exactly
// one of them.
void to_json(nlohmann::json& j, const Lifecycle::DataReady& v);
void from_json(const nlohmann::json& j, Lifecycle::DataReady& v);
void to_json(nlohmann::json& j, const Lifecycle::PlaythroughReady& v);
void from_json(const nlohmann::json& j, Lifecycle::PlaythroughReady& v);
void to_json(nlohmann::json& j, const Lifecycle::GroupTripped& v);
void from_json(const nlohmann::json& j, Lifecycle::GroupTripped& v);
void to_json(nlohmann::json& j, const Lifecycle& v);
void from_json(const nlohmann::json& j, Lifecycle& v);

}  // namespace en::proto
