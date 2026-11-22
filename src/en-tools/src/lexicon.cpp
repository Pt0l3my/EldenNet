// eldennet-engine lexicon: the lint's lexicon (NPC §8.3; O107).
//
//   eldennet-engine lexicon fingerprint [--toml]   words on standard input, one a line, to their fingerprints
//   eldennet-engine lexicon check [--content DIR]  the pack's banned.toml and safety.toml, read as the lint reads them
//
// fingerprint never writes the words anywhere: not to a file, a log or its own output, and a line it refuses is named
// by its number alone. Exit codes: 0 done; 1 a line refused (the others still printed) or a lexicon that fails its
// checks; 2 a usage error or a file that cannot be read.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>

#include "en/core/speech_lint.hpp"
#include "prompt_fixture.hpp"

namespace {

namespace fs = std::filesystem;

constexpr const char* kUsage =
    "usage: eldennet-engine lexicon fingerprint [--toml]\n"
    "       eldennet-engine lexicon check [--content <pack>]\n"
    "\n"
    "  fingerprint  reads words or short phrases (at most three words) from standard input, one a line, and prints\n"
    "               the safety list's fingerprint of each (O107): SHA-256, in hex, of \"eldennet.safety.v1:\" and the\n"
    "               word as the lint normalises it; sorted, without repeats, one a line (--toml: as entries of a TOML\n"
    "               list). The words are written nowhere: not to a file, a log or this output. A line that\n"
    "               normalises to no word or to more than three is named by its number alone.\n"
    "  check        reads the pack's lexicon/banned.toml and safety.toml (--content, content/eldenring by default)\n"
    "               as the lint reads them, and prints each list's size\n";

int fingerprint(bool toml) {
  std::set<std::string> out;
  std::string line;
  std::size_t number = 0;
  int status = 0;
  while (std::getline(std::cin, line)) {
    ++number;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.find_first_not_of(" \t") == std::string::npos) continue;
    const std::optional<std::string> fp = en::core::safety_fingerprint(line);
    line.assign(line.size(), '\0');  // the word is not kept past its fingerprint
    if (!fp) {
      std::fprintf(stderr, "lexicon: line %zu normalises to no word, or to more than three: left out\n", number);
      status = 1;
      continue;
    }
    out.insert(*fp);
  }
  for (const std::string& fp : out) {
    if (toml) {
      std::printf("  \"%s\",\n", fp.c_str());
    } else {
      std::printf("%s\n", fp.c_str());
    }
  }
  std::fprintf(stderr, "lexicon: %zu fingerprint%s\n", out.size(), out.size() == 1 ? "" : "s");
  return status;
}

int check(const fs::path& pack) {
  auto lex = en::tools::read_pack_lexicon(pack);
  if (!lex) {
    std::fprintf(stderr, "lexicon: %s\n", lex.error().c_str());
    return 1;
  }
  std::printf(
      "lexicon: banned %zu, promise %zu, wider %zu, archaic %zu, interjections %zu, safety always %zu, "
      "safety gentle %zu\n",
      lex->banned.size(), lex->promise.size(), lex->wider.size(), lex->archaic.size(), lex->interjections.size(),
      lex->safety_always.size(), lex->safety_gentle.size());
  return 0;
}

}  // namespace

int run_lexicon(int argc, char** argv) {
  if (argc < 2) {
    std::fputs(kUsage, stderr);
    return 2;
  }
  const std::string sub = argv[1];
  if (sub == "--help" || sub == "-h") {
    std::fputs(kUsage, stdout);
    return 0;
  }
  if (sub == "fingerprint") {
    bool toml = false;
    for (int i = 2; i < argc; ++i) {
      const std::string a = argv[i];
      if (a == "--toml") {
        toml = true;
      } else if (a == "--help" || a == "-h") {
        std::fputs(kUsage, stdout);
        return 0;
      } else {
        std::fputs(kUsage, stderr);
        return 2;
      }
    }
    return fingerprint(toml);
  }
  if (sub == "check") {
    fs::path pack = fs::path("content") / "eldenring";
    for (int i = 2; i < argc; ++i) {
      const std::string a = argv[i];
      if (a == "--content" && i + 1 < argc) {
        pack = argv[++i];
      } else if (a == "--help" || a == "-h") {
        std::fputs(kUsage, stdout);
        return 0;
      } else {
        std::fputs(kUsage, stderr);
        return 2;
      }
    }
    return check(pack);
  }
  std::fputs(kUsage, stderr);
  return 2;
}
