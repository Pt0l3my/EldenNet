// The replaced global operator new and delete of the test program, and the fault they can throw (fault_alloc.hpp).
#include "fault_alloc.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#ifdef _WIN32
#include <malloc.h>  // _aligned_malloc, _aligned_free
#endif

namespace en::llm::test::faults {

namespace {

std::atomic<bool> g_armed{false};
std::atomic<bool> g_fired{false};
std::atomic<std::size_t> g_min{0};
std::atomic<std::size_t> g_max{0};
thread_local bool t_exempt = false;

// Whether this allocation is the one to fail: the first in range on a thread not exempt takes the fault.
bool take(std::size_t size) noexcept {
  if (!g_armed.load(std::memory_order_acquire) || t_exempt) return false;
  if (size < g_min.load(std::memory_order_relaxed) || size >= g_max.load(std::memory_order_relaxed)) return false;
  bool expected = true;
  if (!g_armed.compare_exchange_strong(expected, false)) return false;
  g_fired.store(true, std::memory_order_release);
  return true;
}

}  // namespace

void arm(std::size_t min_size, std::size_t max_size) noexcept {
  g_min.store(min_size, std::memory_order_relaxed);
  g_max.store(max_size, std::memory_order_relaxed);
  g_fired.store(false, std::memory_order_relaxed);
  g_armed.store(true, std::memory_order_release);
}

void disarm() noexcept { g_armed.store(false, std::memory_order_release); }

bool fired() noexcept { return g_fired.load(std::memory_order_acquire); }

void exempt_this_thread() noexcept { t_exempt = true; }

}  // namespace en::llm::test::faults

// Every form of the global operator new allocates here and every form of delete frees here: all twenty are replaced,
// so that no memory passes between this program's allocator and a runtime's (a sanitizer's nothrow or aligned new,
// freed here by free(), would be a mismatch).
namespace {

void* allocate(std::size_t size) {
  if (en::llm::test::faults::take(size)) throw std::bad_alloc();
  if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
  throw std::bad_alloc();
}

void* allocate(std::size_t size, std::align_val_t alignment) {
  if (en::llm::test::faults::take(size)) throw std::bad_alloc();
  const auto align = static_cast<std::size_t>(alignment);
#ifdef _WIN32
  if (void* p = _aligned_malloc(size == 0 ? 1 : size, align)) return p;
#else
  void* p = nullptr;
  if (posix_memalign(&p, align, size == 0 ? 1 : size) == 0) return p;
#endif
  throw std::bad_alloc();
}

void release(void* p) noexcept { std::free(p); }

void release_aligned(void* p) noexcept {
#ifdef _WIN32
  _aligned_free(p);
#else
  std::free(p);
#endif
}

}  // namespace

void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void* operator new(std::size_t size, std::align_val_t alignment) { return allocate(size, alignment); }
void* operator new[](std::size_t size, std::align_val_t alignment) { return allocate(size, alignment); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  try {
    return allocate(size);
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
  try {
    return allocate(size);
  } catch (...) {
    return nullptr;
  }
}
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
  try {
    return allocate(size, alignment);
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
  try {
    return allocate(size, alignment);
  } catch (...) {
    return nullptr;
  }
}

void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, std::size_t) noexcept { release(p); }
void operator delete[](void* p, std::size_t) noexcept { release(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete(void* p, std::align_val_t) noexcept { release_aligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { release_aligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { release_aligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { release_aligned(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { release_aligned(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { release_aligned(p); }
