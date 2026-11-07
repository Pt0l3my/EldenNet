# J3: judge scorecard, player experience and AI quality

> **What this is:** one judge's scorecard of the three v1 proposals
> ([P1 lean monolith](P1-lean-monolith.md), [P2 split brain](P2-split-brain.md),
> [P3 experience first](P3-experience-first.md)). All three were read in full, against the research summaries
> (`SN00`, `ER00`) and the detail notes they cite. **Date:** 2026-11-06.
>
> **Lens:** what the player sees, reads and feels:
> - how a conversation feels and how long it takes;
> - bark quality, density and timeliness;
> - FromSoftware tone and each NPC's register;
> - lore grounding, and spoiler control through flags;
> - whether memory is useful;
> - identity correctness (reveals such as Renna → Ranni);
> - typing while the game keeps running, and gamepad players;
> - how well it suits small local models, and what it costs;
> - what the player sees when something fails.
>
> **The owner update of 2026-11-06 applies throughout:** no me3 dependency, our own Game Access Layer instead
> of fromsoftware-rs, and a first-class event engine. All three proposals were written before it. §4 lists
> only the parts of the update a player would notice. The engineering replacement is the lead architect's job
> (J2 §2 has the full inventory).
>
> **Citations:** `P1 §x` is a section of a proposal. `ER0n §x` and `SN0n §x` are research notes. **Judge view**
> marks this scorecard's own recommendation, not a research fact.

---

## 1. Sub-criteria (each scored 1–10)

| Code | Sub-criterion | What earns a high score |
|---|---|---|
| **A** | **Conversation feel** | **Latency:** a time budget for each moment the player can see, and the wait hidden in a way that fits the genre. **Pacing:** pages held long enough to read, with no typewriter effect. **Turn handling:** interrupting, queueing, and committing only what was shown. **Targeting:** the player can find who they can talk to. **Typing in a running world:** safe gating, an exit when danger appears, the draft kept. **Gamepad:** a real path for pad players. **Native talk:** never talking over the game's own dialogue |
| **B** | **Barks** | **Timeliness:** a line lands on the moment or not at all. **Density:** rare beats constant. **Awareness:** what they are, where they are, what is happening in the fight. **Tone:** hostile, chant-like, amnesiac. **Variety:** no repeats. **Coverage:** a fresh install, a free tier and a local GPU all still bark. **Address:** a generic NPC answers when the player calls out to it |
| **C** | **Voice, tone and grounding** | **Register:** FromSoftware's register per NPC, written as cadence rather than effect words. **Mechanics:** owned tics, openers, banned modern words and length caps enforced in code. **Lore:** lore comes only from the NPC's knowledge, and the NPC deflects in character when it does not know. **Spoilers:** gated by flags in layers. **Identity:** reveals happen correctly in both the overlay name and the NPC's self-knowledge. **Measured:** all of this is checked in an eval |
| **D** | **Memory usefulness** | **Callbacks:** the NPC recalls specific things that matter, not just a vague summary. **Sources:** it keeps *witnessed*, *told* and *known* apart, and claims stay claims. **Audience:** only what the NPC could have perceived. **Continuity:** survives reloads, rollbacks and NG+. **Control:** the player can inspect it and clear it |
| **E** | **Robustness as the player sees it** | **Failures:** honest and in-genre, never a fake answer. **Small models:** works on a small local model. **Cost:** cheap, and survives free-tier limits. **First run:** the setup and self-test tell the player in plain words what will work |

---

## 2. Scores at a glance

| Proposal | A Conversation | B Barks | C Voice and grounding | D Memory | E Robustness as felt | **Total /50** |
|---|---|---|---|---|---|---|
| P1 Lean monolith | 7 | 6 | 7 | 8 | 7 | **35** |
| P2 Split brain | 7 | 5 | 6 | 6 | 6 | **30** |
| P3 Experience first | 8 | 9 | 9 | 8 | 8 | **42** |

**Winner for this lens: P3.** It is the only proposal written from the player's moment-to-moment experience, and that shows in the details:
- latency budgets for each visible moment;
- page pacing;
- a bark path that never waits for the network;
- tone rules that code enforces and an eval measures;
- identity handled in the prompt as well as in the overlay.

P1 has the best memory retrieval, the best claim verification and the best environment-aware ambient pools. Those should be grafted in (§6).

---

## 3. Assessment per proposal

### 3.1 P1, lean monolith (35/50)

**A Conversation feel: 7.**
- *Latency.* The target is a 1.5 s median to the first subtitle on cloud and 2.5 s on a local 4B model (C7). An ellipsis appears after 1.2 s, the shape ER06 §5.5 suggests.
- *Warm-up.* A local model is warmed when a persona comes into range, not when chat opens (P1 §8.1). This is the best warm-up timing of the three.
- *Turns.* Pages are split by sentence, and Enter on an empty line skips a page. A new submit interrupts the reply. Lines are committed when shown (§5.6).
- *Addressing.* An `@name` prefix, a "who can hear you" strip and a target marker (§5.1, §9).
- *Gating.* The safe-context rules are precise, and a combat signal closes the box while the reply finishes in the subtitle area (§5.1).
- *Gamepad.* The only proposal with a concrete gesture: hold View + D-pad Up to open the Steam floating keyboard (U6).
- *Losses:*
  - the generic-NPC reply cannot be reached (§7 item 4);
  - there is no per-moment latency budget;
  - nothing says the typed draft survives when danger closes the box.

**B Barks: 6.**
- *Strengths:*
  - eight data-defined triggers, ambient barks included;
  - re-validation at display time, a player-input epoch and squads;
  - floating labels, with a feed fallback when the speaker is off-screen;
  - an explicit "never imply having met the player" rule (§6.2).
- *Environment-aware ambient pools.* They are keyed by (card, region, day | night). This is the only design in which a pre-generated line cannot mention the wrong time or place (§6.2; ER06 §5.6 item 5).
- *Losses:*
  - **There is no zero-LLM floor.** If the player skips the wizard's optional "Prepare barks", camps stay silent until background batches run, and those never run in combat. Live barks are off for local models and free tiers (U7), so on those setups every first encounter is silent.
  - The generic reply is unreachable (§7 item 4).
  - The gloat after the player dies has no check for the death banner (banner 5) (§6.1, §9).

**C Voice, tone and grounding: 7.**
- *Voice.*
  - A shared tone primer that says "elevated but plain speech by register". This matches ER04 §5: most questline NPCs use 0% archaic forms.
  - Cadence, diction, address and deflection fields, written to the PR #384 lesson (SN03 §5.4).
  - Tics with frequency caps and a counter.
  - Filters for banned words and promises.
  - A register-adherence eval (§11.3).
- *Spoilers.* Unverified flags gate in the restrictive direction only (§7.5).
- *Losses.* The identity reveal relies on the overlay's runtime name plus the post-generation lexicon. The persona has a single identity line, so pre-reveal "Renna" is prompted with her full self, and only the output filter stops "Ranni" from appearing (§4.2, §5.4 section 1). There is no explicit "not yours to know" layer and no certainty marking on knowledge.

**D Memory: 8.**
- *Scope.* One `visible_to()` function serves every consumer (§3.5).
- *Facts.* Facts are observed, claimed or verified. A `claim_check` predicate turns "slew Godrick" into a verified fact once flag 9101 is set (§5.9).
- *Disposition.* It moves at most ±1 per conversation, which limits manipulation (§7.2).
- *Recall.* Up to 3 FTS-searchable notes per conversation, ranked by score, plus one guaranteed high-importance note about the player, so "do you remember me?" always has an answer (§7.4).
- *Safety.* Crash recovery of open conversations, a rollback modal with a tested trim, and NG+ partitions.
- *Losses.* The rolling summary (≤ 180 words) still fades details. Notes need ranking weights tuned per model.

**E Robustness as felt: 7.**
- *Failures.* An honest grey line, "(no reply: model timed out)", plus rate-limited toasts (§2.9). This is honest but breaks immersion more than an in-genre "…".
- *First run and load.* A wizard and self-test that include a one-line in-character test. Batch and memory jobs pause in combat. Live barks are off on local models and free tiers.
- *Loss.* When every sentence is filtered out after the single re-ask, the player sees the ellipsis and then **nothing** (§5.5 step 4). That is a masked failure (ER06 §2.4).

### 3.2 P2, split brain (30/50)

**A Conversation feel: 7.**
- *Talk hint.* The best discoverability of the three: "F6 Speak — Kalé" appears when chat is possible, computed continuously (§5.1).
- *Responsiveness.* The box opens in the same frame from the bridge's local state (§5.1). A "…" shows from submit until the first text (§5.5). Six lines of scrollback.
- *Danger.* The box collapses to a one-line draft, and input is released within one snapshot.
- *Native talk.* Unshown segments are held during native talk and resume 1.5 s after it ends (§5.7).
- *Stuck input.* Capture is released on focus loss, after 2 s without an overlay render, and on brain disconnect (R7). This is the strongest guarantee of the three.
- *Losses:*
  - the latency target is looser (p50 ≤ 2.0 s, S-5);
  - pad typing is only "best effort";
  - the hostile call-out window is degenerate (§7 item 5);
  - a brain restart leaves a 1–5 s "restarting…" gap. It is rare, and the player will notice it.

**B Barks: 5.**
- *Strengths.* Pools are the primary path. The bark deadline is stamped in the bridge's clock, so a stale bark is dropped at render time whatever the brain's delay (§2.3), which is a clean idea. Re-validation, the epoch rule and squads are all there.
- *Losses:*
  - **No ambient barks in v1:** P2 §6.4 lists its Ambient class as "reserved; v1 ships none". Generics are therefore aware of "the environment" only through a place word in the pool prompt, and the pool key leaves the place out (§7 item 6).
  - **No zero-LLM floor.** Pools are generated when an archetype first comes within 40 m (§6.3), so the first fight in each family may be silent, and fights on free tiers or local setups stay silent.
  - A generic reply that misses its 3 s deadline means silence.

**C Voice, tone and grounding: 6.**
- *Strengths:*
  - the tone primer;
  - cadence, diction and habit fields;
  - owned phrases capped at once per conversation;
  - secrets with an `until_then` deflection;
  - content at spoiler level 2 or higher unlocks only when every flag behind it is verified (§7.4);
  - the spoiler lexicon is generated from the `lexicon:` fields of locked entries, so it cannot drift out of step with the content.
- *Losses:*
  - no eval of register adherence or tone;
  - no lint on openers or tic overuse;
  - identity reveals handled as in P1, with no per-stage identity line;
  - promise patterns are only counted, never filtered.

**D Memory: 6.**
- *Strengths:*
  - the audience table records *how* each persona perceived an event (`direct`, `overheard` or `witnessed`, §7.6);
  - game-only facts;
  - a claims table whose claims are confirmed by game state (a claimed name matching `PlayerGameData.name`, or flag 9101; §5.8);
  - the previous conversation's tail goes into the history;
  - memory is editable in the F10 panel.
- *Losses:* the summary alone (≤ 220 words) loses specific callbacks, which is exactly the weakness of Mantella-style summaries that ER06 §5.4 names. There are no notes or keepsakes.

**E Robustness as felt: 6.**
- *Strengths.* Crash isolation helps the player: input never sticks and the UI stays alive. The self-test toast puts figures in front of the player ("dialogue 430 ms · pipe 0.2 ms", §8.5).
- *Losses:*
  - An unsigned helper exe spawned from inside a game process is a strong antivirus heuristic, and P2 itself rates it a high support risk (§2.8). Under the owner update, EldenNet also injects itself through its own launcher, which adds to the surface antivirus will look at.
  - Distribution friction is the top adoption killer in the prior art (ER06 §2.4; Player2's 1 in 20, ER00 §3.3).
  - The persona `fallback_lines` used on filtered-empty output ("Hm? Speak up, friend.") are generic canned speech.

### 3.3 P3, experience first (42/50)

**A Conversation feel: 8.**
- *Latency budget.* §1.5 is a budget for each visible moment: key press ≤ 50 ms; a speaker card "Varré …" after 350 ms; the first page in 1.6 s (p50) and 3.0 s (p90) on cloud; a defined miss behaviour at every step.
- *Pacing.* Whole pages with a 120 ms fade, never a typewriter. Hold times are set by reading speed, and a sentence ending in "…" gets an extra 0.4 s pause (§5.6). This is the only design that treats pacing as part of the voice.
- *Turn handling:*
  - A line typed during generation is queued, not cancelled (§5.3).
  - Pages already showing finish, and the rest is "cut off" and never committed.
  - The talk key pressed during a native menu *arms* chat, which opens when the menu closes (§5.1 rule 1). Vanilla keeps the moment, and the player still gets their chat.
  - Danger closes the box, keeps the draft and says "The moment has passed" (§5.2).
  - Rest and the Roundtable count as safe out to 8 m.
- *Losses:*
  - Pads have no typing path unless S4 passes (D4).
  - An optional game-code hook would break the owner rule (§7 item 3).
  - Conversations are one-to-one: other personas overhear but never answer, which is acceptable for v1 (SN00 §4 row 42).

**B Barks: 9.**
- *Coverage.* **Authored seed lines for every card and trigger** give a zero-LLM floor. Barks work on a fresh install, on a free tier, offline, and with a local model that is busy (§4.5, §6.3). Pools extend the seeds in the background, only at calm moments.
- *Triggers.* Twelve triggers with priorities.
- *Death gloat.* It must land within 1.2 s and is hidden once the death banner shows (§1.5, §6.2).
- *Density.* An explicit target: "a camp of five: two or three lines in a whole fight" (§6.1). A global gap of 2.5 s, at most 6 a minute, and an 8 s squad cooldown.
- *Generated-line validation:*
  - amnesia words ("again", "remember", "last time") are rejected (ER04 §4.1, ER06 §5.4);
  - proper nouns must come from the card's lexicon;
  - the player is never addressed by name.
- *Display.* An edge chevron for off-screen speakers within 12 m. No speaker name on on-screen labels, which matches vanilla (ER03 §3.2).
- *Call-out.* A call-out with a 3 s live deadline that falls back to an `addressed` seed. A blind taste test (X2) checks that pools beat the seeds.
- *Loss.* The pool key leaves out the time and region words that the generation prompt uses (§7 item 6). The seeds also mean authoring about 400–800 short lines.

**C Voice, tone and grounding: 9.**
- *Voice structure:*
  - a shared world frame, plus register presets;
  - `reply_words_max` per persona (20 for a Gurranq-like speaker, up to 60 for an orator);
  - a linter that warns on effect adjectives (SN03 §5.4, §12.4);
  - code that strips extra openers;
  - tic counters that add a "do not repeat it" note to the task line;
  - hard lint on banned modern words;
  - pronouns from body type (ER04 §4.8).
- *Measured.* Archaic share is measured per register, and a persona fails release outside 25–60% (archaic-regal) or above 2% (every other register), matching ER04 §5.
- *Spoilers.* Five layers, with a strict default and a relaxed option for players who have finished the game (§7.6). Knowledge is marked fact, rumour or belief. A persona-specific override replaces general rumours under the same knowledge key (§7.5).
- *Vanilla continuity.* A stage `gist` paraphrases what happened in vanilla, so free chat agrees with the scripted beats.
- *Identity.* **Identity is solved in the prompt as well as on screen.** The persona's self-knowledge comes from the active stage's identity line, and it holds back if the row has flipped before the stage flags caught up (§4.2). Renna is prompted as Renna.
- *Losses.* The "Not yours to know" list can prime spoilers if it names the secret (§7 item 10). The "engages at ≤ 5 m" fact is overstated (§7 item 12).

**D Memory: 8.**
- *Model.* A journal (≤ 120 words) plus up to 8 **keepsakes**, sent whole. There is no retrieval to tune per model or language (§7.2).
- *Sources.* Code checks each keepsake's `basis` (said or seen) and renders it as "The Tarnished told you…" or "You saw…", whatever the model wrote (§7.3).
- *Events without a call.* Memory-worthy events (`FoughtBeside`, `NamedAttacked`) become pending notes, so they cost no LLM call of their own.
- *Continuity.* Rollback restores the exact journal version. NG+ partitions. "Forget this NPC" is in settings.
- *Worked example.* Varré after Mohg (§7.1) is the clearest statement of witnessed versus told versus known in any proposal.
- *Losses:* claims are never promoted to facts by flags (P1 and P2 do this better); the attitude can swing without a limit; a cap of 8 can evict an old but important keepsake.

**E Robustness as felt: 8.**
- *Failure table.* §5.11 states, for every failure, what the player sees and what is logged. Toasts are held back during boss fights.
- *Local models.* Memory jobs run at rest, which is when a local GPU is free.
- *Spikes.* X1 tests whether a 4B local model holds the registers in week 1. X3 measures latency in game.
- *First run.* The self-test reports its result in words ("conversation will feel quick / acceptable / slow", §8.6). The bug-report bundle leaves out secrets and chat text by default.
- *Cost.* The full pre-bake is about 400 calls, which is cents, and seeds cover a free tier.
- *Loss.* The lint regeneration can add a second call to the first sentence, which hurts most on local models.

---

## 4. The owner update, through this lens

All three proposals assumed a me3 profile for offline enforcement, Arxan neutralisation and save separation (P1 §8.4, P2 §12.1, P3 §2.1). The player will notice three consequences, and the final design must cover each:

| Player-facing item | Proposals as written | What the final design must give the player (judge view) |
|---|---|---|
| **Install and first launch** | "Unzip, add one me3 profile" (P1 G-install, P2 G6, P3 G7) | Our own launcher becomes the one-click start. Its first screen shows, in plain words, the offline-only notice, the separate-save notice and the AI disclosure (P1 §8.6 step 1 has the right content). Players who list the DLL in their own me3 profile must get the same notices from the in-game wizard |
| **Separate save** | Taken from me3's `savefile` in all three | Modded saves taken online risk bans (ER02 §2.2, ER00 §6). The decision on save separation must be visible to the player: the launcher states whether saves are separated, and the in-game first-run notice repeats it |
| **Multiplayer pause** | All three gate in their own code. P3's check is the strictest: `player_chr_set.len() == 1`, re-checked every snapshot while `ersc.dll` is loaded (§2.5) | Keep P3's check. Add P1's and P2's visible badge, "EldenNet paused (multiplayer)" (P1 §2.6, P2 §2.6), so the silence is explained |
| **Game-code hooks** | P3 §5.2 keeps an optional "menu has the mouse" hook | Drop it (§7 item 3). If S4 shows camera drift, blank mouse input through DirectInput and accept the drift, or open chat only when the player is stationary |
| **Event engine** | P1 has exact conditions (§3.4). P2 has store classes (§3.3). P3 has an audience column per event and golden event files (§3.6, §11.1) | For barks the event engine *is* the experience. It needs four things from this lens: **(a)** an end-to-end budget of ≤ 0.5 s from game change to scheduler for bark-relevant events (4 Hz snapshots plus debounce); **(b)** a confidence field that speech rules can require (§6 graft 15); **(c)** the audience frozen at emission; **(d)** "interrupt" as a property of the event type (SN06 §7.1 item 5), so that, for example, `AggroStart` flushes ambient barks without hard-coded rules |

---

## 5. Winner and why

**P3 wins for player experience and AI quality.** Its structure starts from four player scenes (§1.1) and works back to mechanisms. Three things lift it above the other two:
- **barks never wait for the network, and never go silent on a fresh install** (seeds);
- **tone is enforced and measured**, not only described (lint, register presets, an eval that gates each persona);
- **identity and spoilers are handled at the prompt layer**: stage identity lines, gists, certainty labels and five gating layers.

Its weaknesses are fixable with grafts: claim verification and bounded attitude from P1, context-keyed pools, a pad path, and an anti-priming lint.

P1 is a close second on memory and on concrete details, such as warm-up on approach and the gamepad gesture. P2's split brings real crash isolation, but this lens sees mostly its costs: antivirus friction, restart gaps and a looser latency target. Its bark and tone layers are also the thinnest of the three.

---

## 6. Grafts for the final design (specific)

**Conversation**

1. **Per-moment latency budget and wait cues** (P3 §1.5). Keep the budget table as a normative part of DESIGN.md. Show the speaker card "Name …" after 350 ms. Use in-genre "…" pages instead of spinners (ER04 §5: about 12% of vanilla lines end in "...").
2. **Page pacing** (P3 §5.6). Reveal whole pages, hold them for reading time, pause 0.4 s after an ellipsis, and never use a typewriter effect. Skip with Tab, or Enter on an empty line (P1 §5.6).
3. **Warm up on approach, not on open** (P1 §8.1). When a persona enters range after more than 4 idle minutes, send Ollama's native keep-alive. Render the cached prefix when chat opens (P2 §5.2, P3 §5.3).
4. **Discoverability.** P2's talk hint, "[key] Speak — Name", shown while chat is possible (P2 §5.1). P3's target marker and "heard by" header (P3 §5.1). P3's "They have nothing to say to you." for mute targets.
5. **Typing in a running world:**
   - P3's danger rule: close at once, keep the draft, say "The moment has passed" (P3 §5.2);
   - P2's input-release guarantees: on focus loss, on 2 s without an overlay render, on Esc and on any core fault (P2 R7);
   - P3's arming of chat during a native menu (P3 §5.1 rule 1);
   - P2's hold-and-resume of unshown pages around native talk (P2 §5.7).
6. **Queue, don't cancel.** A line typed while the reply is generating is queued in one replaceable slot. Pages already showing finish, and the rest is dropped and never committed (P3 §5.3).
7. **A gamepad path in v1 (judge view; owner decision).** None of the three ships one unless the unverified Steam floating keyboard works (ER00 §2 row 8 rates pads Low–Med). Proposed path:
   - Add a fixed **intent list** of 6–8 entries: who are you, this place, what you know of a topic from the persona's KB topics, the last thing you saw, farewell, and "speak freely" (Steam keyboard).
   - Each entry fills in a short player line, shown as text (SN06 §7.4).
   - It is dialogue-only, needs one ImGui list and one data file, reuses the XInput hook that blocking already needs, and also serves keyboard players when typing is unsafe.
   - Bind it with P1's gesture (hold View + D-pad Up, P1 §5.1), subject to S4.

**Barks**

8. **Zero-LLM floor** (P3 §4.5, §6.3). Authored seeds for each card and trigger, in our own words. Pools extend them, only at calm moments.
9. **Context-correct pools.** Combat pools must not mention time or place (enforce this in pool validation). Ambient pools are keyed by (card, region, day | night) (P1 §6.2). Any other safe tokens are slot-filled at serve time (ER06 §5.6 item 5).
10. **Pool validation** (P3 §6.3):
    - reject amnesia words;
    - allow only proper nouns from the card's lexicon;
    - never name the player;
    - dedupe;
    - accept a batch only if at least 8 lines pass.
11. **Moments that matter:**
    - the death gloat lands within 1.2 s and is hidden on the death banner (banner 5) (P3 §6.2);
    - barks stay quiet for 3 s after a conversation ends (P3 §5.7);
    - no barks during boss fights (all three).
12. **Density targets as acceptance tests.** "A camp of five gives two or three lines per fight" (P3 §6.1), checked by replay assertions in P1's scenario format ("bark within 1.0 s of `AggroStart`; none after 2.5 s", P1 §11.2).
13. **Expiry checked again at draw time.** Stamp each bark's deadline in the presentation clock, and have the overlay drop it if late (P2 §2.3). This holds in a single process too.
14. **Off-screen handling.** An edge chevron within 12 m, otherwise drop the bark. No speaker names on on-screen labels (P3 §6.6).
15. **Confidence-gated speech rules (judge view; uses the owner's event-engine confidence field):**
    - an engagement detected from lock-on alone has low confidence and produces no aggro bark;
    - aggro lines need a hit exchange, or a lock-on plus closing distance;
    - this is the #771 lesson ("searching" enemies that never engage must not bark), SN05 §8.4.
16. **Working generic replies:**
    - fix the targeting contradictions in P1 and P2 (§7 items 4–5);
    - adopt P3's call-out gate (no boss fight, no *engaged* hostile within 25 m; an unaware sentry is a fair target), with the seed fallback for `addressed`;
    - P3's in-combat "taunt" tap is optional and can wait.

**Voice, tone and grounding**

17. **P3's tone kit as a whole** (P3 §4.4):
    - world frame;
    - register presets;
    - `reply_words_max` per persona;
    - effect-adjective linter;
    - opener lint;
    - tic counters with a task-line reminder;
    - hard banned-word lint.
18. **An eval that gates releases** (P3 §11.2 with P1 §11.3). Check archaic share per register, the word cap, banned words, locked spoiler terms, promise patterns and unknown proper nouns. A persona ships only with a passing report. Run P3's X1 voice spike in week 1, including a 4B local model.
19. **Identity at the prompt level** (P3 §4.2–4.3):
    - each stage can carry its own identity line;
    - self-knowledge follows the stage flags;
    - the on-screen name follows the runtime NpcName row;
    - a lint warning fires when the two disagree.
    - Make "Renna is prompted as Renna" an acceptance test (ER04 §7 rows `20500010` and `20500000`).
20. **Spoiler stack.**
    - P3's five layers and strictness setting (P3 §7.6).
    - P2's rule that content at spoiler level 2 or higher unlocks only on verified flags (P2 §7.4).
    - P2's lexicon generated from locked entries (P2 §7.4 rule 3).
    - P3's certainty labels and per-persona knowledge-key overrides (P3 §7.5).
    - P3's six-flag-state spoiler eval (P3 §11.2).
21. **Anti-priming lint (judge view).** No `forbid` or "not yours to know" entry may contain a term that is locked in the spoiler lexicon. Forbids describe the topic ("what the rite leads to"), never the secret (SN03 §5.4, PR #384).
22. **A gist of what happened in vanilla per stage** (P3 §4.3), so free chat agrees with scripted beats that v1 cannot capture.

**Memory**

23. **P3's journal plus keepsakes**, sent whole (P3 §7.2–7.3), with three additions:
    - P1's `claim_check` and P2's game-state confirmation, which promote a claim to a verified fact (P1 §5.9, P2 §5.8);
    - P1's attitude limit of ±1 step per conversation (P1 §7.2);
    - P1's rule that a high-importance keepsake about the player is never evicted (P1 §7.4).
24. **Audience with "how"** (P2 §7.6: direct, overheard or witnessed). Render overheard lines as overheard in the recap (P1 §5.4 section 12).
25. **Player control.** P2's editable memory view and P3's "forget this NPC" / "forget this journey" (P2 §9, P3 §9.1). Make P3's Varré-after-Mohg table (§7.1) an acceptance test.

**Failure behaviour**

26. **P3's failure table** (P3 §5.11) as the normative player-facing contract:
    - toasts are held back during boss fights;
    - a 20 s timeout gives an uncommitted "…" page plus a toast naming the cause;
    - a reply blocked twice by lint shows an authored deflection;
    - P1's "show nothing" path is never used (§7 item 8).
27. **Self-test in words** (P3 §8.6) plus P2's figures in the ready toast (P2 §8.5). The test persona must be original (P3's "Lantern-Keeper").
28. **Identity-pack check under overhauls** (P1 §4.2). When more than 20% of persona rows' name IDs mismatch, personas are disabled with a toast and barks keep working. This keeps a randomizer from making Patches speak through another body.

---

## 7. Fatal flaws and wrong claims

| # | Where | Finding | Evidence |
|---|---|---|---|
| 1 | All three (P1 §8.4, P2 §12.1, P3 §2.1) | **Overruled by the owner update.** A me3 profile provides offline start, `disable_arxan` and the separate `savefile`. The player-facing install flow and save separation must be redesigned around EldenNet's own launcher (§4) | Owner update item 1; ER02 §2.2 (ban risk of modded saves online) |
| 2 | All three (P1 D5/U1, P2 D1, P3 D1/§2.8) | **Overruled.** Each defaults to vendoring the pre-change fromsoftware-rs subset. Replace with the Game Access Layer. No direct effect on the player beyond patch-day behaviour, which all three already degrade to a banner | Owner update item 2; ER00 §3.2 |
| 3 | P3 §5.2 | **Optional hook on game code** ("menu has the mouse", 2.7.2.0 `0x140766b00`). Breaks the v1 rule of no game-code patching and would need Arxan handling. Must be dropped, not kept as a per-build option | ER03 §7A; ER05 §11 option 4; owner update item 1 |
| 4 | P1 §5.1 vs §6.5 | **The generic reply cannot be reached.** Every rule in the §5.1 target order selects personas (`@name`, native talk, lock-on persona, persona cone, nearest persona), yet §6.5 assumes it can resolve to a card. §6.5 also requires no hostile within 12 m, so a hostile generic target would have to be beyond 12 m while the cone reaches only 8 m. Requirement 3's "respond with simple things" then rests on barks alone | P1 §5.1, §6.5 |
| 5 | P2 §5.1 vs §6.6 | **A degenerate window for calling out to a hostile generic.** Lock-on targets count only within 8 m and the cone reaches 8 m, but the relaxation requires the hostile target to be at least 8 m away and not `Engaged`. Lock-on within 20 m already counts as `Aggro`, which sets `Engaged` (§3.3), so the lock-on path can never qualify | P2 §3.3, §5.1, §6.6 |
| 6 | P2 §6.2–6.3; P3 §6.3 | **Pool lines can name the wrong time or place.** The generation prompt includes the place and a time bucket or region word, but the pool key leaves them out. A line written for "dusk in Limgrave" can play at noon in Stormveil. P1 keys ambient pools by region and day/night, which is right | ER06 §5.6 item 5 (slot-fill safe tokens only) |
| 7 | P1 §6.2; P2 §6.3 | **No zero-LLM bark floor.** Without a pre-bake (optional in P1) or before a first-sight batch finishes (P2), first encounters are silent. With free tiers (50 requests a day) or a busy local GPU, they can stay silent for the whole session. Requirement 3 then fails on exactly the setups the presets recommend | ER06 §5.6 (binding limits: latency, free-tier caps, GPU contention); ER06 §5.2 |
| 8 | P1 §5.5 step 4 | **A masked failure.** If every sentence is filtered and the single re-ask is also empty, P1 shows nothing and only logs it. The player sees an ellipsis and then silence, with no cause given | ER06 §2.4 ("always also surface the real cause") |
| 9 | P2 §2.8 | **Antivirus and install friction understated for the player.** A DLL inside a game spawning an unsigned child exe is a strong heuristic trigger, and P2 rates it "high support risk" itself. The owner's own-launcher injection adds a second suspicious step. The split also does not address SkyrimNet's main crash class, which P2 concedes, so the player pays this cost for isolation from uncatchable faults only | P2 §2.8; SN00 §3 (#819/#830/#831); ER06 §2.4; ER00 §3.3 (Player2: about 1 in 20 downloads reach gameplay) |
| 10 | P3 §7.6 layer 2, §5.4 block 7 | **Priming risk.** Listing forbidden topics in the prompt makes small models more likely to mention them, and a forbid entry that names the secret injects it. Needs the lint in graft 21 | SN03 §5.4 (PR #384: forward-looking material and priming descriptors) |
| 11 | P3 §4.1, §4.2 | **Shared-persona memory keyed by `entity_id`.** Nomadic merchants get memory keyed `persona#entity_id`. The share of generics with `event_entity_id == 0` is still open, so merchants could merge into one memory or fork across sessions. Verify in S1, or give them no memory | ER05 open question 1; ER00 §1.4 identity row; ER00 §7 S1 |
| 12 | P3 §5.2 | **Overstated fact.** "The vanilla template engages at ≤ 5 m" is presented as fact. ER03 says those defaults are LIKELY metres from an *unreachable* block, and that other NPCs may pass different values. The 5 m start is a sound choice but must be playtested | ER03 §2.1 item 4 |
| 13 | All three (P1 U6, P2 D8, P3 D4) | **No gamepad conversation path** unless the unverified Steam floating keyboard works. Elden Ring is widely played on pads and Steam Deck, so a large share of players could be locked out of requirement 2. This is a gap rather than a wrong claim: see graft 7 | ER00 §2 row 8 (pads Low–Med); ER02 §7 (pad options); SN06 §7.4 |
| 14 | P1 §6.1, §9 | **The gloat may overlap the death banner.** `player_down` has a 3 s expiry, and the overlay hides only on `HideAll`, loading or a non-InGame screen. P2 (§6.5) and P3 (§6.2) both hide on banners | ER05 §3 (banner id 5) |
| 15 | P1 §5.4 section 1; P2 §5.3 S1/S3 | **Reveal identity is not handled at the prompt layer.** The persona's single identity line and its private text describe the post-reveal self, so the pre-reveal form (Renna) is prompted with knowledge of the reveal. Only the output lexicon catches "Ranni" being said. A small model can still leak by paraphrase ("the witch who is more than she seems…"). P3 §4.2 fixes this with a per-stage identity line | ER04 §7 (identity changes mid-game; stage-locked secrets) |
