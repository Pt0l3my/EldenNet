#pragma once
// The conversation, part B's (NPC §5, §6; DESIGN §8): the state machine of NPC §6.1 and its timers (§6.2), as a pure
// object the core steps with the time, what it knows of the world, and the step's surface or model event, answering
// what the surfaces show (proto::UiModel) and the requests to send or cancel. It uses speech and the prompt (part A's,
// speech.hpp and prompt.hpp) through references, so its tests run on the stubs (stubs.hpp) until A's parts land.
//
// What B fills in: the states and timers, the target order and the gates (NPC §5.2 to §5.4, §5.8), queue-don't-cancel
// (§6.3), the endings (§5.7), the draft kept per target, toasts (§10.5), and commit on display (DESIGN §8.5, NPC §8.7:
// PageShown commits, nothing unshown is ever committed), with the step's commits for the event log added to the
// output. Part 0 gives the shape and a stub that never opens: every step answers a closed box and no request. B may
// reshape this file freely; the core (core.cpp, B's too) is its only user.
//
// Pure (ARCH §2.3): no I/O, no clock, no threads; time is the step's `now`. Request ids are the conversation's own,
// unique among live requests (llm.hpp's LlmRequest), from a space B chooses, since barks and memory will submit too.
#include <cstdint>
#include <optional>
#include <vector>

#include "en/core/gate.hpp"  // Mono
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

// Why a conversation ended (NPC §5.7).
enum class EndReason : std::uint8_t {
  Closed,      // Esc
  WalkedAway,  // more than 12 m for 2 s
  Danger,      // danger for 10 s
  Travel,      // a loading screen
  Death,       // the player died
  Dormant,     // the gate closed, or the world was left
  NpcState,    // the partner dead, hostile, at a muted or refusing stage, or unresolvable for 2 s
  NativeTalk,  // native talk with the partner
  Idle,        // 120 s without a player line
  Retarget,    // the talk key on another persona, or the addressee decision picked another (O97)
  Degraded,    // a required group tripped (flags, ui, params)
  Fault,       // a core or overlay fault
};

// What the core knows of the world on a step, besides its event.
struct ConversationWorld {
  const proto::WorldSnapshot* snapshot = nullptr;  // the newest snapshot the core holds; null before the first
  bool in_world = false;                           // the lifecycle is InWorld
  bool dormant = true;                             // speech off: the gate closed, or not in a world (ARCH §6.8)
  bool speech = false;  // the ladder at L3 or above: a healthy dialogue model (NetHealth), every structural group up
};

struct ConversationInput {
  Mono now = 0;
  ConversationWorld world;
  const proto::UiEvent* ui = nullptr;    // the step's surface event, if that is what it is
  const proto::LlmEvent* llm = nullptr;  // the step's model event, if that is what it is
};

struct ConversationOutput {
  proto::UiModel ui;                        // what the surfaces show after the step
  std::vector<proto::LlmRequest> requests;  // to submit, in order (C9)
  std::vector<proto::RequestId> cancels;    // to cancel (a cut-off, a regeneration, an ending)
  std::optional<EndReason> ended;           // the conversation ended on this step
};

class Conversation {
 public:
  explicit Conversation(SpeechParts parts) : parts_(parts) {}

  // Part 0's stub: no conversation ever opens, so every step answers a closed box, no page and no request.
  ConversationOutput step([[maybe_unused]] const ConversationInput& in) { return {}; }

  [[nodiscard]] const SpeechParts& parts() const noexcept { return parts_; }

 private:
  SpeechParts parts_;
};

}  // namespace en::core
