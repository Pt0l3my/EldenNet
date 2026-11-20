// The bus and the store requests' queue (bus.hpp; ARCH §8.9).
#include "en/augur/bus.hpp"

#include <algorithm>

#include "en/augur/augur.hpp"

namespace en::augur {

void BusCtx::emit(Event ev) {
  ev.id = augur_.next_id();
  ev.source = SourceId::Subscriber;
  ev.t_mono_ms = now_;
  augur_.resolve_audience(ev, ctx_);
  ev.interrupts = ev.interrupts | kind_interrupts(ev);
  emitted_.push_back(std::move(ev));
}

void Bus::add(Subscriber* s) {
  subs_.push_back(s);
  std::stable_sort(subs_.begin(), subs_.end(), [](const Subscriber* a, const Subscriber* b) { return a->id() < b->id(); });
}

void Bus::admit(std::deque<Event>& fifo, Event ev, BusStats& stats) {
  if (fifo.size() < kFifoCap) {
    fifo.push_back(std::move(ev));
    return;
  }
  const auto newest = [&](Class c) {
    for (auto it = fifo.rbegin(); it != fifo.rend(); ++it) {
      if (it->event_class == c) return std::next(it).base();
    }
    return fifo.end();
  };
  // T first, then K; P never.
  if (ev.event_class == Class::T) {
    ++stats.dropped_t;
    return;
  }
  if (auto t = newest(Class::T); t != fifo.end()) {
    fifo.erase(t);
    ++stats.dropped_t;
    fifo.push_back(std::move(ev));
    return;
  }
  if (ev.event_class == Class::K) {
    ++stats.dropped_k;
    return;
  }
  if (auto k = newest(Class::K); k != fifo.end()) {
    fifo.erase(k);
    ++stats.dropped_k;
    fifo.push_back(std::move(ev));
    return;
  }
  ++stats.over_cap_p;
  fifo.push_back(std::move(ev));
}

void Bus::run(EventBatch&& batch, Mono now, Augur& augur, const AugurCtx& ctx, EventBatch& dispatched) {
  std::deque<Event> fifo;
  for (Event& ev : batch) admit(fifo, std::move(ev), stats_);
  batch.clear();
  const auto guarded = [&](Subscriber* s, auto&& call) {
    const auto i = static_cast<std::size_t>(s->id());
    if (i >= kSubsystemCount || stats_.disabled[i]) return;
    try {
      call();
    } catch (...) {
      // R8: the first exception resets the subsystem, the second disables it for the session.
      if (stats_.exceptions[i] < 255) ++stats_.exceptions[i];
      if (stats_.exceptions[i] >= 2) {
        stats_.disabled[i] = true;
        return;
      }
      try {
        s->reset();
      } catch (...) {
        stats_.disabled[i] = true;
      }
    }
  };
  bool ended = false;
  for (;;) {
    while (!fifo.empty()) {
      Event ev = std::move(fifo.front());
      fifo.pop_front();
      BusCtx bctx(augur, ctx, now);
      for (Subscriber* s : subs_) guarded(s, [&] { s->on_event(ev, bctx); });
      for (Event& e : bctx.emitted()) admit(fifo, std::move(e), stats_);
      dispatched.push_back(std::move(ev));
    }
    if (ended) break;
    ended = true;
    BusCtx bctx(augur, ctx, now);
    for (Subscriber* s : subs_) guarded(s, [&] { s->on_step_end(now, bctx); });
    for (Event& e : bctx.emitted()) admit(fifo, std::move(e), stats_);
    if (fifo.empty()) break;
  }
}

// ---- PersistQueue ----

PersistQueue::Push PersistQueue::push(Item item) {
  if (item.kind == Kind::Diagnostic) {
    if (ring_.empty() && queue_.size() < limits_.queue) {
      queue_.push_back(item);
      return Push::Queued;
    }
    return Push::DroppedDiagnostic;
  }
  const bool speech = item.kind == Kind::Speech;
  if (!speech && paused_) {
    ++not_saved_;
    return Push::Paused;
  }
  if (ring_.empty()) {
    if (queue_.size() < limits_.queue) {
      queue_.push_back(item);
      return Push::Queued;
    }
    // Full: a queued diagnostic makes room first.
    for (auto it = queue_.rbegin(); it != queue_.rend(); ++it) {
      if (it->kind == Kind::Diagnostic) {
        queue_.erase(std::next(it).base());
        queue_.push_back(item);
        return Push::Queued;
      }
    }
  }
  const std::size_t non_speech = ring_.size() - ring_speech_;
  if (speech) {
    // Speech may take any free slot, the reserve included; past a full ring it is still kept (a shown line is never
    // lost: the chat waits while speech_blocked()).
    ring_.push_back(item);
    ++ring_speech_;
    return Push::Spilled;
  }
  if (non_speech < limits_.ring - limits_.speech_reserve && ring_.size() < limits_.ring) {
    ring_.push_back(item);
    return Push::Spilled;
  }
  paused_ = true;
  ++not_saved_;
  return Push::Paused;
}

void PersistQueue::refill() {
  while (queue_.size() < limits_.queue && !ring_.empty()) {
    if (ring_.front().kind == Kind::Speech) --ring_speech_;
    queue_.push_back(ring_.front());
    ring_.pop_front();
  }
}

bool PersistQueue::pop(Item& out) {
  refill();
  if (queue_.empty()) return false;
  out = queue_.front();
  queue_.pop_front();
  refill();
  if (paused_ && ring_.empty() && queue_.size() < limits_.resume_below) paused_ = false;
  return true;
}

}  // namespace en::augur
