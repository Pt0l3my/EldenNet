#pragma once
// MEM §2.2's fact catalogue. `npc_facts` is a cache of one pure fold over what each persona perceived: `fold_fact`
// takes one stored event as one persona perceived it (its frozen row's `how`) and moves that persona's facts. The same
// function runs live, as each event is written, and in recomputation, over a journey's events after a trim (the store's
// EventsForFold; WHY§689), and a property test holds the two to the same facts.
//
// A meeting is a ConversationEnded with at least one committed line of the persona's, or a NativeTalkEnded with the
// persona; a meeting within 10 min of play of the previous one merges with it (one visit to a shop is one meeting). The
// values are text, as the store keeps them: numbers in decimal (locale-proof), play times in ms, `world_seen` and
// `world_cued` as JSON arrays sorted by item. `told_name` and `verified:<key>` are not folded: the journal holds the
// first (MEM §3.6), and the flag frame decides the second (MEM §2.5).
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/store.hpp"

namespace en::core {

namespace fact {
inline constexpr std::string_view kMetCount = "met_count";
inline constexpr std::string_view kFirstMetPt = "first_met_pt";
inline constexpr std::string_view kFirstMetPlace = "first_met_place";
inline constexpr std::string_view kLastMetPt = "last_met_pt";
inline constexpr std::string_view kLastMetPlace = "last_met_place";
inline constexpr std::string_view kSeenCount = "seen_count";
inline constexpr std::string_view kTimesAttacked = "times_attacked";
inline constexpr std::string_view kLastAttackedPt = "last_attacked_pt";
inline constexpr std::string_view kFoughtBeside = "fought_beside";
inline constexpr std::string_view kFoughtLastBoss = "fought_last_boss";
inline constexpr std::string_view kSeenPlayerDie = "seen_player_die";
inline constexpr std::string_view kStageSeen = "stage_seen";
inline constexpr std::string_view kWorldSeen = "world_seen";
inline constexpr std::string_view kWorldSeenPt = "world_seen_pt";
inline constexpr std::string_view kWorldCued = "world_cued";
}  // namespace fact

inline constexpr proto::Pt kMeetingMergeMs = 600000;  // MEM §2.2: meetings within 10 min merge

// A certainty of the world model (MEM §12.2): fact above rumour above belief; 0 for a word that is none of them.
[[nodiscard]] int certainty_rank(std::string_view word) noexcept;

[[nodiscard]] const proto::FactRow* find_fact(const proto::PersonaFacts& facts, std::string_view key) noexcept;
[[nodiscard]] std::optional<std::int64_t> fact_number(const proto::PersonaFacts& facts, std::string_view key) noexcept;
// A JSON array fact's pairs ([[item, word-or-number], ...]), or none.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> fact_pairs(const proto::PersonaFacts& facts,
                                                                          std::string_view key);

// Folds `ev`, as the persona perceived it (`how`), into its facts, and answers the keys it changed (none: the event
// moves no fact of this persona's). `ev`'s audience is not read: the row is the caller's.
std::vector<std::string> fold_fact(proto::PersonaFacts& facts, const proto::StoredEvent& ev, proto::How how);

// The fold of a journey's events in id order, every audience row's persona: the recomputation, whose result replaces
// the journey's facts (PutFacts). Personas in order of their ids; each one's facts in key order.
[[nodiscard]] std::vector<proto::PersonaFacts> fold_all(const std::vector<proto::StoredEvent>& events);

}  // namespace en::core
