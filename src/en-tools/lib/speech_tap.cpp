// The eval's speech tap (speech_tap.hpp; NPC §8.2 to §8.6, §12.2; WHY§410).
#include "speech_tap.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

#include "en/core/core.hpp"  // builtin_content
#include "en/core/speech_content.hpp"

namespace en::tools {

namespace {

constexpr std::array<const char*, core::kRemovedKinds> kRemovedNames = {
    "over_limit", "reasoning", "trailer", "run_on", "markup", "emoji",
    "control",    "name_prefix", "quotes", "character", "long_token"};
constexpr std::array<const char*, kSoftKinds> kSoftNames = {
    "opener", "repeated_opener", "duplicate", "tic_over_cap", "name_twice",
    "wider_promise", "unknown_noun", "archaic", "third_person"};
constexpr std::array<const char*, kHardKinds> kHardNames = {"banned", "promise", "safety"};

std::string lower_ascii(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

// The prefix step 5 took, from the raw stream: the `bytes` bytes of the raw text that end with its first colon, the
// colon left out, trimmed of spaces and of the markup a model may wrap a name in.
std::string prefix_from_raw(std::string_view raw, std::uint32_t bytes) {
  const std::size_t colon = raw.find(':');
  if (colon == std::string_view::npos || bytes == 0) return {};
  const std::size_t want = std::min<std::size_t>(bytes - 1, colon);
  std::string_view p = raw.substr(colon - want, want);
  const auto trim = [](char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '*' || c == '_' || c == '"' || c == '\'' ||
           c == '[' || c == ']' || c == '#' || c == '>' || c == '`';
  };
  while (!p.empty() && trim(p.front())) p.remove_prefix(1);
  while (!p.empty() && trim(p.back())) p.remove_suffix(1);
  return std::string(p.substr(0, std::min<std::size_t>(p.size(), 96)));
}

void count_sentence(const core::Sentence& s, std::span<const std::string> names, std::string_view raw,
                    SpeechCounts& c) {
  for (const core::Removal& r : s.removed) {
    const auto k = static_cast<std::size_t>(r.what);
    if (k < c.removed.size()) ++c.removed[k];
    if (r.what == core::Removed::NamePrefix && !c.name_class) {
      c.name_prefix = prefix_from_raw(raw, r.bytes);
      c.name_class = name_class(c.name_prefix, names);
    }
  }
}

void count_verdict(const core::LintVerdict& v, SpeechCounts& c) {
  ++c.sentences;
  for (const core::SoftHit h : v.soft) {
    const auto k = static_cast<std::size_t>(h);
    if (k < c.soft.size()) ++c.soft[k];
  }
  if (v.hit) {
    const auto k = static_cast<std::size_t>(*v.hit);
    if (k < c.hard.size()) ++c.hard[k];
  }
}

bool archaic_in(const core::LintVerdict& v) {
  return std::find(v.soft.begin(), v.soft.end(), core::SoftHit::Archaic) != v.soft.end();
}

// A reply's lint that also checks a copy of each sentence with a shadow lint whose register has no archaic forms.
class ShadowedLint {
 public:
  ShadowedLint(const core::Lint& lint, const core::LintContext& view) : real_(lint.open(view)) {
    if (view.archaic) {
      core::LintContext shadow = view;
      shadow.archaic = false;
      shadow_ = lint.open(shadow);
    }
  }
  core::LintVerdict check(core::Sentence& s, SpeechCounts& c) {
    core::Sentence copy = s;
    const core::LintVerdict v = real_->check(s);
    count_verdict(v, c);
    if (shadow_) {
      if (archaic_in(shadow_->check(copy))) ++c.archaic_sentences;
    } else if (archaic_in(v)) {
      ++c.archaic_sentences;
    }
    return v;
  }
  [[nodiscard]] core::LintTally tally() const { return real_->tally(); }

 private:
  std::unique_ptr<core::ReplyLint> real_;
  std::unique_ptr<core::ReplyLint> shadow_;
};

void count_flow(core::Flow f, SpeechCounts& c) {
  if (f == core::Flow::IntentOnly) ++c.trailers;
  if (f == core::Flow::Stop) ++c.stream_stops;
}

constexpr std::size_t kRawKeep = 8192;  // of a stream's raw text, for the prefix and the local report

}  // namespace

const char* removed_name(std::size_t kind) noexcept { return kind < kRemovedNames.size() ? kRemovedNames[kind] : "?"; }
const char* soft_name(std::size_t kind) noexcept { return kind < kSoftNames.size() ? kSoftNames[kind] : "?"; }
const char* hard_name(std::size_t kind) noexcept { return kind < kHardNames.size() ? kHardNames[kind] : "?"; }

const char* to_string(NameClass c) noexcept {
  switch (c) {
    case NameClass::Name:
      return "name";
    case NameClass::Short:
      return "short";
    case NameClass::Long:
      return "long";
  }
  return "long";
}

NameClass name_class(std::string_view prefix, std::span<const std::string> names) {
  const std::string p = lower_ascii(prefix);
  for (const std::string& n : names) {
    if (!n.empty() && lower_ascii(n) == p) return NameClass::Name;
  }
  return words_of(prefix) <= 2 ? NameClass::Short : NameClass::Long;
}

std::size_t words_of(std::string_view s) noexcept {
  std::size_t n = 0;
  bool in_word = false;
  for (const char c : s) {
    const bool space = c == ' ' || c == '\t' || c == '\n' || c == '\r';
    if (!space && !in_word) ++n;
    in_word = !space;
  }
  return n;
}

core::LintContext ReplyContext::lint_view() const {
  core::LintContext c;
  c.regeneration = regeneration;
  c.safety = safety;
  c.archaic = archaic;
  c.openers = openers;
  c.tics = tics;
  c.names = lint_names;
  c.told_name = told_name;
  c.tally = tally;
  c.self_names = self_names;
  c.tic_max = tic_max;
  return c;
}

void SpeechCounts::add(const SpeechCounts& o) {
  for (std::size_t i = 0; i < removed.size(); ++i) removed[i] += o.removed[i];
  for (std::size_t i = 0; i < soft.size(); ++i) soft[i] += o.soft[i];
  for (std::size_t i = 0; i < hard.size(); ++i) hard[i] += o.hard[i];
  sentences += o.sentences;
  archaic_sentences += o.archaic_sentences;
  full += o.full;
  over += o.over;
  trailers += o.trailers;
  stream_stops += o.stream_stops;
}

// ---- The wrapped parts ----

struct SpeechTap::Impl {
  std::shared_ptr<const core::SpeechLint> lint;
  std::shared_ptr<const core::DialoguePrompt> prompt;
  core::SpeechSanitizer sanitizer;
  core::SpeechPager pager;
  std::deque<TapAttempt> attempts;  // a deque: the wrappers hold pointers into it

  class Stream final : public core::SentenceStream {
   public:
    Stream(std::unique_ptr<core::SentenceStream> inner, TapAttempt& a) : inner_(std::move(inner)), a_(a) {}
    core::Flow feed(std::string_view delta, std::vector<core::Sentence>& out) override {
      if (a_.raw.size() < kRawKeep) a_.raw.append(delta.substr(0, kRawKeep - a_.raw.size()));
      const std::size_t from = out.size();
      const core::Flow f = inner_->feed(delta, out);
      for (std::size_t i = from; i < out.size(); ++i) count_sentence(out[i], a_.context.names, a_.raw, a_.counts);
      if (f != core::Flow::More && !flowed_) {
        flowed_ = true;
        count_flow(f, a_.counts);
      }
      return f;
    }
    void withdraw() override {
      a_.raw.clear();
      inner_->withdraw();
    }
    void finish(std::vector<core::Sentence>& out) override {
      const std::size_t from = out.size();
      inner_->finish(out);
      for (std::size_t i = from; i < out.size(); ++i) count_sentence(out[i], a_.context.names, a_.raw, a_.counts);
    }
    [[nodiscard]] std::optional<std::string> intent() const override { return inner_->intent(); }

   private:
    std::unique_ptr<core::SentenceStream> inner_;
    TapAttempt& a_;
    bool flowed_ = false;
  };

  class Sanitizer final : public core::Sanitizer {
   public:
    explicit Sanitizer(Impl& tap) : tap_(tap) {}
    [[nodiscard]] std::unique_ptr<core::SentenceStream> open(const core::SanitizeContext& c) const override {
      TapAttempt& a = tap_.attempts.emplace_back();
      a.context.names.assign(c.names.begin(), c.names.end());
      return std::make_unique<Stream>(tap_.sanitizer.open(c), a);
    }

   private:
    Impl& tap_;
  };

  class ReplyLint final : public core::ReplyLint {
   public:
    ReplyLint(const core::Lint& lint, const core::LintContext& c, TapAttempt& a) : a_(a), lint_(lint, c) {}
    core::LintVerdict check(core::Sentence& s) override { return lint_.check(s, a_.counts); }
    [[nodiscard]] core::LintTally tally() const override { return lint_.tally(); }

   private:
    TapAttempt& a_;
    ShadowedLint lint_;
  };

  class Lint final : public core::Lint {
   public:
    explicit Lint(Impl& tap) : tap_(tap) {}
    [[nodiscard]] std::unique_ptr<core::ReplyLint> open(const core::LintContext& c) const override {
      // The conversation opens the sanitizer, then the lint, then the pager, for each attempt (open_parts).
      if (tap_.attempts.empty()) tap_.attempts.emplace_back();
      TapAttempt& a = tap_.attempts.back();
      ReplyContext& r = a.context;
      r.regeneration = c.regeneration;
      r.safety = c.safety;
      r.archaic = c.archaic;
      r.openers.assign(c.openers.begin(), c.openers.end());
      r.tics.assign(c.tics.begin(), c.tics.end());
      r.lint_names.assign(c.names.begin(), c.names.end());
      r.self_names.assign(c.self_names.begin(), c.self_names.end());
      r.tic_max.assign(c.tic_max.begin(), c.tic_max.end());
      r.told_name = std::string(c.told_name);
      r.tally = c.tally;
      return std::make_unique<ReplyLint>(*tap_.lint, c, a);
    }

   private:
    Impl& tap_;
  };

  class ReplyPager final : public core::ReplyPager {
   public:
    ReplyPager(std::unique_ptr<core::ReplyPager> inner, TapAttempt& a) : inner_(std::move(inner)), a_(a) {}
    core::Take push(core::Sentence s) override {
      const core::Take t = inner_->push(std::move(s));
      if (t == core::Take::Full) ++a_.counts.full;
      if (t == core::Take::Over) ++a_.counts.over;
      return t;
    }
    void end() override { inner_->end(); }
    core::Sealing next(core::Mono now) override { return inner_->next(now); }

   private:
    std::unique_ptr<core::ReplyPager> inner_;
    TapAttempt& a_;
  };

  class Pager final : public core::Pager {
   public:
    explicit Pager(Impl& tap) : tap_(tap) {}
    [[nodiscard]] std::unique_ptr<core::ReplyPager> open(const core::PageContext& c) const override {
      if (tap_.attempts.empty()) tap_.attempts.emplace_back();
      TapAttempt& a = tap_.attempts.back();
      a.context.words_max = c.words_max;
      return std::make_unique<ReplyPager>(tap_.pager.open(c), a);
    }

   private:
    Impl& tap_;
  };

  Sanitizer tap_sanitizer{*this};
  Lint tap_lint{*this};
  Pager tap_pager{*this};
};

SpeechTap::SpeechTap(std::shared_ptr<const core::SpeechLint> lint, std::shared_ptr<const core::DialoguePrompt> prompt)
    : impl_(std::make_unique<Impl>()) {
  impl_->lint = std::move(lint);
  impl_->prompt = std::move(prompt);
}

SpeechTap::~SpeechTap() = default;

core::SpeechParts SpeechTap::parts() const noexcept {
  return core::SpeechParts{impl_->tap_sanitizer, impl_->tap_lint, impl_->tap_pager, *impl_->prompt};
}

const std::deque<TapAttempt>& SpeechTap::attempts() const noexcept { return impl_->attempts; }
const core::Sanitizer& SpeechTap::sanitizer() const noexcept { return impl_->sanitizer; }
const core::Lint& SpeechTap::lint() const noexcept { return *impl_->lint; }
const core::Pager& SpeechTap::pager() const noexcept { return impl_->pager; }

// ---- One reply on its own ----

SpeechPass run_speech(const SpeechTap& tap, const ReplyContext& context, std::string_view reply) {
  SpeechPass pass;
  std::vector<core::Sentence> sentences;
  {
    const std::vector<std::string>& names = context.names;
    auto stream = tap.sanitizer().open(core::SanitizeContext{names});
    const std::string raw(reply.substr(0, std::min<std::size_t>(reply.size(), kRawKeep)));
    const core::Flow f = stream->feed(reply, sentences);
    if (f == core::Flow::More) stream->finish(sentences);
    count_flow(f, pass.counts);
    for (const core::Sentence& s : sentences) {
      count_sentence(s, names, raw, pass.counts);
      pass.model_words += static_cast<std::uint32_t>(words_of(s.text));
    }
  }
  pass.model_sentences = static_cast<std::uint32_t>(sentences.size());
  const core::LintContext view = context.lint_view();
  ShadowedLint lint(tap.lint(), view);
  auto pager = tap.pager().open(core::PageContext{context.words_max});
  bool open = true;  // the conversation still takes sentences
  for (std::size_t i = 0; i < sentences.size(); ++i) {
    core::Sentence s = sentences[i];
    const core::LintVerdict v = lint.check(s, pass.counts);
    if (!open) continue;
    switch (v.action) {
      case core::LintAction::Pass: {
        const std::string text = s.text;
        const core::Take t = pager->push(std::move(s));
        if (t == core::Take::Full) ++pass.counts.full;
        if (t == core::Take::Over) ++pass.counts.over;
        if (t != core::Take::Over) pass.shown.push_back(text);
        if (t != core::Take::Kept) {
          open = false;
          pass.cut_by_pager = t == core::Take::Over || i + 1 < sentences.size();
        }
        break;
      }
      case core::LintAction::Drop:
        break;
      case core::LintAction::Regenerate:
      case core::LintAction::Deflect:
      case core::LintAction::Stop:
        pass.stopped = v.action;
        pass.hit = v.hit;
        open = false;
        if (v.action != core::LintAction::Stop) pass.shown.clear();
        break;
    }
  }
  return pass;
}

proto::Expected<std::unique_ptr<SpeechTap>, std::string> builtin_speech_tap() {
  const std::vector<core::ContentFile> files = core::builtin_content();
  auto pack = core::load_content(files);
  if (!pack) return proto::Unexpected{"the built-in content does not load: " + core::describe(pack.error().front())};
  auto speech = core::load_speech(*pack);
  if (!speech) return proto::Unexpected{"the built-in speech does not load: " + core::describe(speech.error().front())};
  return std::make_unique<SpeechTap>(speech->lint, speech->prompt);
}

}  // namespace en::tools
