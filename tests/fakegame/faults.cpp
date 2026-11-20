// Faults on request (README.md): the crashes a game can end with, the exceptions a game raises and handles itself, and
// the program's own unhandled-exception filter, which a game may install, or install again over another's.
#include <intrin.h>
#include <malloc.h>

#include <atomic>
#include <cstring>

#include "program.hpp"

namespace fakegame {
namespace {

std::atomic<std::uint32_t> g_handled{0};
volatile bool g_recurse = true;
volatile int g_sink = 0;  // results kept, so that the optimiser keeps the work that faults
constexpr UINT kTerminateCode = 0x46470099;  // the code `terminate` ends with, unless --exit-code names another

// SAFETY: reads and writes in the first page, which is never mapped: each is the fault it stands for.
__declspec(noinline) void touch_read(std::uintptr_t address) {
  volatile const int* p = reinterpret_cast<volatile const int*>(address);
  (void)*p;
}
__declspec(noinline) void touch_write(std::uintptr_t address) {
  volatile int* p = reinterpret_cast<volatile int*>(address);
  *p = 1;
}
// Both operands unknown to the optimiser: 1 / b alone it turns into a comparison, with no division to trap.
__declspec(noinline) int divide(const volatile int* a, const volatile int* b) { return *a / *b; }

// One byte a page, through a pointer the optimiser cannot see past.
__declspec(noinline) void touch_pages(volatile char* p, std::size_t size, char value) {
  for (std::size_t i = 0; i < size; i += 4096) p[i] = value;
}

// Each frame holds 16 KiB whose address leaves the function, so the optimiser keeps all of it (a volatile array it
// would cut down to the bytes used), and uses it after the call, so no call becomes a jump. Each new frame's stack
// probe then meets the guard page with room left above it for the exception's dispatch: with frames of a few bytes,
// Wine ended the process with code 1 and no exception.
__declspec(noinline) int recurse(int depth) {
  char pad[16384];
  touch_pages(pad, sizeof pad, static_cast<char>(depth));
  int below = 0;
  if (g_recurse) below = recurse(depth + 1);
  touch_pages(pad, 1, static_cast<char>(below));
  return pad[0];
}

// The recursion started at the same distance from the stack's end whatever called it. Whether Wine finds room to
// dispatch the overflow's exception depends on where the 16 KiB frames' probes meet the end, so on how deep the caller
// was: a game loop a few hundred bytes deeper made Wine end the process with code 1 and no exception.
__declspec(noinline) int recurse_from_fixed_depth() {
  ULONG_PTR low = 0, high = 0;
  GetCurrentThreadStackLimits(&low, &high);
  volatile char here = 0;
  const std::uintptr_t at = reinterpret_cast<std::uintptr_t>(&here);
  constexpr std::size_t room = 256u << 10;  // any distance from 64 KiB to 512 KiB served under Wine 9.0
  if (at > low + room) {
    const std::size_t size = at - (low + room);
    auto* p = static_cast<volatile char*>(_alloca(size));
    touch_pages(p, size, 1);
  }
  return recurse(0);
}

// SAFETY: a page of our own, readable and writable but not executable, entered as code: the fault of running data.
__declspec(noinline) void run_data() {
  void* page = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  static_cast<unsigned char*>(page)[0] = 0xC3;  // ret
  void (*fn)() = nullptr;
  std::memcpy(&fn, &page, sizeof fn);
  fn();
}

struct Signal {
  int value;
};
__declspec(noinline) void throw_signal(int value) { throw Signal{value}; }

#if defined(_MSC_VER)
int av_filter(unsigned long code) {
  return code == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}
// The program's own guarded read of an unmapped page, caught by its own frame handler (clang-cl's __try).
bool handled_seh() {
  __try {
    touch_read(0x10);
  } __except (av_filter(GetExceptionCode())) {
    return true;
  }
  return false;
}
#endif

bool handled_cxx() {
  try {
    throw_signal(7);
  } catch (const Signal& s) {
    return s.value == 7;
  }
  return false;
}

// Writes `value` as `digits` upper-case hexadecimal digits at `out`; answers the position after them.
char* put_hex(char* out, std::uint64_t value, int digits) {
  for (int i = digits - 1; i >= 0; --i) *out++ = "0123456789ABCDEF"[(value >> (4 * i)) & 0xF];
  return out;
}

// The program's own unhandled-exception filter. After a stack overflow only a few KiB of stack are left, and the
// formatting of status() needs more (Wine then ended the process with code 1 in the release build), so this line is
// put together by hand: "FAKEGAME filter code=0x... address=0x...".
LONG WINAPI game_filter(EXCEPTION_POINTERS* info) {
  char line[80];
  static const char kHead[] = "FAKEGAME filter code=0x";
  static const char kMid[] = " address=0x";
  char* p = line;
  for (const char* c = kHead; *c != '\0'; ++c) *p++ = *c;
  p = put_hex(p, info->ExceptionRecord->ExceptionCode, 8);
  for (const char* c = kMid; *c != '\0'; ++c) *p++ = *c;
  std::uint64_t address = 0;
  std::memcpy(&address, &info->ExceptionRecord->ExceptionAddress, sizeof address);
  p = put_hex(p, address, 16);
  *p++ = '\n';
  status_raw(line, static_cast<std::size_t>(p - line));
  return options().game_filter_continue ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER;
}

DWORD WINAPI crash_thread(void* param) {
  Crash c{};
  std::memcpy(&c, &param, sizeof c);
  crash_now(c);
}

}  // namespace

const char* crash_name(Crash c) {
  switch (c) {
    case Crash::None: return "none";
    case Crash::AvRead: return "av-read";
    case Crash::AvWrite: return "av-write";
    case Crash::AvExec: return "av-exec";
    case Crash::DivZero: return "div-zero";
    case Crash::Illegal: return "illegal";
    case Crash::StackOverflow: return "stack-overflow";
    case Crash::Breakpoint: return "breakpoint";
    case Crash::FastFail: return "fastfail";
    case Crash::Terminate: return "terminate";
  }
  return "?";
}

bool handled_supported(Handled h) {
#if defined(_MSC_VER)
  (void)h;
  return true;
#else
  return h == Handled::Cxx;  // MinGW's GCC has no __try
#endif
}

void crash_now(Crash c) {
  status("crash kind=%s thread=%s", crash_name(c), GetCurrentThreadId() == game_thread_id() ? "game" : "worker");
  status_flush();
  switch (c) {
    case Crash::AvRead: touch_read(0x10); break;
    case Crash::AvWrite: touch_write(0x10); break;
    case Crash::AvExec: run_data(); break;
    case Crash::DivZero: {
      volatile int one = 1;
      volatile int zero = 0;
      g_sink = divide(&one, &zero);
      break;
    }
    case Crash::Illegal: __builtin_trap();
    case Crash::StackOverflow: (void)recurse_from_fixed_depth(); break;
    case Crash::Breakpoint: __debugbreak(); break;
    case Crash::FastFail: __fastfail(7); break;
    case Crash::Terminate:
      TerminateProcess(GetCurrentProcess(), options().exit_code != 0 ? options().exit_code : kTerminateCode);
      break;
    case Crash::None: break;
  }
  // Reached only if a fault was swallowed and execution went on: end the way a game would that cannot go on.
  status("crash survived kind=%s", crash_name(c));
  status_flush();
  ExitProcess(0x46470098);
}

void crash_on_worker(Crash c) {
  void* param = nullptr;
  std::memcpy(&param, &c, sizeof c);
  HANDLE t = CreateThread(nullptr, 0, crash_thread, param, 0, nullptr);
  if (t != nullptr) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
  }
}

bool handled_exception(Handled h) {
  bool ok = false;
#if defined(_MSC_VER)
  if (h == Handled::Seh) ok = handled_seh();
#endif
  if (h == Handled::Cxx) ok = handled_cxx();
  if (ok) ++g_handled;
  return ok;
}

std::uint32_t handled_count() { return g_handled.load(); }

void install_game_filter() {
  const LPTOP_LEVEL_EXCEPTION_FILTER previous = SetUnhandledExceptionFilter(game_filter);
  status("filter installed replaced=%s", previous == nullptr ? "none" : previous == game_filter ? "self" : "other");
}

}  // namespace fakegame
