#pragma once
// What the launcher says to the player (ARCH §4.7, §4.8): the errors E01 to E11 in their original wording, the
// first-screen notices, and the explanation of a crash recorded in the last session (O71). Each error has an exit
// code of its own, 10 + its number, for scripts and the tests.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace en::launcher {

enum class ErrorCode : std::uint8_t { E01 = 1, E02, E03, E04, E05, E06, E07, E08, E09, E10, E11 };

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

// The step names E10 reports (ARCH §4.6).
inline constexpr std::string_view kStepAlloc = "VirtualAllocEx";
inline constexpr std::string_view kStepWrite = "WriteProcessMemory";
inline constexpr std::string_view kStepThread = "CreateRemoteThread";
inline constexpr std::string_view kStepWait = "LoadLibraryW wait";
inline constexpr std::string_view kStepLoad = "LoadLibraryW";
inline constexpr std::string_view kStepModule = "module check";
inline constexpr std::string_view kStepResume = "ResumeThread";

}  // namespace en::launcher
