// eldennet-engine lodestar: the devkit's commands of Lodestar, EldenNet's own address library (proposal 0005 §9.7).
// Built so far:
//
//   eldennet-engine lodestar find <exe> --gal <folder> --out <report.json>
//
// find reads a game's executable file, never a running game (O73, O75), with what gal/ says to look for (leads.toml,
// recipes.toml; builds.toml for the build's fingerprint; ids.toml is checked to read), runs the recipes that need no
// running game (en-gal's lodestar.hpp) and writes a report of kind `lodestar`, schema 2: whole numbers, booleans,
// fixed words and the leads' labels. Its vocabulary has no key that can hold a byte, a pattern, text, a name or an
// instruction, and the report passes the lint before it is written.
//
// Exit codes: 0 written and complete; 1 written, something to read first (the accessor's count outside its range, no
// accepted site, or the shape and the withdrawn pattern not accepting the same sites); 2 usage; 3 the executable
// refused; 4 a file of gal/ refused; 5 the report refused (a defect); 6 the output not written.
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>

#include "en/gal/builds.hpp"
#include "en/gal/derive.hpp"
#include "en/gal/ids.hpp"
#include "en/gal/lodestar.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/recipes.hpp"
#include "en/gal/report.hpp"
#include "en/gal/schema.hpp"

namespace {

constexpr std::uint64_t kMaxExeBytes = 1ull << 30;

constexpr const char* kUsage =
    "usage: eldennet-engine lodestar find <exe> --gal <folder> --out <report.json>\n"
    "\n"
    "Reads a game's executable file (never a running game) and writes a report of numbers: the image's fingerprint\n"
    "and whether gal/builds.toml knows the build; where the accessor's shape stands, the sites accepted and refused,\n"
    "and every static they load with the places that load it; the classes the leads name, with their tables of\n"
    "methods; and the task groups' table. The report holds whole numbers, booleans, fixed words and the leads' own\n"
    "labels: no bytes, no patterns, no text, no names and no code of the game.\n"
    "\n"
    "  --gal FOLDER      EldenNet's gal/ folder: leads.toml and recipes.toml say what to look for\n"
    "  --out FILE        where the report goes (replaced if it exists)\n"
    "\n"
    "Exit codes: 0 written and complete; 1 written, something to read first; 2 usage; 3 the executable refused;\n"
    "4 a file of gal/ refused; 5 the report refused (a defect); 6 the output not written.\n"
    "\n"
    "Safe use. The EldenNet devkit reads your own copy of the game on your own PC, so that you can make and check\n"
    "mods and study your own install. It never changes the game's files, keeps no keys and shares nothing.\n"
    "What we condemn: sharing the game's files, models, textures, text, sounds or keys; using the game's content\n"
    "commercially; leaking unreleased content; cheating, or any use online or against other players; getting round\n"
    "anti-cheat; using a copy of the game you do not own. The devkit is not built for any of these, and we will not\n"
    "help with them.\n";

// A usage problem, said without repeating what was given (it may be a path).
int usage(const char* problem) {
  std::fprintf(stderr, "lodestar: %s\n", problem);
  std::fputs(kUsage, stderr);
  return 2;
}

// A regular file's bytes, read whole, when it holds at most `max`.
std::optional<std::string> read_file(const std::filesystem::path& path, std::uint64_t max) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) return std::nullopt;
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec || size > max) return std::nullopt;
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

// A file of gal/, read and parsed; nothing, with each problem printed (its file and place, never a value).
template <class Parse>
auto gal_file(const std::filesystem::path& dir, const char* name, bool required, bool* refused, Parse parse)
    -> std::optional<typename decltype(parse(std::string_view{}))::value_type> {
  const std::optional<std::string> text = read_file(dir / name, en::gal::kMaxGalFileBytes);
  if (!text) {
    std::error_code ec;
    if (required || std::filesystem::exists(dir / name, ec)) {
      std::fprintf(stderr, "lodestar: %s cannot be read, or is larger than 256 KiB\n", name);
      *refused = true;
    }
    return std::nullopt;
  }
  auto parsed = parse(std::string_view(*text));
  if (!parsed) {
    for (const en::gal::SchemaError& e : parsed.error()) {
      std::fprintf(stderr, "lodestar: %s\n", en::gal::describe(e).c_str());
    }
    *refused = true;
    return std::nullopt;
  }
  return std::move(*parsed);
}

int run_find(int argc, char** argv) {
  std::string exe_path;
  std::string gal;
  std::string out;
  for (int i = 0; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--help" || a == "-h") {
      std::fputs(kUsage, stdout);
      return 0;
    }
    if (a == "--gal" || a == "--out") {
      if (i + 1 >= argc) return usage("an option needs a value");
      (a == "--gal" ? gal : out) = argv[++i];
    } else if (a.rfind("--", 0) == 0) {
      return usage("an option it does not know");
    } else if (exe_path.empty()) {
      exe_path = a;
    } else {
      return usage("one executable at a time");
    }
  }
  if (exe_path.empty() || gal.empty() || out.empty()) return usage("<exe>, --gal and --out are needed");

  // gal/ first: its files are small, and say what to look for.
  namespace g = en::gal;
  const std::filesystem::path dir(gal);
  bool refused = false;
  const auto leads =
      gal_file(dir, "leads.toml", true, &refused, [](std::string_view t) { return g::derive::parse_leads(t, "leads.toml"); });
  const auto recipes =
      gal_file(dir, "recipes.toml", true, &refused, [](std::string_view t) { return g::parse_recipes(t, {}); });
  const auto builds =
      gal_file(dir, "builds.toml", false, &refused, [](std::string_view t) { return g::parse_builds(t); });
  const auto ids = gal_file(dir, "ids.toml", false, &refused, [](std::string_view t) { return g::parse_ids(t); });
  if (refused || !leads || !recipes) return 4;

  // The executable: its bytes, read once, as a file.
  const std::optional<std::string> exe = read_file(exe_path, kMaxExeBytes);
  if (!exe) {
    std::fputs("lodestar: the executable cannot be read, or is larger than 1 GiB\n", stderr);
    return 3;
  }
  const g::SpanSource source(std::as_bytes(std::span<const char>(*exe)));
  const auto image = g::read_image(source, g::ImageLayout::File);
  if (!image) {
    std::fprintf(stderr, "lodestar: the executable is not a PE32+ image for x64: %s\n",
                 g::describe(image.error()).c_str());
    return 3;
  }

  const g::lodestar::Found found =
      g::lodestar::find(*image, source, exe->size(), *leads, *recipes, builds ? &*builds : nullptr, ids ? &*ids : nullptr);
  const auto text = g::lodestar::write_report(found);
  if (!text) {
    std::fprintf(stderr, "lodestar: the report was refused by its writer: %s\n",
                 g::report::describe(text.error()).c_str());
    return 5;
  }
  const g::report::Vocabulary* known[] = {&g::lodestar::vocabulary()};
  if (const auto linted = g::report::lint(*text, known); !linted) {
    for (const auto& p : linted.error()) {
      std::fprintf(stderr, "lodestar: the report was refused by the lint: %s\n", g::report::describe(p).c_str());
    }
    return 5;
  }
  if (!write_file(out, *text)) {
    std::fputs("lodestar: the report cannot be written\n", stderr);
    return 6;
  }

  const bool complete = g::lodestar::complete(found);
  const g::lodestar::AccessorFound& a = found.accessor;
  std::size_t leads_with_class = 0;
  for (const g::lodestar::LeadClasses& c : found.lead_classes) leads_with_class += c.named + c.named_imp != 0 ? 1 : 0;
  std::printf("lodestar: report written (%s): build %s; accessor %llu places, %llu accepted, %llu statics; "
              "the withdrawn pattern %s, %llu sites only one of the two accepts; classes %zu; groups' table %s; "
              "%zu of %zu leads have a class of their name\n",
              complete ? "complete" : "to read first", found.image.known_build ? "known" : "not known",
              static_cast<unsigned long long>(a.hits), static_cast<unsigned long long>(a.accepted),
              static_cast<unsigned long long>(a.statics_found), a.pattern ? "cut" : "not cut",
              static_cast<unsigned long long>(a.only_shape + a.only_pattern), found.classes.size(),
              g::derive::to_string(found.table.status), leads_with_class, found.lead_classes.size());
  // The accessor's other shapes, in one more line: where they stand and what they reach that is new.
  if (!a.variants.empty()) {
    std::uint64_t places = 0, accepted = 0, fresh = 0;
    for (const g::lodestar::VariantFound& v : a.variants) {
      places += v.hits;
      accepted += v.accepted;
      fresh += v.statics_found - v.known;
    }
    std::printf("lodestar: the accessor's %zu other shapes: %llu places, %llu accepted, %llu new statics\n",
                a.variants.size(), static_cast<unsigned long long>(places),
                static_cast<unsigned long long>(accepted), static_cast<unsigned long long>(fresh));
  }
  return complete ? 0 : 1;
}

}  // namespace

int run_lodestar(int argc, char** argv) {
  if (argc >= 2 && std::string(argv[1]) == "find") return run_find(argc - 2, argv + 2);
  if (argc >= 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
    std::fputs(kUsage, stdout);
    return 0;
  }
  return usage("find is the command built so far (check and diff are still to come)");
}
