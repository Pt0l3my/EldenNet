// eldennet-engine lint: checks of EldenNet's files (ARCH §12.3). Built so far: --report, the check every numbers-only
// report passes before it leaves the PC or is committed (the derivation round, O89; DataPiler's guard, DP36),
// --recording, the same check for each line of a recording (ARCH §7.5, in eldennet.dll's numbers-only form), and --gal,
// the check of a gal/ folder before it is committed or packaged (ARCH §6.3).
//
//   eldennet-engine lint --report <file>
//   eldennet-engine lint --recording <file>
//   eldennet-engine lint --gal <folder>
//
// --report reads the file's "report" key and checks the file against that kind's closed vocabulary (en-gal's
// report.hpp): `derive` (derive.json), `lodestar` (lodestar-found.json), spike.cpp's `rec_stats`, `replay` and
// `spike`, or eldennet.dll's report of an S0 run, `spike_run` (eldennet-spike.json; en_shell's spikerun.hpp): every key
// known, every
// value of its key's type, strings only as fixed words, labels or patterns of at most 64 bytes in canonical form; no
// path, no free string, no byte run longer than a pattern, no float or null, no key given twice. --recording checks a
// JSON Lines file whose every line is a report of the kind `recording` (en_shell's record.hpp), where a key may hold a
// finite number. Each problem is printed with where it is (a recording's with its line), never with what was there.
//
// --gal reads builds.toml, ids.toml, recipes.toml, patterns.toml and leads.toml as the DLL and probe-exe do (each
// optional but builds.toml; a recipe needs its derivation note as a pattern does, and names only builds builds.toml
// holds), and each build's layout with load_layout's checks in a dev build's mode (leads allowed; what the read
// groups need is the DLL's to check at boot); every pattern needs its derivation note, derivations/<id>.md (ARCH §6.3,
// DESIGN O27), and every build a pattern lists as confirmed must be one builds.toml holds. ids.toml is Lodestar's
// catalogue (proposal 0005 §4): a layout that holds a table's items (schema 2) is checked against it, each item under
// the catalogue's label, number and kind, and is refused when the folder has no catalogue.
//
// Exit codes: 0 clean; 1 refused (each problem on standard output); 2 a usage error, or the file could not be read or
// is larger than 256 KiB (a report) or 512 MiB (a recording), or the folder holds no builds.toml.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "en/gal/builds.hpp"
#include "en/gal/derive.hpp"
#include "en/gal/ids.hpp"
#include "en/gal/layout.hpp"
#include "en/gal/lodestar.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/recipes.hpp"
#include "en/gal/report.hpp"
#include "en/shell/record.hpp"
#include "en/shell/spikerun.hpp"

// spike.cpp's reports: rec-stats, replay and spike-report.
const en::gal::report::Vocabulary& rec_stats_vocabulary();
const en::gal::report::Vocabulary& replay_vocabulary();
const en::gal::report::Vocabulary& spike_vocabulary();

namespace {

void lint_usage() {
  std::fputs("usage: eldennet-engine lint --report <file> | --recording <file> | --gal <folder>\n", stderr);
}

constexpr std::uintmax_t kMaxRecordingBytes = 512ull << 20;

std::optional<std::string> read_file(const std::filesystem::path& path, std::uintmax_t max_bytes) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) return std::nullopt;
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec || size > max_bytes) return std::nullopt;
  std::ifstream in(path, std::ios::binary);
  std::string text(static_cast<std::size_t>(size), '\0');
  if (!in || (size != 0 && !in.read(text.data(), static_cast<std::streamsize>(size)))) return std::nullopt;
  return text;
}

// gal/: every file read as the engine reads it, every problem printed with its file and place (never a value).
int lint_gal(const std::filesystem::path& dir) {
  namespace gal = en::gal;
  std::size_t problems = 0;
  const auto say = [&](const std::string& line) {
    std::printf("lint: %s\n", line.c_str());
    ++problems;
  };
  const auto errors = [&](const gal::SchemaErrors& es) {
    for (const gal::SchemaError& e : es) say(gal::describe(e));
  };
  const std::optional<std::string> builds_text = read_file(dir / "builds.toml", gal::kMaxGalFileBytes);
  if (!builds_text) {
    std::fputs("lint: the folder holds no builds.toml that can be read (or it is larger than 256 KiB)\n", stderr);
    return 2;
  }
  const auto builds = gal::parse_builds(*builds_text, "builds.toml");
  if (!builds) errors(builds.error());
  // Lodestar's catalogue, which the layouts' items are checked against.
  std::optional<gal::Catalogue> catalogue;
  if (const std::optional<std::string> text = read_file(dir / "ids.toml", gal::kMaxGalFileBytes)) {
    auto parsed = gal::parse_ids(*text, "ids.toml");
    if (parsed) {
      catalogue = std::move(*parsed);
    } else {
      errors(parsed.error());
    }
  } else if (std::error_code ec; std::filesystem::exists(dir / "ids.toml", ec)) {
    say("ids.toml: cannot be read, or is larger than 256 KiB");
  }
  std::size_t layouts = 0;
  if (builds) {
    gal::LayoutNeeds needs;
    needs.task = false;  // what the groups need, the [task] table included, is the DLL's to check at boot
    needs.ids = catalogue ? &*catalogue : nullptr;
    for (const gal::BuildEntry& e : builds->entries) {
      const std::optional<std::string> text = read_file(dir / e.layout, gal::kMaxGalFileBytes);
      if (!text) {
        say(e.layout + ": cannot be read, or is larger than 256 KiB (builds.toml names it for " + e.key + ")");
        continue;
      }
      ++layouts;
      errors(gal::check_layout(e, *text, needs, gal::LayoutMode::Dev, e.layout));
    }
  }
  std::size_t patterns = 0;
  std::size_t unconfirmed = 0;
  if (const std::optional<std::string> text = read_file(dir / "patterns.toml", gal::kMaxGalFileBytes)) {
    const auto set = gal::parse_patterns(*text, {}, "patterns.toml");
    if (!set) errors(set.error());
    for (std::size_t i = 0; set && i < set->patterns.size(); ++i) {
      const gal::PatternSpec& p = set->patterns[i];
      ++patterns;
      if (p.builds.empty()) ++unconfirmed;
      const std::optional<std::string> note = read_file(dir / "derivations" / (p.id + ".md"), 64u << 10);
      if (!note || note->find_first_not_of(" \t\r\n") == std::string::npos) {
        say("patterns.toml: pattern[" + std::to_string(i) + "]: has no derivation note, derivations/" + p.id + ".md");
      }
      for (const std::string& b : p.builds) {
        const bool known = builds && std::any_of(builds->entries.begin(), builds->entries.end(),
                                                 [&](const gal::BuildEntry& e) { return e.key == b; });
        if (!known) say("patterns.toml: pattern[" + std::to_string(i) + "].builds: names a build builds.toml lacks");
      }
    }
  } else if (std::error_code ec; std::filesystem::exists(dir / "patterns.toml", ec)) {
    say("patterns.toml: cannot be read, or is larger than 256 KiB");
  }
  // Lodestar's recipes: shapes in our own grammar, each with its note, as a pattern has (O27).
  std::size_t recipes = 0;
  if (const std::optional<std::string> text = read_file(dir / "recipes.toml", gal::kMaxGalFileBytes)) {
    const auto set = gal::parse_recipes(*text, {}, "recipes.toml");
    if (!set) errors(set.error());
    for (std::size_t i = 0; set && i < set->recipes.size(); ++i) {
      const gal::RecipeSpec& r = set->recipes[i];
      ++recipes;
      const std::optional<std::string> note = read_file(dir / "derivations" / (r.id + ".md"), 64u << 10);
      if (!note || note->find_first_not_of(" \t\r\n") == std::string::npos) {
        say("recipes.toml: recipe[" + std::to_string(i) + "]: has no derivation note, derivations/" + r.id + ".md");
      }
      for (const std::string& b : r.builds) {
        const bool known = builds && std::any_of(builds->entries.begin(), builds->entries.end(),
                                                 [&](const gal::BuildEntry& e) { return e.key == b; });
        if (!known) say("recipes.toml: recipe[" + std::to_string(i) + "].builds: names a build builds.toml lacks");
      }
    }
  } else if (std::error_code ec; std::filesystem::exists(dir / "recipes.toml", ec)) {
    say("recipes.toml: cannot be read, or is larger than 256 KiB");
  }
  bool leads = false;
  if (const std::optional<std::string> text = read_file(dir / "leads.toml", 64u << 10)) {
    leads = true;
    const auto parsed = en::gal::derive::parse_leads(*text, "leads.toml");
    if (!parsed) errors(parsed.error());
  } else if (std::error_code ec; std::filesystem::exists(dir / "leads.toml", ec)) {
    say("leads.toml: cannot be read, or is larger than 64 KiB");
  }
  if (problems == 0) {
    std::printf("lint: gal/ is clean: builds %zu, layouts %zu, patterns %zu (confirmed in no build: %zu), leads.toml %s, "
                "ids %zu, recipes %zu\n",
                builds ? builds->entries.size() : 0, layouts, patterns, unconfirmed, leads ? "read" : "absent",
                catalogue ? catalogue->entries().size() : 0, recipes);
    return 0;
  }
  std::printf("lint: gal/ is refused (%zu problems)\n", problems);
  return 1;
}

}  // namespace

int run_lint(int argc, char** argv) {
  std::string path;
  bool recording = false;
  bool gal = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      lint_usage();
      return 0;
    }
    if ((a == "--report" || a == "--recording" || a == "--gal") && i + 1 < argc && path.empty()) {
      recording = a == "--recording";
      gal = a == "--gal";
      path = argv[++i];
      continue;
    }
    std::fputs(a == "--report" || a == "--recording" || a == "--gal"
                   ? "lint: --report, --recording and --gal take one file or folder\n"
                   : "lint: --report <file>, --recording <file> and --gal <folder> are the checks built so far\n",
               stderr);
    lint_usage();
    return 2;
  }
  if (path.empty()) {
    lint_usage();
    return 2;
  }
  if (gal) return lint_gal(path);
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    std::fputs("lint: the report cannot be read\n", stderr);
    return 2;
  }
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  // A report is read up to the most any kind allows; its own kind's limit is the lint's to check.
  if (ec || size > (recording ? kMaxRecordingBytes : en::gal::report::kMaxLargeReportBytes)) {
    std::fputs(ec           ? "lint: the report cannot be read\n"
               : recording ? "lint: the recording is larger than 512 MiB\n"
                           : "lint: the report is larger than 1 MiB\n",
               stderr);
    return 2;
  }
  std::ifstream in(path, std::ios::binary);
  std::string text(static_cast<std::size_t>(size), '\0');
  if (!in || (size != 0 && !in.read(text.data(), static_cast<std::streamsize>(size)))) {
    std::fputs("lint: the report cannot be read\n", stderr);
    return 2;
  }
  const en::gal::report::Vocabulary* reports[] = {&en::gal::derive::vocabulary(), &en::gal::lodestar::vocabulary(),
                                                   &rec_stats_vocabulary(), &replay_vocabulary(), &spike_vocabulary(),
                                                   &en::shell::spike_run_vocabulary()};
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
