# Proposal 0005: Lodestar, EldenNet's own address library

**Status: proposal, 2026-11-21 (roadmap S10.1.k; O100). Its three questions, O101 to O103, were decided by the owner
the same day, each as recommended (§14). Built so far, on `feat/lodestar`: step L1 (the catalogue and the table's
format) and the resolver's placement and witness checks, on invented images (§13).** Where this text and the designs differ, the designs hold. A bare §n
is a section of this proposal; DESIGN and ARCH are EldenNet's design documents. Evidence: the derivation rounds R0 to
R0c and R1's twelve runs on game 2.7.2.0 (WHY§171 to §209, §227 to §258, §289 to §303; `docs/ops/probes/`), the
game-access layer as designed and built (ARCH §6; WHY§121, §149, §159 to §170), and the first build of `feat/engine`
at b05fda5 in Lodestar's own container (GCC, Release: no warning, en-gal's 167 test cases pass).

## 1. What the owner asked

"Then we should delegate this job to a bigger model that will create our own game address library, such as what SKSE
and address library does for Skyrim. This will still be EldenNet's part, not separate." (O100, 2026-11-21.)

The question behind it: EldenNet's design runs its reading of the game in a task that the game calls every frame, and
twelve R1 runs did not get that task to run. The function R0c derived for adding the task removes one instead
(WHY§302), and the next step was stopped by a safety system (WHY§303). The owner's answer was not to pick a way round
it, but to build properly what Skyrim's mods are given and Elden Ring's are not.

## 2. What Skyrim's mods are given, and what EldenNet builds of its own

| Skyrim | What it gives a mod | EldenNet's own |
|---|---|---|
| The Address Library | A permanent id for each function and global of the game's executable, and a table per game version that turns an id into an address. A mod names ids, never addresses, so it survives a game update once the new table is out | Lodestar's ids (§4) and its table per build (§5) |
| CommonLib | The game's classes described for C++, read through those ids | Our own labels for classes and fields, and the layout per build (§4.3; `roles.hpp`, ARCH §6.3) |
| SKSE's loader | Loads the mod and checks the game's version | The launcher and the gate (built; ARCH §3.2, §4) |
| SKSE's task interface | A place to run the mod's code in step with the game | Lodestar's frame service (§9.4), and later its task service (§9.6) |
| SKSE's event sources | Typed events from the game | Augur, on what the reader reads (O49); sources of other kinds later, each after its own spike |

Three things differ for Elden Ring, and they shape everything below.

1. **Nobody hands us a table.** The Address Library's tables are made by its maintainers and published. The projects
   that describe Elden Ring's insides are closed to EldenNet for good (O47, O48), so every number comes from our own
   tools, run on the player's own copy, and is checked independently before EldenNet relies on it.
2. **No agent ever sees the game.** The tools run on the owner's PC. What comes back is numbers and our own words:
   offsets, sizes, counts, digests and verdicts. No file, report, message or chat carries anything of the game: no
   bytes, no text, no listing of its code (handover-2026-11-21 §3).
3. **Single-player and offline only.** Lodestar resolves nothing when anti-cheat is present or the session is not the
   player's own solo world (the gate, ARCH §3.2 and §5; WHY§167). Nothing in it serves online play.

## 3. What Lodestar is, and is not

**It is** a part of EldenNet, inside its game-access layer (`en-gal`), with its data in `gal/`:

- a **catalogue of ids**: every place in the game that EldenNet needs, under a number and a label of our own (§4);
- a **table per game build**: for each id, where it is in that build, as numbers, with how it was found and which
  checks it passed (§5);
- **recipes**: for each id, how our tools find it again in a build they have not seen (§6);
- **checks**: what must hold before EldenNet uses an id, at review and again at every start of the game (§7);
- **services** built on those: resolving an id, reading safely, and a place for the per-frame reading to run (§9);
- **devkit commands** that find, check and compare, for the owner now and for patch days later (§9.7).

**It is not:**

- a separate project, a second DLL or a download of its own;
- a copy, a port or a translation of anyone's offsets, patterns, headers or tables (O47, O48);
- a store of anything of the game's: it holds numbers and our own labels, and the game's class names only where they
  already are, as leads in `gal/leads.toml` (WHY§164);
- a hook library: it hooks nothing (R12, O65);
- a way to change the game: it writes nothing to the game's memory and patches nothing of its code or its protection;
- open to other mods in v1 (O103, §14).

## 4. Ids

### 4.1 The rule

An id is **a number and a label, both ours, both permanent**. `1005 task_hub` means the same thing in every build of
the game and every version of EldenNet. Code, tables and reports name ids; nothing else names a place in the game.

- Numbers are given once, in order, and never reused or renumbered. An id EldenNet stops using stays in the catalogue,
  marked retired.
- Labels are lower-case words of our own (`[a-z][a-z0-9_]*`), saying what the thing is for EldenNet, never the game's
  name for it.
- The catalogue is one file, `gal/ids.toml`, independent of any build. Each entry has the number, the label, the kind,
  one or two sentences in our own words saying what we mean by it, the read group or service that needs it, and whether
  EldenNet can run without it.

### 4.2 Kinds

| Kind | What it is | Its value in a build | Numbers |
|---|---|---|---|
| `static` | A slot in the game's data that holds a pointer or a value (a manager's instance, the module handle) | An offset from the start of the image | 1000 onwards |
| `function` | The start of one of the game's functions | An offset from the start of the image, and its length | 2000 onwards |
| `class` | A class of the game's objects, known by its run-time type information | The offsets of its tables of methods, found afresh at every start | 3000 onwards |
| `table` | A table in the game's data (the list of task groups) | An offset, the entries' size and count | 4000 onwards |

The first catalogue is what the built code already reads under labels: the 16 items of `roles.hpp` (5 statics and
functions found by pattern today, 11 managers), the classes, and the task groups' table. Two ids come straight from
R1: `task_remove`, the function R0c took for the registration and run 12 showed to remove (its verdict recorded, so
nobody calls it by mistake again), and `task_add`, its sibling, a lead with no verdict yet (WHY§302).

### 4.3 Members

What hangs off a class is named by the class's label and a label of its own, `character.handle`,
`near_entry.distance`: a field's offset and type, a size, a stride, a slot number, an enum's values, an index into a
table. These are the layout's entries as built (ARCH §6.3: 35 classes, 97 fields), unchanged. They have no numbers of
their own: a field means nothing without its class.

## 5. The data per build

Everything stays in `gal/`, read at start only, ASCII, small (a few kilobytes a build), and checked by
`eldennet-engine lint --gal` as today.

| File | Holds | Changes when |
|---|---|---|
| `gal/ids.toml` (new) | The catalogue (§4) | EldenNet needs a new place in the game |
| `gal/recipes.toml` (new; replaces `patterns.toml`) | How each id is found (§6) | A recipe stops working on a new build |
| `gal/leads.toml` | What to look for by name: the class and manager names the design already gives | A lead is added |
| `gal/builds.toml` | The builds EldenNet knows, by fingerprint (the gate) | A game patch |
| `gal/layouts/<build>.toml` (schema 2) | **The build's table**: every id's value, and the members | A game patch |
| `gal/derivations/<id>.md` | The note per recipe, in our own words (O27) | With its recipe |

An id's entry in a build's table:

```toml
[item.task_hub]
id        = 1005
kind      = "static"
rva       = 0x0                              # where it is: an offset from the start of the image
found_by  = ["accessor", "instance_class"]   # the recipes that gave this value, independently (§6)
witnesses = [0x0, 0x0, 0x0]                  # places in the code that use it, checked at every start (§7)
digest    = ""                               # for a function: a digest of its code as reviewed (§7)
verdict   = ""                               # for a function: what it does, as our tools judged it (§7.2)
checks    = ["placement", "agree", "class"]  # the checks it passed in the reviewed round
status    = "lead"                           # lead | confirmed; a release build refuses a lead (ARCH §6.3)
```

and the file's head carries the **provenance block**: the build's key, the tool's version and SHA-256, the round and
its date, the leads used, `method = "own-derivation"`, and the reviewed record in `docs/ops/probes/`.

**Numbers only.** A table holds offsets, sizes, counts, digests, our labels and fixed words. It holds no byte of the
game and no pattern cut from it. A digest is a fingerprint of code as our tool saw it (§7): it lets EldenNet say "this
is the function the review saw" without keeping any of it.

## 6. How an id is found

### 6.1 Recipes, not bytes

A **recipe** says how to find an id in a build nobody has seen: in our own words in its note, and as data our tools
run. The derivation kit already has every finder below but the last two; what changes is that none of them needs a
byte string kept in the repository (O102, §14).

| Recipe | Finds | How, in plain words | Runs |
|---|---|---|---|
| `accessor` | Statics that hold a manager's instance | The compiler writes the same short sequence wherever the game fetches a manager: load the slot, test it, skip ahead if it is set, otherwise report through a helper. The recipe describes that sequence as a **shape**, in a small grammar of our own (kinds of instruction and of operand, no bytes), and our decoder matches it | Offline and in game |
| `class` | Classes | The class's run-time type information, by the name a lead gives, and the tables of methods that refer to it (ARCH §6.2's RTTI walk) | Offline and in game |
| `instance_class` | Which static holds which manager | The slot's instance, its first word read once through the guarded copy, and that word's class (O92) | In game |
| `table` | The task groups' table and a group's place in it | A run of same-sized entries that each point at a name, cut where the code addresses it (WHY§184) | Offline and in game |
| `relation` | Functions | By what uses them: called at places that have just loaded a given static, with arguments of given kinds; or reached from an id already found (WHY§149) | Offline and in game |
| `invariant` | Statics and fields with no type information | By what the object holds: a counter that rises with play and stands still at times, a pair where one never exceeds the other, a list in ascending order (WHY§193; ARCH §6.10) | In game |
| `witness` (new) | Nothing new: it re-finds a known id | A few places in the code that use the id, kept as offsets; each must still decode to the same use of the same id (§7.1) | At every start |
| `effect` (new) | Nothing: it judges a function | What the function does with what it is handed, followed to a fixed depth: adds to a list, removes from one, only reads, or unknown (§7.2) | Offline and in game |

A shape is our description of a habit of the compiler, written from what the kit measured (R0c: one shape covers
almost every one of 7,151 places; WHY§197). It is not copied from the game, and it works on any build the same
compiler made. The byte string that `patterns.toml` holds today for the same idiom goes when its shape passes the same
tests.

### 6.2 Where and when

- **Offline, in the devkit, with the game closed.** `eldennet-engine lodestar find <the game's executable>` runs every
  recipe that needs no running game and writes a report of numbers and a draft table. It reads only the file (O73,
  O75). This is R0 as built, under a new name and a new report (§11).
- **In game, in a probe build, at the owner's choice.** The recipes that need live objects (`instance_class`,
  `invariant`) run from EldenNet's own thread through the guarded copy, read-only, and report numbers. This is R1
  without its call.
- **At every start, in every build.** No search. EldenNet passes the gate, loads the build's table, and re-finds each
  id through its witnesses and its checks (§7.1). An id that does not check is refused, alone.

Start-up gets simpler and faster than the present design: no scan of the game's code for patterns, only a few hundred
small reads at known places.

## 7. How an id is checked before EldenNet relies on it (O48)

### 7.1 The checks

| Check | What must hold | When |
|---|---|---|
| `placement` | The value lies where its kind must: a static in writable data and aligned, a function at a start the image's own function table lists, with a length inside its bounds | Review; every start |
| `agree` | Two independent recipes give the same value, or the offline round and the in-game round do. One recipe alone never confirms an id | Review |
| `witness` | The places recorded as using the id still decode to that use, in the image as loaded | Every start |
| `digest` | For a function: its code, with addresses masked, has the digest the review recorded. A function another mod has patched fails here, and EldenNet then knows not to touch it | Every start |
| `class` | A manager's instance, and each object read, has a table of methods of the expected class (R4, R5) | Every start and every reading |
| `self-test` | What is read makes sense: values in range, known ids found, counters rising (ARCH §6.7's phases A and B) | Every start; each playthrough |
| `second source` | Ids and counts of params and text agree with what DataPiler reads from the files through Smithbox, numbers only (O89 (4)) | Review |
| `verdict` | For a function EldenNet would ever call: §7.2 | Review; before a call |

An id is `confirmed` only when a reviewed round has passed every check its kind requires, and S0-A's launches agree
(DESIGN §16.1). A release build refuses a table with any lead in it, as now.

**At run time a failed check refuses that id and nothing else.** There is no nearest match, no retry with a looser
rule and no guess. The id's read group is off, the features that need it degrade as ARCH §6.8's ladder says, and one
notice tells the player what is reduced.

### 7.2 A function's verdict

R1's lesson is that a function can have the right shape, the right callers and the right arguments and still do the
opposite of what its lead said. So a function id carries a **verdict**, one word that our own tool gives: `adds`,
`removes`, `reads` or `unknown`. The `effect` recipe follows what the function does with its arguments, to a fixed
depth, and judges only what it can show; anything else is `unknown`.

- The verdict, the depth reached and a few counts are all that leave the tool. It writes out no instruction and no
  code, in any form.
- What a reviewer reads is **our** classifier and its tests on the fake game, which carries both an adding and a
  removing variant of every shape R1 met (`tests/fakegame`). The game's code is never the thing reviewed.
- `unknown` means no call, with no exception.

### 7.3 Calls into the game

Lodestar's services are read-only towards the game. A call into the game's code is never part of a service by
default. Where a later service needs one (§9.6), the function must be `confirmed` with the verdict the service
expects, its arguments taken from the game's own calls, the owner must have agreed to that call, and it is first made
once, at the owner's Mark, in a probe build, and reported (handover-2026-11-21 §3).

## 8. A game patch

1. **Patch day.** The new build's fingerprint is in no table. EldenNet shows its banner and reads nothing (L1). O12
   stands: an unknown build never runs on trial.
2. **Find again, offline.** The owner (later, any maintainer) runs `lodestar find` on the new executable, about five
   minutes with the game closed, then `lodestar diff` against the last build's table. For each id it says: found once
   by its recipes, found but ambiguous, or not found; and for a function, whether its digest is unchanged apart from
   its place (the review carries over) or its code changed (its verdict is judged again).
3. **In game, only for what needs it.** The ids found by `instance_class` and `invariant`, and any member whose
   self-test fails, take one read-only probe run.
4. **Review, then a data-only release.** The record is written, the new table enters `gal/` as leads, the self-tests
   and the smoke route run (ARCH §6.10 steps 4 and 5), and the table is promoted. If no recipe and no code changed,
   the release is the `gal/` folder alone.
5. **What no longer checks is refused, per id.** A build whose table lacks an optional id runs without that id's
   features; one that lacks a required id stays at L1 until it is found.

This is ARCH §6.10's runbook with two changes: the in-game round makes no call into the game, and a patch that only
moves things needs no in-game round for most ids.

## 9. The services

### 9.1 Resolve

The one way from an id to an address. The library is opened with the gate's `BuildProfile` (R7) and answers
`resolve(id)` with a typed handle (a static, a function, a class, a table) or a refusal that names the failed check.
Only a checked id has a handle, by type, and addresses never leave `en-gal` (ARCH §2.4).

### 9.2 Managers and classes

A static gives its manager's instance afresh at each reading, never kept (R5); a class gives the set of method tables
that an object of it may carry. Both are built, and move onto ids.

### 9.3 The reader

`GuardedReader` as built (ARCH §6.5): every read is a copy into our own memory after plausibility checks, a fault is
caught and counted, a thread's stack is never read, and nothing is ever written.

### 9.4 The frame service

The place where EldenNet's reading runs, behind one interface (`TaskBody`, as built), with two providers:

- **`own thread`**: a thread of EldenNet's own that reads at a fixed rate (§10). No call into the game.
- **`game task`**: the game calls our task each frame (§9.6). Not available until the task service is confirmed.

The rest of EldenNet (the snapshot, Augur, the core) does not know which provider runs.

### 9.5 Events

Augur makes EldenNet's events from what the reader reads (O49). Lodestar adds no event source in v1. A source that
needs a hook is a later question, each after its own spike and the owner's approval (R12).

### 9.6 The task service (later; the road R1 was on)

Reaching the game's own task system: the function that adds a task (`task_add`), the arguments it takes, and a task
object laid out as the game's own tasks are (WHY§289: never by guesswork). It is the hard part, and honestly
uncertain: it needs the `effect` recipe to answer `adds` for the sibling, the task's layout found from the game's own
tasks by `invariant` recipes, an explanation of run 4's crash on quit (WHY§209), and then one call at the owner's
Mark. I expect several owner runs. Nothing in v1 waits for it (§10).

### 9.7 The devkit's commands

Under `eldennet-engine`, in Developer mode (O78, O79), numbers only:

| Command | Does |
|---|---|
| `lodestar find <exe>` | Runs the offline recipes; writes the report and a draft table |
| `lodestar check <exe>` | Checks a known build's table against the executable: every id, every check that needs no running game |
| `lodestar diff <old table> <new table>` | What moved, what changed, what was lost |
| `lodestar explain <id>` | The catalogue's entry, its recipe's note, and its state in a build |

### 9.8 Plugins

Unchanged: a plugin gets projections through EldenNet's published interface, never an id, an address or a handle
(O65 to O69).

## 10. Where v1's reading runs

O100's three options, now that the library is the frame around them.

**(a) The game's task first.** Finish the task service, then build v0.1 on it. *For:* readings in step with the
frame, and the place later versions need for acting in the game. *Against:* it is the uncertain part (§9.6), every
step costs the owner a run, the same kind of work was stopped once (WHY§303), and our code would run inside the
game's own frame, where a mistake of ours is the game's crash.

**(b) EldenNet's own thread now; the task service follows when it is proven.** A reader thread takes a snapshot about
ten times a second through the guarded copy. *For:* it needs nothing that is not already found or findable
read-only; EldenNet v1 then makes **no call into the game's code at all** and writes nothing to its memory; the cost
to the game's frame is nil; and across twelve runs the probe's own thread read gigabytes this way with every fault
caught (WHY§235, §240). *Against:* a reading is not in step with the frame, so it can catch an object half-changed,
and events are timed to the reading, one or two tenths of a second late. For a dialogue-only v1 neither matters, and
the first is handled:

- **A pointer is never half-written** on this processor: an aligned 8-byte word is read whole.
- **Structure is read twice.** Pointers, counts and list ends are copied twice and must agree, or the reading is
  dropped and counted; values that change by nature (health, position) are taken from the second copy.
- **Every object is checked before it is trusted**: its class by its method table, its values by range (as built).
  An object freed between two reads either faults, which is caught, or fails its class check.
- **A change must hold for two readings** before Augur makes a life-cycle event of it (entering the world, a death).
- **Floating labels** follow a moving character at the reader's rate, not the frame's. The reader can run faster for
  that small set; S1 measures whether it is good enough, and the bottom feed is the fallback (ARCH §6.8).

**(c) Pause the in-game work.** Nothing is gained: (b) can start at once.

**Recommendation: (b).** It gives v1 everything it needs with the least risk, it makes EldenNet read-only towards the
game as GraceUI already is, and it loses nothing: the task service stays Lodestar's work, behind the same interface,
and the owner decides again when it is confirmed. It also removes the last other call: O92 names the managers by their
instances, so the getter fallback (ARCH §6.2 step 3) leaves release builds with the registration.

## 11. Reports and reviews

- **A new report, schema 2** (`kind = "lodestar"`): ids, offsets, sizes, counts, digests and fixed words, written
  through the closed-vocabulary writer and checked by `lint --report` before it is written, as today (WHY§149). It has
  no key that can carry bytes, a pattern, text, or an instruction in any form; the lists of instruction forms that
  R1's later reports carry (WHY§247) are not in it.
- **DataPiler's guard** holds its own copy of schema 1's vocabulary (WHY§188), so it needs schema 2's before it packs
  a Lodestar report. That is Engine's to do; until then the owner sends the file as R1's were sent.
- **Every returned file is reviewed as data**, and its record written in our own words in `docs/ops/probes/` before
  anything in it enters `gal/` (O89 (3), unchanged).
- **If a safety system stops any of this work**, that line of work stops. It is not rephrased, split or re-routed.
  The owner and Engine are told what stopped, with another way proposed (handover-2026-11-21 §3).

## 12. What stays out

- **Hooks.** Lodestar hooks nothing. A hook of EldenNet's own needs its spike and the owner's approval (R12, O65).
- **Changing the game.** No write to the game's memory, no patch of its code, and nothing that removes, disables or
  gets round its protection: EldenNet reads the image as the game runs it (S0-A, "untouched image").
- **Anything online.** No use with anti-cheat present, in a session with other players, or for cheating.
- **Other projects' work.** No offsets, patterns, headers, tables or code from any of them, at any version. Public
  research gives leads, which say what to look for and are never relied on unchecked (O4, O48).
- **The game's material.** No file, byte, text, name, param, key or code listing of the game in any file, report,
  message or chat.

## 13. The build, in steps the owner can follow

Each step lands on `feat/lodestar` with its tests on the fake game (GCC, Clang, MinGW under Wine, clang-cl), and is
reported to Engine for merging. Steps 1 to 4 need no game.

| Step | What is built | The owner |
|---|---|---|
| L1 | The catalogue, the table's schema 2, its loader and lint; `roles.hpp` on ids. *Done 2026-11-21 (engine dc18c95, merged as fb999ca); the fake game's own table comes with L3, which writes tables.* The resolver's placement and witness checks, first planned for L2 and L4, are built too (feat/lodestar 440233e) | Nothing |
| L2 | Shapes for the accessor; witnesses and digests; `patterns.toml` retired once the shape passes the same tests | Nothing |
| L3 | `lodestar find`, `check` and `diff`; report schema 2 | One offline run on 2.7.2.0, about 5 minutes, game closed |
| L4 | The checks at start; the frame service with the `own thread` provider; the fake game's harness under Wine | Nothing |
| L5 | A read-only spike package: the managers named, the witnesses checked, the reader run at the title and in the world, faults and dropped readings counted; no call | One run, about 15 minutes |
| L6 | The members: fields found by `invariant` recipes on the instances L5 gave, in read-only rounds (S1's precondition) | A few runs |
| L7 | The task service's research: the `effect` recipe and the fake game's variants, then a probe round (§9.6) | Later, by the owner's choice |

After the proposal is accepted, Lodestar writes its design document on `docs/lodestar`, with a code for `wp`, and
sends Engine the text for ARCH §3.2, §6.2 to §6.4 and §6.10, DESIGN §2.1, §4.2 and §16.1, and the roadmap.

## 14. Questions for the owner

### O101: Where does v1's per-frame reading run? (O100's question, §10)

(a) The game's task first: v0.1 waits for the task service. (b) **Recommended.** EldenNet's own thread now, reading
about ten times a second with the checks of §10; the task service stays Lodestar's work and the owner decides again
when it is confirmed. (b2) As (b), and the task service is shelved until a feature needs the game's thread. (c) Pause
the in-game work.

*What (b) gives up:* readings in step with the game's frame, for v1. *What it gains:* v1 makes no call into the game
and writes nothing to it, and the in-game work goes on from today.

*Decided 2026-11-21: (b), "Own thread now (Recommended)". EldenNet v1 reads from its own thread with §10's checks,
makes no call into the game and writes nothing to it, and the release build drops the getter fallback. The task
service stays Lodestar's later work (§9.6, step L7), and the owner decides again when it is confirmed.*

### O102: May `gal/` hold bytes cut from the game?

O89 let patterns of at most 64 bytes, their addresses wildcarded, leave the owner's PC, and `gal/patterns.toml` holds
one (24 bytes, 7 of them fixed). The handover of 2026-11-21 rules out byte patterns copied from the game in any file.

(a) **Recommended.** No bytes: recipes as shapes in our own grammar, tables of numbers and digests (§5, §6); the
existing pattern is retired when its shape passes the same tests; O89 is amended by a dated note. (b) Keep short
wildcarded patterns, as O89 allowed, and treat the handover's rule as covering longer listings only.

*What (a) gives up:* matching a shape is slower than matching bytes, which matters only offline (seconds), since a
start no longer searches (§6.2). *What it gains:* nothing of the game is in the repository at all, and a recipe
survives a patch that changes registers or distances.

*Decided 2026-11-21: (a), "No bytes at all (Recommended)". O89 is amended by a dated note: its patterns are
withdrawn, and `gal/patterns.toml`'s one pattern is retired when its shape passes the same tests (step L2).*

### O103: Who may use the library?

Skyrim's Address Library and SKSE serve every mod. (a) **Recommended.** In v1, EldenNet and its devkit only: the data
is in the open repository under EldenNet's licence, but there is no interface for other mods. Whether to publish one
is asked again after v1.0, with the plugin interface (O65 to O69). (b) Design a public interface now, for other
native mods, bound by the same gate (offline, solo, no anti-cheat). (c) Never public.

*What (a) gives up:* other mods cannot build on it yet. *What it gains:* the library stays small and ours to change
while it is young, and nothing we publish can be turned to uses we would not stand behind.

*Decided 2026-11-21: (a), "EldenNet only in v1 (Recommended)". Asked again after v1.0, with the plugin interface.*

## 15. What changes if this is accepted

- **O89** gains a dated note: R0 becomes `lodestar find`, R1's call leaves the round under O101 (b), and patterns
  leave `gal/` under O102 (a). **O12** is unchanged. **O92** becomes the `instance_class` recipe.
- **ARCH §6.2 and §6.3**: discovery at start becomes "load the table and check each id"; `patterns.toml` becomes
  `recipes.toml`; the layout gains its `[item.*]` entries. **ARCH §6.4 and §3.2**: under O101 (b), `register_task` and
  `call_getter` leave release builds, and Register and FirstTick become the reader's start and first reading.
  **DESIGN §2.1**: the thread that reads the game is the reader, not the game's.
- **DESIGN §16.1**: S0-A counts ids, not five patterns; S0-B measures the reader for two hours, not a game task.
- **Roadmap**: S10.1.f's remaining parts fold into S10.1.k's steps L1 to L7.
