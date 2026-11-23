# Prompt goldens: changes

Every prompt golden (`<name>.prompt.txt`, the dialogue prompt a fixture of `tests/fixtures/prompt/` gives, with its
budget, each block against its cap and the trim) changes only with an entry here naming the fixture and the file's new
hash, which `eldennet-engine golden update <fixture>.prompt.json` prints; `golden check` fails without it, as a route's
golden does (ARCH §8.12). Newest first. The fixtures' persona, names and lines are invented.

- 2026-11-23 `pilot_start` fnv1a64:95e89ce7671406db: first golden (S11.3.c, d): the pilot, Kalé, from the shipped
  persona at `fresh`: block 5b his road alone.
- 2026-11-23 `pilot_middle` fnv1a64:f26343acd29aaa7a: first golden: the pilot at `after_radahn`, block 5b as NPC §3.6
  renders it (152 tokens), an earlier talk's lines in block 6.
- 2026-11-23 `pilot_late` fnv1a64:6416733abf2af777: first golden: the pilot at `erdtree_burned` with his talk about the
  howl set: his `howl_told` stage in block 7, four lines in block 5b (164 tokens), and block 9's "you, now" line.
- 2026-11-22 `crowded_regen` fnv1a64:e3c324ab19b937f7: block 11's cap rises from 40 to 70 tokens and carries one tic
  note at most, the newest (WHY§388): of the two spent tics only the last is noted, and the note loses "in this talk"
  so the ask, a note and the promise reminder fit (67/70); 1,517 tokens in all.
- 2026-11-22 `long_talk` fnv1a64:e940e657a3239436: block 11's cap rises to 70 (WHY§388), so its one note is within it
  (46/70), and the note loses "in this talk".
- 2026-11-22 `first_meeting` fnv1a64:e61106e8951bf2c2: block 11's cap rises from 40 to 70 (WHY§388); its text is
  unchanged.
- 2026-11-22 `first_meeting` fnv1a64:58ce3b5f3d01b03d: first golden (the first conversation's part A).
- 2026-11-22 `long_talk` fnv1a64:a5bb610a80f26900: first golden (the first conversation's part A).
- 2026-11-22 `crowded_regen` fnv1a64:f3a2c0c4f2f64ee5: first golden (the first conversation's part A).
- 2026-11-22 `over_cap` fnv1a64:b4c49dc0da0b9ed3: first golden (the first conversation's part A).
