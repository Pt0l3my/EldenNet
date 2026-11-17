// eldennet.dll's load and its crash filter's look-up (sys_boot.hpp; ARCH §3.1, §1.5): the casts the `eldennet` module
// may not hold (ARCH §2.4), each with its SAFETY note. Windows and clang-cl only.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "en/gal/sys_boot.hpp"

namespace en::gal::sys::boot {

bool on_attach(void* module, InitMain init) noexcept {
  // SAFETY: `module` is the HINSTANCE the loader gave DllMain; DisableThreadLibraryCalls takes it as an HMODULE.
  DisableThreadLibraryCalls(static_cast<HMODULE>(module));
  // InitMain is LPTHREAD_START_ROUTINE on x64 (one calling convention), less its noexcept.
  const LPTHREAD_START_ROUTINE start = init;
  HANDLE t = CreateThread(nullptr, 0, start, module, 0, nullptr);
  if (t == nullptr) return false;
  CloseHandle(t);
  return true;
}

bool pin(void* module) noexcept {
  HMODULE pinned = nullptr;
  // SAFETY: the API's documented use: with FROM_ADDRESS, the "name" is an address inside the module (its base here).
  return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            static_cast<LPCWSTR>(module), &pinned) != FALSE;
}

std::uint64_t address_of_module(void* module) noexcept {
  // SAFETY: a handle is the module's base address; only its number is kept.
  return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(module));
}

void fault_site(std::uint64_t address, FaultSite& out) noexcept {
  out = FaultSite{};
  HMODULE module = nullptr;
  // SAFETY: the faulting address, as a number the exception record gave; the loader only looks it up in its lists.
  const auto at = reinterpret_cast<LPCWSTR>(static_cast<std::uintptr_t>(address));
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, at,
                          &module) ||
      module == nullptr) {
    return;
  }
  const std::uint64_t base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(module));
  out.in_module = true;
  out.offset = address >= base ? address - base : 0;
  wchar_t path[MAX_PATH];
  const DWORD n = GetModuleFileNameW(module, path, MAX_PATH);
  DWORD start = 0;
  for (DWORD i = 0; i < n && i < MAX_PATH; ++i) {
    if (path[i] == L'\\' || path[i] == L'/') start = i + 1;
  }
  std::size_t k = 0;
  for (DWORD i = start; i < n && i < MAX_PATH && k + 1 < sizeof out.name / sizeof out.name[0]; ++i) out.name[k++] = path[i];
  out.name[k] = L'\0';
  out.name_len = k;
}

}  // namespace en::gal::sys::boot
