# en-overlay: EldenNet's display layer, through GraceUI

`en-overlay` is EldenNet's connection to GraceUI (ARCH §2.2, GraceUI G15), C++20, MIT: the banner and the pause badge
as GraceUI notices, EldenNet's claims as a world-state provider, its hotkeys (F11 and the development Mark), the F11
debug view, a panel drawn on Dear ImGui 1.93.0's core with no backend, and EldenNet's space: its pages in GraceUI's menu
and browser view (PAGES; GraceUI's G20, D169). It began as the first in-game build's part 12 on GraceUI 0.1
(`docs/eldennet-changes.md` §14 to §18 and §20, D131 to D156, D164 and D165); the pages' first slice (roadmap S11.4.a)
moved it to GraceUI 0.2.5's ABI 1.2 (§19, §23, §24, §26 to §29, §33, §34 and §36). The first conversation (S11.3, part
C; WHY§364) adds EldenNet's own surfaces on GraceUI 0.2.20's ABI 1.5 (§31, §33, §47 to §50): the lower third, the chat
box, the hint and the toasts. Later increments add the call-out box (v0.6) and the rest of its pages.

It reaches GraceUI through `graceui-sys` alone (`third_party/graceui-sys`, vendored from GraceUI at 7c26477, GraceUI
0.2.68, ABI 1.9, protocol 1.21, whose uploads and on_present it does not use; its floor, 0.2.57, for Workshop's pc_only
marks (D264), which an older GraceUI does not read, with 0.2.56's Copy (D263), 0.2.55's Logs' Level filter with its own
choices (D262), 0.2.51's browser-only Workshop (D258), 0.2.50's filter bar on Logs' stream, 0.2.49's header switch, Docs' document block and Bug report's prose
field and preview, the profile editor's families, slow commands, editor block and chosen row, on 0.2.17's wait handle;
0.2.36's italic face where GraceUI's folder has it, 0.2.38's word for what pressed a hotkey and 0.2.61's Plain text
behind their caps): its
look for GraceUI and `GraceUI_RequestApi(1)` (`graceui_sys.h`), ABI 1's calls, each behind `has_cap` and the table's
size (`graceui_sys.hpp`), and the headless Dear ImGui helper (`graceui_imgui.hpp`). It links no hook library and no Dear
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
   (0.2.57, `en/proto/graceui_pin.hpp`) and the caps it uses: `notices world-state hotkeys`, and with the view
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
  shapes on a 4 by 4 white texture of EldenNet's own, the box's ground and frame and the hint's key box with rounded
  corners (GraceUI's own radii from its token file, a card's 4 px and a control's 2 px, scaled: a fan of points a
  corner and a half-pixel fringe each side fading to nothing, as Dear ImGui's anti-aliased shapes; the sides cover the
  pixels a square rectangle does). The lower third (a name line in gold, upper case, +0.08 em, and two wrapped lines,
  with a shadow; the card's "…"; no ground, as NPC §10.1 has it), the box (its header "Speak to … — heard by: …", the
  history strip's newest lines, the Tarnished's in the italic where GraceUI has it and upright otherwise, a
  waiting line greyed, the input with its selection and caret, scrolled so the caret shows, or the strip alone), and
  the hint ("[T] Speak — …", its key boxed in the words describe_hotkey gives, under the
  marker's diamond, at (0.50 W, 0.68 H): no render feed yet). The fades are stepped at T6's pace (120 ms in, a 120 ms
  cross-fade, 350 ms out), and a Speech page's first submitted frame, never transparent, is the one PageShown names
  (WHY§364 (3)). Toasts are GraceUI notices under EldenNet's keys 3 and 4, each id posted once and let go when the
  model no longer lists it. The box's input mapping: Enter and the pad's A send, Esc and the pad's B close, Tab skips,
  the rest goes to the editor; the holder's end by its reason; the opening key's own character within 100 ms
  swallowed (NPC §5.1).
- **Through GraceUI, on T6** (`chat.hpp`, Windows): at connect, in the registration window, the white texture and
  GRACEUI_FACE_SCALED faces of the type GraceUI's Plain text asks for at connect (O134; ABI 1.9's `plain_text` behind
  `has_cap("plain-text")`, GraceUI 0.2.61's D268, so an older GraceUI gets the serif), the DLL reading from GraceUI's
  folder only the files that type names (`surfaces.hpp`'s `faces_of`; too late after warm-up: then GraceUI's notices'
  face): the serif's, GraceUI's own EB Garamond (30 px) and Noto Sans (18 px), and its EB Garamond Italic (30 px)
  where the folder has it (GraceUI 0.2.36, D241; three of the eight faces EldenNet may register, D139, graceui.h's
  GRACEUI_FACES_ELDENNET since 0.2.39; none below it, and the Tarnished's lines are drawn upright); or Plain text's,
  Noto Sans Regular for the text (30 px) and for the header and the hint (18 px), and Noto Sans SemiBold (20 px) for
  the page's name line, as GraceUI's menu takes SemiBold for its titles (D267), the Tarnished's lines told by their
  colour alone (no sans italic ships); a change of Plain text in the session is noted, and waits for the next launch
  (no face registered after warm-up, GraceUI's invariant 11); the F11 view keeps its own face. The hotkeys `talk`
  (T, with the pad chord D-pad Up then View behind `has_cap("pad-chords")`) and `skip-page` (Tab), named "Talk" and
  "Skip the page";
  `wake_event`'s handle behind `has_cap("wake")`. Then, from poll: the talk hotkey opens (OpenChat, by the pad chord
  where GraceUI's `hotkey_by` says the chord alone pressed it, behind `has_cap("hotkey-by")`, 0.2.38's D243; by the key
  otherwise, and the key's own character swallowed only then), Tab skips, the display's event sizes the three panels
  (made for it, at most one set per display), and a hotkey changed is described again. `Overlay::show(model, now)`,
  after the core's step: each panel submitted at most every 100 ms (the box's first frame at once), shown once it holds
  a list; the box's own panel shown, then `request_input` with GRACEUI_INPUT_EVENTS for the keyboard and the pad (never
  the mouse, O113); granted, the holder's events go to the box's own headless Dear ImGui (the line's text, caret and
  selection; NPC §8.5's 240 characters), the caret set for the IME; refused and not now answered as Close with the
  holder's id or the reason; the model closing the box answered as Close{Shut} with the draft; a fault releases input
  first.

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
  held, from T1's first, scrubbed again of a SteamID64's shape, PV5), each row `{seq, time, line, level}`, its level the
  logger's own (`error`, `warning`, `info`, `debug`; `trace` for a later logger's), never read from its text. The AI
  pages' (`SpaceFacts::ai`, en-overlay's own `AiFacts`, which the DLL maps from en-llm): `ai.status` the cause every AI
  page leads with (PAGES §4.4: inactive, a refused providers file, no provider, no key or a refused one, no healthy
  model for Conversation, the gate, or v0.1's observing; at L3 and L4 "Conversation is ready: NPCs answer when you speak
  to them."; at L1 the banner's own words, O95); `providers` the providers' table (the base URL here
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
  problems.", System's read groups before the first reading, the AI pages'; Logs' tail is newest first
  (`props.newest_first`, D187), in game too since GraceUI 0.2.50 (D257: the newest 200 rows, from the same frames as
  the browser's, so scrubbed alike), with its filter bar (D257): Level, a filter on each row's `level`, and Find in the
  log, a search in its lines, in both views over the rows each holds, in place of PAGES' level tiles; no sort. Since
  GraceUI 0.2.55 (D262) the Level filter names its own choices in the owner's words (O133): Errors, Warnings,
  Information and Details (`error`, `warning`, `info`, `debug`; no trace, which v0.2 never logs).
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
  `calls` and `traces` twice a second, `system.status`, `system.performance`, `health` and `usage` once a second, the
  rest on change); the logs stream
  from its oldest row held, in batches of 100, then each new row once a second.

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

## Content packs (`content`; PAGES §4.6.9; S11.4.a)

At the rail's foot after Settings, a category of its own marked `foot` (PAGES §4.2.1; protocol 1.10, GraceUI D248),
pinned under the rule there in both views; increment 0.2, both views; read-only: no command, no header action (no
content editors, D2). One topic, `content`, a member per section (GraceUI D185's `props.field`), built from
`SpaceFacts::content`, en-proto's `ContentView`, which en-core's `record_load` fills on each load of the content and the
DLL keeps on T6 (null before the first load: an empty body, each section's own words):

- **Shipped content** (`shipped`, a key-value list): the name, EldenNet's version and the credit, our fixed words (the
  shipped pack carries no manifest of its own yet); the shipped cast's persona count and its card count (v0.2 ships
  none: left out); knowledge and event rules as "Included", never counted (SP5).
- **Your overrides** (`overrides`, a table): each file of the overrides folder by its path relative to it (PV5:
  `shown_path`, checked in en-core and again here; anything else on a row "Other files"), its state (Loaded, Refused,
  "Not loaded: another file was refused", or Not read for a file the DLL left out), and a row per problem with its line
  and the content layer's fixed words, which name a key's path and never a value of the file.
- **Turned off** (`off`, a key-value list): disable.toml's personas and cards by id, its knowledge entries, triggers and
  rules by count, each left out when empty (v0.2's content layer turns off personas alone).
- **Reloads** (`reloads`, a table): each load this session, newest first (20 at most): its time since the start, what
  it read (the shipped content and the overrides at the start, the overrides again after a file of the player's own was
  written) and its result in words (loaded, with its warnings; or refused for its problems, the content loaded before
  staying in use or none in use).

The Plugins page, and Content packs' link to it, wait for 1.1 (D104): absent until then.

## Docs (`docs`; PAGES §4.6.9; S11.4.a)

At the rail's foot between Settings and Content packs, marked `foot`; increment 0.2, so drawn in game too (PAGES has it
in the browser at 0.2 and in game at 0.6, but the page format cannot keep a page out of one view: GraceUI's
eldennet-changes §77). A master page (D215): the list `docs.list` (columns Document and Covers, a row a document,
`{id, title, headings}`, the headings joined by " · ") with the list's own search on its titles and headings (D250), so
no body is ever searched; and one section holding the chosen row's document, `{ kind = "document", topic = "docs" }`
(D251), the member `docs/<id>`, `{title, blocks}`. No command.

The ten documents are the drafts in `docs/`, in `docs/README.md`'s Markdown subset, converted by `tools/docs/build.py`
into `src/docs_data.cpp` (`docs.hpp`'s `bundled_docs()`: each one's id, title, headings and body as nlohmann/json writes
it), so nothing reads them from disk at run time and nothing is fetched (PV6). The converter fails, naming the draft and
its line, on anything outside the subset (HTML or angle brackets outside code, an image, an address, another emphasis, a
nested list, a block with no blank line before it, a mark in a table's cell or a heading, a line over 120 characters),
on GraceUI's limits (64 KiB a document, 400 blocks, 2000 characters a paragraph, item or note, 120 a heading, 16 KiB a
code block, 100 items, 8 columns, 200 rows) and on a link to a document or a heading that does not exist; CTest runs it
(`en_docs_build`, `--check`: the file is what the drafts build) and its tests (`en_docs_tests`, invented drafts each
outside the subset its own way). About's slots are filled when the space publishes: `{eldennet_version}` EldenNet's
version, `{graceui_version}` the GraceUI connected, `{build}` the build's label (`SpaceFacts::label`, EN_VERSION_LABEL);
a row whose slot has no value is left out. The list and each document are published when watched and not again (they are
bundled), About again when its slots' values change.

## Tools › Bug report (`tools.report`; PAGES §4.6.8; S11.4.a)

Tools' first page in 0.2 (Workshop, below, is in the browser alone; Event monitor comes later), increment 0.2, both
views: a preview, with no copy or save until 0.6, after EldenNet's scan. Two sections:

- **What happened**: a form (`report.build` {what}) with one prose field (D251), 2000 characters at most, its caution
  in the section's description. The space keeps the words in memory for the preview alone (`Space::report_what_`):
  spaces round them taken off, a CR LF as a line break, scrubbed as the log is (a SteamID64's shape, a word shaped like
  a path or a local address "redacted"); a control character other than a line break or a tab, or more than 2000
  characters, is refused in words and the words kept as they were; an empty field clears them. Every copy but the
  preview's is overwritten, the frame's too. The command's note names the command alone, so the log never holds them;
  they reach no recording (the pages' commands never reach the core), no other topic and no file.
- **The report**: `report_preview` on the topic `report`, `{parts: [{id, label, optional, included, fields}]}`, in our
  words and numbers, from what EldenNet already has in plain data: What happened (once there are words); Versions
  (EldenNet's, the build's label, GraceUI's and the floor, the game's build, what started it); State (the lamp's word
  and why, the start, the offline gate, the reading, the read groups' self-tests and those stopped, Home's problems,
  the AI's lead, the log, the recording, the session's length); and three optional parts, switched by `report.part`
  {id, on} (declared, never a file command): Settings (whether settings.toml was read, and each setting's value as the
  file would hold it, in the mono face; never a secret, and a value shaped like a path left out, PV5), Content (the
  counts of Content packs: personas, cards, the overrides' files by state, what is turned off, the loads and the last
  one's result, the player's own notes on characters counted, never shown) and Recent log (its newest 30 lines with
  their clock, scrubbed again, in the mono face). A part left out stays in the body, its `included` false: GraceUI draws
  it in the muted ink with its fields hidden. Never chat or model text, a key, a SteamID, a path, the Windows user
  name, the character's name or a local address (PV5, PV7). Published at most once a second (its readings' count and
  its log's lines change at every wake), and at once after either command.

## AI › Requests, Home's Model calls card and Pause AI (`ai.requests`, `home`; PAGES §4.6.1, §4.6.2; S11.4.a)

On GraceUI 0.2.46's quick switches and live pages (D252, protocol 1.13), 0.2.49's row drawer and header switch (D256,
protocol 1.15) and 0.2.43's filter bar (D250), from the model calls' record the DLL keeps (`SpaceFacts::calls`,
en-proto's `CallsView`, `ai_calls()`) and Pause AI as the core holds it (`SpaceFacts::ai_paused`, `ai_paused()`):

- **AI › Requests** (`ai.requests`, increment 0.2, live): Now (`ai.status`); This session, `stat_tiles` on `usage`
  (Calls, Tokens in, Tokens out, Failures, Own-initiative this hour, and Cost reported only once a provider's reply
  reported one, D18, "by N calls") and the hour's line, own-initiative calls and every request the model client sent
  in the last hour against `budget.requests_per_hour`, which counts them all; Calls, a table on `calls`' `requests`
  member, every row kept (20, or 200 with Developer views from the next start), newest first: Time, Job (the slot's
  player label), Purpose and Outcome (PAGES' words, en-proto's), Speaker as shown, Provider and model ("model on
  provider"; a model id shaped like a path left out, PV5), First token, Total, Tokens (in and out, a number), then Why
  (the error class in words) and Text shown (a reply's displayed text once a page of it reached the screen, and nothing
  else, PV4), the row's drawer (GraceUI 0.2.49's D256: columns marked `drawer`, left off the row and shown under it,
  label above words, when the row is opened; an older GraceUI shows them on the row). Its filter bar (D250): Job,
  Purpose and Outcome; sorted by Time (newest first), First token, Total or Tokens (most first) on each row's hidden
  keys `at_ms`, `first_token_ms`, `total_ms` and `tokens`, a key the call has no value for left out so that it sorts
  last; no search (PV4). Each row carries its `id`, the record's, as words, which the drawer follows.
- **Home** (live): the Status card's last line, Pause AI "Off" or "On, for this session", and the lead's reason "AI is
  paused for this session." at L2 when not dormant (en-proto's kAiPausedStatus); and Model calls, a table on `calls`'
  `home` member, the newest 20 in Requests' words (Time, Job, Purpose, Speaker, Model, First token, Total, Outcome),
  Text shown in each row's drawer and each row with its call's `id`, with the hour's line under it. Its header has
  Pause AI's switch (GraceUI 0.2.49's D256, PAGES §4.6.1's chip): the quick switch's command and state, `{ id = "pause",
  label = "Pause AI", command = "ai.pause", tone = "secondary", state = { topic = "status", field = "ai_paused" } }`,
  sent by GraceUI as `{"on": ...}` flipped from `status.ai_paused`, "Not known yet" until it comes.
- **The body's size**: `calls` stays within GraceUI's frame to a page (kCallsBodyMax, 240 KiB, below its 256 KiB):
  past it, Requests' oldest rows give up their text, then go; Home's card is the newest 20 whatever happens. `calls` is
  published at most twice a second, `usage` once a second.
- **Pause AI for this session** (D15): the space's quick switch (`[[space.quick]]`, GraceUI §78's form) and Home's
  header switch (§82's), `ai.pause`
  `{"on": bool}`, its state `status.ai_paused` (published from the first body, starting or not), living on Home. The
  command is answered at once and handed to the DLL as an `AiRequest` of kind Pause (`ai_pause()`, which the core takes
  in the same wake); an argument that is not a boolean `on` is `invalid_value`; its note's key says on or off. It is
  not sensitive: a paired device may pause and resume, as it may run the self-test. While paused, `selftest.run` and
  `models.list` are refused `not_now` in en-proto's words (kAiPausedSelfTest, kAiPausedModelList), the AI pages lead
  with the pause, and Requests is greyed at L1 with the other AI pages.

## System › Performance and System › Traces (`system.performance`, `system.traces`; PAGES §4.6.7; S11.4.a)

Both live (D252), in System after Status (Status, Performance, Traces, Logs, as PAGES §4.2.1 has them), each on one
topic, with EldenNet's own figures alone: never one of GraceUI's or the game's as EldenNet's, and a figure with no
source in this version left out (absence is silent).

- **Performance** (`system.performance`, `SpaceFacts::perf`, en-proto's `PerfView`, which the DLL fills each wake from
  the reader's histograms on T2, its own of the core's steps on T6 and the core's log in memory): This session,
  `stat_tiles`, the core's steps' p99 and the reader's, each once it has run; Budgets, a `check_list` on the `budgets`
  member, C3's limits for the reader's step beside its figures (its mean within 0.15 ms, its p99 within 0.5 ms: passed
  within, a warning over); Tasks, a table on `tasks` (Task, Steps, Mean, p99, Longest), a row for the reader's every
  step, each reading it built and the core's every step, a kind that has not run left out; Storage, a key-value list on
  `storage`, in words: no database in this version, and the conversations' log in memory (its entries, at most 4,096,
  and those that gave way). A p99 is its histogram bucket's upper bound (up to a fifth high, never low); the mean and
  the longest are exact. No frame cost (EldenNet registers no callback on GraceUI's frame in this version: its panels
  are lists T6 submits, which GraceUI draws) and no memory (EldenNet counts none), so PAGES' tiles for them wait for a
  source; nor C3's other limits (the render feed, the panels, the callback, the frame rate). Related: Status, Traces.
- **Traces** (`system.traces`, on `traces`: `SpaceFacts::traces`, en-proto's `TracesView`, which en-core's calls'
  record keeps with its rows, `ai_traces()`): This session, `stat_tiles` (First page, median; First page, 90th
  percentile, from 10 lines shown on; First token, median; Lines this session, "of N shown" when some calls for a line
  showed none) and a sentence, one line in game, holding the median and the 90th percentile against C5's targets for
  where the Conversation job's model runs (a cloud provider's 1.6 s and 3.0 s, a local one's 2.5 s and 4.5 s); Lines, a
  table of each line's call kept, newest first (Time, Kind in PAGES' words, Job as Requests shows it, Queue, First
  token, First page shown, Total, Verdict: quick within 1.6 s, acceptable within 3.0 s, slow beyond, a call-out's by
  1.5 s and 2.5 s, none for an own-initiative line or one with no page), a time it does not have left out. No text at
  all: no line, prompt, speaker, provider or model. Related: AI › Requests, AI › Self-test, Settings › Developer.
  Requests and Self-test name Traces among their Related now, and Status names Performance, as PAGES gives them.

## Tools › Workshop (`tools.workshop`; PAGES §4.6.8, SP8; S11.4.a; GraceUI D258)

EldenNet's raw workings for testers, SP1's one exception, increment 0.2, in the browser alone (`browser_only = true`:
absent in game, from its rail, search and Related), present only while Developer views are on (`shown_with =
"debug.panel"`), behind its gate once a page load, nothing of it drawn or watched before (`gate`), and under the red
Spoilers ribbon at its top (`ribbon`; SP8's ribbon read as the page's own, S11.2.bk). Its words are the owner's (O129
(a)): the gate's "Workshop shows EldenNet's raw workings: the text sent to the model and its replies, every event, and
what each character knows. It contains spoilers. Open it for this session?", its button "Open Workshop", its
description, help and section titles. Live (D252). Its header switch, Record this session (D256), sends
`recording.set` {on}, sensitive (D221: it writes to this PC's disk, as `debug.record_sessions` does; O131 (b)). Its
facts are en-proto's `WorkshopView` (`workshop.hpp` there), which the DLL keeps on T6 while the switch is on, null while
it is off; `Workshop` (`workshop.hpp`) builds the bodies, keeps the events stream and takes the command:

- **The last prompt and reply** (`workshop.exchange`, members `facts` and `parts`): the newest call of a conversation
  or of the profile editor (O-g (a)), as sent and as answered: its job, purpose, provider, model and the model that
  answered, its outcome in en-proto's words, finish reason, times, tokens and attempts, what was asked for (the length,
  the temperature, the stop sequences, a schema), and the bytes left out past its bounds (64 KiB of messages, 16 KiB of
  reply and of raw text); then a `report_preview` part a message (its role, and the cache mark after it) and one for
  the reply (its text, and the content as the model wrote it, its reasoning included, where it differs), each a `code`
  field. The DLL has replaced each configured key's value, anything shaped like a key and the playthrough's name
  (matched whole: "[the Tarnished's name]", PV2) before it kept them; here the words shaped like a path or a local
  address and a SteamID64's shape are scrubbed again (`words_scrubbed`, the bug report's rule, PV5). A server's
  message is never kept: the outcome names the error class and its HTTP status.
- **Every event** (`workshop.events`, a stream, ring 500): each of Augur's events of every step, unfiltered, in the
  golden files' form (en-augur's `event_json`), the game's handles named h1, h2, ... for the session; a filter bar on
  kind, class and source and a search in the line (D257); and `workshop.status`'s `events`: how many came, how many
  are kept, and the bus's drops this session.
- **World view** (`workshop.world`, members `frame`, `stages`, `flags`, `tracks`): whether the playthrough's flag frame
  is read, the journey, the watch list, the region; each world stage T, U or F with its `when`; each catalogue flag's
  id, block, whether it is watched, its bit, whether its id exists, whether it is verified in the file and here, what
  it reads, its kind, NG+ rule and spoiler level (a filter on what it reads and on verified here, a search on its
  alias); each track's active step.
- **What each character knows** (`workshop.known`): each persona's known set at its active stage, an item a row, its
  certainty, path, own affairs and whether block 5b shows it or offers it (a filter by character). The reaction cue
  that would fire now comes with memories (v0.3): absent until then (O-c (a)).
- **Unknown characters** (`workshop.unknown`, members `rows`, `toml`): those of the snapshots that bind no persona, by
  NpcParam row and entity id (64 kept, the least recently seen going), with their model, type, team, snapshots, first
  and last seen and nearest distance; and a `report_preview` part each with its `[[persona]]` row for
  identity/personas.toml (ids and numbers only, never a name), a `code` field to select by hand; and Copy alone on the
  block (GraceUI 0.2.56's `share = { copy = true }`, D263): the body's `share` holds every row's TOML, a blank line
  between, LF line ends, 256 KiB at most, as `eldennet-unknown-characters.txt` (the name D263's share holds; no Save is
  offered). GraceUI copies on the click alone and sends EldenNet nothing; a device never gets the share.
- **Recording** (`workshop.status`'s `recording_on` and `recording`): on or off and why, the file this session, its
  lines and size, the rotation, the folder named in words, never its path; and a shortcut to `debug.record_sessions`
  (homed on Settings › Developer: a browser-only page is no setting's home).
- **Event kinds** (`workshop.kinds`, members `kinds`, `rules`): each of Augur's 50 kinds, its builder or what it waits
  for, its placeholders, its payload's fields as the golden form writes them (en-augur's `kind_fields`), the rules
  loaded for it; and each rule loaded, its signal, detector, class, radius, key, times and file and line.

While Developer views are off the space publishes nothing on these topics and takes no event; `recording.set` is
refused `not_now` with "Turn on Developer views in Settings › Developer first."; and the moment they go off each watched
state topic gets `{}` once and the stream an empty batch past a gap (protocol 1.1), since GraceUI's server holds a
watched topic's newest body and a stream's rows for later watchers (G-5's answer: as planned). Every body stays within
`kWorkshopBodyMax` (240 KiB): a table's rows past it are left out and counted. Every `workshop.*` topic is drawn on
Workshop alone, and no setting is homed there. **Workshop is on this PC alone, by pc_only (GraceUI 0.2.57)**: the page
and each `workshop.*` topic say `pc_only = true` (D264, protocol 1.20; the owner's O132 (a)), so GraceUI's server leaves
the page out of a paired device's page data and refuses a device's watch of a Workshop topic with not_now, "Only on the
PC the game runs on.", and `recording.set`, named on Workshop alone, stays sensitive.

## Tests

`en_overlay_tests` (doctest): in every build, the Characters pages (`test_characters.cpp`: the page data with its
master, families, editor and marks, the bodies from invented characters, every refusal at the entry point, the
requests handed over with their reply handles, the answers later, what is published when, and the spoiler golden: no
string a projection does not publish reaches a body), the surfaces laid out (the panels' places, text decoded, upper-cased
and wrapped, a page's, a card's, the box's and the hint's lists checked as GraceUI checks them, the caret and its
scroll, the fades and PageShown, the toasts' keys, the input mapping and the swallowed character, the type at connect
by Plain text and its cap, each type's faces and the name line in its own face), the view's lines (every section, the
newest event and the newest change
first with their ages, each event's columns and its square's source, full lists within the view's lines and its panel,
nothing but our words and numbers whatever the status holds, a long word cut), the newest ring (the last N, the newest
last in its copy, every push counted), the world-state claims, and the space:
its generated page data (the ids, the rail, the increment, GraceUI 0.2.5's checks met), settings.set and settings.reset
with each refusal, a secret's status alone, the AI pages' topics, commands, preset and keys (a key in no frame, answer
or note), what is published and when, the logs stream's batches and ring, the words
(no lamp while starting, the banner's own, absence silent, nothing personal), Content packs' topic from invented content
(`tests/content_fixture.hpp`: each section's member, a layer refused alone and a reload refused at a line, a card turned
off, no count of nothing, and no path that is not relative to the folder, PV5), Docs (its master page and search, the
ten documents in PAGES' order, each within GraceUI's limits, its links to documents and headings that exist and no
address, About's slots filled or their rows left out, the list and the documents published once and About again when a
slot changes), Bug report (its form and preview, the parts in plain data, the player's words leading the preview and in
no other topic, answer, note or log row, scrubbed, refused in words over 2000 characters or with a control character,
cleared by an empty field, the optional parts' switches and an always part's refusal, the log's newest 30 lines
scrubbed again, settings without a secret or a path), Requests and Home's card (`tests/calls_fixture.hpp`: PAGES'
columns and words, the sort keys left out where a call has none, Home's 20 without them, the tiles and the hour's line,
cost only where reported, nothing without a record, no address, key or path in a frame, a model id shaped like a path
left out, the body within the frame to a page with the newest text kept), Pause AI (its command handed over, refused
without a boolean, `status.ai_paused` and the Status card's line, the leads, the self-test and the model list refused
in en-proto's words), Performance and Traces (`tests/system_fixture.hpp`: the tiles, C3's limits beside the figures with
a warning over one, the tasks and the storage in words, a kind of step not yet run left out, no frame or memory figure;
the traces' tiles and sentence against the cloud and the local targets, the 90th percentile only from 10 lines, the
table in PAGES' columns, a time not known left out; nothing published without facts, the rates, and no text, address,
path or SteamID in any frame of either), and in the Linux builds the five fixture states'
`tests/pages/<state>/topics.json` (EN_UPDATE_FIXTURES=1 writes them anew). In the
Windows builds, under Wine, the connection against a fake GraceUI of our own (`tests/fake_grace.hpp`; graceui-sys has no
C mock): ABI 1.9's table, answering as GraceUI 0.2.68 answers and checking as it checks (connect's answers, notices
under keys, the registration window, hidden panels, hotkeys, poll's events by the event's size, the space's slots, and
submit's draw-list checks: counts, bounds, ranges, indices, clip rectangles inside the panel, known textures, the
stride). The cases: the space registered once for its views, or not without the cap or beside anti-cheat; watched
topics published, each command answered once, nothing while unwatched; a frame too large kept; poll passing over the
display's event and any later kind; connect's request; each
failed connect headless; the banner replaced in place and withdrawn, the badge, a long text cut; the notice-only mode;
the world-state claims sent on change, and none without the cap; the view (its atlas before any submit, a hidden panel,
F11, a list before it shows, lists every step while open, hidden again, an even count of presses leaving it as it
was); a view that cannot be made; the surfaces (`test_chat.cpp`: what connect registers, faces too late after warm-up,
Plain text's faces at connect and only their files read, the serif without the cap or with Plain text off, a change of
Plain text in the session noted and nothing registered, older GraceUIs without the chord or the holder's events, the
box opened with its input, typing with the swallowed
character, the caret, Enter, the line's limit and a kept draft, Esc, danger, focus lost and the model's Shut keeping
the draft, refused and not now, a page's PageShown and its fade at ten frames a second, Tab, toasts, the hint's key
words and their change, a fault, and the wait handle); the Mark's hotkey and its presses; F11 or Pause taken by another mod, registered
unbound with its id; GraceUI gone.

CTest also runs `en_pages_build` (`tools/pages/build.py --check`), `en_pages_lint` (GraceUI's own page lint, from a
GraceUI checkout whose files match the pin, `EN_GRACEUI_SOURCE`) and, in a Linux build with `EN_GRACE_DEV`,
`en_pages_preview`: grace-dev serves each fixture state, a browser of our own (`tests/pages/browser.py`) reads every
topic back and sends commands, and each page's in-game structure (`--dump`) matches `tests/pages/golden/`
(EN_UPDATE_GOLDENS=1 writes them anew), but Tools › Workshop's, whose dump is refused in every state (it is in the
browser alone). With `EN_GRACEUI_SOURCE` and its `web/` built, `en_pages_browser` runs GraceUI's space check (D265) on
two folders of the world state, Developer views off and on, in GraceUI's real browser view: no console error, CSP
violation, off-origin request or text under 4.5:1, and each view's rail, each page's presence and the text of Home,
the AI pages, Tools › Workshop and Settings › Developer as `tests/pages/browser/` holds them (Workshop behind its gate
in the owner's words, under its ribbon, absent with Developer views off and on a paired device; Home's and AI's held
since GraceUI 0.2.62 reads a page once its every topic has come, D269); skipped while `web/` is not built. Workshop's
own cases (`test_workshop.cpp`): its four page fields in the owner's words and its place,
every `workshop.*` topic drawn there alone and no setting homed there, nothing published, kept or taken while
Developer views are off, `{}` and the stream cleared the moment they go off and the bodies back when they come on, the
stream's ring and batches, the bodies from invented facts with the prompt's paths, addresses and a SteamID's shape
scrubbed, recording.set at its one command path, and every body within the frame at the facts' bounds; the world
state's `topics.json` holds Workshop's topics with Developer views on.

End to end, the fake game's harness runs eldennet.dll with a test build of GraceUI under Wine with Direct3D 12
(`tests/fakegame/README.md`), `launcher-eldennet-graceui-pages` among its cases: F5's menu on EldenNet's pages, a
setting changed from it, and a browser on GraceUI's server reading the topics and sending commands; and
`launcher-eldennet-graceui-events`: the fake game's world and the lifecycle raising Augur's events, and the view's
squares of both colours counted on the game's frame; `launcher-eldennet-graceui-workshop`: Workshop's page data,
its topics while Developer views are on, Record this session off and on, Workshop never drawn in game, and its topics
cleared once the switch is off; and `launcher-eldennet-graceui-talk-workshop`: the first conversation's request and
reply on Workshop, the character's name masked in the line typed.
