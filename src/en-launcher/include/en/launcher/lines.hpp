#pragma once
// The launcher's console lines off its watch loop (WHY§290). R1 run 9's launcher wrote its console before its log, under
// one lock, from the loop that watches the game; a classic console window whose text is selected (a click inside it,
// QuickEdit) suspends its output, so the loop stopped at its next line, and with it the status lines, the 30 s
// no-confirmation step, the lint and the crash watcher. Now the log is written first, and console lines go through a
// bounded queue to one writer thread: a line that finds the queue full is dropped and counted, never waited on.
// Portable (the Windows writer is out.cpp's), so the queue is tested on Linux with a writer that blocks.
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace en::launcher {

class LineQueue {
 public:
  static constexpr std::size_t kDefaultCapacity = 256;
  explicit LineQueue(std::size_t capacity = kDefaultCapacity) : capacity_(capacity == 0 ? 1 : capacity) {}
  LineQueue(const LineQueue&) = delete;
  LineQueue& operator=(const LineQueue&) = delete;

  // The lines' side: never waits for the writer. False when the line was dropped (the queue full, or closed).
  bool push(std::string line);
  // The writer's side: the next line, waiting for one; nothing once the queue is closed and empty.
  [[nodiscard]] std::optional<std::string> pop();
  // The writer has written the line pop gave it (drain waits for this).
  void written();
  // No more lines: pop answers nothing once the queue is empty.
  void close();
  // Waits at most `ms` until every line queued has been written; answers whether they all were.
  bool drain(std::uint32_t ms);

  [[nodiscard]] std::uint64_t pushed() const;
  [[nodiscard]] std::uint64_t dropped() const;
  [[nodiscard]] std::uint64_t written_count() const;

 private:
  const std::size_t capacity_;
  mutable std::mutex m_;
  std::condition_variable ready_;  // a line queued, or closed
  std::condition_variable done_;   // a line written
  std::deque<std::string> lines_;
  bool closed_ = false;
  std::uint64_t pushed_ = 0, dropped_ = 0, written_ = 0;
};

// The test hook of the launcher's console writer (ELDENNET_TEST_CONSOLE), items joined by commas: `block_after=N` blocks
// the writer for good once it has written N lines, as a console with a selection would; `slow_ms=N` makes it take N ms
// per line (at most 10 s); `queue=N` gives the queue N lines (1 to 256) in place of 256. An item it does not read makes
// the whole hook none.
struct ConsoleHook {
  std::optional<std::uint64_t> block_after;
  std::uint32_t slow_ms = 0;
  std::size_t queue = LineQueue::kDefaultCapacity;
};
[[nodiscard]] ConsoleHook parse_console_hook(std::string_view text);

}  // namespace en::launcher
