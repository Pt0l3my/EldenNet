#pragma once
// BLAKE3's hash mode, our own, for the save backups' manifests and their de-duplication (ARCH §4.4): a 32-byte digest
// of a byte stream, fed in pieces of any size. Written from the BLAKE3 specification (its compression function, chunks
// of 1,024 bytes, the binary tree of chaining values) and checked against its published test vectors.
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace en::launcher {

class Blake3 {
 public:
  Blake3() noexcept;
  void update(std::span<const std::byte> input) noexcept;
  [[nodiscard]] std::array<std::uint8_t, 32> finish() const noexcept;

 private:
  struct Chunk {
    std::array<std::uint32_t, 8> cv{};
    std::uint64_t counter = 0;
    std::array<std::uint8_t, 64> block{};
    std::uint8_t block_len = 0;
    std::uint8_t blocks_done = 0;
  };
  void add_chunk_cv(std::array<std::uint32_t, 8> cv, std::uint64_t total_chunks) noexcept;
  void start_chunk(std::uint64_t counter) noexcept;

  Chunk chunk_;
  std::size_t chunk_len_ = 0;  // bytes in the chunk so far
  std::array<std::array<std::uint32_t, 8>, 54> stack_{};
  std::size_t stack_len_ = 0;
};

[[nodiscard]] std::string to_hex(std::span<const std::uint8_t> bytes);
// The file's digest and size, read in 64 KiB pieces; nothing when it cannot be read.
struct FileDigest {
  std::uint64_t size = 0;
  std::array<std::uint8_t, 32> blake3{};
};
[[nodiscard]] std::optional<FileDigest> digest_file(const std::filesystem::path& file);

}  // namespace en::launcher
