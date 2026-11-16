#pragma once
// The render feed (ARCH §7.4; channel C3): what the overlay needs every frame to place labels over the characters the
// core tracks (SetTracked), stamped on the game thread. It is trivially copyable: it travels through a triple buffer.
#include <array>
#include <cstdint>
#include <type_traits>

#include <nlohmann/json.hpp>

#include "en/proto/fixed.hpp"
#include "en/proto/snapshot.hpp"

namespace en::proto {

// A tracked character's label anchor: its position (the overlay adds label_height_m) and whether it is still there.
struct Head {
  ChrHandle handle;
  std::array<float, 3> pos{};
  bool present = false;

  friend constexpr bool operator==(const Head&, const Head&) = default;
};

struct RenderFeed {
  std::uint64_t seq = 0;
  std::uint64_t qpc_us = 0;
  std::array<float, 16> view{};  // the camera's view matrix
  float fov_rad = 0;
  float aspect = 0;
  bool camera_ok = false;
  FixedVector<Head, 8> heads;

  friend constexpr bool operator==(const RenderFeed&, const RenderFeed&) = default;
};

static_assert(std::is_trivially_copyable_v<RenderFeed>, "the render feed travels through a triple buffer (C3)");

// JSON forms (game_json.cpp), as the snapshot's.
void to_json(nlohmann::json& j, const Head& v);
void from_json(const nlohmann::json& j, Head& v);
void to_json(nlohmann::json& j, const RenderFeed& v);
void from_json(const nlohmann::json& j, RenderFeed& v);

}  // namespace en::proto
