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
  (set alone, first), NORMAL sync, foreign keys, a 2 s busy timeout, temp store in memory, an 8 MB cache, the WAL's
  autocheckpoint at 1,000 pages and a 4 MB journal limit, FAST secure delete, an untrusted schema and no mmap. A new
  file gets the application id `0x454E5054`; a file with another, or none that reads as a database, is `NotADb`.
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
  after a TRUNCATE checkpoint; an archived file of this fingerprint, restored; else a new file. In the last two the
  slot's other live files are archived, never deleted. The store reads no name: the key carries its hash
  (`identity_of`, `soft_match`). File names' times are UTC, made without a clock or a locale (`utc_stamp`). Paths
  never leave en-store; `backups_of` answers the count and times alone, for the Memories page.
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
- the files (`test_files.cpp`): names and UTC stamps, the soft match's rules, a key's live file, a new file with the
  slot's other character archived and restored when that character comes back, another slot untouched, a renamed
  file with no WAL left, three rotating backups with the newest in `.1`, a migration's last two, and a corrupt file
  set aside for the newest backup (never taken for another character);
- the migrations (`test_migrate.cpp`): the shipped set numbered and clean, 0001 on an empty file with every table and
  both checks clean, a re-run that changes nothing, a later migration's backup, a failed one leaving the previous
  version and none of its tables, a newer file opened or refused by `schema_min_reader`, and the lint's every rule.
