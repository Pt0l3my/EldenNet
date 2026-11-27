#pragma once
// en-store's tests: a folder of the test's own under the system's temporary folder, removed afterwards, and small
// helpers over SQLite's C API. Every row and file the tests make is invented.
#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include <sqlite3.h>

#include "en/store/db.hpp"

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

}  // namespace en::store::test
