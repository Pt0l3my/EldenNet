# Elden Ring engine and runtime internals for an in-process LLM NPC mod

> **Scope:** the engine and runtime facts an in-process native DLL needs for an LLM-driven NPC dialogue framework ("EldenNet", modeled on SkyrimNet) on **ELDEN RING PC (Steam)**.
> **Research date:** 2026-11-06. **Game state at time of writing:** App ver. **1.17.2** (exe **2.7.2.0** WW / **2.7.2.1** JP).
> **Evidence tags:** **FACT** = sourced from code or an official or primary source. **LIKELY** = several weaker signals agree. **SPECULATION** = my inference, untested.
> **Method note:** In this session only GitHub (clone/raw) and web-search snippets were reachable. soulsmodding.com, fextralife, Steam and Bandai pages could not be opened directly. Claims based only on search snippets are marked as such. All repositories were read, never built or executed.
> **Fact-check pass (2026-11-06):** an adversarial review re-checked the flagged claims against the cloned sources (`git log`/`git show`, `git ls-remote` to confirm the clones match upstream HEAD), crates.io, independent repos (SoulSplitter, ERSoundBankLoader) and fresh search snippets. Corrections are inline and marked *(corrected)*. See the **Verification log** at the end. The web-search budget ran out partway through, so a few date claims still rest on snippets only.

---

## TL;DR

- **The game is still being patched, and offsets move on every patch.** 1.16.2 shipped around 2026-05-28. 1.17.0 shipped 2026-08-27 alongside the paid *Tarnished Pack* DLC and the Switch 2 *Tarnished Edition*. 1.17.1 shipped on or before 2026-09-08 (exact date unverified) and 1.17.2 on 2026-10-20. `fromsoftware-rs` tracks **107** RVAs per region. The 1.17.0 update changed 104 of them and the 1.17.2 update changed 99. Struct layouts also changed. In 1.17 a new 8-byte field at `ChrIns+0x3b8` shifted the following fields by +8, up to the tail (the modeled total size did not change, because the trailing unknown block shrank by 8) *(corrected: earlier text said `ChrIns` "grew by 8 bytes")*. In 1.17.2, `WorldChrMan` fields shifted by 0x10. `fromsoftware-rs` supports **exactly one exe build per region** and **panics** on any other version. **Hard-coded offsets are not an option. The design must use pattern/RTTI/reflection discovery plus a per-version struct-layout layer** (FACT, git history).
- **Engine:** FromSoftware's in-house *Dantelion2* lineage. Namespaces are `DLKR`/`DLTX`/`DLRF`/`DLUT`/`DLIO` (Dantelion libraries), `FD4` (framework: tasks, resources, params, singletons), `CS` (game layer; DS3 and Sekiro used `Sprj*` here) and `GX` (graphics). Built against the MSVC 2015 STL ABI. Renderer is **D3D12** (FL 12.0). Middleware is **Havok** (physics, behavior, AI navmesh, cloth, and HavokScript `.hks`), **Wwise** audio (`.bnk`/`.wem`/`.pck`, `MoWwiseMan`; not FMOD), **Scaleform** UI and Steamworks. Protection is **Arxan/GuardIT** anti-tamper plus **EasyAntiCheat** (EAC) for online play (FACT/LIKELY, see §2).
- **Getting code onto the game thread:** register a recurring task with `CSTaskImp` into one of **169** `CSTaskGroupIndex` frame phases *(corrected from "~190")* (`FrameBegin`, `ChrIns_PostPhysics`, …). This is the supported pattern in `fromsoftware-rs` and `libER`. Mutating or iterating game objects from any other thread is unsafe (FACT, code docs).
- **Finding singletons:** a generic *FD4Singleton finder*. It scans for the shape of the engine's own "uninitialized singleton" null-check code. The operand that loads the Japanese error string is a wildcard; the scan only validates that the logged file path ends in `FD4Singleton.h` *(corrected)*. It then resolves names through DLRF reflection. This gives `WorldChrMan`, `CSEventFlagMan`, `MsgRepository`, `SoloParamRepository`, `CSMenuMan`, `CSSessionManager`, `WorldAreaTime` and others with no per-patch offsets. `GameDataMan`, `GameMan`, `FieldArea` and the `register_task` function are *not* singletons. In `fromsoftware-rs` they come from the per-version RVA table, which an offline tool (`binary-mapper`) generates from AOB patterns. The library does **no runtime AOB scan**, so an unknown exe build panics (FACT).
- **Characters:** `WorldChrMan` owns `ChrSet`s (player, ghost, summon-buddy, debug and open-field sets, plus a 196-entry `chr_sets` pointer table indexed by the container field of a `FieldInsHandle`) *(wording corrected)*. Each `ChrIns` (base class of `PlayerIns`/`EnemyIns`) exposes `npc_param_id`, `character_id` (cXXXX model), `team_type`, `chr_type`, `event_entity_id` (MSB entity ID), `block_id`, a `FieldInsHandle`, and a module container. The data module holds HP/FP/stamina and the physics module holds position. `WorldChrMan` keeps a **list of characters sorted by distance to the player**, which is ideal for a perception pass (FACT).
- **Names and text:** `NpcParam.nameId` maps into the **NpcName** FMG (slot 18, DLC slots 328/428). Place names come from **PlaceName** (slot 19) and `FieldArea.map_place_name_id`. Dialogue text is **TalkMsg** (menu FMG 1), driven by `TalkParam` rows (`msgId`, `voiceId`, lipsync, timeout). Shipping mods already call or hook `CS::MsgRepositoryImp::GetMessage(repo, group, fmgId, msgId) -> wchar_t*` to resolve or inject text at runtime (FACT).
- **Event flags:** readable at runtime through `CSEventFlagMan` → `CSFD4VirtualMemoryFlag` (group = id/1000, 125-byte/1000-bit blocks, MSB-first bits). IDs encode the map: legacy `AABB####` (e.g. 10000800 = Godrick defeated) and overworld `10XXYY####` (e.g. 1042360000 block for m60_42_36). Each block has "conversation" sub-ranges (`…0700`, `…2700`). Global quest-state flags sit at 3000–4999 (FACT/LIKELY).
- **Talk scripts:** ESD state machines `t<talkId>.esd` inside `/script/talk/mAA_BB_CC_DD.talkesdbnd.dcx`. In ER the talk vocabulary is ~90 commands and ~82 functions (`TalkToPlayer`, `AddTalkListData`, `ShowShopMessage`, `GetEventFlag`, `GetDistanceToPlayer`, …). `fromsoftware-rs` can **invoke ESD talk commands from Rust** against a synthetic `TalkScript` (FACT).
- **Saves:** `%APPDATA%\EldenRing\<SteamID64>\ER0000.sl2` is a raw **BND4** on PC. It holds 10 character slots of 0x280000 bytes, each with an MD5 prefix, plus a profile block holding the SteamID and an AES-256-CBC + DCX-ZSTD copy of `regulation.bin`. Saves are bound to the SteamID. Mod memories should live in a **sidecar store** keyed by SteamID + slot + a character fingerprint, not inside the `.sl2` (FACT/LIKELY).
- **Ecosystem:** **me3** v0.13.0 (2026-08-11) is the latest tagged release and the maintained loader, with `[[natives]]` DLLs and `disable_arxan`. Its `main` branch has later, unreleased fixes (dearxan 0.5.5 → 0.5.7, Proton/launcher fixes up to 2026-10-13). ModEngine2 has been discontinued since 2024-07. **fromsoftware-rs** (Rust) is the most current structure library, with a caveat *(corrected)*: the newest crates.io release, `eldenring` **0.14.0** (2026-05-29, `MIT OR Apache-2.0`), only supports exe **2.6.2.x (1.16.2)**. Support for 1.17.0, 1.17.1 and 1.17.2 exists **only on unreleased git `main`**. **libER** (C++) was last updated for 1.16 (2025-04) and its symbol CSVs are stale. me3's `AI_CODE_POLICY.md` disallows all LLM use *in me3's own code, issue tracker, discussions and Discord*. It is not a restriction on which mods me3 can load, and me3 itself is MIT/Apache. On 2026-10-12 fromsoftware-rs replaced its license files with "MIT NON-AI" / "Apache NON-AI". These ban use in AI-training datasets and "training or development of an AI model or algorithm". `Cargo.toml` still says `MIT OR Apache-2.0`. Check licensing and community relations before depending on either (FACT).

---

## 1. Patch level (Nov 2026) and what it means for offset stability

| App ver. | Exe file version (WW / JP) | Date | Evidence |
|---|---|---|---|
| 1.16 | – | Oct 2024 per news snippets and eldenring.wiki.gg (one snippet dates a 1.16 bug-fix post to July 2025; **sources conflict**) | search snippets (onemoregame.ph 2024/10, mp1st, eldenring.wiki.gg "Game Versions") |
| 1.16.1 | 2.6.1.0 / 2.6.1.1 | 2025-08-21 | DSOGaming/Steam snippets; fromsoftware-rs `rva.rs` history |
| 1.16.2 | 2.6.2.0 / 2.6.2.1 | ~2026-05-28 (stability fixes) | fromsoftware-rs commit `0c3c4d3` "Update to ELDEN RING 1.16.2" (2026-05-28); rutab.net snippet |
| 1.17.0 | 2.7.0.0 / 2.7.0.1 | 2026-08-27 (Tarnished Pack DLC support, balance) | Bandai Namco patch notes (snippet); commit `acb2a19` (2026-08-27) |
| 1.17.1 | 2.7.1.0 / 2.7.1.1 | on or before 2026-09-08 (Tarnished-Pack DLC-item transfer/multiplayer fix; exact date unverified; PS build "1.018.001") | Vice and mp1st snippets (both dated 2026-09-08); commit `29fd138` (2026-09-08) |
| **1.17.2** | **2.7.2.0 / 2.7.2.1** | **2026-10-20** (4 bug fixes incl. an arrow status-buildup exploit, PS4/PS5 online-security fixes, no balance; **Regulation 1.17.3**; required for online play only) | bandainamcoent.com "ELDEN RING Tarnished Edition – Patch 1.17.2" and gaming-st.com (snippets); commit `b726336` (2026-10-20); TGA README "Game: App ver. 1.17.2" |

**FACT:** `fromsoftware-rs` detects the game version from the PE version resource. It reads the fixed `dwProductVersion`, the translation language ID and `ProductName == "ELDEN RING"` (`crates/shared/src/game_version.rs`). English/WW builds report `x.y.z.0` and Japanese builds `x.y.z.1`, and the two have **separate RVA tables** (`crates/eldenring/src/rva.rs`, `rva/rva_ww.rs`, `rva/rva_jp.rs`). A mod must handle both regional executables. The App-version to exe-version mapping (1.17.2 = 2.7.2.0/2.7.2.1) comes from the fromsoftware-rs commit titles. It follows the same pattern as 1.16.2 = 2.6.2.x, 1.17.0 = 2.7.0.x and 1.17.1 = 2.7.1.x (each verified with `git show`).

**FACT, critical for design (added):** `rva::get()` matches **only the single current version per region**. Each patch commit *replaces* the previous match arm (for example, `Ww2710` was removed when `Ww2720` was added), and an unknown version hits `panic!`. Anything that touches an RVA panics on an unsupported exe. That includes `CSTaskImp::run_recurring` (via `register_task`), `GameMan`, `GameDataMan`, `FieldArea` and `wait_for_system_init`. Only `#[singleton]` lookups (runtime scan through the `from-singleton` crate) survive a patch. A mod built on fromsoftware-rs therefore **stops working on every game patch** until upstream updates and the mod is rebuilt. Old game versions are dropped, not kept.

**FACT: How much moves per patch** (from git history of `fromsoftware-rs`):

- 1.16.2: ~7 RVAs changed (15 lines in `rva_ww.rs`, commit `0c3c4d3`).
- 1.17.0: 104 of 107 RVAs changed (commit `acb2a19`, 210 lines). Commit `b080915` ("Update some strucs for 1.17", 2026-08-28 by axd1x8a) inserted `unk3b8: usize` at `ChrIns+0x3b8`. `cloth_state`, `distance_to_player_sqr`, `update_priority`, the per-ChrIns tasks, `hit_material_override`, `debug_role_param_id` and the other fields after it shifted by +8. The trailing `unk548: [u8; 0x38]` became `unk550: [u8; 0x30]`, so the modeled struct size is unchanged. The commit also reworked `PlayerGameData` (24 lines).
- 1.17.1: only 5 RVAs changed (`29fd138`).
- 1.17.2, a bug-fix patch: 99 of 107 RVAs changed (`b726336`). Commit `5baf0dd` "Update shifted WorldChrMan fields" (Dasaav, 2026-10-21 00:47 +0200) re-laid the unknown region between `+0x1e55c` and `+0x1ece0`. Net effect: `debug_chr_creator`, `chr_cam` (`0x1ece0`→`0x1ecf0`), `chr_inses_by_distance`, `chr_inses_by_update_priority` and the omission budgets all moved by +0x10.

**Implication:** even "small" patches can change RVAs **and** struct layouts. Tooling update latency was same-day to ~2 weeks (1.17.0 RVAs the same day, struct fixes the next day, WorldChrMan shift fixed ~11 hours after the 1.17.2 RVA commit).

**Are more updates coming?** 1.17 exists to support the *Tarnished Pack* (new classes, Torrent skins, armor). It was released with the Switch 2 *Tarnished Edition* (2026-08-28, delayed from 2025 per FromSoftware's 2025-10-23 press release). Two bug-fix patches followed within two months. **LIKELY:** more 1.17.x maintenance patches will come while the Switch 2 port and DLC are fresh. **SPECULATION:** a long-term freeze like DS3's (1.15.x) will eventually arrive, but nothing announced confirms one. Plan for patches at any time and for users who refuse to update (keep old-version support). Note that 1.17.2 is required only for online play, so an offline modded user may want to stay on 2.7.1.x. Steam auto-update makes that hard in practice (LIKELY; users need depot downgrades or must block updates). Keeping old versions working requires **your own** layout and RVA tables or runtime scanning, because fromsoftware-rs drops old versions (see above).

---

## 2. Engine lineage and middleware

### 2.1 Dantelion / FD4 / CS layering

| Layer / namespace | What it is | Evidence |
|---|---|---|
| `DLKR` (kernel: allocators, mutexes, condition signals, runnables), `DLTX` (strings), `DLRF` (runtime reflection / `DLRuntimeClass`), `DLUT` (utilities: `DLDateTime`, fixed vectors, ref pointers), `DLIO` (file devices, streams), `DLCR` (crypto), `DLUID` (user input devices) | **Dantelion2** base libraries | fromsoftware-rs `crates/eldenring/src/{dlkr,dltx,dlrf,dlut,dlio,dlcr,dluid}`. libER `include/dantelion2/*`. The singleton macro doc says "Annotates a struct as a Dantelion2 singleton" (`crates/shared/macros/src/singleton.rs`). fromsoftware-rs cites a "dantelion2 leak" archive item as a source of names (`dlut/date_time.rs`). |
| `FD4` | Framework: task system (`FD4TaskBase`, `FD4TaskData{delta_time, task_group_id, seed}`), resources (`FD4ResRep`, `FD4ResCapHolder`), param repository, pad manager, time, singleton template (`FD4Singleton.h`) | fromsoftware-rs `src/fd4/*`, TGA `fd4_singleton.h` |
| `CS` | Game layer: `CSTaskImp`, `WorldChrMan`, `CSEventFlagMan`, `CSMenuManImp`, `MsgRepositoryImp`, … DS3/Sekiro used `Sprj*` for several of these (`SprjEventFlagMan`, `SprjTask`) | fromsoftware-rs `crates/darksouls3`, `crates/sekiro` vs `crates/eldenring` |
| `GX`/`GXFFX`, `FFX`, `FromNet`, `asterids` | Graphics, effects, networking, allocator | fromsoftware-rs `src/gxffx.rs`, `src/ffx.rs`, `src/from_net`, `src/asterids` |

**FACT:** The exe is built with the Visual Studio 2015-ABI MSVC STL, so C++ mods can share `std::` containers with the game if built with MSVC STL and `_ITERATOR_DEBUG_LEVEL=0` (libER README). **LIKELY:** "Dantelion" is a community or database label for the engine family, derived from the `DL` prefix and leaked material, not an official product name (Steam forum and game-database snippets).

### 2.2 Renderer, physics, audio, UI

- **D3D12, FACT:** official PC requirements list DirectX 12 (feature level 12.0) for both minimum and recommended specs (launch coverage, e.g. Gematsu/VGC 2022). Overlay libraries hook DX12. fromsoftware-rs's own debug tool uses `hudhook::hooks::dx12::ImguiDx12Hooks` (`tools/debug-eldenring/src/lib.rs`). hudhook 0.9.x is maintained (last commit 2026-09-08).
- **Havok, FACT:** the task phases include `HavokBehavior`, `HavokWorldUpdate_Pre/Post`, `HavokAi_World`, and `HavokClothUpdate_*` (`cs/task.rs`). Singletons include `CSHkBehManager`, `CSHkAiManager`, `CSHkAiNvmRepository`, `CSHavokMan`, `FD4HkClothManager` and `CSHkBehScriptRepository` (libER `symbols/singletons.csv`). Character behavior graphs are `/chr/cXXXX.behbnd.dcx`. Character control logic is **HavokScript** `/action/script/cXXXX.hks` (412 files, plus `c0000_talk.hks`, `common_define.hks`, `modifier.hks`), per the Smithbox `EldenRingDictionary.txt.json`. DSLuaDecompiler describes HavokScript as "used for character control logic in … Elden Ring". The exact Havok SDK year is **unverified**: tools such as HKLib 0.12 target ER specifically, but no source in reach names the version.
- **Audio = Wwise, FACT:** singletons `MoWwiseMan`, `CSFD4MoWwisebankRepository`, `BankManager` and `FilePackageLowLevelIOBlocking` (the Wwise SDK low-level IO class name) appear in libER `singletons.csv`, re-checked at lines 3, 4, 52 and 185. libER `symbols/sound.csv` also names `CS::CSSound::{make_system_sound, play_system_sound, play_character_sound}`. Assets include `/sound/pck/*.pck`, `/wem/…` and localized `/enus/…` (Smithbox dictionary). rewwise (vswarte) is "a set of tools for working with Elden Ring, Armored Core 6 and Nightreign soundbanks" (rewwise also uses the NON-AI license files). Independent confirmation: `KamiyamaShiki0704/ERSoundBankLoader` loads `cs_c####.bnk` banks "directly through Wwise `LoadBankMemoryCopy`", and BNKEditor warns that replacements must match the game's Wwise version. No FMOD evidence exists for ER.
- **UI = Scaleform, FACT:** `CSScaleform`, `CSScaleformArabic`, `GfxRepository`, `ScaleformTexRepository` (libER) and the `ScaleformStep` task phase (`cs/task.rs`). Native menus are Flash/GFx. Building new native menus is impractical, so use an ImGui overlay or repurpose existing dialogs (see Implications).
- **Steamworks:** `SteamThread0..5` task groups. **Bink video:** LIKELY (`CSMovie`, `MovieStep`; third-party articles say Bink).
- **Protection, FACT:** Arxan/GuardIT anti-tamper with code-restoration routines and encrypted functions. `dearxan` (tremwil) is "able to fully neuter Arxan in all the FromSoftware games using it", with a footnote on "fully". The footnote says the Arxan entry-point stubs still run, and it is tested against "the latest versions" of ER and other titles. me3 integrates it as the `disable_arxan` profile option (since me3 0.8.0), and the dearxan README recommends that option for me3-based mods. fromsoftware-rs ships its own `arxan.rs` code-restoration disabler and advises: "Prefer using the task runtime over hooking the memory image". **EAC:** must be off for any DLL mod. That forces offline play and creates ban risk online (EldenRingEacToggler README, Windows Central, me3 defaults to blocking official servers via `start_online=false`).

---

## 3. Process and threading model

### 3.1 Loading a native DLL

- **me3 (FACT):** a `.me3` TOML profile with `[[natives]] path=…`, plus optional `initializer`/`finalizer` symbols, `load_early`, `optional`, and profile-level `disable_arxan`, `savefile` (separate save name) and `start_online` (`docs/configuration-reference.md`, `schemas/mod-profile.md`). Latest release v0.13.0 (2026-08-11) added "Allow deferring after game props init". It supports Windows, Linux/Proton and macOS/CrossOver.
- **ModEngine2:** "Development … is discontinued. Any future work will be done on me3" (README, 2024-07-19).
- **Startup gate (FACT):** fromsoftware-rs `wait_for_system_init` spins until the global `HINSTANCE` used by `CSWindow` is populated, which happens after CRT init and the duplicate-instance check. `CSTaskImp::wait_for_instance()` must be called from a **non-main** thread, because the main thread creates the singleton (`util/system.rs`, `cs/task.rs`). Typical pattern: in `DllMain`, spawn a thread, wait for `CSTaskImp`, then register tasks (examples `apply-speffect`, `present-action-button`).

### 3.2 The frame pipeline: `CSTaskImp` and task groups

**FACT** (`cs/task.rs`): `CSTaskImp` (FD4 singleton `"CSTask"`) wraps `CSTask`. `CSTask` holds a `CSTaskBase` (a vector of `TaskGroupEntry{index, name: [u16; 64], active}`), a `CSTaskRunnerManager`, `task_runners: [CSTaskRunner; 6]` and `task_runners_ex: [CSTaskRunnerEx; 6]` (the latter unmodeled). The manager holds 6 `DLPlainConditionSignal`s and a `TaskGroupConcurrency` policy table with 6 slots. That the runners are worker threads is LIKELY (inferred from the condition signals; not documented). The `CSTaskGroupIndex` enum has **169** variants *(corrected from "~190")*. Every frame runs these ordered groups, from `FrameBegin` through `SystemStep`, `PadStep`, `GameFlowStep`, `EventMan`, `WorldChrMan_Prepare`, `ChrIns_AILogic`, `HavokBehavior`, `ChrIns_PrePhysics`, `HavokWorldUpdate_*`, `CameraStep`, `SoundStep`, `ChrIns_PostPhysics`, `WorldChrMan_PostPhysics`, `DmgMan_*`, `MenuMan`, `ScaleformStep`, `GraphicsStep`, `DrawStep`, `FileStep`, `Flip` and `DelayDeleteStep`, to `FrameEnd`. Some groups run in parallel (names such as `LocationUpdate_PrePhysics_Parallel` and `GameFlowInGame_MoveMap_PostPhysics_1_Core0/1/2`). Several phases have a `…Safe` twin (`ChrIns_PreBehaviorSafe`, `ChrIns_PrePhysicsSafe`, `ChrIns_PostPhysicsSafe`, …). Their semantics are undocumented, and they are worth testing as the snapshot phase.

**How mods run code on the game thread (FACT):** `CSTaskImp::run_recurring(closure, CSTaskGroupIndex::X)` builds a `RecurringTask` with a fake FD4 task vtable and calls the game's own `register_task`. *(Clarified.)* The AOB `e8 ? ? ? ? 48 8b 0d ? ? ? ? 4c 8b c7 8b d3 e8 $ { ' }` (`mapper-profile.toml` line 328) is used **offline** by `tools/binary-mapper` to produce the RVA. At runtime the library reads `rva::get().register_task`, which panics on an unknown exe. A source comment says "TODO: Implement this by directly manipulating the CSTaskImp's vectors rather than relying on hooking through an AOB". Dropping the handle sets `unregister_requested`, and the task cancels itself (`crates/shared/src/task.rs`). libER exposes the same idea as `CS::CSEzTask` subclasses (`include/coresystem/task.hpp`). The documented safety contract: `instance_mut()` "should only ever be called from the game's main thread (typically either from the task system or from hooked functions that run in the main thread)". `CSTaskImp` itself is documented as thread-safe (`crates/shared/src/static.rs`, `cs/task.rs`).

**Practical rules for EldenNet (LIKELY / design):**

1. Run **one** snapshot task per frame, or every N frames, in a late phase such as `ChrIns_PostPhysics` or `WorldChrMan_PostPhysics`, where positions for the frame are final. In it, copy a small POD snapshot (characters, flags, time, place) into a lock-free queue.
2. Do HTTP/LLM/TTS/STT work on your own threads. **Never block a game task:** a blocked task stalls the frame pipeline. Whether rendering runs on the main thread or on a separate render thread is unverified.
3. Apply results (show a subtitle, start a talk, set a flag) from a task callback that drains an inbound queue. Use an early phase (`FrameBegin`) or the `MenuMan` phase for UI-related calls.
4. hudhook render callbacks run on the D3D12 present path, not inside the task system. Treat game memory as read-only snapshots there (SPECULATION: the fromsoftware-rs debug tool reads game structs from the render loop, but that is a debug tool).

---

## 4. Finding singletons and functions robustly

| Technique | How | Robustness | Source |
|---|---|---|---|
| **FD4Singleton finder** | Scan `.text` for the byte shape `48 8b ?? ?? ?? ?? ?? 48 85 ?? 75 2e 48 8d 0d ?? ?? ?? ?? e8 ?? ?? ?? ?? 4c 8b c8 4c 8d 05 ?? ?? ?? ?? ba ?? ?? 00 00 48 8d 0d ?? ?? ?? ?? e8`, which is `mov reg,[static]; test; jnz +2e; lea rcx,[runtime_class]; call get_name; mov r9,rax; lea r8,[fmt]; mov edx,line; lea rcx,[path]; call log`. *(Corrected.)* The `lea r8` operand (the game's own log-format string, which reports that a singleton was used before it was initialised) is **wildcarded and never compared**. The checks are: the static is in `.data`, the runtime class is in `.data`, the getter is in `.text`, the path is in `.rdata` and **ends with `FD4Singleton.h`**. Then it calls the game's own name getter on the DLRF runtime class. | Very high, because it relies on engine idioms rather than offsets. `from-singleton` README: "first time initialization time of 10-15 ms"; tested on "ER 1.16" (not re-tested on 1.17.x). libER lists ~218 `GLOBAL_*` singletons. |  TGA `table_files/include/tga/fd4_singleton.h`; Dasaav `from-singleton` (tested on ER 1.16); fromsoftware-rs `#[shared::singleton("Name")]` |
| **Per-static AOB patterns** | e.g. `GameDataMan`: `48 89 05 ${ ' } 48 83 c4 38 e9 ${}`; `FieldArea`: `48 8b 05 ${ ' } 48 85 c0 48 0f 44 c6`. Regenerated per exe **offline** by `tools/binary-mapper` into `rva_ww.rs`/`rva_jp.rs`. fromsoftware-rs ships only the resulting RVAs. EldenNet can run the same patterns **at runtime** to survive patches. | Medium: patterns usually survive, RVAs never do | fromsoftware-rs `crates/eldenring/mapper-profile.toml` |
| **RTTI vftables** | `find_rtti_classes` resolves vtables such as `CS::ChrIns`, `CS::ChrSet`, `CSTeamTypeEnemy` | High (MSVC RTTI is present) | fromsoftware-rs `crates/shared/src/rtti.rs`, `mapper-profile.toml [[vmts]]` |
| **Versioned symbol tables** | CSV of RVAs per game version, auto-pulled by `libER_updater.exe` | Breaks when the maintainer lags. libER HEAD `d8ae5c9` (2025-04-24, confirmed with `git ls-remote`) is still at 1.16, so no CSVs exist for 1.16.1 or later | libER `symbols/*.csv` |
| **Interior-anchored AOB for hooked functions** | Anchor at entry+5 because other mods (e.g. Elden Ring Reforged) MinHook the prologue | Needed for coexistence | MapForGoblins `src/re_signatures.hpp` (`GETMESSAGE`) |

**Singletons that matter for EldenNet** (names from fromsoftware-rs `#[singleton]` or libER `singletons.csv`; FACT that they exist):

| Singleton | Use |
|---|---|
| `WorldChrMan` | All loaded characters, main player, distance-sorted list, handle lookup |
| `GameDataMan` (AOB static) | `main_player_game_data` → `PlayerGameData.character_name[17]` (UTF-16), stats; `game_settings` |
| `GameMan` (AOB static) | `save_slot`, warp/move-map requests, `world_area_time` copy, `talk_esd_camera_param_id` |
| `FieldArea` (AOB static) | `WorldInfo` (areas/blocks), `map_place_name_id`, `save_map_name_id`, `current_play_region_id` |
| `WorldAreaTime` / `WorldAreaWeather` | In-game clock (`DLDateTime`), time multiplier; weather |
| `CSEventFlagMan` | Event-flag read/write (§9) |
| `MsgRepository` (`MsgRepositoryImp`) | FMG text lookup/injection (§8.3). In fromsoftware-rs it is an **opaque** unit struct (singleton only, added 2026-06-18). `GetMessage` and the layout come from MapForGoblins' RE, not from fromsoftware-rs |
| `SoloParamRepository` / `FD4ParamRepository` | Live param rows: `get::<NPC_PARAM_ST>(id)` (`fd4/param_repository.rs`) |
| `CSMenuMan` (`CSMenuManImp`) | UI state flags, popup menu (`current_talk_id`), `display_status_message(id)` |
| `CSActionButtonMan` | `present_action_button(ActionButtonParam id)`, a custom "Talk" prompt (example `present-action-button`) |
| `CSSessionManager` | Lobby/protocol state and player list (SteamIDs), used to **disable the mod in multiplayer** |
| `CSTask`, `CSTaskGroup` | Task registration |
| Also present: `CSWorldTalkMan`, `CSWorldAiManager`, `EsdRepository`, `LuaRepository`, `CSRegulationManager`, `CSMapPlaceNameOverrideRegionMan`, `CSSound`, `MoWwiseMan`, `CSWordChecker`, `MsgTagMan` | Not yet modeled publicly. Candidates for later RE |

---

## 5. The character system

### 5.1 Containers

**FACT** (`cs/world_chr_man.rs`, 1.17.2 layout): `WorldChrMan` contains `world_area_chr[28]`, `world_block_chr[192]` (each with a per-block `ChrSet` and `BlockId`), `player_chr_set: ChrSet<PlayerIns>`, `ghost_chr_set`, `summon_buddy_chr_set`, `debug_chr_set`, `open_field_chr_set`, `chr_sets[196]` (pointers, indexed by the container field of a `FieldInsHandle`), `main_player: Option<PlayerIns*>`, `summon_buddy_manager`, `net_chr_sync`, `debug_chr_creator`, **`chr_inses_by_distance: DLVector<{ChrIns*, distance}>`**, `chr_inses_by_update_priority`, and omission budgets. `ChrSet<T>` holds `capacity`, an `entries` array of `ChrSetEntry{chr_ins, chr_load_status, chr_update_type, flags}`, and maps `entity_id_mapping` and `group_id_mapping` (`DLMap<u32, entry*>`). `WorldChrMan::chr_ins_by_handle(&FieldInsHandle)` resolves handles.

### 5.2 `ChrIns` and subclasses

**FACT** (`cs/chr_ins.rs`, RTTI names): `ChrIns` is "Abstract base class to all characters. NPCs, Enemies, Players, Summons, Ghosts…". Fields relevant to us:

- **Identity:** `field_ins_handle`, `npc_param_id` (row in `NpcParam`), `npc_id`/`character_id` (4-digit model, e.g. 8000 = Torrent; all humanoid NPCs share **c0000**), `chr_type` (enum: `Local`, `WhitePhantom`, `Npc=5`, `WhiteSummonNpc=19`, …), `team_type: u8`, `event_entity_id` (MSB/EMEVD entity ID), `p2p_entity_handle`.
- **Location:** `block_id`, `block_origin`, `chunk_position`, `initial_position`, and `distance_to_player_sqr` / `horizontal_distance_to_player_sqr`.
- **Combat cues:** `last_hit_by: FieldInsHandle`, `is_locked_on`, `special_effect` (SpEffect list), the "dead" bit in `ChrInsFlags1c5`, and `lock_on_target_position`.
- **Lifecycle:** `load_state`, `omission_mode`, deactivation and fade distances, `chr_activation_flags`.
- **Modules:** `modules: ChrInsModuleContainer*`.

`PlayerIns` adds `player_game_data`, `block_position`, `current_block_id`, `play_region_id`. `EnemyIns` adds `com_manipulator`, `net_ai_manipulator`, `ride_manipulator` and `npc_think_param: i32`.

**Modules (FACT, `cs/chr_ins/module.rs`):** `data` (`CSChrDataModule`: `hp`, `max_hp`, `fp`, `stamina`, `chara_init_param_id`, `character_behavior_name`, `msb_parts`), `action_flag`, `time_act` (TAE), `behavior`, `super_armor`, `toughness`, `event`, `physics` (`CSChrPhysicsModule.position: HavokPosition`, `orientation: Quaternion`), `fall`, `ladder`, `action_request`, `throw`, `material`, `behavior_data`, `ride`, `wet`, `grass_hit` and others. The `ai`, `talk`, `damage` and `magic` modules are still opaque (`usize`) in public bindings.

**Coordinate spaces (FACT, `position.rs`):** block-local (MSB placement), global world, **Havok** (broadphase space used for raycasts and effective positions) and map-UI space. Havok and block spaces are both in meters, so displacements carry over between them. *(Caveat added.)* Block positions are relative to their own block center, so compare characters in Havok space (or use `ChrIns.distance_to_player_sqr`). Persist positions as `BlockId` + block-local coordinates, never as Havok coordinates. The source calls Havok space "seems to be AABB broadphase space", and whether its origin is stable across map loads is unverified. `cs_phys_world_cast_ray` is exposed as an RVA, which makes line-of-sight checks possible later.

### 5.3 Enumerating characters (sketch)

```text
on task(ChrIns_PostPhysics, every ~0.25 s):
  wcm = WorldChrMan::instance()
  player = wcm.main_player
  for e in wcm.chr_inses_by_distance:            # already sorted, includes only loaded/active chars
     c = e.chr_ins; if e.distance > R: break
     snapshot(c.npc_param_id, c.character_id, c.event_entity_id, c.team_type, c.chr_type,
              c.modules.data.{hp,max_hp}, c.modules.physics.position, c.last_hit_by, dead_bit)
```

`chr_inses_by_distance` is FACT (doc: "A list of ChrIns references sorted by distance to the main player"). Its element is `ChrInsDistanceEntry{chr_ins: NonNull<ChrIns>, distance: f32, _unkc: u32}`. *(Added caveat.)* The docs do not say whether `distance` is squared, though `ChrIns.distance_to_player_sqr` is. Calibrate `R` in game before relying on it. Whether it covers every loaded character, including open-field ones, is LIKELY but unverified. The fallback is to walk `chr_sets[]` entries.

### 5.4 Naming and classifying characters

- **Name:** `NpcParam[npc_param_id].nameId` → NpcName FMG (FACT: in the paramdef, `nameId` is the field that holds the NPC's name entry). MapForGoblins resolves enemy names exactly this way (`GetMessage(NpcName)` with slots `{428, 328, 18}`).
- **Kind of creature:** `character_id` → model name list (`DarkScript3/Resources/er-common.ModelName.txt`, 455 entries, e.g. `c3000 Exile Soldier`, `c4300 Wandering Noble`). c0000 is "Human", so humanoids need `nameId` or `CharaInitParam` equipment (`equip_Wep_Right`, `equip_Helm`, … → item FMGs) for a description.
- **Named vs generic:** `NpcParam.npcType` (0 Standard / 1 Boss), `teamType`, the presence of a `talkID` in the MSB enemy part, and a non-zero `event_entity_id`. A named NPC with a talk script is the "important NPC" signal (LIKELY heuristic). The DarkScript3 `er-common.CharaName.txt` maps NPC IDs to names (e.g. `23070 Preceptor Seluvis`).
- **Persistence keys:** pointers and `FieldInsHandle` are runtime-only. Use `(event_entity_id, npc_param_id, BlockId)` for placed NPCs. **Open question:** how many generic enemies have `event_entity_id == 0`.

---

## 6. AI: think params, Lua goals, team types

- **`NpcThinkParam` (FACT, paramdef):** `logicId` (which logic script the character runs), `battleGoalID`, perception (`eye_dist`, `searchEye_dist`, `searchEye_angY`, `ear_dist`, `nose_dist`), forget timers (`searchTargetLv1/2ForgetTime`, `SightTargetForgetTime`) and call-for-help animations. The think ID comes from the MSB enemy part (`thinkParamID`) or `EnemyIns.npc_think_param`.
- **Scripts (FACT for file layout; LIKELY for the Lua version):** AI is **Lua 5.0** bytecode. DSLuaDecompiler says "Lua 5.0, which is used for AI scripts in almost all FromSoft games", a generic statement that lists ER among its targets but is not ER-specific. Files are `/script/NNNNNN_battle.luabnd.dcx` (396), `/script/NNNNNN_logic.luabnd.dcx` (70) and `/script/aicommon{,_dlc01,_dlc02}.luabnd.dcx` (Smithbox dictionary). Behavior/animation control is **HavokScript** (`.hks`).
- **Hostility (FACT):** `NpcParam.teamType` (whom the NPC's attacks can hit and target) with community enum names: 6 Enemy, 7 Boss, 14 Neutral, 26 Friendly NPC, 27 Hostile NPC, 29 Indiscriminate, … (Smithbox `Param Enums/TEAM_TYPE.json`). At runtime, relations are decided by `CSTeamType{Neutral,Friend,Enemy,Rival}::validate(TeamRelationTargetInfo{oppose_target, friendly_target, self_target})` (`cs/team_relation.rs`). EMEVD changes it live through `2004[02] Set Character Team Type`, which is how friendly NPCs turn hostile. **Read `ChrIns.team_type` at runtime, not the param.**
- **Awareness hooks:** `CSAiTargetingSystemOwner` / `CSTargetSearchSys` (`latest_ai_sound_id`, search slots) and `NpcThinkParamLookupResult{battle_goal_id, logic_id}` (`cs/targeting.rs`). For v1 barks, cheap derived signals are enough (HP delta, `last_hit_by`, dead bit, distance to player, `is_locked_on`).

---

## 7. World and map structure

- **`BlockId` (FACT, `cs/block_id.rs`):** an i32 packing `area:8 | block:8 | region:8 | index:8`, displayed as `mAA_BB_CC_DD`. Areas 50–88 are treated as overworld.
- **Map IDs (FACT, DarkScript3 `er-common.MapName.txt`, 665 entries):**
  - Legacy dungeons: m10 Stormveil, m11 Leyndell / Roundtable Hold (`m11_10`), m12 underground rivers / Mohgwyn, m13 Farum Azula, m14 Raya Lucaria, m15 Haligtree, m16 Volcano Manor, m18 Stranded Graveyard, m19 Stone Platform.
  - DLC legacy: m20 Belurat, m21 Shadow Keep, m22 Stone Coffin Fissure, m25 Finger Birthing Grounds, m28 Midra's Manse.
  - Minor dungeons: m30 catacombs, m31 caves, m32 tunnels, m34 divine towers, m35 Shunning-Grounds, m39; DLC m40–m43; m45 Colosseums.
  - Overworld: **m60** Lands Between (395 tiles) and **m61** Realm of Shadow (172 tiles). Tiles are `m60_XX_YY_00` (small) with coarser LOD grids `_01`/`_02`.
- **Runtime world info (FACT, `cs/field_area.rs`):** `FieldArea.world_info_owner → WorldInfo` lists area, grid-area and block infos (`WorldBlockInfo{block_id, area_info, …}`). `FieldArea.map_place_name_id` is the current place-name text ID (LIKELY PlaceName FMG). `PlayerIns.play_region_id` / `FieldArea.current_play_region_id` index **`PlayRegionParam`**, which governs **multiplayer and region rules** (invasion limits, boss areas, warp permissions). It is not a display name.
- **Place-name data (FACT, paramdefs):** `WorldMapPlaceNameParam{textId → PlaceName, areaNo, gridXNo, gridZNo, posX/Z}` and `WorldMapPointParam{textId1.., eventFlagId, areaNo, gridXNo…}`, plus a `CSMapPlaceNameOverrideRegionMan` singleton (libER). A deterministic "where am I" string can be built from the BlockId, the PlaceName ID and the nearest `WorldMapPlaceNameParam` row.
- **MSB (FACT):** `/map/mapstudio/mAA_BB_CC_DD.msb.dcx` (1,349 files). The `PartEnemy` struct carries `thinkParamID`, `npcParamID`, `talkID`, `platoonID`, `charaInitID` and a common `entityID` (Smithbox `Documentation/Binary Templates/MSB/MSBE/Part.bt`). Offline, this is the master list of where named NPCs and their talk IDs are placed.

---

## 8. Data and config formats

### 8.1 Containers

- **Archives:** `Data0–3.bhd/.bdt` (BHD5) with hashed paths. Unpacking needs UXM Selective Unpacker or Smithbox plus a path dictionary (Smithbox ships `EldenRingDictionary.txt.json`) (LIKELY/FACT).
- **DCX** compression wraps BND4 binders and files. SoulsFormats supports `DCX_KRAK` (Oodle Kraken) and `DCX_ZSTD` (`Formats/DCX.cs`). LIKELY: most ER assets are KRAK. SaveForge's parser expects `DCX…ZSTD` for the regulation copy in saves.
- **`regulation.bin` (FACT):** AES-256-CBC (`Keys.ER_REGULATION_KEY`) over DCX over a **BND4 of `.param` files**. Handled by `SFUtil.DecryptERRegulation`/`EncryptERRegulation` (Smithbox `SoulsFormats/Util/SFUtil.cs`). Paramdefs are in Paramdex/Smithbox and in fromsoftware-rs `tools/param-generator/params/eldenring/*.xml`. At runtime, `SoloParamRepository` holds indexed `ParamResCap`s (`NpcParam` = 6, `NpcThinkParam` = 9, `TalkParam` = 18, `PlayRegionParam` = 38, `WorldMapPlaceNameParam` = 90 in `cs/solo_param_repository.rs`). Rows are read with `FD4ParamRepository::get::<P>(row_id)`.
- **Key params for EldenNet:** `NpcParam` (`nameId`, `teamType`, `npcType`, `vowType`, `threatLv`, `roleNameId`, `soundBankId`, `itemLotId_*`), `CharaInitParam` (NPC loadouts/stats, `npcPlayerThinkId`), `NpcThinkParam`, `TalkParam`, `ItemLotParam` (drops), `ShopLineupParam`, `PlayRegionParam`, `BonfireWarpParam` (graces), `WorldMapPointParam` / `WorldMapPlaceNameParam`.
- **Merge risk:** `regulation.bin` is a single file. Only one mod's copy wins (Nexus snippets), so overhaul mods like ERR or Convergence ship their own. **EldenNet v1 should not ship a regulation.bin.**

### 8.2 FMG message files

`/msg/<lang>/{item,menu}{,_dlc01,_dlc02}.msgbnd.dcx` and `ngword.msgbnd.dcx` (FACT, Smithbox dictionary and FMG registry). FMG IDs (Smithbox `Assets/FMG/ER/Fmg Descriptor Registry.json`):

- **menu:** 1 = TalkMsg, 2 = BloodMsg, 3 = MovieSubtitle, 4 = TalkMsg (female PC), 31 = NetworkMessage, 32 = ActionButtonText, 33 = EventTextForTalk, 34 = EventTextForMap, 200 = MenuText, 203 = SystemMessage, 204 = Dialogues. DLC variants (re-checked in the registry): TalkMsg **360** (DLC1) / **460** (DLC2), MovieSubtitle 362/462, TalkMsg-female 363/463, EventTextForTalk 366/466, Dialogue 372/472.
- **item:** 10–14 names (Goods/Weapons/Armor/Accessories/Spells), **18 = NpcName** ("Characters"), **19 = PlaceName** ("Locations"), 20–28 summaries/descriptions. DLC: NpcName 328/428, PlaceName 329/429.

### 8.3 Runtime text: `MsgRepositoryImp`

**FACT** (MapForGoblins `src/goblin_messages.cpp`, `src/re_signatures.hpp`, built against exe 2.6.2.0):

- `const wchar_t* GetMessage(MsgRepositoryImp* repo, u32 group /*=0*/, u32 fmgSlot, u32 msgId)`. It bounds-checks and null-checks internally and returns NULL on a miss.
- DLC layers are separate slots: NpcName {18, 328, 428}, PlaceName {19, 329, 429}, and similar for goods (10/319/419).
- The repository layout used is `repo+0x08` = base array and `repo+0x14` = slot count.
- Elden Ring Reforged **MinHooks** `GetMessage` to merge DLC text.
- MapForGoblins also **patches FMGs in memory** to add new PlaceName entries.
- *(Added.)* MapForGoblins passes **menu** FMG IDs (ActionButtonText `32`, BloodMsg `{461, 361, 2}`, Tutorial `{475, 375, 207}`) and **item** FMG IDs (`18`, `19`, …) through the same `GetMessage(repo, 0, id, msgId)`. So the runtime slot index appears to equal the Smithbox FMG ID across both binders. **LIKELY:** TalkMsg = slot 1 (DLC 360/460), but this is untested.
- *(Caveat added.)* The AOB and RE notes were made on exe **2.6.2.0** (1.16.2). MapForGoblins v2.9.1 (2026-08-15) predates 1.17, so revalidate the `GETMESSAGE` pattern on 2.7.2.x.

This is the established way to (a) resolve names and (b) **inject arbitrary runtime strings** (hook `GetMessage` and answer a reserved ID range with LLM text). Hook it compatibly: anchor the AOB in the function interior, chain to other hooks, and never assume you are first.

---

## 9. Event scripting (EMEVD) and event flags

- **EMEVD (FACT):** `/event/mAA_BB_CC_DD.emevd.dcx` (~585–587 depending on how variants are counted) plus `common.emevd.dcx` and `common_func.emevd.dcx`. Instruction banks include 2003 Event (set flag, award item lot, warp), 2004 Character (team type, AI state, SpEffect, animations, `Set Character Talk Range` 2004[55]) and 2007 Message (`Display Generic Dialog`, `Display Banner`, `Display Status Message`, area welcome messages) (DarkScript3 `Resources/er-common.emedf.json`, 27 classes). EMEVD is the game's quest/state glue. EldenNet reads its **results** (flags), not its code.
- **Flag storage (FACT, `cs/event_flag.rs`):** `CSEventFlagMan{virtual_memory_flag: CSFD4VirtualMemoryFlag, world_type}`. Lookup: `group = id / 1000`, then `flag_block_descriptors: DLMap<u32, FlagBlockDescriptor>`. Mode 1 is an offset into a holder array and mode 2 is an external block. Each `FlagBlock` is 125 bytes (1000 bits): `byte = (id % 1000) / 8` and `bit = 7 - (id % 8)`. Flags outside allocated groups do not exist. Allocation lists are `/event/eventflag/{legacymap,openmap}{,_dlc02}.eventflagalloclist.dcx`. `get_flag` is a pure read. Do it on the game thread to avoid tearing. *(Caveats added, from `cs/event_flag.rs`.)* (1) `get_flag` returns **`false` for an unallocated group**, so "flag off" and "flag does not exist" look the same. Validate each curated flag ID against the descriptor map once, at startup. (2) `set_flag` on a missing group silently does nothing, and its doc says it "does not inherently network set flags". (3) The Rust helper hard-codes `/1000`, while the engine struct has a live `event_flag_divisor` field. Assert that it equals 1000. (4) Off-thread reads are **not** fully benign, because the `DLMap` descriptor tree lookup can race with block (de)allocation during map loads (SPECULATION, but cheap to avoid).
- **Watching changes:** TGA's Event Flag Logger hooks the flag-set routine (AOB `33 d2 41 8b c2 41 f7 f1 41 8b d8`, the division by the group divisor). A hook gives an event stream ("player just completed X"). Polling a curated flag list each second is simpler for v1.
- **ID encoding:**
  - Global flags (FACT, `soulsmods/elden-ring-eventparam` index.md, EventFlagUsageParam-derived): 0–9999 (system, story progress 100–199, time/weather 500–599, boss-kill 9100–9199, **3000–4999, the per-character state flags**). NPC quest states live here: TGA's comments tie 3269 to Godrick's defeat (it hides Gostoc) and 3468/3469 to Sellen.
  - Shop-stock bands: 100000–390999.
  - Grace bands: 71000 (legacy) and 76000 (open world) (TGA Event Flag Manager).
  - Per-map blocks (FACT for the listed sub-ranges, LIKELY for "owns the whole 10,000-ID range"): overworld `m60_42_36_00` has sub-ranges in **1042360000–1042369999**. These are `0000` graces (20 flags), `0050` init, `0100/2100` system, `0200` character (non-reset), `2200` character (reset), `0400/2400` character flags for minor exploration, `0500/2500` objects, `8500` object-act, **`0700/2700` conversation flags (100 each)**, `0800/2800` boss and `0900/2900` debug (eventparam `EFID_m60_42_36_00`). Legacy dungeons follow `AABB####`: TGA uses **10000800** for Godrick defeated and 14000800 for Rennala. *Independently confirmed* by SoulSplitter's `EldenRing/Boss.cs` (`GodrickTheGraftedStormveilCastle = 10000800`, `RennalaQueenOfTheFullMoon… = 14000800`). The eventparam index prints those blocks one digit shorter (1000800), which looks like a rendering error. Trust runtime-verified IDs.

---

## 10. Talk scripts (ESD)

- **Files (FACT):** `/script/talk/mAA_BB_CC_DD.talkesdbnd.dcx` (36 binders, e.g. `m60_00_00_00`, `m61_00_00_00`, `m11_10_00_00` Roundtable Hold). Each contains `t<talkId>.esd`, where `talkId` is the MSB `PartEnemy.talkID`. ESDLang/esdtool (thefifthmatt, supports ER/AC6/NR) decompiles to Python-like state machines.
- **Vocabulary (FACT, ESDLang `dist/ESDScriptingDocumentation_Talk.json`, filtered to ER):** ~90 commands and ~82 functions.
  - Commands: `TalkToPlayer(talkParamRow, unk, unk, eventFlagId)` [1], `ForceEndTalk` [8], `ShowShopMessage(optionsType)` [10], `SetEventFlag(id, state)` [11], `OpenGenericDialog` [17], `AddTalkListData(slot, textId, unk)` [19], `ClearTalkListData` [20], `OpenRegularShop` [22], `OpenConversationChoicesMenu` [76], `TurnCharacterToFaceEntity` [103], `AwardItemLot` [104], `SetLookAtEntityForTalk` [139], `SetEventFlagValue(base, bits, value)` [147].
  - Functions: `GetDistanceToPlayer`, `GetEventFlag`, `GetTalkListEntryResult`, `GetTalkInterruptReason`, `IsPlayerAttacking`, `HasPlayerBeenAttacked`, `GetSelfHP`, `CheckSpecificPersonTalkHasEnded`, `IsTimeOfDayInRange`, `GetIsOnline`, …
- **Vanilla dialogue path (FACT, `TalkParam` paramdef):** `TalkToPlayer(row)` → `TalkParam{msgId → TalkMsg (FMG 1), voiceId → Wwise voice, msgId_female/voiceId_female, lipSyncStart/lipSyncTime, timeout (how long the voice may play), talkAnimationId, isForceDisp (forces the subtitle on even when subtitles are off)}`. *(Details re-checked in `params/eldenring/TalkParam.xml`.)* `msgId`/`voiceId` are documented as the **male-PC** variants (the paramdef ties them to a male player character), and the `_female` fields are used for female PCs. `lipSyncStart = -1` means no mouth movement. `timeout = -1` falls back to the global NPC voice timeout in the common game-system param, so a voiceless line's display time is governed by `timeout`. Vanilla rows usually share the ID of the TalkMsg line (e.g. 2003000, one of Margit's lines; paramdex `TalkParam.txt`). ESDLang marks the two `unk` args of `TalkToPlayer` as "use -1" and `eventFlagId` as "usually 0".
- **Invoking ESD from a mod (FACT):** fromsoftware-rs `cs/talk_script.rs` builds a `TalkScript{env: CSEzStateTalkEnv, event: CSEzStateTalkEvent, talk_id, npc_talk: CSNpcTalkIns, map_id}` bound to an NPC `FieldInsHandle`. `event_unchecked(args)` executes a talk command, with preconditions "WorldChrMan initialized, CSMenuMan initialized, NPC exists". The `invoke-esd` example opens a custom talk menu (`AddTalkListData`, `ShowShopMessage`, `OpenRegularShop`) on a key press. *(Caveat added.)* That example binds its `TalkScript` (talk_id 1000, `BlockId::none()`) to the **main player's** `FieldInsHandle`, not to an NPC. It only exercises menu, shop and list commands, and it runs in `FrameBegin`. No public example shows `TalkToPlayer` (command 1) on a real NPC through a synthetic `TalkScript`. Treat that path as unproven.

---

## 11. Save system and what can be persisted alongside

- **Location (FACT):** `%APPDATA%\EldenRing\<SteamID64>\ER0000.sl2` (SaveForge `DefaultSavePath` for Proton mirrors this). me3 `savefile` can redirect a modded profile to a separate save name.
- **Format (FACT, SaveForge `backend/core`):** a native PC save is a raw **BND4** (SaveForge refuses AES-wrapped containers as "unsupported"). It holds 10 `USER_DATA00x` character slots of **0x280000** bytes, each preceded by an **MD5**, plus `USER_DATA010` (profile summaries; **SteamID u64**) and `USER_DATA011` (embedded `regulation.bin`, AES-256-CBC + DCX ZSTD). Slots contain the event-flag region (`EventFlagsOffset`), inventory (GaItem), equipment and face data. PS4 saves have no checksums.
- **Binding (LIKELY):** the game rejects saves whose SteamID does not match the logged-in account. Editors therefore rewrite the SteamID field.
- **Runtime identity (FACT):** `GameMan.save_slot` (current slot), `PlayerGameData.character_name` and `GameDataMan`.
- **Persistence design (SPECULATION):** keep EldenNet memories in a sidecar store, e.g. `%APPDATA%\EldenNet\<SteamID>\slot<N>\…` (SQLite/JSONL), keyed by `(SteamID, save_slot, character fingerprint)`. Detect slot reuse and new characters with a fingerprint made of name, creation-time stats and a hash of the first visited flags. An optional stronger link would write a random 32-bit "campaign ID" into an **allocated but unused** event-flag range with `SetEventFlagValue` semantics, so it travels with the `.sl2`. That is risky: flag ranges may be used by other mods, and writes could matter online.

---

## Implications for the AI framework

### v1: minimal and stable

1. **Platform:** a Rust or C++ native DLL loaded by **me3** (`[[natives]]`, `disable_arxan = true`, `start_online = false`, own `savefile`). Refuse to activate if `CSSessionManager` reports a multiplayer session or if the exe version is unknown. Log the detected `2.x.y.z` build.
2. **Version strategy:** resolve singletons with the **FD4Singleton finder**. Resolve the few static and function addresses (`GameDataMan`, `FieldArea`, `GameMan`, `register_task`, `MsgRepositoryImp::GetMessage`) through **AOB + RTTI**. Isolate struct layouts behind a versioned "layout profile" (offset tables per exe build, WW and JP), validated at startup with sanity checks, e.g. `main_player.chr_ins.character_id == 0` and the vftable equal to RTTI `CS::PlayerIns`. If validation fails, **disable features rather than crash**. Either depend on `fromsoftware-rs` (most current, but note the NON-AI license and community stance) or vendor the minimal structs. *(Added after fact-check.)* Depending on fromsoftware-rs **as-is** means the mod hard-fails (panics inside `rva::get()`) on every new exe and drops support for older ones. If you use it, (a) never call RVA-backed APIs before your own version gate, (b) wrap startup so a panic disables EldenNet instead of crashing the game, and (c) prefer a thin fork or vendored subset that resolves `register_task`, `GameDataMan`, `GameMan` and `FieldArea` with **runtime** AOB scans of the same `mapper-profile.toml` patterns. On licensing: crates.io 0.14.0 and git commits up to `acc9da0` (2026-09-21, which already includes 1.17.1 RVAs) are under plain `MIT OR Apache-2.0`, while `main` from 2026-10-12 carries the NON-AI files. Get the license question answered before vendoring post-2026-10-12 code.
3. **Game-thread discipline:** run one perception task (≈4 Hz) in `ChrIns_PostPhysics` that copies a POD snapshot. All LLM/TTS/STT work runs off-thread. One "apply" task drains a command queue on the main thread.
4. **Perception model:**
   - Player: name, HP/FP, position, BlockId, `map_place_name_id` → PlaceName text, `WorldAreaTime` clock.
   - Nearby characters from `chr_inses_by_distance` within R: name (NpcParam.nameId → NpcName), creature type (model ID → name table), `team_type`, `chr_type`, HP%, dead bit, `last_hit_by`, distance.
   - Curated event flags (bosses, NPC quest-state 3000–4999 range, graces) translated to facts with a static table.
5. **NPC tiers:** "important" = has `talkID` / named `nameId` / non-zero `event_entity_id`, plus a curated per-NPC persona file keyed by `npc_param_id` or entity ID. "Generic" = everything else, given only {creature type from model/NpcName, BlockId/place, time, combat events}.
6. **Dialogue I/O, ranked by robustness:**
   - **(a) ImGui overlay** (hudhook DX12) for the player text box, subtitles and bark text. Zero dependence on game UI internals, and it works across patches.
   - **(b) Native subtitle reuse:** hook `GetMessage` and answer a reserved TalkMsg ID range with generated text. Then trigger `TalkToPlayer` on a TalkParam row that points to that msgId, via synthetic `TalkScript`/ESD invocation. This gives vanilla subtitle styling and NPC talk animation (SPECULATION: needs a TalkParam row with no `voiceId` and a sane `timeout`, and testing that `TalkToPlayer` works outside the NPC's own ESD). *(Added.)* TalkParam rows live in `regulation.bin`, which v1 should not ship (§8.1). fromsoftware-rs exposes `get`/`get_mut` on **existing** rows (`SoloParamRepository::get_mut::<P>(id)`, `FD4ParamRepository::get_mut`) but no row insertion. So the native path probably means mutating an existing, otherwise unused TalkParam row in memory (its `msgId`/`timeout`) on the game thread, not adding rows (SPECULATION).
   - **(c) Native prompt:** `CSActionButtonMan::present_action_button` to show a "Talk" prompt near important NPCs.
   - **Audio:** play TTS through your own WASAPI/XAudio2 output with distance attenuation computed from Havok positions. Injecting PCM into Wwise is a later research item. Vanilla voice lines play through `TalkParam.voiceId`.
7. **No actions in v1:** never call ESD commands that mutate state (`SetEventFlag`, `AwardItemLot`, `OpenRegularShop`) except dialogue presentation. This keeps saves and quests safe.
8. **Memory:** a sidecar DB keyed by SteamID + `GameMan.save_slot` + fingerprint. NPC keys are `(event_entity_id | npc_param_id, home BlockId)`, never pointers or handles.

### Later

- Event-flag change hook (TGA AOB) for real-time quest awareness. Damage hooks (`DmgMan_*` phase) for richer combat barks.
- ESD-driven NPC "actions" (face player, animations via `TurnCharacterToFaceEntity`/`RequestAnimation`, give items) behind an allow-list.
- Lipsync via `TalkParam.lipSyncStart/lipSyncTime` or behavior variables. Wwise injection of TTS for in-world 3D audio.
- Line-of-sight checks with `cs_phys_world_cast_ray`. Reading `CSTargetSearchSys` for "the enemy noticed you" barks.
- An offline asset pipeline: dump MSB `PartEnemy` (talkID/npcParamID/entityID per map), NpcName/PlaceName/TalkMsg FMGs, and vanilla dialogue per talk ESD. These become a persona and lore RAG corpus for important NPCs.

---

## Risks

- **Patch churn:** RVAs changed in all of the last 4 patches (≈all RVAs in 1.17.0 and 1.17.2), and struct layouts changed in 2 of them (1.17.0 and 1.17.2), including a bug-fix patch. Silent misreads (wrong offset, plausible garbage) are worse than crashes, so validate aggressively.
- **Upstream single-version policy (added):** fromsoftware-rs `panic!`s on any exe other than the current WW/JP pair, and crates.io lags git (0.14.0 = 1.16.2). Steam auto-updates break the mod on patch day unless EldenNet has its own version gate and runtime scanning.
- **Regional exes:** separate JP builds (`x.y.z.1`) double the version matrix.
- **Arxan:** hooks in protected regions can be reverted or crash without `dearxan`. Prefer task registration over code patches.
- **EAC and online:** any DLL requires EAC off. Users who go online modded risk bans. The framework must hard-disable in multiplayer and default to offline.
- **Mod coexistence:** ERR, Convergence and Seamless Co-op also hook `GetMessage`, menus and saves. Hook chaining, interior AOBs and save-name isolation are required.
- **Thread safety:** game structures have no locks for external readers. Reading off-thread risks torn or stale data and use-after-free on unloaded `ChrIns` (backread/omission unloads).
- **Licensing and community:** fromsoftware-rs moved to "NON-AI" MIT/Apache variants (2026-10-12; Cargo.toml still says `MIT OR Apache-2.0`). The text targets AI-training datasets, but the Apache variant's "any use that contributes to the training or development of an AI model or algorithm" is broad and untested. Whether an LLM-*inference* mod is covered is a legal question, not a technical one. vswarte's rewwise uses the same NON-AI files. me3 forbids LLM use in its own code, issue tracker, discussions and Discord. That governs contributions and support requests, not which DLLs users load. An LLM-driven mod may face friction or refusals for upstream contributions and support.
- **Save integrity:** writing flags or anything into `.sl2` can corrupt progress or interact with online play. Keep v1 read-only.
- **Research gaps:** the TalkMsg/TalkParam injection path, `chr_inses_by_distance` coverage and Wwise injection are unverified in game. AOBs cited from third-party mods must be re-validated on 2.7.2.x. MapForGoblins' `GETMESSAGE` was written against 2.6.2.0. The TGA table as a whole targets App 1.17.2, but individual scripts such as the flag logger are not guaranteed to be tested on it.

---

## Open questions

1. Can `TalkToPlayer` be invoked on an arbitrary NPC through a synthetic `TalkScript` without disturbing that NPC's own ESD state machine, and does it show TalkMsg subtitles for a row without `voiceId`?
2. What is the exact `MsgRepositoryImp` slot index for TalkMsg (menu FMG 1) in the runtime repository, and what are its DLC slot numbers? *Partially answered:* MapForGoblins also uses menu-side IDs (32, 2/361/461, 207/375/475) through the same call, and the Smithbox registry gives TalkMsg 1/360/460. So slot 1/360/460 is LIKELY, but it needs an in-game test.
3. Does `chr_inses_by_distance` include all loaded characters (open-field, summons, ghosts), and how often is it rebuilt?
4. How many generic overworld enemies have `event_entity_id == 0`? Is there a stable alternative key (e.g. MSB part name via `CSChrDataModule.msb_parts`)?
5. How does `FieldArea.map_place_name_id` behave inside legacy dungeons and the DLC, and when does it update?
6. Which Havok SDK and Wwise SDK versions does ER 1.17.x use? This matters for any Wwise source-plugin approach to TTS.
7. Is there a cheap, reliable "player is in dialogue with NPC X" signal (`CSPopupMenu.current_talk_id`, `CSMenuManImp.ui_states`, `CSWorldTalkMan`)?
8. Will FromSoftware keep patching after 1.17.2 (Switch 2 parity, more DLC packs), or freeze?
9. Is there an allocated but unused event-flag range safe for a per-campaign ID that no major overhaul uses?

---

## Sources

**Patch level / official**
- Bandai Namco patch notes 1.17 and 1.17.2: https://www.bandainamcoent.com/news/elden-ring-patch-notes-version-1-17 · https://en.bandainamcoent.eu/elden-ring/news/elden-ring-patch-notes-version-117 · https://en.bandainamcoent.eu/elden-ring/news/elden-ring-patch-notes-version-1172 (search snippets)
- RPG Site 1.17: https://www.rpgsite.net/news/21216-elden-ring-patch-1-17-notes-balance-adjustments-tarnished-pack-dlc · GamerFuzion 1.17.2: https://www.gamerfuzion.com/elden-ring-patch-1-17-2-notes-piquebone-arrow-status-exploit-fixed/ · Vice 1.17.1: https://www.vice.com/en/article/elden-ring-update-today-patch-1-17-1-notes/ · DSOGaming 1.16.1: https://www.dsogaming.com/patches/elden-ring-title-update-1-16-1-released-full-patch-notes/ · rutab.net (1.16.2): https://rutab.net/b/video-games/2026/05/28/dlya-elden-ring-vyshlo-mayskoe-obnovlenie-2026-goda.html
- FromSoftware Tarnished Edition postponement (2025-10-23): https://www.fromsoftware.jp/ww/pressrelease/20251023_eldenring_tarnishededition_postponement.html · Game Informer (Aug 2026 release): https://gameinformer.com/2026/06/04/elden-ring-tarnished-edition-for-nintendo-switch-2-releases-in-august
- PC requirements (DX12 FL12.0): https://www.gematsu.com/2022/02/elden-ring-pc-system-requirements-announced

**Code (read-only clones, commit noted)**
- vswarte/fromsoftware-rs @5702315 (2026-10-24): https://github.com/vswarte/fromsoftware-rs: `crates/eldenring/src/{rva.rs, rva/rva_ww.rs, cs/task.rs, cs/world_chr_man.rs, cs/chr_ins.rs, cs/chr_ins/module*.rs, cs/event_flag.rs, cs/field_area.rs, cs/block_id.rs, cs/team_relation.rs, cs/targeting.rs, cs/talk_script.rs, cs/menu_man.rs, cs/session_manager.rs, cs/game_man.rs, cs/game_data_man.rs, cs/solo_param_repository.rs, fd4/param_repository.rs, position.rs, util/system.rs}`, `crates/eldenring/mapper-profile.toml`, `crates/shared/src/{static.rs, task.rs, arxan.rs}`, `examples/{apply-speffect, invoke-esd, present-action-button}`, `tools/param-generator/params/eldenring/*.xml`, `LICENSE-*-NON-AI`; commits `0c3c4d3`, `acb2a19`, `b080915`, `29fd138`, `b726336`, `5baf0dd`
- Dasaav-dsv/libER @d8ae5c9 (2025-04-24): https://github.com/Dasaav-dsv/libER (`README.md`, `symbols/singletons.csv`, `include/coresystem/task.hpp`)
- Dasaav-dsv/from-singleton: https://github.com/Dasaav-dsv/from-singleton
- The Grand Archives CT @393c439 (2026-10-22, "App ver. 1.17.2"): https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA (`table_files/include/tga/fd4_singleton.h`, `Event Flags/Event Flag Manager/.cea`, `Event Flag Logger.cea`)
- garyttierney/me3 @5f4fa47: https://github.com/garyttierney/me3 (`CHANGELOG.md` v0.13.0, `docs/configuration-reference.md`, `schemas/mod-profile.md`, `crates/mod-host/src/lib.rs`, `AI_CODE_POLICY.md`) · docs https://me3.help/en
- soulsmods/ModEngine2 (discontinued notice): https://github.com/soulsmods/ModEngine2
- tremwil/dearxan: https://github.com/tremwil/dearxan
- veeenu/hudhook: https://github.com/veeenu/hudhook
- quentin452/ERR-MapForGoblins-DLL @60a63f2: https://github.com/quentin452/ERR-MapForGoblins-DLL (`src/goblin_messages.cpp`, `src/re_signatures.hpp`)
- thefifthmatt/ESDLang @9fa7fa9: https://github.com/thefifthmatt/ESDLang (`dist/ESDScriptingDocumentation_Talk.json`)
- AinTunez/DarkScript3 @733f024: https://github.com/AinTunez/DarkScript3 (`DarkScript3/Resources/er-common.{emedf.json,MapName.txt,CharaName.txt,ModelName.txt}`)
- vawser/Smithbox @5827c4c: https://github.com/vawser/Smithbox (`src/Andre/SoulsFormats/SoulsFormats/Util/SFUtil.cs`, `.../Formats/DCX.cs`, `Documentation/Binary Templates/MSB/MSBE/Part.bt`, `Smithbox.Release/Output/Assets/FMG/ER/Fmg Descriptor Registry.json`, `.../PARAM/ER/Param Enums/TEAM_TYPE.json`, `Smithbox.Release/Output/EldenRingDictionary.txt.json`)
- soulsmods/elden-ring-eventparam: https://github.com/soulsmods/elden-ring-eventparam (rendered: https://soulsmods.github.io/elden-ring-eventparam/)
- oisis/EldenRing-SaveForge @ee1042d: https://github.com/oisis/EldenRing-SaveForge (`backend/core/{save_manager.go, offset_defs.go, regulation.go, crypto.go}`, `backend/deploy/target.go`)
- katalash/DSLuaDecompiler: https://github.com/katalash/DSLuaDecompiler
- vswarte/rewwise: https://github.com/vswarte/rewwise
- Nordgaren/Erd-Tools: https://github.com/Nordgaren/Erd-Tools
- Pear0533/CMI: https://github.com/Pear0533/CMI
- techiew/EldenRingEacToggler: https://github.com/techiew/EldenRingEacToggler

**Added during fact-check**
- crates.io API, `eldenring` crate versions/licenses: https://crates.io/api/v1/crates/eldenring (0.14.0 = 2026-05-29, `MIT OR Apache-2.0`)
- FrankvdStam/SoulSplitter `src/SoulMemory/EldenRing/Boss.cs` (independent boss-flag list): https://github.com/FrankvdStam/SoulSplitter
- Dasaav-dsv/from-singleton README (10–15 ms init; tested ER 1.16): https://github.com/Dasaav-dsv/from-singleton
- tremwil/dearxan README: https://github.com/tremwil/dearxan
- KamiyamaShiki0704/ERSoundBankLoader (Wwise `LoadBankMemoryCopy`): https://github.com/KamiyamaShiki0704/ERSoundBankLoader · marieismywaifu/BNKEditor: https://github.com/marieismywaifu/BNKEditor
- Bandai Namco "ELDEN RING Tarnished Edition – Patch 1.17.2" (snippet): https://www.bandainamcoent.com/news/elden-ring-tarnished-edition-patch-1-17-2 · gaming-st.com 1.17.2 summary (snippet, App 1.17.2 / Regulation 1.17.3): https://gaming-st.com/news/eldenring-ver1172-update/ · mp1st 1.17.1: https://mp1st.com/title-updates-and-patches/elden-ring-version-1-17-1-out-minor-update-1-018-001 · eldenring.wiki.gg Game Versions: https://eldenring.wiki.gg/wiki/Game_Versions
- Smithbox `Smithbox.Release/Output/Assets/PARAM/ER/Param Enums/TEAM_TYPE.json` (fetched raw): https://raw.githubusercontent.com/vawser/Smithbox/main/Smithbox.Release/Output/Assets/PARAM/ER/Param%20Enums/TEAM_TYPE.json

**Community docs (search snippets only; not directly fetchable here)**
- Souls Modding wiki ER reference: https://soulsmodding.com/doku.php?id=er-refmat%3Amain · EMEVD intro: https://soulsmodding.wikidot.com/tutorial:intro-to-elden-ring-emevd
- Windows Central, EAC/offline: https://windowscentral.com/how-disable-anti-cheat-elden-ring
- HkbEditor first steps (HKLib/ER): https://ndahn.github.io/HkbEditor/howto/first_steps
- Steam forum on "Dantelion" naming: https://steamcommunity.com/app/1245620/discussions/0/3488627261051077403

---

## Verification log

Adversarial fact-check, 2026-11-06. "Confirmed" means re-derived from primary code or data in this pass (local clones matched upstream HEAD via `git ls-remote` for fromsoftware-rs `5702315`, libER `d8ae5c9`, me3 `5f4fa47` and TGA `393c439`). "Corrected" means the original statement was wrong or misleading and has been fixed in place. "Unverified" means it could not be independently checked here (the web-search budget ran out and bandainamcoent.com/gaming-st.com were not fetchable).

| # | Claim | Verdict | Evidence |
|---|---|---|---|
| 1 | App 1.17.2 = exe 2.7.2.0 (WW) / 2.7.2.1 (JP); fromsoftware-rs commit `b726336` on 2026-10-20 | **Confirmed**, with one nuance: the value is the PE *product* version (`dwProductVersion`) | `git show b726336` (ividyon, 2026-10-20 13:55 +0200; `Ww2720`/`Jp2721`); `crates/shared/src/game_version.rs`; TGA README "Game: App ver. 1.17.2"; https://www.bandainamcoent.com/news/elden-ring-tarnished-edition-patch-1-17-2 (snippet: 2026-10-20) |
| 2 | Commit `b080915` (2026-08-28) inserted an 8-byte field at `ChrIns+0x3b8`, shifting later fields | **Confirmed**. **Corrected** the TL;DR claim that `ChrIns` "grew by 8 bytes": the tail block shrank 0x38→0x30, so the modeled size is unchanged | `git show b080915 -- crates/eldenring/src/cs/chr_ins.rs` (https://github.com/vswarte/fromsoftware-rs/commit/b080915) |
| 3 | Commit `5baf0dd` (2026-10-21) shifted `chr_cam`, `chr_inses_by_distance` by +0x10 | **Confirmed** (also `debug_chr_creator`, `chr_inses_by_update_priority`, omission budgets; `chr_cam` 0x1ece0→0x1ecf0) | `git show 5baf0dd` (Dasaav, 2026-10-21 00:47 +0200) |
| 4 | ER uses Wwise, not FMOD; libER lists `MoWwiseMan`, `CSFD4MoWwisebankRepository`, `FilePackageLowLevelIOBlocking` | **Confirmed** | libER `symbols/singletons.csv` lines 3/4/52/185; independent: https://github.com/KamiyamaShiki0704/ERSoundBankLoader (Wwise `LoadBankMemoryCopy`), https://github.com/vswarte/rewwise |
| 5 | TGA FD4Singleton finder matches null-check code referencing the Japanese string and a path ending `FD4Singleton.h` | **Corrected**: the code shape includes the `lea r8,[string]`, but that operand is wildcarded and never compared. Only the path suffix (`strcmp(... - 14, "FD4Singleton.h")`) and section ranges are validated | TGA `table_files/include/tga/fd4_singleton.h` lines 21–66 |
| 6 | `register_task` found by AOB `e8 ? ? ? ? 48 8b 0d ? ? ? ? 4c 8b c7 8b d3 e8 $ { ' }`; 6 CSTaskRunners | **Confirmed**, with one **clarification**: the AOB is used offline by `binary-mapper`, and runtime uses a fixed RVA (`rva::get().register_task`, panics on an unknown exe). `task_runners: [_; 6]` and `task_runners_ex: [_; 6]` | `crates/eldenring/mapper-profile.toml:326-328`; `cs/task.rs:174-190, 219-221` |
| 7 | `WorldChrMan.chr_inses_by_distance` is documented as "sorted by distance to the main player" | **Confirmed**. Added a caveat: the entry is `{chr_ins, distance: f32, _unkc}`, and whether the distance is squared is unknown | `cs/world_chr_man.rs:101-102, 176-180` |
| 8 | `GetMessage(repo, 0, fmgSlot, msgId) -> wchar_t*/NULL`; NpcName 18/328/428, PlaceName 19/329/429 | **Confirmed** (for exe 2.6.2.0; MapForGoblins predates 1.17) | MapForGoblins `src/goblin_messages.cpp:34-75`, `src/re_signatures.hpp:144-151`; Smithbox `Fmg Descriptor Registry.json` (18/328/428 "Characters", 19/329/429 "Locations") |
| 9 | Flag lookup: group = id/1000, 125-byte blocks, bit = 7 − (id % 8) | **Confirmed**. Added caveats: missing groups read as `false`, the helper hard-codes 1000 while the engine stores `event_flag_divisor`, and `set_flag` is not networked | `cs/event_flag.rs:15-24, 47, 60-84, 117-118` |
| 10 | Godrick defeated = 10000800; `m60_42_36_00` owns 1042360000–1042369999 with conversation sub-ranges 0700/2700 | **Confirmed** for 10000800 (independent source) and for the 0700/2700 sub-ranges. The whole-range "ownership" is downgraded to LIKELY | SoulSplitter `src/SoulMemory/EldenRing/Boss.cs:22,88`; TGA `Event Flag Manager/.cea:1860`; elden-ring-eventparam `index.md` `EFID_m60_42_36_00` |
| 11 | ESD command 1 = `TalkToPlayer(talkParamRow, unk, unk, eventFlagId)`; TalkParam has msgId, voiceId, lipSyncStart/Time, timeout, isForceDisp | **Confirmed** (ESDLang: games ds3/er/ac6, unk args "use -1", flag "usually 0"; paramdef fields present) | ESDLang `dist/ESDScriptingDocumentation_Talk.json` (bank 1 id 1); fromsoftware-rs `tools/param-generator/params/eldenring/TalkParam.xml` |
| 12 | PC `ER0000.sl2` = raw BND4, 10 slots × 0x280000, each with an MD5 prefix; `USER_DATA010` holds the SteamID | **Confirmed** (UserData10 = 0x60000 + MD5; UserData11 has no MD5). Per SaveForge only; no second independent parser checked | SaveForge `backend/core/save_manager.go:23-45, 99-106, 155-199` |
| 13 | `regulation.bin` = AES-256-CBC over DCX over BND4 (`SFUtil.DecryptERRegulation`) | **Confirmed** (16-byte IV prefix, `KeySize = 256`, `CipherMode.CBC`, then `BND4.Read`). The save-embedded copy uses DCX DFLT or ZSTD | Smithbox `SoulsFormats/Util/SFUtil.cs:563-580, 864-884`; SaveForge `backend/core/regulation.go:19-36` |
| 14 | ModEngine2 discontinued → me3; me3 latest release v0.13.0 (2026-08-11) with `disable_arxan` | **Confirmed** (the newest tag on `git ls-remote --tags` is v0.13.0; `disable_arxan` since 0.8.0). Added that `main` has unreleased fixes (dearxan 0.5.7, 2026-10-12) | ModEngine2 README (commit `4f701cc`, 2024-07-19); me3 `CHANGELOG.md`, `docs/blog/posts/arxan-reversing-1.md:23`, `crates/mod-protocol/src/lib.rs:109-166` |
| 15 | fromsoftware-rs → "MIT NON-AI"/"Apache NON-AI" on 2026-10-12; me3 `AI_CODE_POLICY.md` disallows all LLM use | **Confirmed** (commits `603814c`, `6933967`). **Corrected** the scope: the me3 policy covers me3's code, issue tracker, discussion board and Discord, and does not restrict the mods it loads. The NON-AI text targets AI training datasets | `LICENSE-MIT-NON-AI` §1–3, `LICENSE-ASL2-NON-AI` §2.1–2.2; me3 `AI_CODE_POLICY.md` |
| 16 | "fromsoftware-rs v0.14.0, RVAs tracked to 1.17.2" | **Corrected**: crates.io 0.14.0 (2026-05-29) matches only 2.6.2.0/2.6.2.1 (1.16.2); 1.17.x support exists only on unreleased git `main` | https://crates.io/api/v1/crates/eldenring; `git show 9028518:crates/eldenring/src/rva.rs` |
| 17 | fromsoftware-rs supports old game versions alongside new ones (implied by "keep old-version support") | **Refuted** for fromsoftware-rs: each patch replaces the match arm, and unknown versions `panic!` | `crates/eldenring/src/rva.rs` (`get()` → `unwrap_or_else(\|e\| panic!("{e}"))`); `git show b726336` |
| 18 | "~120 tracked RVAs", "≈all changed" in 1.17.0/1.17.2 | **Corrected** to 107 RVAs: 104 changed in 1.17.0 and 99 in 1.17.2 | `rva/bundle.rs` (107 `u32` fields); `git show acb2a19/b726336 -- rva_ww.rs` |
| 19 | "~190 `CSTaskGroupIndex` phases" | **Corrected** to 169 | `cs/task.rs` enum `CSTaskGroupIndex` (FrameBegin … FrameEnd) |
| 20 | "RVAs and layouts changed in 3 of the last 4 patches" | **Corrected**: RVAs changed in all 4 (1.16.2: ~7; 1.17.1: 5); layouts changed in 2 (1.17.0, 1.17.2) | git history `0c3c4d3`, `acb2a19`, `b080915`, `29fd138`, `b726336`, `5baf0dd` |
| 21 | `invoke-esd` builds a `TalkScript` bound to an NPC | **Corrected**: the example binds it to the **main player's** handle and uses only menu/shop commands; `TalkToPlayer` on an NPC is unproven | `examples/invoke-esd/src/lib.rs:214-240` |
| 22 | ER talk vocabulary ~90 commands, ~82 functions | **Confirmed** (90 distinct command IDs, 82 function IDs with `er` in `games`/`games_used`) | ESDLang `dist/ESDScriptingDocumentation_Talk.json` |
| 23 | `SoloParamRepository` indices NpcParam=6, NpcThinkParam=9, TalkParam=18, PlayRegionParam=38, WorldMapPlaceNameParam=90 | **Confirmed** | `cs/solo_param_repository.rs:548-664` |
| 24 | `ChrIns` field set (npc_param_id, character_id, chr_type Npc=5 / WhiteSummonNpc=19, team_type u8, event_entity_id, last_hit_by, dead flag in `ChrInsFlags1c5`) and module list (ai/talk/damage/magic opaque) | **Confirmed** | `cs/chr_ins.rs:122-237, 440-460, 1040-1060`; `cs/chr_ins/module.rs:42-82` |
| 25 | TalkMsg FMG = menu 1; DLC TalkMsg slots | **Confirmed** for menu 1; **added** DLC 360/460 (LIKELY as runtime slots) | Smithbox `Fmg Descriptor Registry.json` |
| 26 | TEAM_TYPE names (6 Enemy, 7 Boss, 14 Neutral, 26 Friendly NPC, 27 Hostile NPC, 29 Indiscriminate) | **Confirmed** (community labels) | Smithbox `PARAM/ER/Param Enums/TEAM_TYPE.json` (raw fetch) |
| 27 | Global flags 3000–4999 for character state; 100–199 story; 500–599 time/weather; 9100–9199 boss kills | **Confirmed** | elden-ring-eventparam `index.md` `EFID_common` (the group row at 3000, FN 2000, covers the character state flags) |
| 28 | TGA comments 3269 (Gostoc hidden after Godrick), 3468/3469 Sellen; flag-logger AOB `33 d2 41 8b c2 41 f7 f1 41 8b d8` | **Confirmed** | TGA `Event Flag Manager/.cea:1865, 2135-2136`; `Event Flag Logger.cea:160` |
| 29 | dearxan "fully neuters Arxan" | **Confirmed with caveat**: the footnote says the entry-point stubs still run; it is tested on the latest game versions | https://github.com/tremwil/dearxan README |
| 30 | EAC must be off for DLL mods → offline only | **Confirmed** (EAC toggler: "disabling EAC forces the game into offline mode"; me3 `start_online` default off) | https://github.com/techiew/EldenRingEacToggler; me3 `docs/configuration-reference.md:40` |
| 31 | 1.17.1 date "on or before 2026-09-08" | **Unverified** beyond press dated 2026-09-08 (mp1st, Vice) and commit `29fd138` | mp1st snippet |
| 32 | 1.16 released Oct 2024 | **Unverified**: snippets conflict (eldenring.wiki.gg Oct 17 2024 vs one outlet dating a 1.16 note to July 2025) | eldenring.wiki.gg "Game Versions" (snippet) |
| 33 | 1.16.2 ~2026-05-28 | **Unverified** beyond commit `0c3c4d3` (2026-05-28) and an OpenCritic "May 2026 Official Update Announced" snippet | search snippet |
| 34 | D3D12 FL 12.0 requirement; Havok/Scaleform/Bink | **Unverified this pass** (not re-searched; budget exhausted). Havok/Scaleform singletons re-confirmed in libER `singletons.csv` | libER `symbols/singletons.csv` |
| 35 | Lua 5.0 for ER AI scripts | Downgraded to **LIKELY** (DSLuaDecompiler's statement is generic, "almost all FromSoft games") | https://github.com/katalash/DSLuaDecompiler README |
