# Proposal 0001: Plugins for EldenNet

**Status: proposal, 2026-11-15 (roadmap S7.10.a).** It changes nothing by itself: the decided design stands until the
owner answers O65 to O69 (§12), and each answer's effect is stated with its question. What it settles itself is listed
in §10, each row open to the owner. Evidence: SkyrimNet's published plugin headers (MIT; WHY§87), ER02 §7.1 on hook
libraries (WHY§86), ARCH §11.6 on content layers, and GraceUI's DESIGN §2.9 and §7. A bare §n is a section of this
proposal; DESIGN, ARCH and NPC are EldenNet's design documents, and "GraceUI §n" and G- and D-numbers are GraceUI's.

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
  and System32 search flags, once EldenNet has reached Ready after its gate and safety steps. So a plugin inherits
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

## 11. Risks

| Risk | Mitigation |
|---|---|
| A plugin's content spoils the story | §5.4's checks; EldenNet's output checks on every line; one switch to turn the plugin off |
| Two plugins hold the same file | The order decides, and the page names the conflict |
| A crafted archive | §6.3; nothing in it runs while it is checked |
| A code plugin crashes the game or harms the PC | Code only through the folder, turned on by the player once with its hash shown; Start without plugins; documented as native code with full rights |
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
