#pragma once
// A fake GraceUI of our own for en-overlay's tests (the Windows builds): ABI 1's table, as graceui.h declares it, with
// the calls en-overlay makes answered as GraceUI 0.1 answers them (eldennet-changes §14 to §18) and checked as GraceUI
// checks them: connect's answers, notices under keys (replaced in place, withdrawn by an empty text, at most four),
// textures in the registration window, panels hidden until shown, hotkeys (a default another mod took registers
// unbound, GRACEUI_UNBOUND with its id, as GraceUI 3f1352b answers, D164), poll's events and submit's draw-list
// checks (counts, the bounds, every command's ranges and indices, clip rectangles inside the panel, known texture ids,
// the command stride). graceui-sys has no C mock; this is what stands in for one. One fake at a time (the table's
// functions are plain C pointers).
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
  bool bound = true;
};

struct Grace {
  // How it answers.
  GraceUiStatus connect_status = GRACEUI_OK;
  std::uint32_t connect_reason = 0;
  std::uint32_t version = GRACEUI_VERSION_PACK(0, 1, 0);
  std::vector<std::string> caps = {"notices", "panels", "textures", "faces", "input", "world-state", "hotkeys"};
  std::uint32_t table_size = sizeof(GraceUiApi1);  // a shorter table: an older GraceUI (D74)
  bool refuse_textures = false;
  GraceUiStatus poll_status = GRACEUI_OK;  // GRACEUI_NOT_CONNECTED: the mod was disconnected
  std::vector<std::uint32_t> taken_keys;   // virtual keys another mod's hotkey holds: these register unbound

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
  std::deque<GraceUiEvent> events;
  std::uint64_t submits = 0;
  std::uint64_t refused_lists = 0;
  std::string last_refusal;
  std::vector<std::uint32_t> world_calls;
  std::uint32_t polls = 0;

  GraceUiApi1 table{};

  Grace();
  ~Grace();
  Grace(const Grace&) = delete;
  Grace& operator=(const Grace&) = delete;

  const GraceUiApi1* api();  // the table, its size as `table_size` says
  // A press of a hotkey (by its action), as poll gives it: bound or not, as if the player had bound it since.
  void press(const std::string& action, std::uint32_t count = 1);
  std::uint64_t panel_of(const std::string& owner_hint = {}) const;  // the only panel made, or 0
};

}  // namespace fake
