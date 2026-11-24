# Lodestar's status, 23 November 2026 (fourteen read-only rounds run, the fourteenth spoilt by GraceUI's presence; its rerun built; the far fields built; the characters manager's places in the layout as leads; the save's slot not yet found)

For the next sitting of the Lodestar session, or a new session that takes it over. Read [HANDOVER.md](HANDOVER.md)
first; this file adds only where Lodestar's own work stands.

## Where things stand

| What | Where | State |
|---|---|---|
| Proposal 0005 | `docs/proposals/0005-lodestar.md` | Accepted: O101 (b), O102 (a) and O103 (a), each as recommended |
| L1: the catalogue (`gal/ids.toml`, `ids.hpp`) and the build's table (`layout.hpp`, schema 2) | engine dc18c95 | Done, merged |
| The resolver: placement and witness checks, `Library` (`resolve.hpp`) | engine 440233e | Done on invented images, merged; nothing in the DLL reads through it yet |
| L2: shapes (`shape.hpp`), the accessor as a recipe (`gal/recipes.toml`, `recipes.hpp`), discovery by the shape, `gal/patterns.toml` withdrawn | engine 700d54e, 21c5b02 | Done, merged: `gal/` holds no byte of the game. The fake game's harness passes 105 of 105 with the shape in use |
| L3: the finder (`lodestar.hpp`, `eldennet-engine lodestar find`), report kind `lodestar` schema 2, the `find` package kind | engine 9edf1c0, 4569730 and the commit after | **Done.** The owner ran it twice on 2.7.2.0 (2026-11-22; `docs/ops/probes/2026-11-22-l3.md`): the shape stands at 7,077 places, all accepted, 217 statics to 217 class objects; the pattern's 4 further sites are rightly left out. `gal/recipes.toml` holds the count |
| L4: the reader on a thread of EldenNet's own (`ReaderLoop`, steady reading), a start through the build's table (`Library` in discovery), eldennet.dll's T2 | engine 4741f2a | **Done.** The DLL starts Discover, WaitEngine, Reader, FirstReading, Ready, calls nothing of the game and registers nothing; on the fake game under Wine it reads ten times a second, the hub's tasks stay the program's own and none of our slots is ever called. The watch block's `reader` and `first_reading` states and the `reader` thread kind were appended. `install_task` and the getter remain only for the probe's R1 round, until L5 |
| L5: the read-only round. `lodestar::name_instances`, `merge`, `draft_table` and `Trial` in en-gal; `probe_read.cpp` in the DLL's probe build; the launcher's `--read`; the `read` package kind and `README-READ.txt.in` | engine 1cc712a, e82d795, 0c2a25a, 0fca63e | **Built; the owner's run is to come.** On the fake game under Wine the round names all eleven managers by their instances' classes (none at a first pass made before the game has them, all at a later one), reads them about 340 times in 34 s with no fault, writes a lint-clean report and a draft table of 14 leads, and the fake hub sees no task of ours. `gal/ids.toml` records each manager's and class's older label (`was`), by which a lead finds its id |
| The owner's read-only run (2026-11-22; `docs/ops/probes/2026-11-22-l5.md`) | package 0.1.0-lodestar.3; the table in the engine's `gal/layouts/2.7.2.0-ww.toml` | **Done, reviewed.** Eight of the eleven managers named and placed (`task_hub`, `characters`, `session`, `hud`, `menus`, `loading`, `camera`, `param_store`), the two character classes and the groups' table: 11 entries in the build's table, all leads. The reader read for 19 minutes, 11,552 readings, with no fault of any kind. Not named: `event_flags`, `world_clock`, `messages` |
| The second read-only run and its logs (2026-11-22; the same record) | package 0.1.0-lodestar.5 | **Done, reviewed.** The same eight at the same places; 4,402 readings with no fault. Its logs showed that `messages` was found behind two slots of the data and the report lost it (`merge`), that `event_flags` is behind no slot, and that 97 in 100 of the searches' candidates were refused when asked. No clock found; one strong candidate for the play time |
| The search fixes and the accessor's other shapes | engine 66c8675 | **Built, not yet run by the owner.** What a search finds is kept through later passes; several slots with one object name the manager; candidates are asked for first and refusals counted by reason; the searches share 4 ms of a step; the rising search keeps the best; a member search and a count of a class's objects in the image's data; five variant recipes (`accessor_near`, `accessor_gap`, `accessor_gap_near`, `accessor_compare`, `accessor_compare_near`) on two new forms (`skip_one`, `compare_slot`), counted beside the accessor's own; report schema 3 |
| The third read-only run (2026-11-22; the same record, "The third round in game") | package 0.1.0-lodestar.6; engine 546b085 | **Done, reviewed.** `messages` named (two slots of the data, one object); 12 leads in the build's table; the eight others unchanged a third time; the refused values were no addresses; the reader kept 9.8 readings a second; the variants counted (1,279 places, the same managers again, 2 new statics; `accessor_gap_near` withdrawn). `event_flags` in none of the places looked; `world_clock` has only a class under a longer name |
| The fourth round's build | engine 606167d | **Built, not yet run by the owner** (the owner agreed to the wide scan on 2026-11-22: "Yes, scan it"). Naming by a class under a longer name (`by_related`); the flags' store as a lead on trial (`flag_store`, no id); the member search over 0x4000 bytes; what the two new statics hold; the code's references to a slot as a search-named manager's witnesses; and the object scan (`probe.hpp`'s `ObjectScan`): the game's private, writable memory gone through once for an unnamed lead's table of methods, then up to three times for what holds what was found. Report schema 4 |
| The fourth read-only run (2026-11-22; the same record, "The fourth round in game") | package 0.1.0-lodestar.7; engine f857bfb | **Done, reviewed.** The event flags' store is behind one of the accessor's 217 statics (held from the title on), and no object of the flags' manager's class exists in the 7.7 GiB the scan read; the world's clock is a part of another manager's object (1,024 bytes in, a pointer to it 192 bytes in), with no static of its own; `messages` has its witnesses (the slot 238 instructions refer to). The scan was far slower than estimated (19.75 GiB in 13 minutes, unfinished) and slowed the reader to 7.5 readings a second |
| The fifth read-only run (2026-11-22; the same record, "The fifth round in game") | package 0.1.0-lodestar.8; engine d613651 | **Done, reviewed.** `event_flags` named (the store; 183 sites): ten of eleven managers, 13 leads. 15,878 readings with no fault. The play time's field found. The object of the clock's class does not hold the time. The report passed its size limit after half an hour; the careful reads proved slow (half a millisecond each) |
| R1's clean-up (O106) | engine 3e11238 | **Done, merged.** The first in-game round's code, launcher step, package and cases removed |
| The sixth read-only run (2026-11-22; the same record, "The sixth round in game") | package 0.1.0-lodestar.9; engine 8e7295d | **Done, reviewed.** `save_data` in the table by the invariant recipe: 14 leads. The reader at 9.6 readings a second. No slow clock in any static's object; a fast one not ruled out there. The report lost every watched word (the fallback was too blunt) |
| The seventh read-only run (2026-11-22; the same record, "The seventh round in game") | package 0.1.0-lodestar.10; engine b0a2fa7 | **Done, reviewed.** The time of day found (hour and minute packed into one word behind static 64422792; told by the owner's two passings of time). The flags' blocks and tree have candidates. The reader at 9.97 readings a second. The sweep's list missed what the owner did; the play time's invariant was refused by its margin rule |
| The eighth read-only run, with the tester's panel (2026-11-22/23; the same record, "The eighth round in game"; roadmap S10.1.l) | package 0.1.0-lodestar.11; engine 3030114, the table at a6cdf26 | **Done, reviewed.** The panel worked on the owner's PC. `world_clock` in the table by the invariant recipe: 15 leads. Bits set in the flags' stretch around both confirmations. The reader at 9.98 readings a second |
| The ninth read-only run (2026-11-23; the same record, "The ninth round in game") | package 0.1.0-lodestar.12; engine 620dcd7 | **Done, reviewed.** The tree beside the flags read whole (11,930 records): key, mode and block index placed; a block is 125 bytes at the holder plus index times 125; the flips at the owner's marks decode to round flag numbers. The lead's "offset" is an index. The panel's button change ran |
| The tenth read-only run, and the reader's correction (2026-11-23; the same record, "The tenth round in game") | package 0.1.0-lodestar.13; engine 6d56207, the flags' fields at e864140 | **Done, reviewed.** The reader's way down the tree arrived at the record of every joined flip; the store keeps the divisor 28 bytes in; the flags' fields are in the layout as leads. L6's three subjects are placed |
| The eleventh read-only run: EldenNet's own reader on the flags (2026-11-23; the same record, "The eleventh round in game") | package 0.1.0-lodestar.14; engine 998d078 | **Done, reviewed.** `discover` through the build's layout works on the game (a partial pass, 19 ms, 12 items from the table, none refused); `ErBackend::flags_alone` read the flag set in every playthrough as set, 44 frames, no fault. The marked flags were not compared: the tree's walk was 47 records short when the owner quit |
| The twelfth round's build: the game's state by what it holds at the title and in the world (roadmap S10.1.m) | package 0.1.0-lodestar.16; engine 082127f | **Done, reviewed** (the record's "The twelfth round in game"). The backend read the four flags the owner changed, and the always-set one, as set: five of five agree. The state search ran clean (four looks, none overrun) and hit 15 slots, so it named none; by the owner's word (the character is third in the list of saves, so slot 2) one hit alone fits: static 64543992, the 32-bit field 1,136 bytes in (LIKELY). Its candidates were only the 1,397 slots live at the title |
| The thirteenth read-only run: the game's state by two characters, and the character search (roadmap S10.1.m; the record's "The thirteenth round in game") | package 0.1.0-lodestar.17; engine 63122b6 | **Done, reviewed.** In the characters manager (absent at the title; its object over 127 KiB): the main player's word at 0x1E508, the near list at 0x1F1E8 (16-byte elements, the address first, a fraction at 8), two lists of addresses at 0x1F1C8 and 0x1F218, the player set at 0x10EF8. In the player's object: the holder of parts at 0x190, the character's sheet at 0x580 (the class the save data points at 8 and 16 bytes in). All LIKELY, none in `gal/`: the manager's places lie past the layout's 0x4000 cap (Engine's to rule). The game's state: the candidate read 4 and 2 for the list's first and third character; the owner's save file shows those are slots 0 and 2, so the candidate is not the slot; the round named nothing |
| The fourteenth round's build: the save's slot by two numbers, the parts watch (roadmap S10.1.m) | package 0.1.0-lodestar.18; engine 6d10b11 | **Built, not yet run by the owner.** `PairSearch` (the field that reads 6 then 2), the character search's one look in the second world, `PartsWatch` (position by walking, health by a hit and a heal); fourteen tasks, eight marks |
| The layout's bit-run type, and the clock's and the play time's fields | engine a5efb98 | **Done.** `bits{word, shift, width}` in en-gal's layout (ARCH §6.3's dated note); `world_clock.hour`, `world_clock.minute` and `save_data.play_time` in the build's layout as leads |
| Digests | — | Moved to L7 (WHY§311): v1 calls no function of the game |
| The text for ARCH and DESIGN (proposal §15) | `docs/lodestar` | Written as dated notes and changed rows: ARCH §1.2, §3.2, §6.2, §6.3, §6.4, §6.10; DESIGN §2.1, §4.2, §16.1 |
| The design document, with a code for `wp` | `docs/lodestar` | Not started |

Engine holds the numbers: O101 to O103; Lodestar's drafts are placed up to WHY§332 (next free: 333, before the
three sent with the second round); S10.1.k carries the steps. Only `design/v1-proposal` and `feat/engine` go into `main`; Lodestar's branches reach it through them.

## The package and the sweep

Install the sweep from Skyrizz's working branch, `feat/skyrizz-lite`, not from its default branch: check that branch
out in the Skyrizz checkout, then `python3 -I tools/privsweep/privsweep.py install`. The sweep's code is the same on
both, but only the working branch's `allow.txt` has the reviewed lines the packages need, among them the one for
authors' addresses inside a file named `THIRD-PARTY-NOTICES.txt` (Skyrizz c68a948).

Lodestar's first install came from the default branch, so its sweep was stricter than the others' and refused
`EldenNet-Find.zip` for toml++'s author's address in its licence notice. Before Engine's answer arrived, the owner
chose "Allow that one address" in Lodestar's chat, and Lodestar committed a line for it on this session's own branch
in Skyrizz, `claude/serene-tesla-aan6oc` (cece89f). **That commit is redundant and should not be merged**: the
working branch already covers the case. The package (SHA-256
75989ada93cae98b70515d374d3cba19ff3e42c1af6a3f4952d488e442b5bdb5, built from engine 9edf1c0) passes the working
branch's sweep as it is, and **was sent to the owner on 2026-11-21**. Everything pushed and sent before had passed the
stricter install. To rebuild the package: BUILD.md, "The spike package", the `find` kind.

## What comes next, in order

1. **The fifteenth round is reviewed (the record's "The fifteenth round in game"); the game's exit code is open.**
   Package 0.1.0-lodestar.19 ran with GraceUI installed and not loaded; the marks were the tasks' own. Settled:
   the save's slot is the 32-bit field 0xAC0 into the object behind the static 0x3D709A8 (the pair search hit it
   in game, 6 then 2: the executable's lead confirmed); health is part 0 of the holder (0x190 of the player), field
   0x138, its maximum 0x13C (leads for `vitals`); position candidates in parts 13 and 4. Not settled: the lists
   (the search still takes element pairs for bounds), the falls' list (part 0 used it up).
   **The game's exit code is settled, and not ours**: the game ended with 0xC0000374 at the quit without GraceUI
   in the process, and the owner's control run with nothing injected (Engine's, WHY§556) ended the same way once
   a save was loaded. It is the game's own on that PC since 2026-11-23; the fault is in the game's teardown of its
   file system (the record has the reading of Engine's stack from the executable). The rounds go on; the README
   says the code will show.
   **The sixteenth round is reviewed (the record's "The sixteenth round in game").** It named the game's state by
   its own checks (the one of 22 fields in an object of the lead's class: 0x3D709A8 + 0xAC0), found the near list
   at the layout's place (119 elements; no fraction in every element this time), health at part 0 + 0x138 again
   (nine other fields behaved alike), and the position in part 13 from 0x70 (x, height, z). The layout now holds,
   as leads: `[item.game_state]`, `game_state.save_slot` (far), `character.parts` 0x190, `character_parts.vitals`
   0x0 and `.motion` 0x68, `vitals.hp` 0x138 and `.max_hp` 0x13C, `motion.pos` 0x70. The game's exit code at the
   quit is the game's own on the owner's PC (WHY§557 to §562); the launcher keeps it quietly (O115).
   **The seventeenth round is reviewed (the record's "The seventeenth round in game"): EldenNet's own reader read
   the world's leads right.** Through `ErBackend::world_alone` and the build's layout, 323 frames, no fault: the
   slot 6 then 2 (it keeps its number at the title: the main player's presence tells the title, the slot does
   not); the player's class at the main player's word; the health under its maximum after the hit and equal to
   it after the heal; the position moving with the walk alone; the near list sorted by distance, a character in
   every element, 8 of 12,720 elements without a distance yet. The game's state's item passed the table's checks
   in the game. The owner checked the maximum HP against the status screen: it is the number the reader read.
   **Next (WHY§460's order, and what a ready start still lacks)**: the rest of the character's fields the world's
   and the player's groups need (handle, the ids, team, the dead and on-screen bits, the block), the save data's
   deaths and the sheet's level and name, the place (field_area), the menus' screen and the session. The
   executable chooses what each round checks; the reader then reads it, as in this round. Engine is changing
   `read_world()` to walk the first 48 of a longer near list.
   **What the plan for it was:** A backend entry that reads the world's
   group alone (as `flags_alone` does the flags; `ErBackend` is Engine's to hear of first), so that a round reads
   through the layout: the game's state's slot, the main player, its parts, health and position, and the near
   list, and reports numbers the owner can check on screen (health and its maximum; the slot) and whether the
   position moves with the walk. With it: the near entry's fraction counted over the elements, not required in
   all; the character search's look shortened (71 s in the sixteenth round). Then WHY§460's next subjects.
   Read the other sessions' messages before a package goes out (a correction of Engine's lay unread once).
   **Leads from the owner's executable (the record's "The offline study of the owner's executable")**: the game's
   state's static is 0x3D709A8 (its class's table of methods 0x2A84210; made with the event flags' store, so not
   there at the title), the save's slot the 32-bit number 0xAC0 into its object (compared with -1 and with the
   count of slots in the code); the main player's word 0x1E508 is read by 412 instructions. All leads until the
   fifteenth round shows them. Engine's word (2026-11-23): the executable chooses what each round checks, in
   WHY§460's order; the game's state's class name is not taken from the executable into `leads.toml` (O111:
   nothing of the executable is written down): a round is given the static as a number and reads the object's
   class from the game's own type information in game, as the naming passes do, and the name enters `leads.toml`
   from there (a small change to the round, Lodestar's, for the round after the fifteenth). The package
   0.1.0-lodestar.19 went to the owner after Engine's read. The executable is in the scratch folder `exe/` (never run; to be deleted when the
   work with it is done); the study's scripts are in the scratch folder `review/` (`exe_*.py`, `pe.py`).
   **Research inputs (O111, 2026-11-23)**: the owner's words, "nothing is off limit for our research, the only
   limit is what we ship with the mod publicly, and what we upload to repo." HANDOVER §3 now reads with O111: nothing
   of the game in files, reports or messages, but a file of the owner's own game given to a session to study is
   allowed. Its conditions: each upload in its own folder outside every checkout, read with `python3 -I` from
   scripts kept elsewhere, never run or built, deleted when the work with it is done; nothing of it in a
   repository, package, published page, report or message (numbers and our own labels only); every fact from it a
   lead until the game confirms it (O48); game files from the owner alone; anything personal in it never written
   down, whatever the owner allows in a chat. The one file worth having is the game's executable (asked of the
   owner): with it, the places the code reads through a manager can be listed offline as leads for the rounds.
   **Far fields (Engine's ruling, WHY§506, 2026-11-23; Lodestar's to build, both sides)**: a field of the layout
   may say `far = true`; its `off` may then lie past `copy_size`, up to 0x100000 into its object, and it is read at
   the object's address plus `off` by a copy of its own size (at most 32 bytes: the whole numbers, f32, bool8, bit,
   bits, ptr, handle, vec3f, dlvector; never utf16[n], mat4f or array). A class whose fields are all far may have
   `copy_size = 0`. Each far read counts against the tick's reads. Done on `feat/lodestar` at 5024ee9: the
   schema and its checks in en-gal's layout.cpp (`FieldLayout::far`, `far_at`, `at()`; `ClassLayout::far_bytes`,
   `buffer_size()`), `ErBackend::object` reading each far field by its own copy into the object's buffer past the
   class's copy, tests on a test world whose roster is 0x12000 bytes with its fields 0x10000 in; and the
   characters manager's places in the build's layout as leads (`main_player` 0x1E508, `by_distance` 0x1F1E8,
   `player_set` 0x10EF8, all far; `near_entry` with `chr` at 0 and `distance` at 8, stride 0x10). ARCH §6.3's dated
   note is Engine's.
   The start's use of the layout is Engine's (discovery through the table and the reader at the start, spikes S0-A
   and S0-B, roadmap S10.1.b), shared as Engine says.
   **The layout for a ready start (roadmap S10.1.m, Lodestar's; Engine's order, WHY§460; the list is WHY§459):**
   `game_state` first, in a round of its own; then the pointers and lists by search and class (`main_player`, the
   player set, the near list, parts, vitals, motion, `player_data`); then the moving numbers by invariant with panel
   tasks (hp, pos, death_count, runes, `menus.screen_state`, the HUD's two, dead, on_screen; a task that costs the
   owner something is optional and says its cost); then the constants as leads from our notes. A start may run with
   the structural groups whole and the optional ones off. The two small ones are done (engine a5efb98): a field
   type for a run of bits in a word (the clock), and the play time as a field of `save_data`.
   - **The plan for `game_state`** (the twelfth round, built as planned): our notes' lead is that its `save_slot` is -1 at
     the title and the slot's number once a save is loaded. So: over the data's slots that point at live memory
     (as the play time's search took them), the first 8 KiB of each object, four passes told by the panel's tasks:
     at the title, in the world, at the title again (the owner quits to the title, not out of the game), in the
     world again. A 32-bit field that reads -1, a small number, -1 and the same small number is the candidate; the
     slot's references in the code are its witnesses; one such slot enters the draft table by the invariant recipe.

2. **R1's clean-up (O106)**: done on `feat/lodestar` (3e11238, and its leftovers as WHY§372 ruled in 532e751), commits of their own (the DLL's R1 mode, the
   launcher's `--probe`, R1's package and cases, and what only R1 used in en-gal), and the dated notes are in ARCH
   and DESIGN. What it left in place because something else may use it is listed in the message to Engine that goes
   with the commit.
3. The design document. Grace's answer on the render feed stands (WHY§309): labels step at the reader's rate, and
   smoothing every frame would need GraceUI's `on_present` callback, by a GraceUI proposal.

## What a sitting needs to know

- The container: `feat/engine` is a worktree at `../eldennet-engine`, on `feat/lodestar`; Wine's loader is
  `/usr/lib/wine/wine64` (not on the path); unset `TMPDIR` for it and give each run its own prefix.
- One build tree beside `build` (GCC, Release) at a time; a second tree building only `en_gal_tests` takes a few
  minutes (Clang, clang-cl, MinGW), then it is removed. The full clang-cl build with the fake game's harness
  (`ctest -R fakegame`) takes about 40 minutes and is worth it whenever `boot.cpp`, the test DLL or the harness change.
- Counts to compare against (engine 532e751, after R1's leftovers went): `en_gal_tests` 220 cases on GCC, Clang
  and MinGW, 235 on clang-cl; ctest 29 of 29 on GCC; the harness 109 of 109 without GraceUI's parts.
- The fake game has two accessors of other shapes (never called) and, with `--field-mirror`, a second word that
  holds one manager's address: the read cases name two managers through variants and one behind two slots.
- A class that holds a `GuardedReader` belongs to the part of en-gal that is built both with and without the probe
  option (`EN_GAL_GAME_SOURCES`), never to the offline part: the reader's shape differs between the two, and the
  tests are built with the option on.
- eldennet.dll may import only what `src/eldennet/tests/imports.allow` lists, and every thread entry in it starts
  with `ffi_guard`; `eldennet_dll_checks` (in the clang-cl tree's ctest) says so.
- The owner's reports are uploads: each goes in a folder of its own and is read with `python3 -I` from a script kept
  elsewhere. They hold numbers only, and are not committed; the record in `docs/ops/probes/` holds their counts.
- A string escape in a `gal/` file must be written as a backslash and `u00A7` in the file itself: a shell heredoc
  here turned it into the character, which `lint --gal` refuses.
- Nothing of the game was needed, read or written so far, and no report of the owner's was asked for.
