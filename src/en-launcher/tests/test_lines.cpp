// The console's lines off the watch loop (WHY§290): a writer that blocks or crawls never holds up the lines' side.
#include <doctest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "en/launcher/lines.hpp"

using namespace en::launcher;
using Clock = std::chrono::steady_clock;

TEST_CASE("the console's queue: lines in order, a full queue drops and counts, closed it ends the writer") {
  LineQueue q(4);
  for (int i = 0; i < 6; ++i) CHECK(q.push("line " + std::to_string(i)) == (i < 4));
  CHECK(q.pushed() == 4);
  CHECK(q.dropped() == 2);
  for (int i = 0; i < 4; ++i) {
    const auto line = q.pop();
    REQUIRE(line.has_value());
    CHECK(*line == "line " + std::to_string(i));
    q.written();
  }
  CHECK(q.drain(0));
  CHECK(q.written_count() == 4);
  CHECK(q.push("after"));
  q.close();
  CHECK_FALSE(q.push("closed"));  // dropped, counted
  CHECK(q.dropped() == 3);
  CHECK(q.pop() == std::optional<std::string>("after"));  // what was queued is still written
  CHECK_FALSE(q.pop().has_value());
}

TEST_CASE("a console that blocks never stops the lines' side, nor the drain past its time") {
  LineQueue q(8);
  std::promise<void> unblock;
  std::shared_future<void> go = unblock.get_future().share();
  std::atomic<int> taken{0};
  // The writer takes one line and blocks, as a console with a selection does.
  std::thread writer([&] {
    while (const auto line = q.pop()) {
      ++taken;
      go.wait();
      q.written();
    }
  });
  const auto t0 = Clock::now();
  int kept = 0;
  for (int i = 0; i < 5000; ++i) kept += q.push("status " + std::to_string(i)) ? 1 : 0;
  const auto took = Clock::now() - t0;
  CHECK(took < std::chrono::seconds(2));  // never waited on the writer
  CHECK(kept >= 8);
  CHECK(kept <= 9);  // the queue's eight, and the one the writer took
  CHECK(q.dropped() == static_cast<std::uint64_t>(5000 - kept));
  const auto d0 = Clock::now();
  CHECK_FALSE(q.drain(200));  // the console never took them: given up
  CHECK(Clock::now() - d0 < std::chrono::seconds(2));
  unblock.set_value();
  q.close();
  writer.join();
  CHECK(q.written_count() == static_cast<std::uint64_t>(kept));
}

TEST_CASE("a slow console takes every line in order while the lines' side goes on") {
  LineQueue q(64);
  std::vector<std::string> got;
  std::thread writer([&] {
    while (const auto line = q.pop()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      got.push_back(*line);
      q.written();
    }
  });
  for (int i = 0; i < 40; ++i) REQUIRE(q.push(std::to_string(i)));
  CHECK(q.drain(10000));
  q.close();
  writer.join();
  REQUIRE(got.size() == 40);
  for (int i = 0; i < 40; ++i) CHECK(got[static_cast<std::size_t>(i)] == std::to_string(i));
  CHECK(q.dropped() == 0);
}

TEST_CASE("the console writer's test hook: block after N lines, slow by N ms, a queue of N, else none") {
  CHECK(parse_console_hook("block_after=12").block_after == std::optional<std::uint64_t>(12));
  CHECK(parse_console_hook("block_after=12").slow_ms == 0);
  CHECK(parse_console_hook("block_after=12").queue == LineQueue::kDefaultCapacity);
  CHECK(parse_console_hook("slow_ms=40").slow_ms == 40);
  CHECK_FALSE(parse_console_hook("slow_ms=40").block_after.has_value());
  const ConsoleHook both = parse_console_hook("block_after=5,queue=8");
  CHECK(both.block_after == std::optional<std::uint64_t>(5));
  CHECK(both.queue == 8);
  for (const char* none : {"", "block_after=", "block_after=x", "slow_ms=99999", "other=1", "block_after=3 ",
                           "queue=0", "queue=257", "block_after=5,queue=8,x"}) {
    const ConsoleHook h = parse_console_hook(none);
    CHECK_FALSE(h.block_after.has_value());
    CHECK(h.slow_ms == 0);
    CHECK(h.queue == LineQueue::kDefaultCapacity);
  }
}
