#pragma once
// The detector semantics (ARCH §8.4): small state machines over the samples of one (rule, subject), at the processed
// snapshots in which the subject is present. No detector fires on a subject's first sample. The first modifier,
// debounce, is applied here too: a fire becomes a candidate, and is emitted (matures) only once its condition has held
// on every sample for debounce_ms; a cancelled candidate leaves the arm state as it was. The later modifiers
// (requires, cooldown, class handling, confidence) are the rule evaluator's (augur.cpp).
//
// Pure and allocation-light: a window detector keeps its window's samples in a small vector, the others a few words.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "en/augur/signals.hpp"
#include "en/augur/types.hpp"

namespace en::augur {

enum class DetectorKind : std::uint8_t {
  RisingEdge,
  FallingEdge,
  ChangedStable,
  CrossesBelow,
  CrossesAbove,
  DropOverWindow,
  RiseOverWindow,
  EnterRadius,
  CountIncrement,
  Engagement,  // the built-in machine (world.hpp): fires on its transitions
  Fact,        // a discrete fact of the Lifecycle source
};

struct DetectorSpec {
  DetectorKind kind = DetectorKind::RisingEdge;
  Mono stable_ms = 0;  // changed_stable
  double value = 0;    // crossings: the threshold; windows: the fraction; enter_radius: m
  double rearm = 0;    // crossings: rearm_above / rearm_below; enter_radius: rearm_m
  Mono window_ms = 0;  // windows; enter_radius: dwell_ms
  // changed_stable: the values that may fire (empty: any), and whether none may.
  std::vector<double> to;
  bool to_none = false;
  std::uint8_t on = 0;  // engagement: 0 on "engaged", 1 on "disengaged"; fact: its FactKind
};

// A fire that matured: when it became a candidate and when it was emitted, the value then, the value before the change
// (an edge's, a count's, changed_stable's previous `last`), and the amount (a window's drop or rise, a count's step).
struct Fire {
  Mono t_c = 0;
  Mono t = 0;
  Value x;
  Value prev;
  double amount = 0;
};

struct WindowSample {
  Mono t = 0;
  double x = 0;
};

// One (rule, subject)'s state.
struct DetState {
  bool seen = false;  // the first sample has been taken
  Value prev;         // the previous sample
  // changed_stable
  Value last;
  Value pending;
  Mono since = 0;
  // crossings, enter_radius
  bool armed = false;
  bool inside = false;
  Mono inside_since = 0;
  // windows
  Mono base = 0;
  std::vector<WindowSample> window;
  // debounce
  bool cand = false;
  Mono cand_t = 0;
  Value cand_x;
  Value cand_prev;
};

// Takes one sample; answers a fire that matured on it. Unsampled values change nothing.
[[nodiscard]] std::optional<Fire> detect(const DetectorSpec& spec, Mono debounce_ms, DetState& st, Mono t,
                                         const Value& x);

// A loading screen re-arms enter_radius (ARCH §8.4) and cancels any candidate.
void on_loading(const DetectorSpec& spec, DetState& st) noexcept;

}  // namespace en::augur
