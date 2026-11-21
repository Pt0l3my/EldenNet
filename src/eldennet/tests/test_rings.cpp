// The DLL's channels (rings.hpp): the SPSC ring's order and capacity, C1's slot (the newest wins, an overwrite marks
// gap, the writer always finds room), and the logger's ring from several threads.
#include <doctest.h>

#include <atomic>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

#include "en/shell/names.hpp"
#include "en/shell/rings.hpp"

using namespace en::shell;

TEST_CASE("rings: the SPSC ring keeps order and refuses past its capacity") {
  SpscRing<int, 4> r;
  for (int i = 0; i < 4; ++i) CHECK(r.try_push(i));
  int x = 9;
  CHECK_FALSE(r.try_push(x));
  CHECK(x == 9);
  for (int i = 0; i < 4; ++i) {
    int out = -1;
    REQUIRE(r.try_pop(out));
    CHECK(out == i);
  }
  int out = 0;
  CHECK_FALSE(r.try_pop(out));
}

TEST_CASE("rings: C1's slot gives the newest snapshot, marks an overwrite as a gap, and always has room") {
  SnapSlot slot;
  en::proto::WorldSnapshot out;
  CHECK_FALSE(slot.take(out));
  for (std::uint64_t seq = 1; seq <= 5; ++seq) {
    auto* s = slot.begin_write();
    REQUIRE(s != nullptr);
    s->seq = seq;
    s->gap = false;
    slot.end_write(true);
  }
  REQUIRE(slot.take(out));
  CHECK(out.seq == 5);
  CHECK(out.gap);  // three were overwritten unread
  CHECK(slot.overwritten() == 3);
  CHECK_FALSE(slot.take(out));  // the older one was freed as stale
  auto* s = slot.begin_write();
  REQUIRE(s != nullptr);
  s->seq = 6;
  s->gap = false;
  slot.end_write(true);
  REQUIRE(slot.take(out));
  CHECK(out.seq == 6);
  CHECK_FALSE(out.gap);
  // An abandoned write publishes nothing.
  s = slot.begin_write();
  REQUIRE(s != nullptr);
  s->seq = 7;
  slot.end_write(false);
  CHECK_FALSE(slot.take(out));
}

TEST_CASE("rings: C1's slot under a writer and a reader on two threads") {
  SnapSlot slot;
  std::atomic<bool> stop{false};
  std::thread writer([&] {
    for (std::uint64_t seq = 1; seq <= 20000; ++seq) {
      if (auto* s = slot.begin_write()) {
        s->seq = seq;
        s->mono_ms = seq * 2;
        s->gap = false;
        slot.end_write(true);
      }
    }
    stop = true;
  });
  std::uint64_t last = 0, taken = 0, bad = 0;
  en::proto::WorldSnapshot out;
  // The writer's end is read before each take, so the newest snapshot is counted even when the writer finished first
  // (under load, before the reader took anything): the loop's old condition took that one and dropped it.
  for (;;) {
    const bool done = stop.load();
    if (slot.take(out)) {
      ++taken;
      if (out.seq <= last || out.mono_ms != out.seq * 2) ++bad;
      last = out.seq;
    } else if (done) {
      break;
    }
  }
  writer.join();
  CHECK(bad == 0);
  CHECK(taken > 0);
}

TEST_CASE("rings: the logger's ring from four threads keeps every line it accepts, and counts what it drops") {
  LogRing ring;
  std::vector<std::thread> threads;
  std::atomic<std::uint64_t> accepted{0};
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&, t] {
      for (int i = 0; i < 300; ++i) {
        const std::string text = "t" + std::to_string(t) + " i=" + std::to_string(i);
        if (ring.try_push(text, static_cast<std::uint64_t>(i))) ++accepted;
      }
    });
  }
  for (auto& th : threads) th.join();
  std::uint64_t popped = 0;
  LogEntry e;
  while (ring.try_pop(e)) {
    ++popped;
    CHECK(e.len > 0);
    CHECK(e.text[0] == 't');
  }
  CHECK(popped == accepted.load());
  CHECK(popped + ring.dropped() == 1200);
  CHECK(popped <= LogRing::kSlots);
}

TEST_CASE("names: the safety checks' file names, without case") {
  CHECK(anticheat_module(L"EasyAntiCheat_EOS.dll"));
  CHECK(anticheat_module(L"easyanticheat.sys"));
  CHECK_FALSE(anticheat_module(L"NotEasyAntiCheat.dll"));
  CHECK(protected_launcher(L"Start_Protected_Game.exe"));
  CHECK_FALSE(protected_launcher(L"start_protected_game.exe.bak"));
  CHECK(ersc_module(L"ERSC.dll"));
  CHECK_FALSE(ersc_module(L"ersc.dll2"));
  CHECK(file_name_of(L"C:\\Games\\Game\\eldennet.dll") == L"eldennet.dll");
  CHECK(file_name_of(L"plain.dll") == L"plain.dll");
}
