#pragma once
// What the launcher says to the player (ARCH §4.7, §4.8): the errors E01 to E11 in their original wording, the
// first-screen notices, and the explanation of a crash recorded in the last session (O71). Each error has an exit
// code of its own, 10 + its number, for scripts and the tests.
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/watch.hpp"

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

// R1, the derivation's in-game round (--probe, O89), told to the owner on the dev console in plain words, one line per
// step as it changes, beside the log's `r1 step=<word>`.
enum class R1Step : std::uint8_t {
  Asked,           // the launcher asked EldenNet for R1 before it loaded it
  Refused,         // the DLL is no probe build (or does not know --probe), or EldenNet is off: nothing is called
  Reading,         // R1 reads the game's image as loaded and checks it against the reviewed leads
  PressMark,       // the checks passed: press Pause at the title, with no save loaded
  Called,          // the one call was made; waiting for the game to run our task
  Confirmed,       // the game runs our task: "task confirmed"
  NoConfirmation,  // 30 s after the call, and the game has not run our task
  NotCalled,       // a check did not pass, so nothing was called
  CallFailed,      // the call itself did not complete
  ReportsClean,    // after the game: both reports passed the lint
  ReportsRefused,  // after the game: a report was refused by the lint (a defect of ours)
  ReportsMissing,  // after the game: a report is missing
};
[[nodiscard]] const char* r1_word(R1Step s) noexcept;  // "asked", "refused", ..., "reports_missing"
[[nodiscard]] std::string r1_text(R1Step s);
// The step the console shows for EldenNet's answer and T1's state: `pressed` once PressMark has been shown,
// `in_first_tick_ms` how long T1 has been in FirstTick. Nothing while the answer has not come and T1 is still early.
[[nodiscard]] std::optional<R1Step> r1_step(proto::watch::ProbeRequest answer, proto::watch::InitState init,
                                            bool pressed, std::uint64_t in_first_tick_ms) noexcept;

// The step names E10 reports (ARCH §4.6).
inline constexpr std::string_view kStepAlloc = "VirtualAllocEx";
inline constexpr std::string_view kStepWrite = "WriteProcessMemory";
inline constexpr std::string_view kStepThread = "CreateRemoteThread";
inline constexpr std::string_view kStepWait = "LoadLibraryW wait";
inline constexpr std::string_view kStepLoad = "LoadLibraryW";
inline constexpr std::string_view kStepModule = "module check";
inline constexpr std::string_view kStepResume = "ResumeThread";

}  // namespace en::launcher
