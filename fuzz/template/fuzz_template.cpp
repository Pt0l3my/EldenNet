// libFuzzer target: en-prompt's template compile from a memory source (ARCH §2.3; NPC §7.1). The input, split at each
// NUL byte, is up to five templates under fixed names: a prompt, a folder of numbered parts and a template that either
// may render. Each name is linted, with and without a context, which compiles every template it reaches, and those it
// names literally, without rendering; rendering waits for its caps on output and steps (roadmap S8.1.k). Besides any
// sanitizer report, a broken property is a finding:
// - lint reports every problem as a finding and throws nothing;
// - a template compiles alike whatever the environment compiled before it (its inja parser is shared).
// An exception that escapes is a finding too, so this entry has no ffi_guard (R3 is for the engine's entries).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/prompt/environment.hpp"
#include "en/prompt/source.hpp"

namespace {

using nlohmann::json;

// A broken property ends the run as a crash does, so that libFuzzer keeps the input.
void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

// The names the input's templates take, in order.
constexpr std::array<std::string_view, 5> kNames = {
    "main.j2",            // a prompt
    "parts/10_open.j2",   // a folder of numbered parts, with "parts/05b_note.j2" below
    "parts/20_close.md",  //
    "lib/line.prompt",    // a template that the prompt or a part may render
    "parts/05b_note.j2",  //
};
constexpr std::array<std::string_view, 3> kLinted = {"main", "parts", "lib/line"};

// An invented context, so that lint also checks the variables a template names.
const json& context() {
  static const json kContext = {{"speaker", {{"name", "An Invented Speaker"}, {"mood", "calm"}}},
                                {"lines", {"a first line", "a second line"}},
                                {"count", 3},
                                {"ready", true}};
  return kContext;
}

std::vector<std::string> lint_all(const en::prompt::Environment& environment) {
  std::vector<std::string> out;
  for (const std::string_view name : kLinted) {
    for (const json* given : {static_cast<const json*>(nullptr), &context()}) {
      std::string text;
      for (const en::prompt::Finding& finding : environment.lint(name, given)) text += to_string(finding) + "\n";
      out.push_back(std::move(text));
    }
  }
  return out;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  // SAFETY: libFuzzer hands over `size` readable bytes, which live until this call returns; the view reads them as
  // the characters they are, and ends where they end.
  const std::string_view input(reinterpret_cast<const char*>(data), size);
  std::map<std::string, std::string> templates;
  std::size_t start = 0;
  for (std::size_t i = 0; i < kNames.size() && start <= input.size(); ++i) {
    const std::size_t end = std::min(input.find('\0', start), input.size());
    templates.emplace(std::string(kNames[i]), std::string(input.substr(start, end - start)));
    start = end + 1;
  }

  const en::prompt::Environment environment(std::make_shared<const en::prompt::MemorySource>(std::move(templates)));
  const std::vector<std::string> first = lint_all(environment);
  require(lint_all(environment) == first, "a template compiles alike whatever was compiled before it");
  return 0;
}
