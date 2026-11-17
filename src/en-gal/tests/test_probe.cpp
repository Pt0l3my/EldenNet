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

// ---- R1's searches (the in-game round of O89): before any discovery, from the image and the game thread ----

TEST_CASE("probe: the heap slots and the slots holding the module's base, read from the image on T1") {
  FakeWorld w;
  const Discovery d = discovered(w);
  const probe::HeapSlots heap = probe::heap_slots(*w.sys, d.image());
  CHECK(heap.slots.size() >= 14);
  CHECK(heap.slots.size() == heap.values.size());
  CHECK(std::find(heap.slots.begin(), heap.slots.end(), w.at("st_save_data") - w.base) != heap.slots.end());
  const probe::Holding hinstance = probe::slots_holding(*w.sys, d.image(), w.base);
  REQUIRE(hinstance.count == 1);
  CHECK(hinstance.slots == std::vector<std::uint32_t>{static_cast<std::uint32_t>(w.at("st_module_handle") - w.base)});
  CHECK(probe::heap_slots(*w.sys, d.image(), 3).slots.size() == 3);
}

TEST_CASE("probe: a SlotSearch over a vtable set of the caller's finds the same slot as discovery's") {
  FakeWorld w;
  const Discovery d = discovered(w);
  probe::SlotSearch search(*w.sys, d.image(), d.vtables(roles::Cls::GameState));
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  for (int tick = 0; tick < 100 && !search.step(gt, reader, 8); ++tick) reader.begin_tick();
  REQUIRE(search.found().size() == 1);
  CHECK(search.found()[0] == w.at("st_game_state") - w.base);
}

TEST_CASE("probe: the rising-counter search finds the play time by the clock, and tells it from a still field") {
  FakeWorld w;
  const Discovery d = discovered(w);
  probe::RisingSearch search(probe::heap_slots(*w.sys, d.image()), 0x40);
  REQUIRE(search.candidates() >= 14);
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t now = 10'000'000;
  const auto pass = [&] {
    for (int tick = 0; tick < 1000; ++tick) {
      reader.begin_tick();
      if (search.step(gt, reader, 7, now)) return;
    }
    FAIL("a pass did not end");
  };
  pass();                     // the first pass: nothing to compare with
  CHECK(search.hits().empty());
  now += 2'000'000;           // two seconds, and the play time rises by them
  w.advance_play(2000);
  pass();
  now += 3'000'000;           // three seconds at a menu: the play time stands still
  pass();
  now += 1'000'000;
  w.advance_play(1000);
  pass();
  CHECK(search.passes() == 4);
  const std::uint32_t slot = static_cast<std::uint32_t>(w.at("st_save_data") - w.base);
  const auto hits = search.hits();
  const auto it = std::find_if(hits.begin(), hits.end(), [&](const auto& h) { return h.slot == slot; });
  REQUIRE(it != hits.end());
  CHECK(it->offset == 0x10);
  CHECK(it->rose == 2);
  CHECK(it->still == 1);
  CHECK(it->other == 0);
}

TEST_CASE("probe: the hub census finds the objects the hub points at by their vtables, to a depth of three") {
  FakeWorld w;
  const Discovery d = discovered(w);
  // A hub of the test's own: its instance points at a group array, whose entries point at two lists of tasks; one task
  // is also pointed at directly (counted once per pointer path, queued once), and one entry is a released object.
  const std::uint64_t hub_static = w.at("st_task_hub");
  const std::uint64_t hub = w.mem.alloc(0x400);
  const std::uint64_t groups = w.mem.alloc(0x40);
  const std::uint64_t list_a = w.mem.alloc(0x20);
  const std::uint64_t list_b = w.mem.alloc(0x20);
  const std::uint64_t vt_run = w.vtable("chr");
  const std::uint64_t vt_other = w.vtable("prop");
  std::vector<std::uint64_t> tasks;
  for (int i = 0; i < 4; ++i) {
    const std::uint64_t t = w.mem.alloc(0x30);
    w.mem.put<std::uint64_t>(t, i < 3 ? vt_run : vt_other);
    tasks.push_back(t);
  }
  const std::uint64_t gone = w.mem.alloc(0x30);
  w.mem.release(gone);
  w.mem.put<std::uint64_t>(hub_static, hub);
  w.mem.put<std::uint64_t>(hub + 0x18, groups);
  w.mem.put<std::uint64_t>(groups + 0x00, list_a);
  w.mem.put<std::uint64_t>(groups + 0x08, list_b);
  w.mem.put<std::uint64_t>(groups + 0x10, gone);
  w.mem.put<std::uint64_t>(list_a + 0x00, tasks[0]);
  w.mem.put<std::uint64_t>(list_a + 0x08, tasks[1]);
  w.mem.put<std::uint64_t>(list_b + 0x00, tasks[2]);
  w.mem.put<std::uint64_t>(list_b + 0x08, tasks[3]);
  probe::Census census({vt_run, vt_other, w.vtable("tomb")}, hub_static, w.base, d.image().size_of_image);
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  bool done = false;
  for (int tick = 0; tick < 200 && !done; ++tick) {
    reader.begin_tick();
    done = census.step(gt, reader, 5);
  }
  REQUIRE(done);
  CHECK(census.faults() >= 1);  // the released object
  const auto vts = census.vtables();
  const auto counts = census.counts();
  REQUIRE(vts.size() == 2);
  for (std::size_t i = 0; i < vts.size(); ++i) CHECK(counts[i] == (vts[i] == vt_run ? 3u : 1u));
  // A static that holds nothing: done at once, with a fault.
  w.mem.put<std::uint64_t>(hub_static, 0);
  probe::Census none({vt_run}, hub_static, w.base, d.image().size_of_image);
  reader.begin_tick();
  CHECK(none.step(gt, reader, 5));
  CHECK(none.vtables().empty());
}
