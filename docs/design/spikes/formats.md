# The spikes' files, for DataPiler's importer (O73)

*2026-11-17. What the first in-game build writes on the owner's PC, and what eldennet-engine's tools make of it, as
a DataPiler job imports them (roadmap S7.11.f). Every file below is numbers, fixed words and our own labels: no
name, no path, no text of the game or the player, no SteamID, no key. The engine's `lint` checks the four kinds of
report and the recordings; DataPiler's guard checks the same keys and words against its own copy.*

## Where they are

All under `%APPDATA%\EldenNet\` (the launcher's `--appdata`, or `ELDENNET_APPDATA` for the DLL, stand in for
`%APPDATA%` in development):

| File | Written by | Form | Checked by |
|---|---|---|---|
| `recordings\<date>-<time>-<pid>-<n>.ensnap.jsonl` | eldennet.dll, with `debug.record_sessions = true` | JSON Lines, each line a report `recording`, schema 1 | `eldennet-engine lint --recording` |
| `spikes\<spike>-<YYYYMMDD-HHMMSS>.jsonl` | eldennet-launcher, with `--spike` | JSON Lines: a line per run, then the tally | none (the launcher's own JSON; below) |
| `spikes\<spike>-<n>\` (every spike's run since 2026-11-23) | the launcher makes it and names it to the game (`ELDENNET_SPIKE_DIR`) | below | below |
| `spikes\<spike>-<n>\graceui-spike.jsonl` (s0e, s0e_alone, s4) | GraceUI's observer | GraceUI's | GraceUI's reader |
| `spikes\<spike>-<n>\eldennet-spike.json` (s0a, s0b, s0c, s0d, s0d-crash) | eldennet.dll: its report of the run | report `spike_run`, schema 1 | `lint --report` |
| `spikes\<spike>-<n>\launcher-run.json` | eldennet-launcher: whether the tester's panel was shown | one JSON object (below) | none |
| `crashes\crash-<YYYYMMDD-HHMMSS>-<pid>.json` | the launcher's crash watcher (O71) | one JSON object | none (below) |
| `probe\r1-derive.json`, `probe\r1-probe.json` | eldennet.dll's probe build, in R1 | reports `derive` and `probe`, schema 1 | `lint --report` |
| what `--out` names (the S0 package's run files: `spikes\report-<spike>.json`) | `eldennet-engine rec-stats`, `replay`, `spike-report` | reports `rec_stats`, `replay` (schema 1), `spike` (schema 2) | `lint --report` |

A report is one JSON object whose first keys are `"report"` (its kind) and `"schema"`; every other key belongs to the
kind's closed vocabulary and holds one type (a whole number, signed or not; a boolean; a word from a fixed list; a
label `[a-z][a-z0-9_]*`; a byte pattern of at most 64 bytes; an object; an array of one type; in recordings only, a
finite number with at most 7 significant digits). There are no nulls: an absent value is an absent key. A new key or a
new word is a new schema, never a change within one (WHY§188).

*2026-11-20:* the spike words `s0e`, `s0e_alone` and `s4` and the banner reason `spike_run` (the runs of GraceUI's
observer, S0-E and S4) joined the recording's, `rec_stats`' and `spike`'s schema 1 before any reader outside the
engine held a copy of those words (DataPiler's guard holds `derive`'s alone); the rule holds from the first such copy.

## Recordings (`recording`, schema 1)

Plain JSON Lines (no zstd: plan §1.3), rotated past 256 MB into the next file `<n>`, flushed every second. Every line
is `{"report":"recording","schema":1,"t":<mono ms>,"k":<kind>,...}`, with `t` the recorder's QPC clock in
milliseconds, the same clock as the snapshots' `mono_ms` and the launcher's Marks.

| `k` | Keys after `t` and `k` |
|---|---|
| `header` (first line of each file) | `ensnap` 1, `game_version` [4], `en_version` [3], `region` (`ww`, `jp`, `other`, `none`), `spike` (`none`, `s0a`, `s0b`, `s0c`, `s0d`, `s0d-crash`, `s0e`, `s0e_alone`, `s4`), `probe_build`, `file` (from 1) |
| `snap` | `seq`, `mono_ms`, `gap`, `build`; `health`: `groups` [11 × `group`, `state` (`Untested`, `Ok`, `Tripped`), `phase_b`, `faults` this snapshot's], `task_us_last` (the snapshot's build), `task_us_p99` (the C13 histogram's p99 since the start); `session`: `role`, `is_my_world`, `lobby`, `player_set_len`, `anticheat`, `ersc_loaded`; `life`: `main_player`, `screen`, `load_counter`?; `ui`: `hud`, `banner`?, `popup_talk_id`?, `talk_cam`, `subtitles_on`; `player`? (`handle`, `level`, `archetype`, `body`, `hp`/`fp`/`stamina` {`cur`, `max`}, `runes_held`, `deaths`, `play_time_ms`, `ng_lvl`, `save_slot`, `dead`, `riding`, `locked_on`?, `last_hit_by`?, `throw_state`, `roundtable`, `statuses` [words], `block`, `pos` [3 floats], `gear` [6], `boss_flag`); `place`? {`place_id`?}; `clock`? [h, m]; `boss` [{`handle`, `name_id`, `damage_taken`}]; `chars` [{`handle`, `param`, `model`, `entity`, `kind`, `team`, `hp`, `dead`, `last_hit_by`?, `recv_dmg_type`, `throw_state`, `dist_m`?, `bearing_deg`?, `on_screen`, `statuses`, `block`, `pos`}]; `flags`? {`watch_version`, `set` [ids], `valid` [ids]}; `texts` [{`fmg`, `id`, `found`}]; `params` [{`param_kind`, `id`, `fields`}] |
| `life` | `life_kind` `data_ready` (`build`, `caps` [group words]), `playthrough_ready` (`slot`, `archetype`, `keepsake`, `play_time_ms`, `name_chars`: the name's length, never the name) or `group_tripped` (`group`) |
| `banner` | `reason` (`unknown_build`, `anticheat`, `not_started`, `discovery_failed`, `register_faulted`, `inert`, `session_failed`, `task_tripped`, `spike_run`) |
| `change` | `what` (`phase`, `gate_opened`, `gate_closed`, `level`, `banner`, `badge`), `from`, `to` (en-core's numbers: a phase or level, the gate's reason, a banner reason, the badge 1 up or 0 down), `mono_ms` |
| `cmd` | `cmd` (`noop`, `set_watch_flags`, `set_tracked`, `resolve_text`, `resolve_params`), then `version`, `n`, `fmg` or `param_kind` by kind: counts, never the ids' text |
| `mark` | `n`: the Mark's number (the launcher's Pause, O90) |

A `?` marks a key left out when the game has no value (`load_counter` without its item, `locked_on` with nothing
locked, a bearing without the camera). The order of lines is the core thread's: a banner's, a lifecycle message's or a
snapshot's line, then the lines of the changes and commands the core answered to it. A snapshot's line carries the
snapshot's own `mono_ms` as `t`; the others the core's step's time. About 3.3 KB a snapshot with six characters, so
about 47 MB an hour at 4 Hz.

## Spike logs (the launcher's JSON)

One line per run, then one tally. A run: `spike`, `run` (from 1), `mode` (`suspended`, `window`, `title`,
`vanilla`; `unknown` in logs older than 2026-11-17), `outcome` (`ready`, `banner`, `inert`, `exit`, `game_ended`,
`timeout`, `no_dll`, `start_failed`, `inject_failed`), `status` (EldenNet's last status: `init` as T1's state word,
`phase`, `level`, `banner`, `flags`, `items_resolved`, `items_total`, `discovery_passes`, `ready_ms`, `task_us_p99`,
`snapshots`, `faults`, `tripped`), `end` (`clean`, `exit`, `crash`, `closed`), `exit_code`, `record_kept`,
`crash_filter`, `run_ms`, `connections_max` (s0c: the game's connections beyond this PC, the most at once; no
address), `panel` (the tester's panel was shown); for a run with a folder, `folder` (its n), `report` (eldennet.dll's
report was read there), `waits` (what the start waited for, its word) and `table_ok` (S0-A's rule for its table). The
tally: `spike`, `tally` 1, `runs`, `ready`, `banner`, `inert`, `exit`, `game_ended`, `timeout`, `no_dll`,
`start_failed`, `inject_failed`, `crashes`, `records`, `filter_records`, `reports`, `waiting`, `table_failed`,
`passed` (the launcher's own rule).

`launcher-run.json`, in a run's folder: `launcher_run` 1, `spike`, `panel` (shown: a window always on top of the
game's, so that frame counts are read knowing it), `tasks`, `tasks_done`, `marks`.

## eldennet.dll's report of an S0 run (`spike_run`, schema 1)

`eldennet-spike.json` (src/eldennet/README.md; en_shell's `spikerun.hpp`), written by T1 once its start has stopped and
by T6 every 10 s after: `spike`, `pid` (the game's process id, which a recording's name holds), `probe_build`,
`game_version` [4], `region`, `en_version` [3], `state` (`none`, `ready`, `inert`, `banner`, `exit`), `banner` (`none`
or the recording's banner words), `waits` (`none`, `layout`, `files`, `build`, `anticheat`, `not_started`), `start_ms`,
`session_ms`, `writes`; `discovery` {`ran`, `ready`, `leads`, `passes`, `items`, `of`, `getter_calls`, `groups_on`};
`table` {`items`, `refused`, `rechecked`, `entries`, `again_refused`, `changed`, `witnesses`}; `trial` {`ran`,
`seh_forced`, `seh_caught`, `rpm_forced`, `rpm_caught`}; `inject` {`group` (a group's word or `none`), `every`,
`after_s`}; `reading` {`started`, `ms`, `snapshots`, `torn`, `held`, `unconfirmed`, `copy_faults`, `injected_faults`,
`tripped` [group words], `p99_us`}.

## Crash records (the watcher's JSON, O71)

`record` 1, `launcher` (its version label), `build` (builds.toml's key or `unknown`), `started`, `ended`
(YYYYMMDD-HHMMSS, local time), `mode`, `spike`, `kind` (`clean`, `exit`, `crash`, `closed`), `exit_code`,
`exit_code_hex`, `session_ms`, `dll_attached`, `status` (as above), `modules` (the loaded modules' file names, no
folder, ASCII `[A-Za-z0-9._+ -]`), `modules_at_crash`, `modules_age_ms`, `marks` (mono ms), `connections_max`,
`panel`, `explained`, and `crash` (`code`, `code_hex`, `thread` `game`/`init`/`other`/`unknown`, `chained`, `module` a file
name, `offset`) or null. The module names are the one free text: other programs' file names, which a guard that
accepts only words may refuse; the record stays on the PC unless the owner sends it, and nothing in it is personal.

## R1's reports

`r1-derive.json` is derive.json's schema 1 over the image as loaded (src/en-tools/README.md), which DataPiler
0.2.41's guard already reads. `r1-probe.json` (`probe`, schema 1; src/eldennet/README.md): `stage`, `elapsed_ms`,
`gate_passed`, `derive` {`passes`, `ms`, `written`}, `checks` [{`item` (`gate`, `fd4_idiom`, `name_layout`,
`register`, `register_abi`, `group`, `task_shape`, `singleton`), `label`?, `status` (`confirmed`, `differs`,
`missing`, `not_run`), `value`?, `expected`?}], `hinstance` {`slots`, `count`}, `register` {`call`, `callee`,
`registers`, `group_index`, `object_size`, `vtable_slots`}, `task` {`first_tick_ms`, `invocations`, `slot_calls`
[{`slot`, `calls`}], `object_written`, `object_nonzero`}, `census` {`done`, `copies`, `faults`, `objects`, `vtables`
[{`rva`, `count`}], `bases` [{`type_descriptor`, `found`, `derived`, `slots_min`, `slots_max`, `run_slot`?}]},
`rising` {`candidates`, `passes`, `hits` [{`slot`, `offset`, `rose`, `still`, `other`}]}, `statics` [{`label`,
`candidates`, `checked`, `at`}], `copy_faults`, `stack_refused`, `allocations`.

## The tools' reports (`rec_stats`, `replay`, `spike`; schema 1)

One key table serves the three kinds (src/en-tools/src/spike.cpp, src/en-tools/README.md):

- `rec_stats`: `header` {`game_version`, `en_version`, `region`, `spike`, `probe_build`, `files`}, `lines`, `kinds`
  [{`kind`, `count`}], `duration_ms`, `snapshots` {`count`, `gaps`, `late` (intervals over 1 s), `stalls` (over 5 s),
  `interval_max_ms`, `rate_mhz`, `chars_max`, `flag_frames`}, `task` {`build_us_p50`, `build_us_p99`, `build_us_max`,
  `p99_us_last`, `p99_us_max`}, `groups` [{`group`, `faults`, `tripped`, `ok`}], `faults`, `life` {`data_ready`,
  `playthrough_ready`, `group_tripped`}, `changes` [{`what`, `count`}], `gate` {`opened`, `closed`, `open_ms`},
  `banners` [{`reason`, `count`}], `commands`, `marks`.
- `replay`: `kinds` [{`kind` `snap`/`life`/`banner`, `count`}], `recorded`, `replayed`, `agree`, `first_difference`?,
  `phases` [{`phase`, `entered`}], `final` {`phase`, `level` (`starting`, `l1`, `l2`), `gate_open`, `playthroughs`}.
- `spike` (schema 2 since 2026-11-23): `spike`, `verdict` (`pass`, `fail`, `incomplete`), `waits` (as `spike_run`'s),
  `logs`, `recordings`, `run_reports`, `rules` [{`rule`, `mode`?,
  `status` (`pass`, `fail`, `not_measured`), `value`?, `limit`?}]; for s0a `modes` [{`mode`, `runs`, `ready`,
  `banner`, `inert`, `crashes`, `timeouts`, `other`, `items_min`, `items_max`, `items_total`, `passes` [5],
  `ready_ms_p50`, `ready_ms_max`}]; otherwise `sessions`, `connections_max`; for s0d `crashes`, `records`,
  `filter_records`; with recordings, `gate` and the first recording's `snapshots` and `task` as rec_stats has them.
  With runs' reports, `table_items`, `table_refused`, `table_changed`; for s0b with them, `reading_ms` and `steady`
  {`torn`, `held`, `unconfirmed`}; for s0d, `trial` {`forced`, `caught`}.
  The rules' words: `runs_per_mode`, `ready_or_l1`, `no_crash`, `both_modes`, `session_length`, `faults`, `no_trip`,
  `task_p99_us`, `gate_closed_unless_solo`, `connections`, `online_menu`, `faults_caught`, `breaker_tripped`,
  `crashes_recorded`, `recording`, `spike_log`, and since schema 2 `table_checked`, `table_unchanged`, `no_call`,
  `ready_start`.

## For the importer

- Lint first, with the engine of the same version (`lint --recording`, `lint --report`); DataPiler's guard adds the
  kinds `probe`, `recording`, `rec_stats`, `replay` and `spike` to its copy of the vocabularies as they are here.
- A recording's snapshots are keyed by (`file`, `seq`); `seq` is the task's and rises by one per snapshot built, so a
  step of more than one is a snapshot the core never took (C1's newest wins), not a lost line.
- Join a recording to its spike log by the header's `spike` and the session's time; join a Mark to the snapshot at or
  just before its `t`.
- Text and param rows stay out until ARCH §7.5's keyed hashes (S10.2.d); a later schema adds them, and adds zstd only
  once DataPiler's Python reads it.
