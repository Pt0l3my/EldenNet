#pragma once
// The journal and block 8 (MEM §2.3, §2.4, §4; v0.3 commit 11). A journal version is the store's row (en-proto's
// JournalVersion), its keepsakes the core's JSON; `journal_of` reads it, `keepsakes_json` writes it. What a dialogue
// prompt shows of the Tarnished comes from the persona's bundle (the journal, the facts, the previous conversation's
// tail; MEM §4.1), composed here into block 8's parts in MEM §4.3's order and words, within its 450 tokens (§4.4).
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "en/proto/store.hpp"

namespace en::core {

struct Keepsake {
  enum class Basis : std::uint8_t { Said, Seen };
  std::string id;  // "k1", "k2", ... per journey and persona, never reused
  std::string text;
  Basis basis = Basis::Said;
  double weight = 0.5;                   // 0..1 in steps of 0.05
  std::string about = "tarnished";       // tarnished | self | world | npc:<id>
  std::optional<std::string> check_key;  // MEM §2.5; Said only
  std::vector<std::string> refs;         // supporting event ids or context-line ids
  proto::Pt since_pt = 0, updated_pt = 0;
  friend bool operator==(const Keepsake&, const Keepsake&) = default;
};

struct Journal {
  std::uint32_t version = 0;
  std::string summary;
  std::vector<Keepsake> keepsakes;
  std::int8_t disposition = 0;  // −2..+2
  std::optional<std::string> told_name;
  bool told_name_verified = false;
  bool told_name_heard = false;  // heard, not told to the persona itself (overheard: DESIGN §8.8)
  proto::Pt as_of_pt = 0;
  proto::EventId upto_event = 0;
  friend bool operator==(const Journal&, const Journal&) = default;
};

// A stored version as the core reads it; a keepsakes text that is not the core's JSON gives none.
[[nodiscard]] std::optional<Journal> journal_of(const proto::JournalVersion& v);
[[nodiscard]] std::string keepsakes_json(const std::vector<Keepsake>& keepsakes);
// MEM §2.3: the heaviest keepsake about the Tarnished with a weight of at least 0.7, the older `since_pt` on a tie.
[[nodiscard]] std::optional<std::size_t> protected_keepsake(const Journal& j) noexcept;

// What the store keeps of one persona for its prompts (MEM §4.1), as the core caches it.
struct MemoryBundle {
  std::optional<Journal> journal;
  std::vector<proto::StoredEvent> prev_tail;  // the last direct lines of the most recent ended conversation
  std::uint32_t pending = 0;                  // events above the watermark it perceived
  std::optional<proto::LivedVersion> lived;
};

// Block 8's parts, rendered (MEM §4.3): the told name and the facts in words on one paragraph, the disposition, the
// summary in the persona's own words, then the three keepsake groups, each in weight order. A keepsake keeps its
// weight and whether it is protected, for the prompt's trim (MEM §4.4).
struct TarnishedBlock {
  struct Line {
    std::string text;
    double weight = 0;
    bool protect = false;
  };
  std::vector<std::string> facts;  // the name first, then each fact's sentence
  std::string disposition;
  std::string summary;
  std::vector<Line> seen, proved, said;
  friend bool operator==(const TarnishedBlock&, const TarnishedBlock&) = default;
};

// The moment block 8 is rendered at.
struct BlockNow {
  proto::Pt pt = 0;                                // the play time now
  std::map<std::int32_t, std::string> boss_names;  // runtime NpcName by name id, as the bars showed them
};

inline constexpr std::uint32_t kBlock8Cap = 450;
// The prompt's estimate (NPC §7.1): ceil(bytes / 3.6 × 1.15).
[[nodiscard]] std::uint32_t estimate_tokens(std::size_t bytes) noexcept;

// The persona's block 8 from its facts and journal (either may be absent), trimmed to its cap by MEM §4.4's order:
// keepsakes by lowest weight down to the protected one, then the fact sentences other than the name, the meetings and
// the attacks. With neither a journal nor a meeting, the single line "You have never spoken with this Tarnished."
// (and the sightings' line when it applies).
[[nodiscard]] TarnishedBlock tarnished_block(const proto::PersonaFacts* facts, const Journal* journal,
                                             const BlockNow& now);
// The block's estimate, as the template sets it out.
[[nodiscard]] std::uint32_t block_tokens(const TarnishedBlock& b) noexcept;

}  // namespace en::core
