// The probe build's allocation watch (R6, ARCH §10.1: debug and probe builds count allocations on the game thread
// after warm-up, and a check asserts none): the DLL's own operator new and delete, all twenty forms, counting while
// the game thread has armed them. A release build has none of this, and uses the C runtime's.
#include "dll.hpp"

#if EN_PROBE

#include <cstdlib>
#include <new>

namespace eldennet {

namespace {
constinit thread_local bool t_armed = false;
constinit std::atomic<std::uint64_t> g_news{0};

void* allocate(std::size_t size) noexcept {
  if (t_armed) g_news.fetch_add(1, std::memory_order_relaxed);
  return std::malloc(size == 0 ? 1 : size);
}
void* allocate(std::size_t size, std::align_val_t a) noexcept {
  if (t_armed) g_news.fetch_add(1, std::memory_order_relaxed);
  return _aligned_malloc(size == 0 ? 1 : size, static_cast<std::size_t>(a));
}
template <class P>
P* or_throw(P* p) {
  if (p == nullptr) throw std::bad_alloc();
  return p;
}
}  // namespace

void arm_allocation_watch(bool armed) noexcept { t_armed = armed; }
std::uint64_t allocations_watched() noexcept { return g_news.load(std::memory_order_relaxed); }

}  // namespace eldennet

using eldennet::allocate;
void* operator new(std::size_t n) { return eldennet::or_throw(allocate(n)); }
void* operator new[](std::size_t n) { return eldennet::or_throw(allocate(n)); }
void* operator new(std::size_t n, std::align_val_t a) { return eldennet::or_throw(allocate(n, a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return eldennet::or_throw(allocate(n, a)); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return allocate(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return allocate(n); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { return allocate(n, a); }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { return allocate(n, a); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { _aligned_free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { _aligned_free(p); }

#endif
