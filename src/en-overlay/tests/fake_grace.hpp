#pragma once
// A fake GraceUI of our own for en-overlay's tests (the Windows builds): ABI 1.9's table, as graceui.h declares it at
// GraceUI 7c26477, with the calls en-overlay makes answered as GraceUI 0.2.68 answers them (eldennet-changes §14 to
// §50, §94; not 1.6's uploaded files, D226, since EldenNet's space has no file command, nor 1.8's on_present, D259,
// which EldenNet does not opt in to: set_on_present is counted and refused) and checked as GraceUI checks them:
// connect's answers, notices under keys (replaced in place, withdrawn by an empty text, at most four), textures in the
// registration window, faces before warm-up and the glyph tables after it, panels hidden until shown, hotkeys (a
// default another mod took registers unbound, GRACEUI_UNBOUND with its id, D164; a pad chord, D204; describe_hotkey and
// a change, D210), input with a holder's events (one holder, its panel the mod's own, shown and taking input; its
// events only while it holds; its end once, last; hotkeys silent while it holds; the caret only while it holds; D77,
// D176), the wait handle (D212), Plain text at connect and in the display's event with its cap (D268), poll's events
// by the event's size (the display's only to ABI 1.2's, D180) and submit's
// draw-list checks (counts, the bounds, every command's ranges and indices, clip rectangles inside the panel, known
// texture ids, the atlas's generation, the command stride). graceui-sys has no C mock; this is what stands in for one.
// One fake at a time (the table's functions are plain C pointers).
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "graceui.h"

namespace fake {

struct Panel {
  GraceUiPanelDesc desc{};
  bool shown = false;
  std::uint64_t lists = 0;          // lists that passed the checks
  std::uint32_t last_vertices = 0;  // the last one's
  std::uint32_t last_commands = 0;
};

struct Hotkey {
  std::string action;
  std::string name;
  std::uint32_t key = 0;
  std::uint32_t chord = 0;
  bool bound = true;
};

struct Face {
  std::uint32_t pixel_height = 0;
  std::uint32_t flags = 0;
  std::uint64_t bytes = 0;
};

struct Grace {
  // How it answers.
  GraceUiStatus connect_status = GRACEUI_OK;
  std::uint32_t connect_reason = 0;
  std::uint32_t version = GRACEUI_VERSION_PACK(0, 2, 68);
  std::vector<std::string> caps = {"notices", "panels",        "textures", "faces",      "input",           "world-state",
                                   "hotkeys", "pages",         "holder-events", "display", "pad-chords",
                                   "hotkey-bindings", "wake", "hotkey-by", "on-present", "plain-text"};
  std::uint32_t table_size = sizeof(GraceUiApi1);  // a shorter table: an older GraceUI (D74)
  bool refuse_textures = false;
  GraceUiStatus poll_status = GRACEUI_OK;  // GRACEUI_NOT_CONNECTED: the mod was disconnected
  std::vector<std::uint32_t> taken_keys;   // virtual keys another mod's hotkey holds: these register unbound
  // request_input's answer: GRACEUI_INPUT_GRANTED, _REFUSED (by `holder_name`'s holder) or _NOT_NOW (`not_now_why`).
  std::uint32_t input_answer = GRACEUI_INPUT_GRANTED;
  std::string holder_name = "graceui";
  std::uint32_t not_now_why = GRACEUI_NOT_NOW_DANGER;
  std::uint32_t display_width = 0;  // connect's answer (ABI 1.2): 0 until the swap chain is bound
  std::uint32_t display_height = 0;
  // GraceUI's Plain text (ui.menu.plain_text; ABI 1.9, D268), the player's kept setting: in connect's answer and in
  // each display event, written only with the "plain-text" cap (an older GraceUI leaves the mod's bytes as they were).
  std::uint32_t plain_text = 0;

  // What it saw.
  std::string id, name, mod_caps;
  std::uint32_t floor = 0;
  std::uint32_t mod_version = 0;
  std::uint32_t connects = 0;
  std::map<std::uint32_t, std::string> notices;  // showing, by key
  std::vector<std::pair<std::uint32_t, std::string>> notice_calls;
  std::vector<std::uint64_t> textures;
  bool window_closed = false;  // the first submit closed the registration window
  std::uint32_t texture_too_late = 0;
  std::map<std::uint64_t, Panel> panels;
  std::vector<Hotkey> hotkeys;
  std::vector<Face> faces;
  std::uint32_t faces_too_late = 0;
  bool warmed = false;              // the atlas baked: faces too late, glyph tables ready
  std::uint32_t glyph_tables = 0;   // glyph_table calls answered ok
  std::map<std::uint32_t, std::uint32_t> glyph_tables_of;  // the same, by face id
  // Input (D77, D176).
  std::uint32_t requests = 0;
  GraceUiInputRequest last_request{};
  bool holding = false;
  std::uint64_t holder_panel = 0;
  std::uint32_t releases = 0;
  std::vector<GraceUiCaret> carets;  // set while holding
  std::uint32_t carets_ignored = 0;
  void* wake = nullptr;              // the wait handle's event, once asked for
  std::uint32_t wakes_asked = 0;
  std::uint32_t present_sets = 0;    // set_on_present calls (ABI 1.8, D259): EldenNet makes none
  std::deque<GraceUiEvent> events;
  std::uint64_t submits = 0;
  std::uint64_t refused_lists = 0;
  std::string last_refusal;
  std::vector<std::uint32_t> world_calls;
  std::uint32_t polls = 0;
  std::map<std::uint32_t, std::uint32_t> poll_sizes;  // polls by the event's size
  // The space (D169): what register_pages was handed, and every frame published and answered, checked as GraceUI 0.2
  // checks them (a state or rows frame of the mod's own space and a topic it declares; a result for a reply that is
  // pending, once).
  std::uint32_t register_calls = 0;
  std::uint32_t space_views = 0;
  std::uint32_t space_version = 0;
  std::string space_pages, space_settings, space_increment;
  std::vector<std::string> published;
  std::uint32_t publish_refused = 0;
  std::map<std::uint64_t, std::string> answers;  // by reply handle
  std::uint32_t answer_refused = 0;
  std::deque<std::string> frames;  // the protocol events' frames, in the order of their events
  std::uint64_t next_reply = 100;

  GraceUiApi1 table{};

  Grace();
  ~Grace();
  Grace(const Grace&) = delete;
  Grace& operator=(const Grace&) = delete;

  const GraceUiApi1* api();  // the table, its size as `table_size` says
  // A press of a hotkey (by its action), as poll gives it: bound or not, as if the player had bound it since. Silent
  // while a holder has input, as GraceUI's are (D77). `by`: what pressed it (GRACEUI_HOTKEY_BY_*, D243), written only
  // with the "hotkey-by" cap, as GraceUI 0.2.38 writes it (an older one leaves 0).
  void press(const std::string& action, std::uint32_t count = 1, std::uint32_t by = GRACEUI_HOTKEY_BY_KEY);
  // The overlay's warm-up: the atlas baked (faces too late after it, glyph tables ready), then the display's event.
  void warm_up();
  // A holder's events, while it holds (D176): a key, the characters of `utf8`, a pad button, and its end.
  void key(std::uint32_t vk, bool down, bool repeat = false);
  void type(const std::string& utf8);
  void pad(std::uint32_t button, bool down);
  void end_input(std::uint32_t why);
  // The player rebinds a hotkey on GraceUI's Hotkeys page (D210): its key, and the change's event.
  void rebind(const std::string& action, std::uint32_t key);
  // Whether the wait handle is set now (it is auto-reset: looking takes it).
  bool woken();
  const Panel* holder() const;
  // The display's event, as GraceUI 0.2.2 gives it after warm-up and after each change (D180), with Plain text as it is
  // now (0.2.61, D268).
  void display();
  // The player turns Plain text on or off in the session (GraceUI › Menu): the display's event follows (D268).
  void set_plain_text(bool on);
  // A view's sub, unsub and cmd, as poll gives them to the space; command answers the reply handle it gave.
  void watch(const std::string& topic);
  void unwatch(const std::string& topic);
  std::uint64_t command(const std::string& name, const std::string& args_json);
  // The published frames of `topic`, parsed, oldest first.
  std::vector<std::string> published_of(const std::string& topic) const;
  std::uint64_t panel_of(const std::string& owner_hint = {}) const;  // the only panel made, or 0
};

}  // namespace fake
