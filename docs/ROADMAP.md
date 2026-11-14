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

- updated: 2026-11-08
- refs: DESIGN§0, DESIGN§16, DESIGN§19, GraceUI:DESIGN§0, GraceUI:DESIGN§10, CLAUDE

EldenNet is an LLM-driven NPC framework for Elden Ring, based on SkyrimNet with its author's
approval; GraceUI is a UI framework for Elden Ring mods, of its own, that every EldenNet release
carries and whose first user is EldenNet's menu and dashboard (G14). This one roadmap covers both.
The design is done and decided, apart from the bio's structure (O46). Two things wait on the owner:
Min's engine blueprint, which becomes EldenNet's foundation and comes before the build, and O46,
which comes before the roster is written. Meanwhile the game's own data (its ids and such) is
researched and documented (O54). Then the engine is adopted, the roster pipeline starts, and
EldenNet's increments v0.1 to v1.0 follow, each shippable to testers, with GraceUI's milestones
beside them. Version 1 stays dialogue-only and read-only towards the game.

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

### S1.5 Working tools and records [done]

- S1.5.a The privacy and safety sweep, git's pre-push gate on every repository [done]
  - refs: tools/privsweep, PSW
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
  - refs: O36
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
- d SkyrimNet's author gives EldenNet the engine blueprint, and EldenNet builds on SkyrimNet [done]
  - refs: O53
  - gives: we may use, copy and adapt SkyrimNet's engine and content, credited as its author
    wishes, with every licence and notice file kept.

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
  - refs: G10, GraceUI:DESIGN§2.9

## S6 Waiting on the owner [waiting]

- needs: S4
- refs: DESIGN§7.8

The two prerequisites the owner set for the engine work and the roster (DESIGN §7.8), and the
research into the game's data that goes ahead before them (O54).

### S6.1 Min's engine blueprint [waiting]

- S6.1.a The owner supplies Min's engine blueprint [waiting]
  - refs: O53, DESIGN§7.8
  - gives: SkyrimNet's game-agnostic core (an LLM layer, TTS, STT, a prompt engine and a context
    engine), EldenNet's foundation from then on.
- b Read it, keep its licence and notice files, and recommend what EldenNet adopts [planned]
  - needs: a
- c The owner decides what EldenNet adopts, with its licence and the owner's terms recorded [waiting]
  - needs: b

### S6.2 The bio's structure [waiting]

- S6.2.a The owner decides O46: the bio's fields, and which NPCs v1.0 covers [waiting]
  - refs: O46, DESIGN§7.8
  - gives: recommended: one bio for every named NPC and named boss, the base game's roster in
    v1.0 and the expansion's in v1.1.

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
- e A dry run: the owner exports from Smithbox and runs `tools/gamedata/smithbox_inspect.py`, which prints counts only [waiting]
  - needs: c
  - refs: O56, ER08, tools/gamedata/README.md
  - gives: whether Smithbox's exports hold what our tools expect, for the owner's game version.
- f DataPiler's design, asked of the PandorumLLM session by the owner, with DataPiler's request 0001 [waiting]
  - needs: d
  - refs: O56, O57, ER08
- g The owner creates the empty public DataPiler repository, so its first commit can be pushed [done]
  - needs: d
  - gives: `Pt0l3my/DataPiler`, created 2026-11-08; its first commit is on its `design/v0` branch.
- h Build DataPiler from that design: launcher and terminal window, the page, the server link, guided exports, jobs, logs [done]
  - needs: f, g
  - refs: O56, ER08
  - gives: DataPiler 0.1 on its `feat/v0.1` branch, tested here on invented data (362 tests, an end-to-end run in a browser).
- i The coverage map of live tracing for every feature, with its eleven gaps (DATA) [done]
  - refs: O57, DATA
  - gives: 105 pieces of live data: 29 backed by research, 65 waiting on a named spike, 11 gaps, none needing a new kind of game read.
- j The owner decides whether to prepare the live checks before the blueprint (O58) [waiting]
  - needs: i
  - refs: O58, DATA
  - gives: recommended: capture plans in our own words and the owner's saves before each named boss fight, nothing built.
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
- n DataPiler 0.2.1: placeholder texts no longer count as names, each layer's names checked on its own, the names rows done once `item_dlc02` arrives, and the 0.2.0 run's smaller fixes [waiting]
  - needs: m
  - refs: O56, ER08
  - gives: offered to the owner; not urgent, as en-0001's counts stand without it.
- o The roster's first ids: DataPiler's jobs en-0002 to en-0004 separate the named characters from the ordinary enemies, and their ids go to O59's research file [done]
  - needs: m
  - refs: O59, O48, ER08
  - gives: name number 0 is the ordinary enemies' default (6,325 of 7,045 rows); 213 real name numbers on 699 rows, in
    `docs/research/eldenring/leads/roster-rows.json`, unverified. DataPiler 0.2.2's job en-0005 adds each name's layer:
    163 names first held by the base game's text and 50 by DLC 1 (the expansion's characters), none new in DLC 2. The 21
    rows whose name numbers hold no text are left out, by the owner's agreement (2026-11-08): the game has no name to show
    for them, so most likely cut or unused characters; revisited if EldenNet meets one in game or with the map work for
    bosses. Open: bosses may take their bar names from event scripts, not NpcParam.
- p The bosses' leads: DataPiler's jobs en-0006 to en-0010 find the bosses' rows and their likely bar names, before EldenNet checks them in game [active]
  - needs: o
  - refs: O43, O59, O48, WHY§36, WHY§46, WHY§47, WHY§48
  - gives: so far the legacy dungeons (en-0007 and en-0008, DataPiler 0.2.10), in
    `docs/research/eldenring/leads/boss-leads.json`, unverified: 141 characters in boss slots of 24 maps, 46 models,
    and the 269 character names no NpcParam row uses. 240 of those names are numbered 90, a model, then three digits,
    and 34 of the 45 non-human models in the boss slots have such names, each in the layer of its model's era: likely
    the bosses' bar names (WHY§47). Next: en-0010 for the smaller dungeons; the open world later, in parts.

## S7 Open items

- needs: S5

Work that can go ahead while S6 waits, and the owner's housekeeping.

### S7.1 GraceUI's changes to EldenNet's design [next]

- refs: GraceUI:docs/eldennet-changes.md
- S7.1.a The main menu on F5 through GraceUI, the bridge (a UiPort in the core) and the overlay [next]
  - refs: G1, G10, DESIGN§12
- b The local server, always on, with pairing for the home network [next]
  - refs: G4, G9, GraceUI:DESIGN§7.2
- c Criteria, presentation and the compatibility matrix's rows [next]
  - refs: ARCH§12.4
- d GraceUI as its own mod in EldenNet's release, and the small fixes found on the way [next]
  - refs: G2
- e The roster's first-meeting field, the roster list and the Playthrough projection [next]
  - refs: G5, G8, DESIGN§7.8

### S7.2 The profile editor pass [next]

- refs: GraceUI:docs/profile-editor-needs.md, O43
- S7.2.a GraceUI revisits the decisions the editor's needs name, with O43 as the reason [next]
- b The editor's pages, fields, validation, commands and its "draft with AI" flow, in GraceUI's design [next]
  - needs: a
  - refs: GraceUI:DESIGN§4.6

### S7.3 Follow-ups from the character-profile design [next]

- refs: DESIGN§7.7, NPC§4.8
- S7.3.a A check-without-saving message, to match GraceUI's profile check [next]
- b Whether merchants count as the player's sightings [next]
- c Whether the unknown-alias error of lint PL3 reveals that an alias exists [next]
- d Hash widths [next]
- e A card's bark word cap against a character's word cap [next]
- f Character keys in the bark pool store [next]

### S7.4 The LLM Jobrunner [waiting]

- S7.4.a The request and the read-only review of version 1.2 go to the session that builds it [waiting]
  - gives: a local job runner for running prompt sets on the owner's own model servers; nothing
    in the reviewed bundle was run.
- b Its next version reviewed, read-only, before EldenNet's prompt sets run on it [planned]
  - needs: a
  - refs: tools/llmworker

### S7.5 GitHub [waiting]

- S7.5.a The owner reviews the design pull requests, EldenNet's and GraceUI's [done]
  - gives: merged by the owner: EldenNet #1 to #3 and GraceUI #1 and #2, the last on 7 November.
- b The temporary branches left by the history rewrite deleted [waiting]
- c GitHub Support asked to purge the closed pull request's ref [waiting]

### S7.6 The public name [waiting]

- S7.6.a The owner picks EldenNet's public name, at any time before the v0.6 public test [waiting]
  - refs: O1
  - gives: a name that trades neither on SkyrimNet's name nor on the game's marks.

### S7.7 An interactive showcase of the menu [planned]

The owner asked on 2026-11-08 for a showcase of the menu to click through in the browser, beside the static
showcase; the Skyrizz session advised on how its own is built. The owner chose to wait for Min's blueprint,
and decided the same day that it is part of EldenNet, not GraceUI (G13's later change).

- S7.7.a The owner decides how it is built (G13) [done]
  - refs: G13, GraceUI:DESIGN§2.5
  - gives: decided 2026-11-08: wait for the blueprint; how to build it is asked again once it has arrived.
- b Build it in EldenNet, test it in a headless browser and publish it privately, linked from the static showcase
  - needs: a, S6.1
  - refs: G13, docs/showcase/showcase.md

### S7.8 GraceUI as a UI framework for any mod

The owner widened GraceUI's scope on 2026-11-08 (G14): a UI framework other modders can use, as
Magelight UI and PrismaUI are for Skyrim; EldenNet requires it and always ships with it, and its
menu and dashboard become GraceUI's first user. The decided design stands until the proposal's
questions are answered.

- S7.8.a A proposal: what the framework changes in GraceUI's and EldenNet's designs, with questions for the owner [done]
  - refs: G14, G1, G10, O25, GraceUI:docs/proposals/0001-ui-framework.md, GraceUI:docs/research/ui-frameworks.md
  - gives: proposal 0001 and the research note FW; the Skyrizz session answered on Skyrim's UI hosts.
- b The owner answers the proposal's questions, G15 to G20 [waiting]
  - needs: a
  - refs: G15, G16, G17, G18, G19, G20
  - gives: G15 to G17 decided 2026-11-08, as recommended (GraceUI owns the overlay; page data and panels; the
    browser view for every mod that opts in); G18 to G20 still wait.
- c Fold the answers into both designs [planned]
  - needs: b
  - refs: GraceUI:docs/eldennet-changes.md

## S8 Adopting the engine [planned]

- needs: S6.1

### S8.1 The blueprint as EldenNet's foundation

- S8.1.a Build and run the engine core outside the game; keep its notices; credit SkyrimNet and Min as Min wishes
  - refs: O53
- b Design what was deferred: memories of other NPCs and their recall, the memory job for NPC-to-NPC exchanges, keepsakes from another NPC's line, context assembly
  - needs: a
  - refs: DESIGN§7.8, MEM§4.5
- c The prompt templates and their engine, the LLM client and its job slots, the TTS and STT seams, the bio's file format
  - needs: a
  - refs: DESIGN§11, DESIGN§13
- d The changes folded into DESIGN, ARCH, NPC and MEM, and into GraceUI's pages that show them, with a review pass
  - needs: b, c
  - gives: the event engine, the game-access layer, identity by verified keys, the spoiler
    layers, the gates, pacing and read-only play stay as designed.
- e Re-base the design on C++ for EldenNet and GraceUI's in-game parts (settled: the blueprint is C++), and choose the C++ overlay in place of hudhook (O25, G15) with the owner's approval
  - needs: a
  - refs: O60, O25, G15

## S9 The roster pipeline [planned]

- needs: S6.2
- refs: DESIGN§7.8, O43, O45, O46

About 140 profiles for the base game, the six personas included: about 170 hours of authoring
and capture on v1.0's critical path (DESIGN §7.8, risk K29). Drafts come from our own notes,
never from game text, and every field is reviewed by hand. Batches go by region or hub, so that
a batch's captures share saves.

### S9.1 The roster's tools

- S9.1.a The roster scan, draft and verify tools, and the lints PL10 to PL12, built as soon as O46 is decided
  - needs: S6.2.a
  - refs: NPC§4.8

### S9.2 Each batch, up to its review

- S9.2.a The census on the author's machine: names stay local, and the index gets ids and numbers only
  - needs: S9.1.a, S8.1.d
  - gives: its leads earlier, from the game-data research (S6.3, O54).
- b A research note per NPC in our own words, with its reveal terms and tells
  - needs: a
- c A draft from each note, every field reviewed by hand, and a second reader's spoiler review at first-meeting level
  - needs: b
  - gives: bound, evaluated and shipped batch by batch: the first in v0.5 (S14.2), the rest of
    the base game in v0.6 (S15.1.d), all of it verified by v1.0 (S16.1.c).

## S10 v0.1 Eyes [planned]

- needs: S8
- refs: DESIGN§16.2

The game-access layer, the event engine and the gates are unchanged whatever the blueprint says
(DESIGN §7.8), but the code starts on the adopted foundation, so v0.1 follows S8. No LLM yet;
GraceUI has no part in v0.1, whose testing uses EldenNet's own F11 debug view.

### S10.1 The spikes for v0.1, in game

- refs: DESIGN§16.1
- S10.1.a The spike and recording build
- b Spikes S0-A to S0-D: the untouched image, the task without dearxan, the launcher and offline play, guarded reads [waiting]
  - needs: a
- c Spikes S1 perception, S2 text, S6's core flags, S7 combat, and S0-E's present-hook half [waiting]
  - needs: a
- d Spike S8 and S3's additions, on v0.1's recordings
  - needs: c
- e What the review left to measure: the fallback copy's cost, hudhook's MinHook in the first build, the play-time width, a Proton path
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

- needs: S10.1, S10.2
- S10.3.a Golden events over three routes; no crash in 10 hours and 20 forced fail-closed launches; the unknown-build banner; the task's cost; dormancy in multiplayer
  - refs: DESIGN§16.3, DESIGN§1.3
- b The read-group self-tests, the matrix's v0.1 rows, and the patch drill if the JP build's layout is ready
  - refs: ARCH§12.4
- c Every CI stage but the eval (from v0.2) gating, and signed builds for testers

## S11 v0.2 First words [planned]

- needs: S10
- refs: DESIGN§16.2, GraceUI:DESIGN§10

### S11.1 The spikes for v0.2

- refs: DESIGN§16.1
- S11.1.a Spike S4, input, and S0-E's input half, in game [waiting]
- b Spike S3, the talk observer, and the pilot persona's S6, in game [waiting]
- c X1, voice in a bottle: candidate models against the prompt, offline
  - refs: tools/llmworker
- d X3, latency in game [waiting]

### S11.2 GraceUI 0.1 Foundations

- S11.2.a Tokens, the protocol, the page descriptions, the web shell, and the fake EldenNet with fixtures
  - refs: G3, GraceUI:DESIGN§9.2
- b For spike S4: one settings page drawn in game through EldenNet's host kit, from graceui.dll's vertex lists
  - needs: a
  - refs: G10, GraceUI:DESIGN§2.9
- c The protocol, lint and contrast tests; the frame cost, the atlas size and the ABI round trip
  - needs: b
- d The owner's look review of the in-game menu, from screenshots [waiting]
  - needs: b
  - refs: G1

### S11.3 EldenNet v0.2

- S11.3.a The LLM client (config file only), the chat box, input capture and gating, the talk observer
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
  - refs: G2, GraceUI:DESIGN§4.6
- b The browser dashboard, always on, on loopback only, with the server's security suite
  - refs: G4, GraceUI:DESIGN§7.2

### S11.5 v0.2's exit checks

- needs: S11.1, S11.3, S11.4
- S11.5.a Latency, no multiplayer requests, output hygiene, silence, spoilers, voice, the config-file install and world awareness, on the pilot
  - refs: DESIGN§16.3
- b The pilot's eval report and release checklist; no input leaks; the spoiler goldens; no secret in any log
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

### S12.2 GraceUI 0.3

- S12.2.a Named NPCs (the pilot) and Playthrough; Conversations and Memories, with Forget
  - refs: GraceUI:DESIGN§4.6
- b Home's Nearby; Speaking up for named NPCs; the top bar's Journey

### S12.3 v0.3's exit checks

- needs: S12.1, S12.2
- S12.3.a Memory; the trim, world trim, NG+ and cue tests; the since-line, claim and cue probes
  - refs: DESIGN§16.3, MEM§12
- b Forget's confirmation and backup; GraceUI's spoiler goldens for memories and journeys

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

### S13.3 GraceUI 0.4

- S13.3.a NPC types; Pacing, Reactions, Speaking up, Combat and Awareness; Events; the event monitor
  - refs: G5, GraceUI:DESIGN§4.6
- b Home's Happening now and Recent events; settings writable in the browser

### S13.4 v0.4's exit checks

- needs: S13.1, S13.2, S13.3
- S13.4.a Barks at three camps, the taste test, the render feed's cost, hygiene and silence for barks and ambient lines
  - refs: DESIGN§16.3
- b The character-profile, subject and player-profile scenarios; profile lints PL1 to PL9; every card's scripted record
  - refs: NPC§4.8
- c GraceUI's command validation and the event monitor's rates; testers tune pacing

## S14 v0.5 Veil [planned]

- needs: S13
- refs: DESIGN§16.2

### S14.1 EldenNet v0.5

- S14.1.a The verified flag catalogue, stages, the knowledge base and the full lexicon
  - refs: MEM§5, MEM§6, MEM§7
- b Five more personas, with their news awareness and subjects; all nine world stages verified or dropped
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
- b Each new bio's roster eval and checklist; GraceUI's world, roster and Playthrough goldens
  - refs: NPC§12

## S15 v0.6 Threshold [planned]

- needs: S14
- refs: DESIGN§16.2

### S15.1 EldenNet v0.6

- S15.1.a The first-run wizard, the call-out with repeat calls, and the pad gesture
  - refs: O13, O11
- b The compatibility matrix and the patch drill, with the JP build as the "new" one
  - refs: ARCH§12.4, O5
- c The profile editor, the sightings list and the draft helper
  - needs: S7.2.b
  - refs: DESIGN§7.7
- d The rest of the base game's roster bios
  - needs: S9.2.c, S14.2.a

### S15.2 GraceUI 0.6

- S15.2.a graceui.dll and its fonts; the menu in game on F5, every page but Workshop
  - refs: G1, G10
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

- S17.1.a The expansion's roster, if O46 keeps it out of v1.0
  - refs: O46
- b The gamepad intent list
  - refs: O11
- c English names from a non-English install's own English text, if spike S2 found it there
  - refs: O26

### S17.2 v1.5

- S17.2.a Text-to-speech and speech-to-text, with GraceUI's Voice pages
  - refs: O8, GraceUI:DESIGN§10

### S17.3 Later [idea]

- refs: DESIGN§13
- S17.3.a The director: after a stretch of silence, one NPC within range is handed a subject [idea]
- b Acts and gestures through the seams, with GraceUI's Acts page [idea]
  - refs: O41
- c The typing freeze after spike S5, and the save redirect hook [idea]
  - refs: O9, O6
- d Native talk, native line ids and hook sources: spikes S9 and S10, H1 to H3 [idea]
  - refs: DESIGN§16.1
