# en-core's dialogue prompt (part A)

`DialoguePrompt` (`include/en/core/dialogue_prompt.hpp`) renders NPC §7.3's dialogue prompt from a `DialogueContext`
on en-prompt, over the content pack's `prompts/` held in memory. The content loader (part B) reads the files and hands
them over by name from the pack's root:

```cpp
std::map<std::string, std::string> templates = /* "prompts/dialogue/01_world_frame.md" -> its text, ... */;
auto prompt = en::core::DialoguePrompt::load(templates);  // every block's part, the five registers, a clean lint
auto rendered = (*prompt)->render(context);                // or a PromptError: then nothing is sent (NPC §7.1)
```

## The record

`DialogueContext::record` is the persona's record as JSON, in NPC §3.1's field names, with its active stage resolved
(inherited fields filled in) as one object under `"stage"`; `"stages"`, if present, is not read. Absent fields are
unset. The fields read, each type-checked (a wrong type refuses the render, naming the field, never its value):

| Field | Into |
|---|---|
| `register` (default `formal`), `pronouns` (default `they`), `reply_words_max` (default 40, clamped to 10–60) | `register`, `pronoun`, `possessive`, `words_max` |
| `public.role`, `public.summary`, `public.appearance` | `persona.role`, `.summary`, `.appearance` |
| `private.personality`, `.honesty`, `.claims`, `.seeks` (list), `.ties` (list) | `persona.…` |
| `voice.cadence`, `.diction`, `.deflection`, `.openers` (list), `.tics` (`[{text, max_per_conversation}]`, or texts) | `persona.…`; `persona.tics` holds the texts |
| `instructions`, `address_forms` (list: the first, then the others) | `persona.instructions`, `persona.address`, `persona.address_also` |
| `stage.identity`, `stage.unlock.text` | `persona.identity`, `persona.unlock` |
| `stage.road`, `stage.where`, `stage.mood`, `stage.gist` | `stage.road`, `.where`, `.mood`, `.gist` |

Phrases (`public.role`, `private.honesty`, `stage.where`, `stage.mood`) lose a final full stop, since the templates
end their clauses.

## The template data

Every name is always present (null, `""` or `[]` when unset), so the templates' names are fixed and the load's lint
checks them: `name` (as the game shows it), `pronoun`, `possessive`, `register`, `words_max`, `persona.*` and
`stage.*` (above); `standing.tied` and `standing.land`, each `{known, heard, believed}` (block 5b's lines); `earlier`
and `history`, each `[{tarnished, text}]`; `scene` (`place`, `time_word`, `weather_word`, `nearby` (at most four),
`tarnished`, `just_now` (at most three), `you_now`: the words after "You, now:"); `recap` (the newest three);
`spent_tics` (each tic whose count in the context's tally has reached its `max_per_conversation`); `reminder` (`""`,
`"banned"`, `"promise"` or `"safety"`); and the empty seams `actions_block` and `actions_tail` (DESIGN §13).

No name carries the player's name: it appears in no prompt until told (DESIGN §8.8), and the told-name line of block 8
comes with memory (v0.3). The Tarnished's words go in through `quote()`.

## Blocks, caps and the trim

The templates are `prompts/dialogue/NN_*.j2|md`, one per block: `01` to `05`, `05b`, `06` to `11`; any other numbered
part refuses the load. Each block is rendered alone (a block that renders nothing is left out), and the blocks'
messages are merged by role, keeping their breakpoints: cache A after block 1, one after block 5, cache B after 5b
(left out with 5b when it is empty). `report()` gives each block's estimate against its cap and what the trim took;
the trim and the budgets are dialogue_prompt.hpp's. Blocks 2 and 11 can pass their caps with fields within the schema's
own caps (block 2: identity, role, summary and appearance at most come to about 300 tokens against 120; block 11: two
tic notes and a reminder come to about 95 against 40): they are reported, never refused.

## Tests

`tests/prompt/test_renderer.cpp` (every build) on invented templates: the order, the merge and the marks, history,
block 11's notes, the data and its refusals, the trim order and OverCap, the load's checks, a render's failure and
byte stability. en-tools' `test_dialogue_prompt.cpp` and the prompt goldens (`tests/fixtures/prompt/`) on the shipped
templates.
