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
  recording vocabulary. R1's report, which the probe build fills, is en-gal's (`en/gal/probe_report.hpp`), so that
  the launcher lints it without en-core (ARCH §2.3).
- `eldennet.dll` (`win/`), the clang-cl build's alone (NativeSys needs structured exception handling), and, with the
  tests or the CMake option `EN_PROBE` on, the same DLL with the probe option, `probe/eldennet.dll`, which runs R1.
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
   `connect` as `eldennet`, with its floor (GraceUI 0.2.8) and the caps it uses, and, connected, the F11 view and the
   Mark's hotkey (below). One log line, `connect ok=… outcome=… how=… looks=… installed=… graceui=… view=… mark=…
   mark_bound=… world=…` (`view=unbound` when F11 was taken first, with a line `view hotkey=unbound` beside it); a
   failed connect (absent, too old, a duplicate id, inactive, refused) leaves EldenNet headless, nothing drawn (ARCH
   §10.3), and the start goes on. `overlay.connect = false` skips it (`connect skipped=1 why=off`).
9. **Discover**: patterns.toml and the layout (en-gal's `discover_with_retries`: at once, then +5, +15, +30 s).
10. **WaitEngine**: the task hub's instance non-null, at most 120 s from T1's start. Then the **crash filter** is
    installed: after the game's C runtime has installed its own, which replaced an earlier one (WHY§172).
11. **Register**: the stack table made again without T1's own stack (WHY§190), then the one call into the game. In a
    run of GraceUI's observer (the launcher's `--spike s0e` or `s4`, S0-E's present-hook half and S4, read from the
    watch block; en-proto's `observer_run`), T1 stops at the start of Register instead, before the task is made or
    anything called: `register skipped=1 why=spike spike=<word>`, then the `spike_run` banner ("Spike run: EldenNet
    makes no call into the game", GraceUI's notice when connected) and L1. What a stopped start keeps runs on: T6
    (the core, the status block, the status lines), T10, T11, the connect and F11's view (`debug.panel`), the recorder
    (its header carries the word). Nothing waits for the task, which T6 and the view read only once it is set, and the
    crash filter stays in. (`s0e_alone` loads no eldennet.dll at all.)
12. **FirstTick**: at most 30 s for the task to pass the group check and phase A; else Inert and its banner.
13. **Ready**: DataReady went to the core on C2; another module exporting `eldennet_instance_marker` is logged as a
    second copy that idles (the toast comes with GraceUI, ARCH §1.6). T1 ends.

A probe build with `probe = { round = "r1" }`, or asked by the launcher's `--probe`, runs R1 after Connect instead
(below), but never in a run of GraceUI's observer: there settings.toml's round is set aside
(`probe round=r1 skipped=1 why=spike`) and the start stops at Register, and the launcher refuses `--probe` with those
spikes.

## The threads and channels (ARCH §1.2, §1.3)

- **T2, the game thread**, runs the task (en-gal's `GalTask` behind a body that names the thread for the crash
  filter, and in a probe build arms the allocation watch and S0-D's fault injection) through `Io`, the task's TaskIo:
  C1 is a `SnapSlot` (two preallocated snapshots; the newest wins and an overwrite marks the next `gap`), C2, C4 and C5
  are SPSC rings of 64. Nothing on it waits or allocates (R6); the probe build counts any allocation there.
- **T6, the core**, waits on one wake signal (or 100 ms), then drains T1's banners, C2's lifecycle messages and C1's
  newest snapshot, in that order, and steps en-core's slice on each and on a 100 ms tick. Once T1 has connected to
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
  logged as not taken; the launcher's Pause stays the Mark whenever GraceUI is absent, so R1's flow is unchanged. When
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
  may get on its row and the home network's never; else cloud with its key in `secrets:<id>`), a job's model (`what=model|model_reset`: its `[slots.<slot>]` provider and model, or the table
  gone), a key on a row (`what=key name=… set=…`: cleaned, the request's copy overwritten; a provider with no key's
  place gets `secrets:<id>` first, `what=key_place`; never one whose address is http:// to another computer or whose
  key is an environment variable's), Remove on a row (`what=remove`: only a provider no job uses, with the keys no
  remaining provider names), each checked by parsing the result first and then posted to T11, which makes the same change on the file as it is then
  (`ai_write file=providers|secrets ok=… why=…`; secrets.toml written with an access list for this user alone,
  `write_private`). The self-test and the model list run on en-llm's handle (T7–T8), which T6 makes when one first
  needs it (`ai_service ok=1 workers=…`) and tells the core's gate at each wake; its notify sets T6's wake. Each run's
  end is one line of words and numbers (`selftest result=passed verdict=quick … first_token_ms=… tps=…`,
  `models_list providers=… passed=…`). en-llm's own log lines are not taken (a server's message could reach them).
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
probe = { round = "r1", register = "mark", object_size = 0x800, settle_s = 10,
          fault_group = "camera", fault_every = 0, fault_after_s = 2 }   # a probe build's only (EN_PROBE)
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

## R1, the probe round (O89; a probe build with `probe = { round = "r1" }`, or the launcher's `--probe`)

**How the owner starts it.** `eldennet-launcher --probe` writes R1 into the watch block's `control.probe` before it
loads anything, and EldenNet answers there at Settings: a probe build accepts (`probe.round` becomes `r1`, with the
file's other probe settings, and `register = "auto"` becomes `"mark"`, so the call waits for the owner's Pause); a
release build, or EldenNet turned off, refuses and stops at once (T1 ends in Exit: no other start, nothing called).
The log's `probe asked=r1 accepted=…` line says which. settings.toml's `probe` table still works alone (the tests).

What the owner's offline round R0c left for the game (its report: the FD4 classes unnamed, `register_task` and its ABI,
the task's base class, run slot and object size, `GameDataMan`'s and `global_hinstance`'s slots, the group index, the
layout fields). On T1, after the window and `settle_s` seconds:

1. The **derivation over the live image** (en-gal's `derive`, `ImageLayout::Memory`, with `gal\leads.toml`): the FD4
   singletons named through their class objects as the game has filled them (`dlrf_name_layout`), `register_task` and
   its ABI from the hub's static and the game's own call sites, the group table as loaded, the RTTI sets. At once, then
   +15, +30 and +60 s while the registration's instance is unnamed. Written as `probe\r1-derive.json`, derive.json's
   schema 1, which DataPiler 0.2.41's guard already reads. `global_hinstance`: the data slots holding the module's
   base.
2. **The checks** before the one call: the gate passed; the FD4 idiom the live image gives the one R0 cut from the
   file, byte for byte and capture for capture (`gal\patterns.toml`'s `fd4_idiom`, reviewed: check `fd4_idiom`, its
   value the live hits and its expected R0's count, which does not decide); the name layout read by at least half the
   class objects; the registration's callee a function with three distinct agreeing argument registers; the group's
   index found in the loaded table and equal to the gated build's layout's (`[task] group`'s index, read as a dev
   build reads the file: a lead `[task]` may hold only its group); the hub's instance non-null. Any check that fails,
   a missing patterns.toml idiom or an unreadable layout included: no call, `stage = "refused"`. Nothing is guessed,
   and nothing rests on a derivation that was not reviewed (O89 (a)). Then the **callee's summary** (spike.6,
   WHY§227), from the live image, the code the call runs: decoded with en-gal's decoder from its RVA within its
   function of the exception directory (or up to its first return), at most 4 KiB, following no call; its instructions
   and bytes, every call with its target RVA or its form (through a register, through memory with its base, or through
   a RIP-relative slot, and whether the base holds an argument's object's first word: a virtual call on it), the
   conditional branches before the first call, whether some path returns through no call, and for rcx, rdx, r8 and r9
   (followed through moves between registers, 64- and 32-bit) their stores to memory before the first call (base
   register, which argument the base holds, displacement, width), the callee-saved registers they were moved to, and
   their compares with an immediate (the value kept from 0 to 65,536), with memory or another register, and tests.
   An instruction whose effect on the registers it does not model ends the tracking, and the report says where
   (`modelled_to`). `r1-probe.json`'s `callee_code` holds it (`register.callee` already holds the RVA). Since spike.7
   (WHY§235: run 6's callee was a 93-byte wrapper of one branch and two calls) the summary knows what each register
   holds along the flow (where two paths meet a register keeps only what both agree on), and gives at every call and
   tail jump which of the callee's arguments rcx, rdx, r8 and r9 hold (`pass`), and for each branch before the first
   call the compare or test that set its flags, with its operands' kinds and its condition (`guard`); then each callee
   it calls directly or tail-jumps to (up to eight) is summarised the same way with those arguments followed in
   (`callee_code.inner`: their `args` are the wrapper's). Since spike.8 (WHY§240: run 7's wrapper tail-jumps to a
   function that bounds the group by 168, tests a register loaded from none of its arguments and leaves through a jump
   through memory) a register also holds a value loaded from memory (a RIP-relative slot, or [base + index*scale +
   disp] with what base and index hold, at most 3 loads deep), a call's result stored to a RIP-relative slot holds that
   slot's value, an inner summary's rcx to r9 that hold no argument hold their entry values, and each guard operand that
   holds no argument gives its `source`; a jump through memory or a register is a tail call site (`tail_memory`,
   `tail_register`, with the base's source and `vtable_src`); every call site gives the known values in rcx to r9 that
   are no argument (`pass_src`); with no extent a function ends at a jump out that no branch goes past; and the first 64
   instructions of each summary are given as `forms` (offset, length, class and each operand's kind: a register with
   its width and the argument it holds, memory, a RIP-relative slot with its section, an immediate up to 65,536 or
   `large`; those no known path reaches `unreached`), no bytes. The tail function's **table** is matched against run
   7's reading (`table`: the wrapper's rcx loaded from a slot S, directly or through one load, or, where the paths into
   its tail jump disagree on rcx, so loaded and null-tested on the path past the test, `via_guard`; and the tail
   function's null-tested value loaded from [entry rcx + group*8 + disp], or one level deeper; else `matched: false`
   and the first shape that failed). At the Mark T1 walks the hub's reachable memory as the hub search does (the control walk), waits
   2 s and walks it again, keeping the blocks' bytes on our heap (within the walk's 6 MiB) for the diffs below, reads
   the table (`table.before`: the singleton's kind and whether it is CSTask's instance or a named singleton's, the
   entries from 0 to the bound that are set, the group's entry, its first word and the word its final jump reads, a
   game function there summarised one level deeper, `depth` 2), and makes the call at once; then it compares the two
   walks (`hub_diff_control`) and lets the first go. Since spike.9 (WHY§248: run 8's tail function moves the task into
   rdx and jumps directly to a function of the group's own, with the entry in rcx) a direct jump past a function's
   decoded code is a tail jump, the table's `target` is that jump's, and **the group's function** is summarised from
   the image at once (`depth` 2, `parent` the tail function, `entry_src` the jump's known values, rcx `holds_entry`),
   with its own direct callees and tail jumps one level further (`depth` 3), 16 summaries at most; in every summary each
   load from our task (the registers' first) or from the group's entry, or from a value loaded from either, is listed
   (`task_reads`, `entry_reads`: offset, field offset, width, the base's source, and what the value is then used for:
   compared with an immediate or otherwise, tested, stored with its base and displacement, passed to a call, called
   through, used as a base, changed), and each store of the task, of a value read from it, or of either changed (a
   tagged pointer: Source's `modified`) is listed with its base's source and displacement (`task_stores`). The walk
   and its diffs start from two more roots, read from the table at the Mark: the singleton and the group's entry (each
   diff's `root`: task_hub, singleton, group_entry; their controls in `hub_diff_controls`), each root within the same
   bound of three walks; and just before the call the singleton's and the entry's **classes** are read through their
   first words by the RTTI walk and matched to the leads' names as O92 does (`singleton_class`, `entry_class`: the
   lead's index where the innermost name is the lead's or it with Imp; numbers only). Since spike.10 (WHY§289: run 9's
   group function tail-jumps through an 8-byte stub that zeroes r9 to **a method of a second singleton G**, with G,
   loaded from a data slot, in rcx, our task in rdx and a 32-bit word of the entry in r8) the site in the group's
   function that reaches G's method gives its entry state (`holds_g`, `holds_word` in `entry_src`); a stub (16 bytes
   or fewer ending in a direct tail jump) is followed without spending a depth level, so G's method is `depth` 3
   (`stub`, `zeroed`), and its direct callees and tail jumps `depth` 4, within the 16 summaries; each summary lists the
   loads based on G (`g_reads`) and the uses of the entry's word (`word_uses`: compared with an immediate or otherwise,
   an index, passed, stored). With no extent, a direct jump to a function's start of the exception directory ends the
   decoding (a stub's forward tail jump). Statically, before the call, the game's own call sites of the registration
   (derive's) are read for what they pass as the task (`game_sites`: a member with its base and displacement, a static,
   a load, an allocator's result with its callee, the function's own argument, a stack slot, or other), and the
   registration's sibling of the same shape (derive's next such candidate) is summarised with its callees breadth first
   to depth 4 (`sibling`, 12 summaries at most). At the Mark G is read through its slot (`method`: null or heap, its
   class against the leads, whether CSTask's instance holds it at +8 and the group's entry at +0x58, where run 9's
   walks met one object, and every table entry's word at the site's offset: group 110's, how many hold their own index,
   how many distinct values), again just before the call and 2 s after it; and G is the walk's fourth root
   (`g_singleton`, with its control) unless it is a root's object or held there (`g_walk`). Since spike.11 (WHY§293:
   run 10's G method builds a 32-byte **request** on its stack and either calls a function with it at once or copies
   it to the heap for G's queue) every summary with an extent is decoded **in parts** where its code goes past it
   (`parts`: the primary entry, the exception directory's entries chained to it, and the code a conditional branch
   past the extent reaches in no entry, `unlisted`); the request is read from G's method (`request`: its vtable in
   rdata, its stack displacement, each field's offset, width, source and role: vtable, task, manager (G's field),
   word, zero; the function called with its address, `now`; its class by RTTI and its slots, the vtable's words into
   the game's code); its slots' functions (the first six) are summarised at depth 4 with the request in rcx
   (`request_slot`, `holds_request`), the request's fields followed (`request_reads`; a load of its task field is the
   task, of its manager field G's manager, `manager_reads`; of its word the entry's word), the function G's method
   calls with it too, each with its direct callees and tail jumps one level further (depth 5), 24 summaries at most.
   The fields of G that G's method reads (by offset and width from its summary) are read just before the call and
   about 50, 250 and 2,000 ms after it (`method.g_fields`: a narrow field's value, an 8-byte one's kind and a
   pointer's class), the byte the method compares first deciding the path (`defer_disp`, `path_taken`: now or
   queued); and G's queue (the G field the method passes in rcx to the queued path's call, `queue.g_disp`) is the
   walk's fifth root (`g_queue`, with its control) and is walked at the same four times (`queue.reads`: the blocks
   whose first word is the request's vtable, each read as a request: its task ours or its kind and class, its manager
   field G's or not, its word, its zeroed byte). Every entry's word is also read for its own index in its low 16 bits
   (`own_low`) and its high 16 bits' values (`highs`). Since spike.12 (WHY§296: run 11's now path's function looks the
   word up in a tree from the manager's +0x18, or with a word of 0 walks a list at its +0x30 and +0x38) the **manager's
   map**'s layout is derived from that function's summary (`map`: the head's displacement in the manager, the root's in
   the head, the byte compared with 0 at each node, the key compared with the word and its width, the value passed on a
   match, from the call's rcx where the summary knows it, `value_known`, else 8 past the key; the children at +0 and
   +16, MSVC's node; the list's begin and end; the function a match calls, `map_call`, and the one called before the task's
   slot on the other branch, `other_call`; or the first part not found, `shape_failed`), and the two functions are
   summarised at depth 5 (the match's call with the map's value in rcx, `holds_value`, its loads `value_reads`, the
   task, the word and the manager) with their callees one level further (6), before the slots' level, within the 24.
   A compare's memory operand is kept where a followed value is compared with it (the key, `compared_other` with its
   displacement and width).
3. **The call**, at the first Mark (the launcher's Pause, pressed at the title with no save loaded: O89, O90;
   `register = "auto"` calls at once, `"never"` not at all): our task object of `object_size` zeroed bytes and a fake
   vtable of 64 guarded slots (or the task classes' count, where the leads name a base), each counting its calls and
   running the probe's body for our object (TaskShape's `every_slot`), in the derived registers and group. The status
   block's `ready` is "task confirmed" once the game has called a slot. Since spike.8 rax as the call left it is kept,
   as a kind (`register.returned`: zero, small with its value, ours, the game's, heap, our task's block, other) with its
   low byte (`returned_low`).
4. **On the game thread**, within the reader's budget: the slot counts (the run slot is the one called every frame);
   how much of our object the hub wrote (one past its last non-zero byte); the **hub census** (the vtables of the objects
   the hub's instance points at, to a depth of three, the image's writable data included); the **rising-counter search**
   over the heap statics (a field rising by the clock and standing still at times: the play time's static, which has
   no RTTI); and each RTTI-classed static's slot.
   On T1, 2 and 20 s after the call: the **hub search** (spike.5, WHY§209: the hub's reachable memory, three pointers
   deep, `hub_search`), the **hub diff** (spike.7, WHY§235, `hub_diff`: that walk compared with the one kept from before
   the call, block by block by path: the blocks compared, re-pointed, gone and new, the 8-byte words that changed, and
   for the first 32 of them their path and the old and new value's kind: zero, a small integer up to 65,536 with its
   value, the group's index, our task object, our vtable, our module, the game's image with its section's kind and RVA,
   the heap, or other; a new pointer's block fresh when the walk before the call never reached it, a word whose 32-bit
   half is the group's index marked; at 20 s each word steady when it held the same new value at 2 s, the steady ones
   listed first; since spike.8 each word `in_control` when its path's word changed in the control diff too, those not
   in the control listed before those in it; since spike.9 a word whose low 48 bits are our object's address, or point
   inside it, under a non-zero tag is `task_tagged` with its high 16 bits), at 2 s the table read again
   (`table.after`), and since spike.10 G and the entries' words (`method.after`) and the array's views (below), since
   spike.11 G's fields and its queue, and the **whole-memory
   search** (spike.6, WHY§227, `memory_search`): every region VirtualQuery calls
   committed and readable (not no-access, a guard page, uncached or write-combined), never a known thread stack nor
   T1's own, read through the crash-guarded copy in chunks of 16 KiB (page by page where one faults or a stack overlaps
   it), within 64 GiB and 30 s since spike.7 (run 6's 8 GiB cap left 2 of 10.6 GiB unread; `stopped` says which cap, if
   any, ended it, and `committed_bytes` the process's committed total, past a cap too), for the 8-aligned words equal to
   our task object's address and those pointing inside it, and since spike.7 those equal to our fake vtable's address
   (our object's first word, read back after the call) and to one of its slots' (our module's thunks), each counted
   apart, with how many different slots the game's words name, and since spike.9 the words whose low 48 bits equal our
   object's or our vtable's address while their high 16 bits are not zero (`tagged_found`, `vtable_tagged_found`,
   ours apart; `tagged_hits`, each with its `tag`), and since spike.11 the words equal to the request's vtable
   (`request_found`, ours apart: the game's requests; `request_hits`, 32 at most, each read by T1 as a request, and
   `request_tasks`: for each distinct class of their tasks, 8 at most, its vtable's slots and the first 16 words of one
   such task as kinds, a pointer's with its class). After each search the classes met are given with their RTTI bases
   (`classes`: G's manager, the request, CSTask's instance, the group's entry, G, the game's tasks; each base count
   and its bases' type descriptors; `shared_bases`, those met in two or more). Each side keeps up to eight hits of each kind, each
   giving what it points at, its region's kind (private, mapped, image) and protection, an image's module (ours, the
   game's or another, by kind, never by name), its section (by its characteristics) and RVA, and the 32-bit words equal
   to the group's index within 64 bytes. **Our own copies are counted apart** (`own_found`, `own_inside`, hits with
   `own`): a region is ours when its allocation base is our module's, our task's block's or our vtable's
   (VirtualAlloc's), or one of the DLL's own heap's. The probe DLL's operator new allocates from a heap of its own
   (`HeapCreate`, `alloc_count.cpp`), so everything the probe keeps (its census, its copies, its state) lies there,
   and the search walks that heap (`HeapWalk`) for its regions' and large blocks' allocation bases; `own_known` is
   false when the heap could not be made or walked, and then the heap's copies count as the game's. While the task
   has not run, T1 also reads how much of our object the hub wrote (`object_written`). Since spike.10 (WHY§289) the
   four views of the array behind CSTask's +0x3B0 and +0x3D0 (run 9's paths, four sibling pointers 32 bytes apart at
   depth 3 of the walk kept before the call) are read again at 2 s (since spike.11 only then; spike.10 read them at
   about 50 and 250 ms too) (`array_views`: each read's changed words, the first eight with their kinds, a new value in the
   game's rdata classed by the RTTI walk, `new_class`, and whether a changed word's 32-bit half is the main window's
   thread id, `main_thread`, or T1's, `caller_thread`).
   Since spike.12 (WHY§296) the manager's map and list are read just before the call, at about 50 and 250 ms and 2 s
   after it, and then every 10 s for as long as the game runs (`map.readings`: the first, each reading whose node
   count, the presence of the group's word or the stage changes, 24 at most, each noted in eldennet.log as
   `stage=map_change`, and the newest; `readings_made`, `changes`). Each reading walks the tree as the manager holds
   it then, checking before it reads on: the head on the heap with its nil byte not zero, each node on the heap with
   its nil byte zero, none met twice, at most 4,096 (else `map_failed`: manager, head, head_nil, pointer, unread,
   node_nil, cycle, too_many, and nothing more of it is read); it gives the manager's word beside the head as a kind
   (`count_word`), the nodes, whether their keys rose in order and the root's parent and the head's children are
   MSVC's, the keys' low 16 bits in order (256 at most) and their high halves counted, whether the group's word is
   among them (`word_present`) or else the nearest keys below and above it, the values' kinds and classes counted;
   and the list's count and its entries' kinds and classes (`list_failed` where it cannot be read). The **stage** of
   each reading (`world_stage`) comes from no world snapshot, which R1's task never builds: world while a singleton's
   instance holds, in its first 128 KiB, a pointer to an object of the leads' vtable sets (the characters' classes),
   title when none does (loading screens too), unknown when nothing could be read. After the 2 s search, while our
   task has not run and the search, complete, found no copy of it (plain or tagged, its vtable's or a slot's), R1
   answers the launcher `absent` (`stage=task_search found=0`), and the console's no-confirmation line then has the
   owner load the save and stay two minutes in the world. Past 256 KiB pretty-printed the report is written on one
   line before anything is cut, and the now path's function's forms and the match's call's go last of all.
5. Every 2 s, `probe\r1-probe.json` (kind `probe`, schema 1; `lint --report` checks it): the checks, the call, the
   counts, the census's vtables and, once it is done, the base classes the hub's objects share, each with its derived
   classes' slot counts and run slot (derive's `task_from_base`), the rising hits and the statics' slots. Once statics'
   slots are known (one rising static that also stood still; one RTTI slot each), the derivation runs again with them,
   so that r1-derive.json carries a pattern at each.

Both files are numbers and the leads' labels only (O75). T1 stays the probe's reporter for the session. The layout's
fields (ARCH §6.10's invariants over objects) wait for the reviewer's next round: the probe's object searches
(`vtable_offsets`, `meter_offsets`, `utf16_offsets`, `u32_offsets`) need the classes' instances that r1-derive.json
gives.

## Tests

`en_shell_tests` (doctest; Linux with GCC and Clang, and the Windows builds under Wine): settings (the defaults, what
v0.1 reads, the later tables, each refusal without its value, the size cap, the probe table only in a probe build), the
log lines (scrubbing a path and a name, a cut line), the rings (order and capacity, C1's newest and gap, C1 under two
threads, the log's ring from four threads), the safety checks' names, and every kind of recording line linting clean
alone and as a file with no name, text or param value in it. R1's report is tested in `en_gal_tests`.

In the clang-cl build, `eldennet_dll_checks` (`tests/dll_checks.py`) runs ARCH §12.1's greps on `src/eldennet`
(DllMain's body is the load contract's and calls nothing else; no detach branch; every `WINAPI`, `CALLBACK` and
`extern "C"` body starts with `ffi_guard`, and is `noexcept`; no `reinterpret_cast` or `std::bit_cast`; no hook
library's or renderer backend's name; no `IsEnableOnlineMode`; `en_dll_module` on the DLL and `en_shell`), the same
greps but DllMain's on `src/en-overlay`, which the DLL links, and reads both DLLs with llvm-objdump:
`eldennet_instance_marker` their only export, and their imports within `tests/imports.allow` (kernel32 and user32: no
network, no hook API, nothing that reaches into another process; part 12 added graceui-sys's `GetTickCount64` and the
C runtime's `ReadConsoleW`, which Dear ImGui's number parsing brings, and spike.6 the probe build's own heap and its
walk, `HeapCreate`, `HeapLock`, `HeapWalk` and `HeapUnlock`; the AI pages en-llm's threads, the C runtime's thread
support with them, `GetSystemDirectoryW` for WinHTTP's full path, and advapi32's token and access-list functions for
secrets.toml).

End to end, the fake game's harness runs the DLL under Wine with the real launcher (`ctest -R
fakegame.launcher-eldennet`; `tests/fakegame/README.md`): injected suspended and at the title, a numbers-only recording
that lints clean, the session gate, a build the DLL's own gal/ does not know, a simulated anti-cheat module, a second
copy, a crash and a stack overflow recorded through the DLL's filter, the watcher started under another loader, R1
from settings.toml, R1 as the owner runs it (`--probe`, no settings.toml: the console's steps, Pause pressed until
taken, both reports linted by the launcher), and `--probe` refused by the release DLL. Without GraceUI, each logs
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
