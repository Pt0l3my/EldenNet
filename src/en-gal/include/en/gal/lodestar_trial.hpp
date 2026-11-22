#pragma once
// The reader's trial of Lodestar's in-game round (step L5; proposal 0005 §6.2, §10; O101): the reader tried on the
// managers the round has just named. On the thread that will read in v1 (the frame service's own thread), about ten
// times a second, each manager's slot and its instance's first word are read through the guarded reader, and the slot
// read again: what was null, what still had its class, what had another, what faulted and what changed between the
// two reads are counted. It reads sixteen bytes a manager a reading, and writes nothing.
//
// It belongs to the part of en-gal that runs in the game (it holds a GuardedReader, whose shape depends on the probe
// option); its counts, TrialFound, are lodestar.hpp's, which the report writes.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include "en/gal/guarded.hpp"
#include "en/gal/health.hpp"
#include "en/gal/lodestar.hpp"
#include "en/gal/sys.hpp"

namespace en::gal::lodestar {

static_assert(kTrialFaultKinds == kFaultKinds, "lodestar.hpp's count of the reader's fault kinds is out of date");

// A task body for the frame service's own thread. Managers are added by the thread that names them, at any time; the
// reader's thread reads those added so far. Nothing is ever removed.
class Trial final : public TaskBody {
 public:
  static constexpr std::size_t kMaxManagers = 32;

  // `base` is the image's address as loaded; a manager's slot and table of methods are offsets from it.
  Trial(SysLayer& sys, std::uint64_t base, std::uint64_t period_us = 100'000) noexcept;

  // Adds a manager the round named (status derived); false when it is there already, or the trial is full.
  bool add(const ManagerFound& manager);

  void run(const proto::GameThread& gt, const FrameCtx& frame) override;

  // The counts so far, from any thread.
  [[nodiscard]] TrialFound found() const;
  [[nodiscard]] std::uint64_t readings() const noexcept { return readings_.load(std::memory_order_relaxed); }

 private:
  struct Slot {
    std::uint32_t id = 0;
    std::string label;
    std::uint64_t slot = 0;    // absolute
    std::uint64_t vtable = 0;  // absolute
    std::atomic<std::uint64_t> null{0};
    std::atomic<std::uint64_t> same{0};
    std::atomic<std::uint64_t> other{0};
    std::atomic<std::uint64_t> fault{0};
    std::atomic<std::uint64_t> torn{0};
  };

  SysLayer& sys_;
  GuardedReader reader_;
  std::uint64_t base_;
  std::uint64_t period_us_;
  std::array<Slot, kMaxManagers> slots_;
  std::atomic<std::size_t> count_{0};  // slots published to the reader's thread
  std::atomic<std::uint64_t> steps_{0};
  std::atomic<std::uint64_t> readings_{0};
  std::atomic<std::uint64_t> first_us_{0};
  std::atomic<std::uint64_t> last_us_{0};
  Histogram reading_us_;
  std::uint64_t next_us_ = 0;  // the reader's thread only
  bool bound_ = false;
};

}  // namespace en::gal::lodestar
