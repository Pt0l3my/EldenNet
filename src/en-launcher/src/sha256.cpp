// SHA-256 (sha256.hpp), from FIPS 180-4 §4.1.2, §4.2.2, §5.1.1, §5.3.3 and §6.2.
#include "en/launcher/sha256.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace en::launcher {

namespace {

constexpr std::array<std::uint32_t, 64> kK = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
    0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
    0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2};

constexpr std::uint32_t rotr(std::uint32_t x, int n) noexcept { return (x >> n) | (x << (32 - n)); }

}  // namespace

Sha256::Sha256() noexcept
    : h_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19} {}

void Sha256::block(const std::uint8_t* b) noexcept {
  std::array<std::uint32_t, 64> w{};
  for (int t = 0; t < 16; ++t) {
    w[t] = std::uint32_t{b[4 * t]} << 24 | std::uint32_t{b[4 * t + 1]} << 16 | std::uint32_t{b[4 * t + 2]} << 8 |
           std::uint32_t{b[4 * t + 3]};
  }
  for (int t = 16; t < 64; ++t) {
    const std::uint32_t s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
    const std::uint32_t s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
    w[t] = w[t - 16] + s0 + w[t - 7] + s1;
  }
  std::uint32_t a = h_[0], bb = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
  for (int t = 0; t < 64; ++t) {
    const std::uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + kK[t] + w[t];
    const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & bb) ^ (a & c) ^ (bb & c));
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = bb;
    bb = a;
    a = t1 + t2;
  }
  h_[0] += a;
  h_[1] += bb;
  h_[2] += c;
  h_[3] += d;
  h_[4] += e;
  h_[5] += f;
  h_[6] += g;
  h_[7] += h;
}

void Sha256::update(std::span<const std::byte> input) noexcept {
  total_ += input.size();
  std::size_t i = 0;
  while (i < input.size()) {
    const std::size_t take = std::min(input.size() - i, buf_.size() - buf_len_);
    std::memcpy(buf_.data() + buf_len_, input.data() + i, take);
    buf_len_ += take;
    i += take;
    if (buf_len_ == buf_.size()) {
      block(buf_.data());
      buf_len_ = 0;
    }
  }
}

std::array<std::uint8_t, 32> Sha256::finish() const noexcept {
  Sha256 s = *this;
  const std::uint64_t bits = total_ * 8;
  std::array<std::byte, 72> pad{};
  pad[0] = std::byte{0x80};
  // The message, a 1 bit, zeros, then its length in bits as 64 bits: to a multiple of 512 bits.
  const std::size_t zeros = (s.buf_len_ < 56 ? 56 - s.buf_len_ : 120 - s.buf_len_) - 1;
  std::array<std::byte, 8> length{};
  for (int k = 0; k < 8; ++k) length[k] = static_cast<std::byte>(bits >> (56 - 8 * k));
  const std::uint64_t kept = s.total_;
  s.update(std::span<const std::byte>(pad.data(), zeros + 1));
  s.update(length);
  s.total_ = kept;
  std::array<std::uint8_t, 32> out{};
  for (int k = 0; k < 8; ++k) {
    out[4 * k] = static_cast<std::uint8_t>(s.h_[k] >> 24);
    out[4 * k + 1] = static_cast<std::uint8_t>(s.h_[k] >> 16);
    out[4 * k + 2] = static_cast<std::uint8_t>(s.h_[k] >> 8);
    out[4 * k + 3] = static_cast<std::uint8_t>(s.h_[k]);
  }
  return out;
}

std::optional<FileSha256> sha256_file(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return std::nullopt;
  Sha256 h;
  FileSha256 out;
  std::vector<char> buf(64 * 1024);
  while (in) {
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    const std::streamsize n = in.gcount();
    if (n <= 0) break;
    h.update(std::as_bytes(std::span<const char>(buf.data(), static_cast<std::size_t>(n))));
    out.size += static_cast<std::uint64_t>(n);
  }
  if (in.bad()) return std::nullopt;
  out.sha256 = h.finish();
  return out;
}

}  // namespace en::launcher
