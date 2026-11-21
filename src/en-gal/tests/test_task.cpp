// The task over FakeSys (task.hpp; ARCH §3.2, §6.4, §7.3; R6, R10): registration once with the layout's argument
// registers, the cadence at 30, 60 and 144 FPS with hitches (always 4 Hz, never a burst), the throttle in and out,
// the command cap, no allocation on the game thread after warm-up, and an exception that escapes the body.
#include <doctest.h>

#include <stdexcept>

#include "en/proto/ffi_guard.hpp"
#include "rig_gal.hpp"

using namespace en::gal;
using namespace en::gal::test;
using en::proto::GameCommand;

namespace {

// Runs `seconds` at `fps`, with a hitch of `hitch_us` every `hitch_every` frames; answers the most snapshots any one
// invocation published and the most consecutive invocations that each published one.
struct Cadence {
  std::uint64_t most_in_one = 0;
  std::uint64_t longest_run = 0;
  std::uint64_t snapshots = 0;
};
Cadence drive(Running& r, double seconds, double fps, std::uint64_t hitch_every = 0, std::uint64_t hitch_us = 0) {
  Cadence c;
  const auto dt = static_cast<std::uint64_t>(1e6 / fps);
  std::uint64_t elapsed = 0, frame = 0, run = 0;
  const std::uint64_t before = r.io.published;
  while (elapsed < static_cast<std::uint64_t>(seconds * 1e6)) {
    ++frame;
    const std::uint64_t step = hitch_every != 0 && frame % hitch_every == 0 ? hitch_us : dt;
    elapsed += step;
    r.w.advance_play(static_cast<std::uint32_t>(step / 1000));
    r.io.published_this_tick = 0;
    r.w.sys->tick(step);
    c.most_in_one = std::max(c.most_in_one, r.io.published_this_tick);
    run = r.io.published_this_tick > 0 ? run + 1 : 0;
    c.longest_run = std::max(c.longest_run, run);
  }
  c.snapshots = r.io.published - before;
  return c;
}

}  // namespace

TEST_CASE("task: registration once, with the layout's shape and argument registers") {
  FakeWorld w;
  const Discovery d = discovered(w);
  FakeIo io;
  GalTask task(*w.sys, d, io);
  REQUIRE(install_task(*w.sys, d, task).has_value());
  REQUIRE(w.sys->registration.has_value());
  const auto& reg = *w.sys->registration;
  CHECK(reg.function == w.at("register_task"));
  CHECK(reg.instance == w.instance("task_hub"));
  CHECK(reg.shape.abi == RegisterAbi{ArgRegister::R8, ArgRegister::Rcx, ArgRegister::Rdx});
  CHECK(reg.shape.vtable_slots == 3);
  CHECK(reg.shape.run_slot == 1);
  CHECK(reg.shape.object_size == 0x10);
  CHECK(reg.shape.group_index == 2);
  CHECK(install_task(*w.sys, d, task).error() == SysError::Twice);  // never twice
  w.sys->tick(16'667);
  CHECK(w.sys->slot_calls(1) == 1);
  CHECK(w.sys->slot_calls(0) == 0);

  FakeWorld f;
  const Discovery fd = discovered(f);
  f.sys->fault_register = true;
  GalTask t2(*f.sys, fd, io);
  CHECK(install_task(*f.sys, fd, t2).error() == SysError::Faulted);
  // The hub's static null: nothing to register with.
  FakeWorld n;
  const Discovery nd = discovered(n);
  n.mem.put<std::uint64_t>(n.at("st_task_hub"), 0);
  GalTask t3(*n.sys, nd, io);
  CHECK(install_task(*n.sys, nd, t3).error() == SysError::Unreadable);
}

TEST_CASE("task: 4 Hz at 30, 60 and 144 FPS, a flag frame on every fourth, never two in one invocation (R10)") {
  for (const double fps : {30.0, 60.0, 144.0}) {
    INFO(fps);
    Running r;
    r.start();
    r.w.world();
    const Cadence c = drive(r, 60.0, fps);
    CHECK(c.snapshots >= 238);
    CHECK(c.snapshots <= 241);
    CHECK(c.most_in_one == 1);
    CHECK(c.longest_run == 1);
    CHECK(r.task.status().flag_frames.load() >= c.snapshots / 4 - 1);
    CHECK(r.task.status().flag_frames.load() <= c.snapshots / 4 + 1);
    CHECK(r.task.status().build_us.count() == r.task.status().snapshots.load() + r.task.status().abandoned.load());
  }
}

TEST_CASE("task: hitches of up to a second never cause a burst (R10)") {
  for (const double fps : {30.0, 60.0, 144.0}) {
    INFO(fps);
    Running r;
    r.start();
    r.w.world();
    // A 1 s hitch every 5 s of frames, and a 3 s stall once: a hitch counts for at most 1 s.
    const Cadence c = drive(r, 60.0, fps, static_cast<std::uint64_t>(5 * fps), 1'000'000);
    CHECK(c.most_in_one == 1);
    CHECK(c.longest_run <= 2);
    CHECK(c.snapshots <= 241);
    CHECK(c.snapshots >= 200);
    const Cadence stall = drive(r, 8.0, fps, static_cast<std::uint64_t>(2 * fps), 3'000'000);
    CHECK(stall.most_in_one == 1);
    CHECK(stall.longest_run <= 2);
  }
}

TEST_CASE("task: three slow builds throttle to 2 Hz and 16 characters; 60 s of fast ones end it (R10)") {
  Running r(WorldOptions{true, 20});
  r.start();
  r.w.world();
  drive(r, 2.0, 60);
  CHECK_FALSE(r.task.status().throttled.load());
  CHECK(r.io.last().chars.size() == 10);  // within 40 m
  r.w.sys->clock_step_us = 600;  // every clock reading moves 0.6 ms: each build takes more than 0.5 ms
  drive(r, 1.0, 60);
  CHECK(r.task.status().throttled.load());
  CHECK(r.task.status().throttle_entries.load() == 1);
  const Cadence slow = drive(r, 20.0, 60);
  CHECK(slow.snapshots >= 39);
  CHECK(slow.snapshots <= 41);
  CHECK(r.io.last().chars.size() == 6);  // within 25 m
  r.w.sys->clock_step_us = 0;
  drive(r, 59.0, 60);
  CHECK(r.task.status().throttled.load());
  drive(r, 2.0, 60);
  CHECK_FALSE(r.task.status().throttled.load());
  const Cadence fast = drive(r, 10.0, 60);
  CHECK(fast.snapshots >= 39);
}

TEST_CASE("task: at most 16 commands an invocation") {
  Running r;
  r.start();
  for (int i = 0; i < 20; ++i) r.io.push_command(GameCommand{});
  r.w.sys->tick(16'667);
  CHECK(r.task.status().commands.load() == 16);
  r.w.sys->tick(16'667);
  CHECK(r.task.status().commands.load() == 20);
}

TEST_CASE("task: nothing on the game thread allocates or frees after warm-up (R6)") {
  Running r;
  r.start();
  r.w.world();
  GameCommand::SetTracked tracked;
  REQUIRE(tracked.handles.try_push_back(en::proto::ChrHandle{r.w.npc_handles[0]}));
  r.io.push_command(GameCommand{tracked});
  r.run(1.0, 60);  // warm-up: the first snapshots
  AllocWatch::news = 0;
  AllocWatch::deletes = 0;
  r.io.push_command(GameCommand{GameCommand::SetWatchFlags{
      3, std::make_shared<const std::vector<std::uint32_t>>(std::vector<std::uint32_t>{100, 1234001, 5000005})}});
  GameCommand::ResolveText text;
  text.fmg = 6;
  REQUIRE(text.ids.try_push_back(8001));
  r.io.push_command(GameCommand{text});
  GameCommand::ResolveParams params;
  REQUIRE(params.ids.try_push_back(4000));
  r.io.push_command(GameCommand{params});
  r.run(10.0, 60, true);
  r.w.loading();
  r.run(1.0, 60, true);
  r.w.world();
  r.run(1.0, 60, true);
  r.w.move_all(Move::Poison);
  r.run(1.0, 60, true);
  CHECK(r.io.published >= 50);
  CHECK(r.io.renders >= 600);
  CHECK(AllocWatch::news.load() == 0);
  CHECK(AllocWatch::deletes.load() == 0);
}

TEST_CASE("task: an exception out of the body trips the task's guard, and every later call returns at once (R3)") {
  struct Thrower final : TaskBody {
    void run(const en::proto::GameThread&, const FrameCtx&) override {
      ++calls;
      throw std::runtime_error("invented");
    }
    int calls = 0;
  } body;
  en::proto::clear_trip(en::proto::Component::Task);
  run_task(body, FrameCtx{1, 1});
  CHECK(en::proto::tripped(en::proto::Component::Task));
  run_task(body, FrameCtx{2, 1});
  CHECK(body.calls == 1);
  en::proto::clear_trip(en::proto::Component::Task);
}
