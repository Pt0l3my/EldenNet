// The store's faults (MEM §8.6; store.hpp's Breaker): what counts towards the breaker and what trips it at once, a full
// disk and a failed write or sync through the fault VFS, a corrupt file in session (set aside, and its newest backup
// back the next session), and a lock held by a second connection, mid-session and at open, which is never counted.
#include <doctest.h>
#include <sqlite3.h>

#include <string>

#include "en/store/store.hpp"
#include "fault_vfs.hpp"
#include "support.hpp"

using namespace en::store;
using namespace en::store::test;
namespace proto = en::proto;
namespace db = en::proto::db;
namespace fs = std::filesystem;

namespace {

struct Session {
  explicit Session(const fs::path& saves, std::function<std::uint64_t()> mono = {}, int busy_ms = 2000)
      : store([&] {
          StoreCfg c = store_config(saves, std::move(mono));
          c.busy_timeout_ms = busy_ms;
          return c;
        }()) {}
  StoreCore store;
  std::uint64_t seq = 0;
  proto::EventId next = 0;

  proto::DbResult send(proto::DbRequest::Op op) {
    proto::DbRequest r;
    r.seq = ++seq;
    r.op = std::move(op);
    return store.handle(r);
  }
  proto::DbResult open(proto::Pt pt = 60000) {
    proto::DbResult r = send(db::Open{invented_key(), pt, 1});
    if (const auto* o = std::get_if<db::Opened>(&r.result)) next = o->next_event_id;
    return r;
  }
  proto::DbResult put(proto::Pt t) {
    db::Append a;
    a.event = event(next, "PlaceEntered", t, {{"kale", proto::How::Direct}});
    proto::DbResult r = send(a);
    if (std::holds_alternative<db::Done>(r.result)) ++next;
    return r;
  }
};

std::optional<Fault> failed_with(const proto::DbResult& r) {
  if (const auto* f = std::get_if<db::Failed>(&r.result)) return f->fault;
  return std::nullopt;
}
std::optional<Fault> tripped_with(const proto::DbResult& r) {
  if (const auto* t = std::get_if<db::Tripped>(&r.result)) return t->cause;
  return std::nullopt;
}

fs::path live_of(const fs::path& saves) {
  const proto::PlaythroughKey k = invented_key();
  return save_folder(saves, k.steamid64).live(k.slot, k.fp8);
}

std::int64_t events_in(const fs::path& live) {
  auto d = Db::open(live, FileKind::Playthrough, false);
  REQUIRE(d.has_value());
  auto ok = healthy(*d);
  CHECK((ok && *ok));
  return count(*d, "events");
}

}  // namespace

TEST_CASE("breaker: three failed transactions within 60 s trip it; BUSY and refusals before SQL never count") {
  Breaker b;
  for (Fault f : {Fault::Busy, Fault::Invalid, Fault::NotOpen, Fault::TooNew}) {
    for (int i = 0; i < 5; ++i) CHECK_FALSE(b.note(f, 1000));
  }
  CHECK_FALSE(b.tripped());
  CHECK_FALSE(b.note(Fault::Constraint, 0));
  CHECK_FALSE(b.note(Fault::Full, 30000));
  CHECK_FALSE(b.note(Fault::Constraint, 60000));  // the first has left the window
  CHECK(b.note(Fault::Full, 61000));              // 30 s, 60 s and 61 s
  CHECK(b.cause() == Fault::Full);
  CHECK(b.note(Fault::Busy, 62000));  // tripped stays tripped
  CHECK(b.cause() == Fault::Full);
}

TEST_CASE("breaker: one CORRUPT, NOTADB or IOERR trips it at once") {
  for (Fault f : {Fault::Corrupt, Fault::NotADb, Fault::IoErr}) {
    Breaker b;
    CHECK(b.note(f, 5));
    CHECK(b.cause() == f);
  }
  Breaker spaced;
  for (std::uint64_t t = 0; t < 10 * Breaker::kWindowMs; t += Breaker::kWindowMs / 2 + 1) {
    CHECK_FALSE(spaced.note(Fault::Other, t));  // never three in one window
  }
}

TEST_CASE("store faults: a full disk fails each write alone; the third within 60 s trips the file, kept as it was") {
  FaultVfs vfs;
  TempDir dir;
  const fs::path saves = dir.path() / "saves";
  {
    Session s(saves);
    REQUIRE(std::holds_alternative<db::Opened>(s.open().result));
    REQUIRE(std::holds_alternative<db::Done>(s.put(70000).result));
    REQUIRE(std::holds_alternative<db::Done>(s.put(71000).result));
    vfs.faults().full_after = 0;
    CHECK(failed_with(s.put(72000)) == Fault::Full);
    vfs.faults().full_after = 0;
    CHECK(failed_with(s.put(72000)) == Fault::Full);
    vfs.faults().full_after = 0;
    CHECK(tripped_with(s.put(72000)) == Fault::Full);
    vfs.faults().clear();
    CHECK(s.store.tripped_for(invented_key()) == Fault::Full);
    CHECK_FALSE(s.store.open());
    // Memory is off for that file this session: its Open answers the trip, and nothing is written.
    CHECK(tripped_with(s.open()) == Fault::Full);
    CHECK(failed_with(s.put(73000)) == Fault::NotOpen);
  }
  CHECK(fs::exists(live_of(saves)));
  CHECK(events_in(live_of(saves)) == 2);
  // The next session opens it as it was.
  Session next(saves);
  REQUIRE(std::holds_alternative<db::Opened>(next.open(80000).result));
  CHECK(std::holds_alternative<db::Done>(next.put(81000).result));
}

TEST_CASE("store faults: a failed write or a failed sync is IOERR and trips the file at once, never set aside") {
  FaultVfs vfs;
  TempDir dir;
  const fs::path saves = dir.path() / "saves";
  {
    Session s(saves);
    REQUIRE(std::holds_alternative<db::Opened>(s.open().result));
    REQUIRE(std::holds_alternative<db::Done>(s.put(70000).result));
    vfs.faults().write_ioerr = true;
    CHECK(tripped_with(s.put(71000)) == Fault::IoErr);
    vfs.faults().clear();
  }
  CHECK(fs::exists(live_of(saves)));
  CHECK(events_in(live_of(saves)) == 1);
  {
    Session s(saves);
    REQUIRE(std::holds_alternative<db::Opened>(s.open(80000).result));
    REQUIRE(std::holds_alternative<db::Done>(s.put(81000).result));
    vfs.faults().sync_ioerr = true;  // a commit in WAL with NORMAL syncs nothing; the checkpoint syncs
    CHECK(std::holds_alternative<db::Done>(s.put(82000).result));
    CHECK(tripped_with(s.send(db::Checkpoint{})) == Fault::IoErr);
    vfs.faults().clear();
    CHECK(s.store.tripped_for(invented_key()) == Fault::IoErr);
  }
  CHECK(events_in(live_of(saves)) == 3);
}

TEST_CASE("store faults: a file found corrupt in session is closed and set aside; the next session restores a backup") {
  FaultVfs vfs;
  TempDir dir;
  const fs::path saves = dir.path() / "saves";
  {
    Session first(saves);
    REQUIRE(std::holds_alternative<db::Opened>(first.open().result));
    for (proto::Pt t = 61000; t < 61040; ++t) REQUIRE(std::holds_alternative<db::Done>(first.put(t).result));
    REQUIRE(std::holds_alternative<db::Done>(first.send(db::Close{}).result));
  }
  {
    Session s(saves);
    REQUIRE(std::holds_alternative<db::Opened>(s.open(70000).result));  // the start backup holds the first 40
    for (proto::Pt t = 70001; t < 70021; ++t) REQUIRE(std::holds_alternative<db::Done>(s.put(t).result));
    // Every page in the file, none in the WAL or the cache: the next read of a page past the first is garbled.
    REQUIRE(checkpoint(s.store.playthrough()->db, true).has_value());
    sqlite3_db_release_memory(s.store.playthrough()->db.raw());
    vfs.faults().garble_reads = true;
    CHECK(tripped_with(s.put(70030)) == Fault::Corrupt);
    vfs.faults().clear();
    CHECK(s.store.tripped_for(invented_key()) == Fault::Corrupt);
    CHECK_FALSE(fs::exists(live_of(saves)));
    CHECK(tripped_with(s.open(70040)) == Fault::Corrupt);
  }
  bool aside = false;
  for (const auto& e : fs::directory_iterator(live_of(saves).parent_path())) {
    if (e.path().filename().string().find(".sqlite.corrupt-") != std::string::npos) aside = true;
  }
  CHECK(aside);
  Session next(saves);
  const proto::DbResult r = next.open(90000);
  const auto* o = std::get_if<db::Opened>(&r.result);
  REQUIRE(o != nullptr);
  CHECK(o->restored_backup);
  CHECK_FALSE(o->created);
  CHECK(next.store.playthrough() != nullptr);
  CHECK(count(next.store.playthrough()->db, "events") == 40);
}

TEST_CASE("store faults: a lock held by another connection answers BUSY after the timeout, counted never") {
  TempDir dir;
  const fs::path saves = dir.path() / "saves";
  Session s(saves, {}, 50);
  REQUIRE(std::holds_alternative<db::Opened>(s.open().result));
  REQUIRE(std::holds_alternative<db::Done>(s.put(61000).result));
  auto other = Db::open(live_of(saves), FileKind::Playthrough, false);
  REQUIRE(other.has_value());
  REQUIRE(exec(*other, "BEGIN IMMEDIATE").has_value());
  for (int i = 0; i < 6; ++i) CHECK(failed_with(s.put(62000)) == Fault::Busy);
  CHECK_FALSE(s.store.tripped_for(invented_key()).has_value());
  CHECK(s.store.open());
  REQUIRE(exec(*other, "COMMIT").has_value());
  CHECK(std::holds_alternative<db::Done>(s.put(63000).result));

  // At open: the file is refused as BUSY, nothing tripped, and it opens once the lock is gone.
  REQUIRE(std::holds_alternative<db::Done>(s.send(db::Close{}).result));
  REQUIRE(exec(*other, "BEGIN IMMEDIATE").has_value());
  CHECK(failed_with(s.open(64000)) == Fault::Busy);
  CHECK_FALSE(s.store.open());
  CHECK_FALSE(s.store.tripped_for(invented_key()).has_value());
  REQUIRE(exec(*other, "ROLLBACK").has_value());
  const proto::DbResult again = s.open(64000);
  REQUIRE(std::holds_alternative<db::Opened>(again.result));
  CHECK(std::get<db::Opened>(again.result).next_event_id == s.next);
}
