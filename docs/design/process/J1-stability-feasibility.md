# J1: Judge scorecard: stability and technical feasibility

> **Lens:** stability and technical feasibility on ELDEN RING as it is in November 2026 (App 1.17.2, exe 2.7.2.0 WW / 2.7.2.1 JP). The lens covers patch churn (RVAs and layouts move, a single-build upstream), crash safety (game-thread-only access, panics in task closures, pointer lifetimes, loading screens), version gating and graceful degradation, coexistence with other mods (Seamless Co-op, Reforged, overlays), the offline and EAC rules, and the licence gate.
> **Date:** 2026-11-06. **Inputs:** `P1-lean-monolith.md`, `P2-split-brain.md`, `P3-experience-first.md`, read in full. Every load-bearing technical claim was checked against `docs/research/eldenring/00–06` (cited `ER0n §x`), with `docs/research/skyrimnet/00` (`SN00`) for lessons.
> **Owner update (2026-11-06) applied:** no me3 dependency of any kind; our own Game Access Layer (GAL) instead of fromsoftware-rs (recommended default, owner to confirm); a first-class event engine. All three proposals predate the update. §4 lists every place they assume me3 or fromsoftware-rs, and §7 says what the lead architect must replace.

---

## 1. Sub-criteria (each scored 1–10)

| Code | Sub-criterion | What a 10 looks like |
|---|---|---|
| **K1** | **Version gate and patch-churn absorption** | The PE version check runs before any RVA- or layout-dependent code. The few function and static addresses are resolved at runtime. Layouts are per-build data checked by self-tests. A degradation ladder ends at "banner only". A layout-only patch can ship as a data update. The previous build stays supported. The game-access backend sits behind a narrow, swappable trait, and the licence options are costed |
| **K2** | **In-process crash safety** | Game memory is touched only on the game thread, and the code enforces it. No reference into game memory is ever formed. Access violations and panics are contained, and catch-all handlers trip breakers. No pointer survives a tick. Work stops during loading. Game-thread work is bounded and allocates nothing. Abort-class failures are designed out or isolated |
| **K3** | **Coexistence, loading and offline/multiplayer safety** | No hooks in game code. System-DLL hooks are installed once, chained and published. No game functions are called after registration. The DLL is loader-agnostic. Multiplayer, including Seamless Co-op, fails closed. The design copes with overhauls and randomizers, HDR, the Steam overlay and Proton |
| **K4** | **Evidence fidelity and v1 hook/call discipline** | Every claim matches the research's evidence level. Nothing in v1 rests on an unverified hook, a freeze or an unproven call path. Spikes come before dependent code. Gaps are labelled honestly |
| **K5** | **Perception and event robustness, and testability** | Exact detection conditions with debounce, hysteresis, dedup keys and TTLs. Re-validation at display time. Explicit ordering and backpressure. Per-source rates and costs. Deterministic replay of recorded snapshots in CI. This is the base for the owner's first-class event engine |

---

## 2. Scores

| Proposal | K1 gate and churn | K2 crash safety | K3 coexistence and offline | K4 evidence and hook discipline | K5 events and testability | **Total /50** |
|---|---|---|---|---|---|---|
| **P1 Lean monolith** | 9 | 9 | 7 | 8 | 8 | **41** |
| **P2 Split brain** | 8 | 8 | 6 | 8 | 8 | **38** |
| **P3 Experience first** | 8 | 7 | 6 | 6 | 7 | **34** |

**Winner for this lens: P1.** It has the strictest discipline around game memory (guarded copies, read groups with breakers, no calls after registration, CI-enforced FFI guards), the smallest and best-specified game-access surface, and the clearest path to the owner's own-GAL default. P2 has the best isolation and backpressure ideas but adds new platform risk. P3 is the best product design, but on this lens it has two discipline lapses: an optional hook on game code, and a premature "verified" flag.

---

## 3. Rationale per proposal

### P1 Lean monolith: 41/50

**K1 = 9.**
- The gate runs before everything (P1 §2.6 step 1) and goes to L1 with zero reads on an unknown build. This matches ER02 §5 step 1 and ER01 Implications 2.
- Layout profiles are data (`layouts/<build>.toml`), and 10 read groups each have their own self-test (P1 §2.6). The self-tests follow ER02 §5 step 4: RTTI subclass, ascending distances, `hp ≤ max_hp`, `npc_param_id` resolves. They add useful extras: `event_flag_divisor == 1000` and "every catalogue flag's group exists", which covers ER01 §9 caveats 1 and 3.
- A capability matrix maps each feature to the groups it needs, and fails closed.
- Unknown builds can enter an opt-in probation mode (observe-only for 10 minutes; P1 §2.6 U2). There is an in-game `probe` tool that recovers offsets from invariants, a data-only update path, and the previous build's profile is kept (P1 §11.5; ER01 §1).
- The `SysLayer` trait is the narrowest backend seam of the three: register_recurring, singleton, param_table, rtti_name (P1 §2.5). P1 is the only proposal that costs option (c) honestly, at about 600 lines, and it adds a CI `cargo deny` rule that fails on NON-AI licence files (P1 §11.6).
- *Deduction:* the patterns and the task shim come from fromsoftware-rs `mapper-profile.toml` and `acc9da0`, and P1 vendors `from-singleton` without having checked its licence (P1 §2.5). Both conflict with the owner's GAL default.

**K2 = 9.** The best crash-safety rules of the three (P1 §2.8 R1–R15):
- one reader;
- **no writes and no game-function calls after registration** (R2);
- every foreign entry point wrapped in `ffi_guard`, with a CI grep check (R3);
- an **SEH-guarded `GuardedReader::copy::<T: Pod>`**: no `&T` into game memory is ever formed, and vtables must lie in the module's `.rdata` (R4). SkyrimNet runs its pool tasks under an SEH handler too (SN00 §3 item 8);
- breakers per read group: 3 faults in 60 s (R9);
- a clippy deny-list, with `unsafe` confined to three modules (R10);
- idle while loading (R11), self-throttle (R14), no work at `DLL_PROCESS_DETACH` (R15).

This answers the ER05 §6.1 streaming-pool lesson and SN00 §3 (#819/#830/#831).

*Deductions:*
- The whole brain runs in process: TLS, SQLite's C code, regex and minijinja over untrusted LLM output. Abort-class failures (stack overflow, OOM, C faults) cannot be caught by `catch_unwind`. P2 §2.8 makes this point, and P1 does not discuss it.
- The guarded `ChrIns` prefix copy is "≈0x600 bytes" (R4), but the 1.17.2 base `ChrIns` ends at `0x580` (ER05 §6.2, the TGA-name row). Copy sizes must come from the layout profile, per subclass.
- P1 does not say that singleton *instance* pointers are re-read from their static slot every tick, although `WorldChrMan` moves on world transitions (ER00 §4 item 3; ER05 Risks).

**K3 = 7.**
- No game-code hooks (R12).
- One MinHook copy, the one hudhook already bundles. This avoids the two-MinHook trampoline corruption described in ER02 §7.1 and ER03 Risks ("only the last to enable was ever called").
- Input hooks are chained, gated by an atomic, and installed once.
- The FMG text reader is hook-free and reads the in-memory table directly (`[MsgRepository]+0x08` / `+0x14`; ER01 §8.3). It therefore never collides with Reforged's `GetMessage` MinHook.
- The **identity-pack check** compares live `NpcParam.nameId` with the expected values. If more than 20% differ, personas are disabled, so an overhaul or randomizer cannot attach a persona to the wrong character (P1 §4.2; ER02 §6).
- Multiplayer fails closed ("must pass, or AI stays off", P1 §2.6 and D16).
- *Deductions:* P1 is built around me3. Offline enforcement, Arxan handling and save separation are all delegated to the profile (P1 §8.4). Its bootstrap assumes "after me3 loads the native post-main" (P1 §2.6) and has no readiness wait for early loading.

**K4 = 8.**
- Claims match the research. The freeze is deferred behind S5 (ER05 §11). The talk observer fails closed until S3 (P1 §5.8; ER03 Risks). The combat heuristic is the ER05 §8.1 one. The S0 pass criterion includes "20 forced-fault runs". `hud_state == HideAll` for cutscenes is marked LIKELY.
- *Deduction:* the profile sets `disable_arxan = true` (P1 §8.4) while R12 argues that v1 needs no Arxan handling. The flag hides the untested question of whether the runtime AOB and singleton scans work on an image that has not been neutralised (ER02 §5: "Arxan-encrypted functions may be unreadable until Arxan runs or is neutered").

**K5 = 8.**
- Exact event conditions with merge keys (`hit:a:v`, 2.5 s), TTLs, hysteresis (LowHp below 0.25, re-armed at 0.35), debounce ("stable for 2 s") and dedup (per place per 10 min) (P1 §3.4).
- One `visible_to()` rule for every consumer (P1 §3.5; SN00 §3 #723).
- Re-validation at display time (P1 §6.3; #784).
- A pure `Core::step(input, now)` with time as an input. Replay is therefore exact, including scheduler expiry (P1 §2.4, §11.2).
- A synthetic snapshot DSL, AdapterMsg recordings, per-source rates (4 Hz snapshot, 1 Hz × about 700 flags ≈ 0.1 ms, render feed every frame) and game-thread cost targets (C3).
- *Deduction:* the detection rules are hard-coded Rust tables. Only the bark triggers are data. There is no confidence field and no explicit bus.

### P2 Split brain: 38/50

**K1 = 8.**
- The gate runs before any game call, and on an unknown build the brain is not even launched (P2 §2.5).
- Runtime AOB plus layout profiles and a probe build, bridge-only patch releases (the patch blast radius really is smaller), N−1 profiles, and `allow_unverified_build` off by default (P2 §11.6, §13 D3–D4).
- *Deduction:* the crate graph has `eldennet-game → FRS` and a `vendor/fromsoftware-rs` tree (P2 §2.2, §12.1). It leans on fromsoftware-rs more widely than P1 does.

**K2 = 8.**
- The **`&GameThread` zero-sized token** means no code off the game thread can even call a reader (P2 §2.1). This is the only proposal that enforces R1 in the type system.
- The in-game DLL has no networking, database, template engine or LLM parser (P2 §2.2 rule 2). That is the smallest in-process attack surface.
- Brain crashes are isolated, and the bridge alone keeps the game safe on disconnect (P2 §2.7).
- *Deductions:*
  - There is **no access-violation-guarded read path**. R3 is `catch_unwind` only, which does not catch an AV from a stale pointer. RTTI and plausibility checks run only at self-test time (P2 §2.5).
  - The game process gains a supervisor that calls `CreateProcess`, IPC threads and a Job Object. These are new moving parts inside the most fragile process.

**K3 = 6.**
- Hooks are chained, there is one ImGui context, and hudhook's MinHook clash is on a published conflict list (P2 §9).
- *Deductions:*
  - An unsigned or new exe spawned by a DLL injected into a game draws antivirus and SmartScreen attention. P2 admits this itself (§2.8, Risk 2), and me3 is quarantined often enough already (ER02 §3.2).
  - Process spawn and named pipes under Proton are unknown (its own S0b).
  - The Seamless Co-op test only looks for an extra `PlayerIns` "within 40 m" (P2 §2.6). A partner farther away is missed if `LobbyState` stays `None` under ERSC's own lobby (ER02 §2.3). P3's whole-set count is better.
  - P2 is built around me3, like the others.

**K4 = 8.**
- Honest about deviating from the research's single-DLL recommendation (ER00 §5 v1 item 1; ER06 §5.1), and it puts the deviation behind a spike (S0b).
- The `hud_state` enum is correct (ER05 §9). Cutscene detection is marked SPECULATION.
- *Deduction:* `resolve_text` does not say how it reads text (P2 §2.4). If it calls `GetMessage`, that is a game-function call through a function Reforged hooks (ER01 §8.3), and it goes beyond the owner's "reads and task registration only".

**K5 = 8.**
- Event store classes P/K/T and an explicit engagement state machine (P2 §3.3–3.4).
- A **`FlagDelta`** watcher that sends only changes (P2 §2.3).
- The **most explicit ordering and backpressure** of the three: latest-wins snapshots, bounded reliable queues of 1,024 per direction, barks dropped first, stall detection at 2 s, and bark deadlines stamped in the bridge's clock so stale barks are dropped at render time (P2 §2.3).
- `bridge-sim` and protocol backpressure tests (P2 §11.1, §11.4).
- *Deduction:* the rules are hard-coded too, with no confidence field.

### P3 Experience first: 34/50

**K1 = 8.**
- The gate also checks `ProductName == "ELDEN RING"`. An unknown build gets "no task registration and no `wait_for_instance`", which is exactly the ER02 §4.1 panic path (P3 §2.5).
- Runtime AOB, data-only layout updates with every earlier build kept, and an offline `probe-exe` that runs patterns against an exe file without starting the game (P3 §11.4).
- The ladder has a well-reasoned rung: if `FLAGS` fails, named chat goes off, because spoiler safety outranks availability (P3 §2.5).
- *Deduction:* there is no opt-in for unknown builds, and no in-game offset-recovery tool.

**K2 = 7.**
- One reader, `catch_unwind`, a 1 KB allocation cap per tick, handles only, and a supervised brain thread that restarts once (P3 §2.7).
- *Deductions:*
  - RTTI is checked "the first time it reads each object kind in a session" (R1). Characters stream in and out (ER05 §6.1), so the vtable check belongs on every object, every tick (a cheap compare against cached vtable addresses).
  - The "committed-page" check stops reads of unmapped pages, but not reads of freed-but-mapped garbage, and there is no AV guard.

**K3 = 6.**
- **Dormant mode in multiplayer:** the task stays registered but reads only `SESSION`, which is a clean way to stay safe in a session. The multiplayer rule adds `player_chr_set.len() == 1` to catch Seamless Co-op (P3 §2.5; ER05 §3).
- *Deductions:*
  - It proposes an **optional 2.7.2.0-only "menu has the mouse" hook** on `eldenring.exe` code if S4 shows camera drift (P3 §5.2; ER03 line on er-overlay-input `0x140766b00`; ER05 §11 option 4). That contradicts its own §2.1 rule ("No hooks or patches on `eldenring.exe` code") and the owner's v1 rule. It would also bring Arxan code restoration into play (ER02 §5).
  - The call-out box captures all input while unengaged hostiles may be very close ("an unaware sentry is fair game"; P3 §6.7). The world does not pause (ER05 §11), so the player can be attacked while unable to act. P1 and P2 require distance.
  - P3 is built around me3, like the others.

**K4 = 6.**
- The optional game-code hook above.
- Flag 9000 is marked `verified = true` in the catalogue before the S6 live log (P3 §3.3). ER05 §3 confirms its behaviour from a decompiled, modded copy of the grace ESD, not from a live 1.17.2 log.
- "40 m is comfortably inside proven territory" (P3 §3.1) leans on AoER's 80 m on **1.17.1**, but `WorldChrMan` fields shifted in 1.17.2 (ER06 §4; ER02 §1).
- The snapshot cadence is "every 15th frame" (P3 §2.1), so it changes with frame rate (frame-unlock mods, drops). P1 and P2 accumulate time instead.

**K5 = 7.**
- Every event carries an explicit audience (P3 §3.6).
- **Golden event tests over recorded sessions**, fed through the real backend trait (`en-game-replay` implements `GameBackend`). A change to the differ that alters events fails CI (P3 §11.1).
- An LLM mock covers every failure path.
- *Deductions:* the rules are hard-coded. "Events are never dropped" when the store queue is full (P3 §2.2) can stall the brain. That is safe for the game thread but unbounded.

---

## 4. Impact of the owner update (2026-11-06)

### 4.1 Where each proposal assumes me3 or fromsoftware-rs (all must be replaced)

| Assumption | P1 | P2 | P3 |
|---|---|---|---|
| **me3 is the loader**, and natives load after game main | §0, §1.1 G-install, §2.6 bootstrap "after me3 loads the native post-main", §8.4 paths `<me3 mods>/` | §0 "loaded by me3", §2.8, §12.1 zip layout | §2.1, §8.4 `<me3 mod folder>`, G7 |
| **Offline** enforced by me3 `start_online = false` (a write to the `Menu.IsEnableOnlineMode` property; ER02 §2.1) | §8.4 profile | §12.1 profile | §2.1 |
| **Arxan** neutralised by me3 `disable_arxan = true` | §8.4 (contradicts its own R12) | §12.1 ("v1 hooks no game code", yet sets it) | §2.1 |
| **Save separation** by me3 `savefile` | §8.4, §8.6 wizard notice | §12.1 | §2.1 |
| **fromsoftware-rs** pre-change vendor (licence option b) as the default | D5, §2.5 `en-er-sys`, §13.2 U1 | §2.2 `FRS`, §2.4 `FrsAccess`, §12.1 `vendor/`, §13.2 D1 | §2.8, §12.1 `third_party/`, §13.2 D1 |
| AOB patterns taken from fromsoftware-rs `mapper-profile.toml` | §2.5 | §2.4 | §2.5 |
| Spike S0 framed around me3 | §12.2 S0 | §12.2 S0a/S0b | §12.2 S0 |

**Effect on scores:** none of this changes the ranking. The more of its game-access layer a proposal already owns, the cheaper the switch:
- **P1** owns the gate, resolver, layouts, guarded reader and FMG reader, so only `en-er-sys` changes.
- **P2** keeps `FrsAccess` as the default.
- **P3** vendors "task registration types, from-singleton use, param repository access".

### 4.2 Coverage of the owner's event engine (requirement 3)

| Part | P1 | P2 | P3 |
|---|---|---|---|
| (a) Snapshot-diff source | Yes (§3.4) | Yes (§3.3) | Yes (§3.6) |
| (a) Flag watcher over a curated catalogue | Yes: `SetFlagWatch`, 1 Hz (§3.1) | Yes: `WatchFlags` / `FlagDelta` (§2.3) | Yes: flag frame bitset (§3.3) |
| (a) Hook sources reserved behind the same interface | No | No | Partial: a `NativeLineHeard` event type (§10) |
| (b) Declarative rules (debounce, hysteresis, cooldown, confidence, dedup) | Conditions are exact but hard-coded; debounce, hysteresis and dedup are present; no confidence | Hard-coded; no confidence | Hard-coded; no confidence |
| (c) Typed events with playtime, actors, place, audience | Yes (§3.4–3.5, §7.7) | Yes (§3.3, §3.5) | Yes, with audience per event (§3.6–3.7) |
| (d) Internal pub/sub bus with ordering and backpressure | Implicit in `Core::step` | **Best** (§2.3) | Partial (§2.2 queues) |
| (e) Rates and costs per source | **Best** (§3.1, C3, R14) | Yes (§3.2) | Yes (§3.1) |
| (f) Replay harness | **Best**: deterministic core plus DSL (§2.4, §11.2) | Yes: bridge-sim (§11.1) | **Best**: golden events through the backend trait (§11.1) |

None of the three has a first-class, data-driven event engine. P1 has the base to build on (pure core, exact conditions, costs), plus P2's bus rules and P3's golden replay tests.

---

## 5. Claim check (load-bearing claims against the research)

| # | Claim (proposal) | Research | Verdict |
|---|---|---|---|
| 1 | fromsoftware-rs `rva::get()` panics on unknown builds, first in `wait_for_instance` (P1 §2.5, P2 §2.4, P3 §2.5) | ER02 §1, §4.1 | **Correct** |
| 2 | A panic in a task closure aborts the game since Rust 1.81, so use `catch_unwind` with `panic = "unwind"` (all) | ER02 §4.1 | **Correct** |
| 3 | Layouts moved (`ChrIns` +8 in 1.17.0; `WorldChrMan` +0x10 in 1.17.2) (all) | ER02 §1 | **Correct** |
| 4 | `register_task`, `GameDataMan`, `GameMan` and `FieldArea` (P2 and P3 add `global_hinstance`) are AOB/RVA items; manager singletons come from the FD4 finder (all) | ER01 §4, ER02 §4.1 | **Correct**. P1 omits `global_hinstance` and specifies no other readiness wait. ER02 §4.1 says DLRF metadata must be populated before the singleton scan |
| 5 | "Task registration is thread-safe by contract" (P1 R1) | ER01 §3.2 ("`CSTaskImp` itself is documented as thread-safe"); `wait_for_instance` must be called off the main thread (ER01 §3.1) | **Correct** |
| 6 | The game task system avoids Arxan problems, so v1 needs no neutralisation (P1 R12, P3 §2.1) | ER02 §5 (avoids *code restoration*), but "Arxan-encrypted functions may be unreadable until Arxan runs or is neutered" | **Half right.** Reads and task registration do not trigger code restoration, but runtime AOB scans of possibly encrypted code have only been exercised with dearxan or after me3's post-main load. **This is a new blocking spike** (§8) |
| 7 | The FMG table can be read hook-free at `[MsgRepository]+0x08` / `+0x14` (P1 §2.5) | ER01 §8.3 (MapForGoblins, 2.6.2.0) | **Correct, needs S2 on 1.17.2** |
| 8 | Reforged MinHooks `GetMessage` (implicit in P1's hook-free choice) | ER01 §8.3, §4 interior-anchored AOB | **Correct.** That is a reason to read tables rather than call or hook the function |
| 9 | `hud_state` enum: 0 HideAll, 1 ShowAll, 2 PopupMenu, 3 Default (P2 §3.1) | ER05 §9 | **Correct** |
| 10 | SpEffect 9621 marks a safe context (P1, P2, P3) | ER05 §3: Roundtable Hold only in single player | **Correct as used** (all scope it to Roundtable) |
| 11 | Flag 9000 means "resting" and is `verified = true` (P3 §3.3) | ER05 §3: confirmed via a decompiled, modded ESD copy, not a live log | **Overstated.** Keep `verified = false` until S6 |
| 12 | `ChrIns` prefix copy ≈ 0x600 bytes (P1 R4) | ER05 §6.2: the 1.17.2 base `ChrIns` ends at 0x580; subclasses differ | **Imprecise.** Size the copies per class from the layout profile |
| 13 | `SetDisableAllChrUpdate` freeze deferred (all) | ER05 §11: single source, 2.6.2.0, foreign thread only | **Correct** |
| 14 | The talk observer is unverified, so fail closed (all) | ER03 §2.5, Risks | **Correct** |
| 15 | The "menu has the mouse" hook is an optional v1 fix (P3 §5.2) | ER05 §11 option 4 and ER03 (er-overlay-input `0x140766b00`, 2.7.2.0 only) | **Contradicts v1 rules.** It is a game-code hook pinned to one build |
| 16 | `player_chr_set.len() == 1` catches Seamless Co-op (P3 §2.5) | ER05 §3 (`PlayerIns` set; ERSC has its own lobby, ER02 §2.3) | **Plausible, S1.** Better than P2's 40 m radius test |
| 17 | AoER proves the snapshot export at 80 m (P2 §2.4, P3 §3.1) | ER06 §4: 1.17.1 only, me3-loaded, **layouts from fromsoftware-rs** | **Correct as prior art,** but under the owner update its code is not reusable as a dependency (it inherits fromsoftware-rs layouts) |
| 18 | NPC IDs: Varré `523010000`, entity `1042360700`, block 3180; Alexander `44909000`; Kalé `32000000`; Renna/Ranni `20500010`/`20500000` (P1, P2, P3) | ER04 §1, §7; ER05 §6.3 | **Correct** (marked unverified where the research says so) |
| 19 | A `dinput8`/`winhttp` proxy makes EAC refuse to start (none claim it) | ER02 §2.1: LIKELY, not guaranteed | Relevant to §7 launcher grafts |

---

## 6. Fatal flaws and wrong claims

"Fatal" means it must change before the design can be adopted on this lens.

**Common to all three:**
1. **FATAL (owner update).** Offline enforcement, Arxan neutralisation and save separation are delegated to me3 (`start_online`, `disable_arxan`, `savefile`; P1 §8.4, P2 §12.1, P3 §2.1). The owner forbids any me3 dependency. me3's offline switch is also a write to a game property (ER02 §2.1), which v1 must not do itself.
2. **FATAL (owner update).** fromsoftware-rs is the default backend (option b), and the AOB patterns come from its `mapper-profile.toml` (P1 D5/§2.5, P2 §2.4, P3 §2.8). The owner's default is our own GAL built from documented facts. Where patterns come from (our own derivation, or attributed pre-change MIT data) becomes an owner decision.
3. **Hidden assumption.** `disable_arxan = true` masks the untested question of whether runtime AOB scans and the FD4 singleton finder resolve on an image that has not been neutralised. ER02 §5 says encrypted functions may be unreadable. P1 and P2 even argue that v1 needs no Arxan handling while setting the flag.
4. **Gap.** All three assume natives load after game main (P1 §2.6). Our own launcher, or a proxy DLL, loads earlier. The init thread must wait for engine readiness before scanning: the `global_hinstance` RVA or an equivalent, and populated DLRF metadata (ER01 §3.1, ER02 §4.1). Only P2 and P3 even list `global_hinstance`.
5. **Gap.** None of them has a first-class event engine: declarative rules with confidence, a typed bus with ordering and backpressure, and reserved hook sources (§4.2).
6. **Gap.** None states that singleton *instance* pointers are re-read from their static slot every tick. `WorldChrMan` moves on world transitions (ER00 §4 item 3; ER05 Risks).

**P1:**

7. Abort-class failures in process (stack overflow, OOM, faults in SQLite's C code or TLS) are not discussed, and `catch_unwind` does not cover them (compare P2 §2.8). This is acceptable for v1 if it is mitigated: recursion limits, bounded buffers, no ML in process. It must be stated.
8. The guarded copy size is a fixed "≈0x600" (R4), although the base `ChrIns` ends at 0x580 (ER05 §6.2).
9. The SEH guard has not been tested next to mods or crash reporters that install vectored exception handlers. These fire before frame handlers (SPECULATION). Add it to S0.

**P2:**

10. **No AV-guarded reads.** `catch_unwind` alone (P2 R3) cannot contain a stale-pointer read on the game thread, and that is a crash.
11. Spawning a child process from inside the game, plus a named pipe and a Job Object (P2 §2.7), adds Defender, SmartScreen and Proton risk (its own S0b) for a benefit the research defers to later (ER00 §3.3; ER06 §5.1). It is not minimal for v1.
12. The text-resolution method is unspecified (P2 §2.4). A `GetMessage` call would be a game-function call through a hooked function (ER01 §8.3).
13. The Seamless Co-op test is limited to 40 m (P2 §2.6).

**P3:**

14. **The optional 2.7.2.0-only hook on `eldenring.exe` code** (P3 §5.2) contradicts P3 §2.1 and the owner's v1 rule (reads, task registration and system-DLL hooks only). Drop it. If the camera drifts in S4, the answer is chat only in safe contexts, not a game hook.
15. Flag 9000 is `verified = true` before the live log (P3 §3.3; ER05 §3).
16. RTTI is validated only once per object kind per session (P3 R1), although the pools stream (ER05 §6.1).
17. The snapshot cadence counts frames, not time (P3 §2.1).
18. The call-out box captures input while hostiles may be very near (P3 §6.7; ER05 §11).

---

## 7. Grafts for the final design

### 7.1 Game Access Layer (owner default: our own; keep it swappable)

1. **P1's narrow backend trait** (`SysLayer`: `register_recurring`, `singleton`, `param_table`, `rtti_name`; P1 §2.5). Our own code is the default implementation. A fromsoftware-rs-backed implementation remains possible only if the owner chooses it. Add **P2's `&GameThread` zero-sized token** to every read method (P2 §2.1, §2.4).
2. **Our own FD4 singleton finder,** written from the documented idiom: the `.text` byte shape, `.data`/`.rdata` placement, a path ending in `FD4Singleton.h`, and the game's own name getter (ER01 §4). **Our own task shim:** a fake FD4 task vtable plus the `register_task` call (ER01 §3.2). That is about 600 lines in P1's estimate.
3. **Keep the AOB-dependent surface tiny.** Only `register_task`, `global_hinstance` (or another readiness signal), `GameDataMan`, `GameMan` and `FieldArea` need patterns (ER01 §4, ER02 §4.1). Everything else is an FD4 singleton: `WorldChrMan`, `CSEventFlagMan`, `CSFeMan`, `CSMenuMan`, `SoloParamRepository`, `MsgRepository`, `CSSessionManager`, `WorldAreaTime`, `CSCamera`, `CSNowLoadingHelper`, `CSTask` (ER02 §4.1, ER05 §1). Prefer a singleton-backed field when both exist, for example boss bars from `CSFeMan` rather than `GameDataMan` (ER05 §8.4). Anchor patterns **inside function bodies** (entry+5 or later), because other mods MinHook prologues (ER01 §4). Validate every hit by RTTI, prologue and length.
4. **P1's read groups, self-tests and capability matrix**, with a **breaker per group** (3 faults in 60 s) and a toast (P1 §2.6, R9). Use P1's 10-group table as the canonical inventory of the game objects v1 needs.
5. **P1's SEH-guarded `GuardedReader::copy::<T: Pod>`**: no `&T` into game memory, a canonical-address check, the vtable in the module's `.rdata`, batched copies sized **per class from the layout profile** (P1 R4). Add a **per-object, per-tick vtable compare** (fixing P3 R1's once-per-kind check). **Re-read singleton statics every tick** and treat null as "loading".
6. **P1's hook-free FMG reader** (`[MsgRepository]+0x08`/`+0x14`; ER01 §8.3), so that v1 calls **no game function after task registration** (P1 R2).
7. **P1's identity-pack check** against overhauls and randomizers (P1 §4.2).
8. **The build key** is PE `FileVersion` plus region (all proposals) plus **P3's `ProductName` check**. Also consider adding the PE `TimeDateStamp` and `SizeOfImage`, to catch a rebuild that keeps the version number (SPECULATION, cheap).

### 7.2 Version gate, ladder and patch procedure

9. Unknown build: L1 banner, zero tasks and zero reads (all three). Keep **P1's opt-in probation** (observe-only for 10 minutes, default off) or P2's `allow_unverified_build`.
10. **P3's ladder rung:** if `FLAGS` fails, named chat goes off. Also **P3's L0 Dormant:** in a multiplayer session the task stays registered but reads only the session group.
11. **Patch tooling:** P3's offline `probe-exe` (runs the patterns against an exe file), P1's in-game `probe` (recovers offsets from invariants), data-only updates when only offsets moved, and N−1 profiles kept (P1 §11.5, P2 §11.6, P3 §11.4).

### 7.3 Crash rules and CI

12. P1 R1–R15 as the normative list. Add P1's CI checks: every `extern` body starts with `ffi_guard`, the clippy deny-list, `unsafe` confined to modules, and a `cargo deny` licence allowlist that **rejects `*NON-AI*` files** (P1 §11.6).
13. **No allocation on the game thread** (P1 R5; P3's ≤ 1 KB per tick), **time-based cadence** (P1/P2), and self-throttle (P1 R14).
14. **State the abort-class risk** and mitigate it: recursion and size limits on templates and parsers, and no ML in process. Keep **P2's split brain as a v1.x seam** behind P1's `CoreTransport` trait (P1 §10), not as the v1 default.

### 7.4 Input and overlay

15. **P2 R7 input-release rules:** release capture on focus loss, after 2 s without overlay renders, on Esc and on danger. Held keys stay masked until released. Capture is a single atomic. Hooks are **installed once at boot and never toggled** (P2 §2.7; ER02 §7.1 and ER03 Risks on MinHook contention). Use one hooking library: hudhook's MinHook (P1 §2.2).
16. **No game-code input hook** (reject P3 §5.2). Chat opens only in a safe context. P1/P2's distance gates also apply to addressing a generic NPC.

### 7.5 Event engine (owner requirement 3)

17. Build on **P1's pure `Core::step(input, now)`** and its exact condition table (P1 §2.4, §3.4), turned into **data rules**. Each rule carries a condition, debounce, hysteresis re-arm, cooldown, confidence, dedup/merge key and TTL.
18. **P2's bus rules:** a latest-wins snapshot slot, bounded reliable queues, barks dropped first, and the game thread never blocks (P2 §2.3). Add **P3's "diagnostics dropped first"** (P3 §2.2), but bound the event queue as well.
19. **The flag watcher** validates every catalogue ID's group against the descriptor map at startup, because unallocated groups read false (ER01 §9). It asserts the divisor is 1000 (P1 §2.6). Unverified flags gate only in the restrictive direction (P1 §7.5, P2 §7.4).
20. **Reserved hook sources behind the same interface:** the flag setter (TGA AOB), damage slot 7 / `HitChr`, and `CSEzStateTalkEvent::invoke` (ER01 §9, ER05 §8.2, §8.6). Each needs Arxan handling, so all of them are **off in v1** (ER02 §5).
21. **The replay harness:** P3's golden event files over recorded sessions, fed through the real backend trait (P3 §11.1), P1's synthetic snapshot DSL (P1 §11.2), and P2's backpressure tests (P2 §11.4).
22. **Display-time re-validation** and **bark deadlines checked in the presenter's own clock** (P1 §6.3, P2 §2.3; SN00 §3 #784).

### 7.6 Loading without me3, offline and saves (from the research; no proposal covers this)

23. **Our own launcher:** start `eldenring.exe` directly with `steam_appid.txt`, the method TGA recommends, so EAC never runs (ER02 §2.1). Create the process suspended and inject with `LoadLibraryW`, the pattern of me3's launcher and LukeYui's Offline Launcher (ER02 §2.1, §3.2). The DLL itself stays loader-agnostic: `DllMain` only spawns the init thread, which **polls for readiness** before any scan (ER01 §3.1).
24. **Proxy DLLs only as a documented fallback.** `dinput8.dll` conflicts with Elden Mod Loader, ME2's dinput8 mode and the practice tool. It cannot carry Seamless Co-op, and it needs `WINEDLLOVERRIDES` under Proton (ER02 §3.3). `winhttp.dll` is used by EROverlay and the EAC Toggler (ER02 §2.1, §3.3). `dxgi.dll` would sit under hudhook's own DXGI hooks and clash with other DXGI injectors (SPECULATION). A proxy's "EAC refuses to start" property is only LIKELY (ER02 §2.1).
25. **Seamless Co-op players** cannot use our launcher for ERSC, because ERSC must start from `ersc_launcher` or a mod engine (ER02 §2.3). Document the path where the player adds EldenNet as a native in their own me3 profile. That is compatibility, not a dependency.
26. **Offline enforced in our own code:** refuse to activate unless `multiplay_role == Host && is_my_world && LobbyState == None && player_chr_set.len() == 1`, re-checked every snapshot (ER05 §3, P3 §2.5). Refuse if an EasyAntiCheat module is loaded in the process (SPECULATION, cheap). **Never write `Menu.IsEnableOnlineMode`**: that is a write.
27. **Saves (owner decision).** Recommended: **no save redirection in v1.** A file-API hook would be one more system-DLL hook, and it could collide with me3's `savefile` and ERSC's `.co2` handling (ER02 §2.3, §3.2). v1 never writes game state, so the `.sl2` is never touched by EldenNet. Instead, the launcher keeps rotating backups of `ER0000.sl2`, and the docs warn against taking a modded-offline save online (ER02 §2.2).

---

## 8. New blocking spikes created by the owner update

| Spike | Question | Pass criterion |
|---|---|---|
| **S0-A Unneutralised image** | On vanilla 2.7.2.0 and 2.7.2.1, with no dearxan and no other mods, do our own FD4 finder and our AOBs (`register_task`, `global_hinstance`, `GameDataMan`, `GameMan`, `FieldArea`) resolve, both under early load (suspended injection) and late load? Do they still resolve when another mod has altered the image (me3 with `disable_arxan`, ERSC, Reforged's prologue hooks)? | All resolve in 20 of 20 launches per mode; otherwise L1, with no crash |
| **S0-B Task without dearxan** | Does a registered recurring task tick for 2 hours with no Arxan neutralisation, with no code-restoration interference and no frame hitches? | 0 faults; task p99 ≤ 0.5 ms |
| **S0-C Launcher and offline** | Does our launcher start the game without EAC (Windows and Proton)? Does the session gate read correct values solo and in an ERSC session (ERSC loaded via the player's me3 profile)? Is a FromSoftware connection impossible? | Gate false in every session; no online menu reachable |
| **S0-D Guarded reads among other mods** | SEH-guarded reads with forced faults, next to ERSC, the Steam overlay and a VEH-installing crash reporter | 20 of 20 forced faults caught; breakers trip; no crash |
| **S0-E Hook coexistence** | DXGI, DirectInput and XInput hooks installed once at boot, in both load orders, next to another DInput-hooking overlay | Both mods' handlers run; no input leak |

These sit next to research spikes S1–S4 and S6 (ER00 §7), which still apply unchanged.

---

## 9. Verdict

**P1 wins on stability and feasibility (41/50), ahead of P2 (38) and P3 (34).**
- **P1** treats game memory most carefully and has the smallest, best-inventoried game-access surface. Its swappable `SysLayer` seam already names option (c), which the owner's update makes the default.
- **P2** adds the type-level game-thread token, the input-release rules and the clearest backpressure design. Its split-process default is a v1.x option, not a v1 foundation.
- **P3** brings the multiplayer-dormant ladder, the ERSC set-count check and golden event replay. It must drop its optional game-code hook.

None of the three is adoptable as written after the owner update. All three must replace me3 (loading, offline, Arxan, saves) and fromsoftware-rs (the backend), and all three must add a first-class event engine (§4, §7).
