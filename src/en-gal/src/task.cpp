// The task (task.hpp; ARCH §3.2, §6.4, §7.3): the trampoline's body call, the one place in en-gal that issues the game
// thread's token (R1's CI grep allows GameThread::issue here and in en-tools' replay alone), the cadence with its
// throttle, the first tick, phase A, and registration.
#include "en/gal/task.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "en/proto/ffi_guard.hpp"

namespace en::gal {

using proto::GameThread;

void run_task(TaskBody& body, const FrameCtx& frame) noexcept {
  if (proto::tripped(proto::Component::Task)) return;  // an exception escaped once: inert for the session (R3)
  proto::ffi_guard(proto::Component::Task, [&] {
    const GameThread gt = GameThread::issue();
    body.run(gt, frame);
  });
}

TaskOptions own_thread_options() noexcept {
  TaskOptions o;
  o.provider = FrameProvider::OwnThread;
  o.period_us = 100'000;            // about ten snapshots a second
  o.throttled_period_us = 200'000;  // five while throttled
  return o;
}

BackendOptions own_thread_backend(const BackendOptions& base) noexcept {
  BackendOptions o = base;
  o.steady = true;
  return o;
}

std::uint64_t ReaderLoop::step() noexcept {
  const std::uint64_t now = sys_.now_us();
  const std::uint64_t dt = last_us_ == 0 || now < last_us_ ? 0 : now - last_us_;
  last_us_ = now;
  ++steps_;
  run_task(body_, FrameCtx{now, static_cast<std::uint32_t>(dt > 0xFFFF'FFFFu ? 0xFFFF'FFFFu : dt)});
  const std::uint64_t took = sys_.now_us() - now;
  return took + 1'000 >= tick_us_ ? 1'000 : tick_us_ - took;
}

const char* to_string(TaskState s) noexcept {
  switch (s) {
    case TaskState::Registered: return "registered";
    case TaskState::PhaseA: return "phase_a";
    case TaskState::Running: return "running";
    case TaskState::Inert: return "inert";
  }
  return "registered";
}

const char* to_string(InertWhy w) noexcept {
  switch (w) {
    case InertWhy::None: return "none";
    case InertWhy::GroupMismatch: return "group_mismatch";
    case InertWhy::PhaseA: return "phase_a";
    case InertWhy::Structural: return "structural";
    case InertWhy::Tripped: return "tripped";
  }
  return "none";
}

GalTask::GalTask(SysLayer& sys, const Discovery& discovery, TaskIo& io, const TaskOptions& options,
                 const BackendOptions& backend)
    : sys_(sys), d_(discovery), io_(io), opts_(options), backend_(sys, discovery, backend) {}

void GalTask::go_inert(InertWhy why) noexcept {
  backend_.make_inert();
  status_.inert_why.store(why, std::memory_order_relaxed);
  status_.state.store(TaskState::Inert, std::memory_order_release);
}

void GalTask::run(const GameThread& gt, const FrameCtx& frame) {
  const TaskState state = status_.state.load(std::memory_order_relaxed);
  if (state == TaskState::Inert) return;  // after one load: zero reads (L1)
  status_.invocations.fetch_add(1, std::memory_order_relaxed);
  const std::uint64_t t0 = sys_.now_us();
  switch (state) {
    case TaskState::Registered: {
      // The first tick: the group at the layout's index must carry the layout's name (ARCH §3.2's FirstTick).
      status_.first_tick_us.store(frame.qpc_now_us, std::memory_order_relaxed);
      const GroupCheck check = backend_.first_tick(gt, opts_.provider == FrameProvider::GameTask);
      status_.group_ok.store(check.ok, std::memory_order_relaxed);
      if (check.found_at) status_.group_found_at.store(static_cast<std::int32_t>(*check.found_at), std::memory_order_relaxed);
      last_us_ = frame.qpc_now_us;
      if (!check.ok) {
        go_inert(InertWhy::GroupMismatch);
        return;
      }
      status_.state.store(TaskState::PhaseA, std::memory_order_release);
      break;
    }
    case TaskState::PhaseA: {
      const PhaseA r = backend_.phase_a(gt);
      last_us_ = frame.qpc_now_us;
      flush(gt);
      if (r == PhaseA::Failed) {
        go_inert(InertWhy::PhaseA);
        return;
      }
      if (r == PhaseA::Passed) {
        (void)io_.push_lifecycle(proto::Lifecycle{proto::Lifecycle::DataReady{d_.profile().key(), backend_.probe(gt)}});
        status_.state.store(TaskState::Running, std::memory_order_release);
      }
      break;
    }
    case TaskState::Running:
      tick_running(gt, frame);
      break;
    case TaskState::Inert:
      return;
  }
  for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
    status_.groups[g].store(backend_.book().state(static_cast<proto::Group>(g)), std::memory_order_relaxed);
  }
  status_.task_us.record(sys_.now_us() - t0);
}

void GalTask::tick_running(const GameThread& gt, const FrameCtx& frame) {
  // Commands first: at most 16 an invocation (ARCH §7.2).
  for (std::uint32_t n = 0; n < opts_.max_commands && io_.pop_command(cmd_); ++n) {
    status_.commands.fetch_add(1, std::memory_order_relaxed);
    const auto applied = backend_.apply(gt, cmd_);
    if (!applied) {
      // A watch list the backend could not take goes back to the core with the retired ones: never freed here (C5).
      if (auto* w = std::get_if<proto::GameCommand::SetWatchFlags>(&cmd_.payload); w != nullptr && w->ids) {
        auto ids = std::move(w->ids);
        if (!io_.retire(ids)) {
          const auto room = std::find_if(holdback_.begin(), holdback_.end(), [](const auto& h) { return !h; });
          if (room != holdback_.end()) *room = std::move(ids);
        }
      }
    }
  }
  // The render feed, every invocation while the core tracks anyone (ARCH §7.4).
  if (!backend_.tracked().empty()) {
    feed_.seq = status_.render_feeds.fetch_add(1, std::memory_order_relaxed) + 1;
    feed_.qpc_us = frame.qpc_now_us;
    if (backend_.render_feed(gt, backend_.tracked(), feed_)) io_.push_render(feed_);
  }
  // R10: time-based cadence. A hitch counts for at most 1 s, and at most one snapshot is built an invocation.
  const std::uint64_t now = frame.qpc_now_us;
  const std::uint64_t dt = std::min<std::uint64_t>(now >= last_us_ ? now - last_us_ : 0, opts_.max_dt_us);
  last_us_ = now;
  acc_us_ += dt;
  const std::uint64_t period = throttled_ ? opts_.throttled_period_us : opts_.period_us;
  if (acc_us_ >= period) {
    acc_us_ = std::min(acc_us_ - period, period);
    build(gt, frame);
  }
  flush(gt);
  if (backend_.inert()) go_inert(InertWhy::Structural);
}

void GalTask::build(const GameThread& gt, const FrameCtx& frame) {
  proto::WorldSnapshot* snap = io_.begin_snapshot();
  if (snap == nullptr) {
    status_.no_slot.fetch_add(1, std::memory_order_relaxed);
    gap_ = true;
    return;
  }
  ++seq_;
  const bool flag_frame = opts_.flag_frame_every != 0 && seq_ % opts_.flag_frame_every == 0;
  backend_.set_flag_frame_due(flag_frame);
  backend_.set_throttled(throttled_);
  const std::uint64_t t0 = sys_.now_us();
  const auto built = backend_.snapshot(gt, *snap);
  const std::uint64_t us = sys_.now_us() - t0;
  status_.build_us.record(us);
  // The throttle (R10): three slow builds in a row halve the rate and the cap; 60 s of builds all within the bound end
  // it.
  if (us > opts_.slow_build_us) {
    calm_since_us_ = frame.qpc_now_us;
    if (++slow_in_row_ >= opts_.slow_builds_to_throttle && !throttled_) {
      throttled_ = true;
      status_.throttle_entries.fetch_add(1, std::memory_order_relaxed);
    }
  } else {
    slow_in_row_ = 0;
    if (throttled_ && frame.qpc_now_us - calm_since_us_ >= opts_.calm_to_unthrottle_us) throttled_ = false;
  }
  status_.throttled.store(throttled_, std::memory_order_relaxed);
  status_.mode.store(backend_.last_mode(), std::memory_order_relaxed);
  status_.vtable_skips.store(backend_.vtable_skips(), std::memory_order_relaxed);
  status_.torn.store(backend_.steady().torn, std::memory_order_relaxed);
  status_.held.store(backend_.steady().held, std::memory_order_relaxed);
  status_.unconfirmed_faults.store(backend_.steady().unconfirmed, std::memory_order_relaxed);
  snap->seq = seq_;
  snap->mono_ms = frame.qpc_now_us / 1000;
  snap->build = d_.profile().key();
  snap->gap = gap_;
  snap->health.task_us_last = static_cast<std::uint32_t>(std::min<std::uint64_t>(us, 0xFFFF'FFFF));
  snap->health.task_us_p99 =
      static_cast<std::uint32_t>(std::min<std::uint64_t>(status_.task_us.percentile_us(990), 0xFFFF'FFFF));
  if (built) {
    gap_ = false;
    status_.snapshots.fetch_add(1, std::memory_order_relaxed);
    if (snap->flags) status_.flag_frames.fetch_add(1, std::memory_order_relaxed);
    if (backend_.last_mode() == ReadMode::Loading) status_.loading_only.fetch_add(1, std::memory_order_relaxed);
    if (backend_.last_mode() == ReadMode::Dormant) status_.dormant.fetch_add(1, std::memory_order_relaxed);
    io_.end_snapshot(true);
  } else {
    // Abandoned (the budget, or a structural fault): the next one carries gap (ARCH §6.5).
    gap_ = true;
    status_.abandoned.fetch_add(1, std::memory_order_relaxed);
    io_.end_snapshot(false);
  }
}

void GalTask::flush(const GameThread&) {
  proto::Lifecycle l;
  while (backend_.pop_lifecycle(l)) (void)io_.push_lifecycle(l);
  for (auto& h : holdback_) {
    if (h && !io_.retire(h)) return;
  }
  while (auto list = backend_.take_retired()) {
    if (!io_.retire(list)) {
      const auto room = std::find_if(holdback_.begin(), holdback_.end(), [](const auto& h) { return !h; });
      if (room == holdback_.end()) return;  // the backend keeps the rest until the ring drains
      *room = std::move(list);
    }
  }
}

proto::Expected<void, SysError> install_task(SysLayer& sys, const Discovery& discovery, TaskBody& body) {
  const auto& task = discovery.layout().task();
  const auto function = discovery.address(roles::Item::RegisterTask);
  const auto hub_static = discovery.address(roles::Item::TaskHub);
  if (!task || task->partial || !function || !hub_static) return proto::Unexpected{SysError::BadShape};
  // The hub's instance, from its static: a read of the image's own data section, on T1 (ARCH §1.2).
  std::array<std::byte, 8> bytes{};
  const std::uint64_t rva = *hub_static - discovery.module_base();
  if (!sys.image().read(rva, bytes)) return proto::Unexpected{SysError::Unreadable};
  std::uint64_t instance = 0;
  std::memcpy(&instance, bytes.data(), sizeof instance);
  if (instance == 0) return proto::Unexpected{SysError::Unreadable};
  const TaskShape shape{task->vtable_slots, task->run_slot, task->object_size, task->group_index, task->register_abi};
  const auto called = sys.register_task(*function, instance, shape, body);  // what it answers is R1's alone
  if (!called) return proto::Unexpected{called.error()};
  return {};
}

}  // namespace en::gal
