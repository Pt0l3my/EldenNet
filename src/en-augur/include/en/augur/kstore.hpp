#pragma once
// The K store (DESIGN §6.4, ARCH §8.4 item 4, §8.10): keyed, short-lived events in RAM, merged within their window,
// removed silently at their TTL, at most 512 entries (the earliest-expiring go first past the cap). They reach prompts
// only as at most three audience-filtered "just now" lines. Beside it, the P events' dedup table (by key, in play
// time) and the per-(rule, subject) cooldown table (in mono time).
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "en/augur/events.hpp"
#include "en/augur/types.hpp"

namespace en::augur {

inline constexpr std::size_t kMaxKEntries = 512;
inline constexpr std::size_t kMaxJustNow = 3;

struct KEntry {
  std::string key;
  std::size_t rule = 0;
  Mono t_first = 0;
  Mono t_last = 0;
  Mono expires = 0;
  Event event;  // the latest form: count and payload aggregated
};

class KStore {
 public:
  // What a K fire does to the store (ARCH §8.4 item 4).
  struct Result {
    bool emit = true;     // a new entry, or a merge whose salient field changed
    bool merged = false;  // the fire was coalesced into an entry
  };
  // Within merge_ms of an entry with the same key and rule: coalesce (count + 1, payload aggregated per kind, the
  // expiry renewed); a new event only when a salient field changes (Struck's `hard` turning true), marked merged.
  // Otherwise a new entry (replacing any older one with that key), emitted. `ev` is updated to what is emitted.
  Result put(Event& ev, std::size_t rule, Mono now, std::uint32_t merge_ms, std::uint32_t ttl_s);
  // Freezes the audience (and id) of the entry an emitted K event made, once the step has resolved it.
  void freeze(const Event& ev);
  // Removes, silently, every entry whose time has come.
  void expire(Mono now);
  void clear() noexcept { entries_.clear(); }

  [[nodiscard]] const std::vector<KEntry>& entries() const noexcept { return entries_; }
  // The newest entries the persona perceived, at most three, newest first.
  [[nodiscard]] std::vector<const KEntry*> just_now(PersonaId persona) const;

 private:
  std::vector<KEntry> entries_;
};

// P events by key: the play time of the last one stored.
class DedupTable {
 public:
  // Whether a P event with this key at t_play is a duplicate within dedup_s; if not, it is recorded.
  [[nodiscard]] bool duplicate(const std::string& key, Mono t_play, std::uint32_t dedup_s);
  void clear() noexcept { last_.clear(); }
  [[nodiscard]] std::size_t size() const noexcept { return last_.size(); }

 private:
  std::map<std::string, Mono> last_;
};
inline constexpr std::size_t kMaxDedupKeys = 4096;  // the oldest go first past it (impl)

// (rule, subject) → the mono time of the last emission.
class CooldownTable {
 public:
  [[nodiscard]] bool cooling(std::size_t rule, std::uint64_t subject, Mono now, std::uint32_t cooldown_s);
  void clear() noexcept { last_.clear(); }

 private:
  std::map<std::pair<std::size_t, std::uint64_t>, Mono> last_;
};
inline constexpr std::size_t kMaxCooldowns = 4096;

}  // namespace en::augur
