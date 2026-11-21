// en_augur_bench: Augur::step on a busy synthetic step (ARCH §8.10's rates and costs). The shipped rule set, the
// snapshot full (32 characters: hostile ones closing in and trading hits, a boss bar, the Tarnished hit and locked on),
// and a steady 4 Hz. Prints the mean, p50, p99 and the most per step, in µs, and the rule set's load time.
//
//   en_augur_bench [steps]       (default 20000; run a Release build for the README's numbers)
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "helpers.hpp"

int main(int argc, char** argv) {
  using namespace en::augur;
  namespace proto = en::proto;
  using Clock = std::chrono::steady_clock;
  const std::size_t steps = argc > 1 ? static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10)) : 20000;

  const auto files = test::shipped_files();
  const test::TestCompiler compiler;
  const auto l0 = Clock::now();
  auto set = RuleSet::load(files, compiler);
  const auto l1 = Clock::now();
  if (!set) {
    std::fputs("bench: the shipped rules do not load\n", stderr);
    return 1;
  }
  test::Rng rng(42);
  // A ring of busy snapshots, built up front so that only Augur::step is timed.
  std::vector<proto::WorldSnapshot> ring;
  std::vector<std::array<float, 3>> pos(32);
  std::vector<std::uint32_t> hp(32, 1000);
  for (std::size_t i = 0; i < 32; ++i) pos[i] = {static_cast<float>(5 + rng.below(35)), static_cast<float>(rng.below(20)), 0};
  std::uint32_t php = 1000;
  for (std::size_t k = 0; k < 400; ++k) {
    proto::WorldSnapshot s = test::snap(0, php);
    for (std::size_t i = 0; i < 32; ++i) {
      pos[i][0] = std::clamp(pos[i][0] + static_cast<float>(rng.unit() - 0.5) * 2.0f, 2.0f, 45.0f);
      if (rng.chance(0.2)) hp[i] = hp[i] > 40 ? hp[i] - 40 : 1000;  // hits; the fallen get back up (new fights)
      proto::Chr c = test::chr(0x1000 + i, pos[i], i % 5 == 0 ? 2 : 6, hp[i]);
      c.last_hit_by = proto::ChrHandle{test::kPlayer};
      (void)s.chars.try_push_back(c);
    }
    php = php > 30 ? php - static_cast<std::uint32_t>(rng.below(30)) : 1000;
    s.player->hp.cur = php;
    s.player->last_hit_by = proto::ChrHandle{0x1000 + rng.below(32)};
    s.player->locked_on = proto::ChrHandle{0x1000 + rng.below(32)};
    s.boss.active = true;
    (void)s.boss.bars.try_push_back(proto::BossBar{proto::ChrHandle{0x1000}, 1, 0});
    ring.push_back(s);
  }
  Augur augur(test::facts());
  augur.set_rules(*set);
  test::TestCtx ctx;
  std::vector<double> us;
  us.reserve(steps);
  EventBatch out;
  std::size_t events = 0;
  for (std::size_t i = 0; i < steps; ++i) {
    proto::WorldSnapshot& s = ring[i % ring.size()];
    s.mono_ms = 250 * (i + 1);
    s.player->play_time_ms = 100000 + s.mono_ms;
    out.clear();
    const auto t0 = Clock::now();
    augur.step(AugurInput{&s, {}}, s.mono_ms, ctx, out);
    const auto t1 = Clock::now();
    us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    events += out.size();
  }
  std::vector<double> sorted = us;
  std::sort(sorted.begin(), sorted.end());
  double sum = 0;
  for (const double v : us) sum += v;
  const auto at = [&](double q) { return sorted[std::min(sorted.size() - 1, static_cast<std::size_t>(q * static_cast<double>(sorted.size())))]; };
  std::printf("bench: %zu rules, %zu steps of 32 characters at 4 Hz, %.2f events a step\n", (*set)->rules().size(), steps,
              static_cast<double>(events) / static_cast<double>(steps));
  std::printf("bench: Augur::step mean %.1f us, p50 %.1f us, p99 %.1f us, most %.1f us\n", sum / static_cast<double>(steps),
              at(0.50), at(0.99), sorted.back());
  std::printf("bench: the rule set loads in %.0f us\n", std::chrono::duration<double, std::micro>(l1 - l0).count());
  return 0;
}
