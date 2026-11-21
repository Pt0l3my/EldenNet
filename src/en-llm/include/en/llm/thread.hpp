#pragma once
// Threads with a stack of a given size, which std::thread cannot set (ARCH §10.2: the net's threads run on 2 MB
// stacks): _beginthreadex on Windows, pthread_attr_setstacksize elsewhere. A thread's first frame is an entry from the
// system, so it is noexcept and starts with ffi_guard (R3): whatever the body throws trips the thread's component and
// ends the thread, never the program.
#include <cstddef>
#include <functional>
#include <optional>

#ifndef _WIN32
#include <pthread.h>
#endif

#include "en/proto/ffi_guard.hpp"

namespace en::llm {

inline constexpr std::size_t kNetStack = std::size_t{2} << 20;  // ARCH §10.2: the handle's threads, WinHTTP's drivers

class StackThread {
 public:
  StackThread() noexcept = default;
  // Starts `body` on a new thread whose stack is `stack_size` bytes (on Windows a reservation of that size; elsewhere
  // at least the system's minimum). Throws std::system_error when the system refuses the thread; `body` then never
  // runs.
  StackThread(en::proto::Component component, std::size_t stack_size, std::function<void()> body);
  // Joins a thread still joinable, as std::jthread does, rather than end the program as std::thread would.
  ~StackThread();
  StackThread(StackThread&& other) noexcept;
  StackThread& operator=(StackThread&& other) noexcept;
  StackThread(const StackThread&) = delete;
  StackThread& operator=(const StackThread&) = delete;

  bool joinable() const noexcept;
  void join() noexcept;
  void detach() noexcept;

 private:
#ifdef _WIN32
  void* handle_ = nullptr;  // the thread's HANDLE
#else
  pthread_t thread_{};
  bool joinable_ = false;
#endif
};

// The size of the calling thread's stack, in bytes, where the system can say: Windows (the stack's reservation) and
// Linux with glibc; nothing elsewhere.
std::optional<std::size_t> current_stack_size() noexcept;

}  // namespace en::llm
