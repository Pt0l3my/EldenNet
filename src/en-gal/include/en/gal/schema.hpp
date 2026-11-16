#pragma once
// The problems found in one of gal/'s files (ARCH §6.3: builds.toml, patterns.toml, layouts/<build>.toml). Each file is
// read whole and every problem in it is reported, up to a cap, each with where it was and what was wrong, never a
// value: "patterns.toml: pattern[2].capture.at: lies outside the pattern". A file with any problem is refused, in CI
// and at boot alike (O4).
#include <cstddef>
#include <string>
#include <vector>

namespace en::gal {

struct SchemaError {
  std::string where;    // the file and the path in it, "builds.toml: build[1].region"; the file alone for a TOML error
  std::string problem;  // "must be one of ww, jp"

  friend bool operator==(const SchemaError&, const SchemaError&) = default;
};

using SchemaErrors = std::vector<SchemaError>;

inline constexpr std::size_t kMaxSchemaErrors = 64;          // past these, the last error says how many more there were
inline constexpr std::size_t kMaxGalFileBytes = 256u << 10;  // a gal/ file larger than this is refused unread

// "where: problem".
[[nodiscard]] std::string describe(const SchemaError& error);

}  // namespace en::gal
