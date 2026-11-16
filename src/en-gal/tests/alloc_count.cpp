// The allocation watch (fake_sys.hpp's AllocWatch; ARCH §10.1's R6 test): this test program replaces the global
// operator new and operator delete with counting ones, which count what the game thread allocates and frees while a
// FakeSys tick runs with the watch armed. A test asserts zero after warm-up.
#include <cstddef>
#include <cstdlib>
#include <new>

#include "fake_sys.hpp"

namespace en::gal::test {
thread_local bool AllocWatch::armed = false;
std::atomic<std::uint64_t> AllocWatch::news{0};
std::atomic<std::uint64_t> AllocWatch::deletes{0};
}  // namespace en::gal::test

using en::gal::test::AllocWatch;

namespace {

void count_new() noexcept {
  if (AllocWatch::armed) AllocWatch::news.fetch_add(1, std::memory_order_relaxed);
}
void count_delete(void* p) noexcept {
  if (p != nullptr && AllocWatch::armed) AllocWatch::deletes.fetch_add(1, std::memory_order_relaxed);
}

void* allocate(std::size_t size) noexcept {
  count_new();
  return std::malloc(size == 0 ? 1 : size);
}

void* allocate(std::size_t size, std::align_val_t alignment) noexcept {
  count_new();
  const auto align = static_cast<std::size_t>(alignment);
#ifdef _WIN32
  return _aligned_malloc(size == 0 ? 1 : size, align);
#else
  void* p = nullptr;
  return posix_memalign(&p, align, size == 0 ? 1 : size) == 0 ? p : nullptr;
#endif
}

void release(void* p) noexcept {
  count_delete(p);
  std::free(p);
}

void release_aligned(void* p) noexcept {
  count_delete(p);
#ifdef _WIN32
  _aligned_free(p);
#else
  std::free(p);
#endif
}

template <class P>
P* or_throw(P* p) {
  if (p == nullptr) throw std::bad_alloc();
  return p;
}

}  // namespace

// All twenty forms, so that no memory passes between this program's allocator and a runtime's (a sanitizer's nothrow
// or aligned new freed here by free() would be a mismatch).
void* operator new(std::size_t size) { return or_throw(allocate(size)); }
void* operator new[](std::size_t size) { return or_throw(allocate(size)); }
void* operator new(std::size_t size, std::align_val_t a) { return or_throw(allocate(size, a)); }
void* operator new[](std::size_t size, std::align_val_t a) { return or_throw(allocate(size, a)); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void* operator new(std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept { return allocate(size, a); }
void* operator new[](std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept { return allocate(size, a); }
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
