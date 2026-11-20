// NativeSys's two calls into the game (sys_internal.hpp; ARCH §6.2 step 3, §6.4; R2): register_task, once, and the FD4
// name getter, once per candidate before it. ARCH §10.1's R2 grep allows a cast of an address to a function type here
// and in the getter's file alone; both live here. Each call runs under SEH from T1: a fault ends the call, not the game
// (ARCH §3.2's Register → banner).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "sys_internal.hpp"

namespace en::gal::sys {

namespace {

// rax is kept (R1 spike.8, WHY§240): a registration may answer whether it kept the task. One that answers nothing
// leaves rax as its last instruction did, which the caller reads as a number only.
using RegisterFn = std::uint64_t (*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);
using GetterFn = std::uint64_t (*)(std::uint64_t);

// The calls, each in a frame of its own (clang's SEH covers the calls a __try makes; WHY§111).
__declspec(noinline) std::uint64_t invoke_register(RegisterFn fn, const std::uint64_t* args) {
  return fn(args[0], args[1], args[2], args[3]);
}
__declspec(noinline) std::uint64_t invoke_getter(GetterFn fn, std::uint64_t class_object) { return fn(class_object); }

}  // namespace

CallOutcome call_register(std::uint64_t function, const std::array<std::uint64_t, 4>& args) noexcept {
  // SAFETY: `function` is register_task as discovery resolved it (an interior pattern, inside a function of the
  // exception directory, with its prologue and length checked) and the review of the derivation round confirmed (O89);
  // the cast makes the one call EldenNet makes into the game, with the argument registers the layout gives.
  const auto fn = reinterpret_cast<RegisterFn>(static_cast<std::uintptr_t>(function));
  const std::uint64_t* a = args.data();
  CallOutcome out;
  __try {
    out.answer = invoke_register(fn, a);
    out.ok = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    out.ok = false;
    out.code = static_cast<std::uint32_t>(GetExceptionCode());
  }
  return out;
}

CallOutcome call_getter(std::uint64_t getter, std::uint64_t class_object) noexcept {
  // SAFETY: `getter` starts a function of the exception directory that an accepted FD4 site calls with this class
  // object in rcx (ARCH §6.2 steps 2 and 3); called once per candidate, before registration, under SEH.
  const auto fn = reinterpret_cast<GetterFn>(static_cast<std::uintptr_t>(getter));
  CallOutcome out;
  __try {
    out.answer = invoke_getter(fn, class_object);
    out.ok = true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    out.ok = false;
    out.code = static_cast<std::uint32_t>(GetExceptionCode());
  }
  return out;
}

}  // namespace en::gal::sys
