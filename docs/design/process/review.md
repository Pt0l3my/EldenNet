# Adversarial review of the v1 design set

> **What this is:** the record of the adversarial review of the four v1 design documents, 2026-11-06. It lists what the review changed, how each conflict reported by the detail writers was resolved, and what is still open. Code **REVIEW**.
> **Reviewed:** [DESIGN.md](../DESIGN.md), [architecture.md](../architecture.md) (ARCH), [npcs-and-dialogue.md](../npcs-and-dialogue.md) (NPC), [memory-and-knowledge.md](../memory-and-knowledge.md) (MEM). Checked against the two research summaries, read in full (ER00, SN00), and the detail notes the design depends on (ER01 §3–§11, ER02 §2–§7, ER04 §1–§2, ER05 §1–§11, ER06 §5–§6), plus the owner update of 2026-11-06.
> **How to read it:** §1 is the verdict. §2 lists the edits by review task. §3 resolves every conflict the detail writers reported. §4 is what remains open, and for whom. Section numbers such as "DESIGN §8.1" point at the documents as they stand after this review.

---

## 1. Verdict

The design set is sound, buildable and faithful to the user's four requirements and to the owner update. It needed consolidation rather than redesign. Each detail document had quietly settled points that DESIGN.md left open or stated differently: the flush rule, the gate at the title screen, self-test placement, group labels and the memory-job output shape among them. The review folded those settlements back into DESIGN.md, so the master document again agrees with its detail documents. Two real defects were found and fixed:
- a fallback in risk K2 that relied on a loader's Arxan option, which breaks the no-me3 rule;
- committed test recordings that would have carried runtime FromSoftware text.

Two feasibility gaps were exposed and assigned to spikes: whether NPC summons sit in `player_chr_set`, which affects the multiplayer gate, and how text reads under mods that hook `GetMessage`. The one blocking unknown is still S0-A: does discovery work on an image whose Arxan protection is untouched? Two features are now marked as the first to cut if the schedule slips (O23 bark pools, O24 floating labels). Nothing else is beyond the minimal v1 without a stability or seam reason.

---

## 2. Changes applied

### 2.1 Cross-document consistency (task 1)

| Topic | Before | After | Files |
|---|---|---|---|
| Crate graph and tool list | DESIGN §2.2 lacked the `en-tools → en-gal` and `en-launcher → en-gal` edges and the proto edges, and listed 10 tools | Edges added (offline modules only); tools add `identity-fill`, `kb-draft` and `flag-verify` | DESIGN §2.2, ARCH §2.1, §12.3, §13 |
| `GameThread` token | "Only the trampoline or replay can build it" (not expressible in Rust) | `!Send` token plus a CI grep that allows the constructor in two files | DESIGN §2.3 |
| Dormant | Drawn as a state reached from InWorld only | Noted as an orthogonal flag that also covers loading and the title | DESIGN §2.5, §4.4 |
| R9 `unsafe` scope | `en-gal::{reader, sys}`, `en-overlay::hooks` only | Adds the attribute-only `eldennet::exports` module (Rust 2024 `#[unsafe(no_mangle)]`) and the out-of-process `en-launcher::win` | DESIGN R9 |
| R14 and commit timing | "Every write its own transaction" vs "the store flushes every 250 ms" (DESIGN, NPC, MEM) | Each DB request is one transaction, run on arrival. Speech commits are sent in the same step; other writes are coalesced by the core at most every 250 ms | DESIGN R14, §8.5; NPC §8.7; MEM §1.5, §8.6 |
| Readiness and scan order | Readiness polled statics that only a later scan finds | Two-phase readiness: window, small scan for two items, poll, full discovery | DESIGN §3.1, §3.5 |
| Self-tests vs R1 | "Structural self-tests before `register_task`" | Image checks before registration, game-object tests on the first ticks; a failure leaves the task inert | DESIGN §3.1, §4.3, §4.4, decision log |
| Title-screen gate | Gate false at the title, so C4 forbade the wizard's self-test | Title clause: lobby `None`, no anti-cheat, `session` healthy; network for the wizard and silent self-test only | DESIGN §3.4, §11.5, decision log |
| Read groups | `CSFeMan` "ui, boss", `CSNowLoadingHelper` "lifecycle" (no such groups) | Boss bars in `ui`, the `boss_fight_active` cross-check in `player`, lifecycle fields in `world` and `ui`; required items listed | DESIGN §4.1, ARCH §6.1 |
| Flags self-test | "Every catalogue ID's group exists", which would disable named chat without the DLC | Core aliases must exist; more than 10% of non-DLC aliases missing fails the group; missing DLC aliases read Unknown | DESIGN §4.3 |
| Capability matrix | No `camera` effect on conversation; no `text` effect on barks | No camera → no cone targeting; no text → card label fallback | DESIGN §4.4 |
| Flag rate | "1 Hz" in some places, "every 4th snapshot" in others | Every 4th snapshot: 1 Hz, 0.5 Hz while throttled, stated everywhere | DESIGN §0, §4.5, §5.1, §6.7; ARCH §8.10; NPC §3.2 |
| Snapshot schema | DESIGN §5.2 lacked `new_params`, `boss_flag`, `overworld`, and the `Option`s | Aligned with ARCH §7.1; "walk ≤ 48, keep ≤ 32" replaces "hard cap 48" beside `ArrayVec<Chr, 32>` | DESIGN §5.1, §5.2 |
| `BossFightEnded` | Timing of the "left" outcome unstated | Victory and Death at once; Left when the 10 s window closes | DESIGN §6.5 |
| `expect_name_id` | Per persona | Per row (variants carry different name IDs) | DESIGN §7.2 |
| Non-human talkers | DESIGN §7.5 listed Alexander and Boc under the mute list, while O10 makes them personas | They sit in silent families but speak through persona entries; Jar Bairn and Rya stay mute in v1 | DESIGN §7.5 |
| Resting and the Roundtable | "Count as safe out to 8 m" was ambiguous, and contradicted §8.7 | Defined as *sheltered*: waives the hostile-distance checks only, never the boss or combat check; rule 5 reaches 8 m; resting is in practice blocked before S3 | DESIGN §8.1; NPC §5.2, §5.4 (combat check moved out of the waiver) |
| Pad chord | View + D-pad Up leaks the map and a spell switch | Chord masking noted; S4 confirms | DESIGN §8.1 |
| Queue vs cut-off | "Queue during generation" overlapped "cut-off while pages show" | Queue until page 1 shows, cut-off after | DESIGN §8.2 |
| Danger | "The box closes" vs "Engaged interrupts only chat input" | Box closes and input is released; shown pages keep pacing; ends after 10 s | DESIGN §8.1, §8.6 |
| `player_fell` | The 2.5 s global gap could block the key moment | Bypasses the gap, still counts toward the cap | DESIGN §9.2 |
| Seed count | "About 125" | 126 for 6 cards + 15 for the fallback = 141 | DESIGN §9.3 |
| Memory-job output | `{summary, keepsakes[…], …}` | Adds `keep[ids]`, keepsake `id` and `refs`; ≤ 3 new per job; deferred and parked states | DESIGN §10.3 |
| SteamID discovery | "Newest `ER0000.sl2`" (stale under ERSC or a me3 savefile name) | Newest full-size save of any name, preferring one written this session | DESIGN §10.5 |
| Fingerprint wording | "keepsake-if-readable" (clashes with journal keepsakes) | "starting gift" | DESIGN §4.1, §10.5 |
| Self-test verdict words | NPC §7.7 used TTFT thresholds (1.0 s / 2.5 s); ARCH §11.7 used an estimated first page (1.6 s / 3.0 s) | One rule, ARCH's, tied to C5 | DESIGN §11.5, NPC §7.7 |
| Alias naming | ARCH `flag:persona.<id>.<state>` vs NPC and MEM `<persona>.<state>` | `flag:<persona>.<state>` everywhere | ARCH §8.3 |
| Hash maps in the pure core | NPC's `IdentityIndex` used `FxHashMap`, which ARCH §2.4's lint bans | `BTreeMap` | NPC §2.2 |
| Settings keys | NPC used `overlay.scale`, `overlay.hdr`, `barks.surface`, `pools.max_jobs_per_session`, none in ARCH's `settings.toml` | `ui.scale`, `ui.hdr`, `ui.paper_white_nits`, `features.labels`, `pools.max_jobs_per_session` in ARCH §11.5 | ARCH §11.5, NPC §10.4, §10.7 |
| Label anchor height | ARCH "head 1.7 m", NPC "head + 0.3 m" and "label height, default 2.0 m" | One rule: position + `label_height_m` (default 2.0 m) | ARCH §7.4, NPC §10.1 |
| `unknown_chr` table | Named by NPC §2.5, missing from MEM's diag schema | Added (IDs and counts only) | MEM §8.7 |
| Repo layout | Missing `lexicon/{safety,signature,context_words,public}`, `knowledge/aliases.toml`, `regions.toml`, `tests/s6/`, fixture names | Added | DESIGN §15, ARCH §13 |
| Loader alternatives | DESIGN §3.3 had no row for other chain loaders (the owner asked for them) | Row added | DESIGN §3.3 |

### 2.2 Feasibility against the research (task 2)

Re-checked and found supported, with the evidence label the design already gives: version gate fields (ER01 §1, ER02 §5); `register_task` from a non-main thread (ER01 §3.1–§3.2); the FD4 idiom and DLRF naming (ER01 §4); `chr_inses_by_distance`, handles and `player_chr_set` (ER01 §5); flag storage and the bit formula, including false for unallocated groups (ER01 §9); FMG slot IDs and the `MsgRepository` layout leads from 2.6.2.0 (ER01 §8.2–§8.3); `hud_state`, screen state, load counter and banner IDs (ER05 §3, §9); ride state, throw states, SpEffect 9621 as a Roundtable-only signal, flag 9000 covering the whole seated time (ER05 §3, §8.5); hostile team set {6, 7, 27, 29} (ER01 §6, ER05 §6.2); DirectInput and XInput as the game's input paths (ER02 §7.2); `steam_appid.txt` and suspended-process injection (ER02 §2.1, §3.2); model and port defaults (ER06 §5.2, §6).

Fixed or downgraded:
- **K2 relied on me3 behaviour.** The fallback "document a path where players use their own loader's Arxan option" leans on me3's `disable_arxan`, which the owner update forbids. K2 now offers (a) a reduced feature set from items that resolve on an untouched image, or (b) holding v1. §3.5 states that EldenNet must behave the same with or without a loader's Arxan option. K5 (Proton) is reworded as compatibility, not dependency (DESIGN §3.5, §17).
- **Multiplayer gate and NPC summons.** If NPC summons or NPC invaders are `PlayerIns` in `player_chr_set`, the count of 1 would make EldenNet dormant in summon-assisted boss fights. A dormant task reads no characters, so `FoughtBeside` could never fire. The count now excludes NPC `chr_type`s, and S1 must list the values (DESIGN §3.4, §16.1; ARCH §7.1).
- **Text under hooking overhauls.** Reforged serves merged DLC text through a `GetMessage` hook (ER01 §8.3), so a raw table walk can show other names than the game does. Caveat added; S2 confirms the layout leads (DESIGN §4.5).
- **Victory banner.** Banner 22 (a god's defeat, ER05 §3) added to the victory evidence beside 1–4 (DESIGN §6.5, ARCH §8.5).
- **Input hooks.** NPC §5.5 dropped `GetDeviceData` events, which needs a hook ARCH never installs. It is now conditional on S4 showing that the game uses buffered input (ER02 §7.2 says it reads `GetDeviceState`).
- **Steam floating keyboard.** It is a call into `steam_api64.dll`, which R2 did not mention. R2 now names it as the one third-party call (S4-gated, never game code); NPC §5.6 says which thread makes it (DESIGN R2, §8.1).
- **R1 wording.** The init thread does read image sections and static slots before the task exists. That is unavoidable and documented in the research (ER01 §3.1). R1 now states it as the single exception, ending at registration, and says that init never follows a static into a heap object.
- **"Set count catches ERSC partners"** is marked SPECULATION until S0-C.

### 2.3 Scope (task 3)

Nothing was cut outright. Every feature in DESIGN serves a requirement or a stability seam, but two serve experience more than the minimal requirement. The review makes them explicit owner decisions with a cut path:
- **O23 bark pools.** Seeds alone meet requirement 3. Pools add a generation scheduler, a validator and `pools.sqlite`. They stay, gated by X2, and are the first thing to move to v1.1 if v0.4 runs late.
- **O24 floating labels.** These are the only feature that reads game memory every frame (the render feed). The bottom feed needs no per-frame reads. Labels stay, but the feed alone ships if S1 or the soak shows any cost or fault.

Kept on purpose, as stability or seam measures: the event-engine test harness and recordings (the owner asked for them), the layout probe tooling (patch absorption), HDR handling (C1's HDR machine), and the reserved tables and traits (DESIGN §13).

### 2.4 Licence hygiene (task 4)

- **Recordings carried FromSoftware text.** `WorldSnapshot.new_text` holds runtime character, place and gear names, yet ARCH §13 claimed committed recordings "contain no game text". Committed recordings are now written in redacted mode, with `fmg<fmg>:<id>` placeholders resolved from our own fixture name table, and lint rejects anything else (DESIGN §6.8; ARCH §7.5, §11.5, §13).
- **Developer flag labels.** DESIGN §10.4 implied that the catalogue stores FromSoftware's labels. It now says they are never stored in the repo: only our own `note` and a `src` pointer are kept (agreeing with MEM §6.1, §7.6).
- **Pattern bytes.** ARCH §6.3 now says that a lead never supplies bytes. Byte strings that the research quotes from fromsoftware-rs's mapper profile, TGA's table or other projects are not pasted; every pattern comes from our own disassembly in S0-A.
- **Wording.** The death banner's on-screen text in DESIGN §1.4 became "the death banner (banner 5)". MEM's absence phrase was reworded so that no SkyrimNet prompt phrasing is echoed.
- **Scan result:** no SkyrimNet prompt, template or bio text was found. Personas, cards, seeds and KB entries are original. FromSoftware material appears only as names and IDs (Kalé, Godrick Soldier, Limgrave, flag and param numbers). Kalé's KB rumour paraphrases a vanilla topic in our own words, which the content rules allow.

### 2.5 Readability (task 5)

Each document now opens with a short "How to read it" after its status block: DESIGN (new), ARCH (new, beside its existing §0 map), NPC (new) and MEM (new, pointing to its §0 table). DESIGN's header no longer calls the detail documents "planned" and links to this review. NPC §14 and MEM §11 say which of their points the review adopted.

---

## 3. Conflicts reported by the detail writers, resolved

| # | Source | Conflict | Resolution |
|---|---|---|---|
| 1 | ARCH | Wizard vs gate at the title | **Adopted** the title clause (DESIGN §3.4, decision log). The owner may veto it; nothing else depends on it but the wizard's self-test |
| 2 | ARCH | `cargo deny` cannot see NON-AI licence files | **Adopted** the licence-file scan and `[bans]` list (DESIGN §4.7, §14) |
| 3 | ARCH | Self-tests vs R1 | **Adopted**; L1 wording now covers "no task" and "inert task" (DESIGN §3.1, §4.4). C2 unchanged |
| 4 | ARCH | Readiness vs scan order | **Adopted** two-phase readiness (DESIGN §3.1, §3.5) |
| 5 | ARCH | `boss` and `lifecycle` group labels | **Adopted** ARCH's mapping (DESIGN §4.1) |
| 6 | ARCH | Missing crate-graph edges | **Adopted** (DESIGN §2.2) |
| 7 | ARCH | R9 `unsafe` scope | **Adopted** (DESIGN R9) |
| 8 | ARCH | No snapshot return path for params | **Adopted** `new_params`, `boss_flag`, `overworld` (DESIGN §5.2) |
| 9 | ARCH | R14 vs the 250 ms flush | **Adopted** ARCH's rule in DESIGN, NPC and MEM (§2.1 above) |
| 10 | ARCH | Flag rate under throttle | **Adopted** "every 4th snapshot"; 0.5 Hz while throttled is acceptable (DESIGN §6.7) |
| 11 | ARCH | Dormant scope | **Adopted** as an orthogonal flag (DESIGN §2.5) |
| 12 | ARCH | `BossFightEnded` Left timing | **Adopted** (DESIGN §6.5) |
| 13 | ARCH | Camera and cone targeting | **Adopted** (DESIGN §4.4) |
| 14 | ARCH | `GameThread` constructor | **Adopted** (DESIGN §2.3) |
| 15 | NPC | Resting chat vs the pre-S3 rule | **Adopted**, with "sheltered" defined. The combat check is never waived (DESIGN §8.1, NPC §5.4) |
| 16 | NPC | Pad chord leaks | **Adopted** the masking; S4 decides (DESIGN §8.1) |
| 17 | NPC | `expect_name_id` per row | **Adopted** (DESIGN §7.2) |
| 18 | NPC | Queue vs cut-off | **Adopted** (DESIGN §8.2) |
| 19 | NPC | `player_fell` and the 2.5 s gap | **Adopted** (DESIGN §9.2) |
| 20 | NPC | Danger and conversation output | **Adopted** (DESIGN §8.1, §8.6) |
| 21 | NPC | Seed count | **Fixed**: 141 (DESIGN §9.3) |
| 22 | NPC | Kalé's entity lead and the merchant range | **Kept** as UNVERIFIED notes for S1. Not used as keys |
| 23 | NPC | Restrictive stages on unverified aliases mute a persona | **Accepted** as the intended cost of strict mode. The Kalé pilot has no such stage; S6 must verify any restrictive alias before it ships |
| 24 | MEM | Developer labels in the catalogue | **Fixed** in DESIGN §10.4: never stored |
| 25 | MEM | Flags self-test vs players without the DLC | **Adopted** core / 10% / DLC-Unknown (DESIGN §4.3) |
| 26 | MEM | Alias naming | **Fixed** in ARCH §8.3 |
| 27 | MEM | Pool table name | **No change needed**: DESIGN never named the table; NPC §9.5 is authoritative |
| 28 | MEM | Where decisions and intents live | **Accepted**: the playthrough file, so trims and forgets cover them |
| 29 | MEM | Memory-job output shape | **Adopted** (DESIGN §10.3) |
| 30 | MEM | SteamID discovery | **Adopted** (DESIGN §10.5) |
| 31 | MEM | Tool list and new files | **Adopted** (DESIGN §2.2, §15; ARCH §12.3, §13) |

---

## 4. Remaining issues

### 4.1 Blocking unknowns (spikes)

1. **S0-A is the whole project's gate.** If AOB and FD4 scans do not resolve on an image with Arxan untouched, even after the retries, K2's options leave either a much smaller v1 or none. Run it first, on both builds, before any other v0.1 work.
2. **`player_chr_set` composition (S1).** If NPC summons or invaders appear in it, the `chr_type` filter of DESIGN §3.4 needs exact values, or the gate and `FoughtBeside` break as described in §2.2.
3. **The talk observer (S3)** decides how much chat is available at all. Before it passes, chat needs `hud == Default`, 3 s free of `PopupMenu` and no persona greeting in progress. So no chat is possible while seated at a grace, and no bark is possible within 15 m of any persona. Expect testers to find v0.2 chat hard to open near graces.
4. **Input (S4):** camera drift, the pad chord, the Steam keyboard under a `steam_appid.txt` launch, and Proton.

### 4.2 Design gaps to settle before code

5. **Play-time width.** ARCH reads `play_time` as a `u32` of milliseconds, which wraps or caps at about 1,193 hours. Rollback is decided by play time (MEM §9.1). MEM should state what a jump of more than 1,000 hours backwards means (a cap or wrap, not a rollback) once S6 shows how the game behaves.
6. **The `ReadProcessMemory` fallback (S0-D).** If vectored handlers force the copy primitive onto `ReadProcessMemory` (about 1 µs per copy), the 512-copy budget alone reaches about 0.5 ms. The snapshot's 0.4 ms p99 would then need fewer copies per character or the 2 Hz throttle. Measure in S0-D.
7. **hudhook's MinHook.** R12 assumes that the DInput and XInput detours can go through the MinHook copy that hudhook bundles. Confirm in the first v0.1 build that hudhook 0.9.3 exposes it. If it does not, the rule "one MinHook in the process" needs another route before any second hook library is added.
8. **Kalé's data.** His flag block is unknown (`base = 0`), and `kale.howl_told` has `id = 0`. The pilot therefore runs with its base stage only until S6 finds them. That is fine for v0.2, but the eval's 2–3 stages per persona cannot be met for Kalé until then.
9. **Clean-room discipline for patterns** (ARCH §6.3) is a process rule that CI cannot fully check, because the provenance field is self-declared. The owner may want a short derivation log per pattern, reviewed with each `gal/` change.

### 4.3 For the owner

10. **New decisions:** O23 (bark pools) and O24 (floating labels), both with a cut path. **Settled:** O3 presents the three options; the owner then decided (c), own GAL (2026-11-06), and DESIGN §19 records it as decided.
11. **Adopted by the review, open to veto:** the title-screen gate clause (§3 item 1) and the sheltered-context definition (§3 item 15).
12. **Proton** has no supported launch path of our own until S0-C passes. Until then it works only through a loader the player already uses.

### 4.4 Not done here

- No commit was made. Before anything is pushed, run the privacy and safety sweep (`tools/privsweep`).
- The process records (P1–P3, J1–J3) were not edited. Where they disagree with DESIGN.md, DESIGN wins, as its header says.
