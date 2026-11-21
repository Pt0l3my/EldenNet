# Lodestar's status, 21 November 2026 (end of the first sitting)

For the next sitting of the Lodestar session, or a new session that takes it over. Read [HANDOVER.md](HANDOVER.md)
first; this file adds only where Lodestar's own work stands.

## Where things stand

| What | Where | State |
|---|---|---|
| Proposal 0005 | `docs/proposals/0005-lodestar.md` (`docs/lodestar`, and on `design/v1-proposal` through Engine) | Accepted: O101 (b), O102 (a) and O103 (a), each as recommended |
| Step L1: the catalogue (`gal/ids.toml`, `ids.hpp`) and the build's table (`layout.hpp`, schema 2) | `feat/lodestar` dc18c95; merged into `feat/engine` as fb999ca | Done |
| The resolver: placement and witness checks, `Library` (`resolve.hpp`) | `feat/lodestar` 440233e | Done on invented images; nothing in the DLL reads through it |
| L2, first half: shapes (`shape.hpp`) and the accessor finder on a shape (`fd4::Accessor`) | `feat/lodestar` 700d54e | Done: on the tests' three invented programs the shape's sites are exactly the pattern's. The pattern path is still the one in use |
| The design document, with a code for `wp` | `docs/lodestar` | Not started |
| The text for ARCH §3.2, §6.2 to §6.4 and §6.10, DESIGN §2.1, §4.2 and §16.1 (proposal §15) | to send to Engine | Not started |

Engine holds the numbers: the questions are O101 to O103, the log entries WHY§304 to §306 (next free: 307), and
S10.1.k carries the steps L1 to L7.

## What comes next, in order

1. **L2, second half: the shape in use.** `gal/recipes.toml` (schema, reader, `lint --gal`, a note per recipe in
   `gal/derivations/`), holding the accessor: its five forms (`load_slot`, `test_loaded`, `branch_set_short`,
   `address_arg1`, `call_relative`), which step gives the static, the class object and the getter, and the path
   suffix. Then the three readers of `patterns.toml`'s idiom move to it: the derivation (`derive.cpp`, which today cuts
   the idiom's bytes from the sites: it would report the shape's hits and sites instead), discovery
   (`discovery.cpp`) and R1's check (`probe_r1.cpp`, "the idiom byte for byte" becomes "the shape's sites"). The fake
   game's harness cases under Wine must pass unchanged, in clang-cl's and MinGW's `fakegame.exe`. Only then is
   `patterns.toml` removed, with its note and ARCH §6.3's text (through Engine). The shape is stricter than the
   pattern (a 64-bit load, a test of the same register, a forward branch), so on the real game its hits may be fewer
   than R0c's 7,151: L3's run reports both counts, and the accepted sites against R1's 7,081.
2. **L2, digests.** A function's digest over its code as loaded, with relocated bytes and RIP-relative displacements
   masked. The launcher has our own BLAKE3 (`blake3.cpp`, WHY§174); it would move where en-gal can link it.
3. **L3**: `lodestar find`, `check` and `diff` in `eldennet-engine`, report schema 2 (`kind = "lodestar"`, no key that
   can carry bytes or instruction forms); send Engine its vocabulary for DataPiler's guard. Then the owner's one
   offline run on 2.7.2.0, as a package (`tools/spikepkg`), swept, with plain steps.
4. **L4**: discovery at start through `Library`, and the frame service's `own thread` provider with §10's checks
   (structure read twice, a change held for two readings). Under O101 (b), `register_task` and `call_getter` leave
   release builds.
5. The design document and the text for Engine, once L2 shows the grammar's final form.

## What a sitting needs to know

- The container: `feat/engine` is a worktree at `../eldennet-engine`; Wine's loader is `/usr/lib/wine/wine64` (not on
  the path); unset `TMPDIR` for it and give each run its own prefix.
- One build tree beside `build` (GCC, Release) at a time. Building only `en_gal_tests` in the second tree takes a few
  minutes: Clang (`CXX=clang++-18`), clang-cl (`cmake/clang-cl-xwin.cmake`), MinGW (`cmake/mingw-w64.cmake`), then
  the tree is removed. The fuzz tree is `-DEN_FUZZ=ON` with Clang; `en_fuzz_galtoml` covers `ids.toml` and the items.
- Counts to compare against: `en_gal_tests` 194 cases on GCC, Clang and MinGW, 209 on clang-cl; ctest 29 of 29 on GCC.
- A witness says that the bytes at a place refer to the id, not that an instruction starts there (one byte into a
  prefixed load the bytes still decode to the same reference). Accepted, tested and said in en-gal's README.
- Nothing of the game was needed, read or written in this sitting, and no report of the owner's was asked for.
