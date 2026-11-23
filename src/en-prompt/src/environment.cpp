// The environment: template names and folders of parts, resolved through its template source, the render stack with
// its cycle guard and depth limit, the cache of compiled templates, and the lint.
#include "en/prompt/environment.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "detail.hpp"

namespace en::prompt {

using nlohmann::json;

namespace {

// A constant, so nothing is built when the library loads or destroyed when it unloads (ARCH §3.1, §1.7).
constexpr std::array<std::string_view, 3> extension_list{".j2", ".md", ".prompt"};

}  // namespace

std::span<const std::string_view> template_extensions() noexcept { return extension_list; }

namespace detail {

// A template file or a folder of parts, by its name from the root.
struct Target {
  std::string name;  // '/' between folders; a file's name keeps its extension
  bool folder = false;
};

class EnvironmentCore {
 public:
  EnvironmentCore(std::shared_ptr<const TemplateSource> source, FunctionRegistry registry,
                  EnvironmentOptions options);

  std::vector<Message> render(std::string_view name, const json& context);
  std::vector<Message> render_source(std::string_view source, const json& context, std::string_view name);
  std::vector<Finding> lint(std::string_view name, const json* context);

  Target resolve(std::string_view name) const;
  // Why rendering `target` now would loop or nest too deeply, or "".
  std::string stack_problem(const Target& target) const;
  std::string render_target(const Target& target, const json& context);
  void clear_cache();

  const FunctionRegistry& registry() const noexcept { return registry_; }
  const TemplateSource& source() const noexcept { return *source_; }

 private:
  std::vector<Target> parts_of(const Target& folder) const;
  std::shared_ptr<const Compiled> load(const Target& file);
  std::shared_ptr<Compiled> compile_text(const std::string& name, std::string_view raw, std::vector<Finding>& findings);
  std::string run(const Compiled& compiled, const json& context);
  std::vector<Message> finish(const std::string& rendered, const std::string& name) const;

  std::shared_ptr<const TemplateSource> source_;
  FunctionRegistry registry_;
  EnvironmentOptions options_;
  std::mutex mutex_;  // guards parser_ while it parses, and cache_
  inja::Environment parser_;
  std::map<std::string, std::shared_ptr<const Compiled>> cache_;
};

namespace {

class Frame;
// The innermost render in progress on this thread, which links to the render it came from, and so on outwards.
// Functions find their render here, so a compiled template, shared by every render, holds no render's state. A plain
// pointer, so nothing is built for a thread or destroyed when it ends (ARCH §1.7).
thread_local Frame* innermost = nullptr;

class Frame final : public RenderHost {
 public:
  Frame(EnvironmentCore& core, std::string name, const json& context)
      : core_(core), name_(std::move(name)), context_(context), outer_(innermost) {
    innermost = this;
  }
  ~Frame() override { innermost = outer_; }
  Frame(const Frame&) = delete;
  Frame& operator=(const Frame&) = delete;

  const json& context() const override { return context_; }

  std::string render_child(std::string_view name, const json& context, bool folder_only,
                           const Site& caller) override {
    Target target;
    try {
      target = core_.resolve(name);
    } catch (const TemplateError& error) {
      throw TemplateError(error.message(), caller);
    }
    if (folder_only && !target.folder) {
      throw TemplateError("render_subcomponent() takes a folder of numbered parts, and '" + target.name +
                              "' is a file: use render_template()",
                          caller);
    }
    if (const std::string problem = core_.stack_problem(target); !problem.empty()) {
      throw TemplateError(problem, caller);
    }
    try {
      return core_.render_target(target, context);
    } catch (const TemplateError& error) {
      throw error.rendered_from(caller);
    }
  }

  const EnvironmentCore* core() const noexcept { return &core_; }
  const std::string& name() const noexcept { return name_; }
  const Frame* outer() const noexcept { return outer_; }

 private:
  EnvironmentCore& core_;
  std::string name_;
  const json& context_;
  Frame* outer_;
};

bool has_template_extension(std::string_view file) {
  const std::span<const std::string_view> extensions = template_extensions();
  return std::any_of(extensions.begin(), extensions.end(), [&](std::string_view extension) {
    return file.size() > extension.size() && file.substr(file.size() - extension.size()) == extension;
  });
}

// A part's name has no folders, so neither '/' nor '\' (which a file's name may hold on Linux).
bool is_part_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.';
}

std::string render_message(const std::string& message) {
  const std::string prefix = "variable '";
  const std::string suffix = "' not found";
  if (message.size() > prefix.size() + suffix.size() && message.compare(0, prefix.size(), prefix) == 0 &&
      message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0) {
    const std::string name = message.substr(prefix.size(), message.size() - prefix.size() - suffix.size());
    return "unknown variable '" + name + "': the context has no such name (a field left unset is there as null)";
  }
  return message;
}

// Whether a dotted name can be found in a context, as far as the context shows; `problem` says why not.
bool path_known(const json& context, const std::string& name, std::string& problem) {
  const json* current = &context;
  std::string walked;
  for (const std::string& segment : split_dots(name)) {
    if (current->is_object()) {
      const auto it = current->find(segment);
      if (it == current->end()) {
        problem = walked.empty() ? "unknown variable '" + segment + "': the context has no such name"
                                 : "'" + walked + "' has no field '" + segment + "'";
        return false;
      }
      current = &*it;
    } else if (current->is_array()) {
      const bool index = !segment.empty() && std::all_of(segment.begin(), segment.end(), [](char c) {
        return c >= '0' && c <= '9';
      });
      if (!index || segment.size() > 9 || std::stoul(segment) >= current->size()) return true;  // not knowable here
      current = &(*current)[std::stoul(segment)];
    } else if (current->is_null()) {
      return true;  // an unset field: its shape is not known from this context
    } else {
      problem = "'" + walked + "' is " + describe(kind_of(*current)) + ", which has no field '" + segment + "'";
      return false;
    }
    walked += (walked.empty() ? "" : ".") + segment;
  }
  return true;
}

void lint_variables(const Compiled& compiled, const json& context, std::vector<Finding>& out) {
  for (const VariableUse& use : compiled.variables) {
    std::string problem;
    if (!use.bound && !path_known(context, use.name, problem)) out.push_back({use.site, "unknown-variable", problem});
  }
  // inja leaves a loop variable behind after its loop, cleared, so a loop or set name equal to a context name hides
  // the context's value for the rest of the template.
  for (const auto& [local, site] : compiled.locals) {
    if (context.contains(local)) {
      out.push_back({site, "shadows-context", "the loop or set name '" + local + "' hides the context's own '" + local + "'"});
    }
  }
}

}  // namespace

RenderHost* current_host() noexcept { return innermost; }

EnvironmentCore::EnvironmentCore(std::shared_ptr<const TemplateSource> source, FunctionRegistry registry,
                                 EnvironmentOptions options)
    : source_(std::move(source)), registry_(std::move(registry)), options_(options) {
  if (!source_) throw std::invalid_argument("a template source is needed");
  if (options_.max_depth == 0) options_.max_depth = 1;

  parser_.set_line_statement(no_line_statement);  // "## Heading" stays text
  parser_.set_search_included_templates_in_files(false);  // inja never opens a file on its own
  parser_.set_trim_blocks(false);  // SkyrimNet's defaults: whitespace is tidied after splitting instead
  parser_.set_lstrip_blocks(false);
  // The parser needs to know every function's name. Each call node gets its checked binding when the tree is
  // rewritten (compile.cpp); this one runs only if a node were somehow left unbound.
  const inja::CallbackFunction unbound = [](inja::Arguments&) -> json {
    throw TemplateError("a function ran without its checks (an engine fault)", Site{});
  };
  for (const auto& spec : registry_.all()) {
    parser_.add_callback(spec->name, -1, unbound);  // any count parses; the rewrite checks the count
    if (spec->min_args() == 0) parser_.add_callback(spec->name, 0, unbound);
  }
}

Target EnvironmentCore::resolve(std::string_view raw) const {
  const std::string shown(raw);
  const auto refuse = [&](const std::string& why) { return TemplateError(why, Site{shown}); };
  const CheckedName checked = check_name(raw);
  if (!checked.problem.empty()) throw refuse(checked.problem);
  const std::string& joined = checked.name;

  Target target;
  const TemplateSource::Entry entry = source_->find(joined);
  if (entry == TemplateSource::Entry::folder) {
    target = Target{joined, true};
  } else if (entry == TemplateSource::Entry::file) {
    if (!has_template_extension(joined)) throw refuse("'" + shown + "' is not a template file (.j2, .md or .prompt)");
    target = Target{joined, false};
  } else {
    std::vector<std::string> found;
    for (const std::string_view extension : template_extensions()) {
      std::string candidate = joined + std::string(extension);
      if (source_->find(candidate) == TemplateSource::Entry::file) found.push_back(std::move(candidate));
    }
    if (found.empty()) throw refuse("no template named '" + shown + "' under the root");
    if (found.size() > 1) throw refuse("'" + shown + "' is ambiguous: " + found[0] + " and " + found[1] + " both exist");
    target = Target{found.front(), false};
  }
  // A link inside the root must not lead out of it.
  if (source_->leads_outside(target.name)) throw refuse("'" + shown + "' leads outside the root");
  return target;
}

std::string EnvironmentCore::stack_problem(const Target& target) const {
  std::vector<const std::string*> chain;  // this environment's renders on this thread, outermost first
  for (const Frame* frame = innermost; frame != nullptr; frame = frame->outer()) {
    if (frame->core() == this) chain.push_back(&frame->name());
  }
  std::reverse(chain.begin(), chain.end());
  for (std::size_t i = 0; i < chain.size(); ++i) {
    if (*chain[i] != target.name) continue;
    std::string path;
    for (std::size_t j = i; j < chain.size(); ++j) path += *chain[j] + " -> ";
    return "a render cycle: " + path + target.name;
  }
  if (chain.size() >= options_.max_depth) {
    return "renders nest deeper than " + std::to_string(options_.max_depth) + " levels: " + *chain.front() +
           " -> ... -> " + target.name;
  }
  return {};
}

std::string EnvironmentCore::render_target(const Target& target, const json& context) {
  if (const std::string problem = stack_problem(target); !problem.empty()) {
    throw TemplateError(problem, Site{target.name});
  }
  Frame frame(*this, target.name, context);
  if (!target.folder) return run(*load(target), context);

  std::string out;
  for (const Target& part : parts_of(target)) {
    const std::string text = render_target(part, context);
    if (!out.empty() && out.back() != '\n' && !text.empty()) out += '\n';  // a part's last line never runs on
    out += text;
  }
  return out;
}

std::vector<Target> EnvironmentCore::parts_of(const Target& folder) const {
  struct Entry {
    std::uint64_t number = 0;
    std::string letters;  // "05b_world.j2" comes after "05_contract.md"
    std::string file;
  };
  std::optional<std::vector<std::string>> files = source_->files(folder.name);
  if (!files) throw TemplateError("the folder cannot be read", Site{folder.name});
  std::sort(files->begin(), files->end());  // so a folder with two faults names the same one from every source
  std::vector<Entry> entries;
  for (const std::string& file : *files) {
    if (file.empty() || file.front() == '.' || !has_template_extension(file)) continue;
    const Site where{folder.name + "/" + file};
    if (!std::all_of(file.begin(), file.end(), is_part_char)) {
      throw TemplateError("part names use letters, digits, '_', '-' and '.'", where);
    }
    std::size_t digits = 0;
    while (digits < file.size() && std::isdigit(static_cast<unsigned char>(file[digits]))) ++digits;
    if (digits == 0 || digits > 18) {
      throw TemplateError("the part '" + file + "' has no number: name it like 10_" + file +
                              ", since a folder's parts render in numeric order",
                          where);
    }
    std::size_t letters = digits;
    while (letters < file.size() && file[letters] >= 'a' && file[letters] <= 'z') ++letters;
    if (letters >= file.size() || file[letters] != '_') {
      throw TemplateError("the part '" + file + "' needs '_' after its number, as in 10_identity.j2 or 05b_world.j2",
                          where);
    }
    // A part may be a link, and a link must not lead out of the root any more than a name may.
    if (source_->leads_outside(where.template_name)) throw TemplateError("the part leads outside the root", where);
    entries.push_back(Entry{std::stoull(file.substr(0, digits)), file.substr(digits, letters - digits), file});
  }
  if (entries.empty()) throw TemplateError("the folder has no numbered parts", Site{folder.name});
  std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    return std::tie(a.number, a.letters, a.file) < std::tie(b.number, b.letters, b.file);
  });
  std::vector<Target> parts;
  for (const Entry& entry : entries) parts.push_back(Target{folder.name + "/" + entry.file, false});
  return parts;
}

std::shared_ptr<Compiled> EnvironmentCore::compile_text(const std::string& name, std::string_view raw,
                                                        std::vector<Finding>& findings) {
  std::string text = prepare_source(raw, name, findings);
  if (!findings.empty()) return nullptr;
  const std::lock_guard<std::mutex> lock(mutex_);
  return compile(parser_, name, std::move(text), registry_, findings);
}

std::shared_ptr<const Compiled> EnvironmentCore::load(const Target& file) {
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (const auto it = cache_.find(file.name); it != cache_.end()) return it->second;
  }
  const std::optional<std::string> raw = source_->read(file.name);
  if (!raw) throw TemplateError("the template file cannot be read", Site{file.name});
  std::vector<Finding> findings;
  std::shared_ptr<Compiled> compiled = compile_text(file.name, *raw, findings);
  if (!compiled || !findings.empty()) throw error_from(findings);
  const std::lock_guard<std::mutex> lock(mutex_);
  return cache_.emplace(file.name, std::move(compiled)).first->second;
}

std::string EnvironmentCore::run(const Compiled& compiled, const json& context) {
  std::ostringstream out;
  try {
    parser_.render_to(out, compiled.tmpl, context);  // reads the parser's settings only: safe beside a parse
  } catch (const TemplateError&) {
    throw;
  } catch (const inja::InjaError& error) {
    throw TemplateError(render_message(error.message),
                        Site{compiled.name, error.location.line, error.location.column});
  } catch (const std::exception& error) {
    throw TemplateError(std::string("the render failed: ") + error.what(), Site{compiled.name});
  }
  std::string text = out.str();
  check_unresolved(text, compiled.name);
  return text;
}

std::vector<Message> EnvironmentCore::finish(const std::string& rendered, const std::string& name) const {
  std::vector<Message> messages = assemble_messages(rendered, name, options_.assemble);
  if (messages.empty()) throw TemplateError("the prompt rendered no messages", Site{name});
  return messages;
}

std::vector<Message> EnvironmentCore::render(std::string_view name, const json& context) {
  if (!context.is_object()) throw TemplateError("the context must be a JSON object", Site{std::string(name)});
  const Target target = resolve(name);
  return finish(render_target(target, context), target.name);
}

std::vector<Message> EnvironmentCore::render_source(std::string_view source, const json& context,
                                                    std::string_view name) {
  const std::string shown(name);
  if (!context.is_object()) throw TemplateError("the context must be a JSON object", Site{shown});
  std::vector<Finding> findings;
  const std::shared_ptr<Compiled> compiled = compile_text(shown, source, findings);
  if (!compiled || !findings.empty()) throw error_from(findings);
  std::string rendered;
  {
    Frame frame(*this, shown, context);
    rendered = run(*compiled, context);
  }
  return finish(rendered, shown);
}

std::vector<Finding> EnvironmentCore::lint(std::string_view name, const json* context) {
  std::vector<Finding> out;
  std::deque<std::pair<Target, bool>> queue;  // with whether to check variable names against the context
  const auto enqueue = [&](const Target& target, bool check_variables) {
    if (!target.folder) {
      queue.emplace_back(target, check_variables);
      return;
    }
    try {
      for (const Target& part : parts_of(target)) queue.emplace_back(part, check_variables);
    } catch (const TemplateError& error) {
      out.push_back({error.site(), "folder", error.message()});
    }
  };
  try {
    enqueue(resolve(name), context != nullptr);
  } catch (const TemplateError& error) {
    out.push_back({error.site(), "missing-template", error.message()});
    return out;
  }

  std::set<std::string> seen;
  while (!queue.empty()) {
    const auto [target, check_variables] = queue.front();
    queue.pop_front();
    if (!seen.insert(target.name).second) continue;
    const std::optional<std::string> raw = source_->read(target.name);
    if (!raw) {
      out.push_back({Site{target.name}, "unreadable", "the template file cannot be read"});
      continue;
    }
    std::vector<Finding> findings;
    const std::shared_ptr<Compiled> compiled = compile_text(target.name, *raw, findings);
    out.insert(out.end(), findings.begin(), findings.end());
    if (!compiled) continue;
    // Templates reached through render_template get variables the caller chooses, so only the named ones are
    // checked against the context.
    if (check_variables) lint_variables(*compiled, *context, out);
    for (const auto& [reference, site] : compiled->references) {
      try {
        enqueue(resolve(reference), false);
      } catch (const TemplateError& error) {
        out.push_back({site, "missing-template", error.message()});
      }
    }
  }
  return out;
}

void EnvironmentCore::clear_cache() {
  const std::lock_guard<std::mutex> lock(mutex_);
  cache_.clear();
}

}  // namespace detail

Environment::Environment(std::shared_ptr<const TemplateSource> source, FunctionRegistry functions,
                         EnvironmentOptions options)
    : core_(std::make_unique<detail::EnvironmentCore>(std::move(source), std::move(functions), options)) {}

Environment::~Environment() = default;
Environment::Environment(Environment&&) noexcept = default;
Environment& Environment::operator=(Environment&&) noexcept = default;

std::vector<Message> Environment::render(std::string_view name, const json& context) const {
  return core_->render(name, context);
}

std::vector<Message> Environment::render_source(std::string_view source, const json& context,
                                                std::string_view name) const {
  return core_->render_source(source, context, name);
}

std::vector<Finding> Environment::lint(std::string_view name, const json* context) const {
  return core_->lint(name, context);
}

const FunctionRegistry& Environment::functions() const noexcept { return core_->registry(); }

const TemplateSource& Environment::source() const noexcept { return core_->source(); }

void Environment::clear_cache() const { core_->clear_cache(); }

}  // namespace en::prompt
