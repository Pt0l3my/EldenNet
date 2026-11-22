// The reader's trial of Lodestar's in-game round (lodestar_trial.hpp).
#include "en/gal/lodestar_trial.hpp"

#include <algorithm>
#include <utility>

namespace en::gal::lodestar {

Trial::Trial(SysLayer& sys, std::uint64_t base, std::uint64_t period_us) noexcept
    : sys_(sys), reader_(sys), base_(base), period_us_(period_us) {}

bool Trial::add(const ManagerFound& manager) {
  const std::size_t n = count_.load(std::memory_order_acquire);
  if (manager.status != derive::Status::Derived || n >= kMaxManagers) return false;
  for (std::size_t i = 0; i < n; ++i) {
    if (slots_[i].id == manager.id) return false;
  }
  Slot& s = slots_[n];  // not yet the reader's: it sees a slot only once the count says so
  s.id = manager.id;
  s.label = manager.label;
  s.slot = base_ + manager.rva;
  s.vtable = base_ + manager.vtable;
  count_.store(n + 1, std::memory_order_release);
  return true;
}

void Trial::run(const proto::GameThread& gt, const FrameCtx& frame) {
  steps_.fetch_add(1, std::memory_order_relaxed);
  // A reading is due at the step nearest its time: half a step early rather than a whole step late, and the next is
  // counted from when this one was due, so that the rate holds whatever the step.
  const std::uint64_t now = frame.qpc_now_us;
  if (now + frame.dt_us / 2 < next_us_) return;
  next_us_ = std::max(next_us_ + period_us_, now + period_us_ / 2);
  if (!bound_) {
    bound_ = true;
    reader_.bind_thread();
  }
  reader_.begin_tick();
  const std::uint64_t t0 = sys_.now_us();
  const std::size_t n = count_.load(std::memory_order_acquire);
  for (std::size_t i = 0; i < n; ++i) {
    Slot& s = slots_[i];
    const auto bump = [](std::atomic<std::uint64_t>& counter) { counter.fetch_add(1, std::memory_order_relaxed); };
    // The slot, the instance's first word, then the slot again: a slot that changed in between judges nothing.
    const auto first = reader_.copy<std::uint64_t>(gt, proto::Group::Session, s.slot);
    if (!first) {
      bump(s.fault);
      continue;
    }
    if (*first == 0) {
      bump(s.null);
      continue;
    }
    const auto word = reader_.copy<std::uint64_t>(gt, proto::Group::Session, *first);
    const auto again = reader_.copy<std::uint64_t>(gt, proto::Group::Session, s.slot);
    if (!again) {
      bump(s.fault);
    } else if (*again != *first) {
      bump(s.torn);
    } else if (!word) {
      bump(s.fault);
    } else {
      bump(*word == s.vtable ? s.same : s.other);
    }
  }
  const std::uint64_t done = sys_.now_us();
  reading_us_.record(done - t0);
  if (readings_.fetch_add(1, std::memory_order_relaxed) == 0) first_us_.store(t0, std::memory_order_relaxed);
  last_us_.store(done, std::memory_order_relaxed);
}

TrialFound Trial::found() const {
  TrialFound out;
  out.steps = steps_.load(std::memory_order_relaxed);
  out.readings = readings_.load(std::memory_order_relaxed);
  const std::uint64_t first = first_us_.load(std::memory_order_relaxed);
  const std::uint64_t last = last_us_.load(std::memory_order_relaxed);
  out.elapsed_ms = last > first ? (last - first) / 1000 : 0;
  out.reading_p50_us = reading_us_.percentile_us(500);
  out.reading_p99_us = reading_us_.percentile_us(990);
  out.reading_max_us = reading_us_.max_us();
  out.copies = reader_.total_copies();
  for (std::size_t k = 0; k < kFaultKinds; ++k) out.faults[k] = reader_.faults(static_cast<FaultKind>(k));
  const std::size_t n = count_.load(std::memory_order_acquire);
  for (std::size_t i = 0; i < n; ++i) {
    const Slot& s = slots_[i];
    TrialManager m;
    m.id = s.id;
    m.label = s.label;
    m.null = s.null.load(std::memory_order_relaxed);
    m.same = s.same.load(std::memory_order_relaxed);
    m.other = s.other.load(std::memory_order_relaxed);
    m.fault = s.fault.load(std::memory_order_relaxed);
    m.torn = s.torn.load(std::memory_order_relaxed);
    out.managers.push_back(std::move(m));
  }
  std::sort(out.managers.begin(), out.managers.end(),
            [](const TrialManager& a, const TrialManager& b) { return a.id < b.id; });
  return out;
}

}  // namespace en::gal::lodestar
