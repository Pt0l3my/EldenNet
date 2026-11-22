// eldennet-engine golden: a route's golden file (ARCH §8.12; the first in-game build's part 11), and a prompt
// fixture's (the first conversation's part A: MEM §12.11's T5 and the prompt goldens).
//
//   eldennet-engine golden [check|update] <route.scn | recording.ensnap.jsonl | fixture.prompt.json>
//                          [--golden FILE] [--changes FILE] [--content DIR]
//
// Runs a scenario (tests/scenarios/*.scn, the synthetic DSL) or a recording through en-core and Augur, checks a
// scenario's expectations, and compares the events, in the golden files' form, with the route's golden file
// (tests/golden/<route>.events.jsonl beside the scenarios' folder, or --golden): exactly, times included. `check` (the
// default) prints our own diff on a difference; `update` (or --update) rewrites the file, prints the diff, and the
// line tests/golden/CHANGES.md needs, since a golden file that changes without its entry there fails `check`.
//
// Exit codes: 0 the golden agrees (check) or was written (update); 1 it differs, an expectation failed, or CHANGES.md
// lacks the golden's entry; 2 a usage error, an input that cannot be read or parsed, or a file that cannot be written.
//
// A prompt fixture (tests/fixtures/prompt/*.prompt.json, src/en-tools/lib/prompt_fixture.hpp) is rendered by en-core's
// dialogue renderer over the content pack's prompts (--content, else the nearest content/eldenring above the fixture),
// twice, each time with templates loaded afresh: the two must agree byte for byte (T5's byte stability), and the
// golden form (the budget, each block against its cap, the trim, the messages) is compared with
// golden/<name>.prompt.txt beside the fixture, its CHANGES.md beside it, as a route's is.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "en/augur/rules.hpp"
#include "golden_run.hpp"
#include "prompt_fixture.hpp"
#include "scn.hpp"

namespace {

namespace fs = std::filesystem;

void usage() {
  std::fputs(
      "usage: eldennet-engine golden [check|update] <route.scn | recording.ensnap.jsonl | fixture.prompt.json>\n"
      "                              [--golden FILE] [--changes FILE] [--content DIR]\n"
      "  check   compare the run's events, or the fixture's prompt, with the golden file (the default); our own\n"
      "          diff on a difference\n"
      "  update  rewrite the golden file (also --update), print the diff and the line CHANGES.md needs\n"
      "  --golden FILE   the golden file (default: ../golden/<route>.events.jsonl beside the input's folder)\n"
      "  --changes FILE  the golden files' change log (default: CHANGES.md beside the golden file)\n"
      "  --content DIR   a prompt fixture's content pack (default: the nearest content/eldenring above it)\n"
      "  a prompt fixture's golden is golden/<name>.prompt.txt beside it; its prompt is rendered twice and must\n"
      "  agree byte for byte (T5)\n"
      "exit codes: 0 agrees or written; 1 differs, an expectation failed or CHANGES.md lacks its entry; 2 usage\n",
      stderr);
}

bool ends_with(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

bool write_file(const fs::path& path, const std::string& text) {
  const fs::path part = path.string() + ".part";
  {
    std::ofstream f(part, std::ios::binary | std::ios::trunc);
    if (!f || !(f << text)) return false;
  }
  std::error_code ec;
  fs::rename(part, path, ec);
  return !ec;
}

}  // namespace

namespace {

// The golden's check or update, a route's or a fixture's alike: the diff, the write, and CHANGES.md's entry.
int compare(const std::string& route, const std::string& text, const fs::path& gpath, const fs::path& cpath, bool update,
            int status, const std::string& what) {
  const std::string hash = en::augur::hash_text(en::augur::fnv1a64(text));
  const auto old = en::tools::read_file(gpath.string(), 64ull << 20);
  if (update) {
    if (status != 0) {
      std::fputs("golden: an expectation failed: the golden file is left as it was\n", stderr);
      return 1;
    }
    const std::string diff = old ? en::tools::line_diff(*old, text) : std::string("(a new golden file)\n");
    if (!write_file(gpath, text)) {
      std::fputs("golden: the golden file cannot be written\n", stderr);
      return 2;
    }
    std::fputs(diff.empty() ? "golden: unchanged\n" : diff.c_str(), stdout);
    std::printf("golden: %s: %s written; CHANGES.md needs a line naming `%s` and %s\n", route.c_str(), what.c_str(),
                route.c_str(), hash.c_str());
    return 0;
  }
  if (!old) {
    std::fprintf(stderr, "golden: %s: the golden file cannot be read: run golden update first\n", route.c_str());
    return 1;
  }
  if (const std::string diff = en::tools::line_diff(*old, text); !diff.empty()) {
    std::fputs(diff.c_str(), stdout);
    std::fprintf(stderr, "golden: %s: the run differs from the golden file\n", route.c_str());
    return 1;
  }
  // A golden file that changed without its entry in CHANGES.md fails (ARCH §8.12).
  const auto log = en::tools::read_file(cpath.string(), 1ull << 20);
  bool entry = false;
  if (log) {
    std::size_t at = 0;
    while (at < log->size() && !entry) {
      std::size_t end = log->find('\n', at);
      if (end == std::string::npos) end = log->size();
      const std::string_view line(log->data() + at, end - at);
      entry = line.find("`" + route + "`") != std::string_view::npos && line.find(hash) != std::string_view::npos;
      at = end + 1;
    }
  }
  if (!entry) {
    std::fprintf(stderr, "golden: %s: CHANGES.md has no entry naming `%s` and %s\n", route.c_str(), route.c_str(),
                 hash.c_str());
    return 1;
  }
  if (status == 0) std::printf("golden: %s: %s agree (%s)\n", route.c_str(), what.c_str(), hash.c_str());
  return status;
}

std::optional<fs::path> nearest_pack(fs::path from) {
  for (int up = 0; up < 8 && !from.empty(); ++up) {
    const fs::path pack = from / "content" / "eldenring";
    std::error_code ec;
    if (fs::is_directory(pack / "prompts", ec)) return pack;
    if (from == from.parent_path()) break;
    from = from.parent_path();
  }
  return std::nullopt;
}

int golden_prompt(const fs::path& in, bool update, const std::optional<std::string>& golden,
                  const std::optional<std::string>& changes, const std::optional<std::string>& content) {
  const auto text = en::tools::read_file(in.string(), 256 * 1024);
  if (!text) {
    std::fputs("golden: the fixture cannot be read, or is larger than 256 KiB\n", stderr);
    return 2;
  }
  auto fixture = en::tools::parse_prompt_fixture(*text);
  if (!fixture) {
    std::fprintf(stderr, "golden: the fixture: %s\n", fixture.error().c_str());
    return 2;
  }
  std::error_code ec;
  const std::optional<fs::path> pack =
      content ? std::optional<fs::path>(fs::path(*content)) : nearest_pack(fs::absolute(in, ec).parent_path());
  if (!pack) {
    std::fputs("golden: no content pack found above the fixture: name one with --content\n", stderr);
    return 2;
  }
  const std::string file = in.filename().string();
  const std::string name = file.substr(0, file.size() - std::string_view(".prompt.json").size());
  // Twice, each from templates loaded afresh: the same input renders byte for byte the same (T5).
  std::string rendered[2];
  for (std::string& out : rendered) {
    auto templates = en::tools::read_pack_prompts(*pack);
    if (!templates) {
      std::fprintf(stderr, "golden: %s\n", templates.error().c_str());
      return 2;
    }
    auto renderer = en::core::DialoguePrompt::load(*templates);
    if (!renderer) {
      std::fprintf(stderr, "golden: the pack's prompts: %s\n", renderer.error().detail.c_str());
      return 2;
    }
    out = en::tools::prompt_golden_text(name, (*renderer)->report(fixture->context()));
  }
  if (rendered[0] != rendered[1]) {
    std::fprintf(stderr, "golden: %s: two renders of one fixture differ (T5)\n", name.c_str());
    return 1;
  }
  const fs::path gpath = golden ? fs::path(*golden) : in.parent_path() / "golden" / (name + ".prompt.txt");
  const fs::path cpath = changes ? fs::path(*changes) : gpath.parent_path() / "CHANGES.md";
  return compare(name, rendered[0], gpath, cpath, update, 0, "the prompt and its report");
}

}  // namespace

int run_golden(int argc, char** argv) {
  bool update = false;
  std::optional<std::string> input, golden, changes, content;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    const bool more = i + 1 < argc;
    if (a == "--help") {
      usage();
      return 0;
    }
    if (a == "--update") {
      update = true;
    } else if (a == "--golden" && more && !golden) {
      golden = argv[++i];
    } else if (a == "--changes" && more && !changes) {
      changes = argv[++i];
    } else if (a == "--content" && more && !content) {
      content = argv[++i];
    } else if ((a == "check" || a == "update") && !input && i == 1) {
      update = update || a == "update";
    } else if (a.rfind("--", 0) == 0 || input) {
      usage();
      return 2;
    } else {
      input = a;
    }
  }
  if (!input) {
    usage();
    return 2;
  }
  const fs::path in(*input);
  if (ends_with(*input, ".prompt.json")) return golden_prompt(in, update, golden, changes, content);
  if (content) {
    usage();
    return 2;
  }
  std::string route;
  std::vector<en::tools::Step> steps;
  std::optional<en::tools::Scenario> scenario;
  en::tools::Names names;
  if (ends_with(*input, ".scn")) {
    const auto text = en::tools::read_file(*input, 256 * 1024);
    if (!text) {
      std::fputs("golden: the scenario cannot be read, or is larger than 256 KiB\n", stderr);
      return 2;
    }
    auto parsed = en::tools::parse_scn(*text);
    if (!parsed) {
      std::fprintf(stderr, "golden: line %zu: %s\n", parsed.error().line, parsed.error().problem.c_str());
      return 2;
    }
    auto sim = en::tools::simulate(*parsed);
    if (!sim) {
      std::fprintf(stderr, "golden: line %zu: %s\n", sim.error().line, sim.error().problem.c_str());
      return 2;
    }
    route = parsed->route;
    scenario = std::move(*parsed);
    steps = std::move(sim->steps);
    names = std::move(sim->names);
  } else if (ends_with(*input, ".ensnap.jsonl")) {
    const auto lines = en::tools::load_recording(*input, "golden");
    if (!lines) return 2;
    std::string stem = in.filename().string();
    route = stem.substr(0, stem.size() - std::string_view(".ensnap.jsonl").size());
    steps = en::tools::steps_of(*lines);
  } else {
    std::fputs("golden: the input is a scenario (.scn) or a recording (.ensnap.jsonl)\n", stderr);
    return 2;
  }
  auto run = en::tools::run_core(steps);
  if (!run) {
    for (const auto& e : run.error()) std::fprintf(stderr, "golden: %s\n", en::augur::describe(e).c_str());
    return 2;
  }
  int status = 0;
  if (scenario) {
    for (const std::string& f : en::tools::check_expects(*scenario, run->events, names)) {
      std::fprintf(stderr, "golden: %s\n", f.c_str());
      status = 1;
    }
  }
  const std::string text = en::tools::golden_text(route, in.filename().string(), *run);
  const fs::path gpath = golden ? fs::path(*golden) : in.parent_path().parent_path() / "golden" / (route + ".events.jsonl");
  const fs::path cpath = changes ? fs::path(*changes) : gpath.parent_path() / "CHANGES.md";
  return compare(route, text, gpath, cpath, update, status, std::to_string(run->events.size()) + " events");
}
