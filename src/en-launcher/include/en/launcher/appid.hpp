#pragma once
// Starting without EAC (ARCH §4.5): `steam_appid.txt` beside the game's executable (the file method, the default), or
// `SteamAppId` and `SteamGameId` in the game's environment (the env method, a setting pending S0-C). The launcher
// records a file it created in `launcher-state.toml`, removes it after the game's window has appeared (or at the
// game's exit, or never: `remove_after`), and never deletes one it did not create; every start and `--cleanup` remove
// a stale one of ours while the game is not running.
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace en::launcher {

enum class AppidMethod : std::uint8_t { File, Env };
enum class RemoveAfter : std::uint8_t { Window, Exit, Never };
[[nodiscard]] const char* to_string(AppidMethod m) noexcept;  // "file", "env"
[[nodiscard]] const char* to_string(RemoveAfter r) noexcept;  // "window", "exit", "never"
[[nodiscard]] std::optional<AppidMethod> appid_method_named(std::string_view s) noexcept;
[[nodiscard]] std::optional<RemoveAfter> remove_after_named(std::string_view s) noexcept;

// What launcher-state.toml records of the file.
struct AppidRecord {
  std::filesystem::path file;
  bool created_by_us = false;
  std::string created_at;  // YYYYMMDD-HHMMSS
  friend bool operator==(const AppidRecord&, const AppidRecord&) = default;
};

// launcher-state.toml's text for a record, and the record read back from one (nothing when it holds none, or cannot
// be read: unsafe for toml++, malformed, or a field missing or of another type).
[[nodiscard]] std::string state_text(const std::optional<AppidRecord>& record);
[[nodiscard]] std::optional<AppidRecord> read_state(std::string_view text);

// What the file holds, compared with the app id: whitespace around it does not count.
enum class AppidFile : std::uint8_t { Absent, Ours /* holds the app id */, Other /* holds anything else */ };
[[nodiscard]] AppidFile classify(const std::optional<std::string>& content, std::uint32_t appid) noexcept;

// The file method's step before the game starts.
enum class AppidStep : std::uint8_t {
  Created,     // written now: recorded as ours
  Used,        // there already with the app id: used as it is, not ours
  Foreign,     // there with something else: left as it is; this start uses the environment instead
  Unwritable,  // could not be written: E06
};
[[nodiscard]] const char* to_string(AppidStep s) noexcept;  // "created", "used", "foreign", "unwritable"

struct AppidPrepared {
  AppidStep step = AppidStep::Unwritable;
  std::optional<AppidRecord> record;  // set when Created
};

// Prepares `<game_dir>/steam_appid.txt` for a start.
[[nodiscard]] AppidPrepared prepare_appid_file(const std::filesystem::path& game_dir, std::uint32_t appid,
                                               const std::string& stamp);

// Removes the recorded file if we created it and it still holds the app id; answers true when it is gone (or was
// never ours to remove, which leaves it alone and answers false).
enum class AppidRemoval : std::uint8_t {
  Removed,
  Gone /* already gone */,
  NotOurs,
  Changed /* holds other text now */,
  Failed
};
[[nodiscard]] const char* to_string(AppidRemoval r) noexcept;
[[nodiscard]] AppidRemoval remove_if_ours(const AppidRecord& record, std::uint32_t appid);

inline constexpr std::string_view kAppidFileName = "steam_appid.txt";

}  // namespace en::launcher
