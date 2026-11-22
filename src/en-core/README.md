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
  `false`, `!`, `&&`, `||`, parentheses and comparisons of `runes` (held) and `journey` (ng_lvl); `world()` and
  `knows()` refused in Augur, and `flag()`, `boss()`, `stage()` and `region()` until the catalogue and regions.toml
  exist; at most 1 KB and 32 deep; an error is a fixed text with its column, never a quote.

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
  (`DialogueContext::record`): the record with `stages` replaced by the active `stage`, its `road`, `at`, `hears` and
  `knows` inherited, and without what is never rendered (the stages' `forbid`, `when`, `chat`, `state`, `approach*`,
  `talked`, `awaits`, `beckon*`; the `seed` and `deflections`; awareness's `sleep_leads`, `skip`, `news`). Files
  outside `identity/` and `personas/` are kept as they are, the overrides applied (`ContentPack::others`), for part A's
  prompts and lexicons. The shipped index and records are built in (`builtin_content()`, as the rule files are); v0.2
  ships the test persona `lantern_keeper` (NPC §3.5, `test_only`) alone. Not yet: `overrides/disable.toml` (P3 §8.4),
  the seed's combat keys (NPC §9.11) and the alias table's use (§2.4).
- **Identity** (`identity.hpp`, `src/identity/`; O45, NPC §2.3). A character binds by its NpcParam row, else a non-zero
  entity id; never by name. `Realm::Game` (the DLL's) leaves `test_only` personas out whatever an override gives them,
  so they never resolve in game; `Realm::Eval` (the self-test, the eval, the tests) keeps them. The tests' overrides
  folder, `tests/scenarios/talk/overrides/`, binds the test persona to the fake game's invented row 81100. The
  directory compiles each stage's `when` with v0.1's predicates: the active stage is the last whose `when` holds, and a
  `when` they refuse (flag(), boss(), stage(), region() until the catalogue) is Unknown, so its stage is active only
  if it only restricts (§3.2). v0.2 reads no text or params: no `expect_name_id` check, and the name is the record's
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
    an end; the memory job waits for v0.3;
  - danger closes the box at once (the overlay answers `Close{Shut, draft}`, the draft kept for the target) while the
    pages pace on; a threat (the Tarnished down or riding, a boss, combat, hostiles near, a drop in HP) for 10 s ends
    the conversation; a menu or a hidden overlay closes the box and holds the pages instead (impl).
- **The event log** (`event_log.hpp`): the conversation answers its commits, and the core applies them in the same
  step to `MemoryLog`, v0.2's log in memory behind the store's interface (`EventStore`; the newest 4,096 events).
- **Toasts** (`toasts.hpp`): NPC §10.5's policy, in the core. The words (DESIGN §2.7's and ours, drafts until the owner
  approves them): "They have nothing to say to you." (2 s), "(Name will not speak with you)", "(Name is asleep.)",
  "(Name does not stir.)", "Not now: danger is near.", "The moment has passed.", "No answer: provider timed out", "API
  key rejected: open settings (F10)", "Provider rate limit, try again shortly", "No answer: the provider failed", "No
  answer: the conversation's prompt could not be made", "No answer: no model is ready", "Chat cannot take the keyboard
  just now.", "Conversation paused: part of the game EldenNet reads stopped working.", "Conversation closed after a
  fault in EldenNet."
- **The core** (`core.hpp`). `Core()` uses the built-in speech parts (`BuiltinSpeech`: part 0's stubs until part A's
  parts land), `Core(SpeechParts)` the caller's; `load_content(files, realm)` loads content (on any error the old
  stays); before any load no persona exists and no conversation opens. `directory()`, `log()` and `conversation()`
  are for the debug view and the tests.

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
overrides and the resolved record, and its errors (`tests/content/`); the fixture flag states and the truth tables'
shape, their comparison with known(P) skipped until S11.3.c (`tests/content/test_fixtures.cpp`); identity, the game's
realm refusing the test persona, and the active stage (`tests/identity/`); and the conversation on a simulated clock
(`tests/conversation/`, over `rig.hpp`): every §6.1 transition and timer, queue-don't-cancel, the cut-off, the endings,
danger and the draft, commit on display with its grace and a late skip, each clause of the gates alone, the target
order, the observer, the refusals, no request while gated (C4) and the cancels, the toasts' policy, and a whole
session through the core into its log.
