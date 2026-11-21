#pragma once
// SHA-256, our own, for GraceUI's pre-flight (ARCH §4.3; GraceUI D52, D83): `graceui.toml` names every file of GraceUI's
// folder by its size and SHA-256. Written from FIPS 180-4 (its message schedule, 64 rounds and padding) and checked
// against NIST's published examples.
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>

namespace en::launcher {

class Sha256 {
 public:
  Sha256() noexcept;
  void update(std::span<const std::byte> input) noexcept;
  [[nodiscard]] std::array<std::uint8_t, 32> finish() const noexcept;

 private:
  void block(const std::uint8_t* b) noexcept;

  std::array<std::uint32_t, 8> h_{};
  std::array<std::uint8_t, 64> buf_{};
  std::size_t buf_len_ = 0;
  std::uint64_t total_ = 0;  // bytes
};

// The file's SHA-256 and size, read in 64 KiB pieces; nothing when it cannot be read.
struct FileSha256 {
  std::uint64_t size = 0;
  std::array<std::uint8_t, 32> sha256{};
};
[[nodiscard]] std::optional<FileSha256> sha256_file(const std::filesystem::path& file);

}  // namespace en::launcher
