#pragma once
// Finding the game (ARCH §4.2): Steam's library folders, then the app's manifest in each, or a path the player gives.
// The game's facts (its Steam app id and where its executable lies under the install folder) are a GameSpec, so the
// logic is tested with invented ones.
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/launcher/vdf.hpp"

namespace en::launcher {

struct GameSpec {
  std::uint32_t appid = 0;
  std::vector<std::string> subpath;  // under the install folder, the last part the executable's file name
  [[nodiscard]] std::string exe_name() const { return subpath.empty() ? std::string{} : subpath.back(); }
};

// The game EldenNet starts: Steam app 1245620, `Game\eldenring.exe` under its install folder (ARCH §4.2, ER02).
[[nodiscard]] GameSpec elden_ring();

// UTF-8 text to and from paths, the same on every platform (Windows paths are UTF-16 inside).
[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view text);
[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);

// A small text file, whole; nothing when it cannot be read or is larger than `max_bytes`.
[[nodiscard]] std::optional<std::string> read_text(const std::filesystem::path& file, std::size_t max_bytes);

// libraryfolders.vdf's library paths, in its order: the newer form (`"0" { "path" "…" }`) and the older one
// (`"1" "…"`). Empty paths are left out.
[[nodiscard]] std::vector<std::filesystem::path> library_paths(const vdf::Node& root);
// An app manifest's install folder (`AppState` → `installdir`): one folder name, never a path or `..`.
[[nodiscard]] std::optional<std::string> install_dir(const vdf::Node& manifest);

struct SteamScan {
  std::vector<std::filesystem::path> libraries;  // as searched: the Steam folder first, then libraryfolders.vdf's
  std::size_t manifests = 0;                     // the app's manifests found
  std::optional<std::filesystem::path> exe;      // the first library's executable that exists
  std::vector<std::string> notes;                // fixed words: what was missing or refused
};

// Searches the libraries of the Steam folder `steam_root` for `spec`'s app.
[[nodiscard]] SteamScan find_in_steam(const std::filesystem::path& steam_root, const GameSpec& spec);

// `--game`: the executable itself, or a folder holding it, directly or in the subpath's folders (the install folder
// or the one above the executable). Nothing when neither exists.
[[nodiscard]] std::optional<std::filesystem::path> resolve_game_path(const std::filesystem::path& given,
                                                                     const GameSpec& spec);

}  // namespace en::launcher
