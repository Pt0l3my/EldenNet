# en-store: the store

The SQLite files of EldenNet's memory, their migrations and backups, and the store's request loop on its own thread,
T9 (ARCH §1.2, §2.2; MEM §8 to §10; v0.3, roadmap S12.1.a). C++20, MIT. It depends on en-proto and SQLite alone
(ARCH §2.3, checked at configure time): never on en-core, en-gal, en-overlay or en-llm. SQLite is 3.54.1's amalgamation,
pinned by its hashes and built as the build's one C target (cmake/sqlite.cmake, THIRD_PARTY.md); en-store reads it
through its C API only. Its static library `en_store` is a module the DLL links (`en_dll_module`).

v0.3 has one kind of file, the playthrough file (one per character, every journey inside); `pools.sqlite` comes with
barks (v0.4), and `diag.sqlite` when a page needs history beyond a session (WHY§689).

## What it holds

- **SQLite's layer** (`db.hpp`). `Db::open(path, kind, create)` opens a playthrough file with MEM §8.5's pragmas: WAL
  (set alone, first), NORMAL sync, foreign keys, a 2 s busy timeout (a test's own, shorter, when given), temp store in
  memory, an 8 MB cache, the WAL's autocheckpoint at 1,000 pages and a 4 MB journal limit, FAST secure delete, an
  untrusted schema and no mmap. A new file gets the application id `0x454E5054`; a file with another, or none that
  reads as a database, is `NotADb`.
  `Stmt` binds and reads every id as a 64-bit integer, text as UTF-8 and a number as itself, never formatted into
  SQL. `Transaction` is `BEGIN IMMEDIATE` (every request) or `EXCLUSIVE` (a migration), rolled back unless committed.
  `execute_batch` runs a whole text of statements split by SQLite's own parser (`sqlite3_prepare_v2` from the tail the
  last one left), never by searching for ';', so a comment with an apostrophe, a string with a semicolon or a trigger's
  body stays whole (MEM §10, WHY§98). `backup_into` is `VACUUM INTO` a path that must not exist; `checkpoint` is
  `wal_checkpoint` (TRUNCATE or PASSIVE); `healthy` is `integrity_check` and `foreign_key_check`; `meta_get` and
  `meta_set` read and write `meta`. Faults are SQLite's classes (`fault_of`: Busy, Full, IoErr, Corrupt, NotADb,
  Constraint, Other); SQLite's message, which can quote SQL or a path, is never kept or logged.
- **Numbers in text** (`number_text`, `number_of`; MEM §10): `std::to_chars` and `std::from_chars`, the shortest exact
  form with no grouping, whatever the process's locale.
- **The migrations** (`migrate.hpp`; MEM §10). `src/migrations/pt/NNNN_name.sql`, built in (`playthrough_sql()`),
  numbered from 1 with no gap and applied forward only by `migrate(db, set, backup)`: a newer file opens only when its
  `schema_min_reader` is at most this build's top (else `TooNew`, memory off); before the first migration that changes
  a file, `backup` receives a copy; each migration runs in one exclusive transaction with `user_version`, so a failure
  leaves the file at its previous version; migration 1 writes `meta.app` and `meta.schema_min_reader`
  (`kSchemaMinReader`, 1: every v1.x migration is additive). `migration_lint` refuses transaction control (BEGIN,
  COMMIT, END, ROLLBACK, SAVEPOINT, RELEASE starting a statement; a trigger's own BEGIN and END are allowed) and
  `journal_mode` anywhere, skipping comments and quoted strings.
- **The files** (`files.hpp`; MEM §8.1 to §8.3). Under `<saves>\<SteamID64>\`: the live `pt-s<slot>-<fp8>.sqlite`;
  `backups\` with three rotating copies (`.1` the newest, taken at each playthrough start and before a forget) and a
  migration's last two (`.pre-<from>-<to>`); `archive\` for an older character in a reused slot
  (`-<utc>.sqlite`, kept for good); a corrupt file set aside in place (`.corrupt-<utc>`) for the newest backup to
  take its place. `locate` finds a key's file in MEM §8.3's order: the live file; a live file in the slot whose meta
  matches the name's hash and the archetype with gifts equal or unread (0xFF) on either side, renamed while closed
  after a TRUNCATE checkpoint; this file set aside as corrupt in an earlier session, with its newest backup restored
  (MEM §8.6); an archived file of this fingerprint, restored; else a new file. In the last three the slot's other live
  files are archived, never deleted. The store reads no name: the key carries its hash
  (`identity_of`, `soft_match`). File names' times are UTC, made without a clock or a locale (`utc_stamp`). Paths
  never leave en-store; `backups_of` answers the count and times alone, for the Memories page.
- **The requests on one file** (`playthrough.hpp`; MEM §8.6, §8.7). A `Playthrough` is an open file with meta's
  counters (`next_event_id`, `next_conv_id`: assigned by the core, persisted with each insert, never going back, even
  after a trim) and its current journey and epoch. Each request runs inside the caller's transaction:
  - `append` stores a P event with its frozen audience rows and the facts its fold changed; a JourneyChanged into a
    later journey first starts that journey's epoch (MEM §9.4), and its event belongs to it. An NpcSaid's first page
    goes into its payload's `pages` (`end`, `pt`), and `append_page` adds each later one, the text joined by a space;
  - `conv_open` and `conv_close` (ids the core's); `upsert_job` (ids the store's); `commit_journal` writes a version,
    its watermark (`memory_progress`, which moves only so), the job done and its `verified:*` facts; `commit_lived`;
    `put_facts` replaces the journey's facts with the core's refold and clears `facts_stale` (WHY§689);
  - the queries: `bundle` (the newest journal and lived versions, the facts, the last six direct lines of the most
    recent ended conversation, block 6a, and how many events above the watermark the persona perceived);
    `job_input` (the persona's own evidence in (watermark, upto], with the conversations its lines belong to, for their
    `shown_context`); `lived_input`; `fold_events` (every event of the journey with every audience row, for a refold);
    `open_jobs`.
  - The event log's SQL is `events.cpp`'s alone, the one file whose SQL names `event_audience` (MEM §1.3; CTest's
    `en_augur_audience_grep`). A persona's query joins on its own rows, so it never reads who else perceived an event.
    The store's one event of its own, a crashed conversation's ConversationEnded, gets its persona-only audience there
    too (`insert_own`).
- **The store's core** (`store.hpp`; MEM §8.3, §9, §3.8; O144). `StoreCore::handle(DbRequest)` answers one
  `DbResult` of the same sequence number, synchronously: the tests and the eval call it so, and T9 runs it behind C8.
  Each write is one `BEGIN IMMEDIATE` transaction; a query reads the persona's own rows. **Open**:
  1. finds or makes the key's file (`locate`); a corrupt one is set aside and the newest backup takes its place;
  2. refuses a newer file before touching it;
  3. takes the start backup, migrates (with the pre-migration copy) and fills a new file's meta and first epoch;
  4. starts a journey's epoch the file has not seen (NG+ while EldenNet was not running);
  5. checks the timeline. H is the newest play time of the current epoch: an event, a reply's last page or a
     conversation's start. Δ = H − the save's play time. Δ ≤ 0 is Clean, and 0 < Δ ≤ 5 min a silent trim.
     Beyond 5 min, or a save from an earlier journey (cut at the later journey's start at the latest), the player
     is asked only if, after the cut, the file holds something that matters (O144): a conversation's turn, a line
     addressed to the Tarnished, a P event whose frozen audience holds anyone, a journal or lived version, a
     StageChanged, or a later journey's epoch. Otherwise the trim is silent too;
  6. closes open conversations as crashed and queues running jobs again (MEM §3.8 steps 1, 2 and 4), once the
     timeline is resolved.
  The choice's summary (`RollbackSummary`) holds ids and numbers only: the save's play time and journey against the
  file's, a count per kind (claims among the journal versions), the newest eight items with who, where and when, and
  the stages that would be undone. While the choice waits, nothing but Resolve and Close is answered. **Resolve**
  Forget trims; Keep starts a `keep` epoch at the save's play time (MEM §9.3); a new character archives the file and
  makes a new one. **Forget** (MEM §9.5) takes a backup first (the undo), removes one NPC's rows in the journey
  (another's overheard rows stay) or the whole journey's (its epochs stay), records the act without content, and
  empties the WAL (a TRUNCATE checkpoint) so the forgotten rows do not linger in it.
- **The breaker** (`Breaker`, MEM §8.6): three failed transactions within 60 s of monotonic time, or one Corrupt,
  NotADb or IoErr, trip the store for that file for the session: the request answers `Tripped` with the cause, the file
  is closed (set aside if corrupt, for the next session to restore its newest backup), and its Open answers `Tripped`
  until the session ends. Busy is never counted, nor a refusal before SQL (Invalid, NotOpen) or a newer file (TooNew).
  A failed transaction takes the open file's counters in RAM back with it, so a retried append still persists them.
- **T9** (`loop.hpp`; ARCH §1.2, §1.3; MEM §8.6). `Store::spawn(cfg, notify)` starts the thread and answers a
  `StoreHandle`. C8 is two bounded queues of our own, 1,024 each way. `send` never blocks: a full queue answers `Full`,
  for the shell's spill ring. The loop waits on C8 with a 250 ms timeout, drains all that is there, and runs each
  request as its own transaction, in order. A run of more than 16 consecutive appends shares one transaction; if it
  fails, they run one by one, so the bad one fails alone. Each result goes back in order, then `notify` wakes the core.
  The timeout paces housekeeping: on the first quiet 250 ms after writes, at most once per 10 s, a passive checkpoint,
  whose result comes back (sequence number 0) only when it trips the breaker. `before_request` is the tests' stall.
- **The trim** (`rollback.cpp`, MEM §9.2), in one transaction:
  - every epoch started after the cut goes whole, and the first at or before it is cut into;
  - a reply shown across the cut keeps its pages shown at or before it;
  - conversations started after the cut go, and one across it ends `trimmed` at its last kept line, its counts redone;
  - journal and lived versions, decisions, recall memories and intents after the cut go;
  - a journal job's range comes down to its last kept event, or the job goes; a lived job of a removed version goes,
    and a running job is queued again;
  - the watermark is the current version's;
  - `facts_stale` is set for the core's refold, and the event counter stays, so no id is reused.
- **`0001_init.sql`**: MEM §8.7's schema as written, with the reserved recall tables (`memories`, `memory_actors`,
  `memories_fts` on FTS5, `embeddings`) and the seams (`npc_intents`, `decisions`). Ours on top of it: `meta` also
  holds `next_conv_id` (conversations' ids, the core's, never reused) and `facts_stale` (set by a trim until the core's
  refold is written, WHY§689); every table whose rows a reader may hold an id of is AUTOINCREMENT, so a trim never
  hands a removed id to a new row (ARCH §2.2's store rules); `events.payload` and `conversations.shown_context` must be
  valid JSON.

## Tests

`en_store_tests` (doctest; GCC and Clang, and the MinGW build under Wine), with invented rows only:

- the pin (`test_sqlite.cpp`): the version, each compile option as SQLite reports it, FTS5, the JSON functions, no
  double-quoted literal, a 64-bit id past 2^53;
- the layer (`test_db.cpp`): the pragmas and the application id, a file not ours or no database at all, a missing
  file never created, ids past 2^32, 2^53 and 2^62 in columns and inside JSON, numbers under a locale with a decimal
  comma (the C++ global locale always; a C locale when the system has one), a batch with an apostrophe in a comment, a
  semicolon in a string and a trigger, a transaction dropped or committed, a constraint, the fault classes, and a
  backup that never overwrites;
- the store's core (`test_store.cpp`): a new file and its start backup; refusal with nothing open; a silent trim
  within 5 min with the counter kept; O144's two sides (an hour holding only unwitnessed events trimmed silently, one
  witnessed event asking, with its summary); the summary's every kind and the stages undone; Keep, and a later crash
  trim touching only its epoch; a new character archived; crash recovery; a reply's pages and a conversation across a
  cut; journal, lived, jobs and the watermark after a trim; forgetting an NPC and a journey; NG+ and a save from the
  earlier journey; a newer file refused before its backups are touched; a corrupt file restored from its backup; and 12
  seeded random cuts (no row past the cut, no orphaned audience, the counter as it was, both checks clean);
- the requests (`test_playthrough.cpp`): an event with its audience, each persona reading its own row alone and
  nothing of another journey, a lower id that never takes the counter back; an NpcSaid's pages with their ends and pts;
  the schema's caps (text, payload, JSON, a kind); a JourneyChanged's epoch; conversations with ids past 2^53; a job's
  life and a journal version's commit with its watermark, job and facts, a duplicate version refused; the bundle's
  six-line tail and pending count, direct lines only; a job's evidence; lived versions, a refold replacing every fact,
  and the fold's every row;
- the files (`test_files.cpp`): names and UTC stamps, the soft match's rules, a key's live file, a new file with the
  slot's other character archived and restored when that character comes back, another slot untouched, a renamed
  file with no WAL left, three rotating backups with the newest in `.1`, a migration's last two, and a corrupt file
  set aside for the newest backup (never taken for another character);
- the faults (`test_faults.cpp`, through `fault_vfs.hpp`, a shim over SQLite's default VFS registered by the tests
  alone): the breaker's window and what it never counts; a full disk failing each write alone and the third tripping
  the file, kept as it was; a failed write, and a failed sync at a checkpoint, as IOERR tripping at once; a file found
  corrupt in session (garbled reads), closed and set aside, its Open refused for the session and its newest backup
  restored the next; a lock held by a second connection answering Busy, mid-session and at open, never counted;
- the loop (`test_loop.cpp`): a stalled store (`before_request`) filling C8 to 1,024 with the next send answering
  Full at once and every request landing once it drains, in order; a burst with one bad append failing alone, a query
  among them in its place and the counter still persisted; the passive checkpoint's pacing;
- the hard kill (`test_crash.cpp`): the test program runs itself as a child (`--crash-child`) that appends through T9
  and writes down each acknowledged id, and is killed without warning (`posix_spawn` and SIGKILL on Linux,
  `TerminateProcess` on Windows and under Wine): every acknowledged id is in the file, which is whole and opens again
  with its counter past them;
- the migrations (`test_migrate.cpp`): the shipped set numbered and clean, 0001 on an empty file with every table and
  both checks clean, a re-run that changes nothing, a later migration's backup, a failed one leaving the previous
  version and none of its tables, a newer file opened or refused by `schema_min_reader`, and the lint's every rule.
