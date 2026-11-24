#pragma once
// en-core's v0.1 slice (the first in-game build, part 5): the lifecycle (ARCH §9, without the store), the gate
// (ARCH §5.1, gate.hpp) and the ladder's first levels, L1 and L2, with Dormant over them (ARCH §6.8). Pure: no I/O, no
// threads, no clocks and no Win32 (ARCH §2.3); the time is the snapshots' own `mono_ms` and the `now` it is given.
//
// The core steps on each input in the order the shell drains them (ARCH §1.3: C2's lifecycle messages, then C1's
// snapshot): `Core::step(CoreInput, Mono)` answers its Outputs, which say what the shell does (the game commands, C15's
// `ai_allowed`) and what the player sees (the banner, the badge), and what changed on this step. Everything the slice
// derives from snapshots can be rebuilt from the next ones, so a lost lifecycle message costs nothing (C2).
//
// What waits for later parts: the store (Playthrough → InWorld waits for `DbResult::Opened` once it exists; here it
// follows at once), barks, memory and the rest of the content.
//
// The first conversation (WHY§364; part B): the surfaces' events (proto::UiEvent, C7), the model client's events
// (proto::LlmEvent, C10) and its health (proto::NetHealth) go in; what the surfaces show (proto::UiModel, C6), the
// requests to send and cancel (C9) and the step's commits come out. The conversation (conversation.hpp) runs on every
// step after Augur, with the personas of the content the shell loaded (load_content); its commits go to the core's
// event log in memory in the same step (event_log.hpp; the store, v0.3, takes its place). The model client's health
// takes the ladder to L3 or L4 (ARCH §6.8), and a conversation needs L3 or above.
//
// Augur (en-augur, ARCH §8; part 11) runs in every step, before anything that consumes the step's events: the slice
// judges the input first (the gate, the phases, the ladder), because Augur's Lifecycle source reads what it changed on
// the same step, and then Augur takes the same input and answers the step's events, which the bus dispatches (none of
// v0.1's subsystems subscribes yet) into Outputs::events. AugurView is what en-core supplies Augur at v0.1.
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "en/augur/augur.hpp"
#include "en/augur/bus.hpp"
#include "en/core/behaviour.hpp"
#include "en/core/content.hpp"
#include "en/core/conversation.hpp"
#include "en/core/event_log.hpp"
#include "en/core/gate.hpp"
#include "en/core/identity.hpp"
#include "en/core/knowledge.hpp"
#include "en/core/predicates.hpp"
#include "en/proto/command.hpp"
#include "en/proto/lifecycle.hpp"
#include "en/proto/llm.hpp"
#include "en/proto/snapshot.hpp"
#include "en/proto/ui.hpp"

namespace en::core {

// The lifecycle's phases (ARCH §9). Dormant is not one of them: it is the gate's orthogonal region (State::dormant).
enum class Phase : std::uint8_t {
  Gate,         // T1 is still starting EldenNet: no DataReady yet
  Banner,       // T1 stopped at a banner (unknown build, anti-cheat, readiness, discovery, registration, first tick)
  Ready,        // the task is registered and ticking, and phase A passed
  Title,        // at the title screen
  Playthrough,  // a playthrough identified (with the store, until its database is open)
  InWorld,      // in a world
  Loading,      // between worlds
};

// The ladder (ARCH §6.8). Off never reaches the core (T1 exits). The values only grow: a level at or above L2 allows
// the model client's calls (C15), and one at or above L3 allows speech.
enum class Level : std::uint8_t {
  Starting,  // neither yet: T1 has neither reached Ready nor stopped
  L1,        // AI off: a banner says why
  L2,        // observing: the task ticks and the structural groups passed phase A
  L3,        // L2, a healthy dialogue model (NetHealth), and an optional group tripped
  L4,        // L2, a healthy dialogue model, and every group up
};

// Why EldenNet stopped at L1 (ARCH §3.2, §5.4, §10.3).
enum class BannerReason : std::uint8_t {
  UnknownBuild,     // the gate refused the build
  AntiCheat,        // an anti-cheat module, at init or later (§5.3)
  NotStarted,       // the game did not finish starting within 120 s (WaitWindow, WaitEngine)
  DiscoveryFailed,  // a required item missing after the last retry
  RegisterFaulted,  // the one register_task call faulted
  Inert,            // the first tick's group check or a phase-A test failed, or no tick in 30 s
  SessionFailed,    // the `session` group failed its self-test or tripped
  TaskTripped,      // the task or a structural group tripped
  SpikeRun,         // a spike run of GraceUI's observer (S0-E, S4): T1 stops at Register and calls nothing
};

[[nodiscard]] const char* to_string(Phase p) noexcept;         // "gate", "banner", "ready", "title", ...
[[nodiscard]] const char* to_string(Level l) noexcept;         // "starting", "L1", "L2", "L3", "L4"
[[nodiscard]] const char* to_string(BannerReason r) noexcept;  // "unknown_build", "anticheat", ...

// What the player reads (ARCH §5.4, §10.3). `build` is the game build as players know it ("2.x.y.z"); `reason` the
// fixed words T1 gives with DiscoveryFailed, RegisterFaulted and Inert. The texts are ours.
[[nodiscard]] std::string banner_text(BannerReason r, std::string_view build, std::string_view reason);
inline constexpr std::string_view kBadgeMultiplayer = "EldenNet paused (multiplayer)";

// A banner from T1, before or instead of DataReady (ARCH §1.4: "Banner(reason) → L1").
struct BootBanner {
  BannerReason reason = BannerReason::UnknownBuild;
  std::string detail;  // fixed words for the reason's %s, never text from the game

  friend bool operator==(const BootBanner&, const BootBanner&) = default;
};

// A playthrough's identity: what PlaythroughReady carries that tells one character from another (the key of DESIGN
// §10.5 is made from these, with the SteamID, by the store). The name stays in the core; nothing here writes it.
struct PlaythroughId {
  std::uint8_t slot = 0;
  proto::PlayerName name;
  std::uint8_t archetype = 0;
  std::uint8_t keepsake = 0;

  friend constexpr bool operator==(const PlaythroughId&, const PlaythroughId&) = default;
};

struct CoreInput {
  struct Tick {};  // C16: the 100 ms tick, for time that passes with no snapshot
  // Pause AI for this session (D15; PAGES §4.6.1): on, the ladder stands at L2 with the reason "AI paused" and no model
  // request goes out (ARCH §1.5, §6.8); off, it climbs back to what the rest of the state gives. Never stored: every
  // core starts with it off.
  struct Pause {
    bool on = false;
  };
  // C1's snapshot, C2's lifecycle message, T1's banner, the tick; then the surfaces' event (C7), the model client's
  // event (C10) and the model client's health, which the shell passes when it changes; and the player's Pause AI.
  std::variant<std::unique_ptr<proto::WorldSnapshot>, proto::Lifecycle, BootBanner, Tick, proto::UiEvent,
               proto::LlmEvent, proto::NetHealth, Pause>
      payload;
};

// Where the slice stands after a step: what the status block and the debug view show.
struct State {
  Phase phase = Phase::Gate;
  Level level = Level::Starting;
  bool gate_open = false;
  Unclean gate_why = Unclean::Loading;  // the last snapshot's verdict
  bool dormant = true;                  // speech off: the gate is closed, or not in a world (§6.8)
  bool badge = false;                   // "EldenNet paused (multiplayer)" is up
  bool ai_paused = false;               // Pause AI is on (D15): L2 at most, for the reason "AI paused"
  std::optional<BannerReason> banner;   // the L1 banner, once shown (it stays)
  std::uint64_t snapshots = 0;          // processed
  std::uint64_t playthroughs = 0;       // entered (a new identity each)

  friend constexpr bool operator==(const State&, const State&) = default;
};

// One change on a step, for the log and the recording: a phase left for another, the gate opening or closing, the
// level changing.
struct Change {
  enum class What : std::uint8_t { Phase, GateOpened, GateClosed, Level, Banner, Badge };
  What what = What::Phase;
  std::uint8_t from = 0;  // the old Phase or Level; for GateClosed the Unclean reason
  std::uint8_t to = 0;    // the new Phase or Level; for Banner the BannerReason; for Badge 1 up, 0 down
  Mono at = 0;            // the snapshot's mono_ms, or the step's now

  friend constexpr bool operator==(const Change&, const Change&) = default;
};

[[nodiscard]] const char* to_string(Change::What w) noexcept;

struct Outputs {
  std::vector<proto::GameCommand> game;  // C4: SetTracked{} when the gate closes and when a world is left (§5.2, §9)
  bool ai_allowed = false;               // C15: network calls allowed (the gate open, and L2 or above)
  State state;
  std::vector<Change> changes;
  std::vector<augur::Event> events;  // Augur's events of the step, in dispatch order (ARCH §8.9)
  proto::UiModel ui;                 // C6: what the surfaces show after this step, whole (the newest wins)
  // C9: to submit, in order (Service::submit); none while !ai_allowed (C4) or while AI is paused (D15).
  std::vector<proto::LlmRequest> requests;
  std::vector<CallNote> calls;              // the conversation's notes of `requests`, for the calls' record (calls.hpp)
  std::vector<proto::RequestId> cancels;    // C9: live requests to cancel (Service::cancel)
  // The step's commits, already applied to the core's event log (log()): chat text, so never for a log file, a
  // recording or a numbers-only report (PV rules).
  std::vector<Commit> commits;
  std::optional<EndReason> ended;  // a conversation ended on this step
};

// What en-core supplies Augur (ARCH §8.1), for what v0.1 knows. There are no personas and no cards until content
// exists (v0.2, v0.4): every character classifies as Mute, and the Tarnished as Player. The predicates are v0.1's
// (predicates.hpp); no persona perceives yet; the seed root is 0 until the store's playthrough key exists.
class AugurView final : public augur::AugurCtx {
 public:
  explicit AugurView(const Predicates& predicates) : predicates_(predicates) {}
  [[nodiscard]] augur::Subject classify(const proto::Chr& chr) const override;
  [[nodiscard]] bool eval(augur::PredicateId pred, const augur::EvalScope& at) const override;
  [[nodiscard]] bool persona_may_perceive(augur::PersonaId id) const override;
  // splitmix64's finaliser over the seed root, the event and the salt: deterministic, and the same in every build.
  // BLAKE3 (DESIGN's blake3(seed_root ‖ ev ‖ salt)) replaces it with the seed root; no v0.1 rule draws from it.
  [[nodiscard]] std::uint64_t rng(augur::EventId ev, std::uint32_t salt) const override;

  std::optional<proto::ChrHandle> player;  // the Tarnished's handle at the last snapshot
  PredicateFacts facts;
  std::uint64_t seed_root = 0;

 private:
  const Predicates& predicates_;
};

// The rule files built into this build (content/eldenring/augur/*.toml, read at configure time): v0.1 has no content
// loader, so the shipped set is part of the binary.
struct BuiltinFile {
  std::string_view name;
  std::string_view text;
};
[[nodiscard]] std::span<const BuiltinFile> builtin_rule_files() noexcept;
// The shipped persona index and records (content/eldenring/identity/ and personas/), by their paths in the pack: the
// shell gives them to load_content as the shipped files, with the overrides folder's after them.
[[nodiscard]] std::span<const BuiltinFile> builtin_content_files() noexcept;
[[nodiscard]] std::vector<ContentFile> builtin_content();
// What the game's values mean, as Augur reads them (teams.toml's hostile set, DESIGN §5.3; the victory banners, §6.5;
// the throw states of a critical hit, §6.5; the open world's areas, ARCH §8.8). teams.toml joins with content.
[[nodiscard]] augur::WorldFacts world_facts();

// The speech parts the core uses when it is given none: part A's sanitizer, lint, pager and prompt, the lint and the
// prompt over the loaded content's lexicon and templates (speech_content.hpp). Defined in core.cpp.
struct BuiltinSpeech;

class Core {
 public:
  // Loads the built-in rule set; a set that fails to load (a defect: en_core_tests loads it) leaves Augur with none,
  // and rule_errors() says why. Speech is the built-in parts.
  Core();
  // The same, with the caller's speech parts, which outlive the core (the tests and the eval).
  explicit Core(SpeechParts speech);
  ~Core();
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  // One input. `now` is the core's mono time; a snapshot is judged by its own mono_ms.
  [[nodiscard]] Outputs step(CoreInput input, Mono now);

  [[nodiscard]] const State& state() const noexcept { return state_; }
  [[nodiscard]] const Gate& gate() const noexcept { return gate_; }
  [[nodiscard]] proto::Capabilities caps() const noexcept { return caps_; }
  [[nodiscard]] const std::optional<PlaythroughId>& playthrough() const noexcept { return current_; }
  // The fixed words T1 gave with its banner (for banner_text's reason), or empty.
  [[nodiscard]] const std::string& banner_detail() const noexcept { return banner_detail_; }
  [[nodiscard]] const augur::Augur& augur() const noexcept { return augur_; }
  [[nodiscard]] const augur::Bus& bus() const noexcept { return bus_; }
  [[nodiscard]] const std::vector<augur::RuleError>& rule_errors() const noexcept { return rule_errors_; }
  // Replaces the rule set (tests and tools); on any error the old set is kept and the errors are answered.
  std::vector<augur::RuleError> load_rules(std::span<const augur::RuleFile> files);
  // Loads the content the shell read (content.hpp), for the realm (identity.hpp: the DLL's is Game), and answers its
  // errors; on any error the old content is kept. The pack's prompts and lexicon load with it (speech_content.hpp), and
  // a pack whose prompts or lexicon fail is refused; the built-in speech's lint and prompt then read the new ones.
  // Before any load the core knows no persona, so no conversation opens.
  std::vector<ContentError> load_content(std::span<const ContentFile> files, Realm realm);
  // The last load's warnings: disable.toml's ids that name nothing (S11.3.g), then the player's own records whose block 2
  // passes its cap (WHY§388), which still render.
  [[nodiscard]] const std::vector<ContentError>& content_warnings() const noexcept { return content_warnings_; }

  [[nodiscard]] const Directory& directory() const noexcept { return directory_; }
  // The last load's findings about the player's layers (profile.hpp): each layer's, a refused one's errors among them,
  // for the profile editor's states (S7.2.b).
  [[nodiscard]] const std::vector<LayerFinding>& layer_findings() const noexcept { return layer_findings_; }
  [[nodiscard]] const MemoryLog& log() const noexcept { return log_; }
  [[nodiscard]] const Conversation& conversation() const noexcept { return conversation_; }
  [[nodiscard]] proto::NetHealth net() const noexcept { return net_; }

  // Quest and world awareness (knowledge.hpp; S11.3.c). A load builds the knowledge with the content (its errors refuse
  // the pack) and a watch list of the catalogue's verified, resolved aliases for the load's realm; the next step sends
  // it as SetWatchFlags when it is not empty (in game none is verified before S6, so nothing is sent). A snapshot's
  // FlagFrame of that list's version gives the flag state, and a change of it the view: each persona's active stage and
  // block 5b's lines, which the conversation reads. Until a playthrough's first frame every alias is Unknown, so
  // nothing holds and block 5b has the road alone; a new playthrough or the title forgets the frame.
  [[nodiscard]] const Knowledge& knowledge() const noexcept { return knowledge_; }
  [[nodiscard]] const WatchList& watch_list() const noexcept { return watch_; }
  [[nodiscard]] const FlagState& flags() const noexcept { return flags_; }
  [[nodiscard]] const KnowledgeView& known_view() const noexcept { return known_; }
  // The player's behaviour settings (behaviour.hpp): from the next step; one out of bounds is refused (the old stays),
  // and its key answered.
  std::optional<std::string> set_behaviour(const Behaviour& b);
  [[nodiscard]] const Behaviour& behaviour() const noexcept { return behaviour_; }

 private:
  void on_flags(const proto::WorldSnapshot& s);
  void forget_flags();
  void on_snapshot(const proto::WorldSnapshot& s, Outputs& out);
  void on_lifecycle(const proto::Lifecycle& l, Mono now, Outputs& out);
  void on_banner(const BootBanner& b, Mono now, Outputs& out);
  void enter(Phase to, Mono at, Outputs& out);
  void set_level(Level to, Mono at, Outputs& out);
  void raise_banner(BannerReason r, Mono at, Outputs& out);
  void track_none(Outputs& out);
  bool ready_for_playthrough(const proto::WorldSnapshot& s) const noexcept;
  void start_playthrough(const proto::WorldSnapshot& s, Outputs& out);
  void settle(Mono at, Outputs& out);
  void run_augur(const proto::WorldSnapshot* s, const proto::Lifecycle* l, Mono now, Outputs& out);
  void climb(Mono at, Outputs& out);
  void run_conversation(Mono at, bool fresh, const proto::UiEvent* ui, const proto::LlmEvent* llm, Outputs& out);
  void init();

  State state_;
  Gate gate_;
  proto::Capabilities caps_;
  std::optional<PlaythroughId> current_;       // the playthrough in progress
  std::optional<PlaythroughId> pending_;       // the newest PlaythroughReady not yet taken up
  std::optional<std::uint64_t> prev_play_ms_;  // the player's play time on the previous snapshot, for "rising"
  bool play_rising_ = false;
  std::optional<std::uint32_t> prev_counter_;  // the load counter on the previous snapshot
  bool prev_in_world_ = false;                 // the previous snapshot showed a world (main player, InGame)
  std::uint8_t stable_ = 0;                    // consecutive snapshots in a world with the same counter (Loading)
  std::string banner_detail_;
  bool paused_multiplayer_ = false;  // the gate closed in a world for a session reason
  bool tracked_none_sent_ = false;   // one SetTracked{} per step
  std::uint16_t groups_tripped_ = 0;  // the groups whose GroupTripped Augur has had, one bit each (once per session)

  Predicates predicates_;
  AugurView view_{predicates_};
  augur::Augur augur_;
  augur::Bus bus_;
  std::vector<augur::RuleError> rule_errors_;

  // The first conversation.
  std::unique_ptr<BuiltinSpeech> builtin_;  // when the core was given no speech parts
  Conversation conversation_;
  Directory directory_;
  bool content_loaded_ = false;
  std::vector<LayerFinding> layer_findings_;
  std::vector<ContentError> content_warnings_;
  MemoryLog log_;
  proto::NetHealth net_;
  bool optional_tripped_ = false;   // an optional group tripped: L3 at most (ARCH §6.8)
  std::unique_ptr<proto::WorldSnapshot> last_;  // the newest snapshot, for the conversation
  std::vector<std::uint64_t> engaged_;          // the opponents the Tarnished is engaged with (Augur's events)
  std::vector<std::uint8_t> hostile_teams_;

  // Quest and world awareness, and the behaviour settings.
  Knowledge knowledge_;
  Realm realm_ = Realm::Game;
  WatchList watch_;
  std::uint32_t watch_version_ = 0;
  bool watch_pending_ = false;  // the list is to go out with the next step
  FlagState flags_;
  KnowledgeView known_;
  Behaviour behaviour_;
};

}  // namespace en::core
