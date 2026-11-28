// The store's core through its requests (store.hpp; MEM §3.8, §8.3, §9; O144): opening a file and its start backup,
// the timeline's check (Clean, a silent trim, the choice and its three answers), what the choice's summary holds, crash
// recovery, a trim's every table, forgetting, NG+, a newer file and a corrupt one. Invented rows and keys throughout.
#include <doctest.h>

#include <map>
#include <string>
#include <vector>

#include "en/store/store.hpp"
#include "support.hpp"

using namespace en::store;
using namespace en::store::test;
namespace proto = en::proto;
namespace db = en::proto::db;
using proto::How;
namespace fs = std::filesystem;

namespace {

constexpr proto::Pt kMin = 60000;
constexpr proto::Pt kHour = 60 * kMin;

proto::PlaythroughKey the_key() {
  proto::PlaythroughKey k;
  k.steamid64 = 76561190000000001ull;
  k.slot = 3;
  k.fp8 = "0a1b2c3d";
  k.name_hash = "hash-of-an-invented-name";
  k.archetype = 5;
  k.gift = 2;
  return k;
}

struct Rig {
  TempDir dir;
  std::int64_t now_wall = 1764201600;
  StoreCfg config() {
    StoreCfg c;
    c.saves = dir.path() / "saves";
    c.build = "test-build";
    c.wall = [this] { return now_wall; };
    c.mono_ms = [] { return 0ull; };
    c.free_bytes = [](const fs::path&) -> std::optional<std::uint64_t> { return 10ull << 30; };
    return c;
  }
  StoreCore store{config()};
  std::uint64_t seq = 0;
  proto::EventId next = 0;
  proto::RowId next_conv = 0;

  proto::DbResult send(proto::DbRequest::Op op) {
    proto::DbRequest r;
    r.seq = ++seq;
    r.op = std::move(op);
    proto::DbResult out = store.handle(r);
    CHECK(out.seq == r.seq);
    return out;
  }
  db::Opened open_at(proto::Pt pt, std::uint16_t journey = 1) {
    auto r = send(db::Open{the_key(), pt, journey});
    const auto* o = std::get_if<db::Opened>(&r.result);
    REQUIRE(o != nullptr);
    next = o->next_event_id;
    next_conv = o->next_conv_id;
    return *o;
  }
  void ok(proto::DbResult r) {
    if (const auto* f = std::get_if<db::Failed>(&r.result)) FAIL("failed: " << db::to_string(f->fault));
  }
  // An event of the journey at `t`, with an audience.
  proto::EventId put(std::string kind, proto::Pt t, std::vector<proto::AudienceRow> aud = {},
                     std::uint16_t journey = 1) {
    db::Append a;
    a.event = event(next, std::move(kind), t, std::move(aud), journey);
    const proto::EventId id = next++;
    ok(send(a));
    return id;
  }
  proto::RowId conversation(std::string persona, proto::Pt t) {
    proto::ConversationRow c;
    c.id = next_conv++;
    c.persona = std::move(persona);
    c.stage = "s";
    c.started_pt = t;
    c.persona_hash = "ph";
    c.kb_hash = "kh";
    ok(send(db::ConvOpen{c}));
    return c.id;
  }
  proto::EventId line(proto::RowId conv, std::string kind, proto::Pt t, std::string text, std::string persona) {
    db::Append a;
    a.event = event(next, kind, t, {{persona, How::Direct}});
    a.event.conv_id = conv;
    a.event.text = std::move(text);
    a.event.subject = kind == "PlayerSaid" ? "player" : "persona:" + persona;
    a.event.object = kind == "PlayerSaid" ? "persona:" + persona : "player";
    const proto::EventId id = next++;
    ok(send(a));
    return id;
  }
  std::int64_t count(const std::string& table) const { return test::count(store.playthrough()->db, table); }
  std::string value(const std::string& sql) const { return test::value(store.playthrough()->db, sql); }
};

const db::Opened& opened_of(const proto::DbResult& r) {
  if (const auto* o = std::get_if<db::Opened>(&r.result)) return *o;
  return std::get<db::Resolved>(r.result).opened;
}

}  // namespace

TEST_CASE("store: a new file is made and opens clean; reopened, its start backup is taken") {
  Rig rig;
  const db::Opened first = rig.open_at(10 * kMin);
  CHECK(first.created);
  CHECK(first.rollback.kind == db::Rollback::Kind::Clean);
  CHECK(first.journey == 1);
  CHECK(first.next_event_id == 1);
  CHECK(first.next_conv_id == 1);
  CHECK_FALSE(first.low_disk);
  CHECK(rig.value("SELECT v FROM meta WHERE k = 'created_by'") == "test-build");
  CHECK(rig.value("SELECT v FROM meta WHERE k = 'name_hash'") == "hash-of-an-invented-name");
  CHECK(rig.value("SELECT reason || ':' || start_pt FROM epochs") == "open:600000");
  rig.put("PlaceEntered", 11 * kMin);
  rig.ok(rig.send(db::Close{}));
  const db::Opened again = rig.open_at(12 * kMin);
  CHECK_FALSE(again.created);
  CHECK(again.rollback.kind == db::Rollback::Kind::Clean);
  CHECK(again.next_event_id == 2);
  const SaveFolder f = save_folder(rig.dir.path() / "saves", the_key().steamid64);
  CHECK(fs::exists(f.backups() / "pt-s3-0a1b2c3d.1.sqlite"));
  // Nothing open: every write and query is refused.
  rig.ok(rig.send(db::Close{}));
  const auto refused = rig.send(db::Bundle{1, "kale"});
  REQUIRE(std::holds_alternative<db::Failed>(refused.result));
  CHECK(std::get<db::Failed>(refused.result).fault == Fault::NotOpen);
}

TEST_CASE("store: a gap of at most 5 min is trimmed silently at the save's play time; ids are never reused") {
  Rig rig;
  rig.open_at(kHour);
  rig.put("PlaceEntered", kHour + kMin);
  const proto::EventId late = rig.put("PersonaSeen", kHour + 4 * kMin, {{"kale", How::Direct}});
  rig.ok(rig.send(db::Close{}));
  const db::Opened o = rig.open_at(kHour + 2 * kMin);  // the file is 2 min ahead: a crash after the last autosave
  CHECK(o.rollback.kind == db::Rollback::Kind::Trimmed);
  CHECK(o.rollback.cut == kHour + 2 * kMin);
  CHECK(o.rollback.rows >= 1);
  CHECK(o.facts_stale);
  CHECK(rig.count("events") == 1);
  CHECK(o.next_event_id == late + 1);  // the counter stays: the trimmed id is never handed out again
}

TEST_CASE("store (O144): an hour's gap holding only unwitnessed events is trimmed silently") {
  Rig rig;
  rig.open_at(kHour);
  for (int i = 1; i <= 5; ++i) rig.put("PlaceEntered", kHour + i * 10 * kMin);  // no audience: no one remembers it
  rig.ok(rig.send(db::Close{}));
  const db::Opened o = rig.open_at(kHour);
  CHECK(o.rollback.kind == db::Rollback::Kind::Trimmed);
  CHECK(o.rollback.delta_ms == 50 * kMin);
  CHECK_FALSE(o.rollback.summary.matters());
  CHECK(rig.count("events") == 0);
}

TEST_CASE("store (O144): an hour's gap holding one witnessed event asks, with the summary for the panel") {
  Rig rig;
  rig.open_at(kHour);
  rig.put("PlaceEntered", kHour + 5 * kMin);
  db::Append hit;
  hit.event = event(rig.next++, "PersonaAttacked", kHour + 30 * kMin, {{"lantern_keeper", How::Witnessed}});
  hit.event.subject = "persona:kale";
  hit.event.place_id = 62001;
  rig.ok(rig.send(hit));
  rig.ok(rig.send(db::Close{}));
  const db::Opened o = rig.open_at(kHour);
  REQUIRE(o.rollback.kind == db::Rollback::Kind::Choice);
  const db::RollbackSummary& s = o.rollback.summary;
  CHECK(s.matters());
  CHECK(s.witnessed == 1);
  CHECK(s.turns + s.addressed + s.memories + s.stages + s.journeys == 0);
  CHECK(s.save_pt == kHour);
  CHECK(s.file_pt == kHour + 30 * kMin);
  CHECK(s.save_journey == 1);
  CHECK(s.file_journey == 1);
  REQUIRE(s.last.size() == 1);
  CHECK(s.last[0] == db::RollbackItem{db::RollbackItem::Kind::Witnessed, "kale", 62001, kHour + 30 * kMin});
  // While it waits, nothing is written or read but the answer.
  const auto w = rig.send(db::Append{event(rig.next, "PlaceEntered", kHour + kMin), {}});
  REQUIRE(std::holds_alternative<db::Failed>(w.result));
  CHECK(std::get<db::Failed>(w.result).fault == Fault::NotOpen);
  CHECK(rig.store.awaiting_choice());
  // Forget: the trim, then recovery.
  const auto r = rig.send(db::Resolve{db::Choice::Forget});
  const db::Opened& done = opened_of(r);
  CHECK(done.rollback.kind == db::Rollback::Kind::Trimmed);
  CHECK(rig.count("events") == 0);
  CHECK_FALSE(rig.store.awaiting_choice());
}

TEST_CASE("store: the summary counts each kind that matters, and names the stages a rollback undoes") {
  Rig rig;
  rig.open_at(kHour);
  const proto::RowId c = rig.conversation("kale", kHour + kMin);
  rig.line(c, "PlayerSaid", kHour + kMin + 1000, "Evening.", "kale");
  rig.line(c, "NpcSaid", kHour + kMin + 2000, "Evening.", "kale");
  db::Append unasked;  // a line addressed to the Tarnished outside a conversation
  unasked.event = event(rig.next++, "NpcSaid", kHour + 20 * kMin, {{"kale", How::Direct}});
  unasked.event.subject = "persona:kale";
  unasked.event.object = "player";
  unasked.event.text = "Over here.";
  rig.ok(rig.send(unasked));
  db::Append stage;
  stage.event = event(rig.next++, "StageChanged", kHour + 25 * kMin, {{"kale", How::Direct}});
  stage.event.subject = "persona:kale";
  stage.event.payload = "{\"from\":\"at_the_church\",\"to\":\"howl_told\"}";
  rig.ok(rig.send(stage));
  db::CommitJournal j;
  j.version.persona = "kale";
  j.version.version = 1;
  j.version.as_of_pt = kHour + 26 * kMin;
  j.version.upto_event_id = rig.next - 1;
  j.version.keepsakes = "[{\"id\":\"k1\",\"basis\":\"said\"},{\"id\":\"k2\",\"basis\":\"seen\"}]";
  j.version.model_fp = j.version.prompt_hash = j.version.persona_hash = "x";
  rig.ok(rig.send(j));
  rig.ok(rig.send(db::Close{}));
  const db::Opened o = rig.open_at(kHour);
  REQUIRE(o.rollback.kind == db::Rollback::Kind::Choice);
  const db::RollbackSummary& s = o.rollback.summary;
  CHECK(s.turns == 2);
  CHECK(s.addressed == 1);
  CHECK(s.stages == 1);
  CHECK(s.memories == 1);
  CHECK(s.claims == 1);
  CHECK(s.witnessed == 0);
  REQUIRE(s.stages_undone.size() == 1);
  CHECK(s.stages_undone[0] == db::StageUndone{"kale", "at_the_church", "howl_told", kHour + 25 * kMin});
  REQUIRE(s.last.size() == 5);
  CHECK(s.last[0].kind == db::RollbackItem::Kind::Memory);  // newest first
  CHECK(s.last[1].kind == db::RollbackItem::Kind::Stage);
  CHECK(s.last[2].kind == db::RollbackItem::Kind::Addressed);
  for (const auto& item : s.last) CHECK(item.who == "kale");
}

TEST_CASE("store: Keep keeps every row in a new epoch, and a later crash trim touches only that epoch") {
  Rig rig;
  rig.open_at(kHour);
  rig.put("PersonaSeen", kHour + 30 * kMin, {{"kale", How::Direct}});
  rig.ok(rig.send(db::Close{}));
  REQUIRE(rig.open_at(kHour).rollback.kind == db::Rollback::Kind::Choice);
  const db::Opened kept = opened_of(rig.send(db::Resolve{db::Choice::Keep}));
  CHECK(kept.rollback.kind == db::Rollback::Kind::Clean);
  CHECK(rig.count("events") == 1);
  CHECK(rig.value("SELECT group_concat(reason) FROM epochs") == "open,keep");
  // In the kept epoch: one event 2 min on, and the save loaded 1 min on: a silent trim of the new epoch alone.
  rig.next = kept.next_event_id;
  rig.put("PersonaSeen", kHour + 2 * kMin, {{"kale", How::Direct}});
  rig.ok(rig.send(db::Close{}));
  const db::Opened later = rig.open_at(kHour + kMin);
  CHECK(later.rollback.kind == db::Rollback::Kind::Trimmed);
  CHECK(rig.count("events") == 1);  // the kept "future" row of the old epoch stays
  CHECK(rig.value("SELECT t_play_ms FROM events") == number_text(kHour + 30 * kMin));
}

TEST_CASE("store: This is a new character archives the old file, never deleting it, and starts a new one") {
  Rig rig;
  rig.open_at(kHour);
  rig.put("PersonaSeen", kHour + 30 * kMin, {{"kale", How::Direct}});
  rig.ok(rig.send(db::Close{}));
  REQUIRE(rig.open_at(10 * kMin).rollback.kind == db::Rollback::Kind::Choice);
  const db::Opened fresh = opened_of(rig.send(db::Resolve{db::Choice::NewCharacter}));
  CHECK(fresh.created);
  CHECK(rig.count("events") == 0);
  const SaveFolder f = save_folder(rig.dir.path() / "saves", the_key().steamid64);
  std::size_t archived = 0;
  for (const auto& e : fs::directory_iterator(f.archive())) archived += e.path().extension() == ".sqlite" ? 1 : 0;
  CHECK(archived == 1);
}

TEST_CASE("store: a crash leaves a conversation open: at the next open it ends as crashed, for its persona") {
  Rig rig;
  rig.open_at(kHour);
  const proto::RowId c = rig.conversation("kale", kHour + kMin);
  rig.line(c, "PlayerSaid", kHour + kMin + 1000, "Evening.", "kale");
  const proto::EventId reply = rig.line(c, "NpcSaid", kHour + kMin + 3000, "Evening to you.", "kale");
  proto::MemoryJob job;
  job.persona = "kale";
  job.state = proto::JobState::Running;
  job.created_pt = kHour;
  rig.ok(rig.send(db::UpsertJob{job}));
  rig.ok(rig.send(db::Close{}));  // the conversation was never closed: a hard kill
  const db::Opened o = rig.open_at(kHour + 2 * kMin);
  CHECK(o.rollback.kind == db::Rollback::Kind::Clean);
  REQUIRE(o.closed_conversations == std::vector<proto::RowId>{c});
  CHECK(rig.value("SELECT end_reason || ':' || ended_pt || ':' || player_lines || ':' || committed_lines "
                  "FROM conversations") == "crash:" + number_text(kHour + kMin + 3000) + ":1:2");
  CHECK(rig.value("SELECT json_extract(payload, '$.reason') FROM events WHERE kind = 'ConversationEnded'") == "crash");
  CHECK(rig.value("SELECT id FROM events WHERE kind = 'ConversationEnded'") == number_text(reply + 1));
  auto b = rig.send(db::Bundle{1, "kale"});
  REQUIRE(std::holds_alternative<db::BundleResult>(b.result));
  CHECK(std::get<db::BundleResult>(b.result).prev_conv == c);
  REQUIRE(o.open_jobs.size() == 1);
  CHECK(o.open_jobs[0].state == proto::JobState::Queued);  // a running job is queued again
  CHECK(o.facts_stale);
  CHECK(o.next_event_id == reply + 2);
}

TEST_CASE("store: a trim keeps a reply's pages shown before the cut, and ends a conversation across it") {
  Rig rig;
  rig.open_at(kHour);
  const proto::RowId c = rig.conversation("kale", kHour + 10 * 1000);
  rig.line(c, "PlayerSaid", kHour + 11 * 1000, "What do you sell?", "kale");
  const proto::EventId reply = rig.line(c, "NpcSaid", kHour + 12 * 1000, "Oil.", "kale");
  rig.ok(rig.send(db::AppendPage{reply, "Rope.", kHour + 15 * 1000}));
  rig.ok(rig.send(db::AppendPage{reply, "Bread.", kHour + 19 * 1000}));
  rig.conversation("kale", kHour + 60 * 1000);  // a later conversation, after the cut
  rig.ok(rig.send(db::Close{}));
  const db::Opened o = rig.open_at(kHour + 17 * 1000);  // between the second and third page
  REQUIRE(o.rollback.kind == db::Rollback::Kind::Trimmed);
  CHECK(rig.value("SELECT text FROM events WHERE id = " + number_text(reply)) == "Oil. Rope.");
  CHECK(rig.value("SELECT t_play_end_ms FROM events WHERE id = " + number_text(reply)) ==
        number_text(kHour + 15 * 1000));
  CHECK(rig.value("SELECT json_array_length(payload, '$.pages') FROM events WHERE id = " + number_text(reply)) == "2");
  CHECK(rig.count("conversations") == 1);
  CHECK(rig.value("SELECT end_reason || ':' || ended_pt || ':' || committed_lines FROM conversations") ==
        "trimmed:" + number_text(kHour + 15 * 1000) + ":2");
}

TEST_CASE("store: a trim takes journal and lived versions after the cut, lowers jobs, and the watermark follows") {
  Rig rig;
  rig.open_at(kHour);
  const proto::EventId a = rig.put("PersonaSeen", kHour + kMin, {{"kale", How::Direct}});
  const proto::EventId b = rig.put("PersonaAttacked", kHour + 3 * kMin, {{"kale", How::Direct}});
  for (std::uint32_t v = 1; v <= 2; ++v) {
    db::CommitJournal j;
    j.version.persona = "kale";
    j.version.version = v;
    j.version.as_of_pt = v == 1 ? kHour + kMin : kHour + 3 * kMin;
    j.version.upto_event_id = v == 1 ? a : b;
    j.version.model_fp = j.version.prompt_hash = j.version.persona_hash = "x";
    rig.ok(rig.send(j));
  }
  db::CommitLived l;
  l.version.persona = "kale";
  l.version.version = 1;
  l.version.journal_version = 2;
  l.version.as_of_pt = kHour + 3 * kMin;
  l.version.model_fp = l.version.prompt_hash = l.version.persona_hash = "x";
  rig.ok(rig.send(l));
  proto::MemoryJob job;
  job.persona = "kale";
  job.from_event_id = a;
  job.upto_event_id = b;
  job.created_pt = kHour + 3 * kMin;
  rig.ok(rig.send(db::UpsertJob{job}));
  rig.ok(rig.send(db::Close{}));
  const db::Opened o = rig.open_at(kHour + 2 * kMin);
  REQUIRE(o.rollback.kind == db::Rollback::Kind::Trimmed);
  CHECK(rig.value("SELECT group_concat(version) FROM npc_journal") == "1");
  CHECK(rig.count("npc_lived") == 0);
  CHECK(rig.value("SELECT watermark FROM memory_progress WHERE persona = 'kale'") == number_text(a));
  REQUIRE(o.open_jobs.size() == 1);
  CHECK(o.open_jobs[0].upto_event_id == a);  // lowered to its last kept event
}

TEST_CASE("store: forgetting one NPC keeps what another perceived; forgetting a journey keeps its epochs") {
  Rig rig;
  rig.open_at(kHour);
  const proto::RowId c = rig.conversation("kale", kHour + kMin);
  db::Append said;
  said.event =
      event(rig.next++, "PlayerSaid", kHour + kMin + 1000, {{"kale", How::Direct}, {"lantern_keeper", How::Overheard}});
  said.event.conv_id = c;
  said.event.text = "Evening.";
  said.event.subject = "player";
  rig.ok(rig.send(said));
  db::Append seen;
  seen.event = event(rig.next++, "PersonaSeen", kHour + kMin, {{"kale", How::Direct}});
  seen.event.subject = "persona:kale";
  rig.ok(rig.send(seen));
  db::Append fact;
  fact.event = event(rig.next++, "PersonaAttacked", kHour + 2 * kMin, {{"kale", How::Direct}});
  fact.event.subject = "persona:kale";
  fact.facts = {proto::PersonaFacts{"kale", {proto::FactRow{"times_attacked", "1", {}, kHour}}}};
  rig.ok(rig.send(fact));
  rig.ok(rig.send(db::Forget{1, "kale", kHour + 3 * kMin}));
  CHECK(rig.count("conversations") == 0);
  CHECK(rig.count("npc_facts") == 0);
  CHECK(rig.value("SELECT count(*) FROM event_audience WHERE persona = 'kale'") == "0");
  CHECK(rig.count("events") == 1);  // the line the Lantern-Keeper overheard: for it, it still happened
  CHECK(rig.value("SELECT persona FROM event_audience") == "lantern_keeper");
  CHECK(rig.value("SELECT persona || ':' || journey FROM forgotten") == "kale:1");
  const SaveFolder f = save_folder(rig.dir.path() / "saves", the_key().steamid64);
  CHECK(fs::exists(f.backups() / "pt-s3-0a1b2c3d.1.sqlite"));  // the undo
  rig.ok(rig.send(db::Forget{1, "", kHour + 4 * kMin}));
  CHECK(rig.count("events") == 0);
  CHECK(rig.count("epochs") == 1);
  CHECK(rig.value("SELECT count(*) FROM forgotten WHERE persona IS NULL") == "1");
}

TEST_CASE("store: NG+ starts a journey's epoch; a save from the earlier journey asks, and Forget takes it back") {
  Rig rig;
  rig.open_at(kHour);
  rig.put("PersonaSeen", kHour + kMin, {{"kale", How::Direct}});
  rig.ok(rig.send(db::Close{}));
  const db::Opened ng = rig.open_at(40 * kHour, 2);  // a journey the file has not seen: its epoch
  CHECK(ng.journey == 2);
  CHECK(rig.value("SELECT group_concat(reason) FROM epochs") == "open,journey");
  rig.put("PlaceEntered", 40 * kHour + kMin, {}, 2);
  rig.ok(rig.send(db::Close{}));
  // A journey-1 save loaded later in play time: the file's later journey matters, so the player is asked.
  const db::Opened back = rig.open_at(41 * kHour, 1);
  REQUIRE(back.rollback.kind == db::Rollback::Kind::Choice);
  CHECK(back.rollback.lower_journey);
  CHECK(back.rollback.summary.journeys == 1);
  CHECK(back.rollback.cut == 40 * kHour - 1);  // cut at the later journey's start at the latest
  const db::Opened forgot = opened_of(rig.send(db::Resolve{db::Choice::Forget}));
  CHECK(forgot.journey == 1);
  CHECK(rig.value("SELECT group_concat(reason) FROM epochs") == "open");
  CHECK(rig.value("SELECT count(*) FROM events WHERE journey = 1") == "1");  // journey 1 is as it was
}

TEST_CASE(
    "store: a file newer than this build may read is refused, memory off; a corrupt one gives way to its backup") {
  Rig rig;
  rig.open_at(kHour);
  rig.put("PlaceEntered", kHour + kMin);
  rig.ok(rig.send(db::Close{}));
  rig.open_at(kHour + 2 * kMin);  // the start backup now holds the event
  rig.ok(rig.send(db::Close{}));
  const SaveFolder f = save_folder(rig.dir.path() / "saves", the_key().steamid64);
  const fs::path live = f.live(3, "0a1b2c3d");
  {
    auto d = Db::open(live, FileKind::Playthrough, false);
    REQUIRE(d);
    REQUIRE(set_user_version(*d, 99));
    REQUIRE(meta_set(*d, "schema_min_reader", "99"));
  }
  const auto newer = rig.send(db::Open{the_key(), kHour + 3 * kMin, 1});
  REQUIRE(std::holds_alternative<db::Failed>(newer.result));
  CHECK(std::get<db::Failed>(newer.result).fault == Fault::TooNew);
  CHECK_FALSE(rig.store.open());
  // Corrupt the live file: the next open sets it aside and restores the newest backup.
  test::write_file(live, std::string(8192, '\x5a'));
  fs::remove(fs::path(live).concat("-wal"));
  fs::remove(fs::path(live).concat("-shm"));
  const db::Opened restored = rig.open_at(kHour + 3 * kMin);
  CHECK(restored.restored_backup);
  CHECK_FALSE(restored.created);
  CHECK(rig.count("events") >= 1);
}

TEST_CASE("store: random cuts (seeded) leave no row past the cut, no orphaned audience, and the counter as it was") {
  for (std::uint64_t seed = 1; seed <= 12; ++seed) {
    CAPTURE(seed);
    Rig rig;
    rig.open_at(kHour);
    std::uint64_t z = seed * 0x9E3779B97F4A7C15ull;
    const auto next_rand = [&] {
      z ^= z >> 33;
      z *= 0xff51afd7ed558ccdull;
      z ^= z >> 33;
      return z;
    };
    proto::Pt t = kHour;
    for (int i = 0; i < 40; ++i) {
      t += static_cast<proto::Pt>(next_rand() % (2 * kMin));
      std::vector<proto::AudienceRow> aud;
      if (next_rand() % 2 == 0) aud.push_back({"kale", How::Witnessed});
      if (next_rand() % 3 == 0) aud.push_back({"boc", How::Direct});
      rig.put(next_rand() % 2 == 0 ? "PersonaSeen" : "PlaceEntered", t, aud);
    }
    const proto::EventId counter = rig.next;
    rig.ok(rig.send(db::Close{}));
    const proto::Pt cut = kHour + static_cast<proto::Pt>(next_rand() % static_cast<std::uint64_t>(t - kHour + 1));
    db::Opened o = rig.open_at(cut);
    if (o.rollback.kind == db::Rollback::Kind::Choice) o = opened_of(rig.send(db::Resolve{db::Choice::Forget}));
    CHECK(rig.value("SELECT count(*) FROM events WHERE t_play_ms > " + number_text(cut)) == "0");
    CHECK(rig.value("SELECT count(*) FROM event_audience a WHERE NOT EXISTS "
                    "(SELECT 1 FROM events e WHERE e.id = a.event_id)") == "0");
    CHECK(o.next_event_id == counter);
    CHECK(healthy(rig.store.playthrough()->db).value_or(false));
  }
}
