#pragma once
// The read groups' health (ARCH §6.7, §6.8; R8) and the task's counters (C13): a breaker per group (three hard faults
// in a sliding 60 s window trip it, for the session, with no half-open state), the groups' states, and a fixed
// 64-bucket histogram of µs. The breakers and states live on the game thread; the counters and histograms are atomics,
// written there and read by any thread (the debug view, the status block). Nothing allocates.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "en/proto/snapshot.hpp"

namespace en::gal {

inline constexpr std::uint32_t kTripFaults = 3;
inline constexpr std::uint64_t kTripWindowMs = 60'000;

class Breaker {
 public:
  // Records a hard fault at `now_ms` (a monotonic clock); answers whether this one tripped the breaker.
  bool fault(std::uint64_t now_ms) noexcept;
  [[nodiscard]] bool tripped() const noexcept { return tripped_; }
  [[nodiscard]] std::uint32_t total() const noexcept { return total_; }

 private:
  std::array<std::uint64_t, kTripFaults - 1> last_{};  // the latest faults' times, newest first
  std::uint32_t held_ = 0;
  std::uint32_t total_ = 0;
  bool tripped_ = false;
};

// C13's histogram: buckets 0 to 3 hold 0 to 3 µs, then four buckets an octave (4 to 4.99 µs, 5 to 5.99, ...), the last
// open-ended (from about 115 ms).
class Histogram {
 public:
  static constexpr std::size_t kBuckets = 64;
  [[nodiscard]] static std::size_t bucket_of(std::uint64_t us) noexcept;
  [[nodiscard]] static std::uint64_t upper_of(std::size_t bucket) noexcept;

  void record(std::uint64_t us) noexcept;
  [[nodiscard]] std::uint64_t count() const noexcept;
  // The upper bound of the bucket holding the `permille`-th value (990 for p99), 0 with no values.
  [[nodiscard]] std::uint64_t percentile_us(std::uint32_t permille) const noexcept;
  [[nodiscard]] std::uint64_t max_us() const noexcept { return max_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint64_t sum_us() const noexcept { return sum_.load(std::memory_order_relaxed); }
  [[nodiscard]] std::uint32_t in_bucket(std::size_t bucket) const noexcept;

 private:
  std::array<std::atomic<std::uint32_t>, kBuckets> buckets_{};
  std::atomic<std::uint64_t> max_{0};
  std::atomic<std::uint64_t> sum_{0};
};

// What made a group's state what it is.
enum class GroupWhy : std::uint8_t {
  None,
  NotDiscovered,  // its items or its layout are missing (ARCH §3.2: an optional item missing turns its group off)
  PhaseA,         // a phase-A self-test failed
  PhaseB,         // a phase-B self-test failed
  Breaker,        // three hard faults in 60 s
  Off,            // off by design (status, until S8)
};
[[nodiscard]] const char* to_string(GroupWhy w) noexcept;

// The groups' states and breakers, on the game thread.
class GroupBook {
 public:
  [[nodiscard]] proto::GroupState state(proto::Group g) const noexcept { return at(g).state; }
  [[nodiscard]] GroupWhy why(proto::Group g) const noexcept { return at(g).why; }
  // Whether the group is read this tick: discovered and not tripped (Untested groups are read for their tests).
  [[nodiscard]] bool live(proto::Group g) const noexcept { return at(g).state != proto::GroupState::Tripped; }
  void set_ok(proto::Group g) noexcept;
  // Takes the group off for the session; answers whether it was live before.
  bool trip(proto::Group g, GroupWhy why) noexcept;
  void set_phase_b(proto::Group g, bool passed) noexcept { at(g).phase_b = passed; }
  // A hard fault; answers whether it tripped the group's breaker now.
  bool hard_fault(proto::Group g, std::uint64_t now_ms) noexcept;
  void begin_tick() noexcept;
  [[nodiscard]] std::uint32_t breaker_total(proto::Group g) const noexcept { return at(g).breaker.total(); }
  // ReadHealth's groups (ARCH §7.1).
  void fill(proto::ReadHealth& out) const noexcept;

 private:
  struct Entry {
    proto::GroupState state = proto::GroupState::Untested;
    GroupWhy why = GroupWhy::None;
    bool phase_b = false;
    std::uint8_t faults = 0;  // this tick, saturating
    Breaker breaker;
  };
  [[nodiscard]] Entry& at(proto::Group g) noexcept { return e_[static_cast<std::size_t>(g)]; }
  [[nodiscard]] const Entry& at(proto::Group g) const noexcept { return e_[static_cast<std::size_t>(g)]; }
  std::array<Entry, proto::kGroupCount> e_{};
};

}  // namespace en::gal
