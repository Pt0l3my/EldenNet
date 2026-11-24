# en-core: the engine's core

The core of EldenNet's engine (ARCH §2.2), C++20, MIT: it steps on the inputs the shell drains (snapshots, lifecycle
messages, T1's banner, the tick) and answers what the shell does and what the player sees. Pure (ARCH §2.3): no I/O, no
threads, no clocks and no Win32; time is the snapshots' own `mono_ms` and the `now` the shell passes. Its static
library `en_core` is a module the DLL links (`en_dll_module`).

## The v0.1 slice (the first in-game build, part 5)

What it holds is the slice the spikes need (the lifecycle without the store, the offline and multiplayer gate, the
ladder with Dormant) and, since the first conversation (WHY§364), content, identity and the conversation, below. Barks,
memory and the store come later.

- **The gate** (`gate.hpp`, ARCH §5.1, DESIGN §3.4). `judge(snapshot, session_failed)` is the predicate: `session` Ok,
  no anti-cheat, and a solo world of one's own (Host, one's own world, no lobby, one entry in the player set) when the
  main player is there, else a quiet title (the title screen, no lobby). The first clause that fails is the verdict's
  reason (`Unclean`: `gap`, `session_failed`, `anticheat`, `not_solo`, `lobby`, `loading`). A value not read is never
  clean: `Unknown` is never Host, None or Title, and a `gap` snapshot is not clean. `Gate` adds the hysteresis: closed
  at first; closed on the first snapshot that is not clean (C4: within one snapshot period); open once every processed
  snapshot has been clean for 5,000 ms of mono time with no gap between two of them longer than 1,000 ms (a clock that
  goes back starts the hold again too). A `session` group that failed or tripped, seen in a snapshot's health or by its
  GroupTripped message, keeps the gate closed for the rest of the session.
- **The lifecycle** (`core.hpp`, ARCH §9). Phases `gate`, `banner`, `ready`, `title`, `playthrough`, `in_world` and
  `loading`, Dormant being the gate's orthogonal region:
  - DataReady → Ready and L2; T1's banner (`BootBanner`: unknown build, anti-cheat, not started, discovery failed,
    registration faulted, inert) → Banner and L1, for good.
  - Ready → Title at a quiet title screen, → Loading during a loading screen (a load then), → the world when the main
    player is already there (a very late load).
  - Title (or Ready, or Loading with no playthrough yet) → Playthrough → InWorld when the snapshot shows the main
    player in game with the structural groups' phase B passed, a PlaythroughReady has named the character (slot 0–9,
    the same slot as the snapshot's, a name), and the play time has risen since the previous snapshot. Without the
    store there is no database to open, so InWorld follows on the same step; with it, the store's `Opened` will.
  - InWorld → Loading when the main player is gone, the load counter moved or the screen is not InGame; → Title at the
    title screen (the playthrough ends; the same character again is a new playthrough).
  - Loading → InWorld once two consecutive snapshots show the world with the same load counter (or none) and the same
    character: the identity of a newer PlaythroughReady, or, without one, the same slot and class; another character
    → Playthrough.
  - What C2 would have said is rebuilt from the snapshots when its message was lost (ARCH §1.3): structural groups Ok in
    the Gate phase stand for DataReady (the capabilities from their health), and a structural group Tripped in a
    snapshot's health for its GroupTripped.
- **The ladder** (ARCH §6.8): `starting` until T1 has reached Ready or stopped, then L2, or L1 once a banner is up: T1's,
  a structural group tripped (`task_tripped`, or `session_failed` for `session`) or anti-cheat seen later (the banner
  stays and the gate stays closed). L1 is for the session. From L2, the model client's health (`proto::NetHealth`,
  which the shell passes when it changes) takes it to L4 with a healthy dialogue model, or L3 once an optional group
  has tripped (in a snapshot's health or by its GroupTripped), and back to L2 when the model is not healthy. "AI
  paused" waits for its setting. `ai_allowed` (C15) is the gate open at L2 or above; speech needs L3 or above.
- **What the player sees** (ARCH §5.4, §10.3): `banner_text(reason, build, detail)` gives the banner's words, and the
  badge "EldenNet paused (multiplayer)" is up while in a world with the gate closed for a reason of the session, until
  the gate has held its 5 s again. A loading screen closes the gate too (it is neither a world nor the title), but
  raises no badge: the badge is for another player, another world or a lobby (impl).

- **Augur** (part 11; en-augur, ARCH §8). The core links en-augur and runs it on every step, before anything that
  consumes events: the slice judges the input first, and the step's lifecycle changes (DataReady, PlaythroughReady,
  loading started and ended, the title, the gate, the ladder, and each read group's trip, once per group) are Augur's
  Lifecycle facts on the same step. `AugurView` is what en-core supplies Augur at v0.1: every character classifies as
  Mute and the Tarnished as Player until personas exist (v0.2); no persona perceives; `rng` is splitmix64's finaliser
  over the seed root (0 until the store). `world_facts()` gives the game's meanings (teams.toml's hostile set
  {6, 7, 27, 29}, the victory banners 1 to 4 and 22, the throw states 3 and 4 of a critical hit, the open world's areas
  60 and 61), each to verify in its spike. The shipped rule files, `content/eldenring/augur/*.toml`, are built in at
  configure time (`builtin_rule_files()`: v0.1 has no content loader); `load_rules` replaces them, keeping the old set
  on any error. `Outputs::events` holds the step's events in dispatch order (no subsystem subscribes yet).
- **Predicates** (`predicates.hpp`, DESIGN §10.4): the DSL as v0.1 evaluates it for Augur's `requires`: `true`,
  `false`, `!`, `&&`, `||`, parentheses and comparisons of `runes_held` (the runes held) and `journey` (ng_lvl);
  `world()` and `knows()` refused in Augur, and `flag()`, `boss()`, `stage()` and `region()`, which v0.1's predicates
  do not read; at most 1 KB and 32 deep; an error is a fixed text with its column, never a quote. `runes >= n` has one
  meaning everywhere, MEM §6.6's, the great runes through the catalogue's `runes.ge<n>` aliases (WHY§474): v0.1's
  predicates refuse it as they refuse `flag()`, so a persona's `when` on it is Unknown in the directory, and the
  knowledge's predicates (below) evaluate it.

`Core::step(CoreInput, Mono)` takes one input (`unique_ptr<WorldSnapshot>`, `Lifecycle`, `BootBanner` or `Tick`, and
since the first conversation a `proto::UiEvent`, a `proto::LlmEvent` or a `proto::NetHealth`, below) and answers
`Outputs`: the game commands (one `SetTracked{}` when the gate closes or a world is left, ARCH §5.2, §9),
`ai_allowed` for C15 (the gate open and L2: at the title it allows the wizard's and the self-test's calls, never
speech), the `State` (phase, level, gate and its reason, Dormant, badge, banner, counts) and the step's `Change`s
(phase, gate opened or closed with its reason, level, banner, badge) for the log, the recording and the status block,
and, for the first conversation, `ui` (the `proto::UiModel` the surfaces show, C6), `requests` (the `LlmRequest`s to
submit, C9; none while `ai_allowed` is false, C4), `cancels` (the request ids to cancel), `commits` (the step's, already
in the core's event log) and `ended` (a conversation ended on the step).
The player's character's name stays inside the core, in the playthrough's identity; nothing here writes it.

```cpp
#include "en/core/core.hpp"

en::core::Core core;
auto out = core.step(en::core::CoreInput{std::make_unique<en::proto::WorldSnapshot>(snapshot)}, now_ms);
if (!out.ai_allowed) net.set_gate_open(false);           // C15
for (auto& c : out.game) commands.try_push(std::move(c));  // C4
```

## The first conversation: the interfaces (part 0, WHY§364)

v0.2's talk with one NPC is built in four parts: part 0, these interfaces; then part A (speech and the prompt) and part
B (the conversation and the eval) at once, each against the other's interfaces and stubs; then part C (the surfaces in
the DLL). A reply's path, in NPC §8.2's order, and who owns each step:

```text
UiEvent Submit    ─► Conversation (B) ─► PromptRenderer (A) ─► dialogue_request() ─► Outputs::requests ─► en-llm
LlmEvent Delta    ─► Conversation (B) ─► SentenceStream (A) ─► ReplyLint (A) ─► ReplyPager (A)
                     Conversation (B) ◄─ Sealing: a page, when the conversation needs one
                     Conversation (B) ─► Outputs::ui (its PageView) ─► the surfaces (C)
UiEvent PageShown ─► Conversation (B): commit on display
```

- **en-proto's `ui.hpp`** (part 0, then C's to read and send): `UiModel`, what the surfaces show after a step, and
  `UiEvent`, what they send back, with JSON forms carrying a version (en-proto's README). `ChatRequest::stop` carries
  the stop sequences, which en-llm sends as `stop` in a full body; `NetHealth` is the model client's health.
- **`core.hpp`** (part 0, then B's): `CoreInput` takes the three new kinds; `Outputs` gains `ui`, `requests` and
  `cancels`. Until B wires the conversation in, `core.cpp` takes each new input as a tick, so v0.1's behaviour and its
  tests are unchanged (`test_inputs.cpp`).
- **`speech.hpp`** (A's): three factories, each opening one object per reply that holds its state:
  - `Sanitizer` opens a `SentenceStream`: deltas in, `Sentence`s out (NPC §8.2 steps 0 to 7), each with what was
    removed and why (`Removal`: the reason and its bytes, never the text), the stream's `Flow` (more, the intent only,
    or stop and cancel), `withdraw()` for en-llm's Reset and the trailer's `intent()`;
  - `Lint` opens a `ReplyLint` from a `LintContext` (the regeneration flag, the safety level, the register, openers,
    tics, the names this turn shows, the told name and the last reply's `LintTally`): each sentence in, a `LintVerdict`
    out (pass, drop, regenerate with the hard list that hit, deflect, or stop at the previous sentence; §8.3, §8.4), and
    the reply's tally after;
  - `Pager` opens a `ReplyPager` from the persona's word cap: sentences in (`Take`: kept, the reply full, or over its
    caps, §8.5), and pages out when the conversation needs one (`Sealing`: a page with its hold and whether it is the
    last, a wait with page 1's fill deadline, or done; §8.6).
- **`prompt.hpp`** (A's): `PromptRenderer::render(DialogueContext)` gives the messages with their cache marks and the
  token estimate, or a `PromptError` (no record, a template's failure, over the hard cap); a failed render sends
  nothing (NPC §7.1). The context holds the persona's record as the content pack holds it (JSON, its stage resolved),
  the name as the game shows it, history, the scene (block 9), the recap and the reminder's list. `kDialogueStops` and
  `dialogue_request()` make the request NPC §7.3 asks for.
- **`conversation.hpp`** (B's): `Conversation`, the state machine of NPC §6.1 as a pure object, holding the parts as
  `SpeechParts` (references) and stepped with `ConversationInput` (the time, what the core knows of the world, and the
  step's surface or model event), answering `ConversationOutput` (the UI model, requests, cancels and whether it
  ended). Part 0's stub never opens.
- **`stubs.hpp`** (part 0; each interface's owner keeps its stub working): `StubSanitizer` (sentences at `.`, `!`,
  `?`), `StubLint` (passes all but a marker word, with §8.4's three actions for it), `StubPager` (a page per sentence,
  2 s, three sentences at most), `StubPrompt` (a frame and the recap, or the refusal it is told) and `StubSpeech`, the
  four as `SpeechParts`.

**Who owns which file.** Part A: `speech.hpp`, `prompt.hpp`, `src/speech/`, `src/prompt/`, `tests/speech/` and
`tests/prompt/` here, and `content/eldenring/{prompts,lexicon}/`, the repository's `tests/fixtures/` (but its
`flagstates/`), en-tools' `render_prompt.cpp` and `golden.cpp`, and `fuzz/`. Part B: `conversation.hpp`, `core.hpp`,
`core.cpp`, `src/content/`, `src/identity/`, `src/conversation/` and `tests/conversation/` here, and en-tools' `eval`
and `mockllm`, `content/eldenring/{identity,personas}/`, and the repository's `tests/scenarios/talk/`,
`tests/knowledge/` and `tests/fixtures/flagstates/`. `stubs.hpp`, this README's part 0 section and en-core's
`CMakeLists.txt` are shared: its globs take every `.cpp` under `src/` and `tests/`, so a new file needs no edit, and en-prompt is linked
already. A context struct of `speech.hpp` or `prompt.hpp` may gain a field with a default that changes nothing; a
signature, an enum's meaning or a stub's behaviour changes only by agreement between A and B. Part C reads `Outputs`
(`ui`, `requests`, `cancels`) in the DLL and sends `UiEvent`s, LLM events and `NetHealth` in; it changes `ui.hpp` only
by agreement, with its JSON version raised when a form changes.

**What A fills in:** the sanitizer's steps 0 to 7 (NFC, the 4 KB cap, reasoning tags, the trailer, the run-on, markup,
the frame, the whitelist, the splitter's boundaries), the hard and soft lists with their data files, the caps, the
splitter and the lazy pager with the hold formula, and the renderer over en-prompt's `MemorySource` with blocks 1 to
11, their caps and the trim order. **What B fills in:** the conversation's states and timers, the target order and the
gates (`conversable`, `safe_context`, the native-talk observer's fail-closed rule), queue-don't-cancel, the endings,
the draft kept per target, toasts, commit on display into a RAM event log behind the store's interface (WHY§364 (7)),
L3 and L4 from `NetHealth`, the core's wiring (`core.hpp`, `core.cpp`) and the eval.


## The first conversation: part B (WHY§364)

Content, identity, the conversation and the core's wiring; the eval is en-tools' (its README, "eval and mockllm").

- **Content** (`content.hpp`, `src/content/`; NPC §2.1, §3.1, §3.2; DESIGN §11.4). `load_content(files)` reads the
  persona index (`identity/personas.toml`) and each enabled row's record (`personas/<id>.toml`) from the files the
  shell hands over: the shipped pack's, then the overrides folder's, each of which replaces the shipped file of its
  path whole. Every file passes en-proto's `unsafe_for_toml` first (so a record has no multi-line string), its cap
  (64 KB, a persona's 16 KB) and a schema of ours written from NPC §3.1's table: unknown keys, types, the closed values,
  counts and caps in words and characters, the base stage's `when = "true"` with its `road`, `at` and `state`, no
  `unlock` on it, `beckon` with `beckon_src`, a `ci:` source only on a test persona, ids unique, a param row or an
  entity id bound to one persona, the record's id its row's. An error names the file, the line and the key in fixed
  words and never quotes a value; at most 64, then a count. The record is kept as JSON (`Persona::record`) beside the
  fields the conversation and the lint act on; `Persona::resolved(stage)` is what part A's renderer takes
  (`DialogueContext::record`): exactly the fields it reads (prompt.hpp's `kRecordFields`), as the file holds them,
  with `stage` the active stage, its `road`, `at`, `hears` and `knows` inherited; nothing else is kept, so what is
  never rendered (the stages' `forbid`, `when`, gate fields and `beckon*`, the id, `seed` and `deflections`, knowledge
  and awareness) never reaches it. Files outside `identity/` and `personas/` are kept as they are, the overrides
  applied (`ContentPack::others`), for part A's prompts and lexicons (`speech_content.hpp`, below). The shipped index and records are built in (`builtin_content()`, as the rule files are); v0.2
  ships the pilot, Kalé (NPC §3.4; below), and the test persona `lantern_keeper` (NPC §3.5, `test_only`); a shipped
  persona that is no test persona records its provenance, `drafted` and `review` (PL12). The overrides folder's
  `disable.toml` (ARCH §11.6, DESIGN §11.4; S11.3.g) turns personas of the index off by id: their rows read as not
  enabled, so no record is read for them and the index, the directory and the knowledge are built without them
  (`ContentPack::disabled_personas`); its schema is checked as any content's (an unknown key or a wrong type refuses the
  pack), and an id that names nothing is a warning (`ContentPack::warnings`, then `Core::content_warnings()`), never an
  error, by its list and place, never the id: every card's, since v0.2 ships no cards, and one for each of
  `characters`, `kb`, `triggers` and `rules`, which v0.2 does not act on yet. Not yet: the seed's combat keys
  (NPC §9.11) and the alias table's use (§2.4).
- **Content packs' projection** (`content_view.hpp`, `src/content/view.cpp`; PAGES §4.6.9, S11.4.a). `record_load`
  writes one load into en-proto's `ContentView`, which en-overlay's space publishes: the shipped cast in the realm
  (`shipped_index`, the shipped index alone whatever the overrides hold; test personas only in the tests' realm),
  whether the shipped pack holds knowledge and event rules (told as included, never counted, SP5); each override file
  by its path relative to the folder (en-proto's `shown_path`, PV5) with its state at that load (Loaded, a layer Refused
  alone, or after a refused load Refused for the files its errors name and Waiting for the rest) and its problems by
  line in the content layer's fixed words (a layer's from `layer_findings()`, the rest from the load's errors or
  `content_warnings()`); the errors that name no file of the folder on a row of their own; disable.toml's personas from
  the content in use; and the load itself, newest first (20 kept). `record_left_out` takes what the shell left out of
  the folder. Never a value of a file.
- **Identity** (`identity.hpp`, `src/identity/`; O45, NPC §2.3). A character binds by its NpcParam row, else a non-zero
  entity id; never by name. `Realm::Game` (the DLL's) leaves `test_only` personas out whatever an override gives them,
  so they never resolve in game; `Realm::Eval` (the self-test, the eval, the tests) keeps them. The tests' overrides
  folder, `tests/scenarios/talk/overrides/`, binds the test persona to the fake game's invented row 81100. The
  directory compiles each stage's `when` with v0.1's predicates: the active stage is the last whose `when` holds, and a
  `when` they refuse (flag(), boss(), stage(), region()) is Unknown, so its stage is active only
  if it only restricts (§3.2); the core's conversation takes the stage from the knowledge's view instead (below), whose
  predicates read the catalogue. v0.2 reads no text or params: no `expect_name_id` check, and the name is the record's
  `display_name_fallback`.
- **The conversation** (`conversation.hpp`, `src/conversation/`; NPC §5, §6, §8.4, §8.7, §10.5; DESIGN §8). A pure
  object the core steps after Augur on every input, with the newest snapshot, the lifecycle, the ladder and the
  Tarnished's engagements from Augur's events:
  - the talk key's target by rules 3 to 5 of §5.2 (rules 0 to 2 wait for own-initiative lines and S3, 6 for v0.6),
    the hint's 500 ms lead; `conversable` (§5.3) and `safe_context` (§5.4: L3 or above, a world, not dormant, the
    Tarnished up and on foot, no boss, no combat, no hostile within 12 m or an engaged one within 25 m, both waived in
    the Roundtable, the HUD at Default with no banner); the talk observer failing closed (DESIGN §8.7, §5.8: the HUD at
    Default, 3 s free of the pop-up's talk id, the talk camera and the HUD at PopupMenu, 3 s since the persona came
    within 8 m); the refusals' toasts (§5.1);
  - NPC §6.1's states (Closed, Open, Awaiting, Streaming, Paging, Held, Ending) and §6.2's timers: the card at 350 ms,
    20 s from the line to the "…" page, the cut-off's 2 s, the native hold's 1.5 s and 10 s and the hidden's 30 s, 12 m
    for 2 s, danger's 10 s, 120 s idle, 2 s unresolvable, the draft's 60 s, 3 s of quiet barks; queue-don't-cancel
    (§6.3); one request in flight, ids from `kConversationRequests` (1 << 48) up; the lint's regeneration once, then
    the persona's deflection (§8.4); a failed request or render: the "…" page, never committed, with its class's toast;
  - commit on display (§8.7): the first PageShown of a reply's page creates its NpcSaid and each later one grows it;
    a page never acknowledged is never committed; acks of the last reply are taken 500 ms after an end; a duplicate is
    ignored; Tab ends a hold no sooner than 300 ms after its first frame, and a skip names its page, so a late one
    never ends the next; the history strip and the next prompt read only committed text;
  - the endings (§5.7), each committed as ConversationEnded with its counts; the input epoch moves on an opening and
    an end; the memory job waits for v0.3. Esc acts on the box only while it is open (WHY§461; the owner, GraceUI's
    G39 (c)): with nothing pending it ends the conversation, with a reply pending it closes the box and the reply
    paces out, then the conversation ends; there is no second Esc, and an Esc that reaches the core with the box
    closed changes nothing. The box opened again on the same target while the reply paces keeps the conversation;
  - danger closes the box at once (the overlay answers `Close{Shut, draft}`, the draft kept for the target) while the
    pages pace on; a threat (the Tarnished down or riding, a boss, combat, hostiles near, a drop in HP) for 10 s ends
    the conversation; a menu or a hidden overlay closes the box and holds the pages instead (impl).
- **The event log** (`event_log.hpp`): the conversation answers its commits, and the core applies them in the same
  step to `MemoryLog`, v0.2's log in memory behind the store's interface (`EventStore`; the newest 4,096 events).
- **Toasts** (`toasts.hpp`): NPC §10.5's policy, in the core. The words (DESIGN §2.7's and ours, drafts until the owner
  approves them): "They have nothing to say to you." (2 s), "(Name will not speak with you)", "(Name is asleep.)",
  "(Name does not stir.)", "Not now: danger is near.", "The moment has passed.", "No answer: provider timed out", "API
  key rejected: paste it again on AI › Providers" (O120), "Provider rate limit, try again shortly", "No answer: the
  provider failed", "No answer: the conversation's prompt could not be made", "No answer: no model is ready", "Chat
  cannot take the keyboard just now.", "Conversation paused: part of the game EldenNet reads stopped working.", "Conversation closed after a
  fault in EldenNet." A box GraceUI refused names the holder by the id GraceUI gives (WHY§442): "Chat cannot take the
  keyboard: GraceUI's menu has the controls." (`graceui`), "Chat cannot take the keyboard: another mod (‹id›) has the
  controls." (an id as GraceUI checks one: 3 to 32 of a-z, 0-9 and '-'), "Chat cannot take the keyboard: another mod
  has the controls." (anything else); a not-now box its reason: "Chat cannot take the keyboard: danger is near.",
  "Chat cannot take the keyboard while the game loads.", "Chat cannot take the keyboard while a game menu is open.",
  and the generic words for not ready or any other.
- **The core** (`core.hpp`). `Core()` uses the built-in speech parts (`BuiltinSpeech`: part A's, below),
  `Core(SpeechParts)` the caller's (the tests' stubs); `load_content(files, realm)` loads content (on any error the old
  stays); before any load no persona exists and no conversation opens. `directory()`, `log()` and `conversation()`
  are for the debug view and the tests.

## The first conversation at run time: parts A and B together (WHY§364, WHY§388)

How the pieces fit once joined. Speech and the prompt are part A's (`src/speech/README.md`, `src/prompt/README.md`),
the conversation part B's (above); the core holds both.

- **Loading.** The shell hands `Core::load_content` the shipped files and then the overrides folder's. The shipped
  pack is built in at configure time (`builtin_content()`: `identity/`, `personas/`, `lexicon/*.toml` and
  `prompts/*/*.md|j2`), so the DLL and the Windows tests read no path for it. `load_content` (content.hpp) checks the
  index and the records and keeps every other file as `others`, the overrides applied; `load_speech`
  (`speech_content.hpp`) then loads `prompts/` into a `DialoguePrompt` (every block's part, the five registers, a clean
  lint) and `lexicon/banned.toml` and `safety.toml` into a `SpeechLint`, and runs block 2's content lint: each enabled
  persona's block 2, rendered alone at each of its stages, past its 120 tokens is an error for shipped content and a
  `test_only` persona, and a warning for the player's own record (`content_warnings()`), which still renders. Any error
  refuses the whole pack and the old content stays, whichever speech parts the core was given. On success the
  built-in lint and prompt forward to the new pair; a reply already open keeps the lexicon it opened with.
- **A line.** Submit commits PlayerSaid and opens the reply's three objects: the `SentenceStream` (the persona's own
  names, for step 5's "Name:"), the `ReplyLint` (its openers, its tics with their `max_per_conversation`, its names and
  forms of address as the turn's proper nouns, its own names for third-person self-reference, and the tally carried
  from the last reply) and the `ReplyPager` (its `reply_words_max`). The prompt is rendered from
  `Persona::resolved(stage)`, which is exactly the fields the renderer reads (`kRecordFields`), with this
  conversation's committed history, the previous one's last lines, the recap, the scene (block 9's time of day), the
  regeneration's reminder and the tally; block 5b's `tied` and `land` come from the knowledge's view (below), and block 9 has the "you, now" line. `dialogue_request()`
  makes the request (the dialogue slot, the two stop sequences), and `Outputs::requests` carries it. A render that
  fails sends nothing: the "…" page and its toast.
- **The stream.** Each Delta goes to `feed()`, which gives a sentence out one delta after its boundary; Ended's
  `finish()` gives the last. Each sentence goes through the lint, in order: `Pass` into the pager; `Drop` (a
  duplicate, or one the opener rule emptied) left out; `Regenerate` (a hard hit in the first sentence of a first try)
  cancels the stream and sends the line again once, with block 11's reminder for the list that hit and a fresh stream,
  lint and pager, within the same 20 s; `Deflect` (the regeneration's first sentence hits too) cancels it and pages the
  persona's least recently used deflection (`DeflectionRing`, one per persona for the session; the store keeps its
  order from v0.3); `Stop` (a hit later) cancels it, and the reply ends at the sentence before. The sanitizer's
  `Flow::Stop` (a run-on, the 4 KB cap) cancels too; `IntentOnly` (a trailer) ends the sentences and reads on for the
  intent. The pager's `Take::Full` or `Take::Over`, whichever comes first, fills the reply and cancels the stream.
- **Pages.** Each step pumps the reply: page 1 is asked for once a sentence is in (the pager may wait 250 ms for a
  second that fits), and the next when a page's hold ends (`clamp(1200 + 55 × chars, 2000, 6500)`, 400 ms more after
  "..."; Tab's Skip no sooner than 300 ms after its first frame). `Page::last` is set only on a page sealed after the
  reply ended or filled. A page sealed while the reply still streamed is not, so once no sentence follows, the
  conversation asks the pager for the page after the one up: nothing left, and the page up is marked last, so a queued
  line goes as soon as it shows (at once if it has), not when its hold ends (NPC §6.3, WHY§432); a page left is sealed
  then, which, with no sentence to come, is the page the hold's end would seal. A page's first `PageShown` commits it
  (commit on display).
- **The tally.** Whenever a reply ends (its last page, a cut-off, a failure, a hold dropped), the conversation keeps
  its lint's tally (the opener used, each tic's count, the tic spent most recently, the told name's uses) and hands it
  to the next reply's lint and prompt: block 11 notes that one tic, the newest, beside the reminder (70 tokens at most).
- **The stubs** (`stubs.hpp`) stay for the tests that want a reply's path without the real rules: the conversation's
  rig and `test_core_wiring.cpp` build a `Conversation` or `Core(SpeechParts)` on `StubSpeech`.

## Quest and world awareness, and the pilot (S11.3.c, d; WHY§364)

- **The pilot** (`content/eldenring/personas/kale.toml`, NPC §3.4): Kalé, a travelling merchant met early, in our own
  words from the design and our notes, `review = "pending"` until the owner reads it field by field. His index row holds
  the design's leads (NPC §2.1: an NpcParam row and his flag block, unverified; the doubtful entity lead is left out);
  his one later stage, `howl_told`, waits on a placeholder alias, unresolved in the catalogue, so it never shows in game
  before S6. His voice is shorter than the design's draft, so that block 4 keeps within its 230 tokens.
- **The world files** (`knowledge.hpp`): `flags/catalog.toml` (MEM §6.1: the aliases v0.2's content uses, each a lead,
  `verified = false`), `regions.toml` (NPC §9.5's keys and Sky column), `world/stages.toml` (MEM §12.4's nine stages,
  their lines our own) and `world/tracks.toml` (Tanith's two steps). `Knowledge::build` checks them and compiles every
  predicate (MEM §6.5: an unknown alias, a number in `flag()`, `knows()`, a world stage's `!` or `stage()` (W1), a
  cycle, a region or a world stage that does not exist are load errors, in fixed words); its errors refuse the pack.
- **Evaluation** (MEM §6.6, §12.3, §12.5, §12.6): three values, Kleene's; an alias Unknown before the playthrough's
  first frame, unverified or invalid; a world stage of spoiler ≥ 2 holds only with every flag atom verified (§7.4);
  each persona's active stage by NPC §3.2; `known(P)` by the six paths, absence silent; `ground()` block 5b's lines
  (own affairs, then tied, then rank; four at most within 200 tokens; the rest kept by id for block 7's topic offers,
  v0.3), the persona's tie line before the world's. No prompt names what a persona must not say, and nothing filters
  model text (O36, O86).
- **The core** (`core.hpp`): a load builds the knowledge and a watch list of the catalogue's verified, resolved
  aliases for the realm; the next step sends it as SetWatchFlags if it is not empty. In game nothing is verified before
  S6, so nothing is sent and every alias stays Unknown: the pilot speaks from his road alone. A `ci:` evidence (test
  content) verifies in the eval's realm only: `tests/scenarios/talk/pilot-overrides/` gives the tests and the eval a
  catalogue of invented ids so, and binds the pilot to the fake game's invented row 81200. A FlagFrame of the list's
  version gives the flag state; a change of it, the view (`known_view()`) the conversation reads: the active stage and
  block 5b. A new playthrough and the title forget the frame.
- **The "you, now" line** (NPC §7.3; O35): `you_now()`, the partner's wounds by DESIGN §5.5's bands and "in the thick of
  a fight" when the Tarnished struck it in the last 8 s; nothing when calm; statuses wait for S8.
- **Behaviour** (`behaviour.hpp`, ARCH §11.5): `features` and `range` with their defaults and bounds, set by
  `Core::set_behaviour` (one out of bounds refused, the old kept). v0.2 reads `features.chat` (off: no hint, the key
  opens nothing, an open conversation ends `closed` at the next step) and `features.self_awareness` (off: no "you, now"
  line); the rest wait for the lines they govern. The DLL's settings.toml reader hands them over at the start and a
  pages' change of a `features.*` or `range.*` key at the next step (src/eldennet/README.md).
- **Content lints at load** (speech_content.hpp): block 2 and block 4 within their caps, and PL4's and PL5's wording
  (banned words in prompt-facing fields and stage texts, quoted sample lines, subjects and stirs as noun phrases;
  effect adjectives warned), errors for shipped and test content, warnings for the player's own.

## The player's own layer and the draft helper (`profile.hpp`; NPC §4.5, §4.7, §4.8; S7.2.b)

- **The file.** `layers/persona-<id>.toml` in the overrides folder (NPC §4.7's name for it), the player's alone: a
  shipped one refuses the pack. `schema = 1`, `target = "persona:<id>"` (the path's), `rev`, `base_hash` (the shipped
  record's FNV-1a, `record_hash`), `drafted = "assisted"` once a draft was taken (PL12), and the four fields a layer may
  set: `instructions`, `subjects` (in place of the shipped list), `register` and `reply_words_max`. `layer_text` writes
  what `read_layer` reads back; a line break or a tab in the text becomes a space (one paragraph).
- **The load.** `load_content` reads each layer with PL1's schema and caps into `ContentPack::layers`, its findings into
  `layer_findings`: an error refuses that layer alone, never the pack. The core, once the speech parts load, runs
  `apply_layers`: the wording of the notes and the subjects against the pack's lexicon (template marks and the safety
  list's fingerprints errors; a banned word, a quoted line, an effect word and a subject that is no noun phrase
  warnings) and block 4 rendered with the layer (past its 230 a warning for the player's own, WHY§472: the pilot's voice
  leaves about six tokens). A layer with no error is merged into its persona (`Persona::layer`; the typed register and
  reply length take its values; `resolved()` renders its instructions and register); `Persona::record` stays the shipped
  file, so the shipped lints never see the player's text (`resolved(i, false)` for them). Every finding joins the
  content's warnings (`to_content_error`, "refused: " before an error's words) and `Core::layer_findings()`.
- **The editor's check.** `check_layer(files, candidate)` runs the same load over the shell's files with the candidate
  in place of the file of its path, and answers the candidate's findings alone (`ContentDown` when nothing loads);
  `words()` gives each its words for the editor, never a value of the file.
- **The draft helper.** `draft_messages` renders `prompts/draft/10_profile.j2` (ours) from the player's notes (400
  characters at most), a fixed phrase of the profile's kind and the player's own current notes and subjects, nothing
  else; `draft_schema()` holds the reply to the two fields. `read_draft` keeps them within their caps (cut at a
  sentence's end; subjects past ten words or the eighth left out), drops a sentence or a subject naming someone neither
  the notes nor the current text hold (a capitalised word that is not the first; "I" is no name), and discards the whole
  draft on a word of the safety list.

## Tests

`en_core_tests` (doctest), on Linux with GCC and Clang and under Wine: the predicate's every clause alone, values not
read, the title clause; the hold of exactly 5,000 ms at 4 Hz, a gap of 1,000 ms (none) and 1,001 ms (the hold again),
a clock that goes back, a `gap` snapshot, the session failing for good; boot to the title, the title's gate (open,
`ai_allowed`, still Dormant); each condition of a playthrough's start alone (no identity, phase B, the play time not
rising, another slot, a slot past 9, an empty name); co-op closing the gate at once with the badge and one
`SetTracked{}`, the badge kept through the 5 s wait; each sign of a loading screen, the counter held over two
snapshots, the same character back, another character (Loading → Playthrough), a loading screen with no new identity;
the title ending a playthrough; very late loads in the world and during a loading screen; T1's banners and their texts;
optional and structural trips; the session's trip; C2's messages rebuilt from health; a later anti-cheat hit; a null
snapshot and a tick. And Augur's wiring (`test_augur_wiring.cpp`): the built-in set loads, the slice's changes reach
Augur on the same step, every subject is Mute or Player, v0.1's predicates and their refusals, and a failed reload.
The first conversation's part 0: with no content loaded, every new input kind but the model client's health in place of
a tick leaves a whole session as it was, with a closed box and no request, and the health moves the ladder to L3 and L4
and back (`test_inputs.cpp`); and each stub does what `stubs.hpp` says, the dialogue request carries the two stop
sequences and a conversation with no world never opens (`test_interfaces.cpp`). Part B: the content's loading, the
overrides and the resolved record (exactly `kRecordFields`, the stage's inheritance applied), and its errors
(`tests/content/`); the pack's speech: the shipped prompts and lexicon loading with it, a pack whose prompts or lexicon
fail refused with the old content kept, and block 2's content lint, an error for shipped and test content and a
warning for the player's own (`tests/content/test_speech_content.cpp`); the fixture flag states and the truth tables'
shape, and T2: known(P) equal to them for both personas at the eight states (`tests/content/test_fixtures.cpp`); identity, the game's
realm refusing the test persona, and the active stage (`tests/identity/`); and the conversation on a simulated clock
(`tests/conversation/`, over `rig.hpp`): every §6.1 transition and timer, queue-don't-cancel, the cut-off, the endings,
danger and the draft, commit on display with its grace and a late skip, each clause of the gates alone, the target
order, the observer, the refusals, no request while gated (C4) and the cancels, the toasts' policy, and a whole
session through the core into its log. Parts A and B joined (`tests/conversation/test_core_speech.cpp`), on the core's
own speech: reasoning, a stage direction and the speaker's name taken out before a reply shows; a banned word
regenerating once with block 11's reminder, then deflecting; the pager's cap filling a reply and cancelling the
stream; a tic spent in one reply noted in the next prompt. Quest and world awareness (`tests/knowledge/`): the shipped
world files and their leads, nothing holding before a frame, the pilot's block 5b at `after_radahn` as NPC §3.6 renders
it and at `erdtree_burned` with a line offered, strict mode and MEM §7.4, the predicates' values and load errors, the
howl stage, Tanith's track, the watch list and a frame (a `ci:` record in the eval's realm alone), T5's byte stability
over the eight states, the developer-label firewall (no alias and no four-word run of a note in the pilot's prompts),
the "you, now" bands and the behaviour bounds, and through the core: the watch list sent once, a frame grounding the
pilot's prompt, a bloodied pilot, self-awareness and chat switched off. The first conversation's polish (WHY§432, §442,
§461, §474; S11.3.g): no second Esc, and a box opened again while its reply paces; a queued line sent when the stream ends with a
one-page reply's page up, before or after it showed, and after a later page sealed while streaming; the refusal's and
not-now's words, an id GraceUI would not give never repeated; `runes_held` in v0.1's predicates and `runes >= n`
refused there, a stage on it Unknown in the directory (`tests/identity/`); and disable.toml
(`tests/content/test_disable.cpp`): a persona off in the pack, the index and the core, the warnings that never quote an
id, and a file of the wrong shape refusing the pack with the old content kept; Content packs' projection
(`tests/content/test_view.cpp`): the realm's cast whatever the overrides hold, warnings by line, a layer refused alone,
a refused load's file and line with the rest waiting and the earlier content's turned-off list kept, the errors of no
file of the folder and an unfit path on a row of their own, what the shell left out, the reloads' cap, and no value of
a file anywhere in it.
