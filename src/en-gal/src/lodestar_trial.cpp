// The reader's trial of Lodestar's in-game round (lodestar_trial.hpp).
#include "en/gal/lodestar_trial.hpp"

#include <algorithm>
#include <cstring>
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
  const std::uint32_t stage_id = stage_id_.load(std::memory_order_relaxed);
  std::array<bool, kMaxManagers> empty{};
  int stage = -1;  // unknown until the stage's manager is read
  for (std::size_t i = 0; i < n; ++i) {
    Slot& s = slots_[i];
    const auto bump = [](std::atomic<std::uint64_t>& counter) { counter.fetch_add(1, std::memory_order_relaxed); };
    // The slot, the instance's first word, then the slot again: a slot that changed in between judges nothing.
    const auto first = reader_.copy<std::uint64_t>(gt, proto::Group::Session, s.slot);
    if (!first) {
      bump(s.fault);
      continue;
    }
    if (s.id == stage_id) stage = *first != 0 ? 1 : 0;
    if (*first == 0) {
      bump(s.null);
      empty[i] = true;
      if (s.run++ == 0) bump(s.null_runs);
      if (s.run > s.null_longest.load(std::memory_order_relaxed)) s.null_longest.store(s.run, std::memory_order_relaxed);
      continue;
    }
    s.run = 0;
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
  if (stage == 1) {
    world_readings_.fetch_add(1, std::memory_order_relaxed);
    for (std::size_t i = 0; i < n; ++i) {
      if (empty[i]) slots_[i].null_world.fetch_add(1, std::memory_order_relaxed);
    }
  }
  if (stage != -1) {
    if (last_stage_ != -1 && stage != last_stage_) stage_changes_.fetch_add(1, std::memory_order_relaxed);
    last_stage_ = stage;
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
  out.world_readings = world_readings_.load(std::memory_order_relaxed);
  out.stage_changes = stage_changes_.load(std::memory_order_relaxed);
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
    m.null_runs = s.null_runs.load(std::memory_order_relaxed);
    m.null_longest = s.null_longest.load(std::memory_order_relaxed);
    m.null_world = s.null_world.load(std::memory_order_relaxed);
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

// ---- The clock search ----

ClockSearch::ClockSearch(std::vector<std::uint32_t> statics, std::uint64_t base, std::uint64_t size)
    : base_(base), size_(size) {
  if (statics.size() > kMaxCandidates) statics.resize(kMaxCandidates);
  candidates_.resize(statics.size());
  for (std::size_t i = 0; i < statics.size(); ++i) candidates_[i].rva = statics[i];
}

void ClockSearch::compare(Candidate& c, std::span<const std::byte> now) noexcept {
  const auto value = [](std::span<const std::byte> bytes, std::size_t field) -> std::uint32_t {
    if (field < kSpan) return static_cast<std::uint32_t>(bytes[field]);
    std::uint32_t v = 0;
    std::memcpy(&v, bytes.data() + (field - kSpan) * 4, 4);
    return v;
  };
  for (std::size_t f = 0; f < kFields; ++f) {
    const std::size_t end = f < kSpan ? f + 1 : (f - kSpan) * 4 + 4;
    if (end > now.size()) continue;  // past what this object's copy holds
    Field& field = c.fields[f];
    const std::uint32_t v = value(now, f);
    if (v >= 60) field.minute_dead = 1;
    if (v >= 24) field.hour_dead = 1;
    if (!c.seen) continue;
    const std::uint32_t was = value(c.last, f);
    if (field.minute_dead == 0 && was < 60) {
      const std::uint32_t d = (v + 60 - was) % 60;
      if (d >= 1 && d <= 5) {
        if (field.ticks < 0xFFFF) ++field.ticks;
        if (v < was && field.wraps < 0xFFFF) ++field.wraps;
      } else if (d != 0) {
        field.minute_dead = 1;  // a jump, or a step back: no clock's minutes
      }
    }
    if (field.hour_dead == 0 && was < 24) {
      const std::uint32_t d = (v + 24 - was) % 24;
      if (d == 1) {
        if (field.hour_steps < 0xFFFF) ++field.hour_steps;
      } else if (d != 0) {
        field.hour_dead = 1;
      }
    }
  }
}

bool ClockSearch::step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies,
                       std::uint64_t now_us) {
  bool ended = false;
  for (std::uint32_t done = 0; done < copies && !candidates_.empty(); ++done) {
    Candidate& c = candidates_[next_];
    if (!c.dead && (c.last_us == 0 || now_us >= c.last_us + kMinIntervalUs)) {
      c.last_us = now_us;
      read_in_sweep_ = true;
      const auto slot = reader.copy<std::uint64_t>(gt, proto::Group::Session, base_ + c.rva);
      const std::uint64_t instance = slot ? *slot : 0;
      // No object, or one that lies in the image (the clock is a heap object): nothing to compare this time.
      const bool outside = instance != 0 && (instance < base_ || instance - base_ >= size_);
      if (!outside) {
        c.seen = false;
      } else {
        if (instance != c.instance) c.seen = false;  // another object: its first copy is compared with nothing
        std::array<std::byte, kSpan> now{};
        bool copied = false;
        while (!copied) {
          copied = reader.copy_into(gt, proto::Group::Session, instance, std::span<std::byte>(now).first(c.span), 8)
                       .has_value();
          if (copied) break;
          if (reader.spent() || c.span <= 0x10) break;
          c.span = c.span > 0x40 ? 0x40 : c.span > 0x20 ? 0x20 : 0x10;  // a smaller object: its end is nearer
          c.seen = false;
        }
        if (!copied) {
          if (!reader.spent()) {
            c.dead = true;
            unreadable_.fetch_add(1, std::memory_order_relaxed);
          }
        } else {
          compare(c, std::span<const std::byte>(now).first(c.span));
          c.last = now;
          c.instance = instance;
          c.seen = true;
        }
      }
    }
    if (++next_ >= candidates_.size()) {
      next_ = 0;
      if (read_in_sweep_) {
        read_in_sweep_ = false;
        ended = true;
        publish();
        passes_.fetch_add(1, std::memory_order_release);
      }
      break;
    }
  }
  return ended;
}

void ClockSearch::publish() noexcept {
  std::array<ClockHit, kMaxHits> best{};
  std::size_t n = 0;
  for (const Candidate& c : candidates_) {
    for (std::size_t f = 0; f < kFields; ++f) {
      const Field& field = c.fields[f];
      if (field.minute_dead != 0 || field.ticks < kMinTicks) continue;
      ClockHit hit;
      hit.rva = c.rva;
      hit.width = f < kSpan ? 1 : 4;
      hit.offset = static_cast<std::uint32_t>(f < kSpan ? f : (f - kSpan) * 4);
      hit.ticks = field.ticks;
      hit.wraps = field.wraps;
      // The hours: a field of the same size right beside it, before or after, that behaved as hours do.
      const std::size_t first = f < kSpan ? 0 : kSpan;
      const std::size_t count = f < kSpan ? kSpan : kSpan / 4;
      for (const int step : {-1, 1}) {
        const std::size_t at = f - first;
        if ((step < 0 && at == 0) || (step > 0 && at + 1 >= count)) continue;
        const std::size_t g = f + static_cast<std::size_t>(step);
        const Field& hour = c.fields[g];
        if (hour.hour_dead != 0 || hour.hour_steps == 0 || hit.has_hour) continue;
        hit.has_hour = true;
        hit.hour_offset = static_cast<std::uint32_t>(g < kSpan ? g : (g - kSpan) * 4);
        hit.hour_steps = hour.hour_steps;
      }
      // Kept in order: a clock (with its hours) first, then the most ticks.
      const auto better = [](const ClockHit& a, const ClockHit& b) {
        return a.has_hour != b.has_hour ? a.has_hour : a.ticks > b.ticks;
      };
      std::size_t at = n < kMaxHits ? n : kMaxHits;
      while (at > 0 && better(hit, best[at - 1])) --at;
      if (at >= kMaxHits) continue;
      for (std::size_t i = (n < kMaxHits ? n : kMaxHits - 1); i > at; --i) best[i] = best[i - 1];
      best[at] = hit;
      if (n < kMaxHits) ++n;
    }
  }
  hit_count_.store(0, std::memory_order_release);
  hits_ = best;
  hit_count_.store(n, std::memory_order_release);
}

ClockFound ClockSearch::found() const {
  ClockFound out;
  out.candidates = candidates_.size();
  out.passes = passes_.load(std::memory_order_acquire);
  out.unreadable = unreadable_.load(std::memory_order_relaxed);
  const std::size_t n = hit_count_.load(std::memory_order_acquire);
  out.hits.assign(hits_.begin(), hits_.begin() + static_cast<std::ptrdiff_t>(n < kMaxHits ? n : kMaxHits));
  return out;
}

}  // namespace en::gal::lodestar
