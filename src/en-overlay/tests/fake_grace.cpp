// The fake GraceUI's table (fake_grace.hpp).
#include "fake_grace.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace fake {

namespace {

constexpr GraceUiConsumer kMe = 0x5EED;
Grace* g_fake = nullptr;

std::string text_of(const char* p, std::uint64_t n) { return p == nullptr ? std::string() : std::string(p, n); }

uint32_t has_cap(const char* name, uint64_t length) {
  const std::string cap = text_of(name, length);
  for (const std::string& c : g_fake->caps) {
    if (c == cap) return 1;
  }
  return 0;
}

GraceUiStatus connect(const GraceUiConnectRequest* request, GraceUiConnectAnswer* answer) {
  Grace& g = *g_fake;
  if (request == nullptr || answer == nullptr || request->size < sizeof(GraceUiConnectRequest)) return GRACEUI_REFUSED;
  ++g.connects;
  g.id = text_of(request->id, request->id_length);
  g.name = text_of(request->name, request->name_length);
  g.mod_caps = text_of(request->caps, request->caps_length);
  g.floor = request->floor;
  g.mod_version = request->version;
  answer->status = g.connect_status;
  answer->reason = g.connect_reason;
  answer->graceui_version = g.version;
  answer->abi_minor = GRACEUI_ABI1_MINOR;
  answer->floor = g.connect_status == GRACEUI_TOO_OLD ? request->floor : 0;
  answer->consumer = g.connect_status == GRACEUI_OK || g.connect_status == GRACEUI_INACTIVE ? kMe : 0;
  return g.connect_status;
}

GraceUiStatus disconnect(GraceUiConsumer) { return GRACEUI_OK; }

bool notice_only() { return g_fake->connect_status == GRACEUI_INACTIVE; }

GraceUiStatus admitted(GraceUiConsumer me) {
  if (me != kMe) return GRACEUI_NOT_CONNECTED;
  if (notice_only()) return GRACEUI_INACTIVE;
  return GRACEUI_OK;
}

GraceUiStatus poll(GraceUiConsumer me, GraceUiEvent* event) {
  Grace& g = *g_fake;
  ++g.polls;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (g.poll_status != GRACEUI_OK) return g.poll_status;
  // GraceUI takes any event from its first size (16 bytes, GraceUI 0.1's) and fills what that size holds; an event
  // smaller than 0.2's never gets the protocol's kinds, nor one smaller than ABI 1.2's the display's (D169, D180).
  if (event == nullptr || event->size < 16) return GRACEUI_REFUSED;
  const std::uint32_t size = event->size;
  ++g.poll_sizes[size];
  for (auto it = g.events.begin(); it != g.events.end(); ++it) {
    if (it->kind == GRACEUI_EVENT_DISPLAY && size < sizeof(GraceUiEvent)) continue;
    GraceUiEvent e = *it;
    g.events.erase(it);
    if (size >= sizeof(GraceUiEvent)) {
      char* frame = event->frame;
      const std::uint64_t capacity = event->frame_capacity;
      *event = e;
      event->frame = frame;
      event->frame_capacity = capacity;
    } else {
      std::memcpy(static_cast<void*>(event), &e, 16);
    }
    event->size = size;
    return GRACEUI_OK;
  }
  event->kind = GRACEUI_EVENT_NONE;
  return GRACEUI_OK;
}

GraceUiStatus register_texture(GraceUiConsumer me, const GraceUiTextureDesc* t, uint64_t* id) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (g.window_closed) {
    ++g.texture_too_late;
    return GRACEUI_TOO_LATE;
  }
  if (g.refuse_textures || t == nullptr || t->format != GRACEUI_TEXTURE_RGBA8 || t->width == 0 || t->height == 0 ||
      t->width > GRACEUI_TEXTURE_SIDE_MAX || t->height > GRACEUI_TEXTURE_SIDE_MAX || t->row_bytes < t->width * 4 ||
      t->pixels == nullptr) {
    return GRACEUI_REFUSED;
  }
  *id = 0x7E00 + g.textures.size();
  g.textures.push_back(*id);
  return GRACEUI_OK;
}

GraceUiStatus register_face(GraceUiConsumer, const GraceUiFaceDesc*, uint32_t*) { return GRACEUI_NOT_YET; }
GraceUiStatus glyph_table(GraceUiConsumer, uint32_t, GraceUiGlyphTable*) { return GRACEUI_NOT_READY; }

GraceUiStatus create_panel(GraceUiConsumer me, const GraceUiPanelDesc* desc, uint64_t* id) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (desc == nullptr || desc->width < 1 || desc->height < 1 || desc->width > 16384 || desc->height > 16384 ||
      (desc->layer != GRACEUI_LAYER_HUD && desc->layer != GRACEUI_LAYER_PANEL && desc->layer != GRACEUI_LAYER_MODAL) ||
      desc->anchor > GRACEUI_ANCHOR_BOTTOM_RIGHT) {
    return GRACEUI_REFUSED;
  }
  if (g.panels.size() >= GRACEUI_PANELS_PER_MOD) return GRACEUI_FULL;
  *id = 0xA000 + g.panels.size() + 1;
  g.panels[*id].desc = *desc;
  return GRACEUI_OK;
}

// GraceUI's checks on a list (graceui.h's submit; D76, D137, D152), as far as a fake needs them.
bool list_ok(const Grace& g, const Panel& p, const GraceUiDrawList& l, std::string& why) {
  if (l.size < sizeof(GraceUiDrawList)) return why = "size", false;
  if (l.command_size < sizeof(GraceUiDrawCommand) || l.command_size > GRACEUI_DRAW_COMMAND_SIZE_MAX) {
    return why = "command_size", false;
  }
  if (std::uint64_t{l.vertex_count} * sizeof(GraceUiDrawVert) > GRACEUI_DRAW_VERTEX_BYTES_MAX) {
    return why = "vertices", false;
  }
  if (std::uint64_t{l.index_count} * sizeof(uint16_t) > GRACEUI_DRAW_INDEX_BYTES_MAX) return why = "indices", false;
  if (l.command_count == 0 || l.command_count > GRACEUI_DRAW_COMMANDS_MAX) return why = "commands", false;
  if (l.vertices == nullptr || l.indices == nullptr || l.commands == nullptr) return why = "null", false;
  const auto* bytes = static_cast<const unsigned char*>(static_cast<const void*>(l.commands));
  for (uint32_t i = 0; i < l.command_count; ++i) {
    const auto& c = *static_cast<const GraceUiDrawCommand*>(static_cast<const void*>(bytes + i * l.command_size));
    if (c.vertex_count > 65536 || std::uint64_t{c.vertex_offset} + c.vertex_count > l.vertex_count) {
      return why = "vertex_range", false;
    }
    if (std::uint64_t{c.index_offset} + c.index_count > l.index_count || c.index_count % 3 != 0) {
      return why = "index_range", false;
    }
    for (uint32_t k = 0; k < c.index_count; ++k) {
      if (l.indices[c.index_offset + k] >= c.vertex_count) return why = "index", false;
    }
    if (!(c.clip_x0 >= 0 && c.clip_y0 >= 0 && c.clip_x1 <= p.desc.width && c.clip_y1 <= p.desc.height &&
          c.clip_x0 <= c.clip_x1 && c.clip_y0 <= c.clip_y1)) {
      return why = "clip", false;
    }
    bool known = false;
    for (std::uint64_t t : g.textures) known = known || t == c.texture;
    if (!known) return why = "texture", false;
  }
  return true;
}

GraceUiStatus submit(GraceUiConsumer me, uint64_t panel, const GraceUiDrawList* list) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  ++g.submits;
  g.window_closed = true;  // the first submit closes the registration window
  auto it = g.panels.find(panel);
  std::string why;
  if (it == g.panels.end() || list == nullptr || !list_ok(g, it->second, *list, why)) {
    ++g.refused_lists;
    g.last_refusal = it == g.panels.end() ? "panel" : list == nullptr ? "null" : why;
    return GRACEUI_REFUSED;
  }
  ++it->second.lists;
  it->second.last_vertices = list->vertex_count;
  it->second.last_commands = list->command_count;
  return GRACEUI_OK;
}

GraceUiStatus show_panel(GraceUiConsumer me, uint64_t panel) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  auto it = g.panels.find(panel);
  if (it == g.panels.end()) return GRACEUI_REFUSED;
  it->second.shown = true;
  return GRACEUI_OK;
}

GraceUiStatus hide_panel(GraceUiConsumer me, uint64_t panel) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  auto it = g.panels.find(panel);
  if (it == g.panels.end()) return GRACEUI_REFUSED;
  it->second.shown = false;
  return GRACEUI_OK;
}

GraceUiStatus request_input(GraceUiConsumer, const GraceUiInputRequest*, GraceUiInputAnswer*) {
  return GRACEUI_REFUSED;  // en-overlay never asks for input in v0.1 (D142)
}
GraceUiStatus release_input(GraceUiConsumer) { return GRACEUI_OK; }

GraceUiStatus register_hotkey(GraceUiConsumer me, const GraceUiHotkeyDesc* desc, uint32_t* id) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (desc == nullptr || desc->size < sizeof(GraceUiHotkeyDesc) || desc->modifiers != 0 || desc->key == 0x1B) {
    return GRACEUI_REFUSED;
  }
  if (desc->pad_chord != 0) return GRACEUI_NOT_YET;
  const std::string action = text_of(desc->action, desc->action_length);
  for (const Hotkey& h : g.hotkeys) {
    if (h.action == action) return GRACEUI_REFUSED;
  }
  if (g.hotkeys.size() >= GRACEUI_HOTKEYS_PER_MOD) return GRACEUI_FULL;
  const bool taken = std::find(g.taken_keys.begin(), g.taken_keys.end(), desc->key) != g.taken_keys.end();
  g.hotkeys.push_back(Hotkey{action, text_of(desc->name, desc->name_length), desc->key, !taken});
  *id = static_cast<uint32_t>(g.hotkeys.size());
  return taken ? GRACEUI_UNBOUND : GRACEUI_OK;  // registered either way, with its id (D164)
}

GraceUiStatus notice(GraceUiConsumer me, const GraceUiNotice* n) {
  Grace& g = *g_fake;
  if (me != kMe) return GRACEUI_NOT_CONNECTED;  // callable in the notice-only mode (D88)
  if (n == nullptr || n->text_length > GRACEUI_NOTICE_TEXT_MAX || (n->text_length == 0 && n->key == 0) ||
      n->tone > GRACEUI_TONE_DANGER || n->duration_ms != 0) {
    return GRACEUI_REFUSED;
  }
  const std::string text = text_of(n->text, n->text_length);
  g.notice_calls.emplace_back(n->key, text);
  if (text.empty()) {
    g.notices.erase(n->key);
    return GRACEUI_OK;
  }
  if (g.notices.count(n->key) == 0 && g.notices.size() >= GRACEUI_NOTICES_PER_MOD) return GRACEUI_FULL;
  g.notices[n->key] = text;
  return GRACEUI_OK;
}

GraceUiStatus set_world_state(GraceUiConsumer me, const GraceUiWorldState* state) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (state == nullptr || (state->flags & ~0x3Fu) != 0) return GRACEUI_REFUSED;
  g.world_calls.push_back(state->flags);
  return GRACEUI_OK;
}

GraceUiStatus log_line(GraceUiConsumer, uint32_t, const char*, uint64_t) { return GRACEUI_NOT_YET; }
GraceUiStatus register_pages(GraceUiConsumer, const GraceUiSpaceDesc*) { return GRACEUI_NOT_YET; }
GraceUiStatus publish(GraceUiConsumer, const char*, uint64_t) { return GRACEUI_NOT_YET; }
GraceUiStatus answer(GraceUiConsumer, uint64_t, const char*, uint64_t) { return GRACEUI_NOT_YET; }
GraceUiStatus set_caret(GraceUiConsumer, const GraceUiCaret*) { return GRACEUI_OK; }

}  // namespace

Grace::Grace() {
  g_fake = this;
  table.abi_minor = GRACEUI_ABI1_MINOR;
  table.has_cap = &has_cap;
  table.connect = &connect;
  table.disconnect = &disconnect;
  table.register_pages = &register_pages;
  table.publish = &publish;
  table.poll = &poll;
  table.answer = &answer;
  table.register_texture = &register_texture;
  table.register_face = &register_face;
  table.glyph_table = &glyph_table;
  table.create_panel = &create_panel;
  table.submit = &submit;
  table.show_panel = &show_panel;
  table.hide_panel = &hide_panel;
  table.request_input = &request_input;
  table.release_input = &release_input;
  table.register_hotkey = &register_hotkey;
  table.notice = &notice;
  table.set_world_state = &set_world_state;
  table.log = &log_line;
  table.set_caret = &set_caret;
}

Grace::~Grace() {
  if (g_fake == this) g_fake = nullptr;
}

const GraceUiApi1* Grace::api() {
  table.size = table_size;
  table.graceui_version = version;
  return &table;
}

void Grace::press(const std::string& action, std::uint32_t count) {
  for (std::size_t i = 0; i < hotkeys.size(); ++i) {
    if (hotkeys[i].action == action) {
      GraceUiEvent e{};
      e.size = sizeof e;
      e.kind = GRACEUI_EVENT_HOTKEY;
      e.hotkey_id = static_cast<std::uint32_t>(i + 1);
      e.count = count;
      events.push_back(e);
    }
  }
}

void Grace::display() {
  GraceUiEvent e{};
  e.size = sizeof e;
  e.kind = GRACEUI_EVENT_DISPLAY;
  e.display_width = 1920;
  e.display_height = 1080;
  e.ui_scale = 1.25f;
  e.ui_scale_default = 1.25f;
  e.scale = 1.25f;
  e.content_scale = 1.0f;
  events.push_back(e);
}

std::uint64_t Grace::panel_of(const std::string&) const { return panels.size() == 1 ? panels.begin()->first : 0; }

}  // namespace fake
