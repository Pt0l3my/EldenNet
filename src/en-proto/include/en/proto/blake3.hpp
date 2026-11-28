#pragma once
// BLAKE3, our own (WHY§174): its hash, keyed-hash and derive-key modes, for the save backups' manifests (ARCH §4.4,
// en-launcher) and the playthrough's fingerprint (MEM §8.3: a keyed hash under a key derived from a context string).
// Written from the BLAKE3 specification (the compression function, chunks of 1,024 bytes, the binary tree of chaining
// values, the mode flags) and checked against its published test vectors. A 32-byte digest of a stream fed in pieces
// of any size; no I/O, no allocation.
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace en::proto {

class Blake3 {
 public:
  using Digest = std::array<std::uint8_t, 32>;
  using Key = std::array<std::uint8_t, 32>;

  Blake3() noexcept;  // the hash mode
  // The keyed-hash mode, under a 32-byte key.
  [[nodiscard]] static Blake3 keyed(const Key& key) noexcept;
  // The derive-key mode: `context` is hashed first, and what is fed after is the key material. The digest is the
  // derived key.
  [[nodiscard]] static Blake3 derive_key(std::string_view context) noexcept;

  void update(std::span<const std::byte> input) noexcept;
  void update(std::string_view input) noexcept { update(std::as_bytes(std::span<const char>(input))); }
  [[nodiscard]] Digest finish() const noexcept;

 private:
  using Cv = std::array<std::uint32_t, 8>;
  struct Chunk {
    Cv cv{};
    std::uint64_t counter = 0;
    std::array<std::uint8_t, 64> block{};
    std::uint8_t block_len = 0;
    std::uint8_t blocks_done = 0;
  };
  Blake3(const Cv& key, std::uint32_t flags) noexcept;
  void add_chunk_cv(Cv cv, std::uint64_t total_chunks) noexcept;
  void start_chunk(std::uint64_t counter) noexcept;

  Cv key_{};
  std::uint32_t flags_ = 0;  // the mode's flag, on every compression
  Chunk chunk_;
  std::size_t chunk_len_ = 0;  // bytes in the chunk so far
  std::array<Cv, 54> stack_{};
  std::size_t stack_len_ = 0;
};

}  // namespace en::proto
