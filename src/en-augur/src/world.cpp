// The world model (world.hpp; ARCH §8.2, §8.3, §8.7).
#include "en/augur/world.hpp"

#include <algorithm>
#include <cmath>

#include "en/augur/augur.hpp"

namespace en::augur {

namespace {

float frac_of(const proto::Meter& m) noexcept {
  return m.max == 0 ? 0.0f : static_cast<float>(m.cur) / static_cast<float>(m.max);
}

float distance(const std::array<float, 3>& a, const std::array<float, 3>& b) noexcept {
  const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

template <class V, class T>
bool contains(const V& v, T x) {
  return std::find(v.begin(), v.end(), x) != v.end();
}

}  // namespace

ActorRef ActorRef::of(const Subject& s, ChrHandle h) noexcept {
  switch (s.kind) {
    case Subject::Kind::Player:
      return player();
    case Subject::Kind::Persona:
      return ActorRef{Kind::Persona, h, s.persona.index, 0};
    case Subject::Kind::Card:
      return ActorRef{Kind::Card, h, s.card.index, s.variant};
    case Subject::Kind::Mute:
      break;
  }
  return mute(h);
}

WorldModel::WorldModel(WorldFacts facts) : facts_(std::move(facts)) {}

void WorldModel::set_rule_count(std::size_t n) {
  rule_count_ = n;
  for (Tracker& t : trackers_) t.det.assign(n, DetState{});
  player_.det.assign(n, DetState{});
  world_.det.assign(n, DetState{});
}

Tracker* WorldModel::find(ChrHandle h) noexcept {
  for (Tracker& t : trackers_) {
    if (t.handle == h) return &t;
  }
  return nullptr;
}

const Tracker* WorldModel::find(ChrHandle h) const noexcept {
  for (const Tracker& t : trackers_) {
    if (t.handle == h) return &t;
  }
  return nullptr;
}

bool WorldModel::hostile(const proto::Chr& c) const noexcept {
  // Live team in the hostile set; a persona with clear hostility bits would not be (DESIGN §5.3): no personas yet.
  return contains(facts_.hostile_teams, c.team);
}

bool WorldModel::hit_by_player(const Tracker& tr) const noexcept {
  return player_.present && tr.chr.last_hit_by && *tr.chr.last_hit_by == player_.p.handle && tr.hp_delta_valid &&
         tr.hp_delta > 0;
}

bool WorldModel::hit_player(const Tracker& tr) const noexcept {
  return player_.present && player_.p.last_hit_by && *player_.p.last_hit_by == tr.handle && player_.hp_delta_valid &&
         player_.hp_delta > 0;
}

bool WorldModel::crit_by_player(const Tracker& tr) const noexcept {
  return player_.present && facts_.crit_player_throw != 0 && player_.p.throw_state == facts_.crit_player_throw &&
         tr.chr.throw_state == facts_.crit_victim_throw;
}

void WorldModel::update(const proto::WorldSnapshot& s, const AugurCtx& ctx) {
  const Mono t = s.mono_ms;
  update_world(s, t);
  update_player(s, t);
  update_trackers(s, t, ctx);
  // The engagement machine, for every present character; then the trackers absent too long go.
  for (Tracker& tr : trackers_) {
    tr.transition.reset();
    if (tr.present) {
      update_engagement(tr, t);
    } else if (tr.eng.on && t - tr.last_seen >= kTrackerDropMs) {
      end_engagement(tr);  // lost: cleared silently
    }
  }
  trackers_.erase(std::remove_if(trackers_.begin(), trackers_.end(),
                                 [&](const Tracker& tr) { return !tr.present && t - tr.last_seen >= kTrackerDropMs; }),
                  trackers_.end());
  // The combat state (§8.7): damage either way, a critical hit, or any engagement at the gate's confidence.
  bool signal = player_.hp_delta_valid && player_.hp_delta > 0;
  for (const Tracker& tr : trackers_) {
    if (!tr.present) continue;
    if (hit_by_player(tr) || crit_by_player(tr) || combatant(tr)) signal = true;
  }
  if (signal) last_combat_signal_ = t;
}

void WorldModel::update_world(const proto::WorldSnapshot& s, Mono t) {
  const bool was_active = world_.has_snapshot && world_.boss.active;
  world_.has_snapshot = true;
  world_.t = t;
  world_.ui = s.ui;
  world_.boss = s.boss;
  world_.place = s.place;
  const bool talk = s.ui.popup_talk_id.has_value() || s.ui.talk_cam;
  if (talk && !world_.native_talk_since) {
    world_.native_talk_since = t;
    world_.native_talk_started = t;
  }
  if (!talk) world_.native_talk_since.reset();
  // A boss fight starts on the bars' rising edge; its Victory evidence is a victory banner seen since.
  if (s.boss.active && !was_active) {
    BossFight f;
    f.ordinal = ++fights_;
    f.started = t;
    f.bars = s.boss.bars;
    fight_ = f;
  }
  if (fight_ && s.ui.banner && !fight_->victory_at && contains(facts_.victory_banners, *s.ui.banner)) {
    fight_->victory_at = t;
  }
}

void WorldModel::note_player_died(Mono t) {
  if (fight_ && !fight_->died_at) fight_->died_at = t;
}

void WorldModel::update_player(const proto::WorldSnapshot& s, Mono t) {
  if (!s.player) {
    player_.present = false;
    player_.hp_delta_valid = false;
    return;
  }
  const proto::Player& p = *s.player;
  player_.hp_delta_valid = false;
  if (player_.has_prev && p.hp.max > 0) {
    player_.hp_delta = (static_cast<float>(player_.prev_hp) - static_cast<float>(p.hp.cur)) / static_cast<float>(p.hp.max);
    player_.hp_delta_valid = true;
  }
  player_.present = true;
  player_.ever = true;
  player_.last_seen = t;
  player_.p = p;
  player_.has_prev = true;
  player_.prev_hp = p.hp.cur;
  player_.hist.push(HistSample{t, frac_of(p.hp), 0.0f});
}

void WorldModel::update_trackers(const proto::WorldSnapshot& s, Mono t, const AugurCtx& ctx) {
  for (Tracker& tr : trackers_) tr.present = false;
  for (const proto::Chr& c : s.chars) {
    if (s.player && c.handle == s.player->handle) continue;  // the Tarnished is its own subject
    Tracker* tr = find(c.handle);
    if (tr == nullptr) {
      if (trackers_.size() >= kMaxTrackers) {
        // Past the cap, the oldest tracker of an absent handle goes first (§8.10).
        auto oldest = trackers_.end();
        for (auto it = trackers_.begin(); it != trackers_.end(); ++it) {
          if (!it->present && (oldest == trackers_.end() || it->last_seen < oldest->last_seen)) oldest = it;
        }
        if (oldest == trackers_.end()) continue;
        if (oldest->eng.on) end_engagement(*oldest);
        trackers_.erase(oldest);
      }
      Tracker fresh;
      fresh.handle = c.handle;
      fresh.det.assign(rule_count_, DetState{});
      trackers_.push_back(std::move(fresh));
      tr = &trackers_.back();
    }
    tr->hp_delta_valid = false;
    if (tr->has_prev && c.hp.max > 0) {
      tr->hp_delta = (static_cast<float>(tr->prev_hp) - static_cast<float>(c.hp.cur)) / static_cast<float>(c.hp.max);
      tr->hp_delta_valid = true;
    }
    tr->present = true;
    tr->last_seen = t;
    tr->chr = c;
    tr->has_prev = true;
    tr->prev_hp = c.hp.cur;
    tr->subject = ctx.classify(c);
    tr->hist.push(HistSample{t, frac_of(c.hp), std::isfinite(c.dist_m) ? c.dist_m : 0.0f});
  }
}

float WorldModel::drop_within(const Ring<HistSample, 32>& hist, Mono now, Mono window) noexcept {
  if (hist.empty()) return 0.0f;
  float best = hist.back().hp_frac;
  for (std::size_t i = 0; i < hist.size(); ++i) {
    if (hist[i].t + window >= now) best = std::max(best, hist[i].hp_frac);
  }
  return best - hist.back().hp_frac;
}

void WorldModel::update_engagement(Tracker& tr, Mono t) {
  Engagement& e = tr.eng;
  const bool alive = !tr.chr.dead;
  if (!alive || !hostile(tr.chr) || !player_.present) {
    // Dead: no Disengaged (AllyDied, KilledNear or FoeKilled instead); no longer hostile, or no Tarnished: cleared.
    if (e.on) end_engagement(tr);
    return;
  }
  const float dist = std::isfinite(tr.chr.dist_m) ? tr.chr.dist_m : 1e9f;
  const bool hit = hit_player(tr) || hit_by_player(tr);
  const bool lock = player_.p.locked_on && *player_.p.locked_on == tr.handle;
  bool closing = false;
  if (lock && dist <= kLockClosingM) {
    // The oldest sample within the last 2 s, other than this one.
    for (std::size_t i = 0; i + 1 < tr.hist.size(); ++i) {
      if (tr.hist[i].t + kLockClosingMs >= t) {
        closing = tr.hist[i].dist - dist >= kLockClosingShrinkM;
        break;
      }
    }
  }
  const bool near = dist <= kNearM;
  // Start conditions, strongest first (DESIGN §5.3).
  std::optional<std::pair<EngageVia, float>> best;
  if (hit) {
    best = {EngageVia::Hit, 0.9f};
  } else if (!e.rearm) {
    if (closing) best = {EngageVia::LockClosing, 0.6f};
    else if (near) best = {EngageVia::Near, 0.6f};
    else if (lock) best = {EngageVia::LockOnly, 0.3f};
  }
  if (e.rearm && !near && !lock) e.rearm = false;

  if (!e.on) {
    if (!best) return;
    e.on = true;
    e.via = best->first;
    e.conf = best->second;
    e.since = t;
    e.last_activity = t;
    e.rearm = false;
    e.squad = join_squad(tr, t);
    tr.transition = EngTransition{true, e.via, e.conf, DisengageWhy::Timeout, false, e.squad};
    return;
  }
  if (hit || lock) e.last_activity = t;
  if (best && best->second > e.conf) {
    const float old = e.conf;
    e.conf = best->second;
    e.via = best->first;
    if (old < kConfGate && e.conf >= kConfGate) {
      tr.transition = EngTransition{true, e.via, e.conf, DisengageWhy::Timeout, false, e.squad};
    }
  }
  std::optional<DisengageWhy> why;
  if (dist > kEngageEndM) why = DisengageWhy::Distance;
  else if (t - e.last_activity >= kEngageTimeoutMs) why = DisengageWhy::Timeout;
  if (!why) return;
  // fled: the distance grew ≥ 15 m within the last 5 s, and it is now beyond 25 m.
  float least = dist;
  for (std::size_t i = 0; i < tr.hist.size(); ++i) {
    if (tr.hist[i].t + kFledWindowMs >= t) least = std::min(least, tr.hist[i].dist);
  }
  const bool fled = dist - least >= kFledGrowM && dist > kFledBeyondM;
  tr.transition = EngTransition{false, e.via, e.conf, *why, fled, e.squad};
  end_engagement(tr);
  if (*why == DisengageWhy::Timeout) e.rearm = true;
}

void WorldModel::end_engagement(Tracker& tr) {
  leave_squad(tr);
  const bool rearm = tr.eng.rearm;
  tr.eng = Engagement{};
  tr.eng.rearm = rearm;
}

SquadId WorldModel::join_squad(const Tracker& tr, Mono t) {
  // An existing squad of the same side (the same live team: no cards yet) with a member engaged within the last 10 s
  // and within 20 m; else a new one.
  for (Squad& sq : squads_) {
    if (sq.team != tr.chr.team) continue;
    for (const ChrHandle m : sq.members) {
      const Tracker* other = find(m);
      if (other == nullptr || !other->eng.on || !other->present) continue;
      if (t - other->eng.last_activity > kSquadRecentMs) continue;
      if (distance(other->chr.pos, tr.chr.pos) > kSquadJoinM) continue;
      sq.members.push_back(tr.handle);
      return sq.id;
    }
  }
  Squad sq;
  sq.id = next_squad_++;
  sq.team = tr.chr.team;
  sq.members.push_back(tr.handle);
  squads_.push_back(std::move(sq));
  return squads_.back().id;
}

void WorldModel::leave_squad(Tracker& tr) {
  if (tr.eng.squad == 0) return;
  for (auto it = squads_.begin(); it != squads_.end(); ++it) {
    if (it->id != tr.eng.squad) continue;
    it->members.erase(std::remove(it->members.begin(), it->members.end(), tr.handle), it->members.end());
    if (it->members.empty()) squads_.erase(it);  // the squad dissolves with its last member
    break;
  }
  tr.eng.squad = 0;
}

void WorldModel::loading(std::span<const Rule> rules) {
  // §9: engagements and squads cleared silently; the characters forgotten (a load replaces them).
  trackers_.clear();
  squads_.clear();
  last_combat_signal_.reset();
  for (const Rule& r : rules) {
    if (r.index < player_.det.size()) on_loading(r.detect, player_.det[r.index]);
    if (r.index < world_.det.size()) on_loading(r.detect, world_.det[r.index]);
  }
  player_.has_prev = false;  // no hp_delta across a load
  player_.hp_delta_valid = false;
  player_.hist.clear();
}

bool WorldModel::in_combat() const noexcept {
  if (world_.boss.active) return true;
  return last_combat_signal_ && world_.t - *last_combat_signal_ < kCombatSignalMs;
}

Value WorldModel::sample(SignalId id, const Tracker& tr) const noexcept {
  const proto::Chr& c = tr.chr;
  switch (id) {
    case SignalId::HpFrac:
      return c.hp.max == 0 ? Value{} : Value::some(frac_of(c.hp));
    case SignalId::HpDelta:
      return tr.hp_delta_valid ? Value::some(tr.hp_delta) : Value{};
    case SignalId::Dead:
      return Value::of(c.dead);
    case SignalId::TeamHostile:
      return Value::of(hostile(c));
    case SignalId::DistM:
      return std::isfinite(c.dist_m) ? Value::some(c.dist_m) : Value{};
    case SignalId::OnScreen:
      return Value::of(c.on_screen);
    case SignalId::LockedByPlayer:
      return Value::of(player_.present && player_.p.locked_on && *player_.p.locked_on == tr.handle);
    case SignalId::HitByPlayer:
      return Value::of(hit_by_player(tr));
    case SignalId::HitPlayer:
      return Value::of(hit_player(tr));
    case SignalId::Engaged:
      return Value::some(tr.eng.on ? tr.eng.conf : 0.0f);
    case SignalId::Throw:
      return Value::some(c.throw_state);
    case SignalId::CritByPlayer:
      return Value::of(crit_by_player(tr));
    default:
      return Value{};  // team_turned (S7), idle_due (v0.4), and the signals that are not a character's
  }
}

Value WorldModel::sample_player(SignalId id) const noexcept {
  if (!player_.present) return Value{};
  const proto::Player& p = player_.p;
  switch (id) {
    case SignalId::HpFrac:
      return p.hp.max == 0 ? Value{} : Value::some(frac_of(p.hp));
    case SignalId::HpDelta:
      return player_.hp_delta_valid ? Value::some(player_.hp_delta) : Value{};
    case SignalId::Dead:
      return Value::of(p.dead);
    case SignalId::Throw:
      return Value::some(p.throw_state);
    case SignalId::Riding:
      return Value::of(p.riding);
    case SignalId::Roundtable:
      return Value::of(p.roundtable);
    case SignalId::PlaceId:
      return world_.place ? Value::of_optional(world_.place->place_id) : Value{};
    case SignalId::BlockArea:
      return Value::some(p.block.area());
    case SignalId::Deaths:
      return Value::some(p.deaths);
    case SignalId::NgLvl:
      return Value::some(p.ng_lvl);
    case SignalId::InCombat:
      return Value::of(in_combat());
    default:
      return Value{};  // crouching (S7) and invisible (S8) are never sampled yet
  }
}

Value WorldModel::sample_world(SignalId id) const noexcept {
  if (!world_.has_snapshot) return Value{};
  switch (id) {
    case SignalId::Banner:
      return Value::of_optional(world_.ui.banner);
    case SignalId::Hud:
      return Value::some(static_cast<double>(world_.ui.hud));
    case SignalId::TalkPopup:
      return Value::of(world_.ui.popup_talk_id.has_value());
    case SignalId::TalkCam:
      return Value::of(world_.ui.talk_cam);
    case SignalId::NativeTalk:
      return Value::of(world_.ui.popup_talk_id.has_value() || world_.ui.talk_cam);
    case SignalId::BossActive:
      return Value::of(world_.boss.active);
    default:
      return Value{};
  }
}

std::optional<std::array<float, 3>> WorldModel::position(const ActorRef& a) const noexcept {
  if (a.kind == ActorRef::Kind::Player) {
    if (!player_.ever) return std::nullopt;
    return player_.p.pos;
  }
  const Tracker* t = find(a.handle);
  if (t == nullptr || !t->present) return std::nullopt;
  return t->chr.pos;
}

std::optional<BlockId> WorldModel::block(const ActorRef& a) const noexcept {
  if (a.kind == ActorRef::Kind::Player) {
    if (!player_.ever) return std::nullopt;
    return player_.p.block;
  }
  const Tracker* t = find(a.handle);
  if (t == nullptr || !t->present) return std::nullopt;
  return t->chr.block;
}

bool WorldModel::same_area(BlockId a, BlockId b) const noexcept {
  // In the open world, the same area byte; indoors, the same full block (ARCH §8.8 item 5).
  if (contains(facts_.open_world_areas, a.area())) return a.area() == b.area();
  return a == b;
}

Stance WorldModel::stance(const ActorRef& viewer, const ActorRef& x) const noexcept {
  if (viewer == x) return Stance::Ally;
  const bool v_player = viewer.kind == ActorRef::Kind::Player;
  const bool x_player = x.kind == ActorRef::Kind::Player;
  const Tracker* vt = v_player ? nullptr : find(viewer.handle);
  const Tracker* xt = x_player ? nullptr : find(x.handle);
  // enemy: engaged with the viewer at the gate's confidence, or on hostile sides (a hostile character and the
  // Tarnished's side, which holds the Tarnished alone until S7 shows which live teams fight beside it). Between two
  // characters, only the team-pair table would say so (S7): never yet.
  if (v_player && xt != nullptr && (combatant(*xt) || hostile(xt->chr))) return Stance::Enemy;
  if (x_player && vt != nullptr && (combatant(*vt) || hostile(vt->chr))) return Stance::Enemy;
  // turned: team_turned, never sampled until S7.
  // ally: the same live team (no cards yet), or both on the Tarnished's side.
  if (vt != nullptr && xt != nullptr && vt->chr.team == xt->chr.team) return Stance::Ally;
  return Stance::Bystander;
}

}  // namespace en::augur
