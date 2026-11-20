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
