#pragma once
// The memory job (MEM §3; DESIGN §10.3; v0.3 commits 13 and 14): one job per persona per journey, created or merged at
// a conversation's end (≥ 2 lines of the Tarnished's and ≥ 4 committed: MEM §3.1), run one at a time on the `memory`
// slot when the moment is calm (MEM §3.2), from the store's evidence above the persona's watermark (MEM §3.3), with the
// prompt of prompts/memory/ (MEM §3.4, ours) and MEM §3.5's schema. Its reply is validated and committed by commit 14.
//
// The core never waits on it: the store answers the input, the model client the reply, each as an input of a later
// step. The prompt holds chat text and the journal: never logged or recorded (PV rules).
#include <cstdint>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "en/core/content.hpp"
#include "en/core/gate.hpp"  // Mono
#include "en/core/knowledge.hpp"
#include "en/core/memory.hpp"
#include "en/proto/expected.hpp"
#include "en/proto/llm.hpp"
#include "en/proto/store.hpp"

namespace en::core {

// The memory job's own range of request ids (the conversation's starts at 2^48).
inline constexpr proto::RequestId kMemoryRequests = proto::RequestId{1} << 50;

// One numbered line of the job's evidence (MEM §3.3), with what validation reads of it (MEM §2.5, §3.6).
struct JobLine {
  enum class Kind : std::uint8_t { Talk, Note, Context };
  Kind kind = Kind::Talk;
  std::string id;          // L1…, N1…, C1…
  std::string who;         // Talk: "The Tarnished", or the persona's name
  std::string text;        // as shown
  std::string when;        // Note: its relative time
  bool player = false;     // a line the Tarnished spoke (a PlayerSaid, direct or overheard)
  bool own = false;        // a line the persona itself spoke
  bool overheard = false;  // perceived as overheard
  bool seen = false;       // a non-speech note perceived directly or witnessed
  bool world = false;      // a world C line (block 5b, the since-line, the cue: MEM §12.7)
  bool just_now = false;   // a "just now" context line
  proto::EventId event = 0;
  friend bool operator==(const JobLine&, const JobLine&) = default;
};

struct CheckOffer {
  std::string key;      // slew:<boss>, great_runes>=n, name
  std::string meaning;  // in the Tarnished's own terms (ours)
  friend bool operator==(const CheckOffer&, const CheckOffer&) = default;
};

// The job's input, assembled (MEM §3.3).
struct JobInput {
  std::string persona, name;
  std::string brief, honesty, gist, road, stage_seen;
  std::vector<std::string> new_world;  // "(heard) the fall of …": at most 3
  std::optional<Journal> journal;
  std::vector<JobLine> lines, notes, context;
  std::vector<CheckOffer> checks;
  bool talk_cut = false;  // the oldest talk lines were left out (60 at most)
  proto::EventId upto = 0;
  proto::Pt as_of_pt = 0;  // the newest evidence's
  std::optional<proto::RowId> conv;
};

// MEM §2.6's relative time, by play time (ours): "a moment ago" within 10 min, "a little while ago" within an hour,
// "earlier today" within 6 h, "some time ago" beyond.
[[nodiscard]] std::string when_words(proto::Pt pt, proto::Pt now);

// What assembly reads besides the store's answer: the persona and its stage now, its facts, the news new at the
// conversation that queued the job, the boss aliases' match words (MEM §2.5) and the world's shorts.
struct InputCtx {
  const Persona* persona = nullptr;
  std::size_t stage = 0;
  const proto::PersonaFacts* facts = nullptr;
  std::vector<std::string> new_world;  // already in words, at most 3
  const Knowledge* knowledge = nullptr;
  proto::Pt now = 0;
};
[[nodiscard]] JobInput assemble_input(const proto::db::JobInputResult& r, const InputCtx& ctx);

// The job's messages from prompts/memory/01 to 06 (MEM §3.4), with a retry's reminder line; and MEM §3.5's schema.
[[nodiscard]] proto::Expected<std::vector<proto::Message>, std::string> memory_messages(
    std::span<const ContentFile> files, const JobInput& in, std::string_view reminder = {});
[[nodiscard]] nlohmann::json memory_schema();

// ---- The reply (commit 14; MEM §3.6, §3.7) ----

// Why a reply failed: each is retried twice (MEM §3.7) with its reminder line.
enum class JobFailure : std::uint8_t { None, Empty, Parse, Truncated, Summary, Provider };
[[nodiscard]] const char* to_string(JobFailure f) noexcept;  // "empty", "parse", "truncated", "summary", "provider"
// The reminder appended on the retry after a failure of that class (ours).
[[nodiscard]] std::string_view retry_reminder(JobFailure f) noexcept;

// What validation reads besides the input: the character's name as the save holds it (for told_name's check and the
// name guard), the lexicon's banned words, the catalogue's aliases, and how each check key stands now.
struct ValidateCtx {
  std::string character;                         // PlaythroughReady's name; empty: none known
  std::function<bool(std::string_view)> banned;  // a banned or safety word in the text
  std::vector<std::string> aliases;              // the catalogue's, which never enter a journal (MEM §7.6)
  std::function<bool(std::string_view)> proved;  // a check key holds now (MEM §2.5), in Unlock mode
  proto::Pt now = 0;
};

struct Validated {
  Journal journal;
  std::vector<proto::FactRow> verified;  // verified:<key> facts that hold now
  std::uint32_t dropped = 0;             // sentences and keepsakes left out by hygiene
  std::uint32_t kept_by_world = 0;       // world keepsakes citing only world C lines (MEM §12.7)
  std::uint32_t unknown_keys = 0;
  std::uint32_t next_keepsake = 1;
};

// MEM §3.6's steps 1 to 9 over the model's text: the new journal, or the failure that asks for a retry.
[[nodiscard]] proto::Expected<Validated, JobFailure> validate_reply(std::string_view text, std::string_view finish,
                                                                    const JobInput& in, const ValidateCtx& ctx);

}  // namespace en::core
