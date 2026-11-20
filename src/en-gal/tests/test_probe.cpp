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

// ---- Spike.7 (WHY§235): the arguments mapped into calls, the flag tests, the inner callee, and the hub's memory ----

namespace {

bool passes(const probe::CalleeCall& c, std::uint8_t reg, std::uint8_t from, bool first_word = false) {
  return std::any_of(c.pass.begin(), c.pass.end(), [&](const probe::PassedArg& p) {
    return p.reg == reg && p.from == from && p.first_word == first_word;
  });
}
bool holds_none(const probe::CalleeCall& c, std::uint8_t reg) {
  return std::none_of(c.pass.begin(), c.pass.end(), [&](const probe::PassedArg& p) { return p.reg == reg; });
}

}  // namespace

TEST_CASE("probe: a wrapper's arguments are mapped into each call through the registers that keep them") {
  // A wrapper of our own ABI (instance rcx, group rdx, task r8): it keeps the task in rbx and the group in rdi, tests
  // a flag of the instance and jumps past both calls when it is set, then hands the task on in r8 and in rcx, and ends
  // in a tail jump where both paths meet.
  const std::vector<std::byte> code = code_of({
      0x48, 0x89, 0x5C, 0x24, 0x08,  //  0: mov [rsp+8], rbx
      0x57,                          //  5: push rdi
      0x48, 0x83, 0xEC, 0x20,        //  6: sub rsp, 0x20
      0x49, 0x8B, 0xD8,              // 10: mov rbx, r8           (the task)
      0x48, 0x8B, 0xFA,              // 13: mov rdi, rdx          (the group)
      0xF6, 0x41, 0x10, 0x01,        // 16: test byte [rcx+0x10], 1   (a flag of the instance)
      0x75, 0x17,                    // 20: jne 45                (past both calls)
      0x4C, 0x8B, 0xC3,              // 22: mov r8, rbx
      0x48, 0x8B, 0xD7,              // 25: mov rdx, rdi
      0xE8, 0x00, 0x00, 0x00, 0x00,  // 28: call 33               (instance, group, task, r9)
      0x48, 0x8B, 0xCB,              // 33: mov rcx, rbx
      0x8B, 0xD7,                    // 36: mov edx, edi
      0xE8, 0x00, 0x00, 0x00, 0x00,  // 38: call 43               (task, group)
      0xEB, 0x00,                    // 43: jmp 45
      0x48, 0x8B, 0x5C, 0x24, 0x30,  // 45: mov rbx, [rsp+0x30]   (both paths meet)
      0x48, 0x83, 0xC4, 0x20,        // 50: add rsp, 0x20
      0x5F,                          // 54: pop rdi
      0xE9, 0x00, 0x01, 0x00, 0x00,  // 55: jmp out               (a tail jump)
  });
  const std::uint32_t rva = 0x200000;
  const probe::CalleeCode c = probe::summarise_callee(code, rva, static_cast<std::uint32_t>(code.size()));
  CHECK_FALSE(c.modelled_to.has_value());
  CHECK(c.call_count == 2);
  REQUIRE(c.calls.size() == 3);
  const probe::CalleeCall& first = c.calls[0];
  CHECK(first.offset == 28);
  CHECK(first.target == std::optional<std::uint32_t>(rva + 33));
  CHECK(first.reached);
  CHECK(first.pass.size() == 4);
  CHECK(passes(first, 1, 1));  // the instance, untouched
  CHECK(passes(first, 2, 2));  // the group, by way of rdi
  CHECK(passes(first, 8, 8));  // the task, by way of rbx
  CHECK(passes(first, 9, 9));
  const probe::CalleeCall& second = c.calls[1];
  CHECK(second.offset == 38);
  CHECK(passes(second, 1, 8));  // the task in rcx
  CHECK(passes(second, 2, 2));  // the group, 32-bit
  CHECK(holds_none(second, 8));  // the first call's to change
  CHECK(holds_none(second, 9));
  const probe::CalleeCall& tail = c.calls[2];
  CHECK(tail.form == probe::CallForm::Tail);
  CHECK(tail.offset == 55);
  CHECK(tail.target == std::optional<std::uint32_t>(rva + 60 + 0x100));
  CHECK(tail.reached);
  CHECK(tail.pass.empty());  // the paths disagree on every argument register
  REQUIRE(c.guards.size() == 1);
  const probe::CalleeGuard& g = c.guards[0];
  CHECK(g.offset == 20);
  CHECK(g.to == 45);
  CHECK(g.cond == 5);  // ne
  CHECK(g.op == probe::GuardOp::Test);
  CHECK(g.op_at == std::optional<std::uint32_t>(16));
  CHECK(g.width == 1);
  REQUIRE(g.operands.size() == 2);
  CHECK(g.operands[0].form == probe::OperandForm::Memory);
  CHECK(g.operands[0].base == std::optional<std::uint8_t>(1));
  CHECK(g.operands[0].base_arg == std::optional<std::uint8_t>(1));
  CHECK(g.operands[0].disp == 0x10);
  CHECK(g.operands[1].form == probe::OperandForm::Immediate);
  CHECK(g.operands[1].imm == std::optional<std::uint64_t>(1));
  CHECK(arg(c, 8).saved == std::vector<std::uint8_t>{3});
  CHECK(arg(c, 2).saved == std::vector<std::uint8_t>{7});
  // The entry of each callee, as its call passes the wrapper's arguments.
  const probe::EntryState e1 = probe::entry_at(first);
  CHECK(e1[0].from == 1);
  CHECK(e1[2].from == 8);
  const probe::EntryState e2 = probe::entry_at(second);
  CHECK(e2[0].from == 8);
  CHECK(e2[1].from == 2);
  CHECK(e2[2].from == 0);
  CHECK(e2[3].from == 0);
}

TEST_CASE("probe: an inner callee's summary follows the wrapper's arguments into it") {
  // The fake game's shape: the wrapper (task rcx, hub rdx, group r8d) calls in with the hub in rcx, the group in edx
  // and the task in r8; the inner callee compares the group with a bound, keeps the task and its vtable in the hub.
  const std::vector<std::byte> code = code_of({
      0x53,                                // 0: push rbx
      0x48, 0x83, 0xEC, 0x20,              // 1: sub rsp, 0x20
      0x4C, 0x89, 0xC3,                    // 5: mov rbx, r8          (the task)
      0x81, 0xFA, 0x00, 0x02, 0x00, 0x00,  // 8: cmp edx, 0x200       (the group, with a bound)
      0x73, 0x19,                          // 14: jae 41
      0x48, 0x89, 0x59, 0x30,              // 16: mov [rcx+0x30], rbx (the task, into the hub)
      0x48, 0x8B, 0x03,                    // 20: mov rax, [rbx]      (its vtable)
      0x48, 0x89, 0x41, 0x38,              // 23: mov [rcx+0x38], rax (the vtable, into the hub)
      0x49, 0x89, 0xD8,                    // 27: mov r8, rbx
      0xE8, 0x00, 0x00, 0x00, 0x00,        // 30: call 35
      0x48, 0x83, 0xC4, 0x20,              // 35: add rsp, 0x20
      0x5B,                                // 39: pop rbx
      0xC3,                                // 40: ret
      0x31, 0xC0,                          // 41: xor eax, eax
      0x48, 0x83, 0xC4, 0x20,              // 43: add rsp, 0x20
      0x5B,                                // 47: pop rbx
      0xC3,                                // 48: ret
  });
  probe::CalleeCall site;
  site.pass = {probe::PassedArg{1, 2, false}, probe::PassedArg{2, 8, false}, probe::PassedArg{8, 1, false}};
  const probe::CalleeCode c =
      probe::summarise_callee(code, 0x300000, static_cast<std::uint32_t>(code.size()), probe::entry_at(site));
  CHECK(c.instructions == 17);
  CHECK(c.returns_early);
  const probe::ArgUse& task = arg(c, 1);  // the wrapper's rcx
  CHECK(task.saved == std::vector<std::uint8_t>{3});
  REQUIRE(task.stores.size() == 2);
  CHECK(task.stores[0].offset == 16);
  CHECK(task.stores[0].via == 3);
  CHECK(task.stores[0].base_arg == std::optional<std::uint8_t>(2));  // the hub: the wrapper's rdx
  CHECK(task.stores[0].disp == 0x30);
  CHECK_FALSE(task.stores[0].first_word);
  CHECK(task.stores[1].offset == 23);
  CHECK(task.stores[1].via == 0);
  CHECK(task.stores[1].disp == 0x38);
  CHECK(task.stores[1].first_word);  // the task's vtable
  const probe::ArgUse& group = arg(c, 8);  // the wrapper's r8
  REQUIRE(group.compares.size() == 1);
  CHECK(group.compares[0].via == 2);
  CHECK(group.compares[0].imm == std::optional<std::uint64_t>(0x200));
  CHECK(arg(c, 9).stores.empty());
  REQUIRE(c.calls.size() == 1);
  CHECK(passes(c.calls[0], 1, 2));
  CHECK(passes(c.calls[0], 2, 8));
  CHECK(passes(c.calls[0], 8, 1));
  REQUIRE(c.guards.size() == 1);
  CHECK(c.guards[0].cond == 3);  // ae
  CHECK(c.guards[0].op == probe::GuardOp::Compare);
  CHECK(c.guards[0].width == 4);
  REQUIRE(c.guards[0].operands.size() == 2);
  CHECK(c.guards[0].operands[0].form == probe::OperandForm::Register);
  CHECK(c.guards[0].operands[0].reg == std::optional<std::uint8_t>(2));
  CHECK(c.guards[0].operands[0].from == std::optional<std::uint8_t>(8));
  CHECK(c.guards[0].operands[1].imm == std::optional<std::uint64_t>(0x200));
  // The report holds the inner summary and lints clean.
  probe::Findings f;
  f.callee_code = c;
  f.callee_inner.push_back(probe::InnerCallee{0x300000, 30, false, c});
  const auto text = probe::write_report(f);
  REQUIRE(text.has_value());
  const std::array<const report::Vocabulary*, 1> known = {&probe::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
}

TEST_CASE("probe: where paths disagree a register holds nothing, and code no known path reaches holds nothing") {
  SUBCASE("two paths into one call") {
    const std::vector<std::byte> code = code_of({
        0x48, 0x85, 0xC9,              // 0: test rcx, rcx
        0x74, 0x05,                    // 3: je 10
        0x4C, 0x8B, 0xC1,              // 5: mov r8, rcx
        0xEB, 0x03,                    // 8: jmp 13
        0x4C, 0x8B, 0xC2,              // 10: mov r8, rdx
        0x48, 0x8B, 0xD9,              // 13: mov rbx, rcx
        0xE8, 0x00, 0x00, 0x00, 0x00,  // 16: call 21
        0xC3,                          // 21: ret
    });
    const probe::CalleeCode c = probe::summarise_callee(code, 0x1000, static_cast<std::uint32_t>(code.size()));
    REQUIRE(c.calls.size() == 1);
    CHECK(passes(c.calls[0], 1, 1));
    CHECK(passes(c.calls[0], 2, 2));
    CHECK(passes(c.calls[0], 9, 9));
    CHECK(holds_none(c.calls[0], 8));  // rcx on one path, rdx on the other
    CHECK(arg(c, 1).saved == std::vector<std::uint8_t>{3});
    REQUIRE(c.guards.size() == 1);
    CHECK(c.guards[0].cond == 4);  // e
    CHECK(c.guards[0].op == probe::GuardOp::Test);
    REQUIRE(c.guards[0].operands.size() == 2);
    CHECK(c.guards[0].operands[0].from == std::optional<std::uint8_t>(1));
    CHECK(c.guards[0].operands[1].from == std::optional<std::uint8_t>(1));
  }
  SUBCASE("a call reached only through a jump the summary cannot follow") {
    const std::vector<std::byte> code = code_of({
        0xFF, 0xE0,                    // 0: jmp rax
        0xE8, 0x00, 0x00, 0x00, 0x00,  // 2: call 7
        0xC3,                          // 7: ret
    });
    const probe::CalleeCode c = probe::summarise_callee(code, 0x2000, static_cast<std::uint32_t>(code.size()));
    REQUIRE(c.calls.size() == 2);
    CHECK(c.calls[0].form == probe::CallForm::TailRegister);  // since spike.8, a tail call through a register
    CHECK(c.calls[0].reached);
    CHECK(c.calls[0].base == std::optional<std::uint8_t>(0));
    CHECK(c.call_count == 1);
    CHECK_FALSE(c.calls[1].reached);
    CHECK(c.calls[1].pass.empty());
    REQUIRE(c.forms.size() == 3);  // the call and the return after it are no known path's: padding
    CHECK_FALSE(c.forms[0].unreached);
    CHECK(c.forms[1].unreached);
    CHECK(c.forms[2].unreached);
  }
  SUBCASE("a flag test past moves, and a byte register's high half") {
    const std::vector<std::byte> code = code_of({
        0x80, 0x79, 0x7C, 0x00,  // 0: cmp byte [rcx+0x7C], 0
        0x48, 0x8B, 0xC2,        // 4: mov rax, rdx      (sets no flag)
        0x74, 0x01,              // 7: je 10
        0xC3,                    // 9: ret
        0x84, 0xE9,              // 10: test cl, ch      (ch: rcx's second byte)
        0x75, 0x01,              // 12: jne 15
        0xC3,                    // 14: ret
        0xC3,                    // 15: ret
    });
    const probe::CalleeCode c = probe::summarise_callee(code, 0x3000, static_cast<std::uint32_t>(code.size()));
    REQUIRE(c.guards.size() == 2);
    CHECK(c.guards[0].op == probe::GuardOp::Compare);
    CHECK(c.guards[0].op_at == std::optional<std::uint32_t>(0));
    REQUIRE(c.guards[0].operands.size() == 2);
    CHECK(c.guards[0].operands[0].base_arg == std::optional<std::uint8_t>(1));
    CHECK(c.guards[0].operands[0].disp == 0x7C);
    CHECK(c.guards[0].operands[1].imm == std::optional<std::uint64_t>(0));
    REQUIRE(c.guards[1].operands.size() == 2);
    CHECK(c.guards[1].operands[0].reg == std::optional<std::uint8_t>(1));
    CHECK(c.guards[1].operands[1].reg == std::optional<std::uint8_t>(1));  // ch is rcx's
    CHECK(arg(c, 1).tested == 1);
  }
}

namespace {

// A hub of the test's own: its instance (a frame counter, a table, the last task), the table (two blocks), the blocks.
struct HubWorld {
  FakeMemory mem;
  FakeSys sys{mem, 0x0000'0001'4000'0000ull, 0x3000};
  Regions rs;
  static constexpr std::uint64_t kGame = 0x0000'0001'4000'0000ull, kMine = 0x0000'7FF8'0000'0000ull;
  static constexpr std::uint64_t kHub = 0x0000'0002'0000'0000ull, kTable = 0x0000'0002'0001'0000ull;
  static constexpr std::uint64_t kB1 = 0x0000'0002'0002'0000ull, kB2 = 0x0000'0002'0003'0000ull;
  static constexpr std::uint64_t kFresh = 0x0000'0002'0004'0000ull, kVtable = 0x0000'0003'0000'0000ull;
  static constexpr std::uint64_t kTask = 0x0000'0004'0000'0000ull, kTaskBytes = 0x818;
  static constexpr std::uint32_t kGroup = 110;

  HubWorld() {
    const auto add = [&](std::uint64_t base, std::uint64_t len, std::uint64_t alloc, probe::RegionKind kind,
                         bool mapped = true) {
      rs.list.push_back(probe::MemoryRegion{base, len, alloc, probe::kPageReadWrite, true, kind});
      if (mapped) mem.map(base, std::vector<std::byte>(len));
    };
    using K = probe::RegionKind;
    add(kGame, 0x3000, kGame, K::Image);
    for (const std::uint64_t b : {kHub, kTable, kB1, kB2, kFresh}) add(b, 0x1000, b, K::Private);
    add(kVtable, 0x1000, kVtable, K::Private);
    add(kTask, 0x1000, kTask, K::Private);
    add(kMine, 0x2000, kMine, K::Image, false);  // our module: its code is never mapped here, so a read of it faults
    std::sort(rs.list.begin(), rs.list.end(), [](const auto& a, const auto& b) { return a.base < b.base; });
    mem.put<std::uint64_t>(kHub + 0x08, 600);      // the frame
    mem.put<std::uint64_t>(kHub + 0x18, kTable);
    mem.put<std::uint64_t>(kHub + 0x30, kB2);      // the last task, before ours
    mem.put<std::uint64_t>(kTable + 0x00, kB1);
    mem.put<std::uint64_t>(kTable + 0x08, kB2);
    mem.put<std::uint64_t>(kB1 + 0x08, 4);         // a count
    mem.put<std::uint64_t>(kVtable, kMine + 0x1000);  // our slots, in our module
    mem.put<std::uint64_t>(kVtable + 8, kMine + 0x1040);
  }
  probe::HubWalk walk() {
    probe::HubWalkSpec spec;
    spec.instance = kHub;
    spec.image_base = kGame;
    spec.image_end = kGame + 0x3000;
    spec.own_image = AddressRange{kMine, kMine + 0x2000};
    spec.keep = AddressRange{kTask, kTask + kTaskBytes};
    return probe::walk_hub(sys, spec);
  }
  probe::DiffSpec diff_spec() const {
    probe::DiffSpec d;
    d.group_index = kGroup;
    d.task = AddressRange{kTask, kTask + kTaskBytes};
    d.vtable = AddressRange{kVtable, kVtable + 0x10};
    d.own_base = kMine;
    d.game_base = kGame;
    d.game_sections = {probe::SectionSpan{0x1000, 0x1000, probe::SectionKind::Code},
                       probe::SectionSpan{0x2000, 0x1000, probe::SectionKind::Data}};
    d.query = rs.query();
    return d;
  }
};

}  // namespace

TEST_CASE("probe: the hub's memory before and after the call, compared block by block by path") {
  HubWorld w;
  const probe::HubWalk before = w.walk();
  REQUIRE(before.blocks.size() == 4);  // the hub, the table, the last task's block, and the table's first block
  CHECK(before.blocks[2].address == HubWorld::kB2);
  CHECK(before.blocks[2].depth == 1);
  CHECK(before.blocks[3].address == HubWorld::kB1);
  CHECK(before.blocks[3].depth == 2);
  CHECK_FALSE(before.capped);
  CHECK(before.bytes.size() == before.read_bytes);
  // The call: the hub keeps our task and our vtable, counts it, and the frame goes on.
  w.mem.put<std::uint64_t>(HubWorld::kHub + 0x08, 601);
  w.mem.put<std::uint64_t>(HubWorld::kHub + 0x30, HubWorld::kTask);
  w.mem.put<std::uint64_t>(HubWorld::kHub + 0x38, HubWorld::kVtable);
  w.mem.put<std::uint64_t>(HubWorld::kTable + 0x10, HubWorld::kTask);
  w.mem.put<std::uint64_t>(HubWorld::kB1 + 0x08, 5);
  w.mem.put<std::uint64_t>(HubWorld::kB1 + 0x10, (std::uint64_t{0x1234} << 32) | HubWorld::kGroup);
  w.mem.put<std::uint64_t>(HubWorld::kB1 + 0x18, HubWorld::kGame + 0x2010);
  w.mem.put<std::uint64_t>(HubWorld::kB1 + 0x20, HubWorld::kFresh);
  const probe::HubWalk after = w.walk();
  CHECK(after.faults == 0);  // our two slots, in our module: not followed

  // The hub search over the walk: our task's address where the hub kept it, not followed into.
  const probe::HubSearch h = probe::search_walk(after, HubWorld::kTask, HubWorld::kTask + 0x808, HubWorld::kGroup);
  CHECK(h.found == 2);
  REQUIRE(h.hits.size() == 2);
  CHECK(h.hits[0].depth == 0);
  CHECK(h.hits[0].path == std::vector<std::uint32_t>{0x30});
  CHECK(h.hits[1].path == std::vector<std::uint32_t>{0x18, 0x10});

  const probe::DiffSpec spec = w.diff_spec();
  const probe::DiffResult r = probe::diff_hub(before, after, spec);
  const probe::HubDiff& d = r.diff;
  CHECK(d.before_blocks == 4);
  CHECK(d.after_blocks == 6);
  CHECK(d.compared == 4);      // the last task's block found again through the table
  CHECK(d.moved == 0);
  CHECK(d.gone == 0);
  CHECK(d.new_blocks == 2);    // our vtable and the fresh block
  CHECK(d.new_bytes == 0x800);
  CHECK(d.changed == 8);
  CHECK_FALSE(d.steady_words.has_value());
  CHECK_FALSE(d.capped);
  CHECK(d.kept_bytes == before.bytes.size());
  REQUIRE(d.words.size() == 8);
  using V = probe::ValueKind;
  CHECK(d.words[0].path == std::vector<std::uint32_t>{0x08});
  CHECK(d.words[0].before.kind == V::Small);
  CHECK(d.words[0].after.value == std::optional<std::uint64_t>(601));
  CHECK(d.words[1].path == std::vector<std::uint32_t>{0x30});
  CHECK(d.words[1].before.kind == V::Heap);
  CHECK_FALSE(d.words[1].before.fresh.has_value());
  CHECK(d.words[1].after.kind == V::Task);
  CHECK(d.words[1].after.fresh == std::optional<bool>(true));
  CHECK(d.words[2].before.kind == V::Zero);
  CHECK(d.words[2].after.kind == V::Vtable);
  CHECK(d.words[3].depth == 1);
  CHECK(d.words[3].path == std::vector<std::uint32_t>{0x18, 0x10});
  CHECK(d.words[3].after.kind == V::Task);
  CHECK(d.words[4].path == std::vector<std::uint32_t>{0x18, 0x00, 0x08});
  CHECK(d.words[4].after.value == std::optional<std::uint64_t>(5));
  CHECK(d.words[5].after.kind == V::Other);
  CHECK(d.words[5].after.group_half);
  CHECK(d.words[6].after.kind == V::Game);
  CHECK(d.words[6].after.section == probe::SectionKind::Data);
  CHECK(d.words[6].after.rva == std::optional<std::uint32_t>(0x2010));
  CHECK(d.words[7].after.kind == V::Heap);
  CHECK(d.words[7].after.fresh == std::optional<bool>(true));
  CHECK(r.changed.size() == 8);

  SUBCASE("a later diff tells the steady changes from the moving ones, the steady first") {
    w.mem.put<std::uint64_t>(HubWorld::kHub + 0x08, 1700);
    w.mem.put<std::uint64_t>(HubWorld::kVtable, HubWorld::kGame + 0x1100);  // read only by the walk: not compared
    const probe::HubWalk later = w.walk();
    probe::DiffSpec s2 = spec;
    s2.earlier = &r.changed;
    const probe::HubDiff d2 = probe::diff_hub(before, later, s2).diff;
    CHECK(d2.changed == 8);
    CHECK(d2.steady_words == std::optional<std::uint64_t>(7));
    REQUIRE(d2.words.size() == 8);
    CHECK(d2.words[0].path == std::vector<std::uint32_t>{0x30});
    CHECK(d2.words[0].steady == std::optional<bool>(true));
    CHECK(d2.words[7].path == std::vector<std::uint32_t>{0x08});
    CHECK(d2.words[7].steady == std::optional<bool>(false));
  }
  SUBCASE("a path that now leads elsewhere, and a block no longer reached") {
    w.mem.put<std::uint64_t>(HubWorld::kTable + 0x00, HubWorld::kFresh);  // the table's first entry, re-pointed
    w.mem.put<std::uint64_t>(HubWorld::kB1 + 0x20, 0);
    const probe::HubDiff d3 = probe::diff_hub(before, w.walk(), spec).diff;
    CHECK(d3.moved == 1);
    CHECK(d3.gone == 1);  // the table's first block
  }
  SUBCASE("the caps stop the walk, and the diff says so") {
    probe::HubWalkSpec small;
    small.instance = HubWorld::kHub;
    small.image_base = HubWorld::kGame;
    small.image_end = HubWorld::kGame + 0x3000;
    small.max_blocks = 2;
    const probe::HubWalk capped = probe::walk_hub(w.sys, small);
    CHECK(capped.capped);
    CHECK(capped.blocks.size() == 2);
    CHECK(probe::diff_hub(before, capped, spec).diff.capped);
  }
  // The report holds both and lints clean.
  probe::Findings f;
  f.hub_diff.push_back(d);
  const auto text = probe::write_report(f);
  REQUIRE(text.has_value());
  const std::array<const report::Vocabulary*, 1> known = {&probe::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
}

TEST_CASE("probe: the whole-memory search counts our vtable's and slots' copies apart, and the committed total") {
  FakeMemory mem;
  const std::uint64_t game = 0x0000'0001'4000'0000ull, mine = 0x0000'7FF8'0000'0000ull;
  FakeSys sys(mem, game, 0x1000);
  const std::uint64_t heap = 0x0000'0003'0000'0000ull, own_heap = 0x0000'7FF9'0000'0000ull;
  const std::uint64_t task = 0x0000'0004'0000'0000ull, vtable = 0x0000'0004'0001'0000ull;
  Regions rs;
  const auto add = [&](std::uint64_t base, std::uint64_t len, probe::RegionKind kind, std::uint32_t protect) {
    rs.list.push_back(probe::MemoryRegion{base, len, base, protect, true, kind});
    mem.map(base, std::vector<std::byte>(len));
  };
  using K = probe::RegionKind;
  add(game, 0x1000, K::Image, probe::kPageReadOnly);
  add(heap, 0x8000, K::Private, probe::kPageReadWrite);
  add(task, 0x1000, K::Private, probe::kPageReadWrite);
  add(vtable, 0x1000, K::Private, probe::kPageReadOnly);
  add(mine, 0x1000, K::Image, probe::kPageReadOnly);
  add(own_heap, 0x1000, K::Private, probe::kPageReadWrite);
  add(0x0000'7FFA'0000'0000ull, 0x1000, K::Private, probe::kPageNoAccess);  // committed, never read
  std::sort(rs.list.begin(), rs.list.end(), [](const auto& a, const auto& b) { return a.base < b.base; });
  const std::vector<std::uint64_t> slots = {mine + 0x400, mine + 0x440, mine + 0x480};
  mem.put<std::uint64_t>(task, vtable);  // our object's first word
  for (std::size_t i = 0; i < slots.size(); ++i) {
    mem.put<std::uint64_t>(vtable + 8 * i, slots[i]);  // our vtable
    mem.put<std::uint64_t>(mine + 0x100 + 8 * i, slots[i]);  // our module's table of thunks
  }
  // The game's copies: our vtable once, the second slot twice and the third once; ours: our vtable in our heap.
  mem.put<std::uint64_t>(heap + 0x10, vtable);
  mem.put<std::uint64_t>(heap + 0x18, slots[1]);
  mem.put<std::uint64_t>(heap + 0x4000, slots[1]);
  mem.put<std::uint64_t>(heap + 0x6008, slots[2]);
  mem.put<std::uint64_t>(own_heap + 0x20, vtable);
  probe::MemorySearchSpec spec;
  spec.target = task;
  spec.object_bytes = 0x808;
  spec.vtable = vtable;
  spec.slots = slots;
  spec.game_base = game;
  spec.own_base = mine;
  spec.own_bases = {own_heap, task, vtable};
  spec.own_known = true;
  const probe::MemorySearch m = probe::search_memory(sys, rs.query(), spec);
  CHECK(m.stopped == probe::SearchStop::Done);
  CHECK(m.vtable_found == 1);
  CHECK(m.own_vtable_found == 2);  // our object's first word, and our heap's copy
  CHECK(m.slot_found == 3);
  CHECK(m.slot_distinct == 2);
  CHECK(m.own_slot_found == 6);    // our vtable and our module's table
  CHECK(m.own_slot_distinct == 3);
  CHECK(m.found == 0);
  CHECK(m.committed_bytes == 0xE000);
  CHECK(m.regions == 7);
  REQUIRE(m.hits.size() == 12);  // the game's vtable and three slots; ours: two vtables and six slots
  CHECK(m.hits[0].points_to == probe::PointsTo::Vtable);
  CHECK_FALSE(m.hits[0].own);
  CHECK(m.hits[1].points_to == probe::PointsTo::Slot);
  CHECK(m.hits[1].slot == std::optional<std::uint32_t>(1));
  CHECK(m.hits[3].slot == std::optional<std::uint32_t>(2));
  CHECK(m.hits[4].own);
  CHECK(m.hits[4].points_to == probe::PointsTo::Vtable);
  SUBCASE("past the cap of bytes the regions are still walked for the committed total") {
    probe::MemorySearchSpec capped = spec;
    capped.max_bytes = 0x1000;
    const probe::MemorySearch b = probe::search_memory(sys, rs.query(), capped);
    CHECK(b.stopped == probe::SearchStop::Bytes);
    CHECK(b.committed_bytes == 0xE000);
    CHECK(b.regions == 7);
    CHECK(b.read_bytes < 0xE000);
  }
  probe::Findings f;
  f.memory_search.push_back(m);
  const auto text = probe::write_report(f);
  REQUIRE(text.has_value());
  const std::array<const report::Vocabulary*, 1> known = {&probe::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
}

// ---- Spike.8 (WHY§240): the values loaded from memory, jumps through memory or a register, the forms, the table ----

namespace {

// A wrapper of run 7's shape, of our own ABI (instance rcx, group rdx, task r8): it keeps the task in rbx and the group
// in edi, loads its own singleton from a global and, while that is null, makes it with two calls and stores it there,
// then tail-jumps with the singleton in rcx, the group in edx and the task in r8. Our instance is never used.
constexpr std::uint32_t kWrapperRva = 0x400000, kSingletonSlot = 0x480000, kTailRva = 0x400100;
std::vector<std::byte> lazy_wrapper() {
  return code_of({
      0x48, 0x89, 0x5C, 0x24, 0x08,              //  0: mov [rsp+8], rbx
      0x57,                                      //  5: push rdi
      0x48, 0x83, 0xEC, 0x20,                    //  6: sub rsp, 0x20
      0x49, 0x8B, 0xD8,                          // 10: mov rbx, r8          (the task)
      0x8B, 0xFA,                                // 13: mov edi, edx         (the group)
      0x48, 0x8B, 0x0D, 0xEA, 0xFF, 0x07, 0x00,  // 15: mov rcx, [rip+S]     (S: 0x480000)
      0x48, 0x85, 0xC9,                          // 22: test rcx, rcx
      0x75, 0x1C,                                // 25: jne 55              (past both calls)
      0xB9, 0x40, 0x00, 0x00, 0x00,              // 27: mov ecx, 0x40
      0xE8, 0x00, 0x00, 0x00, 0x00,              // 32: call 37             (the memory)
      0x48, 0x8B, 0xC8,                          // 37: mov rcx, rax
      0xE8, 0x00, 0x00, 0x00, 0x00,              // 40: call 45             (the singleton, made)
      0x48, 0x8B, 0xC8,                          // 45: mov rcx, rax
      0x48, 0x89, 0x05, 0xC9, 0xFF, 0x07, 0x00,  // 48: mov [rip+S], rax     (rax and rcx now hold S's value)
      0x8B, 0xD7,                                // 55: mov edx, edi         (both paths meet)
      0x4C, 0x8B, 0xC3,                          // 57: mov r8, rbx
      0x48, 0x8B, 0x5C, 0x24, 0x30,              // 60: mov rbx, [rsp+0x30]
      0x48, 0x83, 0xC4, 0x20,                    // 65: add rsp, 0x20
      0x5F,                                      // 69: pop rdi
      0xE9, 0xB5, 0x00, 0x00, 0x00,              // 70: jmp 0x400100         (the tail function)
  });
}

// The tail function of run 7's shape (32 bytes, 10 instructions): the group bounded by 168, the entry for it loaded
// from [singleton + group*8], tested for null (0 answered), else a jump through its vtable's slot at 0x18.
std::vector<std::byte> tail_function() {
  return code_of({
      0x81, 0xFA, 0xA8, 0x00, 0x00, 0x00,  //  0: cmp edx, 168
      0x0F, 0x87, 0x11, 0x00, 0x00, 0x00,  //  6: ja 29
      0x48, 0x8B, 0x0C, 0xD1,              // 12: mov rcx, [rcx + rdx*8]
      0x48, 0x85, 0xC9,                    // 16: test rcx, rcx
      0x74, 0x08,                          // 19: je 29
      0x48, 0x8B, 0x01,                    // 21: mov rax, [rcx]
      0xFF, 0x60, 0x18,                    // 24: jmp [rax + 0x18]
      0x66, 0x90,                          // 27: nop                    (no path reaches it)
      0x31, 0xC0,                          // 29: xor eax, eax
      0xC3,                                // 31: ret
  });
}

const probe::PassedSource* passed_source(const probe::CalleeCall& c, std::uint8_t reg) {
  for (const probe::PassedSource& p : c.pass_src) {
    if (p.reg == reg) return &p;
  }
  return nullptr;
}

}  // namespace

TEST_CASE("probe: a value loaded from a global is followed through a lazily made singleton to the tail jump") {
  const std::vector<std::byte> code = lazy_wrapper();
  const probe::CalleeCode c = probe::summarise_callee(code, kWrapperRva, static_cast<std::uint32_t>(code.size()));
  CHECK_FALSE(c.modelled_to.has_value());
  CHECK(c.call_count == 2);
  REQUIRE(c.guards.size() == 1);
  const probe::CalleeGuard& g = c.guards[0];
  CHECK(g.op == probe::GuardOp::Test);
  CHECK(g.width == 8);
  REQUIRE(g.operands.size() == 2);
  CHECK_FALSE(g.operands[0].from.has_value());  // none of the arguments: our instance is lost at 15
  REQUIRE(g.operands[0].source.has_value());
  CHECK(g.operands[0].source->kind == probe::Source::Kind::Rip);
  CHECK(g.operands[0].source->target == kSingletonSlot);
  CHECK(g.operands[0].source->at == 15);
  CHECK(g.operands[0].source->width == 8);
  REQUIRE(c.calls.size() == 3);
  const probe::CalleeCall& tail = c.calls[2];
  CHECK(tail.form == probe::CallForm::Tail);
  CHECK(tail.target == std::optional<std::uint32_t>(kTailRva));
  CHECK(passes(tail, 2, 2));  // the group
  CHECK(passes(tail, 8, 8));  // the task
  CHECK(holds_none(tail, 1));
  // rcx holds S's value on both paths: loaded on the one, stored there (the call's result moved first) on the other.
  const probe::PassedSource* rcx = passed_source(tail, 1);
  REQUIRE(rcx != nullptr);
  CHECK(rcx->source.kind == probe::Source::Kind::Rip);
  CHECK(rcx->source.target == kSingletonSlot);
  CHECK(rcx->source.at == 15);  // of the loads and stores that make it, the first
  // The calls before the store pass nothing of S.
  CHECK(passed_source(c.calls[1], 1) == nullptr);
  // The forms: classes and operands, no bytes.
  REQUIRE(c.forms.size() == 20);
  using C = probe::InsnClass;
  CHECK(c.forms[0].cls == C::Mov);
  REQUIRE(c.forms[0].operands.size() == 2);
  CHECK(c.forms[0].operands[0].form == probe::OperandForm::Memory);
  CHECK(c.forms[0].operands[0].base == std::optional<std::uint8_t>(4));
  CHECK(c.forms[0].operands[0].disp == 8);
  CHECK(c.forms[0].operands[0].width == 8);
  CHECK(c.forms[1].cls == C::Push);
  CHECK(c.forms[2].cls == C::Sub);
  CHECK(c.forms[2].operands[1].imm == std::optional<std::uint64_t>(0x20));
  CHECK(c.forms[3].operands[1].from == std::optional<std::uint8_t>(8));
  CHECK(c.forms[4].operands[0].width == 4);
  CHECK(c.forms[5].operands[1].form == probe::OperandForm::Rip);
  CHECK(c.forms[5].operands[1].target == std::optional<std::uint32_t>(kSingletonSlot));
  CHECK(c.forms[6].cls == C::Test);
  CHECK(c.forms[7].cls == C::Jcc);
  CHECK(c.forms[7].cond == 5);
  CHECK(c.forms[7].to == std::optional<std::int64_t>(55));
  CHECK(c.forms[9].cls == C::Call);
  CHECK(c.forms[9].to == std::optional<std::int64_t>(37));
  CHECK(c.forms[19].cls == C::Jmp);
  CHECK(c.forms[19].to == std::optional<std::int64_t>(0x100));
  CHECK(std::none_of(c.forms.begin(), c.forms.end(), [](const probe::Form& f) { return f.unreached; }));
  // The sections, from the image's.
  probe::CalleeCode marked = c;
  const std::array<probe::SectionSpan, 2> sections = {probe::SectionSpan{0x1000, 0x7F000, probe::SectionKind::Code},
                                                      probe::SectionSpan{0x480000, 0x1000, probe::SectionKind::Data}};
  probe::mark_sections(marked, sections);
  CHECK(marked.forms[5].operands[1].section == probe::SectionKind::Data);
}

TEST_CASE("probe: the tail function's null test reads its table, and its jump through the entry's vtable is a tail call") {
  const std::vector<std::byte> wrapper_code = lazy_wrapper();
  const probe::CalleeCode wrapper =
      probe::summarise_callee(wrapper_code, kWrapperRva, static_cast<std::uint32_t>(wrapper_code.size()));
  const std::vector<std::byte> code = tail_function();
  const probe::CalleeCode t = probe::summarise_callee(code, kTailRva, 0, probe::entry_at(wrapper.calls[2]));
  CHECK(t.bytes == 32);
  CHECK(t.instructions == 10);
  CHECK(t.call_count == 0);
  CHECK(t.returns_early);
  REQUIRE(t.guards.size() == 2);
  CHECK(t.guards[0].op == probe::GuardOp::Compare);
  CHECK(t.guards[0].operands[0].from == std::optional<std::uint8_t>(2));
  CHECK(t.guards[0].operands[1].imm == std::optional<std::uint64_t>(168));
  const probe::Operand& tested = t.guards[1].operands[0];
  REQUIRE(tested.source.has_value());
  const probe::Source& v = *tested.source;
  CHECK(v.kind == probe::Source::Kind::Memory);
  CHECK(v.at == 12);
  CHECK(v.base == std::optional<std::uint8_t>(1));
  REQUIRE(v.base_src.size() == 1);
  CHECK(v.base_src[0].kind == probe::Source::Kind::Entry);  // the value the tail function was entered with in rcx
  CHECK(v.base_src[0].from == 1);
  CHECK(v.index == std::optional<std::uint8_t>(2));
  REQUIRE(v.index_src.size() == 1);
  CHECK(v.index_src[0].kind == probe::Source::Kind::Argument);
  CHECK(v.index_src[0].from == 2);  // the wrapper's group
  CHECK(v.scale == 8);
  CHECK(v.disp == 0);
  // The jump through the entry's vtable: a tail call through memory, its base the entry's first word.
  REQUIRE(t.calls.size() == 1);
  const probe::CalleeCall& j = t.calls[0];
  CHECK(j.form == probe::CallForm::TailMemory);
  CHECK(j.offset == 24);
  CHECK(j.reached);
  CHECK(j.base == std::optional<std::uint8_t>(0));
  CHECK(j.disp == 0x18);
  CHECK_FALSE(j.vtable_of.has_value());
  REQUIRE(j.vtable_src.has_value());
  CHECK(probe::same_source(*j.vtable_src, v));
  REQUIRE(j.base_src.has_value());
  CHECK(j.base_src->kind == probe::Source::Kind::Memory);
  CHECK(j.base_src->disp == 0);
  CHECK(passes(j, 2, 2));
  CHECK(passes(j, 8, 8));
  REQUIRE(passed_source(j, 1) != nullptr);
  CHECK(probe::same_source(passed_source(j, 1)->source, v));  // the entry, as its own function's this
  // The padding after the jump is no path's.
  REQUIRE(t.forms.size() == 10);
  CHECK(t.forms[7].cls == probe::InsnClass::Nop);
  CHECK(t.forms[7].unreached);
  CHECK_FALSE(t.forms[8].unreached);
  CHECK(t.forms[8].cls == probe::InsnClass::Xor);
  CHECK(t.forms[9].cls == probe::InsnClass::Ret);
  CHECK(t.forms[2].operands[1].scale == 8);
  CHECK(t.forms[2].operands[1].index == std::optional<std::uint8_t>(2));

  // The shapes match run 7's reading.
  const std::vector<probe::InnerCallee> inner = {probe::InnerCallee{kTailRva, 70, true, t, 1}};
  const probe::Table m = probe::match_table(wrapper, inner, 2);
  CHECK(m.matched);
  CHECK(m.singleton_slot == kSingletonSlot);
  CHECK_FALSE(m.hop.has_value());
  CHECK_FALSE(m.array.has_value());
  CHECK(m.disp == 0);
  CHECK(m.bound == 168);
  CHECK(m.slot_disp == std::optional<std::int64_t>(0x18));
  CHECK(m.site == std::optional<std::uint32_t>(24));
  CHECK(m.rva == kTailRva);
  // Each shape that fails is named.
  CHECK(probe::match_table(wrapper, {}, 2).failed == "wrapper_tail");
  CHECK(probe::match_table(wrapper, inner, 8).failed == "tail_bound");  // the group is not in r8
  probe::CalleeCode untested = t;
  untested.guards.pop_back();
  const std::vector<probe::InnerCallee> plain = {probe::InnerCallee{kTailRva, 70, true, untested, 1}};
  CHECK(probe::match_table(wrapper, plain, 2).failed == "tail_null_test");
}

TEST_CASE("probe: a table one level deeper, a jump through a register, and loads that are not followed") {
  SUBCASE("the entries' table at [singleton + 0x20]") {
    const std::vector<std::byte> code = code_of({
        0x83, 0xFA, 0x1F,                    //  0: cmp edx, 31
        0x77, 0x13,                          //  3: ja 24
        0x48, 0x8B, 0x41, 0x20,              //  5: mov rax, [rcx + 0x20]
        0x48, 0x8B, 0x4C, 0xD0, 0x10,        //  9: mov rcx, [rax + rdx*8 + 0x10]
        0x48, 0x85, 0xC9,                    // 14: test rcx, rcx
        0x74, 0x05,                          // 17: je 24
        0x48, 0x8B, 0x01,                    // 19: mov rax, [rcx]
        0xFF, 0x20,                          // 22: jmp [rax]
        0x31, 0xC0,                          // 24: xor eax, eax
        0xC3,                                // 26: ret
    });
    probe::EntryState e{};
    e[1] = probe::EntryArg{2, false};
    const probe::CalleeCode t = probe::summarise_callee(code, kTailRva, 0, e);
    REQUIRE(t.guards.size() == 2);
    REQUIRE(t.guards[1].operands[0].source.has_value());
    const probe::Source& v = *t.guards[1].operands[0].source;
    REQUIRE(v.base_src.size() == 1);
    CHECK(v.base_src[0].kind == probe::Source::Kind::Memory);
    CHECK(v.base_src[0].disp == 0x20);
    CHECK(v.disp == 0x10);
    const std::vector<std::byte> wrapper_code = lazy_wrapper();
    const probe::CalleeCode w =
        probe::summarise_callee(wrapper_code, kWrapperRva, static_cast<std::uint32_t>(wrapper_code.size()));
    const std::vector<probe::InnerCallee> inner = {probe::InnerCallee{kTailRva, 70, true, t, 1}};
    const probe::Table m = probe::match_table(w, inner, 2);
    CHECK(m.matched);
    CHECK(m.array == std::optional<std::int64_t>(0x20));
    CHECK(m.disp == 0x10);
    CHECK(m.bound == 31);
    CHECK(m.slot_disp == std::optional<std::int64_t>(0));
  }
  SUBCASE("a function pointer loaded from a global, jumped to") {
    const std::vector<std::byte> code = code_of({
        0x48, 0x8B, 0x05, 0x00, 0x10, 0x00, 0x00,  // 0: mov rax, [rip + 0x1000]   (0x401007)
        0x48, 0xFF, 0xE0,                          // 7: jmp rax
    });
    const probe::CalleeCode c = probe::summarise_callee(code, kWrapperRva, static_cast<std::uint32_t>(code.size()));
    REQUIRE(c.calls.size() == 1);
    CHECK(c.calls[0].form == probe::CallForm::TailRegister);
    CHECK(c.calls[0].base == std::optional<std::uint8_t>(0));
    REQUIRE(c.calls[0].base_src.has_value());
    CHECK(c.calls[0].base_src->kind == probe::Source::Kind::Rip);
    CHECK(c.calls[0].base_src->target == 0x401007);
    CHECK(passes(c.calls[0], 1, 1));
    REQUIRE(c.forms.size() == 2);
    CHECK(c.forms[1].cls == probe::InsnClass::Jmp);
    REQUIRE(c.forms[1].operands.size() == 1);
    CHECK(c.forms[1].operands[0].reg == std::optional<std::uint8_t>(0));
  }
  SUBCASE("a jump through a RIP-relative slot, a 16-bit load, a segment's and three loads deep") {
    const std::vector<std::byte> code = code_of({
        0x66, 0x8B, 0x01,                          //  0: mov ax, [rcx]          (writes part of rax)
        0x65, 0x48, 0x8B, 0x14, 0x25, 0x58, 0x00, 0x00, 0x00,  //  3: mov rdx, gs:[0x58]
        0x4C, 0x8B, 0x41, 0x08,                    // 12: mov r8, [rcx+8]        (one)
        0x4D, 0x8B, 0x40, 0x10,                    // 16: mov r8, [r8+0x10]      (two)
        0x4D, 0x8B, 0x40, 0x18,                    // 20: mov r8, [r8+0x18]      (three)
        0x4D, 0x8B, 0x48, 0x20,                    // 24: mov r9, [r8+0x20]      (four: not followed)
        0xFF, 0x25, 0x00, 0x20, 0x00, 0x00,        // 28: jmp [rip + 0x2000]
    });
    const probe::CalleeCode c = probe::summarise_callee(code, kWrapperRva, static_cast<std::uint32_t>(code.size()));
    REQUIRE(c.calls.size() == 1);
    const probe::CalleeCall& j = c.calls[0];
    CHECK(j.form == probe::CallForm::TailMemory);
    CHECK(j.rip);
    CHECK(j.target == std::optional<std::uint32_t>(kWrapperRva + 34 + 0x2000));
    CHECK(holds_none(j, 1) == false);  // rcx, untouched
    CHECK(passed_source(j, 2) == nullptr);  // gs:[0x58] is not followed
    const probe::PassedSource* r8 = passed_source(j, 8);
    REQUIRE(r8 != nullptr);
    CHECK(r8->source.disp == 0x18);
    REQUIRE(r8->source.base_src.size() == 1);
    CHECK(r8->source.base_src[0].disp == 0x10);
    REQUIRE(r8->source.base_src[0].base_src.size() == 1);
    CHECK(r8->source.base_src[0].base_src[0].disp == 8);
    CHECK(r8->source.base_src[0].base_src[0].base_src[0].kind == probe::Source::Kind::Argument);
    CHECK(passed_source(j, 9) == nullptr);  // a fourth load deep holds nothing
    CHECK(holds_none(j, 9));
  }
}

TEST_CASE("probe: the table's reads: the singleton, the entries to the bound, the group's entry, its vtable and slot") {
  FakeMemory mem;
  constexpr std::uint64_t kGame = 0x0000'0001'4000'0000ull, kSingleton = 0x0000'0002'0000'0000ull;
  constexpr std::uint64_t kEntry = 0x0000'0002'0001'0000ull, kInstance = 0x0000'0002'0002'0000ull;
  mem.map(kGame, std::vector<std::byte>(0x490000));
  mem.map(kSingleton, std::vector<std::byte>(0x1000));
  mem.map(kEntry, std::vector<std::byte>(0x100));
  mem.put<std::uint64_t>(kGame + kSingletonSlot, kSingleton);
  mem.put<std::uint64_t>(kSingleton + 0x10 + 8 * 3, kEntry + 0x80);  // another group's
  mem.put<std::uint64_t>(kSingleton + 0x10 + 8 * 110, kEntry);
  mem.put<std::uint64_t>(kSingleton + 0x10 + 8 * 168, kEntry + 0x40);
  mem.put<std::uint64_t>(kSingleton + 0x10 + 8 * 169, kEntry + 0x40);  // past the bound: not read
  mem.put<std::uint64_t>(kEntry, kGame + 0x300000);                     // its vtable, in the game's rdata
  mem.put<std::uint64_t>(kGame + 0x300000 + 0x18, kGame + 0x1234);       // its slot, the game's code
  Regions rs;
  rs.list = {probe::MemoryRegion{kGame, 0x490000, kGame, probe::kPageReadOnly, true, probe::RegionKind::Image},
             probe::MemoryRegion{kSingleton, 0x1000, kSingleton, probe::kPageReadWrite, true, probe::RegionKind::Private},
             probe::MemoryRegion{kEntry, 0x1000, kEntry, probe::kPageReadWrite, true, probe::RegionKind::Private}};
  probe::Table shape;
  shape.matched = true;
  shape.singleton_slot = kSingletonSlot;
  shape.disp = 0x10;
  shape.bound = 168;
  shape.slot_disp = 0x18;
  probe::TableReadSpec spec;
  spec.module_base = kGame;
  spec.group_index = 110;
  spec.instance = kInstance;
  spec.leads = {{4, kInstance}, {7, kSingleton}};
  spec.kinds.game_base = kGame;
  spec.kinds.game_sections = {probe::SectionSpan{0x1000, 0x2FF000, probe::SectionKind::Code},
                              probe::SectionSpan{0x300000, 0x180000, probe::SectionKind::Rdata},
                              probe::SectionSpan{0x480000, 0x10000, probe::SectionKind::Data}};
  spec.kinds.query = rs.query();
  const probe::BlockReader read = [&](std::uint64_t at, std::span<std::byte> out) { return mem.read(at, out); };
  const probe::TableRead r = probe::read_table(shape, spec, read);
  CHECK(r.read);
  REQUIRE(r.singleton.has_value());
  CHECK(r.singleton->kind == probe::ValueKind::Heap);
  CHECK_FALSE(r.is_instance);
  CHECK(r.singleton_lead == std::optional<std::uint32_t>(7));
  CHECK(r.entries == 169);
  CHECK(r.nonzero == 3);
  CHECK(r.nonzero_at == std::vector<std::uint32_t>{3, 110, 168});
  REQUIRE(r.entry.has_value());
  CHECK(r.entry->kind == probe::ValueKind::Heap);
  REQUIRE(r.entry_vtable.has_value());
  CHECK(r.entry_vtable->kind == probe::ValueKind::Game);
  CHECK(r.entry_vtable->section == probe::SectionKind::Rdata);
  CHECK(r.entry_vtable->rva == std::optional<std::uint32_t>(0x300000));
  REQUIRE(r.slot_value.has_value());
  CHECK(r.slot_value->section == probe::SectionKind::Code);
  CHECK(r.slot_value->rva == std::optional<std::uint32_t>(0x1234));

  SUBCASE("no entry for the group") {
    mem.put<std::uint64_t>(kSingleton + 0x10 + 8 * 110, 0);
    const probe::TableRead g = probe::read_table(shape, spec, read);
    CHECK(g.read);
    CHECK(g.nonzero_at == std::vector<std::uint32_t>{3, 168});
    REQUIRE(g.entry.has_value());
    CHECK(g.entry->kind == probe::ValueKind::Zero);
    CHECK_FALSE(g.entry_vtable.has_value());
  }
  SUBCASE("a singleton not yet made, and a shape that did not match") {
    mem.put<std::uint64_t>(kGame + kSingletonSlot, 0);
    const probe::TableRead z = probe::read_table(shape, spec, read);
    CHECK_FALSE(z.read);
    REQUIRE(z.singleton.has_value());
    CHECK(z.singleton->kind == probe::ValueKind::Zero);
    CHECK(z.entries == 0);
    probe::Table no;
    CHECK_FALSE(probe::read_table(no, spec, read).singleton.has_value());
  }
  SUBCASE("through a hop and an array, the instance itself") {
    mem.put<std::uint64_t>(kGame + kSingletonSlot, kInstance - 0x40);
    mem.map(kInstance - 0x1000, std::vector<std::byte>(0x2000));
    mem.put<std::uint64_t>(kInstance - 0x40 + 0x40, kInstance);  // the hop: [*S + 0x40]
    mem.put<std::uint64_t>(kInstance + 0x20, kSingleton);         // the array: [singleton + 0x20]
    probe::Table deep = shape;
    deep.hop = 0x40;
    deep.array = 0x20;
    const probe::TableRead h = probe::read_table(deep, spec, read);
    CHECK(h.is_instance);
    CHECK(h.singleton_lead == std::optional<std::uint32_t>(4));
    CHECK(h.read);
    CHECK(h.nonzero == 3);
  }
}

TEST_CASE("probe: a control diff before the call marks the words that change on their own") {
  HubWorld w;
  const probe::HubWalk before = w.walk();
  // The walk 2 s before it: the same memory but the frame counter.
  probe::HubWalk control = before;
  std::uint64_t frame = 599;
  std::memcpy(control.bytes.data() + control.blocks[0].at + 8, &frame, 8);
  const probe::DiffSpec spec = w.diff_spec();
  const probe::DiffResult c = probe::diff_hub(control, before, spec);
  CHECK(c.diff.changed == 1);
  REQUIRE(c.changed.size() == 1);
  CHECK(c.changed[0].after_block == 0);
  CHECK(c.changed[0].offset == 8);
  // The call.
  w.mem.put<std::uint64_t>(HubWorld::kHub + 0x08, 601);
  w.mem.put<std::uint64_t>(HubWorld::kHub + 0x30, HubWorld::kTask);
  w.mem.put<std::uint64_t>(HubWorld::kB1 + 0x08, 5);
  const probe::HubWalk after = w.walk();
  probe::DiffSpec s = spec;
  s.control = &c.changed;
  const probe::DiffResult r = probe::diff_hub(before, after, s);
  CHECK(r.diff.changed == 3);
  CHECK(r.diff.in_control_words == std::optional<std::uint64_t>(1));
  REQUIRE(r.diff.words.size() == 3);
  CHECK(r.diff.words[0].path == std::vector<std::uint32_t>{0x30});  // not in the control: first
  CHECK(r.diff.words[0].in_control == std::optional<bool>(false));
  CHECK(r.diff.words[2].path == std::vector<std::uint32_t>{0x08});  // the frame: last
  CHECK(r.diff.words[2].in_control == std::optional<bool>(true));
  SUBCASE("and later, the steady words not in the control come first") {
    w.mem.put<std::uint64_t>(HubWorld::kHub + 0x08, 1700);
    w.mem.put<std::uint64_t>(HubWorld::kB1 + 0x08, 6);  // moving, not in the control
    probe::DiffSpec s2 = s;
    s2.earlier = &r.changed;
    const probe::HubDiff d2 = probe::diff_hub(before, w.walk(), s2).diff;
    REQUIRE(d2.words.size() == 3);
    CHECK(d2.words[0].path == std::vector<std::uint32_t>{0x30});
    CHECK(d2.words[0].steady == std::optional<bool>(true));
    CHECK(d2.words[1].path == std::vector<std::uint32_t>{0x18, 0x00, 0x08});
    CHECK(d2.words[1].in_control == std::optional<bool>(false));
    CHECK(d2.words[2].in_control == std::optional<bool>(true));
  }
  probe::Findings f;
  f.hub_diff.push_back(r.diff);
  f.hub_diff_control = c.diff;
  const auto text = probe::write_report(f);
  REQUIRE(text.has_value());
  const std::array<const report::Vocabulary*, 1> known = {&probe::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"in_control\": true") != std::string::npos);
  CHECK(text->find("\"hub_diff_control\": {") != std::string::npos);
}
