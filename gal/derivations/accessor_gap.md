# accessor_gap: the accessor with one instruction between its load and its test

*A lead, written 2026-11-22 from R0c's counts and the second in-game round; counted on 2.7.2.0 WW in the third
round the same day, and confirmed in no build.*

**What it finds.** The accessor of `accessor.md` with one more instruction: a 64-bit load of the manager's slot
(`load_slot`, the static), then any one instruction that falls through and does not load the same register anew
(`skip_one`), then the test of the loaded register, the short branch, the class object's address and the call. The
first step gives the static, the fifth the class object, the sixth the getter.

**Why it may exist.** The compiler is free to put an unrelated instruction between a load and its test (a move of an
argument, a store). The accessor's shape asks for the test right after the load and so does not stand at such a
place. Our own research notes say one manager's accessors are written so (ER03, a lead and no more); the second
in-game round found that manager's object behind slots that are none of the accessor's 217 statics
(`docs/ops/probes/2026-11-22-l5.md`).

**What `skip_one` refuses.** A call, a jump or a return (the step must fall through), and a load, an address or a
widening move into the register the first step loaded: the test would then be of something else than the slot's
value, which is what the withdrawn pattern got wrong at 4 places (`accessor.md`).

**What a variant is for.** It does not replace `accessor`: discovery reads that recipe alone, and its counts stand as
they were. The finder (`eldennet-engine lodestar find`, and the in-game round) runs each variant beside it and
reports, under the variant's id: where its shape stands, how many places the finder accepts (each must load the same
path as any accessor, or it is refused), how many statics those places reach, how many of them the accessor's own
places reach too and with the same class object, and the statics that are new, each with its class object, its count
of places and its first places. The in-game round then names over the new statics as over the 217 (O92).

**Counted on the game** (2026-11-22, the third read-only round, over the image as the game runs it;
`docs/ops/probes/2026-11-22-l5.md`). 675 places, all accepted, reaching 89 statics: 87 of the accessor's own 217, each
with the same class object, and 2 new ones (one with 7 places, one with 1 whose class object lies 7 bytes before the
static itself, which is odd and is taken for nothing until a round says what that slot holds). Neither new static
holds a manager the leads ask for. `expect_matches` holds the count.

**How it is to be confirmed.** By its count on the owner's own executable first: a variant that stands nowhere is
withdrawn; one that reaches no new static is kept only as a cross-check. A new static enters a build's table only as
the 217 did: named by its instance's class in game, checked against the image, and a lead until S0-A's launches and
the self-tests confirm it (ARCH §6.3). `builds` stays empty until then.

**Evidence.** The shape's forms are the processor's documented encodings; that such places exist is R0c's own count
of the references to the accessor's path (`docs/ops/probes/2026-11-16-r0c.md`, "What R0c leaves for R1": 26 places
with five other heads, 1,379 references after a 64-bit load of a static, 676 after a compare of a RIP-relative
quadword with zero). Nothing here comes from another project (O48), and no byte of the game is in it (O102).
