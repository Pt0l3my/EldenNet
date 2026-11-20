// The detectors and the debounce modifier (detectors.hpp; ARCH §8.4).
#include "en/augur/detectors.hpp"

#include <algorithm>

namespace en::augur {

namespace {

bool to_allows(const DetectorSpec& spec, const Value& v) {
  if (spec.to.empty() && !spec.to_none) return true;
  if (v.state == Value::State::None) return spec.to_none;
  return std::find(spec.to.begin(), spec.to.end(), v.v) != spec.to.end();
}

// The window's extreme over [max(t − window, base), t]: the maximum for a drop, the minimum for a rise.
double window_extreme(const DetectorSpec& spec, const DetState& st, Mono t) {
  const Mono from = std::max(t >= spec.window_ms ? t - spec.window_ms : Mono{0}, st.base);
  const bool drop = spec.kind == DetectorKind::DropOverWindow;
  double best = 0;
  bool any = false;
  for (const WindowSample& w : st.window) {
    if (w.t < from || w.t > t) continue;
    if (!any || (drop ? w.x > best : w.x < best)) best = w.x;
    any = true;
  }
  return best;
}

double window_amount(const DetectorSpec& spec, const DetState& st, Mono t, double x) {
  const double e = window_extreme(spec, st, t);
  return spec.kind == DetectorKind::DropOverWindow ? e - x : x - e;
}

// Whether a candidate's condition still holds on this sample (debounce: "the post-edge value, below the threshold,
// inside the radius").
bool holds(const DetectorSpec& spec, const DetState& st, Mono t, const Value& x) {
  switch (spec.kind) {
    case DetectorKind::RisingEdge:
      return x.truth();
    case DetectorKind::FallingEdge:
      return x.present() && !x.truth();
    case DetectorKind::ChangedStable:
      return x.same(st.cand_x);
    case DetectorKind::CrossesBelow:
      return x.present() && x.v < spec.value;
    case DetectorKind::CrossesAbove:
      return x.present() && x.v > spec.value;
    case DetectorKind::DropOverWindow:
    case DetectorKind::RiseOverWindow:
      return x.present() && window_amount(spec, st, t, x.v) >= spec.value;
    case DetectorKind::EnterRadius:
      return x.present() && x.v <= spec.value;
    case DetectorKind::CountIncrement:
      return x.present() && st.cand_x.present() && x.v >= st.cand_x.v;
    case DetectorKind::Engagement:
    case DetectorKind::Fact:
      return true;
  }
  return false;
}

// The fire matures: the arm state is consumed now, never at the candidate.
Fire mature(const DetectorSpec& spec, DetState& st, Mono t_c, Mono t, const Value& x, const Value& prev) {
  Fire f{t_c, t, x, prev, 0};
  switch (spec.kind) {
    case DetectorKind::ChangedStable:
      f.prev = st.last;
      st.last = x;
      break;
    case DetectorKind::CrossesBelow:
    case DetectorKind::CrossesAbove:
    case DetectorKind::EnterRadius:
      st.armed = false;
      break;
    case DetectorKind::DropOverWindow:
    case DetectorKind::RiseOverWindow:
      f.amount = window_amount(spec, st, t, x.v);
      st.base = t;
      break;
    case DetectorKind::CountIncrement:
      f.amount = x.v - prev.v;
      break;
    default:
      break;
  }
  return f;
}

void first_sample(const DetectorSpec& spec, DetState& st, Mono t, const Value& x) {
  st.seen = true;
  st.prev = x;
  switch (spec.kind) {
    case DetectorKind::ChangedStable:
      st.last = x;
      st.pending = x;
      st.since = t;
      break;
    case DetectorKind::CrossesBelow:
      st.armed = x.present() && x.v >= spec.value;
      break;
    case DetectorKind::CrossesAbove:
      st.armed = x.present() && x.v <= spec.value;
      break;
    case DetectorKind::DropOverWindow:
    case DetectorKind::RiseOverWindow:
      st.base = t;
      st.window.clear();
      if (x.present()) st.window.push_back(WindowSample{t, x.v});
      break;
    case DetectorKind::EnterRadius:
      st.armed = true;
      st.inside = x.present() && x.v <= spec.value;
      st.inside_since = t;
      break;
    default:
      break;
  }
}

// Whether this sample triggers a fire (before debounce), with this sample's tracking applied.
bool trigger(const DetectorSpec& spec, DetState& st, Mono t, const Value& x) {
  switch (spec.kind) {
    case DetectorKind::RisingEdge:
      return st.prev.present() && !st.prev.truth() && x.truth();
    case DetectorKind::FallingEdge:
      return st.prev.truth() && x.present() && !x.truth();
    case DetectorKind::ChangedStable:
      // Any change restarts the pending value; it fires once it has held stable_ms and differs from `last`.
      if (!x.same(st.pending)) {
        st.pending = x;
        st.since = t;
      }
      return !st.pending.same(st.last) && t - st.since >= spec.stable_ms;
    case DetectorKind::CrossesBelow: {
      const bool fire = st.armed && x.present() && x.v < spec.value;
      if (x.present() && x.v > spec.rearm) st.armed = true;
      return fire;
    }
    case DetectorKind::CrossesAbove: {
      const bool fire = st.armed && x.present() && x.v > spec.value;
      if (x.present() && x.v < spec.rearm) st.armed = true;
      return fire;
    }
    case DetectorKind::DropOverWindow:
    case DetectorKind::RiseOverWindow: {
      if (!x.present()) return false;
      st.window.push_back(WindowSample{t, x.v});
      // Keep only what a later window can still read: from max(t − window, base), which never moves back.
      const Mono keep_from = std::max(t >= spec.window_ms ? t - spec.window_ms : Mono{0}, st.base);
      const auto old = std::find_if(st.window.begin(), st.window.end(),
                                    [&](const WindowSample& w) { return w.t >= keep_from; });
      st.window.erase(st.window.begin(), old);
      if (st.window.size() > 256) st.window.erase(st.window.begin());  // a hard bound (impl)
      return window_amount(spec, st, t, x.v) >= spec.value;
    }
    case DetectorKind::EnterRadius: {
      const bool in = x.present() && x.v <= spec.value;
      if (in && !st.inside) st.inside_since = t;
      st.inside = in;
      if (x.present() && x.v > spec.rearm) st.armed = true;
      return st.armed && st.inside && t - st.inside_since >= spec.window_ms;
    }
    case DetectorKind::CountIncrement:
      return st.prev.present() && x.present() && x.v > st.prev.v;
    case DetectorKind::Engagement:
    case DetectorKind::Fact:
      return false;
  }
  return false;
}

}  // namespace

std::optional<Fire> detect(const DetectorSpec& spec, Mono debounce_ms, DetState& st, Mono t, const Value& x) {
  if (!x.sampled()) return std::nullopt;
  if (!st.seen) {
    first_sample(spec, st, t, x);
    return std::nullopt;
  }
  std::optional<Fire> out;
  const Value prev = st.prev;
  // A candidate waiting for its debounce: cancelled when its condition fails on any sample, emitted once it has held.
  if (st.cand) {
    if (!holds(spec, st, t, x)) {
      st.cand = false;
    } else if (t - st.cand_t >= debounce_ms) {
      out = mature(spec, st, st.cand_t, t, x, st.cand_prev);
      st.cand = false;
    }
  }
  // This sample's own trigger; while a candidate waits the tracking goes on, but no second candidate starts.
  const bool fired = trigger(spec, st, t, x);
  if (fired && !out && !st.cand) {
    if (debounce_ms == 0) {
      out = mature(spec, st, t, t, x, prev);
    } else {
      st.cand = true;
      st.cand_t = t;
      st.cand_x = x;
      st.cand_prev = prev;
    }
  }
  st.prev = x;
  if (out && spec.kind == DetectorKind::ChangedStable && !to_allows(spec, out->x)) return std::nullopt;
  return out;
}

void on_loading(const DetectorSpec& spec, DetState& st) noexcept {
  // A candidate waits on: a boss fight's end, debounced, still matures on the loading screen that follows it.
  if (spec.kind == DetectorKind::EnterRadius) {
    st.armed = true;
    st.inside = false;
  }
}

}  // namespace en::augur
