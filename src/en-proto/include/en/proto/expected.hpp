#pragma once
// Expected<T, E>: a value, or the error that stands in for it (ARCH §2.2, §2.4), our own for C++20, shaped like C++23's
// std::expected without its exceptions: nothing here throws, allocates or aborts, so en-gal, which may do none of
// those (ARCH §2.4), uses it too.
//
//   Expected<BuildProfile, GateError> gate(...);           // a function answers one or the other
//   if (!profile) return Unexpected{profile.error()};       // check first: the accessors below have preconditions
//   use(*profile);
//   Expected<void, StoreError> migrate(...); return {};     // success with no value
//
// Accessing the value of an error, or the error of a value, breaks a precondition, as with std::expected: callers
// check first (ARCH §2.4's lint, "no unchecked access to an Expected"). There is no value() that throws.
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

namespace en::proto {

// The error an Expected is made from: `return Unexpected{why};`.
template <class E>
class Unexpected {
  static_assert(std::is_object_v<E> && !std::is_array_v<E> && !std::is_const_v<E> && !std::is_volatile_v<E>,
                "an error is a plain object type");

 public:
  constexpr explicit Unexpected(const E& error) : error_(error) {}
  constexpr explicit Unexpected(E&& error) noexcept(std::is_nothrow_move_constructible_v<E>)
      : error_(std::move(error)) {}

  constexpr const E& error() const& noexcept { return error_; }
  constexpr E& error() & noexcept { return error_; }
  constexpr E&& error() && noexcept { return std::move(error_); }

  friend constexpr bool operator==(const Unexpected& a, const Unexpected& b) { return a.error_ == b.error_; }

 private:
  E error_;
};

template <class E>
Unexpected(E) -> Unexpected<E>;

namespace detail {
template <class T>
inline constexpr bool kIsUnexpected = false;
template <class E>
inline constexpr bool kIsUnexpected<Unexpected<E>> = true;
}  // namespace detail

// A value of T, or an error of E. Both must move without throwing, so that no assignment can leave it holding
// neither. It is trivially copyable when both are.
template <class T, class E>
class [[nodiscard]] Expected {
  static_assert(std::is_object_v<T> && !std::is_array_v<T> && !detail::kIsUnexpected<std::remove_cv_t<T>>,
                "the value is a plain object type");
  static_assert(std::is_object_v<E> && !std::is_array_v<E> && !std::is_const_v<E>, "the error is a plain object type");
  static_assert(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>,
                "an Expected's value and error move without throwing");

 public:
  using value_type = T;
  using error_type = E;

  // A default value, where T has a public default constructor.
  constexpr Expected() noexcept(std::is_nothrow_default_constructible_v<T>)
    requires std::is_default_constructible_v<T>
      : v_(std::in_place_index<0>) {}

  // A value: `return profile;`.
  template <class U = T>
    requires(std::is_constructible_v<T, U &&> && !std::is_same_v<std::remove_cvref_t<U>, Expected> &&
             !detail::kIsUnexpected<std::remove_cvref_t<U>> &&
             !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>)
  constexpr Expected(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>)  // NOLINT: implicit, as std::expected
      : v_(std::in_place_index<0>, std::forward<U>(value)) {}

  // An error: `return Unexpected{why};`.
  template <class G>
    requires std::is_constructible_v<E, const G&>
  constexpr Expected(const Unexpected<G>& error)  // NOLINT: implicit, as std::expected
      : v_(std::in_place_index<1>, error.error()) {}
  template <class G>
    requires std::is_constructible_v<E, G &&>
  constexpr Expected(Unexpected<G>&& error) noexcept(std::is_nothrow_constructible_v<E, G&&>)  // NOLINT: implicit
      : v_(std::in_place_index<1>, std::move(error).error()) {}

  // A value made in place, from the arguments of one of T's constructors.
  template <class... A>
    requires std::is_constructible_v<T, A&&...>
  constexpr explicit Expected(std::in_place_t, A&&... args) : v_(std::in_place_index<0>, std::forward<A>(args)...) {}

  [[nodiscard]] constexpr bool has_value() const noexcept { return v_.index() == 0; }
  constexpr explicit operator bool() const noexcept { return has_value(); }

  // Precondition: has_value().
  constexpr T& operator*() & noexcept { return *std::get_if<0>(&v_); }
  constexpr const T& operator*() const& noexcept { return *std::get_if<0>(&v_); }
  constexpr T&& operator*() && noexcept { return std::move(*std::get_if<0>(&v_)); }
  constexpr T* operator->() noexcept { return std::get_if<0>(&v_); }
  constexpr const T* operator->() const noexcept { return std::get_if<0>(&v_); }

  // Precondition: !has_value().
  constexpr E& error() & noexcept { return *std::get_if<1>(&v_); }
  constexpr const E& error() const& noexcept { return *std::get_if<1>(&v_); }
  constexpr E&& error() && noexcept { return std::move(*std::get_if<1>(&v_)); }

  // The value, or `fallback` made into a T when there is none.
  template <class U>
  constexpr T value_or(U&& fallback) const& {
    return has_value() ? **this : static_cast<T>(std::forward<U>(fallback));
  }
  template <class U>
  constexpr T value_or(U&& fallback) && {
    return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(fallback));
  }

  friend constexpr bool operator==(const Expected& a, const Expected& b) { return a.v_ == b.v_; }

 private:
  std::variant<T, E> v_;  // index 0: the value; 1: the error. Never valueless: both move without throwing
};

// Success with no value, or an error: `return {};` and `return Unexpected{why};`.
template <class E>
class [[nodiscard]] Expected<void, E> {
  static_assert(std::is_object_v<E> && !std::is_array_v<E> && !std::is_const_v<E>, "the error is a plain object type");
  static_assert(std::is_nothrow_move_constructible_v<E>, "an Expected's error moves without throwing");

 public:
  using value_type = void;
  using error_type = E;

  constexpr Expected() noexcept : v_(std::in_place_index<0>) {}
  template <class G>
    requires std::is_constructible_v<E, const G&>
  constexpr Expected(const Unexpected<G>& error)  // NOLINT: implicit, as std::expected
      : v_(std::in_place_index<1>, error.error()) {}
  template <class G>
    requires std::is_constructible_v<E, G &&>
  constexpr Expected(Unexpected<G>&& error) noexcept(std::is_nothrow_constructible_v<E, G&&>)  // NOLINT: implicit
      : v_(std::in_place_index<1>, std::move(error).error()) {}

  [[nodiscard]] constexpr bool has_value() const noexcept { return v_.index() == 0; }
  constexpr explicit operator bool() const noexcept { return has_value(); }

  // Precondition: !has_value().
  constexpr E& error() & noexcept { return *std::get_if<1>(&v_); }
  constexpr const E& error() const& noexcept { return *std::get_if<1>(&v_); }
  constexpr E&& error() && noexcept { return std::move(*std::get_if<1>(&v_)); }

  friend constexpr bool operator==(const Expected& a, const Expected& b) { return a.v_ == b.v_; }

 private:
  std::variant<std::monostate, E> v_;
};

}  // namespace en::proto
