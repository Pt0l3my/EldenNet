# Elden Ring PC Modding Ecosystem and Toolchain for an LLM NPC Framework

*Research date: 2026-11-06. Scope: ELDEN RING (Steam app 1245620) on PC, patch 1.17.2, including Shadow of the Erdtree. Target: an in-process native DLL that gives NPCs LLM dialogue, modeled on SkyrimNet.*

**Evidence labels.** **FACT** means checked against a primary source: repository source code, an official README, or a patch notice. **LIKELY** means several weaker signals agree, such as search-result snippets, forum posts, or Nexus pages seen only through search. **SPECULATION** means reasoned design inference.

**Access limits.** From this research environment, github.com and raw.githubusercontent.com could be fetched. Nexus Mods, wiki.gg, Fextralife, Bandai Namco, soulsmodding.com, docs.rs and Reddit could not be fetched directly. Claims from those sites come from search-result snippets and are labeled LIKELY. Repository claims were checked against shallow clones read on 2026-11-06. Commit hashes are given where they matter.

**Fact-check pass (2026-11-06).** An adversarial review re-checked the flagged claims against fresh `git ls-remote`/`git fetch` of each repository, the crates.io API and independent search results. Corrections are made inline. Every change and its evidence is listed in the [Verification log](#verification-log) at the end. The biggest corrections: (1) struct layouts, not only RVAs, shifted in 1.17.0 and 1.17.2; (2) a panic inside a game-task closure aborts the game whatever panic profile we choose; (3) `from_singleton` is documented as tested only on ER 1.16, and several fromsoftware-rs statics are found through RVAs anyway; (4) patch timing was bursty, not "every 4–8 weeks", and 1.17.1 shipped on 2026-09-08; (5) Waygate is AGPL-3.0; (6) the "hostile to AI" framing was overstated.

---

## TL;DR

- **The game is still being patched, and every patch breaks native mods.** Four PC builds shipped in 2026: 1.16.2 (May 28), 1.17.0 (Aug 27, for the "Tarnished Pack" DLC), 1.17.1 (Sep 8) and 1.17.2 (Oct 20). These came after a nine-month gap with no patch, so the pace is bursty, not steady. The current executable's version resource reads **2.7.2.0** (Steam 1.17.2). fromsoftware-rs checks `VS_FIXEDFILEINFO.dwProductVersion`, and er-mods-rs calls it `FileVersion`. Function addresses (RVAs) move on every patch. For example, `CS::MsgRepository::GetAndFormat` moved from RVA `0x7633d0` (1.16.2) to `0x7646d0` (1.17.2). **Struct layouts also move.** `ChrIns` fields shifted by 8 bytes in 1.17.0, and `WorldChrMan.chr_inses_by_distance` shifted by 0x10 around 1.17.2.
- **EAC rule: any modded client must stay offline.** Launching `eldenring.exe` directly, instead of the EAC bootstrapper `start_protected_game.exe`, means EAC does not run and the game is offline. me3 does this by default and also forces the game property `Menu.IsEnableOnlineMode=false`. Our framework must never join FromSoftware's servers. Seamless Co-op (its own Steam-lobby P2P) and private servers (Waygate, Reforged 2.0's servers) are separate ecosystems.
- **Use me3 as the loader.** ModEngine2 was archived in July 2024 and its README says work moved to me3. me3 v0.13.0 shipped 2026-08-11 and is still active in October 2026. It loads DLLs from TOML `.me3` profiles via `[[natives]]` entries with `optional`, `load_early` and `initializer` options. It can also give modded play its own save file (`savefile`), neutralize Arxan (`disable_arxan`), and run under Proton. Elden Mod Loader (a dinput8 proxy) is legacy, and the ERSC documentation site says it cannot load ERSC.
- **Use Rust and `fromsoftware-rs`.** The `eldenring` crate (vswarte et al.) is the only maintained, typed in-process SDK we found. It covers:
  - characters: `WorldChrMan` (including `main_player` and `chr_inses_by_distance`), `ChrIns`, `PlayerIns`
  - game data: `CSEventFlagMan`, `SoloParamRepository::get::<P>()` with generated param structs, `WorldAreaTime`
  - talk scripts: ESD/TalkScript invocation
  - game-loop tasks: `CSTaskImp::run_recurring`
  - debug drawing and input: `CSEzDraw`, `DLUserInputManager`
- **fromsoftware-rs is pinned to exact builds.** The main branch accepts only WW `2.7.2.0` and JP `2.7.2.1`, and **panics** on anything else. The first panic comes as early as `CSTaskImp::wait_for_instance`. The latest crates.io release, v0.14.0 (May 29, 2026), only supports 1.16.2 (`2.6.2.0`). Most manager singletons are found by `from_singleton` pattern scanning plus DLRF metadata. That scanner's README lists only "ER 1.16" as tested, though it evidently still works on main. Some statics (`GameDataMan`, `GameMan`, `FieldArea`, `DLUserInputManagerImpl`) and all functions, including task registration, use per-build RVAs generated offline from 33 AOB patterns and 74 RTTI vtables. **Struct field offsets are compiled into the crate and are also per-build.**
- **libER (C++20) is stale and narrow.** Its last game update was for 1.16, in April 2025, and it has no character headers. Erd-Tools, the Elden Ring Debug Tool and the TGA Cheat Engine table read memory from outside the process. They are useful for reverse-engineering but cannot host the framework.
- **The overlay is solved; keyboard input is the trap.**
  - The game is D3D12. `hudhook` 0.9.3 (Rust, September 2026) hooks DXGI and D3D12 with MinHook and subclasses the WndProc.
  - The game reads keyboard and mouse through **DirectInput8** (`IDirectInputDevice8W::GetDeviceState`) and pads through XInput or libScePad. Blocking WndProc messages is therefore not enough to stop typing from moving the character.
  - fromsoftware-rs already ships an `InputBlocker` that hooks DirectInput. It sits in the workspace-only `debug` crate ("not intended to be released to Crates.io") and uses `ilhook`, not MinHook. er-mods-rs independently confirms the problem: "ELDEN RING reads the mouse through DirectInput, which never touches the window procedure".
- **v1 can work with zero data edits.**
  - NPC identity comes from `ChrIns.event_entity_id` and `npc_param_id`, `NpcParam` rows and runtime FMG lookups. `character_id` is a shared model ID, so it is not an identity key.
  - Text goes to an ImGui overlay, or to native UI by detouring `MsgRepository::GetAndFormat`, which a public mod already does instead of editing FMGs.
  - Talking is started by a hotkey or the native action-button prompt.
  - No `regulation.bin`, `msgbnd`, `talkesdbnd` or EMEVD changes means the framework can coexist with Reforged, Convergence and randomizers.
- **Core infrastructure maintainers are anti-AI, though the wider community is mixed.** me3 bans all LLM use in its code, issue tracker, discussions and Discord. fromsoftware-rs ships "MIT NON-AI" and "Apache NON-AI" license files, which forbid ML training with the code, even though its `Cargo.toml` says `MIT OR Apache-2.0`. Some active 2026 downstream projects, however, are openly built with coding agents: er-mods-rs has `AGENTS.md`, and unseamless-coop has `.agents/skills/`. Plan not to depend on upstream goodwill for AI-specific features, and get the license read before shipping.

---

## 1. Patch level and why it dominates the design

| PC version | Date | Notes | Source |
|---|---|---|---|
| 1.16 | 2024-10-17 | Post-DLC fixes | Bandai Namco patch notes (LIKELY, via search) |
| 1.16.1 | 2025-08-21 | Small bug-fix patch | wiki.gg / mp1st (LIKELY) |
| 1.16.2 (exe `2.6.2.0`) | 2026-05-28 | Stability fixes; fromsoftware-rs v0.14.0 targets it | rutab.net, OpenCritic, vpesports (LIKELY); `rva.rs` at tag `v0.14.0` matches `"2.6.2.0"`/`"2.6.2.1"` (FACT) |
| 1.17.0 (exe `2.7.0.0`) | 2026-08-27 | Supports the "Tarnished Pack" DLC (Aug 28: two new starting classes, armor sets, Torrent skins; wiki.gg also lists new NPCs), balance changes, required for online | rpgsite, pcgamesn, Bandai Namco, Push Square (LIKELY); fromsoftware-rs commit `acb2a19` same day (FACT) |
| 1.17.1 (exe `2.7.1.0`) | 2026-09-08 | Fixes a multiplayer issue where DLC-specific items could be obtained when not eligible; no content | Official @ELDENRING post on X, Vice, MP1st (LIKELY, via search); fromsoftware-rs commit `29fd138` "[ER] Update RVAs for 1.17.1" same day (FACT) |
| 1.17.2 (exe `2.7.2.0`) | 2026-10-20 | Piquebone-arrow exploit fix, visual fixes, PS4/PS5 network security | gamerfuzion, twistedvoxel, ixbt.games 2026-10-21 (LIKELY); fromsoftware-rs commit `b726336` 2026-10-20 (FACT) |

No 1.17.3 or 1.18 was found as of 2026-11-06. A "Patch 1.18 Easy Mode" article on thefpsreview.com is dated 2026-04-01 and is an April Fools piece. fromsoftware-rs `main` (`5702315`) and the TGA table README (`393c439`, 2026-10-22) both still target 1.17.2, so 1.17.2 is LIKELY still current.

**FACT:** The er-mods-rs project documents that PE `FileVersion` 2.7.2.0 is Steam patch 1.17.2 ("installed 2026-10-20"). It also says "nothing translates an address from any other build" (`Banon-Labs/er-mods-rs/docs/game-build.md`).

**FACT:** fromsoftware-rs `crates/eldenring/src/rva.rs` (commit `5702315`, 2026-10-24; still `HEAD` by `git ls-remote` on 2026-11-06) matches only `(LANG_ID_EN, "2.7.2.0")` and `(LANG_ID_JP, "2.7.2.1")`. On any other build, `rva::get()` calls `panic!`. It reads `dwProductVersion` from the fixed version info (`crates/shared/src/game_version.rs`), and it `unwrap()`s the resource lookups too. Each update **replaces** the previous build's bundle; it does not add to it. Upstream therefore supports exactly one build per region at a time.

**FACT (struct layouts move too):** fromsoftware-rs commit `b080915` "[ER] Update some strucs for 1.17" (2026-08-28) inserted a field at `ChrIns+0x3b8`. That shifted `distance_to_player_sqr`, `update_priority`, `hit_material_override` and other fields by 8 bytes, and changed `PlayerGameData` too. Commit `5baf0dd` "Update shifted WorldChrMan fields" (2026-10-21, one day after the 1.17.2 RVA update) moved `chr_cam`, `chr_inses_by_distance` and `chr_inses_by_update_priority` by 0x10. A mod with correct RVAs but stale layouts would read garbage, for example a wrong nearby-NPC list.

**FACT:** The same function lands at a different RVA on each build. er-mods-rs records `MsgRepository::GetAndFormat` at `0x7634c0` (1.16.1 dump), `0x7633d0` (1.16.2) and `0x7646d0` (2.7.2.0) (`crates/er-title-flow/src/constants_autoload_state.rs`). er-mods-rs keeps a 535-entry "detour allowlist" of functions it compared instruction by instruction across builds before porting. That shows how much work each patch costs serious native mods.

**Consequence.** Patches are bursty. Nothing shipped for nine months (2025-08-21 to 2026-05-28), then four builds came in under five months around the Switch 2 "Tarnished Edition" and Tarnished Pack launch. Treat every game update as a breaking change for both addresses and layouts. Detect the build at startup, and turn features off rather than crash when the build is unknown. Players who stay on an older build, for example through a Steam depot downpatch (a Steam Community guide "How to downpatch for mods" exists; LIKELY), will need an EldenNet build that matches their game.

---

## 2. Easy Anti-Cheat, offline play and ban risk

### 2.1 How EAC is bypassed (for offline use only)

- **FACT:** Steam starts `start_protected_game.exe`, the EAC bootstrapper, which then starts `eldenring.exe`. All community methods amount to "run `eldenring.exe` without EAC". The TGA table README lists three:
  1. **Recommended:** put `steam_appid.txt` next to `eldenring.exe` and run `eldenring.exe` directly. The speedrun practice tool applies this "textfile method" itself.
  2. **Compatibility:** LukeYui's Offline Launcher (`launch_modded_eldenring.exe`, [GitHub](https://github.com/LukeYui/launch_modded_eldenring); last commit 2022-03-09). It "launches Elden Ring without Easy Anti-Cheat… will not connect to FromSoftware's matchmaking servers." It is explicitly meant as a base for DLL-injecting launchers.
  3. **Legacy:** rename `eldenring.exe` to `start_protected_game.exe`.
- **FACT:** techiew's EAC Toggler (`toggle_anti_cheat.exe`, Nexus mod 90; last commit 2022-05-03) swaps the bootstrapper. It also ships a dummy `winhttp.dll` proxy, so that if an update restores the real `start_protected_game.exe`, EAC refuses to launch "because of the untrusted system file" instead of starting with mods loaded (`techiew/EldenRingEacToggler` README). **Design note (LIKELY):** for the same reason, if EldenNet is installed as a `winhttp.dll`/`dinput8.dll` proxy and the player starts the game through Steam with EAC, EAC should refuse to start. That is a useful safety property, but it is not guaranteed for every EAC version.
- **FACT:** ModEngine2 and me3 both launch `Game/eldenring.exe` directly (`ModEngine2/launcher/launcher.cpp`; `me3/crates/mod-protocol/src/game.rs`).
- **FACT:** me3 goes further. Unless `start_online` is true, `game_properties::start_offline()` overrides the property `Menu.IsEnableOnlineMode` to `"false"` (`me3/crates/mod-host/src/host/game_properties.rs`, called from `after_game_main` in `crates/mod-host/src/lib.rs`). `start_online` can be set in the profile or with the CLI flag `--online`, and it defaults to `false` (`profile_options.start_online.unwrap_or(false)`, `crates/cli/src/commands/launch.rs`). The docs say it exists for private-server mods such as Waygate and DS3OS and is *not* needed for Seamless Co-op. **Caveat (FACT, code):** user `debug_properties` are applied after me3's internal overrides ("user has priority"). A profile that sets `Menu.IsEnableOnlineMode` itself could therefore LIKELY undo the protection.

### 2.2 Ban risk

- **LIKELY:** Players report soft bans ("Inappropriate activity detected", sometimes described as a 180-day quarantine with other flagged players) for edited saves or illegitimate items taken online. FromSoftware has also acknowledged false positives on PC (pcgamesn, Steam forums; a 2024 Shadow of the Erdtree false-positive wave was covered by Windows Central). When the game runs without EAC it cannot connect and plays offline. This connection failure is separate from the "Inappropriate activity" flag; the exact wording of the message was not verified in this pass. Nothing official describes what is detected.
- **FACT (community consensus in tool READMEs):** TGA: "not meant to be used online and you will most likely be banned". ER Debug Tool: "Backup your saves… restore the backups before going online. Using this tool to skip around the game (for example going to the madness grace before talking to melina or killing margit) will cause you to get banned!" The second sentence suggests detection includes impossible quest or flag progressions. That matters for any later version that writes event flags. Offline Launcher: "Please don't attempt to play online without Easy Anti-Cheat."
- **Rule for our framework:** never support `start_online = true`. Always use a separate save. me3's `savefile` option exists "to avoid save corruption and multiplayer bans" (me3 `docs/configuration-reference.md`). v1 makes no gameplay edits, but a save that has been touched by a modded client should still never go online.

### 2.3 Online-like ecosystems that are not FromSoftware servers

- **Seamless Co-op (ERSC, LukeYui, Nexus 510).** `SeamlessCoop/ersc.dll`, `ersc_settings.ini` (password), save extension `.co2`. It is launched by `ersc_launcher.exe` or loaded as a DLL by ModEngine2 (`external_dlls = [ "SeamlessCoop/ersc.dll" ]`). The ERSC documentation site says: "You MUST launch Seamless Coop with either `ersc_launcher.exe` or `ModEngine2`. DLL injectors like Elden Mod Loader and Lazy Loader won't work" (`ersc-docs/ersc-docs.github.io/pages/seamless-modding.md`, last commit 2026-01-30, FACT for the text). This is a separate docs organization. It could not be confirmed as authored by LukeYui, so treat it as semi-official. It predates me3's spread and does not mention me3. Nexus pages describe loading `ersc.dll` as an me3 `[[natives]]` entry (LIKELY). The mirror's `VERSION` file lists every older version with flag `0` and ends with `>2.02 1`. That LIKELY means only ERSC 2.0.2+ is accepted; the file format is undocumented. er-mods-rs pins ERSC `2.0.2` on game 2.7.2.0 (`build-support/prologue_build.rs`), so ERSC 2.0.2 LIKELY runs on 1.17.2.
- **Waygate** (`vswarte/waygate-server`, **AGPL-3.0-only**, last default-branch commit 2026-07-24): a reimplementation of the matchmaking protocol so modded clients can play together. Its README says client setup is "TBD" and that there is "no guarantee" it is ban-safe.
- **Elden Ring Reforged 2.0** (Nov 8, 2025): added its own servers for summons, invasions and messages (PC Gamer, PCGamesN; LIKELY).
- **Implication:** v1 dialogue is local-only, but it will often run inside ERSC sessions. It must load alongside `ersc.dll` and must not hook the same functions (see §7.4).

---

## 3. Mod loaders

### 3.1 ModEngine2 (ME2)

- **FACT:** Archived 2024-07-19. The final commit is "Add notice of discontinuation", and the README says "Development on Mod Engine 2 is discontinued. Any future work will be done on me3." The last release, `release-2.1.0` ("Armored Core 6 Support"), is dated 01 Sep, which LIKELY means 2023.
- **Mechanism (FACT, `installer/dist/config_eldenring.toml`):**
  - A launcher starts the game pre-configured; dinput8 sideloading is optional.
  - `[modengine] external_dlls = [ ... ]` lists native DLLs.
  - `[extension.mod_loader] mods = [{ enabled, name, path }]` gives file-override roots in game-archive layout, with the highest priority winning. There is no merging, so conflicting `regulation.bin` files are not resolved.
  - `loose_params` is "not supported for Elden Ring".
  - There are optional ScyllaHide and Optick hooks.
  - Extensions can export `modengine_ext_init`.
- **Status:** still bundled by the Reforged launcher (LIKELY: "The Launcher EXE overwrites the ModEngine 2 configuration every time it runs"), still referenced by ERSC docs, and still used by older guides. Treat it as a fallback that should keep working, because our DLL will not depend on ME2 APIs.

### 3.2 me3 ("Mod Engine 3", garyttierney)

- **FACT:** v0.13.0 shipped 2026-08-11 (`CHANGELOG.md`), with commits through 2026-10-13. Licensed MIT/Apache-2.0. Supports DS3, Sekiro, ER, AC6 and Nightreign on Windows, on Linux through Proton (Proton 11 since v0.12.0), and on macOS through CrossOver.
- **Profile format (FACT, `schemas/mod-profile.json`, `profileVersion = "v1"`):**
  - `[[supports]] game = "eldenring"`.
  - `[[packages]]` define file-override roots, with `id`, `enabled`, `load_before` and `load_after`.
  - `[[natives]]` define DLLs, with `path`, `optional` (default false; a failure is otherwise fatal), `enabled`, `load_before`/`load_after`, `initializer` (`{delay={ms}}` or `{function="sym"}`), `finalizer` and `load_early`. **A native's ordering `id` is its DLL file name** (`Dependency for Native::id()` returns `path.file_name()`, `crates/mod-protocol/src/native.rs`). So `load_after = [{ id = "ersc.dll", optional = true }]` is how to order after Seamless Co-op (FACT for the id rule; the exact TOML shape follows the `Dependent {id, optional}` schema).
  - Top-level options: `savefile`, `start_online`, `disable_arxan` (default false), `mem_patch` (default on) with `mem_patch_heap_size`, and `debug_properties`.
- **Launch sequence (FACT, `crates/launcher/src/game.rs`, `crates/mod-host/src/lib.rs`):**
  1. Create `eldenring.exe` with `CREATE_SUSPENDED`.
  2. Inject `me3_mod_host` with `VirtualAllocEx`, `WriteProcessMemory` and `CreateRemoteThread`.
  3. In the host: optionally run `dearxan`, skip intro logos, install VFS overrides and savefile redirection, then defer work to "before game main" and "after game main".
  4. After main: analyze RTTI, FD4 step tables and the DLRF runtime-class registry; apply property overrides including offline mode; hook heap allocators; fix oversized regulation files; then `LoadLibrary` each native. The native list is split at the first entry with an `initializer`. That entry and **every native after it** load on a separate thread. `load_early` natives instead load in `before_game_main`.
- **ME2 compatibility (FACT):** `host.rs` looks for `modengine_ext_init` and calls it through a `ModEngineConnectorShim`, so ME2-style extension DLLs still load.
- **Adoption (LIKELY):** Convergence ships `convergence.me3`, and Nexus guides tell players to add `ersc.dll` and other DLLs as natives. Many recent GitHub mods target me3 only, including er-mods-rs (23 DLLs listed in its README, last commit 2026-10-28), EwdenWing (last commit 2025-12-26) and Glorious Merchant. The Nexus LLM companion mod "Un-Alone" (mod 8046) reportedly installs me3 automatically (search snippet).
- **Known issues (FACT):** antivirus quarantines (FAQ). The partial-audio bug for Convergence/Reforged on Linux (Discussion #582, opened 2025-11-06) was fixed by PR #720 and confirmed working in v0.11.0 (2026-03-01 reply). It is no longer an open issue.
- **Policy (FACT, `AI_CODE_POLICY.md`, titled "LLM Usage Policy"):** "All forms of LLM use are disallowed in this project. That includes our code, issue tracker, discussion board, and Discord server." The policy governs participation in the me3 project. It says nothing about which mods me3 may load, and me3 is MIT OR Apache-2.0. We can *use* me3, but we should not expect help with an AI mod in its spaces, and we must never post LLM-written text there.

### 3.3 Elden Mod Loader (techiew) and other injectors

- **FACT:** A `dinput8.dll` proxy that "loads all DLLs found in the 'mods' folder". Load order comes from `load.txt` or the `[loadorder]` section of `mod_loader_config.ini`, with a one-second stagger between mods. The last push was 2024-08-10. It conflicts with ME2's optional dinput8 mode, and per ERSC docs it cannot load Seamless Co-op. Still used by older single-DLL mods such as PostureBarMod.
- **Other methods (FACT):**
  - Proxy DLLs: EROverlay recommends renaming itself to `winhttp.dll`.
  - Standalone injectors: the practice tool's `jdsd_er_practice_tool.exe`, plus an optional dinput8 install.
  - On Proton, proxy DLLs need `WINEDLLOVERRIDES="dinput8=n,b"` (practice tool README).

**Recommendation:** ship an me3 profile as the main install path. Keep the DLL loader-agnostic (plain `DllMain`, which spawns a thread that waits for game init) so it also loads through ME2 `external_dlls` and `winhttp`/`dinput8` proxies. Because me3 natives load after game main, early hooks are unnecessary for v1.

---

## 4. In-process native libraries

### 4.1 fromsoftware-rs (`eldenring` crate), the primary candidate

- **Repository:** `vswarte/fromsoftware-rs` (formerly `eldenring-rs`). Crates: `eldenring`, `darksouls3`, `sekiro`, `nightreign`, `fromsoftware-shared` (+ `macros`, `stl`), `debug`; tools `binary-mapper`, `param-generator` and `debug-eldenring`. Stable toolchain, edition 2024.
- **Releases (FACT, GitHub releases page and crates.io API):** v0.14.0 (2026-05-29; "Update to ELDEN RING 1.16.2"; `rva.rs` at the tag matches `2.6.2.0`/`2.6.2.1`), v0.13.0–0.13.2 (2026-02-23), v0.12.0 (2026-01-11), and earlier. crates.io `max_version` is still 0.14.0 on 2026-11-06. The main branch moved to 1.17.0 on Aug 27, 1.17.1 on Sep 8, and 1.17.2 on Oct 20, 2026, each on the day of the patch, but no crates.io release has followed. **A 1.17.2 mod must depend on unreleased code:** either a git dependency pinned to a commit, or a vendored or forked checkout. er-mods-rs does the latter. It uses a `path` dependency on a sibling fork checkout, and its CI pins the fork at `4612952`, which lists 2.7.2.0, 2.7.1.0 and JP 2.7.1.1 (`docs/game-build.md`). So a fork *can* carry several builds' bundles at once.
- **Singletons exposed (FACT, `#[shared::singleton(...)]` in `crates/eldenring/src`):** `WorldChrMan`, `WorldChrManDbg`, `CSEventFlagMan`, `CSEventMan`, `CSLuaEventMan`, `SoloParamRepository`, `FD4ParamRepository`, `MsgRepository`, `MsbRepository`, `CSMenuMan`, `CSFeMan`, `CSActionButtonMan`, `CSCamera`, `CSFade`, `CSTask`, `CSTaskGroup`, `CSWindow`, `RendMan`, `WorldAreaTime`, `CSWorldGeomMan`, `CSBulletManager`, `CSHavokMan`, `CSSessionManager`, `CSNetMan`, `FD4PadManager`, `CSMouseMan`, `CSGaitem`, `MapItemMan`, `CSSfx`, `WorldSfxMan`, `CSTrophy`, `CSNowLoadingHelper`, and others.
- **Characters (FACT, `cs/world_chr_man.rs`, `cs/chr_ins.rs`):**
  - `WorldChrMan` provides `main_player: Option<OwnedPtr<PlayerIns>>`, `chr_inses_by_distance: DLVector<ChrInsDistanceEntry{chr_ins, distance}>`, `chr_inses_by_update_priority`, the `chr_sets[196]` array, `chr_ins_by_handle(&FieldInsHandle)`, and a debug spawner.
  - `ChrIns` provides `field_ins_handle`, `npc_param_id`, `npc_id`/`character_id`, `chr_type`, `team_type`, `event_entity_id`, `last_hit_by: FieldInsHandle`, `special_effect`, `is_locked_on`, `lock_on_target_position`, `block_id`, and `modules`. The modules include `data` (hp, max_hp, fp, stamina), `physics` (position, orientation), behavior, time_act, toughness, super_armor and others.
  - `PlayerIns` adds `player_game_data` and `npc_param`.
  - **Identity caveat (FACT for the doc comment, LIKELY for the consequence):** `npc_id`/`character_id` is documented as the "4 number identifier… eg. 8000 for Torrent", that is, the character **model** ID (`cXXXX`). Humanoid NPCs widely share the human model c0000, so `character_id` cannot tell named humans apart. Use `event_entity_id` (the MSB entity ID of placed characters) and `npc_param_id` for identity. A single named NPC may also use several NpcParam rows across quest stages or hostility states (SPECULATION; verify against paramdex names).
  - **These fields cover most of what v1 perception needs without hooks (LIKELY):** a nearby-NPC list, the last attacker (`last_hit_by` holds only the most recent hitter), HP deltas and team *types*. Gaps: no typed AI target or aggro state, no "noticed the player" signal, and hostility must be derived from `team_type` pairs. `chr_inses_by_distance` is the game's update-priority list, so it covers loaded characters only.
- **World and data (FACT):**
  - `CSEventFlagMan::get_flag/set_flag(EventFlag)` reads quest progress.
  - `SoloParamRepository::get::<P: SoloParam>(id)` returns typed rows from `param/generated.rs`, generated from community paramdefs. `NpcParam`, `NpcThinkParam` and `TalkParam` are relevant here.
  - `WorldAreaTime::request_time` exists, and the clock can be read.
- **Talk and UI (FACT):**
  - `TalkScript::new(map_id, talk_id, field_ins_handle)` with `event()`/`env()` drives the game's own talk-menu primitives (`AddTalkListData`, `ShowShopMessage`, `GetTalkListMenuResult`; see `examples/invoke-esd`).
  - `CSActionButtonManImp::present_action_button(id)` shows a native "press to interact" prompt (`examples/present-action-button`).
  - `CSMenuMan::display_status_message(fmg_id)` shows a native message.
  - `RendMan.debug_ez_draw` draws debug lines and shapes.
  - `CSFeMan` exposes enemy and friendly name-tag displays and a `SummonMsgQueue` holding a `MenuString`, which are candidate native text sinks (SPECULATION).
- **Gap (FACT):** `MsgRepositoryImp` is an empty struct. There is no FMG lookup API at this commit. er-mods-rs reads the global and detours `GetAndFormat` itself. That detour is narrow: it relabels System → Quit Game rows "without editing bundled FMGs". **A version-tolerant alternative exists:** EwdenWing (`axd1x8a/EwdenWing`, `src/lib.rs`) finds the lower-level MsgRepository lookup `fn(this, version, category, id) -> *const u16` by an AOB call-site pattern (`48 8b f9 44 8d 42 6d e8 $ { ' } 48 85 c0` for ER) and detours it with `retour`. That gives text substitution per (FMG category, id) without per-build RVAs. FACT for the code. Whether the pattern still matches 1.17.2 is unverified, since the last commit is 2025-12-26, before 1.17.
- **Game-loop integration (FACT):** `CSTaskImp::wait_for_instance(Duration)` then `cs_task.run_recurring(closure, CSTaskGroupIndex::X)` registers a Rust closure as a native FD4 task. `CSTaskGroupIndex` has about 170 variants, including `FrameBegin`, `ChrIns_PostPhysics` and `WorldChrMan_PrePhysics`. The handle cancels the task on `Drop`. The safety notes say mutable singleton access must happen on the game's main thread, "typically either from the task system or from hooked functions". `util::system::wait_for_system_init` resolves the game's global `HINSTANCE` **through the `global_hinstance` RVA** and spins until it is non-zero. The closure is invoked from `extern "C" fn execute` (`crates/shared/src/task.rs`). Since Rust 1.81, a panic that unwinds out of an `extern "C"` function aborts the process, so **any panic inside a task closure kills the game, whatever panic profile we use.** Wrap closure bodies in `std::panic::catch_unwind`, which requires building with `panic = "unwind"`.
- **Discovery (FACT):**
  - `#[shared::singleton]` types resolve through `from_singleton`. Its README says it "uses binary regex patterns and lazy evaluation" over `FD4Singleton`/`FD4DerivedSingleton` statics (10–15 ms on first use). The `FromStatic` safety notes add that DLRF metadata must be populated first, "usually by calling… `wait_for_system_init`". The README's tested-versions list names only **"ER 1.16"**. That it still works on 1.17.2 is LIKELY, because fromsoftware-rs main uses it unchanged, but it is not documented. It is a pattern scan, so it should usually survive patches; it is not guaranteed to.
  - **Not every static is pattern-found.** `GameDataMan`, `GameMan`, `FieldArea`, `CharacterTypePropertiesTable`, `MultiplayProperties`, `WorldChrManDbgFlags`, `CryptoSPIRegistry` and `DLUserInputManagerImpl` (through the `obtain_dl_user_input_manager` function) all resolve through `rva::get()` (`load_static_direct`/`load_static_indirect`).
  - Functions and vtables come from a generated `RvaBundle` of 107 entries, produced offline by `tools/binary-mapper` from `mapper-profile.toml` (33 `[[patterns]]` AOBs plus 74 `[[vmts]]` by RTTI class name).
  - **Even `register_task`, `global_hinstance`, `chr_ins_apply_speffect` and `cs_ez_draw_*` are RVAs.** On an unknown build, the first `CSTaskImp::wait_for_instance` call panics, because it calls `wait_for_system_init` → `rva::get()`. That happens even before `run_recurring`. The fromsoftware-rs workspace's `[profile.release] panic = "abort"` applies only to that workspace's own examples. Cargo ignores profiles in dependencies, so a downstream DLL chooses its own. With `panic = "unwind"`, a panic on our own spawned thread can be caught. Do not rely on that, though: the `LazyLock` inside `rva::get()` is poisoned afterwards, and any panic inside a task closure still aborts (see above). Check the version first.
  - **Struct layouts are per-build too.** Field offsets are hard-coded in `#[repr(C)]` structs and were changed for 1.17.0 (`ChrIns`, `PlayerGameData`) and 1.17.2 (`WorldChrMan`) (§1). `FromStatic` finds *where* a singleton is, not *what shape* it has.
- **Arxan helpers (FACT, `crates/shared/src/arxan.rs`):** pattern-find and `jb→jmp` patching of Arxan "code restoration" routines. The doc comments advise: "Prefer using the task runtime over hooking the memory image."
- **Debug crate (FACT):** `crates/debug` is a hudhook/ImGui helper crate whose `Cargo.toml` says "this is not intended to be released to Crates.io". `tools/debug-eldenring` uses `hudhook::hooks::dx12::ImguiDx12Hooks`. The crate adds `InputBlocker`, which hooks `IDirectInputDevice8W::GetDeviceState` for the keyboard and mouse devices with **`ilhook`** (`input/dinput.rs`, `input/hooks.rs`). It blanks them based on ImGui's `want_capture_keyboard`/`want_capture_mouse`. It is `libhotpatch`-aware, which helps with live reload during development. Reusing it means a git dependency on an unpublished crate, so copying the approach (about 260 lines across `input.rs` and `input/*.rs`) is probably cleaner.
- **Licensing (FACT):** the repo ships `LICENSE-MIT-NON-AI` and `LICENSE-ASL2-NON-AI` ("Copyright (c) 2025 Vincent Swarte"). They forbid using the software or its derivatives "for the purpose of training or improving machine learning algorithms, including but not limited to artificial intelligence, natural language processing, or data mining", and say violators "will be subject to legal action". The workspace `Cargo.toml` and the crates.io metadata declare `MIT OR Apache-2.0`. Calling an LLM at runtime is not ML training. However, any EldenNet feature that logs play data to improve or fine-tune models could fall under "improving… machine learning algorithms… data mining". The mismatch and the clear intent need a legal read (Open questions).

### 4.2 libER (C++)

- **FACT:** `Dasaav-dsv/libER`, C++20, Apache-2.0 with LLVM exception. It relies on the game having been built with VS2015, so the MSVC STL ABI matches. It provides game containers and allocators (`from::vector`, `from::string`, …), FD4 tasks (`coresystem/task.hpp`), params (`param/*` plus about 200 paramdef headers), file IO, DLRF reflection and draw.
- It claims "No pattern matching… Symbol definition files, separated by game version" with an optional `libER_updater.exe`. Symbols are compiled into `symbols/*.csv`.
- **The last push was 2025-04-24** (PR "1.16-game-upd"). There is no evidence of 1.16.1, 1.16.2 or 1.17.x symbols (LIKELY stale).
- There are **no `WorldChrMan`/`ChrIns` headers on `main`**, so character access would have to be reverse-engineered from scratch. A `worldchrman-impl` branch exists, but its last commit was 2024-07-10 ("1.12 fixes"), so it is abandoned.
- The Rust port `Nordgaren/liber-rs` was last pushed in 2024.
- Still used in 2026 for narrow param patchers (`imCioco/EldenRing-Modding`) and as a reference by EROverlay (`docs/ui-scene-prototype.md` cites libER commit `d8ae5c9`).

### 4.3 External, out-of-process tools (reference only)

- **Erd-Tools** (`Nordgaren/Erd-Tools`, C#; last commit 2026-03-13) and the **Elden Ring Debug Tool** (C#/WPF, .NET 6; last commit 2026-03-14) use TKGP's PropertyHook: AOB scans plus `ReadProcessMemory`/`WriteProcessMemory` from a separate process. They are good for exploring structures and offsets, but are the wrong architecture for a real-time framework.
- **TGA Cheat Engine table** (`The-Grand-Archives/Elden-Ring-CT-TGA`): supports app version **1.17.2** with CE 7.4/7.5 (FACT, README) and was last pushed 2026-10-22. It is the best place to find current pointer paths, event-flag knowledge and param scripts quickly. It is built from files with CE2FS.

### 4.4 Other building blocks

- **`tremwil/dearxan`** (Rust, with C and C++ static libraries; pushed 2026-11-01): "fully neuter[s] Arxan" (GuardIT) in all current FromSoftware games, including the ER and Nightreign variants. Integrated into me3 as `disable_arxan`, and its README recommends that option over linking it. If it is linked directly, the README "**strongly**" recommends a loader that runs code before the game's entry point. Ordinary me3 natives load after main, so linking it ourselves would also need `load_early`.
- **`techiew/EldenRingMods/ModUtils.h`:** a header-only C++ signature scanner, memory helper and logger.
- **`Nordgaren/ermod-template`:** a Rust ME2-extension template (2025).
- **`Dasaav-dsv/ERSkeletonMan`:** a skeleton-modification header.
- **`ElaDiDu/Scripts-Data-Exposer-FS`:** adds HKS (Lua) functions so scripts can read more game data. This is a precedent for exposing native data to game scripts, relevant if later versions add actions.

---

## 5. Address discovery strategies and version tolerance

| Strategy | Who uses it | Survives patches? | Notes |
|---|---|---|---|
| Hard-coded RVAs per build | fromsoftware-rs `RvaBundle`, er-mods-rs, libER symbols | No: regenerate each patch | Simplest and fastest. Must check the build version first. |
| AOB / signature scan at runtime | ModUtils.h, CE tables, EROverlay ("signature- and RTTI-validated"), `from_singleton` | Usually | Breaks when the compiler output changes. Arxan-encrypted functions may be unreadable until Arxan runs or is neutered. |
| MSVC RTTI (`RTTICompleteObjectLocator`) → vtables by class name | me3 `binary-analysis/rtti.rs`, fromsoftware-rs `shared/rtti.rs`, mapper `[[vmts]]` | Yes, while class names stay the same | Ideal for vtable hooks and for confirming object types at runtime. |
| DLRF runtime-class registry (`DLRF::DLRuntimeClass`) | me3 `binary-analysis/dlrf.rs` (scans `.data`), fromsoftware-rs `dlrf.rs` | Yes | FromSoftware's own reflection; gives class names, including for game properties. |
| FD4Singleton registration scan | `from_singleton` (fromsoftware-rs `FromStatic::instance()`) | Usually (README: tested on ER 1.16 only; used unchanged on 1.17.2 by fromsoftware-rs main, LIKELY works) | Finds the manager singletons by name with binary regex patterns. Locates the object, not its layout. |
| Game task system instead of detours | fromsoftware-rs `run_recurring` | Needs two RVAs (`global_hinstance` via `wait_for_system_init`, and `register_task`) | Avoids Arxan code-restoration problems and hook conflicts. |
| Hard-coded struct layouts | fromsoftware-rs `#[repr(C)]` structs, libER headers | No: `ChrIns` shifted in 1.17.0, `WorldChrMan` in 1.17.2 | Needs runtime sanity checks (see below). |

**Arxan (GuardIT).** Arxan encrypts some functions and runs integrity "code restoration" routines that can undo inline patches (fromsoftware-rs `arxan.rs`; dearxan README). Hooking risk can be reduced three ways: register game tasks instead of hooking (§4.1), use vtable or pointer hooks on objects found through RTTI, or run with me3 `disable_arxan = true` when inline hooks are unavoidable.

**Recommended layering for EldenNet (SPECULATION, built from the above):**
1. At `DllMain` → thread start, read the PE version (`VS_FIXEDFILEINFO.dwProductVersion`, the same thing fromsoftware-rs does) and compare it against our own supported list **before** calling any fromsoftware-rs API that uses RVAs, including `CSTaskImp::wait_for_instance`.
2. Locate singletons only through `FromStatic`, never through raw global addresses. Remember that field offsets are still compiled in.
3. For the few functions we need (task registration, optional text detour), keep a per-build table and a runtime AOB fallback that is verified by function length and prologue. EROverlay's "signature- and RTTI-validated" hooks pass static checks across builds 1.05–1.17.1, and EwdenWing's MsgRepository AOB shows the approach is viable. Only enable features whose addresses resolve and pass a self-test.
4. **Layout self-test** before perception is enabled. Examples: `main_player` is non-null; its `modules.data.hp` is within `0..=max_hp`; `chr_inses_by_distance` entries point at `ChrIns` whose vtable RTTI name is a `ChrIns` subclass, sorted by ascending `distance`; `npc_param_id` resolves in `SoloParamRepository`. Turn perception off if any check fails.
5. Wrap every task closure body in `catch_unwind` (build with `panic = "unwind"`).
6. If anything fails, run in "overlay-only, observe-nothing" mode with a visible warning. Never crash the game.

---

## 6. Existing deep integrations and how they load

| Project | Language / form | How it loads | What it proves |
|---|---|---|---|
| Seamless Co-op (`ersc.dll`) | Closed-source native | `ersc_launcher.exe`, ME2 `external_dlls`, me3 natives | Rewriting large parts of the game's netcode in-process is stable at scale ("3M+ downloads", LIKELY). |
| unseamless-coop (micthiesen) | Rust, fromsoftware-rs | `dinput8.dll` plus a replacement `start_protected_game.exe`; self-aborts if not started by its launcher | Rust + fromsoftware-rs can host Steam lobbies and in-game overlays. |
| er-mods-rs (Banon-Labs) | Rust, 23 DLLs in README, fromsoftware-rs fork through a `path` dependency (CI pins `4612952`) | me3 natives | Narrow runtime UI text replacement (Quit → Save Game rows) through a `MsgRepository::GetAndFormat` detour "without editing bundled FMGs". Per-build gating, a 535-entry detour allowlist, a machine-readable DLL conflict list (`scripts/me3-dll-conflicts.toml`), DirectInput suppression for overlays (`er-dinput-suppress-core`). Built with coding agents (`AGENTS.md`). |
| Practice tool (veeenu) | Rust + hudhook | Standalone injector or `dinput8.dll`; requires steam_appid bypass | Years of stable DX12 ImGui overlay on ER, including Proton and Steam Deck. Its README requires a "completely vanilla install… with no mods whatsoever", so it says nothing about co-loading with other mods. |
| EROverlay (soarqin) | C++ ImGui | `winhttp.dll` proxy, EML, ME2 or me3 | Since 2026-10-16 (commit `c0452df`) renders through an in-engine "UI_SCENE" hook instead of DXGI hooks. Its "signature- and RTTI-validated" hooks pass static checks on 1.05–1.17.1 (README at `afab7e1`, 2026-10-29), which is strong evidence that signature discovery spans many patches. Handles HDR RGB10A2 targets. |
| PostureBarMod (Mordrog) | C++ | Elden Mod Loader | World-space UI above enemy heads with no data changes. |
| EwdenWing (axd1x8a) | Rust (`retour` detour, `pelite` AOB scan) | me3 profile | Hooks all in-game text across five FromSoftware games at runtime through one AOB-found MsgRepository lookup per game, with no per-build RVAs. Last commit 2025-12-26, so 1.17.x is untested. |
| Elden Ring Reforged | Data overhaul + DLLs | Own launcher bundling ME2 (LIKELY); separate saves; own servers in 2.0 | Large `regulation.bin`/map/ESD changes. Our DLL must coexist with it. |
| The Convergence | Data overhaul | Ships a `.me3` profile (LIKELY); 3.0.4.1 on 1.17.2 (LIKELY) | Same coexistence concern; adds NPCs we will not know. |
| Item/Enemy Randomizer (thefifthmatt) | C# generator → data files | Built-in ME2 launch (LIKELY) | Randomized enemy placement means NPC identity must come from the live `ChrIns`, not from map assumptions. |
| TGA table / ER Debug Tool | CE Lua / C# external | Cheat Engine attach / external process | Reverse-engineering references. |
| EldenEncouragement (Nexus 7999) | External exe overlay | Separate process; ME2 suggested for auto-start | **A previous LLM mod for ER:** an OpenAI-backed "maiden" (Ranni, Melina, Blaidd or Millicent) that comments on play through overlay subtitles, with static voice lines and a required server connection (LIKELY, via snippet). It does not have in-process perception or NPC dialogue. |
| Un-Alone (Nexus 8046) | Builds on Elden Encouragement | Reportedly installs me3 automatically | A companion with a relationship system and comments that grow as the bond deepens (LIKELY, via snippet). The second LLM-adjacent ER mod found; not free-form NPC dialogue. |

We found no in-process LLM NPC dialogue framework for Elden Ring in GitHub or Nexus search results (LIKELY; Nexus could not be searched directly). "Chat with Melina" (Nexus 1587) uses fixed dialogue options.

---

## 7. Overlay, text input and audio

### 7.1 Renderer hooking

- **FACT:** ER on PC is D3D12. hudhook (`veeenu/hudhook`, MIT) v0.9.3 (2026-09-08) supports DX9, DX11, DX12 and OpenGL3 on Windows and Wine/Proton. It uses MinHook (`src/mh.rs`) on `IDXGISwapChain::Present`/`Present1`/`ResizeBuffers`/`ResizeBuffers1` and `ID3D12CommandQueue::ExecuteCommandLists`.
- Recent DX12 fixes: 0.9.0 deferred fence signaling and per-frame command contexts; 0.9.2 stabilized swap-chain resize. It also supports `MessageFilter`, `before_wnd_proc`/`after_wnd_proc` callbacks and texture loading. fromsoftware-rs's debug tools depend on it through git.
- **C++ equivalent:** kiero (vtable discovery) + MinHook + Dear ImGui's `imgui_impl_dx12`. This is the well-known generic pattern (LIKELY). EROverlay's history shows the DXGI path was replaced by the in-engine UI_SCENE path for compositing and HDR correctness.
- **Hazards:**
  - Two DXGI Present compositors in one process conflict. er-mods-rs says "some of these must not be loaded together", keeps the list in `scripts/me3-dll-conflicts.toml`, and names "two MinHook instances detouring one prologue and corrupting each other's trampolines" as the usual cause. hudhook bundles its own MinHook (`src/mh.rs` wraps the vendored C library), so a second hudhook-based DLL in the same process is exactly this case.
  - The practice tool says it does not support RTSS, FRAPS or similar overlays; the Steam overlay is an exception.
  - HDR output (RGB10A2) changes how overlay alpha and colors look (EROverlay doc).

### 7.2 Keyboard and mouse capture for a chat box

- **FACT (`eldenring/src/dluid/user_input_manager.rs`, `user_input_device.rs`):** the game's `DLUserInputManagerImpl` holds an `IDirectInput8W`. Keyboard and mouse state come from DirectInput8 `GetDeviceState`, pads from `XInputGetState`, and a `use_lib_sce_pad` flag selects DualSense support.
- **Independent confirmation (FACT):** er-mods-rs `crates/er-dinput-suppress-core/src/lib.rs`: "ELDEN RING reads the mouse through DirectInput, which never touches the window procedure, so hudhook's `MessageFilter`… closes just the legacy-message half of the hole". Its keyboard half "chains one handler onto the keyboard's `GetDeviceState` that zeroes those bytes in a 256-byte read".
- **Consequence:** ImGui gets typed characters through the WndProc (`WM_CHAR`), but the game keeps reading raw key state through DirectInput. Unless we also block DirectInput, typing "w" in the chat box moves the character. fromsoftware-rs `debug::InputBlocker` solves this: it hooks `GetDeviceState` on both devices with `ilhook` and returns blank state while ImGui wants input. **Reuse it, or copy the approach.** Coexistence: other overlays (er-mods-rs, possibly EROverlay) may hook the same `GetDeviceState` entry. Chain to the previous handler and never assume we are the only hook (LIKELY hazard).
- **Pads (SPECULATION):** players on controllers and Steam Deck need a way to enter text. Options: Steam's `ISteamUtils::ShowFloatingGamepadTextInput`/`ShowGamepadTextInput` (Steam is running even when launched through steam_appid.txt), push-to-talk STT, or a quick-reply menu built from the native talk list (`TalkScript` + `AddTalkListData`). While the chat box is open, pad input to the game must also be blocked, for example by hooking XInput or zeroing the virtual input data.
- **Pausing:** ER has no pause button in normal play. Players widely use an offline trick: opening the in-game Help → "Menu Explanation" screen pauses the game. That is from memory and was not re-verified in this pass because the search budget ran out, so treat it as LIKELY. If it is true, the engine has a pause state that a later version might trigger. A chat UI must decide whether to freeze NPC AI or the player. Showing the chat box only when no enemies are hostile nearby is an easy v1 rule (SPECULATION).

### 7.3 Audio (TTS output, STT input)

- v1 can play TTS through its own audio stack (WASAPI via `cpal`/`rodio`/`kira` in Rust, or miniaudio in C++), panned and attenuated using `ChrIns` positions relative to `CSCamera`. This needs no Wwise or soundbank edits (SPECULATION, standard practice). The game's audio middleware is Audiokinetic Wwise: `vswarte/rewwise` (last commit 2026-10-26) unpacks and repacks ER `.bnk` soundbanks and `.wem` audio, and new WEMs require Wwise Studio to author (FACT, rewwise README). Feeding audio into the game's Wwise to get lip-sync and the voice-volume slider would need data or engine work (deferred).
- Microphone capture through the same audio library, with whisper.cpp (`whisper-rs`) in-process or STT in a sidecar.

### 7.4 Coexistence rules

- Prefer `run_recurring` tasks and reading structs over inline hooks. If we must detour, use one hooking library and chain cleanly, publish our hook list, and avoid functions ERSC is likely to hook (session, network and save code).
- Use the overlay only when the chat box is open, or offer an "in-engine UI" mode later. One ImGui context is safest.

---

## 8. Data-editing tools, and whether v1 needs any (it does not)

| Tool | Status (FACT: last push) | What it edits | Needed for v1? |
|---|---|---|---|
| **Smithbox** (vawser; fork of DSMapStudio) | 2026-11-06, active | Params, text (FMG), maps (MSB), models, TAE, HKX, materials; reads game archives without unpacking | No. Use it to *read* NpcParam, TalkParam and FMG to build the NPC identity table offline. |
| DSMapStudio (soulsmods) | last commit 2024-07-20 (the earlier "2025-05-25" was a push date) | Maps, params, text | No (Smithbox supersedes it). |
| WitchyBND (ividyon; Yabber successor) | 2026-07-06 | DCX/BND4/FMG/PARAM/TPF/LUA/… unpack, repack, serialize; needs **Nuxe** to unpack archives | No. Useful to dump `msg/engus/*.msgbnd.dcx` (TalkMsg, NpcName) and `talk/*.talkesdbnd.dcx` for research. |
| Yapped Rune Bear (vawser) | 2024-06-23 | Params | No (legacy; Smithbox is its successor). |
| **DarkScript3** (AinTunez, thefifthmatt) | 2026-03-08 | EMEVD event scripts (JS / "MattScript"); ER instruction list at soulsmods.github.io/emedf | No. **Needed later** for actions such as making an NPC hostile, moving it, or setting flags through events. |
| **ESDLang / esdtool** (thefifthmatt) | 2025-10-21 | Talk ESD state machines, decompiled to Python-like code | No. **Possibly needed later** to add a "Speak freely" entry to vanilla talk menus. |
| SoulsFormats (TKGP) | Archived (2025-05-26) | .NET format library | Indirectly, through the tools above. |

**Why v1 needs zero edits (SPECULATION based on FACTs above):**

1. **Identity:** read `ChrIns.npc_param_id` and `event_entity_id`, then `SoloParamRepository::get::<NPC_PARAM_ST>()` (which has `name_id` and `team_type`). `character_id` is a model ID and is not unique for humans (§4.1). Names come from the runtime FMG via `MsgRepository`: either the global plus a `GetAndFormat` call (per-build RVA), or the AOB-found lookup EwdenWing uses. A curated JSON table keyed by event entity ID and NpcParam ID adds personality and lore and lives **outside** game archives.
2. **Awareness:** event flags (`CSEventFlagMan`), map and block IDs (`ChrIns.block_id`), time of day (`WorldAreaTime`), combat state from HP and `last_hit_by` changes, and team type. All are read-only.
3. **Output:** ImGui subtitles and chat, with optional world-space barks (projected positions above heads, as PostureBarMod does). Later, native-looking text through a narrow `GetAndFormat` or lookup detour that substitutes strings for reserved IDs. er-mods-rs shows this works on 1.17.2 for a few fixed menu strings. EwdenWing shows it works for all text on older builds. Neither has been shown to substitute dynamic per-line text into talk subtitles.
4. **Trigger:** a hotkey or `present_action_button(<existing ActionButtonParam row>)` when the player is near or locked onto an NPC (`ChrIns.is_locked_on`, `chr_inses_by_distance`). Choosing an existing row whose label reads well is an open question.

**Benefit:** v1 changes no `regulation.bin`, `msgbnd` or ESD files, so it can be added to Reforged, Convergence, randomizers and ERSC setups without merging. **Cost:** overhauls change and add NPCs. The identity table must fall back to the generic "barks only" tier for unknown `npc_param_id`s, and display names must come from runtime FMG reads (which then include overhaul text) rather than a baked list.

---

## 9. Language and toolchain recommendation

### 9.1 Rust: fromsoftware-rs + hudhook + me3 (recommended)

**Pros**
- **The only maintained typed SDK we found (LIKELY):** characters, params, flags, tasks, ESD invocation and debug draw. Its RVAs were updated on the day of each 1.17.x patch (1.17.0, 1.17.1 and 1.17.2). Struct-layout fixes followed one day later for 1.17.0 (`b080915`) and 1.17.2 (`5baf0dd`).
- **Shared community stack:** the same ecosystem as me3, dearxan, hudhook, the practice tool, er-mods-rs, unseamless-coop and EwdenWing.
- **Version-tolerant singleton lookup** through `from_singleton` and DLRF.
- **Game tasks without hooks** through `run_recurring`.
- **Strong libraries for the AI side, all pure Rust or with bundled C:**
  - networking: `reqwest` (rustls) and `tokio`/`tokio-tungstenite` for LLM streaming and WebSockets
  - storage: `rusqlite` with the `bundled` feature for SQLite memories
  - data and prompts: `serde_json`; `minijinja` for Jinja-style prompt templates
  - audio: `cpal`/`rodio`/`kira`
  - speech and local models: `whisper-rs`; `ort` for ONNX
  - logging: `tracing`
- **Can cross-compile from Linux** to `x86_64-pc-windows-msvc` with `cargo-xwin` or `xwin` (me3 README; er-mods-rs).
- **Memory safety** around the asynchronous code paths, where most bugs will be.

**Cons**
- Breakage on each patch, a NON-AI license question, and a crates.io release that lags (use a git pin or a fork).
- `panic!` on unknown builds means we must check the version first. Panics inside task closures abort the game through the `extern "C"` boundary, so every closure needs `catch_unwind`.
- Upstream keeps only the newest build's RVA bundle. Supporting players on the previous patch means maintaining our own fork with several bundles, as er-mods-rs does.
- `unsafe` is everywhere at the game boundary, and threading rules are enforced only by convention (main thread for `instance_mut`).
- Struct layouts **do** change, even on bug-fix patches (1.17.2 shifted `WorldChrMan`), and the crate's fixes can lag the RVA update by a day. Wrap field reads in our own accessor layer and run the layout self-test (§5).

### 9.2 C++: libER or a home-grown "CommonLib" with MinHook, kiero and ImGui

**Pros**
- Matches the MSVC STL ABI directly (libER).
- Closest to SkyrimNet's own C++/CommonLib world.
- Mature native libraries: libcurl/cpr, nlohmann/json, the SQLite amalgamation, miniaudio, whisper.cpp, ONNX Runtime.
- Easy to link the dearxan static library.

**Cons**
- No maintained character-level SDK. libER stopped at 1.16 (April 2025) and lacks `WorldChrMan`/`ChrIns`.
- We would have to reverse-engineer and maintain every structure ourselves across four or five patches a year (the 2026 pace).
- Smaller active community in 2026: most new native ER work found is Rust plus me3.

### 9.3 Recommended architecture (SPECULATION)

- **`eldennet.dll` (Rust cdylib, me3 native).** It contains:
  - (a) Version check and feature gating.
  - (b) A perception task registered with `run_recurring` (for example `ChrIns_PostPhysics` at about 5–10 Hz). It snapshots the player, the nearest N `ChrIns`, combat deltas, flags and time into plain data.
  - (c) The hudhook DX12 overlay and the `InputBlocker` approach.
  - (d) An outgoing channel to a worker thread running a tokio runtime.
  - (e) An incoming channel of lines to show or speak, drained on the game thread.
- **The heavy work** — LLM calls, memory database, TTS and STT — runs on worker threads. Optionally it can live in an out-of-process **sidecar** reached over localhost HTTP or WebSocket, which isolates crashes and lets that part be written in any language. Either way, the game thread never blocks on I/O.

```rust
// Only the call shape is grounded; it follows fromsoftware-rs examples/debug-line.
std::thread::spawn(|| {
    if !eldennet::build_supported() { return eldennet::overlay_only_mode(); } // check version BEFORE any RVA use
    let task = CSTaskImp::wait_for_instance(Duration::MAX).unwrap(); // calls rva::get(): version-gate first
    let handle = task.run_recurring(move |_: &FD4TaskData| {
        // A panic here would unwind into `extern "C" fn execute` and abort the game: catch it.
        let _ = std::panic::catch_unwind(|| {
            let Ok(wcm) = (unsafe { WorldChrMan::instance() }) else { return };
            eldennet::perception::tick(wcm);   // read-only snapshot -> channel
            eldennet::output::drain_on_game_thread();
        });
    }, CSTaskGroupIndex::ChrIns_PostPhysics);
    std::mem::forget(handle); // keep the task registered for the process lifetime
});
```

```toml
# EldenNet.me3 (sketch; option names from me3 schemas/mod-profile.json)
profileVersion = "v1"
savefile = "ER0000.eldennet.sl2"  # separate save; check the naming convention
start_online = false              # never online
[[supports]]
game = "eldenring"
[[natives]]
path = 'natives/eldennet.dll'
load_after = [{ id = "ersc.dll", optional = true }]  # native ids are DLL file names
# Seamless Co-op users add: [[natives]] path = '<Game>/SeamlessCoop/ersc.dll'
```

`catch_unwind` needs the closure to be `UnwindSafe`. In practice you wrap it in `AssertUnwindSafe`. Treat the sketch as shape, not compiled code.

---

## 10. Implications for the AI framework

**v1 (minimal, stable, dialogue only)**

1. **Distribution:** one Rust DLL plus an me3 profile. Document ME2 `external_dlls` as a fallback. No archive or regulation edits. Default to a separate save and `start_online = false`. Refuse to run if the `Menu.IsEnableOnlineMode` override is absent; checking that is an open question.
2. **Version gate first:** keep a supported-build table (currently 2.7.2.0 WW and 2.7.2.1 JP). Gate both the RVA bundle and the struct layouts on it. On a mismatch, show an overlay banner "EldenNet disabled for game version X" and touch nothing else. That includes not calling `CSTaskImp::wait_for_instance`, which panics on unknown builds. Without a hooked game task, the banner needs our own DXGI/hudhook overlay, which does not depend on game RVAs. Budget a 1–7 day turnaround after each patch. Upstream RVAs have landed the same day, layout fixes one day later, and then we must rebuild and release.
3. **Prefer reads to writes and tasks to hooks.** v1 needs no gameplay hooks. The only optional detour is native text (`GetAndFormat` or the AOB-found lookup), behind a feature flag. Wrap every task closure in `catch_unwind`.
4. **Perception comes from structs** (`chr_inses_by_distance`, `ChrIns.modules.data`, `last_hit_by`, `team_type`, `event_entity_id`, `npc_param_id`), flags and time, after the layout self-test passes. Generic NPC barks fire on derived events, all detected by diffing snapshots: first aggro (team relation plus distance; an approximation, since no typed aggro flag is exposed), taking or dealing damage, the player's death and the NPC's death.
5. **Named NPC tier:** key named NPCs by `event_entity_id` (placed NPCs) and `npc_param_id`, mapped to curated personality and lore files. Do **not** key on `character_id`, which is a model ID that humans share. Persist memories per save file and character in SQLite. Unknown IDs (overhauls, randomizers, Tarnished Pack additions) fall back to the generic tier, with names pulled from the runtime FMG.
6. **UI:** ImGui chat box and subtitles through hudhook DX12. DirectInput blocking while typing. A controller text path (Steam floating keyboard or STT). World-space bark text placed with the camera projection.
7. **Threading:** game-thread work stays in task closures. All I/O and LLM work happens off-thread and talks through bounded channels. Never hold game pointers across frames; re-resolve each one by `FieldInsHandle` (`chr_ins_by_handle`).
8. **Compatibility checklist:** ERSC loaded at the same time; Reforged and Convergence profiles; Proton (hudhook and me3 support it); HDR on; Steam overlay on.

**Later (v2+)**

- Native-looking subtitles and talk menus: `TalkScript` with `AddTalkListData`, plus string substitution in `GetAndFormat`. Then optional ESD edits with ESDLang to add "Speak freely" to vanilla NPC menus.
- Actions: event flags first, which are already writable through `CSEventFlagMan::set_flag`. Then EMEVD events built with DarkScript3, then SpEffects (`chr_ins_apply_speffect`). Action mode should be off by default in ERSC sessions, because host and client state can drift apart. Flag writes can produce quest states that cannot happen in normal play. The ER Debug Tool warns that skipping progression "will cause you to get banned", so any save touched by action mode must stay offline forever.
- In-engine rendering (EROverlay's UI_SCENE approach) to avoid DXGI conflicts and handle HDR properly.
- Nightreign: fromsoftware-rs and me3 already support it, so much of the code would port.

---

## 11. Risks

- **Patch cadence:** five PC patches between August 2025 and October 2026 (1.16.1, 1.16.2, 1.17.0, 1.17.1, 1.17.2), four of them within five months of 2026. Each breaks RVAs, and at least two (1.17.0, 1.17.2) changed struct layouts we would read. Steam auto-updates can break players' installs overnight. Mitigation: a version gate plus a layout self-test, a quick-update process, and advice to players on blocking updates or downpatching.
- **fromsoftware-rs:** depending on a git commit or fork (not crates.io), panic-on-unknown-build behavior, panics in task closures aborting the game, license ambiguity, and API churn (0.x versions).
- **Arxan:** inline hooks can be silently reverted or crash the game. Mitigation: avoid inline hooks, or require `disable_arxan`.
- **Hook and overlay conflicts** with ERSC, other overlays, ReShade, DLSS mods and RTSS. Two Present compositors, or two MinHook instances on one prologue, corrupt each other.
- **Ban and support misunderstanding:** players who launch through Steam with EAC, or who take saves online. Mitigation: explicit docs, a separate save, and refusing online mode.
- **Antivirus false positives:** a networking DLL injected into a game. me3 itself is often quarantined (FAQ). Mitigation: code signing and clear install docs.
- **Community and policy friction:** me3 bans LLM use in its project spaces, and fromsoftware-rs uses NON-AI licenses. Expect a cold reception and limited upstream help. Do not file AI-generated issues upstream.
- **Thread-safety bugs:** touching singletons off the main thread can cause rare crashes that are hard to reproduce.
- **Proton:** DirectInput behavior under Wine needs separate testing. Audio output from our own stack under Proton also needs testing. me3's earlier Linux audio bug (Discussion #582) was fixed in v0.11.0.

---

## 12. Open questions

1. Does me3 apply `Menu.IsEnableOnlineMode=false` before any network call, and can our DLL check it (through game properties or the `CSSessionManager` state) so it can refuse to run when it is false?
2. ~~What do me3 `load_before`/`load_after` `id`s refer to for `[[natives]]`?~~ **Answered:** the DLL file name (`crates/mod-protocol/src/native.rs`). Still open: do we actually *need* to load after `ersc.dll`, and does ERSC tolerate natives that load before it?
3. The exact behavior of fromsoftware-rs's NON-AI license versus its `Cargo.toml` SPDX `MIT OR Apache-2.0`. Is a runtime-LLM mod acceptable to the authors?
4. Which existing `ActionButtonParam` row reads well as a "Talk" prompt for `present_action_button`, or must the prompt be our own overlay?
5. Is there a stable, version-tolerant way to resolve FMG text without hard RVAs? **Partly answered:** EwdenWing's AOB for the MsgRepository lookup works across five games on pre-1.17 builds. Does it still match 1.17.2, and does the lookup's `(version, category, id)` signature hold?
6. ~~Do layouts change between 1.17.x patches?~~ **Answered: yes.** `ChrIns`/`PlayerGameData` changed in 1.17.0 (`b080915`) and `WorldChrMan` in 1.17.2 (`5baf0dd`). Still open: did the 1.17.2 shift exist already in 1.17.0 or 1.17.1 and go unnoticed? And which other structs we read (`ChrInsModuleContainer`, `CSChrDataModule`) have unverified layouts?
7. Does Steam's floating gamepad keyboard work when the game was started through `steam_appid.txt` or me3?
8. Are ME2-launched setups (the Reforged launcher) still working on 1.17.2, or has Reforged moved to me3? (Nexus pages could not be fetched.)
9. Is it safe to call `TalkScript` events on NPCs whose own ESD is running (vanilla quest NPCs), or only on NPCs with no talk script?
10. Does `from_singleton`'s pattern set have any documented 1.17.x test, or only ER 1.16 as its README lists?
11. Should EldenNet maintain its own fromsoftware-rs fork with several builds' RVA bundles, as er-mods-rs does, so that players one patch behind are supported?

---

## Sources

**Repositories (read directly, 2026-11-06)**
- fromsoftware-rs: https://github.com/vswarte/fromsoftware-rs (commit `5702315`; `crates/eldenring/src/rva.rs`, `crates/eldenring/mapper-profile.toml`, `crates/eldenring/src/cs/{world_chr_man,chr_ins,event_flag,solo_param_repository,talk_script,menu_man,fe_man,msg_repository,task}.rs`, `crates/eldenring/src/dluid/*`, `crates/shared/src/{static,task,arxan,rtti,game_version}.rs`, `crates/debug/src/input/*`, `examples/*`, `tools/binary-mapper/README.md`, `LICENSE-*-NON-AI`)
- fromsoftware-rs history and releases: https://github.com/vswarte/fromsoftware-rs/commits/main/crates/eldenring/src/rva/rva_ww.rs, https://github.com/vswarte/fromsoftware-rs/releases
- me3: https://github.com/garyttierney/me3 (commit `5f4fa47`; `CHANGELOG.md`, `schemas/mod-profile.json`, `docs/configuration-reference.md`, `docs/user-guide/{creating-mod-profiles,faq}.md`, `crates/launcher/src/game.rs`, `crates/mod-host/src/{lib,host,native}.rs`, `crates/mod-host/src/host/game_properties.rs`, `crates/cli/src/commands/launch.rs`, `AI_CODE_POLICY.md`), https://github.com/garyttierney/me3/releases, https://github.com/garyttierney/me3/discussions/582
- ModEngine2: https://github.com/soulsmods/ModEngine2 (README, `installer/dist/config_eldenring.toml`, `launcher/launcher.cpp`), https://github.com/soulsmods/ModEngine2/releases
- libER: https://github.com/Dasaav-dsv/libER (commit `d8ae5c9`); liber-rs: https://github.com/Nordgaren/liber-rs
- from-singleton: https://github.com/Dasaav-dsv/from-singleton
- dearxan: https://github.com/tremwil/dearxan
- hudhook: https://github.com/veeenu/hudhook (`CHANGELOG.md`, `src/hooks/dx12.rs`, `src/renderer/input.rs`)
- Practice tool: https://github.com/veeenu/eldenring-practice-tool
- er-mods-rs: https://github.com/Banon-Labs/er-mods-rs (`README.md`, `docs/game-build.md`, `crates/er-title-flow/src/constants_autoload_state.rs`, `crates/er-game-base/src/rva.rs`, `crates/er-quit-menu-core/src/msg_text_ids.rs`)
- unseamless-coop: https://github.com/micthiesen/unseamless-coop
- EROverlay: https://github.com/soarqin/EROverlay (`docs/ui-scene-prototype.md`)
- PostureBarMod: https://github.com/Mordrog/EldenRing-PostureBarMod
- EwdenWing: https://github.com/axd1x8a/EwdenWing; textrender: https://github.com/axd1x8a/textrender
- Elden Mod Loader: https://github.com/techiew/EldenRingModLoader; EAC Toggler: https://github.com/techiew/EldenRingEacToggler; ModUtils.h: https://github.com/techiew/EldenRingMods
- Offline Launcher: https://github.com/LukeYui/launch_modded_eldenring
- Seamless Co-op mirror: https://github.com/LukeYui/EldenRingSeamlessCoopRelease (`VERSION`); docs: https://github.com/ersc-docs/ersc-docs.github.io (`pages/seamless-modding.md`, `pages/how-to-install.md`, `pages/faq.md`)
- Waygate: https://github.com/vswarte/waygate-server
- TGA CE table: https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA
- Erd-Tools: https://github.com/Nordgaren/Erd-Tools; ER Debug Tool: https://github.com/Nordgaren/Elden-Ring-Debug-Tool; ermod-template: https://github.com/Nordgaren/ermod-template
- Smithbox: https://github.com/vawser/Smithbox; DSMapStudio: https://github.com/soulsmods/DSMapStudio; WitchyBND: https://github.com/ividyon/WitchyBND; SoulsFormats: https://github.com/JKAnderson/SoulsFormats; Yapped Rune Bear: https://github.com/vawser/Yapped-Rune-Bear; DarkScript3: https://github.com/AinTunez/DarkScript3; ESDLang: https://github.com/thefifthmatt/ESDLang
- Scripts-Data-Exposer-FS: https://github.com/ElaDiDu/Scripts-Data-Exposer-FS
- Added in the fact-check pass: fromsoftware-rs commits `b080915`, `5baf0dd`, `acb2a19`, `29fd138`, `b726336` and tag `v0.14.0` (`crates/eldenring/src/rva.rs`); `crates/eldenring/src/util/system.rs`, `crates/shared/src/{static,task}.rs`, `crates/eldenring/src/cs/{game_data_man,game_man,field_area}.rs`, `crates/debug/Cargo.toml`
- crates.io API: https://crates.io/api/v1/crates/eldenring, https://crates.io/api/v1/crates/hudhook
- me3 `crates/mod-protocol/src/native.rs`, `crates/cli/src/commands/profile.rs`; Discussion #582 resolution: https://github.com/garyttierney/me3/discussions/582
- er-mods-rs `crates/er-dinput-suppress-core/src/lib.rs`, `build-support/prologue_build.rs`, `AGENTS.md`
- EwdenWing `src/lib.rs`: https://github.com/axd1x8a/EwdenWing
- EROverlay README (commit `afab7e1`): https://github.com/soarqin/EROverlay
- from-singleton README: https://github.com/Dasaav-dsv/from-singleton
- rewwise (Wwise soundbank tools): https://github.com/vswarte/rewwise
- libER branches (`worldchrman-impl`, `dev`): https://github.com/Dasaav-dsv/libER/branches
- Waygate license (AGPL-3.0): https://github.com/vswarte/waygate-server/blob/main/LICENSE
- ER Debug Tool README (ban warning): https://github.com/Nordgaren/Elden-Ring-Debug-Tool
- unseamless-coop `.agents/skills/`: https://github.com/micthiesen/unseamless-coop

**Patch 1.17.1 / 1.18 checks (via search results; LIKELY)**
- https://x.com/ELDENRING/status/2097236393856524780 (1.17.1 announcement)
- https://www.vice.com/en/article/elden-ring-update-today-patch-1-17-1-notes/
- https://mp1st.com/title-updates-and-patches/elden-ring-version-1-17-1-out-minor-update-1-018-001
- https://ixbt.games/en/news/2026/10/21/fromsoftware-vypustila-obnovlenie-dlia-elden-ring-ulucsili-bezopasnost-i-ustranili-bagi.amp.html (1.17.2)
- https://www.thefpsreview.com/2026/04/01/elden-ring-patch-1-18-adds-easy-mode-but-only-for-the-first-boss-and-only-if-you-ask-nicely/ (April Fools; not a real patch)
- https://www.pushsquare.com/news/2026/06/elden-rings-new-tarnished-dlc-pack-locked-in-for-august, https://eldenring.wiki.gg/wiki/Tarnished_Pack
- https://opencritic.com/news/31863/elden-ring-may-2026-official-update-announced (1.16.2)
- https://steamcommunity.com/sharedfiles/filedetails/?id=3790860843 (downpatch guide)
- https://www.nexusmods.com/eldenring/mods/8046 (Un-Alone), https://www.nexusmods.com/eldenring/mods/1587 (Chat with Melina)

**Patch level (via search results; LIKELY unless noted)**
- https://www.bandainamcoent.com/news/elden-ring-patch-notes-version-1-17
- https://www.rpgsite.net/news/21216-elden-ring-patch-1-17-notes-balance-adjustments-tarnished-pack-dlc
- https://www.pcgamesn.com/elden-ring/patch-notes-1-17-tarnished-pack-dlc
- https://www.gamerfuzion.com/elden-ring-patch-1-17-2-notes-piquebone-arrow-status-exploit-fixed/
- https://twistedvoxel.com/elden-ring-patch-1-17-2/
- https://rutab.net/b/video-games/2026/05/28/dlya-elden-ring-vyshlo-mayskoe-obnovlenie-2026-goda.html
- https://eldenring.wiki.gg/wiki/Game_Version/1.16.1
- https://mp1st.com/title-updates-and-patches/elden-ring-new-update-1-23-released-on-aug-21-for-patch-1-16-1
- https://www.bandainamcoent.com/news/elden-ring-patch-notes-version-1-16

**EAC, bans, mods (via search results; LIKELY)**
- https://www.nexusmods.com/eldenring/mods/90 (EAC Toggler)
- https://www.nexusmods.com/eldenring/mods/98 (Offline Launcher)
- https://www.nexusmods.com/eldenring/mods/117 (Elden Mod Loader)
- https://www.nexusmods.com/eldenring/mods/510 (Seamless Co-op)
- https://windowscentral.com/how-disable-anti-cheat-elden-ring
- https://pcgamesn.com/elden-ring/inappropriate-activity-detected-fix
- https://www.gamepressure.com/S013-amp.asp?ID=17030
- https://nexusmods.com/eldenring/mods/541?tab=docs (Reforged launcher bundling ME2)
- https://www.pcgamer.com/games/rpg/one-of-elden-rings-best-overhaul-mods-just-received-a-massive-update-that-adds-fully-functional-multiplayer
- https://www.pcgamesn.com/elden-ring/reforged-2-0-mod-total-overhaul
- https://www.nexusmods.com/eldenring/mods/5192 (me3 + Convergence natives guidance)
- https://www.nexusmods.com/eldenring/mods/3596 (ersc.dll as me3 native)
- https://www.nexusmods.com/eldenring/mods/10388 (Convergence 3.0.4.1 on 1.17.2)
- https://nexusmods.com/eldenring/mods/428 (Item/Enemy Randomizer)
- https://www.nexusmods.com/eldenring/mods/7999 (EldenEncouragement, LLM overlay)

---

## Verification log

Adversarial fact-check, 2026-11-06. Method: fresh `git ls-remote`/`git fetch` of each repository (read only, nothing built or run), the crates.io API, and independent web search. Bandai Namco, Nexus, fandom/wiki.gg and thefpsreview could not be fetched directly, so those rows rely on search snippets. The web-search budget ran out before Reforged 2.0 and the Menu-Explanation pause trick could be checked. "Confirmed" means a primary source was read. "Corrected" means the original statement was wrong or materially incomplete and has been edited above.

| # | Claim | Verdict | Evidence (URL) |
|---|---|---|---|
| 1 | Current Steam build is 1.17.2, exe version 2.7.2.0, released 2026-10-20 | Confirmed (date and mapping); "still current on 11-06" LIKELY | https://github.com/vswarte/fromsoftware-rs/commit/b726336 ; https://github.com/Banon-Labs/er-mods-rs/blob/main/docs/game-build.md ; https://www.gamerfuzion.com/elden-ring-patch-1-17-2-notes-piquebone-arrow-status-exploit-fixed/ ; https://ixbt.games/en/news/2026/10/21/fromsoftware-vypustila-obnovlenie-dlia-elden-ring-ulucsili-bezopasnost-i-ustranili-bagi.amp.html . Nuance: fromsoftware-rs compares `dwProductVersion`, not the `FileVersion` string. |
| 1a | 1.17.1 shipped "~early Sep" | Corrected: 2026-09-08 | https://x.com/ELDENRING/status/2097236393856524780 ; https://mp1st.com/title-updates-and-patches/elden-ring-version-1-17-1-out-minor-update-1-018-001 |
| 1b | "Native mods have broken roughly every 4–8 weeks in 2026" | Corrected: bursty (9-month gap, then 4 builds in 5 months) | Patch table §1 (dates 2025-08-21, 2026-05-28, 08-27, 09-08, 10-20) |
| 1c | "Patch 1.18" exists | Refuted (April Fools article, 2026-04-01) | https://www.thefpsreview.com/2026/04/01/elden-ring-patch-1-18-adds-easy-mode-but-only-for-the-first-boss-and-only-if-you-ask-nicely/ |
| 2 | fromsoftware-rs main (`5702315`) accepts only WW 2.7.2.0 / JP 2.7.2.1 and panics in `rva::get()` otherwise | Confirmed (still HEAD on 11-06) | https://github.com/vswarte/fromsoftware-rs/blob/5702315/crates/eldenring/src/rva.rs |
| 3 | crates.io v0.14.0 (May 2026) targets 1.16.2, not 1.17.x | Confirmed (published 2026-05-29; tag `rva.rs` matches 2.6.2.0/2.6.2.1) | https://crates.io/api/v1/crates/eldenring ; https://github.com/vswarte/fromsoftware-rs/blob/v0.14.0/crates/eldenring/src/rva.rs |
| 4 | Task registration depends on the per-build `register_task` RVA | Confirmed, with a correction: `wait_for_instance` already panics through the `global_hinstance` RVA, before `run_recurring` | https://github.com/vswarte/fromsoftware-rs/blob/5702315/crates/eldenring/src/cs/task.rs ; `crates/eldenring/src/util/system.rs` |
| 4a | "Release profile `panic = "abort"`, so that panic kills the game" | Corrected: the workspace profile does not apply to downstream crates. Panics inside task closures abort anyway through `extern "C" fn execute` | https://github.com/vswarte/fromsoftware-rs/blob/5702315/Cargo.toml ; `crates/shared/src/task.rs` ; https://blog.rust-lang.org/2024/09/05/Rust-1.81.0.html |
| 4b | "Singletons are found through version-tolerant DLRF reflection" / "tested ER 1.16 → 1.17.2" | Corrected: from-singleton is a regex-pattern scanner whose README lists only ER 1.16 as tested. Several statics use RVAs | https://github.com/Dasaav-dsv/from-singleton ; `crates/eldenring/src/cs/{game_data_man,game_man,field_area}.rs`, `dluid/user_input_manager.rs` |
| 4c | Struct layouts may change (originally speculative, Open question 6) | Corrected to FACT: `ChrIns` changed in 1.17.0, `WorldChrMan` in 1.17.2 | https://github.com/vswarte/fromsoftware-rs/commit/b080915 ; https://github.com/vswarte/fromsoftware-rs/commit/5baf0dd |
| 5 | me3 forces `Menu.IsEnableOnlineMode=false` unless `start_online = true`; default false | Confirmed. Can also be set by the CLI `--online` flag; user `debug_properties` are applied after it | https://github.com/garyttierney/me3/blob/5f4fa47/crates/mod-host/src/host/game_properties.rs ; `crates/mod-host/src/lib.rs` ; `crates/cli/src/commands/launch.rs` (`unwrap_or(false)`) ; `crates/cli/src/commands/profile.rs` |
| 6 | me3 launches `Game/eldenring.exe` with `CREATE_SUSPENDED` and injects via `CreateRemoteThread`, bypassing EAC | Confirmed | https://github.com/garyttierney/me3/blob/5f4fa47/crates/launcher/src/game.rs ; `crates/mod-protocol/src/game.rs` |
| 7 | ModEngine2 archived July 2024 with "future work… me3" notice | Confirmed (archived 2024-07-19; last commit "Add notice of discontinuation") | https://github.com/soulsmods/ModEngine2 |
| 8 | "Seamless Co-op's official docs" say use `ersc_launcher.exe` or ME2; EML/Lazy Loader cannot load it | Confirmed for the text; "official" downgraded to "ERSC documentation site" (LukeYui authorship unverified) | https://github.com/ersc-docs/ersc-docs.github.io/blob/main/pages/seamless-modding.md |
| 8a | ERSC `VERSION` shows minimum `>2.02` (was FACT) | Downgraded to LIKELY (undocumented format); er-mods-rs pins ERSC 2.0.2 on 2.7.2.0 | https://raw.githubusercontent.com/LukeYui/EldenRingSeamlessCoopRelease/main/VERSION ; https://github.com/Banon-Labs/er-mods-rs/blob/main/build-support/prologue_build.rs |
| 9 | `DLUserInputManager` reads keyboard and mouse through DirectInput8 `GetDeviceState`; WndProc blocking is insufficient | Confirmed by two independent codebases | https://github.com/vswarte/fromsoftware-rs/blob/5702315/crates/eldenring/src/dluid/user_input_device.rs ; https://github.com/Banon-Labs/er-mods-rs/blob/main/crates/er-dinput-suppress-core/src/lib.rs |
| 9a | `InputBlocker` is reusable | Corrected: it lives in the unpublished `debug` crate and uses `ilhook` | https://github.com/vswarte/fromsoftware-rs/blob/5702315/crates/debug/Cargo.toml |
| 10 | libER's last game update is 1.16 (April 2025); no `WorldChrMan`/`ChrIns` headers | Confirmed for `main` (`d8ae5c9`, 2025-04-24). An abandoned `worldchrman-impl` branch exists (last commit 2024-07-10) | https://github.com/Dasaav-dsv/libER ; https://github.com/Dasaav-dsv/libER/tree/worldchrman-impl |
| 11 | er-mods-rs records `GetAndFormat` at `0x7633d0` (1.16.2) / `0x7646d0` (2.7.2.0) and detours it to replace UI text without FMG edits | Confirmed. The scope is narrow (Quit → Save Game rows), and the 1.16.2 address was measured on a de-Arxan'd image | https://github.com/Banon-Labs/er-mods-rs/blob/main/crates/er-title-flow/src/constants_autoload_state.rs |
| 12 | me3 `AI_CODE_POLICY.md` disallows all LLM use in code, issues, discussions, Discord | Confirmed (verbatim) | https://github.com/garyttierney/me3/blob/main/AI_CODE_POLICY.md |
| 12a | "The community is hostile to AI" | Corrected: core infrastructure is; er-mods-rs and unseamless-coop openly use coding agents | https://github.com/Banon-Labs/er-mods-rs/blob/main/AGENTS.md ; https://github.com/micthiesen/unseamless-coop/tree/main/.agents/skills |
| 13 | fromsoftware-rs ships MIT/Apache NON-AI licenses forbidding ML training; `Cargo.toml` says `MIT OR Apache-2.0` | Confirmed | https://github.com/vswarte/fromsoftware-rs/blob/5702315/LICENSE-MIT-NON-AI ; https://github.com/vswarte/fromsoftware-rs/blob/5702315/Cargo.toml |
| 14 | hudhook 0.9.3 (2026-09-08) hooks `IDXGISwapChain` Present/ResizeBuffers and `ID3D12CommandQueue::ExecuteCommandLists` with MinHook | Confirmed (crates.io publish 2026-09-08T14:11Z) | https://github.com/veeenu/hudhook/blob/0.9.3/src/hooks/dx12.rs ; https://crates.io/api/v1/crates/hudhook |
| 15 | TGA README lists app version 1.17.2 and recommends `steam_appid.txt` next to `eldenring.exe` | Confirmed (`393c439`, 2026-10-22; "Method 1 - Recommended") | https://github.com/The-Grand-Archives/Elden-Ring-CT-TGA/blob/main/README.md |
| 16 | Waygate is Apache-2.0, active October 2026 | Refuted: AGPL-3.0-only; last default-branch commit 2026-07-24 | https://github.com/vswarte/waygate-server/blob/main/LICENSE |
| 17 | me3 Linux partial-audio bug (#582) is a known issue | Corrected: fixed by PR #720, working in v0.11.0 | https://github.com/garyttierney/me3/discussions/582 |
| 18 | fromsoftware-rs updated "1.17.1 within about a week" | Corrected: same day (2026-09-08) | https://github.com/vswarte/fromsoftware-rs/commit/29fd138 |
| 19 | er-mods-rs uses "a git dependency pinned to a commit" | Corrected: a `path` dependency on a sibling fork, CI-pinned at `4612952` (2.7.2.0 + 2.7.1.0 + JP 2.7.1.1) | https://github.com/Banon-Labs/er-mods-rs/blob/main/docs/game-build.md |
| 20 | `character_id` is a useful secondary NPC key | Corrected: it is the model ID ("8000 for Torrent"); humans share c0000 (LIKELY) | https://github.com/vswarte/fromsoftware-rs/blob/5702315/crates/eldenring/src/cs/chr_ins.rs |
| 21 | `ChrIns` fields give "everything perception needs" | Downgraded to LIKELY "most"; no typed aggro/target state; `last_hit_by` holds only the last hitter | same file |
| 22 | me3 natives' `load_before`/`load_after` ids unknown (Open question 2) | Answered: the DLL file name | https://github.com/garyttierney/me3/blob/5f4fa47/crates/mod-protocol/src/native.rs |
| 23 | Natives with an initializer load on a separate thread | Corrected: every native from the first one with an initializer onward is deferred | https://github.com/garyttierney/me3/blob/5f4fa47/crates/mod-host/src/lib.rs |
| 24 | ER uses Wwise | Confirmed (rewwise handles ER `.bnk`/`.wem`) | https://github.com/vswarte/rewwise |
| 25 | No version-tolerant FMG text path known (Open question 5) | Partly answered: EwdenWing AOB-finds the MsgRepository lookup (1.17.x untested) | https://github.com/axd1x8a/EwdenWing/blob/main/src/lib.rs |
| 26 | Proton 11 support since me3 v0.12.0; v0.13.0 on 2026-08-11 | Confirmed | https://github.com/garyttierney/me3/releases |
| 27 | Tool last-push dates (Erd-Tools, DSMapStudio, ESDLang, DarkScript3) | Minor corrections: HEAD commit dates differ from GitHub push dates (Erd-Tools 2026-03-13, DSMapStudio 2024-07-20, ESDLang 2025-10-20, DarkScript3 2026-03-07) | `git fetch --depth=1` of each repository |
| 28 | Elden Ring Reforged 2.0 (2025-11-08) added its own servers | Unverified (search budget exhausted; remains LIKELY from the original snippets) | https://www.pcgamer.com/games/rpg/one-of-elden-rings-best-overhaul-mods-just-received-a-massive-update-that-adds-fully-functional-multiplayer |
| 29 | Only LLM mod found is EldenEncouragement | Corrected: also "Un-Alone" (Nexus 8046), built on it; still no in-process NPC dialogue framework found | https://www.nexusmods.com/eldenring/mods/8046 |
| 30 | Offline pause via Help → Menu Explanation | Unverified (added from memory, labeled LIKELY) | — |
