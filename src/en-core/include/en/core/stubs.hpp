#pragma once
// Trivial stubs of speech.hpp's and prompt.hpp's interfaces, for tests: the conversation's (part B's) until part A's
// parts land, and anyone's after that wants a reply's path without the real rules. Each does the least its interface
// allows, deterministically, and says so; none sanitizes, lints or wraps anything for real. Header-only, with no
// static object (ARCH §3.1). Part 0 wrote them; each interface's owner keeps its stub working for the other side.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/core/conversation.hpp"
#include "en/core/prompt.hpp"
#include "en/core/speech.hpp"

namespace en::core::stubs {

// Sentences end at '.', '!' or '?' (a run of them) before whitespace or the stream's end; each is trimmed of spaces
// and otherwise passed as it came. Nothing is removed, there is never a trailer, and the flow is always More.
class StubSanitizer final : public Sanitizer {
 public:
  [[nodiscard]] std::unique_ptr<SentenceStream> open(const SanitizeContext&) const override {
    return std::make_unique<Stream>();
  }

 private:
  class Stream final : public SentenceStream {
   public:
    Flow feed(std::string_view delta, std::vector<Sentence>& out) override {
      held_.append(delta);
      for (std::size_t i = 0; i < held_.size(); ++i) {
        if (!is_end(held_[i])) continue;
        std::size_t j = i;
        while (j < held_.size() && is_end(held_[j])) ++j;
        if (j == held_.size()) break;  // perhaps more of the run, or the stream's end, to come
        if (!is_space(held_[j])) {
          i = j - 1;
          continue;
        }
        emit(held_.substr(0, j), out);
        held_.erase(0, j);
        i = static_cast<std::size_t>(-1);  // from the start of what is left
      }
      return Flow::More;
    }
    void withdraw() override { held_.clear(); }
    void finish(std::vector<Sentence>& out) override {
      emit(held_, out);
      held_.clear();
    }
    [[nodiscard]] std::optional<std::string> intent() const override { return std::nullopt; }

   private:
    static bool is_end(char c) { return c == '.' || c == '!' || c == '?'; }
    static bool is_space(char c) { return c == ' ' || c == '\n' || c == '\t' || c == '\r'; }
    void emit(std::string_view text, std::vector<Sentence>& out) {
      while (!text.empty() && is_space(text.front())) text.remove_prefix(1);
      while (!text.empty() && is_space(text.back())) text.remove_suffix(1);
      if (text.empty()) return;
      out.push_back(Sentence{std::string(text), next_++, {}});
    }

    std::string held_;
    std::uint8_t next_ = 0;
  };
};

// Every sentence passes, except one whose text holds `marker`: a hard hit on the banned list, with NPC §8.4's actions
// (Regenerate in the first sentence of a first try, Deflect in the first sentence of the regeneration, Stop in a later
// sentence). No soft hits; the tally passes through unchanged.
class StubLint final : public Lint {
 public:
  explicit StubLint(std::string marker = "BANNED") : marker_(std::move(marker)) {}

  [[nodiscard]] std::unique_ptr<ReplyLint> open(const LintContext& context) const override {
    return std::make_unique<Reply>(marker_, context.regeneration, context.tally);
  }

 private:
  class Reply final : public ReplyLint {
   public:
    Reply(std::string marker, bool regeneration, LintTally tally)
        : marker_(std::move(marker)), regeneration_(regeneration), tally_(tally) {}
    LintVerdict check(Sentence& sentence) override {
      LintVerdict v;
      if (marker_.empty() || sentence.text.find(marker_) == std::string::npos) return v;
      v.hit = HardList::Banned;
      if (sentence.index > 0) {
        v.action = LintAction::Stop;
      } else {
        v.action = regeneration_ ? LintAction::Deflect : LintAction::Regenerate;
      }
      return v;
    }
    [[nodiscard]] LintTally tally() const override { return tally_; }

   private:
    std::string marker_;
    bool regeneration_;
    LintTally tally_;
  };

  std::string marker_;
};

// One page per sentence, its text as it came, held 2,000 ms (the hold's floor); at most three sentences, the third
// filling the reply. A page is the reply's last once it has ended and nothing is left; there is no fill wait.
class StubPager final : public Pager {
 public:
  static constexpr std::uint32_t kHoldMs = 2000;
  static constexpr std::size_t kMaxSentences = 3;

  [[nodiscard]] std::unique_ptr<ReplyPager> open(const PageContext&) const override { return std::make_unique<Reply>(); }

 private:
  class Reply final : public ReplyPager {
   public:
    Take push(Sentence sentence) override {
      if (taken_ >= kMaxSentences) return Take::Over;
      waiting_.push_back(std::move(sentence.text));
      return ++taken_ == kMaxSentences ? Take::Full : Take::Kept;
    }
    void end() override { ended_ = true; }
    Sealing next(Mono) override {
      Sealing s;
      if (waiting_.empty()) {
        s.kind = ended_ ? Sealing::Kind::Done : Sealing::Kind::Wait;
        return s;
      }
      s.kind = Sealing::Kind::Page;
      s.page.text = std::move(waiting_.front());
      s.page.hold_ms = kHoldMs;
      waiting_.erase(waiting_.begin());
      s.page.last = ended_ && waiting_.empty();
      return s;
    }

   private:
    std::vector<std::string> waiting_;
    std::size_t taken_ = 0;
    bool ended_ = false;
  };
};

// A system message (cache-marked) and a user message holding the recap's lines, one per line; or the refusal it was
// given. The estimate is NPC §7.1's formula on the bytes.
class StubPrompt final : public PromptRenderer {
 public:
  std::optional<PromptRefusal> refuse;  // set it to make every render fail so

  [[nodiscard]] proto::Expected<RenderedPrompt, PromptError> render(const DialogueContext& context) const override {
    if (refuse) return proto::Unexpected{PromptError{*refuse, "the stub was told to refuse"}};
    std::string recap;
    for (const std::string& line : context.recap) {
      if (!recap.empty()) recap += '\n';
      recap += "The Tarnished: \"" + line + "\"";
    }
    RenderedPrompt p;
    p.messages = {proto::Message::system("An invented frame for the stub.", true), proto::Message::user(recap)};
    std::size_t bytes = 0;
    for (const proto::Message& m : p.messages) bytes += m.joined().size();
    p.tokens = static_cast<std::uint32_t>((bytes * 115 + 359) / 360);  // ceil(bytes / 3.6 × 1.15)
    return p;
  }
};

// The four, for a conversation's tests: `parts()` hands them over by reference.
struct StubSpeech {
  StubSanitizer sanitizer;
  StubLint lint;
  StubPager pager;
  StubPrompt prompt;

  [[nodiscard]] SpeechParts parts() const { return SpeechParts{sanitizer, lint, pager, prompt}; }
};

}  // namespace en::core::stubs
