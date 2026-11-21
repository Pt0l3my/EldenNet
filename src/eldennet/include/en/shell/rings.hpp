#pragma once
// The DLL's channels (ARCH §1.3), our own and lock-free, over storage made once on T1 and never freed (R14):
// - SpscRing<T, N>: C2 (lifecycle), C4 (commands), C5 (retired watch lists); one producer, one consumer, try only.
// - SnapSlot: C1, two preallocated snapshots, each with an atomic state; the writer takes an Empty slot, else the
//   Ready one the reader has not taken (the newer snapshot then carries gap = true), and the reader takes the newest
//   Ready one, copies it out and frees it. The game thread never waits and never allocates (R6).
// - LogRing: the logger's queue of fixed lines, any thread to T11, never blocking; when full a line is dropped and
//   counted.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "en/proto/snapshot.hpp"
#include "en/shell/logline.hpp"

namespace en::shell {

template <class T, std::size_t N>
class SpscRing {
  static_assert(N >= 2 && (N & (N - 1)) == 0, "a power of two");

 public:
  // The producer's. On false the item is untouched.
  bool try_push(T& item) noexcept {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail - head_.load(std::memory_order_acquire) >= N) return false;
    slots_[tail & (N - 1)] = std::move(item);
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }
  // The consumer's.
  bool try_pop(T& out) noexcept {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    if (head == tail_.load(std::memory_order_acquire)) return false;
    out = std::move(slots_[head & (N - 1)]);
    head_.store(head + 1, std::memory_order_release);
    return true;
  }
  [[nodiscard]] std::size_t size() const noexcept {
    return tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_acquire);
  }

 private:
  std::array<T, N> slots_{};
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
};

class SnapSlot {
 public:
  SnapSlot();
  // The writer's (the game thread): a slot to fill, or null when none is free. Then end_write, with whether it was
  // built (published) or abandoned.
  proto::WorldSnapshot* begin_write() noexcept;
  void end_write(bool publish) noexcept;
  // The reader's (T6): copies the newest published snapshot into `out` and frees its slot; false when none waits.
  bool take(proto::WorldSnapshot& out) noexcept;
  [[nodiscard]] std::uint64_t overwritten() const noexcept { return overwritten_.load(std::memory_order_relaxed); }

 private:
  enum State : std::uint8_t { Empty, Writing, Ready, Reading };
  std::array<std::unique_ptr<proto::WorldSnapshot>, 2> buf_;
  std::array<std::atomic<std::uint8_t>, 2> state_{};
  std::array<std::atomic<std::uint64_t>, 2> order_{};  // publication order, to find the newest
  std::uint64_t published_ = 0;                        // the writer's count
  int writing_ = -1;                                   // the writer's slot
  bool overwrote_ = false;
  std::atomic<std::uint64_t> overwritten_{0};
};

// A line of the log as the ring holds it: its text and the time it was made, in ms since T1's start.
struct LogEntry {
  std::array<char, kLineBytes> text{};
  std::uint16_t len = 0;
  std::uint64_t at_ms = 0;
};

class LogRing {
 public:
  static constexpr std::size_t kSlots = 512;
  LogRing();
  // Any thread: never blocks; false (and counted) when the ring is full.
  bool try_push(std::string_view text, std::uint64_t at_ms) noexcept;
  // T11 only.
  bool try_pop(LogEntry& out) noexcept;
  [[nodiscard]] std::uint64_t dropped() const noexcept { return dropped_.load(std::memory_order_relaxed); }

 private:
  struct Cell {
    std::atomic<std::size_t> seq{0};
    LogEntry entry;
  };
  std::unique_ptr<std::array<Cell, kSlots>> cells_;
  std::atomic<std::size_t> enqueue_{0};
  std::size_t dequeue_ = 0;
  std::atomic<std::uint64_t> dropped_{0};
};

}  // namespace en::shell
