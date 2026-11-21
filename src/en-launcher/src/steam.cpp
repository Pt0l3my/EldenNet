// Finding the game through Steam's library folders, or from a path (steam.hpp; ARCH §4.2).
#include "en/launcher/steam.hpp"

#include <fstream>
#include <sstream>
#include <system_error>

namespace en::launcher {

GameSpec elden_ring() { return GameSpec{1245620, {"Game", "eldenring.exe"}}; }

std::filesystem::path path_from_utf8(std::string_view text) {
  std::u8string u;
  u.reserve(text.size());
  for (const char c : text) u.push_back(static_cast<char8_t>(c));
  return std::filesystem::path(u);
}

std::string path_to_utf8(const std::filesystem::path& path) {
  const std::u8string u = path.u8string();
  std::string out;
  out.reserve(u.size());
  for (const char8_t c : u) out.push_back(static_cast<char>(c));
  return out;
}

std::optional<std::string> read_text(const std::filesystem::path& file, std::size_t max_bytes) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(file, ec);
  if (ec || size > max_bytes) return std::nullopt;
  std::ifstream in(file, std::ios::binary);
  if (!in) return std::nullopt;
  std::ostringstream s;
  s << in.rdbuf();
  if (in.bad()) return std::nullopt;
  return s.str();
}

std::vector<std::filesystem::path> library_paths(const vdf::Node& root) {
  std::vector<std::filesystem::path> out;
  const vdf::Node* folders = vdf::find(root, "libraryfolders");
  if (folders == nullptr || !folders->object) return out;
  for (const vdf::Node& entry : folders->children) {
    const bool numeric = !entry.key.empty() && entry.key.find_first_not_of("0123456789") == std::string::npos;
    if (!numeric) continue;  // the older form's TimeNextStatsReport and the like
    std::string path;
    if (entry.object) {
      if (const vdf::Node* p = vdf::find(entry, "path"); p != nullptr && !p->object) path = p->value;
    } else {
      path = entry.value;
    }
    if (!path.empty()) out.push_back(path_from_utf8(path));
  }
  return out;
}

std::optional<std::string> install_dir(const vdf::Node& manifest) {
  const vdf::Node* state = vdf::find(manifest, "AppState");
  if (state == nullptr || !state->object) return std::nullopt;
  const vdf::Node* dir = vdf::find(*state, "installdir");
  if (dir == nullptr || dir->object || dir->value.empty()) return std::nullopt;
  const std::string& v = dir->value;
  if (v == "." || v == ".." || v.find_first_of("/\\:") != std::string::npos) return std::nullopt;
  return v;
}

SteamScan find_in_steam(const std::filesystem::path& steam_root, const GameSpec& spec) {
  SteamScan scan;
  std::error_code ec;
  scan.libraries.push_back(steam_root);
  const auto text = read_text(steam_root / "steamapps" / "libraryfolders.vdf", vdf::kMaxText);
  if (!text) {
    scan.notes.emplace_back("no library list");
  } else if (const auto root = vdf::parse(*text); !root) {
    scan.notes.emplace_back("the library list cannot be read");
  } else {
    for (const auto& lib : library_paths(*root)) {
      bool seen = false;
      for (const auto& have : scan.libraries) seen = seen || std::filesystem::equivalent(have, lib, ec);
      if (!seen) scan.libraries.push_back(lib);
    }
  }
  const std::string manifest_name = "appmanifest_" + std::to_string(spec.appid) + ".acf";
  for (const auto& lib : scan.libraries) {
    const auto manifest = read_text(lib / "steamapps" / manifest_name, 1u << 20);
    if (!manifest) continue;
    ++scan.manifests;
    const auto parsed = vdf::parse(*manifest);
    const auto dir = parsed ? install_dir(*parsed) : std::nullopt;
    if (!dir) {
      scan.notes.emplace_back("an app manifest without a usable install folder");
      continue;
    }
    std::filesystem::path exe = lib / "steamapps" / "common" / path_from_utf8(*dir);
    for (const auto& part : spec.subpath) exe /= path_from_utf8(part);
    if (std::filesystem::is_regular_file(exe, ec)) {
      if (!scan.exe) scan.exe = exe;
    } else {
      scan.notes.emplace_back("an app manifest whose executable is missing");
    }
  }
  return scan;
}

std::optional<std::filesystem::path> resolve_game_path(const std::filesystem::path& given, const GameSpec& spec) {
  std::error_code ec;
  if (std::filesystem::is_regular_file(given, ec)) return given;
  if (!std::filesystem::is_directory(given, ec)) return std::nullopt;
  // The folder holding the executable, or one of the folders above it along the subpath.
  for (std::size_t skip = spec.subpath.size(); skip > 0; --skip) {
    std::filesystem::path p = given;
    for (std::size_t i = skip - 1; i < spec.subpath.size(); ++i) p /= path_from_utf8(spec.subpath[i]);
    if (std::filesystem::is_regular_file(p, ec)) return p;
  }
  return std::nullopt;
}

}  // namespace en::launcher
