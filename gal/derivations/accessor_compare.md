# accessor_compare: the accessor that compares the slot where it lies

*A lead, written 2026-11-22 from R0c's counts and the second in-game round; counted on no build yet, and
confirmed in none.*

**What it finds.** An accessor that does not need the manager in a register: a 64-bit compare of the RIP-relative
slot with zero, in memory (`compare_slot`, which gives the static), then the short branch taken when the slot is set,
the class object's address into the first argument's register and the call. The first step gives the static, the
third the class object, the fourth the getter.

**Why it may exist.** Where the code only asks whether a manager is there, or loads it again later, the compiler
compares the slot in memory and loads nothing. R0c counted 676 references to the accessor's path that follow a
compare of a RIP-relative quadword with zero; the accessor's shape starts with a load and stands at none of them.

**What `compare_slot` asks.** A 64-bit compare (a 32-bit one is refused as narrow), of a RIP-relative operand, with
an immediate of zero (8 or 32 bits wide): a compare with another number asks another question than whether the
manager is there. Nothing is left in a register, so no `test_loaded` can follow it without a new load.

**What a variant is for.** It does not replace `accessor`: discovery reads that recipe alone, and its counts stand as
they were. The finder (`eldennet-engine lodestar find`, and the in-game round) runs each variant beside it and
reports, under the variant's id: where its shape stands, how many places the finder accepts (each must load the same
path as any accessor, or it is refused), how many statics those places reach, how many of them the accessor's own
places reach too and with the same class object, and the statics that are new, each with its class object, its count
of places and its first places. The in-game round then names over the new statics as over the 217 (O92).

**How it is to be confirmed.** By its count on the owner's own executable first: a variant that stands nowhere is
withdrawn; one that reaches no new static is kept only as a cross-check. A new static enters a build's table only as
the 217 did: named by its instance's class in game, checked against the image, and a lead until S0-A's launches and
the self-tests confirm it (ARCH §6.3). `expect_matches` is set when a round has counted it, and `builds` stays empty
until then.

**Evidence.** The shape's forms are the processor's documented encodings; that such places exist is R0c's own count
of the references to the accessor's path (`docs/ops/probes/2026-11-16-r0c.md`, "What R0c leaves for R1": 26 places
with five other heads, 1,379 references after a 64-bit load of a static, 676 after a compare of a RIP-relative
quadword with zero). Nothing here comes from another project (O48), and no byte of the game is in it (O102).
