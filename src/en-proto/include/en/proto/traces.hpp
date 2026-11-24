#pragma once
// System › Traces (PAGES §4.6.7; DESIGN §1.3's C5, §1.4; roadmap S11.4.a): how long each line took, from its request
// to the screen, as en-core's calls' record builds it (calls.hpp there) from the same events as AI › Requests' rows,
// in plain data that en-overlay's space publishes (the topic `traces`); the verdict's rule and C5's targets; and the
// words the page shows for a line's kind and its verdict. Every word a player reads here is a draft for the owner's
// review.
//
// A line is a call whose answer is meant for the screen: a reply, an own-initiative line or a call-out reply (v0.2
// makes replies alone; the others arrive with their features). Its times are its row's in the calls' record, each from
// the moment its request was handed to the model client, which for a reply is the core's step that took the
// Tarnished's line: the queue (until its first attempt left), the first token, the first page shown (its first page's
// PageShown, whatever the page said: the persona's own line in place of an answer a check withheld counts too) and the
// total (to the request's end). A trace holds no text at all: numbers, its kind and its job.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "en/proto/calls.hpp"

namespace en::proto {

enum class LineKind : std::uint8_t { Reply, OwnInitiative, CallOut };
inline constexpr std::size_t kLineKinds = 3;
enum class LineVerdict : std::uint8_t { None, Quick, Acceptable, Slow };

// The verdict's rule (ARCH §11.7, NPC §7.7, DESIGN §11.5: one rule, C5's cloud targets): a first page within 1.6 s is
// quick, within 3.0 s acceptable, and slow beyond. A call-out reply keeps DESIGN §1.4's own, 1.5 s and 2.5 s. An
// own-initiative line has none: it is prepared before it is wanted and shown once the Tarnished comes near (§1.4), so
// its first page measures that wait as much as the model's.
inline constexpr std::int64_t kQuickPageMs = 1600;
inline constexpr std::int64_t kAcceptablePageMs = 3000;
inline constexpr std::int64_t kQuickCallOutMs = 1500;
inline constexpr std::int64_t kAcceptableCallOutMs = 2500;

// C5's targets for the session's first pages, by where the Conversation job's model runs: on a cloud provider, the
// median within 1.6 s and the second figure within 3.0 s; on a local one (C5's 12 GB local preset), 2.5 s and DESIGN
// §1.4's 4.5 s. C5 and §1.4 give the second target for the 90th percentile; PAGES' tile is the 95th, which is held to
// it (the stricter reading; the notes ask which is meant).
struct PageTargets {
  std::int64_t median_ms = 0;
  std::int64_t p95_ms = 0;
};
inline constexpr PageTargets kCloudTargets{1600, 3000};
inline constexpr PageTargets kLocalTargets{2500, 4500};

// The tiles count from the session's newest first pages and first tokens, this many of each (a session's lines are far
// fewer); the 95th percentile is given only from kTraceP95Lines first pages on, below which it would be the slowest.
inline constexpr std::size_t kTraceSamples = 4096;
inline constexpr std::size_t kTraceP95Lines = 20;

// One line's call. Every time is from at_ms; -1 while it is not known, or never came.
struct TraceRow {
  std::uint64_t id = 0;     // the call's row id in the calls' record (calls.hpp)
  std::uint64_t at_ms = 0;  // when its request was handed over, since T1's start
  LineKind kind = LineKind::Reply;
  std::string slot;                  // the request's slot (en-proto's names)
  std::int64_t queue_ms = -1;        // until its first attempt left; -1 until its end, or when none left
  std::int64_t first_token_ms = -1;  // to its first text
  std::int64_t first_page_ms = -1;   // to its first page's showing
  std::int64_t total_ms = -1;        // to its end
  LineVerdict verdict = LineVerdict::None;  // by its first page and its kind, once one showed

  friend bool operator==(const TraceRow&, const TraceRow&) = default;
};

// The session's lines (PAGES' Traces tiles): every line's call this session, not only the rows kept.
struct TraceSummary {
  std::uint64_t calls = 0;    // the session's calls for a line
  std::uint64_t lines = 0;    // of them, those whose first page reached the screen
  std::uint64_t unshown = 0;  // those that ended with no page shown (a page after the end moves one to `lines`)
  std::int64_t first_page_median_ms = -1;   // of the newest kTraceSamples first pages; -1 with none
  std::int64_t first_page_p95_ms = -1;      // likewise, from kTraceP95Lines first pages on; -1 below
  std::int64_t first_token_median_ms = -1;  // of the newest kTraceSamples first tokens of lines; -1 with none

  friend bool operator==(const TraceSummary&, const TraceSummary&) = default;
};

struct TracesView {
  std::uint64_t revision = 0;     // changes whenever anything below does
  std::size_t kept = kCallsKept;  // as the calls' record keeps: kCallsKept, or kCallsKeptDeveloper with debug.panel
  std::vector<TraceRow> rows;     // newest first (by at_ms, then id), at most `kept`
  TraceSummary summary;

  friend bool operator==(const TracesView&, const TracesView&) = default;
};

// The kind of line a call is for, or none (the self-test, a model list, a draft, a batch, memory work).
[[nodiscard]] std::optional<LineKind> line_of(CallPurpose p) noexcept;
// A line's verdict by its first page (-1: none showed, so none).
[[nodiscard]] LineVerdict verdict_of(LineKind kind, std::int64_t first_page_ms) noexcept;
// The nearest-rank percentile of `samples` (`permille`: 500 for the median, 950 for the 95th): the value at rank
// ceil(n × permille ÷ 1000), at least the first; -1 with none.
[[nodiscard]] std::int64_t percentile(std::vector<std::uint32_t> samples, std::uint32_t permille);

// ---- The words the page shows (drafts for the owner's review) ----

[[nodiscard]] const char* words(LineKind k) noexcept;     // PAGES': "reply", "own-initiative", "call-out"
[[nodiscard]] const char* words(LineVerdict v) noexcept;  // PAGES': "quick", "acceptable", "slow"; "" for None
// Names for logs and tests: "reply", "own_initiative", "call_out"; "none", "quick", "acceptable", "slow".
[[nodiscard]] const char* to_string(LineKind k) noexcept;
[[nodiscard]] const char* to_string(LineVerdict v) noexcept;

}  // namespace en::proto
