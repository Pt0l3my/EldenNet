#pragma once
// en-gal's platform seam (ARCH §6.4, DESIGN §4.7): the few primitives the game-access layer needs from the process it
// runs in, behind one interface, so that everything above it (discovery, the reader's rules, the read groups, the
// task's cadence) is portable C++ tested on Linux over FakeSys, a byte-buffer process of the tests' own, and runs in
// game over NativeSys (native_sys.hpp, clang-cl only).
//
// The seam is narrower than ARCH §6.4's sketch: discovery is not the SysLayer's but en-gal's own portable code over the
// image the SysLayer shows (discovery.hpp), so FakeSys tests the same code that runs in game; and the SysLayer keeps
// only what needs the platform: the module and a guarded view of its image, the copy primitive, the known stacks, the
// clock, the getter fallback and the one call into the game that registers the task (R2).
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "en/gal/layout.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/reader.hpp"
#include "en/proto/expected.hpp"
#include "en/proto/thread.hpp"

namespace en::gal {

// The time a task invocation runs at (ARCH §6.4): a monotonic clock in microseconds, and the time since the last one.
struct FrameCtx {
  std::uint64_t qpc_now_us = 0;
  std::uint32_t dt_us = 0;
};

// What the trampoline runs on the game thread, once per invocation, with the token it issued (R1).
class TaskBody {
 public:
  virtual ~TaskBody() = default;
  virtual void run(const proto::GameThread& gt, const FrameCtx& frame) = 0;

 protected:
  TaskBody() = default;
  TaskBody(const TaskBody&) = default;
  TaskBody& operator=(const TaskBody&) = default;
};

// The trampoline's body call (task.cpp, the one place that issues the token: ARCH §10.1's R1 grep): ffi_guard on the
// task component, the token, then the body. A body that throws trips the component, and every later call returns at
// once (R3): the task is inert from then on. NativeSys's run slot and FakeSys's tick both come here.
void run_task(TaskBody& body, const FrameCtx& frame) noexcept;

// A copy primitive (ARCH §6.5): the SEH-guarded copy, ReadProcessMemory on our own process (S0-D's alternative, which
// raises no exception), or a test's fake. It copies dst.size() bytes from `address` and answers whether every byte
// arrived; a failed copy leaves dst zeroed. It never throws and is never given an address the reader's rules refuse.
class Copier {
 public:
  virtual ~Copier() = default;
  [[nodiscard]] virtual CopyOutcome copy(std::uint64_t address, std::span<std::byte> dst) noexcept = 0;

 protected:
  Copier() = default;
  Copier(const Copier&) = default;
  Copier& operator=(const Copier&) = default;
};

// An address range [begin, end).
struct AddressRange {
  std::uint64_t begin = 0;
  std::uint64_t end = 0;

  [[nodiscard]] constexpr bool overlaps(std::uint64_t at, std::uint64_t len) const noexcept {
    return len != 0 && at < end && begin < at + len;
  }
};

// The thread stacks the reader is never pointed into (WHY§111): reading a stack's guard page lifts it, and Windows grows
// a stack through that page. A fixed table, filled on T1 before the task is registered and by the game thread at its
// first tick, read on the game thread only; nothing allocates.
class StackRanges {
 public:
  static constexpr std::size_t kMax = 256;
  // Adds a range; false when the table is full or the range is empty.
  bool add(AddressRange r) noexcept;
  // Empties the table, or takes out every range that overlaps `r` (NativeSys's rescan, on T1 before the task is
  // registered: no reader runs yet).
  void clear() noexcept { n_ = 0; }
  void remove(AddressRange r) noexcept;
  [[nodiscard]] bool overlaps(std::uint64_t at, std::uint64_t len) const noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return n_; }
  [[nodiscard]] AddressRange at(std::size_t i) const noexcept { return i < n_ ? r_[i] : AddressRange{}; }

 private:
  std::array<AddressRange, kMax> r_{};
  std::size_t n_ = 0;
};

enum class SysError : std::uint8_t {
  Unsupported,  // this SysLayer cannot do it
  Twice,        // the task was registered already: there is one registration per process, never retried
  BadShape,     // the task's shape is out of its bounds (slots, run slot, object size)
  Faulted,      // the call into the game raised an exception, caught by SEH (ARCH §3.2's Register → banner)
  NotInit,      // the getter fallback after registration (ARCH §6.2: init only)
  Unreadable,   // the answer could not be copied
};

[[nodiscard]] const char* to_string(SysError e) noexcept;

// Our task object's shape and its registration (ARCH §6.3's [task], §6.4).
struct TaskShape {
  std::uint32_t vtable_slots = 0;  // our fake vtable's entries: every one a guarded no-op but the run slot
  std::uint32_t run_slot = 0;
  std::uint32_t object_size = 0;   // the zeroed bytes after the vtable pointer, which the game's hub owns
  std::uint32_t group_index = 0;   // the group our task joins
  RegisterAbi abi;                 // which register carries the task, the instance and the group
  // R1 (the probe round, O89): the run slot is not known yet, so every slot counts its calls and runs the body; the
  // slot counts then say which one the game calls every frame. `run_slot` is ignored.
  bool every_slot = false;
};

inline constexpr std::uint32_t kMaxTaskSlots = 64;

class SysLayer {
 public:
  virtual ~SysLayer() = default;

  // ---- T1, at init ----
  // The main module's base address, and its mapped image as a source whose reads are guarded copies (discovery).
  [[nodiscard]] virtual std::uint64_t module_base() const noexcept = 0;
  [[nodiscard]] virtual const ByteSource& image() const noexcept = 0;
  // The init-only getter fallback (ARCH §6.2 step 3; R2): calls the getter at `getter` with the class object in rcx,
  // once per candidate, under SEH, before registration, and answers the address it returned. Refused after
  // registration (NotInit).
  [[nodiscard]] virtual proto::Expected<std::uint64_t, SysError> call_getter(std::uint64_t getter,
                                                                             std::uint64_t class_object) noexcept = 0;
  // Builds our task object (ARCH §6.4: a pointer to a fake vtable, the zeroed bytes, our body) and calls
  // register_task at `function` once, with `instance`, the object and the group in the shape's registers. The object
  // and the body live for the life of the process and are never unregistered (R14). Answers rax as the call left it
  // (R1 spike.8, WHY§240: a registration that answers whether it kept the task; one that answers nothing leaves rax as
  // its last instruction did).
  [[nodiscard]] virtual proto::Expected<std::uint64_t, SysError> register_task(std::uint64_t function,
                                                                               std::uint64_t instance,
                                                                               const TaskShape& shape,
                                                                               TaskBody& body) noexcept = 0;

  // ---- Either thread ----
  [[nodiscard]] virtual Copier& copier() noexcept = 0;
  [[nodiscard]] virtual StackRanges& stacks() noexcept = 0;
  // Adds the calling thread's own stack to stacks() (T1 at init, the game thread at its first tick).
  virtual void note_own_stack() noexcept = 0;
  // A monotonic clock, µs (QPC in game).
  [[nodiscard]] virtual std::uint64_t now_us() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t thread_id() const noexcept = 0;
  // How many times the game called each slot of our fake vtable (the probe's slot counts, R1); 0 past the shape.
  [[nodiscard]] virtual std::uint64_t slot_calls(std::uint32_t slot) const noexcept = 0;
  // Our task object's address once it is made (its vtable pointer, then the bytes the hub owns), else 0: R1 copies it
  // back to see how many of those bytes the game's hub wrote.
  [[nodiscard]] virtual std::uint64_t task_object() const noexcept = 0;
  // A read the stack table refused, checked again (the game thread, on that rare path alone): each range overlapping it
  // whose allocation is no longer a stack (a thread that ended, its memory reused by the game: WHY§190) leaves the
  // table. Answers whether the read no longer overlaps any range. A SysLayer that cannot tell answers false.
  virtual bool forget_stale_stacks(std::uint64_t address, std::uint64_t len) noexcept {
    (void)address;
    (void)len;
    return false;
  }

 protected:
  SysLayer() = default;
  SysLayer(const SysLayer&) = default;
  SysLayer& operator=(const SysLayer&) = default;
};

}  // namespace en::gal
