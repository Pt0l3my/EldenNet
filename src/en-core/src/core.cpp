// en-core: the lifecycle, the gate's effects, the ladder (core.hpp; ARCH §5, §6.8, §9), Augur on every step, and the
// first conversation wired in after it (WHY§364).
#include "en/core/core.hpp"

#include <algorithm>
#include <array>
#include <type_traits>
#include <utility>

#include "content/wording.hpp"  // the lexicon's banned words, for the memory job's hygiene
#include "en/core/choice.hpp"
#include "en/core/profile.hpp"
#include "en/core/speech_content.hpp"
#include "en/core/speech_lint.hpp"
#include "en/core/speech_pager.hpp"
#include "en/core/speech_sanitizer.hpp"

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
    case Level::L3:
      return "L3";
    case Level::L4:
      return "L4";
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
    case BannerReason::SpikeRun:
      return "spike_run";
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
    // A build in EldenNet's table that it cannot read: what failed, in plain words (O114). Its own files for the build
    // missing or damaged, or its reading of the build not complete yet.
    case BannerReason::DiscoveryFailed:
      if (reason == "gal_files" || reason == "files") {
        out = "EldenNet inactive: its files for game build ";
        out += build;
        out += " are incomplete. Reinstall EldenNet, or look for an update.";
        return out;
      }
      [[fallthrough]];
    case BannerReason::RegisterFaulted:
    case BannerReason::Inert:
      out = "EldenNet inactive: it cannot read game build ";
      out += build;
      out += " yet. Look for an update of EldenNet.";
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
    case BannerReason::SpikeRun:
      return "EldenNet test run: it makes no call into the game";
  }
  return out;
}

// ---- Augur's side (ARCH §8; part 11) ----

augur::Subject AugurView::classify(const proto::Chr& chr) const {
  // The Tarnished; a persona by its binding (NpcParam row, then entity id: identity.hpp), at its active stage; every
  // other character is Mute until cards and character profiles exist (v0.4).
  augur::Subject s;
  if (player && chr.handle == *player) {
    s.kind = augur::Subject::Kind::Player;
    return s;
  }
  if (directory == nullptr) return s;
  const auto bound = directory->index().resolve(chr);
  if (!bound || bound->persona >= directory->size()) return s;
  const Persona& p = directory->persona(bound->persona);
  const std::size_t stage = active_stage(bound->persona);
  s.kind = augur::Subject::Kind::Persona;
  s.persona.index = static_cast<std::uint32_t>(bound->persona);
  s.stage_muted = p.chat_at(stage) == ChatMode::Mute;
  s.chat = p.chat_at(stage) == ChatMode::Allowed;
  return s;
}

bool AugurView::eval(augur::PredicateId pred, const augur::EvalScope&) const { return predicates_.eval(pred, facts); }

// MEM §1.2's candidates, but alive (Augur reads the dead bit): a persona of the loaded content, conscious at its active
// stage (asleep or senseless leave audiences; unknown holds back only own-initiative lines) and not at a mute stage.
// speech.toml's `perceivable` and `may_remember` come with cards (v0.4): every persona remembers until then.
bool AugurView::persona_may_perceive(augur::PersonaId id) const {
  if (directory == nullptr || id.index >= directory->size()) return false;
  const Persona& p = directory->persona(id.index);
  const std::size_t stage = active_stage(id.index);
  const Conscious c = p.state_at(stage);
  return c != Conscious::Asleep && c != Conscious::Senseless && p.chat_at(stage) != ChatMode::Mute;
}

std::size_t AugurView::active_stage(std::size_t persona) const noexcept {
  return known != nullptr && persona < known->personas.size() ? known->personas[persona].stage : 0;
}

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

// The built-in speech parts (WHY§364): part A's sanitizer and pager, and its lint and prompt over the loaded content's
// lexicon and templates (speech_content.hpp). The conversation holds the parts by reference for the core's life, so the
// lint and the prompt stand for whichever content is loaded now: a load swaps what they forward to (a reply's lint
// keeps the lexicon it opened with). Before any load no persona exists, so neither is reached; if one were, it fails
// closed: every render refuses and every sentence stops.
struct BuiltinSpeech {
  class LoadedLint final : public Lint {
   public:
    [[nodiscard]] std::unique_ptr<ReplyLint> open(const LintContext& context) const override {
      if (current) return current->open(context);
      return std::make_unique<Closed>();
    }
    std::shared_ptr<const SpeechLint> current;

   private:
    class Closed final : public ReplyLint {
     public:
      LintVerdict check(Sentence&) override { return LintVerdict{LintAction::Stop, HardList::Banned, {}}; }
      [[nodiscard]] LintTally tally() const override { return {}; }
    };
  };
  class LoadedPrompt final : public PromptRenderer {
   public:
    [[nodiscard]] proto::Expected<RenderedPrompt, PromptError> render(const DialogueContext& context) const override {
      if (current) return current->render(context);
      return proto::Unexpected{PromptError{PromptRefusal::Template, "no content is loaded"}};
    }
    std::shared_ptr<const DialoguePrompt> current;
  };

  SpeechSanitizer sanitizer;
  LoadedLint lint;
  SpeechPager pager;
  LoadedPrompt prompt;

  [[nodiscard]] SpeechParts parts() const { return SpeechParts{sanitizer, lint, pager, prompt}; }
};

Core::Core() : augur_(world_facts()), builtin_(std::make_unique<BuiltinSpeech>()), conversation_(builtin_->parts()) {
  init();
}

Core::Core(SpeechParts speech) : augur_(world_facts()), conversation_(speech) { init(); }

void Core::init() {
  std::vector<augur::RuleFile> files;
  for (const BuiltinFile& f : builtin_rule_files()) files.push_back(augur::RuleFile{std::string(f.name), std::string(f.text)});
  rule_errors_ = load_rules(files);
  hostile_teams_ = world_facts().hostile_teams;
  view_.directory = &directory_;  // empty until a load: every character is Mute until then
  view_.known = &known_;
}

Core::~Core() = default;

std::vector<ContentFile> builtin_content() {
  std::vector<ContentFile> files;
  for (const BuiltinFile& f : builtin_content_files()) {
    files.push_back(ContentFile{std::string(f.name), std::string(f.text), ContentOrigin::Shipped});
  }
  return files;
}

std::vector<ContentError> Core::load_content(std::span<const ContentFile> files, Realm realm) {
  auto pack = core::load_content(files);
  if (!pack) return pack.error();  // the old content stays
  // The pack's prompts and lexicon, and block 2's content lint: any error refuses the pack (WHY§388). The pack is
  // checked whole whichever speech parts the core was given.
  auto speech = load_speech(*pack);
  if (!speech) return speech.error();
  // The player's layers (profile.hpp): their wording and block 4 checked with the pack's speech parts, then each that
  // holds no error merged into its persona. A layer's errors refuse that layer alone.
  std::vector<LayerFinding> layer_findings = pack->layer_findings;
  apply_layers(*pack, *speech->prompt, speech->lint->lexicon(), layer_findings);
  // Quest and world awareness: the world files and every predicate compiled (knowledge.hpp); errors refuse the pack.
  auto knowledge = Knowledge::build(*pack);
  if (!knowledge) return knowledge.error();
  content_warnings_ = pack->warnings;  // disable.toml's, then block 2's, then the player's layers'
  content_warnings_.insert(content_warnings_.end(), speech->warnings.begin(), speech->warnings.end());
  for (const LayerFinding& f : layer_findings) content_warnings_.push_back(to_content_error(f));
  layer_findings_ = std::move(layer_findings);
  if (builtin_) {
    builtin_->lint.current = std::move(speech->lint);
    builtin_->prompt.current = std::move(speech->prompt);
  }
  memory_prompts_.clear();
  for (const ContentFile& f : pack->others) {
    if (f.path.rfind("prompts/memory/", 0) == 0 || f.path.rfind("prompts/lived/", 0) == 0) {
      memory_prompts_.push_back(f);
    }
  }
  directory_ = Directory::build(std::move(*pack), realm, predicates_);
  knowledge_ = std::move(*knowledge);
  realm_ = realm;
  watch_ = knowledge_.watch(realm, ++watch_version_);
  watch_pending_ = !watch_.ids.empty();
  forget_flags();
  content_loaded_ = true;
  return {};
}

std::optional<std::string> Core::set_behaviour(const Behaviour& b) {
  if (auto bad = check_behaviour(b)) return bad;
  behaviour_ = b;
  return std::nullopt;
}

void Core::forget_flags() {
  flags_ = knowledge_.unread();
  known_ = knowledge_.view(flags_);
  flags_read_ = false;
  stage_moves_.clear();
}

// MEM §6.6: the flag state from the newest processed frame of the current watch list; the view only when it changes.
void Core::on_flags(const proto::WorldSnapshot& s) {
  if (!s.flags || watch_.ids.empty() || s.flags->watch_version != watch_.version) return;
  const std::uint8_t journey = s.player ? static_cast<std::uint8_t>(s.player->ng_lvl + 1) : flags_.journey;
  FlagState next = knowledge_.from_frame(watch_, *s.flags, realm_, journey);
  if (next == flags_) return;
  flags_ = std::move(next);
  KnowledgeView view = knowledge_.view(flags_);
  // The knowledge subscriber (MEM §1.1): a persona whose active stage moved in play gives a StageChanged. The first
  // frame of a playthrough only says where things stand.
  if (flags_read_) {
    for (std::size_t i = 0; i < view.personas.size() && i < known_.personas.size(); ++i) {
      if (view.personas[i].stage != known_.personas[i].stage) {
        stage_moves_.push_back({static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(known_.personas[i].stage),
                                static_cast<std::uint32_t>(view.personas[i].stage)});
      }
    }
  }
  flags_read_ = true;
  known_ = std::move(view);
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
      view_.facts.runes_held = s->player->runes_held;
      view_.facts.journey = s->player->ng_lvl;
    } else {
      view_.player.reset();
    }
  }
  augur::EventBatch batch;
  augur_.step(augur::AugurInput{s, facts}, now, view_, batch);
  for (const auto& [persona, from, to] : stage_moves_) {
    augur::Event ev;
    ev.id = augur_.next_id();
    ev.kind = augur::StageChanged{augur::PersonaId{persona}, static_cast<augur::StageId>(from),
                                  static_cast<augur::StageId>(to)};
    ev.event_class = augur::Class::P;
    ev.t_mono_ms = now;
    if (last_ && last_->player) {
      ev.t_play_ms = last_->player->play_time_ms;
      ev.journey = last_->player->ng_lvl;
    } else if (s != nullptr && s->player) {
      ev.t_play_ms = s->player->play_time_ms;
      ev.journey = s->player->ng_lvl;
    }
    ev.source = augur::SourceId::Subscriber;
    ev.subject = augur::ActorRef{augur::ActorRef::Kind::Persona, {}, persona, 0};
    ev.radius = augur::Radius::Subject;
    augur_.resolve_audience(ev, view_);
    batch.push_back(std::move(ev));
  }
  stage_moves_.clear();
  bus_.run(std::move(batch), now, augur_, view_, out.events);
}

Outputs Core::step(CoreInput input, Mono now) {
  Outputs out;
  tracked_none_sent_ = false;
  Mono at = now;
  const proto::WorldSnapshot* snapshot = nullptr;
  const proto::Lifecycle* lifecycle = nullptr;
  const proto::UiEvent* ui = nullptr;
  const proto::LlmEvent* llm = nullptr;
  std::visit(Overloaded{
                 [&](std::unique_ptr<proto::WorldSnapshot>& s) {
                   if (s) {
                     at = s->mono_ms;
                     snapshot = s.get();
                     on_snapshot(*s, out);
                     if (state_.phase == Phase::InWorld || state_.phase == Phase::Loading) on_flags(*s);
                   }
                 },
                 [&](proto::Lifecycle& l) {
                   lifecycle = &l;
                   on_lifecycle(l, now, out);
                 },
                 [&](BootBanner& b) { on_banner(b, now, out); },
                 [&](CoreInput::Tick&) {},
                 [&](proto::UiEvent& e) {
                   // The rollback choice's panel (MEM §9.1, O144): its answer and its close are the core's alone.
                   if (const auto* c = std::get_if<proto::UiEvent::Choose>(&e.event)) {
                     if (choice_up_ && c->panel == choice_id_ && store_.mode() == StorePath::Mode::Choice) {
                       choice_up_ = false;
                       store_.resolve(static_cast<proto::db::Choice>(c->answer));
                     }
                   } else if (const auto* x = std::get_if<proto::UiEvent::ChoiceClosed>(&e.event)) {
                     if (choice_up_ && x->panel == choice_id_) {
                       choice_up_ = false;
                       choice_closed_at_ = now;  // asked again at the next safe moment
                     }
                   } else {
                     if (std::holds_alternative<proto::UiEvent::OpenChat>(e.event) &&
                         store_.mode() == StorePath::Mode::Choice) {
                       choice_wanted_ = true;  // the talk key while it waits: the panel again, once it is safe
                     }
                     ui = &e;
                   }
                 },
                 [&](proto::LlmEvent& e) {
                   if (e.id >= kMemoryRequests) {
                     on_memory_event(e, now, out);  // the memory job's: never the conversation's
                   } else {
                     llm = &e;
                   }
                 },
                 [&](proto::NetHealth& h) { net_ = h; },
                 [&](CoreInput::Pause& p) { state_.ai_paused = p.on; },
                 [&](proto::DbResult& r) { on_store(r, now, out); },
                 [&](CoreInput::Account& a) {
                   store_.attach(a.steamid64, a.why);
                   no_account_ = a.steamid64 == 0;
                 },
                 [&](CoreInput::MemoryChoice& c) { store_.resolve(c.choice); },
                 [&](CoreInput::Forget& f) {
                   if (!can_forget(f.journey)) return;
                   const proto::Pt pt = last_ && last_->player ? static_cast<proto::Pt>(last_->player->play_time_ms)
                                                               : 0;
                   if (store_.forget(f.journey, f.persona, pt)) ++forgets_;
                 },
                 [&](CoreInput::StoreBacklog& b) {
                   if (b.behind && !backlog_.behind) {
                     conversation_.notice(ToastClass::Degraded, proto::ToastLevel::Warning, std::string(kMemoryBehind),
                                          now);
                   }
                   backlog_ = b;
                 },
             },
             input.payload);
  store_.set_enabled(behaviour_.features.memory);
  store_.tick(now);
  if (watch_pending_) {  // the watch list of the last load (MEM §6.2): sent once, shared, never copied
    out.game.push_back(proto::GameCommand{proto::GameCommand::SetWatchFlags{
        watch_.version, std::make_shared<const std::vector<std::uint32_t>>(watch_.ids)}});
    watch_pending_ = false;
  }
  settle(at, out);
  climb(at, out);
  out.ai_allowed = state_.gate_open && state_.level >= Level::L2;
  out.state = state_;
  run_augur(snapshot, lifecycle, now, out);
  if (snapshot != nullptr) {
    auto& held = std::get<std::unique_ptr<proto::WorldSnapshot>>(input.payload);
    last_ = std::move(held);  // the newest, which the conversation reads until the next one
  }
  run_conversation(at, snapshot != nullptr, ui, llm, out);
  run_memory(at, out);
  return out;
}

// ARCH §6.8: L2, then L4 with a healthy dialogue model and every group up, L3 with one optional group tripped, and
// back to L2 when the model is not healthy or AI is paused. L1 stays for the session (set_level).
void Core::climb(Mono at, Outputs& out) {
  if (state_.level < Level::L2) return;
  const Level to = !net_.dialogue || state_.ai_paused ? Level::L2 : (optional_tripped_ ? Level::L3 : Level::L4);
  set_level(to, at, out);
}

void Core::run_conversation(Mono at, bool fresh, const proto::UiEvent* ui, const proto::LlmEvent* llm, Outputs& out) {
  // The Tarnished's engagements and death, from Augur's events of this step.
  bool died = false;
  for (const augur::Event& e : out.events) {
    if (std::holds_alternative<augur::PlayerDied>(e.kind)) died = true;
    const auto other = [](const augur::ActorRef& a, const augur::ActorRef& b) -> std::optional<std::uint64_t> {
      if (a.kind == augur::ActorRef::Kind::Player) return b.handle.raw;
      if (b.kind == augur::ActorRef::Kind::Player) return a.handle.raw;
      return std::nullopt;
    };
    if (const auto* en = std::get_if<augur::Engaged>(&e.kind)) {
      if (const auto h = other(en->subject, en->opponent);
          h && std::find(engaged_.begin(), engaged_.end(), *h) == engaged_.end()) {
        engaged_.push_back(*h);
      }
    } else if (const auto* dis = std::get_if<augur::Disengaged>(&e.kind)) {
      if (const auto h = other(dis->subject, dis->opponent)) std::erase(engaged_, *h);
    }
  }
  if (state_.phase != Phase::InWorld) engaged_.clear();
  ConversationInput in;
  in.now = at;
  in.ui = ui;
  in.llm = llm;
  ConversationWorld& w = in.world;
  w.snapshot = last_.get();
  w.fresh = fresh;
  w.in_world = state_.phase == Phase::InWorld;
  w.loading = state_.phase == Phase::Loading;
  w.dormant = state_.dormant;
  w.speech = state_.level >= Level::L3 && out.ai_allowed;
  w.combat = !engaged_.empty();
  w.player_died = died;
  w.cast = content_loaded_ ? &directory_ : nullptr;
  w.predicates = &predicates_;
  w.facts = view_.facts;
  w.hostile_teams = hostile_teams_;
  w.log = &log_;
  w.knowledge = content_loaded_ ? &known_ : nullptr;
  w.chat = behaviour_.features.chat;
  w.self_awareness = behaviour_.features.self_awareness;
  if (store_.mode() == StorePath::Mode::Choice) w.chat_held = kChoicePending;
  w.pages_held = backlog_.pages_held;
  if (store_.mode() == StorePath::Mode::Open) w.store = &store_;
  if (content_loaded_) {
    w.world_model = &knowledge_;
    w.flags = &flags_;
  }
  ConversationOutput co = conversation_.step(in);
  for (const Commit& c : co.commits) log_.apply(c);  // in the same step (NPC §8.7 item 5), and to the store below
  store_.write(out.events, directory_, content_loaded_ ? &known_ : nullptr, at);
  speech_events(co.commits, at, out);
  store_.pump(at, out.db);
  out.ui = std::move(co.ui);
  choice_model(at, out);
  // The conversation's requests after any the step made already (the memory job's, made on a store answer).
  out.requests.insert(out.requests.end(), std::make_move_iterator(co.requests.begin()),
                      std::make_move_iterator(co.requests.end()));
  out.calls = std::move(co.calls);
  out.cancels = std::move(co.cancels);
  out.commits = std::move(co.commits);
  out.ended = co.ended;
  // C4: nothing goes out while gated, nor while AI is paused (D15); the conversation sends none then (below L3), and
  // a request dropped here takes its note with it.
  if (!out.ai_allowed || state_.ai_paused) {
    out.requests.clear();
    std::erase_if(out.calls, [](const CallNote& n) { return n.what == CallNote::What::Sent; });
  }
}

std::vector<std::pair<std::string, bool>> Core::present() const {
  std::vector<std::pair<std::string, bool>> out;
  if (!last_ || !content_loaded_ || state_.phase != Phase::InWorld) return out;
  for (const proto::Chr& c : last_->chars) {
    const auto bound = directory_.index().resolve(c);
    if (!bound || bound->persona >= directory_.size()) continue;
    const std::string& id = directory_.persona(bound->persona).id;
    const auto same = [&](const auto& p) { return p.first == id; };
    if (std::none_of(out.begin(), out.end(), same)) out.emplace_back(id, c.dead);
  }
  return out;
}

bool Core::can_forget(std::uint16_t journey) const noexcept {
  return store_.mode() == StorePath::Mode::Open && store_.journey() == journey &&
         conversation_.state() == proto::ConvState::Closed;
}

// The rollback choice's panel (MEM §9.1, O144; WHY§690): in the model while the choice waits and the moment is calm (a
// world, no danger, no menu), with a new id each time it is asked; taken down when the moment stops being calm, and,
// closed without an answer, asked again kChoiceAgainMs later at the next calm moment, or at the talk key.
void Core::choice_model(Mono now, Outputs& out) {
  if (store_.mode() != StorePath::Mode::Choice || !store_.choice()) {
    choice_up_ = false;
    choice_wanted_ = false;
    choice_closed_at_.reset();
    return;
  }
  const bool calm = conversation_.calm();
  if (choice_up_ && !calm) {
    choice_up_ = false;
    choice_closed_at_ = now;
  }
  if (!choice_up_ && calm &&
      (!choice_closed_at_ || choice_wanted_ || now - *choice_closed_at_ >= kChoiceAgainMs)) {
    choice_up_ = true;
    choice_wanted_ = false;
    ++choice_id_;
  }
  if (!choice_up_) return;
  const NameOf name_of = [this](std::string_view persona) -> std::optional<std::string> {
    if (!content_loaded_) return std::nullopt;
    const std::optional<std::size_t> i = directory_.find(persona);
    if (!i) return std::nullopt;
    return directory_.persona(*i).display_name_fallback;
  };
  out.ui.choice = choice_panel(*store_.choice(), choice_id_, name_of);
}

namespace {

augur::EndReason augur_reason(EndReason r) noexcept {
  switch (r) {
    case EndReason::Travel:
      return augur::EndReason::Travel;
    case EndReason::Death:
      return augur::EndReason::Death;
    case EndReason::Dormant:
      return augur::EndReason::Dormant;
    case EndReason::Idle:
      return augur::EndReason::Timeout;
    default:
      return augur::EndReason::Player;
  }
}

}  // namespace

// MEM §1.1, §1.2: each commit's event, its audience resolved and frozen by Augur as it stands (speech 16 m from the
// speaker: direct for the one spoken to, overheard by others; a conversation's end the persona only), and dispatched
// after Augur's own. A later page grows its NpcSaid, whose audience was frozen with its first.
void Core::speech_events(const std::vector<Commit>& commits, Mono at, Outputs& out) {
  if (commits.empty()) return;
  augur::EventBatch batch;
  const proto::Player* pl = last_ && last_->player ? &*last_->player : nullptr;
  const KnowledgeView* known = content_loaded_ ? &known_ : nullptr;
  const proto::Pt pt_now = pl != nullptr ? static_cast<proto::Pt>(pl->play_time_ms) : 0;
  for (const Commit& c : commits) {
    if (c.kind == Commit::Kind::NpcPage || c.kind == Commit::Kind::CutOff) {
      store_.commit(c, nullptr, directory_, known, pt_now);
      continue;
    }
    const auto idx = directory_.find(c.persona);
    if (!idx) continue;
    // The partner as Augur tracks it, when present (its position is the speech's origin); else by id alone.
    augur::ActorRef partner{augur::ActorRef::Kind::Persona, {}, static_cast<std::uint32_t>(*idx), 0};
    for (const augur::Tracker& t : augur_.world().trackers()) {
      if (t.present && t.subject.kind == augur::Subject::Kind::Persona && t.subject.persona.index == *idx) {
        partner = augur::ActorRef::of(t.subject, t.handle);
        break;
      }
    }
    augur::Event ev;
    ev.id = augur_.next_id();
    ev.t_mono_ms = at;
    ev.t_play_ms = pl != nullptr ? pl->play_time_ms : 0;
    ev.journey = pl != nullptr ? pl->ng_lvl : 0;
    if (pl != nullptr) ev.place.block = pl->block;
    if (last_ && last_->place) {
      ev.place.place_id = last_->place->place_id;
      ev.place.region = last_->place->region;
    }
    ev.source = augur::SourceId::Pipeline;
    const augur::PersonaId persona{static_cast<std::uint32_t>(*idx)};
    const auto conv = static_cast<augur::ConvId>(c.conv);
    switch (c.kind) {
      case Commit::Kind::Started:
        ev.kind = augur::ConversationStarted{persona, conv, augur::Opener{}};
        ev.event_class = augur::Class::T;
        ev.subject = partner;
        ev.radius = augur::Radius::None;
        break;
      case Commit::Kind::PlayerSaid:
        ev.kind = augur::PlayerSaid{partner, std::make_shared<const std::string>(c.text), conv,
                                    augur::InputSource::Typed};
        ev.event_class = augur::Class::P;
        ev.subject = augur::ActorRef::player();
        ev.object = partner;
        ev.radius = augur::Radius::Speech;
        break;
      case Commit::Kind::NpcSaid: {
        augur::NpcSaid e;
        e.speaker = partner;
        e.to = augur::ActorRef::player();
        e.text = std::make_shared<const std::string>(c.text);
        e.conv = conv;
        e.pages = 1;
        ev.kind = std::move(e);
        ev.event_class = augur::Class::P;
        ev.subject = partner;
        ev.object = augur::ActorRef::player();
        ev.radius = augur::Radius::Speech;
        break;
      }
      case Commit::Kind::Ended:
        ev.kind = augur::ConversationEnded{persona, conv, augur_reason(c.reason), c.player_lines, c.committed, {}};
        ev.event_class = augur::Class::P;
        ev.subject = partner;
        ev.radius = augur::Radius::Subject;
        break;
      case Commit::Kind::NpcPage:
      case Commit::Kind::CutOff:
        continue;
    }
    augur_.resolve_audience(ev, view_);
    store_.commit(c, c.kind == Commit::Kind::Started ? nullptr : &ev, directory_, known, pt_now);
    batch.push_back(std::move(ev));
  }
  if (!batch.empty()) bus_.run(std::move(batch), at, augur_, view_, out.events);
}

// C8's answers (store_path.hpp): the world goes on once the open answered, whichever way, and the store's toasts.
void Core::on_store(const proto::DbResult& r, Mono now, Outputs& out) {
  StorePath::Effect e = store_.on_result(r);
  if (e.ready && state_.phase == Phase::Playthrough) enter(Phase::InWorld, now, out);
  for (StoreNotice& n : e.notices) conversation_.notice(n.cls, n.level, std::move(n.text), now);
  if (e.job_input) on_job_input(*e.job_input, now, out);
  if (e.lived_input) on_lived_input(*e.lived_input, e.lived_persona, now, out);
  if (store_.mode() != StorePath::Mode::Open) memory_run_.reset();  // memory off, or a choice: the job waits
}

// ---- The memory job (memory_job.hpp; MEM §3.2, §3.6, §3.7) ----

// MEM §2.5: slew:<boss> holds when the boss's flag holds and the persona knows the world stage of that boss; the
// Great Runes are not read in v0.3, so great_runes>=n never holds yet.
bool Core::claim_holds(std::size_t persona, std::string_view key) const {
  if (key.rfind("slew:", 0) != 0 || persona >= known_.personas.size()) return false;
  const std::string boss(key.substr(5));
  if (knowledge_.eval("boss('" + boss + "')", flags_, known_.world) != Tri::T) return false;
  for (const Known& k : known_.personas[persona].known) {
    for (const WorldStage& w : knowledge_.stages()) {
      if (k.item == "w:" + w.id && w.boss == boss) return true;
    }
  }
  return false;
}

// MEM §3.2: one job at a time, oldest first, on 10 s of calm in a world whose gate is open, with no fight, no rollback
// choice and memory on. The core knows no provider's kind yet, so a cloud's rules hold: a job in flight when the calm
// ends is let finish.
void Core::run_memory(Mono at, Outputs& out) {
  const bool calm = state_.phase == Phase::InWorld && out.ai_allowed && !state_.ai_paused &&
                    store_.mode() == StorePath::Mode::Open && engaged_.empty() &&
                    !(last_ && !last_->boss.bars.empty());
  if (!calm) {
    calm_since_.reset();
  } else if (!calm_since_) {
    calm_since_ = at;
  }
  if (memory_run_) {
    if (memory_run_->retry_at && at >= *memory_run_->retry_at && calm) send_memory(at, out);
    return;
  }
  if (!calm_since_ || at < *calm_since_ + 10000) return;
  const std::vector<proto::MemoryJob> ready = store_.runnable();
  if (ready.empty()) {
    // MEM §13.2: the lived layer at quiet moments only: no journal job waits, 30 s of calm, no conversation with the
    // NPC open, and at most one update per NPC per 2 h of play.
    if (at < *calm_since_ + 30000) return;
    const proto::Pt pt = last_ && last_->player ? static_cast<proto::Pt>(last_->player->play_time_ms) : 0;
    const std::optional<std::string> partner = conversation_.partner();
    for (const auto& [persona, due] : lived_due_) {
      if (!due.due() || (partner && *partner == persona)) continue;
      if (const auto next = lived_next_.find(persona); next != lived_next_.end() && pt < next->second) continue;
      MemoryRun run;
      run.lived = true;
      run.job.persona = persona;
      run.job.kind = proto::JobKind::Lived;
      memory_run_ = std::move(run);
      store_.want_lived_input(persona);
      store_.pump(at, out.db);
      return;
    }
    return;
  }
  MemoryRun run;
  run.job = ready.front();
  run.job.state = proto::JobState::Running;
  store_.save_job(run.job);
  store_.want_job_input(run.job.id);
  memory_run_ = std::move(run);
  store_.pump(at, out.db);
}

void Core::on_job_input(const proto::db::JobInputResult& r, Mono at, Outputs& out) {
  if (!memory_run_ || r.job.id != memory_run_->job.id) return;
  const auto idx = directory_.find(r.job.persona);
  InputCtx ctx;
  if (idx) {
    ctx.persona = &directory_.persona(*idx);
    ctx.stage = *idx < known_.personas.size() ? known_.personas[*idx].stage : 0;
  }
  if (const auto f = store_.facts().find(r.job.persona); f != store_.facts().end()) ctx.facts = &f->second;
  for (const auto& [item, certainty] : store_.job_news(r.job.persona)) {
    const std::string sh = knowledge_.short_of(item);
    const char* word = certainty == "fact" ? "knew" : certainty == "belief" ? "believed" : "heard";
    if (!sh.empty()) ctx.new_world.push_back(std::string("(") + word + ") " + sh);
  }
  ctx.knowledge = &knowledge_;
  ctx.now = last_ && last_->player ? static_cast<proto::Pt>(last_->player->play_time_ms) : 0;
  memory_run_->input = assemble_input(r, ctx);
  send_memory(at, out);
}

void Core::send_memory(Mono at, Outputs& out) {
  MemoryRun& run = *memory_run_;
  run.retry_at.reset();
  if (run.lived) {
    if (!run.lived_input) return;
    auto messages = lived_messages(memory_prompts_, *run.lived_input, retry_reminder(run.failure));
    if (!messages) {
      lived_due_.erase(run.job.persona);
      memory_run_.reset();
      return;
    }
    proto::ChatRequest c;
    c.slot = std::string(proto::kMemory);
    c.purpose = proto::Purpose::Background;
    c.messages = std::move(*messages);
    c.json_schema = lived_schema();
    run.request = next_memory_id_++;
    out.requests.push_back(proto::LlmRequest::chat(*run.request, std::move(c)));
    return;
  }
  if (!run.input) return;
  auto messages = memory_messages(memory_prompts_, *run.input, retry_reminder(run.failure));
  if (!messages) {  // the pack's prompts are broken: a defect, the job waits for the next session
    run.job.state = proto::JobState::Deferred;
    run.job.last_error = "prompt";
    store_.save_job(run.job);
    memory_run_.reset();
    return;
  }
  proto::ChatRequest c;
  c.slot = std::string(proto::kMemory);
  c.purpose = proto::Purpose::Background;
  c.messages = std::move(*messages);
  c.json_schema = memory_schema();
  run.request = next_memory_id_++;
  out.requests.push_back(proto::LlmRequest::chat(*run.request, std::move(c)));
  (void)at;
}

void Core::on_memory_event(const proto::LlmEvent& e, Mono at, Outputs& out) {
  if (!memory_run_ || !memory_run_->request || e.id != *memory_run_->request || e.kind != proto::EventKind::Ended) {
    return;
  }
  MemoryRun& run = *memory_run_;
  run.request.reset();
  const proto::ChatResult result = e.chat.value_or(proto::ChatResult{});
  if (run.lived) {
    on_lived_reply(result, at, out);
    return;
  }
  const auto idx = directory_.find(run.job.persona);
  ValidateCtx vctx;
  vctx.character = current_ ? std::string(current_->name.view()) : std::string();
  if (builtin_ && builtin_->lint.current) {
    const SpeechLint* lint = builtin_->lint.current.get();
    vctx.banned = [lint](std::string_view s) { return wording::banned(lint->lexicon(), s); };
  }
  for (const CatalogueFlag& f : knowledge_.catalogue().flags) vctx.aliases.push_back(f.alias);
  vctx.proved = [this, idx](std::string_view key) { return idx && claim_holds(*idx, key); };
  vctx.now = last_ && last_->player ? static_cast<proto::Pt>(last_->player->play_time_ms) : 0;
  proto::Expected<Validated, JobFailure> v = proto::Unexpected{JobFailure::Provider};
  if (result.error == proto::ErrorClass::None && run.input) {
    v = validate_reply(result.text, result.finish_reason, *run.input, vctx);
  }
  if (v) {
    proto::JournalVersion jv;
    jv.journey = store_.journey();
    jv.persona = run.job.persona;
    jv.version = v->journal.version;
    jv.as_of_pt = v->journal.as_of_pt;
    jv.from_event_id = run.input->journal ? run.input->journal->upto_event : 0;
    jv.upto_event_id = v->journal.upto_event;
    jv.conv_id = run.input->conv;
    jv.summary = v->journal.summary;
    jv.keepsakes = keepsakes_json(v->journal.keepsakes);
    jv.next_keepsake = v->next_keepsake;
    jv.disposition = v->journal.disposition;
    jv.told_name = v->journal.told_name;
    jv.told_name_verified = v->journal.told_name_verified;
    jv.told_name_heard = v->journal.told_name_heard;
    jv.model_fp = result.model.empty() ? std::string("-") : result.model;
    jv.prompt_hash = "memory-v1";
    jv.persona_hash = idx && !directory_.persona(*idx).hash.empty() ? directory_.persona(*idx).hash : std::string("-");
    jv.job_id = run.job.id;
    count_journal_change(lived_due_[run.job.persona], run.input->journal, v->journal);  // MEM §13.2's due count
    store_.commit_journal(proto::db::CommitJournal{std::move(jv), run.job.id, std::move(v->verified)});
    memory_run_.reset();
    store_.pump(at, out.db);
    return;
  }
  // MEM §3.7: two retries in a run, at +5 s and +30 s, each with its reminder; then deferred, parked after 6 attempts.
  run.failure = v.error();
  ++run.job.attempts;
  if (run.tries < 2) {
    run.retry_at = at + (run.tries == 0 ? 5000 : 30000);
    ++run.tries;
    return;
  }
  run.job.state = run.job.attempts >= 6 ? proto::JobState::Parked : proto::JobState::Deferred;
  run.job.last_error = to_string(run.failure);
  store_.save_job(run.job);
  memory_run_.reset();
  store_.pump(at, out.db);
}

void Core::enter(Phase to, Mono at, Outputs& out) {
  if (state_.phase == to) return;
  const Phase from = state_.phase;
  out.changes.push_back(
      Change{Change::What::Phase, static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to), at});
  state_.phase = to;
  // Leaving a world: nothing stays tracked (§9: InWorld → Loading, → Title).
  if (from == Phase::InWorld) track_none(out);
  if (to == Phase::Title) forget_flags();  // the playthrough's frame is gone with it
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
    if (!proto::structural(t->group)) {
      optional_tripped_ = true;  // an optional group: L3 at most
      return;
    }
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
  forget_flags();  // MEM §6.6: every alias Unknown until this playthrough's first frame
  enter(Phase::Playthrough, s.mono_ms, out);
  // ARCH §9: the key, then Open; the world waits for the answer. Without a store (none attached, or memory off) there
  // is no file to open, and the world follows at once.
  if (store_.available() && current_) {
    const std::uint8_t gift = current_->keepsake;
    store_.open(playthrough_key(store_.account(), current_->slot, current_->name.view(), current_->archetype, gift),
                static_cast<proto::Pt>(s.player->play_time_ms), static_cast<std::uint16_t>(s.player->ng_lvl + 1),
                s.mono_ms);
    return;
  }
  store_.close();
  if (no_account_ && !no_account_told_ && behaviour_.features.memory) {
    no_account_told_ = true;
    conversation_.notice(ToastClass::Degraded, proto::ToastLevel::Warning, std::string(kNoAccount), s.mono_ms);
  }
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
  for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
    if (!proto::structural(static_cast<proto::Group>(g)) && s.health.groups[g].state == proto::GroupState::Tripped) {
      optional_tripped_ = true;
    }
  }

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
      if (!store_.waiting_open()) enter(Phase::InWorld, at, out);  // the store's open answered, or there is none
      return;
    case Phase::InWorld:
      if (at_title) {
        enter(Phase::Title, at, out);
        current_.reset();
        store_.close();  // ARCH §9: flush and close the playthrough's file
      } else if (!s.life.main_player || counter_changed || s.life.screen != proto::Screen::InGame) {
        enter(Phase::Loading, at, out);
      }
      return;
    case Phase::Loading:
      if (at_title) {
        enter(Phase::Title, at, out);
        current_.reset();
        store_.close();
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

namespace en::core {

// MEM §13.3: what the NPC's own prompt shows it now: its first-meeting self and road, the lived record, the journal,
// the facts and the feeling in words, and the people (the Tarnished as P1; other NPCs come with their keepsakes, O45).
void Core::on_lived_input(const proto::db::LivedInputResult& r, const std::string& persona, Mono at, Outputs& out) {
  if (!memory_run_ || !memory_run_->lived || memory_run_->job.persona != persona) return;
  const auto idx = directory_.find(persona);
  std::optional<Journal> journal = r.journal ? journal_of(*r.journal) : std::nullopt;
  if (!idx || !journal) {  // nothing to build on yet
    lived_due_.erase(persona);
    memory_run_.reset();
    return;
  }
  const Persona& p = directory_.persona(*idx);
  const std::size_t stage = *idx < known_.personas.size() ? known_.personas[*idx].stage : 0;
  const nlohmann::json rec = p.resolved(stage);
  LivedJobInput in;
  in.persona = persona;
  in.name = p.display_name_fallback;
  if (const auto pub = rec.find("public"); pub != rec.end() && pub->is_object()) {
    in.self_text = pub->value("summary", std::string());
  }
  if (const auto priv = rec.find("private"); priv != rec.end() && priv->is_object()) {
    const std::string personality = priv->value("personality", std::string());
    if (!personality.empty()) in.self_text += (in.self_text.empty() ? "" : " ") + personality;
    if (const auto seeks = priv->find("seeks"); seeks != priv->end() && seeks->is_array()) {
      for (const auto& s : *seeks) {
        if (s.is_string()) in.seeks.push_back(s.get<std::string>());
      }
    }
  }
  if (const auto st = rec.find("stage"); st != rec.end() && st->is_object()) in.road = st->value("road", std::string());
  if (r.lived) in.lived = lived_of(*r.lived);
  in.journal = *journal;
  proto::PersonaFacts facts;
  facts.persona = persona;
  facts.facts = r.facts;
  BlockNow now;
  now.pt = last_ && last_->player ? static_cast<proto::Pt>(last_->player->play_time_ms) : 0;
  const TarnishedBlock words = tarnished_block(&facts, &in.journal, now);
  in.facts = words.facts;
  in.feeling_words = words.disposition;
  in.people = {{"P1", "the Tarnished"}};
  const std::size_t i = *idx;
  in.proved = [this, i](std::string_view key) { return claim_holds(i, key); };
  memory_run_->lived_input = std::move(in);
  send_memory(at, out);
}

void Core::on_lived_reply(const proto::ChatResult& result, Mono at, Outputs& out) {
  MemoryRun& run = *memory_run_;
  const std::string persona = run.job.persona;
  const proto::Pt pt = last_ && last_->player ? static_cast<proto::Pt>(last_->player->play_time_ms) : 0;
  LivedCheck check;
  check.disposition = run.lived_input ? run.lived_input->journal.disposition : 0;
  check.now = pt;
  proto::Expected<Lived, JobFailure> l = proto::Unexpected{JobFailure::Provider};
  if (result.error == proto::ErrorClass::None && run.lived_input) {
    l = validate_lived(result.text, result.finish_reason, *run.lived_input, check);
  }
  if (l) {
    proto::LivedVersion v = lived_version(*l, store_.journey(), persona);
    v.model_fp = result.model.empty() ? std::string("-") : result.model;
    v.prompt_hash = "lived-v1";
    v.persona_hash = "-";
    store_.commit_lived(proto::db::CommitLived{std::move(v), std::nullopt});
    lived_due_.erase(persona);
    lived_attempts_.erase(persona);
    lived_next_[persona] = pt + kLivedEveryMs;
    memory_run_.reset();
    store_.pump(at, out.db);
    return;
  }
  // The slot's 2 retries, then the next quiet moment; parked after 4 attempts until the journal changes again.
  run.failure = l.error();
  if (run.tries < 2) {
    run.retry_at = at + (run.tries == 0 ? 5000 : 30000);
    ++run.tries;
    return;
  }
  if (++lived_attempts_[persona] >= 4) {
    lived_due_.erase(persona);
    lived_attempts_.erase(persona);
  }
  lived_next_[persona] = pt + 600000;
  memory_run_.reset();
}

}  // namespace en::core
