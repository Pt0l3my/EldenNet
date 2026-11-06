# SkyrimNet Research 08: The Skyrim engine under an AI NPC mod

What the Creation Engine (Skyrim SE/AE/VR) provides, which parts SkyrimNet relies on, and what the Elden Ring analogues are.

> **Method and license note.** The SkyrimNet evidence comes from documentation, human-readable shipped content (Papyrus `.psc` sources, the Papyrus headers the repo compiles against, the Spriggit ESP tree, YAML, SQL migrations, the decorator reference, the MIT C++ API headers) and public GitHub pages and issues. No compiled file (DLL, `.pex`, bundled JS, or the binary `SkyrimNet.esp`) was inspected. All ESP facts come from the Spriggit tree. Some public issues quote `SkyrimNet.log` lines. Facts built on them are marked as coming from user reports.
>
> Engine facts come from open-source engine bindings: CommonLibSSE-NG and SKSE64, read on GitHub. Web search snippets were also used.
>
> UESP and the Creation Kit wiki, Nexus and other wikis were **unreachable from this session** (DNS or proxy refusal). Claims that would normally cite them are tagged **BACKGROUND**. These are long-established modding knowledge that was not re-verified here.
>
> Other tags: **FACT** means from a cited source. **WEB** means from a search-result snippet; the URL is given. **INFERENCE** is my reading of the evidence. Elden Ring analogues cite the EldenNet Elden Ring notes and keep their tags.
>
> SkyrimNet's prompts are not reproduced. Only decorator names and short API comments are quoted.

**Path prefixes**

| Prefix | Location |
|---|---|
| `DATA/` | `skyrimnet-beta30-rc2/SKSE/Plugins/SkyrimNet/` |
| `LIB/` | `DATA/library/skyrimnet.base/` |
| `SRC/` | `skyrimnet-beta30-rc2/Source/Scripts/` |
| `DEV/` | `skyrimnet-devkit-beta30-rc2/` |
| `GH/` | clone of `MinLL/SkyrimNet-GamePlugin` |
| `ESP/` | `GH/spriggit/SkyrimNet/` (the ESP as JSON) |
| `HDR/` | `GH/headers/` (Papyrus headers SkyrimNet compiles against) |
| `CL/` | CommonLibSSE-NG, `https://github.com/CharmedBaryon/CommonLibSSE-NG` (main, read 2026-11-06) |
| `SKSE/` | `https://github.com/ianpatt/skse64` (master, read 2026-11-06) |
| `SN0x` / `ER0x` | sibling notes `docs/research/skyrimnet/0x-*.md` / `docs/research/eldenring/0x-*.md` |

---

## TL;DR

- **Skyrim gives an AI mod four large gifts that Elden Ring does not.**
  1. A record database with stable, addressable identities: plugin plus local FormID, and EditorIDs.
  2. A script VM (Papyrus) whose scripts attach to those records.
  3. A typed engine event bus: 53 `BSTEventSource<TES…Event>` sources are script-visible (`CL/…/ScriptEventSourceHolder.h`).
  4. A data-driven behaviour layer: AI packages, factions, relationship ranks, crime, quest stages, voice types and the condition system.

  SkyrimNet uses all four, but does nearly all of its work in native C++.
- **Identity is load-order-relative.** A runtime FormID is the plugin's slot plus a local id. `0xFE` plus a 12-bit index marks light plugins (`CL/RE/T/TESFile.h: GetPartialIndex`). SkyrimNet's content files and identity links use `Plugin.esp|0xLocalID` and `npc:Plugin.esp:0x…`, never the runtime id (`DEV/docs/modding/CONTENT_ROOTS.md`). Its database added plugin plus local id next to the legacy runtime id (migration `0020`). Its internal actor UUID is still a hash of display name and masked FormID, so renames fork identities and need an alias table (migration `0021`; §2.5). The **base vs reference** split (player base `0x7`, player reference `0x14` in `ESP/`) decides whether two bandits are one "person" or two.
- **Only nearby, loaded, high-process actors are fully "alive".** References attach and detach with cells. AI runs at four process levels (`CL/RE/A/AIProcess.h`). Load screens invalidate actor pointers. SkyrimNet's recurring worker-thread CTDs (#819: the reporter "loaded into Riverwood, crashed about 4 seconds after the loading screen") show this lifecycle in action.
- **Skyrim's dialogue is a conditioned database:** Quest → Branch → Topic → Info, voice-type-gated `.fuz` (= `.xwm` + `.lip`) files and a subtitle manager. The 103-value topic `Subtype` enum is Bethesda's **bark taxonomy**: Attack, Hit, Flee, Bleedout, Death, Taunt, AllyKilled, NormalToCombat, CombatToLost, LostIdle, DetectFriendDie, NoticeCorpse, Hello, Goodbye and more (`CL/RE/T/TESTopic.h`). SkyrimNet reads vanilla lines and can fire them, but it generates its own speech and has no separate bark path (SN05 §8).
- **Papyrus is too slow and too coarse for per-prompt state.**
  - Scripts lock per object, and most native calls wait on the frame.
  - The VM gets about 1.2 ms per frame (`fUpdateBudgetMS`, WEB).
  - SkyrimNet's own guide calls a Papyrus decorator "a VM round trip on every prompt render, on the render's critical path" (SkyrimNet-ExamplePlugin `docs/best-practices.md`).

  So Papyrus is glue: packages, look-at, hotkeys, the MCM and ModEvents.
- **SKSE and CommonLibSSE-NG** supply lifecycle messages (`kDataLoaded`, `kPostLoadGame`, `kNewGame`, `kSaveGame`, …), event sinks (dispatched synchronously on the sender's thread under a spin lock), a task queue for game-thread work, co-save records and trampolines. The **Address Library** maps stable IDs to per-runtime addresses for SE 1.5.97, AE 1.6.x and now 1.7.104 with a "V5" encoding (`SKSE/skse64_common/skse_version.h`, `SKSE/skse64/PluginAPI.h`). CommonLib-NG absorbs struct moves such as Actor runtime data shifting from +0xE0 to +0xE8 at 1.6.629 (`CL/RE/A/Actor.h`).
- **UI:** Scaleform SWF menus, the SkyUI MCM and message boxes for vanilla-style UI. SkyrimNet's chat, dashboard and subtitles run in **Prisma UI** (Ultralight HTML/CSS/JS).
- **Saves:** `.ess` holds change forms and Papyrus VM state, and SKSE writes a `.skse` co-save. SkyrimNet keeps its own per-playthrough SQLite plus a playthrough id and a timeline check. Script registrations live on in old saves after the code that made them is gone (`SRC/skynet_Library.psc: Maintenance`).
- **Elden Ring has none of the four gifts.** It has params, MSB placements, event flags, talk ESD, Wwise and Scaleform (ER01). EldenNet must therefore:
  - replace "subscribe to an event" with "diff a per-frame snapshot";
  - replace "look up a record by EditorID" with "read a param row by id";
  - run everything natively, which is where SkyrimNet itself has been heading.

---

## 1. What an AI NPC mod needs from an engine

An NPC dialogue mod needs six capabilities:

1. **Identify** who an actor is, stably across sessions and patches.
2. **Locate** it relative to the player and the world.
3. **Observe** what happens.
4. **Present** speech as voice, subtitles and facing.
5. **Persist** memories across saves.
6. **Act**, as an optional step.

Each capability maps onto an engine feature (§11). For EldenNet v1 only the first five matter. The sections below go through Skyrim's version of each.

---

## 2. Data model: plugins, FormIDs, records

### 2.1 Plugins and load order

- **FACT** (`CL/RE/T/TESFile.h`): a plugin file is a `TESFile` with record flags. `kMaster = 1 << 0` makes it a master (ESM). `kSmallFile = 1 << 9` makes it **light** (ESL, or an ESL-flagged ESP). Each loaded file gets a `compileIndex` (8-bit, full plugins) or a `smallFileCompileIndex` (16-bit field, light plugins).
- **BACKGROUND:** the user's load order sets those indices. Masters load first, and a later plugin's version of a record overrides an earlier one ("last wins" per record). Up to 254 full plugins can be loaded, plus up to 4096 light plugins in slot `0xFE`. Typical modded installs are large. One SkyrimNet bug report lists "154 SKSE DLL plugins, custom load order (~800 plugins)" (#831).
- **WEB:** light plugins could originally hold only object ids `0x800–0xFFF`. Skyrim 1.6.1130 added header version 1.71, which also unlocks `0x001–0x7FF` (esplugin and LOOT changelogs; https://github.com/Ortham/esplugin/blob/master/CHANGELOG.md, https://loot-api.readthedocs.io/en/0.22.3/api/changelog.html).
- **SkyrimNet's own plugin.**
  - FACT (`ESP/RecordData.json` and the record file names): `SkyrimNet.esp` has one master, `Skyrim.esm`. Its records use local ids `0x000001` to `0x0012F5`. The low ids are the dialogue and follow packages and their templates (`0x1`, `0x3`–`0x5`, `0x7`, `0x8`), two keywords (`0x2`, `0x6`) and a Message (`0x9`). The main quest is `0x000802`. Most later records sit in `0x0D63`–`0x12E9`. The highest id, `0x12F5`, is a placed reference inside the test cell `SkyrimNetTestCell` (`0x12E7`).
  - The Spriggit header shows only the master list and `INTV`. It does not show the header version or the ESL or ESM flags. The binary `SkyrimNet.esp` was deliberately not opened, so those stay unknown.
  - **INFERENCE:** ids above `0xFFF` mean it cannot be a light plugin, so it takes a full load-order slot. Header 1.71 is documented as unlocking ids below `0x800` for *light* plugins (WEB, above). Whether a full plugin with low ids needs it too is UNVERIFIED. If it does, that would explain why the README asks Skyrim SE users without ESL support to install BEES (Backported Extended ESL Support), and VR users the VR ESL Support mod (`GH/README.md`).

### 2.2 FormIDs and load-order-relative identity

- **FACT** (`CL/RE/T/TESFile.h`): a runtime FormID is `partialIndex << 24 | localId` for full plugins. For light plugins, `GetPartialIndex()` returns `0xFE000 | smallFileCompileIndex`, so the top 20 bits name the file and 12 bits remain for the record. BACKGROUND: index `0xFF` holds forms created at runtime and saved in the `.ess`. PO3's `IsGeneratedForm` exists to detect these (`HDR/PO3_SKSEFunctions.psc`).
- **The engine resolves (plugin, local id) for you.** In C++ that is `TESDataHandler::LookupForm(localFormID, modName)` (`CL/RE/T/TESDataHandler.h`). In Papyrus it is `Game.GetFormFromFile(localId, "Plugin.esp")`. SkyrimNet's scripts call it 14 times. Ten calls find their controller quest `0x0802` (9 in `SRC/SkyrimNetInternal.psc`, 1 in `skynet_McmScript.psc`). Two fetch `Skyrim.esm 0x084D1B`, the vanilla player-follower faction (BACKGROUND for the name; the script variable is `factionCompanion`, `SkyrimNetInternal.psc` and `skynet_Library.psc`). Two fetch the actor whitelist and blacklist factions `0x12DA`/`0x12DB` (`skynet_WheelMenu.psc`). `Game.GetModByName("vrik.esp") == 255` is its "is this mod installed" test (`SRC/skynet_Library.psc`).
- **What SkyrimNet persists** (FACT):
  - Content files must reference forms as `"Plugin.esp|0x01396B"` (24-bit local id, or 12-bit for `.esl`) and NPCs as `npc:Plugin.esp:0xLocalID`. "The runtime form id spelling, `npc:0A012345`, depends on load order; the hub refuses it" (`DEV/docs/modding/CONTENT_ROOTS.md`).
  - Migration `0020_uuid_mappings_modfile.sql` added `mod_file + local_form_id` so that identity-link NPC refs "resolve across load-order changes". The raw runtime `form_id` column stays for legacy refs, and old rows gain the new columns "the next time the actor is sighted".
  - Hand-written bios are keyed by English name plus `FormID & 0xFFF` (`lydia_C94.prompt`; SN03 §5.5).
- **Lesson for any game:** the identity you store must be the one that survives the user's mod setup changing. In Skyrim that is the defining plugin plus its local id.

### 2.3 Base objects vs references

- **BACKGROUND:** a *base object* (`TESNPC`, record `NPC_`) is the template: name, race, class, voice type, factions, AI data, default packages. A *reference* (`Actor`, a `TESObjectREFR` placed in a cell, record `ACHR`) is one instance in the world, with position, current health, inventory changes and the running AI process.
- **FACT** (`ESP/`):
  - SkyrimNet's player alias is filled with `UniqueActor = 000007:Skyrim.esm`, the player's *base* NPC.
  - Its talk packages target `000014:Skyrim.esm`, the player *reference*. This is the most-referenced vanilla form in the ESP: 30 times.
  - The C++ API documents `0x14` as "the player" (`DEV/CppAPI/PublicAPI.h`).
- **Generic NPCs.** BACKGROUND: generics are typically spawned from leveled-actor lists (`LeveledActor.psc` is among the headers), and NPC templates are resolved at load (`TESResolveNPCTemplatesEvent` exists, `CL/…/ScriptEventSourceHolder.h`). Many references therefore share one base and one display name ("Bandit").
- **INFERENCE:** that is why SkyrimNet separates *unique* bios (keyed per NPC record) from *generic* bios (`bandit_generic.prompt`, matched by display name, 302 files), and gives each actor its own UUID. SN03 §5.5 documents the "Traveler" collision this caused.
- **SkyrimNet's own ESP shows the split.** FACT (`ESP/Cells/9/3/SkyrimNetTestCell - 0012E7…/RecordData.json`): the interior test cell holds `PlacedNpc` (ACHR) references `0x12EA` and `0x12EB`. Their `Base` fields point at the NPC_ records `SkyrimNet_TestNpcAlpha` `0x12E8` and `…Beta` `0x12E9`. Both references are flagged `Persistent`, so they exist even while the cell is unloaded (BACKGROUND for that semantics). The rest of the cell is placed vanilla objects (`PlacedObject` refs to `Skyrim.esm` bases).
- **Names belong to references too.** FACT: SkyrimNet ships a Real Names patch. It renames generic actors with `ObjectReference.SetDisplayName(newName + " [" + oldName + "]")`, choosing the name pool by faction (bandit, Forsworn, guard, Thalmor, vampire, …) (`SRC/RealNamesChange.psc`). The release also ships an `NPCsNamesDistributor.ini` for the NPCs Names Distributor mod. It shows generated names with the title in brackets (`iFormat = 3`) in the crosshair, subtitles, dialogue and dialogue history (`SKSE/Plugins/NPCsNamesDistributor.ini`). **INFERENCE:** these mods give each generic a per-reference display name, so "Bandit" becomes a distinct person in the text. They also change the very input SkyrimNet hashes into its UUIDs (§2.5).

### 2.4 Records an NPC mod reads, and the EditorID problem

**What an NPC record carries.** FACT: SkyrimNet's test NPC shows the fields (`ESP/Npcs/SkyrimNet_TestNpcAlpha…json`):

- flags: `Essential`, `Unique`, `AutoCalcStats`
- level and `DispositionBase`
- `Race` and `Class`
- `Voice` (a `VoiceType`)
- `AIData`: Confidence, EnergyLevel, Responsibility ("NoCrime"), Assistance ("HelpsNobody"), warn/attack thresholds

**Other forms that matter to dialogue:**

| Form | Why it matters |
|---|---|
| Faction (ranks; crime factions) | membership, standing, bounty |
| Keyword | tags on actors, items, locations; SkyrimNet uses two as linked-ref keys (`ESP/Keywords`) |
| Location (`BGSLocation`) | named places with a parent chain and location-type keywords |
| Quest | stages and aliases |
| Global | numeric world state |
| VoiceType | which voice files an actor can use |

**The decorator surface over these forms.** FACT: SkyrimNet's template functions wrap almost all of these:

- `decnpc` returns name, race, gender, class, level, stats, skills and `relationshipRank` "-4 to 4".
- Faction: `is_in_faction`, `get_faction_rank`.
- Keywords: `actor_has_keyword`, `worn_has_keyword`.
- Quests: `get_quest_stage`, `is_quest_active`, `get_quest_journal`, `get_quest_alias_actor`.
- Load order and globals: `get_global_value`, `is_plugin_loaded`, `formid_to_uuid`.
- Magic: `has_magic_effect`, `has_spell`, `has_perk`.
- Sources: `DEV/docs/modding/WORKFLOW_PROMPTS.md` §2.2 and a call count over `LIB/prompts`, which has 292 `decnpc(` calls, 37 `is_in_faction(` and 25 `worn_has_keyword(`.

**EditorIDs are the human keys, but the engine mostly throws them away.**

- FACT (`CL/RE/T/TESTopic.h`): topics keep `formEditorID` in memory, so some form types do.
- WEB: for most forms the runtime `GetFormEditorID` "still returns an empty string (for forms that normally discard editorIDs)". powerofthree's Tweaks restores them (Nexus 51073, search snippet: https://nexusmods.com/skyrimspecialedition/mods/51073).
- FACT: SkyrimNet requires po3 Tweaks "with its `Load EditorIDs` fix left on" (`GH/README.md`). Its decorators and filters take EditorIDs, such as `is_in_faction(uuid, "CrimeFactionWhiterun")` and `get_crime_gold(player.UUID, "CrimeFactionRift")`.
- **Lesson:** a human-readable key layer over numeric ids is worth an extra dependency.

**Engine forms as eligibility keys.** FACT: SkyrimNet's actor filters are lists of EditorIDs. `filters/*.yaml` of `kind: actor` or `kind: memory` take `FactionWhitelist`/`FactionBlacklist`, `RaceWhitelist`/`RaceBlacklist` and `GenderWhitelist`/`GenderBlacklist` (`DEV/docs/modding/CONTENT_ROOTS.md`). The shipped filters stop cats, dogs and wolves from speaking by race (`ACLRaceCat`, `DogRace`, `WolfRace`) and do the same for display mannequins (`ManakinRace`). They also turn SkyrimNet's own whitelist and blacklist factions into always-allow and always-block switches (`LIB/filters/animals_dont_speak.yaml`, `mannequins_dont_speak.yaml`, `skyrimnet_filter_factions.yaml`). A small `LIB/furniture/` root maps furniture forms to display names, e.g. `Skyrim.esm|0x06CF36` to "Counter". So even the library curates human-readable names for engine forms.

### 2.5 One character, many records; one record, many names

The engine's identity model has two quirks that an AI memory has to absorb.

- **Story characters are often several NPC_ records.** BACKGROUND: quests often swap a character's record by disabling one reference and enabling another: a ghost, a Sovngarde version, a post-quest version or a relocated copy. FACT: SkyrimNet ships 12 `identity/*.yaml` records of `kind: succession`. Each one carries memory `from:` one `npc:skyrim.esm:0x…` to another: Astrid, Cicero (two files that chain three records, `0x0550F1` → `0x01E64A` → `0x09BCB0`), Kodlak Whitemane, Hakon One-Eye, Gormlaith, Felldir, Ulfric, Galmar, Erik, Sam Guevenne and Sinding (`LIB/identity/vanilla_*.yaml`). Identity links use the same `npc:Plugin.esp:0xLocalID` spelling (`CONTENT_ROOTS.md`). The hand-written bio follows one of the records. `cicero_CB0.prompt` matches the final Dawnstar Cicero (`0x09BCB0`), and `kodlak_whitemane_68F.prompt` matches the living Kodlak (`0x01A68F`).
- **A display-name change forks the UUID.** FACT (migration `0021_identity_aliases.sql`): "A UUID is a hash of display name and masked FormID, so a display-name change mints a NEW uuid and orphans the old one". When one stable identity (`mod_file` + `local_form_id`) turns up under a second UUID, SkyrimNet records an alias row. It unions the chain at recall time and rewrites nothing. Alias rows carry `game_time`, so loading a save from before a rename undoes it.
- **INFERENCE:** in Skyrim, "who is this?" has three layers: runtime handle, record identity (plugin + local id), and *character* identity (succession plus aliases). SkyrimNet arrived at all three through migrations rather than by design. Rename mods (§2.3), `SetDisplayName` and quest record swaps are ordinary events in a modded game.

---

## 3. The world: worldspaces, cells, loaded area, 3D

- **Structure.** BACKGROUND: the world is split into **worldspaces** (Tamriel, Solstheim, city worldspaces) tiled into exterior **cells** of 4096 × 4096 units, plus standalone **interior cells**. **Locations** (`BGSLocation`) sit on top: named regions with parent links (house → city → hold) and type keywords. They drive "where am I" text, crime jurisdiction and Radiant quest targeting.
- **FACT, SkyrimNet's location string.** It renders as "Breezehome, Kitchen, Hold: Whiterun". SkyrimNet's *named places* add sub-locations "matched only in its own interior cell (or worldspace outdoors), within its radius and 200 units up or down" (`SRC/SkyrimNetApi.psc`, Named Places section). Event history treats a change of `location_building` as a move (`LIB/prompts/components/event_history.prompt`). Weather wording depends on `is_indoors` (`components/context/scene_context.prompt`).
- **Loaded area.** WEB: the loaded area is `uGridsToLoad` cells around the player, default 5, about 10,250 units (https://forums.nexusmods.com/topic/5068975-ugridstoload-talk-and-testing/page/3).
  - BACKGROUND: references outside it have no 3D. They attach and detach as cells load. Script events exist for both (`TESCellAttachDetachEvent`, `TESObjectLoadedEvent`, `TESCellFullyLoadedEvent`; `CL/…/ScriptEventSourceHolder.h`).
- **AI process levels.** FACT (`CL/RE/A/AIProcess.h`, `ProcessLists.h`): `PROCESS_TYPE` is `kHigh=0`, `kMiddleHigh=1`, `kMiddleLow=2`, `kLow=3`. `ProcessLists` keeps separate handle arrays per level and a `ForEachHighActor` iterator.
  - WEB: in the closely related Fallout engine, high-process actors update every frame, middle-high about every 18 s and low about every 120 s (https://geck.uesp.net/wiki/GetActorProcessingLevel; community measurements).
  - **INFERENCE:** only high-process actors (near the player, 3D loaded) can usefully speak, turn and lip-sync. "Nearby actors" in an AI mod should be drawn from the high list, not from all references.
- **Units.** FACT: SkyrimNet converts at "128 units = 1.828 meters" (`DATA/docs/decorators.html: units_to_meters`). Its radii are 4000 units for `FindActorByName`, 2000 for trigger `nearbyRadius`, and 256 and 512 in the talk packages (SN05 §4.2).
- **Why the lifecycle matters.** FACT, #819 (Skyrim AE 1.6.1170, SkyrimNet 0.29.1): the reporter "loaded into Riverwood, crashed about 4 seconds after the loading screen". "Both times the crashing thread was rendering character profiles". The last log entry pointed at a fallback template for a Whiterun guard, and the stack was in engine code at "Address Library ID 14833+0x58". The reporter calls it "a race on actor data" (https://github.com/MinLL/SkyrimNet-GamePlugin/issues/819). **INFERENCE:** cell transitions are when actor objects are torn down and rebuilt. Any off-thread reader of live actor memory is most exposed then. SN01 §12 collects the related CTDs.

---

## 4. Actor AI

### 4.1 Packages, procedures, schedules, sandboxing

**How behaviour is chosen.** BACKGROUND: an actor's behaviour comes from a **package stack**. Sources include the base NPC, quest aliases, scenes and script overrides. The engine re-evaluates the stack and runs the first package whose conditions and schedule (month, weekday, hour, duration) pass. Each package runs a **procedure tree**.

**The procedures.** FACT (`CL/RE/T/TESPackage.h: PACKAGE_PROCEDURE_TYPE`):

- Find, Follow (1), Escort (2), Eat, Sleep, Wander, Travel
- Accompany (7), UseItemAt, Ambush, FleeNotCombat, CastMagic, **Sandbox (12)**, Patrol, Guard
- **Dialogue (15)**, UseWeapon, Activate, Alarm, Flee, Trespass, Spectator, ReactToDead, DoNothing, and others

**What this gives the world.** "Sandbox" is what makes townsfolk wander, sit, eat and lean within a radius. With schedules, it gives the city its daily rhythm (Radiant AI's legacy; §10).

**SkyrimNet's use** (FACT; full detail in SN05 §11–12):

- `SkyrimNet_PlayerDialoguePackage` travels to within 512 units of the player, then 256. Its conditions skip actors that are sitting, in a scene, or whose current procedure is Follow, Escort or Accompany (`GetCurrentAIPackage != 1/2/7`, `ESP/Packages/…000003…json`).
- It is applied in this order: `SetLookAt(target)`, then a keyword linked ref to the target (PO3 `SetLinkedRef` with `SkyrimNet_DialogueTargetKeyword`), then `SkyrimNetApi.RegisterPackage(actor, "TalkToPlayer" or "TalkToNPC", …)`, then `EvaluatePackage()`. Clearing reverses it (`SRC/skynet_MainController.psc: SetActorDialogueTarget`, `ClearActorDialogueTarget`).
  - The NPC-to-NPC package targets "my linked ref with keyword `0x000002`" (`PackageTargetLinkedReference`). The player packages target `Skyrim.esm 0x14` directly (`ESP/Packages/…000004…json`, `…000003…json`). So one package record serves every NPC pair, and the pair is set at runtime through the linked ref.
  - Each package is built from a **package template** (`Type: PackageTemplate`, e.g. `SkyrimNet_PlayerDialoguePackageTemplate 0x000001`) whose public inputs (target, radius, flags) the concrete package fills (`ESP/Packages/`). BACKGROUND: that is the CK's standard way to reuse procedure trees.
- Package overrides go through PapyrusUtil `ActorUtil.AddPackageOverride` (`HDR/ActorUtil.psc`) and SkyrimNet's own registry (`SkyrimNetApi.RegisterPackage`/`RegisterPackageEx`). FACT (`SRC/SkyrimNetApi.psc`, Package Management): the registry has one row per actor and package, plus an **owner** mod name and a priority. The highest priority wins across SkyrimNet rows *and* mirrored ActorUtil overrides, and SkyrimNet saves its copy of those overrides with the game. Mods can `HoldPackages` for the length of their own scene. Holds are owned, survive saves and are cleared by death. `ReinforcePackages` re-applies everything. ModEvents such as `SkyrimNet_OnPackageRemoved` (reason "replaced") tell owners when they lose a row.
- **Lesson:** even "stop and face me" is an AI-behaviour change in Skyrim. It must be negotiated with followers, scenes and other mods, which is why it produced bugs (#141, #156, #644).

### 4.2 Combat AI and combat styles

- **Combat styles.** FACT (`CL/RE/T/TESCombatStyle.h`): a combat style (CSTY) parameterizes combat AI, with general data (`offensiveMult`, `defensiveMult`, `avoidThreatChance`, …) and melee, close-range, long-range and flight blocks. A `CombatController` runs an actor's fight.
- **Combat state.** BACKGROUND: actors move through normal → alert/searching → combat → lost states. Each transition can play a voice line (§5.2). FACT: engine events `TESCombatEvent`, `TESHitEvent`, `TESDeathEvent`, `TESEnterBleedoutEvent` and `TESSpellCastEvent` are script-visible.
- **SkyrimNet's combat inputs.** FACT: combat decorators include `is_in_combat`, `get_combat_targets`, `get_actor_value(uuid,"Health")`, `has_weapon_drawn` and `is_hostile_to_actor` (13 call sites in `LIB/prompts`). Its event types include "combat", "death", "hit", "spell_cast" and "equip" (`PublicAPI.h: PublicRegisterEventCallback`). It keeps these out of long-term history (SN05 §2–3).

### 4.3 Detection and line of sight

- **What the engine offers.** FACT (`CL/RE/A/Actor.h`): `Actor::RequestDetectionLevel(target, priority)` and `Actor::HasLineOfSight(ref, …)`. BACKGROUND: detection combines sight (light level, view cone, LOS), sound (movement, armour weight, combat noise) and sneak skill. The engine already runs it for every high-process actor.
- **What SkyrimNet uses.** FACT: a `has_line_of_sight` decorator (only in the test submodules), and LOS inside audience selection. Fixes include "line of sight no longer leaks through walls and closed doors inside one interior cell" (SN05 §4.3). The engine's detection state is not used as a speech or knowledge gate. Issue #771, "Enemies in search mode issue combat dialogue targeted at player", reports enemies that are only *searching* (they never saw the sneaking player) speaking "as if they are in combat" and calling the player by name (https://github.com/MinLL/SkyrimNet-GamePlugin/issues/771).
- **INFERENCE:** SkyrimNet bypasses the engine's detection model and builds its own audience rule (radius plus LOS). The engine model is per-pair, latent and tuned for combat AI, not for "who heard that sentence".

### 4.4 Factions, crime, relationships, disposition

- **Relationship ranks.** FACT (`CL/RE/B/BGSRelationship.h`): nine levels, from Lover, Ally, Confidant, Friend and Acquaintance through Rival, Foe, Enemy to Archnemesis. SkyrimNet exposes them as `relationshipRank` −4…4 and `get_relationship_rank(a, b)` (`WORKFLOW_PROMPTS.md`).
- **Factions.** BACKGROUND: factions carry ranks, inter-faction reactions (friend, ally, enemy, neutral) and, for crime factions, a jurisdiction, bounty and jail.
- **Crime.** FACT: `Actor::GetCrimeGoldValue(faction)` (`CL/…/Actor.h`). SkyrimNet's `get_crime_gold(player.UUID, "CrimeFactionWhiterun")` reads the player's bounty per hold. Its test notes that it "only works for player" (`LIB/prompts/submodules/test_decorators/0400_faction_relationship_decorators.prompt`; it is not in `decorators.html`). `get_civil_war_side` returns the player's civil-war allegiance (`DATA/docs/decorators.html`, Game System). The HTML reference documents only about 40 decorators. The fuller catalogue is the in-game `get_decorators` listing (`WORKFLOW_PROMPTS.md` §2.2). In the shipped base library, `get_crime_gold`, `get_relationship_rank` and `has_line_of_sight` appear only in the decorator self-test submodules (`LIB/prompts/submodules/test_decorators/*`). They are available to prompt authors, but the default prompts do not lean on them.
- **Personality fields.** BACKGROUND: `AIData` on the NPC carries aggression, confidence, morality and assistance. Morality is called "Responsibility" in Mutagen (§2.4). These are engine-level personality knobs a bio generator can read.

### 4.5 Followers

- **Engine side.** BACKGROUND: vanilla followers are a quest-alias system. The `DialogueFollower` quest holds a Follower alias, a package stack in the alias, and follower factions; `IsPlayerTeammate()` exists (`CL/…/Actor.h`).
- **SkyrimNet's use.** FACT:
  - Decorators can read the follower alias: `get_quest_alias_actor("DialogueFollower","Follower")` and `get_script_property("DialogueFollower","dialoguefollowerscript","pFollowerAlias")`. Both are exercised in `LIB/prompts/submodules/test_decorators/0600_quest_gamesystem_decorators.prompt`. The production prompts use `is_follower` (12 calls).
  - Its main quest has a property `factionPlayerFollowers` → `Skyrim.esm 0x084D1B` (`ESP/Quests/skynet_MainController…json`).
  - Its talk package refuses to override Follow, Escort and Accompany (§4.1).

  Followers "only turn their heads" (SN05 §12).

---

## 5. Dialogue system

### 5.1 Quest → Branch → Topic → Info

**Structure.** FACT (`CL/RE/T/TESTopic.h`, `TESTopicInfo.h`):

- A `TESTopic` (DIAL) has an `ownerBranch` (`BGSDialogueBranch`), an `ownerQuest`, a priority, a type and a `Subtype`, and an array of `TESTopicInfo` children.
- Each info (INFO) holds one or more responses with an emotion type: Neutral, Anger, Disgust, Fear, Sad, Happy, Surprise or Puzzled.
- Info flags include `kSayOnce`, `kRandom`, `kIsForceGreet`, `kForceSubtitle`, `kNoLIPFile`, `kSpendsFavorPoints` and `kStartSceneOnEnd`. A `kSaidOnce` change flag is stored in the save.

**Selection.** BACKGROUND: when an NPC must say something in a topic, the engine walks that topic's infos in order and picks the first whose **conditions** pass, or a random passing one for `kRandom`. Conditions are hundreds of condition functions such as `GetIsVoiceType`, `GetInFaction`, `GetStage` and `GetRelationshipRank`.

**Other pieces.** BACKGROUND:

- Infos can run Papyrus **fragments** (TIF scripts) that set stages or give items.
- **Scenes** (SCEN) sequence multi-actor exchanges in phases.
- **Shared Info** lets many topics reuse one recorded line. WEB: it is "a special type of Misc Dialogue Topic", and referencing infos still need their own voice-type conditions (https://ck.uesp.net/wiki/SharedInfo).

**Lesson.** FACT, the player-facing loop: the dialogue menu shows branches gated by conditions, and topic subtypes include Persuade-type actions (Intimidate, Flatter, Bribe), Gift and Favor. Skyrim's "free" conversation is really a **condition-filtered menu over a pre-written database**. SkyrimNet's quest-dialogue feature turns that database into LLM context:

- `available_dialogue(npc.UUID, "quest")` returns the NPC's currently available branches as example exchanges.
- A `TriggerDialogueLine` action fires the chosen info.
- `LIB/dialogue_actions/*.yaml` deny-lists block infos whose fragments set quest stages.
- See SN06 §5.2. **INFERENCE:** `available_dialogue` evaluates the same conditions the engine would.

### 5.2 Barks are topic subtypes

FACT (`CL/RE/T/TESTopic.h: Subtype`). The engine's own ambient and combat vocabulary is a fixed list of situations. Each one is a topic that NPCs speak when the AI enters that state:

| Group | Subtypes |
|---|---|
| Combat actions | Attack, PowerAttack, Bash, Hit, Block, Taunt, Flee, Bleedout, AvoidThreat, Death, Yield, AcceptYield, GroupStrategy, AllyKilled |
| Awareness transitions | AlertIdle, LostIdle, NormalToAlert, AlertToCombat, NormalToCombat, AlertToNormal, CombatToNormal, CombatToLost, LostToNormal, LostToCombat, DetectFriendDie, ObserveCombat, NoticeCorpse |
| Crime | Steal, Assault, Murder, PickpocketCombat, Trespass, …NPC variants, WereTransformCrime |
| Social and ambient | Hello, GoodBye, Idle, TimeToGo, KnockOverObject, DestroyObject, StandonFurniture, LockedObject, PlayerShout, PlayerCastProjectileSpell… |
| Breath and grunts | EnterSprintBreath, OutofBreath, **CombatGrunt** (101), LeaveWaterBreath |

- **Vanilla behaviour.** BACKGROUND: these live mostly in the `DialogueGeneric` and combat quests, conditioned by voice type. A Nord guard's "I used to be an adventurer like you" is a Hello info for guard voice types.
- **SkyrimNet's relation to them.** FACT: SkyrimNet filters the grunt subtitles out of its captured history (`LIB/filters/combat_grunts.yaml`, SN06 §5.4). That filter is a `dialogue_rule` regex over the *subtitle text* (a fixed list of grunt spellings replaced by ""), not a filter on the `CombatGrunt` subtype. A sibling rule blanks the vanilla placeholder subtitle "(Invisible Continues Linked to Self)" (`LIB/filters/invisible_continues.yaml`). **INFERENCE:** the captured line is handled as text, and vanilla dialogue data contains non-speech placeholder infos that any capture layer must scrub. SkyrimNet does not generate per-subtype barks. Combat speech goes through the normal dialogue pipeline with combat-conditioned prompt modules (SN05 §8.1).
- **INFERENCE:** the subtype list is a ready-made, game-tested **trigger taxonomy** for a bark system, worth copying as an idea.

### 5.3 Voice types, `.fuz`, lip-sync, subtitles

- **Voice types and files.** BACKGROUND and WEB: every NPC has one `VoiceType`. Voice files live at `Sound/Voice/<Plugin>/<VoiceType>/<quest>_<topic>_<infoID>_<n>.fuz`. A `.fuz` bundles an xWMA audio stream with a `.lip` lip-sync track (https://wiki.beyondskyrim.org/wiki/Arcane_University:Voice_Line_Implementation, snippet). One recorded line can cover many NPCs only if they share a voice type.
- **INFERENCE:** this is why vanilla generic dialogue is condition-gated by voice type, and why SkyrimNet ships a `SilentVoice` voice type (`ESP/VoiceTypes`) for actors whose speech is entirely TTS.
- **Subtitles.** FACT (`CL/RE/S/SubtitleManager.h`): the engine keeps `SubtitleInfo{speaker, subtitle, targetDistance, forceDisplay}` entries and shows them by distance. SkyrimNet rewrites vanilla subtitles and also draws its own (SN06 §2, §6).
- **How TTS enters the engine's audio system.** FACT (`ESP/`): SkyrimNet adds no DIAL or INFO records. Its audio records are:
  - three **Sound Categories**, `SkyrimNet_AudioCategoryVoice`, `…VirtualSpeakers` and `…BardSinging`, each flagged `ShouldAppearOnMenu` so it gets a volume slider in Skyrim's Audio settings and each parented to a vanilla category (`0EB803:Skyrim.esm`);
  - one **Sound Output Model**, `SkyrimNet_SOMDialogue2DDry` (`DefinedSpeakerOutput`, `ReverbSendPercent 0`).

  The release puts reference voice samples under the engine's voice-path convention (`Sound/Voice/SkyrimNet.esp/creature_skeleton/skeleton.wav`, `…/creature_flame_atronach/…`). User-posted logs show TTS PCM played through engine `BSSoundHandle`s with native output models. Lip-sync appears to be driven as a viseme stream rather than from `.lip` files (INFERENCE from user reports). SN06 §1.6–1.7 has the evidence. **INFERENCE:** Skyrim's sound-descriptor, category and output-model records let a mod's *runtime-generated* audio inherit the game's mixer, menus and 3D attenuation without inventing an audio stack.

### 5.4 Where SkyrimNet attaches to dialogue

SkyrimNet attaches at four points (detail in SN06 §5–6). Points 2–4 are FACT from `DEV/docs/FEATURES.md` ("Your picks, voiced") and `LIB/prompts/components/dialogue_action.prompt` (the `TriggerDialogueLine` action). Point 1 is INFERENCE from user-posted `SkyrimNet.log` lines in #470 (`TopicInfoEventHandler: …`, plus the reporter's own reading that it handles `TESTopicInfoEvent`).

1. It captures vanilla lines as events through a topic-info event handler (#470).
2. It holds the player's dialogue-menu pick in the DLL to voice it (PR #772).
3. It voices unvoiced lines in the actor's voice type.
4. It fires vanilla infos.

All of this works because each line is a discrete, conditioned, addressable record.

---

## 6. The Papyrus VM

### 6.1 Model

BACKGROUND, confirmed by FACT where noted:

- **Attachment.** Scripts are compiled to `.pex` and attached to forms through a VM adapter (VMAD). Properties are filled with FormKeys at edit time. FACT: SkyrimNet's quest attaches `skynet_Library`, `skynet_MainController` and `skynet_MinAIBridge`. A `PlayerAlias` runs `skynet_PlayerAlias` (`ESP/Quests/skynet_MainController…json`).
- **Events.** Events are methods on those scripts:
  - `OnInit` (once per object lifetime) and `OnPlayerLoadGame`
  - `OnKeyDown`/`OnKeyUp` after `RegisterForKey`
  - `OnEffectStart` on magic effects (SkyrimNet's "spells as hotkeys")
  - SKSE ModEvents after `RegisterForModEvent`
  - MCM callbacks (`SKI_ConfigBase`)

  FACT: these are nearly all the event types SkyrimNet's scripts implement (`SRC/*.psc`). The rest are `OnEffectFinish` and one `OnActivate` (`SkyrimNet_agDebugLever01.psc`). VRIK gesture input (`skynet_vrik_*`), the MinAI bridge (`MinAI_*`) and the legacy text-entry menu (`UITextEntryMenu_*`) all arrive as ModEvents (`skynet_Library.psc`, `skynet_MinAIBridge.psc`, `uitextentrymenu.psc`). No script listens to a combat, hit, death or topic event. Those are consumed natively (§7.3).
- **Natives** are declared in `.psc` and bound by a DLL at load. FACT, Beta27 changelog: `TriggerToggleGlobalAI` existed in the DLL, but "the script never declared it, so the game refused to bind it and logged an error on every load" (`GH/changelog/beta27/papyrus-toggle-global-ai.md`). The binding is by declared signature. FACT (`SKSE/skse64/PluginAPI.h`): a DLL registers its natives through `SKSEPapyrusInterface::Register(callback)`. SKSE calls the callback with the VM, and the plugin binds each `Global Native` function there.
- **Compiling.** FACT: SkyrimNet compiles against **trimmed headers**, not full sources (`papyrus.exe -nocache -h headers -i Source/Scripts`, `GH/CONTRIBUTING.md`). The 89 headers cover vanilla scripts and third-party APIs: `PO3_SKSEFunctions`, `StorageUtil`/`ActorUtil` (PapyrusUtil), `UIExtensions`, `SKI_ConfigBase` (SkyUI), `VRIK`. They are stubs, for example `ActorUtil.psc` has 7 lines and `VoiceType.psc` 2.

### 6.2 Threading and the frame budget

- **One lock per object.** WEB and BACKGROUND: each script object admits one running call at a time; other calls queue. Calling a **latent** function such as `Wait()` releases the lock: "you unlock your script so that other waiting calls can now execute (and therefore possibly cause your variable values to change)" (https://falloutck.uesp.net/wiki/Latent_Functions; same VM family).
- **Frame-synced natives.** Most native calls synchronize with the game's framerate. Only a list of "non-delayed" natives does not (https://ck.uesp.net/wiki/Category:Non-delayed_Native_Function, snippet).
- **The budget.** WEB: the VM gets `fUpdateBudgetMS`, default about 1.2 ms per frame. Guides warn that raising it costs FPS (https://wiki.step-project.com/Guide:Skyrim_INI/Papyrus, snippet). BACKGROUND: under load, calls queue up ("script lag"), and stacks persist in saves.
- **Hot events.** FACT, SkyrimNet's guidance: "`OnItemRemoved` and similar fire constantly; return on the cheapest check first" (ExamplePlugin `docs/best-practices.md`).

### 6.3 Why SkyrimNet keeps Papyrus thin

FACT:

- **Call counts.** SkyrimNet's 21 scripts make about 80 `Debug.Trace` calls. All 133 `StorageUtil` calls are in the Real Names patch. The rest is 14 `GetFormFromFile`, a handful of `ModEvent`, 4 `PO3.SetLinkedRef` and a few `UI.Invoke*` (my count over `SRC/*.psc`). None of the perception, prompt, memory or LLM logic is in Papyrus (SN05 §10).
- **The LLM call from Papyrus is asynchronous.** `SendCustomPromptToLLM` returns "1 on success (task queued)", then invokes a callback function on a quest script. The response is "capped at 750 characters" (`SRC/SkyrimNetApi.psc`). Blocking calls are labelled as such: `PurgeDialogue` is "a blocking call on the Papyrus thread".
- **Papyrus decorators are discouraged:** "A Papyrus decorator is a VM round trip on every prompt render, on the render's critical path". The guide prefers StorageUtil keys read natively through `papyrus_util(...)` (ExamplePlugin `best-practices.md`).
- **The native side is in-process and never blocks the frame.** FACT (`DEV/docs/FEATURES.md`): "Game state is read straight from memory, with no syncing between processes". "The game thread never waits on a network request, a database query or a TTS render". Local Piper TTS runs "in the game's process (CPU)".

**INFERENCE:** in Skyrim, Papyrus earns its keep for three things:

1. Engine verbs that only exist as script functions or records: packages, `SetLookAt`, message boxes, spells as input.
2. The integration bus with other mods: ModEvents, quest-script calls from YAML actions (`WORKFLOW_ACTIONS.md`).
3. In-fiction UI (MCM).

Everything with latency or volume runs native. **Elden Ring has no equivalent VM, so EldenNet loses (1) and (2) and keeps only the native half.**

---

## 7. SKSE and CommonLibSSE-NG

### 7.1 Loading and lifecycle

- **How a plugin declares itself.** FACT (`SKSE/skse64/PluginAPI.h`): since AE, a plugin exports `SKSEPlugin_Version` data and SKSE decides whether to load it. The data lists compatible runtimes, or flags such as `kVersionIndependent_AddressLibraryPostAE`, `kVersionIndependent_Signatures`, `kVersionIndependent_StructsPost629`, and for 1.7.99+ `kVersionIndependentEx_AddressLibraryV5`. It then calls `SKSEPlugin_Load`.
- **CommonLib-NG.** FACT: it generates this boilerplate and builds "a single DLL that works in any Skyrim runtime", SE, AE and VR (`CL/README.md`). SkyrimNet ships CommonLibSSE NG (`DATA/THIRD_PARTY_NOTICES.txt`), one DLL, and an `IsRunningVR()` native.
- **Lifecycle messages.** FACT (`CL/SKSE/Interfaces.h: MessagingInterface`):
  - `kPostLoad`, `kPostPostLoad`, `kInputLoaded`
  - `kDataLoaded` ("after the data handler has loaded all its forms")
  - `kNewGame`, `kPreLoadGame`, `kPostLoadGame`, `kSaveGame`, `kDeleteGame`

  There are also dispatchers for ModEvent, camera, crosshair, action and NiNode-update events.
- **SkyrimNet's lifecycle contract.** FACT: other plugins resolve SkyrimNet's API "once during kDataLoaded". "Data queries return empty results until a save is loaded (check `PublicIsMemorySystemReady()`)" (`DEV/CppAPI/PublicAPI.h`).
- **INFERENCE:** SkyrimNet runs two lifecycles. Forms are ready at `kDataLoaded`. The *playthrough*, meaning the DB, identity and timeline, is ready only after a game loads or starts.

### 7.2 Hooks and trampolines

- **The tools.** FACT: SKSE offers a trampoline interface and a branch trampoline for `write_call`/`write_branch` patches (`CL/SKSE/Trampoline.h`, `PluginAPI.h`). CommonLib adds `REL::Relocation` and vtable helpers (`RelocateVirtual`).
- **SkyrimNet's patches.** FACT: SkyrimNet bundles **MinHook** and **xbyak** (notices). Its public issues describe native patches:
  - in `BSSoundHandle` code: "a return address … sits in the same `BSSoundHandle` code region that SkyrimNet patches" (#831);
  - for dialogue output models;
  - for subtitles and the dialogue menu (SN06 §6).
- **INFERENCE:** hooks are the escape hatch where no event or script API exists, and also the main source of cross-mod fragility.

### 7.3 Event sinks

- **How dispatch works.** FACT (`CL/RE/B/BSTEvent.h`): `BSTEventSource<T>::SendEvent` takes the source's spin lock and calls each sink's `ProcessEvent` **synchronously on the sending thread**. A sink may return `kStop` to halt propagation. `ScriptEventSourceHolder` exposes 53 TES event sources. Examples: activate, active-effect apply/remove, actor location change, cell attach/detach, combat, container changed, death, enter bleedout, equip, hit, magic effect apply, object loaded, package, quest init/stage/start-stop, scene, sleep, spell cast, **topic info** and wait.
- **What SkyrimNet builds on them.** FACT: event types `dialogue`, `combat`, `death`, `hit`, `activation`, `spell_cast`, `equip` (`PublicAPI.h`), and `mod_event` for any ModEvent (SN05 §2).
- **INFERENCE, the rule:** sinks run on the game thread inside the engine's dispatch, so they must only copy data and enqueue it. Their own callbacks are explicitly moved to the worker pool: "Called on ThreadPool — do NOT call RE:: functions directly" (`PublicAPI.h`).

### 7.4 Game-thread work: the task interface

- **The interface.** FACT (`CL/SKSE/Interfaces.h: TaskInterface`): `AddTask(std::function<void()>)` and `AddUITask(...)` queue work that SKSE runs on the main or UI thread.
- **SkyrimNet's thread contract.** FACT, its guidance for extension authors (ExamplePlugin `best-practices.md`):

  | Callback | Thread | Rule |
  |---|---|---|
  | Action execute | game thread | "Change the game here, quickly. No I/O, no queries" |
  | Decorators and eligibility | SkyrimNet workers | "Read only, fast, thread-safe" |
  | DB and file I/O | your own worker | "Never on the game thread" |

  It adds: "Capture form ids, not `RE::Actor*`, in anything that runs later".
- **INFERENCE:** that is the snapshot pattern in one sentence. Its documented exception, decorators reading live state from workers, is where the CTDs cluster (SN01 §1.3, §12).

### 7.5 Co-save serialization

- **The interface.** FACT (`SKSE/PluginAPI.h: SKSESerializationInterface`): a plugin gets a unique id and **save, load, revert and form-delete callbacks**. It writes typed, versioned records (`WriteRecord(type, version, buf, len)`), reads them back in order, and must translate stored FormIDs and handles through `ResolveFormId` and `ResolveHandle`, because load order may have changed. SKSE stores these in a `.skse` co-save next to the `.ess`. WEB: deleting it "may break other SKSE modifications" (https://fileinfo.com/extension/skse).
- **PapyrusUtil.** BACKGROUND: PapyrusUtil's `StorageUtil` (key-value per form) persists through this co-save. SkyrimNet's Real Names patch uses it (`SRC/RealNamesChange.psc`).
- **SkyrimNet's choice.** FACT: it keeps memories in its **own per-playthrough SQLite**. Its playthrough id "persists across save files within the same playthrough" (`SkyrimNetApi.psc: GetSaveUniqueID`). A timeline check prompts the user after loading an older save (SN04 §11). **INFERENCE:** the id itself is probably stored in the co-save or the save; the docs do not say where. A large, queryable, embedding-indexed store does not belong in a co-save blob.

### 7.6 Address Library and version independence

- **The problem.** BACKGROUND: every game patch moves functions and globals. Hard-coded addresses break all native plugins.
- **The Address Library.** WEB: it is a per-runtime database mapping **stable numeric IDs to offsets**, so "different version databases will have the same ID for an address but it may point to different values" (https://nexusmods.com/skyrimspecialedition/mods/32444, mirror snippets).
- **How CommonLib-NG loads it.** FACT (`CL/REL/ID.h`):
  - SE: `Data/SKSE/Plugins/version-<ver>.bin`
  - AE: `versionlib-<ver>.bin`
  - VR: a CSV, `version-<ver>.csv`

  `REL::RelocationID(seID, aeID[, vrID])` picks the right one at runtime.
- **Layout shifts.** FACT: layout changes are absorbed by accessors. For example, `Actor::GetActorRuntimeData()` returns `RelocateMemberIfNewer(SKSE::RUNTIME_SSE_1_6_629, this, 0xE0, 0xE8)` (`CL/RE/A/Actor.h`).
- **Skyrim is still being patched.** FACT (`SKSE/skse64_common/skse_version.h`): runtimes listed include 1.5.97, 1.6.318, 1.6.629/640, 1.6.1130 ("creations patch"), 1.6.1170, **1.7.99 ("creations") and 1.7.104 (hotfix)**. `CURRENT_RELEASE_RUNTIME` is 1.7.104, with Address Library "V5" encoding for 1.7.99+.
- **Crash logs.** FACT: they resolve engine frames to Address Library IDs (#819: "ID 14833+0x58"). Stable IDs are the shared language between users, crash loggers and plugin authors.

### 7.7 The dependency stack: what each community framework fills

FACT (`GH/README.md`, Requirements). Each dependency covers a gap in the engine or the vanilla script API:

| Dependency | Gap it fills (INFERENCE where not stated) |
|---|---|
| SKSE | native plugin loading, Papyrus natives, co-save, task queue (§7.1–7.5) |
| Address Library for SKSE Plugins | version-independent addresses (§7.6) |
| powerofthree's Papyrus Extender | extra Papyrus natives; SkyrimNet uses `SetLinkedRef` (§4.1) and `IsGeneratedForm` is declared (`HDR/PO3_SKSEFunctions.psc`) |
| PapyrusUtil SE | `StorageUtil` per-form storage and `ActorUtil` package overrides (§4.1, §7.5) |
| powerofthree's Tweaks, with `Load EditorIDs` on | EditorIDs at runtime (§2.4) |
| Prisma UI plus Media Keys Fix SKSE, or Magelight UI | web-view overlay for chat and dashboard (§8) |
| Visual C++ Redistributable; "a CPU with AVX2" | runtime for the DLL; AVX2 is presumably for local inference (Whisper, Piper, embeddings) (INFERENCE) |
| Optional: UIExtensions; CUDA 12.x | input wheel; faster local speech recognition |
| SE without ESL support: BEES. VR: Skyrim VR ESL Support, VR Refocused, MFG Fix NG | plugin-format backports; window focus; facial-expression fixes for VR |

**INFERENCE:** a "minimal" Skyrim AI mod already stands on about six community frameworks. SkyrimNet's own Papyrus layer is small (§6.3) because these frameworks expose the engine verbs it needs.

**How other native mods call SkyrimNet.** FACT (`DEV/CppAPI/PublicAPI.h`, MIT):

- It does not use SKSE's messaging interface for its API. Consumers `LoadLibraryA("SkyrimNet")` and resolve exported `Public*` functions with `GetProcAddress` in `FindFunctions()`, "once during kDataLoaded".
- Functions are gated by `PublicGetVersion()`: "Functions from newer API versions stay nullptr when the installed SkyrimNet is older".
- Because `std::string` and `std::function` cross the DLL boundary, consumers must "build with the same MSVC toolset as SkyrimNet (Visual Studio 2022, v143)". SkyrimNet links the static CRT (`/MT`).
- Data queries are thread-safe and return empty results until the database is ready. Action registration "should be done during plugin initialization only".

**INFERENCE:** a C++ ABI across DLLs is convenient but brittle. A C ABI (plain `const char*` and JSON) would avoid the toolset lock-in, which matters for an Elden Ring framework whose ecosystem mixes Rust and C++.

---

## 8. UI: Scaleform, MCM, Prisma UI

- **Vanilla UI.** BACKGROUND and WEB: menus are **Scaleform GFx** movies (SWF, close to ActionScript 2) loaded from `Interface/`, and loose SWFs can override them (Infinity UI page and Nexus forum snippets, https://www.nexusmods.com/skyrimspecialedition/mods/74483). BACKGROUND: SkyUI replaces many menus and provides the **MCM** (Mod Configuration Menu) framework. SKSE lets Papyrus and C++ open custom menus and invoke ActionScript.
- **SkyrimNet's Scaleform-era pieces.** FACT:
  - a custom text-entry menu (`UI.OpenCustomMenu("textentrymenu")`, `UI.InvokeString`; `SRC/uitextentrymenu.psc`). It is now legacy: `SkyrimNetInternal.psc: GetPlayerInput` logs "Papyrus text input removed, use PrismaUI"
  - the MCM (`skynet_McmScript extends SKI_ConfigBase`)
  - a UIExtensions wheel (`skynet_WheelMenu.psc`)
  - vanilla **Message** forms for modal prompts such as the timeline keep/delete question (`SkyrimNetInternal.psc: ClearTimelineMessage`)
- **Prisma UI.** FACT: SkyrimNet's chat, dashboard and subtitles run in Prisma UI views (`PrismaUI/views/SkyrimNet/{chat,dashboard,subtitles}`), or in Magelight UI. Prisma UI is a separate SKSE framework, "Skyrim Next-Gen Web UI Framework". It "utilizes the Ultralight SDK for rendering web content" (https://github.com/PrismaUI-SKSE/framework, README). SkyrimNet also serves the same dashboard at `localhost:8080`.
- **INFERENCE:** browser-tech UI let SkyrimNet share one UI between the game overlay and the desktop browser. Scaleform remains only where a vanilla-looking modal is wanted.
- **The renderer as a sensor (OmniSight).** FACT (`DEV/docs/FEATURES.md`, Vision): SkyrimNet "can take screenshots of actors, items, locations, furniture and the current scene" and asks a vision model to describe them. "Items render on transparent backgrounds at high resolution", and identical items are described only once. The descriptions are stored in SQLite (migrations `0007_omnisight.sql`, `0009`–`0011`, `0031`). **INFERENCE:** this needs native access to the render pipeline, the most engine-specific feature in SkyrimNet. It also stands in for what the record database cannot say, which is what a thing *looks like*.

---

## 9. Saves

- **What a save holds.** BACKGROUND and WEB: a `.ess` holds the header, the plugin list, **change forms** (deltas against plugin records: moved references, changed inventories, said-once flags, quest stages) and the **Papyrus VM state** (script instances, variables, registrations, suspended stacks). It is compressed with zlib or LZ4, and FormIDs are stored as 3-byte ref ids (https://en.uesp.net/wiki/Skyrim_Mod:Save_File_Format, snippet).
- **Two consequences for an AI mod:**
  1. **Script state outlives code.** FACT: SkyrimNet must still call `UnregisterForModEvent("PlayDBVOTopic")` on every load, because "Saves made before the dialogue voice moved into the DLL still carry this registration" (`SRC/skynet_Library.psc: Maintenance`). Its `Maintenance()` reruns registration on every `OnPlayerLoadGame`, because `OnInit` runs once per object lifetime (`skynet_PlayerAlias.psc`, `skynet_MainController.psc`). The quest is `StartGameEnabled` + `RunOnce` (`ESP/`).
  2. **Saves branch.** Players reload older saves, so an external store gets "future" rows. FACT: SkyrimNet's `TimelineState` API and the keep/delete prompt exist for this (SN04 §11).

---

## 10. Game design: Skyrim as a conversation game

- **Scale.** WEB: Skyrim passed 1000 named characters "by just 16 people" (UESP blog snippet, https://blog.uesp.net/?p=1828).
- **A city that lives on a schedule.** Radiant AI's schedules and sandboxing make cities feel inhabited. Radiant Story generates side quests around named NPCs (WEB: https://pt.uesp.net/wiki/Skyrim:Radiant). Factions (guilds, holds, the civil war) give each NPC an allegiance that conditions what they say.
- **Conversation is a core verb.** Quests start, branch and finish in dialogue. Services are dialogue: merchants, trainers, carriage, innkeeper, followers and marriage. FACT: these are the exact categories SkyrimNet uses to classify vanilla lines (`CONTENT_ROOTS.md`: quest, follower, merchant, trainer, carriage, innkeeper, bard, marriage, crime, other). The quest **journal** and quest **stages** are an explicit, queryable story state: `get_quest_stage("MQ104") >= 13` gates world knowledge (`DEV/docs/FEATURES.md`).
- **The player is the Dragonborn,** a named role the world reacts to. FACT: SkyrimNet had to fix about 50 bios that called the player "the Dragonborn" before they are one (`GH/changelog/beta27/player-not-assumed-dragonborn.md`). Story state must gate identity claims.
- **The design implication (INFERENCE).** Skyrim's NPCs already behave as social beings: schedules, homes, factions, relationships, crime reactions, generic barks. SkyrimNet's job is to *replace the words* while letting the engine keep doing the living. Elden Ring's NPCs are fewer, mostly stationary, laconic and quest-flag-driven, and its enemies are amnesiac respawns (ER04 §4.1). An Elden Ring mod has to *supply* more of the "being alive" through text, and must keep generics deliberately simple.

---

## 11. How it maps onto Skyrim engine features

SkyrimNet's reliance on the engine, layered by how hard the dependency is (INFERENCE from the evidence above):

| Layer | Engine features | What would break without it |
|---|---|---|
| **Hard: loading and memory access** | SKSE loader, Address Library, CommonLibSSE-NG structs, MinHook/xbyak patches | everything |
| **Hard: identity** | FormIDs, plugin and local id resolution, base vs reference, EditorIDs (po3 Tweaks) | memories, bios and content keys |
| **Hard: observation** | `BSTEventSource` sinks (hit, death, combat, equip, spell, activate, topic info), lifecycle messages, ModEvents | the event log and reactions |
| **Hard: perception** | process lists or loaded actors, positions, LOS, cell and location data | audiences and "nearby" lists |
| **Medium: presentation** | voice types, sound categories and output models, subtitle manager, facial morphs, Prisma UI | voiced, lip-synced speech (text would survive) |
| **Medium: body language** | AI packages, linked refs (PO3), `SetLookAt`, `EvaluatePackage` | NPCs stopping and facing the speaker |
| **Soft: story context** | quests, stages, aliases, journal, globals, factions, relationship ranks, crime gold | richer, more grounded prompts |
| **Soft: integration** | Papyrus API, ModEvents, quest-script calls, MCM, spells as hotkeys | the mod ecosystem (SeverActions, IntelEngine) |

For EldenNet v1 only the first four layers and the text part of presentation are needed. Elden Ring lacks most of the *soft* layers outright, so the design must not depend on them.

---

## 12. Transferable patterns for an Elden Ring framework

### 12.1 Copy as an idea

1. **Store load-order-proof identities, never runtime handles.** Skyrim's lesson is `Plugin|localId` over runtime FormIDs (§2.2). The Elden Ring equivalent is a stable key:
   - the three-digit **talk code** and `NpcParam` row for named NPCs (ER04 §1);
   - `(event_entity_id, npc_param_id, BlockId)` for placed characters (ER01 §5.4);
   - never `ChrIns*` or `FieldInsHandle`.

   *Why:* identity must survive patches, mods and sessions, exactly as SkyrimNet's migration 0020 learned.
2. **Separate "base" from "instance" in the data model.** Skyrim's `TESNPC` vs `Actor` maps to ER's `NpcParam` and `CharaInitParam` vs `ChrIns`. Key personas on the base for named NPCs. Key generics by archetype (NpcParam, model `cXXXX`, `team_type`) with **no per-instance memory**.

   *Why:* Elden Ring generics respawn on rest (ER04 §4.1). Skyrim's one-UUID-per-bandit approach would create thousands of amnesiac identities.
3. **Use a bark taxonomy modeled on Skyrim's topic subtypes** (§5.2). For v1:
   - combat transitions: NormalToCombat, CombatToLost, LostIdle
   - combat actions: Hit (taken), Attack, Bleedout ("near death"), AllyKilled
   - awareness and social: NoticeCorpse, Taunt, Flee, Hello/Goodbye for named NPCs

   Detect them from snapshot diffs: HP deltas, `last_hit_by`, the dead bit, lock-on, distance. *Why:* it is a game-tested list of situations worth a line. It gives a fixed key for cooldowns and prompt variants, and it maps directly onto ER signals.
4. **Treat engine callbacks as "copy and enqueue" only.** Skyrim sinks fire synchronously inside engine dispatch (§7.3). In Elden Ring the equivalent is a `CSTaskImp` recurring task in a late frame phase, which snapshots and diffs (ER01 §3.2). Workers never touch game memory. *Why:* SkyrimNet's CTD history (#819, #830, #831) is exactly the cost of not doing this.
5. **Two lifecycles: "data ready" and "playthrough ready".** Skyrim has `kDataLoaded` vs `kPostLoadGame`/`kNewGame`. In Elden Ring, wait for system init and the param repository, then a separate "character loaded" state: `GameMan.save_slot`, `PlayerGameData` present. The memory store opens only on the second. *Why:* SkyrimNet's "queries empty until a save is loaded" contract avoids a whole class of bugs.
6. **An external store plus a playthrough fingerprint and timeline check, not save-embedded data.** Skyrim has a co-save and `GetSaveUniqueID`. Elden Ring has a single autosaving `.sl2` per slot, so key a sidecar store by SteamID + slot + character fingerprint (ER01 §11).
7. **A human-readable key layer over numeric ids.** Skyrim needs po3 Tweaks to get EditorIDs back. For Elden Ring, ship a small curated name table (talk code → NPC key, `team_type` → faction label, BlockId → region name) as data. *Why:* content authors and prompts need words, not ids.
8. **Words from structured state.** Skyrim decorators turn relationship rank, crime gold, faction and location into text. Elden Ring's equivalents are:
   - per-NPC quest-state flag blocks (3000–4999) rendered as stage names;
   - hostility bits (+1/+2) as "hostile toward you";
   - PlaceName and BlockId as "where".

   Use a narrow, versioned flag list.
9. **Design three identity layers from day one: handle, record, character.** Skyrim needed `identity/*.yaml` successions and migration 0021 aliases after the fact (§2.5). Elden Ring has the same problem. "NPCs move, vanish and die between stages" (ER04 §4.4), and one character can have several `NpcParam` rows, e.g. Boc's human form uses code 342 while his talk code is 223 (ER04 §1, §8). Ship a curated **succession table** as data (character key → all NpcParam variants and placements), as ER04 §8 already proposes with `npc_identity.toml`. Make the stable character key, not a name hash, the primary key of memory. *Why:* a name-hashed id forks on every rename, and SkyrimNet's fix is an alias union at recall time, which is complexity v1 can avoid.
10. **Eligibility filters as data lists over engine ids.** Skyrim uses race, faction and gender EditorID lists ("animals don't speak", "mannequins don't speak") (§2.4). For Elden Ring, ship YAML lists over model ids (`cXXXX`), NpcParam ids and `team_type`. They decide who may bark, who may converse and who is silent: animals, spirits, invisible dummies. *Why:* it keeps the generic-NPC tier deliberately narrow, and users can tune it without code.
11. **A C ABI for the extension API.** SkyrimNet's C++ API passes `std::string`/`std::function` across DLLs and so locks consumers to MSVC v143 with a matching CRT (§7.7). For EldenNet, export `extern "C"` functions with UTF-8 `const char*` and JSON, plus a version query. *Why:* the ER modding ecosystem is part Rust (fromsoftware-rs, me3), and a C ABI is the only boundary both sides share.

### 12.2 Simplify for v1

- **Perception.** Use Skyrim's *idea* (radius plus LOS) without the engine's detection model. In Elden Ring: `chr_inses_by_distance` within a radius, an optional Havok raycast later, and lock-on as the strongest "addressed" signal.
- **"Where am I".** Skyrim has worldspace, cell, location and named places. For Elden Ring use the region and PlaceName string plus "legacy dungeon vs overworld" (map area < 50 vs 50–88; ER01 §7). Skip sub-room places.
- **Story state.** Skyrim has quest stages, a journal and globals. For Elden Ring use a curated list of event flags per named NPC, polled about once a second, not a flag-set hook (ER01 §9).
- **UI.** Skyrim ended up with Prisma UI (web views). For Elden Ring, an ImGui overlay through hudhook is enough for chat and subtitles (ER02 §7). A browser dashboard on localhost can come later.

### 12.3 Drop for v1, and why

- **Anything resembling AI packages, linked refs or `EvaluatePackage`.** ER has no modder package system. Behaviour is Lua goals, think params and EMEVD (ER01 §6). Dialogue-only v1 needs at most look-at and facing through ESD talk commands, and even that is optional.
- **A scripting bridge** (Papyrus API, ModEvents, quest-script actions). ER has no runtime VM for mods. A C ABI and a file or JSON API can come later if other mods appear.
- **Engine-native voice integration** (voice types, `.fuz`, output models). ER audio is Wwise banks, and rebuilding them is patch-fragile (ER03 TL;DR). v1 is text first, with optional TTS through our own audio path.
- **Relationship ranks, crime and bounty, follower frameworks, the MCM.** These have no Elden Ring analogue. Model "standing" as flags plus our own memory.

---

## 13. Open questions / uncertainties

1. **Not re-verified this session:** UESP and CK wiki details for cell size, process-level ranges, the condition-function list, `.ess` layout and Shared Info semantics. These are tagged BACKGROUND or WEB (search snippets). Re-check before quoting in design docs.
2. **How SkyrimNet builds its "nearby" list.** High-process list, cell scan or loaded references? Undocumented. The CTD reports suggest workers reading actor data directly.
3. **Whether `available_dialogue` evaluates the engine's own conditions** (e.g. through `TESCondition::IsTrue`) or reimplements them. INFERENCE only.
4. **Where `GetSaveUniqueID` is stored:** co-save, `.ess`, or derived from both. The docs only say it persists across saves of one playthrough.
5. **Whether `SkyrimNet.esp` is ESL-flagged.** The Spriggit header shows no flag. Ids up to `0x12F5` argue against it. The BEES requirement suggests a header-1.71 plugin (§2.1).
6. **Skyrim 1.7.x support.** SKSE lists 1.7.99/1.7.104 and an Address Library V5 encoding. Whether SkyrimNet beta30 supports 1.7.x was not found in its readable materials. The issues seen report 1.6.1170.
7. **Which ESP package backs the friendly names `TalkToPlayer`, `TalkToNPC` and `FollowPlayer`.** The ESP has both `SkyrimNet_PlayerDialoguePackage` (`0x3`) and `SkyrimNet_TalkToPlayerPackage` (`0xE9D`, built from the follow template, with an `IsPlayerMovingIntoNewSpace` condition). The mapping lives in the DLL and is undocumented.
8. **What "masked FormID" means in the UUID hash** (migration 0021). It may be the same `& 0xFFF` mask as the bio file names (`lydia_C94`). If so, two different plugins' NPCs with the same name and low 12 bits would collide unless `mod_file` disambiguates. UNVERIFIED.
9. **Elden Ring analogues marked TBD or LIKELY** in the table depend on the ER track. Open items: the "interior" equivalent, a runtime AI-state read for alert/combat/lost, how many generics have `event_entity_id == 0`, and the NG+ cycle counter (ER01 open questions).

---

## 14. Summary table: Skyrim engine feature → how SkyrimNet uses it → Elden Ring analogue

| Skyrim engine feature | How SkyrimNet uses it (evidence) | Elden Ring analogue |
|---|---|---|
| Plugins (ESM/ESP/ESL), load order | Ships one full-slot `SkyrimNet.esp` (master `Skyrim.esm`); `is_plugin_loaded`, `GetModByName==255` checks (§2.1) | None for mods. `regulation.bin` params are a single file (last wins). me3 profiles load DLLs. EldenNet ships no data files (ER01 §8.1, ER02) |
| FormID = load index + local id; `0xFE` light; `0xFF` runtime | Persists `Plugin.esp\|0xLocalID` and `npc:Plugin.esp:0x…`; migration 0020; bios keyed name + `FormID&0xFFF` (§2.2) | Param row ids (`NpcParam`), MSB `event_entity_id`, `BlockId`; the three-digit talk code as canonical NPC key (ER01 §5.4, ER04 §1) |
| Base object vs reference (`TESNPC` vs `Actor`) | Player base `0x7` in alias, player ref `0x14` as package target; unique vs generic bios; UUID per actor (§2.3) | `NpcParam`/`CharaInitParam` (base) vs `ChrIns` (runtime) vs MSB part (placement) (ER01 §5) |
| Race, Class, AIData, VoiceType, flags | `decnpc` (race, class, level, skills); filters by race; `SilentVoice` voice type (§2.4) | `character_id` model (`cXXXX`, humanoids all `c0000`), `NpcParam.npcType`, `teamType`, `soundBankId` (ER01 §5.4, §8.1) |
| Factions, ranks, crime factions | `is_in_faction`, `get_faction_rank`, `get_crime_gold`; whitelist/blacklist factions in ESP (§4.4) | `team_type` (runtime) for hostility; "faction" only as our archetype label; no crime (ER01 §6, ER04 §2.7) |
| Relationship ranks (Lover…Archnemesis) | `relationshipRank` −4…4 in `decnpc`; `get_relationship_rank` decorator (§4.4) | None. Per-NPC hostility flags (+1 absolvable, +2 permanent) and our own memory (ER04 §4.5) |
| Keywords | Linked-ref keys for packages; `actor_has_keyword`, `worn_has_keyword` (§2.4) | None as such. SpEffect lists act as state tags (ER01 §5.2). TBD |
| EditorIDs (mostly unloaded; po3 Tweaks) | Required dependency; decorators and filters take EditorIDs (§2.4) | None at runtime. Paramdex row names and dev flag labels offline; ship our own name table (ER04 §1) |
| Worldspace / cell / interior / `BGSLocation` | Location string, `location_building`, `is_indoors`, named places per interior cell or worldspace (§3) | Map ids `mAA_BB_CC_DD`, `BlockId`, m60/m61 tiles, PlaceName via `map_place_name_id` (ER01 §7). Interior flag TBD (legacy dungeon as proxy) |
| Loaded area, 3D attach, AI process levels | Nearby lists, audiences; CTDs at cell transitions (#819) (§3) | Loaded `ChrSet`s, `chr_inses_by_distance`, `load_state` / `omission_mode` (ER01 §5) |
| AI packages and procedures, schedules, sandbox | TalkToPlayer/TalkToNPC/Follow packages with conditions; `EvaluatePackage`; ActorUtil overrides (§4.1) | Lua battle/logic goals, `NpcThinkParam`, EMEVD AI-state commands; no modder package system. Drop for v1 (ER01 §6) |
| Combat AI, combat styles, combat state | `is_in_combat`, `get_combat_targets`, actor values; combat events as short-lived context (§4.2) | `ChrIns` HP/FP, `last_hit_by`, dead bit, `is_locked_on`; Lua goals; AI alert state TBD (ER01 §5.2, §6) |
| Detection, LOS | `has_line_of_sight`; radius + LOS audience; detection not used (§4.3) | `NpcThinkParam` eye/ear distances; `CSTargetSearchSys`; Havok raycast RVA (ER01 §5.2, §6) |
| Followers (DialogueFollower alias, follower factions) | `get_quest_alias_actor`, `is_follower`; packages avoid Follow/Escort/Accompany (§4.5) | Spirit ashes, NPC summon signs (`WhiteSummonNpc`), Torrent (ER04 §4.7) |
| Dialogue: Quest → Branch → Topic → Info, conditions, scenes | Captures `TESTopicInfoEvent`; `available_dialogue`; fires infos; categories quest/merchant/… (§5.1, §5.4) | Talk ESD state machines (`t<talkId>.esd`), `TalkParam` rows, TalkMsg FMG, menus via `AddTalkListData` (ER01 §10, ER03 §2) |
| Topic subtypes as barks (Hit, Death, Taunt, NormalToCombat, …) | Not generated per subtype; grunts filtered; combat via normal pipeline (§5.2) | Few native barks: some talk-ESD lines, EMEVD `PlaySE`, TalkMsg chants; generic enemies have no talk ESD. EldenNet's own bark taxonomy (ER03 §6, ER04 §2.8) |
| Voice types, `.fuz` (= `.xwm` + `.lip`), facial lip-sync | TTS per voice type; vanilla samples for cloning; `SilentVoice` (§5.3) | Wwise `vcXXX` banks, `Play_v<id>` events; mouth-flap only (`lipSyncStart/Time`) (ER03 TL;DR, §4) |
| Subtitle manager | Rewrites vanilla subtitles; own Prisma UI subtitles (§5.3) | TalkMsg through a `GetMessage` hook (v2), or own ImGui overlay (v1) (ER01 §8.3, ER03 §7) |
| Papyrus VM (scripts on forms, events, latent calls, ~1.2 ms/frame) | Glue only: packages, look-at, hotkeys, MCM, ModEvents; async LLM callback; "VM round trip" warning (§6) | None for runtime mods. EMEVD, ESD and Lua are authored data; EldenNet is fully native (ER01 §9–10) |
| SKSE loader and plugin version data | One CommonLib-NG DLL for SE/AE/VR; `IsRunningVR` (§7.1) | me3 `[[natives]]` (ModEngine2 discontinued); `wait_for_system_init` (ER01 §3.1, ER02 §3) |
| SKSE messaging (`kDataLoaded`, `kPostLoadGame`, `kNewGame`, `kSaveGame`) | API resolve at `kDataLoaded`; DB only after a save loads (§7.1) | No standard. Our own "system ready" plus "character loaded" detection (`GameMan.save_slot`) (ER01 §11) |
| Hooks/trampolines (SKSE trampoline, MinHook, xbyak) | Sound, subtitle and dialogue-menu patches (§7.2) | MinHook-style hooks with interior-anchored AOBs; coexist with other hookers (ER01 §4) |
| Event sinks (`BSTEventSource<TES…Event>`, 53 sources) | Event log types dialogue, combat, death, hit, activation, spell_cast, equip (§7.3) | None public. Per-frame snapshot diff; optional flag-set hook (TGA) (ER01 §9) |
| Task interface (`AddTask`, `AddUITask`) | "Action execute: game thread"; workers must not call `RE::` (§7.4) | `CSTaskImp::run_recurring` in a `CSTaskGroupIndex` phase (ER01 §3.2) |
| Co-save serialization (`.skse`), PapyrusUtil storage | Not used for memories (own SQLite); playthrough id + timeline check (§7.5) | No co-save. Sidecar store keyed by SteamID + slot + character fingerprint (ER01 §11) |
| Address Library + CommonLib relocation (`RelocationID`, `RelocateMemberIfNewer`) | Version independence across 1.5.97 / 1.6.x / VR (1.7.x unconfirmed); crash IDs in reports (§7.6) | fromsoftware-rs RVA tables per exe, AOB patterns, FD4Singleton finder, RTTI; struct layouts move every patch (ER01 §1, §4) |
| Scaleform menus, SkyUI MCM, Message forms, UIExtensions | Legacy text menu, MCM, wheel, modal timeline prompt (§8) | ER also uses Scaleform for menus (ER01 §2.2); no MCM; own config UI |
| Prisma UI (Ultralight web views) | Chat, dashboard, subtitles; same UI on `localhost:8080` (§8) | ImGui overlay through hudhook on D3D12 (ER02 §7); optional local web dashboard |
| Saves (`.ess` change forms + Papyrus state) | Re-registers on every load; cleans stale ModEvent registrations; timeline keep/delete (§9) | `.sl2` BND4, 10 slots, continuous autosave; event flags inside the slot (ER01 §11) |
| Quest journal, quest stages, globals | `get_quest_stage`, `get_quest_journal`, knowledge gated on stages (§10) | No journal. Per-NPC flag blocks 3000–4999, story flags 100–199, boss flags (ER04 §4.4) |
| Calendar time, weather | `gameTime` ("8:00 AM, Morndas, 17th of Last Seed, 4E 201"), `currentWeather`, `season` (`decorators.html`; `WORKFLOW_PROMPTS.md` §2.2) | `WorldAreaTime` clock (no calendar), `WorldAreaWeather` (ER01 §4) |
| Crosshair target (`GetCurrentCrosshairRef`) | Picks the addressee; wheel actions on the crosshair actor (`SRC/skynet_WheelMenu.psc`); capture-crosshair hotkey (`SkyrimNetApi.TriggerCaptureCrosshairPressed`) | Lock-on target, then forward cone, then nearest (ER03 §7E) |
| One character as several NPC_ records (quest swaps, ghosts) | 12 `identity/*.yaml` successions carry memory between records (§2.5) | Several `NpcParam` variants per character, flag-driven moves; curated `npc_identity.toml` (ER04 §1, §8) |
| Per-reference display names (`SetDisplayName`; Real Names, NND) | Real Names patch and NND ini shipped; name-hashed UUIDs fork on rename, so migration 0021 adds aliases (§2.3, §2.5) | None needed: generics keyed by archetype, named NPCs by curated key; never by display name |
| Race/faction/gender lists as eligibility | `filters/*.yaml` Race/Faction/Gender white- and blacklists ("animals don't speak") (§2.4) | YAML lists over model `cXXXX`, NpcParam ids, `team_type` |
| Sound categories, output models, voice path | 3 SNDC records with menu sliders; `SOMDialogue2DDry`; samples under `Sound/Voice/SkyrimNet.esp/` (§5.3) | Wwise buses; own audio output in v1 (ER03) |
| Render capture (OmniSight) | Screenshots of actors, items, scenes described by a vision model (§8) | D3D12 back-buffer capture through the overlay hook (LIKELY feasible, untested). Drop for v1 |
| Inter-plugin API | `LoadLibraryA` + `GetProcAddress`, version-gated, MSVC-ABI (`PublicAPI.h`) (§7.7) | `extern "C"` + JSON exports, version query |
| Radiant AI/Story, ~1000 named NPCs with schedules | Context that NPCs "live"; SkyrimNet replaces words, not routines (§10) | Few, mostly stationary NPCs; respawning generics; flag-driven quests (ER04 §2, §4) |

---

### Sources (web)

- CommonLibSSE-NG: https://github.com/CharmedBaryon/CommonLibSSE-NG (README; `include/REL/ID.h`, `Relocation.h`; `include/RE/T/TESFile.h`, `TESDataHandler.h`, `TESTopic.h`, `TESTopicInfo.h`, `TESPackage.h`, `TESCombatStyle.h`; `include/RE/A/Actor.h`, `AIProcess.h`; `include/RE/P/ProcessLists.h`; `include/RE/B/BSTEvent.h`, `BGSRelationship.h`; `include/RE/S/ScriptEventSourceHolder.h`, `SubtitleManager.h`; `include/SKSE/Interfaces.h`, `Trampoline.h`)
- SKSE64: https://github.com/ianpatt/skse64 (`skse64/PluginAPI.h`, `skse64_common/skse_version.h`, tags v2.2.x–v2.3.1)
- Prisma UI: https://github.com/PrismaUI-SKSE/framework (README)
- SkyrimNet: https://github.com/MinLL/SkyrimNet-GamePlugin/issues/819, /831, /830, /771, /470 (the last two were added in review); https://github.com/MinLL/SkyrimNet-ExamplePlugin (`docs/best-practices.md`)
- Search-snippet sources (pages not opened, because hosts were unreachable):
  - esplugin changelog https://github.com/Ortham/esplugin/blob/master/CHANGELOG.md
  - LOOT API changelog https://loot-api.readthedocs.io/en/0.22.3/api/changelog.html
  - STEP Papyrus INI guide https://wiki.step-project.com/Guide:Skyrim_INI/Papyrus
  - Fallout 4 CK Latent Functions https://falloutck.uesp.net/wiki/Latent_Functions
  - CK non-delayed natives https://ck.uesp.net/wiki/Category:Non-delayed_Native_Function
  - CK SharedInfo https://ck.uesp.net/wiki/SharedInfo
  - GECK process levels https://geck.uesp.net/wiki/GetActorProcessingLevel
  - uGridsToLoad thread https://forums.nexusmods.com/topic/5068975-ugridstoload-talk-and-testing/page/3
  - Address Library https://nexusmods.com/skyrimspecialedition/mods/32444
  - powerofthree's Tweaks https://nexusmods.com/skyrimspecialedition/mods/51073
  - UESP Save File Format https://en.uesp.net/wiki/Skyrim_Mod:Save_File_Format
  - `.skse` https://fileinfo.com/extension/skse
  - Beyond Skyrim voice lines https://wiki.beyondskyrim.org/wiki/Arcane_University:Voice_Line_Implementation
  - UESP blog https://blog.uesp.net/?p=1828
  - UESP Radiant https://pt.uesp.net/wiki/Skyrim:Radiant
  - Infinity UI https://www.nexusmods.com/skyrimspecialedition/mods/74483

---

## Review notes

Critic pass, 2026-11-06. I re-checked claims against the local materials, CommonLibSSE-NG and SKSE64 on GitHub (raw files, re-downloaded), the SkyrimNet-ExamplePlugin `docs/best-practices.md` (raw file) and GitHub issue pages #470, #771, #819 and #831 (web fetch). UESP and the CK wiki were still unreachable (proxy refused `ck.uesp.net` and `en.uesp.net`). No compiled file was opened, and that includes the binary `SkyrimNet.esp`.

**Spot-checked and confirmed (no change needed):**

- 14 `GetFormFromFile` calls.
- `GetModByName("vrik.esp") == 255`.
- 30 references to `000014:Skyrim.esm` in `ESP/`.
- Player alias `UniqueActor 000007`.
- The `GetCurrentAIPackage != 1/2/7`, `GetSitting` and `IsInScene` conditions and the 512/256 radii on `SkyrimNet_PlayerDialoguePackage`.
- The `CONTENT_ROOTS.md` identity spellings and the "hub refuses" wording.
- Migration 0020 text.
- `SendCustomPromptToLLM` "task queued" and "capped at 750 characters".
- `PurgeDialogue` "blocking call on the Papyrus thread".
- The `PlayDBVOTopic` maintenance comment.
- The beta27 changelog quotes (Dragonborn bios, `TriggerToggleGlobalAI`).
- The CONTRIBUTING compile line.
- 89 headers (`ActorUtil.psc` 7 lines, `VoiceType.psc` 2).
- Decorator call counts: `decnpc(` 292, `is_in_faction(` 37, `worn_has_keyword(` 25, `is_follower(` 12.
- 302 generic bios out of 1,509.
- `units_to_meters`, and `MQ104 >= 13` in `FEATURES.md`.
- All four ExamplePlugin quotes.
- `PublicAPI.h`: kDataLoaded, `PublicIsMemorySystemReady`, the event types, and "Called on ThreadPool — do NOT call RE:: functions directly".
- CommonLib facts:
  - 53 unique `TES*Event` sources in `ScriptEventSourceHolder.h` for SE/AE. VR lacks `TESFastTravelEndEvent`, and `BGSEventProcessedEvent` is a 54th, non-TES source.
  - 103 topic subtypes, with `kCombatGrunt = 101`.
  - `PROCESS_TYPE` 0–3.
  - `RelocateMemberIfNewer(…1_6_629, this, 0xE0, 0xE8)`.
  - Address Library file names in `REL/ID.h`.
  - `kSmallFile = 1 << 9` and the `0xFE000 |` partial index.
  - `BSTEventSource::SendEvent` under a spin lock, with `kStop`.
- SKSE facts: runtimes 1.7.99/1.7.104 with `CURRENT_RELEASE_RUNTIME` 1.7.104 (SKSE 2.3.1); the `kVersionIndependent*` and `kVersionIndependentEx_AddressLibraryV5` flags; `ResolveFormId` and `ResolveHandle`; `AddTask` and `AddUITask`.

**Corrected:**

- **#819 quote** (TL;DR, §3). The report quoted "seconds after a loading screen into Riverwood", which is not the issue's wording. It now quotes the reporter exactly ("loaded into Riverwood, crashed about 4 seconds after the loading screen"; "Both times the crashing thread was rendering character profiles") and adds the runtime (AE 1.6.1170).
- **§2.1 local-id ranges.** "Packages `0x000001`–`0x000009`" was wrong: `0x2` and `0x6` are keywords and `0x9` is a Message. The range is now listed per record type. `0x12F5` is identified as a placed reference in the test cell, and the section notes that the Spriggit header does not expose the header version or flags.
- **§2.2 sources.** The `GetFormFromFile` citations now include `skynet_McmScript.psc` and `skynet_Library.psc`.
- **§4.1 order of operations.** The order in `SetActorDialogueTarget` is `SetLookAt` → `SetLinkedRef` → `RegisterPackage` → `EvaluatePackage`, not register-first. The section now also says that the linked-ref keyword drives the NPC-to-NPC package, while the player packages target `0x14` directly.
- **§4.3 #771.** It was "detection is not a perception gate (#771)". It now states what the issue reports: searching (non-detecting) enemies speak combat lines at the player by name.
- **§4.4 citation.** `get_crime_gold` is not in `decorators.html`. It is cited to the test submodule, with its "only works for player" note. `get_civil_war_side` is in `decorators.html`.
- **§5.4 labels.** The topic-info capture point comes from user-posted `SkyrimNet.log` lines in #470, so it is now labelled INFERENCE. The other three points are cited to `FEATURES.md` and `dialogue_action.prompt`.
- **§6.1 event list.** "All the event types" became "nearly all". `OnEffectFinish` and `OnActivate` are added, and the section notes that VRIK, MinAI and text-entry input arrive as ModEvents and that no script handles combat or topic events.
- **§8.** Added evidence that the text-entry menu is legacy (`GetPlayerInput` logs "Papyrus text input removed, use PrismaUI").

**Added:**

- §2.3: SkyrimNet's own test cell as a base-vs-reference example. Per-reference display names (the Real Names patch's `SetDisplayName`, the shipped `NPCsNamesDistributor.ini`).
- §2.4: engine forms as eligibility keys (Race, Faction and Gender filter lists) and the `furniture/` name table.
- §2.5 (new): identity succession across several NPC_ records (12 `identity/*.yaml` files) and the name-hashed UUID fork with alias union (migration 0021).
- §4.1: package templates, and SkyrimNet's package registry (owners, priorities, holds, mirrored ActorUtil overrides, `ReinforcePackages`, package ModEvents).
- §5.2: the grunt filter matches subtitle text, not the subtype, plus the vanilla placeholder-subtitle filter.
- §5.3: how TTS enters the audio system (sound categories, output model, voice-path samples), with pointers to SN06 §1.6–1.7.
- §6.1: SKSE Papyrus native registration. §6.3: `FEATURES.md` on in-process memory reads and a non-blocking game thread.
- §7.7 (new): the dependency stack and what each framework fills, and the MIT C++ API's `LoadLibrary`/`GetProcAddress` pattern with its MSVC-ABI constraint.
- §8: OmniSight render capture.
- §12.1 items 9–11: three identity layers with a succession table, eligibility filters as data, a C ABI.
- §13 questions 7–8. §14: six new rows, and evidence for the time and crosshair rows.

**UNVERIFIED (could not check this session):**

- Every claim tagged BACKGROUND or WEB that would normally cite UESP or the CK wiki. That covers cell size; plugin limits (254 full + 4096 light); process-level update intervals; the latent-function lock semantics; `fUpdateBudgetMS` ≈ 1.2 ms; the `.ess` layout; Shared Info; the leveled-actor and template-resolution background.
- That `Skyrim.esm 0x084D1B` is `PlayerFollowerFaction`. Consistent with the ESP property name `factionPlayerFollowers`, but not checked against the master.
- That `0EB803:Skyrim.esm` is `AudioCategoryPausedDuringMenu`. This comes from PR #463 as cited in SN06, not re-fetched.
- The BEES-and-header-1.71 inference in §2.1 (open question 5). Whether SkyrimNet.esp is ESL-flagged or uses header 1.71 cannot be settled without the binary header, which was not opened.
- The meaning of "masked FormID" in the UUID hash (open question 8).
- Whether the "Persistent" flag on the test-cell references has the usual CK semantics (always loaded). This is BACKGROUND.
- OmniSight's capture mechanism. Only `FEATURES.md` describes the feature. How it hooks the renderer is undocumented.

**License check:** no content derived from compiled binaries was found or added. Quotes from SkyrimNet materials are single sentences or short API comments, well under the ~15-line cap. No prompt template or bio text is reproduced; the `combat_grunts` pattern is described, not copied.
