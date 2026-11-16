// The probe option (probe.hpp; EN_PROBE): the static-slot search on the game thread, the searches by invariant over a
// copied object, and the slot counts. The world is invented.
#include <doctest.h>

#include "en/gal/probe.hpp"
#include "rig_gal.hpp"

using namespace en::gal;
using namespace en::gal::test;

TEST_CASE("probe: a static's slot is the data slot whose instance has its class's vtable") {
  FakeWorld w;
  const Discovery d = discovered(w);
  for (const auto& [cls, item] : {std::pair{roles::Cls::SaveData, "save_data"}, std::pair{roles::Cls::GameState, "game_state"},
                                  std::pair{roles::Cls::FieldArea, "field_area"}}) {
    INFO(item);
    probe::SlotSearch search(*w.sys, d, cls);
    CHECK(search.candidates() >= 14);  // every static whose instance is on the heap
    GuardedReader reader(*w.sys);
    const auto gt = en::proto::GameThread::issue();
    bool done = false;
    for (int tick = 0; tick < 100 && !done; ++tick) {
      reader.begin_tick();
      done = search.step(gt, reader, 4);
    }
    CHECK(done);
    REQUIRE(search.found().size() == 1);
    CHECK(search.found()[0] == w.at(std::string("st_") + item) - w.base);
  }
}

TEST_CASE("probe: searches by invariant answer offsets only") {
  FakeWorld w;
  const Discovery d = discovered(w);
  std::vector<std::byte> obj(0x98);
  REQUIRE(w.mem.read(w.player, obj));
  CHECK(probe::vtable_offsets(obj, d.vtables(roles::Cls::Character)) == std::vector<std::uint32_t>{0});
  std::vector<std::byte> vitals(0x20);
  REQUIRE(w.mem.read(w.mem.get<std::uint64_t>(w.mem.get<std::uint64_t>(w.player + 0x30)), vitals));
  // (hp, max_hp) = (1100, 1200) at 0, and (fp, max_fp) = (70, 80) at 8: current first.
  const auto pairs = probe::meter_offsets(vitals, false);
  CHECK(std::find(pairs.begin(), pairs.end(), 0u) != pairs.end());
  CHECK(std::find(pairs.begin(), pairs.end(), 8u) != pairs.end());
  std::vector<std::byte> sheet(0x80);
  REQUIRE(w.mem.read(w.mem.get<std::uint64_t>(w.instance("save_data") + 0x20), sheet));
  CHECK(probe::utf16_offsets(sheet, u"Tess Wayfarer") == std::vector<std::uint32_t>{0x20});
  CHECK(probe::utf16_offsets(sheet, u"Tess").empty());  // the NUL must follow
  std::vector<std::byte> marks(0x30);
  REQUIRE(w.mem.read(w.instance("event_flags"), marks));
  CHECK(probe::u32_offsets(marks, 1000) == std::vector<std::uint32_t>{8});
}

TEST_CASE("probe: the game's calls of each slot of our fake vtable are counted") {
  Running r;
  r.start();
  for (int i = 0; i < 10; ++i) r.w.sys->tick(16'667);
  CHECK(r.w.sys->slot_calls(1) == r.task.status().invocations.load());
  CHECK(r.w.sys->slot_calls(0) == 0);
  CHECK(r.w.sys->slot_calls(2) == 0);
  CHECK(r.w.sys->slot_calls(64) == 0);
}
