// The built-in functions: inja's own, made strict and safe on empty input, and en-prompt's additions (NPC §7.1).
// Every one is defined on an empty string and an empty list; the tests call each with them.
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

#include "detail.hpp"

namespace en::prompt {

namespace detail {

std::optional<bool> holds(const nlohmann::json& container, const nlohmann::json& value) {
  if (container.is_string()) {
    if (!value.is_string()) return std::nullopt;
    return container.get_ref<const std::string&>().find(value.get_ref<const std::string&>()) != std::string::npos;
  }
  if (container.is_array()) return std::find(container.begin(), container.end(), value) != container.end();
  if (container.is_object()) {
    if (!value.is_string()) return std::nullopt;
    return container.contains(value.get_ref<const std::string&>());
  }
  return std::nullopt;
}

}  // namespace detail

namespace {

using nlohmann::json;
using detail::is_space;

const std::string& text_of(const Call& call, std::size_t i) { return call[i].get_ref<const std::string&>(); }

// Case functions change ASCII letters only, so a UTF-8 sequence is never broken.
char ascii_upper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }
char ascii_lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

std::string oneline(std::string_view text) {
  std::string out;
  bool pending_space = false;
  for (char c : text) {
    if (is_space(c)) {
      pending_space = !out.empty();
      continue;
    }
    if (pending_space) out += ' ';
    pending_space = false;
    out += c;
  }
  return out;
}

std::int64_t whole_number(const Call& call, std::size_t i, const char* what) {
  const json& value = call[i];
  if (!value.is_number_integer()) call.fail(std::string(what) + " must be a whole number");
  return value.get<std::int64_t>();
}

// Max, min and sort need values of one kind that order: all numbers or all strings.
void require_orderable(const Call& call, const json& list) {
  const bool numbers = std::all_of(list.begin(), list.end(), [](const json& v) { return v.is_number(); });
  const bool strings = std::all_of(list.begin(), list.end(), [](const json& v) { return v.is_string(); });
  if (!numbers && !strings) call.fail("needs a list of numbers or a list of strings");
}

std::string element_text(const Call& call, const json& value, std::size_t index) {
  if (value.is_string()) return value.get<std::string>();
  if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
  if (value.is_number()) return value.dump();
  call.fail("element " + std::to_string(index + 1) + " is " + describe(kind_of(value)) + ", which has no text");
}

json add_vars(json context, const json& vars) {
  for (auto it = vars.begin(); it != vars.end(); ++it) context[it.key()] = it.value();
  return context;
}

void add_lookup_functions(FunctionRegistry& r) {
  r.add({.name = "at",
         .params = {{"container", kind::list | kind::object}, {"key", kind::number | kind::string}},
         .returns = kind::any,
         .summary = "The element of a list at an index (from 0), or the value of an object's key; one that is not "
                    "there is an error.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const json& container = call[0];
           if (container.is_array()) {
             const std::int64_t index = whole_number(call, 1, "a list's index");
             if (index < 0 || static_cast<std::uint64_t>(index) >= container.size()) {
               call.fail("index " + std::to_string(index) + " is outside a list of " +
                         std::to_string(container.size()));
             }
             return container[static_cast<std::size_t>(index)];
           }
           if (!call[1].is_string()) call.fail("an object's key must be a string");
           const auto it = container.find(text_of(call, 1));
           if (it == container.end()) call.fail("the object has no key '" + text_of(call, 1) + "'");
           return *it;
         }});
  r.add({.name = "default",
         .params = {{"value", kind::any}, {"fallback", kind::any}},
         .returns = kind::any,
         .summary = "The value, or the fallback when the value is null (a field left unset); a name the context "
                    "lacks is still an error.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json { return call[0].is_null() ? call[1] : call[0]; }});
  r.add({.name = "exists",
         .params = {{"name", kind::string}},
         .returns = kind::boolean,
         .summary = "Whether the render's context holds a name, a dotted path such as \"npc.title\", even as null; "
                    "loop and set names are not looked at.",
         .implementation = [](const Call& call) -> json {
           const std::string& name = text_of(call, 0);
           if (name.empty()) return false;
           return call.context().contains(json::json_pointer(detail::dotted_pointer(name)));
         }});
  r.add({.name = "existsIn",
         .params = {{"object", kind::object}, {"key", kind::string}},
         .returns = kind::boolean,
         .summary = "Whether an object holds a key, even with a null value.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json { return call[0].contains(text_of(call, 1)); }});
  r.add({.name = "get",
         .params = {{"object", kind::object}, {"key", kind::string}, {"fallback", kind::any}},
         .returns = kind::any,
         .summary = "An object's value for a key, or the fallback when the key is absent or null: for records whose "
                    "keys vary.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const auto it = call[0].find(text_of(call, 1));
           return (it == call[0].end() || it->is_null()) ? call[2] : *it;
         }});
  r.add({.name = "first",
         .params = {{"list", kind::list}},
         .returns = kind::any,
         .summary = "The first element of a list; null for an empty list.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json { return call[0].empty() ? json() : call[0].front(); }});
  r.add({.name = "last",
         .params = {{"list", kind::list}},
         .returns = kind::any,
         .summary = "The last element of a list; null for an empty list.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json { return call[0].empty() ? json() : call[0].back(); }});
  r.add({.name = "length",
         .params = {{"value", kind::string | kind::list | kind::object | kind::null}},
         .returns = kind::number,
         .summary = "The number of characters in a string, elements in a list or keys in an object; 0 for null.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const json& value = call[0];
           if (value.is_string()) return detail::utf8_length(value.get_ref<const std::string&>());
           return value.is_null() ? 0 : value.size();
         }});
  r.add({.name = "empty",
         .params = {{"value", kind::any}},
         .returns = kind::boolean,
         .summary = "True for null, \"\", [] and {}; false for any other value, 0 and false included: the test for "
                    "an optional field.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const json& value = call[0];
           if (value.is_null()) return true;
           // nlohmann's empty() is false for every string, "" included, so a string is measured itself.
           if (value.is_string()) return value.get_ref<const std::string&>().empty();
           if (value.is_array() || value.is_object()) return value.empty();
           return false;
         }});
  r.add({.name = "contains",
         .params = {{"container", kind::string | kind::list | kind::object}, {"value", kind::any}},
         .returns = kind::boolean,
         .summary = "A real substring test on a string, a member test on a list, a key test on an object (as "
                    "`value in container`).",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const std::optional<bool> found = detail::holds(call[0], call[1]);
           if (!found) call.fail("can't look for " + describe(kind_of(call[1])) + " in " + describe(kind_of(call[0])));
           return *found;
         }});
}

void add_text_functions(FunctionRegistry& r) {
  r.add({.name = "lower",
         .params = {{"text", kind::string}},
         .returns = kind::string,
         .summary = "The text in lower case (ASCII letters only).",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           std::string out = text_of(call, 0);
           std::transform(out.begin(), out.end(), out.begin(), ascii_lower);
           return out;
         }});
  r.add({.name = "upper",
         .params = {{"text", kind::string}},
         .returns = kind::string,
         .summary = "The text in upper case (ASCII letters only).",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           std::string out = text_of(call, 0);
           std::transform(out.begin(), out.end(), out.begin(), ascii_upper);
           return out;
         }});
  r.add({.name = "capitalize",
         .params = {{"text", kind::string}},
         .returns = kind::string,
         .summary = "The first letter in upper case and the rest in lower case (ASCII letters only); \"\" gives \"\".",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           std::string out = text_of(call, 0);
           if (out.empty()) return out;
           out[0] = ascii_upper(out[0]);
           std::transform(out.begin() + 1, out.end(), out.begin() + 1, ascii_lower);
           return out;
         }});
  r.add({.name = "trim",
         .params = {{"text", kind::string}},
         .returns = kind::string,
         .summary = "The text without white space at either end.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json { return std::string(detail::trim(text_of(call, 0))); }});
  r.add({.name = "oneline",
         .params = {{"text", kind::string}},
         .returns = kind::string,
         .summary = "The text on one line: every run of white space, line breaks included, becomes one space.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json { return oneline(text_of(call, 0)); }});
  r.add({.name = "quote",
         .params = {{"text", kind::string}},
         .returns = kind::string,
         .summary = "Someone's words as one quoted line: double quotes around, inner double quotes made single, "
                    "line breaks made spaces.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           // One line, closed by our own quote mark: a record cannot end its quotation early or start a new
           // section of the prompt (records enter as quoted data, NPC §7.1).
           std::string out = oneline(text_of(call, 0));
           std::replace(out.begin(), out.end(), '"', '\'');
           return "\"" + out + "\"";
         }});
  r.add({.name = "replace",
         .params = {{"text", kind::string}, {"from", kind::string}, {"to", kind::string}},
         .returns = kind::string,
         .summary = "The text with every occurrence of `from` replaced by `to`; an empty `from` changes nothing.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           std::string out = text_of(call, 0);
           const std::string& from = text_of(call, 1);
           const std::string& to = text_of(call, 2);
           if (from.empty()) return out;
           for (std::size_t at = out.find(from); at != std::string::npos; at = out.find(from, at + to.size())) {
             out.replace(at, from.size(), to);
           }
           return out;
         }});
  r.add({.name = "split",
         .params = {{"text", kind::string}, {"separator", kind::string}},
         .returns = kind::list,
         .summary = "The text cut at every separator; \"\" gives [], and an empty separator gives the characters.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const std::string& text = text_of(call, 0);
           const std::string& separator = text_of(call, 1);
           json out = json::array();
           if (text.empty()) return out;
           if (separator.empty()) {
             for (std::string& character : detail::utf8_characters(text)) out.push_back(std::move(character));
             return out;
           }
           std::size_t start = 0;
           for (std::size_t at = text.find(separator); at != std::string::npos; at = text.find(separator, start)) {
             out.push_back(text.substr(start, at - start));
             start = at + separator.size();
           }
           out.push_back(text.substr(start));
           return out;
         }});
  r.add({.name = "starts_with",
         .params = {{"text", kind::string}, {"prefix", kind::string}},
         .returns = kind::boolean,
         .summary = "Whether the text starts with the prefix; every text starts with \"\".",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           return std::string_view(text_of(call, 0)).substr(0, text_of(call, 1).size()) == text_of(call, 1);
         }});
  r.add({.name = "ends_with",
         .params = {{"text", kind::string}, {"suffix", kind::string}},
         .returns = kind::boolean,
         .summary = "Whether the text ends with the suffix; every text ends with \"\".",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const std::string& text = text_of(call, 0);
           const std::string& suffix = text_of(call, 1);
           return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
         }});
  r.add({.name = "join",
         .params = {{"list", kind::list}, {"separator", kind::string}},
         .returns = kind::string,
         .summary = "The elements of a list as one text with the separator between them; [] gives \"\".",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           std::string out;
           const json& list = call[0];
           for (std::size_t i = 0; i < list.size(); ++i) {
             if (i > 0) out += text_of(call, 1);
             out += element_text(call, list[i], i);
           }
           return out;
         }});
  r.add({.name = "to_json",
         .params = {{"value", kind::any}},
         .returns = kind::string,
         .summary = "The value as compact JSON text, for a JSON example or a record written into a prompt.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json { return call[0].dump(); }});
}

void add_list_and_number_functions(FunctionRegistry& r) {
  r.add({.name = "max",
         .params = {{"list", kind::list}},
         .returns = kind::number | kind::string | kind::null,
         .summary = "The largest element of a list of numbers or of strings; null for an empty list.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const json& list = call[0];
           if (list.empty()) return json();
           require_orderable(call, list);
           return *std::max_element(list.begin(), list.end());
         }});
  r.add({.name = "min",
         .params = {{"list", kind::list}},
         .returns = kind::number | kind::string | kind::null,
         .summary = "The smallest element of a list of numbers or of strings; null for an empty list.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const json& list = call[0];
           if (list.empty()) return json();
           require_orderable(call, list);
           return *std::min_element(list.begin(), list.end());
         }});
  r.add({.name = "sort",
         .params = {{"list", kind::list}},
         .returns = kind::list,
         .summary = "A list of numbers or of strings in ascending order; [] gives [].",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           json out = call[0];
           require_orderable(call, out);
           std::sort(out.begin(), out.end());
           return out;
         }});
  r.add({.name = "object",
         .params = {{"key", kind::string}, {"value", kind::any}},
         .required = 0,
         .variadic = true,
         .returns = kind::object,
         .summary = "An object from key and value pairs, for the variables given to render_template; object() is "
                    "{}.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           if (call.size() % 2 != 0) call.fail("takes keys and values in pairs");
           json out = json::object();
           for (std::size_t i = 0; i < call.size(); i += 2) {
             if (!call[i].is_string()) call.fail("argument " + std::to_string(i + 1) + " must be a key (a string)");
             const std::string& key = text_of(call, i);
             if (out.contains(key)) call.fail("the key '" + key + "' is given twice");
             out[key] = call[i + 1];
           }
           return out;
         }});
  r.add({.name = "range",
         .params = {{"count", kind::number}},
         .returns = kind::list,
         .summary = "The whole numbers from 0 to count - 1; a count of 0 or less gives [].",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const std::int64_t count = whole_number(call, 0, "the count");
           json out = json::array();
           if (count > 100000) call.fail("a range of more than 100000 numbers");
           for (std::int64_t i = 0; i < count; ++i) out.push_back(i);
           return out;
         }});
  r.add({.name = "round",
         .params = {{"number", kind::number}, {"digits", kind::number}},
         .required = 1,
         .returns = kind::number,
         .summary = "The number rounded to a number of decimal digits (0 by default, giving a whole number).",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const std::int64_t digits = call.size() > 1 ? whole_number(call, 1, "the digits") : 0;
           if (digits < 0 || digits > 12) call.fail("the digits must be from 0 to 12");
           const double scale = std::pow(10.0, static_cast<double>(digits));
           const double rounded = std::round(call[0].get<double>() * scale) / scale;
           if (digits > 0) return rounded;
           if (!(std::fabs(rounded) < 9.0e18)) call.fail("the number is too large for a whole number");
           return static_cast<std::int64_t>(rounded);
         }});
  r.add({.name = "int",
         .params = {{"value", kind::number | kind::string}},
         .returns = kind::number,
         .summary = "A whole number from a number (cut towards zero) or from text that is one; other text, \"\" "
                    "included, is an error.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const json& value = call[0];
           if (value.is_number_integer()) return value;
           if (value.is_number_float()) {
             const double number = value.get<double>();
             if (!(std::fabs(number) < 9.0e18)) call.fail("the number is too large for a whole number");
             return static_cast<std::int64_t>(number);
           }
           const std::string_view text = detail::trim(value.get_ref<const std::string&>());
           if (text.empty()) call.fail("needs a whole number, got an empty string");
           std::int64_t number = 0;
           const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
           if (error != std::errc() || end != text.data() + text.size()) call.fail("needs a whole number");
           return number;
         }});
  r.add({.name = "float",
         .params = {{"value", kind::number | kind::string}},
         .returns = kind::number,
         .summary = "A number from a number or from text that is one, read the same way in every locale; other "
                    "text, \"\" included, is an error.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const json& value = call[0];
           if (value.is_number()) return value.get<double>();
           const std::string_view text = detail::trim(value.get_ref<const std::string&>());
           if (text.empty()) call.fail("needs a number, got an empty string");
           double number = 0;
           const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
           if (error != std::errc() || end != text.data() + text.size() || !std::isfinite(number)) {
             call.fail("needs a number");
           }
           return number;
         }});
  r.add({.name = "even",
         .params = {{"number", kind::number}},
         .returns = kind::boolean,
         .summary = "Whether a whole number is even.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json { return whole_number(call, 0, "the number") % 2 == 0; }});
  r.add({.name = "odd",
         .params = {{"number", kind::number}},
         .returns = kind::boolean,
         .summary = "Whether a whole number is odd.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json { return whole_number(call, 0, "the number") % 2 != 0; }});
  r.add({.name = "divisibleBy",
         .params = {{"number", kind::number}, {"divisor", kind::number}},
         .returns = kind::boolean,
         .summary = "Whether a whole number divides by another; nothing divides by 0.",
         .cacheable = true,
         .implementation = [](const Call& call) -> json {
           const std::int64_t divisor = whole_number(call, 1, "the divisor");
           return divisor != 0 && whole_number(call, 0, "the number") % divisor == 0;
         }});
}

void add_type_tests(FunctionRegistry& r) {
  struct TypeTest {
    const char* name;
    const char* summary;
    bool (*test)(const json&);
  };
  static const TypeTest tests[] = {
      {"isArray", "Whether the value is a list.", [](const json& v) { return v.is_array(); }},
      {"isBoolean", "Whether the value is a boolean.", [](const json& v) { return v.is_boolean(); }},
      {"isFloat", "Whether the value is a number with a fraction part.", [](const json& v) { return v.is_number_float(); }},
      {"isInteger", "Whether the value is a whole number.", [](const json& v) { return v.is_number_integer(); }},
      {"isNumber", "Whether the value is a number.", [](const json& v) { return v.is_number(); }},
      {"isObject", "Whether the value is an object.", [](const json& v) { return v.is_object(); }},
      {"isString", "Whether the value is a string.", [](const json& v) { return v.is_string(); }},
  };
  for (const TypeTest& t : tests) {
    const auto test = t.test;
    r.add({.name = t.name,
           .params = {{"value", kind::any}},
           .returns = kind::boolean,
           .summary = t.summary,
           .cacheable = true,
           .implementation = [test](const Call& call) -> json { return test(call[0]); }});
  }
}

void add_composition_functions(FunctionRegistry& r) {
  r.add({.name = "render_template",
         .params = {{"name", kind::string}, {"vars", kind::object}},
         .required = 1,
         .returns = kind::string,
         .summary = "Renders another template, or a folder of numbered parts, by name from the root (\"/\" or \"\\\" "
                    "between folders), seeing the context and the vars but not the caller's set names.",
         .markup = true,
         .implementation = [](const Call& call) -> json {
           if (call.size() < 2 || call[1].empty()) return call.render(text_of(call, 0), call.context());
           return call.render(text_of(call, 0), add_vars(call.context(), call[1]));
         }});
  r.add({.name = "render_subcomponent",
         .params = {{"folder", kind::string}, {"mode", kind::string | kind::null}, {"vars", kind::object}},
         .required = 1,
         .returns = kind::string,
         .summary = "Renders a folder of numbered parts by name from the root, with render_mode set to the mode "
                    "when one is given.",
         .markup = true,
         .implementation = [](const Call& call) -> json {
           const bool has_mode = call.size() > 1 && call[1].is_string();
           const bool has_vars = call.size() > 2 && !call[2].empty();
           if (!has_mode && !has_vars) return call.render(text_of(call, 0), call.context(), true);
           json context = has_vars ? add_vars(call.context(), call[2]) : call.context();
           if (has_mode) context["render_mode"] = call[1];
           return call.render(text_of(call, 0), context, true);
         }});
}

}  // namespace

FunctionRegistry builtin_registry() {
  FunctionRegistry registry;
  add_lookup_functions(registry);
  add_text_functions(registry);
  add_list_and_number_functions(registry);
  add_type_tests(registry);
  add_composition_functions(registry);
  return registry;
}

}  // namespace en::prompt
