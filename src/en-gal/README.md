# en-gal: the game access layer

The part of EldenNet's engine that reads the game (ARCH §6), C++20, MIT. It holds, so far, its portable offline part,
which builds with every compiler, and `GuardedReader`'s guarded copy, which needs clang-cl. Its static library
`en_gal` is a module the DLL links (`en_dll_module`), and en-launcher and en-tools use the offline part off game
(ARCH §2.3).

Nothing in it knows the game. It reads any PE32+ image for x64; the patterns, their sections, the layouts and what the
read groups need come from `gal/`'s files and from the caller, and the real values arrive only through the derivation
round (O89): no byte pattern, offset, address or class name of the game is in the code or the tests, whose images,
patterns and layouts are invented. It keeps ARCH §2.4's rules for en-gal: nothing throws or aborts (toml++ is built
here with `TOML_EXCEPTIONS=0`, so a parse answers its error; it keeps either setting in an ABI namespace of its own,
so en-llm's toml++ with exceptions links beside it), indexing only after a bound check, checked arithmetic on every
value read from an image, no lock, and no raw address, `reinterpret_cast` or pointer arithmetic outside `reader` (and,
to come, `sys`).

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
it scans on past a terminator. So before toml++ sees a gal/ file, five rules of our own are checked
(`toml_read::unsafe_for_toml`): a line that starts with `[` opens a table header with a key (a letter, a digit, `_`,
`-` or a quote after `[` or `[[`; such a line inside a multi-line array is refused too); a character outside ASCII
stands only in a comment, and a string writes one as `\uXXXX` (`leads = ["ER01 \u00A73.2"]`); there is no multi-line
string; a `}` never follows `[` or `,`; and a bare key or value that starts with a digit has no `-` or `:` after it
(gal/'s files hold no dates or times). The five inputs are kept in `fuzz/galtoml/corpus/` as `finding-*`.

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
  alone (R4). The plausibility checks, groups, budgets and breakers around it are `GuardedReader`'s, still to come
  (§6.5, §6.7).
- **Why the copy has a frame of its own.** Clang's SEH on x64 covers the calls a `__try` makes, not the instructions
  of its own frame, and clang turns a `memcpy` written in a `__try` into an intrinsic, which is no call: at `/Od` its
  fault goes unhandled, and at `/O2` clang deletes the `__except` altogether. So the `__try` calls `copy_bytes`, which
  is never inlined and does the `memcpy`; the release build (`/O2`, ThinLTO) keeps that call inside the guarded range,
  under `__C_specific_handler`.
- **Guard pages.** Windows lifts a guard page's guard as it raises the fault, so a guard page read once is a guard page
  no more, and its next read goes through. Windows grows a thread's stack through such a page.

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

The fuzz targets `en_fuzz_pe` (the reader and the scan over arbitrary images, in both layouts, with an unreadable page
and small chunks), `en_fuzz_pattern` (the grammar, and the matcher against a plain search) and `en_fuzz_galtoml` (the
three schemas) are in `fuzz/`.
