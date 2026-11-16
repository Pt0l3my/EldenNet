#pragma once
// Our own fixed-capacity containers and flag sets (ARCH §2.3, §7.1): FixedVector<T, N> and FixedString<N> hold their
// items inline and never allocate, so a WorldSnapshot, a RenderFeed or a Lifecycle message can be built on the game
// thread and pass through a preallocated slot or ring (ARCH §1.3, R6). Nothing here throws: a container that is full
// refuses, and says so.
//
// They are trivially copyable when their items are, and they avoid pointer arithmetic (ARCH §2.4): every walk goes
// through std::span.
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace en::proto {

// ---- FixedVector ----

// Up to N items of T, held inline. Slots past size() hold T{}, so two vectors with the same items are equal whatever
// they held before. T must be default-constructible, and copy and move without throwing.
template <class T, std::size_t N>
class FixedVector {
  static_assert(N > 0 && N <= 0xFFFF, "a FixedVector holds 1 to 65,535 items");
  static_assert(std::is_default_constructible_v<T>, "a FixedVector's slots start as T{}");
  static_assert(std::is_nothrow_copy_assignable_v<T> && std::is_nothrow_move_assignable_v<T>,
                "a FixedVector's items copy and move without throwing");

 public:
  using value_type = T;
  using iterator = typename std::span<T>::iterator;
  using const_iterator = typename std::span<const T>::iterator;

  constexpr FixedVector() = default;

  [[nodiscard]] static constexpr std::size_t capacity() noexcept { return N; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] constexpr bool full() const noexcept { return size_ == N; }

  // The items, and nothing past them.
  [[nodiscard]] constexpr std::span<T> items() noexcept { return std::span<T>(items_).first(size_); }
  [[nodiscard]] constexpr std::span<const T> items() const noexcept {
    return std::span<const T>(items_).first(size_);
  }
  constexpr iterator begin() noexcept { return items().begin(); }
  constexpr iterator end() noexcept { return items().end(); }
  constexpr const_iterator begin() const noexcept { return items().begin(); }
  constexpr const_iterator end() const noexcept { return items().end(); }

  // Precondition: i < size(). get() checks instead, and answers null past the end.
  constexpr T& operator[](std::size_t i) noexcept { return items_[i]; }
  constexpr const T& operator[](std::size_t i) const noexcept { return items_[i]; }
  [[nodiscard]] constexpr T* get(std::size_t i) noexcept { return i < size_ ? &items_[i] : nullptr; }
  [[nodiscard]] constexpr const T* get(std::size_t i) const noexcept { return i < size_ ? &items_[i] : nullptr; }

  // Adds an item at the end; false, and nothing changed, when the vector is full.
  [[nodiscard]] constexpr bool try_push_back(const T& item) noexcept {
    if (full()) return false;
    items_[size_] = item;
    ++size_;
    return true;
  }
  [[nodiscard]] constexpr bool try_push_back(T&& item) noexcept {
    if (full()) return false;
    items_[size_] = std::move(item);
    ++size_;
    return true;
  }

  // Removes the last item, if there is one.
  constexpr void pop_back() noexcept {
    if (size_ == 0) return;
    --size_;
    items_[size_] = T{};
  }

  constexpr void clear() noexcept {
    for (T& item : items()) item = T{};
    size_ = 0;
  }

  friend constexpr bool operator==(const FixedVector& a, const FixedVector& b) {
    if (a.size_ != b.size_) return false;
    const std::span<const T> left = a.items();
    const std::span<const T> right = b.items();
    for (std::size_t i = 0; i < left.size(); ++i) {
      if (!(left[i] == right[i])) return false;
    }
    return true;
  }

 private:
  std::array<T, N> items_{};
  std::uint16_t size_ = 0;
};

// ---- UTF-8 ----

// Whether `text` is well-formed UTF-8: no stray continuation byte, no overlong form, no surrogate, nothing past
// U+10FFFF, no character cut at the end.
[[nodiscard]] constexpr bool valid_utf8(std::string_view text) noexcept {
  std::size_t i = 0;
  while (i < text.size()) {
    const auto b0 = static_cast<unsigned char>(text[i]);
    if (b0 < 0x80) {
      ++i;
      continue;
    }
    std::size_t len = 0;
    unsigned char lo = 0x80;
    unsigned char hi = 0xBF;  // the second byte's range, narrowed against overlong forms, surrogates and > U+10FFFF
    if (b0 >= 0xC2 && b0 <= 0xDF) {
      len = 2;
    } else if (b0 == 0xE0) {
      len = 3;
      lo = 0xA0;
    } else if (b0 == 0xED) {
      len = 3;
      hi = 0x9F;
    } else if (b0 >= 0xE1 && b0 <= 0xEF) {
      len = 3;
    } else if (b0 == 0xF0) {
      len = 4;
      lo = 0x90;
    } else if (b0 >= 0xF1 && b0 <= 0xF3) {
      len = 4;
    } else if (b0 == 0xF4) {
      len = 4;
      hi = 0x8F;
    } else {
      return false;
    }
    if (text.size() - i < len) return false;
    const auto b1 = static_cast<unsigned char>(text[i + 1]);
    if (b1 < lo || b1 > hi) return false;
    for (std::size_t k = 2; k < len; ++k) {
      const auto b = static_cast<unsigned char>(text[i + k]);
      if (b < 0x80 || b > 0xBF) return false;
    }
    i += len;
  }
  return true;
}

// The length of the longest prefix of valid UTF-8 `text` that is at most `max_bytes` long and ends on a whole
// character.
[[nodiscard]] constexpr std::size_t utf8_prefix(std::string_view text, std::size_t max_bytes) noexcept {
  if (text.size() <= max_bytes) return text.size();
  std::size_t end = max_bytes;
  while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) --end;  // back to a character's start
  return end;
}

// ---- FixedString ----

// Up to N bytes of well-formed UTF-8 with no NUL, held inline. It holds nothing else: what would not fit, or is not
// UTF-8, is refused and leaves the string as it was. Bytes past size() are zero.
template <std::size_t N>
class FixedString {
  static_assert(N > 0 && N <= 0xFFFF, "a FixedString holds 1 to 65,535 bytes");

 public:
  constexpr FixedString() = default;

  // The string that holds `text`, or nothing when try_assign would refuse it.
  [[nodiscard]] static constexpr std::optional<FixedString> from(std::string_view text) noexcept {
    FixedString s;
    if (!s.try_assign(text)) return std::nullopt;
    return s;
  }

  [[nodiscard]] static constexpr std::size_t capacity() noexcept { return N; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] constexpr std::string_view view() const noexcept { return std::string_view(bytes_.data(), size_); }

  // Takes `text` whole when it is UTF-8 of at most N bytes with no NUL; otherwise false, and nothing changes.
  [[nodiscard]] constexpr bool try_assign(std::string_view text) noexcept {
    if (text.size() > N || !acceptable(text)) return false;
    store(text);
    return true;
  }

  // Takes the longest prefix of `text` that fits and ends on a whole character. False, and nothing changes, when
  // `text` is not UTF-8 or holds a NUL.
  [[nodiscard]] constexpr bool assign_prefix(std::string_view text) noexcept {
    if (!acceptable(text)) return false;
    store(text.substr(0, utf8_prefix(text, N)));
    return true;
  }

  constexpr void clear() noexcept { store({}); }

  friend constexpr bool operator==(const FixedString& a, const FixedString& b) noexcept { return a.view() == b.view(); }
  friend constexpr bool operator==(const FixedString& a, std::string_view b) noexcept { return a.view() == b; }

 private:
  static constexpr bool acceptable(std::string_view text) noexcept {
    return text.find('\0') == std::string_view::npos && valid_utf8(text);
  }

  constexpr void store(std::string_view text) noexcept {
    for (std::size_t i = 0; i < N; ++i) bytes_[i] = i < text.size() ? text[i] : '\0';
    size_ = static_cast<std::uint16_t>(text.size());
  }

  std::array<char, N> bytes_{};
  std::uint16_t size_ = 0;
};

// ---- Flag sets ----

// A flag set is an enum class whose values are bits (ARCH §2.3): StatusSet, GroupSet. One opts in by specialising
// FlagSetTraits with `all`, the bits it defines; ~ keeps to them.
template <class E>
struct FlagSetTraits;

template <class E>
concept FlagSet = std::is_enum_v<E> && requires {
  { FlagSetTraits<E>::all } -> std::convertible_to<E>;
};

template <FlagSet E>
[[nodiscard]] constexpr auto bits_of(E e) noexcept {
  return static_cast<std::underlying_type_t<E>>(e);
}

template <FlagSet E>
[[nodiscard]] constexpr E operator|(E a, E b) noexcept {
  return static_cast<E>(bits_of(a) | bits_of(b));
}
template <FlagSet E>
[[nodiscard]] constexpr E operator&(E a, E b) noexcept {
  return static_cast<E>(bits_of(a) & bits_of(b));
}
template <FlagSet E>
[[nodiscard]] constexpr E operator^(E a, E b) noexcept {
  return static_cast<E>(bits_of(a) ^ bits_of(b));
}
template <FlagSet E>
[[nodiscard]] constexpr E operator~(E a) noexcept {
  return static_cast<E>(~bits_of(a) & bits_of(FlagSetTraits<E>::all));
}
template <FlagSet E>
constexpr E& operator|=(E& a, E b) noexcept {
  return a = a | b;
}
template <FlagSet E>
constexpr E& operator&=(E& a, E b) noexcept {
  return a = a & b;
}
template <FlagSet E>
constexpr E& operator^=(E& a, E b) noexcept {
  return a = a ^ b;
}

// Whether every bit of `bits` is in `set`; any() whether the set holds any bit.
template <FlagSet E>
[[nodiscard]] constexpr bool has_all(E set, E bits) noexcept {
  return (bits_of(set) & bits_of(bits)) == bits_of(bits);
}
template <FlagSet E>
[[nodiscard]] constexpr bool any(E set) noexcept {
  return bits_of(set) != 0;
}

}  // namespace en::proto
