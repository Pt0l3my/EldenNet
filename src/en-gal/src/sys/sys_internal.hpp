#pragma once
// NativeSys's internals (native_sys.hpp), shared by sys/*.cpp: the calls into the game (register.cpp) and the task
// object with its trampoline (trampoline.cpp). Windows and clang-cl only.
#include <array>
#include <cstdint>

#include "en/gal/sys.hpp"

namespace en::gal::sys {

struct CallOutcome {
  bool ok = false;
  std::uint32_t code = 0;     // the exception that stopped the call, when not ok
  std::uint64_t answer = 0;   // rax
};

// register_task at `function`, with the four argument registers rcx, rdx, r8, r9 as `args` gives them; `answer` is rax
// as the call left it (register.cpp).
CallOutcome call_register(std::uint64_t function, const std::array<std::uint64_t, 4>& args) noexcept;
// The FD4 name getter at `getter`, with the class object in rcx (register.cpp).
CallOutcome call_getter(std::uint64_t getter, std::uint64_t class_object) noexcept;

// Our task object (ARCH §6.4): made once, never freed; 0 when one was made already or memory is short (trampoline.cpp).
std::uint64_t make_task_object(const TaskShape& shape, TaskBody& body) noexcept;
std::uint64_t slot_calls(std::uint32_t slot) noexcept;
std::uint64_t task_object() noexcept;  // the block's address, or 0 before it is made
std::uint64_t qpc_us() noexcept;
// The init phase (R2): the getter fallback is refused once register_task has been called.
bool registered() noexcept;
void set_registered() noexcept;

}  // namespace en::gal::sys
