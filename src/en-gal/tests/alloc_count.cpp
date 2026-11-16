// The allocation watch (fake_sys.hpp's AllocWatch; ARCH §10.1's R6 test): this test program replaces the global
// operator new and operator delete with counting ones, which count what the game thread allocates and frees while a
// FakeSys tick runs with the watch armed. A test asserts zero after warm-up.
#include <cstdlib>
#include <new>

#include "fake_sys.hpp"

namespace en::gal::test {
thread_local bool AllocWatch::armed = false;
std::atomic<std::uint64_t> AllocWatch::news{0};
std::atomic<std::uint64_t> AllocWatch::deletes{0};
}  // namespace en::gal::test

using en::gal::test::AllocWatch;

void* operator new(std::size_t size) {
  if (AllocWatch::armed) AllocWatch::news.fetch_add(1, std::memory_order_relaxed);
  void* p = std::malloc(size == 0 ? 1 : size);
  if (p == nullptr) throw std::bad_alloc();
  return p;
}

void* operator new[](std::size_t size) { return ::operator new(size); }

void operator delete(void* p) noexcept {
  if (p != nullptr && AllocWatch::armed) AllocWatch::deletes.fetch_add(1, std::memory_order_relaxed);
  std::free(p);
}

void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
