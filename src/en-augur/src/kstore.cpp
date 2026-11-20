// The K store, the dedup table and the cooldown table (kstore.hpp; ARCH §8.4 item 4, §8.10).
#include "en/augur/kstore.hpp"

#include <algorithm>

namespace en::augur {

namespace {

// Aggregates a merged fire into the entry's event per kind; answers whether a salient field changed.
bool aggregate(Event& entry, const Event& fire) {
  if (auto* s = std::get_if<Struck>(&entry.kind)) {
    const auto* f = std::get_if<Struck>(&fire.kind);
    if (f == nullptr) return false;
    const bool salient = !s->hard && f->hard;  // hard turning false → true
    s->hard = s->hard || f->hard;
    s->frac_total += f->frac_total;  // sums
    if (f->att) s->att = f->att;     // the last attacker
    s->unseen = f->unseen;
    s->dmg_type = f->dmg_type;
    s->count = static_cast<std::uint16_t>(std::min<int>(s->count + 1, 0xFFFF));
    return salient;
  }
  entry.kind = fire.kind;  // other kinds: the latest payload, nothing salient
  return false;
}

}  // namespace

KStore::Result KStore::put(Event& ev, std::size_t rule, Mono now, std::uint32_t merge_ms, std::uint32_t ttl_s) {
  const std::string& key = ev.key ? *ev.key : std::string();
  const Mono expires = now + Mono{ttl_s} * 1000;
  auto it = std::find_if(entries_.begin(), entries_.end(), [&](const KEntry& e) { return e.key == key; });
  if (it != entries_.end() && merge_ms > 0 && it->rule == rule && it->t_first + merge_ms >= now) {
    // Coalesce: count + 1, payload aggregated, the expiry renewed; merging never delays the first event.
    const bool salient = aggregate(it->event, ev);
    it->event.count = static_cast<std::uint16_t>(std::min<int>(it->event.count + 1, 0xFFFF));
    it->t_last = now;
    it->expires = expires;
    it->event.ttl_ms = ttl_s * 1000;
    if (!salient) return Result{false, true};
    Event out = it->event;
    out.id = ev.id;
    out.t_mono_ms = ev.t_mono_ms;
    out.t_play_ms = ev.t_play_ms;
    out.merged = true;
    it->event.merged = true;
    ev = std::move(out);
    return Result{true, true};
  }
  if (it != entries_.end()) entries_.erase(it);
  if (entries_.size() >= kMaxKEntries) {
    // Past the cap, the earliest-expiring entry goes first.
    auto first = std::min_element(entries_.begin(), entries_.end(),
                                  [](const KEntry& a, const KEntry& b) { return a.expires < b.expires; });
    entries_.erase(first);
  }
  ev.ttl_ms = ttl_s * 1000;
  entries_.push_back(KEntry{key, rule, now, now, expires, ev});
  return Result{true, false};
}

void KStore::freeze(const Event& ev) {
  if (!ev.key) return;
  for (KEntry& e : entries_) {
    if (e.key == *ev.key && e.event.t_mono_ms == ev.t_mono_ms) {
      e.event.id = ev.id;
      e.event.audience = ev.audience;
    }
  }
}

void KStore::expire(Mono now) {
  entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const KEntry& e) { return now >= e.expires; }),
                 entries_.end());
}

std::vector<const KEntry*> KStore::just_now(PersonaId persona) const {
  std::vector<const KEntry*> out;
  for (auto it = entries_.rbegin(); it != entries_.rend() && out.size() < kMaxJustNow; ++it) {
    if (visible_to(it->event, persona)) out.push_back(&*it);
  }
  return out;
}

bool DedupTable::duplicate(const std::string& key, Mono t_play, std::uint32_t dedup_s) {
  const auto it = last_.find(key);
  if (it != last_.end() && t_play >= it->second && t_play - it->second < Mono{dedup_s} * 1000) return true;
  if (it == last_.end() && last_.size() >= kMaxDedupKeys) {
    auto oldest = std::min_element(last_.begin(), last_.end(),
                                   [](const auto& a, const auto& b) { return a.second < b.second; });
    last_.erase(oldest);
  }
  last_[key] = t_play;
  return false;
}

bool CooldownTable::cooling(std::size_t rule, std::uint64_t subject, Mono now, std::uint32_t cooldown_s) {
  if (cooldown_s == 0) return false;
  const auto k = std::make_pair(rule, subject);
  const auto it = last_.find(k);
  if (it != last_.end() && now >= it->second && now - it->second < Mono{cooldown_s} * 1000) return true;
  if (it == last_.end() && last_.size() >= kMaxCooldowns) {
    auto oldest = std::min_element(last_.begin(), last_.end(),
                                   [](const auto& a, const auto& b) { return a.second < b.second; });
    last_.erase(oldest);
  }
  last_[k] = now;
  return false;
}

}  // namespace en::augur
