// galtest.dll's side of the watch block (watch_client.hpp).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "watch_client.hpp"

#include <windows.h>

#include <atomic>
#include <string>

#include "en/proto/ffi_guard.hpp"

namespace galtest::watch {

namespace w = en::proto::watch;

namespace {

// Made on T1, never destroyed (R14): plain pointers and handles, constant-initialised.
w::Block* g_block = nullptr;
HANDLE g_crash_event = nullptr;
HANDLE g_ack_event = nullptr;
DWORD g_t1 = 0;
std::atomic<DWORD> g_game{0};
LPTOP_LEVEL_EXCEPTION_FILTER g_previous = nullptr;

HANDLE open_event(std::wstring_view prefix, DWORD pid) {
  const std::wstring name = std::wstring(prefix) + std::to_wstring(pid);
  return OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, name.c_str());
}

std::uint64_t now_ms() {
  LARGE_INTEGER f{}, c{};
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&c);
  return w::mono_ms(static_cast<std::uint64_t>(c.QuadPart), static_cast<std::uint64_t>(f.QuadPart));
}

// The crash filter: the record, written once and with no allocation; the watcher woken; at most 2 s waited for it to
// list the modules; then the filter installed before ours, if any (chained).
LONG WINAPI crash_filter(EXCEPTION_POINTERS* ep) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, LONG{EXCEPTION_CONTINUE_SEARCH}, [&]() -> LONG {
    std::uint32_t none = static_cast<std::uint32_t>(w::CrashState::None);
    if (g_block != nullptr && ep != nullptr && ep->ExceptionRecord != nullptr &&
        g_block->crash.state.compare_exchange_strong(none, static_cast<std::uint32_t>(w::CrashState::Writing))) {
      w::Crash& c = g_block->crash;
      c.code = ep->ExceptionRecord->ExceptionCode;
      const DWORD tid = GetCurrentThreadId();
      c.thread = static_cast<std::uint32_t>(tid == g_game.load() ? w::ThreadKind::Game
                                            : tid == g_t1        ? w::ThreadKind::Init
                                                                 : w::ThreadKind::Other);
      c.chained = g_previous != nullptr ? 1u : 0u;
      HMODULE module = nullptr;
      const auto* address = static_cast<const char*>(ep->ExceptionRecord->ExceptionAddress);
      if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>(address), &module) &&  // the API's documented use
          module != nullptr) {
        // SAFETY: both point into the same mapped module; the difference is the offset the record keeps.
        c.offset = static_cast<std::uint64_t>(address - reinterpret_cast<const char*>(module));
        wchar_t path[MAX_PATH];
        const DWORD n = GetModuleFileNameW(module, path, MAX_PATH);
        DWORD start = 0;
        for (DWORD i = 0; i < n; ++i) {
          if (path[i] == L'\\' || path[i] == L'/') start = i + 1;
        }
        w::set_module_name(c, path + start, n - start);
      }
      c.at_ms = now_ms();
      c.state.store(static_cast<std::uint32_t>(w::CrashState::Written), std::memory_order_release);
      if (g_crash_event != nullptr && g_block->control.watcher_pid.load(std::memory_order_acquire) != 0) {
        SetEvent(g_crash_event);
        if (g_ack_event != nullptr) WaitForSingleObject(g_ack_event, w::kCrashWaitMs);
      }
    }
    return g_previous != nullptr ? g_previous(ep) : LONG{EXCEPTION_CONTINUE_SEARCH};
  });
}

}  // namespace

bool attach() noexcept {
  const DWORD pid = GetCurrentProcessId();
  const std::wstring name = std::wstring(w::kBlockPrefix) + std::to_wstring(pid);
  HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
  if (mapping == nullptr) return false;
  void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, w::kBlockSize);
  if (view == nullptr) return false;
  // SAFETY: the launcher constructed the block in this page; the header is checked before anything else.
  auto* block = static_cast<w::Block*>(view);
  // A watcher that has just made the page writes its header a moment later, the magic last.
  for (int i = 0; i < 50 && !w::valid(*block); ++i) Sleep(20);
  if (!w::valid(*block) || block->header.game_pid != pid) return false;
  g_crash_event = open_event(w::kCrashEventPrefix, pid);
  g_ack_event = open_event(w::kAckEventPrefix, pid);
  g_t1 = GetCurrentThreadId();
  g_block = block;
  return true;
}

void install_filter() noexcept {
  if (g_block != nullptr && g_previous == nullptr) g_previous = SetUnhandledExceptionFilter(crash_filter);
}

bool start_watcher(const std::string& launcher, const std::string& args) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, false, [&] {
    const DWORD pid = GetCurrentProcessId();
    const std::wstring name = std::wstring(w::kBlockPrefix) + std::to_wstring(pid);
    if (HANDLE m = OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str()); m != nullptr) {
      CloseHandle(m);
      return true;  // a launcher watches already
    }
    const auto wide = [](const std::string& s) {
      std::wstring out(s.size(), L'\0');
      const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(),
                                        static_cast<int>(out.size()));
      out.resize(n > 0 ? static_cast<std::size_t>(n) : 0);
      return out;
    };
    const std::wstring exe = wide(launcher);
    std::wstring cmd = L"\"" + exe + L"\" --watch " + std::to_wstring(pid) + L" " + wide(args);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    for (int i = 0; i < 150; ++i) {
      if (HANDLE m = OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str()); m != nullptr) {
        CloseHandle(m);
        return true;
      }
      Sleep(20);
    }
    return false;
  });
}

bool attached() noexcept { return g_block != nullptr; }

void state(w::InitState s) noexcept {
  if (g_block == nullptr) return;
  g_block->status.init.store(static_cast<std::uint32_t>(s), std::memory_order_release);
  g_block->status.updated_ms.store(now_ms(), std::memory_order_relaxed);
}

void items(std::uint32_t resolved, std::uint32_t total, std::uint32_t passes) noexcept {
  if (g_block == nullptr) return;
  g_block->status.items_resolved.store(resolved, std::memory_order_relaxed);
  g_block->status.items_total.store(total, std::memory_order_relaxed);
  g_block->status.discovery_passes.store(passes, std::memory_order_relaxed);
}

void counts(std::uint64_t snapshots, std::uint64_t faults, std::uint64_t tripped, std::uint32_t task_us_p99) noexcept {
  if (g_block == nullptr) return;
  g_block->status.snapshots.store(snapshots, std::memory_order_relaxed);
  g_block->status.faults.store(faults, std::memory_order_relaxed);
  g_block->status.tripped.store(tripped, std::memory_order_relaxed);
  g_block->status.task_us_p99.store(task_us_p99, std::memory_order_relaxed);
  g_block->status.updated_ms.store(now_ms(), std::memory_order_relaxed);
}

void core(std::uint32_t phase, std::uint32_t level, std::uint32_t banner, std::uint32_t flags) noexcept {
  if (g_block == nullptr) return;
  g_block->status.phase.store(phase, std::memory_order_relaxed);
  g_block->status.level.store(level, std::memory_order_relaxed);
  g_block->status.banner.store(banner, std::memory_order_relaxed);
  g_block->status.flags.store(flags, std::memory_order_relaxed);
}

void ready_ms(std::uint32_t ms) noexcept {
  if (g_block != nullptr) g_block->status.ready_ms.store(ms, std::memory_order_relaxed);
}

void game_thread(std::uint32_t tid) noexcept { g_game.store(tid, std::memory_order_relaxed); }

bool crash_requested() noexcept {
  return g_block != nullptr && g_block->control.crash_request.load(std::memory_order_acquire) != 0;
}

std::uint32_t marks() noexcept { return g_block == nullptr ? 0 : g_block->marks.count.load(std::memory_order_acquire); }

}  // namespace galtest::watch
