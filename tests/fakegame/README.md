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
| Two accessors of other shapes, never called: the save ledger's with an instruction between the load of its static and the test, the run warden's comparing its static with zero where it lies; and, with `--field-mirror`, a second word of the data that holds the field plot's address | The accessor as the compiler writes it otherwise (Lodestar's variants, `gal/recipes.toml`), and a manager's address kept twice: two slots, one object |
| The sky clock's hour of the day as a fraction beside its packed word; with `--pass-time` the clock jumps twice in the world (five hours 14 s in, three hours 26 s in); with `--flag-set` one flag of the store's fourth block is set 30 s in | What the read-only round looks for in step L6: a clock told by its jumps when time is let pass, and a flag's bit told by its flipping once (the watch's rises and jumps, the sweep) |
| With `--day-book`, a plain block in the first decoy static's place that keeps the clock's packed word twice (0x10 and 0x18 bytes in); `--clock-rate N` sets the game seconds a second of real time (60 unless asked) | A clock behind a static no lead names, as in the game: the read-only round finds it by what the object holds (minutes that go round twice with hours beside them) and names the static the world's clock by the invariant recipe |
| With `--walk-from-s N` and `--walk-to-s N` the player moves only between those seconds of the world (always, unless asked); with `--hit-at-s N` and `--heal-at-s N` it loses 200 of its health and has it back; an actor's parts block holds eleven further addresses of classed objects | What the read-only round's parts watch looks for: a position told by its moving while the tester walks and standing while they stand, health by a hit and a heal, and the holder of parts by the classed objects it points at |
| With `--slot-by-state` the run warden's save slot reads -1 outside the world and 2 in it (with `--first-slot N`, N in the world before the game has gone back to its title: another save loaded first); with `--retitle-at-s N` (and `--retitle-for-s N`) the game goes back to its title once, N seconds into the world, and loads the world again | The game's state, as our notes give it: a field that tells the title from a loaded save. The read-only round finds its static by that field over four passes (title, world, title, world) marked by the launcher's tasks. With `--slot-copy` the field plot keeps a copy of the slot's number while in the world: a second object that reads both numbers, which the class the leads give for the game's state tells from the first. With `--screen-from-s`, `--screen-to-s` the characters near pass on and off the screen between those seconds of the world, two seconds on and two off, each in its turn, and with `--kill-at-s` the first of them dies (its health to nothing, its dead bit set): the character survey's world (the launcher's `--read-plan 1`) |
| The frame loop on the main thread: ten groups (`Fg_FrameOpen` … `Fg_FrameClose`) run in order every frame at `--fps`, with `--hitch-every`/`--hitch-ms` and `--stall-at`/`--stall-ms` | The game thread and its task groups; `Fg_ActorSettle` (index 5) stands for the group where positions are final (`ChrIns_PostPhysics`) |
| The group names in a static table of 16-byte entries `{ index, flags, name }` | A table of groups wider than a pointer (R0's review), found at its stride |
| The hub's live tasks in a table its instance points at (`TaskLoomFields.live`, +0x28) | A game's hub holding its task lists, which lead to the tasks' classes and their base (part 6) |
| `fg_register_task`, found by an interior pattern at entry + 7 inside a `.pdata` function, with our own ABI: **rcx the task, rdx the hub instance, r8d the group**; the program's four own call sites load the hub from its static into rdx. Since spike.8 a wrapper of the shape R1 run 7 found (WHY§240), answering 1 when the task was kept and 0 when not: it keeps its arguments in callee-saved registers, loads its own singleton (the rack, `FG::TaskRack`) from `fg_static_rack` and tests it, and while it is null makes it with two calls (`fg_rack_alloc`, `fg_rack_build`: a shelf for each of the hub's groups) and stores it there; then, where both ways meet, it tail-jumps to `fg_register_rack` with the rack in rcx, the group in edx, the task in r8 and the hub it was given in r9. That tail function, a leaf with no unwind data, answers 0 past `FG_RACK_BOUND` or where the rack's shelf for the group (at `FG_RACK_SHELVES_OFF` + group*8) is null, and else (since spike.9, the shape R1 run 8 found, WHY§248) moves the task into rdx and jumps directly to the group's own function, `fg_shelf_take`, with the shelf (an `FG::Rack::TaskLoomImp`, whose class's innermost name is the hub's) in rcx and the hub still in r9. Since spike.10 (the shape R1 run 9 found, WHY§289) that function reads nothing of the task but its null test: it loads the ledger, a second singleton (`FG::TaskLedgerImp`, G, from `fg_static_ledger`; while that is null `fg_ledger_missing` makes it), puts the shelf's word (`FG_SHELF_WORD_OFF`, its group's own index) in r8 and tail-jumps through an 8-byte stub that zeroes r9 (`fg_ledger_stub`) to G's method, `fg_ledger_admit`, with G in rcx and the task in rdx. Since spike.11 that method has run 10's shape (below): it builds a request on its stack and calls `fg_request_now` with it, which calls `fg_ledger_note` (an `FG::TaskNote` in the next of the hub's rows) and `fg_ledger_keep`, which calls `fg_register_keep` with the hub in rcx, the group in edx and **the task in r8**; it answers 1. The program's four call sites pass their tasks in four ways a game's might: loaded from the rows it is given, the bench's member (`FG_BENCH_TASK_OFF`), an allocator's result (`fg_task_make`), loaded; and `fg_unregister_task`, the registration's sibling of the same shape, has two call sites that never run. `fg_register_keep` compares the group with a fixed bound (an immediate, `FG_GROUP_BOUND`) and the hub's count, tests the task for null, keeps it and its vtable in the hub's `last_task` and `last_vtable` fields, and queues it | `register_task` and its `register_abi` (ARCH §6.2, §6.4), and what R1's callee summary must read from its code (spike.6), from its callees with the arguments followed in (spike.7), and from its tail function's table (spike.8). Bad arguments fault in the caller's thread; `--register fault` faults on request, `--register ignore` never runs the task |
| The hub's table of each active task's run slot (`TaskLoomFields.run_fns`, +0x40) | A hub caching the function it calls every frame: a registered task's run slot is there (spike.7) |
| A task object: a vtable pointer, then 0x20 bytes the hub owns (the group, state, hub and run count it writes, and since spike.9 a kind the game's own tasks hold from when they are made), so `object_size` is 0x20; three slots: 0 attach (once, on the game thread), **1 run (`self`, `FrameInfo*`) every frame**, 2 query (every 64th frame) | The task object's shape the derivation must find (`vtable_slots`, `run_slot`, `object_size`) |
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
| `--crash av-read\|av-write\|av-exec\|div-zero\|illegal\|stack-overflow\|breakpoint\|fastfail\|terminate` at `--crash-at`, on the game thread or a worker; `--game-filter`, `--game-filter-at`, `--game-filter-result` | A game that crashes, and one that installs (or replaces) its own unhandled-exception filter (S0-D); a stack overflow's recursion (frames of 16 KiB) starts 256 KiB from the stack's end whatever called it, so Wine always finds room to raise it |
| `--handled-every` with `--handled-kind seh\|cxx` | Exceptions a game raises and handles itself, which a vectored handler sees first |
| `--load [boot\|window\|ready\|title\|world:]PATH` | A loader that loads a DLL at one moment (ARCH §3.3's late load) |
| `--selftest-task` (`--selftest-abi`, `--selftest-object-size`, `--selftest-group`) | EldenNet's T1 and trampoline, in miniature: a task registered from another thread, its slots counted |
| Since spike.11 (R1 run 10's shape, WHY§293) G's method, `fg_ledger_admit`, answers 0 for a null task or a ledger not open (`FG_LEDGER_OPEN_OFF`), else builds a 32-byte request on its stack (`FG::TaskRequest`, the compiler's vtable, at [rsp + `FG_REQUEST_DISP`]: the task, G's manager, the word, r9's byte) and calls `fg_request_now` with its address, which notes the task and hands it to the work. The request's slots are 5-byte jumps to `fg_request_now`, `fg_request_cancel` and `fg_request_word`. G's manager holds three tasks of the game's own (`FG::SweepChore`, `FG::TallyChore`, of one base, `FG::Chore`), and G's queue (`FG::TaskQueueImp`: a lock, and a list of requests waiting) two requests of the game's own whose flag 1 keeps them there for ever; the ledger's objects lie in pages of their own | What run 10 found: G's method wraps the task in a request and runs it; the game's own requests and tasks to be found in memory |
| Since spike.12 (R1 run 11's shape, WHY§296) the hand-over is G's manager's: `fg_request_now`, after the note, with a word of 0 calls `fg_map_take` for each bin of the manager's list (`FG_MANAGER_LIST_OFF`: begin, end, capacity; one bin, group 0's), and with any other walks the manager's map from its head (`FG_MANAGER_MAP_OFF`, its count beside it; a tree of our own making with MSVC's node shape: left, parent, right, the nil byte at `FG_NODE_NIL_OFF`, a 4-byte key at `FG_NODE_KEY_OFF`, the value at `FG_NODE_VALUE_OFF`) and on a match calls `fg_map_take` with the node's bin (`FG::TaskBin`; the world's, `FG::WorldBin`), the task, the word and the manager; `fg_map_take` counts the task in the bin and hands it to the work with the bin's group. No match drops the task (`map missed=<word>`). Where the request's flag is set (the game's own lingering requests'), `fg_task_other` with the task, then the task's slot 1. The map holds every group's word but 0 and the world's own (1, 3, 6, 8, past the shift), which the world's first load adds (`map world_words=`) | What run 11 found: the request looks the group's word up in the manager's map and gives the task to what it finds there; at the title the word may be missing |
| The hub's desk at +0x3B0, its index at +0x3D0, the index's four pointers 32 bytes apart from +200 into rows 240 bytes apart (`FG_ROW_BYTES`); each of G's notes an `FG::TaskNote` there, a fresh record in place of a pointer to a static word, stamped with the game thread's id `FG_NOTE_STAMP_MS` later | The array behind CSTask's +0x3B0 and +0x3D0 that changed after the call in R1 runs 7 to 9, seen as four views by the walk (spike.10) |
| `--press-pause-at F`: Pause pressed and released (`SendInput`) at frame F | The player pressing the launcher's Mark, a system hotkey (O90) |
| `--d3d12`: a Direct3D 12 device, queue and flip-model swap chain (960 by 540) on the window, cleared and presented every frame; `d3d12.dll` and `dxgi.dll` loaded by name with this option alone | A game's swap chain, which GraceUI's overlay binds and draws on (part 12) |
| `--press-f11-at F,F…`: F11 pressed and released into the window at each frame listed, the cursor in it and the window in front and holding the focus (as GraceUI's harness found Wine needs); with `--d3d12`, `--press-pause-at` presses Pause the same way | The player pressing GraceUI's hotkeys: F11 for EldenNet's debug view, Pause for its Mark |
| `--grace-report F,F…` with `--grace-probe x0,y0,x1,y1,r,g,b` (given up to three times, reported as `probe`, `probe2` and `probe3`): at each frame listed, one `FAKEGAME grace` line of what a test build of GraceUI says through its test exports (`graceui_test.h`, read through `GetProcAddress`, never linked): the host's state, the mods connected and EldenNet's caps, its notices under keys 1 and 2 (spaces written as `_`), the panels made, shown and drawn, the lists, the textures, the world-state providers and their flags, the window's filter; where the test build has them, the menu (open, the page open, the element focused, its refusals and, from GraceUI 0.2.33, whether its focus ring is drawn: `menu_ring`, G42) and the browser view's server (running, its port, the spaces, the commands pending); and, with `--d3d12`, the pixels within 3 of each probe's colour in its rectangle of the frame just presented, read back after GraceUI drew on it | GraceUI's harness reports, from inside the game's process |
| `--press-keys F:KEY,…`: one of GraceUI's menu keys (`f5`, `next`, `prior`, `up`, `down`, `left`, `right`, `return`, `escape`) pressed and released into the window at each frame listed, as F11 is | The player in GraceUI's menu (F5, D167, D171) |
| `--grace-take-pause`: at boot, before the window, the GraceUI already loaded (the launcher's `--also`) found by its name through graceui-sys, a connection as `fakegame-other` with the `hotkeys` cap, and Pause registered as its hotkey; one `FAKEGAME grace_other found=… connect=… hotkey=… id=…` line | Another mod that took Pause in GraceUI first, so that EldenNet's Mark registers unbound (GraceUI D164) |
| `--grace-lan`: at boot, the GraceUI already loaded (the launcher's `--also`) given 127.0.0.2 alone as its home listener's address (its test build's `GraceUI_Test_SetNetworkAddresses`), before any mod registers pages; one `FAKEGAME grace_lan found=… set=…` line; each `--grace-report` then names the pairing sheet's code while one shows (`pairing=`) | A PC on a home network, for a device paired at home (GraceUI's G9), never a real address of the machine |
| `--grace-copy-at F,F…` (0: at boot, before the window), with a second copy of GraceUI that the launcher's `--also` loaded from a folder other than the game's `GraceUI`: at each frame listed, the first copy loaded that is not `GraceUI\graceui.dll` asked for ABI 1's table (`GraceUI_RequestApi`), then one connection as `fakegame-other` to the table that answered, else to the GraceUI graceui-sys finds; one `FAKEGAME grace_copy name=… table=… look=own\|name\|scan connect=…` line each. Where more than one copy is loaded, each `--grace-report` adds a `grace_copies` line: each copy in load order (`c1`, `c2`: its name relative to the game's folder, its host state, init threads, hook installs, hooks, mods connected and copies named, and, from GraceUI 0.2.35, where its hooks lead in `GraceUI_Test_GetHookName`'s order: how many it answers, how many lead into its own detour and module, the jumps each followed and which is the window procedure's: `leads`, `leads_here`, `jumps`, `window_procedure`) and the reported copy's notices and kept log lines that name another copy; the report reads the copy named `graceui.dll` unless it idles, and then the first copy that does not | Another mod that carries a private copy of GraceUI (GraceUI's D72, D83 and invariant 18) |
| `--pad-chord-at F,F…`: from each frame listed, one read of pad 0 a frame through a test build's XInput filter (`GraceUI_Test_PadRead`): D-pad Up alone for 250 ms, then with View (XInput's BACK) for 650 ms, then neither; one `FAKEGAME pad_chord frame=… reads=… view_reads=… up_seen=… view_seen=… packets=…` line of what the game was given | The player pressing the chat box's pad chord (GraceUI D204), as a game reads XInput once a frame; Wine has no pad |
| `--lock-on-at F[,G]`: in the world the player locked on to the first NPC from frame F (until frame G); one `lock_on` line at each change | The player locking on to a target, which EldenNet's world-state provider claims as danger in v0.1 |
| The preview options, below (`--backdrop`, `--swap-*`, `--shot-at`, `--cursor-once`) | A game's own picture |
| `vehprobe.dll`: `VEHPROBE_MODE=pass\|swallow\|off`, `VEHPROBE_FIRST`, `VEHPROBE_EXIT`, `VEHPROBE_LOG` | A crash reporter's vectored handler that passes faults on, or ends the process at the first (S0-D, ARCH §6.5) |

The version resource is invented too: FileVersion 3.14.15.0 (the fourth field 0: "ww"), ProductName
`EldenNet Fake Game` followed by a trademark sign (U+2122), which the derivation and the gate compare without, and
translation 0x0809 and 1200.

## Preview shots

`tools/preview/shoot.py` takes screenshots of EldenNet in GraceUI with these options, which change nothing when they
are absent. All but the last need `--d3d12`.

- `--backdrop`: a backdrop of our own copied into the back buffer each frame in place of the flat clear, standing in
  for a game's own picture: a dark vertical gradient (slate at the top, near black at the foot) with one soft warm
  light above and right of the centre, computed for each pixel when the swap chain is made (no image file, nothing of
  a game's), with a 4 by 4 ordered dither against banding.
- `--swap-size WxH` (each from 64, at most 3840 by 2160) and `--swap-format 28|24`: the swap chain at that size and
  DXGI format (24 is `R10G10B10A2_UNORM`; unless asked, 960 by 540 and 28, `R8G8B8A8_UNORM`). The window keeps its
  own size and the frame is stretched to it; GraceUI lays out by the swap chain's size. The `d3d12` line names the
  format and whether the backdrop is on, and `--grace-probe` reads a 10-bit frame's channels taken to 8 as below.
- `--shot-at F,F…` with `--shot-dir DIR`: the frame as presented at each frame listed, read back once GraceUI has
  drawn on it, written to `DIR\frame-F.bmp` (24 bits a pixel, bottom-up, as GraceUI's harness writes its shots; a
  10-bit format's channels taken to the nearest of 256 levels), with one `shot frame=… ok=… width=… height=… format=…`
  line and never the folder's path.
- `--cursor-once`: the cursor put in the window at the first key press alone, as GraceUI's harness puts it once.
  Under Wine the cursor put there again before each press reached GraceUI as a mouse move and put its focus ring away
  again after the key had shown it (G42), so a shot of the ring needs it.

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
(`EN_GALTEST_FAULT`) and the ReadProcessMemory copy (`EN_GALTEST_COPY=rpm`). With `EN_GALTEST_FRAME=own` (the
`gal-own-*` cases) it does instead what EldenNet v1's start does since O101: a reader thread of its own steps the task,
ten snapshots a second with steady reading, and nothing is registered with the fake game or called in it; those cases
check the same truth, and that the hub ran no task of ours and none of our slots was called. Each `gal-*` case first makes gal/ as the
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
the read-only round in the probe build (`probe/eldennet.dll`; Lodestar's step L5), with gal/'s leads relabelled to the
catalogue's and the repository's `ids.toml` beside them: from settings.toml (`launcher-eldennet-read`: a first pass
before the fake game has made its managers, a later one that names all eleven by their instances' classes, the
reader's trial on them from a thread of our own, a lint-clean report of numbers and a draft table, and no call into
the game: the fake hub sees no task of ours), as the owner runs it (`launcher-eldennet-read-owner`: `--read` with no
settings.toml, invented saves backed up first, the console's steps in plain words with no key to press, both files
checked clean by the launcher when the game ends), and `--read` with the release DLL
(`launcher-eldennet-read-release`: refused at Settings, nothing read and nothing called), and with the launcher's
console blocked after three lines (`launcher-eldennet-read-console`: the watch loop, the round's steps, the lint and
the end go on in launcher.log). R1's cases, the round with
one call into the game, went with it (retired 2026-11-22, O106); the parts of the fake game shaped after its runs (the
rack and its shelves, G's ledger, queue and map, and the desk's views) stay in the program, where the `register*` and
`gal-*` cases still register through them, with no case of their own; the options only R1's cases passed
(`--table-gap`, `--task-gate`, `--task-queue`, `--map-late`, `--press-pause-every`) went, with the request's queued
path and its check, which only they reached. `launcher-crash-console` crashes
the game while the console is blocked, and the crash watcher still keeps its record. The part 9 tools run on what
these cases leave: `rec-stats` and `replay` on `launcher-eldennet`'s recording (the counts as written, and en-core
stepped again on its inputs answering the DLL's own changes), and `spike-report s0a` on `launcher-spike-s0a`'s log
(three early runs passing their rules, each with its load mode, the late load not run).

The S0 spikes on the table-based start (2026-11-23; roadmap S10.1.b) have cases of their own with the probe build:
`launcher-eldennet-s0a`, `-s0b`, `-s0c`, `-s0d` and `-s0d-crash` give the fake game's gal/ a build's table (the
harness's `s0_table`: schema 2, each of the fifteen statics under the catalogue's label, its place and the places in
the code that load it, from `--describe`, with the repository's `ids.toml`), so a start takes each id from the table
and checks it and searches for none. Each checks eldennet.dll's report of each run in the run's folder (linted, no
path), the table checked at the start and again with none refused or changed, no call into the game, the launcher's
plain words on the console and its tally, and `spike-report` over the spikes and recordings folders: each spike whole,
and, in the `-nostatic` five, with `game_state` out of the table and of patterns.toml as the real game's layout is
today, the start at the discovery banner, S0-B waiting and nothing failed. The S0 package's own cases,
`launcher-package-s0-*`, run its unzipped files with `--set pkg` (BUILD.md). The observer's runs
(`launcher-eldennet-spike-*`, and `launcher-package-spikes-*` with their package) check the tester's panel too: the
run's tasks, their confirmations and Marks, and `launcher-run.json` in the run's folder.

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
`launcher-eldennet-graceui-events` (S10.2.d): Augur's events in F11's view. The fake game's world (its place, the boss
bar, the talk popup at frames 600 to 659) and the lifecycle raise Augur's events, and the view, opened at frame 720,
lists them, the newest first, each line after a 6 by 6 square of its source's colour; two more probes count the world's
green squares and the lifecycle's blue ones in the view's mark column (whole squares of 36 pixels, both colours there,
at most the list's ten, none before F11 or after it again), and the DLL's log gives the events the view opened with and,
every 10 s, how many there have been. `launcher-eldennet-graceui-pages` (S11.4.a): EldenNet's space registered at the
connect (`pages=0`) and served, at the title screen (in the world the fake player reads as in danger, which keeps
GraceUI's menu shut): `--press-keys` opens F5's menu on EldenNet's Home, goes to System › Status and Logs, follows Log
detail's shortcut row to its home on Settings › Developer and changes it there (a command EldenNet answers, applied at
once), goes back a page to Tools › Bug report, then on to Settings › Developer, Docs and Content packs at the rail's
foot, and closes the menu; the report gives the page and the focus at each step. Meanwhile the case's `during` hook, a
browser of the tests' own (`tests/pages/browser.py`) on Linux, joins GraceUI's server at the port the report names (a
page load's cookie, a ticket, a WebSocket), reads each of EldenNet's topics (the lamp and Home's card, Problems,
System's Health, parts and read groups, Compatibility, the logs stream from its first line, the settings; Content
packs': the shipped cast, an overrides folder of the case's own by relative paths, its disable.toml loaded and turning
the test persona off and a folder nested too deep left out, and the start's load; Docs' list of ten documents, About
with the versions and the build's label filled in, and Troubleshooting whole; and Bug report's preview) and sends
commands: one accepted, one value refused, a page's reset refused without its confirmation; Bug report's words of the
case's own, which then lead its preview and are in no other topic's frame, no log or report line, no recording and no
file of the run; and the log's part switched off, an always part's switch refused. settings.toml then holds both
changes, written once by T11. The banner path's case also checks that EldenNet's space is registered for the game's view
alone there, so GraceUI's server does not start for it.

v0.1's coexistence check (S10.3.d) and the chat box's last parts against GraceUI's test build (S11.4.c) have four
cases. `launcher-eldennet-graceui-second-copy` and `-second-copy-first`: a second copy of the same test build in
another mod's folder beside the game (`OtherMod\GraceUI\`), loaded by the launcher's `--also` after the game's own, and
asked for its table by the fake game standing in for that mod (`--grace-copy-at`), which then connects to the GraceUI
that answers; on the banner path, as `-before`. Asked after EldenNet (the game's order), that copy answers null each
time, idles with no thread and no hook, and is named once in GraceUI's own notice and its log, while EldenNet stays on
the game's copy with its banner and F11's view; asked at boot before EldenNet, that copy is the active one, the game's
copy idles once EldenNet asks it (its notice, from GraceUI 0.2.38's D244, naming the copy in use as the one to remove),
and EldenNet's look finds the active one. Which way graceui-sys found it (by name or
by its scan) is left open, since Windows may answer `GetModuleHandleW` with another copy than Wine does; the mods each
copy holds tell that EldenNet is on the active one. Both check that the active copy's hooks stay as they were and the
idle copy has none, and, by GraceUI's `GraceUI_Test_GetHookLead` (0.2.35, D240), that at every report each hook of the
active copy leads into its own detour in its own module, MinHook's two jumps for each but the window procedure's filter,
which takes none ("2,2,2,2,2,0" under Wine: the render half's five and the window procedure, the fake game loading no
DirectInput or XInput in these runs), and that the idle copy answers no hook: one hook holder in the process (GraceUI's
invariant 16). With the other mod's connection, they are also two mods connected each way round.
`launcher-eldennet-graceui-talk-chord`: the talk hotkey's pad chord read through GraceUI's XInput filter
(`--pad-chord-at`) opens the chat box with no key pressed, by the chord (`via=pad_chord`, GraceUI 0.2.38's
`hotkey_by`), the game seeing the anchor and never View; a line typed and sent, none of it the game's; Esc closes the
box. `launcher-eldennet-graceui-talk-danger`: the box open with a draft,
the player locks on (`--lock-on-at`), which EldenNet claims as danger, and GraceUI ends the box's holder at once
(danger, which EldenNet takes as input lost and keeps the draft, while its own conversation's rules do not count a
lock-on); T pressed during the lock-on reopens the box in EldenNet's core, GraceUI answers not now (danger) and the
conversation ends with no line said, the draft still kept (WHY§586); nothing typed reaches the game, and once the
lock-on ends T gives the draft back, which is sent.

The derivation's offline round (part 3) joined it so: `probe-exe`, `probe-exe-slots` and `probe-exe-lint` run the
Windows `eldennet-engine.exe` under Wine on `fakegame.exe` with `derive-leads.toml` (the fake game's own names under our
labels) and check derive.json against `--describe`: the fingerprint (the product name matching without its sign), the
FD4 idiom (byte for byte), the singletons and the decoys' verdicts, where each site's path was found and what R0's rule
made of the same hits, the group table's stride, entries and index field, the registration with its argument registers and call sites, the group's index, the RTTI sets and
task classes (clang-cl), and, given the AOB statics' slots as an in-game round finds them, a pattern at each; then the
lint's refusals of a path, a free string and a long byte run.
