#pragma once
// GraceUI's pre-flight (ARCH §4.3's "GraceUI is installed and fits"; GraceUI D51, D52, D74, D83, D88; part 12): before
// the game starts, `Game\GraceUI\graceui.toml` must be readable, every file it lists must match its size and SHA-256,
// its ABI major must be the one EldenNet speaks and its version at least EldenNet's floor (en-proto's graceui_pin.hpp).
// What fails is told to the player in E12, E08's manner, and the game starts all the same: EldenNet runs with nothing
// shown in game (headless, L2 at most; ARCH §10.3) until GraceUI is installed again. Installing the GraceUI release
// EldenNet carries waits until EldenNet carries one (D53).
//
// graceui.toml's form is GraceUI's DESIGN §2.7 (D52), as GraceUI's own writer, tools/release/manifest.py, lays it out
// in every build's `GraceUI/` folder (D165): `graceui = "a.b.c"`, `abi = [major, minor]`, and a `[files]` table of
// `"relative/name" = { size = N, sha256 = "<64 hex digits>" }`. Its other keys, `protocol`, `caps` and `released`, are
// GraceUI's: accepted whatever they hold and not read here, since nothing EldenNet checks before the start rests on
// them (ARCH §4.3; the bridge protocol is the browser view's, not v0.1's).
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/expected.hpp"

namespace en::launcher {

struct GraceFile {
  std::string name;  // relative to GraceUI's folder, with '/' between its parts
  std::uint64_t size = 0;
  std::array<std::uint8_t, 32> sha256{};
};

struct GraceManifest {
  std::array<std::uint16_t, 3> version{};
  std::uint32_t abi_major = 0;
  std::uint32_t abi_minor = 0;
  std::vector<GraceFile> files;
};

inline constexpr std::size_t kGraceTomlMaxBytes = 1u << 20;
inline constexpr std::size_t kGraceFilesMax = 4096;

// graceui.toml's text: the manifest, or what is wrong with it (fixed words, never a value of the file).
[[nodiscard]] proto::Expected<GraceManifest, std::string> read_graceui_toml(std::string_view text);

enum class GraceCheck : std::uint8_t {
  Ok,
  NoFolder,     // no GraceUI folder beside the game's executable
  NoManifest,   // the folder holds no graceui.toml, or it cannot be read
  Unreadable,   // graceui.toml is not one GraceUI writes
  FileMissing,  // a listed file is missing
  SizeDiffers,  // a listed file's size differs
  HashDiffers,  // a listed file's SHA-256 differs
  AbiMajor,     // its ABI major is not the one EldenNet speaks
  TooOld,       // its version is older than EldenNet's floor
};
[[nodiscard]] const char* to_string(GraceCheck c) noexcept;  // "ok", "no_folder", ...

struct GraceReport {
  GraceCheck result = GraceCheck::NoFolder;
  std::array<std::uint16_t, 3> version{};  // as graceui.toml names it
  std::uint32_t abi_major = 0;
  std::uint32_t abi_minor = 0;
  std::size_t files = 0;    // listed
  std::size_t checked = 0;  // matched, before the first that did not
  std::string file;         // the first listed file that did not match (its relative name, never a path)
  std::string problem;      // with Unreadable: what read_graceui_toml said
};

// The check on `folder`, the game's `GraceUI` folder: the ABI and the version first, then every listed file, in the
// order of their names, each read in full (a few MB in all).
[[nodiscard]] GraceReport check_graceui(const std::filesystem::path& folder);

// What E12's text names, in plain words: "graceui.dll does not match graceui.toml", and the like.
[[nodiscard]] std::string graceui_problem(const GraceReport& r);

// A test package's GraceUI against the game's (--expect-graceui; the owner's request of 2026-11-23, after a run that
// loaded an older GraceUI from the game's folder than the package carried): graceui.toml read from a file, or what is
// wrong with it (fixed words).
[[nodiscard]] proto::Expected<GraceManifest, std::string> read_graceui_manifest(const std::filesystem::path& toml);
// Empty when `have` (the game's) is the GraceUI `want` (the package's) names: the same version, ABI and files, each of
// the same size and SHA-256. Otherwise what differs, in plain words, for E13: versions and a relative file name only.
[[nodiscard]] std::string graceui_differs(const GraceManifest& want, const GraceManifest& have);

}  // namespace en::launcher
