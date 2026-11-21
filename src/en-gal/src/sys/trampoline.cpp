// The task object and its trampoline (sys_internal.hpp; ARCH §6.4): one zeroed block laid out as a C struct, a pointer
// to a fake vtable of `vtable_slots` entries (each a guarded no-op that counts its calls, the run slot our trampoline),
// then the layout's object_size bytes the game's hub owns, then a pointer to our body. Made once, never freed, never
// unregistered (R14). The trampoline is `extern "C"` and noexcept and starts with ffi_guard (R3); it runs only for our
// own block, issues the token through run_task (task.cpp) and hands the body the clock.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <utility>

#include "en/gal/native_sys.hpp"
#include "en/proto/ffi_guard.hpp"
#include "sys_internal.hpp"

namespace en::gal::sys {

namespace {

// Constant-initialised: nothing runs under the loader lock, and nothing at exit (ARCH §3.1, §1.7).
constinit std::array<std::atomic<std::uint64_t>, kMaxTaskSlots> g_calls{};
constinit std::atomic<TaskBody*> g_body{nullptr};
constinit std::atomic<std::uint64_t> g_block{0};
constinit std::atomic<std::uint32_t> g_run_slot{0};
constinit std::atomic<std::uint64_t> g_last_us{0};
constinit std::atomic<bool> g_registered{false};

using SlotFn = std::uint64_t (*)(void*, void*, void*, void*) noexcept;

template <std::size_t N>
std::uint64_t slot_noop(void*, void*, void*, void*) noexcept {
  return proto::ffi_guard(proto::Component::Task, std::uint64_t{0}, [] {
    g_calls[N].fetch_add(1, std::memory_order_relaxed);
    return std::uint64_t{0};
  });
}

std::uint64_t run_body(void* self) noexcept;

// R1's slots (TaskShape::every_slot): each counts its calls and runs the body for our own object, whichever the game
// calls every frame.
template <std::size_t N>
std::uint64_t slot_run(void* self, void*, void*, void*) noexcept {
  return proto::ffi_guard(proto::Component::Task, std::uint64_t{0}, [&] {
    g_calls[N].fetch_add(1, std::memory_order_relaxed);
    return run_body(self);
  });
}

template <std::size_t... I>
constexpr std::array<SlotFn, sizeof...(I)> make_slots(std::index_sequence<I...>) {
  return {&slot_noop<I>...};
}
template <std::size_t... I>
constexpr std::array<SlotFn, sizeof...(I)> make_run_slots(std::index_sequence<I...>) {
  return {&slot_run<I>...};
}
constexpr std::array<SlotFn, kMaxTaskSlots> kSlots = make_slots(std::make_index_sequence<kMaxTaskSlots>{});
constexpr std::array<SlotFn, kMaxTaskSlots> kRunSlots = make_run_slots(std::make_index_sequence<kMaxTaskSlots>{});

template <class Fn>
std::uint64_t address_of(Fn fn) {
  std::uint64_t a = 0;
  static_assert(sizeof fn == sizeof a);
  std::memcpy(&a, &fn, sizeof a);
  return a;
}

}  // namespace

std::uint64_t qpc_us() noexcept {
  LARGE_INTEGER f{}, c{};
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  const auto freq = static_cast<std::uint64_t>(f.QuadPart);
  const auto count = static_cast<std::uint64_t>(c.QuadPart);
  return freq == 0 ? 0 : count / freq * 1'000'000 + count % freq * 1'000'000 / freq;
}

std::uint64_t slot_calls(std::uint32_t slot) noexcept {
  return slot < kMaxTaskSlots ? g_calls[slot].load(std::memory_order_relaxed) : 0;
}

std::uint64_t task_object() noexcept { return g_block.load(std::memory_order_acquire); }

bool registered() noexcept { return g_registered.load(std::memory_order_acquire); }
void set_registered() noexcept { g_registered.store(true, std::memory_order_release); }

std::uint64_t make_task_object(const TaskShape& shape, TaskBody& body) noexcept {
  if (g_block.load(std::memory_order_acquire) != 0) return 0;
  const std::size_t vt_bytes = std::size_t{8} * shape.vtable_slots;
  const std::size_t block_bytes = std::size_t{8} + shape.object_size + 8;
  // SAFETY: two allocations of our own, committed and zeroed by the system, never freed (R14).
  auto* vtable = static_cast<std::uint64_t*>(VirtualAlloc(nullptr, vt_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  auto* block = static_cast<std::byte*>(VirtualAlloc(nullptr, block_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (vtable == nullptr || block == nullptr) return 0;
  for (std::uint32_t i = 0; i < shape.vtable_slots; ++i) {
    vtable[i] = shape.every_slot      ? address_of(kRunSlots[i])
                : i == shape.run_slot ? address_of(&en_task_run)
                                      : address_of(kSlots[i]);
  }
  DWORD old = 0;
  VirtualProtect(vtable, vt_bytes, PAGE_READONLY, &old);  // the game reads it; nothing writes it again
  const std::uint64_t vt_address = address_of(vtable);
  TaskBody* body_ptr = &body;
  // SAFETY: our own block, 8 + object_size + 8 bytes: the vtable pointer first, our body's pointer last.
  std::memcpy(block, &vt_address, 8);
  std::memcpy(block + 8 + shape.object_size, &body_ptr, 8);
  g_run_slot.store(shape.run_slot, std::memory_order_relaxed);
  g_body.store(&body, std::memory_order_release);
  const std::uint64_t at = address_of(block);
  g_block.store(at, std::memory_order_release);
  return at;
}

}  // namespace en::gal::sys

namespace en::gal {

namespace sys {
namespace {

// The body's run for our own object; any other `self` runs nothing. Called inside a slot's ffi_guard.
std::uint64_t run_body(void* self) noexcept {
  std::uint64_t at = 0;
  std::memcpy(&at, &self, sizeof at);
  if (at != g_block.load(std::memory_order_acquire)) return 0;  // not our object: nothing to run
  TaskBody* body = g_body.load(std::memory_order_acquire);
  if (body == nullptr) return 0;
  const std::uint64_t now = qpc_us();
  const std::uint64_t last = g_last_us.exchange(now, std::memory_order_relaxed);
  const std::uint64_t dt = last == 0 || now < last ? 0 : now - last;
  run_task(*body, FrameCtx{now, static_cast<std::uint32_t>(std::min<std::uint64_t>(dt, 0xFFFF'FFFF))});
  return 0;
}

}  // namespace
}  // namespace sys

extern "C" std::uint64_t en_task_run(void* self, void* data) noexcept {
  return proto::ffi_guard(proto::Component::Task, std::uint64_t{0}, [&] {
    (void)data;  // the game's frame data: its layout is no fact of ours (the clock is QPC)
    sys::g_calls[sys::g_run_slot.load(std::memory_order_relaxed)].fetch_add(1, std::memory_order_relaxed);
    return sys::run_body(self);
  });
}

}  // namespace en::gal
