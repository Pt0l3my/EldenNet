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

The pilot's fixtures (S11.3.c, d) name the shipped persona and a fixture flag state (`"pack"`, prompt_fixture.hpp) in
place of a hand-written record, so their goldens follow `content/eldenring/personas/kale.toml` and what he knows at
that state (`known(P)`, block 5b). Their scenes and lines are invented, in our own words.

- `pilot_start`: `fresh`, a first talk: block 5b holds his road alone.
- `pilot_middle`: `after_radahn`, an earlier talk's lines: block 5b as NPC §3.6 renders it, the star as known and the
  castle's and the general's falls as heard.
- `pilot_late`: `erdtree_burned` with his talk about the howl set, so his `howl_told` stage is active (its mood and gist
  in block 7), four lines in block 5b with the queen's fall left for block 7's topic offers (v0.3), and his "you, now"
  line.
