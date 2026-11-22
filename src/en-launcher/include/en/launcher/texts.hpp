#pragma once
// What the launcher says to the player (ARCH §4.7, §4.8): the errors E01 to E12 in their original wording, the
// first-screen notices, and the explanation of a crash recorded in the last session (O71). Each error has an exit
// code of its own, 10 + its number, for scripts and the tests.
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/watch.hpp"

namespace en::launcher {

// E12 (part 12): GraceUI's pre-flight (ARCH §4.3), told in E08's manner, and the game starts all the same.
enum class ErrorCode : std::uint8_t { E01 = 1, E02, E03, E04, E05, E06, E07, E08, E09, E10, E11, E12 };

[[nodiscard]] constexpr int exit_code(ErrorCode e) noexcept { return 10 + static_cast<int>(e); }
[[nodiscard]] std::string error_id(ErrorCode e);  // "E01"
// The error's text, its %s and %u filled from `s` and `code` where it has them.
[[nodiscard]] std::string error_text(ErrorCode e, std::string_view s = {}, std::uint32_t code = 0);
// The buttons an error offers besides closing: E01's "Open eldenring.exe…", E05's "Launch anyway (EldenNet stays
// inactive)", E07's "Continue without a backup", E10's "Start without EldenNet". Empty for the others.
[[nodiscard]] std::string error_action(ErrorCode e);

// The first-screen notices (ARCH §4.8), shown on the first run and whenever their version changes. The dashboard's
// address, read from GraceUI's settings, joins them with GraceUI's pre-flight.
inline constexpr std::uint32_t kNoticesVersion = 1;
struct Notice {
  std::string title;
  std::string body;
};
[[nodiscard]] std::vector<Notice> notices();

// The read-only round (--read; Lodestar's step L5), told to the owner on the dev console in plain words, one line per
// step as it changes, beside the log's `read step=<word>`. Nothing is called. What the tester is to do, and the key
// that confirms it, is the task list's (tasks.hpp).
enum class ReadStep : std::uint8_t {
  Asked,           // the launcher asked EldenNet for the round before it loaded it
  Refused,         // the DLL is no probe build (or does not know --read), or EldenNet is off
  Reading,         // EldenNet reads the game's image as loaded, and names what it can at the title
  LoadSave,        // a first naming pass is done and some manager is still unnamed: load the save
  Done,            // every manager asked for is named
  Stopped,         // the round stopped early (a banner, or no first reading)
  ReportsClean,    // after the game: both files passed their check
  ReportsRefused,  // after the game: a file was refused by its check (a defect of ours)
  ReportsMissing,  // after the game: a file is missing
};
[[nodiscard]] const char* read_word(ReadStep s) noexcept;  // "asked", "refused", ..., "reports_missing"
// `named` of `of`: the managers named so far (LoadSave and Done).
[[nodiscard]] std::string read_text(ReadStep s, std::uint32_t named = 0, std::uint32_t of = 0);
// The step the console shows for EldenNet's answer, T1's state and the status's items (named, asked, passes).
[[nodiscard]] std::optional<ReadStep> read_step(proto::watch::ProbeRequest answer, proto::watch::InitState init,
                                                std::uint32_t named, std::uint32_t of, std::uint32_t passes) noexcept;
// Whether the round's draft table (lodestar-table.toml) holds only what such a table may: `[provenance]`,
// `[item.<label>]` and `key = value` lines of numbers, our own labels and fixed words. No path, no text, no byte run.
[[nodiscard]] bool table_text_clean(std::string_view text) noexcept;

// The step names E10 reports (ARCH §4.6).
inline constexpr std::string_view kStepAlloc = "VirtualAllocEx";
inline constexpr std::string_view kStepWrite = "WriteProcessMemory";
inline constexpr std::string_view kStepThread = "CreateRemoteThread";
inline constexpr std::string_view kStepWait = "LoadLibraryW wait";
inline constexpr std::string_view kStepLoad = "LoadLibraryW";
inline constexpr std::string_view kStepModule = "module check";
inline constexpr std::string_view kStepResume = "ResumeThread";

}  // namespace en::launcher
