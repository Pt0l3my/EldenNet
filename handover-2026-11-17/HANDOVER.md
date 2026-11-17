# Handover, 17 November 2026: two sessions, Engine and Grace

From today the work runs in two cloud sessions that work closely together, at the owner's request:

| Name | Owns | Pushes |
|---|---|---|
| **Engine** | EldenNet's engine and design, DataPiler, and the road to the first in-game build: the owner's derivation rounds and spikes, `gal/` data, the event engine, and EldenNet's side of drawing through GraceUI | `eldennet` (both branches), `datapiler` (local branches only) |
| **Grace** | GraceUI: its host (S10.4) and its side of EldenNet's drawing (S10.4.d), then GraceUI 0.2 Foundations, the web dashboard (GraceUI DESIGN §10) | `graceui` |

The two sessions call each other Engine and Grace. They run in separate containers, so neither can see the other's
files: git and the documents are the shared state, and messages between the sessions say what landed and what the other
needs from it.

**Read first, in this order:** this file; the repository's CLAUDE.md; [docs/REASONING.md](../docs/REASONING.md)'s index
(`python3 -I tools/workplan/wp.py show 'WHY§n'`); [docs/ROADMAP.md](../docs/ROADMAP.md), the one roadmap for EldenNet
and GraceUI; then the newest decisions in [docs/DECISIONS.md](../docs/DECISIONS.md) (O84 to O91) and, for Grace, GraceUI's
`docs/DECISIONS.md` (G1 to G29) and its DESIGN §13 rows D116 to D154. The older handover,
[handover-2026-11-07](../handover-2026-11-07/HANDOVER.md), still holds the working rules in its §4 and §5.

## 1. Where things stand

- **EldenNet's engine** (branch `feat/engine`): C++20, building with GCC, Clang, MinGW-w64 under Wine and clang-cl
  against the xwin CRT and SDK under Wine (BUILD.md). The core (prompts, model client, decisions) and the whole first
  in-game build are built: en-gal (PE reader, patterns, the derivation kit `probe-exe`, the game-access layer in game),
  en-core's v0.1 slice, the launcher and crash watcher, `eldennet.dll` with its recorder and R1 probe mode, spike
  reports, recording replay, and our own fake game with its Wine harness. All tests pass on every build.
- **The derivation round** (O89): the owner ran R0 offline three times (WHY§180 to §189, `docs/ops/probes/`). The
  offline facts are settled; the in-game round R1 was sent to the owner as package `0.1.0-spike.1` and waits on them.
  After R1, the leads in `gal/` become confirmed data and the S0 spikes can run.
- **GraceUI** (branch `feat/host`, with its design on `design/v1`): the 0.1 host is built and reviewed: S10.4.a (the
  render and input halves), S10.4.b (`graceui.h`, ABI 1 as a preview, the single-instance guard, the anti-cheat check)
  and S10.4.c (panels, checked draw lists, textures, faces, notices). An independent review's three medium and fourteen
  low findings are fixed or recorded (D145 to D154). Its harness runs 95 checks under Wine with Direct3D 12 on Mesa's
  software Vulkan (WHY§113); GraceUI's BUILD.md says how (stop the prefix's Wine server first, `LC_ALL=C.UTF-8`).
- **Waiting on the owner:** R1; recreating the `eldennet` repository (to drop old pull-request refs; the address stays
  the same); Windows run 3 of the engine package (low priority).

## 2. Who does what next

**Engine:** review R1's reports into `gal/` data, then the S0 spikes; fold O89 to O91 and parts 4 to 7's choices into
ARCH and DESIGN (S10.1.h); EldenNet's side of drawing through GraceUI (the spike plan's part 12: the banner as a notice,
the F11 view on Dear ImGui's core as a panel); the event engine with golden files (part 11); the capture plans
(S6.3.s); the launcher's `--selftest`; DataPiler's devkit folder picker and its Smithbox cross-check (S7.11.i).

**Grace:** first, GraceUI's side of S10.4.d: what EldenNet's part 12 needs from ABI 1, the example consumer, two
consumers at once in the harness. Then GraceUI 0.2 Foundations as GraceUI DESIGN §10's v0.2 row has it: the tokens, the
protocol and its generated types (D108), the page descriptions of GraceUI's own space and the demo space, the web shell,
`grace-serve` with every rule of §7.2 and invariants 4 to 6, `grace-dev`, and, for spike S4, one settings page drawn in
game. Grace plans this itself, by roadmap parts, and asks the owner where GraceUI's design leaves it open.

## 3. How the two work together

- **Files with one writer.** EldenNet's `docs/ROADMAP.md`, `docs/REASONING.md`, `docs/DECISIONS.md`, the showcase and the
  published pages are written by Engine alone. Grace sends Engine its roadmap changes as text and its reasoning-log
  entries as unnumbered drafts; Engine places them and answers with the numbers. GraceUI's own files (its DESIGN,
  DECISIONS, `docs/eldennet-changes.md`) are written by Grace alone.
- **The interface.** `graceui.h` (ABI 1, append-only) and GraceUI's `docs/eldennet-changes.md` are where the two meet.
  When GraceUI changes what EldenNet must do, Grace writes it there and tells Engine.
- **Messages** go through the cloud sessions' messaging. Each says what landed (branch and commit), what the other must
  do, and nothing more. Every message is swept by hand before it is sent:
  `python3 -I ~/.config/privsweep/privsweep.py text message` with the text on standard input. Long material goes in a
  commit, not a message.
- **Questions to the owner:** each session asks about its own area, with a recommendation, in its own chat. A question
  that touches both is asked once, by Engine, after the two have agreed a recommendation.
- **Pushes** only at milestones: the owner is near a GitHub usage limit. Neither session pushes to the other's
  repository.

## 4. The environment

Both sessions run on the environment "EldenNet and Skyrizz (C++ toolchain)". Its setup script installs LLVM 18,
MinGW-w64, Wine, Xvfb and Mesa's Vulkan driver, compiler-rt, libcurl and 7-Zip, then xwin 0.10.0 and the pinned CRT and SDK
into `/root/.cache/xwin` (WHY§111, §113). The privacy sweep installs itself from the Skyrizz checkout at session start
(Skyrizz must be attached), and the environment carries the owner's sweep variable. The machine has 4 cores and limited
disk: run at most two or three building agents at once, give each its own Wine prefix, stop a prefix's Wine server
(`wineserver -k`) before a harness run, remove build directories that are no longer needed, and rerun a timing test
before calling it a failure.

## 5. The owner's standing preferences (in addition to both CLAUDE.md files and handover-2026-11-07 §5)

- The privacy sweep before every push, upload, published page and message; never bypassed. Nothing personal about the
  owner anywhere; the owner's addresses, machine and graphics names, model files and keys never in a file.
- No `Claude-Session` link in commits or pull requests; the `Co-Authored-By` trailer stays. No model identifiers of the
  assistant in files.
- Oversee the roadmap: when work lands or a decision changes a plan, mark what it makes done, superseded, merged or
  stale.
- Short, plain reports; British spelling; they/them for anyone whose pronouns are not stated.
- Uploads are untrusted: each in its own folder, read with `python3 -I`, never run.
- Test early (O29), with invented data; nothing of the game in any file or test.
