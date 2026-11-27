#pragma once
// The store's messages (ARCH §1.3's C8, §2.2; MEM §8.6): what the core asks of en-store, on T9, and what comes back.
// The core sends a `DbRequest` with a sequence number of its own; the store answers each with one `DbResult` of the
// same number, in order. Rows are plain data: the store keeps the journal's keepsakes and the lived layer's parts as
// the JSON texts the core writes (MEM §8.7), and never reads what they say.
//
// Ids (MEM §0, §10; ARCH §2.2's store rules): an event id is 64-bit, assigned by the core from `next_event_id`,
// monotonic per playthrough file and never reused, even after a trim; every other row id is the store's, never reused
// either. Play time (`pt`) is the game's, in milliseconds, and decides rollback; wall time (seconds since the epoch)
// appears only in file names and diagnostics.
//
// These messages carry chat text, model text and the names of the player's file, so they have no JSON form and are
// never logged, recorded or reported (PV rules): they go from T6 to T9 and back, and nowhere else.
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace en::proto {

using EventId = std::int64_t;  // the core's, from meta.next_event_id
using RowId = std::int64_t;    // the store's: conversations, jobs
using Pt = std::int64_t;       // play time, ms

// How an audience row perceived the event (DESIGN §5.4; MEM §1.2). Augur's `How` is this one.
enum class How : std::uint8_t { Direct, Overheard, Witnessed };
[[nodiscard]] const char* to_string(How h) noexcept;  // "direct", "overheard", "witnessed"
[[nodiscard]] std::optional<How> how_from(std::string_view word) noexcept;

// One audience row, frozen when the event was emitted (MEM §1.2): a profile id (the slug, after the alias table).
struct AudienceRow {
  std::string persona;
  How how = How::Witnessed;
  friend bool operator==(const AudienceRow&, const AudienceRow&) = default;
};

// A P event as the store keeps it (MEM §8.7's `events`). `subject` and `object` are actor keys: "player",
// "persona:<id>", "card:<id>/<variant>" or "mute"; empty for none. `text` is speech only, sanitized; `payload` is the
// kind's JSON object (at most 2,048 bytes: an NpcSaid's pages and their pt, its `to`, `motive` and `cue`; a
// ConversationEnded's `known`). `audience` holds every row when the core writes the event, and, in a query, the
// asking persona's row alone: no reader sees who else perceived it (MEM §1.3).
struct StoredEvent {
  EventId id = 0;
  std::uint16_t journey = 1;
  std::int64_t epoch = 0;  // the store's, on writing: the current epoch
  Pt t_play_ms = 0;
  std::optional<Pt> t_play_end_ms;  // an NpcSaid's last page
  std::string kind;
  std::string rule, source;
  double conf = 1.0;
  std::string subject, object;
  std::optional<RowId> conv_id;
  std::optional<std::int64_t> place_id;
  std::string region;
  std::optional<std::uint32_t> block;
  std::string tod;
  std::string dkey;
  std::optional<std::string> text;
  std::string payload = "{}";
  std::vector<AudienceRow> audience;
  friend bool operator==(const StoredEvent&, const StoredEvent&) = default;
};

// A fact as `npc_facts` keeps it (MEM §2.2): a cache of the core's fold, by key.
struct FactRow {
  std::string key;
  std::string value;
  std::optional<EventId> src_event;
  Pt updated_pt = 0;
  friend bool operator==(const FactRow&, const FactRow&) = default;
};

struct PersonaFacts {
  std::string persona;
  std::vector<FactRow> facts;
  friend bool operator==(const PersonaFacts&, const PersonaFacts&) = default;
};

// A conversation's row (MEM §8.7's `conversations`).
struct ConversationRow {
  RowId id = 0;  // the core's ConvId, unique in the file
  std::uint16_t journey = 1;
  std::int64_t epoch = 0;  // the store's
  std::string persona, stage;
  Pt started_pt = 0;
  std::optional<Pt> ended_pt;
  std::string end_reason;  // empty while open; "crash", "trimmed" or the core's reason
  std::uint32_t player_lines = 0, committed_lines = 0;
  std::optional<EventId> first_event_id, last_event_id;
  std::string shown_context = "[]";  // JSON, at most 4,096 bytes
  std::string persona_hash, kb_hash;
  std::optional<std::string> model_fp;
  friend bool operator==(const ConversationRow&, const ConversationRow&) = default;
};

// A journal version (MEM §2.3, §8.7's `npc_journal`): `keepsakes` is the core's JSON array (at most 8).
struct JournalVersion {
  std::uint16_t journey = 1;
  std::string persona;
  std::uint32_t version = 0;
  Pt as_of_pt = 0;
  EventId from_event_id = 0, upto_event_id = 0;
  std::optional<RowId> conv_id;
  std::string summary;
  std::string keepsakes = "[]";
  std::uint32_t next_keepsake = 1;
  std::int8_t disposition = 0;
  std::optional<std::string> told_name;
  bool told_name_verified = false, told_name_heard = false;
  std::string model_fp, prompt_hash, persona_hash;
  std::optional<RowId> job_id;
  std::int64_t written_wall = 0;
  friend bool operator==(const JournalVersion&, const JournalVersion&) = default;
};

// A lived version (MEM §13, §8.7's `npc_lived`): its parts are the core's JSON.
struct LivedVersion {
  std::uint16_t journey = 1;
  std::string persona;
  std::uint32_t version = 0;
  std::uint32_t journal_version = 0;
  Pt as_of_pt = 0;
  std::string towards = "[]", aims = "[]", learned = "[]";
  std::optional<std::string> self_line, manner;
  std::optional<Pt> self_pt, manner_pt;
  std::uint32_t next_entry = 1;
  std::string model_fp, prompt_hash, persona_hash;
  std::optional<RowId> job_id;
  std::int64_t written_wall = 0;
  friend bool operator==(const LivedVersion&, const LivedVersion&) = default;
};

// A memory job (MEM §3.2, §13.2, §8.7's `memory_jobs`).
enum class JobKind : std::uint8_t { Journal, Lived };
enum class JobCause : std::uint8_t { None, Conversation, Exchange, Witness };
enum class JobState : std::uint8_t { Queued, Running, Deferred, Parked, Done };
[[nodiscard]] const char* to_string(JobKind k) noexcept;
[[nodiscard]] const char* to_string(JobCause c) noexcept;  // "" for None
[[nodiscard]] const char* to_string(JobState s) noexcept;

struct MemoryJob {
  RowId id = 0;  // 0: a new job (UpsertJob gives it its id)
  std::uint16_t journey = 1;
  std::string persona;
  JobKind kind = JobKind::Journal;
  JobCause cause = JobCause::None;
  std::optional<RowId> conv_id;
  std::optional<EventId> from_event_id, upto_event_id;
  std::optional<std::uint32_t> journal_version;
  JobState state = JobState::Queued;
  std::uint32_t attempts = 0;
  Pt created_pt = 0;
  std::string last_error;
  std::optional<std::int64_t> last_try_wall;
  friend bool operator==(const MemoryJob&, const MemoryJob&) = default;
};

// The playthrough's key (MEM §8.3): computed by the core, which alone holds the character's name. `fp8` is the first
// 8 hex digits of the fingerprint, `name_hash` the hex of BLAKE3 over the name; the store never sees the name.
struct PlaythroughKey {
  std::uint64_t steamid64 = 0;
  std::uint8_t slot = 0;
  std::string fp8;
  std::string name_hash;
  std::uint8_t archetype = 0;
  std::uint8_t gift = 0xFF;  // 0xFF: the layout does not read it
  friend bool operator==(const PlaythroughKey&, const PlaythroughKey&) = default;
};

// ---- Requests ----

namespace db {

// Open the playthrough's file (MEM §8.3), take its start backup, migrate it, and check its timeline (MEM §9.1).
struct Open {
  PlaythroughKey key;
  Pt pt_now = 0;
  std::uint16_t journey = 1;  // ng_lvl + 1
};
// The player's answer to the rollback choice (MEM §9.1).
enum class Choice : std::uint8_t { Forget, Keep, NewCharacter };
struct Resolve {
  Choice choice = Choice::Forget;
};
struct Close {};
// A P event with its frozen audience, and the facts its fold changed, in one transaction.
struct Append {
  StoredEvent event;
  std::vector<PersonaFacts> facts;
};
// A later page of an NpcSaid that is shown (MEM §8.6): its text is appended, and the page's end and pt go into the
// payload's `pages`.
struct AppendPage {
  EventId event = 0;
  std::string text;
  Pt page_pt = 0;
};
struct ConvOpen {
  ConversationRow row;
};
struct ConvClose {
  ConversationRow row;  // its id, end and counts; the open fields are kept
};
struct UpsertJob {
  MemoryJob job;
};
// A new journal version, the watermark, the job done and the facts it set (`verified:*`), in one transaction.
struct CommitJournal {
  JournalVersion version;
  std::optional<RowId> job;
  std::vector<FactRow> facts;
};
struct CommitLived {
  LivedVersion version;
  std::optional<RowId> job;
};
// The journey's facts, refolded by the core (WHY§689): every persona's facts in the journey are replaced, and
// `meta.facts_stale` is cleared, in one transaction.
struct PutFacts {
  std::uint16_t journey = 1;
  std::vector<PersonaFacts> personas;
};
// Forget one profiled NPC, or the whole journey when `persona` is empty (MEM §9.5).
struct Forget {
  std::uint16_t journey = 1;
  std::string persona;
  Pt at_pt = 0;
};
struct Checkpoint {};  // a passive checkpoint: at rest and on loading screens (MEM §8.5)

// Queries.
struct Bundle {  // MEM §4.1
  std::uint16_t journey = 1;
  std::string persona;
};
struct JobInput {  // MEM §3.3: the evidence of (watermark, upto]
  RowId job = 0;
};
struct LivedInput {  // MEM §13.3
  std::uint16_t journey = 1;
  std::string persona;
};
struct EventsForFold {  // MEM §9.2: every event of the journey, with every audience row
  std::uint16_t journey = 1;
};

}  // namespace db

struct DbRequest {
  std::uint64_t seq = 0;
  std::variant<db::Open, db::Resolve, db::Close, db::Append, db::AppendPage, db::ConvOpen, db::ConvClose,
               db::UpsertJob, db::CommitJournal, db::CommitLived, db::PutFacts, db::Forget, db::Checkpoint,
               db::Bundle, db::JobInput, db::LivedInput, db::EventsForFold>
      op;
  // Speech (a PlayerSaid, or an NpcSaid page) takes the speech reserve of the spill ring (MEM §8.6).
  [[nodiscard]] bool speech() const noexcept;
};

// ---- Results ----

namespace db {

// What the store failed with, by SQLite's class (MEM §8.6), or the store's own.
enum class Fault : std::uint8_t {
  Busy,        // the file was locked past busy_timeout: never counted by the breaker
  Full,        // the disk is full
  IoErr,       // trips the breaker at once
  Corrupt,     // trips it at once; the file is renamed
  NotADb,      // trips it at once
  Constraint,  // a row the schema refuses: the request's fault
  TooNew,      // a newer schema than this build reads (MEM §10)
  NotOpen,     // no playthrough file is open, or the store tripped
  Invalid,     // a request the store refuses before SQL: a cap, an unknown persona key
  Other,
};
[[nodiscard]] const char* to_string(Fault f) noexcept;

// The timeline's check at open (MEM §9.1).
struct Rollback {
  enum class Kind : std::uint8_t { Clean, Trimmed, Choice };
  Kind kind = Kind::Clean;
  Pt delta_ms = 0;            // H − pt_now
  Pt cut = 0;                 // Trimmed: the cut
  bool lower_journey = false;  // Choice: the save is from an earlier journey than the file's
  std::uint64_t rows = 0;     // Trimmed: rows removed
};

struct Opened {
  Rollback rollback;
  EventId next_event_id = 1;
  std::uint16_t journey = 1;  // the file's current journey after the check
  std::int64_t epoch = 0;
  bool created = false;           // a new file
  bool restored_backup = false;   // the live file was corrupt and the newest backup took its place
  bool restored_archive = false;  // an archived file of this character came back
  bool archived_other = false;    // another character's file in the slot was archived
  bool migrated = false;
  bool low_disk = false;          // under 200 MB free
  bool facts_stale = false;       // the core refolds and sends PutFacts
  std::vector<RowId> closed_conversations;  // closed by crash recovery (MEM §3.8)
  std::vector<MemoryJob> open_jobs;         // queued, running (now queued again), deferred or parked
};
// After Resolve; also what Open answers once a Choice is resolved.
struct Resolved {
  Opened opened;
};
struct Done {};
struct JobSaved {
  RowId id = 0;
};
struct Failed {
  Fault fault = Fault::Other;
};
// The store tripped for this file and this session (MEM §8.6): memory is off, prompts use RAM state.
struct Tripped {
  Fault cause = Fault::Other;
};

struct BundleResult {
  std::optional<JournalVersion> journal;
  std::optional<LivedVersion> lived;
  std::vector<FactRow> facts;
  // The last committed direct lines of the most recent ended conversation with the persona (block 6a), oldest first.
  std::vector<StoredEvent> prev_tail;
  std::optional<RowId> prev_conv;
  std::uint32_t pending_count = 0;  // events above the watermark visible to the persona
};
struct JobInputResult {
  MemoryJob job;
  std::optional<JournalVersion> journal;
  EventId watermark = 0;
  std::vector<StoredEvent> evidence;          // above the watermark, up to the job's upto, the persona's own rows
  std::vector<ConversationRow> conversations;  // those the evidence's speech belongs to, for `shown_context`
};
struct LivedInputResult {
  std::optional<LivedVersion> lived;
  std::optional<JournalVersion> journal;
  std::vector<FactRow> facts;
};
struct FoldEvents {
  std::uint16_t journey = 1;
  std::vector<StoredEvent> events;  // in id order, every audience row
};

}  // namespace db

struct DbResult {
  std::uint64_t seq = 0;  // the request's
  std::variant<db::Done, db::Opened, db::Resolved, db::JobSaved, db::Failed, db::Tripped, db::BundleResult,
               db::JobInputResult, db::LivedInputResult, db::FoldEvents>
      result;
};

}  // namespace en::proto
