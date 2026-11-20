#pragma once
// Augur's small types (ARCH §8.1, §8.6, §8.8; DESIGN §6.4): time, identities, the references events carry, the event
// classes, the audience's rows and the interrupt sets. Game-neutral: a handle is en-proto's, everything else is ours.
#include <array>
#include <compare>
#include <cstdint>
#include <optional>

#include "en/proto/fixed.hpp"
#include "en/proto/snapshot.hpp"

namespace en::augur {

// Mono time in milliseconds, as en-core's (the snapshots' own `mono_ms`, and the `now` the core is given). Augur reads
// no clock: time only comes in.
using Mono = std::uint64_t;

using proto::BlockId;
using proto::ChrHandle;

// A profile id (O45: a persona or a verified character binding) and a card id: indices into the content that en-core
// loads. v0.1 has neither (personas come with v0.2, cards with v0.4), so nothing in this build makes one.
struct PersonaId {
  std::uint32_t index = 0;
  friend constexpr auto operator<=>(const PersonaId&, const PersonaId&) = default;
};
struct CardId {
  std::uint32_t index = 0;
  friend constexpr auto operator<=>(const CardId&, const CardId&) = default;
};
using StageId = std::uint16_t;
using SquadId = std::uint32_t;  // 0: none

// What a character is to EldenNet (ARCH §8.1's AugurCtx::classify): a persona (with whether its stage mutes it and
// whether it may chat), a card, a mute character (perceived, never voiced), or the Tarnished.
struct Subject {
  enum class Kind : std::uint8_t { Mute, Player, Persona, Card };
  Kind kind = Kind::Mute;
  PersonaId persona;  // Persona
  bool stage_muted = false;
  bool chat = false;
  CardId card;  // Card
  std::uint8_t variant = 0;

  friend constexpr bool operator==(const Subject&, const Subject&) = default;
};

// Who an event is about (ARCH §8.6): Player | Persona{id, handle} | Card{id, variant, handle} | Mute{handle}.
struct ActorRef {
  enum class Kind : std::uint8_t { Player, Persona, Card, Mute };
  Kind kind = Kind::Player;
  ChrHandle handle;  // not the Tarnished's: the Tarnished is Player
  std::uint32_t id = 0;  // the persona's or the card's index
  std::uint8_t variant = 0;

  [[nodiscard]] static constexpr ActorRef player() noexcept { return ActorRef{}; }
  [[nodiscard]] static constexpr ActorRef mute(ChrHandle h) noexcept { return ActorRef{Kind::Mute, h, 0, 0}; }
  [[nodiscard]] static ActorRef of(const Subject& s, ChrHandle h) noexcept;

  friend constexpr bool operator==(const ActorRef&, const ActorRef&) = default;
};

// An event's identity: the Augur step that made it and its place in that step's dispatch order (DESIGN §6.4).
struct EventId {
  std::uint64_t step = 0;
  std::uint16_t index = 0;
  friend constexpr auto operator<=>(const EventId&, const EventId&) = default;
};

// P persistent (stored with its audience), K keyed and short-lived (the K store, "just now" lines), T transient (it
// only triggers): DESIGN §6.4.
enum class Class : std::uint8_t { P, K, T };

// How an audience row perceived the event (DESIGN §5.4).
enum class How : std::uint8_t { Direct, Overheard, Witnessed };

// The audience radius by kind (ARCH §8.8 item 2; DESIGN §5.4), and none.
enum class Radius : std::uint8_t { Speech, Witness, Death, Combat, None };
[[nodiscard]] constexpr float radius_m(Radius r) noexcept {
  switch (r) {
    case Radius::Speech:
      return 16.0f;
    case Radius::Witness:
      return 20.0f;
    case Radius::Death:
      return 30.0f;
    case Radius::Combat:
      return 40.0f;
    case Radius::None:
      return 0.0f;
  }
  return 0.0f;
}

struct AudienceEntry {
  PersonaId persona;
  How how = How::Witnessed;
  friend constexpr bool operator==(const AudienceEntry&, const AudienceEntry&) = default;
};
inline constexpr std::size_t kMaxAudience = 16;  // ARCH §8.8 item 7 (impl)

// Where an event happened (DESIGN §6.4): the Tarnished's block, place and region at the time.
struct PlaceRef {
  BlockId block;
  std::optional<std::uint32_t> place_id;
  proto::RegionKey region;
  friend constexpr bool operator==(const PlaceRef&, const PlaceRef&) = default;
};

// The interrupt sets (ARCH §8.8), a flag set.
enum class InterruptSet : std::uint8_t {
  None = 0,
  ChatInput = 1u << 0,
  Ambient = 1u << 1,
  Autonomous = 1u << 2,
  Conversation = 1u << 3,
  ConversationOutput = 1u << 4,
  OwnLinesExceptPlayerFell = 1u << 5,
  OwnLinesWithin20m = 1u << 6,
};

// Which source an observation came from (ARCH §8.2), in the bus's polling order (§8.9 step 2), then the bus itself for
// events subscribers emit.
enum class SourceId : std::uint8_t { Lifecycle, SnapshotDiff, FlagWatch, Pipeline, Timer, Hook, Subscriber };

// The Tarnished's side, a card, or a live team (ARCH §8.6).
struct SideId {
  enum class Kind : std::uint8_t { Tarnished, Card, Team };
  Kind kind = Kind::Tarnished;
  std::uint32_t value = 0;  // the card's index or the live team
  friend constexpr bool operator==(const SideId&, const SideId&) = default;
};

// A boss bar as an event carries it: the bar's character and the id of the name it shows.
struct BossRef {
  ActorRef boss;
  std::int32_t name_id = 0;
  friend constexpr bool operator==(const BossRef&, const BossRef&) = default;
};

// A playthrough, as v0.1 tells one (WHY§176): its save slot and its ordinal in this session. The store's key (SteamID,
// slot, fp8; DESIGN §10.5) replaces it when the store exists. Never the character's name.
struct PlaythroughKey {
  std::uint8_t slot = 0;
  std::uint64_t ordinal = 0;
  friend constexpr bool operator==(const PlaythroughKey&, const PlaythroughKey&) = default;
};

}  // namespace en::augur

template <>
struct en::proto::FlagSetTraits<en::augur::InterruptSet> {
  static constexpr en::augur::InterruptSet all = static_cast<en::augur::InterruptSet>(0x7F);
};

namespace en::augur {
inline constexpr InterruptSet kInterruptAll = proto::FlagSetTraits<InterruptSet>::all;
using proto::operator|;
using proto::operator&;
using proto::any;
}  // namespace en::augur
