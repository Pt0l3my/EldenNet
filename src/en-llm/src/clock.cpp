#include "en/llm/clock.hpp"

namespace en::llm {

void ManualClock::advance_to(TimePoint t) {
  const auto target = t.time_since_epoch().count();
  auto current = ticks_.load();
  while (current < target && !ticks_.compare_exchange_weak(current, target)) {
  }
}

std::shared_ptr<Clock> system_clock() {
  static const auto clock = std::make_shared<SystemClock>();
  return clock;
}

}  // namespace en::llm
