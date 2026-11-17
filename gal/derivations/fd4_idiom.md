# fd4_idiom: the FD4 singleton accessors' shared head

*A lead on 2.7.2.0 WW, derived 2026-11-16 from the owner's own executable file; confirmed in no build yet.*

**What was searched for.** ARCH §6.2's FD4 finder: the game reaches each of its FD4 singletons through a small
accessor, inlined at every use, that loads the singleton's static, and when it is still empty makes it through the
class's runtime class object and logs the source file's path, which ends with `FD4Singleton.h`. Our own kit
(`eldennet-engine probe-exe --derive`, run by DataPiler 0.2.41's job with `gal/leads.toml`, O89 step 1) looked for
that path's text in the image, then for every instruction that loads its address, and decoded backwards from each one
to the accessor's head: the static's load, its test, the conditional jump over the block, the class object's address
and the getter's call. The sites were grouped by the head's shape, and the idiom was cut from the largest group: the
bytes every site of it shares, with the registers' bytes and every displacement made wildcards, so that it matches the
shape and no one site.

**In which build.** 2.7.2.0 WW (`gal/builds.toml`), the run called R0c (the second offline run, after R0's review
fixed the finder; WHY§180 to §189).

**What R0c found.** 3 path strings ending with the suffix, 9,837 instructions loading them, 7,095 of them behind an
accessor's head, in 6 head shapes. The largest shape holds 7,069 sites, and the idiom drawn from it (24 bytes, 7 of
them fixed) matches all 7,069 of them among 7,151 hits in the first `.text`. Each hit was decoded: 7,081 were accepted
(the static a RIP-relative load into `.data`, aligned to 8; the class object in `.data`; the getter a call to the
start of a function of the exception directory; a path loaded in the block), and 70 refused as no accessor's
(operands that are no RIP-relative loads). Part 2's scan, given this pattern, resolves it. The accepted hits reach 217
class objects, 219 statics and 1 getter. None of the class objects holds its name in the file (213 lie past the raw
data the file holds of `.data`): the game fills them when it starts, so the singletons are named in game (R1).

**How it is to be confirmed.** In game, R1 derives the idiom again over the image as loaded (probe build of
eldennet.dll, `ImageLayout::Memory`) and calls into the game only if it is this one, byte for byte (its `fd4_idiom`
check; the live count of hits is reported beside the file's). S0-A's 20 launches per mode then confirm it with the
self-tests, and only then does this pattern list the build in `builds` (ARCH §6.3).

**Evidence.** R0c's report, `derive.json` (numbers and our labels only), reviewed and recorded in
`docs/ops/probes/2026-11-16-r0c.md`. Nothing here comes from another project (O48): the shape is ARCH §6.2's
description, and the bytes are our own kit's cut.
