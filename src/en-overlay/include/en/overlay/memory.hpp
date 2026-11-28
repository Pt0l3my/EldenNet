#pragma once
// EldenNet's memory pages (S12.2.a and S12.2.b without Speaking up; PAGES §4.6.1, §4.6.4, §4.6.5; the spoiler rules SP1
// to SP9, PAGES §4.8; the privacy rules, §4.9): what the playthrough's file holds, as the DLL read it from the store
// (en-store's page queries) and the core, in en-overlay's own types (it links neither en-core nor en-store, ARCH §2.3).
// Portable, on nlohmann/json: no Win32, no call to GraceUI. The space (space.hpp) registers them.
//
// - **The Journey** (the space's scope, GraceUI D254): `journeys`, {choices: [{id, label, read_only}], current}, every
//   journey the file holds, an earlier one read only; empty before a playthrough is loaded.
// - **Characters › Playthrough** (`characters.playthrough`): `playthrough/<journey>`, the people met on that journey
//   (met at least once, SP4, SP5), each with its status word from perception alone (SP7: Here now, Fallen, Alive when
//   last met), when last met and how often; `met/<journey>.<persona>`, the chosen one's Status card (times met, when
//   first and last, the disposition word, the facts in words). Never a stage, a story line or a count of what is left
//   (SP1, SP5): their story lines and World now come with S12.4 (0.5).
// - **History › Conversations** (`history.conversations`): `conversations/<journey>`, every conversation of the
//   journey with a named NPC, newest first (the NPC as shown, when, how many lines, how it ended), its key the
//   conversation's id as a string of digits; `conversation/<id>`, its lines as they were shown, the Tarnished's typed
//   ones and the NPC's displayed ones, nothing unshown (SP2). Both private (PV4; GraceUI D274).
// - **History › Memories** (`history.memories`): `memories/<journey>`, each NPC with a profile that remembers
//   something of the journey; `journal/<journey>.<persona>`, its card (the journal's summary, the disposition word, the
//   name the Tarnished told, when written, the facts in words from MEM §2.2's rendered keys only, never `stage_seen`,
//   `world_*`, `spoke_pt` or `verified:`, SP1), its keepsakes (each one witness's own, at most 8, the protected one
//   marked), its memory work, and its Forget; and `memory.state`, the backups' count and times (no path) and whether a
//   forget may go now. Private where they hold the memory job's words (PV4).
// - **Home's Nearby** (`nearby`): the met NPCs perceived now (SP7), by name as the game shows it, "speaking" while in a
//   conversation, and the disposition word once a journal holds one; no distance, no number.
// - **Commands** (the space's one command path): `memory.forget_npc` {key, confirm} and `memory.forget_journey`
//   {confirm}, each confirmed, its second press counting after 1 s (`confirm_after_ms`, D274), and refused unless the
//   journey is the open one and nothing is talking (a backup first, MEM §9.5). The DLL makes them (MemoryRequest).
//   Watching `conversation/<id>` asks the DLL for that transcript.
// - **Words.** Every word a player reads here is a draft for the owner's review (v0.3's words file lists them).
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace en::overlay {

inline constexpr std::string_view kJourneysTopic = "journeys";
inline constexpr std::string_view kPlaythroughFamily = "playthrough";
inline constexpr std::string_view kMetFamily = "met";
inline constexpr std::string_view kConversationsFamily = "conversations";
inline constexpr std::string_view kConversationFamily = "conversation";
inline constexpr std::string_view kMemoriesFamily = "memories";
inline constexpr std::string_view kJournalFamily = "journal";
inline constexpr std::string_view kMemoryStateTopic = "memory.state";
inline constexpr std::string_view kNearbyTopic = "nearby";
inline constexpr std::string_view kForgetNpc = "memory.forget_npc";
inline constexpr std::string_view kForgetJourney = "memory.forget_journey";

struct KeepsakeFacts {
  std::string text;      // the memory job's words (PV4)
  double weight = 0;     // 0 to 1
  bool told = false;     // a claim: "You told them" (MEM §2.5), else "Seen"
  std::string about;     // our label: "you", "themselves", "the world", or another NPC's name as the game shows it
  bool kept = false;     // the protected one (MEM §2.3)
  std::uint64_t pt = 0;  // play time
  bool operator==(const KeepsakeFacts&) const = default;
};

// One NPC with a profile on one journey, as the store and the core know it.
struct MemberFacts {
  std::string persona;  // its profile id
  std::string name;     // as the game shows it, or the first-meeting name (SP3)
  std::uint32_t met = 0, seen = 0, struck = 0, fought_beside = 0, saw_fall = 0;
  std::optional<std::uint64_t> first_met_pt, last_met_pt;
  bool here = false;      // perceived now (SP7)
  bool fell = false;      // the Tarnished saw them die (SP7)
  bool speaking = false;  // in a conversation now
  // The journal (MEM §2.3), the memory job's words: on History › Memories alone (PV4).
  bool journal = false;
  std::string summary;
  int disposition = 0;  // -2 to 2
  std::optional<std::uint64_t> written_pt;
  std::string told_name;
  std::vector<KeepsakeFacts> keepsakes;
  std::uint32_t queued = 0, running = 0;  // its memory work
  std::string last_job;                   // the newest job's state in our word: "done", "parked", ...
  bool operator==(const MemberFacts&) const = default;
};

struct ConversationFacts {
  std::uint64_t id = 0;
  std::string persona, name;
  std::uint64_t started_pt = 0;
  std::optional<std::uint64_t> ended_pt;
  std::string end_reason;  // the core's word, "crash" or "trimmed"; empty while open
  std::uint32_t lines = 0;  // shown: the Tarnished's and the NPC's committed lines
  bool operator==(const ConversationFacts&) const = default;
};

struct LineFacts {
  bool tarnished = false;
  std::string speaker;  // the NPC's name as shown; empty for the Tarnished's
  std::string text;     // as it was shown (PV4)
  std::uint64_t pt = 0;
  bool operator==(const LineFacts&) const = default;
};

// A time as the PC's clock shows it (its own time zone): the DLL converts a wall time with Win32's own conversion
// (WHY§696: no zoned_time in DLL modules); the tests give their own.
struct LocalTime {
  int year = 1970, month = 1, day = 1, hour = 0, minute = 0;  // month 1 to 12
  bool operator==(const LocalTime&) const = default;
};
// "27 November 2026, 14:05": the day, the month's English name, the year, the 24-hour time; no zone (O148).
[[nodiscard]] std::string time_words(const LocalTime& t);

struct JourneyFacts {
  std::uint16_t journey = 1;
  bool current = false;
  std::vector<MemberFacts> members;              // by name
  std::vector<ConversationFacts> conversations;  // newest first
  bool operator==(const JourneyFacts&) const = default;
};

struct MemoryFacts {
  std::uint64_t revision = 0;  // changes whenever anything below does
  bool loaded = false;         // a playthrough is loaded and its file open
  bool can_forget = false;     // the file open on its current journey, and no conversation open
  std::uint16_t current = 1;
  std::vector<JourneyFacts> journeys;  // in order
  std::map<std::uint64_t, std::vector<LineFacts>> transcripts;  // the conversations read, by id
  std::uint32_t backups = 0;
  std::vector<LocalTime> backup_times;  // newest first, in the PC's own time zone (the DLL converts them, O148)
  [[nodiscard]] const JourneyFacts* journey(std::uint16_t j) const noexcept;
};

// What a command or a watch asks the DLL for.
struct MemoryRequest {
  enum class Kind : std::uint8_t { ForgetNpc, ForgetJourney, Transcript };
  Kind kind = Kind::Transcript;
  std::uint16_t journey = 1;
  std::string persona;    // ForgetNpc
  std::uint64_t conv = 0;  // Transcript
  bool operator==(const MemoryRequest&) const = default;
};

// The words for the disposition (-2 to 2), as History › Memories and Nearby show it.
[[nodiscard]] std::string_view disposition_word(int d) noexcept;
// Play time as the pages write it: "1:02" (hours and minutes).
[[nodiscard]] std::string play_clock(std::uint64_t ms);

class MemoryPages {
 public:
  // Whether a topic is one of these pages', and the body for it (null facts: the title screen's words or nothing).
  [[nodiscard]] static bool owns(std::string_view topic) noexcept;
  [[nodiscard]] static nlohmann::json body(std::string_view topic, const MemoryFacts* facts);
  // A member's key, "<journey>.<persona>", split; none for anything else.
  [[nodiscard]] static std::optional<std::pair<std::uint16_t, std::string>> member_key(std::string_view key);
  // One of the two forget commands: its error code and words, or "" with the request queued.
  std::string command(std::string_view name, const nlohmann::json& args, const MemoryFacts* facts);
  // Each collect: a watched `conversation/<id>` the facts lack is asked of the DLL, once.
  void want(const std::set<std::string>& watched, const MemoryFacts* facts);
  [[nodiscard]] std::vector<MemoryRequest> take_requests();

 private:
  std::vector<MemoryRequest> requests_;
  std::set<std::uint64_t> asked_;
};

}  // namespace en::overlay
