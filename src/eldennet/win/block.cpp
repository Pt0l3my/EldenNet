// EldenNet's side of the launcher's watch block (dll.hpp; en-proto's watch.hpp; O71, O90): T1's states and the status
// the dev console shows, the Marks, the launcher's requests, and the crash filter.
//
// The crash filter (S0-D): installed once the game has started (after the engine wait, or the window on the banner
// path), since an early filter is replaced by the game's own C runtime as it starts (WHY§172); chained to the filter
// before it; guarded (one record per process, a second fault inside it passes straight on); allocation-free and within
// a few KB of stack (after a stack overflow the C runtime cannot even format: nothing of it is called, and the module
// look-up is skipped); it writes the record into the block, wakes the watcher and waits for it at most 2 s. A stack
// overflow alone is not passed on: the process ends in the filter with its code (WHY§196).
#include <array>

#include "dll.hpp"
#include "en/gal/sys_boot.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet::block {

namespace w = en::proto::watch;

namespace {

// Constant-initialised, never destroyed (R14).
constinit std::atomic<w::Block*> g_block{nullptr};
constinit HANDLE g_crash_event = nullptr;
constinit HANDLE g_ack_event = nullptr;
constinit std::atomic<DWORD> g_init{0};
constinit std::atomic<DWORD> g_game{0};
constinit std::atomic<DWORD> g_reader{0};
constinit std::atomic<bool> g_in_filter{false};
constinit std::atomic<bool> g_installed{false};
constinit LPTOP_LEVEL_EXCEPTION_FILTER g_previous = nullptr;
// What T1 reported, kept whether or not a block is attached (the F11 view reads it).
constinit std::atomic<std::uint32_t> g_state{0};
constinit std::atomic<std::uint32_t> g_items_resolved{0};
constinit std::atomic<std::uint32_t> g_items_total{0};
constinit std::atomic<std::uint32_t> g_passes{0};
// The Marks with no block (another loader, and no watcher): EldenNet's own ring, of the block's shape, which only its
// GraceUI Mark writes.
constinit w::Marks g_own_marks{};

HANDLE open_event(std::wstring_view prefix, DWORD pid) {
  const std::wstring name = std::wstring(prefix) + std::to_wstring(pid);
  return OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, name.c_str());
}

LONG WINAPI crash_filter(EXCEPTION_POINTERS* ep) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, LONG{EXCEPTION_CONTINUE_SEARCH}, [&]() -> LONG {
    // A fault inside the filter, or on a second thread at once, goes straight on to the filter before ours.
    if (!g_in_filter.exchange(true, std::memory_order_acq_rel)) {
      w::Block* b = g_block.load(std::memory_order_acquire);
      std::uint32_t none = static_cast<std::uint32_t>(w::CrashState::None);
      if (b != nullptr && ep != nullptr && ep->ExceptionRecord != nullptr &&
          b->crash.state.compare_exchange_strong(none, static_cast<std::uint32_t>(w::CrashState::Writing))) {
        w::Crash& c = b->crash;
        const DWORD code = ep->ExceptionRecord->ExceptionCode;
        c.code = code;
        const DWORD tid = GetCurrentThreadId();
        c.thread = static_cast<std::uint32_t>(tid == g_reader.load() ? w::ThreadKind::Reader
                                              : tid == g_game.load() ? w::ThreadKind::Game
                                              : tid == g_init.load() ? w::ThreadKind::Init
                                                                     : w::ThreadKind::Other);
        c.chained = g_previous != nullptr ? 1u : 0u;
        if (code != EXCEPTION_STACK_OVERFLOW) {
          en::gal::sys::boot::FaultSite site;
          std::uint64_t at = 0;
          static_assert(sizeof at == sizeof ep->ExceptionRecord->ExceptionAddress);
          std::memcpy(&at, &ep->ExceptionRecord->ExceptionAddress, sizeof at);
          en::gal::sys::boot::fault_site(at, site);
          if (site.in_module) {
            c.offset = site.offset;
            w::set_module_name(c, site.name, site.name_len);
          }
        }
        c.at_ms = mono_ms();
        c.state.store(static_cast<std::uint32_t>(w::CrashState::Written), std::memory_order_release);
        if (g_crash_event != nullptr && b->control.watcher_pid.load(std::memory_order_acquire) != 0) {
          SetEvent(g_crash_event);
          if (g_ack_event != nullptr) WaitForSingleObject(g_ack_event, w::kCrashWaitMs);
        }
      }
      // A stack overflow leaves no stack for whatever would handle it after us: the process ends here with its code,
      // as it would end anyway (WHY§196: under Wine, passing it on hung the process for good).
      if (ep != nullptr && ep->ExceptionRecord != nullptr &&
          ep->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
        TerminateProcess(GetCurrentProcess(), EXCEPTION_STACK_OVERFLOW);
      }
    }
    return g_previous != nullptr ? g_previous(ep) : LONG{EXCEPTION_CONTINUE_SEARCH};
  });
}

w::Block* opened() noexcept { return g_block.load(std::memory_order_acquire); }

}  // namespace

bool attach() noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, false, [] {
    if (opened() != nullptr) return true;
    const DWORD pid = GetCurrentProcessId();
    const std::wstring name = std::wstring(w::kBlockPrefix) + std::to_wstring(pid);
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
    if (mapping == nullptr) return false;
    void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, w::kBlockSize);
    if (view == nullptr) return false;
    // The launcher constructed the block in this page (en-proto's watch.hpp: one page, fixed offsets, its atomics
    // lock-free); the header is checked before anything else is read or written. The handle and the view stay open
    // for the life of the process.
    auto* b = static_cast<w::Block*>(view);
    for (int i = 0; i < 50 && !w::valid(*b); ++i) Sleep(20);  // a watcher that has just made it writes the magic last
    if (!w::valid(*b) || b->header.game_pid != pid) return false;
    g_crash_event = open_event(w::kCrashEventPrefix, pid);
    g_ack_event = open_event(w::kAckEventPrefix, pid);
    g_block.store(b, std::memory_order_release);
    return true;
  });
}

bool start_watcher(const Paths& paths) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, false, [&] {
    const DWORD pid = GetCurrentProcessId();
    const std::wstring exe = paths.dll_dir + L"\\eldennet-launcher.exe";
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    std::wstring cmd = L"\"" + exe + L"\" --watch " + std::to_wstring(pid) + L" --no-dialogs";
    if (paths.data_overridden) cmd += L" --appdata \"" + paths.data_root + L"\"";
    if (!start_process(exe, cmd)) return false;
    const std::wstring name = std::wstring(w::kBlockPrefix) + std::to_wstring(pid);
    for (int i = 0; i < 150; ++i) {  // at most 3 s for the watcher to make the block
      if (HANDLE m = OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str()); m != nullptr) {
        CloseHandle(m);
        return true;
      }
      Sleep(20);
    }
    return false;
  });
}

bool attached() noexcept { return opened() != nullptr; }

void install_filter() noexcept {
  if (opened() == nullptr || g_installed.exchange(true)) return;
  g_previous = SetUnhandledExceptionFilter(&crash_filter);
}

void state(w::InitState s) noexcept {
  g_state.store(static_cast<std::uint32_t>(s), std::memory_order_relaxed);
  if (w::Block* b = opened()) {
    b->status.init.store(static_cast<std::uint32_t>(s), std::memory_order_release);
    b->status.updated_ms.store(mono_ms(), std::memory_order_relaxed);
  }
}

void ready_ms(std::uint32_t ms) noexcept {
  if (w::Block* b = opened()) b->status.ready_ms.store(ms, std::memory_order_relaxed);
}

void items(std::uint32_t resolved, std::uint32_t total, std::uint32_t passes) noexcept {
  g_items_resolved.store(resolved, std::memory_order_relaxed);
  g_items_total.store(total, std::memory_order_relaxed);
  g_passes.store(passes, std::memory_order_relaxed);
  if (w::Block* b = opened()) {
    b->status.items_resolved.store(resolved, std::memory_order_relaxed);
    b->status.items_total.store(total, std::memory_order_relaxed);
    b->status.discovery_passes.store(passes, std::memory_order_relaxed);
  }
}

void counts(std::uint64_t snapshots, std::uint64_t faults, std::uint64_t tripped, std::uint32_t p99) noexcept {
  if (w::Block* b = opened()) {
    b->status.snapshots.store(snapshots, std::memory_order_relaxed);
    b->status.faults.store(faults, std::memory_order_relaxed);
    b->status.tripped.store(tripped, std::memory_order_relaxed);
    b->status.task_us_p99.store(p99, std::memory_order_relaxed);
    b->status.updated_ms.store(mono_ms(), std::memory_order_relaxed);
  }
}

void core(std::uint32_t phase, std::uint32_t level, std::uint32_t banner, std::uint32_t flags) noexcept {
  if (w::Block* b = opened()) {
    b->status.phase.store(phase, std::memory_order_relaxed);
    b->status.level.store(level, std::memory_order_relaxed);
    b->status.banner.store(banner, std::memory_order_relaxed);
    b->status.flags.store(flags, std::memory_order_relaxed);
  }
}

void game_thread(std::uint32_t tid) noexcept { g_game.store(tid, std::memory_order_relaxed); }
void reader_thread(std::uint32_t tid) noexcept { g_reader.store(tid, std::memory_order_relaxed); }
void init_thread(std::uint32_t tid) noexcept { g_init.store(tid, std::memory_order_relaxed); }

w::Spike spike() noexcept {
  const w::Block* b = opened();
  const std::uint32_t s = b == nullptr ? 0 : b->control.spike.load(std::memory_order_acquire);
  return s < w::kSpikeCount ? static_cast<w::Spike>(s) : w::Spike::None;
}

w::ProbeRequest probe_request() noexcept {
  const w::Block* b = opened();
  const std::uint32_t p = b == nullptr ? 0 : b->control.probe.load(std::memory_order_acquire);
  // What the launcher may ask: R1, or the read-only round. Anything else there is no request.
  return p == static_cast<std::uint32_t>(w::ProbeRequest::R1) || p == static_cast<std::uint32_t>(w::ProbeRequest::Read)
             ? static_cast<w::ProbeRequest>(p)
             : w::ProbeRequest::None;
}

void probe_answer(w::ProbeRequest answer) noexcept {
  if (w::Block* b = opened()) b->control.probe.store(static_cast<std::uint32_t>(answer), std::memory_order_release);
}

bool crash_requested() noexcept {
  const w::Block* b = opened();
  return b != nullptr && b->control.crash_request.load(std::memory_order_acquire) != 0;
}

w::InitState init_state() noexcept {
  const std::uint32_t s = g_state.load(std::memory_order_relaxed);
  return s < w::kInitStateCount ? static_cast<w::InitState>(s) : w::InitState::None;
}

Items items_seen() noexcept {
  return Items{g_items_resolved.load(std::memory_order_relaxed), g_items_total.load(std::memory_order_relaxed),
               g_passes.load(std::memory_order_relaxed)};
}

namespace {
w::Marks& ring() noexcept {
  w::Block* b = opened();
  return b != nullptr ? b->marks : g_own_marks;
}
}  // namespace

std::uint32_t marks() noexcept { return ring().count.load(std::memory_order_acquire); }

std::uint64_t mark_ms(std::uint32_t n) noexcept {
  if (n == 0) return 0;
  return ring().mono_ms[(n - 1) % w::kMarkRing].load(std::memory_order_relaxed);
}

w::MarkSource mark_source() noexcept { return w::mark_source(ring()); }

bool take_marks() noexcept {
  w::Marks& m = ring();
  if (w::mark_source(m) == w::MarkSource::EldenNet) return true;
  return w::eldennet_asks(m);  // the launcher's console, if any, lets its Pause go and hands the ring over
}

std::uint32_t add_own_mark(std::uint64_t at_ms) noexcept {
  w::Marks& m = ring();
  return w::mark_source(m) == w::MarkSource::EldenNet ? w::add_mark(m, at_ms) : 0;
}

}  // namespace eldennet::block
