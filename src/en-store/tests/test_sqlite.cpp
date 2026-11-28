// SQLite as pinned (O146; MEM §8.5): the version, and every compile option cmake/sqlite.cmake sets, as the library
// itself reports them; FTS5, which the reserved `memories_fts` needs, and the JSON functions the schema's checks use.
#include <doctest.h>

#include <string>

#include <sqlite3.h>

namespace {

struct Mem {
  sqlite3* db = nullptr;
  Mem() { REQUIRE(sqlite3_open(":memory:", &db) == SQLITE_OK); }
  ~Mem() { sqlite3_close(db); }
  int exec(const char* sql) { return sqlite3_exec(db, sql, nullptr, nullptr, nullptr); }
};

}  // namespace

TEST_CASE("sqlite: the pinned version, as the library reports it") {
  CHECK(std::string(sqlite3_libversion()) == "3.54.1");
  CHECK(sqlite3_libversion_number() == 3054001);
  CHECK(std::string(sqlite3_sourceid()) == SQLITE_SOURCE_ID);  // the header and the library are one release
}

TEST_CASE("sqlite: every compile option the build sets, and serialised threading") {
  // SQLite records each option it was built with but SQLITE_TRUSTED_SCHEMA, whose default the pragma shows below.
  for (const std::string opt : {"ENABLE_FTS5", "OMIT_LOAD_EXTENSION", "DQS=0", "DEFAULT_FOREIGN_KEYS",
                                "DEFAULT_WAL_SYNCHRONOUS=1", "DEFAULT_MEMSTATUS=0", "OMIT_DEPRECATED",
                                "OMIT_SHARED_CACHE", "LIKE_DOESNT_MATCH_BLOBS", "THREADSAFE=1"}) {
    CAPTURE(opt);
    CHECK(sqlite3_compileoption_used(opt.c_str()) == 1);
  }
  CHECK(sqlite3_threadsafe() == 1);
  Mem m;
  for (const char* pragma : {"PRAGMA trusted_schema", "PRAGMA foreign_keys"}) {
    CAPTURE(std::string(pragma));
    sqlite3_stmt* st = nullptr;
    REQUIRE(sqlite3_prepare_v2(m.db, pragma, -1, &st, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(st) == SQLITE_ROW);
    CHECK(sqlite3_column_int64(st, 0) == (std::string(pragma) == "PRAGMA trusted_schema" ? 0 : 1));
    sqlite3_finalize(st);
  }
}

TEST_CASE("sqlite: FTS5 and the JSON functions work, and no extension can be loaded") {
  Mem m;
  CHECK(m.exec("CREATE VIRTUAL TABLE t USING fts5(content)") == SQLITE_OK);
  CHECK(m.exec("INSERT INTO t (content) VALUES ('a lamp by the road')") == SQLITE_OK);
  sqlite3_stmt* st = nullptr;
  REQUIRE(sqlite3_prepare_v2(m.db, "SELECT count(*) FROM t WHERE t MATCH 'lamp'", -1, &st, nullptr) == SQLITE_OK);
  REQUIRE(sqlite3_step(st) == SQLITE_ROW);
  CHECK(sqlite3_column_int64(st, 0) == 1);
  sqlite3_finalize(st);
  REQUIRE(sqlite3_prepare_v2(m.db, "SELECT json_valid('[1,2]'), json_array_length('[1,2,3]')", -1, &st, nullptr) ==
          SQLITE_OK);
  REQUIRE(sqlite3_step(st) == SQLITE_ROW);
  CHECK(sqlite3_column_int64(st, 0) == 1);
  CHECK(sqlite3_column_int64(st, 1) == 3);
  sqlite3_finalize(st);
  // A double-quoted string is a name, never a literal (DQS=0).
  CHECK(m.exec("SELECT \"not a column\"") != SQLITE_OK);
  // A 64-bit id reads back exactly, past 2^53.
  REQUIRE(sqlite3_prepare_v2(m.db, "SELECT ?1", -1, &st, nullptr) == SQLITE_OK);
  const sqlite3_int64 big = (sqlite3_int64{1} << 53) + 1;
  sqlite3_bind_int64(st, 1, big);
  REQUIRE(sqlite3_step(st) == SQLITE_ROW);
  CHECK(sqlite3_column_int64(st, 0) == big);
  sqlite3_finalize(st);
}
