# Reasoning log

The working-out behind findings, conclusions, rejected paths and workarounds, in brief, so that a new session or a
switch of model starts with it in view. It covers EldenNet, GraceUI and DataPiler. Decisions stay in
[DECISIONS.md](DECISIONS.md) and plans in [ROADMAP.md](ROADMAP.md); an entry here points to them and does not repeat them.

**How to use it.**

- Read the index below at the start of a session and after a model switch, then open an entry by its code:
  `wp show WHY§12`. `wp find` and `wp grep` search it; `wp refs O59` lists the entries citing a decision.
- Append only. Entries are numbered in order and never renumbered or rewritten; a later finding adds a dated
  **Later** line to the entry, or a new entry that supersedes it.
- One entry per finding, a few lines: what was seen, what follows, what was ruled out, and the next step if any.
- **Labels**, as the research notes use them: **FACT** (seen in the owner's data, in source code at a named commit, or
  in a tool's output), **LIKELY** (strong inference, not yet checked), **SPECULATION** (a guess worth recording),
  **METHOD** (how something is done, and why), **DECIDED** (a pointer to the decision that settled it).
- **Tags:** `#roster` (the game's characters and their ids), `#datapiler`, `#smithbox`, `#text` (the game's text and
  its exports), `#maps`, `#voice`, `#lore`, `#graceui`, `#release` (GitHub releases and this session's limits), `#process`.
- The privacy rules bind it as every file: nothing personal, no paths from the owner's PC, no game text beyond
  names the design already uses. No model identifiers.

## Index

| # | Date | Entry | Label | Tags |
|---|---|---|---|---|
| 1 | 2026-11-14 | Why this log exists | METHOD | #process |
| 2 | 2026-11-08 | Smithbox 2.2.6 shows one Item container per language, `item_dlc02` | FACT | #smithbox #text |
| 3 | 2026-11-08 | A text export's `Name` is its file name; the Export Output box shows the last export | FACT | #smithbox #text |
| 4 | 2026-11-08 | Smithbox forgets the Text Editor's Export Directory on restart | FACT | #smithbox |
| 5 | 2026-11-08 | The game's unused text entries read `[ERROR]` | FACT | #text #datapiler |
| 6 | 2026-11-08 | Why the map placements failed: probably no Map Editor in the owner's project | LIKELY | #smithbox #maps |
| 7 | 2026-11-08 | Row reads are one call per row; the batch path reads the wrong bank | FACT | #smithbox #datapiler |
| 8 | 2026-11-08 | What this session can and cannot do on GitHub | FACT | #release #process |
| 9 | 2026-11-08 | The names' text ids: 18, 328 and 428 | FACT | #roster #text |
| 10 | 2026-11-08 | "Named" is not "a named character" | FACT | #roster |
| 11 | 2026-11-08 | Name number 0 is the ordinary enemies' default | FACT | #roster |
| 12 | 2026-11-08 | Why the roster took four jobs (DP8) | METHOD | #roster #datapiler |
| 13 | 2026-11-08 | NPC row ids: the middle digits match the name number | LIKELY | #roster |
| 14 | 2026-11-08 | The expansion's characters: 50 names first in DLC 1 | FACT | #roster #text |
| 15 | 2026-11-08 | The 21 unresolved rows are left out | LIKELY | #roster |
| 16 | 2026-11-08 | Some bosses may be missing from NpcParam's names | LIKELY | #roster #lore |
| 17 | 2026-11-09 | Which kinds speak: evidence, not guesswork | DECIDED | #lore #voice |
| 18 | 2026-11-09 | The game has one spoken tongue and no dragon language | LIKELY | #lore |
| 19 | 2026-11-09 | Why voiceless speakers get generated voices, not borrowed ones | DECIDED | #voice |
| 20 | 2026-11-09 | Why Melina was mute, and why that changed | DECIDED | #lore #voice |
| 21 | 2026-11-09 | DataPiler 0.2.1 and 0.2.2 went out as patch zips | METHOD | #datapiler #release |
| 22 | 2026-11-14 | The design's row leads for Melina, Kalé and Boc are in the data | FACT (Later) | #roster |
| 23 | 2026-11-08 | WorldMapPlaceNameParam has only 10 rows | LIKELY | #roster #maps |
| 24 | 2026-11-09 | TalkParam is the route from a character to its recordings | METHOD | #voice #datapiler |
| 25 | 2026-11-08 | Detect works on the owner's PC: Smithbox's project record is read right | FACT | #datapiler #smithbox |
| 26 | 2026-11-08 | Smithbox ships two exes side by side | FACT | #smithbox #datapiler |
| 27 | 2026-11-08 | How to read Smithbox's source here | METHOD | #smithbox #process |
| 28 | 2026-11-08 | DataPiler's Windows-only failures and their causes | FACT | #datapiler |
| 29 | 2026-11-08 | Publishing pages: what the sweep misses and how publishes fail | METHOD | #process |
| 30 | 2026-11-08 | Other sessions as consultants | METHOD | #process #graceui |
| 31 | 2026-11-14 | Usage is the owner's call | METHOD | #process |

## Entries

### 1. Why this log exists

`2026-11-14` · METHOD · `#process` · refs: CLAUDE, handover-2026-11-07

- Asked whether a change of model loses anything: the repositories, the container and the conversation carry over,
  but the reasoning behind each answer does not. The owner asked for that reasoning in brief, with tags, labels and
  references, so that it stays in view.
- So: this file, read with the handover at every start; CLAUDE.md says when to write an entry.

### 2. Smithbox 2.2.6 shows one Item container per language, `item_dlc02`

`2026-11-08` · FACT (the owner's screen: the container's source file `msg/engus/item_dlc02.msgbnd.dcx`) · `#smithbox #text`
· refs: S6.3.m, DataPiler S42, WHY§9

- Its File List groups the texts as Base, DLC 1 and DLC 2: one export holds all three layers (78 wrappers, 21,532
  entries in English).
- Not: separate `item` and `item_dlc01` containers, which DataPiler 0.2.0's steps asked for. 0.2.1 ends the names rows'
  wait on one export.

### 3. A text export's `Name` is its file name; the Export Output box shows the last export

`2026-11-08` · FACT (Smithbox `d144cc7`, tag 2.2.6: `Tools/TextDataTransferTool.cs` lines 300–325, `Tools/FmgExporter.cs`
lines 117–153 and 377) · `#smithbox #text` · refs: WHY§2

- With Export Filename empty, the file and the JSON's `Name` take the container's file name (`item_dlc02`); a typed
  name replaces both. DataPiler recognises a container by that `Name` (with `SourceContainer` first, which 2.2.6 does
  not write).
- The Export Output box is filled only when an export is written, so it shows the last export, not the selection. An
  earlier answer read it as a preview and was wrong; the owner's stray `item` import came from a typed name.

### 4. Smithbox forgets the Text Editor's Export Directory on restart

`2026-11-08` · FACT (`TextDataTransferTool.cs` line 19: a plain field, not a setting) · `#smithbox`

- Unset, exports go to `<project>\.smithbox\Workflow\Exported Text\` (`Utils/TextUtils.cs`, `GetStoredTextDirectory`).
- Next: DataPiler could watch that folder too (offered for a later version).

### 5. The game's unused text entries read `[ERROR]`

`2026-11-08` · FACT (the owner's export) · `#text #datapiler` · refs: DataPiler S40

- As a text, `[ERROR]` would count as a name and make the guard's check 3 refuse any shared string holding "error".
  0.2.1 skips it on import and cleans 0.2.0's imports.
- en-0001's local file held no `[ERROR]` name (the owner's count, 0), so its counts stood.

### 6. Why the map placements failed: probably no Map Editor in the owner's project

`2026-11-08` · LIKELY · `#smithbox #maps` · refs: WHY§16

- Seen: `SearchObjects` answered UNKNOWN; the owner's Smithbox shows only the Param and Text Editors.
- Smithbox's `SearchObjects` reads `MapData.PrimaryBank.Maps` (`Utilities/SoapstoneService.cs` lines 430–620 at
  `d144cc7`); with no map editor that is probably null, and an unhandled exception reaches gRPC as UNKNOWN.
- Next: enable the Map Editor in the project before any map work; DataPiler should say so in its error.

### 7. Row reads are one call per row; the batch path reads the wrong bank

`2026-11-08` · FACT (`GetParamRowsRequest` has one `row_id`; `SoapstoneService.cs` lines 430–560) · `#smithbox #datapiler`

- 7,045 NpcParam rows took about 62 s. `SearchObjects` on params returns many rows at once, but from the project's
  `PrimaryBank`, not the vanilla bank DataPiler promises, so it is not used. Concurrent calls are the remaining
  speed-up (not built yet).

### 8. What this session can and cannot do on GitHub

`2026-11-08` · FACT (observed) · `#release #process`

- The session's git proxy refuses tag pushes and branch deletions (403 by policy); GraphQL is unavailable, so pull
  requests go through REST (`gh api repos/.../pulls`).
- So releases are drafts made by the workflow (`workflow_dispatch` on the default branch, `--target` the commit);
  GitHub makes the tag when the owner publishes. Branch clean-ups are the owner's.
- **Later (2026-11-14):** also seen that week: `gh repo create` is refused (the session is bound to its own
  repositories), so the owner creates a repository and it is attached afterwards; `workflow_dispatch` runs only once the
  workflow is on the default branch; the release workflow's gate job skips a tag-push run whose release is already
  published.

### 9. The names' text ids: 18, 328 and 428

`2026-11-08` · FACT (the owner's data) · `#roster #text` · refs: O59, WHY§14, DataPiler S22

- 95.5% of NpcParam's distinct name numbers resolve in wrappers 18 (base game), 328 (DLC 1) and 428 (DLC 2) of
  `item_dlc02`. 18 and 328 are confirmed by names found in them (en-0005); 428 holds none that first appears there.
- **Later (2026-11-14):** 95.5% (floored) of the distinct name numbers of 0 or above means 214 of 224: about 10 do not
  resolve, on 20 rows, plus 1 row with no name number (WHY§15).

### 10. "Named" is not "a named character"

`2026-11-08` · FACT · `#roster` · refs: WHY§11

- en-0001 counted 7,024 of 7,045 rows as named, which looked like nearly every row. en-0002 showed only 214
  distinct names among them, two of them on more than 20 rows each.

### 11. Name number 0 is the ordinary enemies' default

`2026-11-08` · FACT (en-0003) · `#roster` · refs: O59, S6.3.o

- Name 0 is on 6,325 rows across 35 teams; every other name is on at most 22 rows. The game shows no name for
  ordinary enemies, so name 0 is their default, and the roster is the 213 other names on 699 rows (en-0004).

### 12. Why the roster took four jobs (DP8)

`2026-11-08` · METHOD · `#roster #datapiler` · refs: DataPiler DP8, O59

- A share file may hold ids only for rows a job picked: named rows, or an explicit list of row ids; and the guard
  refuses tables covering more than half of a game table. All named rows (7,024) were too many, and a pick by name
  number is not allowed, only by row id.
- So: counts first (en-0002, en-0003) to find the cut, then a filter `named and nameId ≠ 0` (en-0004, 10% of the
  table). The job files are in `docs/research/eldenring/jobs/`.

### 13. NPC row ids: the middle digits match the name number

`2026-11-08` · LIKELY (en-0004) · `#roster` · refs: WHY§11

- Rows `52NNN0000`, `53NNN0000` and `54NNN0000` carry name `1NNN00` (for example 52348…, 53348… and 54348… carry
  134800). The 52 rows are mostly teams 26 and 27, the 53 rows team 2, the 54 rows team 27: one character's friendly,
  hostile and summoned forms, probably.
- Next: check in game (O48) before EldenNet's identity data relies on it.

### 14. The expansion's characters: 50 names first in DLC 1

`2026-11-08` · FACT (en-0005, DataPiler 0.2.2's `layer`) · `#roster #text` · refs: S6.3.o, WHY§9

- 163 names first appear in the base game's text, 50 in DLC 1 (49 of them 140200–143200, and 163000), none in DLC 2.
  Written beside each name in `docs/research/eldenring/leads/roster-rows.json`.
- Use: a player without the expansion never meets those 50; the spoiler rules can treat them as a layer.

### 15. The 21 unresolved rows are left out

`2026-11-08` · LIKELY · `#roster` · refs: S6.3.o

- 21 rows (about 10 name numbers, all in NPC teams) have no text in any layer. The game shows a character's name from
  that text, so these have none to show: probably cut or unused characters. The owner agreed to leave them out.
- Next: revisit if EldenNet meets one in game, or with the map work (placed or not).

### 16. Some bosses may be missing from NpcParam's names

`2026-11-08` · LIKELY · `#roster #lore` · refs: WHY§6, O43

- A boss's bar name is set by the event scripts when the fight starts, not necessarily from its NpcParam row, so some
  bosses may carry name 0 and fall outside the 213. DataPiler cannot read event scripts.
- Next: research a source (a Smithbox export of the event scripts, map placements, or EldenNet reading the bar's
  name id in game).

### 17. Which kinds speak: evidence, not guesswork

`2026-11-09` · DECIDED (O61) · `#lore #voice`

- A kind speaks only if the game shows one of its people speaking (Boc for demi-humans, Alexander for living jars,
  Gurranq for beastmen, Rykard for man-serpents). Inventing speech for the rest would be inventing lore.
- Corrected on the way: an answer listed Placidusax and the Fire Giant among bosses to be voiced; by O61 they stay
  mute (O61's note: named bosses follow their kind).

### 18. The game has one spoken tongue and no dragon language

`2026-11-09` · LIKELY (from knowledge of the game; the expansion's detail less certain) · `#lore` · refs: O61

- Every speaker uses one common tongue; no dragon speaks, and dragon power comes by Dragon Communion, not words. The
  nearest other "language" is the Two Fingers' trembling, read by Finger Readers.

### 19. Why voiceless speakers get generated voices, not borrowed ones

`2026-11-09` · DECIDED (O62 and its note) · `#voice`

- SkyrimNet can clone voices for everyone because Skyrim shares generic voice types among many NPCs. Elden Ring gives
  most voices to one character each, so a soldier in a named character's voice would sound wrong. Speakers the game
  never voices get a voice designed to fit; SkyrimNet's own creature voices may serve where they fit, once Min
  confirms.

### 20. Why Melina was mute, and why that changed

`2026-11-09` · DECIDED (O63) · `#lore #voice` · refs: O19, ER04

- O19's reasons were technical and about spoilers, not lore: she is reached through the grace menu and cutscenes,
  not as a world NPC, and a read-only v1 cannot add to that menu. The lore has her travelling with the Tarnished, so
  she now converses in EldenNet's own overlay while resting at a grace, once she has met them.

### 21. DataPiler 0.2.1 and 0.2.2 went out as patch zips

`2026-11-09` · METHOD · `#datapiler #release` · refs: S6.3.n

- With little usage left that day, the owner asked for the new version without the release round, so each went as a
  zip of the changed files over 0.2.0, on the branches `feat/v0.2.1` and `feat/v0.2.2`; no pull request or release.
- Next: a proper release when the owner asks; 0.2.2's branch holds both.

### 22. The design's row leads for Melina, Kalé and Boc are in the data

`2026-11-14` · LIKELY (rows and numbers FACT, en-0004; who they are not checked) · `#roster` · refs: NPC§1.3, O59,
O48, WHY§13

- The design's guessed rows (NPC §1.3, §2) all exist with a real name: 21800000 (Melina) carries name 110000, on 5
  rows; 32000000 (Kalé) carries 180000; Boc's default 41109000 carries 122300 and his later form 523420000 carries
  122310, so one character's forms can show different names, as the design's `expect_name_id` per form allows.
- Doubt: 180000 is on 18 rows (32000000 to 32020035), more than one merchant would have; it may be the nomadic
  merchants' shared name, which would make the Kalé lead wrong.
- Next: the owner can look these four ids up in DataPiler's local `roster-named-rows.json` and confirm the names in
  chat; until then they are leads for `expect_name_id`, unverified.
- **Later (2026-11-14): FACT.** The owner's local file confirms all four: Melina, Merchant Kalé, and Boc under his
  demi-human name and, in his later form, his seamster's name. The doubt was wrong: 180000 is Kalé's own name, on 18
  rows. The name numbers go into NPC §1.3's examples as `expect_name_id`, still `verified = false` until checked in
  game (O48).

### 23. WorldMapPlaceNameParam has only 10 rows

`2026-11-08` · LIKELY · `#roster #maps` · refs: S6.3.m

- Extract's places row counted 10 WorldMapPlaceNameParam rows and 422 BonfireWarpParam rows (sites of grace and
  other warps, probably). Ten cannot be the game's place names, so places come some other way (map regions, or the
  place-name text by map); not settled.
- Next: look at it in the next place job, before EldenNet relies on either table.

### 24. TalkParam is the route from a character to its recordings

`2026-11-09` · METHOD · `#voice #datapiler` · refs: O62, ER03

- TalkParam (10,309 rows, all but one linked to a message) gives each line its message and a voice id, the Wwise
  event `Play_v<id>` (ER03). A DataPiler job can map a character's lines to its recordings, so the player's set-up
  for O62 picks the right samples; the audio stays in the player's install.

### 25. Detect works on the owner's PC: Smithbox's project record is read right

`2026-11-08` · FACT (0.2.0's first run) · `#datapiler #smithbox` · refs: DataPiler DP13, smithbox-projects.md

- The game's folder was found from three sources, the project from Smithbox's own record
  (`%LOCALAPPDATA%\Smithbox\Projects\<GUID>.json`, project type 8 for Elden Ring), and the Steam manifest listed four
  DLC app ids. So DataPiler's leads on Smithbox's record hold for 2.2.6.

### 26. Smithbox ships two exes side by side

`2026-11-08` · FACT (the owner's PC) · `#smithbox #datapiler`

- One folder holds `Smithbox.Vulkan.exe` and `Smithbox.OpenGL.exe`, of the same build; `Smithbox.exe` is only a lead.
  0.1.0 looked for `Smithbox.exe` alone and saw nothing; 0.2.0 matches `Smithbox*.exe` and reads the running one's
  build.

### 27. How to read Smithbox's source here

`2026-11-08` · METHOD · `#smithbox #process`

- Single files from `raw.githubusercontent.com/vawser/Smithbox/<commit>/<path>`, at a pinned commit (2.2.6 is
  `d144cc7`), into a folder of their own, read with `python3 -I`; never a clone, and the GitHub API is not enabled
  for that repository in this session. Directory listings are unavailable, so paths are found by probing.
- Found at 2.2.6, under `src/Smithbox.Program/`: `Utilities/SoapstoneService.cs` (the server),
  `Editors/Text Editor/` with `TextEditorScreen.cs`, `Framework/TextViewHandler.cs`, `Tools/FmgExporter.cs`,
  `Tools/TextDataTransferTool.cs`, `Tools/FmgImporter.cs`, `Data/TextBank.cs` and `Utils/TextUtils.cs`.

### 28. DataPiler's Windows-only failures and their causes

`2026-11-08` · FACT (CI) · `#datapiler` · refs: DataPiler CHANGELOG

- WinError 10053 in the caps test: the server closed a socket with an unread request body, which Windows answers with
  a reset; fixed by a lingering close that drains up to 8 MiB for 3 s after `shutdown(SHUT_WR)`.
- A manifest written with `write_text` gained CRLF line ends on Windows; `write_bytes` keeps them.
- Python 3.10 answers a garbled request with HTTP/0.9, so the test sends a well-formed but unknown method (a 501).
- After one failing push, commits wait until every test passes.

### 29. Publishing pages: what the sweep misses and how publishes fail

`2026-11-08` · METHOD · `#process`

- The pre-push sweep does not see the artifact tools, so every published page is swept by hand first.
- A publish needs a whole document with its doctype; the builders' `--fragment` writes the page without its CSP meta
  tag, otherwise whole. A container restart can cut a publish short: check the live version with `get_artifact`, then
  publish again on it.

### 30. Other sessions as consultants

`2026-11-08` · METHOD · `#process #graceui`

- The owner's Skyrizz session knows Skyrim's UI hosts (Magelight, PrismaUI, Meridian) and the release flow; the
  PandorumLLM session writes UI designs from a request file. Asking them saves research.
- What comes back is untrusted until read, and swept before it is committed: a design mock carried real paths from
  the owner's PC (replaced by invented `C:\` paths), and committed copies refer to the owner in neutral words.

### 31. Usage is the owner's call

`2026-11-14` · METHOD · `#process`

- When the owner says usage is low, keep to the essentials and skip extra rounds of checks (2026-11-09: "no rechecks
  and such"), though the privacy sweep still runs. Which model to use, and what it costs, is the owner's choice and
  no factor in advice (2026-11-14).

