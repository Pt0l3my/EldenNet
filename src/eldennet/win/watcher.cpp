// T10, the watcher (dll.hpp; ARCH §1.2, §5.3): every 60 s the module clause of the anti-cheat check again, and whether
// Seamless Co-op's ersc.dll is loaded (C14's atomics, which the core's gate reads through each snapshot's session).
// Hot reload (ARCH §11.6) waits for content: there is nothing to reload in v0.1.
#include "dll.hpp"
#include "en/proto/ffi_guard.hpp"
#include "en/shell/names.hpp"

namespace eldennet {

namespace {

constexpr DWORD kPeriodMs = 60'000;

DWORD WINAPI watcher_main(void* param) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, DWORD{1}, [&]() -> DWORD {
    Shell& s = *static_cast<Shell*>(param);
    for (;;) {
      Sleep(kPeriodMs);
      bool anticheat = false;
      bool ersc = false;
      for (const std::wstring& m : module_names()) {
        anticheat = anticheat || en::shell::anticheat_module(m);
        ersc = ersc || en::shell::ersc_module(m);
      }
      if (anticheat && !s.anticheat.exchange(true)) {
        log(en::shell::Line("anticheat").word("when", "later").flag("hit", true), en::shell::LogLevel::Warn);
        SetEvent(s.wake);
      }
      if (ersc != s.ersc.exchange(ersc)) log(en::shell::Line("ersc").flag("loaded", ersc));
    }
  });
}

}  // namespace

bool start_watcher_thread(Shell& s) noexcept {
  HANDLE t = CreateThread(nullptr, 0, &watcher_main, &s, 0, nullptr);
  if (t == nullptr) return false;
  CloseHandle(t);
  return true;
}

}  // namespace eldennet
