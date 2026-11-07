# Elden Ring world-stage and quest leads for named NPCs

> **Scope:** the event flags that tell EldenNet which stage the world is in (endings, journey, story, legacy dungeons, Great Runes, map swaps, boss kills) and where the six v1 personas stand in their questlines (Kalé, Varré, Patches, Blaidd, Boc, Alexander), with Ranni's and Jerren's blocks as headers only. It also records how the questlines touch each other, and what is known only as general game knowledge. Every ID here is a **lead for spike S6**: none has been seen to rise or fall in game yet.
> **Research date:** 2026-11-06. **Game state the data describes:** see §1 (the eventparam dump is pre-DLC; TGA targets App ver. 1.17.2).
> **Evidence tags:** as in [the research README](../README.md), plus **GK** (§1).
> **Method:** a research map of 2026-11-06 was re-checked row by row against the two untrusted clones, read with `grep` and `python3 -I` only, never built or run. The [Verification log](#verification-log) gives each outcome. It extends [ER04 §4.4](04-game-design-and-world.md#44-no-journal-progression-is-flags), [ER05 §5](05-game-state-and-events.md#5-time-of-day-and-weather) and [ER05 §7](05-game-state-and-events.md#7-progression-and-event-flags).

## TL;DR

- **Radahn's star (310) is now LIKELY, not SPECULATION.** TGA's group for Radahn's defeat sets 310 together with 9130 and Radahn's local kill flag, beside the cutscene flags 9414–9417 and 910. The eventparam dump files 310 in the map-swap group as a world event, a green meteor (FACT label). §2.4.
- **The ash starts with Maliketh (9116), LIKELY.** Both datasets have 9116 as Maliketh's kill flag, and TGA notes that it also triggers the cutscene and the change to the Ashen Capital. The Erdtree burning has three map-swap flags (300, 301, 302) besides story flag 118. §2.5.
- **Nine more boss kills, each in both datasets:** Maliketh, Godfrey (golden shade), Hoarah Loux, the Fire Giant, the Elden Beast, Gideon, Placidusax, the Godskin Duo and Astel, with their local flags from TGA. §3.
- **Six persona blocks re-checked:** Varré 3180, Patches 3680 (with +13, +14 and +17), Blaidd 3600, Boc 3940, Kalé 4700 and Alexander 3660. Kalé's and Alexander's blocks are new: Kalé's is confirmed by both datasets, Alexander's by its header code and TGA's bit layout. Ranni's 3740 and Jerren's 3360 have headers but no stage rows in either dataset. §4.
- **New leads found in the re-check:** Radahn's festival has its own flags (9410–9413); Kalé's conversation band holds flags that mark his talk about the Mistwood wolf and about Blaidd (the best candidates for the design's `kale.howl_told`); eight ending-playback flags 9400–9407; and a trophy flag that ties the "hub" flags 104/105 to the Roundtable. §2, §4.2.
- **Corrections:** Patches +14 drops to SPECULATION; the driver of Varré +11 is open; Blaidd's block has no system rows; Alexander +8 and Blaidd +13 are *cleared* by TGA's Radahn group (a TGA reset, only a hint about the game, §2.4); the codename mapping of 133 and 140 is SPECULATION; and ER04's 2026-11-06 "literal labels" correction of Tanith's stages mixed two talk files (now re-corrected in ER04 §4.4).

---

## 1. Sources and method

**What was read** (both clones under the untrusted folder, outside the repository):

1. **`soulsmods/elden-ring-eventparam`** `index.md` @`4d20990` (2024-04-12), here **EP**. FromSoftware's own Japanese flag names, grouped by file (`EFID_common`, `EFID_Talk_NPC<code>`, `EFID_m<map>`), with each row's ID, flag count (FN), usage type and playlog category. The dump is **pre-DLC**: it has no Shadow of the Erdtree flags (330, 76800, the DLC bosses). The build it was taken from is not stated.
2. **The Grand Archives CE table** (`Elden-Ring-CT-TGA`) @`393c439` (2026-10-22, release v1.19.3), here **TGA**. `BUILD_ENV` and the README target **App ver. 1.17.2**. Read: `Event Flags/Event Flag Manager/.cea` (**EFM**, per-boss flag groups with the authors' English comments), `Event Flags/Event Flags by ID/.cea` (**by-ID**, a flat ID-to-name list, including the NPC flags) and `Event Flags/Deprecated/NPCs/*` (per-NPC bit entries). TGA files the NPC entries as deprecated, so they may not be maintained.

**How TGA's NPC entries were mapped to flag IDs.** Each NPC group reads `[[CSFD4VirtualMemoryFlag]+0x28]+offset`. With 125-byte blocks per thousand flags and `bit = 7 - (id % 8)` (ER01 §9), each child's byte and bit give one flag ID. The method reproduces every ID in TGA's by-ID list (for example Varré's group offset `0x18D` gives 3180, Alexander's `0x1C9` gives 3660), so the two TGA files agree.

**Evidence tags.** As in [the research README](../README.md): **FACT** comes from a primary source, **LIKELY** means several weaker signals agree, **SPECULATION** is our own inference. In this note:

- **FACT label** means the dataset's label says so. What the engine does at runtime is LIKELY at best until S6 sees the flag change (ER04 §4.5, ER05 §7).
- A **TGA comment** is the table authors' own observation. It is LIKELY unless a second source agrees. What TGA's code does (for example, which flags a group sets) is FACT about TGA.
- **GK** means general game knowledge that neither clone records. It is LIKELY and owed to S6, and content may use it only to hide something, never to unlock it (§6 item 12).

**No label text is stored.** FromSoftware's developer labels, in Japanese or translated, are FromSoftware text (DESIGN §10.4, MEM §11 item 1). Every meaning below is our own description of what a flag, block or stage identifies. Where it helps, the note says *that* a label carries a code, a name or a development name, never the label's words. TGA's English comments are paraphrased too.

---

## 2. World-stage flags

All in EP `EFID_common` unless stated. Group rows: story at 100 (+100, EP line 636), map swaps at 300 (+100, line 638), time and weather at 500 (line 639), character state at 3000 (FN 2000, line 586), boss kills at 9100 (+100, line 650).

### 2.1 Endings, journey and story

| ID | What it identifies (our words) | Label | Pointer |
|---|---|---|---|
| 20 / 21 / 22 | Game finished with the standard, Ranni's or the Frenzied Flame ending | FACT | EP 708–710 |
| 23 / 24 | Two spare ending slots | FACT | EP 711–712 |
| 30 | The player chose, at a grace, to go on to the next cycle (compare 3001, ER04 §4.3) | FACT | EP 713 |
| 50–58 | One flag per finished journey, from the first (50) to the ninth (58) | FACT | EP 714–722 |
| 100 / 101 / 102 | Game started / tutorial area reached / open world reached | FACT | EP 724–726 |
| 104 / 105 | The hub can be reached / has been reached. The hub is the Roundtable Hold: trophy flag 9339's label names the Roundtable as the hub (FACT label; was LIKELY in ER05 §7) | FACT | EP 727–728, 1742 |
| 108 / 109 | Frenzied Flame received / a second, hidden variant of it (purpose SPECULATION) | FACT | EP 729–730 |
| 110 / 111 / 112 | Forge of the Giants; two Forge events, A and B | FACT | EP 731–733 |
| 114 | Labelled with Ranni's name only; meaning unknown | FACT label | EP 734 |
| 116 | Frenzied Flame held back | FACT | EP 735 |
| 118 / 119 | The Erdtree burned / the Erdtree's hollow reached | FACT | EP 736–737 |
| 120 | Ending seen | FACT | EP 738 |
| 9400–9407 | Eight ending-playback flags, in three lettered families (A-1 to A-4, B-1, B-2, C-1, C-2). Which ending each plays is SPECULATION. TGA's Elden Beast group clears all eight and 120 | FACT labels | EP 1773–1780; EFM 2723–2731 |

### 2.2 Legacy dungeons and Great Runes

| ID | What it identifies | Label | Pointer |
|---|---|---|---|
| 130–140 | Legacy dungeon reached, one flag each, labelled by development codenames. LIKELY mapping: 130 Stormveil, 131 its inner part, 132 Raya Lucaria, 134 Volcano Manor, 135 its interior, 136 Leyndell, 137 its sewers, 138 / 139 the Haligtree's upper and lower parts. SPECULATION: 133 carries a development name that fits no legacy dungeon with certainty, and 140 is Farum Azula by elimination | FACT that each marks a legacy dungeon reached; mapping as stated | EP 739–749 |
| 180 | Holds no Great Rune | FACT | EP 750 |
| 181–187 | Holds at least 1 … at least 7 | FACT | EP 751–757 |
| 191–197 | Holds that rune: 191 Godrick, 192 Radahn, 193 Morgott (labelled with Margit's name), 194 Rykard (confirmed by TGA 9122), 195 Mohg (talk 209), 196 Malenia, 197 Rennala. Whether "held" also means "restored at a Divine Tower" is open (ER05 OQ7) | FACT | EP 758–764 |

### 2.3 Map swaps (group 300)

| ID | What it identifies | Label | Pointer |
|---|---|---|---|
| 300 | The Erdtree on fire, as a world-event map swap | FACT | EP 771 |
| 301 | The same, with a falling-embers effect | FACT | EP 772 |
| 302 | A smaller Erdtree blaze (use SPECULATION) | FACT label | EP 773 |
| 310 | A green meteor, as a world-event map swap. **LIKELY the star that falls after Radahn** (§2.4) | FACT label; reading LIKELY | EP 774; EFM 2307 |
| 311 / 312 | A snowfield meteor / a plains meteor. Whether either ever rises is SPECULATION (possibly cut) | FACT labels | EP 775–776 |
| 320, 370 | A lake tower with its gravity turned upside down; two IDs with the same label. LIKELY the inverted Carian Study Hall in Liurnia, on Ranni's path (GK) | FACT labels | EP 777, 779 |
| 330 | Sealing Tree state (DLC; not in the pre-DLC dump). Polarity unclear (ER04 §2.3) | TGA | EFM 3830 |

All six world-event rows (300–312) share playlog category 3; 320 and 370 do not (FACT).

### 2.4 Radahn's star (310)

- **Evidence (LIKELY):** TGA's EFM group for Starscourge Radahn's defeat (lines 2296–2314) sets, together, Radahn's local kill flag 1252380800, the global 9130 and **310**. 310 follows the group's value, so turning the group off clears it again. The same group carries 910, which TGA comments as driving much of what follows the cutscene, and **9414–9417**: by TGA's comments, a wait between Radahn's death and the cutscene, a switch that turns 910 off, a switch that suppresses the area welcome banner after the warp out of the cutscene, and a red mark placed on the map. The group also clears Blaidd +13 (3613) and Alexander +8 (3668) (§4). Both are written with TGA's always-false modifier, which TGA also applies to loot flags (510300 here) and to NPC states the game sets only after a kill (Gostoc's 3269, labelled as following Godrick's defeat, in Godrick's group, EFM 1865, EP 15619; Tanith's 3109–3111 in Rykard's group, EFM 2602–2604). So the clears are FACT about TGA and only a hint (SPECULATION) about what the game does at Radahn's death (§6 item 7).
- **Support:** EP files 310 in the map-swap group as a world event (§2.3), and Blaidd's +10 is labelled as the stage after the star fell (§4.5).
- **Limits:** 9414–9417 and 910 are not in the EP dump (its rows jump from 9413 to 9419), so their meanings rest on TGA's comments alone (LIKELY). When 310 rises, at Radahn's death, after the cutscene or later, and whether it stays set, is S6 item 2.
- **Correction (2026-11-06):** ER05 §5 called the star reading SPECULATION. It moves to LIKELY on the TGA evidence above.

**Radahn's festival (new lead, FACT labels):** 9410 festival being prepared, 9411 festival under way, 9412 its aftermath, 9413 festival over, 9419 a spare (EP 1781–1785). TGA does not use them. They LIKELY bracket the festival that Blaidd +9 and Alexander +8 wait for, and may stand in for the festival host's stages (§6 item 14).

### 2.5 The Erdtree burning and the ash

- **Burning (FACT labels):** story flag 118 (Erdtree burned), map swaps 300, 301 and 302, and the Forge flags 110–112. Their order and persistence are S6 item 3.
- **The gate before it (LIKELY):** the Fire Giant (9131, §3). TGA hides the Forge of the Giants grace (76510) while the giant lives, which fits the giant guarding the way to the Forge (GK).
- **The ash (LIKELY):** Maliketh's kill flag 9116 (§3). TGA's comment on 9116 says it also triggers the cutscene and the change to the Ashen Capital. ER04 §4.4 has the same point of no return from news sources (LIKELY). Hoarah Loux (9107) and the Elden Beast (9123) are fought in the Ashen Capital (TGA group names).
- **Cross-checks (FACT labels; use LIKELY):** EP has a trophy flag for the Erdtree burning (9341, EP 1744) and, among platform-activity flags 9360–9389, start and end pairs for burning the Erdtree (9366 / 9386) and for beating Maliketh (9367 / 9387) (EP 1753–1772). They may offer a second witness to 118 and 9116.

---

## 3. Boss-kill flags

Rows that ER05 §7 does not hold. "Local" is the map block's own defeated flag (with the encountered flag where TGA has one); "global" is the 91xx kill flag. Identity: TGA's group name and EP's label agree on every row (FACT for both datasets). That the engine sets them as labelled is LIKELY (ER05 §7).

| Boss | Local (encountered) | Global | Pointer and notes |
|---|---|---|---|
| Maliketh, the Black Blade (Farum Azula) | 13000800 (13000801) | **9116** | EFM 2702–2711; EP 1615. TGA: also triggers the cutscene and the Ashen Capital (LIKELY) |
| Godfrey, First Elden Lord (golden shade, Leyndell) | 11000850 (11000851) | **9105** | EFM 2492–2500; EP 1604 (Godfrey, with a qualifier) |
| Hoarah Loux, Warrior (Ashen Capital) | 11050800 (11050801) | **9107** | EFM 2734–2741; EP 1606 (Godfrey) |
| Fire Giant (Mountaintops) | 1252520800 (1252520801) | **9131** | EFM 2674–2682 (hides the Forge grace 76510 while alive); EP 1630 |
| Elden Beast (final boss) | 19000800 | **9123** | EFM 2713–2731 (also clears 9400–9407 and 120); EP 1622 |
| Sir Gideon Ofnir, the All-Knowing (Ashen Capital) | 11050850 (11050851) | **9106** | EFM 2743–2750; EP 1605 (his epithet) |
| Dragonlord Placidusax (Farum Azula) | 13000830 | **9115** | EFM 2684–2690; EP 1614 |
| Godskin Duo (Farum Azula) | 13000850 (13000851) | **9114** | EFM 2692–2700; EP 1613 |
| Astel, Naturalborn of the Void | 12040800 | **9108** | EFM 2838–2844; EP 1607. Not to be confused with Astel, Stars of Darkness (32110800, global 9268, EFM 2752–2759) |

Already in ER05 §7: Margit 9100, Godrick 9101, Morgott 9104, Mohg 9125 / 9112, Rennala 9118, Malenia 9120, Rykard 9122, Radahn 9130, and the DLC's Messmer 9146, Bayle 9163 and Promised Consort Radahn 9143.

---

## 4. Persona flag blocks

### 4.1 Block convention

In the EP talk files, a 20-flag block runs: +0 alive, +1 hostile but can be absolved, +2 hostile and cannot be absolved, +3 dead, +4 unnamed, +5 the initial stage, then named stages (FACT labels, ER04 TL;DR). In most blocks +18 and +19 are system bookkeeping (a kick for real-time stage changes, and the ID of the stage-change event); **Blaidd's block has no such rows** (FACT, §4.5). Two rules matter for the design:

- **Offsets do not follow play order** (LIKELY). TGA files Varré +11 under Limgrave (Godrick's defeat) and +8 to +10 under Liurnia, so +11 is LIKELY reached before +8 (§4.3).
- **"Dead" may also mean "quest over".** The ERR-mined `90005702` sets 3683 and 3943 on death or on a peaceful end (ER05 §7, LIKELY).

Labels marked FACT below are EP stage labels; meanings are ours. Pointers are EP line numbers unless stated.

### 4.2 Kalé (talk 800): block 4700

The block header carries code 800 and Kalé's name (EP 11988), and TGA's merchant entries list 4700, 4701 and 4703 as Kalé's (by-ID 616–618). New in the research (ER04 §4.4 gave only the merchant range 4700–4819).

| Offset | Meaning (our words) | Label | Pointer |
|---|---|---|---|
| +0 / +1 / +2 / +3 | Alive / hostile, can be absolved / hostile, cannot be / dead | FACT; TGA agrees for +0, +1, +3 | EP 19854–19857 |
| +5 | Initial stage | FACT | EP 19859 |
| +6 … +16 | Unnamed | — | EP 19860–19870 |
| +17 | Asleep in the church | FACT label; purpose SPECULATION | EP 19871 |
| +18 / +19 | System | FACT | EP 19872–19873 |

**Conversation flags.** Kalé's persistent band in the First Step tile is 1042369300–349 (EP header 12160; rows 19884–19933), with a volatile band at 1042362710–719. Among them: 1042369308 marks his first conversation as played; **1042369328** and **1042369329** mark that his talk about the wolf of Mistwood and his talk about Blaidd have been played; 1042369320 and 1042369325 are those two topics' conditions (FACT labels). These are the best candidates for the design's `kale.howl_told` (LIKELY; S6 item 15).

### 4.3 Varré (talk 301): block 3180

Header: code 301 and a role name that fits him (EP 11933, ER04 §1). TGA: `Deprecated/NPCs/Varré` and by-ID 952–959.

| Offset | Meaning (our words) | Label | Pointer |
|---|---|---|---|
| +0 / +1 / +2 / +3 | Alive / hostile, can be absolved / hostile, cannot be / dead | FACT; TGA agrees for +0, +1, +3 | EP 15375–15378 |
| +5 | Initial (the First Step). TGA: quest start in Limgrave | FACT | EP 15380; by-ID 955 |
| +6 | After the player reaches Stormveil | FACT | EP 15381 |
| +7 | After the player comes back from the Roundtable | FACT | EP 15382 |
| +8 | After an audience with the Two Fingers. TGA files it under Liurnia, at the Rose Church | FACT | EP 15383; by-ID 957 |
| +9 | After the player invades and wins. TGA ties it to three invasions in Liurnia | FACT | EP 15384; by-ID 958 |
| +10 | After the last trial. TGA ties it to the cloth soaked in blood | FACT | EP 15385; by-ID 959 |
| +11 | After the player gains a Great Rune. TGA files it as Godrick's defeat. Which flag drives it (181, 191 or 9101) is SPECULATION | FACT label | EP 15386; by-ID 956 |
| +12 / +13 | A fight in an underground maze / Varré beaten there. Place SPECULATION (Mohgwyn Palace is a GK guess) | FACT labels | EP 15387–15388 |
| +4, +14 … +17 | Unnamed | — | EP 15379, 15389–15392 |
| +18 / +19 | System | FACT | EP 15393–15394 |

### 4.4 Patches (talk 309): block 3680

Header: code 309 and a role name that fits him (EP 11958). TGA: `Deprecated/NPCs/Patches` and by-ID 916–931.

| Offset | Meaning (our words) | Label | Pointer |
|---|---|---|---|
| +0 / +1 | Alive / hostile, can be absolved | FACT; TGA agrees | EP 16243–16244 |
| +2 | Hostile, cannot be absolved. TGA calls it his opening boss form | FACT; TGA | EP 16245; by-ID 916 |
| +3 | Dead; also the "gone" flag (LIKELY, ER05 §7) | FACT | EP 16246 |
| +5 | Initial. TGA: quest start in Limgrave | FACT | EP 16248 |
| +6 | Waiting at the chest trap. TGA: Murkwater Cave | FACT | EP 16249; by-ID 921 |
| +7 | Meeting again by the lake. TGA: the Scenic Isle grace | FACT | EP 16250; by-ID 922 |
| +8 | Kicks the player off the cliff at the Volcano Manor | FACT | EP 16251 |
| +9 | Meeting again at the Manor | FACT | EP 16252 |
| +10 | Gives the player a gift to take to Tanith | FACT | EP 16253 |
| +11 | Meeting again as a highwayman | FACT | EP 16254 |
| +12 | Last shop. TGA files it under Limgrave and guesses, with a question mark, that it is his quest's last step | FACT | EP 16255; by-ID 927 |
| **+13** | After the cliff kick | FACT | EP 16256 |
| **+14** | Unnamed in EP. TGA files it under Limgrave and guesses, with a question mark, a crouching gesture | SPECULATION *(corrected from LIKELY)* | EP 16257; by-ID 925 |
| +15 / +16 | Unnamed | — | EP 16258–16259 |
| **+17** | Placed nowhere in the world | FACT | EP 16260 |
| +4 | Unnamed | — | EP 16247 |
| +18 / +19 | System | FACT | EP 16261–16262 |

**Murkwater Cave:** TGA's group for Patches as a boss (31000800) also carries 31009210 (his dialogue) and 31008820 (his aggression) (EFM 2951–2956). His volatile talk flags start at 31002700 (EP 16263).

### 4.5 Blaidd (talk 216): block 3600

Header: code 216 and a role name that fits him (EP 11954). TGA: by-ID 648–650 and EFM 2304.

| Offset | Meaning (our words) | Label | Pointer |
|---|---|---|---|
| +0 / +1 / +2 / +3 | Alive / hostile, can be absolved / hostile, cannot be / dead | FACT; TGA agrees for +0, +1, +3 | EP 13845–13848 |
| +5 | Initial. His conversation band is in tile `m60_45_37`, LIKELY Mistwood | FACT | EP 13850 |
| +6 | Just after the joint fight against Darriwil | FACT | EP 13851 |
| +7 | Greeting on joining; that it is Ranni's service is GK | FACT label | EP 13852 |
| +8 | Searching a ruin that the label names after Marika, with a number; which ruin in the shipped map is not settled | FACT label | EP 13853 |
| +9 | Waiting to join Radahn's festival | FACT | EP 13854 |
| +10 | After the star fell | FACT | EP 13855 |
| +11 | Imprisoned | FACT | EP 13856 |
| +12 | Gone mad (chat is refused, NPC §1.2) | FACT | EP 13857 |
| **+13** | Unnamed in EP. TGA calls it Blaidd at the grace, and TGA's Radahn group writes it false (a reset, only a hint about the game, §2.4) | LIKELY | EP 13858; EFM 2304 |
| +4, +14 … +19 | Unnamed. **No system rows** in this block | — | EP 13849, 13859–13864 |

His volatile tile band 1045372700 begins with two flags for being attacked, and 1045372705 marks that the long-range howl conversation has been played (FACT labels, EP 13865–13870). Being volatile, it is no lasting record.

### 4.6 Boc (talk 223; human-form row 342): block 3940

Header: code 223 and a role name that fits him (EP 11971). TGA: `Deprecated/NPCs/Boc` and by-ID 651–656.

| Offset | Meaning (our words) | Label | Pointer |
|---|---|---|---|
| +0 / +1 / +2 / +3 | Alive / hostile, can be absolved / hostile, cannot be / dead. +3 is also "gone" (LIKELY, ER05 §7) | FACT; TGA agrees for +0, +1, +3 | EP 14525–14528 |
| +5 | Initial. TGA: quest start in Limgrave | FACT | EP 14530; by-ID 654 |
| +6 | Lying collapsed in the cave. TGA: Coastal Cave | FACT | EP 14531; by-ID 655 |
| +7 | Becomes the player's attendant. TGA: Liurnia, Lake-Facing Cliffs | FACT | EP 14532; by-ID 656 |
| +4, +8 … +17 | Unnamed: his Altus, Leyndell and rebirth beats are in no label | — | EP 14529, 14533–14542 |
| +18 / +19 | System | FACT | EP 14543–14544 |

The transformation is supported only by the separate `NpcParam` row `523420000` (code 342, LIKELY his later form, ER04 §1).

### 4.7 Alexander (talk 220): block 3660

The EP header carries code 220 and a development name, not the shipped name (EP 11957). His EP talk file (`EFID_Talk_NPC220`, EP 14446) is a generic template on 4980–4999, so the dump has **no stage rows** for 3660. TGA's group offset maps exactly onto 3660 (§1), so the block is LIKELY his. New in the research.

| Offset | Meaning (our words) | Label | Pointer |
|---|---|---|---|
| +0 / +1 / +3 | Friendly / hostile / dead | TGA (LIKELY) | by-ID 640–642 |
| +5 | Quest start. TGA files it under Limgrave; that he is stuck in the ground there is GK | TGA (LIKELY) | by-ID 643 |
| +6 | At the Rear Gael Tunnel Entrance. TGA's NPC entry files it under Limgrave, but TGA's own grace list puts that grace (73257) in Caelid, so the stage's place is LIKELY Caelid | TGA (LIKELY) | by-ID 644, 226 |
| +8 | In Radahn's boss arena, for the festival. TGA's Radahn group writes it false (a reset, only a hint about the game, §2.4) | TGA (LIKELY) | EFM 2305 |

### 4.8 Ranni (talk 106): block 3740, header only

The header carries code 106 and her name (FACT, EP 11961; ER04 §1). The dump has no talk file for 106, so **no stage rows**, and TGA has no Ranni group. Her stages must come entirely from S6 (§6 item 13). Other evidence: her First Step bands (persistent 1042369400–449, EP 12161; volatile from 1042362730, EP 12157), story flag 114 labelled with her name (§2.1), ending flag 21, and the reveal from Renna to Ranni through `NpcParam` 20500010 and 20500000 (community names, LIKELY, ER04 §7).

### 4.9 Jerren (talk 312): block 3360, header only

The header carries code 312 and a role name that fits the lord of Redmane Castle (FACT for the code, EP 11942; code 312 is Jerren in ER04 §1). His EP talk file (`EFID_Talk_NPC312`, EP 17074) is the generic template, so **no stage rows**, and TGA has no Jerren group. That he hosts Radahn's festival is LIKELY (ER04 §2.2, wiki). His stages, or the festival flags of §2.4, are S6 item 14.

---

## 5. Related quests and general game knowledge (GK)

Ties between questlines and the world, by persona. "Evidence" gives the strongest support; a GK tie is LIKELY and owed to S6 (§6 item 12).

| From | To | The tie (our words) | Evidence |
|---|---|---|---|
| Kalé | Blaidd | Kalé tells of a howl in Mistwood that leads to Blaidd; one of Blaidd's lines names Kalé | FACT: Kalé's topic flags 1042369320–329 (§4.2); CSV `216002010` (ER04 §2.2) |
| Kalé | Ranni | Renna appears at the same church at night | GK; her flags share the First Step tile (FACT, §4.8) |
| Kalé | Nomadic merchants | The shared merchant flags 4980–4991 (ER04 §2.4); the merchants' fate under Leyndell | FACT for the flags; the fate is GK and a spoiler |
| Varré | Godrick | +11 follows a Great Rune; TGA ties it to Godrick's defeat (191 / 9101) | FACT label; TGA (LIKELY) |
| Varré | Roundtable | +7 follows the player's return from the Roundtable (104 / 105) | FACT label |
| Varré | Two Fingers | +8 follows an audience with them | FACT label |
| Varré | Mohg and Mohgwyn Palace | His recruitment to Mohg's cause and the way to Mohgwyn (12050800 / 9112 / 195) | GK |
| Varré | Melina | The "maidenless" tic is his (ER04 TL;DR) | FACT (own counts) |
| Patches | Tanith | +10 hands the player a gift for her; her block 3100 has a devouring-Rykard stage (3109) | FACT labels (EP 16253, 15249) |
| Patches | Volcano Manor | +8, +9 and +13 take place there; the Manor is reached at 134 / 135 | FACT labels for +8, +9 and +13; 134 / 135 LIKELY (§2.2) |
| Patches | Rykard | Through the Manor: Rykard's kill (16000800 / 9122 / 194) | LIKELY: Patches meets the player again at the Manor and gives the player a gift for its lady (FACT labels, EP 16252–16253); the lady's block (header EP 11929) has her own file's stages after Rykard's defeat and devouring him (EP 15248–15249); Rykard's local kill flag 16000800 is in the Manor's map m16 (ER04 §3.1), and TGA files his group under the Volcano Manor (EFM 2592) |
| Blaidd | Radahn's festival and the star | +9 waits for the festival, +10 follows the star; 9130, 310, 9410–9413 | FACT labels; 310's reading LIKELY |
| Blaidd | Ranni | +7 joins her service | GK (the label does not name her) |
| Blaidd | Iji | Iji (block 3760, code 224) is tied to the imprisonment stage | GK; Iji's header FACT (EP 11962) |
| Blaidd | Jerren | Jerren hosts the festival Blaidd waits for | LIKELY (ER04 §2.2); §4.9 |
| Alexander | Radahn's festival | +8 is in Radahn's arena; TGA's Radahn group writes it false (a reset, only a hint about the game, §2.4) | TGA (LIKELY) |
| Alexander | Jar Bairn and Jarburg | Jar Bairn's block 3820 (code 226) | GK for the tie; header FACT (EP 11965) |
| Alexander | Mt. Gelmir, Farum Azula, Maliketh | Stuck again in a hot spring; a last duel at Farum Azula (13000800 / 9116) | GK |
| Boc | Rennala | His wish for a human form through Rennala's rebirth (14000800 / 9118 / 197; block 3300, code 200) | GK; Rennala's header FACT (EP 11939) |
| Boc | His mother | His mother's wish that he be beautiful | GK |
| Ranni | Rennala, Radahn, Blaidd, Iji, Seluvis | Her mother (3300 / 9118 / 197); the star as her way to Nokron (9130 / 310); her retainers (3600, 3760, 3560) | GK; headers FACT |
| Ranni | 320 / 370, ending 21 | The lake-tower inversion on her Divine Tower path; the Age of Stars ending | GK for 320 / 370; FACT label for 21 |
| World | Erdtree, Forge, ash | 9131 gates the Forge; 118 / 300–302 burn the Erdtree; 9116 starts the ash; the Roundtable empties after 118 | LIKELY (TGA, ER04 §4.4); the Roundtable change is GK |

---

## 6. What S6 must settle

Items 1–12 are the research map's gaps; 13 and 14 are owed for v1's related ties; 15 and 16 were found in this re-check. Each lead becomes usable only once a rise or fall edge has been seen on 1.17.2 and recorded with a mark (MEM §6.3).

1. **Every ID here is a lead.** Record its edge in game.
2. **310:** does it rise at Radahn's death, after the cutscene (9414–9417), or later? Does it stay set? Do 311 and 312 ever rise?
3. **The Erdtree sequence:** the order and persistence of 300, 301 and 302 against 118, 110–112 and 9131; whether 9116 is the Ashen Capital trigger; what the Roundtable looks like after 118.
4. **320 against 370**, and whether either is Ranni's Study Hall inversion. **114**: what it marks.
5. **Great Runes:** whether 181–187 count down when a rune is used or lost, and whether 191–197 mean obtained or restored (ER05 OQ7).
6. **Unlabelled stages:** Kalé +6 … +16; Alexander everything except +0, +1, +3, +5, +6 and +8; Boc +8 … +17 (his Altus, Leyndell and rebirth beats); Ranni's whole block; Blaidd +13 to +19; Patches +14 to +16; Varré +4 and +14 … +17. Watch each whole 20-flag block, and the persistent conversation bands.
7. **Stage semantics:** exclusive (one stage set at a time) or cumulative; ordering (Varré shows offsets are not play order); whether +1, +2 and +3 behave as labelled (ER04 §10.9); dead against concluded for 3683 and 3943 (ER05 OQ14: re-mine vanilla EMEVD, not ERR).
8. **Kalé's 4700 and Alexander's 3660** in game. Kalé's has both datasets behind it; Alexander's rests on its header code and TGA. Kalé's +17 purpose is unknown.
9. **Journey:** where the NG+ counter lives, what 50–58 (and 30, 3001) do at a cycle change, and which blocks reset (ER04 §10.3).
10. **Allocation:** validate each block's group against the descriptor map at start-up, because an unallocated group reads false (ER01 §9). Confirm that the 3000 group spans 3000–4999 (EP gives FN 2000).
11. **DLC:** no public per-NPC blocks; 330's polarity; 76800 is set only on touch (ER04 §10.1–2).
12. **GK ties** (§5) need a flag or a played confirmation before content relies on them. Until then they can only hide content, never unlock it.
13. **Ranni's 3740 stage offsets**, so that Blaidd's tie to Ranni can go live in v1.
14. **The festival host's stage offsets** (Jerren, block 3360), so that Alexander's festival tie can go live in v1 (Blaidd's own stages carry the festival, MEM §12.4). Check first whether the festival flags 9410–9413 already mark preparation, festival, aftermath and end; if they do, the tie may not need Jerren's offsets.
15. **Kalé's howl flag:** whether 1042369328 or 1042369329 (§4.2) is set once Kalé has told the player about the howl, and whether it persists.
16. **Tanith's block 3100** has two label sets in EP (her own talk file and a second file, ER04 §4.4). Which one the shipped game uses matters for Patches' tie to her.

---

## Sources

- soulsmods/elden-ring-eventparam `index.md` @`4d20990` (2024-04-12): https://github.com/soulsmods/elden-ring-eventparam. Lines cited as EP: `EFID_common` groups 586–650, rows 708–779; boss kills 1599–1634; trophies 1703–1752; activities 1753–1772; endings and festival 1773–1785; block headers 11929–11988; First Step bands 12155–12164; talk files `EFID_Talk_NPC10` (12996), `NPC216` (13841), `NPC220` (14446), `NPC223` (14521), `NPC300` (15236), `NPC301` (15371), `NPC309` (16239), `NPC312` (17074), `NPC800` (19850).
- The-Grand-Archives/Elden-Ring-CT-TGA @`393c439` (2026-10-22, v1.19.3; App ver. 1.17.2): https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA. Under `CheatTable/CheatEntries/Open - The Grand Archives - Elden Ring/`: `Event Flags/Event Flag Manager/.cea` (EFM: Godrick group 1865, Rennala 2135–2136, Radahn 2296–2314, Godfrey 2492–2500, Rykard 2595–2602, Fire Giant 2674–2682, Farum Azula 2684–2711, Elden Beast 2713–2731, Ashen Capital 2734–2750, Astel 2752–2759 and 2838–2844, Murkwater Cave 2951–2956, Sealing Tree 3822–3830); `Event Flags/Event Flags by ID/.cea` (by-ID: grace 73257 at 226, NPC flags 616–959); `Event Flags/Deprecated/NPCs/{Alexander,Blaidd,Boc,Patches,Varré}/`; `BUILD_ENV`, `README.md` line 23.
- EldenNet internal: [ER01 §9](01-engine-internals.md), [ER04](04-game-design-and-world.md) §1, §2.2, §4.4, §7, §10; [ER05](05-game-state-and-events.md) §5, §7 and the open questions.

## Verification log

Re-check of the 2026-11-06 research map against the two clones, on 2026-11-06. Method: each row marked as found in the map's own pass was looked up again by ID with `grep` and `python3 -I` (a helper outside the clones mapped TGA's NPC bit entries to flag IDs, §1). Labels were read for meaning only; none is reproduced. Outcomes: **Confirmed**, **Corrected** (a weaker label, a narrower reading or a fixed detail), **Added** (found in the re-check, not in the map), **Not found**.

| # | Map row | Outcome | Evidence |
|---|---|---|---|
| 1 | 109 a second Frenzied Flame variant | **Confirmed** (purpose SPECULATION) | EP 730 |
| 2 | 111 / 112 Forge events A and B | **Confirmed** | EP 732–733 |
| 3 | 114 labelled only with Ranni's name | **Confirmed** | EP 734 |
| 4 | 130–140 per-ID mapping LIKELY | **Corrected:** LIKELY for 130–132 and 134–139; 133 (its development name unmatched) and 140 (Farum Azula by elimination) are SPECULATION | EP 739–749 |
| 5 | 301 ember effect; 302 smaller blaze | **Confirmed** | EP 772–773 |
| 6 | 310 LIKELY: TGA's Radahn group sets 310 beside 9130 | **Confirmed.** The group also carries 1252380800, 910 and 9414–9417, and writes 3613 and 3668 false (a TGA reset, §2.4) | EFM 2296–2314; EP 774 |
| 7 | 9414–9417 as the cutscene group | **Confirmed in TGA; not found in EP** (the dump skips 9414–9418), so LIKELY on TGA's comments alone | EFM 2310–2313; EP 1784–1785 |
| 8 | 311 / 312 snowfield and plains meteors | **Confirmed** | EP 775–776 |
| 9 | 320 and 370 share a label | **Confirmed** | EP 777, 779 |
| 10 | 104 / 105 the hub, LIKELY the Roundtable | **Corrected upwards:** trophy flag 9339's label names the Roundtable as the hub (FACT label) | EP 727–728, 1742 |
| 11 | Maliketh 13000800 / 9116; TGA notes the Ashen Capital | **Confirmed** in both datasets | EFM 2702–2705; EP 1615 |
| 12 | Godfrey 11000850 / 9105 | **Confirmed** | EFM 2492–2497; EP 1604 |
| 13 | Hoarah Loux 11050800 / 9107 | **Confirmed** | EFM 2734–2737; EP 1606 |
| 14 | Fire Giant 1252520800 / 9131, the gate to the Forge | **Confirmed** for the flags; the gate reading LIKELY (TGA hides the Forge grace) | EFM 2674–2682; EP 1630 |
| 15 | Elden Beast 19000800 / 9123 | **Confirmed**; the group also clears 9400–9407 and 120 | EFM 2713–2731; EP 1622 |
| 16 | Gideon 9106, Placidusax 9115, Godskin Duo 9114, Astel 9108 (identity LIKELY) | **Confirmed, strengthened:** EP's labels agree with TGA's group names. Local flags added (11050850, 13000830, 13000850, 12040800) | EFM 2684–2695, 2743–2746, 2838–2840; EP 1605–1614 |
| 17 | Block convention, +18 / +19 system | **Corrected:** true for Varré, Patches, Boc and Kalé; Blaidd's block has unnamed stage rows there instead | EP 15393–15394, 16261–16262, 14543–14544, 19872–19873, 13863–13864 |
| 18 | Varré's offsets not in play order | **Confirmed** (LIKELY): TGA files +11 under Limgrave, +8 to +10 under Liurnia | by-ID 955–959 |
| 19 | Kalé block 4700: +0 … +3, +5, +17 | **Confirmed, strengthened:** TGA's merchant entries also give 4700, 4701 and 4703 as Kalé's | EP 11988, 19854–19873; by-ID 616–618 |
| 20 | Kalé's howl flag LIKELY in the First Step band | **Added:** the band's rows 1042369320–329 include his topic flags on the Mistwood wolf and on Blaidd | EP 12160, 19904–19913 |
| 21 | Varré +0 … +3, +5 … +13 and TGA's five agreeing entries | **Confirmed** | EP 15375–15388; `Deprecated/NPCs/Varré`; by-ID 952–959 |
| 22 | Varré +11: 191 or 9101 drives it, LIKELY | **Corrected** to SPECULATION: the label speaks of any Great Rune (181 fits as well) | EP 15386; by-ID 956 |
| 23 | Varré +12 / +13 place LIKELY Mohgwyn Palace (GK) | **Not found** in either clone; the place stays SPECULATION | EP 15387–15388 |
| 24 | Patches +2 as his boss form (TGA) | **Confirmed** | EP 16245; by-ID 916 |
| 25 | Patches +13 after the kick | **Confirmed** | EP 16256 |
| 26 | Patches +14 a crouch gesture, LIKELY | **Corrected** to SPECULATION: TGA's own entry carries a question mark, and EP leaves +14 unnamed | EP 16257; by-ID 925 |
| 27 | Patches +17 not present anywhere | **Confirmed** | EP 16260 |
| 28 | Murkwater Cave flags 31009210 / 31008820 | **Confirmed** | EFM 2951–2956 |
| 29 | Blaidd +5 (Mistwood), +7 (Ranni's service), +8 (a Marika ruin, place LIKELY) | **Corrected:** Mistwood and Ranni are not in the labels; Mistwood is LIKELY from his tile band, and the Ranni link is GK. +8's label names a ruin after Marika, with a number, but not which ruin in the shipped map | EP 13850–13853, 13865–13870 |
| 30 | Blaidd +13 at the grace, cleared in TGA's Radahn group | **Confirmed**; unnamed in EP | EFM 2304; EP 13858 |
| 31 | Alexander 3660: header code 220 under a development name, no stage rows | **Confirmed**; his EP talk file is a generic template | EP 11957, 14446 |
| 32 | Alexander +0 / +1 / +3, +5, +6 from TGA, aligned to 3660 | **Confirmed**: the group offset `0x1C9` maps to 3660 | `Deprecated/NPCs/Alexander`; by-ID 640–644 |
| 33 | Alexander +6 at the tunnel; place LIKELY Caelid | **Confirmed**, with new evidence: TGA's grace list places grace 73257 in Caelid | by-ID 226, 644 |
| 34 | Alexander +8 in the boss arena (TGA Radahn group) | **Confirmed**, refined: the group writes 3668 false (a TGA reset, only a hint about the game, §2.4) | EFM 2305 |
| 35 | Boc +5, +6, +7 with TGA; +8 … +17 unnamed | **Confirmed** | EP 14530–14542; by-ID 654–656 |
| 36 | Ranni 3740 header only; no stage rows; no TGA group | **Confirmed**: no talk file for 106 in EP | EP 11961; TGA NPC folders |
| 37 | Jerren block 3360, talk 312, festival host (coordinator's request) | **Confirmed for the header** (code 312); no stage rows (template talk file) and no TGA group. The host role stays LIKELY (ER04 §2.2) | EP 11942, 17074 |
| 38 | Iji 3760 (code 224), Jar Bairn 3820 (226), Rennala 3300 (200), Tanith 3100 (300) | **Confirmed** headers | EP 11929, 11939, 11962, 11965 |
| 39 | Tanith 3109 devouring Rykard; Gostoc 3269 after Godrick | **Confirmed.** Found in passing: Tanith's block has two label sets, so ER04 §4.4's earlier "literal labels" correction mixed files; ER04 §4.4 and its log #22 are re-corrected | EP 15245–15250, 13005–13007, 15619; EFM 1865, 2602 |
| 40 | Radahn festival, ending-playback, trophy and activity flags | **Added** (not in the map): 9410–9413 and 9419; 9400–9407; 9339 and 9341; 9360–9389 | EP 1742–1785 |
