// eldennet-engine lint: checks of EldenNet's files (ARCH §12.3). Built so far: --report, the check every numbers-only
// report passes before it leaves the PC or is committed (the derivation round, O89; DataPiler's guard, DP36), and
// --recording, the same check for each line of a recording (ARCH §7.5, in eldennet.dll's numbers-only form).
//
//   eldennet-engine lint --report <file>
//   eldennet-engine lint --recording <file>
//
// --report reads the file's "report" key and checks the file against that kind's closed vocabulary (en-gal's
// report.hpp): `derive` (derive.json and R1's r1-derive.json) or `probe` (R1's r1-probe.json): every key known, every
// value of its key's type, strings only as fixed words, labels or patterns of at most 64 bytes in canonical form; no
// path, no free string, no byte run longer than a pattern, no float or null, no key given twice. --recording checks a
// JSON Lines file whose every line is a report of the kind `recording` (en_shell's record.hpp), where a key may hold a
// finite number. Each problem is printed with where it is (a recording's with its line), never with what was there.
//
// Exit codes: 0 clean; 1 refused (each problem on standard output); 2 a usage error, or the file could not be read or
// is larger than 256 KiB (a report) or 512 MiB (a recording).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "en/gal/derive.hpp"
#include "en/gal/report.hpp"
#include "en/shell/probe_report.hpp"
#include "en/shell/record.hpp"

namespace {

void lint_usage() { std::fputs("usage: eldennet-engine lint --report <file> | --recording <file>\n", stderr); }

constexpr std::uintmax_t kMaxRecordingBytes = 512ull << 20;

}  // namespace

int run_lint(int argc, char** argv) {
  std::string path;
  bool recording = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      lint_usage();
      return 0;
    }
    if ((a == "--report" || a == "--recording") && i + 1 < argc && path.empty()) {
      recording = a == "--recording";
      path = argv[++i];
      continue;
    }
    std::fputs(a == "--report" || a == "--recording" ? "lint: --report and --recording take one file\n"
                                                     : "lint: --report <file> and --recording <file> are the checks "
                                                       "built so far\n",
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
  if (ec || size > (recording ? kMaxRecordingBytes : en::gal::report::kMaxReportBytes)) {
    std::fputs(ec           ? "lint: the report cannot be read\n"
               : recording ? "lint: the recording is larger than 512 MiB\n"
                           : "lint: the report is larger than 256 KiB\n",
               stderr);
    return 2;
  }
  std::ifstream in(path, std::ios::binary);
  std::string text(static_cast<std::size_t>(size), '\0');
  if (!in || (size != 0 && !in.read(text.data(), static_cast<std::streamsize>(size)))) {
    std::fputs("lint: the report cannot be read\n", stderr);
    return 2;
  }
  const en::gal::report::Vocabulary* reports[] = {&en::gal::derive::vocabulary(), &en::shell::probe_vocabulary()};
  const en::gal::report::Vocabulary* recordings[] = {&en::shell::recording_vocabulary()};
  const auto result = recording ? en::gal::report::lint_lines(text, recordings) : en::gal::report::lint(text, reports);
  const char* what = recording ? "recording" : "report";
  if (result) {
    std::printf("lint: the %s is clean\n", what);
    return 0;
  }
  for (const auto& p : result.error()) std::printf("lint: %s\n", en::gal::report::describe(p).c_str());
  std::printf("lint: the %s is refused (%zu problems)\n", what, result.error().size());
  return 1;
}
