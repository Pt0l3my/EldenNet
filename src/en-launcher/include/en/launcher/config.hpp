#pragma once
// The launcher's settings, `%APPDATA%\EldenNet\config\launcher.toml` (ARCH §4.1, §4.5, §4.8): the game's path once
// `--game` has given it, how to start without EAC, when to remove our appid file, and the first-screen notices seen.
// The launcher writes the file; a player may edit it. A value it does not know is left at its default and named in
// `problems` (fixed words), never quoted.
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/launcher/appid.hpp"

namespace en::launcher {

struct LauncherConfig {
  std::optional<std::filesystem::path> game;  // remembered from --game
  AppidMethod appid_method = AppidMethod::File;
  RemoveAfter remove_after = RemoveAfter::Window;
  std::uint32_t notices_seen = 0;  // the first-screen notices' version last ticked "Don't show again"

  friend bool operator==(const LauncherConfig&, const LauncherConfig&) = default;
};

struct ConfigRead {
  LauncherConfig config;
  std::vector<std::string> problems;
};

[[nodiscard]] ConfigRead read_config(std::string_view text);
[[nodiscard]] std::string config_text(const LauncherConfig& c);

}  // namespace en::launcher
