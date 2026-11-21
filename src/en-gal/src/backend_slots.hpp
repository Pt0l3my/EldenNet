#pragma once
// ErBackend's internals shared by backend.cpp and readers.cpp: its scratch slots and its per-tick item states.
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <span>
#include <vector>

#include "en/gal/backend.hpp"
#include "en/gal/roles.hpp"

namespace en::gal::slots {

// Preallocated copy buffers: one per item's object, then one per class reached through a pointer.
enum : std::size_t {
  kItems = 0,  // roles::kItemCount slots, one per item
  kPlayerData = roles::kItemCount,
  kSettings,
  kPlayer,
  kChr,
  kParts,
  kVitals,
  kMotion,
  kReaction,
  kGrip,
  kPopup,
  kView,
  kNode,
  kFmg,
  kFmgGroup,
  kParamTable,
  kParamRow,
  kRowData,
  kTaskGroup,
  kRenderChr,
  kRenderParts,
  kRenderMotion,
  kCount
};

// An item this tick: its static not read yet, read, its object copied and checked, failed, or null.
enum : std::uint8_t { kUnread = 0, kStaticRead = 1, kObjectOk = 2, kObjectFailed = 3, kNull = 4 };

}  // namespace en::gal::slots

namespace en::gal {

using namespace slots;

// The backend's preallocated buffers: one object slot each of the largest copy size, the near list, the player set,
// a flag block and a string.
class ErBackend::Scratch {
 public:
  Scratch(std::uint32_t object_bytes, std::uint32_t list_bytes)
      : object_(std::max<std::uint32_t>(object_bytes, 64)),
        objects_(static_cast<std::size_t>(object_) * kCount),
        list_(std::max<std::uint32_t>(list_bytes, 64)),
        players_(4096),
        block_(kFlagBlockBytes),
        text_(512) {}
  std::span<std::byte> slot(std::size_t i, std::uint32_t size) noexcept {
    return std::span<std::byte>(objects_).subspan(i * object_, std::min(size, object_));
  }
  std::span<std::byte> list() noexcept { return list_; }
  std::span<std::byte> players() noexcept { return players_; }
  std::span<std::byte> block() noexcept { return block_; }
  std::span<std::byte> text() noexcept { return text_; }

 private:
  std::uint32_t object_;
  std::vector<std::byte> objects_;
  std::vector<std::byte> list_;
  std::vector<std::byte> players_;
  std::vector<std::byte> block_;
  std::vector<std::byte> text_;
};

}  // namespace en::gal
