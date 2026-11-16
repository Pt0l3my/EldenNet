#pragma once
// en-prompt's internals, shared by its source files and never installed. inja stays behind this header: the public
// headers need only nlohmann/json.
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <inja.hpp>

#include "en/prompt/errors.hpp"
#include "en/prompt/registry.hpp"

namespace en::prompt {

// The render a call belongs to: its context, and the way to render another template by name (environment.cpp).
class RenderHost {
 public:
  virtual ~RenderHost() = default;
  virtual const nlohmann::json& context() const = 0;
  virtual std::string render_child(std::string_view name, const nlohmann::json& context, bool folder_only,
                                   const Site& caller) = 0;
};

namespace detail {

// Text printed from data by {{ }} keeps its brackets and braces as these stand-ins until the output has been split
// into messages and checked, so that a record's "[ system ]" is never read as a marker nor its "{{" as unresolved
// syntax (records enter as quoted data, NPC §7.1). Template sources may not hold control characters, so a stand-in
// in the output always comes from data.
inline constexpr char data_bracket = '\x1A';
inline constexpr char data_open_brace = '\x1C';
inline constexpr char data_close_brace = '\x1D';
// inja reads "##" at the start of a line as a line statement; this opener, which no template may hold, turns them
// off, so a Markdown heading stays text.
inline constexpr const char* no_line_statement = "\x1E";

std::string escape_data(std::string_view text);
void restore_data(std::string& text);

bool is_space(char c) noexcept;
std::string_view trim(std::string_view text) noexcept;
bool is_identifier(std::string_view text) noexcept;
std::size_t utf8_length(std::string_view text) noexcept;
std::vector<std::string> utf8_characters(std::string_view text);
std::vector<std::string> split_dots(std::string_view dotted);
// "a.b" as a JSON pointer, "/a/b", with '~' and '/' escaped.
std::string dotted_pointer(std::string_view dotted);
Site site_at(const std::string& name, std::string_view content, std::size_t pos);

// Normalises a template's text (byte-order mark, CRLF, elif) and records what makes it unusable: invalid UTF-8,
// control characters and a comment that never closes (inja would silently drop the rest of the template).
std::string prepare_source(std::string_view raw, const std::string& name, std::vector<Finding>& findings);

struct VariableUse {
  std::string name;
  Site site;
  bool bound = false;  // a loop variable, loop.* or a {% set %} name
};

struct Compiled {
  std::string name;
  inja::Template tmpl;
  std::vector<VariableUse> variables;
  std::vector<std::pair<std::string, Site>> locals;      // loop variables and {% set %} names
  std::vector<std::pair<std::string, Site>> references;  // literal template names given to render functions
};

// Parses `text` and rewrites its tree so that every operator, function, condition, loop and printed value runs
// through checked code; lint problems go to `findings`. Returns null when inja cannot parse the text.
std::shared_ptr<Compiled> compile(inja::Environment& parser, const std::string& name, std::string text,
                                  const FunctionRegistry& registry, std::vector<Finding>& findings);

// The one membership test behind `x in c` and contains(c, x): a substring of a string, a member of a list, a key of
// an object. Empty when the kinds do not fit (a number looked for in a string, say).
std::optional<bool> holds(const nlohmann::json& container, const nlohmann::json& value);

// The innermost render on this thread, or null outside a render.
RenderHost* current_host() noexcept;

// The registry name closest to a misspelt one, or "" when none is close.
std::string closest_name(std::string_view name, const FunctionRegistry& registry);

// One TemplateError for a list of findings (at least one).
TemplateError error_from(const std::vector<Finding>& findings);

}  // namespace detail
}  // namespace en::prompt
