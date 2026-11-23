// GuardedReader's rules (ARCH §6.5), ObjView, the stack table (WHY§111), the breakers (ARCH §6.7, R8) and the C13
// histogram, over FakeSys. Every address and value is invented.
#include <doctest.h>

#include <cmath>
#include <limits>

#include "en/gal/guarded.hpp"
#include "en/gal/health.hpp"
#include "fake_sys.hpp"

using namespace en::gal;
using namespace en::gal::test;
using en::proto::GameThread;
using en::proto::Group;

namespace {

struct Rig {
  Rig() : sys(mem, 0x1'4000'0000ull, 0x1000), reader(sys) {
    obj = mem.alloc(0x100);
    for (std::uint32_t i = 0; i < 0x100; ++i) mem.put<std::uint8_t>(obj + i, static_cast<std::uint8_t>(i));
  }
  FakeMemory mem;
  FakeSys sys;
  GuardedReader reader;
  std::uint64_t obj = 0;
};

}  // namespace

TEST_CASE("guarded: the plausibility rules refuse an address before any copy") {
  StackRanges stacks;
  REQUIRE(stacks.add(AddressRange{0x7000'0000, 0x7010'0000}));
  CHECK(implausible(0, 8, 8, stacks) == FaultKind::Implausible);
  CHECK(implausible(0xFFFF, 8, 1, stacks) == FaultKind::Implausible);
  CHECK_FALSE(implausible(0x10000, 8, 8, stacks).has_value());
  CHECK(implausible(0x10004, 8, 8, stacks) == FaultKind::Implausible);  // unaligned for its type
  CHECK(implausible(kAddressEnd - 4, 8, 1, stacks) == FaultKind::Implausible);
  CHECK(implausible(std::numeric_limits<std::uint64_t>::max() - 2, 8, 1, stacks) == FaultKind::Implausible);
  CHECK(implausible(0x20000, kMaxCopyBytes + 1, 1, stacks) == FaultKind::Implausible);
  CHECK_FALSE(implausible(0x20000, kMaxCopyBytes, 1, stacks).has_value());
  CHECK(implausible(0x7000'0000, 8, 8, stacks) == FaultKind::Stack);
  CHECK(implausible(0x6FFF'FFFC, 8, 4, stacks) == FaultKind::Stack);  // a copy that runs into a stack
  CHECK_FALSE(implausible(0x7010'0000, 8, 8, stacks).has_value());
  CHECK(stacks.add(AddressRange{0x7000'0000, 0x7010'0000}));  // the same range twice is one entry
  CHECK(stacks.size() == 1);
  CHECK_FALSE(stacks.add(AddressRange{5, 5}));
}

TEST_CASE("guarded: copies arrive whole, faults are answers with the destination zeroed, stacks are never read") {
  Rig r;
  const GameThread gt = GameThread::issue();
  r.reader.begin_tick();
  const auto v = r.reader.copy<std::uint32_t>(gt, Group::World, r.obj + 4);
  REQUIRE(v.has_value());
  CHECK(*v == 0x07060504u);
  std::array<std::byte, 16> dst{};
  dst.fill(std::byte{0xEE});
  const auto bad = r.reader.copy_into(gt, Group::World, r.obj + 0xF8, dst);  // runs past the object: an access violation
  REQUIRE_FALSE(bad.has_value());
  CHECK(bad.error().kind == FaultKind::Copy);
  CHECK(bad.error().code == 0xC0000005u);
  CHECK(std::all_of(dst.begin(), dst.end(), [](std::byte b) { return b == std::byte{0}; }));
  // A released region faults; a stack is refused without a copy.
  r.mem.release(r.obj);
  CHECK(r.reader.copy<std::uint8_t>(gt, Group::World, r.obj).error().kind == FaultKind::Copy);
  const std::uint64_t stack = r.mem.alloc(0x1000);
  r.sys.own_stack = AddressRange{stack, stack + 0x1000};
  r.sys.note_own_stack();
  const std::uint64_t copies = static_cast<FakeCopier&>(r.sys.copier()).copies;
  CHECK(r.reader.copy<std::uint64_t>(gt, Group::World, stack + 0x10).error().kind == FaultKind::Stack);
  CHECK(static_cast<FakeCopier&>(r.sys.copier()).copies == copies);
  CHECK(r.reader.faults(FaultKind::Stack) == 1);
  CHECK(r.reader.faults(FaultKind::Copy) == 2);
}

TEST_CASE("guarded: a stack range of a thread that ended is forgotten when a read meets it, and the read goes on") {
  Rig r;
  const GameThread gt = GameThread::issue();
  r.reader.begin_tick();
  const std::uint64_t old_stack = r.mem.alloc(0x1000);  // a stack once; now the game's object (WHY§190)
  r.mem.put<std::uint64_t>(old_stack + 0x10, 0x1122334455667788ull);
  r.sys.own_stack = AddressRange{old_stack, old_stack + 0x1000};
  r.sys.note_own_stack();
  CHECK(r.reader.copy<std::uint64_t>(gt, Group::World, old_stack + 0x10).error().kind == FaultKind::Stack);
  CHECK(r.reader.stale_stacks() == 0);
  r.sys.stacks_stale = true;  // the SysLayer finds the range no longer a stack
  const auto v = r.reader.copy<std::uint64_t>(gt, Group::World, old_stack + 0x10);
  REQUIRE(v.has_value());
  CHECK(*v == 0x1122334455667788ull);
  CHECK(r.reader.stale_stacks() == 1);
  CHECK(r.sys.stacks().size() == 0);
  CHECK(r.reader.faults(FaultKind::Stack) == 1);
  // StackRanges::remove takes out every range that overlaps, and keeps the rest in order.
  StackRanges t;
  REQUIRE(t.add(AddressRange{0x10000, 0x20000}));
  REQUIRE(t.add(AddressRange{0x30000, 0x40000}));
  REQUIRE(t.add(AddressRange{0x50000, 0x60000}));
  t.remove(AddressRange{0x3F000, 0x51000});
  REQUIRE(t.size() == 1);
  CHECK(t.at(0).begin == 0x10000);
  t.clear();
  CHECK(t.size() == 0);
}

TEST_CASE("guarded: the per-tick budget of copies and bytes, and a read off the bound thread") {
  Rig r;
  const GameThread gt = GameThread::issue();
  GuardedReader small(r.sys, ReadBudget{4, 64});
  small.begin_tick();
  for (int i = 0; i < 4; ++i) CHECK(small.copy<std::uint64_t>(gt, Group::World, r.obj).has_value());
  CHECK(small.copy<std::uint64_t>(gt, Group::World, r.obj).error().kind == FaultKind::Budget);
  CHECK(small.spent());
  CHECK(small.copy<std::uint8_t>(gt, Group::World, r.obj).error().kind == FaultKind::Budget);  // spent stays spent
  small.begin_tick();
  CHECK_FALSE(small.spent());
  std::array<std::byte, 48> big{};
  CHECK(small.copy_into(gt, Group::World, r.obj, big).has_value());
  CHECK(small.copy_into(gt, Group::World, r.obj, big).error().kind == FaultKind::Budget);  // 96 bytes > 64
  // Bound to the first tick's thread: a read from another is refused (debug and probe builds).
  r.reader.begin_tick();
  r.reader.bind_thread();
  r.sys.set_thread(2);
  CHECK(r.reader.copy<std::uint8_t>(gt, Group::World, r.obj).error().kind == FaultKind::WrongThread);
  r.sys.set_thread(1);
  CHECK(r.reader.copy<std::uint8_t>(gt, Group::World, r.obj).has_value());
}

TEST_CASE("guarded: the probe's fault injection makes every n-th read of a group a real fault") {
  Rig r;
  const GameThread gt = GameThread::issue();
  r.reader.begin_tick();
  r.reader.inject(Injection{Group::Camera, 3, 0x0000'0123'0000'0000ull});  // nothing is mapped there
  int faults = 0;
  for (int i = 0; i < 9; ++i) faults += r.reader.copy<std::uint32_t>(gt, Group::Camera, r.obj).has_value() ? 0 : 1;
  CHECK(faults == 3);
  CHECK(r.reader.faults(FaultKind::Copy) == 3);
  for (int i = 0; i < 9; ++i) CHECK(r.reader.copy<std::uint32_t>(gt, Group::World, r.obj).has_value());  // another group
  r.reader.inject(Injection{Group::World, 1, 0});
  CHECK(r.reader.copy<std::uint32_t>(gt, Group::World, r.obj).error().kind == FaultKind::Injected);
}

TEST_CASE("guarded: ObjView reads each type by its layout, bounds-checked") {
  std::array<std::byte, 32> b{};
  const auto put = [&](std::size_t at, std::uint64_t v, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) b[at + i] = static_cast<std::byte>(v >> (8 * i));
  };
  put(0, 0xFFFF'FFFEu, 4);  // i32 -2, or u32 4294967294
  put(4, 0x1234, 2);
  put(8, 0xA5, 1);
  const float f[3] = {1.5f, -2.0f, 3.25f};
  std::memcpy(b.data() + 12, f, sizeof f);
  put(24, 0x41, 2);  // "A", then NUL
  const ObjView v(b);
  const auto field = [](std::uint32_t off, FieldType type, std::uint32_t size) {
    FieldLayout f;
    f.off = off;
    f.type = type;
    f.size = size;
    return f;
  };
  const FieldLayout i32 = field(0, FieldType::I32, 4);
  const FieldLayout u32 = field(0, FieldType::U32, 4);
  const FieldLayout u16 = field(4, FieldType::U16, 2);
  FieldLayout bit = field(8, FieldType::Bit, 1);
  bit.bit = 5;
  const FieldLayout vec = field(12, FieldType::Vec3f, 12);
  FieldLayout text = field(24, FieldType::Utf16, 8);
  text.len = 4;
  const FieldLayout past = field(30, FieldType::U32, 4);
  CHECK(v.integer(&i32) == -2);
  CHECK(v.integer(&u32) == 0xFFFF'FFFEll);
  CHECK(v.integer(&u16) == 0x1234);
  CHECK(v.bit(&bit) == true);
  FieldLayout run = field(0, FieldType::Bits, 4);  // the word 0xFFFFFFFE: bits 1 to 31 set
  run.shift = 0;
  run.width = 3;
  CHECK(v.integer(&run) == 6);
  run.shift = 29;
  CHECK(v.integer(&run) == 7);
  FieldLayout wide = field(4, FieldType::Bits, 8);  // 0x1234 at 4, 0xA5 at 8: the word 0x000000A5'00001234
  wide.shift = 32;
  wide.width = 8;
  CHECK(v.integer(&wide) == 0xA5);
  wide.shift = 4;
  wide.width = 32;
  CHECK(v.integer(&wide) == 0x5000'0123ll);
  wide.width = 0;
  CHECK_FALSE(v.integer(&wide).has_value());
  FieldLayout off_end = field(28, FieldType::Bits, 8);
  off_end.width = 4;
  CHECK_FALSE(v.integer(&off_end).has_value());
  CHECK_FALSE(v.bit(&run).has_value());
  bit.bit = 1;
  CHECK(v.bit(&bit) == false);
  CHECK(v.f32(&vec, 1) == -2.0f);
  CHECK_FALSE(v.f32(&vec, 3).has_value());
  std::array<char16_t, 8> units{};
  CHECK(v.utf16(&text, units) == 1);
  CHECK(units[0] == u'A');
  CHECK_FALSE(v.integer(&past).has_value());
  CHECK_FALSE(v.integer(nullptr).has_value());
  FieldLayout ranged = u16;
  ranged.range = std::pair<std::int64_t, std::int64_t>{0, 9};
  CHECK_FALSE(ObjView::in_range(&ranged, 10));
  CHECK(ObjView::in_range(&ranged, 9));
  CHECK(ObjView::in_range(&u16, 99999));
}

TEST_CASE("breakers: three hard faults within 60 s trip a group, for the session") {
  Breaker b;
  CHECK_FALSE(b.fault(1'000));
  CHECK_FALSE(b.fault(30'000));
  CHECK(b.fault(60'999));  // the third within 60 s of the first
  CHECK(b.tripped());
  CHECK_FALSE(b.fault(70'000));  // tripped once; no half-open state
  CHECK(b.tripped());
  Breaker spread;
  for (std::uint64_t t = 0; t < 10; ++t) CHECK_FALSE(spread.fault(t * 31'000));  // never three in one window
  CHECK_FALSE(spread.tripped());
  CHECK(spread.total() == 10);

  GroupBook book;
  CHECK(book.state(Group::Camera) == en::proto::GroupState::Untested);
  CHECK_FALSE(book.hard_fault(Group::Camera, 0));
  CHECK_FALSE(book.hard_fault(Group::Camera, 1));
  CHECK(book.hard_fault(Group::Camera, 2));
  CHECK(book.state(Group::Camera) == en::proto::GroupState::Tripped);
  CHECK(book.why(Group::Camera) == GroupWhy::Breaker);
  CHECK_FALSE(book.live(Group::Camera));
  en::proto::ReadHealth h;
  book.fill(h);
  CHECK(h.of(Group::Camera).faults == 3);
  book.begin_tick();
  book.fill(h);
  CHECK(h.of(Group::Camera).faults == 0);
  CHECK(h.of(Group::Camera).state == en::proto::GroupState::Tripped);
}

TEST_CASE("the C13 histogram: four buckets an octave, percentiles as bucket bounds") {
  std::size_t last = 0;
  for (std::uint64_t us = 0; us < 200'000; us += 7) {
    const std::size_t b = Histogram::bucket_of(us);
    CHECK(b >= last);
    CHECK(Histogram::upper_of(b) >= us);
    last = b;
  }
  CHECK(Histogram::bucket_of(3) == 3);
  CHECK(Histogram::bucket_of(4) == 4);
  CHECK(Histogram::bucket_of(1u << 30) == Histogram::kBuckets - 1);
  Histogram h;
  for (int i = 0; i < 990; ++i) h.record(100);
  for (int i = 0; i < 10; ++i) h.record(5'000);
  CHECK(h.count() == 1000);
  CHECK(h.percentile_us(500) >= 100);
  CHECK(h.percentile_us(500) < 128);
  CHECK(h.percentile_us(990) < 128);
  CHECK(h.percentile_us(999) >= 5'000);
  CHECK(h.max_us() == 5'000);
}
