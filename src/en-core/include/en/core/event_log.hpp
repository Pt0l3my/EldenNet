#pragma once
// The conversation's commits and the event log they go to (NPC §8.7, DESIGN §8.5; WHY§364 (7)). The conversation
// answers what to commit on each step (a conversation started or ended, a line of the Tarnished's sent, a page of a
// reply shown); the core hands each commit to its store in the same step. v0.2 has no store: the log is kept in memory
// behind this interface, `EventStore`, and the store (v0.3) takes its place, each page's commit its own transaction.
//
// Only what was shown is ever committed: an NpcSaid is made by its reply's first PageShown and grows by each later
// one; a page that was sealed, held, dropped, cut off or replaced by the "…" page never reaches the log. The log holds
// chat text, so it is never written to a log file, a recording or a report of numbers (PV rules).
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "en/core/gate.hpp"  // Mono
#include "en/proto/ui.hpp"   // ConvId, ReplyId

namespace en::core {

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
[[nodiscard]] const char* to_string(EndReason r) noexcept;  // "closed", "walked_away", ...

struct Commit {
  enum class Kind : std::uint8_t {
    Started,     // ConversationStarted: `persona`
    PlayerSaid,  // the Tarnished's line, as sent: `text`
    NpcSaid,     // a reply's first page shown: creates the NpcSaid, `text` is page 0
    NpcPage,     // a later page of the same reply shown: `text` is appended (`page` is its index)
    CutOff,      // the reply was cut off after the pages shown (NPC §6.3)
    Ended,       // ConversationEnded: `reason`, `player_lines`, `committed`
  };
  Kind kind = Kind::Started;
  proto::ConvId conv = 0;
  proto::ReplyId reply = 0;  // NpcSaid, NpcPage, CutOff
  std::uint8_t page = 0;     // NpcSaid (0) and NpcPage
  std::string persona;       // the partner's id (every kind)
  std::string text;
  EndReason reason = EndReason::Closed;  // Ended
  std::uint16_t player_lines = 0;        // Ended: the Tarnished's lines sent
  std::uint16_t committed = 0;           // Ended: lines committed (the Tarnished's and the replies shown)
  Mono at = 0;

  friend bool operator==(const Commit&, const Commit&) = default;
};

// One stored event, as the log keeps it.
struct LoggedEvent {
  std::uint64_t id = 0;  // the log's own, from 1
  Commit::Kind kind = Commit::Kind::Started;  // Started, PlayerSaid, NpcSaid or Ended (pages grow their NpcSaid)
  proto::ConvId conv = 0;
  proto::ReplyId reply = 0;
  std::string persona;
  std::string text;          // PlayerSaid's line; NpcSaid's pages joined by a space
  std::uint8_t pages = 0;    // NpcSaid
  bool cut_off = false;      // NpcSaid
  EndReason reason = EndReason::Closed;
  std::uint16_t player_lines = 0, committed = 0;
  Mono at = 0;               // when it was created

  friend bool operator==(const LoggedEvent&, const LoggedEvent&) = default;
};

// The store's side of a commit.
class EventStore {
 public:
  virtual ~EventStore() = default;
  virtual void apply(const Commit& commit) = 0;
};

// v0.2's log, in memory: the newest 4,096 events (ARCH C8's RAM ring), the oldest dropped first.
class MemoryLog final : public EventStore {
 public:
  static constexpr std::size_t kCapacity = 4096;

  void apply(const Commit& commit) override;
  [[nodiscard]] const std::deque<LoggedEvent>& events() const noexcept { return events_; }
  [[nodiscard]] std::uint64_t dropped() const noexcept { return dropped_; }
  // The conversation's last `n` shown lines with `persona` before conversation `before` (block 6 (a)), oldest first.
  [[nodiscard]] std::vector<LoggedEvent> last_lines(std::string_view persona, proto::ConvId before, std::size_t n) const;

 private:
  std::deque<LoggedEvent> events_;
  std::uint64_t next_ = 1;
  std::uint64_t dropped_ = 0;
};

}  // namespace en::core
