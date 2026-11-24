#pragma once
// The model calls' record and Pause AI (PAGES §4.6.1's Model calls card and Pause AI, §4.6.2's AI › Requests; D15,
// D18; roadmap S11.4.a): one row per call EldenNet made this session and the session's usage, as en-core builds them
// (calls.hpp there) and the DLL keeps them on T6, in plain data that en-overlay's space publishes (it links no en-core,
// ARCH §2.3); and the words the pages show for them and for a call Pause AI refused. Every word a player reads here is
// a draft for the owner's review.
//
// PV4: chat and model text only as it was shown. A row holds the displayed text of a reply whose pages reached the
// screen, and nothing else of the model's or the player's text: never a prompt, an unshown reply, a key, a base URL or
// a server's message (a call's failure is its error class and HTTP status alone). Ids are the configuration's:
// providers.toml's provider ids and the models it names, a model id that looks like a path left out (PV5, as AI ›
// Models' pickers leave one out).
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/llm.hpp"

namespace en::proto {

inline constexpr std::size_t kCallsKept = 20;            // the newest rows kept (PAGES: `calls`, 20 kept)
inline constexpr std::size_t kCallsKeptDeveloper = 200;  // with developer mode (debug.panel)
inline constexpr std::size_t kCallTextMaxBytes = 2048;   // a row's displayed text, cut at a character's start
inline constexpr std::uint64_t kCallHourMs = 3'600'000;  // "this hour": the last 60 minutes, as the bound counts

// Why EldenNet made the call. PAGES' words for the first six; v0.2 makes replies, the self-test, model lists and
// profile drafts, and the rest arrive with their features.
enum class CallPurpose : std::uint8_t {
  Reply,          // "reply": the Tarnished's line answered
  OwnInitiative,  // "own-initiative line": an NPC's own line
  CallOut,        // "call-out reply"
  PreparedBatch,  // "prepared-line batch": lines for a pool
  Memory,         // "memory work"
  SelfTest,       // "self-test": its two requests and the model lists it asks for
  ModelList,      // "model list": a provider's list, asked from AI › Providers (ours)
  ProfileDraft,   // "profile draft": the profile editor's draft helper (ours)
};
inline constexpr std::size_t kCallPurposes = 8;

// How the call ended. PAGES' words for shown to pool filled; "waiting" and "answered" are ours. A row has one outcome:
// a page shown comes first (so a reply that broke after a page reached the screen reads shown, its error class kept),
// then a failure, then the rest. "Not shown" covers any answer that never reached the screen: silence, an answer
// withheld by a check (the lint's deflection), one dropped unshown when its conversation closed, or a call stopped
// before it finished.
enum class CallOutcome : std::uint8_t {
  Waiting,        // "waiting": sent, and not ended yet (ours)
  Shown,          // "shown": a page of its answer reached the screen
  NotShown,       // "not shown"
  FallbackModel,  // "fallback model": shown, and answered by another model than the one asked first (the slot's
                  // fallback, after a failure)
  TimedOut,       // "timed out": no first words, or no end, in the slot's time
  Refused,        // "refused": the model declined (a content filter, a refusal)
  Failed,         // "failed": any other failure, a call not sent among them
  PoolFilled,     // "pool filled": a prepared-line batch's lines went into its pool
  Answered,       // "answered": answered where nothing is for the screen (the self-test, a model list, a draft; ours)
};
inline constexpr std::size_t kCallOutcomes = 9;

// One call. A reply is one row whatever it took: en-llm's retries, its fallback and a backup (one request), and the
// conversation's one regeneration after a lint hit (a second request), with their attempts and tokens summed.
struct CallRow {
  std::uint64_t id = 0;            // the row's own, unique in the session; a later row's is higher
  std::uint64_t at_ms = 0;         // when it was sent, since T1's start (a sort key)
  std::string slot;                // the request's slot (en-proto's names); empty for a model list
  CallPurpose purpose = CallPurpose::Reply;
  std::string speaker;             // as shown (SP3), or a type label; empty when nobody speaks
  std::string provider;            // the provider that answered, or that failed last (a providers.toml id)
  std::string model;               // its model, as the configuration names it; empty when it looks like a path
  std::int64_t first_token_ms = -1;  // from at_ms to its first text; -1 when none came (a sort key)
  std::int64_t total_ms = -1;        // from at_ms to its end; -1 while it waits (a sort key)
  std::int32_t tokens_in = -1;       // prompt tokens as the provider reported them, summed; -1: none reported
  std::int32_t tokens_out = -1;      // completion tokens, likewise
  double cost = -1;                  // as the provider's reply reported it (D18), summed; -1: none reported
  std::uint32_t attempts = 0;        // sent to a provider: the first, retries, the fallback, backups; 0: never sent
  bool fallback = false;             // answered by another model than its first attempt's
  CallOutcome outcome = CallOutcome::Waiting;
  // Why it failed; for a fallback model, why the first model did not answer; Cancelled for one stopped before it
  // finished; None otherwise.
  ErrorClass error = ErrorClass::None;
  int http_status = 0;               // with the error: the last attempt's (or the list's) HTTP status, 0 when none came
  std::string text;                  // the displayed text, once a page was shown (PV4): never anything else

  // In and out where reported, as the table's sort key; -1 when neither was.
  [[nodiscard]] std::int64_t tokens() const noexcept {
    if (tokens_in < 0 && tokens_out < 0) return -1;
    return static_cast<std::int64_t>(tokens_in < 0 ? 0 : tokens_in) + (tokens_out < 0 ? 0 : tokens_out);
  }

  friend bool operator==(const CallRow&, const CallRow&) = default;
};

// The session's usage (PAGES' Requests tiles): every call this session, not only the rows kept.
struct CallUsage {
  std::uint64_t calls = 0;
  std::uint64_t tokens_in = 0;    // reported prompt tokens, summed
  std::uint64_t tokens_out = 0;   // reported completion tokens, summed
  std::uint64_t failures = 0;     // calls whose outcome is timed out, refused or failed
  std::uint32_t own_initiative_hour = 0;  // own-initiative calls sent in the last hour
  std::uint32_t requests_hour = 0;  // every request the model client sent in the last hour, retries included (en-llm)
  std::uint32_t bound = 0;          // budget.requests_per_hour, the bound those count against; 0 before a configuration
  double cost = 0;                  // the costs reported, summed (D18: shown only when cost_calls is not 0)
  std::uint64_t cost_calls = 0;     // the calls whose reply reported a cost

  friend bool operator==(const CallUsage&, const CallUsage&) = default;
};

struct CallsView {
  std::uint64_t revision = 0;      // changes whenever anything below does
  std::size_t kept = kCallsKept;   // kCallsKept, or kCallsKeptDeveloper with debug.panel
  std::vector<CallRow> rows;       // newest first (by at_ms, then id), at most `kept`
  CallUsage usage;

  friend bool operator==(const CallsView&, const CallsView&) = default;
};

// ---- The words the pages show (drafts for the owner's review) ----

[[nodiscard]] const char* words(CallPurpose p) noexcept;  // "reply", "own-initiative line", ..., "profile draft"
[[nodiscard]] const char* words(CallOutcome o) noexcept;  // "waiting", "shown", "not shown", ..., "answered"
// The error class in words, a sentence of ours, as the row drawer says why (an HTTP status tells a server that could
// not be reached, 0, from one that failed); empty for None.
[[nodiscard]] const char* error_words(ErrorClass e, int http_status) noexcept;
// Names for logs and tests: "reply", "own_initiative", ...; "waiting", "shown", "not_shown", ...
[[nodiscard]] const char* to_string(CallPurpose p) noexcept;
[[nodiscard]] const char* to_string(CallOutcome o) noexcept;

// Whether a model id the configuration names may be shown on a row (PV5): 1 to 200 bytes, printable, and nothing that
// looks like a path (a backslash, a leading '/' or '~', a drive's letter, "/Users/" or "/home/"), since a local
// server's ids can be its model files' paths, which hold the Windows user name.
[[nodiscard]] bool shown_model(std::string_view id) noexcept;

// ---- Pause AI for this session (D15; the space's `ai.pause` with {"on": bool}, its state `status.ai_paused`) ----
//
// A session flag, off at every start and never stored: while it is on, the ladder stands at L2 with this reason
// (ARCH §1.5, §6.8), and every model call EldenNet would start is refused before it leaves.
inline constexpr std::string_view kAiPausedReason = "AI paused";
inline constexpr std::string_view kAiPausedStatus = "AI is paused for this session.";
inline constexpr std::string_view kAiPausedSelfTest =
    "AI is paused for this session, so the self-test sends nothing. Turn Pause AI off to run it.";
inline constexpr std::string_view kAiPausedModelList =
    "AI is paused for this session, so no provider is asked for its models. Turn Pause AI off to list them.";
inline constexpr std::string_view kAiPausedDraft = "Drafts wait while AI is paused for this session.";
inline constexpr std::string_view kAiPausedCall = "Not sent: AI is paused for this session.";

}  // namespace en::proto
