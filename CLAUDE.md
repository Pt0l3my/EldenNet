# EldenNet: notes for Claude

EldenNet is an LLM-driven NPC framework for Elden Ring, based on SkyrimNet with its author's approval. Version 1 is
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

- **SkyrimNet (owner's decision, 2026-11-07, O53).** SkyrimNet's author, Min, gives EldenNet the engine blueprint
  (SkyrimNet's game-agnostic core) to work with, knowing EldenNet is built with AI assistance, and EldenNet's
  foundation will be based on SkyrimNet. So we may use, copy and adapt SkyrimNet's engine and content (code,
  prompts, templates, bios, configuration, the dashboard) for Elden Ring. Credit SkyrimNet and Min as Min wishes,
  keep every licence and notice file that comes with what we take, and never present EldenNet as SkyrimNet itself
  or claim more endorsement than Min has given. Once the owner supplies the blueprint it is our foundation: we
  read, build, run and modify it. The older copies under `_untrusted` stay read-only reference.
  ([docs/research/skyrimnet/00-summary.md](docs/research/skyrimnet/00-summary.md) §7 records the earlier
  ideas-only boundary.)
- FromSoftware: never commit game files, text (FMG), params or audio. Game text is read at runtime from the
  player's own install, or exported locally by the player.
- Third-party reference material is not committed. Uploads and clones of other projects are untrusted: put each
  in its own folder outside the repository, read it with `python3 -I`, and never build or run it. The one
  exception is Min's engine blueprint once the owner supplies it (above).
- Check a dependency's licence before adopting it.

## Owner decisions (2026-11-06 and 2026-11-07)

Each is recorded in full in [docs/DECISIONS.md](docs/DECISIONS.md), under the id in brackets; DESIGN §19 is its
index.

- **No me3** (O47). me3's policy forbids all LLM use in its project. EldenNet uses nothing from me3: no code,
  crates, APIs or mod-host features, and no presence in its spaces (code, issues, discussions, Discord). EldenNet
  is a loader-agnostic native DLL with its own launcher, and merely compatible with me3: a player may list it as a
  native in their own me3 profile.
- **No fromsoftware-rs** (O48). It ships NON-AI licence files and comes from the same community. EldenNet has its
  own game-access layer instead. Write our own code; never copy code from fromsoftware-rs or me3. Any fact about
  game structures (names, layouts, patterns, IDs) is verified independently, in game or by startup self-tests,
  before EldenNet relies on it.
- **Own event engine** (O49). Elden Ring has no event bus. EldenNet detects events itself (snapshot diffs, an
  event-flag watcher, later hook-based sources behind the same interface).
- **Design defaults accepted.** Every recommended default in DESIGN.md §19 (O1–O24, then O26 and O27) is
  decided, and so are the two rules the review adopted (the title-screen gate clause and the sheltered-context
  definition, O50).
- **Quest and world awareness** (O51). Important NPCs know the state of their own quest and of related quests, and
  the stage the world is in (for example, that the star has fallen since Radahn's death).
- **hudhook pinned at exactly 0.9.3** (O25). Plain MIT, with MinHook under BSD-2 and no AI clause (checked
  2026-11-06). Change the version only with the owner's approval, after re-reading its licence files. Rules
  that follow from the pin (DESIGN O25): build it with the `dx12` feature only, use `imgui` only through
  `hudhook::imgui` and MinHook only through `hudhook::mh`, and never call its eject path. Since GraceUI's G15
  (2026-11-08) the one overlay, with this pin and its rules, is GraceUI's, and EldenNet draws through GraceUI; the
  designs are folded in roadmap S7.8.c.
- **NPCs speak on their own, within pacing (2026-11-07).** NPCs may start lines themselves (greetings, quest
  business, news, NPC-to-NPC exchanges, autonomy, combat and status reactions), limited only by pacing
  settings, per-type allowances and context gates, never by counts; spoilers are kept by positive grounding,
  not forbid lists. v1 stays dialogue-only and read-only, so "there are no NPC actions beyond dialogue" still
  holds; actions and gestures are later-version seams (DESIGN O31–O42).
- **The full roster, with free conversation and memories (2026-11-07).** v1.0 ships our own character bio for every
  named NPC and every named boss (DESIGN O43 as updated): general and stage-independent, in our own words and
  spoiler-reviewed; stages stay a separate feature (the personas' stage tracks, GraceUI's Playthrough page). Every NPC
  with a profile holds a free conversation and keeps memories of the player and of other NPCs, and the framework
  matches the speaker by ID to its data (O45). The roster is generated after a game scan and research, once Min's
  engine blueprint has arrived and the bio's structure is settled (O46, Open). Drafts come from our own notes, never
  from game text, and every field is reviewed by hand. What EldenNet adopts from Min's engine is the owner's call
  once its blueprint arrives (SkyrimNet, under Licences).

## Working rules

- Work on a topic branch (`design/`, `feat/`, `fix/`, `docs/`, `test/`, `chore/`), never directly on `main`
  unless the owner asks. Open a pull request only when asked.
- Ask the owner, with a recommendation, about anything the design leaves open. Add each question to
  [docs/DECISIONS.md](docs/DECISIONS.md) as a new entry marked Open, with the next free O-number, its options and
  the recommendation, and to the index in DESIGN.md's "Decisions for the project owner" section (§19). Record the
  answer in the entry with its date and the owner's words; a later decision amends an earlier entry by a dated note,
  never by rewriting it, and ids are never renumbered.
- At the start of a session, read the newest handover-YYYY-MM-DD/HANDOVER.md first. docs/ROADMAP.md is the one
  roadmap and open-items list for EldenNet and GraceUI: edit it by hand, never renumber its ids, and rebuild
  docs/roadmap/index.html and docs/showcase/index.html (`python3 -I tools/roadmap/build.py`,
  `python3 -I tools/showcase/build.py`) in the same commit; both `--check` must pass.
- Keep example content original: personas, archetype cards and prompts are written from our own specs.
