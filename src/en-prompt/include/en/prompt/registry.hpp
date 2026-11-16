#pragma once
// The one function registry (NPC §7.1, WHY§98 point 8). A template calls only the functions registered here; each has
// a name, typed parameters, a return kind, a one-line summary and whether its results may be reused. The function
// reference (Markdown) and the lint's allowlist (JSON) are generated from it, so neither can drift from the code.
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/prompt/errors.hpp"

namespace en::prompt {

// The kinds of value a template handles. A set of kinds is a bit mask.
using Kinds = unsigned;
namespace kind {
inline constexpr Kinds null = 1u << 0;
inline constexpr Kinds boolean = 1u << 1;
inline constexpr Kinds number = 1u << 2;
inline constexpr Kinds string = 1u << 3;
inline constexpr Kinds list = 1u << 4;
inline constexpr Kinds object = 1u << 5;
inline constexpr Kinds any = null | boolean | number | string | list | object;
}  // namespace kind

// The kind of a JSON value (0 for the binary and discarded values templates never hold).
Kinds kind_of(const nlohmann::json& value);
// "a string", "a string or a list", "any value": for messages.
std::string describe(Kinds kinds);
// "string", "string or list", "any": for the reference.
std::string kind_names(Kinds kinds);

struct Param {
  std::string name;
  Kinds kinds = kind::any;
};

class Call;
class RenderHost;  // the render a call belongs to (internal)

using Implementation = std::function<nlohmann::json(const Call&)>;

struct FunctionSpec {
  static constexpr std::size_t every_param = static_cast<std::size_t>(-1);

  std::string name;
  std::vector<Param> params;
  std::size_t required = every_param;  // the first `required` parameters must be given; the rest are optional
  bool variadic = false;               // the last parameter may repeat
  Kinds returns = kind::any;
  std::string summary;     // one line, for the reference
  bool cacheable = false;  // a pure function of its arguments: a result may be reused for the same arguments
  bool markup = false;     // returns template output, whose markers stay markers when it is printed directly
  Implementation implementation;

  std::size_t min_args() const noexcept;
  std::size_t max_args() const noexcept;  // every_param when variadic
};

// One call of a registry function, as its implementation sees it. Arguments have been checked against the
// parameters' kinds before the implementation runs.
class Call {
 public:
  Call(const FunctionSpec& spec, const std::vector<const nlohmann::json*>& args, const Site& site,
       RenderHost* host) noexcept;

  std::size_t size() const noexcept { return args_.size(); }
  const nlohmann::json& operator[](std::size_t index) const;
  const Site& site() const noexcept { return site_; }
  const FunctionSpec& function() const noexcept { return spec_; }

  // Throws a TemplateError at the call: "name(): message".
  [[noreturn]] void fail(const std::string& message) const;

  // The context of the render this call belongs to.
  const nlohmann::json& context() const;
  // Renders a template file, or a folder of numbered parts, by name from the environment's root.
  std::string render(std::string_view name, const nlohmann::json& context, bool folder_only = false) const;

 private:
  const FunctionSpec& spec_;
  const std::vector<const nlohmann::json*>& args_;
  const Site& site_;
  RenderHost* host_;
};

class FunctionRegistry {
 public:
  // Adds a function. Throws std::invalid_argument for a duplicate name, a reserved word or a malformed spec.
  void add(FunctionSpec spec);
  // The function with this name, or null.
  std::shared_ptr<const FunctionSpec> find(std::string_view name) const;
  // Every function, sorted by name.
  std::vector<std::shared_ptr<const FunctionSpec>> all() const;
  std::size_t size() const noexcept { return functions_.size(); }

 private:
  std::map<std::string, std::shared_ptr<const FunctionSpec>, std::less<>> functions_;
};

// Every built-in function: inja's, made strict and safe on empty input, and en-prompt's own.
FunctionRegistry builtin_registry();

// "round(number[, digits])", "object(key, value, ...)".
std::string signature(const FunctionSpec& spec);

// The function reference, in Markdown (`render-prompt --list-functions`).
std::string reference_markdown(const FunctionRegistry& registry);

// The lint's allowlist: every function's name, arity, parameter and return kinds (`--list-functions --json`).
nlohmann::json allowlist_json(const FunctionRegistry& registry);

}  // namespace en::prompt
