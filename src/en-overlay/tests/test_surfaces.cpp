// The conversation's surfaces (surfaces.hpp), in every build: the model to draw lists in GraceUI's form, the glyph
// layout from an invented glyph table, the fades and PageShown, the toasts' notice keys and the box's input mapping.
#include <doctest.h>

#include <string>
#include <vector>

#include "en/overlay/surfaces.hpp"

using namespace en::overlay::surfaces;
namespace proto = en::proto;

namespace {

constexpr std::uint64_t kAtlas = 0xA71A5;
constexpr std::uint64_t kWhite = 0x7E01;

// An invented face: every printable ASCII glyph and a few more, 10 px wide at a baked 20 px, its quad 1 to 9 across
// and 2 to 18 down from the pen; the space has no quad.
struct FakeFace {
  std::vector<GraceUiGlyph> glyphs;
  GraceUiGlyphTable table{};
  explicit FakeFace(bool with_fallback = true, std::uint32_t generation = 1) {
    std::vector<char32_t> cps;
    for (char32_t c = 0x20; c < 0x7F; ++c) cps.push_back(c);
    for (const char32_t c : {U'É', U'é', U'—', U'…'}) cps.push_back(c);
    if (with_fallback) cps.push_back(0xFFFD);
    for (const char32_t c : cps) {
      GraceUiGlyph g{};
      g.code_point = c;
      g.advance = 10.0f;
      if (c != U' ') {
        g.x0 = 1;
        g.y0 = 2;
        g.x1 = 9;
        g.y1 = 18;
        g.u0 = 0.1f;
        g.v0 = 0.2f;
        g.u1 = 0.3f;
        g.v1 = 0.4f;
      }
      glyphs.push_back(g);
    }
    table.size = sizeof table;
    table.generation = generation;
    table.face_id = 16;
    table.glyph_count = static_cast<std::uint32_t>(glyphs.size());
    table.pixel_height = 20.0f;
    table.ascent = 16.0f;
    table.descent = -4.0f;
    table.atlas = kAtlas;
    table.glyphs = glyphs.data();
  }
  Faces faces() const { return Faces{Face::of(table), Face::of(table)}; }
};

const Display kHd{1920, 1080, 1.0f, 1.0f};

// GraceUI's checks on a list (graceui.h's submit, D76), as the fake GraceUI makes them.
bool valid(const Painter& p, float w, float h, std::string& why) {
  const GraceUiDrawList l = p.view();
  if (l.command_count == 0 || l.command_count > GRACEUI_DRAW_COMMANDS_MAX) return why = "commands", false;
  if (std::uint64_t{l.vertex_count} * sizeof(GraceUiDrawVert) > GRACEUI_DRAW_VERTEX_BYTES_MAX) {
    return why = "vertices", false;
  }
  for (std::uint32_t i = 0; i < l.command_count; ++i) {
    const GraceUiDrawCommand& c = l.commands[i];
    if (c.vertex_count > 65536 || std::uint64_t{c.vertex_offset} + c.vertex_count > l.vertex_count) {
      return why = "vertex_range", false;
    }
    if (std::uint64_t{c.index_offset} + c.index_count > l.index_count || c.index_count % 3 != 0) {
      return why = "index_range", false;
    }
    for (std::uint32_t k = 0; k < c.index_count; ++k) {
      if (l.indices[c.index_offset + k] >= c.vertex_count) return why = "index", false;
    }
    if (!(c.clip_x0 >= 0 && c.clip_y0 >= 0 && c.clip_x1 <= w && c.clip_y1 <= h && c.clip_x0 <= c.clip_x1 &&
          c.clip_y0 <= c.clip_y1)) {
      return why = "clip", false;
    }
    if (c.texture != kAtlas && c.texture != kWhite) return why = "texture", false;
  }
  return true;
}

proto::PageView speech(std::uint8_t index, std::string text, bool last = false) {
  proto::PageView p;
  p.id = proto::PageId{3, 1, index};
  p.kind = proto::PageKind::Speech;
  p.speaker = "Lantern-Keeper";  // the invented test persona's name, as a fake game would show it
  p.text = std::move(text);
  p.count = static_cast<std::uint8_t>(index + 1);
  p.last = last;
  p.hold_ms = 3000;
  return p;
}

GraceUiEvent key(std::uint32_t vk, bool down, bool repeat = false) {
  GraceUiEvent e{};
  e.size = sizeof e;
  e.kind = GRACEUI_EVENT_KEY;
  e.key = vk;
  e.pressed = down ? 1 : 0;
  e.repeat = repeat ? 1 : 0;
  return e;
}

GraceUiEvent character(char32_t c) {
  GraceUiEvent e{};
  e.size = sizeof e;
  e.kind = GRACEUI_EVENT_CHARACTER;
  e.code_point = c;
  return e;
}

GraceUiEvent ended(std::uint32_t why) {
  GraceUiEvent e{};
  e.size = sizeof e;
  e.kind = GRACEUI_EVENT_INPUT_ENDED;
  e.why = why;
  return e;
}

}  // namespace

TEST_CASE("surfaces: the panels sit where NPC §10.1 puts them, within the display") {
  const GraceUiPanelDesc page = page_panel(kHd);
  CHECK(page.anchor == GRACEUI_ANCHOR_BOTTOM);
  CHECK(page.layer == GRACEUI_LAYER_HUD);
  CHECK(page.y == doctest::Approx(216));  // the block's bottom at 0.80 H
  CHECK(page.width <= 0.70f * 1920);
  const GraceUiPanelDesc box = box_panel(kHd);
  CHECK(box.layer == GRACEUI_LAYER_PANEL);
  CHECK((box.flags & GRACEUI_PANEL_TAKES_INPUT) != 0);
  CHECK(box.width == doctest::Approx(768));  // 0.40 W
  CHECK(box.y + box.height == doctest::Approx(0.18f * 1080).epsilon(0.01));  // top 0.82 H, bottom 0.97 H
  const GraceUiPanelDesc hint = hint_panel(kHd);
  CHECK(hint.anchor == GRACEUI_ANCHOR_TOP);
  CHECK(hint.y + 26.0f == doctest::Approx(0.68f * 1080).epsilon(0.002));
  // At 4K and a larger Menu size, the same fractions, the heights by the content scale.
  const Display uhd{3840, 2160, 2.4f, 2.4f};
  CHECK(box_panel(uhd).width == doctest::Approx(1536));
  CHECK(page_panel(uhd).height > page_panel(kHd).height * 2.3f);
}

TEST_CASE("surfaces: text, decoded, upper-cased and wrapped") {
  CHECK(decode("Kal\xC3\xA9") == U"Kalé");
  CHECK(decode("a\xFF" "b") == U"a�b");                 // a byte that starts nothing
  CHECK(decode("\xE2\x80") == U"��");              // a sequence cut short
  CHECK(decode("\xC0\xAF") == U"��");              // an overlong form
  CHECK(decode("\xED\xA0\x80") == U"���");    // a surrogate
  CHECK(upper("Kal\xC3\xA9 the \xC4\x81") == "KAL\xC3\x89 THE \xC4\x80");
  CHECK(chars("Kal\xC3\xA9") == 4);
  const FakeFace ff;
  const Face f = Face::of(ff.table);
  CHECK(measure(f, 20, U"abc") == doctest::Approx(30));
  CHECK(measure(f, 40, U"abc") == doctest::Approx(60));              // scaled from the baked size
  CHECK(measure(f, 20, U"ab", 0.08f) == doctest::Approx(20 + 2 * 1.6f));  // tracking per glyph
  REQUIRE(f.find(U'一') != nullptr);
  CHECK(f.find(U'一')->code_point == 0xFFFD);  // a glyph the face lacks draws U+FFFD
  const FakeFace bare(false);
  CHECK(Face::of(bare.table).find(U'一')->code_point == U'?');
  // 10 px a glyph at 20 px: a 100 px line holds ten.
  const auto lines = wrap(f, 20, U"the road runs on past the lamp", 100, 3);
  REQUIRE(lines.size() == 3);
  CHECK(lines[0] == U"the road");
  CHECK(lines[1] == U"runs on");
  CHECK(lines[2] == U"past the…");  // "lamp" does not fit: the last line ends in "…"
  CHECK(wrap(f, 20, U"abcdefghijklmnop", 100, 2) == std::vector<std::u32string>{U"abcdefghij", U"klmnop"});
  CHECK(wrap(f, 20, U"", 100, 2).empty());
}

TEST_CASE("surfaces: a page's draw list, in GraceUI's form, from the glyph table") {
  const FakeFace ff;
  const Faces faces = ff.faces();
  const GraceUiPanelDesc panel = page_panel(kHd);
  Painter p;
  p.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  paint_page(p, faces, kHd, speech(0, "The road runs on past the lamp."), 1.0f);
  std::string why;
  CHECK_MESSAGE(valid(p, panel.width, panel.height, why), why);
  // The name in upper case and the text, each with its shadow: two quads a glyph that has ink.
  const std::size_t ink = std::string("LANTERN-KEEPER").size() + std::string("Theroadrunsonpastthelamp.").size();
  CHECK(p.glyphs() == 2 * ink);
  CHECK(p.view().atlas_generation == 1);
  for (const GraceUiDrawCommand& c : p.commands()) CHECK(c.texture == kAtlas);
  // At no opacity nothing is drawn; at half, every vertex's alpha is halved.
  Painter none;
  none.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  paint_page(none, faces, kHd, speech(0, "Hm."), 0.0f);
  CHECK(none.empty());
  Painter half;
  half.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  paint_page(half, faces, kHd, speech(0, "Hm."), 0.5f);
  CHECK((half.vertices().back().colour >> 24) == 0x80);
  // A card: the name and "…".
  proto::PageView card = speech(0, "");
  card.kind = proto::PageKind::Card;
  Painter c;
  c.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  paint_page(c, faces, kHd, card, 1.0f);
  CHECK(c.glyphs() == 2 * (std::string("LANTERN-KEEPER").size() + 1));
  // No usable face: nothing at all (the surface waits for GraceUI's warm-up).
  Painter blank;
  blank.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  paint_page(blank, Faces{}, kHd, speech(0, "Hm."), 1.0f);
  CHECK(blank.empty());
}

TEST_CASE("surfaces: the box: header, history, input, selection and the caret, scrolled to show it") {
  const FakeFace ff;
  const Faces faces = ff.faces();
  const GraceUiPanelDesc panel = box_panel(kHd);
  proto::ChatBox box;
  box.view = proto::BoxView::Open;
  box.conv = 3;
  box.state = proto::ConvState::Open;
  box.partner = "Lantern-Keeper";
  box.heard_by = {"Lantern-Keeper"};
  box.history = {{proto::HistoryLine::Who::Tarnished, "", "Good evening.", false},
                 {proto::HistoryLine::Who::Npc, "Lantern-Keeper", "Hm. Evening.", false},
                 {proto::HistoryLine::Who::Tarnished, "", "And the lamp?", true}};
  const std::string text = "Where does it go";
  EditView edit{text, text.size(), 6, 10};
  float scroll = 0;
  Painter p;
  p.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  const Caret caret = paint_box(p, faces, kHd, box, edit, nullptr, 0, scroll);
  std::string why;
  CHECK_MESSAGE(valid(p, panel.width, panel.height, why), why);
  CHECK(scroll == 0);  // the line fits
  CHECK(caret.height > 0);
  // The caret after the last glyph: the field's left, its inner pad, then 16 glyphs of 24 px (12 px each).
  CHECK(caret.x == doctest::Approx(10 + 6 + 16 * 12).epsilon(0.01));
  bool white = false;
  bool atlas = false;
  for (const GraceUiDrawCommand& c : p.commands()) {
    white = white || c.texture == kWhite;
    atlas = atlas || c.texture == kAtlas;
  }
  CHECK(white);
  CHECK(atlas);
  // A line longer than the field scrolls so the caret stays inside; Home scrolls back.
  const std::string longer(120, 'x');
  EditView end{longer, longer.size(), longer.size(), longer.size()};
  Painter q;
  q.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  const Caret far = paint_box(q, faces, kHd, box, end, nullptr, 0, scroll);
  CHECK(scroll > 0);
  CHECK(far.x <= panel.width - 10);
  CHECK(far.x >= 10);
  CHECK_MESSAGE(valid(q, panel.width, panel.height, why), why);
  EditView home{longer, 0, 0, 0};
  Painter r;
  r.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  const Caret back = paint_box(r, faces, kHd, box, home, nullptr, 0, scroll);
  CHECK(scroll == 0);
  CHECK(back.x == doctest::Approx(16));
  // The strip alone (subtitles off, the box hidden by danger): no input, no caret; a strip page joins its lines.
  box.view = proto::BoxView::Strip;
  proto::PageView strip = speech(0, "Hm.");
  strip.surface = proto::Surface::HistoryStrip;
  Painter st;
  st.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  const Caret none = paint_box(st, faces, kHd, box, edit, &strip, 1.0f, scroll);
  CHECK(none.height == 0);
  CHECK_MESSAGE(valid(st, panel.width, panel.height, why), why);
  // Every history line is far more than the strip holds: still within GraceUI's bounds.
  box.view = proto::BoxView::Open;
  box.history.assign(40, proto::HistoryLine{proto::HistoryLine::Who::Npc, "Lantern-Keeper", std::string(230, 'w'),
                                            false});
  Painter big;
  big.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  (void)paint_box(big, faces, kHd, box, edit, nullptr, 0, scroll);
  CHECK_MESSAGE(valid(big, panel.width, panel.height, why), why);
}

TEST_CASE("surfaces: the hint, its key boxed, and the marker") {
  const FakeFace ff;
  const GraceUiPanelDesc panel = hint_panel(kHd);
  proto::TalkHint hint{proto::HintVerb::Speak, "Lantern-Keeper", proto::ChrHandle{0x500}};
  Painter p;
  p.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  paint_hint(p, ff.faces(), kHd, hint, "T");
  std::string why;
  CHECK_MESSAGE(valid(p, panel.width, panel.height, why), why);
  // "T", "Speak", the dash and the name: their glyphs with ink.
  CHECK(p.glyphs() == 1 + 5 + 1 + std::string("Lantern-Keeper").size());
  hint.verb = proto::HintVerb::Next;
  Painter n;
  n.reset(panel.width, panel.height, kWhite, 0.5f, 0.5f);
  paint_hint(n, ff.faces(), kHd, hint, "T");
  CHECK(n.glyphs() == std::string("SpeaktoLantern-Keepernext").size());  // no key for "Speak to … next"
}

TEST_CASE("surfaces: the lower third's fades, and PageShown on the first submitted frame") {
  LowerThird lt;
  CHECK(lt.empty(0));
  lt.update(speech(0, "Hm."), 1000);
  auto f = lt.frame(1000);
  REQUIRE(f.size() == 1);
  CHECK(f[0].alpha > 0);  // the first frame is never transparent
  CHECK(f[0].alpha < 1);
  CHECK(lt.moving(1000));
  const auto shown = lt.submitted(1040);
  REQUIRE(shown);
  CHECK(shown->page == proto::PageId{3, 1, 0});
  CHECK(shown->t_ms == 1040);
  CHECK_FALSE(lt.submitted(1140));  // once
  CHECK(lt.frame(1200)[0].alpha == doctest::Approx(1.0f));
  CHECK_FALSE(lt.moving(1400));
  // The same page again (its count grown) changes nothing; the next cross-fades.
  proto::PageView again = speech(0, "Hm.");
  again.count = 2;
  lt.update(again, 1500);
  CHECK_FALSE(lt.submitted(1500));
  lt.update(speech(1, "The road runs on.", true), 2000);
  f = lt.frame(2000);
  REQUIRE(f.size() == 2);
  CHECK(f[0].page.id.index == 0);
  CHECK(f[1].page.id.index == 1);
  CHECK(lt.frame(2000 + kCrossFadeMs).size() == 1);
  const auto second = lt.submitted(2050);
  REQUIRE(second);
  CHECK(second->page.index == 1);
  // The model drops the last page: it fades out over 350 ms.
  lt.update(std::nullopt, 5000);
  CHECK(lt.frame(5000).size() == 1);
  CHECK(lt.frame(5000 + kFadeOutMs / 2)[0].alpha < 0.6f);
  CHECK(lt.frame(5000 + kFadeOutMs).empty());
  CHECK(lt.empty(5000 + kFadeOutMs));
  // A page the model drops before any frame showed it is never acknowledged, and never fades.
  LowerThird gone;
  gone.update(speech(0, "Hm."), 0);
  gone.update(std::nullopt, 10);
  CHECK(gone.empty(10));
  CHECK_FALSE(gone.submitted(20));
  // A card and the "…" page are never acknowledged; the card pulses.
  LowerThird cards;
  proto::PageView card = speech(0, "");
  card.kind = proto::PageKind::Card;
  cards.update(card, 0);
  CHECK_FALSE(cards.submitted(0));
  CHECK(cards.moving(5000));
  CHECK(cards.frame(0)[0].alpha != doctest::Approx(cards.frame(kPulseMs / 2)[0].alpha));
  // The card's own id (the reply's first page) then the page itself: the page is acknowledged.
  cards.update(speech(0, "Hm."), 800);
  CHECK(cards.submitted(800));
}

TEST_CASE("surfaces: toasts as notices, each id posted once and let go when no longer listed") {
  ToastBook book;
  std::vector<proto::Toast> toasts = {{7, proto::ToastLevel::Info, "They have nothing to say to you."}};
  auto calls = book.update(toasts);
  REQUIRE(calls.size() == 1);
  CHECK(calls[0].key == kToastKeys[0]);
  CHECK(calls[0].text == "They have nothing to say to you.");
  CHECK(calls[0].tone == GRACEUI_TONE_NORMAL);
  CHECK(book.update(toasts).empty());  // listed again: nothing posted again
  toasts.push_back({8, proto::ToastLevel::Error, "No answer: the provider failed"});
  toasts.push_back({9, proto::ToastLevel::Warning, "A third, waiting"});
  calls = book.update(toasts);
  REQUIRE(calls.size() == 1);  // the third finds no key and waits
  CHECK(calls[0].key == kToastKeys[1]);
  CHECK(calls[0].tone == GRACEUI_TONE_DANGER);
  CHECK(book.showing() == 2);
  toasts.erase(toasts.begin());  // 7 goes: withdrawn first, then 9 takes its key
  calls = book.update(toasts);
  REQUIRE(calls.size() == 2);
  CHECK(calls[0].key == kToastKeys[0]);
  CHECK(calls[0].text.empty());
  CHECK(calls[1].key == kToastKeys[0]);
  CHECK(calls[1].tone == GRACEUI_TONE_WARNING);
  // An id let go is never posted again, even if listed again.
  toasts.insert(toasts.begin(), proto::Toast{7, proto::ToastLevel::Info, "They have nothing to say to you."});
  calls = book.update(toasts);
  CHECK(calls.empty());
  calls = book.update({});
  CHECK(calls.size() == 2);
  CHECK(book.showing() == 0);
}

TEST_CASE("surfaces: the box's input: Enter, Esc, Tab, the pad, the holder's end and the swallowed character") {
  using A = InputMap::Action;
  InputMap m;
  m.open(1000, 0x54);  // T
  CHECK(key_char(0x54) == U't');
  CHECK(key_char(0x70) == 0);  // F1 types nothing
  // The T's own character, within 100 ms: swallowed; the next is typed.
  auto r = m.map(character(U't'), 1030);
  CHECK(r.swallowed);
  CHECK(r.action == A::None);
  CHECK(m.map(character(U't'), 1040).action == A::Edit);
  // Only the first character is looked at, and only within 100 ms; Shift's 'T' is the key's too.
  m.open(2000, 0x54);
  CHECK(m.map(character(U'h'), 2010).action == A::Edit);
  CHECK_FALSE(m.map(character(U't'), 2020).swallowed);
  m.open(3000, 0x54);
  CHECK_FALSE(m.map(character(U't'), 3101).swallowed);
  m.open(4000, 0x54);
  CHECK(m.map(character(U'T'), 4050).swallowed);
  m.open(5000, 0);  // the pad's chord: nothing to swallow
  CHECK(m.map(character(U't'), 5010).action == A::Edit);
  // The keys the box takes itself, on a fresh press only; the rest go to its editor.
  CHECK(m.map(key(0x0D, true), 0).action == A::Submit);
  CHECK(m.map(key(0x0D, true, true), 0).action == A::None);
  CHECK(m.map(key(0x0D, false), 0).action == A::None);
  CHECK(m.map(key(0x1B, true), 0).action == A::Escape);
  CHECK(m.map(key(0x09, true), 0).action == A::Skip);
  CHECK(m.map(key(0x25, true), 0).action == A::Edit);  // Left
  CHECK(m.map(key(0x08, true, true), 0).action == A::Edit);  // Backspace's repeat
  CHECK(m.map(character(U'\r'), 0).action == A::None);
  CHECK(m.map(character(U'\b'), 0).action == A::None);
  GraceUiEvent pad{};
  pad.size = sizeof pad;
  pad.kind = GRACEUI_EVENT_PAD;
  pad.button = 0x2000;  // B
  pad.pressed = 1;
  CHECK(m.map(pad, 0).action == A::Escape);
  pad.button = 0x1000;  // A
  CHECK(m.map(pad, 0).action == A::Submit);
  pad.button = 0x0001;
  CHECK(m.map(pad, 0).action == A::Edit);
  GraceUiEvent pointer{};
  pointer.size = sizeof pointer;
  pointer.kind = GRACEUI_EVENT_POINTER;
  CHECK(m.map(pointer, 0).action == A::None);
  // The holder's end, by GraceUI's reason (D176): the box's own release has none.
  CHECK(m.map(ended(GRACEUI_CLOSED_RELEASED), 0).action == A::Ended);
  CHECK_FALSE(m.map(ended(GRACEUI_CLOSED_RELEASED), 0).reason);
  CHECK(m.map(ended(GRACEUI_CLOSED_FOCUS_LOST), 0).reason == proto::CloseReason::FocusLost);
  CHECK(m.map(ended(GRACEUI_CLOSED_FAULT), 0).reason == proto::CloseReason::Fault);
  for (const std::uint32_t why : {GRACEUI_CLOSED_MENU_KEY, GRACEUI_CLOSED_DANGER, GRACEUI_CLOSED_LOADING,
                                  GRACEUI_CLOSED_NO_RENDER, GRACEUI_CLOSED_PLAYER}) {
    CHECK(m.map(ended(why), 0).reason == proto::CloseReason::InputLost);
  }
}
