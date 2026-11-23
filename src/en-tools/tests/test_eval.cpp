// mockllm and the eval (DESIGN §14, ARCH §12.3; WHY§364 (10)): the scripted model's events and timing, its script's
// errors, the recorded model, and `eval --llm mock` running a scripted conversation to a report. All data is invented.
#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "eval_run.hpp"
#include "golden_run.hpp"
#include "mockllm.hpp"

using namespace en;
using namespace en::tools;

namespace {

MockScript script_of(const char* text) {
  auto s = parse_mock_script(text);
  REQUIRE_MESSAGE(s.has_value(), (s.has_value() ? std::string() : s.error()));
  return std::move(*s);
}

std::string all_text(const std::vector<proto::LlmEvent>& events) {
  std::string t;
  for (const proto::LlmEvent& e : events) {
    if (e.kind == proto::EventKind::Delta) t += e.text;
  }
  return t;
}

const std::filesystem::path kTalk = std::filesystem::path(EN_TOOLS_SCENARIOS) / "talk";

}  // namespace

TEST_CASE("mockllm: a reply streams in chunks on the caller's clock, then ends") {
  MockModel m(script_of("schema = 1\n[[reply]]\ntext = \"Hm. The lamp is lit.\"\nfirst_token_ms = 400\nchunk_ms = 50\nchunk_chars = 8\n"));
  m.submit(proto::LlmRequest::chat(7, proto::ChatRequest{}), 1000);
  auto now = m.due(1000);
  REQUIRE(now.size() == 1);
  CHECK(now[0].kind == proto::EventKind::Started);
  CHECK(m.due(1399).empty());
  const auto first = m.due(1400);
  REQUIRE(first.size() == 1);
  CHECK(first[0].text == "Hm. The ");
  CHECK(first[0].id == 7);
  auto rest = m.due(2000);
  REQUIRE_FALSE(rest.empty());
  CHECK(rest.back().kind == proto::EventKind::Ended);
  REQUIRE(rest.back().chat.has_value());
  CHECK(rest.back().chat->ok());
  CHECK("Hm. The " + all_text(rest) == "Hm. The lamp is lit.");
  CHECK(m.served() == 1);
  CHECK_FALSE(m.next_at().has_value());
}

TEST_CASE("mockllm: errors, a think leak with its reset, a trailer, a cancel, and the last reply repeating") {
  MockModel m(script_of(R"toml(schema = 1
[[reply]]
error = "RateLimited"
first_token_ms = 300
[[reply]]
text = "Yes."
think = "plan"
reset = true
trailer = "ACT: nods"
first_token_ms = 10
chunk_ms = 1
chunk_chars = 64
)toml"));
  m.submit(proto::LlmRequest::chat(1, proto::ChatRequest{}), 0);
  auto a = m.due(1000);
  REQUIRE(a.back().kind == proto::EventKind::Ended);
  CHECK(a.back().chat->error == proto::ErrorClass::RateLimited);
  m.submit(proto::LlmRequest::chat(2, proto::ChatRequest{}), 0);
  auto b = m.due(1000);
  CHECK(all_text(b) == "<think>plan</think>Yes.\nACT: nods");
  CHECK(std::any_of(b.begin(), b.end(), [](const proto::LlmEvent& e) { return e.kind == proto::EventKind::Reset; }));
  m.submit(proto::LlmRequest::chat(3, proto::ChatRequest{}), 0);  // the last repeats
  m.cancel(3, 5);
  auto c = m.due(1000);
  REQUIRE(c.size() == 2);  // Started, then the cancel's Ended
  CHECK(c[1].chat->error == proto::ErrorClass::Cancelled);
  CHECK(m.cancelled() == 1);
}

TEST_CASE("mockllm: a script's errors are named by line and key, never quoting a value") {
  CHECK_FALSE(parse_mock_script("schema = 2\n[[reply]]\ntext = \"x\"\n").has_value());
  const auto bad = parse_mock_script("schema = 1\n[[reply]]\ntext = \"x\"\nerror = \"Gremlins\"\n");
  REQUIRE_FALSE(bad.has_value());
  CHECK(bad.error() == "line 4: reply[0].error: must be one of en-llm's error classes");
  const auto unknown = parse_mock_script("schema = 1\n[[reply]]\ntext = \"x\"\nsecret = \"abc\"\n");
  REQUIRE_FALSE(unknown.has_value());
  CHECK(unknown.error() == "line 4: reply[0]: an unknown key");
  CHECK_FALSE(parse_mock_script("schema = 1\n[[reply]]\nfirst_token_ms = 4\n").has_value());  // neither text nor error
}

TEST_CASE("the recorded model replays a recording's events with the live ids") {
  const MockScript s = script_of("schema = 1\n[[reply]]\ntext = \"Hm. Yes.\"\nfirst_token_ms = 100\n");
  std::string recording;
  for (const std::string& line : mock_lines(s, 2)) recording += line + "\n";
  auto events = parse_recorded(recording);
  REQUIRE(events.has_value());
  RecordedModel r(std::move(*events));
  r.submit(proto::LlmRequest::chat(42, proto::ChatRequest{}), 1000);
  const auto out = r.due(5000);
  REQUIRE_FALSE(out.empty());
  for (const proto::LlmEvent& e : out) CHECK(e.id == 42);
  CHECK(all_text(out) == "Hm. Yes.");
  r.submit(proto::LlmRequest::chat(43, proto::ChatRequest{}), 6000);
  r.submit(proto::LlmRequest::chat(44, proto::ChatRequest{}), 6000);  // the recording holds two
  const auto third = r.due(9000);
  const auto none = std::find_if(third.begin(), third.end(), [](const proto::LlmEvent& e) { return e.id == 44; });
  REQUIRE(none != third.end());
  CHECK(none->chat->error == proto::ErrorClass::NoHealthyProvider);
  CHECK_FALSE(parse_recorded("{\"request\": 0, \"after_ms\": 1, \"event\": {}}\n").has_value());
}

TEST_CASE("eval --llm mock and recorded run every scripted conversation to a report, every expectation holding") {
  std::vector<std::filesystem::path> scenarios;
  for (const auto& e : std::filesystem::directory_iterator(kTalk)) {
    const std::string f = e.path().filename().string();
    if (e.path().extension() == ".toml" && f.find(".mock.") == std::string::npos) scenarios.push_back(e.path());
  }
  std::sort(scenarios.begin(), scenarios.end());
  REQUIRE(scenarios.size() >= 4);
  for (const auto& path : scenarios) {
    const std::string name = path.stem().string();
    CAPTURE(name);
    const auto text = read_file(path.string(), 256 * 1024);
    REQUIRE(text.has_value());
    auto scenario = parse_eval_scenario(*text, path.parent_path());
    REQUIRE_MESSAGE(scenario.has_value(), (scenario.has_value() ? std::string() : scenario.error()));
    for (const EvalLlm llm : {EvalLlm::Mock, EvalLlm::Recorded}) {
      if (llm == EvalLlm::Recorded && !scenario->recorded) continue;
      auto run = run_eval(*scenario, llm);
      REQUIRE_MESSAGE(run.has_value(), (run.has_value() ? std::string() : run.error()));
      for (const std::string& f : run->failed) FAIL_CHECK(f);
      const nlohmann::json report = eval_report(*run);
      CHECK(report["kind"] == "eval-local");
      CHECK(report["persona"] == scenario->persona);
      CHECK(report["metrics"]["requests"].get<std::size_t>() >= report["turns"].size());
      // The summary is numbers only: no line of the conversation in it.
      const std::string summary = eval_summary(*run);
      for (const auto& t : report["turns"]) {
        CHECK(summary.find(t["player"].get<std::string>()) == std::string::npos);
        if (!t["reply"].get<std::string>().empty()) CHECK(summary.find(t["reply"].get<std::string>()) == std::string::npos);
      }
    }
  }
}

TEST_CASE("eval: the lint regenerating once, then deflecting; the pager's cap cancelling the stream") {
  const auto run_of = [](const char* name) {
    const auto path = kTalk / (std::string(name) + ".toml");
    auto scenario = parse_eval_scenario(*read_file(path.string(), 256 * 1024), path.parent_path());
    REQUIRE(scenario.has_value());
    auto run = run_eval(*scenario, EvalLlm::Mock);
    REQUIRE(run.has_value());
    return std::move(*run);
  };
  const EvalRun regen = run_of("lantern-regenerate");
  REQUIRE(regen.turns.size() == 2);
  CHECK(regen.turns[0].requests == 2);
  CHECK_FALSE(regen.turns[0].deflection);
  CHECK(regen.turns[0].reply.find("video") == std::string::npos);  // the first try never showed
  CHECK(regen.turns[1].requests == 2);
  CHECK(regen.turns[1].deflection);
  CHECK(regen.requests == 4);
  const nlohmann::json r = eval_report(regen);
  CHECK(r["metrics"]["regenerations"] == 2);
  CHECK(r["metrics"]["deflections"] == 1);

  const EvalRun longer = run_of("lantern-long-reply");
  REQUIRE(longer.turns.size() == 1);
  CHECK(longer.cancels == 1);
  CHECK(longer.turns[0].pages == 2);
  CHECK(longer.turns[0].words == 32);  // past the persona's 30, within the cap and 10 (NPC §8.5)
  CHECK(longer.turns[0].reply.find("travellers") == std::string::npos);  // cut from the stream, never shown
  CHECK_FALSE(longer.turns[0].cut_off);  // the cap fills a reply; it does not cut it off
}

TEST_CASE("eval: the first conversation's turns, as the report holds them") {
  const auto path = kTalk / "lantern-first-words.toml";
  auto scenario = parse_eval_scenario(*read_file(path.string(), 256 * 1024), path.parent_path());
  REQUIRE(scenario.has_value());
  auto run = run_eval(*scenario, EvalLlm::Mock);
  REQUIRE(run.has_value());
  REQUIRE(run->turns.size() == 3);
  CHECK(run->turns[0].player == "Good evening. Where does this road go?");
  CHECK_FALSE(run->turns[0].failure);
  REQUIRE(run->turns[0].first_page_ms.has_value());
  CHECK(*run->turns[0].first_page_ms >= 700);  // the script's first token
  CHECK(run->turns[1].cut_off);
  CHECK(run->turns[1].reply.find("asks for nothing") == std::string::npos);  // never shown, so never in the report
  CHECK(run->endings == std::vector<std::string>{"closed"});
  CHECK(run->cancels >= 1);
}

TEST_CASE("eval: a scenario's errors") {
  CHECK_FALSE(parse_eval_scenario("schema = 1\nname = \"x\"\npersona = \"p\"\n[world]\nparam = 5\n", ".").has_value());  // no steps
  const auto bad = parse_eval_scenario(
      "schema = 1\nname = \"x\"\npersona = \"p\"\n[world]\nparam = 5\n[[step]]\ndo = \"dance\"\n", ".");
  REQUIRE_FALSE(bad.has_value());
  CHECK(bad.error() == "line 6: step[0].do: must be one of open, say, wait, esc, skip, move, expect, flags");
  const auto no_state =
      parse_eval_scenario("schema = 1\nname = \"x\"\npersona = \"p\"\n[world]\nparam = 5\n[[step]]\ndo = \"flags\"\n", ".");
  REQUIRE_FALSE(no_state.has_value());
  CHECK(no_state.error() == "line 6: step[0]: flags needs a state's name");
  const auto not_list = parse_eval_scenario(
      "schema = 1\nname = \"x\"\npersona = \"p\"\n[world]\nparam = 5\n[[step]]\ndo = \"expect\"\nprompt_has = \"a\"\n", ".");
  REQUIRE_FALSE(not_list.has_value());
  CHECK(not_list.error() == "line 6: step[0].prompt_has: must be an array of strings");
  CHECK_FALSE(parse_eval_scenario("schema = 1\nname = \"x\"\npersona = \"p\"\noverrides = \"../up\"\n", ".").has_value());
}

TEST_CASE("eval: the pilot at two states, his prompt grounded by each; a wrong state fails by index, never by text") {
  const auto path = kTalk / "pilot-two-states.toml";
  const auto text = read_file(path.string(), 256 * 1024);
  REQUIRE(text.has_value());
  auto scenario = parse_eval_scenario(*text, path.parent_path());
  REQUIRE(scenario.has_value());
  CHECK(scenario->persona == "kale");
  CHECK(scenario->flags == std::optional<std::string>("fresh"));
  auto run = run_eval(*scenario, EvalLlm::Mock);
  REQUIRE(run.has_value());
  CHECK(run->failed.empty());
  CHECK(run->turns.size() == 2);
  // The world left fresh: the second prompt lacks the news, and the failures name the line and the index alone.
  for (EvalStep& st : scenario->steps) {
    if (st.kind == EvalStep::Kind::Flags) st.flags = "fresh";
  }
  auto stale = run_eval(*scenario, EvalLlm::Mock);
  REQUIRE(stale.has_value());
  REQUIRE(stale->failed.size() == 2);
  CHECK(stale->failed[0].find("the prompt lacks prompt_has[0]") != std::string::npos);
  for (const std::string& f : stale->failed) CHECK(f.find("sky") == std::string::npos);
  // A state that does not exist is the scenario's error.
  for (EvalStep& st : scenario->steps) {
    if (st.kind == EvalStep::Kind::Flags) st.flags = "no_such_state";
  }
  auto missing = run_eval(*scenario, EvalLlm::Mock);
  REQUIRE(missing.has_value());
  CHECK(std::any_of(missing->failed.begin(), missing->failed.end(),
                    [](const std::string& f) { return f.find("no fixture flag state") != std::string::npos; }));
}
