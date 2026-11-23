# en-gal: the game access layer

The part of EldenNet's engine that reads the game (ARCH §6), C++20, MIT. Three static libraries, each a module the DLL
may link (`en_dll_module`):

- `en_gal`, the offline part, which builds with every compiler: the PE reader, the patterns, `gal/`'s schemas, the
  derivation kit that `eldennet-engine probe-exe` runs (part 3), and, with clang-cl,
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
  gives the object) and `call_relative` (gives its target); and, for the accessor's other shapes, `skip_one` (any one
  instruction that falls through and does not load the remembered register anew: a call, a jump, a return, or a
  load, an address or a widening move into that register is refused) and `compare_slot` (a 64-bit compare of a
  RIP-relative slot with zero where it lies; gives the slot, leaves nothing in a register, and a branch form may
  follow it). Each form is one instruction of the architecture's own
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
  refused), `gives` (which step gives the `static`, the `class` and the `getter`, each of the form that gives it; the
  static by a `load_slot` or a `compare_slot`), its
  `path_suffix`, and, as a pattern has, `section`, `expect_matches` (a range up to 65,536), `builds` and `provenance`
  (`own-derivation`). No key of a recipe can hold bytes. `RecipeSpec::accessor()` is what the finder takes. Discovery
  takes the file's text in `DiscoveryOptions::recipes`: with an `accessor` recipe (`roles::kAccessorRecipeId`) the FD4
  singletons are found by its shape and `patterns.toml` needs no `fd4_idiom`, nor is one read; a text that does not
  read is a refusal. On the tests' fake world discovery finds the same items, sites and names by either. The DLL and
  the test DLL read `recipes.toml` when the folder holds it, and a folder with it and no `patterns.toml` is read as
  having no pattern. A file may hold more recipes of the accessor's kind under other ids (the accessor's other
  shapes: `accessor_near`, `accessor_gap`, `accessor_compare`, `accessor_compare_near`, each a lead with its note
  and its count on 2.7.2.0; a fifth stood nowhere there and was withdrawn): discovery reads `accessor` alone, and Lodestar's finder counts the others beside it.
  The repository's `gal/` holds the accessor recipe with its note, and no `patterns.toml` any more;
  the pattern reader stays for the fake game's generated folder, and for the statics' and the function's patterns,
  until discovery goes through the table (the proposal's L3 and L4).
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
  those sites load; the pattern is never written. `complete` says the accessor has a recipe, its count within range, an accepted site, one class object
  for each static and the reverse (`shared_class_objects`, `shared_statics`), and no site that the shape alone
  accepts; a site the pattern alone accepts is reported with its reason and does not count against the run (on
  2.7.2.0 there are 4, each a test of another register than the one loaded: the pattern took another load for the
  manager's slot there). Every other recipe of the accessor's kind is a variant, run the same way and reported under
  its id (`AccessorFound::variants`): its hits, the sites accepted and refused, the statics they reach, how many of
  those the accessor's own sites reach too (`known`) and with the same class object (`agreeing`), and the statics
  that are new, each with its class object, count of sites and witnesses; `all_statics` is the accessor's own and
  then the variants' new ones, which the naming goes through. A variant never changes the accessor's own counts or
  `complete`. `write_report` writes kind `lodestar`, schema 15, through the numbers-only writer: whole
  numbers, booleans, fixed words and the leads' labels. Its vocabulary holds no key of the pattern type and no
  fraction, which a test pins, so a report cannot carry a byte, a pattern, text, a name or an instruction.
  **The in-game round (step L5; proposal 0005 §6.1's `instance_class`, O92)** adds, all read-only:
  `Namer(image, source, leads, ids)`, which names the managers. It reads the image's type information once; then
  `pass(live, accessor, read_word)`, as often as the round likes, reads for each static the accessor's sites load
  the instance it holds as the game runs (8 bytes through `live`) and that instance's first word through the
  caller's guarded read (one word, never at an address inside the image), and takes the word's class from what it
  read at the start. A lead names the one static whose class has its name, or its name with Imp; where no static
  does, the one whose class derives from such a class (`by_base`); and where the image has no class of the lead's
  name at all, the one whose class has a name that holds the lead's (`by_related`, weaker and marked; the third
  round of 2026-11-22 found the world's clock has only such a class); two that match the same way leave it ambiguous.
  `name_instances(...)` is one pass of a namer made for the call. `lead_classes()`, offline too and in every
  report, counts what the image holds under each lead's name: classes of that name and of that name with Imp, the
  tables of methods of those and of every class deriving from them, and the places where a type's name of either
  form stands at all, and the classes whose name holds the lead's and is neither (`related`: a manager that goes by
  a longer name). So a reviewer can tell a manager that is not there from one under another name, or one with
  no type information; the round of 2026-11-22 could not (`docs/ops/probes/2026-11-22-l5.md`). A lead's label is the catalogue's, or the one it had before (`was`, which `gal/ids.toml` records for
  the managers and the two classes). `merge(held, later)` folds a later pass in: a manager the game makes only once
  a save is loaded is null at the title; the counts and what each static held (`held`, every static's object by
  its class, a number; `new_statics`; `others`) are the fullest pass's, since the last pass of a session is made as
  the game closes; what was named stays named, with the pass that named it, and what the
  searches found for a manager stays with it through every later pass (the second round of 2026-11-22 lost it
  there). A pass goes through `all_statics`, marks a manager behind a variant's static (`variant`), and counts for
  each lead the catalogue does not hold how many statics hold an object of its class (`others`: a lead on trial).
  `set_by_search(held, id, result)` takes a slot search's result: the slots found name the manager when they hold
  one object between them (one slot, or several that are copies of one address; the first by offset is taken), two
  objects leave it ambiguous, and the search's own numbers (runs, candidates, checked, unreadable, the slots) are
  kept either way; `set_in_data` records the objects of its class in the image's own data.
  `references(image, source, section, rvas, max)` counts, for each slot, the instructions of the code whose
  RIP-relative operand is the slot, and gives the first places (each found by reading on from its function's start,
  as the resolver reads a witness); `set_search_refs` then makes the slot the code refers to most the manager's,
  with those places as its sites and witnesses, so a manager no accessor loads has witnesses too. A pass also says
  what each static only a variant reaches holds (`new_statics`: nothing, unread, unclassed, or classed with its
  table of methods and whether a lead asks for it). The report's
  `instances` object carries counts, numbers and the catalogue's labels: the classes' names never leave memory.
  `draft_table(found, ids, provenance, image, source)` writes the findings as `[provenance]` and `[item.*]` entries
  (managers, the leads' two classes, the groups' table), each checked first as a start will check it: an entry whose
  placement fails is left out, a witness that refers elsewhere is dropped, `checks` lists what passed, and every
  entry is a lead until its review.
- **`lodestar_trial.hpp`: the reader's trial** (in the part that runs in the game: it holds a `GuardedReader`, whose
  shape depends on the probe option). `Trial` is a task body for the frame service's own thread: about ten times a
  second it reads each named manager's slot, its instance's first word, and the slot again, and counts what was
  null, what still had the class it was named by, what had another, what faulted (caught) and what changed between
  the two reads (`torn`, which judges nothing). Managers are added by the naming thread at any time; nothing is
  removed. With `set_stage(id)` a reading made while that manager has an instance counts as one in the world
  (`world_readings`, `stage_changes`), and each manager's empty readings are counted in stretches (`null_runs`,
  `null_longest`) and by stage (`null_world`), so a report says when a manager was away, not only how often. `found()` gives the counts from any thread, with the readings' p50, p99 and longest, for the report's
  `trial` object. `ClockSearch` looks, in the first bytes of the objects behind the statics no lead named, for a
  field that behaves as a clock's minutes do with one beside it that behaves as hours. `MemberSearch` looks for a
  lead's object that no slot of the data points at: it copies the first bytes of each static's object (0x4000 at
  most, less where its memory ends sooner) and, for every 8-aligned word after the first, takes a table of methods
  of a lead's class as an object lying there itself (`embedded`), and follows what could be a heap address by one
  8-byte read; a hit is a static and an offset into its object, for the review and never a name. (It copied 0x800
  bytes until the third round found nothing that near; now the reader's longest copy, 0x4000.) A pass goes
  through every static once and the next starts 30 s later. Both read through the guarded reader and allocate
  nothing on the reader's thread. `Watch` is step L6's first tool: a way leads to an object (a static's slot, an
  offset added to the address it holds, and for a pointer kept there one more read); once a second each way is
  followed and the object's first 0x200 bytes copied, and from one copy to the next every 8-byte word's halves are
  counted for changes and rises, and now and then the word is looked at (nothing, a small number, a table of
  methods, an address in the image, an address of live memory, something else). A way marked for a clock also has
  every run of 6 bits watched for minutes and of 5 bits for hours (a clock packed into one word; the hours are also
  judged alone, whatever the minutes' pace), and every 32-bit field read as a fraction or as a whole number for a
  steady rise: a rise far above the field's pace, or a fall, is a jump, which is what a clock does when the player
  lets time pass; a field's or a run's first three jumps are kept with their second and the number before and
  after (`RiseHit`, `HourHit`, `JumpEvent`). Two ways of one label say how often they arrived at one object
  (`same_address`). Apart from those jumps it keeps counts and offsets, never what a word held. A way may ask for a
  longer span (to the reader's longest copy, 0x4000; past its first 0x200 bytes only words that changed are
  written), a slower pace, and one step more (`deeper`: the object whose address is kept `via` bytes into the one
  the way led to); and many may be `wide`: every static's object watched only for a clock, followed after the plain
  ways (less often once it has proved still) and listed only where something was found. In the report a word's
  counts of nothing are left out. `Sweep` goes through longer stretches (to 2 MiB a target, a piece of 0x4000 bytes
  a call, a pass every 30 s at most) from the address a way leads to, compares each piece with its copy, and keeps,
  page by page, the single bits that flipped in words and pages that otherwise kept still (`SweepFlip`: the place,
  the bit, the direction, the second; nothing of what the memory holds): a flag set by something the player does.
  `Sweep::mark` takes the moment the tester confirmed a task: the flips seen from 15 s before it to 60 s after are
  kept under that mark (`MarkFlip`), and a pass starts at once; a target of which more than 1,024 words change again
  and again is read no more (`retired`). A wide way is listed only where minutes went round twice with hours beside
  them, and the round names that static the world's clock by the invariant recipe. `TreeRead` walks the tree beside
  the flags once (records of 64 bytes reached by their first three words, 32,768 at most, nothing allocated while
  stepping), counts each of a record's sixteen 32-bit fields over all records, finds the key by the tree's own order
  (a left child's is smaller, a right child's greater), and joins a marked flip to the record that names its block
  by the block's start, index or address (`TreeFound`, `TreeJoin`); `settle` tells addresses from numbers that look
  like them before anything is counted, and an address is never written. `StateSearch` finds a static by what a
  field of its object holds in two states of the game the tester moves it between: over the data's slots that point
  at live memory, the first 0x2000 bytes of each object in four passes (asked for one by one, at the tester's
  marks); a 32-bit field that reads -1, a number from 0 to 15, -1 and such a number again is a hit (`StateFound`),
  with both numbers, and hits that are one field of one object seen through several slots are numbered alike
  (`same_as`). The twelfth round hit 15 slots with the same save loaded twice, so the thirteenth has the tester load
  two saves: the round names the game's state by the invariant recipe where one field in one slot alone read two
  numbers, or, with one save, where one slot alone was hit. Each pass says how long it took and whether the next
  was asked for before it was through. `CharSearch` (lodestar_chars.cpp, `CharFound`) is the second step of the
  layout for a ready start, the pointers and lists by search and class: in looks asked for as the state search's
  are, it reads the characters manager's object and what follows it (0x40000 bytes at most), follows each address
  in it once (the first 0x100 bytes behind it; 8,192 at most) and tells by the first word whether an object of a
  character class lies there (the two classes' tables of methods, the player's told apart). It reports the words
  that hold a character's address, the pairs of neighbouring addresses that bound a run of elements holding
  characters (the element's size, the address's place in it, and a fraction that never falls along the run: the
  list by distance), the addresses of memory that holds the main player's address (the player set), and, for the
  main player's object and the save data's, each address in it with its target's table of methods and how many
  addresses the target begins with (a holder of parts: at least 8 of its first 16 words are addresses; how many of
  those objects have a class is counted). It names nothing; no value of any object is reported, and nothing is
  allocated on the stepping thread. The fourteenth round asks for its look only once the tester says the game is in
  the world (a look asked for as the manager was first named saw it being filled). `PairSearch` (lodestar_parts.cpp,
  `PairFound`) finds a field by two numbers it must read: the owner's save file gave the characters' slots, so the
  save's slot is the 32-bit field (or the byte) that reads the first loaded character's slot in the first world and
  the second's in the second, over the data's slots in use in the world, with no rule about the title and none about
  the object staying; the round names the game's state where one 32-bit field alone did. `PartsWatch` (`PartsFound`)
  is the third step, the moving numbers by invariant: once a second it reads the main player's address from the
  manager, the holder's from the player's object, the holder's 32 words and 0x1000 bytes behind each, compares
  every 32-bit field with the pass before and counts the changes by the phase the tester is in (standing, walking,
  standing, a hit, a heal: the launcher's marks, the waits' ends among them). It reports the parts (each one's
  table of methods), the fields that changed in at least half of the walk's passes and never while the tester stood
  (a position), and the fields that fell in the hit's phase having never changed before, with how often they rose
  in the heal's and whether a field beside them never changed and is at least as large (health and its maximum).
  What a field held is never reported. `ErBackend::flags_alone`
  and `DiscoveryOptions::partial` (backend.hpp, discovery.hpp) let one group of EldenNet's own reader be read before
  the others' fields are placed.
  The round watches each named manager, the objects the event flags' store's first addresses lead to and the
  objects those lead to (and sweeps the stretches all of them start), each place the member search found a
  pointer to an unnamed lead's object at with the object that holds it (0x4000 bytes of both for the world's
  clock), the object whose field rose by the clock, and, as wide ways, the object behind every static. That field's slot is also a static found by what its object holds (`InvariantFound`:
  proposal 0005's `invariant` recipe), which `draft_table` writes with the code's references as its witnesses. A
  pass that names a manager the search named first takes the entry over (`merge`).
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
    `depends` (the in-game round names them, by their instances: O92);
  - the vtable sets and the task classes (their slots, how many functions each slot holds across them, and the one slot
    every class fills with its own: the run slot's candidate; `task_from_base` does the same for a base given by its
    type descriptor);
  - `register_task`: the direct calls after a load of the registration's instance, whose arguments a small register
    model follows back to the start of the block (the instance, a 64-bit argument, a 32-bit one), grouped by callee;
  - the task group's index: its name, as ASCII or UTF-16, in a table of entries holding pointers to names at a stride
    of 8 to 64 bytes (the stride with the longest run), cut where the code addresses an entry's start, with the name
    pointer's offset in the entry and an index field the entries may hold; or beside a small constant where the code
    loads it. A lone pointer (a table of one) is listed last and gives no index;
  - each AOB static's class vtable, for the in-game search, and how many type descriptors carry the class's own name
    in any namespace; given the slot found in game (`Options::static_slots`), a pattern at a reference to it. A static
    whose class has no RTTI cannot be found offline: its slot is zeros in the file and nothing names it, so the in-game
    round finds it by what its instance holds;
  - every pattern cut by `cut.hpp`'s rules: whole instructions, at entry + 5 or later inside a function of the exception
    directory that is no fragment, at most 64 bytes, the RIP-relative and other 32-bit displacements, the branches whose
    targets lie outside the pattern, the relocated bytes and the immediates that do not fit in a signed byte
    wildcarded; unique in `.text`, preferring 20 bytes with 12 fixed and the smaller function; then checked again by
    `scan_patterns` as patterns.toml would hold it;
  - the diagnostics R0c added: per reference to a path, what the site search found; each head shape's sites; where
    each site's and each accepted hit's path load sits; R0's rule over the same hits; what the class objects hold in
    the file; and every executable section decoded through (instructions, bytes that do not decode, their runs and
    where they start, references to the paths);
  - and the list of what the in-game round must confirm. `complete(result)` says whether everything R0 can derive was
    derived.
- **`report.hpp`: the numbers-only writer and its lint.** A report is one JSON object whose keys all come from its
  kind's closed vocabulary, each with one type of value: whole numbers, booleans, fixed words, labels (`[a-z][a-z0-9_]*`),
  patterns of at most 64 bytes in canonical form, objects and arrays, and, for a key typed so, finite numbers that need
  not be whole (`Float`: a recording's positions; no derive key takes one). The `Writer` refuses anything else, and its
  first refusal leaves no text; it writes pretty-printed or on one line (`Style::Line`, a line of a JSON Lines file).
  `lint(text, vocabularies)` checks a file token by token (a repeated key cannot hide a string), with problems that say
  where, never what; `lint_lines` checks a JSON Lines file whose every line is one report of the first line's kind
  (eldennet.dll's recordings), each problem with its line. `derive::write_report` writes derive.json through it, under
  `derive::vocabulary()`; eldennet's en_shell holds the `recording` vocabulary.

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
- **The frame service: a reader on a thread of our own** (proposal 0005 §9.4, §10; O101 (b); `task.hpp`,
  `backend.hpp`, `sys.hpp`'s `FrameProvider`). v1 reads the game from a thread of EldenNet's own, with no call into
  the game and nothing written to it; the game's own task stays a later service. `ReaderLoop(sys, body, tick_us)` is
  that provider's loop: each `step()` runs the body once through `run_task` (the one place that issues the reading
  thread's token), with the clock's time and the time since the last step, and answers how long to wait before the
  next (the tick less what the step took, 1 ms at least). The DLL owns the thread; the tests step it on FakeSys.
  `own_thread_options()` gives the task's settings for it (a snapshot about ten times a second, five while
  throttled, and no group check at the first tick: such a reader is in no group of the game's), and
  `own_thread_backend()` turns on **steady reading** (`BackendOptions::steady`), for a reader that is not in step
  with the frame: (1) every static's instance, every object's table of methods and the near list's two ends are
  remembered as the snapshot is read and read again when it is built, and one that changed drops the reading
  (`ReadError::Unsteady`, counted as `torn`); (2) a change of the lifecycle (the main player there or not, the
  screen) is published at its second sighting running, the first reading that shows it dropped (`held`), so a blip
  of one reading is never published; (3) a group's hard fault counts toward its breaker only when the group faulted
  in the reading before too (`unconfirmed` otherwise). A dropped reading counts against no group, and the next
  snapshot carries `gap`. Phase B and a playthrough's start are concluded only from a reading whose structure held.
  `DiscoveryOptions::frame = FrameProvider::OwnThread` makes discovery ask for no registration function, no task hub
  and no `[task]` in the layout, and ready then means ready to read. `TaskStatus` carries `torn`, `held` and
  `unconfirmed_faults`. With it the FD4 getter is never called either: a site without a name stays unnamed, so names
  come from the name layout or from the build's table. eldennet.dll starts this way (`boot.cpp`: T2, the reader);
  `install_task`, the game's task, is no part of a v1 start, and stays for the later task service (proposal 0005
  §9.6); nothing of EldenNet's calls it now, only the tests and galtest.dll (R1, retired 2026-11-22, O106).
- **Discovery through the build's table** (proposal 0005 §7.1, §9.1; `discovery.hpp`, `resolve.hpp`). A layout of
  schema 2 that holds `[item.*]` entries is loaded against the catalogue (`DiscoveryOptions::ids`, gal/ids.toml's
  text; refused without it), and `Library` checks each entry against the image as loaded (placement, witnesses).
  Each role whose id the table holds takes its place from there, `ItemResult::from_table`, and no pattern, recipe,
  slot or walk looks for it; an entry whose check fails is refused alone (`why` is the check's word, such as
  `witness_elsewhere`), its place handed out to no one, and a required one stops the start. A class the table holds
  gives its vtable set, to the entry. What the table does not hold yet is found as before, so the table can grow id
  by id; with every static in it, a start scans for nothing. `DiscoveryReport::table_items` and `table_refused`
  count them.
- **`probe.hpp`: the probe option** (`EN_PROBE` only). `SlotSearch` finds a static's slot as the data slot whose
  instance has its class's vtable (candidates read on T1 from the image's writable data, `heap_slots`, checked on the
  game thread within the tick's budget), for a layout class or for a vtable set of the caller's (before any
  discovery), or over candidates the caller collected; it keeps the slots found with the object each held
  (`found_objects`: two slots with one object are copies of one address) and how many there were in all.
  `heap_slots` takes an optional question of what is live at a value (`LiveFn`: memory that may be read, none, or
  memory that may not be read; on Windows the caller answers it with VirtualQuery, which reads nothing): only live
  values are kept, and the rest are counted by the answer. Most 8-aligned values of a game's data that could be a
  heap address are not one (in the second round of 2026-11-22 about 97 in 100 were refused when asked). `slots_holding` finds the data slots
  holding a value (`global_hinstance`: the module's base);
  `RisingSearch` copies each candidate's instance again a second or more after its last copy and counts, for every
  32-bit field, the passes it rose in by the elapsed milliseconds (within a fifth), and from its first rise those it
  stood still in or changed otherwise (a play time stands still at the title and while loading; a candidate that
  faults is not copied again); its hits are the 32 fields that rose in the most passes, chosen again as each pass
  ends (the best, not the first met), and a sweep that copied nothing is no pass; for
  a static whose class has no RTTI; `ObjectScan` answers where an object is that nothing known points at: the
  process's private, plainly writable memory (outside the image and the caller's own heap; never a stack) is gone
  through once, 16 KiB at a time, for a word that is one of the targets' tables of methods (a node of level 0),
  then up to three times more for words that hold the address of a node of the level before; each holder's object
  is taken to start at the nearest word before it that is some class's primary table of methods (within 16 KiB, in
  the same region), and a level-0 node that nothing holds is looked at the same way (`embedded`). It asks for each
  region first and again after every mebibyte of it (`RegionQuery`; VirtualQuery in the round), so its reader need
  not ask before each piece. (The fourth round of 2026-11-22 ran it: it found what it was for, read 19.75 GiB in 13
  minutes without finishing, slowed the reader and made the game stutter; the round no longer starts it. Before it
  is used again it must read only pages already in use, check its time after every piece and stop after one sweep.)
  A node is a class, an offset and an address that is never reported; the caller
  compares each node's address with the image's data, where a chain ends. Every node is a lead for a review: a
  found object may be stale, and an owner a neighbour; `vtable_offsets`, `meter_offsets`, `utf16_offsets` and
  `u32_offsets` search one copied object's bytes by invariant and answer offsets only; `searchable` says which regions
  a search reads (readable, not a guard page, no-access, uncached or write-combined), and NativeSys's `region_at`
  (probe builds) is the VirtualQuery behind a caller's question; `GuardedReader::inject` makes every n-th read of a
  group a real fault; `SysLayer::slot_calls` counts the game's calls of each slot, and `SysLayer::task_object` gives our
  object's address.
  Everything they need on the game thread is allocated on T1. What R1 added on T1 around its one call into the game
  (the hub census, the callee's summaries, the whole-memory search, the hub's walks and diffs, the table, the request
  and the map, with `probe_report.hpp`'s report of them) went with it: R1, retired 2026-11-22, O106.

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
than a pattern. `tests/fakegame/` runs the Windows `eldennet-engine.exe` on `fakegame.exe` under Wine against what
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
play time from a still field and keeping the fields that rose most, the object scan level by level, and which regions a
search reads). The vocabulary's check that every key is given once compares the names by a hash first (spike.12), so
that it stays within a compiler's constant-evaluation steps. The clang-cl build adds
NativeSys's (`test_native_sys.cpp`): the module's image, both copy primitives, the stack table and a guard page left
untouched, the rescan without the calling thread's stack and a range forgotten once its guard page is gone, the calls
under SEH with the registration's answer kept, the regions as VirtualQuery describes them, and the task object driven
as a hub would drive it. In game, `tests/galtest/` is a DLL that the fake game loads, and the harness's `gal-*` cases
check what it finds against `fakegame --describe` (`tests/fakegame/README.md`).

The fuzz targets `en_fuzz_pe` (the reader and the scan over arbitrary images, in both layouts, with an unreadable page
and small chunks), `en_fuzz_pattern` (the grammar, and the matcher against a plain search), `en_fuzz_galtoml` (the
three schemas), `en_fuzz_x86` (the decoder at every offset: its parts within the instruction and in order, nothing after
it changing it, every shorter run truncated), `en_fuzz_report` (the lint on any text, and the writer driven by the
input: what it writes always lints clean and holds no string offered that is not a word or a label) and
`en_fuzz_derive` (the derivation over any executable: its report always passes the lint) are in `fuzz/`.
