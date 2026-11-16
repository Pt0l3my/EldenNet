// BLAKE3's hash mode (blake3.hpp), from the specification: the compression function of 7 rounds over a 16-word
// state, 64-byte blocks, 1,024-byte chunks, and parents merged as soon as a subtree is complete.
#include "en/launcher/blake3.hpp"

#include <algorithm>
#include <fstream>
#include <vector>

namespace en::launcher {

namespace {

constexpr std::array<std::uint32_t, 8> kIv = {0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
                                              0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u};
constexpr std::array<std::uint8_t, 16> kPermutation = {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8};
constexpr std::uint32_t kChunkStart = 1;
constexpr std::uint32_t kChunkEnd = 2;
constexpr std::uint32_t kParent = 4;
constexpr std::uint32_t kRoot = 8;
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
  Words16 s = {cv[0],
               cv[1],
               cv[2],
               cv[3],
               cv[4],
               cv[5],
               cv[6],
               cv[7],
               kIv[0],
               kIv[1],
               kIv[2],
               kIv[3],
               static_cast<std::uint32_t>(counter),
               static_cast<std::uint32_t>(counter >> 32),
               block_len,
               flags};
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

Cv first8(const Words16& w) noexcept { return Cv{w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]}; }

// A node's output before its chaining value or the root's bytes are taken.
struct Output {
  Cv cv;
  Words16 block;
  std::uint64_t counter;
  std::uint32_t block_len;
  std::uint32_t flags;
};

Output parent_output(const Cv& left, const Cv& right) noexcept {
  Words16 block{};
  for (std::size_t i = 0; i < 8; ++i) {
    block[i] = left[i];
    block[i + 8] = right[i];
  }
  return Output{kIv, block, 0, static_cast<std::uint32_t>(kBlockLen), kParent};
}

Cv chaining_value(const Output& o) noexcept { return first8(compress(o.cv, o.block, o.counter, o.block_len, o.flags)); }

}  // namespace

Blake3::Blake3() noexcept { start_chunk(0); }

void Blake3::start_chunk(std::uint64_t counter) noexcept {
  chunk_ = Chunk{};
  chunk_.cv = kIv;
  chunk_.counter = counter;
  chunk_len_ = 0;
}

void Blake3::add_chunk_cv(Cv cv, std::uint64_t total_chunks) noexcept {
  // Each trailing zero bit of the count completes a subtree: merge it with the stack's top.
  while ((total_chunks & 1) == 0 && stack_len_ > 0) {
    cv = chaining_value(parent_output(stack_[--stack_len_], cv));
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
                       kChunkEnd | (chunk_.blocks_done == 0 ? kChunkStart : 0u)};
      const std::uint64_t total = chunk_.counter + 1;
      add_chunk_cv(chaining_value(out), total);
      start_chunk(total);
    }
    if (chunk_.block_len == kBlockLen) {
      const std::uint32_t flags = chunk_.blocks_done == 0 ? kChunkStart : 0u;
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

std::array<std::uint8_t, 32> Blake3::finish() const noexcept {
  Output out{chunk_.cv, words_of(chunk_.block), chunk_.counter, chunk_.block_len,
             kChunkEnd | (chunk_.blocks_done == 0 ? kChunkStart : 0u)};
  for (std::size_t i = stack_len_; i > 0; --i) out = parent_output(stack_[i - 1], chaining_value(out));
  const Words16 root = compress(out.cv, out.block, 0, out.block_len, out.flags | kRoot);
  std::array<std::uint8_t, 32> digest{};
  for (std::size_t i = 0; i < 8; ++i) {
    for (std::size_t b = 0; b < 4; ++b) digest[4 * i + b] = static_cast<std::uint8_t>(root[i] >> (8 * b));
  }
  return digest;
}

std::string to_hex(std::span<const std::uint8_t> bytes) {
  static constexpr char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (const std::uint8_t b : bytes) {
    out.push_back(kDigits[b >> 4]);
    out.push_back(kDigits[b & 0xF]);
  }
  return out;
}

std::optional<FileDigest> digest_file(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return std::nullopt;
  Blake3 h;
  FileDigest d;
  std::vector<char> buf(64 * 1024);
  while (in) {
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    const std::streamsize n = in.gcount();
    if (n <= 0) break;
    h.update(std::as_bytes(std::span<const char>(buf.data(), static_cast<std::size_t>(n))));
    d.size += static_cast<std::uint64_t>(n);
  }
  if (in.bad()) return std::nullopt;
  d.blake3 = h.finish();
  return d;
}

}  // namespace en::launcher
