// R1 spike.11 (WHY§293): a function decoded in parts (its chained entries and unlisted code past its extent), the
// request a method of G's builds on its stack and the function it calls with it, the request's fields followed into
// that function, the whole-memory search for the request's vtable, and the report of all of it. Every byte here is our
// own code, assembled by hand; nothing is a game's.
#include <doctest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <vector>

#include "en/gal/image_bytes.hpp"
#include "en/gal/probe.hpp"
#include "en/gal/report.hpp"
#include "fake_image.hpp"
#include "rig_gal.hpp"

using namespace en::gal;
using namespace en::gal::test;

namespace {

constexpr std::uint32_t kMethod = 0x510000, kNow = 0x510200, kAlloc = 0x510400, kPush = 0x510500, kKeep = 0x510600;
constexpr std::uint32_t kVtable = 0x5A0000;
constexpr std::uint8_t kTask = 1;  // the wrapper's task argument: rcx

std::vector<std::byte> bytes_of(std::initializer_list<int> bytes) {
  std::vector<std::byte> out;
  for (const int b : bytes) out.push_back(std::byte{static_cast<std::uint8_t>(b)});
  return out;
}

// A rel32 at `at` in `code`, of an instruction ending at `next`, to `target` (all RVAs from `rva`).
void rel32(std::vector<std::byte>& code, std::size_t at, std::uint32_t rva, std::size_t next, std::uint32_t target) {
  const auto d = static_cast<std::int32_t>(static_cast<std::int64_t>(target) - static_cast<std::int64_t>(rva + next));
  std::memcpy(code.data() + at, &d, 4);
}

// A method of G's of R1 run 10's shape, of our own (141 bytes; G in rcx, the task in rdx, the entry's word in r8d, r9
// zero): a null task or a zero at G+0x30 answers 0; else a request of 32 bytes at [rsp + 0x20] (our vtable, the task,
// G's field at +0x38, the word, r9's low byte); where G's byte at +0x28 is zero, a call with the request's address in
// rcx; else 32 bytes allocated, the request copied there, and G's field at +0x10 called with the copy in rdx.
std::vector<std::byte> method_of_run10() {
  std::vector<std::byte> c = bytes_of({
      0x53,                          //   0: push rbx
      0x57,                          //   1: push rdi
      0x48, 0x83, 0xEC, 0x48,        //   2: sub rsp, 0x48
      0x48, 0x89, 0xCB,              //   6: mov rbx, rcx                 (G)
      0x48, 0x85, 0xD2,              //   9: test rdx, rdx
      0x74, 0x76,                    //  12: je 132
      0x48, 0x83, 0x7B, 0x30, 0x00,  //  14: cmp qword ptr [rbx + 0x30], 0
      0x74, 0x6F,                    //  19: je 132
      0x48, 0x8D, 0x3D, 0, 0, 0, 0,  //  21: lea rdi, [rip + the vtable]
      0x48, 0x89, 0x7C, 0x24, 0x20,  //  28: mov [rsp + 0x20], rdi
      0x48, 0x8B, 0x43, 0x38,        //  33: mov rax, [rbx + 0x38]        (G's manager)
      0x48, 0x89, 0x54, 0x24, 0x28,  //  37: mov [rsp + 0x28], rdx        (the task)
      0x48, 0x89, 0x44, 0x24, 0x30,  //  42: mov [rsp + 0x30], rax
      0x44, 0x89, 0x44, 0x24, 0x38,  //  47: mov [rsp + 0x38], r8d        (the word)
      0x44, 0x88, 0x4C, 0x24, 0x3C,  //  52: mov [rsp + 0x3C], r9b        (zero)
      0x80, 0x7B, 0x28, 0x00,        //  57: cmp byte ptr [rbx + 0x28], 0
      0x75, 0x0C,                    //  61: jne 75
      0x48, 0x8D, 0x4C, 0x24, 0x20,  //  63: lea rcx, [rsp + 0x20]
      0xE8, 0, 0, 0, 0,              //  68: call now
      0xEB, 0x3B,                    //  73: jmp 134
      0xB9, 0x20, 0x00, 0x00, 0x00,  //  75: mov ecx, 0x20
      0xE8, 0, 0, 0, 0,              //  80: call the allocator
      0x48, 0x85, 0xC0,              //  85: test rax, rax
      0x74, 0x2C,                    //  88: je 134
      0x48, 0x89, 0x38,              //  90: mov [rax], rdi
      0x48, 0x8B, 0x4C, 0x24, 0x28,  //  93: mov rcx, [rsp + 0x28]
      0x48, 0x89, 0x48, 0x08,        //  98: mov [rax + 8], rcx
      0x48, 0x8B, 0x4C, 0x24, 0x30,  // 102: mov rcx, [rsp + 0x30]
      0x48, 0x89, 0x48, 0x10,        // 107: mov [rax + 0x10], rcx
      0x8B, 0x4C, 0x24, 0x38,        // 111: mov ecx, [rsp + 0x38]
      0x89, 0x48, 0x18,              // 115: mov [rax + 0x18], ecx
      0x48, 0x89, 0xC2,              // 118: mov rdx, rax
      0x48, 0x8B, 0x4B, 0x10,        // 121: mov rcx, [rbx + 0x10]        (G's queue)
      0xE8, 0, 0, 0, 0,              // 125: call the queue
      0xEB, 0x02,                    // 130: jmp 134
      0x31, 0xC0,                    // 132: xor eax, eax
      0x48, 0x83, 0xC4, 0x48,        // 134: add rsp, 0x48
      0x5F,                          // 138: pop rdi
      0x5B,                          // 139: pop rbx
      0xC3,                          // 140: ret
  });
  rel32(c, 24, kMethod, 28, kVtable);
  rel32(c, 69, kMethod, 73, kNow);
  rel32(c, 81, kMethod, 85, kAlloc);
  rel32(c, 126, kMethod, 130, kPush);
  return c;
}

// The function the method calls with the request (our own; the request in rcx), in two parts: its primary entry (45
// bytes) reads the task and compares the request's word in place; a zero word branches past the extent to the second
// part (at +64, after padding), which reads the task's kind and the manager's flag and either branches back into the
// primary (to keep the task) or answers 1 and jumps back to the primary's epilogue; a trap ends it.
std::vector<std::byte> now_in_parts() {
  std::vector<std::byte> c = bytes_of({
      0x53,                          //  0: push rbx
      0x48, 0x83, 0xEC, 0x20,        //  1: sub rsp, 0x20
      0x48, 0x89, 0xCB,              //  5: mov rbx, rcx                  (the request)
      0x48, 0x8B, 0x41, 0x08,        //  8: mov rax, [rcx + 8]            (the task)
      0x83, 0x79, 0x18, 0x00,        // 12: cmp dword ptr [rcx + 0x18], 0 (the word, in place)
      0x0F, 0x84, 0, 0, 0, 0,        // 16: je 64                         (past the extent)
      0x48, 0x89, 0xC1,              // 22: mov rcx, rax                  (the task)
      0x48, 0x8B, 0x53, 0x10,        // 25: mov rdx, [rbx + 0x10]         (the manager)
      0xE8, 0, 0, 0, 0,              // 29: call keep
      0xB8, 0x01, 0x00, 0x00, 0x00,  // 34: mov eax, 1
      0x48, 0x83, 0xC4, 0x20,        // 39: add rsp, 0x20
      0x5B,                          // 43: pop rbx
      0xC3,                          // 44: ret
  });
  while (c.size() < 64) c.push_back(std::byte{0xCC});
  const std::vector<std::byte> second = bytes_of({
      0x8B, 0x50, 0x20,                    // 64: mov edx, [rax + 0x20]  (the task's kind)
      0x81, 0xFA, 0x51, 0x00, 0x00, 0x00,  // 67: cmp edx, 0x51
      0x74, 0xCB,                          // 73: je 22                  (back into the primary)
      0x48, 0x8B, 0x4B, 0x10,              // 75: mov rcx, [rbx + 0x10]  (the manager)
      0xF6, 0x41, 0x04, 0x01,              // 79: test byte ptr [rcx + 4], 1
      0x74, 0xC1,                          // 83: je 22
      0xB8, 0x01, 0x00, 0x00, 0x00,        // 85: mov eax, 1
      0xE9, 0, 0, 0, 0,                    // 90: jmp 39                 (the primary's epilogue)
      0xCC,                                // 95: int3
      0x48, 0x31, 0xC0,                    // 96: xor rax, rax           (the next function's, never decoded)
      0xC3,                                // 99: ret
  });
  c.insert(c.end(), second.begin(), second.end());
  rel32(c, 18, kNow, 22, kNow + 64);
  rel32(c, 30, kNow, 34, kKeep);
  rel32(c, 91, kNow, 95, kNow + 39);
  return c;
}

probe::EntryState method_entry() {
  probe::EntryState e{};
  e[0].g = true;        // rcx: G
  e[1].from = kTask;    // rdx: the task
  e[2].word = true;     // r8: the entry's word
  e[3].zero = true;     // r9: zero, as the stub left it
  return e;
}

const std::array<probe::SectionSpan, 2> kSections = {{{0x500000, 0x80000, probe::SectionKind::Code},
                                                       {0x580000, 0x40000, probe::SectionKind::Rdata}}};

const probe::FieldRead* read_at(const std::vector<probe::FieldRead>& reads, std::int64_t disp, std::uint32_t offset) {
  for (const probe::FieldRead& r : reads) {
    if (r.disp == disp && r.offset == offset) return &r;
  }
  return nullptr;
}

bool used_as(const probe::FieldRead& r, probe::UseKind k) {
  return std::any_of(r.uses.begin(), r.uses.end(), [&](const probe::FieldUse& u) { return u.kind == k; });
}

}  // namespace

TEST_CASE("probe: the request a method of G's builds on its stack, its fields, and the function it calls with it") {
  const std::vector<std::byte> code = method_of_run10();
  const probe::EntryState e = method_entry();
  const probe::CalleeCode m =
      probe::summarise_callee(code, kMethod, static_cast<std::uint32_t>(code.size()), e, kTask);
  CHECK_FALSE(m.modelled_to.has_value());
  CHECK(m.task_read_count == 0);  // the task is stored, never read
  // The lea of our vtable and of the request's place are addresses the code makes, passed as such.
  REQUIRE(m.calls.size() == 3);
  const probe::CalleeCall& now = m.calls[0];
  CHECK(now.target == std::optional<std::uint32_t>(kNow));
  const auto rcx = std::find_if(now.pass_src.begin(), now.pass_src.end(), [](const auto& p) { return p.reg == 1; });
  REQUIRE(rcx != now.pass_src.end());
  CHECK(rcx->source.kind == probe::Source::Kind::Stack);
  CHECK(rcx->source.disp == 0x20);
  // G's fields: the open word and the deferred byte compared in place, the manager stored, the queue passed in rcx.
  CHECK(m.g_read_count == 4);
  const probe::FieldRead* open = read_at(m.g_reads, 0x30, 14);
  const probe::FieldRead* deferred = read_at(m.g_reads, 0x28, 57);
  const probe::FieldRead* manager = read_at(m.g_reads, 0x38, 33);
  const probe::FieldRead* queue = read_at(m.g_reads, 0x10, 121);
  REQUIRE(open != nullptr);
  REQUIRE(deferred != nullptr);
  REQUIRE(manager != nullptr);
  REQUIRE(queue != nullptr);
  CHECK(open->direct);
  CHECK(open->width == 8);
  CHECK(deferred->direct);
  CHECK(deferred->width == 1);
  CHECK(used_as(*deferred, probe::UseKind::Compared));
  CHECK(used_as(*manager, probe::UseKind::Stored));
  REQUIRE(!queue->uses.empty());
  CHECK(queue->uses[0].kind == probe::UseKind::Passed);
  CHECK(queue->uses[0].reg == std::optional<std::uint8_t>(1));
  CHECK(queue->uses[0].target == std::optional<std::uint32_t>(kPush));

  // The request: its vtable in rdata at [rsp + 0x20], the task at +8, G's field, the word and the zeroed byte.
  const std::optional<probe::RequestShape> r = probe::request_shape(m, e, kTask, kSections);
  REQUIRE(r.has_value());
  CHECK(r->vtable == kVtable);
  CHECK(r->disp == 0x20);
  CHECK(r->site == 28);
  CHECK(r->task_field == 8);
  CHECK(r->now == std::optional<std::uint32_t>(kNow));
  REQUIRE(r->fields.size() == 5);
  const std::array<probe::FieldRole, 5> roles = {probe::FieldRole::Vtable, probe::FieldRole::Task,
                                                 probe::FieldRole::Manager, probe::FieldRole::Word,
                                                 probe::FieldRole::Zero};
  const std::array<std::uint32_t, 5> offsets = {0, 8, 16, 24, 28};
  const std::array<std::uint8_t, 5> widths = {8, 8, 8, 4, 1};
  for (std::size_t i = 0; i < roles.size(); ++i) {
    CHECK(r->fields[i].role == roles[i]);
    CHECK(r->fields[i].offset == offsets[i]);
    CHECK(r->fields[i].width == widths[i]);
  }
  CHECK(r->fields[0].source.kind == probe::Source::Kind::Address);
  CHECK(r->fields[0].source.target == kVtable);
  CHECK(r->fields[2].source.kind == probe::Source::Kind::Memory);
  CHECK(r->fields[2].source.disp == 0x38);
  CHECK(r->field(probe::FieldRole::Manager) == std::optional<std::uint32_t>(16));
  // A vtable outside rdata, or no task at +8, is no request.
  const std::array<probe::SectionSpan, 1> code_only = {{{0x500000, 0x100000, probe::SectionKind::Code}}};
  CHECK_FALSE(probe::request_shape(m, e, kTask, code_only).has_value());
  CHECK_FALSE(probe::request_shape(m, e, 2, kSections).has_value());

  // Into the call: rcx holds the request; into the queue's call, nothing of it (a copy on the heap).
  const probe::EntryState into_now = probe::request_entry_at(now, e, *r, kTask);
  CHECK(into_now[0].request);
  CHECK_FALSE(into_now[0].g);
  const probe::EntryState into_queue = probe::request_entry_at(m.calls[2], e, *r, kTask);
  CHECK_FALSE(into_queue[0].request);
  CHECK_FALSE(into_queue[1].request);

  // The function it calls, entered with the request: in two parts, its fields followed.
  const std::vector<std::byte> nc = now_in_parts();
  probe::SummaryOptions options;
  options.request = &*r;
  const std::array<RuntimeFunction, 1> one = {{{kNow, kNow + 45, 0x5C0000}}};
  options.functions = one;
  const probe::CalleeCode n = probe::summarise_callee(nc, kNow, 45, into_now, kTask, options);
  REQUIRE(n.parts.size() == 2);
  CHECK(n.parts[0].how == probe::PartHow::Primary);
  CHECK(n.parts[0].bytes == 45);
  CHECK(n.parts[1].how == probe::PartHow::Unlisted);
  CHECK(n.parts[1].rva == kNow + 64);
  CHECK(n.parts[1].bytes == 32);  // up to the trap, past the jump back into the primary
  CHECK(n.bytes == 77);
  CHECK(n.returns_early);          // through the second part, which answers 1 without a call
  REQUIRE(n.calls.size() == 1);    // the jump back into the primary is no tail jump
  CHECK(n.calls[0].target == std::optional<std::uint32_t>(kKeep));
  // The request's reads: the task, the word in place, the manager twice.
  CHECK(n.request_read_count == 4);
  const probe::FieldRead* task = read_at(n.request_reads, 8, 8);
  const probe::FieldRead* word = read_at(n.request_reads, 0x18, 12);
  REQUIRE(task != nullptr);
  REQUIRE(word != nullptr);
  CHECK(word->direct);
  CHECK(used_as(*word, probe::UseKind::Compared));
  CHECK(read_at(n.request_reads, 0x10, 25) != nullptr);
  CHECK(read_at(n.request_reads, 0x10, 75) != nullptr);
  // The task's kind, read in the second part from the task the request held; the manager's flag tested in place.
  REQUIRE(n.task_read_count == 1);
  CHECK(n.task_reads[0].disp == 0x20);
  CHECK(n.task_reads[0].offset == 64);
  CHECK(n.task_reads[0].base_src.kind == probe::Source::Kind::Memory);
  CHECK(used_as(n.task_reads[0], probe::UseKind::Compared));
  REQUIRE(n.manager_read_count == 1);
  CHECK(n.manager_reads[0].disp == 4);
  CHECK(n.manager_reads[0].direct);
  CHECK(used_as(n.manager_reads[0], probe::UseKind::Tested));
  // One level further: the call passes the task in rcx and the manager in rdx.
  const probe::EntryState into_keep = probe::request_entry_at(n.calls[0], into_now, *r, kTask);
  CHECK(into_keep[0].from == kTask);
  CHECK(into_keep[1].manager);
  CHECK(n.forms.size() == n.instructions);

  SUBCASE("without the directory, only the extent is decoded, and the branch past it is followed nowhere") {
    const probe::CalleeCode plain = probe::summarise_callee(nc, kNow, 45, into_now, kTask);
    CHECK(plain.parts.empty());
    CHECK(plain.bytes == 45);
    CHECK_FALSE(plain.returns_early);
    CHECK(plain.task_read_count == 0);
  }
  SUBCASE("a chained entry is the second part, decoded within its own extent") {
    const std::array<RuntimeFunction, 2> two = {{{kNow, kNow + 45, 0x5C0000}, {kNow + 64, kNow + 96, 0x5C0010}}};
    const std::array<probe::ChainedEntry, 1> chained = {{{kNow + 64, kNow + 96, kNow}}};
    probe::SummaryOptions o = options;
    o.functions = two;
    o.chained = chained;
    const probe::CalleeCode ch = probe::summarise_callee(nc, kNow, 45, into_now, kTask, o);
    REQUIRE(ch.parts.size() == 2);
    CHECK(ch.parts[1].how == probe::PartHow::Chained);
    CHECK(ch.parts[1].rva == kNow + 64);
    CHECK(ch.parts[1].bytes == 32);
    CHECK(ch.task_read_count == 1);
    // An entry of the directory chained to no one is another function: no part.
    o.chained = {};
    const probe::CalleeCode other = probe::summarise_callee(nc, kNow, 45, into_now, kTask, o);
    CHECK(other.parts.empty());
    CHECK(other.bytes == 45);
  }
  SUBCASE("an unlisted part ends at a direct jump out of the parts, a tail jump") {
    std::vector<std::byte> out = nc;
    rel32(out, 91, kNow, 95, 0x530000);  // the jump at 90 now leaves for a function elsewhere
    const probe::CalleeCode t = probe::summarise_callee(out, kNow, 45, into_now, kTask, options);
    REQUIRE(t.parts.size() == 2);
    CHECK(t.parts[1].bytes == 31);  // up to and with the jump
    REQUIRE(t.calls.size() == 2);
    CHECK(t.calls[1].form == probe::CallForm::Tail);
    CHECK(t.calls[1].target == std::optional<std::uint32_t>(0x530000));
  }
}

TEST_CASE("probe: the exception directory's chained entries, each with the primary its chain leads back to") {
  FakeImageSpec spec;
  spec.sections.push_back(FakeSection{".text", kCode, std::vector<std::uint8_t>(0x2000, 0xCC), 0});
  spec.sections.push_back(FakeSection{".rdata", kReadOnly, std::vector<std::uint8_t>(0x100, 0), 0});
  // A primary, a part chained to it, a part chained to that part (back to the same primary), another primary, and an
  // entry that points at the entry before it.
  spec.functions = {{0x10, 0x80, false, false},
                    {0x100, 0x140, true, false},
                    {0x200, 0x240, true, false},
                    {0x400, 0x500, false, false},
                    {0x600, 0x640, false, true}};
  const BuiltImage built = build(spec);
  const SpanSource source(built.memory);
  const auto image = read_image(source, ImageLayout::Memory);
  REQUIRE(image.has_value());
  const ImageBytes img(*image, source);
  const std::vector<probe::ChainedEntry> chained = probe::chained_entries(img, image->functions);
  const std::uint32_t text = built.rva.at(".text");
  REQUIRE(chained.size() == 3);
  CHECK(chained[0].begin == text + 0x100);
  CHECK(chained[0].primary == text + 0x10);
  CHECK(chained[1].begin == text + 0x200);
  CHECK(chained[1].primary == text + 0x10);  // through the part before it
  CHECK(chained[2].begin == text + 0x600);
  CHECK(chained[2].primary == text + 0x400);
  CHECK(chained[2].end == text + 0x640);
}

TEST_CASE("probe: the whole-memory search counts the request's vtable, the game's with their addresses, ours apart") {
  FakeMemory mem;
  const std::uint64_t game = 0x0000'0001'4000'0000ull, heap = 0x0000'0003'0000'0000ull;
  const std::uint64_t own_heap = 0x0000'7FF9'0000'0000ull, task = 0x0000'0002'0000'0000ull;
  FakeSys sys(mem, game, 0x3000);
  std::vector<probe::MemoryRegion> regions = {
      {heap, 0x2000, heap, probe::kPageReadWrite, true, probe::RegionKind::Private},
      {task, 0x1000, task, probe::kPageReadWrite, true, probe::RegionKind::Private},
      {own_heap, 0x1000, own_heap, probe::kPageReadWrite, true, probe::RegionKind::Private}};
  for (const auto& r : regions) mem.map(r.base, std::vector<std::byte>(r.size));
  const std::uint64_t vtable = game + kVtable;
  mem.put<std::uint64_t>(heap + 0x100, vtable);   // a request of the game's
  mem.put<std::uint64_t>(heap + 0x108, 0x0000'0003'0000'1000ull);
  mem.put<std::uint64_t>(heap + 0x1F00, vtable);  // another
  mem.put<std::uint64_t>(own_heap + 0x40, vtable);  // a copy in our walk's bytes
  probe::MemorySearchSpec spec;
  spec.target = task;
  spec.object_bytes = 0x100;
  spec.game_base = game;
  spec.own_bases = {own_heap, task};
  spec.own_known = true;
  spec.request_vtable = vtable;
  const probe::RegionQuery query = [&](std::uint64_t at) -> std::optional<probe::MemoryRegion> {
    for (const auto& r : regions) {
      if (r.base + r.size > at) return r;
    }
    return std::nullopt;
  };
  probe::MemorySearch m = probe::search_memory(sys, query, spec);
  CHECK(m.request_searched);
  CHECK(m.request_found == 2);
  CHECK(m.own_request_found == 1);
  REQUIRE(m.request_hits.size() == 2);
  REQUIRE(m.request_at.size() == 2);
  CHECK(m.request_at[0] == heap + 0x100);
  CHECK(m.request_at[1] == heap + 0x1F00);
  CHECK(m.request_hits[0].region == probe::RegionKind::Private);
  spec.request_vtable = 0;
  CHECK_FALSE(probe::search_memory(sys, query, spec).request_searched);

  // The report: the request, its slots, the parts, G's fields and path, the queue, the requests found with their
  // fields and tasks, the classes and the words' halves; lint-clean.
  probe::Findings f;
  f.callee_code = probe::CalleeCode{};
  probe::InnerCallee in;
  in.rva = kNow;
  in.depth = 4;
  in.request_slot = 0;
  in.entry_src.push_back(probe::PassedSource{1, probe::Source{}, false, false, false, true, false});
  in.code.parts = {{kNow, 45, probe::PartHow::Primary}, {kNow + 64, 32, probe::PartHow::Unlisted}};
  in.code.request_read_count = 1;
  in.code.request_reads.push_back(probe::FieldRead{});
  in.code.manager_read_count = 1;
  in.code.manager_reads.push_back(probe::FieldRead{});
  f.callee_inner.push_back(in);
  const std::vector<std::byte> code = method_of_run10();
  const probe::CalleeCode mc =
      probe::summarise_callee(code, kMethod, static_cast<std::uint32_t>(code.size()), method_entry(), kTask);
  probe::RequestShape shape = *probe::request_shape(mc, method_entry(), kTask, kSections);
  shape.cls = probe::ClassMatch{true, true, 0x5B0000, probe::LeadMatch::None, std::nullopt};
  shape.slot_count = 3;
  shape.slots = {kNow, 0x510700, 0x510800};
  f.request = shape;
  probe::Method method;
  method.rva = kMethod;
  probe::GFieldsRead gr;
  gr.read = true;
  probe::GField narrow;
  narrow.disp = 0x28;
  narrow.width = 1;
  narrow.read = true;
  narrow.value = 0;
  probe::GField wide;
  wide.disp = 0x38;
  wide.width = 8;
  wide.read = true;
  wide.kind = probe::WordValue{};
  wide.kind->kind = probe::ValueKind::Heap;
  wide.cls = probe::ClassMatch{true, true, 0x5B0100, probe::LeadMatch::None, std::nullopt};
  gr.fields = {narrow, wide};
  method.g_fields = {gr};
  method.defer_disp = 0x28;
  method.path_taken = "now";
  probe::MethodRead mr;
  mr.read = true;
  mr.words_read = 3;
  mr.own_low = 3;
  mr.highs = {{0x9000, 3}};
  method.before = mr;
  f.method = method;
  probe::Queue q;
  q.g_disp = 0x10;
  q.walk = "walked";
  probe::QueueRead qr;
  qr.read = true;
  qr.blocks = 5;
  qr.found = 1;
  probe::QueuedRequest qq;
  qq.depth = 2;
  qq.path = {0x10, 0};
  qq.fields.ours = true;
  qq.fields.manager = true;
  qq.fields.word = 110;
  qq.fields.flag = 0;
  qr.requests.push_back(qq);
  q.reads.push_back(qr);
  f.queue = q;
  f.memory_search.push_back(m);
  probe::RequestSearch rs;
  probe::RequestRead hit;
  hit.ours = false;
  hit.task = probe::WordValue{};
  hit.task->kind = probe::ValueKind::Heap;
  hit.task_class = probe::ClassMatch{true, true, 0x5B0200, probe::LeadMatch::None, std::nullopt};
  hit.manager = true;
  hit.word = 7;
  rs.hits = {hit, hit};
  probe::GameTask gt;
  gt.cls = *hit.task_class;
  gt.vtable = 0x5A1000;
  gt.count = 2;
  gt.slot_count = 4;
  probe::TaskWord tw;
  tw.value.kind = probe::ValueKind::Game;
  tw.value.section = probe::SectionKind::Rdata;
  tw.value.rva = 0x5A1000;
  tw.cls = gt.cls;
  gt.words = {tw};
  rs.tasks = {gt};
  f.request_search = {rs};
  probe::ClassBases cb;
  cb.object = "request";
  cb.cls = *shape.cls;
  cb.base_count = 1;
  cb.bases = {0x5B0300};
  f.classes = {cb};
  f.shared_bases = {{0x5B0300, 2}};
  const auto text = probe::write_report(f);
  REQUIRE(text.has_value());
  const std::array<const report::Vocabulary*, 1> known = {&probe::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  for (const char* key : {"\"how\": \"unlisted\"", "\"holds_request\": true", "\"request_slot\": 0",
                          "\"field\": \"manager\"", "\"src\": \"address\"", "\"now\": ", "\"path_taken\": \"now\"",
                          "\"defer_disp\": 40", "\"queue_walk\": \"walked\"", "\"ours\": true", "\"request_found\": 2",
                          "\"own_request_found\": 1", "\"request_tasks\": [", "\"word_class\": {",
                          "\"object\": \"request\"", "\"shared_bases\": [", "\"own_low\": 3", "\"highs\": [",
                          "\"manager_reads\": [", "\"field_class\": {"}) {
    CHECK_MESSAGE(text->find(key) != std::string::npos, key);
  }
}
