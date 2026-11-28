#pragma once
// The lived layer (MEM §13; proposal 0004; v0.3 commit 15): one versioned record per journey and profiled NPC of how
// things have turned for it, in its own words and citing the journal's keepsakes behind each part: its feelings
// towards people (`towards`), aims that changed, what it has learned, how it sees itself (`self`) and how it now
// speaks with the Tarnished (`manner`). A lived job updates it at quiet moments when the journal has moved enough
// (MEM §13.2), from what the NPC's own prompt would show it (MEM §13.3), with prompts/lived/ (ours, from MEM §13.4);
// validation holds each part to its tier (MEM §13.5); it renders in block 8 (MEM §13.6). It lives only in the
// playthrough file.
#include <cstdint>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "en/core/content.hpp"
#include "en/core/memory.hpp"
#include "en/core/memory_job.hpp"  // JobFailure
#include "en/proto/expected.hpp"
#include "en/proto/llm.hpp"
#include "en/proto/store.hpp"

namespace en::core {

struct LivedEntry {
  std::string id;        // t<n>, a<n>, l<n>; s and m for self and manner
  std::string who;       // towards: "tarnished" or "npc:<id>"
  std::string feeling;   // towards: fond, grateful, admiring, curious, indifferent, wary, afraid, resentful, scornful
  int trust = 0;         // towards another NPC: −2..+2 (the Tarnished's is the disposition)
  std::string replaces;  // aims: the first-meeting `private.seeks` entry it takes the place of, or empty
  std::string text;
  std::vector<std::string> refs;  // keepsake ids of the journal
  friend bool operator==(const LivedEntry&, const LivedEntry&) = default;
};

struct Lived {
  std::uint32_t version = 0, journal_version = 0;
  proto::Pt as_of_pt = 0;
  std::vector<LivedEntry> towards, aims, learned;
  std::optional<LivedEntry> self, manner;
  std::optional<proto::Pt> self_pt, manner_pt;  // when each last changed: the rare and sustained tiers' clocks
  std::uint32_t next_entry = 1;
  friend bool operator==(const Lived&, const Lived&) = default;
};

[[nodiscard]] std::optional<Lived> lived_of(const proto::LivedVersion& v);
[[nodiscard]] proto::LivedVersion lived_version(const Lived& l, std::uint16_t journey, const std::string& persona);

// MEM §13.2: due once the journal has gained 3 new or rewritten keepsakes, changed its protected keepsake or moved its
// disposition since the lived version's (or the journey's start).
struct LivedDue {
  std::uint32_t changed = 0;  // keepsakes new or rewritten since
  bool protected_changed = false;
  bool disposition_moved = false;
  [[nodiscard]] bool due() const noexcept { return changed >= 3 || protected_changed || disposition_moved; }
};
// What one journal commit adds to the count, from the version before to the one after.
void count_journal_change(LivedDue& due, const std::optional<Journal>& before, const Journal& after);
inline constexpr proto::Pt kLivedEveryMs = 2LL * 3600000;  // at most one update per NPC per 2 h of play
inline constexpr proto::Pt kSelfEveryMs = 10LL * 3600000;
inline constexpr proto::Pt kMannerEveryMs = 5LL * 3600000;
inline constexpr std::uint32_t kLivedTokens = 100;  // its share of block 8 (MEM §13.6)

// The job's input (MEM §13.3): who it is, how things have turned so far, its memories, the facts and feeling in words,
// and the people (P1 the Tarnished; another NPC by its letter).
struct LivedJobInput {
  std::string persona, name;
  std::string self_text, road;
  std::vector<std::string> seeks;  // the first-meeting `private.seeks`, which an aim may replace
  std::optional<Lived> lived;
  Journal journal;
  std::vector<std::string> facts;                           // in words
  std::string feeling_words;                                // the disposition in words
  std::vector<std::pair<std::string, std::string>> people;  // letter, label
  std::function<bool(std::string_view)> proved;             // a claim's check key holds now
};
[[nodiscard]] proto::Expected<std::vector<proto::Message>, std::string> lived_messages(
    std::span<const ContentFile> files, const LivedJobInput& in, std::string_view reminder = {});
[[nodiscard]] nlohmann::json lived_schema();

// MEM §13.5's steps 1 to 5: the new record, or the failure that asks for a retry.
struct LivedCheck {
  std::int8_t disposition = 0;  // the Tarnished's trust
  proto::Pt now = 0;
  std::uint32_t dropped = 0;  // entries left out (citations, tiers, agreement, caps)
};
[[nodiscard]] proto::Expected<Lived, JobFailure> validate_lived(std::string_view text, std::string_view finish,
                                                                const LivedJobInput& in, LivedCheck& check);

// MEM §13.6: block 8's lines under "Since then, in your own words:", the Tarnished's `towards` first, then the others,
// the aims, what it learned, the self and the manner, each as the NPC wrote it, within 100 tokens.
[[nodiscard]] std::vector<std::string> lived_lines(const Lived& l);

}  // namespace en::core
