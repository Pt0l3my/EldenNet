// Template sources: a folder on disk, with its checks against leaving the root, and names and texts held in memory,
// with the same name rules applied to its keys. The check of names both use is here too.
#include "en/prompt/source.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "detail.hpp"

namespace en::prompt {

namespace fs = std::filesystem;

namespace detail {

namespace {

bool is_name_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || c == '/' || c == '\\';
}

}  // namespace

CheckedName check_name(std::string_view raw) {
  const std::string shown(raw);
  if (raw.empty()) return {{}, "a template name is needed"};
  if (!std::all_of(raw.begin(), raw.end(), is_name_char)) {
    return {{}, "template names use letters, digits, '_', '-', '.' and '/', and '" + shown + "' has another character"};
  }
  std::string name(raw);
  std::replace(name.begin(), name.end(), '\\', '/');  // SkyrimNet writes "components\\event_history"
  while (!name.empty() && name.back() == '/') name.pop_back();
  for (std::size_t start = 0;;) {
    const std::size_t slash = name.find('/', start);
    const std::string_view segment =
        std::string_view(name).substr(start, slash == std::string::npos ? std::string_view::npos : slash - start);
    if (segment.empty() || segment == "." || segment == "..") {
      return {{}, "'" + shown + "' must name a template inside the root, without '.', '..' or empty folder names"};
    }
    if (slash == std::string::npos) break;
    start = slash + 1;
  }
  return {std::move(name), {}};
}

}  // namespace detail

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

bool TemplateSource::leads_outside(std::string_view) const { return false; }

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

MemorySource::MemorySource(std::map<std::string, std::string> templates) {
  std::map<std::string, std::string, std::less<>> keys;  // each checked name, with the key it was given as
  for (auto& [key, text] : templates) {
    detail::CheckedName checked = detail::check_name(key);
    if (!checked.problem.empty()) throw TemplateError(checked.problem, Site{key});
    if (const auto given = keys.find(checked.name); given != keys.end()) {
      throw TemplateError("'" + given->second + "' and '" + key + "' name the same template", Site{key});
    }
    for (std::size_t slash = checked.name.find('/'); slash != std::string::npos;
         slash = checked.name.find('/', slash + 1)) {
      folders_.insert(checked.name.substr(0, slash));
    }
    keys.emplace(checked.name, key);
    files_.emplace(std::move(checked.name), std::move(text));
  }
  for (const std::string& folder : folders_) {
    if (const auto given = keys.find(folder); given != keys.end()) {
      throw TemplateError("'" + given->second + "' is both a template and a folder", Site{given->second});
    }
  }
}

TemplateSource::Entry MemorySource::find(std::string_view name) const {
  if (files_.find(name) != files_.end()) return Entry::file;
  if (folders_.find(name) != folders_.end()) return Entry::folder;
  return Entry::none;
}

std::optional<std::vector<std::string>> MemorySource::files(std::string_view folder) const {
  if (folders_.find(folder) == folders_.end()) return std::nullopt;
  std::string prefix(folder);
  prefix += '/';
  std::vector<std::string> out;
  for (auto it = files_.lower_bound(prefix); it != files_.end() && it->first.compare(0, prefix.size(), prefix) == 0;
       ++it) {
    const std::string_view rest = std::string_view(it->first).substr(prefix.size());
    if (rest.find('/') == std::string_view::npos) out.emplace_back(rest);  // a file of a folder inside is not its own
  }
  return out;
}

std::optional<std::string> MemorySource::read(std::string_view name) const {
  const auto it = files_.find(name);
  if (it == files_.end()) return std::nullopt;
  return it->second;
}

}  // namespace en::prompt
