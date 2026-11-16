#pragma once
// What the launcher reads from executable files, with en-gal's PE reader (ARCH §2.3's dotted edge): the game's
// fingerprint against gal/builds.toml (ARCH §4.2 step 6), and whether a DLL is ours, by its version resource's
// ProductName (ARCH §4.3). Files are read in pieces, never whole, and only their headers and version resource.
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "en/gal/pe.hpp"

namespace en::launcher {

inline constexpr std::string_view kOurProductName = "EldenNet";

// The file's headers, sections and version resource; nothing when it is not a PE32+ image for x64, cannot be read or
// is larger than 1 GiB.
[[nodiscard]] std::optional<gal::ImageInfo> read_pe_file(const std::filesystem::path& file);
[[nodiscard]] std::optional<std::string> product_name(const std::filesystem::path& file);
// The file exists and its ProductName reads EldenNet.
[[nodiscard]] bool is_ours(const std::filesystem::path& dll);

struct GateOutcome {
  enum class Kind : std::uint8_t {
    Passed,      // a supported or retired build of builds.toml
    Unknown,     // no build of builds.toml matches (or the image has no version): E05
    NoBuilds,    // builds.toml is missing or refused: as unknown, with its reason
    Unreadable,  // the executable cannot be read as a PE32+ image for x64
  };
  Kind kind = Kind::Unreadable;
  std::string key;      // builds.toml's key, when it passed
  std::string version;  // the file version, "a.b.c.d", when the image has one
  std::string why;      // fixed words for a refusal ("build fingerprint differs (TimeDateStamp)")
};
[[nodiscard]] const char* to_string(GateOutcome::Kind k) noexcept;  // "passed", "unknown", "no_builds", "unreadable"
[[nodiscard]] GateOutcome gate_game(const std::filesystem::path& exe, const std::filesystem::path& builds_toml);

}  // namespace en::launcher
