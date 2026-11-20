// NativeSys in a real Windows process (clang-cl, under Wine in CI): the module's image read through guarded copies, the
// two copy primitives, the stack table (WHY§111), the calls into "the game" under SEH, and the task object with its
// trampoline, driven here as a game's hub would drive it. The functions it calls are this test's own.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <doctest.h>

#include <atomic>
#include <cstring>
#include <string>

#include "en/gal/guarded.hpp"
#include "en/gal/native_sys.hpp"
#include "en/gal/pe.hpp"
#include "en/proto/ffi_guard.hpp"
#include "sys/sys_internal.hpp"

using namespace en::gal;

std::uint64_t g_args[4] = {};
const char kName[] = "InventedName";

extern "C" __declspec(noinline) void test_register(std::uint64_t a, std::uint64_t b, std::uint64_t c, std::uint64_t d) {
  g_args[0] = a;
  g_args[1] = b;
  g_args[2] = c;
  g_args[3] = d;
}
extern "C" __declspec(noinline) std::uint64_t test_getter(std::uint64_t) {
  std::uint64_t a = 0;
  const char* p = kName;
  std::memcpy(&a, &p, sizeof a);
  return a;
}
extern "C" __declspec(noinline) void test_faulting(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t) {
  // SAFETY: a deliberate read of the first page, never mapped: the fault a game given bad arguments would meet.
  volatile const std::uint32_t* bad = reinterpret_cast<volatile const std::uint32_t*>(std::uintptr_t{0x10});
  (void)*bad;
}

namespace {

template <class Fn>
std::uint64_t address_of(Fn fn) {
  std::uint64_t a = 0;
  std::memcpy(&a, &fn, sizeof a);
  return a;
}

bool on_wine() {
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  return ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

struct CountingBody final : TaskBody {
  void run(const en::proto::GameThread&, const FrameCtx& frame) override {
    ++runs;
    last_us = frame.qpc_now_us;
  }
  std::atomic<int> runs{0};
  std::uint64_t last_us = 0;
};

}  // namespace

TEST_CASE("native sys: the main module's image reads through guarded copies, as the loader mapped it") {
  NativeSys sys;
  // SAFETY: the module's handle, as a number.
  CHECK(sys.module_base() == reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
  const auto image = read_image(sys.image(), ImageLayout::Memory);
  REQUIRE(image.has_value());
  CHECK(image->section(".text") != nullptr);
  CHECK(image->size_of_image == sys.image().size());
  CHECK_FALSE(image->functions.empty());
  std::array<std::byte, 16> past{};
  CHECK_FALSE(sys.image().read(sys.image().size() - 8, past));
}

TEST_CASE("native sys: both copy primitives copy, and fail cleanly on a page that cannot be read") {
  for (const CopyKind kind : {CopyKind::Seh, CopyKind::Rpm}) {
    const char* name = kind == CopyKind::Seh ? "seh" : "rpm";
    INFO(name);
    NativeSys sys(kind);
    const std::uint64_t value = 0x1122334455667788ull;
    std::array<std::byte, 8> dst{};
    CHECK(sys.copier().copy(address_of(&value), dst).copied);
    std::uint64_t back = 0;
    std::memcpy(&back, dst.data(), 8);
    CHECK(back == value);
    dst.fill(std::byte{0x5A});
    const CopyOutcome bad = sys.copier().copy(sys.no_access_page(), dst);
    if (kind == CopyKind::Rpm && on_wine()) {
      // Wine's ReadProcessMemory on its own process reads through the server, which reads the process's memory file
      // and so ignores the page's protection: a reserved page reads as zeros (WHY§161). Windows refuses it.
      CHECK(std::all_of(dst.begin(), dst.end(), [](std::byte b) { return b == std::byte{0}; }));
      continue;
    }
    CHECK_FALSE(bad.copied);
    CHECK(bad.code == 0xC0000005u);
    CHECK(std::all_of(dst.begin(), dst.end(), [](std::byte b) { return b == std::byte{0}; }));
  }
}

TEST_CASE("native sys: what ReadProcessMemory on our own process does with a guard page and a released one") {
  NativeSys sys(CopyKind::Rpm);
  // SAFETY: pages of our own: one with a guard, one released.
  auto* guard = static_cast<std::byte*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE | PAGE_GUARD));
  auto* gone = static_cast<std::byte*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  REQUIRE(guard != nullptr);
  REQUIRE(gone != nullptr);
  REQUIRE(VirtualFree(gone, 0, MEM_RELEASE) != 0);
  std::array<std::byte, 8> dst{};
  const CopyOutcome g = sys.copier().copy(address_of(guard), dst);
  MEMORY_BASIC_INFORMATION mbi{};
  REQUIRE(VirtualQuery(guard, &mbi, sizeof mbi) != 0);
  const bool guard_kept = (mbi.Protect & PAGE_GUARD) != 0;
  const CopyOutcome r = sys.copier().copy(address_of(gone), dst);
  MESSAGE("ReadProcessMemory, own process: guard page copied=" << g.copied << " guard kept=" << guard_kept
                                                               << "; released page copied=" << r.copied
                                                               << std::string(on_wine() ? " (Wine)" : " (Windows)"));
  CHECK(guard_kept);  // whatever it answers, it never lifts a guard
  if (!on_wine()) {
    CHECK_FALSE(g.copied);
    CHECK_FALSE(r.copied);
  }
}

TEST_CASE("native sys: stacks and guard pages are never read (WHY§111)") {
  NativeSys sys;
  GuardedReader reader(sys);
  const auto gt = en::proto::GameThread::issue();
  reader.begin_tick();
  const std::uint64_t local = 7;
  const auto r = reader.copy<std::uint64_t>(gt, en::proto::Group::World, address_of(&local));
  REQUIRE_FALSE(r.has_value());
  CHECK(r.error().kind == FaultKind::Stack);
  // An allocation with a guard page, as a stack has: the walk finds it, and the reader refuses all of it.
  // SAFETY: two pages of our own; the second keeps its guard because nothing reads it.
  auto* pages = static_cast<std::byte*>(VirtualAlloc(nullptr, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  REQUIRE(pages != nullptr);
  DWORD old = 0;
  REQUIRE(VirtualProtect(pages + 4096, 4096, PAGE_READWRITE | PAGE_GUARD, &old) != 0);
  sys.refresh_stacks();
  CHECK(reader.copy<std::uint64_t>(gt, en::proto::Group::World, address_of(pages)).error().kind == FaultKind::Stack);
  CHECK(reader.copy<std::uint64_t>(gt, en::proto::Group::World, address_of(pages + 4096)).error().kind == FaultKind::Stack);
  MEMORY_BASIC_INFORMATION mbi{};
  REQUIRE(VirtualQuery(pages + 4096, &mbi, sizeof mbi) != 0);
  CHECK((mbi.Protect & PAGE_GUARD) != 0);  // untouched
}

TEST_CASE("native sys: the rescan leaves out the calling thread's stack; a freed stack range is forgotten (WHY§190)") {
  NativeSys sys;
  GuardedReader reader(sys);
  const auto gt = en::proto::GameThread::issue();
  reader.begin_tick();
  const std::uint64_t local = 7;
  CHECK(reader.copy<std::uint64_t>(gt, en::proto::Group::World, address_of(&local)).error().kind == FaultKind::Stack);
  sys.rescan_stacks();
  ULONG_PTR low = 0, high = 0;
  GetCurrentThreadStackLimits(&low, &high);
  CHECK_FALSE(sys.stacks().overlaps(low, high - low));
  // An allocation with a guard page, in the table, then freed and made again without one (the game's object where a
  // stack was): the read refused at first goes through once the range is found stale.
  // SAFETY: pages of our own; the guard page is never read.
  auto* pages = static_cast<std::byte*>(VirtualAlloc(nullptr, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  REQUIRE(pages != nullptr);
  DWORD old = 0;
  REQUIRE(VirtualProtect(pages + 4096, 4096, PAGE_READWRITE | PAGE_GUARD, &old) != 0);
  sys.refresh_stacks();
  REQUIRE(sys.stacks().overlaps(address_of(pages), 8));
  CHECK(reader.copy<std::uint64_t>(gt, en::proto::Group::World, address_of(pages + 4096)).error().kind == FaultKind::Stack);
  REQUIRE(VirtualProtect(pages + 4096, 4096, PAGE_READWRITE, &old) != 0);  // no guard page: no longer a stack's shape
  CHECK(reader.copy<std::uint64_t>(gt, en::proto::Group::World, address_of(pages)).has_value());
  CHECK(reader.stale_stacks() == 1);
  CHECK_FALSE(sys.stacks().overlaps(address_of(pages), 8));
  VirtualFree(pages, 0, MEM_RELEASE);
}

TEST_CASE("native sys: the regions VirtualQuery describes, and the whole-memory search over this process (WHY§227)") {
  NativeSys sys;
  // SAFETY: pages of our own: a "task" block, a page that holds its address, and a page with no access.
  auto* task = static_cast<std::byte*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  auto* holder = static_cast<std::byte*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  auto* closed = static_cast<std::byte*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS));
  REQUIRE((task != nullptr && holder != nullptr && closed != nullptr));
  const std::uint64_t at = address_of(task);
  std::memcpy(holder + 0x40, &at, sizeof at);
  const auto r = sys.region_at(address_of(holder) + 0x10);
  REQUIRE(r.has_value());
  CHECK(r->base == address_of(holder));
  CHECK(r->allocation_base == address_of(holder));
  CHECK(r->committed);
  CHECK(r->kind == probe::RegionKind::Private);
  CHECK(r->protect == PAGE_READWRITE);
  CHECK(probe::searchable(*r));
  const auto image = sys.region_at(sys.module_base());
  REQUIRE(image.has_value());
  CHECK(image->kind == probe::RegionKind::Image);
  CHECK(image->allocation_base == sys.module_base());
  CHECK_FALSE(probe::searchable(*sys.region_at(address_of(closed))));
  CHECK_FALSE(sys.region_at(kAddressEnd).has_value());

  probe::MemorySearchSpec spec;
  spec.target = at;
  spec.object_bytes = 0x100;
  spec.game_base = sys.module_base();
  spec.own_bases = {at};
  spec.own_known = true;
  ULONG_PTR low = 0, high = 0;
  GetCurrentThreadStackLimits(&low, &high);
  spec.skip.push_back(AddressRange{low, high});
  const probe::MemorySearch m =
      probe::search_memory(sys, [&](std::uint64_t a) { return sys.region_at(a); }, spec);
  CHECK(m.stopped == probe::SearchStop::Done);
  CHECK(m.found >= 1);
  CHECK(m.read_bytes > 0);
  CHECK(m.unread_bytes >= 4096);  // the page with no access, never touched
  bool held = false;
  for (const probe::MemoryHit& h : m.hits) held = held || (!h.own && h.region == probe::RegionKind::Private && h.offset == 0);
  CHECK(held);
  VirtualFree(closed, 0, MEM_RELEASE);
  VirtualFree(holder, 0, MEM_RELEASE);
  VirtualFree(task, 0, MEM_RELEASE);
}

TEST_CASE("native sys: the calls into the game run under SEH, and the task object's trampoline runs our body") {
  // A call that faults is an answer (ARCH §3.2's Register → banner).
  const sys::CallOutcome fault = sys::call_register(address_of(&test_faulting), {1, 2, 3, 4});
  CHECK_FALSE(fault.ok);
  CHECK(fault.code == 0xC0000005u);

  NativeSys sys;
  // The getter fallback, before registration only.
  const auto name = sys.call_getter(address_of(&test_getter), 0x1234);
  REQUIRE(name.has_value());
  CHECK(*name == address_of(&kName[0]));

  CountingBody body;
  const TaskShape shape{4, 2, 0x18, 7, RegisterAbi{ArgRegister::Rdx, ArgRegister::R9, ArgRegister::Rcx}};
  CHECK(sys.register_task(address_of(&test_register), 0xABCDEF00, TaskShape{4, 4, 0x18, 7, {}}, body).error() ==
        SysError::BadShape);
  REQUIRE(sys.register_task(address_of(&test_register), 0xABCDEF00, shape, body).has_value());
  CHECK(g_args[0] == 7);            // rcx: the group
  CHECK(g_args[3] == 0xABCDEF00);  // r9: the instance
  const std::uint64_t block = g_args[1];  // rdx: our task object
  REQUIRE(block != 0);
  CHECK(sys.register_task(address_of(&test_register), 1, shape, body).error() == SysError::Twice);
  CHECK(sys.call_getter(address_of(&test_getter), 0).error() == SysError::NotInit);

  // The hub's side: the object's first field is its vtable; slot 2 runs the task, the others are counted no-ops.
  std::uint64_t vtable = 0;
  std::memcpy(&vtable, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(block)), 8);
  using Slot = std::uint64_t (*)(void*, void*, void*, void*);
  const auto slot = [&](int i) {
    std::uint64_t entry = 0;
    std::memcpy(&entry, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(vtable + 8ull * i)), 8);
    Slot fn = nullptr;
    std::memcpy(&fn, &entry, sizeof fn);
    return fn;
  };
  void* self = reinterpret_cast<void*>(static_cast<std::uintptr_t>(block));
  for (int i = 0; i < 3; ++i) CHECK(slot(2)(self, nullptr, nullptr, nullptr) == 0);
  CHECK(body.runs == 3);
  CHECK(slot(0)(self, nullptr, nullptr, nullptr) == 0);
  CHECK(slot(3)(self, nullptr, nullptr, nullptr) == 0);
  CHECK(sys.slot_calls(2) == 3);
  CHECK(sys.slot_calls(0) == 1);
  CHECK(sys.slot_calls(1) == 0);
  CHECK(sys.slot_calls(3) == 1);
  // Another object through the run slot: not ours, nothing runs.
  std::uint64_t other[4] = {vtable, 0, 0, 0};
  CHECK(en_task_run(other, nullptr) == 0);
  CHECK(body.runs == 3);
  CHECK(body.last_us > 0);
  // The vtable is read-only once made.
  MEMORY_BASIC_INFORMATION mbi{};
  REQUIRE(VirtualQuery(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(vtable)), &mbi, sizeof mbi) != 0);
  CHECK(mbi.Protect == PAGE_READONLY);
}
