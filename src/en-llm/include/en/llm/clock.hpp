#pragma once
// The client's clock: the system's steady clock in use, a manual one in tests, so timeouts, cooldowns and the
// requests-per-hour window can be tested without waiting.
#include <atomic>
#include <chrono>
#include <memory>

namespace en::llm {

using SteadyClock = std::chrono::steady_clock;
using TimePoint = SteadyClock::time_point;
using Millis = std::chrono::milliseconds;

class Clock {
 public:
  virtual ~Clock() = default;
  virtual TimePoint now() const = 0;
};

class SystemClock final : public Clock {
 public:
  TimePoint now() const override { return SteadyClock::now(); }
};

// Moves only when told to. Safe to read and advance from several threads.
class ManualClock final : public Clock {
 public:
  ManualClock() = default;
  TimePoint now() const override { return TimePoint(SteadyClock::duration(ticks_.load())); }
  void advance(SteadyClock::duration d) { ticks_.fetch_add(d.count()); }
  // Moves forward to `t`; never backwards.
  void advance_to(TimePoint t);

 private:
  // Starts an hour in, so that "an hour ago" is still a valid time point.
  std::atomic<SteadyClock::rep> ticks_{std::chrono::duration_cast<SteadyClock::duration>(std::chrono::hours(1)).count()};
};

std::shared_ptr<Clock> system_clock();

}  // namespace en::llm
