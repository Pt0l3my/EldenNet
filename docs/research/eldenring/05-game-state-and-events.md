# Elden Ring game state and events an in-process mod can read for NPC awareness

> **Scope:** which game state and which events a native DLL inside `eldenring.exe` can observe, so that EldenNet NPCs can be "aware". For each item: what it is, how it is reached, which public library already exposes it, how stable it is across patches, and what it is worth to the AI.
> **Research date:** 2026-11-06. **Game:** ELDEN RING PC (Steam), App ver. **1.17.2** (exe 2.7.2.0 WW / 2.7.2.1 JP), Regulation ver. 1.17.3 (regulation number not re-verified in the fact-check), Shadow of the Erdtree included.
> **Evidence tags:** **FACT** = read in source code, a paramdef, an EMEDF or a primary data dump. **LIKELY** = several weaker signals agree. **SPECULATION** = my inference, untested.
> **Method:** shallow clones read on 2026-11-06, never built or executed. soulsmodding.com could not be reached from this session (DNS blocked), so its flag lists were replaced by the `soulsmods/elden-ring-eventparam` dump and the TGA table. Companion reports: [01-engine-internals](01-engine-internals.md) (containers, singletons, flag storage), [02-modding-ecosystem](02-modding-ecosystem.md), [03-dialogue-and-presentation](03-dialogue-and-presentation.md).

> **Fact-check pass (2026-11-06).** An adversarial review re-checked the flagged claims against the cloned sources and independent repos (er-mods-rs, lraty-li/Elden-Ring-Mods, DarkScript3 EMEDF, TGA's pointer chains). Corrections are made inline and are marked *(corrected)*. Every verdict and its evidence is in the [Verification log](#verification-log). The biggest corrections: (1) the TGA "Targeted Npc Info → Name" chain reads `PlayerIns.player_game_data`. On an `EnemyIns` the same `+0x580` slot is `com_manipulator`, so it is **not** evidence that NPCs carry a name block; (2) TGA's NpcThinkParam chain `[[[chr+0x58]+0x18]+0xC0]+0x30` points to a **fixed** ChrIns → AI-object path, which weakens MapForGoblins' "no fixed offset" conclusion; (3) boss-kill flags run 9100–**9295**, and the DLC global kill flags exist (Messmer 9146, Bayle 9163, Promised Consort Radahn 9143); Mohg, Lord of Blood is **9112**; (4) flag 180 means "owns **0** Great Runes", not "≥ N"; (5) the `07xx` entity suffix has exceptions (Count Ymir `20010455`, Gurranq `1051430800`), and the MapForGoblins table was mined from **ERR's** EMEVD, not vanilla's; (6) SpEffect 9621 is applied in single player only in Roundtable Hold (elsewhere only during invasions); (7) the subarea pop-up is EMEVD `2007[13]`, not `2003[13]`; (8) the freeze was live-tested only via `CreateRemoteThread` on 2.6.2.0. Note: web search was exhausted and bandainamcoent.com was unreachable during the fact-check, so the patch level was not re-checked from the web (local evidence only: TGA v1.19.2 changelog "Supported version to ELDEN RING App Ver. 1.17.2", er-mods-rs `docs/game-build.md` "2.7.2.0 (Steam patch 1.17.2, installed 2026-10-20)").

Repositories read (commit, date):

| Repo | Commit | Notes |
|---|---|---|
| `vswarte/fromsoftware-rs` (crate `eldenring`) | `5702315` (2026-10-24) | RVAs only for 2.7.2.0 / 2.7.2.1 |
| `Banon-Labs/er-mods-rs` | `c107d31` (2026-10-28) | Ghidra-backed notes, 1.16.2 to 1.17.2 |
| `The-Grand-Archives/Elden-Ring-CT-TGA` | `393c439` (2026-10-22, v1.19.3) | "Requirements: App ver. 1.17.2" |
| `quentin452/ERR-MapForGoblins-DLL` | `60a63f2` (2026-08-15) | RE notes pinned to exe 2.6.x (1.16.x) |
| `soulsmods/elden-ring-eventparam` | `4d20990` (2024-04-12) | Pre-DLC flag dump |
| `FrankvdStam/SoulSplitter` | `aeb7cbe` (2025-11-14) | Autosplitter with base and DLC boss flags |
| `AinTunez/DarkScript3` | `733f024` (2026-03-07) | `er-common.emedf.json`, name tables |
| `Nordgaren/Erd-Tools`, `Dasaav-dsv/libER`, `iArtorias/elden_pause` | `a2757f9`, `d8ae5c9`, `9f6d4e7` | Older, used for cross-checks |
| `soulsmods/Paramdex` `ER/Names/*.txt` (raw) | master | NpcParam/CharaInitParam row names |

---

## TL;DR

- **Almost everything v1 needs is readable without hooks.** A recurring `CSTaskImp` task on the game thread can snapshot the player, the nearby characters, the HUD state and event flags every ~250 ms. Events come from diffing those snapshots. fromsoftware-rs already models most of it: `PlayerGameData`, `GameDataMan`, `WorldChrMan`, `ChrIns` and its modules, `CSFeManImp`, `CSMenuManImp`, `CSEventFlagMan`, `WorldAreaTime` and `FieldArea` (FACT).
- **Player identity is cheap and rich.** `PlayerGameData` exposes the name (`[u16;17]`), level, runes held, all eight attributes, HP/FP/stamina, class (`archetype`), keepsake (`starting_gift`), every equipped item as an `ItemId`, the equipped Great Rune, Scadutree and Revered Spirit Ash blessing levels, `multiplay_role`, `is_my_world` and `visited_areas`. `GameDataMan` adds `death_count`, `play_time`, `ng_lvl`, `just_died` and **`boss_fight_active` plus `boss_health_bar_npc_param_id`** (FACT for the fields; the bar-related semantics are LIKELY from the names, see §8.4).
- **"Boss fight started" is solved.** Read `GameDataMan.boss_fight_active` and `CSFeManImp.boss_health_displays[3]` (`fmg_id`, `field_ins_handle`, `damage_taken`). Persistent outcomes are in flags: `AABB0800` defeated and `AABB0801` encountered, e.g. Godrick 10000800 / 10000801 (FACT, TGA labels). The convention has exceptions (Margit `10000850`, Starscourge Radahn `1252380800`).
- **"Is the player in combat" has no clean global.** The engine's `IsBattleState` reads an AI-FSM field (`== 6`) on a `CS::CSAiThink` object (FACT per MapForGoblins static RE on 2.6.2.0, one source). MapForGoblins concluded that the object is linked to its `EnemyIns` only through an AI manager and dropped detection. *(corrected)* That conclusion is weaker than first stated: TGA reaches an NpcThinkParam row through the fixed chain `[[[ChrIns+0x58 chr_ctrl]+0x18 manipulator]+0xC0]+0x30` on 1.17.2, which suggests a fixed ChrIns → AI-object path (LIKELY, untested; §8.1). v1 should still use a heuristic built from HP deltas, `last_hit_by`, HUD enemy tags, the lock-on target and the boss bar (SPECULATION).
- **Damage has no public hook.** The hit path is known (`CSChrDamageModule` vtable slot 7 → `HitChr`, documented by er-mods-rs), but `AttackDamageInfo` is not mapped publicly. v1 should infer "X hit Y for N" from per-character HP deltas plus `ChrIns.last_hit_by`, and use `CSChrActionFlagModule.received_damage_type` (FACT fields) for the attack type.
- **Event flags are the progression backbone and the most patch-stable data.** Examples: boss kills 9100–9295 *(corrected; minor-dungeon bosses use 9200–9295)*, Great Rune possession 191–197 and counts 180 (zero) / 181–187 (≥ 1 … ≥ 7), endings 20–22, time 530–532, weather 550–575, "resting at grace" 9000 (set and cleared by the grace talk ESD), grace flags 71000+/76000+, and NPC quest bands in 3000–4999. Read them with `CSFD4VirtualMemoryFlag::get_flag` (FACT).
- **Named vs generic is a data problem, not an engine flag.** MapForGoblins counts 7,039 vanilla `NpcParam` rows, of which only 715 have a `nameId` (single source, not re-counted; Paramdex lists 6,864 named rows, which fits the total). The best signals are a talk ESD on the placement, a resolvable NpcName, a common (not universal) entity-ID suffix `07xx`, and a `52xxxxxxx`/`53xxxxxxx` humanoid NpcParam ID. A curated allowlist should be the ground truth for v1 (LIKELY).
- **Talk and menu state are reachable.** `CSNpcTalkIns` holds `talk_param_id`, the NPC `field_ins_handle` and `menu_state.current_open_menu` (`MenuType`: TalkList, RegularShop, …). `CSFeManImp.hud_state` reports ESC and popup menus. Hooking `CSEzStateTalkEvent::invoke` (vtable RVA in fromsoftware-rs) gives every talk command an NPC runs (FACT/LIKELY).
- **Typing does not pause the game.** The best lever is the engine's own cutscene freeze `CS::CSEventUtility::SetDisableAllChrUpdate(bool)`. It freezes every character and resumes instantly (MapForGoblins, verified live on 2.6.2.0, but only by calling it from an external `CreateRemoteThread`, not from a game task). fromsoftware-rs also exposes a per-character `ChrDebugFlags.disabled_updates` bit, which is a possible crate-backed alternative (untested). Add input capture as done in er-mods-rs `er-overlay-input`. Single-player only.
- **Fragility:** fromsoftware-rs `rva::get()` panics on any exe other than 2.7.2.0 (EN) / 2.7.2.1 (JP). The panic fires lazily, on the first RVA-backed call (`GameDataMan`, `GameMan`, `FieldArea`, vtable hooks). Singleton-backed types (`WorldChrMan`, `CSEventFlagMan`, `CSFeMan`) do not go through it. 1.17.x support exists only on git `main`; the newest crates.io release (0.14.0) targets 1.16.2 (see 02). Struct layouts moved in both 1.17.0 and 1.17.2. Flag, param and FMG IDs are data and are far more stable than offsets (LIKELY). Any later *hooks* also have to deal with Arxan code restoration and require EAC off, i.e. offline play (01 §2, 02 §2).

---

## 1. Access model and stability tiers

**Threading (FACT):** game objects must be read on the game thread. fromsoftware-rs documents `PlayerIns::local_player()` as safe "only … on the main thread", and examples register work with `CSTaskImp` in a `CSTaskGroupIndex` phase. The perception loop should copy plain values into a snapshot struct inside the task. Serialization and LLM work then run on other threads.

**How each class of data is found:**

| Mechanism | Examples | Stability across patches |
|---|---|---|
| FD4Singleton finder (DLRF name) | `WorldChrMan`, `CSEventFlagMan`, `CSFeMan`, `CSMenuMan`, `WorldAreaTime`, `CSSessionManager`, `CSLuaEventMan`, `CSHavokMan`, `SoloParamRepository` | Discovery survives patches; **field offsets do not** |
| Per-version RVA (`rva_ww.rs` / `rva_jp.rs`) | `GameDataMan`, `GameMan`, `FieldArea`, vtables (`cs_ez_state_talk_event_vmt`, `cslua_event_msg_exec_*_vmt`), `cs_menu_man_imp_display_status_message`, `cs_phys_world_cast_ray` | Regenerated every patch. The crate refuses unknown exes: `ERGameVersion::from_lang_version` only accepts `"2.7.2.0"` (EN) and `"2.7.2.1"` (JP), and `rva::get()` does `panic!` on anything else (`crates/eldenring/src/rva.rs`, verified at `5702315`). A panic inside a game-task closure takes the game down (02), so check the PE version yourself before touching any RVA-backed type |
| Custom AOB (not in the crate) | `MsgRepositoryImp::GetMessage`, damage functions, `SetDisableAllChrUpdate`, flag-setter hook, TGA "Last Hit"/"Targeted" sites | Patterns often survive; must be re-validated per patch |
| Game data IDs | Event flags, NpcParam/BonfireWarpParam rows, FMG text IDs, SpEffect IDs | Authored data. Rarely renumbered by FromSoftware (LIKELY). **Overhaul mods (ERR, Convergence) change params and FMGs** |

**Layout drift is real (FACT, git history):** `b080915` "[ER] Update some strucs for 1.17" (2026-08-28) and `5baf0dd` "Update shifted WorldChrMan fields" (after 1.17.2). Every offset in this report is version-pinned. Only names and IDs carry over.

---

## 2. Player identity and stats

All rows are FACT from `cs/player_game_data.rs`, `cs/game_data_man.rs` and `cs/chr_ins.rs` unless marked. Access path: `GameDataMan::instance()?.main_player_game_data` or `PlayerIns::local_player()?.player_game_data`.

| Item | Field / access | Crate | AI value |
|---|---|---|---|
| Character name | `PlayerGameData.character_name: [u16; 17]` (UTF-16, offset `0x9C` on 1.17.2 by field arithmetic; er-mods-rs independently calls `+0x9c` "the raw PGD name"). Caveat: the game's own name getter (2.7.2.0 `0x25f8f0`) reads a second string at `PlayerGameData+0x8e8`, which "can hold the shortened display name" (er-mods-rs `er-quickload/src/constants/stats_panel_text.rs`). Use the raw `+0x9C` name | yes | High: NPCs address the player by name |
| Level, runes held, lifetime runes | `level`, `rune_count`, `rune_memory` | yes | High: "you reek of runes", wealth remarks |
| Attributes | `vigor`…`arcane`, plus `effective_*` after buffs | yes | Medium: describe build ("a sorcerer's frame") |
| HP / FP / stamina | `current_hp`, `current_max_hp`, `current_fp`, `current_stamina` (+ max). Live HP is also `ChrIns.modules.data.hp/max_hp` | yes | High for barks ("you're bleeding out") |
| Class / origin | `archetype: u8`. TGA dropdown: 0 Vagabond, 1 Warrior, 2 Hero, 3 Bandit, 4 Astrologer, 5 Prophet, 6 Confessor, 7 Samurai, 8 Prisoner, 9 Wretch; 10 "Idus Knight", 11 "Heavy Knight" (TGA `ArcheTypeIdDLC3`). *(corrected)* Which product added classes 10/11 was not verified; the earlier "Tarnished Pack" label is unsourced | yes (enum names from TGA) | Medium: origin flavour |
| Keepsake, gender, voice | `starting_gift`, `gender`, `voice_type` | yes | Low/Medium: pronouns |
| Equipped gear | `equipment.equipment_entries: ChrAsmEquipEntries`: weapons L/R ×3, arrows/bolts, `protector_head/chest/hands/legs`, `accessories[4]` (talismans), `quick_tems[10]`, `pouch[6]`; spells in `EquipMagicData.entries[14]`; `physick_tears[2]` | yes (IDs) | High: "a Moonveil? You're no ordinary Tarnished" |
| Gear names | `ItemId` (category in high bits) → FMG. Slots (MapForGoblins `goblin_messages.cpp`): Weapon {410,310,11}, Protector {413,313,12}, Accessory {416,316,13}, Goods {419,319,10}, AoW {422,322,35,42} (DLC02, DLC01, base) | **no**: needs `GetMessage` AOB | Required for any gear-aware line |
| Equipped Great Rune | `EquipItemData.great_rune: EquipDataItem` (gaitem handle). TGA reads it as goods `0xB00000BF` Godrick, `C0` Radahn, `C1` Morgott, `C2` Rykard, `C3` Mohg, `C4` Malenia | yes | Medium |
| Great Runes owned | Flags **191** Godrick, **192** Radahn, **193** Morgott, **194** Rykard, **195** Mohg, **196** Malenia, **197** Rennala ("大ルーン所持チェック", possession *check*). *(corrected)* **180** = "count check: 0", **181–187** = "≥ 1" … "≥ 7" (eventparam `EFID_common`). The dump names 193/194/195 by dev codenames (マルギット "Margit", グールの王 "Ghoul King", グレーターデーモン "Greater Demon"). The Rykard/Mohg mapping is FACT by cross-reference: TGA ties the same codenames to Rykard (kill flag 9122) and Mohg, Lord of Blood (9112), and flags 191–196 match goods IDs 191–196 (0xBF–0xC4). "Check" suggests EMEVD evaluates them, so they may lag item pickup (LIKELY) | via flags | High for story NPCs |
| Rune Arc active | `rune_arc_active` | yes | Low |
| DLC blessings | `scadutree_blessing`, `reversed_spirit_ash` | yes | Medium in the Realm of Shadow |
| Flasks | `max_hp_flask`, `max_fp_flask` | yes | Low |
| Deaths, playtime, NG+ | `GameDataMan.death_count`, `play_time` (ms), `ng_lvl`; flags 50–58 "cycle N cleared" | yes | Medium: "you've died a great many times" |
| Places visited | `PlayerGameData.visited_areas: FNVector<u32>` ("all visited play area IDs") | yes | Medium: memory seed ("you've seen Caelid?") |
| Save slot | `GameMan.save_slot` | yes | Sidecar memory key |

**Stability:** names and field semantics are stable. Offsets are not. `PlayerGameData` was reworked in 1.17 (commit `b080915`).

---

## 3. Player status flags

| Status | How to read it | Evidence | Robustness |
|---|---|---|---|
| **Dead / just died** | `ChrIns.chr_flags1c5.death_flag` (bit 7), HP == 0, `GameDataMan.just_died`, `death_count` increment | FACT (`chr_ins.rs`, `game_data_man.rs`) | High |
| **"YOU DIED" and other banners** | `CSFeManImp.frontend_values.full_screen_message_request_id` (a `FeTextEffectParam` id), `CSDeathRestartEvent.full_screen_message_id`. Banner ids in `menu_man.rs`: 1 DEMIGOD FELLED, 2 LEGEND FELLED, 3 GREAT ENEMY FELLED, 4 ENEMY FELLED, 5 YOU DIED, 7 HOST VANQUISHED, 11 LOST GRACE DISCOVERED, 17 MAP FOUND, 21 GREAT RUNE RESTORED, 22 GOD SLAIN | FACT for the fields and constants. LIKELY that vanilla banners write these same fields | Good event source |
| **On Torrent** | `ChrIns.modules.ride.ride_node.ride_state`: 0 none, 3 getting on, **5 riding**, 7 dismounting; `is_mounted`; `PlayerGameData.mount_handle`. Torrent is `character_id == 8000` and lives in `WorldChrMan.summon_buddy_chr_set` | FACT for the fields. The state values come from a crate doc comment (`CSRideNode.ride_state`), i.e. the authors' observation, not an enum from the binary | High |
| **Mount/dismount event** | Lua events `OnEnterRideObj` 4065 / `OnLeaveRideObj` 4066 (`LuaEventId`) | FACT for the IDs. A dispatch hook is needed (§8.5) | Optional |
| **Multiplayer: invaded / summoned / host** | `PlayerGameData.multiplay_role` (`Host`, `WhiteSummon`, `RedInvasionA`, …), `is_my_world`, `ChrIns.chr_type` (`Local`, `WhitePhantom`, `BloodyFinger`, …), `CSSessionManager` `LobbyState` (`None`/`Host`/`Client`…). Flags 200 host / 201 guest | FACT | High. Use it to **disable EldenNet** |
| **Resting at grace** | Flag **9000** "篝火休息中フラグ" (resting at bonfire). Related Lua events: `OnEvent_BonfireLvUp` 4081 (level-up at grace), `OnEvent_BonfireRespawn` 4082 | FACT for the name (eventparam). Runtime behaviour confirmed independently: the grace talk ESD `t000001000` does `SetEventFlag(9000, 1)` right after `StartBonfireAnimLoop` and `SetEventFlag(9000, 0)` when the player stands up, and `common.emevd` clears it on load (decompiled copies in lraty-li/Elden-Ring-Mods `Gacha_Ring/t000001000.py`, `emved/common.emevd.dcx.js`, 2023, a modded copy of the vanilla script). So the flag covers the whole time the player is seated, menus included | High |
| **Safe zone (Roundtable)** | Active SpEffect **9621** (`stateInfo` 457, "Disallow Hostile Actions") in `ChrIns.special_effect.entries()` | FACT (er-mods-rs `docs/er-mechanics/map-combat-restrictions.md`, 1.17.1: "the only row in the regulation with `stateInfo = 457`"). *(corrected)* In single player only Roundtable Hold (`m11_10`, event 11102600) applies it, outside one lower region and outside ceremony 20. Stormveil, Raya Lucaria, Volcano Manor and Mohgwyn apply it only while the multiplayer state is `Invasion`. It is a Roundtable flag, not a general "safe zone" signal | High (Roundtable only) |
| **Locked on** | `PlayerIns.locked_on_enemy: FieldInsHandle` | FACT | High |
| **In legacy map** | Flag 211 "set when I am in a legacy map"; BlockId area < 50 | FACT for the flag name (eventparam). Runtime behaviour LIKELY | Medium |
| **In combat** | No reliable global (§8.1) | — | Heuristic |
| **Status effects** | `PlayerGameData.resistance_gauges[7]`, `proc_status_timers[7]`; SpEffect list | FACT | Medium for barks ("you're frostbitten") |

---

## 4. Location

**Map identity (FACT, `cs/block_id.rs`, `cs/chr_ins.rs`):** `PlayerIns.current_block_id: BlockId` packs `mAA_BB_CC_DD`. Also on `PlayerIns`: `block_position` (block-local metres), `safe_block_id`/`safe_block_pos` (last save-safe spot) and `play_region_id`. Every `ChrIns` has `block_id`, `chunk_position` (global) and a Havok position in `modules.physics.position`.

**Classifying the place (FACT for the ranges, DarkScript3 `er-common.MapName.txt`, 665 entries):**
- m10–m16 and m18/m19: base legacy dungeons and set pieces (m10 Stormveil, m11 Leyndell and Roundtable `m11_10`, m12 underground rivers, m13 Farum Azula, m14 Raya Lucaria, m15 Haligtree, m16 Volcano Manor).
- m20–m28: DLC legacy dungeons (m20 Belurat, m21 Shadow Keep, m22 Stone Coffin, m25, m28 Midra's Manse).
- m30–m35, m39: catacombs, caves, tunnels, divine towers and others. m40–m43 are their DLC equivalents. m45 is the colosseums.
- m60: Lands Between overworld. m61: Realm of Shadow.

`WorldBlockInfo.world_area_info_index == -1` marks an overworld block (FACT, `cs/field_area.rs`). Rule: overworld = area 60/61; interior = everything else; "legacy" = area < 30, with m12 underground as a special case (SPECULATION, from the naming table).

**Place-name text:**
- `FieldArea.map_place_name_id` and `save_map_name_id` (FACT fields, LIKELY PlaceName FMG ids). Also `current_play_region_id`, which indexes `PlayRegionParam` (a multiplayer region, not a name).
- er-mods-rs builds a save-independent name from the game's own params. `WORLD_MAP_PLACE_NAME_PARAM_ST` and `BONFIRE_WARP_PARAM_ST` both key a PlaceName `textId` by `(areaNo, gridXNo, gridZNo)`. The world-map table names overworld tiles, and the grace table names legacy interiors (FACT, `crates/er-profile-summary-core/src/map_place_names.rs`).
- The subarea pop-up the player just saw is `CSFeManImp.subarea_name_popup_message_id` (FACT field). *(corrected)* The crate's doc comment says it is set by EMEVD "`2003[13]` DisplaySubareaWelcomeMessage". DarkScript3's `er-common.emedf.json` lists that instruction as **`2007[13]` "Display Subarea Welcome Message"**; `2003[13]` is "Modify Navimesh Connection Bitflag". Trust the EMEDF index. It is a good "arrived at X" event.

**Nearest grace (FACT, paramdef `BonfireWarpParam.xml`):** each row carries `eventflagId` (discovered/lit), `bonfireEntityId`, `areaNo/gridXNo/gridZNo`, `posX/Y/Z`, `textId1..8` (PlaceName). Compute the nearest grace from the player's block and position. The last rested grace is at `GameMan+0xB60` as a grace entity ID (TGA "Last Grace", values like `10002950` "[Stormveil Castle] Godrick the Grafted"). fromsoftware-rs leaves that field as `unkb60`, so it is FACT for 1.17.2 only.

**Coordinates (FACT, `position.rs`):** block-local, global/chunk, Havok and map-UI spaces all exist. Use Havok or block metres for distances. Map-UI space is only for "on the map, NE of …" phrasing.

**AI value:** high. Location strings feed every prompt ("in the Weeping Peninsula, near the Castle Morne Rampart grace"). Interior/overworld drives generic-enemy barks and ambience.

---

## 5. Time of day and weather

- **Clock (FACT, `cs/world_area_time.rs`):** `WorldAreaTime.clock: DLDateTime` (packed hours/minutes/seconds), `time_passage_multiplier`, `target_hour/minute/second` and `request_time(h,m,s)`. `GameMan.world_area_time` is a "backup of local time when player is in someone else's world". The crate also has `AiSightTimeOfDay {Morning, Noon, Evening, Night, Midnight}`, which shows the engine buckets time for AI sight.
- **Time flags (FACT names, LIKELY runtime-maintained):** 530 morning, 531 noon, 532 night ("時間フラグ").
- **Weather:** a `WorldAreaWeather` singleton exists (libER `symbols/singletons.csv`, 1.16) but its layout is not public. Use flags **550–565** for specific weather (550 clear, 551 sunny, 552 cloudy, 554 rain, 555 heavy rain, 556 storm, 558 snow, 559 heavy snow, 560 fog, 561 dense fog, 563 sandstorm) and **570–575** for families (sunny, cloudy, rain, snow, fog, other) (FACT names from eventparam; runtime upkeep LIKELY, verify live). EMEDF `3[31] IF Weather Active` and the `Weather` enum (0 Default, 1 Rain, 2 Snow, 4 Fog, …) confirm the vocabulary (FACT, `er-common.emedf.json`).
- **World events (FACT names):** 300 "天変地異：世界樹炎上" (cataclysm: Erdtree burning), 310 "天変地異：緑隕石" (cataclysm: green meteorite; that this is the star that falls after Radahn is an interpretation, SPECULATION), 118 "world tree burned".
- **AI value:** medium. Cheap, flavourful ("a cold night for travel"), and it affects which NPCs appear (some are night-only).

---

## 6. Nearby characters

### 6.1 Enumeration

**FACT (`cs/world_chr_man.rs`):** `WorldChrMan.chr_inses_by_distance: DLVector<ChrInsDistanceEntry{chr_ins, distance}>` is "a list of ChrIns references sorted by distance to the main player". This is the natural perception query: iterate until `distance > R`. Fallbacks are `chr_sets[196]` / `world_block_chr[192]` and `player_chr_set`, `summon_buddy_chr_set` ("spirit ashes as well as Torrent") and `ghost_chr_set`. Resolve handles with `WorldChrMan::chr_ins_by_handle(&FieldInsHandle)`.

**Caution (FACT, MapForGoblins `docs/re/combat_enemy_list_structure_re_prompt.md`, 1.16.2):** a raw walk of the per-block `EnemyIns*` array at `block+0x18` went "from ~33 EnemyIns to zero" minutes apart. It behaves like a streaming pool. Use the crate's typed containers and re-resolve every tick. Never cache raw pointers across ticks or map loads.

### 6.2 Per-character fields

| Field | Access (FACT unless noted) | AI value |
|---|---|---|
| Handle | `ChrIns.field_ins_handle` (runtime-only) | Per-tick identity |
| Entity ID | `ChrIns.event_entity_id` (MSB/EMEVD ID; 0 for many generics, open question) | Persistent identity for placed NPCs |
| NpcParam ID | `ChrIns.npc_param_id`; `SoloParamRepository::get::<NPC_PARAM_ST>(id)` | Classification, name |
| Model | `ChrIns.character_id` (`npc_id`), e.g. 8000 Torrent. Names in DarkScript3 `er-common.ModelName.txt` (`c3000 Exile Soldier`, `c4300 Wandering Noble`, …) | Describing generic enemies |
| Name | `NpcParam.nameId` → NpcName FMG {428,328,18}; boss bars use the NpcName **9xxxxxxxx** band (`903251600` Tree Sentinel), authored per map in EMEVD `2003[11] Display Boss Health Bar(..., nameId)` (MapForGoblins `windows_enemy_name_runtime_source_re_findings.md`) | Named NPCs and bosses |
| Humanoid NPC name (alt.) | *(corrected, now SPECULATION)* TGA "Targeted Npc Info → Name" reads `[chr+0x580]+0x9C` (`Scripts/Helpers/Targeted Npc Info/Name.xml`). In the fromsoftware-rs 1.17.2 layout `ChrIns` ends at `0x580`. On a `PlayerIns`, `+0x580` is `player_game_data` (so `+0x9C` is `character_name`). On an `EnemyIns`, `+0x580` is `com_manipulator` (`cs/chr_ins.rs`). `WorldChrMan.player_chr_set` is `ChrSet<PlayerIns>` "holding the players". The TGA entry therefore yields a real name only when the target is a `PlayerIns` (players, phantoms). It is **not** evidence that c0000 NPCs carry a name block. Whether NPC invaders/phantoms are `PlayerIns` is unverified | Do not use for v1 |
| Team | `ChrIns.team_type: u8` (runtime, changed by EMEVD `2004[02]`). EMEDF enum: 0 Disabled, 6 Enemy, 7 Strong Enemy, 8 Ally, 15 Charmed, 26 Friendly NPC, 27 Hostile NPC, 29 Indiscriminate, … | Hostility |
| Hostility vs player | Two 79×79 `CSTeamTypeRelation` matrices: damage (`CanTeamTypeDamageEachOther`) and targeting (`ChrIns::CanTargetTeamType`). Roundtable NPCs have team 0 and are not valid targets (er-mods-rs `netdamage.rs`, `map-combat-restrictions.md`). fromsoftware-rs has the `CSTeamType{Neutral,Friend,Enemy,Rival}` classes but not the matrix | Friend/foe line selection |
| HP / FP / stamina | `modules.data.{hp,max_hp,fp,stamina}` | "It's wounded" |
| Distance | `distance_to_player_sqr`, `horizontal_distance_to_player_sqr`, or the sorted list | Bark gating |
| On screen | `chr_flags1c4.is_onscreen` ("in camera's view frustum, doesn't consider walls") | "Can the player see it" proxy |
| Line of sight | `CSHavokMan.phys_world.cast_ray(filter, origin, delta, owner)` (RVA `cs_phys_world_cast_ray`). Filter semantics undocumented. Friendly HUD tags already carry `is_line_of_sight_blocked` | Hearing vs seeing |
| Dead | `chr_flags1c5.death_flag`; `chr_flags1c6.has_dropped_runes` | Kill events |
| Last attacker | `ChrIns.last_hit_by: FieldInsHandle` | "Who hit whom" |
| Last attack type | `CSChrActionFlagModule.received_damage_type` ("Param ID of the last received attack"), `damage_level` | Hit flavour |
| Active SpEffects | `special_effect.entries()` → `param_id` | Buffs, status, safe zones |
| Animation | `CSChrTimeActModule.anim_queue[10]` (`anim_id`, `play_time`) | Later: gestures, emotes |
| AI think params | `EnemyIns.npc_think_param` (ID). TGA also reaches the live NpcThinkParam **row** via `[[[[chr+0x58]+0x18]+0xC0]+0x30]` (FACT for 1.17.2 TGA chain; `+0x58` = `ChrIns.chr_ctrl`, `+0x18` = `ChrCtrl.manipulator` in fromsoftware-rs) | Perception radii (NpcThinkParam `eye_dist`, `ear_dist`, `BattleStartDist`) |
| AI state (battle/alert) | *(corrected)* No public, tested offset. MapForGoblins found none (§8.1). The TGA chain above suggests the object at `[[chr+0x58]+0x18]+0xC0` may be the `CSAiThink` (SPECULATION, needs a live RTTI check) | Later |
| Is talking | No `ChrIns` field. Comes from the talk system (§10) | Conversation locking |

### 6.3 Named important NPC vs generic enemy

No single engine flag exists. Ranked signals:

1. **Has a talk script.** Each interactable placement has a `TalkID` (MSB `PartEnemy.talkID`) that selects `t<id>.esd`. fromsoftware-rs notes "there is an instance of [TalkScript] created per interactible NPC when a map is loaded" (FACT, `cs/talk_script.rs`). The runtime link from `ChrIns` to its talk instance is unmapped (the `talk` module is opaque). So build an offline **`(map, entity_id) → talkID` table from the MSBs**, or capture it from talk-event hooks (§10).
2. **NpcName resolves.** Vanilla: 7,039 NpcParam rows, 715 with `nameId != 0`, 695 resolve (MapForGoblins `docs/re/windows_enemy_name_runtime_source_re_findings.md` §1; single source, regulation version not stated, LIKELY 1.16.x; not re-counted here). Generic soldiers, trolls and animals are nameless in vanilla, so any name is a strong "special" hint. Invaders and bosses also have names, so combine it with team type and talk presence. Boss *bar* names do not come from `NpcParam.nameId` but from the EMEVD `2003[11]` argument (same doc).
3. **Entity-ID suffix `07xx` (common, not universal).** *(corrected)* Most quest NPCs in MapForGoblins' death-flag tables have entity IDs ending in 07xx: Seluvis 1034500701, Varré 1042360700, Patches 31000701, Boc 11050730, Sellen 14000713, Nepheli 10000730, Kenneth 1045380700, Diallos 1039440710, Kalé 11000705, Millicent 15000700, Irina 1045340700. There are exceptions: Count Ymir `20010455` (DLC), Gurranq `1051430800` (boss-style `90005860` template, flag == entity), and Dung Eater `35000850`. The tables were mined from the **ERR (Elden Ring Reforged) EMEVD**, not vanilla, and the doc corrects itself once: entity `1034500710` is Ranni, not Iji (`docs/emevd_death_flags_results.md`). Use the suffix as a candidate generator only. Boss defeat flags use `08xx`/`085x` (10000800, 10000850).
4. **NpcParam ID shape.** Paramdex names suggest humanoid (c0000) story NPCs use 9-digit IDs `5` + CharaInitParam ID + 4 digits: `523090000` Patches, `523010000` White Mask Varré, `533110000` Castellan Edgar. Creature IDs embed the model: `52200000` Promised Consort Radahn (c5220), `21200000` Malenia (c2120). "Talk Dummy" rows (`10000100`, …) are invisible talkers (LIKELY).
5. **`NpcParam.npcType`** (enum `NPC_TYPE`; the "0 standard / 1 boss" values were not found in any local paramdef, Unverified) and team types 26/27 (Friendly/Hostile NPC) vs 6/7 (Enemy/Strong Enemy) (team enum FACT, DarkScript3 EMEDF `Team Type`; usage LIKELY). Note that Roundtable NPCs use team **0** (`Disabled`), so "team 0" does not mean "unimportant" (er-mods-rs).

**Recommendation (SPECULATION):** ship a curated `named_npcs.json` keyed by NpcParam ID ranges and entity IDs (Paramdex names + MSB talk IDs + NpcName FMG). Each entry carries persona, voice and quest-flag set. Anything not in it falls back to "generic", described by model name + team + location. In-game heuristics only add candidates for review and never promote a character on their own.

---

## 7. Progression and event flags

**Reading (FACT, `cs/event_flag.rs`):** `CSEventFlagMan::instance()?.virtual_memory_flag.get_flag(id) -> bool`. Group = `id / 1000`, then a `DLMap` lookup. Unallocated groups read false. SoulSplitter reads flags from outside the process by walking the same tree (`ReadEventFlag`, `src/SoulMemory/EldenRing/EldenRing.cs`), which cross-checks the algorithm. **IDs are data, not offsets, so a curated flag list survives patches** (LIKELY). Watching changes needs a hook on the setter (TGA flag logger; see 01 §9) or polling a curated list about once a second.

**Global flags (FACT, eventparam `EFID_common`, pre-DLC):**

| Range / ID | Meaning |
|---|---|
| 20 / 21 / 22 | Game cleared: normal / Ranni / Frenzied Flame ending |
| 50–58 | Cycle N cleared (NG+ count) |
| 100, 101, 102 | Game started, tutorial map reached, open field reached |
| 104 / 105 | "Hub" (拠点) reachable / reached, LIKELY Roundtable Hold |
| 108 | Received the Frenzied Flame |
| 118, 119 | Erdtree burned, reached the Erdtree hollow |
| 130–140 | Legacy dungeon reached (Stormveil, Leyndell, Volcano Manor, …) |
| 180–187, 191–197 | Great Rune count (180 = zero, 181–187 = ≥ 1 … ≥ 7) / possession (above) |
| 200 / 201 | Host / guest |
| 530–532, 550–575 | Time and weather |
| 9000 / 9001 / 9020 | Resting at grace / suppress grace menu / BGM control while resting |
| 9100–9295 | Boss kill flags. The block "GrF：ボス撃破フラグ" is 9100 (+100), but per-boss entries continue at 9170–9184 (Divine Towers, overworld forts) and 9200–9295 (catacombs, caves, …). DLC bosses use 9140+ (e.g. 9143, 9146, 9163, from TGA). `611xx`/`612xx` mirror them ("defeat the boss", shop/loot side) *(corrected)* |
| 71000–75999, 76000–76999 | Grace lit, legacy / open world ("篝火開放フラグ") |
| 62000–62599 | Map fragment pickups and map-point unlocks |

**Boss flags (FACT, TGA `Event Flag Manager/.cea` and SoulSplitter `Boss.cs`):**

| Boss | Defeated (local) | Global kill flag |
|---|---|---|
| Margit | 10000850 (encountered 10000851) | 9100 |
| Godrick | 10000800 (encountered 10000801) | 9101 |
| Rennala | 14000800 | 9118 |
| Starscourge Radahn | 1252380800 | 9130 |
| Morgott | 11000800 | 9104 |
| Rykard | 16000800 | 9122 |
| Malenia | 15000800 | 9120 |
| Mohg, the Omen / Lord of Blood | 35000800 / 12050800 | 9125 / **9112** *(corrected; eventparam "ボス撃破 グレーターデーモン", TGA)* |
| Tree Sentinel (Limgrave) | 1042360800 | none (field boss; TGA lists only the defeat + loot flags) |
| Messmer the Impaler (DLC) | 21010800 (encountered 21010801) | **9146** *(TGA)* |
| Bayle the Dread (DLC) | 2054390800 (encountered 2054390801) | **9163** *(TGA)* |
| Promised Consort Radahn (DLC) | 20010800 (encountered 20010801) | **9143** *(TGA)* |

All rows were re-checked against TGA `Event Flags/Event Flag Manager/.cea` (commit `393c439`) and SoulSplitter `Boss.cs`. The base-game global flags also match the eventparam names (9101 "接ぎ木の王", 9104 "忌み鬼マルギット（本気）" = Morgott, 9118 Rennala, 9120 Malenia, 9122 "グールの王" = Rykard, 9130 Radahn). SoulSplitter labels 21010800 "Base Serpent Messmer"; TGA calls the same flag "Messmer the Impaler". DLC overworld tiles use a `20XXYY####` prefix (Bayle `2054390800` for `m61_54_39`), versus `10XXYY####` in the base game (LIKELY from these examples). Large-tile placements use other prefixes (Starscourge Radahn `1252380800`). The eventparam dump predates the DLC (2024-04), so DLC flags must come from SoulSplitter/TGA or live logging.

**Graces (FACT):** e.g. 71000 Godrick the Grafted, 71001 Margit, 71400 Raya Lucaria Grand Library, 76111 Gatefront, 76422 Starscourge Radahn (TGA, SoulSplitter `Grace.cs`, 418 entries). `BonfireWarpParam.eventflagId` links each grace row to its flag, so the list can be generated at runtime.

**NPC questlines (FACT for the IDs, with a caveat):**
- Quest state lives in 3000–4999, in blocks of about 20 per NPC: Sellen 346x (3468 "returned to the academy", 3469 "became a ball of magic"), Gostoc 3269 "after Godrick defeated", Tanith 3109–3111.
- Per-map conversation bands sit at `…0700/…2700` (01 §9).
- MapForGoblins mined the EMEVD common template `90005702(entity, death_flag, …)` for "NPC gone" flags: Patches 3683, Boc 3943, Kenneth 3583, Thops 3803, Sellen 3463, Nepheli 4223, Diallos 3443, Yura 3623, Seluvis 1034509302. **The same ID is set when the NPC dies or when the quest concludes peacefully** (`docs/emevd_death_flags_results.md`). So "gone" does not mean "killed". *(caveat added)* The mining was done on **ERR's** decompiled EMEVD, and the doc itself says "the user runtime-tests before trusting". Its own later table corrects one row (Iji's flag is `1034499202`; `1034509403` belongs to Ranni). Treat these IDs as LIKELY for vanilla until each one is checked against vanilla EMEVD or a live flag log.

**AI value:** very high for named NPCs. Flags turn into world-state facts ("Godrick has fallen", "you bear two Great Runes", "Radahn's star fell"). Each named NPC should get its own flag-derived quest-stage summary.

---

## 8. Combat events

### 8.1 "In combat" and aggro: what MapForGoblins learned the hard way

**FACT per one source (MapForGoblins `docs/re/combat_state_gate_re_findings.md` and `combat_enemy_list_structure_re_prompt.md`, ER 2.6.2.0, static Ghidra RE plus live disassembly):**
- `IsBattleState` is registered as an AI function (string xref at `FUN_140089710`). It reads `[[arg+0xC950]+0x30C] == 6`, and sibling getters test 5 (alert) and 1/3/4 (search variants).
- The arg is a **`CS::CSAiThink`** (size 0xF0D0, pooled), not a `ChrIns`. The EnemyIns→CSAiThink link is "manager-mediated (no fixed offset)". *(caveat added)* The MapForGoblins docs contradict each other here: the body of the findings still writes `IsBattleState(chr)` with `rcx = a ChrIns`, and only the later "RESOLVED" banner reclassifies the arg as `CSAiThink`. The banner is the authors' final word, but it is a Ghidra reading that was never live-tested. RTTI names `CS::CSAiThink`, `CS::CSAiThinkOwner` and nested `CSAiThink` classes do exist (er-mods-rs `docs/recon/deobf-rtti-classmap-2720.tsv`).
- *(new lead)* TGA reaches a live NpcThinkParam row for the lock-on target through `[[[[ChrIns+0x58]+0x18]+0xC0]+0x30]` (`Scripts/Helpers/Targeted Npc Info/NpcThinkParam/.xml`, 1.17.2). In fromsoftware-rs, `+0x58` is `ChrIns.chr_ctrl` and `+0x18` is `ChrCtrl.manipulator`. So a **fixed** pointer path exists from a ChrIns to an AI object that knows its think param. If that object is the `CSAiThink`, battle state would be `[[obj+0xC950]+0x30C]` (2.6.2.0 offsets; must be re-found on 1.17.2). This is the cheapest thing to test live before accepting "no fixed offset" (SPECULATION).
- The engine's counting aggregate (`FUN_1405d8790`) is entity-ID-keyed and answers "is entity X in battle", not "am I in combat".
- No clean global bool exists. `EcTestFieldBattleBgmSoundPlaying` (EMEVD `3[33] IF Field Battle BGM Active(threatLv, …)`) reads sound state "filtered by a BGM-id param" and lags behind.
- HUD enemy tags only cover lock-on or recent damage. Their `ChrIns` also lacked the AI pointer.
- MapForGoblins replaced detection with a world freeze. The internal FSM numbering differs from the EMEDF `AI State Type` enum (0 Normal, 1 Recognition, 2 Alert, 3 Combat, 4 Passive Alert, 5 Active Alert, 6 Wait Before Forget; DarkScript3 `er-common.emedf.json`), which is another trap.

**Other leads (LIKELY):**
- Erd-Tools patches a `CombatCloseMapAoB` = `E8 ?? ?? ?? ?? 84 C0 75 ?? 38 83 ?? ?? ?? ?? 75 ?? 83 E6 FE`. It is a call to a bool predicate the game uses to close the map in combat. Resolving that call target could give the game's own "player in combat" test, but it is untested on 1.17.2.
- The `CSTargetingSystemOwner` vtable has `is_battle_state()` and `get_target_ene0_position()` (`cs/targeting.rs`). Reaching it needs the unmapped AI module (`ChrInsModuleContainer.ai: usize`, opaque) or the CSAiThink (see the TGA chain above).

**v1 heuristic (SPECULATION):** `in_combat = boss_fight_active || (player HP dropped in the last 8 s && last_hit_by resolves to a hostile chr) || (the player damaged a hostile in the last 8 s: its last_hit_by == player handle and its HP dropped) || any CSFeMan.enemy_chr_tag_displays[i] visible`, with a decay timer. "Aggro" for a generic enemy is the first of: it damages the player, it is damaged by the player, or the player locks on to it while it is a hostile team within its NpcThinkParam `eye_dist`. That is good enough for barks.

### 8.2 Damage ("X hit Y with Z for N")

**Engine path (FACT, er-mods-rs `crates/er-npc-possess/src/possess/netdamage.rs`):** "a chr-vs-chr hit that survives the team/immunity filter reaches the victim's `CSChrDamageModule` vtable slot 7". The generic routine is 1.16.2 `0x140445060` / 1.17 candidate `0x1404455c0`. The player's `CSPlayerDamageModule` overrides it (1.17 `0x14044d050`). The routine classifies attacker and victim into a 7×7 mode table, looks up `AtkParam`, fills an `AttackDamageInfo`, and calls `HitChr` → `ApplyDamage`. Bullets end in the same slot-7 call with the bullet's owner as attacker. **`AttackDamageInfo` is not mapped publicly** (er-mods-rs documents single fields only, e.g. `+0x11c` selects the 7×7 mode table), and the crate's `damage` module pointer is opaque (`ChrInsModuleContainer.damage: usize`).

**Hook-free reconstruction (FACT fields; SPECULATION method):** each tick, for every tracked chr, `ΔHP = prev_hp − hp`. If `ΔHP > 0`, the attacker is `last_hit_by` and the attack row is the victim's `received_damage_type`. The weapon is "the attacker's equipped right weapon" for players (or `CSChrTimeActModule` animation for more detail). This misses multi-hits inside one tick and simultaneous attackers, which is acceptable for barks. `CSFeManImp.enemy_chr_tag_displays[8]` already carries the game's own `damage_taken` and `pre_damage_hp` per tagged enemy (FACT, `fe_man.rs`). That is exactly the "player hit X for N" number the HUD shows.

**Optional hooks for later:** TGA "Last Hit Npc Info" AOB `48 8b 03 48 8b cb ?? ?? ?? ?? ff 50 10 83 f8 01` (captures the last chr hit, supported on 1.17.2 per the TGA README). Or the slot-7 vtable entry, after mapping `AttackDamageInfo`.

### 8.3 Kills and deaths

- **NPC died:** `death_flag` rising edge. Killer = its `last_hit_by` (FACT fields).
- **Player died:** §3. Also Lua `HostDead` 4000, `OnDeadEvent` 4090, `DeathRestartEvent` 4006 (FACT IDs, `cs/lua_event_man.rs`).
- **Boss killed:** `CSLuaEventScriptImitation.clear_boss_id` / `clear_boss` (FACT fields), the defeated flag rising edge, and the "felled" banners.

### 8.4 Boss fight started

`GameDataMan.boss_fight_active`, `boss_fight_timer`, `boss_health_bar_entity_id`, `boss_health_bar_npc_param_id` (FACT for the fields at `5702315`). *(downgraded)* Only `boss_fight_active` ("Whether a boss fight is currently active") and `boss_fight_timer` have doc comments. That the two `boss_health_bar_*` fields hold the entity/NpcParam of the *displayed* bar is LIKELY from the names, not live-verified. `GameDataMan` holds one entity/param pair while the HUD has three bar slots, so for fights that show more than one bar read `CSFeManImp.boss_health_displays[3]`, which adds the bar's NpcName `fmg_id`, `field_ins_handle` and `damage_taken`. `GameDataMan` is RVA-resolved, so it triggers the version panic on unknown builds. Lua `ReportBossArea` 4110 and `ReportClearedBossArea` 4112 exist. "Encountered" flags (`AABB0801`) persist the first sighting. **Robustness: high, once checked live.** These are the HUD's own inputs.

### 8.5 Parry, backstab, riposte

FACT per one source (er-mods-rs `docs/er-mechanics/crits.md`, read from the 1.17.x regulation with their param reader; Paramdex has no ER ThrowParam names file to cross-check): "A backstab or riposte is a throw". ThrowParam **10000000** = backstab (attacker anim `a000_070000`, ±135°, 1.8 m), **30000000** = riposte (`a000_070010`) (rows with `AtkChrId == DefChrId == 0`). Large and plunging throws use other rows. The parried state is a defender SpEffect with `throwCondition` 10 (INFERRED there). fromsoftware-rs exposes `CSChrThrowModule.throw_node.throw_state` (`InThrowAttacker` 3, `InThrowTarget` 4, `DeathAttacker` 5, `DeathTarget` 6). The throw param ID and target are not named. **v1:** treat "player `InThrowAttacker` while a nearby hostile is `InThrowTarget`" as a critical hit. Parry detection waits for the SpEffect check (LIKELY workable).

### 8.6 Event streams worth hooking later

| Stream | Hook point | Crate support |
|---|---|---|
| Lua-imitation events (death, boss area, mount, grace level-up, ladder) | `CSLuaEventMsgExec::execute(proxy, CSScriptCallParam{event_id, actor_character_event_id, …})` | vtable RVAs `cslua_event_msg_exec_func_vmt` / `_string_vmt`; structs modeled |
| Talk commands (§10) | `CSEzStateTalkEvent::invoke(&EzStateEvent{id, args[60]})` | vtable RVA `cs_ez_state_talk_event_vmt` |
| Banners | `CSMenuManImp::display_status_message(id)` or poll `full_screen_message_request_id` | RVA present |
| Flag changes | flag-set routine (TGA AOB) | no |

---

## 9. Menu state

- **HUD mode (FACT, `cs/fe_man.rs`):** `CSFeManImp.hud_state`: `HideAll` 0, `ShowAll` 1 ("used when esc menu is open"), `PopupMenu` 2 ("special state used when popup menus are open"), `Default` 3. This is the cheapest "a menu is up" signal.
- **UI elements (FACT, `cs/menu_man.rs`):** `CSMenuManImp.ui_states[0x46]` with `created`/`visible` bits per element. Index meanings are not documented (open question). `CSPopupMenu.current_talk_id` holds the talk being shown. `CSChrMenuFlags.pause_menu_state` controls whether the ESC menu may open.
- **Which NPC, which menu (FACT, `cs/talk_script.rs`):** `CSNpcTalkIns.base.{talk_id, talk_param_id, field_ins_handle, event_flag_id, talk_interrupt_reason}` and `menu_state.current_open_menu: MenuType` (1 TalkList, 5 RegularShop, 6 SellShop, 9 EnhanceShop, 10 Soul (LIKELY level-up), 12 ConversationChoices, 15 GenericDialog, 26 TailoringShop, 27 AshOfWarShop, …). The pointer to the active NPC's `CSNpcTalkIns` comes for free in a `CSEzStateTalkEvent::invoke` hook (`event.npc_talk_ins`). Without a hook, the world talk manager (`CSWorldTalkMan`, libER symbol) is unmapped.
- **Loading / not in world:** `WorldChrMan.main_player` is `None`. SoulSplitter reads a `ScreenState` from MenuMan (InGame 0, Loading 1, MainMenu 256). `CSNowLoadingHelper.menu_load_counter` increments per loading screen (FACT). Pause perception in all of these.

**AI value:** high for gating. Don't bark during menus or cutscenes. Do treat "player opened Kalé's shop" as a conversation-start cue.

---

## 10. Talk state and "is talking"

Vanilla talk is per-NPC ESD (03 §TL;DR). For awareness we need three things: (a) "the player is in a conversation with NPC X", (b) "NPC X just said TalkParam line Y", (c) "talk was interrupted by an attack". A hook on `CSEzStateTalkEvent::invoke` covers (a) and (b). Each call carries the command `id` (`TalkToPlayer` = 1, `OpenRegularShop` = 22, `ForceEndTalk` = 8, `AddTalkListData` = 19; ESDLang docs) and the `CSNpcTalkIns` with the NPC handle. For (c), use `talk_interrupt_reason` plus the ESD environment queries `HasPlayerBeenAttacked` / `GetTalkInterruptReason` (FACT names, ESDLang). That every NPC's talk command goes through this one vtable is LIKELY, since fromsoftware-rs builds its synthetic `TalkScript` with the game's own vtable. **Without a hook** in v1, use an "in conversation" proxy: `hud_state == PopupMenu` and the nearest named NPC within ~3 m facing the player (SPECULATION).

---

## 11. Player chat input: the game does not pause

**Problem:** ER has no pause. The ESC menu and talk menus do not stop enemies. Typing a sentence near hostiles gets the player killed, and keystrokes would also move the character.

**Options (ranked):**
1. **Engine freeze (FACT, MapForGoblins `game_timestep_freeze_re_findings.md`, "SOLVED … live").** `CS::CSEventUtility::SetDisableAllChrUpdate(bool)` (2.6.2.0 `er+0x5f4d40`, entry AOB `40 53 48 83 EC 40 48 C7 44 24 20 FE FF FF FF 48 83 3D ?? ?? ?? ?? 00 0F`) "freezes ALL characters (player + enemies + NPCs) in pose, still rendered; camera/UI/input keep running. Resume is INSTANT". It is "ER's own cutscene freeze". The function must be called; poking its flag in `WorldChrManDbg` does nothing. Single-player only. World time and weather probably keep running (LIKELY). *(caveats added)* (a) The live test was an external `CreateRemoteThread(er+0x5f4d40, 0/1)`, i.e. a call from a foreign thread on 2.6.2.0. An in-process mod should call it from a game-thread `CSTaskImp` task, and that call path has not been tested. (b) The global flag is `[WorldChrManDbg+0x8]` (fromsoftware-rs leaves it as `WorldChrManDbg.unk8`). Each character's effective state is that flag XOR a per-chr bit (`[ChrIns+0x531]` on 2.6.2.0). That bit matches fromsoftware-rs `ChrIns.debug_flags.disabled_updates` (bit 8, "Disables all updates to the character (physics, ai, etc)"). Setting that bit on nearby hostiles only is a crate-backed alternative that leaves the player free (SPECULATION, untested). (c) The "enemies frozen, player free" variant is marked "UNTESTED theory" by MapForGoblins itself.
2. **Branch-flip pause** (`iArtorias/elden_pause`, 2022): patches a `je`/`jne` at `0F 84 ? ? ? ? C6 83 ? ? 00 00 00 48 8D ? ? ? ? ? 48 89 ? ? 89`. It is old, unverified on 1.17.2, and MapForGoblins reports a resume hitch proportional to the pause length.
3. **Context gating:** allow free typing only when safe. Safe means resting (flag 9000), in Roundtable Hold (SpEffect 9621; this does not cover other areas in single player), with no hostile within R, or in a talk popup with no hostile within R. Otherwise use push-to-talk STT or quick-reply choices.
4. **Input capture (FACT, er-mods-rs `crates/er-overlay-input/src/lib.rs`):** hook the "menu has the mouse" predicate (2.7.2.0 `0x140766b00`), blank DirectInput keyboard and mouse reads, and centre XInput. Release keys held at close only after they come up.

**Recommendation (SPECULATION):** v1 opens the chat box only when the context is safe *or* when the user accepts an auto-freeze (option 1). It always captures input (option 4) and refuses to freeze when multiplayer state is not `Host` + solo.

---

## 12. Coverage matrix: fromsoftware-rs vs custom RE

`5702315` is git `main`, not a crates.io release: the newest published `eldenring` crate (0.14.0) targets 1.16.2 (02). Pin the git revision.

| Item | Exposed by `eldenring` crate (`5702315`) | Needs custom RE/AOB |
|---|---|---|
| Player name, level, runes, stats, HP/FP/SP, class, keepsake, blessings | yes (`PlayerGameData`) | — |
| Equipped item IDs, Great Rune slot, spells | yes | — |
| **Item / NPC / place names (FMG text)** | no (`MsgRepositoryImp` is an empty unit struct with only the `#[shared::singleton("MsgRepository")]` locator, `cs/msg_repository.rs`) | `GetMessage` AOB (MapForGoblins `re_signatures.hpp` `GETMESSAGE`, interior AOB, entry = match − 5, 2.6.x). Alternative without RE: extract the FMGs offline from `msg/engus/*.msgbnd.dcx` (SoulsFormats/WitchyBND) and ship an ID → text table. That breaks on overhaul mods that change text |
| Death count, playtime, NG+, just_died | yes (`GameDataMan`) | — |
| Boss fight active + bar | yes (`GameDataMan`, `CSFeManImp`) | — |
| On Torrent | yes (`CSChrRideModule`) | — |
| Multiplayer role / session | yes | — |
| Location: block, position, play region, place-name ID | yes (`PlayerIns`, `FieldArea`) | — |
| Last grace | no (`GameMan.unkb60`) | TGA offset |
| Time of day | yes (`WorldAreaTime`) | — |
| Weather (direct) | no | `WorldAreaWeather` layout, or use flags |
| Nearby chars, distance-sorted list, handles | yes (`WorldChrMan`) | — |
| Per-chr HP, team, NpcParam, model, entity ID, death, last_hit_by, SpEffects, throw state | yes | — |
| Team relation matrix | partial (classes only) | matrix address (er-mods-rs) |
| AI battle state / aggro target | no (`ai` module opaque) | CSAiThink link (unsolved publicly; TGA's `chr_ctrl → manipulator → +0xC0` chain is the lead) |
| Damage events with amounts | no (`damage` module opaque) | slot-7 hook + `AttackDamageInfo` |
| Raycast LOS | yes (`CSPhysWorld::cast_ray`) | filter semantics |
| Event flags read/write | yes | flag-set hook for a change stream |
| Talk instances, MenuType | partial (structs + vtable RVA) | the hook, or the `CSWorldTalkMan` layout |
| Lua events | yes (structs + vtable RVAs) | the hook |
| HUD state, banners, enemy damage tags | yes (`CSFeManImp`) | — |
| World freeze | partial: per-chr `ChrDebugFlags.disabled_updates` only | `SetDisableAllChrUpdate` AOB for the global freeze |
| Params (NpcParam, BonfireWarpParam, …) | yes (`SoloParamRepository::get`, generated param structs) | — |

---

## Implications for the AI framework

**v1 (minimal, stable):**
1. **One perception task, read-only.** Register a `CSTaskImp` task (e.g. `ChrIns_PostPhysics` or `FrameBegin`) that runs every ~15 frames. It copies a `WorldSnapshot`: player block, the §2 fields, time, flags of interest, HUD state, boss bar, and up to N characters from `chr_inses_by_distance` within ~40 m with the §6.2 fields. Swap it into a lock-free buffer. Write no game memory in v1, except optionally the freeze call.
2. **Events by diffing snapshots, no hooks.** Emit `PlayerDamaged`, `PlayerHitEnemy`, `ChrDied(killer)`, `PlayerDied`, `BossFightStarted/Ended`, `EnteredArea(place)`, `GraceLit`, `FlagChanged(curated)`, `MountChanged`, `CritLanded`, `ConversationProxyStart`. Debounce, and attach "salience" scores for the bark scheduler.
3. **Two NPC tiers, decided by data.** A curated `named_npcs.json` (NpcParam IDs, entity IDs, talk IDs, persona, quest flags) defines "important". Everything else is generic, described as `{model name, NpcName if any, team, HP state, location}`, and may only bark on combat events.
4. **World-fact compiler.** Map the curated flags (bosses, Great Runes, endings, key quest states) to short natural-language facts, regenerated whenever a flag flips. Named NPCs get global facts plus their own quest-stage facts. Generics get only local facts.
5. **Location and time strings** from BlockId → PlaceName (WorldMapPlaceNameParam / BonfireWarpParam tables), nearest grace, interior/overworld, and the time bucket.
6. **Text resolution** through one `GetMessage` AOB with the DLC slot fallback order (`{4xx, 3xx, base}`). Cache per ID.
7. **Hard gates.** Disable everything unless `multiplay_role == Host && is_my_world && LobbyState == None`. Suspend perception when `main_player` is `None` or during loading screens. Run offline only, with EAC off (me3 `start_online = false`; see 02 §2). Seamless Co-op sessions use their own lobby and must also be detected and refused.
8. **Version gate.** Read the exe's PE `FileVersion` yourself **before** any RVA-backed call. Start only on exes that fromsoftware-rs recognises, because `rva::get()` panics otherwise. Self-check on load (name non-empty, `hp <= max_hp`, flag 100 set on a loaded save) and fall back to "inactive + log" on failure.
9. **Flag catalog provenance.** Tag every curated flag with its source (eventparam dump, TGA, SoulSplitter, ERR-mined, live log) and a "live-verified" bit. The fact-check found off-by-meaning errors (flag 180, Mohg 9112, Iji/Ranni) that only cross-checks caught.

**Later:**
- Hook `CSEzStateTalkEvent::invoke` for exact conversation and shop awareness, and to know which vanilla line an NPC just said. Every hook (vtable or detour) must survive Arxan code restoration. Use me3 `disable_arxan` or fromsoftware-rs `shared::arxan`, and prefer vtable swaps and the task runtime over patching `.text` (01 §2).
- Test the TGA chain `[[[chr+0x58]+0x18]+0xC0]` as the CSAiThink pointer. If it holds, "enemy noticed you" barks become a cheap poll.
- Hook Lua event dispatch and the flag setter for exact event streams.
- Map `AttackDamageInfo` (attack row, damage per type, crit flag) for "you struck me with fire!".
- Resolve the CSAiThink link for true aggro and target awareness, and enemy "noticed you" barks.
- Use raycast LOS for "seen vs heard".
- Add a live-tool flag logger to extend the DLC flag catalog.

---

## Risks

- **Patch churn:** two layout shifts in 1.17.x. fromsoftware-rs pins a single exe and has lagged patches by same-day to ~2 weeks (01 §1). The custom AOBs (GetMessage, freeze, damage) need their own validation per patch.
- **Wrong-object reads:** MapForGoblins' combat RE read "garbage" through a plausible offset for weeks. Every unverified offset needs a live sanity check (RTTI vtable compare) before use.
- **Pointer lifetime:** characters stream in and out, and `WorldChrMan` "moves on world transitions" (MapForGoblins). Re-resolve every tick and keep handles, not pointers.
- **Flag semantics:** shared "dead or concluded" flags, pre-DLC dumps, and EMEVD-maintained flags that may lag. Validate each curated flag live.
- **Overhaul mods** (ERR, Convergence) change NpcParam rows, FMG text and flags. Data tables must be per-mod or derived at runtime from params.
- **Freeze side-effects:** `SetDisableAllChrUpdate` is a cutscene tool. Calling it during EMEVD-driven sequences or boss transitions may conflict with the game's own use (SPECULATION). It was only ever tested from a foreign thread on 2.6.2.0.
- **Single-source RE:** the combat-state, freeze, NpcParam-count and death-flag findings all come from one project (MapForGoblins, 2.6.2.0, partly mined from ERR's data). Its docs revise themselves often ("SUPERSEDED", "CORRECTION", "RESOLVED"). Treat each finding as a lead to re-check on 1.17.2, not a spec.
- **Arxan and EAC:** in-process code is fine offline, but `.text` patches can be reverted by Arxan code restoration, and anything injected means EAC must be off. Never let the framework run on official servers (02 §2).
- **Licensing:** fromsoftware-rs is "MIT/Apache NON-AI". It forbids use "for the purpose of training or improving machine learning algorithms" (`LICENSE-MIT-NON-AI`). Runtime use in an LLM mod is probably not training, but logs of game state should not feed fine-tuning datasets. Get a legal read (see 02).

## Open questions

1. What fraction of nearby generic enemies have `event_entity_id == 0`? How should non-placed or respawning generics be identified across sessions?
2. Is there a stable path `EnemyIns → CSAiThink` (MapForGoblins hints at `[[EnemyIns+0xC0]+0xC950]`, unconfirmed)? Is the AI module (`ChrInsModuleContainer.ai`) that link?
3. What is the call target in Erd-Tools' `CombatCloseMapAoB` on 1.17.2? Is it a usable "player in combat" predicate?
4. Are flags 530–532 and 550–575 updated continuously by the engine or EMEVD, or only at specific events?
5. Do vanilla banners pass through `display_status_message`, or only through `full_screen_message_request_id`?
6. What do the `CSMenuManImp.ui_states[]` indices mean (talk, shop, level-up, map)?
7. Is "Great Rune restored" (Divine Tower) a separate flag per rune? Its IDs are not in the pre-DLC dump.
8. What is `cast_ray`'s filter value for character-to-character LOS?
9. Is `chr_inses_by_distance` complete (open-field + block + summon sets) at all distances?
10. What is the layout of `AttackDamageInfo` on 1.17.2 (attacker, AtkParam ID, per-element damage, crit/backstab flag)?
11. Is `[[[ChrIns+0x58]+0x18]+0xC0]` (TGA's NpcThinkParam path) the `CSAiThink`? Does `[[it+0xC950]+0x30C]` read 6 while that enemy fights the player on 1.17.2?
12. Are NPC invaders and phantoms (e.g. Bloody Finger Nerijus) `PlayerIns` or `EnemyIns`? If `PlayerIns`, they carry a real `PlayerGameData` name at `+0x9C`.
13. Do `GameDataMan.boss_health_bar_entity_id` / `_npc_param_id` follow the first bar, the last bar or the primary boss in multi-bar fights?
14. Which vanilla (not ERR) EMEVD entries back the `90005702` death flags? A vanilla re-mine with DarkScript3 would settle it.
15. What are the `NPC_TYPE` enum values for `NpcParam.npcType` (Smithbox/Paramdex meta)?

## Sources

- fromsoftware-rs (crate `eldenring`): https://github.com/vswarte/fromsoftware-rs (`crates/eldenring/src/cs/{player_game_data,game_data_man,game_man,field_area,world_chr_man,chr_ins,chr_ins/module/*,fe_man,menu_man,menu_type,talk_script,ez_state_talk,lua_event_man,event_flag,targeting,team_relation,sp_effect,havok_man,world_area_time,session_manager,multiplay_type,now_loading}.rs`, `rva.rs`, `tools/param-generator/params/eldenring/{BonfireWarpParam,WorldMapPlaceNameParam,NpcParam,TalkParam}.xml`, `LICENSE-MIT-NON-AI`)
- er-mods-rs: https://github.com/Banon-Labs/er-mods-rs (`crates/er-npc-possess/src/possess/netdamage.rs`, `crates/er-overlay-input/src/lib.rs`, `crates/er-profile-summary-core/src/map_place_names.rs`, `crates/er-target-bars/src/lib.rs`, `docs/er-mechanics/{crits,map-combat-restrictions}.md`)
- MapForGoblins: https://github.com/quentin452/ERR-MapForGoblins-DLL (`src/goblin_enemy_names.cpp`, `src/goblin_messages.cpp`, `src/re_signatures.hpp`, `docs/re/combat_state_gate_re_findings.md`, `docs/re/combat_enemy_list_structure_re_prompt.md`, `docs/re/game_timestep_freeze_re_findings.md`, `docs/re/windows_enemy_name_runtime_source_re_findings.md`, `docs/emevd_death_flags_results.md`, `docs/elden_ring_notification_systems.md`)
- TGA Cheat Table: https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA (`Event Flags/Event Flag Manager/.cea`, `Statistics/Character/{Great Rune,Last Grace}.xml`, `Scripts/Helpers/{Targeted Npc Info,Last Hit Npc Info}/.cea`, `MiscWIP/Dropdowns/*.xml`, `CHANGELOG.md`)
- Event flag dump: https://github.com/soulsmods/elden-ring-eventparam (`index.md`)
- SoulSplitter: https://github.com/FrankvdStam/SoulSplitter (`src/SoulMemory/EldenRing/{EldenRing,Boss,Grace,ScreenState}.cs`)
- DarkScript3: https://github.com/AinTunez/DarkScript3 (`DarkScript3/Resources/er-common.{emedf.json,ModelName.txt,MapName.txt,CharaName.txt}`)
- Paramdex names: https://raw.githubusercontent.com/soulsmods/Paramdex/master/ER/Names/NpcParam.txt (also `CharaInitParam.txt`)
- ESDLang: https://github.com/thefifthmatt/ESDLang (`dist/ESDScriptingDocumentation_Talk.json`)
- libER: https://github.com/Dasaav-dsv/libER (`symbols/singletons.csv`)
- Erd-Tools: https://github.com/Nordgaren/Erd-Tools (`src/Erd-Tools/Hook/Offsets.cs`, `CombatCloseMapAoB`)
- elden_pause: https://github.com/iArtorias/elden_pause (`elden_pause/main.cpp`)
- Patch level: https://www.bandainamcoent.com/news/elden-ring-patch-notes-version-1-17 ; https://gaming-st.com/news/eldenring-ver1172-update/ ; https://twistedvoxel.com/elden-ring-patch-1-17-2/
- Pause mods (context): https://www.pcgamer.com/you-can-finally-pause-elden-ring-with-this-simple-mod ; https://www.nexusmods.com/eldenring/mods/231
- Death-counter prior art: https://github.com/Quidrex/DSDeaths ; https://nexusmods.com/eldenring/mods/2989
- Not reachable this session: https://soulsmodding.com (event flag and param references)
- Added by the fact-check:
  - er-mods-rs `crates/er-quickload/src/constants/stats_panel_text.rs` (PGD name `+0x9c` vs display name `+0x8e8`), `docs/game-build.md` (2.7.2.0 = Steam 1.17.2, de-Arxan'd image), `docs/recon/deobf-rtti-classmap-2720.tsv` (CSAiThink RTTI): https://github.com/Banon-Labs/er-mods-rs
  - lraty-li/Elden-Ring-Mods (decompiled grace ESD `Gacha_Ring/t000001000.py`, `Gacha_Ring/emved/common.emevd.dcx.js`, 2023-03, commit `a4f7c5d`): https://github.com/lraty-li/Elden-Ring-Mods
  - TGA `Scripts/Helpers/Targeted Npc Info/{Name,Hp,Character Type,Team Type}.xml` and `.../NpcThinkParam/.xml` (pointer chains), `Event Flags/Event Flag Manager/.cea` (Mohg 9112, Messmer 9146, Bayle 9163, Consort Radahn 9143), `MiscWIP/Dropdowns/ArcheTypeIdDLC3.xml`
  - fromsoftware-rs `cs/chr_ins.rs` (`PlayerIns`/`EnemyIns` layouts, `ChrDebugFlags.disabled_updates`, `ChrCtrl.manipulator`), `cs/chr_ins/module.rs` (opaque `ai`/`talk`/`damage`), `cs/world_chr_man_dbg.rs`, `cs/msg_repository.rs`, `crates/shared/src/arxan.rs`
  - DarkScript3 `er-common.emedf.json` (2007[13] "Display Subarea Welcome Message", `AI State Type`, `Team Type` enums)
  - MapForGoblins `docs/re/combat_enemy_list_structure_re_prompt.md` (RESOLVED banner), `docs/re/windows_enemy_name_runtime_source_re_findings.md` §1

## Verification log

Adversarial fact-check, 2026-11-06. Method: re-read each cited file in the shallow clones, then cross-check against an independent repo where one exists. Web search was exhausted and bandainamcoent.com / twistedvoxel.com could not be reached, so no claim below relies on fresh web pages. Verdicts: **Confirmed** (evidence agrees), **Corrected** (statement changed in the text), **Unverified** (single source or no independent check possible), **Refuted** (the claim is wrong).

| # | Claim | Verdict | Evidence |
|---|---|---|---|
| 1 | fromsoftware-rs `5702315` recognises only exe 2.7.2.0 (EN) / 2.7.2.1 (JP) and panics otherwise | **Confirmed**, refined | `crates/eldenring/src/rva.rs` (`from_lang_version`, `unwrap_or_else(\|e\| panic!("{e}"))`). The panic is lazy: it fires on the first RVA-backed access (`GameDataMan`, `GameMan`, `FieldArea`, vtables, `cast_ray`). Singleton-found types (`WorldChrMan`, `CSEventFlagMan`, `CSFeMan`, `WorldAreaTime`) do not use it. https://github.com/vswarte/fromsoftware-rs/blob/main/crates/eldenring/src/rva.rs |
| 2 | `GameDataMan` has `boss_fight_active`, `boss_health_bar_entity_id`, `boss_health_bar_npc_param_id`, set while a boss bar is active | **Confirmed** (fields) / **Unverified** (semantics) | `cs/game_data_man.rs`: only `boss_fight_active` ("Whether a boss fight is currently active") and `boss_fight_timer` carry doc comments. The bar link is inferred from the names. Downgraded in §8.4 |
| 3 | `PlayerGameData.character_name` is `[u16;17]` at `0x9C`, the offset TGA reads via `[chr+0x580]+0x9C` | **Confirmed** (offset) / **Refuted** (the NPC inference) | Field arithmetic over `cs/player_game_data.rs` (`chr_type: ChrType` is `repr(i32)` at 0x98, so the name sits at 0x9C, followed by `gender` 0xBE and `archetype` 0xBF). er-mods-rs `stats_panel_text.rs` independently says "raw PGD name at `+0x9c`". But `[chr+0x580]` is `PlayerIns.player_game_data`, while `EnemyIns+0x580` is `com_manipulator` (`cs/chr_ins.rs`). The "NPCs carry a PlayerGameData block" inference is withdrawn (§6.2) |
| 4 | Flags 191–197 = Great Rune possession (191 Godrick … 197 Rennala); 180–187 = "owns ≥ N" | **Corrected** | `soulsmods/elden-ring-eventparam` `index.md` lines 750–764: 180 = "大ルーン所持数チェック：0個" (zero), 181–187 = "1個以上"…"7個以上". 193/194/195 use dev codenames (Margit / Ghoul King / Greater Demon). These map to Morgott/Rykard/Mohg via TGA kill flags 9104/9122/9112 and goods IDs 191–196. https://github.com/soulsmods/elden-ring-eventparam |
| 5 | Godrick defeated 10000800, encountered 10000801, global kill 9101 | **Confirmed** | TGA `Event Flags/Event Flag Manager/.cea` lines 1859–1862 (`isDefeated`, `isEncountered`, `9101 ボス撃破 接ぎ木の王`); SoulSplitter `Boss.cs` `GodrickTheGraftedStormveilCastle = 10000800`; eventparam 9101 "ボス撃破 接ぎ木の王" |
| 5b | Boss kill flags are 9100–9199; Mohg Lord of Blood and DLC bosses have no global flag | **Corrected** | eventparam lists per-boss "ボス撃破" entries through **9295** (9200+ = catacombs etc.). TGA: Mohg, Lord of Blood **9112**, Messmer **9146**, Bayle **9163**, Promised Consort Radahn **9143** |
| 6 | Flag 9000 is set while resting at a Site of Grace | **Confirmed** (independent) | Name: eventparam "篝火休息中フラグ". Behaviour: the grace talk ESD `t000001000` sets 9000 = 1 after `StartBonfireAnimLoop` and 0 on leaving; `common.emevd` clears it at init. https://github.com/lraty-li/Elden-Ring-Mods (`Gacha_Ring/t000001000.py` l.784/858, `emved/common.emevd.dcx.js` l.385). Caveat: a 2023 modded copy of the vanilla script |
| 7 | `CSChrRideModule.ride_node.ride_state` = 5 while riding (0 none, 3 mounting, 7 dismounting) | **Confirmed** (crate doc only) | `cs/chr_ins/module/ride.rs`: `/// 0 = none, 3 = getting on, 5 = riding, 7 = dismounting`. This is an author observation, not a binary enum. No second source checked |
| 8 | `IsBattleState` reads `[[arg+0xC950]+0x30C]==6` on a pooled `CSAiThink`; no fixed EnemyIns offset exists | **Confirmed** (read) / **Unverified → weakened** (no fixed path) | MapForGoblins `combat_state_gate_re_findings.md` + RESOLVED banner in `combat_enemy_list_structure_re_prompt.md` (2.6.2.0). The two docs contradict each other on the arg type. TGA `Targeted Npc Info/NpcThinkParam/.xml` uses a fixed chain `[[[[chr+0x58]+0x18]+0xC0]+0x30]` to the NpcThinkParam row on 1.17.2, which suggests a fixed ChrIns → AI-object path |
| 9 | Vanilla NpcParam has 7,039 rows, 715 with non-zero `nameId` | **Unverified** (single source) | MapForGoblins `windows_enemy_name_runtime_source_re_findings.md` §1 (regulation version not stated). Plausibility: Paramdex `ER/Names/NpcParam.txt` has 6,864 named rows. https://raw.githubusercontent.com/soulsmods/Paramdex/master/ER/Names/NpcParam.txt |
| 10 | SpEffect 9621 (stateInfo 457) is the only stateInfo-457 row and is applied by EMEVD in protected areas such as Roundtable | **Confirmed**, **Corrected** (scope) | er-mods-rs `docs/er-mechanics/map-combat-restrictions.md` (1.17.1): "the only row in the regulation with `stateInfo = 457`". In single player only Roundtable Hold applies it (event 11102600). Stormveil, Raya Lucaria, Volcano Manor and Mohgwyn apply it only under multiplayer state `Invasion` |
| 11 | `SetDisableAllChrUpdate(true)` freezes everyone incl. player, resumes instantly; poking the `WorldChrManDbg` flag does nothing | **Confirmed** (single source) with caveats | MapForGoblins `game_timestep_freeze_re_findings.md` "SOLVED" (2.6.2.0). Tested only via external `CreateRemoteThread`. The per-chr bit it XORs matches fromsoftware-rs `ChrDebugFlags.disabled_updates` (`cs/chr_ins.rs`) |
| 12 | ThrowParam 10000000 = backstab, 30000000 = riposte, resolved through `CSChrThrowModule` | **Confirmed** (single source) | er-mods-rs `docs/er-mechanics/crits.md` table (rows, anims, angles). `CSChrThrowModule.throw_node.throw_state` exists (`module/throw.rs`). No Paramdex ThrowParam names file exists to cross-check (404) |
| 13 | fromsoftware-rs `MsgRepositoryImp` is empty, so FMG lookup needs a custom `GetMessage` AOB | **Confirmed**, alternative added | `cs/msg_repository.rs`: `pub struct MsgRepositoryImp;`. MapForGoblins `src/re_signatures.hpp` `GETMESSAGE` (RVA 0x266d3c0 on 2.6.x, interior AOB). Offline FMG extraction is a no-RE alternative for vanilla text |
| 14 | Quest-NPC entity IDs in the MapForGoblins death-flag table all end in 07xx | **Refuted** as "all" / **Corrected** to "most" | `docs/emevd_death_flags_results.md`: Gurranq `1051430800`, Count Ymir `20010455`, Dung Eater `35000850`; mined from **ERR** EMEVD; the doc's own Iji row was wrong (`1034500710` = Ranni) |
| 15 | Subarea pop-up ID is set by EMEVD `2003[13]` | **Corrected** | DarkScript3 `er-common.emedf.json`: `2007[13]` = "Display Subarea Welcome Message", `2003[13]` = "Modify Navimesh Connection Bitflag". The fromsoftware-rs doc comment has the wrong bank |
| 16 | Archetypes 10/11 come from a "Tarnished Pack" | **Unverified**, label removed | TGA `ArcheTypeIdDLC3.xml` only says "DLC3". No source checked names the product |
| 17 | Banner IDs (1 DEMIGOD FELLED … 22 GOD SLAIN), `MenuType` values, Lua event IDs (4065/4066/4081/4082/4110) | **Confirmed** | `cs/menu_man.rs` constants, `cs/menu_type.rs`, `cs/lua_event_man.rs` |
| 18 | Talk commands `TalkToPlayer` 1, `ForceEndTalk` 8, `AddTalkListData` 19, `OpenRegularShop` 22; `GetTalkInterruptReason` / `HasPlayerBeenAttacked` used in ER | **Confirmed** | ESDLang `dist/ESDScriptingDocumentation_Talk.json` (bank 1; `games_used` includes `er`) |
| 19 | EMEDF Team Type values (6 Enemy, 7 Strong Enemy, 26 Friendly NPC, 27 Hostile NPC, 29 Indiscriminate) and Weather enum | **Confirmed** | DarkScript3 `er-common.emedf.json` enums |
| 20 | `GameMan+0xB60` = last rested grace (TGA) | **Confirmed** (1.17.2 TGA only) | TGA `Statistics/Character/Last Grace.xml` (`GameMan` + `B60`); fromsoftware-rs `cs/game_man.rs` `unkb60: u32` |
| 21 | Grace, legacy, time and weather flag names (71000/76000, 130–140, 530–532, 550–575, 211, 200/201) | **Confirmed** (names) / **Unverified** (runtime upkeep of 530–575) | eventparam `EFID_common` |
| 22 | fromsoftware-rs license is "MIT/Apache NON-AI" | **Confirmed** | `LICENSE-MIT-NON-AI`, `LICENSE-ASL2-NON-AI`; `Cargo.toml` still says `license = "MIT OR Apache-2.0"` (inconsistent) |
| 23 | Game patch level App 1.17.2 / exe 2.7.2.0, regulation 1.17.3 | **Confirmed** (exe/app, local evidence) / **Unverified** (regulation 1.17.3) | TGA `CHANGELOG.md` v1.19.2 (2026-10-20) "Supported version to ELDEN RING App Ver. 1.17.2"; er-mods-rs `docs/game-build.md` "2.7.2.0 (Steam patch 1.17.2, installed 2026-10-20)". Bandai Namco patch notes were unreachable this session |
