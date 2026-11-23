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
  en-gal's PE reader (`exe`), GraceUI's pre-flight (`graceui`, with our own SHA-256, `sha256`), the watcher's
  records, the spikes' tally and the pre-flight's order (`record`), and the observer's runs' folder name and the
  game's environment (`spikes`).
- `eldennet-launcher` (`src/win/`), the Windows builds' program: processes, injection, the watch block's mapping,
  the watch loop with the dev console and the Mark, the dialogs and the flow.

It depends on en-gal's offline part (the PE reader, `builds.toml`'s gate and the read-only round's report vocabulary,
`lodestar.hpp`'s; ARCH §2.3's dotted edge), and through it on en-proto, whose watch block (`watch.hpp`) it
shares with EldenNet in the game, and whose `ffi_guard` starts its entries (R3: `wWinMain` and its window-enumeration
callback, component `Launcher`). Never on en-core or eldennet's en_shell: `en_link_rule` (`cmake/link_rules.cmake`)
stops the configuration if `en_launcher_core` or `eldennet-launcher` reaches any target of the build but en-gal's
offline part, en-proto and the header-only libraries, and names the chain that reaches it.

## The command line

```text
eldennet-launcher [options] [-- game arguments]
  --game PATH  --dll PATH  --no-backup  --vanilla-offline            the player's (ARCH §4.1)
  --backup-only  --cleanup  --print-diagnostics  --watch PID          each alone
development and tests:
  --inject-at suspended|window|title  --title-wait S  --also PATH
  --appid-method file|env  --remove-after window|exit|never
  --console  --read  --spike s0a|s0b|s0c|s0d|s0d-crash|s0e|s0e_alone|s4 [--runs N] [--run-timeout S]
  --no-dialogs [--yes]  --no-steam-check  --gal DIR  --steam DIR  --appdata DIR
```

`--help` prints the list. Arguments that are no option (Proton's forwarded `%command%`, ARCH §4.9) are set aside and
counted; everything after `--` goes to the game (the tests' fake game takes its options so). Exit codes: 0 done (the
game started and ended, whatever its own exit), 1 a spike's runs did not all pass its rule, 2 a usage error, 3 nothing
could be done (no AppData folder, nothing to watch), and 11 to 21 the errors E01 to E11 (10 + the error's number);
E12, GraceUI's pre-flight, is told and never ends the launcher.

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
   game (`dinput8.dll`, `winhttp.dll`, `dxgi.dll`) are named and never block. Then **GraceUI** (ARCH §4.3, GraceUI D83,
   D88), when EldenNet is loaded: `GraceUI\graceui.toml` beside the game readable, as GraceUI's DESIGN §2.7 has it and
   GraceUI's own `tools/release/manifest.py` writes it in every GraceUI build's folder (D165): `graceui`, `abi`, and
   `[files]` with each file's size and SHA-256 are read; `protocol`, `caps` and `released` are GraceUI's, accepted
   whatever they hold and never a failure. Every listed file must match (in the order of their names; a name outside the
   folder refuses the file), its ABI major 1 and its version at least EldenNet's floor, 0.2.3 (en-proto's
   `graceui_pin.hpp`). The log says `preflight graceui=ok|no_folder|no_manifest|unreadable|
   file_missing|size_differs|hash_differs|abi_major|too_old version=… abi=… files=… checked=…`. What fails is told in
   **E12**, in E08's manner (what is missing or damaged, by its name inside GraceUI's folder, never a path, and what to
   do), and the game starts all the same: EldenNet runs with nothing shown in game (ARCH §10.3). `--read` and the
   spikes, headless by design (O90), only log it. Installing the GraceUI release EldenNet carries waits until it carries
   one (GraceUI D53).
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
session's record.

**The console never holds up the watch** (WHY§290, since spike.10). An earlier launcher wrote each line to the console
before its log, under one lock, from the watch loop; a classic console window whose text is selected (a click inside it,
QuickEdit) suspends its output, and the loop stopped there with its status lines, the round's steps, the lint and the
crash watcher (R1's run 9). Now every line goes to `launcher.log` first, then into a bounded queue (`LineQueue`, 256
lines) that one thread of its own writes to the console (or the standard output a test reads); a line that finds the
queue full is dropped, never waited on, the first drop noted in the log (`console dropped=1 queue_full=1`) and the
totals at the end (`console lines=… dropped=… drained=…`, after waiting a second at most for the console). QuickEdit is
off for the launcher's console while it runs, the console's mode restored at the end. For the tests only,
`ELDENNET_TEST_CONSOLE` blocks the writer after N lines (`block_after=N`), slows it (`slow_ms=N`) or shortens the queue
(`queue=N`).

**GraceUI's Mark** (part 12) is EldenNet's own: a GraceUI hotkey on the same key, Pause, which EldenNet registers when
the console holds Pause. EldenNet then asks for the ring (the block's `marks.source`, en-proto's `MarkSource`); the
launcher lets its key go, takes the presses already queued as its last, and hands the ring over
(`mark hotkey=pause released=1 source=eldennet`). From then on EldenNet alone stamps Marks, numbered on in the same
ring, and the launcher reads each from the ring into the session's record (`mark n=… mono_ms=… source=eldennet`). One
writer at a time, and the owner presses Pause either way. EldenNet asks only once its hotkey is bound: when another
mod's GraceUI hotkey took Pause first, EldenNet's registers unbound (GraceUI D164) and never asks. Then, as without
GraceUI, nothing asks, and the launcher's Pause stays the Mark.

## The read-only round with --read (Lodestar's step L5, the owner's in-game round)

`--read` asks a probe build of eldennet.dll for the read-only round (src/eldennet/README.md), in which EldenNet names
the managers it reads and tries its reader, and calls nothing in the game. It brings `--console`, and goes with a start
only, never with `--no-backup`, `--vanilla-offline` or `--spike`. The save backup is made first, and with `--read` a
failed one ends the launcher (E07, with no "continue without a backup"). Before it loads anything the launcher writes
the request into the watch block's `control.probe` (`read`), and EldenNet answers there: accepted, or refused (not a
probe build, or turned off), when it stops. With `--read` the launcher also shows the tester's panel (the owner's
request, 2026-11-22; `tasks.hpp`, `win/panel.cpp`): a window of its own at the top right of the game's screen,
always on top and never taking the keyboard, that lists the round's tasks in order, each with its timer, the done
ones greyed. A waiting task ends when its time is up; a task of the tester's ends at the Pause key (the launcher's
system hotkey) or a click on the panel's button, which speaks for the task it belongs to (each such task has its
own word for it); the round's own first task ends when EldenNet says to load the save; the last task (quit the
game) has no button and ends with the watch. The confirmation of a marked task stamps a Mark into the watch block, which the round reads
(`--read-pace-s S`, for the tests: every wait lasts S seconds and the tester's tasks confirm themselves). Each task
is also told on the console and logged (`task n=… of=… by=key|click|time|round mark=…`). (`--probe`, which asked for R1, the round with one
call into the game, went with it: retired 2026-11-22, O106; it is an unknown option now, and its two words in the block
keep their numbers, unused.) The console tells each step once (`EldenNet read: …`), beside the log's `read step=<word>
named=<n> of=<m>` (`texts.hpp`'s `ReadStep`, from EldenNet's answer, T1's state and the status's items, which in this
round are the managers named, asked for and the naming passes): `asked`, `reading` (the game's image, at the title),
`load_save` (a first pass is done and a manager is still unnamed: "Now load your save."), `done` (every manager named:
load the save if it is not loaded yet, and stay about ten minutes in the world), or `refused` and `stopped`. When the
game ends, the launcher checks the two files in `%APPDATA%\EldenNet\probe\`: `lodestar-found.json` by the lint and the
`lodestar` vocabulary, and `lodestar-table.toml` by `table_text_clean` (only `[provenance]`, `[item.<label>]` and known
keys with numbers, our labels and fixed words: no path, no text), each as `read report=<name>
lint=clean|refused|missing`, and says `reports_clean`, `reports_refused` or `reports_missing`.

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
- `s0e`, `s0e_alone`, `s4`: the runs of GraceUI's observer (S0-E's present-hook half and S4, input; roadmap S10.1.g,
  S11.1.a), whose measurements are GraceUI's (grace-spike.dll's report): one watched session each, passing when it
  did not crash. Before the game starts the launcher makes the run's folder, `spikes\<word>-<n>` (n one more than the
  word's highest there, from 1; `--appdata` moves it with everything else; the name is the word and the number,
  nothing else), and names it in the game's environment as `ELDENNET_SPIKE_DIR`, where the observer writes its report
  (`spike_dir spike=<word> n=<n> made=1`, never the path). With `s0e` and `s4` eldennet.dll stops at Register and
  makes no call into the game (src/eldennet/README.md); `s0e_alone` loads no eldennet.dll at all, only the `--also`
  DLLs (`started … eldennet=0`; no `--dll` with it). `--read` goes with none of them, so the read-only round is never
  in such a run.

In every other start `ELDENNET_SPIKE_DIR` is not set: one the launcher's own environment holds is left out of the
game's. The game's environment is always made by the launcher (`spikes.hpp`'s `game_environment`: the launcher's own,
with the appid for the env method and the spike folder as above).

## Files

`%APPDATA%\EldenNet\` (`--appdata` in place of `%APPDATA%`): `config\launcher.toml` (the game's path, `appid_method`,
`remove_after`, `notices_seen`), `config\launcher-state.toml` (the appid file it made), `logs\launcher.log` (its
lines; one older log is kept past 1 MiB), `crashes\`, `spikes\`, `save-backups\`. Its lines (`LAUNCHER <event>
key=value … t=<ms>`, on standard output when there is one, and in the log) hold numbers and fixed words, never a path,
a user name or an id.

## What waits

GraceUI's install (ARCH §4.3: the release EldenNet carries, once it carries one), plugins (§4.12, from 1.1), the dashboard off
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
round trip; `launcher.toml`; the command line and its refusals (`--read`'s and `s0e_alone`'s among them); the spike
folder's name and the game's environment (the variable set in an observer's run, dropped otherwise, any case of its
name); the console's queue (a writer that blocks never holds up the lines' side or the drain past its time, a slow one
takes every line in order, the drops counted) and its test hook; the read-only round's steps on the console
from EldenNet's answer and T1's state; the texts and exit codes; records, their JSON and the
next start's explanation; the spikes' tally and rules; the pre-flight's order; the gate and the DLL check on invented
images (en-gal's FakeImage); SHA-256 against NIST's examples (whole, in pieces, every length around the padding's
edges, a file); GraceUI's pre-flight on invented GraceUI folders (every file matching, a newer minor, and each way it
fails), a `graceui.toml` that GraceUI's own `manifest.py` wrote for an invented folder of ours
(`tests/graceui-manifest.toml`, compiled in byte for byte) and D165's keys with other values, and `graceui.toml`'s
refusals; E12's text.

End to end, the fake game's harness runs the launcher under Wine (`ctest -R fakegame.launcher`): in both builds, a
vanilla start (the appid file made and removed at the window, the last crash explained), a crash with no DLL, the env
method, someone else's appid file, `--backup-only`, `--cleanup`, `--print-diagnostics` through invented Steam folders,
E02, E05, E08 and a usage error; with clang-cl and galtest.dll as the DLL, injection suspended, at the window and at
the title (Ready, the dev console's status, en-core's lifecycle and gate in game, a Mark pressed in the fake game and
seen in the block), a crash on the game thread recorded through EldenNet's filter, a game that replaces the filter, a
stand-in crash reporter loaded with `--also`, the S0-A, S0-D crash and S0-C loops, and the watcher started by
EldenNet under another loader, and a crash while the console is blocked (`launcher-crash-console`); and with
eldennet.dll, the read-only round as the owner runs it (`--read`) and `--read` refused by the release DLL; the runs
of GraceUI's observer (`launcher-eldennet-spike-s0e`, `-s4`, `-s0e-alone`: the folder, the
variable, no call into the game); and, with the GraceUI folder of a test build of GraceUI installed beside the fake
game, the pre-flight passing on GraceUI's own `graceui.toml`, the Mark handed over to GraceUI's hotkey
(`fakegame.launcher-eldennet-graceui`), the launcher keeping Pause when another mod holds it in GraceUI
(`fakegame.launcher-eldennet-graceui-unbound`), and an S0-E run's banner as GraceUI's notice
(`fakegame.launcher-eldennet-graceui-spike-s0e`). `launcher-vanilla`, `launcher-env` and `launcher-spike-s0c` check
that a leftover `ELDENNET_SPIKE_DIR` never reaches the game.
