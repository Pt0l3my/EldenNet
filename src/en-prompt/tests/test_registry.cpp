// The one function registry and what is generated from it: the reference and the lint's allowlist (NPC §7.1,
// WHY§98 point 8).
#include <stdexcept>

#include "support.hpp"

using namespace en_test;

TEST_CASE("the built-in registry holds inja's functions, made strict, and en-prompt's own") {
  const prompt::FunctionRegistry registry = prompt::builtin_registry();
  for (const char* name :
       {"at", "default", "exists", "existsIn", "first", "last", "length", "lower", "upper", "capitalize", "join",
        "max", "min", "sort", "range", "round", "int", "float", "even", "odd", "divisibleBy", "isArray", "isBoolean",
        "isFloat", "isInteger", "isNumber", "isObject", "isString", "empty", "contains", "starts_with", "ends_with",
        "trim", "oneline", "quote", "replace", "split", "get", "object", "to_json", "render_template",
        "render_subcomponent"}) {
    CAPTURE(name);
    CHECK(registry.find(name) != nullptr);
  }
  CHECK(registry.find("super") == nullptr);  // inja's, but not ours: refused at load
  CHECK(registry.find("render_template")->markup);
  CHECK_FALSE(registry.find("render_template")->cacheable);
  CHECK(registry.find("capitalize")->cacheable);
}

TEST_CASE("the reference and the allowlist are generated from the registry") {
  const prompt::FunctionRegistry registry = prompt::builtin_registry();
  const std::string reference = prompt::reference_markdown(registry);
  const json allowlist = prompt::allowlist_json(registry);

  REQUIRE(allowlist["functions"].size() == registry.size());
  std::size_t rows = 0;
  for (std::size_t at = reference.find("\n| `"); at != std::string::npos; at = reference.find("\n| `", at + 1)) ++rows;
  CHECK(rows == registry.size());
  for (const auto& spec : registry.all()) {
    CAPTURE(spec->name);
    CHECK(reference.find("| `" + prompt::signature(*spec) + "` |") != std::string::npos);
  }
  for (const json& entry : allowlist["functions"]) {
    CHECK(registry.find(entry["name"].get<std::string>()) != nullptr);
    if (entry["name"] == "round") {
      CHECK(entry["min_args"] == 1);
      CHECK(entry["max_args"] == 2);
      CHECK(entry["returns"] == json::array({"number"}));
    }
    if (entry["name"] == "object") {
      CHECK(entry["variadic"] == true);
      CHECK(entry["max_args"].is_null());
    }
    if (entry["name"] == "render_template") CHECK(entry["markup"] == true);
  }
}

TEST_CASE("signatures show optional and repeating parameters") {
  const prompt::FunctionRegistry registry = prompt::builtin_registry();
  CHECK(prompt::signature(*registry.find("round")) == "round(number[, digits])");
  CHECK(prompt::signature(*registry.find("render_subcomponent")) == "render_subcomponent(folder[, mode, vars])");
  CHECK(prompt::signature(*registry.find("object")) == "object([key, value], ...)");
  CHECK(prompt::signature(*registry.find("at")) == "at(container, key)");
}

TEST_CASE("kinds are described in words") {
  CHECK(prompt::describe(prompt::kind::string) == "a string");
  CHECK(prompt::describe(prompt::kind::string | prompt::kind::list) == "a string or a list");
  CHECK(prompt::describe(prompt::kind::null | prompt::kind::number | prompt::kind::object) ==
        "null, a number or an object");
  CHECK(prompt::describe(prompt::kind::any) == "any value");
  CHECK(prompt::kind_names(prompt::kind::number | prompt::kind::string) == "number or string");
  CHECK(prompt::kind_of(json(1.5)) == prompt::kind::number);
  CHECK(prompt::kind_of(json::array()) == prompt::kind::list);
}

TEST_CASE("a registered function is callable, checked, located and listed") {
  prompt::FunctionRegistry registry = prompt::builtin_registry();
  registry.add({.name = "title_of",
                .params = {{"id", prompt::kind::string}},
                .returns = prompt::kind::string,
                .summary = "A test function: the title kept for an id.",
                .cacheable = true,
                .implementation = [](const prompt::Call& call) -> json {
                  if (call[0] == "orla") return "the bridge-keeper";
                  call.fail("no title for this id");
                }});
  const TempTree tree;
  const prompt::Environment environment(tree.root(), registry);
  CHECK(environment.render_source("[ user ]\n{{ title_of(id) }}", json{{"id", "orla"}})[0].text() ==
        "the bridge-keeper");

  const auto wrong_kind = error_of([&] { environment.render_source("[ user ]\n{{ title_of(3) }}", json::object()); });
  CHECK(wrong_kind.message() == "title_of(): id must be a string, got a number");
  const auto refused = error_of(
      [&] { environment.render_source("[ user ]\n\n{{ title_of(\"nobody\") }}", json::object(), "custom.j2"); });
  CHECK(refused.message() == "title_of(): no title for this id");
  CHECK(refused.site().template_name == "custom.j2");
  CHECK(refused.site().line == 3);

  CHECK(prompt::reference_markdown(registry).find("`title_of(id)`") != std::string::npos);
  CHECK(says(error_of([] { render_inline("[ user ]\n{{ title_of(\"orla\") }}"); }), "unknown function 'title_of'"));
}

TEST_CASE("a function without arguments is called with brackets") {
  prompt::FunctionRegistry registry = prompt::builtin_registry();
  registry.add({.name = "hour_word",
                .params = {},
                .returns = prompt::kind::string,
                .summary = "A test function: a time of day.",
                .implementation = [](const prompt::Call&) -> json { return "dusk"; }});
  const TempTree tree;
  const prompt::Environment environment(tree.root(), registry);
  CHECK(environment.render_source("[ user ]\n{{ hour_word() }}", json::object())[0].text() == "dusk");
  CHECK(says(error_of([&] { environment.render_source("[ user ]\n{{ hour_word }}", json::object()); }),
             "'hour_word' is a function: call it as hour_word()"));
}

TEST_CASE("a malformed or duplicate function is refused") {
  prompt::FunctionRegistry registry = prompt::builtin_registry();
  const auto spec = [](std::string name) {
    return prompt::FunctionSpec{.name = std::move(name),
                                .params = {},
                                .returns = prompt::kind::string,
                                .summary = "A test function.",
                                .implementation = [](const prompt::Call&) -> json { return ""; }};
  };
  CHECK_THROWS_AS(registry.add(spec("upper")), std::invalid_argument);       // already there
  CHECK_THROWS_AS(registry.add(spec("not")), std::invalid_argument);         // a reserved word
  CHECK_THROWS_AS(registry.add(spec("two words")), std::invalid_argument);   // not a name
  CHECK_THROWS_AS(registry.add(spec("__check")), std::invalid_argument);     // the engine's own
  prompt::FunctionSpec silent = spec("silent");
  silent.implementation = nullptr;
  CHECK_THROWS_AS(registry.add(silent), std::invalid_argument);
  prompt::FunctionSpec greedy = spec("greedy");
  greedy.required = 2;
  CHECK_THROWS_AS(registry.add(greedy), std::invalid_argument);
  prompt::FunctionSpec wordy = spec("wordy");
  wordy.summary = "two\nlines";
  CHECK_THROWS_AS(registry.add(wordy), std::invalid_argument);
  CHECK_NOTHROW(registry.add(spec("fine")));
}
