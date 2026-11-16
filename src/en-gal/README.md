# en-gal: the game access layer

The part of EldenNet's engine that reads the game (ARCH §6), C++20, MIT. Three static libraries, each a module the DLL
may link (`en_dll_module`):

- `en_gal`, the offline part, which builds with every compiler: the PE reader, the patterns, `gal/`'s schemas, the
  derivation kit that `eldennet-engine probe-exe` runs (part 3), and, with clang-cl, `GuardedReader`'s guarded copy.
  en-launcher and en-tools use it off game (ARCH §2.3).
- `en_gal_game`, the part that runs in the game process (part 4): `GuardedReader`, the read groups and their breakers,
  discovery over the live image, `ErBackend`'s reads, the task; portable C++ over `SysLayer`, the platform seam, tested
  on Linux over the tests' `FakeSys`; and, with clang-cl, `NativeSys` (`src/sys/`), the seam in game.
- `en_gal_game_probe`, the same with the probe option (`EN_PROBE`; ARCH §2.5): built for the tests and the test DLL,
  and when the CMake option `EN_PROBE` is on. A release build links `en_gal_game`.

Nothing in it knows the game. It reads any PE32+ image for x64; the patterns, their sections, the layouts and what the
read groups need come from `gal/`'s files and from the caller, and the real values arrive only through the derivation
round (O89): no byte pattern, offset, address or class name of the game is in the code or the tests, whose images,
patterns and layouts are invented. It keeps ARCH §2.4's rules for en-gal: nothing throws or aborts (toml++ is built
with `TOML_EXCEPTIONS=0`, so a parse answers its error; in every module alike, since its reader interface has a
virtual function only without exceptions, so two settings in one program are two vtables of one name, which the linker
merges: eldennet-engine, linking en-gal and en-llm, crashed in GCC's build), indexing only after a bound check, checked arithmetic on every
value read from an image, no lock, and no raw address, `reinterpret_cast` or pointer arithmetic outside `reader` and
`sys`, each with its `// SAFETY:` note.

## The offline part

- **`pe.hpp`: the PE32+ reader** (ARCH §6.1's executable image). `read_image(source, layout)` reads the headers, the
  section table, the version resource (VS_FIXEDFILEINFO's file and product versions, ProductName from the string table
  the first translation names, and that translation) and the exception directory's function table, from the file
  (`ImageLayout::File`) or from the image as the loader mapped it (`ImageLayout::Memory`). It reads through a
  `ByteSource` and copies what it needs, never pointing into the image: offline the source is the file's bytes
  (`SpanSource`), in game it will be `sys`'s image view, whose reads are guarded copies. Every offset the image gives is
  checked against the image's bounds before it is followed; the resource tree is read to its fixed depth of three.
  What is malformed is refused as a `PeError` naming the problem and where it was found (`describe`). `ImageInfo`
  finds sections by name and by RVA, places an RVA's bytes in the source (`offset_of`), finds the function holding an
  RVA (`function_at`, by binary search), and `is_fragment` tells a function's fragment (chained unwind information, or
  an entry pointing at another).
- **`patterns.hpp`: the patterns** (ARCH §6.2, §6.3). The grammar (`parse_pattern`): hex byte pairs, `??` for a
  wildcard, whitespace between them ignored; 1 to 256 bytes, at least 4 of them fixed; refused with the problem and the
  character where it was found. `MultiMatcher` finds many patterns in one pass, anchored on a pair of fixed bytes (or
  one) and checked whole, over a stream fed in overlapping chunks; a gap between chunks is never examined and is
  counted. `parse_patterns` reads `patterns.toml` (functions, statics and idioms, each with only its own fields; the
  provenance always `own-derivation`; captures within their patterns; ids unique; the caller's required ids present).
  `scan_patterns(image, source, set)` matches each section once, in 1 MiB copies (an unreadable copy is read again page
  by page, on the source's page boundaries, and the pages it cannot read are skipped and counted), and applies the
  rules: exactly `expect_matches` hits (a range for idioms); a function's hit inside a function of the exception
  directory, not a fragment, at `entry + anchor_min` or later (`anchor_min ≥ 5`), ending within it, with its length
  within `fn_len` and its prologue's bytes if given; a static's RIP-relative displacement reaching `target_section`,
  aligned to 8; an idiom's hits kept only where every capture lands in its section, aligned, at a function's start
  where asked; and every hit of a pattern resolving to the same function or static. Results are RVAs, whichever the
  layout.
- **`builds.hpp`: `gal/builds.toml` and the gate** (ARCH §3.2, §6.3). `parse_builds` checks every field's form, the key
  (the file version and the region joined), the region against the file version's fourth field (0 ww, 1 jp: a lead,
  ER01 §1), the layout path (`layouts/<name>.toml`), a SizeOfImage above 0 (the placeholder before a probe is
  refused), and unique keys and fingerprints. `gate(image, builds)` passes only an exact fingerprint (file version,
  product name, region, TimeDateStamp, SizeOfImage), and otherwise answers `NoVersion`, `UnknownBuild` or
  `FingerprintDiffers` with the fields that differ ("build fingerprint differs (TimeDateStamp)"). A retired build
  passes too. What passes is a `BuildProfile`, which only `gate()` makes (R7).
- **`layout.hpp`: the layouts** (ARCH §6.3). `load_layout(profile, text, needs, mode)` is the only maker of a `Layout`,
  and needs the gate's `BuildProfile`: a layout written for another build is refused (R7, by type). It checks the
  schema; the field types (`u8` to `array{elem, len, stride}`), each field within its class's copy size or stride and
  every copy size within 0x4000; every `to` and `elem` naming a class of the file; each enum table total over the
  range of the fields that use it; what the read groups need (`LayoutNeeds`: fields with their types, enum tables with
  the snapshot enum's names, params) present; and, in a release build, no entry left a `lead` (an entry with no
  `status` is a lead). `max_copy_size()` sizes the scratch buffers (ARCH §6.5).

Every schema problem is reported, up to 64, with where it was and what was wrong, never a value
(`patterns.toml: pattern[2].capture: lies outside the pattern`); a gal/ file over 256 KiB is refused unread.

**toml++ 3.4.0's traps.** `en_fuzz_galtoml` found inputs that trip an assertion of toml++'s in a debug build and
are undefined behaviour in a release one, where the assertion is an assumption or `__builtin_unreachable`: a table
header whose first key does not start right after `[` (`[!]`, `[é]`); a character outside ASCII where whitespace may
stand (after a key or a value, or inside a bare value, which toml++ scans quotes and all up to a space, `]`, `}`, `,`
or `#`): its test for non-ASCII whitespace reaches "unreachable" for U+00A1 to U+0499 and other ranges; a `}`
where an array expects a value (`[1, }]`); and a date followed by a space and one digit (`1979-05-27 7,`), from which
it scans on past a terminator. So before toml++ sees a gal/ file, en-proto's `unsafe_for_toml` (toml_check.hpp, which
en-llm's readers of the player's files run too, with the player's rules) checks it, following TOML's contexts: a line
that starts with `[` opens a table header with a key (a letter, a digit, `_`, `-` or a quote after `[` or `[[`; such a
line inside a multi-line array is refused too); no control character but a tab; a character outside ASCII stands only
in a comment, and a string writes one as `\uXXXX` (`leads = ["ER01 \u00A73.2"]`); a quote never stands inside what
toml++ scans as a bare value (up to a space, a tab, `]`, `}`, `,`, `#` or a line break, `=` and `[` included), nor
right after a key's own character; after a value only a terminator or a comment; there is no multi-line string; a `}`
never stands where a value must; and a bare key or value that starts with a digit has no `-` or `:` after it (gal/'s
files hold no dates or times). en_fuzz_galtoml's five inputs are kept in `fuzz/galtoml/corpus/` as `finding-*`,
and en_fuzz_config's two in `fuzz/config/corpus/`.

```cpp
#include "en/gal/builds.hpp"
#include "en/gal/layout.hpp"
#include "en/gal/patterns.hpp"

const en::gal::SpanSource source(exe_bytes);                                  // offline: the file
auto image = en::gal::read_image(source, en::gal::ImageLayout::File);         // Expected<ImageInfo, PeError>
auto profile = en::gal::gate(*image, *builds);                                 // Expected<BuildProfile, GateError>
auto layout = en::gal::load_layout(*profile, layout_text, needs, en::gal::LayoutMode::Release);
const en::gal::ScanReport found = en::gal::scan_patterns(*image, source, *patterns);
```

## The derivation kit (part 3: the offline round R0 of O89)

Our own code, written from the documented formats and ARCH §6.2's description (O48): no byte, pattern, offset or name
of any game is in it, and every name it looks for comes from a leads file.

- **`x86.hpp`: the x86-64 instruction-length decoder.** For 64-bit mode: legacy and REX prefixes, the one-, two- and
  three-byte maps, ModRM, SIB, displacements and immediates, VEX, EVEX and XOP. `decode(bytes)` answers an
  `Instruction`: its length, where its displacement and immediates lie, whether the displacement is RIP-relative or an
  absolute address, whether the immediate is a branch's displacement, and the flow (call, jump, branch, return). It
  names no mnemonic, reads at most 15 bytes, never throws and allocates nothing. Where processors disagree it follows
  LLVM (a 66 prefix shortens JMP's, Jcc's and XBEGIN's displacement but not CALL's; a REX followed by another prefix is
  ignored); it sizes opcodes a map leaves undefined by the map's shape, where LLVM refuses them; and it refuses APX
  (REX2, EVEX map 4) and the VIA PadLock opcodes.
- **`image_bytes.hpp`: an image read once.** Every section's bytes as the loader maps them, copied through a
  `ByteSource`, for reads by RVA (`u32`, `u64`, `ascii`, `utf16`, `rva_of` a virtual address).
- **`rtti.hpp`: the RTTI walk.** MSVC's complete-object locators (signature 1, their own offset), type descriptors,
  class hierarchies and base class arrays, and the vtables one pointer after a pointer to a locator, with their slots
  (pointers into code). `find_type_descriptor("Space::Class")` by the decorated name; `derived_from(walk, base)` gives the
  primary and subobject vtables of every class that lists the base (ARCH §6.2's vtable sets).
- **`fd4.hpp`: the FD4 singleton finder.** `find(img, idiom, config)` matches the idiom's pattern over `.text`, checks
  each hit's placement as ARCH §6.2 says, names it through the runtime class object's name layout, and indexes by name
  (two statics behind one name: ambiguous). Where the pattern stops short of the path's load, the path is the first
  load from `.rdata` in the block the idiom's jump skips. `idiom_from(spec)` builds the idiom from patterns.toml's
  `fd4_idiom`, by its captures' names (`static`, `class`, `getter`, `path`).
- **`derive.hpp`: the derivation.** `parse_leads(text)` reads the leads file (what to look for: names and shapes, never
  bytes, under labels of its author's own); `derive(image, source, leads, options)` finds, from the file:
  - the fingerprint (the version resource, the region from FileVersion's fourth field, the product name compared with
    the leads', TimeDateStamp, SizeOfImage, the section table);
  - the FD4 idiom, cut from the sites that load a path ending with the leads' suffix (the closest `mov reg, [rip+x];
    test reg, reg; jnz` before each load whose jump passes it): the largest group of sites with the same head, their
    shared bytes, and the captures; the name layout (the offset in the class objects that points at an identifier in
    `.rdata` for the most of them); the singletons by the leads' names, the rest only counted;
  - the vtable sets and the task classes (their slots, how many functions each slot holds across them, and the one slot
    every class fills with its own: the run slot's candidate);
  - `register_task`: the direct calls after a load of the registration's instance, whose arguments a small register
    model follows back to the start of the block (the instance, a 64-bit argument, a 32-bit one), grouped by callee;
  - the task group's index: its name, as ASCII or UTF-16, in a table of pointers to names, cut where the code addresses
    the table, or beside a small constant where the code loads it;
  - each AOB static's class vtable, for R1's search, and, given R1's slot (`Options::static_slots`), a pattern at a
    reference to it;
  - every pattern cut by `cut.hpp`'s rules: whole instructions, at entry + 5 or later inside a function of the exception
    directory that is no fragment, at most 64 bytes, the RIP-relative and other 32-bit displacements, the branches whose
    targets lie outside the pattern, the relocated bytes and the immediates that do not fit in a signed byte
    wildcarded; unique in `.text`, preferring 20 bytes with 12 fixed and the smaller function; then checked again by
    `scan_patterns` as patterns.toml would hold it;
  - and the list of what R1 must confirm. `complete(result)` says whether everything R0 can derive was derived.
- **`report.hpp`: the numbers-only writer and its lint.** A report is one JSON object whose keys all come from its
  kind's closed vocabulary, each with one type of value: whole numbers, booleans, fixed words, labels (`[a-z][a-z0-9_]*`),
  patterns of at most 64 bytes in canonical form, objects and arrays. The `Writer` refuses anything else, and its first
  refusal leaves no text; `lint(text, vocabularies)` checks a file token by token (a repeated key cannot hide a
  string), with problems that say where, never what. `derive::write_report` writes derive.json through it, under
  `derive::vocabulary()`.

```cpp
#include "en/gal/derive.hpp"

const auto leads = en::gal::derive::parse_leads(leads_text);              // Expected<Leads, SchemaErrors>
const en::gal::SpanSource source(exe_bytes);
const auto image = en::gal::read_image(source, en::gal::ImageLayout::File);
const auto result = en::gal::derive::derive(*image, source, *leads);
const auto json = en::gal::derive::write_report(result);                  // Expected<std::string, report::Problem>
```

## The guarded copy (clang-cl only)

`GuardedReader`'s copy (ARCH §6.5, DESIGN R4): `seh_copy(address, dst)` copies bytes from an address into storage the
caller owns under one structured exception handler and answers whether every byte arrived. It needs `__try` and
`__except`, which an MSVC-like compiler has: the clang-cl build (`cmake/clang-cl-xwin.cmake`, ARCH §2.6) adds
`reader.cpp`, and the GCC, Clang and MinGW-w64 builds leave it out, with a line saying so.

```cpp
#include "en/gal/reader.hpp"

std::array<std::byte, 64> scratch{};                                  // storage the caller owns
const en::gal::CopyOutcome out = en::gal::seh_copy(address, scratch);  // address: a std::uint64_t
if (!out.copied) {
  // out.code: 0xC0000005 (access violation), 0xC0000006 (in-page error) or 0x80000001 (guard page); scratch is zeroed
}
```

- `seh_copy(address, dst)` copies `dst.size()` bytes from `address`. An access violation, an in-page error or a guard
  page stops it: it answers `{false, code}` with `dst` zeroed, so nothing of a partial copy survives. Any other
  exception is not the copy's, and goes on to the next handler. An empty `dst` copies nothing and answers `{true, 0}`.
- It never throws, keeps no state and takes no lock, and the pointer it makes from the address lives for the copy
  alone (R4). The plausibility checks, groups, budgets and breakers around it are `GuardedReader`'s (below).
- **Why the copy has a frame of its own.** Clang's SEH on x64 covers the calls a `__try` makes, not the instructions
  of its own frame, and clang turns a `memcpy` written in a `__try` into an intrinsic, which is no call: at `/Od` its
  fault goes unhandled, and at `/O2` clang deletes the `__except` altogether. So the `__try` calls `copy_bytes`, which
  is never inlined and does the `memcpy`; the release build (`/O2`, ThinLTO) keeps that call inside the guarded range,
  under `__C_specific_handler`.
- **Guard pages.** Windows lifts a guard page's guard as it raises the fault, so a guard page read once is a guard page
  no more, and its next read goes through. Windows grows a thread's stack through such a page.

## In game (part 4 of the first in-game build)

What runs in the game process, around the one call EldenNet makes into the game. Nothing here names the game either:
`roles.hpp` gives every item, class and field a label of ours (`characters`, `save_data`, `vitals.hp`, ...), and the
game's names (the singletons' runtime-class names, the RTTI classes, the task group's name) and every offset come from
`gal/`'s files under those labels (O48; WHY§123). A field is checked by its kind (a whole number of any width, a
pointer, a bit, ...), so a layout says what the game holds and the reader widens it.

- **`sys.hpp`: the seam.** `SysLayer` keeps what needs the platform: the main module and a guarded view of its image,
  the copy primitive (`Copier`), the stack table, the clock, the init-only getter fallback and `register_task`.
  Discovery is not the seam's but portable code over the image it shows, so the tests run the same discovery the game
  does. `run_task(body, frame)` is the trampoline's body call, in `task.cpp`, the one place that issues the game
  thread's token (R1); an exception out of the body trips the task's guard and every later call returns at once (R3).
- **`native_sys.hpp`: NativeSys** (`src/sys/`, clang-cl only). The image view reads pages VirtualQuery calls
  committed, readable and no guard page, through the SEH copy; the copy primitive is the SEH copy or
  ReadProcessMemory on our own process (`CopyKind::Rpm`, S0-D's setting); the stack table holds every allocation with
  a guard page (the shape of a thread's stack; in an image, its guard pages alone) and each thread's own stack as it
  asks (`note_own_stack`, T1 at init and the game thread at its first tick). `register.cpp` holds the two casts of an
  address to a function (R2): `register_task`, called once with the layout's argument registers, and the FD4 getter,
  once per candidate before it, both under SEH. `trampoline.cpp` makes our task object once (a pointer to a read-only
  fake vtable of `vtable_slots` guarded no-ops that count their calls, the run slot `en_task_run`; the layout's
  `object_size` zeroed bytes; our body's pointer), never freed; `en_task_run` runs only for our own object.
- **`guarded.hpp`: GuardedReader** (ARCH §6.5). Every read takes the token and a group; plausibility before any copy
  (non-null, at least 64 KiB, within user space, aligned for its type, no overflow, at most 0x4000 bytes, never in a
  known stack); 512 copies and 256 KiB a tick; faults counted by kind (C13); and, in debug and probe builds, a read
  off the thread that ran the first tick refused. `ObjView` reads typed fields from a copy by the layout, bounds-checked
  and widened.
- **`health.hpp`: breakers and C13.** A breaker trips on the third hard fault within 60 s, for the session (R8); the
  group book holds each group's state (Untested, Ok, Tripped and why); `Histogram` is C13's 64 buckets of µs, four
  an octave, with percentiles as bucket bounds.
- **`discovery.hpp`: discovery in process** (ARCH §3.2, §6.2). `gate_image` reads the mapped headers and gates them on
  builds.toml; `discover` runs one pass: the AOB items of patterns.toml by item label, one multi-pattern pass; a static
  the probe round gives by its slot (`DiscoveryOptions::static_slots`, the derivation's `--static label=rva`) needs no
  pattern; the FD4 singletons from patterns.toml's `fd4_idiom` and its `path_suffix`, named through the class object
  where builds.toml's `dlrf_name_layout` says so, else by the getter, and matched to the layout's classes by their
  `singleton` names (two statics behind a name: ambiguous); the RTTI sets, each layout class's vtable and those of every
  class deriving from it. `discover_with_retries` passes again at +5, +15 and +30 s while a required item is missing;
  an optional item missing turns only its group off. A refused file is an error, never retried.
- **`backend.hpp`: ErBackend.** Each snapshot re-reads every static it uses and compares each instance's vtable with
  its class's set (R5): a mismatch skips the group for the tick and is a hard fault only on the third tick in a row
  with the main player present; a list entry of another class is dropped softly, and more than a quarter of them is a
  hard fault for world. Each group's reads carry ARCH §6.7's per-tick plausibility (enum values the layout names, ranges,
  hp within max, an orthonormal camera, an hour below 24, play time never going back); a hard fault counts toward the
  group's breaker, and a structural group that trips makes the backend inert. Phase A runs on the first ticks (at most
  8; an instance still null is waited for), phase B on the first full snapshot of each playthrough, with
  `PlaythroughReady` (the name read once, sent only there). While loading only the lifecycle subset is read (R11), and
  at the title or with the session not solo only the session and the lifecycle (Dormant). The flags descend the
  layout's tree to each group's 125-byte block once per group; text and params are binary searches, within the
  per-tick caps; a replaced watch list goes back to the core (C5); the render feed re-checks, every frame, the address
  the last walk found each tracked character at (its vtable and handle).
- **`task.hpp`: GalTask and `install_task`.** The first tick checks the group at the layout's index by name (else inert,
  with where the name really is for the probe); phase A follows, then `DataReady`; then each invocation drains at most
  16 commands, stamps the render feed, and builds a snapshot every 250 ms of accumulated monotonic time (a hitch counts
  for at most 1 s; at most one snapshot an invocation), a flag frame every fourth; three builds over 0.5 ms throttle to
  2 Hz and 16 characters within 25 m, and 60 s of fast builds end it. `TaskStatus` holds C13's histograms (each
  invocation and each build) and the counters, as atomics. `install_task` reads the hub's instance from its static
  through the image and makes the one registration.
- **`probe.hpp`: the probe option** (`EN_PROBE` only). `SlotSearch` finds a static's slot as the data slot whose
  instance has its class's vtable (candidates read on T1 from the image's writable data, checked on the game thread
  within the tick's budget); `vtable_offsets`, `meter_offsets`, `utf16_offsets` and `u32_offsets` search one copied
  object's bytes by invariant and answer offsets only; `GuardedReader::inject` makes every n-th read of a group a real
  fault; `SysLayer::slot_calls` counts the game's calls of each slot.

```cpp
#include "en/gal/discovery.hpp"
#include "en/gal/native_sys.hpp"
#include "en/gal/task.hpp"

en::gal::NativeSys sys;                                                     // T1; lives for the process
auto gated = en::gal::gate_image(sys, builds_text);                         // Expected<Gated, GateFailure>
auto found = en::gal::discover_with_retries(sys, *gated, patterns_text, layout_text, {}, wait_until);
en::gal::GalTask task(sys, *found.last, io);                                // io: the DLL's rings (TaskIo)
const auto registered = en::gal::install_task(sys, *found.last, task);      // the one call into the game
```

## Tests

`cmake --build <build> --target en_gal_tests`, then run `en_gal_tests` (under Wine for the Windows builds). The offline
part's tests run in every build, on `FakeImage`s (`tests/fake_image.hpp`): PE32+ images of the tests' own, laid out as
a linker would, from invented bytes, in both layouts, with whatever sections a test gives them, a version resource and
a function table with chained and indirect entries. They check the same facts from the file and from memory; every
problem a malformed image can have, with where; unreadable bytes in the source; the grammar's forms and refusals; the
matcher against a plain search, fed whole and in chunks of many sizes, with gaps and held-back positions counted; each
of ARCH §6.2's rules over planted patterns, functions and statics; unreadable pages skipped and the matches around
them found; the three schemas with every refusal and its path; the gate's exact match; and R7 (a `BuildProfile` only
from the gate, a `Layout` only for the gated build). The clang-cl build adds the guarded copy's tests: a copy from good
memory (across two committed pages too) arrives whole; a copy from a page reserved but not committed, from a guard
page (whose guard is gone after), from the null page, the kernel's half, a no-access page or a released region, and
one across the end of a committed region, fails cleanly with the destination zeroed, and the program goes on; 2,000
rounds of faults and good copies, and four threads faulting at once, leave nothing behind.

The derivation kit's tests run in every build too. The decoder's (`test_x86.cpp`) take encodings worked out by hand,
every shorter run of each refused as truncated; `tests/x86_objdump.py`, in the Linux builds, compares it with
llvm-objdump over every instruction of the build's own `eldennet-engine` and `en_gal_tests` (any disagreement is a
finding) and over 20,000 seeded random byte strings (three kinds no compiler emits counted apart: a stray REX, APX, and
opcodes a map leaves undefined). The rest (`test_derive.cpp`, `test_report.cpp`) run on an invented program that
`tests/fake_program.hpp` writes like an assembler (`tests/sample_program.hpp`: FD4-style accessors of a shape of their
own, class objects that name them at +0x10, RTTI with a second base and a same-named class elsewhere, task classes, a
registration with its own argument order, a table of UTF-16 group names, statics read by small functions, and decoys):
the walk, the finder, the cutter's wildcards, the leads and their refusals, every derived fact against the program's own
labels, the decoys refused, the statics' patterns from both layouts, and the report holding none of the program's names;
the writer refusing what is outside its vocabulary and the lint refusing a path, a free string and a byte run longer
than a pattern. `tests/fakegame/` runs the Windows `eldennet-engine.exe` on `fakegame.exe` under Wine against what
`fakegame --describe` says.

The in-game part's tests (part 4) run in every build over `FakeSys` (`tests/fake_sys.hpp`: a process of byte regions,
copies that fail as an access violation would, released regions, declared stacks, a clock and a thread of the test's
own, and a hub's tick) and `FakeWorld` (`tests/fake_world.hpp`: an invented game of eleven singletons of invented names,
a registration of its own argument order, AOB statics, MSVC RTTI with a character base and its subclasses, a flag
tree, text and param tables, and its gal/ texts). They check the reader's rules, ObjView, the breakers' window and
the histogram (`test_guarded.cpp`); discovery: every item where the world put it, the getter called once a candidate
and refused after registration, the probe round's slot, the retries' times, optional and structural items missing, a
release build refusing leads (`test_discovery.cpp`); every snapshot field against the world, phase A and phase B, R11's
copies while loading, statics that move (as tombstones, released, kept), are nulled and repointed (R5), vtable swaps and
the three-tick rule, real faults tripping an optional group (L3) and a structural one (inert, then no copy at all), the
budget's gap, the first tick's group check, the render feed and C5 (`test_backend.cpp`); registration once with the
layout's registers, 4 Hz at 30, 60 and 144 FPS and with 1 s hitches and 3 s stalls (one snapshot an invocation at
most), the throttle in and out, the command cap, R3, and no allocation or free on the game thread after warm-up, with
this test program's own counting operator new (`test_task.cpp`, `alloc_count.cpp`); and the probe's searches
(`test_probe.cpp`). The clang-cl build adds NativeSys's (`test_native_sys.cpp`): the module's image, both copy
primitives, the stack table and a guard page left untouched, the calls under SEH, and the task object driven as a hub
would drive it. In game, `tests/galtest/` is a DLL that the fake game loads, and the harness's `gal-*` cases check what
it finds against `fakegame --describe` (`tests/fakegame/README.md`).

The fuzz targets `en_fuzz_pe` (the reader and the scan over arbitrary images, in both layouts, with an unreadable page
and small chunks), `en_fuzz_pattern` (the grammar, and the matcher against a plain search), `en_fuzz_galtoml` (the
three schemas), `en_fuzz_x86` (the decoder at every offset: its parts within the instruction and in order, nothing after
it changing it, every shorter run truncated), `en_fuzz_report` (the lint on any text, and the writer driven by the
input: what it writes always lints clean and holds no string offered that is not a word or a label) and
`en_fuzz_derive` (the derivation over any executable: its report always passes the lint) are in `fuzz/`.
