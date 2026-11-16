// Executable files' facts (exe.hpp).
#include "en/launcher/exe.hpp"

#include <fstream>
#include <system_error>

#include "en/gal/builds.hpp"
#include "en/launcher/steam.hpp"

namespace en::launcher {

namespace {

// A file read in pieces at their offsets.
class FileSource final : public gal::ByteSource {
 public:
  FileSource(const std::filesystem::path& file, std::uint64_t size) : in_(file, std::ios::binary), size_(size) {}
  [[nodiscard]] bool good() const { return static_cast<bool>(in_); }
  [[nodiscard]] std::uint64_t size() const noexcept override { return size_; }
  [[nodiscard]] bool read(std::uint64_t offset, std::span<std::byte> dst) const noexcept override {
    if (offset > size_ || dst.size() > size_ - offset) return false;
    in_.clear();
    in_.seekg(static_cast<std::streamoff>(offset));
    in_.read(reinterpret_cast<char*>(dst.data()), static_cast<std::streamsize>(dst.size()));  // a byte buffer
    return static_cast<std::size_t>(in_.gcount()) == dst.size();
  }

 private:
  mutable std::ifstream in_;
  std::uint64_t size_;
};

std::string version_text(const std::array<std::uint16_t, 4>& v) {
  return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]) + "." + std::to_string(v[3]);
}

}  // namespace

std::optional<gal::ImageInfo> read_pe_file(const std::filesystem::path& file) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(file, ec);
  if (ec || size > (std::uint64_t{1} << 30)) return std::nullopt;
  const FileSource source(file, size);
  if (!source.good()) return std::nullopt;
  auto image = gal::read_image(source, gal::ImageLayout::File);
  if (!image) return std::nullopt;
  return std::move(*image);
}

std::optional<std::string> product_name(const std::filesystem::path& file) {
  const auto image = read_pe_file(file);
  if (!image || !image->version || !image->version->product_name) return std::nullopt;
  return *image->version->product_name;
}

bool is_ours(const std::filesystem::path& dll) {
  const auto name = product_name(dll);
  return name && *name == kOurProductName;
}

const char* to_string(GateOutcome::Kind k) noexcept {
  switch (k) {
    case GateOutcome::Kind::Passed:
      return "passed";
    case GateOutcome::Kind::Unknown:
      return "unknown";
    case GateOutcome::Kind::NoBuilds:
      return "no_builds";
    case GateOutcome::Kind::Unreadable:
      return "unreadable";
  }
  return "unreadable";
}

GateOutcome gate_game(const std::filesystem::path& exe, const std::filesystem::path& builds_toml) {
  GateOutcome out;
  const auto image = read_pe_file(exe);
  if (!image) {
    out.why = "the executable cannot be read";
    return out;
  }
  if (image->version) out.version = version_text(image->version->file_version);
  const auto text = read_text(builds_toml, 256u << 10);
  if (!text) {
    out.kind = GateOutcome::Kind::NoBuilds;
    out.why = "builds.toml is missing";
    return out;
  }
  const auto builds = gal::parse_builds(*text);
  if (!builds) {
    out.kind = GateOutcome::Kind::NoBuilds;
    out.why = "builds.toml is refused";
    return out;
  }
  const auto profile = gal::gate(*image, *builds);
  if (!profile) {
    out.kind = GateOutcome::Kind::Unknown;
    out.why = gal::describe(profile.error());
    return out;
  }
  out.kind = GateOutcome::Kind::Passed;
  out.key = profile->entry().key;
  return out;
}

}  // namespace en::launcher
