#pragma once
// The GraceUI release EldenNet is built against (part 12; GraceUI D74, D83, D88): the ABI major it speaks and the
// oldest GraceUI it works with, which eldennet.dll names in `connect` and the launcher checks before it starts the game
// (ARCH §4.3). graceui-sys's headers are vendored from GraceUI at the same commit (third_party/graceui-sys).
#include <array>
#include <cstdint>
#include <string_view>

namespace en::proto::graceui_pin {

inline constexpr std::uint32_t kAbiMajor = 1;  // graceui.h's GRACEUI_ABI_MAJOR, which `connect` asks
// GraceUI 0.2.44 (D251, protocol 1.12): Docs' document block and Bug report's prose field and preview, which an older
// GraceUI's decoder refuses (a block kind it does not know fails the whole page data: the space would not register).
// It holds what the floor held before: the profile editor's (S7.2.b) topic families and master-detail (0.2.19, D215),
// slow commands (0.2.20, D216), the editor block (0.2.21, D217) and a save's chosen row (0.2.22, D218), on 0.2.17's
// wait handle (ABI 1.5, D212), which the chat box's opening within 50 ms needs (NPC §5.1); and Docs' search, the
// filter bar (0.2.43, D250), and the rail's foot (0.2.42, D248). The headers are GraceUI 0.2.46's (ABI 1.7, protocol
// 1.13; EldenNet uses neither's uploads, D226); what came after 0.2.22 but the pages is used only behind its has_cap,
// or, for 0.2.36's EB Garamond Italic (D241), where GraceUI's folder holds the file: without it the Tarnished's lines
// are drawn upright. 0.2.45's quick switches and live pages (D252, protocol 1.13) do not raise it: an older decoder
// skips the space's `quick` and a page's `live`, so a GraceUI from 0.2.44 shows the pages without them, and Pause AI,
// which only the quick switch sends, cannot be turned on there.
inline constexpr std::array<std::uint16_t, 3> kFloor = {0, 2, 44};
inline constexpr std::string_view kCommit = "fe7d9db";  // GraceUI's feat/foundations (D169 to D253), GraceUI 0.2.46

// Whether a GraceUI version (major, minor, patch) is at least the floor.
[[nodiscard]] constexpr bool at_least_floor(const std::array<std::uint16_t, 3>& v) noexcept {
  for (std::size_t i = 0; i < 3; ++i) {
    if (v[i] != kFloor[i]) return v[i] > kFloor[i];
  }
  return true;
}

}  // namespace en::proto::graceui_pin
