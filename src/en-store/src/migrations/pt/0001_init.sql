-- The playthrough file's first schema (MEM §8.7, original; v0.3). One file per character, every journey of it inside.
-- Applied by en-store's runner in one transaction (MEM §10): no transaction or journal-mode statement belongs here.
-- Row ids that a reader may hold are never reused: the event and conversation ids are the core's, from meta's
-- counters, and every other table that a later row could reuse an id of is AUTOINCREMENT.

CREATE TABLE meta (k TEXT PRIMARY KEY, v TEXT NOT NULL) WITHOUT ROWID;
-- keys: app='eldennet.playthrough', schema_min_reader, created_by, last_opened_by, steamid64, slot, fp_version, fp8,
--       name_hash, archetype, gift, first_seen_pt, next_event_id, next_conv_id, current_journey, current_epoch,
--       facts_stale ('1' after a trim, until the core's refold is written; WHY§689)

CREATE TABLE epochs (                       -- timeline segments: open, keep (MEM §9.3), journey (§9.4)
  epoch INTEGER PRIMARY KEY AUTOINCREMENT, journey INTEGER NOT NULL, start_pt INTEGER NOT NULL,
  start_event_id INTEGER NOT NULL, reason TEXT NOT NULL CHECK (reason IN ('open','keep','journey')),
  created_wall INTEGER NOT NULL);

CREATE TABLE events (
  id INTEGER PRIMARY KEY,                  -- core-assigned, 64-bit, never reused
  journey INTEGER NOT NULL, epoch INTEGER NOT NULL REFERENCES epochs(epoch),
  t_play_ms INTEGER NOT NULL, t_play_end_ms INTEGER,        -- end: last page of an NpcSaid
  kind TEXT NOT NULL, rule TEXT, source TEXT, conf REAL NOT NULL,
  subject TEXT, object TEXT,               -- 'player' | 'persona:kale' | 'card:godrick_soldier/veteran' | 'mute'
  conv_id INTEGER, place_id INTEGER, region TEXT, block INTEGER, tod TEXT,
  dkey TEXT,                               -- P dedup key
  text TEXT CHECK (text IS NULL OR length(text) <= 600),    -- speech only, sanitized
  payload TEXT NOT NULL DEFAULT '{}' CHECK (json_valid(payload) AND length(payload) <= 2048));

CREATE TABLE event_audience (
  event_id INTEGER NOT NULL REFERENCES events(id) ON DELETE CASCADE,
  persona TEXT NOT NULL, journey INTEGER NOT NULL,
  how TEXT NOT NULL CHECK (how IN ('direct','overheard','witnessed')),
  PRIMARY KEY (persona, event_id)) WITHOUT ROWID;

CREATE TABLE conversations (
  id INTEGER PRIMARY KEY,                  -- core-assigned from meta.next_conv_id, never reused
  journey INTEGER NOT NULL, epoch INTEGER NOT NULL,
  persona TEXT NOT NULL, stage TEXT NOT NULL,
  started_pt INTEGER NOT NULL, ended_pt INTEGER, end_reason TEXT,
  player_lines INTEGER NOT NULL DEFAULT 0, committed_lines INTEGER NOT NULL DEFAULT 0,
  first_event_id INTEGER, last_event_id INTEGER,
  shown_context TEXT NOT NULL DEFAULT '[]' CHECK (json_valid(shown_context) AND length(shown_context) <= 4096),
  persona_hash TEXT NOT NULL, kb_hash TEXT NOT NULL, model_fp TEXT);

-- npc_facts: a cache of fold(events) (MEM §2.2); its keys include world_seen, world_seen_pt, world_cued and spoke_pt
CREATE TABLE npc_facts (
  journey INTEGER NOT NULL, persona TEXT NOT NULL, key TEXT NOT NULL, value TEXT NOT NULL,
  src_event INTEGER, updated_pt INTEGER NOT NULL,
  PRIMARY KEY (journey, persona, key)) WITHOUT ROWID;

CREATE TABLE npc_journal (                  -- versioned; the newest version is current
  journey INTEGER NOT NULL, persona TEXT NOT NULL, version INTEGER NOT NULL,
  as_of_pt INTEGER NOT NULL, from_event_id INTEGER NOT NULL, upto_event_id INTEGER NOT NULL,
  conv_id INTEGER, summary TEXT NOT NULL,
  keepsakes TEXT NOT NULL CHECK (json_valid(keepsakes) AND json_array_length(keepsakes) <= 8),  -- MEM §2.3's fields
  next_keepsake INTEGER NOT NULL,
  disposition INTEGER NOT NULL CHECK (disposition BETWEEN -2 AND 2),
  told_name TEXT, told_name_verified INTEGER NOT NULL DEFAULT 0, told_name_heard INTEGER NOT NULL DEFAULT 0,
  model_fp TEXT NOT NULL, prompt_hash TEXT NOT NULL, persona_hash TEXT NOT NULL,
  job_id INTEGER, written_wall INTEGER NOT NULL,
  PRIMARY KEY (journey, persona, version)) WITHOUT ROWID;

CREATE TABLE npc_lived (                    -- the bio's lived layer (MEM §13), versioned; the newest version is current
  journey INTEGER NOT NULL, persona TEXT NOT NULL, version INTEGER NOT NULL,
  journal_version INTEGER NOT NULL, as_of_pt INTEGER NOT NULL,   -- the journal version it read, and that one's as_of_pt
  towards TEXT NOT NULL CHECK (json_valid(towards) AND json_array_length(towards) <= 4),
  aims    TEXT NOT NULL CHECK (json_valid(aims) AND json_array_length(aims) <= 2),
  learned TEXT NOT NULL CHECK (json_valid(learned) AND json_array_length(learned) <= 2),
  self_line TEXT CHECK (self_line IS NULL OR json_valid(self_line)), self_pt INTEGER,   -- pt of its last change
  manner TEXT CHECK (manner IS NULL OR json_valid(manner)), manner_pt INTEGER,          -- (MEM §13.5's spacing)
  next_entry INTEGER NOT NULL,
  model_fp TEXT NOT NULL, prompt_hash TEXT NOT NULL, persona_hash TEXT NOT NULL,
  job_id INTEGER, written_wall INTEGER NOT NULL,
  PRIMARY KEY (journey, persona, version)) WITHOUT ROWID;

CREATE TABLE memory_progress (
  journey INTEGER NOT NULL, persona TEXT NOT NULL, watermark INTEGER NOT NULL, updated_pt INTEGER NOT NULL,
  PRIMARY KEY (journey, persona)) WITHOUT ROWID;

CREATE TABLE memory_jobs (
  id INTEGER PRIMARY KEY AUTOINCREMENT, journey INTEGER NOT NULL, persona TEXT NOT NULL,
  kind TEXT NOT NULL DEFAULT 'journal' CHECK (kind IN ('journal','lived')),        -- MEM §3, §13
  cause TEXT CHECK (cause IN ('conversation','exchange','witness')),              -- journal jobs, what made it (§3.1)
  conv_id INTEGER,                         -- the newest conversation merged in; NULL for a witness's own job
  from_event_id INTEGER, upto_event_id INTEGER,       -- journal jobs
  journal_version INTEGER,                 -- lived jobs: the journal version that made it due (§13.2)
  state TEXT NOT NULL CHECK (state IN ('queued','running','deferred','parked','done')),
  attempts INTEGER NOT NULL DEFAULT 0, created_pt INTEGER NOT NULL, last_error TEXT, last_try_wall INTEGER);

CREATE TABLE persona_aliases (old TEXT NOT NULL, new TEXT NOT NULL, since TEXT NOT NULL,
  applied_wall INTEGER NOT NULL, PRIMARY KEY (old, new)) WITHOUT ROWID;

CREATE TABLE forgotten (id INTEGER PRIMARY KEY AUTOINCREMENT, journey INTEGER NOT NULL, persona TEXT,  -- NULL: journey
  at_pt INTEGER NOT NULL, rows_removed INTEGER NOT NULL, wall INTEGER NOT NULL);

-- seams, created now and used later or rarely (DESIGN §13)
CREATE TABLE npc_intents (id INTEGER PRIMARY KEY AUTOINCREMENT, journey INTEGER NOT NULL, persona TEXT NOT NULL,
  event_id INTEGER REFERENCES events(id) ON DELETE CASCADE, t_play_ms INTEGER NOT NULL,
  raw TEXT NOT NULL CHECK (length(raw) <= 240), status TEXT NOT NULL DEFAULT 'ignored');
CREATE TABLE decisions (id INTEGER PRIMARY KEY AUTOINCREMENT, journey INTEGER NOT NULL, t_play_ms INTEGER NOT NULL,
  task TEXT NOT NULL, subject TEXT, options TEXT NOT NULL, chosen TEXT, confidence REAL,
  backend TEXT NOT NULL, latency_ms INTEGER, prompt_hash TEXT);

-- reserved and empty in v1 (DESIGN §13): per-witness memories for recall (MEM §4.5), in §2.3's fields
CREATE TABLE memories (id INTEGER PRIMARY KEY AUTOINCREMENT, journey INTEGER NOT NULL, persona TEXT NOT NULL,
  type TEXT NOT NULL, basis TEXT NOT NULL, about TEXT NOT NULL, emotion TEXT NOT NULL,
  importance REAL NOT NULL, tags TEXT NOT NULL DEFAULT '', place_id INTEGER,
  content TEXT NOT NULL CHECK (length(content) <= 200),
  src_events TEXT NOT NULL,                -- JSON: the sorted 64-bit ids of the events it remembers
  src_key BLOB NOT NULL,                   -- blake3(src_events): one memory per witness and moment
  first_event_id INTEGER, last_event_id INTEGER, t_play_ms INTEGER NOT NULL, updated_wall INTEGER,
  UNIQUE (journey, persona, src_key));
CREATE TABLE memory_actors (memory_id INTEGER NOT NULL REFERENCES memories(id) ON DELETE CASCADE,
  actor TEXT NOT NULL,                     -- 'player' | 'persona:<id>' | 'card:<id>': what recall gathers by
  PRIMARY KEY (actor, memory_id)) WITHOUT ROWID;
CREATE VIRTUAL TABLE memories_fts USING fts5(content, tags, content='memories', content_rowid='id');
CREATE TABLE embeddings (memory_id INTEGER PRIMARY KEY REFERENCES memories(id) ON DELETE CASCADE,
  producer TEXT NOT NULL,                  -- blake3(model id ‖ revision ‖ dim ‖ normalisation ‖ text template)
  dim INTEGER NOT NULL, vec BLOB NOT NULL, created_wall INTEGER NOT NULL);

CREATE INDEX events_jt      ON events (journey, t_play_ms);
CREATE INDEX events_et      ON events (epoch, t_play_ms);
CREATE INDEX events_conv    ON events (conv_id) WHERE conv_id IS NOT NULL;
CREATE INDEX events_kind    ON events (journey, kind);
CREATE INDEX audience_event ON event_audience (event_id);
CREATE INDEX audience_pj    ON event_audience (persona, journey, event_id);
CREATE INDEX conv_pj        ON conversations (persona, journey, started_pt);
CREATE INDEX conv_open      ON conversations (ended_pt) WHERE ended_pt IS NULL;
CREATE INDEX jobs_state     ON memory_jobs (state, journey, persona);
CREATE INDEX journal_asof   ON npc_journal (journey, as_of_pt);
CREATE INDEX lived_asof     ON npc_lived (journey, as_of_pt);
CREATE INDEX memories_pj    ON memories (persona, journey, t_play_ms);
CREATE INDEX intents_pj     ON npc_intents (persona, journey);
