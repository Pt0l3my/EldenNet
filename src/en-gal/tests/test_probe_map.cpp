// R1 spike.12 (WHY§296): the manager's map that the now path's function walks, its layout derived from that function's
// summary, and the walk's checks on trees made by hand (a cycle, a nil head, a node off the heap, more than 4,096
// nodes), and the report of it. Every byte here is our own code, assembled by hand, and every key and value is
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

using namespace en::gal;

namespace {

constexpr std::uint32_t kNow = 0x520000, kTake = 0x520400, kOther = 0x520600;
constexpr std::uint8_t kTask = 1;

std::vector<std::byte> bytes_of(std::initializer_list<int> bytes) {
  std::vector<std::byte> out;
  for (const int b : bytes) out.push_back(std::byte{static_cast<std::uint8_t>(b)});
  return out;
}
void rel32(std::vector<std::byte>& code, std::size_t at, std::uint32_t rva, std::size_t next, std::uint32_t target) {
  const auto d = static_cast<std::int32_t>(static_cast<std::int64_t>(target) - static_cast<std::int64_t>(rva + next));
  std::memcpy(code.data() + at, &d, 4);
}

// A now path's function of R1 run 11's shape, of our own (173 bytes; the request in rcx: its task at +8, its manager at
// +0x10, its word at +0x18, its byte at +0x1C). With a word of 0, for each entry of the manager's list (its end at
// manager + 0x28 + 0x10, its begin at + 8) a call with the entry and the task; with any other word, a walk of the map
// from the head at manager + 0x18 (the root at the head's +8, the nil byte at each node's +25, the key at +32), and on
// a match a call with the node's value (+40), the task, the word and the manager. Then, where the request's byte is not
// zero, a call with the task and one through the task's first word.
std::vector<std::byte> now_of_run11() {
  std::vector<std::byte> c = bytes_of({
      0x53,                          //   0: push rbx
      0x56,                          //   1: push rsi
      0x57,                          //   2: push rdi
      0x48, 0x83, 0xEC, 0x20,        //   3: sub rsp, 0x20
      0x48, 0x89, 0xCB,              //   7: mov rbx, rcx                  (the request)
      0x48, 0x8B, 0x71, 0x08,        //  10: mov rsi, [rcx + 8]            (the task)
      0x44, 0x8B, 0x43, 0x18,        //  14: mov r8d, [rbx + 0x18]         (the word)
      0x45, 0x85, 0xC0,              //  18: test r8d, r8d
      0x75, 0x2E,                    //  21: jne 69
      0x48, 0x8B, 0x43, 0x10,        //  23: mov rax, [rbx + 0x10]         (the manager)
      0x48, 0x83, 0xC0, 0x28,        //  27: add rax, 0x28
      0x48, 0x8B, 0x48, 0x10,        //  31: mov rcx, [rax + 0x10]         (the list's end)
      0x48, 0x2B, 0x48, 0x08,        //  35: sub rcx, [rax + 8]            (its begin)
      0x48, 0xC1, 0xF9, 0x03,        //  39: sar rcx, 3
      0x48, 0x85, 0xC9,              //  43: test rcx, rcx
      0x7E, 0x59,                    //  46: jle 137
      0x48, 0x8B, 0x43, 0x10,        //  48: mov rax, [rbx + 0x10]
      0x48, 0x8B, 0x40, 0x30,        //  52: mov rax, [rax + 0x30]
      0x48, 0x8B, 0x08,              //  56: mov rcx, [rax]
      0x48, 0x89, 0xF2,              //  59: mov rdx, rsi
      0xE8, 0, 0, 0, 0,              //  62: call take
      0xEB, 0x44,                    //  67: jmp 137
      0x4C, 0x8B, 0x4B, 0x10,        //  69: mov r9, [rbx + 0x10]          (the manager)
      0x49, 0x8B, 0x49, 0x18,        //  73: mov rcx, [r9 + 0x18]          (the head)
      0x48, 0x8B, 0x51, 0x08,        //  77: mov rdx, [rcx + 8]            (the root)
      0x48, 0x89, 0xC8,              //  81: mov rax, rcx
      0x80, 0x7A, 0x19, 0x00,        //  84: cmp byte ptr [rdx + 25], 0
      0x75, 0x2F,                    //  88: jne 137
      0x44, 0x39, 0x42, 0x20,        //  90: cmp [rdx + 32], r8d           (the key)
      0x73, 0x06,                    //  94: jae 102
      0x48, 0x8B, 0x52, 0x10,        //  96: mov rdx, [rdx + 16]
      0xEB, 0x06,                    // 100: jmp 108
      0x48, 0x89, 0xD0,              // 102: mov rax, rdx
      0x48, 0x8B, 0x12,              // 105: mov rdx, [rdx]
      0x80, 0x7A, 0x19, 0x00,        // 108: cmp byte ptr [rdx + 25], 0
      0x74, 0xE8,                    // 112: je 90
      0x48, 0x39, 0xC8,              // 114: cmp rax, rcx
      0x74, 0x12,                    // 117: je 137
      0x44, 0x3B, 0x40, 0x20,        // 119: cmp r8d, [rax + 32]
      0x72, 0x0C,                    // 123: jb 137
      0x48, 0x8B, 0x48, 0x28,        // 125: mov rcx, [rax + 40]           (the value)
      0x48, 0x89, 0xF2,              // 129: mov rdx, rsi
      0xE8, 0, 0, 0, 0,              // 132: call take
      0x80, 0x7B, 0x1C, 0x00,        // 137: cmp byte ptr [rbx + 0x1C], 0
      0x74, 0x11,                    // 141: je 160
      0x48, 0x89, 0xF1,              // 143: mov rcx, rsi
      0xE8, 0, 0, 0, 0,              // 146: call other
      0x48, 0x8B, 0x06,              // 151: mov rax, [rsi]
      0x48, 0x89, 0xF1,              // 154: mov rcx, rsi
      0xFF, 0x50, 0x08,              // 157: call [rax + 8]
      0xB8, 0x01, 0x00, 0x00, 0x00,  // 160: mov eax, 1
      0x48, 0x83, 0xC4, 0x20,        // 165: add rsp, 0x20
      0x5F,                          // 169: pop rdi
      0x5E,                          // 170: pop rsi
      0x5B,                          // 171: pop rbx
      0xC3,                          // 172: ret
  });
  rel32(c, 63, kNow, 67, kTake);
  rel32(c, 133, kNow, 137, kTake);
  rel32(c, 147, kNow, 151, kOther);
  return c;
}

probe::RequestShape request_of_run10() {
  probe::RequestShape r;
  r.vtable = 0x5A0000;
  r.disp = 0x20;
  r.task_field = 8;
  const std::array<std::pair<std::uint32_t, probe::FieldRole>, 5> fields = {{{0, probe::FieldRole::Vtable},
                                                                            {8, probe::FieldRole::Task},
                                                                            {16, probe::FieldRole::Manager},
                                                                            {24, probe::FieldRole::Word},
                                                                            {28, probe::FieldRole::Zero}}};
  for (const auto& [off, role] : fields) {
    probe::RequestField f;
    f.offset = off;
    f.role = role;
    f.width = off == 24 ? 4 : off == 28 ? 1 : 8;
    r.fields.push_back(f);
  }
  return r;
}

probe::CalleeCode summary_of(const std::vector<std::byte>& code, const probe::RequestShape& r) {
  probe::EntryState e{};
  e[0].request = true;
  e[3].zero = true;  // as G's method's stub leaves r9
  probe::SummaryOptions options;
  options.request = &r;
  return probe::summarise_callee(code, kNow, static_cast<std::uint32_t>(code.size()), e, kTask, options);
}

// A heap of our own, and MSVC-shaped trees in it: a head (its nil byte 1) and nodes (left +0, parent +8, right +16, a
// colour at +24, the nil byte at +25, a 4-byte key at +32, an 8-byte value at +40).
class Heap {
 public:
  static constexpr std::uint64_t kBase = 0x2000'0000;
  explicit Heap(std::size_t size = 1u << 20) : bytes_(size) {}
  std::uint64_t alloc(std::size_t n) {
    const std::uint64_t at = kBase + used_;
    used_ += (n + 15) & ~std::size_t{15};
    REQUIRE(used_ <= bytes_.size());
    return at;
  }
  void put(std::uint64_t at, std::uint64_t v, std::size_t width = 8) {
    std::memcpy(bytes_.data() + (at - kBase), &v, width);
  }
  [[nodiscard]] std::uint64_t get(std::uint64_t at) const {
    std::uint64_t v = 0;
    std::memcpy(&v, bytes_.data() + (at - kBase), 8);
    return v;
  }
  [[nodiscard]] bool on(std::uint64_t at) const { return at >= kBase && at < kBase + bytes_.size(); }
  [[nodiscard]] probe::BlockReader reader() const {
    return [this](std::uint64_t at, std::span<std::byte> out) {
      if (!on(at) || at + out.size() > kBase + bytes_.size()) return false;
      std::memcpy(out.data(), bytes_.data() + (at - kBase), out.size());
      return true;
    };
  }
  [[nodiscard]] probe::HeapTest test() const {
    return [this](std::uint64_t at) { return on(at); };
  }

 private:
  std::vector<std::byte> bytes_;
  std::size_t used_ = 0;
};

constexpr std::int64_t kHeadOff = 0x18, kBeginOff = 0x30, kEndOff = 0x38;

struct Tree {
  std::uint64_t manager = 0, head = 0;
  std::vector<std::uint64_t> nodes;  // by insertion
};

// A tree of our own making: an unbalanced binary search tree whose leaves point at the head, the head's parent its
// root and its children the first and last nodes.
Tree tree_of(Heap& h, std::initializer_list<std::uint32_t> keys) {
  Tree t;
  t.manager = h.alloc(0x40);
  t.head = h.alloc(48);
  h.put(t.head + 25, 1, 1);
  h.put(t.head + 0, t.head);
  h.put(t.head + 8, t.head);
  h.put(t.head + 16, t.head);
  h.put(t.manager + kHeadOff, t.head);
  std::uint64_t count = 0;
  for (const std::uint32_t k : keys) {
    const std::uint64_t n = h.alloc(48);
    h.put(n + 0, t.head);
    h.put(n + 16, t.head);
    h.put(n + 32, k, 4);
    h.put(n + 40, 0x3000'0000ull + k);  // an invented value: the key's
    std::uint64_t parent = t.head;
    std::uint64_t at = h.get(t.head + 8);
    bool left = false;
    while (at != t.head) {
      parent = at;
      std::uint32_t pk = 0;
      pk = static_cast<std::uint32_t>(h.get(at + 32) & 0xFFFF'FFFF);
      left = k < pk;
      at = h.get(at + (left ? 0 : 16));
    }
    h.put(n + 8, parent);
    if (parent == t.head) {
      h.put(t.head + 8, n);
    } else {
      h.put(parent + (left ? 0 : 16), n);
    }
    if (count == 0 || k < (h.get(h.get(t.head + 0) + 32) & 0xFFFF'FFFF)) h.put(t.head + 0, n);
    if (count == 0 || k > (h.get(h.get(t.head + 16) + 32) & 0xFFFF'FFFF)) h.put(t.head + 16, n);
    t.nodes.push_back(n);
    ++count;
  }
  h.put(t.manager + kHeadOff + 8, count);
  return t;
}

probe::MapShape shape_of_run11() {
  probe::MapShape s;
  s.head_disp = kHeadOff;
  s.root_disp = 8;
  s.nil_disp = 25;
  s.key_disp = 32;
  s.key_width = 4;
  s.value_disp = 40;
  s.left_disp = 0;
  s.right_disp = 16;
  s.list_begin = kBeginOff;
  s.list_end = kEndOff;
  s.call = kTake;
  return s;
}

}  // namespace

TEST_CASE("probe: the map's layout, from a now path's function of R1 run 11's shape") {
  const std::vector<std::byte> code = now_of_run11();
  const probe::RequestShape r = request_of_run10();
  const probe::CalleeCode n = summary_of(code, r);
  CHECK_FALSE(n.modelled_to.has_value());
  // The word's compares with memory keep their operand: [node + 32], 4 bytes, twice.
  const auto compares = std::count_if(n.word_uses.begin(), n.word_uses.end(), [](const probe::FieldUse& u) {
    return u.kind == probe::UseKind::ComparedOther && u.width == 4 && u.disp == 32;
  });
  CHECK(compares == 2);
  probe::EntryState e{};
  e[0].request = true;
  e[3].zero = true;
  const probe::MapShape s = probe::map_shape(n, e, r, kTask);
  CHECK(s.failed.empty());
  CHECK(s.head_disp == 0x18);
  CHECK(s.root_disp == 8);
  CHECK(s.nil_disp == 25);
  CHECK(s.key_disp == 32);
  CHECK(s.key_width == 4);
  CHECK(s.value_disp == 40);  // 8 past the key: the paths into the call disagree on rcx's base, so it is not known
  CHECK_FALSE(s.value_known);
  CHECK(s.left_disp == 0);
  CHECK(s.right_disp == 16);
  CHECK(s.list_begin == std::optional<std::int64_t>(0x30));
  CHECK(s.list_end == std::optional<std::int64_t>(0x38));
  CHECK(s.call == std::optional<std::uint32_t>(kTake));
  CHECK(s.call_site == std::optional<std::uint32_t>(132));  // the match's call, not the list's (r9 no manager there)
  CHECK(s.other == std::optional<std::uint32_t>(kOther));
  {  // where the summary knows rcx at the call, a load of the node's: its displacement is the value's
    probe::CalleeCode known = n;
    for (probe::CalleeCall& c : known.calls) {
      if (c.offset != 132) continue;
      probe::Source v;
      v.kind = probe::Source::Kind::Memory;
      v.disp = 48;
      v.width = 8;
      v.base_src.push_back(probe::Source{});
      c.pass_src.push_back(probe::PassedSource{1, v});
    }
    const probe::MapShape k = probe::map_shape(known, e, r, kTask);
    CHECK(k.value_known);
    CHECK(k.value_disp == 48);
  }

  // Into the match's call: rcx holds the value, followed as such.
  const auto site =
      std::find_if(n.calls.begin(), n.calls.end(), [](const probe::CalleeCall& c) { return c.offset == 132; });
  REQUIRE(site != n.calls.end());
  probe::EntryState into = probe::request_entry_at(*site, e, r, kTask);
  CHECK(into[1].from == kTask);
  CHECK(into[2].word);
  CHECK(into[3].manager);
  into[0] = probe::EntryArg{};
  into[0].value = true;
  const std::vector<std::byte> take = bytes_of({
      0x8B, 0x41, 0x08,  // 0: mov eax, [rcx + 8]   (a field of the value)
      0x85, 0xC0,        // 3: test eax, eax
      0xC3,              // 5: ret
  });
  const probe::CalleeCode t = probe::summarise_callee(take, kTake, 6, into, kTask);
  CHECK(t.value_read_count == 1);
  REQUIRE(t.value_reads.size() == 1);
  CHECK(t.value_reads[0].disp == 8);
  CHECK(t.value_reads[0].width == 4);

  // Without the request's manager field nothing is derived; without the word's compares no key.
  probe::RequestShape no_manager = r;
  no_manager.fields[2].role = probe::FieldRole::Other;
  CHECK(probe::map_shape(n, e, no_manager, kTask).failed == "head");
  probe::CalleeCode no_key = n;
  no_key.word_uses.clear();
  CHECK(probe::map_shape(no_key, e, r, kTask).failed == "key");
  probe::CalleeCode no_call = n;
  no_call.calls.clear();
  CHECK(probe::map_shape(no_call, e, r, kTask).failed == "call");
  CHECK(probe::map_shape(probe::CalleeCode{}, e, r, kTask).failed == "no_now");
}

TEST_CASE("probe: the map walked in order, the word found or its nearest keys, and the list") {
  Heap h;
  const Tree t = tree_of(h, {0x90000040, 0x90000010, 0x90000070, 0x90000005, 0x90000020, 0x10000003});
  const std::uint64_t list = h.alloc(3 * 8);
  for (std::uint64_t i = 0; i < 3; ++i) h.put(list + 8 * i, 0x4000'0000ull + i);
  h.put(t.manager + kBeginOff, list);
  h.put(t.manager + kEndOff, list + 3 * 8);
  const probe::MapShape s = shape_of_run11();

  const probe::MapWalk w = probe::walk_map(h.reader(), h.test(), t.manager, s, 0x90000020);
  CHECK(w.read.failed.empty());
  CHECK(w.read.read);
  CHECK(w.read.nodes == 6);
  CHECK(w.read.ordered);
  CHECK(w.read.root_parent == std::optional<bool>(true));
  CHECK(w.read.head_ends == std::optional<bool>(true));
  CHECK(w.count_word == std::optional<std::uint64_t>(6));
  CHECK(w.read.key_lows == std::vector<std::uint32_t>{3, 5, 0x10, 0x20, 0x40, 0x70});
  REQUIRE(w.read.key_highs.size() == 2);
  CHECK(w.read.key_highs[0] == std::pair<std::uint32_t, std::uint32_t>{0x1000, 1});
  CHECK(w.read.key_highs[1] == std::pair<std::uint32_t, std::uint32_t>{0x9000, 5});
  CHECK(w.read.word_present == std::optional<bool>(true));
  CHECK_FALSE(w.read.below.has_value());
  REQUIRE(w.values.size() == 6);
  CHECK(w.values.front() == 0x3000'0000ull + 0x10000003);
  CHECK(w.values.back() == 0x3000'0000ull + 0x90000070);
  CHECK(w.read.list_count == std::optional<std::uint64_t>(3));
  CHECK(w.list == std::vector<std::uint64_t>{0x4000'0000ull, 0x4000'0001ull, 0x4000'0002ull});

  // A word not there: the nearest keys below and above it.
  const probe::MapWalk absent = probe::walk_map(h.reader(), h.test(), t.manager, s, 0x9000006E);
  CHECK(absent.read.word_present == std::optional<bool>(false));
  CHECK(absent.read.below == std::optional<std::uint64_t>(0x90000040));
  CHECK(absent.read.above == std::optional<std::uint64_t>(0x90000070));
  const probe::MapWalk past = probe::walk_map(h.reader(), h.test(), t.manager, s, 0x9000FFFF);
  CHECK(past.read.below == std::optional<std::uint64_t>(0x90000070));
  CHECK_FALSE(past.read.above.has_value());

  // An empty map (the root the head itself): no node; a shape not derived: nothing read.
  Heap e;
  const Tree empty = tree_of(e, {});
  const probe::MapWalk none = probe::walk_map(e.reader(), e.test(), empty.manager, s, 7);
  CHECK(none.read.failed.empty());
  CHECK(none.read.nodes == 0);
  CHECK(none.read.word_present == std::optional<bool>(false));
  CHECK_FALSE(none.read.root_parent.has_value());
  CHECK(none.read.list_count == std::optional<std::uint64_t>(0));
  probe::MapShape failed = s;
  failed.failed = "key";
  const probe::MapWalk refused = probe::walk_map(h.reader(), h.test(), t.manager, failed, 7);
  CHECK_FALSE(refused.read.read);
  CHECK(refused.values.empty());
}

TEST_CASE("probe: the map's walk refuses a cycle, a nil head, a node off the heap and more than 4,096 nodes") {
  const probe::MapShape s = shape_of_run11();
  SUBCASE("a cycle: a node's child an earlier node") {
    Heap h;
    const Tree t = tree_of(h, {20, 10, 30});
    h.put(t.nodes[2] + 16, t.nodes[0]);  // 30's right child: the root
    const probe::MapWalk w = probe::walk_map(h.reader(), h.test(), t.manager, s, 10);
    CHECK(w.read.failed == "cycle");
    CHECK(w.values.empty());
    CHECK_FALSE(w.read.word_present.has_value());
  }
  SUBCASE("a nil head: its nil byte zero") {
    Heap h;
    const Tree t = tree_of(h, {20, 10});
    h.put(t.head + 25, 0, 1);
    CHECK(probe::walk_map(h.reader(), h.test(), t.manager, s, 10).read.failed == "head_nil");
  }
  SUBCASE("a node off the heap") {
    Heap h;
    const Tree t = tree_of(h, {20, 10, 30});
    h.put(t.nodes[1] + 0, 0x7FF0'0000'0000ull);  // 10's left child
    CHECK(probe::walk_map(h.reader(), h.test(), t.manager, s, 10).read.failed == "pointer");
  }
  SUBCASE("a node whose nil byte is not zero, other than the head") {
    Heap h;
    const Tree t = tree_of(h, {20, 10, 30});
    h.put(t.nodes[2] + 25, 1, 1);
    CHECK(probe::walk_map(h.reader(), h.test(), t.manager, s, 10).read.failed == "node_nil");
  }
  SUBCASE("a head off the heap, and no manager") {
    Heap h;
    const Tree t = tree_of(h, {20});
    h.put(t.manager + kHeadOff, 0x10);
    CHECK(probe::walk_map(h.reader(), h.test(), t.manager, s, 10).read.failed == "head");
    const probe::MapWalk w = probe::walk_map(h.reader(), h.test(), 0, s, 10);
    CHECK(w.read.failed == "manager");
    CHECK(w.read.list_failed == "manager");
  }
  SUBCASE("4,096 nodes walked, 4,097 refused") {
    Heap h(1u << 22);
    Tree t;
    t.manager = h.alloc(0x40);
    t.head = h.alloc(48);
    h.put(t.head + 25, 1, 1);
    h.put(t.manager + kHeadOff, t.head);
    std::uint64_t prev = t.head;
    for (std::uint32_t k = 0; k < 4097; ++k) {  // a chain to the right, each node the next one's parent
      const std::uint64_t n = h.alloc(48);
      h.put(n + 0, t.head);
      h.put(n + 8, prev);
      h.put(n + 16, t.head);
      h.put(n + 32, k, 4);
      if (prev == t.head) {
        h.put(t.head + 8, n);
      } else {
        h.put(prev + 16, n);
      }
      prev = n;
      t.nodes.push_back(n);
    }
    CHECK(probe::walk_map(h.reader(), h.test(), t.manager, s, 5).read.failed == "too_many");
    h.put(t.nodes[4094] + 16, t.head);  // cut after 4,095: walked
    const probe::MapWalk w = probe::walk_map(h.reader(), h.test(), t.manager, s, 5);
    CHECK(w.read.failed.empty());
    CHECK(w.read.nodes == 4095);
    CHECK(w.read.key_lows.size() == probe::kMaxMapKeys);
    CHECK(w.read.word_present == std::optional<bool>(true));
  }
  SUBCASE("a list out of order, and none") {
    Heap h;
    const Tree t = tree_of(h, {20});
    h.put(t.manager + kBeginOff, 0x2000'0100);
    h.put(t.manager + kEndOff, 0x2000'00F0);
    CHECK(probe::walk_map(h.reader(), h.test(), t.manager, s, 10).read.list_failed == "order");
    probe::MapShape no_list = s;
    no_list.list_begin.reset();
    CHECK(probe::walk_map(h.reader(), h.test(), t.manager, no_list, 10).read.list_failed == "no_list");
  }
}

TEST_CASE("probe: the map in the report, with its readings, linted") {
  probe::Findings f;
  f.map = probe::ManagerMap{};
  f.map->shape = shape_of_run11();
  f.map->shape.other = kOther;
  f.map->word = 0x9000006Eu;
  f.map->made = 3;
  f.map->changes = 1;
  probe::MapRead title;
  title.stage = "title";
  title.read = true;
  title.nodes = 2;
  title.ordered = true;
  title.key_lows = {3, 0x40};
  title.key_highs = {{0x9000, 2}};
  title.word_present = false;
  title.below = 0x90000040;
  probe::ValueCount v;
  v.kind.kind = probe::ValueKind::Heap;
  v.cls = probe::ClassMatch{true, true, 0x5B0000, probe::LeadMatch::None, std::nullopt};
  v.count = 2;
  title.values.push_back(v);
  title.list_count = 1;
  title.list_values.push_back(v);
  probe::MapRead world = title;
  world.at_ms = 30000;
  world.stage = "world";
  world.word_present = true;
  world.below.reset();
  probe::MapRead failed;
  failed.at_ms = 40000;
  failed.failed = "cycle";
  failed.list_failed = "no_list";
  f.map->readings = {title, world, failed};
  const auto pretty = probe::write_report(f);
  REQUIRE(pretty.has_value());
  const std::array<const report::Vocabulary*, 1> vocab = {&probe::vocabulary()};
  CHECK(report::lint(*pretty, vocab).has_value());
  CHECK(pretty->find("\"word_present\": true") != std::string::npos);
  CHECK(pretty->find("\"map_failed\": \"cycle\"") != std::string::npos);
  // On one line, the same report, linted the same.
  const auto line = probe::write_report(f, report::Style::Line);
  REQUIRE(line.has_value());
  CHECK(line->find('\n') + 1 == line->size());  // one line, ended
  CHECK(line->size() < pretty->size());
  CHECK(report::lint(*line, vocab).has_value());
  // A shape not derived names its part.
  f.map->shape = probe::MapShape{};
  f.map->shape.failed = "nil";
  const auto refused = probe::write_report(f);
  REQUIRE(refused.has_value());
  CHECK(refused->find("\"shape_failed\": \"nil\"") != std::string::npos);
}
