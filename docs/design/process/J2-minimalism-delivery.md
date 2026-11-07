# J2: judge scorecard, minimalism, scope discipline and delivery

> **What this is:** one judge's scorecard of the three v1 proposals
> ([P1 lean monolith](P1-lean-monolith.md), [P2 split brain](P2-split-brain.md),
> [P3 experience first](P3-experience-first.md)), read in full against the research summaries
> (`SN00`, `ER00`) and the detail notes they cite. **Date:** 2026-11-06.
>
> **Lens:** does the proposal deliver exactly the user's v1 with the least code and content? Could a solo
> developer or a tiny team ship it in small increments, behind a sensible spike-first plan? Are the
> extension seams real but cheap? Is anything over-engineered or under-specified? Is install and setup
> friction minimal, and is the testing story realistic?
>
> **The owner update of 2026-11-06 applies throughout:** no me3 dependency, our own Game Access Layer
> instead of fromsoftware-rs, and a first-class event engine. All three proposals were written before it.
> §2 lists where each one assumed me3 or fromsoftware-rs. Those points are scored as "needs replacement",
> not as design errors, because every proposal followed the research's recommended stack (ER00 §3.2).
>
> Citations: `P1 §x` = proposal section; `ER0n §x` / `SN0n §x` = research notes. "Judge view" marks a
> recommendation of this scorecard, not a research fact.

---

## 1. Sub-criteria (each scored 1–10)

| Code | Sub-criterion | What earns a high score |
|---|---|---|
| **A** | **Scope fit** | Delivers the four user requirements exactly. Important NPCs get personality, awareness, memory and free chat. Generics get self, place and combat awareness plus short lines. Dialogue only. Nothing large is added, and nothing required is missing |
| **B** | **Code and content economy** | The fewest processes, crates, threads, protocols, tables and authored lines that reach v1. No machinery whose payoff comes only after v1. Under-specified parts also lose points, because they turn into rework |
| **C** | **Delivery plan** | Spikes come before code that depends on them. Each increment can ship to testers and is sized for one person. The core user value (named-NPC chat) arrives early. Spike and milestone dependencies agree with each other |
| **D** | **Seams: real but cheap** | Interfaces that cost little now and let later features (actions, TTS/STT, vector memory, native subtitles, another backend, a sidecar) plug in without reshaping. This includes the swappable game-access layer and an event-engine foundation that the owner now requires |
| **E** | **Setup and testing realism** | One-step install, a safe first run, little that antivirus or Proton can break. A headless, replayable test story that a tiny team can run in CI. Success criteria a tiny team can actually measure |

---

## 2. What the owner update overturns in each proposal

All three proposals rely on me3 for the three things it normally provides: offline enforcement
(`start_online = false`), Arxan neutralisation (`disable_arxan = true`) and save separation
(`savefile`). All three also default to vendoring the pre-licence-change fromsoftware-rs subset (option b).
None of them specifies a launcher of its own, and none names a first-class event engine.

| Item | P1 lean monolith | P2 split brain | P3 experience first |
|---|---|---|---|
| me3 as loader | §0 "one DLL plus one me3 profile"; §2.6 step list "after me3 loads the native post-main"; §8.4 profile; G-install | §0 "loaded by me3"; §12.1 shipped zip and profile; G6 | §2.1 `[[natives]]` profile; G7 |
| `disable_arxan = true` | §8.4 profile; S0 tests "me3 load with `disable_arxan`" | §12.1 profile, commented "v1 hooks no game code", which argues *against* needing it | §2.1 |
| Separate save via me3 | §8.4 `savefile` | §12.1 `savefile` | §2.1 "its own `savefile`" |
| Offline enforcement | Own session gate (§2.6 `session` group, D16) plus me3 `start_online` | Own `Suspended{multiplayer}` gate (§2.6) plus me3 | Own multiplayer rule (§2.5) plus me3 |
| fromsoftware-rs | U1 default (b): vendor from `acc9da0` into `en-er-sys` behind `SysLayer`; option (c) costed at "about 600 lines and a few weeks" (§2.5) | D1 default (b): `FrsAccess` with `vendor/fromsoftware-rs/`; `OwnAccess` named as option (c) (§2.4) | D1 default (b): vendored plumbing behind `GameBackend`; (c) rated "High cost" (§2.8) |
| Game-code hooks | None (R12) | None (R6) | None by default. **But** §5.2 keeps an optional "menu has the mouse" hook on game code if S4 shows camera drift. It must go: it is game-code patching, which needs Arxan handling (ER02 §5, §11) |
| Event engine | Closest foundation: a pure `Core::step(input, now)` (§2.4), an exact diff table (§3.4), one audience rule (§3.5) and a scenario DSL with assertions (§11.2). Detection conditions live in code, not in data | A diff table with P/K/T store classes (§3.3), a scenario DSL and `*.expect.yaml` (§11.1) | A diff table with an audience column (§3.6). **Golden event files per recorded session, so CI fails on any event change** (§11.1) |
| Own launcher | None | None (only an "external" launch mode for the brain) | None |

**Judge view on absorbing the update with the least code:**

1. **Game Access Layer.** P1 §2.5 has the best inventory to start from: register one task, find about 10
   singletons, read about 60 fields, walk one vector, one flag tree and one FMG table. Its `SysLayer`
   trait is narrow enough to be the GAL boundary as it stands. P1's own cost estimate for option (c), about
   600 lines plus a few weeks, is the figure to present to the owner. One decision for the owner: may the
   AOB *pattern bytes* be taken from the MIT-era `mapper-profile.toml` as documented facts, or must they be
   re-derived (TGA's table, EROverlay-style signatures, our own probing)?
2. **No Arxan neutralisation** adds a blocking question to spike S0 that no proposal asks: do the runtime AOB
   scans (`register_task` call site, `GameDataMan`, `GameMan`, `FieldArea`) and the FD4Singleton finder
   resolve on an image that dearxan has not touched? ER02 §5 warns that "Arxan-encrypted functions may be
   unreadable until Arxan runs or is neutered". The DLL's init thread waits for game init, so Arxan's entry
   stubs will have run, but the spike must show it on 2.7.2.0 and 2.7.2.1 (ER01 §3.2, ER02 §5).
3. **Launcher, minimal form.** Run `eldenring.exe` directly with the `steam_appid.txt` / `SteamAppId`
   method that ER02 §2.1 recommends, never `start_protected_game.exe`. Inject the DLL with one
   `LoadLibraryW` remote thread, and let the DLL's init thread tolerate early or late load. The DLL itself
   refuses to activate if an EasyAntiCheat module is present or the session gate fails, so offline safety
   holds whoever loaded it. Do **not** ship a `dinput8`/`winhttp` proxy in v1. It clashes with Elden Mod
   Loader and other proxies, needs `WINEDLLOVERRIDES` under Proton, and Seamless Co-op cannot be loaded
   through such injectors (ER02 §3.3, §2.3). ER02 §2.1 does note one useful property of a proxy: EAC refuses
   to start next to it. That is worth a v1.x note.
4. **Save separation (owner decision).** v1 writes nothing to game memory, flags or the `.sl2`, so the save
   stays vanilla (ER02 §2.2 still advises a separate save). The cheapest stable default is: no redirection
   in v1, and the launcher keeps rotating backups of `ER0000.sl2`. The alternative is an opt-in
   `CreateFileW` path rewrite to `ER0000.eldennet.sl2`. That is a system-DLL hook, so it is allowed, and it
   costs some code plus a one-time save copy for the player.
5. **Event engine, minimal form.** Make it a named component **inside the pure core**, not a threaded bus
   framework. Sources (snapshot differ, flag watcher, and later hooks) implement one `EventSource`
   interface. Detection rules are TOML records with debounce, hysteresis, cooldown, merge key and TTL,
   extending P1's `triggers.toml` shape (§6.1). Subscribers (conversation, barks, memory) are called in a
   fixed order within `step()`. Backpressure means drop-oldest for short-lived and transient events, and
   never drop persistent ones. The harness is P1's `en-replay` plus P3's golden event files.

---

## 3. Scores

| Proposal | A Scope fit | B Economy | C Delivery | D Seams | E Setup & tests | **Total /50** |
|---|---|---|---|---|---|---|
| **P1 lean monolith** | 7 | 6 | 7 | 8 | 8 | **36** |
| **P2 split brain** | 7 | 4 | 5 | 7 | 5 | **28** |
| **P3 experience first** | 6 | 5 | 8 | 7 | 7 | **33** |

**Winner for this lens: P1, the lean monolith.** It has the fewest moving parts for the guarantees it
makes, the strongest crash-safety discipline (an SEH-guarded reader, read groups with breakers, `ffi_guard`
enforced in CI), the most testable core (a pure `step()`), and the clearest inventory for the owner's own
Game Access Layer. P3 has the better delivery order and the leaner memory and spoiler design, and the
final design should take those from it (§5). P2's process split is the largest source of v1 code, and its
benefit arrives mostly after v1.

---

## 4. Rationale per proposal

### 4.1 P1 lean monolith (36/50)

**A. Scope fit: 7.** Everything the user asked for is present, with modest gold-plating: ambient barks
(§6.1 `idle_ambient`), optional live barks (U7), and a **multi-turn** RAM-only chat with generic NPCs
(§6.5). That goes past "simple things". ER04 §8 item 4 even says "Generic NPCs offer no conversation in
v1". Folding named service NPCs into the persona pipeline (§4.1 tier B) is sensible and adds no machinery.

**B. Economy: 6.** One process and eight crates. The core is game-neutral without being abstract for its own
sake. Two things overreach. First, memory has five parts: event log, facts, versioned summary, up to 3
FTS5 notes per conversation, and disposition. Notes come with a tuned ranking formula
(`0.5·bm25 + 0.3·importance + 0.2·exp(−Δ/τ)`, §7.4). ER00 §3.3 and ER06 §5.4 say summaries plus facts are
enough for v1, and P3 shows that a whole-journal memory fits the budget with no retrieval at all. Second,
several things could wait: `try_previous_layout` probation mode (§2.6, U2), `DecisionRequest` used in v1
only to break target ties (§10), `en-tools draft`, and 16 owner decisions. Content stays light (8
personas, 12 cards with no seed lines), but barks then depend on an LLM pre-bake (36 calls on first run,
§6.2).

**C. Delivery: 7.** The spike table has pass criteria (§12.2). v0.1 "Eyes" ships perception, recording
and replay with **no AI**, the right first increment. But barks (v0.2) come before conversation (v0.3), so
the core user value waits behind pools, floating labels and combat-heuristic tuning. The plan is also
internally inconsistent: S7, the `AggroStart` accuracy spike, "blocks v0.4/v0.5", yet barks driven by
`AggroStart` ship in v0.2 (§12.2 vs §12.3). C1 asks for ≥ 50 h across ≥ 5 machines, which is a lot for a
solo developer.

**D. Seams: 8.** The `SysLayer` trait takes three backends behind a cargo feature (§2.5). The pure
functional core with time as an input is the cheapest possible base for replay and for the event engine.
`trait CoreTransport` (§10) leaves P2's sidecar open as a later option at zero v1 cost. The seam table is
concrete and every row is cheap. The reserved `decisions` table and `GameCommand::Action` behind a feature
flag are fine.

**E. Setup and testing: 8.** The in-game wizard probes endpoints and self-tests (§8.6). The strongest test
story of the three: core tests on Linux, a synthetic scenario DSL, a mock SSE server, `insta` prompt
goldens with a byte-stability assertion, DB migration and trim tests, `cargo deny` with a licence
allowlist, cross-compiled CI, and a patch procedure that produces data-only layout updates (§11). The only
install weakness is the me3 dependency, which every proposal shares.

### 4.2 P2 split brain (28/50)

**A. Scope fit: 7.** The cleanest scope of the three: no ambient barks, live barks off, a one-line generic
reply, 8 personas (§1.2, §6). One extra: the identity table covers **all** known named NPCs so that they
can be silenced and named (§4.2). That is curation work the user did not ask for. A default-silent rule
(P3 §4.6) achieves the same result with no content.

**B. Economy: 4.** The split adds a whole subsystem that v1 does not need: a protocol crate with framing,
handshake, versioning, an auth token, resync and bounded queues; a supervisor with job objects, heartbeat
and backoff; two IPC threads; `bridge-sim`; protocol tests; spike S0b; and two signed binaries (§2.3, §2.7,
§11.4). The research recommends the opposite for v1: one in-process DLL with a serializable boundary, and
a sidecar later "if crash isolation becomes worth it" (ER06 §5.1 item 3; ER00 §5 item 1; ER00 §3.3). P2
admits that the split "does not fix" the main crash class, because game reads stay in the bridge (§2.8).
Its memory design, on the other hand, is the most economical: summary + facts + claims, loaded whole,
no ranking (§7.2–§7.3), exactly ER06 §5.4.

**C. Delivery: 5.** v0.1 is pure plumbing (bridge, supervisor, pipe, handshake, heartbeat) with no
perception and no player-visible value. Chat arrives only at v0.4, after barks. The plan is honest that
S0b decides "the angle itself" and falls back to an in-process build (§12.2). But that means the first
weeks risk building a protocol the project may throw away.

**D. Seams: 7.** The `&GameThread` zero-sized token (§2.1) is the cheapest, strongest enforcement of
"game memory only on the game thread" in any proposal. The P/K/T store classes (§3.3) and the `UiAck`
commit protocol are clean. The split itself is a seam, but an expensive one, and P1's `CoreTransport`
keeps the same option open for free.

**E. Setup and testing: 5.** A DLL that spawns an unsigned exe is exactly what antivirus and SmartScreen
flag. P2 rates this "High support risk" itself (§2.8), and ER02 §3.2 reports that me3 is often
quarantined. Proton process spawning and pipes are unknown (S0b). Testing is good (`bridge-sim`, replay,
scenarios, protocol and brain-kill drills), but covers more surface. The real development win, restarting
the brain without restarting the game (ER06 §5.1), can be had more cheaply with hot-reloaded content and
the headless replay harness.

### 4.3 P3 experience first (33/50)

**A. Scope fit: 6.** Four experiential "scenes" as the spec (§1.1) are an excellent acceptance test for
the user's requirement 2. But P3 adds the most beyond v1:
- a **taunt key**, which is a new input verb;
- a red-edged call-out box for generics;
- off-screen edge chevrons;
- "arming" chat while a vanilla menu is open;
- a shared nomad-merchant persona keyed per entity;
- a conditional Ranni;
- 12 + 1 personas and about 12 cards;
- register presets, opener lint and tic counters;
- a strictness setting;
- an archaic-share release gate.

Each piece is small. Together they are a v1.x feature list.

**B. Economy: 5.** One process (good) but eleven crates. The memory design is the leanest *with* good
recall. A versioned journal of ≤ 120 words plus ≤ 8 keepsakes with a code-validated `basis` (said or
seen) is sent whole: "no retrieval at all" (§7.2). Seed lines remove the network from the bark path
entirely. Content, though, is the heaviest of the three: 12 cards × about 11 triggers × 3–6 seeds is
about 400–800 authored lines; about 400 pool keys need a pre-bake; and personas carry about 18 capped
fields each (§4.3, §4.5, §6.3).

**C. Delivery: 8.** The best order. v0.1 is the gate, snapshot and recording. v0.2 is events with golden
tests. v0.3 is **one persona, no memory**, so the user's core value comes before barks (v0.5). Two
offline spikes stand out: X1 "voice in a bottle" in week 1 tests the biggest product risk (tone) before
any chat code is written, and X2 is a bark taste test (§12.2). The wizard comes late (v0.7), which is
fine for internal testers.

**D. Seams: 7.** `GameBackend` with an `apply()` that only accepts `Noop`; a `Recall` trait; a
`SpeechSink` whose `page()` returns `PageTiming`, so TTS later swaps the pacing source without touching
the pacing code (§9.4); a reserved `NativeLineHeard` event. All are real and cheap. The core is less
strictly pure than P1's, so replay determinism needs more care.

**E. Setup and testing: 7.** The first run shows the ladder level and capabilities before asking for a
key (§8.6), and seeds mean "Prepare barks" is truly optional. Golden event files over recorded routes
are exactly the owner's harness requirement. The offline eval that gates each persona is automated and
cheap per run. Weak points:
- the snapshot cadence counts frames ("every 15th frame", §2.1), so at 30 FPS it halves while the
  debounce windows stay in seconds;
- no SEH guard on reads (§2.7 R1 checks only null, alignment and committed pages);
- the optional game-code hook (§5.2) breaks the no-patching rule.

---

## 5. Ideas the final design should graft in

Each graft names its source and where it lands. They are ordered roughly by value per line of code.

| # | Graft | From | Where it lands |
|---|---|---|---|
| G1 | **Pure functional core:** `Core::step(input, now) -> Outputs`, with time as an input and effects as outputs. This is what makes debounce, cooldown, expiry and the scheduler replayable bit for bit | P1 §2.4 | Event engine core and replay harness (owner item 3f) |
| G2 | **SEH-guarded copies:** `GuardedReader::copy::<T: Pod>`, batched per object (one `ChrIns` prefix copy, not one per field), plus plausibility checks (canonical address, vtable in `.rdata`). `catch_unwind` cannot catch an access violation, and this can | P1 §2.8 R4 | Game Access Layer |
| G3 | **Read groups, a capability matrix and breakers:** each field read belongs to a group with its own self-test. Features declare the groups they need and fail closed. 3 faults in 60 s trip a group for the session | P1 §2.6, §2.8 R9 | GAL degradation ladder |
| G4 | **GAL inventory and trait:** the "one task, ~10 singletons, ~60 fields, one vector, one flag tree, one FMG table" inventory, behind a narrow `SysLayer`-style trait. Present P1's option-(c) estimate (about 600 lines, a few weeks) to the owner | P1 §2.5 | Owner decision; GAL |
| G5 | **`&GameThread` token:** a zero-sized type that only the task trampoline can build. Every GAL read takes it, so off-thread reads cannot compile | P2 §2.1 | GAL API |
| G6 | **`ffi_guard` on every foreign entry**, with a CI grep check. `unsafe` is confined to the reader, the sys layer and the input hooks, each block with a `// SAFETY:` note. Clippy deny-list in those crates | P1 §2.8 R3, R10 | Whole DLL |
| G7 | **Event retention classes:** P (persistent, with audience), K (short-lived, keyed, TTL), T (transient trigger only). One visibility rule serves every consumer | P2 §3.3; P1 §3.5; P3 §3.6 | Typed event model |
| G8 | **Golden event files** per recorded or scripted session. CI fails on any event diff unless the golden file is updated on purpose | P3 §11.1 | Event-engine test harness |
| G9 | **Journal memory with no retrieval:** a first-person summary of ≤ 120 words plus ≤ 8 keepsakes. Each keepsake's `basis` (said or seen) is validated by code and rendered "The Tarnished told you…" / "You saw…". It is sent whole. Add P2's game-fed facts and claims. Drop P1's FTS notes and ranking weights, but keep the reserved tables | P3 §7.2–§7.3; P2 §7.2 | Memory |
| G10 | **Stage `gist` and `forbid`:** the author's own paraphrase of what has passed in vanilla up to each stage, plus a per-stage "not yours to know" list with a deflection instruction. Cheap; it keeps free chat consistent with scripted talk that v1 cannot capture | P3 §4.3, §5.4 block 7 | Persona schema; spoiler control |
| G11 | **Unverified flags gate only in the restrictive direction:** content that depends on an unverified flag stays hidden | P1 §7.5; P2 §7.4; P3 §7.6 | Flag catalogue |
| G12 | **Seed lines as a zero-network floor for barks,** capped for v1 at 3 lines × about 6 triggers × about 6 cards (about 100 original lines). LLM pools extend them in the background. Barks then work offline and on first run with no pre-bake | P3 §4.5, §6.3 (reduced) | Barks |
| G13 | **Bark trigger set without ambient** for v1: aggro, struck hard, low HP, ally fell, player down, player fled, player healed. Live barks off. One expiry rule: 2.5 s from trigger to display | P2 §6.1, §6.3; P1 §6.1 | Bark rules (data) |
| G14 | **Multiplayer gate that catches Seamless Co-op at any distance:** `multiplay_role == Host && is_my_world && lobby == None && player_chr_set.len() == 1`, re-checked every snapshot whenever `ersc.dll` is loaded | P3 §2.5 | Session gate (owner item 1) |
| G15 | **"Spoiler safety outranks availability":** if the flags read group fails, named chat turns off and barks keep running | P3 §2.5 L2 | Degradation ladder |
| G16 | **Default silent for anything unresolved,** instead of curating every named NPC. Uncurated named friendlies get "They have nothing to say to you." | P3 §4.1, §4.6 | Identity |
| G17 | **Commit on display,** through the `SpeechSink` callback, plus a player-input epoch that cancels queued barks | P1 §5.6, §6.3; P2 §5.6; P3 §5.8 | Conversation and scheduler |
| G18 | **Milestone order:** conversation with one pilot persona and no memory comes before barks. Week-1 offline spike X1 tests whether candidate models hold the registers and length caps with our prompt layout | P3 §12.2–§12.3 | Plan |
| G19 | **Identity-pack check:** if more than 20% of persona rows' `NpcParam.nameId` mismatch, disable personas with a toast and keep barks running. About 30 lines; it protects drop-in coexistence with overhauls and randomizers | P1 §4.2 | Identity |
| G20 | **Patch procedure with an invariant-search probe** (RTTI vtables, `hp ≤ max_hp` pairs, ascending distances, the known UTF-16 name) that writes a draft layout file. Data-only updates; keep the previous build's profile | P1 §11.5; P2 §11.6; P3 §11.4 | GAL maintenance |
| G21 | **Time-based cadence:** snapshot on accumulated `delta_time` or QPC, never on a frame count | P1 §3.1; P2 §3.2 | Perception |
| G22 | **`SpeechSink::page()` returns `PageTiming`,** so TTS later replaces reading-time pacing without code changes | P3 §9.4 | TTS seam |
| G23 | **Transport-agnostic core** (`CoreTransport`, in-process channels in v1), so P2's sidecar remains a v1.x option at zero v1 cost | P1 §10; P2 §10 | Seams |

**Size the v1.0 content to a solo developer (judge view):** one pilot persona at the chat milestone, then
4–6 personas at v1.0. Pick them from the NPCs whose stages are labelled in both flag datasets (ER04 §8
item 2), for example Varré, Kalé, Patches, Blaidd, Alexander and Boc. Ship 6 archetype cards. This trims
P1's 8 + 12 and P3's 12 + 1 + 12. Personas are gated by spike S6 anyway, and every extra persona is
another flag block to verify in game.

**Defer from v1 (judge view):**
- **From P1:** FTS notes and their ranking weights; `try_previous_layout` probation mode; ambient barks;
  multi-turn generic chat; using `DecisionRequest` in v1 (keep the type, reserved).
- **From P2:** the process split, the protocol, the supervisor and `bridge-sim`; the all-NPC identity
  table; editing memory in the F10 panel.
- **From P3:** the taunt key; the call-out box; edge chevrons; arming chat over a vanilla menu; the
  nomad-merchant shared persona; Ranni; the archaic-share release gate; the spoiler strictness setting;
  the 400-key full pre-bake.
- **Generic replies:** a one-line reply to a player who addresses a generic (P2 §6.6) is the most this
  lens would allow. It reuses the chat box and the `bark` slot. Put it in the last pre-release milestone,
  because barks alone already satisfy "respond with simple things".
- **Floating labels:** build the bottom-left bark feed first. It needs no camera read group and no
  per-frame anchors. All three proposals already have it as their fallback. Add floating labels within the
  same milestone once the `camera` self-test passes.

**A merged milestone skeleton (judge view):**

| Step | Contents | Exit |
|---|---|---|
| M0 Spikes | S0 without dearxan (AOBs, singletons, task registration, forced-fault runs) through the EldenNet launcher **and** as an me3 native; S1 perception; S4 input; S3 talk observer; S6 flags for the pilot persona; X1 offline voice test | Written pass/fail for each |
| M1 Eyes | GAL, version gate, ladder, one task, snapshot, session gate, launcher v0, recording, event engine with golden files, debug overlay. No LLM | Golden events over 3 routes; 0 crashes in 10 h |
| M2 First words | Chat box, input blocking, safe gating, one pilot persona, prompt, sanitizer, paged subtitles, commit on display. RAM only | Time to first text and output-hygiene targets met on the pilot |
| M3 Remembrance | SQLite, event log with audience, facts, journal, claims, rollback trim, NG+ partition | Trim test green; memory probes pass |
| M4 Camp voices | 6 cards with seeds, pools, scheduler, feed, then labels | Bark timeliness and legality on recordings |
| M5 Veil | Verified flag catalogue, stages with `gist`/`forbid`, knowledge base, spoiler lexicon, 4–6 personas | Spoiler eval |
| M6 Release | Wizard, compatibility matrix (Seamless Co-op, Reforged, Convergence, Proton, HDR), patch drill, soak | All criteria |

---

## 6. Fatal flaws and wrong or unsupported claims

**Severity:** **F** is fatal as written: it violates a hard constraint or an owner directive and must be
replaced. **M** is a material flaw. **m** is minor.

| # | Sev | Proposal | Flaw or claim | Evidence / correction |
|---|---|---|---|---|
| 1 | F | P1, P2, P3 | Each design rests on me3: loader, `start_online`, `savefile` and `disable_arxan`. None specifies a launcher of its own | Owner update 1. Replace as in §2: a loader-agnostic DLL, an EldenNet launcher using the `steam_appid` method, our own offline and EAC checks, and a save decision (ER02 §2.1–§2.2, §3.3) |
| 2 | F | P1, P2, P3 | Vendored fromsoftware-rs (option b) is the default | Owner update 2: own GAL by default. P1 §2.5's inventory and `SysLayer` are the best starting point |
| 3 | F | P1, P2, P3 | `disable_arxan = true`. No spike checks that AOB and singleton scans work on an image dearxan has not touched | ER02 §5: "Arxan-encrypted functions may be unreadable until Arxan runs or is neutered". Add this to S0 as a blocking question |
| 4 | F | P3 | An optional game-code hook ("menu has the mouse", §5.2) if the camera drifts | Owner update 1: no game-code patching in v1. Without dearxan, inline hooks "can be silently reverted or crash the game" (ER02 §11). If S4 fails, accept the drift or close the box; do not hook |
| 5 | M | P2 | The process split as the v1 default | Against the research's v1 recommendation (ER06 §5.1 items 1 and 3; ER00 §5 item 1, §3.3). By P2's own account the antivirus risk is "High support risk" (§2.8), and the split does not isolate game reads, the main crash class (§2.8) |
| 6 | M | P2, P3 | Read safety relies on `catch_unwind` (P2 R3) or on null, alignment and committed-page checks (P3 R1) | A panic guard does not catch an access violation. A stale-but-plausible layout after a patch can fault inside the task and kill the game; ER00 §4 item 1 counts silent misreads among the worst outcomes. Use P1's SEH-guarded copies (G2) |
| 7 | M | P2 | Seamless Co-op detection looks only for an extra `PlayerIns` "within 40 m" (§2.6) | ERSC runs its own P2P lobby (ER02 §2.3), and ER05 Implications 7 says those sessions "must also be detected and refused". A partner farther than 40 m would be missed. Count the whole `player_chr_set` (G14) |
| 8 | M | P1 | Spike S7 (`AggroStart` accuracy) "blocks v0.4/v0.5", yet barks driven by `AggroStart` ship in v0.2 | §12.2 vs §12.3 contradict each other. Either move S7 before barks or treat the heuristic as provisional in v0.2 |
| 9 | M | P1 | Memory adds FTS5 notes with a hand-tuned ranking formula | ER00 §3.3 and ER06 §5.4: summaries plus facts are enough in v1. This adds a retrieval path with weights to tune per model and language (SN00 §6 recall rows). Use G9 |
| 10 | M | P1, P3 | Conversations with generic NPCs: P1 multi-turn (§6.5); P3 a call-out box plus a taunt key (§6.7) | ER04 §8 item 4: "Generic NPCs offer no conversation in v1". The user asked for "simple things". At most a one-line reply, late (§5) |
| 11 | M | P3 | Scope and content volume: 12 + 1 personas, 12 cards, about 400–800 seed lines, about 400 pool keys | Not sized for a solo developer. Each persona needs its flag block verified in S6 (ER04 §4.5: the bits were "not observed at runtime"). Trim as in §5 |
| 12 | m | P3 | Snapshot "every 15th frame (≈ 4 Hz at 60 FPS)" | The cadence halves at 30 FPS while debounce windows are in seconds. ER05 Implications 1 says "~15 frames", but P1 and P2 time the snapshot by clock, which is more robust (G21) |
| 13 | m | P3 | The flag catalogue example marks 9000 `verified = true` (§3.3) | No flag has been live-verified yet. ER05 Implications 9 and ER04 §4.5 require a live log before setting the bit |
| 14 | m | P3 | Varré's persona ships the tic `"maidenless"` (§4.3) | A coinage owned by FromSoftware and Varré's signature word (ER04: 14 of its 15 uses). One word is small, but the "owned tic" mechanism invites shipping vanilla catchphrases. Describe the tic instead, or source it from the player's local export (hard constraint: no FromSoftware text in the repo) |
| 15 | m | P2 | The me3 profile sets `disable_arxan = true` "as ER00 §3.2 recommends; v1 hooks no game code" | The comment argues against itself. ER02 §5 says dearxan is needed only "when inline hooks are unavoidable" |
| 16 | m | P1 | C1 asks for 0 crashes over ≥ 50 h on ≥ 5 machines | Unrealistic for a solo developer before release. P3's 30 h on 3 machines, one of them under Proton, is a better v1.0 bar; widen it in beta |
| 17 | m | P2 | The identity table covers "**all** known named NPCs" (§4.2) | Curation beyond v1. Default-silent (G16) gives the same safety for free |
| 18 | m | P1, P2, P3 | None names a first-class event engine with declarative detection rules, confidence, or explicit bus ordering and backpressure | Owner update 3. The building blocks exist (G1, G7, G8). The minimal form is in §2 item 5. "Confidence" can be a per-rule constant (for example 0.6 for the `AggroStart` heuristic, 1.0 for a flag edge) that consumers threshold. No proposal has it |

---

## 7. Bottom line

For minimalism and delivery, **P1 is the base to build on**, with three changes:
- **P3's delivery order and memory:** conversation first, the X1 tone spike in week 1, the whole-journal
  memory with keepsakes, and stage `gist`/`forbid`.
- **P2's cheap safety tricks:** the `&GameThread` token and the P/K/T event classes.
- **The owner update:** the GAL replaces the vendored fromsoftware-rs subset; an EldenNet launcher
  replaces the me3 profile; S0 runs without dearxan; and the event engine becomes a named component
  inside P1's pure core.

Size the content to 4–6 personas and 6 cards, and defer every interaction verb beyond "type to a named NPC".
