// The conversation's surfaces, laid out (surfaces.hpp): portable and pure.
#include "en/overlay/surfaces.hpp"

#include <algorithm>
#include <cmath>

namespace en::overlay::surfaces {

namespace {

constexpr std::uint32_t kVkTab = 0x09;
constexpr std::uint32_t kVkReturn = 0x0D;
constexpr std::uint32_t kVkEscape = 0x1B;
constexpr std::uint32_t kVkSpace = 0x20;
constexpr std::uint32_t kPadA = 0x1000;  // XINPUT_GAMEPAD_A: send
constexpr std::uint32_t kPadB = 0x2000;  // XINPUT_GAMEPAD_B: cancel, as Esc (GraceUI §31 item 3)
constexpr char32_t kEllipsis = U'…';
constexpr char32_t kDash = U'—';
constexpr std::size_t kDoneKept = 64;
constexpr std::size_t kHistoryShown = 6;  // the history strip's newest lines, before the room left cuts them

std::uint32_t tone_of(proto::ToastLevel l) noexcept {
  switch (l) {
    case proto::ToastLevel::Info: return GRACEUI_TONE_NORMAL;
    case proto::ToastLevel::Warning: return GRACEUI_TONE_WARNING;
    case proto::ToastLevel::Error: return GRACEUI_TONE_DANGER;
  }
  return GRACEUI_TONE_NORMAL;
}

void append(std::u32string& out, std::string_view utf8) { out += decode(utf8); }

std::u32string u32(std::string_view utf8) { return decode(utf8); }

void put_utf8(std::string& out, char32_t c) {
  if (c < 0x80) {
    out += static_cast<char>(c);
  } else if (c < 0x800) {
    out += static_cast<char>(0xC0 | (c >> 6));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    out += static_cast<char>(0xE0 | (c >> 12));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (c >> 18));
    out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  }
}

char32_t upper_of(char32_t c) noexcept {
  if (c >= U'a' && c <= U'z') return c - 0x20;
  if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 0x20;  // Latin-1's small letters
  if (c == 0xFF) return 0x178;
  // Latin Extended-A: pairs of capital and small, the small one odd from U+0100 to U+0137 and from U+014A to U+0177,
  // even from U+0139 to U+0148 and from U+0179 to U+017E. U+0131 (dotless i) and U+0138 (kra) have no capital here.
  if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177)) return (c & 1u) != 0 && c != 0x131 ? c - 1 : c;
  if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c & 1u) == 0 ? c - 1 : c;
  return c;
}

float lh(float px) noexcept { return px * 1.15f; }  // a line's height: the face's size, and a little air

}  // namespace

std::uint32_t faded(std::uint32_t colour, float alpha) noexcept {
  const float a = std::clamp(alpha, 0.0f, 1.0f) * static_cast<float>(colour >> 24);
  return (colour & 0x00FFFFFFu) | (static_cast<std::uint32_t>(std::lround(a)) << 24);
}

// ---- The panels ----

GraceUiPanelDesc page_panel(const Display& d) noexcept {
  const float s = d.content_scale;
  GraceUiPanelDesc p{};
  p.size = sizeof p;
  p.layer = GRACEUI_LAYER_HUD;
  p.anchor = GRACEUI_ANCHOR_BOTTOM;
  p.width = std::max(1.0f, std::floor(0.70f * d.width));
  p.height = std::max(1.0f, std::ceil(s * (lh(kNamePx) + 3.0f * lh(kPagePx) + 12.0f)));
  p.x = 0;
  p.y = std::floor(0.20f * d.height);  // the block's bottom at 0.80 H, above the native subtitle band
  return p;
}

GraceUiPanelDesc box_panel(const Display& d) noexcept {
  GraceUiPanelDesc p{};
  p.size = sizeof p;
  p.layer = GRACEUI_LAYER_PANEL;
  p.anchor = GRACEUI_ANCHOR_BOTTOM;
  p.flags = GRACEUI_PANEL_TAKES_INPUT;
  p.width = std::max(1.0f, std::floor(0.40f * d.width));
  p.height = std::max(1.0f, std::floor(0.15f * d.height));  // its top at 0.82 H, its bottom at 0.97 H
  p.x = 0;
  p.y = std::floor(0.03f * d.height);
  return p;
}

GraceUiPanelDesc hint_panel(const Display& d) noexcept {
  const float s = d.content_scale;
  GraceUiPanelDesc p{};
  p.size = sizeof p;
  p.layer = GRACEUI_LAYER_HUD;
  p.anchor = GRACEUI_ANCHOR_TOP;
  p.width = std::max(1.0f, std::floor(0.40f * d.width));
  p.height = std::max(1.0f, std::ceil(s * (26.0f + lh(kHintPx) + 8.0f)));
  p.x = 0;
  p.y = std::max(0.0f, std::floor(0.68f * d.height - 26.0f * s));  // the hint's line at 0.68 H, the marker above it
  return p;
}

// ---- Faces ----

Face Face::of(const GraceUiGlyphTable& t) noexcept {
  Face f;
  f.id = t.face_id;
  f.generation = t.generation;
  f.atlas = t.atlas;
  f.pixel_height = t.pixel_height;
  f.ascent = t.ascent;
  f.descent = t.descent;
  if (t.glyphs != nullptr && t.glyph_count != 0) f.glyphs = std::span<const GraceUiGlyph>(t.glyphs, t.glyph_count);
  return f;
}

const GraceUiGlyph* Face::find(char32_t cp) const noexcept {
  const auto at = [&](char32_t c) -> const GraceUiGlyph* {
    const auto it = std::lower_bound(glyphs.begin(), glyphs.end(), c,
                                     [](const GraceUiGlyph& g, char32_t v) { return g.code_point < v; });
    return it != glyphs.end() && it->code_point == c ? &*it : nullptr;
  };
  if (const GraceUiGlyph* g = at(cp)) return g;
  if (const GraceUiGlyph* g = at(0xFFFD)) return g;
  return at(U'?');
}

// ---- Text ----

std::u32string decode(std::string_view s) {
  std::u32string out;
  out.reserve(s.size());
  std::size_t i = 0;
  while (i < s.size()) {
    const auto b = static_cast<unsigned char>(s[i]);
    std::size_t n = 0;
    char32_t c = 0;
    if (b < 0x80) {
      c = b;
      n = 1;
    } else if ((b & 0xE0) == 0xC0) {
      c = b & 0x1Fu;
      n = 2;
    } else if ((b & 0xF0) == 0xE0) {
      c = b & 0x0Fu;
      n = 3;
    } else if ((b & 0xF8) == 0xF0) {
      c = b & 0x07u;
      n = 4;
    }
    bool ok = n != 0 && i + n <= s.size();
    for (std::size_t k = 1; ok && k < n; ++k) {
      const auto cb = static_cast<unsigned char>(s[i + k]);
      ok = (cb & 0xC0) == 0x80;
      c = (c << 6) | (cb & 0x3Fu);
    }
    // Overlong forms, surrogates and values past U+10FFFF are refused like any other bad sequence.
    ok = ok && !(n == 2 && c < 0x80) && !(n == 3 && c < 0x800) && !(n == 4 && c < 0x10000) &&
         !(c >= 0xD800 && c <= 0xDFFF) && c <= 0x10FFFF;
    if (!ok) {
      out += U'�';
      ++i;
      continue;
    }
    out += c;
    i += n;
  }
  return out;
}

std::string upper(std::string_view utf8) {
  std::string out;
  out.reserve(utf8.size());
  for (const char32_t c : decode(utf8)) put_utf8(out, upper_of(c));
  return out;
}

std::size_t chars(std::string_view utf8) noexcept {
  std::size_t n = 0;
  for (const char c : utf8) n += (static_cast<unsigned char>(c) & 0xC0) != 0x80 ? 1 : 0;
  return n;
}

float measure(const Face& face, float px, std::u32string_view text, float tracking) noexcept {
  if (!face.usable()) return 0;
  const float scale = px / face.pixel_height;
  float w = 0;
  for (const char32_t c : text) {
    if (const GraceUiGlyph* g = face.find(c)) w += g->advance * scale + tracking * px;
  }
  return w;
}

std::vector<std::u32string> wrap(const Face& face, float px, std::u32string_view text, float width,
                                 std::size_t max_lines) {
  std::vector<std::u32string> lines;
  if (max_lines == 0) return lines;
  std::u32string line;
  std::size_t i = 0;
  const auto fits = [&](std::u32string_view s) { return measure(face, px, s) <= width; };
  while (i < text.size()) {
    // The next word and the spaces before it.
    std::size_t j = i;
    while (j < text.size() && text[j] == U' ') ++j;
    std::size_t k = j;
    while (k < text.size() && text[k] != U' ') ++k;
    const std::u32string_view spaces = text.substr(i, j - i);
    const std::u32string_view word = text.substr(j, k - j);
    std::u32string trial = line;
    if (!line.empty()) trial += spaces;
    trial += word;
    if (fits(trial)) {
      line = std::move(trial);
      i = k;
      continue;
    }
    if (!line.empty()) {
      lines.push_back(std::move(line));
      line.clear();
      if (lines.size() == max_lines) break;
      i = j;  // the word starts the next line, its spaces dropped
      continue;
    }
    // A word wider than a whole line: as much of it as fits.
    std::size_t cut = 1;
    while (j + cut < k && fits(word.substr(0, cut + 1))) ++cut;
    lines.emplace_back(word.substr(0, cut));
    if (lines.size() == max_lines) {
      i = j + cut;
      break;
    }
    i = j + cut;
  }
  if (!line.empty() && lines.size() < max_lines) {
    lines.push_back(std::move(line));
    line.clear();
  }
  // Text left over: the last line ends in "…".
  bool rest = !line.empty();
  for (std::size_t r = i; !rest && r < text.size(); ++r) rest = text[r] != U' ';
  if (rest && !lines.empty()) {
    std::u32string& last = lines.back();
    while (!last.empty() && !fits(last + kEllipsis)) last.pop_back();
    while (!last.empty() && last.back() == U' ') last.pop_back();
    last += kEllipsis;
  }
  return lines;
}

// ---- The painter ----

void Painter::reset(float width, float height, std::uint64_t white, float u, float v) {
  width_ = width;
  height_ = height;
  white_ = white;
  u_ = u;
  v_ = v;
  clip_ = {0, 0, width, height};
  generation_ = 0;
  glyphs_ = 0;
  vertices_.clear();
  indices_.clear();
  commands_.clear();
}

void Painter::clip(float x0, float y0, float x1, float y1) {
  x0 = std::clamp(x0, 0.0f, width_);
  y0 = std::clamp(y0, 0.0f, height_);
  clip_ = {x0, y0, std::clamp(x1, x0, width_), std::clamp(y1, y0, height_)};
}

void Painter::clip_all() { clip_ = {0, 0, width_, height_}; }

GraceUiDrawCommand& Painter::command_for(std::uint64_t texture, std::size_t vertices) {
  if (!commands_.empty()) {
    GraceUiDrawCommand& c = commands_.back();
    if (c.texture == texture && c.clip_x0 == clip_[0] && c.clip_y0 == clip_[1] && c.clip_x1 == clip_[2] &&
        c.clip_y1 == clip_[3] && c.vertex_count + vertices <= 65535u) {
      return c;
    }
  }
  GraceUiDrawCommand c{};
  c.clip_x0 = clip_[0];
  c.clip_y0 = clip_[1];
  c.clip_x1 = clip_[2];
  c.clip_y1 = clip_[3];
  c.texture = texture;
  c.vertex_offset = static_cast<std::uint32_t>(vertices_.size());
  c.index_offset = static_cast<std::uint32_t>(indices_.size());
  commands_.push_back(c);
  return commands_.back();
}

GraceUiDrawCommand* Painter::room(std::uint64_t texture, std::size_t vertices, std::size_t indices) {
  // GraceUI's bounds (D76): within them, or nothing more is drawn; a command never starts that would pass the count.
  const bool new_command = commands_.empty() || commands_.back().texture != texture;
  if ((vertices_.size() + vertices) * sizeof(GraceUiDrawVert) > GRACEUI_DRAW_VERTEX_BYTES_MAX ||
      (indices_.size() + indices) * sizeof(std::uint16_t) > GRACEUI_DRAW_INDEX_BYTES_MAX ||
      (new_command && commands_.size() >= GRACEUI_DRAW_COMMANDS_MAX)) {
    return nullptr;
  }
  GraceUiDrawCommand& c = command_for(texture, vertices);
  if (commands_.size() > GRACEUI_DRAW_COMMANDS_MAX) {
    commands_.pop_back();
    return nullptr;
  }
  return &c;
}

void Painter::poly(std::uint64_t texture, const GraceUiDrawVert* v, std::size_t n) {
  GraceUiDrawCommand* c = room(texture, n, 3 * (n - 2));
  if (c == nullptr) return;
  const auto base = static_cast<std::uint16_t>(c->vertex_count);
  for (std::size_t i = 0; i < n; ++i) vertices_.push_back(v[i]);
  for (std::size_t i = 1; i + 1 < n; ++i) {
    indices_.push_back(base);
    indices_.push_back(static_cast<std::uint16_t>(base + i));
    indices_.push_back(static_cast<std::uint16_t>(base + i + 1));
  }
  c->vertex_count += static_cast<std::uint32_t>(n);
  c->index_count += static_cast<std::uint32_t>(3 * (n - 2));
}

void Painter::mesh(std::uint64_t texture, const GraceUiDrawVert* v, std::size_t n, const std::uint16_t* idx,
                   std::size_t m) {
  GraceUiDrawCommand* c = room(texture, n, m);
  if (c == nullptr) return;
  const auto base = static_cast<std::uint16_t>(c->vertex_count);
  for (std::size_t i = 0; i < n; ++i) vertices_.push_back(v[i]);
  for (std::size_t i = 0; i < m; ++i) indices_.push_back(static_cast<std::uint16_t>(base + idx[i]));
  c->vertex_count += static_cast<std::uint32_t>(n);
  c->index_count += static_cast<std::uint32_t>(m);
}

void Painter::quad(std::uint64_t texture, float x0, float y0, float x1, float y1, float u0, float v0, float u1,
                   float v1, std::uint32_t colour) {
  if (x1 <= clip_[0] || y1 <= clip_[1] || x0 >= clip_[2] || y0 >= clip_[3] || (colour >> 24) == 0) return;
  const GraceUiDrawVert v[4] = {{x0, y0, u0, v0, colour}, {x1, y0, u1, v0, colour}, {x1, y1, u1, v1, colour},
                                {x0, y1, u0, v1, colour}};
  poly(texture, v, 4);
}

void Painter::rect(float x0, float y0, float x1, float y1, std::uint32_t colour) {
  if (x1 <= x0 || y1 <= y0) return;
  quad(white_, x0, y0, x1, y1, u_, v_, u_, v_, colour);
}

void Painter::edge(float x0, float y0, float x1, float y1, float t, std::uint32_t colour) {
  rect(x0, y0, x1, y0 + t, colour);
  rect(x0, y1 - t, x1, y1, colour);
  rect(x0, y0 + t, x0 + t, y1 - t, colour);
  rect(x1 - t, y0 + t, x1, y1 - t, colour);
}

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kFringe = 0.5f;  // each side of an outline: a pixel's width in all, as Dear ImGui's AA fringe

// A corner's points: enough segments that the arc strays at most a quarter of a pixel from the circle, 1 to 16.
std::size_t corner_points(float radius) noexcept {
  const float step = std::acos(std::clamp(1.0f - 0.25f / std::max(radius, 0.25f), -1.0f, 1.0f));
  const float segments = step > 0 ? std::ceil(0.5f * kPi / step) : 16.0f;
  return static_cast<std::size_t>(std::clamp(segments, 1.0f, 16.0f)) + 1;
}

// Strips joining ring `a` to ring `b` (each `n` points, closed), appended as triangles' indices.
void join(std::vector<std::uint16_t>& idx, std::size_t a, std::size_t b, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t j = (i + 1) % n;
    for (const std::size_t k : {a + i, a + j, b + j, a + i, b + j, b + i}) idx.push_back(static_cast<std::uint16_t>(k));
  }
}

}  // namespace

void Painter::outline(float x0, float y0, float x1, float y1, float radius, float inset, std::size_t per_corner,
                      std::uint32_t colour, std::vector<GraceUiDrawVert>& out) const {
  // Moved in by `inset`, a corner keeps its centre while the radius lasts, then becomes the inset rectangle's corner.
  const float at = std::max(radius, inset);
  const float r = std::max(radius - inset, 0.0f);
  const float cx[4] = {x0 + at, x1 - at, x1 - at, x0 + at};
  const float cy[4] = {y0 + at, y0 + at, y1 - at, y1 - at};
  for (std::size_t k = 0; k < 4; ++k) {
    const float from = kPi * (1.0f + 0.5f * static_cast<float>(k));  // top left: from the left, clockwise on screen
    for (std::size_t i = 0; i < per_corner; ++i) {
      const float a = from + 0.5f * kPi * static_cast<float>(i) / static_cast<float>(per_corner - 1);
      out.push_back(GraceUiDrawVert{cx[k] + r * std::cos(a), cy[k] + r * std::sin(a), u_, v_, colour});
    }
  }
}

void Painter::round_rect(float x0, float y0, float x1, float y1, float radius, std::uint32_t colour) {
  if (x1 <= x0 || y1 <= y0 || (colour >> 24) == 0) return;
  radius = std::min({radius, (x1 - x0) / 2.0f, (y1 - y0) / 2.0f});
  if (radius < kFringe) {
    rect(x0, y0, x1, y1, colour);
    return;
  }
  const std::size_t per = corner_points(radius);
  const std::size_t n = 4 * per;
  shape_.clear();
  shape_indices_.clear();
  outline(x0, y0, x1, y1, radius, kFringe, per, colour, shape_);              // the ground, its colour whole
  outline(x0, y0, x1, y1, radius, -kFringe, per, faded(colour, 0), shape_);  // the fringe's outer side, clear
  for (std::size_t i = 1; i + 1 < n; ++i) {
    for (const std::size_t k : {std::size_t{0}, i, i + 1}) shape_indices_.push_back(static_cast<std::uint16_t>(k));
  }
  join(shape_indices_, 0, n, n);
  mesh(white_, shape_.data(), shape_.size(), shape_indices_.data(), shape_indices_.size());
}

void Painter::round_edge(float x0, float y0, float x1, float y1, float radius, float t, std::uint32_t colour) {
  if (x1 <= x0 || y1 <= y0 || t <= 0 || (colour >> 24) == 0) return;
  radius = std::min({radius, (x1 - x0) / 2.0f, (y1 - y0) / 2.0f});
  if (radius < kFringe) {
    edge(x0, y0, x1, y1, t, colour);
    return;
  }
  const std::size_t per = corner_points(radius);
  const std::size_t n = 4 * per;
  shape_.clear();
  shape_indices_.clear();
  // Four rings, from the outline outwards by half a pixel to `t` inwards and half a pixel more: clear, whole, whole,
  // clear. A frame inside the rect, as edge() draws it.
  outline(x0, y0, x1, y1, radius, -kFringe, per, faded(colour, 0), shape_);
  outline(x0, y0, x1, y1, radius, kFringe, per, colour, shape_);
  outline(x0, y0, x1, y1, radius, std::max(kFringe, t - kFringe), per, colour, shape_);
  outline(x0, y0, x1, y1, radius, t + kFringe, per, faded(colour, 0), shape_);
  join(shape_indices_, 0, n, n);
  join(shape_indices_, n, 2 * n, n);
  join(shape_indices_, 2 * n, 3 * n, n);
  mesh(white_, shape_.data(), shape_.size(), shape_indices_.data(), shape_indices_.size());
}

void Painter::diamond(float cx, float cy, float r, std::uint32_t colour) {
  if ((colour >> 24) == 0) return;
  const GraceUiDrawVert v[4] = {{cx, cy - r, u_, v_, colour}, {cx + r, cy, u_, v_, colour},
                                {cx, cy + r, u_, v_, colour}, {cx - r, cy, u_, v_, colour}};
  poly(white_, v, 4);
}

float Painter::text(const Face& face, float px, float x, float y, std::u32string_view s, std::uint32_t colour,
                    float tracking) {
  if (!face.usable()) return x;
  const float scale = px / face.pixel_height;
  for (const char32_t c : s) {
    const GraceUiGlyph* g = face.find(c);
    if (g == nullptr) continue;
    if (g->x1 > g->x0 && g->y1 > g->y0) {
      const std::size_t before = vertices_.size();
      quad(face.atlas, x + g->x0 * scale, y + g->y0 * scale, x + g->x1 * scale, y + g->y1 * scale, g->u0, g->v0, g->u1,
           g->v1, colour);
      if (vertices_.size() != before) {
        ++glyphs_;
        generation_ = face.generation;
      }
    }
    x += g->advance * scale + tracking * px;
  }
  return x;
}

GraceUiDrawList Painter::view() const noexcept {
  GraceUiDrawList l{};
  l.size = sizeof l;
  l.atlas_generation = generation_;
  l.vertex_count = static_cast<std::uint32_t>(vertices_.size());
  l.index_count = static_cast<std::uint32_t>(indices_.size());
  l.command_count = static_cast<std::uint32_t>(commands_.size());
  l.command_size = sizeof(GraceUiDrawCommand);
  l.vertices = vertices_.data();
  l.indices = indices_.data();
  l.commands = commands_.data();
  return l;
}

// ---- The surfaces ----

void paint_page(Painter& p, const Faces& f, const Display& d, const proto::PageView& page, float alpha) {
  const float s = d.content_scale;
  const GraceUiPanelDesc panel = page_panel(d);
  const float block = std::min(panel.width, std::floor((s > 1.1f ? 0.70f : 0.46f) * d.width));
  const std::u32string name = u32(upper(page.speaker));
  std::vector<std::u32string> lines;
  if (page.kind == proto::PageKind::Speech) {
    lines = wrap(f.serif, kPagePx * s, u32(page.text), block, 3);
  } else {
    lines.emplace_back(1, kEllipsis);  // the card, and the "…" page after no answer
  }
  const float total = lh(kNamePx * s) + static_cast<float>(lines.size()) * lh(kPagePx * s);
  float y = std::max(0.0f, panel.height - 6.0f * s - total);
  const float shadow = std::max(1.0f, 1.5f * s);
  const auto centred = [&](const Face& face, float px, const std::u32string& t, std::uint32_t colour, float tracking) {
    const float w = measure(face, px, t, tracking);
    const float x = std::floor((panel.width - w) / 2.0f);
    p.text(face, px, x + shadow, y + shadow, t, faded(kShadow, alpha), tracking);
    p.text(face, px, x, y, t, faded(colour, alpha), tracking);
  };
  if (!name.empty()) centred(f.serif, kNamePx * s, name, kNameGold, kNameTracking);
  y += lh(kNamePx * s);
  for (const std::u32string& l : lines) {
    centred(f.serif, kPagePx * s, l, kTextInk, 0);
    y += lh(kPagePx * s);
  }
}

Caret paint_box(Painter& p, const Faces& f, const Display& d, const proto::ChatBox& box, const EditView& edit,
                const proto::PageView* strip_page, float strip_alpha, float& scroll) {
  const float s = d.content_scale;
  const GraceUiPanelDesc panel = box_panel(d);
  const float w = panel.width;
  const float h = panel.height;
  const float pad = std::round(10.0f * s);
  const bool input = box.view == proto::BoxView::Open;
  const float radius = std::round(kCardRadius * s);
  p.round_rect(0, 0, w, h, radius, kBoxGround);
  p.round_edge(0, 0, w, h, radius, std::max(1.0f, std::round(s)), kBoxEdge);
  float top = pad;
  // The header (NPC §5.1, O97): the partner, and who hears.
  if (input) {
    std::u32string header = U"Speak to ";
    append(header, box.partner);
    if (!box.heard_by.empty()) {
      header += U' ';
      header += kDash;
      header += U" heard by: ";
      for (std::size_t i = 0; i < box.heard_by.size(); ++i) {
        if (i != 0) header += U", ";
        append(header, box.heard_by[i]);
      }
    }
    const std::vector<std::u32string> one = wrap(f.sans, kHeaderPx * s, header, w - 2 * pad, 1);
    if (!one.empty()) p.text(f.sans, kHeaderPx * s, pad, top, one.front(), kNameGold);
    top += lh(kHeaderPx * s) + 4.0f * s;
  }
  // The input's field, at the bottom.
  const float field_h = lh(kInputPx * s) + 6.0f * s;
  const float bottom = input ? h - pad - field_h - 4.0f * s : h - pad;
  // The history strip, newest at the bottom: as many of the newest lines as fit (each wrapped to two at most).
  // The Tarnished's lines in the italic face (NPC §10.1), the serif's where GraceUI has none.
  struct Line {
    std::u32string text;
    std::uint32_t colour;
    const Face* face;
  };
  std::vector<Line> strip;
  const auto add = [&](const Face& face, const std::u32string& t, std::uint32_t colour) {
    for (std::u32string& l : wrap(face, kHistoryPx * s, t, w - 2 * pad, 2)) {
      strip.push_back(Line{std::move(l), colour, &face});
    }
  };
  const std::size_t from = box.history.size() > kHistoryShown ? box.history.size() - kHistoryShown : 0;
  for (std::size_t i = from; i < box.history.size(); ++i) {
    const proto::HistoryLine& l = box.history[i];
    std::u32string t;
    if (l.who == proto::HistoryLine::Who::Npc) {
      append(t, l.speaker);
      t += U": ";
    }
    append(t, l.text);
    if (l.waiting) t += U" (waiting)";
    const bool npc = l.who == proto::HistoryLine::Who::Npc;
    const std::uint32_t colour = npc ? kTextInk : kPlayerLine;
    add(npc ? f.serif : f.tarnished(), t, l.waiting ? faded(colour, 0.6f) : colour);
  }
  if (strip_page != nullptr && strip_page->surface == proto::Surface::HistoryStrip) {
    std::u32string t;
    append(t, strip_page->speaker);
    t += U": ";
    if (strip_page->kind == proto::PageKind::Speech) {
      append(t, strip_page->text);
    } else {
      t += kEllipsis;
    }
    add(f.serif, t, faded(kTextInk, strip_alpha));
  }
  const float line = lh(kHistoryPx * s);
  float y = bottom - line;
  p.clip(pad, top, w - pad, bottom);
  for (std::size_t i = strip.size(); i-- > 0 && y >= top - 0.5f;) {
    p.text(*strip[i].face, kHistoryPx * s, pad, y, strip[i].text, strip[i].colour);
    y -= line;
  }
  p.clip_all();
  if (!input) return Caret{};
  // The input: the field, the selection behind the text, the caret after it; scrolled so the caret shows.
  const float fx0 = pad;
  const float fx1 = w - pad;
  const float fy0 = h - pad - field_h;
  const float fy1 = h - pad;
  p.rect(fx0, fy0, fx1, fy1, rgba(0xFF, 0xFF, 0xFF, 0x10));
  p.rect(fx0, fy0, fx1, fy0 + std::max(1.0f, std::round(s)), kBoxEdge);
  const float px = kInputPx * s;
  const float inner = 6.0f * s;
  const float room = std::max(1.0f, fx1 - fx0 - 2 * inner);
  const std::size_t cursor = std::min(edit.cursor, edit.text.size());
  // The caret stands a pixel clear of the glyph before it (the preview shots showed it touching the last letter).
  const float gap = cursor > 0 ? std::max(1.0f, std::round(s)) : 0.0f;
  const float caret_at = measure(f.serif, px, u32(edit.text.substr(0, cursor))) + gap;
  const float total = measure(f.serif, px, u32(edit.text)) + (cursor == edit.text.size() ? gap : 0.0f);
  if (caret_at - scroll > room - 2.0f * s) scroll = caret_at - room + 2.0f * s;
  if (caret_at - scroll < 0) scroll = caret_at;
  scroll = std::clamp(scroll, 0.0f, std::max(0.0f, total - room + 2.0f * s));
  const float tx = fx0 + inner - scroll;
  const float ty = fy0 + 3.0f * s;
  p.clip(fx0 + 1.0f, fy0, fx1 - 1.0f, fy1);
  const std::size_t a = std::min({edit.select_start, edit.select_end, edit.text.size()});
  const std::size_t b = std::min(std::max(edit.select_start, edit.select_end), edit.text.size());
  if (b > a) {
    const float sx0 = tx + measure(f.serif, px, u32(edit.text.substr(0, a)));
    const float sx1 = tx + measure(f.serif, px, u32(edit.text.substr(0, b)));
    p.rect(sx0, ty, sx1, ty + lh(px), kSelection);
  }
  p.text(f.serif, px, tx, ty, u32(edit.text), kTextInk);
  const float cx = std::floor(tx + caret_at);
  p.rect(cx, ty + 1.0f * s, cx + std::max(1.0f, std::round(1.5f * s)), ty + lh(px) - 1.0f * s, kCaret);
  p.clip_all();
  return Caret{cx, ty, lh(px)};
}

void paint_hint(Painter& p, const Faces& f, const Display& d, const proto::TalkHint& hint, std::string_view key) {
  const float s = d.content_scale;
  const GraceUiPanelDesc panel = hint_panel(d);
  const float px = kHintPx * s;
  // The marker's 10 px diamond, centred, and the line 14 px below it.
  p.diamond(std::floor(panel.width / 2.0f), 7.0f * s, 5.0f * s, kMarker);
  const float y = 26.0f * s;
  std::u32string words;
  std::u32string boxed;
  switch (hint.verb) {
    case proto::HintVerb::Speak: words = U" Speak "; break;
    case proto::HintVerb::Answer: words = U" Answer "; break;
    case proto::HintVerb::Next: words = U"Speak to "; break;
  }
  if (hint.verb == proto::HintVerb::Next) {
    append(words, hint.name);
    words += U" next";
  } else {
    boxed = u32(key);  // the key as the player bound it (describe_hotkey); none when the talk hotkey is unbound
    words += kDash;
    words += U' ';
    append(words, hint.name);
  }
  const float key_pad = 4.0f * s;
  const float key_w = boxed.empty() ? 0.0f : measure(f.sans, px, boxed) + 2 * key_pad;
  const float total = key_w + measure(f.sans, px, words);
  float x = std::floor((panel.width - total) / 2.0f);
  if (!boxed.empty()) {
    p.round_edge(x, y - 2.0f * s, x + key_w, y + lh(px) + 1.0f * s, std::round(kControlRadius * s),
                 std::max(1.0f, std::round(s)), kHintInk);
    p.text(f.sans, px, x + key_pad, y, boxed, kHintInk);
    x += key_w;
  }
  p.text(f.sans, px, x, y, words, kHintInk);
}

// ---- The lower third over time ----

float LowerThird::alpha_in(std::uint64_t now) const noexcept {
  const std::uint64_t first = first_.value_or(now);
  const std::uint64_t since = now > first ? now - first : 0;
  if (current_ && current_->kind == proto::PageKind::Card) {
    // The card's "…" pulses (NPC §6.2), stepped: a triangle between 0.45 and 1.
    const std::uint64_t t = since % kPulseMs;
    const float phase = static_cast<float>(t) / static_cast<float>(kPulseMs);
    return 0.45f + 0.55f * (phase < 0.5f ? 1.0f - 2.0f * phase : 2.0f * phase - 1.0f);
  }
  // The first frame drawn is never transparent: its time is the one PageShown names.
  return std::min(1.0f, static_cast<float>(since + kSubmitEveryMs) / static_cast<float>(kFadeInMs));
}

void LowerThird::update(const std::optional<proto::PageView>& page, std::uint64_t now) {
  const bool same = page && current_ && current_->id == page->id && current_->kind == page->kind;
  if (same) {
    current_ = page;  // its count and `last` may grow; its text is sealed
    return;
  }
  if (current_ && first_) {
    // The page up leaves: a cross-fade to the next, or the last fade-out.
    leaving_ = current_;
    leave_from_ = alpha_in(now);
    leave_at_ = now;
    leave_ms_ = page ? kCrossFadeMs : kFadeOutMs;
  }
  current_ = page;
  first_.reset();
  acked_ = false;
}

std::vector<LowerThird::Drawn> LowerThird::frame(std::uint64_t now) const {
  std::vector<Drawn> out;
  if (leaving_ && now < leave_at_ + leave_ms_) {
    const float left = 1.0f - static_cast<float>(now - leave_at_) / static_cast<float>(leave_ms_);
    if (left > 0) out.push_back(Drawn{*leaving_, leave_from_ * left});
  }
  if (current_) out.push_back(Drawn{*current_, alpha_in(now)});
  return out;
}

std::optional<proto::UiEvent::PageShown> LowerThird::submitted(std::uint64_t now) {
  if (leaving_ && now >= leave_at_ + leave_ms_) leaving_.reset();
  if (!current_) return std::nullopt;
  if (!first_) first_ = now;
  if (current_->kind != proto::PageKind::Speech || acked_) return std::nullopt;
  acked_ = true;
  return proto::UiEvent::PageShown{current_->id, *first_};
}

bool LowerThird::moving(std::uint64_t now) const noexcept {
  if (leaving_ && now < leave_at_ + leave_ms_ + kSubmitEveryMs) return true;
  if (!current_) return false;
  return !first_ || current_->kind == proto::PageKind::Card || alpha_in(now) < 1.0f ||
         now < *first_ + kFadeInMs + kSubmitEveryMs;
}

bool LowerThird::empty(std::uint64_t now) const noexcept {
  return !current_ && !(leaving_ && now < leave_at_ + leave_ms_);
}

// ---- Toasts ----

std::vector<ToastBook::Call> ToastBook::update(std::span<const proto::Toast> toasts) {
  std::vector<Call> calls;
  const auto listed = [&](std::uint32_t id) {
    return std::any_of(toasts.begin(), toasts.end(), [&](const proto::Toast& t) { return t.id == id; });
  };
  for (std::size_t k = 0; k < on_.size(); ++k) {
    if (on_[k] != 0 && !listed(on_[k])) {
      calls.push_back(Call{kToastKeys[k], std::string(), GRACEUI_TONE_NORMAL});
      done_.push_back(on_[k]);
      on_[k] = 0;
    }
  }
  if (done_.size() > kDoneKept) {
    done_.erase(done_.begin(), done_.begin() + static_cast<std::ptrdiff_t>(done_.size() - kDoneKept));
  }
  for (const proto::Toast& t : toasts) {
    if (t.id == 0 || t.text.empty()) continue;
    if (std::find(on_.begin(), on_.end(), t.id) != on_.end()) continue;
    if (std::find(done_.begin(), done_.end(), t.id) != done_.end()) continue;
    const auto free = std::find(on_.begin(), on_.end(), 0u);
    if (free == on_.end()) break;  // it waits for a key
    *free = t.id;
    std::string text = t.text.substr(0, std::min<std::size_t>(t.text.size(), proto::kToastMaxBytes));
    calls.push_back(Call{kToastKeys[static_cast<std::size_t>(free - on_.begin())], std::move(text), tone_of(t.level)});
  }
  return calls;
}

std::size_t ToastBook::showing() const noexcept {
  return static_cast<std::size_t>(std::count_if(on_.begin(), on_.end(), [](std::uint32_t id) { return id != 0; }));
}

// ---- Input ----

char32_t key_char(std::uint32_t vk) noexcept {
  if (vk >= 0x41 && vk <= 0x5A) return static_cast<char32_t>(vk + 0x20);  // 'A'..'Z' type 'a'..'z'
  if (vk >= 0x30 && vk <= 0x39) return static_cast<char32_t>(vk);
  if (vk == kVkSpace) return U' ';
  return 0;
}

void InputMap::open(std::uint64_t now, std::uint32_t vk) noexcept {
  swallow_ = key_char(vk);
  until_ = now + kSwallowMs;
}

InputMap::Mapped InputMap::map(const GraceUiEvent& e, std::uint64_t now) noexcept {
  Mapped m;
  switch (e.kind) {
    case GRACEUI_EVENT_KEY: {
      const bool down = e.pressed != 0;
      const bool fresh = down && e.repeat == 0;
      if (e.key == kVkEscape) {
        m.action = fresh ? Action::Escape : Action::None;
      } else if (e.key == kVkReturn) {
        m.action = fresh ? Action::Submit : Action::None;
      } else if (e.key == kVkTab) {
        m.action = fresh ? Action::Skip : Action::None;
      } else {
        m.action = Action::Edit;
      }
      return m;
    }
    case GRACEUI_EVENT_CHARACTER: {
      const char32_t c = e.code_point;
      const char32_t key = swallow_;
      const bool armed = key != 0 && now <= until_;
      swallow_ = 0;  // only the first character after the open is looked at
      if (armed && (c == key || (c >= U'A' && c <= U'Z' && c + 0x20 == key))) {
        m.swallowed = true;
        return m;
      }
      if (c < 0x20 || c == 0x7F) return m;  // Enter's, Tab's, Esc's and Backspace's own characters
      m.action = Action::Edit;
      return m;
    }
    case GRACEUI_EVENT_PAD:
      if (e.button == kPadB) {
        m.action = e.pressed != 0 ? Action::Escape : Action::None;
      } else if (e.button == kPadA) {
        m.action = e.pressed != 0 ? Action::Submit : Action::None;
      } else {
        m.action = Action::Edit;
      }
      return m;
    case GRACEUI_EVENT_INPUT_ENDED:
      m.action = Action::Ended;
      switch (e.why) {
        case GRACEUI_CLOSED_RELEASED: break;  // the box's own release, or its panel hidden by it
        case GRACEUI_CLOSED_FOCUS_LOST: m.reason = proto::CloseReason::FocusLost; break;
        case GRACEUI_CLOSED_FAULT: m.reason = proto::CloseReason::Fault; break;
        default: m.reason = proto::CloseReason::InputLost; break;  // the menu key, danger, loading, no render, the exit
      }
      return m;
    default:
      return m;  // the pointer: v0.2's box takes no pointer (its field keeps the focus)
  }
}

}  // namespace en::overlay::surfaces
