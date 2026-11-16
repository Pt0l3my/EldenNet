// The environment: template names and folders of parts, the render stack with its cycle guard and depth limit, the
// cache of compiled templates, and the lint.
#include "en/prompt/environment.hpp"

#include <algorithm>
#include <cctype>
#include <deque>
#include <fstream>
#include <iterator>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "detail.hpp"

namespace en::prompt {

namespace fs = std::filesystem;
using nlohmann::json;

const std::vector<std::string>& template_extensions() {
  static const std::vector<std::string> extensions{".j2", ".md", ".prompt"};
  return extensions;
}

namespace detail {

// A template file or a folder of parts, by its name from the root.
struct Target {
  std::string name;  // '/' between folders; a file's name keeps its extension
  fs::path path;
  bool folder = false;
};

class EnvironmentCore {
 public:
  EnvironmentCore(fs::path root, FunctionRegistry registry, EnvironmentOptions options);

  std::vector<Message> render(std::string_view name, const json& context);
  std::vector<Message> render_source(std::string_view source, const json& context, std::string_view name);
  std::vector<Finding> lint(std::string_view name, const json* context);

  Target resolve(std::string_view name) const;
  // Why rendering `target` now would loop or nest too deeply, or "".
  std::string stack_problem(const Target& target) const;
  std::string render_target(const Target& target, const json& context);
  void clear_cache();

  const FunctionRegistry& registry() const noexcept { return registry_; }
  const fs::path& root() const noexcept { return root_; }

 private:
  std::vector<Target> parts_of(const Target& folder) const;
  std::shared_ptr<const Compiled> load(const Target& file);
  std::shared_ptr<Compiled> compile_text(const std::string& name, std::string_view raw, std::vector<Finding>& findings);
  std::string run(const Compiled& compiled, const json& context);
  std::vector<Message> finish(const std::string& rendered, const std::string& name) const;

  fs::path root_;
  fs::path canonical_root_;
  FunctionRegistry registry_;
  EnvironmentOptions options_;
  std::mutex mutex_;  // guards parser_ while it parses, and cache_
  inja::Environment parser_;
  std::map<std::string, std::shared_ptr<const Compiled>> cache_;
};

namespace {

class Frame;
// The renders in progress on this thread, innermost last. Functions find their render here, so a compiled template,
// shared by every render, holds no render's state.
thread_local std::vector<Frame*> frames;

class Frame final : public RenderHost {
 public:
  Frame(EnvironmentCore& core, std::string name, const json& context)
      : core_(core), name_(std::move(name)), context_(context) {
    frames.push_back(this);
  }
  ~Frame() override { frames.pop_back(); }
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

 private:
  EnvironmentCore& core_;
  std::string name_;
  const json& context_;
};

std::string utf8_name(const fs::path& path) {
  const std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

bool has_template_extension(std::string_view file) {
  return std::any_of(template_extensions().begin(), template_extensions().end(), [&](const std::string& extension) {
    return file.size() > extension.size() && file.substr(file.size() - extension.size()) == extension;
  });
}

bool is_name_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || c == '/' || c == '\\';
}

bool inside(const fs::path& root, const fs::path& path) {
  auto r = root.begin();
  auto p = path.begin();
  for (; r != root.end(); ++r, ++p) {
    if (p == path.end() || *r != *p) return false;
  }
  return true;
}

bool read_file(const fs::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !in.bad();
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

RenderHost* current_host() noexcept { return frames.empty() ? nullptr : frames.back(); }

EnvironmentCore::EnvironmentCore(fs::path root, FunctionRegistry registry, EnvironmentOptions options)
    : root_(std::move(root)), registry_(std::move(registry)), options_(options) {
  std::error_code error;
  if (!fs::is_directory(root_, error)) throw std::invalid_argument("the template root is not a folder");
  canonical_root_ = fs::canonical(root_, error);
  if (error) throw std::invalid_argument("the template root cannot be resolved");
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
  if (raw.empty()) throw refuse("a template name is needed");
  if (!std::all_of(raw.begin(), raw.end(), is_name_char)) {
    throw refuse("template names use letters, digits, '_', '-', '.' and '/', and '" + shown + "' has another character");
  }
  std::string name(raw);
  std::replace(name.begin(), name.end(), '\\', '/');  // SkyrimNet writes "components\\event_history"
  while (!name.empty() && name.back() == '/') name.pop_back();

  std::vector<std::string> segments;
  for (std::size_t start = 0;;) {
    const std::size_t slash = name.find('/', start);
    segments.push_back(name.substr(start, slash == std::string::npos ? std::string::npos : slash - start));
    if (slash == std::string::npos) break;
    start = slash + 1;
  }
  fs::path path = root_;
  std::string joined;
  for (const std::string& segment : segments) {
    if (segment.empty() || segment == "." || segment == "..") {
      throw refuse("'" + shown + "' must name a template inside the root, without '.', '..' or empty folder names");
    }
    path /= segment;
    joined += (joined.empty() ? "" : "/") + segment;
  }

  std::error_code error;
  Target target;
  if (fs::is_directory(path, error)) {
    target = Target{joined, path, true};
  } else if (fs::is_regular_file(path, error)) {
    if (!has_template_extension(joined)) throw refuse("'" + shown + "' is not a template file (.j2, .md or .prompt)");
    target = Target{joined, path, false};
  } else {
    std::vector<Target> found;
    for (const std::string& extension : template_extensions()) {
      fs::path candidate = path;
      candidate += extension;
      if (fs::is_regular_file(candidate, error)) found.push_back(Target{joined + extension, candidate, false});
    }
    if (found.empty()) throw refuse("no template named '" + shown + "' under the root");
    if (found.size() > 1) {
      throw refuse("'" + shown + "' is ambiguous: " + found[0].name + " and " + found[1].name + " both exist");
    }
    target = found.front();
  }
  // A link inside the root must not lead out of it.
  const fs::path real = fs::weakly_canonical(target.path, error);
  if (error || !inside(canonical_root_, real)) throw refuse("'" + shown + "' leads outside the root");
  return target;
}

std::string EnvironmentCore::stack_problem(const Target& target) const {
  std::vector<const std::string*> chain;
  for (const Frame* frame : frames) {
    if (frame->core() == this) chain.push_back(&frame->name());
  }
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
    fs::path path;
  };
  std::vector<Entry> entries;
  std::error_code error;
  fs::directory_iterator it(folder.path, error);
  if (error) throw TemplateError("the folder cannot be read", Site{folder.name});
  for (; it != fs::directory_iterator(); it.increment(error)) {
    if (error) throw TemplateError("the folder cannot be read", Site{folder.name});
    std::error_code kind_error;
    if (!it->is_regular_file(kind_error)) continue;
    const std::string file = utf8_name(it->path().filename());
    if (file.empty() || file.front() == '.' || !has_template_extension(file)) continue;
    const Site where{folder.name + "/" + file};
    if (!std::all_of(file.begin(), file.end(), is_name_char)) {
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
    std::error_code link_error;
    const fs::path real = fs::weakly_canonical(it->path(), link_error);
    if (link_error || !inside(canonical_root_, real)) throw TemplateError("the part leads outside the root", where);
    entries.push_back(Entry{std::stoull(file.substr(0, digits)), file.substr(digits, letters - digits), file,
                            it->path()});
  }
  if (entries.empty()) throw TemplateError("the folder has no numbered parts", Site{folder.name});
  std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
    return std::tie(a.number, a.letters, a.file) < std::tie(b.number, b.letters, b.file);
  });
  std::vector<Target> parts;
  for (const Entry& entry : entries) parts.push_back(Target{folder.name + "/" + entry.file, entry.path, false});
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
  std::string raw;
  if (!read_file(file.path, raw)) throw TemplateError("the template file cannot be read", Site{file.name});
  std::vector<Finding> findings;
  std::shared_ptr<Compiled> compiled = compile_text(file.name, raw, findings);
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
    std::string raw;
    if (!read_file(target.path, raw)) {
      out.push_back({Site{target.name}, "unreadable", "the template file cannot be read"});
      continue;
    }
    std::vector<Finding> findings;
    const std::shared_ptr<Compiled> compiled = compile_text(target.name, raw, findings);
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

Environment::Environment(fs::path root, FunctionRegistry functions, EnvironmentOptions options)
    : core_(std::make_unique<detail::EnvironmentCore>(std::move(root), std::move(functions), options)) {}

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

const std::filesystem::path& Environment::root() const noexcept { return core_->root(); }

void Environment::clear_cache() const { core_->clear_cache(); }

}  // namespace en::prompt
