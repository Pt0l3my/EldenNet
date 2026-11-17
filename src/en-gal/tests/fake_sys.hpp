#pragma once
// FakeSys (ARCH §6.4, DESIGN §4.7): the tests' SysLayer over a byte-buffer "process" of their own (FakeMemory), so that
// discovery, GuardedReader's rules, the groups and the task's cadence are tested on Linux with no game and no Windows.
// Its addresses are numbers in a map of regions: a copy reads only within one readable region, else it fails as an
// access violation would; a region can be released (a freed object), and stacks can be declared. The clock and the
// thread are the test's to set, and tick() runs the registered task as a game's hub would. Everything is invented.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "en/gal/pe.hpp"
#include "en/gal/sys.hpp"

namespace en::gal::test {

class FakeMemory {
 public:
  // A region of `bytes` at `at`.
  void map(std::uint64_t at, std::vector<std::byte> bytes);
  // A fresh zeroed region of `size` bytes on the heap, 16-aligned, with unmapped room after it.
  std::uint64_t alloc(std::size_t size);
  // The region starting at `at` is no longer readable (a freed object).
  void release(std::uint64_t at);
  [[nodiscard]] bool read(std::uint64_t at, std::span<std::byte> dst) const noexcept;
  bool write(std::uint64_t at, std::span<const std::byte> src);
  template <class T>
  void put(std::uint64_t at, const T& v) {
    static_assert(std::is_trivially_copyable_v<T>);
    write(at, std::as_bytes(std::span<const T, 1>(&v, 1)));
  }
  template <class T>
  T get(std::uint64_t at) const {
    T v{};
    std::array<std::byte, sizeof(T)> b{};
    if (read(at, b)) std::memcpy(&v, b.data(), sizeof(T));
    return v;
  }
  void put_utf16(std::uint64_t at, std::u16string_view text);  // with its NUL
  [[nodiscard]] std::uint64_t reads() const noexcept { return reads_; }

 private:
  struct Region {
    std::vector<std::byte> bytes;
    bool readable = true;
  };
  [[nodiscard]] const Region* region_of(std::uint64_t at, std::uint64_t len, std::uint64_t* base) const noexcept;
  std::map<std::uint64_t, Region> regions_;
  std::uint64_t heap_ = 0x0000'0010'0000'0000ull;
  mutable std::uint64_t reads_ = 0;
};

// The copy primitive over FakeMemory: a read outside one readable region fails with 0xC0000005, its bytes zeroed.
class FakeCopier final : public Copier {
 public:
  explicit FakeCopier(const FakeMemory& mem) : mem_(mem) {}
  [[nodiscard]] CopyOutcome copy(std::uint64_t address, std::span<std::byte> dst) noexcept override;
  std::uint64_t copies = 0;

 private:
  const FakeMemory& mem_;
};

// The mapped image, as the loader would map it at `base`: offsets are RVAs.
class FakeImageSource final : public ByteSource {
 public:
  FakeImageSource(const FakeMemory& mem, std::uint64_t base, std::uint64_t size) : mem_(mem), base_(base), size_(size) {}
  [[nodiscard]] std::uint64_t size() const noexcept override { return size_; }
  [[nodiscard]] bool read(std::uint64_t offset, std::span<std::byte> dst) const noexcept override;

 private:
  const FakeMemory& mem_;
  std::uint64_t base_;
  std::uint64_t size_;
};

// The allocation watch (R6): operator new and delete counted while `armed` on the calling thread (alloc_count.cpp).
struct AllocWatch {
  static thread_local bool armed;
  static std::atomic<std::uint64_t> news;
  static std::atomic<std::uint64_t> deletes;
};

class FakeSys final : public SysLayer {
 public:
  FakeSys(FakeMemory& mem, std::uint64_t base, std::uint64_t image_size);

  // ---- SysLayer ----
  [[nodiscard]] std::uint64_t module_base() const noexcept override { return base_; }
  [[nodiscard]] const ByteSource& image() const noexcept override { return image_; }
  [[nodiscard]] proto::Expected<std::uint64_t, SysError> call_getter(std::uint64_t getter,
                                                                     std::uint64_t class_object) noexcept override;
  [[nodiscard]] proto::Expected<void, SysError> register_task(std::uint64_t function, std::uint64_t instance,
                                                              const TaskShape& shape, TaskBody& body) noexcept override;
  [[nodiscard]] Copier& copier() noexcept override { return copier_; }
  [[nodiscard]] StackRanges& stacks() noexcept override { return stacks_; }
  void note_own_stack() noexcept override { stacks_.add(own_stack); }
  // Every reading advances the clock by clock_step_us (a build that takes time, for the throttle's tests).
  [[nodiscard]] std::uint64_t now_us() const noexcept override {
    drift_ += clock_step_us;
    return now_us_ + drift_;
  }
  [[nodiscard]] std::uint32_t thread_id() const noexcept override { return thread_; }
  [[nodiscard]] std::uint64_t slot_calls(std::uint32_t slot) const noexcept override {
    return slot < calls_.size() ? calls_[slot] : 0;
  }
  [[nodiscard]] std::uint64_t task_object() const noexcept override { return block_; }
  // A stack range the test marks stale (a thread that ended, its memory the game's: WHY§190) is forgotten on asking.
  bool forget_stale_stacks(std::uint64_t address, std::uint64_t len) noexcept override {
    if (!stacks_stale) return false;
    stacks_.remove(AddressRange{address, address + len});
    return !stacks_.overlaps(address, len);
  }
  bool stacks_stale = false;

  // ---- The test's controls ----
  void set_now_us(std::uint64_t t) noexcept { now_us_ = t; }
  void set_thread(std::uint32_t id) noexcept { thread_ = id; }
  // The getter fallback's emulation: the class object's answer (the address of its name), or nothing (a fault).
  std::function<std::optional<std::uint64_t>(std::uint64_t getter, std::uint64_t class_object)> getter;
  struct Registration {
    std::uint64_t function = 0;
    std::uint64_t instance = 0;
    TaskShape shape;
    TaskBody* body = nullptr;
  };
  std::optional<Registration> registration;
  bool fault_register = false;  // the call into the game faults (ARCH §3.2's Register → banner)
  bool registered_after = false;
  // Runs the registered task's run slot once, `dt_us` after the last tick, as the game's hub would; with `watch` the
  // allocation watch is armed for it.
  void tick(std::uint64_t dt_us, bool watch = false);
  std::uint64_t clock_step_us = 0;
  AddressRange own_stack{0x0000'0000'7FF0'0000ull, 0x0000'0000'8000'0000ull};
  std::uint64_t getter_calls = 0;

 private:
  std::uint64_t base_;
  FakeImageSource image_;
  FakeCopier copier_;
  StackRanges stacks_;
  std::uint64_t now_us_ = 1'000'000;
  mutable std::uint64_t drift_ = 0;
  std::uint32_t thread_ = 1;
  std::vector<std::uint64_t> calls_;
  FakeMemory& mem_;
  std::uint64_t block_ = 0;  // our task object, in the fake process, once registered
};

}  // namespace en::gal::test
