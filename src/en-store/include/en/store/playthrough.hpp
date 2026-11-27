#pragma once
// One open playthrough file and the store's requests on it (MEM §8.6, §8.7): the event log with its frozen audiences,
// pages, conversations, facts, journal and lived versions, memory jobs, and the queries the core reads them back by.
// None of these opens a transaction: the caller wraps each request in one (StoreCore; a burst of appends shares one).
// Every id is 64-bit; every query takes the journey, and a persona sees only its own audience rows (MEM §1.3).
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "en/proto/store.hpp"
#include "en/store/db.hpp"
#include "en/store/files.hpp"

namespace en::store {

struct Playthrough {
  Db db;
  SaveFolder folder;
  proto::PlaythroughKey key;
  std::uint16_t journey = 1;         // meta.current_journey
  std::int64_t epoch = 0;            // meta.current_epoch
  proto::EventId next_event_id = 1;  // meta.next_event_id: never decreases, even after a trim
  proto::RowId next_conv_id = 1;     // meta.next_conv_id
  std::int64_t wall = 0;             // the wall time of the request being run, for its rows (seconds)
};

// Reads meta's counters and current journey and epoch into `pt`.
[[nodiscard]] Result<void> load_meta(Playthrough& pt);
// A new epoch (MEM §9.3, §9.4): its row, and meta's current epoch and journey.
[[nodiscard]] Result<std::int64_t> new_epoch(Playthrough& pt, std::uint16_t journey, proto::Pt start_pt,
                                             std::string_view reason);

// ---- Writes (one request each) ----

// A P event and its audience; the facts its fold changed. A JourneyChanged into a later journey starts that journey's
// epoch first (MEM §9.4), and its event belongs to it.
[[nodiscard]] Result<void> append(Playthrough& pt, const proto::db::Append& a);
// A later page of an NpcSaid: its text appended after a space, its end and pt in the payload's `pages`.
[[nodiscard]] Result<void> append_page(Playthrough& pt, const proto::db::AppendPage& p);
[[nodiscard]] Result<void> conv_open(Playthrough& pt, const proto::ConversationRow& row);
[[nodiscard]] Result<void> conv_close(Playthrough& pt, const proto::ConversationRow& row);
[[nodiscard]] Result<proto::RowId> upsert_job(Playthrough& pt, const proto::MemoryJob& job);
[[nodiscard]] Result<void> commit_journal(Playthrough& pt, const proto::db::CommitJournal& c);
[[nodiscard]] Result<void> commit_lived(Playthrough& pt, const proto::db::CommitLived& c);
[[nodiscard]] Result<void> put_facts(Playthrough& pt, const proto::db::PutFacts& f);

// ---- Queries ----

[[nodiscard]] Result<proto::db::BundleResult> bundle(const Playthrough& pt, const proto::db::Bundle& q);
[[nodiscard]] Result<proto::db::JobInputResult> job_input(const Playthrough& pt, const proto::db::JobInput& q);
[[nodiscard]] Result<proto::db::LivedInputResult> lived_input(const Playthrough& pt, const proto::db::LivedInput& q);
[[nodiscard]] Result<proto::db::FoldEvents> fold_events(const Playthrough& pt, const proto::db::EventsForFold& q);

// The open memory jobs (queued, running, deferred, parked), oldest first.
[[nodiscard]] Result<std::vector<proto::MemoryJob>> open_jobs(const Db& db);

// The block 6a tail's length: the last committed direct lines of the most recent ended conversation (MEM §4.2).
inline constexpr std::size_t kTailLines = 6;

// ---- Rollback, recovery and forgetting (rollback.cpp; MEM §3.8, §9) ----

// Where a cut falls (MEM §9.2's loop): from the newest epoch back, every epoch that starts after the cut goes whole,
// and the first that starts at or before it is cut into (rows after the cut go). No epoch left: none cut into.
struct CutPlan {
  proto::Pt cut = 0;
  std::vector<std::int64_t> whole;
  std::optional<std::int64_t> partial;
  std::uint16_t partial_journey = 1;
  // An SQL condition over a table with an `epoch` column: its rows the cut removes, by `time` (the column of play time
  // it is cut by). Every value in it is ours (ids and play times as numbers).
  [[nodiscard]] std::string removes(std::string_view time, std::string_view table = {}) const;
};
[[nodiscard]] Result<CutPlan> plan_cut(const Db& db, proto::Pt cut);

// H of MEM §9.1: the newest play time the current epoch holds (an event, a reply's last page or a conversation's
// start), or the epoch's start when it holds none.
[[nodiscard]] Result<proto::Pt> newest_pt(const Playthrough& pt);
// What a cut would undo (O144): each kind counted, the newest few items, the stages undone.
[[nodiscard]] Result<proto::db::RollbackSummary> summarise(const Playthrough& pt, const CutPlan& plan);
// MEM §9.2's trim, inside the caller's transaction; the rows it removed. Sets meta.facts_stale for the core's refold.
[[nodiscard]] Result<std::uint64_t> trim(Playthrough& pt, const CutPlan& plan);
// MEM §3.8 steps 1, 2 and 4: open conversations closed as crashed, each with its ConversationEnded for the persona;
// running jobs queued again. The conversations closed.
[[nodiscard]] Result<std::vector<proto::RowId>> recover(Playthrough& pt);
// MEM §9.5's rows, inside the caller's transaction (the backup before it, and the checkpoint after, are the caller's):
// one profiled NPC's in the journey, or the whole journey's when `persona` is empty. The rows removed.
[[nodiscard]] Result<std::uint64_t> forget(Playthrough& pt, std::uint16_t journey, const std::string& persona,
                                           proto::Pt at_pt);

// ---- The event log's SQL (events.cpp: the only file whose SQL names event_audience, MEM §1.3) ----
namespace events {

[[nodiscard]] Result<void> insert(Playthrough& pt, const proto::StoredEvent& ev);
// An event of the store's own (MEM §3.8: a conversation a crash left open, ended), perceived by its persona alone: the
// audience Augur gives every ConversationEnded (the subject radius), so the store composes no other.
[[nodiscard]] Result<void> insert_own(Playthrough& pt, proto::StoredEvent ev, const std::string& persona);
[[nodiscard]] Result<void> add_page(Playthrough& pt, const proto::db::AppendPage& p);
// The persona's view: events of the journey whose audience holds it, each with its own row alone, by id, in
// (after, upto] (upto unset: no bound).
[[nodiscard]] Result<std::vector<proto::StoredEvent>> seen_by(const Db& db, std::uint16_t journey,
                                                              const std::string& persona, proto::EventId after,
                                                              std::optional<proto::EventId> upto);
[[nodiscard]] Result<std::uint32_t> count_seen_by(const Db& db, std::uint16_t journey, const std::string& persona,
                                                  proto::EventId after);
// The last `n` committed lines of a conversation the persona perceived directly, oldest first.
[[nodiscard]] Result<std::vector<proto::StoredEvent>> tail(const Db& db, std::uint16_t journey,
                                                           const std::string& persona, proto::RowId conv,
                                                           std::size_t n);
// Every event of the journey with every audience row, by id (the refold, MEM §9.2).
[[nodiscard]] Result<std::vector<proto::StoredEvent>> all_of(const Db& db, std::uint16_t journey);
// The events a cut removes, summarised (O144): turns, lines addressed to the Tarnished, witnessed events, stage
// changes, with their newest items.
[[nodiscard]] Result<void> summarise_cut(const Db& db, const CutPlan& plan, proto::db::RollbackSummary& out);
// The events a cut removes, deleted (their audience rows and intents cascade); an NpcSaid that straddles the cut keeps
// the pages shown at or before it. The rows removed.
[[nodiscard]] Result<std::uint64_t> cut_events(const Db& db, const CutPlan& plan);
// One persona's audience rows in a journey, then the speech and the persona's own events left with no row (MEM §9.5).
[[nodiscard]] Result<std::uint64_t> forget_persona(const Db& db, std::uint16_t journey, const std::string& persona);

}  // namespace events

}  // namespace en::store
