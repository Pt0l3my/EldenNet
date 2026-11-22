# Lodestar's status, 22 November 2026 (the third read-only round is built)

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

1. **The owner's third read-only run** (package `EldenNet-Read`, label 0.1.0-lodestar.6, from engine 66c8675).
   Review its two files and its logs as the others' (`python3 -I`, a folder of its own; the scratchpad's review
   scripts are gone with the container: they only print the report's counts). What to look for, in this order:
   - `accessor.variants`: where each of the five other shapes stands, and the statics only they reach. Set each
     variant's `expect_matches` to its count, and withdraw one that stands nowhere.
   - `instances.managers`: whether `messages` and `event_flags` are named now (`variant`, or `by_search` with
     `search_objects` 1), and each search's own numbers.
   - `search`: of the values that could be an address, how many had no memory behind them (`not_committed`) and how
     many memory that may not be read (`no_access`); that settles what the 97 in 100 were.
   - `members.member_hits`: a static and an offset where an unnamed lead's object lies or is pointed at.
   - `rising`: the best hits, by passes risen; `trial`: the reader's rate with the searches running (10 a second
     is the aim), and `search.steps_cut`.
   Add a section to `docs/ops/probes/2026-11-22-l5.md` or a new record, and place what it names in the build's
   table as leads. The clock's, the play time's and the members' hits are for the review to judge: nothing enters
   the table from them without a second look in a later round.
2. Retire R1's package and `probe_r1.cpp` (the last use of `install_task` and the getter), with the
   owner's and Engine's word; `lodestar check` and `diff` against a table.
3. The design document. Grace's answer on the render feed stands (WHY§309): labels step at the reader's rate, and
   smoothing every frame would need GraceUI's `on_present` callback, by a GraceUI proposal.

## What a sitting needs to know

- The container: `feat/engine` is a worktree at `../eldennet-engine`, on `feat/lodestar`; Wine's loader is
  `/usr/lib/wine/wine64` (not on the path); unset `TMPDIR` for it and give each run its own prefix.
- One build tree beside `build` (GCC, Release) at a time; a second tree building only `en_gal_tests` takes a few
  minutes (Clang, clang-cl, MinGW), then it is removed. The full clang-cl build with the fake game's harness
  (`ctest -R fakegame`) takes about 40 minutes and is worth it whenever `boot.cpp`, the test DLL or the harness change.
- Counts to compare against: `en_gal_tests` 243 cases on GCC, Clang and MinGW, 258 on clang-cl; ctest 29 of 29 on
  GCC; the harness 115 of 115 (seven `gal-own-*` cases and three `launcher-eldennet-read*` cases joined it).
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
