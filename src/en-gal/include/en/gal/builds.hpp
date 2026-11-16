#pragma once
// gal/builds.toml and the version gate (ARCH §3.2's Gate, §6.3; DESIGN §4.3; R7). The gate compares the image's
// fingerprint (its file version, product name, region, TimeDateStamp and SizeOfImage) with the builds EldenNet knows,
// and only an exact match passes: a same-version rebuild stops at L1 with "build fingerprint differs". What passes is
// a BuildProfile, which only gate() makes; a Layout needs one (layout.hpp), so nothing layout-dependent can run before
// the gate (R7, by type).
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/gal/pe.hpp"
#include "en/gal/schema.hpp"
#include "en/proto/expected.hpp"
#include "en/proto/snapshot.hpp"

namespace en::gal {

enum class Region : std::uint8_t { Ww, Jp };
enum class BuildStatus : std::uint8_t { Supported, Retired };  // a retired build is kept for players who block updates

[[nodiscard]] const char* to_string(Region region) noexcept;  // "ww", "jp"

struct BuildEntry {
  std::string key;  // "1.2.3.4-ww": the file version and the region
  std::string app;  // the version players see
  std::array<std::uint16_t, 4> file_version{};
  std::string product_name;
  Region region = Region::Ww;
  std::uint32_t time_date_stamp = 0;
  std::uint32_t size_of_image = 0;
  std::string layout;             // "layouts/<name>.toml", relative to gal/
  bool dlrf_name_layout = false;  // the class-name layout is confirmed (S0-A)
  BuildStatus status = BuildStatus::Supported;

  friend bool operator==(const BuildEntry&, const BuildEntry&) = default;
};

struct Builds {
  std::vector<BuildEntry> entries;  // in the file's order: an entry's index is its BuildKey
};

// Reads builds.toml (schema 1): every field present and of its form, the key the file version and region joined, the
// region agreeing with the file version's fourth field (0 ww, 1 jp: a lead, ER01 §1, confirmed by S0-A), a layout
// path inside gal/'s layouts/, and keys and fingerprints unique. A placeholder SizeOfImage of 0 is refused.
[[nodiscard]] proto::Expected<Builds, SchemaErrors> parse_builds(std::string_view text,
                                                                 std::string_view file = "builds.toml");

// What the gate compares.
struct Fingerprint {
  std::array<std::uint16_t, 4> file_version{};
  std::string product_name;
  std::optional<Region> region;  // from the fourth version field; nothing for a value the rule does not name
  std::uint32_t time_date_stamp = 0;
  std::uint32_t size_of_image = 0;
};

// The image's fingerprint, or nothing when it has no version resource or no ProductName.
[[nodiscard]] std::optional<Fingerprint> fingerprint(const ImageInfo& image);

enum class GateProblem : std::uint8_t {
  NoVersion,           // the image has no version resource or ProductName
  UnknownBuild,        // no entry has its file version and product name
  FingerprintDiffers,  // an entry has them, but its region, TimeDateStamp or SizeOfImage differs
};

inline constexpr std::uint8_t kDiffersRegion = 1;
inline constexpr std::uint8_t kDiffersTimeDateStamp = 2;
inline constexpr std::uint8_t kDiffersSizeOfImage = 4;

struct GateError {
  GateProblem problem = GateProblem::NoVersion;
  std::uint8_t differs = 0;  // for FingerprintDiffers, the closest entry's differing fields (kDiffers*)

  friend constexpr bool operator==(const GateError&, const GateError&) = default;
};

// "build fingerprint differs (TimeDateStamp)".
[[nodiscard]] std::string describe(const GateError& error);

class BuildProfile;
[[nodiscard]] proto::Expected<BuildProfile, GateError> gate(const ImageInfo& image, const Builds& builds);

// The build the gate matched (R7). It can be copied, but only gate() makes one.
class BuildProfile {
 public:
  [[nodiscard]] const BuildEntry& entry() const noexcept { return entry_; }
  [[nodiscard]] proto::BuildKey key() const noexcept { return key_; }

 private:
  BuildProfile(BuildEntry entry, proto::BuildKey key) : entry_(std::move(entry)), key_(key) {}
  friend proto::Expected<BuildProfile, GateError> gate(const ImageInfo& image, const Builds& builds);

  BuildEntry entry_;
  proto::BuildKey key_;
};

}  // namespace en::gal
