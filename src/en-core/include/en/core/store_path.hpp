#pragma once
// The store in the core (ARCH §9; MEM §1.5, §8.6, §9.1; v0.3 commit 9). The core speaks to en-store only in C8's
// messages (en-proto's store.hpp): it answers requests in Outputs::db, and the shell hands back each DbResult as an
// input. The core never links en-store.
//
// - **The key** (MEM §8.3): `playthrough_key` from the account the shell found (MEM §8.2), the slot, the name, the
//   archetype and the starting gift. The name is hashed here and goes no further.
// - **Opening** (ARCH §9): the lifecycle waits in Playthrough for Opened; a lock held at open (Busy) is retried 3
//   times, 1 s apart, and then memory is off for the session, as for any other failure. Once Opened (or Resolved), the
//   core refolds the journey's facts from its events (EventsForFold), writes them back when the file says they are
//   stale (PutFacts; WHY§689), and only then lifts the writes it held.
// - **The choice** (MEM §9.1, O144): the world goes on (InWorld), named chat is refused with a status toast, and writes
//   wait in a buffer of 4,096 (the oldest non-speech dropped first) until the player answers; there is never an answer
//   of the core's own. The summary is for the panel (slice 3).
// - **Writes** (MEM §1.5, §8.6): each P event of the step with its frozen audience and the facts its fold moved (one
//   Append), each conversation's row, and each later page of a reply. Ids come from the file's counters. Speech goes
//   out in the step it happens, everything queued before it with it; other writes are coalesced and go out at most
//   every 250 ms.
// - **Memory off** (MEM §8.6): a failed open or a tripped store leaves the session in RAM (the core's MemoryLog), with
//   one toast naming the cause.
//
// The requests carry chat text and the file's key: never logged or recorded (PV rules).
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/augur/events.hpp"
#include "en/core/event_log.hpp"
#include "en/core/identity.hpp"
#include "en/core/knowledge.hpp"
#include "en/core/memory.hpp"
#include "en/core/toasts.hpp"
#include "en/proto/store.hpp"

namespace en::core {

// MEM §8.3's fingerprint and key. `name` is the character's name as UTF-8; the fingerprint hashes it as UTF-16LE code
// units, as the save holds it. `gift` is 0xFF when the layout does not read it.
[[nodiscard]] proto::PlaythroughKey playthrough_key(std::uint64_t steamid64, std::uint8_t slot, std::string_view name,
                                                    std::uint8_t archetype, std::uint8_t gift);

// What the core shows of the store: toasts (the words are drafts for the owner, src/en-core/README.md).
struct StoreNotice {
  ToastClass cls = ToastClass::Degraded;
  proto::ToastLevel level = proto::ToastLevel::Warning;
  std::string text;
  friend bool operator==(const StoreNotice&, const StoreNotice&) = default;
};
inline constexpr std::string_view kChoicePending = "(Not yet: EldenNet is waiting for your answer about this save.)";
inline constexpr std::string_view kBackupRestored =
    "EldenNet's memory file was damaged, so its newest backup was restored.";
inline constexpr std::string_view kLowDisk = "Less than 200 MB of disk is free: EldenNet may stop saving memories.";
// The shell's spill ring past its non-speech part (MEM §8.6), and no account found under the saves folder (§8.2).
inline constexpr std::string_view kMemoryBehind = "Memory is behind; recent events are not being saved.";
inline constexpr std::string_view kNoAccount = "Memory is off for this session: no Elden Ring save folder was found.";
// "Memory is off for this session: " and the cause's words.
[[nodiscard]] std::string memory_off_text(proto::db::Fault cause);

class StorePath {
 public:
  enum class Mode : std::uint8_t {
    Off,      // no store: none attached, memory off, or no playthrough
    Opening,  // Open or Resolve sent, or the facts being refolded: writes wait
    Choice,   // the timeline asks (MEM §9.1): writes wait, named chat is refused
    Open,     // writes flow
    Down,     // the open failed or the store tripped for this file: RAM for the session
  };
  static constexpr Mono kCoalesceMs = 250;
  static constexpr std::size_t kHeldCap = 4096;
  static constexpr int kBusyRetries = 3;
  static constexpr Mono kBusyRetryMs = 1000;

  // The account the shell found (MEM §8.2): from then on a playthrough opens its file. Never logged.
  void attach(std::uint64_t steamid64) noexcept { steamid_ = steamid64; }
  // features.memory (behaviour.hpp): off closes an open file and opens none.
  void set_enabled(bool on);
  [[nodiscard]] bool available() const noexcept { return steamid_ != 0 && enabled_; }
  [[nodiscard]] std::uint64_t account() const noexcept { return steamid_; }

  // A playthrough starts: its file is opened (the previous one closed first).
  void open(const proto::PlaythroughKey& key, proto::Pt pt_now, std::uint16_t journey, Mono now);
  // The playthrough ends (the title, another character): what is queued goes out, then Close. Held writes of an
  // unanswered choice are dropped: the next session asks again.
  void close();
  // The player's answer to the choice.
  void resolve(proto::db::Choice choice);

  // What a result means for the core: the world may go on (the open answered, whichever way), and what to toast.
  struct Effect {
    bool ready = false;
    std::vector<StoreNotice> notices;
    std::optional<proto::db::JobInputResult> job_input;  // the memory job's evidence (MEM §3.3)
    std::optional<proto::db::LivedInputResult> lived_input;  // the lived job's (MEM §13.3)
    std::string lived_persona;
  };
  Effect on_result(const proto::DbResult& result);
  // Time passing: a Busy open's retry.
  void tick(Mono now);

  // The step's events after the bus, the P ones written with their audiences frozen, speech left to commit(); the
  // content and the knowledge's view name personas and stages.
  void write(const std::vector<augur::Event>& events, const Directory& cast, const KnowledgeView* known, Mono now);
  // One commit of the conversation, in order, with the event the core made of it (its audience resolved; none for a
  // start or a page) and the play time of the step: a conversation's row, a line, a later page, an end.
  void commit(const Commit& c, const augur::Event* ev, const Directory& cast, const KnowledgeView* known, proto::Pt pt);
  // Moves what is due into `out`: everything when speech waits or 250 ms have passed since the last flush.
  void pump(Mono now, std::vector<proto::DbRequest>& out);

  [[nodiscard]] Mode mode() const noexcept { return mode_; }
  [[nodiscard]] bool waiting_open() const noexcept { return waiting_open_; }
  [[nodiscard]] const std::optional<proto::db::RollbackSummary>& choice() const noexcept { return choice_; }
  [[nodiscard]] const std::map<std::string, proto::PersonaFacts>& facts() const noexcept { return facts_; }
  [[nodiscard]] std::uint16_t journey() const noexcept { return journey_; }
  [[nodiscard]] std::size_t held() const noexcept { return held_.size(); }
  [[nodiscard]] std::uint64_t dropped() const noexcept { return dropped_; }
  // Each persona's bundle (MEM §4.1): asked for at PersonaApproached (the prefetch) and when a conversation starts,
  // and again when one ends, so its tail is the newest; null until the store answers.
  void want_bundle(const std::string& persona);
  [[nodiscard]] const MemoryBundle* bundle(std::string_view persona) const;

  // The memory jobs (MEM §3.1, §3.2): this journey's open jobs as the store last answered them. A conversation's end
  // with at least 2 lines of the Tarnished's and 4 committed queues one, or merges into the persona's open one
  // (attempts reset); a running job takes the merge after it ends. Each queued job keeps the news new at its last
  // conversation (`known` items absent from world_seen before it), for the job's input.
  [[nodiscard]] std::vector<proto::MemoryJob> runnable() const;  // queued, oldest first
  void want_job_input(proto::RowId job);
  void save_job(const proto::MemoryJob& job);
  // A journal version, the job done and the verified facts, in one transaction; the persona's bundle again after it.
  void commit_journal(proto::db::CommitJournal c);
  [[nodiscard]] std::vector<std::pair<std::string, std::string>> job_news(std::string_view persona) const;
  // The lived layer (MEM §13): its job's input, and a new version in one transaction, the bundle again after.
  void want_lived_input(const std::string& persona);
  void commit_lived(proto::db::CommitLived c);
  // The file's row for a conversation of this session, once written.
  [[nodiscard]] std::optional<proto::RowId> conversation_row(proto::ConvId conv) const;

 private:
  // A write, made when it happens and given its ids when it goes out.
  struct Op {
    enum class Kind : std::uint8_t { Event, ConvOpen, ConvClose, Page, Job };
    Kind kind = Kind::Event;
    proto::StoredEvent event;    // Event: id 0 until it goes out
    proto::ConversationRow row;  // ConvOpen, ConvClose: id 0 until it goes out
    proto::ConvId conv = 0;      // the session's conversation, mapped to its row
    proto::ReplyId reply = 0;    // an NpcSaid and its pages
    std::string text;            // Page
    proto::Pt pt = 0;            // Page, Job
    bool speech = false;
    std::string persona;  // Job
  };
  [[nodiscard]] proto::StoredEvent stored(const augur::Event& ev, const Directory& cast,
                                          const KnowledgeView* known) const;
  void add(Op op);
  void emit(Op& op);
  void lift();
  void send(proto::DbRequest::Op op, bool speech);
  void fail(proto::db::Fault cause, Effect& e);

  std::uint64_t steamid_ = 0;
  bool enabled_ = true;
  Mode mode_ = Mode::Off;
  bool waiting_open_ = false;  // the lifecycle waits for this open's answer
  std::optional<proto::PlaythroughKey> key_;
  proto::Pt open_pt_ = 0;
  std::uint16_t open_journey_ = 1;
  std::uint64_t seq_ = 0;
  std::uint64_t open_seq_ = 0;  // the Open or Resolve in flight
  std::uint64_t fold_seq_ = 0;  // the EventsForFold in flight
  int busy_tries_ = 0;
  std::optional<Mono> retry_at_;
  Mono now_ = 0;
  bool facts_stale_ = false;
  proto::EventId next_event_ = 1;
  proto::RowId next_conv_ = 1;
  std::uint16_t journey_ = 1;
  std::optional<proto::db::RollbackSummary> choice_;
  std::map<std::string, proto::PersonaFacts> facts_;  // the journey's, by profile id
  std::map<proto::ConvId, proto::RowId> convs_;
  std::map<std::pair<proto::ConvId, proto::ReplyId>, proto::EventId> replies_;  // each NpcSaid, for its pages
  std::map<std::string, MemoryBundle, std::less<>> bundles_;
  std::map<std::uint64_t, std::string> bundle_seqs_;           // the Bundles in flight, by sequence number
  std::map<std::string, proto::MemoryJob, std::less<>> jobs_;  // this journey's open jobs, by persona
  std::map<std::uint64_t, std::string> job_seqs_;              // the UpsertJobs in flight
  std::uint64_t input_seq_ = 0;                                // the JobInput in flight
  std::uint64_t lived_seq_ = 0;                                // the LivedInput in flight
  std::string lived_persona_;
  std::map<std::string, std::vector<std::pair<std::string, std::string>>, std::less<>> news_;  // item, certainty
  std::map<std::string, proto::EventId, std::less<>> merge_after_;  // a running job's persona: the newer upto
  std::vector<Op> held_;
  std::vector<proto::DbRequest> queued_;
  bool urgent_ = false;  // speech is queued
  Mono last_flush_ = 0;
  std::uint64_t dropped_ = 0;
};

}  // namespace en::core
