# Proposal 0001: Plugins for EldenNet

**Status: proposal, 2026-11-15 (roadmap S7.10.a). O65 to O69 answered by the owner the same day, "all five as
recommended", and folded into EldenNet's and GraceUI's designs (S7.10.c); the dashboard's upload route decided as
GraceUI's G28 (a), a narrow route on the loopback listener (GraceUI §7.2 item 18). §13 added the same day, with O70
decided as the owner designed it, and O71 and O72 Open.** Where this text and the designs differ, the designs hold. Each
answer's effect is stated with its question (§12). What it settles itself is listed in §10, each row open to the owner.
Evidence: SkyrimNet's published plugin headers (MIT; WHY§87), ER02 §7.1 on hook libraries (WHY§86), ARCH §11.6 on
content layers, and GraceUI's DESIGN §2.9 and §7. A bare §n is a section of this proposal; DESIGN, ARCH and NPC are
EldenNet's design documents, and "GraceUI §n" and G- and D-numbers are GraceUI's.

## 1. What the owner asked

After the design fold made GraceUI the one owner of the overlay's hooks (GraceUI G15), the owner asked whether
EldenNet could hook too, and then, on 2026-11-15:

> do you mean we will be able to supply a layer for plugins to be added to EldenNet so they can hook directly into
> EldenNet (or vice versa?) for simple and fast plugin installation? I imagine something like installing a plugin (as
> folder or compressed format) into EldenNet's Plugin folder, and that's it, when game launches it auto installs. Or
> perhaps this could be done whilst the game is running, on http UI, install into a drop zone or add as compressed file
> manually, then the plugins gets installed and asks for a game restart to finish (same for uninstall). Or perhaps both
> routes? what do you think?

On EldenNet's own hooks the owner said "I lay towards (a), but I want to discuss this further", and then "yes, write
the plugins proposal".

## 2. Two meanings of "hook"

- **Patching** is rewriting the first bytes of a function so that it jumps into a mod's code. A mod needs it only to
  watch or change the game itself, or the screen and the keys. This is what EldenNet's R12 and GraceUI's invariant 16
  are about.
- **Plugging in** is calling an interface another mod publishes. Nothing is patched. SkyrimNet's plugins work this
  way: a plugin finds SkyrimNet's DLL at run time and calls the 45 functions its header names (WHY§87). Nothing hooks
  SkyrimNet, and SkyrimNet calls its plugins back through the same interface. EldenNet can offer the same, in the shape
  GraceUI's interface already has (GraceUI D70, D73).
- **What the evidence says about patching.** The conflict modders report is "two MinHook instances detouring one
  prologue and corrupting each other's trampolines" (ER02 §7.1), and two overlays also fight over the screen and the
  keys. So what must have one owner is each hooked function, not the right to hook (WHY§86). GraceUI owns the
  functions the overlay and input need. Other functions can belong to someone else.

## 3. EldenNet's own hooks (O65)

The fold wrote that no consumer of GraceUI may link any hook library (GraceUI invariant 16, DESIGN R12). That is
stricter than §2's evidence needs, and it leaves EldenNet no way to add the hooks its own design keeps open: the
hook-based event sources of O49 (ARCH §8.11) and the save redirect of DESIGN §3.6.

- **(a) EldenNet's own hook library, for other functions only** (recommended). EldenNet may link one hook library of
  its own and hook game functions, and system functions GraceUI does not hook. Its rules:
  - never a function GraceUI hooks (GraceUI publishes the list: the swap chain's `Present`, `Present1`,
    `ResizeBuffers` and `ResizeBuffers1`, the command queue's `ExecuteCommandLists`, the game's window procedure,
    DirectInput's `GetDeviceState` and XInput's `XInputGetState`; ER02 §7.1, GraceUI D69);
  - each hook installed once, after `connect` has answered (so GraceUI's are in place), chained, and never removed,
    as R12 already says for GraceUI's;
  - each hook still needs its own spike and the owner's approval before it ships, because patching game code is
    what R2 and Arxan's checks are about (ARCH §8.11);
  - the library is chosen when the first hook is designed, its licence checked first (under O60's C++, MinHook
    itself is the likely one, linked into `eldennet.dll`).
- **(b) GraceUI installs hooks for EldenNet** through its interface. One hook library stays in the process, but
  GraceUI would then patch game code for other mods, which breaks its read-only rule, and EldenNet's hook features
  would wait for GraceUI's releases.
- **(c) Decide per feature**, when the first hook is proposed.

Nothing in v1 is affected under any answer: v1 patches no game code (R2) and reads the game by snapshots and the flag
watcher.

## 4. Two kinds of plugin

The risk of a plugin depends on what is inside it, so this proposal keeps two kinds apart.

| | Content plugin | Code plugin |
|---|---|---|
| Holds | Data only: character profiles, NPC cards, lore entries, layers on shipped profiles | A DLL that calls EldenNet's published interface |
| Checked by | EldenNet, as its own content: schemas, caps, references, lint and spoiler checks | Nobody can check native code: EldenNet checks only what it is given through the interface |
| Can reach | Nothing: it is read, never run | Everything the player's account can, like any native mod |
| Installs from | The Plugins folder or the dashboard (§6) | The Plugins folder only, turned on by the player once (§7.3) |
| When | 1.1, recommended (O66) | After v1.0, designed now (O68) |

## 5. Content plugins (O66)

### 5.1 What one may hold

A content plugin is one more layer of EldenNet's content (ARCH §11.6), with a manifest.

| Content | May add | May replace | Rule |
|---|---|---|---|
| Character profiles (`characters/`) | A profile for a character id in EldenNet's index that has none | A shipped profile, whole | Bindings stay EldenNet's (`identity/characters.toml`); nothing speaks on an unverified binding (DESIGN §7.8) |
| NPC cards (`archetypes/`) | A card for a type EldenNet's `identity/types.toml` defines | A shipped card, whole | The kind rules stay EldenNet's |
| Lore (`knowledge/`) | Entries | A shipped file, whole | `requires` may name only flags in EldenNet's verified catalogue; each entry's spoiler fields are checked (§5.4) |
| Layers (`layers/`) | Fields added to a shipped profile: subjects, manner lines, the instructions a player's own layer may add | — | Below the player's own layers (NPC §4.7) |

**Never in a plugin:** `identity/` (speech, kinds, bindings), `flags/`, `world/` (stages and tracks), `lexicon/` (the
locked terms and tells that guard spoilers), `augur/` (event rules wait for "your own event rules", a later version),
`prompts/` (they carry the spoiler grounding; named extension points may come later), `gal/` (game layouts), and any
file EldenNet's `disable.toml` rules mark as not disableable (ARCH §11.6). Game facts come only from EldenNet's own
verified data (O48), so a plugin can describe a character but never say where the game keeps one. Voices wait for
TTS (v1.5) and follow O62: a plugin may name which of the player's own recordings a voice is made from, never ship
audio.

### 5.2 The manifest

Every plugin carries `plugin.toml` at its top. An invented example:

```toml
id = "example-lore"                # [a-z0-9-]{3,32}, unique among the installed plugins
name = "Example lore pack"
version = "1.2.0"
kind = "content"                   # content | code
eldennet = ">=1.1, <2"             # the EldenNet versions it is made for
author = "An example author"
licence = "CC-BY-4.0"
summary = "Lore for a few invented places, in the author's own words."

[files]                            # every file it holds, with its size and SHA-256
"knowledge/example.toml" = { size = 2048, sha256 = "…" }
```

Only the files the manifest lists are read; anything else in the folder is ignored and named on the page. A file whose
size or hash differs turns the plugin off with the reason, as GraceUI's version file does (GraceUI D52).

### 5.3 Where it sits

The layers, lowest first: EldenNet's shipped content, then the plugins in the player's order, then the player's
`overrides\`, then `disable.toml`, then the player's own profiles (`profiles\layers\` and `profiles\characters\`). So
the player's own files always win, and when two plugins hold the same file, the later one in the order wins and the
page shows the conflict.

### 5.4 The checks

A plugin is checked exactly as EldenNet checks its own layers on a reload (ARCH §11.6): schemas, caps (ARCH §11.6's
per-file caps, and a cap on each plugin's total, EST 4 MB), predicates, aliases, template compilation against the
fixture contexts and the lint. On top of that:

- the manifest itself, the version range and the listed files;
- only §5.1's folders, and only additions or replacements that table allows;
- every character id, type, flag and term it names must exist in EldenNet's own data;
- every public field (what a first meeting shows) must pass the same locked-term check as shipped content, so a
  plugin cannot put a spoiler where the roster or a stranger would see it.

A file that fails is refused with its file and line; the rest of the plugin loads only if nothing it depends on was
refused. Whatever a plugin holds, EldenNet's output checks still run on every line the model writes (DESIGN §10.4).

### 5.5 When it applies

As the reload table says (ARCH §11.6): profiles, cards and lore at the next request, without a restart. A plugin
turned on, off or reordered applies the same way; only a code plugin needs a restart (§7.3).

### 5.6 What the player sees

GraceUI's Content packs page (GraceUI §4.6.9), read-only today (GraceUI D46), becomes the home of plugins: each with its
name, version, author, licence, summary, state (on, off, refused, waiting for a restart), its position in the order, its
conflicts and the results of its checks, in both views. Turning one on or off and reordering work in both views; adding
a file is the browser's alone (§6.2). So D46's "read-only" would be amended, and its "no hub" stands. A bug report names
every plugin by id, version and hash, never by path (PV5).

## 6. Install routes (O67)

Both routes lead into one installer with one set of checks.

### 6.1 The Plugins folder

- `plugins\` sits beside EldenNet's own files, where a mod manager puts what it installs. A plugin is a folder holding
  `plugin.toml`, or a `.zip` holding one such folder.
- **Before the game starts**, EldenNet's launcher looks at `plugins\` (ARCH §4): it checks each new folder or archive
  (§6.3), unpacks an archive into `plugins\<id>\` and removes the archive, and records the plugin. A new content
  plugin is turned on at the end of the order; a new code plugin stays off until the player turns it on (§7.3).
- **To remove** one, delete its folder, or use Remove on the page (§6.2).
- If the folder is not writable, the launcher says so in E06's manner and changes nothing.

### 6.2 The dashboard

- On the Content packs page, in the browser only: a drop zone with an **Add file** button, the box DataPiler's
  update window has (DataPiler DP32).
- The file goes to EldenNet, which keeps it in `plugins\pending\` and checks it on its watcher thread (T10), never on
  the game, render or input threads. The page then shows the result: "Ready" and the plugin turned on (content applies
  at once, §5.5), or "Refused" with the reasons by file and line.
- **Remove** turns a content plugin off at once and deletes its folder at the next start.
- **This PC only.** The command is sensitive, so a paired phone or tablet is refused it (GraceUI D81, G9), and every
  check of GraceUI's server applies (GraceUI §7.2). Files arrive through an upload route GraceUI would add for it,
  since its frames are capped at 16 KiB (GraceUI §6.8): a question for GraceUI once O67 is answered.
- **Never code.** The dashboard refuses a code plugin and says to use the Plugins folder (§7.3): a DLL arriving over
  the dashboard would turn any bug in the server into running code on the player's PC.

### 6.3 Archive rules

- `.zip` only, with stored or deflated entries, at most 32 MiB and 2,000 entries (EST), unpacked no larger than
  128 MiB (EST).
- No entry may name a path outside the plugin's own folder (no `..`, no drive letters, no absolute paths), no links,
  and no names that differ only in case.
- A content plugin may hold only the text files §5.1 allows (`.toml`, `.md`); a code plugin's archive is unpacked by
  the launcher alone.
- Nothing in an archive is ever run while it is checked.

## 7. Code plugins (O68)

### 7.1 The interface

`EldenNet_RequestApi(major)`, with GraceUI's ABI rules (GraceUI D73, D74): plain C, one append-only table per major,
every struct starting with its size, UTF-8 strings with a length, a user pointer on every callback, and `connect`
answering `ok`, `too_old`, `duplicate_id`, `inactive` or `refused`. A plugin pulls its events with `poll` on its own
thread (GraceUI D71's model), so EldenNet never calls a plugin on the game, render or input threads.

What it offers, in the order of SkyrimNet's published functions (WHY§87), adapted to EldenNet and to v1's rule of
dialogue only:

| Group | What a plugin may do |
|---|---|
| Versions | Read EldenNet's version and the interface's; check a capability by name |
| Events | Subscribe to EldenNet's events as projections, the same player-safe forms GraceUI's views receive; report events of its own, which EldenNet records as the plugin's claims, never as the game's facts (claims stay claims, DESIGN K15) |
| Context | Add a short fact about a character or the scene, which EldenNet places in its prompt as a fact, never as an instruction, capped in length and checked by the lint |
| Memories | Read a character's memories as History › Memories shows them; add a memory from an event the plugin reports |
| Lore | Add or remove lore entries at run time, under §5.4's rules |
| Model calls | Send a prompt or a decision to the model on a slot the player assigns, within EldenNet's pacing, its request budget and its gates (no request while dormant, C4); each call shown on AI › Requests with the plugin's name |
| Settings | Describe its settings as EldenNet's own are described, so they appear on GraceUI's pages with one home each |

Actions and gestures stay later-version seams (DESIGN O31 to O42), for plugins as for EldenNet.

### 7.2 What a code plugin never gets from EldenNet

API keys and provider settings; anything that is not a projection (flags, stages, prompts, unshown model text);
game memory (the game-access layer is not offered); a way past the gates, the pacing or the lint; any write to the
game. A plugin is native code, so these limits guard against mistakes, never against a hostile DLL (as GraceUI §7.1
says of its own consumers).

### 7.3 Loading

- **By EldenNet**: enabled code plugins in `plugins\` are loaded by EldenNet itself, by full path with the DLL-folder
  and System32 search flags, once EldenNet has reached Ready after its gate and safety steps. *Since O70 (§13.3):*
  they run outside the game, in the plugin cluster or detached in a host of their own, which EldenNet starts at that
  point; only a plugin that must draw in game or patch the game is loaded inside it. So a plugin inherits
  EldenNet's fail-closed rules: nothing is loaded on an unknown build, beside anti-cheat or at L1, and plugins hear
  "dormant" when multiplayer starts.
- **Turned on once by the player**: the launcher shows a new or changed DLL with its name, version, author and SHA-256
  and asks before it is ever loaded; the answer is kept until the DLL's hash changes.
- **Or by the player's own loader**: a code plugin a loader starts finds EldenNet through the same helper GraceUI's
  consumers use (GraceUI D70) and connects the same way.
- **Faults**: a plugin whose `connect` fails, or which faults at an entry, is turned off for the session and named in
  one notice. The launcher offers **Start without plugins**, for support.
- Pages and panels of its own come through GraceUI, as for any GraceUI consumer, once G18 allows other mods.

### 7.4 Timing

Designed now, as a seam in DESIGN §13; built and opened after v1.0, with a preview first, as GraceUI's ABI is (G18).
Min's engine blueprint is EldenNet's foundation once it arrives (O53, O60), and SkyrimNet already has a plugin
interface, so the interface is re-checked against the blueprint then; what EldenNet adopts is the owner's call.

## 8. Hooks for code plugins (O69)

With §7's interface, most plugins never need to patch anything. For one that does:

- **(a) Not through EldenNet at first** (recommended). A plugin that patches the game does so on its own, and the guide
  tells it never to hook a function GraceUI or EldenNet hooks; EldenNet publishes its list as GraceUI does.
- **(b) EldenNet installs hooks for plugins** with its own library (O65 (a)), so each function keeps one owner, and a
  second request for a hooked function is refused with the owner's name.
- **(c) Never.**

(b) can follow once code plugins have shipped and a plugin needs it.

## 9. Privacy, security and licences

- **Privacy.** A content plugin reaches nothing. A code plugin gets only projections, never keys. No plugin file, id
  or setting reaches a log, report or URL with a path or a name of the player's (PV1 to PV8); installed plugins appear
  in a bug report by id, version and hash.
- **Security.** Code never arrives through the browser (§6.2). Archives are checked before anything is unpacked
  (§6.3). The dashboard route is this PC's alone (GraceUI D81).
- **Licences.** EldenNet never bundles, hosts or lists third-party plugins: there is no plugin store and no download, as
  GraceUI's Content packs page has no hub (GraceUI D2, D46). Each manifest names its licence, which the page shows.
  EldenNet's own example plugin, for tests and the guide, holds invented content in our own words.
- **Game material.** The FromSoftware rule binds our example plugin. A third party's plugin is its author's
  responsibility, and the guide asks authors to write in their own words as EldenNet does.

## 10. Settled here, open to the owner

Each would join DESIGN §18 when the answers are folded in.

| Decision | Choice | Holds under |
|---|---|---|
| Manifest | `plugin.toml`: id, name, version, kind, EldenNet range, author, licence, summary, and every file with its size and SHA-256; only listed files are read | O66 (a) or (b) |
| Layer order | Shipped content, plugins in the player's order, `overrides\`, `disable.toml`, the player's profiles | O66 (a) or (b) |
| Game facts | Only from EldenNet's verified data: plugins never add bindings, flags, stages, layouts or locked terms | — |
| Archive rules | §6.3 | O67 |
| Code through the browser | Never | — |
| Plugin model calls | Within EldenNet's pacing, budget and gates, shown on AI › Requests | O68 (a) |
| Validators | Nothing installs until its validator passes; checked in `plugins\pending\` and moved into place whole; content dry-run against the fixture states; code checked statically, by the library-clash check and by a trial run (§13.1, §13.2) | — |
| Safeguards | A breaker per content plugin; budgets and a watchdog per code plugin; quarantine after two stops (§13.5) | O70 |

## 11. Risks

| Risk | Mitigation |
|---|---|
| A plugin's content spoils the story | §5.4's checks; EldenNet's output checks on every line; one switch to turn the plugin off |
| Two plugins hold the same file | The order decides, and the page names the conflict |
| A crafted archive | §6.3; nothing in it runs while it is checked |
| A code plugin crashes the game or harms the PC | Code plugins run outside the game, in the cluster or detached (§13.3), so a crash stops only plugins and the pop-up lets the player choose (§13.6); code only through the folder, turned on by the player once with its hash shown; validators and a trial run first (§13.1); Start without plugins; documented as native code with full rights |
| Two plugins clash over a library | The library-clash check, against every installed plugin, on or off; Detached as the way out (§13.2) |
| A plugin's model calls cost the player money | The request budget, pacing and gates apply; each call is shown with the plugin's name |
| Support burden of a public interface | A preview first, a stated support window per major (GraceUI D74), versions checked at `connect` |
| Scope against "minimal and stable" | Nothing in v1.0; content plugins first, in 1.1; code plugins later |

## 12. Questions for the owner (O65 onwards, in the order recommended)

### O65: How may EldenNet itself hook?

- **As it would be put.** **(a) Its own hook library, for functions GraceUI does not hook** (recommended): game
  functions, and system functions GraceUI leaves alone, each hook installed once after GraceUI's, chained and never
  removed, each needing its spike and the owner's approval. **(b) Through a hook service in GraceUI's interface.**
  **(c) Decide per feature,** when the first hook is proposed.
- **In plain words.** Should EldenNet be allowed to patch game functions with a tool of its own, as long as it never
  touches the few functions GraceUI uses for the screen and the controls?
- **Recommendation: (a).** It matches what the evidence rules out (two libraries on one function, WHY§86), keeps
  GraceUI read-only towards the game, and keeps EldenNet's features in EldenNet's releases. *What (a) gives up:* a
  second hook library in the process, which is safe only while the two never share a function, so EldenNet's CI
  checks its targets against GraceUI's list.
- **What each answer changes.** (a): GraceUI's invariant 16 becomes "GraceUI's functions have one owner" (no consumer
  hooks a function GraceUI hooks; EldenNet's own hooks on other functions follow its R12); R12 is rewritten to match;
  the hook-based event sources and the save redirect each still need their spike and approval. (b): GraceUI's
  interface gains a hook service and its read-only rule an exception. (c): R12 stays "EldenNet hooks nothing".

### O66: Content plugins?

- **As it would be put.** **(a) Yes, in 1.1** (recommended), as §5 describes. **(b) In v1.0.** **(c) Not planned:**
  the player's own overrides stay the only way to change content.
- **In plain words.** Should people be able to share packs of character profiles, NPC cards and lore that EldenNet
  checks and loads like its own, and when?
- **Recommendation: (a).** Content plugins are mostly the layering EldenNet already has, plus a manifest and an
  installer, and they cannot run code. In 1.1 they do not delay v1.0. *What (a) gives up:* players wait one release.
- **What each answer changes.** (a) and (b): §5 joins the design (ARCH §11.6's layers, DESIGN §11.4, the Content
  packs page, with GraceUI's D46 amended) and §10's rows apply; (b) moves it into v1.0's scope. (c): §5 and §6 fall
  away.

### O67: How are plugins installed?

- **As it would be put.** **(a) Both routes** (recommended): the Plugins folder, checked and installed by the
  launcher before the game starts, and the dashboard's drop zone, this PC only and never for code. **(b) The folder
  only.** **(c) The dashboard only.**
- **In plain words.** Should you install a plugin by dropping it into EldenNet's Plugins folder, by dropping it on
  the dashboard while the game runs, or both?
- **Recommendation: (a).** The folder works with mod managers and without the game running; the dashboard is the
  quick way while playing; one installer and one set of checks serve both. *What (a) gives up:* GraceUI gains an
  upload route, which its server suite must cover.
- **What each answer changes.** (a): §6 as written; GraceUI is asked for the upload route (a G-question there).
  (b): §6.2 falls away, and GraceUI needs nothing. (c): §6.1 falls away, and code plugins need another route, or none.

### O68: Code plugins?

- **As it would be put.** **(a) Designed now, built and opened after v1.0** (recommended): §7's interface, EldenNet
  loading enabled code plugins from its Plugins folder after its gate, each turned on once by the player, and a
  player's own loader also able to start one. **(b) Never:** content plugins only. **(c) With v1.0.**
- **In plain words.** Should other modders' DLLs be able to use EldenNet's characters, memories and model calls, as
  SkyrimNet's plugins do, and when?
- **Recommendation: (a).** It is what makes EldenNet a framework other mods can build on, as SkyrimNet is, while v1.0
  stays minimal. *What (a) gives up:* a public interface to support for years, and the trust that comes with loading
  other people's code.
- **What each answer changes.** (a): DESIGN §13 gains the seam, §7 is designed in ARCH after v1.0, re-checked against
  Min's blueprint. (b): §7 and §8 fall away. (c): §7 moves into v1.0's scope.

### O69: May code plugins patch the game through EldenNet?

- **As it would be put.** **(a) Not at first** (recommended): plugins patch on their own, never a function GraceUI or
  EldenNet hooks. **(b) Yes, through a hook service of EldenNet's** (O65 (a)'s library), one owner per function.
  **(c) Never.**
- **In plain words.** If a plugin needs to patch the game, should EldenNet do the patching for it, so that two mods
  never patch the same function?
- **Recommendation: (a).** The interface covers what plugins need first, and a hook service is better designed once
  real plugins ask for it. *What (a) gives up:* until then, two plugins could patch one function and break each other.
- **What each answer changes.** (a): the guide's rule and EldenNet's published list. (b): EldenNet's interface gains
  hook calls, after code plugins have shipped. (c): the guide forbids patching for EldenNet plugins.

## 13. Validators, safeguards and the plugin cluster (added 2026-11-15)

After O65 to O69 were decided, the owner asked for validators and safeguards by kind, and then designed where code
plugins run, what happens when they stop, and the rows of the Plugins page:

> do you think we could also add safeguards and validators for plugins by their type (content, code, etc.) so the
> plugin would be validated first if it's compatible and the files don't cause bad code (that wouldn't work, prompts
> that would fail, syntax issues, etc.) then if validator passes, then the plugin can install, and then safeguards
> could be monitoring elements that prevent critical failures by catching and stopping them

> would it be possible to design and add another layer for plugins, and run them as one cluster in one process.
> EldenNet would still have link for each plugin and recognise them as separate plugins, and if one acts up or crashes,
> EldenNet would know exactly which one done it. Then in this case the whole cluster would likely crash but Elden Ring
> game and EldenNet would stay alive.

> I would go further and make this a pop up UI, stating plugin layer crashes due to plugin <title>. and whatever else
> needs reporting there, then offer a choice to user to close down the pop up window (an usual X button at the top
> right corner), text buttons: "Restart plugins", "Restart plugins without <crashed plugin title in different text
> color>", "Restart plugins with <crashed plugin title in different text color> detached", "Ignore (no plugins)".
> For "run it's own", we could add setting like that to the plugin UI page in EldenNet, something like a list of
> plugins in rows, each row would have toggle switch: on/off, cluster (part of cluster), detached (its own process).
> "Plugins can clash over shared libraries." this is something we need to add to validator: if a plugin such as that
> is already installed (even if the plugin is currently switched off) the validator would catch incompatibilities
> based on this issue.

### 13.1 Validators, by kind

Nothing installs until its validator passes. A plugin is checked in `plugins\pending\` and moved into place whole, or
not at all, so nothing is ever half-installed.

| Kind | What the validator checks | What it cannot check |
|---|---|---|
| Content | Everything of §5.4, and a dry run: EldenNet builds its whole content with the plugin in a scratch copy and renders, at the fixture states, every prompt the plugin's files feed into, so a file that would break a prompt or push it over its token budget is refused before it installs | Nothing that matters: content is read, never run, and the model's lines are still checked as they come (§13.5) |
| Code | The manifest, its EldenNet range and interface major; that the file is a 64-bit Windows DLL with the entry EldenNet expects, and the exact file its manifest's hash names; what it can reach, read from its imports (the network, files, other processes), shown to the player before it is turned on; its libraries against every other installed code plugin's, on or off (§13.2); then, once the player has turned it on, a trial run in a throwaway plugin host outside the game, with a short scripted session of fixture events: a plugin that crashes, hangs or misuses the interface is refused before the game starts | Whether its code is free of bugs: no tool can prove that, which is why §13.5's safeguards exist |

Plugin authors get the same checks as a test kit (the interface's mock and the trial run as a tool, as GraceUI's
harness and C mock serve its consumers, GraceUI D85), so most problems are caught before a plugin is published.

### 13.2 The library-clash check

When a plugin's library is found by name, Windows reuses a library of that name already loaded in the process,
whichever folder it came from. So two plugins in one cluster that ship different versions of the same library cannot
both have their own: the second gets the first's copy, and may crash.

- The validator reads every code plugin's imports and the libraries it ships in its folder (name, version, hash), and
  compares them with every installed code plugin's, **on or off**, so that turning one on later cannot create a clash
  unseen.
- The same library and the same file: fine. The same name with another version or file: a clash. The validator names
  both plugins and the library, shows the clash on both rows, and offers to install the new plugin **Detached**
  (§13.3), since a detached plugin has a process of its own, or not to install it. A clash between two Cluster rows
  is never started together.
- At run time the cluster also watches every library loaded into it, and a library loaded a second time under the
  same name for another plugin is logged with both plugins' names.
- The guide tells authors to build their libraries into their own DLL, which avoids the problem altogether.

### 13.3 Where code plugins run: the plugin cluster (O70)

- **The cluster.** One small program, the plugin host, started by EldenNet once it has reached Ready (§7.3), runs every
  code plugin whose row says **Cluster**: one extra process outside the game, however many plugins. It has memory of
  its own, so a crash in it never reaches the game or EldenNet.
- **One link per plugin.** EldenNet gives each plugin its own tagged connection, and the host gives each its own
  threads, named after it. So EldenNet knows which plugin made each call and how much processor time each one uses,
  and the host's crash handler sees whose code was running when it failed.
- **Detached.** A plugin whose row says **Detached** runs alone in a host of its own.
- **Every host** talks to EldenNet over a private channel EldenNet hands to it alone, with no network port and nothing
  another program can join; it runs under Windows limits on memory and processor time, never as administrator, and is
  closed with the game. It loads only plugins from EldenNet's Plugins folder that the player turned on, checked by
  their hashes, and takes no file from its command line (§9).
- **Inside the game**, a separate and trusted tier: only a plugin that must draw on the game's screen through GraceUI,
  or patch the game, runs inside it, after a stronger warning; nothing in §13.5 can protect the game from it.
- **How sure the attribution is.** A crash in a plugin's own code, on its own thread, is named for certain. Plugins in
  one cluster share memory, so one plugin can damage something another then trips over; a crash outside the faulting
  plugin's own code is then reported as "most likely", and **Detached** is how the player makes sure.

### 13.4 The Plugins page

Plugins get a page of their own, **Plugins**, at the foot of the menu from 1.1 (GraceUI D104); Content packs keeps
EldenNet's own content and the player's overrides. The page is a list of plugins in rows, as the owner designed it:

- **Each row:** the plugin's name, version and author; its **On/Off** switch; for a code plugin, **Cluster** or
  **Detached**; its state (on, off, refused, stopped, waiting for a restart); a clash or a failed check, in words.
  Opening a row shows §5.6's details and the plugin's reports; Remove is there too.
- Content plugins keep their order, which decides between two plugins holding the same file (§5.3).
- In the browser, the drop zone and **Add file** sit at the head of the list (§6.2).

### 13.5 Safeguards while the game runs

- **Content plugins.** Each part of a prompt carries the layer it came from. If the model's lines keep failing
  EldenNet's checks while one plugin's content was in the prompt (three times in ten minutes, EST), or its content keeps
  pushing prompts over their budget, that plugin is turned off for the session, with a notice naming it; everything
  else carries on.
- **Code plugins.** Every call checked; budgets per plugin (model calls, events, memories, lore); a watchdog per
  plugin, which marks one "not responding" after 10 s without an answer (EST); the cluster's limits as a whole, with
  each plugin's processor time measured. A plugin over its budget or not responding is cut off: EldenNet refuses its
  calls and drops its events. A host that stops, by a crash or a limit, brings up §13.6's pop-up; the game and EldenNet
  carry on.
- **Quarantine.** A plugin that stopped its host twice is off from the next start until the player turns it back on.

### 13.6 When plugins stop: the pop-up (O70, the owner's design)

- **What it says.** "Plugins stopped: <plugin> crashed" (or "stopped responding", "used too much memory"), with the
  plugin's title in the danger colour and in bold, since colour is never the only signal (GraceUI §3.12); how sure
  EldenNet is ("in its own code", or "most likely", §13.3); the plugin's version; which plugins were in the cluster
  and have stopped with it, and which detached plugins are still running; **Details**, which opens the report on the
  Plugins page, and **Copy report** (scrubbed, PV1 to PV8).
- **Its choices.**
  - The **X** at the top right closes it. Code plugins stay off, and the same choices stay on the Plugins page.
  - **Restart plugins**: the same plugins again. After a second stop in the session, it asks once more first.
  - **Restart plugins without <plugin>**: that plugin's row is switched off.
  - **Restart plugins with <plugin> detached**: that plugin's row is set to Detached, and it restarts in a host of its
    own.
  - **Ignore (no plugins)**: code plugins stay off for the rest of the session, and the pop-up does not come back for
    it.
- **Where it shows.** In the browser at once. In game, first as a notice ("Plugins stopped: open Plugins to choose"),
  and as the pop-up only at a safe moment, never in danger, as every view (GraceUI invariant 9). If the player never
  chose, it shows again on the launcher's first screen at the next start.
- **A detached plugin** that stops brings up the same pop-up about itself alone; the cluster keeps running.

### 13.7 The crash watcher (O71)

- **When the game itself crashes** (in EldenNet, GraceUI, the game, another mod or an in-game plugin), something outside
  the game must record it, because everything inside the game, the dashboard's server included, stops with it.
- **The watcher is the launcher**, staying in the background while the game runs instead of exiting after it has
  started it. It started the game, so it holds a handle to it and sees the moment it ends and how. For the moment of a
  crash, EldenNet's crash filter in the game signals the watcher and waits, and the watcher records from outside:
  where the crash happened (the module), the kind of error and the thread, the last lines of EldenNet's log and the
  last events, and which plugins and mods were loaded, with their versions. This is how crash reporters work; whether
  it sits well beside the game's own handler and other mods' is for S0-D's checks (LIKELY).
- **At the next start**, the launcher and the dashboard say what happened in plain words, and a plugin found at fault is
  off until the player turns it back on. When the fault is in the game's own code, the report says "most likely".
- **A full crash dump**, if one is written, stays on the PC: it can hold whatever was in the game's memory, the player's
  keys and chats included. The report a player shares is the scrubbed summary.
- **Players who start the game through their own loader** get the same watcher, started by EldenNet itself.
- **It changes** DESIGN §18's process model ("one DLL plus a short-lived launcher") and ARCH §1.1, hence O71.

### 13.8 The sandbox (O72)

Each plugin host could run with reduced rights, the kind of sandbox web browsers use, so that even a hostile plugin
could not read the player's files, reach the network or touch the game's memory unless its manifest asks and the player
allows it. It is the strongest protection, but it limits what honest plugins can do, and its support under Proton is
unknown, so it would come later, as an option, after a spike.

### 13.9 Questions for the owner (O70 onwards)

#### O70: Where do code plugins run, and what happens when they stop?

- **Decided by the owner on 2026-11-15**, as the owner designed it: one plugin cluster outside the game by default,
  each plugin still linked and recognised on its own; a row per plugin with **On/Off** and **Cluster** or **Detached**;
  the validator's library-clash check against every installed plugin, on or off; and the pop-up of §13.6, with its X
  and its four choices. Plugins that must draw in game or patch the game stay a separate, trusted tier inside it.
  This amends O68's loading (§7.3).

#### O71: Should the launcher stay running as the crash watcher?

- **As it would be put.** **(a) Yes** (recommended): the launcher stays in the background while the game runs and
  records any crash of the game itself, as §13.7 describes; players who use their own loader get the watcher started
  by EldenNet. **(b) No:** the launcher stays short-lived, and fault finding after a crash has only EldenNet's own log.
  **(c) A watcher program of its own**, separate from the launcher.
- **In plain words.** When the whole game crashes, should EldenNet's launcher be waiting in the background to record
  what happened, so the next start can tell you why?
- **Recommendation: (a).** Nothing inside the game survives its crash, and the launcher already holds the game's
  handle. *What (a) gives up:* one small process for the length of a session, and the design's "short-lived" launcher.
- **What each answer changes.** (a): DESIGN §18's process model and ARCH §1.1 and §4; the watcher's checks join S0-D.
  (b): §13.7 falls away, and the plugins-stopped pop-up covers only the cluster. (c): as (a), with one more program
  to ship and sign.

#### O72: A sandbox for plugin hosts?

- **As it would be put.** **(a) Later, as an option** (recommended): after code plugins ship, a spike checks Windows'
  reduced-rights sandbox on Windows and under Proton, and a plugin's manifest names what it needs for the player to
  allow. **(b) Never.** **(c) From the start of code plugins.**
- **In plain words.** Should plugins one day be able to run boxed in, so that even a malicious one could not read your
  files or reach the internet without your say?
- **Recommendation: (a).** It is the strongest protection, but it limits what honest plugins can do and may not work on
  the Steam Deck, so it is better added once real plugins exist. *What (a) gives up:* until then, a hostile plugin can
  do what any program the player runs can.
- **What each answer changes.** (a): a spike after code plugins ship, and a field in the manifest. (b): nothing. (c):
  the sandbox becomes part of code plugins' first release, and the spike comes first.
