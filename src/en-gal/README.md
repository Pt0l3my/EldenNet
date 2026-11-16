# en-gal: the game access layer

The part of EldenNet's engine that reads the game (ARCH §6), C++20, MIT. For now it holds one thing:
`GuardedReader`'s guarded copy (ARCH §6.5, DESIGN R4), which copies bytes from an address into storage the caller
owns under one structured exception handler and answers whether every byte arrived. Nothing in it knows the game.
Its static library `en_gal` is a module the DLL links (`en_dll_module`).

It needs structured exception handling (`__try`/`__except`), which an MSVC-like compiler has: it builds with clang-cl
(`cmake/clang-cl-xwin.cmake`, ARCH §2.6), and the GCC and Clang builds on Linux and MinGW-w64's skip it, with a line
saying so.

## The guarded copy

```cpp
#include "en/gal/reader.hpp"

std::array<std::byte, 64> scratch{};                                  // storage the caller owns
const en::gal::CopyOutcome out = en::gal::seh_copy(address, scratch);  // address: a std::uint64_t
if (!out.copied) {
  // out.code: 0xC0000005 (access violation), 0xC0000006 (in-page error) or 0x80000001 (guard page); scratch is zeroed
}
```

- `seh_copy(address, dst)` copies `dst.size()` bytes from `address`. An access violation, an in-page error or a guard
  page stops it: it answers `{false, code}` with `dst` zeroed, so nothing of a partial copy survives. Any other
  exception is not the copy's, and goes on to the next handler. An empty `dst` copies nothing and answers `{true, 0}`.
- It never throws, keeps no state and takes no lock, and the pointer it makes from the address lives for the copy
  alone (R4). The plausibility checks, groups, budgets and breakers around it are `GuardedReader`'s, still to come
  (§6.5, §6.7).
- **Why the copy has a frame of its own.** Clang's SEH on x64 covers the calls a `__try` makes, not the instructions
  of its own frame, and clang turns a `memcpy` written in a `__try` into an intrinsic, which is no call: at `/Od` its
  fault goes unhandled, and at `/O2` clang deletes the `__except` altogether. So the `__try` calls `copy_bytes`, which
  is never inlined and does the `memcpy`; the release build (`/O2`, ThinLTO) keeps that call inside the guarded range,
  under `__C_specific_handler`.
- **Guard pages.** Windows lifts a guard page's guard as it raises the fault, so a guard page read once is a guard page
  no more, and its next read goes through. Windows grows a thread's stack through such a page.

## Tests

`cmake --build <clang-cl build> --target en_gal_tests`, then `WINEPREFIX=<dir> wine64 en_gal_tests.exe`, on pages of
the test's own holding invented bytes. A copy from good memory (across two committed pages too) arrives whole. A copy
from a page reserved but not committed, from a guard page (whose guard is gone after), from the null page, the kernel's
half, a no-access page or a released region, and one across the end of a committed region, fails cleanly with the
destination zeroed, and the program goes on. 2,000 rounds of faults and good copies, and four threads faulting at
once, leave nothing behind.
