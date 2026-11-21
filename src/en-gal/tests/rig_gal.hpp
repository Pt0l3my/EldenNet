#pragma once
// The in-game part's test rig (part 4): a FakeWorld discovered as T1 would discover it, and a TaskIo of fixed slots
// that records what the task sends without allocating on the game thread.
#include <doctest.h>

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "en/gal/discovery.hpp"
#include "en/gal/task.hpp"
#include "fake_world.hpp"

namespace en::gal::test {

inline Discovery discovered(FakeWorld& w, const DiscoveryOptions& options = {}) {
  auto gated = gate_image(*w.sys, w.builds);
  REQUIRE(gated.has_value());
  auto d = discover(*w.sys, *gated, w.patterns, w.layout, options);
  if (!d) {
    for (const auto& e : d.error()) MESSAGE(describe(e));
  }
  REQUIRE(d.has_value());
  return std::move(*d);
}

// A TaskIo of fixed slots: one snapshot being built, the last one published kept, a command ring, a lifecycle log.
class FakeIo final : public TaskIo {
 public:
  FakeIo() : building_(std::make_unique<proto::WorldSnapshot>()), last_(std::make_unique<proto::WorldSnapshot>()) {}

  bool pop_command(proto::GameCommand& out) noexcept override {
    if (cmd_head_ == cmd_tail_) return false;
    out = std::move(commands_[cmd_head_ % commands_.size()]);
    ++cmd_head_;
    return true;
  }
  void push_command(proto::GameCommand c) {
    REQUIRE(cmd_tail_ - cmd_head_ < commands_.size());
    commands_[cmd_tail_ % commands_.size()] = std::move(c);
    ++cmd_tail_;
  }
  proto::WorldSnapshot* begin_snapshot() noexcept override {
    if (no_slot) return nullptr;
    ++begun;
    return building_.get();
  }
  void end_snapshot(bool built) noexcept override {
    if (!built) {
      ++dropped;
      return;
    }
    ++published;
    ++published_this_tick;
    *last_ = *building_;
  }
  bool push_lifecycle(const proto::Lifecycle& l) noexcept override {
    if (life_count >= life.size()) return false;
    life[life_count++] = l;
    return true;
  }
  bool retire(std::shared_ptr<const std::vector<std::uint32_t>>& list) noexcept override {
    if (retired_count >= retired.size()) return false;
    retired[retired_count++] = std::move(list);
    return true;
  }
  void push_render(const proto::RenderFeed& feed) noexcept override {
    ++renders;
    render = feed;
  }

  const proto::WorldSnapshot& last() const { return *last_; }
  template <class K>
  std::size_t count_life() const {
    std::size_t n = 0;
    for (std::size_t i = 0; i < life_count; ++i) n += std::holds_alternative<K>(life[i].payload) ? 1 : 0;
    return n;
  }

  bool no_slot = false;
  std::uint64_t begun = 0, published = 0, dropped = 0, renders = 0, published_this_tick = 0;
  std::array<proto::Lifecycle, 64> life{};
  std::size_t life_count = 0;
  std::array<std::shared_ptr<const std::vector<std::uint32_t>>, 16> retired{};
  std::size_t retired_count = 0;
  proto::RenderFeed render;

 private:
  std::unique_ptr<proto::WorldSnapshot> building_;
  std::unique_ptr<proto::WorldSnapshot> last_;
  std::array<proto::GameCommand, 32> commands_{};
  std::size_t cmd_head_ = 0, cmd_tail_ = 0;
};

// A FakeWorld discovered, its task registered and ticked to Running.
struct Running {
  explicit Running(const WorldOptions& o = {}, const TaskOptions& t = {}, const BackendOptions& b = {})
      : w(o), d(discovered(w)), task(*w.sys, d, io, t, b) {
    REQUIRE(d.ready());
    REQUIRE(install_task(*w.sys, d, task).has_value());
  }
  // Ticks until the task leaves phase A (at most 10 ticks of `dt_us`).
  void start(std::uint64_t dt_us = 16'667) {
    for (int i = 0; i < 10 && task.status().state.load() != TaskState::Running &&
                    task.status().state.load() != TaskState::Inert;
         ++i) {
      w.sys->tick(dt_us);
    }
  }
  // Ticks for `seconds` of game time at `fps`, with the world's play time advancing.
  void run(double seconds, double fps, bool watch = false) {
    const auto dt = static_cast<std::uint64_t>(1e6 / fps);
    const auto n = static_cast<std::uint64_t>(seconds * fps);
    for (std::uint64_t i = 0; i < n; ++i) {
      w.advance_play(static_cast<std::uint32_t>(dt / 1000));
      w.sys->tick(dt, watch);
    }
  }

  FakeWorld w;
  Discovery d;
  FakeIo io;
  GalTask task;
};

}  // namespace en::gal::test
