# Lodestar's status, 21 November 2026 (after its second sitting)

For the next sitting of the Lodestar session, or a new session that takes it over. Read [HANDOVER.md](HANDOVER.md)
first; this file adds only where Lodestar's own work stands.

## Where things stand

| What | Where | State |
|---|---|---|
| Proposal 0005 | `docs/proposals/0005-lodestar.md` | Accepted: O101 (b), O102 (a) and O103 (a), each as recommended |
| L1: the catalogue (`gal/ids.toml`, `ids.hpp`) and the build's table (`layout.hpp`, schema 2) | engine dc18c95 | Done, merged |
| The resolver: placement and witness checks, `Library` (`resolve.hpp`) | engine 440233e | Done on invented images, merged; nothing in the DLL reads through it yet |
| L2: shapes (`shape.hpp`), the accessor as a recipe (`gal/recipes.toml`, `recipes.hpp`), discovery by the shape, `gal/patterns.toml` withdrawn | engine 700d54e, 21c5b02 | Done, merged: `gal/` holds no byte of the game. The fake game's harness passes 105 of 105 with the shape in use |
| L3: the finder (`lodestar.hpp`, `eldennet-engine lodestar find`), report kind `lodestar` schema 2, the `find` package kind | engine 9edf1c0 | Built, merged. **The owner's offline run has not happened** |
| The package for that run, `EldenNet-Find.zip`, label 0.1.0-lodestar.1 | not in the repository | Built, checked under Wine, swept and **sent to the owner** (below) |
| Digests | — | Moved to L7 (WHY§311): v1 calls no function of the game |
| The design document, with a code for `wp`; the text for ARCH and DESIGN (proposal §15) | `docs/lodestar` | Not started |

Engine holds the numbers: O101 to O103; WHY§304 to §312 are Lodestar's drafts placed (next free: 313); S10.1.k carries
the steps. Only `design/v1-proposal` and `feat/engine` go into `main`; Lodestar's branches reach it through them.

## The package and the sweep

The sweep at first refused `EldenNet-Find.zip` under its rule `email`: `THIRD-PARTY-NOTICES.txt` holds toml++'s MIT
licence unchanged, and its copyright line carries its author's address. The owner chose, in Lodestar's chat on
2026-11-21, "Allow that one address": one narrow, commented line in the sweep's `allow.txt` (rule `email`, that one
address, only in `THIRD-PARTY-NOTICES.txt` and `tomlplusplus/LICENSE`). It is committed in Skyrizz on this session's
branch there, `claude/serene-tesla-aan6oc` (cece89f), and takes effect for other sessions once the owner or the
Skyrizz session merges it into Skyrizz's working branch; until then another container's sweep refuses such a package
again. The package (SHA-256 75989ada93cae98b70515d374d3cba19ff3e42c1af6a3f4952d488e442b5bdb5, built from engine
9edf1c0) then passed the sweep and **was sent to the owner on 2026-11-21**. To rebuild it: BUILD.md, "The spike
package", the `find` kind.

## What comes next, in order

1. **The owner's offline run** (about 5 minutes, the game closed): the owner drops the
   game's executable onto `find.cmd` and sends back `lodestar-found.json` and the console's line. Review it as data
   (`lint --report`, then against en-tools' README): the shape's hits and accepted sites against R0c's 7,151 and
   7,081, `only_shape` and `only_pattern` (both should be 0), the statics found (R0c: 219) and class objects (217),
   the classes and the table (168 entries of 32 bytes, index 110). Write the record in `docs/ops/probes/` through
   Engine, set the recipe's `expect_matches` from the count, and send Engine a log draft.
2. **L4: the reader thread.** Discovery at start through `Library` once a table exists, and the frame service's `own
   thread` provider with proposal §10's checks (structure read twice, a change held for two readings). Under O101 (b),
   `register_task` and `call_getter` leave release builds. Grace's answer (WHY§309): nothing in GraceUI assumes
   per-frame updates; labels step at the reader's rate and a camera turn shows it most; smoothing every frame would
   need GraceUI's `on_present` callback, which is designed but not in ABI 1.2, by a GraceUI proposal.
3. **L5: the read-only in-game package**, which names the managers' slots by their instances' classes (O92) and
   writes the first table for 2.7.2.0; it replaces R1's package (`tools/spikepkg`'s `r1` and README-R1.txt.in are
   legacy until then).
4. `lodestar check` and `diff`, and the draft-table writer, when a table exists to check.
5. The design document and the text for Engine.

## What a sitting needs to know

- The container: `feat/engine` is a worktree at `../eldennet-engine`, on `feat/lodestar`; Wine's loader is
  `/usr/lib/wine/wine64` (not on the path); unset `TMPDIR` for it and give each run its own prefix.
- One build tree beside `build` (GCC, Release) at a time; a second tree building only `en_gal_tests` takes a few
  minutes (Clang, clang-cl, MinGW), then it is removed. The full clang-cl build with the fake game's harness
  (`ctest -R fakegame`) takes about 40 minutes and is worth it whenever `boot.cpp`, the test DLL or the harness change.
- Counts to compare against: `en_gal_tests` 204 cases on GCC, Clang and MinGW, 219 on clang-cl; ctest 29 of 29 on
  GCC; the harness 105 of 105.
- A string escape in a `gal/` file must be written as a backslash and `u00A7` in the file itself: a shell heredoc
  here turned it into the character, which `lint --gal` refuses.
- Nothing of the game was needed, read or written so far, and no report of the owner's was asked for.
