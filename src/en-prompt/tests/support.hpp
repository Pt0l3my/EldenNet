#pragma once
// Helpers for en-prompt's tests: a temporary template tree, rendering shortcuts and error capture. Every template,
// name and record in the tests is invented.
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <doctest.h>
#include <nlohmann/json.hpp>

#include "en/prompt/prompt.hpp"

namespace en_test {

using nlohmann::json;
namespace prompt = en::prompt;
using prompt::Role;

// A folder of template files under the system's temporary folder, removed afterwards.
class TempTree {
 public:
  TempTree() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("en-prompt-test-" + std::to_string(stamp) + "-" + std::to_string(counter++));
    std::filesystem::create_directories(root_);
  }
  ~TempTree() {
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }
  TempTree(const TempTree&) = delete;
  TempTree& operator=(const TempTree&) = delete;

  void write(const std::string& relative, const std::string& content) const {
    const std::filesystem::path path = root_ / std::filesystem::path(relative);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
  }

  const std::filesystem::path& root() const { return root_; }

 private:
  std::filesystem::path root_;
};

// Runs `f` and returns the TemplateError it throws; the test fails when it throws none.
template <class F>
prompt::TemplateError error_of(F&& f) {
  try {
    f();
  } catch (const prompt::TemplateError& error) {
    return error;
  }
  FAIL("expected a TemplateError");
  return prompt::TemplateError("none", prompt::Site{});
}

inline bool says(const prompt::TemplateError& error, const std::string& part) {
  return error.message().find(part) != std::string::npos;
}

// One environment, rooted at an empty folder, for templates held in memory.
inline const prompt::Environment& inline_environment() {
  static const TempTree tree;
  static const prompt::Environment environment(tree.root());
  return environment;
}

inline std::vector<prompt::Message> render_inline(const std::string& source, const json& context = json::object(),
                                                  const std::string& name = "strict.j2") {
  return inline_environment().render_source(source, context, name);
}

// The text of the one user message that `body` renders to.
inline std::string user(const std::string& body, const json& context = json::object()) {
  const std::vector<prompt::Message> messages = render_inline("[ user ]\n" + body, context);
  REQUIRE(messages.size() == 1);
  CHECK(messages.front().role == Role::user);
  return messages.front().text();
}

inline prompt::TemplateError failure(const std::string& source, const json& context = json::object()) {
  return error_of([&] { render_inline(source, context); });
}

inline bool has_rule(const std::vector<prompt::Finding>& findings, const std::string& rule) {
  for (const prompt::Finding& finding : findings) {
    if (finding.rule == rule) return true;
  }
  return false;
}

}  // namespace en_test
