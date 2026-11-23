# en-overlay: EldenNet's display layer, through GraceUI

`en-overlay` is EldenNet's connection to GraceUI (ARCH §2.2, GraceUI G15), C++20, MIT: the banner and the pause badge
as GraceUI notices, EldenNet's claims as a world-state provider, its hotkeys (F11 and the development Mark), the F11
debug view, a panel drawn on Dear ImGui 1.93.0's core with no backend, and EldenNet's space: its pages in GraceUI's menu
and browser view (PAGES; GraceUI's G20, D169). It began as the first in-game build's part 12 on GraceUI 0.1
(`docs/eldennet-changes.md` §14 to §18 and §20, D131 to D156, D164 and D165); the pages' first slice (roadmap S11.4.a)
moved it to GraceUI 0.2.5's ABI 1.2 (§19, §23, §24, §26 to §29, §33, §34 and §36). The first conversation (S11.3, part
C; WHY§364) adds EldenNet's own surfaces on GraceUI 0.2.20's ABI 1.5 (§31, §33, §47 to §50): the lower third, the chat
box, the hint and the toasts. Later increments add the call-out box (v0.6) and the rest of its pages.

It reaches GraceUI through `graceui-sys` alone (`third_party/graceui-sys`, vendored from GraceUI at 92fc19c, GraceUI
0.2.25, ABI 1.5, protocol 1.8; its floor, 0.2.22, for the profile editor's families, slow commands, editor block and
chosen row, on 0.2.17's wait handle): its look for GraceUI and `GraceUI_RequestApi(1)` (`graceui_sys.h`), ABI 1's calls, each behind `has_cap` and the table's size
(`graceui_sys.hpp`), and the headless Dear ImGui helper (`graceui_imgui.hpp`). It links no hook library and no Dear
ImGui backend and hooks nothing (R12, GraceUI's invariant 16); it never links en-core or en-gal, so it can never reach
the game's memory (ARCH §2.3, checked by `en_link_rule`); and nothing in it waits but the look, on T1 (R6).

## Two parts

- **Portable, in every build** (tested on Linux with GCC and Clang too): `view.hpp`, the F11 view's lines,
  `world.hpp`, the world-state claims, and `surfaces.hpp`, the conversation's surfaces laid out. All are pure, on
  en-proto and `graceui.h`, which is plain C.
- **The Windows builds** (MinGW-w64 and clang-cl): `overlay.hpp`, the connection, `chat.hpp`, the surfaces' calls
  into GraceUI, and Dear ImGui's core
  (`en_imgui_core`), downloaded at configure time at GraceUI's own pin (the commit of tag v1.93.0, each file by its
  SHA-256) and compiled with GraceUI's `gu_imgui_core` definitions: no demo, debug tools, file, shell or Win32 functions.
  Its warnings are off as third-party code's, but for `en_dll_module`'s two rules, which Clang keeps as errors (the DLL
  links it).

## The connection (`overlay.hpp`)

On T1, after WaitWindow (ARCH §3.2's Connect, and ConnectB on the banner path):

1. **`find`**: graceui-sys's look (`graceui_sys_find`), by module name, an export scan, then `GraceUI\graceui.dll` in
   the game's folder, which it loads, again every 250 ms for at most 30 s. With `GRACEUI_SYS_STOP_IF_ABSENT` (GraceUI
   D164), when nothing answers, no copy is installed in the game's folder and no module is named `graceui.dll`, nothing
   can answer later, and the look ends at once: a start without GraceUI is not held up 30 s. The rule is graceui-sys's
   own, and `installed` in the log is its word for it, on its by-name look too since GraceUI c781a85 (D170; en-overlay's
   own reading of a find by name, WHY§231, is gone). Both load orders work:
   GraceUI loaded before EldenNet is found by its name, and GraceUI installed in the game's folder but not yet loaded is
   loaded by EldenNet (and found by its name if a loader loads it later).
2. **`Overlay::connect`** as `eldennet` (its id and its space's), display name `EldenNet`, its version, its GraceUI floor
   (0.2.22, `en/proto/graceui_pin.hpp`) and the caps it uses: `notices world-state hotkeys`, and with the view
   `panels textures`. The answers: `connected`; `notices_only` (inactive for the anti-cheat, with a handle: the banner
   through `notice` alone, D88); or a failed connect, `absent`, `too_old`, `duplicate_id`, `inactive` (the overlay
   could not start or turned itself off), `refused` or `not_yet`, which leave EldenNet headless (ARCH §10.3's "Overlay
   failed to install"): the DLL logs one line.
3. Connected, with `debug.panel` on: the view's Dear ImGui context (headless, `graceui::imgui::prepare`, Dear ImGui's
   own bitmap font), its atlas registered as a texture before any submit (EldenNet's registration window), a hidden
   panel on the panel layer at the display's top left (24, 24; 680 by 500), and F11 registered as the hotkey
   `debug-view` (D155). A step that fails leaves the view off (the report names it: `no_caps`, `atlas`, `panel`,
   `hotkey`) and the connection as it is. F11 taken first, by another mod's hotkey or the menu key, registers unbound
   (`GRACEUI_UNBOUND` with its id, D164): a success, the view made all the same (`unbound` in the report, and the DLL
   logs it), which nothing opens until the player binds the hotkey on GraceUI's Hotkeys page (GraceUI 0.6); there is
   no other way to it.
4. The Mark's hotkey (`mark`, Pause) when asked for, `register_mark` on T6 otherwise. Unbound (another mod's hotkey
   took Pause first), it is registered with its id and `mark_bound()` stays false: the DLL then never asks the launcher
   for Pause, which stays the Mark (src/eldennet/README.md).

From then on EldenNet's core thread (T6) alone calls the rest, once per wake (at most every 100 ms):

- **`banner(text)`**: one notice under EldenNet's key 1 with a duration of 0, its text replaced in place by a new text
  and withdrawn by an empty one; the same text again calls nothing. It works in the notice-only mode. **`badge(text)`**:
  the pause badge, likewise under key 2.
- **`world(flags)`**: the claims (`world.hpp`), sent when they change.
- **`step(status, space, facts, now)`**: `poll` (with a buffer for a space's frames) until GraceUI has nothing waiting,
  passing over every kind it does not use; each hotkey event names its presses since the last; an odd count of F11's
  toggles the view, and the Mark's are counted for the DLL; the space's events go to the space (below). To open, one list is built and submitted
  and then the panel shown, so it never shows empty; to close, the panel is hidden. While it is open, every step builds
  a frame with `io.DisplaySize` the panel's size, `ImGui::Render()`, appends its lists into one GraceUI list
  (`graceui::imgui::DrawList`) and submits it; a frame past ABI 1's bounds is not built, and the view keeps its last
  list. `not_connected` or `inactive` from `poll` ends it: nothing more is called.

The view takes no input (D142): F11 shows it and hides it.

## The conversation's surfaces (`surfaces.hpp`, `chat.hpp`; NPC §5, §8.6, §10)

- **Laid out, in every build** (`surfaces.hpp`, pure): en-proto's `UiModel` as draw lists in GraceUI's own form (D76:
  Dear ImGui's vertex layout, 16-bit indices, a command per texture and clip, within ABI 1's bounds). The text is
  GraceUI's faces only: each glyph's quad from a glyph table, emitted on the shared atlas's texture id and scaled from
  the size GraceUI baked it at to the surface's size at the content scale (NPC §10.1's sizes at 1080 lines); solid
  shapes on a 4 by 4 white texture of EldenNet's own. The lower third (a name line in gold, upper case, +0.08 em, and
  two wrapped lines, with a shadow; the card's "…"), the box (its header "Speak to … — heard by: …", the history
  strip's newest lines, a waiting line greyed, the input with its selection and caret, scrolled so the caret shows, or
  the strip alone), and the hint ("[T] Speak — …", its key boxed in the words describe_hotkey gives, under the
  marker's diamond, at (0.50 W, 0.68 H): no render feed yet). The fades are stepped at T6's pace (120 ms in, a 120 ms
  cross-fade, 350 ms out), and a Speech page's first submitted frame, never transparent, is the one PageShown names
  (WHY§364 (3)). Toasts are GraceUI notices under EldenNet's keys 3 and 4, each id posted once and let go when the
  model no longer lists it. The box's input mapping: Enter and the pad's A send, Esc and the pad's B close, Tab skips,
  the rest goes to the editor; the holder's end by its reason; the opening key's own character within 100 ms
  swallowed (NPC §5.1).
- **Through GraceUI, on T6** (`chat.hpp`, Windows): at connect, in the registration window, the white texture and two
  GRACEUI_FACE_SCALED faces, GraceUI's own EB Garamond (30 px) and Noto Sans (18 px) as the DLL reads them from
  GraceUI's folder (too late after warm-up: then GraceUI's notices' face); the hotkeys `talk` (T, with the pad chord
  D-pad Up then View behind `has_cap("pad-chords")`) and `skip-page` (Tab), named "Talk" and "Skip the page";
  `wake_event`'s handle behind `has_cap("wake")`. Then, from poll: the talk key opens (OpenChat), Tab skips, the
  display's event sizes the three panels (made for it, at most one set per display), and a hotkey changed is described
  again. `Overlay::show(model, now)`, after the core's step: each panel submitted at most every 100 ms (the box's
  first frame at once), shown once it holds a list; the box's own panel shown, then `request_input` with
  GRACEUI_INPUT_EVENTS for the keyboard, mouse and pad; granted, the holder's events go to the box's own headless Dear
  ImGui (the line's text, caret and selection; NPC §8.5's 240 characters), the caret set for the IME; refused and not
  now answered as Close with the holder's id or the reason; the model closing the box answered as Close{Shut} with the
  draft; a fault releases input first.

## The F11 view (`view.hpp`)

Its first line is a ribbon, "EldenNet developer view (F11 closes it)" (PAGES SP8), then EldenNet's version and build
label; T1's state and discovery's items and passes; en-core's phase, level and banner, the gate (open or closed, and
why), Dormant and the badge; the task's snapshots, its p99 and read mode; each read group's state from the newest
snapshot (`ok`, `untested`, `tripped`, with its hard faults), the groups C2 reported tripped, the copies and the faults
by kind; the Marks and who stamps them; GraceUI's version; then two lists, each the newest first with its ages:

- **Augur's events** (DESIGN §14's "live events with audiences"; roadmap S10.2.d), the last ten, under a heading with
  how many there have been: each line a square of its source's colour (blue the lifecycle's, green the world's, amber
  EldenNet's own pipeline, none in v0.1), its age, its kind, its class (`P`, `K`, `T`), its subject (`player`, `mute`,
  `persona:<index>`, `card:<index>`, `none`), its reason (Augur's `reason`, en-augur's `words.hpp`: `place 6100100`,
  `hard`, `Victory`, `active`, ...), a K event's merges (`x3 merged`), and its audience: the radius, how many rows, the
  first row (`p12.direct`) and how many more (`+2`). An event never shows a handle, a key, a name, an HP fraction or a
  distance;
- **en-core's changes**, the last five: the ladder's and the lifecycle's moves (DESIGN §14's "ladder"), kept beside
  Augur's lifecycle events because they hold what those leave out (the gate's reason, the banner, the badge).

T6 keeps both lists in rings of fixed size (`NewestRing`), which the DLL fills after each step of the core and copies
into the view's status, a fixed snapshot: nothing is allocated for them, on T6 or on GraceUI's present thread. The view
holds no game text: every word is the name of one of our own enums (Augur's among them) or a word of the view's own,
each checked again as it is written (letters, digits and `_.+-`; anything else `?`), and the rest are numbers. It is 32
lines of Dear ImGui's own 13 px font, 2 px apart, in a panel of 680 by 500.

## The world-state claims (`world.hpp`)

From what v0.1 knows: the title and loading from en-core's phase; multiplayer when the badge is up or the gate closed
for a reason of the session; the anti-cheat from Safety's check and T10's. Two are stand-ins, documented as such:
danger, until en-core's `safe_context` (NPC §5.4) with en-augur's combat state, is the player dead, a boss bar, or the
player locked on, in a world; a game menu, until NPC §5.8's menu read, is the HUD's pop-up menu state, in a world. The
game's own text entry is not read in v0.1.

## EldenNet's space (`space.hpp`; PAGES, GraceUI D160, D169)

- **One source**: `pages/space.toml`, in GraceUI's page format (eldennet-changes §19): the rail of PAGES §4.2.1 in its
  order (a category with no page yet holds none, and GraceUI shows no empty category), the pages (Home, System ›
  Status and Logs, Settings › Developer from the first slice; AI › Providers, Models and Self-test from the second,
  writable from the third on GraceUI 0.2.8's protocol 1.2: D192 to D196; the fourth on GraceUI 0.2.9's D197 and D198;
  the fifth, AI › Models as one card per job, on GraceUI 0.2.12's protocol 1.3: D202), its topics (`settings`,
  `status`, `errors`, `system.status`, `logs`; `ai.status`, `providers`, `health`, `selftest`, `ai.jobs` for each
  card's live status and `ai.budgets` for its fixed budgets, a member per job, `ai.choices.<slot>` for each job's model
  picker, `ai.choices.<slot>.fallback` for its fallback's and `ai.choices.providers` for every provider picker), its
  commands (`settings.set`, `settings.reset`, `selftest.run`, `models.list`, `providers.add`, `providers.remove`,
  `providers.key`) and the settings whose home is there (`debug.panel`, `debug.record_sessions`, `debug.log_level`;
  `ai.preset` and each job's provider, model and fallback, `slots.<slot>.provider`, `.model`, `.fallback_provider` and
  `.fallback`, on the job's card). The page data says `"minor": 3`. `python3 -I tools/pages/build.py` writes `pages/pages.json`, `pages/settings.json` and `src/space_data.cpp` from it (`--check` in
  CTest); every page ships in the increment EldenNet registers or an earlier one, `0.2` since the AI pages (D173).
  Every word a player reads there is a draft for review.
- **Registered at the connect** (T1, eldennet-changes §23 item 1): `Options::space_views`, both views on the main path,
  the game's alone on the banner path (PAGES §4.4); `Report::pages` is register_pages's answer, and the log's connect
  line says it. Without the `pages` cap, or in the notice-only mode, nothing is registered.
- **Served on T6** (`Overlay::step(view, space, facts, now)`): poll with a buffer of GRACEUI_FRAME_FROM_PAGE_MAX, a
  WATCHED or UNWATCHED frame to the space, each COMMAND answered at once and exactly once with what `Space::command`
  answers (`internal` if no space can), every other kind passed over (the display's among them), then what
  `Space::collect` has for the watched topics published.
- **Projections only** (PAGES §3): each body is built from `SpaceFacts`, EldenNet's own enums (the DLL maps en-core's
  and en-gal's, a switch each), numbers and words, never game text, flags, stages, prompts, model text, paths or names.
  Absence is silent: a fact EldenNet lacks is left out. `status` is the top bar's lamp (`word`, `tone`, `reason`,
  `page`; no lamp while EldenNet is starting; at L3 "Speaking, in part" in ember and at L4 "Speaking" in moss, each with
  its reason, words for the owner's review, WHY§422, §442; Dormant over L2 to L4) and Home's Status card (`items`); `errors` Home's Problems (`checks`: the
  failed start in the banner's own words with its fix, a read group stopped, a refused settings file);
  `system.status` System › Status's Health (`items`), EldenNet's parts as lamps (`checks`: the start, reading the game,
  the core, a refused providers file with why in words and its line, the anti-cheat watch, the log, the recording
  when asked for, GraceUI), the read groups' self-tests
  (`columns`, `rows`) and the Compatibility tab's versions and how EldenNet started (`compatibility`, the member its
  block reads, GraceUI D185's `props.field`); `logs` the log's own lines (500
  held, from T1's first, scrubbed again of a SteamID64's shape, PV5). The AI pages' (`SpaceFacts::ai`, en-overlay's
  own `AiFacts`, which the DLL maps from en-llm): `ai.status` the cause every AI page leads with (PAGES §4.4:
  inactive, a refused providers file, no provider, no key or a refused one, no healthy model for Conversation, the
  gate, or v0.1's observing; at L3 and L4 "Conversation is ready: NPCs answer when you speak to them."; at L1 the
  banner's own words, O95); `providers` the providers' table (the base URL here
  alone, PV2; the key column is the row's secret, D194 and D197, its cells "set", "refused" (its last call refused
  for its key, or for want of one), "none" (no key's place and none asked for, or http:// on another computer) or "",
  never a key; a Remove on each row, D192) and the model list's checks, in words; `health` each provider's state (a
  key refused, or asked for, among them), when it last answered and its jobs; `ai.jobs` each card's live status
  (D202), a member per job, `{word, tone}` in the lamp's tones: No model (glint), Failing (rot: its provider's key
  refused or asked for), No key yet (glint), Cooling down (ember: its provider rate-limited), Resting after failures
  (ember: every model of the job cooling) or Ready (moss), and no frame at all before the DLL has its facts;
  `ai.budgets` each card's key-value list, a member per job: "Same as Conversation" for one that borrows, then its fixed
  budgets from en-llm's slot table (the reserved slots get no card until their features exist, PAGES §4.5);
  `selftest` the verdict, the steps, the figures and this session's earlier runs, nothing before the first run;
  `ai.choices.<slot>` a job's model picker's choices (D193), what the job's provider (one chosen for it and waiting,
  else its own) listed last, in its order, at most 256, any id that looks like a path left out (`pickable`: a
  backslash, a leading '/' or '~', a drive's letter, a home folder; PV5); `ai.choices.<slot>.fallback` its fallback
  picker's, what the fallback's provider (one chosen and waiting, else the fallback's own, else the job's) listed;
  `ai.choices.providers` every provider picker's, the providers but one whose key was refused. Never a key or the model's text. `space.state`, GraceUI's framework topic (GraceUI 0.2.7, D188): Home and the AI pages greyed at L1 with
  the banner's own words (PAGES §4.4, O95's one form), nothing greyed otherwise (at L2 every page stays usable, led by
  `ai.status`). Blocks whose topic can publish nothing for them carry `props.empty` words (D186): Home's Problems "No
  problems.", System's read groups before the first reading, the AI pages'; Logs' tail is newest first in the browser
  (`props.newest_first`, D187).
- **One command path** (GraceUI's invariant 3): `settings.set` (the key known, not locked, the value of its type and in
  its bounds or choices) and `settings.reset` (a key at once; a page or every setting only with `confirm: true`,
  D173), each refusal by its code, with words in `detail` where the code says too little; nothing while the settings
  file the player wrote was refused at the start. A secret is held and published as its status alone; one with a space
  or a control inside is refused. What a command changed is taken by the DLL (`take_changes`), which applies it and
  writes settings.toml, or for `ai.preset` and a job's pick providers.toml (refused while providers.toml is). A job's
  provider must be one of providers.toml's whose key was not refused: chosen while it listed the job's model, it is
  one change, that model on it; otherwise it waits in the space, shown on its row, until a model of its list is chosen
  (nothing written meanwhile; dropped by a reset, by the job's own provider chosen again, or when the provider goes).
  A job's model must be one its picker offers (a model its provider listed, `pickable`), and is one change, `Change`'s
  `provider` saying which. A job's fallback likewise, on its card's last two rows, only for a job with a model of its
  own (not one that borrows another's): its provider waits until its list holds the fallback, and the fallback is one
  of that provider's models (else the job's provider's), never the job's own model there; one change of
  `slots.<slot>.fallback`. A reset of a job's provider or model row clears its pick (one change, an empty model), its
  whole table, so the preset's provider, model and fallback hold again; a reset of either fallback row clears the
  fallback alone (one change, an empty fallback); with no pick, a reset only drops a provider waiting. A picker's
  Default (GraceUI D199, the value "") is the row's reset. The rows follow the facts' `revision` (`sync`), and the
  settings body's `changed` (GraceUI D198) names a job's provider and model rows only while its table in providers.toml
  gives a model (`AiSlot::picked`), its fallback rows while it gives a fallback (`AiSlot::fallback_picked`), any other
  row while its value is off its default. The AI pages' commands are
  answered at once and handed to the DLL (`take_requests`, each an `AiRequest`): `selftest.run` and `models.list`
  (only with a provider, while the gate is open, one at a time: `busy` for the same, `not_now` in words for the
  other); `providers.add` with no fields (the header's: the chosen preset's providers that providers.toml lacks; not
  for Custom) or with the form's `name` and `base_url` (a provider of one's own: the name 1 to 32 of letters, digits,
  `-_.`, no provider's already, nor a key's already kept; the address checked by en-proto's `check_base_url`, each
  refusal in words, never logged; `http://` to this PC, or to another PC of the home network by a literal private
  address, O98 (a), as a local provider with no key); `providers.remove` with a row's `id` (destructive, so `confirm_required` without
  `confirm: true`, D173, D192; never a provider a job uses, the jobs named); and `providers.key` with a row's `id` and
  `value` (D194: empty clears it; refused, in words that never echo it, for a provider whose key is an environment
  variable's, whose address is http:// to another computer, a home server's included, or whose row reads Not needed;
  the frame's copy of the value overwritten, and the poll buffer's after each command). An argument of the wrong type is refused as `invalid_value`.
- **What it publishes and when**: nothing for a topic nobody watches; a state topic when it is watched (again for each
  new watch) and when its body changes, no oftener than its rate (`status`, `providers` and `selftest` 4 a second,
  `system.status` and `health` once a second, the rest on change); the logs stream from its oldest row held, in batches of 100, then each new row once a second.

## The Characters pages and the profile editor (`characters.hpp`; PAGES §4.6.4, NPC §4.7; S7.2.b)

On GraceUI 0.2.22's topic families, master list, slow commands, editor block and chosen row (D215 to D218), registered
in `pages/space.toml` like every page (its 0.2 increment, where PAGES ships Named NPCs in 0.3: the owner wants profiles
edited now) and served by the space, which owns a `Characters` for them:

- **Characters › Named NPCs** (`characters.named`): the roster as the page's master list, `named.list` (a table, a row
  per profile EldenNet voices: its key, `persona.<id>` as WHY§387 has it, its first-meeting name and its role); its
  Profile tab holds the chosen row's general profile (the family `named`, a key-value list of `public.summary`,
  `public.role`, the `subjects` and our labels for its way of speaking and its news temperament: SP1's fields alone) and
  "Your notes", an editor block over the family `profiles`. The Allowances and Reactions tabs, and Met and Nearby, wait
  for the allowances and the sightings store: absent until then (SP5).
- **The editor's document** (`profiles/<key>`, D217): the four fields a layer may set (NPC §4.5): `instructions`
  (prose, 35 words, 210 characters), `subjects` (a list of 8 at most, 10 words and 50 characters each), `register` (a
  choice of NPC §3.3's five, by our labels) and `reply_words_max` (10 to 60), each nullable, null where the layer leaves
  it to the shipped profile, with the shipped value beside it as its `reference`; its revision (the layer file's `rev`);
  a state line (Not written yet, Saved, Saved with warnings, Updated since you wrote this, Not loaded, Newer file); the
  draft's help naming the provider, or `draft_blocked` saying why it cannot run now. A newer file's fields are locked.
- **Commands**: `profile.check`, `profile.save` {target, rev, doc}, `profile.reset` {target, all, confirm},
  `profile.restore` {target}, and `profile.draft` {target, notes}, the space's one slow command (D216). Each is checked
  at the space's entry point (a row's key, a whole revision, the doc's four fields of their kinds, a list item's text
  taken from `{text, key}`, the notes within 400 characters and not blank, a reset confirmed and of a layer that exists,
  a draft not blocked), then handed to the DLL as a `ProfileRequest` with GraceUI's reply handle (`Space::command`'s
  third argument). The DLL answers through `Space::answer_profile` (messages {field, item, level, words}; a save's
  `chosen` row; a draft's `{draft, notice}`), and the Overlay hands each answer to GraceUI once (`answer_later`, and
  every step before it publishes, so a save's answer comes before its document's new revision).
- **Published** like every state topic: the list and each watched member on change, nothing unwatched, and a member
  whose row is gone not at all. A watch of a member is logged as its family and its key (the log takes words alone).
- **Sensitive** (protocol 1.8, D221; S11.4.e): `providers.add`, `providers.remove` and `providers.key` are marked, and
  the settings `debug.panel` and `debug.record_sessions`; GraceUI refuses them from a paired device before EldenNet
  sees them. An older GraceUI reads past the marks (it has no devices to refuse).

## Tests

`en_overlay_tests` (doctest): in every build, the Characters pages (`test_characters.cpp`: the page data with its
master, families, editor and marks, the bodies from invented characters, every refusal at the entry point, the
requests handed over with their reply handles, the answers later, what is published when, and the spoiler golden: no
string a projection does not publish reaches a body), the surfaces laid out (the panels' places, text decoded, upper-cased
and wrapped, a page's, a card's, the box's and the hint's lists checked as GraceUI checks them, the caret and its
scroll, the fades and PageShown, the toasts' keys, the input mapping and the swallowed character), the view's lines (every section, the newest event and the newest change
first with their ages, each event's columns and its square's source, full lists within the view's lines and its panel,
nothing but our words and numbers whatever the status holds, a long word cut), the newest ring (the last N, the newest
last in its copy, every push counted), the world-state claims, and the space:
its generated page data (the ids, the rail, the increment, GraceUI 0.2.5's checks met), settings.set and settings.reset
with each refusal, a secret's status alone, the AI pages' topics, commands, preset and keys (a key in no frame, answer
or note), what is published and when, the logs stream's batches and ring, the words
(no lamp while starting, the banner's own, absence silent, nothing personal), and in the Linux builds the five fixture
states' `tests/pages/<state>/topics.json` (EN_UPDATE_FIXTURES=1 writes them anew). In the
Windows builds, under Wine, the connection against a fake GraceUI of our own (`tests/fake_grace.hpp`; graceui-sys has no
C mock): ABI 1.5's table, answering as GraceUI 0.2.25 answers and checking as it checks (connect's answers, notices
under keys, the registration window, hidden panels, hotkeys, poll's events by the event's size, the space's slots, and
submit's draw-list checks: counts, bounds, ranges, indices, clip rectangles inside the panel, known textures, the
stride). The cases: the space registered once for its views, or not without the cap or beside anti-cheat; watched
topics published, each command answered once, nothing while unwatched; a frame too large kept; poll passing over the
display's event and any later kind; connect's request; each
failed connect headless; the banner replaced in place and withdrawn, the badge, a long text cut; the notice-only mode;
the world-state claims sent on change, and none without the cap; the view (its atlas before any submit, a hidden panel,
F11, a list before it shows, lists every step while open, hidden again, an even count of presses leaving it as it
was); a view that cannot be made; the surfaces (`test_chat.cpp`: what connect registers, faces too late after warm-up,
older GraceUIs without the chord or the holder's events, the box opened with its input, typing with the swallowed
character, the caret, Enter, the line's limit and a kept draft, Esc, danger, focus lost and the model's Shut keeping
the draft, refused and not now, a page's PageShown and its fade at ten frames a second, Tab, toasts, the hint's key
words and their change, a fault, and the wait handle); the Mark's hotkey and its presses; F11 or Pause taken by another mod, registered
unbound with its id; GraceUI gone.

CTest also runs `en_pages_build` (`tools/pages/build.py --check`), `en_pages_lint` (GraceUI's own page lint, from a
GraceUI checkout whose files match the pin, `EN_GRACEUI_SOURCE`) and, in a Linux build with `EN_GRACE_DEV`,
`en_pages_preview`: grace-dev serves each fixture state, a browser of our own (`tests/pages/browser.py`) reads every
topic back and sends commands, and each page's in-game structure (`--dump`) matches `tests/pages/golden/`
(EN_UPDATE_GOLDENS=1 writes them anew).

End to end, the fake game's harness runs eldennet.dll with a test build of GraceUI under Wine with Direct3D 12
(`tests/fakegame/README.md`), `launcher-eldennet-graceui-pages` among its cases: F5's menu on EldenNet's pages, a
setting changed from it, and a browser on GraceUI's server reading the topics and sending commands; and
`launcher-eldennet-graceui-events`: the fake game's world and the lifecycle raising Augur's events, and the view's
squares of both colours counted on the game's frame.
