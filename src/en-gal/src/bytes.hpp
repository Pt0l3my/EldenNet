#pragma once
// en-gal's portable part reads numbers out of bytes it was handed, never out of memory it points at: little-endian
// values assembled from a span with every offset checked, and arithmetic that answers nothing rather than wrap
// (ARCH §2.4: checked arithmetic on values read from the game; no pointer arithmetic outside reader and sys).
// Internal to en-gal.
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace en::gal::bytes {

// a + b, or nothing if it would pass the type's range.
[[nodiscard]] constexpr std::optional<std::uint64_t> add(std::uint64_t a, std::uint64_t b) noexcept {
  if (b > std::numeric_limits<std::uint64_t>::max() - a) return std::nullopt;
  return a + b;
}

// a * b, or nothing if it would pass the type's range.
[[nodiscard]] constexpr std::optional<std::uint64_t> mul(std::uint64_t a, std::uint64_t b) noexcept {
  if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) return std::nullopt;
  return a * b;
}

// Whether [offset, offset + len) lies within `size`.
[[nodiscard]] constexpr bool within(std::uint64_t offset, std::uint64_t len, std::uint64_t size) noexcept {
  return offset <= size && len <= size - offset;
}

// The little-endian value of `N` bytes at `offset`, or nothing past the end.
template <class T>
[[nodiscard]] constexpr std::optional<T> le(std::span<const std::byte> data, std::uint64_t offset) noexcept {
  if (!within(offset, sizeof(T), data.size())) return std::nullopt;
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < sizeof(T); ++i) {
    value |= std::uint64_t{std::to_integer<std::uint8_t>(data[static_cast<std::size_t>(offset) + i])} << (8 * i);
  }
  return static_cast<T>(value);
}

[[nodiscard]] constexpr std::optional<std::uint16_t> u16(std::span<const std::byte> d, std::uint64_t at) noexcept {
  return le<std::uint16_t>(d, at);
}
[[nodiscard]] constexpr std::optional<std::uint32_t> u32(std::span<const std::byte> d, std::uint64_t at) noexcept {
  return le<std::uint32_t>(d, at);
}
[[nodiscard]] constexpr std::optional<std::uint64_t> u64(std::span<const std::byte> d, std::uint64_t at) noexcept {
  return le<std::uint64_t>(d, at);
}

// The byte at `at`, or nothing past the end.
[[nodiscard]] constexpr std::optional<std::uint8_t> u8(std::span<const std::byte> d, std::uint64_t at) noexcept {
  if (at >= d.size()) return std::nullopt;
  return std::to_integer<std::uint8_t>(d[static_cast<std::size_t>(at)]);
}

[[nodiscard]] constexpr bool is_power_of_two(std::uint64_t v) noexcept { return v != 0 && (v & (v - 1)) == 0; }

// v rounded up to a multiple of `align` (a power of two), or nothing past the range.
[[nodiscard]] constexpr std::optional<std::uint64_t> align_up(std::uint64_t v, std::uint64_t align) noexcept {
  const std::optional<std::uint64_t> bumped = add(v, align - 1);
  if (!bumped) return std::nullopt;
  return *bumped & ~(align - 1);
}

}  // namespace en::gal::bytes
