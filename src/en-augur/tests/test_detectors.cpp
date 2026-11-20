// The detectors (ARCH §8.4) one by one, and the seeded property tests of §8.12 that are theirs: hysteresis, debounce
// and gaps. Every property prints its seed on failure.
#include <doctest.h>

#include <vector>

#include "en/augur/detectors.hpp"
#include "helpers.hpp"

namespace en::augur {
namespace {

using test::Rng;

struct S {
  Mono t;
  Value x;
};

std::vector<Fire> run(const DetectorSpec& spec, Mono debounce, const std::vector<S>& samples) {
  DetState st;
  std::vector<Fire> out;
  for (const S& s : samples) {
    if (auto f = detect(spec, debounce, st, s.t, s.x)) out.push_back(*f);
  }
  return out;
}

std::vector<S> bools(std::initializer_list<int> v, Mono step = 250) {
  std::vector<S> out;
  Mono t = 0;
  for (const int b : v) {
    out.push_back(S{t, Value::of(b != 0)});
    t += step;
  }
  return out;
}

std::vector<S> nums(std::initializer_list<double> v, Mono step = 250) {
  std::vector<S> out;
  Mono t = 0;
  for (const double x : v) {
    out.push_back(S{t, Value::some(x)});
    t += step;
  }
  return out;
}

DetectorSpec spec(DetectorKind k) {
  DetectorSpec s;
  s.kind = k;
  return s;
}

TEST_CASE("no detector fires on a subject's first sample") {
  CHECK(run(spec(DetectorKind::RisingEdge), 0, bools({1, 1})).empty());
  CHECK(run(spec(DetectorKind::CountIncrement), 0, nums({5, 5})).empty());
  DetectorSpec r = spec(DetectorKind::EnterRadius);
  r.value = 10;
  r.rearm = 30;
  r.window_ms = 0;
  CHECK(run(r, 0, nums({2})).empty());
  CHECK(run(r, 0, nums({2, 2})).size() == 1);  // dwell 0: on the second sample
  DetectorSpec c = spec(DetectorKind::CrossesBelow);
  c.value = 0.25;
  c.rearm = 0.35;
  CHECK(run(c, 0, nums({0.1, 0.1})).empty());  // starts below: not armed
}

TEST_CASE("edges, and unsampled values change nothing") {
  CHECK(run(spec(DetectorKind::RisingEdge), 0, bools({0, 1, 1, 0, 1})).size() == 2);
  CHECK(run(spec(DetectorKind::FallingEdge), 0, bools({1, 0, 0, 1, 0})).size() == 2);
  std::vector<S> gappy = bools({0, 0, 1});
  gappy.insert(gappy.begin() + 2, S{300, Value{}});  // unsampled between
  CHECK(run(spec(DetectorKind::RisingEdge), 0, gappy).size() == 1);
}

TEST_CASE("changed_stable: a change that holds for stable_ms, and the `to` filter") {
  DetectorSpec s = spec(DetectorKind::ChangedStable);
  s.stable_ms = 1000;
  // 5 then 7 for 500 ms, back to 5 (no fire), then 7 for 1 s (fires once, at its fifth sample).
  std::vector<S> v = nums({5, 7, 7, 7, 5, 7, 7, 7, 7, 7, 7});
  const auto f = run(s, 0, v);
  REQUIRE(f.size() == 1);
  CHECK(f[0].x.v == 7);
  CHECK(f[0].prev.v == 5);
  CHECK(f[0].t == 2250);
  DetectorSpec to = spec(DetectorKind::ChangedStable);
  to.to = {5};
  CHECK(run(to, 0, std::vector<S>{{0, Value::none()}, {250, Value::some(3)}, {500, Value::some(5)},
                                  {750, Value::none()}, {1000, Value::some(5)}})
            .size() == 2);
  // none is a value of its own
  DetectorSpec any = spec(DetectorKind::ChangedStable);
  CHECK(run(any, 0, std::vector<S>{{0, Value::some(1)}, {250, Value::none()}, {500, Value::some(1)}}).size() == 2);
}

TEST_CASE("crosses_below and crosses_above: hysteresis") {
  DetectorSpec b = spec(DetectorKind::CrossesBelow);
  b.value = 0.25;
  b.rearm = 0.35;
  CHECK(run(b, 0, nums({1.0, 0.2, 0.3, 0.2, 0.4, 0.2})).size() == 2);  // 0.3 does not re-arm, 0.4 does
  DetectorSpec a = spec(DetectorKind::CrossesAbove);
  a.value = 0.8;
  a.rearm = 0.5;
  CHECK(run(a, 0, nums({0.1, 0.9, 0.6, 0.9, 0.4, 0.95})).size() == 2);
}

TEST_CASE("windows: a drop over the window, the base reset on each fire") {
  DetectorSpec d = spec(DetectorKind::DropOverWindow);
  d.value = 0.001;
  d.window_ms = 1000;
  const auto f = run(d, 0, nums({1.0, 0.9, 0.9, 0.7, 0.7, 0.71, 0.6}));
  REQUIRE(f.size() == 3);
  CHECK(f[0].amount == doctest::Approx(0.1));
  CHECK(f[1].amount == doctest::Approx(0.2));
  CHECK(f[2].amount == doctest::Approx(0.11));
  // A drop across a gap longer than the window is not seen (only samples inside the window count).
  CHECK(run(d, 0, std::vector<S>{{0, Value::some(1.0)}, {1500, Value::some(0.5)}}).empty());
  DetectorSpec r = spec(DetectorKind::RiseOverWindow);
  r.value = 0.2;
  r.window_ms = 1000;
  CHECK(run(r, 0, nums({0.2, 0.3, 0.45, 0.5, 0.5})).size() == 1);
  CHECK(run(r, 0, nums({0.2, 0.25, 0.3, 0.35, 0.4, 0.45}, 500)).empty());  // too slow
}

TEST_CASE("enter_radius: dwell, re-arm beyond rearm_m, and loading") {
  DetectorSpec r = spec(DetectorKind::EnterRadius);
  r.value = 10;
  r.rearm = 30;
  r.window_ms = 2000;
  // inside for 2 s (fires once), out to 20 m (no re-arm), back in, out beyond 30, back in for 2 s.
  const auto f = run(r, 0, nums({50, 9, 9, 9, 9, 9, 9, 9, 9, 20, 9, 9, 9, 9, 9, 9, 9, 9, 9, 31, 8, 8, 8, 8, 8, 8, 8, 8, 8}));
  CHECK(f.size() == 2);
  DetState st;
  for (Mono t = 0; t <= 2500; t += 250) (void)detect(r, 0, st, t, Value::some(5));
  CHECK_FALSE(st.armed);
  on_loading(r, st);
  CHECK(st.armed);
}

TEST_CASE("count_increment: the step as the amount") {
  const auto f = run(spec(DetectorKind::CountIncrement), 0, nums({3, 3, 5, 5, 4, 6}));
  REQUIRE(f.size() == 2);
  CHECK(f[0].amount == 2);
  CHECK(f[1].amount == 2);
}

TEST_CASE("debounce: emitted once the condition held, cancelled otherwise with the arm state untouched") {
  DetectorSpec b = spec(DetectorKind::CrossesBelow);
  b.value = 0.25;
  b.rearm = 0.35;
  // Below for 250 ms then back above: cancelled; the arm is not consumed, so the next crossing fires.
  const auto f = run(b, 500, nums({1.0, 0.2, 0.3, 0.2, 0.2, 0.2}));
  REQUIRE(f.size() == 1);
  CHECK(f[0].t_c == 750);
  CHECK(f[0].t == 1250);
  DetectorSpec fall = spec(DetectorKind::FallingEdge);
  const auto g = run(fall, 1000, bools({1, 0, 0, 0, 0, 0}));
  REQUIRE(g.size() == 1);
  CHECK(g[0].t - g[0].t_c >= 1000);
}

// ---- Seeded properties (§8.12) ----

TEST_CASE("property: hysteresis: never two fires without a re-arm crossing between them") {
  for (std::uint64_t seed = 1; seed <= 200; ++seed) {
    CAPTURE(seed);
    Rng rng(seed);
    DetectorSpec b = spec(DetectorKind::CrossesBelow);
    b.value = 0.25 + rng.unit() * 0.2;
    b.rearm = b.value + 0.05 + rng.unit() * 0.2;
    const Mono debounce = rng.chance(0.5) ? 0 : 250 * rng.below(4);
    std::vector<S> v;
    double x = rng.unit();
    for (Mono t = 0; t < 30000; t += 250) {
      x = std::clamp(x + (rng.unit() - 0.5) * 0.3, 0.0, 1.0);
      v.push_back(S{t, Value::some(x)});
    }
    const auto fires = run(b, debounce, v);
    for (std::size_t i = 1; i < fires.size(); ++i) {
      bool rearmed = false;
      for (const S& s : v) {
        if (s.t > fires[i - 1].t && s.t <= fires[i].t_c && s.x.v > b.rearm) rearmed = true;
      }
      CHECK(rearmed);
    }
  }
}

TEST_CASE("property: debounce: no emission before the condition has held for debounce_ms") {
  for (std::uint64_t seed = 1; seed <= 200; ++seed) {
    CAPTURE(seed);
    Rng rng(seed);
    const Mono debounce = 250 * (1 + rng.below(8));
    std::vector<S> v;
    bool x = false;
    for (Mono t = 0; t < 30000; t += 250) {
      if (rng.chance(0.2)) x = !x;
      v.push_back(S{t, Value::of(x)});
    }
    for (const DetectorKind k : {DetectorKind::RisingEdge, DetectorKind::FallingEdge}) {
      for (const Fire& f : run(spec(k), debounce, v)) {
        CHECK(f.t - f.t_c >= debounce);
        for (const S& s : v) {
          if (s.t >= f.t_c && s.t <= f.t) CHECK(s.x.truth() == (k == DetectorKind::RisingEdge));
        }
      }
    }
  }
}

TEST_CASE("property: gaps: dropping snapshots never creates an edge, and an edge across a gap fires once") {
  for (std::uint64_t seed = 1; seed <= 300; ++seed) {
    CAPTURE(seed);
    Rng rng(seed);
    std::vector<S> dense;
    bool x = rng.chance(0.5);
    double n = 0;
    for (Mono t = 0; t < 20000; t += 250) {
      if (rng.chance(0.15)) x = !x;
      dense.push_back(S{t, Value::of(x)});
    }
    std::vector<S> kept;
    for (const S& s : dense) {
      if (rng.chance(0.5)) kept.push_back(s);
    }
    // The edges of the kept snapshots, exactly: each between two kept samples whose values differ upwards.
    std::size_t edges = 0;
    for (std::size_t i = 1; i < kept.size(); ++i) {
      if (!kept[i - 1].x.truth() && kept[i].x.truth()) ++edges;
    }
    CHECK(run(spec(DetectorKind::RisingEdge), 0, kept).size() == edges);
    CHECK(run(spec(DetectorKind::RisingEdge), 0, kept).size() <= run(spec(DetectorKind::RisingEdge), 0, dense).size());
    // A count that rises across a gap fires once, with the whole step.
    std::vector<S> counts;
    for (const S& s : dense) {
      if (rng.chance(0.1)) n += 1 + static_cast<double>(rng.below(3));
      counts.push_back(S{s.t, Value::some(n)});
    }
    std::vector<S> kept_counts;
    for (std::size_t i = 0; i < counts.size(); ++i) {
      if (i == 0 || rng.chance(0.4)) kept_counts.push_back(counts[i]);
    }
    double total = 0;
    for (const Fire& f : run(spec(DetectorKind::CountIncrement), 0, kept_counts)) total += f.amount;
    CHECK(total == kept_counts.back().x.v - kept_counts.front().x.v);
    // Hysteresis on a subsequence never fires more than on the dense run.
    DetectorSpec b = spec(DetectorKind::CrossesBelow);
    b.value = 0.3;
    b.rearm = 0.5;
    std::vector<S> hp, hp_kept;
    double h = 1;
    for (const S& s : dense) {
      h = std::clamp(h + (rng.unit() - 0.5) * 0.4, 0.0, 1.0);
      hp.push_back(S{s.t, Value::some(h)});
      if (rng.chance(0.5)) hp_kept.push_back(hp.back());
    }
    CHECK(run(b, 0, hp_kept).size() <= run(b, 0, hp).size());
  }
}

}  // namespace
}  // namespace en::augur
