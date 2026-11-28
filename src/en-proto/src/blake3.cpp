// BLAKE3 (blake3.hpp), from the specification: the compression function of 7 rounds over a 16-word state, 64-byte
// blocks, 1,024-byte chunks, parents merged as soon as a subtree is complete, and the modes' flags (keyed hash 16,
// derive-key context 32, derive-key material 64) on every compression of their stream.
#include "en/proto/blake3.hpp"

#include <algorithm>

namespace en::proto {

namespace {

constexpr std::array<std::uint32_t, 8> kIv = {0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
                                              0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u};
constexpr std::array<std::uint8_t, 16> kPermutation = {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8};
constexpr std::uint32_t kChunkStart = 1;
constexpr std::uint32_t kChunkEnd = 2;
constexpr std::uint32_t kParent = 4;
constexpr std::uint32_t kRoot = 8;
constexpr std::uint32_t kKeyedHash = 16;
constexpr std::uint32_t kDeriveKeyContext = 32;
constexpr std::uint32_t kDeriveKeyMaterial = 64;
constexpr std::size_t kBlockLen = 64;
constexpr std::size_t kChunkLen = 1024;

using Words16 = std::array<std::uint32_t, 16>;
using Cv = std::array<std::uint32_t, 8>;

constexpr std::uint32_t rotr(std::uint32_t x, int n) noexcept { return (x >> n) | (x << (32 - n)); }

void g(Words16& s, std::size_t a, std::size_t b, std::size_t c, std::size_t d, std::uint32_t mx,
       std::uint32_t my) noexcept {
  s[a] = s[a] + s[b] + mx;
  s[d] = rotr(s[d] ^ s[a], 16);
  s[c] = s[c] + s[d];
  s[b] = rotr(s[b] ^ s[c], 12);
  s[a] = s[a] + s[b] + my;
  s[d] = rotr(s[d] ^ s[a], 8);
  s[c] = s[c] + s[d];
  s[b] = rotr(s[b] ^ s[c], 7);
}

Words16 compress(const Cv& cv, const Words16& block, std::uint64_t counter, std::uint32_t block_len,
                 std::uint32_t flags) noexcept {
  Words16 s = {cv[0],   cv[1],   cv[2],   cv[3],
               cv[4],   cv[5],   cv[6],   cv[7],
               kIv[0],  kIv[1],  kIv[2],  kIv[3],
               static_cast<std::uint32_t>(counter), static_cast<std::uint32_t>(counter >> 32), block_len, flags};
  Words16 m = block;
  for (int round = 0; round < 7; ++round) {
    g(s, 0, 4, 8, 12, m[0], m[1]);
    g(s, 1, 5, 9, 13, m[2], m[3]);
    g(s, 2, 6, 10, 14, m[4], m[5]);
    g(s, 3, 7, 11, 15, m[6], m[7]);
    g(s, 0, 5, 10, 15, m[8], m[9]);
    g(s, 1, 6, 11, 12, m[10], m[11]);
    g(s, 2, 7, 8, 13, m[12], m[13]);
    g(s, 3, 4, 9, 14, m[14], m[15]);
    if (round < 6) {
      Words16 p{};
      for (std::size_t i = 0; i < 16; ++i) p[i] = m[kPermutation[i]];
      m = p;
    }
  }
  for (std::size_t i = 0; i < 8; ++i) {
    s[i] ^= s[i + 8];
    s[i + 8] ^= cv[i];
  }
  return s;
}

Words16 words_of(const std::array<std::uint8_t, 64>& bytes) noexcept {
  Words16 w{};
  for (std::size_t i = 0; i < 16; ++i) {
    w[i] = std::uint32_t{bytes[4 * i]} | (std::uint32_t{bytes[4 * i + 1]} << 8) |
           (std::uint32_t{bytes[4 * i + 2]} << 16) | (std::uint32_t{bytes[4 * i + 3]} << 24);
  }
  return w;
}

Cv key_words(const Blake3::Key& key) noexcept {
  Cv w{};
  for (std::size_t i = 0; i < 8; ++i) {
    w[i] = std::uint32_t{key[4 * i]} | (std::uint32_t{key[4 * i + 1]} << 8) | (std::uint32_t{key[4 * i + 2]} << 16) |
           (std::uint32_t{key[4 * i + 3]} << 24);
  }
  return w;
}

Cv first8(const Words16& w) noexcept { return Cv{w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]}; }

// A node's output before its chaining value or the root's bytes are taken.
struct Output {
  Cv cv;
  Words16 block;
  std::uint64_t counter;
  std::uint32_t block_len;
  std::uint32_t flags;
};

Output parent_output(const Cv& key, std::uint32_t mode, const Cv& left, const Cv& right) noexcept {
  Words16 block{};
  for (std::size_t i = 0; i < 8; ++i) {
    block[i] = left[i];
    block[i + 8] = right[i];
  }
  return Output{key, block, 0, static_cast<std::uint32_t>(kBlockLen), kParent | mode};
}

Cv chaining_value(const Output& o) noexcept { return first8(compress(o.cv, o.block, o.counter, o.block_len, o.flags)); }

}  // namespace

Blake3::Blake3() noexcept : Blake3(kIv, 0) {}

Blake3::Blake3(const Cv& key, std::uint32_t flags) noexcept : key_(key), flags_(flags) { start_chunk(0); }

Blake3 Blake3::keyed(const Key& key) noexcept { return Blake3(key_words(key), kKeyedHash); }

Blake3 Blake3::derive_key(std::string_view context) noexcept {
  Blake3 ctx(kIv, kDeriveKeyContext);
  ctx.update(context);
  return Blake3(key_words(ctx.finish()), kDeriveKeyMaterial);
}

void Blake3::start_chunk(std::uint64_t counter) noexcept {
  chunk_ = Chunk{};
  chunk_.cv = key_;
  chunk_.counter = counter;
  chunk_len_ = 0;
}

void Blake3::add_chunk_cv(Cv cv, std::uint64_t total_chunks) noexcept {
  // Each trailing zero bit of the count completes a subtree: merge it with the stack's top.
  while ((total_chunks & 1) == 0 && stack_len_ > 0) {
    cv = chaining_value(parent_output(key_, flags_, stack_[--stack_len_], cv));
    total_chunks >>= 1;
  }
  stack_[stack_len_++] = cv;
}

void Blake3::update(std::span<const std::byte> input) noexcept {
  std::size_t at = 0;
  while (at < input.size()) {
    if (chunk_len_ == kChunkLen) {
      // The chunk is full and more input follows: it is not the root.
      const Output out{chunk_.cv, words_of(chunk_.block), chunk_.counter, chunk_.block_len,
                       flags_ | kChunkEnd | (chunk_.blocks_done == 0 ? kChunkStart : 0u)};
      const std::uint64_t total = chunk_.counter + 1;
      add_chunk_cv(chaining_value(out), total);
      start_chunk(total);
    }
    if (chunk_.block_len == kBlockLen) {
      const std::uint32_t flags = flags_ | (chunk_.blocks_done == 0 ? kChunkStart : 0u);
      chunk_.cv = first8(compress(chunk_.cv, words_of(chunk_.block), chunk_.counter, kBlockLen, flags));
      ++chunk_.blocks_done;
      chunk_.block = {};
      chunk_.block_len = 0;
    }
    const std::size_t room = std::min(kBlockLen - chunk_.block_len, kChunkLen - chunk_len_);
    const std::size_t take = std::min(room, input.size() - at);
    for (std::size_t i = 0; i < take; ++i) {
      chunk_.block[chunk_.block_len + i] = static_cast<std::uint8_t>(input[at + i]);
    }
    chunk_.block_len = static_cast<std::uint8_t>(chunk_.block_len + take);
    chunk_len_ += take;
    at += take;
  }
}

Blake3::Digest Blake3::finish() const noexcept {
  Output out{chunk_.cv, words_of(chunk_.block), chunk_.counter, chunk_.block_len,
             flags_ | kChunkEnd | (chunk_.blocks_done == 0 ? kChunkStart : 0u)};
  for (std::size_t i = stack_len_; i > 0; --i) {
    out = parent_output(key_, flags_, stack_[i - 1], chaining_value(out));
  }
  const Words16 root = compress(out.cv, out.block, 0, out.block_len, out.flags | kRoot);
  Digest digest{};
  for (std::size_t i = 0; i < 8; ++i) {
    for (std::size_t b = 0; b < 4; ++b) digest[4 * i + b] = static_cast<std::uint8_t>(root[i] >> (8 * b));
  }
  return digest;
}

}  // namespace en::proto
