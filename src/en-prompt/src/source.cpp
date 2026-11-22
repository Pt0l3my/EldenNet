// Template sources: names and texts held in memory, with the template name rules applied to its keys, and the check of
// names that a folder on disk uses too (folder.cpp: FolderSource lives apart, so that a program rendering from memory
// links no file system code).
#include "en/prompt/source.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

#include "detail.hpp"

namespace en::prompt {

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

bool TemplateSource::leads_outside(std::string_view) const { return false; }

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
