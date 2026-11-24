# The capture plans: what each in-game check records, where and when

*2026-11-24. Draft for roadmap S6.3.s, under O58 (b) as its note of 2026-11-16 amends it. The checks are DESIGN
§16.1's S1, S2 and S6, and the roster's captures (DESIGN §7.8). Our own words throughout: no game text, and no id that
EldenNet's design and research documents do not already hold. The open points are listed at the end.*

## 1. What this is

The owner decided O58 (b) on 2026-11-15: prepare the live checks in our own words before anything is built. On
2026-11-16 the owner dropped the prepared saves (O58's note): no copies of the save file are kept, the in-game checks
run during a normal playthrough once EldenNet runs in game, and a check that fails waits for its next chance. So every
plan below reads "when the player next reaches X, this check runs and records Y", never "load the save before X".
Nothing is restored, replayed or set up for a check.

**Who does what.**
- **The owner plays**, offline and without Easy Anti-Cheat, as in every round so far. Most checks ask nothing more.
  S6's checks (and S7's) also ask for a Mark, a press of Pause at the moment the script names: the launcher's Mark
  without GraceUI, GraceUI's once its hotkey holds Pause (O90; WHY§217).
- **EldenNet records**: the DLL writes its recording with `debug.record_sessions = true` (`docs/design/spikes/
  formats.md`), and the launcher stamps the Marks on the recording's clock.
- **The engine's tools read the recording on the owner's PC**: `flag-verify` for S6, `roster-verify` for the roster,
  `rec-stats` and the spike reports for S1 and S2 (ARCH §12.3).
- **What comes back** is numbers, fixed words and our own labels only, each file linted and swept before it leaves the
  PC (`docs/design/spikes/README.md`): spike reports, evidence records, `rec-stats`. Recordings stay on the owner's PC,
  where DataPiler imports them (S7.11.f). The one exception is v0.1's three golden routes (§3, S10.2.c), redacted
  before they are committed.

**Before the first capture.** A check is judged only once what it reads is live:
- every check: a ready start (the static and 36 fields WHY§459 counts, in WHY§460's order) and S0 passed; S1 and S2
  then run without GraceUI (O90). S0-B's two hours of normal play are the first session worth recording for them;
- S1: the world group whole, with the near characters' type, team, dead and on-screen fields that Lodestar's
  eighteenth round is to place (O117, O118); the player and session groups; the params group for each row's live name
  id;
- S2: the `text` group (the messages manager is placed, its fields are not) and the `ui` group's boss bars;
- S6: the flags group, whose reader already reads the game through the build's layout (§7), and the Mark;
- the roster: the census (`roster-scan`) and `identity/characters.toml`, from v0.4 (DESIGN §7.8).

**When a check fails, or its moment has passed.** The alias or binding stays `verified = false`, its script or route
stays open, and it runs again the next time the player reaches the same kind of moment: another grace, another fight,
another meeting. A moment that comes once in a playthrough (a boss's death, a quest's step) next comes in the next
cycle, which a normal playthrough reaches at its end (§2, K25). Meanwhile the unverified alias is silent (MEM §6.4)
and the unverified binding resolves as if its profile did not exist (DESIGN §7.7): waiting delays a feature, never
shows a spoiler.

## 2. The checkpoint list

The moments of a normal playthrough at which checks run. K1 to K5 recur in every session; K6 to K25 follow the
game's order as most playthroughs meet it, and the open world lets the order change (Caelid before Liurnia, say): each
checkpoint runs when the player gets there. "Recorded" is what the recording must hold for the check; what leaves the
PC is the tools' numbers (§1).

**K1. The title screen, before any world loads** (every session)
- Checks: S2's title-screen text case (DATA §3.1 gap 1) and the language field (§4, T1 and T3).
- The owner: nothing.
- Recorded: phase A's `text` self-test (tables found, known ids resolved: counts); whether a known NpcName id resolves
  before any world is loaded; the language field's value.

**K2. A save loaded** (every session)
- Checks: phase B's self-tests, `flags` among them (flag 100 set, every core alias's group present); `ng_lvl` and
  the save's slot read; S1's first snapshots (§3, R1).
- The owner: nothing.
- Recorded: the groups' states, the flag frame, the player's journey and slot. The world is told from the title by the
  main player's presence, never by the slot (WHY§575).

**K3. Resting at a site of grace** (every rest; the first few suffice)
- Checks: S6 `rest.at_grace` (9000; §5.1).
- The owner: a Mark on sitting down and a Mark on standing up.
- Recorded: 9000's edges and every watched id that moved within ±5 s of a Mark.

**K4. Lighting a grace not lit before** (until ten have been lit while recording)
- Checks: S6's grace family (MEM §6.3: 10 sampled graces rise when lit).
- The owner: a Mark as the grace lights.
- Recorded: the derived `grace.<row>` flag's rise.

**K5. A change of weather in the open field** (whenever it comes)
- Checks: S6's weather flags, if DATA §3.1 gap 4 puts them in S6's core scope (§5.1).
- The owner: a Mark when the rain starts and when it stops.
- Recorded: a wide watch over the weather range (550–575; see the open point on the range).

**K6. The Church of Elleh and the road north to the Gatefront** (Limgrave, the first stretch)
- Checks: S1 R1 (a golden route, §3); Kalé resolves (S1's pass); S6's pilot block, 4700, at his first talk and at his
  talk about the howling (§5.3); roster batch 1 begins (§6).
- The owner: talk to Kalé as the game offers; a Mark at the end of each talk.
- Recorded: the open field's list; Kalé's row, live name id and entity id; his block and conversation band.

**K7. The camp at the Gatefront ruins** (Godrick's soldiers)
- Checks: S1 R2 (a golden route); the first soldier family (S1's pass); S7's first camp (outside these plans).
- The owner: fight the camp as one would.
- Recorded: every soldier's row, entity and team; the fight's hits, deaths and engagements.

**K8. The first boss fight whose bar shows, after the build runs** (whatever boss it is)
- Checks: S1 R3 (a golden route); S2's first of at least ten fights (§4, T4); S6's boss flag where the catalogue has
  one (§5.1).
- The owner: fight; a Mark when the boss falls.
- Recorded: the bars, `boss_fight_active`, the holder's row, the boss flag's rise within 10 s.

**K9. Spirit ashes called for the first time while recording**
- Checks: S1 R4 (the summon set).
- The owner: nothing beyond calling them.
- Recorded: the spirit's `chr_type`, team and list entry; Torrent's beside it when mounted (model 8000, B-6).

**K10. The first cave, catacomb or tunnel; the first demi-human site**
- Checks: S1 R5 (a block set; the place id in a dungeon); S2 (its boss); the demi-human card's range (Boc's row must
  stay outside it, NPC §1.2).
- The owner: play through it.
- Recorded: the list inside the dungeon, the place ids, the demi-humans' rows.

**K11. The first NPC summon the player calls for a boss**
- Checks: S1 R6 (the player set's `chr_type` values; the gate's fields through the fight, DATA §3.1 gap 9).
- The owner: call the summon at its sign, as one would.
- Recorded: the session's fields every snapshot, the summon's `chr_type`, the set's members.

**K12. The first NPC invasion; then each named invader**
- Checks: S1 R7 (the invader's class and `chr_type`, ER05 open question 12; the gate's fields, gap 9; the
  announcement's signal, gap 10); the invader's roster binding (§6).
- The owner: a Mark when the game announces the invasion.
- Recorded: the invader's row, kind and class; the session's fields; the banner and message ids.

**K13. The Roundtable Hold** (the first visit, and each return)
- Checks: the place id of the hub (sheltered, DESIGN §8.1); roster batch 2 (its residents, as each arrives).
- The owner: nothing.
- Recorded: the residents' rows and entities.

**K14. Stormveil Castle and its lord**
- Checks: S1 R8 (the first of two legacy dungeons, gap 7); S2 (the castle's bosses); S6 `boss.godrick` (9101, the
  pilot's) and `runes.ge1` (181) as the first great rune is gained; Varré's +11 (which of 181, 191 or 9101 drives it,
  MEM §12.12 item 9); roster batch 3.
- The owner: a Mark when the lord falls.
- Recorded: the castle's list and place ids; the bars; 9101, 180–187, 191–197 and Varré's block.

**K15. Each great rune gained, and each restored at a divine tower**
- Checks: S6 `runes.ge1` to `runes.ge7` (181–187) and what 191–197 mean (ER07 §6 item 5).
- The owner: a Mark at each.
- Recorded: a wide watch over 180–197.

**K16. Liurnia and the academy**
- Checks: the second soldier family (Raya Lucaria's soldiers, ER04 §2.7); S1 R8's second legacy dungeon; S6
  `boss.rennala` (9118); the persona moments S6 lists for the lake country (§5.4); roster batch 4.
- The owner: a Mark when the queen falls, and at each persona meeting.
- Recorded: as K14.

**K17. Caelid and the festival at the red castle**
- Checks: S6 `boss.radahn` (9130), `story.star_fell` (310) against it, the festival flags 9410–9413 and the
  cutscene flags 9414–9417 (MEM §12.12 items 1 and 10); what the general's fall clears in the persona blocks
  (item 6); roster batch 5.
- The owner: a Mark when the general falls and a Mark on first sight of the crater where the star came down.
- Recorded: a wide watch over 300–399, 9400–9420 and the blocks of Blaidd, Alexander and the festival's host.

**K18. The underground rivers and the eternal cities**
- Checks: roster batch 6; S1's list in a large underground area.
- The owner: nothing.

**K19. Altus, Mt Gelmir and the manor**
- Checks: the third soldier family (Leyndell's soldiers, ER04 §2.7); S6 `boss.rykard` (9122); Tanith's block 3100
  (item 7); the manor's persona moments (§5.4); roster batch 7.
- The owner: a Mark when the manor's lord falls, and at each persona meeting.

**K20. Leyndell, the royal capital**
- Checks: S6 `boss.morgott` (9104); roster batch 8.
- The owner: a Mark when the king falls.

**K21. The lord of blood's fight, in his hidden palace**
- Checks: S6 `boss.mohg_lord` (9112); roster batch 9 (in part).
- The owner: a Mark when he falls.

**K22. The mountaintops and the Forge**
- Checks: S6 `story.erdtree_burned` (118) against 300–302, 110–112 and 9131 (item 2); roster batch 9.
- The owner: a Mark when the Fire Giant falls and a Mark when the great tree is set alight.
- Recorded: a wide watch over 100–140, 300–399 and 9100–9140.

**K23. Crumbling Farum Azula**
- Checks: S6 9116 (`boss.maliketh`, a lead: whether his fall starts the ash, item 3); roster batch 10.
- The owner: a Mark when he falls and a Mark on first sight of the ash over the capital.
- Recorded: a wide watch over 300–399 through the change.

**K24. The ashen capital and the end of the cycle**
- Checks: roster batch 10.
- The owner: nothing. The endings (20–22, 120) are not world stages (MEM §12.4).

**K25. The start of the next cycle**
- Checks: S6's NG+ recording, which sets `ng` for every watched alias (MEM §12.12 item 5), 9101's `ng = reset` (the
  pilot's), `ng_lvl` and `JourneyChanged`; and the next chance for every check whose moment passed in the cycle before.
- The owner: a Mark as the new cycle begins.
- Recorded: the flag frame on both sides of the transition.

## 3. S1's capture routes

S1 asks of the near list (DESIGN §16.1): its coverage (the open-field, block and summon sets at all distances, ER05
open question 9), whether its `distance` is squared, the share of `entity == 0` (ER05 open question 1), the invader's
class (ER05 open question 12), which `chr_type` values fill `player_chr_set` (NPC summons, NPC invaders; DESIGN §3.4),
and its cost; for the roster, each binding's rows and entities (§6). **The rule** (DESIGN §16.1): the snapshot within
0.4 ms at p99, and Kalé and three soldier families resolve. It blocks v0.1.

**The starting point.** The read-only rounds have shown where the near list lies and how it behaves (§7). Lodestar's
eighteenth round (O117, O118) is to place each near character's type, team, dead bit, on-screen bit and area, by how
each behaves near a few weak enemies, one of them killed. S1 does not search for those fields: it takes their places
from the build's layout and checks what their values mean across the sets the eighteenth round did not meet (summons,
invaders, a dungeon's block set, a crowd).

**What every S1 record shows**, per session (from the recording, by `rec-stats` and an S1 report still to be built):
- the list's whole length and the number walked (at most the nearest 48) and kept (at most 32 within 40 m);
- per kept character: `chr_type`, live team, entity id zero or not, row, model, dead and on-screen bits, and the
  distance twice, as the list holds it and as computed from the two positions: the list's equal to the computed one,
  or to its square, settles the squared question;
- the entity share, by kind (generic, named, summon);
- the session's gate fields (role, own world, lobby, set count) each snapshot, and the player set's members' kinds;
- the snapshot's cost, p50 and p99.

**R1. The road and the gate** (a golden route; the first half of DESIGN §4.6's smoke route)
- Where: from the Church of Elleh along the road north to the Gatefront ruins; a rest at a grace there; a ride; a
  journey to the Roundtable Hold and back.
- The player: walks and rides it, talks to Kalé as the game offers, rests once.
- Each record shows: the open field's list at every distance up to the list's end; Torrent's entry while mounted;
  Kalé's row (32000000), its live name id against 180000, and his entity id, which S1 gives (personas.toml leaves the
  research lead out); the place ids at the church, the ruins and the hub; the loading screens.
- Judged by: Kalé resolves (the pass's first half); the cost over the whole session.

**R2. A camp skirmish** (a golden route; the smoke route's camp fight)
- Where: the camp at the Gatefront ruins.
- The player: fights the camp, with spirit ashes if the playthrough has them.
- Each record shows: every soldier's row inside the Godrick soldier family's range (43110000, NPC §1.2) and no other
  character's row inside it; their teams and entity share; the dead bit's rise at each kill; the spirit's kind if
  called.
- Judged by: the first soldier family resolves to its card.

**R3. Boss attempts** (a golden route)
- Where: the first boss fight whose bar shows after the build runs (K8), whatever boss it is.
- The player: fights it; a death and then the win make the fullest route, a win alone serves.
- Each record shows: `boss_fight_active` and the bars; the holder's row; the player's death and the boss's; the boss
  flag's rise within 10 s of the win, where the catalogue has the flag.
- Judged by: the cost through a fight; the events against the golden file once recorded.

**R4. A spirit summon** (K9)
- The player: calls spirit ashes.
- Each record shows: the spirit's `chr_type` and team, its entry in the list at the distances it fights, and that it is
  not counted in the set count (B-7).

**R5. Inside a small dungeon** (K10)
- Where: the first cave, catacomb or tunnel; the first demi-human site.
- Each record shows: the list inside a block (does it hold the block's characters the player sees, at every distance);
  the place id on entering and leaving (DATA §3.1 gap 7's question, also asked of the legacy dungeons in R8); the
  demi-humans' rows inside the card's range (41000000) and Boc's row outside it.

**R6. An NPC summon beside the player** (K11)
- The player: calls an NPC summon from its sign for a boss.
- Each record shows: the summon's `chr_type`; whether it is in `player_chr_set`, and the set's length with and without
  NPC types; every gate field through the fight (gap 9). EldenNet must not go dormant: if a field flips, that is the
  finding.

**R7. An NPC invasion** (K12)
- The player: plays on through the invasion.
- Each record shows: the invader's class (player-class or enemy-class object, ER05 open question 12) and `chr_type`;
  whether it is in `player_chr_set`; every gate field (gap 9); the banner or message id at the game's announcement, or
  failing one, the first sighting of the invader's kind within the snapshot's range, which then stands for the
  announcement (gap 10). A named invader's row also serves its roster binding (§6).

**R8. Two legacy dungeons** (K14, then K16 or whichever comes next)
- Where: Stormveil Castle, then the next legacy dungeon the playthrough enters.
- Each record shows: the place id on entering and on crossing each (gap 7); the list's block sets indoors; the soldiers'
  and knights' families.

**R9. The other soldier families** (K16, K19)
- Where the playthrough first meets them: Raya Lucaria's soldiers in Liurnia, Leyndell's in Altus and the capital
  (ER04 §2.7), or other card families first (see the open point on which three count).
- Judged by: each family's rows inside its card's range, and no other's (the pass's second half).

**v0.1's golden routes** are R1, R2 and R3, the recorded counterparts of the three synthetic routes Augur shipped with
(`road-and-gate`, `camp-skirmish`, `boss-attempts`; WHY§225). Each recording becomes a route with `golden update
<recording>` (S10.2.c), checked by S10.3.a (C12: stable over the three, ten replays identical). R1 and R2 together walk
DESIGN §4.6's smoke route, which a playthrough past Limgrave reaches by travelling there by grace, as a patch day would
ask anyway: a walk, not a save. Their recordings are committed only redacted (DATA §1.4; gap 5's fixtures for param
rows, S10.2.d).

## 4. S2's capture routes

S2 asks (DESIGN §16.1): the text tables read on this build; where the game holds its active text language; whether the
install carries English text (O26); and whether each boss bar's `name_id` is the id of the name the bar shows, over at
least ten fights, one with two bars (ER05 open question 13). **The rule:** 100 ids of each table, WeaponName and
ProtectorName included (WHY§105), match the player's own export; the language is read, or the wizard's confirmation is
used; every shipped bar binding matches its fight. It blocks v0.1. Game text never leaves the PC: the comparisons run
there and only their counts come back.

**T1. Text before a world loads** (K1; DATA §3.1 gap 1)
- When: at the title, before the first load of a session.
- Records: whether known NpcName ids (Kalé's 180000 among them) resolve with no world loaded: counts found and matched.

**T2. A hundred ids of each table** (any session, at the title and in the world)
- What: for NpcName, PlaceName, WeaponName and ProtectorName, 100 ids each, read in game and compared with the
  player's own export (`fmg-import`'s local database). The ids are chosen on the PC, the roster's name ids
  (`roster-rows.json`) and the place ids R1 to R8 record among them; `gear-names.json` holds counts only, so the gear
  tables' ids come from the player's export.
- Records: per table, how many match, differ or are not found.

**T3. The language** (K1 and K2)
- What: the field where the game holds its active text language, read at the title and in the world, against the
  language the owner confirms in the wizard. If no field is found, the wizard's confirmation stands (O26).
- English text in another language's install (O26 (b)) is for v1.1, and only if the owner plays in another language.

**T4. The boss bars, over at least ten fights** (K8 onwards)
- When: the first ten bar fights after the build runs, whatever they are (field, dungeon and legacy-dungeon bosses),
  one of them with two bars.
- The player: fights; nothing more, unless the confirmation below is the way chosen.
- Records, per fight: the bar slots used; each bar's `name_id`, damage taken and holder's handle; the holder's row
  once it comes within the snapshot's range; `boss_fight_active`'s edges; in the two-bar fight, which bar the game's own
  single bar pair follows (ER05 open question 13); whether the bar's `name_id` agrees with the leads in
  `boss-leads.json`; whether the name the bar shows is the one its `name_id` gives (see the open point on how).
- Judged by: ten fights recorded, the two-bar one among them; each `name_id` the name its bar shows.

**T5. The roster's bars** (from v0.4, §6)
- When: every named boss's fight once, each as the playthrough reaches it.
- Records: its bar's `name_id` against its holder's row (`expect_param`), for every bar of a multi-bar fight, and its
  phases' rows (S1's question of one row or several); S3's record of its kind, in the same capture (DESIGN §7.8).

## 5. S6's expectation scripts

Each script keeps MEM §6.3's shape: `tests/s6/<topic>.toml` lists expectations against named Marks; the session is
recorded with the wide watch over each script's ranges; `flag-verify <recording> --script <topic>` aligns the edges
with the Marks and writes evidence records to `docs/design/spikes/S6/evidence/` (build, alias, id, Mark, edge timing,
the other ids that moved within ±5 s, a verdict; no game text); a person applies the proposed catalogue patch
(`verified = true`, `evidence += [id]`); the patch-day smoke route re-checks 100, 9000, 181 and 9101. The Marks' names
below are our own labels.

**How a mismatch is recorded.** A record is written whatever the outcome, and its verdict says which way it went: the
expected edge in its window; no edge in the window; the other edge; an edge outside the window; a flag that did not
stay when `then_stays` asks it to; a rise inside `quiet_before_ms`. The ids that moved within ±5 s go in the record
either way, which is how a wrong lead shows its right neighbour; for a new block, `flag-verify` proposes offsets from
the wide watch's edges. A mismatch sets nothing: the alias stays `verified = false`, the expectation stays in its
script, and it runs again at its next chance (§1). An alias still unverified at its release is dropped (DESIGN §16.1's
pass rule: "every alias used is verified, or dropped").

### 5.1 The core (v0.1; DESIGN §16.3)

`tests/s6/core.toml`, run at K3, K4, K8 and K15 as each comes:

```toml
[[expect]]                         # K3: resting at a grace
alias = "rest.at_grace"            # 9000
mark = "sit_at_grace"
edge = "rise"
within_ms = 3000
[[expect]]
alias = "rest.at_grace"
mark = "stand_up"
edge = "fall"
within_ms = 3000
[[expect]]                         # K15: each great rune gained (runes.ge2 at the second, and so on)
alias = "runes.ge1"                # 181; the wide watch holds 180-197
mark = "great_rune_gained"
edge = "rise"
within_ms = 10000
then_stays = true
```

- **9000:** as above, at every rest until it agrees; a few rests settle it.
- **100:** read as set at every load by the `flags` self-test, which the rounds have shown (§7). Its rise comes only at
  a new character's start, so no script waits on it (see the open points).
- **180–187 and 191–197:** each `runes.ge<n>` at the gaining of the nth great rune; at a divine tower, a Mark
  `great_rune_restored` with no expectation, so that the wide watch shows whether 191–197 mean held or restored and
  whether 181–187 ever fall (ER07 §6 item 5).
- **The boss flags:** each `boss.<name>` of the catalogue at its fight's end, in MEM §6.3's form (`mark =
  "<name>_down"`, `edge = "rise"`, `within_ms = 10000`, `then_stays = true`, `quiet_before_ms = 600000`): 9101 at K14,
  9118 at K16, 9130 at K17, 9122 at K19, 9104 at K20, 9112 at K21, and 9116 at K23 as a lead (§5.2). They are also the
  victory evidence of `BossFightEnded`.
- **`ng_lvl`:** not a flag: its rise by one at K25 is read from the snapshot, with `JourneyChanged`.
- **The grace family** (K4): `grace_lit` Marks; `flag-verify` takes the derived `grace.<row>` flag nearest each Mark;
  ten that agree verify the family.
- **The weather** (K5): `rain_starts` and `rain_stops` Marks with the wide watch only, until DATA §3.1 gap 4 names the
  family flags and settles the range.

### 5.2 The pilot and the world items (v0.2; MEM §12.12 items 1 to 5)

- **Kalé** (`tests/s6/kale.toml`, at K6): `kale_first_talk_end` and `kale_howl_talk_end` Marks, with the wide watch
  over his block (4700–4719) and his conversation band in the First Step tile (ER07 §4.2). Expected: +0 set
  throughout; +5 at the first meeting; 1042369308 rising at the first talk's end (the `talked` candidate); 1042369328
  or 1042369329 rising at the howl talk's end (`kale.howl_told`, item 9; ER07 §6 item 15), and staying set. +17's
  purpose (`kale.asleep`) is watched whenever the player finds him away or asleep, with a Mark `kale_away`.
- **The castle** (`tests/s6/godrick.toml`, at K14): MEM §6.3's own example; with K25 it gives 9101's `ng`.
- **The star** (`tests/s6/star.toml`, at K17; item 1): `general_down` and `crater_seen` Marks. Expected: 9130 rises
  at `general_down` and stays; 310 rises (when: at the death, after the cutscene flags 9414–9417, or later, is the
  finding) and stays; 311 and 312 are watched for any rise. If 310 is not the star, `star_fallen` becomes
  `world('general_fallen')` alone, and only with the crater recorded in the same run that verifies 9130 (MEM §12.4).
- **The Forge** (`tests/s6/forge.toml`, at K22; item 2): `fire_giant_down` and `tree_alight` Marks. Expected: 9131 at
  the first; 118 at the second; the order and persistence of 300–302 and 110–112 around them recorded, 118 primary
  unless the recording shows otherwise.
- **The ash** (`tests/s6/ash.toml`, at K23; item 3): `black_blade_down` and `ash_seen` Marks, the wide watch over
  300–399. Expected: 9116 at the first; whether a 300-range flag marks the ash at the second is the finding.
- **The boss flags' `then_stays`** (item 4): carried by each boss's expectation in §5.1.
- **The next cycle** (`tests/s6/ngplus.toml`, at K25; item 5): one Mark `ng_started`. Every watched alias that falls
  at the transition gets `ng = reset`, every one that stays `ng = keep`. Every alias in a world stage or track must
  read `reset` (DESIGN §16.1's pass rule).

### 5.3 What speaking up adds (MEM §12.12 item 11; NPC §3.6)

For each persona and stage, at each meeting: whether the stage waits on a talk with the Tarnished (`beckon_src`), and
which flag marks that conversation as played (`talked`), both read from a `<persona>_talk_end` Mark and the flags that
rise within the 10 s settle window, with S3's record of the talk beside it. Kalé's +17 and Boc's +6 are read for
`state` (awake, asleep or senseless) by what the player finds at the meeting. A value not verified stays unset.

### 5.4 The personas' blocks (v0.5, each before its authoring starts)

One script per persona, `tests/s6/<persona>.toml`, run across the playthrough: a Mark at each meeting
(`<persona>_met_<n>`) and at each deed a stage's label names, with the wide watch over the persona's 20-flag block and
its conversation bands. Expected at every meeting: +0 set while the persona is alive and friendly. The finding, per
block: which offset rises at which meeting, and whether the block is exclusive (one stage set at a time) or cumulative
(item 6). The stages are NPC §3.6's; the leads and the labels are ER07 §4.

- **Varré** (3180), first met at the First Step: the stages NPC §3.6 lists; which flag drives +11 (181, 191 or 9101,
  item 9), read at K14; where +12 and +13 take place; a played observation of whom he serves (item 8).
- **Patches** (3680), first met in Murkwater Cave (ER07 §4.4): his opening hostile form (+2) and its row, which NPC
  §1.2 leaves to be found (S1); his later meetings, each with its place: +11 is verified only together with its place,
  and so is +12's (item 9).
- **Blaidd** (3600), first met in the misty woods to the east (LIKELY, ER07 §4.5), after Kalé's talk: his places after
  +5 (item 9); +13 watched at K17 for the clear the general's fall is said to write (item 6).
- **Alexander** (3660, the whole block a lead), first met stuck in the ground in Limgrave (GK, NPC §3.6): the block
  confirmed (item 9), +8 at the festival, its clear at K17 (item 6), his first offset after the festival and his
  later beats' offsets if the block is exclusive (items 6 and 9).
- **Boc** (3940), first met hiding by a Limgrave road: +6 and +7; his beats after them in Altus, in the capital and
  after (item 9); his later form's row, 523420000 (LIKELY, NPC §1.2), at the meeting that shows it (S1); a played
  observation of his path (item 8).
- **Tanith's block** (3100, a track, not a persona; item 7), at the manor (K19): which of its two label sets the game
  reads, then the offsets of `hosting` (+5 or +6) and `vigil` (3109).
- **Ranni (3740) and the festival's host (3360)** (item 10): at the festival (K17), first whether 9410–9413 mark its
  preparation, the festival, the aftermath and the end; the two blocks' offsets only if they do not. Until then both
  ties stay deferred, and no offset is authored ahead of the recording.

**Played observations** (item 8) are not flag edges: a Mark with our own label at the moment the game itself shows the
fact, kept as the record that a `basis = "played"` tie cites in its `evidence` (MEM §12.2).

## 6. The roster, batch by batch

The roster is every named NPC and named boss of the base game, about 140 with the six personas (DESIGN §7.8: about 45
questline NPCs, 5 to 10 services, about 23 invaders, 40 to 60 bosses, less the overlap). The expansion's follow in
v1.1 (O46), in a batch of their own.

**How they are grouped.** A batch is a region or a hub (DESIGN §7.8; our region keys in `content/eldenring/
regions.toml`). Each NPC belongs to the batch where a normal playthrough first meets it: a named boss to its arena's, a
named invader to the place it invades, a hub resident to the hub. A later form (a reveal, a hostile turn, a summon
form, a boss form) is confirmed in the batch where the form appears, under the NPC's one id (DATA C-3). The census
(`roster-scan --batch`) writes each batch's members from the leads (`roster-rows.json`, `named-census.json`,
`boss-leads.json`, `npc-flags.json`), and the batches follow the owner's own order through the world.

**What each meeting confirms**, per NPC and per form, from the recording of that meeting:
- **S1** (rows and entities): the live row and its live name id against the binding's `expect_name_id`; the entity id,
  non-zero or zero, and whether it is the lead's; its `chr_type` and live team; for a boss, whether its phases use one
  row or several. `roster-verify <recording> --batch <n>` writes the evidence to `tests/roster/`.
- **S2** (a named boss): its bar's `name_id` against its holder's row, every bar of a multi-bar fight (§4, T5).
- **S3**, in the same capture: a named boss's kind (at least 30 of its lines, over several attempts if it has few, or
  `scripted = "none"`), and every named invader and hostile form (mostly `scripted = "none"`; DESIGN §7.8).
- **S6**, where the NPC has a flag block (55 in `npc-flags.json`): the wide watch over its block at each meeting. No
  numeric link from the blocks to the talk groups was found (`named-census.json`), so a block is bound to its NPC only
  by edges seen at that NPC's meetings.
- **The owner** meets the NPC as the game offers, in view for a few seconds; calls an NPC summon where one is offered
  (its summon form serves the companion layer, O46); plays through an invasion. No Mark is needed for S1 or S2.

**The batches, in the order most playthroughs meet them:**
1. **Limgrave, Stormhill and the Weeping Peninsula**, with their caves, catacombs and Castle Morne: the six personas'
   first forms (NPC §1.2), the region's questline NPCs and services, its field and dungeon bosses, its invaders.
2. **The Roundtable Hold**, the hub: each resident at the first visit it is there; the hub batch stays open all
   playthrough, as residents arrive and leave.
3. **Stormveil Castle**: its NPCs and its bosses.
4. **Liurnia, the academy and the manor by the lake**: the lake country's NPCs, bosses and invasions.
5. **Caelid and Dragonbarrow**, with the festival at the red castle: its NPCs, the festival's company and bosses.
6. **The underground rivers and the eternal cities**: their NPCs and bosses.
7. **Altus, Mt Gelmir and the manor in the mountain**: their NPCs, the manor's company, bosses and invaders.
8. **Leyndell, the royal capital**, with what lies beneath it: its NPCs and bosses.
9. **The mountaintops, the snowfield, the great tree's sanctuary and the hidden palace**: their NPCs and bosses.
10. **Farum Azula, the ashen capital and the end**: the last NPCs and bosses of the cycle.

**Schedule and cost** (DESIGN §7.8). The roster's captures start at v0.4, once the census has run and character
bindings ship, run in batches through v0.5 and v0.6, and finish by the v1.0 release candidate: S1 about 5 to 8 hours,
S2 about 8 to 10, S3 about 10 and 2 to 3 more for invaders and hostile forms, 27 to 33 hours in all, made as the
playthrough goes. A binding not verified by the release candidate ships held: listed and described in GraceUI, silent
in game. The personas' own rows are confirmed earlier: Kalé's at v0.1 (S1's pass), the other five's when first met,
before v0.5.

## 7. What the read-only rounds have settled

Lodestar's seventeen rounds (`docs/ops/probes/2026-11-22-l5.md`) read the game and called nothing in it. What they
settled is not planned again here:
- **The flags.** EldenNet's own reader reads the flags through the build's layout, finds the flag set in every
  playthrough (100) set in every round, and agrees with the game on flags the owner changed, five of five (WHY§462,
  §468). S6 starts from a working reader: it checks meanings, not the store.
- **Discovery through the build's layout** in the game: the table's items resolved, none refused (WHY§462, §574).
- **The game's state and the save's slot** (WHY§551, §567, §574); the slot keeps its number at the title, so the
  world is told from the title by the main player's presence (WHY§575).
- **The main player, health and position** read by EldenNet's own reader, with no fault; the maximum health checked by
  the owner against the game's own screen (WHY§568, §574).
- **The near list**: at the layout's place (WHY§569); never the main player (LIKELY, WHY§502); up to 119 elements in
  the open world (WHY§572), the nearest 48 walked and the whole length kept (WHY§573); kept by distance, a character
  behind every address read, and 8 of 12,720 elements with no plain distance yet, so the distance is judged element by
  element (WHY§569, §576). S1 still owes the list's coverage across sets and whether its distance is squared.
- **The order of what a ready start lacks** and how far it may rest on leads (WHY§459, §460).
- **How a round runs**: a read round keeps GraceUI out of the process, and the launcher's panel keeps the Marks on
  Pause (WHY§553); a crash code at the game's quit came with or without EldenNet, so it is not a check's failure, and a
  recording up to the quit stands (WHY§554).
- **Next, not settled:** the near characters' type, team, dead and on-screen bits and area are the eighteenth round's
  (O117, O118); S1 starts from its places (§3).

## Open points

1. **Telling Marks apart in normal play.** MEM §6.3 has the tester play a script and press Mark at each step, so the
   presses come in the script's order. In a normal playthrough the moments come in any order and some recur. Ways:
   the launcher's tester's panel (S10.1.l) lists the moments of the scripts still open and the press stamps the one
   chosen; or bare numbered Marks that `flag-verify` matches to the nearest expected edge of any open script.
2. **The wide watch without GraceUI.** MEM §6.3 turns it on in F11's debug view, which is GraceUI's; S6 runs without
   GraceUI (O90). A setting or the launcher's panel would serve; neither is designed.
3. **Which three soldier families make S1's pass.** DESIGN §16.1 does not name them. Taken as the three soldier cards,
   the third (Leyndell's) comes in Altus, late for a v0.1 exit; taken as the first three of the six cards' families
   the playthrough meets, Limgrave may serve (ER04 §2.7 places Godrick's soldiers, Kaiden sellswords and demi-humans
   there). The owner's choice, with Engine's recommendation.
4. **What v0.1's recording lacks for S1.** It holds the player set's length without NPC types (`player_set_len`), not
   the set's members' kinds; distances computed from positions (`dist_m`), not the list's own; not the list's whole
   length; not each character's class (player-class or enemy-class) for the invader question; not the game's own bar
   pair (ER05 open question 13). An S1 report and these fields, or a spike block in the recording, are needed.
5. **No S1 or S2 report.** `spike-report` measures S0 only (ARCH §12.3); S1's and S2's rules need their own.
6. **S2's text match has no path.** v0.1's recordings keep a look-up's table, id and whether it was found, never the
   text (ARCH §7.5), so the 100-id match against the player's export needs ARCH §7.5's keyed hashes (S10.2.d) or a
   comparison run on the PC; neither is designed.
7. **"The name the bar shows."** Telling whether a bar's `name_id` is the name on screen needs the owner's eyes or a
   local comparison with the holder's row's name (bars use their own band of name ids, DATA §3.1 gap 1); without
   GraceUI's debug view there is no designed way to show the resolved name to the owner. A holder outside the
   snapshot's 40 m gives no row until it comes near.
8. **The first two-bar fight.** Our sources name none; T4 takes the first fight that shows two bars.
9. **Coverage needs a second witness.** Nothing in the design lists the characters the list should hold, beyond the
   summon and player sets and what the owner sees; a Mark when a kind is plainly present (a summon called, an invader
   announced, a crowd in view) is the plan's witness, which is weak at long range.
10. **The 32-character place** for C3's cost (DESIGN §16.3: "the 32-character route is chosen at v0.1"): which place
    keeps 32 characters within 40 m is not known.
11. **The roster's timing.** The roster's captures start at v0.4 (DESIGN §7.8), but a normal playthrough may have
    passed the early batches by then, so their next chance is the next cycle. Keeping the sightings (numbers only)
    from every recording from v0.1 on, and matching them once the census lands, would save that; recordings run at
    about 47 MB an hour, so what is kept, and for how long, is the owner's to agree.
12. **v0.2 and the next cycle.** The pilot's S6 (9101's `ng = reset`, the NG+ recording) blocks v0.2 (DESIGN §16.3),
    and in a normal playthrough an NG+ start comes only at a cycle's end.
13. **Flag 100's edge** is seen only at a new character's start, and MEM §6.3's shape has edges only, no state check;
    v0.1 rests on the `flags` self-test for it.
14. **The core's boss flags** are not listed in DESIGN §16.1; this plan takes the catalogue's `boss.*` aliases.
    Margit's 9100 and the bosses' local defeat flags (ER05 §7) are not in the catalogue.
15. **The weather range**: DESIGN §5.5 cites 550–575 and NPC §9.5 uses 570–575; S6's core scope names neither yet
    (DATA §3.1 gap 4).
16. **The verdict's words and the played observation's form** are not designed: MEM §6.3 names "a verdict", and MEM
    §12.2's `evidence` names "S6 played-observation record ids".
17. **What the golden recordings carry.** Redacted, they still hold the owner's character's numbers: position, level,
    play time, gear ids, the save's slot. O116 asked the owner before a round's report held three such numbers; the
    same question stands before these recordings are committed to a public repository.
    *Engine, 2026-11-24:* the owner's standing word on what may be uploaded covers it: "include it as long as
    nothing personal about me, no privacy issue, no safety issue is being added/uploaded. game time, character
    names are fine." Game numbers of the character are not personal; the redaction still keeps only what the
    goldens' checks read, and the owner is told before the first golden is committed.
18. **DATA's additions are not in DESIGN §16.1 yet.** Gaps 1 (S2's title case), 4 (weather in S6's core), 7 (place ids
    in legacy dungeons), 9 (the gate's fields through a summon and an invasion) and 10 (the announcement) say their
    question joins the spike's row; the rows do not carry them. This plan includes them.
19. **Gap 11** (an NPC on horseback): DATA recommends dropping the tag for NPCs (b); no mounted-NPC check is planned
    unless (a) is chosen.
