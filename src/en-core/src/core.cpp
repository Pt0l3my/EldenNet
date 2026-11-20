// en-core's v0.1 slice: the lifecycle, the gate's effects and the ladder's first levels (core.hpp; ARCH §5, §6.8, §9).
#include "en/core/core.hpp"

#include <array>
#include <type_traits>
#include <utility>

namespace en::core {

namespace {

constexpr std::array<proto::Group, 3> kStructural = {proto::Group::Session, proto::Group::World, proto::Group::Player};

bool structural_ok(const proto::ReadHealth& h) noexcept {
  for (const proto::Group g : kStructural) {
    if (h.of(g).state != proto::GroupState::Ok) return false;
  }
  return true;
}

bool structural_phase_b(const proto::ReadHealth& h) noexcept {
  for (const proto::Group g : kStructural) {
    if (h.of(g).state != proto::GroupState::Ok || !h.of(g).phase_b) return false;
  }
  return true;
}

proto::Capabilities caps_of(const proto::ReadHealth& h) noexcept {
  proto::Capabilities c;
  for (std::size_t i = 0; i < proto::kGroupCount; ++i) {
    const auto g = static_cast<proto::Group>(i);
    if (h.of(g).state == proto::GroupState::Ok) c.groups = c.groups | proto::bit_of(g);
  }
  return c;
}

template <class... F>
struct Overloaded : F... {
  using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>;

}  // namespace

const char* to_string(Phase p) noexcept {
  switch (p) {
    case Phase::Gate:
      return "gate";
    case Phase::Banner:
      return "banner";
    case Phase::Ready:
      return "ready";
    case Phase::Title:
      return "title";
    case Phase::Playthrough:
      return "playthrough";
    case Phase::InWorld:
      return "in_world";
    case Phase::Loading:
      return "loading";
  }
  return "unknown";
}

const char* to_string(Level l) noexcept {
  switch (l) {
    case Level::Starting:
      return "starting";
    case Level::L1:
      return "L1";
    case Level::L2:
      return "L2";
  }
  return "unknown";
}

const char* to_string(BannerReason r) noexcept {
  switch (r) {
    case BannerReason::UnknownBuild:
      return "unknown_build";
    case BannerReason::AntiCheat:
      return "anticheat";
    case BannerReason::NotStarted:
      return "not_started";
    case BannerReason::DiscoveryFailed:
      return "discovery_failed";
    case BannerReason::RegisterFaulted:
      return "register_faulted";
    case BannerReason::Inert:
      return "inert";
    case BannerReason::SessionFailed:
      return "session_failed";
    case BannerReason::TaskTripped:
      return "task_tripped";
  }
  return "unknown";
}

const char* to_string(Change::What w) noexcept {
  switch (w) {
    case Change::What::Phase:
      return "phase";
    case Change::What::GateOpened:
      return "gate_opened";
    case Change::What::GateClosed:
      return "gate_closed";
    case Change::What::Level:
      return "level";
    case Change::What::Banner:
      return "banner";
    case Change::What::Badge:
      return "badge";
  }
  return "unknown";
}

std::string banner_text(BannerReason r, std::string_view build, std::string_view reason) {
  std::string out;
  switch (r) {
    case BannerReason::UnknownBuild:
      out = "EldenNet inactive: game build ";
      out += build;
      out += " not supported yet";
      return out;
    case BannerReason::DiscoveryFailed:
    case BannerReason::RegisterFaulted:
    case BannerReason::Inert:
      out = "EldenNet inactive: game build ";
      out += build;
      out += " not supported yet (reason: ";
      out += reason;
      out += ")";
      return out;
    case BannerReason::NotStarted:
      return "EldenNet inactive: the game did not finish starting";
    case BannerReason::AntiCheat:
      return "EldenNet inactive: Easy Anti-Cheat is running. EldenNet only runs offline; start the game with the "
             "EldenNet launcher.";
    case BannerReason::SessionFailed:
      out = "EldenNet inactive: it could not confirm this is an offline solo session (game build ";
      out += build;
      out += ").";
      return out;
    case BannerReason::TaskTripped:
      return "EldenNet stopped reading the game after an error. Your game is not affected.";
  }
  return out;
}

// ---- Augur's side (ARCH §8; part 11) ----

augur::Subject AugurView::classify(const proto::Chr& chr) const {
  // Every subject is Mute or Player until personas exist (v0.2) and cards (v0.4).
  augur::Subject s;
  s.kind = player && chr.handle == *player ? augur::Subject::Kind::Player : augur::Subject::Kind::Mute;
  return s;
}

bool AugurView::eval(augur::PredicateId pred, const augur::EvalScope&) const { return predicates_.eval(pred, facts); }

bool AugurView::persona_may_perceive(augur::PersonaId) const { return false; }  // no persona yet

std::uint64_t AugurView::rng(augur::EventId ev, std::uint32_t salt) const {
  std::uint64_t z = seed_root ^ (ev.step * 0x9E3779B97F4A7C15ull) ^ (std::uint64_t{ev.index} << 32) ^ salt;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

augur::WorldFacts world_facts() {
  augur::WorldFacts f;
  f.hostile_teams = {6, 7, 27, 29};       // teams.toml's hostile set (DESIGN §5.3; usage to verify, S7)
  f.victory_banners = {1, 2, 3, 4, 22};   // a boss fight's Victory (DESIGN §6.5; to verify, S2 and S7)
  f.crit_player_throw = 3;                // CritLanded: the Tarnished's throw 3 while the victim's is 4 (§6.5)
  f.crit_victim_throw = 4;
  f.open_world_areas = {60, 61};          // the open world (ARCH §8.8)
  return f;
}

Core::Core() : augur_(world_facts()) {
  std::vector<augur::RuleFile> files;
  for (const BuiltinFile& f : builtin_rule_files()) files.push_back(augur::RuleFile{std::string(f.name), std::string(f.text)});
  rule_errors_ = load_rules(files);
}

std::vector<augur::RuleError> Core::load_rules(std::span<const augur::RuleFile> files) {
  auto loaded = augur::RuleSet::load(files, predicates_);
  if (!loaded) return loaded.error();  // the old set stays (ARCH §8.5)
  augur_.set_rules(*loaded);
  return {};
}

void Core::run_augur(const proto::WorldSnapshot* s, const proto::Lifecycle* l, Mono now, Outputs& out) {
  // The Lifecycle source's facts: what the slice changed on this step, and the read groups that tripped (from C2's
  // message or a snapshot's health; once per group for the session).
  std::vector<augur::LifeFact> facts;
  const auto trip = [&](proto::Group g) {
    const auto bit = static_cast<std::uint16_t>(1u << static_cast<unsigned>(g));
    if ((groups_tripped_ & bit) != 0) return;
    groups_tripped_ = static_cast<std::uint16_t>(groups_tripped_ | bit);
    facts.push_back(augur::LifeFact{augur::FactKind::GroupTripped, static_cast<std::uint8_t>(g), {}});
  };
  if (l != nullptr) {
    if (const auto* t = std::get_if<proto::Lifecycle::GroupTripped>(&l->payload)) trip(t->group);
  }
  if (s != nullptr) {
    for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
      if (s->health.groups[g].state == proto::GroupState::Tripped) trip(static_cast<proto::Group>(g));
    }
  }
  for (const Change& c : out.changes) {
    switch (c.what) {
      case Change::What::Phase: {
        const auto from = static_cast<Phase>(c.from);
        const auto to = static_cast<Phase>(c.to);
        if (from == Phase::Loading && (to == Phase::InWorld || to == Phase::Playthrough)) {
          facts.push_back(augur::LifeFact{augur::FactKind::LoadingEnded, 0, {}});
        }
        if (to == Phase::Ready) facts.push_back(augur::LifeFact{augur::FactKind::DataReady, 0, {}});
        if (to == Phase::Playthrough) {
          const std::uint8_t slot = current_ ? current_->slot : 0;
          facts.push_back(augur::LifeFact{augur::FactKind::PlaythroughReady, 0, {slot, state_.playthroughs}});
        }
        if (to == Phase::Loading) facts.push_back(augur::LifeFact{augur::FactKind::LoadingStarted, 0, {}});
        if (to == Phase::Title) facts.push_back(augur::LifeFact{augur::FactKind::TitleReached, 0, {}});
        break;
      }
      case Change::What::GateOpened:
        facts.push_back(augur::LifeFact{augur::FactKind::GateChanged, 1, {}});
        break;
      case Change::What::GateClosed:
        facts.push_back(augur::LifeFact{augur::FactKind::GateChanged, 0, {}});
        break;
      case Change::What::Level:
        facts.push_back(augur::LifeFact{augur::FactKind::LadderChanged, c.to, {}});  // Starting, L1, L2: the same order
        break;
      case Change::What::Banner:
      case Change::What::Badge:
        break;
    }
  }
  if (s != nullptr) {
    if (s->player) {
      view_.player = s->player->handle;
      view_.facts.runes = s->player->runes_held;
      view_.facts.journey = s->player->ng_lvl;
    } else {
      view_.player.reset();
    }
  }
  augur::EventBatch batch;
  augur_.step(augur::AugurInput{s, facts}, now, view_, batch);
  bus_.run(std::move(batch), now, augur_, view_, out.events);
}

Outputs Core::step(CoreInput input, Mono now) {
  Outputs out;
  tracked_none_sent_ = false;
  Mono at = now;
  const proto::WorldSnapshot* snapshot = nullptr;
  const proto::Lifecycle* lifecycle = nullptr;
  std::visit(Overloaded{
                 [&](std::unique_ptr<proto::WorldSnapshot>& s) {
                   if (s) {
                     at = s->mono_ms;
                     snapshot = s.get();
                     on_snapshot(*s, out);
                   }
                 },
                 [&](proto::Lifecycle& l) {
                   lifecycle = &l;
                   on_lifecycle(l, now, out);
                 },
                 [&](BootBanner& b) { on_banner(b, now, out); },
                 [&](CoreInput::Tick&) {},
             },
             input.payload);
  settle(at, out);
  out.ai_allowed = state_.gate_open && state_.level == Level::L2;
  out.state = state_;
  run_augur(snapshot, lifecycle, now, out);
  return out;
}

void Core::enter(Phase to, Mono at, Outputs& out) {
  if (state_.phase == to) return;
  const Phase from = state_.phase;
  out.changes.push_back(
      Change{Change::What::Phase, static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to), at});
  state_.phase = to;
  // Leaving a world: nothing stays tracked (§9: InWorld → Loading, → Title).
  if (from == Phase::InWorld) track_none(out);
}

void Core::set_level(Level to, Mono at, Outputs& out) {
  if (state_.level == to || state_.level == Level::L1) return;  // L1 is for the session
  out.changes.push_back(
      Change{Change::What::Level, static_cast<std::uint8_t>(state_.level), static_cast<std::uint8_t>(to), at});
  state_.level = to;
}

void Core::raise_banner(BannerReason r, Mono at, Outputs& out) {
  if (!state_.banner) {
    state_.banner = r;
    out.changes.push_back(Change{Change::What::Banner, 0, static_cast<std::uint8_t>(r), at});
  }
  set_level(Level::L1, at, out);
}

void Core::track_none(Outputs& out) {
  if (tracked_none_sent_) return;
  tracked_none_sent_ = true;
  out.game.push_back(proto::GameCommand{proto::GameCommand::SetTracked{}});
}

void Core::on_lifecycle(const proto::Lifecycle& l, Mono now, Outputs& out) {
  if (const auto* ready = std::get_if<proto::Lifecycle::DataReady>(&l.payload)) {
    caps_ = ready->caps;
    if (state_.phase == Phase::Gate) enter(Phase::Ready, now, out);
    if (state_.level == Level::Starting) set_level(Level::L2, now, out);
  } else if (const auto* p = std::get_if<proto::Lifecycle::PlaythroughReady>(&l.payload)) {
    pending_ = PlaythroughId{p->slot, p->name, p->archetype, p->keepsake};
  } else if (const auto* t = std::get_if<proto::Lifecycle::GroupTripped>(&l.payload)) {
    if (!proto::structural(t->group)) return;  // an optional group: L3, with the net
    if (t->group == proto::Group::Session) {
      if (gate_.session_failed_now()) {
        out.changes.push_back(
            Change{Change::What::GateClosed, static_cast<std::uint8_t>(Unclean::SessionFailed), 0, now});
        track_none(out);
      }
      state_.gate_open = false;
      state_.gate_why = Unclean::SessionFailed;
      raise_banner(BannerReason::SessionFailed, now, out);
    } else {
      raise_banner(BannerReason::TaskTripped, now, out);
    }
  }
}

void Core::on_banner(const BootBanner& b, Mono now, Outputs& out) {
  banner_detail_ = b.detail;
  enter(Phase::Banner, now, out);
  raise_banner(b.reason, now, out);
}

bool Core::ready_for_playthrough(const proto::WorldSnapshot& s) const noexcept {
  return s.life.main_player && s.life.screen == proto::Screen::InGame && s.player.has_value() &&
         structural_phase_b(s.health) && s.player->save_slot <= 9 && pending_.has_value() && !pending_->name.empty() &&
         pending_->slot == s.player->save_slot && play_rising_;
}

void Core::start_playthrough(const proto::WorldSnapshot& s, Outputs& out) {
  current_ = pending_;
  pending_.reset();
  ++state_.playthroughs;
  enter(Phase::Playthrough, s.mono_ms, out);
  // Without the store there is no database to open (§9's DbOpened): the world follows at once.
  enter(Phase::InWorld, s.mono_ms, out);
}

void Core::on_snapshot(const proto::WorldSnapshot& s, Outputs& out) {
  ++state_.snapshots;
  const Mono at = s.mono_ms;
  if (gate_.step(s)) {
    if (gate_.open()) {
      out.changes.push_back(Change{Change::What::GateOpened, 0, 0, at});
    } else {
      out.changes.push_back(Change{Change::What::GateClosed, static_cast<std::uint8_t>(gate_.why()), 0, at});
      track_none(out);  // §5.2: the render feed stops
    }
  }
  state_.gate_open = gate_.open();
  state_.gate_why = gate_.why();

  // What C2 would have said, rebuilt from the snapshot when its message was lost: a structural trip, a later
  // anti-cheat hit (§5.3: the gate stays false for the rest of the session, with the banner).
  if (s.health.of(proto::Group::Session).state == proto::GroupState::Tripped) {
    raise_banner(BannerReason::SessionFailed, at, out);
  } else if (s.health.of(proto::Group::World).state == proto::GroupState::Tripped ||
             s.health.of(proto::Group::Player).state == proto::GroupState::Tripped) {
    raise_banner(BannerReason::TaskTripped, at, out);
  }
  if (s.session.anticheat) raise_banner(BannerReason::AntiCheat, at, out);

  // The playthrough's own clocks: play time rising over two snapshots, and the load counter held over two.
  if (s.player) {
    play_rising_ = prev_play_ms_ && *prev_play_ms_ > 0 && s.player->play_time_ms > *prev_play_ms_;
    prev_play_ms_ = s.player->play_time_ms;
  } else {
    play_rising_ = false;
    prev_play_ms_.reset();
  }
  const bool in_world_now = s.life.main_player && s.life.screen == proto::Screen::InGame;
  const bool counter_changed = s.life.load_counter && prev_counter_ && *s.life.load_counter != *prev_counter_;
  if (in_world_now) {
    stable_ = static_cast<std::uint8_t>(
        (prev_in_world_ && s.life.load_counter == prev_counter_) ? (stable_ < 2 ? stable_ + 1 : 2) : 1);
  } else {
    stable_ = 0;
  }
  prev_counter_ = s.life.load_counter;
  prev_in_world_ = in_world_now;

  const bool at_title = !s.life.main_player && s.life.screen == proto::Screen::Title;
  switch (state_.phase) {
    case Phase::Banner:
      return;
    case Phase::Gate:
      // DataReady lost (C2 overflowed): the task ticks and phase A passed if the structural groups are Ok.
      if (!structural_ok(s.health)) return;
      if (caps_.groups == proto::GroupSet::None) caps_ = caps_of(s.health);
      enter(Phase::Ready, at, out);
      if (state_.level == Level::Starting) set_level(Level::L2, at, out);
      [[fallthrough]];
    case Phase::Ready:
      if (s.life.main_player) {
        if (ready_for_playthrough(s)) start_playthrough(s, out);  // a very late load (§3.3)
      } else if (at_title) {
        enter(Phase::Title, at, out);
      } else if (s.life.screen == proto::Screen::Loading) {
        enter(Phase::Loading, at, out);  // loaded during a loading screen (§3.3)
      }
      return;
    case Phase::Title:
      if (s.life.main_player && ready_for_playthrough(s)) start_playthrough(s, out);
      return;
    case Phase::Playthrough:
      enter(Phase::InWorld, at, out);  // never left here without the store; kept for when it waits
      return;
    case Phase::InWorld:
      if (at_title) {
        enter(Phase::Title, at, out);
        current_.reset();
      } else if (!s.life.main_player || counter_changed || s.life.screen != proto::Screen::InGame) {
        enter(Phase::Loading, at, out);
      }
      return;
    case Phase::Loading:
      if (at_title) {
        enter(Phase::Title, at, out);
        current_.reset();
        return;
      }
      if (!in_world_now || stable_ < 2) return;
      if (!current_ || (pending_ && *pending_ != *current_)) {
        if (ready_for_playthrough(s)) start_playthrough(s, out);  // another character, or the first one
        return;
      }
      if (pending_ ||
          (s.player && s.player->save_slot == current_->slot && s.player->archetype == current_->archetype)) {
        pending_.reset();
        enter(Phase::InWorld, at, out);
      }
      return;
  }
}

void Core::settle(Mono at, Outputs& out) {
  const bool in_world = state_.phase == Phase::InWorld;
  // The badge: the gate closed in a world for a reason of the session (another player, another world, a lobby), up
  // until the gate opens again; a loading screen alone never raises it (impl).
  if (state_.gate_open) {
    paused_multiplayer_ = false;
  } else if (in_world && state_.gate_why == Unclean::NotSolo) {
    paused_multiplayer_ = true;
  } else if (state_.phase == Phase::Title) {
    paused_multiplayer_ = false;
  }
  const bool badge = in_world && !state_.gate_open && paused_multiplayer_ && state_.level != Level::L1;
  if (badge != state_.badge) {
    out.changes.push_back(Change{Change::What::Badge, state_.badge ? std::uint8_t{1} : std::uint8_t{0},
                                 badge ? std::uint8_t{1} : std::uint8_t{0}, at});
    state_.badge = badge;
  }
  state_.dormant = !(in_world && state_.gate_open);
}

}  // namespace en::core
