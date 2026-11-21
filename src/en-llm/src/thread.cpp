// Threads with a stack of a given size (thread.hpp).
#include "en/llm/thread.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <memory>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#else
#include <climits>
#endif

namespace en::llm {

namespace {

using en::proto::ffi_guard;

// What a new thread runs, handed to it through the system's one pointer of context.
struct Start {
  en::proto::Component component;
  std::function<void()> body;
};

// Takes the Start over and runs its body; the guard around it is the entry's.
void run_start(void* arg) {
  // SAFETY: `arg` is the Start that the StackThread constructor made with new for this thread alone and handed to the
  // system's thread call, which passes it here unchanged; from here on this thread owns it.
  const std::unique_ptr<Start> start(static_cast<Start*>(arg));
  start->body();
}

en::proto::Component component_of(void* arg) noexcept {
  // SAFETY: as in run_start: the Start made for this thread, read before run_start takes it over.
  return static_cast<const Start*>(arg)->component;
}

#ifdef _WIN32
unsigned __stdcall thread_entry(void* arg) noexcept {
  return ffi_guard(component_of(arg), 0u, [arg] {
    run_start(arg);
    return 0u;
  });
}
#else
void* thread_entry(void* arg) noexcept {
  return ffi_guard<void*>(component_of(arg), nullptr, [arg]() -> void* {
    run_start(arg);
    return nullptr;
  });
}
#endif

}  // namespace

StackThread::StackThread(en::proto::Component component, std::size_t stack_size, std::function<void()> body) {
  auto start = std::make_unique<Start>(Start{component, std::move(body)});
#ifdef _WIN32
  // A reservation of the size asked, not only its first commit: the stack can grow to it and no further.
  const auto size = static_cast<unsigned>(std::min<std::size_t>(stack_size, 0xFFFF0000u));
  const std::uintptr_t thread =
      _beginthreadex(nullptr, size, &thread_entry, start.get(), STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
  if (thread == 0) throw std::system_error(errno, std::generic_category(), "a thread could not be started");
  start.release();  // the thread owns it now
  // SAFETY: _beginthreadex answers the new thread's HANDLE as an integer; this turns it back into the handle it is.
  handle_ = reinterpret_cast<void*>(thread);
#else
  pthread_attr_t attr;
  if (const int rc = pthread_attr_init(&attr); rc != 0) {
    throw std::system_error(rc, std::generic_category(), "a thread could not be started");
  }
  const std::size_t size = stack_size < static_cast<std::size_t>(PTHREAD_STACK_MIN)
                               ? static_cast<std::size_t>(PTHREAD_STACK_MIN)
                               : stack_size;
  int rc = pthread_attr_setstacksize(&attr, size);
  if (rc == 0) rc = pthread_create(&thread_, &attr, &thread_entry, start.get());
  pthread_attr_destroy(&attr);
  if (rc != 0) throw std::system_error(rc, std::generic_category(), "a thread could not be started");
  start.release();  // the thread owns it now
  joinable_ = true;
#endif
}

StackThread::~StackThread() { join(); }

StackThread::StackThread(StackThread&& other) noexcept { *this = std::move(other); }

StackThread& StackThread::operator=(StackThread&& other) noexcept {
  if (this != &other) {
    join();
#ifdef _WIN32
    handle_ = std::exchange(other.handle_, nullptr);
#else
    thread_ = other.thread_;
    joinable_ = std::exchange(other.joinable_, false);
#endif
  }
  return *this;
}

bool StackThread::joinable() const noexcept {
#ifdef _WIN32
  return handle_ != nullptr;
#else
  return joinable_;
#endif
}

void StackThread::join() noexcept {
  if (!joinable()) return;
#ifdef _WIN32
  WaitForSingleObject(handle_, INFINITE);
  CloseHandle(handle_);
  handle_ = nullptr;
#else
  pthread_join(thread_, nullptr);
  joinable_ = false;
#endif
}

void StackThread::detach() noexcept {
  if (!joinable()) return;
#ifdef _WIN32
  CloseHandle(handle_);
  handle_ = nullptr;
#else
  pthread_detach(thread_);
  joinable_ = false;
#endif
}

std::optional<std::size_t> current_stack_size() noexcept {
#if defined(_WIN32)
  ULONG_PTR low = 0;
  ULONG_PTR high = 0;
  GetCurrentThreadStackLimits(&low, &high);
  if (high <= low) return std::nullopt;
  return static_cast<std::size_t>(high - low);
#elif defined(__GLIBC__)
  pthread_attr_t attr;
  if (pthread_getattr_np(pthread_self(), &attr) != 0) return std::nullopt;
  std::size_t size = 0;
  const int rc = pthread_attr_getstacksize(&attr, &size);
  pthread_attr_destroy(&attr);
  if (rc != 0 || size == 0) return std::nullopt;
  return size;
#else
  return std::nullopt;
#endif
}

}  // namespace en::llm
