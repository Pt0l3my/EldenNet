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

- updated: 2026-11-16
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

## S6 Waiting on the owner [active]

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
- s The capture plans, in our own words: S6's expectation scripts, the S1 and S2 capture routes and the checkpoint list, the roster batch by batch [next]
  - needs: j
  - refs: O58, DATA, MEM§6.3, DESIGN§7.8, DESIGN§16.1
  - gives: what each live check records, where and when, so the captures start the day the first build runs.
- t The owner keeps a save before each named boss fight and each later form of a roster NPC, on their own PC, never committed and never taken online [done]
  - needs: j
  - refs: O58, DESIGN§7.8
  - gives: dropped on 2026-11-16 by the owner (O58's note): no prepared saves; the in-game checks run during a normal
    playthrough once EldenNet runs in game, and a check that fails waits for its next chance.
- u The gear's names: which text entries hold the weapon and armour names, checked on the owner's data (DataPiler 0.2.40, jobs en-0041 to en-0043) [done]
  - refs: DATA§3.1, DESIGN§4.5, WHY§105
  - gives: 2026-11-16: WeaponName (11) and ProtectorName (12) name 80.0% and 76.4% of the rows by row id, and no row needs the expansion's layers; DESIGN §4.5 lists them, and S2 checks them in game.

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

### S7.2 The profile editor pass [next]

- refs: GraceUI:docs/profile-editor-needs.md, O43
- S7.2.a GraceUI revisits the decisions the editor's needs name (O43), with the needs brought to O46 and O86 [next]
  - refs: O43, O46, O86
  - gives: the needs date from 2026-11-07, before O46 settled the bio the player's own profiles follow (the Tarnished's
    own included) and O86 retired the lints against fixture states (PL7) they assume; SP1's `subjects` came with
    GraceUI's O46 fold on 2026-11-16.
- b The editor's pages, fields, validation, commands and its "draft with AI" flow, in EldenNet's pages (PAGES; GraceUI's design held them until G20) [next]
  - needs: a
  - refs: PAGES§4.6

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
- f The owner's first run of 1.4: a key kept and used on Windows, and a provider's reported costs [waiting]
  - needs: e

### S7.5 GitHub [waiting]

- S7.5.a The owner reviews the design pull requests, EldenNet's and GraceUI's [done]
  - gives: merged by the owner: EldenNet #1 to #3 and GraceUI #1 and #2, the last on 7 November.
- b The temporary branches left by the history rewrite deleted [done]
  - gives: deleted by the owner on 2026-11-16; both pointed into the clean history, so they exposed nothing.
- c GitHub Support asked to purge the refs of the pull requests that still reach the rewritten history [waiting]

### S7.6 The public name [waiting]

- S7.6.a The owner picks EldenNet's public name, at any time before the v0.6 public test [waiting]
  - refs: O1
  - gives: a name that trades neither on SkyrimNet's name nor on the game's marks.

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
    commands, with a one-use ticket, refused from paired devices, nothing kept (GraceUI §7.2 item 18).
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
  - needs: e, S10.1.a
  - refs: O73
- g DataPiler's replay harness: recordings replayed through EldenNet's engine outside the game, with the owner's model services [planned]
  - needs: f, S8
  - refs: O74

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

## S8 Our own engine, on SkyrimNet's logic (O84) [active]

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
v0.1's testing uses, draw through it.

### S10.1 The spikes for v0.1, in game

- refs: DESIGN§16.1
- S10.1.a The spike and recording build, its recordings in numbers that DataPiler imports (O73)
  - refs: O73
- b Spikes S0-A to S0-D: the untouched image, the task without dearxan, the launcher and offline play, guarded reads [waiting]
  - needs: a
- c Spikes S1 perception, S2 text, S6's core flags, S7 combat, and S0-E's present-hook half, on GraceUI's 0.1 Host [waiting]
  - needs: a
- d Spike S8 and S3's additions, on v0.1's recordings
  - needs: c
- e What the review left to measure: the fallback copy's cost, MinHook in the first build (GraceUI's host since G15; with Dear ImGui in place of hudhook since 2026-11-16, O25), the play-time width, a Proton path
  - needs: b, c
  - refs: REVIEW§4.2, REVIEW§4.3

### S10.2 EldenNet v0.1

- S10.2.a The launcher, the DLL load contract, and the offline and session gates
  - refs: DESIGN§3, ARCH§4, ARCH§5, O5
- b The game-access layer: discovery, layouts, guarded reads, the eleven read groups, the degradation ladder
  - refs: DESIGN§4, ARCH§6, O3, O27
- c The game-thread task, the world snapshot, and Augur, the event engine, with golden files
  - refs: DESIGN§5, DESIGN§6, ARCH§8
- d The banner, the debug view, recording and replay
  - refs: DESIGN§14

### S10.3 v0.1's exit checks

- needs: S10.1, S10.2, S10.4
- S10.3.a Golden events over three routes; no crash in 10 hours and 20 forced fail-closed launches; the unknown-build banner; the task's cost; dormancy in multiplayer
  - refs: DESIGN§16.3, DESIGN§1.3
- b The read-group self-tests, the matrix's v0.1 rows, and the patch drill if the JP build's layout is ready
  - refs: ARCH§12.4
- c Every CI stage but the eval (from v0.2) gating, and signed builds for testers
- d One hook library in the process, GraceUI's; EldenNet's imports and symbols free of any; two consumers each way round; a second GraceUI copy idling
  - refs: G15, GraceUI:DESIGN§10

### S10.4 GraceUI 0.1 Host

- refs: G15, GraceUI:DESIGN§2.9, GraceUI:DESIGN§10
- S10.4.a graceui.dll as the host: Dear ImGui 1.93.0's DX12 and Win32 backends with MinHook 1.3.4 (O25 as changed 2026-11-16), Present, the window procedure with the WM_CHAR surrogate pairing, DInput and XInput, capture and masking
  - refs: O25, GraceUI:DESIGN§2.2
- b The anti-cheat check and the notice-only mode, the single-instance guard, and ABI 1 as a preview
  - refs: GraceUI:DESIGN§13
- c Panels, checked draw lists, textures at load, faces and glyph tables, notices, and the player's exit
  - needs: a
- d EldenNet's banner as a notice and its F11 view on a headless Dear ImGui, submitted as a panel; our example consumer in CI
  - needs: c

## S11 v0.2 First words [planned]

- needs: S10
- refs: DESIGN§16.2, GraceUI:DESIGN§10

### S11.1 The spikes for v0.2

- refs: DESIGN§16.1
- S11.1.a Spike S4, input, and S0-E's input half, in game [waiting]
- b Spike S3, the talk observer, and the pilot persona's S6, in game [waiting]
- c X1, voice in a bottle: candidate models against the prompt, offline, with its test questions
  - refs: NPC§12.2, ARCH§12.3
- d X3, latency in game [waiting]

### S11.2 GraceUI 0.2 Foundations (0.1 before G15)

- S11.2.a Tokens, the protocol, the page descriptions, the web shell, and the fake host for any space, with fixtures
  - refs: G3, GraceUI:DESIGN§9.2
- b For spike S4: one settings page drawn in game by GraceUI's host
  - needs: a, S10.4
  - refs: G15, GraceUI:DESIGN§2.9
- c The protocol, lint and contrast tests; the frame cost, the atlas size and the ABI round trip
  - needs: b
- d The owner's look review of the in-game menu, from screenshots [waiting]
  - needs: b
  - refs: G1

### S11.3 EldenNet v0.2

- S11.3.a The LLM client in the DLL (built in S8.1.a and g; config file only), the chat box, input capture and gating, the talk observer
  - refs: DESIGN§8, DESIGN§11
- b The prompt, the sanitizer and lint, pages, and commit on display
  - refs: DESIGN§8.3, DESIGN§8.4, DESIGN§8.5
- c One pilot persona, in memory only, with the world view, what it knows and its road; news raised as O28 sets
  - refs: DESIGN§10.6, O10, O28
- d Positive grounding, the "you, now" line, and settings with the range and the feature switches
  - refs: O36, DESIGN§11.4
- e The eval tool and its CI stage, the fixture states and the pilot's truth tables
  - refs: DESIGN§14

### S11.4 GraceUI 0.2

- S11.4.a The GraceUI folder in EldenNet's release, and the first pages in the browser: Home, AI, System, Tools, Content packs, Docs
  - refs: G2, PAGES§4.6
- b The browser dashboard, always on, on loopback only, with the server's security suite
  - refs: G4, GraceUI:DESIGN§7.2
- c The server in GraceUI with two spaces and per-space routing; EldenNet's subtitles, chat box and call-out box as panels taking input through request_input, with the input-leak tests across holders
  - refs: G17, GraceUI:DESIGN§10

### S11.5 v0.2's exit checks

- needs: S11.1, S11.3, S11.4
- S11.5.a Latency, no multiplayer requests, output hygiene, silence, spoilers, voice, the config-file install and world awareness, on the pilot
  - refs: DESIGN§16.3
- b The pilot's eval report and release checklist; no input leaks; the test questions per character (O86); no secret in any log
  - refs: NPC§12

## S12 v0.3 Remembrance [planned]

- needs: S11
- refs: DESIGN§16.2

### S12.1 EldenNet v0.3

- S12.1.a SQLite: the event log, facts, journal, claims, rollback, NG+ and crash recovery
  - refs: DESIGN§10, MEM§8, MEM§9
- b What each persona has seen of the world, the since-line, and reaction cues
  - refs: MEM§12.13, O28
- c The pacing core and the autonomous class; the pilot's own-initiative lines, once S3 and S6 allow them
  - refs: DESIGN§9.4, DESIGN§8.9, O31
- d The 60-second news cooldown, subjects, persona instructions and the player's profile layers, by file
  - refs: O30, DESIGN§7.7
- e One memory per witness, and the lived layer's update at quiet moments
  - refs: O46, DESIGN§10.3, MEM§13

### S12.2 GraceUI 0.3

- S12.2.a Named NPCs (the pilot) and Playthrough; Conversations and Memories, with Forget
  - refs: PAGES§4.6
- b Home's Nearby; Speaking up for named NPCs; the top bar's Journey

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
- h The decisions slot and the next-speaker decision, on the CI fixtures
  - refs: DESIGN§9.7, DESIGN§11.2, ARCH§11.8, WHY§98, WHY§99
- i Stance, unseen attackers and the concealed Tarnished, as spikes S7 and S8 verify their reads
  - refs: DESIGN§6.5, WHY§98

### S13.3 GraceUI 0.4

- S13.3.a NPC types; Pacing, Reactions, Speaking up, Combat and Awareness; Events; the event monitor
  - refs: G5, PAGES§4.6
- b Home's Happening now and Recent events; settings writable in the browser

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
  - refs: O13, O11, O88, DESIGN§11.5
- b The compatibility matrix and the patch drill, with the JP build as the "new" one
  - refs: ARCH§12.4, O5
- c The profile editor, the sightings list and the draft helper
  - needs: S7.2.b
  - refs: DESIGN§7.7
- d The rest of the base game's roster bios
  - needs: S9.2.c, S14.2.a

### S15.2 GraceUI 0.6

- S15.2.a The menu's fonts; the menu in game on F5, every page but Workshop; the space switcher and GraceUI's own space
  - refs: G1, G15
- b Settings, First light, the home-network switch and pairing, the call-out page, and the WIP pages
  - refs: G9, G7

### S15.3 The public test

- S15.3.a Crash safety, unknown builds, cost and multiplayer on the full matrix; the wizard's install; latency on the matrix
  - needs: S15.1, S15.2, S7.6.a
  - refs: DESIGN§16.3
- b GraceUI in game: the close matrix, the Steam Deck and HDR, the ABI and handshake tests, pairing
  - needs: S15.2
  - refs: GraceUI:DESIGN§10

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
