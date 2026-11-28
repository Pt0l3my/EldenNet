#pragma once
// en-store's tests: a folder of the test's own under the system's temporary folder, removed afterwards, and small
// helpers over SQLite's C API. Every row and file the tests make is invented.
#include <sqlite3.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <random>
#include <string>

#include "en/store/db.hpp"
#include "en/store/migrate.hpp"
#include "en/store/playthrough.hpp"
#include "en/store/store.hpp"

namespace en::store::test {

class TempDir {
 public:
  TempDir() {
    static std::atomic<unsigned> n{0};
    std::random_device rd;
    path_ = std::filesystem::temp_directory_path() /
            ("en-store-test-" + std::to_string(rd()) + "-" + std::to_string(n.fetch_add(1)));
    std::filesystem::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

inline void write_file(const std::filesystem::path& p, const std::string& text) {
  std::filesystem::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary | std::ios::trunc) << text;
}

// One value of a one-row query, as text ("" for NULL or no row).
inline std::string value(const Db& db, const std::string& sql) {
  auto s = Stmt::prepare(db, sql);
  if (!s) return "<error>";
  auto r = s->step();
  if (!r || !*r || s->null(0)) return "";
  return s->text(0);
}

inline std::int64_t count(const Db& db, const std::string& table) {
  auto s = Stmt::prepare(db, "SELECT count(*) FROM " + table);
  if (!s) return -1;
  auto r = s->step();
  return r && *r ? s->i64(0) : -1;
}

// A playthrough file of the test's own at `path`, migrated, with its first epoch (journey 1, from pt 0) and counters
// that start at realistic ids: past 2^33 for events, as MEM §10 asks of fixtures.
inline constexpr std::int64_t kFirstEvent = (std::int64_t{1} << 33) + 1;
inline Playthrough make_playthrough(const std::filesystem::path& path) {
  Playthrough pt;
  auto db = Db::open(path, FileKind::Playthrough, true);
  if (!db) return pt;
  pt.db = std::move(*db);
  (void)migrate(pt.db, playthrough_migrations(), {});
  (void)meta_set(pt.db, "next_event_id", number_text(kFirstEvent));
  (void)meta_set(pt.db, "next_conv_id", "1");
  pt.next_event_id = kFirstEvent;
  pt.wall = 1764201600;
  (void)new_epoch(pt, 1, 0, "open");
  return pt;
}

// An event of the tests' own: a kind, its play time and its audience; every other field left as the core leaves it.
inline proto::StoredEvent event(std::int64_t id, std::string kind, proto::Pt t,
                                std::vector<proto::AudienceRow> aud = {}, std::uint16_t journey = 1) {
  proto::StoredEvent e;
  e.id = id;
  e.journey = journey;
  e.kind = std::move(kind);
  e.t_play_ms = t;
  e.source = "snapshot_diff";
  e.audience = std::move(aud);
  return e;
}

// An invented account, slot and character, and a store's configuration under `saves` with a fixed wall clock and a
// roomy disk; `mono` is the breaker's clock (0 unless given).
inline proto::PlaythroughKey invented_key() {
  proto::PlaythroughKey k;
  k.steamid64 = 76561190000000002ull;
  k.slot = 1;
  k.fp8 = "7e8f9a0b";
  k.name_hash = "hash-of-another-invented-name";
  k.archetype = 2;
  k.gift = 4;
  return k;
}
inline StoreCfg store_config(const std::filesystem::path& saves, std::function<std::uint64_t()> mono = {}) {
  StoreCfg c;
  c.saves = saves;
  c.build = "test-build";
  c.wall = [] { return std::int64_t{1764201600}; };
  c.mono_ms = mono ? std::move(mono) : [] { return std::uint64_t{0}; };
  c.free_bytes = [](const std::filesystem::path&) -> std::optional<std::uint64_t> { return 10ull << 30; };
  return c;
}

// The crash child (test_crash.cpp): run by the test program itself when its first argument is `--crash-child`.
int crash_child(int argc, char** argv);

}  // namespace en::store::test
