# en-tools: eldennet-engine

The command-line host that runs EldenNet's engine outside the game (ARCH §12.3), C++20, MIT. One program,
`eldennet-engine`, with a subcommand each: `render-prompt`, `chat`, `decide`, `probe`, `models`, the devkit's
`probe-exe` and `lint`, the spikes' `rec-stats`, `replay` and `spike-report`, and Augur's `golden` (this file's subject). `eldennet-engine <command> --help` lists a command's options.

## probe-exe: the derivation's offline round (R0, O89)

```text
eldennet-engine probe-exe <exe> --derive --leads <leads.toml> --out <derive.json> [--static <label>=<rva>]...
eldennet-engine probe-exe <exe> --sections
```

It reads the game's executable **file**, and nothing else: never a running process and never the game's memory (O73 to
O75); run it with the game closed. It derives what the game access layer needs (en-gal's `derive.hpp`) and writes
`derive.json`, through the numbers-only writer, after the lint has passed it: numbers, fixed words, the leads' labels and
patterns of at most 64 bytes with their wildcards. No path, no name or text read from the file, and no code beyond those
patterns is in it.

- `--leads FILE` says what to look for, by name and shape, never by bytes (ARCH §6.3: what a lead may carry): the
  product name, the FD4 path's suffix, the singletons' names, which singleton's instance the registration takes, the
  task group's name, the task base class, the classes of the vtable sets, and the AOB statics (each a class, or the
  module's base). Labels are its author's own, `[a-z][a-z0-9_]*`; only they reach the report. At most 64 KiB.
- `--out FILE` is replaced if it exists (written beside it as `FILE.part`, then renamed).
- `--static LABEL=RVA` (repeatable; hexadecimal `0x…` or decimal) gives a static's slot that the in-game round R1 found,
  so that a pattern is cut at a reference to it. Without it the static is reported as `needs_slot`, with its class's
  vtable for R1's search.
- `--help` prints the options, the exit codes and the devkit's disclosure of safe use and of what we condemn (proposal
  0002 §2.4, a draft for the owner to amend).
- `--sections`, alone, is the diagnostic for an executable that is refused: it prints the section table's numbers as
  the table gives them, read with only the checks a safe read needs (en-gal's `read_section_table`), and nothing else:

  ```text
  probe-exe: image size 0x1d000 file size 0x1c00 section alignment 0x1000
  probe-exe: section 0 va 0x1000 vsize 0x8000 raw 0x400 rawsize 0x600 flags 0x60000020
  ```

  `va` is the section's VirtualAddress, `vsize` its VirtualSize, `raw` its PointerToRawData, `rawsize` its SizeOfRawData
  and `flags` its Characteristics, by its index in the table; the image size is SizeOfImage. Numbers and fixed words
  only, as `lint --report` asks of a report: no name, no byte of the file. Exit codes 0 (printed), 2 (usage: with any
  other option) and 3 (the headers or the table cannot be read).

**The game's leads** are `gal/leads.toml`: only what the design names, each with its section (ARCH §6.1 to §6.3).
The design names no task base class, so its `[task] base` holds a marker of ours that matches nothing, and R0 reports
the task `missing` for R1 to find; GameMan and FieldArea are left out until a design document gives their classes.
**The R0 package** for the owner, `EldenNet-devkit-R0.zip`, holds `eldennet-engine.exe` from the clang-cl Release
build, `gal/leads.toml`, `README-R0.txt` (what the tool reads, writes and never does, and the disclosure above),
`LICENSE.txt`, `THIRD_PARTY.md`, `THIRD-PARTY-NOTICES.txt` (the three libraries' licences) and `SHA256SUMS`, which
DataPiler 0.2.41's `probe_exe` step checks the tool and the leads file against before it runs them.
`EldenNet-devkit-R0b.zip` is the same package with the reader that keeps only the loader's rules on sections, and
`--sections` (README-R0.txt says how to run it); derive.json's format is unchanged. `EldenNet-devkit-R0c.zip`, for the
second offline run, carries the kit R0's review asked for (the FD4 finder's path search and its decoding of each hit,
an idiom's 65,536 hits, tables of wider entries, the product name without symbols, and the diagnostics below) in the
same folder, `EldenNet-devkit-R0`, with the same command lines, so DataPiler 0.2.41's job runs it unchanged.

**For DataPiler's job (roadmap S7.11.h).** Exit codes:

| Code | Meaning | derive.json |
|---|---|---|
| 0 | Everything the round derives offline was derived (`"complete": true`) | written |
| 1 | Something was not derived: each item's `status` says what (`missing`, `ambiguous`, `refused`, `not_unique`, `depends`) | written |
| 2 | A usage error (an unknown option, a missing `--derive`, `--leads` or `--out`, a `--static` that names no static of the leads or is no RVA) | not written |
| 3 | The executable cannot be read, is larger than 1 GiB, or is not a PE32+ image for x64 | not written |
| 4 | The leads file cannot be read, is larger than 64 KiB, or was refused (each problem on a line: `leads: <where>: <problem>`) | not written |
| 5 | The report was refused by its own writer or by the lint: a defect of ours, to report | not written |
| 6 | derive.json cannot be written | not written |

Messages go to standard error, one line each, starting `probe-exe: `; none shows a path or anything read from the
executable (a malformed image's message gives the problem and an offset; a leads file's, the key where it lies). A
section the reader refuses is named by the rule it breaks, its index and its numbers, in `--sections`' words, in a line
of at most 251 characters with no path sign, so that DataPiler's log shows it whole:
`probe-exe: the executable is not a PE32+ image for x64: a section is misplaced: section 1 starts below the end of
section 0, 0x9000; va 0x1000 vsize 0x8000 raw 0xa00 rawsize 0x600 flags 0x40000040 (at 0x1b0)`. The rules are the
loader's own (en-gal's README): virtual ranges aligned, ascending, disjoint and within SizeOfImage as the loader rounds
it, and no raw range that wraps; raw ranges in any order, overlapping, shared, empty or past the end of the file are
read as the loader maps them. On success one summary line goes to standard output:
`probe-exe: derive.json written (complete|incomplete): fd4 <status>, <n> of <m> singletons; registration <status>;
task <status>; group <status>`. It holds the file and a copy of its sections in memory (about twice the file's size),
and decodes `.text` a few times over: here about 3.5 million instructions a second, a third of a second for 3.5 MB of
code; with `--static`, each static adds up to 16 passes over `.text`.

**derive.json** (`"report": "derive"`, `"schema": 1`): `complete`; `image` (the fingerprint: file size, image base,
TimeDateStamp, SizeOfImage, FileVersion and ProductVersion, `region` ww, jp or other, `product_name` matches (once
symbols are removed), differs, absent or not_given, the translation, the section table with the usual names or `other`);
`fd4` (the path strings and their references, the idiom's pattern, captures and hits, part 2's scan's verdict on it, the
name layout, the hits accepted and refused by reason, and each singleton by label with its static, class object, getter
and sites); `rtti` (the locators and vtables found, each vtable set's primary and subobject vtables, the task classes'
vtables, slots and run slot); `register` (the candidates, best first, each with its callee, call sites, argument
registers and whether they agree, and the first one's pattern); `group` (the index candidates and where they came from);
`statics` (each static's class vtable, slot, references and pattern); `confirm` (what R1 must confirm, as fixed items
with a label or a number); `stats` (instructions decoded, bytes that did not decode, base relocations). Every pattern
carries its length, fixed bytes, where it hit, its function's entry and length, its anchor (at least 5), its matches in
`.text` (1) and whether part 2's scan resolved it.

**What R0c changed in derive.json, within schema 1.** DataPiler 0.2.41's guard checks a report against its own copy of
schema 1's vocabulary, every key and every word: a new key, or a new word such as `matches_without_symbols`, would make
it refuse the whole report. So R0c adds no key and no word. `product_name` is `matches` when the names agree once
symbols are removed (the gate compares so too), and every diagnostic below uses schema 1's keys, nested where they say
what they count. A new key waits for a schema 2 and a DataPiler that reads it.

| Where | What it holds |
|---|---|
| `image.names` | `of`: the characters of the version resource's ProductName; `agree`: those left once symbols are removed. Fewer means it matched without them |
| `fd4.refused` | R0c's finder, by reason: `unreadable` now also counts a hit that does not decode as the idiom (its static, class object or path not a RIP-relative operand, its getter no relative call) |
| `group` | `references`: instructions addressing the name; `matches`: 8-aligned pointers to it. Each candidate: `length` the stride of its entries (8 for plain pointers), `entries`, `index` (its place), `offset` how far before the name pointer code addresses the entry (its start; 0 where code addresses the name field itself), `encoding`, `value` the index field its entries hold, if one holds its place plus a constant. Tables of two entries or more come first, then code, then lone pointers (`entries` 1), which give no index |
| `statics[].matches` | Type descriptors of a class or struct of the class's own name, in any namespace (0: no RTTI by that name anywhere) |
| `stats.sections` | Every executable section, decoded through as `.text` is: `section`, `rva`, `virtual_size`, `instructions`, `undecodable` (bytes stepped over), `entries` (runs of them), `hits` (runs right after an instruction that does not fall through: data after code), `matches` (runs whose first byte is a VEX, EVEX or XOP escape: C4, C5, 62, 8F), `functions` (undecodable bytes inside functions of the exception directory), `path_references` (instructions addressing an FD4 path string) |
| `stats.fd4.candidates` | Per reference to a path, what the site search found, as `index` and `references` (the count): 0 a site; 1 decoding from the function's entry does not land on the reference; 2 a head whose jump does not pass it; 3 no head but a compare of a RIP-relative quadword with zero before it; 4 no head but a 64-bit load of a `.data` static before it; 5 none of these within 24 instructions before it |
| `stats.fd4.sets` | The head shapes, largest first (8 at most): `index` (rank), `sites`, `sites_matched` (of them, matched by the idiom) |
| `stats.fd4.captures` | Where the idiom's sites load their paths, as their references show (16 largest buckets): `capture` path, `at` (the displacement's offset from the site's start) and `insn_end`, `length`, `index` (references into `.rdata` between the jump and it), `slot` (the register it writes, 0 rax to 15 r15), `value` (its opcode: map × 256 + byte; 141 lea, 139 mov from memory, 184 to 191 mov of a 64-bit immediate), `sites` (the count) |
| `stats.fd4.idiom` | `hits`: the idiom's; `captures`: where R0c's finder found each accepted hit's path, as above but `hits` for the count, and `anchor` (bytes before the hit) for a load hoisted before the head instead of `at` and `insn_end` |
| `stats.fd4.accepted`, `stats.fd4.refused` | R0's rule over the same hits, for the comparison: no decoding, and the first reference into `.rdata` after the pattern taken for the path |
| `stats.fd4.names` | Of the accepted hits' class objects: `of` how many, `agree` how many the best name layout reads (with its `offset` and `encoding`, even under half), `fixups` how many have a base relocation in their first 128 bytes (a pointer the file initialises), `unreadable` how many lie past the raw data the file holds of their section (zeros until the game writes them) |
| `stats.fd4.classes`, `loads`, `calls` | Distinct class objects, statics and getters of the accepted hits |

## lint --report, lint --recording and lint --gal

```text
eldennet-engine lint --report <file>
eldennet-engine lint --recording <file>
eldennet-engine lint --gal <folder>
```

The check every numbers-only report passes before it leaves the PC or is committed (plan §1.2; DataPiler's guard checks
the same, DP36). `--report` reads the file's `report` key and checks the file against that kind's vocabulary
(`derive`, which R1's `r1-derive.json` is too, or `probe`, R1's `r1-probe.json`; both vocabularies are en-gal's):
every key known and given once, every value of its key's type, strings only as the key's fixed words, labels or
canonical patterns of at most 64 bytes, no float, no null, at most 64 patterns, nested at most 16 deep, at most 256 KiB.
So a path, a free string, a name and a byte run longer than a pattern are refused. Exit codes: 0 clean (`lint: the
report is clean`); 1 refused, each problem on a line of standard output, `lint: <where>: <problem>`, never quoting what
was there; 2 a usage error, or a file that cannot be read or is larger than 256 KiB.

`--recording` checks eldennet.dll's recording (`*.ensnap.jsonl`, src/eldennet/README.md) the same way, line by line:
every line one report of the kind `recording`, whose keys may also hold a finite number that is not whole (a
position, a distance), each problem with its line (`lint: line 12: chars[0].pos: ...`). Exit codes as above, a file
larger than 512 MiB being 2 (`lint: the recording is clean`, `... is refused`).

`--gal` checks a gal/ folder before it is committed or packaged (ARCH §6.3): `builds.toml` (required), `patterns.toml`
and `leads.toml` (each if present) read as the DLL and probe-exe read them, and each build's layout with
`load_layout`'s checks in a dev build's mode (leads allowed; what the read groups need is the DLL's to check at boot).
Every pattern needs its derivation note, `derivations/<id>.md` (ARCH §6.3, DESIGN O27), and every build a pattern
lists as confirmed must be one builds.toml holds. Each problem is printed as `lint: <file>: <where>: <problem>`, never
with a value; on success one line, `lint: gal/ is clean: builds 1, layouts 1, patterns 1 (confirmed in no build: 1),
leads.toml read`. Exit codes as above, 2 for a folder without a readable builds.toml. ARCH §6.3's check that no
pattern holds a byte string quoted in `docs/research/` needs the design branch's research folder beside it, and waits.

## rec-stats, replay and spike-report (the first in-game build's part 9)

```text
eldennet-engine rec-stats <recording> [--out FILE]
eldennet-engine replay <recording> [--out FILE] [--events FILE]
eldennet-engine spike-report s0a|s0b|s0c|s0d [--log FILE]... [--recording FILE]... [--need N] [--out FILE]
```

Each reads what the owner's PC wrote, numbers and fixed words only: eldennet.dll's recordings (`*.ensnap.jsonl`, linted
first as `lint --recording` lints them, then read back with en_shell's `read_recorded_line`, the recorder's own words)
and the launcher's spike logs (`%APPDATA%\EldenNet\spikes\<spike>-<stamp>.jsonl`: a line per run with its load
`mode`, then the tally). Each writes a numbers-only report through en-gal's writer, of the kinds `rec_stats`, `replay`
and `spike` (schema 1; `lint --report` checks them), to `--out` or, for rec-stats and replay, to standard output.
`docs/design/spikes/formats.md` describes the inputs and the reports for DataPiler's importer.

- **rec-stats**: the lines by kind; the header (game version, region, EldenNet's version, the spike, a probe build,
  the files); the length; the snapshots (count, `gap` flags, intervals over 1 s and over 5 s, the longest, the rate in
  mHz, the most characters, the flag frames); the task (the snapshots' build times at p50, p99 and the most, and the
  C13 histogram's p99, last and highest); each group's faults (summed: a snapshot holds its own tick's), whether it
  tripped and in how many snapshots it was Ok; the lifecycle messages; the core's changes by kind; the gate's openings,
  closings and time open; the banners; the commands; the Marks.
- **replay**: the recording's inputs (its lifecycle messages, banners and snapshots, in their order, each at the
  latest time seen so far) stepped through en-core's v0.1 slice, and so Augur (part 11), again, and its changes (what, from and to) compared
  with the changes the recording holds, which the DLL's core answered in game: `agree`, or the first that differs.
  The name of a playthrough was never recorded, only its length, so a stand-in of that length takes its place (the
  core tells playthroughs apart by slot, class and name together). Exit 1 when it disagrees: the core or the recorder
  has a defect, or the file was changed. The report adds Augur's events: `events` (their count) and `event_kinds`
  (each kind's count), and `--events FILE` writes them in the golden files' form, so that a recording can become a
  route (below).
- **spike-report**: DESIGN §16.1's pass rules, each `pass`, `fail` or `not_measured`, with its value and limit:
  - `s0a` over `--log` files: per load mode (`suspended`, early; `window` or `title`, late), `runs_per_mode` (at least
    `--need`, 20 by default), `ready_or_l1` (every run Ready or L1: a banner or inert) and `no_crash`; and
    `both_modes`, not measured until both loads have run. Per mode too: the items resolved (least and most, of how
    many), the runs by discovery pass (the retries at +5, +15, +30 s) and the time to Ready (p50, most). The
    third-party rows (O91) are the owner's runs under the same rules, a log each.
  - `s0b` over the session's recording and its log: `session_length` (at least 7,200 s of snapshots), `faults` (0),
    `no_trip` (no group tripped), `task_p99_us` (the C13 p99 at the session's end, at most 500), `no_crash`.
  - `s0c` over each session's recording and the s0c log: `gate_closed_unless_solo` (no snapshot of another world, a
    lobby, another player or anti-cheat left with the gate open: the core closes it on the first, so its change
    follows that snapshot's line), `connections` (the game's connections beyond this PC, at most 0), `no_crash`, and
    `online_menu`, the owner's to see (never measured); the gate's openings, closings and time open.
  - `s0d` over the session's recording, its log and the s0d-crash log: `faults_caught` (at least `--need` faults
    caught, 20 by default), `breaker_tripped` (a group's breaker tripped), `no_crash` (the session), and
    `crashes_recorded` (every forced crash recorded by the watcher).

  The verdict is `pass` when every rule measured passes and none is missing, `incomplete` when one could not be
  measured from the files given (`online_menu` aside), else `fail`; each rule is a line on standard output
  (`spike-report: rule=task_p99_us status=pass value=310 limit=500`), then the verdict.

Exit codes: 0 written (replay agreeing; spike-report passing); 1 replay disagreeing, or a spike's verdict `fail` or
`incomplete`; 2 a usage error, or a file that cannot be read, is too large (a recording 512 MiB, a log 16 MiB) or is
refused (a recording by the lint, a log line that is not the launcher's); 5 a report refused by its own writer (a
defect of ours). No message quotes what a file held.

## golden: Augur's golden files (part 11)

```text
eldennet-engine golden [check|update] <route.scn | recording.ensnap.jsonl> [--golden FILE] [--changes FILE]
```

Runs a scenario or a recording through en-core and Augur, checks a scenario's expectations, and compares the events,
in the golden files' form (src/en-augur/README.md), with the route's golden file: `tests/golden/<route>.events.jsonl`
beside the input's folder, or `--golden`. The comparison is exact, times included: replays are deterministic (C12).
`check`, the default, prints our own diff on a difference (`@@ golden line N, run line N @@`, then `-` the golden's
lines and `+` the run's, with a line of context, at most 40); then it checks that `CHANGES.md` beside the golden file
(or `--changes`) has a line naming `` `<route>` `` and the golden file's hash, so a golden file that changes without its
entry fails (ARCH §8.12). `update` (or `--update`) rewrites the file (beside it as `.part`, then renamed), prints the
diff and the line CHANGES.md needs; it writes nothing when an expectation fails. A recording's route is its file name
without `.ensnap.jsonl`.

Exit codes: 0 agrees (check) or written (update); 1 differs, an expectation failed, or CHANGES.md lacks the entry;
2 a usage error, an input that cannot be read or parsed (a recording refused by the lint), or a file that cannot be
written. No message quotes an input beyond a word of the DSL's own grammar.

**The scenario DSL** (`tests/scenarios/*.scn`, `lib/scn.hpp`): ARCH §8.12's synthetic DSL in our own form for what v0.1
can say. `route <name>`, `seed <n>`, `step <ms>` (the snapshot period, 250 by default), `end <ms>` (3 s after the last
action by default), then `at <ms> <verb> ...`, applied at the first snapshot at or after its time, in the file's order:

| Verb | What it does |
|---|---|
| `spawn <name> team=<n> pos=x,y,z [hp=c/m] [param=<n>] [offscreen]` | a character (handles are invented, from 0x1000); `spawn player hp=.. pos=..` sets the Tarnished's |
| `despawn <name>`, `move <who> to=x,y,z [over=<ms>]` | gone; moved, in a straight line over the time |
| `hit <a> -> <b> dmg=<n>`, `heal <who> amount=<n>`, `kill <who> [by=<a>]` | HP down with `last_hit_by` set; up; 0 with the dead bit (the Tarnished's death count + 1) |
| `lock <name>`, `unlock`, `throw <who> <n>` | the Tarnished's lock-on; a throw state |
| `ride on\|off`, `place <id>\|none`, `banner <n>\|none`, `talk on\|off`, `ng <n>` | the ride, the place, the banner, a popup's talk id, the journey |
| `boss <name> on\|off` | a boss bar for the character (its `param` as the name id) |
| `session solo\|coop`, `title`, `load [over=<ms>] [revive]` | another player in the world (the gate closes); the title screen; a loading screen, the Tarnished revived after it |
| `wait`, `drop_snapshots <n>` | nothing; the next n snapshots are not delivered (gaps) |

`set_flag` (the flag catalogue, v0.2), `say` (the Pipeline source) and `expect bark` (v0.4) are refused with the version
they wait for. Expectations: `expect event <Kind> [field=value ...] [within <a>..<b>]` (at least one),
`expect no <Kind> ...` (none), `expect count <n> <Kind> ...` (exactly n) and `expect audience <Kind> ... = [<ids>]`
(the first match's audience). A field is a top-level key of the event's golden form (`subject`, `object`, `key`,
`class`, `conf`, `count`, `merged`, `source`) or one of its payload's; a character matches by the scenario's name, a list
as `[a,b]`, a number within 1e-6. The player plays the lifecycle messages a session starts with (DataReady, then the
playthrough's), and a snapshot every step: a solo world of one's own, every group Ok, the Tarnished at the given
place, its play time rising.

## gal/ for the owner's 2.7.2.0 WW (R0c into R1)

The repository's `gal/` now holds what R0c, the second offline run (2026-11-16), gave, each value a lead until R1 and
S0-A confirm it (ARCH §6.3): `builds.toml`'s fingerprint for 2.7.2.0 WW (App 1.17.2); `patterns.toml`'s FD4 idiom
(`fd4_idiom`, 24 bytes, 7 fixed, R0c's 7,151 hits, confirmed in no build) with its note
`derivations/fd4_idiom.md`; and the draft layout `layouts/2.7.2.0-ww.toml`, a `[task]` holding only the group's name
and index (110: R0c's table of 168 entries of 32 bytes, UTF-16 names at +16), which only R1 reads. The registration,
the task's shape, the statics, the classes and fields are R1's to find; `leads.toml` is R0's, unchanged but for its
comments. CTest's `en_tools_cli` runs `lint --gal` on it in the Linux builds.

## Tests

`en_tools_tests` (doctest, in the Linux builds): the DSL's refusals with their lines, the three routes' expectations
and golden files (exact, and CHANGES.md naming each), determinism (byte-identical over 10 runs, with and without a
seeded loss), 50% snapshot loss against the dense run (src/en-augur/README.md, "Snapshot loss"), a recording of a
route's inputs written as the DLL writes it replaying to the same events, and the diff. CTest's `en_golden.<route>`
runs `golden check` on each route as CI does.

`tests/cli_test.py` (CTest's `en_tools_cli`, in the Linux builds) runs both commands as DataPiler's job would, on the
derivation tests' invented program that `en_tools_write_sample` writes as a file: every exit code above, derive.json's
content, the static's pattern from a slot, the lint's refusals of a path, a free string, a byte run and a float without
quoting them, a recording linted clean and one with a path refused at its line, `lint --gal` clean on the
repository's gal/ and on an invented folder and refusing seven invented ones (a pattern without its note, a layout
for another build, a confirmed build builds.toml lacks, a bad builds.toml, a missing layout, a half-filled lead
`[task]`, a provenance that is not our own derivation) without quoting them; rec-stats, replay and spike-report on
recordings that `en_tools_write_recording` writes as eldennet.dll's core thread does (a solo session of two hours, the
same with a slow task and a fault, a co-op visit that closes the gate, faults injected until a breaker trips) and on
invented spike logs: the counts, every replay agreeing and a recording with a change taken out disagreeing at it, each
spike's rules and verdict passing, failing (19 runs, a crash, a slow p99, a connection, the gate left open, a crash
not recorded) and incomplete (one load only), and the refusals; and no path in any message; `replay --events` and `golden` on a recording (update, check, a golden changed by hand or
without its CHANGES.md entry, an unmet expectation, usage errors); `--sections` against the table read in Python, a refused section's line
(its shape, its numbers, and DataPiler's log's shape), and a copy whose last section's raw data runs past the end of
the file and whose SizeOfImage is short of its alignment, which derives as the file itself does. The Windows builds run the same commands under Wine on `fakegame.exe`
(`tests/fakegame/`).
