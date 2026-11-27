// eldennet-engine eval and mockllm (DESIGN §14, NPC §12.1, §12.2, ARCH §12.2, §12.3; WHY§364 (10); O139 to O142).
//
//   eldennet-engine eval <scenario.toml> --llm mock|recorded [--arm FOLDER] [--report FILE]
//   eldennet-engine eval --set <x1.toml> --jobpack <folder>
//   eldennet-engine eval --set <x1.toml> --jobrunner-results <results.jsonl> [--jobrunner-results ...]
//                        [--report FILE] [--summary FILE] [--questions FILE]
//   eldennet-engine mockllm --script <mock.toml> [--requests N]
//
// `eval` runs a scripted conversation (tests/scenarios/talk/) through the real core against a scripted model (mock)
// or a recorded one (recorded), on a simulated clock, checks the scenario's expectations, prints the numbers, and with
// --report writes the report: JSON with the conversation's text, the model's included, so a local-only kind, written
// only where --report says and never into the repository (eval-reports/ is ignored by git) or anywhere it is uploaded.
// Live models and the judged sets wait for a key and the judge's check (DESIGN §14). `mockllm` checks a script and
// prints the events it gives each request, one JSON line each, as `--llm recorded` reads them.
//
// With --set, X1 outside the game (eval_x1.hpp): --jobpack writes the set's jobpacks for LLM Jobrunner into a folder,
// one `<pack>.jobpack.json` each; --jobrunner-results reads the results.jsonl of Jobrunner's zips back through the
// speech and writes the local report (model text), the numbers-only summary and the test questions' replies with
// their prompts (model text), each only where its flag says. What it prints is numbers and the servers' and models'
// bounded names.
//
// Exit codes: 0 every expectation held (eval), the jobpacks were written, the results were read with none refused, or
// the script is good (mockllm); 1 an expectation failed, a scripted run of the set did not hold our history, or a
// result line was refused; 2 a usage error, or an input that cannot be read, parsed or loaded, or an output that
// cannot be written.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <vector>

#include "eval_run.hpp"
#include "eval_x1.hpp"
#include "golden_run.hpp"
#include "mockllm.hpp"

namespace {

namespace fs = std::filesystem;

void eval_usage() {
  std::fputs(
      "usage: eldennet-engine eval <scenario.toml> --llm mock|recorded [--arm FOLDER] [--report FILE]\n"
      "       eldennet-engine eval --set <x1.toml> --jobpack <folder>\n"
      "       eldennet-engine eval --set <x1.toml> --jobrunner-results <results.jsonl> [--jobrunner-results ...]\n"
      "                            [--report FILE] [--summary FILE] [--questions FILE]\n"
      "  --llm mock      the scenario's scripted model (its `mock` script; mockllm)\n"
      "  --llm recorded  the scenario's recorded model events (its `recorded` file)\n"
      "  --arm FOLDER    an overrides folder after the scenario's (the positive-wording arm's former prompts)\n"
      "  --report FILE   write the report there: it holds the conversation's text, so keep it local (eval-reports/)\n"
      "  --set FILE      X1's set (tests/x1/x1.toml): its scenarios, arms, jobpacks and repeats\n"
      "  --jobpack DIR   write the set's jobpacks for LLM Jobrunner there, one <pack>.jobpack.json each\n"
      "  --jobrunner-results FILE  a results.jsonl from a Jobrunner zip, read back through the speech\n"
      "  --summary FILE  the numbers-only summary; --questions FILE the test questions' replies with their prompts\n"
      "exit codes: 0 done; 1 an expectation failed, the set's history did not hold, or a result line was refused;\n"
      "            2 usage, an input, or an output\n",
      stderr);
}

void mock_usage() {
  std::fputs(
      "usage: eldennet-engine mockllm --script <mock.toml> [--requests N]\n"
      "  checks a mock script and prints the events it gives its first N requests (default 1), one JSON line each\n"
      "exit codes: 0 the script is good; 2 usage, or a script that cannot be read or parsed\n",
      stderr);
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

int run_x1(const std::string& set_path, const std::optional<std::string>& jobpack,
           const std::vector<std::string>& results, const std::optional<std::string>& report,
           const std::optional<std::string>& summary, const std::optional<std::string>& questions) {
  const auto text = en::tools::read_file(set_path, 256 * 1024);
  if (!text) {
    std::fputs("eval: the set cannot be read, or is larger than 256 KiB\n", stderr);
    return 2;
  }
  auto set = en::tools::parse_x1_set(*text, fs::path(set_path).parent_path());
  if (!set) {
    std::fprintf(stderr, "eval: the set: %s\n", set.error().c_str());
    return 2;
  }
  auto items = en::tools::build_x1_items(*set);
  if (!items) {
    std::fprintf(stderr, "eval: the set: %s\n", items.error().c_str());
    return 1;
  }
  if (jobpack) {
    auto packs = en::tools::x1_jobpacks(*set, *items);
    if (!packs) {
      std::fprintf(stderr, "eval: %s\n", packs.error().c_str());
      return 1;
    }
    std::error_code ec;
    fs::create_directories(*jobpack, ec);
    for (const en::tools::X1Jobpack& p : *packs) {
      const std::string bytes = en::tools::jobpack_text(p);
      if (!write_file(fs::path(*jobpack) / (p.name + ".jobpack.json"), bytes)) {
        std::fputs("eval: a jobpack cannot be written\n", stderr);
        return 2;
      }
      std::printf("jobpack %s: %zu items, %zu runs per server, %zu bytes\n", p.name.c_str(), p.items, p.runs, bytes.size());
    }
    return 0;
  }
  auto tap = en::tools::builtin_speech_tap();
  if (!tap) {
    std::fprintf(stderr, "eval: %s\n", tap.error().c_str());
    return 2;
  }
  en::tools::X1Import import;
  for (std::size_t f = 0; f < results.size(); ++f) {
    const auto body = en::tools::read_file(results[f], en::tools::kResultsFileMax);
    if (!body) {
      std::fprintf(stderr, "eval: results file %zu cannot be read, or is larger than 64 MB\n", f + 1);
      return 2;
    }
    en::tools::read_jobrunner_results(*body, f + 1, *items, import);
  }
  const en::tools::X1Outputs out = en::tools::x1_measure(*set, *items, import, **tap);
  std::fputs(out.printed.c_str(), stdout);
  const auto put = [](const std::optional<std::string>& path, const nlohmann::json& j) {
    return !path || write_file(*path, j.dump(1) + "\n");
  };
  if (!put(report, out.report) || !put(summary, out.summary) || !put(questions, out.questions)) {
    std::fputs("eval: an output cannot be written\n", stderr);
    return 2;
  }
  return import.refused.empty() ? 0 : 1;
}

int run_eval(int argc, char** argv) {
  std::optional<std::string> scenario, llm, report, arm, set, jobpack, summary, questions;
  std::vector<std::string> results;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    const bool more = i + 1 < argc;
    if (a == "--help") {
      eval_usage();
      return 0;
    }
    if (a == "--llm" && more && !llm) {
      llm = argv[++i];
    } else if (a == "--report" && more && !report) {
      report = argv[++i];
    } else if (a == "--arm" && more && !arm) {
      arm = argv[++i];
    } else if (a == "--set" && more && !set) {
      set = argv[++i];
    } else if (a == "--jobpack" && more && !jobpack) {
      jobpack = argv[++i];
    } else if (a == "--jobrunner-results" && more && results.size() < 64) {
      results.emplace_back(argv[++i]);
    } else if (a == "--summary" && more && !summary) {
      summary = argv[++i];
    } else if (a == "--questions" && more && !questions) {
      questions = argv[++i];
    } else if (a.rfind("--", 0) == 0 || scenario) {
      eval_usage();
      return 2;
    } else {
      scenario = a;
    }
  }
  if (set) {
    // X1: exactly one of --jobpack and --jobrunner-results, and none of the single scenario's flags.
    const bool one = jobpack.has_value() != !results.empty();
    const bool single = scenario || llm || arm;
    const bool outputs = report || summary || questions;
    if (!one || single || (jobpack && outputs)) {
      eval_usage();
      return 2;
    }
    return run_x1(*set, jobpack, results, report, summary, questions);
  }
  if (jobpack || !results.empty() || summary || questions) {
    eval_usage();
    return 2;
  }
  if (!scenario || !llm || (*llm != "mock" && *llm != "recorded")) {
    if (llm && *llm == "live") std::fputs("eval: --llm live waits for a key and the judge's check (DESIGN 14)\n", stderr);
    eval_usage();
    return 2;
  }
  const auto text = en::tools::read_file(*scenario, 256 * 1024);
  if (!text) {
    std::fputs("eval: the scenario cannot be read, or is larger than 256 KiB\n", stderr);
    return 2;
  }
  auto parsed = en::tools::parse_eval_scenario(*text, fs::path(*scenario).parent_path());
  if (!parsed) {
    std::fprintf(stderr, "eval: %s\n", parsed.error().c_str());
    return 2;
  }
  en::tools::EvalOptions options;
  if (arm) options.arm = fs::path(*arm);
  auto run = en::tools::run_eval(*parsed, *llm == "mock" ? en::tools::EvalLlm::Mock : en::tools::EvalLlm::Recorded,
                                 options);
  if (!run) {
    std::fprintf(stderr, "eval: %s\n", run.error().c_str());
    return 2;
  }
  std::fputs(en::tools::eval_summary(*run).c_str(), stdout);
  for (const std::string& f : run->failed) std::fprintf(stdout, "  failed: %s\n", f.c_str());
  if (report && !write_file(*report, en::tools::eval_report(*run).dump(2) + "\n")) {
    std::fputs("eval: the report cannot be written\n", stderr);
    return 2;
  }
  return run->failed.empty() ? 0 : 1;
}

int run_mockllm(int argc, char** argv) {
  std::optional<std::string> script;
  std::size_t requests = 1;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    const bool more = i + 1 < argc;
    if (a == "--help") {
      mock_usage();
      return 0;
    }
    if (a == "--script" && more && !script) {
      script = argv[++i];
    } else if (a == "--requests" && more) {
      const std::string n = argv[++i];
      if (n.empty() || n.size() > 3 || n.find_first_not_of("0123456789") != std::string::npos || std::stoul(n) == 0) {
        mock_usage();
        return 2;
      }
      requests = std::stoul(n);
    } else {
      mock_usage();
      return 2;
    }
  }
  if (!script) {
    mock_usage();
    return 2;
  }
  const auto text = en::tools::read_file(*script, 256 * 1024);
  if (!text) {
    std::fputs("mockllm: the script cannot be read, or is larger than 256 KiB\n", stderr);
    return 2;
  }
  auto parsed = en::tools::parse_mock_script(*text);
  if (!parsed) {
    std::fprintf(stderr, "mockllm: %s\n", parsed.error().c_str());
    return 2;
  }
  for (const std::string& line : en::tools::mock_lines(*parsed, requests)) std::printf("%s\n", line.c_str());
  return 0;
}
