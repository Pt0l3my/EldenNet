// The launcher's command line (options.hpp; ARCH §4.1).
#include "en/launcher/options.hpp"

#include <string_view>

#include "en/launcher/s0.hpp"

namespace en::launcher {

const char* to_string(InjectAt i) noexcept {
  switch (i) {
    case InjectAt::Suspended:
      return "suspended";
    case InjectAt::Window:
      return "window";
    case InjectAt::Title:
      return "title";
  }
  return "suspended";
}

namespace {

std::optional<std::uint32_t> number(std::string_view s, std::uint32_t lo, std::uint32_t hi) {
  if (s.empty() || s.size() > 9) return std::nullopt;
  std::uint32_t n = 0;
  for (const char c : s) {
    if (c < '0' || c > '9') return std::nullopt;
    n = n * 10 + static_cast<std::uint32_t>(c - '0');
  }
  if (n < lo || n > hi) return std::nullopt;
  return n;
}

std::optional<proto::watch::Spike> spike_named(std::string_view s) {
  using proto::watch::Spike;
  for (std::uint32_t i = 1; i < proto::watch::kSpikeCount; ++i) {
    if (s == proto::watch::to_string(static_cast<Spike>(i))) return static_cast<Spike>(i);
  }
  return std::nullopt;
}

}  // namespace

proto::Expected<Options, std::string> parse_options(const std::vector<std::string>& args) {
  Options o;
  int actions = 0;
  bool runs_given = false;
  const auto set_action = [&](Action a) {
    o.action = a;
    ++actions;
  };
  for (std::size_t i = 0; i < args.size(); ++i) {
    std::string arg = args[i];
    if (arg == "--") {
      o.game_args.assign(args.begin() + static_cast<std::ptrdiff_t>(i) + 1, args.end());
      break;
    }
    if (arg.rfind("--", 0) != 0) {
      ++o.ignored;  // Proton's forwarded command, or anything else that is no option
      continue;
    }
    std::string value;
    bool has_value = false;
    if (const auto eq = arg.find('='); eq != std::string::npos) {
      value = arg.substr(eq + 1);
      arg.resize(eq);
      has_value = true;
    }
    const auto take = [&](std::string& out) -> bool {
      if (has_value) {
        out = value;
        return true;
      }
      if (i + 1 >= args.size()) return false;
      out = args[++i];
      return true;
    };
    std::string v;
    const auto need = [&]() -> proto::Expected<void, std::string> {
      if (!take(v) || v.empty()) return proto::Unexpected{std::string("missing value for ") + arg};
      return {};
    };
    const auto flag = [&]() -> proto::Expected<void, std::string> {
      if (has_value) return proto::Unexpected{arg + " takes no value"};
      return {};
    };
    proto::Expected<void, std::string> ok{};
    if (arg == "--game") {
      if ((ok = need())) o.game = v;
    } else if (arg == "--dll") {
      if ((ok = need())) o.dll = v;
    } else if (arg == "--also") {
      if ((ok = need())) o.also.push_back(v);
    } else if (arg == "--expect-graceui") {
      if ((ok = need())) o.expect_graceui = v;
    } else if (arg == "--gal") {
      if ((ok = need())) o.gal = v;
    } else if (arg == "--steam") {
      if ((ok = need())) o.steam = v;
    } else if (arg == "--appdata") {
      if ((ok = need())) o.appdata = v;
    } else if (arg == "--no-backup") {
      if ((ok = flag())) o.no_backup = true;
    } else if (arg == "--vanilla-offline") {
      if ((ok = flag())) o.vanilla = true;
    } else if (arg == "--backup-only") {
      if ((ok = flag())) set_action(Action::BackupOnly);
    } else if (arg == "--cleanup") {
      if ((ok = flag())) set_action(Action::Cleanup);
    } else if (arg == "--print-diagnostics") {
      if ((ok = flag())) set_action(Action::Diagnostics);
    } else if (arg == "--help") {
      if ((ok = flag())) set_action(Action::Help);
    } else if (arg == "--console") {
      if ((ok = flag())) o.console = true;
    } else if (arg == "--read") {
      if ((ok = flag())) o.read = true;
    } else if (arg == "--read-plan") {
      if ((ok = need())) {
        if (const auto n = number(v, 0, proto::watch::kPlanCount - 1)) {
          o.read_plan = *n;
          o.read_plan_given = true;
        } else {
          ok = proto::Unexpected{std::string("--read-plan is 0 (the rounds up to the seventeenth) or 1 (the survey)")};
        }
      }
    } else if (arg == "--read-pace-s") {
      if ((ok = need())) {
        if (const auto n = number(v, 1, 600))
          o.read_pace_s = *n;
        else
          ok = proto::Unexpected{std::string("--read-pace-s is 1 to 600 seconds")};
      }
    } else if (arg == "--spike-pace-s") {
      if ((ok = need())) {
        if (const auto n = number(v, 1, 600))
          o.spike_pace_s = *n;
        else
          ok = proto::Unexpected{std::string("--spike-pace-s is 1 to 600 seconds")};
      }
    } else if (arg == "--no-dialogs") {
      if ((ok = flag())) o.no_dialogs = true;
    } else if (arg == "--yes") {
      if ((ok = flag())) o.yes = true;
    } else if (arg == "--no-steam-check") {
      if ((ok = flag())) o.no_steam_check = true;
    } else if (arg == "--inject-at") {
      if ((ok = need())) {
        if (v == "suspended")
          o.inject_at = InjectAt::Suspended;
        else if (v == "window")
          o.inject_at = InjectAt::Window;
        else if (v == "title")
          o.inject_at = InjectAt::Title;
        else
          ok = proto::Unexpected{std::string("--inject-at is suspended, window or title")};
      }
    } else if (arg == "--title-wait") {
      if ((ok = need())) {
        if (const auto n = number(v, 0, 600))
          o.title_wait_s = *n;
        else
          ok = proto::Unexpected{std::string("--title-wait is 0 to 600 seconds")};
      }
    } else if (arg == "--appid-method") {
      if ((ok = need())) {
        if (const auto m = appid_method_named(v))
          o.appid_method = *m;
        else
          ok = proto::Unexpected{std::string("--appid-method is file or env")};
      }
    } else if (arg == "--remove-after") {
      if ((ok = need())) {
        if (const auto m = remove_after_named(v))
          o.remove_after = *m;
        else
          ok = proto::Unexpected{std::string("--remove-after is window, exit or never")};
      }
    } else if (arg == "--spike") {
      if ((ok = need())) {
        if (const auto s = spike_named(v))
          o.spike = *s;
        else
          ok = proto::Unexpected{std::string("--spike is s0a, s0b, s0c, s0d, s0d-crash, s0e, s0e_alone or s4")};
      }
    } else if (arg == "--runs") {
      if ((ok = need())) {
        if (const auto n = number(v, 1, 1000)) {
          o.runs = *n;
          runs_given = true;
        } else {
          ok = proto::Unexpected{std::string("--runs is 1 to 1000")};
        }
      }
    } else if (arg == "--run-timeout") {
      if ((ok = need())) {
        if (const auto n = number(v, 5, 3600))
          o.run_timeout_s = *n;
        else
          ok = proto::Unexpected{std::string("--run-timeout is 5 to 3600 seconds")};
      }
    } else if (arg == "--watch") {
      if ((ok = need())) {
        if (const auto n = number(v, 1, 0xFFFFFFF)) {
          o.watch_pid = *n;
          set_action(Action::Watch);
        } else {
          ok = proto::Unexpected{std::string("--watch takes the game's process id")};
        }
      }
    } else {
      return proto::Unexpected{"unknown option: " + arg};
    }
    if (!ok) return proto::Unexpected{ok.error()};
  }
  using proto::watch::Spike;
  if (actions > 1) {
    return proto::Unexpected{std::string("--backup-only, --cleanup, --print-diagnostics, --watch and --help go alone")};
  }
  if (o.vanilla && (o.dll || !o.also.empty() || o.spike != Spike::None || o.inject_at != InjectAt::Suspended)) {
    return proto::Unexpected{
        std::string("--vanilla-offline injects nothing: not with --dll, --also, --spike or --inject-at")};
  }
  if (o.action != Action::Start && (o.spike != Spike::None || !o.game_args.empty())) {
    return proto::Unexpected{std::string("--spike and game arguments go with a start")};
  }
  if (runs_given && o.spike != Spike::S0a && o.spike != Spike::S0dCrash) {
    return proto::Unexpected{std::string("--runs goes with --spike s0a or s0d-crash")};
  }
  if (o.spike == Spike::S0eAlone && o.dll) {
    return proto::Unexpected{std::string("--spike s0e_alone loads no eldennet.dll: not with --dll")};
  }
  if (o.yes && !o.no_dialogs) return proto::Unexpected{std::string("--yes goes with --no-dialogs")};
  if (o.read) {
    // The read-only round (Lodestar's L5): nothing is called, but it is a development run all the same: a save
    // backup made first, EldenNet's own DLL and one plain start.
    if (o.action != Action::Start || o.no_backup || o.vanilla || o.spike != Spike::None) {
      return proto::Unexpected{
          std::string("--read goes with a start, and not with --no-backup, --vanilla-offline or --spike")};
    }
    o.console = true;  // its plain lines
  } else if (o.read_pace_s != 0) {
    return proto::Unexpected{std::string("--read-pace-s goes with --read")};
  } else if (o.read_plan_given) {
    return proto::Unexpected{std::string("--read-plan goes with --read")};
  }
  if (o.spike_pace_s != 0 && !run_folder(o.spike)) {
    return proto::Unexpected{std::string("--spike-pace-s goes with --spike s0a, s0b, s0c, s0d, s0d-crash, s0e, "
                                         "s0e_alone or s4")};
  }
  return o;
}

std::string usage_text() {
  return "usage: eldennet-launcher [options] [-- game arguments]\n"
         "Starts ELDEN RING offline without Easy Anti-Cheat, loads EldenNet into it and stays as the crash watcher.\n"
         "  --game PATH          the game's executable or its folder (remembered); else found through Steam\n"
         "  --dll PATH           load this DLL instead of eldennet.dll beside the launcher\n"
         "  --no-backup          skip the save backup\n"
         "  --vanilla-offline    start offline without EAC and load nothing\n"
         "  --backup-only        back up the saves and exit\n"
         "  --cleanup            remove a steam_appid.txt this launcher created, if the game is not running\n"
         "  --print-diagnostics  print what discovery and the pre-flight checks find, and exit\n"
         "  --watch PID          watch a running game as its crash watcher (EldenNet starts this under other loaders)\n"
         "development and tests:\n"
         "  --inject-at suspended|window|title   when to load the DLL (default suspended)\n"
         "  --title-wait S       with --inject-at title: seconds after the window (default 15)\n"
         "  --also PATH          load this DLL too, before EldenNet's, unchecked\n"
         "  --expect-graceui PATH   a test package's graceui.toml: if the game's GraceUI is not that one, ask (E13)\n"
         "  --appid-method file|env   --remove-after window|exit|never   (else launcher.toml's)\n"
         "  --console            the development console: status lines, and the Mark on the Pause key\n"
         "  --read               the read-only round, with the probe build of eldennet.dll: the console; EldenNet\n"
         "                       names the managers it reads and tries its reader, and calls nothing in the game;\n"
         "                       a panel beside the game lists the tester's tasks, each with its timer, and the\n"
         "                       Pause key confirms the task under way (--read-pace-s S: the tests' pace;\n"
         "                       --read-plan N: which tasks, 0 the rounds up to the seventeenth, 1 the survey)\n"
         "  --spike s0a|s0b|s0c|s0d|s0d-crash  [--runs N] [--run-timeout S]   the S0 spikes: each run's folder\n"
         "                       spikes\\<word>-<n> holds EldenNet's report of it, and a panel beside the game lists\n"
         "                       the tester's tasks (--spike-pace-s S: the tests' pace)\n"
         "  --spike s0e|s0e_alone|s4   GraceUI's observer's runs (S0-E, S4): EldenNet calls nothing into the game,\n"
         "                       and s0e_alone loads only the --also DLLs; its report goes to spikes\\<word>-<n>;\n"
         "                       the panel lists the run's steps too\n"
         "  --no-dialogs [--yes] lines instead of dialogs; questions answered no (or yes)\n"
         "  --no-steam-check     --gal DIR   --steam DIR   --appdata DIR\n"
         "exit codes: 0 done; 1 a spike's runs did not all pass; 2 usage; 11 to 21 the errors E01 to E11\n";
}

}  // namespace en::launcher
