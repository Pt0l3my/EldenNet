#pragma once
// Tools › Workshop (PAGES §4.6.8, SP8; GraceUI D258; roadmap S11.4.a): what the DLL keeps on T6 while Developer views
// (`debug.panel`) are on, in plain data that en-overlay's space publishes on Workshop's topics, in the browser alone,
// behind its gate and under its ribbon. Nothing of it is kept while the switch is off, and nothing of it is ever
// written: it is gone at the next start.
//
// Workshop is SP1's one exception: it shows the prompt as sent, the reply as the model wrote it, every event, flags,
// stages and what each character knows. What is still kept out, here as everywhere (PV2, PV5): a key (each configured
// key's value, and anything shaped like one), the character's name (the playthrough's, matched whole), a SteamID, an
// absolute path or a local address (a server's message is left out, a model id shaped like a path too). The DLL
// scrubs every text before it is kept here; en-overlay scrubs paths and addresses again as it publishes.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "en/proto/llm.hpp"

namespace en::proto {

inline constexpr std::size_t kWorkshopMessagesMax = 64 * 1024;  // bytes of the prompt's messages kept, in all
inline constexpr std::size_t kWorkshopReplyMax = 16 * 1024;     // bytes of the reply's text kept, and of its raw text
inline constexpr std::size_t kWorkshopEventRing = 500;          // the events kept, the newest
inline constexpr std::size_t kWorkshopEventLineMax = 1024;      // bytes of one event's line
inline constexpr std::size_t kWorkshopUnknownMax = 64;          // unknown characters kept (the least recently seen go)
inline constexpr std::size_t kWorkshopRowsMax = 4096;           // a table's rows (the flags, the known sets)
inline constexpr std::size_t kWorkshopBodyMax = 240 * 1024;     // bytes of a topic's body, below GraceUI's frame

// ---- The last prompt and reply: the newest call of the conversation or the profile editor (O-g's (a)) ----

struct WorkshopMessage {
  std::string role;  // "system", "user", "assistant"
  std::string text;  // its parts joined as sent, scrubbed
  bool cache_break = false;

  friend bool operator==(const WorkshopMessage&, const WorkshopMessage&) = default;
};

struct WorkshopExchange {
  bool any = false;              // a call was made since Developer views came on
  RequestId id = 0;
  std::uint64_t at_ms = 0;       // when it was sent, since T1's start
  std::string slot;              // en-proto's slot name
  std::string purpose;           // calls.hpp's words: "reply", "own-initiative line", "profile draft"
  std::vector<WorkshopMessage> messages;
  std::size_t messages_cut = 0;  // bytes of the messages left out past kWorkshopMessagesMax
  int max_tokens = -1;           // -1: the slot's
  double temperature = -1;       // -1: the slot's
  bool schema = false;           // a JSON schema was asked for
  std::size_t stops = 0;         // stop sequences
  // Its reply.
  bool started = false;          // its first attempt was sent
  bool ended = false;
  std::string provider;          // a providers.toml id
  std::string model;             // as the configuration names it; empty when shaped like a path (PV5)
  std::string served_model;      // as the provider named it; likewise
  std::string text;              // what streamed so far, then the reply as the model client gave it, scrubbed
  std::string raw;               // the content exactly as sent (its reasoning included), when it differs from `text`
  std::size_t text_cut = 0;      // bytes left out past kWorkshopReplyMax
  std::size_t raw_cut = 0;
  std::string finish;            // the provider's finish reason
  ErrorClass error = ErrorClass::None;
  int http_status = 0;
  std::int32_t tokens_in = -1;   // as reported; -1: not reported
  std::int32_t tokens_out = -1;
  std::int32_t tokens_cached = -1;
  std::int64_t first_token_ms = -1;
  std::int64_t total_ms = -1;
  std::uint32_t attempts = 0;

  friend bool operator==(const WorkshopExchange&, const WorkshopExchange&) = default;
};

// ---- Every event, unfiltered: one of Augur's, in the golden files' form (the stream's row) ----

struct WorkshopEvent {
  std::uint64_t t_ms = 0;  // since T1's start
  std::string kind;        // "Struck"
  std::string cls;         // "P", "K", "T"
  std::string source;      // "snapshot_diff", "lifecycle", ...
  std::string line;        // the golden line (en-augur's event_json), its handles named h1, h2, ... for the session

  friend bool operator==(const WorkshopEvent&, const WorkshopEvent&) = default;
};

struct WorkshopEventCounts {
  std::uint64_t seen = 0;       // events since Developer views came on
  std::uint64_t dropped_t = 0;  // the bus's drops this session (its FIFO's cap): T events
  std::uint64_t dropped_k = 0;  // and K events

  friend bool operator==(const WorkshopEventCounts&, const WorkshopEventCounts&) = default;
};

// ---- The world view with raw verification states (MEM §6, §12; en-core's knowledge.hpp) ----

struct WorkshopFlag {
  std::string alias;
  std::uint32_t id = 0;       // 0: unresolved
  std::string block;          // its block's alias, else empty
  std::uint8_t offset = 0;
  bool watched = false;       // in the watch list: its bit is read
  bool bit = false;
  bool valid = false;         // its id exists in the game
  bool verified_file = false; // the catalogue says so
  bool verified_here = false; // in this realm
  std::string reads;          // "T", "U", "F"
  std::string kind;           // "outcome", "state"
  std::string ng;             // "reset", "keep", "unknown"
  std::uint8_t spoiler = 0;

  friend bool operator==(const WorkshopFlag&, const WorkshopFlag&) = default;
};

struct WorkshopStage {
  std::string id;
  std::string value;  // "T", "U", "F"
  std::string when;
  std::string boss;
  std::uint8_t spoiler = 0;
  std::uint8_t rank = 0;

  friend bool operator==(const WorkshopStage&, const WorkshopStage&) = default;
};

struct WorkshopTrack {
  std::string id;
  std::string step;  // the active step, or empty

  friend bool operator==(const WorkshopTrack&, const WorkshopTrack&) = default;
};

struct WorkshopWorld {
  bool have_frame = false;
  std::uint8_t journey = 1;
  std::uint32_t watch_version = 0;
  std::size_t watched = 0;  // ids in the watch list
  std::string region;       // the Tarnished's region key, or empty
  std::vector<WorkshopFlag> flags;
  std::vector<WorkshopStage> stages;
  std::vector<WorkshopTrack> tracks;
  std::size_t flags_cut = 0;

  friend bool operator==(const WorkshopWorld&, const WorkshopWorld&) = default;
};

// ---- Each persona's known set (MEM §12.5, §12.6); the reaction cue waits for v0.3 (O-c's (a)) ----

struct WorkshopKnown {
  std::string persona;    // its id
  std::string stage;      // its active stage's id
  std::string item;       // "w:<id>" or "q:<track>.<step>"
  std::string certainty;  // "fact", "rumour", "belief"
  std::string path;       // "own", "kept", "tie", "sky", "near", "word"
  bool own_affairs = false;
  std::string block5b;    // "shown", "offered" or empty

  friend bool operator==(const WorkshopKnown&, const WorkshopKnown&) = default;
};

struct WorkshopKnownSets {
  std::vector<WorkshopKnown> rows;
  std::size_t personas = 0;
  std::size_t cut = 0;

  friend bool operator==(const WorkshopKnownSets&, const WorkshopKnownSets&) = default;
};

// ---- Unknown characters: those of the snapshots that bind no persona (identity.hpp), with their TOML ----

struct WorkshopUnknown {
  std::uint32_t param = 0;
  std::uint32_t entity = 0;
  std::uint32_t model = 0;
  std::uint8_t kind = 0;
  std::uint8_t team = 0;
  std::uint64_t first_ms = 0;  // since T1's start
  std::uint64_t last_ms = 0;
  std::uint64_t seen = 0;      // snapshots it was in
  float nearest_m = 0;
  std::string toml;            // a [[persona]] row for identity/personas.toml: ids and numbers only

  friend bool operator==(const WorkshopUnknown&, const WorkshopUnknown&) = default;
};

// ---- The recorder (ARCH §7.5; debug.record_sessions; the space's recording.set) ----

enum class RecordingWhy : std::uint8_t {
  Off,         // never on this session
  AtStart,     // Record sessions was on at the start
  TestRun,     // an S0 test run records whatever the settings say
  StartedHere, // started from Workshop
  StoppedHere, // stopped from Workshop
  Failed,      // a file could not be opened
};

struct WorkshopRecording {
  bool on = false;
  RecordingWhy why = RecordingWhy::Off;
  std::uint64_t file = 0;     // its file's number this session, from 1; 0: none yet
  std::uint64_t lines = 0;    // this session
  std::uint64_t refused = 0;  // lines the writer refused (a defect of ours)
  std::uint64_t bytes = 0;    // of the file being written
  std::uint64_t rotate_bytes = 0;

  friend bool operator==(const WorkshopRecording&, const WorkshopRecording&) = default;
};

// ---- Augur's event kinds and schemas, and the rules loaded ----

struct WorkshopKind {
  std::string name;
  std::string built_by;      // its payload builder's name, or empty
  std::string waits_for;     // what a kind with no builder waits for, or empty
  std::string placeholders;  // its builder's, joined
  std::string fields;        // its payload's fields as the golden form writes them, joined
  std::size_t rules = 0;     // the rules loaded that make it

  friend bool operator==(const WorkshopKind&, const WorkshopKind&) = default;
};

struct WorkshopRule {
  std::string id;
  std::string kind;
  std::string cls;
  std::string signal;
  std::string detector;
  std::string radius;
  std::string key;
  std::uint32_t ttl_s = 0;
  std::uint32_t cooldown_s = 0;
  std::string where;  // its file and line

  friend bool operator==(const WorkshopRule&, const WorkshopRule&) = default;
};

struct WorkshopKinds {
  std::vector<WorkshopKind> kinds;
  std::vector<WorkshopRule> rules;

  friend bool operator==(const WorkshopKinds&, const WorkshopKinds&) = default;
};

// ---- All of it ----

struct WorkshopView {
  WorkshopExchange exchange;
  WorkshopEventCounts events;
  WorkshopWorld world;
  WorkshopKnownSets known;
  std::vector<WorkshopUnknown> unknown;  // the most recently seen first
  WorkshopRecording recording;
  WorkshopKinds kinds;

  friend bool operator==(const WorkshopView&, const WorkshopView&) = default;
};

[[nodiscard]] const char* words(RecordingWhy w) noexcept;  // the recorder's state in the player's words
[[nodiscard]] const char* to_string(RecordingWhy w) noexcept;  // "off", "at_start", ...

}  // namespace en::proto
