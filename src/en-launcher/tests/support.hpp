#pragma once
// A folder of the test's own under the system's temporary folder, removed afterwards, and small file helpers.
// Everything the tests put there is invented: Steam folders, app ids, save folders and their bytes.
#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace en::launcher::test {

class TempDir {
 public:
  TempDir() {
    static std::atomic<unsigned> n{0};
    std::random_device rd;
    path_ = std::filesystem::temp_directory_path() /
            ("en-launcher-test-" + std::to_string(rd()) + "-" + std::to_string(n.fetch_add(1)));
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

inline std::string read_file(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace en::launcher::test
