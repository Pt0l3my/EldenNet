# Lodestar's status, 22 November 2026 (after step L4: the reader thread)

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
| Digests | — | Moved to L7 (WHY§311): v1 calls no function of the game |
| The text for ARCH and DESIGN (proposal §15) | `docs/lodestar` | Written as dated notes and changed rows: ARCH §1.2, §3.2, §6.2, §6.3, §6.4, §6.10; DESIGN §2.1, §4.2, §16.1 |
| The design document, with a code for `wp` | `docs/lodestar` | Not started |

Engine holds the numbers: O101 to O103; WHY§304 to §312 are Lodestar's drafts placed (next free: 313); S10.1.k carries
the steps. Only `design/v1-proposal` and `feat/engine` go into `main`; Lodestar's branches reach it through them.

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

1. **L5: the read-only in-game package**, which names the managers' slots by their instances' classes (O92), checks
   the witnesses, runs the reader at the title and in the world with its faults and dropped readings counted, and
   writes the first table for 2.7.2.0. It replaces R1's package (`tools/spikepkg`'s `r1` and README-R1.txt.in are
   legacy until then), and with it `probe_r1.cpp`'s call, the last use of `install_task` and the getter, goes. On the
   real game a start cannot name the managers until that table exists: the name layout is dead there (WHY§314) and
   the getter is never called (O101), so today's DLL stops at the banner on the real build, as it must.
2. `lodestar check` and `diff`, and the draft-table writer, which L5 needs.
3. The design document. Grace's answer on the render feed stands (WHY§309): labels step at the reader's rate, and
   smoothing every frame would need GraceUI's `on_present` callback, by a GraceUI proposal.

## What a sitting needs to know

- The container: `feat/engine` is a worktree at `../eldennet-engine`, on `feat/lodestar`; Wine's loader is
  `/usr/lib/wine/wine64` (not on the path); unset `TMPDIR` for it and give each run its own prefix.
- One build tree beside `build` (GCC, Release) at a time; a second tree building only `en_gal_tests` takes a few
  minutes (Clang, clang-cl, MinGW), then it is removed. The full clang-cl build with the fake game's harness
  (`ctest -R fakegame`) takes about 40 minutes and is worth it whenever `boot.cpp`, the test DLL or the harness change.
- Counts to compare against: `en_gal_tests` 219 cases on GCC, Clang and MinGW, 234 on clang-cl; ctest 29 of 29 on
  GCC; the harness 112 of 112 (seven `gal-own-*` cases joined it).
- The owner's reports are uploads: each goes in a folder of its own and is read with `python3 -I` from a script kept
  elsewhere. They hold numbers only, and are not committed; the record in `docs/ops/probes/` holds their counts.
- A string escape in a `gal/` file must be written as a backslash and `u00A7` in the file itself: a shell heredoc
  here turned it into the character, which `lint --gal` refuses.
- Nothing of the game was needed, read or written so far, and no report of the owner's was asked for.
