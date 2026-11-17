# en-launcher: eldennet-launcher and its crash watcher

`eldennet-launcher.exe` is our own launcher (ARCH §4, DESIGN §3.2, O5), C++20, MIT: it finds the game, checks before it
starts, backs up the saves, starts the game offline without Easy Anti-Cheat, loads EldenNet into it and stays in the
background as the crash watcher until the game ends (O71). No code comes from any other launcher or loader; it uses
nothing of me3 or fromsoftware-rs (O47, O48), and a player's own me3 profile may list `eldennet.dll` as a native
instead (ARCH §4.11). It is a windowless Win32 program: errors and notices are task dialogs.

It is built in two parts (the first in-game build, part 7):

- `en_launcher_core`, portable and tested on Linux: Steam's KeyValues reader and the game's discovery (`vdf`,
  `steam`), the save backups (`backup`, with `blake3`), the appid file and the launcher's record of it (`appid`),
  `launcher.toml` (`config`), the command line (`options`), the texts (`texts`), the executables' facts through
  en-gal's PE reader (`exe`), and the watcher's records, the spikes' tally and the pre-flight's order (`record`).
- `eldennet-launcher` (`src/win/`), the Windows builds' program: processes, injection, the watch block's mapping,
  the watch loop with the dev console and the Mark, the dialogs and the flow.

It depends on en-gal's offline part (the PE reader and `builds.toml`'s gate; ARCH §2.3's dotted edge), and through it
on en-proto, whose watch block (`watch.hpp`) it shares with EldenNet in the game, and whose `ffi_guard` starts its
entries (R3: `wWinMain` and its window-enumeration callback, component `Launcher`).

## The command line

```text
eldennet-launcher [options] [-- game arguments]
  --game PATH  --dll PATH  --no-backup  --vanilla-offline            the player's (ARCH §4.1)
  --backup-only  --cleanup  --print-diagnostics  --watch PID          each alone
development and tests:
  --inject-at suspended|window|title  --title-wait S  --also PATH
  --appid-method file|env  --remove-after window|exit|never
  --console  --probe  --spike s0a|s0b|s0c|s0d|s0d-crash [--runs N] [--run-timeout S]
  --no-dialogs [--yes]  --no-steam-check  --gal DIR  --steam DIR  --appdata DIR
```

`--help` prints the list. Arguments that are no option (Proton's forwarded `%command%`, ARCH §4.9) are set aside and
counted; everything after `--` goes to the game (the tests' fake game takes its options so). Exit codes: 0 done (the
game started and ended, whatever its own exit), 1 a spike's runs did not all pass its rule, 2 a usage error, 3 nothing
could be done (no AppData folder, nothing to watch), and 11 to 21 the errors E01 to E11 (10 + the error's number).

The development options are built into every launcher and listed apart: they change nothing unless given. `--also`
loads a DLL that is not ours (a stand-in for another mod, such as the tests' `vehprobe.dll`), before EldenNet's.

## What a start does (ARCH §4.2 to §4.6)

1. **Explains the last session**: a record the watcher kept and no start has explained yet (below) is shown once.
2. **Finds the game**: `--game` (the executable or a folder holding it, remembered in `launcher.toml`), else the
   remembered path, else Steam: the registry's Steam folder (`--steam` in its place), `steamapps\libraryfolders.vdf`
   in either of its forms, each library's `appmanifest_1245620.acf` and its one-folder `installdir`, and
   `steamapps\common\<installdir>\Game\eldenring.exe`. Nothing found: E01, with "Open eldenring.exe…".
3. **Removes a stale `steam_appid.txt`** its record says it made, if the game is not running (`--cleanup` does only
   this).
4. **The first-screen notices** (ARCH §4.8), on the first run and when their version changes; "Don't show again" is
   kept as `notices_seen`. The dashboard's address joins them with GraceUI's pre-flight.
5. **The gate**: the executable's fingerprint against `gal/builds.toml` beside the launcher (`--gal` in its place).
   An unknown build, or no `builds.toml`, is E05: launch anyway, and EldenNet stays inactive (its own gate).
6. **Pre-flight** (ARCH §4.3), in this order: Steam running (`steam.exe`, or the registry's live `ActiveProcess`), no
   `start_protected_game.exe`, the game not running, and the DLL ours (`--dll`, else `eldennet.dll` beside the
   launcher: its version resource's ProductName reads `EldenNet`): E02, E04, E03, E08. Other proxy DLLs beside the
   game (`dinput8.dll`, `winhttp.dll`, `dxgi.dll`) are named and never block. GraceUI's pre-flight comes with its host.
7. **Save backups** (ARCH §4.4): each `%APPDATA%\EldenRing\<17 digits>\` folder's `ER0000.sl2` and `.bak`, to
   `%APPDATA%\EldenNet\save-backups\<id>\<YYYYMMDD-HHMMSS>\` (each copied to `.tmp`, its size and BLAKE3 hash checked
   against the source, then renamed), with a `manifest.toml` of sizes and hashes; no new backup when every file equals
   the newest's; the newest 10 kept, older ones deleted only after a new one was made; nothing under the save folder
   ever written. A failure is E07 (continue without a backup?). BLAKE3's hash mode is our own (`blake3.cpp`, from
   the specification), checked against its published test vectors.
8. **Without EAC** (ARCH §4.5), for each run: the file method writes `1245620` into `steam_appid.txt` beside the
   game if it is absent (recorded in `launcher-state.toml` as ours), uses one that holds the id as it is, and leaves
   one with anything else alone, starting that run with the environment instead; it cannot write: E06. The env
   method (`appid_method = "env"`, pending S0-C) sets `SteamAppId` and `SteamGameId` in the game's environment. Ours
   is removed once the game's window shows (or after 120 s), at the game's exit, or never (`remove_after`), and only
   while it still holds the id.
9. **Start and inject** (ARCH §4.6): `CreateProcessW` suspended in the game's folder, no handles inherited; the watch
   block made; each DLL (`--also` first, then EldenNet's) loaded by its absolute path on a thread of the game's own
   on `LoadLibraryW`, waited for (30 s), found in the game's module list, its memory freed; then the game resumed.
   With `--inject-at window` the game runs first and the DLLs load once its window shows, with `title`
   `--title-wait` seconds after (about when the title shows: a late load, ARCH §3.3). A failed step ends the game
   and is E10, naming the step; "Start without EldenNet" starts it again with nothing loaded. `--vanilla-offline`
   loads nothing.
10. **Watch** until the game ends, then exit.

## The crash watcher (O71)

The launcher makes the **watch block** (en-proto's `watch.hpp`: one page, `Local\EldenNet.Watch.<pid>`, with two
events) before it loads anything, and EldenNet opens it: EldenNet writes T1's state and its status there, and, from
its crash filter, a crash record (the code, the thread, the faulting module's file name and offset), then wakes the
watcher and waits at most 2 s. While it waits, the watcher lists the game's modules (by file name) and answers. When
the game ends, the watcher reads its exit code and keeps a **record** in `%APPDATA%\EldenNet\crashes\` of any end that
was not clean (a crash, or an exit with a code): the kind, the codes, EldenNet's last status and crash record, the
modules, the session's length, the Marks. Numbers, fixed words and module file names only: no dump, no path, no user
or account name, nothing of the game's or the player's text (the scrubbed summary O71 allows to be shared; a full dump
is never written). A game EldenNet's filter never saw (a fast fail, or a game that replaces the filter) still leaves
its exit code and the last module list (listed every 5 s). The newest 20 records are kept, and the next start explains
the newest one.

Under another loader, EldenNet starts the watcher itself: `eldennet-launcher --watch <pid>` opens or makes the block
and watches the same way.

## The dev console and the Mark (O90)

`--console` prints EldenNet's status from the block every second while it changes (T1's state, the lifecycle's phase,
the level, the gate's flags, discovery's items and passes, snapshots, faults, groups tripped, the task's p99, the
Marks), on the console the launcher was started from or one of its own, and registers **Pause** as a system hotkey
(`RegisterHotKey`, no hook). Each press is a Mark: numbered from 1, stamped in mono milliseconds (QPC, the snapshots'
own clock) into the block's ring of 32, where EldenNet's recorder takes it into the recording, and kept in the
session's record. It is retired when GraceUI's Mark arrives.

## R1 with --probe (O89, the owner's in-game round)

`--probe` runs the derivation's in-game round with the probe build of eldennet.dll (src/eldennet/README.md): it brings
`--console` (the Mark), and goes with a start only, never with `--no-backup`, `--vanilla-offline` or `--spike`. The
save backup is made first, and with `--probe` a failed one ends the launcher (E07, with no "continue without a
backup"). Before it loads anything the launcher writes R1 into the watch block's `control.probe`, and EldenNet answers
there: accepted, or refused (not a probe build, or turned off), when it stops and calls nothing.

The console then tells each step once, in plain words (`EldenNet R1: …`, on the console only), beside the log's
`r1 step=<word>` (`texts.hpp`'s `R1Step`): `asked` (stay at the title, load no save), `reading` (the live derivation
and its checks), `press_mark` (the checks passed: press Pause at the title with no save loaded), `called`,
`task_confirmed` (the game runs our task: load the save and play), or `refused`, `not_called` (a check failed),
`call_failed`, `no_confirmation` (30 s after the call). When the game ends, the launcher lints both reports in
`%APPDATA%\EldenNet\probe\` with the vocabularies `eldennet-engine lint --report` uses (`r1 report=<name>
lint=clean|refused|missing`) and says `reports_clean`, `reports_refused` or `reports_missing`.

## The S0 spikes (DESIGN §16.1, the plan's §1.2)

`--spike` tells EldenNet the spike (in the block) and keeps a numbers-only log in `%APPDATA%\EldenNet\spikes\`: a JSON
line per run and the tally.

- `s0a --runs N`: each run starts and injects (as `--inject-at` says), waits for T1 to stop (Ready, a banner, inert)
  or `--run-timeout` (180 s), then closes the game (`WM_CLOSE`, then after 10 s `TerminateProcess` with "ENLC").
  It passes when every run reached Ready or L1 and none crashed.
- `s0d-crash --runs N`: at Ready it asks EldenNet for a crash (a probe build's) and waits for it; it passes when every
  run's crash was recorded.
- `s0b`, `s0c`, `s0d`: one watched session; `s0c` counts the game's TCP connections to anything but this PC every 2 s
  (the most at once; no address is kept). They pass when the session did not crash.

## Files

`%APPDATA%\EldenNet\` (`--appdata` in place of `%APPDATA%`): `config\launcher.toml` (the game's path, `appid_method`,
`remove_after`, `notices_seen`), `config\launcher-state.toml` (the appid file it made), `logs\launcher.log` (its
lines; one older log is kept past 1 MiB), `crashes\`, `spikes\`, `save-backups\`. Its lines (`LAUNCHER <event>
key=value … t=<ms>`, on standard output when there is one, and in the log) hold numbers and fixed words, never a path,
a user name or an id.

## What waits

GraceUI's pre-flight and its install (ARCH §4.3, with GraceUI's host), plugins (§4.12, from 1.1), the dashboard off
game (§4.13), Proton (§4.9: E11's text is here, raised once S0-C settles the recipe), `--selftest` with the fake game
for the owner's smoke run (the spike package's, docs/design/spikes/README.md), EldenNet's last log lines in a record
(with the DLL, part 6), and signing.

## Tests

`en_launcher_tests` (doctest; Linux with GCC and Clang, and the Windows builds under Wine): BLAKE3 against 16 of its
published vectors, whole and in pieces of seven sizes, and a file's digest; KeyValues (escapes, comments, conditions,
case, every refusal and bound); finding the game in invented Steam folders in either form of the library list, with
manifests that name a path, a missing executable, no list; `--game` as a file or a folder; backups of invented save
folders (the manifest's hashes, no duplicate, a changed or vanished file, two in one second, the rotation of 3 over 6,
folders that are no backup left alone, nothing under the saves touched, a backup folder that cannot be made); the
appid file (created, used, someone else's, removal only of ours and only while it holds the id) and the state file's
round trip; `launcher.toml`; the command line and its refusals (`--probe`'s among them); R1's steps on the console
from EldenNet's answer and T1's state; the texts and exit codes; records, their JSON and the
next start's explanation; the spikes' tally and rules; the pre-flight's order; the gate and the DLL check on invented
images (en-gal's FakeImage).

End to end, the fake game's harness runs the launcher under Wine (`ctest -R fakegame.launcher`): in both builds, a
vanilla start (the appid file made and removed at the window, the last crash explained), a crash with no DLL, the env
method, someone else's appid file, `--backup-only`, `--cleanup`, `--print-diagnostics` through invented Steam folders,
E02, E05, E08 and a usage error; with clang-cl and galtest.dll as the DLL, injection suspended, at the window and at
the title (Ready, the dev console's status, en-core's lifecycle and gate in game, a Mark pressed in the fake game and
seen in the block), a crash on the game thread recorded through EldenNet's filter, a game that replaces the filter, a
stand-in crash reporter loaded with `--also`, the S0-A, S0-D crash and S0-C loops, and the watcher started by
EldenNet under another loader; and with eldennet.dll, R1 as the owner runs it (`--probe`) and `--probe` refused by the
release DLL.
