// Augur's step: the rule evaluator, the modifiers, the payload builders and the audience (augur.hpp; ARCH §8.4,
// §8.5, §8.8, §8.9).
#include "en/augur/augur.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace en::augur {

namespace {

constexpr Mono kBossOutcomeWindowMs = 10000;  // Left only once 10 s pass without Victory or Death (§8.5)

std::string hex(std::uint64_t v) {
  char buf[24];
  std::snprintf(buf, sizeof buf, "%llx", static_cast<unsigned long long>(v));
  return buf;
}

// An actor as a key writes it: handles as #<hex>, which the golden files normalise.
std::string key_text(const ActorRef& a) {
  switch (a.kind) {
    case ActorRef::Kind::Player:
      return "player";
    case ActorRef::Kind::Persona:
      return "persona:" + std::to_string(a.id);
    case ActorRef::Kind::Card:
      return "card:" + std::to_string(a.id) + "#" + hex(a.handle.raw);
    case ActorRef::Kind::Mute:
      break;
  }
  return "#" + hex(a.handle.raw);
}

constexpr std::array<std::string_view, 11> kGroupWords = {"session", "world", "combat", "player", "flags", "text",
                                                          "ui",      "place", "camera", "params", "status"};
constexpr std::array<std::string_view, 5> kLevelWords = {"starting", "L1", "L2", "L3", "L4"};

struct Placeholders {
  std::array<std::pair<std::string_view, std::string>, 3> items{};
  std::size_t n = 0;
  void add(std::string_view name, std::string value) {
    if (n < items.size()) items[n++] = {name, std::move(value)};
  }
  [[nodiscard]] const std::string* find(std::string_view name) const {
    for (std::size_t i = 0; i < n; ++i) {
      if (items[i].first == name) return &items[i].second;
    }
    return nullptr;
  }
};

std::string render_key(const std::string& tmpl, const Placeholders& ph) {
  std::string out;
  std::size_t at = 0;
  while (at < tmpl.size()) {
    const std::size_t open = tmpl.find('{', at);
    if (open == std::string::npos) {
      out.append(tmpl, at, std::string::npos);
      break;
    }
    out.append(tmpl, at, open - at);
    const std::size_t close = tmpl.find('}', open);
    if (close == std::string::npos) break;  // the loader refused such a key
    const std::string* v = ph.find(std::string_view(tmpl).substr(open + 1, close - open - 1));
    out += v != nullptr ? *v : std::string("none");
    at = close + 1;
  }
  return out;
}

std::uint64_t subject_key(const SubjectRef& s) {
  switch (s.kind) {
    case SubjectRef::Kind::Chr:
      return s.handle.raw;
    case SubjectRef::Kind::Player:
      return 1ull << 63;
    case SubjectRef::Kind::World:
      break;
  }
  return (1ull << 63) | 1u;
}

float dist3(const std::array<float, 3>& a, const std::array<float, 3>& b) {
  const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

Augur::Augur(WorldFacts facts) : world_(std::move(facts)), sources_(make_sources(SignalSet{})) {}

void Augur::set_rules(std::shared_ptr<const RuleSet> rules) {
  rules_ = std::move(rules);
  for (auto& v : by_signal_) v.clear();
  SignalSet used;
  if (rules_) {
    for (const Rule& r : rules_->rules()) {
      by_signal_[static_cast<std::size_t>(r.signal)].push_back(r.index);
      used.set(static_cast<std::size_t>(r.signal));
    }
  }
  world_.set_rule_count(rules_ ? rules_->rules().size() : 0);
  sources_ = make_sources(used);
  cooldowns_.clear();
  deferred_.clear();
}

void Augur::step(const AugurInput& input, Mono now, const AugurCtx& ctx, EventBatch& out) {
  ++step_;
  ++stats_.steps;
  next_index_ = 0;
  const std::size_t first = out.size();
  // 1. The world model takes the input. A loading screen first clears what §9 clears: engagements, squads, the K
  // store (barks and the conversation are the subscribers').
  const bool loading = std::any_of(input.life.begin(), input.life.end(),
                                   [](const LifeFact& f) { return f.kind == FactKind::LoadingStarted; });
  if (loading) {
    world_.loading(rules_ ? rules_->rules() : std::span<const Rule>{});
    kstore_.clear();
  }
  kstore_.expire(now);
  if (input.snapshot != nullptr) world_.update(*input.snapshot, ctx);
  // 2 and 3. Each source in its order; its fires sorted and through the modifiers.
  Observations obs;
  std::vector<Pending> fires;
  for (const auto& src : sources_) {
    obs.clear();
    fires.clear();
    src->poll(input, now, world_, obs);
    if (rules_) evaluate(obs, now, ctx, fires);
    for (Pending& p : fires) p.source = src->id();
    emit_batch(fires, now, ctx, out);
    if (src->id() == SourceId::SnapshotDiff) resolve_deferred(now, false, ctx, out);
    if (src->id() == SourceId::Timer && !obs.facts.empty()) resolve_deferred(now, true, ctx, out);
  }
  // 4. Ids in dispatch order, and the audiences resolved and frozen.
  for (std::size_t i = first; i < out.size(); ++i) {
    out[i].id = EventId{step_, next_index_++};
    resolve_audience(out[i], ctx);
    if (out[i].event_class == Class::K) kstore_.freeze(out[i]);
  }
}

void Augur::evaluate(const Observations& obs, Mono now, const AugurCtx& ctx, std::vector<Pending>& fires) {
  (void)ctx;
  const auto rules = rules_->rules();
  for (const Sample& s : obs.samples) {
    for (const std::size_t ri : by_signal_[static_cast<std::size_t>(s.signal)]) {
      const Rule& r = rules[ri];
      DetState* st = nullptr;
      Pending p;
      p.rule = ri;
      p.subject = s.subject;
      switch (s.subject.kind) {
        case SubjectRef::Kind::Chr: {
          Tracker* tr = world_.find(s.subject.handle);
          if (tr == nullptr) continue;
          const std::uint8_t kind = tr->subject.kind == Subject::Kind::Persona ? kKindPersona : kKindGeneric;
          if ((r.subjects & kind) == 0 || tr->subject.kind == Subject::Kind::Player) continue;
          if (r.alive && tr->chr.dead) continue;
          if (r.in_combat && !WorldModel::combatant(*tr)) continue;
          st = &tr->det[ri];
          p.distance = std::isfinite(tr->chr.dist_m) ? tr->chr.dist_m : 0.0f;
          p.handle = tr->handle.raw;
          if (r.detect.kind == DetectorKind::Engagement) {
            if (!tr->transition || tr->transition->engaged != (r.detect.on == 0)) continue;
            p.transition = tr->transition;
            p.fire = Fire{s.t, s.t, s.value, s.value, 0};
            ++stats_.fires;
            fires.push_back(p);
            continue;
          }
          break;
        }
        case SubjectRef::Kind::Player: {
          if ((r.subjects & kKindPlayer) == 0) continue;
          if (r.alive && world_.player().p.dead) continue;
          if (r.in_combat && !world_.in_combat()) continue;
          st = &world_.player().det[ri];
          break;
        }
        case SubjectRef::Kind::World:
          if ((r.subjects & kKindWorld) == 0) continue;
          st = &world_.world().det[ri];
          break;
      }
      if (st == nullptr) continue;
      if (auto f = detect(r.detect, r.debounce_ms, *st, s.t, s.value)) {
        p.fire = *f;
        ++stats_.fires;
        fires.push_back(p);
      }
    }
  }
  for (const Fact& f : obs.facts) {
    if (f.kind != Fact::Kind::Life) continue;
    for (const std::size_t ri : by_signal_[static_cast<std::size_t>(SignalId::Lifecycle)]) {
      const Rule& r = rules[ri];
      if (r.detect.kind != DetectorKind::Fact || r.detect.on != static_cast<std::uint8_t>(f.life.kind)) continue;
      Pending p;
      p.rule = ri;
      p.subject = SubjectRef{SubjectRef::Kind::World, {}};
      p.fire = Fire{now, now, Value::some(f.life.value), Value{}, 0};
      p.fact = f.life;
      ++stats_.fires;
      fires.push_back(p);
    }
  }
}

void Augur::emit_batch(std::vector<Pending>& fires, Mono now, const AugurCtx& ctx, EventBatch& out) {
  const auto rules = rules_ ? rules_->rules() : std::span<const Rule>{};
  // §8.9 step 3: by (priority, subject distance, handle); the rule's place in the set breaks a tie.
  std::stable_sort(fires.begin(), fires.end(), [&](const Pending& a, const Pending& b) {
    const Rule& ra = rules[a.rule];
    const Rule& rb = rules[b.rule];
    if (ra.priority != rb.priority) return ra.priority < rb.priority;
    if (a.distance != b.distance) return a.distance < b.distance;
    if (a.handle != b.handle) return a.handle < b.handle;
    return a.rule < b.rule;
  });
  for (Pending& p : fires) finish(p, now, ctx, out);
}

Event Augur::start_event(const Rule& rule, const Pending& p, Mono now) const {
  Event ev;
  ev.event_class = rule.cls;
  ev.t_mono_ms = now;
  const PlayerTrack& pl = world_.player();
  ev.t_play_ms = pl.ever ? pl.p.play_time_ms : 0;
  ev.journey = pl.ever ? pl.p.ng_lvl : 0;
  if (pl.ever) ev.place.block = pl.p.block;
  if (world_.world().place) {
    ev.place.place_id = world_.world().place->place_id;
    ev.place.region = world_.world().place->region;
  }
  switch (p.subject.kind) {
    case SubjectRef::Kind::Chr:
      if (const Tracker* tr = world_.find(p.subject.handle)) ev.subject = ActorRef::of(tr->subject, tr->handle);
      break;
    case SubjectRef::Kind::Player:
      ev.subject = ActorRef::player();
      break;
    case SubjectRef::Kind::World:
      break;
  }
  ev.confidence = rule.confidence;
  ev.source = p.source;
  ev.rule = rule.id;
  ev.priority = rule.priority;
  ev.distance_m = p.distance;
  ev.order_handle = p.handle;
  ev.radius = rule.radius;
  return ev;
}

bool Augur::finish(Pending& p, Mono now, const AugurCtx& ctx, EventBatch& out) {
  const Rule& rule = rules_->rules()[p.rule];
  Event ev = start_event(rule, p, now);
  Placeholders ph;
  ph.add("subject", ev.subject ? key_text(*ev.subject) : std::string("world"));
  const Tracker* tr = p.subject.kind == SubjectRef::Kind::Chr ? world_.find(p.subject.handle) : nullptr;
  const PlayerTrack& pl = world_.player();
  const auto actor_of = [&](ChrHandle h) -> std::optional<ActorRef> {
    if (pl.ever && h == pl.p.handle) return ActorRef::player();
    if (const Tracker* t = world_.find(h); t != nullptr && t->present) return ActorRef::of(t->subject, h);
    return std::nullopt;
  };
  const auto bars_of = [&](const proto::FixedVector<proto::BossBar, 3>& bars) {
    proto::FixedVector<BossRef, 3> out_bars;
    for (const proto::BossBar& b : bars) {
      const auto a = actor_of(b.handle);
      (void)out_bars.try_push_back(BossRef{a ? *a : ActorRef::mute(b.handle), b.name_id});
    }
    return out_bars;
  };

  // The payload, at emission (§8.5: builders registered in code).
  switch (rule.payload) {
    case PayloadId::Place: {
      if (!p.fire.x.present()) {
        ++stats_.refused;  // left for no place: nothing entered
        return false;
      }
      PlaceEntered e;
      e.place_id = static_cast<std::uint32_t>(p.fire.x.v);
      if (p.fire.prev.present()) e.prev = static_cast<std::uint32_t>(p.fire.prev.v);
      ph.add("place_id", std::to_string(e.place_id));
      ev.kind = e;
      break;
    }
    case PayloadId::BossStart: {
      ev.kind = BossFightStarted{bars_of(world_.world().boss.bars)};
      ph.add("fight_id", std::to_string(world_.fight() ? world_.fight()->ordinal : 0));
      break;
    }
    case PayloadId::BossOutcome: {
      if (!p.outcome) {
        // Victory and Death emit as soon as their evidence is seen; Left only when the window ends without either.
        p.fight = world_.fight() ? world_.fight()->ordinal : 0;
        deferred_.push_back(Deferred{p, p.fire.t_c + kBossOutcomeWindowMs});
        return false;
      }
      BossFightEnded e;
      e.outcome = *p.outcome;
      if (world_.fight() && world_.fight()->ordinal == p.fight) e.bars = bars_of(world_.fight()->bars);
      ph.add("fight_id", std::to_string(p.fight));
      ev.kind = e;
      break;
    }
    case PayloadId::TalkStart: {
      NativeTalkStarted e;
      e.signals.popup = world_.world().ui.popup_talk_id.has_value();
      e.signals.talk_cam = world_.world().ui.talk_cam;
      ev.kind = e;
      break;
    }
    case PayloadId::TalkEnd: {
      NativeTalkEnded e;
      const auto& started = world_.world().native_talk_started;
      e.held_ms = started && p.fire.t >= *started ? static_cast<std::uint32_t>(p.fire.t - *started) : 0;
      ev.kind = e;
      break;
    }
    case PayloadId::PlayerDied: {
      PlayerDied e;
      if (pl.ever && pl.p.last_hit_by) e.killer = actor_of(*pl.p.last_hit_by);
      if (e.killer && e.killer->kind == ActorRef::Kind::Player) e.killer.reset();
      e.in_boss = world_.world().boss.active || std::any_of(deferred_.begin(), deferred_.end(), [&](const Deferred& d) {
                    return rules_->rules()[d.pending.rule].payload == PayloadId::BossOutcome;
                  });
      ev.kind = e;
      ev.object = e.killer;
      break;
    }
    case PayloadId::Mount:
      ev.kind = MountChanged{p.fire.x.truth()};
      break;
    case PayloadId::Struck: {
      Struck e;
      e.count = 1;
      e.frac_total = static_cast<float>(p.fire.amount);
      if (tr != nullptr) {
        e.vic = ActorRef::of(tr->subject, tr->handle);
        e.dmg_type = tr->chr.recv_dmg_type;
        e.hard = WorldModel::drop_within(tr->hist, p.fire.t, kHardWindowMs) >= kHardFraction;
        // The Tarnished's hit is read; an NPC attacker's waits for S7 (unknown, not unseen).
        if (pl.present && tr->chr.last_hit_by && *tr->chr.last_hit_by == pl.p.handle) {
          if (std::isfinite(tr->chr.dist_m) && tr->chr.dist_m > kWitnessM) {
            e.unseen = true;
          } else {
            e.att = ActorRef::player();
          }
        }
      } else if (p.subject.kind == SubjectRef::Kind::Player) {
        e.vic = ActorRef::player();
        e.hard = WorldModel::drop_within(pl.hist, p.fire.t, kHardWindowMs) >= kHardFraction;
        if (pl.p.last_hit_by) {
          // An unseen attack: last_hit_by names no tracked character, or one farther than 20 m.
          const Tracker* a = world_.find(*pl.p.last_hit_by);
          if (a == nullptr || !a->present || !std::isfinite(a->chr.dist_m) || a->chr.dist_m > kWitnessM) {
            e.unseen = true;
          } else {
            e.att = ActorRef::of(a->subject, a->handle);
          }
        }
      } else {
        return false;
      }
      ph.add("att", e.att ? key_text(*e.att) : std::string(e.unseen ? "unseen" : "none"));
      ev.object = e.att;
      ev.kind = e;
      break;
    }
    case PayloadId::LowHp:
      ev.kind = LowHp{ev.subject.value_or(ActorRef{}), static_cast<float>(p.fire.x.v)};
      break;
    case PayloadId::Healed:
      ev.kind = Healed{ev.subject.value_or(ActorRef{}), static_cast<float>(p.fire.amount)};
      break;
    case PayloadId::Engaged: {
      if (tr == nullptr || !p.transition) return false;
      ev.kind = Engaged{ActorRef::of(tr->subject, tr->handle), ActorRef::player(), p.transition->via,
                        p.transition->squad};
      ev.object = ActorRef::player();
      ev.confidence = p.transition->conf;  // the branch's value
      ph.add("opponent", "player");
      break;
    }
    case PayloadId::Disengaged: {
      if (tr == nullptr || !p.transition) return false;
      ev.kind = Disengaged{ActorRef::of(tr->subject, tr->handle), ActorRef::player(), p.transition->fled,
                           p.transition->why};
      ev.object = ActorRef::player();
      ev.confidence = p.transition->conf;
      ph.add("opponent", "player");
      break;
    }
    case PayloadId::AllyDied: {
      if (tr == nullptr) return false;
      // Live witnesses on its side within 20 m: the same live team (no cards yet; the Tarnished's side holds no
      // character until S7).
      std::vector<std::pair<float, ChrHandle>> near;
      for (const Tracker& w : world_.trackers()) {
        if (!w.present || w.handle == tr->handle || w.chr.dead || w.chr.team != tr->chr.team) continue;
        const float d = dist3(w.chr.pos, tr->chr.pos);
        if (d <= kWitnessM) near.emplace_back(d, w.handle);
      }
      if (near.empty()) {
        ++stats_.refused;
        return false;
      }
      std::sort(near.begin(), near.end());
      AllyDied e;
      e.victim = ActorRef::of(tr->subject, tr->handle);
      e.side = SideId{SideId::Kind::Team, tr->chr.team};
      for (std::size_t i = 0; i < near.size() && i < kMaxAudience; ++i) e.witnesses.push_back(near[i].second);
      ev.kind = std::move(e);
      break;
    }
    case PayloadId::Crit:
      ev.kind = CritLanded{ev.subject.value_or(ActorRef{})};
      ev.object = ActorRef::player();
      break;
    case PayloadId::Journey:
      ev.kind = JourneyChanged{static_cast<std::uint8_t>(p.fire.x.v)};
      ph.add("ng_lvl", std::to_string(static_cast<unsigned>(p.fire.x.v)));
      break;
    case PayloadId::DataReady:
      ev.kind = DataReady{};
      break;
    case PayloadId::Playthrough:
      ev.kind = PlaythroughReady{p.fact ? p.fact->key : PlaythroughKey{}};
      ph.add("slot", std::to_string(p.fact ? p.fact->key.slot : 0));
      break;
    case PayloadId::LoadingStarted:
      ev.kind = LoadingStarted{};
      break;
    case PayloadId::LoadingEnded:
      ev.kind = LoadingEnded{};
      break;
    case PayloadId::Title:
      ev.kind = TitleReached{};
      break;
    case PayloadId::Gate:
      ev.kind = GateChanged{p.fact && p.fact->value != 0};
      ph.add("active", p.fact && p.fact->value != 0 ? "open" : "closed");
      break;
    case PayloadId::Group: {
      const std::uint8_t g = p.fact ? p.fact->value : 0;
      ev.kind = GroupTripped{static_cast<proto::Group>(g < proto::kGroupCount ? g : 0)};
      ph.add("group", std::string(kGroupWords[g < kGroupWords.size() ? g : 0]));
      break;
    }
    case PayloadId::Ladder: {
      const std::uint8_t l = p.fact ? p.fact->value : 0;
      ev.kind = LadderChanged{static_cast<LadderLevel>(l < kLevelWords.size() ? l : 0)};
      ph.add("to", std::string(kLevelWords[l < kLevelWords.size() ? l : 0]));
      break;
    }
  }

  // requires, evaluated at emission: false suppresses the fire, and its edge is consumed.
  if (rule.requires_id != kAlwaysTrue && !ctx.eval(rule.requires_id, EvalScope{&ev})) {
    ++stats_.by_requires;
    return false;
  }
  // cooldown, per (rule, subject), in mono time: suppressed, never deferred.
  if (cooldowns_.cooling(rule.index, subject_key(p.subject), now, rule.cooldown_s)) {
    ++stats_.by_cooldown;
    return false;
  }
  if (!rule.key.empty()) ev.key = std::make_shared<const std::string>(render_key(rule.key, ph));
  // class handling.
  switch (rule.cls) {
    case Class::P:
      if (rule.dedup_s > 0 && ev.key && dedup_.duplicate(*ev.key, ev.t_play_ms, rule.dedup_s)) {
        ++stats_.by_dedup;
        return false;
      }
      break;
    case Class::K: {
      const KStore::Result r = kstore_.put(ev, rule.index, now, rule.merge_ms, rule.ttl_s);
      if (r.merged) ++stats_.merged;
      if (!r.emit) return false;
      break;
    }
    case Class::T:
      break;
  }
  ev.interrupts = rule.interrupts | kind_interrupts(ev);
  if (std::holds_alternative<PlayerDied>(ev.kind)) world_.note_player_died(now);
  ++stats_.emitted;
  out.push_back(std::move(ev));
  return true;
}

void Augur::resolve_deferred(Mono now, bool timer, const AugurCtx& ctx, EventBatch& out) {
  for (std::size_t i = 0; i < deferred_.size();) {
    Deferred& d = deferred_[i];
    std::optional<BossOutcome> outcome;
    const auto& f = world_.fight();
    if (f && f->ordinal == d.pending.fight) {
      if (f->victory_at && (!f->died_at || *f->victory_at <= *f->died_at)) {
        outcome = BossOutcome::Victory;
      } else if (f->died_at) {
        outcome = BossOutcome::Death;
      }
    }
    if (!outcome && timer && now >= d.deadline) outcome = BossOutcome::Left;
    if (!outcome) {
      ++i;
      continue;
    }
    Pending p = d.pending;
    p.outcome = outcome;
    p.source = timer ? SourceId::Timer : SourceId::SnapshotDiff;
    deferred_.erase(deferred_.begin() + static_cast<std::ptrdiff_t>(i));
    finish(p, now, ctx, out);
  }
}

void Augur::resolve_audience(Event& ev, const AugurCtx& ctx) const {
  ev.audience.clear();
  if (ev.radius == Radius::None) return;
  // Origin: the subject's position; the Tarnished's for the Tarnished's and the world's events.
  const ActorRef origin_actor = ev.subject && ev.subject->kind != ActorRef::Kind::Player ? *ev.subject : ActorRef::player();
  auto origin = world_.position(origin_actor);
  auto origin_block = world_.block(origin_actor);
  if (!origin || !origin_block) {
    origin = world_.position(ActorRef::player());
    origin_block = world_.block(ActorRef::player());
  }
  if (!origin || !origin_block) return;
  const float radius = radius_m(ev.radius);
  struct Cand {
    float d;
    std::uint64_t handle;
    AudienceEntry entry;
  };
  std::vector<Cand> cands;
  for (const Tracker& t : world_.trackers()) {
    if (!t.present || t.subject.kind != Subject::Kind::Persona) continue;
    if (!ctx.persona_may_perceive(t.subject.persona)) continue;
    const float d = dist3(t.chr.pos, *origin);
    if (d > radius || !world_.same_area(t.chr.block, *origin_block)) continue;
    const ActorRef me = ActorRef::of(t.subject, t.handle);
    How how = How::Witnessed;
    if ((ev.subject && *ev.subject == me) || (ev.object && *ev.object == me)) {
      how = How::Direct;
    } else if (ev.radius == Radius::Speech) {
      how = How::Overheard;
    }
    cands.push_back(Cand{d, t.handle.raw, AudienceEntry{t.subject.persona, how}});
  }
  std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
    return a.d != b.d ? a.d < b.d : a.handle < b.handle;
  });
  for (const Cand& c : cands) {
    if (ev.audience.size() >= kMaxAudience) break;
    if (std::any_of(ev.audience.begin(), ev.audience.end(),
                    [&](const AudienceEntry& a) { return a.persona == c.entry.persona; })) {
      continue;
    }
    ev.audience.push_back(c.entry);
  }
}

}  // namespace en::augur
