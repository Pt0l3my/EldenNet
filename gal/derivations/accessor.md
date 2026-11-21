# accessor: the managers' accessors, as a shape

*A lead on 2.7.2.0 WW, written 2026-11-21 from what the offline round R0c measured on the owner's own executable
file; counted on that executable in no run yet, and confirmed in no build.*

**What it finds.** ARCH §6.2's finder: the game reaches each of its managers (the FD4 singletons) through a small
accessor, inlined at every use. The accessor loads the manager's slot; when the slot is still empty it hands the
manager's class object to a helper and reports the use, naming a source file whose path ends with `FD4Singleton.h`.
Every place where it stands gives three things: the slot (a static), the class object and the helper (the getter).

**The shape.** Five steps, each one instruction, each starting where the one before ends, in the grammar of
`shape.hpp`: a 64-bit load of a RIP-relative slot into a register (`load_slot`, which gives the static); a 64-bit
test of that same register with itself (`test_loaded`); a branch forward with an 8-bit distance, taken when the
register is set (`branch_set_short`); the address of a RIP-relative object into the first argument's register
(`address_arg1`, which gives the class object); and a relative call (`call_relative`, which gives the getter). The
path is not part of the shape: the finder looks for its load in the block the branch skips, or just before the
shape, as it did for the pattern (WHY§180). The shape says nothing of which registers or which distances a place
uses, and holds no byte of the game.

**Where it comes from.** R0c (2026-11-16; `docs/ops/probes/2026-11-16-r0c.md`) found the path's text in the image,
every instruction that loads its address, and, decoding back from each, the accessor's head. It grouped the places by
the head's form: 7,095 places in 6 forms, the largest holding 7,069. That largest form is this shape: its five
instructions have the lengths 7, 3, 2, 7 and 5 bytes, 24 in all, with the slot's, the class object's and the
getter's references where the shape's first, fourth and fifth steps give them. Until 2026-11-21 `gal/patterns.toml`
held the same head as a pattern of 24 bytes, 7 of them fixed, cut from the game's code by our own kit; O102 withdrew
it (no bytes from the game in `gal/`), and this shape is its replacement.

**How the two compare.** The pattern fixed only the bytes every place shares and left the rest to the finder, which
decoded each hit and refused what was no accessor (70 of its 7,151 hits). The shape asks for more at the outset: the
load must be 64-bit, the test must be of the register just loaded, and the branch must go forward. So what the
pattern hit and then refused, the shape should never hit. On our own invented programs (en-gal's tests, as files and
as mapped) the shape's sites are exactly the pattern's accepted ones, with every place, verdict and name the same,
and discovery finds the same managers by either (WHY§308). On the game itself the shape has not been counted yet.

**How it is to be confirmed.** Step L3's offline run counts the shape on the owner's executable: its hits, and its
accepted sites against R0c's 7,081 and the 217 class objects and 219 statics they reached. A difference in the
accepted sites is a finding to read before anything relies on the shape. The other five forms R0c saw (26 places)
are not covered; if a manager EldenNet needs is reached only through one of them, it gets a recipe of its own. Then
`expect_matches` takes the counted range, the in-game round checks the shape over the image as loaded, S0-A's
launches confirm it with the self-tests, and only then does the recipe list the build in `builds` (ARCH §6.3).

**Evidence.** R0c's report (numbers and our labels only), reviewed and recorded in
`docs/ops/probes/2026-11-16-r0c.md`. Nothing here comes from another project (O48): the accessor is ARCH §6.2's
description, the shape's forms are the processor's documented encodings, and the measurements are our own kit's.
