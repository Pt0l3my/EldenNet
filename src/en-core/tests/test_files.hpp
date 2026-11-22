#pragma once
// The tests' own files, built in at configure time (en-core's CMakeLists.txt): the overrides folder that binds the
// test persona (tests/scenarios/talk/overrides/), the fixture flag states and the truth tables, each by its path from
// the repository's root. Built in so that the Windows builds read no path under Wine.
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/content.hpp"
#include "en/core/core.hpp"

namespace en::core::test {

[[nodiscard]] std::span<const BuiltinFile> test_files() noexcept;

// The files below `prefix` (a folder of the repository, ending in '/'), with the prefix taken off their paths.
inline std::vector<ContentFile> files_under(std::string_view prefix, ContentOrigin origin) {
  std::vector<ContentFile> out;
  for (const BuiltinFile& f : test_files()) {
    if (f.name.substr(0, prefix.size()) == prefix) {
      out.push_back(ContentFile{std::string(f.name.substr(prefix.size())), std::string(f.text), origin});
    }
  }
  return out;
}

// The shipped content with the test overrides after it: the test persona bound to the fake game's row 81100.
inline std::vector<ContentFile> content_with_overrides() {
  std::vector<ContentFile> files = builtin_content();
  for (ContentFile& f : files_under("tests/scenarios/talk/overrides/", ContentOrigin::Override)) files.push_back(std::move(f));
  return files;
}

inline std::string_view test_file(std::string_view path) {
  for (const BuiltinFile& f : test_files()) {
    if (f.name == path) return f.text;
  }
  return {};
}

}  // namespace en::core::test
