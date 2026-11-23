// The gates (conversation.hpp; NPC §5.3, §5.4; ARCH §6.8): safe_context and conversable, each the first clause that
// fails in the design's order.
#include <array>

#include "en/core/conversation.hpp"

namespace en::core {

const char* to_string(NotConversable v) noexcept {
  constexpr std::array<const char*, 9> kNames = {"dead",    "asleep",     "muted", "stage",      "hostile",
                                                 "recently_attacked", "native_talk", "busy", "unavailable"};
  const auto i = static_cast<std::size_t>(v);
  return i < kNames.size() ? kNames[i] : "unavailable";
}

const char* to_string(Unsafe v) noexcept {
  constexpr std::array<const char*, 6> kNames = {"not_active", "player", "boss", "combat", "hostiles", "menu"};
  const auto i = static_cast<std::size_t>(v);
  return i < kNames.size() ? kNames[i] : "not_active";
}

std::optional<Unsafe> safe_context(const Surroundings& w) noexcept {
  if (!w.active) return Unsafe::NotActive;  // below L3, dormant, loading or out of a world
  if (w.player_dead || w.riding) return Unsafe::Player;
  if (w.boss) return Unsafe::Boss;
  if (w.combat) return Unsafe::Combat;  // never waived
  if (!w.sheltered) {                   // sheltered waives only the distance checks
    if (w.engaged_hostile_m <= 25.0f) return Unsafe::Hostiles;
    if (w.hostile_m <= 12.0f) return Unsafe::Hostiles;
    if (!w.caps_combat && w.hostile_m <= 25.0f) return Unsafe::Hostiles;  // stricter without the combat group
  }
  if (w.menu || w.banner) return Unsafe::Menu;  // §5.8 before S3; an open box ignores the banner (O108)
  return std::nullopt;
}

std::optional<NotConversable> conversable(const PartnerFacts& p, const Surroundings& w, Mono now) noexcept {
  if (p.persona == nullptr || p.chr == nullptr) return NotConversable::Unavailable;
  if (p.chr->dead) return NotConversable::Dead;
  const Conscious state = p.persona->state_at(p.stage);
  if (state == Conscious::Asleep || state == Conscious::Senseless) return NotConversable::Asleep;  // unknown talks
  // Muted: may_speak, the row check and the pack's state, and the dialogue allowance (§1.4): none of them is read in
  // v0.2 (no speech.toml, no types.toml, no name ids), so a resolved persona may speak.
  if (p.persona->chat_at(p.stage) != ChatMode::Allowed) return NotConversable::Stage;
  if (p.hostile) return NotConversable::Hostile;  // the +1/+2/+3 bits wait for the catalogue
  if (p.attacked_at && now - *p.attacked_at < ConversationTimes::kAttackedMs) return NotConversable::RecentlyAttacked;
  if (w.native) return NotConversable::NativeTalk;  // before S3 the observer cannot tell how near: any native talk
  if (p.summon || p.holds_boss_bar) return NotConversable::Busy;
  if (!w.caps_conversation) return NotConversable::Unavailable;
  return std::nullopt;
}

}  // namespace en::core
