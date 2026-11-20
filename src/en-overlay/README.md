# en-overlay: EldenNet's display layer, through GraceUI

`en-overlay` is EldenNet's connection to GraceUI (ARCH §2.2, GraceUI G15), C++20, MIT: the banner and the pause badge
as GraceUI notices, EldenNet's claims as a world-state provider, its hotkeys (F11 and the development Mark) and the F11
debug view, a panel drawn on Dear ImGui 1.93.0's core with no backend. It is the first in-game build's part 12, on
GraceUI's ABI 1 preview (GraceUI 0.1, `feat/foundations` at 3f1352b; GraceUI's `docs/eldennet-changes.md` §14 to §18
and §20, D131 to D156, D164 and D165). Later increments add EldenNet's own surfaces (subtitles, the chat box, the
call-out box) as panels.

It reaches GraceUI through `graceui-sys` alone (`third_party/graceui-sys`, vendored from GraceUI at 32ab1ed, GraceUI
0.2.3): its look
for GraceUI and `GraceUI_RequestApi(1)` (`graceui_sys.h`), ABI 1's calls, each behind `has_cap` and the table's size
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
   panel on the panel layer at the display's top left (24, 24; 680 by 420), and F11 registered as the hotkey
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
- **`step(status)`**: `poll` until GraceUI has nothing waiting; each hotkey event names its presses since the last; an
  odd count of F11's toggles the view, and the Mark's are counted for the DLL. To open, one list is built and submitted
  and then the panel shown, so it never shows empty; to close, the panel is hidden. While it is open, every step builds
  a frame with `io.DisplaySize` the panel's size, `ImGui::Render()`, appends its lists into one GraceUI list
  (`graceui::imgui::DrawList`) and submits it; a frame past ABI 1's bounds is not built, and the view keeps its last
  list. `not_connected` or `inactive` from `poll` ends it: nothing more is called.

The view takes no input in GraceUI 0.1 (D142): F11 shows it and hides it.

## The F11 view (`view.hpp`)

Its first line is a ribbon, "EldenNet developer view (F11 closes it)" (PAGES SP8), then EldenNet's version and build
label; T1's state and discovery's items and passes; en-core's phase, level and banner, the gate (open or closed, and
why), Dormant and the badge; the task's snapshots, its p99 and read mode; each read group's state from the newest
snapshot (`ok`, `untested`, `tripped`, with its hard faults), the groups C2 reported tripped, the copies and the faults
by kind; the Marks and who stamps them; GraceUI's version; and the last eight of en-core's changes, the newest first,
with their ages. It holds no game text: every word is the name of one of our own enums or a word of the view's own, each
checked again as it is written (letters, digits and `_.+-`; anything else `?`), and the rest are numbers. "The last
events" are en-core's changes until en-augur's events (part 11) join them.

## The world-state claims (`world.hpp`)

From what v0.1 knows: the title and loading from en-core's phase; multiplayer when the badge is up or the gate closed
for a reason of the session; the anti-cheat from Safety's check and T10's. Two are stand-ins, documented as such:
danger, until en-core's `safe_context` (NPC §5.4) with en-augur's combat state, is the player dead, a boss bar, or the
player locked on, in a world; a game menu, until NPC §5.8's menu read, is the HUD's pop-up menu state, in a world. The
game's own text entry is not read in v0.1.

## Tests

`en_overlay_tests` (doctest): in every build, the view's lines (every section, the newest change first with its age,
nothing but our words and numbers whatever the status holds, a long word cut) and the world-state claims. In the
Windows builds, under Wine, the connection against a fake GraceUI of our own (`tests/fake_grace.hpp`; graceui-sys has no
C mock): ABI 1's table, answering as GraceUI 0.1 answers and checking as it checks (connect's answers, notices under
keys, the registration window, hidden panels, hotkeys, poll's events, and submit's draw-list checks: counts, bounds,
ranges, indices, clip rectangles inside the panel, known textures, the stride). The cases: connect's request; each
failed connect headless; the banner replaced in place and withdrawn, the badge, a long text cut; the notice-only mode;
the world-state claims sent on change, and none without the cap; the view (its atlas before any submit, a hidden panel,
F11, a list before it shows, lists every step while open, hidden again, an even count of presses leaving it as it
was); a view that cannot be made; the Mark's hotkey and its presses; F11 or Pause taken by another mod, registered
unbound with its id; GraceUI gone.

End to end, the fake game's harness runs eldennet.dll with a test build of GraceUI under Wine with Direct3D 12
(`tests/fakegame/README.md`).
