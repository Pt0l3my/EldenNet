<!--
EldenNet's roadmap, for EldenNet and GraceUI: the one source. Edit it by hand;
tools/roadmap/build.py turns it into the page docs/roadmap/index.html.

  ## S8 Adopting the engine [planned]    a stage: "## ", its id, its title, a status in brackets
  ### S8.1 The blueprint                 a step: "### ", its id (the stage's id, a dot, a number)
  - S8.1.d Fold the changes in [next]    a part: "- ", its id (or just its letter), title, status
    - needs: S6.1.c, b                   ids of anything, any stage; a bare letter is a part here
    - refs: DESIGN§7.8, O46, G4          codes or repository paths, commas or spaces
    - gives: one line on what it gives

Fields (needs, refs, gives, status) under a heading, not indented, belong to the stage or step;
indented (spaces or a tab) under a part, to the part. A line that is none of these, under a
heading, is its note. A part's bare letter is its id only when it is the next letter in its step
and the title after it doesn't start in lower case ("- a new part" has no id, and is reported).
An unknown field, a status written wrong ([Planed], [next ]) and an empty title are reported.
Statuses: done, active, next, planned, waiting (on the owner: only they can do it), idea. A part
with no status is planned (once tools/workplan/tasks.json exists, it takes its tasks' status); a
step or stage with none takes its parts'. Ids are never renumbered: a dropped part keeps its id
and says so, and a new part takes the next free letter.

References: the codes of EldenNet's wp index (DESIGN§16.2, MEM§12, NPC§4.8, SN04, ER07, REVIEW,
CLAUDE), the owner's decisions O1 and on (docs/DECISIONS.md), GraceUI's own
(GraceUI:DESIGN§10, its owner decisions G1 and on, its design log GraceUI:D67, and
GraceUI:<path>), checked against GraceUI's files when its checkout sits beside EldenNet's, or
repository paths, which must exist. Spike names in titles (S0-A to S0-E, S1 to S10, X1 to X3,
DESIGN §16.1) are spikes, never ids. No game text, no character or place names of the game, and
nothing personal: the page is published.

Build: python3 -I tools/roadmap/build.py, in the same commit as any edit to this file or the tool;
check: the same with --check, which also fails when the page is no longer what the source builds;
tests: python3 -I -m unittest discover -s tools/roadmap. Publish: build with --fragment FILE,
sweep FILE (privsweep files FILE), then publish it to the roadmap's page on claude.ai, at the same
address every time, kept private:
https://claude.ai/artifact/1HQVhkTsKhgR1EdkqUijxk (private; first published 2026-11-07;
slug 024a612a-24db-41f3-b91d-c0b13685df61).
-->

# EldenNet's roadmap

- updated: 2026-11-27
- refs: DESIGN§0, DESIGN§16, DESIGN§19, GraceUI:DESIGN§0, GraceUI:DESIGN§10, CLAUDE

EldenNet is an LLM-driven NPC framework for Elden Ring, based on SkyrimNet with its author's
approval; GraceUI is a UI framework for Elden Ring mods, of its own, that every EldenNet release
carries and whose first user is EldenNet's menu and dashboard (G14). This one roadmap covers both.
The design is done and decided, the bio's five layers included (O46). EldenNet builds its own
engine on SkyrimNet's proven logic (O84), and its core runs outside the game: the template
engine, the model client and the decision calls (S8.1.a). Meanwhile the game's own data (its ids
and such) is researched and documented (O54). Then the engine grows towards the game, the roster
pipeline starts, and EldenNet's increments v0.1 to v1.0 follow, each shippable to testers, with
GraceUI's milestones beside them. Version 1 stays dialogue-only and read-only towards the game.

## S0 Research [done]

- refs: RES

### S0.1 SkyrimNet, studied for its design [done]

- S0.1.a Its architecture, conversation pipeline, prompts and characters [done]
  - refs: SN01, SN02, SN03
- b Memory and knowledge; events, perception and triggers [done]
  - refs: SN04, SN05
- c Speech, UI and dialogue; actions, decisions and the game master; the Skyrim engine [done]
  - refs: SN06, SN07, SN08
- d The summary: what EldenNet borrows, and the ideas-only boundary of the time [done]
  - refs: SN00
  - gives: the boundary is replaced by the owner's SkyrimNet decision (S4.2.d).

### S0.2 Elden Ring [done]

- S0.2.a Engine internals and the modding ecosystem [done]
  - refs: ER01, ER02
- b Dialogue and presentation; the game's design and world [done]
  - refs: ER03, ER04
- c Game state and events; prior art [done]
  - refs: ER05, ER06
- d The feasibility summary [done]
  - refs: ER00
- e The game's text taken out of the notes, and the world-stage and quest leads [done]
  - refs: ER07
  - gives: leads only: every fact is verified in game or by a self-test before EldenNet relies
    on it.

## S1 The v1 design [done]

- needs: S0
- refs: DESIGN§0, DESIGN§18

### S1.1 The design panel [done]

- S1.1.a Three proposals: the lean monolith, the split brain and experience first [done]
  - refs: P1, P2, P3
- b Three judges' scorecards: stability, minimalism and the player's experience [done]
  - refs: J1, J2, J3
  - gives: the lean monolith wins, with the other two's best ideas grafted on.

### S1.2 The master design and its detail documents [done]

- S1.2.a The master design [done]
  - refs: DESIGN§0, DESIGN§1.3, DESIGN§17
- b The code architecture, NPCs and dialogue, memory and knowledge [done]
  - refs: ARCH, NPC, MEM

### S1.3 The review rounds [done]

- S1.3.a The adversarial review: consistency, feasibility, scope, licences and readability [done]
  - refs: REVIEW§2, REVIEW§3
- b Three verification passes on the overlay records, and split table cells mended [done]
  - refs: REVIEW§4.3, O25

### S1.4 The owner's first answers [done]

- S1.4.a No me3, no fromsoftware-rs, our own game-access layer and our own event engine [done]
  - refs: O47, O48, O49, O3, O4
- b Every recommended default accepted, and the review's two rules [done]
  - refs: DESIGN§19, O1, O2, O23, O24, O50, REVIEW§4.3
- S1.4.c hudhook pinned at exactly 0.9.3; game text in another language; a note per pattern [done]
  - refs: O25, O26, O27
  - gives: superseded in part by O25 as changed on 2026-11-16: Dear ImGui 1.93.0 and MinHook 1.3.4 replace hudhook,
    held by GraceUI's host (S8.1.e); O26 and O27 stand.

### S1.5 Working tools and records [done]

- S1.5.a The privacy and safety sweep, git's pre-push gate on every repository [done]
  - refs: CLAUDE, O64
  - gives: since 2026-11-15 the sweep lives only in the owner's private repository, installed from that checkout, and
    EldenNet's history was rewritten without it (O64; S7.5).
- S1.5.b wp, the section index for reading the documents cheaply [done]
  - refs: tools/workplan, WP
- S1.5.c llmworker, a client for running prompts on OpenAI-compatible models [done]
  - refs: tools/llmworker
- d The project's records: the decisions files, this roadmap, the showcase and the handover [done]
  - refs: docs/DECISIONS.md, docs/ROADMAP.md, tools/roadmap, docs/showcase, handover-2026-11-07/HANDOVER.md
  - gives: what the owner decided, where things stand and what comes next, kept in the
    repositories, since sessions and their containers end.

## S2 Quest and world awareness [done]

- needs: S1
- refs: DESIGN§10.6, MEM§12, O51

### S2.1 Important NPCs know their quest and the world's stage [done]

- S2.1.a The design: the world view, what each persona knows, the world stages and each persona's road [done]
  - refs: DESIGN§10.6, MEM§12, ER07
- b The final review of quest and world awareness [done]
  - refs: MEM§12.12
- c News raised unasked on a reaction (O28), and every check run as soon as it makes sense (O29) [done]
  - refs: O28, O29, MEM§12.13, DESIGN§16.3

## S3 Speaking up and pacing [done]

- needs: S2
- refs: DESIGN§8.9, DESIGN§9

### S3.1 NPCs speak on their own, within pacing [done]

- S3.1.a Pacing, never counts: own-initiative lines limited by pacing, allowances and context gates [done]
  - refs: O31, DESIGN§9.4
- b Named NPCs speak up for quest business, greetings and news [done]
  - refs: O32, O30, DESIGN§8.9, DESIGN§9.7
- c Autonomy on a timer, for an NPC within range [done]
  - refs: O37, DESIGN§9.8

### S3.2 What the player sets [done]

- S3.2.a Reactions per event type, with their cooldowns, the range and the pacing values [done]
  - refs: O33
- b A page of every NPC type, with what each type may do [done]
  - refs: O38, DESIGN§7.6

### S3.3 What NPCs know of each other and of themselves [done]

- S3.3.a NPC-to-NPC exchanges, a setting on by default [done]
  - refs: O34, DESIGN§9.7
- b Awareness of their own dialogue and state [done]
  - refs: O35
- c Status effects, their own and each other's [done]
  - refs: O40, DESIGN§9.9
- d Combat dialogue for every combatant, boss fights included, never over a scripted line [done]
  - refs: O42, DESIGN§9.10

### S3.4 Grounding and scope [done]

- S3.4.a Positive grounding: the prompt says what has happened, and forbid lists stay in code [done]
  - refs: O36, O86
  - gives: simplified by O86 on 2026-11-15: grounding is the one spoiler mechanism and the guards beside it are dropped
    (S7.13); `forbid` stays code-only, as the topics of the test questions.
- b Version 1 stays dialogue-only and read-only; acts and gestures are later seams [done]
  - refs: O41, DESIGN§13

## S4 Profiles and the roster decisions [done]

- needs: S3
- refs: DESIGN§7.7, DESIGN§7.8

### S4.1 Character profiles [done]

- S4.1.a One profile system at three depths, bosses included, bound only through verified keys [done]
  - refs: O43, DESIGN§7.7, NPC§4.5
- b Calm lines of cards and character profiles wait for spike S8's sleep signal [done]
  - refs: O44
- c Subjects each profile likes to talk about; a director as a later seam [done]
  - refs: NPC§4.6, DESIGN§13

### S4.2 The full roster [done]

- S4.2.a A bio of our own for every named NPC and named boss, general and stage-independent [done]
  - refs: O43, DESIGN§7.8
- b Free conversation and memories for every profiled NPC, the speaker matched by id [done]
  - refs: O45
- c The bio's structure and the roster's reach put to the owner, with a recommendation [done]
  - refs: O46
  - gives: decided 2026-11-16, as revised in proposal 0004: one bio in five layers (S6.2.a).
- d SkyrimNet's author gives EldenNet the engine blueprint, and EldenNet builds on SkyrimNet [done]
  - refs: O53, O84
  - gives: we may use, copy and adapt SkyrimNet's engine and content, credited as its author wishes, with every licence
    and notice file kept. Since 2026-11-15 (O84): our own engine follows SkyrimNet's logic and structure, not its
    prompts, and Min's DLL is neither used nor shipped (S6.1.c).

## S5 GraceUI's design [done]

- needs: S1
- refs: GraceUI:DESIGN§0

### S5.1 The design, its research and the mock [done]

- S5.1.a GraceUI's design, research notes and HTML mock [done]
  - refs: GraceUI:DESIGN§0, GraceUI:docs/research
- b The profile editor's needs, kept for the next design pass [done]
  - refs: GraceUI:docs/profile-editor-needs.md
- c What EldenNet's design must change for GraceUI, listed [done]
  - refs: GraceUI:docs/eldennet-changes.md

### S5.2 The owner's answers, G1 to G10 [done]

- S5.2.a Dear ImGui with the Grace theme in game, the full web UI in the browser [done]
  - refs: G1, GraceUI:DESIGN§2.6
- b A standalone mod that each EldenNet release carries; the browser app in Preact [done]
  - refs: G2, G3, GraceUI:DESIGN§2.7
- c The browser dashboard always on; other devices on the home network only after pairing [done]
  - refs: G4, G9, GraceUI:DESIGN§7.2
- d Every NPC type, and every named NPC's general profile, listed from the start [done]
  - refs: G5, G8, GraceUI:D67
- e A larger text scale on the Steam Deck; later features as WIP entries [done]
  - refs: G6, G7
- f EldenNet keeps the one overlay, and graceui.dll hooks nothing [done]
  - refs: G10, G15, GraceUI:DESIGN§2.9
  - gives: superseded by G15 on 2026-11-08: GraceUI owns the overlay and EldenNet draws through it; folded into both
    designs on 2026-11-15 (S7.8.c).

## S6 Waiting on the owner [done]

- needs: S4
- refs: DESIGN§7.8

The two prerequisites the owner set for the engine work and the roster (DESIGN §7.8), both settled (O84 on 2026-11-15,
O46 on 2026-11-16), and the research into the game's data that went ahead before them (O54). The capture plans
(S6.3.s) remain.

### S6.1 Min's engine blueprint [done]

- S6.1.a The owner supplies Min's engine blueprint [done]
  - refs: O53, DESIGN§7.8
  - gives: supplied 2026-11-15: an LLM layer, speech to text, voice detection, text to speech and a prompt engine,
    as a closed DLL with MIT headers; no context engine or content store yet (WHY§95).
- b Read it, keep its licence and notice files, and recommend what EldenNet adopts [done]
  - needs: a
  - refs: WHY§95, O80, O81, O82, O83
  - gives: supplied 2026-11-15: FatelessAiEngine 0.2.0, a closed Windows DLL with MIT headers, kept outside the repositories (WHY§95); proposal 0003 recommends what EldenNet adopts, where it runs, Min's online service and the keys, as O80 to O83.
- c The owner decides what EldenNet adopts, with its licence and the owner's terms recorded [done]
  - needs: b
  - refs: O84, O80, O81, O82, O83
  - gives: decided 2026-11-15 (O84): our own engine, MIT, in C++, on open libraries, following SkyrimNet's logic and structure, not its prompts, with no telemetry; O80 to O83 withdrawn, and Min's DLL neither used nor shipped.

### S6.2 The bio's structure

- S6.2.a The owner decides O46: the bio's fields, and which NPCs v1.0 covers [done]
  - refs: O46, O85, DESIGN§7.8, WHY§96
  - gives: decided 2026-11-16, as recommended (proposal 0004): five layers (first meeting, revealed by verified stages,
    companion, lived per journey, the Tarnished's own), the base game's roster in v1.0 and the expansion's in v1.1;
    followers after v1.0 (O85).
- b Fold O46's five layers into DESIGN §7.8, NPC §3.1, MEM (the lived layer's update job) and the roster pipeline, with GraceUI's roster entries (D67) [done]
  - needs: a
  - refs: O46, O85, O86, G5
  - gives: folded 2026-11-16: the bio's fields in NPC §3.1 and DESIGN §7.8; the lived layer as MEM §13; GraceUI's roster shows `public.summary`, `public.role` and `subjects`.

### S6.3 An early census

- S6.3.a The owner decides O54: whether the numbers-only roster census may run before the blueprint and O46 [done]
  - refs: O54, DESIGN§7.8
  - gives: decided 2026-11-08: nothing is built before the blueprint; the game's data is
    researched and documented now, as standalone research.
- b Research where the game's data can come from: online databases, modding tools and their licences, a Python tool of our own [done]
  - needs: a
  - refs: O54, O48, ER02, ER08
  - gives: a research note for the owner (ER08): the sources, their licences, and how our tools read Smithbox's exports.
- c The owner chooses the source our tools read: Smithbox (O55) [done]
  - refs: O55, ARCH§12.3
  - gives: adopted under ER08's five conditions: MIT, no AI clause; never bundled; numbers only from the owner's own exports.
- d The owner decides how far our tools go before the blueprint (O56) [done]
  - needs: b, c
  - refs: O56, ER08
  - gives: decided 2026-11-08: DataPiler, a tool of its own in a public repository (its decisions DP1 to DP5).
- e A dry run: the owner exports from Smithbox and runs `tools/gamedata/smithbox_inspect.py`, which prints counts only [done]
  - needs: c
  - refs: O56, ER08, tools/gamedata/README.md
  - gives: superseded by DataPiler on 2026-11-08 (h, m): its jobs have read the owner's own data through Smithbox's
    server link since en-0001, so the dry run was never needed; marked done on 2026-11-16.
- f DataPiler's design, asked of the PandorumLLM session by the owner, with DataPiler's request 0001 [done]
  - needs: d
  - refs: O56, O57, ER08
  - gives: the answer to request 0001, adopted as DataPiler's design on 2026-11-08 (DataPiler's DP5 and DP6).
- g The owner creates the empty public DataPiler repository, so its first commit can be pushed [done]
  - needs: d
  - gives: the DataPiler repository, created 2026-11-08; its first commit is on its `design/v0` branch.
- h Build DataPiler from that design: launcher and terminal window, the page, the server link, guided exports, jobs, logs [done]
  - needs: f, g
  - refs: O56, ER08
  - gives: DataPiler 0.1 on its `feat/v0.1` branch, tested here on invented data (362 tests, an end-to-end run in a browser).
- i The coverage map of live tracing for every feature, with its eleven gaps (DATA) [done]
  - refs: O57, DATA
  - gives: 105 pieces of live data: 29 backed by research, 65 waiting on a named spike, 11 gaps, none needing a new kind of game read.
- j The owner decides whether to prepare the live checks before the blueprint (O58) [done]
  - needs: i
  - refs: O58, DATA
  - gives: (b), decided 2026-11-15: capture plans in our own words and the owner's saves before each named boss fight, nothing built.
- k DataPiler's fix round: share files limited to what a feature needs (its DP8), the guard's text check, the expansion and the names' facts [done]
  - needs: h
  - refs: O56, O59
  - gives: DP8 enforced (lineage and a sixth guard check), the text check fixed, the expansion as the owner's setting, the names' wrapper leads checked by data; 396 tests.
- l Where the shared id leads go (O59) [done]
  - refs: O59, ER08
  - gives: a numbers-only research file under `docs/research/`, each lead unverified, until EldenNet's identity data replaces it.
- m DataPiler's first run on the owner's PC, and its first job's share file brought back here [done]
  - needs: k
  - refs: O56, O59
  - gives: 2026-11-08, DataPiler 0.2.0 with Smithbox 2.2.6 on game 2.7.2.0: the character, dialogue and place rows read
    through the server link, and job en-0001's counts. The names' wrapper leads 18, 328 and 428 held (95.5% of the name
    numbers resolve), all in the one English item container Smithbox shows, `item_dlc02`. None of the named rows has the
    game's `[ERROR]` placeholder as its name (the owner's count on the local file), so the counts stand. Counts only, so
    nothing for O59's file yet.
- n DataPiler 0.2.1: placeholder texts no longer count as names, each layer's names checked on its own, the names rows done once `item_dlc02` arrives, and the 0.2.0 run's smaller fixes [done]
  - needs: m
  - refs: O56, ER08
  - gives: offered to the owner; not urgent, as en-0001's counts stand without it. Released on 2026-11-08 and kept by
    every later version (0.2.40 by 2026-11-16).
- o The roster's first ids: DataPiler's jobs en-0002 to en-0004 separate the named characters from the ordinary enemies, and their ids go to O59's research file [done]
  - needs: m
  - refs: O59, O48, ER08
  - gives: name number 0 is the ordinary enemies' default (6,325 of 7,045 rows); 213 real name numbers on 699 rows, in
    `docs/research/eldenring/leads/roster-rows.json`, unverified. DataPiler 0.2.2's job en-0005 adds each name's layer:
    163 names first held by the base game's text and 50 by DLC 1 (the expansion's characters), none new in DLC 2. The 21
    rows whose name numbers hold no text are left out, by the owner's agreement (2026-11-08): the game has no name to show
    for them, so most likely cut or unused characters; revisited if EldenNet meets one in game or with the map work for
    bosses. Open then: bosses may take their bar names from event scripts, not NpcParam; p found them very likely
    numbered by model (WHY§47, WHY§54), each binding left to the in-game check.
- p The bosses' leads: DataPiler's jobs en-0006 to en-0012 find the bosses' rows and their likely bar names, before EldenNet checks them in game [done]
  - needs: o
  - refs: O43, O59, O48, WHY§36, WHY§46, WHY§47, WHY§48, WHY§49, WHY§54
  - gives: the legacy and smaller dungeons and the open world of the base game and the expansion (en-0007, en-0008,
    en-0010 to en-0012, DataPiler 0.2.10 to 0.2.14), in `docs/research/eldenring/leads/boss-leads.json`, unverified:
    442 characters in boss slots of 179 maps, and the 269 character names no NpcParam row uses. 240 of those names
    are numbered 90, a model, then three digits; 98 of the 128 non-human models in the boss slots have such names,
    each in the layer of its map's era: likely the bosses' bar names (WHY§47, WHY§54). 98 of the 101 models so named
    are placed; three are in no boss slot. The binding of each boss to its bar name waits for EldenNet's check in game.
- q The named characters' placements and forms: DataPiler's jobs en-0013 (every map: map, entity, model and dialogue ids of the 699 named rows) and en-0014 (each named row's team and type) [done]
  - needs: o
  - refs: O43, O59, O48, WHY§55
  - gives: `docs/research/eldenring/leads/named-census.json`: 204 of the 213 names placed (422 of 699 rows, 546
    placements), their maps, entity and talk ids, team and type numbers and boss slots; the talk id's map suffix and
    talk-group rule; no numeric link from the flag blocks to the talk groups, which waits for the in-game check.
- r The named characters' flag blocks, from FromSoftSaveManager (Apache-2.0) [done]
  - needs: o
  - refs: O43, O51, O59, WHY§55
  - gives: `docs/research/eldenring/leads/npc-flags.json`: 55 blocks (status, questline stages, 112 progress steps)
    and 6 endings, numbers only, unverified; bound to the roster's ids later, never by the slug.
- s The capture plans, in our own words: S6's expectation scripts, the S1 and S2 capture routes and the checkpoint list, the roster batch by batch [done]
  - needs: j
  - refs: O58, DATA, MEM§6.3, DESIGN§7.8, DESIGN§16.1, WHY§580
  - gives: what each live check records, where and when, so the captures start the day the first build runs.
    Written on 2026-11-24 (docs/design/spikes/capture-plans.md; WHY§580), for a normal playthrough (O58 as amended):
    25 checkpoints, nine S1 routes (the first three v0.1's golden routes, S10.2.c), S2's routes and the ten boss
    fights, S6's scripts in MEM §6.3's shape, the roster in ten base-game batches, and 19 open points (S6.3.v).
- t The owner keeps a save before each named boss fight and each later form of a roster NPC, on their own PC, never committed and never taken online [done]
  - needs: j
  - refs: O58, DESIGN§7.8
  - gives: dropped on 2026-11-16 by the owner (O58's note): no prepared saves; the in-game checks run during a normal
    playthrough once EldenNet runs in game, and a check that fails waits for its next chance.
- u The gear's names: which text entries hold the weapon and armour names, checked on the owner's data (DataPiler 0.2.40, jobs en-0041 to en-0043) [done]
  - refs: DATA§3.1, DESIGN§4.5, WHY§105
  - gives: 2026-11-16: WeaponName (11) and ProtectorName (12) name 80.0% and 76.4% of the rows by row id, and no row needs the expansion's layers; DESIGN §4.5 lists them, and S2 checks them in game.
- v The capture plans' open points: telling Marks apart in normal play, S6's wide watch without GraceUI, v0.1's recording form for S1 (the list's distance and length, the player set's types, each character's class), the S1 and S2 reports, S2's text match without text [next]
  - needs: s
  - refs: WHY§580
  - gives: the 19 open points listed at the end of docs/design/spikes/capture-plans.md, each settled or put to the
    owner before the capture it blocks; the design's stale "prepared saves" wording carries dated notes since
    2026-11-24.

## S7 Open items

- needs: S5

Work beside the engine and the increments, and the owner's housekeeping.

### S7.1 GraceUI's changes to EldenNet's design [next]

- refs: GraceUI:docs/eldennet-changes.md
- S7.1.a The main menu on F5 through GraceUI, and the bridge (a UiPort in the core) [next]
  - refs: G1, G15, DESIGN§12
  - gives: the overlay's items (eldennet-changes §4) became GraceUI's host's under G15, and what EldenNet keeps of them
    was folded with S7.8.c on 2026-11-15 (eldennet-changes §11).
- b The local server's items left for EldenNet (eldennet-changes §3 items 2, 3, 4 and 6) [next]
  - refs: G4, G9, G17, G30, GraceUI:DESIGN§7.2
  - gives: C4's wording, the privacy scrub, F11's address commands and keys bound to their address. The server is
    GraceUI's since G17, our own small server since G30, and EldenNet's side of it (its space registered after its
    gate, its sensitive commands marked, the launcher's first screen) was folded with S7.8.c on 2026-11-15.
- c Criteria, presentation and the compatibility matrix's rows [next]
  - refs: ARCH§12.4
- d GraceUI as its own mod in EldenNet's release, and the small fixes found on the way [next]
  - refs: G2
- e The roster list and the Playthrough projection [next]
  - refs: G5, G8, DESIGN§7.8
  - gives: the roster's first-meeting field came with O46's fold on 2026-11-16 (S6.2.b: `public.summary`, `public.role`
    and `subjects`), and its names at the title screen with DATA §3.1's gap 1; eldennet-changes §9 items 3 and 4 remain.

### S7.2 The profile editor pass [done]

- refs: GraceUI:docs/profile-editor-needs.md, O43
- S7.2.a GraceUI revisits the decisions the editor's needs name (O43), with the needs brought to O46 and O86 [done]
  - refs: O43, O46, O86, GraceUI:D213, WHY§375, WHY§376
  - gives: the needs date from 2026-11-07, before O46 settled the bio the player's own profiles follow (the Tarnished's
    own included) and O86 retired the lints against fixture states (PL7) they assume; SP1's `subjects` came with
    GraceUI's O46 fold on 2026-11-16. Done on 2026-11-22 as a design pass (GraceUI feat/foundations f9c4d17): D2's
    exception recorded (D213); the needs brought to G20, O46 and O86 (GraceUI's proposal 0010 §2); the components
    GraceUI owes put to the owner as G38. EldenNet's answers to proposal 0010 §6 are WHY§377.
- b The editor's pages, fields, validation, commands and its "draft with AI" flow, in EldenNet's pages (PAGES; GraceUI's design held them until G20) [done]
  - needs: a, c
  - refs: PAGES§4.6, WHY§377, WHY§520, WHY§521, WHY§522, WHY§524, WHY§525
  - gives: built on 2026-11-23 (engine wip/s12-editor, merged at c223b6e), on GraceUI 0.2.25's components (floor
    0.2.22): Characters › Named NPCs as a master list, a Profile tab with the general profile (first-meeting fields
    only) and Your notes over the four fields a layer may set; Check runs the content load's own checks, Save checks the
    revision and writes the player's own layer in the overrides folder (the previous one kept), and the content loads
    again once no conversation is open; Draft with AI sends only the player's notes and current text to the draft slot
    and returns proposals, a saved draft marking the layer assisted. A layer keeps its persona's key until the player's
    own characters come (WHY§521). Ships in v0.2, ahead of PAGES' v0.3. The words players read went to the owner.
    The owner's O112 the same day: the words as written, the page in v0.2, and the notes saved on the PC alone (the
    editor's writing commands to be marked sensitive, S11.4.e).
- c GraceUI's components for the editor and the Characters pages: topic families, master-detail in both views, the editor block, slow commands, the text field in game (GraceUI's G38, proposal 0010; protocol 1.5) [done]
  - refs: G38, GraceUI:D57, GraceUI:D169, WHY§375, WHY§377
  - gives: put to the owner on 2026-11-22 and decided the same day, all three as recommended ("All now, in 0.2.x
    (Recommended)", "Keep in memory (Recommended)", "One paragraph (Recommended)"; GraceUI feat/foundations f747f46):
    built now, in the order text field in game, topic families with master-detail, slow commands, the editor, each
    tried on the demo space. Being built. Part a done on 2026-11-22: GraceUI's own text field in game, GraceUI 0.2.18
    (GraceUI's D214, WHY§378; protocol 1.4 and ABI 1.5 unchanged). Next: topic families and master-detail (part b),
    slow commands (c), and the editor block with EldenNet's five additions (d; GraceUI's proposal 0010 §7). Part b done
    the same day: topic families and master-detail, GraceUI 0.2.19, protocol 1.5 (GraceUI's D215, WHY§383). Next: slow
    commands (c), then the editor block with EldenNet's five additions (d). Part c done the same day: slow commands,
    answered within 90 s, GraceUI 0.2.20, protocol 1.6 (GraceUI's D216, WHY§386). Next: the editor block with
    EldenNet's five additions (d), a profile's member keyed as its row (WHY§387). Part d done the same day: the editor
    block in both views with EldenNet's five additions, GraceUI 0.2.21, protocol 1.7 (GraceUI's D217, WHY§391). All four
    parts of G38 are built on GraceUI's side; S7.2.b's Profile tab can be built on them. A save's answer may name the
    master list's next row, chosen: "<key>" (GraceUI 0.2.22, GraceUI's D218, WHY§393), for a named.<param> row that
    becomes mine.<id> (WHY§387).

### S7.3 Follow-ups from the character-profile design [next]

- refs: DESIGN§7.7, NPC§4.8
- S7.3.a A check-without-saving message, to match GraceUI's profile check [next]
- b Whether merchants count as the player's sightings [next]
- c Whether the unknown-alias error of lint PL3 reveals that an alias exists [next]
- d Hash widths [next]
- e A card's bark word cap against a character's word cap [next]
- f Character keys in the bark pool store [next]

### S7.4 The LLM Jobrunner

- S7.4.a The request and the read-only review of version 1.2 go to the session that builds it [done]
  - gives: a local job runner for running prompt sets on the owner's own model servers; nothing in the reviewed bundle
    was run. The owner runs 1.2 against their servers (2026-11-16) and asked this session to build 1.3 (c).
- b Its next version reviewed, read-only, before EldenNet's prompt sets run on it [done]
  - needs: a
  - refs: tools/llmworker
  - gives: superseded on 2026-11-16: from 1.3 this session builds and tests the Jobrunner itself (c), from 1.2's
    reviewed source, so no separate read-only review is needed; 1.3 carries the review's nine fixes.
- c Version 1.3: a chat tab for each server (images and text files attached), Check servers and a test jobpack, servers by address with Find servers, an update installed from a dropped zip, DataPiler's job cards, and the 1.2 review's nine fixes [done]
  - needs: a
  - gives: asked by the owner on 2026-11-16, built in a local copy and delivered as a release zip; the owner's
    addresses and names never enter it. Followed the same day by 1.3.1 (d) and 1.4 (e).
- d Version 1.3.1: model-server slots on this PC that are off (Auto, Always or Never), and cards named by the owner's own names [done]
  - needs: c
  - gives: the owner's report on 1.3 (2026-11-16): slots that were off showed as dead servers, and one server twice.
- e Version 1.4: cloud providers (NanoGPT, OpenRouter, any OpenAI-style endpoint) with write-only keys, a spending guard, and a screenshot pasted into Chat [done]
  - needs: d
  - gives: delivered as a release zip on 2026-11-16; keys are kept with Windows' DPAPI, which only the owner's run
    can check (f).
- f The owner's first run of 1.4: a key kept and used on Windows, and a provider's reported costs [done]
  - needs: e
  - gives: dropped on 2026-11-16 by the owner, who has no use for the cloud providers now: "you can remove that
    jobrunner cloud one, not interested in that now". The feature stays in 1.4, untried on Windows.

### S7.5 GitHub [waiting]

- S7.5.a The owner reviews the design pull requests, EldenNet's and GraceUI's [done]
  - gives: merged by the owner: EldenNet #1 to #3 and GraceUI #1 and #2, the last on 7 November.
- b The temporary branches left by the history rewrite deleted [done]
  - gives: deleted by the owner on 2026-11-16; both pointed into the clean history, so they exposed nothing.
- c GitHub Support asked to purge the refs of the pull requests that still reach the rewritten history [waiting]
  - gives: checked on 2026-11-16: the closed pull requests #1 to #5 still reach 102 commits of the old history.
    Today's sweep finds nothing in any of their 372 file versions or in any commit's author, committer or message,
    all from noreply addresses. What remains is the old sweep's `personal.txt`: one word kept as a salted SHA-256
    hash, with its salt beside it. It cannot be read back, but it can confirm a right guess, so the purge stays
    worth asking for if that word is easy to guess.

### S7.6 The public name [waiting]

- S7.6.a The owner picks EldenNet's public name, at any time before the v0.6 public test [done]
  - refs: O1
  - gives: picked on 2026-11-16: the public name is EldenNet ("EldenNet is fine"; O1's note). The credits keep
    saying it is its own project, not SkyrimNet, and that the game's name is its makers' mark.

### S7.7 An interactive showcase of the menu [next]

The owner asked on 2026-11-08 for a showcase of the menu to click through in the browser, beside the static
showcase; the Skyrizz session advised on how its own is built. The owner chose to wait for Min's blueprint,
and decided the same day that it is part of EldenNet, not GraceUI (G13's later change).
Min's engine arrived on 2026-11-15, with no UI, and EldenNet builds its own (O84), so the question was asked again
(c): built from page data (O87).

- S7.7.a The owner decides how it is built (G13) [done]
  - refs: G13, GraceUI:DESIGN§2.5
  - gives: decided 2026-11-08: wait for the blueprint; how to build it is asked again once it has arrived.
- b Build it in EldenNet from page data, test it in a headless browser and publish it privately, linked from the static showcase [next]
  - needs: a, c
  - refs: G13, O87, docs/showcase/showcase.md
- c How it is built, asked again in EldenNet now that Min's engine has arrived, and the owner's answer (G13) [done]
  - needs: a
  - refs: G13, O84, O87, WHY§95
  - gives: decided 2026-11-16 (O87, as recommended): from page data, with invented sample data and a small renderer,
    tested in a headless browser and published privately.

### S7.8 GraceUI as a UI framework for any mod

The owner widened GraceUI's scope on 2026-11-08 (G14): a UI framework other modders can use, as
Magelight UI and PrismaUI are for Skyrim; EldenNet requires it and always ships with it, and its
menu and dashboard become GraceUI's first user. G15 to G17 were decided on 2026-11-08 and folded
into both designs on 2026-11-15 (c); G18 to G20 were decided on 2026-11-16 and folded the same day (d).

- S7.8.a A proposal: what the framework changes in GraceUI's and EldenNet's designs, with questions for the owner [done]
  - refs: G14, G1, G10, O25, GraceUI:docs/proposals/0001-ui-framework.md, GraceUI:docs/research/ui-frameworks.md
  - gives: proposal 0001 and the research note FW; the Skyrizz session answered on Skyrim's UI hosts.
- b The owner answers the proposal's questions, G15 to G20 [done]
  - needs: a
  - refs: G15, G16, G17, G18, G19, G20
  - gives: G15 to G17 decided 2026-11-08, as recommended (GraceUI owns the overlay; page data and panels; the browser
    view for every mod that opts in); G18 to G20 decided 2026-11-16, as recommended: the framework opens to other
    modders after EldenNet v1.0, as GraceUI 1.1; no SkyrimNet-derived material in GraceUI; EldenNet's pages and their
    spoiler rules live in EldenNet.
- c Fold the answers into both designs [done]
  - needs: b
  - refs: GraceUI:docs/eldennet-changes.md, GraceUI:DESIGN§2.9, GraceUI:DESIGN§4.13
  - gives: G15 to G17 and the map's G21 to G27 folded on 2026-11-15: GraceUI's DESIGN (the host, D69 to D101, the map
    section), its notes and invariants 15 to 18, eldennet-changes §11, and EldenNet's DESIGN, ARCH and NPC documents;
    what G18 to G20 decide is folded when the owner answers them (d).
- d Fold G18 to G20 now that the owner has answered them: EldenNet's pages, SP1 to SP9 and its halves of PV1 to PV8 move to EldenNet (G20) [done]
  - needs: b
  - refs: G18, G19, G20, PAGES§1, GraceUI:DESIGN§4, GraceUI:docs/eldennet-changes.md, WHY§107
  - gives: folded 2026-11-16: GraceUI's §4 content, SP1 to SP9, PV1, PV2, PV4, PV5, PV8, D61, the fixtures and goldens
    are EldenNet's PAGES under their numbers, with pointers left in GraceUI; GraceUI keeps the frame, search, the
    settings row, PV3, PV6, PV7 and the map (WHY§107); 1.1 opens the framework (G18); GraceUI carries nothing
    SkyrimNet-derived (G19).
- e Where GraceUI's own settings live now that EldenNet's pages are EldenNet's (its menu key, scale and browser view, homed on EldenNet's pages today), and PV3's sentence on paired devices brought to D84 [done]
  - needs: d
  - refs: G20, WHY§107, GraceUI:D79, GraceUI:D84, GraceUI:D115, PAGES§4.5
  - gives: settled 2026-11-16 by GraceUI's D115: GraceUI's own settings (the menu key, the size, the menu's options,
    the browser view's server) are homed on GraceUI's own pages at the rail's foot, with links from EldenNet's pages;
    Home's cards and the World page's switch are EldenNet's (`ui.home_cards`, `ui.show_world`); PV3 says GraceUI keeps
    the paired device's hash, in its user folder.

### S7.9 The map (GraceUI proposal 0002)

The owner asked on 2026-11-14 for an interactive map of the playthrough: quest progress, NPC placements and
trails, world changes and lore notes from memories, with tooltips and glow, over the game's own map read from
the player's game, and optionally opened by the map key.

- S7.9.a A proposal with a mock-up on an invented island, and questions for the owner [done]
  - refs: G21, G22, G23, G24, G25, G26, G27, O51, GraceUI:docs/proposals/0002-map.md, WHY§56
  - gives: proposal 0002 (base map, layers, two views, the map key, glow, lore notes) and the owner's mock-up.
- b The owner answers G21 to G26 [done]
  - needs: a
  - refs: G21, G22, G23, G24, G25, G26
  - gives: answered 2026-11-15, after the research's revised recommendations (f): the player's own game map (G21), still glow (G22), lore notes added by the model with a setting on by default (G23), the map in 1.1 (G24), the map key a setting off by default (G25), the game's own reveal in game (G26).
- c The map spike: the run-time read through the devkit, the fit check (D93), checks in game, the map key [planned]
  - needs: b, S7.11.d, S10.1.a
  - refs: G21, G25, G26, G27, O47, O48, O73, O74, WHY§78, WHY§90, WHY§92, WHY§94
  - gives: the research (e to i) found the sources: the tiles and masks, the areas, the world-to-map fit, and the
    terrain with its floor rule. Left: their run-time read, through the devkit's first version (S7.11.d, O74); their
    checks in game, which need EldenNet in the game (O73); and catching the map key, with spike S4.
- d Build the map panel, the `map` projection and the Map page, when G24 says [done]
  - needs: c, S7.8.c
  - refs: G24, GraceUI:docs/eldennet-changes.md
  - gives: merged into S17.1.d on 2026-11-15: G24 decided (a), so the build is part of 1.1, after v1.0, on groundwork
    v1.0 builds anyway (the sightings record, the world stages).
- e Research ahead of the spike: the game's five map tables, their fields and sizes (DataPiler 0.2.19, job en-0015), then their coordinates (DataPiler 0.2.20, job en-0016; DP21 decided) [done]
  - needs: a
  - refs: O48, O59, WHY§58
  - gives: whether the resting places, map points, place names, revealed pieces and the dungeons' conversion carry the coordinates the map needs, verified on the owner's PC.
    Fields verified 2026-11-14 (en-0015: 91 of 91, types as defined; 422, 472, 10, 34 and 196 rows); the values come
    with en-0016, as research material that stays out of the repositories.
    Since then the map tiles: the game's own masks unpacked (en-0027), which name the tiles it draws today and give
    one geometry for every sheet; levels 3 and 4 are an older layout (en-0019 to en-0027, DataPiler 0.2.23 to 0.2.32,
    WHY§62 to WHY§77; en-0029 drew every sheet's levels 0 to 2 whole from the masks, their sizes are measured, and
    en-0031 passed every tile the game draws). The world-to-map fit followed (WHY§78): one transform per open-world
    sheet, to about 20 units, and the underground and the dungeons through the game's own conversion rows, to about 40.
    The areas' image followed (WHY§80, en-0032 and en-0033): the exact shapes of the area names and finer areas at
    4 m. Still open here then: the terrain heights (G27), researched since in g to i (WHY§90 to WHY§94), and catching
    the map key (c).
- f The research's findings put into proposal 0002 (its §14): the base composed from the game's own tiles for the map pieces held, the placements and areas found, G26 put again [done]
  - needs: e
  - refs: G21, G24, G26, WHY§78, WHY§80, WHY§82, GraceUI:docs/proposals/0002-map.md
  - gives: revised recommendations for the owner's G21 to G26; terrain heights, the archives' run-time read and the map key stay with c.
- g Research ahead of the spike: the terrain's heights, from the ground models at three levels of detail (job en-0036) [done]
  - needs: e
  - refs: G27, WHY§89, WHY§90, O48
  - gives: yes (WHY§90): the ground models hold the heights, and the tile numbers in their names alone place them; one 1 km square's 16 tiles meet at their edges (median 0.04 m), open ground agrees with the resting places' heights (0.02 m and 1.34 m), and the 1 km level is enough for the relief (rms 0.03 m from the finest). Left for c: the run-time read, a rule for the gaps (18 to 100% ground per tile here), and the rest of the map.
- h Research ahead of the spike: the expansion's and the underground's terrain, and a far view of the land (jobs en-0037 and en-0038) [done]
  - needs: g
  - refs: G27, WHY§90, WHY§91, WHY§92, O48
  - gives: (WHY§92) the expansion's ground is placed by tile numbers as the base game's, level 2 matches level 0 (rms 0.01 m) and its resting places (7 of 9 within 0.03 m), but its land is partly other models (breaks on about 7% of tile edges), which the far view fills coarsely (a median 0.4 m off, up to 296 m at cliffs); the underground is read whole with its placement file, floors told from ceilings by their facing (5 of 6 resting places on a floor within 0.2 m), up to 34 floors deep. Which floor to show, and the expansion's assets, stay with c.
- i Research ahead of the spike: the underground's floor rule, found on one map and checked on five more (jobs en-0039 and en-0040) [done]
  - needs: h
  - refs: G27, WHY§92, WHY§93, WHY§94, O48
  - gives: (WHY§94) the plain topmost floor fails in four of the five new maps; the revised rule, the topmost walkable floor reachable from where each map's characters and resting places stand (a 2 m grid, steps up to 2.9 m), picks the held-out characters' floor for 86% and otherwise a reachable upper level (14%), never a rock top. Where levels stack, the in-game relief shows the upper one.

### S7.10 Plugins (EldenNet proposal 0001)

The owner asked on 2026-11-15 for an easy way to add plugins to EldenNet: dropped into a Plugins folder and
installed when the game starts, or dropped on the dashboard while it runs, with a restart to finish; and whether
EldenNet may hook as well as GraceUI.

- S7.10.a A proposal: content plugins and code plugins, how they are installed, and EldenNet's own hooks, with questions for the owner [done]
  - refs: O65, O66, O67, O68, O69, docs/proposals/0001-plugins.md, WHY§86, WHY§87
  - gives: proposal 0001: EldenNet's own hook library for functions GraceUI does not hook; content plugins checked like
    EldenNet's own content, from 1.1; both install routes, code never through the browser; a published interface for
    code plugins after v1.0.
- b The owner answers O65 to O69 [done]
  - needs: a
  - refs: O65, O66, O67, O68, O69
  - gives: all five decided on 2026-11-15, as recommended ("all five as recommended").
- c Fold the answers into EldenNet's and GraceUI's designs [done]
  - needs: b
  - refs: DESIGN§2.6, DESIGN§11.4, DESIGN§13, DESIGN§18, ARCH§4, ARCH§11.6, GraceUI:DESIGN§13, GraceUI:docs/eldennet-changes.md
  - gives: R12 and GraceUI's invariant 16 as O65 decided; the plugins layer, the launcher's step and flags, the seam and
    the decision-log rows in EldenNet's design; GraceUI's published hook list (D102) and plugins on its Content packs
    page (D103); the upload route put to the owner as G28.
- d GraceUI's upload route for the dashboard's drop zone [done]
  - needs: c
  - refs: G28, O67, GraceUI:DESIGN§7.2
  - gives: G28 decided 2026-11-15, as recommended: one narrow route on the loopback listener for a space's sensitive file
    commands, with a one-use ticket, refused from paired devices, nothing kept (GraceUI §7.2 item 18). Built on
    2026-11-23 (the route: GraceUI 0.2.26, S11.2.am).
- e Validators and safeguards by kind, and where code plugins run: proposal 0001 §13, with O70 decided as the owner designed it [done]
  - needs: c
  - refs: O70, O71, O72, docs/proposals/0001-plugins.md, WHY§88
  - gives: validators before anything installs, the library-clash check against every installed plugin, the plugin
    cluster outside the game with Detached rows, the plugins-stopped pop-up with four choices, folded into both designs
    (GraceUI D104, D105).
- f The owner answers O71 (the launcher as the crash watcher) and O72 (a sandbox for plugin hosts) [done]
  - needs: e
  - refs: O71, O72
  - gives: both decided 2026-11-15, as recommended: the launcher stays in the background as the crash watcher, crash
    dumps staying on the PC; a sandbox for plugin hosts later, as an option, after a spike.

### S7.11 The devkit and the workbench (O73, O74; DataPiler DP35)

The owner decided on 2026-11-15 that one reader of the game's data of our own, the EldenNet devkit, serves EldenNet in
the game and DataPiler on the PC, and ships nothing of the game; that it works with the game closed too, within safe
limits and with a disclosure of safe use and of what we condemn (O75); that in-game data for research comes from
EldenNet itself, built after its engine (S8; our own since O84); and that DataPiler becomes the workbench and the
replay harness.

- S7.11.a The owner decides where in-game data comes from (O73), the devkit (O74) and DataPiler's part (DP35) [done]
  - refs: O73, O74, O48, O53
  - gives: EldenNet in the game, built after its engine (S8; our own since O84); the devkit, shipping nothing of the
    game, at first only while the game runs (O74), with it closed too since O75; DataPiler as the workbench and the
    replay harness.
- b The devkit's design, in proposal 0002 §2: its parts and first version, its safe limits and disclosure (O75), the keys taken from the user's own game, the command-line tool and its browser tools (O78), where it lives (O79) [done]
  - needs: a
  - refs: O74, O75, O78, O79, O48
  - gives: decided 2026-11-15: the devkit works with the game closed, within safe limits and with a disclosure (O75); its browser tools include viewers, never exporting (O78); it ships as part of EldenNet, its features behind Developer mode (O79).
- c The devkit's first check: a file read through it matching Smithbox's copy byte for byte, and the archives read with the game closed and while it runs, on the owner's PC through DataPiler [planned]
  - needs: b
  - refs: O74, WHY§56
- d The devkit's first version: archives, map models, placement files and map textures (the map's run-time read, S7.9.c), then text [planned]
  - needs: c
  - refs: O74, G27
- e DataPiler reads through the devkit (its DP35, amending its DP28) and keeps a local index of the game's data [planned]
  - needs: d
  - refs: O74
- f DataPiler imports EldenNet's play recordings [planned]
  - needs: S10.1.a
  - refs: O73
  - gives: recordings are numbers-only JSON Lines, so the import needs no read through the devkit (e) (the plan for
    S10.1.a, 2026-11-16).
- g DataPiler's replay harness: recordings replayed through EldenNet's engine outside the game, with the owner's model services [planned]
  - needs: f, S8
  - refs: O74
- h DataPiler runs the derivation's offline round with EldenNet's devkit tool on the game's executable, checks that the result holds only numbers and wildcarded patterns, and packs it to send (DataPiler DP36)
  - needs: S10.1.a
  - refs: O89, O73
- i DataPiler compares the param and text ids and counts that EldenNet read in game with Smithbox's reading of the files, numbers only (DataPiler DP36)
  - needs: h, S10.1.f
  - refs: O89, O48

### S7.12 EldenNet off game (proposal 0002)

The owner asked on 2026-11-15 for EldenNet's dashboard in the browser with the game closed: what EldenNet saved, to
look at, and settings changed off game that apply when EldenNet next loads; and for the devkit's tools in the browser.

- S7.12.a A proposal: the dashboard off game, the settings off game, GraceUI's server for hosts outside the game, the devkit's browser tools [done]
  - refs: O76, O77, O78, G29
  - gives: proposal 0002 §2.5, §3 and §4, with O76 to O79 and GraceUI's G29 put to the owner.
- b The owner answers O76 to O79 [done]
  - needs: a
  - refs: O76, O77, O78, O79
  - gives: answered 2026-11-15: the launcher runs EldenNet's engine off game once it is built (O76 (b); our own engine since O84); all settings but a short list change off game (O77 (a)); viewers in the devkit's tools (O78 (b)); the devkit shipped as part of EldenNet, behind Developer mode (O79).
- c The answers folded into EldenNet's and GraceUI's designs: the launcher's dashboard on EldenNet's engine off game, the settings index's off-game mark, Developer mode and the devkit's tools [planned]
  - needs: b
  - refs: O76, O77, O78, O79, G4
- d The owner answers GraceUI's G29 (its server for hosts outside the game) [done]
  - needs: a
  - refs: G29, O76
  - gives: decided 2026-11-16, as recommended: GraceUI publishes its server (our own since G30) as a library for hosts
    outside the game, EldenNet's launcher first.
- e GraceUI's server for hosts outside the game, folded into both designs as G29 decided [done]
  - needs: c, d
  - refs: G29, O76, GraceUI:DESIGN§7.2, ARCH§4.13
  - gives: folded 2026-11-16: `grace-serve` is one library for hosts outside the game under every rule of GraceUI's
    §7.2 (its item 26) and invariant 4, loopback only; G4 widened off game; the launcher serves through it (ARCH
    §4.13, DESIGN §3.2), with the rest of O76 left to c.

### S7.13 Spoilers by grounding alone (O86)

The owner decided on 2026-11-15 that the playthrough state, telling each NPC what it knows, what it is doing and its own
part in its quest, is the one spoiler mechanism, and that the design's extra guards go.

- S7.13.a The owner decides how strict spoiler prevention is (O86) [done]
  - refs: O86, O36
  - gives: decided 2026-11-15: grounding only; a few test questions per character check the model's own knowledge.
- b Fold O86 into the designs: the guards (locked-term sets, the output lock, reveal terms and tells, PL7's fixture checks, the second reader's reveal list, the fallback ladder) leave DESIGN, ARCH, MEM, NPC and the tests; GraceUI's SP2 is re-cut; every background job is grounded like dialogue [done]
  - needs: a
  - refs: O86, O36, G5
  - gives: folded 2026-11-16: grounding alone (DESIGN §10.4, MEM §7); test questions per character (NPC §12.1, MEM §7.8); GraceUI's SP2 re-cut as "shown, grounded or impersonal".

## S8 Our own engine, on SkyrimNet's logic (O84) [done]

- needs: S6.1

### S8.1 Our own engine as EldenNet's foundation

- S8.1.a Design and build the engine's core: the model client and its job slots, decision calls, and the template engine on inja with SkyrimNet's template rules; run it outside the game first; credit SkyrimNet's design [done]
  - refs: O84, O53, O60, NPC§7.1, ARCH§11, ARCH§11.8
  - gives: built 2026-11-16 on the engine branch: en-prompt (the template engine on inja, strict as NPC §7.1 says), en-llm (the client of ARCH §11 with its slots, error classes, health, reasoning retry, backup request and hourly budget, and the decision calls of ARCH §11.8) and the command, with render-prompt, chat and decide; 126 unit tests; a run from template to streamed reply against a local mock; it compiles for Windows.
- b Design what was deferred: memories of other NPCs and their recall, the memory job for NPC-to-NPC exchanges, keepsakes from another NPC's line, context assembly
  - needs: a
  - refs: DESIGN§7.8, MEM§4.5
  - gives: settled in part by f on 2026-11-16: each witness writes its own memory, with schema-bound fields (MEM §2.3),
    and recall after v1 sits behind the Recall seam (MEM §4.5). The caps across the roster, relations between NPCs,
    the exchange job's trigger and numbers, the keepsake from another NPC's line and context assembly stay here.
- c The TTS and STT seams, and the bio's file format
  - needs: a
  - refs: DESIGN§13, DESIGN§7.8
  - gives: the template engine and its rules, the LLM client with its job slots and the decision calls, first listed
    here, were built in a and g and folded in f on 2026-11-16 (ARCH §11, ARCH §11.8, NPC §7.1).
- d The changes folded into DESIGN, ARCH, NPC and MEM, and into GraceUI's pages that show them, with a review pass
  - needs: b, c
  - gives: the event engine, the game-access layer, identity by verified keys, the spoiler
    layers, the gates, pacing and read-only play stay as designed.
- e Re-base the design on C++ for EldenNet and GraceUI's in-game parts (O60; our own engine is C++, O84), and choose the C++ overlay in place of hudhook (O25, G15) with the owner's approval [done]
  - needs: a
  - refs: O60, O84, O25, G15, G30, WHY§103, WHY§104
  - gives: done 2026-11-16: DESIGN, ARCH, NPC, MEM, game data and GraceUI's design re-based from Rust crates on C++20 modules, with the behaviour, numbers and ids unchanged; the overlay chosen by the owner (Dear ImGui 1.93.0 and MinHook 1.3.4, O25 and G15 as changed); GraceUI's server decided as our own (G30), with an independent review's rules folded in (GraceUI D111 to D114, its §7.6); the core-to-overlay hand-off a triple buffer, so the present thread never waits (ARCH C6).
- f Fold in what SkyrimNet 1.2.0 shows our design lacks (WHY§98), and the Skyrizz session's measurements (WHY§99): the reasoning retry, our own decision format, the backup request, stance and concealment, plugin rollback, the store's traps, a generated function reference, grounded background jobs, per-witness memories, evaluation with test questions on the model's own knowledge [done]
  - needs: a
  - refs: WHY§97, WHY§98, WHY§99, O84, O46
  - gives: folded 2026-11-16: ARCH §11 (the reasoning retry, the backup request, the quantisation floor, the hourly budget), ARCH §11.8 Decision calls (new), Augur's stance and concealment (ARCH §8), the store's rules, NPC §7.1's template rules, MEM §3 and §4's per-witness memories and recall, DESIGN §13's plugin interface rules, the evaluation in NPC §12.
- g The engine's next increment: the WinHTTP transport for the Windows build, the local probes and `/v1/models`, Ollama's warm-up, and the asynchronous handle (submit, cancel, events) that the core drives [done]
  - needs: a
  - refs: ARCH§11.1, ARCH§11.3, ARCH§11.4, O84
  - gives: built 2026-11-16 on the engine branch: the WinHTTP transport, loaded by its full path from the system directory, with one conformance suite for it and libcurl, run under Wine; the local probes with the probe and models commands; the asynchronous handle (submit, cancel, the input epoch, events with backpressure, exactly one end per request) and Ollama's warm-up; 125 tests on Linux and 127 under Wine; every command run through the Windows build under Wine (WHY§101).
- h Run the engine on Windows itself, against a local server and a cloud provider: what Wine cannot show (WinHTTP with the system's proxy, TLS with real certificates, IPv6 loopback, cancelling on Windows) [active]
  - needs: g
  - refs: ARCH§11.1, WHY§101, WHY§108
  - gives: run 1 on 2026-11-16 (WHY§108): the home network's llama-server, TLS with real certificates and the tests
    pass, except two WinHTTP checks that real Windows answers differently from Wine (the connect deadline, the TLS
    failure's wording), fixed in m; run 2 the same day, with a cloud key: the reply and the backup request work against
    a real provider, and the cloud decision missed its 1.5 s. Still to run: cancelling a reply part-way, and run 3's
    timings of NanoGPT's candidate models, which choose `cloud-nanogpt`'s (O88, n; ARCH §11.4).
- i Build on the designed toolchain: clang-cl and lld-link against the xwin SDK and CRT on Linux (pinned), the SEH reader, sanitizers on the Linux test runs and the first fuzz targets, beside the MinGW build that Wine runs [done]
  - needs: e
  - refs: ARCH§12.1, DESIGN§2.1, WHY§102, WHY§103, WHY§111, WHY§112
  - gives: built 2026-11-16 on the engine branch: clang-cl and lld-link against xwin's CRT and SDK, pinned in
    `cmake/xwin.lock`; en-gal's SEH-guarded copy, its ARCH §6.5 sketch corrected (a never-inlined copy, guard pages
    caught); AddressSanitizer, UBSan and ThreadSanitizer builds and three libFuzzer targets on Linux. Every build and
    test passes, under Wine for the Windows builds.
- j Bring the built engine to the re-based rules: `en-prompt` takes a template source, so `en-core` does no I/O (ARCH §2.3); `LlmRequest` and `LlmEvent` as plain data in `en-proto` (ARCH §2.2); `ffi_guard` on the transports' callbacks (R3) and a note on each cast (R9); no static with a destructor that runs at exit; the SSE caps and the net threads' stacks as ARCH gives them [done]
  - needs: e
  - refs: ARCH§2.3, ARCH§2.4, ARCH§3.1, ARCH§10.2, DESIGN§2.6, WHY§103, WHY§106
  - gives: built 2026-11-16 on the engine branch: `en-prompt` takes a template source (a folder for the tools; memory
    for `en-core`, which then opens no file); `en-proto` holds `ffi_guard` and the model client's messages as plain
    data with their JSON forms (a decision call's rules pick is data); `en-llm`'s foreign entries start with
    `ffi_guard`, its casts carry their notes, its net threads run on 2 MB stacks and ARCH §10.2's caps hold; Clang
    refuses a dynamic initialiser or an exit-time destructor in every module the DLL links. GCC, Clang and Wine pass
    every test.
- k The template engine's last caps: output at most 64 KB and a bound on render steps (ARCH §10.2)
  - needs: j
  - refs: ARCH§10.2, NPC§7.1
- l The handle's give-way test failed twice in about twenty runs of the suite under heavy load, and in none of about 1,000 since: find the race, from what the test now reports when it fails
  - needs: j
  - refs: ARCH§11.3, WHY§106
- m Fold WinHTTP's two findings from the first Windows run: the driver's own connect deadline, and a TLS failure told by how far the exchange got [done]
  - needs: h
  - refs: ARCH§11.1, WHY§108, WHY§109
  - gives: built 2026-11-16 on the engine branch (WHY§109): the driver's own connect deadline and TLS failures told by
    phase, the wording a pure function tested on every build; the second Windows package went to the owner the same
    day for run 3, with a step that times NanoGPT's candidate models (O88).
- n A provider for every use, cloud by default (O88): presets with several providers, a slot per use with `actions` and `vision` reserved, OpenRouter's and NanoGPT's presets with measured models, the wizard leading with a cloud key, and Settings › AI with a row per slot [done]
  - needs: m
  - refs: O88, ARCH§11.1, ARCH§11.4, ARCH§11.7, DESIGN§11.5, PAGES§4.5, WHY§108, WHY§110
  - gives: folded and built 2026-11-16 (WHY§110): ARCH §11.1 (a provider for every use), §11.4 (`actions` and `vision`
    reserved, presets in two forms, OpenRouter's `cloud-budget` beside NanoGPT's `cloud-nanogpt`, the 1.5 s rule) and
    §11.7, DESIGN §11.2, §11.3, §11.5 (the wizard leads with a cloud key) and §13, PAGES §4.5 and §4.6.2 (a row per
    slot); the engine reads both preset forms and the reserved slots, with GCC and Clang en_llm 155 cases and Wine
    157. NanoGPT's models wait on run 3's timings (h).

## S9 The roster pipeline [planned]

- needs: S6.2
- refs: DESIGN§7.8, O43, O45, O46

About 140 profiles for the base game, the six personas included: about 170 hours of authoring
and capture on v1.0's critical path (DESIGN §7.8, risk K29). Drafts come from our own notes,
never from game text, and every field is reviewed by hand. Batches go by region or hub, so that
a batch's captures share a stretch of play.

### S9.1 The roster's tools

- S9.1.a The roster scan, draft and verify tools, and the lints PL10 to PL12, built now that O46 is decided [next]
  - needs: S6.2.a
  - refs: NPC§4.8, DESIGN§7.8

### S9.2 Each batch, up to its review

- S9.2.a The census on the author's machine: names stay local, and the index gets ids and numbers only
  - needs: S9.1.a, S8.1.d
  - gives: its leads earlier, from the game-data research (S6.3, O54).
- b A research note per NPC in our own words, with the later truths that give its test questions (O86)
  - needs: a
- c A draft from each note, every field reviewed by hand, and a second reader's spoiler review at first-meeting level
  - needs: b
  - gives: bound, evaluated and shipped batch by batch: the first in v0.5 (S14.2), the rest of
    the base game in v0.6 (S15.1.d), all of it verified by v1.0 (S16.1.c).

## S10 v0.1 Eyes [planned]

- needs: S8
- refs: DESIGN§16.2

The game-access layer, the event engine and the gates are our own and stay as designed (DESIGN §7.8),
but the code starts on our own engine (O84), so v0.1 follows S8. No LLM yet.
Since G15, GraceUI's 0.1 Host ships with v0.1 (S10.4): EldenNet's banner and its F11 debug view, which
v0.1's testing uses, draw through it. Since O90 the spikes S1, S2, S6 and S7 run without it (S6 and S7 with a
development Mark in the launcher), and only S0-E's present-hook half waits on it (S10.1.g).

### S10.1 The spikes for v0.1, in game

- refs: DESIGN§16.1
- S10.1.a The spike and recording build, its recordings in numbers that DataPiler imports (O73) [active]
  - refs: O73
  - gives: planned on 2026-11-16 in thirteen parts (two builds from one code base). Parts 1 to 10 of the spike
    build are built (2026-11-16 and 17), on the engine's branch, with the package 0.1.0-spike.1 sent for R1 (WHY§197
    to §204); part 10's S0 package and the launcher's smoke run wait for R1's review. Parts 11 (Augur v0.1 with golden
    files) and 12 (en-overlay on GraceUI's ABI 1 preview) started on 2026-11-20, after the layering fix (WHY§211).
    Part 12 built the same day on GraceUI f51bcc7 (engine 9a42539): en-overlay connects to GraceUI on T1 in both load
    orders, with the banner and pause badge as notices, the world-state provider (danger and a game menu as v0.1
    stand-ins), the F11 view on Dear ImGui's core behind `debug.panel`, the Mark on GraceUI's hotkey with the launcher's
    Pause as the fallback, and the launcher's GraceUI pre-flight (E12); tested against a fake GraceUI table and end to
    end under Wine with GraceUI's test build on Direct3D 12 (WHY§215 to §221). Part 11 built the same day (engine
    47fd54a): en-augur v0.1, pure and game-neutral, run in every core step with 25 shipped rule files; the `.scn`
    scenario language, `eldennet-engine golden` and three synthetic routes with golden files; `replay` driving Augur
    from recordings; the property and backpressure tests; about 26 µs a busy step (WHY§222 to §226). Part 12 moved to
    GraceUI 3f1352b the same day: an unbound hotkey answered as such, the helper's stop-if-absent, GraceUI's own
    folder and graceui.toml (WHY§230 to §233). Since 2026-11-22 (O106) R1 is retired, so part 10's R1 package and probe
    mode go (S10.1.f); the S0 package waits on Lodestar's table instead of R1's review (S10.1.b, S10.1.k).
- b Spikes S0-A to S0-D: the untouched image, the reader without dearxan (the task before O101), the launcher and offline play, guarded reads [waiting]
  - needs: a, f, m
  - gives: since 2026-11-22 (O101, Lodestar's L4) S0-A checks the ids of the build's table in place of the finder's hits
    and five patterns, and S0-B the reader on EldenNet's own thread for 2 h in place of the game's task (DESIGN §16.1);
    both wait on L5's first table (S10.1.k).
    Since 2026-11-23 they wait on a ready start's layout (S10.1.m; WHY§460).
    The S0 package built on 2026-11-23 on the table-based start (engine merge of wip/s12-s0; WHY§479 to §487): S0-A
    checks the table's ids at the start and again at its end, the image untouched; S0-B reads 2 h on T2 with its steady
    counts, and says in plain words that it waits while the start's layout is incomplete; S0-C the launcher and offline
    play; S0-D 40 forced faults caught and crashes on request recorded. Each run's facts in its own folder; spike-report
    reads the folder whole; the tester's panel lists each run's steps. Checked on the fake game whole and lacking
    `game_state`, as the real game is today. Open: whether a start stopped at the banner installs the crash filter
    (WHY§484; decided 2026-11-23, WHY§510: it does). Package 0.1.0-s0.1, not yet sent: S0-A, S0-C and S0-D can run on the real game now, S0-B after S10.1.m.
    Package 0.1.0-s0.1 rebuilt from engine 31a98e4 and sent to the owner the same day: the sums, help and version under
    Wine, its nine package cases on the fake game (whole, and lacking `game_state` as today) and the sweep passed.
- c Spikes S1 perception and S2 text without GraceUI, and S6's core flags and S7 combat with the launcher's development Mark [waiting]
  - needs: b
  - refs: O90
  - gives: until O90 (2026-11-16) this part ran on GraceUI's 0.1 Host, with S0-E's present-hook half, now g.
- d Spike S8 and S3's additions, on v0.1's recordings
  - needs: c
- e What the review left to measure: the fallback copy's cost, MinHook in the first build (GraceUI's host since G15; with Dear ImGui in place of hudhook since 2026-11-16, O25), the play-time width, a Proton path
  - needs: b, c
  - refs: REVIEW§4.2, REVIEW§4.3
- f The derivation round: the game's executable read with the game closed, as a DataPiler job (S7.11.h), then one probe session from the launcher; each returned file reviewed, and its derivation notes written, before anything is relied on [done]
  - needs: a
  - refs: O89, ARCH§6.2, ARCH§6.10
  - gives: R0 ran on the owner's PC (two fixes, then R0c settled the offline facts: WHY§180 to §189,
    docs/ops/probes/), and R1's package was sent. R1 run 1 (2026-11-17, spike.2 after spike.1's launcher failed on
    Windows): the idiom and the task group confirmed, the class names not read (9 of 217), so no call; spike.3 sent
    (WHY§206). Run 2 showed the names are not in the image at all; the owner chose the instances' RTTI (O92), and
    spike.4 was sent (WHY§207). Run 3: CSTask named, every check passed and the call returned ok, but the task never
    ticked at the title screen; run 4 loads a save after the call (WHY§208). Run 4: no tick in the world either, and a
    crash on quit outside our module; spike.5 searches where the task was stored (WHY§209). Run 5 (no other mods):
    the call returned ok, and our task was nowhere in the hub's reachable memory; spike.6 searches all memory and
    summarises the callee (WHY§227). Spike.6 sent on 2026-11-20 (engine fbf4c8b; WHY§234). Run 6: the callee is a
    wrapper handing the group and the task to two inner calls, and 8 of 10.6 GiB held no copy of our task; spike.7
    follows them (WHY§235). Spike.7 sent on 2026-11-20 (engine bddcaa7; WHY§236). Run 7: the wrapper tail-jumps with
    the group and the task to a function that bounds the group by 168 and refuses on a null entry; all readable memory
    held no copy of our task; spike.8 reads that table and the call's return value (WHY§240). Spike.8 sent on
    2026-11-20 (engine 9a6e3ea; WHY§247). Run 8: the call returns 1 and group 110's entry exists, yet nothing keeps our
    task; spike.9 reads the group's own function and its memory (WHY§248). Spike.9 sent on 2026-11-20 (engine
    a51ab03; WHY§258).
    Run 9 (2026-11-21): the call returns 1, and the group's function only null-tests our task and hands it, with one
    word of the entry, to a method of a second singleton (0x2658FB0, not yet read). Nothing in memory keeps our task,
    plainly or tagged, and the group's memory did not change. The launcher's log stopped at the call, so no 30 s text
    was shown and the crash watcher was blocked. Spike.10 reads that method and the second singleton, and the
    launcher's console no longer blocks its watch (WHY§289, WHY§290). Spike.10 sent on 2026-11-21 (engine b5c2966;
    WHY§291, WHY§292).
    Run 10 (2026-11-21): G's method never reads our task; it wraps it in a 32-byte request of its own and hands that
    at once to 0x1267F0 or queues it on G, by one byte of G's. Nothing keeps our task, and the launcher's fix held.
    Spike.11 reads the request's methods, 0x1267F0 whole, G's queue, and the game's own requests and tasks (WHY§293).
    Spike.11 sent on 2026-11-21 (engine da2fdfc; WHY§295).
    Run 11 (2026-11-21): the request runs at once, as its own slot 1, and passes our task on only if group 110's word
    is in a map of the manager's; it was dropped, with no copy left. Spike.12 reads that map at the title and in the
    world, after a save is loaded without a second call (WHY§296).
    Spike.12 sent on 2026-11-21 (engine 5e21387; WHY§299).
    Run 12 (2026-11-21): every group, 110 included, is in the manager's map at the title, and the function it leads
    to removes a task from the group's lists: the derived register_task is the game's unregistration, and its sibling
    is the likely registration. Spike.13 reads the sibling's chain and calls it only if its chain appends (WHY§302).
    Spike.13 not built: on 2026-11-21 a safety classifier stopped the work while game code was being written out, and
    the route is with the owner as O100 (WHY§303). O100 decided the same day: R1's road goes on as Lodestar's work,
    EldenNet's own address library (S10.1.k).
    Since 2026-11-21 (O101, O102) R1's package can make no call: it is legacy until Lodestar's read-only L5 package.
    Superseded by S10.1.k and retired by the owner on 2026-11-22 (O106; WHY§363): R0 lives on as `lodestar find`, and
    R1's package, the DLL's R1 mode and the launcher's `--probe` are removed by Lodestar. What R1 found stays in the log
    and `docs/ops/probes/`.
- g Spike S0-E's present-hook half on GraceUI's 0.1 Host, in both load orders, beside another overlay [done]
  - needs: a, S10.4.a
  - refs: O90, G15, GraceUI:D175, WHY§549
  - gives: planned with the Grace session on 2026-11-20 and run beside R1, independent of our task. GraceUI's side
    built the same day (GraceUI feat/spikes 4dade85, D175): grace-spike.dll, an observer that connects as a mod and
    writes a report of numbers into the folder the launcher names; read-only counters in graceui.dll's test build;
    grace-standin.dll, our own stand-in for another overlay, patching Present before or after GraceUI; and a reader
    with the pass rules. The owner's runs A1 to A4 (both load orders, injected early and late) and A0 (GraceUI alone).
    EldenNet's side: the launcher's spike words s0e, s0e_alone and s4, the spike folder in the game's environment,
    and eldennet.dll making no call into the game in these runs (WHY§241 to §243). EldenNet's side built the same
    day (engine 5c5b3bc; WHY§249 to §251). The owner's package sent on 2026-11-20: engine def31b4 with GraceUI 0.2.2
    at f5c16a2, runs A1 to A4, A0 and B checked on the fake game (WHY§255 to §257).
    Repacked on 2026-11-23 at Grace's request, on the owner's go-ahead: 0.1.0-spikes.2, engine fd6e23f (the release DLL
    with the first conversation) with GraceUI 0.2.22 at b488e3c. BUILD.md's checks passed: the sums, help and version
    under Wine, the six package cases (A2 stalled once under the machine's load and passed three times alone), the 11
    GraceUI cases, GraceUI's spike and spike-after scenarios, its report tool on each run (no row failed), the sweep.
    Sent to the owner the same day (WHY§255's Later line).
    The tester's panel for these runs too, at the owner's request ("implement instruction box overlay for your in game
    tests", 2026-11-23, after Lodestar's advice): each run's steps with their timers, Pause confirming, and a Mark in
    the ring confirming once EldenNet holds it; whether the panel was shown goes into each run's record (WHY§485). To
    be repacked as 0.1.0-spikes.3 with GraceUI 0.2.24. Repacked from engine 31a98e4 with GraceUI 0.2.24 at 9360224 and
    sent to the owner the same day, in place of spikes.2: the sums, help and version under Wine, 14 GraceUI and spike
    cases, the six package cases (the panel ticking through each run's steps), GraceUI's spike and spike-after
    scenarios, its report tool on each run (no row failed) and the sweep passed.
    The owner's run A1 crashed twice the same day, at GraceUI's first Present, before a frame was drawn, beside other
    overlays (WHY§509); with Grace. A start stopped at a banner now installs EldenNet's crash filter (WHY§510).
    A0 (no EldenNet) crashed the same way. The next package adds run A5 (GraceUI and its observer alone) and 200 ms
    state lines, with Grace's test build of the first frames (WHY§511).
    Packed and sent the same day as 0.1.0-spikes.4 (engine 7b8c987, GraceUI 0.2.27 at 2286f20): 15 GraceUI and spike
    cases, the seven package cases, GraceUI's two spike scenarios, its report tool on each run and the sweep passed.
    A5 reached the title and opened GraceUI's menu (WHY§517); the stand-in was the cause of the crashes, so no run loads
    it any more (WHY§518). Open with Grace: the menu's pointer pulled back by the game's cursor (0.2.29), and the
    owner's two decisions on Alt+Tab and Tab. The next package follows them.
    The owner's spikes.4 runs the same day (WHY§529): A5 and B ran to their end (B on the older GraceUI 0.2.24 left in
    the game's folder), A1 fell twice at GraceUI's first Present with EldenNet connected and no stand-in, so its cause
    is open with Grace (WHY§518's Later line). Every run of the next package checks the game's GraceUI against its own
    first (E13, WHY§528), and its chat box takes no mouse (O113).
    Packed and sent the same day as 0.1.0-spikes.5 (engine a64486f, GraceUI 0.2.30 at a04a234: D232's pointer, G40,
    G41 and the observer's fault recorder, 0xC0000374 included): the sums, help and version under Wine, 18 GraceUI and
    spike cases, the six run cases and the other-GraceUI case, and the sweep passed. A1 is kept as it was, so that the
    recorder names the caller if it falls again.
    The owner's spikes.5 runs the same day (WHY§549): S0-E passed in all five runs (A5, A1 to A4), in both load
    orders, beside the Steam overlay and another overlay, and A1 no longer fell; S4 passed all it could measure. Every
    run ended at the quit with the same fault in the game's own teardown, as did a read round without GraceUI
    (WHY§550): with Grace and Lodestar, and a control run with nothing injected asked of the owner.
    Packed and sent the same day as 0.1.0-spikes.6 (engine bf92dbd, GraceUI 0.2.33 at 2e219de: D236's pointer, G42's
    focus ring, the typing test on the keyboard and pad alone, D235's heap guard): 19 GraceUI and spike cases and the
    seven package cases passed. At its title the owner again read EldenNet's banner (O114's "files incomplete")
    as a GraceUI warning: since engine 961cae6 an observer run shows the test run's plain note instead (WHY§565),
    for the next package.
    Status on 2026-11-27: done. In spikes.5, S0-E passed in all five runs (A5, A1 to A4), in both load orders, beside
    the Steam overlay and another overlay: hooked once with 9 hooks, EldenNet connected, no allocation on the present
    thread after warm-up (WHY§549). The fault at every quit is the game's own (WHY§550, §556). S0-E's input half is
    S11.1.a's.
- h O89 to O91 folded into the designs: the derivation round (ARCH §6.2, §6.10), the spikes without GraceUI and the launcher's Mark (DESIGN §14, §16.1), the third-party rows (DESIGN §16.1) [done]
  - refs: O89, O90, O91, ARCH§6.2, ARCH§4.14, DESIGN§16.1
  - gives: folded on 2026-11-17 with the first in-game build's choices (WHY§159 to §204): ARCH
    §6.2 to §6.5 and §6.10 (the round, what R0c showed, the schemas and the seam as built), §2.3,
    §3.2 with the crash filter, §4.1 and §4.14 (the launcher's options, the watch block, the Mark),
    §5 (not solo means dormant); DESIGN §3.1, §14 and §16.1; dated notes on O89 and O90.

- i Parts 11 and 12 folded into ARCH and DESIGN: the connect and its look (ARCH §1.4, §3.2), T6's calls to GraceUI (§1.2), en-overlay as built (§2.2), the GraceUI pre-flight and E12 (§4.3, §4.7), the Mark's hand-over on Pause (§4.14, DESIGN §14, MEM §6.3), the settings read (§11.5), the DLL's imports (§12.1), and Augur as built (§8) [done]
  - needs: a
  - refs: ARCH§1.4, ARCH§8, DESIGN§14
  - gives: listed by part 12's notes on 2026-11-20; Augur's folds joined when part 11 landed the same day: §8's
    opening (Augur after the lifecycle slice in the same step, WHY§222), §8.4, §8.5 (the rule schema's additions and
    Struck's 1 s window) and §8.7 as settled (WHY§223), §8.12's lossy-run differences (WHY§224) and §7.5's recording
    reader (WHY§225). ARCH §1.4's "As built" bullet (the first build headless) is superseded by part 12.
    Folded on 2026-11-20 (design/v1-proposal, merge of docs/s10-fold-i): ARCH §1.2, §1.4, §2.2, §3.2, §4.1, §4.3,
    §4.7, §4.14, §7.5, §8 (its opening, §8.1, §8.4, §8.5, §8.7, §8.12), §11.5 and §12.1; DESIGN §2.4, §14 and §16.1
    and MEM §6.3 by dated notes; a dated note on O90 (the launcher's Mark kept as the fallback, WHY§217).
- j O96 and O97 folded into the design: the System One api style (ARCH §11.1, §11.4, §11.8, DESIGN §11), and decisions choosing the addressee of the player's lines and of NPC-to-NPC lines (NPC §5, §9.2, §9.4, §9.8, DESIGN §8.1, §9) [done]
  - refs: O96, O97, ARCH§11.8, DESIGN§11.2, WHY§282
  - gives: decided by the owner on 2026-11-21; built with the decisions slot in v0.4 (S13.2.h). Folded the same day
    (merge of docs/s10-fold-j) into ARCH §11.1 to §11.4, §11.8 and §12.2; DESIGN §0, §1.2, §7.8, §8.1, §8.2, §9.7, §11.1,
    §11.2, §16.2, §16.3 and §18; NPC §3.1, §4.5, §5.1, §5.2, §5.7, §6.1, §7.8, §9.2, §9.4, §9.6, §9.8 and §13; PAGES
    §4.6.2, §4.10 and §4.11. Six readings are marked for the owner's review (WHY§282).
- k Lodestar: EldenNet's own address library for Elden Ring, as SKSE's Address Library is for Skyrim, and the services
    built on it, by the Lodestar session (proposal 0005) [active]
  - needs: f
  - refs: O100, O101, O102, O103, O105, O106, O47, O48, O49, O89, WHY§302, WHY§303, WHY§304, WHY§305
  - gives: started on 2026-11-21 by the owner's decision O100, as a part of EldenNet, in a session of its own on a
    larger model (handover-2026-11-21). Stable ids of our own for the game's functions and data, per game build, found
    and checked from the player's own executable with nothing of the game kept but numbers; then what EldenNet builds
    on it, first the game's own way of adding a task (WHY§302). Its first piece is proposal 0005, with its questions to
    the owner. R1's road (S10.1.f) goes on here.
    Proposal 0005 written the same day (docs/proposals/0005-lodestar.md; WHY§304, WHY§305), its questions put to the
    owner as O101 to O103. Its steps: L1 the catalogue, the table's schema 2, its loader and lint, the fake game's table;
    L2 shapes for the accessor, witnesses and digests, `patterns.toml` retired; L3 `lodestar find`, `check` and `diff`,
    report schema 2 (one offline run on 2.7.2.0, game closed); L4 the checks at start and the frame service on EldenNet's
    own thread; L5 a read-only spike package, no call (one run); L6 the members, in read-only rounds; L7 the task
    service's research, later and by the owner's choice. L1 started, since it holds under every answer.
    L1 done on 2026-11-21 (engine fb999ca): `gal/ids.toml`, the catalogue of 20 ids of our own, and layout schema 2 with
    its loader and lint; nothing that runs in the game changes yet (WHY§306). L2's shapes wait on O102; meanwhile the
    resolver's placement and witness checks, on invented images.
    The owner decided O101 to O103 the same day, as recommended: v1 reads from EldenNet's own thread with no call into
    the game, `gal/` holds no bytes from the game (O89 amended), and the library serves EldenNet alone in v1.
    Of L2 and L4, the resolver with its placement and witness checks built on invented images the same day (engine
    c2401d0; WHY§307). L2's first half the same day: the accessor's shape in our own grammar, matched beside the pattern
    with the same accepted sites on invented programs (engine 33cf307; WHY§308). L2 done the same day (engine 6da3291):
    `gal/recipes.toml`'s accessor in use and `gal/patterns.toml` withdrawn, so `gal/` holds no byte of the game (O102;
    WHY§310); digests moved to L7 (WHY§311). R1's package can no longer make its call and is legacy until L5's
    read-only package replaces it. L3's tool and package built the same day (engine ffc172b; WHY§312): `eldennet-engine
    lodestar find` and report schema 2, kind `lodestar`; the owner's offline run (about 5 minutes, game closed) and its
    review come next, then L4.
    The finding package (EldenNet-Find.zip, 0.1.0-lodestar.1) sent to the owner the same day (WHY§313).
    The owner's run on 2026-11-22 (docs/ops/probes/2026-11-22-l3.md): R0c reproduced; the shape at 7,077 places, all also
    the pattern's, the pattern 4 more; the class objects no objects with methods, so O92's way is the only one (WHY§314,
    WHY§315). A second package (0.1.0-lodestar.2) asks why those 4 differ; then the recipe's expected count, then L4.
    The second run the same day: the 4 sites test another register than the one loaded, so they are no manager's slot;
    the shape is right, its expected count is 7,077 on 2.7.2.0, and one class object goes with each static (engine
    4624553; WHY§316). L3 done. Next: L4 (discovery through the table, the reader thread), then L5, the read-only package.
    L4 done on 2026-11-22 (engine 3f65014; WHY§317 to §321): a start takes each id the build's table holds from the table
    and checks it, searching for nothing the table holds; the reader on EldenNet's own thread (T2) with steady reading
    (torn readings dropped, lifecycle changes held for a second sighting, faults confirmed twice); no registration and
    no getter in a v1 start; ARCH §1.2, §3.2, §6.2 to §6.4, §6.10 and DESIGN §2.1, §4.2, §16.1 updated. On the real build
    a start ends at the discovery banner until L5 writes the first table. Next: L5, the read-only in-game package.
    L5 built the same day (engine 7995413; WHY§322 to §325): the read-only round (managers named by their instances'
    classes, the draft table checked as a start checks it, the reader's trial), the launcher's `--read` and the read
    package (0.1.0-lodestar.3); the owner's run (about 15 minutes) is next. After its review: the first table for
    2.7.2.0 enters `gal/` as leads, R1's package and `probe_r1.cpp` retire with the owner's word, and S10.1's items that
    waited on R1's call only for the managers' places are superseded by this round.
    L5 done on 2026-11-22 (engine d8b3777; the owner's run; docs/ops/probes/2026-11-22-l5.md; WHY§326 to §330): eight of
    the eleven managers placed, the reader read-only on the real game for 19 minutes with no fault, and the first table
    for 2.7.2.0 in `gal/` as leads. Open: event_flags, world_clock and messages. Next: a count per lead of the classes the
    image holds under its name (one offline run, game closed), the naming cap lifted, the trial counted by stage; then L6
    (the fields by invariants), which also takes the three if they have no type information. R1's package and
    `probe_r1.cpp` can retire when the owner agrees.
    The count per lead built the same day (engine merge of 9e17bab): in every lodestar report, the classes the image holds
    under each lead's name and its name with Imp, and those deriving from them; a lead with no class of its own named by
    the one deriving from it; naming with no cap; the trial by stage. Its offline run (package 0.1.0-lodestar.4, game
    closed, about 5 minutes) is with the owner; its answer decides how event_flags, world_clock and messages are found.
    The count's run the same day (WHY§331, WHY§332): messages and event_flags have classes of their names, so they can be
    named by their instances; world_clock has none and needs L6's invariants. Next: one more read-only in-game round, with
    instances inside the image read, a read-only search of the data slots for a lead's class, and L6's first invariant
    searches (the play time) on the reader's thread.
    The second read-only round built the same day (engine merge of 2ebeb19; WHY§333 to §335), with the owner (package
    0.1.0-lodestar.5): it looks again for event_flags and messages, and starts L6 with the clock and the play time by what
    their objects hold; its searches ask Windows before they read.
    Its run the same day (WHY§336 to §339): the eight managers and the table the same as in the first session; the reader
    again without fault; no clock in whole minutes and hours; one candidate for the play time; event_flags and messages
    still unnamed. Next: an offline run counting a second accessor shape and alternative names for the three leads, then
    an in-game round with the searches budgeted by time and their hits ranked.
    The second round reviewed with its logs the same day (WHY§340 to §342): messages found behind two slots, lost by a
    fault of the round's and mended; the event flags behind no slot. The third round built (engine 8815db2; WHY§343):
    searches keep what they find, candidates asked for first, a 4 ms share, a member search, five variant recipes of the
    accessor; with the owner (package 0.1.0-lodestar.6).
    The third round reviewed the same day (engine f95f3cb; WHY§344 to §348): nine of the eleven managers named (messages
    by the slot search), twelve leads in the build's table; event_flags and world_clock still open; the variants counted,
    gap_near withdrawn; the play time's candidate the same across two sessions. Next: a fourth round for the last two.
    The fourth round built the same day (engine 0d79ece, Lodestar's 606167d; WHY§351 to §353): the object scan, a wide
    read-only search of the game's private memory that the owner agreed to (O105), naming by a longer name (by_related),
    the flags' store as a lead on trial, the member search over 0x4000 bytes, and the code's references as witnesses;
    report schema 4; package 0.1.0-lodestar.7, its README corrected (engine 539d341), sent to the owner the same day.
    The fourth round reviewed the same day (engine 2522e1f; WHY§354 to §359): the flags' store found behind one of the
    accessor's statics, the world's clock a field of another manager, messages with eight witnesses; 12 leads in the
    table. The scan did not finish and slowed the reader, and stays off (WHY§357). 1008 `event_flags` is the object the
    flags are read through, and 1012 `world_clock` waits for L6 (WHY§359). Next: a round that names the store by O92's
    rule, then L6's first round (the clock's and the play time's fields), no scan. The owner retired R1 the same day
    (O106; WHY§363): Lodestar removes its package, the DLL's R1 mode and the launcher's `--probe`.
    L6's first round built the same day (engine 4c882c3, Lodestar's 24e2918; WHY§365): the watch on the named managers,
    the world's clock and the play time; event_flags named through the store; no scan, after the owner saw the fourth
    round's scan stutter the game for a few seconds (WHY§357). Package 0.1.0-lodestar.8 sent to the owner the same day
    (engine 8e09907; no "Pass time" at a grace in this round, since a jump of hours counts against the clock's test).
    L6's first round reviewed the same day (engine 9050db4; WHY§368 to §371): event_flags named (10 of 11 managers, 13
    leads), the play time's field found in four sessions, the clock not where its class is. R1 removed (O106; WHY§363,
    §372). Next: mend the report's size and the ask-first share, then look for the clock more widely and read along the
    way to a flag.
    L6's second round built the same day (engine eff28bf, Lodestar's 48e5003; WHY§374): the clock looked for in every
    static's object, the flags one step on, save_data by invariant; the report bounded and the ask-first share checked
    after every read. Package 0.1.0-lodestar.9, for the owner's run.
    L6's second round reviewed the same day (engine 0b958f2; WHY§381, §382): save_data placed by invariant (14 leads),
    the reader at 9.6 a second, no slow clock in any static's object (WHY§382's Later line, §385). Next: the clock by a
    jump (the owner lets time pass at a
    grace), the flags' next step reported and watched bit by bit, and the report's fallback graded. Lodestar puts that
    round's plan to the owner first, since it asks more of them than walking about.
    L6's third round built the same day (engine a484129, Lodestar's 9f451ec; WHY§389, §390): the clock by a jump when
    time is let pass, the flags two steps on with a bounded sweep for bits that flip once (about 24 MiB, put to the
    owner first by O105's note), and the report's fallback graded. The owner said yes to the sweep (O105's note), and
    package 0.1.0-lodestar.10 went to them the same day (engine b0a2fa7).
    L6's third round reviewed the same day (WHY§394 to §397): the time of day found (hour and minute in one word behind
    an accessor's static, told by the owner's two passings of time), candidates for the flags' blocks and tree, the
    reader at 9.97 a second. 1012 `world_clock` is that object (WHY§359's Later line). Next: the clock into the table by
    what its object holds; a flag the owner sets checked against the blocks, with the tester's marks (S10.1.l).
    L6's fourth round built the same day (engine 9ba4556, Lodestar's 3030114; WHY§444, §445): the tester's panel and
    its marks for the sweep, the clock written by what its object holds, the play time judged by what it does.
    Package 0.1.0-lodestar.11, for the owner's run.
    L6's fourth round reviewed the same day (WHY§447, §448): world_clock in the table (15 leads), flags seen set at
    the owner's marks. The fifth built (engine be13ea4, Lodestar's 99b3de1; WHY§449): the tree beside the flags read
    once and joined to the marked flips (at most 32,768 records of 64 bytes, about 2 MiB, within the owner's yes of
    O105's note). Package 0.1.0-lodestar.12 sent to the owner the same day (engine 620dcd7).
    L6's fifth round reviewed the same day (WHY§451 to §453): the flags' tree read whole; key, mode and block index
    placed; the flags decode to numbers. Next: the reader's change (a value is an index, WHY§452), a round that reads
    flags by number as the reader will, then the flags' fields into the layout.
    The reader's change made the same day (engine 6d56207; WHY§454): a value is an index. L6's sixth round built
    (package 0.1.0-lodestar.13): the reader's way down the tree tried against the bits the sweep sees, and the divisor
    looked for.
    L6 done as far as its three subjects go (2026-11-23; WHY§455, §456): world_clock, save_data and the event flags'
    fields are in the build's layout as leads (engine 9d8fb57), after ten read-only rounds. Next (WHY§457): a round
    that runs EldenNet's own flags reader on T2 beside the round's tree reader; then the start's use of the layout
    (S10.1.b).
    The eleventh round built the same day (engine 998d078; WHY§458): EldenNet's own reader on the flags, alone, through
    the build's layout, compared with the round's tree reader and the known flag (package 0.1.0-lodestar.14, for the
    owner's run). A ready start still needs one static (`game_state`) and 36 fields (WHY§459): S10.1.m.
    The eleventh round reviewed the same day (WHY§462): discovery through the layout and the backend's flags group work
    on the game (flag 100 read as set, 44 frames, no fault); the marked flags' comparison moves to the twelfth round
    (WHY§464).
    The twelfth round reviewed the same day (WHY§468): the backend's flags group agrees with the game on four flags
    the owner changed and the always-set one (WHY§457's comparison done).
- l A tester's panel for the owner's rounds: each task in order, with a timer and a mark when done (the owner's request) [done]
  - needs: k
  - refs: WHY§398, WHY§396, WHY§444, WHY§445
  - gives: asked by the owner on 2026-11-22, after the seventh read-only round. By Lodestar: in game through GraceUI if
    its overlay can show it there today, else a window of the launcher's own. The tester's marks also tell the round
    when to look. Built the same day with the eighth round (engine 3030114): the launcher's task panel, with --read,
    the tasks in order, timers, Pause or a click to confirm; the confirmations are Marks in the watch block's ring
    (WHY§444, §445). GraceUI's overlay is not cleared for the real game yet (S10.1.g, S11.1.a).
    Proved on the owner's PC in the eighth round (WHY§450); the owner's one change, a button that speaks for its task and
    none for the last, is in the ninth.
- m The layout for a ready start: `game_state`, then the session's, world's and player's fields and the snapshot's mode, by L6's ways [active]
  - needs: k
  - refs: WHY§459, WHY§460, ARCH§6.7, ARCH§6.8
  - gives: counted by Lodestar on 2026-11-23: one static and 36 fields. The order and the rule for leads are Engine's
    ruling (WHY§460): `game_state` first; then pointers and lists by search; then moving numbers by invariant with panel
    tasks (any task with a cost optional); then constants as leads checked by their properties, the session gate's
    failing closed. A start runs with the optional groups off; v0.2's conversation needs its groups too. By Lodestar.
    Begun on 2026-11-23: the twelfth round built (engine 4625fc3, package 0.1.0-lodestar.15; WHY§463): `game_state`
    by a state search over four looks (title, world, title, world), with the owner's agreement to its reading (O105's
    third note). After Engine's read the same day (engine 297f95f; WHY§466): the bits field type, and
    `world_clock.hour`, `world_clock.minute` and `save_data.play_time` in the layout as leads; the round tells each
    look's time and whether the tester overran it (then it names no slot by itself), its waits a minute each.
    Package 0.1.0-lodestar.16, for the owner's run (15 never went out).
    The twelfth round reviewed the same day (WHY§469, §470): `game_state` has one candidate (static 64543992, 1,136
    bytes in; LIKELY, by the owner's word for the slot), not in the table. Next: the thirteenth round confirms it with a
    second character and begins the pointers and lists, after the owner's word on their reading.
    The thirteenth round built the same day (engine merge of Lodestar's 63122b6; WHY§488, §489): `game_state` by two
    saves, and the character search (main_player, the near list, the player set, the player's parts, player_data's
    pointer), with the owner's agreement to its reading (O105's fourth note). Package 0.1.0-lodestar.17, awaiting the
    owner's run.
    The thirteenth round reviewed the same day (WHY§502 to §505): the characters manager's main_player, near list and
    player set, the player's parts holder and the sheet found as leads (LIKELY; not in gal/). The manager's places lie
    past the layout's 0x4000: they are far fields, each read by its own small copy (Engine's ruling, WHY§506), the
    layout's and the backend's sides by Lodestar. game_state: the twelfth round's candidate is ruled out (the owner's
    save file gives the slots; it read 4 for slot 0's character; WHY§507). Next: the fourteenth round looks for the
    field that reads the loaded character's slot in two worlds, then the parts and the moving numbers by invariant.
    The fourteenth round built the same day (engine 6d10b11, merged at 9c33d80; package 0.1.0-lodestar.18): the save's
    slot by the two numbers it must read (the pair search), the character search's look in the world, and the parts
    watch (position by walking, health by a hit and a heal), with the owner's agreement (WHY§531, §532). Awaiting the
    owner's run, after three README changes of Engine's. Next for Lodestar: the far fields (WHY§506), both sides.
    The far fields built the same day (engine 5024ee9, merged at a64486f; WHY§534): the characters manager's places
    are in the layout as leads. The fourteenth round is with the owner (package 0.1.0-lodestar.18).
    The fourteenth round's run was spoilt the same day (WHY§536): GraceUI was loaded in a read round, so the marks were
    the owner's Pause presses, and the game ended with a crash code at the quit. To be run again once the read round no
    longer connects to GraceUI (WHY§537) and the parts watch takes the main player from characters.main_player
    (WHY§538).
    The fourteenth round's rerun built the same day (engine 57f7136, merged at 46f13b9; package 0.1.0-lodestar.19): a
    read round never connects to GraceUI, and the main player by the layout's word. Awaiting the owner's run.
    game_state: leads from the owner's executable the same day (O111; WHY§543): its static 0x3D709A8, the save's slot
    at 0xAC0; the fifteenth round's pair search is to show them in game.
    The fifteenth round reviewed the same day (WHY§551 to §554): the save's slot confirmed in game where the
    executable's study put it (game_state's static, field 0xAC0); health found by the parts watch (part 0 of the holder,
    0x138, maximum 0x13C); position candidates. game_state enters the table in the sixteenth round, by the game's own
    type information. Open: the game's exit code at the quit (0xC0000374 without GraceUI in the process); a control
    run with nothing injected and two short runs asked of the owner to tell the PC from the code.
    The sixteenth round built the same day (engine 920e745, merged at eb1d1c4; package 0.1.0-lodestar.20; WHY§563,
    §564): game_state by the class the leads give, on the pair search's hits; the layout's list judged first; the
    falls and moves ranked. The quit's code is the game's on that PC (WHY§556, §557, §561); it came at every quit
    for an afternoon and evening, then stopped, cause unknown (WHY§562). Awaiting the owner's run.
    The sixteenth round's package (0.1.0-lodestar.20, engine 6ee4d5d, merged at c6472f0) is with the owner after
    Engine's read of its README (sent again with the README corrected, the programs the same). Awaiting the
    owner's run.
    The sixteenth round reviewed on 2026-11-24 (WHY§567 to §569): game_state named by the class the leads give
    and entered in the build's table, with save_slot (engine 8441e8d, merged at b1b2e2b); the near list
    confirmed at the layout's place; health and position as leads in the layout. Next: the seventeenth round
    reads them through EldenNet's own reader (a backend entry beside flags_alone, Lodestar's).
    The seventeenth round built the same day (engine 1a262a7, merged at 5c07157; package 0.1.0-lodestar.21; WHY§571,
    O116): EldenNet's own reader on the world's leads (ErBackend::world_alone): the slot, the main player,
    health, position, the near list, once a second through the layout; the report holds the slot, the health
    and its maximum by the owner's word, never the position. Reviewed by Engine; the package is with the owner.
    Awaiting the owner's run.
    Before a ready start: snapshot() must walk a near list longer than 48 in part, not fault (WHY§572).
    Done the same day (engine 982f09a; WHY§573): the first 48 walked, the whole length kept, only bounds that make
    no sense a fault.
    The seventeenth round reviewed the same day (WHY§574 to §576): EldenNet's own reader read the slot, the main
    player, the health, the position and the near list through the build's layout in the game, with no fault.
    Open: the owner's check of the maximum HP. Next: the rest of the world's and the player's fields, in
    WHY§460's order (the character's other fields, the save data's deaths and the sheet's level and name, then
    field_area, the menus' screen and the session).
    The eighteenth round built the same day (engine b365148, merged at 067b725; package 0.1.0-lodestar.22; WHY§577
    to §579, O117, O118): the character survey under plan 1, for a character's other fields. Reviewed by
    Engine; the package (README at 02b0f27) is with the owner. Awaiting the owner's run.
    The eighteenth round's first run (2026-11-24; WHY§597, §598): the dead bit found (0x1C6, bits 0 and 1), and
    on-screen candidates (first, bit 4 of 0x1C4); the still fields lost to a fault of the survey's at the quit,
    mended (engine bf36cbe, merged at 88aaa16; then de126c7, merged at b511cef: only a pass that is full enough
    replaces the summary). Package 0.1.0-lodestar.23 (de126c7) repeats the round; its README read by Engine.
    The eighteenth round reviewed after its rerun (2026-11-24; WHY§605): a character's handle, kind, model, entity,
    team and the on-screen and dead bits found by the survey and entered as leads (engine e816200, merged at 6771b7e);
    the type and the block open. Next: the nineteenth round reads them through EldenNet's own reader on the near
    list's characters.
    The nineteenth round built the same day (engine 20825c7, merged at 274ab87; WHY§608): EldenNet's own reader reads
    the near characters' handle, kind, model, entity, team and the dead and on-screen bits through the layout, beside
    the survey; twenty checks for the type, the block and the dead bit's rivals. Within O117 (behaviour only), so no
    new question to the owner; README read by Engine. Package 0.1.0-lodestar.24.
    The nineteenth round reviewed (2026-11-24; WHY§610 to §614): EldenNet's own reader and the survey agree on a
    character's seven fields in the game; the type (0x68) and the block (0x38) told by the checks and entered as leads
    (engine cef6833); a layout entry is confirmed once the reader, a second reading and the field's own behaviour agree
    in one run (WHY§614). Next: the save data's deaths and the sheet's level, with the player's block at a crossing and
    the player's own death.
    WHY§614 applied the same day (engine 95a4f73, merged with cef6833 at dcceedf; WHY§615): a field may state its own
    status, and fifteen fields of rounds 17 and 19 are confirmed for 2.7.2.0-ww; every class stays a lead. The merged
    tip passed the fake game's suite under Wine, 140 of 140, GraceUI's cases included. The twentieth round (the save
    data's deaths, the sheet's level, the player's block at a crossing and its own death, with a mark per task on the
    panel at the owner's request) is being prepared, its reading and tasks to be asked of the owner outright.
    The twentieth round built the same day (engine a9d0649 and 5ffdf5a; WHY§623 to §626; O121 to O124): the sheet scan
    for the count of deaths and the level, the main player's block at a crossing and its own death, the clock and the
    play time through the reader; the launcher's panel says what each task gathered. Engine's review asked for a text
    test that holds in every script; any 16-bit unit from U+0020 is now a letter. Package 0.1.0-lodestar.26, README
    read by Engine.
    The twentieth round ran twice and lost its report to the 256 KiB limit after the first mark (WHY§630, §631); the
    read round's report may be 1 MiB now (the owner's word, O126; WHY§632), the round says when it cannot write, and the
    case runs at the game's sizes (engine 033adec, merged at 3d90fe8; package 0.1.0-lodestar.27, README read by
    Engine). Awaiting the owner's third run.
    The twentieth round's third run reviewed on 2026-11-27 (engine fe04479, merged at 2b93764; WHY§654 to §657):
    position, type and block confirmed (28 fields); the sheet's level and name placed as leads from the owner's status
    screen; the dead bit does not mark the main player's death, which is read from the health until a field is
    confirmed; the count of deaths not found. Next: the reader on the sheet, the deaths by behaviour, the player's death,
    the clock in a longer session.
    The twenty-first round built on 2026-11-27 (engine 0ce2889, merged at 27c3b17; package 0.1.0-lodestar.28, README
    read by Engine; WHY§670 to §672): a stand, the tester's death and four minutes in the world, for the clock's and the
    play time's second reading, the count of deaths, the player's death by its health, the sheet through the reader,
    and a character's focus and stamina (O136, O137, O138). The read-state and read-clock cases mended. Awaiting the
    owner's run.
### S10.2 EldenNet v0.1

- S10.2.a The launcher, the DLL load contract, and the offline and session gates
  - refs: DESIGN§3, ARCH§4, ARCH§5, O5
- b The game-access layer: discovery, layouts, guarded reads, the eleven read groups, the degradation ladder
  - refs: DESIGN§4, ARCH§6, O3, O27
- c The reader (EldenNet's own thread since O101; the game-thread task before), the world snapshot, and Augur, the event engine, with golden files
  - refs: DESIGN§5, DESIGN§6, ARCH§8, O101
  - gives: Augur v0.1 built on 2026-11-20 (S10.1.a's part 11, WHY§222 to §226), with three synthetic routes' goldens;
    the task and snapshot in parts 4 and 6. What C12 (S10.3.a) still needs: three recorded routes of the owner's game,
    redacted so they may be committed, each made a route with `golden update <recording>`.
    Since 2026-11-22 (O101; Lodestar's L4, engine 3f65014) the task closure is stepped on EldenNet's own reader thread
    (T2), about 30 times a second with a snapshot about 10 times a second, and a v1 start registers nothing in the game.
- d The banner, the debug view, recording and replay
  - refs: DESIGN§14
  - gives: the banner as a GraceUI notice and the F11 debug view built on 2026-11-20 (S10.1.a's part 12); recording and
    replay built in parts 6 and 9. Still to join (found by S10.1.i's fold): the F11 view shows en-core's changes, not
    Augur's events with their audiences, which DESIGN §14's F11 list asks for. Joined on 2026-11-20 (engine 2f2f843):
    Augur's last ten events with their audiences, then en-core's last five changes, checked on the fake game with
    GraceUI's test build (WHY§272).

### S10.3 v0.1's exit checks

- needs: S10.1, S10.2, S10.4
- S10.3.a Golden events over three routes; no crash in 10 hours and 20 forced fail-closed launches; the unknown-build banner; the task's cost; dormancy in multiplayer
  - refs: DESIGN§16.3, DESIGN§1.3
- b The read-group self-tests, the matrix's v0.1 rows (with S0's third-party rows not yet run), and the patch drill if the JP build's layout is ready
  - refs: ARCH§12.4, O91
- c Every CI stage but the eval (from v0.2) gating, and signed builds for testers
- d One hook library in the process, GraceUI's; EldenNet's imports and symbols free of any; two consumers each way round; a second GraceUI copy idling [done]
  - refs: G15, GraceUI:DESIGN§10, WHY§581, WHY§583, WHY§590
  - gives: on 2026-11-24, with Grace (the owner's request): dll_checks.py (no hook library's symbol, imports against a
    reviewed list); the fake-game cases -second-copy and -second-copy-first (a second GraceUI copy idles, installs no
    hook and is named once; EldenNet and another mod stay on the active copy; engine 08fef7a, merged at 4c5632d) and
    -unbound (two consumers each way round, with the package cases A1 to A4 by hand); GraceUI's own cases of a mod
    beside a second copy and its hook leads (S11.2.ax). Left: the hook-lead check in EldenNet's case, with the move
    to GraceUI 0.2.36. Done the same day at GraceUI 0.2.38 (engine dc601a3, merged at 6930988; WHY§590): at every
    report the active copy's six hooks each lead into its own detour and module, and the idle copy answers none; a
    mutation of the report fails both cases. With GraceUI's own copy idle, its notice now names the copy in use.

### S10.4 GraceUI 0.1 Host

- refs: G15, GraceUI:DESIGN§2.9, GraceUI:DESIGN§10
- S10.4.a graceui.dll as the host: Dear ImGui 1.93.0's DX12 and Win32 backends with MinHook 1.3.4 (O25 as changed 2026-11-16), Present, the window procedure with the WM_CHAR surrogate pairing, DInput and XInput, capture and masking [done]
  - refs: O25, GraceUI:DESIGN§2.2
  - gives: the render half built on 2026-11-16 (GraceUI's branch feat/host): the build tree with both libraries
    pinned, the start, the five hooks on the swap chain and the queue, and the render path, passing the harness's
    checks under Wine on software Vulkan (WHY§113 to §117). The input half followed the same day: the window
    procedure filtered by a subclass, with surrogate pairs and the IME; DirectInput and XInput masked while a
    holder has the controls and until a closing press is released; the holder and the player's exit as pure rules
    (WHY§126 to §133). In game, S4 checks what Wine could not show (WHY§133).
- b The anti-cheat check and the notice-only mode, the single-instance guard, and ABI 1 as a preview [done]
  - refs: GraceUI:DESIGN§13
  - gives: built on 2026-11-16 (GraceUI's feat/host): `graceui.h` with ABI 1's first table (connect, notices;
    the other slots answer "not yet"), the consumers' helper and a first example mod, a second copy idling, and
    the anti-cheat check before anything is returned, a hit leaving notices only (WHY§134 to §139).
- c Panels, checked draw lists, textures at load, faces and glyph tables, notices, and the player's exit [done]
  - needs: a
  - gives: built on 2026-11-16 (GraceUI's feat/host): panels on five layers, draw lists checked and handed over
    through a triple buffer, textures and faces registered before a mod's first draw list and baked into one
    atlas, notices in the Grace look, input for a panel through the holder, and world-state providers; a second
    example mod draws through them in the harness (WHY§140 to §147). The fonts' folder ships from 0.6, as §10 has
    it; until then notices fall back to Dear ImGui's own font.
- d EldenNet's banner as a notice and its F11 view on a headless Dear ImGui, submitted as a panel; our example consumer in the harness [done]
  - needs: c
  - gives: from 2026-11-17 GraceUI's side is the Grace session's (handover-2026-11-17): a helper that turns Dear
    ImGui's draw data into GraceUI's draw lists, an example of the banner and the F11 view, and two consumers at once,
    each way round. F11 comes through GraceUI's hotkeys, brought forward from 0.2 into 0.1. EldenNet's side is the
    spike plan's part 12, built after them. "In CI" became the harness run in the session: neither repository has
    workflows, by the owner's choice. Done on 2026-11-20 (GraceUI feat/host f51bcc7, D155, D156): hotkeys and a first
    `poll`, the header-only helper `graceui_imgui.hpp`, the example `hello-debug-view` beside `hello-notice`, each
    loaded before and after GraceUI, 106 harness checks passing on both toolchains; what EldenNet must do is GraceUI's
    `docs/eldennet-changes.md` §17 and §18. EldenNet's part 12 started the same day. From part 12's run, GraceUI's D164
    and D165 (feat/foundations 3f1352b, 2026-11-20): GRACEUI_UNBOUND for a hotkey that did not bind, graceui-sys's
    installed flag and STOP_IF_ABSENT, and GraceUI's build writing graceui.toml; EldenNet moves its pin there
    (eldennet-changes §20).

## S11 v0.2 First words [planned]

- needs: S10
- refs: DESIGN§16.2, GraceUI:DESIGN§10

### S11.1 The spikes for v0.2

- refs: DESIGN§16.1
- S11.1.a Spike S4, input, and S0-E's input half, in game [active]
  - refs: GraceUI:D175, O94, WHY§438, WHY§549, WHY§566, O101, O106, O113
  - gives: GraceUI's side built on 2026-11-20 with S10.1.g's (run B: 60 s of typing in the observer's panel, a held
    key across the menu's open, a pad's close). Until R1 gets our task running, the movement row is the owner's word
    and GraceUI's counters (messages passed while held, DirectInput and XInput states masked); EldenNet's recording
    measures it once the task runs. The talk key is one key, T by default (O94, 2026-11-21; no published default
    layout binds it, WHY§277), so S4 also presses T in game in the default layout and checks the game does nothing.
    Its second round's pad checks (the chord's 150 ms, 0.4 s and tap, that D-pad Up then View is free in the game, the
    late View opening the map, the Steam Deck) run on testers' machines: the owner tests with keyboard and mouse only
    (2026-11-22; GraceUI's D205, WHY§350). Since the first conversation (2026-11-22, WHY§438), the skip hotkey, Tab by
    default, is masked from the game whenever it is bound, since GraceUI's hotkeys have no pause: S4 also checks that
    the game does not need Tab, and if it does, the skip takes another default.
    Since O101 and O106 (2026-11-21 and 22), EldenNet's own reader measures the movement row once a ready start
    (S10.1.m) lets it run; until then the row rests on the owner's word and GraceUI's counters.
    In spikes.5 and spikes.6 (2026-11-23 and 24) S0-E passed in all five runs, and the typing row held 60 s twice with
    nothing passed to the game (WHY§549, WHY§566). Left: T and Tab, movement, the Steam keyboard, Proton, and the pad
    rows on testers' machines.
    Since O113 (2026-11-23) the box never takes the mouse, so the camera still turns with it while the player types:
    S4's camera drift row now means drift while the mouse is still.
    Status on 2026-11-27: active, not waiting. Half of it has passed in the owner's runs, but no package's steps hold
    the T press in the default layout (O94) or the Tab check (WHY§438): the next spikes package adds both, and the
    part waits on the owner again once it has them.
    Changed the same day: a check through EldenNet cannot show either, as EldenNet binds T and Tab on its main path
    and GraceUI masks a bound key from the game. Guides to the default keyboard layout disagree about Tab. So the
    owner is asked to press both in the game without mods, in its default layout, on foot and in its menus.
- b Spike S3, the talk observer, and the pilot persona's S6, in game [planned]
  - needs: S10.1.m, S10.1.c
  - gives: Status on 2026-11-27: planned, not waiting. There is no package for the owner to run: the build's layout
    has no fields for the HUD, the talk pop-up or the talk camera, which are leads alone, and S6's core flags are
    S10.1.c's, which waits on S10.1.b.
- c X1, voice in a bottle: candidate models against the prompt, offline, with its test questions [active]
  - refs: NPC§12.2, ARCH§12.3, O139, O140, O141, O142
  - gives: Status on 2026-11-27: next. It runs offline and needs nothing but a model to call; it chooses the
    models.toml the game lacks (S11.4.a's second slice). Built: the two personas X1 names, the pilot and the test
    persona, and the eval with mock and recorded models (S11.3.e). Left: the eval's live models, which it refuses so
    far; the three X1-only register fixtures (NPC §12.2); both personas' test questions as scenarios; the
    positive-wording arm; the two 4B local models; and a cloud key, asked for through the session's credential form,
    or a run by the owner.
    Decided on 2026-11-27 (O139 to O142): the design's two candidates and alternates, with the 4B pair, all run on the
    owner's own servers through LLM Jobrunner. EldenNet's eval exports X1's turns as Jobrunner jobpacks and imports
    its results; no key here and no cloud call; a $5 cap for any cloud run; the Engine session scores the test
    questions and presents the results. Being built on 2026-11-27 (engine wip/x1).
    Built the same day (engine 656b17b to cc57a7a, merged into feat/engine; WHY§673 to §680): the three register
    fixtures, both personas' test questions (six each, asked at a first meeting, three repeats), five voice sets of 50
    turns, the positive-wording arm on the three built prompts, and the eval's Jobrunner export (three jobpacks, 536
    runs a server) and import (per server and model: C10, the word cap, the pager's cuts, openers, tics, contract
    breaks, the archaic share, first token and tokens a second). The package went to the owner the same day. Left:
    the owner's run, the Engine session's scoring, then models.toml (S11.4.a).
- d X3, latency in game [planned]
  - needs: S10.1.m, S11.1.c
  - refs: O128
  - gives: Status on 2026-11-27: planned, not waiting. It needs a conversation in the real game, so S10.1.m's ready
    start (the target rules read the HUD, teams and distances), and the models X1 picks. Its tool is ready: System ›
    Traces at the 90th percentile (engine 1ee7d14 and 4058eb0; O128).

### S11.2 GraceUI 0.2 Foundations (0.1 before G15)

- S11.2.a Tokens, the protocol, the page descriptions, the web shell, and the fake host for any space, with fixtures [done]
  - refs: G3, GraceUI:DESIGN§9.2
  - gives: split on 2026-11-20 into e to l, each built and tested on its own (the Grace session's plan).
- b For spike S4: GraceUI's menu on F5 drawn in game by GraceUI's host, with one page of GraceUI's own (GraceUI > Menu) [done]
  - needs: i, l, S10.4
  - refs: G15, G20, GraceUI:D115, GraceUI:D167, GraceUI:D169, GraceUI:D171, GraceUI:D174, GraceUI:DESIGN§2.9, GraceUI:DESIGN§3.16
  - gives: the ABI's remaining slots typed (register_pages, publish, answer, poll's protocol kinds); the menu as an
    input holder with its close routine; harness screenshots, the frame cost and the atlas size, for S4 and the
    owner's look review. The page is GraceUI > Menu (ui.scale, ui.menu.theme: GraceUI's own settings, GraceUI:D115), agreed by
    the two sessions on 2026-11-20 in place of Behaviour > Pacing, an EldenNet page since G20; S4 checks input, which
    either page serves.
    S4's page in game done on 2026-11-20 (GraceUI feat/foundations ade807c, D167): F5 opens GraceUI > Menu as the
    holder of kind menu, laid out and tessellated on GraceUI's own thread, the present thread lending the newest list
    and allocating nothing; under Wine on lavapipe the atlas is 1024×2048 (baked in 25 to 41 ms), the page 2,752
    vertices and 4 commands, the present thread's own work a median of 0.15 ms at 1920×1080 and under 1 ms at p99 at
    640×360 (WHY§229); screenshots went to the owner for the look review (d). Next: the ABI's protocol slots
    (register_pages, publish, answer, poll's protocol kinds), mapped onto grace-serve's SpacePort.
    The protocol slots and the server in graceui.dll done the same day (GraceUI 959d4ec, D169): register_pages, publish,
    answer and poll's watched, unwatched and command events, callable from any thread of a mod's, a command unanswered
    in 10 s answered internal; the server started on GraceUI's thread while a space asks for the browser view, never in
    notice-only mode. EldenNet's steps for T6 are GraceUI's docs/eldennet-changes.md §23, its status lamp §24. Next: the
    menu showing a mod's space. GraceUI's D170 the same day (c781a85), from EldenNet's part 12 second run: no Direct3D DLL
    imported, the imports reviewed, and graceui-sys's installed set on the by-name path (WHY§231, §232).
    The menu showing every mod's space done the same day (GraceUI feat/foundations 430e8d4 to 0fc1033, D171): F5's
    menu shows every space registered for the game's view, mods' first and GraceUI's own last, with GraceUI's
    categories at a mod's rail's foot and a switcher from two mods' spaces; it watches the open space's settings topic
    as a view does, and its commands reach the mod with a reply handle and the 10 s deadline, a refusal's words shown
    under the row. Harness scenarios menu, pages and menu-space. With it GraceUI 0.2.0 is complete (D174) but for d,
    the owner's look review (WHY§237 to §239).
- c The protocol, lint and contrast tests; the frame cost, the atlas size and the ABI round trip [done]
  - gives: merged into e to l and b on 2026-11-20, each part with its own tests.
- d The owner's look review of the in-game menu, from screenshots [done]
  - needs: b
  - refs: G1, GraceUI:D177, GraceUI:D178
  - gives: first answers on 2026-11-20 from screenshots: a larger default size (GraceUI's D177, ui.scale 1.25) and a
    slider's margin, built in GraceUI c4ada9f; GraceUI's G31 (the pointer) and G32 (quieting a mod's notices) put to
    the owner. It stays waiting until the owner has looked at the new screenshots. The pointer (GraceUI's G31, decided
    2026-11-20): our own Grace pointer, still, built as D178 in GraceUI 67fa60f. G32 decided the same day, as
    recommended, and built as D181 (S11.2.o).
    The look review's pictures made and put to the owner on 2026-11-21, as a private page: GraceUI's own pages, the
    demo space and EldenNet's pages as of engine 8008dd1, in game under the D3D12 harness and in the browser, with nine
    things to judge (WHY§294). Approved by the owner on 2026-11-21, as drawn (GraceUI's D201, a dated note on G1).
- e Tokens: tokens/grace.toml, its build step (CSS custom properties and a C++ header), the contrast check and the no-loop lint; 0.1's notices drawn from the header [done]
  - refs: GraceUI:DESIGN§3.15, GraceUI:D108, GraceUI:D157
  - gives: built on 2026-11-20 (GraceUI feat/foundations 2ae00d0): tokens/grace.toml, tools/tokens/build.py with the
    contrast, type-minimum and no-loop checks, web/src/tokens.css and the grace-tokens header; 0.1's notices draw from
    the header, and the 106 harness checks still pass (WHY§212).
- f The bridge protocol: one schema, generated C++ and TypeScript types with a drift check, grace-protocol's round trips, golden frames and a frame fuzzer [done]
  - refs: GraceUI:DESIGN§6, GraceUI:D108, GraceUI:D55, GraceUI:D159
  - gives: built on 2026-11-20 (GraceUI feat/foundations beef3ee): one schema,
    src/grace-protocol/protocol.toml, with the framework's envelope, kinds, errors, limits, settings description and
    commands; a generator for checked C++ types and web/src/protocol.ts; grace-protocol on nlohmann/json 3.12.0; golden
    frames, round trips and a clean fuzzer. EldenNet's own topics and commands join the schema as its pages are built
    (S11.4). A risk for S4 to measure: the bridge's budget for EldenNet's largest payload (WHY§213).
- g The server's pure core: the HTTP/1.1 head parser and writer, the WebSocket handshake and frame codec, and the Host, Origin, Fetch Metadata and cookie checks, fuzzed [done]
  - refs: GraceUI:DESIGN§7.2, GraceUI:DESIGN§7.6, G30, GraceUI:D158
  - gives: built on 2026-11-20 (GraceUI feat/foundations 630639d): grace_serve_core, pure: an incremental head
    parser, the response writer with §7.2 item 10's headers on every response, the Host, Origin, Fetch Metadata and
    cookie checks, the WebSocket handshake with our own SHA-1 and Base64, and a strict RFC 6455 frame codec; 48 unit
    cases under the sanitizers, a chunking differential, the RFC vectors and two clean fuzzers. For EldenNet's launcher
    as a later host (G29): the cookie and ticket are 32 random bytes in base64url, and the loopback origin is always
    http://127.0.0.1:PORT, localhost and [::1] redirected there (WHY§214).
- h Page descriptions: their format, grace-pages, GraceUI's own space and the demo space with fixtures, the settings descriptions and the page lint [done]
  - needs: f
  - refs: GraceUI:DESIGN§2.5, GraceUI:DESIGN§6.9, GraceUI:D79, GraceUI:D115, GraceUI:D160
  - gives: built on 2026-11-20 (GraceUI feat/foundations 23430fa): page data as protocol types; a space as one TOML
    file (GraceUI's own six pages and eight settings, and an invented demo space using every setting type);
    tools/pages/build.py, which checks a space against the schema and by the page lint and writes the fixtures. EldenNet
    writes its space in the same form and lints it from a pinned GraceUI (GraceUI's docs/eldennet-changes.md §19, for
    S11.4.a).
- i The server library, grace-serve: sockets, threads, the files graceui.toml lists, the ticket and per-space routing, as one library; the security suite with two spaces (invariants 4 to 6) [done]
  - needs: f, g
  - refs: G4, G17, G29, GraceUI:DESIGN§7.2
  - gives: built on 2026-11-20 (GraceUI feat/foundations ce4d0d4, 5631bf0): sockets on two threads, the listed web
    files, the cookie and ticket, and per-space routing (Server::add_space, SpacePort); the security suite, 35 cases
    over real loopback sockets with two spaces, passing under the sanitizers and under Wine; graceui.toml a shared
    golden the server's reader parses. How EldenNet's launcher links it (G29): GraceUI's docs/eldennet-changes.md §22.
    Since D172 (protocol 1.1) an empty batch past a gap clears a stream in every view, and since D173 a destructive
    command without its confirmation is refused (WHY§238, §239).
- j The fake host for any space, grace-dev, on fixtures and scripted time [done]
  - needs: h, i
  - refs: GraceUI:DESIGN§9.2
  - gives: built on 2026-11-20 (GraceUI feat/foundations 0ca834c, D168): grace-dev serves spaces on the real
    grace-serve (the built-in ones and an author's folder, each checked as register_pages checks), with a settings
    store, fixture topics, every command answered once, and scripted time; `--dump` prints a page's in-game structure
    without Windows. EldenNet previews its own pages with it (GraceUI's docs/eldennet-changes.md §26). GraceUI's own
    pages have invented sample states for it (fixtures/graceui/topics.json).
- k The web shell: Preact and TypeScript, the rail, top bar, search and page parts; budgets, the bundle and CSP scan, a reproducible build [done]
  - needs: e, f, h
  - refs: G3, GraceUI:DESIGN§4.2, GraceUI:DESIGN§7.5
  - gives: built on 2026-11-20 (GraceUI feat/foundations 356572e, D166): the browser view, which a real Chromium under
    Wine loaded from graceui.dll's own server and drew the demo space with, no request outside its origin. Since
    D172 and D173: a refusal's words (result.detail), cleared streams, confirmations for destructive commands, and
    pages joining in their increment (GraceUI 0.2 shows Menu, Browser view and About).
- l The in-game views, grace-view and grace-draw: layout, focus and search, tessellated with the tokens; golden vertex lists and one parity golden against the web shell [done]
  - needs: e, h
  - refs: GraceUI:DESIGN§3.16, GraceUI:DESIGN§9.3, GraceUI:D162
  - gives: built on 2026-11-20 (GraceUI feat/foundations 7c69f93): grace-view lays out the frame and one page with
    the tokens into a display list, with keyboard and pad navigation and settings search, each change a ready
    settings.set or settings.reset; grace-draw tessellates it into ABI 1's draw lists, so the menu passes the same
    checker as a mod's panel (the Menu page at 1920×1080: 2,720 vertices, 4 commands, no allocation on a second
    frame); 20 tests with golden structure dumps. Editing text, secret and key settings in game, and drawing
    components, wait for v0.6 (GraceUI D57); S4's page needs neither.
- m GraceUI's settings kept between sessions, in its folder's user\settings.json, only values that differ from their defaults [done]
  - refs: GraceUI:D179, GraceUI:D84
  - gives: built on 2026-11-20 in GraceUI 0.2.2 (feat/foundations 1e33a46), at the owner's go-ahead; a release folder
    carries no user\ folder (the manifest skips it). Wanted before testers get v0.2 (S11.5).
- n The display at connect and in an event of its own, and faces baked at the content scale (ABI 1.2) [done]
  - refs: GraceUI:D180, O93
  - gives: built on 2026-11-20 in GraceUI 0.2.2 (1e33a46, proposal 0004): the connect's answer carries ui.scale, its
    default and the display's size; GRACEUI_EVENT_DISPLAY (10) comes after warm-up and after each change, with the
    content scale ((H / 1080) × ui.scale / its default, O93's rule with no constant) and the scale the faces were baked
    at (WHY§252 to §254).
- o Quiet notices: a switch per mod on GraceUI's Installed mods page, which joins in 0.2 (GraceUI 0.2.3) [done]
  - refs: GraceUI:D181, GraceUI:D173
  - gives: decided by the owner on 2026-11-20 as GraceUI's G32 (a), and built the same day (GraceUI 32ab1ed): a quiet
    mod's notice is still answered ok and kept on the page, not drawn over the game, and the switch is kept between
    sessions. The harness's "quiet" scenario on both toolchains, the view and web tests, and the quiet store's unit
    tests (WHY§259 to §262).
- p The in-game menu by mouse: clicks, a dragged slider and the wheel (GraceUI 0.2.4) [done]
  - refs: GraceUI:D182
  - gives: built on 2026-11-20 (GraceUI feat/foundations e93151e), from GraceUI's own roadmap, no decision needed;
    EldenNet's pages in the menu take the mouse with nothing to do.
- q The security review's fixes, before testers get v0.2 (GraceUI 0.2.5) [done]
  - refs: GraceUI:D183
  - gives: an independent review on 2026-11-20 (one read-only reviewer, given the code and the rules, probing a live
    grace-dev) found the wire layer sound and six faults higher up: the anti-cheat world-state flag only logged and the
    60 s module re-check missing (now either closes the browser view's server for the session); a destructive action
    without its confirmation (register_pages now refuses one); the reserved space id and topic name; a third WebSocket
    superseding a view before its ticket was shown; settings publications not held to the registration (secrets masked
    to "set" or ""). Open: telling a space which view sent a command (GraceUI DESIGN §7.3), an ABI 1 addition before
    the home network's pairing (WHY§264, §265) (the view: done in GraceUI 0.2.25, D224; the pairing: S11.2.al). Built on 2026-11-20 (GraceUI feat/foundations 8d45b20): 185 harness
    checks on both toolchains, the anti-cheat flag and re-check scenarios among them; no change to the ABI.
- r A mod space's state components drawn in game: key-value and check lists, stat tiles, a computed sentence (GraceUI 0.2.6) [done]
  - refs: GraceUI:D184, GraceUI:D171
  - gives: built on 2026-11-20 at EldenNet's request (GraceUI feat/foundations 51091c7), no change of the ABI or the
    page data: the menu subscribes to the open page's topics as a browser does, and the registry keeps a topic's
    newest state while any view watches it (WHY§271). The other five requests of EldenNet's first pages are GraceUI's
    proposal 0006 (G33), put to the owner and EldenNet the same day.
- s Five additions to mods' pages: a block's field, an empty-state text, a newest-first log tail, a space's greyed pages, shortcut rows as controls (GraceUI 0.2.7) [done]
  - refs: GraceUI:D185, GraceUI:D186, GraceUI:D187, GraceUI:D188, GraceUI:D189
  - gives: proposal 0006 (G33), decided by the owner on 2026-11-20 ("Agreed to proposal 0006") with EldenNet's
    per-page greying, and built the same day (GraceUI feat/foundations beca859), no change of the ABI. EldenNet's
    pages take them as each slice is built; Home and System are reworked to use them in a later slice (WHY§273).
- t The browser view's rail and tables match the game's (page-less categories hidden, column gaps, a one-line top bar), and a page-shots harness scenario for look reviews (GraceUI D191) [done]
  - refs: GraceUI:D191, WHY§278, WHY§279
  - gives: found and done on 2026-11-21 (GraceUI feat/foundations c397464, no change of the ABI) while making the
    owner's "Decisions in pictures" page from EldenNet's pages as built (engine 80ef2fe), in both views.
- u Row actions, pickers from a topic, row secrets and small forms for mods' pages: proposal 0007 (G34) built (GraceUI 0.2.8) [done]
  - refs: G34, GraceUI:D192, GraceUI:D193, GraceUI:D194, GraceUI:D195, GraceUI:D196, O95, WHY§281
  - gives: decided by the owner on 2026-11-21 (O95's words) and built the same day (GraceUI feat/foundations 0738fa3),
    protocol 1.2, ABI 1.2 unchanged. Row actions and the row secret are declared on a table's block in the page data,
    not as columns in a topic's body (WHY§281); how EldenNet uses them is GraceUI's docs/eldennet-changes.md §41.
    In game, focus now stays on its element when a topic's body arrives. EldenNet's AI pages take them in the next
    slice (S11.4.a).
- v A row secret's refused and none states, the changed mark from the space's own list, no empty default printed (GraceUI 0.2.9) [done]
  - refs: GraceUI:D197, GraceUI:D198, WHY§288
  - gives: asked by EldenNet's third pages slice and done on 2026-11-21 (GraceUI feat/foundations a1d8f78), protocol
    and ABI 1.2 unchanged. EldenNet's next pages slice uses them: "none" for a server that needs no key, "refused" for a
    key the provider refused, a picker in the `changed` list only when the player picked a model, and "Default: the
    preset's model." in its help (GraceUI's docs/eldennet-changes.md §42).
- w A picker's empty default reads Default, in both views (GraceUI 0.2.10) [done]
  - needs: d
  - refs: WHY§294
  - gives: found by the owner's look review (S11.2.d), whose pictures of EldenNet's per-job model pickers showed them
    blank; GraceUI's D199 draws an empty default as "Default" and offers it first. Done on 2026-11-21 (GraceUI
    feat/foundations 913a0e2), protocol and ABI unchanged; nothing for EldenNet to change (GraceUI's
    docs/eldennet-changes.md §43).
- x Sections as cards: a chip, a live status and a section's own actions, for AI › Models' card per job (GraceUI's
    proposal 0008; GraceUI 0.2.12) [done]
  - needs: w
  - refs: PAGES§4.6.2, G36, GraceUI:D202, WHY§298
  - gives: put to the owner on 2026-11-21 as GraceUI's G36 (Open): three optional fields on a section, a static chip
    (the slot id), a status from a state topic (a dot and a word in the lamp's tones) and one or two actions of a
    header action's shape ("Prepare lines now"); protocol 1.3, ABI unchanged. Recommended: all three. EldenNet's
    AI › Models then moves its jobs from the settings block under the Jobs table into one section per job.
    Decided by the owner on 2026-11-21, (a) all three, and built the same day as GraceUI's D202 (0.2.12, protocol 1.3,
    ABI 1.2 unchanged; feat/foundations a9aac78): `chip`, `status` {topic, field} and `actions` on a section, page data
    using them at `"minor": 3` (GraceUI's docs/eldennet-changes.md §45). EldenNet's next pages slice uses them.
- y A row secret's line in game only when a row takes a key (GraceUI 0.2.11) [done]
  - needs: v
  - refs: GraceUI:D200, WHY§297
  - gives: asked by EldenNet's fourth pages slice, where every row of a local server read Not needed and the line
    "Key: set in the browser view." still showed. Done on 2026-11-21 (GraceUI feat/foundations 75cc11a), all checks
    passed; nothing for EldenNet to change (GraceUI's docs/eldennet-changes.md §44).
- z The browser view paces every frame it sends, subscriptions included (GraceUI 0.2.13) [done]
  - needs: x
  - refs: GraceUI:D203, WHY§300, WHY§301
  - gives: asked by EldenNet's fifth pages slice, whose AI › Models reads 14 topics and could lose one right after
    connecting. The server's limit stays (every frame counted, 20 a second, the rest dropped unread); the web transport
    now sends subscriptions, unsubscriptions and commands through one queue, in order, at 16 a second, and an
    unsubscription of a subscription still waiting takes it back. Done on 2026-11-21 (GraceUI feat/foundations
    162148f), web only; nothing for EldenNet to change (GraceUI's docs/eldennet-changes.md §46).
- aa Pad chords for mods' hotkeys (GraceUI's proposal 0005; GraceUI 0.2.14, ABI 1.3) [done]
  - refs: G35, O95, O11, NPC§5.6, GraceUI:D204, GraceUI:D205, WHY§349, WHY§350
  - gives: decided by the owner on 2026-11-21 (O95; GraceUI's G35) and built on 2026-11-22 (GraceUI feat/foundations
    a1b09a9), ahead of S4's second round. Every pad behaviour is checked without a pad, by unit timelines and the
    harness under Wine (GraceUI's D205); what only a real pad shows waits for testers (S11.1.a). EldenNet's side is the
    opt-in only (S11.4.c; GraceUI's docs/eldennet-changes.md §47).
- ab The Hotkeys page lists every hotkey, live (GraceUI 0.2.15) [done]
  - refs: GraceUI:D115, GraceUI:D204, GraceUI:D206, WHY§361
  - gives: shipped in 0.2 instead of 0.6, at the owner's word, on 2026-11-22 (GraceUI feat/foundations f6fb731): the
    menu's key, then each mod's hotkeys with their names, keys, pad chords and states. The menu's key is shown locked,
    "Later version", until rebinding (S11.2.ac). EldenNet gives each hotkey a player's name (GraceUI's
    docs/eldennet-changes.md §48).
- ac Rebinding hotkeys on GraceUI's Hotkeys page, for every mod (GraceUI's proposal 0009) [done]
  - needs: ab
  - refs: G37, O94, O77, GraceUI:D115, GraceUI:D205, GraceUI:D208, GraceUI:D209, GraceUI:D210, GraceUI:D211, WHY§362, WHY§366
  - gives: put to the owner on 2026-11-22 and decided the same day, as Grace and Engine recommended: every binding on
    GraceUI › Hotkeys, a taken key asked about and then moved, pad chords by two pickers, ABI 1.4's describe_hotkey,
    and built now, in 0.2.x. EldenNet's side is S11.4.c. Built the same day as GraceUI 0.2.16 (protocol 1.4, ABI 1.4;
    feat/foundations bc211cc): Change key, Change pad chord, Reset, a taken key confirmed then moved, hotkeys.menu live,
    describe_hotkey and a changed event; the bindings in game only (O77's note; GraceUI's docs/eldennet-changes.md §49).
- ad A wait handle for each mod's events (GraceUI's D71, ABI 1.5) [done]
  - refs: NPC§5.1, WHY§364, GraceUI:D212, GraceUI:D71, WHY§373
  - gives: asked by EldenNet's first conversation on 2026-11-22, for T6's opening within 50 ms: a mod gives GraceUI a
    Win32 event, set whenever something is queued for it (a hotkey's press or change, a holder's event, a protocol
    event, the display event), never a callback. T6 waits on it, with its 100 ms as the fallback. Built the same day as
    GraceUI 0.2.17 (feat/foundations 77ffd2e; GraceUI's docs/eldennet-changes.md §50).
- ae Text and secret settings, forms, row secrets and search typed in game (GraceUI 0.2.18) [done]
  - refs: G38, GraceUI:D214, GraceUI:D57, WHY§378, WHY§380
  - gives: built on 2026-11-22 (GraceUI feat/foundations a170951) as S7.2.c's part a. EldenNet's fields that read "set
    in the browser view" in game can be typed in game too from 0.2.18 (PAGES §4.6.2's note; GraceUI's
    docs/eldennet-changes.md §52).
- af Topic families and a page's master list in both views (GraceUI 0.2.19, protocol 1.5) [done]
  - refs: G38, GraceUI:D215, GraceUI:D36, WHY§383, WHY§384
  - gives: built on 2026-11-22 (GraceUI feat/foundations fbd4ec3) as S7.2.c's part b; for Named NPCs, NPC types,
    Playthrough, History and the editor's lists (GraceUI's docs/eldennet-changes.md §53).
- ag Slow commands, answered within 90 s (GraceUI 0.2.20, protocol 1.6) [done]
  - refs: G38, GraceUI:D216, GraceUI:D169, WHY§386
  - gives: built on 2026-11-22 (GraceUI feat/foundations 4749729) as S7.2.c's part c; for `profile.draft` (GraceUI's
    docs/eldennet-changes.md §54).
- ah The editor block in both views (GraceUI 0.2.21, protocol 1.7) [done]
  - refs: G38, GraceUI:D217, WHY§391, WHY§392, WHY§377
  - gives: built on 2026-11-22 (GraceUI feat/foundations f35f495) as S7.2.c's part d; for the profile editor's Profile
    tab (S7.2.b; GraceUI's docs/eldennet-changes.md §55).
- ai A save's chosen row in both views (GraceUI 0.2.22) [done]
  - refs: GraceUI:D218, GraceUI:D215, WHY§393, WHY§387
  - gives: built on 2026-11-22 (GraceUI feat/foundations 62f492e) at EldenNet's request; for the profile editor
    (S7.2.b; GraceUI's docs/eldennet-changes.md §56).
- aj Search across spaces in game (GraceUI 0.2.23) [done]
  - refs: GraceUI:D219, GraceUI:D166, GraceUI:D214, WHY§465
  - gives: built on 2026-11-23 (GraceUI feat/foundations 8776e80 and b52cd7d), brought forward from GraceUI 0.4 at the
    owner's go-ahead: the menu's search in game finds every space's settings, each named by its space and opened on
    its home page with its row revealed, as the browser view's already did. No change of ABI or protocol, and nothing
    for EldenNet to change (GraceUI's docs/eldennet-changes.md §58).
- ak The init's step readable from outside, for a stalled start (GraceUI 0.2.24) [done]
  - refs: GraceUI:D220, GraceUI:D175, WHY§467, WHY§255
  - gives: built on 2026-11-23 (GraceUI feat/foundations 79c1c24): the init thread sets a step before each part,
    read through GraceUI_Test_InitStep in the test build and written by the spike observer as init_step;
    report.py names it when the host never hooked, and its movement row now says S10.1.m. No change of ABI or
    protocol (GraceUI's docs/eldennet-changes.md §59).
- al Other devices at home: the switch, Pair a device in game, Forget, and what a paired device may not do (GraceUI 0.2.25, protocol 1.8) [done]
  - refs: G9, GraceUI:D221, GraceUI:D222, GraceUI:D223, GraceUI:D224, GraceUI:D225, GraceUI:D66, GraceUI:D113, WHY§490, WHY§491, WHY§492
  - gives: built on 2026-11-23 (GraceUI feat/foundations b1eb5bc to bc8fb8f) at the owner's go-ahead. A server suite of
    9 cases on 127.0.0.2 and a pairing harness scenario through the real menu under Wine (13 checks; 245 in the full
    harness, the MinGW build whole and the clang-cl build all but the menu's frame-cost p99, which passed alone); an
    independent review found no way past the pairing page and five gaps, fixed before release (D225). The command's
    view (§7.3, D224) closes S11.2.q's open item. EldenNet marks its sensitive commands (S11.4.e; GraceUI's
    docs/eldennet-changes.md §60).
- am Uploads: a space's file commands, the drop zone, POST /upload with a one-use ticket, a mod's file through ABI 1.6 (GraceUI 0.2.26, protocol 1.9, ABI 1.6) [done]
  - refs: G28, GraceUI:D226, GraceUI:D227, GraceUI:D228, GraceUI:D229, GraceUI:D114, GraceUI:D103, WHY§512, WHY§513, WHY§514
  - gives: built on 2026-11-23 (GraceUI feat/foundations b1f98bd to 2754912) at the owner's go-ahead. A server suite of
    five upload cases (the ticket, its size and hash, one at a time, one file queued a space at a time, this PC alone)
    on Linux and under Wine; a browser test through the page's own drop zone; the pages harness scenario's upload
    through the real ABI under Wine (246 checks in the full harness, each build whole); an independent review found no
    way past the ticket and five gaps, fixed before release (D229). EldenNet declares plugins.install as a file command
    and reads its file through ABI 1.6 (S17.1.e; GraceUI's docs/eldennet-changes.md §61).
- an The first frames' step and GraceUI's log beside the spike report, for A1's crashes (GraceUI 0.2.27, test build) [done]
  - refs: GraceUI:D230, GraceUI:D220, GraceUI:D175, WHY§509, WHY§516
  - gives: built on 2026-11-23 (GraceUI feat/foundations 7022e43). Each state line carries frame_step (1 to 11, GraceUI's
    first frames from a Present through its detour to the game's Present returning after its first frame), with a line
    at each change; report.py names the step when nothing was drawn; graceui-log.txt beside the report holds GraceUI's
    own log, never in the report. Both spike scenarios check it (248 checks in the full harness). The next package's
    runs place A1's fall within GraceUI's first frames (GraceUI's docs/eldennet-changes.md §62).
- ao The spike observer's fault recorder (GraceUI 0.2.28, test-only) [done]
  - refs: GraceUI:D231, GraceUI:D230, WHY§518
  - gives: built on 2026-11-23 (GraceUI feat/foundations 6edaca4, run dffd92b). A vectored handler records, never handles,
    each fault's code, access, null target, frame step and the faulting thread's frames (the return address first where
    the pointer is 0), as fixed words in the report and module names in graceui-faults.txt; 250 checks in the full
    harness.
- ap The menu's pointer by the mouse's own motion (GraceUI 0.2.29), for the owner's rubber-band mouse in A5 [done]
  - refs: GraceUI:D232, GraceUI:D130, GraceUI:D176, WHY§519
  - gives: built on 2026-11-23 (GraceUI feat/foundations 6a25179). Raw input's or DirectInput's deltas move GraceUI's
    pointer while a holder has the mouse; a harness game that recentres its cursor checks it. Its full harness run
    passed the same day on both builds, 252 checks (GraceUI 142eb49). Next: 0.2.30 with the owner's G40 (Alt+Tab: the
    menu stays open) and G41 (Tab no longer moves between elements in game), which spikes.5 waits for.
- aq The menu across an Alt+Tab, and no Tab between elements in game (GraceUI 0.2.30) [done]
  - refs: G40, G41, GraceUI:D233, WHY§533
  - gives: built on 2026-11-23 (GraceUI feat/foundations 45998e9, run a04a234), with the observer's fault recorder
    taking 0xC0000374 too (d2f352d). The harness's menu-away scenario: Tab moves no focus; away and back through the
    real menu on the same page and row; danger while away closes it. 256 checks, both builds.
- ar A test build's free port and grace-dev's master detail (GraceUI 0.2.31) [done]
  - refs: GraceUI:D234, WHY§524, WHY§526, WHY§535
  - gives: built on 2026-11-23 (GraceUI feat/foundations 6a50f1d, 0d034ea and 386d1ba) at the owner's go-ahead:
    GRACEUI_TEST_PORT (free or a port) serves a test build's browser view off 8642 with the settings unchanged, and
    `--dump` gives a page's topics until none is new. 258 checks in GraceUI's full harness. EldenNet's side is
    S11.2.as.
- as EldenNet's GraceUI cases on a free port, and Named NPCs' goldens with a master's detail (GraceUI's docs/eldennet-changes.md §64) [done]
  - needs: ar
  - refs: GraceUI:D234, WHY§535, WHY§524, WHY§540, WHY§541, WHY§542
  - gives: the cases set GRACEUI_TEST_PORT=free and read the port from GraceUI_Test_GetServer; the editor's case stops
    writing a port into GraceUI's settings.json; the goldens regenerated with 0.2.31's grace-dev; graceui-sys's pin
    moved when the vendored files change. Done on 2026-11-23 (engine wip/s12-pin bad88ac and 0f537ed, merged at
    4bfccf9): graceui-sys at GraceUI 386d1ba (0.2.31, ABI 1.6, protocol 1.9; EldenNet uses neither's uploads; floor
    0.2.22), GRACE_ENV for the GraceUI cases, the editor's settings with no port, and only the two Named NPCs goldens
    changed, to the chosen row's detail. GCC 39 of 39 with the pages' lint and preview, eval 8 of 8; clang-cl's nine
    unit programs and 18 GraceUI and spike cases under Wine, every server on a free port.

- at GraceUI's test build guards its own heap, for spikes.6 (GraceUI 0.2.32) [done]
  - refs: GraceUI:D235, GraceUI:D231, WHY§536, WHY§539, WHY§546, WHY§547, WHY§548
  - gives: built on 2026-11-23 (GraceUI feat/foundations 093fd51, 7fe1248 and 511ec93) at Engine's request: guard bytes
    and a sealed header on every block of graceui.dll's own, a quarantine of freed blocks, checks at each free and
    every 5 s, a breach raised as 0xE0475548 and named in the spike report by its block and its maker (module and
    offset). The full harness (266 checks, its regression run with GRACEUI_TEST_HEAP_GUARD=off) and a sweep of every
    scenario with the guard on found no breach under Wine. spikes.6 ships graceui.pdb beside the test build's
    graceui.dll (GraceUI's docs/eldennet-changes.md §65), after spikes.5's results.

- au The owner's spikes.5 findings in GraceUI: the pointer kept where the mouse took it, still or moving; the observer's typing test on the keyboard and the pad only (GraceUI 0.2.33) [done]
  - refs: GraceUI:D236, GraceUI:D232, O113, WHY§549, WHY§559, WHY§560
  - gives: built on 2026-11-23 (GraceUI feat/foundations 910ad0a, run 2e219de). The menu-recentred scenario holds still
    for 1.5 s and keeps the pointer; the spike scenario's typing test checks the letters reach the panel and the mouse
    the game. For spikes.6, with 0.2.31's free test port and 0.2.32's heap guard.
- av G42: the menu's focus ring after a key or the pad, gone with the mouse (GraceUI 0.2.33) [done]
  - refs: G42, GraceUI:D237
  - gives: decided by the owner on 2026-11-23 as recommended ("After keys or pad") and built the same day (GraceUI
    feat/foundations 63d63c3); the harness's menu-focus-ring scenario reads the ring's pixels beside the focus (none at
    open, there after a key, gone after a mouse move, the focus kept). EldenNet's goldens do not change.
- aw The spike report places a dropped press: the game window's focus and place, the presses that reached the game, each input detour's longest call and the game's gaps between DirectInput reads, in each state line (GraceUI 0.2.34) [done]
  - refs: GraceUI:D238, GraceUI:D239, WHY§566, WHY§570
  - gives: built on 2026-11-24 (GraceUI feat/foundations d3a6837, 2d8cef8) at Engine's request after spikes.6's A1.
    The harness checks each count (a press with no holder by DirectInput and by message, focus lost and gained, a
    300 ms gap between reads, the detours' longest calls). The owner's A1 run again showed the first A1's drops a
    first-run effect (WHY§566); for a next one, the owner's rough time of a missed press is set against the state
    line near it. For spikes.7.
- ax S10.3.d's GraceUI side: where each hooked function leads, in GraceUI's test build, and a mod of EldenNet's kind beside a second copy each way round (GraceUI 0.2.35) [done]
  - refs: GraceUI:D240, GraceUI:D72, GraceUI:D102, WHY§581, WHY§582
  - gives: built on 2026-11-24 (GraceUI feat/foundations f368290) at Engine's request; GraceUI_Test_GetHookLead says,
    per hook, whether its entry leads to the active copy's detour and into which module. Left for S10.3.d: EldenNet's
    own case against GraceUI's test build (a second copy beside the game while EldenNet runs; every hook leading into
    the active copy).
- ay S11.4.c's italic face: EB Garamond Italic in GraceUI's fonts folder for mods' faces (GraceUI 0.2.36) [done]
  - refs: GraceUI:D241, GraceUI:D139
  - gives: built on 2026-11-24 (GraceUI feat/foundations 9f536a0); EldenNet registers it as a third scaled face for
    the Tarnished's lines. Rounded corners need no change in GraceUI (GraceUI's docs/eldennet-changes.md §69).
- az The in-game menu's pad mode: the key hints as the pad's buttons, Y for search, View for the hints, the right stick to scroll; a pad-only walkthrough in the harness (GraceUI 0.2.37) [done]
  - refs: GraceUI:D242, GraceUI:D33, GraceUI:D237
  - gives: built on 2026-11-24 (GraceUI feat/foundations a9755cb); the menu-pad scenario is the first of v1.0's
    pad-only walkthrough. Left for later: §3.8's key caps for the keyboard (they would move every golden), a 44 px
    floor for smaller rows, and Start's quick switches. Nothing for EldenNet to change (GraceUI's
    docs/eldennet-changes.md §70).
- ba What pressed a hotkey (ABI 1.7) and the notice when GraceUI's own copy idles (GraceUI 0.2.38) [done]
  - refs: GraceUI:D243, GraceUI:D244, GraceUI:D240, WHY§585
  - gives: built on 2026-11-24 (GraceUI feat/foundations 3ea17b6) at Engine's request after its second-copy and chord
    cases; EldenNet reads hotkey_by for the chat box's via and its swallow, with its pin moving to 0.2.38. Pinned the
    same day (engine 678b313, merged at 6930988; WHY§585, §590).
- bb The preview's three findings in GraceUI's menu: the links back by the space's increment, the rail's ring whole, a related row's words clear of its ring (GraceUI 0.2.39) [done]
  - refs: GraceUI:D245, GraceUI:D173, WHY§595, WHY§599
  - gives: built on 2026-11-24 (GraceUI feat/foundations c9d699e) from Engine's preview shots (S11.4.c); EldenNet
    regenerates its goldens with a Related card at 0.2.39, with its next pin. Pinned the same day with Content packs
    (engine 7773f47, merged at 86d3991): every golden moved by its Related rows alone (WHY§599); the faces by name
    (GRACEUI_FACES_ELDENNET, D139).
- bc The menu's keyboard hints as key caps (GraceUI 0.2.40) [done]
  - refs: GraceUI:D246, GraceUI:D242, WHY§606, WHY§607
  - gives: built on 2026-11-24 (GraceUI feat/foundations c53f414; GraceUI's docs/eldennet-changes.md §73): each key of
    the footer's hints in a cap, its label beside it, so every golden's footer moves. EldenNet pins 0.2.40 once
    GraceUI's full run is green, regenerates its goldens once, and reads a whole hints line from
    GraceUiTestMenu.footer instead of the screen. Pinned the same day (engine b294e06, merged at 01ed439): all 18
    goldens moved by their footer alone (WHY§606); no check of the fake game's harness read a whole hints line, so
    none changed (WHY§607).
- bd The key caps' arrows drawn (GraceUI 0.2.41) [done]
  - refs: GraceUI:D247, GraceUI:D246
  - gives: built on 2026-11-24 (GraceUI feat/foundations 94284d7, with the gap after "Shift+" and its run in 0ed0e61):
    each arrow in a footer's key cap drawn as a shaft and a head, centred, instead of the mono face's glyph.
    EldenNet's footers with arrows move at its next pin after S11.2.bc, which takes 0.2.41 and 0.2.42 (S11.2.be)
    together; GraceUiTestMenu.footer is unchanged. Pinned with 0.2.42 to 0.2.44 (engine c9832bb, merged at 0538221;
    WHY§617).
- be The rail's foot for a mod's categories, and empty words without "yet" (GraceUI 0.2.42, protocol 1.10) [done]
  - refs: GraceUI:D248, GraceUI:D249, PAGES§4.2.1, PAGES§4.6.9, O119
  - gives: asked by Engine from Content packs on 2026-11-24; Grace builds both as framework details of the design
    decided (its D248 and D249): a category may say "foot": true, and both views pin such categories to the rail's
    bottom under a rule; a block with no items and no words of its own reads "Nothing to show." in both views (no
    "yet", which SP5 and GraceUI's invariant 2 both forbid for anything not reached). EldenNet then marks Settings,
    Content packs and Docs as its foot. Built the same day (GraceUI feat/foundations 9157984, its run in b36bd2a);
    foot categories come after every other. EldenNet takes 0.2.41 and 0.2.42 in one pin after S11.2.bc. Pinned at
    0.2.44 (engine c9832bb, merged at 0538221): Settings, Docs and Content packs are EldenNet's foot.
- bf A table's own filter bar and sort, done in the view (GraceUI's proposal 0012, G43 (a); protocol 1.11) [done]
  - refs: G43, GraceUI:D250, PAGES§4.6.2
  - gives: put to the owner on 2026-11-24 and decided as recommended (G43 (a), the owner's words "(a) Filter bar now,
    Events later (Recommended)"): a table or a master list carries `filter_bar`, up to four filters from the values
    its published rows hold, up to four sort options on a column or a hidden key of each row, an optional search on
    named columns, all in the view and never sent to the space; a row with no value for the key sorts last either
    way (GraceUI feat/foundations 1d6a41c). For AI › Requests (0.2), Named NPCs and Memories
    (0.3); Events' paged filtering is designed with its pages for 0.4. Engine answered the proposal's §6 that day
    (`calls` and the roster are whole state topics); the hidden keys are last_met_s, first_met_s, at_ms,
    first_token_ms, total_ms, tokens, a memory's time and weight; "names as shown" is the search over the name column.
    Built the same day as GraceUI 0.2.43 (D250, feat/foundations 72f77a0, its run there; GraceUI's
    docs/eldennet-changes.md §76). EldenNet's next pin takes 0.2.41 to 0.2.43 together, and AI › Requests uses it.
- bg A document block, a prose form field and the report preview (GraceUI's proposal 0013, G44 (a)) [done]
  - refs: G44, GraceUI:D251, PAGES§4.6.8, PAGES§4.6.9, G16
  - gives: GraceUI's answer to Docs and Tools › Bug report (2026-11-24): a document block on a master page (headings,
    paragraphs, lists, code, tables, spans with links to other documents, never a URL); `"prose": true` on a form
    field (2000 characters); the report preview drawn at last (parts with their fields, an optional part's switch
    sending the space's own command). Engine asked the same day for a link to a heading, a note block, code blocks up
    to 16 KiB and a multi-line field in the preview, all four taken into GraceUI's proposal 0013. Put to the owner the
    same day and decided as recommended (G44 (a), "(a) All three (Recommended)"): the three together, in the protocol
    minor after the filter bar's. Until then the bug report's preview is built from a table with a switch column and
    a key-value list per part; Docs' documents are drafted in our own words for the owner's review meanwhile.
    Built the same day as GraceUI 0.2.44 (D251, protocol 1.12, feat/foundations 3fac29b; GraceUI's
    docs/eldennet-changes.md §77), in both views, the demo's Handbook, lamp report and Notes field as examples: Docs a
    master page whose list's own filter-bar search covers titles and headings only; "What happened" a form field with
    `"prose": true`; `report_preview` sending the space's command for a part's switch. EldenNet's Docs and Bug report
    drop their stand-ins with its next pin, which takes 0.2.41 to 0.2.44 together. Done the same day (engine
    0538221; S11.4.a).
- bh Quick switches and live updates, then a space's scope and paged tables (GraceUI's proposal 0014, G45 (a)) [done]
  - refs: G45, GraceUI:D252, GraceUI:D254, GraceUI:D255, PAGES§4.6.1, PAGES§4.6.4, PAGES§4.9
  - gives: decided by the owner on 2026-11-24 (G45 (a), "All four, in two steps (Recommended)"): quick switches (a
    setting's shortcut or a session switch with its state) and live updates (Live, every 5 s, Paused) first, as
    protocol 1.13, for EldenNet 0.2's Home, Requests and Pause AI; then a space's scope (the Journey) and paged tables,
    as the next minor, for 0.3's Journey, Conversations and later Events. Engine's answers the same day: a topic of its
    own, `journeys`, since its labels carry the character's name (PV); a family member per journey for Playthrough and
    Memories; 200 rows a page and a total of perceived events only (SP7); Pause AI as `ai.pause` with {"on"}, its state
    `status.ai_paused`; Named NPCs scoped, its list a family per journey, Nearby only on the current journey.
    Step one built the same day as GraceUI 0.2.45 (D252, protocol 1.13, feat/foundations 385d80e; GraceUI's
    docs/eldennet-changes.md §78): quick switches (a setting's shortcut, or a session switch reading a boolean field
    of a state topic; pad Start opens them in game, the power button in the browser) and live updates (Live, every
    5 s, Paused; Paused keeps watching and counts what came). EldenNet's Pause AI binds as `ai.pause` with
    `status.ai_paused`. Step two (the scope and paged tables, protocol 1.14) takes Engine's answers.
    GraceUI 0.2.46 the same day (D253, feat/foundations fe7d9db, its run in b83ec41): fixes from EldenNet's 0.2.44 pin,
    a document's member watched in game, so Docs shows, and a preview's fields wrapped with their line breaks kept.
    Step two the same day as GraceUI 0.2.47 (D254, protocol 1.14, feat/foundations 7e076dd; GraceUI's
    docs/eldennet-changes.md §80), in the formats Engine gave: the Journey as a space's scope on its own `journeys`
    topic, scoped pages reading a family member per journey, a read-only journey greying the page, and tables and
    master lists read a page at a time (filters, sort, search and scope sent; Show more; New rows). For EldenNet 0.3's
    History, Playthrough and Named NPCs.
    GraceUI 0.2.48 (D255, feat/foundations b9eb970): four fixes to D254's paging found by a review of 0.2.47; EldenNet's
    0.3 pin takes 0.2.48, and publishes its History pages' changes topic when a character loads.
- bi What is out of view costs nothing, pages open at their top, a row drawer and a header switch (GraceUI 0.2.49, protocol 1.15) [done]
  - refs: GraceUI:D256, GraceUI:D253, WHY§633, WHY§635, WHY§636, WHY§637
  - gives: built on 2026-11-24 (GraceUI feat/foundations b5b7204; GraceUI's docs/eldennet-changes.md §82) from
    EldenNet's 0.2.46 pin: the tessellator skips what lies wholly outside its clip, so Docs stays within a mod's draw
    limits; a page opens on its first stop with its header in view; a table's body column may be `"drawer": true`, in
    a row's drawer; a page's header action may be a switch on a state topic's boolean. EldenNet's next pin (after
    S11.4.a's System pages) takes 0.2.49: Docs' exit dropped, Requests' Why and Text shown in the drawer, Home's Pause AI
    chip beside the quick switch.
    Taken by EldenNet's pin of 2026-11-25 at 0.2.53 (engine 6b92444; WHY§646, §647, §650).
- bj Live feeds and log tails drawn in game, and a filter bar on a stream (GraceUI 0.2.50, protocol 1.16) [done]
  - refs: GraceUI:D257, PAGES§4.6.7
  - gives: built on 2026-11-24 (GraceUI feat/foundations 789a17f; GraceUI's docs/eldennet-changes.md §83): in game a
    stream shows its newest 200 rows, each with its time, wrapped (System › Logs and any feed on Home); a log tail may
    carry a filter bar (a `level` filter and a search on the line), standing in for PAGES §4.6.7's level tiles as one
    control in both views. EldenNet's next pin takes it with 0.2.49, a `level` on each log row.
    Taken by EldenNet's pin of 2026-11-25 at 0.2.53 (engine 6b92444; WHY§646, §651): EldenNet's GraceUI floor is 0.2.50,
    the first whose page check takes a filter bar on a stream.
- bk A page in the browser alone, shown with a setting, behind a gate, with a ribbon (GraceUI 0.2.51, protocol 1.17) [done]
  - refs: GraceUI:D258, PAGES§4.6.8, PAGES§4.9
  - gives: built on 2026-11-24 (GraceUI feat/foundations 337b143; GraceUI's docs/eldennet-changes.md §84) for
    EldenNet's Tools › Workshop (D10, SP8): `browser_only`, `shown_with = "debug.panel"`, a `gate` with the space's own
    words once a browser session, and a `ribbon` at the page's top while it is open. SP8's ribbon is read as each
    page's: every page that shows unfiltered developer content carries it (Workshop now, the Event monitor's plumbing at
    0.4), not every page of the session. For Workshop's build after the next pin.
    Taken by Workshop's build on 2026-11-27 (engine d1a96c2).
- bl A panel drawn in each frame by its mod's callback, on the present thread: on_present, told its time and whether the menu is open (GraceUI 0.2.52, ABI 1.8) [done]
  - refs: GraceUI:D259, GraceUI:D71, WHY§364, WHY§645, PAGES§4.6.7
  - gives: built on 2026-11-25 (GraceUI feat/foundations 832b8d9; GraceUI's docs/eldennet-changes.md §85): one of a
    mod's panels gets a callback on the present thread with room reserved for its lists; it answers KEEP or DRAW, and
    GraceUI copies and checks the list without allocating. Each call is told the frame, its delta, the panel's place,
    `spent_us` (the mod's callbacks together in the frame before) and GRACEUI_PRESENT_MENU_OPEN. Over 0.5 ms a frame for
    120 frames, or an exception, suspends a mod's callbacks for the session. For EldenNet's subtitle fades and world
    labels at the frame rate, and Performance's frame cost from `spent_us` by the flag, when taken. Nothing changes for
    EldenNet until it opts in.
- bm The in-game points from EldenNet's 0.2.46 build: a table's column labels and slots, wrapped sentences and descriptions, a tile's unit in the small face (GraceUI 0.2.53) [done]
  - refs: GraceUI:D260, WHY§642, WHY§643, PAGES§4.6.7
  - gives: built on 2026-11-25 (GraceUI main 7fa3042; GraceUI's docs/eldennet-changes.md §86). In grace-view alone, with no change to EldenNet's page data: the in-game table gets a header line and a slot
    per column (numbers right-aligned; a missing cell leaves its slot empty), or each value with its column's label when
    slots would be too narrow; a computed sentence and a section's description wrap to the card; a tile's unit drawn in
    the small face. Taken with 0.2.49 to 0.2.52 by EldenNet's pin of 2026-11-25 (engine 6b92444; WHY§652).
- bn Wrapped in game: a page's description, a statement, a key-value list's value and a focused setting's help (GraceUI 0.2.54) [done]
  - refs: GraceUI:D260, WHY§642, PAGES§4.6
  - gives: GraceUI's D261, built on 2026-11-25 (GraceUI feat/foundations 806d5dd; GraceUI's docs/eldennet-changes.md
    §87); in grace-view alone, with no change to EldenNet's page data, the protocol or the ABI. EldenNet's goldens move
    with it at the next pin.
- bo The points from EldenNet's 0.2.53 pin: a stream's rows in grace-dev's dump, words for an unpaged filter's values, no drawer for an empty row, a stream's bar with or without rows, a page's first focus in its body, slots by their widest values (GraceUI 0.2.55, protocol 1.18) [done]
  - refs: GraceUI:D260, WHY§648, WHY§649, WHY§650, WHY§651, WHY§652, PAGES§4.6.7
  - gives: GraceUI's D262, agreed and built on 2026-11-25 (GraceUI feat/foundations 3623304; GraceUI's
    docs/eldennet-changes.md §88). An unpaged filter's `choices` (protocol 1.18) raises EldenNet's floor
    to 0.2.55 once Logs' Level filter takes words of its own, which go to the owner first. Home's first focus leaves the
    Pause AI switch, so a stray Enter after F5 pauses nothing (amending D256); a cut cell ends in "…". With no body
    control yet, the first focus is the first section's label. EldenNet's next pin takes 0.2.54 and 0.2.55.
- bp The Bug report's copy and save through GraceUI's share (GraceUI's proposal 0015; GraceUI 0.2.56, protocol 1.19) [done]
  - refs: GraceUI:docs/proposals/0015-report-copy-and-save.md, WHY§653, PAGES§4.6.8
  - gives: GraceUI's D263, settled on 2026-11-25 in EldenNet's formats (WHY§653): the scanned report as the preview's
    `share`, the clipboard GraceUI's in both views, `report.save` EldenNet's in game and a download in the browser,
    neither text nor buttons on a paired device. For EldenNet's 0.6 Bug report, or sooner.
    Built the same day (GraceUI feat/foundations e62f0ca; GraceUI's docs/eldennet-changes.md §89): `share = { copy =
    true, save = "report.save" }` on the preview block, `report.save` sensitive and not a file command, the block's topic
    the space's own; a device receives no `share` for any topic a report preview draws. EldenNet's floor for it is
    0.2.56. Copy alone also serves Workshop's unknown characters' TOML.
- bq This-PC-only pages and topics, and page data above GraceUI's minor refused at registration (GraceUI 0.2.57, protocol 1.20) [done]
  - refs: GraceUI:D258, WHY§653, PAGES§4.6.8, O132
  - gives: GraceUI's D264, agreed on 2026-11-25 from EldenNet's Workshop survey: a page's `pc_only` keeps it out of a
    paired device's view and page data, a topic's `pc_only` refuses a device's watch ("Only on the PC the game runs
    on."), checked so that such a topic is drawn on such pages alone and a setting homed there is sensitive; and
    register_pages refuses page data whose minor its GraceUI does not have, in its log and its one notice. Workshop
    takes both marks at the pin after it (O132: not released before).
    Built the same day (GraceUI feat/foundations b4e10f8; GraceUI's docs/eldennet-changes.md §90): commands named only
    on pc_only pages are sensitive, and settings homed there are left out of a device's settings topic; page data's
    minor is the floor's (20 with pc_only).
- br A space check of the browser view over a consumer's space folder (GraceUI 0.2.58) [done]
  - refs: GraceUI:D258, PAGES§4.6.8
  - gives: GraceUI's D265, planned on 2026-11-25: `npm run check:space` runs the real browser view over GraceUI's fake
    host with a space's fixtures and prints a text dump per page and state (shown_with on and off, a gate before and
    after its confirm), with the contrast, CSP and console checks; EldenNet keeps the dump as a golden of its own.
    Built the same day (GraceUI feat/foundations 0c8d0fa; GraceUI's docs/eldennet-changes.md §91): the npm script
    `check:space` in GraceUI's web, over a space folder and a golden, dumping this PC's view, a paired device's and each
    shown_with switch turned the other way, a gated page's gate first and a page not there named absent.
- bs A slider's typed entry in both views: Enter or a press on the value, the bounds in words, snapped to the step (GraceUI 0.2.59) [done]
  - refs: GraceUI:D173, GraceUI:DESIGN§3.8, PAGES§4.5
  - gives: GraceUI's D266, built on 2026-11-27 (GraceUI feat/foundations 960d428; GraceUI's docs/eldennet-changes.md
    §92): every integer, number and duration setting may be typed (a full stop or comma for the decimal point, the unit
    ignored, snapped to the step within the bounds; a refusal in words under the row), with no change to the protocol,
    the page data or the ABI. EldenNet's settings hold no numbers yet; a dump with a slider focused gains "Enter  Type".
    Taken at EldenNet's pin of GraceUI 0.2.62 on 2026-11-27 (engine b37e90b), with no golden changed.
- bt Plain text in game: the menu's serif and display roles in Noto Sans when GraceUI's Plain text is on (GraceUI 0.2.60) [done]
  - refs: GraceUI:D180, PAGES§4.6
  - gives: GraceUI's D267, built on 2026-11-27 (GraceUI feat/foundations 111119f; GraceUI's docs/eldennet-changes.md
    §93): every space's pages in game follow GraceUI › Menu's Plain text, baked at launch and applied at once from the
    nearest sans bakes in a session; grace-dev's dumps keep the serif faces. EldenNet's own panels (subtitles, chat box,
    F11) keep their faces until GraceUI publishes the setting to mods and the owner decides they follow it (O134).
    Decided on 2026-11-27 (O134): the subtitles and the chat box follow it from the next launch, in Noto Sans of their
    own; for EldenNet's pin of GraceUI 0.2.61 (D268, ABI 1.9), which publishes the value to mods.
    Taken at EldenNet's pin of GraceUI 0.2.62 on 2026-11-27 (engine b37e90b), with grace-dev's serif dumps unchanged.
- bu Plain text for mods: plain_text at connect and with the display event, a mod's own Noto Sans faces from GraceUI's fonts folder (GraceUI 0.2.61, ABI 1.9) [done]
  - refs: GraceUI:D180, GraceUI:D241, O134
  - gives: GraceUI's D268, built on 2026-11-27 (GraceUI feat/foundations bdc260b; GraceUI's docs/eldennet-changes.md
    §94): `plain_text` (1 or 0) in GraceUiConnectAnswer (56 bytes) and in GRACEUI_EVENT_DISPLAY (168 bytes), set with
    ui.scale and its default at once, after warm-up and on each change; has_cap("plain-text") for older hosts; the
    faces registered from GraceUI's fonts folder in a mod's registration window. For O134's build at the next pin.
    Taken at EldenNet's pin of GraceUI 0.2.62 on 2026-11-27 (engine b37e90b; S11.4.g; WHY§681 to §683).
- bv The Workshop build's five points: the space check whole through a pipe and waiting for every topic, a page's presence in game by id, device search and shown_with pinned by tests (GraceUI 0.2.62) [done]
  - refs: GraceUI:D265, WHY§663, WHY§668
  - gives: GraceUI's D269, built on 2026-11-27 (GraceUI feat/foundations fc2205a; GraceUI's docs/eldennet-changes.md
    §95): the space check exits once its output is handed on and waits for every topic before its stillness, so
    EldenNet's browser golden can hold Home's and AI's text again; `GraceUI_Test_MenuPage(space, page)` answers a
    page's presence in game by id; a device's search and a shown_with page before the settings come are pinned by
    GraceUI's DOM tests. No change to the protocol, the page data or the ABI.
    Taken at EldenNet's pin of GraceUI 0.2.62 on 2026-11-27 (engine b37e90b; WHY§684, WHY§685): the browser golden
    from stdout with Home's and AI's text, and Workshop's absence in game by GraceUI_Test_MenuPage.
- bw A held d-pad or stick repeats in the menu, and in both views a repeat only moves along or steps a number (GraceUI 0.2.63) [done]
  - refs: GraceUI:D269, PAGES§4.6
  - gives: GraceUI's D270, built on 2026-11-27 (GraceUI main 01d6e94; GraceUI's docs/eldennet-changes.md §96): the
    first repeat after 400 ms, then every 80 ms; a repeat never turns a switch, follows a link or acts; a held Enter
    no longer closes the field it opened. Nothing for EldenNet to change; its own panels still time their own repeats.
- bx The in-game menu fades in on open and out on the player's close (GraceUI 0.2.64) [done]
  - refs: GraceUI:D271, PAGES§4.6
  - gives: GraceUI's D271, built on 2026-11-27 (GraceUI main 892d9f0; GraceUI's docs/eldennet-changes.md §97): 120 ms
    in, and 200 ms out on F5, Esc, B at the top level or the close button, input back with the game at once. Danger,
    a load, focus lost, the watchdog, a fault and the player's exit still take it away at once, cutting a fade short;
    Reduce motion turns both fades off. EldenNet's panels and notices draw as before. Nothing for EldenNet to change;
    a fake-game check that reads pixels just after a close waits about 250 ms first, or turns Reduce motion on.
- by In game a page's body fades in at a change of page (GraceUI 0.2.65) [done]
  - refs: GraceUI:D272, PAGES§4.6
  - gives: GraceUI's D272, built on 2026-11-27 (GraceUI main 006241c; GraceUI's docs/eldennet-changes.md §98): at a
    change of page or space (the rail, LT and RT, Page Up and Down, a link, the switcher) the new page's body fades in
    over 120 ms, its focus ring with it; the rail, the top bar and the key hints stay still; Reduce motion turns it
    off, and an open's own fade covers its first page. Nothing for EldenNet to change; a fake-game check that reads a
    page's pixels just after a change waits about 150 ms, or until the test build's alpha_milli reads 1000.
- bz GraceUI follows its own settings with the menu closed too; the test build's plain_text is right before the first open (GraceUI 0.2.66) [done]
  - refs: GraceUI:D273, WHY§686
  - gives: GraceUI's D273, built on 2026-11-27 (GraceUI main 68d68ea; GraceUI's docs/eldennet-changes.md §99), from
    the question of EldenNet's 0.2.62 pin (WHY§686): GraceUI's thread reads its own settings (Menu size, the theme,
    Plain text, Reduce motion) at its first wake and at every change, open or not, so GraceUI_Test_GetMenu's
    plain_text reads 1 before the menu first opens and follows a browser's change at once. Nothing a mod hears
    changes; EldenNet's Plain text case may read the field again at the next pin.

### S11.3 EldenNet v0.2

- S11.3.a The LLM client in the DLL (built in S8.1.a and g; set up on F5 › AI or in the config files, no wizard
    until v0.6), the chat box, input capture and gating, the talk observer [done]
  - refs: DESIGN§8, DESIGN§11, WHY§364, WHY§399, WHY§400, WHY§404, WHY§418, WHY§419, WHY§420, WHY§421, WHY§442
  - gives: built offline on 2026-11-22 (WHY§364; engine b045026 to b4451c8), on the fake game and a scripted model:
    the core's chat requests sent through en-llm's handle in the DLL with their stop sequences, its cancels, and the
    dialogue slot's health for L3 and L4; the conversation's states, timers, draft, queued line and endings; gating by
    conversable and safe_context, target rules 3 to 5 and the talk observer failing closed, each clause tested alone
    on a simulated clock (WHY§418 to §422). Waits for the game: real snapshots from Lodestar's reader (S10.1.k) for
    the HUD, the pop-up, the talk camera, teams and distances, then S4 and S3 in game (S11.1.a, b). Left: a refused or
    not-now toast that names the holder (WHY§442); target rules 1 and 2 (f), 0 with the own-initiative lines (S12.1.c)
    and 6 with the call-out (S15.1.a); the mute state's sources (`speech.toml`, `types.toml`); a second Escape
    (S11.4.d, dropped on 2026-11-23, WHY§461). By O108 (2026-11-22, the owner) a game banner stops the box opening but never closes an open one, which
    holds its pages: the core's gate changed to match on 2026-11-23 (engine a7f8509).
    The polish on 2026-11-23 (engine merge 5ec1555; WHY§493 to §495, §497): the second Esc's rule removed (G39 (c));
    the refusal's toast names the holder by GraceUI's id, and not-now by its reason (its words approved, O110);
    `features` and `range` reach the core from settings.toml and the pages.
    Title changed on 2026-11-27 from "config file only": since S11.4.a's second to fifth slices (G34, approved as O95;
    O98, O99, O104), a v0.2 player sets providers, keys and models on AI › Providers and AI › Models.
    Left on 2026-11-27, overtaking the list above: none of its own. The refused toast is built (the polish, engine
    5ec1555; O110); the second Escape was dropped (G39 (c)); the mute state's sources go with S13.2.b (`types.toml`)
    and S1 (`speech.toml`, S10.1.c). In game it waits for S10.1.m's HUD, menus and talk camera; teams and position
    are confirmed already (WHY§614, WHY§654).
    Status on 2026-11-27: done. Built and polished offline (engine b045026 to b4451c8, a7f8509 for O108, then 5ec1555;
    WHY§364, WHY§418 to §422, WHY§493 to §497). What is left belongs to other parts: target rules 1 and 2 (f), 0
    (S12.1.c) and 6 (S15.1.a); S10.1.m's fields in game; S4 and S3 (S11.1.a, b).
- b The prompt, the sanitizer and lint, pages, and commit on display [done]
  - refs: DESIGN§8.3, DESIGN§8.4, DESIGN§8.5, WHY§402, WHY§403, WHY§409, WHY§412, WHY§414, WHY§415, WHY§427, WHY§432
  - gives: built offline on 2026-11-22 (WHY§364): the prompt's blocks in en-prompt's syntax, rendered from the
    persona's record within their caps and trim (WHY§403, §414, §428 to §430); the sanitizer (WHY§409 to §411); the
    lint with `banned.toml` and the safety list kept as fingerprints (O107; WHY§412, §413), one regeneration and the
    deflection ring; the pager's caps, lazy sealing and holds (WHY§415); commit on display into v0.2's event log, kept
    in memory behind the store's interface (WHY§364 (7)); `render-prompt --fixture`, the prompt goldens and a fuzzer
    for the sanitizer. Waits for the game: block 9's scene from Lodestar's reader (S10.1.k), which has the time of day
    alone so far (WHY§421). Left: the safety list's entries (h); a queued line that waits for a one-page reply's hold
    (WHY§432); a narrower "Name:" rule, judged once the eval counts its removals (WHY§410); each page written to the
    store with v0.3's log (S12.1.a).
    The queued line goes once the stream ends with the last page up (2026-11-23, WHY§494).
    Status on 2026-11-27: done. Built offline (WHY§364, WHY§403, WHY§409 to §415), the queued line's hold closed
    (WHY§494). Left to other parts: the safety list's entries (h); the count of the sanitizer's "Name:" removals, which
    goes with e's eval, before the narrower rule is judged (WHY§410); the pages written to the store (S12.1.a); block
    9's time of day in game, once the clock field is confirmed (a lead so far, WHY§657).
- c One pilot persona, in memory only, with the world view, what it knows and its road; news raised as O28
    sets [waiting]
  - refs: DESIGN§10.6, O10, O28, WHY§418, WHY§424
  - gives: on 2026-11-22 the test persona was built: invented, `test_only`, built into the core's content and bound to
    the fake game's invented character through the tests' overrides, in the tests' realm alone (WHY§418, §423). The
    pilot persona and `known(P)` are not; the truth tables wait for `known(P)` (WHY§424).
    Built offline on 2026-11-23 (engine 5d6577e; WHY§471 to §475): the pilot Kalé (`content/eldenring/personas/
    kale.toml`, drafted with help, its review pending) with the world stages, tracks, regions and the catalogue's
    aliases as unverified leads; `known(P)` passes the truth tables for both personas at the eight states. Waits for
    the owner: the review, field by field (O43, O45, O46; its three questions answered as O109). Waits for the game: S1 for his row, S6
    for his flag block, his talk's flag and the world's flags; until then, in game, he knows only his road.
    Status on 2026-11-27: waiting. It is built: the pilot, `known(P)` and the truth tables at the eight states (engine
    5d6577e; WHY§471 to §477), the eval at two states (engine 02c3856), and news raised by the world frame's item 9.
    The next step is the owner's alone: the pilot's review, field by field, still pending in his file (O43, O45,
    O46). O109 answered three questions from that review but is not the review. After it, S1 for his row and S6 for
    his flags in game (S11.1.b).
- d Positive grounding, the "you, now" line, and settings with the range and the feature switches [done]
  - refs: O36, DESIGN§11.4, WHY§424
  - gives: on 2026-11-22 the fixture states were built, the eight and `ashen`, as catalogue aliases, block offsets and
    track steps with no flag id and no text (WHY§424). The pilot persona and `known(P)` are not (c); the "you, now"
    line has its place in block 9's scene, which nothing fills yet (WHY§421), and the range and the feature switches
    are not read yet.
    Built offline on 2026-11-23 (WHY§471, §473, §476): block 5b by `known(P)` (NPC §3.6's `after_radahn` block word
    for word), the "you, now" line, and `features` and `range` with ARCH §11.5's bounds in the core, `features.chat`
    and `features.self_awareness` read. Left: the DLL's settings reader handing them to the core (with the polish).
    The settings reader hands them to the core since 2026-11-23 (WHY§497); no page offers the keys yet.
    Status on 2026-11-27: done. Block 5b by `known(P)`, the "you, now" line, `features` and `range` are built (WHY§471,
    §473, §476), and the DLL's settings reader hands them to the core (WHY§497; engine 5ec1555). That no page offers
    the keys is by design: the F10 Ranges page is v0.6's (DESIGN §16.2), and the features' switches are not settings
    of v0.2's pages.
- e The eval tool and its CI stage, the fixture states and the pilot's truth tables [active]
  - needs: S10.3.c
  - refs: DESIGN§14, WHY§424, WHY§425, WHY§426, WHY§431
  - gives: built offline on 2026-11-22 (WHY§364 (10)): `eldennet-engine eval` with `--llm mock` and `recorded`, four
    scenarios on the test persona (first words, no answer, a regeneration then a deflection, the word cap cutting the
    stream), its report a local-only kind, run by CTest on demand under the label `eval` (WHY§425, §426, §431);
    `mockllm` as a scripted model; the fixture states and the truth tables, written and gated (WHY§424). Nothing of it
    waits for the game. Left: the CI stage on content changes, and the live and judged sets, which wait for a key and
    the judge's check; the pilot's truth tables with `known(P)` (c); mockllm's local SSE server, for en-llm's live path
    (WHY§425); counting the sanitizer's "Name:" removals (WHY§410).
    On 2026-11-23 the pilot's truth tables run with `known(P)` (c), and the eval takes fixture states and checks what
    the prompt holds: the pilot speaks at two states, his knowledge differing as it should (WHY§477).
    Status on 2026-11-27: active. Built: the eval with mock and recorded models, five scenarios and mockllm (engine
    26b8551 and d2cb93d); the fixture states and both personas' truth tables, which the eval takes (engine 02c3856;
    WHY§424 to §426, §431, §477); the eval's CTest label, run on demand. Left: the CI stage, which needs S10.3.c, since
    there is no CI service yet; the live and judged sets, which need a key and the judge's check (O29), work shared
    with X1 (S11.1.c); mockllm as a local SSE server (WHY§425); the "Name:" removal count (WHY§410).
    On 2026-11-27 the speech tap counts the "Name:" prefixes by class, the other removals, the lint's hits, the pager's
    cuts and the archaic share (engine 656b17b; WHY§677). The live path and mockllm's SSE server wait (O140).
- f Target rules 1 and 2: the talk key arming chat on the game's own talk, once spike S3 has checked the talk observer
  - needs: S11.1.b
  - refs: NPC§5.2, WHY§420
  - gives: left out of the first conversation, which takes targets by rules 3 to 5 alone (2026-11-22).
- g The overrides' `disable.toml`: shipped personas and cards turned off by id, shown on Content packs [done]
  - refs: ARCH§11.6, DESIGN§11.4, PAGES§4.6, O119, WHY§499
  - gives: not built with the first conversation's content loading (2026-11-22), which reads the overrides folder's
    whole files alone.
    Built on 2026-11-23 (WHY§499): `disable.toml` turns shipped personas off by id, checked strictly, warnings never
    naming an id; cards and the other lists warn (none shipped in v0.2). Left: the Content packs page, which the
    engine's pages do not have yet.
    Built on 2026-11-24 (engine 7bfe283, merged at 86d3991; O119): Content packs shows what disable.toml turns off,
    and the fake game's pages case reads it turning the test persona off. Status on 2026-11-27: done.
- h The shipped safety list's fingerprints, made by the owner with `lexicon fingerprint` [waiting]
  - refs: O107, NPC§8.3, WHY§412
  - gives: `safety.toml` ships with its header and no entries (2026-11-22). The owner fingerprints the words on their
    own PC, and only the sorted fingerprints are committed; no word is written in the repository, its tests or its
    reports (O107).

### S11.4 GraceUI 0.2

- S11.4.a The GraceUI folder in EldenNet's release, and the first pages in the browser: Home, AI, System, Tools, Content packs, Docs [done]
  - needs: S11.2.h
  - refs: G2, G20, PAGES§4.6, GraceUI:D163, GraceUI:D190, O95, O98, O99, O104, O119, O120, O125, O127, O128, O129, O130, O131, O132, O133, O135, G34, G36
  - gives: since G20 these are EldenNet's pages, built from EldenNet's page descriptions in S11.2.h's format. The
    allowances' and reactions' inherit tri-states (PAGES) use GraceUI's third = "inherit" and the settings topic's
    `inherited` list, added on 2026-11-20 at EldenNet's request (GraceUI feat/foundations ac4bc39). Since GraceUI
    0.2.0 (2026-11-20) the same space shows in F5's menu too (D171), and EldenNet follows protocol 1.1 (D172) and
    D173's confirmations, increments and words: GraceUI's docs/eldennet-changes.md §27 to §29. Since GraceUI's D181 a
    player may quiet EldenNet's notices, so what the player must know (the banner's reason, a failed start) is on
    EldenNet's Home page and in its log too, never in a notice alone (GraceUI's docs/eldennet-changes.md §34).
    First slice built on 2026-11-20 (engine 75e6303): EldenNet's space with Home, System and Settings › Developer
    (PAGES §4.5 homes the debug settings there), linted by GraceUI's tools at 8d45b20 (0.2.5), T6's register_pages,
    publish on watch and answer, settings written on T11; graceui-sys re-vendored at 8d45b20. Tested in grace-dev
    (goldens of each page's structure) and on the fake game in both views. In game, GraceUI 0.2 draws only a mod's
    settings and shortcut rows, so Home's cards show in the browser alone until GraceUI draws a mod's components
    (WHY§266 to §270). AI, Tools, Content packs and Docs follow. Since GraceUI 0.2.6 (S11.2.r) the in-game menu draws
    Home's and System's cards too.
    Second slice on 2026-11-21 (engine 80ef2fe), at GraceUI 0.2.7 (beca859): AI › Providers, Models and Self-test.
    Providers come from the shipped presets (GraceUI has no form or per-row action yet); keys are secret settings,
    write-only, shown as Set or Missing, kept in secrets.toml with ARCH §11.5's one-user access list; the self-test and
    the model list are long commands answered at once. Checked on the fake game in both views against a fake model
    server, with the key in no frame, log, report, recording or file (WHY§274 to §276). No models.toml ships yet, so in the
    game only Custom can be chosen until X1 and S8.1.h have measured the models. GraceUI's proposal 0007 (G34): row
    actions, live pickers, row secrets and small forms, for the AI pages' providers and models, put to the owner and
    EldenNet on 2026-11-21; not built. GraceUI's D190 records EldenNet's commands on its secret settings as built.
    The owner approved proposal 0007 (G34, built as GraceUI 0.2.8, S11.2.u) and the slices' open choices on 2026-11-21 (O95): no lamp while starting, the
    tones, labels by the build's increment, the banner's words on greyed pages (the Now card's sentence follows in the
    next slice), and the words as drafted.
    Third slice on 2026-11-21 (engine 8008dd1), at GraceUI 0.2.8 (0738fa3, protocol 1.2): the AI pages fully editable
    with G34 as built (GraceUI's D192 to D196): a Remove and the key on each provider's row, a small form for a provider
    of one's own, a model picker per job from what its provider lists. One base-URL check in en-proto serves the form,
    the providers file and the key rows: https://, http:// to this PC, or http:// to a literal private address with no
    key (O98); no name, password, query or fragment; each refusal in words on System › Status (WHY§283, WHY§284). O95's
    choices applied (the Now card at L1 in the banner's words); Home and System on GraceUI 0.2.7's props (Home greyed
    at L1, "No problems.", Logs newest first). EldenNet's GraceUI floor is 0.2.8. Checked on the fake game in both views
    against a fake model server, the key nowhere (WHY§285 to §287). Next: a provider picker per job; three display
    requests to GraceUI (a key not needed, a key refused, a picker's default) sent the same day.
    Fourth slice on 2026-11-21 (engine 2d125a1), at GraceUI 0.2.9 (908c179; GraceUI:D197, GraceUI:D198): a provider
    picker per job on AI › Models (a provider whose list lacks the job's model waits, unwritten, for one of its models;
    a reset clears the job's pick), a key's Refused and Not needed on AI › Providers (a server on this PC takes a key
    only once a call asks for one; one on the home network never), and the changed mark from the space (a job's rows
    only while the providers file holds its pick). EldenNet's GraceUI floor is 0.2.9, since 0.2.8 draws both new
    states as Missing. Checked on the fake game in both views against a fake model server with a keyless path, both
    keys nowhere (WHY§297). Left to the owner: the slice's eight readings; cards per job and a fallback picker with
    GraceUI's proposal 0008 (S11.2.x).
    Fifth slice on 2026-11-21 (engine b05fda5), at GraceUI 0.2.12 (a9aac78; G36 (a), GraceUI:D202): AI › Models as one
    card per job (Conversation, Quick lines, Prepared lines, Memory, Self-test), each a section with its slot as a
    chip, its state as a live status (Ready, Cooling down, Resting after failures, Failing, No key yet, No model), its
    provider and model, its fallback and the fallback's provider (new; only for a job with a model of its own), and
    its fixed budgets read-only; the Jobs table gone; a picker's Default is its row's reset. EldenNet's GraceUI floor
    is 0.2.12. Checked on the fake game in both views against the fake model server, both keys nowhere (WHY§300).
    Left: the Decisions (0.4) and Profile drafts (0.6) cards, Prepared lines' pool row, state and "Prepare lines now"
    (its section action, with the pool), Advice, Apply and Reset to preset; GraceUI's pacing of a page's
    subscriptions, asked the same day. The owner accepted the slice's eleven readings as written (O99), and both slices
    are in PAGES §4.5 and §4.6.2 and ARCH §11.1, §11.2 and §11.4 as dated notes. The fourth slice's readings were decided as built the
    same day (O104). GraceUI paced the subscriptions the same day (S11.2.z).
    Since the first conversation (2026-11-22, WHY§422, §442): the core's ladder has L3 and L4, which the pages still
    show as Observing, since en-overlay's pages have no speaking level yet. Words players read, left for the owner's
    review with the level the pages give them.
    L3 and L4 are speaking levels on the pages since 2026-11-23 (WHY§496), their words approved (O110). Left:
    L2's words still say NPCs speak from a later version, which a v0.2 build makes untrue (fixtures and goldens).
    Overtaken by O120 (2026-11-24; engine 425a72d), as noted on 2026-11-27: L2's words say "NPCs speak once a model
    answers Conversation".
    Content packs on 2026-11-24 (engine 7bfe283, merged at 86d3991), at GraceUI 0.2.39 (S11.2.bb): a category of its own
    after Settings, both views, read-only, as PAGES §4.6.9 has it: the shipped content (fixed words for its name and
    credit, since the pack has no manifest yet; the personas counted, knowledge and event rules as Included), the
    overrides folder's files each Loaded, Refused, Not loaded (another file refused the load) or Not read, with a row
    per problem, what disable.toml turns off, and the session's reloads (WHY§600 to §604). In v0.2 the content layer
    turns off personas alone and ships no card, so those rows stay empty in game; a layer's problem names its field
    but no line. Left: Docs and Tools › Bug report, waiting on GraceUI's answers (a document page, a multi-line field
    and a preview). Its readings and words accepted as written by the owner the same day (O119), PAGES §4.6.9 noted.
    Docs' ten documents drafted in our own words the same day (engine ad4b82b, merged at 164f00a), sent to the owner
    for review; O120's words merged with them (the refused-key toast, Home's Observing reason; Writing content without
    its own-profile section). Next: GraceUI 0.2.44's pin with the Docs page and Tools › Bug report on its document
    block, prose field and report preview (S11.2.bg).
    Built the same day at GraceUI 0.2.44 (engine c9832bb, 6abc997 and faf9e43, merged at 0538221; WHY§617 to §622):
    Docs, a master page with its list's search over titles and headings and the ten documents bundled in the DLL by a
    checked build step; Tools › Bug report, the "What happened" prose field and the report's preview (What happened,
    Versions and State always in; Settings, Content and the recent log switchable by `report.part`), scrubbed of
    SteamIDs, paths and local addresses, the typed words kept in memory for the preview alone; Settings, Docs and
    Content packs as the rail's foot. EldenNet's GraceUI floor is 0.2.44 (WHY§619). Docs shows in game too, since a
    page cannot yet be kept out of one view; in game its documents show "Nothing to show." until GraceUI watches a
    document's member (WHY§621), and a preview field is one line (both asked of Grace). The pages' new words accepted
    as built by the owner the same day (O125). Copy and save of the report stay 0.6's.
    The model calls' record and Pause AI for this session built on the engine's side the same day (engine b8371f3,
    46705d3, 05f13d4, merged at c9f35cb; WHY§627 to §629): a row per call v0.2 makes, its displayed text only if shown,
    the session's usage, the cost only where reported; Pause AI as a core input that holds the ladder at L2 and sends
    nothing. Next: GraceUI 0.2.46's pin, wiring AI › Requests, Home's Model calls card and the Pause AI quick switch.
    Built the same day at GraceUI 0.2.46 (engine 9f1befc and 3e80305, merged at 2f27925; WHY§633 to §637): AI › Requests
    (tiles, the table with its filter bar on job, purpose and outcome, sorted on time, first token, total and tokens;
    live), Home's Model calls card (the last 20; Home live), and Pause AI for this session as GraceUI's quick switch
    (`ai.pause`, `status.ai_paused`, not sensitive; the Status card's line; the self-test and model lists refused).
    Left: a row drawer for Why and Text shown, and Pause AI's chip in Home's header, both asked of Grace. The new words
    accepted as built by the owner the same day (O127). The fake game's suite under Wine: 144 of 144.
    System › Performance and System › Traces built the same day at GraceUI 0.2.46 (engine 1ee7d14, merged at 0b0030b;
    WHY§638 to §644): Performance with the core's and the reader's step p99 as tiles, C3's two task limits as Budgets,
    a row per kind of step, and Storage in words (no database in v0.2); Traces with the first page's median and 95th
    percentile (from 20 lines on), the first token's median and the lines, a sentence against the targets for where the
    Conversation model runs, and a row per line with its queue, first token, first page, total and verdict, never text.
    Frame cost and memory are left out until a source exists: GraceUI 0.2.52's `on_present` (S11.2.bl) gives the first.
    The fake game's suite under Wine: 145 of 145. Its words, and whether Traces holds its p90 or its p95 to C5's target,
    put to the owner (O128). Left: Tools › Workshop, on GraceUI 0.2.51 (S11.2.bk).
    The owner chose the 90th percentile and accepted the words as built on 2026-11-25 (O128).
    GraceUI 0.2.53's pin on 2026-11-25 (engine 361fd65 and 4058eb0, merged at 6b92444; WHY§646 to §652), taking 0.2.49 to
    0.2.53 together: Docs' exit dropped; Requests' Why and Text shown and Home's card's Text shown in row drawers, each
    row keyed by its call's id; Home's Pause AI header switch beside the quick switch; Logs' filter bar on each row's
    own level, and Logs in game; in-game tables with a header line and slots; Traces at its 90th percentile from 10
    lines on. EldenNet's GraceUI floor is 0.2.50, page data minor 16. The fake game's suite under Wine: 145 of 145.
    Tools › Workshop planned the same day (a survey of the seven parts PAGES gives it, each on T6), and its words and
    readings decided by the owner (O129 to O133): the full gate sentence, Developer views turning it on at once, a
    "Record this session" header switch, this PC alone (not released until GraceUI gives a this-PC-only mark, asked of
    Grace), and Logs' level words. Being built on GraceUI 0.2.55, EldenNet's floor rising to 0.2.55 with Logs' words.
    Tools › Workshop built on 2026-11-27 (engine wip/workW, merged at d1a96c2; WHY§658 to §668), with GraceUI pinned at
    0.2.58 (0c8d0fa) and EldenNet's floor at 0.2.57, page data minor 20: in the browser alone behind its gate and ribbon,
    on this PC alone by `pc_only`, its seven sections and Record this session; Logs' level words (O133); GraceUI's space
    check over EldenNet's pages as en_pages_browser. The fake game's suite under Wine: 144 of 147, the pause case fixed
    after, and the read-state and read-clock cases failing alike at the base in this container (for Lodestar). Left for
    v0.2's pages: none; the Bug report's Copy and Save stay 0.6's (S11.2.bp).
    Done on 2026-11-27: every page this part names is built, and the GraceUI folder ships in EldenNet's packages.
- b The browser dashboard, always on, on loopback only: EldenNet's space served whenever its gate and safety steps
    pass [done]
  - needs: S11.2.i
  - refs: G4, GraceUI:DESIGN§7.2
  - gives: the server's security suite moved to S11.2.i on 2026-11-20 (the Grace session's plan).
    Status on 2026-11-27: done. EldenNet registers its space for both views on the main path, and on the banner path
    (a refused gate or anti-cheat) for the game's view alone, so the browser never gets it there. GraceUI's server
    and its security suite are S11.2.i, done. In the fake game's pages case a browser reads every topic and sends
    commands, its banner case checks the game's view alone, and the anti-cheat case puts GraceUI in its notice-only
    mode. In the real game the start still stops at discovery, so the browser has not served EldenNet's space there:
    G4's rule working as designed, and the check in game is S11.5's.
- c EldenNet's subtitles and chat box as panels taking input through request_input, with the input-leak tests across
    holders (the call-out box moved to S15.1.a) [done]
  - refs: G17, GraceUI:DESIGN§10, GraceUI:D176, GraceUI:D139, WHY§364, WHY§434, WHY§435, WHY§436, WHY§437, WHY§439, WHY§441
  - gives: the server with two spaces and per-space routing moved to S11.2.i on 2026-11-20. The chat box needs typed
    characters, which ABI 1 does not yet give a mod's panel (WHY§241): GraceUI's input event comes first. Agreed on
    2026-11-20 as GraceUI's D176 (ABI 1.1, GraceUI 0.2.1, EldenNet's floor for v0.2): keys, characters, the pointer
    and pad buttons for the holder's mod only, its end and why, and the caret for the IME, fed to EldenNet's own
    Dear ImGui context (GraceUI's docs/eldennet-changes.md §31). EldenNet's text sizes follow GraceUI 0.2.2's display
    event and scaled faces (S11.2.n), and drop their constant (GraceUI's docs/eldennet-changes.md §33). The pad chord that opens the box (NPC §5.6) waits
    for a D-entry of GraceUI's, for S4's second round after R1: GraceUI's proposal 0005, pad chords for hotkeys, put to
    the owner and EldenNet on 2026-11-20, approved by the owner on 2026-11-21 (O95; GraceUI's G35); not built. Built the same day in GraceUI 0.2.1 (feat/foundations
    2131839): the chat box can take typing through ABI 1.1 (WHY§244 to §246). The pad chord built on 2026-11-22 in
    GraceUI 0.2.14 (GraceUI's D204, ABI 1.3, has_cap("pad-chords"); S11.2.aa): GRACEUI_PAD_CHORD(XINPUT_GAMEPAD_DPAD_UP,
    XINPUT_GAMEPAD_BACK) on the talk hotkey, one GRACEUI_EVENT_HOTKEY on completion; EldenNet's floor for the chord is
    0.2.14 (GraceUI's docs/eldennet-changes.md §47). Not wired in EldenNet yet (wired on 2026-11-22, below). GraceUI 0.2.15 fixes a rare lost end of a
    holder (GraceUI's D207, WHY§360): the chat box hears every end from 0.2.15. By G37 (2026-11-22, WHY§362, S11.2.ac)
    EldenNet's three key settings become the defaults it registers, its Settings › Hotkeys a read-only list with a link
    to GraceUI › Hotkeys, and the call-out names the key from describe_hotkey (ABI 1.4). From GraceUI 0.2.16 the talk
    key and chord are rebound on GraceUI › Hotkeys, and describe_hotkey's floor is 0.2.16. The chat box's text uses
    GraceUI's glyph_table (Grace's answer of 2026-11-22, WHY§364). T6 waits on GraceUI's wait handle (ABI 1.5, floor
    0.2.17) for the chat box's opening within 50 ms (NPC §5.1): 1 to 5 ms from a hotkey's key to the wake in GraceUI's
    harness (WHY§373).
    Built offline on 2026-11-22 (engine b4451c8; WHY§434 to §441), at GraceUI 0.2.20, floor 0.2.17: the lower third,
    the speaker card, the chat box and the hint as GraceUI panels, their text in quads from GraceUI's glyph table on
    GraceUI's own EB Garamond and Noto Sans, the fades stepped at T6's pace; input through request_input and the
    holder's events (the caret, the swallowed character, Esc, every holder end, the model's close answered with the
    draft); the talk and skip hotkeys, with the pad chord behind its cap; T6 on GraceUI's wait handle, 1 ms from the
    talk key to the box's first frame on the fake game; no input leak across the menu, the F11 view and the box; a
    typed line answered in the lower third and committed, end to end on the fake game. Waits for the game: S4 and
    S0-E's input half (S11.1.a). Left: the chord and danger tested against GraceUI's real test build, as they are so
    far against the stand-in only; rounded corners, and the Tarnished's lines in italic, which wait for more faces per
    mod in GraceUI (D139 gives EldenNet eight) and an italic face; the pointer in the box (a second Escape,
    d, dropped on 2026-11-23). The
    call-out box waits for v0.6 (WHY§364 (5); S15.1.a). Since O113 (2026-11-23, the owner's direction) the box asks
    for the keyboard and the pad, never the mouse: no pointer, and the camera still turns while the player types
    (engine 8424f15).
    On 2026-11-24 the chord and the danger close were tested against GraceUI's real test build (engine 08fef7a,
    merged at 4c5632d; WHY§584 to §587): -talk-chord and -talk-danger. The italic face (GraceUI 0.2.36, S11.2.ay),
    the rounded corners and the kept draft's loss when T is pressed during danger (WHY§586) are being done with the
    move to GraceUI 0.2.36. Done the same day at GraceUI 0.2.38, ABI 1.7 (engine 07ae392 to 678b313, merged at
    6930988; WHY§588 to §594): the Tarnished's lines in the box's history in EB Garamond Italic where GraceUI ships it;
    the box's ground and frame rounded at GraceUI's card radius and the hint's key at its control radius; a box GraceUI
    refuses at its opening keeps the kept draft; the chord alone opens the box by the chord (via=pad_chord, no
    swallow), from GraceUI's hotkey_by. Left: the call-out box (v0.6), and S4 and S0-E's input half in game.
    The same day the fake game learnt a backdrop of our own, a swap chain at a given size and format and shots of the
    frame as presented, driven by tools/preview/shoot.py (engine a44d9f4 and 2311c1d, merged at de94506; WHY§595,
    §596): the box, the answer, the hint, the menu and F11's view, on a test backdrop, are in the showcase's In game
    section. The caret, seen touching the last letter, now stands a pixel clear (engine 52e6d72).
    Title changed on 2026-11-27 from "subtitles, chat box and call-out box": the call-out box went to v0.6 on
    2026-11-22 (WHY§364 (5); S15.1.a). O113 (2026-11-23) overtook the pointer in the box of the Left list above.
    Status on 2026-11-27: done. The lower third, the speaker card, the chat box and the hint are built as panels
    (engine b4451c8); the pad chord and the danger close are tested against GraceUI's real test build (engine
    08fef7a); the italic lines, the rounded corners and the kept draft are done (engine 6930988; WHY§588 to §594), and
    the caret is fixed (engine 52e6d72); the box no longer takes the mouse (O113, engine 8424f15); on the fake game no
    input leaks across the menu, F11 and the box. Left to other parts: S4 and S0-E's input half in game (S11.1.a).
    Plain text for the subtitles and the chat box (O134, 2026-11-27) is g.
- d A second Escape while a reply paces (NPC §5.7): dropped by the owner's G39 (c) on 2026-11-23 [done]
  - needs: c
  - refs: NPC§5.7, WHY§437, WHY§461, G39
  - gives: the core's rule is built, but with the box closed nothing holds input and GraceUI takes no hotkey on Escape,
    so the second press never reaches EldenNet (2026-11-22). Dropped on 2026-11-23 by the owner (GraceUI's G39 (c),
    proposal 0011): Esc and the pad act on the box only while it is open, Enter sending and Esc cancelling and
    closing; once the box has closed or the reply is out, they are the game's again, and a reply still pacing
    finishes (WHY§461). The core's unreachable rule is removed with the conversation's polish.
    Title changed on 2026-11-27 to say the part was dropped (G39 (c)), as the header asks of a dropped part. It read
    "a way for it to reach EldenNet once the box has closed, asked of GraceUI" after the colon.
- e EldenNet's sensitive commands and settings marked for paired devices (GraceUI 0.2.25's protocol 1.8) [done]
  - needs: S11.2.al
  - refs: G9, GraceUI:D224, WHY§490, WHY§523
  - gives: EldenNet's space marks `sensitive_commands = ["secrets.set", "secrets.clear", "providers.update"]` and
    `sensitive = true` on any setting a paired device must never change, so GraceUI refuses them from a device before
    any confirmation (GraceUI's docs/eldennet-changes.md §60). With the pages' work. Done on 2026-11-23 with the profile
    editor (merged at c223b6e) under EldenNet's own names, since §60's example names no command of EldenNet's space
    and GraceUI's lint refuses an undeclared one: `providers.add`, `providers.remove` and `providers.key`, and the
    settings `debug.panel` and `debug.record_sessions`; the editor's save is left unmarked (WHY§523). The fake game's
    case pairs a device on 127.0.0.2 and checks it refused (WHY§525). By the owner's O112 (b) the editor's save, reset
    and restore join them: notes are saved on the PC alone. Built the same day (engine d37d35a); the editor's case
    checks a save and a reset from the paired device refused.
- f A start stopped at discovery names what failed, and Home's build row says supported only once the game is read [done]
  - refs: WHY§530, WHY§529, PAGES§4.5, O114
  - gives: on the owner's PC the banner said "game build 2.7.2.0 not supported yet (reason: gal_files)" while Home
    said "2.7.2.0, supported" (2026-11-23). The words decided by the owner the same day (O114 (a)): "EldenNet inactive:
    its files for game build X are incomplete. Reinstall EldenNet, or look for an update." or "...it cannot read game
    build X yet. Look for an update of EldenNet."; the row "X, recognised, not readable yet" until the game is read.
    Built the same day (engine d37d35a): the banner by the stop's detail, the Home and System › Status rows by whether
    the reader runs (the Status check waiting while the start is under way, failing once it stopped).
- g EldenNet's subtitles and chat box follow GraceUI's Plain text from the next launch (O134) [done]
  - needs: S11.2.bu
  - refs: O134, WHY§681, WHY§682, WHY§683
  - gives: with Plain text on at launch, the subtitles and the chat box in Noto Sans of their own, F11 keeping its
    faces; at EldenNet's pin of GraceUI 0.2.61 (GraceUI's D268), where the engine pins 0.2.58.
    Built on 2026-11-27 with GraceUI pinned at 0.2.62 (fc2205a), taking 0.2.59 to 0.2.62 together (engine wip/pinH,
    merged at b37e90b; WHY§681 to §687). The type is chosen at connect behind the plain-text cap, and only its files
    are read. With Plain text on, Noto Sans Regular takes the text and the ui, and Noto Sans SemiBold the name line;
    the Tarnished's lines keep their colour. A change in the session is logged for the next launch. EldenNet's floor
    stays 0.2.57. The browser golden holds Home's and AI's text again, and Workshop's absence in game is checked by
    id. The fake game's suite under Wine: 150 of 150.

### S11.5 v0.2's exit checks

- needs: S11.1, S11.3, S11.4
- S11.5.a Latency, no multiplayer requests, output hygiene, silence, spoilers, voice, the install (C13: the README,
    then AI › Providers or the config files) and world awareness, on the pilot
  - refs: DESIGN§16.3
  - gives: title changed on 2026-11-27 from "the config-file install": C13's v0.2 path is the README with AI ›
    Providers and Models, or the config files, since S11.4.a's second to fifth slices (G34, approved as O95; O98,
    O99, O104).
- b The pilot's eval report and release checklist; no input leaks; the test questions per character (O86); no secret in any log
  - refs: NPC§12

## S12 v0.3 Remembrance [planned]

- needs: S11
- refs: DESIGN§16.2

### S12.1 EldenNet v0.3

- S12.1.a SQLite: the event log, facts, journal, claims, rollback, NG+ and crash recovery [active]
  - refs: DESIGN§10, MEM§8, MEM§9, WHY§688, WHY§689, O144, O146
  - gives: planned on 2026-11-27 (engine wip/v03): v0.3's offline parts in 21 commits and three slices, built back to
    back as the owner chose: the store and personas' audiences in Augur; memory in the core; speaking up, the DLL and
    the pages' data. Personas first get an audience (WHY§688). SQLite 3.54.1 is pinned (O146). The rollback choice
    is asked only when what would be forgotten matters, in a centred panel (O144). The speaking-up settings wait on
    O145.
- b What each persona has seen of the world, the since-line, and reaction cues
  - refs: MEM§12.13, O28
- c The pacing core and the autonomous class; the pilot's own-initiative lines, once S3 and S6 allow them [done]
  - refs: DESIGN§9.4, DESIGN§8.9, O31, NPC§5.2, WHY§420, O147
  - gives: with them target rule 0, the talk key answering a line a persona has just addressed to the Tarnished
    (NPC §5.2), which the first conversation left out (2026-11-22).
    Moved to S13.2.j on 2026-11-27 by O147: own lines cannot run in game before S3 and S6, so they and the pacing
    core ship whole in v0.4, with the speaking-up settings (O145).
- d The 60-second news cooldown, subjects, persona instructions and the player's profile layers, by file
  - refs: O30, DESIGN§7.7
  - gives: noted on 2026-11-27: persona instructions and the player's profile layers are built already (the v0.3
    survey); the subjects and autonomy's subject line move with autonomy to S13.2.j (O147).
- e One memory per witness, and the lived layer's update at quiet moments
  - refs: O46, DESIGN§10.3, MEM§13

### S12.2 GraceUI 0.3

- S12.2.a Named NPCs (the pilot) and Playthrough; Conversations and Memories, with Forget
  - refs: PAGES§4.6
- b Home's Nearby; Speaking up for named NPCs; the top bar's Journey
  - gives: noted on 2026-11-27: Speaking up moves to S13.3.a, with the own lines (O145, O147).

### S12.3 v0.3's exit checks

- needs: S12.1, S12.2
- S12.3.a Memory; the trim, world trim, NG+ and cue tests; the since-line, claim and cue probes
  - refs: DESIGN§16.3, MEM§12
- b Forget's confirmation and backup; the spoiler goldens for memories and journeys
  - refs: PAGES§5

## S13 v0.4 War cries [planned]

- needs: S12
- refs: DESIGN§16.2

### S13.1 The spikes for v0.4

- refs: DESIGN§16.1
- S13.1.a Spike S7's additions: NPC-against-NPC engagement, and which cards voice scripted lines [waiting]
- b Spike S8, statuses and sleep, in game, and S3's detector of scripted lines per kind [waiting]
- c X2, the bark taste test: pools against seeds [waiting]

### S13.2 EldenNet v0.4

- S13.2.a Six archetype cards with seeds, rules, the scheduler, the feed then labels, and pools
  - refs: O15, O23, O24
- b The combat and ambient classes on the same pacing; reactions and allowances; the NPC types file
  - refs: O33, O38
- c Combat for every combatant; generic exchanges and autonomy; boss-fight lines, kind by kind
  - refs: O34, O42, DESIGN§9.10
- d Status awareness and reactions as S8 passes; generic calm lines once it verifies sleep; sparse idle remarks
  - refs: O40, O44, O14
- e Character profiles: bindings, resolution, card layers and the player's own; profiles converse and remember on the test fixtures
  - refs: O43, O45, DESIGN§7.7
- f The roster index, once the census has run
  - needs: S9.2.a, e
  - refs: DESIGN§7.8
- g The first roster captures in game, binding by binding [waiting]
  - needs: f
- h The decisions slot and the next-speaker decision, on the CI fixtures; the addressee decisions (O97) and the System One api style (O96)
  - refs: DESIGN§9.7, DESIGN§11.2, ARCH§11.8, WHY§98, WHY§99, O96, O97, WHY§280, WHY§282, NPC§9.2, NPC§5.1, DESIGN§8.1
  - gives: widened on 2026-11-21 by the owner's O96 and O97: the next-speaker decision picks a speaker and an addressee
    together; a decision picks whom the player's line addresses when two or more may answer it; and a provider may take
    the System One protocol for the decisions job, tested against Ollama locally and against OpenRouter with the
    owner's key. Folded into the design the same day (S10.1.j).
- i Stance, unseen attackers and the concealed Tarnished, as spikes S7 and S8 verify their reads
  - refs: DESIGN§6.5, WHY§98
- j The pacing core and the Autonomous class; the pilot's own-initiative lines, his autonomy and subjects, and target rule 0
  - needs: S11.1.b
  - refs: DESIGN§9.4, DESIGN§8.9, O31, O145, O147, NPC§5.2, WHY§420
  - gives: moved from S12.1.c on 2026-11-27 by O147, so that they ship whole: with the speaking-up settings, and
    working in game only once spike S3 and the pilot's S6 have passed.

### S13.3 GraceUI 0.4

- S13.3.a NPC types; Pacing, Reactions, Speaking up, Combat and Awareness; Events; the event monitor
  - refs: G5, PAGES§4.6
- b Home's Happening now and Recent events; settings writable in the browser
  - gives: noted on 2026-11-27: settings writable in the browser are built already, in v0.2. In the fake game's
    pages case a browser changes a setting and settings.toml keeps it (S11.4.a). Left for v0.4: Home's Happening now
    and Recent events.

### S13.4 v0.4's exit checks

- needs: S13.1, S13.2, S13.3
- S13.4.a Barks at three camps, the taste test, the render feed's cost, hygiene and silence for barks and ambient lines
  - refs: DESIGN§16.3
- b The character-profile, subject and player-profile scenarios; profile lints PL1 to PL6, PL8 and PL9 (PL7 retired by O86); every card's scripted record
  - refs: NPC§4.8
- c GraceUI's command validation and the event monitor's rates; testers tune pacing

## S14 v0.5 Veil [planned]

- needs: S13
- refs: DESIGN§16.2

### S14.1 EldenNet v0.5

- S14.1.a The verified flag catalogue, stages, the knowledge base and the full lexicon
  - refs: MEM§5, MEM§6, MEM§7
- b Five more personas, with their news awareness, subjects and test questions; all nine world stages verified or dropped
  - refs: O10, O30, MEM§12
- c Each persona's tracks, ties, roads and bridging stages, truth tables and awareness probes
- d All of spike S6, in game [waiting]

### S14.2 The roster's first batch

- S14.2.a The first batch of bios, reviewed, linted, bound and evaluated
  - needs: S9.2.c, S13.2.g
  - refs: DESIGN§7.8

### S14.3 GraceUI 0.5

- S14.3.a History's World; Named NPCs for every persona and the roster's first bios; Playthrough's story lines and World now
  - refs: G8, GraceUI:D67

### S14.4 v0.5's exit checks

- needs: S14.1, S14.2, S14.3
- S14.4.a Spoilers, voice and world awareness on six personas; release lint on all shipped content
  - refs: DESIGN§16.3
- b Each new bio's roster eval and checklist; the world, roster and Playthrough goldens
  - refs: NPC§12, PAGES§5

## S15 v0.6 Threshold [planned]

- needs: S14
- refs: DESIGN§16.2

### S15.1 EldenNet v0.6

- S15.1.a The first-run wizard, the call-out with repeat calls, and the pad gesture
  - refs: O13, O11, O88, DESIGN§11.5, NPC§9.7, WHY§364, WHY§420
  - gives: since 2026-11-22 also the call-out box as a panel, which S11.4.c leaves to v0.6 (WHY§364 (5)), and target
    rule 6 (NPC §5.2), which the first conversation left out.
- b The compatibility matrix and the patch drill, with the JP build as the "new" one
  - refs: ARCH§12.4, O5
- c The profile editor, the sightings list and the draft helper
  - needs: S7.2.b
  - refs: DESIGN§7.7, WHY§521
  - gives: narrowed on 2026-11-23: the profile editor and the draft helper came early, in v0.2, over the shipped
    personas (S7.2.b). Left for here: the sightings list and the player's own characters it makes (`mine.<id>` rows,
    WHY§521).
- d The rest of the base game's roster bios
  - needs: S9.2.c, S14.2.a

### S15.2 GraceUI 0.6

- S15.2.a The menu's fonts; the menu in game on F5, every page but Workshop; the space switcher and GraceUI's own space
  - refs: G1, G15, GraceUI:DESIGN§10
  - gives: noted on 2026-11-27: much of this is built in GraceUI 0.2. F5's menu shows EldenNet's space since GraceUI
    0.2.0, GraceUI's own space last and a switcher once two mods have spaces (GraceUI's D171), and Workshop is in
    the browser alone (PAGES §4.6.8; GraceUI's D258). Left for 0.6: the menu's fonts and the menu in game for the
    pages v0.6 adds, as GraceUI's DESIGN §10 gives its 0.6 row.
- b Settings, First light, the home-network switch and pairing, the call-out page, and the WIP pages
  - refs: G9, G7

### S15.3 The public test

- S15.3.a Crash safety, unknown builds, cost and multiplayer on the full matrix; the wizard's install; latency on the matrix
  - needs: S15.1, S15.2, S7.6.a
  - refs: DESIGN§16.3
- b GraceUI in game: the close matrix, the Steam Deck and HDR, the ABI and handshake tests, pairing
  - needs: S15.2
  - refs: GraceUI:DESIGN§10
  - gives: the pad and Steam Deck rows run on testers' machines; the owner tests with keyboard and mouse only
    (2026-11-22, WHY§350).

## S16 v1.0 [planned]

- needs: S15
- refs: DESIGN§16.2

### S16.1 EldenNet v1.0

- S16.1.a The 30-hour soak on three machines [waiting]
- b Docs: install, loaders, authoring, patching and the AI disclosure
  - refs: O22
- c A bio for every named NPC and named boss of the base game, every binding verified or the bio held with its reason
  - needs: S15.1.d
  - refs: DESIGN§7.8, O43

### S16.2 GraceUI 1.0

- S16.2.a The accessibility pass, a pad-only walkthrough, the Steam Deck's scale and the full matrix; Named NPCs with the whole roster
  - refs: G6
  - gives: the pad-only walkthrough and the Steam Deck's scale run on testers' machines; the owner tests with keyboard
    and mouse only (2026-11-22, WHY§350).

### S16.3 The release

- needs: S16.1, S16.2
- S16.3.a Every criterion green on the release candidate, EldenNet's and GraceUI's
  - refs: DESIGN§1.3, DESIGN§16.3
- b The owner signs and publishes: GitHub as the canonical source, and a Nexus mirror with the AI disclosure [waiting]
  - needs: a
  - refs: O22

## S17 After v1.0 [planned]

- needs: S16

### S17.1 v1.1

- S17.1.a The expansion's roster, which O46 keeps out of v1.0
  - refs: O46
  - gives: decided 2026-11-16: the base game's roster in v1.0 and the expansion's in v1.1; a bio lands earlier if its
    bindings verify in time (DESIGN §7.8).
- b The gamepad intent list
  - refs: O11
- c English names from a non-English install's own English text, if spike S2 found it there
  - refs: O26
- d The map of the journey: the map panel, the map projection and the Map page
  - needs: S7.9.c, S7.8.c
  - refs: G24, GraceUI:DESIGN§4.13
- e Content plugins: the plugins layer and its checks, the launcher's install step, and the Content packs page's plugin list, with the dashboard's drop zone through GraceUI's upload route (G28)
  - needs: S7.10.c
  - refs: O66, O67, G28, docs/proposals/0001-plugins.md
  - gives: GraceUI's route is built (0.2.26, S11.2.am, 2026-11-23): EldenNet declares `file_commands = ["plugins.install"]`,
    lists it in `sensitive_commands`, puts an upload block on the Plugins page and reads the file through ABI 1.6 (a
    buffer of GRACEUI_UPLOAD_MAX, 32 MiB, or one grown on GRACEUI_FULL), its validator first (GraceUI's
    docs/eldennet-changes.md §61).
- f GraceUI 1.1 opens the framework to other modders: the modders' guide, the three example mods, the harness, the C mock, its own release page and a frozen ABI 1
  - refs: G18, GraceUI:DESIGN§10
  - gives: G18 (a), decided 2026-11-16: after EldenNet v1.0; ABI 1 is a preview until then.

### S17.2 v1.5

- S17.2.a Text-to-speech and speech-to-text, with the Voice pages
  - refs: O8, PAGES§4.6.3, GraceUI:DESIGN§10

### S17.3 Later [idea]

- refs: DESIGN§13
- S17.3.a The director: after a stretch of silence, one NPC within range is handed a subject [idea]
- b Acts and gestures through the seams, with GraceUI's Acts page [idea]
  - refs: O41
- c The typing freeze after spike S5, and the save redirect hook [idea]
  - refs: O9, O6
- d Native talk, native line ids and hook sources: spikes S9 and S10, H1 to H3, the hooks with EldenNet's own library (O65) [idea]
  - refs: DESIGN§16.1, O65
- e Code plugins through EldenNet's published interface, a preview first, in the plugin cluster outside the game or detached, and a hook service for them only if one is needed [idea]
  - refs: O68, O69, O70, docs/proposals/0001-plugins.md
- f A sandbox for plugin hosts, as an option, after a spike on Windows and under Proton [idea]
  - refs: O72
- g Followers: characters who travel with the Tarnished, on the companion layer every bio carries, once actions open [idea]
  - refs: O85, O46
- h The recall gate: candidate memories gathered in code, one picked by a decision call, behind the Recall seam [idea]
  - refs: MEM§4.5, DESIGN§13, WHY§99
