// The rollback choice's panel, laid out (surfaces.hpp; MEM §9.1, O144; WHY§690): portable and pure.
#include <algorithm>
#include <cmath>

#include "en/overlay/surfaces.hpp"

namespace en::overlay::surfaces {

namespace {

constexpr std::uint32_t kVkTab = 0x09;
constexpr std::uint32_t kVkReturn = 0x0D;
constexpr std::uint32_t kVkLeft = 0x25;
constexpr std::uint32_t kVkUp = 0x26;
constexpr std::uint32_t kVkRight = 0x27;
constexpr std::uint32_t kVkDown = 0x28;
constexpr std::uint32_t kPadUp = 0x0001;  // XINPUT_GAMEPAD_DPAD_UP
constexpr std::uint32_t kPadDown = 0x0002;
constexpr std::uint32_t kPadLeft = 0x0004;
constexpr std::uint32_t kPadRight = 0x0008;
constexpr std::uint32_t kPadA = 0x1000;
constexpr std::size_t kBulletLines = 2;

float lh(float px) noexcept { return px * 1.15f; }

}  // namespace

std::uint32_t tone_colour(proto::ChoiceTone t) noexcept {
  switch (t) {
    case proto::ChoiceTone::Save: return rgba(0x8C, 0xB8, 0xE6, 0xFF);    // the save's side: a cool blue
    case proto::ChoiceTone::Memory: return kNameGold;                     // EldenNet's memory: its gold
    case proto::ChoiceTone::Undone: return rgba(0xE0, 0x8A, 0x72, 0xFF);  // what the cut would undo: a warm red
    case proto::ChoiceTone::Stage: return rgba(0xB8, 0xA0, 0xE8, 0xFF);   // a quest stage undone: a violet
    case proto::ChoiceTone::Note: return kPlayerLine;
  }
  return kPlayerLine;
}

GraceUiPanelDesc choice_panel(const Display& d) noexcept {
  GraceUiPanelDesc p{};
  p.size = sizeof p;
  p.layer = GRACEUI_LAYER_MODAL;
  p.anchor = GRACEUI_ANCHOR_CENTRE;
  p.flags = GRACEUI_PANEL_TAKES_INPUT;
  p.width = std::max(1.0f, std::floor(std::min(0.56f * d.width, 900.0f * d.content_scale)));
  p.height = std::max(1.0f, std::floor(std::min(0.70f * d.height, 640.0f * d.content_scale)));
  p.x = 0;
  p.y = 0;
  return p;
}

void paint_choice(Painter& p, const Faces& f, const Display& d, const proto::ChoicePanel& c, std::size_t focus) {
  const float s = d.content_scale;
  const GraceUiPanelDesc panel = choice_panel(d);
  const float w = panel.width;
  const float h = panel.height;
  const float pad = std::round(22.0f * s);
  const float radius = std::round(kCardRadius * s);
  p.round_rect(0, 0, w, h, radius, kChoiceGround);
  p.round_edge(0, 0, w, h, radius, std::max(1.0f, std::round(s)), kBoxEdge);
  float top = pad;
  const float title_px = kChoiceTitlePx * s;
  const float text_px = kChoiceTextPx * s;
  for (const std::u32string& l : wrap(f.ui, title_px, decode(c.title), w - 2 * pad, 2)) {
    p.text(f.ui, title_px, pad, top, l, kNameGold);
    top += lh(title_px);
  }
  top += 6.0f * s;
  for (const std::u32string& l : wrap(f.ui, text_px, decode(c.lead), w - 2 * pad, 3)) {
    p.text(f.ui, text_px, pad, top, l, kTextInk);
    top += lh(text_px);
  }
  top += 10.0f * s;
  // The answers' row at the bottom; the bullets fill what is left above it, each whole or not at all.
  const float answer_px = kChoiceAnswerPx * s;
  const float button_h = std::round(lh(answer_px) + 16.0f * s);
  const float row_top = h - pad - button_h;
  const float square = std::round(8.0f * s);
  const float indent = std::round(22.0f * s);
  for (const proto::ChoiceBullet& b : c.bullets) {
    const std::vector<std::u32string> lines = wrap(f.text, text_px, decode(b.text), w - 2 * pad - indent, kBulletLines);
    const float need = lh(text_px) * static_cast<float>(lines.size()) + 6.0f * s;
    if (lines.empty() || top + need > row_top - 12.0f * s) break;
    const float sq_top = std::round(top + (lh(text_px) - square) / 2.0f);
    p.rect(pad, sq_top, pad + square, sq_top + square, tone_colour(b.tone));
    for (const std::u32string& l : lines) {
      p.text(f.text, text_px, pad + indent, top, l, kTextInk);
      top += lh(text_px);
    }
    top += 6.0f * s;
  }
  const std::size_t n = c.answers.size();
  if (n == 0) return;
  const float gap = std::round(12.0f * s);
  const float button_w = std::floor((w - 2 * pad - gap * static_cast<float>(n - 1)) / static_cast<float>(n));
  const float ring = std::max(1.0f, std::round(2.0f * s));
  for (std::size_t i = 0; i < n; ++i) {
    const float x0 = pad + static_cast<float>(i) * (button_w + gap);
    const bool on = i == focus;
    p.round_rect(x0, row_top, x0 + button_w, row_top + button_h, radius, on ? kChoiceFocus : kBoxGround);
    p.round_edge(x0, row_top, x0 + button_w, row_top + button_h, radius, on ? ring : std::max(1.0f, std::round(s)),
                 on ? kNameGold : kBoxEdge);
    const std::vector<std::u32string> one = wrap(f.ui, answer_px, decode(c.answers[i]), button_w - 12.0f * s, 1);
    if (one.empty()) continue;
    const float tw = measure(f.ui, answer_px, one.front());
    p.text(f.ui, answer_px, std::round(x0 + (button_w - tw) / 2.0f), std::round(row_top + 8.0f * s), one.front(),
           on ? kNameGold : kTextInk);
  }
}

ChoiceInput::Mapped ChoiceInput::map(const GraceUiEvent& e) noexcept {
  Mapped m;
  switch (e.kind) {
    case GRACEUI_EVENT_KEY:
      if (e.pressed == 0) return m;
      if (e.key == kVkLeft || e.key == kVkUp) m.action = Action::Previous;
      if (e.key == kVkRight || e.key == kVkDown || e.key == kVkTab) m.action = Action::Next;
      if (e.key == kVkReturn && e.repeat == 0) m.action = Action::Answer;
      return m;
    case GRACEUI_EVENT_PAD:
      if (e.pressed == 0) return m;
      if (e.button == kPadLeft || e.button == kPadUp) m.action = Action::Previous;
      if (e.button == kPadRight || e.button == kPadDown) m.action = Action::Next;
      if (e.button == kPadA) m.action = Action::Answer;
      return m;
    case GRACEUI_EVENT_INPUT_ENDED:
      m.action = Action::Ended;
      switch (e.why) {
        case GRACEUI_CLOSED_RELEASED: break;
        case GRACEUI_CLOSED_FOCUS_LOST: m.reason = proto::CloseReason::FocusLost; break;
        case GRACEUI_CLOSED_FAULT: m.reason = proto::CloseReason::Fault; break;
        default: m.reason = proto::CloseReason::InputLost; break;  // danger, loading, the player's exit, no render
      }
      return m;
    default:
      return m;  // characters and the pointer: the panel takes neither
  }
}

}  // namespace en::overlay::surfaces
