# EldenNet's pages

**Status: design, moved here from GraceUI's design on 2026-11-16, by GraceUI's G20 (a), which the owner decided that
day, as recommended (GraceUI D82; roadmap S7.8.d).** Nothing is built yet. EldenNet's menu and dashboard are content
EldenNet ships: it registers them with GraceUI at start as its space, and GraceUI draws them in both views, in game on
F5 and in the player's browser. This document holds that content: the information architecture of EldenNet's space, its
categories and pages with their ids, the home of every setting, each page with its help text, and Home's cards (§4); the
spoiler rules SP1 to SP9 (§4.8); EldenNet's privacy rules (§4.9); its fixtures and goldens (§5); and D61 (§6). DESIGN
§12 points here.

**How to read the references.** §4 keeps GraceUI's numbers: §4.n here is what GraceUI's §4.n was until 2026-11-16, and
the parts GraceUI kept stand here as one-line pointers under the same numbers, so a reference by number resolves in
either document. §1 to §3, §5 and §6 are this document's own. "GraceUI §n" is a section of GraceUI's design; G1 to G30
are GraceUI's owner decisions and D1 to D114 its design log, cited bare as its design cites them (D61 alone moved here);
invariants are GraceUI's, numbered as in its CLAUDE.md, 1 and 2 with their EldenNet form in §3. SND (SkyrimNet's
dashboard) and IA (information architecture) are GraceUI's research notes, and "the first proposal" is GraceUI's first
proposal for the menu, which IA studies. EldenNet's own codes are as everywhere (DESIGN, ARCH, NPC, MEM, the research
notes, O1 onwards); the moved text still says "EldenNet DESIGN §n" where GraceUI's design did. Its wording is GraceUI's,
changed only where it named its old place.

---

## 1. What this holds, and what stays in GraceUI

- **Here, as EldenNet's** (G20 (a)): its rail of categories and pages with their ids (§4.2.1); the states of the menu
  as EldenNet's levels set them (§4.4); every setting's home (§4.5); each page, with its help text, and Home's cards
  (§4.6); the spoiler rules (§4.8); its privacy rules PV1, PV2, PV4, PV5 and PV8 (§4.9); every EldenNet feature and its
  home (§4.10); SkyrimNet's pages with no EldenNet counterpart (§4.11); the first proposal's pages (§4.12); the
  fixture states and the spoiler, roster and Playthrough goldens (§5); and D61, keys bound to their address (§6).
- **In GraceUI, as the framework's** (D82): the frame and its page parts (GraceUI §4.2, but for §4.2.1), global search
  (GraceUI §4.3), the settings row (GraceUI §4.7), the components (GraceUI §2.5, §3.8), the Grace theme (GraceUI §3),
  the privacy rules PV3, PV6 and PV7, which bind every space's pages (GraceUI §4.9), and a demo space with invented
  content for GraceUI's own tests and harness. Invariants 1 and 2 keep their framework form there (§3).
- **The map** stays in GraceUI §4.13: its design under GraceUI's G21 to G27, with D89 to D98, whose centre is the
  framework's map panel kind (D91). Its entry in this rail is §4.2.1's, and Settings › Map's rows are §4.5's (D92, D95).

## 2. EldenNet's space

- **Shipped and registered** (D82; GraceUI's proposal 0001 §4.10). These pages are EldenNet's page data, kept in its
  repository, compiled into `eldennet.dll` and registered with GraceUI at its start as its space (D75), with EldenNet's
  settings description (GraceUI §6.9); GraceUI draws them with its frame, components and theme in both views.
- **One command path** (invariant 3). Every change from either view reaches EldenNet's one validating entry point;
  GraceUI routes it there and decides nothing.
- **Checked in EldenNet's CI.** GraceUI's page lint (invariant 12, GraceUI §9.1) and its parity tests (GraceUI §9.3)
  run on these pages in EldenNet's CI against a pinned GraceUI, beside the goldens of §5; the lint ships as a GraceUI
  tool. A change to a component and to these pages can land in two repositories.
- **Topics and commands.** The pages read EldenNet's topics and send its commands as GraceUI §6.5 and §6.6 list them,
  in GraceUI's protocol schema: G20 (a) moved the pages, not that schema (WHY§107).

## 3. Invariants 1 and 2, in EldenNet's form

GraceUI's CLAUDE.md keeps their framework form: a view shows only what its space published, and absence is silent. For
EldenNet's space they read:

1. **Projections only.** No view of these pages ever receives raw content, flags, stages, prompts or unshown model text
   (spoilers, SP1; the spoiler goldens, §5).
2. **Absence is silent.** Nothing not reached is shown, counted, greyed or hinted at; the veiled look is only for a
   perceived character whose name the game has not shown (SP5; the goldens, §5). The one exception is the roster, by
   the owner's decision (G5, G8): every NPC type, and every named NPC's and named boss's general profile, are listed
   from the start (D67), holding only what a first meeting shows and reviewed for spoilers like shipped content;
   everything about this playthrough follows the rule in full (the roster and Playthrough goldens, §5).

---

## 4. Information architecture

*Moved from GraceUI §4 on 2026-11-16 (G20 (a)), under its numbers; §4.2.2 to §4.2.5, §4.3, §4.7, PV3, PV6, PV7 and
§4.13 stayed in GraceUI.*

EldenNet's pages are the dashboard of an AI framework, so their structure follows the way SkyrimNet groups a
framework's elements (SND): a collapsible sidebar of categories with pages, a slim top bar, a Home grid of cards, and
pages built from a few regular parts (D38). Names are plain framework words; Elden Ring's features and settings live
inside the pages, in our own words, and game names appear only at run time (D39). The Grace look (GraceUI §3) stays,
with gold now also naming a page's sections (D48). This section is binding. It replaces the first proposal's
flavour-named sections (IA §3 and §5's page set; §4.12 maps old pages to new); IA's mapping evidence and its spoiler
and privacy studies still stand.

### 4.1 Principles

1. **One home per setting.** Each setting has exactly one home page, where search leads. Other pages may show it as a
   shortcut row: the same control, marked "lives in Behaviour › Pacing". Settings holds only the settings no category
   page can hold, and links out to the rest (D40).
2. **Changes apply as they are made** (D14). No Save button; EldenNet writes `settings.toml` after 300 ms of quiet and
   hot-reloads it. Danger in game closes the menu and keeps every change; a confirmation still open is cancelled.
3. **Each row says whether it works now**: live, waits on a game check on this build, takes effect in v1.5, later
   version, or no lines in this version (GraceUI §4.7). A setting that does nothing yet never looks as if it works.
4. **Player-safe projections only** (SP1). Live views show only what the Tarnished perceived (SP7).
5. **Same pages in both views** (D5). The browser adds only Workshop, behind `debug.panel` (D10).
6. **Framework categories, plain names** (D38, D39). The rail and page titles use plain words in British spelling (AI,
   Voice, Characters, History, Behaviour, System, Tools). Page text speaks to the player as "you", in our own words.
   Names of places, regions and characters appear only at run time, from EldenNet's projections (SP3); EldenNet's page
   descriptions hard-code none. Where a Purpose line below says "the Tarnished", the page's description reads "you".
7. **A feature's switch lives on its page** (D45). The top bar's quick switches are shortcuts to those homes.
8. **Later versions are stated, never faked** (G7). A later-version entry carries a "WIP" tag and opens a statement
   page with no controls; a feature that is not planned never appears (§4.11).
9. **Nothing personal leaves a page** (PV1–PV8).

### 4.2 The frame

GraceUI's (D79, D82): GraceUI §4.2.2 to §4.2.5 hold the rail's behaviour, the top bar, the page parts and the sizes,
as EldenNet's space fills them. EldenNet's own part of the frame is its rail, §4.2.1.

#### 4.2.1 The sidebar

```
EldenNet                                   [collapse]
Home
AI                 ▸ Providers · Models · Self-test · Requests · Director (WIP)
Voice              ▸ Text and subtitles · Text-to-speech (WIP) · Speech-to-text (WIP)
Characters         ▸ Playthrough · Named NPCs · NPC types
Map (WIP)
History            ▸ Events · Conversations · Memories · World
Behaviour          ▸ Conversation · Speaking up · Combat · Awareness · Reactions · Pacing · Acts (WIP)
System             ▸ Status · Performance · Traces · Logs
Tools              ▸ Event monitor · Bug report · Workshop (browser, developer)
────────────────
Settings
Docs
Content packs
Plugins (1.1)
```

| Category | Pages, with their ids | First shown |
|---|---|---|
| Home | Home `home` | 0.2 |
| AI | Providers `ai.providers` · Models `ai.models` · Self-test `ai.selftest` · Requests `ai.requests` · Director `ai.director` (WIP: later) | 0.2; Director a statement from 0.6 |
| Voice | Text and subtitles `voice.text` · Text-to-speech `voice.tts` (WIP: v1.5) · Speech-to-text `voice.stt` (WIP: v1.5) | 0.4 (browser), 0.6; the two v1.5 statements from 0.6 |
| Characters | Playthrough `characters.playthrough` · Named NPCs `characters.named` · NPC types `characters.types` | 0.3; 0.3; 0.4 |
| Map | Map `map` (WIP: v1.1; GraceUI §4.13) | A statement from 0.6; the page in 1.1 |
| History | Events `history.events` · Conversations `history.conversations` · Memories `history.memories` · World `history.world` | 0.4; 0.3; 0.3; 0.5 |
| Behaviour | Conversation `behaviour.conversation` · Speaking up `behaviour.speaking` · Combat `behaviour.combat` · Awareness `behaviour.awareness` · Reactions `behaviour.reactions` · Pacing `behaviour.pacing` · Acts `behaviour.acts` (WIP: later) | 0.2 (file) to 0.6; Acts a statement from 0.6 |
| System | Status `system.status` · Performance `system.performance` · Traces `system.traces` · Logs `system.logs` | 0.2 |
| Tools | Event monitor `tools.monitor` · Bug report `tools.report` · Workshop `tools.workshop` (browser, `debug.panel`) | 0.4; 0.2; 0.2 |
| Foot | Settings `settings` (groups `settings.general`, `settings.hotkeys`, `settings.menu`, `settings.browser`, `settings.developer`, and `settings.map` from 1.1; tab `settings.all`) · Docs `docs` · Content packs `content` · Plugins `plugins` (from 1.1, D104) | 0.2 (browser, partly read-only), 0.6 |

Home and 30 pages in eight categories: Home and 24 pages work in v1, one arrives in 1.1 (Map, GraceUI §4.13), two in
v1.5, two are later-version statements and Workshop is for developers; then three foot pages, and a fourth, Plugins,
from 1.1 (D104). Outside the rail: First light (the setup flow), search and confirmation modals (§4.6.10).

### 4.3 Global search

GraceUI's (D82): GraceUI §4.3, which also lists what EldenNet's space indexes and what it never indexes (D21, SP9).

### 4.4 States of the whole menu

| State | Usable | The rest says |
|---|---|---|
| Title screen (no playthrough loaded) | Home (Setup, Status, Problems), all of AI, Voice › Text and subtitles, System, Tools › Bug report, Settings, Docs, Content packs, every settings row of Behaviour, Characters › Named NPCs (the roster: Profile, Allowances, Reactions) and NPC types (every type, G5), First light | Playthrough, the History pages and the Event monitor: "Load a character to see this" |
| L1 banner (EldenNet DESIGN §4.4) | System, Tools › Bug report, Settings, Docs, Voice › Text and subtitles; in the browser only after a later L1 trip that is not anti-cheat, since EldenNet's space is never registered for the browser when the gate or the safety step fails, and the server stops when anti-cheat is found later (GraceUI §7.2 item 1, D81) | Greyed: "EldenNet is inactive: reason" |
| L2 observing (no healthy model), or AI paused | All | Home and AI lead with the cause |
| Dormant (multiplayer, loading) | All; live views paused | "Paused (multiplayer)"; settings stay editable |
| Lost contact (browser) | The last snapshot, read-only | The stale state (GraceUI §3.9) |

*2026-11-21, as built (S11.4.a's third slice; O95 (4)):* at L1, Home and the AI pages are greyed through GraceUI's
`space.state` (D188) with en-core's banner words themselves ("EldenNet inactive: …"), one form on every page: the AI
pages' Now card and their refusals at L1 carry the same words. System and Settings stay usable. L2 greys nothing.

### 4.5 Every setting and its home

Keys are EldenNet's `settings.toml` (ARCH §11.5, schema 2) plus the keys GraceUI asks for (marked *new*; see GraceUI's
`docs/eldennet-changes.md`). Since D84, the menu's and the server's own keys (`hotkeys.menu`, `ui.scale`, `ui.menu.*`
and `dashboard.*`, marked *GraceUI's* below) are GraceUI's settings, kept in `Game\GraceUI\user\` and migrated out of
EldenNet's `settings.toml`; the bindings of every registered hotkey, EldenNet's included, are kept by GraceUI too (D77).
Since GraceUI's D115 (2026-11-16, roadmap S7.8.e) their homes are the pages of GraceUI's own space at the rail's foot
(D79: Menu, Hotkeys, Browser view), whose commands go through GraceUI's own space; EldenNet's pages carry only links to
them, under "Shortcuts on". The two settings that arrange EldenNet's own pages are EldenNet's: `ui.home_cards` and
`ui.show_world`. Effect timing is ARCH §11.6's: Now, Next moment, Next request, Next launch.

| Key | Player label | Home | Shortcuts on | Effect |
|---|---|---|---|---|
| `enabled` | EldenNet on | Settings › General | Quick switches | Pause now, off from the next launch |
| `features.chat` | Conversation with named NPCs | Behaviour › Conversation | Quick switches | Next moment |
| `features.callout` | Call out to other NPCs | Behaviour › Conversation | Quick switches | Next moment |
| `features.speak_up` | Named NPCs speak unasked | Behaviour › Speaking up | Quick switches | Next moment |
| `features.barks` | Other NPCs speak unasked | Behaviour › Speaking up | Combat, Quick switches | Next moment |
| `features.npc_to_npc` | NPCs greet and answer each other | Behaviour › Speaking up | Quick switches | Next moment |
| `features.self_awareness` | NPCs know what they said and how they are | Behaviour › Awareness | Quick switches | Next moment |
| `features.status_awareness` | NPCs notice statuses | Behaviour › Awareness | Quick switches | Next moment |
| `features.memory` | Named NPCs remember | History › Memories | Quick switches | Next moment |
| `features.labels` | Floating labels (else the feed) | Voice › Text and subtitles | Combat, Quick switches | Now |
| `autonomy.enabled`, `autonomy.period_s` | NPCs speak up on their own; how often | Behaviour › Speaking up | Quick switches (enabled) | Next moment |
| `range.talk_m` | Speaking-to-you range | Behaviour › Pacing | Conversation | Next moment |
| `range.hear_m` | Overheard-lines range | Behaviour › Pacing | Speaking up | Next moment |
| `range.fight_m` | Combat-lines range | Behaviour › Pacing | Combat | Next moment |
| `pacing.gap_s`, `pacing.window_s` | Time between lines; the window | Behaviour › Pacing | — | Next moment |
| `pacing.group_gap_s`, `pacing.fight_max` | Between lines of one fighting group; lines per window in a fight | Behaviour › Pacing | Combat | Next moment |
| `pacing.idle_max`, `pacing.talk_max` | Idle remarks per window; talk per window | Behaviour › Pacing | Speaking up | Next moment |
| `reactions.<id>.on`, `.cooldown_s` (20 ids, NPC §9.1) | Per reaction | Behaviour › Reactions | In battle on Combat; Afflictions on Awareness; To you, Between NPCs and Calm remarks on Speaking up | Next moment |
| `allow."<kind\|type\|slot>".<key>`, `.reactions.<id>` (EldenNet DESIGN §7.6) | Per allowance | Characters › NPC types | Named NPCs (the slot's row), Combat (`combat`, `boss_fight`), Awareness (`status_reactions`, awareness); Voice › Text-to-speech (a read-only `tts` summary) | Next moment |
| `ui.scale` *GraceUI's* | Text and menu size | GraceUI › Menu | Voice › Text and subtitles (a link) | Now; in game the atlas rebuilds when the menu closes (D29) |
| `ui.hdr`, `ui.paper_white_nits` | HDR; paper white | Voice › Text and subtitles | — | Now |
| `ui.follow_game_subtitles` | Follow the game's subtitle setting | Voice › Text and subtitles | Conversation | Now |
| `ui.latency_toasts` | Latency toasts | Settings › Developer | Traces | Now |
| `pools.max_jobs_per_session` | Prepared-line batches per session | AI › Models (Prepared lines) | Combat | Next request |
| `[slots.<slot>]` `provider`, `model`, `fallback`, `fallback_provider` (`providers.toml`, ARCH §11.1; O88) | Each job's provider, model and fallback | AI › Models (one row per slot) | — | Next request |
| `safety.level` | Content safety: faithful or gentle (O17) | Behaviour › Conversation | — | Next request |
| `hotkeys.talk`, `hotkeys.skip_page` | Talk; skip page (*since G37, 2026-11-22: defaults EldenNet registers, rebound on GraceUI › Hotkeys; WHY§362*) | Settings › Hotkeys | Conversation | Now |
| `hotkeys.menu` *new*, *GraceUI's*, replacing `hotkeys.settings` | Open the menu (F5) | GraceUI › Hotkeys | Settings › Hotkeys (a link) | Now |
| `hotkeys.debug` | Debug view (F11) (*since G37, 2026-11-22: a default EldenNet registers, rebound on GraceUI › Hotkeys; WHY§362*) | Settings › Hotkeys | Settings › Developer | Now |
| `ui.menu.theme` *new*, *GraceUI's* | Grace or Grace Clear | GraceUI › Menu | Top bar | Now |
| `ui.menu.plain_text`, `.reduce_motion`, `.key_hints`, `.pad_glyphs`, `.backdrop`, `.compact_rows` *new*, *GraceUI's* | The menu's own options (D19) | GraceUI › Menu | — | Now |
| `ui.show_world` *new*, EldenNet's (GraceUI's D115; was `ui.menu.show_world`) | Show the World page | Settings › General | — | Now |
| `ui.home_cards` *new*, EldenNet's (GraceUI's D115; was `ui.menu.home_cards`) | Home's cards, their order and Setup's place | Home › Customise | — | Now |
| `dashboard.enabled` (on), `dashboard.port` (8642) *new*, *GraceUI's* (G4) | Serve the browser dashboard; its port | GraceUI › Browser view | Settings (a link) | Now (the server starts, stops or moves) |
| `dashboard.lan` (off) *new*, *GraceUI's* (G9) | Other devices on your network | GraceUI › Browser view | Settings (a link) | Now |
| `map.base`, `map.key_opens` (the game's map), `map.glow`, `map.flare_new`, `map.layers`, `map.lore_by_ai` (on) *new* (1.1, D95) | The map's settings: base map, the map key, glow, brighten new things once, the layers shown, Lore notes by AI | Settings › Map | Map | Now |
| `augur.allow_custom_rules` | Your own event rules | Behaviour › Reactions (a locked "later version" row) | — | — |
| `debug.panel`, `.record_sessions`, `.record_redact_text`, `.log_level` | Developer options | Settings › Developer | Logs (log level), Workshop (panel) | Now |

The first proposal's `ui.menu.names` is dropped: names are plain only (D39).

**A row per slot** (O88, whose "Settings › AI" is the AI page's Models). AI › Models shows one row for each job slot,
with its provider, model and fallback: Conversation (`dialogue`), Quick lines (`bark_live`), Prepared lines
(`bark_pool`), Memory (`memory`), Profile drafts (`draft`), Decisions (`decisions`) and Self-test (`selftest`). A slot
with no model of its own, from the preset or the player, takes Conversation's provider, model and fallback, and its row
says so, "Same as Conversation", until it is changed; Decisions follows Quick lines in the same way (ARCH §11.4).
Without a preset, every row but Conversation starts so. A change writes the slot's `[slots.<slot>]` in
`providers.toml`, its provider named, and Reset to preset clears it. The reserved slots, `actions` and `vision`, get
their rows only once their features exist.
*2026-11-21, as built (S11.4.a's third slice; GraceUI D193):* each shown job's model is a settings row,
`slots.<slot>.model` (Conversation's, Quick lines', Prepared lines', Memory's and Self-test's model; home AI › Models;
next request), a text setting whose choices come from a state topic of the space's own, `ai.choices.<slot>`: what the
job's provider listed at the last model list or self-test, an id that looks like a path left out (PV5). EldenNet checks
at its entry point that a value is one of them. The row's value is the job's model now, the preset's or the one chosen,
and stays shown when the list lacks it; a choice writes the slot's `[slots.<slot>]` `provider` (the job's provider now)
and `model`, any fallback there kept; the row's reset removes `[slots.<slot>]`, so the preset's models hold again. The
provider and fallback pickers, Decisions and Profile drafts come later.
*2026-11-21, as built (S11.4.a's fourth and fifth slices; O99):* new rows `slots.<slot>.provider` ("Conversation's
provider" … "Self-test's provider"), `slots.<slot>.fallback_provider` and `slots.<slot>.fallback` ("Conversation's
fallback provider", "Conversation's fallback" …), all homed on the job's card on AI › Models, next request. A
picker's Default is its row's reset. A reset of the job's provider or model removes its whole `[slots.<slot>]`, a
reset of either fallback row the fallback alone. A job's rows count as changed while its table gives a model, its
fallback rows while it gives a fallback. On a paired device, `settings.set` and `settings.reset` of these rows join
the commands to refuse.

**Other sources.** From 1.1, `config\plugins.toml` (each plugin's switch, its Cluster or Detached mode and the order;
changed only through EldenNet's plugin commands) on Plugins. `providers.toml` (read and write) on AI › Providers (its
entries) and AI › Models (each slot's provider, model and fallback, and the preset); `secrets.toml` **write-only** on
AI › Providers (the UI sees set, missing or rejected, never a value); `models.toml` (read) on AI › Models;
`identity/types.toml` (read) on NPC types; persona and bio display fields only (`display_name_fallback`,
`public.summary`, `public.role`, `subjects`, the news temperament) on Named NPCs; each persona stage's public line, for
the stage seen only, on Playthrough; a world stage's `line` only for stages that hold, on History › World; `overrides/`
and `disable.toml` (read, as relative paths, ids and counts) on Content packs.
View state, never stored: live updates, the journey in view, the rail collapsed, sections open.

**Actions** (each a command, GraceUI §6.6): Pause AI for this session (Home, D15); run the self-test (AI › Self-test,
Home); find local servers (AI › Providers); prepare lines now (AI › Models; at rest or at the title only); Forget this
NPC and Forget this journey (History › Memories; confirmation, a backup, no undo but the backup); reset a row, a page or
everything; show a sample line (Voice › Text and subtitles); capture a key (Settings › Hotkeys); build, preview, copy or
save a bug report (Tools › Bug report, after EldenNet's deterministic scan); disconnect every browser (Settings ›
Browser view); pair a device while `dashboard.lan` is on (in game only), and forget one at any time (G9).

**The page lint** (invariant 12, GraceUI §9.1) checks every key above against EldenNet's settings description: exactly
one `home`, and the home is one of the page ids `home`, `ai.models`, `voice.text`, `characters.types`,
`history.memories`, `behaviour.conversation`, `behaviour.speaking`, `behaviour.awareness`, `behaviour.reactions`,
`behaviour.pacing`, `settings.general`, `settings.hotkeys`, `settings.menu`, `settings.browser` or `settings.developer`,
and from 1.1 `settings.map`. AI › Providers holds `providers.toml` entries and keys, which are not settings rows.
*2026-11-21, as built:* so it is since GraceUI 0.2.8: the second slice's key rows (`keys.<provider>`, secret settings
homed on AI › Providers) are gone, and each key is its provider's row secret (GraceUI D194).

### 4.6 Pages

Each page below gives its purpose, contents and controls, the topics it reads (GraceUI §6.5) and the commands it sends
(GraceUI §6.6), the increment it ships in, and its spoiler and privacy rules. Every page also has the header, help
dialog and Related card of GraceUI §4.2.4.

#### 4.6.1 Home

- **Purpose.** Is EldenNet working, who is near, what was just said and what was asked of the model.
- **Header actions.** Pause AI (a chip that toggles; session only, D15) · Run self-test (`selftest.run`; the result
  in a toast and on AI › Self-test) · Customise.
- *Since 2026-11-24, as built (engine 2f27925; GraceUI D252):* Pause AI is GraceUI's quick switch ("Pause AI for this
  session", `ai.pause`, its state `status.ai_paused`), with its line on the Status card; the header chip waits for a
  header action that is a switch. Home is live, and its Model calls card shows the last 20.
- **Cards** (a grid of mixed widths, each with an uppercase gold title; the title, or Enter on it, opens its page):

| Card | Width (3 / 5 columns) | Contents | Reads | From |
|---|---|---|---|---|
| **Setup** (collapsible) | Full / full | Three cards side by side. *Model*: the provider and key status in words, the preset (a shortcut; home AI › Models), Test with the self-test's verdict word. *Voice*: "Text first; spoken voices arrive in v1.5" and a link. *Microphone*: "Talking aloud arrives in v1.5" and a link. "Run setup again" opens First light. Open until the self-test has passed once on this install, then collapsed | `providers`, `selftest`, `health` | 0.2 |
| **Status** | 1 / 1 | The state word and its reason; in game "You can talk now" or why not; one dot per model slot with its word; EldenNet's version, the game build (supported or not), session time; whether AI is paused; the dashboard's address, or "off" (G4) | `status`, `health` | 0.2 |
| **Nearby** | 1 / 2 | Perceived characters (SP7): on-screen name or type label, a state word, wounds in words for combatants, statuses after S8, "speaking", a met NPC's disposition word; at most twice a second; no HP numbers, no distances to hostiles; its title opens Characters › Playthrough | `nearby` | 0.3 |
| **Happening now** | 1 / 1 | Short-lived events the Tarnished perceives (Augur's TTL events, EldenNet DESIGN §6.4), each with our label and a time-left bar redrawn at the live rate; "Nothing right now" | `shortlived` | 0.4 |
| **Problems** | 1 / 1 | Recent failures in words (EldenNet's failure contract): the model unreachable, a key refused, a read group off; each with its fix and a link; "No problems" in `moss` | `errors` | 0.2 |
| **Lines** | 2 / 3 | Displayed lines (the speaker as shown, the line), conversations opened and closed, notices, reloads | `activity` | 0.2 |
| **Recent events** | Full / 2 | The last ten perceived events in our labels | `monitor.events` (its tail) | 0.4 |
| **Model calls** | Full / full | The last 20 calls: time, slot, purpose, speaker (as shown, SP3), model, first token, total, outcome (the words of AI › Requests); the displayed text only if it was shown; this hour's own-initiative calls against the bound | `calls`, `usage` | 0.2 |

- **Grid.** Three columns under 1600 px, five at 1600 px and wider (CSS px at the effective scale, GraceUI §4.2.5). The
  default order is the table's. Cards are placed in that order with no packing (no `dense` flow), so the visual order
  and the focus order agree at every width; at three columns Recent events takes the full width, so no cell is left
  empty. A custom order may leave a gap; nothing reorders cards to fill it.
- **Customise** (a drawer): show or hide each card, Setup at the top or the bottom, card order (drag in the browser;
  "Move up" and "Move down" on the focused card in game). Stored as `ui.home_cards` (EldenNet's, GraceUI's D115), whose
  home is this drawer. A hidden card subscribes to nothing.
- **Rules.** SP7 on Nearby, Happening now and Recent events; Lines shows only what was displayed. Chat and model text
  only on Lines and Model calls (PV4).
- **Keys and pad.** Cards are focus targets in reading order; ↑↓ inside a card's rows; A on a title opens its page.
- *2026-11-21, as built (S11.4.a):* Status and Problems only. Problems reads "No problems." when there are none
  (GraceUI D186's empty words; the moss tone is GraceUI's to give), and holds a refused providers file with why in words
  and its line, beside a refused settings file. At L1 the page is greyed with the banner's words (§4.4).

#### 4.6.2 AI

**Providers** (`ai.providers`; 0.2 read-only, 0.6 writable in both views).

- **Purpose.** Where EldenNet's model calls go: a cloud service with the player's own key, or a local server.
- **Header actions.** Find local servers (`servers.find`; only while EldenNet's gate allows network, C4) · Add
  provider.
- **Tabs.** *Providers*: an **Advice** banner from `models.toml` (which preset suits most players and why, with "Use
  this", which opens Models with the preset chosen); one card per entry of `providers.toml`: id, cloud or local
  (`kind`), flavour (OpenAI-compatible, Ollama, llama.cpp, LM Studio or KoboldCpp; ARCH §11.1), base URL (shown on this
  page only), key status (Set, Missing, Rejected) with a masked, write-only "Set key" and "Clear key", requests at once
  (`max_concurrency`; default 2 for cloud, 1 for local), JSON answers (`json_mode`: off, JSON object, JSON schema;
  default JSON schema), API style (`api_style`: Chat, the default, or System One, which only the Decisions job uses;
  EldenNet ARCH §11.1; *2026-11-21, O96:* one more field on the card, from 0.4 with the decisions slot, roadmap
  S13.2.h, writable from 0.6 like the rest); *Advanced*: extra header names, whose values are write-only like keys. **Local servers found**:
  one row per probe (the four known servers on their ports, in ARCH §11.4's order; llama.cpp on 9931, then its legacy
  8080), found or not, with "Use". *Health*: per provider, healthy, cooling down (time left) or failing (the error class
  in words: unreachable, key refused, rate limited, timed out, bad reply), the last success, and the slots that use it
  (ARCH §11.2).
- **Data.** Reads `providers`, `health`. Writes `providers.update`, `secrets.set`, `secrets.clear`, `servers.find`.
- **Rules.** Keys never come back (PV2). On a paired device (G9) the page is read-only: no Add provider, Use, Set key or
  Clear key, and no field can be changed, since GraceUI §7.2 item 16 refuses `secrets.set`, `secrets.clear` and
  `providers.update` there. The base URL appears only here, never in a log, report, title or URL; elsewhere a host that
  is not a known public provider is scrubbed (PV5). In the browser view the key field is never a password-type input,
  not even briefly (no show or hide toggle that flips its type), because browsers ignore `autocomplete="off"` there and
  offer to save, and sync, what is typed. It is a text input with `autocomplete="off"`, `spellcheck="false"`,
  `autocapitalize="off"` and `autocorrect="off"`, masked by the stylesheet (`-webkit-text-security: disc`; Chromium,
  Safari, Firefox 114 and later), outside any `<form>`, and cleared once sent. In game it is GraceUI's own field, drawn
  as discs and never as text (D57). On a pad it opens the Steam keyboard, masked. No spoilers.
- **Related.** Models, Self-test, Docs › What is sent to your provider.
- *2026-11-21, as built (S11.4.a's third slice, GraceUI 0.2.8, protocol 1.2; D192 to D196):* header actions Add the
  preset's provider (`providers.add` with no fields: the chosen preset's providers that providers.toml lacks) and List
  models (`models.list`: every provider, those no job uses too). Sections: *Now* (`ai.status`); *Providers*, a table
  (Provider, Kind, Server type, Address, Key, Requests at once, JSON answers) with a **Remove** on each row
  (`providers.remove` with the row's id, destructive, confirmed; refused in words for a provider a job uses, naming the
  jobs) and the **key** on each row (`providers.key` with the id and the value, the row's secret: its cells "set" or
  "", read Set or Missing, never the key; an empty value clears it), and the preset's shortcut row; *Your own provider*,
  a small form (`providers.add` with a Name of up to 32 letters, digits, '-', '_' or '.', unique ignoring case and no
  name a key is kept under, and a Base URL checked as EldenNet ARCH §11.1 says, each refusal in words); *Models
  listed*; *Health*. Advice, Advanced, Find local servers and Local servers found are not built. Writes, as built:
  `providers.add`, `providers.remove`, `providers.key`, `models.list` and `settings.set` (`ai.preset`), not
  `providers.update`, `secrets.set` or `secrets.clear` (GraceUI D190); on a paired device these are the commands to
  refuse (GraceUI proposal 0007 §5). A key is refused, in words that never echo it, for a provider whose key is an
  environment variable's and for one reached by http:// on another computer. In game a row's key reads "Key: Set" or
  "Missing", set in the browser view, and the form reads "Filled in the browser view: Name, Base URL." (GraceUI's
  words). A provider the form adds is OpenAI-compatible with JSON schema answers; local, with no key's place, when it is
  reached by http:// on this PC or the home network (a key may then be set on this PC's row, never on the home
  network's); cloud otherwise, its key in `secrets:<id>`.
- *2026-11-21, O98 (a):* the form takes a server on another PC of the home network by its literal private address
  (RFC 1918 IPv4, or fc00::/7 IPv6) over http://, never a host name; it takes no key, and EldenNet never probes for it:
  only the player adds it (Find local servers probes this PC alone, EldenNet ARCH §11.4).
- *2026-11-21, as built (S11.4.a's fourth slice, GraceUI 0.2.9; D197):* the Key column reads Set, Missing, Refused (the
  last call was refused for its key, or asked for one; the field kept) or Not needed (no field); a key is refused in
  words on a row that reads Not needed ("This provider takes no key. If it asks for one, its row says so after List
  models."). Health says "Failing: it refused its key" or "Failing: it asks for a key". In game "Key: Refused" and
  "Key: Not needed" are GraceUI's words, and since GraceUI 0.2.11 (D200) the line "Key: set in the browser view." shows
  only when a row takes a key. The fourth slice's readings were decided as built (O104).
- *2026-11-22, GraceUI 0.2.18 (D214; WHY§378):* in game a row's key, a provider's form and EldenNet's other text and
  secret settings are typed too, a secret masked and never shown; the line "set in the browser view" and the form's
  "Filled in the browser view" give way to the fields themselves once EldenNet's floor reaches 0.2.18.

**Models** (`ai.models`; 0.2 read-only, 0.6 writable).

- **Purpose.** Which model does each job, and the preset that sets them all.
- **Header actions.** The preset (O88: Cloud budget on OpenRouter, the default; Cloud on NanoGPT; Local 12 GB;
  Local 16 GB+; Custom) and Apply · Reset to preset.
- **Sections.** **Advice**: EldenNet's advice for this machine in words (an 8 GB GPU is pointed at the cloud); a notice
  when the shipped advice changed in this version; the free-model warning (free models are not advised for prepared
  lines and autonomy, and free presets turn autonomy off, O37). **One card per job slot**, by player label with the
  slot id as a chip: *Conversation* (`dialogue`), *Quick lines* (`bark_live`), *Prepared lines* (`bark_pool`),
  *Memory* (`memory`), *Self-test* (`selftest`), from 0.4 *Decisions* (`decisions`) and from 0.6 *Profile drafts*
  (`draft`, with the helper, EldenNet DESIGN §7.7); each with its provider (a picker of the providers on Providers),
  model (a picker from the provider's model list, or text), fallback model and its provider (a card with no model of
  its own reads "Same as Conversation" until changed, §4.5; O88), a health dot and word, and the fixed
  budgets read-only (tokens, temperature, first-token and total time, retries: "the same in every preset", EldenNet
  DESIGN §11.2). The **Prepared lines** card also holds `pools.max_jobs_per_session` (its home), the pool's state in
  words (cards ready, generating, paused in a fight; counts cover every type, which are all listed (G5); pool lines are
  never listed before they play) and "Prepare lines now" (`pools.prebake`; at rest or at the title only). The
  **Decisions** card serves EldenNet's decision calls (in v1, who speaks next at a shared place; EldenNet ARCH §11.8)
  and says in words that a preset naming no model for it uses the Quick lines model, and that the rules decide when
  no model answers in time. *2026-11-21, O96, O97:* in v1 also to whom that speaker speaks, and whom the player's line
  addresses when two or more may answer it; on a provider set to System One (Providers), a chat answer from the
  fallback model is tried before the rules. Since EldenNet's decision calls came into v1 it is a card like the others, with no "WIP"
  tag (G7's note of 2026-11-16).
- **Data.** Reads `providers`, `health`, `settings`. Writes `providers.update`, `settings.set`, `pools.prebake`.
- **Rules.** Pool lines are never listed before they play. Privacy as Providers; on a paired device (G9) whatever here
  writes `providers.update` is read-only, as on Providers.
- **Keys and pad.** Pickers open as lists; type to filter in the browser.
- **Related.** Providers, Self-test, Requests, Behaviour › Combat.
- *2026-11-21, as built (S11.4.a's third slice):* sections Now, Preset (`ai.preset`: Cloud budget on OpenRouter, Cloud
  on NanoGPT, Custom), Jobs (a table: Job, Provider, Model, Fallback, State; five rows, "Same as Conversation" for a job
  that borrows) and Each job's model: five picker rows, `slots.<slot>.model` (§4.5's note), not cards per job. No
  provider or fallback picker yet, so a provider the player adds serves a job only through the player's own file; no
  Apply, Reset to preset (each row's reset does it for that job), Advice or fixed budgets.
- *2026-11-21, as built (S11.4.a's fourth slice):* each job's provider became a picker beside its model; a provider whose
  list lacks the job's model takes over once one of its models is chosen; the changed mark (D198) only on the jobs the
  providers file holds a pick for.
- *2026-11-21, as built (S11.4.a's fifth slice, GraceUI 0.2.12, protocol 1.3; G36 (a), D202; O99):* sections Now,
  Preset, then one card per job (Conversation, Quick lines, Prepared lines, Memory, Self-test), each a section with the
  slot as its chip and the job's state as its live status (`ai.jobs`: Ready, Cooling down, Resting after failures,
  Failing, No key yet, No model, in the lamp's tones), its four rows (provider, model, fallback provider, fallback) and
  its fixed budgets read-only (`ai.budgets`: Longest answer, Temperature, First token within, Whole answer within,
  Retries), "Same as Conversation" first in that list for a job that borrows. The Jobs table is gone; the cards are
  open. Not built: Decisions and Profile drafts (0.4, 0.6), Advice, Apply and Reset to preset, Prepared lines' own row,
  state and "Prepare lines now" (a section action, with the pool). EldenNet's GraceUI floor is 0.2.12.

**Self-test** (`ai.selftest`; 0.2).

- **Purpose.** Is the model fast and well-formed enough for conversation?
- **Header action.** Run self-test (one at a time; gated like Find local servers).
- **Sections.** **Verdict** (quick ≤ 1.6 s, acceptable ≤ 3.0 s, slow beyond, for the estimated first page; EldenNet
  DESIGN §11.5) · **Figures** (first token, tokens per second, the JSON check, the test line from the test persona as
  shown) · **At start-up** (this session's silent self-test) · **Earlier runs this session** (time, model, verdict).
- **Data.** Reads `selftest`. Writes `selftest.run`.
- **Rules.** The test line is shown here and in First light only (PV4), never logged.
- **Related.** Models, System › Traces.

**Requests** (`ai.requests`; 0.2).

- **Purpose.** Every model call this session and what it cost, without prompt text.
- **Tiles.** Calls · Tokens in · Tokens out · Failures · Own-initiative calls this hour (against the bound) · Cost
  (only where the provider's reply reports it, D18).
- **Filter bar.** Slot · Purpose (reply, own-initiative line, call-out reply, prepared-line batch, memory work,
  self-test) · Outcome (shown, not shown, fallback model, timed out, refused, failed; pool filled for a prepared-line
  batch) · sort. No free-text search. "Not shown" covers any answer that never reached the screen: silence, an
  answer withheld by a check, or one dropped unshown when its conversation closed (NPC §5.7); an own-initiative call
  that ends so still reserved its slot (EldenNet O37). Home › Model calls uses these purpose and outcome words.
- **Table.** Time, slot, purpose, speaker (as shown, SP3, or a type label), provider and model, first token, total,
  tokens, outcome; the row drawer shows the displayed text only if it was shown, and the error class in words.
- **Data.** Reads `calls` (20 kept, 200 in developer mode), `usage`.
- **Rules.** No prompt or unshown text, ever (invariant 1); no base URL.
- **Related.** Models, System › Traces, System › Logs.
- *Since 2026-11-24, as built (engine 2f27925; GraceUI D250, D252):* the slot is shown as its job's label ("Job"); the
  row drawer's fields are the table's last two columns, Why and Text shown, until GraceUI has a row drawer; the hour's
  line under the tiles gives own-initiative calls and every request against the bound; cost is shown only once a
  provider reported one, with how many calls did; the page is live.

**Director** (`ai.director`; a later-version statement). A director that chooses who speaks next and starts scenes on
its own is a later version (EldenNet DESIGN §13, the `Directed` class). In this version NPCs speak through greetings,
quest business, news, NPC-to-NPC exchanges and autonomy, held by pacing and allowances. Related: Behaviour ›
Speaking up, Behaviour › Pacing. No controls.

#### 4.6.3 Voice

In v1 NPCs speak in text, so Voice opens with the text that carries their words; spoken voices follow in v1.5 (D43).

**Text and subtitles** (`voice.text`; 0.4 in the browser, D17; 0.6 in game).

- **Purpose.** How NPC lines appear on screen.
- **Sections.** **Lines**: `features.labels` (floating labels over speakers, or the feed at the side). **Subtitles**:
  `ui.follow_game_subtitles`. **Size**: a link to GraceUI › Menu, the home of `ui.scale` (GraceUI's D115; 75–150%; the
  menu and every EldenNet text follow it; in game the atlas rebuilds when the menu closes, D29). **HDR**: `ui.hdr`
  (auto, off, on), `ui.paper_white_nits`. **Preview**: "Show a sample line" (`preview.subtitle`; in game drawn by
  EldenNet's real surfaces for 3 s after the menu closes; in the browser a drawn sample).
- **Data.** Reads `settings`. Writes `settings.set`, `preview.subtitle`.
- **Rules.** The sample line is ours.
- **Related.** Behaviour › Conversation, Behaviour › Combat, GraceUI › Menu.

**Text-to-speech** (`voice.tts`; v1.5, a statement page in v1). What v1.5 adds (a voice per persona and card through
an out-of-process engine, page timing from the audio), and what never comes (cloning the cast's voices). A read-only
summary of the `tts` allowance by kind, each row a link to Characters › NPC types; voice keys once content carries
them. No controls in v1. Its v1.5 controls (engine, endpoint, test voice, volume) are designed with v1.5.

**Speech-to-text** (`voice.stt`; v1.5, a statement page in v1). Talking aloud to named NPCs arrives in v1.5
(`InputSource::Stt`); typing stays. Push-to-talk will live in Settings › Hotkeys. No controls in v1.

#### 4.6.4 Characters

Playthrough holds this journey's state; Named NPCs and NPC types hold the roster, which is the same in every
playthrough and listed from the start with only what a first meeting shows (G5, G8, D64): every named NPC and every
named boss (EldenNet O43, as updated; D67).

**Playthrough** (`characters.playthrough`; 0.3, its story lines and World now from 0.5; D63–D65).

- **Purpose.** The people you have met on this journey, how they are now, and where their stories stand as you have
  seen them.
- **Layout.** **Tiles**: People met · Stories seen (met people with a story line shown) · Spoken with this session.
  **World now**: a card with the line of the highest-ranked world stage that holds (SP6) and a link to History › World;
  absent while `ui.show_world = false`. **Filter bar**: names as shown · status · region (regions entered) · sort
  (last met, first met, name). **Master–detail** (D36). The table: the name as the game shows it (SP3), status, last met
  (a place by its runtime name in a region entered, and play time ago), their story as last seen (shortened), since you
  last spoke (the first note). The detail: the name; **Status** (the word, where and when last met, times met, the
  disposition word); **Their story** (the line for the stage seen, with when it was seen); **Related** (for each met
  person that stage's line links, their own line and a link to their row); **Has heard of** (the world stages that hold,
  each with a certainty word); **Since you last spoke** (up to three notes, each from a record: a world stage that began
  to hold since the last conversation, meetings since, a fact such as "fought beside you"); links to Named NPCs (the
  general profile), History › Memories and History › Conversations, filtered to them.
- **Status words**, from perception only (SP7): *Here now* (perceived now); *Alive when last met* (with where and
  when); *Away* (the Tarnished came within speaking range of where they were last met and did not perceive them);
  *Fell* (the Tarnished perceived their death). Never from a flag alone, so a death the Tarnished did not see reads
  "Alive when last met".
- **Data.** Reads `playthrough`, `world`. Writes nothing. An earlier journey chosen in the top bar shows read-only.
- **Rules.** Only people met on this journey (SP4, SP5). A story line only for the stage active at the last meeting
  (EldenNet's `stage_seen`), never a later or unseen one; a related story only for a met person the stage's own line
  links; never a stage id, road, gist, track, knowledge entry, next step, "has something for you" mark, fate or count of
  what is left (SP1, SP5). Story lines are authored and reviewed when written; notes are our labels (SP2). No chat or
  model text (PV4); lines and notes are never searched (GraceUI §4.3). At the title screen: "Load a character to see
  this".
- **Keys and pad.** ↑↓ the table, → or A into the detail.
- **Related.** Named NPCs, History › Memories, History › World.

**Named NPCs** (`characters.named`; 0.3 for the pilot, 0.5 for every persona and the roster's first bios, the whole
base-game roster by EldenNet v1.0).

- **Purpose.** Every named character EldenNet voices: who they appear to be, and what each may do. The same in every
  playthrough (D64).
- **Layout.** Two panes. Left: search (names as shown, or first-meeting names) and the chips Met · Nearby · All; the
  whole roster from the start (G8): every named NPC and every named boss with a shipped bio, in name order, a boss the
  game never shows calm before its fight with a "Boss" tag (D67), by the name the game shows or, until it shows another,
  the first-meeting name (the pre-reveal fallback, SP3); then named NPCs without a shipped bio once seen (SP4), marked
  "no lines in this version" unless the player's own profile gives them a voice. Met and Nearby follow the journey in
  view and are absent at the title screen. Right, tabs: **Profile** (the name, with the veiled look for a perceived
  character whose name the game has not shown, SP5; the **general profile**, from its bio's first-meeting layer (a
  persona's, or the roster bio of every other named NPC and named boss, EldenNet O46), each field reviewed at the
  opening state when written (the Stranger Test, EldenNet NPC §3.1): who they appear to be (`public.summary`), what they
  do (`public.role`) and, for one that speaks, what they like to talk about (`subjects`); the news temperament in words;
  for a met character, an "On this journey" line with the status word and a link to their row on Playthrough);
  **Allowances** (the slot's row of `allow."<slot>"`, its slot `persona:…`, `character:…`, `mine:…` or `named:…`, as
  shortcuts, with inherited values faint and their source); **Reactions** (this slot's per-reaction tri-states,
  `allow."<slot>".reactions.<id>`, as shortcuts into NPC types; cooldowns are the same for every NPC and live on
  Behaviour › Reactions). Every character, met or not, opens on Profile.
- **Data.** Reads `named`, `named/<id>`, `kinds`. Writes `settings.set` (shortcut rows).
- **Rules.** A general profile holds first-meeting knowledge only (G8): its bio's `public.summary`, `public.role` and
  `subjects` (EldenNet O46); never a stage, fate, later form, reveal, quest step or relation the story discloses later,
  nor a road, gist, private field, tie, forbid list, beckon, knowledge entry or quest track (SP1). Every named boss and
  every named invader with a shipped bio is in the roster from the start, by its first-meeting name (for a boss the game
  never shows calm before its fight, the name its bar shows), read from the player's install; its general profile says
  only what a first meeting shows: for such a boss, which has the seen fields only and never converses (EldenNet O46;
  one of a speaking kind keeps its combat lines, O42), what the opening of its fight shows; for an NPC the game can
  show calm, that meeting, never a later fight (D67). Bosses and invaders without a bio stay out until the game names
  them (SP4).
- **Keys and pad.** ↑↓ the list, → or A into the detail, LB/RB the tabs.
- **Related** (per character): Playthrough (their row), History › Memories and History › Conversations filtered to
  them, NPC types.

**NPC types** (`characters.types`; 0.4 in the browser, 0.6 in game).

- **Purpose.** Every type of NPC, labelled and tagged, and what each may do (O38). The home of every allowance.
- **Toolbar.** Search · kind chips · Reset page.
- **Master.** Every kind and type from the start (G5), in EldenNet DESIGN §7.6's order: each kind's own row ("Named,
  every NPC", "Humanoid, every type", …; for a kind whose only type is the kind itself, such as boss, invader or
  mount, that row is the type), followed by its slots or types (archetype cards are the humanoid rows), then a locked
  "unknown" row. Each row: our label, tag chips (hollow while a tag rests on an unverified lead), how many the
  Tarnished perceives near now (SP7), and summary marks for Speech, Unasked and Battle.
- **Detail.** The type's general profile (what anyone would see on meeting one, in our own words, G5), then
  allowances as tri-states (inherit, on, off) showing the inherited value and its source, which is a link of EldenNet
  DESIGN §7.6's chain (named slot, type, live kind, base kind, the shipped default); a source with a row of its own
  here opens that row, on this page or from Named NPCs. A reaction's own switch on Behaviour › Reactions is a separate
  gate (ARCH §11.5), never shown as a source; a row it turns off says so in a faint second line. Grouped: Speech
  (`dialogue`; `tts` "takes effect in v1.5"), Unasked (`greet_player`, `npc_to_npc`, `autonomy`, `news`,
  `stage_talk`), Reactions (per reaction id, and `status_reactions`, which gates the four Afflictions reactions),
  Battle (`combat`, `boss_fight`), Awareness (`self_awareness`, `status_awareness`), Acts (`actions`, `gestures`,
  locked "later version", O41). `dialogue` off greys the rest. A card's detail adds its voice source and pool state; a
  type with no voice source says "no lines in this version".
- **Data.** Reads `kinds`, `settings`. Writes `settings.set`, `settings.reset`.
- **Rules.** Every type from the start (G5), each described in first-meeting words; no boss names in type labels or
  tags (a named boss appears only as its roster slot among the named slots, by its first-meeting name, D67); tags
  never name a place or a quest. Near counts follow SP7.
- **Keys and pad.** Master–detail (D36); ← → cycles a tri-state; X resets the row. Wide browser windows add a grid
  view.
- **Related.** Playthrough, Named NPCs, Behaviour › Reactions, Behaviour › Combat.

#### 4.6.5 History

Every History page shows the journey chosen in the top bar; earlier journeys are read-only. Nothing in History is
editable but Forget (D13). Before a playthrough is loaded, each page says "Load a character to see this".

**Events** (`history.events`; 0.4).

- **Purpose.** What the Tarnished saw happen on this journey, as EldenNet recorded it.
- **Tiles.** Events this journey · This session · Lines shown.
- **Filter bar.** Family (fights, statuses, speech, greetings and meetings, world) · who (names as shown, or type
  labels) · region (regions entered) · sort · direction. No free-text search.
- **Table.** Play time, the event in our label, a family pill, subject and other (SP3), region, a confidence word
  (certain, likely), who heard it (met NPCs, from the event's audience).
- **Data.** `history.page` with `view: events` (GraceUI §6.6).
- **Rules.** Only events the Tarnished perceived (SP7; EldenNet's `seen` bit, GraceUI's eldennet-changes §2); plumbing
  events (flag and stage changes, readiness, read groups) are never listed; every text is our label or a name (SP2,
  SP3).
- **Related.** Tools › Event monitor, Conversations, Behaviour › Reactions.

**Conversations** (`history.conversations`; 0.3).

- **Purpose.** Every conversation with a named NPC, as it was shown.
- **Layout.** A list (the NPC as shown, when, how many lines, how it ended) and the transcript of the chosen one: the
  Tarnished's typed lines and the NPC's displayed lines, nothing unshown.
- **Data.** `history.page` with `view: conversations`; `conversation/<id>`.
- **Rules.** Only lines that were shown, as they were shown (SP2). Chat text on screen only (PV4): never in search, a
  title or a URL; ids are opaque numbers.
- **Related.** Memories, Characters › Named NPCs.

**Memories** (`history.memories`; 0.3).

- **Purpose.** What each NPC with a profile remembers of the Tarnished and of other NPCs (D68), and the means to make
  them forget.
- **Header.** `features.memory` (Named NPCs remember), the page's own switch and its home (D45).
- **Tiles.** NPCs met · Keepsakes · Memory work waiting.
- **Filter bar.** NPC · basis (seen, you told them) · sort (time, weight).
- **NPC card**, with one NPC chosen: the journal's summary, the disposition word, the name the Tarnished told, when the
  journal was last written, and the facts in words, from MEM §2.2's rendered keys only (times met, where you first
  met, seen you pass by, fought beside you, saw you fall, times struck). Facts are counters EldenNet keeps, with no
  weight and no text of their own. Never the `stage_seen`, `world_seen`, `world_seen_pt`, `world_cued`, `spoke_pt` or
  `verified:` keys (SP1); "fought beside you" without the boss's name (SP4); a place by its runtime name, and only a
  region entered (SP3).
- **Table** (keepsakes only, at most 8 per NPC, each one witness's own memory, MEM §2.3). NPC, when, the keepsake,
  weight (a number and a word), basis ("Seen", or "You told them" for a claim, MEM §2.5), about (you, themselves, the
  world, or another NPC by the name the game shows, D68), and a mark on the one protected keepsake (the heaviest about
  you with weight 0.7 or more, MEM §2.3).
  A name the Tarnished told is shown on the NPC card, never as a row (MEM §2.5 drops it as a duplicate).
- **Toolbar.** Forget this NPC (with an NPC chosen) · Forget this journey (in `rot`, at the right). Both name exactly
  what goes and what stays, take a backup and need a deliberate second press; in game, a danger close cancels them
  (MEM §9.5).
- **Sections.** **Memory work** (waiting, running, the last result in words) · **Backups** (count and times, no
  paths). A note: "Diaries: later version." Memories of scenes are in v1: each NPC that witnessed a scene writes its
  own, one per witness (EldenNet MEM §2.3, §3.1), and they show as its keepsakes.
- **Data.** Reads `memories`, `journal/<id>`. Writes `settings.set`, `memory.forget_npc`, `memory.forget_journey`.
- **Rules.** Journals and keepsakes are shown as EldenNet's memory job wrote them, from grounded inputs only (SP2;
  EldenNet O86). Model text on screen only (PV4); no paths.
- **Keys and pad.** Forget: A, then a second A on the confirmation's focused "Forget" after 1 s.
- **Related.** Conversations, Characters › Named NPCs, Settings › Developer.

**World** (`history.world`; 0.5).

- **Purpose.** What has changed in the world, as far as the Tarnished has seen, and who has heard (O28–O30).
- **Contents.** The world stages that hold by verified flags only (SP6), each by its impersonal `line`, in rank order;
  for each, the met NPCs who have heard, with a certainty word. No counts, locked rows, "x of n", "not yet", quest
  tracks or NPC stages (D11). EldenNet's knowledge base itself is never shown (SP1).
- **Data.** Reads `world`. Shown unless `ui.show_world = false`.
- **Related.** Characters › Named NPCs, Behaviour › Speaking up.

#### 4.6.6 Behaviour

**Conversation** (`behaviour.conversation`; 0.2 in the file, 0.6 for the call-out and the page in game).

- **Purpose.** Talking with named NPCs, calling out to others, and how faithful lines are.
- **Sections.** **Conversation**: `features.chat`, with one sentence. **Call-out**: `features.callout`. **Content
  safety**: `safety.level` (faithful, gentle; O17), with "Safety and spoiler rules always apply". **Keys**: talk and
  skip page (shortcuts), reach (`range.talk_m`, a shortcut), following the game's subtitles (a shortcut). **Notes**:
  typing only at safe moments (O9), the Steam keyboard on a pad (O11), names only once told (O18), lines to the
  Tarnished wait while this menu is open (D34). A later-version line: choosing from a list of intents on a pad (v1.1).
- **Data.** Reads `settings`. Writes `settings.set`.
- **Related.** Settings › Hotkeys, Speaking up.

**Speaking up** (`behaviour.speaking`; 0.3 for named NPCs, 0.4 for all).

- **Purpose.** Everything about NPCs starting to speak on their own (O32, O34, O37, O39).
- **Sections.** **Who speaks unasked**: `features.speak_up` (named NPCs) and `features.barks` (other NPCs), homed here,
  and talk per window (`pacing.talk_max`, a shortcut). Which types may speak unasked is set per type on NPC types and
  per NPC on Named NPCs, not here. **Greetings**: the `greet_player` row (a shortcut); in v1 only named NPCs greet the
  Tarnished, because the cards are hostile. **Quest business**: the `stage_talk` row (a shortcut) and one readiness word
  per met named NPC (ready; waits for a game check on your build; not in this version); unmet personas show nothing here
  (SP5); never a stage, never a "has something for you" mark. **News**: the `news` row (a shortcut) and one
  sentence on when news is raised. **Between NPCs**: `features.npc_to_npc` (homed here), the `greet_npc` and `answer`
  rows (shortcuts) and the overheard-lines range (`range.hear_m`, a shortcut); exchanges end by themselves.
  **Autonomy**: `autonomy.enabled` and `autonomy.period_s` (homed here), the bound in words, the free-preset note.
  **Calm remarks**: the `idle`, `saw_kill` and `saw_victory` rows and `pacing.idle_max` (shortcuts). Every reaction
  shortcut here is drawn as on Reactions (principle 1): its switch and its cooldown slider (0–6 h of play), each help
  line ending with ARCH §11.5's default ("Default: on, 10 min").
- **Data.** Reads `settings`, `named` (readiness). Writes `settings.set`.
- **Related.** Reactions, Pacing, Characters › NPC types, AI › Director (WIP).

**Combat** (`behaviour.combat`; 0.4).

- **Purpose.** Combat lines for every combatant, either side, boss fights included (O42). A hub: every row is a
  shortcut to its home (D47).
- **Sections.** **Who speaks**: `features.barks` (a shortcut) · a kinds × {combat, boss fight} matrix (shortcuts
  into NPC types) · **Scripted-line holds** per kind
  (plays; held until its check passes on your build, S3; no check needed) · the In battle reaction rows · combat
  pacing (`pacing.group_gap_s`, `pacing.fight_max`, `range.fight_m`) · **Lines from**: written lines and prepared
  pools per card, with `pools.max_jobs_per_session` · labels or feed.
- **Data.** Reads `kinds`, `settings`, `health` (the pools' state). Writes through shortcuts only.
- **Rules.** Every card is listed (G5); no boss names; pool lines never listed before they play.
- **Related.** Reactions, Pacing, Characters › NPC types, AI › Models.

**Awareness** (`behaviour.awareness`; 0.4, each status as S8 passes).

- **Purpose.** What NPCs know of themselves and of statuses (O35, O40).
- **Sections.** **Self-awareness**: `features.self_awareness` (their own recent lines, and how they are now).
  **Status awareness**: `features.status_awareness`; the seven statuses in our words (frost, poison, rot, bleeding,
  madness, sleep, blight), each "live" or "waits on the status check" (S8); the Afflictions reaction rows and the
  `status_reactions` and awareness allowances per kind (shortcuts). A later-version line: fleeing, healing, gestures
  and warnings while a status builds (O41).
- **Data.** Reads `settings`, `kinds`. Writes `settings.set`.
- **Related.** Reactions, Characters › NPC types, Behaviour › Acts (WIP).

**Reactions** (`behaviour.reactions`; 0.4).

- **Purpose.** Every kind of event NPCs may react to, with its switch and cooldown. The home of all twenty
  `reactions.<id>` rows.
- **Filter chips** (families): In battle (`engaged`, `struck_hard`, `low_hp`, `ally_fell`, `foe_fell`, `player_fell`,
  `foe_fled`, `foe_healed`) · Afflictions (`status_self`, `status_plea`, `status_warn`, `status_foe`) · To you
  (`greet_player`, `stage_talk`, `news`) · Between NPCs (`greet_npc`, `answer`) · Calm remarks (`idle`, `saw_kill`,
  `saw_victory`). Autonomy is not here: its switch and period live in Speaking up.
- **Table** (editable cells): label · On · Cooldown (a slider with units, 0–6 h of play) · scope in words · lines from
  (written, prepared, live, with a cost mark) · availability · this session (played, refused) · overrides ("n types",
  a link to NPC types).
- **Foot.** A locked row: "Your own event rules: later version" (`augur.allow_custom_rules`).
- **Data.** Reads `settings`, `monitor.decisions` (session counts). Writes `settings.set`, `settings.reset`.
- **Keys and pad.** ↑↓ rows, → across cells, ← → on a cooldown changes it, Shift for ten steps.
- **Related.** Tools › Event monitor, History › Events, Characters › NPC types.

**Pacing** (`behaviour.pacing`; 0.4, tuned by testers, EldenNet X2).

- **Purpose.** How often anyone speaks unasked, and how far lines reach.
- **Header actions.** Presets Quiet · Default · Lively (D20; each writes the six pacing keys).
- **Sections.** **Gaps**: `pacing.gap_s`, `pacing.group_gap_s`. **Window**: `pacing.window_s`, `pacing.fight_max`,
  `pacing.idle_max` (off, sparse, normal, custom), `pacing.talk_max`. **Reach**: `range.talk_m`, `range.hear_m`,
  `range.fight_m`. **What this means**: a computed sentence ("At most 6 lines a minute in a fight, …"). A note: replies
  to the Tarnished are never limited.
- **Data.** Reads `settings`. Writes `settings.set`.
- **Related.** Speaking up, Combat, Reactions.

**Acts** (`behaviour.acts`; a later-version statement). NPC acts and gestures (fleeing, healing, giving, gestures) are
a later version (O41, EldenNet DESIGN §13); v1 is dialogue only and never writes to the game. The `actions` and
`gestures` allowances show locked on NPC types. No controls.

#### 4.6.7 System

**Status** (`system.status`; 0.2 for Health, the rest as their sources exist; usable at L1).

- **Purpose.** Is EldenNet healthy on this machine and build, and if not, why?
- **Tabs.** **Health**: the level and its reason; the game build and layout profile, supported or not; the eleven read
  groups with their self-test results and breaker trips; the capability matrix in words; the gate (offline, own world,
  lobby) in words. **Checks**: each feature waiting on a game check on this build, in player words, with the spike named
  for testers. **Compatibility**: supported game builds; how EldenNet was started (its launcher or another loader);
  GraceUI's installed version, the one this EldenNet came with, and its folder's check (GraceUI §2.9); overlays and
  other hooks recognised at start-up by module name; Proton and Steam Deck notes; known pairs that do not work.
- **Data.** Reads `status`, `system.status`.
- **Rules.** Checks name features, never flags, stages or characters.
- **Related.** Performance, Logs, Docs › Troubleshooting.
- *2026-11-21, as built (S11.4.a):* Health (the state, its reason, the start, the game build, offline and solo, the read
  groups' count), EldenNet's parts as lamps (the start, reading the game, the core, a refused providers file with why
  in words and its line, never its text, the anti-cheat watch, the log, the recording, GraceUI) and the read groups'
  self-tests, whose empty words say they appear after the first reading; Compatibility as the same topic's
  `compatibility` member (GraceUI D185), so `system.status` is the page's one topic. Logs lists newest first in the
  browser (D187).

**Performance** (`system.performance`; 0.2).

- **Tiles.** Frame cost with the menu open and closed (p99), the core step p99, memory.
- **Sections.** **Budgets** (C3's limits beside the figures) · **Tasks** (p99 per task) · **Storage** (the database's
  size, the backups' count and last time; no paths, no SteamID).
- **Data.** Reads `system.performance`.
- **Related.** Status, Traces.
- *Since 2026-11-24, as built (engine 0b0030b; WHY§638, §639; O128 open):* the tiles are the core's step p99 and the
  reader's step p99 (C3's task); frame cost and memory are absent until a source exists, since v0.2 runs none of its
  code in the game's frame and counts none of its memory (GraceUI D259's `on_present` gives the first, by its menu-open
  flag, once EldenNet takes it). Budgets holds C3's reader's mean and p99 as checks, Passed or a Warning; Tasks a row
  per kind of step (count, mean, p99, longest); Storage in words, since v0.2 keeps no database and no backups.

**Traces** (`system.traces`; 0.2).

- **Purpose.** How long each line took, from its cause to the screen.
- **Tiles.** First page p50 and p95 against C5's targets · first token p50 · lines this session.
- **Table.** Time, kind (reply, own-initiative, call-out), slot, queue, first token, first page shown, total, verdict
  (quick, acceptable, slow). No text.
- **Data.** Reads `traces`.
- **Related.** AI › Requests, AI › Self-test, Settings › Developer (latency toasts).
- *Since 2026-11-24, as built (engine 0b0030b; WHY§640, §641; O128 open):* times run from the request's handover;
  Queue is the model client's wait before the first attempt left; the first page is the reply's first page committed,
  whatever it said. The p95 tile and its clause appear from 20 lines on, held to C5's p90 target; the targets follow
  where the Conversation model runs. A verdict for replies (ARCH §11.7's rule) and call-outs (DESIGN §1.4's) alone.
  The slot is shown as its job ("Job"). As many lines as the calls' record keeps rows (20, 200 with Developer views).

**Logs** (`system.logs`; 0.2).

- **Contents.** Level tiles that filter (error, warning, info, debug, trace) · search within the scrubbed tail · the
  scrubbed log tail, newest first (500).
- **Data.** Reads `logs`.
- **Rules.** Scrubbed as reports are (PV5); no download of raw files (Bug report makes the shareable bundle).
- **Related.** Tools › Bug report, Settings › Developer (log level).

#### 4.6.8 Tools

**Event monitor** (`tools.monitor`; 0.4).

- **Purpose.** Watch events and EldenNet's decisions live: why a line did or did not play.
- **Header actions.** Pause view · Clear view (the view only).
- **Segmented switch.** *Events*: tiles (events a second, shown, total); a family filter; rows of time, event, family,
  subject and other (SP3), confidence, who perceived it, reactions requested; 500 rows in the browser, 100 in game.
  *Decisions*: each own-initiative request and its result (played, or refused by which rule, with the cooldown left)
  and its line source.
- **Data.** Reads `monitor.events`, `monitor.decisions`.
- **Rules.** SP7; plumbing events only in developer mode, under the Spoilers ribbon (SP8).
- **Related.** History › Events, Behaviour › Reactions.

**Bug report** (`tools.report`; 0.2 preview, 0.6 copy and save).

- **Contents.** What happened (free text, with "don't include personal details"); the bundle preview with every field
  visible and the optional parts switchable; copy or save only after EldenNet's deterministic scan passes.
- **Data.** Reads `report`. Writes `report.build`, `report.save`, `report.copy`.
- **Rules.** Never chat or model text, keys, SteamID, paths, the Windows user name, the character's name or local
  addresses (PV5, PV7).
- **Related.** System › Logs, Docs › Troubleshooting.
- *Since 2026-11-24, as built at 0.2 (engine 0538221; GraceUI D251):* "What happened" is a prose form field
  (`report.build` {what}, 2000 characters at most) under the caution; the preview's parts are What happened, Versions
  and State, always in, and Settings, Content and the recent log, each switched by `report.part` {id, on}; every
  value is scrubbed of a SteamID's, a path's and a local address's shape, and the typed words stay in memory for the
  preview alone. Related is System › Logs and Docs (Related names a page, not one document). `report.save` and
  `report.copy` come at 0.6, marked sensitive.

**Workshop** (`tools.workshop`; 0.2; the browser only, with `debug.panel` on).

- The last prompt and reply (keys redacted), every event unfiltered, the world view with raw verification states,
  each persona's known set and the cue that would fire now, unknown characters with "copy as TOML", recording
  controls, Augur's event kinds and schemas.
- A "contains spoilers" confirmation once a session, and the red Spoilers ribbon on every page while it is open
  (SP8).

#### 4.6.9 The foot

**Settings** (`settings`; 0.6 for all of it; Developer and Browser view readable in the browser from 0.2).

- **Purpose.** The home of the settings that belong to no category page, and an index of all the rest (D40).
- **Header actions.** Run setup again (First light) · Reset all (confirmed) · Copy settings (`settings.toml` holds no
  secret; PV7).
- **Tabs.** *Settings*: two columns. Left, a navigator card with a search box, then groups. Groups that are homes here:
  **General** (`enabled`, with "Safety and spoiler rules always apply"; `ui.show_world`), **Hotkeys** (`hotkeys.talk`,
  `hotkeys.skip_page`, `hotkeys.debug`; capture, conflicts, reset; pad chords only if S4 finds one, D33; a link to
  GraceUI › Hotkeys for the menu key and every mod's bindings; *since 2026-11-22, GraceUI's G37 (WHY§362): a read-only
  list of EldenNet's hotkeys from GraceUI's topic, with that link, the three keys being defaults EldenNet registers and
  every binding changed on GraceUI › Hotkeys*), **Developer** (`debug.panel`, `debug.record_sessions`,
  `debug.record_redact_text`, `debug.log_level`, `ui.latency_toasts`). Groups that link out, each sub-item with an
  outward arrow and no rows: GraceUI (Menu, Hotkeys, Browser view; its own pages, GraceUI's D115), AI (Providers,
  Models), Voice (Text and subtitles), Characters (NPC types), History (Memories), Behaviour (Conversation, Speaking up,
  Awareness, Reactions, Pacing). Right, the chosen group's sections. *All settings*: every setting, grouped by category
  and page, each row a shortcut marked "lives in …"; filters for changed from default and for availability (live, waits
  on a game check, v1.5, later, off because …).
- **Data.** Reads `settings`. Writes `settings.set`, `settings.reset`.
- **Keys and pad.** The navigator and the content are two scopes; → moves into the content; Enter on a link-out item
  opens its page.

**Docs** (`docs`; 0.2 in the browser, 0.6 in game).

- **Purpose.** EldenNet's documentation, bundled and offline (PV6, D46).
- **Documents.** Getting started · Installing and starting EldenNet, and updating GraceUI · What is sent to your
  provider (the AI disclosure) · Privacy · Spoilers: how EldenNet keeps them · Keys and pad · Troubleshooting (which
  names EldenNet's command-line tools) · Writing content (personas, cards, overrides, event schemas) · Licences and
  credits (`NOTICE`) · About (EldenNet's and GraceUI's versions, build).
- **Layout.** The document list at the left, the reader at the right; in game, plain styled text.
- **Rules.** Written to name no late-game person, place or event, and reviewed like content. Search indexes titles and
  headings only.
- *Since 2026-11-24, as built (engine 0538221; GraceUI D250, D251):* a master page, its list's own search over titles
  and headings; each document a family member with one document block; the ten bundled in the DLL. It shows in game
  from 0.2 too, since a page cannot yet be kept out of one view.

**Content packs** (`content`; 0.2).

- **Purpose.** What content EldenNet is running: the shipped pack and the player's own overrides, with no hub and no
  downloads (D46). Plugins have a page of their own from 1.1 (D104), which this page links to.
- **Sections.** **Shipped content** (name, version, credit; persona and card counts, since the cast is published;
  knowledge and rules as "included", with no counts, SP5) · **Your overrides** (relative paths under `overrides/`,
  each loaded or refused, with file and line) · **Turned off** (`disable.toml`: personas and cards by id; knowledge
  entries, triggers and rules as counts) · **Reloads** (time, layer, result).
- **Data.** Reads `content`. Writes nothing (no content editors, D2).
- **Rules.** Relative paths only (PV5).
- *Since 2026-11-24, as built (engine 86d3991; O119; WHY§602):* a file of the overrides folder is Loaded, Refused, "Not
  loaded: another file was refused" (an error outside a player's layer refuses the whole load) or "Not read" (one the
  DLL left out: a link, a folder too deep, past 256 files, unreadable), with a row per problem; a path that is not fit
  to show goes on an "Other files" row. The reloads' layer is what the load read: "Shipped content and your overrides"
  at the start, "Your overrides" after the profile editor saved. Until the pack has a manifest its name and credit are
  fixed words; cards and the turned-off knowledge, triggers and rules stay empty while the content layer gives none, and
  a layer's problem names its field without a line. The page is a category of its own after Settings, with no rule
  above it, until GraceUI draws a foot for a mod's categories.
- *Since 2026-11-24, at GraceUI 0.2.44 (D248; engine 0538221):* Settings, Docs and Content packs are the rail's foot,
  under its rule, in that order.

**Plugins** (`plugins`; 1.1 for content plugins, code plugins after v1.0; D104).

- **Purpose.** The plugins EldenNet runs, as the owner designed the page (EldenNet O70): a list of plugins in rows, each
  with its switch, and for a code plugin whether it runs in the plugin cluster or detached in a host of its own.
- **Rows.** Name, version and author; the **On/Off** switch; for a code plugin, **Cluster** or **Detached**; the state
  (on, off, refused, stopped, waiting for a restart), with a library clash or a failed check in words. Content
  plugins keep their order, which decides between two plugins holding the same file. Opening a row shows the licence,
  the summary, what the plugin may reach (a code plugin's, from its imports), its check results by file and line, its
  reports, and Remove.
- **Head of the list.** In the browser only: the drop zone with **Add file** (content plugins; this PC only, never code;
  the file sent by GraceUI §7.2 item 18's route, G28). When code plugins have stopped, a banner with the plugins-stopped
  pop-up's choices (D105).
- **Data.** Reads `plugins`. Sends EldenNet's plugin commands: `plugins.enable`, `plugins.mode`, `plugins.order`,
  `plugins.remove`, `plugins.restart` (with the pop-up's choice), and `plugins.install`, which EldenNet marks
  sensitive (D81).
- **Rules.** Relative paths only (PV5); a plugin's own text (its name, summary and reports) is always text, never
  markup (D81).

#### 4.6.10 Outside the rail

- **First light** (D16): notices (offline only, save backups, data sent to the provider, the AI disclosure) → status
  → provider (a masked key or a local probe) → model preset → self-test with its verdict (EldenNet's checks 4a–4e) →
  optional prepared lines → text size (G6) and the browser view (its address, and the switch to turn it off; G4) →
  done, with a checklist and "Open Docs › Getting started". It opens at the title screen on first run (in the browser
  too, if the player opens the dashboard first), and again from Home › Setup and Settings.
- **Modals**: the Forget confirmations, Reset page or Reset all, key capture, the Workshop spoiler confirmation, and
  from 1.1 the plugins-stopped pop-up (D105).
- **Search** (GraceUI §4.3) is a modal scope over the page.

### 4.7 The settings row

GraceUI's (D82): GraceUI §4.7. Each row is generated from EldenNet's settings description (GraceUI §6.9).

### 4.8 Spoiler rules (from IA §6; binding on both views)

- **SP1. Projections, not raw content.** The core never sends the UI flag ids, catalogue aliases, notes, stage ids,
  roads, gists, private fields, a bio's revealed, companion and lived layers, forbid lists, knows, beckons, ties, heard
  lines, knowledge text, prompts or model text that was not displayed. The only authored texts shown are written to be
  impersonal: `public.summary`, `public.role` and `subjects` (the roster's general profiles, every named NPC's and
  named boss's, G5, G8; EldenNet O43, O46), a world stage's `line`, a persona stage's public line for the stage seen
  (Playthrough), our labels and our help. Workshop is the one exception (SP8).
- **SP2. Shown, grounded or impersonal (was the lock filter).**
  *Later (2026-11-15):* EldenNet's O86 keeps spoilers by grounding alone and drops its locked-term sets and output
  lock, so this filter goes; SP2 is re-cut in EldenNet's fold (its roadmap S7.13.b): the views show only what was
  shown in game or written impersonal (SP1).
  *Re-cut (2026-11-16):* a view holds only what the game showed (lines displayed, names as SP3 allows, perceived events
  in our labels, SP7), what EldenNet wrote from the same grounded inputs as its prompts (journals and keepsakes; from
  1.1, lore notes), and SP1's authored texts, each reviewed when written (the Stranger Test for public fields, EldenNet
  NPC §3.1). Nothing is checked, dropped or counted for spoilers at projection time: each of EldenNet's prompts and jobs
  holds only what the NPC knows at its stage, and a few test questions per character measure what the model itself
  knows of the game (EldenNet O86).
- **SP3. Names as the game shows them.** On-screen names come from the runtime name of the current row, or the
  first-meeting name (the pre-reveal fallback); a veiled persona stays veiled; regions by the names of regions
  entered. GraceUI stores no name.
- **SP4. Seen before listed.** Named NPCs without a shipped general profile appear once seen; regions once entered;
  bosses and invaders without a shipped bio never by name before the game names them, and bosses never by name outside
  a live bar except in the roster. The roster is the exception: every NPC type, and every named NPC and named boss
  with a general profile, is listed from the start by its first-meeting name (G5, G8, D67).
- **SP5. Absence is silent.** No counts, progress, placeholders, greyed rows or "not yet" for anything not reached.
  (A later-version statement page names a feature of EldenNet, never a part of the game, so it is not covered. The
  roster's first-meeting profiles are the owner's one exception, G5 and G8; Playthrough follows this rule in full.)
- **SP6. Strict mode.** An unverified flag never unlocks anything a page shows.
- **SP7. Perception.** Live views and History › Events show only characters the Tarnished perceives, events whose
  audience includes the Tarnished, and lines that were displayed: never hidden characters, a boss before its bar, an
  invader before the game announces it, HP numbers or distances to hostiles.
- **SP8. Developer views behind a gate**: Workshop and F11 only, with `debug.panel` on, a confirmation each session
  and a visible ribbon.
- **SP9. Search and titles** carry no story or chat text; nor do URLs.

### 4.9 Privacy rules (from IA §7)

- **PV1. This PC only**, unless the player switches on the home network, and then only devices paired with a code shown
  in game (G9): Host, Origin and Fetch Metadata checks and a session cookie on this PC or a device cookie on a paired
  device (GraceUI §7.2); no remote access.
- **PV2. Write-only secrets.** Keys go in and never come out. A provider's base URL is shown on AI › Providers only.
  The character's name shows only in the top bar's Journey control and on pages about the player's own story.
- **PV3. GraceUI keeps nothing.** GraceUI's framework rule (GraceUI §4.9), binding these pages too.
- **PV4. Chat and model text stay on screen**: only on Home's Lines and Model calls cards, AI › Requests (shown text
  only), AI › Self-test and First light (the test line), History › Conversations, History › Memories and Workshop;
  never in a log, report, URL, title, toast history or search.
- **PV5. Scrubbing.** System › Logs and the Bug report scrub SteamID64, absolute paths and
  the Windows user name, the character's name, and base-URL hosts that are not a known public provider, and, with
  `dashboard.lan` (G9), the PC's network addresses and the paired devices' names.
  *2026-11-21, as built (S11.4.a):* the ids a provider's model list sends reach AI › Models' pickers alone, never a
  log, a report or another page; one that looks like a path (a backslash, a leading '/' or '~', a drive's letter,
  `/Users/` or `/home/`), as a local server's can, is left out, since it may hold the Windows user name.
- **PV6. No outside requests.** GraceUI's framework rule (GraceUI §4.9), binding these pages too.
- **PV7. Clipboard only on a click.** GraceUI's framework rule (GraceUI §4.9), binding these pages too.
- **PV8. Recordings never capture the menu.** EldenNet's session recordings skip menu content.

### 4.10 Every EldenNet feature and its home

| EldenNet feature | Home | Also shown (read-only or a shortcut) | Version |
|---|---|---|---|
| Providers: cloud with the player's key, local servers, local probes | AI › Providers | Home › Setup, First light | 0.2 / 0.6 |
| Provider health and error classes | AI › Providers › Health | Home › Status (dots) | 0.2 |
| Model slots, fallbacks, fixed budgets | AI › Models | — | 0.2 / 0.6 |
| Model presets and EldenNet's advice | AI › Models | Home › Setup, AI › Providers (advice) | 0.2 |
| The `decisions` slot: decision calls (the next speaker at a shared place and whom it addresses, and whom the player's line addresses, in v1, O97; the recall gate later) | AI › Models (the Decisions card) | — | 0.4 / 0.6 |
| A provider's API style for decision calls, Chat or System One (O96) | AI › Providers (the provider's card) | AI › Models (the Decisions card, in words) | 0.4 / 0.6 |
| Self-test (in First light, and silent at start-up) | AI › Self-test | Home › Setup, First light | 0.2 |
| Model call log, usage, cost | AI › Requests | Home › Model calls | 0.2 |
| Prepared lines (pools), preparing them | AI › Models (Prepared lines) | Behaviour › Combat | 0.4 |
| A director (`Directed` class) | AI › Director (statement) | — | later |
| Text presentation: subtitles, labels or feed, scale, HDR, a preview | Voice › Text and subtitles | — | 0.4 / 0.6 |
| Text-to-speech (`SpeechSink`, the `tts` allowance) | Voice › Text-to-speech | NPC types (`tts`) | 1.5 |
| Speech-to-text and push-to-talk (`InputSource::Stt`) | Voice › Speech-to-text | — | 1.5 |
| Named NPCs (personas, the roster's named NPCs and named bosses, other named NPCs), the roster's general profiles (G8, D67) | Characters › Named NPCs | Home › Nearby | 0.3 / 0.5 |
| NPC types, kinds, tags, named slots, allowances | Characters › NPC types | Named NPCs, Combat, Awareness; Voice › Text-to-speech (`tts`, read-only) | 0.4 / 0.6 |
| Archetype cards | Characters › NPC types (the humanoid rows) | Combat (pool state) | 0.4 |
| The mute list, unknown characters | Characters › NPC types (the locked unknown row) | Workshop (curation) | 0.4 |
| The stored event log | History › Events | Home › Recent events | 0.4 |
| Live events and decisions (Augur's tap, the decision ring) | Tools › Event monitor | Reactions (session counts) | 0.4 |
| Short-lived events (TTL) | Home › Happening now | — | 0.4 |
| Conversations | History › Conversations | Home › Lines | 0.3 |
| Journals, keepsakes, facts, memory work, backups | History › Memories | — | 0.3 |
| Forget this NPC, Forget this journey | History › Memories | — | 0.3 |
| NG+ journeys | Top bar › Journey (scopes History, Playthrough, and Named NPCs' Met and Nearby chips) | — | 0.3 |
| This playthrough: people met, their status, their stories as seen, the world now (G5, G8) | Characters › Playthrough | Named NPCs (a line per met character), Home › Nearby | 0.3 / 0.5 |
| World stages and who has heard (quest and world awareness) | History › World | Characters › Playthrough (World now, has heard of) | 0.5 |
| Quest business readiness | Behaviour › Speaking up | — | 0.3 |
| The knowledge base | Never shown (SP1); its override files on Content packs | — | — |
| Conversation (chat), call-out, content safety | Behaviour › Conversation | Quick switches | 0.2 / 0.6 |
| Speaking up, greetings, news, NPC-to-NPC, autonomy, idle remarks | Behaviour › Speaking up | Quick switches | 0.3 / 0.4 |
| Combat dialogue, boss fights, scripted-line holds | Behaviour › Combat | — | 0.4 |
| Self-awareness, status awareness, the seven statuses | Behaviour › Awareness | Home › Nearby (statuses) | 0.4 |
| Reactions and cooldowns per event type | Behaviour › Reactions | Combat, Awareness, Speaking up | 0.4 |
| Pacing and reach, presets | Behaviour › Pacing | Combat, Speaking up, Conversation | 0.4 |
| Acts and gestures | Behaviour › Acts (statement) | NPC types (locked) | later |
| Your own event rules | Behaviour › Reactions (a locked row) | — | later |
| Thoughts (`npc_thought`), diaries, `@name` addressing, the pad intent list | No page: a later-version line on Awareness, Memories and Conversation (memories of scenes are in v1, one per witness, on History › Memories) | — | later / 1.1 |
| Spoiler layers, safety rules, context gates | No setting; stated on Settings › General and Docs › Spoilers | — | — |
| The master switch | Settings › General | Quick switches | 0.6 |
| Pause AI for the session | Home's header | Quick switches | 0.2 |
| Hotkeys | Settings › Hotkeys (EldenNet's); GraceUI › Hotkeys (the menu key) | Conversation | 0.6 |
| The menu's own options, the theme | GraceUI › Menu | Top bar (theme) | 0.6 |
| The browser dashboard (always on, G4) | GraceUI › Browser view | Home › Status (the address), F11, the launcher's first screen | 0.2 / 0.6 |
| Developer options, latency toasts, recordings | Settings › Developer | Logs, Traces | 0.2 |
| Degradation ladder, read groups, capabilities, gate, unknown builds | System › Status › Health | Top bar lamp, Home › Status | 0.2 |
| Checks waiting on spikes | System › Status › Checks | Row badges everywhere | 0.2 |
| Compatibility (builds, loaders, overlays, Proton, Deck) | System › Status › Compatibility | Docs | 0.2 |
| GraceUI's own version, files and compatibility (G2) | System › Status › Compatibility | F11, Docs › About | 0.2 |
| Performance, storage | System › Performance | — | 0.2 |
| Latency traces | System › Traces | F11 | 0.2 |
| Logs | System › Logs | — | 0.2 |
| Bug report | Tools › Bug report | — | 0.2 / 0.6 |
| Developer views (prompts, raw world, curation, recordings) | Tools › Workshop | F11 | 0.2 |
| Content layers, overrides, `disable.toml`, reload results | Content packs | — | 0.2 |
| Documentation, the AI disclosure, licences | Docs | First light (disclosure) | 0.2 |
| The setup flow | First light (outside the rail) | Home › Setup, Settings | 0.6 |
| Toasts, banner, chat box, call-out box, rollback modal, F11 | EldenNet's own panels and notices, drawn through GraceUI's host; not pages (GraceUI §1.3) | — | — |
| Command-line tools (`en-tools replay`, `eval`, `selftest`, `probe-exe`) | Not in the UI; Docs › Troubleshooting names them | — | — |

### 4.11 SkyrimNet pages with no EldenNet counterpart

"Adapted" means the idea lives on in an EldenNet page; the rest are later, not planned or never. Later-version items
with a page of their own show as statements (G7); everything else is simply absent. The evidence is SND §5.

| SkyrimNet page or settings group | EldenNet | Why |
|---|---|---|
| AI › Vision (screenshots described by a model; a gallery) | Not planned | No EldenNet DESIGN entry; sending screen captures to a provider is a privacy question; heavy |
| AI › GameMaster | Later: AI › Director (statement) | EldenNet DESIGN §13, the `Directed` class |
| AI › Providers › Routing (rotation, failover, a first-token guard) | Adapted and simplified: one fallback per slot on Models; health on Providers | SN01 §14.2 |
| Request styles per provider, decision providers | Adapted: decision calls in EldenNet's own format over the same providers, on the `decisions` slot (AI › Models' Decisions card); no request styles of their own. *2026-11-21, O96:* and one API style per provider, Chat or System One, which only decision calls use (AI › Providers), our own client written from the published protocol | EldenNet DESIGN §11.2; ARCH §11.1, §11.8; O96 |
| A hosted first-party account, sign-in, a community link | Never | No accounts; PV rules |
| Voice › Bard Singing | Not planned | SN01 §14.3 |
| Voice › Samples (cloning) | Never for the cast's voices | EldenNet DESIGN §1.2, Never column |
| Voice › Effects | Not planned (could follow text-to-speech) | SN01 §14.3 |
| Voice › Text-to-Speech, Speech-to-Text | v1.5 (statement pages in v1) | O8 |
| Settings › Audio & Voice: mixing, volumes, audible distance, spatial audio | v1.5, designed with text-to-speech (EldenNet's `SpeechSink`); nothing in v1 | O8; EldenNet DESIGN §12 |
| Settings › Audio & Voice: the player's voice and its placement, narration | Not planned (the player character is unvoiced; EldenNet voices NPC lines only) | SN06 §7.3 (the Tarnished is a silent protagonist); O8 |
| Settings › Audio & Voice: voicing the game's own lines | Not applicable (the game voices its own lines) | — |
| Settings › Audio & Voice: lip-sync helpers | Later (v2: native subtitles and mouth movement, S9); none in v1 | EldenNet DESIGN §1.2, §12 (ER03 §5) |
| Characters: generated profiles, profile updates | Not planned: profile updates live on as each bio's lived layer, made per journey from its memories (EldenNet O46); uncurated NPCs use their card or stay mute | EldenNet DESIGN §7.1, §10.3 |
| Characters: virtual entities | Not planned | SN01 §14.3 |
| Home: latest screenshot, profile and diary shortcuts, pinned characters with teleport | Not planned; teleport never (a game write) | EldenNet DESIGN §0 item 3 |
| History › Diaries | Later | EldenNet DESIGN §1.2, Deferred |
| History › World Knowledge (entries, packs, a knowledge check, an assistant) | The knowledge base is never shown (SP1); its files on Content packs; no knowledge check (Workshop shows the known set) | Spoilers |
| History › Places | Adapted: the region filter on History › Events | Regions are context only |
| History › Events: creating, editing and deleting events; the schema explorer | Never for edits (D13); schemas in Docs › Writing content and Workshop | Validation; spoilers |
| History › Memories: creating, editing, generating, testing recall | Never for edits; recall later behind the `Recall` seam | D13; EldenNet DESIGN §13 |
| Behaviour › Prompts (editor, backups, preview, assistant) | Not in the UI (SP1, invariant 1); prompts are files with overrides | Prompts carry spoilers |
| Behaviour › Actions (library, eligibility, tags) | Later: Behaviour › Acts (statement) | O41 |
| Behaviour › Triggers | Adapted: Behaviour › Reactions; player rule files later | O33; `augur.allow_custom_rules` |
| Behaviour › Universal Translator | Not planned | English only (O16) |
| System › Packages (behaviour packages on actors) | Never | Zero game writes |
| Tools › File Editor | Not in v1 (D2); Workshop editors later | No content editors in v1 |
| Tools › Game Data Explorer | Never | Spoilers (SP rules) |
| Tools › AI Assistant (agents), the tool server | Not planned | SN01 §14.3; console commands would be game writes |
| Tools › cloud GPU rental | Not planned | SN01 §14.3 |
| Debug tools | Adapted: Workshop and F11 | SP8 |
| Wiki (fetched online) | Adapted: Docs, bundled offline | PV6 |
| Plugins (a hub, install, update) | Adapted: Content packs, read-only; no hub | SN01 §14.2 |
| Settings › per-character overrides | Adapted: Characters › NPC types | O38 |
| Settings › pending default changes | Adapted: the advice notice on AI › Models | `models.toml` is volatile |
| Settings › Records (item, spell and furniture names) | Never | Names come from the player's install at run time |
| Settings › Dialogue Actions (the game's own lines) | Later (v2: native lines) | EldenNet DESIGN §13 |
| Settings › NPC thoughts | Later (`npc_thought`) | EldenNet DESIGN §13 |
| Settings › NPC groups, identity links, actor filter lists | Not planned; types and tags cover them (allowances narrow by type) | EldenNet DESIGN §7.6 |
| Settings › dialogue, memory and speech text filters | Not planned (EldenNet's sanitiser and lint are fixed; the safety level) | NPC §8 |
| Settings › Memory (embeddings, scoring, re-indexing) | Not applicable in v1 (the journal is sent whole); later with recall | EldenNet DESIGN §10.1 |
| Settings › player reactions, whisper mode, continuous scenes | Not planned (the Tarnished is silent); scenes adapted as NPC-to-NPC exchanges within pacing | SN06 §7.3; O34 |
| Settings › General: usage data, automatic content updates | Never (no telemetry; no request but the provider's) | EldenNet DESIGN §1.2 |
| Settings › Dashboard: language, opening on the targeted NPC, theme presets | English only (O16); a crosshair jump is a possible later nicety; two Grace variants | — |
| Settings › Dashboard › Web server | Adapted: GraceUI › Browser view (always on as SkyrimNet's; loopback with a session cookie; other devices only once switched on and paired, G9) | G4, G9, D58–D60 |
| Settings › Developer: the tool server, agents, analytics | Not planned; log level and recordings in Settings › Developer | — |
| The setup tutorial, the feature guide | Adapted: First light; Docs › Getting started | D16 |

### 4.12 From the first proposal

The first proposal's flavour-named sections map to the new pages so:

| First proposal | Now |
|---|---|
| Grace (Home) | Home |
| The Named | Characters › Named NPCs (its Memories and Conversations tabs move to History) |
| Kinds | Characters › NPC types |
| Pacing | Behaviour › Pacing |
| Reactions | Behaviour › Reactions |
| Beckoning | Behaviour › Speaking up (self-awareness moves to Awareness) |
| War cries | Behaviour › Combat |
| Afflictions | Behaviour › Awareness |
| Parley | Behaviour › Conversation |
| Tidings | History › World |
| Remembrance | History › Memories and History › Conversations |
| Augury | Tools › Event monitor (and History › Events for the stored log) |
| Oracles | AI › Providers, Models, Self-test, Requests |
| Voice | Voice › Text-to-speech, Speech-to-text |
| Lettering | Voice › Text and subtitles; GraceUI › Menu; GraceUI › Browser view |
| Bindings | Settings › Hotkeys; GraceUI › Hotkeys (the menu key) |
| Vigil | System › Status, Performance, Traces, Logs; Content packs; Settings › Developer |
| Missive | Tools › Bug report |
| Workshop | Tools › Workshop |
| All settings | Settings › All settings |
| First light | First light |

### 4.13 The map (1.1)

GraceUI's: GraceUI §4.13, the map's design under G21 to G27 (D89 to D98), around the framework's map panel kind (D91);
EldenNet's `map` projection fills its layers, and SP1 to SP9 bind it (§4.8). The Map entry of this rail is §4.2.1's,
and Settings › Map's rows are §4.5's (D92, D95).

---

## 5. Fixtures and goldens

Moved from GraceUI §9.1 and §9.2 on 2026-11-16 (G20 (a)). They run in EldenNet's CI against a pinned GraceUI: the web
view in a headless browser on Linux, and the in-game view in GraceUI's harness on Windows (GraceUI §9.1, D85). GraceUI's
own tests run over its demo space (D82).

- **Fixtures**: scenario files among EldenNet's test fixtures (a first run with Setup open, a quiet road, a busy fight,
  the title screen, an L1 banner, multiplayer, lost contact, a second journey after NG+, a long history, every badge
  state), with invented names and our own words. Never game text or names. GraceUI's fake host, `grace-dev`, answers
  from them as EldenNet would (GraceUI §9.2).
  *2026-11-21, as built (S11.4.a):* five states so far (starting, title, world, multiplayer, banner), their topics
  written by en_overlay_tests, the AI pickers' `ai.choices.<slot>` among them (`system.compatibility` is folded into
  `system.status`); grace-dev dumps each page's in-game structure over a named state into a golden, Home and Self-test
  greyed at L1 among them (`home.banner`, `ai.selftest.banner`).

| Layer | What | Where it runs |
|---|---|---|
| Spoiler goldens | For each of EldenNet's fixture states (with our invented names), each page's and each Home card's rendered text, tiles and counts match a golden; no count includes a veiled or absent item; a hidden character, a boss before its bar or an unannounced invader changes no count; History › Events lists no event the Tarnished did not perceive; an earlier journey shows read-only; titles and URLs carry no chat text; the roster at the title-screen fixture, named bosses included, shows only general profiles (`public.summary`, `public.role` and, for an entry that speaks, `subjects`), in name order, and an overlap fixture (an invented character the game can show calm, with a later boss form) shows no "Boss" tag and no bar name, met or not; Playthrough shows nothing of anyone not met, "Alive when last met" for someone who died unseen, the line of the stage seen and never a later one, and no related story for an unmet person | EldenNet's CI: Linux (web), Windows (in game) |

---

## 6. Decision log

D61 moved here from GraceUI's decision log on 2026-11-16 (G20 (a)): it was always EldenNet's rule for its own provider
settings (GraceUI's G17 kept it EldenNet's). The other D-entries these pages cite stay in GraceUI's log (GraceUI §13).

| # | Decision | Choice | Alternatives | Why | Source |
|---|---|---|---|---|---|
| D61 | Keys bound to their address | Changing a provider's base URL or kind, from either view, clears its key and its extra header values, which must then be set again (EldenNet's rule for its own provider settings, applied by EldenNet's command entry point; recorded here since G20 (a)) | A confirmation in game for every address change | No visitor to the dashboard can send a player's key to another address | GraceUI §7.4 |
