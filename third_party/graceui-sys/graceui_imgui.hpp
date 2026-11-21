// SPDX-License-Identifier: MIT
// graceui_imgui.hpp: a mod's own headless Dear ImGui, drawn on a GraceUI panel (D76, D144, D156). Header-only C++, for a
// mod that compiles Dear ImGui's core (1.93.0, GraceUI's version) with no backend and no hook (invariant 16): include
// imgui.h before this header, or let it include it.
//
//   graceui::imgui::prepare(io)                   once, after ImGui::CreateContext(): the flags GraceUI's renderer needs
//   graceui::imgui::register_atlas(api, me, io)   once, in the mod's registration window: its font atlas baked and
//                                                 registered as a texture of its own (no texture change during play)
//   graceui::imgui::DrawList list;  list.build(*ImGui::GetDrawData(), width, height);  api.submit(me, panel, list.view())
//                                                 every frame of the mod's own, on its own thread
//   graceui::imgui::feed(io, event)               from GraceUI 0.2.1 (ABI 1.1, D176): each holder event the mod polled,
//                                                 before its ImGui::NewFrame(), so InputText works in its panel
//
// A frame is built in the panel's own pixels: io.DisplaySize is the panel's width and height, and GraceUI puts the panel
// where its anchor says. Every ImDrawList of the frame is appended into one GraceUI list, each command keeping its vertex
// offset (GraceUI's renderer honours it, ImGuiBackendFlags_RendererHasVtxOffset), its clip rectangle moved into the
// panel's pixels and held within them. A frame that does not fit ABI 1's bounds (GRACEUI_DRAW_VERTEX_BYTES_MAX,
// GRACEUI_DRAW_INDEX_BYTES_MAX, GRACEUI_DRAW_COMMANDS_MAX) is not built: build() answers false, and the mod skips that
// frame (GraceUI would refuse it, and keep the panel's last list).
//
// SAFETY: the raw pointers here are Dear ImGui's own buffers, read for the counts its ImVector gives, and this list's
// own vectors, which view() hands to GraceUI's submit, which copies them before it returns.
#ifndef GRACEUI_IMGUI_HPP
#define GRACEUI_IMGUI_HPP

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <vector>

#include "graceui_sys.hpp"
#include "imgui.h"
#include "imgui_internal.h"  // ImFontAtlasBuildMain: a backend-less Dear ImGui bakes its atlas once, itself

namespace graceui::imgui {

static_assert(sizeof(ImDrawVert) == sizeof(GraceUiDrawVert) && sizeof(ImDrawIdx) == sizeof(std::uint16_t),
              "D76's layout is Dear ImGui's default one (ImDrawVert, 16-bit indices): no conversion is needed");

// The flags a headless Dear ImGui sets for GraceUI: commands keep their vertex offsets, and nothing is written to disk.
inline void prepare(ImGuiIO& io) noexcept {
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
  io.MouseDrawCursor = false;  // GraceUI's host draws the Grace pointer for a holder that takes the mouse (D178)
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
}

// Bakes the font atlas once, as it stands (add the fonts first), and registers it as one of the mod's textures, in its
// registration window (before its first submit): the texture's id, or 0 if GraceUI refused it. Dear ImGui is then told
// the atlas is on the GPU, so it never asks for an update during play (invariant 11).
inline std::uint64_t register_atlas(const Api& api, GraceUiConsumer me, ImGuiIO& io) noexcept {
  ImFontAtlasBuildMain(io.Fonts);
  ImTextureData* atlas = io.Fonts->TexData;
  if (atlas == nullptr || atlas->Pixels == nullptr || atlas->Format != ImTextureFormat_RGBA32) {
    return 0;
  }
  GraceUiTextureDesc desc{};
  desc.size = sizeof desc;
  desc.format = GRACEUI_TEXTURE_RGBA8;
  desc.width = static_cast<std::uint32_t>(atlas->Width);
  desc.height = static_cast<std::uint32_t>(atlas->Height);
  desc.row_bytes = desc.width * 4;
  desc.pixels = atlas->Pixels;
  std::uint64_t id = 0;
  if (api.register_texture(me, desc, id) != GRACEUI_OK) {
    return 0;
  }
  atlas->SetTexID(static_cast<ImTextureID>(id));
  atlas->SetStatus(ImTextureStatus_OK);
  return id;
}

// A virtual key (VK_*, which GRACEUI_EVENT_KEY carries) as Dear ImGui's key; ImGuiKey_None for one it has no name for.
// The numbers are Windows' own, written here so that this header needs no windows.h.
inline ImGuiKey key_of(std::uint32_t vk) noexcept {
  if (vk >= 0x30 && vk <= 0x39) {
    return static_cast<ImGuiKey>(ImGuiKey_0 + static_cast<int>(vk - 0x30));
  }
  if (vk >= 0x41 && vk <= 0x5A) {
    return static_cast<ImGuiKey>(ImGuiKey_A + static_cast<int>(vk - 0x41));
  }
  if (vk >= 0x60 && vk <= 0x69) {
    return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + static_cast<int>(vk - 0x60));
  }
  if (vk >= 0x70 && vk <= 0x87) {
    return static_cast<ImGuiKey>(ImGuiKey_F1 + static_cast<int>(vk - 0x70));
  }
  switch (vk) {
    case 0x08: return ImGuiKey_Backspace;
    case 0x09: return ImGuiKey_Tab;
    case 0x0D: return ImGuiKey_Enter;
    case 0x10: case 0xA0: return ImGuiKey_LeftShift;
    case 0xA1: return ImGuiKey_RightShift;
    case 0x11: case 0xA2: return ImGuiKey_LeftCtrl;
    case 0xA3: return ImGuiKey_RightCtrl;
    case 0x12: case 0xA4: return ImGuiKey_LeftAlt;
    case 0xA5: return ImGuiKey_RightAlt;
    case 0x13: return ImGuiKey_Pause;
    case 0x14: return ImGuiKey_CapsLock;
    case 0x1B: return ImGuiKey_Escape;
    case 0x20: return ImGuiKey_Space;
    case 0x21: return ImGuiKey_PageUp;
    case 0x22: return ImGuiKey_PageDown;
    case 0x23: return ImGuiKey_End;
    case 0x24: return ImGuiKey_Home;
    case 0x25: return ImGuiKey_LeftArrow;
    case 0x26: return ImGuiKey_UpArrow;
    case 0x27: return ImGuiKey_RightArrow;
    case 0x28: return ImGuiKey_DownArrow;
    case 0x2C: return ImGuiKey_PrintScreen;
    case 0x2D: return ImGuiKey_Insert;
    case 0x2E: return ImGuiKey_Delete;
    case 0x5B: return ImGuiKey_LeftSuper;
    case 0x5C: return ImGuiKey_RightSuper;
    case 0x5D: return ImGuiKey_Menu;
    case 0x6A: return ImGuiKey_KeypadMultiply;
    case 0x6B: return ImGuiKey_KeypadAdd;
    case 0x6D: return ImGuiKey_KeypadSubtract;
    case 0x6E: return ImGuiKey_KeypadDecimal;
    case 0x6F: return ImGuiKey_KeypadDivide;
    case 0x90: return ImGuiKey_NumLock;
    case 0x91: return ImGuiKey_ScrollLock;
    case 0xBA: return ImGuiKey_Semicolon;
    case 0xBB: return ImGuiKey_Equal;
    case 0xBC: return ImGuiKey_Comma;
    case 0xBD: return ImGuiKey_Minus;
    case 0xBE: return ImGuiKey_Period;
    case 0xBF: return ImGuiKey_Slash;
    case 0xC0: return ImGuiKey_GraveAccent;
    case 0xDB: return ImGuiKey_LeftBracket;
    case 0xDC: return ImGuiKey_Backslash;
    case 0xDD: return ImGuiKey_RightBracket;
    case 0xDE: return ImGuiKey_Apostrophe;
    case 0xE2: return ImGuiKey_Oem102;
    default: return ImGuiKey_None;
  }
}

// One XInput button's mask (XINPUT_GAMEPAD_*, which GRACEUI_EVENT_PAD carries) as Dear ImGui's gamepad key.
inline ImGuiKey pad_key_of(std::uint32_t button) noexcept {
  switch (button) {
    case 0x0001: return ImGuiKey_GamepadDpadUp;
    case 0x0002: return ImGuiKey_GamepadDpadDown;
    case 0x0004: return ImGuiKey_GamepadDpadLeft;
    case 0x0008: return ImGuiKey_GamepadDpadRight;
    case 0x0010: return ImGuiKey_GamepadStart;
    case 0x0020: return ImGuiKey_GamepadBack;
    case 0x0040: return ImGuiKey_GamepadL3;
    case 0x0080: return ImGuiKey_GamepadR3;
    case 0x0100: return ImGuiKey_GamepadL1;
    case 0x0200: return ImGuiKey_GamepadR1;
    case 0x1000: return ImGuiKey_GamepadFaceDown;
    case 0x2000: return ImGuiKey_GamepadFaceRight;
    case 0x4000: return ImGuiKey_GamepadFaceLeft;
    case 0x8000: return ImGuiKey_GamepadFaceUp;
    default: return ImGuiKey_None;
  }
}

// One holder event (GRACEUI_EVENT_KEY to GRACEUI_EVENT_PAD, polled with an event of ABI 1.1's size) as the mod's own
// context's input: true if it was one of those kinds. Call it for each event before ImGui::NewFrame(), on the thread
// that owns the context. Keys come with their modifiers; characters as typed (InputText takes them); the pointer in the
// panel's pixels, as the panel's frame is built (a pointer outside the panel is outside every window); wheel notches;
// a pad's buttons as Dear ImGui's gamepad keys (a mod that wants pad navigation sets ImGuiConfigFlags_NavEnableGamepad).
// The end of the holder clears every key and button held and moves the pointer off the panel, so nothing stays held or
// hovered while the mod does not hold input; the mod asks again before it takes the player's input again.
inline bool feed(ImGuiIO& io, const GraceUiEvent& event) noexcept {
  if (event.size < sizeof(GraceUiEvent)) {
    return false;
  }
  switch (event.kind) {
    case GRACEUI_EVENT_KEY: {
      io.AddKeyEvent(ImGuiMod_Ctrl, (event.modifiers & GRACEUI_MOD_CTRL) != 0);
      io.AddKeyEvent(ImGuiMod_Shift, (event.modifiers & GRACEUI_MOD_SHIFT) != 0);
      io.AddKeyEvent(ImGuiMod_Alt, (event.modifiers & GRACEUI_MOD_ALT) != 0);
      const ImGuiKey key = key_of(event.key);
      if (key != ImGuiKey_None) {
        io.AddKeyEvent(key, event.pressed != 0);  // an auto-repeat is a press again, as Dear ImGui's Win32 backend sends
        io.SetKeyEventNativeData(key, static_cast<int>(event.key), -1);
      }
      return true;
    }
    case GRACEUI_EVENT_CHARACTER:
      io.AddInputCharacter(event.code_point);
      return true;
    case GRACEUI_EVENT_POINTER:
      io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
      io.AddMousePosEvent(event.x, event.y);
      if (event.button_event != 0 && event.button <= GRACEUI_BUTTON_X2) {
        io.AddMouseButtonEvent(static_cast<int>(event.button), event.pressed != 0);
      }
      if (event.wheel_x != 0.0f || event.wheel_y != 0.0f) {
        io.AddMouseWheelEvent(-event.wheel_x, event.wheel_y);  // Dear ImGui's horizontal wheel is positive leftwards
      }
      return true;
    case GRACEUI_EVENT_PAD: {
      io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
      const ImGuiKey key = pad_key_of(event.button);
      if (key != ImGuiKey_None) {
        io.AddKeyEvent(key, event.pressed != 0);
      }
      return true;
    }
    case GRACEUI_EVENT_INPUT_ENDED:
      io.AddFocusEvent(false);  // Dear ImGui clears every key and button held at its next frame
      io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
      return true;
    default:
      return false;
  }
}

// One GraceUI draw list from a frame of Dear ImGui's. Its vectors keep their room from frame to frame.
class DrawList {
 public:
  // From `data` (ImGui::GetDrawData() after ImGui::Render()) for a panel of `width` x `height` pixels; false, with the
  // list emptied, if the frame does not fit ABI 1's bounds. `atlas_generation` is the shared atlas's generation, for a
  // frame that draws glyphs of GraceUI's shared atlas (glyph_table); 0 otherwise.
  bool build(const ImDrawData& data, float width, float height, std::uint32_t atlas_generation = 0) {
    clear();
    std::size_t vertices = 0;
    std::size_t indices = 0;
    std::size_t commands = 0;
    for (const ImDrawList* list : data.CmdLists) {
      vertices += static_cast<std::size_t>(list->VtxBuffer.Size);
      indices += static_cast<std::size_t>(list->IdxBuffer.Size);
      for (const ImDrawCmd& cmd : list->CmdBuffer) {
        commands += cmd.UserCallback == nullptr && cmd.ElemCount != 0 ? 1 : 0;
      }
    }
    if (vertices * sizeof(GraceUiDrawVert) > GRACEUI_DRAW_VERTEX_BYTES_MAX ||
        indices * sizeof(std::uint16_t) > GRACEUI_DRAW_INDEX_BYTES_MAX || commands > GRACEUI_DRAW_COMMANDS_MAX) {
      return false;
    }
    vertices_.reserve(vertices);
    indices_.reserve(indices);
    commands_.reserve(commands);
    for (const ImDrawList* list : data.CmdLists) {
      const auto vertex_base = static_cast<std::uint32_t>(vertices_.size());
      const auto index_base = static_cast<std::uint32_t>(indices_.size());
      const auto list_vertices = static_cast<std::uint32_t>(list->VtxBuffer.Size);
      vertices_.resize(vertex_base + list_vertices);
      if (list_vertices != 0) {
        std::memcpy(vertices_.data() + vertex_base, list->VtxBuffer.Data, list_vertices * sizeof(ImDrawVert));
      }
      indices_.insert(indices_.end(), list->IdxBuffer.Data, list->IdxBuffer.Data + list->IdxBuffer.Size);
      for (const ImDrawCmd& cmd : list->CmdBuffer) {
        if (cmd.UserCallback != nullptr || cmd.ElemCount == 0) {
          continue;  // a callback is the mod's own business: GraceUI draws vertices only
        }
        GraceUiDrawCommand command{};
        command.clip_x0 = std::clamp(cmd.ClipRect.x - data.DisplayPos.x, 0.0f, width);
        command.clip_y0 = std::clamp(cmd.ClipRect.y - data.DisplayPos.y, 0.0f, height);
        command.clip_x1 = std::clamp(cmd.ClipRect.z - data.DisplayPos.x, command.clip_x0, width);
        command.clip_y1 = std::clamp(cmd.ClipRect.w - data.DisplayPos.y, command.clip_y0, height);
        command.texture = static_cast<std::uint64_t>(cmd.GetTexID());
        command.vertex_offset = vertex_base + cmd.VtxOffset;
        command.vertex_count = std::min<std::uint32_t>(list_vertices - cmd.VtxOffset, 65536u);
        command.index_offset = index_base + cmd.IdxOffset;
        command.index_count = cmd.ElemCount;
        commands_.push_back(command);
      }
    }
    generation_ = atlas_generation;
    return true;
  }

  // The list as submit takes it: valid until the next build() or clear().
  GraceUiDrawList view() const noexcept {
    GraceUiDrawList list{};
    list.size = sizeof list;
    list.atlas_generation = generation_;
    list.vertex_count = static_cast<std::uint32_t>(vertices_.size());
    list.index_count = static_cast<std::uint32_t>(indices_.size());
    list.command_count = static_cast<std::uint32_t>(commands_.size());
    list.command_size = sizeof(GraceUiDrawCommand);
    list.vertices = vertices_.data();
    list.indices = indices_.data();
    list.commands = commands_.data();
    return list;
  }

  void clear() noexcept {
    vertices_.clear();
    indices_.clear();
    commands_.clear();
    generation_ = 0;
  }

  bool empty() const noexcept { return commands_.empty(); }

 private:
  std::vector<GraceUiDrawVert> vertices_;
  std::vector<std::uint16_t> indices_;
  std::vector<GraceUiDrawCommand> commands_;
  std::uint32_t generation_ = 0;
};

}  // namespace graceui::imgui

#endif  // GRACEUI_IMGUI_HPP
