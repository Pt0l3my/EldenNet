#pragma once
// The GraceUI release EldenNet is built against (part 12; GraceUI D74, D83, D88): the ABI major it speaks and the
// oldest GraceUI it works with, which eldennet.dll names in `connect` and the launcher checks before it starts the game
// (ARCH §4.3). graceui-sys's headers are vendored from GraceUI at the same commit (third_party/graceui-sys).
#include <array>
#include <cstdint>
#include <string_view>

namespace en::proto::graceui_pin {

inline constexpr std::uint32_t kAbiMajor = 1;                    // graceui.h's GRACEUI_ABI_MAJOR, which `connect` asks
inline constexpr std::array<std::uint16_t, 3> kFloor = {0, 2, 9};  // GraceUI 0.2.9: ABI 1.2, protocol 1.2, D197, D198
inline constexpr std::string_view kCommit = "908c179";             // GraceUI's main (D169 to D198)

// Whether a GraceUI version (major, minor, patch) is at least the floor.
[[nodiscard]] constexpr bool at_least_floor(const std::array<std::uint16_t, 3>& v) noexcept {
  for (std::size_t i = 0; i < 3; ++i) {
    if (v[i] != kFloor[i]) return v[i] > kFloor[i];
  }
  return true;
}

}  // namespace en::proto::graceui_pin
