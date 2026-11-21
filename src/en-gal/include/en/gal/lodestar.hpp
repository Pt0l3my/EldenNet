#pragma once
// Lodestar's finder (proposal 0005 §6.2, §11; O100, O102): what `eldennet-engine lodestar find` runs on an executable
// with the game closed, and its report. It runs the recipes that need no running game and says, in numbers, what they
// found: the image's fingerprint and whether gal/builds.toml knows the build; the accessor's shape (its places, the
// sites accepted and refused, and every static they load, each with the places that load it); the classes the leads
// name, by their run-time type information, with their tables of methods; and the task groups' table.
//
// For as long as the shape is new, it also counts what the withdrawn pattern would have found: the derivation cuts
// that pattern from the executable in memory, as R0 did, and the finder compares the two sets of accepted sites. The
// pattern is never written: the report's vocabulary has no key that can hold a byte, a pattern, text, a name or an
// instruction in any form (report schema 2, kind `lodestar`). It holds whole numbers, booleans, fixed words and the
// leads' own labels.
//
// It reads the file only (O73, O75): nothing here runs the game, opens its memory or knows it.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "en/gal/builds.hpp"
#include "en/gal/derive.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/recipes.hpp"
#include "en/gal/report.hpp"
#include "en/proto/expected.hpp"

namespace en::gal::lodestar {

inline constexpr std::size_t kMaxStaticsReported = 1024;  // statics written; the count says how many there were
inline constexpr std::size_t kMaxSiteWitnesses = 8;       // places kept per static (layout.hpp's kMaxWitnesses)

struct ImageFound {
  std::array<std::uint16_t, 4> file_version{};
  bool has_version = false;
  std::uint32_t time_date_stamp = 0;
  std::uint32_t size_of_image = 0;
  std::uint64_t file_size = 0;
  std::uint64_t sections = 0;
  std::uint64_t functions = 0;
  bool known_build = false;                // gal/builds.toml's gate passes
  std::optional<GateProblem> gate_problem; // else why not
};

// One static the accessor's accepted sites load: a manager's slot, not yet named (its name is the in-game round's).
struct StaticFound {
  std::uint32_t rva = 0;
  std::uint32_t class_object = 0;
  std::uint32_t sites = 0;
  std::vector<std::uint32_t> witnesses;  // the first places that load it, each the load itself
};

enum class Expect : std::uint8_t { Within, Below, Above };

struct AccessorFound {
  bool recipe = false;  // recipes.toml holds an accessor
  std::uint64_t hits = 0;
  std::uint64_t sites = 0;     // the hits kept (all of them, up to the finder's cap)
  std::uint64_t accepted = 0;
  std::array<std::uint64_t, 9> refused{};  // by fd4::Refusal
  Expect expect = Expect::Within;          // the hits against the recipe's expect_matches
  std::uint64_t statics_found = 0;         // distinct statics of the accepted sites
  std::uint64_t class_objects = 0;
  std::vector<StaticFound> statics;        // by offset, the first kMaxStaticsReported
  std::vector<std::uint32_t> getters;      // distinct, by offset, the first 64
  // The withdrawn pattern, cut again in memory for the comparison and never written.
  bool pattern = false;
  std::uint64_t pattern_hits = 0;
  std::uint64_t pattern_accepted = 0;
  std::uint64_t both = 0;          // accepted sites the two share
  std::uint64_t only_shape = 0;    // accepted by the shape alone
  std::uint64_t only_pattern = 0;  // accepted by the pattern alone
};

struct ClassFound {
  std::string label;  // the lead's
  derive::Status status = derive::Status::Missing;
  std::uint32_t type_descriptor = 0;
  std::uint64_t classes = 0;
  std::vector<std::uint32_t> vtables;  // primary, ascending, the first 64
};

struct TableFound {
  derive::Status status = derive::Status::Missing;
  std::uint32_t rva = 0;
  std::uint32_t entries = 0;
  std::uint32_t stride = 0;
  std::optional<std::uint32_t> name_offset;
  bool wide = false;
  std::uint32_t index = 0;  // the lead's group's place
};

struct Found {
  ImageFound image;
  AccessorFound accessor;
  std::vector<ClassFound> classes;
  TableFound table;
};

// Runs the offline recipes on an image read from its file. `builds` may be null (the build is then unknown).
[[nodiscard]] Found find(const ImageInfo& image, const ByteSource& source, std::uint64_t file_size,
                         const derive::Leads& leads, const RecipeSet& recipes, const Builds* builds);

// Whether the run found what the next step needs: the accessor's recipe, with hits within its range and at least one
// accepted site, and, where the pattern could be cut, no accepted site that only one of the two has.
[[nodiscard]] bool complete(const Found& found);

[[nodiscard]] const report::Vocabulary& vocabulary() noexcept;  // kind `lodestar`, schema 2
[[nodiscard]] proto::Expected<std::string, report::Problem> write_report(const Found& found);

}  // namespace en::gal::lodestar
