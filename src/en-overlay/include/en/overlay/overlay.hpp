#pragma once
// en-overlay's connection to GraceUI (part 12; ARCH §1.4, §2.2, §3.2's Connect and ConnectB; GraceUI eldennet-changes
// §14 to §18, D131 to D156), in the Windows builds. Through graceui-sys alone: its look for GraceUI and
// GraceUI_RequestApi(1) (graceui_sys.h), ABI 1's calls behind `has_cap` and the table's size (graceui_sys.hpp), and its
// headless Dear ImGui helper for the F11 view (graceui_imgui.hpp). It links no hook library and no Dear ImGui backend,
// hooks nothing (R12; GraceUI's invariant 16), and nothing in it waits but `find`, on T1 (R6).
//
// The threads: T1 finds GraceUI and connects (`find`, `Overlay::connect`), before anything is submitted, so the view's
// atlas is registered in EldenNet's registration window, and registers EldenNet's space (its pages, space.hpp; D169);
// from then on one thread alone, EldenNet's core thread (T6), calls the rest: the banner and the pause badge as
// notices, the world-state claims, the Mark's hotkey, and `step`, which polls GraceUI's events (F11 and the Mark, and
// the space's: a topic watched or no longer, a view's command), answers each command at once through the space's one
// command path, publishes what the space has for its watched topics, opens and closes the view and, while it is open,
// builds a frame on Dear ImGui's core and submits it. The F11 view takes no input (D142): it shows, and F11 hides it.
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "en/overlay/chat.hpp"
#include "en/overlay/space.hpp"
#include "en/overlay/view.hpp"
#include "en/proto/ui.hpp"
#include "graceui_imgui.hpp"
#include "graceui_sys.hpp"

struct ImGuiContext;

namespace en::overlay {

inline constexpr std::string_view kId = "eldennet";  // EldenNet's id with GraceUI, and its space's (D136)
inline constexpr std::string_view kName = "EldenNet";
inline constexpr std::uint32_t kBannerKey = 1;    // the banner's notice (D134)
inline constexpr std::uint32_t kBadgeKey = 2;     // the pause badge's
inline constexpr std::uint32_t kContentKey = 5;   // the content's problems (3 and 4 are the toasts', surfaces.hpp)
inline constexpr std::uint32_t kKeyF11 = 0x7A;    // VK_F11: the debug view's default (D155)
inline constexpr std::uint32_t kKeyPause = 0x13;  // VK_PAUSE: the Mark's, the launcher's own key (O90)
inline constexpr std::string_view kActionView = "debug-view";
inline constexpr std::string_view kActionMark = "mark";
inline constexpr std::uint32_t kFindTimeoutMs = 30000;  // graceui-sys's bound on the look (EST)

// How the look for GraceUI went, for the log.
struct Found {
  graceui::Api api;            // null: no GraceUI that serves ABI 1
  std::uint32_t how = 0;       // GRACEUI_SYS_BY_NAME, _BY_SCAN or _BY_PATH; 0 for none
  std::uint32_t attempts = 0;  // looks made
  std::uint32_t version = 0;   // the GraceUI found: GraceUI_Version(), or 0
  bool installed = false;      // at the last look, a module named graceui.dll was loaded or GraceUI\graceui.dll lay
                               // in the game's folder (graceui-sys's own word, GraceUI D51, D164, D170)
  std::uint64_t ms = 0;        // how long the look took
};

// On T1, never in DllMain: graceui-sys's look (`graceui_sys_find`: by module name, an export scan, then
// GraceUI\graceui.dll in the game's folder, which it loads), again every 250 ms for at most `timeout_ms`, with
// GRACEUI_SYS_STOP_IF_ABSENT (GraceUI D164): when nothing answers, no copy is installed in the game's folder and no
// module is named graceui.dll, nothing can answer later, and the look ends at once, so a start without GraceUI is not
// held up 30 s (GraceUI comes only from that folder, D51, D83; a copy loaded later from elsewhere under another name is
// not waited for).
[[nodiscard]] Found find(std::uint32_t timeout_ms = kFindTimeoutMs) noexcept;

// What `connect` came to (GraceUI eldennet-changes §14 item 1).
enum class Outcome : std::uint8_t {
  Connected,    // ok: everything EldenNet asked for that has_cap names
  NoticesOnly,  // inactive (anti-cheat), with a handle: notices alone (D88)
  Absent,       // no GraceUI found
  TooOld,       // GraceUI is older than EldenNet's floor (GraceUI has shown its own notice)
  DuplicateId,  // another mod took the id "eldennet"
  Inactive,     // inactive for another reason: the overlay could not start, or turned itself off
  Refused,      // an argument failed its check (a defect of ours)
  NotYet,       // the table has no connect slot
};
[[nodiscard]] const char* to_string(Outcome o) noexcept;  // "connected", "notices_only", "absent", ...
[[nodiscard]] constexpr bool usable(Outcome o) noexcept {
  return o == Outcome::Connected || o == Outcome::NoticesOnly;
}

struct Options {
  std::array<std::uint16_t, 3> version{};  // EldenNet's own
  bool view = false;                       // debug.panel: the F11 view, its panel, atlas and hotkey (PAGES SP8)
  bool mark = false;                       // the Mark's hotkey at once (else `register_mark` later)
  // EldenNet's space (D169): the views to register it for (GRACEUI_VIEW_GAME, GRACEUI_VIEW_BROWSER), or 0 for none. The
  // banner path registers it for the game's view alone: the browser never gets it when the gate or the safety step
  // fails (PAGES §4.4, GraceUI §7.2 item 1).
  std::uint32_t space_views = 0;
  // The conversation's surfaces (chat.hpp; WHY§364, part C): the talk and skip hotkeys, the lower third, the chat box,
  // the hint and the toasts. `fonts` reads GraceUI's own font files from GraceUI's folder, those of the type connect
  // chooses (chat.hpp); empty, the surfaces draw GraceUI's notices' face.
  bool surfaces = false;
  Surfaces::Fonts fonts;
};

struct Report {
  Outcome outcome = Outcome::Absent;
  GraceUiStatus status = GRACEUI_NOT_YET;  // what connect answered
  std::uint32_t reason = 0;                // with inactive or refused: GRACEUI_INACTIVE_* or GRACEUI_REFUSED_*
  std::uint32_t graceui_version = 0;
  std::uint32_t floor = 0;                 // with too_old: the version EldenNet needs
  bool world = false;                      // has_cap names world-state
  bool hotkeys = false;                    // has_cap names hotkeys
  const char* view = "off";                // the F11 view: "off", "ready", "unbound" (made, its hotkey registered but
                                           // unbound: F11 was taken first, D164), or why not: "no_caps", "atlas",
                                           // "panel", "hotkey"
  bool mark = false;                       // the Mark's hotkey is registered, bound or not
  bool mark_bound = false;                 // and bound: only then does EldenNet ask the launcher for Pause
  GraceUiStatus pages = GRACEUI_NOT_YET;   // register_pages's answer; not_yet when not asked, or without the cap
  SurfacesReport surfaces;                 // the conversation's surfaces, when asked for
};

class Overlay {
 public:
  // On T1: connects through `api` as "eldennet", with EldenNet's floor and the caps it uses, and, connected, makes the
  // view if asked for (its own Dear ImGui context, its atlas registered as a texture before any submit, a hidden panel,
  // F11) and the Mark's hotkey if asked for. Null when nothing can be shown (`report` says why); the DLL keeps what it
  // answers for the process.
  [[nodiscard]] static Overlay* connect(const graceui::Api& api, const Options& options, Report& report);
  ~Overlay();  // the tests' own: the DLL never destroys it (R14)
  Overlay(const Overlay&) = delete;
  Overlay& operator=(const Overlay&) = delete;

  // ---- On T6 from here on.
  [[nodiscard]] bool notices_only() const noexcept { return notices_only_; }
  [[nodiscard]] bool view_ready() const noexcept { return panel_ != 0 && view_hotkey_ != 0; }  // bound or not
  [[nodiscard]] bool view_bound() const noexcept { return view_ready() && view_bound_; }
  [[nodiscard]] bool view_open() const noexcept { return open_; }
  [[nodiscard]] bool mark_registered() const noexcept { return mark_hotkey_ != 0; }  // bound or not
  [[nodiscard]] bool mark_bound() const noexcept { return mark_hotkey_ != 0 && mark_bound_; }
  [[nodiscard]] bool pages() const noexcept { return pages_; }  // EldenNet's space is registered
  [[nodiscard]] std::uint32_t graceui_version() const noexcept { return graceui_version_; }
  // The conversation's surfaces, or null (not asked for, or a cap missing).
  [[nodiscard]] Surfaces* surfaces() noexcept { return surfaces_.get(); }
  // GraceUI's wait handle for EldenNet's events (a Win32 HANDLE, ABI 1.5's wake_event, D212), or null: T6 waits on it.
  [[nodiscard]] void* wake() const noexcept { return surfaces_ ? surfaces_->wake() : nullptr; }

  // The banner: one notice under EldenNet's key, shown until replaced (duration 0); a new text replaces it in place and
  // an empty one withdraws it. The same text again calls nothing. Works in the notice-only mode too.
  GraceUiStatus banner(std::string_view text, std::uint32_t tone = GRACEUI_TONE_WARNING) noexcept;
  // The pause badge, likewise under its own key.
  GraceUiStatus badge(std::string_view text) noexcept;
  // EldenNet's claims as a world-state provider (world.hpp), sent when they change.
  GraceUiStatus world(std::uint32_t flags) noexcept;
  // A notice of its own under `key` (not the banner's, the badge's or the toasts'), shown for `duration_ms`: the
  // content's problems (NPC §4.7). Works in the notice-only mode too.
  GraceUiStatus note(std::uint32_t key, std::string_view text, std::uint32_t duration_ms) noexcept;
  // The Mark's hotkey, Pause, registered now (connected, with the hotkeys cap): GRACEUI_OK once registered and bound;
  // GRACEUI_UNBOUND once registered but unbound (D164: another mod's hotkey took Pause first), a success that carries
  // its id: the launcher's Pause then stays the Mark, and only a bound Mark asks for it. Asked again, it answers the
  // same without calling GraceUI.
  GraceUiStatus register_mark() noexcept;

  struct Step {
    std::uint32_t marks = 0;          // Mark presses taken from poll
    std::uint32_t view_presses = 0;   // F11 presses taken
    bool lost = false;                // GraceUI answered not connected or inactive: nothing more is shown
    GraceUiStatus refused = GRACEUI_OK;  // a submit's or a panel call's answer other than ok, if any
    // The space's (D169).
    std::uint32_t commands = 0;              // taken, each answered once: at once, or later (answer_later)
    std::uint32_t published = 0;             // frames GraceUI took
    GraceUiStatus page_refused = GRACEUI_OK;  // a publish's or an answer's answer other than ok, if any
    // The surfaces': what the player did (the talk key, Tab, the box's events), for the core, in order.
    std::vector<proto::UiEvent> ui;
  };
  // Polls every event waiting, passing over any kind it does not use: hotkeys (an odd count of F11 presses toggles the
  // view; the Mark's are counted) and, with the space registered, its events, each handed to `space` (a command is
  // answered at once with what the space's one command path answers, given `facts`); then publishes the frames the
  // space has for its watched topics at `now_ms`, and, while the view is open, builds a frame of `status` and submits
  // it. A frame past ABI 1's bounds is not submitted: the view keeps its last list.
  Step step(const ViewStatus& status, Space* space = nullptr, const SpaceFacts* facts = nullptr,
            std::uint64_t now_ms = 0);
  // After the core took the step's events: the newest model drawn by the surfaces (chat.hpp), and the events that
  // raised (PageShown, Close). Nothing without the surfaces, or once GraceUI has gone.
  [[nodiscard]] std::vector<proto::UiEvent> show(const proto::UiModel& model, std::uint64_t now_ms);
  // The answers the space has for commands it answers later (the profile editor's, characters.hpp), each handed to
  // GraceUI with its reply handle, once: step does it every wake, and the DLL once it has made a request, so that an
  // answer is not held until the next wake. Its count; nothing once GraceUI has gone.
  std::uint32_t answer_later(Space& space);
  // The overlay's own code faulted (ffi_guard tripped): the box's input given back at once.
  void fault() noexcept;

 private:
  Overlay(const graceui::Api& api, GraceUiConsumer me, bool notices_only, std::uint32_t version);
  bool make_view(const char*& why);
  bool build(const ViewStatus& status);
  GraceUiStatus notice(std::uint32_t key, std::string_view text, std::uint32_t tone,
                       std::array<char, GRACEUI_NOTICE_TEXT_MAX>& shown, std::uint32_t& shown_length) noexcept;

  graceui::Api api_;
  GraceUiConsumer me_ = 0;
  bool notices_only_ = false;
  std::uint32_t graceui_version_ = 0;
  bool caps_world_ = false;
  bool caps_hotkeys_ = false;
  bool lost_ = false;
  // The notices as shown (key 1 the banner, key 2 the badge), to call nothing when the text is the same.
  std::array<char, GRACEUI_NOTICE_TEXT_MAX> banner_{};
  std::uint32_t banner_length_ = 0;
  std::array<char, GRACEUI_NOTICE_TEXT_MAX> badge_{};
  std::uint32_t badge_length_ = 0;
  std::uint32_t world_ = 0;
  bool world_sent_ = false;
  // The view.
  ImGuiContext* imgui_ = nullptr;
  std::uint64_t panel_ = 0;
  std::uint32_t view_hotkey_ = 0;
  bool view_bound_ = false;
  std::uint32_t mark_hotkey_ = 0;
  bool mark_bound_ = false;
  bool open_ = false;
  ViewLines lines_;
  graceui::imgui::DrawList list_;
  // The space.
  bool pages_ = false;
  std::array<char, GRACEUI_FRAME_FROM_PAGE_MAX> frame_{};  // poll writes a space's event's frame here
  std::vector<std::string> frames_;                         // what the space has to publish, each step
  // The conversation's surfaces.
  std::unique_ptr<Surfaces> surfaces_;
};

}  // namespace en::overlay
