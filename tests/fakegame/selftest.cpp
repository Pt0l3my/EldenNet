// --selftest-task: the fake game proves its own registration path the way EldenNet will use it (ARCH §3.2's T1 and
// §6.4's trampoline), from a thread that is not the game's. It waits for the hub's static, builds one zeroed task
// block (a pointer to a fake vtable of three slots, the hub's 0x18 bytes, then a pointer to its body), registers it in
// the stand-in group through the program's own function with the published ABI (or with two arguments swapped), and
// counts each slot's calls, the thread they come on and whether the body pointer survives the hub's writes.
#include <atomic>
#include <cstring>

#include "program.hpp"

namespace fakegame {
namespace {

struct Body {
  std::atomic<std::uint64_t> attach{0};
  std::atomic<std::uint64_t> runs{0};
  std::atomic<std::uint64_t> queries{0};
  std::atomic<std::uint64_t> wrong_thread{0};
  std::atomic<std::uint64_t> wrong_group{0};
  std::atomic<bool> body_ok{true};
  std::atomic<bool> registered{false};
  std::atomic<bool> faulted{false};
  std::atomic<std::uint32_t> fault_code{0};
  DWORD game_thread = 0;
  std::uint32_t group = 0;
  std::uint32_t object_size = 0;
};

Body& body() {
  static Body b;
  return b;
}

// SAFETY: `self` is the block this file built, of 8 + object_size + 8 bytes; the body pointer is its last 8.
Body* body_of(void* self) {
  void* p = nullptr;
  std::memcpy(&p, static_cast<const std::byte*>(self) + 8 + body().object_size, sizeof p);
  if (p != &body()) {
    body().body_ok = false;
    return nullptr;
  }
  return static_cast<Body*>(p);
}

void st_attach(void* self, void*) {
  if (Body* b = body_of(self)) ++b->attach;
}
void st_run(void* self, const FG::FrameInfo* frame) {
  Body* b = body_of(self);
  if (b == nullptr) return;
  ++b->runs;
  if (GetCurrentThreadId() != b->game_thread) ++b->wrong_thread;
  if (frame->group != b->group) ++b->wrong_group;
}
std::uint32_t st_query(void* self) {
  if (Body* b = body_of(self)) ++b->queries;
  return 0;
}

struct FakeVtable {
  void (*attach)(void*, void*);
  void (*run)(void*, const FG::FrameInfo*);
  std::uint32_t (*query)(void*);
};
const FakeVtable kVtable{st_attach, st_run, st_query};

using RegisterFn = std::uint64_t (*)(FG::TaskUnit*, FG::TaskLoomImp*, std::uint32_t);

// The call goes through a pointer, as EldenNet's will (its address comes from discovery). What it answers is not read.
void call_register(RegisterFn fn, void* first, void* second, std::uint32_t group) {
  (void)fn(static_cast<FG::TaskUnit*>(first), static_cast<FG::TaskLoomImp*>(second), group);
}

#if defined(_MSC_VER)
// A fault in the call is caught here, as T1's SEH guard around register_task catches it (ARCH §3.2, Register).
bool guarded_register(RegisterFn fn, void* first, void* second, std::uint32_t group, std::uint32_t& code) {
  __try {
    call_register(fn, first, second, group);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    code = GetExceptionCode();
    return false;
  }
  return true;
}
#endif

DWORD WINAPI selftest_thread(void*) {
  void* hub = nullptr;
  for (int i = 0; i < 3000 && hub == nullptr; ++i) {
    hub = fg_static_taskloom;
    if (hub == nullptr) Sleep(10);
  }
  if (hub == nullptr) {
    status("selftest no-hub");
    return 1;
  }
  Body& b = body();
  const std::size_t size = 8 + b.object_size + 8;
  auto* block = static_cast<std::byte*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  const FakeVtable* vt = &kVtable;
  Body* self = &b;
  std::memcpy(block, &vt, sizeof vt);
  std::memcpy(block + 8 + b.object_size, &self, sizeof self);

  RegisterFn volatile fn = fg_register_task;
  const bool swapped = options().selftest_swapped;
  void* first = swapped ? hub : static_cast<void*>(block);
  void* second = swapped ? static_cast<void*>(block) : hub;
  status("selftest register group=%u abi=%s object_size=0x%X", b.group, swapped ? "swapped" : "published",
         b.object_size);
#if defined(_MSC_VER)
  std::uint32_t code = 0;
  if (!guarded_register(fn, first, second, b.group, code)) {
    b.faulted = true;
    b.fault_code = code;
    status("selftest register faulted code=0x%08X", code);
    return 2;
  }
#else
  call_register(fn, first, second, b.group);  // MinGW's GCC has no __try: a fault here ends the program
#endif
  b.registered = true;
  status("selftest registered");
  return 0;
}

}  // namespace

void selftest_start(DWORD game_thread) {
  Body& b = body();
  b.game_thread = game_thread;
  b.object_size = options().selftest_object_size;
  b.group = options().selftest_group >= 0 ? static_cast<std::uint32_t>(options().selftest_group) : standin_group();
  HANDLE t = CreateThread(nullptr, 0, selftest_thread, nullptr, 0, nullptr);
  if (t != nullptr) CloseHandle(t);
}

SelftestStats selftest_stats() {
  Body& b = body();
  SelftestStats s;
  s.registered = b.registered;
  s.faulted = b.faulted;
  s.fault_code = b.fault_code;
  s.attach = b.attach;
  s.runs = b.runs;
  s.queries = b.queries;
  s.wrong_thread = b.wrong_thread;
  s.wrong_group = b.wrong_group;
  s.body_ok = b.body_ok;
  return s;
}

}  // namespace fakegame
