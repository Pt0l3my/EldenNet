// eldennet-engine probe-exe: the derivation's offline round R0 (O89; ARCH §6.2, §6.10), part of the EldenNet devkit.
//
//   eldennet-engine probe-exe <exe> --derive --leads <leads.toml> --out <derive.json> [--static <label>=<rva>]...
//
// It reads the executable file and nothing else: never a running process, never the game's memory (O73 to O75). What it
// finds is written through the numbers-only writer (en-gal's report.hpp), checked again by the lint before it is
// written, and holds only numbers, fixed words, the leads' labels and patterns of at most 64 bytes: no path, no name or
// text read from the file, no code beyond those patterns. Its messages never show a path or a value read from a file.
//
// Exit codes (for DataPiler's job, roadmap S7.11.h):
//   0  derive.json written; everything the round derives offline was derived
//   1  derive.json written; something was not derived (each item's status says what)
//   2  a usage error; nothing written
//   3  the executable could not be read, is larger than 1 GiB, or is not a PE32+ image for x64; nothing written
//   4  the leads file could not be read or was refused; nothing written
//   5  the report was refused by its own writer or lint (a defect of ours); nothing written
//   6  derive.json could not be written
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "en/gal/derive.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/report.hpp"

namespace {

constexpr std::uint64_t kMaxExeBytes = 1ull << 30;
constexpr std::uint64_t kMaxLeadsBytes = 64u << 10;

constexpr const char* kUsage =
    "usage: eldennet-engine probe-exe <exe> --derive --leads <leads.toml> --out <derive.json>\n"
    "                                 [--static <label>=<rva>]...\n"
    "\n"
    "Reads a game's executable file (never a running game) and writes derive.json: the fingerprint, the FD4\n"
    "singletons, the RTTI vtable sets, the task registration and its argument registers, the task group's index,\n"
    "candidate patterns of at most 64 bytes with their wildcards, and what the in-game round must confirm. The file\n"
    "holds only numbers, fixed words, the leads' labels and those patterns.\n"
    "\n"
    "  --derive          derive the facts the leads ask for (the offline round, R0)\n"
    "  --leads FILE      what to look for: names and shapes, never bytes\n"
    "  --out FILE        where derive.json goes (replaced if it exists)\n"
    "  --static L=RVA    a static's slot that the in-game round found (hexadecimal 0x... or decimal), so that a\n"
    "                    pattern is cut at a reference to it; repeatable\n"
    "\n"
    "Exit codes: 0 written and complete; 1 written, something not derived; 2 usage; 3 the executable refused;\n"
    "4 the leads refused; 5 the report refused (a defect); 6 the output not written.\n"
    "\n"
    "Safe use. The EldenNet devkit reads your own copy of the game on your own PC, so that you can make and check\n"
    "mods and study your own install. It never changes the game's files, keeps no keys and shares nothing.\n"
    "What we condemn: sharing the game's files, models, textures, text, sounds or keys; using the game's content\n"
    "commercially; leaking unreleased content; cheating, or any use online or against other players; getting round\n"
    "anti-cheat; using a copy of the game you do not own. The devkit is not built for any of these, and we will not\n"
    "help with them.\n";

struct Args {
  std::string exe;
  std::string leads;
  std::string out;
  bool derive = false;
  std::vector<std::pair<std::string, std::uint32_t>> statics;
};

std::optional<std::uint32_t> parse_rva(const std::string& s) {
  if (s.empty() || s.size() > 12) return std::nullopt;
  try {
    std::size_t used = 0;
    const unsigned long long v = std::stoull(s, &used, s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0 ? 16 : 10);
    if (used != s.size() || v > 0xFFFFFFFFull) return std::nullopt;
    return static_cast<std::uint32_t>(v);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// A usage problem, said without repeating what was given (it may be a path).
int usage(const char* problem) {
  std::fprintf(stderr, "probe-exe: %s\n", problem);
  std::fputs(kUsage, stderr);
  return 2;
}

// A file's bytes, read whole, when it is a regular file of at most `max` bytes.
std::optional<std::string> read_file(const std::string& path, std::uint64_t max, bool* too_large) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) return std::nullopt;
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec) return std::nullopt;
  if (size > max) {
    *too_large = true;
    return std::nullopt;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::string bytes(static_cast<std::size_t>(size), '\0');
  if (size != 0 && !in.read(bytes.data(), static_cast<std::streamsize>(size))) return std::nullopt;
  return bytes;
}

bool write_file(const std::string& path, const std::string& text) {
  const std::string part = path + ".part";
  {
    std::ofstream out(part, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) return false;
  }
  std::error_code ec;
  std::filesystem::rename(part, path, ec);
  if (ec) {
    std::filesystem::remove(part, ec);
    return false;
  }
  return true;
}

}  // namespace

int run_probe_exe(int argc, char** argv) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      std::fputs(kUsage, stdout);
      return 0;
    }
    if (a == "--derive") {
      args.derive = true;
    } else if (a == "--leads" || a == "--out" || a == "--static") {
      if (i + 1 >= argc) return usage("an option needs a value");
      const std::string v = argv[++i];
      if (a == "--leads") {
        args.leads = v;
      } else if (a == "--out") {
        args.out = v;
      } else {
        const std::size_t eq = v.find('=');
        const std::optional<std::uint32_t> rva = eq == std::string::npos ? std::nullopt : parse_rva(v.substr(eq + 1));
        if (!rva || !en::gal::report::is_label(v.substr(0, eq))) return usage("--static takes <label>=<rva>");
        args.statics.emplace_back(v.substr(0, eq), *rva);
      }
    } else if (a.rfind("--", 0) == 0) {
      return usage("an option it does not know");
    } else if (args.exe.empty()) {
      args.exe = a;
    } else {
      return usage("one executable at a time");
    }
  }
  if (!args.derive) return usage("--derive is the mode built so far (--against gal/ is still to come)");
  if (args.exe.empty() || args.leads.empty() || args.out.empty()) return usage("<exe>, --leads and --out are needed");

  // The leads first: they are small, and say what to look for.
  bool too_large = false;
  const std::optional<std::string> leads_text = read_file(args.leads, kMaxLeadsBytes, &too_large);
  if (!leads_text) {
    std::fputs(
        too_large ? "probe-exe: the leads file is larger than 64 KiB\n" : "probe-exe: the leads file cannot be read\n",
        stderr);
    return 4;
  }
  const auto leads = en::gal::derive::parse_leads(*leads_text, "leads");
  if (!leads) {
    for (const auto& e : leads.error()) std::fprintf(stderr, "probe-exe: %s\n", en::gal::describe(e).c_str());
    return 4;
  }
  for (const auto& [label, _] : args.statics) {
    bool known = false;
    for (const auto& s : leads->statics) known = known || s.label == label;
    if (!known) return usage("--static names a label the leads' statics do not have");
  }

  // The executable: its bytes, read once, as a file.
  too_large = false;
  const std::optional<std::string> exe = read_file(args.exe, kMaxExeBytes, &too_large);
  if (!exe) {
    std::fputs(
        too_large ? "probe-exe: the executable is larger than 1 GiB\n" : "probe-exe: the executable cannot be read\n",
        stderr);
    return 3;
  }
  const en::gal::SpanSource source(std::as_bytes(std::span<const char>(*exe)));
  const auto image = en::gal::read_image(source, en::gal::ImageLayout::File);
  if (!image) {
    std::fprintf(stderr, "probe-exe: the executable is not a PE32+ image for x64: %s\n",
                 en::gal::describe(image.error()).c_str());
    return 3;
  }

  en::gal::derive::Options options;
  options.file_size = exe->size();
  options.static_slots = args.statics;
  const en::gal::derive::Result result = en::gal::derive::derive(*image, source, *leads, options);
  const auto text = en::gal::derive::write_report(result);
  if (!text) {
    std::fprintf(stderr, "probe-exe: the report was refused by its writer: %s\n",
                 en::gal::report::describe(text.error()).c_str());
    return 5;
  }
  const en::gal::report::Vocabulary* known[] = {&en::gal::derive::vocabulary()};
  if (const auto linted = en::gal::report::lint(*text, known); !linted) {
    for (const auto& p : linted.error()) {
      std::fprintf(stderr, "probe-exe: the report was refused by the lint: %s\n", en::gal::report::describe(p).c_str());
    }
    return 5;
  }
  if (!write_file(args.out, *text)) {
    std::fputs("probe-exe: derive.json cannot be written\n", stderr);
    return 6;
  }

  const bool complete = en::gal::derive::complete(result);
  std::size_t singletons = 0;
  for (const auto& s : result.fd4.singletons) singletons += s.status == en::gal::derive::Status::Derived ? 1 : 0;
  std::printf(
      "probe-exe: derive.json written (%s): fd4 %s, %zu of %zu singletons; registration %s; task %s; group %s\n",
      complete ? "complete" : "incomplete", en::gal::derive::to_string(result.fd4.status), singletons,
      result.fd4.singletons.size(), en::gal::derive::to_string(result.registration.status),
      en::gal::derive::to_string(result.task.status), en::gal::derive::to_string(result.group.status));
  return complete ? 0 : 1;
}
