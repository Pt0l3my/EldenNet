// The function registry, calls, and the reference and allowlist generated from it.
#include "en/prompt/registry.hpp"

#include <stdexcept>
#include <utility>

#include "detail.hpp"

namespace en::prompt {

Kinds kind_of(const nlohmann::json& value) {
  switch (value.type()) {
    case nlohmann::json::value_t::null:
      return kind::null;
    case nlohmann::json::value_t::boolean:
      return kind::boolean;
    case nlohmann::json::value_t::number_integer:
    case nlohmann::json::value_t::number_unsigned:
    case nlohmann::json::value_t::number_float:
      return kind::number;
    case nlohmann::json::value_t::string:
      return kind::string;
    case nlohmann::json::value_t::array:
      return kind::list;
    case nlohmann::json::value_t::object:
      return kind::object;
    default:
      return 0;
  }
}

namespace {

struct KindWords {
  Kinds kind;
  const char* noun;     // for the reference
  const char* phrase;   // for messages
};

constexpr KindWords kind_words[] = {
    {kind::null, "null", "null"},           {kind::boolean, "boolean", "a boolean"},
    {kind::number, "number", "a number"},   {kind::string, "string", "a string"},
    {kind::list, "list", "a list"},         {kind::object, "object", "an object"},
};

std::string join_words(Kinds kinds, bool phrases) {
  std::vector<std::string> words;
  for (const KindWords& w : kind_words) {
    if (kinds & w.kind) words.emplace_back(phrases ? w.phrase : w.noun);
  }
  std::string out;
  for (std::size_t i = 0; i < words.size(); ++i) {
    if (i > 0) out += (i + 1 == words.size()) ? " or " : ", ";
    out += words[i];
  }
  return out.empty() ? std::string(phrases ? "no value" : "none") : out;
}

}  // namespace

std::string describe(Kinds kinds) { return kinds == kind::any ? "any value" : join_words(kinds, true); }

std::string kind_names(Kinds kinds) { return kinds == kind::any ? "any" : join_words(kinds, false); }

std::size_t FunctionSpec::min_args() const noexcept { return required == every_param ? params.size() : required; }

std::size_t FunctionSpec::max_args() const noexcept { return variadic ? every_param : params.size(); }

Call::Call(const FunctionSpec& spec, const std::vector<const nlohmann::json*>& args, const Site& site,
           RenderHost* host) noexcept
    : spec_(spec), args_(args), site_(site), host_(host) {}

const nlohmann::json& Call::operator[](std::size_t index) const {
  if (index >= args_.size() || args_[index] == nullptr) fail("argument " + std::to_string(index + 1) + " is missing");
  return *args_[index];
}

void Call::fail(const std::string& message) const { throw TemplateError(spec_.name + "(): " + message, site_); }

const nlohmann::json& Call::context() const {
  if (host_ == nullptr) fail("needs a render in progress");
  return host_->context();
}

std::string Call::render(std::string_view name, const nlohmann::json& context, bool folder_only) const {
  if (host_ == nullptr) fail("needs a render in progress");
  return host_->render_child(name, context, folder_only, site_);
}

namespace {

// Words a function may not be called: inja's operators and literals, and its statement words.
bool reserved(std::string_view name) {
  static const char* const words[] = {"and",   "or",     "not",    "in",       "true",    "false",
                                      "null",  "if",     "else",   "endif",    "for",     "endfor",
                                      "set",   "include", "extends", "block", "endblock", "raw", "loop"};
  for (const char* word : words) {
    if (name == word) return true;
  }
  return false;
}

void check_spec(const FunctionSpec& spec) {
  const auto refuse = [&](const std::string& why) {
    throw std::invalid_argument("function '" + spec.name + "' cannot be registered: " + why);
  };
  if (!detail::is_identifier(spec.name)) refuse("its name must be letters, digits and '_'");
  if (spec.name.compare(0, 2, "__") == 0) refuse("names starting with '__' are the engine's own");
  if (reserved(spec.name)) refuse("its name is a reserved word");
  if (!spec.implementation) refuse("it has no implementation");
  if (spec.summary.empty() || spec.summary.find('\n') != std::string::npos) refuse("its summary must be one line");
  if (spec.required != FunctionSpec::every_param && spec.required > spec.params.size()) {
    refuse("it requires more arguments than it has parameters");
  }
  if (spec.variadic && spec.params.empty()) refuse("a variadic function needs a parameter to repeat");
  for (const Param& param : spec.params) {
    if (!detail::is_identifier(param.name)) refuse("parameter names must be letters, digits and '_'");
    if ((param.kinds & kind::any) == 0) refuse("parameter '" + param.name + "' accepts no kind of value");
  }
  if ((spec.returns & kind::any) == 0) refuse("it returns no kind of value");
}

}  // namespace

void FunctionRegistry::add(FunctionSpec spec) {
  check_spec(spec);
  if (functions_.count(spec.name) != 0) {
    throw std::invalid_argument("function '" + spec.name + "' is already registered");
  }
  std::string name = spec.name;
  functions_.emplace(std::move(name), std::make_shared<const FunctionSpec>(std::move(spec)));
}

std::shared_ptr<const FunctionSpec> FunctionRegistry::find(std::string_view name) const {
  const auto it = functions_.find(name);
  return it == functions_.end() ? nullptr : it->second;
}

std::vector<std::shared_ptr<const FunctionSpec>> FunctionRegistry::all() const {
  std::vector<std::shared_ptr<const FunctionSpec>> out;
  out.reserve(functions_.size());
  for (const auto& entry : functions_) out.push_back(entry.second);
  return out;  // a std::map keeps them sorted by name
}

std::string signature(const FunctionSpec& spec) {
  std::string out = spec.name + "(";
  for (std::size_t i = 0; i < spec.params.size(); ++i) {
    const bool optional = i >= spec.min_args();
    if (optional) out += "[";
    if (i > 0) out += ", ";
    out += spec.params[i].name;
    if (optional) out += "]";
  }
  if (spec.variadic) out += spec.params.empty() ? "..." : ", ...";
  out += ")";
  // Optional parameters share one bracket: "f(a[, b][, c])" becomes "f(a[, b, c])".
  std::string tidy;
  for (std::size_t i = 0; i < out.size(); ++i) {
    if (out[i] == ']' && i + 1 < out.size() && out[i + 1] == '[') {
      ++i;
      continue;
    }
    tidy += out[i];
  }
  return tidy;
}

namespace {

std::string escape_cell(std::string text) {
  std::string out;
  for (char c : text) {
    if (c == '|') out += "\\|";
    else out += c;
  }
  return out;
}

std::string arguments_cell(const FunctionSpec& spec) {
  if (spec.params.empty()) return "none";
  std::string out;
  for (std::size_t i = 0; i < spec.params.size(); ++i) {
    if (i > 0) out += "; ";
    out += "`" + spec.params[i].name + "`: " + kind_names(spec.params[i].kinds);
  }
  if (spec.variadic) out += " (repeats)";
  return out;
}

nlohmann::json kind_list(Kinds kinds) {
  nlohmann::json out = nlohmann::json::array();
  for (const KindWords& w : kind_words) {
    if (kinds & w.kind) out.push_back(w.noun);
  }
  return out;
}

}  // namespace

std::string reference_markdown(const FunctionRegistry& registry) {
  std::string out =
      "# Template functions\n\n"
      "Generated from en-prompt's function registry by `eldennet-engine render-prompt --list-functions`; edit the\n"
      "registry in code, never this page. A template may call only these functions; any other name fails when the\n"
      "template loads. \"Cached\" means the result depends only on the arguments, so it may be reused.\n\n"
      "| Function | Arguments | Returns | Cached | What it does |\n"
      "|---|---|---|---|---|\n";
  for (const auto& spec : registry.all()) {
    std::string summary = spec->summary;
    if (spec->markup) summary += " Printed directly, its markers stay markers.";
    out += "| `" + signature(*spec) + "` | " + escape_cell(arguments_cell(*spec)) + " | " +
           escape_cell(kind_names(spec->returns)) + " | " + (spec->cacheable ? "yes" : "no") + " | " +
           escape_cell(summary) + " |\n";
  }
  out +=
      "\n## Operators, conditions and printing\n\n"
      "- `and`, `or` and `not` take booleans only, and `and` and `or` stop at the first side that decides. A condition\n"
      "  (`{% if %}`) must be a boolean: a string, a number, a list or null is an error, so the string \"false\" is\n"
      "  never true. Test an optional field with `empty(x)`, a count with `n > 0`.\n"
      "- `not` binds tighter than `==`: `not a == b` means `(not a) == b`, so it is refused; write `not (a == b)`.\n"
      "- `x in s` tests a substring when `s` is a string, a member when it is a list and a key when it is an object.\n"
      "- `==` and `!=` compare values of one kind (any value may be compared with null); `<`, `<=`, `>` and `>=` take\n"
      "  two numbers or two strings. `+` adds numbers or joins strings; `-`, `*`, `/`, `%` and `^` take numbers, and\n"
      "  `/` or `%` by zero is an error.\n"
      "- `{{ x }}` prints a string, a number or a boolean. Null (an unset field), a list or an object is an error: use\n"
      "  `join()` or `to_json()`. Text printed from data is quoted data: its markers and braces stay text.\n"
      "- Loops have `loop.index` and `loop.index0` (from 0), `loop.index1` (from 1), `loop.first` and `loop.is_first`,\n"
      "  `loop.last` and `loop.is_last`, and `loop.parent`.\n";
  return out;
}

nlohmann::json allowlist_json(const FunctionRegistry& registry) {
  nlohmann::json functions = nlohmann::json::array();
  for (const auto& spec : registry.all()) {
    nlohmann::json params = nlohmann::json::array();
    for (const Param& param : spec->params) params.push_back({{"name", param.name}, {"kinds", kind_list(param.kinds)}});
    nlohmann::json max_args = nullptr;
    if (!spec->variadic) max_args = spec->max_args();
    functions.push_back({{"name", spec->name},
                         {"min_args", spec->min_args()},
                         {"max_args", max_args},
                         {"params", std::move(params)},
                         {"variadic", spec->variadic},
                         {"returns", kind_list(spec->returns)},
                         {"cacheable", spec->cacheable},
                         {"markup", spec->markup},
                         {"summary", spec->summary}});
  }
  return {{"format", 1}, {"generated_by", "en-prompt"}, {"functions", std::move(functions)}};
}

}  // namespace en::prompt
