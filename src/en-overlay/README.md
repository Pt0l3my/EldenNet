# en-overlay: EldenNet's display layer, through GraceUI

`en-overlay` is EldenNet's connection to GraceUI (ARCH §2.2, GraceUI G15), C++20, MIT: the banner and the pause badge
as GraceUI notices, EldenNet's claims as a world-state provider, its hotkeys (F11 and the development Mark), the F11
debug view, a panel drawn on Dear ImGui 1.93.0's core with no backend, and EldenNet's space: its pages in GraceUI's menu
and browser view (PAGES; GraceUI's G20, D169). It began as the first in-game build's part 12 on GraceUI 0.1
(`docs/eldennet-changes.md` §14 to §18 and §20, D131 to D156, D164 and D165); the pages' first slice (roadmap S11.4.a)
moved it to GraceUI 0.2.5's ABI 1.2 (§19, §23, §24, §26 to §29, §33, §34 and §36). Later increments add EldenNet's own
surfaces (subtitles, the chat box, the call-out box) as panels, and the rest of its pages.

It reaches GraceUI through `graceui-sys` alone (`third_party/graceui-sys`, vendored from GraceUI at 8d45b20, GraceUI
0.2.5; its floor, 0.2.3): its look for GraceUI and `GraceUI_RequestApi(1)` (`graceui_sys.h`), ABI 1's calls, each behind `has_cap` and the table's size
(`graceui_sys.hpp`), and the headless Dear ImGui helper (`graceui_imgui.hpp`). It links no hook library and no Dear
ImGui backend and hooks nothing (R12, GraceUI's invariant 16); it never links en-core or en-gal, so it can never reach
the game's memory (ARCH §2.3, checked by `en_link_rule`); and nothing in it waits but the look, on T1 (R6).

## Two parts

- **Portable, in every build** (tested on Linux with GCC and Clang too): `view.hpp`, the F11 view's lines, and
  `world.hpp`, the world-state claims. Both are pure, on en-proto and `graceui.h`, which is plain C.
- **The Windows builds** (MinGW-w64 and clang-cl): `overlay.hpp`, the connection, and Dear ImGui's core
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
   (0.2.3, `en/proto/graceui_pin.hpp`) and the caps it uses: `notices world-state hotkeys`, and with the view
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
  order (a category with no page yet holds none, and GraceUI shows no empty category), the slice's pages (Home, System ›
  Status and Logs, Settings › Developer), its topics (`settings`, `status`, `errors`, `system.status`,
  `system.compatibility`, `logs`), its commands (`settings.set`, `settings.reset`) and the settings v0.1 reads whose
  home is there (`debug.panel`, `debug.record_sessions`, `debug.log_level`). `python3 -I tools/pages/build.py` writes
  `pages/pages.json`, `pages/settings.json` and `src/space_data.cpp` from it (`--check` in CTest); every page ships in
  the increment EldenNet registers, `0.1` (D173). Every word a player reads there is a draft for review.
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
  `page`; no lamp while EldenNet is starting) and Home's Status card (`items`); `errors` Home's Problems (`checks`: the
  failed start in the banner's own words with its fix, a read group stopped, a refused settings file);
  `system.status` System › Status's Health (`items`), EldenNet's parts as lamps (`checks`: the start, reading the game,
  the core, the anti-cheat watch, the log, the recording when asked for, GraceUI) and the read groups' self-tests
  (`columns`, `rows`); `system.compatibility` the versions and how EldenNet started; `logs` the log's own lines (500
  held, from T1's first, scrubbed again of a SteamID64's shape, PV5).
- **One command path** (GraceUI's invariant 3): `settings.set` (the key known, not locked, the value of its type and in
  its bounds or choices) and `settings.reset` (a key at once; a page or every setting only with `confirm: true`,
  D173), each refusal by its code, with words in `detail` where the code says too little; nothing while the settings
  file the player wrote was refused at the start. A secret is held and published as its status alone. What a command
  changed is taken by the DLL (`take_changes`), which applies it and writes settings.toml.
- **What it publishes and when**: nothing for a topic nobody watches; a state topic when it is watched (again for each
  new watch) and when its body changes, no oftener than its rate (`status` 4 a second, `system.status` once a second,
  the rest on change); the logs stream from its oldest row held, in batches of 100, then each new row once a second.

## Tests

`en_overlay_tests` (doctest): in every build, the view's lines (every section, the newest event and the newest change
first with their ages, each event's columns and its square's source, full lists within the view's lines and its panel,
nothing but our words and numbers whatever the status holds, a long word cut), the newest ring (the last N, the newest
last in its copy, every push counted), the world-state claims, and the space:
its generated page data (the ids, the rail, the increment, GraceUI 0.2.5's checks met), settings.set and settings.reset
with each refusal, a secret's status alone, what is published and when, the logs stream's batches and ring, the words
(no lamp while starting, the banner's own, absence silent, nothing personal), and in the Linux builds the five fixture
states' `tests/pages/<state>/topics.json` (EN_UPDATE_FIXTURES=1 writes them anew). In the
Windows builds, under Wine, the connection against a fake GraceUI of our own (`tests/fake_grace.hpp`; graceui-sys has no
C mock): ABI 1.2's table, answering as GraceUI 0.2.5 answers and checking as it checks (connect's answers, notices
under keys, the registration window, hidden panels, hotkeys, poll's events by the event's size, the space's slots, and
submit's draw-list checks: counts, bounds, ranges, indices, clip rectangles inside the panel, known textures, the
stride). The cases: the space registered once for its views, or not without the cap or beside anti-cheat; watched
topics published, each command answered once, nothing while unwatched; a frame too large kept; poll passing over the
display's event and any later kind; connect's request; each
failed connect headless; the banner replaced in place and withdrawn, the badge, a long text cut; the notice-only mode;
the world-state claims sent on change, and none without the cap; the view (its atlas before any submit, a hidden panel,
F11, a list before it shows, lists every step while open, hidden again, an even count of presses leaving it as it
was); a view that cannot be made; the Mark's hotkey and its presses; F11 or Pause taken by another mod, registered
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
