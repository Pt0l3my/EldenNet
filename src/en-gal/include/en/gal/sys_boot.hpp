#pragma once
// The Win32 work of eldennet.dll's load and of its crash filter that needs an address turned into a pointer or a
// module (ARCH §2.4: such casts live in `sys`, and the `eldennet` module holds none; §3.1). Windows only, built with
// NativeSys (src/sys/boot.cpp).
// - on_attach: DllMain's one step (ARCH §3.1): DisableThreadLibraryCalls and CreateThread(init), nothing else under
//   the loader lock; `init` gets the module's handle.
// - pin: the module pinned for the life of the process (ARCH §1.5), so no FreeLibrary can unload code the game's hub
//   and the crash filter point into.
// - fault_site: where a faulting address lies, for the crash record: its module's file name (no folder) and the
//   offset in it. Allocation-free and with a small stack (a crash filter after a stack overflow), and it touches no
//   memory but the loader's own lists.
#include <cstddef>
#include <cstdint>

namespace en::gal::sys::boot {

using InitMain = unsigned long (*)(void* module) noexcept;

// False when the thread could not be started (nothing else is tried under the loader lock).
bool on_attach(void* module, InitMain init) noexcept;
// Pins the module holding `module` (its handle); false when the loader refused.
bool pin(void* module) noexcept;

struct FaultSite {
  bool in_module = false;
  std::uint64_t offset = 0;  // from the module's base
  wchar_t name[64] = {};     // the module's file name, cut at 63 characters
  std::size_t name_len = 0;
};
// `address` is the exception record's ExceptionAddress, as a number.
void fault_site(std::uint64_t address, FaultSite& out) noexcept;

// The address of our own module's marker export, or of any symbol in it, as a number (the module's base for `pin`).
[[nodiscard]] std::uint64_t address_of_module(void* module) noexcept;

}  // namespace en::gal::sys::boot
