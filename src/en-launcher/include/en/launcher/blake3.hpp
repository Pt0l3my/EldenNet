#pragma once
// BLAKE3 for the save backups' manifests and their de-duplication (ARCH §4.4): the hash itself is en-proto's, our own
// (en/proto/blake3.hpp, written from the specification and checked against its published test vectors); here are the
// digest's hex form and a file's digest.
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include "en/proto/blake3.hpp"

namespace en::launcher {

using en::proto::Blake3;

[[nodiscard]] std::string to_hex(std::span<const std::uint8_t> bytes);
// The file's digest and size, read in 64 KiB pieces; nothing when it cannot be read.
struct FileDigest {
  std::uint64_t size = 0;
  std::array<std::uint8_t, 32> blake3{};
};
[[nodiscard]] std::optional<FileDigest> digest_file(const std::filesystem::path& file);

}  // namespace en::launcher
