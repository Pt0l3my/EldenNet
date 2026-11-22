// The probe option (probe.hpp; EN_PROBE): the static-slot search on the game thread, the searches by invariant over a
// copied object, the slot counts, the rising-counter search and the object scan. The world is invented.
#include <doctest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <vector>

#include "en/gal/probe.hpp"
#include "en/gal/report.hpp"
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

// ---- The round's searches: before any discovery, from the image and the game thread ----

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

TEST_CASE("probe: the heap slots with a question of what is live keep the live ones and count the rest") {
  FakeWorld w;
  const Discovery d = discovered(w);
  const probe::HeapSlots all = probe::heap_slots(*w.sys, d.image());
  CHECK(all.pointer_like == all.slots.size());
  CHECK(all.not_committed == 0);
  // The save's object is said to be gone, the game state's to be behind a guard: neither is a candidate.
  const std::uint64_t save = w.instance("save_data"), game = w.instance("game_state");
  std::uint64_t asked = 0;
  const probe::LiveFn live = [&](std::uint64_t v) {
    ++asked;
    return v == save ? probe::Liveness::NotCommitted : v == game ? probe::Liveness::NoAccess : probe::Liveness::Live;
  };
  const probe::HeapSlots some = probe::heap_slots(*w.sys, d.image(), 1u << 16, &live);
  CHECK(asked == all.slots.size());
  CHECK(some.pointer_like == all.slots.size());
  CHECK(some.not_committed == 1);
  CHECK(some.no_access == 1);
  CHECK(some.slots.size() == all.slots.size() - 2);
  CHECK(std::find(some.values.begin(), some.values.end(), save) == some.values.end());
  CHECK(std::find(some.values.begin(), some.values.end(), game) == some.values.end());
  // `max` counts what is kept.
  const probe::HeapSlots three = probe::heap_slots(*w.sys, d.image(), 3, &live);
  CHECK(three.slots.size() == 3);
}

TEST_CASE("probe: two slots that hold one object are found as two slots with one object") {
  FakeWorld w;
  const Discovery d = discovered(w);
  // The save's slot holds the game state's object too: a copy of one address.
  const std::uint64_t game = w.instance("game_state");
  w.mem.put<std::uint64_t>(w.at("st_save_data"), game);
  probe::SlotSearch search(probe::heap_slots(*w.sys, d.image()), d.vtables(roles::Cls::GameState));
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  for (int tick = 0; tick < 100 && !search.step(gt, reader, 8); ++tick) reader.begin_tick();
  CHECK(search.checked() == search.candidates());
  REQUIRE(search.found().size() == 2);
  CHECK(search.found_total() == 2);
  REQUIRE(search.found_objects().size() == 2);
  CHECK(search.found_objects()[0] == game);
  CHECK(search.found_objects()[1] == game);
}

TEST_CASE("probe: the rising-counter search keeps the fields that rose most, not the first it met") {
  FakeWorld w;
  // Forty objects whose field rises once and never again, then one whose field rises at every pass: with room for
  // 32 hits, the one that keeps rising must be among them, and first.
  probe::HeapSlots candidates;
  std::vector<std::uint64_t> objects;
  for (std::uint32_t i = 0; i < 41; ++i) {
    objects.push_back(w.mem.alloc(0x40));
    candidates.slots.push_back(0x1000 + 8 * i);
    candidates.values.push_back(objects.back());
    w.mem.put<std::uint32_t>(objects.back() + 0x20, 5000);
  }
  probe::RisingSearch search(std::move(candidates), 0x40);
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  std::uint64_t now = 10'000'000;
  const auto pass = [&] {
    for (int tick = 0; tick < 1000; ++tick) {
      reader.begin_tick();
      if (search.step(gt, reader, 16, now)) return;
    }
    FAIL("a pass did not end");
  };
  pass();
  CHECK(search.risen() == 0);
  for (int p = 0; p < 6; ++p) {
    now += 1'000'000;
    for (std::uint32_t i = 0; i < 41; ++i) {
      const bool rises = i == 40 || p == 0;
      if (rises) w.mem.put<std::uint32_t>(objects[i] + 0x20, w.mem.get<std::uint32_t>(objects[i] + 0x20) + 1000);
    }
    pass();
  }
  CHECK(search.passes() == 7);
  CHECK(search.risen() == 41);
  const auto hits = search.hits();
  REQUIRE(hits.size() == probe::RisingSearch::kMaxHits);
  CHECK(hits[0].slot == 0x1000 + 8 * 40);
  CHECK(hits[0].offset == 0x20);
  CHECK(hits[0].rose == 6);
  CHECK(hits[0].still == 0);
  CHECK(hits[1].rose == 1);
  CHECK(hits[1].still == 5);
  CHECK(hits[1].slot == 0x1000);  // among equals, the first met
  CHECK(std::is_sorted(hits.begin(), hits.end(), [](const auto& a, const auto& b) { return a.rose > b.rose; }));
}

TEST_CASE("probe: the object scan finds an object nothing known points at, and what holds it, level by level") {
  FakeWorld w;
  const Discovery d = discovered(w);
  // The game state's object, which no slot of the data holds any more. A prop holds its address at 0x28; the field
  // area's object (which a static holds) holds the prop's address at 0x18. Elsewhere, an object of the save's class
  // lies inside a larger object, at its 0x40, and nothing points at it.
  const std::uint64_t game = w.instance("game_state");
  w.mem.put<std::uint64_t>(w.at("st_game_state"), 0);
  const std::uint64_t prop = w.mem.alloc(0x60);
  w.set_vtable(prop, "prop");
  w.mem.put<std::uint64_t>(prop + 0x28, game);
  const std::uint64_t field = w.instance("field_area");
  w.mem.put<std::uint64_t>(field + 0x18, prop);
  const std::uint64_t big = w.mem.alloc(0x80);
  w.set_vtable(big, "tomb");
  w.mem.put<std::uint64_t>(big + 0x40, w.vtable("save"));
  const std::uint64_t save = w.instance("save_data");
  w.mem.put<std::uint64_t>(w.at("st_save_data"), 0);
  w.mem.put<std::uint64_t>(save, 0);  // the old save object is gone: only the one inside `big` is of its class
  // The memory as the scan is told of it: each object a private, writable region of its own; one region that is the
  // caller's (never read, though it holds the game state's table of methods); one that may not be read.
  const std::uint64_t own = w.mem.alloc(0x40);
  w.mem.put<std::uint64_t>(own + 0x10, w.vtable("game"));
  const std::uint64_t guarded = w.mem.alloc(0x40);
  w.mem.put<std::uint64_t>(guarded, w.vtable("game"));
  struct Piece {
    std::uint64_t at;
    std::uint64_t size;
    std::uint32_t protect;
  };
  std::vector<Piece> pieces = {{game, 0x20, probe::kPageReadWrite},    {prop, 0x60, probe::kPageReadWrite},
                               {field, 0x20, probe::kPageReadWrite},   {big, 0x80, probe::kPageReadWrite},
                               {own, 0x40, probe::kPageReadWrite},
                               {guarded, 0x40, probe::kPageReadWrite | probe::kPageGuard}};
  std::sort(pieces.begin(), pieces.end(), [](const Piece& a, const Piece& b) { return a.at < b.at; });
  std::uint64_t asked = 0;
  const probe::RegionQuery regions = [&](std::uint64_t at) -> std::optional<probe::MemoryRegion> {
    ++asked;
    for (const Piece& p : pieces) {
      if (at < p.at + p.size) {
        probe::MemoryRegion r;
        r.base = p.at;
        r.size = p.size;
        r.allocation_base = p.at;
        r.protect = p.protect;
        r.committed = true;
        r.kind = probe::RegionKind::Private;
        return r;
      }
    }
    return std::nullopt;
  };
  const auto rva = [&](std::uint64_t va) { return static_cast<std::uint32_t>(va - w.base); };
  std::vector<std::uint32_t> primary;
  for (const char* cls : {"game", "prop", "field", "tomb", "save", "hud", "lobby"}) primary.push_back(rva(w.vtable(cls)));
  probe::ObjectScan scan(regions, {{rva(w.vtable("game")), 7}, {rva(w.vtable("save")), 9}}, primary, w.base,
                         d.image().size_of_image, {own});
  GuardedReader reader(*w.sys);
  const auto gt = en::proto::GameThread::issue();
  int steps = 0;
  for (; steps < 1000 && !scan.done(); ++steps) {
    reader.begin_tick();
    (void)scan.step(gt, reader, 2);  // two pieces a step: a sweep takes several
  }
  REQUIRE(scan.done());
  CHECK(steps > 4);
  CHECK(scan.sweeps() == 4);  // the objects, then three levels of holders
  CHECK(scan.unreadable() == 0);
  CHECK(scan.bytes() == 4 * (0x20 + 0x60 + 0x20 + 0x80));
  CHECK(scan.regions() == 4 * 4);
  const auto nodes = scan.nodes();
  const auto node = [&](std::uint8_t level, std::uint64_t address) -> const probe::ObjectScan::Node* {
    const auto it = std::find_if(nodes.begin(), nodes.end(),
                                 [&](const auto& n) { return n.level == level && n.address == address; });
    return it != nodes.end() ? &*it : nullptr;
  };
  REQUIRE(nodes.size() == 5);
  // Level 0: the game state's object, and the save's that lies inside the big one. Neither the caller's own copy nor
  // the guarded one is found.
  const auto* found_game = node(0, game);
  const auto* found_save = node(0, big + 0x40);
  REQUIRE(found_game != nullptr);
  REQUIRE(found_save != nullptr);
  CHECK(found_game->lead == 7);
  CHECK(found_game->vtable == rva(w.vtable("game")));
  CHECK(found_game->holders == 1);
  CHECK(found_save->lead == 9);
  CHECK(found_save->holders == 0);
  // Level 1: the prop holds the game state's address at 0x28; the big object encloses the save's at 0x40.
  const auto* at_prop = node(1, prop);
  REQUIRE(at_prop != nullptr);
  CHECK(at_prop->vtable == rva(w.vtable("prop")));
  CHECK(at_prop->offset == 0x28);
  CHECK_FALSE(at_prop->embedded);
  CHECK(nodes[at_prop->parent].address == game);
  CHECK(at_prop->lead == 7);
  CHECK(at_prop->holders == 1);
  const auto* at_big = node(1, big);
  REQUIRE(at_big != nullptr);
  CHECK(at_big->embedded);
  CHECK(at_big->offset == 0x40);
  CHECK(at_big->vtable == rva(w.vtable("tomb")));
  CHECK(nodes[at_big->parent].address == big + 0x40);
  // Level 2: the field area's object holds the prop's address at 0x18. Its own address is in the image's data, where
  // the caller ends the chain: the scan itself finds no holder of it.
  const auto* at_field = node(2, field);
  REQUIRE(at_field != nullptr);
  CHECK(at_field->offset == 0x18);
  CHECK(nodes[at_field->parent].address == prop);
  CHECK(at_field->holders == 0);
  const probe::Holding held = probe::slots_holding(*w.sys, d.image(), at_field->address);
  REQUIRE(held.count == 1);
  CHECK(held.slots[0] == w.at("st_field_area") - w.base);
  CHECK(asked > 0);

  // Nothing of the class anywhere: one sweep, and it is over. Nothing to look for: over at once.
  probe::ObjectScan none(regions, {{rva(w.vtable("hud")), 1}}, primary, w.base, d.image().size_of_image, {own});
  for (int k = 0; k < 1000 && !none.done(); ++k) {
    reader.begin_tick();
    (void)none.step(gt, reader, 8);
  }
  CHECK(none.done());
  CHECK(none.sweeps() == 1);
  CHECK(none.nodes().empty());
  probe::ObjectScan idle(regions, {}, primary, w.base, d.image().size_of_image, {});
  CHECK(idle.done());
  CHECK(idle.step(gt, reader, 8));
}

TEST_CASE("probe: a region is searched only when committed, readable and of ordinary memory") {
  probe::MemoryRegion r{0x10000, 0x1000, 0x10000, probe::kPageReadWrite, true, probe::RegionKind::Private};
  CHECK(probe::searchable(r));
  for (const std::uint32_t p : {probe::kPageReadOnly, probe::kPageWriteCopy, probe::kPageExecuteRead,
                                probe::kPageExecuteReadWrite, probe::kPageExecuteWriteCopy}) {
    r.protect = p;
    CHECK(probe::searchable(r));
  }
  for (const std::uint32_t p : {probe::kPageNoAccess, probe::kPageExecute, probe::kPageReadWrite | probe::kPageGuard,
                                probe::kPageReadWrite | probe::kPageNoCache, probe::kPageReadWrite | probe::kPageWriteCombine,
                                0u}) {
    r.protect = p;
    CHECK_FALSE(probe::searchable(r));
  }
  r.protect = probe::kPageReadWrite;
  r.committed = false;
  CHECK_FALSE(probe::searchable(r));
}
