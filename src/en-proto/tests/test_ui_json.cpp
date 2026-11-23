// The JSON forms of the conversation's surfaces' messages (ui.hpp): a round trip for every type, through JSON values and
// through text, with every field away from its default and every event kind; the version, which must be there and be
// kUiVersion; a field left out keeping its default; and malformed JSON refused with its path, never a value. All names
// and lines are invented.
#include <doctest.h>

#include <cstdint>
#include <string>
#include <utility>

#include "en/proto/json.hpp"
#include "en/proto/ui.hpp"

using namespace en::proto;
using nlohmann::json;

namespace {

// A handle past 2^53, where a reader holding numbers as doubles would lose it.
constexpr std::uint64_t kBigHandle = (std::uint64_t{1} << 60) + 11;

template <class T>
void round_trip(const T& value) {
  const json j = value;
  CHECK(j.get<T>() == value);
  CHECK(read_json<T>(j.dump()) == value);
}

template <class T>
std::string refusal(const json& j) {
  try {
    (void)j.get<T>();
  } catch (const ProtoError& e) {
    return e.what();
  }
  return "";
}

PageView sample_page() {
  PageView p;
  p.id = PageId{3, 2, 1};
  p.kind = PageKind::Speech;
  p.surface = Surface::HistoryStrip;
  p.speaker = "Oswin the Lamplighter";
  p.text = "The road bends here, and the lamps bend with it... caf\xC3\xA9 \xE2\x80\x94 or so they say.";
  p.count = 3;
  p.last = true;
  p.hold_ms = 4930;
  return p;
}

ChatBox sample_box() {
  ChatBox b;
  b.view = BoxView::Strip;
  b.conv = 3;
  b.state = ConvState::Held;
  b.partner = "Oswin the Lamplighter";
  b.heard_by = {"Oswin the Lamplighter", "Merrit of the Ferry"};
  b.history = {HistoryLine{HistoryLine::Who::Tarnished, "", "Where does this road go?", false},
               HistoryLine{HistoryLine::Who::Npc, "Oswin the Lamplighter", "Down, mostly.", false},
               HistoryLine{HistoryLine::Who::Tarnished, "", "And then?", true}};
  b.draft = "And after th";
  b.max_chars = 200;
  b.max_bytes = 600;
  return b;
}

UiModel sample_model() {
  UiModel m;
  m.page = sample_page();
  m.chat = sample_box();
  m.hint = TalkHint{HintVerb::Answer, "Oswin the Lamplighter", ChrHandle{kBigHandle}};
  m.toasts = {Toast{7, ToastLevel::Warning, "No answer: provider timed out"}, Toast{8, ToastLevel::Error, "x"}};
  return m;
}

UiEvent event_of(UiEvent::OpenChat e) { return UiEvent{e}; }
UiEvent event_of(UiEvent::Submit e) { return UiEvent{std::move(e)}; }
UiEvent event_of(UiEvent::PageShown e) { return UiEvent{e}; }
UiEvent event_of(UiEvent::Close e) { return UiEvent{std::move(e)}; }
UiEvent event_of(UiEvent::Skip e) { return UiEvent{e}; }

}  // namespace

TEST_CASE("the model's parts round-trip, every field away from its default") {
  round_trip(PageId{(1u << 31) + 5, 77, 255});
  round_trip(sample_page());
  PageView card;
  card.id = PageId{4, 1, 0};
  card.kind = PageKind::Card;
  card.speaker = "Oswin the Lamplighter";
  round_trip(card);
  PageView failure = card;
  failure.kind = PageKind::Failure;
  failure.text = "...";
  failure.hold_ms = 2000;
  round_trip(failure);
  for (const auto& line : sample_box().history) round_trip(line);
  round_trip(sample_box());
  round_trip(TalkHint{HintVerb::Next, "Merrit of the Ferry", ChrHandle{kBigHandle}});
  round_trip(Toast{1u << 30, ToastLevel::Info, "They have nothing to say to you."});

  // The form: words in snake_case, ids and handles as integers, exact past 2^53.
  const json page = sample_page();
  CHECK(page["id"] == json{{"conv", 3}, {"reply", 2}, {"index", 1}});
  CHECK(page["surface"] == "history_strip");
  CHECK(page["hold_ms"] == 4930);
  const json hint = TalkHint{HintVerb::Speak, "Oswin", ChrHandle{kBigHandle}};
  CHECK(hint["target"].get<std::uint64_t>() == kBigHandle);
  CHECK(hint.dump().find(std::to_string(kBigHandle)) != std::string::npos);
  CHECK(json(sample_box())["view"] == "strip");
  CHECK(json(sample_box())["state"] == "held");
  CHECK(json(sample_box())["history"][2]["waiting"] == true);
}

TEST_CASE("a whole model round-trips with its version; an empty one is the default") {
  round_trip(sample_model());
  round_trip(UiModel{});
  const json m = sample_model();
  CHECK(m["version"] == kUiVersion);
  CHECK(m["toasts"].size() == 2);
  const json empty = UiModel{};
  CHECK(empty["page"].is_null());
  CHECK(empty["hint"].is_null());
  CHECK(empty["chat"]["view"] == "closed");
  CHECK(empty["chat"]["max_chars"] == kLineMaxChars);
  CHECK(empty["chat"]["max_bytes"] == kLineMaxBytes);
  CHECK(json{{"version", 1}}.get<UiModel>() == UiModel{});
}

TEST_CASE("every event kind round-trips with its version, and holds exactly one kind") {
  round_trip(event_of(UiEvent::OpenChat{OpenVia::PadChord}));
  round_trip(event_of(UiEvent::Submit{"Is it far, the place with the bells? caf\xC3\xA9"}));
  round_trip(event_of(UiEvent::PageShown{PageId{3, 2, 1}, (std::uint64_t{1} << 54) + 3}));
  round_trip(event_of(UiEvent::Close{CloseReason::Refused, "", "an invented mod"}));
  round_trip(event_of(UiEvent::Close{CloseReason::Shut, "half a line", ""}));
  round_trip(event_of(UiEvent::Skip{PageId{3, 2, 0}}));
  round_trip(UiEvent{});
  for (const CloseReason r : {CloseReason::Escape, CloseReason::FocusLost, CloseReason::InputLost, CloseReason::Refused,
                              CloseReason::NotNow, CloseReason::Fault, CloseReason::Shut}) {
    round_trip(event_of(UiEvent::Close{r, "d", ""}));
  }

  const json shown = event_of(UiEvent::PageShown{PageId{3, 2, 1}, 81234});
  CHECK(shown == json{{"version", 1}, {"page_shown", {{"page", {{"conv", 3}, {"reply", 2}, {"index", 1}}}, {"t_ms", 81234}}}});
  CHECK(json(UiEvent{})["open_chat"]["via"] == "talk_key");
  CHECK(json(event_of(UiEvent::Close{CloseReason::NotNow, "", "a menu is open"}))["close"]["reason"] == "not_now");
  CHECK(read_json<UiEvent>(R"({"version": 1, "skip": {}})") == event_of(UiEvent::Skip{}));
  CHECK(read_json<UiEvent>(R"({"version": 1, "submit": {"text": "Hello."}})") == event_of(UiEvent::Submit{"Hello."}));
}

TEST_CASE("a field left out keeps its default") {
  const auto page = json{{"text", "Only this."}}.get<PageView>();
  CHECK(page.text == "Only this.");
  CHECK(page.count == 1);
  CHECK(page.kind == PageKind::Speech);
  const auto box = json{{"view", "open"}}.get<ChatBox>();
  CHECK(box.view == BoxView::Open);
  CHECK(box.max_chars == kLineMaxChars);
  CHECK(json::object().get<UiEvent::Close>() == UiEvent::Close{});
  CHECK(json::object().get<TalkHint>() == TalkHint{});
}

TEST_CASE("malformed forms are refused, naming the path and the problem") {
  // The version: there, and the one this build reads.
  CHECK(refusal<UiModel>(json::object()) == "UiModel: needs \"version\"");
  CHECK(refusal<UiModel>(json{{"version", 2}}) == "UiModel.version: must be 1");
  CHECK(refusal<UiModel>(json{{"version", "1"}}) == "UiModel.version: must be 1");
  CHECK(refusal<UiEvent>(json{{"skip", json::object()}}) == "UiEvent: needs \"version\"");
  CHECK(refusal<UiEvent>(json{{"version", -1}, {"skip", json::object()}}) == "UiEvent.version: must be 1");
  CHECK(refusal<UiModel>(json::array()) == "UiModel: must be an object");

  // Exactly one kind.
  CHECK(refusal<UiEvent>(json{{"version", 1}}) ==
        "UiEvent: needs exactly one of \"open_chat\", \"submit\", \"page_shown\", \"close\" and \"skip\"");
  CHECK(refusal<UiEvent>(json{{"version", 1}, {"skip", json::object()}, {"submit", json::object()}}) ==
        "UiEvent: needs exactly one of \"open_chat\", \"submit\", \"page_shown\", \"close\" and \"skip\"");
  CHECK(refusal<UiEvent>(json{{"version", 1}, {"typing", json::object()}}) == "UiEvent: has an unknown field \"typing\"");

  // Each field of its own type, by its path.
  CHECK(refusal<UiModel>(json{{"version", 1}, {"chat", {{"view", "wide"}}}}) ==
        "UiModel.chat.view: must be one of closed, open, strip");
  CHECK(refusal<UiModel>(json{{"version", 1}, {"page", {{"id", {{"index", 256}}}}}}) ==
        "UiModel.page.id.index: is out of range");
  CHECK(refusal<UiModel>(json{{"version", 1}, {"page", {{"hold_ms", -5}}}}) ==
        "UiModel.page.hold_ms: must be a whole number of 0 or more");
  CHECK(refusal<UiModel>(json{{"version", 1}, {"chat", {{"history", {{{"who", "narrator"}}}}}}}) ==
        "UiModel.chat.history[0].who: must be one of tarnished, npc");
  CHECK(refusal<UiModel>(json{{"version", 1}, {"toasts", {{{"level", "loud"}}}}}) ==
        "UiModel.toasts[0].level: must be one of info, warning, error");
  CHECK(refusal<UiModel>(json{{"version", 1}, {"hint", {{"target", -1}}}}) ==
        "UiModel.hint.target: must be a whole number of 0 or more");
  CHECK(refusal<UiEvent>(json{{"version", 1}, {"open_chat", {{"via", "mouse"}}}}) ==
        "UiEvent.open_chat.via: must be one of talk_key, pad_chord");
  CHECK(refusal<UiEvent>(json{{"version", 1}, {"close", {{"reason", "bored"}}}}) ==
        "UiEvent.close.reason: must be one of escape, focus_lost, input_lost, refused, not_now, fault, shut");
  CHECK(refusal<UiEvent>(json{{"version", 1}, {"page_shown", {{"t_ms", 1.5}}}}) ==
        "UiEvent.page_shown.t_ms: must be a whole number of 0 or more");
  CHECK(refusal<UiEvent>(json{{"version", 1}, {"submit", {{"text", 5}}}}) == "UiEvent.submit.text: must be a string");

  // A value is never quoted: the player's line in a refused field stays out of the message.
  const std::string line = "an-invented-line-that-must-not-be-echoed";
  CHECK(refusal<UiEvent>(json{{"version", 1}, {"close", {{"reason", line}}}}).find(line) == std::string::npos);
  CHECK(refusal<UiModel>(json{{"version", 1}, {"chat", {{"state", line}}}}).find(line) == std::string::npos);

  // From text: past the caps or not JSON at all, before any field is read.
  CHECK_THROWS_WITH_AS(read_json<UiEvent>("{\"version\": "), "the text is not valid JSON", ProtoError);
}

TEST_CASE("names") {
  CHECK(std::string(to_string(ConvState::Streaming)) == "streaming");
  CHECK(std::string(to_string(PageKind::Failure)) == "failure");
  CHECK(std::string(to_string(Surface::LowerThird)) == "lower_third");
  CHECK(std::string(to_string(BoxView::Open)) == "open");
  CHECK(std::string(to_string(HistoryLine::Who::Npc)) == "npc");
  CHECK(std::string(to_string(HintVerb::Next)) == "next");
  CHECK(std::string(to_string(ToastLevel::Warning)) == "warning");
  CHECK(std::string(to_string(OpenVia::PadChord)) == "pad_chord");
  CHECK(std::string(to_string(CloseReason::InputLost)) == "input_lost");
}
