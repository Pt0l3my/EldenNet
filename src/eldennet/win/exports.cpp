// eldennet.dll's two exports (ARCH §2.2, §3.1): DllMain, which only starts T1 (DisableThreadLibraryCalls and
// CreateThread, in en-gal's sys::boot) and does nothing at any other reason (R14: the detach branch is empty), and the
// instance marker R16's first copy looks for in other modules. No cast, no allocation, no lock, no file here.
#include "dll.hpp"
#include "en/gal/sys_boot.hpp"
#include "en/proto/ffi_guard.hpp"

extern "C" __declspec(dllexport) const std::uint32_t eldennet_instance_marker = 0x454E4D4Bu;  // "ENMK"

extern "C" BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, TRUE, [&] {
    if (reason == DLL_PROCESS_ATTACH) en::gal::sys::boot::on_attach(module, &eldennet::init_main);
    return TRUE;
  });
}
