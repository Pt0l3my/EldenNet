// eldennet-engine eval and mockllm (DESIGN §14, NPC §12.1, ARCH §12.2, §12.3; WHY§364 (10)).
//
//   eldennet-engine eval <scenario.toml> --llm mock|recorded [--report FILE]
//   eldennet-engine mockllm --script <mock.toml> [--requests N]
//
// `eval` runs a scripted conversation (tests/scenarios/talk/) through the real core against a scripted model (mock)
// or a recorded one (recorded), on a simulated clock, checks the scenario's expectations, prints the numbers, and with
// --report writes the report: JSON with the conversation's text, the model's included, so a local-only kind, written
// only where --report says and never into the repository (eval-reports/ is ignored by git) or anywhere it is uploaded.
// Live models and the judged sets wait for a key and the judge's check (DESIGN §14). `mockllm` checks a script and
// prints the events it gives each request, one JSON line each, as `--llm recorded` reads them.
//
// Exit codes: 0 every expectation held (eval) or the script is good (mockllm); 1 an expectation failed; 2 a usage
// error, or an input that cannot be read, parsed or loaded, or a report that cannot be written.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "eval_run.hpp"
#include "golden_run.hpp"
#include "mockllm.hpp"

namespace {

namespace fs = std::filesystem;

void eval_usage() {
  std::fputs(
      "usage: eldennet-engine eval <scenario.toml> --llm mock|recorded [--report FILE]\n"
      "  --llm mock      the scenario's scripted model (its `mock` script; mockllm)\n"
      "  --llm recorded  the scenario's recorded model events (its `recorded` file)\n"
      "  --report FILE   write the report there: it holds the conversation's text, so keep it local (eval-reports/)\n"
      "exit codes: 0 every expectation held; 1 an expectation failed; 2 usage, an input, or the report\n",
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

int run_eval(int argc, char** argv) {
  std::optional<std::string> scenario, llm, report;
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
    } else if (a.rfind("--", 0) == 0 || scenario) {
      eval_usage();
      return 2;
    } else {
      scenario = a;
    }
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
  auto run = en::tools::run_eval(*parsed, *llm == "mock" ? en::tools::EvalLlm::Mock : en::tools::EvalLlm::Recorded);
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
