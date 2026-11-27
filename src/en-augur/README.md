# en-augur: Augur, the event engine

Augur turns snapshot diffs, lifecycle changes and time into typed events with frozen audiences (ARCH §8, DESIGN §6;
the first in-game build's part 11, roadmap S10.2.c). C++20, MIT, our own code. It is **pure**: no I/O, no clock (time
comes in as `Mono`), no thread, and no exception of its own; it is **below en-core** in the graph and **game-neutral**:
what en-core knows comes through `AugurCtx` (who a character is, the predicates, who may perceive, the seed), and what
the game's values mean through `WorldFacts` (the hostile teams, the victory banners, a critical hit's throw states, the
open world's areas). Its static library `en_augur` is a module the DLL links (`en_dll_module`: no static object with a
dynamic initialiser or an exit-time destructor), and it depends on en-proto alone (with toml++ for the rule files).

## What it holds

| Header | ARCH | What |
|---|---|---|
| `types.hpp` | §8.1, §8.6, §8.8 | `Mono`, `Subject`, `ActorRef`, `EventId`, `Class`, `How`, `Radius`, `PlaceRef`, `InterruptSet`, `SideId`, `BossRef`, `PlaythroughKey` |
| `events.hpp` | §8.6, DESIGN §6.4 | Every v1 kind of the catalogue as a struct, `EventKind` (their variant, in the catalogue's order), `Event`, `visible_to`, the kinds' interrupt sets |
| `signals.hpp` | §8.3 | The signal registry (code-defined), `Value` (unsampled, none or a value), the lifecycle facts |
| `detectors.hpp` | §8.4 | The detectors as state machines, with debounce, the first modifier |
| `rules.hpp` | §8.5 | The rule files' schema, `RuleSet::load` and its validation, the payload builders' table, `PredicateCompiler` |
| `world.hpp` | §8.2, §8.7 | The world model: trackers, the Tarnished, the world, engagement, squads, combat state, stance, concealment, the boss fight |
| `kstore.hpp` | §8.4, §8.10 | The K store (merge, TTL, cap 512, "just now"), the dedup table, the cooldown table |
| `sources.hpp` | §8.2 | `EventSource`, `Observations`, `AugurInput`; SnapshotDiff, Lifecycle and Timer built, FlagWatch and Pipeline the seams, H1 to H3 compiled out (`EN_HOOKS` stops the build) |
| `augur.hpp` | §8.1, §8.9 | `AugurCtx`, `Augur::step` (§8.9's steps 1 to 4), `resolve_audience` |
| `audience.hpp` | §8.8; MEM §1.3 | `visible_to` (RAM or stored events), `Visible`, `stored_audience` |
| `bus.hpp` | §8.9 | The bus (step 5), the step FIFO and its drops, the exception policy, `PersistQueue` (the store requests' policy) |
| `golden.hpp` | §8.12 | An event in the golden files' form, handles renamed h1, h2, ...; and each kind's payload schema as that form writes it (`kind_fields`, `fields_text`: Tools › Workshop's, held to `event_json` kind by kind) |
| `words.hpp` | §8.12, DESIGN §14 | Augur's words, one closed vocabulary for the golden files and the F11 view (classes, sources, radii, actors, the payloads' enums, the kinds' names, the detectors' as the rule files write them), and `reason`, an event's one telling fact at a glance: a word and, where the payload's telling field is one, a number (a place's id, a count, a slot), never an HP fraction, a distance or a handle |

`Augur::step(AugurInput{snapshot, life}, now, ctx, out)` takes one step of en-core's: a processed snapshot or none, and
the lifecycle changes en-core's slice made on the same step. The world model takes the snapshot (step 1); the sources
are polled in their order, Lifecycle, SnapshotDiff, FlagWatch, Pipeline, Timer (step 2); each source's fires are sorted
by (priority, subject distance, handle), the rule's place breaking a tie, and go through the modifiers in §8.4's order:
debounce (in the detector), `requires`, the cooldown, class handling (P dedup by key in play time, K merge and TTL, T
dispatched), confidence (step 3); then the ids are given in dispatch order and the audiences resolved and frozen (step
4). en-core then runs the bus (step 5).

## The v0.1 catalogue, and v0.3's personas

The shipped rules are `content/eldenring/augur/{world,combat,lifecycle,persona}.toml`, our own, written from the
design's specs; en-core builds them in. en-core classifies a character bound to a loaded persona as that persona, and
lets it perceive while it is conscious and its stage does not mute it (core.hpp's `AugurView`); every other character
is Mute until cards exist (v0.4).

**v0.3's persona rules** (`persona.toml`; MEM §1.1, DESIGN §6.5), each an event that feeds memory, with its audience:

- `PersonaSeen`: within 10 m for 2 s, re-armed beyond 30 m or after loading; P, the persona only.
- `PersonaApproached`: within 6 m for 1 s, re-armed beyond 10 m (the memory bundle's prefetch); T, no audience.
- `PersonaAttacked`: the Tarnished's hit takes its HP down (`hit_by_player` rising), 0.95; P, witness 20 m, the
  victim direct.
- `PersonaDied{by_player}`: the dead bit rising; P, death 30 m.
- `FoughtBeside{boss}`: a boss fight's Victory with the persona alive, not hostile and within 40 m, 0.9; P, the
  persona only.
- `NativeTalkStarted` and `Ended` with a persona: `talking_with`, the nearest persona within 3 m while a talk signal or
  the pop-up menu is up, its start dedup'd for 5 min, 0.6; P, the persona only.
- `KilledNear{witnesses}`: the Tarnished kills a character that is no persona with personas within 20 m; K, 120 s,
  witness 20 m.

- The audience radius `subject` is MEM §1.1's "the persona only": the subject persona, direct, when it may perceive.
  Every audience leaves out a dead persona (MEM §1.2).
- The world's own `NativeTalkStarted` and `Ended` (world.toml) stay T, with no persona, for the scheduler's lock.
- en-core gives the conversation's commits (`ConversationStarted`, `PlayerSaid`, `NpcSaid` at its first page,
  `ConversationEnded`) as events with audiences resolved here: speech 16 m from the speaker, direct for the one spoken
  to and overheard by the rest; an end the persona only.
- **Left for v0.4 (O147):** the pacing core and NPCs' own lines are v0.4's, so `CameNear` and `AutonomyTick`, which
  only arm greetings, quest business and autonomy, are not built, and a rule file naming them is refused. The hostility
  bits' path of `PersonaAttacked` waits for S7's `team_turned`; whether a persona beside a victory came as a summon,
  for S7 too. `StageChanged` is the knowledge subscriber's, in code (slice 2).

| Event | v0.1 | From |
|---|---|---|
| `PlaceEntered` | emitted | `place_id` stable 2 s; P, dedup 10 min per place |
| `BossFightStarted`, `BossFightEnded` | emitted | `boss_active`'s edges; Victory on a victory banner, Death on a PlayerDied, Left 10 s on (the boss flag's evidence waits for flags) |
| `NativeTalkStarted`, `NativeTalkEnded` | emitted, no persona | the popup's talk id or the talk camera (T, 0.6); `hud == PopupMenu` with a persona within 3 m waits for personas |
| `PlayerDied` | emitted | the dead bit, the death count, banner 5: three rules, one event (dedup 5 s) |
| `MountChanged` | emitted | the ride edge (K 30 s) |
| `Struck`, `LowHp`, `Healed` | emitted | HP drops, crossings and rises (§8.7's attacker, `hard` and `unseen`) |
| `Engaged`, `Disengaged` | emitted, the Tarnished's pairs | the engagement machine; NPC pairs wait for S7 (v0.4) |
| `AllyDied` | emitted | a death with live witnesses of its live team within 20 m |
| `CritLanded` | emitted | the Tarnished's throw 3 while the character's is 4 |
| `DataReady`, `PlaythroughReady`, `LoadingStarted`, `LoadingEnded`, `TitleReached`, `JourneyChanged`, `GateChanged`, `GroupTripped`, `LadderChanged` | emitted | en-core's lifecycle slice (as facts), and `ng_lvl` for `JourneyChanged` |
| `GraceRested`, `RestEnded`, `GraceLit`, `FlagChanged` | waits (v0.2) | flag edges: the flag catalogue and FlagWatch, with the world view |
| `PersonaSeen`, `PersonaApproached`, `PersonaAttacked`, `PersonaDied`, `FoughtBeside` | v0.3 | above |
| `StageChanged` | waits (v0.3, slice 2) | the knowledge subscriber, in code |
| `KilledNear` | emitted (v0.3) | persona witnesses; the victim's card from v0.4 |
| `IdleNear` | waits (v0.4) | cards with ambient lines |
| `CameNear`, `AutonomyTick` | waits (v0.4; O147) | the pacing core: greetings and autonomy |
| `FoeKilled` | waits (v0.4) | S7's check of `last_hit_by` for NPC attackers |
| `StatusStarted`, `StatusEnded`, `StatusBurst` | waits (v0.4) | S8's statuses |
| `PlayerSaid`, `NpcSaid`, `Callout*`, `Conversation*` | waits (v0.2 on) | the Pipeline source: the chat, the call-out |
| `NativeLineHeard` | waits (v0.2 on) | S3's scripted-line detector |
| `NpcThought`, `DecisionMade` | never in v1 | reserved seams |

A rule file naming a kind that waits is refused at load with the version and source it waits for. Bark rules are
v0.4's.

## The rule files

The schema is DESIGN §6.3's and ARCH §8.5's, every key checked and unknown keys refused, with the line of each problem
(`RuleError`: file, line, problem; a toast's text, DESIGN §11.4). Any error refuses the whole set, so a reload keeps the
old one (`Core::load_rules`). User rule files load only with `allow_custom` (`augur.allow_custom_rules`, off in v1).
Bounds: ARCH §8.5's, each file ≤ 64 KB and ≤ 256 rules; and this build's own: a window ≤ 10 s, `stable_ms` and
`dwell_ms` ≤ 60 s, an id ≤ 48 bytes, a key ≤ 96 bytes, `requires` ≤ 1 KB, at most 64 errors reported.

What this build adds to the schema, each the smallest reading of a rule the catalogue states:

- `subjects.in_combat = true`: only while the subject is in combat (the Tarnished: §8.7's combat state; a character:
  engaged at 0.6 or more). Healed's "in combat, any combatant".
- `detect = { kind = "changed_stable", stable_ms, to = [...] }`: `to` lists the values that may fire (whole numbers,
  true or false, "none"); the change is tracked either way. PlayerDied's banner 5.
- `detect = { kind = "engagement", on = "engaged" | "disengaged" }`: the machine's transitions.
- `detect = { kind = "fact", value = "<fact>" }` on the signal `lifecycle`: the Lifecycle source's facts
  (`data_ready`, `playthrough_ready`, `loading_started`, `loading_ended`, `title_reached`, `gate_changed`,
  `group_tripped`, `ladder_changed`), so that every event, the lifecycle's included, has a rule file.
- `payload` may be left out when one builder makes the kind (`boss_outcome` must be named: it defers).
- Signals: `crit_by_player` (CritLanded), `native_talk` (talk popup or talk camera) and `lifecycle` (the facts) join
  §8.3's registry. `clock_bucket` is not registered (the seven words' hours are not set down), nor `flag:<alias>`
  (v0.2: a rule naming one is refused); `team_turned`, `crouching`, `invisible` and `idle_due` are registered and never
  sampled until their spikes verify them.

`requires` compiles through en-core's predicates (`en/core/predicates.hpp`): v0.1 evaluates `true`, `false`, `!`,
`&&`, `||`, parentheses and comparisons of `runes_held` (the runes held) and `journey`; `world()` and `knows()` are
refused in Augur for good, and `flag()`, `boss()`, `stage()`, `region()` and `runes >= n` (MEM §6.6's great runes,
through the catalogue's aliases; WHY§474) until Augur reads the catalogue and regions.toml (every alias is unknown).

## What ARCH §8 left open, and how this build reads it

- **Augur's place in `Core::step`.** It runs on every step before anything that consumes events; en-core's lifecycle
  slice judges the input just before it, because the Lifecycle source reads what the slice changed on the same step
  (loading, the gate, the ladder): a loading screen clears engagements on the snapshot that shows it, not a step later.
- **The input.** en-augur is below en-core, so it takes `AugurInput` (the snapshot and the slice's changes as
  `LifeFact`s), not en-core's `CoreInput`. `GroupTripped` comes from C2's message or a snapshot's health, once per group
  for the session. `PlaythroughKey` is the slot and the session's ordinal until the store's key exists.
- **Time.** Detectors sample at each processed snapshot's `mono_ms`; events carry the step's `now`. The Timer source
  fires once per whole second crossed (at most 60 at once); the K store expires at the start of every step, the boss
  outcome's Left on a Timer fire. In a replay the steps are the recording's inputs (ticks are not recorded), so a Left
  lands on the first input past its second: deterministic for every replay.
- **First samples.** No detector fires on one; the engagement machine is not a detector, so a hostile first seen
  within 8 m engages on that snapshot.
- **Engagement.** Only with a live character on a hostile live team, and a Tarnished present. A stronger condition
  raises the confidence and re-emits only when it crosses 0.6. `LockClosing` compares with the oldest sample within the
  last 2 s. After a Timeout, Near and lock-on start it again only once neither holds (else a hostile standing near would
  engage every 12 s); a hit always does. A character lost (absent 2 s), no longer hostile, or with no Tarnished, ends
  its engagement silently, as a death does. `fled` reads the Tarnished's distance over the last 5 s.
- **Squads.** The same side is the same live team (no cards yet); "a member engaged within the last 10 s" is a member
  engaged now whose last exchange, lock or start is within 10 s. The Tarnished's side holds the Tarnished alone until
  S7 shows which live teams fight beside it.
- **Combat state.** A signal at a snapshot: the Tarnished's HP dropped, a character hit by the Tarnished, a critical
  hit's throw states, or an engagement at 0.6 or more.
- **Struck.** The shipped window is 1 s, not §8.5's 300 ms, so a hit across a dropped snapshot still fires once (the
  base resets on each fire, so a longer window counts nothing twice). For a character struck, the attacker is read
  only when it is the Tarnished (unseen beyond 20 m); another character's is unknown until S7. For the Tarnished struck,
  its `last_hit_by` names the attacker if that is a present character within 20 m, else the hit is unseen. An unseen
  hit names no attacker (`att` empty; its key says `unseen`, an unknown one `none`).
- **The boss outcome.** `fight_id` is the session's ordinal of the fight (the start's EventId is not given when its key
  is made). Victory's evidence is a victory banner at any snapshot since the fight began (it may show before the bars
  go), Death's a PlayerDied emitted since; the earlier wins; Left when 10 s pass from the falling edge without either.
- **Loading.** On `LoadingStarted`: engagements, squads and the K store cleared silently, the characters forgotten,
  `enter_radius` re-armed; the Tarnished's and the world's detectors carry on (a fast travel still enters its place),
  and a debounce candidate waits on (a boss fight's end still matures on the loading screen after a death).
- **The K store.** A K fire merges only into an entry of the same rule; a fire of another rule with the same key
  replaces the entry (Engaged and Disengaged share `eng:a:b`). An entry's audience is frozen with its event's. The
  dedup and cooldown tables keep at most 4,096 keys each, the oldest going first.
- **Interrupts** are the kind's (§8.8's table, in code, with Engaged's conditions); a rule's `interrupts` adds to them.
- **Audience.** The origin is the subject's position, the Tarnished's for its own and the world's events, and the
  Tarnished's when the subject is no longer present.
- **The bus.** On overflow the newest T event goes first, then the newest K. v0.1 has no subscriber: en-core takes the
  dispatched events into `Outputs::events`. `PersistQueue` keeps the store requests' policy until the store exists
  (v0.3): speech may take any free slot of the ring and is kept past a full one (the chat waits while
  `speech_blocked()`), and persistence resumes once the ring is empty and the queue below 512.
- **`AugurCtx::rng`** is splitmix64's finaliser over the seed root (0 until the store's playthrough key), the event and
  the salt, in en-core; BLAKE3 replaces it with the seed root. No v0.1 rule draws from it.
- **Hashes.** A rule set's hash and a golden file's are FNV-1a 64 (`fnv1a64:<hex>`): BLAKE3 is en-launcher's, which
  neither en-augur nor en-tools links.

## The golden files and scenarios

`tests/golden/<route>.events.jsonl`: a header (`golden`, `route`, `source`, `rules_hash`, `events`), then one event a
line with the keys `seq` (step, index), `t`, `t_play`, `kind`, `class`, `rule`, `source`, `subject`, `object`, `key`,
`conf`, `audience`, `count`, `merged`, `interrupts`, `payload`, numbers to three decimals, handles renamed h1, h2, ... in
order of first appearance (no handle of a session, no name, no SteamID). `eldennet-engine golden` writes and checks
them (src/en-tools/README.md), and `tests/golden/CHANGES.md` must name each golden's hash. The three v0.1 routes are
synthetic scenarios of ours (`tests/scenarios/`: a camp skirmish, three boss encounters, a road with a co-op visit);
a recording of the owner's game becomes a route the same way once a redacted recording may be committed (S10.2.d).

## Tests

`en_augur_tests` (doctest; every build, under Wine for the Windows ones), with invented data only:

- each detector, debounce's cancel and maturing, the first-sample rule, `to`, windows and their base;
- the rule files: the shipped set loads with v0.1's 23 kinds; a good rule read field by field; every refusal with its
  file and line (unknown keys at each level, the catalogue, the registry, type compatibility, hysteresis, class rules,
  placeholders, radius, interrupts, `requires`, every bound, ids unique, a kind that waits, a builder for another kind,
  debounce on the machine, a fact unknown, the TOML pre-check and toml++'s errors); the file and set bounds; user files;
  any error refusing the whole set;
- Augur::step: Struck (merged, hard, salient re-emits, the Tarnished's attacker, unseen, unknown), LowHp, PlayerDied
  once from its three rules, each boss outcome, PlaceEntered's stability and dedup, MountChanged, NativeTalk,
  JourneyChanged, CritLanded, AllyDied's witnesses, Healed only in combat, `requires` and cooldowns, the audience
  (radius, area, how, the cap of 16, who may perceive, "just now"), loading, interrupts;
- the world model: each start condition, the raise across 0.6, the ends (Distance with fled, Timeout with its re-arm,
  death), squads, the combat state, stance, trackers dropped after 2 s and the cap of 256;
- the bus: dispatch order, emitted events at the tail, the FIFO's drops, the exception policy, a stalled store;
- the persona rules and audiences (`test_personas.cpp`): seen once and re-armed beyond 30 m, approached, an attack
  direct for the victim and witnessed within 20 m only, a death witnessed and never by the dead, a persona that may not
  perceive in no audience, a kill witnessed by personas or no event, FoughtBeside at a victory by the persona only, a
  native talk's persona the nearer of two within 3 m; `visible_to` over stored and RAM events (the journey, the row,
  the reserved kinds) and `stored_audience`; CTest's `en_augur_audience_grep` (MEM §1.3's two greps);
- the words: each enum's, `?` outside it, the golden form writing the same, each kind's reason, and every kind's reason
  a word of ours;
- seeded properties (fixed seeds; a failure prints its seed): hysteresis (detectors, and LowHp through Augur), debounce,
  gaps (no edge invented, an edge across a gap once, a count across a gap once), merge (one entry, `count` the inputs,
  only salient re-emits), ordering, and determinism (byte-identical golden text over 10 runs).

`en_core_tests` checks the wiring (the built-in set, the slice's facts on the same step, Mute and Player, v0.1's
predicates, a failed reload keeping the old set); `en_tools_tests` the scenario DSL, the three goldens, determinism and
50% snapshot loss (below); CTest's `en_golden.<route>` runs `golden check` on each route.

**Snapshot loss.** With half the snapshots dropped at random (12 seeds a route), every event of the lossy run is one
of the dense run's, never earlier; every event of a lasting state (the lifecycle's, places, boss fights, deaths, the
mount, the journey, native talk, AllyDied) is there with its payload, at most two gaps and a second later. What may
differ, by the design's own rules: per-hit events (Struck: the hits of a dropped snapshot reach the next kept one as one
drop), states briefer than the gap they fall in (a near pass, a lock-on before the closing, a heal within 1 s, a
critical hit's two snapshots, a low HP the next hit kills), NativeTalkEnded's `held_ms`, and the gate, whose 5 s hold
starts again after any gap over 1 s (ARCH §5.1).

**Not here.** Net saturation is en-llm's, tested there ("service: a full queue gives a bark_pool job's place away as
Dropped and a memory job's as Preempted": pool jobs first, memory jobs requeued, replies never). A stalled store is
tested on `PersistQueue`; the store itself is v0.3's.

## Rates and costs (ARCH §8.10)

`en_augur_bench [steps]` steps Augur on a busy synthetic step: the shipped 25 rules, 32 characters (mostly hostile,
closing and trading hits), the Tarnished hit and locked on, a boss bar, at 4 Hz.

Clang 18, Release, on the 4-core Linux machine of the build (a 2.1 GHz Xeon), 20,000 steps, three runs, about 7
events a step:

| Measure | Augur::step | ARCH §8.10's budget |
|---|---|---|
| Mean | 25 to 27 µs | ≤ 1 ms for SnapshotDiff and the world model, ≤ 1 ms for the rules (≈ 25 × 33) |
| p50 | 23 to 25 µs | |
| p99 | 51 to 71 µs | |
| Most | 0.9 to 2.1 ms (the machine's scheduling: a step's own work is the p99's) | |

Loading the shipped rule set (three files, 25 rules, through toml++) takes 0.5 ms warm and 5 ms cold. An unoptimised
GCC build is about 30 times slower (766 µs mean): use a Release build for numbers.
