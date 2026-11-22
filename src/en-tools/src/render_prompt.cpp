// eldennet-engine render-prompt: renders a template folder or file with a JSON context into chat messages, lints
// templates, prints the function reference generated from en-prompt's registry, and renders the dialogue prompt a
// fixture gives (--fixture: en-core's renderer over the content pack's templates, as the engine renders it).
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>

#include <nlohmann/json.hpp>

#include "en/prompt/prompt.hpp"
#include "prompt_fixture.hpp"

namespace {

namespace fs = std::filesystem;
namespace prompt = en::prompt;

constexpr const char* usage_text =
    "usage: eldennet-engine render-prompt <template folder or file> --context <file.json> [--root <dir>]\n"
    "                                     [--json] [--no-merge]\n"
    "       eldennet-engine render-prompt <template folder or file> --lint [--context <file.json>] [--root <dir>]\n"
    "       eldennet-engine render-prompt --list-functions [--json]\n"
    "       eldennet-engine render-prompt --fixture <file.json> [--content <pack>] [--json]\n"
    "\n"
    "  Renders the template, or the numbered parts of a folder, with the context into chat messages. Template\n"
    "  names in render_template() resolve from --root, the current folder by default; the template must lie\n"
    "  inside it.\n"
    "  --json            print the messages as JSON (role, content, cache mark and parts)\n"
    "  --no-merge        keep consecutive messages with the same role apart\n"
    "  --lint            check the template and the templates it names, every branch, without rendering;\n"
    "                    with --context, also find variable names the context lacks\n"
    "  --list-functions  print the function reference (Markdown), or with --json the lint's allowlist\n"
    "  --fixture         render the dialogue prompt a fixture gives (tests/fixtures/prompt/), with en-core's\n"
    "                    renderer over the pack's prompts/ (--content, content/eldenring by default): its blocks,\n"
    "                    caps, trim and cache marks, in the prompt goldens' form, or with --json as JSON\n";

struct Options {
  std::string target;
  std::string context_file;
  std::string root;
  bool json = false;
  bool merge = true;
  bool lint = false;
  bool list_functions = false;
  std::string fixture;
  std::string content;
};

int usage_error(const std::string& message) {
  std::fprintf(stderr, "render-prompt: %s\n\n%s", message.c_str(), usage_text);
  return 2;
}

int failure(const std::string& message) {
  std::fprintf(stderr, "render-prompt: %s\n", message.c_str());
  return 1;
}

// Returns an empty string when the arguments are usable, or what is wrong with them.
std::string parse_arguments(int argc, char** argv, Options& options) {
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    const auto value = [&](std::string& out) -> bool {
      if (i + 1 >= argc) return false;
      out = argv[++i];
      return true;
    };
    if (argument == "--context") {
      if (!value(options.context_file)) return "--context needs a file";
    } else if (argument == "--root") {
      if (!value(options.root)) return "--root needs a folder";
    } else if (argument == "--json") {
      options.json = true;
    } else if (argument == "--no-merge") {
      options.merge = false;
    } else if (argument == "--lint") {
      options.lint = true;
    } else if (argument == "--fixture") {
      if (!value(options.fixture)) return "--fixture needs a file";
    } else if (argument == "--content") {
      if (!value(options.content)) return "--content needs a folder";
    } else if (argument == "--list-functions") {
      options.list_functions = true;
    } else if (argument == "--help" || argument == "-h") {
      return "help";
    } else if (!argument.empty() && argument.front() == '-') {
      return "unknown option " + argument;
    } else if (options.target.empty()) {
      options.target = argument;
    } else {
      return "one template at a time";
    }
  }
  if (options.list_functions) return {};
  if (!options.fixture.empty()) {
    if (!options.target.empty() || options.lint || !options.context_file.empty() || !options.root.empty()) {
      return "--fixture takes no template, --context, --root or --lint";
    }
    return {};
  }
  if (!options.content.empty()) return "--content goes with --fixture";
  if (options.target.empty()) return "name a template folder or file";
  if (!options.lint && options.context_file.empty()) return "--context is needed to render";
  return {};
}

bool read_context(const std::string& file, nlohmann::json& out, std::string& problem) {
  std::ifstream in(fs::path(file), std::ios::binary);
  if (!in) {
    problem = "cannot read the context file";
    return false;
  }
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  out = nlohmann::json::parse(text, nullptr, false);
  if (out.is_discarded()) {
    problem = "the context file is not valid JSON";
    return false;
  }
  if (!out.is_object()) {
    problem = "the context must be a JSON object";
    return false;
  }
  return true;
}

// The template's name from the root, or an empty string when it lies outside the root.
std::string name_from_root(const fs::path& root, const std::string& target) {
  std::error_code error;
  const fs::path absolute_root = fs::weakly_canonical(fs::absolute(root, error), error);
  const fs::path absolute_target = fs::weakly_canonical(fs::absolute(fs::path(target), error), error);
  if (error) return {};
  const fs::path relative = absolute_target.lexically_relative(absolute_root);
  const std::string name = relative.generic_string();
  if (name.empty() || name == "." || name.compare(0, 2, "..") == 0) return {};
  return name;
}

int list_functions(const Options& options) {
  const prompt::FunctionRegistry registry = prompt::builtin_registry();
  if (options.json) {
    std::printf("%s\n", prompt::allowlist_json(registry).dump(2).c_str());
  } else {
    std::fputs(prompt::reference_markdown(registry).c_str(), stdout);
  }
  return 0;
}

int lint(const prompt::Environment& environment, const std::string& name, const nlohmann::json* context) {
  const std::vector<prompt::Finding> findings = environment.lint(name, context);
  for (const prompt::Finding& finding : findings) std::printf("%s\n", prompt::to_string(finding).c_str());
  if (findings.empty()) {
    std::fprintf(stderr, "render-prompt: no problems found\n");
    return 0;
  }
  std::fprintf(stderr, "render-prompt: %zu problem%s found\n", findings.size(), findings.size() == 1 ? "" : "s");
  return 1;
}

int render(const prompt::Environment& environment, const std::string& name, const nlohmann::json& context,
           const Options& options) {
  const std::vector<prompt::Message> messages = environment.render(name, context);
  const std::size_t tokens = prompt::estimate_tokens(messages);
  std::size_t breakpoints = 0;
  for (const prompt::Message& message : messages) breakpoints += message.breakpoints();
  if (options.json) {
    const nlohmann::json out = {{"template", name},
                                {"messages", prompt::to_json(messages)},
                                {"cache_breakpoints", breakpoints},
                                {"token_estimate", tokens}};
    std::printf("%s\n", out.dump(2).c_str());
  } else {
    std::fputs(prompt::format_plain(messages).c_str(), stdout);
    std::fprintf(stderr, "render-prompt: %zu message%s, %zu cache breakpoint%s, about %zu tokens\n", messages.size(),
                 messages.size() == 1 ? "" : "s", breakpoints, breakpoints == 1 ? "" : "s", tokens);
  }
  return 0;
}

// The dialogue prompt a fixture gives, through en-core's renderer: the prompt goldens' form, or JSON.
int render_fixture(const Options& options) {
  std::ifstream in(fs::path(options.fixture), std::ios::binary);
  if (!in) return failure("cannot read the fixture");
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  auto fixture = en::tools::parse_prompt_fixture(text);
  if (!fixture) return failure("the fixture: " + fixture.error());
  const fs::path pack = options.content.empty() ? fs::path("content") / "eldenring" : fs::path(options.content);
  auto templates = en::tools::read_pack_prompts(pack);
  if (!templates) return failure(templates.error());
  auto renderer = en::core::DialoguePrompt::load(*templates);
  if (!renderer) return failure("the pack's prompts: " + renderer.error().detail);
  const auto report = (*renderer)->report(fixture->context());
  std::string name = fs::path(options.fixture).filename().string();
  if (name.size() > 12 && name.compare(name.size() - 12, 12, ".prompt.json") == 0) name.resize(name.size() - 12);
  if (options.json) {
    nlohmann::json out = {{"fixture", name}};
    if (report) {
      nlohmann::json messages = nlohmann::json::array();
      for (const en::proto::Message& m : report->prompt.messages) {
        nlohmann::json parts = nlohmann::json::array();
        for (const en::proto::Part& p : m.parts) parts.push_back({{"text", p.text}, {"cache", p.cache_break}});
        messages.push_back({{"role", m.role == en::proto::Role::System ? "system"
                                     : m.role == en::proto::Role::User ? "user"
                                                                       : "assistant"},
                            {"content", m.joined()},
                            {"parts", parts}});
      }
      nlohmann::json blocks = nlohmann::json::array();
      for (std::size_t i = 0; i < en::core::kBlockCount; ++i) {
        blocks.push_back({{"block", en::core::kBlocks[i].number},
                          {"tokens", report->blocks[i].tokens},
                          {"cap", en::core::kBlocks[i].cap},
                          {"over_cap", report->blocks[i].over_cap}});
      }
      out["messages"] = messages;
      out["token_estimate"] = report->prompt.tokens;
      out["blocks"] = blocks;
      out["trimmed"] = {{"earlier", report->earlier_trimmed},
                        {"exchanges", report->exchanges_trimmed},
                        {"nearby", report->nearby_trimmed}};
    } else {
      out["refused"] = {{"detail", report.error().detail}};
    }
    std::printf("%s\n", out.dump(2).c_str());
  } else {
    std::fputs(en::tools::prompt_golden_text(name, report).c_str(), stdout);
  }
  if (!report) {
    std::fprintf(stderr, "render-prompt: the render was refused, so nothing would be sent: %s\n",
                 report.error().detail.c_str());
    return 1;
  }
  return 0;
}

}  // namespace

int run_render_prompt(int argc, char** argv) {
  Options options;
  if (const std::string problem = parse_arguments(argc, argv, options); !problem.empty()) {
    if (problem == "help") {
      std::fputs(usage_text, stdout);
      return 0;
    }
    return usage_error(problem);
  }
  if (options.list_functions) return list_functions(options);
  if (!options.fixture.empty()) return render_fixture(options);

  try {
    std::error_code error;
    const fs::path root = options.root.empty() ? fs::current_path(error) : fs::path(options.root);
    const std::string name = name_from_root(root, options.target);
    if (name.empty()) return failure("the template must lie inside the root (--root, or the current folder)");

    nlohmann::json context;
    const bool have_context = !options.context_file.empty();
    if (have_context) {
      std::string problem;
      if (!read_context(options.context_file, context, problem)) return failure(problem);
    }

    prompt::EnvironmentOptions environment_options;
    environment_options.assemble.merge_same_role = options.merge;
    const prompt::Environment environment(root, prompt::builtin_registry(), environment_options);
    if (options.lint) return lint(environment, name, have_context ? &context : nullptr);
    return render(environment, name, context, options);
  } catch (const prompt::TemplateError& error) {
    return failure(error.what());
  } catch (const std::exception& error) {
    return failure(error.what());
  }
}
