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
| `fg_register_task`, found by an interior pattern at entry + 7 inside a `.pdata` function, with our own ABI: **rcx the task, rdx the hub instance, r8d the group**; the program's four own call sites load the hub from its static into rdx | `register_task` and its `register_abi` (ARCH §6.2, §6.4). Bad arguments fault in the caller's thread; `--register fault` faults on request, `--register ignore` never runs the task |
| A task object: a vtable pointer, then 0x18 bytes the hub writes (group, state, hub, a run count), so `object_size` is 0x18; three slots: 0 attach (once, on the game thread), **1 run (`self`, `FrameInfo*`) every frame**, 2 query (every 64th frame) | The task object's shape the derivation must find (`vtable_slots`, `run_slot`, `object_size`) |
| Seventeen accessors with the FD4 shape (`mov rbx,[rip+static]; test; jnz; lea rcx,[rip+class]; call getter; mov rdx,rax; mov r8d,site; lea rcx,[rip+path]; call log`), the path ending in `FD4Singleton.h`, the class object's name at +0x18 | The FD4 singleton idiom (ARCH §6.2): eleven names (`TaskLoom` for CSTask's role, `ActorRoster` for WorldChrMan's, and so on; `--describe` maps each), two of them with a second site, and three decoys: `Duplicata` (two statics: ambiguous), `Strayling` (path ends in `.hpp`), `Rockbound` (its static is in `.rdata`) |
| Four AOB functions, each with a distinct shape referring to a static at entry + 5 or later | `global_hinstance`, `GameDataMan`, `GameMan` and `FieldArea` (ARCH §6.1) |
| C++ classes in the invented namespace `FG` (`FG::Actor`, `FG::PlayerActor`, `FG::NpcActor`, `FG::BeastActor`, `FG::MountActor` with a second base `FG::Saddle`, `FG::Prop`, `FG::Tombstone`, the singletons' `FG::…Imp`) and `Decoy::Actor` | MSVC RTTI for the walk (ARCH §6.2): type descriptors in `.data`, locators in `.rdata`, an actor set with a second vtable and a same-named class elsewhere. The clang-cl build only: MinGW's GCC writes its own RTTI |
| Title, loading and world phases (`--title-frames`, `--load-frames`, `--stay-title`, `--reload-every`), a main player and `--actors` actors with health, positions and a distance-sorted list | The lifecycle and the structural groups' self-tests (ARCH §6.7, §9) |
| `--move-every` with `--move-mode poison\|free\|keep`: every singleton, the ledger's sheet and the main player move; the old place becomes an `FG::Tombstone`, is released, or stays | Statics that move and torn-down objects (ARCH §6.6, R5) |
| `--online`, `--online-at` | The session turning online (the gate) |
| `--crash av-read\|av-write\|av-exec\|div-zero\|illegal\|stack-overflow\|breakpoint\|fastfail\|terminate` at `--crash-at`, on the game thread or a worker; `--game-filter`, `--game-filter-at`, `--game-filter-result` | A game that crashes, and one that installs (or replaces) its own unhandled-exception filter (S0-D) |
| `--handled-every` with `--handled-kind seh\|cxx` | Exceptions a game raises and handles itself, which a vectored handler sees first |
| `--load [boot\|window\|ready\|title\|world:]PATH` | A loader that loads a DLL at one moment (ARCH §3.3's late load) |
| `--selftest-task` (`--selftest-abi`, `--selftest-object-size`, `--selftest-group`) | EldenNet's T1 and trampoline, in miniature: a task registered from another thread, its slots counted |
| `vehprobe.dll`: `VEHPROBE_MODE=pass\|swallow\|off`, `VEHPROBE_FIRST`, `VEHPROBE_EXIT`, `VEHPROBE_LOG` | A crash reporter's vectored handler that passes faults on, or ends the process at the first (S0-D, ARCH §6.5) |

The version resource is invented too: FileVersion 3.14.15.0 (the fourth field 0: "ww"), ProductName
`EldenNet Fake Game`, translation 0x0809 and 1200.

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

The derivation's offline round (part 3) joined it so: `probe-exe`, `probe-exe-slots` and `probe-exe-lint` run the
Windows `eldennet-engine.exe` under Wine on `fakegame.exe` with `derive-leads.toml` (the fake game's own names under our
labels) and check derive.json against `--describe`: the fingerprint, the FD4 idiom (byte for byte), the singletons and
the decoys' verdicts, the registration with its argument registers and call sites, the group's index, the RTTI sets and
task classes (clang-cl), and, given the AOB statics' slots as R1 would find them, a pattern at each; then the lint's
refusals of a path, a free string and a long byte run.
