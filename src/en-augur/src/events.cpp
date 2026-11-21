// The catalogue's names, visible_to and the kinds' interrupt sets (events.hpp; ARCH §8.6, §8.8).
#include "en/augur/events.hpp"

#include <algorithm>
#include <array>

#include "en/augur/signals.hpp"

namespace en::augur {

namespace {

constexpr float kInterruptEngagedM = 25.0f;

}  // namespace

std::string_view kind_name(std::size_t index) noexcept { return index < kKindNames.size() ? kKindNames[index] : ""; }

std::optional<std::size_t> kind_index(std::string_view name) noexcept {
  const auto it = std::find(kKindNames.begin(), kKindNames.end(), name);
  if (it == kKindNames.end()) return std::nullopt;
  return static_cast<std::size_t>(it - kKindNames.begin());
}

std::optional<SignalId> signal_by_name(std::string_view name) noexcept {
  for (const SignalInfo& s : kSignals) {
    if (s.name == name) return s.id;
  }
  return std::nullopt;
}

bool visible_to(const Event& ev, PersonaId persona) noexcept {
  return std::any_of(ev.audience.begin(), ev.audience.end(),
                     [&](const AudienceEntry& a) { return a.persona == persona; });
}

InterruptSet kind_interrupts(const Event& ev) noexcept {
  if (std::holds_alternative<Engaged>(ev.kind)) {
    return ev.confidence >= 0.6f && ev.distance_m <= kInterruptEngagedM
               ? InterruptSet::ChatInput | InterruptSet::Ambient | InterruptSet::Autonomous
               : InterruptSet::None;
  }
  if (std::holds_alternative<PlayerDied>(ev.kind)) {
    return InterruptSet::Conversation | InterruptSet::OwnLinesExceptPlayerFell;
  }
  if (std::holds_alternative<LoadingStarted>(ev.kind)) return kInterruptAll;
  if (std::holds_alternative<NativeTalkStarted>(ev.kind)) {
    return InterruptSet::ConversationOutput | InterruptSet::OwnLinesWithin20m;
  }
  if (const auto* h = std::get_if<NativeLineHeard>(&ev.kind); h != nullptr && h->phase == LinePhase::Started) {
    return InterruptSet::ConversationOutput | InterruptSet::OwnLinesWithin20m;
  }
  return InterruptSet::None;
}

}  // namespace en::augur
