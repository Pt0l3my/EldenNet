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
| 32 | 2026-11-14 | Smithbox 2.2.6 has no event-script editor | FACT | #smithbox #roster |
| 33 | 2026-11-14 | Most of the base game's bosses are missing from NpcParam's names | LIKELY | #roster |
| 34 | 2026-11-14 | Bosses sit in entity slots ending 800 to 899 | LIKELY | #roster #maps |
| 35 | 2026-11-14 | Smithbox lists every map as if loaded; DataPiler 0.2.x misread it | FACT | #smithbox #maps #datapiler |
| 36 | 2026-11-14 | The two routes to the bosses, and why both | METHOD | #roster #datapiler |
| 37 | 2026-11-14 | Only Smithbox's plain Map Editor makes the map data its server reads | FACT | #smithbox #maps |
| 38 | 2026-11-14 | A whole text container makes ordinary words look like game text | FACT | #datapiler #text |
| 39 | 2026-11-14 | The map UNKNOWN was DataPiler's own request | FACT | #smithbox #maps #datapiler |
| 40 | 2026-11-14 | Smithbox loads maps only while its Map Editor is drawn | FACT | #smithbox #maps |
| 41 | 2026-11-14 | Smithbox loads one map at a time; ten at once kept one | FACT | #smithbox #maps #datapiler |
| 42 | 2026-11-14 | Where a map load's time goes, and why more PC does not help | FACT | #smithbox #maps |
| 43 | 2026-11-14 | One map that never loads held up the rest | LIKELY | #smithbox #maps #datapiler |
| 44 | 2026-11-14 | What DataPiler can and cannot learn from Smithbox about a failed map | FACT | #smithbox #maps #datapiler |
| 45 | 2026-11-14 | en-0007's full load: 46 of 47 maps, one empty, and Smithbox's auto-save | FACT | #smithbox #maps #datapiler |
| 46 | 2026-11-14 | What the legacy dungeons' boss slots hold | FACT | #roster #maps |
| 47 | 2026-11-14 | Bosses' bar names are numbered by their model | LIKELY | #roster #text |
| 48 | 2026-11-14 | The other unused names: 13 in the named range, 16 of one unknown family | LIKELY | #roster #text |

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
- **Later (2026-11-14):** wrong cause. The UNKNOWN came from DataPiler's request, which lacked its search clause
  (WHY§39); the Visual Map Editor is still needed for the map data (WHY§37).

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

### 32. Smithbox 2.2.6 has no event-script editor

`2026-11-14` · FACT (`Editors/Project Editor/JSON/ProjectDescriptor.cs` at `d144cc7`) · `#smithbox #roster` · refs:
WHY§16, DESIGN§7.8

- A project's editors are Param, Text, Map, MapData, Model, Material, Anim, Gparam and Havok; none reads the event
  scripts, where a boss's bar name is set when its fight starts. So bar names cannot come through Smithbox, and
  DataPiler never opens the game's archives itself (no keys, no Oodle).

### 33. Most of the base game's bosses are missing from NpcParam's names

`2026-11-14` · LIKELY (en-0003) · `#roster` · refs: WHY§16, WHY§14

- Of the 213 real names, 49 appear on enemy-team (6) rows, mostly characters who also have a friendly form; only 14
  appear on enemy rows alone, 12 of them from DLC 1 (140903 to 143200) and 2 from the base game (121610, 121800). The
  base game has far more bosses than that, so most of them probably carry name 0 and get their bar name from the
  event scripts.
- Next: a route to the bar names (put to the owner, 2026-11-14): the names no character row uses, map placements,
  or EldenNet reading the bar in game.

### 34. Bosses sit in entity slots ending 800 to 899

`2026-11-14` · LIKELY (FromSoftware's habit, as modders know it; not checked in our data) · `#roster #maps` · refs:
DataPiler S46, WHY§36

- A map's boss usually has the entity id of its map block followed by 800 (for example a castle's boss at …0800 and
  a second at …0850). DataPiler 0.2.3's boss-slot pick keeps placements ending 800 to 899, so it may catch some
  helpers and miss a boss numbered otherwise.
- Next: en-0007's rows, checked against which bosses live in which dungeon, then in game (O48).
- **Later (2026-11-14):** en-0009 loaded the tutorial dungeon's map (m18_00_00_00, 53 placed characters): two
  boss slots, entity ids 18000800 and 18000850 (rows 46400007 and 43113906, models 4640 and 4311), as many as the
  bosses that map is known to hold. Neither row is among the 699 named rows, so both carry name 0, as WHY§33 expected.
- **Later (2026-11-14):** en-0007 found boss slots in 24 of the 27 legacy-dungeon maps that hold characters (WHY§46).
  The slots also hold helpers and adds, as feared, so a boss is told by its model and its count, not by its slot alone.

### 35. Smithbox lists every map as if loaded; DataPiler 0.2.x misread it

`2026-11-14` · FACT (`Utilities/SoapstoneService.cs`, `SearchObjects`, at `d144cc7`) · `#smithbox #maps #datapiler` ·
refs: DataPiler S44, WHY§6

- A file search over the map bank returns every map Smithbox knows, loaded or not; entries come only from loaded maps.
  DataPiler 0.2.x took the file search for the loaded maps, so it would have read every map as loaded at once and
  found no placements. 0.2.3 tells a loaded map by an entry search, and gains the list of all maps for free.
- The first map run's UNKNOWN came before any of this (WHY§6): the project's missing Map Editor.

### 36. The two routes to the bosses, and why both

`2026-11-14` · METHOD · `#roster #datapiler` · refs: WHY§32, WHY§33, WHY§34, DESIGN§7.7

- (a) en-0006: the character names no NpcParam row uses. A boss whose row carries name 0 shows its bar name from
  those entries, so they hold the bar names EldenNet binds bosses by, among unused or cut names.
- (b) en-0007: the characters placed in boss slots of the legacy dungeons. They give the bosses' rows, entity ids
  and models; EldenNet can bind by entity id as well (DESIGN §7.7).
- Neither links a boss's row to its bar name; the event scripts do, and Smithbox cannot read them. EldenNet sees both
  at once in game. Until then the two lists, with what is known of where each boss lives, are leads.
- Risk: check 6 refuses en-0006's ids if more than half of the names are unused; the card then still shows the
  count, and the next step is the owner's.
- **Later (2026-11-14):** the two routes meet after all, though not through the event scripts: most unused names are
  numbered by a model, and the boss slots give each boss's model (WHY§47). That narrows each boss to a few candidate
  bar names; the binding itself still waits for the game.

### 37. Only Smithbox's plain Map Editor makes the map data its server reads

`2026-11-14` · FACT (`Editors/Project Editor/ProjectEditorHandler.cs` lines 136–140 and 272–275 at `d144cc7`) ·
`#smithbox #maps` · refs: WHY§6, WHY§35

- `MapData`, which `SearchObjects` reads, is made only when the project's `EnableMapEditor` is on, and the Map Editor
  screen only when `MapData` exists. "Map Data Editor" (`EnableMapDataEditor`) is another editor. The owner first
  ticked Map Data Editor, and en-0007 met UNKNOWN again.
- So: tick the Map Editor in the project's Available Editors, press Update Project, restart Smithbox, keep its Map
  Editor tab open while a map job runs.

### 38. A whole text container makes ordinary words look like game text

`2026-11-14` · FACT (en-0006 and Copy for sharing, DataPiler 0.2.3) · `#datapiler #text` · refs: DataPiler DP18, DP17

- With the whole English item container imported (21,532 entries, descriptions included), check 3 found the table
  name `unused` inside some description and refused the share file, and refused a log copy for one line that listed
  file names. The names are the only texts DataPiler writes, so the owner narrowed check 3 to them (DP18).
- The same run showed 269 of 483 character-name entries unused (56%), over check 6's half; the owner let such
  number-only lists be shared whole (DP17). Job en-0008 runs en-0006 again on 0.2.4.

### 39. The map UNKNOWN was DataPiler's own request

`2026-11-14` · FACT (SoapstoneLib's `SoapstoneServer.cs` lines 108–121 and 223–227, at Smithbox `d144cc7`) ·
`#smithbox #maps #datapiler` · refs: WHY§6, WHY§37, DataPiler S51

- Smithbox's server library checks `request.Search` is set and throws otherwise, which a client sees as UNKNOWN.
  DataPiler's map listing sent no search clause from 0.1.0 on, so every map job failed, with or without the Visual
  Map Editor; the fake server in its tests answered such requests, so nothing caught it.
- 0.2.5 sends an empty clause (it goes on the wire as an empty field, checked), and the fake refuses a search without
  one. The rest of the map search was checked in the same source: the resource needs a type and a game, and the map
  key template is one the library knows.
- Lesson: a fake must refuse what the real server refuses; read the server's own argument checks before trusting a
  fake.

### 40. Smithbox loads maps only while its Map Editor is drawn

`2026-11-14` · FACT (`Editors/Map Editor/MapEditorStub.cs` lines 23–75, `Framework/MapCommandQueue.cs` lines 22–61, at
`d144cc7`) · `#smithbox #maps` · refs: WHY§39, DataPiler S52

- `OpenResource` queues `map load <id>`; Smithbox takes one queued command per frame and passes a "map" command to the
  Map Editor's `OnGUI` only while that editor's window is drawn (`ImGui.Begin` true), which then calls `LoadMap`. With
  Smithbox minimised or another tab in front, nothing loads: en-0007's first batch timed out after 900 s that way.
- Also learned: DataPiler's loaded check is sound (an empty search matches every entry, and map entries are a key type
  the library knows), and the old per-batch limit would have failed big dungeons anyway; 0.2.6 waits as long as maps
  keep loading and shows the progress.
- Next: en-0009 loads one small map family first, to see loading work and time it, then en-0007.
- **Later (2026-11-14):** en-0009 worked with the Map Editor in front: map loading through DataPiler is confirmed.

### 41. Smithbox loads one map at a time; ten at once kept one

`2026-11-14` · FACT (`Editors/Map Editor/MapUniverse.cs` lines 43–86, at `d144cc7`) · `#smithbox #maps #datapiler` ·
refs: WHY§40, DataPiler S53

- `LoadMap` refuses while another map is loading ("map already loading") unless a caller allows concurrent loads,
  which the command path never does. DataPiler 0.2.3 to 0.2.6 asked for ten at once, so Smithbox loaded the first and
  dropped nine, and the job waited for them in vain (en-0007's second run: 1 of 49 maps, then no more).
- 0.2.7 asks for one map at a time and asks again every 10 s until Smithbox takes it; the fake server refuses a load
  while another runs, so the tests now cover it.
- Parallel jobs ("array jobs", the owner's idea) cannot help here, for the same reason; they do help row reads, which
  Smithbox answers each on its own (0.2.7 reads four at once).
- **Later (2026-11-14):** confirmed by en-0007's third run on 0.2.6: 1 of 49 maps loaded within the first minute, then
  none for 15 minutes, as predicted. Timings from the same log: the tutorial map took 6.2 s (en-0009), the first
  legacy-dungeon map under a minute.

### 42. Where a map load's time goes, and why more PC does not help

`2026-11-14` · FACT (`MapUniverse.cs` lines 64–80 and 120–245, at `d144cc7`) · `#smithbox #maps` · refs: WHY§41

- Before the async load, Smithbox loads the map's Havok collision unless `MapEditor_SkipHavokLoad` is on; then, on the
  Vulkan build with `Viewport_Enable_Rendering` on, every model and (with `Viewport_Enable_Texturing`) every texture.
  The OpenGL build never loads map models or textures. DataPiler needs only the MSB's placed characters.
- The owner saw about 3.5 minutes for the first legacy-dungeon map with the PC nearly idle: Smithbox's own one-at-a-time
  work, not a lack of CPU, GPU or memory, so DataPiler cannot speed it up by using more of the PC.
- Next: time en-0009 with rendering off and Havok skipped (or on the OpenGL build) against the first run.
- **Later (2026-11-14):** with Enable Model Rendering off, the first map of en-0007's fourth run loaded in 2 s.

### 43. One map that never loads held up the rest

`2026-11-14` · LIKELY · `#smithbox #maps #datapiler` · refs: WHY§41, WHY§42, DataPiler S57

- en-0007 on 0.2.7: map 1 loaded in 2 s, map 2 never did in 15 minutes although DataPiler asked again every 10 s and
  Smithbox kept answering. Smithbox clears its loading state in `finally` (`MapUniverse.cs` lines 236–266), so it was
  not stuck loading map 1; the likely cause is that map 2's own load fails every time, its exception kept by Smithbox.
- DataPiler's log did not name the map, so it could not be looked up. 0.2.8 names each map, skips one not loaded
  after 5 minutes, leaves it out of the placements and stops only after three skipped in a row.
- Also found that day: 0.2.7's newest-first order sorted on a time the page never received (fixed in 0.2.8).
- Next: the map 0.2.8 skips, and what Smithbox's own log says when it fails.

### 44. What DataPiler can and cannot learn from Smithbox about a failed map

`2026-11-14` · FACT (`Smithbox.cs` lines 120–160, 228–250 and 485–495; `Application/CFG.cs` lines 1415–1465; at
`d144cc7`) · `#smithbox #maps #datapiler` · refs: WHY§43, DataPiler DP19, S58, S59

- Smithbox's log goes to its own windows (an in-memory provider behind the Action Log and Warning Log); no file and no
  server call carry it, so DataPiler cannot read why a map failed. The owner reads the Warning Log; DataPiler names the
  map and when.
- Its preferences live in `Configuration.json`, written when Smithbox closes or saves its configuration. DataPiler
  0.2.9 reads two of them (DP19) to set the first map's limit, and corrects from the first measured load on.
- A timeout is the only failure signal; with model rendering off a map loads in about 2 s, so 10 s a map is safe,
  with a second try at the end for any skipped.
- **Later (2026-11-14):** where the owner finds it (`Logger/GUI/LoggerUI.cs` lines 95–160): two small arrow buttons in
  Smithbox's top menu bar, after Help, open the Action Log and the Warning Log; each log window has Clear Log and Copy
  to Clipboard. en-0007 on 0.2.9 runs, skipping some maps after the short limit.

### 45. en-0007's full load: 46 of 47 maps, one empty, and Smithbox's auto-save

`2026-11-14` · FACT (DataPiler's and Smithbox's logs, 0.2.9; `SoapstoneService.cs` lines 192–215 at `d144cc7`) ·
`#smithbox #maps #datapiler` · refs: WHY§43, WHY§44, DataPiler S60, S61

- With model rendering off, 47 legacy-dungeon maps took about 13 minutes: small ones 2 s, big ones 20 to 55 s
  (Leyndell's m11_00_00_00 first skipped at 10 s, then counted as it loaded late). Smithbox's "a map is already in the
  process of loading" lines are DataPiler's 2 s re-asks, harmless.
- m10_01_00_99 never showed as loaded, yet Smithbox's auto-save named it among its loaded maps from the 0.2.7 run on:
  it loads with nothing an entry search lists (LIKELY an empty map), so WHY§43's "map that never loads" was this map,
  not a failing load.
- Reading placements then stopped on a placed character without TalkID: Smithbox leaves out a property a part lacks;
  0.2.10 reads it as -1.
- Smithbox's auto-save (every 5 minutes) wrote every loaded map, and the params, into the project folder ("Saved map:
  ..."): unchanged copies of the game's own, but files in the owner's Smithbox project, outside the game. DataPiler
  never asks Smithbox to save; loading maps for research makes them part of the project's auto-save.
- Also: thousands of "Unable to allocate texture descriptor" lines while 50 maps were loaded; restarting Smithbox
  after a map job frees what they hold.

### 46. What the legacy dungeons' boss slots hold

`2026-11-14` · FACT (en-0007 on DataPiler 0.2.10; `leads/boss-leads.json`) · `#roster #maps` · refs: WHY§34, WHY§33,
WHY§45, roadmap S6.3.p

- 50 maps read (m10_01_00_99 asked for twice and left out, as WHY§45 expected: the card's "50 of 51" counts it); 27
  hold characters, 24 of them boss slots. 141 placements in the slots, 46 models: 44 at …800 to …809, 23 at …850 to
  …859, the rest at …810 to …849 and …860 to …869.
- Not all are bosses. Four models fill most of the helpers' sub-ranges at once (24, 15, 8 and 4 of a kind), and
  twelve maps put rows at both …800 and …801, of one model or of two numbered one apart (a boss and its other form
  or partner, LIKELY). Five models with bar names sit in boss slots of two maps each, so one body serves two bosses
  or one boss met twice.
- 13 slot placements have rows that carry a real name in NpcParam (three names, 132400, 132500 and 120000), likely
  characters who also appear in another role, as WHY§33 expected; 120000 is on both rows of one two-form boss.
- The last two digits of a slot's row id are, with few exceptions, one value per map (LIKELY an area code), and rows
  ending 00 are shared base rows. Noted, never relied on.

### 47. Bosses' bar names are numbered by their model

`2026-11-14` · LIKELY (en-0007 and en-0008 side by side; the event scripts that state the link are out of reach,
WHY§32) · `#roster #text` · refs: WHY§36, WHY§46, O48, O59, DESIGN§7.7

- 240 of the 269 character names no NpcParam row uses are numbered 90, then a four-digit model, then three digits
  (102 models). Of the 45 non-human models in the boss slots, 34 have such names. The 11 without are a second model
  beside a named one (four cases, its number one off: a boss's other form under one bar, LIKELY), the four
  many-of-a-kind helpers of WHY§46, and three single placements.
- The layers agree with it: every expansion model (5000 and up) in the slots has its names in DLC 1's layer, every
  base-game model in the base game's; the human model's names (900000…) are in both.
- Variants: 000 to 002 look like a dungeon's own boss and its forms; 300s, 500s and 600s likely the same kind of foe
  met elsewhere (SPECULATION). 67 named models are in no legacy-dungeon slot: bosses of the smaller dungeons, the
  evergaols and the open world.
- So the leads give each boss a few candidate bar names. EldenNet's in-game check (S2 records the bar and its holder)
  confirms one, and nothing is bound by the pattern alone (O48).
- Next: en-0010 (the smaller dungeons, m30 to m45); the open world later, in parts, with Smithbox restarted between
  jobs and its auto-save off.

### 48. The other unused names: 13 in the named range, 16 of one unknown family

`2026-11-14` · LIKELY (en-0008) · `#roster #text` · refs: WHY§33, WHY§47

- 13 six-digit numbers, in the range of named characters' names (6 base game, 7 DLC 1), are used by no row. They are
  likely names shown only by event scripts (a named character's bar name, or a name revealed later); 121810 sits next
  to 121800, a base-game name only enemy rows carry.
- 16 eight-digit numbers form one family: 101 to 117 (no 106), then five digits, all from the base game. What they
  name is unknown (SPECULATION: not characters the roster needs).
- The owner can tell their kind apart by looking a few up in DataPiler's local workspace; the names stay on the PC
  and in chat, never here.
