// Built-ins safe on empty input (NPC §7.1): every registry function is called with empty strings, empty lists and
// the other empty values, in every position; each returns a value or fails with a located TemplateError.
#include "support.hpp"

using namespace en_test;

namespace {

// The empty value of each kind a parameter accepts.
std::vector<json> empty_values(prompt::Kinds kinds) {
  std::vector<json> out;
  if (kinds & prompt::kind::string) out.emplace_back("");
  if (kinds & prompt::kind::list) out.push_back(json::array());
  if (kinds & prompt::kind::object) out.push_back(json::object());
  if (kinds & prompt::kind::null) out.emplace_back(nullptr);
  if (kinds & prompt::kind::number) out.emplace_back(0);
  if (kinds & prompt::kind::boolean) out.emplace_back(false);
  return out;
}

}  // namespace

TEST_CASE("every registry function is defined on empty input") {
  const prompt::FunctionRegistry registry = prompt::builtin_registry();
  std::size_t calls = 0;
  std::size_t located_errors = 0;
  for (const auto& spec : registry.all()) {
    const std::size_t most = spec->variadic ? spec->params.size() + 2 : spec->max_args();
    for (std::size_t count = spec->min_args(); count <= most; ++count) {
      std::vector<std::vector<json>> choices;
      for (std::size_t i = 0; i < count; ++i) {
        choices.push_back(empty_values(i < spec->params.size() ? spec->params[i].kinds : spec->params.back().kinds));
      }
      // Every combination of the empty values, counted like an odometer.
      std::vector<std::size_t> pick(count, 0);
      for (bool more = true; more;) {
        json context = json::object();
        std::string call = spec->name + "(";
        for (std::size_t i = 0; i < count; ++i) {
          context["a" + std::to_string(i)] = choices[i][pick[i]];
          call += (i > 0 ? ", a" : "a") + std::to_string(i);
        }
        call += ")";
        const std::string source = "[ user ]\n({{ to_json(" + call + ") }})";
        CAPTURE(source);
        CAPTURE(context.dump());
        try {
          render_inline(source, context, "empty.j2");
        } catch (const prompt::TemplateError& error) {
          CHECK(error.site().template_name == "empty.j2");  // defined behaviour: a located error
          ++located_errors;
        }
        ++calls;
        more = false;
        for (std::size_t i = 0; i < count && !more; ++i) {
          if (++pick[i] < choices[i].size()) {
            more = true;
          } else {
            pick[i] = 0;
          }
        }
      }
    }
  }
  CHECK(calls > 150);
  CHECK(located_errors < calls);
}

TEST_CASE("text functions give what the reference says on empty input") {
  const json empty = {{"s", ""}, {"l", json::array()}, {"o", json::object()}};
  CHECK(user("({{ capitalize(s) }}|{{ upper(s) }}|{{ lower(s) }}|{{ trim(s) }}|{{ oneline(s) }}|"
             "{{ replace(s, \"a\", \"b\") }}|{{ join(l, \", \") }})",
             empty) == "(||||||)");
  CHECK(user("{{ quote(s) }} {{ length(s) }} {{ length(l) }} {{ length(o) }}", empty) == "\"\" 0 0 0");
  CHECK(user("{{ to_json(split(s, \",\")) }} {{ to_json(split(\"ab\", s)) }} {{ to_json(sort(l)) }} "
             "{{ to_json(range(0)) }}",
             empty) == "[] [\"a\",\"b\"] [] []");
  CHECK(user("{{ to_json(first(l)) }} {{ to_json(last(l)) }} {{ to_json(max(l)) }} {{ to_json(min(l)) }}", empty) ==
        "null null null null");
  CHECK(user("{{ contains(s, s) }} {{ contains(l, s) }} {{ starts_with(s, s) }} {{ ends_with(s, s) }} "
             "{{ empty(s) }} {{ empty(l) }} {{ empty(o) }}",
             empty) == "true false true true true true true");
  CHECK(user("{{ replace(\"abc\", s, \"x\") }} {{ get(o, \"k\", \"fallback\") }} {{ default(s, \"unused\") }}|",
             empty) == "abc fallback |");
  CHECK(says(failure("[ user ]\n{{ int(s) }}", empty), "int(): needs a whole number, got an empty string"));
  CHECK(says(failure("[ user ]\n{{ float(s) }}", empty), "float(): needs a number, got an empty string"));
  CHECK(says(failure("[ user ]\n{{ first(l) }}", empty), "is null"));
}

TEST_CASE("text functions keep UTF-8 whole and read numbers the same everywhere") {
  CHECK(user("{{ upper(\"élan\") }} {{ capitalize(\"élan\") }} {{ capitalize(\"hELLO wORLD\") }}") ==
        "éLAN élan Hello world");
  CHECK(user("{{ length(\"élan\") }} {{ to_json(split(\"aé\", \"\")) }}") == "4 [\"a\",\"é\"]");
  CHECK(user("{{ float(\"2.5\") }} {{ int(\" 42 \") }} {{ round(2.567, 2) }} {{ round(2.5) }}") == "2.5 42 2.57 3");
  CHECK(user("{{ quote(\"He said \\\"go\\\"\\nnow\") }} {{ oneline(\"  a \\n\\t b  \") }}") ==
        "\"He said 'go' now\" a b");
}
