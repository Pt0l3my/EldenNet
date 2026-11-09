# Owner decisions

The project owner's decisions on EldenNet, binding, one entry each, numbered O1 onwards. This file is the one
record of them: [DESIGN §19](design/DESIGN.md#19-decisions-for-the-project-owner) is its index, and every O-number the
design cites is an entry here. GraceUI's decisions are in GraceUI's own `docs/DECISIONS.md` (G1 onwards); those that
change EldenNet are listed in GraceUI's `docs/eldennet-changes.md` and are recorded here when EldenNet takes them up.

- **Ids.** An id is never renumbered or reused. O1–O46 are the questions DESIGN §19 put to the owner, in its order.
  O47–O53 are owner decisions first recorded in CLAUDE.md (O52 in a rule it has since replaced), which predate §19's
  table or sit outside it; they take the next ids in the order they were made. O54 onwards are later questions.
- **An entry** gives the date and status, the question as it was put (with its options), what was decided, the
  owner's own words where they were recorded, and what it amends. A bare section sign (§6, §7.8) means a section of
  DESIGN.md; ARCH, NPC and MEM are its detail documents, SN and ER the research notes, and the other codes (spikes
  S0–S10, criteria C, risks K) are the design's.
- **Changes.** A later decision may refine, widen or supersede an earlier one, and says so under **Amends**. The
  earlier entry keeps its text as decided, with a dated note under **Later changes**; the question stays as it was
  put. Where a question was re-worded before the owner answered, the entry gives the wording that was answered and
  notes the first one.
- **O1–O24** were the design's recommended defaults; the owner accepted them together on 2026-11-06
  ([process/review.md](design/process/review.md) §4.3), so they record the default as accepted, with no words of the
  owner's own.
- **A new question** takes the next free id: an entry here marked **Open**, with the question, its options and a
  recommended default, and a row in DESIGN §19's index. The owner's answer is recorded in the entry with its date and
  their words, and the index row is updated.
- "The Tarnished" is the player's character.

## By date

- **2026-11-06.** O47–O49 (no me3, no fromsoftware-rs, our own event engine), before the design. O1–O25 and O50 (the
  design's defaults, the review's two rules and the overlay pin). O26, O27 and O51 (game text in another language,
  pattern notes, quest and world awareness). O28 and O29 (news raised unprompted; test everything early).
- **2026-11-07.** O31–O42 (speaking up, pacing, reactions, autonomy, NPC types, greetings, status effects, no actions,
  combat dialogue) and O52 (studying SkyrimNet's dashboard). O30, O43 and O44 (which reactions raise news, character
  profiles, calm lines and sleep), raised by O28 and by O31–O42. O43's update, O45 and O53 (the full roster, free conversation with memories,
  building on SkyrimNet), which superseded O52.
- **2026-11-08.** O54 (no build before the blueprint; the game's data researched and documented now) and O55
  (Smithbox as the source our tools read, under ER08's five conditions). O56 (how far our tools go before the
  blueprint: DataPiler, a tool of its own; a dry run first). O57 (three kinds of game data; live tracing for every feature). O58 put to the owner (preparing the live
  checks before the blueprint). O59 (the game's id leads go to a numbers-only research file). O60 (C++ expected, settled at the blueprint).
- **Open.** O46 (the bio's structure and the v1.0 roster) and O58 (preparing the live checks before the blueprint).

## O1

**Project name** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Project name. **Options:** Keep "EldenNet" / rename before the first public build.

**Decision.** Keep for development; pick the public name before the v0.6 public test. It must not trade on SkyrimNet's name (SN00 §7) or lean on FromSoftware marks. The `en-` crate prefix keeps a rename cheap.

## O2

**Process model** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Process model. **Options:** Monolith / split brain.

**Decision.** Monolith. Revisit only if crash dumps show abort-class faults in core code.

## O3

**Game-access backend** · *2026-11-06 · Decided by the owner: (c)*

**Question.** Game-access backend. **Options:** (a) current fromsoftware-rs after a legal read of its NON-AI licence files; (b) fromsoftware-rs at a pre-2026-10-12 commit (plain MIT/Apache, ≤ `acc9da0`), with the 1.17.2 changes re-derived by us; (c) our own GAL.

**Decision.** (c), own GAL, behind `GameBackend`/`SysLayer`. The seam stays for replay and tests, not for fromsoftware-rs. Recorded in CLAUDE.md. O4 follows from it.

**See also.** O48 (no fromsoftware-rs), the standing ruling behind it.

## O4

**Sources for patterns and offsets** · *2026-11-06 · Decided, following O3*

**Question.** fromsoftware-rs licence path, and sources for patterns and offsets. **Options:** (a) legal read; (b) pre-change code; (c) own code.

**Decision.** (c), own code. Facts from the research are leads only and are confirmed in game or by self-tests. No code or data from fromsoftware-rs or me3 at any commit.

**See also.** O48; O27 is the record behind this clean-room rule.

## O5

**How EldenNet starts** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** How EldenNet starts. **Options:** Own launcher / player-configured me3 native / proxy.

**Decision.** Own launcher as the primary path; me3-native documented for ERSC users; no proxy shipped.

**See also.** O47 (no me3): EldenNet is merely compatible with me3.

## O6

**Save separation** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Save separation. **Options:** None + backups / redirect hook (v1.x) / file swap.

**Decision.** None in v1, with rotating backups and warnings; spike the redirect hook in v1.x.

## O7

**Default LLM provider** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Default LLM provider. **Options:** Cloud budget (own OpenRouter key) / local.

**Decision.** Offer both; suggest cloud under 12 GB of VRAM.

## O8

**Text-to-speech** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** TTS. **Options:** v1 / v1.5.

**Decision.** TTS in v1.5.

## O9

**Typing safety** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Typing safety. **Options:** Gating / opt-in freeze.

**Decision.** Gating only; the freeze becomes a v1.x opt-in after S5.

## O10

**The first content pack** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** First content pack. **Options:** Which personas.

**Decision.** Kalé (pilot), Varré, Patches, Blaidd, Alexander, Boc: early NPCs whose stages are labelled in both flag datasets (ER04 §8). Ranni later, as the spoiler stress test.

**Later changes.** O43, as updated on 2026-11-07, adds our own character profile for every named NPC and every named boss in v1.0, beside these six personas, which keep their stage tracks (§7.8).

## O11

**Gamepad support** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Gamepad support. **Options:** Keyboard only / + Steam keyboard / + intent list.

**Decision.** Keyboard + Steam floating keyboard if S4 passes; intent list in v1.1.

## O12

**Unknown builds** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Unknown builds. **Options:** Banner / opt-in probation.

**Decision.** Banner only.

## O13

**Call-out replies** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Call-out replies. **Options:** On / off / defer.

**Decision.** On, in v0.6.

## O14

**Ambient barks** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Ambient barks. **Options:** Off / sparse / normal.

**Decision.** Sparse.

**Later changes.** Under O31 (pacing only, 2026-11-07) the density became a pacing value: sparse is 2 idle remarks per 60 s window, `pacing.idle_max`'s default (§9.4). O44 (2026-11-07) holds generic NPCs' idle remarks until S8 verifies a sleep or senseless signal for the speaker's kind.

## O15

**The six archetype cards** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Archetype cards. **Options:** Which 6.

**Decision.** Godrick Soldier, Vulgar Militia, Kaiden Sellsword, Raya Lucaria Soldier, Leyndell Soldier, Demi-Human, plus the fallback.

**Later changes.** O43 (2026-11-07) counts the six cards and the fallback as the seven kind profiles.

## O16

**Language** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Language. **Options:** English / follow the game.

**Decision.** English only. Prompts, lines, lint lists and memories are English. Names come from the game's own text tables (§4.5); a game whose text language is not English is O26.

**Later changes.** The sentences after "English only" were added with O26 (2026-11-06), which settles a game whose text language is not English.

## O17

**Content safety** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Content safety. **Options:** Faithful / gentle.

**Decision.** Faithful.

## O18

**The player's name** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Player's name. **Options:** Never / after introduction / always.

**Decision.** After introduction, per persona.

**See also.** O26: a player's name in a non-Latin script never reaches a prompt.

## O19

**Melina, bosses and invaders** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24; updated 2026-11-07*

**Question.** Melina, bosses, invaders. **Options:** Silent / special.

**Decision.** Silent in v1.

**Later changes.** *2026-11-07, by O42 and O43 as first decided:* their own voices stayed out of v1.0's shipped content until v1.1 (O43); O42's combat dialogue applies in their fights to every speaker that has a voice. Under O43 a player's own character profile could give a boss or an invader combat lines in v1 (§7.7), our own boss profiles were to follow in v1.1, and Melina is never bound under any option.

*2026-11-07, by O43 as updated and O45, the same day:* Our own character profiles for every named boss and every named NPC, named invaders among them, ship in v1.0 (O43), so "silent" now covers Melina, who is never bound under any option, and any boss or invader without a profile. A bound boss or invader speaks combat lines in its fights once S3 has recorded its kind (O42), and holds a conversation and keeps memories only where the game lets the Tarnished meet it calm and not hostile (O45); built-in rules (a) and (b) are unchanged, so a bar holder or an invading phantom never converses. A player's own character profile may still give a voice to anyone without one (§7.7).

## O20

**Memories in NG+** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** NG+ memories. **Options:** Hidden / per persona.

**Decision.** Hidden.

## O21

**Telemetry** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Telemetry. **Options:** None / local.

**Decision.** Local only, never uploaded.

## O22

**Distribution** · *2026-11-06 · The design's recommended default, accepted by the owner with O1–O24*

**Question.** Distribution. **Options:** GitHub / Nexus / both.

**Decision.** GitHub as the canonical signed source; Nexus mirror with an AI disclosure.

## O23

**Generated bark pools in v1.0** · *2026-11-06 · Decided by the owner as recommended, with its cut path (process/review.md §4.3, item 10)*

**Question.** Generated bark pools in v1.0. **Options:** Seeds only / seeds + pools.

**Decision.** Seeds + pools, gated by X2 (pools stay off by default if X2 fails). Pools serve variety, not stability. If v0.4 runs late, ship seeds only and move pools to v1.1; requirement 3 is met by seeds alone.

## O24

**Floating bark labels in v1.0** · *2026-11-06 · Decided by the owner as recommended, with its cut path (process/review.md §4.3, item 10)*

**Question.** Floating bark labels in v1.0. **Options:** Feed only / feed + labels.

**Decision.** Feed + labels, labels only once the `camera` group passes. Labels are the one feature that reads game memory every frame (the render feed). If S1 or the soak shows any cost or fault there, ship the feed only.

## O25

**The overlay library: hudhook pinned at 0.9.3** · *2026-11-06 · Decided by the owner*

**Question.** Overlay library. **Options:** hudhook, pinned / a newer hudhook / another hook library / our own DX12 hooks.

**Decision.** hudhook, pinned at exactly 0.9.3 (`=0.9.3`). It is plain MIT, its bundled MinHook is BSD-2, and neither carries an AI clause (checked 2026-11-06). *Rules that follow from the pin (verification pass, 2026-11-06):* only its `dx12` feature is built (`default-features = false`), so its other renderers and its process injector (`inject`) never enter our DLL. `en-overlay` takes `imgui` (0.12) only through the re-export `hudhook::imgui`, MinHook only through hudhook's own FFI module `hudhook::mh`, and `windows` at hudhook's version (0.62), so it adds no copy of its own. hudhook's eject path is never called, because it uninitialises the shared MinHook (R12). `THIRD_PARTY.md` reproduces the notice of every licence file found in a linked dependency's source, among them hudhook's MIT, the BSD-2 notices of MinHook and HDE (`vendor/minhook/LICENSE.txt`) and Dear ImGui's MIT (inside `imgui-sys`); the `licence-files` CI stage fails without them (ARCH §12.1). Any change of version needs the owner's approval and a fresh read of its licence files (ARCH §12.1).

**Recorded in.** CLAUDE.md, Owner decisions ("hudhook pinned at exactly 0.9.3"). GraceUI's G10 keeps the one overlay, and every rule here, in EldenNet.

**Later changes.** 2026-11-08: by GraceUI's G15 (a), decided by the owner, the one overlay moves into GraceUI so that any mod can use GraceUI without EldenNet; the pin and every rule here move with it, unchanged, and EldenNet draws its surfaces through GraceUI and links no hook library. The designs are folded in roadmap S7.8.c (GraceUI's proposal 0001).

## O26

**Game text in another language** · *2026-11-06 · Decided by the owner as recommended*

**Question.** Game text in another language. **Options:** (a) treat a non-English text language as if the `text` group were off; (b) also read English names from the install's own English text, if S2 finds it there; (c) follow the game's language.

**Decision.** (a) in v1, (b) in v1.1 if S2 confirms it. Under (a) the capability matrix applies (§4.4: persona fallback names, card `label_fallback`), so no NPC, card or place name in a prompt comes from non-English text, and the wizard says that English game text gives the full experience. Every option needs S2 to find where the game holds its active text language, and (b) also needs it to find whether the install carries English text; if S2 finds no language field, the wizard asks the player to confirm that the game text is English, and EldenNet otherwise runs as if `text` were off. The S2 row in §16.1 carries that scope. A player's name in a non-Latin script never reaches a prompt or the overlay: `PlayerSaid` is `SafeText` (ARCH R13), whose whitelist keeps Latin letters only (NPC §8.2 step 6), and MEM §3.6 step 2 applies the same whitelist to `told_name`, so such a player stays unnamed to every persona, as before an introduction (O18).

**Amends.** O16: settles a game whose text language is not English.

## O27

**A derivation note per pattern** · *2026-11-06 · Decided by the owner as recommended*

**Question.** Pattern derivation log. **Options:** None / a short note per pattern.

**Decision.** A short note per pattern (what was searched for, in which build, the evidence file), reviewed with each `gal/` change. It is the record behind O4's clean-room rule, which CI cannot fully check because `provenance` is self-declared (ARCH §6.3).

**See also.** O4 and O48, whose clean-room rule this note records.

## O28

**World news raised unprompted** · *2026-11-06 · Decided by the owner; updated 2026-11-07 by O30, O31 and O39*

**Question.** Do personas raise new world news unprompted? **Options:** (a) only when the talk turns to it; (b) once, in the first conversation after the news reaches them, if they have met the Tarnished before in this journey (the cue rides on the since-line), and only for news that touches their own affairs (a stage's `knows`, or a tie with its own certainty); (c) whenever it is new to them.

**Decision.** Yes, but only when a reaction prompts it, never every time. The event reaction system is Augur (§6). W1 keeps world state out of its rules, so the conversation subscriber computes the reaction from what Augur, the flags, the world view and the journal have recorded, with no new event kind, thread or LLM call (MEM §12.13). A persona raises news unasked only in a conversation the player opened, once it and the Tarnished have met before in this journey, and only when a recorded reaction fires (which reactions: O30). Then one cue line in block 7 tells it that the item is on its mind, for the first reply only, and the model may still let it be. There is at most one cue per conversation, and each item is cued at most once per persona per journey. Uncued, world-frame item 9 says to wait for the talk to turn to the news. Greeting the Tarnished with news on approach stays out of v1 (§8.7) as a later option (§1.2, §13). C14 measures both sides: a cued item raised on ≥ 50% of cued controls, and any news on ≤ 10% of uncued controls after `fresh`.

**Owner's words.** "yes, but it shouldn't be 100% of the time, rather a reaction that prompts it. Do we have an event reaction system? When should the NPC bring up news unprompted?"

**Follows from.** O51 (quest and world awareness).

**Later changes.** *2026-11-07, by O31 and O39:* the count limits are gone (one cue per conversation, the first reply only, each item once per persona per journey), and news may be raised in a line the persona starts itself, not only in a conversation the player opened (O30 as re-scoped). At most one cue line renders per reply, a cue's repetition follows the `news` reaction type's cooldown (a player setting, per persona and item), and a cue may also ride on the persona's own greeting, so news on approach is no longer a later option. Uncued, world-frame item 9 now lets the talk turn to the news, where it said to wait for it.

*2026-11-07, by O30:* the `news` cooldown defaults to 60 s of play per persona and item (3 h before).

## O29

**Testing as soon as it makes sense: C14 gates** · *2026-11-06 · Decided by the owner*

**Question.** Does C14 (world awareness) gate releases? **Options:** (a) report only; (b) gate from v0.5; (c) report in v0.5, gate at v1.0.

**Decision.** C14 gates, and every check runs as soon as it makes sense. The rule is in §14 (Schedule, Judge check), and §16.3 applies it to every criterion, test family and spike. Deterministic, measured and zero-tolerance checks gate from the increment their subject ships in; judged rates gate there too, once the judge check passes in that increment. C14 runs on Kalé from v0.2 (its ken-locked side at once, its judged rates after the v0.2 judge check), adds the since-line, claim and cue probes at v0.3, and runs on all six personas from v0.5. Targets are unchanged.

**Owner's words.** "yes we should test everything asap that makes sense."

**Follows from.** O51 (quest and world awareness): C14 is its criterion.

## O30

**Which reactions raise news** · *2026-11-07 · Decided by the owner*

**Question.** Which reactions, besides quest business (O32), let a persona raise news unasked, in a reply or in a line it starts itself? **Options:** (a) own stake only: news new since the last meeting that touches the persona's own affairs (its stage `knows` it, or an active tie with its own certainty covers it); (b) three reactions, chosen per persona by a code-only field `awareness.news = close | plain | eager` (default `plain`): own stake for every persona; the Tarnished's part (the persona holds a proved `slew:<boss>` claim for the item's boss) for `plain` and `eager`; local talk (news new since the last conversation that recorded what the persona knew, from its own ground, with that conversation at most 6 h of play ago) for `eager` only; (c) as (b) without the field: all three for every persona; (d) as (b), plus reactions that need a record v1 does not keep: the Tarnished asked after the item at an earlier meeting (a new fact), a fight at the persona's side against the item's boss (once boss bars map to catalogue aliases), keepsakes matched to an item by their words.

*The question as first put (2026-11-06), with O28, was "Which reactions let a persona raise news unprompted?" It was re-scoped on 2026-11-07, before the owner's answer, to cover lines a persona starts itself as well as replies (O39). Until the answer the design carried (b), the recommendation, as O30's default.*

**Decision.** (b), with the news cooldown at 60 s of play per persona and item instead of 3 h. Kalé and Alexander are `eager`, Boc `close`, and Blaidd, Varré, Patches and the Lantern-Keeper `plain` (MEM §12.13). `reactions.news.cooldown_s` defaults to 60 (bounds 0–21,600, a player setting), and nothing else limits repetition (O31): at most one cue line renders per reply, so an item may be cued again in the same conversation once 60 s have passed; the earlier mention stays in the history, and the model decides. C14's uncued rate and its human sample watch for recital (K19). The owner's other two points are recorded here: autonomy lines may turn to what an NPC likes to talk about, one of its profile's `subjects` (§9.8, §7.7); and a director that breaks a silence by handing an NPC within range a subject is a later seam (§13), with nothing built in v1. Revisit after the first cued-raise report.

**Owner's words.** "approved, no need to be 3 hours cooldown (60 sec is enough), and we can give autonomy to NPCs to talk about what they like in general. In time, we can implement something like SN's Gamemaster, to prompt a random NPC within range with a subject to talk about to break silence."

**Follows from.** O28. **Amends.** O28: its news cooldown is 60 s instead of 3 h. O37: autonomy lines may turn to an NPC's own subjects.

## O31

**Pacing only, no count limits** · *2026-11-07 · Decided by the owner*

**Question.** Do NPCs' unprompted lines have count limits? **Options:** (a) count limits as designed: bark probabilities, once per spawn or per engagement, the squad's first line only, one reaction cue per conversation and each item once per persona per journey; (b) pacing only: a minimum time between lines, a cap within a time window and a cooldown per event type, all player settings.

**Decision.** (b), pacing only. Every count limit goes (NPC §9.1, MEM §12.13). The three rules are a gap (2.5 s between any two lines, 8 s between two lines of one fighting group), a window per scheduler class (6 fight lines, 2 idle remarks and 4 talk lines per 60 s) and a cooldown per event type, keyed by its scope (per speaker, per squad, per persona and item, and so on; §9.4). Replies are outside pacing. A live call reserves its window slot, so model calls never exceed what pacing can show. Context gates are not count limits and stay. O28's count limits are superseded.

**Owner's words.** "I would not restrict how many times an NPC can speak up or react. The only type of restriction I want for now is pacing of dialogue (time between dialogue lines, time window (how many times can NPCs speak within a time window), cooldown per event type)."

**Amends.** O28: its count limits are superseded. O14: "sparse" becomes a pacing value (`pacing.idle_max`).

## O32

**Speaking up for quest business** · *2026-11-07 · Decided by the owner*

**Question.** When does a named NPC speak to the Tarnished unasked? **Options:** (a) never: v1 as designed, with no autonomous greetings; (b) when it has quest business for the Tarnished, the Tarnished is within range, and it is alive and conscious.

**Decision.** (b), widened by O39. The motive is computed from recorded state: an authored stage field, `beckon`, set only where S6 shows that the stage waits on a talk, and no native talk with the persona since the stage became active (`StageChanged` and `NativeTalkEnded`, or the game's own conversation-played flag). The range is `range.talk_m` (8 m by default). Conscious comes from an authored per-stage `state` until S8 verifies a runtime signal; unknown fails closed. The line is one live line in the persona's voice, with an authored seed as its fallback, shown only once S3 has validated the talk observer and never over native talk or a native approach line. EldenNet stays read-only: the line draws the Tarnished in, and the stage moves only through the game's own talk (§8.9; MEM §12.14).

**Owner's words.** "I think the NPCs should want to speak when: they have a quest for the player (either a new one or a new quest stage that requires the player to speak with the NPC to progress), the player is within range (this should also be a setting added now, so NPCs don't trey to talk when player is too far away), the NPC is alive and conscious (not asleep or knocked out)."

**Later changes.** O39 (2026-11-07) widens it: every motive, a greeting included, may start a line, limited only by pacing and allowances.

## O33

**Reaction settings for the player** · *2026-11-07 · Decided by the owner*

**Question.** What can the player set about NPC reactions? **Options:** (a) the per-feature switches and bark density only; (b) per event type, whether NPCs react and its cooldown, plus the range, the pacing values, NPC-to-NPC reactions and self-awareness.

**Decision.** (b). Every motive is a reaction type with `on` and `cooldown_s` in `settings.toml` `[reactions]`, except autonomy, which `autonomy.enabled` and `autonomy.period_s` switch and time (ARCH §11.5; the catalogue in NPC §9.1). The keys ship with each feature, the F10 pages at v0.6. Safety and spoiler layers and the context gates stay non-disableable (ARCH §11.6).

**Owner's words.** "we should add settings for the player to set event types reactions and cooldowns. I think there should be also a setting for NPCs to be able to react to each others dialogue lines and to be aware of their own dialogue (and state)."

**See also.** O34 and O35, the two settings the owner's words name.

## O34

**NPCs reacting to each other's lines** · *2026-11-07 · Decided by the owner*

**Question.** May NPCs react to each other's lines? **Options:** (a) no: NPC-to-NPC talk deferred, as designed; (b) yes, as a setting.

**Decision.** (b), a setting, on by default. Every shown line is an audience-tagged `NpcSaid` event. Its addressee, or else the nearest listener on the speaker's side (on either side in a fight), may answer: generics from response pools keyed to the line they answer, personas with one live line outside a fight. A heard line lives 6 s, less than the 20 s answer cooldown, so exchanges end without a count, and a heard line never spreads news (NPC §9.8; MEM §1.4).

**Owner's words.** "a setting for NPCs to be able to react to each others dialogue lines".

## O35

**NPCs aware of their own lines and state** · *2026-11-07 · Decided by the owner*

**Question.** Are NPCs aware of their own dialogue and state? **Options:** (a) only through a conversation's history; (b) yes, as a setting.

**Decision.** (b), a setting, on by default. A persona's own unprompted lines enter its prompt and its memory; a generic keeps a short ring of its own lines; a "you, now" line gives its wounds, whether it is fighting, and its status effects. Native lines it spoke stay the `NativeLineHeard` seam until S10 (MEM §1.4, §2.6; NPC §7.3).

**Owner's words.** "to be aware of their own dialogue (and state)".

## O36

**Positive grounding instead of forbid lists** · *2026-11-07 · Decided by the owner*

**Question.** How does the prompt keep the model inside what has happened? **Options:** (a) negative prompting, as designed: world-frame item 9's "no word has reached you" and block 7's "Not yours to know" forbid list; (b) positive grounding: tell the model exactly what stage the NPC is at, where related quests stand and the world's stage, and to speak from these facts.

**Decision.** (b); the best solution is left to the design. Block 5b becomes "As things stand now", in three parts (the persona's own road, the affairs tied to it, the land); world-frame items 4 and 9 say to speak from that moment; the forbid line leaves the prompt, and `forbid` becomes code-only data for the evals; a persona's secrets sit in its private self as positive facts. The deterministic guards stay and gain tells; C9 gains a judged part; a fallback ladder is ready if positive grounding alone fails (§10.4; MEM §7, §12.6).

**Owner's words.** "I would like to minimize or avoid negative prompting, perhaps it would be a better approach to allow the framework to see stages and tell the model exactly what stage the NPC itself is in, what related quests are staged at, and the world stage, and then instruct the model to relate to these facts and be aware (otherwise I leave best solution to you)."

**Follows from.** O51: the stages it tells the model about are quest and world awareness's.

**Amends.** The prompt as designed: block 7's "Not yours to know" forbid list and world-frame item 9's "no word has reached you" leave the prompt.

## O37

**Autonomy** · *2026-11-07 · Decided by the owner*

**Question.** May NPCs be prompted to speak on their own, on a timer? **Options:** (a) no; (b) yes: an NPC within range is prompted every period, a setting.

**Decision.** (b). `autonomy.period_s` is 120 s by default. An NPC is prompted only when pacing would let a line through at that moment, and a call reserves its slot even when the answer is silence, so own-initiative calls stay within the talk window: at most 240 an hour at the defaults, about $0.05 an hour at worst on the cloud-budget preset. Personas use their own prompt; generics use `bark_live` with their card. The wizard's free-model presets turn autonomy off (§9.8).

**Owner's words.** "we could also add autonomous NPC behaviour (Autonomy?) so NPC could be prompted every X period (a setting) within range to speak."

**Later changes.** O30 (2026-11-07): autonomy lines may turn to what an NPC likes to talk about, one of its profile's `subjects` (§9.8, §7.7); a director that breaks a silence is a later seam (§13).

## O38

**NPC types and allowances** · *2026-11-07 · Decided by the owner*

**Question.** What can the player set per kind of NPC? **Options:** (a) the per-feature switches only; (b) a page of every NPC type, labelled and tagged, with what each type may do, and a slot of its own for each named NPC.

**Decision.** (b). Types and tags come from our own content file, `identity/types.toml`; each tag rests on a GAL signal that is a lead until its spike passes, and display names come from the player's install at runtime. Allowances cover dialogue (off silences the type entirely, chat and call-out replies included, and so TTS), TTS (in effect from v1.5), greetings, NPC-to-NPC exchanges, autonomy, reactions per event type, combat lines, boss-fight lines, status reactions, news and quest business, self- and status awareness, and actions and gestures as later-version seams (O41). Allowances only narrow: they never give a voice to a type without one, and the safety and spoiler layers stay (§7.6; NPC §1.4).

**Owner's words.** "settings what type of NPC exist and what they are allowed to do, this page would list all types of NPC properly labelled/taged (by race, boss type, etc.) and the user could adjust what each of these NPCs are allowed to do (TTS voice, dialogues (text, if off then that disables TTS as well), actions (including gestures), etc.), special NPCs with names get their own slot, since they are special."

**See also.** O41 (actions and gestures as later-version seams); GraceUI's G5 lists every type from the start.

## O39

**Greetings, to the Tarnished and between NPCs** · *2026-11-07 · Decided by the owner*

**Question.** May NPCs greet the Tarnished or each other to start a dialogue? **Options:** (a) only for quest business (O32); (b) yes, limited only by pacing and allowances.

**Decision.** (b). One model covers every line an NPC starts: each has a motive (a greeting, quest business, news, an event or status reaction, an NPC-to-NPC answer, an autonomy tick); each motive is an event type with its setting and cooldown; allowances say who may act on it; pacing is the only limit; context gates stay. In v1 only personas greet the Tarnished, because every shipped card is hostile; NPCs greet each other within the Tarnished's hearing and may hold short exchanges. This replaces §8.7's former rule of no autonomous greetings (§8.9, §9.7).

**Owner's words.** "and yes NPC should be allowed to greet the player or other NPC to initiate dialogue (only restricted by dialogue pacing, and NPC allowances (what the NPC type is allowed to do)."

**Amends.** O32: widened from quest business to every motive. O28: a news cue may ride on a greeting. §8.7's former rule of no autonomous greetings is replaced.

## O40

**Status effect awareness** · *2026-11-07 · Decided by the owner*

**Question.** Do NPCs know about status effects? **Options:** (a) no; (b) yes: their own and each other's, and they can react.

**Decision.** (b). Seven statuses of our own (frostbite, poison, rot, sleep, hemorrhage, madness, death blight) become event types, each live only once the new spike S8 verifies how the game shows it; until then nothing fires (fail closed). The sleep signal also feeds "conscious". Reactions are spoken only in v1 (O41): a warning, a plea for help, fear or a remark, in words (§9.9; ARCH §6.7).

**Owner's words.** "Can you also add status effect awareness as well? so that NPCs would be aware when they are frozen, bleeding, poisoned, etc. (and would also see each other's status effects and could react (e.g. warn each other, maybe ask for heal, be scared and try to flee, etc.)."

**See also.** O41 (reactions are spoken only) and O44 (calm status lines wait for S8).

## O41

**No actions in v1** · *2026-11-07 · Decided by the owner*

**Question.** Do NPCs act (gestures, fleeing, healing) in v1? **Options:** (a) keep v1 dialogue-only and read-only, with actions in a later version through the seams; (b) gestures only in v1, which needs an animation request, a game call R2 forbids; (c) full actions in v1, with game writes and calls, so R2, R12 and the read-only promise go.

**Decision.** (a). The action and gesture allowances appear on the NPC-type page as seams marked "later version"; status reactions are spoken only; fleeing, healing and gestures come with actions in a later version through the existing seams (§13). R2, the `actions` feature refused in release CI and "no NPC actions beyond dialogue" all stay true.

**Owner's words.** "yes let's go with your recommendation and keep v1 dialogue based and no actions added yet."

## O42

**Combat dialogue for every combatant** · *2026-11-07 · Decided by the owner*

**Question.** Do NPCs speak in fights, boss fights included? **Options:** (a) generic barks against the Tarnished only, none in boss fights, as designed; (b) lines during boss fights as a per-type allowance, off by default; (c) combat dialogue for any NPC engaged in combat, with the Tarnished or with each other, boss fights included, on by default.

**Decision.** (c). The combat motives cover every combatant: engagement and blows between any two actors, either side speaking, and nearby NPCs able to answer. Named NPCs speak combat lines in their own voice from seeds and pools, never live, because a combat line is worth showing only within 1.2–3 s. Lines during boss fights are an allowance, on by default for every type. A combat line fires only when its NPC is not speaking scripted dialogue, read per NPC kind from S3's talk observer and the `NativeLineHeard` seam; a kind not yet verified is held (fail closed), so the default takes effect kind by kind, and kinds with no scripted combat lines need no detector. A line never covers a scripted line or a banner (§9.2, §9.10).

**Owner's words.** "agreed, and yes I want combat dialogue, not only for bosses but any NPCs the are engaged in combat (with player or each other)." and "yes, combat dialogue should be on by default, and should only fire when the NPC isn't speaking scripted dialogue".

**Amends.** The design's generic barks against the Tarnished only, with none in boss fights; O19: combat dialogue applies in their fights to every speaker that has a voice.

## O43

**Character profiles, and the full roster** · *2026-11-07 · Decided by the owner; updated by the owner the same day (the roster)*

**Question.** Character profiles: how does the player instruct one NPC, bosses included, and when do bosses, invaders and other named NPCs get voices of their own? **Options:** (a) the profile system in v1: profiles at three depths (kind profiles, the archetype cards; character profiles, new, for one NPC such as a boss or an invader; personas), each NPC bound only through verified keys; the player's own instructions on any profile, and character profiles of their own for any named NPC, boss or invader they have met (generic NPCs take the player's instructions per kind, through their card's layer), stored locally, validated and never shipped, written in an editor with an optional "draft with AI" helper whose drafts they review; v1.0 ships the six personas and the seven kind profiles (the six cards and the fallback), and our own boss profiles follow in v1.1, each after S3 has recorded that boss and a spoiler review; (b) as (a), with our own profiles for the main bosses in v1.0; (c) profiles for personas and kinds only in v1: the player's instructions on them and no character tier, so bosses, invaders and other named NPCs stay without a voice in v1.

*The question as first put (2026-11-07, with O31–O42) was "Do bosses, invaders and creatures get voices of their own?" Options: (a) not in v1: types with no persona or card (bosses, invaders, creatures, spirit summons, nomadic merchants, uncurated named NPCs) are listed with their allowances at the shipped defaults and marked "no lines in this version"; O42 applies in their fights to every speaker with a voice; boss and invader cards follow in v1.1, each after S3 has recorded its scripted lines; (b) cards for the main bosses in v1.0, in our own words: about one card per boss kind, S3 per boss before its lines play, a spoiler review of each card, and O19 reversed for them; (c) as (b), plus creature cards whose lines are sounds told in words. Recommended then: (a), not in v1. It was re-put in the form above after the owner asked for character profiles (their words below).*

**Decision.** (a). The design carries it (§7.7; NPC §4.5–§4.8). The answer to the owner's question (below) is the profile system of §7.7. A binding is made from what the GAL identifies (an NpcParam row with its name id, an entity id, or a live boss bar's name id), each a lead until S1, S2 or the player's own sighting verifies it, and nothing speaks on an unverified binding; display names come from the player's install at runtime and never ship. Under every option: conversation and memory stay the persona's (a character speaks combat lines from seeds and pools, never live, and, when not hostile, greetings, idle remarks, exchanges, autonomy and call-out replies); a bound boss or invader speaks combat lines only, and only once S3 has recorded its kind (O42), so a player's profile for a kind not yet recorded is saved but held, and the editor says so; shipped profiles pass a spoiler review and the lints, while a player's own profile is theirs (its spoiler checks never block and never point at a term, and the output guards still hold); Melina is never bound (O19). (b) puts a spoiler review and an S3 record per boss on v1.0's critical path, and most of those lines would be held until S3 anyway; (c) leaves the owner's request for bosses unmet in v1.

**Owner's words.** "approved", in answer to the recommendation "build the profile system and editor into v1. v1.0 ships with the six personas and the kind profiles, and you can add your own for anyone right away. Our own boss profiles follow in v1.1, after each boss's spoiler review." Before that, the owner had asked (2026-11-07): "how will you establish that? will you implement character profiles so that we can instruct per NPC (including bosses)?"

**Amends.** O19: Melina is never bound. O15: the six cards and the fallback are the seven kind profiles.

**Later changes.** *2026-11-07, updated by the owner, answering GraceUI's roster questions.* The owner's words: "we should ship character bios for every named NPCs and bosses with names. These are just for important NPC who can have unique charbio. What stage the game is at is irrelevant, this is just so that we ship charbio, an entity for every named NPC which can give them a personality when requested." and "The character bios are shipped full roster from beginning, for stages we will ship a separate feature the Playthrough page." So v1.0 ships our own character profile, holding a bio, for every named NPC and every named boss, beside the six personas and the seven kind profiles; boss profiles no longer wait for v1.1. A bio is general and stage-independent: personality, manner, voice and what the NPC is on a first meeting, in our own words, reviewed for spoilers like shipped content, and nothing about where its story stands; stages stay the personas' (§10.6) and GraceUI's Characters › Playthrough page. Still decided: the three depths, bindings through verified keys only (nothing speaks on an unverified one), the player's layer and own profiles, the editor and the draft helper, S3's record before a boss's lines play, the spoiler review and lints, and Melina never bound. Superseded by O45: conversation and memory are no longer the persona's alone. The bio's structure and the roster's exact scope are O46 (§7.8).

## O44

**Calm lines wait for a verified sleep signal** · *2026-11-07 · Decided by the owner as recommended*

**Question.** May generic NPCs speak calm lines before a sleep signal is verified? **Options:** (a) no: every generic calm line (idle remarks, greetings, exchanges, autonomy) waits for S8 to verify a sleep or senseless signal, as the "alive and conscious" rule asks; combat lines are unaffected before S8: an engaged NPC is almost always awake, and one put to sleep mid-fight is silenced once S8's sleep signal is live; (b) ship idle remarks as O14 decided, without the check, and hold only the new calm motives until S8; (c) treat generics as awake until S8, for every calm line.

**Decision.** (a), as recommended. Every calm line of a card or a character profile (idle remarks, greetings, exchanges outside a fight, autonomy, calm status lines) waits until S8 verifies the sleep or senseless signal for the speaker's kind, and then plays only while the signal reads awake; a character profile counts as generic here, because it has no authored state (§7.7). Combat lines are unaffected, and call-out replies follow §9.6's gates. A soldier asleep by a fire who remarks on the weather would break the owner's rule in plain view. S8 runs on v0.1's recordings, so it very likely lands before v0.4 ships barks; if it fails, the owner may choose (b) then, and v0.4 ships combat lines either way (§9.4, §16.2).

**Owner's words.** "yes", given to the recommendation.

**Amends.** O14: idle remarks wait for S8, like every other calm line.

## O45

**Free conversation and memories for every profiled NPC** · *2026-11-07 · Decided by the owner*

**Question.** Which NPCs hold a free conversation and keep memories in v1.0, and how does the framework find a speaker's data? **Options:** (a) personas only, as designed: a character profile gives short lines, never converses and remembers nothing; (b) every NPC with a profile: named NPCs and named bosses converse and remember, of the Tarnished and of each other, and the framework matches each speaker by ID to its data.

**Decision.** (b). So in v1.0 every named NPC and named boss with a profile can hold a free conversation and keeps memories, of the Tarnished and of other NPCs, made through interactions; events trigger NPCs to talk (Augur, §6); the framework identifies the speaker and matches it by ID to its profile, memories and related data in EldenNet's database (§7.8); the roster's profiles are generated after a game scan and research, which wait for Min's engine blueprint and for the bio's structure (O46). The conversable and safe-context predicates, built-in rules (a) and (b), the context gates, pacing, allowances and the spoiler layers still apply, so a boss or an invader converses only where the game lets the Tarnished meet it calm and not hostile, and most will speak only combat lines in practice. Drafts are generated from our own research notes, never from game text, and every field is reviewed by hand before it ships. Memory, prompts and context are designed here at the level of principle; what Min's blueprint may replace is listed in §7.8 and designed once the owner decides what EldenNet adopts.

**Owner's words.** "We will scan the game and research every applicable NPCs, then we will generate a profile for them. However that will be done after Min supplies the blueprint for his engine, and also once we are settled about the structure of the character biography that every NPC will use. The AI framework will identify the NPC that is selected to speak, then match it by ID with its character profile, memories, and anything else that is related to that NPC from AI framework database. We will need to ship these character profiles with 1.0, and a working memory and event system (events that will trigger the NPCs to talk and memories they make with the player or other NPCs via interactions). So yes 1.0 will give NPCs free conversation with memories."

**Waits for.** Min's engine blueprint (O53) and the bio's structure (O46).

**Amends.** O43: conversation and memory are no longer the persona's alone. O19: a bound boss or invader converses only where the game lets the Tarnished meet it calm and not hostile.

## O46

**The character bio's structure and the v1.0 roster** · *Open since 2026-11-07*

**Question.** What is the structure of the character bio that every named NPC's profile uses, and which NPCs does the v1.0 roster cover? **Options:** (a) **One bio for every named NPC**: the fields in the recommendation, shared by the six personas (their files need no change, since the bio takes the persona schema's names where it has the field) and by every other named NPC and named boss, with a persona's stage track as an optional part that only an NPC with an S6-verified flag block has; (b) two schemas: NPC §4.5's character profile grown with conversation fields, and personas unchanged; (c) a minimal bio (who, manner, cadence and a first-meeting line), with conversation built from it alone.

**Recommendation.** (a), with the base game's roster in v1.0 and the DLC's in v1.1. *Fields and caps* (§7.8): the index (`identity/characters.toml`, code only): `id` (slug ≤ 32, the one key every store uses), `kind` (`named`, `merchant`, `boss`, `invader`; `boss` only for a boss the game never shows calm before its fight, the entries GraceUI tags "Boss" (GraceUI D67); an NPC the game can show calm keeps its calm kind across all its forms), `content` (`base` or `dlc`), `bindings` (an NpcParam row with its `expect_name_id` and `form`; a non-zero entity id; a boss bar's `name_id` with `expect_param`, valid only while that bar shows), each with `verified`; *public* (others' prompts and GraceUI's roster; the Stranger Test at the opening state): `brief` ≤ 24 chars, `public.summary` ≤ 60 words (what a first meeting shows), `public.appearance` ≤ 40 words, `display_name_fallback` ≤ 32 chars (shown only when the install shows no name); *self* (its own prompts only): `identity` ≤ 40 words (required in a roster bio that speaks; a persona's comes from its stages, NPC §2.7), `private.personality` ≤ 80 words, `private.honesty` (truthful, guarded or liar, with ≤ 25 words), `pronouns`; *voice*: `register`, `voice.cadence` ≤ 60 words, `voice.diction` ≤ 40 words, `address_forms` ≤ 3, `reply_words_max` 10–60, `words_max` 6–12, `deflections` 3–5 of ≤ 20 words; *knowledge*: `allegiance` and `home` (≤ 20 words each), `awareness.home` ≤ 3 regions, `knows` ≤ 6 facts true at every state, `knows_when` ≤ 6 entries gated by verified flags, `lexicon` ≤ 8, `subjects` 3–8 (also shown in GraceUI's roster, so held to the Stranger Test at the opening state, PL11), `awareness.news` (`close`, `plain` or `eager`; code only); *lines*: `speech`, `replies`, `ambient`, `base`, seeds (3 per key its kind uses); `instructions`, empty when shipped (the player's layer); *provenance* (code only): `drafted` (`hand` or `assisted`) and `review`. A file stays ≤ 16 KB and renders within the existing block caps. *Never holds:* a stage, fate, later form or reveal; a step or relation the story discloses later; vanilla lines or a close paraphrase of them; its own name in a prompt-facing field (`display_name_fallback`, NPC §3.1, is shown only when the install shows no name, and otherwise fills only NPC §2.7's default self-knowledge line, which a roster bio never uses, since a roster bio that speaks requires its `identity`: a persona's is its pre-reveal name, as today; a roster bio's is our own first-meeting label, such as "the hooded smith", or is left unset, since display names never ship, O43); flags, numbers or aliases in prose; directions or feelings for the model; the word "quest" in any prompt-facing field. *Binding by ID:* a live NPC reaches its bio only through a verified key (§7.2 step 2), and the bio's `id` keys its allowance slot, its journal, facts and claims, its seeds and pools and its roster entry; one id covers every form and reveal row; names never key anything. *The v1.0 roster:* every named NPC (a personal name on its NpcName row: questline characters, services, named merchants and named invaders) and every boss whose bar shows one individual's name, in the base game, about 140 profiles with the six personas; Melina and the mount excepted (O19, rule (f)); a named boss of a voiceless kind (§7.1 F) gets a short silent bio (public fields only), so it is listed and described and never speaks; bosses whose bar shows a kind's name, and nomadic merchants, get no bio and stay mute unless a profile binds them (§7.1). *Why:* one loader, lint set and editor form, and one target for the generator and for whatever Min's blueprint maps; a persona adds its stage track, which needs S6 per NPC, and the fields tied to its story that a bio never holds (NPC §3.1: `private.background`, `private.goals`, `private.relationships`, `knowledge.*`, `awareness.tie`). The DLC waits because its flag research is pre-DLC, verifying its bindings needs saves inside it, and players without it gain nothing; its bios may land earlier if their bindings verify in time. (b) keeps two schemas for one job; (c) is too thin for a free conversation.

**Follows from.** O43 as updated and O45. The owner wants it settled before the profiles are generated (O45).

## O47

**No me3** · *2026-11-06 · Decided by the owner; predates §19*

**Question.** None was put: a standing ruling the owner gave on 2026-11-06, before the design's questions were drawn up, recorded in CLAUDE.md that day.

**Decision.** me3's policy forbids all LLM use in its project. EldenNet uses nothing from me3: no code, crates, APIs or mod-host features, and no presence in its spaces (code, issues, discussions, Discord). EldenNet is a loader-agnostic native DLL with its own launcher, and merely compatible with me3: a player may list it as a native in their own me3 profile.

**Recorded in.** CLAUDE.md, Owner decisions ("No me3"); applied by the design from the start (DESIGN, header).

**See also.** O5 (own launcher; me3-native documented for ERSC users).

## O48

**No fromsoftware-rs** · *2026-11-06 · Decided by the owner; predates §19*

**Question.** None was put: a standing ruling the owner gave on 2026-11-06, before the design's questions were drawn up, recorded in CLAUDE.md that day.

**Decision.** fromsoftware-rs ships NON-AI licence files and comes from the same community. EldenNet has its own game-access layer instead. Write our own code; never copy code from fromsoftware-rs or me3. Any fact about game structures (names, layouts, patterns, IDs) is verified independently, in game or by startup self-tests, before EldenNet relies on it.

**Recorded in.** CLAUDE.md, Owner decisions ("No fromsoftware-rs"); applied by the design from the start (DESIGN, header; §4.7).

**See also.** O3 and O4, which settle the backend and the sources in the design's terms; O27.

## O49

**Our own event engine** · *2026-11-06 · Decided by the owner; predates §19*

**Question.** None was put: a standing ruling the owner gave on 2026-11-06, before the design's questions were drawn up, recorded in CLAUDE.md that day.

**Decision.** Elden Ring has no event bus. EldenNet detects events itself (snapshot diffs, an event-flag watcher, later hook-based sources behind the same interface).

**Recorded in.** CLAUDE.md, Owner decisions ("Own event engine"); the design's Augur (§6).

**See also.** O28 and O45 build on Augur; §7.8 keeps Augur and its event model whatever Min's blueprint brings (O53).

## O50

**The review's two rules: the title-screen clause and sheltered places** · *2026-11-06 · Accepted by the owner with O1–O24; outside §19's table*

**Question.** Put to the owner with O1–O24, as the two rules the adversarial review adopted (process/review.md §4.3, item 11): the title-screen gate clause (review §3, item 1) and the sheltered-context definition (review §3, item 15).

**Decision.** Both rules stand. *The title clause* (§3.4): at the title screen there is no `PlayerGameData`, so no role can be read; there the gate is clean only if the lobby is `None`, no anti-cheat module is loaded and the `session` group is healthy, and the clause permits network calls for the wizard and the silent self-test (§11.5) and nothing else: speech stays dormant until a world is loaded (ARCH §5.1). *Sheltered places* (§8.1): they waive the two hostile-distance checks of the safe context, never the boss and combat checks, and let the nearest-persona rule reach 8 m instead of 5 m: the Roundtable and resting at a grace (once S6 verifies its flag).

**Recorded in.** CLAUDE.md, Owner decisions ("Design defaults accepted"); process/review.md §4.3 ("both rules in item 11 stand"); DESIGN §18 (the title-screen gate and sheltered-context rows).

## O51

**Quest and world awareness** · *2026-11-06 · Decided by the owner (a requirement); outside §19's table*

**Question.** None was put: a requirement the owner set on 2026-11-06, recorded in CLAUDE.md that day.

**Decision.** Important NPCs know the state of their own quest and of related quests, and the stage the world is in (for example, that the star has fallen since Radahn's death).

**Recorded in.** CLAUDE.md, Owner decisions ("Quest and world awareness"); DESIGN §10.6 ("Owner requirement (2026-11-06)") and MEM §12 carry the design.

**Led to.** O28, O29 and O30 (news raised unprompted, C14 gating, which reactions); O36 (positive grounding) and O43 as updated (bios stay stage-independent; stages stay the personas' and GraceUI's Playthrough page's) build on it.

## O52

**Studying SkyrimNet's dashboard in a sandbox** · *2026-11-07 · Decided at the owner's request; superseded by O53 the same day*

**Question.** None was put: an exception the owner asked for, for GraceUI's research, recorded in CLAUDE.md on 2026-11-07, when SkyrimNet could still be used for ideas, architecture and published numbers only.

**Decision.** One exception, at the owner's request: SkyrimNet's dashboard's pre-built web bundle may be run to study the UI's structure, served on loopback in a headless browser with every outside request blocked. Only what the browser shows is looked at; the bundle's source is never read, searched or copied.

**Recorded in.** CLAUDE.md, Licences (until O53 replaced it); GraceUI's research note on SkyrimNet's dashboard (SND) was made under it.

**Later changes.** *2026-11-07, by O53:* superseded; SkyrimNet's engine and content, its dashboard included, may now be used, copied and adapted.

## O53

**SkyrimNet: building on Min's engine blueprint** · *2026-11-07 · Decided by the owner; outside §19's table*

**Question.** None was put: the owner's decision of 2026-11-07, recorded in CLAUDE.md that day, after SkyrimNet's author, Min, offered the engine blueprint.

**Decision.** SkyrimNet's author, Min, gives EldenNet the engine blueprint (SkyrimNet's game-agnostic core) to work with, knowing EldenNet is built with AI assistance, and EldenNet's foundation will be based on SkyrimNet. So we may use, copy and adapt SkyrimNet's engine and content (code, prompts, templates, bios, configuration, the dashboard) for Elden Ring. Credit SkyrimNet and Min as Min wishes, keep every licence and notice file that comes with what we take, and never present EldenNet as SkyrimNet itself or claim more endorsement than Min has given. Once the owner supplies the blueprint it is our foundation: we read, build, run and modify it. The older copies under `_untrusted` stay read-only reference. What EldenNet adopts from Min's engine is the owner's call once its blueprint arrives; nothing of the engine core is adopted until its licence and the owner's terms are recorded (§7.8).

**Recorded in.** CLAUDE.md, Licences ("SkyrimNet (owner's decision, 2026-11-07)") and Owner decisions ("The full roster"); DESIGN header (licence hygiene) and §7.8 ("Deferred until Min's blueprint"). GraceUI's CLAUDE.md applies it to GraceUI (GraceUI G12).

**Supersedes.** The earlier rule that SkyrimNet gives ideas, architecture and published numbers only (SN00 §7, and CLAUDE.md until this decision on 2026-11-07), and O52's sandbox-only exception.

**Waiting.** The blueprint itself; then the owner's decision on what EldenNet adopts, which O45's roster and O46's bio wait for.

## O54

**An early, numbers-only roster census** · *Put 2026-11-07 · Decided by the owner 2026-11-08*

**Question.** May `en-tools roster-scan` (with the parts of `identity-fill` and `fmg-import` it needs) be built, and the numbers-only census run on the author's machine (names kept there, the index getting ids and numbers only), before Min's blueprint arrives and before O46 is decided? **Options:** (a) wait for both prerequisites, as DESIGN §7.8 plans; (b) build those tools and run the numbers-only census now, since it depends on neither.

**Recommendation.** (b): it settles the roster's numbers and leads early, against K29's schedule risk, and commits nothing of the bio's structure.

**Decision.** Neither as put. Nothing of EldenNet is built before Min's blueprint arrives, `roster-scan` and the tools beside it included; the game's own data (its ids and such) is researched and documented now, as standalone research for EldenNet, usable whatever the blueprint brings. Where that data comes from (an online database, a modding tool's exports, or a Python tool of our own reading the owner's install) is researched first (ER08) and then put to the owner as O55. The owner's words: "Can we carry out this research as a standalone documentation for EldenNet? What I mean is that there is no point to build yet until we get the Blueprint engine, but we could gather data about the game itself to get data about IDs and such. That is usable information regardless. Is there a database online that we could access for what we need? what source should we use? should we extract from game files with a tool like DSmapstudio (perhaps we could write a python tool to work with it that could easily extract info for us)?"

**Follows from.** DESIGN §7.8 (Schedule risk) and K29.

## O55

**Smithbox as the source our game-data tools read** · *2026-11-08 · Decided by the owner*

**Question.** None was put as such: while O54's research was asking where the game's data should come from (an online database, a modding tool such as DSMapStudio, or a Python tool of our own; the owner had asked "should we extract from game files with a tool like DSmapstudio (perhaps we could write a python tool to work with it that could easily extract info for us)?"), the owner chose.

**Decision.** Smithbox (the maintained successor of DSMapStudio) is the source our tools read: the owner exports from their own install with it, and our own tools (`fmg-import`, `identity-fill`, `roster-scan`, ARCH §12.3, and any research tool written before them) read those exports on the owner's PC, names kept there and only numbers and ids leaving it. Other sources serve only as leads to cross-check. Adopting it waits for its licence and any AI clause or policy to be read (CLAUDE.md: check a dependency's licence before adopting it), in ER08. The owner's words: "also about Smithbox, let's use that for our tools."

**Follows from.** O54; O48 (facts verified independently before EldenNet relies on them); ARCH §12.3.

**Later changes.** 2026-11-08, the same day: ER08 §3.1 read Smithbox's licence and stance. It is plain MIT with no AI clause, and openly built with an AI coding agent, so nothing stops adopting it; it is adopted under five conditions (ER08's TL;DR): EldenNet never bundles, mirrors or redistributes Smithbox, whose repository carries the game's own regulation files, and our tools read only exports from the owner's own install; our tools take numbers only from that data, never Smithbox's names, annotations or layouts as facts (O48); none of its me3 features is used (O47); its loopback server, which can write into the running game, is switched off in its `Startup.json` unless a read-only client of ours uses it; and the GPL duties of the SoulsFormats it ships fall on whoever redistributes it, not on reading its exports. How far our tools go before the blueprint is O56.

## O56

**How far our game-data tools go before the blueprint** · *Put and decided by the owner 2026-11-08*

**Question.** With Smithbox as the source (O55), how far should our tools go before Min's blueprint arrives? (O54 builds nothing of EldenNet before then; O55 allows "any research tool written before them".) **Options** (ER08 §6): (a) **document only**: ER08 and a census procedure in our own words, no script until `roster-scan` is built after the blueprint; (b) **a small research script** on Smithbox's param and text exports (about 3–5 days of work): standalone, outside `en-tools`, Python's standard library only, run by the owner on their PC; it writes the census with names into the git-ignored `roster.local/` and a numbers-only lead file (NpcParam rows per form, `nameId`s, talk ids as metadata) with its provenance; entity ids, model ids, boss bars and flag blocks wait for the in-game captures S1, S2 and S6, with public leads meanwhile; `roster-scan` replaces it later; (c) as (b), plus a read-only client of Smithbox's loopback server for map placements (entity ids, models): 2–4 more days, two more libraries, and a server that can also write into the running game; (d) as (c), plus readers of our own for talk and event scripts, or reading the install directly (about 20–30 days, and the game's compression library and keys on EldenNet).

**Recommendation.** (b), with ER08 §7's recommendations as its defaults: no tool of ours decodes the game's compression or decrypts its files; only the ids a feature binds are committed, in a numbers-only research file under `docs/research/` after the privacy sweep and a numbers-only check, until `identity/characters.toml` takes them; the expansion's numbers are gathered in the same pass, marked as such; neither fstools-rs nor SoulsRandomizers is used, even as leads; slugs come from the name a first meeting shows. *Why:* it gives the game's ids now, usable whatever the blueprint brings, for a week's work at most, with no keys and no binary parsing, and keeps Smithbox at arm's length (its files in, numbers out). What (c) and (d) add are numbers the design captures and verifies in game anyway. *What it gives up:* entity, model and boss-bar leads before the captures.

**Follows from.** O54, O55, O48; ER08 §6 and §7; DESIGN §7.8.

**Discussion.** 2026-11-08: asked to choose, the owner asked first to be shown the options: "let's talk about this first, what is the cleanest and problem free method to extract game data, what game data do we need right now, do we know well enough Smithbox, what kind of extracted data do we need (that's relevant to your questions that you offer, I don't know well enough to answer, so educate me first, explain various options and their benefits." The explanation was given in the conversation; the question stays Open.

**Discussion.** 2026-11-08, after the explanation: the owner chose a dry run first. "good idea, let's do that first so we can test what we can extract from Smithbox." `tools/gamedata/smithbox_inspect.py` reads the owner's own Smithbox exports (NpcParam, TalkParam, the item text containers) on their PC and prints counts and checks only, never a name or other game text; the owner pastes its report into the conversation, and O56 is decided from it.

**Decision.** Beyond the options as put: a tool of its own, **DataPiler**, in a public repository of its own. Started from an executable, it opens in the browser as a tab with all its tools: Smithbox's status, extraction by data type or bundle, a drop zone for job files the assistant writes (it runs the job and writes a file the owner shares back), a PowerShell terminal window and logging. Its buttons fetch data through Smithbox's local server with read calls only, while the game is closed (the server-link part of option (c)), with guided exports for what the server does not serve, such as the game's text; its design is asked of the PandorumLLM session. DataPiler's own decisions DP1 to DP5, in its `docs/DECISIONS.md`, record the details and the owner's words. The owner's words here: "can you make this Smithbox plugin to come with established repo, executable to run, http UI, and interactive button that run tools. [...] for the design you could ask PandorumLLM agent (write request)." The dry run stays available meanwhile.

## O57

**Three kinds of game data, and live tracing for every feature** · *2026-11-08 · Decided by the owner*

**Question.** None was put: the owner's direction of 2026-11-08, given while O56 was discussed.

**Decision.** EldenNet keeps three kinds of game data apart: (1) **research extraction**: offline, on the owner's PC, from the owner's own install (Smithbox's exports, O55 and O56), giving documentation and leads, never shipped; (2) **the framework's data**: the data files EldenNet builds and ships for its functions (the identity bindings and the roster index, the flag catalogue, regions, NPC types), made from those leads and verified (O48) before EldenNet relies on them; (3) **live tracing during play**: what EldenNet's game-access layer and event engine read and detect while the game runs (the characters near the player, flags, boss bars, places, combat and status), on which every feature depends. How the data is got does not matter, so long as everything needed is got; what must hold is that EldenNet can trace and monitor events, NPCs and data during play so that every feature works. A coverage map checks it: each feature, the live data it needs, how EldenNet reads it in game, how that is verified, and the gaps.

**Owner's words.** "Also, we need to make clear distinction between data extraction for research and general data collection for the AI framework functions, and in-game live event and data tracing/monitoring for the AI framework in action (during gameplay). I don't mind how we get the data as long as we get what we need, we just need to ensure that EldenNet will be able to track/trace/monitor events, NPCs, data during gameplay so that all features work."

**Follows from.** O45, O48, O49, O51, O54, O55, O56.

## O58

**Preparing the live checks before the blueprint** · *Open since 2026-11-08*

**Question.** Live tracing is verified only in game (DATA §1.4), and every capture waits for the first build, which follows Min's blueprint (DATA §3.2). Should anything be prepared now, without building EldenNet? **Options** (DATA §4): (a) **wait**: prepare nothing until the blueprint, so the capture plans and saves are made when v0.1's spikes start; (b) **prepare now, without building**: write the capture plans in our own words (S6's expectation scripts, the S1 and S2 capture routes and the checkpoint list, the roster batch by batch), and the owner keeps a save before each named boss fight and each later form of a roster NPC as they play (about 40–60 for S2 alone), copies on their own PC, never committed, kept apart from any save they take online; (c) as (b), plus a standalone read-only spike build (the game-access layer, the recorder and the debug view, no model and no network) before the blueprint, so the in-game spikes can run early.

**Recommendation.** (b). It builds nothing, so O54 holds; it serves whatever the blueprint brings, since the game-access layer, Augur and identity by verified keys are unchanged by it (DESIGN §7.8); and it takes the slowest part of the captures, preparing the saves, off the critical path (K29). *What it gives up:* some of the owner's play time, and plans that may need revising once the blueprint is known. (c) would find a failing live source sooner, but with code written before its foundation is known; it stays a later choice if the blueprint is long delayed.

**Follows from.** O54, O57; DATA §3 and §4; DESIGN §7.8, §16.1.

## O59

**Where the game's id leads go** · *2026-11-08 · Decided by the owner, as recommended*

**Question.** ER08 §7 item 3 left open how much of an id index may be committed, and where. With DataPiler sharing ids only for rows a job picks for a purpose (DataPiler's DP8), where do the leads the owner shares go? **Options:** (a) **a research file**: a numbers-only file under `docs/research/`, each lead marked unverified, with DataPiler's provenance block, replaced by EldenNet's own identity data (`identity/characters.toml`) once EldenNet runs and checks them in game; (b) **not committed yet**: the leads stay in the conversation and the owner's share folder until EldenNet is built.

**Decision.** (a). Only the ids a feature binds (each named character's row ids per form and name number, and later entity ids, boss-bar name ids and flag block bases) are committed, in a numbers-only research file under `docs/research/`, each with `verified = false` and the share file's provenance, after the privacy sweep and a numbers-only check; EldenNet's own identity data replaces it once the leads are checked in game (O48).

**Owner's words.** "A research file (Recommended)".

**Follows from.** O54–O57; ER08 §5 and §7 item 3; DataPiler's DP8.

## O60

**The implementation language: C++ expected, settled at the blueprint** · *2026-11-08 · Decided by the owner (direction); confirmed the same day*

**Question.** GitHub labels EldenNet a Python project, since its only code so far is its Python tools. The design says one Rust `cdylib` (DESIGN §2.1), chosen on 2026-11-06 because research found Rust's `fromsoftware-rs` the only maintained typed SDK for the game and hudhook a proven Rust overlay (ER02, ER00); O47 and O48 then excluded fromsoftware-rs and me3, and O53 made SkyrimNet, a C++ SKSE plugin, EldenNet's foundation. Should EldenNet and GraceUI be written in C++? **Options:** decide now, or when Min's blueprint arrives, as part of adopting it (roadmap S8). **Recommendation:** decide at the blueprint; C++ if the blueprint is C++, which is likely.

**Decision.** Wait for the blueprint, with C++ as the expected route for both EldenNet and GraceUI's in-game parts. Python stays for the tools (the sweep, `wp`, the page builders, DataPiler), and GraceUI's browser view stays a web app (G3). When the blueprint is adopted, the design is re-based on C++: the game-access layer, the event engine and the rest of the core, and the overlay, where O25's hudhook (Rust) gives way to a C++ overlay, a change to O25 and to GraceUI's G15 that the owner approves then.

**Owner's words.** "We should wait for the blueprint, but it sounds like C++ would be the right route either way, for both EldenNet and GraceUI."

**Follows from.** O25, O47, O48, O53; GraceUI G3, G15; DESIGN §2.1; ER00, ER02.

**Later changes.** 2026-11-08, the same day: asked on SkyrimNet's Discord whether the blueprint engine is in C++, Min answered: "Yep! It's C++". So the language is settled: EldenNet and GraceUI's in-game parts are written in C++, and the design is re-based on C++ when the blueprint is adopted (roadmap S8.1.e). The overlay that replaces hudhook still needs the owner's approval when it is chosen, as a change to O25 and G15.

## O61

**Which kinds of creature and enemy may speak** · *2026-11-09 · Decided by the owner, as recommended*

**Question.** DESIGN §7.1 gives generic humanoids an archetype card (barks and a one-line call-out reply) and keeps the
voiceless kinds mute. Which of the kinds that never speak in game, and are not plainly human, may speak by the game's
lore? **Options:** (a) **the evidence rule**: a kind may speak only if the game itself shows one of its people speaking,
and every other kind stays mute; (b) humans and human-like peoples only; (c) anything with a mind, trolls, Misbegotten,
giants and dragons included.

**Decision.** (a). The kinds it lets speak, each a lead to check before its archetype card is written (O48): demi-humans
(a demi-human speaks in game), living jars (a warrior jar speaks), the beastmen of Farum Azula (a beast clergyman
speaks), man-serpents (transformed people, and their lord speaks as a serpent), albinaurics, Omens and the expansion's
Hornsent. Each speaks in a manner its archetype card sets (crude, few words, archaic and so on), never in another
people's voice. With no speaker shown in game, Misbegotten, trolls, giants, dragons and the Kindred of Rot stay mute, as
do constructs, the undead, spirits and beasts. A humanoid silent by its own nature or vow stays silent. A new speaker
found in the game's own content can move a kind across, by a dated note here.

**Dragons and languages.** Asked whether dragons speak a tongue of their own, as in Skyrim: the game shows no dragon
language and no dragon speaking (its dragon powers come by Dragon Communion, not by words), and no other spoken language
than the one common tongue its peoples share, so dragons stay mute and EldenNet invents no language. DESIGN §1.2's
later-version seam, "creature voices told in words", is where a dragon's roar could be described.

**Owner's words.** "I like your logic, so I would go with choice "a"".

**Follows from.** DESIGN §7.1 (tiers E and F), §7.5; O48; the positive grounding of spoilers (2026-11-07).

## O62

**Voices from the game's own voice recordings** · *2026-11-09 · Decided by the owner*

**Question.** DESIGN §1.2 listed cloning the cast's voices under Never, following ER03's advice (generic TTS voices per
archetype, for the legal and ethical risk of cloning the original voice actors). Asked which NPCs have voice samples,
what can serve which, and which wanted speakers have none, the options put were: (a) keep Never, with generic and
designed voices; (b) voices cloned on the player's PC from their own install, as Skyrim mods do; (c) licensed voices.
The recommendation was (a).

**Decision.** (b), as SkyrimNet does it: EldenNet's TTS (v1.5, O8) generates speech from the game's own voice
recordings, each character in its own voice where the game voices it (SkyrimNet keys its voices by Skyrim's voice types
and clones them from the game's own `.fuz` files, SN00 row 34). What follows from the rules that stand:

- The samples come from the player's own install, on the player's PC (the voice banks in `Game/sd`, ER03), and stay
  there, as game text does. EldenNet never commits, ships or uploads game audio, samples or voice models.
- `voice_key` resolves the character's own game voice first (NPC §10.8). Which game voice serves a speaker the game
  never voices (generic humanoids, O61's kinds, silent bosses) is settled in v1.5's design, by archetype, body type and
  age band.
- What Min's engine blueprint brings for TTS is adopted then (O53).
- The mod page states that the voices are AI-generated from the player's own game files, as Nexus Mods' AI disclosure
  asks.

This supersedes DESIGN §1.2's Never entry and ER03's voice-design note.

**Owner's words.** "we will absolutely use the game provided voice samples and generate based on those with TTS, just like SkyrimNet does"

**Follows from.** O8, O53, O61; DESIGN §1.2; ER03; SN00 row 34.

