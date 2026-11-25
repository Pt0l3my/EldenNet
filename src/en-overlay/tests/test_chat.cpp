// The conversation's surfaces against the fake GraceUI (chat.hpp; fake_grace.hpp; the Windows builds, under Wine):
// what is registered at connect (the white texture in the registration window, the faces before warm-up, the hotkeys
// with their players' names and the chord, the wait handle), the box opened by the model with its input asked for,
// typed in through the holder's events (the swallowed character, the caret, Enter, Esc), each way it closes (danger
// keeping the draft, the model's Shut, refused and not now), pages and their PageShown, Tab, the toasts, the hint's key
// words and their change, a fault, the wait handle's wake, and no input left with the box once it has closed.
#include <doctest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <windows.h>

#include "en/overlay/overlay.hpp"
#include "fake_grace.hpp"

using namespace en::overlay;
namespace proto = en::proto;

namespace {

const std::vector<unsigned char> kSerif(64, 0x5A);  // invented bytes: the fake reads no font
const std::vector<unsigned char> kSans(48, 0x5B);
const std::vector<unsigned char> kItalic(56, 0x5C);

// GraceUI's fonts as the DLL reads them: from 0.2.36 its folder holds EB Garamond Italic too (D241).
Options options(bool fonts = true, bool italic = true) {
  Options o;
  o.version = {0, 2, 0};
  o.surfaces = true;
  if (fonts) o.fonts = Surfaces::Fonts{kSerif, kSans, kItalic};
  if (!italic) o.fonts.italic = {};
  return o;
}

std::unique_ptr<Overlay> connect(fake::Grace& g, const Options& o, Report& r) {
  return std::unique_ptr<Overlay>(Overlay::connect(graceui::Api(g.api()), o, r));
}

ViewStatus status() { return ViewStatus{}; }

proto::UiModel open_box(proto::ConvId conv = 1, std::string draft = {}) {
  proto::UiModel m;
  m.chat.view = proto::BoxView::Open;
  m.chat.conv = conv;
  m.chat.state = proto::ConvState::Open;
  m.chat.partner = "Lantern-Keeper";  // invented
  m.chat.heard_by = {"Lantern-Keeper"};
  m.chat.draft = std::move(draft);
  return m;
}

proto::PageView speech(std::uint8_t index) {
  proto::PageView p;
  p.id = proto::PageId{1, 1, index};
  p.speaker = "Lantern-Keeper";
  p.text = "Hm. The road runs on past the lamp.";
  p.hold_ms = 3000;
  return p;
}

// The first event of its kind, by value (the events are often a temporary's).
template <class T>
std::optional<T> find(const std::vector<proto::UiEvent>& events) {
  for (const proto::UiEvent& e : events) {
    if (const auto* x = std::get_if<T>(&e.event)) return *x;
  }
  return std::nullopt;
}

// A connected overlay with its surfaces, warmed up, its display known, the box opened by the talk key.
struct Session {
  fake::Grace g;
  Report r;
  std::unique_ptr<Overlay> o;
  std::uint64_t now = 10'000;
  Session() {
    o = connect(g, options(), r);
    g.warm_up();
    (void)o->step(status(), nullptr, nullptr, now);
  }
  std::vector<proto::UiEvent> poll() { return o->step(status(), nullptr, nullptr, now).ui; }
  std::vector<proto::UiEvent> show(const proto::UiModel& m) { return o->show(m, now); }
  void open() {
    g.press("talk");
    const auto ev = poll();
    REQUIRE(find<proto::UiEvent::OpenChat>(ev).has_value());
    REQUIRE(show(open_box()).empty());
    REQUIRE(g.holding);
  }
};

}  // namespace

TEST_CASE("chat: connect registers the surfaces' texture, faces, hotkeys and wait handle") {
  fake::Grace g;
  Report r;
  auto o = connect(g, options(), r);
  REQUIRE(o != nullptr);
  REQUIRE(o->surfaces() != nullptr);
  CHECK(g.mod_caps == "notices world-state hotkeys panels textures faces input holder-events display pad-chords "
                      "hotkey-bindings wake hotkey-by");
  CHECK(std::string(r.surfaces.state) == "ready");
  CHECK(g.textures.size() == 1);  // the white texture, in the registration window
  CHECK_FALSE(g.window_closed);
  // The three faces, GraceUI's own files as the DLL read them, scaled, before warm-up (eldennet-changes §33, §69): the
  // serif, the sans and the serif's italic, at the serif's size (the Tarnished's lines scale down from it).
  REQUIRE(g.faces.size() == 3);
  CHECK(r.surfaces.faces == 3);
  CHECK(r.surfaces.italic);
  CHECK(g.faces[0].pixel_height == 30);
  CHECK(g.faces[0].flags == GRACEUI_FACE_SCALED);
  CHECK(g.faces[0].bytes == kSerif.size());
  CHECK(g.faces[1].pixel_height == 18);
  CHECK(g.faces[2].pixel_height == 30);
  CHECK(g.faces[2].flags == GRACEUI_FACE_SCALED);
  CHECK(g.faces[2].bytes == kItalic.size());
  // The hotkeys by their stable ids and players' names (§48): talk on T with the pad chord, D-pad Up then View (§47),
  // and skip on Tab.
  REQUIRE(g.hotkeys.size() == 2);
  CHECK(g.hotkeys[0].action == "talk");
  CHECK(g.hotkeys[0].name == "Talk");
  CHECK(g.hotkeys[0].key == 0x54);
  CHECK(g.hotkeys[0].chord == GRACEUI_PAD_CHORD(0x0001, 0x0020));
  CHECK(g.hotkeys[1].action == "skip-page");
  CHECK(g.hotkeys[1].key == 0x09);
  CHECK(std::string(r.surfaces.talk) == "bound");
  CHECK(std::string(r.surfaces.chord) == "asked");
  // The wait handle (ABI 1.5), set once when asked for.
  CHECK(r.surfaces.wake);
  REQUIRE(o->wake() != nullptr);
  CHECK(WaitForSingleObject(static_cast<HANDLE>(o->wake()), 0) == WAIT_OBJECT_0);
  CHECK(o->surfaces()->key_words() == "T");  // describe_hotkey's words
  CHECK(g.panels.empty());  // no panel until the display is known
}

TEST_CASE("chat: faces after warm-up are too late, and the surfaces draw GraceUI's notices' face") {
  fake::Grace g;
  g.warmed = true;
  Report r;
  auto o = connect(g, options(), r);
  REQUIRE(o != nullptr);
  CHECK(r.surfaces.faces == 0);
  CHECK_FALSE(r.surfaces.italic);
  CHECK(r.surfaces.face_status == GRACEUI_TOO_LATE);
  CHECK(g.faces_too_late == 3);
  g.display();
  (void)o->step(status(), nullptr, nullptr, 1000);
  proto::UiModel m;
  m.hint = proto::TalkHint{proto::HintVerb::Speak, "Lantern-Keeper", proto::ChrHandle{0x500}};
  CHECK(o->show(m, 1000).empty());
  CHECK(g.refused_lists == 0);
  CHECK(g.panels.size() == 3);
  bool shown = false;
  for (const auto& [id, p] : g.panels) shown = shown || (p.shown && p.lists == 1);
  CHECK(shown);  // the hint, drawn from face 1's glyphs
}

TEST_CASE("chat: the italic face read for each show where GraceUI's folder has it; none registered below 0.2.36") {
  // With it (GraceUI 0.2.36, D241): its glyph table read with the serif's for each show that draws, so that the box
  // draws the Tarnished's lines with it (surfaces.hpp's Faces::tarnished; test_surfaces.cpp checks the quads).
  constexpr std::uint32_t kItalicFace = 18;  // the fake's ids: the mod's faces from 16, in registration order
  {
    Session s;
    s.open();
    CHECK(s.g.glyph_tables_of[kItalicFace] >= 1);
    CHECK(s.g.refused_lists == 0);
  }
  // Without the file (an older GraceUI): two faces, no third asked for, and the box drawn all the same.
  fake::Grace g;
  Report r;
  auto o = connect(g, options(true, false), r);
  REQUIRE(o != nullptr);
  CHECK(std::string(r.surfaces.state) == "ready");
  CHECK(g.faces.size() == 2);
  CHECK(r.surfaces.faces == 2);
  CHECK_FALSE(r.surfaces.italic);
  g.warm_up();
  (void)o->step(status(), nullptr, nullptr, 10'000);
  proto::UiModel m = open_box();
  m.chat.history = {{proto::HistoryLine::Who::Tarnished, "", "Good evening.", false}};
  CHECK(o->show(m, 10'000).empty());
  CHECK(g.holding);
  CHECK(g.refused_lists == 0);
  CHECK(g.glyph_tables_of.count(kItalicFace) == 0);
}

TEST_CASE("chat: older GraceUIs: no chord without the cap, and no surfaces without the holder's events") {
  fake::Grace g;
  g.caps = {"notices", "panels", "textures", "faces", "input", "world-state", "hotkeys", "holder-events", "display"};
  Report r;
  auto o = connect(g, options(), r);
  REQUIRE(o->surfaces() != nullptr);
  CHECK(std::string(r.surfaces.chord) == "no_cap");
  CHECK(g.hotkeys[0].chord == 0);
  CHECK_FALSE(r.surfaces.wake);
  CHECK(o->wake() == nullptr);
  CHECK(o->surfaces()->key_words() == "T");  // no describe_hotkey: the default
  fake::Grace h;
  h.caps = {"notices", "panels", "textures", "faces", "input", "world-state", "hotkeys", "display"};
  auto p = connect(h, options(), r);
  CHECK(p->surfaces() == nullptr);
  CHECK(std::string(r.surfaces.state) == "no_caps");
}

TEST_CASE("chat: the talk key opens the box: its panel shown, then input asked for with the holder's events") {
  Session s;
  s.g.press("talk");
  const auto ev = s.poll();
  REQUIRE(ev.size() == 1);
  CHECK(std::get<proto::UiEvent::OpenChat>(ev[0].event).via == proto::OpenVia::TalkKey);
  CHECK(s.show(open_box()).empty());
  REQUIRE(s.g.holding);
  const fake::Panel* box = s.g.holder();
  REQUIRE(box != nullptr);
  CHECK(box->shown);
  CHECK(box->lists == 1);  // its list before it showed
  CHECK((box->desc.flags & GRACEUI_PANEL_TAKES_INPUT) != 0);
  CHECK(s.g.last_request.kind == GRACEUI_HOLDER_PANEL);
  CHECK(s.g.last_request.flags == GRACEUI_INPUT_EVENTS);
  CHECK(s.g.last_request.wants == (GRACEUI_WANT_KEYBOARD | GRACEUI_WANT_PAD));  // not the mouse (O113)
  CHECK(s.g.refused_lists == 0);
  // The open's times, for the latency the log reports.
  const auto opened = s.o->surfaces()->take_opened();
  REQUIRE(opened);
  CHECK(opened->pressed == s.now);
  CHECK(opened->submitted == s.now);
  // The caret set on the box's panel for the IME.
  REQUIRE_FALSE(s.g.carets.empty());
  CHECK(s.g.carets.back().panel == s.g.holder_panel);
  CHECK(s.g.carets.back().height > 0);
  // Hotkeys are silent while the box holds input: T types.
  s.g.press("talk");
  CHECK(s.poll().empty());
}

TEST_CASE("chat: typing: the talk key's character swallowed, the caret moves, Enter sends and empties the line") {
  Session s;
  s.open();
  const std::size_t carets = s.g.carets.size();
  s.g.type("t");  // the T that opened the box, as its WM_CHAR follows
  s.now += 20;
  CHECK(s.poll().empty());
  CHECK(s.o->surfaces()->counts().swallowed == 1);
  s.g.type("Hello there");
  s.now += 30;
  CHECK(s.poll().empty());
  s.now += 100;
  CHECK(s.show(open_box()).empty());
  CHECK(s.g.carets.size() > carets);  // the caret moved with the text
  CHECK(s.g.carets.back().x > s.g.carets[carets - 1].x);
  // Backspace twice, by key, through the box's own Dear ImGui.
  s.g.key(0x08, true);
  s.g.key(0x08, false);
  s.g.key(0x08, true);
  s.g.key(0x08, false);
  s.g.key(0x0D, true);
  s.now += 40;
  const auto ev = s.poll();
  auto sent = find<proto::UiEvent::Submit>(ev);
  REQUIRE(sent);
  CHECK(sent->text == "Hello the");
  // The line emptied: Enter again sends nothing.
  s.g.key(0x0D, false);
  s.g.key(0x0D, true);
  CHECK(!find<proto::UiEvent::Submit>(s.poll()).has_value());
  // The pad's A sends too.
  s.g.type("Yes");
  s.g.pad(0x1000, true);
  auto pad = find<proto::UiEvent::Submit>(s.poll());
  REQUIRE(pad);
  CHECK(pad->text == "Yes");
}

TEST_CASE("chat: the line keeps NPC §8.5's limit, and the kept draft is typed on, never replaced") {
  Session s;
  s.g.press("talk");
  (void)s.poll();
  CHECK(s.show(open_box(1, "Where does")).empty());
  s.g.type(" it go");
  s.g.key(0x0D, true);
  s.now += 40;
  auto sent = find<proto::UiEvent::Submit>(s.poll());
  REQUIRE(sent);
  CHECK(sent->text == "Where does it go");
  proto::UiModel m = open_box();
  m.chat.max_chars = 5;
  m.chat.conv = 2;  // another conversation in the same box takes its draft
  m.chat.draft = "abc";
  CHECK(s.show(m).empty());
  s.g.type("defgh");
  s.g.key(0x0D, true);
  s.now += 40;
  sent = find<proto::UiEvent::Submit>(s.poll());
  REQUIRE(sent);
  CHECK(sent->text == "abcde");
}

TEST_CASE("chat: Esc closes with its draft and gives input back; nothing of the box's input outlives it") {
  Session s;
  s.open();
  s.g.type("Half a line");
  s.g.key(0x1B, true);
  s.now += 30;
  const auto ev = s.poll();
  auto closed = find<proto::UiEvent::Close>(ev);
  REQUIRE(closed);
  CHECK(closed->reason == proto::CloseReason::Escape);
  CHECK(closed->draft == "Half a line");
  CHECK_FALSE(s.g.holding);
  CHECK(s.g.releases == 1);
  // The core closes the box: it hides, and no Shut follows (it was the player's Esc).
  proto::UiModel m = open_box();
  m.chat.view = proto::BoxView::Closed;
  CHECK(s.show(m).empty());
  for (const auto& [id, p] : s.g.panels) CHECK_FALSE(p.shown);
  // GraceUI's `released` end, and anything late, are passed over; the talk key opens it again.
  CHECK(s.poll().empty());
  s.g.press("talk");
  CHECK(find<proto::UiEvent::OpenChat>(s.poll()).has_value());
  CHECK(s.show(open_box(1, "Half a line")).empty());
  CHECK(s.g.holding);
}

TEST_CASE("chat: danger ends the holder and keeps the draft; the model's Shut keeps it too") {
  Session s;
  s.open();
  s.g.type("Wait for me");
  s.g.end_input(GRACEUI_CLOSED_DANGER);
  s.now += 30;
  auto ev = s.poll();
  auto closed = find<proto::UiEvent::Close>(ev);
  REQUIRE(closed);
  CHECK(closed->reason == proto::CloseReason::InputLost);
  CHECK(closed->draft == "Wait for me");
  // The model still says Open until the core steps: the box asks for nothing again until it reopens.
  const std::uint32_t requests = s.g.requests;
  CHECK(s.show(open_box()).empty());
  CHECK(s.g.requests == requests);
  // Focus lost.
  proto::UiModel closed_model;
  CHECK(s.show(closed_model).empty());
  s.g.press("talk");
  (void)s.poll();
  CHECK(s.show(open_box(1, "Wait for me")).empty());
  s.g.end_input(GRACEUI_CLOSED_FOCUS_LOST);
  closed = find<proto::UiEvent::Close>(s.poll());
  REQUIRE(closed);
  CHECK(closed->reason == proto::CloseReason::FocusLost);
  CHECK(closed->draft == "Wait for me");
  // The model closes the box while it holds input (danger, an ending): Shut, with the draft, and input given back.
  CHECK(s.show(closed_model).empty());
  s.g.press("talk");
  (void)s.poll();
  CHECK(s.show(open_box(1, "Wait")).empty());
  s.g.type(" here");
  (void)s.poll();
  ev = s.show(closed_model);
  closed = find<proto::UiEvent::Close>(ev);
  REQUIRE(closed);
  CHECK(closed->reason == proto::CloseReason::Shut);
  CHECK(closed->draft == "Wait here");
  CHECK_FALSE(s.g.holding);
}

TEST_CASE("chat: refused and not now answered as Close, the holder named; the box never shows") {
  Session s;
  s.g.input_answer = GRACEUI_INPUT_REFUSED;
  s.g.holder_name = "another-mod";
  auto ev = s.show(open_box());
  auto closed = find<proto::UiEvent::Close>(ev);
  REQUIRE(closed);
  CHECK(closed->reason == proto::CloseReason::Refused);
  CHECK(closed->detail == "another-mod");
  CHECK(closed->draft.empty());
  for (const auto& [id, p] : s.g.panels) CHECK_FALSE(p.shown);
  CHECK(s.show(open_box()).empty());  // asked once per open
  CHECK(s.show(proto::UiModel{}).empty());
  s.g.input_answer = GRACEUI_INPUT_NOT_NOW;
  s.g.not_now_why = GRACEUI_NOT_NOW_GAME_MENU;
  closed = find<proto::UiEvent::Close>(s.show(open_box()));
  REQUIRE(closed);
  CHECK(closed->reason == proto::CloseReason::NotNow);
  CHECK(closed->detail == "game_menu");
  // Before warm-up nothing can be drawn: not now, not ready.
  fake::Grace g;
  Report r;
  auto o = connect(g, options(), r);
  closed = find<proto::UiEvent::Close>(o->show(open_box(), 1));
  REQUIRE(closed);
  CHECK(closed->reason == proto::CloseReason::NotNow);
  CHECK(closed->detail == "not_ready");
  CHECK(g.requests == 0);
}

TEST_CASE("chat: a page's PageShown on its first submitted frame; Tab skips it; the fades at ten frames a second") {
  Session s;
  proto::UiModel m;
  m.page = speech(0);
  auto ev = s.show(m);
  auto shown = find<proto::UiEvent::PageShown>(ev);
  REQUIRE(shown);
  CHECK(shown->page == proto::PageId{1, 1, 0});
  CHECK(shown->t_ms == s.now);
  CHECK(s.g.refused_lists == 0);
  const std::uint64_t submits = s.g.submits;
  s.now += 40;
  CHECK(s.show(m).empty());
  CHECK(s.g.submits == submits);  // not again within 100 ms
  s.now += 70;
  CHECK(s.show(m).empty());  // once more for the fade-in, never a second PageShown
  CHECK(s.g.submits == submits + 1);
  s.now += 500;
  CHECK(s.show(m).empty());
  CHECK(s.g.submits == submits + 1);  // still: nothing to change
  // Tab, with no holder: Skip names the page up.
  s.g.press("skip-page");
  ev = s.poll();
  auto skip = find<proto::UiEvent::Skip>(ev);
  REQUIRE(skip);
  CHECK(skip->page == proto::PageId{1, 1, 0});
  // In the box, Tab is a key of the holder's: the same.
  s.g.press("talk");
  (void)s.poll();
  proto::UiModel both = open_box();
  both.page = speech(0);
  CHECK(s.show(both).empty());
  REQUIRE(s.g.holding);
  s.g.key(0x09, true);
  skip = find<proto::UiEvent::Skip>(s.poll());
  REQUIRE(skip);
  // A card is never acknowledged.
  proto::UiModel c;
  c.page = speech(0);
  c.page->id = proto::PageId{2, 1, 0};
  c.page->kind = proto::PageKind::Card;
  s.now += 1000;
  CHECK(!find<proto::UiEvent::PageShown>(s.show(c)).has_value());
}

TEST_CASE("chat: toasts as notices under EldenNet's keys, withdrawn when no longer listed") {
  Session s;
  proto::UiModel m;
  m.toasts = {{4, proto::ToastLevel::Info, "They have nothing to say to you."}};
  (void)s.show(m);
  REQUIRE(s.g.notices.count(3) == 1);
  CHECK(s.g.notices[3] == "They have nothing to say to you.");
  (void)s.show(m);
  CHECK(s.g.notice_calls.size() == 1);
  m.toasts.clear();
  (void)s.show(m);
  CHECK(s.g.notices.empty());
}

TEST_CASE("chat: what pressed the talk hotkey (ABI 1.7): the chord alone opens by the chord, with no swallow") {
  // GraceUI 0.2.38's hotkey_by (D243). The chord: OpenChat by the pad chord, and a T typed at once is the player's.
  const auto typed_after = [](fake::Grace& g, std::unique_ptr<Overlay>& o, std::uint64_t& now, std::uint32_t by,
                              proto::OpenVia& via) {
    g.press("talk", 1, by);
    const auto ev = o->step(status(), nullptr, nullptr, now).ui;
    const auto open = find<proto::UiEvent::OpenChat>(ev);
    REQUIRE(open.has_value());
    via = open->via;
    REQUIRE(o->show(open_box(), now).empty());
    REQUIRE(g.holding);
    g.type("t");  // within 100 ms of the open, as the key's own WM_CHAR would come
    g.key(0x0D, true);
    now += 20;
    const auto sent = find<proto::UiEvent::Submit>(o->step(status(), nullptr, nullptr, now).ui);
    return sent ? sent->text : std::string();
  };
  {
    Session s;
    proto::OpenVia via{};
    CHECK(typed_after(s.g, s.o, s.now, GRACEUI_HOTKEY_BY_CHORD, via) == "t");
    CHECK(via == proto::OpenVia::PadChord);
    CHECK(s.o->surfaces()->counts().swallowed == 0);
  }
  // The key, and the key and the chord both before one poll: by the key, its character swallowed.
  for (const std::uint32_t by : {GRACEUI_HOTKEY_BY_KEY, GRACEUI_HOTKEY_BY_KEY | GRACEUI_HOTKEY_BY_CHORD}) {
    CAPTURE(by);
    Session s;
    proto::OpenVia via{};
    CHECK(typed_after(s.g, s.o, s.now, by, via).empty());
    CHECK(via == proto::OpenVia::TalkKey);
    CHECK(s.o->surfaces()->counts().swallowed == 1);
  }
  // An older GraceUI (no "hotkey-by", nor 0.2.52's "on-present"): 0 whatever pressed it, taken as the key, as before
  // 0.2.38.
  fake::Grace g;
  for (const char* later : {"hotkey-by", "on-present"}) {
    g.caps.erase(std::remove(g.caps.begin(), g.caps.end(), std::string(later)), g.caps.end());
  }
  REQUIRE(std::find(g.caps.begin(), g.caps.end(), "hotkey-by") == g.caps.end());
  Report r;
  auto o = connect(g, options(), r);
  REQUIRE(o != nullptr);
  g.warm_up();
  std::uint64_t now = 10'000;
  (void)o->step(status(), nullptr, nullptr, now);
  proto::OpenVia via{};
  CHECK(typed_after(g, o, now, GRACEUI_HOTKEY_BY_CHORD, via).empty());
  CHECK(via == proto::OpenVia::TalkKey);
  CHECK(o->surfaces()->counts().swallowed == 1);
}

TEST_CASE("chat: the hint's key words from describe_hotkey, again on a change; the swallow follows the new key") {
  Session s;
  proto::UiModel m;
  m.hint = proto::TalkHint{proto::HintVerb::Speak, "Lantern-Keeper", proto::ChrHandle{0x500}};
  (void)s.show(m);
  CHECK(s.o->surfaces()->key_words() == "T");
  s.g.rebind("talk", 0x4B);  // K
  (void)s.poll();
  CHECK(s.o->surfaces()->key_words() == "K");
  const std::uint64_t submits = s.g.submits;
  s.now += 200;
  (void)s.show(m);
  CHECK(s.g.submits == submits + 1);  // the hint drawn again with its new key
  s.open();
  s.g.type("kt");
  s.g.key(0x0D, true);
  auto sent = find<proto::UiEvent::Submit>(s.poll());
  REQUIRE(sent);
  CHECK(sent->text == "t");  // K's own character swallowed, T's typed
}

TEST_CASE("chat: a fault gives input back at once, and the next show reports it") {
  Session s;
  s.open();
  s.g.type("Lost");
  (void)s.poll();
  s.o->fault();
  CHECK_FALSE(s.g.holding);
  auto closed = find<proto::UiEvent::Close>(s.show(open_box()));
  REQUIRE(closed);
  CHECK(closed->reason == proto::CloseReason::Fault);
  CHECK(closed->draft == "Lost");
}

TEST_CASE("chat: the wait handle is set when something waits for the poll, and not otherwise") {
  Session s;
  HANDLE wake = static_cast<HANDLE>(s.o->wake());
  REQUIRE(wake != nullptr);
  (void)WaitForSingleObject(wake, 0);  // take the set it was given with
  CHECK(WaitForSingleObject(wake, 0) == WAIT_TIMEOUT);
  s.g.press("talk");
  CHECK(WaitForSingleObject(wake, 0) == WAIT_OBJECT_0);
  (void)s.poll();
  CHECK(WaitForSingleObject(wake, 0) == WAIT_TIMEOUT);
}
