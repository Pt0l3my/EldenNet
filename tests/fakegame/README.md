# The fake game

`fakegame.exe` is our own Windows program that stands in for a game in EldenNet's tests: the derivation kit
(`probe-exe`), the game-access layer in game, the DLL and the launcher are tested against it under Wine, and the owner
can run it on Windows for the smoke run. **Nothing in it is the game's.** Every name, class, path, string, byte pattern,
offset, enum value and number is invented here, shaped only by what the design describes (ARCH §1.4, §3.2, §4.6,
§6.1 to §6.6). It is MIT, like the rest of the engine, and is never shipped to players.

`tests/vehprobe/vehprobe.dll` is our own stand-in for a crash reporter with a vectored exception handler (S0-D, O91).

## What each part stands in for

| Part | Stands in for |
|---|---|
| A visible top-level window, then a non-null `fg_static_hinstance`, then the statics (`--boot-delay-ms`, `--statics-delay-ms`, `--no-window`) | The game's start as T1 meets it: readiness 1 (the window) and 2 (statics non-null; ARCH §1.4, §3.2) |
| The frame loop on the main thread: ten groups (`Fg_FrameOpen` … `Fg_FrameClose`) run in order every frame at `--fps`, with `--hitch-every`/`--hitch-ms` and `--stall-at`/`--stall-ms` | The game thread and its task groups; `Fg_ActorSettle` (index 5) stands for the group where positions are final (`ChrIns_PostPhysics`) |
| The group names in a static table of 16-byte entries `{ index, flags, name }` | A table of groups wider than a pointer (R0's review), found at its stride |
| The hub's live tasks in a table its instance points at (`TaskLoomFields.live`, +0x28) | A game's hub holding its task lists, which R1's census follows to the tasks' classes and their base (part 6) |
| `fg_register_task`, found by an interior pattern at entry + 7 inside a `.pdata` function, with our own ABI: **rcx the task, rdx the hub instance, r8d the group**; the program's four own call sites load the hub from its static into rdx | `register_task` and its `register_abi` (ARCH §6.2, §6.4). Bad arguments fault in the caller's thread; `--register fault` faults on request, `--register ignore` never runs the task |
| A task object: a vtable pointer, then 0x18 bytes the hub writes (group, state, hub, a run count), so `object_size` is 0x18; three slots: 0 attach (once, on the game thread), **1 run (`self`, `FrameInfo*`) every frame**, 2 query (every 64th frame) | The task object's shape the derivation must find (`vtable_slots`, `run_slot`, `object_size`) |
| Seventeen accessors with the FD4 shape (`mov rbx,[rip+static]; test; jnz; lea rcx,[rip+class]; call getter; mov rdx,rax; mov r8d,site; lea rcx,[rip+path]; call log`), the path ending in `FD4Singleton.h`, the class object's name at +0x18 | The FD4 singleton idiom (ARCH §6.2): eleven names (`TaskLoom` for CSTask's role, `ActorRoster` for WorldChrMan's, and so on; `--describe` maps each), two of them with a second site, and three decoys: `Duplicata` (two statics: ambiguous), `Strayling` (path ends in `.hpp`), `Rockbound` (its static is in `.rdata`) |
| R0c's variants: five more sites of five of those names, with the same head but the static in rdi, r12, rsi or rbx, and the path loaded after a format string, into rdx right after the getter's call, through a pointer to it, by its absolute address, or into rsi before the head; and `Mimicry`, a decoy whose "static" is read through rsi, which only the idiom's wildcards match | The ways a game's compiler may load the path (R0's review), so that the idiom cut from them has the registers' bytes wildcarded (as the game's was) and no path capture, and the finder must find each path by its text; `--describe` gives each site's path load |
| Four AOB functions, each with a distinct shape referring to a static at entry + 5 or later | `global_hinstance`, `GameDataMan`, `GameMan` and `FieldArea` (ARCH §6.1) |
| C++ classes in the invented namespace `FG` (`FG::Actor`, `FG::PlayerActor`, `FG::NpcActor`, `FG::BeastActor`, `FG::MountActor` with a second base `FG::Saddle`, `FG::Prop`, `FG::Tombstone`, the singletons' `FG::…Imp`) and `Decoy::Actor` | MSVC RTTI for the walk (ARCH §6.2): type descriptors in `.data`, locators in `.rdata`, an actor set with a second vtable and a same-named class elsewhere. The clang-cl build only: MinGW's GCC writes its own RTTI |
| Title, loading and world phases (`--title-frames`, `--load-frames`, `--stay-title`, `--reload-every`), a main player and `--actors` actors with health, positions and a distance-sorted list | The lifecycle and the structural groups' self-tests (ARCH §6.7, §9) |
| `--move-every` with `--move-mode poison\|free\|keep`: every singleton, the ledger's sheet and the main player move; the old place becomes an `FG::Tombstone`, is released, or stays | Statics that move and torn-down objects (ARCH §6.6, R5) |
| `--online`, `--online-at` | The session turning online (the gate) |
| `MarkLedger`'s flag store: a binary tree of three groups (0, 7, 4321) under a sentinel head, blocks of 125 bytes by an offset from a holder (mode 1) or by address (mode 2); flags 100 and 4321010 always set, 7005 while in the world | The flag tree and its blocks (ARCH §6.9) |
| `PhraseBook`'s 24 text tables, three holding invented strings (ids 1100 to 1102 in table 7, 6100100 in table 9, 500 in table 3), each a header, a group of ids, offsets and UTF-16 strings | The text tables (ARCH §6.9), in a layout of our own |
| `TableVault`'s param tables: slot 6 holds a row for each actor's `npc_param_id` (81100 to 81600), sorted by id | The param tables (ARCH §6.9) |
| `--crash av-read\|av-write\|av-exec\|div-zero\|illegal\|stack-overflow\|breakpoint\|fastfail\|terminate` at `--crash-at`, on the game thread or a worker; `--game-filter`, `--game-filter-at`, `--game-filter-result` | A game that crashes, and one that installs (or replaces) its own unhandled-exception filter (S0-D) |
| `--handled-every` with `--handled-kind seh\|cxx` | Exceptions a game raises and handles itself, which a vectored handler sees first |
| `--load [boot\|window\|ready\|title\|world:]PATH` | A loader that loads a DLL at one moment (ARCH §3.3's late load) |
| `--selftest-task` (`--selftest-abi`, `--selftest-object-size`, `--selftest-group`) | EldenNet's T1 and trampoline, in miniature: a task registered from another thread, its slots counted |
| `--press-pause-at F`: Pause pressed and released (`SendInput`) at frame F, and with `--press-pause-every N` again every N frames | The player pressing the launcher's Mark, a system hotkey (O90), until it is taken |
| `--d3d12`: a Direct3D 12 device, queue and flip-model swap chain (960 by 540) on the window, cleared and presented every frame; `d3d12.dll` and `dxgi.dll` loaded by name with this option alone | A game's swap chain, which GraceUI's overlay binds and draws on (part 12) |
| `--press-f11-at F,F…`: F11 pressed and released into the window at each frame listed, the cursor in it and the window in front and holding the focus (as GraceUI's harness found Wine needs); with `--d3d12`, `--press-pause-at` presses Pause the same way | The player pressing GraceUI's hotkeys: F11 for EldenNet's debug view, Pause for its Mark |
| `--grace-report F,F…` with `--grace-probe x0,y0,x1,y1,r,g,b`: at each frame listed, one `FAKEGAME grace` line of what a test build of GraceUI says through its test exports (`graceui_test.h`, read through `GetProcAddress`, never linked): the host's state, the mods connected and EldenNet's caps, its notices under keys 1 and 2 (spaces written as `_`), the panels made, shown and drawn, the lists, the textures, the world-state providers and their flags, the window's filter; and, with `--d3d12`, the pixels within 3 of the colour in the rectangle of the frame just presented, read back after GraceUI drew on it | GraceUI's harness reports, from inside the game's process |
| `--grace-take-pause`: at boot, before the window, the GraceUI already loaded (the launcher's `--also`) found by its name through graceui-sys, a connection as `fakegame-other` with the `hotkeys` cap, and Pause registered as its hotkey; one `FAKEGAME grace_other found=… connect=… hotkey=… id=…` line | Another mod that took Pause in GraceUI first, so that EldenNet's Mark registers unbound (GraceUI D164) |
| `vehprobe.dll`: `VEHPROBE_MODE=pass\|swallow\|off`, `VEHPROBE_FIRST`, `VEHPROBE_EXIT`, `VEHPROBE_LOG` | A crash reporter's vectored handler that passes faults on, or ends the process at the first (S0-D, ARCH §6.5) |

The version resource is invented too: FileVersion 3.14.15.0 (the fourth field 0: "ww"), ProductName
`EldenNet Fake Game` followed by a trademark sign (U+2122), which the derivation and the gate compare without, and
translation 0x0809 and 1200.

## Status lines and `--describe`

Every line it prints starts with `FAKEGAME <event>` and holds `key=value` pairs, ending with `t=<ms>` since the start;
the last is `FAKEGAME end frames=… builtin_runs=… foreign_tasks=… foreign_runs=… moves=… hitches=… handled=… exit=…`.
`--status-file PATH` appends them to a file as well, for runs on Windows, where a program started by a launcher has
no console. `vehprobe.dll` prints `VEHPROBE …` lines the same way. `--help` lists the options. Exit codes: 0 or
`--exit-code`, 2 for a bad command line, and a crash's own code (under Wine only its low 8 bits reach Linux).

`fakegame.exe --describe` prints one JSON object: the fingerprint, every FD4 site, the AOB items with their patterns
and captures, the registration's ABI, the task's shape and the group table, every class's vtable, the field offsets
and the enums. It is the truth the derivation kit's and the GAL's tests compare their answers with, and it checks its
own patterns against its own code (`self_check`).

## The harness

`harness.py` runs each case of its `CASES` table under Wine on Xvfb (`xvfb-run -a`), checks the exit code and the
lines, and, for `describe`, reads `fakegame.exe` from outside with a PE reader of its own: the fingerprint, each AOB
pattern (one match, inside its `.pdata` function, the capture in `.data`), the FD4 finder's whole logic (placements,
naming through the class object, the ambiguous and refused decoys) and, for clang-cl, the RTTI locators and the actor
set. CTest runs one test per case (`ctest -R fakegame`, about 2 s each); the first, `fakegame.prefix`, makes the Wine
prefix (`EN_WINE_PREFIX`, by default `build-*/wineprefix`) and turns Wine's crash dialog and its JIT debugger off (with
`winedbg --auto` attached, a crash's exit code raced it), and no two cases run in it at once, each with a wineserver of
its own. On Windows the cases run directly (`--native`). A case may start another program with paths CMake gives it
(`program="{launcher}"`, `--set launcher=…`), so the DLL's and the launcher's chain cases, early and late, join the
same table when those parts land.

The game-access layer in game (part 4) joined it with `tests/galtest/galtest.dll`, our own test DLL, which the fake
game loads (`--load ready:` or `window:`) and which does what EldenNet's T1 and task will do: the gate, discovery with
its retries, the one registration, the task's reads at 4 Hz, the probe's slot searches and, on request, fault injection
(`EN_GALTEST_FAULT`) and the ReadProcessMemory copy (`EN_GALTEST_COPY=rpm`). Each `gal-*` case first makes gal/ as the
derivation round would (`make_gal`): `probe-exe --derive` on `fakegame.exe` with the leads and the AOB statics' slots,
then builds.toml, patterns.toml and the layout from derive.json and `gal-layout.toml.in`, whose offsets it checks
against `--describe`; then it runs the fake game with the DLL and checks the `GALTEST` lines: every item where
`--describe` says, one registration, every group Ok, about one snapshot per 15 frames at 60 FPS, the world's values,
the flags, text and rows asked for, the slot counts (the run slot every frame, attach once, query every 64th), the slot
searches' finds, no allocation on the game thread; and, by case, moving statics, loading screens, the session going
online, an optional group's breaker, a structural group's (inert), the RPM copy, and a group table shifted since the
derivation (the first tick finds another name and goes inert).

The launcher (part 7) joined it with the `launcher-*` cases: each runs `eldennet-launcher.exe` with `--no-dialogs`
on a copy of `fakegame.exe` in the case's own folder (`{game}`), with an `%APPDATA%` of its own (`{appdata}`) and
gal/ (the derivation's for the cases with `galtest.dll`, else a `builds.toml` written from the fake game's own
fingerprint), after the case's own invented files (saves, a stale record of the appid file, someone else's appid
file, a crash record of an earlier session, Steam folders with a library list and an app manifest). The fake game's
arguments follow `--`, with `--status-file`, where galtest's and vehprobe's lines go too (a game started by a launcher
has no console). The checks read the launcher's `LAUNCHER` lines and the files it leaves: the appid file and its
record, backups and their manifests, the watcher's crash records (each checked for paths and the user's name), the
spikes' logs. Eleven cases run in both builds (no DLL: a vanilla start and a crash, the env method, someone else's
appid file, `--backup-only`, `--cleanup`, `--print-diagnostics`, E02, E05, E08, a usage error); ten more with
`galtest.dll` in the clang-cl build: injection suspended, at the window and at the title, a crash recorded through
EldenNet's filter, a game that replaces the filter, `--also vehprobe.dll`, the S0-A, S0-D crash and S0-C loops, and
`launcher-watch`, where the fake game loads galtest itself and galtest starts the watcher (`--watch <pid>`), whose
lines the case reads from its log once every process has ended (`settle`). galtest.dll does EldenNet's part of the
watch block for them (`tests/galtest/watch_client.cpp`): T1's states and the status, a chained crash filter installed
once the game has started, a crash on the launcher's request, and en-core's slice stepped on its snapshots, whose
phase, level and gate it reports.

EldenNet's DLL (part 6) joined it with the `launcher-eldennet*` cases (clang-cl): each installs `eldennet.dll` as a
player would, in `{case}/eldennet/` with gal/ beside it (the derivation's, as `make_gal` writes it, and `leads.toml`),
and gives it an %APPDATA% of the case's own through `ELDENNET_APPDATA` (the launcher's `--appdata`), with the case's
settings.toml; the DLL's log (`ELDENNET` lines) joins the case's lines. They check: injected suspended (T1 from Pin to
Ready, the core in the world with the gate open, the dev console's status, a Mark recorded, a recording that
`eldennet-engine lint --recording` passes with no name in it, a clean end) and at the title (Dormant there); the
session gate closing with the badge when the game goes online; a build the DLL's own gal/ does not know (its banner,
no task); a simulated anti-cheat module (a copy of vehprobe.dll named `EasyAntiCheat_EOS.dll`, loaded with `--also`);
a second copy loaded by the game's own loader, idle behind R16's mutex and named by the first at Ready; a crash and a
stack overflow on the game thread recorded through the DLL's own filter; the watcher started by the DLL beside it under another loader; and
R1 in the probe build (`probe/eldennet.dll`, the leads' task base replaced by a marker as the real game's is): its two
reports lint clean, every check confirmed, one call at the Mark with the registers and group `--describe` gives, the
slot counts (attach once, run every frame, query every 64th), the bytes of our object the hub wrote, the census's base
class with the run slot, the hinstance's and the statics' slots, the rising play time (with loading screens for it to
stand still in), no allocation on the game thread, and the live derivation's singletons and statics. Then R1 as the
owner runs it (`launcher-eldennet-probe`: `--probe` with no settings.toml, invented saves backed up first, the default
settle of 10 s, Pause pressed every second from the title until it is taken; the console's six steps in order, the
reviewed idiom and the layout's group confirmed, the launcher's lint of both reports), and `--probe` with the release
DLL (`launcher-eldennet-probe-release`: refused at Settings, nothing more, no report). The part 9 tools run on what
these cases leave: `rec-stats` and `replay` on `launcher-eldennet`'s recording (the counts as written, and en-core
stepped again on its inputs answering the DLL's own changes), and `spike-report s0a` on `launcher-spike-s0a`'s log
(three early runs passing their rules, each with its load mode, the late load not run).

GraceUI joined it with part 12 (`launcher-eldennet-graceui*`, clang-cl, with a test build of GraceUI, GraceUI's own
`GRACEUI_TEST_EXPORTS` build, whose `GraceUI` folder CMake's `EN_GRACEUI_DIR` names; without it the cases are left out).
Each installs that folder as it is, as GraceUI's build lays it out beside its `bin` (GraceUI D165): `game/GraceUI/` with
`graceui.dll`, `LICENSE.txt`, `NOTICE.txt` and the `graceui.toml` GraceUI's `tools/release/manifest.py` wrote of each
file's size and SHA-256, so the launcher's pre-flight checks GraceUI's own version file, and runs the fake game with
`--d3d12`, under Wine with Mesa's lavapipe (the prefix's set-up gives wined3d its Vulkan renderer, as GraceUI's BUILD.md
does). `launcher-eldennet-graceui`: GraceUI loaded after EldenNet, by EldenNet's own look, on the main path: the
launcher's pre-flight passes; EldenNet connects (the view ready, the Mark's hotkey registered, a world-state provider);
F11 shows the view, whose ground the fake game reads back over most of its rectangle, and F11 again hides it; and the
launcher's console hands its Pause over, so the Pause pressed into the window is a Mark of EldenNet's own, which the
launcher's record takes from the ring. `launcher-eldennet-graceui-before`: GraceUI loaded before EldenNet (the
launcher's `--also`), found by its name, on the banner path of a build the DLL's gal/ does not know: the banner as one
notice under EldenNet's key, in the warning tone with the build's number, and F11's view there too.
`launcher-eldennet-graceui-unbound`: GraceUI loaded before EldenNet, and the fake game, as another mod would, connected
to it at boot with Pause as its own hotkey (`--grace-take-pause`): EldenNet's Mark registers unbound (`mark=1
mark_bound=0`, GraceUI D164), EldenNet never asks for the ring (`marks source=launcher bound=0`), the launcher keeps its
Pause, and the Pause pressed is the launcher's own Mark (`mark n=1` with no source), which EldenNet's recorder reads
from the ring. (Loading GraceUI from the fake game's main thread at boot instead hung under Wine 9 in the load of
`dxgi.dll`, which `graceui.dll` imports, while eldennet.dll's threads ran; the launcher's `--also` loads it into the
suspended process.) `launcher-eldennet-graceui-anticheat`: the simulated anti-cheat module beside GraceUI: GraceUI's own
check puts it in the notice-only mode, and EldenNet's banner is a notice with nothing else (no panel, no provider).

The derivation's offline round (part 3) joined it so: `probe-exe`, `probe-exe-slots` and `probe-exe-lint` run the
Windows `eldennet-engine.exe` under Wine on `fakegame.exe` with `derive-leads.toml` (the fake game's own names under our
labels) and check derive.json against `--describe`: the fingerprint (the product name matching without its sign), the
FD4 idiom (byte for byte), the singletons and the decoys' verdicts, where each site's path was found and what R0's rule
made of the same hits, the group table's stride, entries and index field, the registration with its argument registers and call sites, the group's index, the RTTI sets and
task classes (clang-cl), and, given the AOB statics' slots as R1 would find them, a pattern at each; then the lint's
refusals of a path, a free string and a long byte run.
