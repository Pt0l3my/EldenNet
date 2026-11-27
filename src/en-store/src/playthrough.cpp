// The store's requests on one playthrough file (playthrough.hpp; MEM §8.6, §8.7). The event log's own SQL is in
// events.cpp; this file holds the rest: meta's counters, epochs, conversations, facts, journal and lived versions, jobs
// and the queries that read them back.
#include "en/store/playthrough.hpp"

#include <set>

namespace en::store {

namespace {

using proto::EventId;
using proto::RowId;

Result<std::int64_t> meta_number(const Db& db, std::string_view key, std::int64_t fallback) {
  auto v = meta_get(db, key);
  if (!v) return Unexpected{v.error()};
  if (!*v) return fallback;
  const auto n = number_of(**v);
  if (!n) return Unexpected{Fault::Corrupt};
  return *n;
}

proto::JobState state_of(std::string_view w) {
  if (w == "running") return proto::JobState::Running;
  if (w == "deferred") return proto::JobState::Deferred;
  if (w == "parked") return proto::JobState::Parked;
  if (w == "done") return proto::JobState::Done;
  return proto::JobState::Queued;
}

proto::JobCause cause_of(std::string_view w) {
  if (w == "conversation") return proto::JobCause::Conversation;
  if (w == "exchange") return proto::JobCause::Exchange;
  if (w == "witness") return proto::JobCause::Witness;
  return proto::JobCause::None;
}

constexpr std::string_view kJobColumns =
    "id, journey, persona, kind, cause, conv_id, from_event_id, upto_event_id, journal_version, state, attempts, "
    "created_pt, last_error, last_try_wall";

proto::MemoryJob read_job(const Stmt& s) {
  proto::MemoryJob j;
  j.id = s.i64(0);
  j.journey = static_cast<std::uint16_t>(s.i64(1));
  j.persona = s.text(2);
  j.kind = s.text(3) == "lived" ? proto::JobKind::Lived : proto::JobKind::Journal;
  j.cause = cause_of(s.text(4));
  j.conv_id = s.opt_i64(5);
  j.from_event_id = s.opt_i64(6);
  j.upto_event_id = s.opt_i64(7);
  if (const auto v = s.opt_i64(8)) j.journal_version = static_cast<std::uint32_t>(*v);
  j.state = state_of(s.text(9));
  j.attempts = static_cast<std::uint32_t>(s.i64(10));
  j.created_pt = s.i64(11);
  j.last_error = s.text(12);
  j.last_try_wall = s.opt_i64(13);
  return j;
}

constexpr std::string_view kJournalColumns =
    "journey, persona, version, as_of_pt, from_event_id, upto_event_id, conv_id, summary, keepsakes, next_keepsake, "
    "disposition, told_name, told_name_verified, told_name_heard, model_fp, prompt_hash, persona_hash, job_id, "
    "written_wall";

proto::JournalVersion read_journal(const Stmt& s) {
  proto::JournalVersion v;
  v.journey = static_cast<std::uint16_t>(s.i64(0));
  v.persona = s.text(1);
  v.version = static_cast<std::uint32_t>(s.i64(2));
  v.as_of_pt = s.i64(3);
  v.from_event_id = s.i64(4);
  v.upto_event_id = s.i64(5);
  v.conv_id = s.opt_i64(6);
  v.summary = s.text(7);
  v.keepsakes = s.text(8);
  v.next_keepsake = static_cast<std::uint32_t>(s.i64(9));
  v.disposition = static_cast<std::int8_t>(s.i64(10));
  v.told_name = s.opt_text(11);
  v.told_name_verified = s.i64(12) != 0;
  v.told_name_heard = s.i64(13) != 0;
  v.model_fp = s.text(14);
  v.prompt_hash = s.text(15);
  v.persona_hash = s.text(16);
  v.job_id = s.opt_i64(17);
  v.written_wall = s.i64(18);
  return v;
}

constexpr std::string_view kLivedColumns =
    "journey, persona, version, journal_version, as_of_pt, towards, aims, learned, self_line, self_pt, manner, "
    "manner_pt, next_entry, model_fp, prompt_hash, persona_hash, job_id, written_wall";

proto::LivedVersion read_lived(const Stmt& s) {
  proto::LivedVersion v;
  v.journey = static_cast<std::uint16_t>(s.i64(0));
  v.persona = s.text(1);
  v.version = static_cast<std::uint32_t>(s.i64(2));
  v.journal_version = static_cast<std::uint32_t>(s.i64(3));
  v.as_of_pt = s.i64(4);
  v.towards = s.text(5);
  v.aims = s.text(6);
  v.learned = s.text(7);
  v.self_line = s.opt_text(8);
  v.self_pt = s.opt_i64(9);
  v.manner = s.opt_text(10);
  v.manner_pt = s.opt_i64(11);
  v.next_entry = static_cast<std::uint32_t>(s.i64(12));
  v.model_fp = s.text(13);
  v.prompt_hash = s.text(14);
  v.persona_hash = s.text(15);
  v.job_id = s.opt_i64(16);
  v.written_wall = s.i64(17);
  return v;
}

Result<std::optional<proto::JournalVersion>> newest_journal(const Db& db, std::uint16_t journey,
                                                            const std::string& persona) {
  auto s = Stmt::prepare(db, "SELECT " + std::string(kJournalColumns) +
                                 " FROM npc_journal WHERE journey = ?1 AND persona = ?2 ORDER BY version DESC LIMIT 1");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::int64_t{journey}).bind(2, std::string_view(persona));
  auto r = s->step();
  if (!r) return Unexpected{r.error()};
  if (!*r) return std::optional<proto::JournalVersion>{};
  return std::optional<proto::JournalVersion>{read_journal(*s)};
}

Result<std::optional<proto::LivedVersion>> newest_lived(const Db& db, std::uint16_t journey,
                                                        const std::string& persona) {
  auto s = Stmt::prepare(db, "SELECT " + std::string(kLivedColumns) +
                                 " FROM npc_lived WHERE journey = ?1 AND persona = ?2 ORDER BY version DESC LIMIT 1");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::int64_t{journey}).bind(2, std::string_view(persona));
  auto r = s->step();
  if (!r) return Unexpected{r.error()};
  if (!*r) return std::optional<proto::LivedVersion>{};
  return std::optional<proto::LivedVersion>{read_lived(*s)};
}

Result<std::vector<proto::FactRow>> facts_of(const Db& db, std::uint16_t journey, const std::string& persona) {
  auto s = Stmt::prepare(db,
                         "SELECT key, value, src_event, updated_pt FROM npc_facts WHERE journey = ?1 AND persona = ?2 "
                         "ORDER BY key");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::int64_t{journey}).bind(2, std::string_view(persona));
  std::vector<proto::FactRow> out;
  for (;;) {
    auto r = s->step();
    if (!r) return Unexpected{r.error()};
    if (!*r) break;
    out.push_back(proto::FactRow{s->text(0), s->text(1), s->opt_i64(2), s->i64(3)});
  }
  return out;
}

Result<void> set_facts(const Db& db, std::uint16_t journey, const std::string& persona,
                       const std::vector<proto::FactRow>& facts) {
  if (facts.empty()) return {};
  if (persona.empty()) return Unexpected{Fault::Invalid};
  auto s = Stmt::prepare(db,
                         "INSERT INTO npc_facts (journey, persona, key, value, src_event, updated_pt) "
                         "VALUES (?1, ?2, ?3, ?4, ?5, ?6) ON CONFLICT (journey, persona, key) DO UPDATE SET "
                         "value = excluded.value, src_event = excluded.src_event, updated_pt = excluded.updated_pt");
  if (!s) return Unexpected{s.error()};
  for (const proto::FactRow& f : facts) {
    if (f.key.empty()) return Unexpected{Fault::Invalid};
    s->reset();
    s->bind(1, std::int64_t{journey}).bind(2, std::string_view(persona)).bind(3, std::string_view(f.key));
    s->bind(4, std::string_view(f.value)).bind(5, f.src_event).bind(6, f.updated_pt);
    if (auto r = s->run(); !r) return r;
  }
  return {};
}

Result<proto::EventId> watermark_of(const Db& db, std::uint16_t journey, const std::string& persona) {
  auto s = Stmt::prepare(db, "SELECT watermark FROM memory_progress WHERE journey = ?1 AND persona = ?2");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::int64_t{journey}).bind(2, std::string_view(persona));
  auto r = s->step();
  if (!r) return Unexpected{r.error()};
  return *r ? s->i64(0) : EventId{0};
}

}  // namespace

Result<void> load_meta(Playthrough& pt) {
  auto next = meta_number(pt.db, "next_event_id", 1);
  if (!next) return Unexpected{next.error()};
  auto conv = meta_number(pt.db, "next_conv_id", 1);
  if (!conv) return Unexpected{conv.error()};
  auto journey = meta_number(pt.db, "current_journey", 1);
  if (!journey) return Unexpected{journey.error()};
  auto epoch = meta_number(pt.db, "current_epoch", 0);
  if (!epoch) return Unexpected{epoch.error()};
  if (*next < 1 || *conv < 1 || *journey < 1 || *journey > 0xFFFF) return Unexpected{Fault::Corrupt};
  pt.next_event_id = *next;
  pt.next_conv_id = *conv;
  pt.journey = static_cast<std::uint16_t>(*journey);
  pt.epoch = *epoch;
  return {};
}

Result<std::int64_t> new_epoch(Playthrough& pt, std::uint16_t journey, proto::Pt start_pt, std::string_view reason) {
  auto s = Stmt::prepare(pt.db,
                         "INSERT INTO epochs (journey, start_pt, start_event_id, reason, created_wall) "
                         "VALUES (?1, ?2, ?3, ?4, ?5)");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::int64_t{journey}).bind(2, start_pt).bind(3, pt.next_event_id).bind(4, reason).bind(5, pt.wall);
  if (auto r = s->run(); !r) return Unexpected{r.error()};
  const std::int64_t epoch = pt.db.last_insert_rowid();
  if (auto r = meta_set(pt.db, "current_epoch", number_text(epoch)); !r) return Unexpected{r.error()};
  if (auto r = meta_set(pt.db, "current_journey", number_text(journey)); !r) return Unexpected{r.error()};
  pt.epoch = epoch;
  pt.journey = journey;
  return epoch;
}

Result<void> append(Playthrough& pt, const proto::db::Append& a) {
  if (a.event.kind == "JourneyChanged" && a.event.journey > pt.journey) {
    if (auto r = new_epoch(pt, a.event.journey, a.event.t_play_ms, "journey"); !r) return Unexpected{r.error()};
  }
  if (auto r = events::insert(pt, a.event); !r) return r;
  for (const proto::PersonaFacts& f : a.facts) {
    if (auto r = set_facts(pt.db, a.event.journey, f.persona, f.facts); !r) return r;
  }
  return {};
}

Result<void> append_page(Playthrough& pt, const proto::db::AppendPage& p) { return events::add_page(pt, p); }

Result<void> conv_open(Playthrough& pt, const proto::ConversationRow& row) {
  if (row.id < 1 || row.persona.empty()) return Unexpected{Fault::Invalid};
  auto s = Stmt::prepare(pt.db,
                         "INSERT INTO conversations (id, journey, epoch, persona, stage, started_pt, shown_context, "
                         "persona_hash, kb_hash, model_fp) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)");
  if (!s) return Unexpected{s.error()};
  s->bind(1, row.id).bind(2, std::int64_t{row.journey}).bind(3, pt.epoch).bind(4, std::string_view(row.persona));
  s->bind(5, std::string_view(row.stage)).bind(6, row.started_pt);
  s->bind(7, std::string_view(row.shown_context.empty() ? std::string("[]") : row.shown_context));
  s->bind(8, std::string_view(row.persona_hash)).bind(9, std::string_view(row.kb_hash)).bind(10, row.model_fp);
  if (auto r = s->run(); !r) return r;
  if (row.id >= pt.next_conv_id) {
    pt.next_conv_id = row.id + 1;
    if (auto r = meta_set(pt.db, "next_conv_id", number_text(pt.next_conv_id)); !r) return r;
  }
  return {};
}

Result<void> conv_close(Playthrough& pt, const proto::ConversationRow& row) {
  auto s = Stmt::prepare(pt.db,
                         "UPDATE conversations SET ended_pt = ?2, end_reason = ?3, player_lines = ?4, "
                         "committed_lines = ?5, first_event_id = ?6, last_event_id = ?7, shown_context = ?8, "
                         "model_fp = coalesce(?9, model_fp) WHERE id = ?1");
  if (!s) return Unexpected{s.error()};
  s->bind(1, row.id)
      .bind(2, row.ended_pt)
      .bind(3, std::string_view(row.end_reason.empty() ? "closed" : row.end_reason));
  s->bind(4, std::int64_t{row.player_lines}).bind(5, std::int64_t{row.committed_lines});
  s->bind(6, row.first_event_id).bind(7, row.last_event_id);
  s->bind(8, std::string_view(row.shown_context.empty() ? std::string("[]") : row.shown_context)).bind(9, row.model_fp);
  if (auto r = s->run(); !r) return r;
  if (pt.db.changes() != 1) return Unexpected{Fault::Invalid};
  return {};
}

Result<RowId> upsert_job(Playthrough& pt, const proto::MemoryJob& job) {
  if (job.persona.empty()) return Unexpected{Fault::Invalid};
  const bool fresh = job.id == 0;
  auto s = Stmt::prepare(
      pt.db, fresh ? "INSERT INTO memory_jobs (journey, persona, kind, cause, conv_id, from_event_id, upto_event_id, "
                     "journal_version, state, attempts, created_pt, last_error, last_try_wall) VALUES "
                     "(?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14)"
                   : "UPDATE memory_jobs SET journey = ?2, persona = ?3, kind = ?4, cause = ?5, conv_id = ?6, "
                     "from_event_id = ?7, upto_event_id = ?8, journal_version = ?9, state = ?10, attempts = ?11, "
                     "created_pt = ?12, last_error = ?13, last_try_wall = ?14 WHERE id = ?1");
  if (!s) return Unexpected{s.error()};
  if (!fresh) s->bind(1, job.id);
  s->bind(2, std::int64_t{job.journey}).bind(3, std::string_view(job.persona));
  s->bind(4, std::string_view(proto::to_string(job.kind)));
  if (job.cause == proto::JobCause::None) {
    s->bind_null(5);
  } else {
    s->bind(5, std::string_view(proto::to_string(job.cause)));
  }
  s->bind(6, job.conv_id).bind(7, job.from_event_id).bind(8, job.upto_event_id);
  if (job.journal_version) {
    s->bind(9, std::int64_t{*job.journal_version});
  } else {
    s->bind_null(9);
  }
  s->bind(10, std::string_view(proto::to_string(job.state))).bind(11, std::int64_t{job.attempts});
  s->bind(12, job.created_pt);
  if (job.last_error.empty()) {
    s->bind_null(13);
  } else {
    s->bind(13, std::string_view(job.last_error));
  }
  s->bind(14, job.last_try_wall);
  if (auto r = s->run(); !r) return Unexpected{r.error()};
  if (fresh) return pt.db.last_insert_rowid();
  if (pt.db.changes() != 1) return Unexpected{Fault::Invalid};
  return job.id;
}

Result<void> commit_journal(Playthrough& pt, const proto::db::CommitJournal& c) {
  const proto::JournalVersion& v = c.version;
  if (v.persona.empty() || v.version < 1) return Unexpected{Fault::Invalid};
  auto s = Stmt::prepare(pt.db, "INSERT INTO npc_journal (" + std::string(kJournalColumns) +
                                    ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, "
                                    "?17, ?18, ?19)");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::int64_t{v.journey}).bind(2, std::string_view(v.persona)).bind(3, std::int64_t{v.version});
  s->bind(4, v.as_of_pt).bind(5, v.from_event_id).bind(6, v.upto_event_id).bind(7, v.conv_id);
  s->bind(8, std::string_view(v.summary))
      .bind(9, std::string_view(v.keepsakes))
      .bind(10, std::int64_t{v.next_keepsake});
  s->bind(11, std::int64_t{v.disposition}).bind(12, v.told_name).bind(13, std::int64_t{v.told_name_verified ? 1 : 0});
  s->bind(14, std::int64_t{v.told_name_heard ? 1 : 0}).bind(15, std::string_view(v.model_fp));
  s->bind(16, std::string_view(v.prompt_hash)).bind(17, std::string_view(v.persona_hash)).bind(18, c.job);
  s->bind(19, v.written_wall != 0 ? v.written_wall : pt.wall);
  if (auto r = s->run(); !r) return r;
  // The watermark the version covers (MEM §3.7): it moves only on a commit.
  auto w =
      Stmt::prepare(pt.db,
                    "INSERT INTO memory_progress (journey, persona, watermark, updated_pt) VALUES (?1, ?2, ?3, ?4) "
                    "ON CONFLICT (journey, persona) DO UPDATE SET watermark = excluded.watermark, "
                    "updated_pt = excluded.updated_pt");
  if (!w) return Unexpected{w.error()};
  w->bind(1, std::int64_t{v.journey}).bind(2, std::string_view(v.persona)).bind(3, v.upto_event_id).bind(4, v.as_of_pt);
  if (auto r = w->run(); !r) return r;
  if (c.job) {
    auto j = Stmt::prepare(pt.db, "UPDATE memory_jobs SET state = 'done' WHERE id = ?1");
    if (!j) return Unexpected{j.error()};
    j->bind(1, *c.job);
    if (auto r = j->run(); !r) return r;
  }
  return set_facts(pt.db, v.journey, v.persona, c.facts);
}

Result<void> commit_lived(Playthrough& pt, const proto::db::CommitLived& c) {
  const proto::LivedVersion& v = c.version;
  if (v.persona.empty() || v.version < 1) return Unexpected{Fault::Invalid};
  auto s = Stmt::prepare(pt.db, "INSERT INTO npc_lived (" + std::string(kLivedColumns) +
                                    ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, "
                                    "?17, ?18)");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::int64_t{v.journey}).bind(2, std::string_view(v.persona)).bind(3, std::int64_t{v.version});
  s->bind(4, std::int64_t{v.journal_version}).bind(5, v.as_of_pt).bind(6, std::string_view(v.towards));
  s->bind(7, std::string_view(v.aims)).bind(8, std::string_view(v.learned)).bind(9, v.self_line).bind(10, v.self_pt);
  s->bind(11, v.manner)
      .bind(12, v.manner_pt)
      .bind(13, std::int64_t{v.next_entry})
      .bind(14, std::string_view(v.model_fp));
  s->bind(15, std::string_view(v.prompt_hash)).bind(16, std::string_view(v.persona_hash)).bind(17, c.job);
  s->bind(18, v.written_wall != 0 ? v.written_wall : pt.wall);
  if (auto r = s->run(); !r) return r;
  if (c.job) {
    auto j = Stmt::prepare(pt.db, "UPDATE memory_jobs SET state = 'done' WHERE id = ?1");
    if (!j) return Unexpected{j.error()};
    j->bind(1, *c.job);
    if (auto r = j->run(); !r) return r;
  }
  return {};
}

Result<void> put_facts(Playthrough& pt, const proto::db::PutFacts& f) {
  auto d = Stmt::prepare(pt.db, "DELETE FROM npc_facts WHERE journey = ?1");
  if (!d) return Unexpected{d.error()};
  d->bind(1, std::int64_t{f.journey});
  if (auto r = d->run(); !r) return r;
  for (const proto::PersonaFacts& p : f.personas) {
    if (auto r = set_facts(pt.db, f.journey, p.persona, p.facts); !r) return r;
  }
  return meta_set(pt.db, "facts_stale", "0");
}

Result<proto::db::BundleResult> bundle(const Playthrough& pt, const proto::db::Bundle& q) {
  proto::db::BundleResult out;
  auto j = newest_journal(pt.db, q.journey, q.persona);
  if (!j) return Unexpected{j.error()};
  out.journal = std::move(*j);
  auto l = newest_lived(pt.db, q.journey, q.persona);
  if (!l) return Unexpected{l.error()};
  out.lived = std::move(*l);
  auto f = facts_of(pt.db, q.journey, q.persona);
  if (!f) return Unexpected{f.error()};
  out.facts = std::move(*f);
  // The most recent ended conversation with the persona in this journey, and its tail (block 6a).
  auto c = Stmt::prepare(pt.db,
                         "SELECT id FROM conversations WHERE journey = ?1 AND persona = ?2 AND ended_pt IS NOT NULL "
                         "ORDER BY started_pt DESC, id DESC LIMIT 1");
  if (!c) return Unexpected{c.error()};
  c->bind(1, std::int64_t{q.journey}).bind(2, std::string_view(q.persona));
  auto has = c->step();
  if (!has) return Unexpected{has.error()};
  if (*has) {
    out.prev_conv = c->i64(0);
    auto tail = events::tail(pt.db, q.journey, q.persona, *out.prev_conv, kTailLines);
    if (!tail) return Unexpected{tail.error()};
    out.prev_tail = std::move(*tail);
  }
  auto w = watermark_of(pt.db, q.journey, q.persona);
  if (!w) return Unexpected{w.error()};
  auto n = events::count_seen_by(pt.db, q.journey, q.persona, *w);
  if (!n) return Unexpected{n.error()};
  out.pending_count = *n;
  return out;
}

Result<proto::db::JobInputResult> job_input(const Playthrough& pt, const proto::db::JobInput& q) {
  proto::db::JobInputResult out;
  auto s = Stmt::prepare(pt.db, "SELECT " + std::string(kJobColumns) + " FROM memory_jobs WHERE id = ?1");
  if (!s) return Unexpected{s.error()};
  s->bind(1, q.job);
  auto r = s->step();
  if (!r) return Unexpected{r.error()};
  if (!*r) return Unexpected{Fault::Invalid};
  out.job = read_job(*s);
  auto j = newest_journal(pt.db, out.job.journey, out.job.persona);
  if (!j) return Unexpected{j.error()};
  out.journal = std::move(*j);
  auto w = watermark_of(pt.db, out.job.journey, out.job.persona);
  if (!w) return Unexpected{w.error()};
  out.watermark = *w;
  auto ev = events::seen_by(pt.db, out.job.journey, out.job.persona, out.watermark, out.job.upto_event_id);
  if (!ev) return Unexpected{ev.error()};
  out.evidence = std::move(*ev);
  // The conversations the evidence's lines belong to, for their `shown_context`.
  std::set<RowId> convs;
  for (const proto::StoredEvent& e : out.evidence) {
    if (e.conv_id) convs.insert(*e.conv_id);
  }
  auto cs = Stmt::prepare(pt.db,
                          "SELECT id, journey, epoch, persona, stage, started_pt, ended_pt, end_reason, player_lines, "
                          "committed_lines, first_event_id, last_event_id, shown_context, persona_hash, kb_hash, "
                          "model_fp FROM conversations WHERE id = ?1");
  if (!cs) return Unexpected{cs.error()};
  for (const RowId id : convs) {
    cs->reset();
    cs->bind(1, id);
    auto got = cs->step();
    if (!got) return Unexpected{got.error()};
    if (!*got) continue;
    proto::ConversationRow c;
    c.id = cs->i64(0);
    c.journey = static_cast<std::uint16_t>(cs->i64(1));
    c.epoch = cs->i64(2);
    c.persona = cs->text(3);
    c.stage = cs->text(4);
    c.started_pt = cs->i64(5);
    c.ended_pt = cs->opt_i64(6);
    c.end_reason = cs->text(7);
    c.player_lines = static_cast<std::uint32_t>(cs->i64(8));
    c.committed_lines = static_cast<std::uint32_t>(cs->i64(9));
    c.first_event_id = cs->opt_i64(10);
    c.last_event_id = cs->opt_i64(11);
    c.shown_context = cs->text(12);
    c.persona_hash = cs->text(13);
    c.kb_hash = cs->text(14);
    c.model_fp = cs->opt_text(15);
    out.conversations.push_back(std::move(c));
  }
  return out;
}

Result<proto::db::LivedInputResult> lived_input(const Playthrough& pt, const proto::db::LivedInput& q) {
  proto::db::LivedInputResult out;
  auto l = newest_lived(pt.db, q.journey, q.persona);
  if (!l) return Unexpected{l.error()};
  out.lived = std::move(*l);
  auto j = newest_journal(pt.db, q.journey, q.persona);
  if (!j) return Unexpected{j.error()};
  out.journal = std::move(*j);
  auto f = facts_of(pt.db, q.journey, q.persona);
  if (!f) return Unexpected{f.error()};
  out.facts = std::move(*f);
  return out;
}

Result<proto::db::FoldEvents> fold_events(const Playthrough& pt, const proto::db::EventsForFold& q) {
  auto all = events::all_of(pt.db, q.journey);
  if (!all) return Unexpected{all.error()};
  return proto::db::FoldEvents{q.journey, std::move(*all)};
}

Result<std::vector<proto::MemoryJob>> open_jobs(const Db& db) {
  auto s = Stmt::prepare(db, "SELECT " + std::string(kJobColumns) +
                                 " FROM memory_jobs WHERE state IN ('queued', 'running', 'deferred', 'parked') "
                                 "ORDER BY created_pt, id");
  if (!s) return Unexpected{s.error()};
  std::vector<proto::MemoryJob> out;
  for (;;) {
    auto r = s->step();
    if (!r) return Unexpected{r.error()};
    if (!*r) break;
    out.push_back(read_job(*s));
  }
  return out;
}

}  // namespace en::store
