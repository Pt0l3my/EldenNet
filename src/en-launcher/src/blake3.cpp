// A digest's hex form and a file's digest (blake3.hpp); the hash is en-proto's.
#include "en/launcher/blake3.hpp"

#include <fstream>
#include <vector>

namespace en::launcher {

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
