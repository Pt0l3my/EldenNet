#pragma once
// The conversation's surfaces (NPC §5, §6, §8.6, §8.7, §10): what the core publishes for them after each step,
// UiModel, and what they send back, UiEvent (ARCH §1.3's C6 and C7). Since GraceUI's G15 both stay on T6, the core's
// thread: the core steps, and the overlay's part on T6 draws the newest model through GraceUI's ABI (at most ten times
// a second while GraceUI's ABI has no on_present, WHY§364) and hands the core what the player did. Plain data, with
// JSON forms (ui_json.cpp) for recordings, replays, the eval and the fake game's tests; each form carries a version.
//
// A model holds no model text that is not on display: the page is the one the surfaces show now (sealed for display,
// and acknowledged by PageShown on its first visible frame), the history strip holds committed lines and the player's
// own queued line, and nothing unsealed, held, dropped or captured as a trailer is in it (NPC §8.7). Names are the
// game's own, as the player's install shows them at run time; every other word a player reads (toasts, the card's
// "…") is ours. The surfaces own the layout, the fonts, the fades and the key glyphs (NPC §10.1).
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/proto/snapshot.hpp"

namespace en::proto {

// The JSON forms' version: written in every UiModel and UiEvent, and the only one read.
inline constexpr std::uint32_t kUiVersion = 1;

// The caps the surfaces keep (NPC §8.5, §8.6).
inline constexpr std::size_t kLineMaxChars = 240;    // the player's line, as typed
inline constexpr std::size_t kLineMaxBytes = 720;
inline constexpr std::size_t kPageMaxChars = 116;    // two wrapped lines of 58
inline constexpr std::size_t kPageMaxBytes = 180;
inline constexpr std::size_t kSpeakerMaxBytes = 48;  // a name line
inline constexpr std::size_t kToastMaxBytes = 160;

using ConvId = std::uint32_t;   // a conversation's, from 1 in a session; 0 is none
using ReplyId = std::uint32_t;  // a reply's, from 1 within its conversation (a regeneration is the same reply)

// A page of a reply: what PageShown acknowledges, and what a commit names (NPC §8.7).
struct PageId {
  ConvId conv = 0;
  ReplyId reply = 0;
  std::uint8_t index = 0;  // from 0

  friend constexpr bool operator==(const PageId&, const PageId&) = default;
};

// The conversation's states (NPC §6.1): `ConvState` covers the reply; the box is the orthogonal flag (BoxView).
enum class ConvState : std::uint8_t {
  Closed,     // no conversation
  Open,       // open, nothing pending: the player may send a line
  Awaiting,   // a line sent, no page shown yet (the addressee decision, the request, the first page's sealing)
  Streaming,  // pages show while the reply still streams
  Paging,     // the stream is done; its pages are still showing
  Held,       // unshown pages held: native talk within 20 m, or the overlay hidden (NPC §5.8)
  Ending,     // an end trigger fired: the current page plays out (closed, retarget) or the end is at once
};

// ---- The lower third (NPC §8.6, §8.7, §10.1) ----

enum class PageKind : std::uint8_t {
  Speech,   // a page of the reply's text: PageShown on its first visible frame, and the core commits it
  Card,     // the speaker card, the name and "…" pulsing while a reply is awaited (NPC §6.2): never acknowledged
  Failure,  // the "…" page after no answer (NPC §6.2): shown, never acknowledged, never committed
};

enum class Surface : std::uint8_t {
  LowerThird,    // the conversation page above the native subtitle band
  HistoryStrip,  // the chat box's history strip, with the game's subtitles off (NPC §8.6)
};

struct PageView {
  PageId id;
  PageKind kind = PageKind::Speech;
  Surface surface = Surface::LowerThird;
  std::string speaker;        // the name line: the speaker's name as the game shows it (the surface sets its case)
  std::string text;           // the page's text, at most kPageMaxChars and kPageMaxBytes; empty for a card
  std::uint8_t count = 1;     // the reply's pages sealed so far: id.index < count
  bool last = false;          // the reply is complete and no page follows this one: it fades out after its hold
  std::uint32_t hold_ms = 0;  // how long it holds from its first visible frame (NPC §8.6); 0 for a card

  bool operator==(const PageView&) const = default;
};

// ---- The chat box (NPC §5.1, §5.4, §6.3, §10.1) ----

enum class BoxView : std::uint8_t {
  Closed,  // nothing drawn
  Open,    // the box: header, history strip and input; the overlay holds input (request_input) while it is open
  Strip,   // the history strip alone, unfocused and holding no input (subtitles off, the box hidden by danger)
};

// A line of the history strip, oldest first.
struct HistoryLine {
  enum class Who : std::uint8_t { Tarnished, Npc };
  Who who = Who::Tarnished;
  std::string speaker;   // an NPC line's speaker, as the game shows the name; empty for the Tarnished
  std::string text;      // committed text (NPC §8.7 item 6), or the player's queued line
  bool waiting = false;  // the queued line, greyed with "(waiting)" until it is dispatched (NPC §6.3)

  bool operator==(const HistoryLine&) const = default;
};

struct ChatBox {
  BoxView view = BoxView::Closed;
  ConvId conv = 0;
  ConvState state = ConvState::Closed;
  std::string partner;                // the header's target, as the game shows the name ("Speak to …", O97)
  std::vector<std::string> heard_by;  // the personas in the speech audience ("… heard by: …", NPC §5.1)
  std::vector<HistoryLine> history;
  // The text the input holds when the box opens (a kept draft, NPC §5.4): the overlay takes it each time the view
  // becomes Open or the conversation changes, and keeps its own text from then on.
  std::string draft;
  std::uint16_t max_chars = kLineMaxChars;  // the input's limits (NPC §8.5)
  std::uint16_t max_bytes = kLineMaxBytes;

  bool operator==(const ChatBox&) const = default;
};

// ---- The talk hint and the target marker (NPC §5.1, §5.2) ----

enum class HintVerb : std::uint8_t {
  Speak,   // "[V] Speak — <name>" (the talk key as bound: V by default, O152)
  Answer,  // "[V] Answer — <name>": a persona's line addressed to the Tarnished showed within 10 s (NPC §5.9)
  Next,    // "Speak to <name> next": chat armed for the persona in a native talk (NPC §5.2, rule 1)
};

struct TalkHint {
  HintVerb verb = HintVerb::Speak;
  std::string name;  // the target's name as the game shows it
  // The marker's character: the overlay places the diamond over its head from the render feed (it is tracked), and
  // puts the hint at (0.50 W, 0.68 H) when the feed has no head for it or no camera.
  ChrHandle target;

  bool operator==(const TalkHint&) const = default;
};

// ---- Toasts (NPC §10.5): GraceUI notices ----

enum class ToastLevel : std::uint8_t { Info, Warning, Error };

struct Toast {
  std::uint32_t id = 0;  // the core's, unique in the session: a surface posts each id once
  ToastLevel level = ToastLevel::Info;
  std::string text;      // our own words, at most kToastMaxBytes

  bool operator==(const Toast&) const = default;
};

// ---- The rollback choice (MEM §9.1, O144; WHY§690): EldenNet's own panel, on GraceUI's modal layer ----

// A bullet's colour: the save's side, EldenNet's memory's side, what the cut would undo, a quest stage it would undo,
// and a note.
enum class ChoiceTone : std::uint8_t { Save, Memory, Undone, Stage, Note };
struct ChoiceBullet {
  ChoiceTone tone = ChoiceTone::Note;
  std::string text;  // our own words, names as the game shows them, or words such as "someone you know"

  bool operator==(const ChoiceBullet&) const = default;
};
enum class ChoiceAnswer : std::uint8_t { Forget, Keep, NewCharacter };
inline constexpr std::size_t kChoiceAnswers = 3;
struct ChoicePanel {
  std::uint32_t id = 0;  // a new one each time it is asked: an answer or a close names it
  std::string title;
  std::string lead;
  std::vector<ChoiceBullet> bullets;
  std::vector<std::string> answers;  // in ChoiceAnswer's order; the first has the focus when it opens

  bool operator==(const ChoicePanel&) const = default;
};

// ---- What the core publishes after each step (C6) ----

struct UiModel {
  std::optional<PageView> page;  // the lower third (or the strip's page), or nothing
  ChatBox chat;
  std::optional<TalkHint> hint;
  // The toasts up now, oldest first: the core keeps NPC §10.5's policy (at most two, six seconds each, the queue, the
  // rate limit, held during a boss fight), so a surface shows what is listed and lets go of what is no longer.
  std::vector<Toast> toasts;
  // The rollback choice, while it waits and the moment is safe (MEM §9.1); the surfaces hold input for it.
  std::optional<ChoicePanel> choice;

  bool operator==(const UiModel&) const = default;
};

// ---- What the surfaces send back (C7) ----

enum class OpenVia : std::uint8_t {
  TalkKey,   // the talk key (O94), on key-down
  PadChord,  // the pad's chord: D-pad Up, then View (GraceUI D204)
};

enum class CloseReason : std::uint8_t {
  Escape,     // Esc or the pad's B in the open box (NPC §5.7; WHY§461: none once the box has closed)
  FocusLost,  // the game's window lost focus (NPC §5.5)
  InputLost,  // GraceUI ended the hold: its watchdog (2 s without a frame), the player's exit, the hold lost (D77)
  Refused,    // request_input answered refused: `detail` names the holder; the box never opened
  NotNow,     // request_input answered not_now: `detail` is its reason; the box never opened
  Fault,      // the overlay's guard tripped (ffi_guard released input first)
  Shut,       // the model closed the box (danger, an ending): the overlay's answer, carrying its draft
};

struct UiEvent {
  // The talk key or the chord went down (NPC §5.1): the core picks the target (NPC §5.2) and opens the box, or toasts.
  // It comes first and has no default member initializer, as Lifecycle's DataReady: the variant needs its default
  // constructor while this class is still being defined. The variant value-initialises it, so `via` is TalkKey.
  struct OpenChat {
    OpenVia via;

    friend bool operator==(const OpenChat&, const OpenChat&) = default;
  };
  // Enter with a line in the box (NPC §6.1, §6.3): the player's text as typed, which the core sanitizes and caps
  // (kLineMaxChars, kLineMaxBytes) before it commits it.
  struct Submit {
    std::string text;

    friend bool operator==(const Submit&, const Submit&) = default;
  };
  // A Speech page's first frame drawn with non-zero opacity (NPC §8.7 item 1). `t_ms` is that frame's time on the
  // core's clock (T6's mono milliseconds); a duplicate is ignored.
  struct PageShown {
    PageId page;
    std::uint64_t t_ms = 0;

    friend bool operator==(const PageShown&, const PageShown&) = default;
  };
  // The box closed on the surfaces' side, or never opened.
  struct Close {
    CloseReason reason = CloseReason::Escape;
    std::string draft;   // what the input held, kept 60 s for its target (NPC §5.4); empty when it never opened
    std::string detail;  // Refused: the holder GraceUI named; NotNow: GraceUI's reason; empty otherwise

    friend bool operator==(const Close&, const Close&) = default;
  };
  // Tab while a page shows: end its hold, at least 300 ms after it showed (NPC §8.6). `page` is the page that showed
  // when Tab went down, so a skip never ends the page after it.
  struct Skip {
    PageId page;

    friend bool operator==(const Skip&, const Skip&) = default;
  };

  // The rollback choice's answer: Enter or A on a focused answer (there is no answer of EldenNet's own).
  struct Choose {
    std::uint32_t panel = 0;
    ChoiceAnswer answer = ChoiceAnswer::Forget;

    friend bool operator==(const Choose&, const Choose&) = default;
  };
  // The choice's panel closed with no answer (danger, a load, the player's exit) or never took input: the core asks
  // again at the next safe moment.
  struct ChoiceClosed {
    std::uint32_t panel = 0;
    CloseReason reason = CloseReason::InputLost;
    std::string detail;  // Refused: the holder GraceUI named; NotNow: GraceUI's reason

    friend bool operator==(const ChoiceClosed&, const ChoiceClosed&) = default;
  };

  // A default UiEvent is an OpenChat by the talk key, as a default Lifecycle is a DataReady.
  std::variant<OpenChat, Submit, PageShown, Close, Skip, Choose, ChoiceClosed> event;

  bool operator==(const UiEvent&) const = default;
};

// ---- Names: the JSON forms' words for each enum, in snake_case ----

const char* to_string(ConvState v);    // closed, open, awaiting, streaming, paging, held, ending
const char* to_string(PageKind v);     // speech, card, failure
const char* to_string(Surface v);      // lower_third, history_strip
const char* to_string(BoxView v);      // closed, open, strip
const char* to_string(HistoryLine::Who v);  // tarnished, npc
const char* to_string(HintVerb v);     // speak, answer, next
const char* to_string(ToastLevel v);   // info, warning, error
const char* to_string(OpenVia v);      // talk_key, pad_chord
const char* to_string(CloseReason v);  // escape, focus_lost, input_lost, refused, not_now, fault, shut
const char* to_string(ChoiceTone v);   // save, memory, undone, stage, note
const char* to_string(ChoiceAnswer v);  // forget, keep, new_character

// ---- JSON forms (ui_json.cpp) ----
//
// As the model client's messages (llm.hpp): every field under its own name, an absent optional as null; read strictly,
// with a field left out keeping its default and anything else refused with ProtoError, naming the path and the
// problem. A UiModel and a UiEvent also carry "version", which must be there and be kUiVersion; a UiEvent holds
// exactly one of "open_chat", "submit", "page_shown", "close", "skip", "choose" and "choice_closed". A handle is an
// integer.
//
//   {"version": 1, "page_shown": {"page": {"conv": 3, "reply": 2, "index": 0}, "t_ms": 81234}}

void to_json(nlohmann::json& j, ConvState v);
void from_json(const nlohmann::json& j, ConvState& v);
void to_json(nlohmann::json& j, PageKind v);
void from_json(const nlohmann::json& j, PageKind& v);
void to_json(nlohmann::json& j, Surface v);
void from_json(const nlohmann::json& j, Surface& v);
void to_json(nlohmann::json& j, BoxView v);
void from_json(const nlohmann::json& j, BoxView& v);
void to_json(nlohmann::json& j, HistoryLine::Who v);
void from_json(const nlohmann::json& j, HistoryLine::Who& v);
void to_json(nlohmann::json& j, HintVerb v);
void from_json(const nlohmann::json& j, HintVerb& v);
void to_json(nlohmann::json& j, ToastLevel v);
void from_json(const nlohmann::json& j, ToastLevel& v);
void to_json(nlohmann::json& j, OpenVia v);
void from_json(const nlohmann::json& j, OpenVia& v);
void to_json(nlohmann::json& j, CloseReason v);
void from_json(const nlohmann::json& j, CloseReason& v);
void to_json(nlohmann::json& j, ChoiceTone v);
void from_json(const nlohmann::json& j, ChoiceTone& v);
void to_json(nlohmann::json& j, ChoiceAnswer v);
void from_json(const nlohmann::json& j, ChoiceAnswer& v);

void to_json(nlohmann::json& j, const PageId& v);
void from_json(const nlohmann::json& j, PageId& v);
void to_json(nlohmann::json& j, const PageView& v);
void from_json(const nlohmann::json& j, PageView& v);
void to_json(nlohmann::json& j, const HistoryLine& v);
void from_json(const nlohmann::json& j, HistoryLine& v);
void to_json(nlohmann::json& j, const ChatBox& v);
void from_json(const nlohmann::json& j, ChatBox& v);
void to_json(nlohmann::json& j, const TalkHint& v);
void from_json(const nlohmann::json& j, TalkHint& v);
void to_json(nlohmann::json& j, const Toast& v);
void from_json(const nlohmann::json& j, Toast& v);
void to_json(nlohmann::json& j, const ChoiceBullet& v);
void from_json(const nlohmann::json& j, ChoiceBullet& v);
void to_json(nlohmann::json& j, const ChoicePanel& v);
void from_json(const nlohmann::json& j, ChoicePanel& v);
void to_json(nlohmann::json& j, const UiModel& v);
void from_json(const nlohmann::json& j, UiModel& v);
void to_json(nlohmann::json& j, const UiEvent::OpenChat& v);
void from_json(const nlohmann::json& j, UiEvent::OpenChat& v);
void to_json(nlohmann::json& j, const UiEvent::Submit& v);
void from_json(const nlohmann::json& j, UiEvent::Submit& v);
void to_json(nlohmann::json& j, const UiEvent::PageShown& v);
void from_json(const nlohmann::json& j, UiEvent::PageShown& v);
void to_json(nlohmann::json& j, const UiEvent::Close& v);
void from_json(const nlohmann::json& j, UiEvent::Close& v);
void to_json(nlohmann::json& j, const UiEvent::Skip& v);
void from_json(const nlohmann::json& j, UiEvent::Skip& v);
void to_json(nlohmann::json& j, const UiEvent::Choose& v);
void from_json(const nlohmann::json& j, UiEvent::Choose& v);
void to_json(nlohmann::json& j, const UiEvent::ChoiceClosed& v);
void from_json(const nlohmann::json& j, UiEvent::ChoiceClosed& v);
void to_json(nlohmann::json& j, const UiEvent& v);
void from_json(const nlohmann::json& j, UiEvent& v);

}  // namespace en::proto
