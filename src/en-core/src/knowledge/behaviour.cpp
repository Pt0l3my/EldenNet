// The behaviour settings' bounds (behaviour.hpp; ARCH §11.5).
#include "en/core/behaviour.hpp"

#include <cmath>

namespace en::core {

std::optional<std::string> check_behaviour(const Behaviour& b) {
  const auto out = [](float v, Bounds r) { return !std::isfinite(v) || v < r.lo || v > r.hi; };
  if (out(b.range.talk_m, Ranges::kTalk)) return std::string("range.talk_m");
  if (out(b.range.hear_m, Ranges::kHear)) return std::string("range.hear_m");
  if (out(b.range.fight_m, Ranges::kFight)) return std::string("range.fight_m");
  return std::nullopt;
}

}  // namespace en::core
