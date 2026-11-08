# Handover, 7 November 2026

For the session that picks up EldenNet and GraceUI. The work so far ran in one long session whose working files lived
only in its own container; this folder holds those the next steps need. The design of both is done and decided, apart
from one open question (O46; O54 was decided on 2026-11-08); there is no code yet, and the build waits on the owner for Min's engine
blueprint.

**Read first, in this order:**

1. This file.
2. [docs/DECISIONS.md](../docs/DECISIONS.md): the owner's decisions O1–O57, one entry each (O46 and O56 open). GraceUI's
   own are in GraceUI's `docs/DECISIONS.md` (G1–G13, none open; G13 decided on 2026-11-08).
3. DESIGN §0, the design at a glance: `python3 -I tools/workplan/wp.py show DESIGN§0`.
4. [docs/ROADMAP.md](../docs/ROADMAP.md), the one roadmap and open-items list for both repositories (or its page,
   `docs/roadmap/index.html`).
5. GraceUI's `docs/DESIGN.md` §0, "At a glance" (grep its headings, then read that range).

Then both CLAUDE.md files, which bind this work (§5).

## 1. Where things stand

**Branches.**

| Repository | Branch | State |
|---|---|---|
| EldenNet | `design/v1-proposal` | The design branch. `fdea52a` before this folder's commit, which adds the work listed below |
| EldenNet | `main` | The owner merged pull requests #2 and #3 on 7 November; it holds everything up to `fdea52a` |
| GraceUI | `design/v1` | The design branch. `ebe85cd` before this round's commit (its DECISIONS.md and the §12 index) |
| GraceUI | `main` | The owner merged pull requests #1 and #2 on 7 November; it holds everything up to `ebe85cd` |

**Pull requests.** EldenNet's #1 (6 November), #2 ("EldenNet v1 design stage") and #3, and GraceUI's #1 ("GraceUI
design stage") and #2 are all merged; none is open. Work committed since waits on the design branches; a new pull
request is opened only when the owner asks. Both repositories are public.

**The history rewrite (7 November, the owner's option (a)).** EldenNet's `design/v1-proposal` and `main` were rewritten
so that no commit carries the FromSoftware text (developer labels and quoted game text) later removed from the research
notes. GraceUI's history was not rewritten. Two things are left, both the owner's:

- **Delete the temporary branches** on EldenNet's remote: `chore/clean-main` and `design/v1-proposal-clean`. Both point
  into the clean history and nothing needs them.
- **Ask GitHub Support to purge `refs/pull/1/head`**, the first pull request's ref (`a79ebfb`), and the cached old
  commits. That ref still points at the pre-rewrite history with the removed text; only GitHub Support can remove a pull
  request's ref (the alternative is deleting and re-creating the repository).

**What this round added** (7 November). The owner asked for a roadmap and documents "for better memory and easier to
follow milestone and design stages", after consulting the Skyrizz agent, "and also a showcase of the UI and the
features we have so far, which will get gradually updated".

- **docs/DECISIONS.md** in both repositories. EldenNet's carries O1–O46 from DESIGN §19 and O47–O53, the owner
  decisions first recorded in CLAUDE.md (no me3, no fromsoftware-rs, our own event engine, the review's two rules, quest
  and world awareness, the SkyrimNet dashboard study and the SkyrimNet permission, which superseded it). GraceUI's
  carries G1–G10, G11 (the owner's request that started GraceUI) and G12 (O53 as GraceUI applies it). DESIGN §19 and
  GraceUI DESIGN §12 are now their indexes; where a decision was re-worded in place, the entry restores the text as
  decided from git history and adds dated notes.
- **docs/ROADMAP.md** (18 stages S0–S17, 57 steps, 168 parts) and its page `docs/roadmap/index.html`, built by
  `tools/roadmap/build.py` (adapted from Skyrizz's; 48 tests).
- **docs/showcase/**: what EldenNet is, the features designed so far and GraceUI's mock (8 screenshots at 1280×720,
  lossless WebP, 689 KB in all), built from `docs/showcase/showcase.md` by `tools/showcase/build.py` (37 tests). It
  lists the roadmap's stages, so it is rebuilt whenever a status changes there.
- `wp` no longer reads `GraceUI:DESIGN§n` as EldenNet's own DESIGN§n.
- This folder.

**Published pages.** The roadmap and the showcase are published as private claude.ai pages, each at the same address
every time: the roadmap at <https://claude.ai/artifact/1HQVhkTsKhgR1EdkqUijxk> and the showcase at
<https://claude.ai/artifact/Hq7hkTZyyvzwyvCQzZgvYa>, both first published on 2026-11-07. Each address and its slug are
recorded in the source's header comment, and the roadmap's also in the showcase's `roadmap-published` field, which
turns on the showcase's link to it. Republish to the same slug with the live version as the base (`get_artifact` gives
it), and only the owner changes who may see a page.

**Decided but not yet folded.**

- **GraceUI's decisions change EldenNet's design** (G1, G2, G4–G6, G8–G10): the main menu on F5 through GraceUI and
  the bridge (a `UiPort` in the core), the local server always on with pairing for the home network, the overlay drawing
  `graceui.dll`'s vertex lists, criteria and the compatibility matrix's rows, GraceUI as its own mod in every release,
  and small fixes. GraceUI's `docs/eldennet-changes.md` §1–§9 lists them; none is in EldenNet's documents yet apart from
  the roster (its §9 item 1). Roadmap S7.1.
- **O43 needs GraceUI's profile editor.** GraceUI's `docs/profile-editor-needs.md` names the GraceUI rules to revisit
  (D2's no content editors, SP1, SP4, PV4 and others) and the editor's pages, fields, validation, commands and its
  "draft with AI" flow. Roadmap S7.2.
- **O53, building on Min's engine.** Memory, prompt and context design that the engine may supply is marked as waiting
  for the blueprint rather than designed twice (DESIGN §7.8; roadmap S8.1). The research notes keep the earlier
  ideas-only boundary as dated evidence (SN00 §7).
- **Put to the owner as O54** (roadmap S6.3.a): whether the numbers-only census may start before the
  prerequisites, as DESIGN §7.8 allowed "if the owner agrees". Decided on 2026-11-08: no build before the blueprint;
  the game's data is researched and documented now (S6.3.b; the research note becomes ER08, then O55 asks which
  source to use).

## 2. Next steps, in order

1. **Min's engine blueprint** (roadmap S6.1, waiting on the owner). When the owner supplies it, it becomes EldenNet's
   foundation (O53): read it, keep every licence and notice file that comes with it, and recommend what EldenNet adopts
   and how SkyrimNet and Min are credited, as Min wishes. The owner decides what is adopted, recorded as a new entry in
   docs/DECISIONS.md. Then adopt it (S8.1): build and run its core outside the game; design what was deferred to it
   (memories of other NPCs and their recall, the memory job for NPC-to-NPC exchanges, keepsakes from another NPC's line,
   context assembly); take its prompt templates and their engine, the LLM client and its job slots, the TTS and STT
   seams and the bio's file format; fold the changes into DESIGN, ARCH, NPC and MEM and into GraceUI's pages that show
   them, with a review pass. The event engine, the game-access layer, identity by verified keys, the spoiler layers, the
   gates, pacing and read-only play stay as designed.
2. **O46, the bio's structure and the v1.0 roster** (S6.2, waiting). The recommendation is (a): one bio for every
   named NPC and named boss, the base game's roster in v1.0 and the expansion's in v1.1. The owner wants it settled
   before the profiles are generated. Check the recommendation against the blueprint's own bio format when it arrives,
   before asking again.
3. **The roster pipeline** (S9), once O46 is decided (its leads sooner from the game-data research of O54; roadmap
   S6.3): the roster's scan, draft and verify tools and the lints PL10 to PL12 (NPC §4.8); then batch by batch, by
   region or hub: the census on the author's machine (names stay local; the index gets ids and numbers only), a research
   note per NPC in our own words, a draft from each note with every field reviewed by hand, and a second reader's
   spoiler review at first-meeting level. About 140 base-game profiles and about 170 hours, on v1.0's critical path
   (DESIGN §7.8, risk K29). Drafts never come from game text.
4. **GraceUI's changes to EldenNet's design** (S7.1). These can go ahead while step 1 waits: fold
   `docs/eldennet-changes.md` §1–§9 into EldenNet's DESIGN, ARCH and NPC, and record G1, G2, G4–G6 and G8–G10 in
   EldenNet's docs/DECISIONS.md as its item 7.4 says, in the owner's words as GraceUI's DECISIONS.md gives them.
5. **The profile editor pass** (S7.2), in GraceUI: revisit the rules `docs/profile-editor-needs.md` §1 names, with O43
   as the reason, then design the editor in GraceUI's DESIGN (§4.6).
6. **The follow-ups from the character-profile design** (S7.3; commit `588f083`'s message): a check-without-saving
   message to match GraceUI's `profile.check`; whether merchants count as the player's sightings; whether PL3's
   unknown-alias error reveals that an alias exists; hash widths; `bark_words_max` against a character's `words_max`;
   character keys in `pools.sqlite`.
7. **The LLM Jobrunner** (S7.4). The owner's local job runner for prompt sets, built by the owner's own tools session
   from EldenNet's request (`jobrunner/request-jobrunner.md` here). Version 1.2 was reviewed read-only, with nothing in
   it run (`jobrunner/review-jobrunner-1.2.md`, 23 items): no path from a jobpack, a server reply or a website to code
   on the PC, but one privacy defect (item 1: the server's address and port reach result errors, and so the shared zip),
   so until it is fixed the owner checks a result zip's failed items before sharing it. The owner passes the review on;
   its next version is reviewed the same way before EldenNet's prompt sets run on it. EldenNet's own client for model
   servers the session can reach is `tools/llmworker`.
8. **Housekeeping**: the owner's GitHub clean-up (§1; S7.5); the public name, any time before the v0.6 public test
   (O1; S7.6).
9. **Then the build** (S10–S17): EldenNet's increments v0.1 Eyes, v0.2 First words, v0.3 Remembrance, v0.4 War cries,
   v0.5 Veil, v0.6 Threshold (the public test) and v1.0, then v1.1 (DESIGN §16.2) and v1.5 (speech), the spikes
   in game coming first where §16.1 places them, with GraceUI 0.1 Foundations to 1.0 beside them (GraceUI DESIGN
   §10). When code starts, consider Skyrizz's task list, bundles, cards, reports and evaluations; the Skyrizz agent's
   advice was to leave them until then.

## 3. This folder

| Path | What it is |
|---|---|
| `HANDOVER.md` | This file |
| `event-list.md` | The events EldenNet notices, written for the owner on 7 November: those that existed, what quest and world awareness added, what speaking up added, every reaction the player can set with its default, and what to add later. Input for the event catalogue when the engine is adopted, and for GraceUI's Reactions page |
| `jobrunner/request-jobrunner.md` | EldenNet's request for the LLM Jobrunner (jobpack/1, the page, the outputs, robustness): the spec its reviews check against |
| `jobrunner/sample.jobpack.json` | A two-item test jobpack in that format (invented content); put your own server aliases in place of `<server>` and `<judge-server>` before running it |
| `jobrunner/review-jobrunner-1.2.md` | The read-only review of version 1.2: verdict, then 23 items by severity, each with where, problem, evidence and fix |

These are copies of working files that lived only in the old session's scratch space; each was swept before it was
copied here. In the Jobrunner documents, the name of the owner's local model-server tool, a server port and the owner's
server aliases were replaced with generic words or placeholders, since the owner's local set-up stays out of the
repository.

**Not here, by design**: the third-party copies read during the design (SkyrimNet's files, the Jobrunner 1.2 bundle,
Magelight UI and others) and the owner's reference screenshots; the owner can supply them again, and they go in their
own folders outside the repositories (§5). GraceUI's mock screenshots are not kept either: its `mock/build/shoot.js`
takes them again.

## 4. How the work is done

- **Decisions** are the owner's. Ask only what the design leaves open, always with a recommendation. A new question is
  an entry marked Open in that repository's docs/DECISIONS.md, with the next free id (O58 in EldenNet; in GraceUI the
  framework proposal's questions take G15 onwards), its options and the recommendation, and a row in its index (DESIGN
  §19, GraceUI DESIGN §12). The answer goes in the entry with its date and the owner's words. A later decision amends an
  earlier one by a dated note under **Later changes**, never by rewriting it; the question stays as it was put; ids are
  never renumbered or reused. What the designers settle themselves goes in DESIGN §18 and GraceUI DESIGN §13 (the
  D-entries). A GraceUI decision that changes EldenNet is added to GraceUI's `docs/eldennet-changes.md`.
- **The roadmap.** Edit docs/ROADMAP.md by hand (its header comment gives the format); never renumber an id; a dropped
  part keeps its id and says so. Statuses: done, active, next, planned, waiting (on the owner) and idea; with no task
  list yet, a part's status is written on the part. Rebuild the page in the same commit as any edit:
  `python3 -I tools/roadmap/build.py`, and `--check` must pass. It checks GraceUI references against GraceUI's files
  when its checkout sits beside EldenNet's (as `graceui`), else accepts them. The page names none of the game's
  characters, places or terms (only the game itself), so cite a section rather than a decision whose title has one
  (the roadmap cites neither O19 nor O39 for that reason).
- **The showcase** is updated as features land: edit `docs/showcase/showcase.md`, change its `updated` date, rebuild,
  and commit the source, the page and the screenshots together. Rebuild it too whenever a stage's or a part's status
  changes in the roadmap (its `--check` says when).
- **Publishing**: build with `--fragment FILE`, then sweep FILE by hand (`python3 -I tools/privsweep/privsweep.py files
  FILE`): the upload hook covers a tool named Artifact, not the claude-artifacts publishing tools. The copy is a whole
  document without the content-security line, as `publish_files` takes it. Publish it privately to the same address as
  before, and record a new address in the source's header. A published page carries no paths,
  addresses, personal names, or names of the game's characters or places.
- **Handovers** at every break: a dated folder like this one, with HANDOVER.md and copies of the working files the next
  steps need, each swept first, since containers are thrown away.
- **Reading** is cheap: `wp` for EldenNet's documents, grep with line numbers and then a range for anything else, one
  subagent for a wide search, long jobs in the background.
- **Commits** go to topic branches (`design/`, `feat/`, `fix/`, `docs/`, `test/`, `chore/`), never to `main` unless the
  owner asks, with the environment's default noreply identity, after the sweep. A pull request is opened only when the
  owner asks.
- **Skyrizz** is the same owner's project. The owner asked for its agent to be consulted on these records, and its
  tools may be copied; `privsweep.py` and `personal.txt` are kept identical to Skyrizz's, updated by copying.

### The tools

| Tool | Commands (from the repository root) |
|---|---|
| privsweep | `python3 -I tools/privsweep/privsweep.py install` once per container if `~/.config/privsweep` is missing (the session-start hook runs it); `... files PATH...` to sweep files by hand, `... text WHERE` for a message on stdin, `... repo . --head`, `... selftest`. It is git's pre-push gate and the upload hook for the tools its matcher names (not the claude-artifacts tools: sweep a fragment by hand); never bypass it |
| wp | `python3 -I tools/workplan/wp.py show CODE --list` for sizes, `show CODE` for one section, `refs CODE`, `find in:docs/design text:'...'`, `grep '...'`, `stat`. Codes: `DESIGN§n`, `ARCH§n`, `NPC§n`, `MEM§n`, `SNnn`, `ERnn`, and `DOC:DECISIONS§o46` for a decision |
| llmworker | `python3 -I tools/llmworker/llmworker.py list`, `ask ENDPOINT "..."`, `batch ENDPOINT --in FILE --out FILE`. Endpoints live in a file outside the repository (its README); never commit an address, a host name or a key |
| roadmap | `python3 -I tools/roadmap/build.py` (build), `--check`, `--fragment FILE`; tests: `python3 -I -m unittest discover -s tools/roadmap` |
| showcase | `python3 -I tools/showcase/build.py` (build), `--check`, `--fragment FILE`, `--images-from DIR` for new screenshots (needs Pillow); tests: `python3 -I -m unittest discover -s tools/showcase` |
| GraceUI's mock | In GraceUI: `python3 -I mock/build/build.py mock` writes the pages; `mock/build/shoot.js` (Playwright's Chromium) takes the 1280×720 and 2560×1440 screenshots into a folder outside the repository |

## 5. Standing rules and the owner's preferences (binding)

Both CLAUDE.md files bind, and where they overlap the stricter one holds. In short, with what the owner said in this
session:

- **Privacy first.** Nothing is pushed, published or sent before the sweep passes; never bypass it. Nothing personal
  about the owner in any file, page, commit or message: no real name, account or machine names, contact details, keys,
  local addresses or paths. The owner's own local commands and model servers go in chat only. No model identifiers of
  the assistant in files, apart from the commit trailer.
- **Licences.** No me3 and no fromsoftware-rs in any form (O47, O48). SkyrimNet may be used, copied and adapted (O53),
  credited as Min wishes, with every licence and notice file kept, never presented as SkyrimNet itself and never with
  more endorsement than Min has given. SeverActions has no licence: describe its look in our own words, copy nothing.
  Nothing of Magelight UI in v1. OFL fonts only, from their publishers. hudhook stays at exactly 0.9.3 with its rules
  (O25). Check a dependency's licence before adopting it.
- **FromSoftware.** No game files, text, params, audio, fonts, icons or art; game text and names come at run time from
  the player's own install. Fixtures, examples and screenshots use invented names; published pages name nothing of the
  game.
- **Untrusted copies** go in their own folder outside the repositories, are read with grep or `python3 -I`, and are
  never built or run. Min's blueprint, once the owner supplies it, is the one exception.
- **Version 1** is dialogue-only and read-only towards the game; actions and gestures are later seams, and later
  features show in GraceUI as dimmed entries tagged "WIP" (the owner's word, G7).
- **Pacing, never counts.** In the owner's words: "I would not restrict how many times an NPC can speak up or react."
  NPCs' own lines are limited only by pacing (time between lines, a time window, a cooldown per event type), per-type
  allowances and context gates such as range, and being alive and conscious.
- **Positive grounding.** Spoilers are kept by telling the model plainly what stage the NPC, its related quests and the
  world are at, not by forbid lists; the owner asked to "minimize or avoid negative prompting".
- **Combat dialogue** is on by default, for any NPC in a fight, with the player or with each other, and only when the
  NPC isn't speaking scripted dialogue.
- **The roster**: our own bio for every named NPC and named boss, general and stage-independent; stages are a separate
  feature (the Playthrough page). Drafts from our own notes, never from game text, every field reviewed by hand.
- **Test early** (O29): every check runs from the first increment in which its subject exists.
- **Questions** always come with a recommendation, and every answer is recorded (§4).
- **The showcase** is kept current as features land.
- **Writing**: British spelling; they/them for anyone whose pronouns are not stated, the owner included; GFM tables
  escape a pipe inside a code span as `\|`.
- **Branches**: topic branches only; `main` only when the owner asks; a pull request only when asked.
