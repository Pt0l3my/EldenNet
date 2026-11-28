// The JSON forms of the conversation's surfaces' messages (ui.hpp): UiModel and UiEvent with their parts, written with
// every field under its own name and read strictly, with the path and the problem in every refusal (json_read.hpp).
// Each top-level form carries "version", which must be kUiVersion.
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "en/proto/ui.hpp"
#include "json_read.hpp"

namespace en::proto {

using nlohmann::json;

// ---- Names ----

const char* to_string(ConvState v) {
  switch (v) {
    case ConvState::Closed: return "closed";
    case ConvState::Open: return "open";
    case ConvState::Awaiting: return "awaiting";
    case ConvState::Streaming: return "streaming";
    case ConvState::Paging: return "paging";
    case ConvState::Held: return "held";
    case ConvState::Ending: return "ending";
  }
  return "closed";
}

const char* to_string(PageKind v) {
  switch (v) {
    case PageKind::Speech: return "speech";
    case PageKind::Card: return "card";
    case PageKind::Failure: return "failure";
  }
  return "speech";
}

const char* to_string(Surface v) {
  switch (v) {
    case Surface::LowerThird: return "lower_third";
    case Surface::HistoryStrip: return "history_strip";
  }
  return "lower_third";
}

const char* to_string(BoxView v) {
  switch (v) {
    case BoxView::Closed: return "closed";
    case BoxView::Open: return "open";
    case BoxView::Strip: return "strip";
  }
  return "closed";
}

const char* to_string(HistoryLine::Who v) {
  switch (v) {
    case HistoryLine::Who::Tarnished: return "tarnished";
    case HistoryLine::Who::Npc: return "npc";
  }
  return "tarnished";
}

const char* to_string(HintVerb v) {
  switch (v) {
    case HintVerb::Speak: return "speak";
    case HintVerb::Answer: return "answer";
    case HintVerb::Next: return "next";
  }
  return "speak";
}

const char* to_string(ToastLevel v) {
  switch (v) {
    case ToastLevel::Info: return "info";
    case ToastLevel::Warning: return "warning";
    case ToastLevel::Error: return "error";
  }
  return "info";
}

const char* to_string(OpenVia v) {
  switch (v) {
    case OpenVia::TalkKey: return "talk_key";
    case OpenVia::PadChord: return "pad_chord";
  }
  return "talk_key";
}

const char* to_string(CloseReason v) {
  switch (v) {
    case CloseReason::Escape: return "escape";
    case CloseReason::FocusLost: return "focus_lost";
    case CloseReason::InputLost: return "input_lost";
    case CloseReason::Refused: return "refused";
    case CloseReason::NotNow: return "not_now";
    case CloseReason::Fault: return "fault";
    case CloseReason::Shut: return "shut";
  }
  return "escape";
}

const char* to_string(ChoiceTone v) {
  switch (v) {
    case ChoiceTone::Save: return "save";
    case ChoiceTone::Memory: return "memory";
    case ChoiceTone::Undone: return "undone";
    case ChoiceTone::Stage: return "stage";
    case ChoiceTone::Note: return "note";
  }
  return "note";
}

const char* to_string(ChoiceAnswer v) {
  switch (v) {
    case ChoiceAnswer::Forget: return "forget";
    case ChoiceAnswer::Keep: return "keep";
    case ChoiceAnswer::NewCharacter: return "new_character";
  }
  return "forget";
}

// ---- Readers ----

namespace detail {

void read(const json& j, const At& at, ChrHandle& out);  // game_json.cpp

void read(const json& j, const At& at, ConvState& out);
void read(const json& j, const At& at, PageKind& out);
void read(const json& j, const At& at, Surface& out);
void read(const json& j, const At& at, BoxView& out);
void read(const json& j, const At& at, HistoryLine::Who& out);
void read(const json& j, const At& at, HintVerb& out);
void read(const json& j, const At& at, ToastLevel& out);
void read(const json& j, const At& at, OpenVia& out);
void read(const json& j, const At& at, CloseReason& out);
void read(const json& j, const At& at, ChoiceTone& out);
void read(const json& j, const At& at, ChoiceAnswer& out);
void read(const json& j, const At& at, PageId& out);
void read(const json& j, const At& at, PageView& out);
void read(const json& j, const At& at, HistoryLine& out);
void read(const json& j, const At& at, ChatBox& out);
void read(const json& j, const At& at, TalkHint& out);
void read(const json& j, const At& at, Toast& out);
void read(const json& j, const At& at, ChoiceBullet& out);
void read(const json& j, const At& at, ChoicePanel& out);
void read(const json& j, const At& at, UiModel& out);
void read(const json& j, const At& at, UiEvent::OpenChat& out);
void read(const json& j, const At& at, UiEvent::Submit& out);
void read(const json& j, const At& at, UiEvent::PageShown& out);
void read(const json& j, const At& at, UiEvent::Close& out);
void read(const json& j, const At& at, UiEvent::Skip& out);
void read(const json& j, const At& at, UiEvent::Choose& out);
void read(const json& j, const At& at, UiEvent::ChoiceClosed& out);
void read(const json& j, const At& at, UiEvent& out);

constexpr std::array kConvStates = {ConvState::Closed,    ConvState::Open, ConvState::Awaiting, ConvState::Streaming,
                                    ConvState::Paging,    ConvState::Held, ConvState::Ending};
constexpr std::array kPageKinds = {PageKind::Speech, PageKind::Card, PageKind::Failure};
constexpr std::array kSurfaces = {Surface::LowerThird, Surface::HistoryStrip};
constexpr std::array kBoxViews = {BoxView::Closed, BoxView::Open, BoxView::Strip};
constexpr std::array kWhos = {HistoryLine::Who::Tarnished, HistoryLine::Who::Npc};
constexpr std::array kHintVerbs = {HintVerb::Speak, HintVerb::Answer, HintVerb::Next};
constexpr std::array kToastLevels = {ToastLevel::Info, ToastLevel::Warning, ToastLevel::Error};
constexpr std::array kOpenVias = {OpenVia::TalkKey, OpenVia::PadChord};
constexpr std::array kCloseReasons = {CloseReason::Escape,  CloseReason::FocusLost, CloseReason::InputLost,
                                      CloseReason::Refused, CloseReason::NotNow,    CloseReason::Fault,
                                      CloseReason::Shut};
constexpr std::array kChoiceTones = {ChoiceTone::Save, ChoiceTone::Memory, ChoiceTone::Undone, ChoiceTone::Stage,
                                     ChoiceTone::Note};
constexpr std::array kChoiceAnswersAll = {ChoiceAnswer::Forget, ChoiceAnswer::Keep, ChoiceAnswer::NewCharacter};

void read(const json& j, const At& at, ConvState& out) { read_enum(j, at, kConvStates, out); }
void read(const json& j, const At& at, PageKind& out) { read_enum(j, at, kPageKinds, out); }
void read(const json& j, const At& at, Surface& out) { read_enum(j, at, kSurfaces, out); }
void read(const json& j, const At& at, BoxView& out) { read_enum(j, at, kBoxViews, out); }
void read(const json& j, const At& at, HistoryLine::Who& out) { read_enum(j, at, kWhos, out); }
void read(const json& j, const At& at, HintVerb& out) { read_enum(j, at, kHintVerbs, out); }
void read(const json& j, const At& at, ToastLevel& out) { read_enum(j, at, kToastLevels, out); }
void read(const json& j, const At& at, OpenVia& out) { read_enum(j, at, kOpenVias, out); }
void read(const json& j, const At& at, CloseReason& out) { read_enum(j, at, kCloseReasons, out); }
void read(const json& j, const At& at, ChoiceTone& out) { read_enum(j, at, kChoiceTones, out); }
void read(const json& j, const At& at, ChoiceAnswer& out) { read_enum(j, at, kChoiceAnswersAll, out); }

void read(const json& j, const At& at, PageId& out) {
  out = PageId{};
  Fields(j, at)("conv", out.conv)("reply", out.reply)("index", out.index).done();
}

void read(const json& j, const At& at, PageView& out) {
  out = PageView{};
  Fields(j, at)("id", out.id)("kind", out.kind)("surface", out.surface)("speaker", out.speaker)("text", out.text)(
      "count", out.count)("last", out.last)("hold_ms", out.hold_ms)
      .done();
}

void read(const json& j, const At& at, HistoryLine& out) {
  out = HistoryLine{};
  Fields(j, at)("who", out.who)("speaker", out.speaker)("text", out.text)("waiting", out.waiting).done();
}

void read(const json& j, const At& at, ChatBox& out) {
  out = ChatBox{};
  Fields(j, at)("view", out.view)("conv", out.conv)("state", out.state)("partner", out.partner)(
      "heard_by", out.heard_by)("history", out.history)("draft", out.draft)("max_chars", out.max_chars)(
      "max_bytes", out.max_bytes)
      .done();
}

void read(const json& j, const At& at, TalkHint& out) {
  out = TalkHint{};
  Fields(j, at)("verb", out.verb)("name", out.name)("target", out.target).done();
}

void read(const json& j, const At& at, Toast& out) {
  out = Toast{};
  Fields(j, at)("id", out.id)("level", out.level)("text", out.text).done();
}

void read(const json& j, const At& at, ChoiceBullet& out) {
  out = ChoiceBullet{};
  Fields(j, at)("tone", out.tone)("text", out.text).done();
}

void read(const json& j, const At& at, ChoicePanel& out) {
  out = ChoicePanel{};
  Fields(j, at)("id", out.id)("title", out.title)("lead", out.lead)("bullets", out.bullets)("answers", out.answers)
      .done();
}

namespace {

// The version a top-level form must carry: there, and kUiVersion.
void check_version(const json& j, const At& at) {
  if (!j.is_object()) fail(at, "must be an object");
  const auto it = j.find("version");
  if (it == j.end()) fail(at, "needs \"version\"");
  // A whole number, signed or not as it was made (text reads as unsigned); no other value converts to kUiVersion.
  if (!it->is_number_integer() || it->get<std::int64_t>() != std::int64_t{kUiVersion}) {
    fail(At{&at, "version", 0}, "must be " + std::to_string(kUiVersion));
  }
}

}  // namespace

void read(const json& j, const At& at, UiModel& out) {
  out = UiModel{};
  check_version(j, at);
  std::uint32_t version = 0;
  Fields(j, at)("version", version)("page", out.page)("chat", out.chat)("hint", out.hint)("toasts", out.toasts)(
      "choice", out.choice)
      .done();
}

void read(const json& j, const At& at, UiEvent::OpenChat& out) {
  out = UiEvent::OpenChat{};
  Fields(j, at)("via", out.via).done();
}

void read(const json& j, const At& at, UiEvent::Submit& out) {
  out = UiEvent::Submit{};
  Fields(j, at)("text", out.text).done();
}

void read(const json& j, const At& at, UiEvent::PageShown& out) {
  out = UiEvent::PageShown{};
  Fields(j, at)("page", out.page)("t_ms", out.t_ms).done();
}

void read(const json& j, const At& at, UiEvent::Close& out) {
  out = UiEvent::Close{};
  Fields(j, at)("reason", out.reason)("draft", out.draft)("detail", out.detail).done();
}

void read(const json& j, const At& at, UiEvent::Skip& out) {
  out = UiEvent::Skip{};
  Fields(j, at)("page", out.page).done();
}

void read(const json& j, const At& at, UiEvent::Choose& out) {
  out = UiEvent::Choose{};
  Fields(j, at)("panel", out.panel)("answer", out.answer).done();
}

void read(const json& j, const At& at, UiEvent::ChoiceClosed& out) {
  out = UiEvent::ChoiceClosed{};
  Fields(j, at)("panel", out.panel)("reason", out.reason)("detail", out.detail).done();
}

void read(const json& j, const At& at, UiEvent& out) {
  check_version(j, at);
  std::uint32_t version = 0;
  std::optional<UiEvent::OpenChat> open_chat;
  std::optional<UiEvent::Submit> submit;
  std::optional<UiEvent::PageShown> page_shown;
  std::optional<UiEvent::Close> close;
  std::optional<UiEvent::Skip> skip;
  std::optional<UiEvent::Choose> choose;
  std::optional<UiEvent::ChoiceClosed> choice_closed;
  Fields(j, at)("version", version)("open_chat", open_chat)("submit", submit)("page_shown", page_shown)(
      "close", close)("skip", skip)("choose", choose)("choice_closed", choice_closed)
      .done();
  const int kinds = int{open_chat.has_value()} + int{submit.has_value()} + int{page_shown.has_value()} +
                    int{close.has_value()} + int{skip.has_value()} + int{choose.has_value()} +
                    int{choice_closed.has_value()};
  if (kinds != 1) {
    fail(at, "needs exactly one of \"open_chat\", \"submit\", \"page_shown\", \"close\", \"skip\", \"choose\" "
             "and \"choice_closed\"");
  }
  if (open_chat) {
    out.event = *open_chat;
  } else if (submit) {
    out.event = std::move(*submit);
  } else if (page_shown) {
    out.event = *page_shown;
  } else if (close) {
    out.event = std::move(*close);
  } else if (skip) {
    out.event = *skip;
  } else if (choose) {
    out.event = *choose;
  } else {
    out.event = std::move(*choice_closed);
  }
}

}  // namespace detail

// ---- Writers: every field, an absent optional as null ----

namespace {

json write(const PageId& v) { return {{"conv", v.conv}, {"reply", v.reply}, {"index", v.index}}; }

json write(const PageView& v) {
  return {{"id", write(v.id)},       {"kind", to_string(v.kind)}, {"surface", to_string(v.surface)},
          {"speaker", v.speaker},    {"text", v.text},            {"count", v.count},
          {"last", v.last},          {"hold_ms", v.hold_ms}};
}

json write(const HistoryLine& v) {
  return {{"who", to_string(v.who)}, {"speaker", v.speaker}, {"text", v.text}, {"waiting", v.waiting}};
}

json write(const ChatBox& v) {
  json history = json::array();
  for (const HistoryLine& line : v.history) history.push_back(write(line));
  return {{"view", to_string(v.view)},
          {"conv", v.conv},
          {"state", to_string(v.state)},
          {"partner", v.partner},
          {"heard_by", v.heard_by},
          {"history", std::move(history)},
          {"draft", v.draft},
          {"max_chars", v.max_chars},
          {"max_bytes", v.max_bytes}};
}

json write(const TalkHint& v) { return {{"verb", to_string(v.verb)}, {"name", v.name}, {"target", v.target.raw}}; }

json write(const Toast& v) { return {{"id", v.id}, {"level", to_string(v.level)}, {"text", v.text}}; }

json write(const ChoiceBullet& v) { return {{"tone", to_string(v.tone)}, {"text", v.text}}; }

json write(const ChoicePanel& v) {
  json bullets = json::array();
  for (const ChoiceBullet& b : v.bullets) bullets.push_back(write(b));
  return {{"id", v.id}, {"title", v.title}, {"lead", v.lead}, {"bullets", std::move(bullets)}, {"answers", v.answers}};
}

json write(const UiModel& v) {
  json toasts = json::array();
  for (const Toast& t : v.toasts) toasts.push_back(write(t));
  return {{"version", kUiVersion},
          {"page", v.page ? write(*v.page) : json(nullptr)},
          {"chat", write(v.chat)},
          {"hint", v.hint ? write(*v.hint) : json(nullptr)},
          {"toasts", std::move(toasts)},
          {"choice", v.choice ? write(*v.choice) : json(nullptr)}};
}

json write(const UiEvent::OpenChat& v) { return {{"via", to_string(v.via)}}; }
json write(const UiEvent::Submit& v) { return {{"text", v.text}}; }
json write(const UiEvent::PageShown& v) { return {{"page", write(v.page)}, {"t_ms", v.t_ms}}; }
json write(const UiEvent::Close& v) {
  return {{"reason", to_string(v.reason)}, {"draft", v.draft}, {"detail", v.detail}};
}
json write(const UiEvent::Skip& v) { return {{"page", write(v.page)}}; }
json write(const UiEvent::Choose& v) { return {{"panel", v.panel}, {"answer", to_string(v.answer)}}; }
json write(const UiEvent::ChoiceClosed& v) {
  return {{"panel", v.panel}, {"reason", to_string(v.reason)}, {"detail", v.detail}};
}

json write(const UiEvent& v) {
  json out = {{"version", kUiVersion}};
  if (const auto* open_chat = std::get_if<UiEvent::OpenChat>(&v.event)) {
    out["open_chat"] = write(*open_chat);
  } else if (const auto* submit = std::get_if<UiEvent::Submit>(&v.event)) {
    out["submit"] = write(*submit);
  } else if (const auto* page_shown = std::get_if<UiEvent::PageShown>(&v.event)) {
    out["page_shown"] = write(*page_shown);
  } else if (const auto* close = std::get_if<UiEvent::Close>(&v.event)) {
    out["close"] = write(*close);
  } else if (const auto* skip = std::get_if<UiEvent::Skip>(&v.event)) {
    out["skip"] = write(*skip);
  } else if (const auto* choose = std::get_if<UiEvent::Choose>(&v.event)) {
    out["choose"] = write(*choose);
  } else {
    out["choice_closed"] = write(std::get<UiEvent::ChoiceClosed>(v.event));
  }
  return out;
}

}  // namespace

void to_json(json& j, ConvState v) { j = to_string(v); }
void from_json(const json& j, ConvState& v) { detail::read_root(j, "ConvState", v); }
void to_json(json& j, PageKind v) { j = to_string(v); }
void from_json(const json& j, PageKind& v) { detail::read_root(j, "PageKind", v); }
void to_json(json& j, Surface v) { j = to_string(v); }
void from_json(const json& j, Surface& v) { detail::read_root(j, "Surface", v); }
void to_json(json& j, BoxView v) { j = to_string(v); }
void from_json(const json& j, BoxView& v) { detail::read_root(j, "BoxView", v); }
void to_json(json& j, HistoryLine::Who v) { j = to_string(v); }
void from_json(const json& j, HistoryLine::Who& v) { detail::read_root(j, "Who", v); }
void to_json(json& j, HintVerb v) { j = to_string(v); }
void from_json(const json& j, HintVerb& v) { detail::read_root(j, "HintVerb", v); }
void to_json(json& j, ToastLevel v) { j = to_string(v); }
void from_json(const json& j, ToastLevel& v) { detail::read_root(j, "ToastLevel", v); }
void to_json(json& j, OpenVia v) { j = to_string(v); }
void from_json(const json& j, OpenVia& v) { detail::read_root(j, "OpenVia", v); }
void to_json(json& j, CloseReason v) { j = to_string(v); }
void from_json(const json& j, CloseReason& v) { detail::read_root(j, "CloseReason", v); }
void to_json(json& j, ChoiceTone v) { j = to_string(v); }
void from_json(const json& j, ChoiceTone& v) { detail::read_root(j, "ChoiceTone", v); }
void to_json(json& j, ChoiceAnswer v) { j = to_string(v); }
void from_json(const json& j, ChoiceAnswer& v) { detail::read_root(j, "ChoiceAnswer", v); }

void to_json(json& j, const PageId& v) { j = write(v); }
void from_json(const json& j, PageId& v) { detail::read_root(j, "PageId", v); }
void to_json(json& j, const PageView& v) { j = write(v); }
void from_json(const json& j, PageView& v) { detail::read_root(j, "PageView", v); }
void to_json(json& j, const HistoryLine& v) { j = write(v); }
void from_json(const json& j, HistoryLine& v) { detail::read_root(j, "HistoryLine", v); }
void to_json(json& j, const ChatBox& v) { j = write(v); }
void from_json(const json& j, ChatBox& v) { detail::read_root(j, "ChatBox", v); }
void to_json(json& j, const TalkHint& v) { j = write(v); }
void from_json(const json& j, TalkHint& v) { detail::read_root(j, "TalkHint", v); }
void to_json(json& j, const Toast& v) { j = write(v); }
void from_json(const json& j, Toast& v) { detail::read_root(j, "Toast", v); }
void to_json(json& j, const ChoiceBullet& v) { j = write(v); }
void from_json(const json& j, ChoiceBullet& v) { detail::read_root(j, "ChoiceBullet", v); }
void to_json(json& j, const ChoicePanel& v) { j = write(v); }
void from_json(const json& j, ChoicePanel& v) { detail::read_root(j, "ChoicePanel", v); }
void to_json(json& j, const UiModel& v) { j = write(v); }
void from_json(const json& j, UiModel& v) { detail::read_root(j, "UiModel", v); }
void to_json(json& j, const UiEvent::OpenChat& v) { j = write(v); }
void from_json(const json& j, UiEvent::OpenChat& v) { detail::read_root(j, "OpenChat", v); }
void to_json(json& j, const UiEvent::Submit& v) { j = write(v); }
void from_json(const json& j, UiEvent::Submit& v) { detail::read_root(j, "Submit", v); }
void to_json(json& j, const UiEvent::PageShown& v) { j = write(v); }
void from_json(const json& j, UiEvent::PageShown& v) { detail::read_root(j, "PageShown", v); }
void to_json(json& j, const UiEvent::Close& v) { j = write(v); }
void from_json(const json& j, UiEvent::Close& v) { detail::read_root(j, "Close", v); }
void to_json(json& j, const UiEvent::Skip& v) { j = write(v); }
void from_json(const json& j, UiEvent::Skip& v) { detail::read_root(j, "Skip", v); }
void to_json(json& j, const UiEvent::Choose& v) { j = write(v); }
void from_json(const json& j, UiEvent::Choose& v) { detail::read_root(j, "Choose", v); }
void to_json(json& j, const UiEvent::ChoiceClosed& v) { j = write(v); }
void from_json(const json& j, UiEvent::ChoiceClosed& v) { detail::read_root(j, "ChoiceClosed", v); }
void to_json(json& j, const UiEvent& v) { j = write(v); }
void from_json(const json& j, UiEvent& v) { detail::read_root(j, "UiEvent", v); }

}  // namespace en::proto
