// The interfaces part A and part B build on (speech.hpp, prompt.hpp, conversation.hpp; WHY§364), through the stubs
// (stubs.hpp): each stub does what it says, so B's tests can lean on it, and the conversation's stub never opens. The
// dialogue request carries NPC §7.3's two stop sequences. All lines are invented.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/core/conversation.hpp"
#include "en/core/prompt.hpp"
#include "en/core/speech.hpp"
#include "en/core/stubs.hpp"

using namespace en;
using namespace en::core;
using namespace en::core::stubs;

namespace {

std::vector<std::string> texts(const std::vector<Sentence>& sentences) {
  std::vector<std::string> out;
  for (const Sentence& s : sentences) out.push_back(s.text);
  return out;
}

}  // namespace

TEST_CASE("the stub sanitizer splits a stream at sentence ends, across deltas, and passes the text as it came") {
  const StubSanitizer sanitizer;
  auto stream = sanitizer.open(SanitizeContext{});
  std::vector<Sentence> out;
  CHECK(stream->feed("Down, mostly.", out) == Flow::More);
  CHECK(out.empty());  // the end of a delta is not the stream's: "." may be the start of "..."
  stream->feed(" The lamps", out);
  CHECK(texts(out) == std::vector<std::string>{"Down, mostly."});
  stream->feed(" go with it! Or so", out);
  CHECK(texts(out) == std::vector<std::string>{"Down, mostly.", "The lamps go with it!"});
  stream->feed(" they say...", out);
  stream->finish(out);
  CHECK(texts(out) == std::vector<std::string>{"Down, mostly.", "The lamps go with it!", "Or so they say..."});
  CHECK(out[2].index == 2);
  CHECK(out[2].removed.empty());
  CHECK_FALSE(stream->intent().has_value());

  auto withdrawn = sanitizer.open(SanitizeContext{});
  withdrawn->feed("A plan in the open", out);
  withdrawn->withdraw();
  std::vector<Sentence> none;
  withdrawn->finish(none);
  CHECK(none.empty());
}

TEST_CASE("the stub lint passes all but its marker, with NPC 8.4's three actions for a hard hit") {
  const StubLint lint;
  auto first = lint.open(LintContext{});
  Sentence ok{"Down, mostly.", 0, {}};
  CHECK(first->check(ok) == LintVerdict{});
  Sentence hit0{"A BANNED word.", 0, {}};
  const LintVerdict regen = first->check(hit0);
  CHECK(regen.action == LintAction::Regenerate);
  CHECK(regen.hit == HardList::Banned);
  Sentence hit1{"Then a BANNED one.", 1, {}};
  CHECK(first->check(hit1).action == LintAction::Stop);

  LintContext again;
  again.regeneration = true;
  again.tally.name_uses = 2;
  auto second = lint.open(again);
  CHECK(second->check(hit0).action == LintAction::Deflect);
  CHECK(second->tally() == again.tally);
}

TEST_CASE("the stub pager: a page per sentence, held 2 s, three sentences at most, the last page marked") {
  const StubPager pager;
  auto reply = pager.open(PageContext{40});
  CHECK(reply->next(0).kind == Sealing::Kind::Wait);
  CHECK(reply->push(Sentence{"One.", 0, {}}) == Take::Kept);
  CHECK(reply->push(Sentence{"Two.", 1, {}}) == Take::Kept);
  CHECK(reply->push(Sentence{"Three.", 2, {}}) == Take::Full);
  CHECK(reply->push(Sentence{"Four.", 3, {}}) == Take::Over);
  Sealing s = reply->next(10);
  REQUIRE(s.kind == Sealing::Kind::Page);
  CHECK(s.page == Page{"One.", StubPager::kHoldMs, false});
  reply->end();
  CHECK(reply->next(20).page.text == "Two.");
  s = reply->next(30);
  CHECK(s.page == Page{"Three.", StubPager::kHoldMs, true});
  CHECK(reply->next(40).kind == Sealing::Kind::Done);
}

TEST_CASE("the stub prompt renders the recap with a cache-marked frame, or refuses as told; a failed render is nothing") {
  StubPrompt prompt;
  const std::vector<std::string> recap = {"Where does the road go?", "And then?"};
  DialogueContext context;
  context.persona = "invented_keeper";
  context.recap = recap;
  auto rendered = prompt.render(context);
  REQUIRE(rendered.has_value());
  REQUIRE(rendered->messages.size() == 2);
  CHECK(rendered->messages[0].role == proto::Role::System);
  CHECK(rendered->messages[0].has_cache_break());
  CHECK(rendered->messages[1].joined() == "The Tarnished: \"Where does the road go?\"\nThe Tarnished: \"And then?\"");
  CHECK(rendered->tokens > 0);

  prompt.refuse = PromptRefusal::Template;
  auto refused = prompt.render(context);
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error().why == PromptRefusal::Template);
}

TEST_CASE("a dialogue request: the dialogue slot, a reply, NPC 7.3's two stop sequences, the slot's budget") {
  RenderedPrompt rendered;
  rendered.messages = {proto::Message::system("An invented frame.", true), proto::Message::user("Hello.")};
  const proto::ChatRequest request = dialogue_request(rendered);
  CHECK(request.slot == "dialogue");
  CHECK(request.purpose == proto::Purpose::Reply);
  CHECK(request.messages == rendered.messages);
  CHECK(request.stop == std::vector<std::string>{"\nThe Tarnished:", "\nTarnished:"});
  CHECK_FALSE(request.max_tokens.has_value());
  CHECK_FALSE(request.temperature.has_value());
}

TEST_CASE("the conversation's stub holds the parts by reference and never opens") {
  const StubSpeech speech;
  Conversation conversation(speech.parts());
  CHECK(&conversation.parts().sanitizer == &speech.sanitizer);
  CHECK(&conversation.parts().prompt == &speech.prompt);
  const proto::UiEvent open{proto::UiEvent::OpenChat{proto::OpenVia::TalkKey}};
  ConversationInput in;
  in.now = 1000;
  in.ui = &open;
  const ConversationOutput out = conversation.step(in);
  CHECK(out.ui == proto::UiModel{});
  CHECK(out.requests.empty());
  CHECK(out.cancels.empty());
  CHECK_FALSE(out.ended.has_value());
}
