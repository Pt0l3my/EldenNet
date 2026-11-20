#pragma once
// The settings EldenNet's first in-game build reads (ARCH §11.5, as far as v0.1 goes): settings.toml in
// %APPDATA%\EldenNet\config, read once on T1 in the Settings state (ARCH §3.2). Strict: a key no schema defines, a value of the wrong
// type or out of its range refuses the whole file, and the defaults are used, with the problems for the log (never a
// value of the file). The tables ARCH §11.5 defines for later increments (features, autonomy, range, pacing, ui, pools,
// budget, safety, hotkeys, augur, reactions, allow) are accepted as tables and not read yet. Two tables are this
// build's own: `gal` (the copy primitive, S0-D) and, in a probe build only (EN_PROBE), `probe` (the R1 round, O89).
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/snapshot.hpp"

namespace en::shell {

enum class LogLevel : std::uint8_t { Error, Warn, Info, Debug };
enum class CopyMode : std::uint8_t { Seh, Rpm };
// R1 (O89): which round a probe build runs. None: the normal start, with the probe's fault injection if asked.
enum class ProbeRound : std::uint8_t { None, R1 };
// When R1 makes its one call into the game, once its checks have passed: at the first Mark (the launcher's Pause,
// pressed at the title with no save loaded: O89, O90), at once, or never (the facts that need no call only).
enum class ProbeRegister : std::uint8_t { Mark, Auto, Never };

[[nodiscard]] const char* to_string(LogLevel l) noexcept;
[[nodiscard]] const char* to_string(CopyMode c) noexcept;
[[nodiscard]] const char* to_string(ProbeRound r) noexcept;
[[nodiscard]] const char* to_string(ProbeRegister r) noexcept;

struct ProbeSettings {
  ProbeRound round = ProbeRound::None;
  ProbeRegister register_when = ProbeRegister::Mark;
  std::uint32_t object_size = 0x800;  // R1's task object: generous, since the game's own size is what R1 measures
  std::uint32_t settle_s = 10;        // R1: after the window, before the live derivation's first pass
  // S0-D's fault injection (part 4's probe option): every `fault_every`-th read of `fault_group`, from
  // `fault_after_s` seconds after the first world snapshot. 0: off.
  proto::Group fault_group = proto::Group::Camera;
  std::uint32_t fault_every = 0;
  std::uint32_t fault_after_s = 2;
};

struct Settings {
  bool enabled = true;            // false: Off (T1 exits at once)
  bool record_sessions = false;   // debug.record_sessions: the recorder (ARCH §7.5)
  LogLevel log_level = LogLevel::Info;
  bool overlay_connect = true;    // overlay.connect: GraceUI's connect (part 12); false: headless, Connect skipped
  bool debug_panel = false;       // debug.panel: the F11 debug view (PAGES SP8: a developer view, off by default)
  CopyMode copy = CopyMode::Seh;  // gal.copy
  ProbeSettings probe;
};

struct SettingsProblem {
  std::string where;    // "debug.log_level"
  std::string problem;  // "must be one of error, warn, info, debug"
};

struct LoadedSettings {
  Settings settings;
  std::vector<SettingsProblem> problems;  // why the file was refused (the defaults are then used)
  std::vector<std::string> notes;         // what was accepted but not taken (none in this build)
  bool refused = false;
};

inline constexpr std::size_t kMaxSettingsBytes = 64u << 10;

// The file's text; `probe_build` says whether the `probe` table is allowed (EN_PROBE).
[[nodiscard]] LoadedSettings parse_settings(std::string_view text, bool probe_build);

}  // namespace en::shell
