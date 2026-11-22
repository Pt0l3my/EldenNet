# accessor_compare_near: the accessor that compares the slot where it lies, with a 32-bit branch

*A lead, written 2026-11-22 from R0c's counts and the second in-game round; counted on 2.7.2.0 WW in the third
round the same day, and confirmed in no build.*

**What it finds.** `accessor_compare`'s shape with the branch in its longer form (`branch_set_near`), for the reason
`accessor_near` gives: a 64-bit compare of the slot with zero, the branch with a 32-bit distance, the class object's
address and the call. The first step gives the static, the third the class object, the fourth the getter.

**Why it may exist.** Both reasons at once; it costs nothing to count.

**What a variant is for.** It does not replace `accessor`: discovery reads that recipe alone, and its counts stand as
they were. The finder (`eldennet-engine lodestar find`, and the in-game round) runs each variant beside it and
reports, under the variant's id: where its shape stands, how many places the finder accepts (each must load the same
path as any accessor, or it is refused), how many statics those places reach, how many of them the accessor's own
places reach too and with the same class object, and the statics that are new, each with its class object, its count
of places and its first places. The in-game round then names over the new statics as over the 217 (O92).

**Counted on the game** (2026-11-22, the third read-only round, over the image as the game runs it;
`docs/ops/probes/2026-11-22-l5.md`). 4 places, all accepted, reaching 2 statics, both among the accessor's own 217 and
each with the same class object. It adds no static; it stays as a cross-check. `expect_matches` holds the count.

**How it is to be confirmed.** By its count on the owner's own executable first: a variant that stands nowhere is
withdrawn; one that reaches no new static is kept only as a cross-check. A new static enters a build's table only as
the 217 did: named by its instance's class in game, checked against the image, and a lead until S0-A's launches and
the self-tests confirm it (ARCH §6.3). `builds` stays empty until then.

**Evidence.** The shape's forms are the processor's documented encodings; that such places exist is R0c's own count
of the references to the accessor's path (`docs/ops/probes/2026-11-16-r0c.md`, "What R0c leaves for R1": 26 places
with five other heads, 1,379 references after a 64-bit load of a static, 676 after a compare of a RIP-relative
quadword with zero). Nothing here comes from another project (O48), and no byte of the game is in it (O102).
