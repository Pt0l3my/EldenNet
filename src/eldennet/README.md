# eldennet: the DLL in the game

`eldennet.dll` is EldenNet's engine in the game process (ARCH §2.2, §3), C++20, MIT: the launcher loads it (part 7),
or a player's own loader does. This is the first in-game build's DLL (part 6), reading the game through en-gal and
stepping en-core's v0.1 slice, with GraceUI's connect since part 12 (en-overlay): the banner and the pause badge as
GraceUI notices, EldenNet as a world-state provider, the development Mark on GraceUI's hotkey, and the F11 debug view. No code comes from any other project; it uses nothing of me3 or fromsoftware-rs (O47, O48), and
nothing in it knows the game: every fact comes from `gal/` beside it.

It is built in two parts:

- `en_shell`, portable and tested in every build (`en_shell_tests`): `settings.hpp` (settings.toml as v0.1 reads it),
  `logline.hpp` (the scrubbed log's lines), `rings.hpp` (the channels C1 to C5 and the log's ring), `record.hpp` (the
  recorder's numbers-only lines) and `names.hpp` (the safety checks' file names). en-tools links it for `lint`'s
  recording vocabulary. The read-only round's report, which the probe build fills, is en-gal's
  (`en/gal/lodestar.hpp`), so that the launcher lints it without en-core (ARCH §2.3).
- `eldennet.dll` (`win/`), the clang-cl build's alone (NativeSys needs structured exception handling), and, with the
  tests or the CMake option `EN_PROBE` on, the same DLL with the probe option, `probe/eldennet.dll`, which runs the
  read-only round.
  A release build is `eldennet.dll` alone, and CI's `options` stage refuses `EN_PROBE` there (ARCH §12.1).

## The start (ARCH §1.4, §3.2), as built

`DllMain` does one thing (ARCH §3.1): for `DLL_PROCESS_ATTACH`, en-gal's `sys::boot::on_attach` calls
`DisableThreadLibraryCalls` and starts T1; any other reason returns `TRUE` and nothing else (R14: there is no detach
branch). No static object of the DLL or the modules it links has a dynamic initialiser or an exit-time destructor
(`en_dll_module`, ARCH §3.1, §1.7). Its exports are `DllMain` and `eldennet_instance_marker` (R16).

T1 then runs, each state logged and written to the watch block for the launcher's dev console:

1. **Pin** the module (`GET_MODULE_HANDLE_EX_FLAG_PIN`), and take **R16's mutex** `Local\EldenNet.Instance.<pid>`. A
   second copy finds it, writes one `OutputDebugStringW` line and ends T1: no log, no block, no thread, no task. (ARCH
   §3.2 takes the mutex in Safety; it is taken at Pin here, so that a second copy loaded a moment later cannot overtake
   the first while it gates.)
2. **Gate**: the main module's mapped headers against `gal\builds.toml` beside the DLL (en-gal's `gate_image`).
3. **Safety**: the anti-cheat check (ARCH §5.3: a module whose file name starts with `EasyAntiCheat`, or a parent
   `start_protected_game.exe`); the log starts (`%APPDATA%\EldenNet\logs\eldennet.log`); the launcher's watch block is
   opened, or, under another loader, the watcher is started (`eldennet-launcher.exe --watch <pid>` beside the DLL,
   O71) and its block awaited for 3 s.
4. **Settings**: `%APPDATA%\EldenNet\config\settings.toml` (below); `enabled = false` ends T1 (Off).
5. T6 (the core), T10 (the watcher) and T11 (the log writer) start.
6. An unknown build or anti-cheat takes the **banner path**: WaitWindow, the crash filter, **ConnectB** (below; with
   anti-cheat GraceUI's own check puts it in the notice-only mode), the banner (L1), end.
7. **WaitWindow**: a visible top-level window of the process, polled every 100 ms, at most 120 s from T1's start.
8. **Connect** (`overlay.cpp`, en-overlay): GraceUI found by graceui-sys's look (its module name, an export scan, then
   `GraceUI\graceui.dll` in the game's folder, which it loads), again every 250 ms for at most 30 s, but at once when no
   copy is installed there and none is loaded (graceui-sys's own rule, `GRACEUI_SYS_STOP_IF_ABSENT`, GraceUI D164); then
   `connect` as `eldennet`, with its floor (GraceUI 0.2.17) and the caps it uses, and, connected, the F11 view, the
   Mark's hotkey and, on the main path, the conversation's surfaces with GraceUI's own fonts read from its folder
   (below; `surfaces state=… faces=… talk=… chord=… wake=…`). One log line, `connect ok=… outcome=… how=… looks=… installed=… graceui=… view=… mark=…
   mark_bound=… world=…` (`view=unbound` when F11 was taken first, with a line `view hotkey=unbound` beside it); a
   failed connect (absent, too old, a duplicate id, inactive, refused) leaves EldenNet headless, nothing drawn (ARCH
   §10.3), and the start goes on. `overlay.connect = false` skips it (`connect skipped=1 why=off`).
9. **Discover**: the layout, `recipes.toml`, `ids.toml` and `patterns.toml` where the folder holds them (en-gal's
   `discover_with_retries`: at once, then +5, +15, +30 s), for a reader on a thread of our own
   (`FrameProvider::OwnThread`; proposal 0005 §9.4, O101). What the build's table holds (the layout's `[item.*]`
   entries, Lodestar) is taken from it and checked, and nothing looks for it; what it does not hold yet is found as
   before. No registration function, task hub or `[task]` is asked for, and the game's getter is never called: the
   line `discovery … table=… table_refused=… getter_calls=0 …`.
10. **WaitEngine**: a static the game sets as it starts is non-null (the task hub's instance where discovery has it,
    else the module handle's; `engine ready=1 by=<label>`), at most 120 s from T1's start. Then the **crash filter**
    is installed: after the game's C runtime has installed its own, which replaced an earlier one (WHY§172).
11. **Reader**: T2, a thread of EldenNet's own, is made suspended; the stack table is made again with T2's stack in it
    and without T1's (WHY§190); then T2 is let go: `reader started=1 stacks=… period_ms=100`. Nothing of the game is
    called and nothing is registered with it (O101 (b)). In a run of GraceUI's observer (the launcher's `--spike s0e`
    or `s4`, S0-E's present-hook half and S4, read from the watch block; en-proto's `observer_run`), T1 stops at the
    start of Reader instead, before the reader is made: `reader skipped=1 why=spike spike=<word>`, then the
    `spike_run` banner (GraceUI's notice when connected) and L1. What a stopped start keeps runs on: T6 (the core,
    the status block, the status lines), T10, T11, the connect and F11's view (`debug.panel`), the recorder (its
    header carries the word). Nothing waits for the task, which T6 and the view read only once it is set, and the
    crash filter stays in. (`s0e_alone` loads no eldennet.dll at all.)
12. **FirstReading**: at most 30 s for the reader to pass phase A and build its first snapshot
    (`first_reading state=running ms=…`); else Inert and its banner (`no_reading`, or the task's own reason).
13. **Ready**: DataReady went to the core on C2; another module exporting `eldennet_instance_marker` is logged as a
    second copy that idles (the toast comes with GraceUI, ARCH §1.6). T1 ends.

The watch block's states for this start, `reader` and `first_reading`, were appended to en-proto's list; `register`
and `first_tick` keep their numbers there for the game's task, which nothing enters now (R1, retired 2026-11-22,
O106).

**An S0 run** (the launcher's `--spike s0a`, `s0b`, `s0c`, `s0d` or `s0d-crash`; DESIGN §16.1 on the table-based start,
roadmap S10.1.b; `spike.cpp`, en_shell's `spikerun.hpp`) is this start, run as far as gal/'s layout allows, with a
report beside it: at Safety the run's folder (`ELDENNET_SPIKE_DIR`, accepted only as the launcher's
`<data>\spikes\<word>-<n>`), after Discover its counts and the build's table as checked, before Reader (S0-D, a probe
build) the trial, 20 guarded copies forced to fault by each copy primitive (the SEH copy at a page of our own with no
access, ReadProcessMemory at the null page), and the group the reader's faults go into (settings.toml's probe table
where it names one, else the first optional group this start reads, every read of it from 2 s into the world); at the
terminal state, before T1 says so, the table checked again over the image as it is then (`spike_stopped … rechecked=…
changed=…`) and `eldennet-spike.json` written, which T6 writes again every 10 s with the reader's counts (snapshots,
torn, held, unconfirmed, the faults by kind, the groups tripped, the p99). A start that stops short of its reading
says why (`waits`: layout, files, build, anticheat, not_started). The S0 sessions (`s0b`, `s0c`, `s0d`) record
whatever settings.toml says. A discovery refused only for a required static that has no place in the table and no way
to be found, as the real game's `game_state` today (WHY§459), stops at the banner with the detail `layout`, and an S0
run then checks the table with one pass that refuses nothing for such a static (en-gal's `partial`).

A probe build with `probe = { round = "read" }`, or asked by the launcher's `--read`, runs the read-only round after
Connect instead (below), but never in a run of GraceUI's observer: there settings.toml's round is set aside
(`probe round=read skipped=1 why=spike`) and the start stops at Reader, and the launcher refuses `--read` with those
spikes.

## The threads and channels (ARCH §1.2, §1.3)

- **T2, the reader**, a thread of EldenNet's own (O101; never the game's thread), steps en-gal's `ReaderLoop` about
  thirty times a second for as long as the game runs: each step runs the task (en-gal's `GalTask` with
  `own_thread_options()` and steady reading, behind a body that names the thread for the crash filter, whose record
  says `reader` for it and `game` for the thread of the game's window, and in a probe build arms the allocation watch
  and S0-D's fault injection) through `Io`, the task's TaskIo:
  C1 is a `SnapSlot` (two preallocated snapshots; the newest wins and an overwrite marks the next `gap`), C2, C4 and C5
  are SPSC rings of 64. Nothing on it waits or allocates (R6); the probe build counts any allocation there.
- **T6, the core**, waits on one wake signal and, once connected, GraceUI's wait handle (ABI 1.5; or 100 ms), then
  loads the conversation's content once T1 has read it, drains T1's banners, C2's lifecycle messages and C1's newest
  snapshot, in that order, tells the core the model client's health when it changes and hands it the client's events,
  and steps en-core's slice on each and on a 100 ms tick. Once T1 has connected to
  GraceUI, T6 alone calls it, after each wake (`overlay_tick`, below). C14's atomics (anti-cheat,
  `ersc.dll`) join each snapshot's session before the core judges it. It sends the core's commands on C4 (an outbox
  past a full ring), frees retired watch lists (C5), keeps the status block (the core's phase, level and gate flags,
  snapshots, faults, the groups tripped, the task's p99), records the Marks, logs each change and a status line every
  10 s (with `events=`, how many of Augur's events there have been), hands each change and each of Augur's events of
  the step to the F11 view's rings, and notes a stall (a running task with no snapshot for 5 s; WHY§175). A probe build honours the launcher's
  crash request (S0-D's `s0d-crash`) on this thread.
- **T10, the watcher**, repeats the anti-cheat module check every 60 s and notes `ersc.dll` (C14). Hot reload waits
  for content.
- **T11, the log writer**: any thread pushes a fixed line onto a ring of 512 and never waits (a full ring drops and
  counts); T11 writes `ELDENNET <event> key=value ... t=<ms since T1's start>` to `logs\eldennet.log`, rotated past
  1 MiB with one older file kept. A line holds numbers and words only (`[A-Za-z0-9_.+-]`, at most 48 characters):
  anything else a caller passes is written as `redacted`, so no path, user name or text can reach the log.

## GraceUI on T6 (part 12; `overlay.cpp`)

After each wake, once T1 has published the connection:

- **The banner**: en-core's L1 banner, in its text (ARCH §5.4, §10.3, en-core's `banner_text`, with the game build's
  number, read from the image when the gate refused it), as one notice under EldenNet's key 1 with a duration of 0, in
  the warning tone, replaced in place when the reason changes; it works in the anti-cheat's notice-only mode. **The
  pause badge**, "EldenNet paused (multiplayer)", likewise under key 2, withdrawn by an empty text. Each is logged
  (`notice what=banner|badge … status=…`).
- **The world-state claims** (GraceUI D78), sent when they change (`world flags=…`): the title and loading from the
  core's phase, multiplayer from the badge or the gate's session reasons, the anti-cheat from C14; danger and a game
  menu are v0.1's stand-ins (en-overlay's `world.hpp`).
- **The Mark** (O90): GraceUI's hotkey `mark` on Pause, registered when the launcher's console holds Pause (or at
  connect with `debug.panel`), then, once it is bound, the watch block's ring asked for (`marks source=asked`, then
  `source=eldennet` once the launcher has let its key go and handed it over; with no console, at once). Each press is
  then a Mark of EldenNet's own, stamped when T6 takes it (at most 100 ms after it), numbered on in the same ring and
  recorded like the launcher's. Without a block, EldenNet keeps a ring of its own. Until the hand-over, a press is
  logged as not taken; the launcher's Pause stays the Mark whenever GraceUI is absent. When
  another mod's hotkey took Pause first, GraceUI registers EldenNet's unbound (`GRACEUI_UNBOUND`, D164): the ring is
  never asked for, and the launcher's Pause stays the Mark (`marks source=launcher bound=0`; registered on T6,
  `mark_hotkey status=11 bound=0`).
- **The F11 view** (`debug.panel = true`; PAGES SP8, off by default): `poll`'s F11 presses open and close it; while it
  is open each wake builds a frame of EldenNet's status on Dear ImGui's core and submits it (`view presses=… open=…
  events=…`, the events it lists). It shows T1's state and discovery's items, en-core's phase, level, banner and gate,
  the task's snapshots, p99 and read mode, each read group's state, the groups tripped, the faults by kind, the Marks,
  GraceUI's version, Augur's last ten events with their audiences (each mapped on T6 to en-augur's words, `words.hpp`:
  no handle, key or name) and en-core's last five changes; no game text (en-overlay's README).
- **EldenNet's pages** (en-overlay's space, PAGES; GraceUI D169): registered by T1 at the connect (`connect … pages=0`
  when GraceUI took it; both views on the main path, the game's alone on the banner path). T6 keeps the space from its
  first wake, its logs stream fed from every line T11 writes (a tail ring of 256, T11 to T6), and, connected, fills its
  facts from the core, the task, the block and the shell, and lets `step` hand it GraceUI's events: each topic watched
  or no longer and each command logged (`page what=watched|unwatched topic=…`, `page what=command name=… key=…
  result=…`), every command answered at once. A change is applied (`setting key=… effect=now|next_launch`: the log's
  detail at once, the developer views and the recording from the next launch) and, after 300 ms with no other (D14),
  every change of the session is handed to T11, which reads settings.toml again, edits it (en_shell's `edit_settings`:
  every other key and table kept, comments not; a file the parse refuses is never written over) and writes it beside
  and renames it over (`settings_write ok=… keys=… why=…`). T6 does no file work for the pages.
- **The AI pages** (`ai.cpp`; PAGES §4.6.2, ARCH §11): T1, on the main path after Safety, reads `providers.toml` and
  `secrets.toml` from the config folder, `models.toml` from `content\eldenring\` beside the DLL, and makes en-llm's
  WinHTTP transport (the system's `winhttp.dll` by its full path); `ai_files providers=… models=… secrets=…
  transport=…`. T6 parses them with en-llm (`ai_config result=ready|missing|refused …`, a refusal by its file and line
  alone with the refusal's kind, `why=credentials|query|plain_public|plain_cloud|plain_key|scheme|other`, never its
  message; the pages say why in words, by the line), fills the space's `AiFacts` each wake (the files' parts only when they change; the
  configuration's `revision`, which rises with each edit taken; what each provider listed at its newest answered list,
  for the pickers alone, never logged), sets the preset's row from the file, and takes the space's changes and
  requests: the preset and Add the preset's provider (`ai_change what=preset|add`: providers.toml's `preset`, the
  preset's missing providers added with their keys in `secrets:<id>`), a provider of one's own (`what=add_own`:
  OpenAI-compatible; local when reached by http:// on this PC or the home network, with no key's place, which this PC's
  may get on its row once a call asks for a key and the home network's never; else cloud with its key in
  `secrets:<id>`), a job's pick (`what=model|provider|model_reset`: its `[slots.<slot>]` provider and model, on the
  provider the space names, a fallback with no provider of its own kept where it was, or the table gone), a key on a
  row (`what=key name=… set=…`: cleaned, the request's copy overwritten; a provider with no key's place gets
  `secrets:<id>` first, `what=key_place`, but only once a call was refused for want of a key; never one whose address
  is http:// to another computer or whose key is an environment variable's), Remove on a row (`what=remove`: only a provider no job uses, with the keys no
  remaining provider names), each checked by parsing the result first and then posted to T11, which makes the same change on the file as it is then
  (`ai_write file=providers|secrets ok=… why=…`; secrets.toml written with an access list for this user alone,
  `write_private`). The self-test and the model list run on en-llm's handle (T7–T8), which T6 makes when one first
  needs it (`ai_service ok=1 workers=…`) and tells the core's gate at each wake; its notify sets T6's wake. Each run's
  end is one line of words and numbers (`selftest result=passed verdict=quick … first_token_ms=… tps=…`,
  `models_list providers=… passed=…`). en-llm's own log lines are not taken (a server's message could reach them).
- **The first conversation** (`conversation.cpp`, `ai.cpp`, `overlay.cpp`; WHY§364, part C): T1, on the main path,
  reads the overrides folder (`%APPDATA%\EldenNet\overrides\`, DESIGN §11.4: every `.toml`, `.md` and `.j2`, each
  within its cap) and
  the realm, the game's; `ELDENNET_TEST_REALM=eval` in the environment is the tests' switch, which a probe build alone
  takes and the release DLL refuses (`realm asked=eval accepted=0`), so a test persona never resolves in a player's
  game (NPC §2.1). T6 loads them into the core after the shipped content (`content_load ok=… personas=… bound=…
  warnings=…`); the first error or warning is shown once as a GraceUI notice (key 5, 15 s) naming its file, key and
  line in fixed words, never the text (NPC §4.7). Each step's
  requests go to en-llm's handle (made at the first that needs one), its cancels cancel them, and the handle's events
  for the conversation's ids go back to the core; its health (the dialogue slot usable) is the core's NetHealth. After
  the step, `poll`'s events of the surfaces (the talk key, Tab, the box's typing) go to the core in the same wake and
  the newest UiModel is drawn (en-overlay's chat.hpp); what that raises (PageShown, Close) goes to the core at once.
  The log holds the conversation by kinds and numbers alone: `chat what=open_key|opened|submit|page_shown|close|skip`
  (`opened`: the talk key's poll and the box's first submit, `ms` between), `commit kind=… conv=… reply=… page=…
  chars=…`, `llm what=submit|cancel … id=…`; never the chat's or the model's text.
- GraceUI gone (`poll` answers not connected or inactive): `overlay lost=1`, and nothing more is called.

## The watch block and the crash filter (O71, O90)

EldenNet's side of en-proto's `watch.hpp`: T1's states and the time to its terminal one, discovery's counts, the
status above, the Marks the launcher stamps, and the launcher's requests. The **crash filter** is chained to the one
it replaces and writes one record per process (a fault inside it, or a second at once, goes straight on): the code,
the thread (game, init or other), whether it chained, and the faulting module's file name and offset (en-gal's
`fault_site`: no folder); then it wakes the watcher and waits for it at most 2 s. It allocates nothing and calls
nothing of the C runtime; after a stack overflow it skips the module look-up too, so it works in the few KB a thread
has left then, and ends the process itself with the overflow's code instead of passing it on (under Wine, passing it
on hung the process for good, with any filter of a DLL's; WHY§196). Every other exception goes on to the filter before
ours.

## Settings (ARCH §11.5, as far as v0.1 goes)

```toml
schema = 2
enabled = true                                  # false: Off, T1 ends at once
debug = { panel = false, record_sessions = false, log_level = "info" }  # panel: F11's view; log_level: error .. debug
overlay = { connect = true }                    # GraceUI's connect (part 12); false: headless, Connect skipped
gal = { copy = "seh" }                          # or "rpm": ReadProcessMemory on our own process (S0-D)
probe = { round = "read", settle_s = 10,
          fault_group = "camera", fault_every = 0, fault_after_s = 2 }   # a probe build's only (EN_PROBE)
                                                # round = "read": the read-only round (no call; below)
```

Strict: a key no schema defines, a value of the wrong type or out of range refuses the whole file, and the defaults
are used, with each problem's place in the log (never a value). ARCH §11.5's tables for later increments (`features`,
`autonomy`, `range`, `pacing`, `ui`, `pools`, `budget`, `safety`, `hotkeys`, `augur`, `reactions`, `allow`) are accepted
as tables and not read yet. `ELDENNET_APPDATA`, in the environment, stands for `%APPDATA%` (development and the tests,
as the launcher's `--appdata` does).

## The recorder (ARCH §7.5, numbers only)

With `debug.record_sessions = true`, T6 writes `%APPDATA%\EldenNet\recordings\<date>-<time>-<pid>-<n>.ensnap.jsonl`:
plain JSON Lines (no zstd yet: plan §1.3), rotated past 256 MB, flushed every second. Each line is one report of the kind
`recording` through en-gal's closed-vocabulary writer on one line: `{"report":"recording","schema":1,"t":<mono ms>,
"k":"header"|"snap"|"life"|"banner"|"change"|"cmd"|"mark",...}`. `eldennet-engine lint --recording` checks a file, and
DataPiler's guard can check it key by key (O73). `record.hpp`'s `read_recorded_line` reads a line back with the same
words, into what the tools need (eldennet-engine's rec-stats, replay and spike-report): what en-core's slice reads of a
snapshot, the lifecycle messages (a playthrough's name a stand-in of its recorded length), the banners, the changes,
the commands' kinds and the Marks; the rest is counted, not rebuilt. Numbers only by construction: there is no free string, so no name,
SteamID, path, game text or param value can be written. A text look-up's answer keeps its table, id and whether it was
found; a param row its param, id and field count; PlaythroughReady its slot, class, keepsake, play time and the name's
length. Keyed hashes and redacted text wait for S10.2.d.

## The read-only round (Lodestar's step L5; a probe build with `probe = { round = "read" }`)

`probe_read.cpp`. It calls nothing of the game, registers nothing and writes nothing to it; it is the one round a
probe build runs (R1, the round with one call into the game, was retired 2026-11-22, O106).

**How the owner starts it.** `eldennet-launcher --read` writes its request into the watch block's `control.probe`
before it loads anything, and EldenNet answers there at Settings: a probe build accepts (`probe.round` becomes `read`,
with the file's other probe settings); a release build, or EldenNet turned off, refuses and stops at once (T1 ends in
Exit: no other start). The log's `probe asked=read accepted=…` line says which. settings.toml's `probe` table still
works alone (the tests). `round = "r1"` and R1's own keys (`register`, `object_size`) are refused like any unknown
word or key.

On T1, after Connect and `settle_s` seconds:

1. **The finder over the live image** (en-gal's `lodestar::find`, with gal/'s `leads.toml`, `recipes.toml`,
   `ids.toml` and `builds.toml`): the accessor's shape and the statics its sites load, the leads' classes, the
   groups' table. `read stage=found known_build=… recipe=… hits=… accepted=… statics=… ms=…`.
2. **The managers, by their instances** (`lodestar::Namer`, O92): the image's type information read once, then each
   pass reads the statics' slots and one 8-byte word at each instance, through the guarded copy under the reader's
   plausibility rules. Again every 10 s for as long as a lead is unnamed (the round of 2026-11-22 stopped after 60
   passes, and the world got five), each folded in (`merge`): `read stage=named pass=… tried=… null=… classed=… named=… of=…`. The watch block's
   items carry named, asked and passes, for the launcher's console.
3. **The reader's trial** on T2, a thread of our own as in a v1 start (Reader, FirstReading, Ready): en-gal's `Trial`
   under `ReaderLoop`, each named manager read about ten times a second. `read stage=trial … readings=… same=…
   null=… other=… fault=… torn=… faults=… p99_us=…` every 10 s.
   The trial counts its readings by stage (those made while the characters' manager has an instance are the
   world's) and each manager's empty stretches.
   **The searches by what an object holds** start once a save is loaded (the trial's first reading in the world), or
   three minutes after the trial began: for a lead that has a class of its name and still no static, en-gal's
   `probe::SlotSearch` over the data's slots, one lead at a time, again a minute later while it finds none
   (`read stage=search id=… candidates=… found=…`; one slot names the manager, `by_search`); `lodestar::ClockSearch`
   over the accessor's statics no lead has named; and `probe::RisingSearch` over the data's slots that point at the
   heap (`read stage=searches …`, then `read stage=held clock_passes=… clock_hits=… clocks=… rising_passes=…
   rising_hits=… search_faults=…` every 10 s). T1 makes each search and T2 steps it; a slot search T1 takes back is
   freed two of T2's steps later. The searches, and T1's one read at each instance, read through `QuietSys`, whose
   copy asks first (`AskFirstCopier`): every page of the range must be committed and readable, with no guard page,
   by VirtualQuery; only then is it copied, by ReadProcessMemory on our own process. So a search raises no exception
   in the game and lifts no guard page, whatever a pointer holds (`search_refused` counts what it left alone); the
   trial reads as v1 will.
4. **Two files in `%APPDATA%\EldenNet\probe`**, every 2 s: `lodestar-found.json` (kind `lodestar`, schema 2: the
   finder's numbers, `instances` and `trial`), and, when the names change, `lodestar-table.toml` (`draft_table`: the
   build's table as leads, each entry checked against the image; its `tool_sha256` is zeros, which the review
   replaces with the package's own digest of the DLL).

Numbers, fixed words and the catalogue's labels only (O75): no class's name is written. T1 stays the round's reporter
for the session. On the fake game under Wine (`launcher-eldennet-read`): a first pass before the managers exist names
none, a later one names all eleven, the trial reads them about 340 times in 34 s with no fault, and the fake hub sees
no task of ours.

## Tests

`en_shell_tests` (doctest; Linux with GCC and Clang, and the Windows builds under Wine): settings (the defaults, what
v0.1 reads, the later tables, each refusal without its value, the size cap, the probe table only in a probe build), the
log lines (scrubbing a path and a name, a cut line), the rings (order and capacity, C1's newest and gap, C1 under two
threads, the log's ring from four threads), the safety checks' names, every kind of recording line linting clean
alone and as a file with no name, text or param value in it, and an S0 run's report, ready or waiting, linting clean
with its words.

In the clang-cl build, `eldennet_dll_checks` (`tests/dll_checks.py`) runs ARCH §12.1's greps on `src/eldennet`
(DllMain's body is the load contract's and calls nothing else; no detach branch; every `WINAPI`, `CALLBACK` and
`extern "C"` body starts with `ffi_guard`, and is `noexcept`; no `reinterpret_cast` or `std::bit_cast`; no hook
library's or renderer backend's name; no `IsEnableOnlineMode`; `en_dll_module` on the DLL and `en_shell`), the same
greps but DllMain's on `src/en-overlay`, which the DLL links, and reads both DLLs with llvm-objdump:
`eldennet_instance_marker` their only export, and their imports within `tests/imports.allow` (kernel32 and user32: no
network, no hook API, nothing that reaches into another process; part 12 added graceui-sys's `GetTickCount64` and the
C runtime's `ReadConsoleW`, which Dear ImGui's number parsing brings, and spike.6 the probe build's own heap,
`HeapCreate`; the AI pages en-llm's threads, the C runtime's thread support with them, `GetSystemDirectoryW` for
WinHTTP's full path, and advapi32's token and access-list functions for secrets.toml).

End to end, the fake game's harness runs the DLL under Wine with the real launcher (`ctest -R
fakegame.launcher-eldennet`; `tests/fakegame/README.md`): injected suspended and at the title, a numbers-only recording
that lints clean, the session gate, a build the DLL's own gal/ does not know, a simulated anti-cheat module, a second
copy, a crash and a stack overflow recorded through the DLL's filter, the watcher started under another loader, the
read-only round from settings.toml, the same as the owner runs it (`--read`, no settings.toml: the console's steps,
both files checked by the launcher), and `--read` refused by the release DLL. Without GraceUI, each logs
`connect ok=0 outcome=absent installed=0 looks=1`: the look ends at once. With a test build of GraceUI (CMake's
`EN_GRACEUI_DIR`, the GraceUI folder GraceUI's build lays out), seven cases more (`fakegame.launcher-eldennet-graceui*`;
the spike run's and EldenNet's pages' beside these):
GraceUI loaded after EldenNet, by its own look, on the main path (the pre-flight, the connect, F11's view drawn on the
game's Direct3D 12 frame and hidden again, the world-state provider, and the Mark handed over to GraceUI's hotkey);
GraceUI loaded before it, on the banner path (the banner as one notice, F11's view); another mod holding Pause in
GraceUI, so EldenNet's Mark registers unbound and the launcher keeps Pause; the anti-cheat's notice-only mode; and
Augur's events from the fake game's world and the lifecycle listed in F11's view, their squares counted on the frame;
and the AI pages against a fake model server on loopback (`launcher-eldennet-graceui-ai`): a provider added, a key set,
the self-test from the browser and from the menu, the model list, a provider removed with its key, and the key in no
frame, log line, recording or file.
