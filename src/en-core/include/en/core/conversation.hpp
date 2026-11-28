#pragma once
// The conversation, part B's (NPC §5, §6; DESIGN §8): the state machine of NPC §6.1 and its timers (§6.2), as a pure
// object the core steps with the time, what it knows of the world, and the step's surface or model event, answering
// what the surfaces show (proto::UiModel), the requests to send or cancel, and the commits for the event log.
//
// - **Opening** (§5.1, §5.2): the talk key's target is the lock-on target, else the best in a 25° cone within 8 m,
//   else the nearest within 5 m (8 m sheltered), among the characters the identity resolves to personas; the hint
//   changes target only after the new best has led for 500 ms. The box opens when the target is conversable (§5.3),
//   the context is safe (§5.4: L3 or above, a world, no danger) and the native-talk observer, which fails closed until
//   S3 (DESIGN §8.7, §5.8), lets it: the HUD at Default, 3 s free of the pop-up and the talk camera, and 3 s since the
//   persona came within reach. A refusal gives its toast, or nothing (§5.1).
// - **A line** (§6.1, §6.3): Submit commits PlayerSaid and sends one request, built by the prompt (prompt.hpp) and
//   read back through the sanitizer, the lint and the pager (speech.hpp). Only one request is in flight. In Awaiting
//   or Held, nothing of the reply shown yet, a line waits in the one queued slot (a later one replaces it) and goes
//   when the reply's last page starts showing; in Streaming or Paging it cuts the reply off: the page up stays until it
//   has shown 2 s or its hold ends, the stream is cancelled, unshown pages are dropped, and the NpcSaid is marked cut
//   off. A failed request (en-llm has tried the fallback model already) or 20 s with nothing shown gives the "…" page,
//   never committed, and a toast (DESIGN §11.1, §11.2); a lint hit regenerates once, then deflects (§8.4).
// - **Commit on display** (DESIGN §8.5, NPC §8.7): a reply's first PageShown makes its NpcSaid and each later one grows
//   it; a page never acknowledged is never committed; acks of the last reply are taken 500 ms after an end.
// - **Holding** (§5.8): native talk within 20 m (the partner more than 3 m away), or the overlay hidden, holds the
//   unshown pages and pauses their timers; they resume 1.5 s after the native talk or when the overlay shows again,
//   and are dropped after 10 s (native) or 30 s (hidden).
// - **Danger** (§5.4): the box closes at once, its draft kept 60 s for the target, "The moment has passed."; pages
//   shown keep pacing; 10 s of it ends the conversation. **Endings** (§5.7) as EndReason says, each committed.
// - **Toasts** (§10.5): toasts.hpp's policy.
//
// Pure (ARCH §2.3): no I/O, no clock, no threads; time is the step's `now`. Request ids are the conversation's own,
// from `kConversationRequests` up (core.hpp's ranges), unique among live requests.
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/event_log.hpp"
#include "en/core/gate.hpp"  // Mono
#include "en/core/identity.hpp"
#include "en/core/store_path.hpp"
#include "en/core/toasts.hpp"
#include "en/core/knowledge.hpp"
#include "en/core/predicates.hpp"
#include "en/core/prompt.hpp"
#include "en/core/speech.hpp"
#include "en/proto/llm.hpp"
#include "en/proto/snapshot.hpp"
#include "en/proto/ui.hpp"

namespace en::core {

// Speech and the prompt, part A's (or stubs.hpp's in tests), held by reference: the core owns them.
struct SpeechParts {
  const Sanitizer& sanitizer;
  const Lint& lint;
  const Pager& pager;
  const PromptRenderer& prompt;
};

// The conversation's request ids: its own range of LlmRequest ids (barks and memory get theirs as they come).
inline constexpr proto::RequestId kConversationRequests = proto::RequestId{1} << 48;

// The timers and distances of NPC §5 and §6.2.
struct ConversationTimes {
  static constexpr Mono kCardMs = 350;             // the speaker card, in Awaiting with nothing shown
  static constexpr Mono kTotalMs = 20000;          // from Submit, shared with the regeneration
  static constexpr Mono kFailureMs = 2400;         // the "…" page: the hold floor, plus 400 after "…"
  static constexpr Mono kCutOffMs = 2000;          // the page up when a line cuts the reply off
  static constexpr Mono kSkipMinMs = 300;          // Tab ends a hold no sooner (§8.6)
  static constexpr Mono kGraceMs = 500;            // acks after an end
  static constexpr Mono kWalkAwayMs = 2000;
  static constexpr Mono kDangerMs = 10000;
  static constexpr Mono kNativeResumeMs = 1500;
  static constexpr Mono kNativeDropMs = 10000;
  static constexpr Mono kHiddenDropMs = 30000;
  static constexpr Mono kIdleMs = 120000;
  static constexpr Mono kDraftMs = 60000;
  static constexpr Mono kQuietBarksMs = 3000;
  static constexpr Mono kHintLeadMs = 500;
  static constexpr Mono kObserverMs = 3000;        // free of native talk, and since the persona came within reach
  static constexpr Mono kUnresolvedMs = 2000;
  static constexpr Mono kAttackedMs = 30000;
  static constexpr Mono kUnackedTailMs = 500;      // a page up when cut off, not yet acknowledged, waits this long
  static constexpr float kReachM = 8.0f;           // the cone's reach, the persona's approach
  static constexpr float kNearestM = 5.0f;         // rule 5 (8 m sheltered)
  static constexpr float kConeDeg = 25.0f;
  static constexpr float kWalkAwayM = 12.0f;
  static constexpr float kNativePartnerM = 3.0f;   // native talk this near the partner is with the partner (before S3)
};

// Why a persona may not talk now (NPC §5.3), the first check that fails, in this order.
enum class NotConversable : std::uint8_t { Dead, Asleep, Muted, Stage, Hostile, RecentlyAttacked, NativeTalk, Busy, Unavailable };
// Why the context is not safe (NPC §5.4), the first check that fails, in this order.
enum class Unsafe : std::uint8_t { NotActive, Player, Boss, Combat, Hostiles, Menu };

[[nodiscard]] const char* to_string(NotConversable v) noexcept;  // "dead", "asleep", ...
[[nodiscard]] const char* to_string(Unsafe v) noexcept;          // "not_active", "player", ...

// What the gates read, worked out from the newest snapshot and the core's state.
struct Surroundings {
  bool active = false;       // L3 or above, in a world, not dormant (§6.8)
  bool player_dead = false;
  bool riding = false;
  bool boss = false;         // a boss fight
  bool combat = false;       // the Tarnished engaged (Augur's Engaged and Disengaged)
  bool sheltered = false;    // the Roundtable (the rest flag waits for the catalogue): waives the distance checks
  float hostile_m = std::numeric_limits<float>::infinity();          // the nearest live hostile
  float engaged_hostile_m = std::numeric_limits<float>::infinity();  // the nearest that struck or was struck by the Tarnished
  bool caps_combat = true;   // the combat group reads; without it no hostile within 25 m at all
  bool menu = false;         // the HUD not at Default
  bool banner = false;       // a game banner: it stops the box opening, never closes an open one (O108)
  bool native = false;       // the pop-up's talk id, the talk camera, or the HUD at PopupMenu (the observer, before S3)
  bool hidden = false;       // the HUD hidden, or a banner: the overlay's pages are held
  bool caps_conversation = false;  // session, world, player, flags, ui, place and params all read
  bool degraded = false;     // flags, ui or params tripped
  bool camera = false;       // the camera group reads: the cone rule
};

[[nodiscard]] std::optional<Unsafe> safe_context(const Surroundings& w) noexcept;

// One persona's side of §5.3, for a character the identity resolved.
struct PartnerFacts {
  const Persona* persona = nullptr;
  std::size_t stage = 0;
  const proto::Chr* chr = nullptr;
  bool summon = false;                  // the row's form is a summon
  bool hostile = false;                 // its live team is hostile
  bool holds_boss_bar = false;
  std::optional<Mono> attacked_at;      // the Tarnished last struck it
};

[[nodiscard]] std::optional<NotConversable> conversable(const PartnerFacts& p, const Surroundings& w, Mono now) noexcept;

// What the core knows of the world on a step, besides its event.
struct ConversationWorld {
  const proto::WorldSnapshot* snapshot = nullptr;  // the newest snapshot the core holds; null before the first
  bool fresh = false;      // this step's input was that snapshot
  bool in_world = false;   // the lifecycle is InWorld
  bool loading = false;    // the lifecycle is Loading
  bool dormant = true;     // speech off: the gate closed, or not in a world (ARCH §6.8)
  bool speech = false;     // the ladder at L3 or above: a healthy dialogue model, every structural group up
  bool combat = false;     // the Tarnished engaged
  bool player_died = false;  // PlayerDied on this step
  const Directory* cast = nullptr;         // the personas; null before content is loaded
  const Predicates* predicates = nullptr;  // for the stages' `when`
  PredicateFacts facts;
  std::span<const std::uint8_t> hostile_teams;  // teams.toml's hostile set
  const MemoryLog* log = nullptr;          // block 6 (a): the previous conversation's lines
  // Quest and world awareness (knowledge.hpp; S11.3.c, d), each defaulting to what changes nothing: the core's view of
  // the newest flag state, which then gives each persona's active stage and block 5b's lines (else the stage comes from
  // `predicates`, and block 5b holds the road alone); and the player's switches (behaviour.hpp): `chat` off opens no
  // conversation and ends an open one at the next step, `self_awareness` off leaves block 9's "you, now" line out.
  const KnowledgeView* knowledge = nullptr;
  bool chat = true;
  bool self_awareness = true;
  // Named chat held (MEM §9.1: the rollback choice waits): the talk key opens nothing and toasts these words.
  std::string_view chat_held;
  // Memory (v0.3; MEM §4): the store path while its file is open, whose facts and bundles give block 8 and block 6 (a);
  // null keeps block 8 empty and takes block 6 (a) from `log`.
  const StorePath* store = nullptr;
};

struct ConversationInput {
  Mono now = 0;
  ConversationWorld world;
  const proto::UiEvent* ui = nullptr;    // the step's surface event, if that is what it is
  const proto::LlmEvent* llm = nullptr;  // the step's model event, if that is what it is
};

// What the model calls' record (calls.hpp) needs of the conversation's requests, which their ChatRequest does not say:
// the reply each is for and its speaker as the Tarnished sees them (the speaker card's name, SP3); and a reply whose
// answer a check withheld (the lint's deflection: the persona's own line shows in its place, so the model's did not).
struct CallNote {
  enum class What : std::uint8_t { Sent, Withheld };
  What what = What::Sent;
  proto::RequestId request = 0;  // Sent: the request
  proto::ConvId conv = 0;
  proto::ReplyId reply = 0;
  std::string slot;              // Sent: the request's slot
  std::string speaker;           // Sent: as shown

  friend bool operator==(const CallNote&, const CallNote&) = default;
};

struct ConversationOutput {
  proto::UiModel ui;                        // what the surfaces show after the step
  std::vector<proto::LlmRequest> requests;  // to submit, in order (C9)
  std::vector<CallNote> calls;              // for the model calls' record, in order
  std::vector<proto::RequestId> cancels;    // to cancel (a cut-off, a regeneration, an ending)
  std::vector<Commit> commits;              // for the event log, in order, in this step
  std::optional<EndReason> ended;           // the conversation ended on this step
};

class Conversation {
 public:
  explicit Conversation(SpeechParts parts);
  ~Conversation();
  Conversation(const Conversation&) = delete;
  Conversation& operator=(const Conversation&) = delete;

  ConversationOutput step(const ConversationInput& in);
  // A toast of the core's own (the store's, store_path.hpp), under the same policy; shown from the next step.
  void notice(ToastClass cls, proto::ToastLevel level, std::string text, Mono now);

  [[nodiscard]] const SpeechParts& parts() const noexcept { return parts_; }
  // For tests and the debug view.
  [[nodiscard]] proto::ConvState state() const noexcept;
  [[nodiscard]] bool box_open() const noexcept;
  [[nodiscard]] std::optional<std::string> partner() const;     // the partner's persona id
  [[nodiscard]] std::optional<std::string> queued() const;      // the line waiting in the queued slot
  [[nodiscard]] std::optional<Mono> barks_quiet_until() const noexcept;
  [[nodiscard]] std::uint64_t epoch() const noexcept;           // the input epoch: a box opening, an end
  [[nodiscard]] std::optional<proto::RequestId> live_request() const noexcept;

  struct Impl;

 private:
  SpeechParts parts_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace en::core
