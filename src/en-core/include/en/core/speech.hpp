#pragma once
// Speech, part A's (NPC §8): the sanitizer, the lint and the pager, as the conversation (conversation.hpp, part B's)
// runs them on each reply, in NPC §8.2's order: the stream's deltas go through the sanitizer into sentences (steps 0
// to 7), each sentence through the lint (steps 8 and 9) and into the pager (steps 10 and 11), which seals pages when
// the conversation needs one.
//
// Each of the three is a factory, a const object the conversation holds by reference, that opens one object per reply
// holding that reply's state; nothing is kept between replies but what the conversation carries (LintTally). Part A
// implements them in src/speech/; stubs.hpp has trivial stubs for the conversation's tests. Pure, as all of en-core
// (ARCH §2.3): no I/O, no clock (the time is the conversation's `now`), no static object with a dynamic initialiser
// (ARCH §3.1). The removed text of the stream is never kept, shown or logged: only what was removed and why.
//
// A context struct may gain fields, each with a default that changes nothing, without breaking its users; a change to
// a function's signature or to an enum's meaning is agreed between A and B first (src/en-core/README.md).
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/gate.hpp"  // Mono

namespace en::core {

// ---- The sanitizer (NPC §8.2, steps 0 to 7) ----

// Why a step took text out. Counted per sentence for the log, the eval and the debug view.
enum class Removed : std::uint8_t {
  OverLimit,   // 0: the raw stream past 4 KB
  Reasoning,   // 1: <think>, <thinking>, <reasoning> (any case) and what they hold; an unterminated one to the end
  Trailer,     // 2: an ACT: or ACTION: line and the rest of the stream, captured as the intent
  RunOn,       // 3: "The Tarnished:" or "Tarnished:" at a line start, and the rest of the stream
  Markup,      // 4: tags, [...] spans, (...) spans of at most 60 chars, *...*, _..._, markdown markers, link targets
  Emoji,       // 4: Extended_Pictographic, variation selectors, ZWJ
  Control,     // 4: C0 and C1 controls (a newline becomes a space, and that counts nothing)
  NamePrefix,  // 5: a leading "Name:"
  Quotes,      // 5: quotation marks around a whole sentence or reply
  Character,   // 6: a character outside the whitelist
  LongToken,   // 6: a token of more than 24 chars
};
inline constexpr std::size_t kRemovedKinds = 11;

struct Removal {
  Removed what = Removed::Markup;
  std::uint32_t bytes = 0;  // of the raw stream

  friend bool operator==(const Removal&, const Removal&) = default;
};

struct Sentence {
  // The sanitized sentence: the whitelist's characters only, "…" as "...", spaces collapsed, none at either end.
  std::string text;
  std::uint8_t index = 0;        // its place among the reply's sentences, from 0
  std::vector<Removal> removed;  // what steps 1 to 6 took out of the raw text it came from, by reason

  friend bool operator==(const Sentence&, const Sentence&) = default;
};

// Whether more sentences may come from the stream, and whether the rest of it is wanted.
enum class Flow : std::uint8_t {
  More,        // keep streaming
  IntentOnly,  // a trailer began (step 2): no sentence follows; read the rest of the stream, which is the intent
  Stop,        // no sentence follows and nothing more is wanted (a run-on, the 4 KB cap): cancel the stream
};

struct SanitizeContext {
  // The speaker's names, for step 5's leading "Name:" besides the general pattern. Copied at open().
  std::span<const std::string> names;
};

// One reply's sanitizer. feed() and finish() append the sentences that completed, in order.
class SentenceStream {
 public:
  virtual ~SentenceStream() = default;
  // The next delta of the reply's stream, as en-llm passed it on (LlmEvent Delta).
  virtual Flow feed(std::string_view delta, std::vector<Sentence>& out) = 0;
  // LlmEvent Reset: the text passed on so far was reasoning after all. Drops what is held; sentences already given out
  // stay given (the conversation decides what that means for pages it has shown).
  virtual void withdraw() = 0;
  // The stream ended, or was cancelled: the open fragment, if any, as the last sentence.
  virtual void finish(std::vector<Sentence>& out) = 0;
  // The trailer's text (step 2): the speaker's alone, logged and never shown (SN07 §11.4-A); nothing without one.
  [[nodiscard]] virtual std::optional<std::string> intent() const = 0;
};

class Sanitizer {
 public:
  virtual ~Sanitizer() = default;
  [[nodiscard]] virtual std::unique_ptr<SentenceStream> open(const SanitizeContext& context) const = 0;
};

// ---- The lint (NPC §8.3, §8.4) ----

// The hard lists: a hit blocks the sentence. Each has its reminder for the regeneration (block 11's note, §8.4).
enum class HardList : std::uint8_t {
  Banned,   // banned modern and game words (§3.3)
  Promise,  // the high-confidence promise patterns
  Safety,   // the safety list at both levels; at gentle, graphic gore too (O107: kept hashed)
};

// The soft list (§8.3): fixed or logged, and counted by the eval.
enum class SoftHit : std::uint8_t {
  Opener,          // an extra or off-list opener: stripped
  RepeatedOpener,  // the previous reply's opener: stripped
  Duplicate,       // a sentence the reply already holds: dropped
  TicOverCap,      // logged; reminder in block 11
  NameTwice,       // the told name used a second time: logged; reminder in block 11
  WiderPromise,    // the wider promise and claim list: logged
  UnknownNoun,     // a proper noun no lexicon, name, place or KB entry of this turn holds: logged
  Archaic,         // archaic forms in a register that has none: logged
  ThirdPerson,     // third-person self-reference: logged
};

enum class LintAction : std::uint8_t {
  Pass,        // show the sentence, as the soft lint fixed it
  Drop,        // leave it out and go on (a duplicate)
  Regenerate,  // a hard hit in the first sentence of a first try: cancel, resend with `hit`'s reminder (§8.4)
  Deflect,     // a hard hit in the first sentence of the regeneration: the persona's deflection instead
  Stop,        // a hard hit in a later sentence: end the reply at the previous sentence, in character
};

struct LintVerdict {
  LintAction action = LintAction::Pass;
  std::optional<HardList> hit;  // the hard list that hit: set for Regenerate, Deflect and Stop
  std::vector<SoftHit> soft;    // the sentence's soft hits

  friend bool operator==(const LintVerdict&, const LintVerdict&) = default;
};

enum class Safety : std::uint8_t { Faithful, Gentle };  // `safety.level` (ARCH §11.5, O17)

// What the conversation keeps between replies for the soft lint: the next reply's context carries the last reply's.
struct LintTally {
  std::optional<std::uint8_t> opener;    // the opener the reply used, by its index in the persona's openers
  std::array<std::uint8_t, 3> tics{};    // one count per tic in this conversation (at most three tics, §3.1)
  std::uint8_t name_uses = 0;            // the told name's uses in this conversation

  friend bool operator==(const LintTally&, const LintTally&) = default;
};

struct LintContext {
  bool regeneration = false;              // this reply is the regeneration (§8.4)
  Safety safety = Safety::Faithful;
  bool archaic = false;                   // the persona's register has archaic forms
  std::span<const std::string> openers;   // voice.openers
  std::span<const std::string> tics;      // voice.tics
  std::span<const std::string> names;     // proper nouns this turn shows: lexicons, runtime names, places, the KB
  std::string_view told_name;             // the Tarnished's name, once told (MEM §2.2); empty until then
  LintTally tally;                        // the last reply's (LintTally{} for a conversation's first)
  // Part A's additions, each defaulting to what changes nothing:
  std::span<const std::string> self_names;  // the persona's own names, for third-person self-reference (none: unchecked)
  std::span<const std::uint8_t> tic_max;    // each tic's max_per_conversation, in `tics`' order (missing: 1)
};

// One reply's lint: its sentences in order, each checked once.
class ReplyLint {
 public:
  virtual ~ReplyLint() = default;
  // May fix the sentence in place (a stripped opener).
  virtual LintVerdict check(Sentence& sentence) = 0;
  // After the reply: what the next reply's context carries.
  [[nodiscard]] virtual LintTally tally() const = 0;
};

class Lint {
 public:
  virtual ~Lint() = default;
  // Copies what it needs from the context.
  [[nodiscard]] virtual std::unique_ptr<ReplyLint> open(const LintContext& context) const = 0;
};

// ---- The pager (NPC §8.5, §8.6) ----

struct PageContext {
  std::uint16_t words_max = 0;  // the persona's reply_words_max (block 5, §8.1)
};

struct Page {
  std::string text;           // at most two wrapped lines of 58 chars, and 180 bytes
  std::uint32_t hold_ms = 0;  // clamp(1200 + 55 × chars, 2000, 6500), plus 400 when it ends in "..."
  bool last = false;          // the reply has ended and nothing is left: this is its last page

  friend bool operator==(const Page&, const Page&) = default;
};

// What a sentence did to the reply (§8.5's caps).
enum class Take : std::uint8_t {
  Kept,  // in the reply
  Full,  // in the reply, which is now full (the word cap, three sentences or 600 bytes): cancel the stream
  Over,  // the reply was full already: left out
};

struct Sealing {
  enum class Kind : std::uint8_t {
    Page,  // `page` is sealed
    Wait,  // nothing yet: ask again when a sentence arrives, the stream ends, or at `until` if it is set
    Done,  // the reply has ended and every page is out
  };
  Kind kind = Kind::Wait;
  Page page;
  std::optional<Mono> until;  // Wait: page 1's fill wait (250 ms from when it was first needed)

  friend bool operator==(const Sealing&, const Sealing&) = default;
};

// One reply's pages, sealed lazily: when the conversation needs one (page 1 once its first sentence is complete, page
// k+1 when page k's hold ends), it asks for it.
class ReplyPager {
 public:
  virtual ~ReplyPager() = default;
  // The next sentence the lint passed.
  virtual Take push(Sentence sentence) = 0;
  // No sentence follows: the stream ended, was cancelled, or the sanitizer said so.
  virtual void end() = 0;
  // The next page, which the conversation needs now.
  virtual Sealing next(Mono now) = 0;
};

class Pager {
 public:
  virtual ~Pager() = default;
  [[nodiscard]] virtual std::unique_ptr<ReplyPager> open(const PageContext& context) const = 0;
};

}  // namespace en::core
