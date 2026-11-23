# en-core's speech: the sanitizer, the lint and the pager (part A)

The first conversation's speech (NPC §8.2 to §8.6; WHY§364), behind part 0's interfaces in `speech.hpp`. Each part is
a const factory the conversation holds by reference and opens once per reply; the conversation (part B) runs them in
NPC §8.2's order: deltas into the sanitizer, each sentence through the lint, the lint's passes into the pager, and a
page sealed when the conversation needs one. Pure, as all of en-core (ARCH §2.3): no I/O, no clock, no static object
with a dynamic initialiser (ARCH §3.1).

```cpp
#include "en/core/speech_lint.hpp"
#include "en/core/speech_pager.hpp"
#include "en/core/speech_sanitizer.hpp"

auto lexicon = en::core::parse_lexicon(banned_toml_text, safety_toml_text);  // the content loader reads the files
const en::core::SpeechSanitizer sanitizer;
const en::core::SpeechLint lint(std::move(*lexicon));
const en::core::SpeechPager pager;
```

| Part | Header | What it does |
|---|---|---|
| `SpeechSanitizer` | `speech_sanitizer.hpp` | Steps 0 to 7 on a stream split anywhere; sentences given out once final; removals by reason and raw bytes, never their text; the trailer's intent; `withdraw()` for en-llm's Reset |
| `SpeechLint` | `speech_lint.hpp` | The hard lists (safety by fingerprint, promise, banned) and §8.4's action; the soft list; the tally between replies; `parse_lexicon`; `safety_fingerprint`; `DeflectionRing` |
| `SpeechPager` | `speech_pager.hpp` | §8.5's caps (`Take`), lazy sealing with page 1's 250 ms wait, the over-long split, a short sentence joining the next; `page_hold_ms`, `hold_ends` (Tab), `page_fits` |

## What the conversation must know

- **A sentence is given out once it is final**: once the next sentence has begun, or the stream has ended. So the
  last sentence of a reply comes at `finish()` (or at a cut: a trailer, a run-on, the 4 KB cap), and a sentence
  mid-reply one delta after its boundary. Removals between two sentences go with the earlier; those after the last of a
  reply go with it; those of a reply with no sentence go with none.
- **Flow**: `IntentOnly` after a trailer (keep reading for `intent()`, no sentence follows), `Stop` after a run-on or
  the cap (cancel). Feeds after `Stop` are ignored.
- **The lint's verdicts**: `Regenerate` and `Deflect` come only while no sentence of the reply has passed (a sentence
  the opener rule emptied is dropped, so the next is still the first); after a verdict that ends the reply, every later
  sentence answers `Stop` with the same list. `check()` may change the sentence's text (a stripped opener).
- **`Take::Over`** means the sentence was left out **and** the reply is full: cancel the stream on `Full` or `Over`,
  whichever comes first (a crossing sentence past the cap + 10 words, or past 600 bytes, answers `Over` without a
  `Full` before it).
- **`Sealing::Wait` without `until`**: ask again when a sentence arrives or the reply ends (a sentence under 12
  characters waits for the one it joins). Once the reply is full or has ended, nothing waits.
- **`Page::last`** is set on a page sealed once the reply is full or has ended with nothing left. A page sealed while
  the stream was still running cannot know it is the final one: the next `next()` answers `Done`.
- `LintContext` gained `self_names` (third-person self-reference) and `tic_max` (each tic's `max_per_conversation`;
  missing is 1). `PageContext{0}` takes the schema's default of 40 words.
- `DeflectionRing` holds one persona's ring for a journey; its `order()` is what the store will keep (v0.3).

## Tests

`tests/speech/` (doctest, every build): each sanitizer step alone, the splitter, withdraw, and seeded property tests on
random streams fed whole, one byte at a time and at random cuts; SHA-256 on NIST's examples, the safety normalisation
and its matching on invented words only, each hard list and action, each soft item, the tally, the ring and the
lexicon's errors; the hold formula, Tab's floor, the fit, the caps, the first sentence's cut, page 1's wait, the short
sentence, the split, and a seeded property on random sentences. `fuzz/sanitize` and `fuzz/pager` are their libFuzzer
targets. en-tools' `test_dialogue_prompt.cpp` runs the shipped lexicon and a recorded stream through all three.
