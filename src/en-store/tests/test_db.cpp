// en-store's layer over SQLite (db.hpp; MEM §8.5, §10): the pragmas of a playthrough file, statements and transactions,
// a batch split by SQLite's own parser, 64-bit ids exactly, numbers the same in every locale, and the faults by class.
#include <doctest.h>

#include <clocale>
#include <cstdint>
#include <locale>
#include <sstream>
#include <string>

#include "support.hpp"

using namespace en::store;
using en::store::test::TempDir;

namespace {

// A locale whose decimal point is a comma and which groups thousands: what a player's could be.
struct CommaPunct : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};

Db fresh(const TempDir& dir, const char* name = "t.sqlite") {
  auto db = Db::open(dir.path() / name, FileKind::Playthrough, true);
  REQUIRE(db);
  return std::move(*db);
}

}  // namespace

TEST_CASE("db: a playthrough file opens in WAL with MEM §8.5's pragmas and its application id") {
  TempDir dir;
  Db db = fresh(dir);
  CHECK(test::value(db, "PRAGMA journal_mode") == "wal");
  CHECK(test::value(db, "PRAGMA synchronous") == "1");     // NORMAL
  CHECK(test::value(db, "PRAGMA foreign_keys") == "1");
  CHECK(test::value(db, "PRAGMA secure_delete") == "2");   // FAST
  CHECK(test::value(db, "PRAGMA trusted_schema") == "0");
  CHECK(test::value(db, "PRAGMA temp_store") == "2");      // MEMORY
  CHECK(test::value(db, "PRAGMA cache_size") == "-8192");
  CHECK(test::value(db, "PRAGMA mmap_size") == "0");
  CHECK(test::value(db, "PRAGMA journal_size_limit") == "4194304");
  CHECK(test::value(db, "PRAGMA application_id") == "1162760276");  // 0x454E5054
  db.close();
  // Opened again: the same, and the application id kept.
  auto again = Db::open(dir.path() / "t.sqlite", FileKind::Playthrough, false);
  REQUIRE(again);
  CHECK(test::value(*again, "PRAGMA application_id") == "1162760276");
}

TEST_CASE("db: a file that is not ours, or no database at all, is NotADb; a missing one is not created") {
  TempDir dir;
  test::write_file(dir.path() / "junk.sqlite", std::string(4096, 'x'));
  auto junk = Db::open(dir.path() / "junk.sqlite", FileKind::Playthrough, false);
  REQUIRE_FALSE(junk);
  CHECK(junk.error() == Fault::NotADb);
  {
    sqlite3* raw = nullptr;
    REQUIRE(sqlite3_open((dir.path() / "other.sqlite").string().c_str(), &raw) == SQLITE_OK);
    REQUIRE(sqlite3_exec(raw, "PRAGMA application_id = 7; CREATE TABLE x (a)", nullptr, nullptr, nullptr) == SQLITE_OK);
    sqlite3_close(raw);
  }
  auto other = Db::open(dir.path() / "other.sqlite", FileKind::Playthrough, false);
  REQUIRE_FALSE(other);
  CHECK(other.error() == Fault::NotADb);
  auto none = Db::open(dir.path() / "absent.sqlite", FileKind::Playthrough, false);
  REQUIRE_FALSE(none);
  CHECK(none.error() == Fault::IoErr);
  CHECK_FALSE(std::filesystem::exists(dir.path() / "absent.sqlite"));
}

TEST_CASE("db: ids past 2^32 and 2^53 bind and read back exactly, in columns and inside JSON") {
  TempDir dir;
  Db db = fresh(dir);
  REQUIRE(exec(db, "CREATE TABLE t (id INTEGER PRIMARY KEY, j TEXT)"));
  const std::int64_t ids[] = {(std::int64_t{1} << 33) + 7, (std::int64_t{1} << 53) + 1, (std::int64_t{1} << 62) + 3,
                              INT64_MAX};
  for (const std::int64_t id : ids) {
    auto ins = Stmt::prepare(db, "INSERT INTO t (id, j) VALUES (?1, json_object('ref', ?1))");
    REQUIRE(ins);
    ins->bind(1, id);
    REQUIRE(ins->run());
    auto get = Stmt::prepare(db, "SELECT id, json_extract(j, '$.ref') FROM t WHERE id = ?1");
    REQUIRE(get);
    get->bind(1, id);
    auto r = get->step();
    REQUIRE(r);
    REQUIRE(*r);
    CHECK(get->i64(0) == id);
    CHECK(get->i64(1) == id);  // SQLite's JSON keeps an integer exact
  }
}

TEST_CASE("db: numbers are written and read the same under a locale with a decimal comma") {
  const std::locale before = std::locale::global(std::locale(std::locale::classic(), new CommaPunct));
  const char* c_before = std::setlocale(LC_ALL, nullptr);
  const std::string c_saved = c_before ? c_before : "C";
  // A C locale with a decimal comma where one is installed (the tests' image may hold none: then only the C++ one).
  for (const char* name : {"de_DE.UTF-8", "fr_FR.UTF-8", "de_DE.utf8"}) {
    if (std::setlocale(LC_ALL, name) != nullptr) break;
  }
  std::ostringstream os;
  os << 1234567;
  CHECK(os.str() != "1234567");  // the stream under the global locale groups: the reason streams are never used
  CHECK(number_text(1234567) == "1234567");
  CHECK(number_text(-90071992547409930) == "-90071992547409930");
  CHECK(number_of("9007199254740993") == 9007199254740993);
  CHECK_FALSE(number_of("1.234").has_value());
  CHECK_FALSE(number_of("12a").has_value());
  CHECK_FALSE(number_of("").has_value());
  TempDir dir;
  Db db = fresh(dir);
  REQUIRE(exec(db, "CREATE TABLE f (x REAL)"));
  auto ins = Stmt::prepare(db, "INSERT INTO f VALUES (?1)");
  REQUIRE(ins);
  ins->bind(1, 0.75);
  REQUIRE(ins->run());
  CHECK(test::value(db, "SELECT x FROM f") == "0.75");  // SQLite's own text, never the locale's
  CHECK(test::value(db, "SELECT printf('%.2f', x) FROM f") == "0.75");
  std::setlocale(LC_ALL, c_saved.c_str());
  std::locale::global(before);
}

TEST_CASE("db: a batch is split by SQLite's own parser: an apostrophe in a comment, a semicolon in a string") {
  TempDir dir;
  Db db = fresh(dir);
  const std::string sql =
      "-- the Tarnished's own table; it holds one row\n"
      "CREATE TABLE notes (t TEXT);\n"
      "/* a block comment; with 'quotes' */\n"
      "INSERT INTO notes VALUES ('a; b; c');\n"
      "INSERT INTO notes VALUES ('it''s here');\n"
      "CREATE TRIGGER notes_guard BEFORE DELETE ON notes BEGIN SELECT RAISE(ABORT, 'kept; always'); END;\n";
  REQUIRE(execute_batch(db, sql));
  CHECK(test::count(db, "notes") == 2);
  CHECK(test::value(db, "SELECT t FROM notes WHERE t LIKE 'a%'") == "a; b; c");
  CHECK_FALSE(exec(db, "DELETE FROM notes"));  // the trigger, whole: its body's ';' did not split it
  CHECK(execute_batch(db, "  -- only a comment\n"));
  const auto bad = execute_batch(db, "CREATE TABLE ok (a); CREATE TABLE ok (a);");
  REQUIRE_FALSE(bad);
  CHECK(bad.error() == Fault::Other);
}

TEST_CASE("db: a transaction rolls back unless committed, and a constraint is the request's fault") {
  TempDir dir;
  Db db = fresh(dir);
  REQUIRE(exec(db, "CREATE TABLE t (a INTEGER NOT NULL CHECK (a > 0))"));
  {
    auto tx = Transaction::immediate(db);
    REQUIRE(tx);
    REQUIRE(exec(db, "INSERT INTO t VALUES (1)"));
  }  // dropped: rolled back
  CHECK(test::count(db, "t") == 0);
  {
    auto tx = Transaction::immediate(db);
    REQUIRE(tx);
    REQUIRE(exec(db, "INSERT INTO t VALUES (2)"));
    REQUIRE(tx->commit());
  }
  CHECK(test::count(db, "t") == 1);
  const auto bad = exec(db, "INSERT INTO t VALUES (-1)");
  REQUIRE_FALSE(bad);
  CHECK(bad.error() == Fault::Constraint);
}

TEST_CASE("db: SQLite's result classes as the store's faults") {
  CHECK(fault_of(SQLITE_BUSY) == Fault::Busy);
  CHECK(fault_of(SQLITE_BUSY_SNAPSHOT) == Fault::Busy);
  CHECK(fault_of(SQLITE_LOCKED) == Fault::Busy);
  CHECK(fault_of(SQLITE_FULL) == Fault::Full);
  CHECK(fault_of(SQLITE_IOERR_WRITE) == Fault::IoErr);
  CHECK(fault_of(SQLITE_IOERR_FSYNC) == Fault::IoErr);
  CHECK(fault_of(SQLITE_CORRUPT) == Fault::Corrupt);
  CHECK(fault_of(SQLITE_CORRUPT_INDEX) == Fault::Corrupt);
  CHECK(fault_of(SQLITE_NOTADB) == Fault::NotADb);
  CHECK(fault_of(SQLITE_CONSTRAINT_CHECK) == Fault::Constraint);
  CHECK(fault_of(SQLITE_ERROR) == Fault::Other);
}

TEST_CASE("db: a backup is a consistent copy, made only where nothing is; meta keeps its values") {
  TempDir dir;
  Db db = fresh(dir);
  REQUIRE(exec(db, "CREATE TABLE meta (k TEXT PRIMARY KEY, v TEXT NOT NULL) WITHOUT ROWID"));
  REQUIRE(meta_set(db, "next_event_id", "8589934599"));
  REQUIRE(meta_set(db, "next_event_id", "8589934600"));
  CHECK(meta_get(db, "next_event_id").value_or(std::nullopt) == "8589934600");
  CHECK(meta_get(db, "absent").value_or(std::string("x")) == std::nullopt);
  const auto copy = dir.path() / "copy.sqlite";
  REQUIRE(backup_into(db, copy));
  CHECK_FALSE(backup_into(db, copy));  // never over a file that exists
  auto c = Db::open(copy, FileKind::Playthrough, false);
  REQUIRE(c);
  CHECK(meta_get(*c, "next_event_id").value_or(std::nullopt) == "8589934600");
  CHECK(healthy(*c).value_or(false));
  REQUIRE(checkpoint(db, true));
  REQUIRE(checkpoint(db, false));
}
