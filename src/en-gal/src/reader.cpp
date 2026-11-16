// en-gal's reader: the one SEH-guarded copy (reader.hpp; ARCH §6.5, DESIGN R4), in C++ with __try and __except inline,
// compiled by clang-cl: no separate shim (ARCH §2.6).
#include "en/gal/reader.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>

namespace en::gal {

static_assert(sizeof(void*) == sizeof(std::uint64_t), "en-gal reads a 64-bit address space");

namespace {

// The faults a read can meet: a page not mapped, not committed or not readable; a page its file could not bring in;
// a guard page. Any other exception is not the copy's, and goes on to the next handler.
int filter(unsigned long code) noexcept {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_GUARD_PAGE:
      return EXCEPTION_EXECUTE_HANDLER;
    default:
      return EXCEPTION_CONTINUE_SEARCH;
  }
}

// The copy, in a frame of its own. Clang's SEH on x64 catches a fault raised in a function that the __try calls, not
// one raised by an instruction of the __try's own frame, and clang makes a memcpy written in the __try an intrinsic
// that is no call at all: so the __try calls this, which is never inlined.
// SAFETY: `src` may hold any address, which is what the caller's guard is for, and is only read; `dst` is the
// caller's own storage, valid for `len` bytes.
__declspec(noinline) void copy_bytes(void* dst, const void* src, std::size_t len) noexcept {
  std::memcpy(dst, src, len);
}

}  // namespace

CopyOutcome seh_copy(std::uint64_t address, std::span<std::byte> dst) noexcept {
  if (dst.empty()) return {true, 0};
  // SAFETY: the one place where en-gal makes a pointer from an address. It lives for the guarded copy below alone:
  // only read through, inside the guard, and never kept.
  const void* src = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address));
  std::byte* const to = dst.data();
  const std::size_t len = dst.size();
  __try {
    copy_bytes(to, src, len);
  } __except (filter(GetExceptionCode())) {
    const auto code = static_cast<std::uint32_t>(GetExceptionCode());
    std::memset(to, 0, len);
    return {false, code};
  }
  return {true, 0};
}

}  // namespace en::gal
