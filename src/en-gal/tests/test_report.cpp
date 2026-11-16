// The numbers-only writer and the lint (report.hpp), over a vocabulary of the tests' own: what the writer refuses never
// reaches its text, what it writes lints clean, and the lint refuses a path, a free string, a byte run longer than a
// pattern and anything else outside the vocabulary, wherever the text came from.
#include <doctest.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/patterns.hpp"
#include "en/gal/report.hpp"

namespace {

using en::gal::BytePattern;
using en::gal::parse_pattern;
using en::gal::report::Key;
using en::gal::report::lint;
using en::gal::report::Type;
using en::gal::report::Vocabulary;
using en::gal::report::Writer;

constexpr std::array<std::string_view, 3> kColours = {"red", "green", "blue"};
constexpr std::array<Key, 10> kKeys = {{
    {"size", Type::Unsigned},
    {"delta", Type::Signed},
    {"ok", Type::Boolean},
    {"colour", Type::Word, Type::Object, kColours},
    {"label", Type::Label},
    {"bytes", Type::Pattern},
    {"part", Type::Object},
    {"parts", Type::Array, Type::Object},
    {"rvas", Type::Array, Type::Unsigned},
    {"colours", Type::Array, Type::Word, kColours},
}};
constexpr Vocabulary kVocabulary{"sample", 1, kKeys};
constexpr std::array<const Vocabulary*, 1> kKnown = {&kVocabulary};

BytePattern pat(const char* text) {
  auto p = parse_pattern(text);
  REQUIRE(p.has_value());
  return *p;
}

std::string written() {
  Writer w(kVocabulary);
  w.number("size", 42);
  w.signed_number("delta", -7);
  w.boolean("ok", true);
  w.word("colour", "green");
  w.label("label", "task_loom");
  w.pattern("bytes", pat("48 8B 1D ?? ?? ?? ?? 48 85 DB"));
  w.begin_object("part");
  w.number("size", 0);
  w.end_object();
  w.begin_array("parts");
  w.begin_object();
  w.label("label", "first");
  w.begin_array("rvas");
  w.number(0x1000);
  w.number(0x2000);
  w.end_array();
  w.end_object();
  w.begin_object();
  w.end_object();
  w.end_array();
  w.begin_array("colours");
  w.word("red");
  w.word("blue");
  w.end_array();
  auto text = w.finish();
  REQUIRE(text.has_value());
  return *text;
}

std::vector<std::string> problems(std::string_view text) {
  const auto r = lint(text, kKnown);
  std::vector<std::string> out;
  if (!r) {
    for (const auto& p : r.error()) out.push_back(en::gal::report::describe(p));
  }
  return out;
}

bool lints_clean(std::string_view text) { return problems(text).empty(); }

// The sample with `from` replaced by `to`.
std::string edited(std::string from, std::string to) {
  std::string text = written();
  const std::size_t at = text.find(from);
  REQUIRE(at != std::string::npos);
  text.replace(at, from.size(), to);
  return text;
}

}  // namespace

TEST_CASE("report: what the writer writes lints clean, pretty-printed") {
  const std::string text = written();
  CHECK(text.rfind("{\n  \"report\": \"sample\",\n  \"schema\": 1,\n  \"size\": 42,", 0) == 0);
  CHECK(text.find("\"rvas\": [4096, 8192]") != std::string::npos);
  CHECK(text.find("\"colours\": [\"red\", \"blue\"]") != std::string::npos);
  CHECK(text.find("\"bytes\": \"48 8B 1D ?? ?? ?? ?? 48 85 DB\"") != std::string::npos);
  CHECK(text.back() == '\n');
  CHECK(lints_clean(text));
}

TEST_CASE("report: the writer refuses what is outside the vocabulary, and writes nothing after") {
  const auto refusal = [](auto&& write) {
    Writer w(kVocabulary);
    w.number("size", 1);
    write(w);
    w.number("size", 2);  // ignored once refused
    const auto r = w.finish();
    REQUIRE_FALSE(r.has_value());
    return en::gal::report::describe(r.error());
  };
  CHECK(refusal([](Writer& w) { w.number("path", 1); }) == "path: is not a key of this report's vocabulary");
  CHECK(refusal([](Writer& w) { w.word("size", "red"); }) == "size: must be a whole number of 0 or more");
  CHECK(refusal([](Writer& w) { w.word("colour", "purple"); }) == "colour: is not one of its words");
  CHECK(refusal([](Writer& w) { w.label("label", "C:\\Users"); }) == "label: is not a label");
  CHECK(refusal([](Writer& w) { w.label("label", "TaskLoom"); }) == "label: is not a label");
  CHECK(refusal([](Writer& w) { w.label("label", "two words"); }) == "label: is not a label");
  CHECK(refusal([](Writer& w) { w.label("label", std::string(49, 'a')); }) == "label: is not a label");
  CHECK(refusal([](Writer& w) { w.number("report", 1); }) == "report: is the report's own key");
  CHECK(refusal([](Writer& w) { w.begin_object("part"); }) == "an object or array was left open");
  CHECK(refusal([](Writer& w) { w.end_array(); }) == "end_array without an array open");
  CHECK(refusal([](Writer& w) {
          w.begin_array("rvas");
          w.word("red");
        }) == "rvas[0]: must be a whole number of 0 or more");
  CHECK(refusal([](Writer& w) {
          w.begin_array("rvas");
          w.number("size", 3);
        }) == "rvas[0]: an array's item takes no key");
  CHECK(refusal([](Writer& w) {
          w.begin_object("part");
          w.number(3);
        }) == "part: an object's value needs a key");

  // A byte run longer than a pattern is no pattern.
  BytePattern run;
  run.value.assign(65, 0x90);
  run.mask.assign(65, 0xFF);
  CHECK(refusal([&](Writer& w) { w.pattern("bytes", run); }) ==
        "bytes: is not a pattern of 1 to 64 bytes with 4 fixed");
  BytePattern loose = pat("90 90 90 90");
  loose.mask[0] = 0;
  CHECK(refusal([&](Writer& w) { w.pattern("bytes", loose); }) ==
        "bytes: is not a pattern of 1 to 64 bytes with 4 fixed");
  CHECK(refusal([&](Writer& w) {
          w.begin_array("parts");
          for (int i = 0; i < 65; ++i) {
            w.begin_object();
            w.pattern("bytes", pat("48 8B 1D 01"));
            w.end_object();
          }
        }) == "parts[64].bytes: one pattern too many for a report");
}

TEST_CASE("report: the lint refuses a path, a free string and a long byte run") {
  CHECK(problems(edited("\"task_loom\"", "\"C:\\\\Users\\\\someone\\\\game.exe\"")) ==
        std::vector<std::string>{"label: is not a label"});
  CHECK(problems(edited("\"task_loom\"", "\"/home/someone/game.exe\"")) ==
        std::vector<std::string>{"label: is not a label"});
  CHECK(problems(edited("\"green\"", "\"Some free text\"")) ==
        std::vector<std::string>{"colour: is not one of its words"});
  CHECK(problems(edited("\"size\": 42", "\"size\": \"42\"")) ==
        std::vector<std::string>{"size: is a string where the report holds a whole number of 0 or more"});
  std::string run;
  for (int i = 0; i < 80; ++i) run += i == 0 ? "90" : " 90";
  CHECK(problems(edited("\"48 8B 1D ?? ?? ?? ?? 48 85 DB\"", "\"" + run + "\"")) ==
        std::vector<std::string>{"bytes: is not a pattern of 1 to 64 bytes with 4 fixed"});
  CHECK(problems(edited("\"48 8B 1D ?? ?? ?? ?? 48 85 DB\"", "\"48 8b 1d ?? ?? ?? ?? 48 85 db\"")) ==
        std::vector<std::string>{"bytes: is not a pattern in canonical form"});
  CHECK(problems(edited("\"48 8B 1D ?? ?? ?? ?? 48 85 DB\"", "\"48  8B 1D ?? ?? ?? ?? 48 85 DB\"")) ==
        std::vector<std::string>{"bytes: is not a pattern in canonical form"});
}

TEST_CASE("report: the lint refuses every other kind of value or key outside the vocabulary") {
  CHECK(problems(edited("\"size\": 42", "\"size\": 4.5")) ==
        std::vector<std::string>{"size: a number that is not whole"});
  CHECK(problems(edited("\"size\": 42", "\"size\": null")) ==
        std::vector<std::string>{"size: null is not a value a report holds"});
  CHECK(problems(edited("\"size\": 42", "\"size\": -1")) ==
        std::vector<std::string>{"size: must be a whole number of 0 or more"});
  CHECK(problems(edited("\"size\": 42", "\"name\": 42")) ==
        std::vector<std::string>{"name: is not a key of this report's vocabulary"});
  CHECK(problems(edited("\"delta\": -7", "\"size\": 7")) == std::vector<std::string>{"size: is a key given twice"});
  CHECK(problems(edited("\"ok\": true", "\"ok\": 1")) == std::vector<std::string>{"ok: must be true or false"});
  CHECK(problems(edited("\"rvas\": [4096, 8192]", "\"rvas\": [4096, \"C:\\\\x\"]")) ==
        std::vector<std::string>{"parts[0].rvas[1]: is a string where the report holds a whole number of 0 or more"});
  CHECK(problems(edited("\"part\": {", "\"part\": [")).size() >= 1);
  CHECK(problems(edited("\"report\": \"sample\"", "\"report\": \"other\"")).front() ==
        "report: is not a kind of report this tool knows");
  CHECK(problems(edited("\"schema\": 1", "\"schema\": 2")) ==
        std::vector<std::string>{"schema: is not the schema this tool reads"});
  CHECK(
      problems(edited("\"report\": \"sample\",\n  \"schema\": 1,", "\"schema\": 1, \"report\": \"sample\",")).size() >=
      1);
  CHECK(problems("[]") == std::vector<std::string>{"a report is one JSON object"});
  CHECK(problems("{\"report\": \"sample\", \"schema\": 1") == std::vector<std::string>{"is not JSON"});
  CHECK(problems(written() + "{}") == std::vector<std::string>{"is not JSON"});
  CHECK(problems("") == std::vector<std::string>{"is not JSON"});
  CHECK(problems(std::string(300 << 10, ' ')) == std::vector<std::string>{"is larger than 256 KiB"});
  std::string deep = "{\"report\": \"sample\", \"schema\": 1, \"part\": ";
  for (int i = 0; i < 20; ++i) deep += "{\"part\": ";
  deep += "{}";
  for (int i = 0; i < 21; ++i) deep += "}";
  deep += "}";
  CHECK(problems(deep).back().find("nested too deep") != std::string::npos);
  // A repeated key cannot hide a string behind a later value, as a parsed document would.
  CHECK(problems(edited("\"label\": \"task_loom\"", "\"label\": \"C:\\\\x\", \"label\": \"task_loom\"")) ==
        std::vector<std::string>{"label: is not a label", "label: is a key given twice"});
}
