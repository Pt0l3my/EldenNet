// eldennet-engine lint: checks of EldenNet's files (ARCH §12.3). Built so far: --report, the check every numbers-only
// report passes before it leaves the PC or is committed (the derivation round, O89; DataPiler's guard, DP36).
//
//   eldennet-engine lint --report <file>
//
// It reads the file's "report" key and checks the file against that kind's closed vocabulary (en-gal's report.hpp):
// every key known, every value of its key's type, strings only as fixed words, labels or patterns of at most 64 bytes
// in canonical form; no path, no free string, no byte run longer than a pattern, no float or null, no key given twice.
// Each problem is printed with where it is, never with what was there.
//
// Exit codes: 0 the report is clean; 1 it was refused (each problem on standard output); 2 a usage error, or the file
// could not be read or is larger than 256 KiB.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "en/gal/derive.hpp"
#include "en/gal/report.hpp"

namespace {

void lint_usage() { std::fputs("usage: eldennet-engine lint --report <file>\n", stderr); }

}  // namespace

int run_lint(int argc, char** argv) {
  std::string path;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      lint_usage();
      return 0;
    }
    if (a == "--report" && i + 1 < argc && path.empty()) {
      path = argv[++i];
      continue;
    }
    std::fputs(
        a == "--report" ? "lint: --report takes one file\n" : "lint: --report <file> is the check built so far\n",
        stderr);
    lint_usage();
    return 2;
  }
  if (path.empty()) {
    lint_usage();
    return 2;
  }
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    std::fputs("lint: the report cannot be read\n", stderr);
    return 2;
  }
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec || size > en::gal::report::kMaxReportBytes) {
    std::fputs(ec ? "lint: the report cannot be read\n" : "lint: the report is larger than 256 KiB\n", stderr);
    return 2;
  }
  std::ifstream in(path, std::ios::binary);
  std::string text(static_cast<std::size_t>(size), '\0');
  if (!in || (size != 0 && !in.read(text.data(), static_cast<std::streamsize>(size)))) {
    std::fputs("lint: the report cannot be read\n", stderr);
    return 2;
  }
  const en::gal::report::Vocabulary* known[] = {&en::gal::derive::vocabulary()};
  const auto result = en::gal::report::lint(text, known);
  if (result) {
    std::puts("lint: the report is clean");
    return 0;
  }
  for (const auto& p : result.error()) std::printf("lint: %s\n", en::gal::report::describe(p).c_str());
  std::printf("lint: the report is refused (%zu problems)\n", result.error().size());
  return 1;
}
