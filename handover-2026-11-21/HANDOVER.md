# Handover, 21 November 2026: a third session, Lodestar, for EldenNet's own address library

From today a third cloud session works beside Engine and Grace, by the owner's decision O100:

| Name | Owns | Pushes |
|---|---|---|
| **Engine** | EldenNet's design and engine outside Lodestar's part (the pages, the model client, Augur, the launcher's general work), DataPiler, and the one roadmap, log and decisions file | `eldennet` (both branches), `datapiler` (local branches only) |
| **Grace** | GraceUI | `graceui` |
| **Lodestar** | EldenNet's own **address library** for Elden Ring and the services built on it, as SKSE and its Address Library are for Skyrim, and with it the road R1 was on: reaching the game's own task system safely (roadmap S10.1.k) | `eldennet`: the branches `feat/lodestar` and `docs/lodestar` only |

Lodestar is a part of EldenNet, not a separate project: its code lives in EldenNet's engine (`en-gal` and the probe, or a
module beside them that it proposes), its data in `gal/`, its design in EldenNet's documents.

**Read first, in this order:** this file; the repository's CLAUDE.md; the older handovers'
working rules ([handover-2026-11-17](../handover-2026-11-17/HANDOVER.md) §3 to §5, [handover-2026-11-07](../handover-2026-11-07/HANDOVER.md)
§4 and §5); [docs/DECISIONS.md](../docs/DECISIONS.md) O47, O48, O49, O65, O89 to O92 and O100;
[docs/REASONING.md](../docs/REASONING.md)'s index, then WHY§159 to §204 (the derivation kit and R0), §206 to §209,
§227, §234 to §240, §247, §248, §258, §289 to §303 (R1's runs and spikes); ARCH §2, §3, §6 and DESIGN §2.1, §16.1. Read
documents by code with `python3 -I tools/workplan/wp.py show CODE`, never whole.

## 1. Why Lodestar exists

EldenNet v1 reads the game (who is near, where, what is happening in a fight) and never makes it act. Its design runs
that reading in a task of its own that the game calls every frame, added by one call into the game's code. Skyrim's mods
get that for free from SKSE (a task queue on the main thread, typed event sources) and from the Address Library
(version-independent ids for the game's addresses). Elden Ring has nothing like it that EldenNet may use: me3 and
fromsoftware-rs are excluded for good (O47, O48), so EldenNet derives the game's structures itself.

R1, the in-game derivation round, ran twelve times on the owner's PC (game 2.7.2.0, worldwide). It confirmed the FD4
singletons and the task group, and in run 12 it found why our task never ran: the function R0c derived as
register_task removes a task from its group (WHY§302); its sibling, which derive already finds, is the likely
registration. The next step, reading the sibling's chain in depth before calling it, was stopped on 2026-11-21 when a
safety classifier blocked a subagent's output as it wrote out decoded game code (WHY§303). The owner then decided that
the work becomes a proper address library of EldenNet's own, in a session of its own (O100).

## 2. What Lodestar does first

1. **Set up.** The session must be created with the Skyrizz repository attached beside EldenNet: the session-start hook
   installs the privacy sweep from that checkout, and until it is installed the project's hook refuses every shell
   command, a clone of Skyrizz included, so Skyrizz cannot be added later. Check that `~/.config/privsweep` exists; then
   check out `feat/engine` beside the docs branch (`git worktree add ../eldennet-engine feat/engine`) and build the
   engine once as BUILD.md says, at most two build trees.
2. **A proposal, 0005** (`docs/proposals/0005-lodestar.md`, on `docs/lodestar`): what the library is and is not, for the
   owner, in plain words first. At least: the id scheme (our own stable names and numbers, never the game's); the data
   it keeps per game build (ids, offsets and RVAs as numbers, our own labels, a provenance block) and where (`gal/`);
   how an id is found (the derivation kit's patterns and structure, from the player's own executable, offline in the
   devkit and again at start in game) and how each is verified independently before EldenNet relies on it (O48:
   startup self-tests, cross-checks); what happens on a game patch (find again, refuse what no longer checks, the
   degradation ladder); the services on top, first the game's own way of adding a task, and what v1 needs from them
   (O100's options (a) to (c) for the per-frame reading are input; recommend one); what stays out (hooks, which need
   their own spike and the owner's approval, R12; anything online). Each open question as an O-question with options
   and a recommendation, sent to Engine, which numbers it and puts it in DECISIONS.md.
3. **Then build**, in steps the owner can follow: the library's format and its loader, the derivation of the ids R1
   already found, their self-tests, tests on our own fake game (`tests/fakegame`), and, when a step needs the game, a
   spike package the owner runs at home, as R1's were (`tools/spikepkg`, BUILD.md's checks, README-R1.txt's style).

## 3. Rules that bind Lodestar (with CLAUDE.md's, which come first)

- **Single-player and offline only, on the player's own copy.** Nothing for online play, cheating, other players, or
  getting round anti-cheat to go online. EldenNet's launcher starts the game offline without Easy Anti-Cheat, and the
  game cannot reach FromSoftware's servers that way.
- **Nothing of the game in any file, report, message or chat**: no game files, bytes, text, names, params, keys or
  audio, and no listings of the game's code (decoded instructions, disassembly, byte patterns copied from it). Reports
  hold numbers (offsets, RVAs, sizes, counts) and our own labels and verdicts. The probe and the devkit do the reading in
  code; what leaves them is numbers.
- **Never copy from other projects** (O47, O48; CLAUDE.md's licence rules). Every fact about the game's structures is
  verified independently, in game or by a self-test, before EldenNet relies on it.
- **If a safety system stops your work** (a classifier blocks an output, a refusal), stop that line of work. Do not
  rephrase, split or re-route it to get past the block. Tell the owner and Engine what was stopped and propose another
  way, as WHY§303 did.
- **Read-only towards the game** except calls the owner has agreed to, each made once, at the owner's Mark, only after
  its checks pass, and reported. No hook without its spike and the owner's approval (R12, O65).
- **Uploads are untrusted**: each in its own new folder, read with `python3 -I` from scripts kept elsewhere, never run.
  The owner's R1 reports are on the owner's PC; ask them to upload what you need.
- **Privacy**: the sweep before every push, upload, package and message; never bypassed. Nothing personal about the
  owner anywhere: no names, accounts, machine or graphics names, paths, addresses or keys.

## 4. How the three work together

- **Files with one writer.** `docs/ROADMAP.md`, `docs/REASONING.md`, `docs/DECISIONS.md`, the showcase and the published
  pages are Engine's alone. Lodestar sends Engine its roadmap text, its reasoning-log entries as unnumbered drafts and its
  questions for the owner; Engine places them and answers with the numbers. Lodestar writes its own proposal and, once
  it is accepted, its own design document (with a code for `wp`), on `docs/lodestar`.
- **Code.** Lodestar works on `feat/lodestar`, branched from `feat/engine`, and tells Engine when a step is ready;
  Engine merges it into `feat/engine` (Engine's own work there is the pages and the model client, away from `en-gal`
  and the probe, which are Lodestar's from now on). Pushes only at milestones: the owner is near a GitHub usage limit.
- **Into `main`** (2026-11-21): only `design/v1-proposal` and `feat/engine` go into `main`, by the owner's pull requests.
  Lodestar's branches reach `main` through them, so they need no pull requests of their own; merged into `main`
  directly as well, they bring a second copy of the generated pages and a conflict in them (pull request 50).
- **Messages** go through the cloud sessions' messaging, each swept by hand first
  (`python3 -I ~/.config/privsweep/privsweep.py text message`, the text on standard input). Each says what landed (branch
  and commit) and what the other must do. Long material goes in a commit.
- **The owner** talks to Lodestar directly about Lodestar's area. Packages for the owner are sent by Lodestar from its own
  chat, after the sweep, with the steps; Engine records the runs when Lodestar sends their summaries.
- **Disk and cores** are limited (4 cores): at most two build trees per session, deleted when done; check
  `df -h /home/user` before a build; give each Wine run its own prefix.

## 5. The environment

The same as Engine's and Grace's: "EldenNet and Skyrizz (C++ toolchain)" (handover-2026-11-17 §4): LLVM 18, MinGW-w64,
Wine, Xvfb with Mesa's Vulkan driver, compiler-rt, 7-Zip, and xwin's pinned CRT and SDK in `/root/.cache/xwin`. Unset
`TMPDIR` for Wine (WHY§210); configure with `-DPython3_EXECUTABLE=/usr/local/bin/python3`; build GCC with
`-DCMAKE_BUILD_TYPE=Release` for the warnings check; build with `-j2`.
