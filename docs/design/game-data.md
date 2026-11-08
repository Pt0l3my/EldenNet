# EldenNet v1: game data, from research to live tracing

> **Status:** detail document for [DESIGN.md](DESIGN.md), 2026-11-08. Code **DATA**. It answers the owner's direction O57 ([DECISIONS](../DECISIONS.md#o57)): EldenNet keeps three kinds of game data apart (§1), and it must be able to trace events, NPCs and data during play so that every feature works. §2 checks that feature by feature, §3 lists the gaps and risks, and §4 the one question only the owner can settle. This document adds no design rule: where it finds the design short, §3 says what would close it and where it goes on the roadmap. DESIGN.md wins wherever the two disagree.
> **Companions:** DESIGN §4–§6 and ARCH §6–§8 own the game access layer (GAL), the snapshot and Augur; NPC owns identity, kinds and types; MEM owns the flag catalogue and world awareness; [ER08](../research/eldenring/08-game-data-sources.md) owns the research sources and their licences. **This document owns** the boundary between the three kinds of game data and the coverage map.
> **Citations:** `ER0n §x`, `SN0n §x` and the design codes as in DESIGN.md; GraceUI's design is cited as "GraceUI §x" and its decisions as "GraceUI Dn" or "Gn". Evidence labels (FACT, LIKELY, SPECULATION) are the research's. Every game number the cited documents give is a lead until its check passes (O48); this document quotes no number they do not already quote.
> **Licence hygiene:** no game text, developer label or param data appears here; the worked example uses placeholders.

---

## 1. The three kinds of game data

The owner decided on 2026-11-08 (O57) that EldenNet keeps three kinds of game data apart, that how the data is got does not matter so long as everything needed is got, and that what must hold is live tracing: "we just need to ensure that EldenNet will be able to track/trace/monitor events, NPCs, data during gameplay so that all features work."

1. **Research extraction** (kind 1): offline, on the owner's PC, from the owner's own install. It gives documentation and leads, and is never shipped.
2. **The framework's data** (kind 2): the data files EldenNet builds and ships for its functions, made from kind 1's leads and verified (O48) before EldenNet relies on them.
3. **Live tracing during play** (kind 3): what the GAL and Augur read and detect while the game runs, on which every feature depends.

### 1.1 Research extraction (kind 1)

What the game's own files say, read on the owner's PC so that names and text never leave it.

- **Sources.** Smithbox's exports from the owner's install (O55): param CSV (NpcParam, TalkParam and others) and text JSON of the `item` containers, which hold NpcName and PlaceName (ER08 §4.1), read by our own tools with Python's standard library (ER08 §4.2). Smithbox has no file export of map placements or event scripts. Entity and model ids could come only through its loopback server's read calls, which read the maps Smithbox has loaded (O56 option (c), not recommended; ER08 §1, §3.1). Boss-bar ids have no usable route, since Smithbox gives the event scripts only raw and compressed, and flag blocks have none at all (ER08 §1). Public datasets are leads for cross-checking only (ER08 §2, §6), and the research notes (ER01–ER08) record what is known in our own words.
- **Tools.** Now: the dry run, `tools/gamedata/smithbox_inspect.py`, which prints counts and checks only (O56's discussion; roadmap S6.3.e). Whether a research script that writes leads follows before the blueprint is O56 (Open). After the blueprint and O46: `en-tools roster-scan`, `identity-fill`, `fmg-import` and `kb-draft` (ARCH §12.3; DESIGN §7.8 step 1). `fmg-import` also serves players: it turns an export a player made from their own install into their own text database, DESIGN §4.5's fallback to the in-memory table walk, kept on the player's PC and never shipped (ARCH §12.3; §1.3).
- **Outputs.** On the owner's PC: the census with names (`roster.local/census.toml`) and every export (`game-export/`), both git-ignored (`.gitignore`, its game-data block), and the text database `fmg-<lang>.db`, kept in application data outside any checkout (ARCH §12.3). Leaving the PC, after the privacy sweep and a numbers-only check: ids, numbers and our slugs, each with a source tag and its provenance (exe and regulation version, DLC installed, language, tool build, date; ER08 §1, §5).

### 1.2 The framework's data (kind 2)

What EldenNet ships so that its functions can recognise what they read: ids, numbers, our slugs, our labels and our words, never game text. Every entry that rests on a game fact carries its lead's source and a `verified` state (a `status` in the layouts, ARCH §6.3), and only a verified entry may unlock or give a voice.

| File | Game facts it holds | Leads from (kind 1) | Verified by (kind 3) | From |
|---|---|---|---|---|
| `gal/patterns.toml`, `gal/layouts/<build>.toml`, `builds.toml` | AOB patterns, offsets, copy sizes and row sizes per build | ER01, ER05; `probe-exe` offline; the probe build's invariant search (DESIGN §4.6) | S0-A; each read group's self-test at phase A and B; the patch drill | v0.1 |
| `identity/personas.toml` | each persona's NpcParam rows per form with `expect_name_id`, entity ids, flag block; talk code as metadata | ER04, ER07; `identity-fill` | S1; S6 for the block; the identity-pack check at every start | v0.2 (the pilot), v0.5 (all six) |
| `identity/characters.toml` | the roster index: id, kind, content, bindings (a row with `expect_name_id` and form, an entity id, a bar's `name_id` with `expect_param`), each `verified` | the census (ER08 §1) | S1 and S2 roster captures through `roster-verify`; release lint; the roster identity-pack check | v0.4 |
| `identity/archetypes.toml`, each card's `match` (`archetypes/<card>.toml`), `identity/humanoid_models.toml` | the cards' param ids and ranges (resolution step 3) and the humanoid model ids (step 4, with their label heights, which are ours) | ER04 §2.7; the census | S1 (the families resolve; the up axis), S7 at the camps for each card | v0.4 |
| `identity/speech.toml`, `identity/types.toml` | param families and ranges, model ids, the nomadic merchants' talk code, the per-boss and per-kind `scripted` records | ER04 §2.7, §2.9; the census | S1 (families), S3 (`scripted`), S7 (cards) | `speech.toml` with the first voice; `types.toml` as a file at v0.4 |
| `teams.toml` | the hostile team set; the team-pair table | ER05 §6.2 | S7; the table by S7's additions | v0.1; the table v0.4 |
| `regions.toml` | PlaceName ids and map areas mapped to our region keys; the Sky column; our phrases | ER05 §4, ER04 §3.1; PlaceName ids from the census (ER08 §1) | the `place` self-test; S1 (place ids in legacy dungeons and the DLC) | v0.2 |
| `flags/catalog.toml` | aliases to flag ids or block offsets, with kind, `ng`, `src`, `verified`, spoiler and our own note (never a developer label) | ER04, ER05 §7, ER07; public leads (ER08 §6) | S6 through `flag-verify`; descriptor-map validation at phase B; release lint | v0.1 (core aliases), v0.5 (all verified or dropped) |
| `world/stages.toml`, `world/tracks.toml` | the world stages and tracks, by alias | ER07 | S6's world-awareness items (MEM §12.12) | v0.2 |
| `augur/*.toml`, `barks/triggers.toml` | rules over signals and aliases; no game id of their own | ours | golden replays | v0.1, v0.4 |

The evidence that verifies an entry is itself numbers only: `docs/design/spikes/S6/evidence/` for flags (MEM §6.3) and `tests/roster/` for bindings (DESIGN §7.8). Display names never ship: they come from the player's install at run time (O43, DESIGN §7.2).

### 1.3 Live tracing during play (kind 3)

What EldenNet reads and detects in the running game, on the player's PC, without writing anything or calling game code (DESIGN §0 items 2–5).

- **Reads** (DESIGN §5): the `WorldSnapshot` every 250 ms (session, lifecycle, HUD and talk fields, the Tarnished, place, clock, boss bars and up to 32 characters within 40 m, nearest first); the flag frame on every fourth snapshot, about 740 ids at 1 Hz; the render feed while labels show; runtime text and param rows on demand (`ResolveText`, `ResolveParams`); and the grace and place rows exported once the world is ready. Where the table walk cannot serve text, a player's own `fmg-import` database is the fallback (DESIGN §4.5; §1.1).
- **Detection** (DESIGN §6): Augur's sources, `SnapshotDiff` (each snapshot against the last processed one), `FlagWatch` (flag edges against the catalogue), `Lifecycle`, `Pipeline` and `Timer`, feed the world model (trackers, engagement, squads) and the declarative rules, which emit typed events with frozen audiences. Hook sources are reserved and compiled out in v1.
- **Health:** eleven read groups, each with a self-test and a breaker, and the degradation ladder (DESIGN §4.3, §4.4). A group that fails turns off only what needs it.
- **Capture tools** (dev builds): recordings (`*.ensnap.zst`, ARCH §7.5), the F11 debug view with its unknown-characters list, wide watch and Mark key (MEM §6.3), and `flag-verify` and `roster-verify`, which turn captures into the evidence of §1.2.

### 1.4 Side by side

| | Research extraction (1) | The framework's data (2) | Live tracing (3) |
|---|---|---|---|
| **Holds** | What the game's files say: rows, name ids, text, counts, with provenance | Ids, numbers, our slugs, labels and words, each game fact with `src` and `verified` | What the running game shows now: characters, flags, bars, place, time, combat, statuses; the events Augur makes of them |
| **Made by, where** | The owner, on their PC, from their own install, with Smithbox's exports and our research tools | Designers and authors, in the repository's `gal/` and `content/eldenring/` | The GAL and Augur, in the game process on the player's PC; testers' captures |
| **May be committed or shipped** | Research notes in our own words; a few ids cited as examples with their source; numbers-only lead files after the sweep and a numbers-only check (where they live before the blueprint is O56; ER08 §7 item 3) | All of it, shipped with EldenNet, once lint passes; only verified entries may speak or unlock | Redacted recordings: `TextEntry.text` as placeholders, and the `new_params` rows (`ResolveParams`' answers and the Ready export's grace and place rows) replaced by rows from a fixture table of our own, as names come from `tests/fixtures/names.toml`, keeping only ids a feature binds (until ARCH §7.5 does this, §3.1 gap 5, no recording that holds `new_params` rows is committed); golden event files and evidence records (numbers only, no game text, no personal data) |
| **Never** | Game files, FMG text, params as dumps or whole columns, developer labels, keys, Oodle, Smithbox or any part of it, an id list copied from any source (CLAUDE.md, Licences; ER08 §5) | Game text or names, developer labels, whole param columns, anything from fromsoftware-rs or me3 (O47, O48) | An unredacted recording in the repository; the player's name or SteamID (hashed in recordings); the player's store; any upload (DESIGN §1.2, "Never") |
| **Verified how (O48)** | Never by itself: the tool's own fail-closed checks (strides, versions, matching provenance) prove consistency, not truth, and two readings agreeing never set `verified` (ER08 §4.4) | Through evidence from kind 3, applied by a person; release lint requires it | Self-tests per read group at phase A and B; spikes S0–S8; captures; golden replays; the patch-day smoke route; the identity-pack checks |
| **When** | Now, before the blueprint (ER08, the dry run); the census tools after the blueprint and O46 (roadmap S9.1, S9.2) | From v0.1 (`gal/`, core aliases), growing to v1.0, when every binding is verified or its bio held (DESIGN §16.2) | From v0.1 Eyes, the first build; every spike runs on it, so it waits for the blueprint (roadmap S10 follows S8) |

### 1.5 How one fact moves between them

A character's NpcParam row, with `R` for the row, `N` for its name id, `E` for its entity id and `<id>` for our slug:

| Step | Kind | Where | What happens | Check |
|---|---|---|---|---|
| 1. Lead | 1 | the owner's PC | Smithbox's NpcParam export gives row `R` with name id `N`; the census keeps the name beside them in `roster.local/`; a numbers-only lead (`R`, `N`, the form, provenance) is written | the tool's own checks; the privacy sweep and a numbers-only check before the lead leaves the PC |
| 2. Binding | 2 | the repository | `identity/characters.toml` gains, under `<id>`, `{ param = R, expect_name_id = N, form = "default", verified = false, src = "owner-export" }` | lint; release lint rejects any binding that would speak unverified; in play the NPC resolves as if the profile did not exist (DESIGN §7.7) |
| 3. Sighting | 3 | the owner's game, at a prepared save | an S1 roster capture: the snapshot carries the character's handle, row `R` and entity `E`; the GAL reads row `R`'s live name id through `SoloParamRepository`; the F11 sightings record the keys | the `world` and `params` self-tests on this build |
| 4. Evidence | 3 to 2 | the repository | `roster-verify` matches the lead to the sighting and writes a record to `tests/roster/` (build, ids, verdict; no text); a person sets `verified = true` and cites it, and may add `E` as a second key | release lint requires the record |
| 5. Every start | 3 | every player's game | each row binding's live name id is compared with `N`; if more than 20% of the roster's row bindings miss, every roster bio goes quiet with one toast | the roster identity-pack check (DESIGN §7.8) |
| 6. In play | 3 | every player's game | resolution matches the live row and its live name id to `<id>`, which keys the bio, allowance slot, audiences, journal and roster entry; the name shown comes from the install's NpcName for `N`, never from steps 1–2 | DESIGN §7.2 step 2 |
| 7. Patch day | 2 and 3 | — | ids are data and change less than offsets (ER05 TL;DR); the census diff between game versions lists changed rows (ER08 §4.4); the self-tests and smoke route run again | the patch drill (DESIGN §4.6) |

A flag takes the same road: a lead in a research note (ER07), then a catalogue alias with `verified = false` and a `src` pointer, then an edge at a tester's mark in an S6 recording, then `flag-verify`'s evidence and `verified = true`; at every playthrough start the descriptor map confirms the id exists (MEM §6.2). Until it is verified it can hide content but never unlock it (MEM §6.4).

### 1.6 Rules at the boundaries

- **From 1 to 2:** numbers and ids only, each a lead with its source tag and provenance, and only the ids a feature binds (rows per form, name ids, entity ids, bar ids, flag-block bases), pending O56 and ER08 §7 item 3; never a whole column (ER08 §5). Names, text and labels stay in kind 1.
- **From 3 to 2:** only evidence records, applied by a person. A capture never edits a data file by itself: the probe build's draft layout and `flag-verify`'s proposed patch are reviewed first (DESIGN §4.6; MEM §6.3).
- **From 2 to 3:** the framework's data tells the live layer what to watch and how to recognise it (the watch list, bindings, ranges), but never stands in for a reading: a binding speaks only while the live row and its live name id agree with it, and a flag counts only as the frame reads it.
- **From 3 back to 1:** no game text and no param rows. Runtime text and the rows read at run time stay on the player's PC. A committed recording is redacted (ARCH §7.5), and its `new_params` rows are replaced by fixture rows of our own, keeping only ids a feature binds; ARCH §7.5 redacts text only so far (§3.1 gap 5). What a capture discovers (an unknown character's keys, a wide watch's edges) comes back as a proposed kind-2 entry for review (DESIGN §7.2 step 5; MEM §6.3), never as text.
- **Any method will do** (O57): a need may be met by a new source, a capture or a research lead, so long as it ends in a live reading that a self-test, a spike or a capture has verified.

---

## 2. The coverage map

### 2.1 How to read it

One table per family of v1 features. Each row is one piece of live data a feature needs during play.

- **Read or detected in play:** the GAL source (DESIGN §4.1, ARCH §6.1, §7.1) or the Augur source and rule (DESIGN §6.2, §6.5).
- **Framework data:** the kind-2 file it relies on (§1.2), or "—".
- **Verified by:** the self-test, spike, capture or replay that confirms it, with its roadmap item where it is a spike.
- **Status:**
  - **Backed**: designed and backed by research. The field or flag and its meaning are FACT in the research note cited, and nothing beyond the checks every read shares (S0-A's discovery, S0-B's task, S0-D's guards, its group's self-tests and the v0.1 golden routes) is needed before a feature relies on it. It is still a lead until those checks pass on the build (O48).
  - **Spike**: designed, with a spike pending. The source is designed, but its meaning, values or ids are leads that the named spike or capture must verify first; until then the feature fails closed (held, silent or Unknown).
  - **Gap**: no designed live source, or one that rests on a lead no planned check verifies (§3.1). A check covers a need only if its scope in DESIGN §16.1 names it: a pass rule that drops what is not verified (S6's "every alias used is verified, or dropped") does not count as coverage.
  - **Rows built on other rows** (their source is "as those rows", or a list of row ids, as in F-8 and the N family) carry the status of the spike that holds most of what they show, and name any row they draw on that is a Gap; each gap is counted once, under its own row.
- **Design:** the section that designs it.

The map covers game data only. Authored content (bios, seeds, prompts, our phrases), the player's settings and input capture are not game data. Spikes S5, S9 and S10 and the hook sources H1–H3 are later (DESIGN §16.1); no v1 row depends on them.

### 2.2 Start-up, the gate and lifecycle (A)

Every other row presumes these: no world read happens unless the build is known, discovery has succeeded and the gate is clean; while the gate is false only the `session` and lifecycle reads run, and the phase-A self-tests run on the first ticks, usually at the title (DESIGN §3.4, §4.3, §4.4).

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| A-1 | The game build | PE headers of the main module: versions, product name, timestamp, image size, sections | `builds.toml`; `gal/layouts/<build>.toml` | the gate (an unknown build stops at L1, C2); `probe-exe` offline | **Backed** · ER01 §1 | DESIGN §4.1, §4.4 |
| A-2 | Every object the GAL reads: 5 AOB items, 11 FD4 singletons, RTTI vtable sets | Interior-anchored AOBs, our FD4 finder, RTTI walks; no game call after task registration | `gal/patterns.toml`, layouts | S0-A (20 of 20 launches per mode), roadmap S10.1.b; each group's self-test | **Spike** S0-A · ER01 §4 gives the idiom | DESIGN §4.2; ARCH §6.2 |
| A-3 | A game-thread task that ticks safely | `register_task` once; the 4 Hz snapshot builder | — | S0-B (2 h, no fault, p99 ≤ 0.5 ms), S10.1.b | **Spike** S0-B · ER01 §3.2 | DESIGN §0 item 4, §5.1 |
| A-4 | Reads that never crash the game | SEH-guarded copies; a breaker per read group | layouts' copy sizes | S0-D (forced faults beside other mods), S10.1.b | **Spike** S0-D | DESIGN §2.6, §4.3; ARCH §6.5 |
| A-5 | Solo, offline, in the player's own world | `multiplay_role`, `is_my_world` (PlayerGameData); lobby state (CSSessionManager); non-NPC entries of `player_chr_set`; anti-cheat and ERSC modules | — | the `session` self-test (must pass); S0-C; S1 lists which `chr_type` values fill `player_chr_set` | **Spike** S0-C, S1 · fields FACT, ER05 §3 | DESIGN §3.4; ARCH §5.1 |
| A-6 | In the world, loading, at the title | `main_player` (WorldChrMan); screen state (CSMenuMan); load counter (CSNowLoadingHelper, its singleton to be confirmed in S0-A) | — | the `world` and `ui` self-tests | **Backed** · ER05 §9 | DESIGN §4.4, §6.2 (`Lifecycle`) |
| A-7 | The gate stays clean through an NPC summon and an NPC invasion (J-12, J-13) | The gate's fields, as A-5: `multiplay_role`, `is_my_world`, lobby state and the set count | — | none planned: S0-C checks the gate solo and in ERSC, and S1 lists only which `chr_type` values fill `player_chr_set` (§3.1 gap 9) | **Gap** · fields FACT, ER05 §3; the game has an `Invasion` multiplayer state (ER05 §3, SpEffect 9621) | DESIGN §3.4 |

### 2.3 Who is near: perception and identity (B)

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| B-1 | The characters near the Tarnished, nearest first (≤ 32 within 40 m) | `WorldChrMan.chr_inses_by_distance` (≤ 48 entries walked); each vtable checked against the RTTI set every tick | — | the `world` self-test; S1 (does the list hold open-field, block and summon sets at all distances; ER05 open question 9), S10.1.c | **Spike** S1 · container FACT, ER05 §6.1 | DESIGN §4.1, §5.1, §5.2 |
| B-2 | Each one's handle, row, model, entity id, `chr_type`, live team, HP, dead bit, position, block, on-screen bit, `last_hit_by` | `ChrIns` fields and modules; distances computed by us from Havok positions | — | the `world` and `combat` self-tests | **Backed** · ER05 §6.2 | DESIGN §5.2; ARCH §6.1, §7.1 |
| B-3 | Which persona a character is, in every form | Resolution steps 1–2: an exact row whose live name id equals its `expect_name_id`, or a non-zero entity id | `identity/personas.toml` | S1 (the pilot and three soldier families resolve); the identity-pack check at start (more than 20% misses turn personas off) | **Spike** S1 | DESIGN §7.2; NPC §2 |
| B-4 | Which card a generic humanoid is | Step 3, a param range or family; step 4, a humanoid model with a live hostile team gives the fallback card | each card's `match` (params and ranges, unverified; `identity/archetypes.toml`) for step 3; `identity/humanoid_models.toml` and `teams.toml` for step 4 | S1; S7 at the camps for each card | **Spike** S1 · family names are community labels, ER04 §2.7 | DESIGN §7.2, §7.4 |
| B-5 | Its kind: NPC, NPC summon, invader | `chr_type` | `types.toml` built-in rules (b)–(d) | S1 (the value set in our builds; whether NPC invaders are `PlayerIns` or `EnemyIns`, ER05 open question 12) | **Spike** S1 · names FACT, ER01 §5.2 | DESIGN §7.6; NPC §1.3, §1.4 |
| B-6 | Its kind: the mount | model id 8000 | — | the `world` self-test; S1 lists it with the other tags | **Backed** · ER05 §3 | NPC §1.4 |
| B-7 | Its kind: a spirit summon | `chr_type`; membership of the summon set is not read | — | S1 | **Spike** S1 · container FACT, ER05 §6.1 | NPC §1.4 |
| B-8 | Its kind: a boss now (it holds a bar) | `CSFeMan` boss bars (`handle`, `name_id`, damage taken), cross-checked with `boss_fight_active` | — | the `ui` self-test; v0.1 goldens; S2 (≥ 10 fights, one with two bars) | **Spike** S2 · fields FACT, their live meaning LIKELY, ER05 §8.4 | DESIGN §4.1, §7.6 |
| B-9 | Whether it may be perceived, speak or remember | The resolved key | `speech.toml` (`perceivable`, `may_speak`, `may_remember`) | S1 for the families; release lint | **Spike** S1 · silent families, ER04 §2.9 | DESIGN §7.5 |
| B-10 | The name it shows | NpcName text for the live row's name id, from the in-memory FMG table walk (`MsgRepository`); where the walk cannot serve, the player's own `fmg-import` database (local, never shipped) | — | the `text` self-test (≥ 20 tables; known ids give 1–64 printable characters); S2 (100 ids match the player's export), S10.1.c | **Spike** S2 · table layout FACT on 2.6.2.0 only, ER01 §8.3 | DESIGN §4.5, §7.2 |
| B-11 | The install's text language | Where the game holds its active language | — | S2; else the wizard's confirmation | **Spike** S2 | DESIGN §16.1 (S2); O26 |
| B-12 | Unknown characters and sightings (for the debug list and the player's own profiles) | The keys of unresolved or seen characters, from B-2 | — | our own record; no extra check | **Backed** · ER05 §6.2 | DESIGN §7.2 step 5, §7.7 |
| B-13 | Where a character is on screen (cone, labels, the talk marker) | `CSCamera` matrix, field of view and aspect; the render feed of ≤ 8 handles | — | the `camera` self-test; its singleton to be confirmed in S0-A | **Backed** · ER02 §4.1, ER03 §A | DESIGN §4.1, §5.1 |

### 2.4 The roster and character profiles (C; O43–O46)

The roster's membership (who has a personal name, which name id a first meeting shows, the "Boss" tag) is kind 1 and kind 2 only: it is settled by the census and by review (DESIGN §7.8; ER08 §1). The name text itself is read live from the player's install (N-4; GraceUI D67).

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| C-1 | Which roster profile a character is | Resolution step 2: a verified character binding (an exact row with its live name id, or a non-zero entity id) | `identity/characters.toml` | S1 roster captures and `roster-verify`, in batches from v0.4 to the v1.0 release candidate (roadmap S9.2, S14.2, S15.1, S16.1); release lint | **Spike** S1 | DESIGN §7.7, §7.8 |
| C-2 | Which named boss holds a bar | The bar's `name_id` and its holder's live row (`expect_param`), valid only while the bar shows | bindings `bar`, `bar.expect_param` | S2 roster captures (every named boss's fight once); S2's multi-bar fights (ER05 open question 13) | **Spike** S2 · ER05 §8.4 | DESIGN §7.7, §7.8 |
| C-3 | Every form of one NPC under one id (reveal rows, hostile and summon forms) | Resolution, per form | bindings per form; the alias table | S1 captures "the forms a reveal or a hostile turn uses"; S1's check of whether a boss's phases use one row or several | **Spike** S1 | DESIGN §7.8, §16.1 (S1) |
| C-4 | Whether a named boss or invader speaks scripted lines (so its combat lines may play) | S3's per-kind detector (J-11) | `types.toml` `[[boss]]` and `[[named]]` records | S3 records: ≥ 30 lines detected per kind, or `scripted = "none"`, during the S1 and S2 captures | **Spike** S3 · §3.2 risk 1 | DESIGN §7.8, §9.10 |
| C-5 | Whether an overhaul has remapped the roster's rows | Every row binding's live name id (`NpcParam.name_id` through `SoloParamRepository`) | the index | at every start: more than 20% misses quiet every roster bio | **Backed** · ER05 §6.2 | DESIGN §7.8 |
| C-6 | The player's own character profile, bound from a sighting | The keys recorded at the sighting, compared with the live NPC on every resolution | the player's profile folder | the binding holds only while every recorded key agrees | **Backed** · ER05 §6.2 | DESIGN §7.7 |
| C-7 | Knowledge a bio unlocks by the world's state (`knows_when`) | Verified catalogue flags in the frame (E-1) | `flags/catalog.toml` | S6 | **Spike** S6 | DESIGN §7.8; O46 |

### 2.5 Place, time and the here and now (D)

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| D-1 | Where the Tarnished and each character are: block and position | `block_id` and the physics position of each `ChrIns` | — | the `world` and `player` self-tests | **Backed** · ER05 §4 | DESIGN §5.2 |
| D-2 | The place the Tarnished is in (`PlaceEntered`, block 9) | `FieldArea.map_place_name_id` | — | the `place` self-test (the id resolves); its behaviour in legacy dungeons and the DLC is ER00 §7's S1 question, which DESIGN §16.1's S1 does not carry (§3.1 gap 7) | **Gap** · field FACT, PlaceName ids LIKELY, ER05 §4 | DESIGN §4.1, §6.5 |
| D-3 | The region (Sky path, idle context, `awareness.home`, the region an NPC stands in) | The block's map area and the place id, mapped to a region key | `regions.toml` | the `place` self-test; the eval on any change to the file; the place-id part as D-2 (§3.1 gap 7) | **Gap** (the place-id part) · map-area ranges FACT, ER05 §4; PlaceName ids are census leads, ER08 §1 | NPC §9.5; MEM §12.5 |
| D-4 | The same area, for audiences | The block's area byte in the open world, the full block indoors | — | golden replays | **Backed** · ER05 §4 | DESIGN §5.4 |
| D-5 | Place names | PlaceName text from the FMG table walk; where the walk cannot serve, the player's own `fmg-import` database (local, never shipped) | — | the `text` self-test; S2 | **Spike** S2 · ER01 §8.3 | DESIGN §4.5; MEM §2.2 |
| D-6 | Graces lit and the nearest grace | `BonfireWarpParam` and `WorldMapPlaceNameParam` rows, exported once the world is ready; each grace's flag on the watch list | derived grace ids | the `params` self-test; S6's grace-family check (10 sampled graces rise when lit) | **Spike** S6 · the paramdef FACT, ER05 §4 | DESIGN §6.5 (`GraceLit`); MEM §6.2, §6.3 |
| D-7 | The time of day | `WorldAreaTime` clock | our seven time words | the `place` self-test (hour < 24) | **Backed** · ER05 §5 | DESIGN §5.5; NPC §9.5 |
| D-8 | The weather (a word in block 9, idle context) | The weather family flags 570–575 | catalogue `weather.<w>` | none planned: S6's pass rule would drop the aliases, but its scope does not name them (§3.1 gap 4) | **Gap** · names FACT, upkeep LIKELY, ER05 §5 | DESIGN §5.5; NPC §7.3, §9.5 |
| D-9 | Resting at a grace (`GraceRested`, a sheltered place) | Flag 9000 | `rest.at_grace` | S6's core flags (v0.1), S10.1.c | **Spike** S6 · FACT, ER05 §3 (the grace talk script sets and clears it) | DESIGN §6.5, §8.1 |
| D-10 | At the Roundtable (a sheltered place) | SpEffect 9621 in the Tarnished's list | — | the smoke route's Roundtable step; golden replays | **Backed** · ER05 §3 | DESIGN §5.2, §8.1 |
| D-11 | The Tarnished's look: body type and gear | `PlayerGameData` body type and six equipped `ItemId`s | — | the `player` self-test | **Backed** · ER05 §2 | DESIGN §5.2, §5.5 |
| D-12 | The names of two visible pieces of gear | Item-name text for each `ItemId` category | — | none planned: the `text` self-test and S2 check names of characters and places only | **Gap** · §3.1 gap 3; categories a lead, ER05 §2, ER01 §8.3 | DESIGN §5.5; NPC §7.2 |
| D-13 | The Tarnished's condition (HP, FP and stamina bands, riding, mounting) and each NPC's HP band | Data module values; the Tarnished's `ride_state` 5 (`PlayerIns` only) | — | the `player` self-test; golden replays | **Backed** · ER05 §3, §6.2 | DESIGN §4.1, §5.2, §5.5, §6.5 (`MountChanged`) |
| D-14 | The Great Rune count | Flags 180–187 | catalogue | S6's core flags | **Spike** S6 · FACT, possible lag LIKELY, ER05 §2 | DESIGN §5.5, §10.3 |
| D-15 | An NPC on horseback (the *on horseback* tag of NPC §7.2's `brief`) | Nothing designed: the GAL reads the ride state for the Tarnished only, and `Chr` holds no riding field (DESIGN §4.1, §5.2) | — | none planned (§3.1 gap 11) | **Gap** · the ride field FACT for the Tarnished, its values from one crate's doc comment, ER05 §3 | NPC §7.2 |

### 2.6 Quest and world awareness (E; O51)

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| E-1 | The flag frame (about 740 ids at 1 Hz) | `CSEventFlagMan`: `id / 1000` to the descriptor map, then the block, byte and bit; a `valid` bit per id | the catalogue's watch list (≤ 2,048) | the `flags` self-test (divisor 1000, flag 100 set, every core alias valid, more than 10% of non-DLC aliases missing fails the group); validation at every playthrough start | **Backed** · ER01 §9 | DESIGN §4.5, §5.1; MEM §6.2 |
| E-2 | Flag edges, with alias, provenance and verified bit | `FlagWatch` against the catalogue | `flags/catalog.toml` | golden replays | **Backed** · ER01 §9 (polling a curated list) | DESIGN §6.2 |
| E-3 | The nine world stages | Outcome flags: boss kills, the star, the burning, the ash | `world/stages.toml` | S6 items 1–5 (MEM §12.12); `castle_fallen` with the pilot (v0.2), S11.1.b | **Spike** S6 · ER07 §2, §3 (the star and the ash LIKELY) | DESIGN §10.6; MEM §12.4 |
| E-4 | Boss defeats (`boss()`, a victory, `slew:<boss>` claims) | Catalogue boss flags | catalogue | S6's core flags | **Spike** S6 · TGA labels FACT, ER05 TL;DR, ER07 §3 | DESIGN §6.5, §10.3, §10.4 |
| E-5 | A persona's own stage | Its 20-flag block | `personas/<id>.toml` stages; `flag_block` | S6 per persona (the pilot at v0.2, all at v0.5), with each block's stage semantics | **Spike** S6 · ER07 §4, ER04 §4 | DESIGN §7.3, §10.6; MEM §12.12 items 6, 9 |
| E-6 | A persona's hostility bits (+1, +2, +3) | Flags of its block | the persona file | S6 | **Spike** S6 · labels FACT, ER04 §4.5, ER07 §4.1 | DESIGN §8.1 |
| E-7 | Related quests: tracks no persona owns | A track's block, or the festival's own flags | `world/tracks.toml` | S6 items 7 and 10; until then the ties stay deferred | **Spike** S6 · ER07 §4.8, §4.9 (headers only) | DESIGN §10.6; MEM §12.12 |
| E-8 | Journeys and NG+ | `ng_lvl`; each alias's `ng` (reset or keep) | catalogue `ng` | S6 item 5 (one NG+ recording) | **Spike** S6 · `ng_lvl` FACT, ER05 §2 | DESIGN §10.5; MEM §12.9 |
| E-9 | What each profiled NPC knows of it (`known(P)`, `known(C)`) | A pure rule over E-3 to E-8, its region (D-3) and `world_seen` | `regions.toml` (Sky), `awareness.home`, ties | MEM §12.11's tests; golden replays | **Spike** S6 (its inputs) | DESIGN §10.6; MEM §12.5 |
| E-10 | The DLC's aliases | The same read; an invalid DLC alias reads Missing silently | catalogue `dlc = true` | validation at playthrough start; S6 in DLC saves for v1.1 | **Spike** S6 · no public DLC block dump, ER08 §1 | MEM §6.2; DESIGN §16.2 (v1.1) |

### 2.7 Conversation (F)

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| F-1 | Whom the player means: lock-on target, a 25° cone within 8 m, the nearest within 5 m | `PlayerIns.locked_on`; positions (D-1) and the camera (B-13) | — | the `player` and `camera` self-tests | **Backed** · ER05 §3, §6.2 | DESIGN §8.1 |
| F-2 | Native talk open, ended, and with whom | Popup talk id, the talk camera, `hud == PopupMenu` with a persona within 3 m | — | S3 (≥ 95% precision over 30 events), S11.1.b; fail-closed rules until then | **Spike** S3 · fields FACT, ER05 §9; which NPC is talking needs a talk instance, unmapped without a hook | DESIGN §6.5 (`NativeTalkStarted`), §8.7 |
| F-3 | Conversable: alive, the stage allows chat, not hostile, not attacked in 30 s, no native talk within 8 m | Dead bit; live team against the hostile set; the hostility bits where verified (E-6); `Struck` by the player; F-2 | `teams.toml` | S7 (team usage), S6 (bits), S3 | **Spike** S7 · team enum FACT, usage LIKELY, ER05 §6.2 | DESIGN §5.3, §8.1 |
| F-4 | Safe context: alive, not riding, no boss fight, not in combat, no hostile near, `hud == Default`; sheltered places | D-9, D-10, D-13, B-8, H rows, the HUD state | — | S7 for the combat signal | **Spike** S7 · H-10 (cutscenes) is a gap | DESIGN §8.1 |
| F-5 | Danger while typing | `Engaged` (≥ 0.6) within 25 m; `PlayerDied`; loading | — | S7 | **Spike** S7 | DESIGN §6.5, §8.1 |
| F-6 | A name the player gives in chat | The Tarnished's name, read once per playthrough | — | the `player` self-test (valid UTF-16, 1–16 characters) | **Backed** · ER05 §2 | DESIGN §5.2, §10.2 |
| F-7 | "You, now": the NPC's own wounds and fighting | HP bands (D-13); engagement (J-3) | — | S7 | **Spike** S7 | DESIGN §16.2 (v0.2); NPC §7.3 |
| F-8 | Here and now (block 9): place, time, weather, the Tarnished's gear, nearby others by their `brief`, "just now" lines | D-2, D-7, D-8, D-12, D-15, B-1 to B-3, C-1, Augur's K events | — | as those rows | **Spike** S1 (resolution) · D-2, D-8, D-12 and D-15 are gaps | DESIGN §8.3; NPC §7.3 |

### 2.8 Speaking up, pacing and native-talk coexistence (H; O28–O42)

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| H-1 | Someone came near (`CameNear`), and the ring in which a line is prepared | Positions and our own distances | `settings.toml` ranges | golden replays (`speak_up.scn`) | **Backed** · ER05 §6.2 | DESIGN §6.5, §8.9 |
| H-2 | The autonomy tick for each NPC in range | `Timer`, phase-staggered per handle | `settings.toml` | golden replays (`autonomy_bound.scn`) | **Backed** · ER05 §6.2 (positions) | DESIGN §9.8; ARCH §8.2 |
| H-3 | Quest business: a stage that waits on the Tarnished talking with a persona (`beckons()`) | The active stage (E-5), `StageChanged`, `NativeTalkEnded` (F-2), the stage's `talked` flag | persona `beckon`, `beckon_src`, `talked` | S6 per persona and stage; S3 with S6 for the 10 s settle window | **Spike** S6 | DESIGN §8.9; MEM §12.14 |
| H-4 | A persona awake, asleep or senseless by its stage | The active stage's `state` | persona `state`, `sleep_leads` | S6 (the pilot's and one other persona's open sleep leads) | **Spike** S6 | DESIGN §8.9 |
| H-5 | A persona's native approach line, played or pending | Per persona and stage: whether one plays, its radius, its played flag | persona `approach`, `approach_m`, `approach_played` | S3 per persona and stage | **Spike** S3 | DESIGN §8.9 |
| H-6 | No native talk or scripted line within 20 m, and no unrecorded talker within 20 m | F-2; the scripted-line detector (J-11); the kinds of nearby NPCs (B-4, B-5) | `types.toml` `scripted` | S3; S7 for the cards | **Spike** S3 · §3.2 risk 1 | DESIGN §8.7, §9.4 |
| H-7 | News raised unasked (reaction cues) | World stages known (E-9), proved claims, the journal | `awareness.news` | S6 for the stages | **Spike** S6 | DESIGN §10.6; MEM §12.13 |
| H-8 | Display gates: not loading or dormant, `hud == Default`, no banner, the subtitle band | Lifecycle (A-6); `hud_state`, banner id, `show_subtitles` | — | the `ui` self-test (HUD state 0–3, banner in range); S3's talk-field recordings for the HUD values (S10.1.d); the v0.1 goldens for the banners' meanings | **Spike** S3 · the HUD enum FACT, ER05 §9; that vanilla banners write these fields LIKELY, ER05 §3; the cutscene part is H-10 | DESIGN §8.7, §9.4, §12 |
| H-9 | Pacing's clock (gaps, windows, cooldowns in play time) | Play time | `[pacing]` | the `player` self-test (play time rising) | **Backed** · ER05 §2 | DESIGN §9.4 |
| H-10 | A cutscene is playing (presentation hidden, pages held, the chat box closed under the danger rule) | `hud == HideAll` (`hud_state`, every snapshot) | — | none planned: S3 records the talk fields only "during menus, greetings and boss lines" (§3.1 gap 6) | **Gap** · the enum FACT, ER05 §9; its use in cutscenes LIKELY (DESIGN §12); ER00 §7's cutscene question (its S8 row) is carried by no spike in DESIGN §16.1 | DESIGN §12; NPC §10.2 |

### 2.9 Combat and boss fights (J)

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| J-1 | Hostile to the Tarnished | The live team in the hostile set, never the param's team | `teams.toml` | S7 at three camps (≥ 90% within 1 s, ≤ 5% false), S10.1.c | **Spike** S7 · ER05 §6.2 | DESIGN §5.3 |
| J-2 | Hits on and by the Tarnished (`Struck`, `hard`) | HP deltas with `last_hit_by`; the received damage type | — | the `combat` self-test; golden replays | **Backed** · ER05 §8.2 | DESIGN §6.5 |
| J-3 | Engagement between the Tarnished and a hostile | A hit exchange (0.9); lock-on while closing or within 8 m (0.6); lock-on alone (0.3) | — | S7 at three camps | **Spike** S7 · no clean global, ER05 §8.1 | DESIGN §5.3 |
| J-4 | Hits, engagement and kills between NPCs | An NPC victim's `last_hit_by` naming another NPC; the team-pair table | `teams.toml` team-pair table | S7's additions (v0.4), S13.1.a | **Spike** S7 | DESIGN §9.10; ARCH §8.7 |
| J-5 | Low HP and heals | HP fraction crossings and rises | rule files | golden replays | **Backed** · ER05 §6.2 | DESIGN §6.5 |
| J-6 | Deaths: an NPC's, the Tarnished's | Dead bit rising; the death count; banner 5 | — | golden replays | **Backed** · ER05 §3, §8.3 | DESIGN §6.5 |
| J-7 | Critical hits | Throw states (the Tarnished attacking, the hostile thrown) | — | the `combat` self-test (states 0–6); golden replays | **Backed** · ER05 §8.5 | DESIGN §6.5 (`CritLanded`) |
| J-8 | A foe fled | Distance growth over a window | — | golden replays | **Backed** · ER05 §6.2 | DESIGN §5.3 |
| J-9 | Sides and squads | The same card, or the same live team | `teams.toml` | S7 | **Spike** S7 | DESIGN §9.10 |
| J-10 | A boss fight started, and how it ended | Bars non-empty or `boss_fight_active`; a victory by banner 1–4 or 22, or a boss flag rising within 10 s; a death by `PlayerDied` | catalogue boss flags | v0.1 goldens; S2; S6's core flags | **Spike** S2, S6 · fields FACT, live meaning LIKELY, ER05 §8.4 | DESIGN §6.5; ARCH §8.5 |
| J-11 | Whether an NPC is speaking scripted dialogue now (combat and boss lines) | S3's per-kind detector from the talk fields and the talk-instance leads | `types.toml` `scripted` | S3 per kind (≥ 95% of ≥ 30 lines within 0.3 s; no false report over 10 min), S10.1.d, S13.1.b | **Spike** S3 · §3.2 risk 1; ER00 §7 (S3's open question) | DESIGN §9.4, §9.10 |
| J-12 | A persona fighting beside the Tarnished (`FoughtBeside`) | The persona's summon-form row, its `chr_type`, a boss victory | `personas.toml` summon rows | S1 | **Spike** S1 · the gate through an NPC summon is A-7, a gap | DESIGN §6.5, §7.1 |
| J-13 | An invasion, and a bound invader | The invader class; `player_chr_set`; every gate field through the invasion (A-7): if any flips, EldenNet goes dormant and a bound invader never speaks | built-in rule (b); bindings | S1 for the class and the set's `chr_type` values; the other gate fields: none planned (§3.1 gap 9) | **Spike** S1 · ER05 open question 12; A-7 is a gap | DESIGN §7.6, §3.4 |

### 2.10 Status and consciousness (K; S8)

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| K-1 | The Tarnished's statuses | `resistance_gauges[7]`, `proc_status_timers[7]` | §9.9's status set | S8 items 1 and 4, S10.1.d and S13.1.b | **Spike** S8 · fields FACT, slot meaning open, ER05 §3 | DESIGN §9.9 |
| K-2 | Each NPC's statuses | The SpEffect list of every snapshot character, classified by a `SpEffectParam` field; the optional `status` read group | classification rows in `params` | S8 items 2–4 and 6 | **Spike** S8 · ER05 §6.2 | DESIGN §4.3, §9.9; ARCH §6.1 |
| K-3 | Asleep or senseless at run time (`conscious`), which every card's and character's calm lines wait for (O44) | The sleep ailment's SpEffect; resting or lying animation ids; throw state 4 | — | S8 item 5 (≥ 95% over 20 cases) | **Spike** S8 · §3.2 risk 2; animation field FACT, ER05 §6.2 | DESIGN §8.9, §9.9 |

### 2.11 NPC-to-NPC exchanges and memories of others (L)

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| L-1 | Two NPCs near each other on the same side (`greet_npc`) | `CameNear{X, Y}`; the same card or the same live team (J-9) | — | golden replays (`chatter.scn`); S7 for the same side, S1 for the card | **Spike** S7 · team enum FACT, usage LIKELY, ER05 §6.2 | DESIGN §9.7 |
| L-2 | Pairs that are not hostile though on different teams | The team-pair table | `teams.toml` | S7's additions | **Spike** S7 | DESIGN §9.7 |
| L-3 | Who hears a line (16 m) and who may answer | Positions, area (D-4), alive and conscious (K-3), voice source (B-9) | — | S8 for calm answers by cards and characters | **Spike** S8 | DESIGN §9.7 |
| L-4 | Whom a line was by, to or about, as profile ids (keepsakes about other NPCs) | Resolution (B-3, C-1) | — | S1 | **Spike** S1 | DESIGN §7.8; MEM §2.5 |

### 2.12 Memory and persistence (M)

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| M-1 | Which playthrough file: SteamID, slot, character fingerprint | The save folder's name; `save_slot` (GameMan); name, archetype and starting gift (PlayerGameData) | — | the `player` self-test (slot 0–9) | **Backed** · ER01 §11, ER05 §2 | DESIGN §10.5; MEM §8.2, §8.3 |
| M-2 | The rollback clock | Play time | — | the trim test (#825) | **Backed** · ER05 §2 | DESIGN §10.5; MEM §9.1 |
| M-3 | The journey number | `ng_lvl + 1` | — | the `player` self-test; S6 item 5 for the flags (E-8) | **Backed** · ER05 §2 | DESIGN §10.5; MEM §9.4 |
| M-4 | Audiences: which profiled NPCs saw, heard or witnessed each event | Resolution, radii, the same area, conscious | — | S1 | **Spike** S1 | DESIGN §5.4; MEM §1.2 |
| M-5 | Facts: met and seen counts, first and last place and play time met, times attacked, fought beside, saw the Tarnished die | `PersonaSeen`, `Struck`, `FoughtBeside`, `PlayerDied`, D-2, M-2 | — | golden replays | **Spike** S1 | DESIGN §10.2; MEM §2.2 |

### 2.13 GraceUI's pages that show game state (N)

GraceUI reads only EldenNet's projections (GraceUI invariant 1), so each page takes the status of the rows it shows, by §2.1's rule for rows built on other rows. Its System pages show EldenNet's own health and carry no game data.

| Row | Need | Read or detected in play | Framework data | Verified by | Status | Design |
|---|---|---|---|---|---|---|
| N-1 | Top bar and Home › Status: the gate, the level and its reason, "you can talk now", the journey | A-5, A-6, F-4, M-3 | — | as those rows | **Spike** S7 (safe to talk) | GraceUI §4.6.1, §6.5 (`status`) |
| N-2 | Home › Nearby (≤ 2 Hz): perceived characters, a state word, wounds in words, statuses, "speaking", a met NPC's disposition | B-1 to B-10, C-1, D-13, K-1, K-2, J-11 | — | as those rows | **Spike** S1 | GraceUI §4.6.1, §6.5 (`nearby`) |
| N-3 | Home › Happening now and Recent events; Tools › Event monitor | Augur's short-lived and stored events, from every family's rows; admission decisions; only events the Tarnished perceived (N-9) | — | golden replays; each event as its source row | **Spike** S1 (the events' subjects and audiences) · N-9 is a gap | GraceUI §4.6.1, §4.6.8, §6.5 (`shortlived`, `monitor.*`) |
| N-4 | Characters › Named NPCs: the roster by the name the install shows, at the title screen too | NpcName text for the bar binding's `name_id` (a Boss-tagged entry) or the entry's `expect_name_id` (every other), while dormant at the title; else `display_name_fallback` | the index | none: the dormant flag allows no text reads, and S2 has no title-screen case (§3.1 gap 1) | **Gap** · §3.1 gap 1 | GraceUI §4.6.4, D67; GraceUI eldennet-changes §9 item 3 |
| N-5 | Characters › Playthrough: the people met, their status from perception (here now, alive when last met, away, fell), last met, the stage seen, the world now | B and M rows, E-5, E-9; "away" needs a record of where each was last met and a check on coming near it; "fell" needs a stored death event the Tarnished perceived (N-9) | — | "away": nothing designed in EldenNet (§3.1 gap 2); "fell": as N-9 | **Gap** · §3.1 gap 2; N-9 is a gap | GraceUI §6.5 (`playthrough`); GraceUI eldennet-changes §9 item 4 |
| N-6 | History › World: the world stages that hold, and who has heard | E-3, E-9 | — | S6 | **Spike** S6 | GraceUI §4.6.5, §6.5 (`world`) |
| N-7 | Characters › NPC types: kinds and types, counts near now, verification states | B-4 to B-9 | `types.toml` | S1 | **Spike** S1 | GraceUI §4.6.4, §6.5 (`kinds`) |
| N-8 | The menu closes at once in danger | The danger signals of F-5 | — | S7; GraceUI's in-game checks | **Spike** S7 | GraceUI §5.1 (invariant 9) |
| N-9 | Whether the Tarnished perceived an event (the `seen` bit), for the live views, History › Events and "fell" | Nothing designed: DESIGN §5.4's audiences cover profiled NPCs only, and MEM's `events` table has no `seen` column (MEM §8.7); the reads exist (D-1, D-4) | — | none planned (§3.1 gap 8) | **Gap** · GraceUI SP7; eldennet-changes §2 item 7 | GraceUI SP7 (§4.8); DESIGN §5.4 |
| N-10 | The game has announced an invader (SP7: never an invader before the game announces it) | Nothing designed: no signal of the announcement is chosen (a banner or message id, or the first sighting of the invader class, B-5, are candidates) | — | none planned (§3.1 gap 10) | **Gap** · GraceUI SP7 and its spoiler goldens | GraceUI SP7 (§4.8) |

### 2.14 Counts

| Family | Rows | Backed | Spike | Gap |
|---|---|---|---|---|
| A Start-up, gate and lifecycle | 7 | 2 | 4 | 1 |
| B Perception and identity | 13 | 4 | 9 | 0 |
| C Roster and character profiles | 7 | 2 | 5 | 0 |
| D Place, time, here and now | 15 | 6 | 4 | 5 |
| E Quest and world awareness | 10 | 2 | 8 | 0 |
| F Conversation | 8 | 2 | 6 | 0 |
| H Speaking up and pacing | 10 | 3 | 6 | 1 |
| J Combat and boss fights | 13 | 5 | 8 | 0 |
| K Status and consciousness | 3 | 0 | 3 | 0 |
| L NPC-to-NPC | 4 | 0 | 4 | 0 |
| M Memory and persistence | 5 | 3 | 2 | 0 |
| N GraceUI's pages | 10 | 0 | 6 | 4 |
| **All** | **105** | **29** | **65** | **11** |

By spike, the 65 rows wait on: S1 (17), S6 (16), S7 (12), S2 (6), S3 (6), S8 (4) and S0-A to S0-D (4), each row counted under its first-named spike; S2 and S6 share J-10, S0-C and S1 share A-5, and S7 and S1 share L-1. The 11 Gap rows are A-7, D-2, D-3, D-8, D-12, D-15, H-10, N-4, N-5, N-9 and N-10; rows built on them (F-4, F-8, J-12, J-13, N-3) keep their own status and name them (§2.1).

---

## 3. Gaps and risks

### 3.1 Gaps

Needs with no designed live source, or a lead no planned check verifies, and one boundary the design does not yet enforce (gap 5). Each is small, and none needs a new kind of read: gap 11's option (a) adds one field to an object already read. Gaps 3, 4, 6, 7 and 9 close mainly by adding a case to a planned spike's scope; gaps 1, 2, 5, 8 and 10 mainly by a design change; gap 11 by either.

1. **Roster names at the title screen** (N-4). GraceUI lists the whole roster from the start, at the title screen too, by the name the player's install shows; a Boss-tagged entry by the name its bar shows (GraceUI G5, G8, D67; its eldennet-changes §9 item 3). The names are NpcName text: for a Boss-tagged entry, the bar binding's `name_id`, which differs from the row's name id (bars use their own band of NpcName ids, set per map in the event scripts, ER05 §6.2); for every other entry, its `expect_name_id`. At the title EldenNet is Dormant, which allows only the `session` and lifecycle reads (DESIGN §4.4; ARCH §5.1), and no planned check shows that the text table can be read before a world loads. **Closes by** a design change: while Dormant at the title, once phase A's `text` self-test has passed, `ResolveText` look-ups of NpcName for those ids are allowed (no world object is read); at L1, with `text` off, or for an id that does not resolve, the roster shows `display_name_fallback`. S2's text check gains a title-screen case: a known NpcName resolves before any world loads. **Roadmap:** S7.1.e (the roster list), folded into DESIGN §4.4, ARCH §6.8 and DESIGN §7.8's GraceUI row; the S2 case in DESIGN §16.1 (S10.1.c).
2. **"Away" on the Playthrough page** (N-5). GraceUI shows a met NPC as away "when the Tarnished came within speaking range of where they were last met and did not perceive them", a small new record per profiled NPC (GraceUI eldennet-changes §9 item 4). Its "fell" status needs gap 8. EldenNet records the first and last place met as a place, not a position, and has no rule for coming near it. The reads exist (D-1, B-1). **Closes by** a design change: a per-NPC last-seen position and block in the facts (MEM §2.2) and an Augur rule, `CameNear` to that point with the NPC not perceived, that writes the away record; rollback trims it like any fact. **Roadmap:** S7.1.e (the Playthrough projection).
3. **Gear names** (D-12). Block 9 names at most two visible pieces of the Tarnished's gear from runtime text (DESIGN §5.5; NPC §7.2), but DESIGN §4.5 lists only the NpcName and PlaceName tables, and the `text` self-test and S2 check only those. The item categories' FMG slots are leads from one source on an older build (ER05 §2; ER01 §8.3 for goods). **Closes by** adding the item-name categories to DESIGN §4.5, to the `text` self-test's known ids and to S2's 100-id match against the player's export; until then the `observed` render leaves gear names out, as it does when `text` is off. **Roadmap:** S10.1.c (S2).
4. **Weather in S6's scope** (D-8). Weather words appear only when verified (NPC §7.3, §9.5), and S6's pass rule (every alias used is verified or dropped) covers the `weather.<w>` aliases, but S6's scope (DESIGN §16.1; MEM §12.12) does not name them, ER05's open question 4 (do the weather flags stay current?) is unanswered, and DESIGN §5.5 cites 550–575 where NPC §9.5 uses the family flags 570–575. **Closes by** naming the family flags in S6's core scope, with a recording through weather changes in the open world, and settling the range in DESIGN §5.5; until then the weather bucket stays `-`. **Roadmap:** S10.1.c (S6's core flags).
5. **Param rows in committed recordings** (kind 3 at the boundary, §1.4, §1.6). A recording's `snap` records carry `new_params`: `ResolveParams`' answers and, from Ready, the grace and place export, whose rows hold each grace's flag, text id, map area, grid and position (DESIGN §5.2; ARCH §6.1, §7.1; MEM §6.1), so every recording made after Ready holds those columns for every grace. ARCH §7.5's redaction replaces only `TextEntry.text` and profile text, and is forced on for `tests/recordings/`; CLAUDE.md never allows params to be committed, and ER08 §5 never lets them leave the PC as dumps or whole columns. **Closes by** a design change to ARCH §7.5: the redacted mode also replaces every `new_params` row with a row from a fixture table of our own, as `tests/fixtures/names.toml` serves names, keeping only ids a feature binds, and `en-tools lint` rejects a committed recording whose rows are not fixtures. **Roadmap:** S10.2.d (the v0.1 recorder), before S10.3.a's golden routes.
6. **Cutscenes** (H-10). DESIGN §12 hides presentation, and NPC §10.2 holds pages and closes the chat box under the danger rule, while `hud == HideAll`, which DESIGN §12 reads as a cutscene (LIKELY). ER05 §9 gives only the enum as FACT, not its use in cutscenes, and ER00 §7's question of how cutscenes and loading screens are detected (its S8 row) went to no spike in DESIGN §16.1. Until then H-8's `hud == Default` gate is right only if cutscenes leave the HUD in another state. **Closes by** a cutscene pass in S3's talk-field recordings, which already record `hud_state`: a few story and boss-entry cutscenes, with `hud_state`, the screen state and the banner id. **Roadmap:** DESIGN §16.1's S3 row; S10.1.d on v0.1's recordings, S11.1.b in game.
7. **The place id in legacy dungeons and the DLC** (D-2, D-3). ER00 §7's S1 asks how `map_place_name_id` behaves there, but DESIGN §16.1's S1 row does not carry the question, and the `place` self-test checks only that the id resolves; ER05 §4 rates the field as LIKELY a PlaceName id. **Closes by** adding the question to S1's row in DESIGN §16.1: record the id on entering and crossing two legacy dungeons and one DLC area (the DLC part at v1.1). **Roadmap:** S10.1.c (S1).
8. **What the Tarnished perceived** (N-9; N-3 and N-5 draw on it). GraceUI's live views and History › Events show only events whose audience includes the Tarnished (GraceUI SP7), and it asks for a `seen` bit on stored events, set when the Tarnished perceived the event (eldennet-changes §2 item 7), from which the Playthrough page's "fell" status is built (§9 item 4). DESIGN §5.4 defines audiences for profiled NPCs only, and no EldenNet design document has a `seen` bit. No new read is needed. **Closes by** a design change: a Tarnished audience rule on DESIGN §5.4's radii and same area, built from D-1 and D-4, feeding the projections, and a `seen` column in MEM's `events` table (MEM §8.7; a migration under MEM §10). **Roadmap:** S7.1.a (the bridge, eldennet-changes §2).
9. **The gate through an NPC summon and an NPC invasion** (A-7; J-12, J-13). The gate needs `multiplay_role == Host`, `is_my_world`, `LobbyState == None` and the set count (DESIGN §3.4). S0-C checks it solo and in ERSC, and S1 checks only which `chr_type` values fill `player_chr_set`; no planned check records the other fields while an NPC invades or an NPC summon fights beside the Tarnished, though the game has an `Invasion` multiplayer state (ER05 §3, SpEffect 9621). If any field flips, EldenNet goes dormant, a bound invader never speaks and `FoughtBeside` never fires. **Closes by** adding to S1's question in DESIGN §16.1 (or S0-C's): record every gate field (role, own world, lobby, the set count) through one NPC invasion and one NPC summon. **Roadmap:** S10.1.c (S1). Risk 7 is the set-count half.
10. **The invader's announcement** (N-10). GraceUI never shows an invader before the game announces it (SP7), and its spoiler goldens check that an unannounced invader changes no count. EldenNet has no signal for the announcement. **Closes by** a design choice of signal, checked in the same S1 invasion recording as gap 9: for example a banner or message id, if the game raises one, or else the first sighting of the invader class (B-5) within the snapshot's range, which then stands for the announcement. **Roadmap:** S7.1.a (the projections), with S10.1.c for the recording.
11. **An NPC on horseback** (D-15). NPC §7.2's `brief` tags nearby others *on horseback*, but the GAL reads the ride state for the Tarnished only (DESIGN §4.1, "player only"; the `Chr` struct of §5.2 has no riding field), and the field's values come from one crate's doc comment (ER05 §3). **Closes by** one of two design changes: (a) read the ride module's state for every snapshot character, with a layout entry, the `world` self-test and an S1 check on a mounted NPC; or (b) drop *on horseback* for NPCs from NPC §7.2, keeping it for the Tarnished. **Recommended: (b)** for v1: no research covers an NPC's ride module, and the tag is colour, not a feature. **Roadmap:** S10.1.c for (a); a content fix to NPC §7.2 for (b).

### 3.2 Risks

Needs that are designed but rest on a spike that may fail. In every case the design fails closed, so a failure costs a feature, never safety or a spoiler.

1. **Scripted lines and native talk, without a hook** (F-2, H-5, H-6, J-11, C-4). S3's observer reads the popup talk id, the talk camera and the HUD state, and its per-kind detector of scripted combat and boss lines adds the talk-instance leads (DESIGN §16.1, S3). Which NPC is talking, and scripted lines that open no menu, may need a talk instance, and without a hook the world talk manager is unmapped (ER05 §9; ER00 §7, S3's open question); no talk-instance read is in the GAL inventory (DESIGN §4.1). If S3 finds no hook-free signal for a kind, that kind's lines stay held: the boss lines of about ten base-game bosses with scripted lines, persona combat lines in their boss forms, and any calm line within 20 m of an unrecorded talker. If the observer fails outright, every persona own-initiative line stays dark; chat is unaffected (DESIGN §8.9). **Closes by** S3 (roadmap S10.1.d on v0.1's recordings, S11.1.b, S13.1.b). If it needs a talk-instance read, that read joins the inventory with its layout entry and self-test (a design change). The hook source `HookTalkInvoke` stays reserved for v1.x, after its own spike and an Arxan review (DESIGN §6.2).
2. **The sleep signal** (K-3, L-3). Every card's and character's calm own-initiative lines (greetings, idle remarks, exchanges, autonomy, calm status lines) wait on S8's sleep or senseless signal for its kind (O44): the roster's 130-odd NPCs besides the personas, though its bosses and invaders speak only combat lines anyway. If S8 cannot verify it for a kind, those NPCs still converse and speak combat lines, but never speak up calm. The SpEffect walk and the gauges are in the inventory as leads owed to S8; the animation queue that S8 item 5 names for scripted sleepers is not yet in it, and joins it only if S8 needs it (DESIGN §4.1, §9.9, §16.1). **Closes by** S8 (S10.1.d on recordings, S13.1.b in game).
3. **NPC against NPC** (J-4, L-2). Until S7 verifies `last_hit_by` for NPC attackers and the team-pair table, NPC pairs never engage, `FoeKilled` is not emitted and soldiers say nothing to the Tarnished's summons (DESIGN §9.10). **Closes by** S7's additions (S13.1.a).
4. **About 140 bindings by capture** (C-1 to C-4). Every roster NPC needs its binding verified in game: about 27–33 hours of captures at prepared saves, plus the play to make the saves (S2 alone needs about 40–60), on v1.0's critical path (K29). Smithbox gives rows and name ids only; entity, model, bar and flag-block leads wait for the captures, with public leads meanwhile (ER08 §1, §6). A binding not verified by the release candidate ships held. **Closes by** the captures (S9.2, S14.2, S15.1, S16.1); see §4.
5. **Every check waits for the blueprint** (all Spike rows). Every in-game check needs EldenNet running, so everything gathered before the blueprint stays `verified = false` (ER08 §4.4), and v0.1, the first build, follows the engine's adoption (roadmap S10 needs S8; O54). A live source that fails (risk 1 or 2) is found only then. See §4.
6. **The DLC** (E-10; v1.1). No public per-NPC dump of the DLC's flag blocks exists, both developer flag dumps stop before the DLC's maps, and its bindings need saves inside it (ER08 §1; ER04 §10). The design defers the DLC's roster to v1.1 (O46's recommendation) and reads a missing DLC alias as Missing. **Closes by** S6 and the roster captures in DLC saves (S17.1).
7. **The session gate in summon fights and NPC invasions** (A-5, A-7, J-12, J-13). If NPC summons or NPC invaders count in `player_chr_set`, EldenNet would go dormant in every summon-assisted boss fight and `FoughtBeside` could never fire; the gate counts only non-NPC entries, and S1 lists which `chr_type` values appear (DESIGN §3.4). The other gate fields (role, own world, lobby) could flip the same way, and no planned check records them through an NPC invasion or an NPC summon: that half is gap 9. If any field flips, EldenNet goes dormant and a bound invader never speaks. **Closes by** S1 (S10.1.c), with gap 9's addition to its question.
8. **Overhauls and text hooks.** A mod that serves text through a `GetMessage` hook can show names the raw tables lack (DESIGN §4.5), and one that remaps rows trips the identity-pack checks, which silence personas or the roster with a toast (DESIGN §7.2, §7.8). Designed fail-closed; the compatibility matrix tests it (v0.6, S15.1).

### 3.3 Scope boundaries (decided, not gaps)

- **Own-quest stages only for the six personas in v1.0.** Every other questline NPC knows the land as its ground and the sky show it, but not its own road, which needs a flag block per NPC verified by S6 and makes it a persona (O43 as updated, O46; DESIGN §7.8, §10.6).
- **No line of sight** in v1, because the ray-cast filter is undocumented (DESIGN §5.4; ER05 §6.2).
- **No AI target or battle state:** engagement is a heuristic, which S7 may later replace behind the same signal (DESIGN §5.3; ER05 §8.1).
- **No game-code hooks:** the flag-set, damage and talk-invoke hook sources are reserved and compiled out; no v1 row depends on them (DESIGN §0 item 3, §6.2).
- **Generics remember nothing,** so a generic's entity id of 0 (ER05 open question 1) costs nothing in v1 (DESIGN §10.2).
- **Talk ids are metadata,** never a key (DESIGN §7.2).

### 3.4 Does the design cover everything?

Not yet in full, though every shortfall is small. Every v1 feature has a designed live source for every piece of game data it needs, apart from the gaps of §3.1: four GraceUI asks that EldenNet's design has yet to fold in (gaps 1 and 2 under S7.1.e, gaps 8 and 10 under S7.1.a), five additions to the scope of S1, S2, S3 and S6 (gaps 3, 4, 6, 7 and 9), one NPC tag with no source (gap 11), and one boundary the recorder does not yet enforce (gap 5, no map row). Of the 105 needs mapped, 29 rest on research FACT and the checks every read shares, 65 wait on a named spike, and 11 are gaps. The spikes that carry the most weight are S1 (identity and the roster's rows), S6 (every world, quest and stage flag), S7 (hostility and engagement) and S3 (native talk and scripted lines); S3 and S8 are the two whose failure would cost the most, and both fail closed (§3.2 risks 1 and 2).

---

## 4. Questions for the owner

One question, which the designers cannot settle because it turns on O54's limit and on the owner's own play time. It would become O58; DECISIONS.md is not edited here.

**Preparing the live checks before the blueprint.** Live tracing is verified only in game (§1.4), and every capture waits for the first build, which follows the blueprint (§3.2 risks 4 and 5). Should anything be prepared now, without building EldenNet?

- **(a) Wait.** Prepare nothing until the blueprint; the capture plans and saves are made when v0.1's spikes start. Gives up: nothing now, but the saves and plans then sit on the critical path (K29).
- **(b) Prepare now, without building.** Write the capture plans in our own words (S6's expectation scripts as in MEM §6.3, the S1 and S2 capture routes and the checkpoint list, roster batch by batch), and the owner keeps a save before each named boss fight and each later form of a roster NPC as they play (about 40–60 for S2 alone, DESIGN §7.8): copies of the save file on their own PC, never committed, and kept apart from any save they take online, since a capture plays them offline with mods (DESIGN §3.6). Gives up: some of the owner's play time, and plans that may need a revision once the blueprint is known.
- **(c) As (b), plus a standalone read-only spike build** (the GAL, the recorder and the debug view, no model and no network) before the blueprint, so that S0, S1, S2, S3, S6 and S8 can run early. Gives up: O54's rule that nothing of EldenNet is built before the blueprint, and code that may need rewriting if the foundation the blueprint brings changes the language or the crates, even though the GAL's design survives it (DESIGN §7.8).

**Recommendation: (b).** It builds nothing, so O54 holds; it serves whatever the blueprint brings, since the GAL, Augur and identity by verified keys are unchanged by it (DESIGN §7.8); and it takes the slowest part of the captures, preparing the saves, off the critical path. (c) would find a failing live source sooner (§3.2 risks 1 and 2), but at the cost of code written before its foundation is known; it stays a later choice if the blueprint is long delayed.
