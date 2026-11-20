# Golden files: changes

Every golden file (`<route>.events.jsonl`) changes only with an entry here naming the route and the file's new hash,
which `eldennet-engine golden update` prints; `golden check` fails without it (ARCH §8.12, DESIGN C12). Newest first.

The three routes of v0.1 are synthetic, from `tests/scenarios/`: our own routes, names and numbers, no game data. A
recording of the owner's game becomes a route of its own the same way (`golden update <file>.ensnap.jsonl`), once a
redacted recording may be committed (S10.2.d).

- 2026-11-20 `camp-skirmish` fnv1a64:78bc5af4234bb149: first golden (Augur v0.1, part 11).
- 2026-11-20 `boss-attempts` fnv1a64:6bc9449712fafd59: first golden (Augur v0.1, part 11).
- 2026-11-20 `road-and-gate` fnv1a64:f591301ff2185987: first golden (Augur v0.1, part 11).
