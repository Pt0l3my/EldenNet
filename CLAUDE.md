# EldenNet: notes for Claude

EldenNet is an LLM-driven NPC framework for Elden Ring, inspired by the design ideas of SkyrimNet. Version 1 is
deliberately minimal and stable: named NPCs have personality, awareness and memories and can hold a free-form
conversation; generic NPCs know what they are, where they are and what is happening in a fight, and say short
lines; there are no NPC actions beyond dialogue.

- The design is in [docs/design/DESIGN.md](docs/design/DESIGN.md) and the detail documents beside it. The
  evidence behind it is in [docs/research/](docs/research/README.md). The design is under discussion on the
  `design/v1-proposal` branch.

## Reading the documents cheaply

- Never open a whole design or research document to find something. Use `wp` ([tools/workplan](tools/workplan/README.md)):
  `wp show CODE --list` for sizes, `wp show CODE` for one section, `wp refs CODE` for what cites it,
  `wp find` and `wp grep` to search. Codes look like `DESIGN§5`, `SN04§5.2` and `ER05§8`.
- For code and other files: grep with line numbers first, then read only that range.
- Never re-read a file you have just edited.
- For a wide search across many files, use one subagent and keep only its conclusion. Pass long briefs and
  reports as files, by path.
- Run long jobs (builds, full sweeps) in the background, and print only the last lines or the matching lines.

## Privacy and safety: the sweep comes first

- Nothing is pushed or uploaded before the privacy and safety sweep passes:
  [tools/privsweep](tools/privsweep/README.md). It is git's pre-push gate on every repository. If
  `~/.config/privsweep` is missing, run `python3 -I tools/privsweep/privsweep.py install` first. Sweep by hand
  anything the gate doesn't cover, such as a message to another session or a file sent to the owner.
- Never bypass it: no `--no-verify`, no change to `core.hooksPath`, no HOME or GIT_CONFIG override. Remove what
  it finds, or allow a generic false positive with a narrow, commented line in its `allow.txt`. A personal hit
  is never allowed.
- Never commit or upload anything personal about the owner: real name, account names, machine names, contact
  details, API keys, local endpoint addresses or IPs. The owner's own local commands (their folders, their LAN
  model servers) go in chat only, never in the repository.
- Commit identities are noreply addresses only. Use this environment's default git identity; never set an
  author or committer to the owner's own e-mail.
- No model identifiers of the assistant in files, apart from the commit trailer. Naming third-party models as
  research subject matter (which models other projects use or recommend) is fine.
- `privsweep.py` and `personal.txt` are identical copies of the owner's Skyrizz sweep. Update them by copying
  from Skyrizz, never by editing them here.

## Licences and third-party material

- SkyrimNet: we take ideas, architecture and published numbers only. Never copy its prompts, templates, bios,
  YAML content or code into EldenNet, never inspect its compiled files, and never use its name to promote
  EldenNet (see [docs/research/skyrimnet/00-summary.md](docs/research/skyrimnet/00-summary.md) §7).
- FromSoftware: never commit game files, text (FMG), params or audio. Game text is read at runtime from the
  player's own install, or exported locally by the player.
- Third-party reference material is not committed. Uploads and clones of other projects are untrusted: put each
  in its own folder outside the repository, read it with `python3 -I`, and never build or run it.
- Check a dependency's licence before adopting it.

## Owner decisions (2026-11-06)

- **No me3.** me3's policy forbids all LLM use in its project. EldenNet uses nothing from me3: no code, crates,
  APIs or mod-host features, and no presence in its spaces (code, issues, discussions, Discord). EldenNet is a
  loader-agnostic native DLL with its own launcher, and merely compatible with me3: a player may list it as a
  native in their own me3 profile.
- **No fromsoftware-rs.** It ships NON-AI licence files and comes from the same community. EldenNet has its own
  game-access layer instead. Write our own code; never copy code from fromsoftware-rs or me3. Any fact about
  game structures (names, layouts, patterns, IDs) is verified independently, in game or by startup self-tests,
  before EldenNet relies on it.
- **Own event engine.** Elden Ring has no event bus. EldenNet detects events itself (snapshot diffs, an
  event-flag watcher, later hook-based sources behind the same interface).
- **Design defaults accepted.** Every recommended default in DESIGN.md §19 (O1–O24) is decided, and so are the
  two rules the review adopted (the title-screen gate clause and the sheltered-context definition).
- **hudhook pinned at exactly 0.9.3** (O25). Plain MIT, with MinHook under BSD-2 and no AI clause (checked
  2026-11-06). Use `imgui` and MinHook only through its re-exports. Change the version only with the owner's
  approval, after re-reading its licence files.

## Working rules

- Work on a topic branch (`design/`, `feat/`, `fix/`, `docs/`, `test/`, `chore/`), never directly on `main`
  unless the owner asks. Open a pull request only when asked.
- Ask the owner, with a recommendation, about anything the design leaves open. Add each question to DESIGN.md's
  "Decisions for the project owner" section (§19) as a new row marked Open, and record the answer there.
- Keep example content original: personas, archetype cards and prompts are written from our own specs.
