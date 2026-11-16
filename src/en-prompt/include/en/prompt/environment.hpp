#pragma once
// The template environment: inja, made strict (NPC §7.1, WHY§99), with SkyrimNet's composition (SN03 §3).
//
// A prompt is a template file (.j2, .md or .prompt) or a folder of numbered parts ("10_identity.j2",
// "05b_world.j2"), rendered in numeric order and joined. Templates include others by name from the root with
// render_template(name[, vars]) and render_subcomponent(folder[, mode[, vars]]); a render that returns to a template
// already being rendered, or nests deeper than the limit, fails.
//
// Strictness, at load: an unknown function, a wrong number of arguments, {% include %} and {% extends %}, a 'not'
// before a comparison without parentheses, a condition that cannot be a boolean, a name such as "a-b" that inja reads
// as one name, loop.* outside a loop, an unclosed comment and control characters are errors. At render: an unknown
// variable, printing null (an unset field), a list or an object, a condition or an and/or/not operand that is not a
// boolean, mismatched kinds in a comparison or a sum, and every function's own checks are errors. Each error names
// the template, the line and the column. Nothing is cached from a failed render, and it returns nothing.
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/prompt/errors.hpp"
#include "en/prompt/messages.hpp"
#include "en/prompt/registry.hpp"

namespace en::prompt {

namespace detail {
class EnvironmentCore;
}

struct EnvironmentOptions {
  // How deeply renders may nest (SkyrimNet stops at 16 nested renders, Beta26).
  std::size_t max_depth = 16;
  AssembleOptions assemble;
};

class Environment {
 public:
  // `root` is the folder template names are resolved from; it must exist. The registry is fixed from here on.
  explicit Environment(std::filesystem::path root, FunctionRegistry functions = builtin_registry(),
                       EnvironmentOptions options = {});
  ~Environment();
  Environment(Environment&&) noexcept;
  Environment& operator=(Environment&&) noexcept;
  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;

  // Renders a template file or a folder of parts, named from the root ("prompts/dialogue", "selftest.j2"), with a
  // JSON object as its context, into chat messages. Throws TemplateError.
  std::vector<Message> render(std::string_view name, const nlohmann::json& context) const;

  // The same for template source held in memory, named `name` in errors; its render_template calls resolve from
  // the root. It is not cached.
  std::vector<Message> render_source(std::string_view source, const nlohmann::json& context,
                                     std::string_view name = "<inline>") const;

  // Lints a template, or every part of a folder, and the templates it names literally in render_template and
  // render_subcomponent, without rendering. With a context, also finds variable names the context lacks (in every
  // branch, taken or not) and loop or set names that hide the context's own.
  std::vector<Finding> lint(std::string_view name, const nlohmann::json* context = nullptr) const;

  const FunctionRegistry& functions() const noexcept;
  const std::filesystem::path& root() const noexcept;

  // Forgets compiled templates, after template files change on disk.
  void clear_cache() const;

 private:
  std::unique_ptr<detail::EnvironmentCore> core_;
};

// The file extensions a template may have, in the order a bare name tries them.
const std::vector<std::string>& template_extensions();

}  // namespace en::prompt
