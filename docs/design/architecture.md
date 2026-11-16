# EldenNet v1: code architecture

> **Status:** detail document under [DESIGN.md](DESIGN.md), 2026-11-06, re-based on C++ on 2026-11-16 (O60, O84;
> roadmap S8.1.e). It expands DESIGN §2–§6, §11, §14 and §15 into implementation detail. **DESIGN.md is
> authoritative.** Where this document seems to disagree with it, DESIGN wins, and the gap is a bug in this document.
> **Target:** ELDEN RING PC/Steam, App 1.17.2, exe 2.7.2.0 (WW) and 2.7.2.1 (JP), offline only.
> **Citations:** `DESIGN §x` points into the master design. `ER0n §x` and `SN0n §x` are research notes, and `P1 §x` and `J1 #n` point into [process/](process/). LIKELY, SPECULATION and RE-needed keep the research's evidence labels. **(impl)** marks a number or choice this document adds. DESIGN does not fix it, and it can change without a design review.
> **How to read it:** §0 maps each section to the DESIGN section it expands. Read §1–§3 for how EldenNet starts and stays safe, §6–§7 for the Game Access Layer and the snapshot, §8 for Augur, and §10 for the crash-safety rules and how CI enforces them. The review's changes are in [process/review.md](process/review.md).
> **Owner rules applied throughout:** no me3 code, crates, APIs or reliance on its behaviour. No fromsoftware-rs code or data, at any commit. Facts about game structures from the research are *leads*, and each one is confirmed in game or by a self-test before code relies on it (DESIGN §4.2, O4). No game writes, no game-code hooks, and no game-function calls after `register_task`, except the init-time singleton-name getter fallback (DESIGN R2, §4.2).

---

## 0. Reading guide

| § | Covers | Expands |
|---|---|---|
| 1 | Processes, threads, channels, startup, shutdown, single instance | DESIGN §2.1, R6, R14, R16 |
| 2 | The 11 modules, their APIs, dependency rules, lints, build options, build profile | DESIGN §2.2–§2.4 |
| 3 | The DLL load contract and the init state machine | DESIGN §3.1, §3.5 |
| 4 | The launcher, the alternatives, and the me3-native page | DESIGN §3.2, §3.3, §3.6 |
| 5 | The offline and multiplayer gate | DESIGN §3.4 |
| 6 | The Game Access Layer | DESIGN §4 |
| 7 | `WorldSnapshot`, cadence, render feed, recordings | DESIGN §5.1–§5.2 |
| 8 | Augur, the event engine | DESIGN §5.3–§5.4, §6 |
| 9 | The lifecycle state machine | DESIGN §2.5 |
| 10 | Crash-safety rules and their enforcement | DESIGN §2.6–§2.7 |
| 11 | The LLM client, configuration and decision calls | DESIGN §11 |
| 12 | Testing, CI and tooling | DESIGN §14, §4.6 |
| 13 | Repo layout | DESIGN §15 |

**Clocks and units.**
- `mono` is milliseconds (or microseconds where marked `_us`) from `QueryPerformanceCounter`. It is the only wall-independent clock.
- `play` is the game's play time in milliseconds (`GameDataMan`), and it is the rollback clock (DESIGN §10.5).
- Distances are metres in Havok space. Angles are degrees unless marked `rad`.
- Offsets are hex. "Copy size" is the number of bytes one guarded copy reads for one object.

---

## 1. Processes and threads

### 1.1 Processes

| Process | Lifetime | Contains |
|---|---|---|
| `eldennet-launcher.exe` | A launch, then, since O71, the whole session in the background as the crash watcher; it exits when the game does (§4) | Discovery, pre-flight, backups, `steam_appid.txt`, injection; plugins (§4.12); the crash watcher ([proposal 0001](../proposals/0001-plugins.md) §13.7) |
| `eldenring.exe` | The game session | `eldennet.dll`: GAL, core, store, net, overlay. All ML is reached over HTTP (DESIGN §2.1) |

No helper process is spawned from inside the game. The out-of-process core stays a seam (`CoreTransport`, §2.2, DESIGN §13).

### 1.2 Thread inventory

| # | Thread | Created by | Owner | Runs | Game memory | May block | Allocation | On an exception |
|---|---|---|---|---|---|---|---|---|
| T1 | **Init** (transient) | `DllMain` → `CreateThread` | `eldennet`'s `boot`, `en-gal`'s `sys` | Gate, safety, settings, connect to GraceUI, readiness, discovery, `register_task`, worker spawn (§3) | Image only (PE headers, `.text`/`.rdata`/`.data` scans, static slots). The name getter fallback is the only call into game code | Yes (polls with `Sleep(100)`) | Free | `ffi_guard` → L1 banner through GraceUI's `notice` if connected, otherwise a silent exit |
| T2 | **Game thread** | The game | `en-gal`'s `task` | The one task closure: command drain, render feed, snapshot, flag frame, text and params (§7.3) | **Yes, only here** (R1) | Never | None after warm-up (the first 3 snapshot ticks); retired buffers go back to the core (C5) | `ffi_guard` → task component tripped → the closure returns at once → L1 |
| T3 | **Present** | The game (D3D12 present path) | GraceUI's render callback on its Present hook, drawn with Dear ImGui's DX12 backend (GraceUI G15, O25); EldenNet's part is `en-overlay`'s `on_present` callback | EldenNet's callback draws `UiModel` and the render feed as panels within GraceUI's budget (0.5 ms p99, GraceUI D71); GraceUI stamps the last render | No | Never | EldenNet's code: none after warm-up | GraceUI suspends a callback over budget for 120 frames; EldenNet's own trip → its panels draw nothing |
| T4 | **Window procedure** | The game's window thread | GraceUI (its window-procedure filter) | GraceUI pairs characters, places the IME, releases capture on focus loss and hands characters to the holder, EldenNet's chat box among them | No | Never | None | GraceUI's rules |
| T5 | **Game input** (one or more) | Whichever game thread calls `GetDeviceState` / `XInputGetState` | GraceUI (its DInput and XInput detours) | Zero the keyboard and mouse buffers and neutralise pads while a holder has input; the held-key mask (DESIGN §8.1, applied by GraceUI to every holder) | No | Never | None | GraceUI's rules |
| T6 | **Core** | T1, after the safety step | `eldennet`'s `shell` running `en-core` | Waits on one wake signal that every producer of C1–C11 and C16 raises, then drains each in a fixed order → `Core::step(input, now)` → effects | No | Only on its wake signal | Bounded (§10.2) | Subsystem reset after the first exception, disabled after the second (R8). An exception outside a subsystem stops the core: capture released, banner "EldenNet stopped after an internal error" |
| T7–T8 | **Net**: en-llm's own threads (built), a fixed pool of one more worker than the providers' total concurrency (2 to 8) and one dispatcher; on Windows also each exchange's driver thread (WHY§102) | T6 at Ready | `en-llm` (its `Service`, §2.2) | HTTPS and SSE streams; local probes; the dispatcher keeps the start deadlines and calls the notify callback (C10) | No | Network I/O: each worker waits on its own request | Yes | Caught per request: the request ends as `BadRequest` (built) |
| T9 | **Store** | T6 at Ready | `en-store` | One SQLite connection per open file, through its C API; migrations; backups | No | Disk I/O | Yes | Store tripped → toast "Memory is off for this session"; P events stay in the RAM ring (§8.9) |
| T10 | **Watcher** | T6 | `eldennet`'s `watch` (`ReadDirectoryChangesW`) | Hot reload with parse and validation (§11.6); saves the player's profile files sent on C17, after the same validation; every 60 s re-checks loaded modules for anti-cheat and `ersc.dll` (C14) | No | Yes | Yes | Hot reload off for the session, with a toast |
| T11 | **Log writer** | T6 | `eldennet`'s logger (our own, scrubbed; it never blocks a caller) | Writes rotating logs | No | Disk I/O | Yes | Lines dropped |

No other thread is created by EldenNet. GraceUI, a separate module, starts its own short init thread and its server's
threads (GraceUI D71, D81). The game's own threads enter EldenNet's code only through T2 and through GraceUI's
`on_present` callback on T3, and WinHTTP's own threads only through en-llm's status callback, which queues what
completed for the exchange's driver (WHY§102). Each of those entries is `noexcept` and starts with `ffi_guard` (R3).

### 1.3 Channels

```mermaid
flowchart LR
  T2["T2 game task"] -->|"C1 snapshot slot (2, latest wins)"| T6["T6 core"]
  T2 -->|"C2 lifecycle ring (64)"| T6
  T2 -->|"C3 render feed (triple buffer)"| T3["T3 present"]
  T6 -->|"C4 command ring (64)"| T2
  T2 -->|"C5 retire ring (64)"| T6
  T6 -->|"C6 UiModel (triple buffer)"| T3
  T3 -->|"C7 UiEvent queue (256)"| T6
  T4 -->|"C7: GraceUI's panel input, pulled with poll"| T6
  T6 <-->|"C8 store (1,024 each way)"| T9["T9 store"]
  T6 -->|"C9 requests (64)"| T78["T7-T8 net"]
  T78 -->|"C10 stream events (1,024)"| T6
  T10["T10 watcher"] -->|"C11 reloads (8)"| T6
  T6 -.->|"C12 GraceUI's input events"| T5["GraceUI's input holder"]
  T10 -.->|"C14 env atomics"| T2
  T6 -.->|"C15 ai_allowed"| T78
```

| # | From → to | Type | Capacity | Overflow policy |
|---|---|---|---|---|
| C1 | Task → core | `SnapSlot`: two preallocated `WorldSnapshot` buffers, each with an atomic state `Empty/Writing/Ready/Reading` | 2 | The writer overwrites a `Ready` slot the reader has not taken and sets `gap = true` in it. The reader always finds the newest snapshot. The core copies it out (≈ 8 KB, one copy into its own preallocated snapshot) and releases the slot at once |
| C2 | Task → core | SPSC ring of the fixed-size `Lifecycle` (a `std::variant` of trivially copyable kinds) | 64 | Not expected to fill. If full, the task sets the sticky `LIFECYCLE_OVERFLOW` atomic, and the core rebuilds the lifecycle state from the next snapshot's `life` and `health` fields (§9) |
| C3 | Task → overlay | Triple buffer of `RenderFeed` (§7.5), with an atomic index swap | 3 | Latest wins |
| C4 | Core → task | SPSC ring of `GameCommand` (DESIGN §2.3). Payloads are inline (a `FixedVector` within its capacity) or a `std::shared_ptr` whose last copy the task never releases (C5) | 64 | The core keeps an outbox. `SetWatchFlags` and `SetTracked` coalesce, last writer wins. `Resolve*` requests are deduplicated and re-sent if no answer arrives within 2 snapshots (impl) |
| C5 | Task → core | SPSC ring of `Retired` (the `std::shared_ptr<const std::vector<std::uint32_t>>` watch lists a newer version replaced) | 64 | If full, the task parks the pointer in a fixed 4-slot holdback and retries next tick. It never frees memory on the game thread |
| C6 | Core → overlay | Triple buffer of `UiModel`, with an atomic index swap, as C3: the core fills the free slot and publishes it; the overlay takes the newest at the start of a frame | 3 | Latest wins. The core owns every slot and reuses one only once the overlay has moved off it, so the present thread never waits and never frees (R6). Not `std::atomic<std::shared_ptr>`: MSVC's library and libstdc++ take a lock in it, and the present thread could drop the last copy |
| C7 | Overlay → core; since GraceUI's G15 also the chat box's input, which GraceUI's window procedure hands over and T6 pulls with `poll` | A lock-free bounded queue of `UiEvent` (our own) | 256 | `try_push`. On full, typing, page and debug events are dropped and counted. `Submit` and `CloseChat` wait in a one-slot retry cell inside the overlay and are pushed on the next frame |
| C8 | Core ↔ store | Bounded queues of our own (`DbRequest` / `DbResult`) | 1,024 each way | Diagnostics (traces, counters) are dropped first. P events are never dropped: they spill into the core's 4,096-entry RAM ring, with 256 slots reserved for speech (§8.9, DESIGN §6.6) |
| C9 | Core → net | The handle's request queue (built): `Service::submit` never blocks, and answers `Accepted`, `Full`, `Duplicate`, `Invalid` or `ShutDown` | 64 | Pool jobs are dropped first (an evicted `bark_pool` job ends `Dropped`) and memory jobs requeue (`Preempted`). Replies are never dropped; at most one dialogue request is in flight, and a persona's own-initiative request yields: the player's `Submit` and any input epoch cancel it first (DESIGN §6.6, §8.9) |
| C10 | Net → core | The handle's event queue of `LlmEvent` (built), read with `poll`, `drain` or `wait`; its notify callback raises the core's wake signal | 1,024 | A worker with a delta for a full queue waits; backpressure then reaches the HTTP stream. Tokens are never dropped. An ending never waits: it may pass the capacity by at most one per live request, so shutdown never blocks (WHY§101) |
| C11 | Watcher → core | A bounded queue of our own of `Reload` | 8 | Coalescing: a newer `ContentPack` or `Settings` replaces a queued one |
| C12 | Overlay ↔ GraceUI's input | GraceUI's state since its G15: EldenNet asks with `request_input` and `release_input`, and reads "input granted, refused, lost and free" and "input held by" through `poll`; the player-input epoch stays EldenNet's own atomic (`EPOCH`, a `std::atomic<std::uint64_t>`), advanced on those events | — | None |
| C13 | Task → debug and core | Per-group fault counters and a fixed 64-bucket timing histogram, all atomics | — | None; they saturate |
| C14 | Init and watcher → task | `ANTICHEAT` and `ERSC_LOADED`, each a `std::atomic<bool>` | — | None |
| C15 | Core → net | `AI_ALLOWED`: the client's gate, a `std::atomic<bool>` set with `Service::set_gate_open` (built). The net checks it before every send, as a second lock behind the core's own gate (C4 criterion, §5); closing it stops running requests and ends queued ones as `GateClosed` | — | None |
| C16 | Clock → core | A 100 ms tick (impl): the core's wait on its wake signal ends at the next tick at the latest, so expiry, page pacing and timers advance with no snapshots (title, loading) | — | Ticks coalesce |
| C17 | Core → watcher | A bounded queue of our own of `ProfileWrite` (`target`, `rev`, the file's text) from the settings UI's profile editor (DESIGN §7.7) | 8 | Never blocks: when full the save is refused with "busy, try again"; the result returns on C11 as `Reload::ProfileResult { target, rev, outcome }` |

The game, present, window and input threads use only non-blocking `try_push` and `try_pop` on lock-free queues,
atomics and preallocated buffers (DESIGN §2.1, R6). The SPSC rings (C2, C4, C5) and C7 are our own lock-free rings over
preallocated arrays. The bounded queues of C8, C11 and C17 are our own too, a mutex and a condition variable around a
ring, with the overflow policies above; only T6, T9 and T10 use them, and the core only with `try_push`. Every producer
of the core's inputs raises its one wake signal, the handle's notify callback among them.

### 1.4 Startup order

```mermaid
sequenceDiagram
  participant L as Loader (launcher, me3 or other)
  participant D as DllMain
  participant I as T1 init
  participant O as en-overlay
  participant U as GraceUI (graceui.dll)
  participant G as Game thread
  participant C as T6 core
  L->>D: LoadLibraryW (any time)
  D->>D: DisableThreadLibraryCalls, CreateThread(init)
  D-->>L: TRUE (no other work under loader lock)
  I->>I: pin module, gate (PE version vs gal/builds.toml)
  I->>I: safety (single-instance mutex, anti-cheat)
  I->>I: settings.toml: enabled? (no → exit, Off)
  I->>C: spawn core, watcher, log writer
  I->>I: readiness 1: main window exists (poll 100 ms)
  I->>O: connect to GraceUI (GraceUI G15; EldenNet hooks nothing)
  O->>U: find graceui.dll, GraceUI_RequestApi(1), connect
  U-->>O: ok, inactive (anti-cheat), too_old or refused
  O->>U: register faces, panels, hotkeys; world-state provider
  alt gate or safety failed
    I->>C: Banner(reason) → L1, init exits
  else
    I->>I: scan for readiness items (global_hinstance, CSTask static)
    I->>I: readiness 2: statics non-null, DLRF names resolve (≤ 120 s total)
    I->>I: discovery: AOBs, FD4 singletons, RTTI (retries +5, +15, +30 s)
    I->>G: register_task(ChrIns_PostPhysics)
    I->>C: spawn store and net
    G->>G: first ticks: group check, phase-A self-tests
    G->>C: Lifecycle::DataReady(caps) → L2, L3 or L4
    I->>I: exit
  end
```

- The core is up before the overlay, so the overlay always has a `UiModel` to draw, the banner included.
- **The connect** (GraceUI §2.9, D70–D73, D88). The launcher has already checked GraceUI's folder (§4.2). `en-overlay`'s helper finds `graceui.dll` on T1 (by module name, an export scan, then `Game\GraceUI\graceui.dll`, polled every 250 ms for at most 30 s) and connects; GraceUI's own init thread installs the hooks. A failed connect leaves EldenNet headless: L2 at most, nothing drawn, logged, with a hint the next time the launcher runs (§10.3). After GraceUI's own anti-cheat hit, `connect` answers `inactive (anti-cheat)` and the banner goes through `notice`, which stays callable. EldenNet registers its space for the browser view only once its gate and safety steps pass (GraceUI D81).
- Store and net threads start only on the path that can reach L2. An L1 session never opens a database or a socket; the dashboard's socket is GraceUI's (GraceUI D81).
- `register_task` is called from T1, a non-main thread, as the game's contract requires (ER01 §3.1).

### 1.5 Shutdown

The game never asks a DLL to shut down. EldenNet therefore never relies on a shutdown step.

| Event | What happens |
|---|---|
| Quit to the title | Lifecycle `Title` (§9): the conversation ends, P events flush, and the playthrough DB closes. Global DBs stay open |
| Runtime disable (`enabled = false` while running) | AI paused: the ladder drops to L2 (Observe), speech is dropped and no LLM request is sent. GraceUI's hooks, EldenNet's connection to them and the task stay installed (R12). Full **Off** takes effect at the next launch |
| Process exit (any path) | No code runs. Every DB request is one WAL transaction (R14). Speech commits (`PlayerSaid`, and each `NpcSaid` page as it shows) are sent as their own request in the same step. Other writes are coalesced by the core into one request at most every 250 ms (DESIGN §8.5). So a hard kill loses at most ≤ 250 ms of non-speech writes, and a shown line only in the few milliseconds before its own transaction commits. Logs may lose their last buffered lines |
| Crash of the game | Same as process exit. On the next `PlaythroughReady`, open conversations are closed and queued for memory (DESIGN §10.3) |
| `FreeLibrary` by a third party | Refused: T1 pins the module with `GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN)` before anything else, because GraceUI's callbacks and the task point into our code (impl) |

### 1.6 Single-instance mutex (R16)

- Name: `Local\EldenNet.Instance.<pid>`, made per process by the PID.
- T1 calls `CreateMutexW(NULL, FALSE, name)`. If `GetLastError() == ERROR_ALREADY_EXISTS`, this copy is the second one. It closes the handle, writes one `OutputDebugStringW` line and exits T1. It installs no hooks, spawns no thread and registers no task.
- The first copy holds the handle for the life of the process and never closes it.
- At Ready, the first copy enumerates loaded modules. If another module exports the marker symbol `eldennet_instance_marker`, it shows one toast: "A second copy of EldenNet was loaded and is idle." This happens, for example, when both the launcher and a me3 profile load the DLL.

### 1.7 `DLL_PROCESS_DETACH` (R14)

`DllMain` handles `DLL_PROCESS_ATTACH` only. The detach branch returns `TRUE` and does nothing: no unregistering, no flushing, no thread joins, no frees and no logging. A CI grep checks that the branch is empty (§10).

The C++ runtime also runs code at detach, outside that branch: the destructors of objects of static or thread storage
duration. So the DLL holds none with a non-trivial destructor: its long-lived state is made at boot and never destroyed,
and clang's `-Wexit-time-destructors`, an error in every module the DLL links, keeps such objects out (§10.1).

---

## 2. Modules

### 2.1 Graph

```mermaid
flowchart TB
  PROTO["en-proto<br/>messages, GameBackend, GameThread"]
  AUG["en-augur<br/>event engine"]
  PROMPT["en-prompt<br/>templates on inja (built)"]
  CORE["en-core<br/>identity, scheduler, conversation,<br/>barks, memory logic, prompts, content"]
  STORE["en-store (SQLite)"]
  LLM["en-llm (HTTP/SSE, built)"]
  GAL["en-gal<br/>gate, discovery, layouts, reader, task"]
  OVL["en-overlay<br/>display layer: panels and notices<br/>through GraceUI; F11 on Dear ImGui's core, no backend"]
  GUI["GraceUI's consumer modules<br/>graceui-sys, grace-protocol,<br/>grace-draw, grace-view (pinned tag)"]
  DLL["eldennet (DLL)"]
  LAUNCH["en-launcher (exe)"]
  TOOLS["en-tools (exe: eldennet-engine)"]
  AUG --> PROTO
  CORE --> AUG
  CORE --> PROTO
  CORE --> PROMPT
  STORE --> PROTO
  LLM --> PROTO
  GAL --> PROTO
  OVL --> PROTO
  OVL --> GUI
  CORE --> GUI
  DLL --> CORE & GAL & OVL & STORE & LLM
  TOOLS --> CORE & PROMPT & STORE & LLM
  TOOLS -.->|"offline part only"| GAL
  LAUNCH -.->|"offline part only"| GAL
```

The two dotted edges were added here and adopted into DESIGN §2.2 by the review. `eldennet-engine probe-exe` and the
launcher's build check need the PE parser, the pattern grammar and the `builds.toml` schema. These live in `en-gal`'s
**offline part** (its `pe`, `patterns`, `builds` and `layout` files): code over byte spans, with no Win32, no raw
address and no `reinterpret_cast`. It is a static library target of its own, which the full `en-gal` links; the dotted
edges link only that target, so `sys`, `reader` and `task` never reach the tools or the launcher.

### 2.2 Responsibilities and public API sketches

| Module | Owns | Public surface (sketch) |
|---|---|---|
| `en-proto` | Every message that crosses a thread or the adapter boundary. Each has a JSON form through nlohmann/json (`to_json`, `from_json`) | `WorldSnapshot`, `RenderFeed`, `Lifecycle`, `GameCommand`, `UiModel`, `UiEvent`, `LlmRequest`/`LlmEvent`, `DbRequest`/`DbResult`, `Capabilities`, `ReadHealth`, `ChrHandle`, `BlockId`, `SafeText`, the interface `GameBackend`, the token `GameThread`, `PROTO_VERSION`, `ffi_guard` and its components (R3), `parse_json`. Built (S8.1.j): `ffi_guard.hpp` (a constant-initialised tripped flag per component), `llm.hpp` (`LlmRequest`/`LlmEvent` and the request, decision and result types they carry, each with its JSON form; en-llm keeps their names by using-declarations, and §2.3 keeps `en-core` off `en-llm`) and `json.hpp` (§10.2's JSON caps, checked before nlohmann/json parses) |
| `en-augur` | Sources, world model, signals, detectors, rules, events, audience resolution, the K store, bus ordering | `Augur(RuleSet, Registry)`, `void Augur::step(const CoreInput&, Mono, const AugurCtx&, EventBatch&)`, the interface `EventSource`, `Expected<RuleSet, std::vector<RuleError>> RuleSet::load(std::span<const RuleFile>, const PredicateCompiler&)`, `Event`, `EventKind` |
| `en-prompt` (built, O84) | The template engine on inja 3.4, with SkyrimNet's template rules (its message markers and folders of numbered parts) made strict (NPC §7.1): composition with its guards, the function registry and the reference generated from it, lint; template sources (§2.3) | `Environment(std::shared_ptr<const TemplateSource>, FunctionRegistry, EnvironmentOptions)`, with `MemorySource(std::map<std::string, std::string>)` for `en-core` and `FolderSource(root)` for the tools (`Environment(root, …)` builds one), `std::vector<Message> Environment::render(std::string_view name, const nlohmann::json& context) const`, which throws a `TemplateError` naming the template, line and column, `std::vector<Finding> Environment::lint(std::string_view name, const nlohmann::json* context) const`, `FunctionRegistry::add` |
| `en-core` | Identity and tiers, content loading and validation, predicates, the world view and per-persona knowledge (`knowledge::world`), scheduler, conversation, barks, memory logic, prompts (rendered with en-prompt), decision calls (§11.8), sanitizer, lint, ladder policy, bus subscribers | `Core(std::shared_ptr<const ContentPack>, std::shared_ptr<const Settings>)`, `Outputs Core::step(CoreInput, Mono)`, `Expected<ContentPack, std::vector<ContentError>> ContentPack::load(const ContentFs&)`, the interface `CoreTransport`, `WorldView world::derive(const FlagState&, const WorldFacts&, const WorldContent&)`, `KnownSet world::known(PersonaId, const FlagState&, const WorldFacts&, const WorldView&, const WorldSeen&, const ContentPack&)`, `std::optional<Cue> world::cue(PersonaId, StageId, const KnownSet&, const WorldSeen&, const WorldCued&, const PersonaFacts&, const RenderedJournal&, const ContentPack&, Pt)` (MEM §12.13; `StageId` is the active stage `known()` resolved, whose `at` chooses LocalHere or LocalHome; its candidates are what the known set holds, O86) |
| `en-store` | SQLite files, migrations, the request loop, backups | `StoreHandle Store::spawn(StoreCfg)`, `Expected<void, Full> StoreHandle::send(DbRequest)`, `BoundedQueue<DbResult>& StoreHandle::results()` |
| `en-llm` (built, O84) | The OpenAI-compatible client and its transports (libcurl, WinHTTP), SSE parser, slots, health, budgets, decision calls (§11.8), local probes, warm-up, and the asynchronous handle the core drives (WHY§101) | `Service(Config, std::shared_ptr<TransportFactory>, ServiceOptions)`, `SubmitResult Service::submit(LlmRequest)`, and its `cancel(RequestId)`, `cancel_before_epoch(Epoch)`, `poll`, `drain`, `wait`, `set_notify`, `set_gate_open`, `warm_up(slot)`, `reload(Config)` and `shutdown`; the blocking `Client::chat` and `Client::decide` beneath it; `probe_local` (its `LocalEndpoint`s) and `list_models` |
| `en-gal` | The version gate, discovery, layouts, `GuardedReader`, read groups, the task, the snapshot builder, the FMG, param and flag readers, `SysLayer` | `Expected<BuildProfile, GateError> gate(const ImageView&, const Builds&)`, `Expected<Discovery, DiscoveryReport> discover(const SysLayer&, const BuildProfile&)`, `Expected<TaskToken, GalError> install_task(Discovery, TaskWiring)`, `class ErBackend : public GameBackend`, the interface `SysLayer` (§6.4) |
| `en-overlay` | EldenNet's display layer (GraceUI G15): the connection to GraceUI, every surface as a panel (subtitles, the chat box, the call-out box, the bark feed and labels, the talk hint) drawn in an `on_present` callback with `grace-draw` and `grace-view`, toasts and the banner as notices, its faces registered in GraceUI's atlas, input through `request_input`, its hotkeys registered with GraceUI, the world-state provider, and the F11 debug view on Dear ImGui's core (1.93.0, the version GraceUI pins, on a context of its own with no backend) submitted as a panel | `Expected<OverlayHandle, ConnectError> Overlay::connect(OverlayCfg, UiModelReader /* C6 */, RenderFeedReader, UiEventSink)`, `input::request()`, `input::release()`, `input::epoch()` |
| `eldennet` | `DllMain`, the boot sequence, thread spawning, wiring, the effect shell; its foreign entries start with en-proto's `ffi_guard` | `extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) noexcept`, `extern "C" __declspec(dllexport) const std::uint32_t eldennet_instance_marker` |
| `en-launcher` | The launcher program (§4) | CLI only |
| `en-tools` | The `eldennet-engine` command (built: `render-prompt`, `chat`, `decide`, `probe`, `models`), with the developer tools as its subcommands (§12.3), the replay `GameBackend` | CLI only |

```cpp
// en-core: the seam that lets a later out-of-process core replace the in-process one (DESIGN §13).
class CoreTransport {
 public:
  virtual ~CoreTransport() = default;
  virtual Expected<void, TransportError> push(CoreInput input, Mono now) = 0;
  virtual void poll_outputs(std::vector<Outputs>& out) = 0;
};
class InProcess final : public CoreTransport { Core core_; /* … */ };   // v1: calls Core::step directly on T6

struct CoreInput {
  struct Tick {};
  std::variant<std::unique_ptr<WorldSnapshot>, Lifecycle, UiEvent, LlmEvent, DbResult, Reload,
               EnvFacts /* SteamID folders, paths */, Tick> payload;
};
struct Outputs {
  std::vector<GameCommand> game; std::shared_ptr<const UiModel> ui /* null: unchanged */;
  std::vector<LlmRequest> llm; std::vector<RequestId> cancel; std::vector<DbRequest> db; bool ai_allowed;
  std::vector<LogRecord> log;
};
```

**The store's rules** (`en-store`; WHY§98 names the traps they avoid):
- a migration file is split into statements by SQLite's own parser (each prepared in turn from the tail the last one
  left), never by searching for `;`, so a trigger or a string that holds one stays whole;
- every id is 64-bit end to end: bound and read as a 64-bit integer, never through a 32-bit value or a double, and
  written as a string wherever a JSON reader may hold numbers as doubles (GraceUI's browser view);
- ids are never reused: a trim (MEM §9.2) never hands a removed row's id to a new row (impl: `AUTOINCREMENT`, or a
  high-water mark kept across trims), so an id that a view or, after v1.0, a code plugin holds (O68) never comes to
  name another row;
- numbers written into SQL, JSON, prompts or logs read the same in every locale: the shortest form that reads back
  exactly, with `.` as the decimal point and no grouping;
- the DB tests use realistic ids, above 2³² and above 2⁵³, never only 1, 2 and 3 (§12.2).

### 2.3 Dependency rules

| Module | May depend on | Must not depend on |
|---|---|---|
| `en-proto` | The standard library; nlohmann/json 3.11.3 (each message's JSON form). Our own fixed-capacity `FixedVector` and `FixedString` and the flag sets (an `enum class` with bitwise operators) live here | Anything with I/O, threads, clocks, Win32 or game knowledge |
| `en-augur` | `en-proto`, nlohmann/json, toml++ 3.4.0 (the rule files), our own `IndexMap` (a vector in insertion order with a hash index beside it) | `en-core`, `en-gal`, `en-store`, `en-llm`, `en-overlay`, any thread or network library, the Windows SDK, `std::filesystem` and file streams, any clock |
| `en-prompt` (built) | inja 3.4.0 (vendored, MIT), nlohmann/json; the standard library (`std::filesystem` and file streams in its folder source only, for the tools) | Every other module of ours, the Windows SDK, any clock |
| `en-core` | `en-augur`, `en-proto`, `en-prompt` (the template engine on inja, O84), toml++, nlohmann/json, RE2 (where a pattern comes from content, §10.2), utf8proc (grapheme boundaries), BLAKE3 (its C implementation), GraceUI's `grace-protocol` (pinned by tag; GraceUI eldennet-changes §2 item 5) | `en-gal`, `en-overlay`, `en-store`, `en-llm`, any network library, libcurl, WinHTTP, SQLite, the Windows SDK, any clock |
| `en-store` | `en-proto`, SQLite (its amalgamation, pinned at adoption, through its C API), the standard library's threads (the request loop and C8's queues) | `en-core`, `en-gal`, `en-overlay`, `en-llm` |
| `en-llm` (built) | `en-proto`, nlohmann/json, toml++ (its configuration files), libcurl (Linux and test builds, the `EN_WITH_CURL` option), the system's WinHTTP (Windows: loaded at run time by its full path from the system directory, never imported), the standard library's threads | `en-core`, `en-gal`, `en-overlay`, `en-store` |
| `en-gal` | `en-proto`, the Windows SDK's headers (`sys` only), toml++ | `en-core`, `en-augur`, `en-store`, `en-llm`, `en-overlay`, Dear ImGui, any network library |
| `en-overlay` | `en-proto`; GraceUI's `graceui-sys` (its header-only C helper and thin C++ wrapper), `grace-draw` and `grace-view`, pinned by tag and commit (GraceUI G15, its §2.7); Dear ImGui's core with no renderer backend, at the version GraceUI pins (1.93.0), for F11's context; the Windows SDK's headers | `en-gal` (it can never reach game memory), `en-core`; since GraceUI's G15, MinHook or any other hook library, an ImGui renderer backend, and GraceUI's `grace-host` and `grace-serve` (GraceUI invariant 16; DESIGN R12) |
| `eldennet` | All of the above and the Windows SDK (`ReadDirectoryChangesW` for T10). Its queues and its logger are our own (§1.3) | — |
| `en-launcher` | `en-gal`'s offline part, the Windows SDK, BLAKE3 (the backup manifests, §4.4); once it serves the dashboard off game (O76), GraceUI's `grace-serve`, pinned by tag (GraceUI G29, §4.13) | Every other module of ours |
| `en-tools` | `en-core`, `en-prompt`, `en-augur`, `en-store`, `en-llm`, `en-gal`'s offline part, zstd; doctest for its tests | `en-overlay`, the rest of `en-gal` (`sys`, `reader`, `task`) |

**`en-prompt` inside `en-core`.** `en-prompt` finds every template through a template source of its own
(`TemplateSource`, built in roadmap S8.1.j), which answers only what a name is (a file, a folder of parts or nothing),
which files a folder holds and what a file says. The name rules, a folder's parts and their order, and composition stay
in the environment, so every source gives the same messages and the same errors. The tools use `FolderSource`, which
reads a folder and refuses a name or a link that leads out of it. `en-core` does no I/O, so it hands `en-prompt` the
content pack's templates, already loaded by the content loader, in a `MemorySource`, which opens no file (its tests
render with no file of those names on disk); the folder source is never reached from `en-core`. Errors stay inside
`en-core` as `TemplateError`, caught where a prompt is built.

Every third-party library is vendored with its licence, or fetched at configure time from its own project, pinned by
SHA-256, into the build tree, and listed in `THIRD_PARTY.md` (§12.1).

**Never, in any module, at any version:** fromsoftware-rs's crates (`eldenring`, `shared`, `fromsoftware-*`),
`from-singleton`, me3's crates (`me3-*`), `dearxan`, libER, or code copied from any of them. The third-party check
refuses them by name (§12.1), and review refuses code copied from them the same way.

### 2.4 Lints and raw addresses

Every module builds with warnings as errors (`-Wall -Wextra -Wpedantic`, `/W4`) and with the checked-in `clang-tidy`
configuration, which carries the checks below for each module; a CI grep backs the ones `clang-tidy` cannot express.

| Module | Checks (`clang-tidy` and the CI grep) | Raw addresses and casts |
|---|---|---|
| `en-proto`, `en-augur`, `en-core` | Banned calls: every clock (`std::chrono`'s `now`, `QueryPerformanceCounter`), `std::this_thread::sleep_for` and `sleep_until`, `std::filesystem` and file streams, `std::getenv`. Banned types: `std::unordered_map` and `std::unordered_set` outside our own `IndexMap`, whose hash index is never iterated (iteration order differs between standard libraries and must replay bit for bit; use `std::map` or `IndexMap`) | None: no raw address and no `reinterpret_cast` |
| `en-gal` | Nothing that throws or aborts (`throw`, `assert`, `std::abort`, `std::terminate`, `__builtin_unreachable`); no unchecked access to a `std::optional` or an `Expected` (`bugprone-unchecked-optional-access` for the first); no unchecked indexing (`operator[]` only after a bound check, never `.at()`); checked arithmetic on values read from the game, never a silent wrap or a narrowing conversion; every raw-address operation with a `// SAFETY:` note; no lock type (`std::mutex`, `std::shared_mutex`, `std::condition_variable`, `CRITICAL_SECTION`, `SRWLOCK`) | `reader` (the SEH-guarded copy), `sys` (image view, trampoline, `register_task`, getter fallback, mutex, module and thread APIs) |
| `en-overlay` | Same list as `en-gal`. Since GraceUI's G15 it holds no hook: never unhooking or unloading while the game runs is the rule of GraceUI's `grace-host` (R12) | `abi` (calls across GraceUI's C ABI through `graceui-sys`, the `on_present` callback's entry, spans over its own draw lists) |
| `eldennet` | Same list as `en-gal` | None. Its `exports` file holds its two exports (`DllMain` and `eldennet_instance_marker`, §2.2) and no cast; `DllMain` calls `en::gal::sys::boot::on_attach`, which does the Win32 work |
| `en-store`, `en-llm`, `en-prompt`, `en-tools` | No unchecked access to a `std::optional` or an `Expected` (a warning in tests) | No game address. Casts only for a C API's own pointers (SQLite's, libcurl's and WinHTTP's callback contexts, `GetProcAddress`'s answer) |
| `en-launcher` | Same list as `en-gal` | `win` (process creation and injection: the other process's addresses and the Win32 handles). It runs outside the game process, so R9's in-process list does not cover it |

Every raw-address operation and every `reinterpret_cast` carries a `// SAFETY:` note (R9). A CI grep and `clang-tidy`
(`cppcoreguidelines-pro-type-reinterpret-cast`, `cppcoreguidelines-pro-bounds-pointer-arithmetic`) keep them inside
the files of the last column. `bugprone-exception-escape` checks that no exception leaves a destructor or a `noexcept`
function, and so no entry from foreign code (R3, R15).

### 2.5 Build options (all off by default)

Each is a CMake option that defines its macro (`EN_PROBE` and the rest) for the modules it names only; every other
module builds the same with it on or off.

| Option | Modules | Enables | Release CI |
|---|---|---|---|
| `probe` (`EN_PROBE`) | `en-gal`, `eldennet` | In-game invariant search that writes a draft layout (§6.10), plus the fault-injection hooks used by the C1 forced fail-closed launches | Refused |
| `hooks` (`EN_HOOKS`) | `en-augur`, `en-gal` | Hook sources H1–H3 (§8.11), and with them EldenNet's own hook library, in `en-gal`'s `hooks` (O65). Compiled out in v1 | Refused |
| `actions` (`EN_ACTIONS`) | `en-proto`, `en-core` | `GameCommand::Action`, executors, prompt partials | Refused |
| `freeze` (`EN_FREEZE`) | `en-proto`, `en-gal` | `GameCommand::Freeze` | Refused |
| `offline` | `en-gal` | Not an option but a target of its own: the offline part (§2.1), which the tools and the launcher link | Allowed (tools and launcher) |

The release job reads the release build's CMake cache and fails if any of `EN_PROBE`, `EN_HOOKS`, `EN_ACTIONS` or
`EN_FREEZE` is on, or if `eldennet.dll` links MinHook or an ImGui renderer backend at all (its link graph and its
symbols, `MH_*` and `ImGui_Impl*`; GraceUI's G15). GraceUI's release job checks its own pinned Dear ImGui and MinHook
(DESIGN O25).

### 2.6 Build profile and target

```cmake
# CMakeLists.txt: the release build of the Windows binaries, with clang-cl and lld-link
cmake_minimum_required(VERSION 3.20)            # CI pins LLVM and the xwin SDK and CRT
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)   # /MT: the static CRT
if(MSVC)                                        # clang-cl
  add_compile_options(
    /EHsc                                       # R3: ffi_guard's catch-all depends on it; CI fails if this changes
    /W4 /WX /permissive- /utf-8 /O2 -flto=thin
    /Z7 -gline-tables-only                      # PDBs ship beside each release for crash triage, not in the zip
    /Brepro)
  add_link_options(/DEBUG /Brepro /INCREMENTAL:NO)
endif()
```

- **Target:** x64 Windows with MSVC's ABI, built on Linux CI with clang-cl and lld-link (LLVM, pinned) against the
  MSVC CRT and Windows SDK that xwin downloads (pinned), through a CMake toolchain file beside the MinGW-w64 one in
  `cmake/`. So debug information is PDB, and SEH (`__try`/`__except`) is available to C++ directly.
- **Static CRT:** `/MT`, so the DLL needs no Visual C++ runtime on the player's machine.
- **Deterministic link:** `/Brepro` at compile and link, path prefixes mapped (`/clang:-ffile-prefix-map`),
  `SOURCE_DATE_EPOCH` and no incremental link (§12.1).
- **Overflow in `en-gal`:** arithmetic on values read from the game goes through checked helpers of our own
  (`__builtin_add_overflow` and its kin), so an overflow is a read fault for its group → breaker, never a silent wrap.
- **The SEH-guarded copy** is C++ in `en-gal`'s `reader` (§6.5), compiled by clang-cl with `__try`/`__except` inline:
  no separate shim.
- **The second Windows build:** MinGW-w64 (GCC, its runtime linked statically) builds every module but `en-gal`'s
  `reader` and `sys`, which need `__try`, and so not the DLL; it runs the Windows tests under Wine in the container
  (WHY§102). Linux builds (GCC or Clang) run every portable test.

---

## 3. The DLL load contract

### 3.1 `DllMain`

```cpp
// src/eldennet/src/exports.cpp: the DLL's two exports, and no cast (§2.4)
extern "C" BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) noexcept {
  return ffi_guard(Component::DllMain, TRUE, [&] {
    if (reason == DLL_PROCESS_ATTACH) en::gal::sys::boot::on_attach(module, &eldennet::boot::init_main);
    return TRUE;                                 // every other reason: nothing (R14)
  });
}
```

`on_attach` calls `DisableThreadLibraryCalls(module)` and `CreateThread(init_main)`, then returns. It allocates nothing, takes no lock and touches no file. The module path is resolved later, on T1, with `GetModuleFileNameW`. From that path T1 finds `content/`, `gal/` and the default log folder, so the install folder can live anywhere (DESIGN §3.1).

The CRT runs the dynamic initialisers of the DLL's static objects under the loader lock, before `DllMain`, so none of
them has one: clang's `-Wglobal-constructors` is an error in every module the DLL links, as `-Wexit-time-destructors`
is for detach (§1.7). Constants are `constexpr`, and the rest of the long-lived state is made at boot, after `DllMain`
has returned.

### 3.2 Init state machine

```mermaid
stateDiagram-v2
  [*] --> Pin
  Pin --> Gate
  Gate --> Safety: build known
  Gate --> SafetyBanner: unknown build
  Safety --> Settings: first copy, no anti-cheat
  Safety --> Exit: second copy
  Safety --> WaitWindow_B: anti-cheat present
  SafetyBanner --> WaitWindow_B: first copy
  SafetyBanner --> Exit: second copy
  Settings --> Exit: enabled = false (Off)
  Settings --> WaitWindow
  WaitWindow --> Connect: window exists
  WaitWindow --> Banner: 120 s
  WaitWindow_B --> ConnectB: window exists
  ConnectB --> Banner
  Connect --> ReadyScan
  ReadyScan --> WaitEngine: readiness items located or skipped
  WaitEngine --> Discover: statics non-null and DLRF names resolve
  WaitEngine --> Banner: 120 s since T1 start
  Discover --> Discover: retry at +5, +15, +30 s
  Discover --> Register: required items resolved
  Discover --> Banner: required item missing after +30 s
  Register --> FirstTick
  Register --> Banner: register call faulted
  FirstTick --> Ready: group check and phase-A tests pass
  FirstTick --> Inert: tests fail, or no tick in 30 s
  Inert --> Banner
  Ready --> Exit
  Banner --> Exit
```

| State | Does | Timeout | Failure → |
|---|---|---|---|
| Pin | Pin the module (§1.5); note the QPC start time | — | — |
| Gate | Read `VS_FIXEDFILEINFO`, `ProductName`, region, `TimeDateStamp` and `SizeOfImage` from the main module's mapped headers; match against `gal/builds.toml` (§6.3) | — | Unknown build → banner path (no task, zero reads, C2) |
| Safety | Mutex (§1.6). Anti-cheat check (§5.3). Logging starts here, so a second copy never writes to the shared log; gate results buffered until now are flushed | — | Second copy → Exit. Anti-cheat → banner |
| Settings | Parse `settings.toml` (§11.5). If parsing fails, use the defaults and show a toast later | — | `enabled = false` → Exit (Off) |
| WaitWindow | Poll every 100 ms for a visible top-level window owned by this PID | 120 s from T1 start | Banner (logged; drawn if the window appears later) |
| Connect | `Overlay::connect` (GraceUI G15; §1.4): find `graceui.dll`, `GraceUI_RequestApi(1)`, `connect`; then register EldenNet's faces, panels, hotkeys and its world-state provider. EldenNet installs no hook (R12): GraceUI's init thread hooks DXGI and D3D12 (its Present hook, which draws with Dear ImGui's DX12 backend), the window procedure, and DInput and XInput, all through its one MinHook, finding `GetDeviceState` through a throwaway `IDirectInputDevice8W`'s vtable and `XInputGetState` in every XInput DLL already loaded, by module name (GraceUI D69) | 30 s (the helper's bound) | A failed connect (`too_old`, `refused`, no GraceUI) → no banner can show. Log it, continue headless at L2 at most, and leave the launcher its hint (§10.3, GraceUI D88) |
| ConnectB | The banner path's connect: with anti-cheat present GraceUI is notice-only and `connect` answers `inactive (anti-cheat)`, so the banner goes through `notice`, which stays callable; an unknown build connects as usual and shows its banner the same way (GraceUI D88) | 30 s | No GraceUI → no banner can show; logged, with the launcher's hint |
| ReadyScan | Scan the image for the two readiness items only: `global_hinstance` (AOB) and the `CSTask` static (FD4 finder, §6.2). This is the earliest scan, done after the window exists | Part of the 120 s | Not found → readiness falls back to "window exists and the `CSTask` static is non-null" (DESIGN §3.1) |
| WaitEngine | Poll every 100 ms: `global_hinstance` static non-null (if found), then the `CSTask` static non-null and the DLRF name of its runtime class readable | 120 s from T1 start | Banner "EldenNet inactive: the game did not finish starting" |
| Discover | Resolve the full inventory (§6.1): 5 AOB items, 11 singletons, RTTI vtable sets, and image checks (§6.2). Unresolved items are retried at +5, +15 and +30 s (DESIGN §3.5) | ≤ 31 s plus scan time (budget 500 ms per pass, impl) | A required item is missing → banner. An optional item is missing → its group is disabled |
| Register | Build our task object and call `register_task` once from T1 (§6.4) | — | Fault caught by SEH → banner. Nothing else is retried |
| FirstTick | Wait for the first tick. On the game thread, the task checks that the group at the layout's index is named `ChrIns_PostPhysics` (§6.3), then runs the phase-A self-tests over ≤ 8 ticks (§6.7) | 30 s for the first tick (impl) | No tick or a failed structural test → task **inert** (the closure returns after one atomic load; zero reads) → banner |
| Ready | Send `Lifecycle::DataReady{build, caps}`; spawn store and net; T1 exits | — | — |

**Required vs optional items.**
- **Required:** `register_task`, `CSTask`, `WorldChrMan`, `GameDataMan`, `GameMan` and `CSSessionManager`. They back `boot`, `world`, `player` and `session`.
- **Optional:** everything else. A missing optional item disables only its read group (DESIGN §3.5).

`CSNowLoadingHelper` is optional too. Without it, loading is detected from `main_player` and the screen state alone (§9).

### 3.3 Early, late and very late load

| Load moment | Who | What the init thread sees | Result |
|---|---|---|---|
| Early: the process is suspended, before game main | Our launcher (§4.6) | No window yet. WaitWindow and WaitEngine take a few seconds | Normal path |
| Late: after game main, before the title | A player's me3 native, ME2, a proxy loader | The window exists, and statics fill at once | Passes the readiness wait immediately (DESIGN §3.3) |
| Very late: at the title or in the world | A delayed injector | As above | The first snapshot already shows `main_player`, so the lifecycle goes straight to Playthrough (§9) |
| During a loading screen | Any | `WorldChrMan.main_player` null; the load counter moving | The lifecycle starts in Loading and resolves on the next stable snapshot |
| Twice (launcher plus a me3 profile) | Both | The second copy sees the mutex | The second copy idles (§1.6) |

The DLL never assumes which loader it has, never calls a loader export, and needs no initializer symbol.

---
## 4. The EldenNet launcher

`eldennet-launcher.exe` is our own small Win32 program, about 400 lines. It has no main window: notices and errors use `TaskDialogIndirect`. It follows the documented community pattern: start `eldenring.exe` directly so EAC never runs, then inject a DLL into the suspended process (ER02 §2.1, §3.2). No code is taken from any launcher.

### 4.1 Command line

| Flag | Effect |
|---|---|
| *(none)* | Discover the game, run the pre-flight checks, back up saves, start offline without EAC, inject `eldennet.dll`, exit |
| `--game <path>` | Use this `Game` folder or `eldenring.exe` instead of Steam discovery. Remembered in `%APPDATA%\EldenNet\config\launcher.toml` |
| `--dll <path>` | Inject this DLL instead of `eldennet.dll` next to the launcher (DESIGN §3.2) |
| `--no-backup` | Skip the save backup (DESIGN §3.2) |
| `--vanilla-offline` | Start the game offline without EAC and **without** injecting anything (DESIGN §3.2) |
| `--backup-only` | Back up saves and exit. Meant for players who start through their own loader (§4.11) |
| `--cleanup` | Remove a `steam_appid.txt` that this launcher created, if the game is not running, and exit |
| `--print-diagnostics` | Write the discovery and pre-flight results to the console and to `launcher.log`, and exit |
| `--no-plugins` | Start with every plugin off, for support (§4.12; the first screen's Start without plugins) |
| `--plugins` | Check and install what is new in `plugins/`, show the list with the code plugins to turn on, and exit without starting the game: the way to manage plugins for players who start through their own loader (§4.12) |

`--game`, `--backup-only`, `--cleanup` and `--print-diagnostics` are additions to DESIGN's three flags (impl); `--no-plugins` and `--plugins` come with plugins in 1.1 (O66, O67).

### 4.2 Finding the game

1. If `--game` is given, or `launcher.toml` remembers a path, use it.
2. Otherwise read the Steam root from `HKCU\Software\Valve\Steam\SteamPath`, falling back to `HKLM\SOFTWARE\WOW6432Node\Valve\Steam\InstallPath`.
3. Parse `<steam>\steamapps\libraryfolders.vdf` (a small key-value reader of our own) and list every library `path`.
4. In each library, look for `steamapps\appmanifest_1245620.acf` (Steam app 1245620, ER02 header). Read `installdir`, and form `<library>\steamapps\common\<installdir>\Game\eldenring.exe`.
5. If nothing is found, show E01 with an "Open eldenring.exe…" button.
6. Read the exe's PE version resources from the **file** with the `pe` reader of `en-gal`'s offline part, and match them
   against `gal/builds.toml`. An unknown build shows E05, which offers "Launch anyway (EldenNet stays inactive)" (DESIGN
   §3.2).

### 4.3 Pre-flight checks

| Check | How | Failure |
|---|---|---|
| Steam is running | A process named `steam.exe` exists, or `HKCU\Software\Valve\Steam\ActiveProcess\pid` names a live process | E02 |
| No EAC bootstrapper | No process named `start_protected_game.exe` | E04 |
| The game is not already running | No `eldenring.exe` process | E03 |
| The DLL exists and is ours | The file exists, and its version resource reads `ProductName = EldenNet` | E08 |
| GraceUI is installed and fits (GraceUI D83, D88) | `Game\GraceUI\graceui.toml` is readable and every listed file matches its size and SHA-256; its ABI major is one EldenNet speaks and its version at least EldenNet's floor. Otherwise the launcher installs the GraceUI release EldenNet carries, never over a newer one, and over an older major only while the carried release still serves that major (GraceUI D74); outside that window it installs only when the player confirms, after naming the mods GraceUI's last log shows connecting on the older major | E08's manner: what is missing or would stop working, and what to do |
| The game folder is writable | Only needed for the file method (§4.5) | E06 |
| Other proxy DLLs | `dinput8.dll`, `winhttp.dll` or `dxgi.dll` next to `eldenring.exe` | Information only: "Other mods detected: …". Never blocks |

### 4.4 Save backups (rotation of 10)

- **Source:** each `%APPDATA%\EldenRing\<SteamID64>\` folder whose name is 17 digits. From each, copy `ER0000.sl2` and `ER0000.sl2.bak` if present (ER01 §11).
- **Destination:** `%APPDATA%\EldenNet\save-backups\<SteamID64>\<YYYYMMDD-HHMMSS>\`.
- **Method:** copy to `*.tmp`, check that the sizes match, then rename. Each backup folder gets a `manifest.toml` with sizes and blake3 hashes.
- **No duplicates:** if both hashes equal the newest backup's, no new folder is made (impl).
- **Rotation:** keep the newest 10 folders per SteamID and delete older ones. Deletion happens only after a new backup has succeeded.
- **Never:** the launcher never writes, moves or renames anything under `%APPDATA%\EldenRing\`.
- **Failure:** E07 offers "Continue without a backup" or "Cancel".

### 4.5 Starting without EAC: `steam_appid.txt` (the `SteamAppId` variant is pending S0-C)

| Step | File method (default) | Environment method (S0-C candidate, SPECULATION) |
|---|---|---|
| Create | If `<Game>\steam_appid.txt` is absent, write `1245620` to it. If it exists with that content, use it as is | Set `SteamAppId=1245620` and `SteamGameId=1245620` in the child's environment block. Write no file |
| Record | `%APPDATA%\EldenNet\config\launcher-state.toml`: `{ appid_file = "<path>", created_by_us = true/false, created_at }` | Nothing to record |
| Remove | `remove_after = "window"` (default pending S0-C): after the resume, the launcher waits up to 120 s for the game's main window, then deletes the file **only if `created_by_us`**. If S0-C shows Steam reads the file later, switch to `remove_after = "exit"` (wait for the game to exit) or `"never"`. Every launcher start, and `--cleanup`, removes a stale file we created when the game is not running | — |

The launcher never deletes a `steam_appid.txt` it did not create. The method is chosen in `launcher.toml` (`appid_method = "file" | "env"`).

### 4.6 Process creation and injection

```mermaid
sequenceDiagram
  participant LA as Launcher
  participant P as eldenring.exe (suspended)
  LA->>P: CreateProcessW(Game\eldenring.exe, cwd=Game, CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT)
  LA->>P: VirtualAllocEx(len of DLL path, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
  LA->>P: WriteProcessMemory(wide DLL path, NUL-terminated)
  LA->>P: CreateRemoteThread(LoadLibraryW from kernel32, path)
  P-->>LA: thread exits (WaitForSingleObject, 30 s)
  LA->>LA: GetExitCodeThread != 0, and the module is listed (Toolhelp32 snapshot)
  LA->>P: VirtualFreeEx, then ResumeThread(main thread)
  LA->>LA: wait for the window (§4.5), remove steam_appid.txt if ours
  LA->>LA: stay as the crash watcher until the game ends (O71), then exit
```

- **Absolute path:** the DLL path is absolute, so the game's search order is never used.
- **Failure before resume:** if any step fails, the launcher calls `TerminateProcess` on the suspended game and shows E10. A half-injected game never starts. E10 offers "Start without EldenNet" (the same as `--vanilla-offline`).
- **No elevation:** the launcher requests no elevation (`asInvoker` manifest).

### 4.7 Error texts

| Code | Text (original wording) |
|---|---|
| E01 | "ELDEN RING was not found. Pick eldenring.exe, or start the launcher with --game \"<folder>\"." |
| E02 | "Steam is not running. Start Steam, then run the EldenNet launcher again." |
| E03 | "ELDEN RING is already running. Close it, then start it again from this launcher." |
| E04 | "Easy Anti-Cheat is running (start_protected_game.exe). Close the game, then start it from this launcher." |
| E05 | "This game build (%s) is not supported by this EldenNet version yet. Launch anyway? EldenNet will stay inactive until an update arrives." |
| E06 | "The launcher could not write steam_appid.txt into the game folder (%s). Move the game to a folder you can write to, or see the install guide." |
| E07 | "Your save could not be backed up (%s). Continue without a backup?" |
| E08 | "eldennet.dll was not found next to the launcher (or at the --dll path)." |
| E09 | "The game could not be started: %s (code %u)." |
| E10 | "EldenNet could not be loaded into the game: the %s step failed (code %u). The game was closed again. Antivirus software sometimes blocks this step; see the install guide. Start the game without EldenNet instead?" |
| E11 | "This launcher must run inside the game's Proton prefix. See the Proton notes in the install guide." |

### 4.8 First-screen notices

The notices show on the first run, and again whenever their version number changes. The player can tick "Don't show again" (stored as `notices_seen = <version>`):

> **EldenNet runs offline only.** The game starts without Easy Anti-Cheat and cannot reach FromSoftware's servers. Never take a save you played with mods into online play.
> **Your saves are backed up, not separated.** EldenNet keeps the last 10 copies in `%APPDATA%\EldenNet\save-backups`. It never edits your save.
> **What you type is sent to the AI provider you choose.** That may be a cloud service or a program on your own PC.
> **NPC lines are generated by AI.** They are not written by FromSoftware, and they can be wrong.
> **Your dashboard.** While the game runs, GraceUI serves EldenNet's pages to a browser on this PC at `http://127.0.0.1:%u` (the port is read from GraceUI's own settings; GraceUI D84).

### 4.9 Proton (placeholder, pending S0-C)

S0-C settles the recipe. The candidate, SPECULATION until then:
1. In the game's Steam properties, set the launch options to `"<path>/eldennet-launcher.exe" %command%`. Proton then starts our launcher inside the game's prefix (`compatdata/1245620`). The launcher ignores the forwarded arguments and starts `eldenring.exe` itself.
2. `%APPDATA%` and the save path resolve inside the prefix.
3. Steam discovery reads the prefix registry; `--game "Z:\…\Game"` is the fallback.

If the recipe fails, the documented fallback is the player's own me3 profile (§4.11, DESIGN K5). E11 fires when the launcher detects it is running outside a prefix while Steam reports Proton.

### 4.10 Loading alternatives

| Method | Verdict | Notes |
|---|---|---|
| **Own launcher** | **Primary** | No third-party loader, and no EAC by construction (DESIGN §3.3) |
| **Player-configured me3 native** | **Supported, documented in our own docs only** | Needed for Seamless Co-op users (ER02 §2.3). It is a late load, so it passes the readiness wait at once. We rely on no me3 feature |
| `dinput8.dll` proxy | Rejected | Clashes with Elden Mod Loader, ME2's dinput8 mode and the practice tool. Cannot carry ERSC. Needs `WINEDLLOVERRIDES` under Proton (ER02 §3.3) |
| `winhttp.dll` proxy | v1.x note | Used by EROverlay and the EAC Toggler. EAC LIKELY refuses to start next to it (ER02 §2.1) |
| `dxgi.dll` proxy | Rejected | It would sit under GraceUI's own DXGI hooks and other DXGI injectors (SPECULATION) |
| ME2 `external_dlls`, Elden Mod Loader | Work, unsupported | The DLL is loader-agnostic; both loaders are discontinued or legacy (ER02 §3.1, §3.3) |
| Other chain loaders (Lazy Loader and similar) | Work if they call `LoadLibraryW`, unsupported | Nothing in the DLL depends on the loader |
| Our own proxy DLL | Deferred (DESIGN §1.2) | The proxy conflicts above would apply to us too |

### 4.11 The me3-native page (`dist/using-a-loader.md`, our docs only)

The page is written by us and lives only in our repository and release zip. It never links to or posts in me3's spaces. Outline:
1. **EldenNet does not need me3.** The launcher is the normal way to start it. me3 is a separate project with its own rules, which do not allow LLM-related work in its spaces. Please do not ask its maintainers or community about EldenNet. EldenNet support lives in EldenNet's issue tracker only.
2. **If you already start the game through your own me3 profile** (for example for Seamless Co-op), add `eldennet.dll` to that profile as a native DLL, by its absolute path. No load order is needed, because EldenNet waits for the game by itself.
3. **Do not also use the EldenNet launcher for the same session.** If both load it, the second copy stays idle.
4. **Co-op:** EldenNet goes dormant in any multiplayer or Seamless Co-op session and wakes 5 s after you are alone again.
5. **Saves and online play:** your profile's own save and online settings are yours to manage. EldenNet never writes your save, and it enforces its own offline check whatever the profile says. Back up first with `eldennet-launcher.exe --backup-only`.
6. **If something fails,** EldenNet shows a banner with the reason. Send the bug-report bundle from the settings panel (F10, Diagnostics).

---

### 4.12 Plugins (from 1.1; O66, O67)

Before the game starts, after the pre-flight checks (§4.3), the launcher looks at `<install>/plugins/`:
- each new folder holding a `plugin.toml`, and each `.zip` holding one such folder, is checked by [proposal
  0001](../proposals/0001-plugins.md) §6.3's archive rules and §5.2's manifest; an archive is unpacked into
  `plugins/<id>/` and then removed; a folder whose files no longer match its manifest is turned off with the reason;
- nothing installs until its validator passes ([proposal 0001](../proposals/0001-plugins.md) §13.1): it is checked in
  `plugins/pending/` and moved into place whole, or not at all, and a library clash with any installed code plugin, on
  or off, is named on both rows with Detached offered as the way out (§13.2);
- a new content plugin is turned on at the end of the player's order; a new or changed code plugin (after v1.0,
  O68) stays off until the player turns it on, with its name, version, author, SHA-256 and what it can reach shown,
  and the answer is kept until the DLL's hash changes; once it is on, a trial run in a throwaway plugin host, outside
  the game, must pass before the game starts with it (O70);
- plugins removed on the dashboard are deleted now; if `plugins/` is not writable, the launcher says so in E06's
  manner and changes nothing;
- **Start without plugins** starts the game with every plugin off, for support.

The dashboard's route (proposal 0001 §6.2) writes into `plugins/pending/` and is checked by T10 in the game, never on
the game, render or input threads; code never arrives that way. When the game is started another way (a player's own
loader, §4.10), T10 makes the same checks at boot for anything new in `plugins/`, and a code plugin stays off until the
player turns it on with `--plugins`.

### 4.13 The dashboard off game (O76; GraceUI G29)

Once EldenNet's engine runs off game in the launcher (O76 (b)), the launcher serves EldenNet's pages (PAGES) to the
player's browser with the game closed. It serves them through `grace-serve`, the one server library GraceUI publishes
for hosts outside the game (GraceUI G29 (a), decided by the owner on 2026-11-16, as recommended; its §7.2 item 26),
never through a server of its own, so every rule of GraceUI's §7.2 and its invariant 4 holds as in the game: loopback
only; the `Host`, `Origin` and Fetch Metadata checks, the session cookie and the ticket; a strict CSP; only the files
`graceui.toml` lists. The home-network switch and pairing (GraceUI G9) stay in the game at first, so the launcher opens
no network listener. `eldennet.dll` links no `grace-serve`: in the game the server is GraceUI's. What the off-game pages
show, the settings off game and the handover of the address between the launcher and the game are O76 to O79's fold
(roadmap S7.12.c).

## 5. The offline and multiplayer gate

### 5.1 Predicate

The gate is computed by the core on every snapshot from the `session` fields (§7.1):

```text
anticheat_ok = !snapshot.session.anticheat                 (C14 atomic, §5.3)
session_ok   = health.session == Ok                        (group passed its tests and is not tripped)
in_world     = snapshot.life.main_player
solo_world   = role == Host && is_my_world && lobby == None && player_set_len == 1
at_title     = !in_world && screen == Title && lobby == None

clean        = session_ok && anticheat_ok && (solo_world if in_world else at_title)
```

- **`clean` false** → the gate goes **false at once**: on the same snapshot, so within ≤ 250 ms (C4).
- **`clean` true** → the gate goes **true** only after `clean` has held on every processed snapshot for 5,000 ms of mono time, with no snapshot gap longer than 1,000 ms (impl) inside that window.
- **Unknown values:** a field that cannot be read, a value out of the enum range, or a `gap` snapshot all count as not clean.
- **The `at_title` clause** gives the wizard's self-test (DESIGN §11.5) a place to run, because at the title screen there is no `PlayerGameData` to read a role from. At the title the lifecycle is still Dormant for speech (§6.8), so the title clause only permits network calls for the wizard and the silent self-test.
- **If `session` fails its self-test or trips,** `session_ok` is false for the rest of the session. AI stays off; that is L1 for the structural case (DESIGN §4.4).

### 5.2 Effects of a false gate

On the step where the gate turns false, the core:
1. sets `ai_allowed = false` in that step's outputs (C15);
2. cancels every in-flight LLM request (the net aborts the HTTP streams);
3. ends the conversation with reason `dormant`, committing only lines already shown;
4. drops every pending bark and closes the call-out box;
5. sends `SetTracked{[]}`, so the render feed stops.

The net also refuses any request while `AI_ALLOWED` is false. A core bug therefore still cannot cause traffic (C4). While the gate is false, the GAL task reads only the `session` fields and the lifecycle subset (§7.3).

### 5.3 Anti-cheat check (SPECULATION, cheap)

| When | Thread | Check |
|---|---|---|
| Init, in the Safety state | T1 | `EnumProcessModules` on our own process: any module whose file name starts with `EasyAntiCheat` (case-insensitive). The parent process (`CreateToolhelp32Snapshot`, `th32ParentProcessID`) has the image name `start_protected_game.exe`. If the parent has already exited, the parent clause is skipped |
| Every 60 s | T10 | The module clause again; also refreshes `ERSC_LOADED` (a module named `ersc.dll`) |

A hit at init → the L1 banner through GraceUI's notice-only mode: GraceUI makes the same check before it hooks, draws notices only, and answers `connect` with `inactive (anti-cheat)` (GraceUI D88). A later hit → `ANTICHEAT = true` → the gate goes false for the rest of the session, with the banner, and EldenNet withdraws its space from the browser view (GraceUI D81).

### 5.4 Player-facing texts

| Situation | Surface | Text |
|---|---|---|
| Gate false in the world | Badge, top right | "EldenNet paused (multiplayer)" (DESIGN §2.7) |
| Waiting to re-activate | Badge | "EldenNet paused (multiplayer)" stays up until the 5 s window completes. No countdown is shown |
| Anti-cheat present | Banner | "EldenNet inactive: Easy Anti-Cheat is running. EldenNet only runs offline; start the game with the EldenNet launcher." |
| `session` group failed | Banner | "EldenNet inactive: it could not confirm this is an offline solo session (game build %s)." |

### 5.5 What EldenNet never does

- It never writes `Menu.IsEnableOnlineMode` or any game property; that would be a game write (DESIGN §3.4, J1 #1). CI greps the code for the string, and only the test that asserts its absence may contain it (§10).
- It never connects anywhere except the `base_url`s in `providers.toml` and the localhost probe ports (§11.4). Offline play follows from starting without EAC (ER02 §2.2).

---

## 6. Game Access Layer (GAL)

### 6.1 Object inventory

Types are our own. Offsets and copy sizes live in `gal/layouts/<build>.toml` (§6.3). **Copy size** names where the byte count for one guarded copy comes from. Field leads cite the research; each lead is confirmed by the probe and the self-tests before its `status` becomes `confirmed` (§6.3).

| Object | Discovery | Fields read (type) | Group | Copy-size source |
|---|---|---|---|---|
| Executable image | PE headers of the main module | `FileVersion` (4×u16), `ProductName` (UTF-16), translation (u16 lang, u16 codepage), `TimeDateStamp` (u32), `SizeOfImage` (u32), section table (name, RVA, size, flags) | gate | PE structures (fixed) |
| `register_task` | Interior-anchored AOB → containing function from `.pdata` | Code address only; called once | boot | — |
| `global_hinstance` | AOB → RIP-relative static in `.data` | `hinstance: u64` (non-null = ready) | boot | 8 |
| `GameDataMan` | AOB static | `play_time: u32` (ms), `death_count: u32`, `ng_lvl: u32`, `boss_fight_active: u8`, `show_subtitles: u8` (inside the settings block via `settings: ptr`), `player_game_data: ptr` | player, session, ui | `class.GameDataMan.copy_size` |
| `PlayerGameData` | Via `GameDataMan.player_game_data` | `name: u16[17]` (raw name; lead `+0x9C`, ER05 §2), `level: u32`, `archetype: u8`, `body: u8`, `keepsake: u8`, `runes: u32`, `multiplay_role: u8`, `is_my_world: u8`, `gear: u32[6]` (right and left weapon, head, chest, hands, legs) | player, session | `class.PlayerGameData.copy_size` |
| `GameMan` | AOB static | `save_slot: i32`, `talk_esd_camera_param_id: i32` | player, ui | `class.GameMan.copy_size` |
| `FieldArea` | AOB static | `map_place_name_id: i32` | place | `class.FieldArea.copy_size` |
| `CSTask` | FD4 singleton | Instance pointer, passed to `register_task`. Read on the game thread: the task-group table entry at our index (`name: u16[64]`) | boot | `class.CSTaskGroupEntry.stride` |
| `WorldChrMan` | FD4 singleton | `main_player: ptr`, `by_distance: DLVector{begin: ptr, end: ptr}` of `{chr: ptr, distance: f32, pad: u32}`, `player_chr_set: {entries: ptr, capacity: u32}` | world, session | `class.WorldChrMan.copy_size`; vector elements `class.ChrDistEntry.stride` × ≤ 48 |
| `ChrIns` (and subclasses) | Through the vector; the vtable is checked against the RTTI set every tick | `vtable: ptr`, `handle: u64`, `npc_param_id: u32`, `model: u32`, `entity_id: u32`, `chr_type: i32`, `team: u8`, `block_id: u32`, `flags_onscreen: bit`, `flags_dead: bit`, `last_hit_by: u64`, `modules: ptr` | world, combat | `class.<rtti-name>.copy_size` per class (base `ChrIns` lead 0x580, ER05 §6.2) |
| `ChrIns` modules | Via `modules` | data: `hp, max_hp, fp, max_fp, stamina, max_stamina: i32`; physics: `pos: f32[4]`; action flag: `recv_dmg_type: i32`; throw: `throw_state: u8` | world, combat | `class.<Module>.copy_size` |
| `PlayerIns` extras | Main player only | `locked_on: u64`; ride module `ride_state: u8` (5 = riding, ER05 §3); SpEffect list head (walked ≤ 64 entries, `param_id: i32`; looking for 9621) | player, combat | `class.PlayerIns.copy_size`, `class.SpEffectEntry.stride` |
| `ChrIns` SpEffect list (S8) | Through the vector, every snapshot character | `special_effect` entries (`param_id: i32`), bound and stride from S8; `PlayerGameData.resistance_gauges[7]`, `proc_status_timers[7]` (the Tarnished); `SpEffectParam` classification rows (the `stateInfo` lead) in `params` | status | `class.SpEffectEntry.stride` |
| `CSSessionManager` | FD4 singleton | `lobby_state: i32` | session | `class.CSSessionManager.copy_size` |
| `CSEventFlagMan` | FD4 singleton | `divisor: u32`, `descriptor_map: DLMap header`, `holder_base: ptr`; nodes `{left, parent, right: ptr, key: u32, mode: u32, value: u64}`; blocks of 125 bytes | flags | `class.FlagNode.stride`, 125 per block |
| `CSFeMan` | FD4 singleton | `hud_state: i32`, `banner_id: i32`, `boss_bars: {handle: u64, name_id: i32, damage_taken: i32}[3]` | ui | `class.CSFeMan.copy_size` |
| `CSMenuMan` | FD4 singleton | `screen_state: i32` (0 InGame, 1 Loading, 256 Title; ER05 §9), `popup: ptr` → `current_talk_id: i32` | ui | `class.CSMenuMan.copy_size`, `class.CSPopupMenu.copy_size` |
| `CSNowLoadingHelper` | FD4 singleton (confirm in S0-A) | `menu_load_counter: u32` | ui | `class.CSNowLoadingHelper.copy_size` |
| `WorldAreaTime` | FD4 singleton | `clock: u64` (packed hours, minutes, seconds) | place | `class.WorldAreaTime.copy_size` |
| `CSCamera` | FD4 singleton (confirm in S0-A) | `pers_cam: ptr` → `matrix: f32[16]`, `fov: f32` (rad), `aspect: f32` | camera | `class.CSCam.copy_size` |
| `SoloParamRepository` | FD4 singleton | Slot table → param resource → `PARAM` header `{row_count: u16, rows: ptr}`, row descriptors `{id: u32, data: ptr}`; row fields `NpcParam.{name_id: i32, team: u8}`, `BonfireWarpParam.{flag: u32, text_id: i32, area: u8, grid_x: u8, grid_z: u8, pos: f32[3]}`, `WorldMapPlaceNameParam.{text_id: i32, area: u8, grid_x: u8, grid_z: u8}` | params | `param.<Name>.row_size` (must equal the paramdef size in the layout) |
| `MsgRepository` | FD4 singleton | `tables: ptr` (lead `+0x08`), `count: i32` (lead `+0x14`, ER01 §8.3); FMG `{group_count, groups[], string_offsets[]}`; UTF-16 strings | text | `class.Fmg*` strides; strings ≤ 512 bytes |

That is 5 AOB items, 11 FD4 singletons and about 65 fields (DESIGN §4.1).

**Group labels.** The boss bars belong to `ui`. The lifecycle subset is `WorldChrMan.main_player` (in `world`) plus the screen state and load counter (in `ui`). The `boss_fight_active` cross-check is in `player`. That keeps exactly the eleven breaker groups of DESIGN §4.3 (DESIGN §4.1 now uses the same labels). `status` is optional, off until S8 passes; its trip turns off status awareness and the runtime sleep signal only (DESIGN O40).

### 6.2 Discovery

All discovery runs on T1 over the executable image. Sections are copied in 1 MiB chunks through `GuardedReader` (with an overlap of the longest pattern). Unreadable pages are skipped and logged, never touched directly (impl).

**AOB items** (`register_task`, `global_hinstance`, `GameDataMan`, `GameMan`, `FieldArea`). All patterns are matched in a single multi-pattern pass over `.text`.
1. Each pattern must match exactly `expect_matches` times (default 1).
2. **Functions:** the hit is mapped to its containing function through the exception directory (`.pdata` `RUNTIME_FUNCTION`). The pattern must sit at `entry + anchor_min` or later, with `anchor_min ≥ 5`, so it is inside the body: other mods hook prologues (ER01 §4). Then the function length (`EndAddress − BeginAddress`) is checked to lie within `fn_len`, and the optional prologue bytes are compared.
3. **Statics:** the RIP-relative displacement at `capture.at` resolves to an address in `.data`, aligned to 8.
4. **RTTI, where declared:** on the first tick, the instance's vtable must resolve to the declared RTTI class.

**FD4 singleton finder** (our own code, written from the documented idiom; ER01 §4).
1. Scan `.text` for the instruction shape `mov reg, [rip+static]; test reg, reg; jnz short; lea rcx, [rip+class]; call getter; …; lea rcx, [rip+path]; call log`. The byte grammar is the `fd4_idiom` entry in `patterns.toml`, from our own derivation in S0-A.
2. For each hit, check the placement: `static ∈ .data` and 8-aligned, `class ∈ .data`, `getter ∈ .text` (and the start of a `.pdata` function), and `path ∈ .rdata`. Read the path as an ASCII string of ≤ 260 bytes; it must end with `FD4Singleton.h`.
3. **Name the singleton:** if `builds.toml` says `dlrf_name_layout = true` (S0-A), read the DLRF runtime-class name from the class object at `dlrf.name_ptr_off` (an `.rdata` string of ≤ 128 bytes). Otherwise, call the getter once for this candidate, from T1 under the SEH guard. This is the only game-function call apart from `register_task`, and it never happens after registration (DESIGN §4.2).
4. **Index by name.** A name found twice with different statics is ambiguous; that singleton is unresolved and its group fails closed.
5. **Report** each `{name, static_rva, class_rva}` in the discovery report (log, F11 view, probe tools).

Budget: about 10–15 ms of matching is expected (ER01 §4). A whole discovery pass, including chunk copies, is capped at 500 ms (impl).

**RTTI walk.**
- Find type descriptors by mangled name (`.?AV<Class>@CS@@`) in `.data`.
- Find the complete-object locators in `.rdata` that reference them: x64 signature 1, image-relative offsets, and a self-RVA check.
- The vtable starts one pointer after the locator's address slot.
- Each class's hierarchy descriptor gives its base-class array. Every class that lists `CS::ChrIns` as a base joins the **ChrIns vtable set**, which is kept as a sorted fixed array for binary search. `CS::PlayerIns` gets its own set.
- Singleton classes declared in the layout (`rtti = "…"`) get one expected vtable each.

### 6.3 File schemas (`gal/`, read at boot only)

```toml
# gal/builds.toml
schema = 1
[[build]]
key              = "2.7.2.0-ww"
app              = "1.17.2"
file_version     = "2.7.2.0"
product_name     = "ELDEN RING"
region           = "ww"            # from the 4th version field (0 = ww, 1 = jp; ER01 §1), cross-checked with the translation lang
time_date_stamp  = 0x00000000      # from our own probe of this exe
size_of_image    = 0x00000000
layout           = "layouts/2.7.2.0-ww.toml"
dlrf_name_layout = false           # true once S0-A confirms the class-name layout
status           = "supported"     # supported | retired (kept for players who block updates)
```

The gate requires all of `file_version`, `product_name`, `region`, `time_date_stamp` and `size_of_image` to match. A same-version rebuild therefore stops at L1 with the reason "build fingerprint differs" (J1 #8).

```toml
# gal/patterns.toml
schema = 1
[[pattern]]
id             = "register_task"
kind           = "function"        # function | static | idiom
section        = ".text"
bytes          = "<hex bytes with ?? wildcards; from our own derivation in S0-A>"
expect_matches = 1
anchor_min     = 5                 # interior anchor: hit ≥ entry + 5
fn_len         = [0x40, 0x400]     # bounds on the .pdata length
prologue       = ""                # optional exact bytes at the entry
builds         = ["2.7.2.0-ww", "2.7.2.1-jp"]   # where it was confirmed
provenance     = { method = "own-derivation", leads = ["ER01 §3.2"], evidence = "docs/ops/probes/2.7.2.0-ww.json", date = "YYYY-MM-DD" }

[[pattern]]
id      = "GameDataMan"
kind    = "static"
section = ".text"
bytes   = "<…>"
capture = { at = 3, kind = "rip_rel32", insn_end = 7 }   # target = hit + insn_end + disp32
target_section = ".data"
rtti    = "CS::GameDataMan"         # checked on the first tick, against the instance
builds  = ["2.7.2.0-ww"]
provenance = { method = "own-derivation", leads = ["ER01 §4"], evidence = "…", date = "…" }
```

- **Derivation note (DESIGN O27):** every pattern has a short note, `gal/derivations/<id>.md`, in our own words: what was searched for, in which build, how the hit was confirmed, and the evidence file. It is reviewed with each `gal/` change, and lint fails on a pattern without one. It is the record behind `provenance.method = "own-derivation"`, which CI cannot check by itself.
- **Grammar:** hex byte pairs, `??` for a wildcard byte, whitespace ignored.
- **Load-time validation:** ids are unique; every required id is present; `capture` lies within the pattern; and `provenance.method` is `own-derivation`. Anything else fails CI and the boot (O4).
- **What a lead may carry:** a lead says *what* to look for (the function's role, its caller, the singleton's name, the idiom's shape). It never supplies the bytes. Byte strings that the research quotes from fromsoftware-rs's mapper profile, from TGA's table or from other projects are not pasted into `patterns.toml`; every pattern is cut from our own disassembly of the exe in S0-A and recorded with its evidence file (DESIGN §4.2, O4).

```toml
# gal/layouts/2.7.2.0-ww.toml
schema       = 1
build        = "2.7.2.0-ww"
derived_from = "probe"                       # or the key of the previous build
probe_record = "docs/ops/probes/2.7.2.0-ww.json"

[task]
group          = { name = "ChrIns_PostPhysics", index = 0 }  # the name is checked on the first tick
register_abi   = "v1"                        # argument-order variant, fixed by S0-A
vtable_slots   = 0
run_slot       = 0
object_size    = 0x0

[class.ChrIns]
rtti      = "CS::ChrIns"
copy_size = 0x580                            # lead ER05 §6.2; must cover every field below
status    = "lead"                           # lead | confirmed
[class.ChrIns.fields]
vtable       = { off = 0x0, ty = "ptr" }
handle       = { off = 0x0, ty = "u64",  group = "world" }
npc_param_id = { off = 0x0, ty = "u32",  group = "world" }
team         = { off = 0x0, ty = "u8",   group = "world", range = [0, 78] }
dead         = { off = 0x0, ty = "bit",  bit = 7, group = "world" }
last_hit_by  = { off = 0x0, ty = "u64",  group = "combat" }
modules      = { off = 0x0, ty = "ptr",  to = "ChrModules", group = "world" }

[enum.hud_state]  # raw value → snapshot enum
values = { 0 = "HideAll", 1 = "ShowAll", 2 = "PopupMenu", 3 = "Default" }

[param.NpcParam]
slot     = 6                                 # lead ER01 §8.1
row_size = 0x0
fields   = { name_id = { off = 0x0, ty = "i32" }, team = { off = 0x0, ty = "u8" } }

[selftest]
npc_name_ids   = []                          # known ids that must give 1–64 printable chars (ids only, never text)
place_name_ids = []
npc_param_ids  = []
```

- **Field types:** `u8`, `u16`, `u32`, `u64`, `i32`, `f32`, `bool8`, `bit`, `ptr`, `handle`, `vec3f`, `mat4f`, `utf16[n]`, `dlvector`, `dlmap`, `array{elem, len, stride}`.
- **Validation at boot:**
  - every `off + size ≤ copy_size`, and `copy_size ≤ 0x4000`;
  - every `to` and `elem` names a defined class;
  - every field the groups require is present;
  - enum tables are total over their `range`.
- **Lead and confirmed:** a release build refuses a layout that has any `lead` entry, so every offset is confirmed before players run it. Dev builds accept leads, with a warning banner.

### 6.4 `SysLayer`, `GameBackend` and `GameThread`

```cpp
// en-proto
class GameThread {                                 // empty and move-only; only issue() makes one
 public:
  GameThread(GameThread&&) noexcept = default;     // no copy
  // Only the task trampoline (en-gal) and the replay harness (en-tools) may call this; CI enforces it.
  static GameThread issue() noexcept { return GameThread{}; }
 private:
  GameThread() noexcept = default;
};

class GameBackend {                                // DESIGN §2.3, unchanged
 public:
  virtual ~GameBackend() = default;
  virtual Capabilities probe(const GameThread& gt) = 0;
  virtual Expected<void, ReadError> snapshot(const GameThread& gt, WorldSnapshot& out) = 0;
  virtual Expected<void, ReadError> render_feed(const GameThread& gt, std::span<const ChrHandle> want,
                                                RenderFeed& out) = 0;
  virtual Expected<void, ApplyError> apply(const GameThread& gt, const GameCommand& cmd) = 0;
};
```

- The token cannot be copied, and only `GameThread::issue` makes one. C++ has no `Send`, so the type alone does not
  keep it, or a reference to it, on the thread that made it.
- `GameThread::issue` may appear only in `src/en-gal/src/task.cpp` (the trampoline) and `src/en-tools/src/replay.cpp`.
  A CI grep enforces this (§10), because C++ cannot limit a public function to two files.
- Code off the game thread therefore holds no token to read with, short of breaking that CI rule (DESIGN §2.3). Debug
  and test builds also check, at each read, that it runs on the thread that issued the token (impl).

```cpp
// en-gal: the narrow platform seam (DESIGN §4.7). Our own implementation is `NativeSys`; replay and
// unit tests use `FakeSys` over a byte-buffer "process".
struct FrameCtx { std::uint64_t qpc_now_us; std::uint32_t dt_us; };
class TaskBody {
 public:
  virtual ~TaskBody() = default;
  virtual void run(const GameThread& gt, const FrameCtx& frame) = 0;
};
class SysLayer {
 public:
  virtual ~SysLayer() = default;
  // init (register_recurring once)
  virtual const ImageInfo& image() const = 0;                                         // sections, version, .pdata
  virtual Expected<StaticSlot, SysError> singleton(std::string_view name) const = 0; // from the FD4 finder
  virtual Expected<VtableSet, SysError> rtti_vtables(std::string_view cls, bool subclasses) const = 0;
  virtual Expected<TaskToken, SysError> register_recurring(TaskPhase phase, std::unique_ptr<TaskBody> body) = 0;
  // game thread
  virtual Expected<ParamTableView, SysError> param_table(const GameThread& gt, ParamKind which) const = 0;
  virtual GuardedReader reader(const GameThread& gt) const = 0;
};
```

**Task trampoline.**
- Our task object is one zeroed block laid out as a C struct: a pointer to a static fake vtable (`vtable_slots`
  entries, each a guarded no-op), then the layout's `object_size` bytes, then a pointer to our `TaskBody`.
- The `run_slot` entry is the `extern "C"` function `en_task_run(self, data)`, `noexcept`. It starts with
  `ffi_guard(Component::Task, …)`, checks the `TASK_INERT` atomic, issues the `GameThread` token, reads QPC and calls
  `body->run`.
- The object and its `TaskBody` are never freed: they live for the life of the process and are never unregistered
  (R14).
- The `register_task` argument order is the layout's `register_abi`, confirmed in S0-A.

### 6.5 `GuardedReader`

```cpp
// src/en-gal/src/reader.cpp: our own SEH-guarded copy, in C++, compiled by clang-cl (no separate shim)
namespace {
int en_filter(unsigned long code) noexcept {
  return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER
                                                                               : EXCEPTION_CONTINUE_SEARCH;
}
}  // namespace

// SAFETY: the one read of a game address, after the plausibility checks below, into storage the caller owns.
// `memcpy` stays a call (`len` is never a constant): clang's SEH catches a fault raised in a callee's frame, not one
// raised in the `__try`'s own (S0-D's forced faults test it).
bool en_seh_copy(void* dst, const void* src, std::size_t len, unsigned long& code) noexcept {
  __try {
    std::memcpy(dst, src, len);
    return true;
  } __except (en_filter(code = GetExceptionCode())) {
    return false;
  }
}
```

```cpp
// Pod: trivially copyable, standard layout, every bit pattern valid (integers, floats and arrays of them); each
// type's size is checked with static_assert against its layout.
template <class T>
concept Pod = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

class GuardedReader {
 public:
  // One SEH-protected copy into caller-owned storage. Never forms a pointer or reference into game memory.
  Expected<void, ReadFault> copy_into(Group group, std::uint64_t addr, std::span<std::byte> dst);
  template <Pod T>
  Expected<T, ReadFault> copy(Group group, std::uint64_t addr);
  Expected<ObjView, ReadFault> object(Group group, std::uint64_t addr, const ClassLayout& cls, const VtableSet& set,
                                      ScratchBuf& scratch);   // copy + vtable check
};
```

**Plausibility, before any copy:**
- `addr ≠ 0`;
- `0x10000 ≤ addr` and `addr + len ≤ 0x7FFF_FFFF_0000` (canonical user mode);
- `addr % align == 0` for the type;
- `addr + len` does not overflow;
- `len ≤ 0x4000`.

**Plausibility, after an object copy:** the vtable at `+0` lies in `.rdata` and is a member of the expected `VtableSet` (R4).

**Reading copied values.** `ObjView` reads fields from the copied bytes by layout offset, with bounds-checked `get`. Each typed field is checked against its `range`.

**Budget per tick** (impl): ≤ 512 copies and ≤ 256 KiB. Past either cap, the snapshot is abandoned for this tick and the next snapshot carries `gap = true`; this counts as a soft fault.

**Faults.**
- A caught exception, or a failed plausibility check on a structural field, is a **hard fault** for the field's group (§6.7).
- A vtable mismatch on one list entry is a **soft skip**: the entry is dropped from this snapshot and counted. More than 25% of entries mismatching in one tick is a hard fault for `world` (impl).

**Vectored handlers.** Crash reporters that install vectored exception handlers see the exception before our frame handler (SPECULATION, J1 #9). S0-D tests this. If S0-D fails, the same API switches its copy primitive to `ReadProcessMemory(GetCurrentProcess(), …)`, which raises no exception at all, at a cost of about 1 µs per copy (impl, measured in S0-D).

**Allocation.** Every scratch buffer is preallocated at registration, sized to the largest `copy_size` per class.

### 6.6 Per-tick re-resolution and vtable compares (R5)

Each snapshot tick:
1. Reads every static slot again (8 bytes each). No instance pointer is kept across ticks.
2. A null `WorldChrMan` or `main_player` means not in the world, or loading; the snapshot then carries only lifecycle fields.
3. Compares each singleton instance's vtable with its expected RTTI vtable. A mismatch means the object moved or was torn down. The group is skipped for this tick, and it is a hard fault only if it repeats on 3 consecutive ticks while `main_player` is present (impl).
4. Walks `by_distance` from `begin` to `end`. Each entry is checked: an aligned pointer, element count ≤ 48, and an entry vtable in the ChrIns set.
5. Resolves the core's tracked handles by matching `handle` within this walk. There is no separate handle-lookup call, and a handle that is absent is simply not present this tick.

### 6.7 Read groups, self-tests and breakers

**Phase A** runs on the first ticks after registration, wherever the game is (usually the title). **Phase B** runs at each playthrough start, on the first in-world snapshots (DESIGN §4.3). A phase-A test that needs the world is deferred to phase B.

| Group | Phase A | Phase B | Per-tick plausibility | Structural |
|---|---|---|---|---|
| `session` | `CSSessionManager` instance vtable OK; lobby in its enum range | Solo play reads `Host` and `is_my_world`; `player_set_len == 1` | Lobby and role in range | **Yes**; must pass, or AI stays off |
| `world` | `WorldChrMan` static resolves; at the title, `main_player` null is accepted | Entry vtables are ChrIns subclasses; distances ascend (within 0.5 m); `0 < max_hp ≤ 100000`, `hp ≤ max_hp`; every `npc_param_id` resolves | The same, per entry | **Yes** |
| `combat` | — | Handles resolve or are null; throw state in 0–6 | The same | No |
| `player` | `GameDataMan` and `GameMan` instances non-null, with vtables matching wherever the layout declares an RTTI class | The main player's vtable is `CS::PlayerIns`; the name is valid UTF-16 of 1–16 chars; slot in 0–9; play time rising over 2 snapshots | Slot and level in range; play time never decreasing within a playthrough | **Yes** |
| `flags` | Divisor == 1000 | Flag 100 set; every catalogue ID's group exists (unallocated groups read false, ER01 §9) | Block pointers aligned | No |
| `text` | ≥ 20 tables | Known NpcName and PlaceName IDs give 1–64 printable chars | String length ≤ 256 UTF-16 units | No |
| `ui` | `hud_state` in 0–3; screen state in {0, 1, 256} | Banner and popup values in range | The same | No |
| `place` | — | The place ID resolves; hour < 24 | The same | No |
| `camera` | — | Orthonormal rotation (each row's length within 1 ± 0.01, rows orthogonal within 0.01); FOV in 0.3–2.5 rad | The same | No |
| `params` | Known rows found; row sizes match the paramdef | — | Row IDs ascending in the descriptor table | No |
| `status` | — | Every walked list ends within its bound; classified statuses map to DESIGN §9.9's set; the Tarnished's gauge slots in range | The same | No |

**Breaker.** Each group counts hard faults in a sliding 60 s mono window. On the third fault it trips: the group is off
for the session, its fields read as empty (`std::nullopt`), the features that depend on it degrade (§6.8), and one toast
names the reduced feature (R8). There is no half-open state in v1: a reset needs a restart. A structural group tripping
moves the ladder to L1, and the task goes inert.

### 6.8 Capability matrix and ladder

| Feature | Requires | Degrades without |
|---|---|---|
| Any AI | `session`, `world`, `player`, and the gate true (§5) | — |
| Named chat | + `flags`, `ui`, `place`, `params` | No `text` → persona fallback names. No `combat` → stricter gating (no hostile ≤ 25 m at all). No `camera` → no cone targeting; lock-on and nearest remain. **No `flags` → chat off, barks on** (DESIGN §4.4) |
| Barks | + `combat` | No `camera` → bottom feed instead of floating labels. No `text` → card `label_fallback` |
| Status awareness, runtime `conscious` | + `status` | Off: no status words, no status lines; generics' calm lines held until S8 verifies the sleep signal (O44) |
| Own-initiative persona lines | Named chat + S3 passed | Before S3: none (DESIGN §8.9) |
| Call-out | Barks + `ui` | — |
| Persistence | `player` | — |
| Recording, debug view | `world` | — |

```mermaid
stateDiagram-v2
  [*] --> Off: enabled = false at boot
  [*] --> L1: unknown build, anti-cheat, readiness timeout, discovery failed
  [*] --> L2: task ticking and phase-A structural tests pass
  L2 --> L4: provider healthy, AI not paused, all groups OK
  L2 --> L3: provider healthy, an optional group tripped
  L4 --> L3: optional group trips
  L4 --> L2: no healthy provider, or AI paused
  L3 --> L2: no healthy provider, or AI paused
  L2 --> L1: structural group trips, or task tripped
  L3 --> L1: structural group trips, or task tripped
  L4 --> L1: structural group trips, or task tripped
```

- The level is recomputed by the core on every step from `ReadHealth`, provider health and settings.
- **Dormant** is an orthogonal flag over L2–L4. It is set when the gate is false, while loading and at the title. While it is set, speech is dropped and the task reads only `session` and the lifecycle subset (DESIGN §4.4).
- Unknown builds always stop at L1, because probation mode is deferred (O12).

### 6.9 Text, param and flag formats

**FMG table walk** (`text`; S2 confirms the indexing).
- `tables = [MsgRepository + tables_off]` and `count = [MsgRepository + count_off]`, with leads `+0x08` and `+0x14`.
- Group 0 is used, and `fmg = tables[slot]` (layout `fmg.indexing = "flat"`).
- **FMG header** (the public file format): `group_count`, `string_count`, `groups_off`, `offsets_off`.
- **Group:** `{offset_index: i32, first_id: i32, last_id: i32}` at `fmg.group_stride`.
- **Lookup:** binary-search the groups for `first_id ≤ id ≤ last_id`, then `index = offset_index + (id − first_id)` and `str = string_offsets[index]` (an `i64`; 0 means no entry). Copy ≤ 512 bytes and stop at the UTF-16 NUL.
- **DLC order** is {4xx, 3xx, base}: NpcName {428, 328, 18}, PlaceName {429, 329, 19} (DESIGN §4.5).
- **Cache:** results are cached per `(fmg, id)` until the `tables` pointer changes (impl cap 4,096 entries). There are ≤ 16 new look-ups per tick.

**Param walk** (`params`).
- `SoloParamRepository` slot `param.<Name>.slot` → resource → `PARAM` header.
- Row descriptors are sorted by `id` and binary-searched.
- The row is copied with `row_size`. A row size that disagrees with the paramdef size fails the group.
- There are ≤ 32 new rows per tick (DESIGN §4.5). The grace and place-name export at Ready runs over several ticks at ≤ 1 ms each (DESIGN §2.5).

**Flag tree walk** (`flags`; ER01 §9).
1. Sort the watch list by `id / 1000`.
2. For each distinct group, descend the descriptor map: a balanced BST with a sentinel head node, ≤ 64 levels (impl), key `u32`.
3. A descriptor of mode 1 gives `block = holder_base + value`, and mode 2 gives `block = value`. Copy the 125-byte block once.
4. For each ID: `byte = (id % 1000) / 8` and `bit = 7 − id % 8`.
5. A missing group sets the ID's `valid` bit to 0, and the ID reads false (DESIGN §4.5).

The cost is one descent and one 125-byte copy per group, not per flag.

### 6.10 Patch-absorption runbook (budget 1–7 days; DESIGN §4.6)

```mermaid
flowchart LR
  A["Patch day: players see the L1 banner (C2)"] --> B["eldennet-engine probe-exe new.exe"]
  B --> C{"patterns and singletons resolve offline?"}
  C -->|"yes"| D["dev build with EN_PROBE in game:<br/>invariant search → draft layout"]
  C -->|"no"| X["re-derive patterns (own derivation),<br/>update patterns.toml"] --> D
  D --> E["self-tests + smoke route + golden compare"]
  E -->|"pass, no code change"| F["data-only release of gal/"]
  E -->|"code change needed"| G["new DLL release"]
```

| Step | Who and what | Output |
|---|---|---|
| 1 | Automatic. The new build's fingerprint is not in `builds.toml`, so the game shows the L1 banner. Pin an issue "Build 2.x.y.z: unsupported, in progress" | — |
| 2 | `eldennet-engine probe-exe <eldenring.exe> --against gal/` checks the file offline: PE fingerprint, every pattern (match count, function bounds, capture target section), the FD4 idiom hits by name, and the RTTI classes | `probe-exe.json` with a diff against the previous build |
| 3 | A dev build with the `probe` option (`EN_PROBE`) runs in game. It resolves discovery, then searches for each layout field by invariant: RTTI vtables at object starts; `hp ≤ max_hp` pairs; the ascending float vector; the player's known UTF-16 name; known flag 100 set; the divisor value 1000. It writes `layouts/<new>.draft.toml` and the probe record | Draft layout plus a JSON record (offset, invariant used, sample values) |
| 4 | Run the self-tests on the draft, then the recorded smoke route (Church of Elleh → Gatefront → camp fight → Kalé → Roundtable). Replay it and compare its events with the golden files (§8.12) | Pass or fail report |
| 5 | Promote the fields from `lead` to `confirmed`, add the build to `builds.toml`, and keep every earlier profile (N−1 at least, all supported builds by default) | `gal/` changes |
| 6 | If no code changed, ship a data-only release: a signed zip of `gal/` that replaces `<install>/gal/`. Otherwise ship a new DLL | Release |

---

## 7. `WorldSnapshot`

### 7.1 Schema (units and sources)

Every field is fixed-capacity and heap-free. The in-memory size is about 8 KB, most of it the text slots; a serialised
snapshot is typically 3–4 KB (DESIGN §5.2). An empty `std::optional` means "not read this tick, or its group is off".

```cpp
// The parts first, then the snapshot. FixedVector<T, N> and FixedString<N> are our own (§2.3): nothing here allocates.
struct Meter { std::uint32_t cur, max; };                                       // (cur, max), from the data module
struct Session { MultiplayRole role; /* PGD.multiplay_role */ bool is_my_world; /* PGD */
  LobbyState lobby; /* CSSessionManager */
  std::uint8_t player_set_len; /* WorldChrMan.player_chr_set: non-null entries whose chr_type is not an NPC type
                                  (DESIGN §3.4; S1 lists the values) */
  bool anticheat, ersc_loaded; /* C14 */ };
struct Life { bool main_player; /* WorldChrMan */ Screen screen; /* CSMenuMan: InGame|Loading|Title */
  std::optional<std::uint32_t> load_counter; /* CSNowLoadingHelper */ };
struct Ui { HudState hud; /* CSFeMan 0–3 */ std::optional<std::uint16_t> banner; /* CSFeMan, empty if 0 or -1 */
  std::optional<std::uint32_t> popup_talk_id; /* CSMenuMan popup, empty if < 0 */
  bool talk_cam; /* GameMan id != -1 */ bool subtitles_on; /* GameDataMan settings */ };
struct Player {
  ChrHandle handle; std::uint16_t level; std::uint8_t archetype; BodyType body;                 // PGD
  Meter hp, fp, stamina;                                                                         // data module
  std::uint32_t runes_held; /* PGD */ std::uint32_t deaths; /* GDM */ std::uint64_t play_time_ms; /* GDM, widened */
  std::uint8_t ng_lvl; /* GDM */ std::uint8_t save_slot; /* GameMan */ bool dead; /* dead bit */
  bool riding; /* ride_state == 5 */ std::optional<ChrHandle> locked_on, last_hit_by;
  std::uint8_t throw_state; /* throw module */ bool roundtable; /* SpEffect 9621 in the list */
  StatusSet statuses; /* bitset of DESIGN §9.9, from the gauges; empty until S8 */
  BlockId block; /* mAA_BB_CC_DD packed */ std::array<float, 3> pos; /* Havok, m */
  Gear gear; /* 6 ItemIds; names come via ResolveText */ bool boss_flag; /* GDM.boss_fight_active */ };
struct Place { std::optional<std::uint32_t> place_id; /* FieldArea, empty if <= 0 */
  RegionKey region; /* from BlockId */ bool legacy; /* area < 30 */ bool overworld; /* area 60 or 61 */ };
struct Clock { std::uint8_t h, m; };                                                            // WorldAreaTime
struct BossBar { ChrHandle handle; std::int32_t name_id, damage_taken; };                      // CSFeMan
struct Boss { bool active; /* bars non-empty OR player.boss_flag */ FixedVector<BossBar, 3> bars; };
struct Chr { ChrHandle handle; std::uint32_t param, model, entity; std::uint8_t kind; /* chr_type */
  std::uint8_t team; /* live team */ Meter hp; bool dead; std::optional<ChrHandle> last_hit_by;
  std::int32_t recv_dmg_type; std::uint8_t throw_state; float dist_m; /* |pos - player.pos|, computed by us */
  float bearing_deg; /* vs camera forward; NaN without camera */ bool on_screen;
  StatusSet statuses; /* bitset of DESIGN §9.9 */ std::optional<bool> asleep; /* empty until S8 */
  BlockId block; std::array<float, 3> pos; };
struct FlagFrame { std::uint32_t watch_version; std::bitset<2048> bits, valid; };
struct TextEntry { std::uint16_t fmg; std::uint32_t id; std::optional<FixedString<256>> text; };  // sanitised UTF-8
struct ParamRowInfo { ParamKind param; std::uint32_t id;
  FixedVector<std::pair<std::uint8_t, std::int64_t>, 8> fields; };

struct WorldSnapshot {                          // DESIGN §5.2 carries the same fields since the review
  std::uint64_t seq;                            // +1 per snapshot built by the task
  std::uint64_t mono_ms;                        // QPC at the start of the build
  bool gap;                                     // an unread snapshot was overwritten, or the budget was hit (§6.5)
  BuildKey build;                               // index into builds.toml
  ReadHealth health;                            // per group: Untested|Ok|Tripped, faults this tick, task_us last/p99
  Session session;
  Life life;
  Ui ui;
  std::optional<Player> player;                 // empty when main_player is null
  std::optional<Place> place;
  std::optional<Clock> clock;
  Boss boss;
  FixedVector<Chr, 32> chars;                   // nearest first
  std::optional<FlagFrame> flags;               // every 4th snapshot
  FixedVector<TextEntry, 16> new_text;          // answers to ResolveText
  FixedVector<ParamRowInfo, 32> new_params;     // answers to ResolveParams and the Ready export
};
```

- **Distances are our own.** `dist_m` is computed from Havok positions, so it does not matter whether the game's vector stores squared distances (S1, ER01 §5.3). The game's vector is used only for the order and the cut-off.
- **The player's name** is read once per playthrough and sent only in `Lifecycle::PlaythroughReady` (DESIGN §5.2). It is not a snapshot field.
- **Torrent and spirit summons** appear with their `kind`, so the core can perceive them and never voice them.

### 7.2 Rates, caps and budgets

| Item | Rate | Cap | Game-thread budget |
|---|---|---|---|
| Snapshot | Every 250 ms of accumulated mono time | Walk ≤ 48 vector entries; keep ≤ 32 within 40 m, nearest first (DESIGN §5.1) | p99 ≤ 0.4 ms |
| Flag frame | Every 4th snapshot (1 Hz; 0.5 Hz while throttled) | ≤ 2,048 IDs (v1 ≈ 740, including the world-awareness aliases of MEM §12.12) | ≤ 0.1 ms |
| Render feed | Every frame while `SetTracked` is non-empty | Camera + ≤ 8 handles | ≤ 0.03 ms |
| Text, params | On demand | 16 FMG + 32 rows per tick | Included |
| Command drain | Every frame | ≤ 16 commands per frame (impl) | ≤ 0.02 ms |
| Whole task | — | — | Mean ≤ 0.15 ms, p99 ≤ 0.5 ms at 32 characters (C3) |
| Dormant | 4 Hz | `session` + lifecycle subset only | ≤ 0.05 ms (impl) |
| Loading | 4 Hz | Lifecycle subset only (R11) | ≤ 0.02 ms (impl) |

### 7.3 Cadence and self-throttle (R10)

```text
per task invocation:
  dt  = min(qpc_now - qpc_last, 1000 ms); acc += dt          # a hitch can never cause a burst
  drain ≤ 16 commands; if tracked non-empty: render_feed()
  if acc >= period:                                           # period = 250 ms, or 500 ms while throttled
      acc = min(acc - period, period)                         # at most one snapshot per invocation
      build snapshot; every 4th: flag frame; text and params within caps
      record the build time in the histogram (C13)
```

**Throttle.** After 3 consecutive snapshot builds over 0.5 ms, the period becomes 500 ms (2 Hz), and the cap becomes 16 characters within 25 m. The normal rate returns after 60 s in which every build is ≤ 0.5 ms. The F11 view shows the state. Frame counts are never used (J2 G21).

### 7.4 Render feed

```cpp
struct Head { ChrHandle handle; std::array<float, 3> pos; bool present; };
struct RenderFeed {
  std::uint64_t seq, qpc_us;                    // stamped on the game thread
  std::array<float, 16> view; float fov_rad, aspect; bool camera_ok;
  FixedVector<Head, 8> heads;                   // label anchor: position + label_height_m (NPC §10.3; default 2.0 m)
};
```

- The overlay projects each head with the swap chain's viewport size, which the overlay knows.
- It drops any label whose deadline has passed in its own clock (`qpc_us` and its own QPC), and any whose handle is no longer `present`. Deadlines are checked at draw time (DESIGN §9.4).

### 7.5 Recording format (`*.ensnap.zst`)

- **Container:** zstd-compressed JSON Lines, written by a debug-only recorder in the effect shell (`debug.record_sessions`, off by default). Recordings stay on the player's machine and are never uploaded.
- **First line:** a header.

  ```json
  {"ensnap":1,"en_version":"0.1.0","build":"2.7.2.0-ww","content_hash":"b3:…","rules_hash":"b3:…",
   "seed_root":"…","salt":"…","created_play_ms":0}
  ```

- **Then one record per line:** `{"t":<mono_ms>,"k":"snap"|"life"|"ui"|"llm"|"db"|"cmd"|"reload"|"tick","b":{…}}`. Each
  record is the JSON form (nlohmann/json, §2.2) of the message that crossed a boundary. A recording is therefore the
  input list of `Core::step`, plus the commands for checking.
- **Privacy:** the player's name and SteamID are replaced by keyed blake3 hashes using the per-file `salt`. Typed text is kept, because replay needs it. The bug-report bundle excludes recordings unless the player ticks a box.
- **The player's profiles:** a `reload` record carries the text of the player's profile files (replay needs it), like
  typed text; editor keystrokes never reach the core, only the saved file. A `ui` record keeps a `ProfileWrite` (and any
  other UI message carrying a profile document) as `{target, rev, bytes}` only: the core only forwards it, and replay
  takes the text from the `reload` record. A draft's notes (its `ui` record) and reply (its `llm` records) are kept for
  replay like typed text. With `debug.record_redact_text` on, and in any recording the bug-report bundle includes, each
  player-profile text field becomes `profile:<slot>:<field>:<chars>`, and a draft's notes and reply become
  `profile:draft:notes:<chars>` and `profile:draft:reply:<chars>`; replay renders the placeholders.
  `eldennet-engine lint` rejects a committed recording that holds player-profile or draft text (impl).
- **Game text (licence):** `snap` records carry `new_text`, which is runtime FromSoftware text (character, place and
  gear names). A recording meant for the repository is written with `debug.record_redact_text = true` (forced on for
  `tests/recordings/`): each `TextEntry.text` becomes the placeholder `fmg<fmg>:<id>`, and replay resolves placeholders
  from `tests/fixtures/names.toml`, a table of our own invented names. `eldennet-engine lint` rejects a committed
  recording with any non-placeholder `text` (impl).
- **Size:** about 4 MB per hour (DESIGN §5.2). The recorder rotates files at 256 MB (impl).

---

## 8. Augur, the event engine

Augur turns snapshot diffs, flag edges, lifecycle changes, pipeline outputs and time into typed events with frozen audiences. It is pure: it does no I/O and reads no clock, and it runs first in every `Core::step` (DESIGN §6).

### 8.1 Module surface

```cpp
// What en-core supplies, so en-augur stays below en-core in the graph and game-neutral.
class AugurCtx {
 public:
  // Persona{id, stage_muted, chat}, Card{id, variant}, Mute, Player
  virtual Subject classify(const Chr& chr) const = 0;
  virtual bool eval(PredicateId pred, const EvalScope& at) const = 0;  // compiled `requires` (DESIGN §10.4)
  virtual bool persona_may_perceive(PersonaId id) const = 0;           // alive and not muted at the current stage
  // deterministic: blake3(seed_root ‖ ev ‖ salt)
  virtual std::uint64_t rng(EventId ev, std::uint32_t salt) const = 0;
 protected:
  ~AugurCtx() = default;
};

class Augur {
 public:
  void step(const CoreInput& input, Mono now, const AugurCtx& ctx, EventBatch& out);
  void resolve_audience(Event& ev, const AugurCtx& ctx) const;   // for events subscribers emit
  const KStore& kstore() const;                                  // "just now" lines (≤ 3, audience-filtered)
 private:
  SignalRegistry registry_; std::shared_ptr<const RuleSet> rules_; WorldModel world_; KStore kstore_;
  CooldownTable cooldowns_; DedupTable dedup_; std::vector<std::unique_ptr<EventSource>> sources_;
};
```

### 8.2 Sources

```cpp
class EventSource {                                 // DESIGN §6.2, unchanged
 public:
  virtual ~EventSource() = default;
  virtual void poll(const CoreInput& input, const WorldModel& world, Observations& out) = 0;
};
struct Observations {
  std::vector<Sample> samples;   // (subject, signal, value, t_mono) for this step
  std::vector<Fact> facts;       // discrete: FlagEdge{alias, value, verified}, Lifecycle(..), Speech(..), TimerFire{..}
};
```

| Source | Reads | Produces |
|---|---|---|
| `SnapshotDiff` | The new snapshot against the **last processed** one, which the world model keeps, so a dropped snapshot never fakes an edge | Samples for every registered snapshot signal. The world model also updates trackers (dropped after 2 s absent, impl), engagement and squads. It also yields the samples behind `CameNear` pairs, `FoeKilled` and status edges (after S8) |
| `FlagWatch` | `FlagFrame` against the catalogue's watch list version | `FlagEdge` facts. Frames whose `watch_version` differs from the core's current version are ignored |
| `Lifecycle` | `CoreInput`'s `Lifecycle` and `ReadHealth` | `Lifecycle(..)` facts, `GroupTripped`, `GateChanged` |
| `Pipeline` | `CoreInput`'s `UiEvent` (`Submit`, `CloseChat`) and events subscribers emit during the step | `Speech(..)`, `Conversation(..)` facts |
| `Timer` | `now` crossing each whole second | `TimerFire` facts: the `IdleNear` checks (per actor, phase-staggered by `hash(handle) mod 30 s`), engagement timeouts, the gate's 5 s window, K-store expiry, the deferred `BossFightEnded` outcome; `AutonomyTick` per NPC in range (phase `hash(handle) mod autonomy.period_s`) |
| H1–H3 | Reserved, behind the `hooks` option (§8.11) | — |

### 8.3 Signal registry (code-defined; adding one takes code)

| Signal | Type | Subjects | Derivation |
|---|---|---|---|
| `hp_frac` | f32 0..1 | chr, player | `hp.cur / hp.max`. Not sampled when `hp.max == 0` |
| `hp_delta` | f32 | chr, player | (prev hp − hp) / max, over the last processed snapshot |
| `dead` | bool | chr, player | Dead bit |
| `team_hostile` | bool | chr | Live `team` ∈ the `teams.toml` hostile set {6, 7, 27, 29}, and not a persona with clear hostility bits (DESIGN §5.3) |
| `team_turned` | bool | chr | Live `team` ≠ the `team` of its `NpcParam` row (`Chr.param`, resolved through `ResolveParams`): a charmed foe or a summon fighting on a side not its own (§8.7). To verify in game (S7); never sampled until then |
| `dist_m` | f32 | chr | `Chr.dist_m` |
| `on_screen` | bool | chr | `Chr.on_screen` |
| `locked_by_player` | bool | chr | `player.locked_on == handle` |
| `hit_by_player` | bool | chr | `last_hit_by == player` and `hp_delta > 0` |
| `hit_player` | bool | chr | `player.last_hit_by == handle` and the player's `hp_delta > 0` |
| `engaged` | enum + f32 | chr | Engagement state and confidence (§8.7) |
| `throw` | u8 | chr, player | Throw state |
| `riding` | bool | player | `player.riding` |
| `crouching` | bool | player | A crouch state, from a source verified in game; none is known yet, so it is never sampled (§8.7) |
| `invisible` | bool | player | An invisibility effect in the player's SpEffect list (the list walked for 9621), once its id is verified in game; never sampled until then (§8.7) |
| `roundtable` | bool | player | `player.roundtable` |
| `place_id` | u32? | player | `place.place_id` |
| `block_area` | u8 | player | Area byte of `player.block` |
| `clock_bucket` | enum | world | Seven time-of-day words (DESIGN §5.5) |
| `flag:<alias>` | bool + verified | world, persona | Catalogue flag; `flag:<persona>.<state>` for persona blocks, e.g. `flag:kale.howl_told` (MEM §6.1 naming) |
| `banner` | u16? | world | `ui.banner` |
| `hud` | enum | world | `ui.hud` |
| `talk_popup`, `talk_cam` | bool | world | `ui.popup_talk_id.is_some()`, `ui.talk_cam` |
| `boss_active` | bool | world | `boss.active` |
| `deaths` | u32 | player | `player.deaths` |
| `ng_lvl` | u8 | player | `player.ng_lvl` |
| `in_combat` | bool | player | §8.7 |
| `idle_due` | bool | chr | Set by the Timer at the actor's 30 s phase |

World stages and tracks are not signals in v1: `world()` and `knows()` are refused in Augur and bark `requires` (MEM §12.10, W1). They are derived in bus step 1 (§8.9) and read by the conversation, memory and debug-view subscribers. The reaction cue that lets a persona raise news unasked (O28, MEM §12.13) is not an Augur rule either: the conversation subscriber computes it from what Augur, the flag watcher, the world view and the journal have already recorded.

### 8.4 Detector semantics

For one `(rule, subject)`, let `x₀, x₁, …` be the signal's values at the processed snapshots `t₀, t₁, …` in which the subject is present. A detector is a small state machine over these samples. **No detector fires on a subject's first sample.**

| Detector | State | Fires at `tₖ` when | Re-arm |
|---|---|---|---|
| `rising_edge` / `falling_edge` | — | `xₖ₋₁ = false ∧ xₖ = true` (or the reverse) | Next opposite edge |
| `changed_stable{stable_ms}` | `pending (v, since)`, `last` | `xₖ = v ≠ last` and `v` held for every sample in `[since, tₖ]` with `tₖ − since ≥ stable_ms`; sets `last = v` | Any change restarts `pending` |
| `crosses_below{value, rearm_above}` | `armed`; initially `x₀ ≥ value` | `armed ∧ xₖ < value`; then `armed = false` | `xⱼ > rearm_above` sets `armed` |
| `crosses_above{value, rearm_below}` | Mirror image | `armed ∧ xₖ > value` | `xⱼ < rearm_below` |
| `drop_over_window{fraction, window_ms}` | Samples in the window, base time `b` | `max{xⱼ : tⱼ ∈ [max(tₖ − window, b), tₖ]} − xₖ ≥ fraction`; then `b = tₖ` | Implicit: only samples after the fire count |
| `rise_over_window{fraction, window_ms}` | Mirror image | `xₖ − min{…} ≥ fraction` | As above |
| `enter_radius{m, dwell_ms, rearm_m}` | `armed`, `inside_since` | `armed ∧ dist ≤ m` on every sample of the last `dwell_ms` | `dist > rearm_m`, subject lost, or loading |
| `count_increment` | `last` | `xₖ > xₖ₋₁`; payload `count = xₖ − xₖ₋₁` | — |
| `engagement` | Built-in machine (§8.7) | Its own transitions | Its own |

**Modifiers, applied in this order after a detector fires:**
1. **`debounce_ms`:** the fire becomes a *candidate* at `t_c`. It is emitted at the first `tₖ` with `tₖ − t_c ≥ debounce_ms` if the detector's condition (the post-edge value, below the threshold, inside the radius) held on every sample in between. Otherwise the candidate is cancelled, and the arm state is not consumed. With `debounce_ms = 0` the fire is emitted at `t_c`.
2. **`requires`:** the content predicate is evaluated at emission. If it is false, the fire is suppressed and the edge is consumed.
3. **`cooldown_s`:** per `(rule, subject)`, in mono time. A fire inside the cooldown is suppressed, and its edge is consumed; nothing is deferred.
4. **`class` handling:**
   - **P, `dedup_s` > 0:** suppress the event if the store's last P event with the same `key` has `t_play` within `dedup_s × 1000` ms.
   - **K, `merge_ms` > 0:** if the K store holds the same `key` with `t_first ≥ now − merge_ms`, *coalesce*: `count += 1`, payload aggregates per kind (sums, maxima, last attacker), and `expires = now + ttl`. A new event is emitted only if a *salient* payload field changes; for `Struck` that is `hard` turning false → true. The new event is marked `merged`. Otherwise a new entry is created and emitted. **Merging never delays the first event.**
   - **K, TTL:** an entry is removed silently when `now ≥ expires`.
   - **T:** the event is dispatched and never stored.
5. **Confidence:** the rule's `confidence`, or the branch value for `engagement`, or `1.0` for a verified catalogue flag and `0.8` for an unverified one (DESIGN §6.5).

### 8.5 Rule files

Rule files live in `content/eldenring/augur/*.toml`, with the schema of DESIGN §6.3. Two original examples:

```toml
[[rule]]
id = "struck"
emits = "Struck"
subjects = { kinds = ["generic", "persona", "player"], alive = true }
signal = "hp_frac"
detect = { kind = "drop_over_window", fraction = 0.001, window_ms = 300 }   # fires on each processed HP drop (base resets on fire)
payload = "struck"            # code-registered builder: att = last_hit_by; hard if ≥ 0.30 of max within 3 s; unseen (§8.7)
class = "K"
key = "hit:{att}:{subject}"
ttl_s = 10
merge_ms = 2500
confidence = 0.9
audience = { radius = "witness" }
priority = 20

[[rule]]
id = "boss_ended"
emits = "BossFightEnded"
subjects = { kinds = ["world"] }
signal = "boss_active"
detect = { kind = "falling_edge" }
debounce_ms = 1000
payload = "boss_outcome"      # Victory on banner 1–4 or 22, or a boss flag rising; Death on PlayerDied; else Left after 10 s
class = "P"
key = "boss:{fight_id}"       # fight_id = the EventId of the matching BossFightStarted
confidence = 1.0
audience = { radius = "death" }
priority = 5
```

`payload` names a builder registered in code (impl). The `boss_outcome` builder may defer emission by up to 10 s: Victory and Death emit as soon as their evidence is seen, and Left emits only when the 10 s window ends without either (DESIGN §6.5).

**Load-time validation** (no unknown keys; any error keeps the old rule set and shows a toast naming the file and line, DESIGN §11.4):
- `id` is unique; `emits` is in the `EventKind` catalogue; `signal` is in the registry; the detector is type-compatible (edges on bool signals, `changed_stable` on bool, enum or id signals, crossings and windows on numbers, `count_increment` on integers, `enter_radius` on `dist_m`).
- `class = K` needs `key` and `ttl_s ≥ 1`. `class = T` forbids `ttl_s`, `merge_ms` and `dedup_s`. `dedup_s` is allowed only on P.
- `key` placeholders exist in the payload; `audience.radius` ∈ {`speech`, `witness`, `death`, `combat`, `none`}; `interrupts` are known names (§8.8).
- `requires` compiles and names only catalogue aliases; `priority` is in 0–100 (lower runs first).
- Bounds: `debounce_ms ≤ 60,000`, `merge_ms ≤ 30,000`, `ttl_s ≤ 600`, `cooldown_s ≤ 3,600`, `dedup_s ≤ 86,400`; `confidence` in (0, 1].
- Each file is ≤ 64 KB; the total is ≤ 256 rules (impl).
- User rule files load only when `augur.allow_custom_rules = true`, which is off in v1 (DESIGN §6.3).

### 8.6 Event catalogue (every v1 event of DESIGN §6.5)

Since O45, every `PersonaId` in this catalogue reads *profile id* (a persona or a verified character binding, DESIGN §7.8), except in `StageChanged`, which only a stage track emits.

```cpp
// Each kind is a struct of its own, and EventKind is the std::variant of them all, in this order.
// world (P unless noted)
// PlaceEntered: changed_stable 2 s; dedup 10 min per place. GraceRested / RestEnded: flag 9000 rising (P, dedup
// 10 min) / falling (T)
struct PlaceEntered { std::uint32_t place_id; std::optional<std::uint32_t> prev; };
struct GraceRested { std::optional<std::uint32_t> grace; }; struct RestEnded {};
struct GraceLit { std::uint32_t flag, grace; };                                  // BonfireWarpParam flag rising
struct FlagChanged { std::shared_ptr<const std::string> alias; bool value, verified; };
struct BossFightStarted { FixedVector<BossRef, 3> bars; };
struct BossFightEnded { BossOutcome outcome; /* Victory|Death|Left */ FixedVector<BossRef, 3> bars; };
// personas
struct PersonaSeen { PersonaId persona; };                 // ≤ 10 m for 2 s; re-arm > 30 m or after loading
struct PersonaApproached { PersonaId persona; };           // T: ≤ 6 m for 1 s; re-arm beyond 10 m
struct PersonaAttacked { PersonaId persona; AttackVia via; /* HpDrop|HostilityBit */ };   // conf 0.95
struct PersonaDied { PersonaId persona; bool by_player; };
struct FoughtBeside { PersonaId persona; std::int32_t boss; };                            // conf 0.9
// NativeTalkStarted: T + scheduler lock; P with a persona, dedup 5 min; conf 0.6 until S3
struct NativeTalkStarted { std::optional<PersonaId> persona; TalkSignals signals; };
struct NativeTalkEnded { std::optional<PersonaId> persona; std::uint32_t held_ms; };
struct StageChanged { PersonaId persona; StageId from, to; };                             // P, the persona only
// player
struct PlayerDied { std::optional<ActorRef> killer; bool in_boss; };  // dead rising | deaths +1 | banner 5; dedup 5 s
struct MountChanged { bool riding; };                                                     // K 30 s
// combat (K)
struct Struck { std::optional<ActorRef> att; ActorRef vic; bool hard, unseen; std::uint16_t count; float frac_total;
  std::int32_t dmg_type; };                                                               // 10 s; unseen: §8.7
struct LowHp { ActorRef subject; float frac; };                                           // < 0.25, re-arm > 0.35; 20 s
struct Healed { ActorRef subject; float frac; };  // T: any combatant; +≥ 20% of max within 1 s, in combat; conf 0.8
struct Engaged { ActorRef subject, opponent; EngageVia via; SquadId squad; };             // key eng:{a}:{b}; 30 s
struct Disengaged { ActorRef subject, opponent; bool fled; DisengageWhy why; };
struct AllyDied { ActorRef victim; SideId side; std::vector<ChrHandle> witnesses; };      // 10 s
struct CritLanded { ActorRef victim; };            // player throw 3 while the victim's throw is 4; 10 s
struct KilledNear { CardId victim_card; std::vector<PersonaId> witnesses; };              // 120 s
struct IdleNear { ActorRef subject; };                                                    // T
// FoeKilled: K 10 s, an NPC kills an NPC; emitted once S7 verifies last_hit_by for NPC attackers
struct FoeKilled { ActorRef victim, by; };
// Status*: K; after S8
struct StatusStarted { ActorRef subject; Status status; }; struct StatusEnded { ActorRef subject; Status status; };
struct StatusBurst { ActorRef subject; Status status; };
// proximity and timers (T)
// CameNear: within range.talk_m for 1 s; re-arm beyond + 4 m; the Tarnished only while not concealed from subject
// (§8.7)
struct CameNear { ActorRef subject, other; };
struct AutonomyTick { ActorRef subject; };
// pipeline
struct PlayerSaid { std::optional<ActorRef> to; SafeText text; std::optional<ConvId> conv; InputSource input; };
// NpcSaid: P with a profiled speaker (a persona or a character, O45), K (6 s) with a card
struct NpcSaid { ActorRef speaker; std::optional<ActorRef> to; SafeText text; std::optional<ConvId> conv;
  std::uint8_t pages; Motive motive; std::optional<ItemId> cue; };
// Callout*: T, nothing is remembered
struct CalloutSent { ActorRef target; }; struct CalloutReplied { ActorRef target; bool from_seed; };
struct ConversationStarted { PersonaId persona; ConvId conv; Opener opened_by; /* Player | Npc(LineId) */ };  // T
// ConversationEnded's known: ≤ 24 (item, certainty) of the known set at the end; paths are not stored (MEM §12.7);
// empty without a flag frame
struct ConversationEnded { PersonaId persona; ConvId conv; EndReason reason; std::uint16_t player_lines, committed;
  std::optional<std::vector<std::pair<ItemId, Certainty>>> known; };
// lifecycle (P for PlaythroughReady and journey changes, T otherwise)
struct DataReady {}; struct PlaythroughReady { PlaythroughKey key; }; struct LoadingStarted {}; struct LoadingEnded {};
struct TitleReached {}; struct JourneyChanged { std::uint8_t ng_lvl; }; struct GateChanged { bool active; };
struct GroupTripped { Group group; }; struct LadderChanged { Level to; };
// scripted lines (T). NativeLineHeard: T + lock; live per kind S3 verifies; talk_id after S10
struct NativeLineHeard { std::optional<ActorRef> speaker; LinePhase phase; /* Started | Ended */
  std::optional<std::uint32_t> talk_id; };
// reserved seams (never emitted in v1)
struct NpcThought {}; struct DecisionMade {};
```

`ActorRef` is `Player | Persona{id, handle} | Card{id, variant, handle} | Mute{handle}`. `PlaceRef` is `{block, place_id, region}` (DESIGN §6.4). `Motive` is the reaction type a line answers to (NPC §9.1), or `Reply`; `Opener` is `Player | Npc(LineId)`; `LinePhase` is `Started | Ended`; `Status` is one of DESIGN §9.9's seven, and `StatusSet` their bitset; `SideId` is the Tarnished's side, a card or a live team. Which reaction types each event drives is DESIGN §6.5 and NPC §9.1.

### 8.7 Engagement, squads and combat state

```mermaid
stateDiagram-v2
  [*] --> Idle
  Idle --> Engaged: start condition (via, conf)
  Engaged --> Engaged: stronger condition → conf raised (re-emit if it crosses 0.6)
  Engaged --> Idle: 12 s with no exchange and no lock-on → Disengaged(why=Timeout)
  Engaged --> Idle: distance > 30 m → Disengaged(why=Distance)
  Engaged --> Idle: subject dead (no Disengaged; AllyDied, KilledNear or FoeKilled instead)
  Engaged --> Idle: rest or loading (cleared silently)
```

- **Applies to:** each pair of combatants on hostile sides: the Tarnished and a hostile NPC, or two NPCs on different sides that exchange hits (`last_hit_by` for NPC attackers, verified by S7), or that the team-pair table in `teams.toml` (verified by S7) marks hostile (DESIGN O42).
- **Start conditions, strongest first** (DESIGN §5.3):
  - `Hit`, conf 0.9: `hit_player ∨ hit_by_player`;
  - `LockClosing`, conf 0.6: `locked_by_player`, `dist ≤ 20 m`, and the distance shrank ≥ 3 m over 2 s;
  - `Near`, conf 0.6: `dist ≤ 8 m`;
  - `LockOnly`, conf 0.3: `locked_by_player`.
- **NPC pairs:** `Hit` (0.9) from a hit exchange either way, read from HP deltas and `last_hit_by`, once S7 verifies `last_hit_by` for NPC attackers (until then NPC pairs never engage, and a `Struck` on an NPC victim whose `last_hit_by` names another NPC carries `att: None`); `Near` (0.6, ≤ 8 m) once S7 verifies the team-pair table. Lock-on conditions are the Tarnished's only.
- **`fled`** is computed when the engagement ends: the player's distance grew ≥ 15 m within the last 5 s, and it is now > 25 m.
- **Squads:** when `e` becomes engaged, it joins an existing squad of the same side (the same card, or the same live team for non-card NPCs) that has a member engaged within the last 10 s and within 20 m of `e`. Otherwise it starts a new squad. A squad dissolves when its last member disengages or dies, and on rest or loading. The group gap (8 s, `pacing.group_gap_s`) lives in the scheduler (DESIGN §9.4).
- **Combat state:** `in_combat = boss.active ∨ now − last_signal < 8 s`. A signal is damage either way, `CritLanded`, or any engagement with conf ≥ 0.6, refreshed on every step it exists. Gating and barks use only conf ≥ 0.6 (DESIGN §5.3).
- **Stance towards each viewer** (WHY§97, WHY§98; DESIGN §6.5). For a viewer V and each other person X that a prompt or
  a speaker choice names, the world model gives X's stance towards V, the first that holds: `enemy` if X is engaged
  with V (≥ 0.6) or the two are on hostile sides (one `team_hostile` and the other on the Tarnished's side, or, between
  two NPCs, a pair the team-pair table marks hostile once S7 verifies it); `turned` if `team_turned` holds for X;
  `ally` if X is on V's side (the same card, the same live team, or both on the Tarnished's side); `bystander`
  otherwise. "Turned", a live team that differs from the row's team, is to verify in game (S7); until then no one
  reads as turned. The prompt gives the stance as a fact of the moment, so it outranks what earlier lines imply.
- **Unseen attacks.** A hit whose source resolves to no one within 20 m of the victim (its `last_hit_by` was read and
  names no tracked character, or one farther than 20 m) marks its `Struck` `unseen` (§8.6), and no prompt or memory
  then names an attacker for it. An attacker that cannot be read yet (an NPC's, before S7) is unknown, not unseen.
- **A concealed Tarnished.** The Tarnished is concealed from a viewer V while it crouches more than 5 m from V
  (impl), or while an invisibility effect holds (`crouching` and `invisible`, §8.3, each once verified in game; until
  then it is never concealed). A concealed Tarnished is not greeted and not addressed: `CameNear{V, Tarnished}` waits
  until it is no longer concealed from V, so standing up near V greets as arriving does, and the scheduler refuses, at
  request and at display, an own-initiative line from V addressed to the Tarnished (DESIGN §9.4). A conversation the
  Tarnished opens is untouched.

### 8.8 Audience and interrupts

**Audience** (resolved in step 4 of §8.9 and then frozen):
1. **Origin:** the subject's position; the speaker's position for speech; the player's position for player events.
2. **Radius by kind:** speech 16 m, witnessing 20 m, deaths 30 m, combat noise 40 m (DESIGN §5.4).
3. **Candidates:** profiled NPCs (personas and verified character bindings, O45; DESIGN §7.8) present in the current snapshot with `persona_may_perceive` (`persona_may_perceive` includes `conscious`, MEM §1.2).
4. **Distance:** Euclidean in Havok space, from the candidate to the origin.
5. **Same area:**
   - in the open world (area 60 or 61), the same area byte of `BlockId`;
   - indoors, the same full `BlockId`.
6. **How:**
   - `direct` if the persona is the addressee, subject or object;
   - `overheard` for speech not addressed to it;
   - `witnessed` otherwise.
7. **Cap:** 16 entries (impl), nearest first. Generics never get rows. There is no line-of-sight test in v1 (DESIGN §5.4).

One function, `visible_to(event, persona)`, serves prompts, the recap, memory jobs, facts and decision calls (§11.8), so
an event's audience bounds every reader: a speaker choice offers only candidates who perceived what they would answer
(WHY§99). Events a code plugin reports after v1.0 (O68) are bound the same way.

**Interrupt sets** are flag sets (`InterruptSet`, an `enum class` with bitwise operators) carried on each event
(DESIGN §6.5):

| Event | `interrupts` |
|---|---|
| `Engaged` with conf ≥ 0.6 within 25 m | `CHAT_INPUT \| AMBIENT \| AUTONOMOUS` |
| `PlayerDied` | `CONVERSATION \| OWN_LINES_EXCEPT_PLAYER_FELL` |
| `LoadingStarted` | `ALL` |
| `NativeTalkStarted` | `CONVERSATION_OUTPUT \| OWN_LINES_WITHIN_20M` |
| `NativeLineHeard{Started}` | `CONVERSATION_OUTPUT \| OWN_LINES_WITHIN_20M` |

### 8.9 Bus: order and backpressure

**Order inside one step** (DESIGN §6.6):
1. Update the world model from the input: trackers from the snapshot; `FlagState` from the newest processed flag frame; then the `WorldView` (MEM §12.3), pure and memoised per flag generation and content version, so every later step reads one `world_gen`.
2. Poll the sources in fixed order: Lifecycle, SnapshotDiff, FlagWatch, Pipeline, Timer.
3. Evaluate rules sorted by `(priority, subject distance, handle)`.
4. Resolve and freeze audiences.
5. Dispatch each event to **facts → log → conversation → barks → memory → UI/debug**. Each event goes to all six before the next event starts. The `barks` subscriber schedules every own-initiative line (DESIGN §9.4).

**The step FIFO.**
- Events that subscribers emit (through `BusCtx::emit`, which calls `resolve_audience`) join the tail of the same step's FIFO.
- The FIFO holds ≤ 256 events. On overflow, T events are dropped first, then K events, and the drop is logged. P events are never dropped.
- Every event from snapshot *n* is delivered before snapshot *n+1* is processed.
- **Exceptions:** each subscriber call is wrapped in a catch-all handler. The first exception resets that subsystem;
  the second disables it for the session (R8).

```cpp
class Subscriber {
 public:
  virtual ~Subscriber() = default;
  virtual SubsystemId id() const = 0;
  virtual void on_event(const Event& ev, BusCtx& ctx) = 0;   // may emit events and push Outputs
  virtual void on_step_end(Mono /*now*/, BusCtx& /*ctx*/) {}
};
```

**Backpressure** (DESIGN §6.6):

| Queue | Bound | Policy |
|---|---|---|
| Snapshot input | 2 slots | Latest wins; the bus itself has no queue |
| Step FIFO | 256 | Drop T, then K; never P |
| Store requests | 1,024 | Diagnostics dropped first. P events spill into a RAM ring of 4,096 (256 reserved for speech). When the non-speech part is full, persistence of non-speech P events pauses, with the toast "Memory is behind; recent events are not being saved", until the queue drains below 512 (impl). When the speech reserve is full, the chat stops showing new pages (they wait), so a shown line can never be lost (impl) |
| Net requests | 64 | Pool jobs dropped first; memory jobs requeue; replies never dropped (one dialogue request in flight; an own-initiative one yields to the player's `Submit`) |
| UI | 1 | Latest wins |

**Latency:** game change → scheduler ≤ 250 ms + ≤ 5 ms. Bark rules have no debounce, so p95 ≤ 300 ms (DESIGN §6.6).

### 8.10 Rates and costs

| Source or stage | Rate | Game thread | Core per run |
|---|---|---|---|
| SnapshotDiff + world model | 4 Hz | ≤ 0.4 ms p99 (the snapshot) | ≤ 1 ms |
| Rule evaluation | 4 Hz | — | ≤ 1 ms for ≈ 25 rules × 33 subjects (impl split of DESIGN's 2 ms) |
| FlagWatch | Every 4th snapshot: 1 Hz (0.5 Hz while throttled), ≈ 740 IDs | ≤ 0.1 ms | ≤ 0.2 ms |
| Timer, Pipeline, Lifecycle | 1 Hz / per event | — | Negligible |
| Dispatch | Per event | — | ≤ 0.1 ms per event, excluding prompt rendering, which is deferred to the effect shell's request build |
| Hook sources | Per call | Reserved | Reserved |

The world model keeps ≤ 256 trackers and the K store ≤ 512 entries (impl). Past the cap, the oldest trackers of absent handles and the earliest-expiring K entries go first.

### 8.11 Reserved hook sources H1–H3

These are compiled out in v1 (the `hooks` build option, `EN_HOOKS`). They implement the same `EventSource` interface and
emit into the same kinds, plus the reserved ones. Each **requires a code patch or a game-memory pointer write**, both
forbidden in v1 (R2, R12). Each therefore needs its spike and an owner decision before v1.x. Since GraceUI's G15 they
would use EldenNet's own hook library, never on a function GraceUI hooks (O65, R12; WHY§84, WHY§86).

| Source | Hook point (lead) | Would emit | Spike prerequisites |
|---|---|---|---|
| H1 `HookFlagSetter` | The flag-set routine (TGA AOB lead; ER01 §9) | `FlagChanged` with no polling delay, for any flag | The Arxan behaviour of an inline hook without neutralisation; coexistence with other mods that hook it; which threads call it (the hook may only push to a lock-free ring); ≤ 0.5 µs per call |
| H2 `HookDamage` | `CSChrDamageModule` vtable slot 7 / `HitChr` (ER05 §8.2) | Exact `Struck` with the amount and attack param, multi-hits included | `AttackDamageInfo` mapping; a vtable swap is a game write; ERSC conflicts; a burst rate of about 100 calls per second |
| H3 `HookTalkInvoke` | `CSEzStateTalkEvent::invoke` (ER05 §8.6, §10) | Exact `NativeTalkStarted/Ended`, and `NativeLineHeard` | S3 and S9 results; Reforged and ESD-mod coexistence; the same vtable-write question |

Polling stays as the cross-check when a hook source exists. Golden replays run with and without it.

### 8.12 Test harness

**Golden files** (`tests/golden/<route>.events.jsonl`):

```json
{"golden":1,"recording":"limgrave-camp.ensnap.zst","rules_hash":"b3:…","content_hash":"b3:…"}
{"seq":[412,0],"t_play":731204,"kind":"Engaged","class":"K","subject":"card:godrick_soldier#h3","object":null,
 "key":"eng:h3","conf":0.9,"audience":[],"count":1,"payload":{"via":"Hit","squad":1}}
```

- Handles are normalised to `h1, h2, …` in order of first appearance, and names and SteamID are never present.
- `eldennet-engine golden check` compares exactly, with our own diff. Replays are deterministic, so times match exactly
  too (C12).
- `eldennet-engine golden update` rewrites the files. CI fails if a golden file changes without a matching entry in
  `tests/golden/CHANGES.md`.

**Synthetic DSL** (`tests/scenarios/*.scn`; parsed by `en-tools`; original example):

```text
seed 7
at 0     spawn s1 card=godrick_soldier team=6 pos=12,0,0 hp=400/400
at 0     spawn kale persona=kale pos=-15,0,2
at 1000  move player to=6,0,0 over=1000
at 2200  hit s1 -> player dmg=80
at 2600  set_flag rest=false
at 9000  kill s1 by=player
expect   event Engaged subject=s1 via=Hit conf=0.9 within 2200..2500
expect   bark engaged speaker=s1 within 1000 of Engaged
expect   no bark class=combat after 2500 of Engaged until 9000
expect   event KilledNear witnesses=[kale] within 9000..9300
```

Verbs: `spawn`, `move`, `hit`, `kill`, `set_flag`, `say`, `wait`, plus `drop_snapshots n` for gap tests. Assertions: `event`, `bark`, `no`, `audience`, `count`.

**Property tests** (seeded property tests of our own on doctest, on `en-augur`: the seeds are fixed, and a failure
prints its seed):
- Hysteresis: never two fires without a re-arm crossing between them.
- Debounce: no emission before the condition has held for `debounce_ms`.
- Merge: same-key events within `merge_ms` give exactly one entry, with `count` equal to the number of inputs, and only salient changes re-emit.
- Gaps: dropping any subset of snapshots never creates an edge that the kept snapshots do not show, and an edge that spans a gap fires exactly once.
- Ordering: dispatch order equals the sort by `(priority, distance, handle)` within each source batch, then FIFO.
- Determinism: the same inputs give byte-identical events over 10 runs (C12).

**Backpressure tests:**
- A stalled store: no P event is lost; ring and toast behaviour as specified; the speech reserve holds.
- Snapshot loss of 50% at random: the golden events match the dense run, except for documented timing differences.
- Net saturation: pool jobs are dropped first, and dialogue survives.
- FIFO overflow: T is dropped before K, and P never.

---

## 9. Lifecycle state machine

DESIGN §2.5 draws Dormant as a state reached from InWorld. It is implemented as an **orthogonal gate region**, so loading and dormancy can overlap without extra states. The diagram below is the composite.

```mermaid
stateDiagram-v2
  [*] --> Gate
  Gate --> Banner: unknown build, anti-cheat, second copy, discovery failed
  Gate --> Ready: readiness, discovery, task registered, phase-A tests
  Ready --> Title: main_player false
  Ready --> Playthrough: main_player true (very late load)
  Title --> Playthrough: main_player, phase-B tests, slot/name/play time valid
  Playthrough --> InWorld: DbOpened and rollback resolved
  InWorld --> Loading: main_player false, load counter moved, or screen != InGame
  Loading --> InWorld: same playthrough key, screen InGame, counter stable
  Loading --> Playthrough: different playthrough key
  Loading --> Title: screen == Title
  state Gate_region {
    Active --> Dormant: clean false (same snapshot)
    Dormant --> Active: clean for 5 s
  }
```

| Transition | Detection (snapshot fields) | Core actions |
|---|---|---|
| → Ready | `Lifecycle::DataReady{caps}` from the task | Load content. Ask for the grace and place-name param export through `ResolveParams` batches (≤ 1 ms per tick). Open `pools.sqlite` and `diag.sqlite`. Run the silent self-test if the gate allows (§11.7) |
| Ready/Title → Playthrough | `life.main_player`; phase-B results in `health`; `player.save_slot` in 0–9; the name is valid (from `PlaythroughReady`); `play_time_ms` > 0 and rising over 2 snapshots | Compute the playthrough key `(SteamID, slot, fp8)` (DESIGN §10.5). Send `DbRequest::Open`. Writes are buffered until the rollback check resolves |
| Playthrough → InWorld | `DbResult::Opened{rollback}`: silent trim, modal, or clean | Close open conversations and queue their memory jobs. Load facts. Lift the write buffer |
| InWorld → Loading | `!life.main_player`, or `life.load_counter` changed, or `life.screen != InGame` | End the conversation (commit shown lines only, reason `travel`). Clear barks, K events, squads and engagements. `SetTracked{[]}`. The task now reads only the lifecycle subset (R11) |
| Loading → InWorld | `main_player`, `screen == InGame`, and the load counter the same on 2 consecutive snapshots | Re-arm `PersonaSeen` (DESIGN §6.5) |
| Loading → Playthrough | The new key ≠ the old key (slot or fingerprint changed) | Flush and close the old DB, then the Playthrough actions |
| → Title | `screen == Title` | Flush and close the playthrough DB. Journey state cleared |
| Player died (in InWorld) | `PlayerDied` (§8.6) | End the conversation (reason `death`). For 1.2 s only `player_fell` may fire. Persist `PlayerDied` (DESIGN §2.5) |
| Rest start / end | `GraceRested` / `RestEnded` (flag 9000; verified in S6) | Start: the context is safe; run queued memory jobs and pool refills. End: clear generic engagements, squads and cooldowns, because the world respawns |
| Journey change | `player.ng_lvl` changed | New journey partition; earlier journeys become read-only (DESIGN §10.5) |
| Active → Dormant | Gate `clean` false (§5.1) | §5.2 effects. The badge shows |
| Dormant → Active | `clean` for 5 s | Badge removed. Barks and chat become available on the next step |

The task sends `Lifecycle` messages for `DataReady`, the playthrough identity (`PlaythroughReady{slot, name, archetype, keepsake, play_time_ms}`, read once) and group trips. Everything else is derived by the core from snapshot fields, so a lost lifecycle message can always be rebuilt (C2).

---

## 10. Crash safety

### 10.1 Rules and their enforcement

| Rule (DESIGN §2.6) | Enforcement |
|---|---|
| R1 One reader | `GuardedReader` needs a `const GameThread&`; the token cannot be copied, and only `GameThread::issue` makes one (§6.4). CI grep: `GameThread::issue` only in `en-gal/src/task.cpp` and `en-tools/src/replay.cpp`. `en-overlay` cannot link `en-gal` (a CI check of CMake's target graph). A compile-fail test (CTest, `WILL_FAIL`) copies a token and makes one outside `issue`; debug and test builds check the reading thread against the issuing one |
| R2 No writes, no calls | `GuardedReader` has no write method. `SysLayer` exposes no call apart from `register_recurring` and the init-only getter fallback (`sys::fd4::name_via_getter`, which asserts the init phase atomic). CI grep in `en-gal`: no `volatile` store, `std::memcpy` or `std::memmove` into a game address, and no cast of an address to a function-pointer type (`reinterpret_cast`, `std::bit_cast`), outside `sys/register.cpp` and `sys/fd4.cpp`. Conformance test: with `actions` off, no non-read `GameCommand` is ever emitted |
| R3 Guard foreign entries | `ffi_guard(component, fallback, f)` = a `try` with a catch-all handler + a per-component `TRIPPED` atomic, inside an entry marked `noexcept`. CI grep: every `extern "C"`, `WINAPI` or `CALLBACK` function body in the project starts with `ffi_guard(`; `bugprone-exception-escape` checks that no exception leaves a `noexcept` entry (§2.4). CI reads the build's flags and fails unless C++ exceptions are on (`/EHsc`) |
| R4 Copies, never references | `copy<T>` for `Pod` types only; plausibility checks (§6.5). CI grep: no dereference of a game address, and no pointer or reference made from one, in `en-gal` outside `reader.cpp`. Forced-fault tests in S0-D and with the `probe` option |
| R5 Re-resolve every tick | The snapshot builder starts by reading the statics (§6.6). `FakeSys` test: statics move, are nulled and repointed between ticks |
| R6 Never block | No lock type in `en-gal` and `en-overlay` (`std::mutex`, `std::shared_mutex`, `std::condition_variable`, Win32's locks; §2.4). Debug and probe builds replace the global `operator new` and `operator delete` with counting ones that record allocations on the game thread after warm-up, and a test asserts zero |
| R7 Gate before layout | Type state: `BuildProfile` (and so any `Layout`) has a private constructor and exists only as the value of `gate()`'s `Expected`. Unit test |
| R8 Breakers | Unit tests of the 3-in-60-s window; S0-D in game; subsystem reset and disable tests in `en-core` |
| R9 Lints | `clang-tidy` with the checked-in configuration per module (§2.4), and warnings as errors, in CI on the Windows target |
| R10 Time-based cadence, self-throttle | Simulated frame times of 30, 60 and 144 FPS and 1 s hitches → always 4 Hz, never a burst. Throttle entry and exit tests |
| R11 Idle while loading | Replay of a recorded loading screen: read counters show the lifecycle subset only |
| R12 GraceUI's functions are GraceUI's; EldenNet's own hooks are few and approved (GraceUI G15; O65) | In v1 EldenNet links no hook library and installs no hook: the third-party check refuses MinHook, Detours and every other known hook library in EldenNet's project by name (§12.1), the `licence-files` stage fails on any MinHook source, and the release job reads `eldennet.dll`'s import table and symbols for a hook library (`MH_*`, a detours library) and fails on one (GraceUI invariant 16). Once a first hook of EldenNet's own is approved (O65), its one library is allowed in `en-gal`'s `hooks` alone, behind the `hooks` option: the check then names that library as the one exception, and a CI check compares every hook target in the source with GraceUI's published list (GraceUI D102) and fails on a match. The process's overlay and input hooks are GraceUI's, in `graceui.dll`'s `grace-host`, installed once (`std::call_once`) with no uninstall API: it never unhooks or unloads while the game runs (O25), and GraceUI's own checks ban MinHook's disable, remove and uninitialise calls (GraceUI §2.2). EldenNet's own hooks follow the same rules: installed once, after `connect` has answered, chained, never removed. S0-E checks GraceUI's in both load orders, on GraceUI's 0.1 Host |
| R13 Sanitised, capped strings | `UiModel` accepts only `SafeText`, whose constructor runs the sanitizer and the byte cap. Seeded property tests of our own (C7) |
| R14 Nothing at detach | CI grep: the `DLL_PROCESS_DETACH` path is empty, and clang's `-Wexit-time-destructors`, an error, keeps destructors out of detach (§1.7). One DB request = one transaction. Hard-kill tests during a reply |
| R15 Abort-class failures designed out | The caps in §10.2, each with a test. Destructors never throw (a throw out of one ends the process): `bugprone-exception-escape` checks every destructor in the project |
| R16 Single instance | §1.6; a double-load row in the compatibility matrix |

### 10.2 Abort-class caps (R15)

A catch-all handler does not stop a stack overflow, a C-level fault (SEH, which `/EHsc` never turns into a C++
exception), an exception leaving a destructor or another `noexcept` function (`std::terminate`) or, in practice,
running out of memory. The caps keep each of these out of reach.

| Resource | Cap (impl unless cited) |
|---|---|
| Template rendering | The template engine (en-prompt on inja, built, O84; NPC §7.1): nesting depth ≤ 16 (built); output ≤ 64 KB; a bound on render steps |
| JSON parsing (memory jobs, pools, SSE payloads) | Input ≤ 256 KB; nesting depth ≤ 32 (checked before nlohmann/json parses it; built: en-proto's `parse_json`, on every JSON en-llm parses). A model list, read as it parses and keeping only its ids, has its own bound: 1 MB from a local probe, 8 MB from a provider, and the same depth |
| SSE | Line ≤ 64 KB; total response ≤ 1 MB; a stream that passes 16 KB of display text is cancelled and ends as `Truncated` with the text that fitted (built) |
| Sanitizer and splitter | Input per reply ≤ 16 KB; iterative, never recursive; output 600 bytes per reply and 180 per page (DESIGN §8.4) |
| Predicate DSL | Expression ≤ 1 KB; parse depth ≤ 32 |
| Regex | RE2 (linear time), where a pattern comes from content, with `max_mem` at 2 MB for its compiled program and its DFA together; patterns ≤ 1,024 bytes; `std::regex` never on content or player text |
| Content files | 64 KB per file, 16 KB per persona, card or character profile (DESIGN §11.4); the override tree and the player's `profiles\` together ≤ 8 MB and ≤ 2,000 files; `profiles\` ≤ 256 files of ≤ 8 KB |
| Core memory | Event FIFO 256; RAM ring 4,096; trackers 256; K store 512; text cache 4,096; param cache 2,048; target ≤ 64 MB resident for all of EldenNet |
| Threads | Core stack 8 MB; the net's workers 2 MB each (built: en-llm's `StackThread` for the handle's workers and dispatcher and WinHTTP's drivers); no recursion on T2–T5 |
| SQLite | `PRAGMA max_page_count` sized to a 512 MB file cap; `diag.sqlite` rotated at 50 MB (DESIGN §10.5) |
| In-process ML | None (DESIGN §1.2) |

### 10.3 Failure contract (DESIGN §2.7, completed)

| Failure | Ladder | Player sees |
|---|---|---|
| Unknown build | L1 | Banner: "EldenNet inactive: game build 2.x.y.z not supported yet" |
| Discovery or readiness failed | L1 | Banner: "EldenNet inactive: game build 2.x.y.z not supported yet (reason: %s)" |
| Anti-cheat present; `session` failed | L1 | §5.4 texts |
| Task or structural group tripped | L1 | Banner: "EldenNet stopped reading the game after an error. Your game is not affected." |
| Optional group tripped | L3 | One toast naming the feature, for example "Floating labels are off for this session." |
| Core subsystem disabled | L3 | Toast: "Conversations are off for this session after an internal error." |
| Store tripped | L3 | Toast: "Memory is off for this session." |
| No healthy provider | L2 | Badge: "AI paused: provider unavailable (F10)" |
| Slow first token, no reply in 20 s, 401/403, 429, lint twice | — | As in DESIGN §2.7 |
| Multiplayer | Dormant | Badge: "EldenNet paused (multiplayer)" |
| Overlay failed to install: since GraceUI's G15, a failed connect to GraceUI (missing, `too_old`, `refused`) | L2 at most | Nothing can be drawn. Logged, plus a `launcher.log` hint the next time the launcher runs (GraceUI D88); with `too_old`, GraceUI's own notice names EldenNet and both versions |

Toasts wait until boss fights end, are rate-limited to one per error class per minute, and show at most 2 at a time.

---

## 11. LLM client and configuration

### 11.1 Client and provider schema

- **API:** one OpenAI-compatible client over `/v1/chat/completions` with `stream: true`, plus `/v1/models` (ER06 §5.2).
  Built in C++ as en-llm (O84): libcurl on Linux and in tests, the system's WinHTTP on Windows, each with the system
  root store, HTTP/1.1 keep-alive and a connect timeout of 3 s (impl), and our own SSE parser (`data:` lines,
  `[DONE]`). WinHTTP is loaded at run time by its full path in the system directory and never imported by name: a
  proxy `winhttp.dll` beside the game (§4.10) would otherwise stand in for it and see every request, the key's header
  included (WHY§101).
- **Streaming:** `stream_options.include_usage` is requested where supported. Decision calls also ask for `logprobs`
  (the top 5, impl) where the provider offers them (§11.8).
- **Reasoning:** reasoning is switched off per provider through `extra_body`, and `<think>` is stripped anyway
  (DESIGN §11.1). The switch goes with every request, retries included (§11.2).
- **Quantisation floor:** where a provider lets a request filter its upstreams by quantisation (OpenRouter's
  `provider.quantizations`), `quant_floor` keeps every request at that level or above; the client writes it in the
  flavour's own field and keeps it on a retry, as it keeps the reasoning switch (§11.2). Elsewhere it is ignored: a
  local model's quantisation is the file the player loaded (§11.4).
- **A provider for every use** (O88): a provider is an endpoint with its own key, one `[[provider]]` entry with its
  `base_url` and `key_ref`, and every slot names its own provider, model and fallback (`provider`, `model`, `fallback`
  and `fallback_provider`, in `[slots.*]` below or in the preset's entry for the slot, §11.4). That is how each use
  gets a provider of its own: replies on one, prepared lines on another, decisions on a fast one. A slot that names no
  provider takes the one its preset gives it, and a fallback that names none takes its model's provider. The reserved
  slots `actions` and `vision` are written the same way (§11.4).

```toml
# %APPDATA%\EldenNet\config\providers.toml
schema = 1
preset = "cloud-budget"                 # or "custom"; presets come from content/eldenring/models.toml

[[provider]]
id = "openrouter"
flavor = "openai"                       # openai | ollama | llamacpp | lmstudio | koboldcpp
kind = "cloud"                          # cloud | local
base_url = "https://openrouter.ai/api/v1"
key_ref = "secrets:openrouter"          # or "env:VARNAME"; never inline
headers = {}
extra_body = { reasoning = { enabled = false }, provider = { sort = "latency" } }
max_concurrency = 2                     # cloud 2, local 1 (impl)
json_mode = "json_schema"               # none | json_object | json_schema
quant_floor = "fp8"                     # where the provider filters its upstreams by quantisation (impl)
cache_marks = "breakpoints"             # none | breakpoints | cache_prompt: how the cache marks travel (NPC §7.1)
logprobs = true                         # whether decision calls may ask for logprobs (§11.8)

[slots.dialogue]                        # optional per-slot overrides of the preset's model choice
model = "google/gemma-4-26b-a4b-it"
fallback = "google/gemma-4-31b-it"
# provider = "…" and fallback_provider = "…" name a provider by id (O88): by default the one the preset gives this
# slot, and for the fallback the primary model's
```

### 11.2 Error classes, actions and health

| Class | Detected by | `dialogue` | `bark_live` | `bark_pool` | `memory` | Health effect |
|---|---|---|---|---|---|---|
| `FirstTokenTimeout` | No content delta by the slot's first-token time | Fallback model once (already sent if a backup request started, below), then "…" and a toast | Seed | Retry once | Retry (2 total), then requeue | +1 failure |
| `TotalTimeout` | Total time exceeded | Commit shown pages; "…" and a toast | Seed | Keep seeds | Requeue | +1 |
| `RateLimited` | HTTP 429 | Fallback model once, then a toast | Seed | Drop the batch | Requeue | Provider cooling 60 s (honour `Retry-After` up to 300 s, impl) |
| `Auth` | HTTP 401/403 | Toast "API key rejected: open settings (F10)" | Seed | Drop | Requeue | Provider disabled until `providers.toml` or `secrets.toml` changes |
| `BadRequest` | HTTP 400/404/422 | Retry once without the optional fields (`json_schema`, `stream_options`, the rest of `extra_body`), but never without the reasoning switch or the quantisation floor (below), then a toast | Seed | Drop | Retry once stripped the same way, then requeue | Not counted (our request) |
| `Server` | HTTP 5xx, connection reset | Fallback model once | Seed | Retry once | Requeue | +1 |
| `Empty` | `[DONE]` with no content | Fallback model once | Seed | Batch rejected | Retry | +1 |
| `Truncated` | `finish_reason = length` before a full sentence | Show the complete sentences | Seed | Keep valid lines | Retry | +1 |
| `Refused` | A content-filter finish, or a refusal pattern | Authored deflection (DESIGN §2.7) | Seed | Drop | Keep the journal | Not counted |

**The reasoning switch is never stripped** (WHY§98). A reasoning model sent without it may spend the whole budget
thinking, so a retry keeps it, and keeps the quantisation floor (§11.1). When the error says that the model refuses
reasoning off (a 400 or 422 whose body names reasoning, impl), the one retry asks for the lowest effort the flavour
offers instead (`minimal`, else `low`), and the client records that about the (provider, model) pair: its later
requests start at that effort for the rest of the session, and the log and the debug view say so once.

**A backup request** (cloud replies; WHY§98). When a `dialogue` reply on a cloud provider has had no content delta
2.5 s (EST) after it was sent, the client sends the same request to the slot's fallback model, if that pair is healthy,
cancelling a running `bark_pool` or `memory` request on that provider if the backup needs its connection (that job
requeues). Whichever streams first is kept and the other is cancelled at once, which counts as no failure for its
pair; both count towards the requests per hour (§11.4). The core still sees one request (§1.3). Own-initiative
lines get no backup, and neither do local providers, which serve one request at a time (§11.3).

**Decision calls** take their rules at once on every class, with no retry (§11.8); their failures count in health as
any slot's do.

**Health is tracked per (provider, model):**
- 3 consecutive counted failures put the pair in a 60 s cooldown, during which the slot uses its fallback model (DESIGN §11.1).
- A success resets the count.
- When every model of a slot is cooling or disabled, the core sees "no healthy provider" for that slot. That gives L2 if it is `dialogue`, and seeds only if it is the bark slots.

### 11.3 Local GPU priority, pause rules and warm-up

- **Priority on a local provider:** replies (`dialogue`) > own-initiative `dialogue` requests, each with the
  `decisions` call that chose it (§11.8) > `bark_live` > `memory` > `bark_pool`. An own-initiative request is
  cancelled by the input epoch, so it never delays a reply (DESIGN §8.9, O37; DESIGN §11.1). Local providers run one
  request at a time.
- **Preemption:** a higher-priority job cancels a running `bark_pool` or `memory` job, which requeues. It never cancels dialogue.
- **Pause rules for background jobs** (`memory`, `bark_pool`):
  - paused while `in_combat`, during a boss fight, while a conversation is open on a local provider, and while the gate is false;
  - memory resumes on cloud after 10 s calm, and on local at the next rest or after 45 s calm (DESIGN §10.3);
  - pools generate at rest or after 60 s without combat, never in boss fights, and never while a local model serves dialogue (DESIGN §9.3).
- **Warm-up:** on `PersonaApproached`, if a local `ollama` provider has served no request for > 4 min, send its
  native `POST /api/generate {model, keep_alive: "10m"}` with no prompt. Ollama's `/v1` endpoints do not document
  `keep_alive` (ER06 §5.2). Other flavours get no warm-up in v1. Built as en-llm's `Service::warm_up(slot)`, where a
  warm-up that succeeded counts as serving (WHY§101).

### 11.4 Slots, `models.toml`, presets and local probes

| Slot | Use | max_tokens | temp | First token | Total | Retries | On failure |
|---|---|---|---|---|---|---|---|
| `dialogue` | Persona replies; persona own-initiative lines (cancelled by the input epoch, DESIGN §8.9) | 140 | 0.8 | 6 s (10 s local) | 20 s | 0, plus 1 lint regeneration (replies only) | Replies: fallback model once, then "…" and a toast. Own-initiative lines: the seed or silence (DESIGN §8.9) |
| `bark_live` | Call-out replies; generic autonomy ticks (DESIGN §9.8) | 32 | 0.9 | 1.5 s | 3 s | 0 | Seed |
| `bark_pool` | Pool batches | 600 | 1.0 | 15 s | 60 s | 1 | Keep seeds |
| `memory` | Journal job; the lived layer's update (MEM §13.8) | 500 | 0.3 | 20 s | 60 s | 2, then requeue | Keep the journal, or the lived version |
| `selftest` | Wizard | 32 | 0 | 10 s | 20 s | 0 | Show the error |
| `draft` | The profile helper: one draft per click (DESIGN §7.7; NPC §4.7) | 500 | 0.7 | 20 s | 60 s | 0 | Show the error; nothing is written |
| `decisions` | Decision calls (§11.8): the next speaker in v1, the recall gate after v1; at most 12 options, the top 5 logprobs where offered (§11.1) | 100 | 0 (impl) | 1.5 s (3 s local, impl) | 5 s | 0 | The rules fallback: the task's rule (§11.8) |
| `actions` | *Reserved* (O88): NPC actions, after v1 (O31 to O42). An act is to be picked as a decision call picks, from options code builds (DESIGN §13) | 100 | 0 | Set with the feature | Set with the feature | Set with the feature | No act: the rules fallback (§11.8) |
| `vision` | *Reserved* (O88): a later feature that lets an NPC see the screen; none is designed yet | Set with the feature | Set with the feature | Set with the feature | Set with the feature | Set with the feature | Set with the feature |

Unknown slot names fall through to `dialogue`; a preset that names no `draft` model uses its `dialogue` model, and one
that names no `decisions` model uses its `bark_live` model (impl). Budgets are identical in every preset (DESIGN §11.2).
The reserved slots keep the files and Settings in the shape their features will need (O88): `[slots.actions]`,
`[slots.vision]` and a preset's `actions` and `vision` are read and checked as every slot's are, but no request uses
them yet. A request that names one falls through to `dialogue`, as an unknown name does, until its feature exists and
brings the budgets left open above.

**Requests per hour** (WHY§99). Beside these budgets for each request, `budget.requests_per_hour` (§11.5) bounds what
the client sends in any rolling hour, retries, backups and decision calls included. At the bound, replies still go out,
since the player asked; own-initiative lines take their seeds or silence, decision calls their rules, and background
jobs wait. One toast says so, and the debug view shows the count. The default, 600 (impl), sits above the 506 an hour
measured for SkyrimNet outside scenes (WHY§99) and is meant to stop a runaway, not ordinary play.

**Presets** (O88). A preset names a model for each slot and the providers its slots use, in one of two forms. A preset
of one provider may write it as `provider = { flavor, kind, base_url }`, as the local presets do. A preset may instead
name its providers, each a table `[preset.<id>.providers.<name>]` with the same three keys, and each slot then names
its own (`provider = "<name>"`) and, where it differs, its fallback's (`fallback_provider = "<name>"`, by default the
slot's own); a slot may leave out `provider` only when its preset names one provider. Each of a preset's providers is
matched to an entry of `providers.toml` as the single one is: by `base_url` where it gives one, otherwise by kind and
flavour. The keys stay with those entries (`key_ref`, §11.5): a preset never holds one. Refused, with the file and
line: a preset with neither form or with both, a slot naming a provider its preset does not define, and one name given
to two providers.

Two cloud presets ship (O88): OpenRouter's, `cloud-budget`, offered first, and NanoGPT's, `cloud-nanogpt`. A preset
that names its models is measured before it ships, and again whenever a model changes: its `decisions` and `bark_live`
models must reach a first token within 1.5 s, those slots' first-token budget, which DeepSeek V3.2 through NanoGPT
missed at 2.9 s (WHY§108). NanoGPT's models are to be measured (run 3's timings, roadmap S8.1.h); OpenRouter's stay
X1's to confirm or replace (DESIGN §11.3).

```toml
# content/eldenring/models.toml (shipped; volatile, ER06 §6)
schema = 1

[preset.cloud-budget.providers.openrouter]   # OpenRouter's (O88), offered first
flavor = "openai"
kind = "cloud"
base_url = "https://openrouter.ai/api/v1"

[preset.cloud-budget]
dialogue  = { provider = "openrouter", model = "google/gemma-4-26b-a4b-it", fallback = "google/gemma-4-31b-it" }
bark_live = { provider = "openrouter", model = "google/gemma-4-26b-a4b-it" }
bark_pool = { provider = "openrouter", model = "deepseek/deepseek-v4-flash" }
memory    = { provider = "openrouter", model = "qwen/qwen3-235b-a22b-2507" }
warn_free_tier_for = ["bark_pool"]

[preset.cloud-nanogpt.providers.nanogpt]     # NanoGPT's (O88)
flavor = "openai"
kind = "cloud"
base_url = "https://nano-gpt.com/api/v1"

[preset.cloud-nanogpt]                        # every model to be measured before it ships (roadmap S8.1.h)
dialogue  = { provider = "nanogpt", model = "…", fallback = "…" }
bark_live = { provider = "nanogpt", model = "…" }   # a first token within 1.5 s
bark_pool = { provider = "nanogpt", model = "…" }
memory    = { provider = "nanogpt", model = "…" }
decisions = { provider = "nanogpt", model = "…" }   # a first token within 1.5 s

[preset.local-12gb]
provider = { kind = "local" }           # one provider, filled from the probe
all = { model_hint = ["gemma-4-e4b", "qwen3.5-4b"], quant = "Q4", background_at_rest_only = true }

[preset.local-16gb]
provider = { kind = "local" }
all = { model_hint = ["qwen3.5-9b"], quant = "Q4" }
```

**Local probe order** (DESIGN §11.3): LM Studio `:1234`, Ollama `:11434`, llama.cpp `:9931` then the legacy `:8080`, and KoboldCpp `:5001`.
- Each probe is a `GET http://127.0.0.1:<port>/v1/models` with a 500 ms connect timeout and 1.5 s total (impl).
- A port counts only if it answers with a model list. The flavour is confirmed by `GET /api/version` (Ollama) or `/props` (llama.cpp). A port's owner is never assumed (ER06 §5.2).
- Probes run only from the wizard and the F10 "Find local servers" button, and never while the gate is false.

### 11.5 Settings and secrets

```toml
# %APPDATA%\EldenNet\config\settings.toml
schema = 2
enabled = true                                           # false → Off at the next launch, AI paused now (§1.5)
features = { chat = true, barks = true, speak_up = true, callout = true, memory = true, labels = true, npc_to_npc = true, self_awareness = true, status_awareness = true }
autonomy = { enabled = true, period_s = 120 }           # 30–1800; enabled gates the autonomy motive only, the tick still runs (DESIGN §9.8); free-tier presets write enabled = false
range    = { talk_m = 8.0, hear_m = 20.0, fight_m = 40.0 }   # 3–16, 8–30, 10–40
pacing   = { gap_s = 2.5, group_gap_s = 8.0, window_s = 60, fight_max = 6, idle_max = 2, talk_max = 4 }
ui = { hdr = "auto", paper_white_nits = 203, follow_game_subtitles = true, latency_toasts = false }   # NPC §10.7; `scale` is GraceUI's own setting since GraceUI D84, and EldenNet's text follows it
pools = { max_jobs_per_session = 60 }                    # 0 turns generated pools off (NPC §9.5, O23)
budget = { requests_per_hour = 600 }                     # 60–3600 (impl); replies are never refused (§11.4)
safety = { level = "faithful" }                          # faithful | gentle (O17)
hotkeys = { talk = "T", skip_page = "Tab", debug = "F11" }   # talk default from S4; the defaults EldenNet registers with GraceUI, whose Hotkeys page keeps the player's bindings (GraceUI D77, D84); the menu key (F5) is GraceUI's, so `settings` goes, by a migration before v0.6 (unknown keys are refused)
augur = { allow_custom_rules = false }
debug = { panel = false, record_sessions = false, record_redact_text = false, log_level = "info" }
# tables last: every key above is top-level

[reactions]                                              # every reaction id but autonomy (NPC §9.1); cooldown_s 0–21600, seconds of play
engaged      = { on = true, cooldown_s = 20 }
struck_hard  = { on = true, cooldown_s = 20 }
low_hp       = { on = true, cooldown_s = 60 }
ally_fell    = { on = true, cooldown_s = 20 }
foe_fell     = { on = true, cooldown_s = 20 }
player_fell  = { on = true, cooldown_s = 0 }
foe_fled     = { on = true, cooldown_s = 30 }
foe_healed   = { on = true, cooldown_s = 30 }
status_self  = { on = true, cooldown_s = 30 }
status_plea  = { on = true, cooldown_s = 45 }
status_warn  = { on = true, cooldown_s = 20 }
status_foe   = { on = true, cooldown_s = 20 }
idle         = { on = true, cooldown_s = 90 }
greet_player = { on = true, cooldown_s = 600 }
greet_npc    = { on = true, cooldown_s = 300 }
answer       = { on = true, cooldown_s = 20 }
stage_talk   = { on = true, cooldown_s = 300 }
news         = { on = true, cooldown_s = 60 }       # O30: 60 s of play per persona and item
saw_kill     = { on = true, cooldown_s = 60 }
saw_victory  = { on = true, cooldown_s = 600 }
# autonomy is switched and timed by `autonomy` above (enabled, period_s), not here

[allow."kind:creature"]                                  # DESIGN §7.6: kind, then type, then named slot
dialogue = false
[allow."card:vulgar_militia"]
reactions = { greet_npc = false }                      # its greetings only; answers stay
[allow."persona:patches"]
autonomy = false
reactions = { saw_kill = false }
```

- `features.barks` is the master switch for generic NPCs' own-initiative lines, `features.speak_up` for named NPCs'. `features.ambient` is gone (`pacing.idle_max`; schema 2 migrates `off` → 0, `sparse` → 2, `normal` → 4).
- Effective permission for a line = the feature switch ∧ `reactions.T.on` ∧ the speaker's allowance for T (DESIGN §7.6) ∧ a voice source ∧ every gate. Every line, replies and call-out replies included, also needs the speaker's `dialogue` allowance. `overrides/disable.toml` `triggers = [...]` still removes a trigger from content; either switch turns a trigger off.
- Before the F10 pages ship (v0.6) the README's config-file section documents these keys; each key ships in the increment of its feature (DESIGN §16.2).

```toml
# %APPDATA%\EldenNet\config\secrets.toml (keys only)
[keys]
openrouter = "<entered in the wizard or by the player>"
```

- **Secrets are never echoed.** Keys are held as `Secret(String)`, whose `Debug` and `Display` print `"<redacted>"`. They are never logged and never put in recordings, debug dumps or the bug-report bundle.
- **File permissions:** the wizard creates the file with an ACL for the current user only.
- **Strict parsing:** every config file is parsed with toml++, refusing any key its schema does not define, and checked against its `schema` version. A newer schema is refused with a toast. An older one is migrated in memory and written back only on the player's next settings change.

### 11.6 Override layering, `disable.toml` and hot reload

**Content layers** (DESIGN §11.4):
1. `<install>/content/eldenring/…` (shipped);
2. `<install>/plugins/<id>/…` (from 1.1, O66): the content plugins that are on, in the player's order (`config\plugins.toml`), each adding or replacing only what [proposal 0001](../proposals/0001-plugins.md) §5.1 allows, a later plugin winning over an earlier one, every conflict shown on the Content packs page;
3. `%APPDATA%\EldenNet\overrides\…`: the same relative paths, and each file **replaces** the shipped or plugin file whole;
4. `overrides\disable.toml`.
5. `%APPDATA%\EldenNet\profiles\…`: the player's own profiles (DESIGN §7.7; NPC §4.7). `layers\` adds fields to one
   shipped profile after layers 1–4, `characters\` adds character profiles of the player's own, and `tarnished.toml`
   is the Tarnished's own bio (O46: `public.summary` and `public.appearance` only). They are never shipped, bundled or
   uploaded, and they always win over a plugin's.

```toml
# %APPDATA%\EldenNet\overrides\disable.toml
personas = ["patches"]          # that persona becomes mute
cards    = ["kaiden_sellsword"] # its humanoids fall back to `nameless_soldier`
characters = []                 # shipped character profiles; their NPCs become unbound
kb       = ["rumour.some_key"]
triggers = ["idle"]
rules    = []                   # only rules marked disable_ok = true in their file
```

- **Not disableable:** `identity/speech.toml` (perceivability, `may_speak` as the voice source, `may_remember`),
  `identity/types.toml`'s kind rules, the binding rule of `identity/characters.toml` (nothing speaks on an unverified
  binding), the built-in rules (a)–(f) (NPC §1.3), the output guards' word lists (`lexicon/banned.toml` and
  `lexicon/safety.toml`, NPC §8.3), `flags/catalog.toml`, `teams.toml` and the world frame; and, as rules in code,
  every context gate of DESIGN §9.4 (alive, conscious, native talk and scripted lines win, the death window, quiet
  after a conversation, the input epoch, `hud`, banners, loading, dormancy), the
  conversable and safe-context predicates, strict mode, the grounding from the playthrough state (what each NPC knows
  at its stage, the one spoiler mechanism, O86; there is no spoiler setting), and R2 (the action and gesture
  allowances cannot be turned on). These are the safety rules and the grounding, and an attempt is reported as a
  validation error.
- **Not hot-reloaded:** `gal/` is read at boot only and is never overridden.

**Hot reload** (on T10):
1. A `ReadDirectoryChangesW` watcher covers `config/`, `overrides/` and `profiles/`, plus `content/` in dev builds and
   `plugins/` from 1.1, with a 500 ms debounce (DESIGN §11.4).
2. On a change, T10 parses and validates the whole affected layer: schemas, caps, predicates, aliases, template
   compilation against fixture contexts (an unknown name fails it, NPC §7.1), and lint; for a plugin also its
   manifest, the folders and kinds of change §5.1 of proposal 0001 allows, and every id, type, flag and term it names
   (O86 drops proposal 0001 §5.4's locked-term check). A player's profile file is validated alone, with NPC §4.8's
   rules for player files: a refused one keeps its last good version, every other file loads, and the toast and the
   editor name the file, field and line, never its text.
3. Success sends a `Reload` holding a `std::shared_ptr<const ContentPack>` or a `std::shared_ptr<const Settings>` over
   C11. The core swaps it between steps. A save sent on C17 also gets its `Reload::ProfileResult` on C11, saved or
   refused.
4. Failure keeps the old version and shows a toast with the file and line.

When each change takes effect:

| Change | Effect |
|---|---|
| Personas, KB, cards, prompts | Next request. Only the edited persona's cached prefix is invalidated |
| The player's profiles | Next request; only that profile's cached prefix is invalidated, and its hash changes, which retires its generated pool lines; an edit of `subjects` or `stirs` alone invalidates no cached prefix and retires no pool line |
| Providers, keys | Next request; in-flight requests finish on the old config |
| Hotkeys, UI | Immediately |
| `features`, `autonomy`, `range`, `pacing`, `reactions`, `allow` | The next step; pending requests are re-gated; cooldowns, window entries and reservations are kept; a visible line whose speaker is no longer allowed fades |
| Augur rules | The next step. Detector state is kept for rules whose `id` and detector are unchanged, and reset otherwise |
| `enabled` | §1.5 |

**Staleness bound** (WHY§99: SkyrimNet's cached template values may be up to 55 s stale). No value a template
renders is older than the step that builds its request: live state (who is near and their stance, health in words,
the place, the hour, combat) is read from that step's world model; values derived from flags (the world view, known
sets, stages) are memoised per flag generation (§8.9), so they are at most one processed flag frame old (1 s, or 2 s
while throttled); and a cached prefix is keyed by every input it renders and rebuilt when one changes. Nothing is
cached by time alone. A request that waits is bounded by its class's expiry, and its gates are checked again at
display (DESIGN §9.4).

**Caps:** 64 KB per file, 16 KB per persona, card or character profile, 8 KB per player profile file, and the override tree and `profiles\` as in §10.2.

### 11.7 Wizard and self-test

The wizard opens at the title screen on the first run, or when `providers.toml` is missing. Its network steps run only while the gate allows them (§5.1).

| Step | Shows or does | Pass rule |
|---|---|---|
| 1 Notices | The four statements of §4.8 | Acknowledged |
| 2 Status | Build, ladder level, capability list (§6.8) | Informational |
| 3 Provider | *Cloud*, first (O88): a masked key field, written to `secrets.toml`. The preset follows the key's provider where the key's form marks it (OpenRouter's keys begin `sk-or-`); otherwise the player says which provider the key is for, OpenRouter first, then NanoGPT. The key is never tried on a provider to learn whose it is. The provider's entry and the preset go to `providers.toml` (§11.1, §11.4). *Local*, one choice away: the probe results (§11.4) and a model picker | A key is entered and its provider known, or a model is picked |
| 4a Reachability | `GET /v1/models` on each provider the chosen preset uses; its models are listed | HTTP 200 and the models are present |
| 4b Latency | `selftest` slot: a streamed 32-token reply; measures TTFT and tokens per second | Completes |
| 4c Voice check | One Lantern-Keeper line through the real `dialogue` pipeline (prompt, sanitizer, lint) | The output passes the lint |
| 4d JSON check | The `memory` slot returns a 2-field object against a schema | Parses |
| 4e Verdict | Estimated first page = TTFT + 15 tokens ÷ tokens per second. Shown as "quick" (≤ 1.6 s), "acceptable" (≤ 3.0 s) or "slow", plus the figures. The words follow C5's cloud targets | Informational |
| 5 Pre-bake | Optional bark pool pre-bake with a progress bar; play can start at once | Optional |

**Silent self-test** at every Ready while the gate allows: steps 4a and 4b only. A failure gives one toast and one log
line with the cause. `eldennet-engine selftest` runs steps 4a–4e outside the game.

### 11.8 Decision calls

A decision call asks the model to pick one of a few options that code has built. It is EldenNet's own format (O84:
SkyrimNet's logic, not its prompts) over the same chat completions as every other request (§11.1), on the `decisions`
slot (§11.4), which replaces the reserved `meta` slot and `DecisionRequest` (WHY§98, WHY§99).

**The principle: code gathers, the model picks.** Every model call, a decision or a line, gets only the records it
needs, gathered by code by id; the model never searches, joins or counts. Bookkeeping (who is present, who heard what,
cooldowns, stages, the known set) stays in code, and the model only picks or writes. Thinking is a last resort. In
bench runs, code that gathered the records a question needed gave 93 to 100% right answers, against 0 to 25% when the
model was handed an index or told the joins (LIKELY, WHY§99).

```text
DecisionCall {                          // built by en-core, rendered from prompts/decisions/
  task:     NextSpeaker | RecallGate    // RecallGate after v1
  kind:     Choice | YesNo
  subject:  optional ProfileId          // the NPC it is for, if one (the recall gate's)
  question: one plain question
  state:    [(name, text)]              // the records it needs, gathered by id, quoted as data
  options:  [(key, text)]               // Choice only: short keys (A, B, … or neutral labels), and none; by code
  rule:     the key the task's rules pick from the same state (a pure function of it, run by en-core when it
            builds the call): data, one of the options' keys, checked before anything is sent
}
DecisionPick { key, probs: optional [(key, f32)], by: Model | Rules, latency_ms }
```

Both are plain data in en-proto's `llm.hpp` (S8.1.j), read and acted on by en-llm's `decision.hpp` (O84, WHY§100),
the pick with how it was read and the call's one row.

- **Options are built by code.** Only legal options are offered, at most 12 (EST, as MEM §4.5's gate keeps), each a
  short key with one line that code writes from records; the model cannot name an option code did not offer, and
  `none` is an option wherever picking nothing is a legal answer (silence, no memory).
- **The answer.** Where the provider takes a JSON schema (`json_mode = "json_schema"`, §11.1), the reply is held to an
  enum of the keys. Elsewhere the model answers in plain words and the reader goes by meaning: the key, the option's
  own words or a plain paraphrase ("no one" and "silence" for `none`), whatever the case, quotes, markup or words
  around it. An answer that matches no option, or more than one, is a miss. A provider that rejects the schema
  (`BadRequest`) gets plain answers for the rest of the session.
- **Yes or no.** A question that can be put as yes or no is put that way (`YesNo`) and read the same way. In the bench
  runs, typed decisions on a small model never failed to parse but were weak on subtle readings, while plain chat
  matched or beat them once readers went by meaning and questions were plain yes or no (WHY§99).
- **Probabilities, never a cut-off.** Where the provider returns logprobs (§11.1), the call's row records each key's
  probability, for the diagnostics and the evaluation. Nothing cuts on it: the pick stands whatever its probability,
  since SkyrimNet's 0.70 cut-off lost about a quarter of right answers (WHY§99).
- **Thinking off.** Every decision call carries the reasoning switch (§11.1, §11.2), and `<think>` is stripped as
  anywhere. A task may turn thinking on only when its evaluation shows the plain pick failing, and then at minimal
  effort within the slot's 5 s: thinking cost 2 to 35 s a step in the bench runs, with rare runaways near 50k tokens
  (WHY§99).
- **A rules fallback, always.** The task's rule decides when no model serves the slot (none set up, or none healthy),
  the gate is false (§5.1),
  the requests per hour are spent (§11.4), the call fails or runs out of time (any class of §11.2, with no retry) or
  its answer is a miss. Replays with `--llm mock:<script>` and the scenario tests use the rules unless the script
  answers (§12.3).
- **One row per call.** Each call writes exactly one row in the playthrough file's `decisions` table (MEM §8.7;
  DESIGN §13's conformance test): the task, its subject, the option keys, the pick, its probability where offered,
  whether the model or the rules decided, the latency and the prompt's hash; never the prompt's text or a name. A call
  emits no event: `DecisionMade` stays reserved for actions (§8.6).
- **Prompts.** The templates are `prompts/decisions/NN_*.j2`, under NPC §7.1's rules: records enter as quoted data,
  never as instructions, and examples use names no record holds.

**The next speaker** (v1; NPC §9.2, DESIGN §9.7). Outside a fight, when a moment leaves two or more profiled NPCs
eligible for the same calm motive (a heard line with no addressee, a deed several of them saw, the Tarnished coming
near several at once at a shared place such as the Roundtable Hold), code lists the legal speakers: each one the
motive's own rule accepts and the scheduler would admit now (DESIGN §9.4), in the audience of what it would answer
(§8.8), never the line's own speaker and never a card. Each option is a neutral label with the candidate's `brief`,
`public.summary` and `stirs` (NPC §9.2), never a name, and `none`, silence, is always offered. With one legal speaker
there is no call. The model picks the speaker only: the addressee is code's, the talk target the motive sets (the
heard line's speaker for an answer, the Tarnished for a greeting, no one for a remark on a deed). The call goes out
when the moment arrives, so its pick is usually ready before the gap (`pacing.gap_s`) allows the next line. Combat
speakers stay with the rules, since a combat line cannot wait for a call. The rule is the motive's own: the addressee
or the nearest listener on the speaker's side for an answer (`addressee_or_side_listener`), otherwise the scheduler's
order (NPC §9.4).

**The recall gate** (after v1, with MEM §4.5's `SceneRecall`). Code gathers the candidate memories and the gate picks
the one meant, or `none`, by a question that names the setup, the matter and the outcome, the form that did best in
the bench runs (WHY§99). Its rule takes the candidate both searches ranked first, else none (MEM §4.5).

---

## 12. Testing and tooling

### 12.1 CI pipeline

| Stage | Runs on | Fails on |
|---|---|---|
| `fmt` | Linux: `clang-format`, with the checked-in configuration | Any diff |
| `tidy-pure` | Linux: `clang-tidy` and warnings as errors over `en-proto`, `en-augur`, `en-core`, `en-prompt`, `en-store`, `en-llm`, `en-tools` | §2.4 lists |
| `tidy-win` | Linux: the same over the clang-cl build against xwin's SDK | §2.4 lists for `en-gal`, `en-overlay`, `eldennet`, `en-launcher` |
| `greps` | Linux | `ffi_guard` first statement in every `extern "C"`, `WINAPI` and `CALLBACK` body (R3); `GameThread::issue` allowlist (R1); empty detach branch (R14); `IsEnableOnlineMode` anywhere except its absence test; raw addresses, `reinterpret_cast` and pointer arithmetic outside the files §2.4 allows, or without a `// SAFETY:` note; clocks and `std::unordered_map` in the pure modules; the R12 backstop (since GraceUI's G15: any MinHook (`MH_`), ImGui renderer backend (`ImGui_Impl`) or detour symbol in EldenNet's sources); `patterns.toml` provenance not `own-derivation` |
| `test` | Linux, with GCC and with Clang: doctest through CTest, under AddressSanitizer and UndefinedBehaviorSanitizer, and ThreadSanitizer for the threaded modules | Unit, property, DB, conformance, prompt snapshots (golden files, our own diff), golden replay, scenarios, backpressure; any sanitizer report |
| `test-win` | Linux: the MinGW-w64 build's Windows tests under Wine (WHY§102) | Any failure |
| `fuzz` | Linux: a libFuzzer target for every parser (the PE parser, the pattern grammar, SSE frames, TOML content), for a fixed time per change | A crash, a sanitizer report or a hang |
| `content` | Linux: `eldennet-engine lint` over `content/` and `gal/` | Schemas, caps, predicates, aliases, adjectives, banned words, gender-neutral cards, beckon (W13), positive wording (W15, warn), opponent-neutral combat seeds, and `lead` entries in release layouts; no spoiler lint against term lists (O86: stage texts and bio fields are reviewed when written) |
| `eval` | Linux: `eldennet-engine eval --preset cloud`, on a change to a persona's content, the world files (`world/stages.toml`, `world/tracks.toml`, `regions.toml`), the flag catalogue (`flags/catalog.toml`, an alias's `verified` included), the KB (`knowledge/`), the lexicon (`lexicon/`), a prompt, the sanitizer or a model default, and on release tags; never on every commit. The provider key is a CI secret (from v0.2, DESIGN §16.3) | A set below its gate (NPC §12.1, §12.5); a failed judge check (DESIGN §14) |
| `licences` | Linux: the third-party check over `THIRD_PARTY.md` and the build's sources. Every third-party source is vendored with its licence files, or fetched at configure time from its own project and pinned by SHA-256 (`CMakeLists.txt`), and each is listed in `THIRD_PARTY.md` with its version and licence, against an allowlist (MIT, Apache-2.0, Apache-2.0 WITH LLVM-exception, BSD-2-Clause, BSD-3-Clause, ISC, Zlib, Unicode-3.0, CC0-1.0, BSL-1.0, MPL-2.0, the curl licence, SQLite's public-domain dedication). System libraries are listed apart: WinHTTP, which the Windows binaries load from the system directory, and libcurl, which only the Linux and test builds link. The ban list refuses fromsoftware-rs, `from-singleton`, me3, `dearxan` and libER by name. *Since GraceUI's G15 (2026-11-08):* MinHook and the overlay's renderer backends are GraceUI's, so EldenNet's ban list also refuses, by name, every known hook library (MinHook, Detours, PolyHook, subhook, funchook, Frida, Dobby, kiero) and any ImGui renderer backend (`imgui_impl_*`); Dear ImGui's core stays allowed without a backend, for F11, as one copy at the version GraceUI pins (DESIGN O25). Once a first hook of EldenNet's own is approved (O65), its one hook library is the ban list's single exception, allowed in `en-gal` behind `hooks`, its licence checked when it is chosen. A hook library under another name is caught when it is adopted, by the licence check every new dependency gets (CLAUDE.md) (R12) | A source not listed, unpinned, or fetched from elsewhere than its own project; a hash that differs; any other licence; a listed name |
| `licence-files` | `eldennet-engine lint --licence-files`: walks every third-party source folder, vendored or fetched into the build tree, and fails on any file whose text contains `NON-AI` or `NON AI`. It also compares the SHA-256 of Dear ImGui 1.93.0's `LICENSE.txt` (F11's core) with the value recorded in the repository, and checks that `THIRD_PARTY.md` carries the notice of every licence file it finds in a linked dependency's source (DESIGN O25). *Since GraceUI's G15:* in EldenNet's tree no source may be MinHook (`MinHook.h`, or a C definition of `MH_Initialize`) or have a name that matches `(?i)min[-_]?hook\|detour\|retour\|ilhook\|neohook\|frida\|dobby\|kiero\|polyhook\|subhook\|funchook`, save the one hook library O65 may allow in `en-gal`'s `hooks`; MinHook's licence checks, and its HDE's, are GraceUI's (GraceUI §2.2, GraceUI's proposal 0001 §4.14) | A licence file contradicting a library's declared licence (the case in ER00 §7); a changed Dear ImGui licence text; a MinHook copy, or a source whose name matches the pattern; a missing third-party notice |
| `options` | Linux: the release build's CMake cache, and `eldennet.dll`'s link graph and symbols | `EN_PROBE`, `EN_HOOKS`, `EN_ACTIONS` or `EN_FREEZE` on; MinHook or an ImGui renderer backend linked at all, since GraceUI's G15 (GraceUI's release job checks its own pinned Dear ImGui and MinHook) |
| `build-release` | Two independent runners: clang-cl and lld-link with the pinned LLVM and xwin SDK and CRT, `SOURCE_DATE_EPOCH`, path prefixes mapped, `/Brepro` | Their SHA-256 hashes differ (not reproducible) |
| `sign` | Every tester build from v0.1, and release tags | `SHA256SUMS` signed with the project's minisign key (the public key is in the README); Authenticode signing when a certificate is available (K4) |

### 12.2 Test types

| Type | Modules | Examples |
|---|---|---|
| Unit | All (doctest) | Detectors, scheduler with a simulated clock, identity, predicates, splitter, budgets, layout validation, pattern grammar, FMG and param parsing against synthetic bytes, world derivation, `known(P)` truth tables, the reaction-cue table and journey (MEM §12.11 T2, T12, T13), stance and concealment (§8.7), and the decision reader over keys, option words, plain yes and no, and misses (§11.8) |
| Property (seeded, our own on doctest) | `en-augur`, `en-core` | §8.12; sanitizer (never throws, never shows markup or a trailer, respects caps) |
| Fuzz (libFuzzer) | `en-gal`'s offline part, `en-augur`, `en-core`, `en-llm` | The PE parser, the pattern grammar, SSE frames and TOML content, under AddressSanitizer and UndefinedBehaviorSanitizer |
| Windows, under Wine | Every module the MinGW-w64 build makes | The Windows-only paths that run without the game: WinHTTP's conformance suite and the handle's tests in `en-llm` (built, WHY§102), and the rest as it is built |
| Golden replay | `en-tools` | 3 recorded routes; 10 identical replays (C12) |
| Scenario | `en-tools` | `.scn` files (§8.12) for barks, gating, interrupts, gate hysteresis, memory (`wren_and_kale`, MEM §2.8), knowledge (`star_falls`, MEM §12.11) and identity (`veiled`, NPC §2.7) |
| Prompt snapshot | `en-core` | Golden files, compared by our own diff and rewritten only by `eldennet-engine golden update`, per persona × stage × scene; blocks 1–5 byte-stable, 5b at a fixed known set and active ties; grounding renders at 6 flag states (what the persona is told at each state, O86); block-5b renders at the 8 fixture states plus `ashen`, and since-line and reaction-cue renders (MEM §12.11 T4); each decision task's prompt (§11.8) |
| DB | `en-store` | Every migration, split by SQLite's own parser; trim at 3 cut points, with no id reused after it; realistic ids (above 2³² and 2⁵³) read back exactly; numbers written the same under another locale (§2.2); crash mid-reply; NG+; slot reuse; `world_seen`, `world_seen_pt`, `world_cued` and `spoke_pt` after trim and NG+ |
| Conformance | `en-core` | A trailer never displays; with `actions` off no non-read command; one row per decision (DESIGN §13) |
| `FakeSys` | `en-gal` | Discovery over synthetic images; statics moving, nulls, vtable swaps, faults → breakers |
| Backpressure | `en-core`, `en-augur` | §8.12 |
| Eval | `eldennet-engine eval` | 50 turns per persona at 2–3 stages, plus test questions on what the model already knows of the game, 6 per persona and 4 per roster bio (O86; a persona's topics are its `stages.forbid`, a roster bio's the later truths its research note lists; NPC §12.1) and memory probes (criteria C9–C11), plus the world-awareness set (criterion C14, DESIGN §1.3). Every set runs from the increment its subject ships in and gates on DESIGN §16.3's schedule (O29), the judged rates once DESIGN §14's judge check has passed. A persona ships only with a passing report |
| In game | Dev builds | S0-A–S0-E, smoke route, 30 h soak (C1), 20 forced fail-closed launches with the `probe` option's fault injection (unknown build, missing pattern, forced read faults) |

### 12.3 `eldennet-engine` subcommands (`en-tools`)

`en-tools` builds one command, `eldennet-engine`, with its own small argument parser (built). The subcommands built so
far are marked; the others are added to it as they are built.

| Command | Purpose | Key flags |
|---|---|---|
| `chat` (built) | Send rendered messages to a configured slot and stream the reply (§11.1) | `--config`, `--slot`, `--messages`, `--purpose` |
| `decide` (built) | Ask a decision of a configured slot (§11.8) | `--config`, `--request` |
| `probe` (built) | Find the local model servers on this PC's usual ports (§11.4) | `--json` |
| `models` (built) | List the models each configured provider offers | `--config`, `--provider`, `--models`, `--json` |
| `replay <file.ensnap.zst>` | Drive the real `Core` (and Augur) through the replay `GameBackend` | `--llm recorded\|mock:<script>\|live:<providers.toml>`, `--out timeline.jsonl` |
| `golden check\|update` | Compare or rewrite `tests/golden` | `--route <name>` |
| `eval` | Scripted persona turns and probes against a provider | `--persona`, `--stages`, `--preset <cloud\|local12>`, `--set awareness` (C14, NPC §12.5), `--report out.md` |
| `lint` | Content and `gal/` validation; `--licence-files` (§12.1); `--profiles <dir>` checks a folder of player profile files with the player's rules (NPC §4.8) | `--strict` (release rules) |
| `render-prompt <template>` (built) | Print the exact prompt: a template file or folder of parts rendered with a JSON context into chat messages, or linted; later for a persona, stage and scene fixture | `--context`, `--root`, `--json`, `--no-merge`, `--lint`, `--list-functions` (built); `--slot`, `--fixture` |
| `probe-exe <eldenring.exe>` | Offline fingerprint, patterns, FD4 idiom and RTTI check against `gal/` | `--against gal/`, `--json` |
| `mockllm` | A local SSE server with scripted delays, 429s, `<think>` leaks, trailers and malformed JSON | `--script mock.toml`, `--port` |
| `bake-pools` | Pre-generate bark pools outside the game | `--cards`, `--providers` |
| `selftest` | Wizard steps 4a–4e outside the game (§11.7) | `--providers` |
| `fmg-import <export dir>` | Turn the **player's own** WitchyBND or Smithbox FMG export into `%APPDATA%\EldenNet\gamedata\fmg-<lang>.db` (git-ignored; never shipped) | `--lang` |
| `identity-fill` | Fill each persona row's `expect_name_id` from the author's own params (numbers only, never text; NPC §2.1) | `--persona` |
| `kb-draft` | Offline KB skeletons from the author's own export, every `text` left empty; `--check` flags ≥ 6-word overlaps with the export (MEM §5.6) | `--persona`, `--export`, `--check` |
| `roster-scan` | The roster's census from the author's own exports and install data: numbers into the index as leads, names only in the git-ignored `roster.local/` (DESIGN §7.8) | `--batch`, `--export` |
| `roster-draft` | Offline: one call per NPC turns our own research note into a draft bio in the git-ignored `drafts.local/roster/`; reads no game text, export or vanilla line (DESIGN §7.8) | `--id`, `--batch`, `--providers` |
| `roster-verify <recording>` | Match the roster's binding leads to a capture's sightings and write evidence records to `tests/roster/` (S1, S2; DESIGN §7.8) | `--batch` |
| `flag-verify <recording>` | Align flag edges with S6 marks and write evidence records (MEM §6.3) | `--script tests/s6/<topic>.toml` |

### 12.4 Compatibility matrix (rows from v0.1, the full matrix at v0.6; DESIGN §16.3)

| Axis | Values |
|---|---|
| Build | 2.7.2.0 WW, 2.7.2.1 JP, an unknown build (simulated by removing the row) |
| Platform | Windows 10, Windows 11, Proton (Steam Deck or desktop Linux) |
| Load path | Launcher (early); player's me3 native (late); both (double load → mutex); delayed injector (very late) |
| Other mods | Seamless Co-op (via the player's me3), Reforged, Convergence, a randomizer, the Steam overlay, another DInput-hooking overlay (S0-E) |
| GraceUI (GraceUI G15, D83, D88) | Another GraceUI consumer loaded; GraceUI without EldenNet; two copies of GraceUI at two paths; GraceUI listed as a native in a loader; an older GraceUI over a newer one; an installed older major, inside and outside the carried release's support window; another mod's hooking overlay beside GraceUI's; another mod as a second world-state provider |
| Display | SDR, HDR |
| Expected | Known builds: L4 with all groups OK and no input leak. Unknown: L1 with zero task registrations and zero reads (C2). ERSC session: Dormant within one snapshot (C4). Double load: one active copy. GraceUI rows: one hook library in the process (GraceUI's); a second GraceUI idles; the launcher installs or explains; a failed connect is headless and logged |

Each increment runs the rows its own features touch, with that increment's expectations: from v0.1 the builds (the unknown one included), the load paths (double load included), ERSC through the player's me3, Reforged, Proton, the Steam overlay, another DInput-hooking overlay (the present hook, GraceUI's since its G15, S0-E), the GraceUI rows that need no input (one hook library, a second copy idling, a failed connect, the launcher's install), SDR and HDR; from v0.2 the input-leak axis, across every input holder; from v0.4 labels × HDR. The full matrix certifies v0.6.

### 12.5 Patch drill

At v0.1's exit (or v0.2's, if the JP layout is not ready by then), and again before v0.6, the team rehearses §6.10 using **JP as the "new" build**:
1. Remove the `2.7.2.1-jp` rows and layout.
2. Run `probe-exe`, then the in-game probe, the self-tests and the smoke route.
3. Ship a data-only `gal/` to testers.

The drill is timed. It passes if the regenerated layout equals the shipped one in every confirmed field, and the golden events match.

---

## 13. Repo layout

```text
eldennet/
  CMakeLists.txt  .clang-format  .clang-tidy        # the CMake project, C++20 (§2.6); the checked-in configurations
  cmake/          the toolchain files: clang-cl and lld-link with xwin's SDK and CRT; mingw-w64.cmake (built)
  third_party/    inja/  doctest/                   # vendored with their licences (built); the rest is fetched at
                                                    # configure time, pinned by SHA-256, into the build tree
  src/            each module: CMakeLists.txt, include/en/<name>/*.hpp, src/*.cpp, tests/
    en-proto/     include/en/proto/{snapshot, render, lifecycle, command, ui, llm, db, backend, thread, text,
                       fixed, ffi_guard, json}.hpp  src/{json, llm, ffi_guard}.cpp   (llm, json, ffi_guard built)
    en-augur/     src/{sources/, world, signals, detectors, rules, events, audience, kstore, bus}.cpp
    en-prompt/    src/{environment, source, compile, builtins, messages, registry, text, errors, version}.cpp   (built)
    en-core/      src/{step, transport, identity/, content/, predicates, knowledge/, scheduler, conversation/,
                       barks/, memory/, decisions/, prompts/, sanitize, lint, ladder, gate}.cpp
    en-store/     src/{loop, migrations/, events, facts, journal, rollback, backup}.cpp
    en-llm/       src/{client, config, request, message, transport, transport_curl, transport_winhttp, sse, stream,
                       errors, health, decision, probe, service, thread, secret, clock, version}.cpp   (built)
    en-gal/       src/{gate, pe, patterns, builds, layout, fd4, rtti, groups, snapshot, flags,
                       text, params, render, backend, task, probe}.cpp  # the offline part: pe, patterns, builds, layout
                  src/reader.cpp                       # raw addresses: GuardedReader and its SEH-guarded copy
                  src/sys/{image, register, fd4, trampoline, boot, process}.cpp   # raw addresses: the Win32 seam
    en-overlay/   src/{surfaces/, fonts, style, input, debug}.cpp  src/abi.cpp   (no hooks: GraceUI's, G15)
    eldennet/     src/{exports, boot, shell, wiring, watch, recorder, log}.cpp   (ffi_guard: en-proto's)
    en-launcher/  src/{main, discover, vdf, preflight, backup, appid, notices, errors}.cpp  src/win/inject.cpp
    en-tools/     src/{main, render_prompt, chat, decide, probe}.cpp (built)
                  src/{replay, golden, eval, lint, probe_exe, mockllm, bake_pools, selftest, fmg_import,
                       identity_fill, kb_draft, flag_verify, scn}.cpp
  content/eldenring/
    prompts/{dialogue,callout,memory,pool,draft,decisions}/NN_*.j2  registers/*.md  personas/  archetypes/  characters/
    identity/{personas,archetypes,characters,speech,humanoid_models,types}.toml  flags/catalog.toml  knowledge/{*,aliases}.toml
    world/{stages,tracks}.toml
    lexicon/{banned,safety,signature,context_words,public}.toml  barks/triggers.toml
    augur/{world,persona,combat,ambient}.toml  teams.toml  regions.toml  models.toml
  gal/        builds.toml  patterns.toml  layouts/{2.7.2.0-ww,2.7.2.1-jp}.toml  derivations/<id>.md
  tests/      scenarios/*.scn  fixtures/{names.toml, flagstates/ (8 states + ashen), personas/ (test_veiled, test_stake), characters/ (test_tollwarden), profiles/ (player-file fixtures with canary strings), db/}  recordings/*.ensnap.zst (redacted)
              golden/{*.events.jsonl, CHANGES.md}  s6/*.toml  knowledge/{expect,cues}.toml
  tools/      workplan/
  dist/       README-install.md  using-a-loader.md  ai-disclosure.md
  docs/
    research/
    design/   DESIGN.md  architecture.md  npcs-and-dialogue.md  memory-and-knowledge.md  spikes/  process/
    authoring/
    ops/      patching.md  probes/<build>.json
  LICENSE (MIT)  THIRD_PARTY.md
```

**Not in the repository:**
- No FromSoftware material: no FMG text, params, audio or game binaries.
- Recordings committed under `tests/recordings/` contain no game text: they are written in redacted mode (§7.5), so names appear only as `fmg<fmg>:<id>` placeholders. They are made on a test account, with the name and SteamID hashed.
- Anything built from a player's own exports goes to git-ignored folders (`gamedata/`, `*.local.*`).
- The player's own profiles (`%APPDATA%\EldenNet\profiles\`) never enter the repository; the fixtures under `tests/fixtures/profiles/` are ours.
- `docs/ops/probes/` holds only our own probe records: offsets, invariants used, and sample numbers.
