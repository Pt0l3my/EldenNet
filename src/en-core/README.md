# en-core: the engine's core

The core of EldenNet's engine (ARCH §2.2), C++20, MIT: it steps on the inputs the shell drains (snapshots, lifecycle
messages, T1's banner, the tick) and answers what the shell does and what the player sees. Pure (ARCH §2.3): no I/O, no
threads, no clocks and no Win32; time is the snapshots' own `mono_ms` and the `now` the shell passes. Its static
library `en_core` is a module the DLL links (`en_dll_module`).

## The v0.1 slice (the first in-game build, part 5)

What it holds now is the slice the spikes need: the lifecycle without the store, the offline and multiplayer gate, and
the ladder's first levels with Dormant. Identity, content, conversations, barks, memory and prompts come later.

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
  stays and the gate stays closed). L1 is for the session. An optional group's trip changes nothing here: L3 and L4
  need the provider's health and come with the net.
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

`Core::step(CoreInput, Mono)` takes one input (`unique_ptr<WorldSnapshot>`, `Lifecycle`, `BootBanner` or `Tick`) and
answers `Outputs`: the game commands (one `SetTracked{}` when the gate closes or a world is left, ARCH §5.2, §9),
`ai_allowed` for C15 (the gate open and L2: at the title it allows the wizard's and the self-test's calls, never
speech), the `State` (phase, level, gate and its reason, Dormant, badge, banner, counts) and the step's `Change`s
(phase, gate opened or closed with its reason, level, banner, badge) for the log, the recording and the status block.
The player's character's name stays inside the core, in the playthrough's identity; nothing here writes it.

```cpp
#include "en/core/core.hpp"

en::core::Core core;
auto out = core.step(en::core::CoreInput{std::make_unique<en::proto::WorldSnapshot>(snapshot)}, now_ms);
if (!out.ai_allowed) net.set_gate_open(false);           // C15
for (auto& c : out.game) commands.try_push(std::move(c));  // C4
```

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
