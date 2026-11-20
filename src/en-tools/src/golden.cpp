// eldennet-engine golden: a route's golden file (ARCH §8.12; the first in-game build's part 11).
//
//   eldennet-engine golden [check|update] <route.scn | recording.ensnap.jsonl> [--golden FILE] [--changes FILE]
//
// Runs a scenario (tests/scenarios/*.scn, the synthetic DSL) or a recording through en-core and Augur, checks a
// scenario's expectations, and compares the events, in the golden files' form, with the route's golden file
// (tests/golden/<route>.events.jsonl beside the scenarios' folder, or --golden): exactly, times included. `check` (the
// default) prints our own diff on a difference; `update` (or --update) rewrites the file, prints the diff, and the
// line tests/golden/CHANGES.md needs, since a golden file that changes without its entry there fails `check`.
//
// Exit codes: 0 the golden agrees (check) or was written (update); 1 it differs, an expectation failed, or CHANGES.md
// lacks the golden's entry; 2 a usage error, an input that cannot be read or parsed, or a file that cannot be written.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "en/augur/rules.hpp"
#include "golden_run.hpp"
#include "scn.hpp"

namespace {

namespace fs = std::filesystem;

void usage() {
  std::fputs(
      "usage: eldennet-engine golden [check|update] <route.scn | recording.ensnap.jsonl> [--golden FILE] [--changes FILE]\n"
      "  check   compare the run's events with the golden file (the default); our own diff on a difference\n"
      "  update  rewrite the golden file (also --update), print the diff and the line CHANGES.md needs\n"
      "  --golden FILE   the golden file (default: ../golden/<route>.events.jsonl beside the input's folder)\n"
      "  --changes FILE  the golden files' change log (default: CHANGES.md beside the golden file)\n"
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

int run_golden(int argc, char** argv) {
  bool update = false;
  std::optional<std::string> input, golden, changes;
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
  std::string route;
  std::vector<en::tools::Step> steps;
  std::optional<en::tools::Scenario> scenario;
  std::map<std::uint64_t, std::string> names;
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
    std::printf("golden: %s: %zu events written; CHANGES.md needs a line naming `%s` and %s\n", route.c_str(),
                run->events.size(), route.c_str(), hash.c_str());
    return 0;
  }
  if (!old) {
    std::fprintf(stderr, "golden: %s: the golden file cannot be read: run golden update first\n", route.c_str());
    return 1;
  }
  if (const std::string diff = en::tools::line_diff(*old, text); !diff.empty()) {
    std::fputs(diff.c_str(), stdout);
    std::fprintf(stderr, "golden: %s: the events differ from the golden file\n", route.c_str());
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
  if (status == 0) std::printf("golden: %s: %zu events agree (%s)\n", route.c_str(), run->events.size(), hash.c_str());
  return status;
}
