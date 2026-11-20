// The probe option (probe.hpp; EN_PROBE): the static-slot search on the game thread, the searches by invariant over a
// copied object, the slot counts, and spike.6's callee summary and whole-memory search. The world and the code are
// invented.
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

// ---- Spike.6 (WHY§227): the callee's summary on code of our own, and the whole-memory search over FakeSys ----

namespace {

std::vector<std::byte> code_of(std::initializer_list<int> bytes) {
  std::vector<std::byte> out;
  for (const int b : bytes) out.push_back(std::byte{static_cast<std::uint8_t>(b)});
  return out;
}

const probe::ArgUse& arg(const probe::CalleeCode& c, std::uint8_t reg) {
  for (const probe::ArgUse& a : c.args) {
    if (a.reg == reg) return a;
  }
  FAIL("no argument " << int{reg});
  return c.args.front();
}

}  // namespace

TEST_CASE("probe: the callee's summary follows its arguments to their stores, saves and compares before the first call") {
  // A registration of our own ABI (task r8, instance rcx, group edx), as a compiler might lay it out: the task kept in
  // rbx and stored into the instance, the group compared with a bound, a virtual call on the task, a direct call, and
  // a refusal path that returns through no call.
  const std::vector<std::byte> code = code_of({
      0x48, 0x89, 0x5C, 0x24, 0x08,  //  0: mov [rsp+8], rbx      (not an argument)
      0x57,                          //  5: push rdi
      0x48, 0x83, 0xEC, 0x20,        //  6: sub rsp, 0x20
      0x49, 0x8B, 0xD8,              // 10: mov rbx, r8           (the task, callee-saved)
      0x8B, 0xFA,                    // 13: mov edi, edx          (the group, callee-saved)
      0x83, 0xFA, 0x40,              // 15: cmp edx, 0x40
      0x73, 0x1D,                    // 18: jae 49
      0x48, 0x85, 0xDB,              // 20: test rbx, rbx
      0x74, 0x18,                    // 23: je 49
      0x48, 0x89, 0x59, 0x18,        // 25: mov [rcx+0x18], rbx   (the task, into the instance)
      0x48, 0x8B, 0x03,              // 29: mov rax, [rbx]        (the task's vtable)
      0x48, 0x8B, 0xCB,              // 32: mov rcx, rbx
      0xFF, 0x50, 0x10,              // 35: call [rax+0x10]       (a virtual call on the task)
      0xE8, 0x00, 0x00, 0x00, 0x00,  // 38: call 43
      0x48, 0x83, 0xC4, 0x20,        // 43: add rsp, 0x20
      0x5F,                          // 47: pop rdi
      0xC3,                          // 48: ret
      0x32, 0xC0,                    // 49: xor al, al
      0x48, 0x83, 0xC4, 0x20,        // 51: add rsp, 0x20
      0x5F,                          // 55: pop rdi
      0xC3,                          // 56: ret
  });
  const std::uint32_t rva = 0x123400;
  const probe::CalleeCode c = probe::summarise_callee(code, rva, static_cast<std::uint32_t>(code.size()));
  CHECK(c.read);
  CHECK(c.bytes == code.size());
  CHECK(c.instructions == 21);
  CHECK_FALSE(c.cut);
  CHECK_FALSE(c.undecodable);
  CHECK(c.first_call == 35u);
  CHECK(c.first_ret == 48u);
  CHECK(c.returns_early);  // the refusal path
  CHECK_FALSE(c.modelled_to.has_value());
  REQUIRE(c.call_count == 2);
  REQUIRE(c.calls.size() == 2);
  CHECK(c.calls[0].form == probe::CallForm::Memory);
  CHECK(c.calls[0].base == std::optional<std::uint8_t>(0));
  CHECK(c.calls[0].disp == 0x10);
  CHECK(c.calls[0].vtable_of == std::optional<std::uint8_t>(8));
  CHECK(c.calls[1].form == probe::CallForm::Direct);
  CHECK(c.calls[1].target == std::optional<std::uint32_t>(rva + 43));
  REQUIRE(c.branch_count == 2);
  CHECK(c.branches[0].offset == 18);
  CHECK(c.branches[0].to == 49);
  CHECK(c.branches[1].offset == 23);
  CHECK(c.branches[1].to == 49);
  const probe::ArgUse& task = arg(c, 8);
  CHECK(task.saved == std::vector<std::uint8_t>{3});
  REQUIRE(task.stores.size() == 1);
  CHECK(task.stores[0].offset == 25);
  CHECK(task.stores[0].via == 3);
  CHECK(task.stores[0].base == std::optional<std::uint8_t>(1));
  CHECK(task.stores[0].base_arg == std::optional<std::uint8_t>(1));  // rcx: the instance itself
  CHECK(task.stores[0].disp == 0x18);
  CHECK(task.stores[0].width == 8);
  CHECK(task.tested == 1);
  const probe::ArgUse& group = arg(c, 2);
  CHECK(group.saved == std::vector<std::uint8_t>{7});
  REQUIRE(group.compares.size() == 1);
  CHECK(group.compares[0].offset == 15);
  CHECK(group.compares[0].imm == std::optional<std::uint64_t>(0x40));
  CHECK(arg(c, 1).stores.empty());  // [rsp+8] took rbx before anything held an argument
  CHECK(arg(c, 9).saved.empty());
  // The report holds it and lints clean.
  probe::Findings f;
  f.callee_code = c;
  const auto text = probe::write_report(f);
  REQUIRE(text.has_value());
  const std::array<const report::Vocabulary*, 1> known = {&probe::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
}

TEST_CASE("probe: the callee's summary stops where it models nothing, at its bound, and at its first return") {
  SUBCASE("an instruction it does not model ends the tracking") {
    const std::vector<std::byte> code = code_of({
        0x48, 0x89, 0x0D, 0x10, 0x00, 0x00, 0x00,  // 0: mov [rip+0x10], rcx   (a store into a static)
        0x48, 0x83, 0xFA, 0x07,                    // 7: cmp rdx, 7
        0xF3, 0x48, 0xAB,                          // 11: rep stosq            (rdi, rcx: not modelled)
        0x49, 0x8B, 0xD8,                          // 14: mov rbx, r8          (not followed past it)
        0xC3,                                      // 17: ret
    });
    const probe::CalleeCode c = probe::summarise_callee(code, 0x1000, 0);
    CHECK(c.instructions == 5);
    CHECK(c.modelled_to == 11u);
    REQUIRE(arg(c, 1).stores.size() == 1);
    CHECK(arg(c, 1).stores[0].rip);
    CHECK(arg(c, 1).stores[0].disp == 0x1000 + 7 + 0x10);  // the slot's RVA
    CHECK_FALSE(arg(c, 1).stores[0].base.has_value());
    REQUIRE(arg(c, 2).compares.size() == 1);
    CHECK(arg(c, 2).compares[0].imm == std::optional<std::uint64_t>(7));
    CHECK(arg(c, 8).saved.empty());
    CHECK(c.returns_early);
    CHECK(c.call_count == 0);
  }
  SUBCASE("an immediate past the bound is counted, not kept; a jump to the end and a tail jump") {
    const std::vector<std::byte> code = code_of({
        0x41, 0x81, 0xF8, 0x00, 0x00, 0x02, 0x00,  // 0: cmp r8d, 0x20000
        0x77, 0x01,                                // 7: ja 10
        0xC3,                                      // 9: ret
        0xE9, 0x00, 0x01, 0x00, 0x00,              // 10: jmp out (a tail jump)
    });
    const probe::CalleeCode c = probe::summarise_callee(code, 0x2000, static_cast<std::uint32_t>(code.size()));
    REQUIRE(arg(c, 8).compares.size() == 1);
    CHECK_FALSE(arg(c, 8).compares[0].imm.has_value());
    CHECK(c.returns_early);
    CHECK(c.branch_count == 1);
  }
  SUBCASE("every path through a call: no early return") {
    const std::vector<std::byte> code = code_of({
        0x48, 0x85, 0xC9,              // 0: test rcx, rcx
        0x74, 0x06,                    // 3: je 11
        0xE8, 0x00, 0x00, 0x00, 0x00,  // 5: call
        0xC3,                          // 10: ret
        0xE8, 0x00, 0x00, 0x00, 0x00,  // 11: call
        0xC3,                          // 16: ret
    });
    const probe::CalleeCode c = probe::summarise_callee(code, 0x3000, static_cast<std::uint32_t>(code.size()));
    CHECK_FALSE(c.returns_early);
    CHECK(c.call_count == 2);
    CHECK(arg(c, 1).tested == 1);
  }
  SUBCASE("a function longer than the bound is cut at 4 KiB") {
    const std::vector<std::byte> code(probe::kCalleeBound, std::byte{0x90});
    const probe::CalleeCode c = probe::summarise_callee(code, 0x4000, 0x3000);
    CHECK(c.cut);
    CHECK(c.bytes == probe::kCalleeBound);
    CHECK(c.extent == 0x3000);
    CHECK_FALSE(c.undecodable);
  }
  SUBCASE("bytes that do not decode") {
    const probe::CalleeCode c = probe::summarise_callee(code_of({0x90, 0x06}), 0x5000, 2);  // push es: not in 64-bit
    CHECK(c.undecodable);
    CHECK(c.instructions == 1);
  }
  SUBCASE("nothing read") {
    const probe::CalleeCode c = probe::summarise_callee({}, 0x6000, 0);
    CHECK_FALSE(c.read);
    CHECK(c.instructions == 0);
  }
}

namespace {

// A process of the test's own regions, as VirtualQuery would describe them.
struct Regions {
  std::vector<probe::MemoryRegion> list;  // by address
  probe::RegionQuery query() const {
    return [this](std::uint64_t at) -> std::optional<probe::MemoryRegion> {
      for (const probe::MemoryRegion& r : list) {
        if (r.base + r.size > at) return r;
      }
      return std::nullopt;
    };
  }
};

// Minimal PE headers: two sections, code at 0x1000 and writable data at 0x2000.
std::vector<std::byte> headers() {
  std::vector<std::byte> h(0x1000);
  const auto put32 = [&](std::size_t at, std::uint32_t v) { std::memcpy(h.data() + at, &v, 4); };
  const auto put16 = [&](std::size_t at, std::uint16_t v) { std::memcpy(h.data() + at, &v, 2); };
  h[0] = std::byte{'M'};
  h[1] = std::byte{'Z'};
  put32(0x3C, 0x40);
  put32(0x40, 0x0000'4550);
  put16(0x46, 2);     // sections
  put16(0x54, 0xF0);  // the optional header's size
  const std::size_t table = 0x40 + 24 + 0xF0;
  put32(table + 8, 0x1000);
  put32(table + 12, 0x1000);
  put32(table + 16, 0x1000);
  put32(table + 36, 0x6000'0020);
  put32(table + 40 + 8, 0x1000);
  put32(table + 40 + 12, 0x2000);
  put32(table + 40 + 16, 0x1000);
  put32(table + 40 + 36, 0xC000'0040);
  return h;
}

}  // namespace

TEST_CASE("probe: the whole-memory search counts the game's copies of our task's address apart from ours") {
  FakeMemory mem;
  const std::uint64_t game = 0x0000'0001'4000'0000ull, mine = 0x0000'7FF8'0000'0000ull;
  FakeSys sys(mem, game, 0x3000);
  const std::uint64_t task = 0x0000'0002'0000'0000ull, size = 0x800;
  const std::uint32_t group = 110;
  // The regions: the game's image (headers, code, data), its heap, a mapped view, a guard page, a stack, an
  // uncommitted reserve, an execute-only page, a page that faults, our module, our heap and our task's block.
  Regions rs;
  const auto add = [&](std::uint64_t base, std::uint64_t len, std::uint64_t alloc, std::uint32_t protect,
                       probe::RegionKind kind, bool committed = true, bool mapped = true) {
    rs.list.push_back(probe::MemoryRegion{base, len, alloc, protect, committed, kind});
    if (mapped && committed) mem.map(base, std::vector<std::byte>(len));
  };
  using K = probe::RegionKind;
  add(game, 0x1000, game, probe::kPageReadOnly, K::Image);
  add(game + 0x1000, 0x1000, game, probe::kPageExecuteRead, K::Image);
  add(game + 0x2000, 0x1000, game, probe::kPageReadWrite, K::Image);
  const std::uint64_t heap = 0x0000'0003'0000'0000ull, view = 0x0000'0004'0000'0000ull;
  add(heap, 0x10000, heap, probe::kPageReadWrite, K::Private);  // larger than a chunk
  add(view, 0x1000, view, probe::kPageReadOnly, K::Mapped);
  const std::uint64_t guard = 0x0000'0005'0000'0000ull, stack = 0x0000'0005'0001'0000ull;
  add(guard, 0x1000, guard, probe::kPageReadWrite | probe::kPageGuard, K::Private);
  add(stack, 0x2000, stack, probe::kPageReadWrite, K::Private);
  add(0x0000'0005'1000'0000ull, 0x10000, 0x0000'0005'1000'0000ull, 0, K::Private, false);
  const std::uint64_t exec_only = 0x0000'0005'2000'0000ull, faulty = 0x0000'0005'3000'0000ull;
  add(exec_only, 0x1000, exec_only, probe::kPageExecute, K::Private);
  add(faulty, 0x1000, faulty, probe::kPageReadWrite, K::Private, true, false);  // described, not readable
  add(task, 0x1000, task, probe::kPageReadWrite, K::Private);
  add(mine, 0x1000, mine, probe::kPageReadOnly, K::Image);
  add(mine + 0x2000, 0x1000, mine, probe::kPageReadWrite, K::Image);
  const std::uint64_t own_heap = 0x0000'7FF9'0000'0000ull;
  add(own_heap, 0x4000, own_heap, probe::kPageReadWrite, K::Private);
  std::sort(rs.list.begin(), rs.list.end(), [](const auto& a, const auto& b) { return a.base < b.base; });
  mem.map(game, headers());
  mem.map(mine, headers());
  // The copies: the game's static, its heap (one past a chunk's edge, one pointing inside, the group's index beside
  // one), a mapped view; ours in our module's data and our heap, and a self-link in our task's block.
  mem.put<std::uint64_t>(game + 0x2010, task);
  mem.put<std::uint64_t>(heap + 0x40, task);
  mem.put<std::uint32_t>(heap + 0x48, group);
  mem.put<std::uint64_t>(heap + 0x4008, task);
  mem.put<std::uint64_t>(heap + 0x9000, task + 0x18);
  mem.put<std::uint64_t>(view + 0x100, task);
  mem.put<std::uint64_t>(guard + 0x10, task);      // never read: a guard page
  mem.put<std::uint64_t>(stack + 0x10, task);      // never read: a known stack
  mem.put<std::uint64_t>(mine + 0x2020, task);
  mem.put<std::uint64_t>(own_heap + 0x100, task);
  mem.put<std::uint64_t>(task + 0x20, task);
  mem.put<std::uint64_t>(heap + 0x800, task + size + 8);  // past our object: not counted
  sys.stacks().add(AddressRange{stack, stack + 0x2000});

  probe::MemorySearchSpec spec;
  spec.target = task;
  spec.object_bytes = 8 + size;
  spec.group_index = group;
  spec.game_base = game;
  spec.own_base = mine;
  spec.own_bases = {own_heap, task};
  spec.own_known = true;
  const probe::MemorySearch m = probe::search_memory(sys, rs.query(), spec);
  CHECK(m.stopped == probe::SearchStop::Done);
  CHECK(m.own_known);
  CHECK(m.found == 4);
  CHECK(m.inside == 1);
  CHECK(m.own_found == 3);
  CHECK(m.own_inside == 0);
  CHECK(m.faults == 1);                 // the page described but not there
  CHECK(m.stack_bytes == 0x2000);
  CHECK(m.unread_bytes == 0x1000 * 2);  // the guard page and the execute-only page; the reserve is not committed
  CHECK(m.regions == rs.list.size() - 1);
  REQUIRE(m.hits.size() == 8);
  // The game's first, by address: its static, its heap's three, its view; then ours.
  CHECK_FALSE(m.hits[0].own);
  CHECK(m.hits[0].region == K::Image);
  CHECK(m.hits[0].module == probe::ModuleKind::Game);
  CHECK(m.hits[0].section == probe::SectionKind::Data);
  CHECK(m.hits[0].rva == std::optional<std::uint32_t>(0x2010));
  CHECK(m.hits[1].region == K::Private);
  CHECK(m.hits[1].near_group == 1);
  CHECK(m.hits[2].offset == 0);
  CHECK(m.hits[3].offset == 0x18);
  CHECK(m.hits[4].region == K::Mapped);
  CHECK(m.hits[4].protect == probe::kPageReadOnly);
  CHECK(m.hits[5].own);
  CHECK(m.hits[5].region == K::Private);  // our task's block
  CHECK(m.hits[6].module == probe::ModuleKind::Ours);
  CHECK(m.hits[6].section == probe::SectionKind::Data);
  CHECK(m.hits[7].region == K::Private);  // our heap
  CHECK(m.read_bytes > 0x10000);
  // The report holds it and lints clean.
  probe::Findings f;
  f.memory_search.push_back(m);
  const auto text = probe::write_report(f);
  REQUIRE(text.has_value());
  const std::array<const report::Vocabulary*, 1> known = {&probe::vocabulary()};
  CHECK(report::lint(*text, known).has_value());

  SUBCASE("without our heap's regions, its copies count as the game's, and the report says so") {
    probe::MemorySearchSpec blind = spec;
    blind.own_bases = {task};
    blind.own_known = false;
    const probe::MemorySearch b = probe::search_memory(sys, rs.query(), blind);
    CHECK_FALSE(b.own_known);
    CHECK(b.found == 5);
    CHECK(b.own_found == 2);
  }
  SUBCASE("the caps stop it, and say which") {
    probe::MemorySearchSpec capped = spec;
    capped.max_bytes = 0x1000;
    const probe::MemorySearch b = probe::search_memory(sys, rs.query(), capped);
    CHECK(b.stopped == probe::SearchStop::Bytes);
    CHECK(b.read_bytes <= 0x1000 + kMaxCopyBytes);
    capped = spec;
    capped.max_us = 3'000'000;
    sys.clock_step_us = 1'000'000;
    const probe::MemorySearch t = probe::search_memory(sys, rs.query(), capped);
    sys.clock_step_us = 0;
    CHECK(t.stopped == probe::SearchStop::Time);
  }
  SUBCASE("a range kept out is never read, page by page") {
    probe::MemorySearchSpec kept = spec;
    kept.skip.push_back(AddressRange{heap + 0x4000, heap + 0x5000});
    const probe::MemorySearch b = probe::search_memory(sys, rs.query(), kept);
    CHECK(b.found == 3);
    CHECK(b.stack_bytes == 0x2000 + 0x1000);
  }
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
  const std::vector<std::byte> h = headers();
  CHECK(probe::section_kind(h, 0x10) == probe::SectionKind::Header);
  CHECK(probe::section_kind(h, 0x1800) == probe::SectionKind::Code);
  CHECK(probe::section_kind(h, 0x2FF8) == probe::SectionKind::Data);
  CHECK(probe::section_kind(h, 0x3000) == probe::SectionKind::Other);
  CHECK(probe::section_kind(std::vector<std::byte>(0x40), 0x10) == probe::SectionKind::Other);
}
