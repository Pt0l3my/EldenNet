#pragma once
// MEM §8.2's SteamID discovery, over a listing the shell makes (win/store.cpp lists %APPDATA%\EldenRing\ with
// FindFirstFileExW; the tests give their own): the account whose folder holds the playthrough files. Only metadata is
// read, never a save's contents, and the SteamID itself is never logged or recorded (it names the player's account).
//
// 1. Only folders whose name is 17 decimal digits count (ER01 §11).
// 2. One such folder: it.
// 3. Several: the folder whose save file was modified last. A save file is any regular file in it of at least
//    10 × 0x280000 bytes, so ERSC's `.co2` and other loaders' names count as well as `ER0000.sl2`. A save file
//    modified since the game started (`since`, the watcher's sight of a save) wins over any older one.
// 4. None, or several with no save file in any: none, and memory is off for the session (the core's toast).
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace en::shell {

inline constexpr std::uint64_t kSaveFileMinBytes = 10ull * 0x280000;

struct SaveFileMeta {
  std::uint64_t bytes = 0;
  std::int64_t modified = 0;  // seconds since the epoch, as the listing gives it
  bool regular = true;        // not a folder, a link or a device
};
struct AccountFolder {
  std::string name;  // the folder's own name, as listed
  std::vector<SaveFileMeta> files;
};

enum class AccountWhy : std::uint8_t {
  One,      // one account folder
  Newest,   // several: the newest save file's
  Changed,  // several: a save file modified since the game started
  None,     // no account folder, or several with no save file
};
[[nodiscard]] const char* to_string(AccountWhy w) noexcept;  // one, newest, changed, none

struct AccountPick {
  std::uint64_t steamid64 = 0;  // 0 with None
  AccountWhy why = AccountWhy::None;
  std::size_t folders = 0;      // the account folders listed
};

// A 17-digit folder name as its number; none for anything else.
[[nodiscard]] std::optional<std::uint64_t> steamid_folder(std::string_view name) noexcept;
// Whether a file of the listing counts as a save file (step 3).
[[nodiscard]] bool save_file(const SaveFileMeta& f) noexcept;
// Steps 1 to 4. Ties go to the lower SteamID, so the pick never depends on the listing's order.
[[nodiscard]] AccountPick pick_account(const std::vector<AccountFolder>& listing,
                                       std::optional<std::int64_t> since = std::nullopt);

}  // namespace en::shell
