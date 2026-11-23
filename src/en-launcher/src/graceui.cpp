// GraceUI's pre-flight (graceui.hpp).
#include "en/launcher/graceui.hpp"

#include <toml.hpp>

#include <algorithm>
#include <fstream>
#include <optional>

#include "en/launcher/sha256.hpp"
#include "en/proto/graceui_pin.hpp"
#include "en/proto/toml_check.hpp"

namespace en::launcher {

namespace {

namespace pin = proto::graceui_pin;
using Refusal = proto::Unexpected<std::string>;

// "a.b.c", each a number within GRACEUI_VERSION_PACK's bits (10, 10 and 12).
bool parse_version(std::string_view s, std::array<std::uint16_t, 3>& out) {
  const std::array<std::uint32_t, 3> max = {1023, 1023, 4095};
  std::size_t part = 0;
  std::uint32_t value = 0;
  bool digits = false;
  for (const char c : s) {
    if (c >= '0' && c <= '9') {
      value = value * 10 + static_cast<std::uint32_t>(c - '0');
      if (value > max[part]) return false;
      digits = true;
    } else if (c == '.' && digits && part < 2) {
      out[part++] = static_cast<std::uint16_t>(value);
      value = 0;
      digits = false;
    } else {
      return false;
    }
  }
  if (!digits || part != 2) return false;
  out[2] = static_cast<std::uint16_t>(value);
  return true;
}

bool parse_hex(std::string_view s, std::array<std::uint8_t, 32>& out) {
  if (s.size() != 64) return false;
  for (std::size_t i = 0; i < 64; ++i) {
    const char c = s[i];
    int v = -1;
    if (c >= '0' && c <= '9') v = c - '0';
    if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
    if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
    if (v < 0) return false;
    out[i / 2] = static_cast<std::uint8_t>(i % 2 == 0 ? v << 4 : out[i / 2] | v);
  }
  return true;
}

// A name inside GraceUI's folder: relative, '/' between its parts, no empty part, no "." or "..", no ':' or '\', and
// printable ASCII (GraceUI's own file names).
bool inside_folder(std::string_view name) {
  if (name.empty() || name.size() > 240 || name.front() == '/' || name.back() == '/') return false;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= name.size(); ++i) {
    if (i < name.size()) {
      const char c = name[i];
      if (c < 0x20 || c > 0x7E || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
          c == '|') {
        return false;
      }
      if (c != '/') continue;
    }
    const std::string_view part = name.substr(start, i - start);
    if (part.empty() || part == "." || part == "..") return false;
    start = i + 1;
  }
  return true;
}

std::optional<std::string> read_file(const std::filesystem::path& p, std::size_t max_bytes) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return std::nullopt;
  std::string text;
  text.resize(max_bytes + 1);
  in.read(text.data(), static_cast<std::streamsize>(text.size()));
  const auto n = static_cast<std::size_t>(in.gcount());
  if (in.bad() || n > max_bytes) return std::nullopt;
  text.resize(n);
  return text;
}

}  // namespace

proto::Expected<GraceManifest, std::string> read_graceui_toml(std::string_view text) {
  if (text.size() > kGraceTomlMaxBytes) return Refusal{"graceui.toml is too large"};
  if (proto::unsafe_for_toml(text, proto::TomlText::Player)) return Refusal{"graceui.toml cannot be read safely"};
  toml::parse_result parsed = toml::parse(text);
  if (!parsed) return Refusal{"graceui.toml is not valid TOML"};
  const toml::table& t = parsed.table();
  GraceManifest m;
  const auto version = t["graceui"].value<std::string_view>();
  if (!version || !parse_version(*version, m.version)) return Refusal{"graceui.toml names no version"};
  const toml::array* abi = t["abi"].as_array();
  if (abi == nullptr || abi->size() != 2) return Refusal{"graceui.toml names no ABI"};
  const auto major = (*abi)[0].value<std::int64_t>();
  const auto minor = (*abi)[1].value<std::int64_t>();
  if (!major || !minor || *major < 0 || *minor < 0 || *major > 1023 || *minor > 1023) {
    return Refusal{"graceui.toml names no ABI"};
  }
  m.abi_major = static_cast<std::uint32_t>(*major);
  m.abi_minor = static_cast<std::uint32_t>(*minor);
  const toml::table* files = t["files"].as_table();
  if (files == nullptr || files->empty()) return Refusal{"graceui.toml lists no files"};
  if (files->size() > kGraceFilesMax) return Refusal{"graceui.toml lists too many files"};
  bool dll = false;
  for (const auto& [key, value] : *files) {
    GraceFile f;
    f.name = std::string(key.str());
    if (!inside_folder(f.name)) return Refusal{"graceui.toml lists a file outside its folder"};
    const toml::table* entry = value.as_table();
    if (entry == nullptr) return Refusal{"graceui.toml lists a file without its size and hash"};
    const auto size = (*entry)["size"].value<std::int64_t>();
    const auto sha = (*entry)["sha256"].value<std::string_view>();
    if (!size || *size < 0 || !sha || !parse_hex(*sha, f.sha256)) {
      return Refusal{"graceui.toml lists a file without its size and hash"};
    }
    f.size = static_cast<std::uint64_t>(*size);
    dll = dll || f.name == "graceui.dll";
    m.files.push_back(std::move(f));
  }
  if (!dll) return Refusal{"graceui.toml does not list graceui.dll"};
  return m;
}

const char* to_string(GraceCheck c) noexcept {
  switch (c) {
    case GraceCheck::Ok: return "ok";
    case GraceCheck::NoFolder: return "no_folder";
    case GraceCheck::NoManifest: return "no_manifest";
    case GraceCheck::Unreadable: return "unreadable";
    case GraceCheck::FileMissing: return "file_missing";
    case GraceCheck::SizeDiffers: return "size_differs";
    case GraceCheck::HashDiffers: return "hash_differs";
    case GraceCheck::AbiMajor: return "abi_major";
    case GraceCheck::TooOld: return "too_old";
  }
  return "no_folder";
}

GraceReport check_graceui(const std::filesystem::path& folder) {
  GraceReport r;
  std::error_code ec;
  if (!std::filesystem::is_directory(folder, ec)) {
    r.result = GraceCheck::NoFolder;
    return r;
  }
  const std::optional<std::string> text = read_file(folder / "graceui.toml", kGraceTomlMaxBytes);
  if (!text) {
    r.result = GraceCheck::NoManifest;
    return r;
  }
  auto m = read_graceui_toml(*text);
  if (!m) {
    r.result = GraceCheck::Unreadable;
    r.problem = m.error();
    return r;
  }
  r.version = m->version;
  r.abi_major = m->abi_major;
  r.abi_minor = m->abi_minor;
  r.files = m->files.size();
  if (m->abi_major != pin::kAbiMajor) {
    r.result = GraceCheck::AbiMajor;
    return r;
  }
  if (!pin::at_least_floor(m->version)) {
    r.result = GraceCheck::TooOld;
    return r;
  }
  for (const GraceFile& f : m->files) {
    std::filesystem::path p = folder;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= f.name.size(); ++i) {
      if (i == f.name.size() || f.name[i] == '/') {
        p /= f.name.substr(start, i - start);
        start = i + 1;
      }
    }
    r.file = f.name;
    if (!std::filesystem::is_regular_file(p, ec)) {
      r.result = GraceCheck::FileMissing;
      return r;
    }
    if (std::filesystem::file_size(p, ec) != f.size || ec) {
      r.result = GraceCheck::SizeDiffers;
      return r;
    }
    const std::optional<FileSha256> d = sha256_file(p);
    if (!d || d->size != f.size) {
      r.result = d ? GraceCheck::SizeDiffers : GraceCheck::FileMissing;
      return r;
    }
    if (d->sha256 != f.sha256) {
      r.result = GraceCheck::HashDiffers;
      return r;
    }
    ++r.checked;
  }
  r.file.clear();
  r.result = GraceCheck::Ok;
  return r;
}

proto::Expected<GraceManifest, std::string> read_graceui_manifest(const std::filesystem::path& toml) {
  const std::optional<std::string> text = read_file(toml, kGraceTomlMaxBytes);
  if (!text) return Refusal{"graceui.toml could not be read"};
  return read_graceui_toml(*text);
}

std::string graceui_differs(const GraceManifest& want, const GraceManifest& have) {
  const auto version = [](const std::array<std::uint16_t, 3>& v) {
    return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]);
  };
  if (have.version != want.version) {
    return "the game's GraceUI is version " + version(have.version) + ", and this package carries " +
           version(want.version);
  }
  if (have.abi_major != want.abi_major || have.abi_minor != want.abi_minor) {
    return "the game's GraceUI speaks ABI " + std::to_string(have.abi_major) + "." + std::to_string(have.abi_minor) +
           ", and this package's " + std::to_string(want.abi_major) + "." + std::to_string(want.abi_minor);
  }
  for (const GraceFile& w : want.files) {
    const auto h =
        std::find_if(have.files.begin(), have.files.end(), [&](const GraceFile& f) { return f.name == w.name; });
    if (h == have.files.end()) return "both are version " + version(want.version) + ", but the game's lacks " + w.name;
    if (h->size != w.size || h->sha256 != w.sha256) {
      return "both are version " + version(want.version) + ", but their " + w.name + " differ";
    }
  }
  for (const GraceFile& h : have.files) {
    const auto w =
        std::find_if(want.files.begin(), want.files.end(), [&](const GraceFile& f) { return f.name == h.name; });
    if (w == want.files.end()) {
      return "both are version " + version(want.version) + ", but the game's has " + h.name + " besides";
    }
  }
  return {};
}

std::string graceui_problem(const GraceReport& r) {
  const auto version = [](const std::array<std::uint16_t, 3>& v) {
    return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]);
  };
  switch (r.result) {
    case GraceCheck::Ok: return "nothing is wrong";
    case GraceCheck::NoFolder: return "there is no GraceUI folder beside the game";
    case GraceCheck::NoManifest: return "GraceUI's folder has no readable graceui.toml";
    case GraceCheck::Unreadable: return r.problem.empty() ? "graceui.toml could not be read" : r.problem;
    case GraceCheck::FileMissing: return r.file + " is missing from GraceUI's folder";
    case GraceCheck::SizeDiffers:
    case GraceCheck::HashDiffers: return r.file + " does not match graceui.toml";
    case GraceCheck::AbiMajor:
      return "this GraceUI speaks ABI " + std::to_string(r.abi_major) + ", and EldenNet needs ABI " +
             std::to_string(pin::kAbiMajor);
    case GraceCheck::TooOld:
      return "this GraceUI is version " + version(r.version) + ", and EldenNet needs " + version(pin::kFloor) +
             " or newer";
  }
  return "GraceUI could not be checked";
}

}  // namespace en::launcher
