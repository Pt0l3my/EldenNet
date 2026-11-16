#pragma once
// Save backups, a rotation of 10 (ARCH §4.4, DESIGN §3.6, O6). From each save folder under the game's save root whose
// name is 17 digits (a SteamID64), the save files present are copied to `<backups>/<id>/<stamp>/` (each to `.tmp`, its
// size and BLAKE3 hash checked against the source's, then renamed), with a `manifest.toml` of their sizes and hashes.
// No copy is made when every file equals the newest backup's; the newest `keep` backups are kept, and older ones are
// deleted only after a new one has been made. Nothing under the save root is ever written, moved or renamed.
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace en::launcher {

struct BackupSpec {
  std::filesystem::path saves_root;    // %APPDATA%\EldenRing
  std::filesystem::path backups_root;  // %APPDATA%\EldenNet\save-backups
  std::vector<std::string> files;      // the save's file names, in each SteamID's folder
  std::size_t keep = 10;
  std::string stamp;  // YYYYMMDD-HHMMSS, local time, from the caller (no clock here)
};

// The game's save files (ER01 §11): the save and the game's own backup of it.
[[nodiscard]] std::vector<std::string> elden_ring_save_files();

struct BackupReport {
  std::size_t folders = 0;            // save folders found (17-digit names)
  std::size_t made = 0;               // new backups
  std::size_t unchanged = 0;          // folders whose files equal their newest backup
  std::size_t empty = 0;              // folders with none of the files
  std::size_t removed = 0;            // older backups deleted by the rotation
  std::vector<std::string> problems;  // fixed words, no path or id: what failed
  [[nodiscard]] bool ok() const noexcept { return problems.empty(); }
};

[[nodiscard]] BackupReport back_up_saves(const BackupSpec& spec);

// A backup folder's name: YYYYMMDD-HHMMSS, or that with "-N" (N 2 to 99) when two fall in one second.
[[nodiscard]] bool is_backup_name(std::string_view name) noexcept;
// A save folder's name: 17 decimal digits.
[[nodiscard]] bool is_steam_id_folder(std::string_view name) noexcept;

}  // namespace en::launcher
