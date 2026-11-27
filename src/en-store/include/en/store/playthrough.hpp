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
  std::uint16_t journey = 1;          // meta.current_journey
  std::int64_t epoch = 0;             // meta.current_epoch
  proto::EventId next_event_id = 1;   // meta.next_event_id: never decreases, even after a trim
  proto::RowId next_conv_id = 1;      // meta.next_conv_id
  std::int64_t wall = 0;              // the wall time of the request being run, for its rows (seconds)
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

// ---- The event log's SQL (events.cpp: the only file whose SQL names event_audience, MEM §1.3) ----
namespace events {

[[nodiscard]] Result<void> insert(Playthrough& pt, const proto::StoredEvent& ev);
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

}  // namespace events

}  // namespace en::store
