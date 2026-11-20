#pragma once
// NativeSys (ARCH §6.4; DESIGN §4.7): the SysLayer in game, on Windows, built by clang-cl only (sys/*.cpp). It is the
// only code of en-gal that turns an address into a pointer or a function (ARCH §2.4: `reader` and `sys`), each place
// with its SAFETY note:
// - the main module and a view of its mapped image whose reads are guarded copies, page by page where a range holds a
//   page that is not committed, not readable or a guard page (VirtualQuery first: a guard page is never touched);
// - the copy primitive: the SEH copy (reader.hpp), or ReadProcessMemory on our own process, which raises no exception
//   at all (S0-D's alternative, a setting);
// - the stacks the reader is never pointed into (WHY§111): at construction every allocation of the process that holds
//   a guard page (the shape of a thread's stack; in an image, the guard pages alone), and each thread's own stack as it
//   asks (T1 at init, the game thread at its first tick); a thread made later is covered once it has asked;
// - the init-only getter fallback and the one call into the game that registers our task (sys/register.cpp), both
//   under SEH, from T1; the task object and its fake vtable of guarded no-ops, with the run slot our trampoline
//   (sys/trampoline.cpp), made once and never freed (R14);
// - the clock (QPC, µs) and the thread's id;
// - in a probe build, the regions of the process as VirtualQuery describes them (R1's whole-memory search).
#include <cstdint>
#include <memory>
#include <optional>

#include "en/gal/sys.hpp"
#if EN_PROBE
#include "en/gal/probe.hpp"
#endif

namespace en::gal {

enum class CopyKind : std::uint8_t { Seh, Rpm };

class NativeSys final : public SysLayer {
 public:
  explicit NativeSys(CopyKind copy = CopyKind::Seh);
  NativeSys(const NativeSys&) = delete;
  NativeSys& operator=(const NativeSys&) = delete;
  ~NativeSys() override;

  [[nodiscard]] std::uint64_t module_base() const noexcept override { return base_; }
  [[nodiscard]] const ByteSource& image() const noexcept override;
  [[nodiscard]] proto::Expected<std::uint64_t, SysError> call_getter(std::uint64_t getter,
                                                                     std::uint64_t class_object) noexcept override;
  [[nodiscard]] proto::Expected<std::uint64_t, SysError> register_task(std::uint64_t function, std::uint64_t instance,
                                                                       const TaskShape& shape,
                                                                       TaskBody& body) noexcept override;
  [[nodiscard]] Copier& copier() noexcept override;
  [[nodiscard]] StackRanges& stacks() noexcept override { return stacks_; }
  void note_own_stack() noexcept override;
  [[nodiscard]] std::uint64_t now_us() const noexcept override;
  [[nodiscard]] std::uint32_t thread_id() const noexcept override;
  [[nodiscard]] std::uint64_t slot_calls(std::uint32_t slot) const noexcept override;
  [[nodiscard]] std::uint64_t task_object() const noexcept override;
  bool forget_stale_stacks(std::uint64_t address, std::uint64_t len) noexcept override;

  [[nodiscard]] CopyKind copy_kind() const noexcept { return kind_; }
  // The copy primitive the settings chose (gal.copy): on T1, before the task is registered and reads anything.
  void set_copy_kind(CopyKind kind) noexcept { kind_ = kind; }
  // Walks the process's memory again for guard pages (the stacks of threads made since); answers how many ranges the
  // table holds.
  std::size_t refresh_stacks() noexcept;
  // The table made again from nothing, less the calling thread's own stack: on T1 just before the task is registered,
  // so that the stack of a thread that has ended since the first scan (the launcher's loading thread), or that ends
  // after it (T1 itself, at Ready), refuses no read of the game's objects placed where it was (WHY§190). The game
  // never points into T1's stack while it lives. Answers how many ranges the table holds.
  std::size_t rescan_stacks() noexcept;
  // An address that faults when read, for the probe's fault injection: a page reserved with no access, never freed.
  [[nodiscard]] std::uint64_t no_access_page() noexcept;
#if EN_PROBE
  // R1's whole-memory search (WHY§227): the region holding `at`, as VirtualQuery describes it (nothing is read there),
  // or nothing past user space.
  [[nodiscard]] std::optional<probe::MemoryRegion> region_at(std::uint64_t at) const noexcept;
#endif

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::uint64_t base_ = 0;
  CopyKind kind_;
  StackRanges stacks_;
};

// The run slot's entry, which the game calls with our task object (sys/trampoline.cpp). Exported for the probe's
// records only: it is called through the fake vtable.
extern "C" std::uint64_t en_task_run(void* self, void* data) noexcept;

}  // namespace en::gal
