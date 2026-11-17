#pragma once
// GuardedReader (ARCH §6.5; DESIGN R4): every read of the game's memory, on the game thread, as a copy into storage the
// caller owns, after plausibility checks, within a per-tick budget, never into a known thread stack (WHY§111), through
// the SysLayer's copy primitive (the SEH copy, ReadProcessMemory, or a test's fake). It never forms a pointer or a
// reference into game memory: a copy is bytes, and ObjView reads typed fields out of them by the layout's offsets,
// with bounds checks. Each read takes the game thread's token (R1); nothing allocates.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>

#include "en/gal/layout.hpp"
#include "en/gal/sys.hpp"
#include "en/proto/expected.hpp"
#include "en/proto/snapshot.hpp"
#include "en/proto/thread.hpp"

namespace en::gal {

enum class FaultKind : std::uint8_t {
  Implausible,  // the address failed a plausibility rule: null, below 64 KiB, past user space, unaligned, too long
  Stack,        // the address lies in a known thread stack (WHY§111)
  Copy,         // the copy faulted: `code` is the exception
  Budget,       // the tick's copies or bytes are spent (a soft fault: the snapshot is abandoned, the next has gap)
  WrongThread,  // a read off the thread that ran the first tick (debug and probe builds check every read)
  Injected,     // a fault the probe option injected (EN_PROBE)
};
inline constexpr std::size_t kFaultKinds = 6;
[[nodiscard]] const char* to_string(FaultKind k) noexcept;

struct ReadFault {
  FaultKind kind = FaultKind::Implausible;
  std::uint32_t code = 0;
};

// ARCH §6.5's plausibility, before any copy.
inline constexpr std::uint64_t kLowestAddress = 0x10000;
inline constexpr std::uint64_t kAddressEnd = 0x7FFF'FFFF'0000;  // canonical user mode's end, less a guard
inline constexpr std::uint32_t kMaxCopyBytes = 0x4000;
// Why a copy of `len` bytes at `address` may not be made, or nothing.
[[nodiscard]] std::optional<FaultKind> implausible(std::uint64_t address, std::uint64_t len, std::uint32_t align,
                                                   const StackRanges& stacks) noexcept;

// ARCH §6.5's budget per tick (impl): past either cap the snapshot is abandoned.
struct ReadBudget {
  std::uint32_t copies = 512;
  std::uint32_t bytes = 256u << 10;
};

// Pod: trivially copyable, standard layout; every bit pattern of the types read this way is valid.
template <class T>
concept Pod = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

#if EN_PROBE
// The probe option's fault injection (R1's fault injection, S0-D): every `every`-th read of `group` is made at
// `address` instead, a real address that faults (a no-access page NativeSys reserves), or refused as Injected when
// the address is 0. Never into a stack: the address passes the same plausibility rules.
struct Injection {
  proto::Group group = proto::Group::Session;
  std::uint32_t every = 0;  // 0: off
  std::uint64_t address = 0;
};
#endif

class GuardedReader {
 public:
  GuardedReader(SysLayer& sys, ReadBudget budget = {}) noexcept : sys_(sys), budget_(budget) {}

  // Starts a tick: the budget's counts go back to zero.
  void begin_tick() noexcept;
  // Reads from now on must come from the calling thread (checked at every read in debug and probe builds).
  void bind_thread() noexcept;

  // One guarded copy into caller-owned storage. `align` is the address's required alignment.
  proto::Expected<void, ReadFault> copy_into(const proto::GameThread& gt, proto::Group group, std::uint64_t address,
                                             std::span<std::byte> dst, std::uint32_t align = 1) noexcept;
  template <Pod T>
  proto::Expected<T, ReadFault> copy(const proto::GameThread& gt, proto::Group group, std::uint64_t address) noexcept {
    std::array<std::byte, sizeof(T)> bytes{};
    const auto done = copy_into(gt, group, address, bytes, static_cast<std::uint32_t>(alignof(T)));
    if (!done) return proto::Unexpected{done.error()};
    T out;
    std::memcpy(&out, bytes.data(), sizeof(T));
    return out;
  }

  [[nodiscard]] bool spent() const noexcept { return spent_; }
  [[nodiscard]] std::uint32_t copies() const noexcept { return copies_; }
  [[nodiscard]] std::uint32_t bytes() const noexcept { return bytes_; }
  // Faults by kind since the start (C13): written here, read by any thread.
  [[nodiscard]] std::uint64_t faults(FaultKind k) const noexcept {
    return faults_[static_cast<std::size_t>(k)].load(std::memory_order_relaxed);
  }
  [[nodiscard]] std::uint64_t total_copies() const noexcept { return total_copies_.load(std::memory_order_relaxed); }
  // Stack ranges found stale and forgotten (a thread that ended; WHY§190).
  [[nodiscard]] std::uint64_t stale_stacks() const noexcept { return stale_stacks_.load(std::memory_order_relaxed); }

#if EN_PROBE
  void inject(const Injection& injection) noexcept { injection_ = injection; }
  [[nodiscard]] const Injection& injection() const noexcept { return injection_; }
#endif

 private:
  ReadFault fail(FaultKind kind, std::uint32_t code = 0) noexcept;

  SysLayer& sys_;
  ReadBudget budget_;
  std::uint32_t copies_ = 0;
  std::uint32_t bytes_ = 0;
  bool spent_ = false;
  std::uint32_t thread_ = 0;
  bool bound_ = false;
  std::array<std::atomic<std::uint64_t>, kFaultKinds> faults_{};
  std::atomic<std::uint64_t> total_copies_{0};
  std::atomic<std::uint64_t> stale_stacks_{0};
#if EN_PROBE
  Injection injection_;
  std::array<std::uint32_t, proto::kGroupCount> reads_{};
#endif
};

// Typed reads from the bytes of one copied object, by a field's layout (ARCH §6.5's ObjView): bounds-checked, widened,
// and nothing when the field is absent (a null layout pointer) or does not fit the bytes.
class ObjView {
 public:
  ObjView() = default;
  explicit ObjView(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

  [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return bytes_; }
  // A whole number of any width (u8 to u64, i32 sign-extended, bool8, handle), or a pointer, widened.
  [[nodiscard]] std::optional<std::int64_t> integer(const FieldLayout* f) const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> u64(const FieldLayout* f) const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> ptr(const FieldLayout* f) const noexcept { return u64(f); }
  [[nodiscard]] std::optional<bool> bit(const FieldLayout* f) const noexcept;
  // The `i`-th float of a f32, vec3f or mat4f field.
  [[nodiscard]] std::optional<float> f32(const FieldLayout* f, std::size_t i = 0) const noexcept;
  // A utf16[n] field up to its first NUL, into `out`; its length in units, or nothing when it has no NUL.
  [[nodiscard]] std::optional<std::size_t> utf16(const FieldLayout* f, std::span<char16_t> out) const noexcept;
  // A DL vector's two pointers, a DL map's pointer and count.
  [[nodiscard]] std::optional<std::uint64_t> second(const FieldLayout* f) const noexcept;
  // Whether an integer lies within the field's range (true when it has none).
  [[nodiscard]] static bool in_range(const FieldLayout* f, std::int64_t v) noexcept;

 private:
  [[nodiscard]] std::optional<std::uint64_t> raw(std::uint32_t off, std::uint32_t size) const noexcept;
  std::span<const std::byte> bytes_;
};

}  // namespace en::gal
