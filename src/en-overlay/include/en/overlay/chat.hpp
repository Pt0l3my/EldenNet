#pragma once
// The conversation's surfaces through GraceUI (WHY§364, part C), in the Windows builds: the panels, the box's input
// and its own Dear ImGui, the talk and skip hotkeys, the toasts as notices, all on T6 (GraceUI's G15: T6 alone calls
// GraceUI). surfaces.hpp lays them out; this calls GraceUI. Overlay (overlay.hpp) owns one and hands it its events.
//
// - At connect, on T1 (EldenNet's registration window): a small white texture of its own, for the solid shapes; its two
//   faces (GraceUI's own EB Garamond and Noto Sans, from GraceUI's folder, as GRACEUI_FACE_SCALED faces, before
//   warm-up: GraceUI eldennet-changes §33); the hotkeys talk (T, with the pad chord D-pad Up then View behind
//   has_cap("pad-chords"), §47) and skip (Tab), by their stable action ids and players' names (§48, §49).
// - On T6, each event poll gives it: the talk key opens (OpenChat), Tab skips the page up; while the box holds input,
//   GraceUI's holder events (D176): keys, characters and the pad to the box's own Dear ImGui context, Enter sends, Esc
//   and the pad's B close, and the holder's end closes with its reason; the display's event (D180); a hotkey changed
//   (D210), whose words the hint takes from describe_hotkey.
// - On T6, after the core's step (`show`): the newest model drawn, each panel submitted at most every 100 ms; the box's
//   input asked for when the model opens it (request_input with the holder's events, the box's own panel shown first),
//   refused and not now answered as Close; the box closed by the model answered as Close{Shut} with its draft; the
//   caret set for the IME while the box holds input; and PageShown on a Speech page's first submitted frame.
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/overlay/surfaces.hpp"
#include "en/proto/ui.hpp"
#include "graceui_sys.hpp"

struct ImGuiContext;

namespace en::overlay {

inline constexpr std::uint32_t kKeyTalk = 0x54;   // VK 'T': the talk key's default (O94; NPC §5.1)
inline constexpr std::uint32_t kKeySkip = 0x09;   // VK_TAB: the page's skip (NPC §8.6)
inline constexpr std::string_view kActionTalk = "talk";
inline constexpr std::string_view kActionSkip = "skip-page";
inline constexpr std::string_view kNameTalk = "Talk";
inline constexpr std::string_view kNameSkip = "Skip the page";
// The chat box's pad chord (NPC §5.6, O11; GraceUI D204): D-pad Up, the anchor, then View (XInput's BACK).
inline constexpr std::uint32_t kTalkChord = GRACEUI_PAD_CHORD(0x0001u, 0x0020u);
inline constexpr float kSerifDesignPx = 30.0f;  // EldenNet's serif face, baked for the page's text (the rest scale down)
inline constexpr float kSansDesignPx = 18.0f;   // its sans face: the box's header and the hint

// The box's input line, on its own headless Dear ImGui context (GraceUI §31 item 3): fed one holder event at a time,
// each followed by a frame, so that every key and character is taken in order. Never drawn: the box draws the line from
// GraceUI's glyphs (surfaces.hpp); this keeps the text, the caret and the selection.
class Editor {
 public:
  Editor();
  ~Editor();
  Editor(const Editor&) = delete;
  Editor& operator=(const Editor&) = delete;

  // The box opens (or its conversation changes): the line holds `text`, the caret at its end, its limits NPC §8.5's.
  void set(std::string_view text, std::size_t max_chars, std::size_t max_bytes);
  void feed(const GraceUiEvent& e, std::uint64_t now);
  void clear() { set({}, max_chars_, max_bytes_); }
  [[nodiscard]] std::string text() const;
  [[nodiscard]] surfaces::EditView view() const noexcept;
  [[nodiscard]] std::uint32_t edits() const noexcept { return edits_; }  // rises with each change of text or caret

 private:
  void frame(std::uint64_t now);
  ImGuiContext* ctx_ = nullptr;
  std::array<char, proto::kLineMaxBytes + 1> buf_{};
  std::size_t max_chars_ = proto::kLineMaxChars;
  std::size_t max_bytes_ = proto::kLineMaxBytes;
  unsigned id_ = 0;
  bool focus_ = false;
  bool to_end_ = false;
  std::uint64_t last_ = 0;
  std::uint32_t edits_ = 0;
  int cursor_ = 0;
  int select_start_ = 0;
  int select_end_ = 0;
};

// What the surfaces registered at connect, for the log.
struct SurfacesReport {
  const char* state = "off";  // "ready", "no_caps", "texture", "hotkey"
  std::uint32_t faces = 0;    // EldenNet's faces registered (0 to 2)
  GraceUiStatus face_status = GRACEUI_NOT_YET;  // the last register_face's answer (too_late after GraceUI's warm-up)
  const char* talk = "off";   // "bound", "unbound"
  const char* chord = "off";  // "asked" (with the key), "no_cap", "refused"
  bool wake = false;          // GraceUI's wait handle (ABI 1.5)
};

class Surfaces {
 public:
  struct Fonts {
    std::span<const unsigned char> serif;  // GraceUI's own EB Garamond Regular, as the DLL read it from GraceUI's folder
    std::span<const unsigned char> sans;   // GraceUI's own Noto Sans Regular
  };
  // On T1 at connect, before any submit: null when the caps it needs are missing or a step fails (`report` says why).
  [[nodiscard]] static std::unique_ptr<Surfaces> make(const graceui::Api& api, GraceUiConsumer me, const Fonts& fonts,
                                                      const GraceUiConnectAnswer& answer, SurfacesReport& report);
  ~Surfaces();
  Surfaces(const Surfaces&) = delete;
  Surfaces& operator=(const Surfaces&) = delete;

  // ---- T6 from here on.
  // One event of poll's: true when it is the surfaces' (their hotkeys, a holder's events, the display, a hotkey
  // changed), with what the player did appended to `out`.
  bool take(const GraceUiEvent& e, std::uint64_t now, std::vector<proto::UiEvent>& out);
  // The newest model drawn (above); answers the events it raised.
  [[nodiscard]] std::vector<proto::UiEvent> show(const proto::UiModel& model, std::uint64_t now);
  // A fault in the overlay's own code (ffi_guard tripped): input given back at once; the next show reports it.
  void fault() noexcept;

  [[nodiscard]] void* wake() const noexcept { return wake_; }  // GraceUI's wait handle, or null
  [[nodiscard]] bool holding() const noexcept { return holding_; }
  [[nodiscard]] std::uint32_t talk_hotkey() const noexcept { return talk_; }
  [[nodiscard]] std::uint32_t skip_hotkey() const noexcept { return skip_; }
  [[nodiscard]] const std::string& key_words() const noexcept { return key_words_; }
  // The last open: when its talk key was polled, and when the box's first frame was submitted (T6's mono ms).
  struct Opened {
    std::uint64_t pressed = 0;
    std::uint64_t submitted = 0;
  };
  [[nodiscard]] std::optional<Opened> take_opened() noexcept;
  // Counters for the log and the tests.
  struct Counts {
    std::uint64_t submits = 0;
    std::uint64_t refused = 0;  // submits or panel calls GraceUI refused
    std::uint64_t shown = 0;    // PageShown raised
    std::uint64_t toasts = 0;   // notices posted
    std::uint64_t swallowed = 0;
  };
  [[nodiscard]] const Counts& counts() const noexcept { return counts_; }

 private:
  Surfaces(const graceui::Api& api, GraceUiConsumer me);
  struct Panel {
    std::uint64_t id = 0;
    GraceUiPanelDesc desc{};
    bool shown = false;
    std::uint64_t last_submit = 0;
    bool submitted = false;
    surfaces::Painter painter;
  };
  bool panels(std::uint64_t now);
  bool faces();
  bool submit(Panel& p, std::uint64_t now, bool force);
  void hide(Panel& p);
  void describe_talk();
  void release(std::vector<proto::UiEvent>* out, std::optional<proto::CloseReason> reason);
  void caret(const surfaces::Caret& c);
  [[nodiscard]] bool due(const Panel& p, std::uint64_t now) const noexcept;

  graceui::Api api_;
  GraceUiConsumer me_ = 0;
  std::uint64_t white_ = 0;
  std::uint32_t serif_face_ = 0;
  std::uint32_t sans_face_ = 0;
  surfaces::Faces faces_;
  std::uint32_t talk_ = 0;
  std::uint32_t skip_ = 0;
  std::uint32_t talk_vk_ = kKeyTalk;
  std::string key_words_ = "T";
  void* wake_ = nullptr;
  surfaces::Display display_;
  surfaces::Display made_for_;
  Panel page_;
  Panel box_;
  Panel hint_;
  surfaces::LowerThird lower_;
  surfaces::LowerThird strip_;
  surfaces::ToastBook toasts_;
  surfaces::InputMap input_;
  Editor editor_;
  // The box.
  bool holding_ = false;
  bool asked_ = false;          // input asked for since the model last opened the box
  bool faulted_ = false;
  proto::ConvId box_conv_ = 0;  // the conversation whose draft the editor took
  std::optional<proto::ChatBox> last_box_;
  std::uint32_t box_edits_ = 0;
  float scroll_ = 0;
  surfaces::Caret caret_{};
  std::optional<proto::TalkHint> last_hint_;
  std::string last_hint_words_;
  std::uint64_t pressed_ = 0;   // the last talk key's poll
  std::optional<Opened> opened_;
  Counts counts_;
};

}  // namespace en::overlay
