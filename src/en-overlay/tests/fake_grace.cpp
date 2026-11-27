// The fake GraceUI's table (fake_grace.hpp).
#include "fake_grace.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <nlohmann/json.hpp>
#include <string_view>

#include <windows.h>

namespace fake {

namespace {

constexpr GraceUiConsumer kMe = 0x5EED;
constexpr std::uint64_t kAtlas = 0xA7;  // the shared atlas's texture id, once baked
constexpr std::uint32_t kNoticeFace = 1;
constexpr std::uint32_t kFirstModFace = 16;
Grace* g_fake = nullptr;

// The fake's glyph tables: an invented face per id, ASCII and a little more, 10 px advances at its baked size.
std::map<std::uint32_t, std::vector<GraceUiGlyph>>& tables() {
  static std::map<std::uint32_t, std::vector<GraceUiGlyph>> t;
  return t;
}

const std::vector<GraceUiGlyph>& glyphs_of(std::uint32_t face, float px) {
  auto& t = tables();
  auto it = t.find(face);
  if (it != t.end()) return it->second;
  std::vector<GraceUiGlyph> g;
  std::vector<std::uint32_t> cps;
  for (std::uint32_t c = 0x20; c < 0x7F; ++c) cps.push_back(c);
  for (const std::uint32_t c : {0xE9u, 0x2014u, 0x2026u, 0xFFFDu}) cps.push_back(c);
  for (const std::uint32_t c : cps) {
    GraceUiGlyph x{};
    x.code_point = c;
    x.advance = px / 2.0f;
    if (c != 0x20) {
      x.x0 = 1;
      x.y0 = 2;
      x.x1 = px / 2.0f - 1;
      x.y1 = px - 2;
      x.u0 = 0.0f;
      x.v0 = 0.0f;
      x.u1 = 0.01f;
      x.v1 = 0.01f;
    }
    g.push_back(x);
  }
  return t.emplace(face, std::move(g)).first->second;
}

void queue(Grace& g, const GraceUiEvent& e) {
  g.events.push_back(e);
  if (g.wake != nullptr) SetEvent(static_cast<HANDLE>(g.wake));
}

GraceUiEvent event_of(std::uint32_t kind) {
  GraceUiEvent e{};
  e.size = sizeof e;
  e.kind = kind;
  return e;
}

std::string key_words(std::uint32_t vk) {
  if ((vk >= 0x41 && vk <= 0x5A) || (vk >= 0x30 && vk <= 0x39)) return std::string(1, static_cast<char>(vk));
  if (vk >= 0x70 && vk <= 0x87) return "F" + std::to_string(vk - 0x6F);
  if (vk == 0x09) return "Tab";
  if (vk == 0x13) return "Pause";
  return "Key " + std::to_string(vk);
}

void end_holder(Grace& g, std::uint32_t why) {
  if (!g.holding) return;
  g.holding = false;
  g.holder_panel = 0;
  GraceUiEvent e = event_of(GRACEUI_EVENT_INPUT_ENDED);
  e.why = why;
  queue(g, e);
}

std::string text_of(const char* p, std::uint64_t n) { return p == nullptr ? std::string() : std::string(p, n); }

uint32_t has_cap(const char* name, uint64_t length) {
  const std::string cap = text_of(name, length);
  for (const std::string& c : g_fake->caps) {
    if (c == cap) return 1;
  }
  return 0;
}

// Whether this GraceUI has 0.2.61's Plain text for mods (D268).
bool plain_cap() { return has_cap(GRACEUI_CAP_PLAIN_TEXT, std::strlen(GRACEUI_CAP_PLAIN_TEXT)) != 0; }

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
  if (answer->size >= offsetof(GraceUiConnectAnswer, display_height) + sizeof answer->display_height) {  // ABI 1.2's
    answer->ui_scale = 1.25f;
    answer->ui_scale_default = 1.25f;
    answer->display_width = g.display_width;
    answer->display_height = g.display_height;
  }
  // ABI 1.9's (D268), with the cap: the player's Plain text; without it the mod's bytes are left as the mod set them.
  if (answer->size >= sizeof(GraceUiConnectAnswer) && plain_cap()) {
    answer->plain_text = g.plain_text;
  }
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
    const bool protocol = it->kind == GRACEUI_EVENT_WATCHED || it->kind == GRACEUI_EVENT_UNWATCHED ||
                          it->kind == GRACEUI_EVENT_COMMAND;
    if ((it->kind == GRACEUI_EVENT_DISPLAY || protocol) && size < sizeof(GraceUiEvent)) continue;
    if (protocol) {
      // Its frame into the mod's buffer; one too small answers full with the size needed, the event kept.
      const std::string& text = g.frames.front();
      event->frame_length = text.size();
      if (event->frame == nullptr || event->frame_capacity < text.size()) return GRACEUI_FULL;
      std::memcpy(event->frame, text.data(), text.size());
      event->kind = it->kind;
      event->reply = it->reply;
      g.frames.pop_front();
      g.events.erase(it);
      return GRACEUI_OK;
    }
    GraceUiEvent e = *it;
    g.events.erase(it);
    if (size >= sizeof(GraceUiEvent)) {
      char* frame = event->frame;
      const std::uint64_t capacity = event->frame_capacity;
      const std::uint32_t mods_plain = event->plain_text;
      *event = e;
      event->frame = frame;
      event->frame_capacity = capacity;
      if (!plain_cap()) event->plain_text = mods_plain;  // a GraceUI before 0.2.61's bytes
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

// A face before warm-up (D139): a file of some bytes (the fake reads no font), 6 to 128 px; too late after it. A mod
// may register two (GRACEUI_FACES_PER_MOD), and EldenNet (the id `eldennet`) eight (GRACEUI_FACES_ELDENNET).
GraceUiStatus register_face(GraceUiConsumer me, const GraceUiFaceDesc* face, uint32_t* id) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (g.warmed) {
    ++g.faces_too_late;
    return GRACEUI_TOO_LATE;
  }
  if (face == nullptr || id == nullptr || face->data == nullptr || face->data_length < 4 || face->pixel_height < 6 ||
      face->pixel_height > 128 || (face->flags & ~GRACEUI_FACE_SCALED) != 0) {
    return GRACEUI_REFUSED;
  }
  if (g.faces.size() >= (g.id == "eldennet" ? GRACEUI_FACES_ELDENNET : GRACEUI_FACES_PER_MOD)) return GRACEUI_FULL;
  g.faces.push_back(Face{face->pixel_height, face->flags, face->data_length});
  *id = kFirstModFace + static_cast<std::uint32_t>(g.faces.size() - 1);
  return GRACEUI_OK;
}

GraceUiStatus glyph_table(GraceUiConsumer me, uint32_t face, GraceUiGlyphTable* table) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (table == nullptr || table->size < sizeof(GraceUiGlyphTable)) return GRACEUI_REFUSED;
  if (!g.warmed) return GRACEUI_NOT_READY;
  float px = 15.0f;  // the notices' face
  if (face != kNoticeFace) {
    if (face < kFirstModFace || face - kFirstModFace >= g.faces.size()) return GRACEUI_REFUSED;
    px = static_cast<float>(g.faces[face - kFirstModFace].pixel_height);  // baked at a content scale of 1
  }
  const std::vector<GraceUiGlyph>& glyphs = glyphs_of(face, px);
  table->generation = 1;
  table->face_id = face;
  table->glyph_count = static_cast<std::uint32_t>(glyphs.size());
  table->pixel_height = px;
  table->ascent = px * 0.8f;
  table->descent = -px * 0.2f;
  table->line_gap = 0;
  table->atlas = kAtlas;
  table->glyphs = glyphs.data();
  ++g.glyph_tables;
  ++g.glyph_tables_of[face];
  return GRACEUI_OK;
}

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
    bool known = g.warmed && c.texture == kAtlas;
    for (std::uint64_t t : g.textures) known = known || t == c.texture;
    if (!known) return why = "texture", false;
    if (c.texture == kAtlas && l.atlas_generation != 1) return why = "generation", false;
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
  if (g.holding && g.holder_panel == panel) end_holder(g, GRACEUI_CLOSED_RELEASED);  // hiding it ends its holder
  return GRACEUI_OK;
}

// One holder at a time, answered at once (D77): the mod's own panel, shown, made to take input, on a layer that suits
// the kind; the holder's events only with GRACEUI_INPUT_EVENTS (D176).
GraceUiStatus request_input(GraceUiConsumer me, const GraceUiInputRequest* req, GraceUiInputAnswer* answer) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  ++g.requests;
  if (req == nullptr || answer == nullptr || req->size < sizeof(GraceUiInputRequest)) return GRACEUI_REFUSED;
  g.last_request = *req;
  const auto it = g.panels.find(req->panel);
  if (it == g.panels.end() || !it->second.shown || (it->second.desc.flags & GRACEUI_PANEL_TAKES_INPUT) == 0 ||
      req->kind != GRACEUI_HOLDER_PANEL || it->second.desc.layer == GRACEUI_LAYER_MODAL ||
      (req->flags & ~GRACEUI_INPUT_EVENTS) != 0) {
    return GRACEUI_REFUSED;
  }
  answer->answer = g.holding ? GRACEUI_INPUT_REFUSED : g.input_answer;
  if (answer->answer == GRACEUI_INPUT_REFUSED) {
    answer->holder_kind = GRACEUI_HOLDER_MENU;
    std::snprintf(answer->holder, sizeof answer->holder, "%s", g.holding ? "eldennet" : g.holder_name.c_str());
  } else if (answer->answer == GRACEUI_INPUT_NOT_NOW) {
    answer->why = g.not_now_why;
  } else {
    g.holding = true;
    g.holder_panel = req->panel;
  }
  return GRACEUI_OK;
}

GraceUiStatus release_input(GraceUiConsumer me) {
  Grace& g = *g_fake;
  if (me != kMe) return GRACEUI_NOT_CONNECTED;
  if (g.holding) ++g.releases;
  end_holder(g, GRACEUI_CLOSED_RELEASED);
  return GRACEUI_OK;
}

GraceUiStatus register_hotkey(GraceUiConsumer me, const GraceUiHotkeyDesc* desc, uint32_t* id) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (desc == nullptr || desc->size < sizeof(GraceUiHotkeyDesc) || desc->modifiers != 0 || desc->key == 0x1B) {
    return GRACEUI_REFUSED;
  }
  const bool chords = std::find(g.caps.begin(), g.caps.end(), "pad-chords") != g.caps.end();
  if (desc->pad_chord != 0 && !chords) return GRACEUI_NOT_YET;  // a GraceUI before 0.2.14 (D204)
  if (desc->pad_chord != 0 && ((desc->pad_chord >> 16) == 0 || (desc->pad_chord & 0xFFFFu) == 0 ||
                               (desc->pad_chord >> 16) == (desc->pad_chord & 0xFFFFu))) {
    return GRACEUI_REFUSED;  // two different buttons
  }
  const std::string action = text_of(desc->action, desc->action_length);
  for (const Hotkey& h : g.hotkeys) {
    if (h.action == action) return GRACEUI_REFUSED;
  }
  if (g.hotkeys.size() >= GRACEUI_HOTKEYS_PER_MOD) return GRACEUI_FULL;
  const bool taken = std::find(g.taken_keys.begin(), g.taken_keys.end(), desc->key) != g.taken_keys.end();
  g.hotkeys.push_back(Hotkey{action, text_of(desc->name, desc->name_length), desc->key, desc->pad_chord, !taken});
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
GraceUiStatus register_pages(GraceUiConsumer me, const GraceUiSpaceDesc* space) {
  Grace& g = *g_fake;
  if (me != kMe) return GRACEUI_NOT_CONNECTED;
  if (notice_only()) return GRACEUI_INACTIVE;  // no server beside an anti-cheat
  ++g.register_calls;
  if (space == nullptr || space->size < sizeof(GraceUiSpaceDesc) || space->views == 0 ||
      (space->views & ~(GRACEUI_VIEW_GAME | GRACEUI_VIEW_BROWSER)) != 0) {
    return GRACEUI_REFUSED;
  }
  if (!g.space_pages.empty()) return GRACEUI_DUPLICATE_ID;
  const std::string pages = text_of(space->pages, space->pages_length);
  const std::string settings = text_of(space->settings, space->settings_length);
  const nlohmann::json p = nlohmann::json::parse(pages, nullptr, false);
  const nlohmann::json s = nlohmann::json::parse(settings, nullptr, false);
  if (p.is_discarded() || s.is_discarded() || !p.is_object() || p.value("space", "") != g.id || !s.is_object() ||
      g.id == "graceui") {
    return GRACEUI_REFUSED;
  }
  // GraceUI 0.2.5's (D183): no topic named "pages", and a destructive action only with its confirmation's text.
  for (const nlohmann::json& t : p.value("topics", nlohmann::json::array())) {
    if (t.value("id", "") == "pages") return GRACEUI_REFUSED;
  }
  for (const nlohmann::json& page : p.value("pages", nlohmann::json::array())) {
    for (const nlohmann::json& a : page.value("actions", nlohmann::json::array())) {
      if (a.value("tone", "") == "destructive" && a.value("confirm", "").empty()) return GRACEUI_REFUSED;
    }
  }
  g.space_views = space->views;
  g.space_version = space->version;
  g.space_pages = pages;
  g.space_settings = settings;
  g.space_increment = text_of(space->increment, space->increment_length);
  return GRACEUI_OK;
}

GraceUiStatus publish(GraceUiConsumer me, const char* frame, uint64_t length) {
  Grace& g = *g_fake;
  if (me != kMe) return GRACEUI_NOT_CONNECTED;
  if (g.space_pages.empty()) return GRACEUI_NOT_YET;
  const nlohmann::json f = nlohmann::json::parse(text_of(frame, length), nullptr, false);
  bool declared = false;
  if (!f.is_discarded() && f.is_object() && (f.value("t", "") == "state" || f.value("t", "") == "rows")) {
    const std::string shape = f.value("t", "") == "state" ? "state" : "stream";
    const nlohmann::json pages = nlohmann::json::parse(g.space_pages);
    // A topic of its own, never a family itself; or a family's member, `<family>/<key>` (GraceUI 0.2.19, D215): the
    // key of a-z, 0-9, '.', '_' and '-', the whole id at most 64 bytes, a state.
    const std::string topic = f.value("topic", "");
    const std::size_t slash = topic.find('/');
    const std::string key = slash == std::string::npos ? std::string() : topic.substr(slash + 1);
    const bool good_key = !key.empty() && topic.size() <= 64 && std::all_of(key.begin(), key.end(), [](char c) {
      return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    });
    for (const nlohmann::json& t : pages["topics"]) {
      const bool family = t.value("family", false);
      declared = declared || (!family && t.value("id", "") == topic && t.value("shape", "") == shape) ||
                 (family && slash != std::string::npos && good_key && shape == "state" &&
                  t.value("id", "") == topic.substr(0, slash));
    }
  }
  // GraceUI 0.2.5's (D183): the settings topic's schema and description, if sent, are the registered ones.
  if (declared && f.value("topic", "") == "settings" && f.contains("body") && f["body"].is_object()) {
    const nlohmann::json registered = nlohmann::json::parse(g.space_settings);
    const nlohmann::json& body = f["body"];
    if ((body.contains("schema") && body["schema"] != registered["schema"]) ||
        (body.contains("description") && body["description"] != registered["settings"])) {
      declared = false;
    }
  }
  if (length > GRACEUI_FRAME_TO_PAGE_MAX || !declared || f.value("space", "") != g.id) {
    ++g.publish_refused;
    return GRACEUI_REFUSED;
  }
  g.published.push_back(text_of(frame, length));
  return GRACEUI_OK;
}

GraceUiStatus answer(GraceUiConsumer me, uint64_t reply, const char* frame, uint64_t length) {
  Grace& g = *g_fake;
  if (me != kMe) return GRACEUI_NOT_CONNECTED;
  const nlohmann::json f = nlohmann::json::parse(text_of(frame, length), nullptr, false);
  const bool result = !f.is_discarded() && f.is_object() && f.value("t", "") == "result" &&
                      (f.value("ok", false) || f.contains("error"));
  if (!result || reply < 100 || reply >= g.next_reply || g.answers.count(reply) != 0) {
    ++g.answer_refused;
    return GRACEUI_REFUSED;
  }
  g.answers[reply] = text_of(frame, length);
  return GRACEUI_OK;
}
// The caret, while its mod holds input and for the holding panel; one set otherwise is ignored (D176).
GraceUiStatus set_caret(GraceUiConsumer me, const GraceUiCaret* caret) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (caret == nullptr || caret->size < sizeof(GraceUiCaret)) return GRACEUI_REFUSED;
  if (!g.holding || caret->panel != g.holder_panel) {
    ++g.carets_ignored;
    return GRACEUI_OK;
  }
  g.carets.push_back(*caret);
  return GRACEUI_OK;
}

GraceUiStatus describe_hotkey(GraceUiConsumer me, uint32_t id, GraceUiHotkeyBinding* b) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (b == nullptr || b->size < sizeof(GraceUiHotkeyBinding) || id == 0 || id > g.hotkeys.size()) {
    return GRACEUI_REFUSED;
  }
  const Hotkey& h = g.hotkeys[id - 1];
  b->bound = h.bound ? (h.key != 0 ? GRACEUI_HOTKEY_KEY_BOUND : 0u) | (h.chord != 0 ? GRACEUI_HOTKEY_CHORD_BOUND : 0u)
                     : 0u;
  b->key = h.key;
  b->modifiers = 0;
  b->pad_chord = h.chord;
  std::snprintf(b->key_text, sizeof b->key_text, "%s", h.key != 0 ? key_words(h.key).c_str() : "");
  std::snprintf(b->chord_text, sizeof b->chord_text, "%s", h.chord != 0 ? "D-pad Up, then View" : "");
  return GRACEUI_OK;
}

GraceUiStatus wake_event(GraceUiConsumer me, void** event) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  if (event == nullptr) return GRACEUI_REFUSED;
  ++g.wakes_asked;
  if (g.wake == nullptr) g.wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  SetEvent(static_cast<HANDLE>(g.wake));  // once when asked for: the first wait polls what may be there already
  HANDLE mine = nullptr;
  DuplicateHandle(GetCurrentProcess(), static_cast<HANDLE>(g.wake), GetCurrentProcess(), &mine, SYNCHRONIZE, FALSE, 0);
  *event = mine;
  return GRACEUI_OK;
}

// ABI 1.8's (D259): EldenNet does not opt in to drawing a panel each frame, so any call is counted, and refused.
GraceUiStatus set_on_present(GraceUiConsumer me, uint64_t, const GraceUiPresentHook*) {
  Grace& g = *g_fake;
  if (const GraceUiStatus a = admitted(me); a != GRACEUI_OK) return a;
  ++g.present_sets;
  return GRACEUI_REFUSED;
}

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
  table.describe_hotkey = &describe_hotkey;
  table.wake_event = &wake_event;
  table.set_on_present = &set_on_present;
  tables().clear();
}

Grace::~Grace() {
  if (wake != nullptr) CloseHandle(static_cast<HANDLE>(wake));
  if (g_fake == this) g_fake = nullptr;
}

const GraceUiApi1* Grace::api() {
  table.size = table_size;
  table.graceui_version = version;
  return &table;
}

void Grace::press(const std::string& action, std::uint32_t count, std::uint32_t by) {
  if (holding) return;  // a hotkey fires only while no holder has input (D77)
  for (std::size_t i = 0; i < hotkeys.size(); ++i) {
    if (hotkeys[i].action == action) {
      GraceUiEvent e = event_of(GRACEUI_EVENT_HOTKEY);
      e.hotkey_id = static_cast<std::uint32_t>(i + 1);
      e.count = count;
      e.hotkey_by = std::find(caps.begin(), caps.end(), "hotkey-by") != caps.end() ? by : 0u;
      queue(*this, e);
    }
  }
}

void Grace::warm_up() {
  warmed = true;
  if (display_width == 0) {
    display_width = 1920;
    display_height = 1080;
  }
  display();
}

void Grace::key(std::uint32_t vk, bool down, bool repeat) {
  if (!holding) return;
  GraceUiEvent e = event_of(GRACEUI_EVENT_KEY);
  e.key = vk;
  e.pressed = down ? 1 : 0;
  e.repeat = repeat ? 1 : 0;
  queue(*this, e);
}

void Grace::type(const std::string& utf8) {
  if (!holding) return;
  for (std::size_t i = 0; i < utf8.size();) {
    const auto b = static_cast<unsigned char>(utf8[i]);
    const std::size_t n = b < 0x80 ? 1 : (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : 4;
    std::uint32_t c = n == 1 ? b : n == 2 ? (b & 0x1Fu) : n == 3 ? (b & 0x0Fu) : (b & 0x07u);
    for (std::size_t k = 1; k < n && i + k < utf8.size(); ++k) {
      c = (c << 6) | (static_cast<unsigned char>(utf8[i + k]) & 0x3Fu);
    }
    GraceUiEvent e = event_of(GRACEUI_EVENT_CHARACTER);
    e.code_point = c;
    queue(*this, e);
    i += n;
  }
}

void Grace::pad(std::uint32_t button, bool down) {
  if (!holding) return;
  GraceUiEvent e = event_of(GRACEUI_EVENT_PAD);
  e.button = button;
  e.pressed = down ? 1 : 0;
  queue(*this, e);
}

void Grace::end_input(std::uint32_t why) { end_holder(*this, why); }

void Grace::rebind(const std::string& action, std::uint32_t key) {
  for (std::size_t i = 0; i < hotkeys.size(); ++i) {
    if (hotkeys[i].action == action) {
      hotkeys[i].key = key;
      hotkeys[i].bound = true;
      GraceUiEvent e = event_of(GRACEUI_EVENT_HOTKEY_CHANGED);
      e.hotkey_id = static_cast<std::uint32_t>(i + 1);
      queue(*this, e);
    }
  }
}

bool Grace::woken() { return wake != nullptr && WaitForSingleObject(static_cast<HANDLE>(wake), 0) == WAIT_OBJECT_0; }

const Panel* Grace::holder() const {
  const auto it = panels.find(holder_panel);
  return holding && it != panels.end() ? &it->second : nullptr;
}

void Grace::watch(const std::string& topic) {
  GraceUiEvent e{};
  e.size = sizeof e;
  e.kind = GRACEUI_EVENT_WATCHED;
  events.push_back(e);
  frames.push_back(R"({"v":1,"t":"sub","space":"eldennet","id":0,"topic":")" + topic + "\"}");
}

void Grace::unwatch(const std::string& topic) {
  GraceUiEvent e{};
  e.size = sizeof e;
  e.kind = GRACEUI_EVENT_UNWATCHED;
  events.push_back(e);
  frames.push_back(R"({"v":1,"t":"unsub","space":"eldennet","id":0,"topic":")" + topic + "\"}");
}

std::uint64_t Grace::command(const std::string& name, const std::string& args_json) {
  GraceUiEvent e{};
  e.size = sizeof e;
  e.kind = GRACEUI_EVENT_COMMAND;
  e.reply = next_reply++;
  events.push_back(e);
  frames.push_back(R"({"v":1,"t":"cmd","space":"eldennet","id":)" + std::to_string(e.reply) + R"(,"name":")" + name +
                   R"(","args":)" + args_json + "}");
  return e.reply;
}

std::vector<std::string> Grace::published_of(const std::string& topic) const {
  std::vector<std::string> out;
  for (const std::string& f : published) {
    if (nlohmann::json::parse(f).value("topic", "") == topic) out.push_back(f);
  }
  return out;
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
  e.faces_scale = faces.empty() ? 0.0f : 1.0f;
  e.plain_text = plain_text;  // poll writes it only with the cap
  queue(*this, e);
}

void Grace::set_plain_text(bool on) {
  plain_text = on ? 1u : 0u;
  display();
}

std::uint64_t Grace::panel_of(const std::string&) const { return panels.size() == 1 ? panels.begin()->first : 0; }

}  // namespace fake
