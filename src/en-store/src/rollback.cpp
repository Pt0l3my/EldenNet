// Rollback, crash recovery and forgetting on one playthrough file (playthrough.hpp; MEM §3.8, §9, §13.7, §12.9; O144).
// Each function runs inside the caller's transaction; the event log's own SQL is events.cpp's.
#include <algorithm>
#include <map>

#include "en/store/playthrough.hpp"

namespace en::store {

namespace {

using proto::Pt;
using proto::RowId;

std::string id_list(const std::vector<std::int64_t>& ids) {
  std::string out;
  for (const std::int64_t id : ids) {
    if (!out.empty()) out += ", ";
    out += number_text(id);
  }
  return out;
}

Result<std::uint64_t> run_counting(const Db& db, const std::string& sql) {
  auto s = Stmt::prepare(db, sql);
  if (!s) return Unexpected{s.error()};
  if (auto r = s->run(); !r) return Unexpected{r.error()};
  return static_cast<std::uint64_t>(db.changes());
}

Result<std::int64_t> one_number(const Db& db, const std::string& sql) {
  auto s = Stmt::prepare(db, sql);
  if (!s) return Unexpected{s.error()};
  auto r = s->step();
  if (!r) return Unexpected{r.error()};
  return *r && !s->null(0) ? s->i64(0) : std::int64_t{0};
}

}  // namespace

std::string CutPlan::removes(std::string_view time, std::string_view table) const {
  const std::string col = table.empty() ? std::string("epoch") : std::string(table) + ".epoch";
  std::string out = "(";
  bool any = false;
  if (!whole.empty()) {
    out += col + " IN (" + id_list(whole) + ")";
    any = true;
  }
  if (partial) {
    if (any) out += " OR ";
    out += "(" + col + " = " + number_text(*partial) + " AND " + std::string(time) + " > " + number_text(cut) + ")";
    any = true;
  }
  if (!any) out += "0";
  return out + ")";
}

Result<CutPlan> plan_cut(const Db& db, Pt cut) {
  CutPlan plan;
  plan.cut = cut;
  auto s = Stmt::prepare(db, "SELECT epoch, journey, start_pt FROM epochs ORDER BY epoch DESC");
  if (!s) return Unexpected{s.error()};
  for (;;) {
    auto r = s->step();
    if (!r) return Unexpected{r.error()};
    if (!*r) break;
    if (cut >= s->i64(2)) {
      plan.partial = s->i64(0);
      plan.partial_journey = static_cast<std::uint16_t>(s->i64(1));
      break;
    }
    plan.whole.push_back(s->i64(0));
  }
  return plan;
}

Result<Pt> newest_pt(const Playthrough& pt) {
  auto start = one_number(pt.db, "SELECT start_pt FROM epochs WHERE epoch = " + number_text(pt.epoch));
  if (!start) return Unexpected{start.error()};
  // The newest record of the epoch: an event, the last page of a reply, or a conversation's start.
  const std::string e = number_text(pt.epoch);
  auto newest = one_number(pt.db,
                           "SELECT max(coalesce((SELECT max(coalesce(t_play_end_ms, t_play_ms)) FROM events "
                           "WHERE epoch = " +
                               e +
                               "), 0), coalesce((SELECT max(started_pt) FROM conversations "
                               "WHERE epoch = " +
                               e + "), 0))");
  if (!newest) return Unexpected{newest.error()};
  return std::max(*start, *newest);
}

Result<proto::db::RollbackSummary> summarise(const Playthrough& pt, const CutPlan& plan) {
  using Item = proto::db::RollbackItem;
  proto::db::RollbackSummary out;
  out.file_journey = pt.journey;
  if (auto r = events::summarise_cut(pt.db, plan, out); !r) return Unexpected{r.error()};
  // What the memory job wrote after the cut: journal and lived versions (with the claims among the journal's
  // keepsakes), and recall memories (reserved, empty in v1).
  auto j = Stmt::prepare(pt.db,
                         "SELECT persona, as_of_pt, (SELECT count(*) FROM json_each(keepsakes) k "
                         "WHERE json_extract(k.value, '$.basis') = 'said') FROM npc_journal WHERE as_of_pt > ?1 "
                         "UNION ALL SELECT persona, as_of_pt, 0 FROM npc_lived WHERE as_of_pt > ?1 "
                         "UNION ALL SELECT persona, t_play_ms, 0 FROM memories WHERE t_play_ms > ?1 "
                         "ORDER BY 2 DESC");
  if (!j) return Unexpected{j.error()};
  j->bind(1, plan.cut);
  std::vector<Item> memory_items;
  for (;;) {
    auto r = j->step();
    if (!r) return Unexpected{r.error()};
    if (!*r) break;
    ++out.memories;
    out.claims += static_cast<std::uint32_t>(j->i64(2));
    memory_items.push_back(Item{Item::Kind::Memory, j->text(0), std::nullopt, j->i64(1)});
  }
  // Later journeys' epochs the cut removes.
  for (const std::int64_t e : plan.whole) {
    auto s = Stmt::prepare(pt.db, "SELECT start_pt FROM epochs WHERE epoch = ?1 AND reason = 'journey'");
    if (!s) return Unexpected{s.error()};
    s->bind(1, e);
    auto r = s->step();
    if (!r) return Unexpected{r.error()};
    if (*r) {
      ++out.journeys;
      memory_items.push_back(Item{Item::Kind::Journey, std::string(), std::nullopt, s->i64(0)});
    }
  }
  // The newest few of every kind together, newest first.
  out.last.insert(out.last.end(), memory_items.begin(), memory_items.end());
  std::stable_sort(out.last.begin(), out.last.end(), [](const Item& a, const Item& b) { return a.pt > b.pt; });
  if (out.last.size() > proto::db::kSummaryItems) out.last.resize(proto::db::kSummaryItems);
  return out;
}

Result<std::uint64_t> trim(Playthrough& pt, const CutPlan& plan) {
  std::uint64_t rows = 0;
  const auto add = [&](Result<std::uint64_t> r) -> bool {
    if (!r) return false;
    rows += *r;
    return true;
  };
  const std::string cut = number_text(plan.cut);
  // The event log: removed rows cascade to their audience and intents; a reply across the cut keeps its shown pages.
  auto ev = events::cut_events(pt.db, plan);
  if (!add(ev)) return Unexpected{ev.error()};
  // Conversations started after the cut go; one across it ends at its last kept line, trimmed, its counts redone.
  auto conv = run_counting(pt.db, "DELETE FROM conversations WHERE " + plan.removes("started_pt"));
  if (!add(conv)) return Unexpected{conv.error()};
  if (plan.partial) {
    const std::string p = number_text(*plan.partial);
    auto straddling = run_counting(
        pt.db,
        "UPDATE conversations SET end_reason = 'trimmed', "
        "ended_pt = coalesce((SELECT max(coalesce(e.t_play_end_ms, e.t_play_ms)) FROM events e WHERE e.conv_id = "
        "conversations.id AND e.kind IN ('PlayerSaid', 'NpcSaid')), started_pt), "
        "player_lines = (SELECT count(*) FROM events e WHERE e.conv_id = conversations.id AND e.kind = 'PlayerSaid'), "
        "committed_lines = (SELECT count(*) FROM events e WHERE e.conv_id = conversations.id AND e.kind IN "
        "('PlayerSaid', 'NpcSaid')), "
        "last_event_id = (SELECT max(e.id) FROM events e WHERE e.conv_id = conversations.id) "
        "WHERE epoch = " +
            p + " AND started_pt <= " + cut + " AND (ended_pt IS NULL OR ended_pt > " + cut + ")");
    if (!add(straddling)) return Unexpected{straddling.error()};
  }
  // Journal and lived versions written from evidence after the cut, and the rows timed after it.
  for (const char* sql : {"DELETE FROM npc_journal WHERE as_of_pt > ", "DELETE FROM npc_lived WHERE as_of_pt > ",
                          "DELETE FROM decisions WHERE t_play_ms > ", "DELETE FROM memories WHERE t_play_ms > ",
                          "DELETE FROM npc_intents WHERE t_play_ms > "}) {
    auto r = run_counting(pt.db, sql + cut);
    if (!add(r)) return Unexpected{r.error()};
  }
  // Jobs: those of a removed conversation go; a journal job's range comes down to its last kept event, or the job goes
  // with none left; a lived job whose journal version was removed goes; a running job is queued again.
  {
    auto r = run_counting(pt.db,
                          "DELETE FROM memory_jobs WHERE conv_id IS NOT NULL AND conv_id NOT IN "
                          "(SELECT id FROM conversations)");
    if (!r) return Unexpected{r.error()};
  }
  {
    struct Range {
      RowId id = 0;
      std::int64_t from = 0, upto = 0;
    };
    std::vector<Range> ranges;
    auto s = Stmt::prepare(pt.db,
                           "SELECT id, coalesce(from_event_id, 0), upto_event_id FROM memory_jobs WHERE kind = "
                           "'journal' AND upto_event_id IS NOT NULL AND state <> 'done'");
    if (!s) return Unexpected{s.error()};
    for (;;) {
      auto r = s->step();
      if (!r) return Unexpected{r.error()};
      if (!*r) break;
      ranges.push_back(Range{s->i64(0), s->i64(1), s->i64(2)});
    }
    for (const Range& g : ranges) {
      auto last = one_number(pt.db, "SELECT coalesce(max(id), 0) FROM events WHERE id >= " + number_text(g.from) +
                                        " AND id <= " + number_text(g.upto));
      if (!last) return Unexpected{last.error()};
      const std::string sql = *last == 0 ? "DELETE FROM memory_jobs WHERE id = " + number_text(g.id)
                                         : "UPDATE memory_jobs SET upto_event_id = " + number_text(*last) +
                                               " WHERE id = " + number_text(g.id);
      if (auto r = exec(pt.db, sql); !r) return Unexpected{r.error()};
    }
  }
  for (const char* sql :
       {"DELETE FROM memory_jobs WHERE kind = 'lived' AND journal_version > coalesce((SELECT max(v.version) FROM "
        "npc_journal v WHERE v.journey = memory_jobs.journey AND v.persona = memory_jobs.persona), 0)",
        "UPDATE memory_jobs SET state = 'queued' WHERE state = 'running'"}) {
    auto r = run_counting(pt.db, sql);
    if (!r) return Unexpected{r.error()};
  }
  // The watermark is the current version's (MEM §3.7), 0 with none left.
  auto w = run_counting(pt.db,
                        "UPDATE memory_progress SET watermark = coalesce((SELECT v.upto_event_id FROM npc_journal v "
                        "WHERE v.journey = memory_progress.journey AND v.persona = memory_progress.persona ORDER BY "
                        "v.version DESC LIMIT 1), 0)");
  if (!w) return Unexpected{w.error()};
  // The epochs that started after the cut; the current one is the epoch cut into.
  if (!plan.whole.empty()) {
    auto e = run_counting(pt.db, "DELETE FROM epochs WHERE epoch IN (" + id_list(plan.whole) + ")");
    if (!add(e)) return Unexpected{e.error()};
  }
  if (plan.partial) {
    if (auto r = meta_set(pt.db, "current_epoch", number_text(*plan.partial)); !r) return Unexpected{r.error()};
    if (auto r = meta_set(pt.db, "current_journey", number_text(plan.partial_journey)); !r)
      return Unexpected{r.error()};
    pt.epoch = *plan.partial;
    pt.journey = plan.partial_journey;
  } else {
    // The cut lies before every epoch: the timeline starts again at the cut.
    if (auto r = new_epoch(pt, 1, plan.cut, "open"); !r) return Unexpected{r.error()};
  }
  // The facts are a cache of the events' fold: the core refolds them (WHY§689). Event ids are never reused: the
  // counter stays where it was.
  if (auto r = meta_set(pt.db, "facts_stale", "1"); !r) return Unexpected{r.error()};
  return rows;
}

Result<std::vector<RowId>> recover(Playthrough& pt) {
  std::vector<RowId> closed;
  struct Open {
    RowId id = 0;
    std::uint16_t journey = 1;
    std::string persona;
    Pt started = 0;
  };
  std::vector<Open> open;
  {
    auto s = Stmt::prepare(pt.db, "SELECT id, journey, persona, started_pt FROM conversations WHERE ended_pt IS NULL");
    if (!s) return Unexpected{s.error()};
    for (;;) {
      auto r = s->step();
      if (!r) return Unexpected{r.error()};
      if (!*r) break;
      open.push_back(Open{s->i64(0), static_cast<std::uint16_t>(s->i64(1)), s->text(2), s->i64(3)});
    }
  }
  for (const Open& c : open) {
    // 1. Closed at its last committed line, as crashed, its counts recomputed from its events.
    const std::string id = number_text(c.id);
    auto last = one_number(
        pt.db, "SELECT coalesce(max(coalesce(t_play_end_ms, t_play_ms)), 0) FROM events WHERE conv_id = " + id +
                   " AND kind IN ('PlayerSaid', 'NpcSaid')");
    if (!last) return Unexpected{last.error()};
    auto player = one_number(pt.db, "SELECT count(*) FROM events WHERE conv_id = " + id + " AND kind = 'PlayerSaid'");
    if (!player) return Unexpected{player.error()};
    auto committed = one_number(
        pt.db, "SELECT count(*) FROM events WHERE conv_id = " + id + " AND kind IN ('PlayerSaid', 'NpcSaid')");
    if (!committed) return Unexpected{committed.error()};
    const Pt ended = std::max(*last, c.started);
    auto u = Stmt::prepare(pt.db,
                           "UPDATE conversations SET ended_pt = ?2, end_reason = 'crash', player_lines = ?3, "
                           "committed_lines = ?4, last_event_id = (SELECT max(id) FROM events WHERE conv_id = ?1) "
                           "WHERE id = ?1");
    if (!u) return Unexpected{u.error()};
    u->bind(1, c.id).bind(2, ended).bind(3, *player).bind(4, *committed);
    if (auto r = u->run(); !r) return Unexpected{r.error()};
    // 2. Its ConversationEnded, the persona's own, so the facts fold it as any other end.
    proto::StoredEvent e;
    e.id = pt.next_event_id;
    e.journey = c.journey;
    e.t_play_ms = ended;
    e.kind = "ConversationEnded";
    e.source = "store";
    e.subject = "persona:" + c.persona;
    e.conv_id = c.id;
    e.payload = "{\"reason\":\"crash\",\"player_lines\":" + number_text(*player) +
                ",\"committed\":" + number_text(*committed) + ",\"known\":null}";
    if (auto r = events::insert_own(pt, std::move(e), c.persona); !r) return Unexpected{r.error()};
    closed.push_back(c.id);
  }
  if (!closed.empty()) {
    if (auto r = meta_set(pt.db, "facts_stale", "1"); !r) return Unexpected{r.error()};
  }
  // 4. A job a crash left running is queued again, its attempts unchanged.
  if (auto r = exec(pt.db, "UPDATE memory_jobs SET state = 'queued' WHERE state = 'running'"); !r) {
    return Unexpected{r.error()};
  }
  return closed;
}

Result<std::uint64_t> forget(Playthrough& pt, std::uint16_t journey, const std::string& persona, Pt at_pt) {
  std::uint64_t rows = 0;
  const std::string j = number_text(journey);
  if (persona.empty()) {
    // The whole journey: every row with it, in every table (events cascade to their audience and intents); the
    // epochs stay, for the rollback's arithmetic.
    for (const char* table : {"events", "conversations", "npc_facts", "npc_journal", "npc_lived", "memory_progress",
                              "memory_jobs", "npc_intents", "decisions", "memories"}) {
      auto r = run_counting(pt.db, std::string("DELETE FROM ") + table + " WHERE journey = " + j);
      if (!r) return Unexpected{r.error()};
      rows += *r;
    }
  } else {
    for (const char* table :
         {"npc_journal", "npc_lived", "npc_facts", "memory_progress", "memory_jobs", "conversations", "npc_intents"}) {
      auto s = Stmt::prepare(pt.db, std::string("DELETE FROM ") + table + " WHERE journey = ?1 AND persona = ?2");
      if (!s) return Unexpected{s.error()};
      s->bind(1, std::int64_t{journey}).bind(2, std::string_view(persona));
      if (auto r = s->run(); !r) return Unexpected{r.error()};
      rows += static_cast<std::uint64_t>(pt.db.changes());
    }
    auto ev = events::forget_persona(pt.db, journey, persona);
    if (!ev) return Unexpected{ev.error()};
    rows += *ev;
  }
  // The act itself, without content.
  auto f = Stmt::prepare(
      pt.db, "INSERT INTO forgotten (journey, persona, at_pt, rows_removed, wall) VALUES (?1, ?2, ?3, ?4, ?5)");
  if (!f) return Unexpected{f.error()};
  f->bind(1, std::int64_t{journey});
  if (persona.empty()) {
    f->bind_null(2);
  } else {
    f->bind(2, std::string_view(persona));
  }
  f->bind(3, at_pt).bind(4, static_cast<std::int64_t>(rows)).bind(5, pt.wall);
  if (auto r = f->run(); !r) return Unexpected{r.error()};
  return rows;
}

}  // namespace en::store
