// The probe build's allocation watch (R6, ARCH §10.1: debug and probe builds count allocations on the game thread
// after warm-up, and a check asserts none): the DLL's own operator new and delete, all twenty forms, counting while
// the game thread has armed them. A release build has none of this, and uses the C runtime's.
//
// Since spike.6 (WHY§227) they allocate from a heap of the DLL's own (HeapCreate), not the process's: R1's
// whole-memory search tells the probe's own copies of our task's address apart from the game's by the heap they lie in
// (own_heap_bases). An alignment above the heap's 16 bytes is made by allocating more and keeping the block's start
// just before the aligned address. If the heap cannot be made, the process's is used and own_heap_bases says so.
#include "dll.hpp"

#if EN_PROBE

#include <cstddef>
#include <cstring>
#include <new>
#include <span>

namespace eldennet {

namespace {
constinit thread_local bool t_armed = false;
constinit std::atomic<std::uint64_t> g_news{0};
constinit std::atomic<HANDLE> g_heap{nullptr};

// Our heap, made at the first allocation (no static initialiser, nothing under the loader lock: DllMain allocates
// nothing). Two threads that both make one keep the first; the other stays empty, never used (one page).
HANDLE heap() noexcept {
  HANDLE h = g_heap.load(std::memory_order_acquire);
  if (h != nullptr) return h;
  HANDLE made = HeapCreate(0, 0, 0);
  if (made == nullptr) made = GetProcessHeap();
  HANDLE expected = nullptr;
  if (!g_heap.compare_exchange_strong(expected, made, std::memory_order_acq_rel)) return expected;
  return made;
}

std::uint64_t number(const void* p) noexcept {
  std::uint64_t n = 0;
  std::memcpy(&n, &p, sizeof n);
  return n;
}

void* allocate(std::size_t size) noexcept {
  if (t_armed) g_news.fetch_add(1, std::memory_order_relaxed);
  return HeapAlloc(heap(), 0, size == 0 ? 1 : size);
}
void* allocate(std::size_t size, std::align_val_t a) noexcept {
  if (t_armed) g_news.fetch_add(1, std::memory_order_relaxed);
  const auto align = static_cast<std::size_t>(a);
  if (align <= 16) return HeapAlloc(heap(), 0, size == 0 ? 1 : size);
  if (size > SIZE_MAX - align - sizeof(void*)) return nullptr;
  void* raw = HeapAlloc(heap(), 0, size + align + sizeof(void*));
  if (raw == nullptr) return nullptr;
  const std::uint64_t at = number(raw);
  const std::uint64_t aligned = (at + sizeof(void*) + align - 1) & ~static_cast<std::uint64_t>(align - 1);
  std::byte* out = static_cast<std::byte*>(raw) + (aligned - at);
  std::memcpy(out - sizeof(void*), &raw, sizeof raw);  // the block's start, for the aligned delete
  return out;
}
void release(void* p) noexcept {
  if (p != nullptr) HeapFree(heap(), 0, p);
}
void release(void* p, std::align_val_t a) noexcept {
  if (p == nullptr) return;
  if (static_cast<std::size_t>(a) <= 16) return release(p);
  void* raw = nullptr;
  std::memcpy(&raw, static_cast<std::byte*>(p) - sizeof(void*), sizeof raw);
  release(raw);
}
template <class P>
P* or_throw(P* p) {
  if (p == nullptr) throw std::bad_alloc();
  return p;
}
}  // namespace

void arm_allocation_watch(bool armed) noexcept { t_armed = armed; }
std::uint64_t allocations_watched() noexcept { return g_news.load(std::memory_order_relaxed); }

// The allocation bases of our heap's regions and large blocks (HeapWalk under the heap's lock; VirtualQuery reads the
// page tables only), into `out`; how many, or nothing when our allocations cannot be told apart (the process's heap in
// use, or the walk refused). Nothing is allocated while the heap is locked.
std::optional<std::size_t> own_heap_bases(std::span<std::uint64_t> out) noexcept {
  HANDLE h = g_heap.load(std::memory_order_acquire);
  if (h == nullptr || h == GetProcessHeap() || !HeapLock(h)) return std::nullopt;
  std::size_t n = 0;
  bool whole = true;
  std::uint64_t region = 0, region_end = 0;  // the last region VirtualQuery described
  PROCESS_HEAP_ENTRY e{};
  e.lpData = nullptr;
  while (HeapWalk(h, &e) != FALSE) {
    const std::uint64_t at = number(e.lpData);
    if (at >= region && at < region_end) continue;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(e.lpData, &mbi, sizeof mbi) == 0) continue;
    region = number(mbi.BaseAddress);
    region_end = region + mbi.RegionSize;
    const std::uint64_t base = number(mbi.AllocationBase);
    bool known = false;
    for (std::size_t i = 0; i < n && !known; ++i) known = out[i] == base;
    if (known) continue;
    if (n == out.size()) {
      whole = false;
      break;
    }
    out[n++] = base;
  }
  const bool ended = GetLastError() == ERROR_NO_MORE_ITEMS;
  HeapUnlock(h);
  if (!whole || !ended) return std::nullopt;
  return n;
}

}  // namespace eldennet

using eldennet::allocate;
using eldennet::release;
void* operator new(std::size_t n) { return eldennet::or_throw(allocate(n)); }
void* operator new[](std::size_t n) { return eldennet::or_throw(allocate(n)); }
void* operator new(std::size_t n, std::align_val_t a) { return eldennet::or_throw(allocate(n, a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return eldennet::or_throw(allocate(n, a)); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return allocate(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return allocate(n); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { return allocate(n, a); }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { return allocate(n, a); }
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, std::size_t) noexcept { release(p); }
void operator delete[](void* p, std::size_t) noexcept { release(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete(void* p, std::align_val_t a) noexcept { release(p, a); }
void operator delete[](void* p, std::align_val_t a) noexcept { release(p, a); }
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept { release(p, a); }
void operator delete[](void* p, std::size_t, std::align_val_t a) noexcept { release(p, a); }

#endif
