#pragma once
// The first conversation's surfaces (WHY§364, part C; NPC §5.1, §5.5, §8.6, §8.7, §10.1, §10.2, §10.5): the lower
// third's page and the speaker card, the chat box, the talk hint and its marker, and the toasts as GraceUI notices,
// laid out from en-proto's UiModel. Portable and pure, on en-proto and graceui.h (plain C), so it is tested in every
// build: the model's draw lists in GraceUI's own form (D76: Dear ImGui's vertex layout, 16-bit indices, a command per
// texture and clip), the glyphs from a glyph table (GraceUI's, `glyph_table`), the fades, the toasts' notice keys and
// the box's input mapping. The Windows part (overlay.hpp) owns the panels, the box's own Dear ImGui context and the
// calls into GraceUI, all on T6 (GraceUI's G15: T6 alone calls GraceUI).
//
// The text is GraceUI's faces only (no font file of ours, NPC §10.1 as WHY§364 (2) settled it): a face's quads come
// from its glyph table, each emitted into the list on the shared atlas's texture id, scaled from the size GraceUI baked
// it at to the surface's size at the content scale (GraceUI eldennet-changes §33). Dear ImGui is not used here.
//
// Motion is fades only, stepped at T6's pace (at most ten submits a second while GraceUI's ABI has no on_present,
// WHY§364 (3)): 120 ms in (inside the hold), a 120 ms cross-fade to the next page, 350 ms out after the last. A Speech
// page's first frame drawn with a non-zero opacity is the one PageShown acknowledges (NPC §8.7 item 1).
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/ui.hpp"
#include "graceui.h"

namespace en::overlay::surfaces {

// ---- Colours: GraceUI's vertex colour, the bytes R, G, B, A (D76) ----

[[nodiscard]] constexpr std::uint32_t rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept {
  return std::uint32_t{r} | (std::uint32_t{g} << 8) | (std::uint32_t{b} << 16) | (std::uint32_t{a} << 24);
}
// A colour's alpha multiplied by `alpha` (0 to 1).
[[nodiscard]] std::uint32_t faded(std::uint32_t colour, float alpha) noexcept;

// NPC §10.1's colours.
inline constexpr std::uint32_t kNameGold = rgba(0xC8, 0xB2, 0x7A, 0xFF);
inline constexpr std::uint32_t kTextInk = rgba(0xE8, 0xE2, 0xD0, 0xFF);
inline constexpr std::uint32_t kShadow = rgba(0x00, 0x00, 0x00, 0xB3);      // #000 at 0.7
inline constexpr std::uint32_t kBoxGround = rgba(0x00, 0x00, 0x00, 0xB3);   // #000 at 0.70
inline constexpr std::uint32_t kBoxEdge = rgba(0xC8, 0xB2, 0x7A, 0x80);     // #C8B27A at 0.5
inline constexpr std::uint32_t kPlayerLine = rgba(0xA8, 0xA3, 0x98, 0xFF);  // the Tarnished's lines (italic in the
                                                                            // design: GraceUI has no italic face yet)
inline constexpr std::uint32_t kHintInk = rgba(0xE8, 0xE2, 0xD0, 0xD9);     // #E8E2D0 at 0.85
inline constexpr std::uint32_t kMarker = rgba(0xC8, 0xB2, 0x7A, 0xD9);      // #C8B27A at 0.85
inline constexpr std::uint32_t kSelection = rgba(0xC8, 0xB2, 0x7A, 0x59);
inline constexpr std::uint32_t kCaret = rgba(0xE8, 0xE2, 0xD0, 0xFF);

// ---- Sizes, at 1080 lines and the default Menu size (NPC §10.1, §10.7; GraceUI §32, §33) ----

inline constexpr float kNamePx = 20.0f;     // the page's name line, upper case, +0.08 em
inline constexpr float kPagePx = 30.0f;     // the page's text
inline constexpr float kHeaderPx = 18.0f;   // the box's header
inline constexpr float kHistoryPx = 20.0f;  // the box's history strip
inline constexpr float kInputPx = 24.0f;    // the box's input
inline constexpr float kHintPx = 18.0f;     // the hint
inline constexpr float kNameTracking = 0.08f;

// The fades and the pulse (NPC §8.6, §10.1), and T6's pace.
inline constexpr std::uint64_t kFadeInMs = 120;
inline constexpr std::uint64_t kCrossFadeMs = 120;
inline constexpr std::uint64_t kFadeOutMs = 350;
inline constexpr std::uint64_t kSubmitEveryMs = 100;  // at most ten submits a second, per panel
inline constexpr std::uint64_t kPulseMs = 1200;       // the speaker card's "…", one pulse
inline constexpr std::uint64_t kSwallowMs = 100;      // the opening key's character, within this of the open

// ---- The display (GraceUI's display event, D180) ----

struct Display {
  float width = 0;   // the back buffer's size
  float height = 0;
  float content_scale = 1.0f;  // (H / 1080) x ui.scale / its default: the sizes above are multiplied by it
  float faces_scale = 0.0f;    // what EldenNet's scaled faces were baked at (0: none)

  [[nodiscard]] bool known() const noexcept { return width >= 64.0f && height >= 64.0f; }
  friend bool operator==(const Display&, const Display&) = default;
};

// The surfaces' panels, in the display's pixels from their anchors (GraceUI's panels cannot move or grow, so they are
// made for the display they are drawn on).
[[nodiscard]] GraceUiPanelDesc page_panel(const Display& d) noexcept;  // the lower third, hud layer, bottom centre
[[nodiscard]] GraceUiPanelDesc box_panel(const Display& d) noexcept;   // the chat box, panel layer, takes input
[[nodiscard]] GraceUiPanelDesc hint_panel(const Display& d) noexcept;  // the hint and its marker, hud layer

// ---- Faces: a glyph table, read-only (GraceUI's, valid while its generation is current) ----

struct Face {
  std::uint32_t id = 0;
  std::uint32_t generation = 0;
  std::uint64_t atlas = 0;  // the shared atlas's texture id
  float pixel_height = 0;   // the size GraceUI baked it at
  float ascent = 0;
  float descent = 0;
  std::span<const GraceUiGlyph> glyphs;  // sorted by code point

  [[nodiscard]] static Face of(const GraceUiGlyphTable& table) noexcept;
  [[nodiscard]] bool usable() const noexcept { return !glyphs.empty() && pixel_height > 0; }
  // The glyph for a code point; a missing one is U+FFFD's, else '?''s, else null (nothing drawn, no advance).
  [[nodiscard]] const GraceUiGlyph* find(char32_t cp) const noexcept;
};

// The surfaces' two faces: the serif's (the page, the history and the input) and the sans's (the box's header, the
// hint). Either may be GraceUI's notices' face when EldenNet's own could not be registered.
struct Faces {
  Face serif;
  Face sans;
  [[nodiscard]] bool usable() const noexcept { return serif.usable() && sans.usable(); }
};

// ---- Text ----

// The code points of `utf8`, an invalid sequence as U+FFFD.
[[nodiscard]] std::u32string decode(std::string_view utf8);
// `utf8` in upper case, for the name line: ASCII, Latin-1 and Latin Extended-A, the rest as it is.
[[nodiscard]] std::string upper(std::string_view utf8);
// How many code points `utf8` holds.
[[nodiscard]] std::size_t chars(std::string_view utf8) noexcept;

// The width of `text` in `face` at `px` pixels, with `tracking` em added to each glyph's advance.
[[nodiscard]] float measure(const Face& face, float px, std::u32string_view text, float tracking = 0) noexcept;
// `text` broken into at most `max_lines` lines of at most `width`, at spaces; a word wider than a line is cut where it
// must be. What does not fit the last line is cut, ending in "…".
[[nodiscard]] std::vector<std::u32string> wrap(const Face& face, float px, std::u32string_view text, float width,
                                               std::size_t max_lines);

// ---- Draw lists in GraceUI's form (D76) ----

class Painter {
 public:
  // A list for a panel of `width` by `height`; `white` is a texture of EldenNet's own whose texel at (u, v) is white,
  // for the solid shapes.
  void reset(float width, float height, std::uint64_t white, float u, float v);
  void rect(float x0, float y0, float x1, float y1, std::uint32_t colour);
  void edge(float x0, float y0, float x1, float y1, float thickness, std::uint32_t colour);  // a frame inside the rect
  void diamond(float cx, float cy, float radius, std::uint32_t colour);
  // A line of text from the pen at (x, y), the line's top, as GraceUI's glyph quads are measured; answers the pen's x
  // after it. A glyph's quad is clipped by the current clip.
  float text(const Face& face, float px, float x, float y, std::u32string_view text, std::uint32_t colour,
             float tracking = 0);
  // The clip rectangle for what follows (in the panel's pixels, within it); clip_all() for the whole panel.
  void clip(float x0, float y0, float x1, float y1);
  void clip_all();

  [[nodiscard]] GraceUiDrawList view() const noexcept;
  [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }
  [[nodiscard]] const std::vector<GraceUiDrawVert>& vertices() const noexcept { return vertices_; }
  [[nodiscard]] const std::vector<std::uint16_t>& indices() const noexcept { return indices_; }
  [[nodiscard]] const std::vector<GraceUiDrawCommand>& commands() const noexcept { return commands_; }
  [[nodiscard]] std::uint32_t glyphs() const noexcept { return glyphs_; }  // glyph quads emitted

 private:
  void quad(std::uint64_t texture, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1,
            std::uint32_t colour);
  void poly(std::uint64_t texture, const GraceUiDrawVert* v, std::size_t n);  // a convex fan
  GraceUiDrawCommand& command_for(std::uint64_t texture, std::size_t vertices);

  float width_ = 0;
  float height_ = 0;
  std::uint64_t white_ = 0;
  float u_ = 0;
  float v_ = 0;
  std::array<float, 4> clip_{};
  std::uint32_t generation_ = 0;
  std::uint32_t glyphs_ = 0;
  std::vector<GraceUiDrawVert> vertices_;
  std::vector<std::uint16_t> indices_;
  std::vector<GraceUiDrawCommand> commands_;
};

// ---- The surfaces, laid out ----

// The lower third: a name line (gold, upper case) and the page's text (two wrapped lines), with a shadow, centred, at
// `alpha`; a card is the name and "…" at the pulse's alpha. Drawn on the page panel, or into the box's strip.
void paint_page(Painter& p, const Faces& f, const Display& d, const proto::PageView& page, float alpha);

// The box's input as its editor holds it: the text, and the caret and the selection as byte offsets into it.
struct EditView {
  std::string_view text;
  std::size_t cursor = 0;
  std::size_t select_start = 0;
  std::size_t select_end = 0;
};
struct Caret {
  float x = 0;  // in the box panel's own pixels
  float y = 0;
  float height = 0;
};
// The box: its ground and edge, the header ("Speak to …", "— heard by: …" when others listen, NPC §5.1, O97), the
// history strip's newest lines (a waiting line greyed, "(waiting)"), a strip page if the page is the strip's, and the
// input with its selection and caret, scrolled so the caret shows (`scroll` is the box's, kept between frames). Answers
// the caret's rectangle, for the IME (set_caret). With the view Strip, the strip alone: no input, no caret.
Caret paint_box(Painter& p, const Faces& f, const Display& d, const proto::ChatBox& box, const EditView& edit,
                const proto::PageView* strip_page, float strip_alpha, float& scroll);

// The hint, "[T] Speak — Kalé", "[T] Answer — Kalé" or "Speak to Kalé next", its key boxed (`key`, the words GraceUI's
// describe_hotkey gives; empty, no key), under the marker's diamond.
// v0.2's surfaces read no render feed, so both sit where NPC §10.1 puts them without a camera: (0.50 W, 0.68 H).
void paint_hint(Painter& p, const Faces& f, const Display& d, const proto::TalkHint& hint, std::string_view key);

// ---- The lower third over time: fades, the cross-fade, and the frame PageShown acknowledges ----

class LowerThird {
 public:
  struct Drawn {
    proto::PageView page;
    float alpha = 0;
  };
  // The model's page at `now`: a new id starts its fade-in at the next frame (cross-fading the page before it); none
  // fades the page up out (350 ms).
  void update(const std::optional<proto::PageView>& page, std::uint64_t now);
  // What a frame at `now` draws, oldest first.
  [[nodiscard]] std::vector<Drawn> frame(std::uint64_t now) const;
  // The frame at `now` was submitted: answers the PageShown of a Speech page whose first frame it was (once each).
  [[nodiscard]] std::optional<proto::UiEvent::PageShown> submitted(std::uint64_t now);
  // Whether a frame now would change what the last showed (a fade under way, a page to start).
  [[nodiscard]] bool moving(std::uint64_t now) const noexcept;
  [[nodiscard]] bool empty(std::uint64_t now) const noexcept;
  [[nodiscard]] const std::optional<proto::PageView>& current() const noexcept { return current_; }

 private:
  [[nodiscard]] float alpha_in(std::uint64_t now) const noexcept;
  std::optional<proto::PageView> current_;
  std::optional<std::uint64_t> first_;  // the current page's first submitted frame
  bool acked_ = false;
  std::optional<proto::PageView> leaving_;
  std::uint64_t leave_at_ = 0;
  std::uint64_t leave_ms_ = 0;
  float leave_from_ = 1.0f;
};

// ---- Toasts as GraceUI notices (NPC §10.5): each id posted once, let go when the model no longer lists it ----

inline constexpr std::array<std::uint32_t, 2> kToastKeys = {3, 4};  // EldenNet's notice keys for toasts (1 and 2 are
                                                                    // the banner's and the badge's; four in all, D134)
class ToastBook {
 public:
  struct Call {
    std::uint32_t key = 0;
    std::string text;  // empty: withdraw the notice under `key`
    std::uint32_t tone = GRACEUI_TONE_NORMAL;
  };
  // The notices to post and withdraw for the toasts the model lists now, withdrawals first. A listed toast that finds
  // no key free waits for one (the core lists two at most).
  [[nodiscard]] std::vector<Call> update(std::span<const proto::Toast> toasts);
  [[nodiscard]] std::size_t showing() const noexcept;

 private:
  std::array<std::uint32_t, kToastKeys.size()> on_{};  // the toast id under each key; 0 for none
  std::vector<std::uint32_t> done_;                     // ids posted and let go: never posted again
};

// ---- The box's input (GraceUI D176): a holder's events as the box takes them ----

class InputMap {
 public:
  enum class Action : std::uint8_t {
    None,    // passed over
    Edit,    // to the box's editor (its own Dear ImGui)
    Submit,  // Enter, or the pad's A
    Escape,  // Esc, or the pad's B
    Skip,    // Tab: the page up ends its hold
    Ended,   // the holder ended: `reason`, or none when the box released it itself
  };
  struct Mapped {
    Action action = Action::None;
    std::optional<proto::CloseReason> reason;  // Ended: why; none for GraceUI's `released` (the box's own release)
    bool swallowed = false;                    // the opening key's character, dropped
  };
  // The box opened at `now` on a talk key of virtual key `vk` (0 for the pad's chord): the first character within
  // kSwallowMs that is the key's own is swallowed (NPC §5.1).
  void open(std::uint64_t now, std::uint32_t vk) noexcept;
  [[nodiscard]] Mapped map(const GraceUiEvent& e, std::uint64_t now) noexcept;

 private:
  char32_t swallow_ = 0;
  std::uint64_t until_ = 0;
};

// The character a talk key types, lower case: letters, digits and space; 0 for any other key.
[[nodiscard]] char32_t key_char(std::uint32_t vk) noexcept;

}  // namespace en::overlay::surfaces
