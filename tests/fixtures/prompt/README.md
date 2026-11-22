# Prompt fixtures

Inputs of the dialogue prompt (the first conversation's part A, WHY§364): each `<name>.prompt.json` is an invented
persona's record in NPC §3.1's form with its stage resolved, and a scene, a recap and a history of invented lines
(src/en-tools/lib/prompt_fixture.hpp has the format). `eldennet-engine render-prompt --fixture` prints the prompt one
gives; `eldennet-engine golden check` renders it twice (T5) and compares it with `golden/<name>.prompt.txt`, whose
changes `golden/CHANGES.md` records. Nothing here is a game's: the persona, its places and its words are ours.

- `first_meeting`: a first talk, a quiet scene, one line said.
- `long_talk`: the previous conversation's lines and ten exchanges, so block 6 is trimmed to its 900 tokens (the
  earlier lines first, then the oldest exchanges) and a tic's note reaches block 11.
- `crowded_regen`: four long nearby others, so block 9 is trimmed to its 210; the "you, now" line, the just-now
  lines, block 5b's lines in both parts, the promise reminder and two spent tics, of which block 11 notes the newest
  alone (WHY§388).

Every fixture's block 2 is within its 120 tokens: `golden check` refuses one past it, as the content's load refuses
shipped and test records past it (WHY§388).
- `over_cap`: two exchanges far past the hard cap, which the trim may not touch: the render is refused.
