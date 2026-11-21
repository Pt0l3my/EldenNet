// SPDX-License-Identifier: MIT
// graceui_sys.hpp: a thin C++ wrapper over graceui_sys.h and ABI 1's table (D70, D136). Header-only, C++17 or later:
// every call checks that the table covers its slot (an older GraceUI's table may be shorter, D74) and answers
// GRACEUI_NOT_YET where it does not; strings are std::string_view; nothing allocates and nothing throws.
#pragma once

#include <cstdint>
#include <string_view>

#include "graceui_sys.h"

namespace graceui {

// A mod as connect names it (GraceUiConnectRequest).
struct Mod {
  std::string_view id;    // 3 to 32 of a-z, 0-9 and '-'
  std::string_view name;  // what players see
  std::uint32_t version = 0;
  std::uint32_t floor = 0;  // the oldest GraceUI it works with
  std::string_view caps;    // GRACEUI_CAP_* names separated by spaces
};

class Api {
 public:
  constexpr Api() noexcept = default;
  constexpr explicit Api(const GraceUiApi1* table) noexcept : table_(table) {}

  // Finds GraceUI as graceui_sys_find does: from the mod's own thread, never from DllMain.
  static Api find(std::uint32_t flags = GRACEUI_SYS_LOAD, std::uint32_t timeout_ms = GRACEUI_SYS_TIMEOUT_MS,
                  GraceUiSysFound* found = nullptr) noexcept {
    return Api(graceui_sys_find(flags, timeout_ms, found));
  }

  constexpr explicit operator bool() const noexcept { return table_ != nullptr; }
  constexpr const GraceUiApi1* table() const noexcept { return table_; }
  std::uint32_t version() const noexcept { return table_ != nullptr ? table_->graceui_version : 0; }

  bool has_cap(std::string_view cap) const noexcept {
    return GRACEUI_API1_COVERS(table_, has_cap) && table_->has_cap(cap.data(), cap.size()) != 0;
  }

  GraceUiStatus connect(const Mod& mod, GraceUiConnectAnswer& answer) const noexcept {
    answer = GraceUiConnectAnswer{};
    answer.size = sizeof answer;
    if (!GRACEUI_API1_COVERS(table_, connect)) {
      answer.status = GRACEUI_NOT_YET;
      return GRACEUI_NOT_YET;
    }
    GraceUiConnectRequest request{};
    request.size = sizeof request;
    request.abi_minor = GRACEUI_ABI1_MINOR;
    request.version = mod.version;
    request.floor = mod.floor;
    request.id = mod.id.data();
    request.id_length = mod.id.size();
    request.name = mod.name.data();
    request.name_length = mod.name.size();
    request.caps = mod.caps.data();
    request.caps_length = mod.caps.size();
    return table_->connect(&request, &answer);
  }

  GraceUiStatus disconnect(GraceUiConsumer consumer) const noexcept {
    return GRACEUI_API1_COVERS(table_, disconnect) ? table_->disconnect(consumer) : GRACEUI_NOT_YET;
  }

  // A notice: `key` 0 for a new one, else the mod's key, which replaces the notice showing under it, and withdraws it
  // with an empty text; `duration_ms` 0 until replaced or withdrawn.
  GraceUiStatus notice(GraceUiConsumer consumer, std::string_view text, std::uint32_t tone = GRACEUI_TONE_NORMAL,
                       std::uint32_t key = 0, std::uint32_t duration_ms = 0) const noexcept {
    if (!GRACEUI_API1_COVERS(table_, notice)) {
      return GRACEUI_NOT_YET;
    }
    GraceUiNotice in{};
    in.size = sizeof in;
    in.tone = tone;
    in.key = key;
    in.duration_ms = duration_ms;
    in.text = text.data();
    in.text_length = text.size();
    return table_->notice(consumer, &in);
  }

  // ---- Textures, faces and glyphs (D76, D80): textures until the mod's first submit, faces before warm-up.
  GraceUiStatus register_texture(GraceUiConsumer consumer, const GraceUiTextureDesc& texture,
                                 std::uint64_t& id) const noexcept {
    return GRACEUI_API1_COVERS(table_, register_texture) ? table_->register_texture(consumer, &texture, &id)
                                                         : GRACEUI_NOT_YET;
  }
  GraceUiStatus register_face(GraceUiConsumer consumer, const GraceUiFaceDesc& face, std::uint32_t& id) const noexcept {
    return GRACEUI_API1_COVERS(table_, register_face) ? table_->register_face(consumer, &face, &id) : GRACEUI_NOT_YET;
  }
  // GRACEUI_NOT_READY until the overlay's warm-up has baked the shared atlas: ask again later.
  GraceUiStatus glyph_table(GraceUiConsumer consumer, std::uint32_t face, GraceUiGlyphTable& table) const noexcept {
    table = GraceUiGlyphTable{};
    table.size = sizeof table;
    return GRACEUI_API1_COVERS(table_, glyph_table) ? table_->glyph_table(consumer, face, &table) : GRACEUI_NOT_YET;
  }

  // ---- Panels and draw lists (D76).
  GraceUiStatus create_panel(GraceUiConsumer consumer, const GraceUiPanelDesc& panel,
                             std::uint64_t& id) const noexcept {
    return GRACEUI_API1_COVERS(table_, create_panel) ? table_->create_panel(consumer, &panel, &id) : GRACEUI_NOT_YET;
  }
  GraceUiStatus submit(GraceUiConsumer consumer, std::uint64_t panel, const GraceUiDrawList& list) const noexcept {
    return GRACEUI_API1_COVERS(table_, submit) ? table_->submit(consumer, panel, &list) : GRACEUI_NOT_YET;
  }
  GraceUiStatus show_panel(GraceUiConsumer consumer, std::uint64_t panel) const noexcept {
    return GRACEUI_API1_COVERS(table_, show_panel) ? table_->show_panel(consumer, panel) : GRACEUI_NOT_YET;
  }
  GraceUiStatus hide_panel(GraceUiConsumer consumer, std::uint64_t panel) const noexcept {
    return GRACEUI_API1_COVERS(table_, hide_panel) ? table_->hide_panel(consumer, panel) : GRACEUI_NOT_YET;
  }

  // ---- Input (D77) and the world-state providers (D78).
  GraceUiStatus request_input(GraceUiConsumer consumer, const GraceUiInputRequest& request,
                              GraceUiInputAnswer& answer) const noexcept {
    answer = GraceUiInputAnswer{};
    answer.size = sizeof answer;
    return GRACEUI_API1_COVERS(table_, request_input) ? table_->request_input(consumer, &request, &answer)
                                                      : GRACEUI_NOT_YET;
  }
  GraceUiStatus release_input(GraceUiConsumer consumer) const noexcept {
    return GRACEUI_API1_COVERS(table_, release_input) ? table_->release_input(consumer) : GRACEUI_NOT_YET;
  }
  GraceUiStatus set_world_state(GraceUiConsumer consumer, std::uint32_t flags) const noexcept {
    GraceUiWorldState state{};
    state.size = sizeof state;
    state.flags = flags;
    return GRACEUI_API1_COVERS(table_, set_world_state) ? table_->set_world_state(consumer, &state)
                                                        : GRACEUI_NOT_YET;
  }

  // ---- Hotkeys and poll (D155; has_cap(GRACEUI_CAP_HOTKEYS)). A GraceUI before D155 answers not_yet to both.
  // A hotkey: `key` a Windows virtual key (0 for none), `modifiers` GRACEUI_MOD_*; GRACEUI_OK with its id, bound, or
  // GRACEUI_UNBOUND with its id, its default taken or none (D164): it fires nothing until the player binds it.
  GraceUiStatus register_hotkey(GraceUiConsumer consumer, std::string_view action, std::string_view name,
                                std::uint32_t key, std::uint32_t modifiers, std::uint32_t& id) const noexcept {
    if (!GRACEUI_API1_COVERS(table_, register_hotkey)) {
      return GRACEUI_NOT_YET;
    }
    GraceUiHotkeyDesc in{};
    in.size = sizeof in;
    in.key = key;
    in.modifiers = modifiers;
    in.action = action.data();
    in.action_length = action.size();
    in.name = name.data();
    in.name_length = name.size();
    return table_->register_hotkey(consumer, &in, &id);
  }
  // The mod's next event: GRACEUI_OK with one, its kind GRACEUI_EVENT_NONE once nothing is waiting. Never waits.
  // Hotkeys, and from GraceUI 0.2.1 a holder's events (D176): with no buffer, the space's events wait for a poll that
  // has one (below). GraceUI gives a holder's events and hotkeys before a space's, so a space's event that needs a
  // buffer (GRACEUI_FULL, kept) means nothing else waits.
  GraceUiStatus poll(GraceUiConsumer consumer, GraceUiEvent& event) const noexcept {
    if (!has_cap(GRACEUI_CAP_HOLDER_EVENTS)) {
      return poll(consumer, event, nullptr, 0);
    }
    event = GraceUiEvent{};
    event.size = sizeof event;
    const GraceUiStatus status = table_->poll(consumer, &event);
    if (status == GRACEUI_FULL) {
      event = GraceUiEvent{};
      event.size = sizeof event;
      return GRACEUI_OK;  // kind GRACEUI_EVENT_NONE: the space's event waits for a poll with a buffer
    }
    return status;
  }
  // The same, with `buffer` for a space's event's frame (D169): GRACEUI_FRAME_FROM_PAGE_MAX bytes always suffice;
  // GRACEUI_FULL with event.frame_length the size needed when they do not, the event kept for the next call.
  GraceUiStatus poll(GraceUiConsumer consumer, GraceUiEvent& event, char* buffer, std::uint64_t capacity) const noexcept {
    event = GraceUiEvent{};
    // A GraceUI before 0.2 knows only the event's first four fields: it is handed that much, and gives hotkeys alone.
    event.size = buffer != nullptr && has_cap(GRACEUI_CAP_PAGES) ? sizeof event : 16u;
    event.frame = buffer;
    event.frame_capacity = buffer != nullptr ? capacity : 0;
    return GRACEUI_API1_COVERS(table_, poll) && has_cap(GRACEUI_CAP_HOTKEYS) ? table_->poll(consumer, &event)
                                                                             : GRACEUI_NOT_YET;
  }

  // ---- A space (D169; has_cap(GRACEUI_CAP_PAGES)): its page data and settings description as JSON, its frames.
  GraceUiStatus register_pages(GraceUiConsumer consumer, std::uint32_t views, std::uint32_t version,
                               std::string_view pages, std::string_view settings,
                               std::string_view increment) const noexcept {
    if (!GRACEUI_API1_COVERS(table_, register_pages) || !has_cap(GRACEUI_CAP_PAGES)) {
      return GRACEUI_NOT_YET;
    }
    GraceUiSpaceDesc in{};
    in.size = sizeof in;
    in.views = views;
    in.version = version;
    in.pages = pages.data();
    in.pages_length = pages.size();
    in.settings = settings.data();
    in.settings_length = settings.size();
    in.increment = increment.data();
    in.increment_length = increment.size();
    return table_->register_pages(consumer, &in);
  }
  // A state or rows frame of the mod's own space.
  GraceUiStatus publish(GraceUiConsumer consumer, std::string_view frame) const noexcept {
    return GRACEUI_API1_COVERS(table_, publish) && has_cap(GRACEUI_CAP_PAGES)
               ? table_->publish(consumer, frame.data(), frame.size())
               : GRACEUI_NOT_YET;
  }
  // The result frame of the command whose event gave `reply`.
  GraceUiStatus answer(GraceUiConsumer consumer, std::uint64_t reply, std::string_view frame) const noexcept {
    return GRACEUI_API1_COVERS(table_, answer) && has_cap(GRACEUI_CAP_PAGES)
               ? table_->answer(consumer, reply, frame.data(), frame.size())
               : GRACEUI_NOT_YET;
  }

 private:
  const GraceUiApi1* table_ = nullptr;
};

}  // namespace graceui
