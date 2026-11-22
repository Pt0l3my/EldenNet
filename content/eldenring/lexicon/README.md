# The lexicon: the lint's lists

The reply lint (NPC §8.3, §8.4; `src/en-core/include/en/core/speech_lint.hpp`) reads two files from here. The content
loader reads them and hands their texts to en-core's `parse_lexicon`; en-core does no I/O. `eldennet-engine lexicon
check` reads them as the lint does and prints how many entries each list holds.

## banned.toml

EldenNet's own lists, in our own words, format 1:

- `[hard]`: lists of words and phrases that block a sentence (game mechanics, meta and the interface, modern idiom,
  modern objects and the real world, assistant-speak). Any key under `[hard]` is a list of the banned kind.
- `[promise]`: `hard`, the high-confidence promise patterns, which block a sentence; `wider`, the wider promise and
  claim list, logged and counted by the eval.
- `[soft]`: `interjections`, the global opener set; `archaic`, the archaic forms logged in a register that has none.

An entry matches whole words in order, in any case, with accents ignored and the text cut at anything but letters,
digits and an apostrophe inside a word; an entry's last word ending in `*` matches any word it begins. Words with a
sense of their own in the land (save, load, button, game, player, loot, cool) are left out, as the file's comments say.

## safety.toml

The safety list (O107): slurs and explicit sexual terms, refused at both safety levels (`always`), and graphic gore,
refused at `gentle` only (`gentle`). The repository holds **fingerprints only, never the words**, and so do its tests
and reports.

- A fingerprint is SHA-256, in 64 lower-case hex digits, of `eldennet.safety.v1:` followed by the normalised word or
  phrase of at most three words, joined by one space.
- The normalisation, fixed in the lint, is the same for the list's words and for a reply's: the text is cut into
  chunks at white space and at anything that is not a letter, a digit, a look-alike (`@ $ ! |`) or a separator
  (`. , ; : - – — … _ * ~` and the apostrophes); a chunk's separators and `!` at either end are dropped; letters lose
  their accents and their case; in a chunk with a letter, the separators inside are dropped and the look-alikes become
  letters (0 o, 1 i, 3 e, 4 a, 5 s, 7 t, 8 b, @ a, $ s, ! i, | i), while a chunk with no letter keeps its digits alone;
  three or more one-letter words in a row become one word; and a letter repeated becomes one ("fooo" is "fo").
- The lint looks up each of a sentence's words and each run of two and three of them. It is a little narrower than a
  plain list (O107's trade): a word split across a phrase's spaces, or misspelt, is not caught.

**This file ships with no entries.** Filling it is a separate step for the owner: on the owner's own PC, write the
words one a line into `eldennet-engine lexicon fingerprint`, which prints a fingerprint for each, sorted and without
repeats, and writes the words nowhere (not to a file, a log or a report); paste the fingerprints into `always` or
`gentle`. A word whose normalised form is also an ordinary word (the repeats squeezed) catches that word too, so check
the list against ordinary speech before shipping it. `eldennet-engine lexicon check` refuses a fingerprint that is not
64 lower-case hex digits and one listed twice.
