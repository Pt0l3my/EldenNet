#pragma once
// The task (ARCH §3.2's Register and FirstTick, §6.4, §7.3; R10, R11; C13): registration once through the derived
// register_task with its argument registers as data, and the body the trampoline runs on the game thread. Its first
// tick checks the group at the layout's index by name (else inert); phase A's self-tests follow over at most 8 ticks
// (a structural failure: inert); then each invocation drains at most 16 commands, stamps the render feed while the core
// tracks anyone, and builds a snapshot every 250 ms of accumulated monotonic time (500 ms while throttled), never more
// than one an invocation and never in a burst after a hitch, with a flag frame on every fourth. Three builds in a row
// over 0.5 ms throttle it; 60 s with every build at most 0.5 ms end the throttle. Frame counts are never used. Nothing
// it does on the game thread allocates or blocks after warm-up (R6); an inert task returns after one load.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "en/gal/backend.hpp"
#include "en/gal/discovery.hpp"
#include "en/gal/health.hpp"
#include "en/gal/sys.hpp"
#include "en/proto/command.hpp"
#include "en/proto/lifecycle.hpp"
#include "en/proto/render.hpp"
#include "en/proto/snapshot.hpp"

namespace en::gal {

// The task's channels to the core (ARCH §1.3: C1 to C5), which the DLL wires to its rings and the tests to fakes. Every
// call comes on the game thread; none may block or allocate.
class TaskIo {
 public:
  virtual ~TaskIo() = default;
  virtual bool pop_command(proto::GameCommand& out) noexcept = 0;   // C4
  virtual proto::WorldSnapshot* begin_snapshot() noexcept = 0;      // C1: a preallocated slot, or null when none is free
  virtual void end_snapshot(bool built) noexcept = 0;               // C1: publish it, or drop an abandoned one
  virtual bool push_lifecycle(const proto::Lifecycle& l) noexcept = 0;  // C2; false when full (the core rebuilds)
  // C5: a watch list a newer one replaced. On true the channel has taken it (`list` is left null); false when the ring
  // is full, and the task holds it back for the next tick: it never frees one on the game thread.
  virtual bool retire(std::shared_ptr<const std::vector<std::uint32_t>>& list) noexcept = 0;
  virtual void push_render(const proto::RenderFeed& feed) noexcept = 0;  // C3
};

struct TaskOptions {
  std::uint64_t period_us = 250'000;            // 4 Hz
  std::uint64_t throttled_period_us = 500'000;  // 2 Hz
  std::uint64_t max_dt_us = 1'000'000;          // a hitch never counts for more than 1 s
  std::uint64_t slow_build_us = 500;            // a build over this is slow
  std::uint32_t slow_builds_to_throttle = 3;
  std::uint64_t calm_to_unthrottle_us = 60'000'000;
  std::uint32_t max_commands = 16;              // per invocation (ARCH §7.2, impl)
  std::uint32_t flag_frame_every = 4;           // snapshots
};

enum class TaskState : std::uint8_t {
  Registered,  // waiting for the first tick
  PhaseA,      // the first ticks: phase A's self-tests
  Running,     // reading
  Inert,       // L1: the closure returns at once and reads nothing
};
[[nodiscard]] const char* to_string(TaskState s) noexcept;

enum class InertWhy : std::uint8_t { None, GroupMismatch, PhaseA, Structural, Tripped };
[[nodiscard]] const char* to_string(InertWhy w) noexcept;

// The task's status (C13 and the status block): written on the game thread, read by any.
struct TaskStatus {
  std::atomic<TaskState> state{TaskState::Registered};
  std::atomic<InertWhy> inert_why{InertWhy::None};
  std::atomic<bool> group_ok{false};
  std::atomic<std::int32_t> group_found_at{-1};  // where the layout's group name is, when not at its index
  std::atomic<std::uint64_t> invocations{0};
  std::atomic<std::uint64_t> first_tick_us{0};   // the clock at the first tick
  std::atomic<std::uint64_t> snapshots{0};       // built and published
  std::atomic<std::uint64_t> abandoned{0};       // the budget ran out, or a structural read faulted
  std::atomic<std::uint64_t> no_slot{0};         // C1 had no free slot
  std::atomic<std::uint64_t> flag_frames{0};
  std::atomic<std::uint64_t> loading_only{0};    // snapshots of the lifecycle subset (R11)
  std::atomic<std::uint64_t> dormant{0};
  std::atomic<std::uint64_t> commands{0};
  std::atomic<std::uint64_t> render_feeds{0};
  std::atomic<std::uint64_t> throttle_entries{0};
  std::atomic<bool> throttled{false};
  std::array<std::atomic<proto::GroupState>, proto::kGroupCount> groups{};  // each group's state, after every tick
  std::atomic<ReadMode> mode{ReadMode::Dormant};  // the last snapshot's
  std::atomic<std::uint64_t> vtable_skips{0};
  Histogram task_us;   // every invocation, µs
  Histogram build_us;  // every snapshot build, µs
};

class GalTask final : public TaskBody {
 public:
  GalTask(SysLayer& sys, const Discovery& discovery, TaskIo& io, const TaskOptions& options = {},
          const BackendOptions& backend = {});
  GalTask(const GalTask&) = delete;
  GalTask& operator=(const GalTask&) = delete;

  void run(const proto::GameThread& gt, const FrameCtx& frame) override;

  [[nodiscard]] const TaskStatus& status() const noexcept { return status_; }
  [[nodiscard]] ErBackend& backend() noexcept { return backend_; }
  [[nodiscard]] const ErBackend& backend() const noexcept { return backend_; }

 private:
  void go_inert(InertWhy why) noexcept;
  void tick_running(const proto::GameThread& gt, const FrameCtx& frame);
  void build(const proto::GameThread& gt, const FrameCtx& frame);
  void flush(const proto::GameThread& gt);

  SysLayer& sys_;
  const Discovery& d_;
  TaskIo& io_;
  TaskOptions opts_;
  ErBackend backend_;
  TaskStatus status_;
  std::uint64_t last_us_ = 0;
  std::uint64_t acc_us_ = 0;
  std::uint64_t seq_ = 0;
  bool gap_ = false;
  std::uint32_t slow_in_row_ = 0;
  bool throttled_ = false;
  std::uint64_t calm_since_us_ = 0;
  std::array<std::shared_ptr<const std::vector<std::uint32_t>>, 4> holdback_{};  // C5's holdback
  proto::GameCommand cmd_;  // the drain's one slot, kept so that taking a command allocates nothing
  proto::RenderFeed feed_;
};

// Registers the task (ARCH §3.2's Register): the task hub's instance read from its static through the image, then the
// SysLayer's one call into the game, with the layout's shape and argument registers. Called once, from T1.
[[nodiscard]] proto::Expected<void, SysError> install_task(SysLayer& sys, const Discovery& discovery, TaskBody& body);

}  // namespace en::gal
