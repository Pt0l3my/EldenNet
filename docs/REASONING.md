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
| 49 | 2026-11-14 | The smaller dungeons hold the model pattern: 61 of 101 named models placed | FACT | #roster #maps |
| 50 | 2026-11-14 | Empty tiles looked like failed loads; Smithbox's file keys tell them apart | FACT | #smithbox #maps #datapiler |
| 51 | 2026-11-14 | The graphics driver went down with Smithbox holding about 720 maps; jobs now keep what they read | LIKELY | #smithbox #maps #datapiler |
| 52 | 2026-11-14 | What Smithbox puts on the graphics card for a map, and the OpenGL build as the way round | FACT | #smithbox #maps #datapiler |
| 53 | 2026-11-14 | The OpenGL build's first 300 maps: no texture warnings; one-go jobs, Resume job and an icon | LIKELY | #smithbox #maps #datapiler |
| 54 | 2026-11-14 | The open world's boss slots: 98 of 101 named models placed | FACT | #roster #maps |
| 55 | 2026-11-14 | The named characters' flag blocks, and the jobs for their placements and forms | METHOD | #roster #flags #datapiler |
| 56 | 2026-11-14 | The map: the game's map only as the player's own copy; trails are sightings; masking against one upload | METHOD | #graceui #map #spoilers #licences |
| 57 | 2026-11-14 | The named characters' census: placements, forms and the talk id's two patterns | FACT | #roster #datapiler #talk |
| 58 | 2026-11-14 | Where the map's coordinates can come from: five map tables, but no placement positions through Smithbox | FACT | #map #datapiler #graceui |
| 59 | 2026-11-14 | The world map's frame: about one map unit per metre, north up, three sheets | LIKELY | #map #datapiler #graceui |
| 60 | 2026-11-15 | The areas' colour keys: 16 area names and 49 finer areas on the base map, 11 names in the expansion | LIKELY | #map #datapiler #graceui |
| 61 | 2026-11-15 | The game's map tiles: four sheets, five zoom levels, a variant per revealed piece | LIKELY | #map #datapiler #graceui |
| 62 | 2026-11-15 | Each tile code bit is one map piece, and map units are level-0 pixels | LIKELY | #map #datapiler #graceui |
| 63 | 2026-11-15 | A tile's pieces that never meet have a variant each, laid together | FACT | #map #datapiler #graceui |
| 64 | 2026-11-15 | Level 2's export holds a second drawing of the map; levels 2 to 4 nest by two | LIKELY | #map #datapiler #graceui |
| 65 | 2026-11-15 | With the check, level 2 is one image; its base and pieces 15 to 20 stand apart | LIKELY | #map #datapiler #graceui |
| 66 | 2026-11-15 | Level 2's export is two tile sets under one set of names; Smithbox's export overwrites by name | LIKELY | #map #datapiler #graceui |
| 67 | 2026-11-15 | The game's container holds both level-2 sets; the one that fits levels 0, 3 and 4 is incomplete | FACT | #map #datapiler #graceui |
| 68 | 2026-11-15 | Level-1 codes do not settle levels 0 and 1; set A is not level 1 at a quarter | FACT | #map #datapiler #graceui |
| 69 | 2026-11-15 | Every base-map level checked: level 0 is mixed like level 2, level 1 hardly passes | FACT | #map #datapiler #graceui |
| 70 | 2026-11-15 | Level 1 sits at the top of its grid; levels 0 and 1 hold an old tile set four columns off | FACT | #map #datapiler #graceui |
| 71 | 2026-11-15 | The expansion's levels 1 and 2 sit at the top left; level 2 is 2,089 / 10,496 of level 0; the old set fills 960 places | FACT | #map #datapiler #graceui |
| 72 | 2026-11-15 | The game's tile masks are a KRAK-packed binder of four files, one per sheet; DataPiler opens it with the game's Oodle | FACT | #map #datapiler #smithbox |
| 73 | 2026-11-15 | The game's masks name today's tiles; levels 3 and 4 are an older layout; one geometry on every sheet | FACT | #map #datapiler #graceui |
| 74 | 2026-11-15 | en-0029: today's tiles match at levels 0 and 1 and on the expansion; level 2 met the older layout's level 3 | FACT | #map #datapiler |
| 75 | 2026-11-15 | en-0030: levels 1 and 2 match on every sheet; level 0 against level 1 drifts from the top left | FACT | #map #datapiler |
| 76 | 2026-11-15 | Every level of every map draws whole from the masks; the levels' true sizes are 0.74857 and 0.19875 | FACT | #map #datapiler #graceui |
| 77 | 2026-11-15 | en-0031: at the measured sizes every variant the game draws passes, at every level of every map | FACT | #map #datapiler |
| 78 | 2026-11-15 | The world-to-map fit: one transform per open-world sheet; the underground and the dungeons go through the game's conversion | FACT | #map #datapiler #graceui |
| 79 | 2026-11-15 | The areas' colour image is very likely the map-information binders, one per kilometre square | LIKELY | #map #datapiler #graceui |

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
- **Later (2026-11-14):** the empty map is handled since DataPiler 0.2.11, which tells loaded maps by their file keys
  (WHY§50).

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
- **Later (2026-11-14):** en-0010 holds the pattern (WHY§49). The era check goes by map, not by model number: model
  7100 is placed in two base-game catacombs and its names are in the base game's layer, so "5000 and up" above was a
  gloss, not the rule.

### 48. The other unused names: 13 in the named range, 16 of one unknown family

`2026-11-14` · LIKELY (en-0008) · `#roster #text` · refs: WHY§33, WHY§47

- 13 six-digit numbers, in the range of named characters' names (6 base game, 7 DLC 1), are used by no row. They are
  likely names shown only by event scripts (a named character's bar name, or a name revealed later); 121810 sits next
  to 121800, a base-game name only enemy rows carry.
- 16 eight-digit numbers form one family: 101 to 117 (no 106), then five digits, all from the base game. What they
  name is unknown (SPECULATION: not characters the roster needs).
- The owner can tell their kind apart by looking a few up in DataPiler's local workspace; the names stay on the PC
  and in chat, never here.
- **Later (2026-11-14):** 10 of the 13 share their first four digits with real names on rows (the same character's
  other names, LIKELY). 130901 sits beside 130900, the name on two human rows in one cave's boss slots (WHY§49): a
  likely bar name for that character.

### 49. The smaller dungeons hold the model pattern: 61 of 101 named models placed

`2026-11-14` · FACT (en-0010 on DataPiler 0.2.10; `leads/boss-leads.json`) · `#roster #maps` · refs: WHY§46, WHY§47,
WHY§48, roadmap S6.3.p

- 71 maps of m30 to m45 hold characters, 60 of them boss slots: 115 placements, 42 models. Most dungeons have one
  boss at …800 (91 placements at …800 to …809), with helpers at …810 to …819 and a few second bosses at …850.
- 35 of the 41 non-human models have names of WHY§47's shape. The six without are again helpers (4 and 3 of a kind),
  the two odd single models of en-0007 (rows 10000000 and 1000000, likely stand-ins), and two numbered near a named
  model (3701 by 3700, 4603 by 4600). Every model in a base-game map has its names in the base game's layer, every
  model in an expansion map (m40 to m43) in DLC 1's: no exception in 256 slots.
- With en-0007: 76 non-human slot models, 61 with candidate bar names; 61 of the 101 named non-human models are now
  placed. The other 40 (25 base game, 15 expansion) are in no dungeon slot: likely the open world's bosses.
- Characters without boss slots: three m34 maps, five expansion maps (m40_02, m42_00 to m42_03) and the three m45
  maps, which hold one character each, numbered outside 800 to 899. 12 slot placements carry real row names: 130900,
  137600 and 142700 (nine of one, at m41_02's …800 and …810 to …817).
- Next: the open world, one job per family so no tile is missed: en-0011 (m60, the base game) and en-0012 (m61, the
  expansion), Smithbox restarted before each. If one is too much for Smithbox at once, it is split by tiles.

### 50. Empty tiles looked like failed loads; Smithbox's file keys tell them apart

`2026-11-14` · FACT (DataPiler's and Smithbox's logs of en-0011 on 0.2.10; `SoapstoneService.cs` `GetObjects` and
`GetServerInfo`, `Universe.cs` `LoadMapAsync` line 180, `SoulsKey.cs` `ParseFileKey`, at `d144cc7`) ·
`#smithbox #maps #datapiler` · refs: WHY§45, WHY§43, DataPiler S62, DP4

- en-0011 (m60, 829 maps) stopped twice at the same place, 452 of 829: three tiles in a row (m60_43_56_00 to
  m60_43_58_00, with m60_42_56_00 and two others skipped earlier) never showed as loaded. Smithbox's log shows each
  of them loading at once with nothing but its texture jobs: tiles with nothing in them. An entry search lists no
  entry for such a map, so 0.2.3 to 0.2.10 took it for a failed load, and three in a row for a stuck Smithbox.
- Smithbox sets a map's container once its MSB is read, entries or none, and answers a map's file key
  (`map/mapstudio/<id>.msb`) through GetObject and BatchGetObjects only while that container exists. Both calls are on
  DP4's list, so DataPiler 0.2.11 asks BatchGetObjects which maps are loaded, a few hundred keys a call. GetServerInfo
  also lists the loaded maps, but it is not on the list, and it carries the paths of Smithbox and the project.
- Also in those logs: Smithbox failed to read a few tiles' navmesh files (NVA), caught inside its loader, harmless to
  the read; its auto-save still ran, writing every loaded map into the project folder every five minutes.
- Next: en-0011 again on 0.2.11, then en-0012.

### 51. The graphics driver went down with Smithbox holding about 720 maps; jobs now keep what they read

`2026-11-14` · LIKELY (DataPiler's log of en-0011 on 0.2.11; Smithbox's logs of the runs before; `Universe.cs`
`LoadMapAsync` at `d144cc7`) · `#smithbox #maps #datapiler` · refs: WHY§50, WHY§45, DataPiler S63

- en-0011's third run loaded 718 maps in 33 minutes, then Smithbox (its Vulkan build) and the GPU's driver went down
  while loading m60_51_56_00, and the job failed with UNAVAILABLE. The owner's PC has had no driver crash in games or
  AI inference.
- DataPiler never touches the GPU; it only sends requests to Smithbox. Smithbox keeps every loaded map. On the Vulkan
  build it prepares each map's model and texture lists even with rendering off, and refreshes its textures after
  every load. Its log showed thousands of "Unable to allocate texture descriptor" lines from about 400 open-world maps
  (and after a few dozen legacy-dungeon maps). So Smithbox running out of graphics resources with hundreds of maps
  held is the likely cause, not the hardware. Windows' Event Viewer (System log, near 19:17) would say which driver
  reset; not checked yet.
- DataPiler 0.2.12 (S63): each map's placements are read as soon as it loads and kept on disk at once, so a crash
  costs only the map in hand; and a job pauses once Smithbox holds 300 maps, asking for a restart of Smithbox. The
  0.2.11 run's maps were not kept: that version had no such file.

### 52. What Smithbox puts on the graphics card for a map, and the OpenGL build as the way round

`2026-11-14` · FACT (`Universe.cs` `LoadMapAsync` lines 124–246 and `Smithbox.cs` lines 60–121 at `d144cc7`; the
owner's Smithbox folder holds both builds, DataPiler S1x facts of 2026-11-08) · `#smithbox #maps #datapiler` · refs:
WHY§51, DataPiler DP20, S64

- After reading a map's MSB, Smithbox's Vulkan build prepares its model and texture lists whatever the rendering
  setting (`Vulkan || Viewport_Enable_Rendering`), and every load ends with a texture and compass refresh; the models
  and textures themselves load only with rendering on. None of it serves DataPiler, which reads the entries only, from
  ordinary memory. The OpenGL build (`Smithbox.OpenGL.exe`, the same program started on its OpenGL context) skips the
  lists with rendering off. No program with a window runs without the GPU, so "no GPU" is not on offer, only less.
- Whether the OpenGL build spares the card in practice is untried (the refresh still runs). The owner chose a setting,
  on by default, that map jobs wait for the OpenGL build (DataPiler DP20, 0.2.13); the next open-world run is the
  trial, and if it shows no "Unable to allocate texture descriptor" lines, the 300-map pause of S63 can be raised.

### 53. The OpenGL build's first 300 maps: no texture warnings; one-go jobs, Resume job and an icon

`2026-11-14` · LIKELY (DataPiler's and Smithbox's logs of en-0011 on 0.2.13 with `Smithbox.OpenGL.exe`) ·
`#smithbox #maps #datapiler` · refs: WHY§51, WHY§52, DataPiler S65, S66, DP20

- On the OpenGL build, en-0011 loaded and read 300 open-world maps in 10 minutes (about 2 s a map, as on Vulkan),
  until 0.2.13's pause. Smithbox's Warning Log held only the navmesh (NVA) read errors, and no "Unable to allocate
  texture descriptor" line; on Vulkan those began near 400 maps, so the trial is short of proof, but the OpenGL build
  likely spares the graphics card.
- The owner wants a map job in one go: DataPiler 0.2.14 drops the pause (S65). Each map is still kept as it is read,
  so a crash costs the map in hand.
- 0.2.14 also adds Resume job (S66): a stopped run's card shows the maps kept and the next map, and Resume goes on
  from them; Start again asks first and starts from the beginning. Cards stopped by 0.2.12 or 0.2.13 show the count
  from their file.
- The owner asked for an exe icon after a famous blood-red katana of the game. DataPiler's rules keep game art out of
  a public repository, so the icon is our own drawing in its spirit (a blood-red katana in a rising slash, with
  drops), in three levels of detail so it reads at 16 px. The patch carries an exe built by BUILD.md's local route,
  with the toolchain's licence notices; the sweep's allow list now passes the authors' addresses in those notices,
  in that file only.

### 54. The open world's boss slots: 98 of 101 named models placed

`2026-11-14` · FACT (en-0011 and en-0012 on DataPiler 0.2.14; `leads/boss-leads.json`) · `#roster #maps` · refs:
WHY§47, WHY§49, WHY§53, roadmap S6.3.p

- en-0011 (m60, 829 maps, resumed after 0.2.13's pause, then in one go on the OpenGL build): 293 maps hold
  characters (14,716), 72 of them boss slots, 107 placements, 46 models; 38 of the 45 non-human ones have names of
  WHY§47's shape, all in the base game's layer. en-0012 (m61): 132 maps hold characters (8,741), 31 of them boss
  slots, 95 placements, 28 models; 16 of the 27 non-human ones have such names, all in DLC 1's layer.
- Open-world entity ids have ten digits and keep the …800 to …899 habit. Characters live mostly in the small tiles
  (_00); a tile's _10 version holds some too, and repeats 16 of its _00 tile's boss placements (same entity, row and
  model): kept once each. A few large tiles (_01, _02, _12) hold characters as well.
- Of all 101 named non-human models, 98 are now placed in a boss slot; 3253, 3575 and 3700 are in none (a boss
  outside the slots, or a cut one). Of the 128 non-human models in boss slots, 98 have candidates; the rest are
  helpers in batches (the expansion's world has more: one model 16 times), stand-ins (100, 1000) and models numbered
  next to a named one. Five slot rows carry real names in the expansion's world (142100 to 142400), one in the base
  game's (130401).
- The research for the bosses' leads is done; binding each boss to its bar name waits for EldenNet in game (O48).

### 55. The named characters' flag blocks, and the jobs for their placements and forms

`2026-11-14` · METHOD · `#roster #flags #datapiler` · refs: ER08§2, O51, O59, DESIGN§7.7, WHY§54

- Flag blocks: the owner allowed FromSoftSaveManager (Apache-2.0) as a source. Read at the commit ER08 pinned
  (`f2980db`), as text from an untrusted folder, never built or run. Its Elden Ring data gives 55 characters' blocks
  keyed by their first flag (3100 to 4800). The block's first four flags mark the character's status (friendly,
  hostile, dead), the base + 5 to + 18 their questline's stages, and 112 progress steps point into 238 flags; 6
  endings have reached, unlock and lock flags. `leads/npc-flags.json` keeps the numbers only, with slugs of ours as
  labels (DESIGN §7.7's "ids, numbers and our slugs"); its names, stage texts and ending names stay out, and the
  source, commit and licence are named in the file. Binding a block to a character goes through the roster's ids
  in game (O48).
- en-0013 reads every map once more (about 1,300, all families; resumable on 0.2.14) and keeps the placements of the
  699 named rows: each character's maps, entity ids, models and talk ids. en-0014 re-reads the named rows' team and
  type, which roster-rows.json did not keep, to tell friendly, hostile and summoned forms apart. Both pass
  DataPiler's checker (DP8: rows picked by an explicit list).
- Next: the two share files; then join placements, forms, flag blocks and boss leads into one census per character.

### 56. The map: the game's map only as the player's own copy; trails are sightings; masking against one upload

`2026-11-14` · METHOD · `#graceui #map #spoilers #licences` · refs: G21–G26, O47, O48, O51, GraceUI:docs/proposals/0002-map.md

- The game's map image is FromSoftware's artwork: an altered copy is still derived from it, so it is never committed,
  shipped or shown in our documents. It can be used only as the player's own copy, read at run time from their game,
  on the basis on which game text is read. The mock-up therefore uses an invented island and a painted stand-in.
- A map mod may change the image's size or framing, so the base map is a setting with a fit check, and GraceUI's own
  drawn base is the fallback (G21, after the owner's point about custom maps).
- SPECULATION: the game loads its map textures only when its own map opens, so replacing that map by the map key (G25)
  may leave nothing to read in memory; the archives are the other route (keys read from the player's game, never
  shipped). The spike S7.9.c settles it; no route means the drawn base is the map.
- Trails are sightings (D89): the game's characters move between places as their stories advance and are seen only
  when loaded near the player, so a trail is the places the player met them, never tracking.
- Invariant 1 (views receive projections only) meets invariant 11 (one texture upload, because hudhook 0.9.3 leaks a
  buffer per upload) in game: new areas are reached during play. Put as G26, recommending one upload at start masked
  at draw time, with a tested exception to invariant 1.
- The owner asked for glow; invariant 14 (no loops) is kept in the recommendation (still glow, one-shot flares), and
  put as G22.
- Later (2026-11-14): the owner requires the map's exact shape, areas and accurate placements in every base, our own
  texture included. A shape traced from the game's map is a copy of its art, so none ships: the drawn base is built on
  the player's PC from their own game (D97), and the spike (S7.9.c) must also find exact sources for it (the map
  image; the game's map data and terrain where a map mod does not fit).

### 57. The named characters' census: placements, forms and the talk id's two patterns

`2026-11-14` · FACT · `#roster #datapiler #talk` · refs: O59, O43, O45, O48, WHY§55, WHY§54

- en-0013 and en-0014 ran on game 2.7.2.0 (DataPiler 0.2.17). 204 of the 213 name numbers are placed on some map:
  422 of their 699 NpcParam rows, in 546 placements (242 in the open world, 67 in the expansion's, 237 in dungeons and
  other maps). Nine names are placed nowhere: spawned by scripts, or unused (leads only).
- A talk id's last four digits are its map's code (the first four digits of a dungeon map's id; 6000 in the open
  world, 6100 in the expansion's) in 341 of 341 placements. Its first three digits, the talk group, equal the name
  number // 100 - 1000 in 324 of 341; the 17 others are one name using another's group. So name number, NpcParam rows
  and talk scripts join on numbers alone, before any game text.
- 142 placed names are on one map; 62 are on two to eleven (map variants _10 counted once): the characters who move
  as their stories advance, the places the map's trails (D89) and the stage tracks will need.
- teamType splits the rows into families (26 and 27 mostly talk; 2 mostly in the 53xxxxxxx rows; 6 mostly without
  talk), but what each number means is not taken from any label: EldenNet checks it in game (O48).
- No simple numeric rule links the 55 flag blocks (WHY§55) to the talk groups; that join is EldenNet's in-game check.
- `leads/named-census.json` keeps it all as numbers, one name per line.

### 58. Where the map's coordinates can come from: five map tables, but no placement positions through Smithbox

`2026-11-14` · FACT · `#map #datapiler #graceui` · refs: O48, O59, G21, D97, WHY§56, WHY§57, DataPiler S67 and DP21

- Placed parts' positions cannot be read through Smithbox's server: at the build the owner runs (2.2.6), SoapstoneLib's
  `ToPropertyValue` turns only scalars and strings into property values, a part's `Position` is a vector, and
  Smithbox's `Entity.GetPropertyValue` takes a property's name with no path into it. Read as text from an untrusted
  folder at Smithbox's commit d144cc7. Characters' positions therefore come from EldenNet in game (the sightings,
  D89), not from DataPiler.
- Smithbox's definitions for Elden Ring name five map tables whose rows carry coordinates as plain numbers (an area, a
  grid cell and a position): the resting places (`BonfireWarpParam`), the map's points (`WorldMapPointParam`), the
  place names (`WorldMapPlaceNameParam`, each tied to a map piece), the pieces the map reveals
  (`WorldMapPieceParam`: rectangles, a flag, a centre and scales) and a conversion of the dungeons' coordinates onto
  the open world (`WorldMapLegacyConvParam`: source and destination area, grid and position, with base points).
  These are labels until verified (O48): DataPiler 0.2.18's `check_fields` (job en-0015) asks the owner's own
  Smithbox which fields exist, with no values.
- Sharing the coordinates themselves needs an exception to DataPiler's DP8, put to the owner as DP21 (recommended: a
  narrow one for these tables' coordinate columns, shared for research and never committed).
- Later (2026-11-14): en-0015 ran on the owner's PC (DataPiler 0.2.19, game 2.7.2.0). All 91 fields named are in the
  server's answers, each with the type Smithbox's definition gives (u8 area and grid numbers, f32 positions,
  rectangles and scales, u32 flags). The tables hold 422 resting places, 472 map points, 10 place names, 34 map
  pieces and 196 conversion rows. The fields are verified in DataPiler's sense (P4); reading their values waits for
  DP21.
- Later (2026-11-14): the owner decided DP21 in wider terms, "using game data for research and development is fine,
  we are just careful what we ship online". DataPiler 0.2.20 shares the five tables' rows whole under the label
  `map_coords`; job en-0016 reads them. The share file is research material: it stays out of every repository, and
  only what is derived from it (a fitted conversion, counts, error figures) is committed. The owner also chose the
  map's terrain (GraceUI G27 (a)): heights built locally, a 3D relief in the browser, our own sprites as the fallback.

### 59. The world map's frame: about one map unit per metre, north up, three sheets

`2026-11-14` · LIKELY · `#map #datapiler #graceui` · refs: WHY§58, G21, G27, D97, DataPiler DP21

From en-0016 (DataPiler 0.2.20, game 2.7.2.0), the five map tables' rows, read as research material that stays out of
the repositories. Only these derived numbers are kept here.

- **Three sheets.** Each resting place shows on one of three maps by its display mask: 284 on the first, 32 on the
  second (all of them in area 12, the underground), 105 on the third (the expansion's areas). Map pieces fall in three
  id ranges to match (0 to 20, 100 to 105, 1000 to 1004, plus 70 and 1070), each opened by a flag of its own.
- **One frame for the open world.** A world position is the grid cell times 256 plus the position in the cell. With a
  uniform scale s, map x = s (X - X0) and map y = s (Z0 - Z) (north up) places all four place names of area 60 inside
  their own pieces' rectangles and all 166 open-world resting places inside the first sheet's pieces for every s from
  0.90 to 1.02, and best near s = 1.0 with X0 about 7,700 and Z0 about 16,700. So the pieces' rectangles are very
  likely in a map space of about one unit per metre. The bounds are loose (100 to 1,000 units of slack): the exact fit
  needs anchors, a set of places whose map position is known, from the player's map image or the game in the spike.
- **The dungeons' conversion.** 196 rows, 194 marked base points, carry positions from 25 dungeon and underground
  areas into the open world (118 into area 60, 45 into 61, 7 into area 70) and a few into other dungeons: the frames
  the drawn base needs to place marks inside legacy dungeons on the world map.
- Only 10 place names exist as map labels, tied to pieces; the 422 resting places and 472 map points are the dense
  anchors.
- Next: anchors for an exact fit (map image or in game), then the fit's error per sheet, recorded here.
- Later (2026-11-15): the dungeons' conversion is mostly plain offsets. Grouping its rows by source area and grid cell
  and destination area, 44 of the 50 groups with two or more rows move every point by the same offset (within 0.5 m),
  so a dungeon's frame maps onto the world by one translation; 6 groups spread up to about 3.5 km (several pieces of a
  dungeon sharing one cell, to be split by their own base points), and 64 groups have one row only.
- Later (2026-11-15), a lead (O48: unverified): Smithbox's definitions name `MapNameTexParam` and `MapPieceTexParam`
  (and `_m61` twins for the expansion) as colour keys (red, green, blue) to an area-name id, a save-name id and a
  multiplayer area. So the game very likely keeps its areas as a colour-coded texture, one colour per area: the exact
  area shapes G21 and D97 ask for, readable from the player's install like the map image, and a second source of
  anchors for the exact fit. `MapGridCreateHeightLimitInfo` holds a minimum and maximum height per row: a possible
  coarse height lead for G27's fallback, its key to be found.
  Later the same day: en-0017 found neither of that table's two fields in Smithbox's answer (it may hold no rows, or
  other names); it was left out, as the relief comes from the terrain itself (G27).
- Later (2026-11-15): the exact fit is WHY§78. On the base map X0 is 7,041 and Z0 16,688 with s = 1 (the first guess
  was 659 units off across, which the pieces' rectangles allowed); the expansion has its own (7,120 and 16,684); the
  underground and the dungeons go through the conversion. Of the six groups of conversion rows that spread, two are
  rows to different maps of one area: grouped by source map and destination map, three spread far (areas 13, 15 and
  25, 551 m to 3.2 km) and one by 1.2 m.

### 60. The areas' colour keys: 16 area names and 49 finer areas on the base map, 11 names in the expansion

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: WHY§59, G21, D97, DataPiler DP21, DP24, DP25

From en-0017 (DataPiler 0.2.23, game 2.7.2.0): the four colour-key tables, research material kept out of the
repositories. Derived numbers only here.

- **Area names.** The base map's key gives 32 colours for 16 area-name numbers in five groups of three or four
  (61000 to 65002), each name under two colours (one with the name's number negative, one positive). The expansion's
  key gives 11 names, several rows switched off by their disable flag.
- **Finer areas.** The second key gives 49 colours on the base map for multiplayer areas in the same five groups
  (61xxxxx to 65xxxxx: 10, 13, 10, 7 and 9 of them), and 18 named areas among 42 colours in the expansion. 18 of the
  base colours are the very colours the name key uses, so one image very likely serves both keys: each pixel's colour
  says the area's name and its finer area. The order differs between the two (one name's colour is another's finer
  area number), so they join by colour, never by number.
- **What it gives the map.** If the image is found among the textures, the exact area shapes for the drawn base, the
  masking and the area labels come from it, and its colours checked against where the resting places stand give the
  exact world-to-map fit. Its name is not known yet; en-0018 (the map tiles, counted) is the next step.

### 61. The game's map tiles: four sheets, five zoom levels, a variant per revealed piece

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: WHY§59, WHY§60, G21, G26, D90, DataPiler DP25

From en-0018 (DataPiler 0.2.23): counts of the map tiles the owner exported from Smithbox's Texture Viewer, numbers
only; the images stay on the owner's PC.

- **FACT, the layout.** 28,469 tiles, every one 256 by 256 pixels. Sheets 0 and 1 have five zoom levels on square
  grids of 41, 31, 11, 6 and 3 tiles a side; sheet 10 has three (41, 31 and 9 a side); sheet 11 has three small ones
  (its level 0 in a window of 16 by 10 tiles). Every grid position has a variant with code 0 at every level (1,681 =
  41 squared at level 0, down to 9 = 3 squared at level 4).
- **LIKELY, the codes.** The other variants carry codes that are sets of bits (0x10, 0x20, 0x40, 0x4000, 0x40000 on
  sheet 0; single low bits on sheets 1 and 10), so the code is very likely the set of map pieces revealed over that
  tile, and the game draws each tile in the variant for the pieces the player holds. Sheet 0 is the base map, sheet 1
  the underground and sheet 10 the expansion (their counts match WHY§59's three sheets); sheet 11 is a small fourth.
- **LIKELY, the scale.** Level 0 spans 41 x 256 = 10,496 pixels, which holds the pieces' rectangles (0 to about
  9,650), so the map's units are very likely level-0 pixels, and by WHY§59 about one level-0 pixel per metre.
- **What it gives the map.** EldenNet can draw the player's own map exactly as the game does, choosing each tile's
  variant from the pieces the player holds (their flags, WHY§58): the reveal G26 and D90 need comes from the game's
  own variants, not from shapes of ours. The colour-coded area image of WHY§60 is not among the map tiles.
- Next: en-0019, each level-0 tile's position and code, to tell which bit is which piece and test map units against
  level-0 pixels exactly.

### 62. Each tile code bit is one map piece, and map units are level-0 pixels

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: WHY§59, WHY§61, G21, G26, D90

From en-0019 (each level-0 tile's sheet, column, row and code, 12,442 tiles) against the pieces' rectangles of en-0016.
Numbers only.

- **Bits are pieces.** The base sheet uses exactly 21 code bits and has 21 pieces (0 to 20); the underground 6 and 6
  (100 to 105); the expansion 5 and 5 (1000 to 1004). Matching each bit's tiles to the rectangle that holds most of
  them, with no order imposed, pairs bit n with piece n on the base sheet for 18 of 21 bits (the rest involve pieces
  13 and 14, whose rectangles are identical, and one bit whose tiles span the whole map), bit n with piece 1000 + n
  on the expansion (each 73 to 100 per cent inside), and bit n with piece 100 + n underground (54 to 74 per cent).
- **Units are level-0 pixels, rows from the bottom.** The fit is best with one map unit to one level-0 pixel, no
  horizontal shift (within 64 pixels), and the tile rows counted up from the bottom of a map 41 x 256 = 10,496 pixels
  high: tile row = (10,496 - unit y) / 256. Shares under 100 per cent fit a reading that the pieces' rectangles bound
  where the map may be scrolled when the piece is open, not every tile it reveals.
- **With WHY§59** a world position goes to map units (about one per metre, north up), map units to level-0 pixels,
  and a tile's variant follows the bits of the pieces the player holds. What is still loose is §59's world-to-map
  offset (hundreds of units of slack): it needs anchors from an image.
- Later (2026-11-15): the offset is settled in WHY§78 (base map x = X − 7,041, y = 16,688 − Z, one unit a metre).

### 63. A tile's pieces that never meet have a variant each, laid together

`2026-11-15` · FACT (the variant sets), LIKELY (how the game draws them) · `#map #datapiler #graceui` · refs: WHY§61,
WHY§62, G26, D90

From en-0019's codes per level-0 tile position, and measurements of the owner's en-0020 overviews (numbers only; the
images stay out of every repository).

- **Variant sets.** On every sheet, a position's exported codes are exactly every subset of a few *widest* codes
  (codes no other code of the position contains), and nothing else. On the base sheet 290 of 1,681 positions have the
  base alone, 514 one widest code, 531 two, 315 three and 31 four; underground 191 positions have two; the expansion
  2. Widest codes can share a bit (152 positions on the base sheet): pieces 1 and 2 meet, 2 and 4 meet, 1 and 4 not.
- **So the game lays them together.** With every piece found there is often no single variant to show; the drawing
  is the base with each widest variant over it. DataPiler 0.2.26 showed only the code with the most bits, so the
  other pieces' parts of a tile stayed unexplored: these were the "joins" in the overviews. The joins themselves line
  up (across each tile edge the colour step matches the step inside a tile, and no shift or doubled border fits
  better), so the bottom-up layout of WHY§62 stands.
- **For GraceUI (G26, D90).** A map drawn from the player's own tiles picks, per position and widest code, the subset
  of the pieces the player holds, and lays those variants over the base: one texture per widest code, not one per
  position.

### 64. Level 2's export holds a second drawing of the map; levels 2 to 4 nest by two

`2026-11-15` · FACT (the nesting and the second drawing), LIKELY (that the game never draws it) · `#map #datapiler
#graceui` · refs: WHY§61, WHY§62, WHY§63, G26, D90, DataPiler S78

From measurements of the owner's en-0020 overviews of DataPiler 0.2.26 and 0.2.27 (numbers only; the images stay out
of every repository).

- **Levels 2, 3 and 4 nest by two, from the bottom left.** Each level-2 tile at the edge of the map matches the level-3
  image at twice its size, at its own place counted from the bottom left (correlation 0.82 to 0.96 at full size, about
  0.99 on small thumbnails); counted from the top it does not (about 0). Levels 0 and 1 do not nest so (41 and 31
  tiles a side: three quarters).
- **A second drawing.** A block of about 7 by 7 tiles at level 2 instead shows the whole map again at about three
  quarters of that scale, clouds and all, ending partway through a tile, with the right image beyond its edge. Its
  tiles match level 3 at their place at most about 0.4. They are variant files like the others, told apart by no
  name; laying every widest variant (0.2.27) brought more of them in than 0.2.26's one variant did.
- **So variants are checked, not trusted.** DataPiler 0.2.28 measures each variant against the next level's variant
  for the same pieces (and its base) and leaves out one below 0.7: on tiles cut from these images, the right drawing
  scored 0.90 to 1.00 and the other at most 0.46. That the game never shows the second drawing is LIKELY (its map at
  that zoom shows one image); whether it is a leftover or drawn at another zoom is open. en-0021 measures every tile
  of levels 2 to 4.
- **For GraceUI (G26, D90).** A map drawn from the player's own tiles must use only variants that pass this check (or
  the game's own rule, once known); the code alone does not say which file is part of the map.

### 65. With the check, level 2 is one image; its base and pieces 15 to 20 stand apart

`2026-11-15` · FACT (the scores, the pieces' rows), LIKELY (what they mean) · `#map #datapiler #graceui` · refs: WHY§62,
WHY§64, G26, D90, DataPiler S78

From the owner's en-0021 overviews (DataPiler 0.2.28; measured here, numbers only, the images kept out of every
repository) and en-0016's piece rows.

- **One image.** Laid with the check, every level-2 land tile matches level 3 at its place at 0.99 or more, but for a
  handful around Leyndell (0.52 to 0.84) and north of Caelid (0.56); dark margin tiles score low, as their texture
  does not line up. Level 3 itself is clean: nearly every variant scores 1.00 against level 4.
- **The base is not part of it.** Level 2's code-0 tiles are a dark margin with a faint outline of the land at another
  scale, transparent over part of column 8 and row 0: not the same drawing as the level's variants. Where a variant
  covers a tile this does not show.
- **Pieces 15 to 20 are not fragments.** Their open flags are 62004 to 62009 with no pickup event (the fragments, 0 to
  14, open by 62010 to 62053 with pickup events 63010 to 63053), their rectangles are wide regions (a quarter of the
  map or more), and levels 3 and 4 have no variant for them. At level 2 their variants are among those left out (0.2
  or less), with some fragment codes at some places (scores near 0) and partial ones (0.5 to 0.7) where level 3 lacks
  the same mix of pieces.
- Open: what flags 62004 to 62009 open, whether the left-out variants are stale files the game never asks for, and a
  rule for the mid scores. The per-tile numbers of en-0021 (`map-tile-check.json`) will tell more.

### 66. Level 2's export is two tile sets under one set of names; Smithbox's export overwrites by name

`2026-11-15` · FACT (the numbers, Smithbox's export code), LIKELY (that two sets were mixed by it) · `#map #datapiler
#graceui` · refs: WHY§64, WHY§65, G26, D90, DataPiler S78

From en-0021's `map-tile-check.json` (DataPiler 0.2.28, numbers only) and Smithbox's texture export source (read in an
untrusted copy, commit d144cc7).

- **Level 2 holds two sets.** Of its 2,045 variants, 424 match level 3 (379 at 1.00) and 1,621 do not; every one that
  matches has a variant of the same code at its level-3 place, and 1,391 of those that do not have none. All 854
  with bits 15 to 20 fail; level 3 and 4 use bits 0 to 14 only. Level 3 is clean against level 4 (519 of 525 match,
  504 at 1.00), and level 4 against level 3 (4,195 of 4,516).
- **One copy of each name, all in one folder.** Every tile was found once, in a single folder.
- **Smithbox writes a container's textures into one folder by their names alone** ("Export textures from container":
  each texture of each TPF in the binder, `File.WriteAllBytes` on `<name>.<type>`), with no check for a file already
  there. Two sets of tiles with the same names (two containers, two TPFs in one binder, or an earlier export left in
  the folder) end as one mixed set, the last written winning name by name.
- **So, LIKELY:** level 2's export is a set drawn for the levels as they are (codes of bits 0 to 14, matching level 3)
  mixed with a set at three quarters of that scale (bits 0 to 20, its own base), the second winning wherever both had
  a name. That explains the gaps (places where no right variant is left) as well as the second drawing.
- Next: the owner re-exports the map tiles container into an empty folder, with older exports moved out of
  DataPiler's folder, and notes which containers hold `MENU_MapTile` textures; en-0021 run again tells which.
- Later (2026-11-15): not the export. Smithbox lists one container of map tiles, one file per tile, and shows the same
  images as the export for two checked tiles, under the same names. The mixing is in the game's container. WHY§67.

### 67. The game's container holds both level-2 sets; the one that fits levels 0, 3 and 4 is incomplete

`2026-11-15` · FACT (the checks), LIKELY (that the game draws level 2 from neither alone) · `#map #datapiler #graceui` ·
refs: WHY§62, WHY§64, WHY§66, G26, D90, DataPiler S78

From the owner's look at two level-2 tiles in Smithbox, en-0021's numbers and en-0019's level-0 codes.

- **In the container.** `menu/71_maptile.tpfbhd` is the only container of map tiles; each file holds one texture of its
  own name. Two level-2 files the check left out (column 2, row 3, code 0x20, and column 4, row 8, code 0x100000) show
  in Smithbox as they were exported: part of a dark margin with land at its edge, and open sea with a city.
- **Set B fits the other levels exactly.** For all 424 level-2 variants that match level 3, every piece of the code is
  among the level-0 codes of the 4 by 4 level-0 tiles beneath it, with no shift: level 2 = level 0 at a quarter, level
  3 = level 2 at a half. Shifted by a tile either way, the share falls to about a quarter.
- **Set A fits neither.** For the 1,621 others it is 0.27 at a quarter scale and at most 0.65 at three sixteenths with
  any shift; it has all of level 2's code-0 tiles and every code with bits 15 to 20. Set B has no code-0 tile, files at
  67 of 121 places, and 59 subsets missing from its own widest codes.
- **So** the map's own drawing at level 2 is set B, with gaps; the game must draw that zoom from something else, or
  from set A at another scale (en-0022, the level-1 codes, tests whether set A is level 1 at a quarter). For GraceUI:
  draw from levels 0, 3 and 4 (and 1 once checked), and from level 2 only where a variant passes the check.

### 68. Level-1 codes do not settle levels 0 and 1; set A is not level 1 at a quarter

`2026-11-15` · FACT (the shares), LIKELY (what they leave open) · `#map #datapiler #graceui` · refs: WHY§62, WHY§67, G26,
D90

From en-0022 (each level-1 tile's sheet, column, row and code: 8,078 tiles, 4,660 on the base sheet) against en-0019's
level-0 codes and en-0021's level-2 sets. The test, as in WHY§67: is every piece of a variant's code among the codes of
the finer tiles beneath it?

- **Level 1 against level 0, at three quarters:** 2,873 of 3,699 variants (78%) pass with no shift, and about as many
  with any shift (best 80%): no clean nest of the kind level 2's set B shows (100% in place, 27% shifted). Variants
  with bits 15 to 20 pass 95%; those with bits 0 to 14 only, 69%.
- **Set A against level 1:** 58% at a quarter with no shift, best 87% at a third with a shift of 192 pixels: set A is
  not level 1 at a quarter. Set B against level 1 at a third: 79% in place, best 92% shifted.
- **What it leaves open:** a level-1 tile spans one and a third level-0 tiles a side, so the codes beneath hold most
  bits anyway (the shifted share barely drops): codes alone cannot tell whether levels 0 and 1 hold one drawing each.
  That needs their images, at a size an overview can take (level 1 at a half, level 0 at a quarter). Until then the
  map's fit uses level 3, which is clean against levels 2 (set B) and 4.

### 69. Every base-map level checked: level 0 is mixed like level 2, level 1 hardly passes

`2026-11-15` · FACT (the scores), SPECULATION (what level 1 is) · `#map #datapiler #graceui` · refs: WHY§64, WHY§67,
WHY§68, DataPiler S79

From en-0023 (DataPiler 0.2.29, numbers only): every variant of the base map's five levels against level 3 (level 3
against level 4), at LEVEL_SCALES from the bottom left.

- **Levels 2 to 4 as before.** Level 3 passes 519 of 525, level 4 4,195 of 4,508, level 2 424 of 2,045 (the same
  variants as en-0021).
- **Level 0 is mixed.** 1,353 of 5,169 measured variants pass (26%), in two clear groups (978 score 0.9 or more,
  1,403 below 0); 633 of the 1,391 places with variants have one that passes, spread over the whole map. Of 1,796
  variants with bits 15 to 20, 3 pass. The scale and anchor are right for those that pass: scores of 0.9 cannot come
  by chance.
- **Level 1 hardly passes.** 38 of 3,698 (1%), at 32 of 806 places, with failing scores near 0 both ways. Either
  level 1's scale or anchor is not three quarters from the bottom left, or it is all of the other drawing.
- Next: en-0025 lays levels 0 and 1 out unchecked, and level 1's base, to fit their scale against level 3 by image.

### 70. Level 1 sits at the top of its grid; levels 0 and 1 hold an old tile set four columns off

`2026-11-15` · FACT (the offsets), LIKELY (what the old set is) · `#map #datapiler #graceui` · refs: WHY§64, WHY§67,
WHY§69, DataPiler S80

From the owner's en-0025 images (levels 0 and 1 laid out without the check, shrunk four times), each tile located on
its own in the level-3 image by normalised cross-correlation (numbers only; the images stay out of every
repository), and en-0024's check of the other sheets.

- **Two sets at level 0, one scale.** Of level 0's tiles that match level 3 well (0.6 or more), 97 sit where their
  names put them and 326 sit exactly 128 level-3 pixels further right: 1,024 map units, four level-0 tiles, with no
  vertical offset. Level 1 shows the same two groups (three level-1 tiles apart). So the container holds a second set
  of tiles at the same scale whose columns are numbered four apart: an older layout of the same map, LIKELY left
  behind by a patch. Level 2's odd set is not this shift (WHY§67), and stays left out.
- **Level 1 sits at the top.** At three quarters of level 0 its tiles match about 12 level-3 pixels lower than a
  bottom anchor puts them, the same in every row (11 to 13): the 64-pixel gap its 31-tile grid leaves over the map
  (10.7 level-3 pixels) is at the bottom. Assuming the bottom is why level 1 passed 1% in en-0023; the underground's
  level 1 (7% in en-0024) has the same grids.
- **The other sheets (en-0024).** The underground: levels 3 and 4 pass 95% and 96%, level 2 67% and level 0 63%.
  The expansion passes almost nothing at any level, so its scales or anchors differ from LEVEL_SCALES; sheet 11 has
  base tiles only. en-0026 re-checks levels 0 and 1 with level 1 lifted and the old set's place measured, and lays
  out the expansion's levels to fit its geometry.
- **Later (2026-11-15, WHY§73):** reversed. The tiles that match level 3 four columns further right are today's,
  as the game's masks name them, and level 3 is the older layout; the tiles that matched it in place are that
  layout's leftovers. Level 1 at the top of its frame stands.

### 71. The expansion's levels 1 and 2 sit at the top left; level 2 is 2,089 / 10,496 of level 0; the old set fills 960 places

`2026-11-15` · FACT (the fits and counts), SPECULATION (sheet 11) · `#map #datapiler #graceui` · refs: WHY§67,
WHY§70, DataPiler S81

From the owner's en-0026 images of the expansion's map (sheet 10, every level laid out unchecked; numbers only, the
images stay out of every repository) and en-0026's check of levels 0 and 1 with the old set's place measured.

- **The expansion's geometry.** Its level 0 is the base map's 41 by 41 tiles. Level 1 is three quarters of it at the
  top of its 31-tile grid, as on the base map. Level 2 is not a quarter: fitted against level 0 shrunk four times it
  is 1 / 1.258 of that, so 2,089 of level 0's 10,496 pixels a side (0.199), at the top left of its 9-tile grid
  (2,304 pixels), with no offset. DataPiler 0.2.31 holds these as the sheet's own scales and anchors.
- **What the old set adds.** On the base map's level 0, 1,654 variants fail at their own place but match 1,024 map
  units to the right. At that place, 694 are already covered by today's passing variant of the same code, 485 stand
  in for today's variant of the same code that fails, 456 add pieces today's passing variants lack, and 19 fill a
  place with no passing variant: 960 in all. Level 1 has 788 (296, 231, 243 and 18), the underground's level 0 243
  (188 covered, 55 standing in) and its level 1 146 (109, 37). An overview at levels 0 and 1 now moves them there.
- **Still open.** The third set, which fails both in place and moved (level 0 2,162, level 1 2,569), and level 2's
  odd set. Sheet 11's grids (34 by 21, 26 by 17 and 7 by 6 tiles) fit neither table; SPECULATION: a smaller map with
  its own scales, which its overviews in en-0028 will show.
- Next: en-0028 re-checks the expansion at its own scales and lays out every sheet's levels 0 to 2 with the old set
  moved; en-0027 unpacks the game's masks (WHY§72).
- **Later (2026-11-15, WHY§73):** the expansion's geometry is every sheet's. The variants 0.2.31 moved were today's
  tiles, moved into the older layout; DataPiler 0.2.32 withdraws the moving.
- **Later (2026-11-15, WHY§76):** measured on the game's own overviews, level 2 is 0.19875 of level 0 (about
  2,086 of 10,496 pixels), not 2,089, and level 1 0.74857, not three quarters.

### 72. The game's tile masks are a KRAK-packed binder of four files, one per sheet; DataPiler opens it with the game's Oodle

`2026-11-15` · FACT (the file and its layers), SPECULATION (what a mask holds) · `#map #datapiler #smithbox` · refs:
WHY§70, WHY§71, DataPiler DP28, S81

From the owner's Smithbox unpack of the game's `menu` folder and the file they uploaded (kept in scratch only).

- **The file.** `menu/71_maptile.mtmskbnd.dcx` sits beside the tiles' own containers. It is a DCX packed with Oodle
  (KRAK): 5,873 bytes that unpack to 300,712, a BND4 binder of four `.mtmsk` files, `menu_maptile_m00`, `m01`, `m10`
  and `m11`, one per sheet, in the game's ScaleformMaptile mask folder. Smithbox unpacks it but shows nothing of it.
  SPECULATION: each mask holds, for every tile place and level, which pieces reveal it: the game's own rule for which
  variant to draw, which would settle WHY§70's old set and the third set at once.
- **Opening it.** DataPiler 0.2.31 reads the DCX and BND4 layouts itself (learnt from SoulsFormats, MIT, read only)
  and loads the game's own `oo2core_6_win64.dll` from the Game folder for KRAK (DP28); the owner's copy is the
  64-bit library with `OodleLZ_Decompress` among its exports, read without loading it. Its call was checked here
  against a stand-in library built for the purpose.
- **The owner's full unpack.** Smithbox unpacked the whole game into DataPiler's folder quickly, but failed to write
  the same 75 or so files in two runs: patched shaders, two map variants (`m60_42_40_10`, `m60_47_42_10`), a few
  sounds, one script and the game's two network key files. None is in `menu`; the keys were never written (no such
  folder), and DataPiler refuses a job that names a key or the regulation.
- Next: en-0027 unpacks it on the PC; the owner sends the four masks, kept in scratch, to decode their layout.
- **Later (2026-11-15, WHY§73):** en-0027 opened it on the owner's PC with the game's own Oodle. The four files are
  `MENU_MapTile_M00.mtmsk` to `M11` in `menu/ScaleForm/maptile/mask`, and the speculation holds: each is an XML
  list of the pieces that touch each tile.

### 73. The game's masks name today's tiles; levels 3 and 4 are an older layout; one geometry on every sheet

`2026-11-15` · FACT (the masks, counts and fits), LIKELY (why the older layout is there) · `#map #datapiler #graceui` ·
refs: WHY§63, WHY§67, WHY§70, WHY§71, WHY§72, DataPiler S82

From the four mask files en-0027 unpacked on the owner's PC (kept in scratch only), against the tile lists of en-0019
and en-0022 and the scores of en-0026 (numbers only), and the owner's en-0021 image of level 2's base tiles.

- **What a mask is.** Each `MENU_MapTile_Mxx.mtmsk` is plain XML: `MapTileMask` entries whose id is level * 10,000 +
  column * 100 + row, as the tiles' names number them, and whose mask is the pieces that touch that tile, for levels
  0 to 2 only. The base map has 1,369, 784 and 65 entries, the underground 418, 250 and 26, the expansion 480, 288 and
  30; sheet 11's 2,434 are all marked as not existing (it has base tiles only).
- **The game's rule.** At every entry of sheets 0, 1 and 10 every subset of the mask is exported, the mask's own code
  always. So the game draws the variant whose code is the mask without the pieces not yet revealed, and any other code
  at a place is not drawn. The base map's level 0 has such leftovers at 877 places (2,319 variants): exactly S77's
  places with several widest codes, which were never pieces to lay together.
- **The older layout.** The leftovers fall inside today's masks four columns to the left at level 0 (704, against 636
  at five) and three at level 1, with no row shift. Against level 3 the image check passed 1,324 leftovers in place and
  3 of today's variants, while 1,304 of today's passed four columns right. So levels 3 and 4, which have no masks, are
  an older layout whose ground sits 1,024 map units right of today's, and the leftovers are its tiles: WHY§70's "old
  set" is today's. LIKELY: the map was laid out anew when the expansion came, and the container kept every file no
  new one replaced.
- **One geometry.** Fitting each piece's tile extent at levels 1 and 2 against level 0 gives, on the base map, 0.748
  (across) and 0.750 (down) at level 1 with the map at the top (offset 88 pixels; 64 expected) and 0.191 and 0.199 at
  level 2 (offset 189; 215 expected); the expansion, the control, gives about 0.75 and 0.20 (offset 212). Level 2's
  base tiles on the owner's en-0021 image turn transparent at 8.16 tiles across and 0.84 up: a 2,089-pixel map at the
  top left of a 9 by 9 frame. So every sheet has the expansion's geometry (WHY§71), and WHY§67's quarter was the older
  layout's level 2.
- DataPiler 0.2.32 draws overviews from the masks, adds `in_mask` to the check, uses the one geometry and never takes
  level 3 or 4 as the reference. en-0028 was withdrawn before it ran; en-0029 re-checks levels 0 to 2 of every sheet
  and lays them out from the masks.
- **Later (2026-11-15, WHY§76):** the one geometry holds, with level 1 at 0.74857 and level 2 at 0.19875 of level
  0, measured to a third of a pixel on the game's own overviews.

### 74. en-0029: today's tiles match at levels 0 and 1 and on the expansion; level 2 met the older layout's level 3

`2026-11-15` · FACT · `#map #datapiler` · refs: WHY§73, DataPiler S82, S83

From en-0029 (DataPiler 0.2.32, numbers only): every variant of levels 0 to 2 of sheets 0, 1 and 10 against the
reference level (level 2; level 2 itself against its nearest level), with `in_mask` from the game's masks.

- **Today's tiles match.** Of the variants the masks name, the base map's level 1 passes 1,982 of 1,982 (median
  0.961) and level 0 2,312 of 2,873 (0.840); the underground's level 1 326 of 326 and level 0 458 of 519 measured; the
  expansion's levels 0, 1 and 2 634 of 760, 502 of 502 and 98 of 98. So the one geometry of WHY§73 holds on every
  sheet. Of the other layout's variants, 31 of 2,296 pass in place on the base map's level 0.
- **Level 2 met the wrong reference.** On the base map and the underground no variant of level 2 passed (median
  0.099): as the reference itself it was measured against its nearest level, and level 3, the older layout, tied with
  level 1 and won as the coarser. The expansion, which has no level 3, passed 98 of 98. DataPiler 0.2.33 measures each
  level the game draws against its nearest such level (level 0 against level 1), never 3 or 4.
- **Base tiles score lower** at level 0 (554 of 1,681 pass, median 0.614): unrevealed parchment, whose grain does not
  scale between levels as the drawing does, compared with level 2 at a fifth. Against level 1 more should pass.
- Next: en-0030 checks again on 0.2.33. en-0029's overviews, drawn from the masks without any check, show each level
  as the game draws it.

### 75. en-0030: levels 1 and 2 match on every sheet; level 0 against level 1 drifts from the top left

`2026-11-15` · FACT (the scores, the speed), LIKELY (a scale off by about a thousandth) · `#map #datapiler` · refs:
WHY§73, WHY§74, DataPiler S83

From en-0030 (DataPiler 0.2.33, numbers only): every variant of levels 0 to 2 of sheets 0, 1 and 10 against its
nearest level the game draws.

- **Levels 1 and 2 match everywhere.** Today's variants pass 1,982 of 1,982 and 737 of 737 on the base map, 326 of 326
  and 55 of 55 on the underground, 502 of 502 and 98 of 98 on the expansion (medians 0.96 to 1.0); level 2's bases too.
  The other layout's pass almost none (2% at most). So the masks' tiles and today's geometry hold at levels 1 and 2.
- **Level 0 drifts.** Against level 1 (cells of 6 level-1 pixels) today's level-0 variants pass 54% on the base map,
  66% on the underground and 57% on the expansion, fewer than against level 2 in en-0029 (80%, 88%, 83%). On the base
  map the share falls from the top left: by columns from 70% (0-4) to 30-48% (25-39), by rows from 90% at the top to
  32-47% low down; en-0029's numbers show the same fall, gentler. LIKELY: level 0 is not exactly 4/3 of level 1 about
  the top left (a thousandth off moves the far side by some 10 level-1 pixels); the images will say.
- **Speed.** On the owner's PC en-0030 took about a minute, where checks of this size took about half an hour before
  0.2.33 (worker processes, Windows' own decoder).
- Next: fit level 0 against level 1 on en-0029's overviews (each drawn from the masks), then the exact world fit.

### 76. Every level of every map draws whole from the masks; the levels' true sizes are 0.74857 and 0.19875

`2026-11-15` · FACT (the images, the fits), SPECULATION (sheet 11) · `#map #datapiler #graceui` · refs: WHY§71, WHY§73,
WHY§75, DataPiler S85

From the owner's en-0029 overviews, each drawn from the game's masks with every piece revealed (levels 0 and 1 shrunk
four times; the images stay in scratch, never in a repository).

- **Whole maps.** Levels 0 to 2 of the base map, the underground and the expansion each show their map fully revealed,
  with no stray tile and no seam: the Lands Between filling 92% of its canvas at every level, with the dark margin to
  the right and below; the underground's three cave regions; the Realm of Shadow under its great tree, inside its faded
  border. Sheet 11 shows only a grey grid with a rough placeholder in one corner, and its masks are all marked as not
  existing: SPECULATION, an unused or test sheet.
- **The true sizes.** Matching 128-pixel patches of each level inside the next and fitting each axis by least squares:
  level 1 is 0.74855 of level 0 on the base map and the expansion (0.74866 on the underground) and level 2 0.19875 on
  both, level 2 against level 1 0.26545; every offset within half a pixel of the top left, residuals 0.3 pixel over 100
  to 335 patches. So three quarters (WHY§70) and 2,089 / 10,496 (WHY§71) were near but not exact, and WHY§75's drift
  is the difference, about 14 level-1 pixels at the far side. DataPiler 0.2.34 holds the measured sizes.
- Next: en-0031 confirms the check on 0.2.34; then the world-to-map fit at level 0, the map now known tile by tile.

### 77. en-0031: at the measured sizes every variant the game draws passes, at every level of every map

`2026-11-15` · FACT · `#map #datapiler` · refs: WHY§75, WHY§76, DataPiler S85

From en-0031 (DataPiler 0.2.34, numbers only): every variant of levels 0 to 2 of sheets 0, 1 and 10 against its
nearest drawn level, at the measured sizes.

- **All of today's variants pass:** the base map's 2,873, 1,982 and 737 at levels 0, 1 and 2, the underground's 520,
  326 and 55, the expansion's 760, 502 and 98 (medians 0.98 to 1.0). Level 0's bases pass as well (1,681 of 1,681 on
  the base map, median 0.99, where en-0030 had 0.44): the parchment scales with the drawing after all, and only the
  sizes were off.
- **The other layout's tiles do not** (0 to 2%; 2 of 20 on the expansion): the masks tell them apart exactly.
- So the map tiles are settled: which tile the game draws at every place and level (its masks, WHY§73), and where each
  level lies against level 0 (WHY§76). Next: the world-to-map fit at level 0.

### 78. The world-to-map fit: one transform per open-world sheet; the underground and the dungeons go through the game's conversion

`2026-11-15` · FACT · `#map #datapiler #graceui` · refs: WHY§59, WHY§62, WHY§76, WHY§77, G21, D97, DataPiler DP21

Measured in scratch on the five map tables (en-0016) and the game's own level-0 overviews (en-0029), which stay out of
the repositories; only these derived numbers are kept.

- **Map units.** One map unit is one level-0 pixel, from the top left of the sheet's 10,496-pixel frame (WHY§62); a
  level-1 or level-2 pixel is the unit times 0.74857 or 0.19875, also from the top left (WHY§76). A world position is
  X = grid x × 256 + position x and Z = grid z × 256 + position z, in metres.
- **The base map (sheet 0, area 60):** map x = X − 7,041 and map y = 16,688 − Z: one unit a metre, north up. Fitted on
  the place-name banners drawn into the tiles, each name centred on its banner. Measured the same way, the five
  banners on this sheet (four names of area 60 and one carried from area 10 by the conversion below) lie −17 to +2
  units from their names across and within about 20 up and down. All 166 open-world resting places land on drawn land.
- **The expansion (sheet 10, area 61):** map x = X − 7,120 and map y = 16,684 − Z. Its one place name lies within about
  10 units of its banner, and all 59 of its open-world resting places land on drawn land.
- **The underground (sheet 1) and the dungeons have no transform of their own.** A position in any other area is
  carried onto the open world by `WorldMapLegacyConvParam`, then drawn with a transform above: the base map's for the
  underground (its sheet shares the base map's frame) and for the dungeons that end in area 60, the expansion's for
  those that end in 61. The rule that fits: take the rows whose source is the position's own map (its area and both
  grid numbers, which name a map in a dungeon and a cell in the open world); prefer a row straight onto area 60 or 61;
  else follow a row to another map and start again (one underground map goes through two other areas). Where one map
  has rows to the same destination with different offsets (areas 13, 15 and 25, 551 m to 3.2 km apart: pieces of one
  dungeon placed apart), the nearest base point is used (LIKELY: no check here tells them apart).
- **Checks.** The three underground place names land on their banners (0, +4 and +36 units across, within about 20 up
  and down). All 30 underground resting places with a row land on the drawn caves; 118 dungeon resting places land on
  the base map's pieces and 46 on the expansion's, the sheet each one's display mask names (WHY§59: 284, 32 and 105).
  Three have no row (two in one underground map, one in area 19); how the game places them is not known.
- **Precision.** About ±20 units on the base map and the expansion (the banners' own placement is the limit, and the
  expansion has only one); about ±40 underground, where two paths through the conversion can differ by up to 36 units
  (a loop of rows does not quite close) and one banner sits 36 units off. A resting place's icon is larger than that
  at level 0. A few units better would need the game's own icons: one screenshot per sheet at level 0 with resting
  places showing.
- The transforms are ours, fitted. The conversion rows are game data: read from the player's install at run time,
  never committed.
- Next: EldenNet's map projection and GraceUI's map page (G21) use these: a world position to its sheet, map units
  and level pixels.

### 79. The areas' colour image is very likely the map-information binders, one per kilometre square

`2026-11-15` · LIKELY · `#map #datapiler #graceui` · refs: WHY§60, WHY§78, O48, DataPiler S81

- Smithbox's file dictionary for the game (read as text from an untrusted copy) names 72 files
  `other/mapinfotex/m6x_XX_ZZ_02.mapinfotexbnd.dcx`: 48 for area 60 and 24 for area 61, one per level-2 cell of the
  open world (very likely 4 × 4 cells of 256 m, so 1,024 m a side). They fill area 60's cells 8 to 14 across and 7
  to 16 up, and area 61's 10 to 14 and 8 to 13. Nothing else in the dictionary is named for map names, areas or name
  textures.
- So the colour keys' image (WHY§60) is very likely cut into these squares, a texture or more in each binder. A name
  in a dictionary is a label until the files are read (O48).
- Jobs en-0032 (the 48) and en-0033 (the 24) unpack them on the owner's PC with DataPiler's unpack step (0.2.31
  onwards). The inner files stay local, and the owner uploads them as research material. Next: read their textures
  in scratch, check their colours against the keys and the resting places (WHY§78's transforms), and if they hold,
  record the areas' shapes.
