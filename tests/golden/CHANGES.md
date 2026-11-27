# Golden files: changes

Every golden file (`<route>.events.jsonl`) changes only with an entry here naming the route and the file's new hash,
which `eldennet-engine golden update` prints; `golden check` fails without it (ARCH §8.12, DESIGN C12). Newest first.

The three routes of v0.1 are synthetic, from `tests/scenarios/`: our own routes, names and numbers, no game data. A
recording of the owner's game becomes a route of its own the same way (`golden update <file>.ensnap.jsonl`), once a
redacted recording may be committed (S10.2.d).

- 2026-11-27 `camp-skirmish` fnv1a64:40876fd3a9aeeeee: the header's `rules_hash` alone, for v0.3's persona rules
  (content/eldenring/augur/persona.toml). No event changes: the route has no persona, so every character stays Mute,
  no persona rule fires and every audience stays empty.
- 2026-11-27 `boss-attempts` fnv1a64:e2063ecf209e36d4: the header's `rules_hash` alone, for the same rule file. No
  event changes: no persona, so FoughtBeside finds no one beside the Tarnished at the victory.
- 2026-11-27 `road-and-gate` fnv1a64:1fa73905c65c11fe: the header's `rules_hash` alone, for the same rule file. No
  event changes: no persona.
- 2026-11-20 `camp-skirmish` fnv1a64:78bc5af4234bb149: first golden (Augur v0.1, part 11).
- 2026-11-20 `boss-attempts` fnv1a64:6bc9449712fafd59: first golden (Augur v0.1, part 11).
- 2026-11-20 `road-and-gate` fnv1a64:f591301ff2185987: first golden (Augur v0.1, part 11).
