// The frame service's own-thread provider (task.hpp's ReaderLoop; backend.hpp's steady reading; proposal 0005 §9.4 and
// §10; O101 (b)): the reading runs on a thread of our own, at a fixed rate, with no call into the game. Since the game
// may change what is being read, a reading whose structure changed while it was read is dropped; a change of the
// lifecycle is published at its second sighting; and a group's fault counts only when it comes twice running. On
// FakeSys, a process of the tests' own bytes, with the clock the tests set.
#include <doctest.h>

#include <cstdint>
#include <string>

#include "en/gal/backend.hpp"
#include "en/gal/discovery.hpp"
#include "en/gal/task.hpp"
#include "en/proto/lifecycle.hpp"
#include "fake_world.hpp"
#include "rig_gal.hpp"

using namespace en::gal;
using namespace en::gal::test;
using en::proto::Group;

namespace {

DiscoveryOptions own_thread_discovery() {
  DiscoveryOptions o;
  o.frame = FrameProvider::OwnThread;
  return o;
}

// A FakeWorld read from "a thread of our own": discovered for it, never registered with the fake game, stepped by the
// test with the clock moved between steps.
struct Reading {
  explicit Reading(bool steady = true, std::uint64_t tick_us = 33'333)
      : d(discovered(w, own_thread_discovery())),
        task(*w.sys, d, io, own_thread_options(), steady ? own_thread_backend() : BackendOptions{}),
        loop(*w.sys, task, tick_us),
        tick(tick_us) {
    REQUIRE(d.ready());
  }
  void steps(int n) {
    for (int i = 0; i < n; ++i) {
      now += tick;
      w.sys->set_now_us(now);
      w.advance_play(static_cast<std::uint32_t>(tick / 1000));
      (void)loop.step();
    }
  }
  // Steps until the task reads (past its first tick and phase A).
  void start() {
    for (int i = 0; i < 12 && task.status().state.load() != TaskState::Running &&
                    task.status().state.load() != TaskState::Inert;
         ++i) {
      steps(1);
    }
    REQUIRE(task.status().state.load() == TaskState::Running);
  }
  // Steps until `n` more readings have been tried (published or dropped).
  void readings(std::uint64_t n) {
    const std::uint64_t until = io.begun + n;
    for (int i = 0; i < 400 && io.begun < until; ++i) steps(1);
    REQUIRE(io.begun == until);
  }

  FakeWorld w;
  Discovery d;
  FakeIo io;
  GalTask task;
  ReaderLoop loop;
  std::uint64_t tick;
  std::uint64_t now = 1'000'000;
};

}  // namespace

TEST_CASE("frame: a reader on a thread of our own reads about ten times a second, and calls nothing of the game") {
  Reading r;
  r.start();
  r.w.world();
  const std::uint64_t before = r.io.published;
  r.steps(90);  // three seconds at about 30 steps a second
  const std::uint64_t made = r.io.published - before;
  CHECK(made >= 27);
  CHECK(made <= 31);
  CHECK(r.loop.steps() >= 90);
  CHECK(r.task.status().state.load() == TaskState::Running);
  CHECK(r.io.last().chars.size() == 6);
  CHECK(r.io.last().life.main_player);
  // Nothing was registered with the game and no getter was called: the reading is all there is.
  CHECK_FALSE(r.w.sys->registration.has_value());
  CHECK(r.w.sys->getter_calls == 0);
  // It is in no group of the game's: the first tick checked none.
  CHECK(r.task.status().group_ok.load());
  CHECK(r.task.status().torn.load() == 0);
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
    CHECK(r.task.backend().book().breaker_total(static_cast<Group>(g)) == 0);
  }
}

TEST_CASE("frame: discovery for a thread of our own needs no registration, no task hub and no [task] in the layout") {
  FakeWorld w;
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  // patterns.toml without the registration's pattern: refused for the game's task, read for a thread of our own.
  const std::size_t a = w.patterns.find("[[pattern]]\nid = \"register_task\"");
  REQUIRE(a != std::string::npos);
  const std::size_t b = w.patterns.find("[[pattern]]", a + 5);
  REQUIRE(b != std::string::npos);
  std::string no_register = w.patterns;
  no_register.erase(a, b - a);
  CHECK_FALSE(discover(*w.sys, *gated, no_register, w.layout).has_value());
  const auto d = discover(*w.sys, *gated, no_register, w.layout, own_thread_discovery());
  if (!d.has_value()) {
    for (const SchemaError& e : d.error()) MESSAGE(describe(e));
  }
  REQUIRE(d.has_value());
  CHECK(d->ready());
  CHECK(d->report().items[roles::index(roles::Item::RegisterTask)].status == ItemStatus::Missing);
  // And without the layout's [task]: the same.
  const std::size_t t = w.layout.find("[task]");
  REQUIRE(t != std::string::npos);
  const std::size_t after = w.layout.find("\n[", t + 1);
  REQUIRE(after != std::string::npos);
  std::string no_task = w.layout;
  no_task.erase(t, after + 1 - t);
  CHECK_FALSE(discover(*w.sys, *gated, w.patterns, no_task).has_value());
  const auto bare = discover(*w.sys, *gated, no_register, no_task, own_thread_discovery());
  if (!bare.has_value()) {
    for (const SchemaError& e : bare.error()) MESSAGE(describe(e));
  }
  REQUIRE(bare.has_value());
  CHECK(bare->ready());
}

TEST_CASE("frame: a reading whose structure changed while it was read is dropped, and counts against no group") {
  Reading r;
  r.start();
  r.w.world();
  r.readings(4);
  const std::uint64_t published = r.io.published;
  // In the middle of a reading, as the near list is copied, "the game" clears the characters' static: what the
  // reading rests on is no longer what it read.
  const std::uint64_t slot = r.w.at("st_characters");
  const std::uint64_t instance = r.w.instance("characters");
  bool fired = false;
  r.w.sys->fake_copier().before_copy = [&](std::uint64_t address) {
    if (!fired && address == r.w.near) {
      fired = true;
      r.w.mem.put<std::uint64_t>(slot, 0);
    }
  };
  r.readings(1);
  CHECK(fired);
  CHECK(r.task.status().torn.load() == 1);
  CHECK(r.io.published == published);  // nothing of that reading reached the core
  CHECK(r.io.dropped >= 1);
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
    CHECK(r.task.backend().book().breaker_total(static_cast<Group>(g)) == 0);
  }
  // The game puts it back: the next reading is whole, and says that the one before it was lost; the ones after do
  // not.
  r.w.sys->fake_copier().before_copy = nullptr;
  r.w.mem.put<std::uint64_t>(slot, instance);
  r.readings(1);
  CHECK(r.io.published == published + 1);
  CHECK(r.io.last().gap);
  CHECK(r.io.last().chars.size() == 6);
  r.readings(2);
  CHECK(r.io.published == published + 3);
  CHECK_FALSE(r.io.last().gap);
  CHECK(r.task.status().torn.load() == 1);
  CHECK(r.task.status().state.load() == TaskState::Running);
}

TEST_CASE("frame: without steady reading the same change is taken as it comes") {
  // The control: the game's own task reads in step with the frame, so nothing changes under it and nothing is read
  // twice. Here the same mid-reading change goes unnoticed by a reader that does not read steadily.
  Reading r(false);
  r.start();
  r.w.world();
  r.readings(4);
  const std::uint64_t slot = r.w.at("st_characters");
  const std::uint64_t instance = r.w.instance("characters");
  bool fired = false;
  r.w.sys->fake_copier().before_copy = [&](std::uint64_t address) {
    if (!fired && address == r.w.near) {
      fired = true;
      r.w.mem.put<std::uint64_t>(slot, 0);
    }
  };
  const std::uint64_t published = r.io.published;
  r.readings(1);
  CHECK(fired);
  CHECK(r.task.status().torn.load() == 0);
  CHECK(r.io.published == published + 1);
  r.w.mem.put<std::uint64_t>(slot, instance);
}

TEST_CASE("frame: a change of the lifecycle is published at its second sighting, and a blip of one reading never") {
  Reading r;
  r.start();
  r.w.world();
  r.readings(4);
  REQUIRE(r.io.last().life.main_player);
  const std::uint64_t held = r.task.status().held.load();
  // A loading screen: the first reading that shows it is dropped, the second publishes it.
  r.w.loading();
  std::uint64_t published = r.io.published;
  r.readings(1);
  CHECK(r.io.published == published);
  CHECK(r.task.status().held.load() == held + 1);
  CHECK(r.io.last().life.screen != en::proto::Screen::Loading);
  r.readings(1);
  CHECK(r.io.published == published + 1);
  CHECK(r.io.last().life.screen == en::proto::Screen::Loading);
  // Back in the world, the same way.
  r.w.world();
  published = r.io.published;
  r.readings(1);
  CHECK(r.io.published == published);
  r.readings(1);
  CHECK(r.io.published == published + 1);
  CHECK(r.io.last().life.screen == en::proto::Screen::InGame);
  CHECK(r.io.last().life.main_player);
  // A blip: one reading at a loading screen, then the world again. Nothing of it is published.
  const std::uint64_t held2 = r.task.status().held.load();
  r.w.loading();
  r.readings(1);
  r.w.world();
  published = r.io.published;
  r.readings(2);
  CHECK(r.task.status().held.load() == held2 + 1);
  CHECK(r.io.published == published + 2);
  CHECK(r.io.last().life.screen == en::proto::Screen::InGame);
  for (std::size_t g = 0; g < en::proto::kGroupCount; ++g) {
    CHECK(r.task.backend().book().breaker_total(static_cast<Group>(g)) == 0);
  }
}

TEST_CASE("frame: a group's fault counts only when it comes in two readings running") {
  Reading r;
  r.start();
  r.w.world();
  r.readings(4);
  // One reading with a lobby value the layout does not name, then the true one again: seen, not counted.
  r.w.set_lobby(9);
  r.readings(1);
  r.w.set_lobby(0);
  r.readings(2);
  CHECK(r.task.backend().book().breaker_total(Group::Session) == 0);
  CHECK(r.task.status().unconfirmed_faults.load() == 1);
  // Two readings running: the second counts.
  r.w.set_lobby(9);
  r.readings(2);
  CHECK(r.task.backend().book().breaker_total(Group::Session) == 1);
  r.w.set_lobby(0);
  r.readings(2);
  CHECK(r.task.backend().book().breaker_total(Group::Session) == 1);
  // The control, without steady reading: the first one counts at once.
  Reading plain(false);
  plain.start();
  plain.w.world();
  plain.readings(4);
  plain.w.set_lobby(9);
  plain.readings(1);
  CHECK(plain.task.backend().book().breaker_total(Group::Session) == 1);
}

TEST_CASE("frame: the loop waits the tick less what a step took, and a slow step starts no burst") {
  Reading r(true, 33'333);
  r.now += 33'333;
  r.w.sys->set_now_us(r.now);
  CHECK(r.loop.step() == 33'333);  // a step that took no time on the clock
  // A step during which the clock moves: every reading of the clock advances it.
  r.w.sys->clock_step_us = 2'000;
  r.now += 33'333;
  r.w.sys->set_now_us(r.now);
  const std::uint64_t wait = r.loop.step();
  CHECK(wait < 33'333);
  CHECK(wait >= 1'000);
  r.w.sys->clock_step_us = 40'000;  // a step slower than the tick
  r.now += 33'333;
  r.w.sys->set_now_us(r.now);
  CHECK(r.loop.step() == 1'000);
  // A tick under a millisecond is taken as a millisecond.
  ReaderLoop quick(*r.w.sys, r.task, 10);
  r.w.sys->clock_step_us = 0;
  CHECK(quick.step() == 1'000);
}

TEST_CASE("frame: the options for a thread of our own") {
  const TaskOptions t = own_thread_options();
  CHECK(t.provider == FrameProvider::OwnThread);
  CHECK(t.period_us == 100'000);
  CHECK(t.throttled_period_us == 200'000);
  CHECK(TaskOptions{}.provider == FrameProvider::GameTask);
  BackendOptions base;
  base.keep = 12;
  const BackendOptions b = own_thread_backend(base);
  CHECK(b.steady);
  CHECK(b.keep == 12);
  CHECK_FALSE(BackendOptions{}.steady);
}
