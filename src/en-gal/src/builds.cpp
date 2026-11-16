// gal/builds.toml and the version gate (builds.hpp; ARCH §3.2, §6.3; R7).
#include "en/gal/builds.hpp"

#include <algorithm>

#include "toml_reader.hpp"

namespace en::gal {

using proto::Expected;
using proto::Unexpected;

const char* to_string(Region region) noexcept {
  switch (region) {
    case Region::Ww: return "ww";
    case Region::Jp: return "jp";
  }
  return "ww";
}

namespace {

using toml_read::Errors;
using toml_read::Table;

// The region the fourth file-version field names (0 ww, 1 jp; a lead, ER01 §1), or nothing.
std::optional<Region> region_of(const std::array<std::uint16_t, 4>& version) {
  if (version[3] == 0) return Region::Ww;
  if (version[3] == 1) return Region::Jp;
  return std::nullopt;
}

// "1.2.3.4" as four fields of at most 65,535.
std::optional<std::array<std::uint16_t, 4>> parse_version(std::string_view text) {
  std::array<std::uint16_t, 4> out{};
  std::size_t field = 0;
  std::uint32_t value = 0;
  std::size_t digits = 0;
  for (const char c : text) {
    if (c == '.') {
      if (digits == 0 || field >= 3) return std::nullopt;
      out[field++] = static_cast<std::uint16_t>(value);
      value = 0;
      digits = 0;
    } else if (c >= '0' && c <= '9') {
      if (++digits > 5) return std::nullopt;
      value = value * 10 + static_cast<std::uint32_t>(c - '0');
      if (value > 0xFFFF) return std::nullopt;
    } else {
      return std::nullopt;
    }
  }
  if (digits == 0 || field != 3) return std::nullopt;
  out[3] = static_cast<std::uint16_t>(value);
  return out;
}

std::string version_text(const std::array<std::uint16_t, 4>& v) {
  return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]) + "." + std::to_string(v[3]);
}

// The version players see: one to four dot-separated numbers.
bool is_app_version(std::string_view s) {
  if (s.empty() || s.size() > 16) return false;
  std::size_t dots = 0;
  bool digit = false;
  for (const char c : s) {
    if (c == '.') {
      if (!digit) return false;
      ++dots;
      digit = false;
    } else if (c >= '0' && c <= '9') {
      digit = true;
    } else {
      return false;
    }
  }
  return digit && dots <= 3;
}

bool is_product_name(std::string_view s) {
  return !s.empty() && s.size() <= 64 &&
         std::none_of(s.begin(), s.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; });
}

void read_build(const toml::table& t, const std::string& path, Errors& errors, BuildEntry& out) {
  Table b(t, path, errors);
  const auto key = b.text("key", true);
  if (const auto app = b.text("app", true)) {
    if (!is_app_version(*app)) errors.add(b.where("app"), "must be one to four dot-separated numbers");
    out.app = *app;
  }
  const auto version = b.text("file_version", true);
  bool have_version = false;
  if (version) {
    if (const auto v = parse_version(*version)) {
      out.file_version = *v;
      have_version = true;
    } else {
      errors.add(b.where("file_version"), "must be four dot-separated numbers of at most 65535");
    }
  }
  if (const auto name = b.text("product_name", true)) {
    if (!is_product_name(*name)) errors.add(b.where("product_name"), "must be 1 to 64 bytes with no control character");
    out.product_name = *name;
  }
  const auto region = b.text("region", true);
  bool have_region = false;
  if (region) {
    if (*region == "ww") {
      out.region = Region::Ww;
      have_region = true;
    } else if (*region == "jp") {
      out.region = Region::Jp;
      have_region = true;
    } else {
      errors.add(b.where("region"), "must be one of ww, jp");
    }
  }
  if (have_version && have_region && region_of(out.file_version) != out.region) {
    errors.add(b.where("region"), "does not agree with file_version's fourth field (0 for ww, 1 for jp)");
  }
  if (key) {
    out.key = *key;
    if (!toml_read::is_build_key(*key)) {
      errors.add(b.where("key"), "must be a build key such as 1.2.3.4-ww");
    } else if (have_version && have_region && *key != version_text(out.file_version) + "-" + to_string(out.region)) {
      errors.add(b.where("key"), "must be file_version and region joined by -");
    }
  }
  if (const auto stamp = b.integer("time_date_stamp", true, 0, 0xFFFFFFFFLL)) {
    out.time_date_stamp = static_cast<std::uint32_t>(*stamp);
  }
  if (const auto size = b.integer("size_of_image", true, 1, 0xFFFFFFFFLL)) {
    out.size_of_image = static_cast<std::uint32_t>(*size);
  }
  if (const auto layout = b.text("layout", true)) {
    const bool ok = layout->size() > 13 && layout->starts_with("layouts/") && layout->ends_with(".toml") &&
                    layout->find('/', 8) == std::string::npos && toml_read::is_relative_path(*layout);
    if (!ok) errors.add(b.where("layout"), "must be layouts/<name>.toml");
    out.layout = *layout;
  }
  out.dlrf_name_layout = b.boolean("dlrf_name_layout", false).value_or(false);
  if (const auto status = b.text("status", true)) {
    if (*status == "supported") {
      out.status = BuildStatus::Supported;
    } else if (*status == "retired") {
      out.status = BuildStatus::Retired;
    } else {
      errors.add(b.where("status"), "must be one of supported, retired");
    }
  }
  b.done();
}

}  // namespace

Expected<Builds, SchemaErrors> parse_builds(std::string_view text, std::string_view file) {
  Errors errors{std::string(file)};
  const std::optional<toml::table> doc = toml_read::parse(text, errors);
  if (!doc) return Unexpected{errors.take()};
  Table root(*doc, "", errors);
  (void)root.integer("schema", true, 1, 1);
  Builds builds;
  if (const toml::array* list = root.array("build", true)) {
    if (list->empty() || list->size() > 64) errors.add("build", "must list 1 to 64 builds");
    for (std::size_t i = 0; i < list->size() && i < 64; ++i) {
      const std::string path = toml_read::indexed("build", i);
      const toml::table* t = list->get(i)->as_table();
      if (t == nullptr) {
        errors.add(path, "must be a table");
        continue;
      }
      BuildEntry entry;
      read_build(*t, path, errors, entry);
      for (const BuildEntry& other : builds.entries) {
        if (!entry.key.empty() && other.key == entry.key) errors.add(path + ".key", "is not unique");
        if (other.file_version == entry.file_version && other.product_name == entry.product_name &&
            other.region == entry.region && other.time_date_stamp == entry.time_date_stamp &&
            other.size_of_image == entry.size_of_image) {
          errors.add(path, "has the fingerprint of an earlier build");
        }
      }
      builds.entries.push_back(std::move(entry));
    }
  }
  root.done();
  if (!errors.empty()) return Unexpected{errors.take()};
  return builds;
}

std::optional<Fingerprint> fingerprint(const ImageInfo& image) {
  if (!image.version || !image.version->product_name) return std::nullopt;
  Fingerprint fp;
  fp.file_version = image.version->file_version;
  fp.product_name = *image.version->product_name;
  fp.region = region_of(fp.file_version);
  fp.time_date_stamp = image.time_date_stamp;
  fp.size_of_image = image.size_of_image;
  return fp;
}

std::string describe(const GateError& error) {
  switch (error.problem) {
    case GateProblem::NoVersion: return "the game has no version information";
    case GateProblem::UnknownBuild: return "unknown build";
    case GateProblem::FingerprintDiffers: {
      std::string out = "build fingerprint differs (";
      const auto add = [&](std::uint8_t bit, const char* name) {
        if ((error.differs & bit) == 0) return;
        if (out.back() != '(') out += ", ";
        out += name;
      };
      add(kDiffersRegion, "region");
      add(kDiffersTimeDateStamp, "TimeDateStamp");
      add(kDiffersSizeOfImage, "SizeOfImage");
      return out + ")";
    }
  }
  return "unknown build";
}

Expected<BuildProfile, GateError> gate(const ImageInfo& image, const Builds& builds) {
  const std::optional<Fingerprint> fp = fingerprint(image);
  if (!fp) return Unexpected{GateError{GateProblem::NoVersion, 0}};
  std::optional<std::uint8_t> closest;
  for (std::size_t i = 0; i < builds.entries.size(); ++i) {
    const BuildEntry& e = builds.entries[i];
    if (e.file_version != fp->file_version || e.product_name != fp->product_name) continue;
    std::uint8_t differs = 0;
    if (fp->region != e.region) differs |= kDiffersRegion;
    if (fp->time_date_stamp != e.time_date_stamp) differs |= kDiffersTimeDateStamp;
    if (fp->size_of_image != e.size_of_image) differs |= kDiffersSizeOfImage;
    if (differs == 0 && i <= 0xFFFF) return BuildProfile(e, proto::BuildKey{static_cast<std::uint16_t>(i)});
    const auto weight = [](std::uint8_t d) { return (d & 1) + ((d >> 1) & 1) + ((d >> 2) & 1); };
    if (!closest || weight(differs) < weight(*closest)) closest = differs;
  }
  if (!closest) return Unexpected{GateError{GateProblem::UnknownBuild, 0}};
  return Unexpected{GateError{GateProblem::FingerprintDiffers, *closest}};
}

}  // namespace en::gal
