#pragma once
// The settings EldenNet's first in-game build reads (ARCH §11.5, as far as v0.1 goes): settings.toml in
// %APPDATA%\EldenNet\config, read once on T1 in the Settings state (ARCH §3.2). Strict: a key no schema defines, a value of the wrong
// type or out of its range refuses the whole file, and the defaults are used, with the problems for the log (never a
// value of the file). `features` and `range` (ARCH §11.5's keys and bounds, en-core's behaviour.hpp) are read since the
// first conversation's polish and handed to the core (WHY§476); the tables ARCH §11.5 defines for later increments
// (autonomy, pacing, ui, pools, budget, safety, hotkeys, augur, reactions, allow) are accepted as tables and not read
// yet. Two tables are this build's own: `gal` (the copy primitive, S0-D) and, in a probe build only (EN_PROBE), `probe`
// (the read-only round and S0-D's fault injection).
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/behaviour.hpp"
#include "en/proto/snapshot.hpp"
#include "en/shell/logline.hpp"

namespace en::shell {

enum class CopyMode : std::uint8_t { Seh, Rpm };
// Which round a probe build runs. None: the normal start, with the probe's fault injection if asked. Read (Lodestar's
// step L5; proposal 0005 §6.2): the read-only round, which names the managers by their instances, tries the reader on
// them and calls nothing. (R1, the round with one call into the game, was retired 2026-11-22, O106: its word and its
// keys are refused like any unknown one.)
enum class ProbeRound : std::uint8_t { None, Read };

[[nodiscard]] const char* to_string(LogLevel l) noexcept;
[[nodiscard]] const char* to_string(CopyMode c) noexcept;
[[nodiscard]] const char* to_string(ProbeRound r) noexcept;

struct ProbeSettings {
  ProbeRound round = ProbeRound::None;
  std::uint32_t settle_s = 10;  // the read-only round: after the window, before its first reading
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
  core::Behaviour behaviour;      // features and range (ARCH §11.5), for Core::set_behaviour
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

// One setting changed from EldenNet's pages (its space's one command path, en-overlay's space.hpp), to be written to
// settings.toml: a key of a table, `debug.log_level`, and its new value, a boolean, a word or a number.
struct SettingsEdit {
  std::string key;
  bool is_text = false;
  bool flag = false;
  std::string text;
  bool is_number = false;  // `number` is the value (range.talk_m)
  double number = 0;

  friend bool operator==(const SettingsEdit&, const SettingsEdit&) = default;
};

// Whether `key` is one of the behaviour keys the core takes (ARCH §11.5's `features.*` and `range.*`).
[[nodiscard]] bool is_behaviour_key(std::string_view key) noexcept;
// `b` with one behaviour key changed, or false (and `b` as it was) for a key that is none of them or a value of the
// wrong type: a switch takes a boolean, a range a number. Bounds are the core's (core::check_behaviour), not checked here.
[[nodiscard]] bool edit_behaviour(core::Behaviour& b, const SettingsEdit& e);

// settings.toml's text with `edits` made (D14: the pages' changes, written after 300 ms of quiet), or why not, worded
// without a value: an empty text makes a file of its own (`schema = 2` and the edits); a text the parse refuses is
// never written over (the player's file stays as they wrote it); every other key and table, the later increments'
// among them, is kept with its value, and the result must be one parse_settings accepts. Comments and the file's
// layout are not kept: toml++ writes the tables again (the slice's notes ask whether that is enough).
struct EditedSettings {
  bool ok = false;
  std::string text;
  std::string why;  // when not ok: "refused", "not_a_table", "unknown_key", "result_refused"
};
[[nodiscard]] EditedSettings edit_settings(std::string_view text, const std::vector<SettingsEdit>& edits,
                                           bool probe_build);

}  // namespace en::shell
