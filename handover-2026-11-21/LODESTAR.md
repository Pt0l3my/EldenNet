# Lodestar's status, 22 November 2026 (after the owner's finding runs)

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
| Digests | — | Moved to L7 (WHY§311): v1 calls no function of the game |
| The design document, with a code for `wp`; the text for ARCH and DESIGN (proposal §15) | `docs/lodestar` | Not started |

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

1. **L4: the reader thread.** Discovery at start through `Library` once a table exists, and the frame service's `own
   thread` provider with proposal §10's checks (structure read twice, a change held for two readings). Under O101 (b),
   `register_task` and `call_getter` leave release builds. Grace's answer (WHY§309): nothing in GraceUI assumes
   per-frame updates; labels step at the reader's rate and a camera turn shows it most; smoothing every frame would
   need GraceUI's `on_present` callback, which is designed but not in ABI 1.2, by a GraceUI proposal.
2. **L5: the read-only in-game package**, which names the managers' slots by their instances' classes (O92) and
   writes the first table for 2.7.2.0; it replaces R1's package (`tools/spikepkg`'s `r1` and README-R1.txt.in are
   legacy until then).
3. `lodestar check` and `diff`, and the draft-table writer, when a table exists to check.
4. The design document and the text for Engine. Among it: the name-layout branch (`[dlrf]`, builds.toml's
   `dlrf_name_layout`) is dead for this game, since the class objects are one-byte things (WHY§314).

## What a sitting needs to know

- The container: `feat/engine` is a worktree at `../eldennet-engine`, on `feat/lodestar`; Wine's loader is
  `/usr/lib/wine/wine64` (not on the path); unset `TMPDIR` for it and give each run its own prefix.
- One build tree beside `build` (GCC, Release) at a time; a second tree building only `en_gal_tests` takes a few
  minutes (Clang, clang-cl, MinGW), then it is removed. The full clang-cl build with the fake game's harness
  (`ctest -R fakegame`) takes about 40 minutes and is worth it whenever `boot.cpp`, the test DLL or the harness change.
- Counts to compare against: `en_gal_tests` 207 cases on GCC, Clang and MinGW, 222 on clang-cl; ctest 29 of 29 on
  GCC; the harness 105 of 105.
- The owner's reports are uploads: each goes in a folder of its own and is read with `python3 -I` from a script kept
  elsewhere. They hold numbers only, and are not committed; the record in `docs/ops/probes/` holds their counts.
- A string escape in a `gal/` file must be written as a backslash and `u00A7` in the file itself: a shell heredoc
  here turned it into the character, which `lint --gal` refuses.
- Nothing of the game was needed, read or written so far, and no report of the owner's was asked for.
