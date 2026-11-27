// The event log's SQL (playthrough.hpp's `events`; MEM §1, §8.7). The only file of en-store, and of EldenNet, whose
// SQL names `event_audience` (MEM §1.3's grep, en_augur_audience_grep): an audience is written once, frozen, and read
// back only as the asking persona's own row, or whole for the refold.
#include <algorithm>
#include <map>

#include "en/store/playthrough.hpp"

namespace en::store::events {

namespace {

using proto::EventId;
using proto::StoredEvent;

constexpr std::string_view kColumns =
    "e.id, e.journey, e.epoch, e.t_play_ms, e.t_play_end_ms, e.kind, e.rule, e.source, e.conf, e.subject, e.object, "
    "e.conv_id, e.place_id, e.region, e.block, e.tod, e.dkey, e.text, e.payload";

StoredEvent read_event(const Stmt& s) {
  StoredEvent e;
  e.id = s.i64(0);
  e.journey = static_cast<std::uint16_t>(s.i64(1));
  e.epoch = s.i64(2);
  e.t_play_ms = s.i64(3);
  e.t_play_end_ms = s.opt_i64(4);
  e.kind = s.text(5);
  e.rule = s.text(6);
  e.source = s.text(7);
  e.conf = s.f64(8);
  e.subject = s.text(9);
  e.object = s.text(10);
  e.conv_id = s.opt_i64(11);
  e.place_id = s.opt_i64(12);
  e.region = s.text(13);
  if (const auto b = s.opt_i64(14)) e.block = static_cast<std::uint32_t>(*b);
  e.tod = s.text(15);
  e.dkey = s.text(16);
  e.text = s.opt_text(17);
  e.payload = s.text(18);
  return e;
}

void bind_text_or_null(Stmt& s, int i, const std::string& v) {
  if (v.empty()) {
    s.bind_null(i);
  } else {
    s.bind(i, std::string_view(v));
  }
}

}  // namespace

Result<void> insert(Playthrough& pt, const StoredEvent& ev) {
  if (ev.kind.empty()) return Unexpected{Fault::Invalid};
  auto s = Stmt::prepare(pt.db,
                         "INSERT INTO events (id, journey, epoch, t_play_ms, t_play_end_ms, kind, rule, source, conf, "
                         "subject, object, conv_id, place_id, region, block, tod, dkey, text, payload) VALUES "
                         "(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19)");
  if (!s) return Unexpected{s.error()};
  s->bind(1, ev.id).bind(2, std::int64_t{ev.journey}).bind(3, pt.epoch).bind(4, ev.t_play_ms).bind(5, ev.t_play_end_ms);
  s->bind(6, std::string_view(ev.kind));
  bind_text_or_null(*s, 7, ev.rule);
  bind_text_or_null(*s, 8, ev.source);
  s->bind(9, ev.conf);
  bind_text_or_null(*s, 10, ev.subject);
  bind_text_or_null(*s, 11, ev.object);
  s->bind(12, ev.conv_id).bind(13, ev.place_id);
  bind_text_or_null(*s, 14, ev.region);
  if (ev.block) {
    s->bind(15, std::int64_t{*ev.block});
  } else {
    s->bind_null(15);
  }
  bind_text_or_null(*s, 16, ev.tod);
  bind_text_or_null(*s, 17, ev.dkey);
  s->bind(18, ev.text);
  s->bind(19, std::string_view(ev.payload.empty() ? std::string_view("{}") : std::string_view(ev.payload)));
  if (auto r = s->run(); !r) return r;
  // An NpcSaid's first page: its end and pt, unless the core gave its pages already.
  if (ev.kind == "NpcSaid" && ev.text) {
    auto p = Stmt::prepare(pt.db,
                           "UPDATE events SET payload = json_set(payload, '$.pages', "
                           "json_array(json_object('end', length(text), 'pt', t_play_ms))) "
                           "WHERE id = ?1 AND json_type(payload, '$.pages') IS NULL");
    if (!p) return Unexpected{p.error()};
    p->bind(1, ev.id);
    if (auto r = p->run(); !r) return r;
  }
  // The frozen audience: one row per persona, as Augur resolved it.
  if (!ev.audience.empty()) {
    auto a =
        Stmt::prepare(pt.db, "INSERT INTO event_audience (event_id, persona, journey, how) VALUES (?1, ?2, ?3, ?4)");
    if (!a) return Unexpected{a.error()};
    for (const proto::AudienceRow& row : ev.audience) {
      if (row.persona.empty()) return Unexpected{Fault::Invalid};
      a->reset();
      a->bind(1, ev.id).bind(2, std::string_view(row.persona)).bind(3, std::int64_t{ev.journey});
      a->bind(4, std::string_view(proto::to_string(row.how)));
      if (auto r = a->run(); !r) return r;
    }
  }
  if (ev.id >= pt.next_event_id) {
    pt.next_event_id = ev.id + 1;
    if (auto r = meta_set(pt.db, "next_event_id", number_text(pt.next_event_id)); !r) return r;
  }
  return {};
}

Result<void> add_page(Playthrough& pt, const proto::db::AppendPage& p) {
  auto s =
      Stmt::prepare(pt.db,
                    "UPDATE events SET "
                    "text = CASE WHEN text IS NULL OR text = '' THEN ?2 ELSE text || ' ' || ?2 END, "
                    "t_play_end_ms = ?3, "
                    "payload = json_set(payload, '$.pages', json_insert(coalesce(json_extract(payload, '$.pages'), "
                    "json('[]')), '$[#]', json_object('end', length(CASE WHEN text IS NULL OR text = '' THEN ?2 "
                    "ELSE text || ' ' || ?2 END), 'pt', ?3))) "
                    "WHERE id = ?1 AND kind = 'NpcSaid'");
  if (!s) return Unexpected{s.error()};
  s->bind(1, p.event).bind(2, std::string_view(p.text)).bind(3, p.page_pt);
  if (auto r = s->run(); !r) return r;
  if (pt.db.changes() != 1) return Unexpected{Fault::Invalid};  // no such NpcSaid
  return {};
}

Result<std::vector<StoredEvent>> seen_by(const Db& db, std::uint16_t journey, const std::string& persona, EventId after,
                                         std::optional<EventId> upto) {
  auto s = Stmt::prepare(db, std::string("SELECT ") + std::string(kColumns) +
                                 ", a.how FROM event_audience a JOIN events e ON e.id = a.event_id "
                                 "WHERE a.persona = ?1 AND a.journey = ?2 AND e.journey = ?2 AND e.id > ?3 "
                                 "AND (?4 IS NULL OR e.id <= ?4) ORDER BY e.id");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::string_view(persona)).bind(2, std::int64_t{journey}).bind(3, after).bind(4, upto);
  std::vector<StoredEvent> out;
  for (;;) {
    auto r = s->step();
    if (!r) return Unexpected{r.error()};
    if (!*r) break;
    StoredEvent e = read_event(*s);
    e.audience.push_back(proto::AudienceRow{persona, proto::how_from(s->text(19)).value_or(proto::How::Witnessed)});
    out.push_back(std::move(e));
  }
  return out;
}

Result<std::uint32_t> count_seen_by(const Db& db, std::uint16_t journey, const std::string& persona, EventId after) {
  auto s =
      Stmt::prepare(db, "SELECT count(*) FROM event_audience WHERE persona = ?1 AND journey = ?2 AND event_id > ?3");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::string_view(persona)).bind(2, std::int64_t{journey}).bind(3, after);
  auto r = s->step();
  if (!r) return Unexpected{r.error()};
  return *r ? static_cast<std::uint32_t>(s->i64(0)) : 0u;
}

Result<std::vector<StoredEvent>> tail(const Db& db, std::uint16_t journey, const std::string& persona,
                                      proto::RowId conv, std::size_t n) {
  auto s = Stmt::prepare(db, std::string("SELECT ") + std::string(kColumns) +
                                 ", a.how FROM event_audience a JOIN events e ON e.id = a.event_id "
                                 "WHERE a.persona = ?1 AND a.journey = ?2 AND a.how = 'direct' AND e.conv_id = ?3 "
                                 "AND e.kind IN ('PlayerSaid', 'NpcSaid') ORDER BY e.id DESC LIMIT ?4");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::string_view(persona)).bind(2, std::int64_t{journey}).bind(3, conv);
  s->bind(4, static_cast<std::int64_t>(n));
  std::vector<StoredEvent> out;
  for (;;) {
    auto r = s->step();
    if (!r) return Unexpected{r.error()};
    if (!*r) break;
    StoredEvent e = read_event(*s);
    e.audience.push_back(proto::AudienceRow{persona, proto::How::Direct});
    out.push_back(std::move(e));
  }
  std::reverse(out.begin(), out.end());
  return out;
}

Result<std::vector<StoredEvent>> all_of(const Db& db, std::uint16_t journey) {
  auto s = Stmt::prepare(
      db, std::string("SELECT ") + std::string(kColumns) + " FROM events e WHERE e.journey = ?1 ORDER BY e.id");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::int64_t{journey});
  std::vector<StoredEvent> out;
  std::map<EventId, std::size_t> at;
  for (;;) {
    auto r = s->step();
    if (!r) return Unexpected{r.error()};
    if (!*r) break;
    at[s->i64(0)] = out.size();
    out.push_back(read_event(*s));
  }
  // Every audience row (by persona within an event: event_audience is WITHOUT ROWID and keeps no insertion order).
  auto a = Stmt::prepare(db,
                         "SELECT event_id, persona, how FROM event_audience WHERE journey = ?1 "
                         "ORDER BY event_id, persona");
  if (!a) return Unexpected{a.error()};
  a->bind(1, std::int64_t{journey});
  for (;;) {
    auto r = a->step();
    if (!r) return Unexpected{r.error()};
    if (!*r) break;
    const auto it = at.find(a->i64(0));
    if (it == at.end()) continue;
    out[it->second].audience.push_back(
        proto::AudienceRow{a->text(1), proto::how_from(a->text(2)).value_or(proto::How::Witnessed)});
  }
  return out;
}

}  // namespace en::store::events
