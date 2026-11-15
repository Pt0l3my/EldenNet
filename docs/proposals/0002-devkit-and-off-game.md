# Proposal 0002: The EldenNet devkit, and EldenNet off game

**Status: proposal, 2026-11-15 (roadmap S7.11.b and S7.12.a). O74 and O75 decided by the owner the same day; O76 to
O79, and GraceUI's G29, open.** Where this text and the designs differ, the designs hold. A bare §n is a section of
this proposal; DESIGN and ARCH are EldenNet's design documents, and "GraceUI §n" and G- and D-numbers are GraceUI's.
Evidence: the terrain research (WHY§89 to WHY§94), the map's research (WHY§56, WHY§62 to WHY§82), GraceUI's server
(GraceUI §7.2, invariant 4) and EldenNet's settings (ARCH §11.5).

## 1. What the owner asked

- "One game-data reader of our own: could this be EldenNet devkit?" Decided as O74 (a): a reader of our own, published
  with EldenNet, serving EldenNet in the game, DataPiler on the PC (its DP35) and, later, plugin authors.
- "allow it closed with safe limits and disclosure about safe usage and what is condemned (we make that clear)."
  Decided as O75: the devkit works with the game closed too, within the limits of §2.3 and with the disclosure of §2.4.
- "I also want the devkit and EldenNet to have a http based side that can run off game so the user can open up UI
  out of game, in case of devkit users could use tools, in case of EldenNet, the framework would simple save its stage
  that can be opened in browser and inspected, also, settings can be changed (maybe not all of them but some) which
  manifest when EldenNet loads in game." Proposed in §2.5 (the devkit's tools) and §3 (EldenNet off game), with GraceUI's
  part in §4; questions in §6.

## 2. The devkit (O74, O75)

### 2.1 What it is

A C++ library and a command-line tool, MIT, published with EldenNet. Our own code, with every fact about the game's
formats verified on the owner's install (O48); nothing from me3 or fromsoftware-rs. Its callers:

- EldenNet in the game: the map's run-time read (S7.9.c), then names and text.
- EldenNet's launcher, off game: the map's cache built before play, and a check of the game's files after an update,
  before the game starts (§3).
- DataPiler on the PC: the workbench's reads and its local index (its DP35).
- Plugin authors, later: beside the plugin interface's headers and its checker (O68).

### 2.2 What it reads, in order

1. The archives (`.bhd` and `.bdt`), with the keys taken from the user's own game when used, never stored or shipped,
   and the files found by the game's own path hashes.
2. DCX through the game's own Oodle library, loaded from the game's folder and never copied; BND4 binders.
3. Map models (positions and triangles, as WHY§90 to WHY§94 read them) and placement files (parts, positions,
   rotations and scales, WHY§92).
4. The map's textures and masks (WHY§62 to WHY§77), decoded in memory.
5. Then the game's text, for names at run time; its data tables later, each layout verified on the owner's install.

### 2.3 Safe limits (O75)

- It works with the game closed or running, and never reads or writes the game's memory.
- **Read-only.** It never writes, repacks or patches a game file.
- **No bulk export.** The command-line tool and the browser tools report counts, ids and checks. The devkit never
  writes the game's models, textures, text or sounds out as files: the library hands data to its caller in memory, and
  the caller's own rules apply (EldenNet's caches stay on the player's PC; DataPiler's workspace stays local and its
  guard checks every share).
- **Nothing of the game ships with it**: no files, text, keys or Oodle.
- It reads only the install it is pointed at or finds on the PC.

### 2.4 The disclosure (O75)

Shown in the devkit's README, on the command-line tool's first run and in `--help`, and at the foot of its browser
tools. Draft, for the owner to amend:

> **Safe use.** The EldenNet devkit reads your own copy of Elden Ring on your own PC, so that you can make and check
> mods and study your own install. It never changes the game's files, keeps no keys and shares nothing.
>
> **What we condemn.** Sharing the game's files, models, textures, text, sounds or keys; using the game's content
> commercially; leaking unreleased content; cheating, or any use online or against other players; getting round
> anti-cheat; using a copy of the game you do not own. The devkit is not built for any of these, and we will not help
> with them.

The devkit's licence stays MIT: the disclosure states our position and the safe use; it is not a licence term.

### 2.5 The devkit's browser tools (O78)

`eldennet-dk serve` opens the devkit's tools on the PC's loopback, on GraceUI's server for hosts outside the game (§4).
Recommended (O78 (a)): checks and numbers only.

- A self-test of the install: the game's version, and what the devkit can read in it.
- A content plugin checked against the game's ids, with O66's checks.
- Research numbers, as DataPiler asks for them.
- No viewers of the game's models, textures or text: Smithbox is there for that.

### 2.6 Where it lives (O79)

Recommended (O79 (a)): a `devkit/` folder in EldenNet's repository, under EldenNet's design and reasoning log, with
releases of its own.

### 2.7 First checks (S7.11.c)

A file read through the devkit matches Smithbox's copy byte for byte, checked through DataPiler on the owner's PC; the
archives are read with the game closed and, separately, while it runs.

## 3. EldenNet off game (O76, O77)

### 3.1 What the player gets

The dashboard in the browser while the game is closed: the same pages, showing what EldenNet saved, read-only, and the
settings, changed now and applied when EldenNet next loads in the game.

### 3.2 How (O76 (a), recommended)

- **EldenNet saves its state as page data.** At each save point and on exit, it writes what each page shows: the
  projections GraceUI's pages receive, already through the spoiler layers (SP1 to SP9) and the privacy rules (PV1 to
  PV8). The files stay in EldenNet's folder on the PC, one set per character, known by the save's fingerprint; the
  Steam ID is never shown.
- **The launcher serves them.** It runs outside the game and stays as the crash watcher (O71); off game it serves
  GraceUI's pages over those files, on loopback, under GraceUI's server rules (invariant 4). When EldenNet loads in the
  game, the launcher hands the dashboard's address to it, and takes it back when the game exits: one dashboard at a
  time, at the same address.
- **What shows.** Each page says "as of the last session" and when that was. Anything live (who is near, the talk
  under way) is absent, never greyed (invariant 2). The off-game dashboard never touches the game, so it may stay open
  whatever the game does; EldenNet itself stays dormant beside anti-cheat, as now.

### 3.3 Settings off game (O77)

- Each setting in the settings index (GraceUI D101) carries a mark: changeable off game (applied at the next load) or
  in game only. Recommended (O77 (a)): every setting changeable off game but a short list that needs the live game,
  fixed in the index; secrets stay write-only.
- A change is checked by the same validator as in the game, written to `settings.toml` (ARCH §11.5), and shown as
  "applies when EldenNet next loads". EldenNet reads the file at load, as now; nothing else of EldenNet's state is
  written off game.

### 3.4 What it changes

- GraceUI's G4 (the dashboard, always on): also off game, while the launcher runs.
- GraceUI's invariant 4 and §7.2: the server also runs outside the game, in the launcher's process and the devkit's,
  from a library GraceUI publishes (G29).
- EldenNet: the launcher serves the dashboard (DESIGN §3.2, ARCH §4); EldenNet writes the page data at save points and
  on exit; the settings index gains the off-game mark.

## 4. GraceUI's server for hosts outside the game (G29)

GraceUI publishes its server and page runtime as a library for hosts outside the game: EldenNet's launcher and the
devkit. The same rules hold: loopback only; Host, Origin, Fetch Metadata, the session cookie and the ticket checked; a
strict CSP; only the files listed served. The home-network switch and its pairing (G9) stay in the game at first,
since pairing shows a code in the game. Recommended (G29 (a)): one library; the alternative, (b), has each host write
its own server.

## 5. Privacy and safety

- The page data holds only what the pages show, already filtered; never a key, a path or a Steam ID; and it stays on
  the PC.
- The devkit keeps the limits of §2.3 and shows the disclosure of §2.4.
- Nothing new leaves the PC.

## 6. Questions for the owner

### O76: How does EldenNet's dashboard work off game?

(a) **Recommended.** The launcher serves GraceUI's pages over the page data EldenNet writes at each save point and on
exit: read-only, plus the settings (§3.2). (b) The launcher runs EldenNet's engine off game, to build the pages from
the saved files: more complete, heavier, and only once Min's engine is adopted. (c) Settings only, off game.

### O77: Which settings change off game?

(a) **Recommended.** All but a short in-game-only list, marked in the settings index (§3.3). (b) A chosen few: model
providers and keys, voices, features and pacing. (c) None: off game is for looking only.

### O78: What do the devkit's browser tools show?

(a) **Recommended.** Checks and numbers only (§2.5). (b) Also viewers of the game's models, textures and text, shown in
the browser and never exported.

### O79: Where does the devkit live?

(a) **Recommended.** A `devkit/` folder in EldenNet's repository, with releases of its own. (b) A repository of its
own.

### GraceUI's G29: GraceUI's server for hosts outside the game?

(a) **Recommended.** One server library GraceUI publishes (§4). (b) Each host writes its own. G29 follows O76: with
O76 (c), only the settings pages need it.
