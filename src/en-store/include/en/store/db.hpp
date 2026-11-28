#pragma once
// en-store's thin layer over SQLite's C API (MEM §8.5, §10; ARCH §2.2's store rules): a connection, a statement, a
// transaction and a batch run by SQLite's own parser. Every id is bound and read as a 64-bit integer, never through a
// 32-bit value or a double; text is UTF-8; a number is bound as itself, never formatted into SQL. Errors are SQLite's
// classes as the store's Fault (store.hpp): SQLite's own message, which can quote SQL or a path, is never kept.
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "en/proto/expected.hpp"
#include "en/proto/store.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace en::store {

using proto::db::Fault;
template <class T>
using Result = proto::Expected<T, Fault>;
using proto::Unexpected;

// SQLite's result code as a Fault (its primary class: SQLITE_BUSY and SQLITE_LOCKED are Busy, SQLITE_FULL Full, every
// SQLITE_IOERR_* IoErr, SQLITE_CORRUPT Corrupt, SQLITE_NOTADB NotADb, SQLITE_CONSTRAINT Constraint).
[[nodiscard]] Fault fault_of(int rc) noexcept;

// The pragmas of one kind of file (MEM §8.5's table).
enum class FileKind : std::uint8_t { Playthrough };

class Db {
 public:
  Db() = default;
  ~Db();
  Db(Db&& o) noexcept;
  Db& operator=(Db&& o) noexcept;
  Db(const Db&) = delete;
  Db& operator=(const Db&) = delete;

  // Opens (or, with `create`, creates) the file and sets its kind's pragmas: WAL, NORMAL sync, foreign keys, the busy
  // timeout, temp store and cache, the checkpoint and journal limits, FAST secure delete, an untrusted schema, no
  // mmap, and the application id on a new file. A file another application wrote (its application id) is NotADb.
  [[nodiscard]] static Result<Db> open(const std::filesystem::path& path, FileKind kind, bool create,
                                       int busy_timeout_ms = kBusyTimeoutMs);
  // MEM §8.5: a lock held by another connection is waited for this long before a request fails as Busy.
  static constexpr int kBusyTimeoutMs = 2000;

  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }
  [[nodiscard]] sqlite3* raw() const noexcept { return db_; }
  void close() noexcept;
  // Rows changed by the last statement, and the rowid the last INSERT gave.
  [[nodiscard]] std::int64_t changes() const noexcept;
  [[nodiscard]] std::int64_t last_insert_rowid() const noexcept;

 private:
  sqlite3* db_ = nullptr;
};

class Stmt {
 public:
  Stmt() = default;
  ~Stmt();
  Stmt(Stmt&& o) noexcept;
  Stmt& operator=(Stmt&& o) noexcept;
  Stmt(const Stmt&) = delete;
  Stmt& operator=(const Stmt&) = delete;

  [[nodiscard]] static Result<Stmt> prepare(const Db& db, std::string_view sql);

  // Parameters count from 1, as SQLite's do.
  Stmt& bind(int i, std::int64_t v);
  Stmt& bind(int i, std::string_view v);
  Stmt& bind(int i, double v);
  Stmt& bind_null(int i);
  Stmt& bind(int i, const std::optional<std::int64_t>& v) { return v ? bind(i, *v) : bind_null(i); }
  Stmt& bind(int i, const std::optional<std::string>& v) { return v ? bind(i, std::string_view(*v)) : bind_null(i); }
  // The first bind that failed, if any (a misuse: an index out of range), answered by step().
  [[nodiscard]] bool bind_failed() const noexcept { return bind_failed_; }

  // True: a row is ready; false: done. A bind that failed is Invalid.
  [[nodiscard]] Result<bool> step();
  // Steps once more and expects no row (an INSERT, UPDATE or DELETE).
  [[nodiscard]] Result<void> run();
  void reset() noexcept;

  // Columns count from 0.
  [[nodiscard]] int columns() const noexcept;
  [[nodiscard]] bool null(int i) const noexcept;
  [[nodiscard]] std::int64_t i64(int i) const noexcept;
  [[nodiscard]] double f64(int i) const noexcept;
  [[nodiscard]] std::string text(int i) const;
  [[nodiscard]] std::optional<std::int64_t> opt_i64(int i) const noexcept;
  [[nodiscard]] std::optional<std::string> opt_text(int i) const;

 private:
  sqlite3_stmt* st_ = nullptr;
  bool bind_failed_ = false;
};

// One transaction: BEGIN IMMEDIATE (every request) or EXCLUSIVE (a migration). Rolled back unless committed.
class Transaction {
 public:
  [[nodiscard]] static Result<Transaction> immediate(const Db& db);
  [[nodiscard]] static Result<Transaction> exclusive(const Db& db);
  ~Transaction();
  Transaction(Transaction&& o) noexcept;
  Transaction& operator=(Transaction&&) = delete;
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  [[nodiscard]] Result<void> commit();

 private:
  explicit Transaction(sqlite3* db) noexcept : db_(db) {}
  sqlite3* db_ = nullptr;
};

// One statement with no parameters and no rows wanted.
[[nodiscard]] Result<void> exec(const Db& db, std::string_view sql);
// A whole text of statements, split by SQLite's own parser: each is prepared in turn from the tail the last one left
// (`sqlite3_prepare_v2`), never by searching for ';', so a comment with an apostrophe or a string with a semicolon is
// safe (MEM §10).
[[nodiscard]] Result<void> execute_batch(const Db& db, std::string_view sql);

// PRAGMA user_version.
[[nodiscard]] Result<std::uint32_t> user_version(const Db& db);
[[nodiscard]] Result<void> set_user_version(const Db& db, std::uint32_t v);

// Copies the whole database into a new file (VACUUM INTO): a consistent backup, the source left as it is. The target
// must not exist.
[[nodiscard]] Result<void> backup_into(const Db& db, const std::filesystem::path& target);
// PRAGMA wal_checkpoint(TRUNCATE) or (PASSIVE).
[[nodiscard]] Result<void> checkpoint(const Db& db, bool truncate);
// PRAGMA integrity_check and foreign_key_check: both clean.
[[nodiscard]] Result<bool> healthy(const Db& db);

// The meta table's value of a key, and setting one.
[[nodiscard]] Result<std::optional<std::string>> meta_get(const Db& db, std::string_view key);
[[nodiscard]] Result<void> meta_set(const Db& db, std::string_view key, std::string_view value);

// A path as SQLite takes it: UTF-8.
[[nodiscard]] std::string utf8_path(const std::filesystem::path& p);

// Numbers as the store writes them into text (meta's values): the shortest form that reads back exactly, with '.' as
// the decimal point and no grouping, whatever the process's locale (MEM §10).
[[nodiscard]] std::string number_text(std::int64_t v);
[[nodiscard]] std::optional<std::int64_t> number_of(std::string_view text) noexcept;

}  // namespace en::store
