// en-store's layer over SQLite's C API (db.hpp).
#include "en/store/db.hpp"

#include <charconv>
#include <climits>
#include <system_error>

#include <sqlite3.h>

namespace en::store {

Fault fault_of(int rc) noexcept {
  switch (rc & 0xFF) {
    case SQLITE_BUSY:
    case SQLITE_LOCKED:
      return Fault::Busy;
    case SQLITE_FULL:
      return Fault::Full;
    case SQLITE_IOERR:
    case SQLITE_CANTOPEN:
    case SQLITE_PROTOCOL:
      return Fault::IoErr;
    case SQLITE_CORRUPT:
      return Fault::Corrupt;
    case SQLITE_NOTADB:
      return Fault::NotADb;
    case SQLITE_CONSTRAINT:
    case SQLITE_MISMATCH:
    case SQLITE_TOOBIG:
      return Fault::Constraint;
    default:
      return Fault::Other;
  }
}

namespace {

// MEM §8.5's pragmas for the playthrough file. journal_mode is set first, on its own, before any transaction.
constexpr std::string_view kPlaythroughPragmas =
    "PRAGMA synchronous = NORMAL;"
    "PRAGMA foreign_keys = ON;"
    "PRAGMA temp_store = MEMORY;"
    "PRAGMA cache_size = -8192;"
    "PRAGMA wal_autocheckpoint = 1000;"
    "PRAGMA journal_size_limit = 4194304;"
    "PRAGMA secure_delete = FAST;"
    "PRAGMA trusted_schema = OFF;"
    "PRAGMA mmap_size = 0;";
constexpr std::int64_t kPlaythroughAppId = 0x454E5054;  // "ENPT"

}  // namespace

std::string utf8_path(const std::filesystem::path& p) {
  const std::u8string u = p.u8string();
  return std::string(u.begin(), u.end());
}

std::string number_text(std::int64_t v) {
  char buf[24];
  const auto r = std::to_chars(buf, buf + sizeof buf, v);
  return std::string(buf, r.ptr);
}

std::optional<std::int64_t> number_of(std::string_view text) noexcept {
  std::int64_t v = 0;
  const auto r = std::from_chars(text.data(), text.data() + text.size(), v);
  if (r.ec != std::errc{} || r.ptr != text.data() + text.size() || text.empty()) return std::nullopt;
  return v;
}

// ---- Db ----

Db::~Db() { close(); }
Db::Db(Db&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }
Db& Db::operator=(Db&& o) noexcept {
  if (this != &o) {
    close();
    db_ = o.db_;
    o.db_ = nullptr;
  }
  return *this;
}

void Db::close() noexcept {
  if (db_ != nullptr) {
    sqlite3_close_v2(db_);
    db_ = nullptr;
  }
}

std::int64_t Db::changes() const noexcept { return db_ != nullptr ? sqlite3_changes64(db_) : 0; }

Result<Db> Db::open(const std::filesystem::path& path, FileKind kind, bool create) {
  (void)kind;  // one kind in v0.3; pools and diag come later (WHY§689)
  Db d;
  int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE;
  if (create) flags |= SQLITE_OPEN_CREATE;
  const std::string p = utf8_path(path);
  sqlite3* raw = nullptr;
  const int rc = sqlite3_open_v2(p.c_str(), &raw, flags, nullptr);
  d.db_ = raw;
  if (rc != SQLITE_OK) return Unexpected{fault_of(rc)};
  sqlite3_extended_result_codes(raw, 1);
  sqlite3_busy_timeout(raw, 2000);
  // A file that is not a database answers NOTADB to its first read: the application id is that read.
  Stmt app;
  if (auto s = Stmt::prepare(d, "PRAGMA application_id"); s) {
    app = std::move(*s);
  } else {
    return Unexpected{s.error()};
  }
  auto has = app.step();
  if (!has) return Unexpected{has.error()};
  const std::int64_t id = *has ? app.i64(0) : 0;
  app.reset();
  app = Stmt{};
  if (id != 0 && id != kPlaythroughAppId) return Unexpected{Fault::NotADb};
  {
    auto jm = Stmt::prepare(d, "PRAGMA journal_mode = WAL");
    if (!jm) return Unexpected{jm.error()};
    auto r = jm->step();
    if (!r) return Unexpected{r.error()};
    if (!*r || jm->text(0) != "wal") return Unexpected{Fault::IoErr};
  }
  if (auto r = execute_batch(d, kPlaythroughPragmas); !r) return Unexpected{r.error()};
  if (id == 0) {
    if (auto r = exec(d, "PRAGMA application_id = " + number_text(kPlaythroughAppId)); !r) return Unexpected{r.error()};
  }
  return d;
}

// ---- Stmt ----

Stmt::~Stmt() {
  if (st_ != nullptr) sqlite3_finalize(st_);
}
Stmt::Stmt(Stmt&& o) noexcept : st_(o.st_), bind_failed_(o.bind_failed_) { o.st_ = nullptr; }
Stmt& Stmt::operator=(Stmt&& o) noexcept {
  if (this != &o) {
    if (st_ != nullptr) sqlite3_finalize(st_);
    st_ = o.st_;
    bind_failed_ = o.bind_failed_;
    o.st_ = nullptr;
  }
  return *this;
}

Result<Stmt> Stmt::prepare(const Db& db, std::string_view sql) {
  if (!db.is_open()) return Unexpected{Fault::NotOpen};
  if (sql.size() > INT_MAX) return Unexpected{Fault::Invalid};
  Stmt s;
  const int rc = sqlite3_prepare_v2(db.raw(), sql.data(), static_cast<int>(sql.size()), &s.st_, nullptr);
  if (rc != SQLITE_OK) return Unexpected{fault_of(rc)};
  if (s.st_ == nullptr) return Unexpected{Fault::Invalid};  // no statement in the text
  return s;
}

Stmt& Stmt::bind(int i, std::int64_t v) {
  if (sqlite3_bind_int64(st_, i, static_cast<sqlite3_int64>(v)) != SQLITE_OK) bind_failed_ = true;
  return *this;
}
Stmt& Stmt::bind(int i, std::string_view v) {
  if (v.size() > INT_MAX ||
      sqlite3_bind_text(st_, i, v.data(), static_cast<int>(v.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
    bind_failed_ = true;
  }
  return *this;
}
Stmt& Stmt::bind(int i, double v) {
  if (sqlite3_bind_double(st_, i, v) != SQLITE_OK) bind_failed_ = true;
  return *this;
}
Stmt& Stmt::bind_null(int i) {
  if (sqlite3_bind_null(st_, i) != SQLITE_OK) bind_failed_ = true;
  return *this;
}

Result<bool> Stmt::step() {
  if (st_ == nullptr || bind_failed_) return Unexpected{Fault::Invalid};
  const int rc = sqlite3_step(st_);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  return Unexpected{fault_of(rc)};
}

Result<void> Stmt::run() {
  auto r = step();
  if (!r) return Unexpected{r.error()};
  while (*r) {
    r = step();
    if (!r) return Unexpected{r.error()};
  }
  return {};
}

void Stmt::reset() noexcept {
  if (st_ != nullptr) {
    sqlite3_reset(st_);
    sqlite3_clear_bindings(st_);
  }
  bind_failed_ = false;
}

bool Stmt::null(int i) const noexcept { return sqlite3_column_type(st_, i) == SQLITE_NULL; }
std::int64_t Stmt::i64(int i) const noexcept { return static_cast<std::int64_t>(sqlite3_column_int64(st_, i)); }
double Stmt::f64(int i) const noexcept { return sqlite3_column_double(st_, i); }
std::string Stmt::text(int i) const {
  const unsigned char* t = sqlite3_column_text(st_, i);
  const int n = sqlite3_column_bytes(st_, i);
  return t != nullptr && n > 0 ? std::string(reinterpret_cast<const char*>(t), static_cast<std::size_t>(n))
                               : std::string();
}
std::optional<std::int64_t> Stmt::opt_i64(int i) const noexcept {
  if (null(i)) return std::nullopt;
  return i64(i);
}
std::optional<std::string> Stmt::opt_text(int i) const {
  if (null(i)) return std::nullopt;
  return text(i);
}

// ---- Transaction ----

Result<Transaction> Transaction::immediate(const Db& db) {
  if (auto r = exec(db, "BEGIN IMMEDIATE"); !r) return Unexpected{r.error()};
  return Transaction(db.raw());
}
Result<Transaction> Transaction::exclusive(const Db& db) {
  if (auto r = exec(db, "BEGIN EXCLUSIVE"); !r) return Unexpected{r.error()};
  return Transaction(db.raw());
}
Transaction::~Transaction() {
  if (db_ != nullptr) sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
}
Transaction::Transaction(Transaction&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }
Result<void> Transaction::commit() {
  if (db_ == nullptr) return Unexpected{Fault::Invalid};
  const int rc = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) return Unexpected{fault_of(rc)};  // the destructor rolls back what is left
  db_ = nullptr;
  return {};
}

// ---- Helpers ----

Result<void> exec(const Db& db, std::string_view sql) {
  auto s = Stmt::prepare(db, sql);
  if (!s) return Unexpected{s.error()};
  return s->run();
}

Result<void> execute_batch(const Db& db, std::string_view sql) {
  if (!db.is_open()) return Unexpected{Fault::NotOpen};
  if (sql.size() > INT_MAX) return Unexpected{Fault::Invalid};
  const char* at = sql.data();
  const char* const end = sql.data() + sql.size();
  while (at < end) {
    sqlite3_stmt* st = nullptr;
    const char* tail = nullptr;
    const int rc = sqlite3_prepare_v2(db.raw(), at, static_cast<int>(end - at), &st, &tail);
    if (rc != SQLITE_OK) return Unexpected{fault_of(rc)};
    if (st == nullptr) break;  // only white space or comments were left
    int s = SQLITE_ROW;
    while (s == SQLITE_ROW) s = sqlite3_step(st);
    sqlite3_finalize(st);
    if (s != SQLITE_DONE) return Unexpected{fault_of(s)};
    if (tail == nullptr || tail <= at) break;
    at = tail;
  }
  return {};
}

Result<std::uint32_t> user_version(const Db& db) {
  auto s = Stmt::prepare(db, "PRAGMA user_version");
  if (!s) return Unexpected{s.error()};
  auto r = s->step();
  if (!r) return Unexpected{r.error()};
  if (!*r) return Unexpected{Fault::Other};
  const std::int64_t v = s->i64(0);
  if (v < 0 || v > 0x7FFFFFFF) return Unexpected{Fault::Corrupt};
  return static_cast<std::uint32_t>(v);
}

Result<void> set_user_version(const Db& db, std::uint32_t v) {
  return exec(db, "PRAGMA user_version = " + number_text(v));
}

Result<void> backup_into(const Db& db, const std::filesystem::path& target) {
  std::error_code ec;
  if (std::filesystem::exists(target, ec)) return Unexpected{Fault::Invalid};
  auto s = Stmt::prepare(db, "VACUUM INTO ?1");
  if (!s) return Unexpected{s.error()};
  s->bind(1, std::string_view(utf8_path(target)));
  return s->run();
}

Result<void> checkpoint(const Db& db, bool truncate) {
  return exec(db, truncate ? "PRAGMA wal_checkpoint(TRUNCATE)" : "PRAGMA wal_checkpoint(PASSIVE)");
}

Result<bool> healthy(const Db& db) {
  auto ic = Stmt::prepare(db, "PRAGMA integrity_check");
  if (!ic) return Unexpected{ic.error()};
  auto r = ic->step();
  if (!r) return Unexpected{r.error()};
  if (!*r || ic->text(0) != "ok") return false;
  auto fk = Stmt::prepare(db, "PRAGMA foreign_key_check");
  if (!fk) return Unexpected{fk.error()};
  auto f = fk->step();
  if (!f) return Unexpected{f.error()};
  return !*f;
}

Result<std::optional<std::string>> meta_get(const Db& db, std::string_view key) {
  auto s = Stmt::prepare(db, "SELECT v FROM meta WHERE k = ?1");
  if (!s) return Unexpected{s.error()};
  s->bind(1, key);
  auto r = s->step();
  if (!r) return Unexpected{r.error()};
  if (!*r) return std::optional<std::string>{};
  return std::optional<std::string>{s->text(0)};
}

Result<void> meta_set(const Db& db, std::string_view key, std::string_view value) {
  auto s = Stmt::prepare(db, "INSERT INTO meta (k, v) VALUES (?1, ?2) ON CONFLICT (k) DO UPDATE SET v = excluded.v");
  if (!s) return Unexpected{s.error()};
  s->bind(1, key).bind(2, value);
  return s->run();
}

}  // namespace en::store
