// The watch block (watch.hpp): its layout, the creator's part, the crash record's module name, the Marks' ring.
#include <doctest.h>

#include <memory>
#include <new>
#include <ostream>
#include <string>

#include "en/proto/watch.hpp"

using namespace en::proto::watch;

TEST_CASE("watch block: the creator constructs and initialises it, magic last; an opener checks it") {
  alignas(Block) unsigned char page[kBlockSize] = {};
  Block* b = ::new (page) Block();
  CHECK_FALSE(valid(*b));
  initialise(*b, 4321);
  CHECK(valid(*b));
  CHECK(b->header.game_pid == 4321);
  CHECK(b->header.size == sizeof(Block));
  b->header.version = 2;
  CHECK_FALSE(valid(*b));  // another layout: refused, read no further
}

TEST_CASE("watch block: a module's file name is kept as ASCII, anything else '?', cut to fit") {
  Block b{};
  const std::wstring name = L"fake_game-2.exe";
  set_module_name(b.crash, name.c_str(), name.size());
  CHECK(module_name(b.crash) == "fake_game-2.exe");
  const std::wstring odd = L"möd\\x.dll";
  set_module_name(b.crash, odd.c_str(), odd.size());
  CHECK(module_name(b.crash) == "m?d?x.dll");
  const std::wstring longer(200, L'a');
  set_module_name(b.crash, longer.c_str(), longer.size());
  CHECK(module_name(b.crash).size() == kModuleNameBytes - 1);
  set_module_name(b.crash, L"", 0);
  CHECK(module_name(b.crash).empty());
}

TEST_CASE("watch block: Marks numbered from 1 in a ring of the newest 32") {
  Block b{};
  for (std::uint64_t i = 1; i <= 40; ++i) CHECK(add_mark(b.marks, 1000 * i) == i);
  CHECK(b.marks.count.load() == 40);
  CHECK(b.marks.mono_ms[(40 - 1) % kMarkRing].load() == 40000);
  CHECK(b.marks.mono_ms[0].load() == 33000);  // mark 33 took mark 1's place
  CHECK(b.marks.mono_ms[8].load() == 9000);   // mark 9 is still there: no mark 41 yet
}

TEST_CASE("watch block: the Marks' ring changes hands once, the launcher's first, then EldenNet's") {
  SUBCASE("the launcher holds its key: EldenNet asks, the launcher hands over") {
    Block b{};
    CHECK(mark_source(b.marks) == MarkSource::None);
    launcher_holds(b.marks);
    CHECK(mark_source(b.marks) == MarkSource::Launcher);
    CHECK(add_mark(b.marks, 100) == 1);  // the launcher's Mark
    CHECK_FALSE(eldennet_asks(b.marks));  // not yet EldenNet's: the launcher must let its key go
    CHECK(mark_source(b.marks) == MarkSource::Asked);
    CHECK_FALSE(eldennet_asks(b.marks));  // asking again changes nothing
    CHECK(mark_source(b.marks) == MarkSource::Asked);
    launcher_holds(b.marks);  // a late registration does not take it back
    CHECK(mark_source(b.marks) == MarkSource::Asked);
    launcher_hands_over(b.marks);
    CHECK(mark_source(b.marks) == MarkSource::EldenNet);
    CHECK(add_mark(b.marks, 200) == 2);  // EldenNet's, numbered on
    CHECK_FALSE(eldennet_asks(b.marks));
    launcher_holds(b.marks);
    launcher_hands_over(b.marks);
    CHECK(mark_source(b.marks) == MarkSource::EldenNet);
  }
  SUBCASE("nobody holds the key: the ring is EldenNet's at once") {
    Block b{};
    CHECK(eldennet_asks(b.marks));
    CHECK(mark_source(b.marks) == MarkSource::EldenNet);
    launcher_holds(b.marks);  // a launcher that registers later stays out
    CHECK(mark_source(b.marks) == MarkSource::EldenNet);
  }
  SUBCASE("a hand-over nobody asked for changes nothing; an unknown value reads as none") {
    Block b{};
    launcher_holds(b.marks);
    launcher_hands_over(b.marks);
    CHECK(mark_source(b.marks) == MarkSource::Launcher);
    b.marks.source.store(77);
    CHECK(mark_source(b.marks) == MarkSource::None);
  }
  CHECK(std::string(to_string(MarkSource::EldenNet)) == "eldennet");
  CHECK(std::string(to_string(MarkSource::Asked)) == "asked");
}

TEST_CASE("watch block: mono milliseconds from a counter, exact and without overflow") {
  CHECK(mono_ms(0, 10'000'000) == 0);
  CHECK(mono_ms(10'000'000, 10'000'000) == 1000);
  CHECK(mono_ms(15'000, 10'000'000) == 1);
  CHECK(mono_ms(0xFFFF'FFFF'FFFF'FFFFull, 10'000'000) == 1'844'674'407'370'955ull);
  CHECK(mono_ms(5, 0) == 0);
}

TEST_CASE("watch block: names") {
  CHECK(std::string(to_string(InitState::Ready)) == "ready");
  CHECK(std::string(to_string(InitState::WaitEngine)) == "wait_engine");
  // The states of a start that reads on our own thread were appended: the earlier numbers stand.
  CHECK(static_cast<std::uint32_t>(InitState::Exit) == 15);
  CHECK(static_cast<std::uint32_t>(InitState::Reader) == 16);
  CHECK(static_cast<std::uint32_t>(InitState::FirstReading) + 1 == kInitStateCount);
  CHECK(std::string(to_string(InitState::Reader)) == "reader");
  CHECK(std::string(to_string(InitState::FirstReading)) == "first_reading");
  CHECK_FALSE(terminal(InitState::FirstReading));
  CHECK(terminal(InitState::Banner));
  CHECK_FALSE(terminal(InitState::Discover));
  CHECK(std::string(to_string(Spike::S0dCrash)) == "s0d-crash");
  CHECK(std::string(to_string(Spike::S0e)) == "s0e");
  CHECK(std::string(to_string(Spike::S0eAlone)) == "s0e_alone");
  CHECK(std::string(to_string(Spike::S4)) == "s4");
  CHECK(std::string(to_string(static_cast<Spike>(kSpikeCount))) == "unknown");
  // Appended: the words already in use keep their numbers in the block.
  CHECK(static_cast<std::uint32_t>(Spike::S0dCrash) == 5);
  CHECK(static_cast<std::uint32_t>(Spike::S0e) == 6);
  CHECK(static_cast<std::uint32_t>(Spike::S0eAlone) == 7);
  CHECK(static_cast<std::uint32_t>(Spike::S4) == 8);
  for (std::uint32_t i = 0; i < kSpikeCount; ++i) {
    const auto s = static_cast<Spike>(i);
    CHECK(observer_run(s) == (s == Spike::S0e || s == Spike::S0eAlone || s == Spike::S4));
  }
  CHECK(std::string(to_string(ThreadKind::Game)) == "game");
  CHECK(static_cast<std::uint32_t>(ThreadKind::Reader) == 4);  // appended
  CHECK(std::string(to_string(ThreadKind::Reader)) == "reader");
}
