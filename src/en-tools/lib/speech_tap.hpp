#pragma once
// The eval's speech tap (NPC §8.2 to §8.6, §12.1, §12.2; WHY§410): what the speech did to each reply, counted where
// the conversation runs it, with en-core unchanged. The eval hands the core its speech parts (Core(SpeechParts)): part
// A's sanitizer, the lint and the pager over the content's lexicon, and the prompt, each wrapped so that every reply
// attempt the conversation opens is recorded with its context (the sanitizer's names, the lint's context, the pager's
// word cap) and its counts:
//   - the sanitizer's removals by step (Removed), one per sentence that lost text to a step;
//   - the "Name:" prefix (step 5, the reply's first sentence), classed as one of the speaker's names, a general prefix
//     of at most two words (which a narrower rule would still take), or a longer one (which it would keep, WHY§410);
//   - the lint's soft hits by kind and hard hits by list;
//   - the archaic sentences, whatever the register allows: a shadow lint with `archaic = false` beside the real one
//     (the `archaic-regal` share, NPC §12.2);
//   - the pager's Take::Full and Take::Over.
// run_speech() runs one reply through the same three parts on its own, as the conversation's take() does, with an
// attempt's recorded context: what the X1 import uses for each reply a model gave outside the game (eval_x1.hpp).
//
// Model text the tap holds (the raw stream, the prefix) is for the local-only reports alone.
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/conversation.hpp"  // SpeechParts
#include "en/core/dialogue_prompt.hpp"
#include "en/core/speech.hpp"
#include "en/core/speech_lint.hpp"
#include "en/core/speech_pager.hpp"
#include "en/core/speech_sanitizer.hpp"
#include "en/proto/expected.hpp"

namespace en::tools {

inline constexpr std::size_t kSoftKinds = 9;  // core::SoftHit, Opener to ThirdPerson
inline constexpr std::size_t kHardKinds = 3;  // core::HardList, Banned to Safety

// Names for the reports: "over_limit", "name_prefix", ...; "opener", "tic_over_cap", ...; "banned", "promise", ...
[[nodiscard]] const char* removed_name(std::size_t kind) noexcept;
[[nodiscard]] const char* soft_name(std::size_t kind) noexcept;
[[nodiscard]] const char* hard_name(std::size_t kind) noexcept;

enum class NameClass : std::uint8_t { Name, Short, Long };
[[nodiscard]] const char* to_string(NameClass c) noexcept;  // "name", "short", "long"

// The class of a "Name:" prefix that step 5 took: one of the speaker's names (any case), else by its words.
[[nodiscard]] NameClass name_class(std::string_view prefix, std::span<const std::string> names);

// One reply's context, owned (en-core's contexts are views): the sanitizer's names, the lint's context, the word cap.
struct ReplyContext {
  std::vector<std::string> names;  // SanitizeContext::names
  bool regeneration = false;
  core::Safety safety = core::Safety::Faithful;
  bool archaic = false;
  std::vector<std::string> openers, tics, lint_names, self_names;
  std::vector<std::uint8_t> tic_max;
  std::string told_name;
  core::LintTally tally;
  std::uint16_t words_max = 0;  // PageContext::words_max

  [[nodiscard]] core::LintContext lint_view() const;
  friend bool operator==(const ReplyContext&, const ReplyContext&) = default;
};

// What the speech did to one reply attempt.
struct SpeechCounts {
  std::array<std::uint32_t, core::kRemovedKinds> removed{};  // sentences that lost text to each step
  std::array<std::uint32_t, kSoftKinds> soft{};
  std::array<std::uint32_t, kHardKinds> hard{};
  std::uint32_t sentences = 0;          // sentences the lint checked
  std::uint32_t archaic_sentences = 0;  // of them, with an archaic form, whatever the register allows
  std::uint32_t full = 0, over = 0;     // the pager's Take::Full and Take::Over
  std::uint32_t trailers = 0;           // a trailer began (step 2: Flow::IntentOnly); its text counts with no sentence
  std::uint32_t stream_stops = 0;       // a run-on or the 4 KB cap ended the stream (steps 3 and 0: Flow::Stop)
  std::optional<NameClass> name_class;  // set when step 5 took a "Name:"
  std::string name_prefix;              // that prefix, without its colon: model text, for the local reports only

  [[nodiscard]] std::uint32_t name_prefixes() const noexcept {
    return removed[static_cast<std::size_t>(core::Removed::NamePrefix)];
  }
  void add(const SpeechCounts& o);  // sums the numbers; keeps this one's prefix
  friend bool operator==(const SpeechCounts&, const SpeechCounts&) = default;
};

// One attempt the conversation opened (a reply, its regeneration, or a reply started again after a Reset).
struct TapAttempt {
  ReplyContext context;
  SpeechCounts counts;
  std::string raw;  // the stream as it came, withdrawn text dropped: model text
};

class SpeechTap {
 public:
  SpeechTap(std::shared_ptr<const core::SpeechLint> lint, std::shared_ptr<const core::DialoguePrompt> prompt);
  ~SpeechTap();
  SpeechTap(const SpeechTap&) = delete;
  SpeechTap& operator=(const SpeechTap&) = delete;

  // The wrapped parts, for Core(SpeechParts); they live as long as the tap.
  [[nodiscard]] core::SpeechParts parts() const noexcept;
  // Every attempt opened so far, in order.
  [[nodiscard]] const std::deque<TapAttempt>& attempts() const noexcept;

  // The plain parts, for run_speech.
  [[nodiscard]] const core::Sanitizer& sanitizer() const noexcept;
  [[nodiscard]] const core::Lint& lint() const noexcept;
  [[nodiscard]] const core::Pager& pager() const noexcept;

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
};

// One reply run through the speech on its own, as the conversation's take() runs it on a first try: each sentence the
// sanitizer gives (the stream fed whole, then finished) through the lint and, if it passes, into the pager. Every
// sentence is counted, those after a hard hit or the pager's cap included, since the measures are of the model's own
// reply; `shown` and `stopped` say what the conversation would have shown and why it stopped.
struct SpeechPass {
  SpeechCounts counts;
  std::vector<std::string> shown;         // the sentences that would show, in order, as the lint fixed them
  std::optional<core::LintAction> stopped;  // Regenerate (a hard hit in the first sentence), Stop (a later one)
  std::optional<core::HardList> hit;      // the list that stopped it
  std::uint32_t model_words = 0;          // the whole reply's words, every sentence the sanitizer gave
  std::uint32_t model_sentences = 0;
  bool cut_by_pager = false;              // the pager's cap came with sentences still after it, or Over
};
[[nodiscard]] SpeechPass run_speech(const SpeechTap& tap, const ReplyContext& context, std::string_view reply);

// A tap over the built-in content's lint and prompt: what the X1 import reads replies with.
[[nodiscard]] proto::Expected<std::unique_ptr<SpeechTap>, std::string> builtin_speech_tap();

// Words as the eval counts them: runs of characters between spaces, tabs and line breaks.
[[nodiscard]] std::size_t words_of(std::string_view s) noexcept;

}  // namespace en::tools
