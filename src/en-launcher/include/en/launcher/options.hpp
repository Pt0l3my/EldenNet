#pragma once
// The launcher's command line (ARCH §4.1), with the development options of the first in-game build (the plan for
// roadmap S10.1.a, parts 7 and 10): when to inject, a second DLL, the dev console with the Mark (O90), the S0 loops,
// and the folders a test or a spike points elsewhere. Arguments arrive as UTF-8; an argument that is no option (what
// Proton forwards with `%command%`, ARCH §4.9) is set aside and counted, and everything after `--` goes to the game.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "en/launcher/appid.hpp"
#include "en/proto/expected.hpp"
#include "en/proto/watch.hpp"

namespace en::launcher {

enum class Action : std::uint8_t { Start, BackupOnly, Cleanup, Diagnostics, Watch, Help };
// When the DLL is loaded: into the suspended process (the default, ARCH §4.6), once the game's window shows, or a
// while after (`title`: `title_wait_s` after the window, about when the title screen shows; a late load, ARCH §3.3).
enum class InjectAt : std::uint8_t { Suspended, Window, Title };
[[nodiscard]] const char* to_string(InjectAt i) noexcept;  // "suspended", "window", "title"

struct Options {
  Action action = Action::Start;
  // The player's options (ARCH §4.1).
  std::optional<std::string> game;  // --game: the executable, or a folder holding it
  std::optional<std::string> dll;   // --dll
  bool no_backup = false;           // --no-backup
  bool vanilla = false;             // --vanilla-offline: start offline without EAC, inject nothing
  // Development and tests.
  std::vector<std::string> also;  // --also DLL: injected before ours, unchecked (a stand-in for another mod)
  // --expect-graceui PATH: a test package's own graceui.toml; the game's GraceUI must be that one, or E13 asks
  std::optional<std::string> expect_graceui;
  InjectAt inject_at = InjectAt::Suspended;
  std::uint32_t title_wait_s = 15;          // --title-wait S
  std::optional<AppidMethod> appid_method;  // --appid-method file|env (else launcher.toml's)
  std::optional<RemoveAfter> remove_after;  // --remove-after window|exit|never
  bool console = false;                     // --console: the dev console, its status lines and the Mark on Pause (O90)
  bool read = false;                        // --read: the read-only round in a probe build (Lodestar's L5); --console too
  std::uint32_t read_plan = 0;              // --read-plan N: the read round's plan (watch.hpp's kPlan*), with --read
  bool read_plan_given = false;
  std::uint32_t read_pace_s = 0;            // --read-pace-s S: the tests' pace for the round's tasks (tasks.hpp)
  std::uint32_t spike_pace_s = 0;           // --spike-pace-s S: the tests' pace for an S0 run's tasks (s0.hpp)
  bool no_dialogs = false;                  // --no-dialogs: lines instead of dialogs; every question answered no
  bool yes = false;                         // --yes: with --no-dialogs, questions answered yes instead
  bool no_steam_check = false;              // --no-steam-check: the fake game needs no Steam
  std::optional<std::string> gal;           // --gal DIR: the gal/ folder (else beside the launcher)
  std::optional<std::string> steam;         // --steam DIR: Steam's folder (else the registry's)
  std::optional<std::string> appdata;       // --appdata DIR: in place of %APPDATA%
  proto::watch::Spike spike = proto::watch::Spike::None;  // --spike s0a|s0b|s0c|s0d|s0d-crash|s0e|s0e_alone|s4
  std::uint32_t runs = 1;                                 // --runs N (s0a, s0d-crash)
  std::uint32_t run_timeout_s = 180;                      // --run-timeout S: a run's limit to reach a terminal state
  std::uint32_t watch_pid = 0;                            // --watch PID
  std::vector<std::string> game_args;                     // after --
  std::size_t ignored = 0;                                // arguments set aside
};

[[nodiscard]] proto::Expected<Options, std::string> parse_options(const std::vector<std::string>& args);
[[nodiscard]] std::string usage_text();

}  // namespace en::launcher
