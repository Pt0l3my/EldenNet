// Templates on disk: FolderSource, with its checks against leaving the root, and the Environment over a folder. Apart
// from source.cpp and environment.cpp so that a program that renders only from memory (en-core, in the DLL: ARCH §2.3)
// links none of it, and so none of the C++ runtime's file system code (eldennet.dll's imports, dll_checks).
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "detail.hpp"
#include "en/prompt/environment.hpp"
#include "en/prompt/source.hpp"

namespace en::prompt {

namespace fs = std::filesystem;

namespace {

std::string utf8_name(const fs::path& path) {
  const std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

bool inside(const fs::path& root, const fs::path& path) {
  auto r = root.begin();
  auto p = path.begin();
  for (; r != root.end(); ++r, ++p) {
    if (p == path.end() || *r != *p) return false;
  }
  return true;
}

// Whether `path`, once its links are followed, lies outside the root.
bool leads_out(const fs::path& canonical_root, const fs::path& path) {
  std::error_code error;
  const fs::path real = fs::weakly_canonical(path, error);
  return error || !inside(canonical_root, real);
}

bool read_file(const fs::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !in.bad();
}

}  // namespace

FolderSource::FolderSource(fs::path root) : root_(std::move(root)) {
  std::error_code error;
  if (!fs::is_directory(root_, error)) throw std::invalid_argument("the template root is not a folder");
  canonical_root_ = fs::canonical(root_, error);
  if (error) throw std::invalid_argument("the template root cannot be resolved");
}

std::optional<fs::path> FolderSource::path_of(std::string_view name) const {
  const detail::CheckedName checked = detail::check_name(name);
  if (!checked.problem.empty() || checked.name != name) return std::nullopt;
  fs::path path = root_;
  for (std::size_t start = 0;;) {
    const std::size_t slash = checked.name.find('/', start);
    path /= checked.name.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
    if (slash == std::string::npos) break;
    start = slash + 1;
  }
  return path;
}

TemplateSource::Entry FolderSource::find(std::string_view name) const {
  const std::optional<fs::path> path = path_of(name);
  if (!path) return Entry::none;
  std::error_code error;
  const fs::file_status status = fs::status(*path, error);  // follows links: leads_outside() checks where they go
  if (fs::is_directory(status)) return Entry::folder;
  if (fs::is_regular_file(status)) return Entry::file;
  return Entry::none;
}

std::optional<std::vector<std::string>> FolderSource::files(std::string_view folder) const {
  const std::optional<fs::path> path = path_of(folder);
  if (!path || leads_out(canonical_root_, *path)) return std::nullopt;
  std::error_code error;
  fs::directory_iterator it(*path, error);
  if (error) return std::nullopt;
  std::vector<std::string> out;
  // An error ends the listing, whether or not the iterator has reached its end by then.
  for (; it != fs::directory_iterator(); it.increment(error)) {
    if (error) return std::nullopt;
    std::error_code kind_error;
    if (it->is_regular_file(kind_error)) out.push_back(utf8_name(it->path().filename()));  // a link to a file counts
  }
  if (error) return std::nullopt;
  return out;
}

std::optional<std::string> FolderSource::read(std::string_view name) const {
  const std::optional<fs::path> path = path_of(name);
  std::string text;
  if (!path || leads_out(canonical_root_, *path) || !read_file(*path, text)) return std::nullopt;
  return text;
}

bool FolderSource::leads_outside(std::string_view name) const {
  const std::optional<fs::path> path = path_of(name);
  return !path || leads_out(canonical_root_, *path);
}

Environment::Environment(std::filesystem::path root, FunctionRegistry functions, EnvironmentOptions options)
    : Environment(std::make_shared<const FolderSource>(std::move(root)), std::move(functions), options) {}

}  // namespace en::prompt
