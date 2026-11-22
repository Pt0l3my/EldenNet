# en-gal: the game access layer

The part of EldenNet's engine that reads the game (ARCH §6), C++20, MIT. Three static libraries, each a module the DLL
may link (`en_dll_module`):

- `en_gal`, the offline part, which builds with every compiler: the PE reader, the patterns, `gal/`'s schemas, the
  derivation kit that `eldennet-engine probe-exe` runs (part 3), R1's probe report, and, with clang-cl,
  `GuardedReader`'s guarded copy. en-launcher and en-tools use it off game (ARCH §2.3).
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
  **Sections keep the loader's rules, and no more** (a protected game's executable was refused by stricter ones):
  virtual ranges aligned to SectionAlignment, ascending from the end of the headers, disjoint, and within SizeOfImage
  as the loader rounds it (to SectionAlignment and to 4 KiB); and no raw range whose end passes 4 GiB. Raw ranges are
  otherwise free: in any order, overlapping, shared, empty (SizeOfRawData or PointerToRawData 0) or past the end of the
  file; SizeOfRawData may pass VirtualSize and the reverse; a section of no size at all is let through, and holds no
  RVA. In a file, `file_span` gives what the file holds of a section (its raw data, at most its virtual size, up to the
  end of the file) and `read_mapped` reads an RVA as the loader maps it, zeros for the rest; the pattern scan and
  `ImageBytes` read through them. A section's refusal carries a `SectionFault`: the rule, the section's index (never
  its name), the bound it broke and its numbers, worded by `describe` in at most 192 characters (`describe_numbers`'
  words, `va`, `vsize`, `raw`, `rawsize` and `flags`). `read_section_table` reads the headers' sizes and the table with
  only the checks a safe read needs, for `probe-exe --sections`. In the exception directory, an entry of no length
  (GNU toolchains emit them) is left out, not refused.
- **`patterns.hpp`: the patterns** (ARCH §6.2, §6.3). The grammar (`parse_pattern`): hex byte pairs, `??` for a
  wildcard, whitespace between them ignored; 1 to 256 bytes, at least 4 of them fixed; refused with the problem and the
  character where it was found. `MultiMatcher` finds many patterns in one pass, anchored on a pair of fixed bytes (or
  one) and checked whole, over a stream fed in overlapping chunks; a gap between chunks is never examined and is
  counted. `parse_patterns` reads `patterns.toml` (functions, statics and idioms, each with only its own fields; the
  provenance always `own-derivation`; captures within their patterns; ids unique; the caller's required ids present).
  `scan_patterns(image, source, set)` matches each section once, in 1 MiB copies (an unreadable copy is read again page
  by page, on the source's page boundaries, and the pages it cannot read are skipped and counted), and applies the
  rules: exactly `expect_matches` hits (a range for idioms, up to 65,536 for an idiom whose every inlined accessor is a
  hit, 4,096 for the other kinds); a function's hit inside a function of the exception
  directory, not a fragment, at `entry + anchor_min` or later (`anchor_min ≥ 5`), ending within it, with its length
  within `fn_len` and its prologue's bytes if given; a static's RIP-relative displacement reaching `target_section`,
  aligned to 8; an idiom's hits kept only where every capture lands in its section, aligned, at a function's start
  where asked; and every hit of a pattern resolving to the same function or static. Results are RVAs, whichever the
  layout.
- **`builds.hpp`: `gal/builds.toml` and the gate** (ARCH §3.2, §6.3). `parse_builds` checks every field's form, the key
  (the file version and the region joined), the region against the file version's fourth field (0 ww, 1 jp: a lead,
  ER01 §1), the layout path (`layouts/<name>.toml`), a SizeOfImage above 0 (the placeholder before a probe is
  refused), and unique keys and fingerprints. `gate(image, builds)` passes only an exact fingerprint (file version,
  product name, region, TimeDateStamp, SizeOfImage), the product names compared once their symbols are removed
  (`same_product_name`: a trademark, registered or copyright sign and their like, and zero-width characters, with spaces
  made one; ASCII and every other letter kept), and otherwise answers `NoVersion`, `UnknownBuild` or
  `FingerprintDiffers` with the fields that differ ("build fingerprint differs (TimeDateStamp)"). A retired build
  passes too. What passes is a `BuildProfile`, which only `gate()` makes (R7).
- **`layout.hpp`: the layouts** (ARCH §6.3). `load_layout(profile, text, needs, mode)` is the only maker of a `Layout`,
  and needs the gate's `BuildProfile`: a layout written for another build is refused (R7, by type). It checks the
  schema; the field types (`u8` to `array{elem, len, stride}`), each field within its class's copy size or stride and
  every copy size within 0x4000; every `to` and `elem` naming a class of the file; each enum table total over the
  range of the fields that use it; what the read groups need (`LayoutNeeds`: fields with their types, enum tables with
  the snapshot enum's names, params) present; and, in a release build, no entry left a `lead` (an entry with no
  `status` is a lead). `max_copy_size()` sizes the scratch buffers (ARCH §6.5). A `[task]` that is a lead may hold
  only its `group` (the derivation's offline round gives the group's index; `register_abi`, `vtable_slots`,
  `run_slot` and `object_size` are the in-game round's to find, left out rather than guessed): it is `partial`, and
  discovery is never ready with it, `install_task` refuses it and the first tick reads nothing (the real game's draft
  layout, `gal/layouts/2.7.2.0-ww.toml`). A confirmed `[task]`, or a lead with any of those keys, needs them all.
- **`ids.hpp`: Lodestar's catalogue, `gal/ids.toml`** (proposal 0005 §4; O100). Lodestar is EldenNet's own address
  library, and an id is a number and a label, both ours and both permanent. `parse_ids(text)` reads the catalogue and
  reports every problem: a number outside its kind's block (statics from 1000, functions from 2000, classes from 3000,
  tables from 4000) or not above the one before it (the file ascends, so a new id never takes an old number's place);
  a label, or an earlier label (`was`), that is not ours in form (`[a-z][a-z0-9_]*`, at most 48 bytes) or is used
  twice; a meaning that is empty, over 400 bytes or not printable ASCII; a `needs` that is not `boot`, a read group's
  name or `task_service`; a retired id still required. The catalogue holds no fact about any build: an offset, a byte
  or a name there is an unknown key. `roles.hpp`'s items each carry their catalogue number (`ItemInfo::id`), and a
  test holds the two together on the repository's own `gal/ids.toml`, compiled into the tests byte for byte; a second
  test pins each number to its label, so that a renumbering fails the build. The function's catalogue label is
  `task_add`; `register_task`, which schema 1's reports and `patterns.toml` keep, is its `was`.
- **`layout.hpp`, schema 2: the build's table** (proposal 0005 §5 to §7). A schema 2 layout is also where each id lies
  in that build: `[item.<label>]` holds the id's number and kind; its place as numbers (`rva` for a static, a function
  or a table; `length` for a function; `stride` and `count` for a table; `vtables`, ascending, for a class), every one
  inside the build's image (builds.toml's SizeOfImage); `found_by`, the recipes that gave the value, each of which
  must find that kind (`accessor`, `instance_class` and `invariant` a static, `class` a class, `table` a table,
  `relation` a function or a static); up to 8 `witnesses` (places in the code that use it); for a function a `digest`
  (32 lower-case hex digits) and a `verdict` (`adds`, `removes`, `reads`, `unknown`); the `checks` it passed; and its
  `status`. A check an entry claims must have what it checks (`witness` its witnesses, `digest` its digest, `verdict`
  its verdict), and a `confirmed` item must have passed `placement` and `agree` (one recipe alone never confirms an
  id), then `witness` (a class: `class`), and a function `digest` too. `[provenance]` is required: `method =
  "own-derivation"`, the tool's version and SHA-256, the round's label, its date and the leads. Items are read against
  the catalogue the caller gives (`LayoutNeeds::ids`): each under the catalogue's label, number and kind, none
  retired, and a file with items is refused without one. A release build refuses a lead item as any other lead. No
  key of an item can hold a byte of the game: `bytes`, `pattern`, `text`, `name` and the like are unknown keys, and
  refused. A schema 1 layout reads as before and holds neither key.
- **`resolve.hpp`: Lodestar's resolver** (proposal 0005 §7.1, §9.1). The checks a start makes on the build's table,
  with no search: `check_item(image, source, item)` re-finds one item in the image as loaded. *Placement*: a static in
  a section that is read and written and not run, with room for a pointer; a function at a start the image's own
  function table lists, of the table's length; a table whole within one section that is read and not run; a class's
  tables of methods in read-only data, aligned to 8. *Witness*: at each place the table records as using the id, the
  instruction's own bytes are read (one read of at most 15 bytes through the `ByteSource`: offline the file, in game
  the guarded view of the mapped image) and decoded with our decoder, and must refer to the id: a static or a table
  by a RIP-relative operand, a function by a relative call or jump or by a RIP-relative operand (a branch on a
  condition is no witness). The first witness that fails refuses the item and is named by its index. A witness says
  that the bytes at a place refer to the id, not that an instruction starts there: one byte into a prefixed load the
  bytes still decode to the same reference, which is accepted. `Library(layout, image, source)` checks every item
  once, on the init thread, and `place(id, kind)` and `vtables(id)` hand out the offsets of checked items only; a
  refused item answers its `ItemProblem` (fixed words, `to_string`; `check_of` names the check it belongs to), an id
  the table lacks `Unknown`, an id asked for as another kind `Kind`. One refusal touches no other item: nothing is
  looked for nearby and nothing is retried. Nothing in the DLL reads through the library yet: discovery still uses
  `patterns.toml`, until the first table is found and reviewed (the proposal's L3 and L4); the digest, class,
  self-test and verdict checks are later steps'.
  `check_layout(entry, text, needs, mode)` makes the same checks for builds.toml's entry without the gate and keeps no
  `Layout` (R7 still keeps that to `load_layout`): `eldennet-engine lint --gal` runs it.

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
- **`fd4.hpp`: the FD4 singleton finder.** `find(img, idiom, config)` matches the idiom's pattern over `.text`, decodes
  each hit (its static's, class object's and path's displacements must be RIP-relative operands, and its getter's a
  relative call, where the captures say: a pattern that wildcards a register's ModRM also matches other addressing
  forms, which are refused as no idiom), checks its placement as ARCH §6.2 says, names it through the runtime class
  object's name layout, and indexes by name (two statics behind one name: ambiguous). Where the pattern stops short of
  the path's load, the path is the load whose string ends with the suffix, of every load in the block the idiom's jump
  skips (whichever comes first, in whichever register, before or after the getter's call: a RIP-relative address, a
  RIP-relative pointer to the string, or a 64-bit absolute address), or of the 64 bytes before the hit in its function
  (a hoisted load); each site keeps where it found it (`Site::path_load`). R0's rule (the first load from `.rdata` in
  the block, whatever its text, and no decoding of the hit) stays as `Config::first_reference`, for the derivation's
  comparison only. `idiom_from(spec)` builds the idiom from patterns.toml's
  `fd4_idiom`, by its captures' names (`static`, `class`, `getter`, `path`).
- **`shape.hpp`: shapes, recipes without bytes** (proposal 0005 §6.1; O102). A shape is a run of at most 16
  instruction forms in a grammar of our own over the decoder's fields: `load_slot` (a 64-bit load of a RIP-relative
  slot into a register, which the shape remembers; gives the slot), `test_loaded` (a 64-bit test of that register with
  itself), `branch_set_short` and `branch_set_near` (a branch forward taken when the test found it set, with an 8-bit
  or a 32-bit distance), `address_arg1` (the address of a RIP-relative object into the first argument's register;
  gives the object) and `call_relative` (gives its target). Each form is one instruction of the architecture's own
  encoding, with no prefix but REX.W where it is 64-bit, whatever registers and distances it uses; the steps'
  instructions follow one another. `match_at` answers the shape at one place, with what each step gives and where its
  branch goes; `find` looks at every offset of a section, as the pattern scan does, so a site is found also where a
  straight decode is out of step, with a three-byte test on the first step's encoding before each decode. No byte of
  any program is in a shape. `fd4::find(img, accessor, config)` runs the finder on a shape (`fd4::Accessor`: the
  shape and which step gives the static, the class object and the getter): every hit is a site already decoded, its
  path found by decoding on, its placement and name judged as a pattern's site's are. On the tests' three invented
  programs, as files and as mapped, the shape's sites are exactly the pattern's that are not refused as no idiom, with
  every place, verdict and name the same: the test that let `gal/patterns.toml`'s idiom go (O102).
- **`recipes.hpp`: `gal/recipes.toml`** (proposal 0005 §6; O102). `parse_recipes(text, required_ids)` reads the
  recipes, one kind so far: an `accessor`, with its `steps` (1 to 16 forms of the grammar; a test before any load is
  refused), `gives` (which step gives the `static`, the `class` and the `getter`, each of the form that gives it), its
  `path_suffix`, and, as a pattern has, `section`, `expect_matches` (a range up to 65,536), `builds` and `provenance`
  (`own-derivation`). No key of a recipe can hold bytes. `RecipeSpec::accessor()` is what the finder takes. Discovery
  takes the file's text in `DiscoveryOptions::recipes`: with an `accessor` recipe (`roles::kAccessorRecipeId`) the FD4
  singletons are found by its shape and `patterns.toml` needs no `fd4_idiom`, nor is one read; a text that does not
  read is a refusal. On the tests' fake world discovery finds the same items, sites and names by either. The DLL and
  the test DLL read `recipes.toml` when the folder holds it, and a folder with it and no `patterns.toml` is read as
  having no pattern. The repository's `gal/` holds the accessor recipe with its note, and no `patterns.toml` any more;
  the pattern reader stays for the fake game's generated folder, and for the statics' and the function's patterns,
  until discovery goes through the table (the proposal's L3 and L4). R1's check of the idiom byte for byte now finds
  no idiom in the repository's `gal/` and refuses its call, which is what O101 asks: no call into the game.
- **`lodestar.hpp`: Lodestar's finder and its report** (proposal 0005 §6.2, §11; step L3). `lodestar::find(image,
  source, file_size, leads, recipes, builds)` runs, on an executable read from its file, the recipes that need no
  running game: the image's fingerprint and the gate's verdict against `builds.toml`; the accessor's shape (its hits
  against the recipe's `expect_matches`, the sites accepted and refused by reason, and every static the accepted sites
  load, each with its class object, its count of sites and up to 8 witnesses, the loads themselves, which the resolver
  re-finds); the classes the leads name with their tables of methods; and the task groups' table (through the
  derivation, as R0 ran it). While the shape is new it also cuts the withdrawn pattern again in memory, as the
  derivation does, and counts the accepted sites the two share and those only one accepts; for the sites the pattern
  alone accepts it says why the shape does not stand there (`shape::mismatch_at`: the first step that differs, and
  whether it is the form in all but its width, a test of another register, or something else) and which statics only
  those sites load; the pattern is never written. `complete` says the accessor has a recipe, its count within range, an accepted site, and no site that only
  one of the two accepts. `write_report` writes kind `lodestar`, schema 2, through the numbers-only writer: whole
  numbers, booleans, fixed words and the leads' labels. Its vocabulary holds no key of the pattern type and no
  fraction, which a test pins, so a report cannot carry a byte, a pattern, text, a name or an instruction.
- **`derive.hpp`: the derivation.** `parse_leads(text)` reads the leads file (what to look for: names and shapes, never
  bytes, under labels of its author's own); `derive(image, source, leads, options)` finds, from the file:
  - the fingerprint (the version resource, the region from FileVersion's fourth field, the product name compared with
    the leads' as the gate compares it, once symbols are removed, TimeDateStamp, SizeOfImage, the section table);
  - the FD4 idiom, cut from the sites that load a path ending with the leads' suffix (the closest `mov reg, [rip+x];
    test reg, reg; jnz` before each load whose jump passes it, on to the next call after the load): the largest group
    of sites with the same head, their shared bytes, and the captures (the path's only where every site loads it at
    the same place); the name layout (the offset in the class objects that points at an identifier in `.rdata` for
    the most of them); the singletons by the leads' names, the rest only counted. The idiom is `derived` when its
    sites all match, part 2's scan resolves it and a hit is accepted; without a name layout the singletons are
    `depends` (R1 names them through the getter, ARCH §6.2 step 3);
  - the vtable sets and the task classes (their slots, how many functions each slot holds across them, and the one slot
    every class fills with its own: the run slot's candidate; `task_from_base` does the same for a base given by its
    type descriptor, which R1's census finds in game);
  - `register_task`: the direct calls after a load of the registration's instance, whose arguments a small register
    model follows back to the start of the block (the instance, a 64-bit argument, a 32-bit one), grouped by callee;
  - the task group's index: its name, as ASCII or UTF-16, in a table of entries holding pointers to names at a stride
    of 8 to 64 bytes (the stride with the longest run), cut where the code addresses an entry's start, with the name
    pointer's offset in the entry and an index field the entries may hold; or beside a small constant where the code
    loads it. A lone pointer (a table of one) is listed last and gives no index;
  - each AOB static's class vtable, for R1's search, and how many type descriptors carry the class's own name in any
    namespace; given R1's slot (`Options::static_slots`), a pattern at a reference to it. A static whose class has no
    RTTI cannot be found offline: its slot is zeros in the file and nothing names it, so R1 finds it by what its
    instance holds;
  - every pattern cut by `cut.hpp`'s rules: whole instructions, at entry + 5 or later inside a function of the exception
    directory that is no fragment, at most 64 bytes, the RIP-relative and other 32-bit displacements, the branches whose
    targets lie outside the pattern, the relocated bytes and the immediates that do not fit in a signed byte
    wildcarded; unique in `.text`, preferring 20 bytes with 12 fixed and the smaller function; then checked again by
    `scan_patterns` as patterns.toml would hold it;
  - the diagnostics R0c added: per reference to a path, what the site search found; each head shape's sites; where
    each site's and each accepted hit's path load sits; R0's rule over the same hits; what the class objects hold in
    the file; and every executable section decoded through (instructions, bytes that do not decode, their runs and
    where they start, references to the paths);
  - and the list of what R1 must confirm. `complete(result)` says whether everything R0 can derive was derived.
- **`report.hpp`: the numbers-only writer and its lint.** A report is one JSON object whose keys all come from its
  kind's closed vocabulary, each with one type of value: whole numbers, booleans, fixed words, labels (`[a-z][a-z0-9_]*`),
  patterns of at most 64 bytes in canonical form, objects and arrays, and, for a key typed so, finite numbers that need
  not be whole (`Float`: a recording's positions; no derive key takes one). The `Writer` refuses anything else, and its
  first refusal leaves no text; it writes pretty-printed or on one line (`Style::Line`, a line of a JSON Lines file).
  `lint(text, vocabularies)` checks a file token by token (a repeated key cannot hide a string), with problems that say
  where, never what; `lint_lines` checks a JSON Lines file whose every line is one report of the first line's kind
  (eldennet.dll's recordings), each problem with its line. `derive::write_report` writes derive.json through it, under
  `derive::vocabulary()`; eldennet's en_shell holds the `recording` vocabulary.
- **`probe_report.hpp`: R1's report** (kind `probe`, schema 1; O89). What the probe build of eldennet.dll finds in
  game, beside the derivation it runs over the live image: the checks before its one call, the call, the slot counts,
  the hub's census, the rising fields and the statics' slots. The DLL fills `probe::Findings`, and
  `probe::write_report` writes it under `probe::vocabulary()`. It is here rather than in eldennet's en_shell, which
  links en-core, so that the launcher lints R1's two reports after an R1 session (`--probe`) with en-gal's offline
  part alone (ARCH §2.3).

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
  asks (`note_own_stack`, T1 at init and the game thread at its first tick). A thread that ends leaves its range in the
  table while the game may reuse its memory (Wine frees an ended thread's stack late, and T1 ends at Ready; WHY§190):
  `rescan_stacks`, on T1 just before registration, makes the table again without T1's own stack, and a read the table
  refuses is checked once more (`forget_stale_stacks`, on that rare path alone): a range whose allocation no longer
  holds a guard page leaves the table and the read goes on (`GuardedReader::stale_stacks` counts them).
  `set_copy_kind` takes settings' copy primitive on T1, before registration. `sys_boot.hpp` (`src/sys/boot.cpp`) holds
  what eldennet.dll's load and crash filter need of an address turned into a module: DllMain's `on_attach`, `pin`, and
  `fault_site` (the faulting module's file name and offset, allocation-free, for the crash record). `register.cpp` holds the two casts of an
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
  instance has its class's vtable (candidates read on T1 from the image's writable data, `heap_slots`, checked on the
  game thread within the tick's budget), for a layout class or for a vtable set of the caller's (R1, before any
  discovery); `slots_holding` finds the data slots holding a value (R1's `global_hinstance`: the module's base);
  `RisingSearch` copies each candidate's instance again a second or more after its last copy and keeps the 32-bit
  fields that rose by the elapsed milliseconds (within a fifth), with how often each rose, stood still or changed
  otherwise (a play time stands still at the title and while loading; a candidate that faults is not copied again), for
  a static whose class has no RTTI; `Census` copies the task hub's instance and what it points at to a depth of three
  (heap objects and the image's writable data; a copy that faults is made again at half its length, down to 16 bytes),
  and counts the objects whose first 8 bytes are one of the image's vtables (the hub's own tasks, whose shared class is
  the task's base); `vtable_offsets`, `meter_offsets`, `utf16_offsets` and `u32_offsets` search one copied object's
  bytes by invariant and answer offsets only; `GuardedReader::inject` makes every n-th read of a group a real fault;
  `SysLayer::slot_calls` counts the game's calls of each slot, and `TaskShape::every_slot` (R1, the run slot not yet
  known) makes every slot count its calls and run the body for our own object; `SysLayer::task_object` gives our
  object's address, so that R1 can see how much of it the hub wrote. Everything they need on the game thread is
  allocated on T1. Spike.6 (WHY§227) adds two of T1's: `summarise_callee`, the registration's callee as its code reads
  (decoded from its RVA within its function or up to its first return, at most 4 KiB, no call followed: its calls and
  their forms, its branches before the first call, whether a path returns through no call, and each argument
  register's stores, moves to callee-saved registers, compares and tests before the first call, followed through moves
  between registers until an instruction it does not model), and `search_memory`, every committed and searchable
  region (`searchable`: readable, not a guard page, no-access, uncached or write-combined) read through the copy
  primitive in chunks of `kMaxCopyBytes`, page by page where one faults or a stack or a kept-out range overlaps it,
  within a cap of bytes and of time, for the words equal to an address or pointing into the object there, the regions
  whose allocation base the caller names as its own counted apart, and an image hit's section read from its headers by
  its characteristics (`section_kind`). Both fill `probe_report.hpp`'s structures. NativeSys's `region_at` (probe
  builds) is the VirtualQuery behind the search. Spike.7 (WHY§235) follows the task past the wrapper run 6 found:
  `summarise_callee` knows what each register holds along the flow (where paths meet a register keeps only what both
  agree on, and code no known path reaches holds nothing), maps the arguments into every call and tail jump (`pass`),
  reads each branch's flag test before the first call (`guards`: the compare or test, past moves that set no flag, with
  its operands' kinds: a register and the argument it holds, a memory operand's base and displacement, a RIP-relative
  slot, an immediate), and takes an entry state, so that each callee of the wrapper is summarised with the wrapper's
  arguments followed into it (`entry_at`); `search_memory` also counts the words equal to our fake vtable's address and
  to its slots', reads up to 64 GiB (the time cap the bound) and walks past a cap for the committed total; and
  `walk_hub`, `search_walk` and `diff_hub` (`probe_hub.cpp`) keep spike.5's walk of the hub's reachable memory, search
  it, and compare two walks block by block by path, each changed word's old and new value by kind (zero, a small
  integer, the group's index, our task, our vtable, our module, the game's image with its section and RVA, the heap,
  other), a new pointer's block fresh or not, and, against an earlier diff, steady or not. Spike.8 (WHY§240) reads the
  tail function run 7 found: `summarise_callee` also follows values loaded from memory (a RIP-relative slot, or
  [base + index*scale + disp] with what base and index hold, at most 3 loads deep; a call's result stored to a
  RIP-relative slot holds that slot's value, and an inner summary's rcx to r9 that hold no argument their entry
  values), gives each guard operand's source, models a jump through memory or a register as a tail call site, ends a
  function with no extent at its first jump out that no branch goes past, and gives each summary's first 64
  instructions as forms (class and operand kinds, no bytes; `mark_sections` fills a RIP-relative slot's section);
  `match_table` checks a wrapper and its tail function against run 7's shapes (the wrapper's rcx at its tail jump, or
  past its null test where its paths disagree, `via_guard`) and `read_table` reads the singleton and
  its entries through a caller's guarded reader, kinds and indices only (`classify_value`, the diff's own); and
  `diff_hub` marks each changed word against a control diff's (`in_control`). `SysLayer::register_task` answers rax as
  the call left it. Spike.9 (WHY§248) reads the group's own function run 8 found: with no extent a direct jump past the
  decoded code is a tail jump, and `match_table` takes the tail function's direct jump with the entry in rcx
  (`target`); given our task's argument, `summarise_callee` reports every load from the task or from the group's entry
  (`EntryArg::entry`, set by `group_entry_at` and carried into callees by `entry_at(call, parent)`), or from a value
  loaded from either, with what that value is then used for along the flow, and every store of the task or of a value
  read from it, a followed value changed by add, or, xor, and, sub, a shift, a bit op or a lea staying followed
  (Source's `modified`); `search_memory` counts words whose low 48 bits are our object's or vtable's address under a
  non-zero tag, and `classify_value` names such a word `task_tagged` with its high bits; `read_table` can answer the
  addresses it read for T1 (the walk's roots, the classes), never reported; `match_lead` names a class against the
  leads' names as O92 does (derive's `innermost_class` gives the name). Spike.10 (WHY§289) reads the second
  singleton's method run 9 found: `method_site` finds the group function's site that passes our task with rcx loaded
  from a slot (a tail jump first), `method_entry_at` gives G's method its entry (`EntryArg::g`, `word`), `stub_jump`
  knows a stub and `through_stub` carries the state through it (`zero` where it zeroes a register), and
  `entry_at(call, parent)` carries G, the word and a zero into callees; `summarise_callee` lists the loads based on G
  (`g_reads`) and the uses of the entry's word (`word_uses`), a use as an index is `index`, and with `starts` (the
  exception directory's) a direct jump to a function's start goes past nothing; `task_argument` reads what a call site
  passes as the task (the function decoded from its start onto the call, read back to the register's last write);
  `diff_view` compares one of the array's views across the call; `read_table` answers every entry's address. Spike.11
(WHY§293) reads the request run 10 found G's method building: with `SummaryOptions` (the exception directory's entries
and, from `chained_entries`, the ones chained to a primary by UNW_FLAG_CHAININFO or by an entry pointing at another)
`summarise_callee` decodes a function with an extent in parts where its code goes past it (`parts`: its chained
entries, then the code its conditional branches reach past the extent in no entry, decoded linearly up to 4 KiB to a
return, a trap, a jump out of the parts, another entry's start or a part), a jump into a part being no tail jump, and
gives up to 128 forms then; a lea of rip or rsp makes an address (Source's `Address`, `Stack`), passed at calls, and the
stores to the stack frame are kept for `request_shape`, which finds the request (its rdata vtable at [rsp + d], the task
at +8, G's field as its manager, the entry's word, the zeroed register) and the function called with its address;
`request_entry_at` carries the request, its task, manager and word into a callee, whose loads from the request and the
manager are `request_reads` and `manager_reads`, a load of its task field the task; and `search_memory` counts the words
equal to the request's vtable (`request_vtable`), the game's with their addresses for T1.

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
a function table with chained and indirect entries, and, on request, raw data placed by hand, a file cut short and a
SizeOfImage of the test's own. They check the same facts from the file and from memory; every problem a malformed
image can have, with where, and for a section its rule, index and numbers; each raw layout the loader accepts (out of
order, overlapping, shared, empty, past the end of the file; sections of no size; a virtual size of 1 GiB; SizeOfImage
short of the last section's alignment) read as the loader maps it, through `read_mapped`, `ImageBytes` and the scan;
unreadable bytes in the source; the grammar's forms and refusals; the
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
registration with its own argument order, a table of UTF-16 group names, statics read by small functions, and decoys;
and R0c's variants: the path loaded after a format string, into rdx, through a pointer, by its absolute address,
hoisted before the head or before the getter's call, a decoy that only the idiom's wildcards match, a table of 16-byte
group entries holding their index, statics without RTTI, and a trademark sign in the product name):
the walk, the finder, the cutter's wildcards, the leads and their refusals, every derived fact against the program's own
labels, the decoys refused, the statics' patterns from both layouts, and the report holding none of the program's names;
the writer refusing what is outside its vocabulary and the lint refusing a path, a free string and a byte run longer
than a pattern. `test_probe_report.cpp` writes every part of R1's report, which lints clean, and refuses a label that
is not one, leaving no text. `tests/fakegame/` runs the Windows `eldennet-engine.exe` on `fakegame.exe` under Wine against what
`fakegame --describe` says.

The in-game part's tests (part 4) run in every build over `FakeSys` (`tests/fake_sys.hpp`: a process of byte regions,
copies that fail as an access violation would, released regions, declared stacks, a clock and a thread of the test's
own, and a hub's tick) and `FakeWorld` (`tests/fake_world.hpp`: an invented game of eleven singletons of invented names,
a registration of its own argument order, AOB statics, MSVC RTTI with a character base and its subclasses, a flag
tree, text and param tables, and its gal/ texts). They check the reader's rules, ObjView, the breakers' window and
the histogram, and a stale stack range forgotten (`test_guarded.cpp`); discovery: every item where the world put it, the getter called once a candidate
and refused after registration, the probe round's slot, the retries' times, optional and structural items missing, a
release build refusing leads (`test_discovery.cpp`); every snapshot field against the world, phase A and phase B, R11's
copies while loading, statics that move (as tombstones, released, kept), are nulled and repointed (R5), vtable swaps and
the three-tick rule, real faults tripping an optional group (L3) and a structural one (inert, then no copy at all), the
budget's gap, the first tick's group check, the render feed and C5 (`test_backend.cpp`); registration once with the
layout's registers, 4 Hz at 30, 60 and 144 FPS and with 1 s hitches and 3 s stalls (one snapshot an invocation at
most), the throttle in and out, the command cap, R3, and no allocation or free on the game thread after warm-up, with
this test program's own counting operator new (`test_task.cpp`, `alloc_count.cpp`); and the probe's searches
(`test_probe.cpp`: the slot searches, the heap slots and the module base's slot, the rising-counter search telling a
play time from a still field, the hub census to a depth of three past a released object; the callee's summary on code
of our own, hand-assembled: an argument kept in a callee-saved register and stored into another, a compare with a
bound, a virtual call on an argument's object, a refusal path that returns through no call, a RIP-relative store, an
immediate past the bound, an instruction it does not model, the 4 KiB bound and bytes that do not decode; and the
whole-memory search over regions of the test's own: the game's copies in its image, heap and a mapped view, ours in our
module, heap and task block, a guard page, an execute-only page and a stack never read, a page that faults, a range
kept out, both caps, and its copies counted as the game's when our heap's regions are not known; spike.7's on code of
our own: a wrapper's arguments mapped into two calls and a tail jump through the registers that keep them, a flag of
the instance tested before a branch past both calls, an inner callee summarised with the wrapper's arguments (the task
and its vtable kept in the hub, the group compared with a bound), two paths that disagree, a call reached only through
a jump the summary cannot follow, a compare found past a move and a byte register's high half; the hub walked, searched
and diffed over memory of the test's own (each kind of value, a fresh block, a block found again by another path, a
path re-pointed, a block gone, the caps, and a later diff's steady words first); and the search for our vtable's and
slots' copies, ours apart, with the committed total past a cap; spike.8's on code of our own: a wrapper of run 7's
shape whose singleton, loaded from a global on one path and made and stored there on the other, reaches its tail jump
on both, its guard's source and its forms; a tail function whose null test reads [entry rcx + group*8] and whose jump
through the entry's vtable is a tail call through memory, the padding after it unreached, and the table's shapes
matched (one level deeper too) or the first that failed named; a jump through a register loaded from a global, one
through a RIP-relative slot, loads not followed (16-bit, a segment's, a fourth level) and a function with no extent
ending at its jump out; the table's reads over memory of the test's own (the entries to the bound, the group's entry,
its vtable and slot, no entry, no singleton yet, a hop and an array, the instance); and the control diff's words
marked and ordered; spike.9's on code of our own: a tail function of run 8's shape whose direct jump past its decoded
code is a tail call to the group's function, matched as the table's `target`; that function with the entry and the
task followed in: its two task reads (one stored on the entry, one compared with a small immediate), its read of the
entry's count, the task's store in the entry's list as a tagged pointer (an or by a register) and the entry carried
into its call; a function reading the task's vtable and calling through its slot, a pointer read from the task and
read from in turn, fields compared and tested in place, a lea into the task stored to a global and a bit set in the
task stored through it; a diff word and the whole-memory search's words holding our address under a tag (ours apart,
our vtable's too, none for a tagged pointer inside our object); spike.10's on code of our own: a group function of run
9's shape (its site the tail jump, not the report's call), its stub (r9 zeroed, G and the word carried; one whose
target follows it ends at its jump only with the starts) and a method of G's with its task reads, three reads of G,
four uses of the word and its call carrying both; a call site's task as a load, a member, a result, a static and a
stack slot with a branch landing on the call; a view's changed words with the threads' ids; a class's innermost name
against the leads' names; the
table's addresses for T1; and the report of all of it, lint-clean; spike.11's (`test_probe_request.cpp`) on code of our
own: a method of G's of run 10's shape building its request on the stack (its five fields and roles, the function it
calls with its address, no request where the vtable is not in rdata or the task not at +8), that function in two parts
(an unlisted second part past its extent with branches back into the primary, a chained one instead, an entry chained to
no one, and a jump out of the parts as a tail jump) with the request's task, word and manager followed and carried one
level further; the chained entries of an image of our own; the search for the request's vtable; and the report of all
of it, lint-clean; spike.12's (`test_probe_map.cpp`) on code and trees of our own: a now path's function of run 11's
shape (its list walked with a word of 0, its map with any other) and the map's layout derived from its summary (the
head, the root, the nil byte, the key from the word's compares, the match's call and the other branch's, the list's
begin and end; the value 8 past the key where the summary knows nothing in rcx, else its load's), nothing derived
without the request's manager field, the word's compares or the call; the match's call entered with the map's value,
its load a value read; MSVC-shaped trees walked in order with the word present or its nearest keys, an empty map, and
the walk's refusals (a cycle, a nil head, a node off the heap, a node's nil byte set, a head off the heap, no manager,
4,097 nodes against 4,095 walked, a list out of order and none); and the map in the report, pretty and on one line,
lint-clean). The vocabulary's check that every key is given once compares the names by a hash first (spike.12), so
that it stays within a compiler's constant-evaluation steps. The clang-cl build adds
NativeSys's (`test_native_sys.cpp`): the module's image, both copy primitives, the stack table and a guard page left
untouched, the rescan without the calling thread's stack and a range forgotten once its guard page is gone, the calls
under SEH with the registration's answer kept, and the task object driven as a hub would drive it. In game, `tests/galtest/` is a DLL that the fake game loads, and the harness's `gal-*` cases check what
it finds against `fakegame --describe` (`tests/fakegame/README.md`).

The fuzz targets `en_fuzz_pe` (the reader and the scan over arbitrary images, in both layouts, with an unreadable page
and small chunks), `en_fuzz_pattern` (the grammar, and the matcher against a plain search), `en_fuzz_galtoml` (the
three schemas), `en_fuzz_x86` (the decoder at every offset: its parts within the instruction and in order, nothing after
it changing it, every shorter run truncated), `en_fuzz_report` (the lint on any text, and the writer driven by the
input: what it writes always lints clean and holds no string offered that is not a word or a label) and
`en_fuzz_derive` (the derivation over any executable: its report always passes the lint) are in `fuzz/`.
