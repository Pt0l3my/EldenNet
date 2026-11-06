# Elden Ring game design, world and lore for an LLM NPC framework

> **Scope:** ELDEN RING (PC/Steam, app 1245620) plus *Shadow of the Erdtree* (2024) and the 2026 *Tarnished Pack*. News sources use that name; SaveForge's regulation-1.17 data calls it "Tarnished Edition DLC". This report covers the NPC roster, world structure, the systems that constrain an AI-NPC layer, the house writing style, a lore primer and spoiler gating, all from the point of view of EldenNet. EldenNet is a SkyrimNet-style in-process mod: v1 is dialogue only, named NPCs get persona and memory, and generic enemies only bark.
> **Research date:** 2026-11-06. **Game state:** App ver. 1.17.2 (see `01-engine-internals.md` §1).
> **Evidence tags:** **FACT** means checked against data or code I read (repository files, data dumps). **LIKELY** means several secondary sources agree, usually search snippets of wikis or guides. **SPECULATION** means design inference.
> **Access limits:** Fextralife, wiki.gg, Fandom, soulsmodding.com and gist.github.com could not be fetched from this session. Claims taken from them come from search-result snippets and are tagged LIKELY. GitHub could be read. The game's own text and flag data were read from public GitHub datasets (see §1). Nothing was executed.
>
> **Fact-check pass (2026-11-06).** An adversarial review re-downloaded every dataset fresh from raw GitHub into a separate directory (the TalkMsg CSV and eventparam `index.md` are byte-identical to the author's copies, by SHA-256), sparse-cloned TGA at `393c439`, and recomputed every count. It cross-checked against independent repositories the author did not use: SoulSplitter `Boss.cs`, DarkScript3 `er-common.MapName.txt`, Paramdex `TalkParam.txt` and SaveForge's regulation-1.17 data. The web-search budget was already used up, and news and wiki hosts (pcgamer.com, opencritic.com, Fextralife) could not be reached. DLC-dialogue and 2026-content claims therefore could not be re-checked and stay LIKELY. Corrections are made inline and marked *(corrected)*. Every verdict is listed in the [Verification log](#verification-log). The main corrections:
> 1. The talk code is *not* uniform across the three datasets. Seluvis's flag block is labelled 306 but his TalkMsg and `NpcParam` code is 307. Gostoc and Hewg have non-`52CCC` rows. Code 335 means different characters in TalkMsg and `NpcParam`.
> 2. Flags 3000–3099 are test, common and misc blocks, not NPC blocks.
> 3. The "746 roars" belong to the Troll family, not to Misbegotten.
> 4. "Omen" occurs in 19 lines, not 44: the 44 counted substrings such as "moment".
> 5. The dump has 8,013 records, not 8,055. Of these, 381 are cutscene or narration lines that do not follow the talk-code layout.
> 6. Ranni's archaic share is 37% when counting thee/thou/ye forms only, and 43% when hath/art/mine are included.

---

## TL;DR

- **Most named NPCs have a three-digit "talk code".** For most NPCs the same number appears in TalkMsg line IDs (`301001040` = Varre, code 301), in FromSoftware's internal flag labels (`キャラ状態フラグ_301_戦場医師`) and in human NPCs' `NpcParam` rows (`523010000` = `52`+`301`+`0000`). DLC NPCs use the 4xx range (Leda `524180000`). *(corrected)* The datasets do **not** always agree:
  - Seluvis's flag block is labelled `306` while his TalkMsg and `NpcParam` code is 307.
  - Hyetta's lines and rows sit under Irina's 310, but a separate, unlabelled flag block `341_狂った指巫女` also exists.
  - Boc's human-model row is `523420000` (342, LIKELY his transformed form), not 223.
  - Gostoc (`36650000`) and Hewg (`34510100`) do not use `52CCC` rows.
  - Code 335 is a cut "dreambrew" NPC in TalkMsg but "Reeling Rico" in `NpcParam`.

  Use the talk code as the main key, but curate every mapping (FACT for the listed agreements and exceptions).
- **Questline state is readable and regular.** Base-game questline NPCs own 20-flag blocks at **3100–4259** *(corrected: 3000–3099 are test, common-event and misc-global blocks, and the 3000 group label covers 3000–4999 as a whole)*. Merchants use 4700–4819, and a shared merchant block uses 4980–4991. In each block the dev labels are +0 alive, +1 hostile (absolvable), +2 hostile (not absolvable), +3 dead, and from +5 onward named story stages. Blaidd's block at 3600 runs from "after fighting with Darriwil" to "imprisoned" and "gone mad". Story, boss, Great Rune, grace, time and weather flags are documented too. This is the substrate for knowledge and spoiler gating. FACT for the labels. That the engine actually sets each bit as labelled, for example +1 on attack, is LIKELY: it was not observed at runtime.
- **Most NPCs do *not* speak archaic English.** Across about 7,530 lines in the dump, archaic forms appear in 37–58% of lines for Ranni, Rennala and Margit/Morgott. *(corrected:* Ranni is 37% with thee/thou/thy/ye only and 43% when hath/art/mine are included.*)* They appear in 0–2% of lines for nearly every Tarnished or commoner. The average line is 8.1 words. "Maidenless" is Varre's tic: 14 of its 15 uses are his and the other is Melina's. Register signals status, so set it per NPC (FACT, own analysis; recomputed).
- **Generic enemies are amnesiac by design.** Resting respawns them. Only bosses, mini-bosses, invaders and story enemies stay dead, via persistent `…0800` flags. Generic memory belongs at the faction or area level (LIKELY for the rule, FACT for the flags).
- **The game already ships a bark vocabulary.** TalkMsg holds faction chants ("…Repel the Omen, and undo Tarnish"), a marching song, war-cry directions ("For Kaiden!") and 39 regional "talking corpse" vignette blocks (codes 700–706). These are the style references for barks (FACT). Generic-enemy voice IDs appear to follow `7` + the 4-digit `NpcParam` family: `7405…` is Kaiden Sellsword (`40500000`), `7434…` Mad Pumpkin Head, `7438…` Stonedigger and `7460…` Troll (LIKELY; pattern inferred from four matches).
- **Fewer bosses talk than expected.** Base-game bosses with lines: Margit/Morgott, Godrick, Rennala, Malenia, Mohg, Rykard, Godfrey, Gurranq/Maliketh, Gideon. Radahn, Radagon and the Elden Beast have no lines in the dump. In the DLC, Rellana, Romina and Promised Consort Radahn reportedly shipped silent, and Messmer speaks (LIKELY; not re-checkable in the fact-check pass).
- **Base-game NPC invaders have no lines in the dump** (Vyke, Nerijus, Eleonora, Anastasia, Istvan…). Their codes exist in `NpcParam` (for example Vyke 304, Eleonora 327, Nerijus 343, Istvan 345), but no TalkMsg line uses them. Any taunts are new content (FACT for the dump).
- **Display names change at reveals:** "Witch Renna"→Ranni, Margit→Morgott, Gurranq→Maliketh, "Heartbroken Maiden"→Roderika, Lightseeker→Finger Maiden Hyetta, Yura→Shabriri. Each name sits on a separate `NpcParam` row variant (FACT, Paramdex community labels). Reading the runtime NpcName therefore LIKELY gets reveal timing for free, but the in-game FMG text was not checked. Static tables and dev labels leak spoilers.
- **Modded play is off FromSoftware's servers.** Vanilla offline play has no player messages, bloodstains or invasions, only developer messages and scripted NPC invaders (LIKELY). *Caveat (added):* players who also run Seamless Co-op have friends as co-op partners and invaders over a separate P2P layer, and Reforged 2.0 adds its own message and invasion servers (doc 02 §2.3). Do not assume the player is alone.
- **Content still grows.** Patch 1.17 added classes, armor, Torrent skins and, reportedly, two NPC invaders, Knight Leontiel and "Noble Broken Mask". News sources call the release the "Tarnished Pack" (2026-08-28). *(corrected)* SaveForge's regulation-1.17 data independently confirms Leontiel's armor items, but calls the content "Tarnished Edition DLC". The release date and the invaders could not be re-checked. Version the knowledge base per patch and DLC (LIKELY).

---

## 1. Data sources and the NPC key

Three groups of public datasets were cross-checked:

1. **`soulsmods/elden-ring-eventparam`** (`index.md`, @`4d20990`, 2024-04-12, pre-DLC). FromSoftware's own Japanese event-flag names, grouped as `EFID_common`, `EFID_Talk_NPC<code>` and `EFID_m<map>`. This is the source for the flag ranges in §3–4 (FACT).
2. **The Grand Archives CE table** (`Elden-Ring-CT-TGA` @`393c439`, 2026-10-22, app 1.17.2). It provides:
   - English flag labels in `Event Flags/Event Flags by ID/.cea`;
   - about 210 boss `isDefeated` definitions in `Event Flag Manager/.cea`;
   - per-NPC groups in `Deprecated/NPCs/*`. TGA files these under "Deprecated", so they may not be maintained.

   TGA confirms many entries from (1) independently (FACT). Its README states "Game: App ver. 1.17.2", and `393c439` is TGA release v1.19.3. TGA's "Event Flags by ID" also has a combined "Irina / Hyetta" NPC category. It has no DLC NPC categories.
3. **Text and row names.**
   - `alidhl/dialogue-generator/ER-Data.csv` (@`bf5e75d`, 2024-04-15) is a third-party, pre-DLC dump of English game text. *(corrected)* It has **8,013 records** (8,056 physical lines, because some texts contain newlines), not 8,055. Its provenance is undocumented: the README describes only an RNN trained on it. It is mostly 9-digit talk lines (7,632). It also holds 381 four- and eight-digit lines that are cutscene or narration subtitles, for example `14000400` (the intro's "Godwyn the Golden was first to perish") and `2003xxxx` (a Margit cutscene). So it is not a pure TalkMsg export. It contains LIKELY-cut content: Asimi, the talking Silver Tear (218); "Guilbert" (303); a dreambrew quest (335).
   - Community `NpcParam` row names come from `soulsmods/Paramdex` (6,864 rows, including DLC) and `Nordgaren/Erd-Tools`. These names are **community-assigned labels, not game text**: they can differ from the in-game NpcName FMG.
   - Paramdex `TalkParam.txt` (7,513 rows, base game only; each row is named after its first line) was used in the fact-check pass as an independent check of talk codes.

**The talk code (FACT for the NPCs listed; see the exceptions below).** The 9-digit TalkMsg IDs are laid out as `CCC BBB LLL` (code, block, line). The 4- and 8-digit cutscene and narration IDs do not follow this layout. Each code below was identified from the content of its lines. Where the other sources have the NPC, they *usually* agree: flag labels such as `216_狼頭の戦士` "wolf-headed warrior" and `309_盗賊の頭` "bandit chief", and `NpcParam 52CCC####` names.

| Code | NPC | Code | NPC |
|---|---|---|---|
| 100 | Melina | 300 | Tanith |
| 102 | Enia | 301 | White Mask Varre |
| 106 | Ranni ("I am the witch Renna") | 302 | Gatekeeper Gostoc |
| 200 / 201 | Rennala / amber-egg children | 307 | Seluvis |
| 202 | Gurranq/Maliketh | 309 | Patches |
| 203 | Malenia | 310 | Irina *and* Hyetta (shared code) |
| 204 | Margit/Morgott | 311 / 312 | Edgar / Jerren |
| 206 | Godfrey | 313 / 314 | Rya / Diallos |
| 207 | Godrick | 316 | Sellen |
| 209 | Mohg | 318 | Yura (row reused for Shabriri) |
| 213 | Hewg | 319 | D and his twin |
| 214 | Rykard | 320 | Roderika |
| 216 | Blaidd | 321–326 | Kenneth Haight, Fia, Dung Eater, Gideon, Rogier, Bernahl |
| 219 | Albus | 330 / 331 | Miriel / Boggart |
| 220 / 226 | Alexander / Jar Bairn | 333 / 334 | Thops / Nepheli |
| 223 / 224 | Boc / Iji | 348 / 349 | Millicent / Gowry |
| 228 | Latenna | 351 | Corhyn |
| 112 | Goldmask (only "..." and "...!") | 800 / 801 | Kalé / nomadic merchants |
| 101 | A Finger Reader crone ("Your fingers, please…") | 227 | Spirit-jellyfish girl ("Sister... Where did you go?") |
| 332 | Volcano Manor ghost (asks you to kill the serpent) | 337 | Leyndell "old man" host (flag label `337_王都老人`; whether he shipped is unverified) |

**Known mismatches between the datasets (added in the fact-check pass, FACT):**

| Case | TalkMsg | Flag labels (eventparam) | `NpcParam` (Paramdex) |
|---|---|---|---|
| Seluvis | 307 | block 3560 labelled **`306_傀儡師`** | `523070000` (307) |
| Hyetta | lines under 310 ("My name is Hyetta", TalkParam `31001001`) | stages in Irina's block 3380, plus a separate block 4080 `341_狂った指巫女` with no stage labels. TalkParam 341 has 56 blank rows | rows `5231000x0` (310 variants) |
| Boc | 223 | block 3940 `223_亜人針子` | demi-human `41109000`; human-model row `523420000` "Boc the Seamster" (**342**; LIKELY his transformed form) |
| Code 335 | cut "dreambrew" NPC | `335_ジコ坊` | `523350000` "**Reeling Rico**" |
| Gostoc / Hewg | 302 / 213 | 302 / 213 | `36650000` / `34510100` (not `52CCC`) |
| Vyke | none | block 3240 `304_傷騎士` ("wounded knight") | `523040000` (304) |
| Unknown | none | block 3120 `1100_輝石魔術の師匠` ("glintstone sorcery master"), a 4-digit code | none |

Most human-model NPCs embed the code in `NpcParam` (`52CCC####`), with the exceptions above. Non-human NPCs use model-family IDs instead:

| NPC | `NpcParam` |
|---|---|
| Ranni | `20500000` |
| Blaidd | `20100000` |
| Melina | `21800000` |
| Boc | `41109000` |
| Alexander | `44909000` |
| Iji | `46040000` |
| Latenna | `31709000` |
| Gurranq | `21109000` |
| Kalé | `32000000` |

So the identity table must be curated, not computed (FACT, Paramdex). The flag-block base cannot be derived from the code either. There is no formula: 216 maps to 3600, 301 to 3180, 106 to 3740.

---

## 2. NPC taxonomy

### 2.1 Tiers for EldenNet

| Tier | Members | v1 treatment |
|---|---|---|
| **A. Questline NPCs** | About 45 base-game and 12–14 DLC named characters with multi-stage quests | Persona, memory, free conversation, gated knowledge |
| **B. Service NPCs** | Merchants, smiths, Finger Readers, spirit tuner, Twin Maiden Husks | Light persona; vanilla menus stay authoritative |
| **C. Talking bosses** | Margit, Godrick, Rennala, Morgott, Mohg, Malenia, Rykard, Godfrey, Maliketh, Gideon, Messmer | No chat in v1; optional combat lines |
| **D. NPC invaders** | About 40 named red phantoms | Silent or one-line taunts |
| **E. Generic humanoids** | Faction soldiers, sorcerers, nobles, demi-humans … | Barks only |
| **F. Voiceless** | Beasts, dragons, giants, golems, spirits, Twin Maiden Husks, Goldmask | Never speak |

### 2.2 Major base-game questline NPCs

Columns: code (§1) · where first met · role · disposition and voice. "Register" refers to §5.

| NPC (code) | First met | Role | Disposition / voice |
|---|---|---|---|
| Melina (100) | Grace near the First Step / Gatefront | Offers the "accord", levels you, grants Torrent, guides you to the Forge | Calm, guarded, terse. Modern register |
| Ranni (106) | Church of Elleh at night, as "Renna" | Carian princess; Age of Stars questline | Regal, aloof, archaic ("Thou'rt possessed of the power, no?") |
| Blaidd (216) | Mistwood, Limgrave | Ranni's half-wolf shadow | Gruff and loyal. Plain speech ("Kalé send you, did he? Ever the bloody busybody.") |
| Iji (224) | Western Liurnia, near Caria Manor | Carian smith and war counsellor | Courteous elder |
| Seluvis (307) | Seluvis's Rise | Ranni's preceptor, puppeteer | Condescending schemer |
| Varre (301) | The First Step | Mohg's war surgeon; recruits you to Mohg | Sneering, mocks the "maidenless" |
| Patches (309) | Murkwater Cave | Trickster, ambusher, merchant | Wheedling ("Well, well, well?"), cowardly surrender |
| Alexander (220) | Stuck in the ground, Limgrave | Warrior Jar on a pilgrimage of battle | Jovial and honour-bound ("Hello-o? … I'm stuck!") |
| Jar Bairn (226) | Jarburg | Child jar | Childlike ("Hallo coz") |
| Boc (223) | Coastal Cave | Demi-human seamster | Insecure, sweet, colloquial ("Could you help us out, cully?") |
| Roderika (320) | Stormhill Shack | Grieving noble, becomes Spirit Tuner | Gentle, melancholic |
| Hewg (213) | Roundtable Hold | Chained smith | Gruff, fatalistic ("Let's get smithing.") |
| Enia (102) | Roundtable Hold | Finger Reader; takes Remembrances | Cryptic, prophetic |
| Gideon Ofnir (324) | Roundtable Hold | "All-Knowing" leader; information broker | Polished, ambitious, long sentences (10.6 words per line on average) |
| Rogier (325) | Stormveil, then Roundtable | Sorcerer investigating Godwyn's death | Genial, curious, wasting away |
| Nepheli Loux (334) | Stormveil, then Roundtable | Warrior, Gideon's ward | Stoic, curt |
| Gostoc (302) | Stormveil gate | Gatekeeper and swindler | Obsequious, treacherous |
| Kenneth Haight (321) | Limgrave ruins / Fort Haight | Self-styled heir of Limgrave | Pompous noble |
| Edgar (311) / Irina (310) | Castle Morne approach | Castellan and his blind daughter | Dutiful; Edgar later becomes an avenger |
| Hyetta (310) | Lake-Facing Cliffs | Blind "Lightseeker"; Frenzied Flame questline | Soft, pleading |
| Yura (318) → Shabriri | First Step area | Hunter of Bloody Fingers; body later taken by Shabriri | Courteous samurai, then a seductive heretic |
| D (319) | Summonwater Village | Hunter of the Dead (Golden Order zealot) | Earnest, rigid |
| Fia (322) | Roundtable Hold | Deathbed Companion | Tender, sorrowful |
| Dung Eater (323) | Roundtable Hold | Cursed murderer | Hateful, vile. Content-policy risk |
| Sellen (316) | Waypoint Ruins cellar | Exiled glintstone sorceress, mentor | Brisk, ambitious |
| Thops (333) | Church of Irith | Poor academy scholar | Humble, wistful |
| Rya (313) | Liurnia, near Scenic Isle | Volcano Manor scout (Zorayas) | Timid, kind |
| Tanith (300) | Volcano Manor | Proprietress; recruits assassins | Gracious, fanatical |
| Bernahl (326) | Warmaster's Shack | Recusant knight | Blunt, principled |
| Diallos (314) | Roundtable / Academy Gate Town | House Hoslow knight | Earnest, insecure |
| Gowry (349) | Gowry's Shack, Caelid | "Sage" with a hidden agenda | Kindly façade |
| Millicent (348) | Church of the Plague | Rot-afflicted swordswoman | Earnest, grateful |
| Corhyn (351) | Roundtable Hold | Incantation teacher; Goldmask's follower | Devout, warm |
| Goldmask (112) | Altus Plateau bridge | Silent scholar of the Golden Order | Never speaks |
| Miriel (330) | Church of Vows | Turtle pastor; site of absolution | Gentle |
| Boggart (331) | Liurnia prawn shack | Prawn-selling blackguard | Rough ("What are you lookin' at?") |
| Jerren (312) | Redmane Castle | Witch-hunter castellan; host of Radahn's festival | Gruff veteran |
| Latenna (228) | Slumbering Wolf's Shack | Albinauric archer; later a spirit ash | Bitter ("Foul Tarnished, what do you want?") |
| Albus (219) | Village of the Albinaurics | Frightened elder (secret medallion) | Fearful |
| Gurranq (202) | Bestial Sanctum | Beast clergyman who devours Deathroot | Feral, broken ("Feed me more...Death.", `202003020`) |
| Kalé (800) | Church of Elleh | Nomadic merchant | Wry and practical |

Locations and roles are LIKELY, from common wiki and guide knowledge. Quotes and codes are FACT, from the dump.

### 2.3 Shadow of the Erdtree NPCs

Row IDs are FACT (Paramdex). Locations are FACT-for-the-label where a Paramdex row names them, as in `524180068 Needle Knight Leda (NPC) (Mohgwyn Palace)`. Such rows exist for Leda, Ansbach, Freyja, Hornsent, Moore, Thiollier, Igon, Ymir, Dane (`524250090` Moorth Ruins) and Queelign (`524130098` Shadow Keep, Church District). These annotations are community labels, re-checked in the fact-check pass. Other locations, and the questline facts, are LIKELY (PC Gamer, Game8, wiki snippets). The "likely code" column assumes the `52CCC####` pattern holds for the DLC.

| NPC (`NpcParam` / likely code) | First met | Role | Disposition |
|---|---|---|---|
| Needle Knight Leda (`524180000` / 418) | Mohgwyn Palace, then Land of Shadow | Leader of Miquella's followers; later turns on "traitors" | Zealous, suspicious |
| Sir Ansbach (`524160000` / 416) | Belurat Main Gate Cross | Mohg's former knight | Courtly, measured |
| Redmane Freyja (`524170000` / 417) | Three-Path Cross | Radahn's knight | Blunt warrior |
| Hornsent (`524140000` / 414) | Three-Path Cross | Hornsent avenger hunting Messmer and Marika | Terse, bitter |
| Moore (`524150000` / 415) | Belurat Main Gate Cross | Merchant-like Miquella follower | Affable |
| Thiollier (`524190000` / 419) | Pillar Path Cross | St. Trina's devotee, poisoner | Soft-spoken, shy |
| Dryleaf Dane (`524250000` / 425) | Moorth Ruins (Paramdex `524250090`) | Monk of the "dryleaf" martial arts | Hearty |
| Igon (`524030000` / 403) | Pillar Path Waypoint | Dragon-hunter sworn against Bayle | Loud, vengeful |
| Count Ymir, High Priest (`524200000` / 420) | Cathedral of Manus Metyr | Priest of the Mother of Fingers | Fervent, polite |
| Swordhand of Night Jolán (`524090000` / 409) | Same cathedral | Ymir's assassin; ends the quest by attacking you | Aloof |
| Hornsent Grandam (`59100000`) | Belurat storeroom | Elderly Hornsent seeking vengeance | Hate-filled |
| Fire Knight Queelign (`524130000` / 413) | Paramdex NPC row at Shadow Keep, Church District; invasion sites unverified *(corrected)* | Zealot; later friendly | Fanatic |
| St. Trina (`53300000`) | Various | Sleep-aspect of Miquella (spoiler) | Whether St. Trina speaks at all is unverified |

Leda's group can join her final fight. Paramdex has "(Boss) (Enir-Ilim)" rows for Leda, Hornsent, Moore, Freyja and Dane. Burning the Sealing Tree advances every DLC quest, which makes it a DLC-wide point of no return (LIKELY, PCGamesN). TGA labels flag **330** as the "Sealing Tree status flag" (`Event Flag Manager/.cea`, Romina entry). Its polarity is unclear: TGA's code treats `330 == false` together with Messmer dead as "Sealing tree already burnt".

### 2.4 Merchants and service NPCs

- **Merchants:**
  - Kalé (800).
  - Many "Nomadic Merchants" (801). Their barks include "Ahh, welcome...dear customer" and "I'm hungry. I've been hungry so long."
  - Isolated, Hermit, Imprisoned and Abandoned merchants.
  - Pidia, Gostoc, Patches, Boggart and Miriel.
  - The silent, invulnerable Twin Maiden Husks (LIKELY, Game8/Fextralife).
- **Merchant flags:** TGA tracks merchant alive, hostile and dead states at 4700–4753. Dev labels 4980–4986 (`EFID_Talk_NPC899`, `赤目商人共通_…`) record a shared "red-eyed merchant" state: all normal (4980), all hostile (4981), attacked once (4982), attacked ≥2 times (4983), met (4985) and sleep reward taken (4986) (FACT, re-checked). Other talk files reuse 4980–4986 with generic template labels; ignore those.
- **Shared persona:** one regional "nomad merchant" template with a per-instance attack counter is enough.
- **Smiths and services:**
  - Hewg and Iji smith.
  - Roderika tunes spirits.
  - Enia takes Remembrances.
  - Corhyn, Miriel, Sellen, Thops, Rogier, D, Gowry and Seluvis sell spells or items.
- **Commerce stays vanilla.** In v1, the vanilla `ShowShopMessage`/ESD menus remain the only path to buying and selling.

### 2.5 Talking bosses

FACT for base-game lines (from the dump):

- **Margit/Morgott (204):** "Foul Tarnished." / "Put these foolish ambitions to rest."
- **Godrick (207):** "Lowly Tarnished... Thou'rt unfit even to graft..." / "Great Godfrey, didst thou witness?"
- **Rennala (200):** "Come, sweetings, become what ye desire." Her amber-egg children (201) chant "Sleep tight, bound tight / by Mother's amber."
- **Malenia (203):** "I am Malenia. Blade of Miquella." / "Is this...my first...defeat?"
- **Mohg (209):** a countdown in Latin (`sex … ūnus, nihil`), then "Miquella is mine and mine alone."
- **Rykard (214):** "Let us devour the gods, together!" / "A serpent never dies."
- **Godfrey (206):** "Dearest Marika... I am returned."
- **Gurranq/Maliketh (202).**
- **Gideon (324).**

Silent: Radahn (base game), Radagon, the Elden Beast, Placidusax, the Fire Giant, Astel, Loretta and most field bosses. This is FACT for absence from the dump, and LIKELY for the shipped game, because the dump's provenance and completeness are undocumented. Its 7,632 nine-digit lines are, however, in line with Paramdex's 7,513 base-game TalkParam rows plus body-type variants.

DLC (not re-checkable in the fact-check pass: the news hosts and wikis were unreachable, and no DLC TalkMsg export was found on GitHub):

- **Rellana, Romina, Promised Consort Radahn:** shipped silent; cut voicelines were datamined (LIKELY, from the PC Gamer and OpenCritic headlines only).
- **Messmer:** speaks (LIKELY; one snippet attributes "Those stripped of the Grace of Gold shall all meet death…").
- **Midra, Bayle (whose fight Igon narrates) and Gaius:** unverified.

### 2.6 NPC invaders

The names below are FACT, from Paramdex `523CCC0000`/`524CCC0000` rows. Which of them are invaders, as opposed to human-model field bosses such as Esgar, Magnus or Ancient Dragon-Man, is LIKELY.

- **Base game:** Vyke, Nerijus, Eleonora, Ensha, Anastasia, Old Knight Istvan, Rileigh the Idle, Juno Hoslow, Vargram, Okina, Inaba, Adan, Moongrum, Mad Tongue Alberich, Recusant Henricus, Errant Sorcerer Wilhelm, Necromancer Garris, Nameless White Masks, Great Horned Tragoth, Maleigh Marais, Preceptor Miriam, Bloody Finger Ravenmount Assassin, Dolores, Reeling Rico, Fia's Champions, Knights of the Great Jar.
- **DLC:** Jolán and Anna, Fire Knight Queelign, Logur, Moonrithyll and others.
- **Tarnished Pack (2026):** Knight Leontiel (Caelid, after Radahn) and "Noble Broken Mask". A new 1.17 Leyndell NPC invasion is also reported and may be the same character. Neither invader is in Paramdex yet (LIKELY: KeenGamer, NintendoLife, GameRant).

Several invaders are quest targets:

- Istvan, Rileigh and Juno are Volcano Manor contracts. *(corrected for precision)* 345, 346 and 347 are **talk codes inside conversation-flag labels**, not flag IDs:

  | Talk code | Conversation-flag block | Label | `NpcParam` |
  |---|---|---|---|
  | 345 | `1042399300` | Volcano request target 1, invasion condition | `523450000` Old Knight Istvan |
  | 346 | `1039539200` | target 2 | `523460000` Rileigh the Idle |
  | 347 | `1050569200` | target 3 | `523470000` Juno Hoslow |

  The labels read `火山館の依頼ターゲットN_侵入条件` (FACT).
- Nerijus and Eleonora are Yura's prey.
- Vyke ties into Frenzied Flame lore. A talking corpse says "Ahh, Lord Vyke, it seems that you were no lord, after all."

**None of these invaders has lines in the TalkMsg dump** (FACT for the dump; re-checked). Codes such as 304 (Vyke), 327 (Eleonora), 339 (Ensha), 343 (Nerijus), 344 (Tragoth), 345–347 and 353 (Anastasia) appear in `NpcParam` but in no 9-digit TalkMsg ID. Invader names occur only in *other* NPCs' lines: Yura on Nerijus and Eleonora, Gideon on Ensha, a talking corpse on Vyke. Exception to watch: code 335 is "Reeling Rico" in `NpcParam`, but the 335 TalkMsg lines belong to the cut dreambrew NPC, not to Rico. Vanilla "taunts" are gestures and behaviour, not speech (LIKELY). Doc 03 mentions "invader taunts" in talk ESD. That conflicts with this dump evidence, and doc 03 tags it only LIKELY. Vyke has a full 20-flag state block (3240, `304_傷騎士`) despite having no lines, which hints at a cut questline (SPECULATION).

### 2.7 Generic humanoid factions

`NpcParam` base IDs are FACT (community row names). Allegiance is LIKELY, from lore summaries. The bark column is SPECULATION.

| Faction (`NpcParam`) | Where | Allegiance | Barks should draw on |
|---|---|---|---|
| Godrick Soldier `43110000`, Godrick Knight `43510000` | Limgrave, Stormveil | Godrick, "the Golden lineage" | Grafting, Stormveil's walls, contempt for Tarnished |
| Leyndell Soldier `43130000`, Leyndell Knight `43530000` | Altus, Leyndell | Erdtree / Morgott | "Repel the Omen, undo Tarnish", the Erdtree's thorns |
| Raya Lucaria Soldier `43120000`, Cuckoo Knight `43520000`, Battlemage `37040000`, Marionette `38500000` | Liurnia, Academy | Academy of Raya Lucaria | Glintstone, the sealed academy, the "Glintstone Key" |
| Carian knights (bosses or invaders such as Bols, Moongrum) | Liurnia | Caria and the moon | Full moon, Lady Ranni |
| Haligtree Soldier `43160000`, Haligtree Knight `43560000`, Cleanrot Knight `38000000` | Snowfield, Haligtree, Caelid | Miquella / Malenia | Rot, Miquella's promise |
| Mausoleum Soldier `43150000` | Walking Mausoleums | Guards of demigod remains | Duty, silence |
| Kaiden Sellsword `40500000` | Limgrave / Caelid roads | Mercenaries | "For Kaiden!" (vanilla direction, ID 740500100) |
| Vulgar Militia `43210000`, Highwayman `43770000` | Roads, Altus | Brigands | Loot, runes |
| Wandering Noble `43000000`, Noble's Page `37030000`, Commoner `36600000` | Everywhere / Leyndell | Faded nobility | Pleading, gossip, fear |
| Demi-Human `41000000` | Limgrave caves and forts | Demi-human chiefs and queens | Pidgin or chatter only |
| Misbegotten `34500000` | Morne, Stormveil | Beast-men; rebels or Godrick's thralls | Roars. *(corrected:* the 746xxxxxx roar directions most likely belong to the Troll family, `46000000`; no Misbegotten bark block was identified*)* |
| Albinauric `31700000` | Liurnia, Snowfield | Persecuted artificial folk | Fear of "Omenkillers", Haligtree hope |
| Omenkiller `48200000` | Village of the Albinaurics | Omen-hunting zealots | Butchery, contempt for the "impure" |
| Fire Monk `39000000`, Fire Prelate `39100000` | Mountaintops, Caelid | Keepers of the giants' flame | Fire, vigil |
| Man-Serpent `39500000` | Volcano Manor | Rykard | "Devour the gods" |
| Sanguine Noble `35500000`, Bloodhound Knight `42900000` | Altus, Mohgwyn | Mohg / Bloody Fingers | Blood, "the dynasty" |
| Godskin Apostle `35600000` | Divine towers, Farum Azula | Gloam-Eyed Queen (lore) | Mocking, sermonising |
| Black Knife Assassin `21000000` | Catacombs | Night of the Black Knives | Near-silent whispers |
| Nox Priest `33000000`, Ancestral Follower `33600000` | Underground cities | Eternal City cults | Ancestor and star worship |
| Crucible Knight `25000000` | Field bosses | Oldest Erdtree knights | Gravitas (silent in vanilla) |
| **DLC:** Messmer Soldier `58300000`, Fire Knight `51600000`, Black Knight `58400000`, Horned Warrior `52500000`, Gravebird `50900000`, Bloodfiend `50800000`, Furnace Golem `51700000` (golem: voiceless) | Land of Shadow | Messmer's crusade / Hornsent / native peoples | Messmer's flame, the purge, "the Impaler" |

### 2.8 The bark vocabulary the game already ships (FACT)

| ID | Content |
|---|---|
| `736000100…180` / `743000400…480` | Erdtree guardian chant: "Erdtree, O Golden Circle. Grant us the droplets of life, … We are your guardians. We swat at flies, snuff out flame, Repel the Omen, and undo Tarnish." |
| `730000100…160` (also under Godrick's 207) | "Golden Ones" song: "One day, we'll return together. To our home, bathed in rays of gold… Not the fool Omen King. Nor the rank malformed twins." |
| `743000200…290` | A marching song: "We journey on a road so far, / Death's fortune's what we seek, / Coffin in tow, we'll find a hero…" |
| `743000100` | "Agheel, oh, Agheel… Feral flame of Agheel, burn true…" |
| `740500100`, `746000100…140`, `743400100…110`, `743820100…120` | Voice-actor directions, not text: war-cries, roars, snoring, and a man whose "psyche snaps" from a fly in his helmet. *(added)* The prefix after `7` matches an `NpcParam` family: `7405` = Kaiden Sellsword `40500000`, `7434` = Mad Pumpkin Head `43400000` (the fly in the helmet), `7438` = Stonedigger `43820000` (pickaxe swings), `7460` = Troll `46000000` (roars, sword, snoring). `7430` (marching song, "coffin in tow", Erdtree chant, wailing) fits Wandering Noble `43000000` (LIKELY) |
| 700–706 | **"Talking corpses"** (dev flag label `話す死体`), one code per region, 39 blocks in all. 700 = Limgrave and Weeping Peninsula ("The demi-humans wax wroth… Where are you, Lord Kenneth?"). Its conversation flags sit in tiles `m60_42_40`, `43_31`, `43_34`, `43_37`, `43_39`, `44_37` and `45_36`. 701 = Liurnia ("old Albus"), 702 = Altus/Gelmir (flag label "話す死体（高山）"), 703 = Caelid ("Aeonia", "Sellia"), 704 = Mountaintops/Snowfield ("Sol", "Lord Miquella"), 705 = Mohgwyn ("Mohgwyn Dynasty"), 706 = Leyndell. The region mapping is LIKELY, inferred from content and flag tiles |

**Lesson:** FromSoftware's ambient speech is chant-like, faction-ideological, tied to place, short, and often addressed to no one.

### 2.9 The voiceless set (proposed, SPECULATION)

Default is silence. A character may speak only if it is a curated named NPC or on an allowlist of human-like factions (§2.7). The allowlist excludes beasts, dragons, giants and trolls, golems, Erdtree Avatars, spirits, mindless undead, Kindred of Rot, scarabs, fingercreepers and unnamed living jars. Twin Maiden Husks and Goldmask are silent by design.

Beware edge cases: Alexander and Jar Bairn are jars that talk; Boc is a demi-human who talks; Rya is a man-serpent who talks.

---

## 3. World structure

### 3.1 Regions, legacy dungeons and map IDs

**Map-area IDs** (FACT; inferred from where boss flags live and from TGA labels, and confirmed in the fact-check pass against DarkScript3 `Resources/er-common.MapName.txt`):

| ID | Area |
|---|---|
| `m10` | Stormveil (`m10_01` = Chapel of Anticipation) |
| `m11` | Leyndell (`m11_05` = Ashen Capital) |
| `m12` | Underground: Ainsel, Nokron, Deeproot, Mohgwyn (`m12_05`) |
| `m13` | Crumbling Farum Azula |
| `m14` | Raya Lucaria |
| `m15` | Haligtree / Elphael |
| `m16` | Volcano Manor |
| `m18` | Stranded Graveyard |
| `m19` | "Stone Platform", the Elden Beast arena |
| `m20` | Belurat `m20_00`, Enir-Ilim `m20_01` |
| `m21` | Shadow Keep `m21_00`, Specimen Storehouse `m21_01` (Messmer's flag `21010800` lives here), West Rampart `m21_02` |
| `m22` | Stone Coffin Fissure |
| `m25` | "Finger Birthing Grounds" (Manus Metyr; Metyr's boss flag is `25000800`) |
| `m28` | Midra's Manse |
| `m30`–`m35`, `m39` | Catacombs, caves, tunnels, divine towers, Shunning-Grounds; `m39` = Ruin-Strewn Precipice |
| `m40`–`m43` | DLC mini-dungeons: catacombs `m40`, gaols `m41`, forges `m42` … |
| `m60` | Base overworld |
| `m61` | DLC overworld (tile flags `20XXYY####`, e.g. Rugalea `2044470800`) |

**Region list.** TGA's grace list covers 53 sub-regions and 418 grace flags (FACT, recounted). 382 entries have the form "Region - Sub-region - Grace", across 48 sub-regions. 36 legacy-dungeon entries have the form "Dungeon - Grace", across 5 dungeons. The Roundtable (`71190`) is listed separately. Base-game regions: Limgrave, Stormhill, Weeping Peninsula, Liurnia (52 graces), Caelid, Dragonbarrow, Altus, Mt. Gelmir, Leyndell (Royal and Ashen), Mountaintops, Snowfield, Haligtree and the underground. DLC regions: Gravesite Plain, Scadu Altus, Shadow Keep, Rauh, Cerulean Coast, Jagged Peak, Abyssal Woods, Scaduview and Enir-Ilim.

**Legacy-dungeon codenames.** Dev "legacy reached" flags 130–140 use codenames: エジンバラ "Edinburgh" (Stormveil), モンサン "Mont-Saint(-Michel)" (Raya Lucaria), 火山牢 (Volcano Manor), 王都 (Leyndell), 腐れ大樹 "rotten great tree" (Haligtree) and others (FACT labels; the codename-to-place mapping is LIKELY).

### 3.2 Graces, travel and Torrent

Grace unlock flags (FACT):

| Range | Graces |
|---|---|
| 71000+ | Legacy dungeons (71000 = Godrick's grace) |
| 72000+ | DLC legacy |
| 73000+ | Mini-dungeons |
| 76000+ | Overworld (76800 = "Realm of Shadow - Gravesite Plain - Gravesite Plain", LIKELY the DLC arrival grace) |

*Caveat (added):* a grace flag is set only when the player touches that grace. A player can walk away from the arrival point without touching it. "DLC entered" should therefore also accept being in map `m61`/`m20`–`m28`, or any DLC boss, grace or NPC flag.

Graces are the respawn, rest, level-up (through Melina), time-skip and fast-travel nodes. Torrent is summoned by whistle. TGA labels Melina's flag 4680 "Torrent whistle" (FACT). Torrent is known in-world: Ranni says "I'd heard tell of a Tarnished hurtling about atop a spectral steed."

### 3.3 Time and weather

- **Time:** the dev labels for flags 530/531/532 are `時間フラグ：朝/昼/夜`, morning/noon/night (FACT for the labels). *(corrected)* Whether the engine keeps these flags up to date continuously was **not** verified. TGA does not use them, and doc 05 also rates runtime maintenance LIKELY. Prefer `WorldAreaTime` (doc 01) as the primary clock and use the flags as a cross-check. Resting offers "pass time" to Morning, Noon or Nightfall (LIKELY).
- **Night-only content:** Renna at Church of Elleh, Night's Cavalry, Deathbirds (LIKELY).
- **Weather:** the labels of flags 550–563 are, in order:
  - 550–553: very clear (快晴), sunny, cloudy, light cloud;
  - 554–557: rain, heavy rain, storm, and a special storm variant (`嵐（守護末）`);
  - 558–563: snow, heavy snow, fog, thick fog, thick fog with rain, sandstorm.

  564 and 565 are unnamed. "Series" flags 570–575 group them as clear, cloudy, rain, snow, fog and other (FACT for the labels; runtime maintenance LIKELY, as for time).
- **Map-wide transitions** (flags 300–320): Erdtree burning, meteor strikes in three regions, and the Liurnia tower gravity inversion (FACT).

These are cheap and robust inputs for "environment" context, alongside `WorldAreaTime` (doc 01).

---

## 4. Systems that shape the AI design

### 4.1 Respawning makes generic enemies amnesiac

Resting at a grace, or dying, respawns ordinary enemies. Bosses, many mini-bosses, invaders and story enemies stay dead (LIKELY, Fextralife and checklist sites). Their persistence lives in flags such as `10000800` (Godrick defeated) (FACT; TGA and, independently, SoulSplitter `Boss.cs`). *(corrected)* The pattern is `…0800`/`…0801` (defeated/encountered) for a map block's primary boss, and `…0850`/`…0851` for a secondary boss in the same block: Margit is `10000850`/`10000851`. Overworld field bosses use tile flags such as `1042380800` (Stormhill Deathbird). TGA defines about 210 `isDefeated` entries. A generic soldier can know "a Tarnished has been killing us here" only as a faction rumour, never as personal memory.

### 4.2 Death and runes

Runes, which are both currency and XP, drop where the player dies and can be recovered (LIKELY). Vanilla NPCs ignore deaths but notice absence. Hewg: "Well, where've you been hiding? I took you for dead." (`213010000/10`). Remarks on long absences are in-genre; remarks on respawns are not.

### 4.3 NG+ cycles

Dev flag 3001 "chose to go to the next cycle" (FACT). Difficulty scaling stops at NG+7 (journey 8) (LIKELY, Fextralife/guides). NG+ resets quest flags, so NPC memories must be partitioned by journey (SPECULATION). Detecting the journey counter at runtime is an open question (doc 01).

### 4.4 No journal: progression is flags

The game has no quest log. State lives in event flags (FACT, `EFID_common`):

| Range | Meaning |
|---|---|
| 100–120 | Story: game start, tutorial reached, open field reached, Roundtable reached, Frenzied Flame received (108), Forge of the Giants (110), flame suppressed (116), Erdtree burning (118), ending seen (120) |
| 130–140 | Legacy dungeon reached |
| 180–187 | Great Runes held: 0 … ≥7 |
| 191–197 | Specific Great Rune held. Dev names: 接ぎ木の王 Godrick, ラダーン Radahn, マルギット (Morgott), グールの王 "ghoul king" = Rykard (confirmed by TGA flag 9122), グレーターデーモン "greater demon" = Mohg (talk 209), マレニア Malenia, レナラ Rennala |
| 9100–9199 | Boss-kill "common" flags. DLC bosses continue into 92xx, e.g. 9271 and 9275 (TGA) |
| 3000–4999 | Group label `GrF：キャラ状態フラグ`. **Per-NPC state blocks at 3100–4259** (§1). *(corrected)* 3000–3099 are test, common-event and misc-global blocks. 4650–4699 hold Melina's conversation flags, 4700–4819 the merchant states, and 4980–4991 the shared "red-eyed merchant" flags |
| 330 | "Sealing Tree status flag" (TGA label; polarity unclear, see §2.3) |

Overworld conversation flags sit inside each map tile's block. Each tile has a `…2700` "volatile" (揮発) range, apparently cleared when the map block unloads (LIKELY), and a `…9200` persistent conversation range. For example, tile `m60_42_36` (the First Step) holds conversation flags for Varre (301), Kalé (800), Ranni (106) and Melina (100) (FACT labels).

**Example stage lists** (FACT, dev labels):

- **Blaidd (3600):** initial → after Darriwil fight → greeting on joining Ranni → searching ruins → waiting for Radahn festival → after star fell → imprisoned → gone mad.
- **Patches (3680):** initial → waiting at chest trap → reunion at lake → kicks you off a cliff at the Volcano Manor → reunion at the Manor → entrusts gift for Tanith → reunion as highwayman → final shop.
- **Tanith (3100):** *(corrected to the literal labels)* in the first manor (`最初の館にいる`) → in front of the boss room → appears after the boss is defeated → after Rykard is defeated → **devouring Rykard** → nowhere.
- **Fia (4120):** initial → regained the stigma → temporarily absent → near Godwyn's corpse → just before lying with Godwyn → boss-warp object → after the boss → killed by "Danny" (D's twin).
- **Enia (3480):** initial → PC holds 2 Great Runes → PC refused entry to the Erdtree → Erdtree burned → dead.

Rya, Fia, Gurranq and Varre have equally detailed blocks.

NPCs move, vanish and die between stages, and many quests lock out at fixed milestones:

- burning the Erdtree empties most of the Roundtable
- killing Maliketh turns Leyndell into the Ashen Capital
- in the DLC, burning the Sealing Tree advances every quest

(LIKELY, GameRant/PCGamesN.)

### 4.5 Hostility and absolution

Attacking a friendly NPC sets +1 (absolvable) or +2 (permanent hostile), and death sets +3. *(downgraded)* The block layout and labels are FACT. That the engine sets these exact bits on attack and death is LIKELY: it was not observed at runtime. Verify with TGA's Event Flag Logger before relying on it. Absolution uses Celestial Dew at the Church of Vows. It restores the non-hostile state of attacked NPCs but does not revive the dead or calm characters who are hostile by default (LIKELY, Fandom snippet). EldenNet must read these bits before every exchange. A hostile NPC gets no conversation, or a hostile bark.

### 4.6 Online features in modded, offline play

Offline play has no bloodstains, player messages or player phantoms; developer messages remain (LIKELY, TechRadar/Digital Trends). Modded clients must stay off FromSoftware's servers (doc 02). Scripted NPC invasions and NPC summon signs still happen. Do not build perception on messages. *Caveat (added):* Seamless Co-op (ERSC) is a widely used DLL mod with its own P2P sessions, and Elden Ring Reforged 2.0 runs its own servers for summons, invasions and messages (doc 02 §2.3). Doc 02 expects EldenNet to "often run inside ERSC sessions". The player may then have human co-op partners and friend invaders, and, under Reforged, player messages. v1 dialogue should stay local to each client, and perception must tolerate extra player characters (SPECULATION).

### 4.7 Summons and spirit ashes

Renna gives the Spirit Calling Bell. Named spirit ashes include Latenna, Tiche, and Jolán and Anna. Questline NPCs such as Alexander, Nepheli, Blaidd, D, Millicent and Leda's group appear as **boss-fight summon signs** (LIKELY), with `chr_type` `WhiteSummonNpc` (doc 01). A summoned Nepheli is the Roundtable Nepheli, so co-op fights are high-value memory events.

### 4.8 The player as Tarnished

NPCs never use the player's typed name. They say "Tarnished", "champion called by grace", "cully" or "coz". Lines that refer to the player come in **body-type variants at ID+1** (`201002150` "he'll be born again" / `201002151` "she'll…") (FACT). TalkParam also has `msgId_female` (doc 01). Generated text must use the right pronouns.

---

## 5. Tone and writing style (data-driven prompt guidance)

The figures below come from the 7,532 non-empty, non-stage-direction base-game lines in `ER-Data.csv`. The fact-check pass recounted 7,531. This total includes the 4- and 8-digit cutscene and narration lines (§1); the per-NPC shares use 9-digit IDs only. FACT, own counts, recomputed. The archaic share depends on the regex, so ranges are given.

- **Length:** 8.1 words per subtitle line on average. Short merchant barks average 4.0 words (Nomadic merchant), and broken creatures are terse (Gurranq 4.2). About 12% of lines end in "...". Speech is chunked: one breath, one line.
- **Openers:** "Oh" (239 lines), "Well" (167), "Ah" (143), "Ahh" (56), "O" (36, invocative: "O Marika", "O, dearest Miquella"), "Heh" (28), "Very well" (26), "Hmm" (22). "Ahh," is real but less frequent than "Oh,".
- **Register is a status marker.** Archaic pronouns and verbs appear in a share of each NPC's lines as follows:

  | NPC | Pronouns only (thee/thou/thy/thine/ye) | Including hath/art/didst/'twas/mine/ne'er… |
  |---|---|---|
  | Rennala (40 lines) | 57.5% | 57.5% |
  | Margit/Morgott (49) | 49.0% | 49.0% |
  | Ranni (239) | 37.2% | 43.1% (the author's 41.8% lies within this range) |
  | Godfrey (8) | 25% | 25% |
  | Gurranq/Maliketh (66) | 16.7% | 22.7% |
  | Godrick (25) | 8% | 12% |
  | Melina (236) | 4.7% | 5.5%: almost all of it when quoting Marika ("I divest each of thee of thy grace") |
  | Varre, Patches, Blaidd, Boc, Gideon, Seluvis, Iji, Kalé, Yura, Tanith, Hewg, D, Fia … | 0% | 0–1.6% |

  The sample sizes for bosses are small (8–66 lines), so treat those percentages as indicative only.

  Commoners use British-inflected modern English with idioms: "plumb out of luck", "cully", "coz", "mate", "Oi!".
- **Signature tics belong to individuals.** "Maidenless" appears 15 times: 14 from Varre, plus one from Melina. "Coz" is Jar Bairn's. "In Marika's own words." is Melina's. A shared style prompt must not spread these tics to every NPC.
- **Politeness and pathos are everywhere.** "please" appears in 186 lines (163 as a whole word; 186 also counts "pleased" and "pleasure"), "I see" in 61, "thank you" in 48, "my thanks" in 34, "forgive me" in 22, "farewell" in 15.
- **The lexicon recurs** (whole-word, case-insensitive counts per line, recomputed): Erdtree (101), grace (82), Elden Ring (62), Elden Lord (60), demigod(s) (53), Two Fingers (43), Golden Order (41), Shattering (23), Omen (**19**), frenzied flame (16), Greater Will (10). *(corrected: the earlier figure of 44 for "Omen" was a substring count that included "moment" and "women"; "grace" was 88 as a substring.)*
- **Conventions:**
  - NPCs introduce themselves by name and title.
  - They repeat their last line on re-talk ("Back already?").
  - They talk around secrets rather than explain them.
  - They never use game-mechanic words.
  - Bosses get a defeat monologue.
  - Lines are often sentence fragments.

**Prompt rules for EldenNet (SPECULATION, derived from the above):**

- Set a per-NPC `register` (`archaic-regal | formal | colloquial | broken | chant | silent`).
- Cap replies at 1–3 lines of about 8–12 words each (SkyrimNet already uses similar caps, doc skyrimnet/02).
- Allow at most one opener interjection.
- Ban modern words such as "quest", "level", "HP", "OK", "boss", "NPC", "game".
- Refer to items and places only by their in-game names.
- Prefer implication over exposition.
- Never volunteer lore the NPC would not know (§7).

---

## 6. Lore primer for the world knowledge base

Facts marked (t) are stated in game text. The rest is community consensus and should be stored as *belief* or *uncertain* entries.

**Base game.**

- The Lands Between were ruled by **Queen Marika the Eternal**, vessel of the **Elden Ring**, raised up by the **Greater Will**, an outer god-like will.
  - The Greater Will speaks through the **Two Fingers**, whose words Finger Readers interpret. Melina: "Have you heard of the Finger Maidens? They serve the Two Fingers…" (t).
  - The **Erdtree** and the **Golden Order** are the Ring's visible order. The dead "return to the Erdtree" through its roots: "A proper death means returning to the Erdtree." (t, talking corpse).
- **Destined Death** (the Rune of Death) was removed from the Ring and sealed in Maliketh's Black Blade.
  - On the **Night of the Black Knives**, a stolen fragment killed Godwyn the Golden's soul but not his body ("Godwyn the Golden was first to perish", t, intro).
  - Those Who Live in Death and the Deathroot followed. Ranni's role as plotter is LIKELY, wiki consensus.
- Marika shattered the Ring.
  - The **Shattering** was a war among demigods who held **Great Runes**: Godrick, Rennala, Radahn, Rykard, Malenia, Mohg and Morgott.
  - The intro names "General Radahn, conqueror of the stars" (t).
  - Malenia's scarlet rot bloomed over Caelid after her battle with Radahn (rot: t, "It's rotting… The very earth that Caelid rests upon").
- **The Tarnished** are warriors who lost grace and were exiled with Godfrey, the first Elden Lord. They have been recalled by grace to gather Great Runes and become Elden Lord ("Arise now, ye Tarnished. Ye dead, who yet live." t).
- To enter the thorn-sealed Erdtree, the Tarnished must burn it at the Forge of the Giants. Melina is the kindling, unless the Frenzied Flame (flag 108) takes her place.
- **Other powers:** the **Frenzied Flame** (Three Fingers, Shabriri; it "melts it all away" t), **Scarlet Rot** (Malenia), the **Formless Mother** (blood, Mohg), the **Dark Moon** (Ranni's **Age of Stars**) and the **Fell God's** flame (the Fire Giants).
- **Identity reveals:** Radagon is Marika's other self, Hoarah Loux is Godfrey, and Margit is Morgott. These are top-tier spoilers.
- **Endings:** Fracture, Stars, Order (Goldmask), Despair (Fia/Duskborn), Frenzied Flame, Blessing of Despair (Dung Eater) (LIKELY, widely documented).

**Shadow of the Erdtree.**

- Miquella abandoned his flesh at Mohgwyn and entered the hidden **Land (Realm) of Shadow**, behind the **Shadow Tree / Scadutree**, to become a god at Enir-Ilim.
- **Messmer the Impaler**, Marika's son, led a purging crusade there against the **Hornsent**. The community reading is that the Hornsent persecuted Marika's people, the shamans (LIKELY or uncertain; Kotaku, wiki.gg snippets).
- Leda's band follows Miquella's trail: Freyja, Moore, Dane and Thiollier, plus Ansbach and the Hornsent for their own reasons.
- Promised Consort Radahn is the final fight. The ending text reads "A lord will usher in a god's return, and the lord's soul will require a vessel" (LIKELY).
- Rellana is Rennala's twin, Midra is a Lord of Frenzied Flame, Metyr is the Mother of Fingers, and Bayle is a dread dragon.
- Keep DLC truths gated behind the DLC arrival grace (76800) and further DLC flags.

**Not canon here:** *Elden Ring Nightreign* (2025) is a standalone spin-off. Its characters and events must stay out of the knowledge base.

---

## 7. Spoiler and lore-consistency gating

**The problem.**

- An LLM knows the whole wiki, including fan theory.
- Vanilla NPCs know only their own slice, and reveal it on fixed quest beats.
- Identities change mid-game. Paramdex name variants show it:
  - "Heartbroken Maiden" (`523200000`) → "Roderika" (`523200079`) → "Roderika, Spirit Tuner";
  - "Scribe" → "Brother Corhyn";
  - "Smithing Master" → "War Counselor Iji";
  - "Lightseeker Hyetta" → "Finger Maiden Hyetta" (`523100035`);
  - "Renna the Witch" (`20500010`) vs "Ranni the Witch" (`20500000`);
  - Yura's row variant "Shabriri" (`523180050`).

  "Mohg, the Omen" (`48000000`) and "Mohg, Lord of Blood" (`48000068`) are two encounters with the same character rather than a reveal. *Caveat (added):* Paramdex names are community labels. That the in-game NpcName FMG changes in the same way, through `NpcParam.nameId`, is LIKELY but was not checked against game text.
- FromSoftware's **internal labels are spoilers too.**
  - Melina's code is labelled `娘マリカ` ("daughter Marika") and Millicent's `マレニアの娘` ("Malenia's daughter").
  - Yura's code is `混沌の宿主` ("host of chaos").
  - Hyetta shares Irina's code. Irina's flag block 3380 (`310_城主の娘`, "castellan's daughter") holds Hyetta's stages: grape hand-overs A, B and C, "learned what the grapes are", Frenzied Flame before and after. Stage 3386 is labelled `偽装死している`, "is feigning death". This is a spoiler-grade hint, and its meaning is SPECULATION.

  Dev labels must never reach prompts.

**Proposed gating model (SPECULATION):**

1. **Three stores.** *World truth* is never shown directly. *NPC belief* is per NPC, with an optional "lies" field (Gideon, Gowry, Patches, Gostoc). *Player-seen* holds witnessed events and vanilla lines already shown, recorded via a TalkMsg hook (doc 01).
2. **Each knowledge entry carries a `requires` predicate** over:
   - story flags (100–120);
   - boss flags (`…0800`/`…0850`, `91xx`–`92xx`);
   - Great Rune flags (180–197);
   - grace or region flags (7xxxx; for the DLC, 76800 plus the map ID);
   - the Sealing Tree flag (330, once its polarity is verified);
   - NPC stage bits (curated block base + n; blocks are 20-aligned between 3100 and 4259);
   - the journey number. Example: "Radahn is dead" requires the Radahn defeated flag. "Ranni is Renna" requires Ranni's reveal stage.
3. **Display names come from the runtime NpcName FMG** of the NPC's current `NpcParam` row, so aliases reveal themselves in sync with vanilla.
4. **Stage-locked secrets.** An NPC may reveal secret *S* only at or after the vanilla stage in which it reveals *S*. Before that, it deflects in character.
5. **Hard state guards.** Dead NPCs never speak. Hostile NPCs do not converse. NPCs flagged "nowhere" (for example Tanith +10) are not instantiated.
6. **Corpus hygiene.** Strip cut content: Asimi (218), Guilbert (303) and the dreambrew NPC (335) are LIKELY cut. Whether any other code in a raw dump shipped must be checked before use: for example 337 (an old Leyndell host), 227 (the jellyfish girl) and 332 (the Volcano Manor ghost). Code 335 is reused as "Reeling Rico" in `NpcParam`, so never join TalkMsg to `NpcParam` by code alone. Also strip dev labels and *Nightreign*. Mark every theory entry `uncertain`.

---

## 8. Implications for the AI framework

### v1 (minimal, stable)

1. **Canonical identity.** Use a curated `persona_id` as the primary NPC key, with `talk_code` (3 digits) as its usual source. Ship a curated `npc_identity.toml` that maps `{persona_id → talk_code(s), NpcParam rows (all variants), flag_base, persona file, register, voiceless?}`. Resolve live characters by `npc_param_id` (doc 01) through this table. Do not derive identity from names, and do not derive `flag_base` or `NpcParam` rows from the talk code. *(added)* Seluvis (flag 306 / talk 307), Irina/Hyetta (310 rows and flags shared, plus a stray 341 block), Boc (223 / human form 342), Gostoc, Hewg and code 335 break any formula (§1).
2. **Questline adapter.** For each Tier-A NPC, read its 20 state bits each time it comes into perception range. Map each stage to a persona overlay (location context, mood, unlocked knowledge, forbidden topics). Start with NPCs whose stages are labelled in both flag datasets: Blaidd, Patches, Varre, Rya, Diallos, Rogier, Boc, Gideon, Nepheli, D, Edgar, Gostoc, Alexander, Kenneth, Yura, Bernahl, Sellen, Thops, Kalé and Roderika.
3. **Vanilla-first coexistence.** Free chat is a side channel. Commerce, quest progression and item gifts stay in vanilla ESD menus. Because v1 has no actions, the system prompt must forbid promises ("I'll meet you at…", "take this") and claims of having done something.
4. **Generic barks.** Stateless per spawn. Inputs are:
   - the faction (§2.7, keyed by `NpcParam` prefix; the vanilla `7`+family voice blocks in §2.8 are style seeds for the matching families);
   - PlaceName;
   - time and weather (`WorldAreaTime` first, flags 530–575 as a cross-check);
   - combat events: player sighted, hit, ally killed, player killed, player flees, player heals, mounted on Torrent, spirit summoned.

   Output: one line of at most 10 words, in faction register, modelled on the vanilla chants. Apply a global rate limit and a per-faction cooldown. Generic NPCs offer no conversation in v1.
5. **Voiceless allowlist** (§2.9). Default silent.
6. **Memory partitioning.** Key memories by journey (NG cycle) as well as save slot. Wipe or archive NPC memories on a journey change. Record co-op summons and invader kills as memories for the relevant named NPCs.
7. **Style module.** Per-NPC register and catchphrase list (§5), length caps, a banned-modern-words list, and an implication-over-exposition instruction. Never use the player's real name, and use body-type pronouns.
8. **Offline assumptions.** No reliance on messages or bloodstains. Scripted invaders are treated like bosses (silent by default).

### Later

- Ambient spirits: generated "talking corpse" vignettes, using the 700–706 style.
- Faction-level rumour memory ("the Tarnished who sacked Fort Haight").
- Talking-boss combat lines on HP thresholds.
- DLC personas.
- Tarnished Pack invaders.
- NPC↔NPC Roundtable chatter (Gideon, Fia, Rogier, D, Nepheli, Corhyn, Hewg, Enia, Roderika). This is a natural "scene" location, since the Roundtable is a hub with many named NPCs.

---

## 9. Risks

- **Spoilers and canon drift.** The LLM's prior knowledge holds endings, identity reveals and fan theory. Mitigate with predicate-gated retrieval, explicit "you do not know X" lists, and a post-generation check for named entities the NPC should not know.
- **Contradicting vanilla.** A free reply may contradict a later scripted beat, for example Patches pretending to be honest or Gideon revealing his plans. Persona stage overlays must encode what each NPC conceals.
- **Identity collisions.** Margit and Morgott share code 204, Irina and Hyetta share 310, and Yura and Shabriri share 318. Summon, invader and boss versions of one NPC have different `NpcParam` rows. *(added)* The three datasets also disagree in places: Seluvis 306/307, Boc 223/342, and code 335 meaning two different characters.
- **Dataset provenance.** *(added)* The TalkMsg CSV is an undocumented third-party export: a hobby RNN project's training data, mixing talk and cutscene lines. The Paramdex names are community labels. The eventparam index carries FromSoftware's own names but is pre-DLC, dated 2024-04-12. The style statistics and "no lines" claims are only as good as that export. Regenerate every corpus from the user's own install (FMG export) before shipping.
- **Content-policy friction.** The Dung Eater's dialogue, Fia's intimacy and Mohg's blood rites may trip provider filters. Consider "tone-down" persona variants.
- **Copyright.** TalkMsg text is FromSoftware's. Build persona and lore corpora locally from the user's own install (FMG export) instead of redistributing game text. Quotes in this report are short research excerpts.
- **Data staleness.** The eventparam and TalkMsg dumps are pre-DLC (2024). Paramdex and TGA are current, but DLC NPC state blocks are undocumented. Patches 1.17.x added content (invaders), so the knowledge base needs versioning.
- **Generic-enemy realism.** Respawned enemies cannot remember the player. Barks that imply memory ("you again!") break lore unless framed as faction rumour.
- **Overused memes.** "Maidenless", "Ahh," and "Tarnished" spam would read as parody. Enforce per-NPC catchphrase ownership and frequency caps.

---

## 10. Open questions

1. Where are the per-NPC state blocks for DLC NPCs (Leda, Ansbach, Freyja, Moore, Thiollier, Dane, Igon, Ymir, Hornsent)? No public dump covers them.
2. Is flag 76800 (Gravesite Plain grace) a reliable "DLC entered" signal? It is set only when that grace is touched (§3.2). Which DLC story flags beyond boss kills mark milestones? Known so far: Messmer `21010800` and Promised Consort `20010800`, both confirmed by TGA and SoulSplitter. *Partial answer (fact-check pass):* TGA labels flag 330 "Sealing Tree status flag", but its polarity needs runtime confirmation.
3. Where is the NG+ journey counter at runtime, and which flags survive a cycle change?
4. Do the Tarnished Pack invaders (Leontiel, Noble Broken Mask) have TalkMsg lines? What are their `NpcParam`/flag IDs?
5. What exactly do Messmer, Midra and Bayle/Igon say? A DLC TalkMsg export is needed.
6. How reliable is the region mapping for talking-corpse codes 700–706, and can those spirits be detected at runtime?
7. Which Tier-A NPCs have mid-quest states in which talking to them would be lore-breaking (for example Rya after the draught, Blaidd when mad)? Should chat be disabled there?
8. Melina is not a normal world NPC. She appears via grace and cutscenes. Does she need a special channel?
9. *(added)* Do flags 530–532 and 550–575 track time and weather continuously at runtime, or only at scripted moments? And do the +1/+2/+3 NPC state bits get set exactly as labelled? A TGA Event Flag Logger session would settle both.
10. *(added)* Did the DLC really ship Rellana, Romina and Promised Consort Radahn silent? Does St. Trina speak? This can be checked against a DLC TalkMsg export (FMG 360/460), because the news and wiki hosts were unreachable from the fact-check session.
11. *(added)* Who are the codes 341 (flag block `狂った指巫女`, with blank TalkParam rows) and 1100 (flag block `輝石魔術の師匠`)? Is "Reeling Rico" (`523350000`) a shipped invader?

---

## Sources

**Repositories and datasets (read 2026-11-06)**
- soulsmods/elden-ring-eventparam, `index.md` @4d20990 (2024-04-12): https://github.com/soulsmods/elden-ring-eventparam
- The-Grand-Archives/Elden-Ring-CT-TGA @393c439 (2026-10-22): `CheatTable/CheatEntries/Open - The Grand Archives - Elden Ring/Event Flags/{Event Flags by ID,Event Flag Manager,Deprecated/NPCs}/` https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA
- soulsmods/Paramdex `ER/Names/NpcParam.txt`, `TalkParam.txt`: https://github.com/soulsmods/Paramdex/tree/master/ER/Names
- Nordgaren/Erd-Tools @a2757f9, `src/Erd-Tools/Resources/Params/Names/NpcParam.txt`: https://github.com/Nordgaren/Erd-Tools
- alidhl/dialogue-generator `ER-Data.csv` @bf5e75d (2024-04-15, base-game TalkMsg dump): https://github.com/alidhl/dialogue-generator
- EldenNet internal: `docs/research/eldenring/01-engine-internals.md`, `02-modding-ecosystem.md`, `docs/research/skyrimnet/02-conversation-pipeline.md`

**Game systems (search snippets; LIKELY)**
- Sites of Grace (respawn rules): https://eldenring.wiki.fextralife.com/Site+of+Grace · https://game-checklists.com/elden-ring/all-sites-of-grace/
- New Game Plus: https://eldenring.wiki.fextralife.com/New+Game+Plus · https://www.gameslearningsociety.org/wiki/can-you-go-past-ng-7-in-elden-ring/
- Absolution / Church of Vows: https://eldenring.fandom.com/wiki/Church_of_Vows · https://www.gamerevolution.com/guides/702887-elden-ring-atonement-reset-npc-hostility
- Passing time and night-only NPCs: https://screenrant.com/elden-ring-locations-passing-time-unlock-content-guide/ · https://twinfinite.net/guides/elden-ring-wait-pass-time-how/
- Offline messages and bloodstains: https://www.techradar.com/news/elden-ring-messages-are-going-nowhere-but-you-can-turn-them-off-heres-how · https://digitaltrends.com/gaming/disable-messages-elden-ring
- Points of no return: https://gamerant.com/elden-ring-point-of-no-return · https://www.pcgamesn.com/elden-ring/dlc-npcs-disappear
- Twin Maiden Husks: https://game8.co/games/Elden-Ring/archives/368949 · https://eldenring.wiki.fextralife.com/Twin+Maiden+Husks
- Invaders: https://eldenring.wiki.fextralife.com/Invaders · https://segmentnext.com/elden-ring/mechanics/npc-invader · https://gamerguides.com/elden-ring-shadow-of-the-erdtree/database/enemies/invaders · https://eldenring.fandom.com/wiki/Fire_Knight_Queelign

**DLC and 2026 content (LIKELY)**
- Leda questline: https://www.pcgamer.com/games/rpg/elden-ring-leda-quest · https://game8.co/games/Elden-Ring/archives/457033
- Ymir/Jolán: https://powerpyx.com/elden-ring-shadow-of-the-erdtree-dlc-ymir-jolan-questline-walkthrough · https://eldenring.wiki.gg/wiki/Count_Ymir
- Hornsent / Shaman Village: https://eldenring.wiki.gg/wiki/Hornsent_(Lore) · https://eldenring.wiki.gg/wiki/Hornsent_Grandam
- Silent DLC bosses / cut lines: https://pcgamer.com/games/rpg/dataminer-unearths-cut-voicelines-from-elden-ring-shadow-of-the-erdtree-s-stoic-final-boss · https://opencritic.com/news/3219/cut-dialogue-for-elden-ring-shadow-of-the-erdtrees-rellana-and-romina-may-have-been-recovered
- Messmer: https://eldenring.wiki.fextralife.com/Messmer+the+Impaler · https://consolepulse.com/multiplatform/elden-ring/guides/elden-ring-messmer-the-impaler-lore-explained
- DLC story: https://kotaku.com/elden-ring-shadow-of-the-erdtree-s-story-explained-1851566799 · https://windowscentral.com/gaming/elden-ring-dlc-how-is-radahn-alive-in-shadow-of-the-erdtree-ending-explained
- Patch 1.17 / Tarnished Pack: https://www.bandainamcoent.com/news/elden-ring-patch-notes-version-1-17 · https://www.rpgsite.net/news/21216-elden-ring-patch-1-17-notes-balance-adjustments-tarnished-pack-dlc · https://en.bandainamcoent.eu/elden-ring/news/how-get-new-elden-ring-classes-weapons-and-torrent-skin-customizations
- Tarnished Pack invaders: https://keengamer.com/articles/guides/how-to-find-knight-leontiel-and-noble-broken-mask-in-elden-ring-tarnished-edition/ · https://www.nintendolife.com/guides/elden-ring-tarnished-edition-weapons-armor-npc-invasions-steed-attire-content · https://eldenring.wiki.gg/wiki/Knight_Leontiel

**Added in the fact-check pass (read 2026-11-06)**
- FrankvdStam/SoulSplitter @aeb7cbe (2025-11-14), `src/SoulMemory/EldenRing/Boss.cs`: boss flags 10000800, 10000850, 16000800, 21010800 and 20010800. https://github.com/FrankvdStam/SoulSplitter
- AinTunez/DarkScript3 @733f024 (2026-03-07), `DarkScript3/Resources/er-common.MapName.txt`: map IDs. https://github.com/AinTunez/DarkScript3
- soulsmods/Paramdex `ER/Names/TalkParam.txt` (7,513 base-game rows; Hyetta at `31001001`, blank 341 rows): https://github.com/soulsmods/Paramdex/blob/master/ER/Names/TalkParam.txt
- SaveForge @ee1042d (2026-09-12), `frontend/src/data/riskInfo.ts` and `backend/db/data/head.go`: regulation-1.17 "Tarnished Edition DLC" content, including Leontiel's armor. https://github.com/oisis/EldenRing-SaveForge
- TGA `Event Flag Manager/.cea`, lines about 1850 (Margit 10000850), 3637 (Metyr 25000800) and 3810–3830 (flag 330, "Sealing Tree status flag")
- elden-ring-eventparam `index.md`, `EFID_common` lines 586–806 (groups, time and weather), `EFID_Talk_NPC899` (merchant common flags), blocks 3000–4259 (lines 11924–11986)

**Lore and style (LIKELY)**
- Lore overviews: https://eldenring.fandom.com/wiki/Lore · https://eldenring.wiki.gg/wiki/Greater_Will · https://www.gamespot.com/articles/elden-ring-story-guide-heres-the-lore-of-the-lands-between/1100-6502537/
- Margit lines: https://eldenring.wiki.gg/wiki/Margit · https://en.wikiquote.org/wiki/Elden_Ring
- Localization and voice: https://www.pcgamer.com/its-a-tier-above-how-a-giant-cryptic-rpg-like-elden-ring-gets-translated

---

## Verification log

The fact-check pass ran on 2026-11-06. Datasets were re-downloaded from raw GitHub into a fresh directory, and SHA-256 matched the author's copies. TGA was sparse-cloned at `393c439`, and every count was recomputed with standalone scripts. Independent cross-checks used SoulSplitter, DarkScript3, Paramdex `TalkParam.txt` and SaveForge. No web search was available, and pcgamer.com, opencritic.com and Fextralife were unreachable, so claims that rest only on news or wiki pages are marked **Unverified**.

Abbreviations in the Evidence column:
- `CSV` = `alidhl/dialogue-generator/ER-Data.csv` @bf5e75d — https://github.com/alidhl/dialogue-generator
- `EP` = `soulsmods/elden-ring-eventparam` `index.md` — https://github.com/soulsmods/elden-ring-eventparam
- `TGA` = `The-Grand-Archives/Elden-Ring-CT-TGA` @393c439, `Event Flags/` — https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA
- `PD-Npc` / `PD-Talk` = Paramdex `ER/Names/NpcParam.txt` / `TalkParam.txt` — https://github.com/soulsmods/Paramdex

| # | Claim | Verdict | Evidence |
|---|---|---|---|
| 1 | Base-game line IDs start with the speaker's talk code: 301 Varre, 216 Blaidd, 106 Ranni ("I am the witch Renna") | **Confirmed** (for 9-digit IDs). *Corrected:* the dump has 8,013 records, not 8,055, including 381 four- and eight-digit cutscene and narration lines that do not follow the layout | CSV: `301001000`…, `216002010` "Kalé send you…", `106001010` "I am the witch Renna." |
| 2 | Human NPCs' `NpcParam` IDs follow `52CCC0000` with CCC = talk code (Yura `523180000`, Leda `524180000`) | **Corrected.** True for about 25 base-game humans and the DLC 4xx rows. Exceptions: Gostoc `36650000`, Hewg `34510100`, Boc's human form `523420000` (342 vs talk 223), and 335 = "Reeling Rico" in `NpcParam` but the cut dreambrew NPC in TalkMsg | PD-Npc rows 523010000…524320000, `36650000`, `34510100`, `523350000` |
| 3 | eventparam labels 20-flag NPC blocks in 3000–4999; 3600–3619 = Blaidd (216), 3611 imprisoned, 3612 gone mad | **Confirmed**, with a correction: 3000–3099 are test, common-event and misc-global blocks, and NPC blocks run 3100–4259 | EP lines 11924–11986 (`3600 | 20 | キャラ状態フラグ_216_狼頭の戦士`); 13856 `3611 牢に囚われている`; 13857 `3612 気が狂った` |
| 4 | +0 alive, +1 hostile (absolvable), +2 hostile (not absolvable), +3 dead; TGA labels 3600/3601/3603 Blaidd alive-friendly/alive-hostile/dead | **Confirmed** for the labels. The runtime semantics (what sets the bits) were downgraded to LIKELY | EP 13845–13848; TGA `Event Flags by ID/.cea` lines 648–650 |
| 5 | 530/531/532 = morning/noon/night; 550–575 = weather | **Confirmed** for the labels; runtime maintenance is **Unverified**. *Corrected:* 557 = a storm variant, 562 = thick fog with rain, 564–565 unnamed | EP `EFID_common` lines 782–806. TGA does not use these flags |
| 6 | TGA `isDefeated`: Godrick 10000800, Rykard 16000800, Messmer 21010800 | **Confirmed** independently. Margit is 10000850 (secondary-boss pattern) | TGA `Event Flag Manager/.cea` lines 1860, 2595, 3673; SoulSplitter `Boss.cs` lines 22, 100, 595 — https://github.com/FrankvdStam/SoulSplitter |
| 7 | 14 of 15 "maidenless" lines are Varre's (301), 1 is Melina's (100) | **Confirmed** | CSV: `100010020`, `301001040` … `301806000` |
| 8 | Archaic forms in >40% of lines for Rennala, Margit/Morgott and Ranni; <3% for Varre, Patches, Blaidd, Boc, Gideon and others | **Corrected.** Rennala 57.5% and Margit 49.0% are exact. Ranni is 37.2% with thee/thou/thy/ye only and 43.1% when hath/art/mine are included, so ">40%" depends on the regex. The listed commoners are all 0% | Recount over 9-digit CSV lines (scripted) |
| 9 | No base-game NPC invader has a line in the dump | **Confirmed** for the dump. Invader codes (304, 327, 339, 343–347, 353) never occur as TalkMsg prefixes. Caveat: 335 is Rico in `NpcParam` but a different NPC in TalkMsg | CSV code inventory; PD-Npc |
| 10 | Irina and Hyetta share code 310; TalkMsg 310 opens "My name is Irina"; 310 flag-stage labels describe Hyetta's grapes and Frenzied Flame | **Confirmed**, three ways: TalkParam `31001001` "My name is Hyetta"; `NpcParam` `523100010` "Lightseeker Hyetta"; TGA category "Irina / Hyetta". A separate flag block `341_狂った指巫女` exists without stage labels | EP lines 16628–16642 (3385–3394); PD-Talk; PD-Npc; TGA |
| 11 | Rellana, Romina and Promised Consort Radahn have no dialogue in the shipped DLC (cut lines datamined) | **Unverified.** No DLC TalkMsg export was found, and news hosts were unreachable. The claim rests on the article headlines cited by the author | (pcgamer.com and opencritic.com unreachable) |
| 12 | TGA labels 76800 "Gravesite Plain" grace; 418 grace flags across 53 sub-regions | **Confirmed**: 382 three-part entries over 48 sub-regions plus 36 two-part entries over 5 legacy dungeons. Caveat: a grace flag is set only on touch | TGA `Event Flags by ID/.cea` line 367 `76800:Realm of Shadow - Gravesite Plain - Gravesite Plain` |
| 13 | "Same talk code appears in TalkMsg, flag labels and `NpcParam`; three datasets agree" | **Corrected.** Seluvis has flag label 306 but talk/`NpcParam` 307. Hyetta has a stray 341 flag block. Boc's human form is 342 | EP line 11952 `3560 … 306_傀儡師`; CSV `307001000`; PD-Npc `523070000` |
| 14 | `…0850/0851` = defeated/encountered | **Corrected.** `…0800`/`…0801` is the primary boss of a block and `…0850`/`…0851` a secondary one (Margit) | TGA `.cea` lines 1850–1851, 1860–1861; SoulSplitter `Boss.cs` line 25 |
| 15 | Misbegotten roar directions are the 746xxxxxx block | **Corrected** to LIKELY Troll. Voice blocks follow `7`+`NpcParam` family: 7405 Kaiden, 7434 Mad Pumpkin Head, 7438 Stonedigger, 7460 Troll | CSV `740500100`, `743400110`, `743820100`, `746000100`; PD-Npc `40500000`, `43400000`, `43820000`, `46000000` |
| 16 | Lexicon count "Omen (44)" | **Corrected** to 19. 44 counted substrings such as "moment" and "women". "grace" is 82 as a whole word (88 as a substring) | Recount |
| 17 | Map IDs m19 Elden Beast, m22 Stone Coffin Fissure, m25 Manus Metyr, m28 Midra's Manse | **Confirmed**: m19 = "Stone Platform", m25 = "Finger Birthing Grounds". DLC mini-dungeons m40–m43 added | DarkScript3 `er-common.MapName.txt` — https://github.com/AinTunez/DarkScript3; TGA boss flags 25000800, 28000801 |
| 18 | Volcano Manor target labels "345/346" | **Corrected** for precision: these are talk codes 345/346/347 inside conversation-flag labels at 1042399300, 1039539200 and 1050569200, matching Istvan, Rileigh and Juno in `NpcParam` | EP lines 12119, 12172, 12282; PD-Npc `5234[5-7]0000` |
| 19 | Melina flag 4680 "Torrent whistle"; merchants 4700–4753; red-eyed merchant 4980–4986 | **Confirmed** | TGA lines 810, 617–639; EP `EFID_Talk_NPC899` |
| 20 | Rykard's Great Rune label グールの王 is confirmed by TGA 9122 | **Confirmed** | TGA `.cea` line 2597 `FLAG(9122) -- Defeat the boss King of ghouls - ボス撃破 グールの王` |
| 21 | First Step tile `m60_42_36` holds conversation flags for Varre, Kalé, Ranni and Melina | **Confirmed** | EP lines 12158–12162 (`1042369200…450`) |
| 22 | Tanith stage list ("carrying out contracts → audience granted …") | **Corrected** to the literal labels (in manor → before boss room → appears after boss …) | EP blocks 3105–3110 |
| 23 | DLC NPC rows and locations (Leda `524180068` Mohgwyn …) | **Confirmed** for the labels. Dane's location was added (Moorth Ruins). Queelign's "invades Belurat / Church of the Crusade" is downgraded: Paramdex places his NPC row at Shadow Keep, Church District | PD-Npc `5240x/5241x/5242x` rows |
| 24 | Spirit ash "Jolán and Anna" exists | **Confirmed** | PD-Npc `150000004` |
| 25 | TGA targets App ver. 1.17.2 | **Confirmed** | TGA `README.md` line 23 |
| 26 | Patch 1.17 added Tarnished Pack content including Knight Leontiel (2026-08-28) | **Partly confirmed.** Leontiel armor items exist in regulation 1.17 per SaveForge, which names the content "Tarnished Edition DLC". The date, the invaders and "Noble Broken Mask" are Unverified | SaveForge `riskInfo.ts`, `head.go` — https://github.com/oisis/EldenRing-SaveForge |
| 27 | Messmer speaks; Midra, Bayle and Gaius unknown | **Unverified** (no DLC text source reachable) | — |
| 28 | Quotes used as style references (Hewg "I took you for dead", Gurranq, Boc "cully", Jar Bairn "Hallo coz", intro lines) | **Confirmed**, except that Gurranq's quote is "Feed me more...Death." (`202003020`), not "Feed me...Death" | CSV IDs as cited |
| 29 | Body-type variants at ID+1 (`201002150`/`201002151`) | **Confirmed** | CSV |
| 30 | "Modded play is offline; no messages or invasions" | **Corrected (caveat).** Seamless Co-op and Reforged 2.0 provide co-op, invasions and messages through their own networks | Doc 02 §2.3 |
