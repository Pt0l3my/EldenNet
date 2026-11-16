#pragma once
// R3 (DESIGN §2.6, ARCH §10.1): no exception crosses into foreign code. Every entry that foreign code calls (the
// loader, the game's task, GraceUI's callbacks, the callbacks of Windows and of libraries, the top of every thread we
// start) is noexcept and starts with ffi_guard: a catch-all handler, and a tripped flag per component that says an
// exception was caught there. What a tripped component means (a toast, a subsystem off) is its owner's decision.
//
// The flags are constant-initialised atomics: no dynamic initialiser runs under the loader lock and no destructor at
// exit (ARCH §3.1, §1.7).
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace en::proto {

// The components whose entries foreign code calls. Append only: a component keeps its value, and new ones take the
// room after the last (kComponentSlots).
enum class Component : std::uint8_t {
  DllMain,       // the loader's calls (ARCH §3.1)
  Task,          // the game's task callback (T2)
  Present,       // GraceUI's on_present callback (T3)
  Overlay,       // GraceUI's other callbacks to en-overlay
  LlmTransport,  // libcurl's and WinHTTP's callbacks, and WinHTTP's driver threads (en-llm)
  LlmWorker,     // the model handle's worker and dispatcher threads (en-llm)
  Tools,         // the eldennet-engine command's own entries, such as its signal handler
};

inline constexpr std::size_t kComponentSlots = 32;  // one flag each, named or not yet
static_assert(static_cast<std::size_t>(Component::Tools) < kComponentSlots, "a component past the flags' room");

const char* to_string(Component component) noexcept;

// Whether the component's guard has caught an exception since the process started, or since clear_trip().
[[nodiscard]] bool tripped(Component component) noexcept;
void trip(Component component) noexcept;
void clear_trip(Component component) noexcept;

// Runs `f` inside a catch-all handler and returns its answer; on any exception, trips `component` and returns
// `fallback`. For an entry that answers foreign code: a status, a byte count, a handle.
template <class T, class F>
[[nodiscard]] T ffi_guard(Component component, T fallback, F&& f) noexcept {
  static_assert(std::is_nothrow_move_constructible_v<T>, "the fallback leaves the handler by a move that cannot throw");
  try {
    return std::forward<F>(f)();
  } catch (...) {
    trip(component);
  }
  return fallback;
}

// The same for an entry that answers nothing.
template <class F>
void ffi_guard(Component component, F&& f) noexcept {
  try {
    std::forward<F>(f)();
  } catch (...) {
    trip(component);
  }
}

}  // namespace en::proto
