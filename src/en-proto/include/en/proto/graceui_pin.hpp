#pragma once
// The GraceUI release EldenNet is built against (part 12; GraceUI D74, D83, D88): the ABI major it speaks and the
// oldest GraceUI it works with, which eldennet.dll names in `connect` and the launcher checks before it starts the game
// (ARCH §4.3). graceui-sys's headers are vendored from GraceUI at the same commit (third_party/graceui-sys).
#include <array>
#include <cstdint>
#include <string_view>

namespace en::proto::graceui_pin {

inline constexpr std::uint32_t kAbiMajor = 1;  // graceui.h's GRACEUI_ABI_MAJOR, which `connect` asks
// GraceUI 0.2.55 (D262, protocol 1.18): System › Logs' Level filter with its own choices, in the owner's words (O133),
// which an older GraceUI's registration refuses on a bar it filters itself (its own choices were a paged table's alone,
// D254). It holds what the floor held before: 0.2.51's Tools › Workshop, in the browser alone, shown with Developer
// views, gated and with its ribbon (D258, protocol 1.17), whose four fields an older decoder drops without a word, so
// that the page would be drawn in game, ungated (an older registration compares no page data's minor); 0.2.50's filter
// bar on Logs' log tail (D257, protocol 1.16), which an older registration refuses; 0.2.49's header switch (D256, protocol 1.15: Home's Pause AI, which an older
// decoder would read as a plain button); Docs' document block and Bug report's prose field and preview (0.2.44, D251);
// the profile editor's (S7.2.b) topic families and master-detail (0.2.19, D215), slow commands (0.2.20, D216), the
// editor block (0.2.21, D217) and a save's chosen row (0.2.22, D218), on 0.2.17's wait handle (ABI 1.5, D212), which
// the chat box's opening within 50 ms needs (NPC §5.1); Docs' search, the filter bar (0.2.43, D250), and the rail's
// foot (0.2.42, D248); and 0.2.45's quick switches and live pages (D252, protocol 1.13). The headers are GraceUI
// 0.2.55's (ABI 1.8, protocol 1.18; EldenNet uses neither 1.6's uploads, D226, nor 1.8's on_present, D259); what came
// after 0.2.22 but the pages is used only behind its has_cap, or, for 0.2.36's EB Garamond Italic (D241), where
// GraceUI's folder holds the file: without it the Tarnished's lines are drawn upright.
inline constexpr std::array<std::uint16_t, 3> kFloor = {0, 2, 55};
inline constexpr std::string_view kCommit = "3623304";  // GraceUI's main (D169 to D262), GraceUI 0.2.55

// Whether a GraceUI version (major, minor, patch) is at least the floor.
[[nodiscard]] constexpr bool at_least_floor(const std::array<std::uint16_t, 3>& v) noexcept {
  for (std::size_t i = 0; i < 3; ++i) {
    if (v[i] != kFloor[i]) return v[i] > kFloor[i];
  }
  return true;
}

}  // namespace en::proto::graceui_pin
