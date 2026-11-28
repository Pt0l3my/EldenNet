#pragma once
// The conversation's surfaces through GraceUI (WHY§364, part C), in the Windows builds: the panels, the box's input
// and its own Dear ImGui, the talk and skip hotkeys, the toasts as notices, all on T6 (GraceUI's G15: T6 alone calls
// GraceUI). surfaces.hpp lays them out; this calls GraceUI. Overlay (overlay.hpp) owns one and hands it its events.
//
// - At connect, on T1 (EldenNet's registration window): a small white texture of its own, for the solid shapes; its
//   faces, GraceUI's own files from its folder as GRACEUI_FACE_SCALED faces, before warm-up (GraceUI
//   eldennet-changes §33, §69, §94): EB Garamond and Noto Sans, and EB Garamond Italic where GraceUI's folder has it
//   (0.2.36's D241), or, with GraceUI's Plain text on at connect (O134; ABI 1.9's plain_text, D268), Noto Sans
//   Regular and SemiBold (surfaces.hpp's faces_of); the hotkeys talk (V, O152, with the pad chord D-pad Up then View
//   behind has_cap("pad-chords"), §47) and skip (Tab), by their stable action ids and players' names (§48, §49).
// - On T6, each event poll gives it: the talk hotkey opens (OpenChat, by the key or the pad chord as GraceUI's
//   hotkey_by says, D243), Tab skips the page up; while the box holds input, GraceUI's holder events (D176): keys,
//   characters and the pad to the box's own Dear ImGui context, Enter sends, Esc and the pad's B close, and the
//   holder's end closes with its reason; the display's event (D180), whose Plain text is noted and changes no face
//   until the next launch (D268); a hotkey changed (D210), whose words the hint takes from describe_hotkey.
// - On T6, after the core's step (`show`): the newest model drawn, each panel submitted at most every 100 ms; the box's
//   input asked for when the model opens it (request_input with the holder's events, the box's own panel shown first),
//   refused and not now answered as Close; the box closed by the model answered as Close{Shut} with its draft; the
//   caret set for the IME while the box holds input; and PageShown on a Speech page's first submitted frame.
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/overlay/surfaces.hpp"
#include "en/proto/ui.hpp"
#include "graceui_sys.hpp"

struct ImGuiContext;

namespace en::overlay {

inline constexpr std::uint32_t kKeyTalk = 0x56;   // VK 'V': the talk key's default (O152, for O94's T; NPC §5.1)
inline constexpr std::uint32_t kKeySkip = 0x09;   // VK_TAB: the page's skip (NPC §8.6)
inline constexpr std::string_view kActionTalk = "talk";
inline constexpr std::string_view kActionSkip = "skip-page";
inline constexpr std::string_view kNameTalk = "Talk";
inline constexpr std::string_view kNameSkip = "Skip the page";
// The chat box's pad chord (NPC §5.6, O11; GraceUI D204): D-pad Up, the anchor, then View (XInput's BACK).
inline constexpr std::uint32_t kTalkChord = GRACEUI_PAD_CHORD(0x0001u, 0x0020u);

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
  std::uint32_t faces = 0;    // EldenNet's faces registered (0 to 3)
  bool italic = false;        // the italic among them (GraceUI's folder has it from 0.2.36)
  GraceUiStatus face_status = GRACEUI_NOT_YET;  // the last register_face's answer (too_late after GraceUI's warm-up)
  std::uint32_t files = 0;    // font files read, of those the type names (one file serves two roles in Plain text)
  bool plain_cap = false;     // GraceUI says Plain text (has_cap("plain-text"), 0.2.61)
  surfaces::Type type = surfaces::Type::Serif;  // the faces' type for the session (O134)
  // The face each role draws with, by its file's name (surfaces.hpp's face_name), "graceui" for GraceUI's notices'
  // face where none of EldenNet's is registered; the name line and the Tarnished's lines fall back to the text's.
  std::string_view text = "graceui";
  std::string_view ui = "graceui";
  std::string_view name = "graceui";
  std::string_view tarnished = "graceui";
  const char* talk = "off";   // "bound", "unbound"
  const char* chord = "off";  // "asked" (with the key), "no_cap", "refused"
  bool wake = false;          // GraceUI's wait handle (ABI 1.5)
};

class Surfaces {
 public:
  // GraceUI's own font files, read by the DLL from GraceUI's folder: a file's bytes by its name there ("fonts\\..."),
  // empty where the folder has none. Asked only for the files of the type chosen at connect (faces_of).
  using Fonts = std::function<std::string(std::string_view file)>;
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
  // The faces' type chosen at connect, and GraceUI's Plain text as its display events have said it since, with how
  // many times it changed: a change is noted, and the faces stay as they are until the next launch (D268, O134).
  [[nodiscard]] surfaces::Type type() const noexcept { return type_; }
  [[nodiscard]] bool plain_text_now() const noexcept { return plain_now_; }
  [[nodiscard]] std::uint32_t plain_text_changes() const noexcept { return plain_changes_; }
  [[nodiscard]] bool holding() const noexcept { return holding_; }
  // The rollback choice's panel (MEM §9.1, O144): whether it holds input now, and the focused answer.
  [[nodiscard]] bool choice_holding() const noexcept { return choice_holding_; }
  [[nodiscard]] std::size_t choice_focus() const noexcept { return choice_focus_; }
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
  // The rollback choice's panel: made at its first use (so that a session without a choice makes no panel for it),
  // shown, given input as GRACEUI_HOLDER_MODAL with the holder's events, and its answer or its close sent back.
  void show_choice(const proto::UiModel& model, std::uint64_t now, std::vector<proto::UiEvent>& out);
  void take_choice(const GraceUiEvent& e, std::vector<proto::UiEvent>& out);
  void release_choice();
  void caret(const surfaces::Caret& c);
  [[nodiscard]] bool due(const Panel& p, std::uint64_t now) const noexcept;

  graceui::Api api_;
  GraceUiConsumer me_ = 0;
  std::uint64_t white_ = 0;
  std::uint32_t text_face_ = 0;
  std::uint32_t ui_face_ = 0;
  std::uint32_t italic_face_ = 0;  // 0: none, and the Tarnished's lines are drawn upright in the text's face
  std::uint32_t name_face_ = 0;    // 0: none (the serif's type), and the name line is the text's
  surfaces::Faces faces_;
  surfaces::Type type_ = surfaces::Type::Serif;
  bool plain_cap_ = false;
  bool plain_now_ = false;
  std::uint32_t plain_changes_ = 0;
  std::uint32_t talk_ = 0;
  std::uint32_t skip_ = 0;
  std::uint32_t talk_vk_ = kKeyTalk;
  std::string key_words_ = "V";
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
  bool pressed_by_key_ = true;  // ... and whether its key pressed it (not the pad chord alone), as far as GraceUI says
  bool hotkey_by_ = false;      // GraceUI says what pressed a hotkey (has_cap("hotkey-by"), ABI 1.7, D243)
  std::optional<Opened> opened_;
  Counts counts_;
  // The rollback choice.
  Panel choice_;
  surfaces::Display choice_for_;    // the display its panel was made for
  bool choice_holding_ = false;
  std::uint32_t choice_asked_ = 0;  // the panel id input was last asked for
  std::size_t choice_focus_ = 0;
  bool choice_dirty_ = false;       // the focus moved: drawn again
  std::optional<proto::ChoicePanel> last_choice_;
};

}  // namespace en::overlay
