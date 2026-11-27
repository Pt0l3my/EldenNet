// The migration runner (migrate.hpp; MEM §10): the shipped 0001 on an empty file, a re-run, a newer file, a migration
// that fails half-way, the backup taken first, and the lint on transaction control.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/store/migrate.hpp"
#include "support.hpp"

using namespace en::store;
using en::store::test::TempDir;

namespace {

Db fresh(const TempDir& dir, const char* name = "pt.sqlite") {
  auto db = Db::open(dir.path() / name, FileKind::Playthrough, true);
  REQUIRE(db);
  return std::move(*db);
}

constexpr std::string_view kTwoGood =
    "-- the second migration's comment, with an apostrophe: it's harmless\n"
    "CREATE TABLE extra (a INTEGER NOT NULL DEFAULT 0, note TEXT DEFAULT 'one; two');\n"
    "INSERT INTO extra (a) VALUES (1);\n";
constexpr std::string_view kTwoBad =
    "CREATE TABLE half (a INTEGER);\n"
    "INSERT INTO nowhere VALUES (1);\n";  // fails after its first statement ran

}  // namespace

TEST_CASE("migrate: the shipped playthrough migrations are numbered from 1 with no gap, and clean") {
  const auto set = playthrough_migrations();
  REQUIRE(set.size() == playthrough_sql().size());
  REQUIRE_FALSE(set.empty());
  for (std::size_t i = 0; i < set.size(); ++i) {
    CAPTURE(std::string(set[i].name));
    CHECK(set[i].n == i + 1);
    CHECK(migration_lint(set[i].sql).empty());
  }
  CHECK(set[0].name == "0001_init.sql");
}

TEST_CASE("migrate: 0001 on an empty file makes MEM §8.7's tables, checked whole; a second run changes nothing") {
  TempDir dir;
  Db db = fresh(dir);
  const auto set = playthrough_migrations();
  auto r = migrate(db, set, dir.path() / "pre.sqlite");
  REQUIRE(r);
  CHECK(r->from == 0);
  CHECK(r->to == top_of(set));
  CHECK_FALSE(r->backed_up);  // an empty file needs no copy
  CHECK_FALSE(std::filesystem::exists(dir.path() / "pre.sqlite"));
  CHECK(user_version(db).value_or(0) == top_of(set));
  CHECK(meta_get(db, "app").value_or(std::nullopt) == "eldennet.playthrough");
  CHECK(meta_get(db, "schema_min_reader").value_or(std::nullopt) == "1");
  for (const char* t : {"meta", "epochs", "events", "event_audience", "conversations", "npc_facts", "npc_journal",
                        "npc_lived", "memory_progress", "memory_jobs", "persona_aliases", "forgotten", "npc_intents",
                        "decisions", "memories", "memory_actors", "memories_fts", "embeddings"}) {
    CAPTURE(std::string(t));
    CHECK(test::count(db, t) >= 0);
  }
  CHECK(healthy(db).value_or(false));
  auto again = migrate(db, set, dir.path() / "pre2.sqlite");
  REQUIRE(again);
  CHECK(again->from == again->to);
  CHECK_FALSE(again->backed_up);
}

TEST_CASE("migrate: a later migration takes a backup first, and one that fails leaves the previous version") {
  TempDir dir;
  Db db = fresh(dir);
  auto set = playthrough_migrations();
  REQUIRE(migrate(db, set, {}));
  const std::uint32_t top = top_of(set);
  // A good next migration: the backup first, then the new version, its apostrophe and semicolon read whole.
  std::vector<Migration> good = set;
  good.push_back(Migration{top + 1, "next_good.sql", kTwoGood});
  auto r = migrate(db, good, dir.path() / "pre-good.sqlite");
  REQUIRE(r);
  CHECK(r->backed_up);
  CHECK(std::filesystem::exists(dir.path() / "pre-good.sqlite"));
  CHECK(r->to == top + 1);
  CHECK(test::value(db, "SELECT note FROM extra") == "one; two");
  // A bad one: nothing of it stays, and the file is still at the version before it.
  std::vector<Migration> bad = good;
  bad.push_back(Migration{top + 2, "next_bad.sql", kTwoBad});
  auto f = migrate(db, bad, dir.path() / "pre-bad.sqlite");
  REQUIRE_FALSE(f);
  CHECK(user_version(db).value_or(0) == top + 1);
  CHECK(test::count(db, "half") == -1);  // its table was rolled back with it
  CHECK(healthy(db).value_or(false));
}

TEST_CASE("migrate: a newer file opens only when this build may read it (schema_min_reader)") {
  TempDir dir;
  Db db = fresh(dir);
  const auto set = playthrough_migrations();
  REQUIRE(migrate(db, set, {}));
  REQUIRE(set_user_version(db, 99));
  auto readable = migrate(db, set, {});
  REQUIRE(readable);
  CHECK(readable->newer);
  CHECK(user_version(db).value_or(0) == 99);  // never written down
  REQUIRE(meta_set(db, "schema_min_reader", "99"));
  auto refused = migrate(db, set, {});
  REQUIRE_FALSE(refused);
  CHECK(refused.error() == Fault::TooNew);
  REQUIRE(meta_set(db, "schema_min_reader", "seven"));
  auto unreadable = migrate(db, set, {});
  REQUIRE_FALSE(unreadable);
  CHECK(unreadable.error() == Fault::TooNew);
}

TEST_CASE("migrate: the lint refuses transaction control and journal mode, never a word in a comment or string") {
  CHECK(migration_lint("BEGIN;\nCREATE TABLE a (x);\nCOMMIT;") == "BEGIN");
  CHECK(migration_lint("CREATE TABLE a (x);\ncommit;") == "COMMIT");
  CHECK(migration_lint("CREATE TABLE a (x); END;") == "END");
  CHECK(migration_lint("ROLLBACK;") == "ROLLBACK");
  CHECK(migration_lint("SAVEPOINT s; CREATE TABLE a (x); RELEASE s;") == "SAVEPOINT");
  CHECK(migration_lint("PRAGMA journal_mode = DELETE;") == "journal_mode");
  CHECK(migration_lint("-- BEGIN, COMMIT and journal_mode in a comment\nCREATE TABLE a (x);").empty());
  CHECK(migration_lint("/* BEGIN; COMMIT; */ CREATE TABLE a (t TEXT DEFAULT 'BEGIN; COMMIT');").empty());
  CHECK(migration_lint("CREATE TABLE \"commit\" (x); CREATE TABLE [end] (y);").empty());
  CHECK(migration_lint("CREATE TRIGGER t AFTER INSERT ON a BEGIN UPDATE a SET x = 1; DELETE FROM b; END;\n"
                       "CREATE TABLE c (z);")
            .empty());
  CHECK(migration_lint("CREATE TRIGGER t AFTER INSERT ON a BEGIN SELECT 1; END; COMMIT;") == "COMMIT");
  CHECK(migration_lint("SELECT CASE WHEN 1 THEN 2 END;").empty());
}
