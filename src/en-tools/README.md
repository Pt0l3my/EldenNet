# en-tools: eldennet-engine

The command-line host that runs EldenNet's engine outside the game (ARCH §12.3), C++20, MIT. One program,
`eldennet-engine`, with a subcommand each: `render-prompt`, `chat`, `decide`, `probe`, `models`, and the devkit's
`probe-exe` and `lint` (this file's subject). `eldennet-engine <command> --help` lists a command's options.

## probe-exe: the derivation's offline round (R0, O89)

```text
eldennet-engine probe-exe <exe> --derive --leads <leads.toml> --out <derive.json> [--static <label>=<rva>]...
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

**The game's leads** are `gal/leads.toml`: only what the design names, each with its section (ARCH §6.1 to §6.3).
The design names no task base class, so its `[task] base` holds a marker of ours that matches nothing, and R0 reports
the task `missing` for R1 to find; GameMan and FieldArea are left out until a design document gives their classes.
**The R0 package** for the owner, `EldenNet-devkit-R0.zip`, holds `eldennet-engine.exe` from the clang-cl Release
build, `gal/leads.toml`, `README-R0.txt` (what the tool reads, writes and never does, and the disclosure above),
`LICENSE.txt`, `THIRD_PARTY.md`, `THIRD-PARTY-NOTICES.txt` (the three libraries' licences) and `SHA256SUMS`, which
DataPiler 0.2.41's `probe_exe` step checks the tool and the leads file against before it runs them.

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
executable (a malformed image's message gives the problem and an offset; a leads file's, the key where it lies). On success one summary line goes to standard output:
`probe-exe: derive.json written (complete|incomplete): fd4 <status>, <n> of <m> singletons; registration <status>;
task <status>; group <status>`. It holds the file and a copy of its sections in memory (about twice the file's size),
and decodes `.text` a few times over: here about 3.5 million instructions a second, a third of a second for 3.5 MB of
code; with `--static`, each static adds up to 16 passes over `.text`.

**derive.json** (`"report": "derive"`, `"schema": 1`): `complete`; `image` (the fingerprint: file size, image base,
TimeDateStamp, SizeOfImage, FileVersion and ProductVersion, `region` ww, jp or other, `product_name` matches, differs,
absent or not_given, the translation, the section table with the usual names or `other`); `fd4` (the path strings and
their references, the idiom's pattern, captures and hits, part 2's scan's verdict on it, the name layout, the hits
accepted and refused by reason, and each singleton by label with its static, class object, getter and sites);
`rtti` (the locators and vtables found, each vtable set's primary and subobject vtables, the task classes' vtables,
slots and run slot); `register` (the candidates, best first, each with its callee, call sites, argument registers and
whether they agree, and the first one's pattern); `group` (the index candidates and where they came from); `statics`
(each static's class vtable, slot, references and pattern); `confirm` (what R1 must confirm, as fixed items with a label
or a number); `stats` (instructions decoded, bytes that did not decode, base relocations). Every pattern carries its
length, fixed bytes, where it hit, its function's entry and length, its anchor (at least 5), its matches in `.text` (1)
and whether part 2's scan resolved it.

## lint --report

```text
eldennet-engine lint --report <file>
```

The check every numbers-only report passes before it leaves the PC or is committed (plan §1.2; DataPiler's guard checks
the same, DP36). It reads the file's `report` key and checks the file against that kind's vocabulary (so far `derive`):
every key known and given once, every value of its key's type, strings only as the key's fixed words, labels or
canonical patterns of at most 64 bytes, no float, no null, at most 64 patterns, nested at most 16 deep, at most 256 KiB.
So a path, a free string, a name and a byte run longer than a pattern are refused. Exit codes: 0 clean (`lint: the
report is clean`); 1 refused, each problem on a line of standard output, `lint: <where>: <problem>`, never quoting what
was there; 2 a usage error, or a file that cannot be read or is larger than 256 KiB.

## Tests

`tests/cli_test.py` (CTest's `en_tools_cli`, in the Linux builds) runs both commands as DataPiler's job would, on the
derivation tests' invented program that `en_tools_write_sample` writes as a file: every exit code above, derive.json's
content, the static's pattern from a slot, the lint's refusals of a path, a free string, a byte run and a float without
quoting them, and no path in any message. The Windows builds run the same commands under Wine on `fakegame.exe`
(`tests/fakegame/`).
