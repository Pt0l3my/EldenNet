#pragma once
// A fault for the R3 tests: once armed, the next allocation through operator new whose size lies in [min, max) throws
// std::bad_alloc, as running out of memory would, on any thread but the test servers' own (which keep serving). It
// fires once and disarms itself. The test program replaces the global operator new for it (fault_alloc.cpp); unarmed,
// each allocation only reads one flag. Libraries that allocate with malloc (libcurl, WinHTTP) are never touched, so
// the fault lands in our own code inside their callbacks.
#include <cstddef>

namespace en::llm::test::faults {

void arm(std::size_t min_size, std::size_t max_size) noexcept;
void disarm() noexcept;
// Whether the fault armed last has fired.
bool fired() noexcept;
// For the test servers' threads: no fault ever fires on the calling thread.
void exempt_this_thread() noexcept;

}  // namespace en::llm::test::faults
