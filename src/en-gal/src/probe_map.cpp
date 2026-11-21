// R1 spike.12 (WHY§296; EN_PROBE only): the manager's map that the now path's function walks with any word but 0, and
// the list it walks with a word of 0. The layout is derived from that function's summary (probe.hpp's map_shape), never
// given, and the walk checks every node before it reads the next (probe.hpp's walk_map). Our own code; nothing here
// knows the game, and nothing it reads is kept but numbers.
#include "en/gal/probe.hpp"

#if EN_PROBE

#include <algorithm>
#include <cstring>
#include <map>
#include <set>

namespace en::gal::probe {

namespace {

std::optional<std::size_t> slot_of(std::uint8_t reg) noexcept {
  switch (reg) {
    case 1: return 0;
    case 2: return 1;
    case 8: return 2;
    case 9: return 3;
    default: return std::nullopt;
  }
}

// What the now path's function's values are, by its entry state and the request's fields.
class Roles {
 public:
  Roles(const EntryState& entry, const RequestShape& request, std::uint8_t task)
      : entry_(entry), task_(task), task_field_(request.task_field), manager_field_(request.field(FieldRole::Manager)),
        word_field_(request.field(FieldRole::Word)) {}

  // A load from the request's field at `field` (an 8-byte one, or with `narrow` up to 4 bytes).
  [[nodiscard]] bool from_request(const Source& s, std::optional<std::uint32_t> field, bool narrow) const noexcept {
    if (!field || s.kind != Source::Kind::Memory || s.index || s.base_src.size() != 1 || s.disp < 0 ||
        static_cast<std::uint64_t>(s.disp) != *field) {
      return false;
    }
    if (narrow ? s.width > 4 : s.width != 8) return false;
    const Source& b = s.base_src.front();
    const auto i = b.kind == Source::Kind::Entry ? slot_of(b.from) : std::nullopt;
    return i && entry_[*i].request;
  }
  // The manager plus a constant: the request's manager field (or a register entered with the manager), as added to
  // by an add or a lea of an immediate.
  [[nodiscard]] std::optional<std::int64_t> manager_plus(const Source& s, int depth = 0) const noexcept {
    if (depth > 4) return std::nullopt;
    if (s.kind == Source::Kind::Entry) {
      const auto i = slot_of(s.from);
      return i && entry_[*i].manager ? std::optional<std::int64_t>(0) : std::nullopt;
    }
    if (from_request(s, manager_field_, false)) return 0;
    if (s.kind == Source::Kind::Modified && (s.op == InsnClass::Add || s.op == InsnClass::Lea) && s.imm &&
        s.origin.size() == 1) {
      const auto base = manager_plus(s.origin.front(), depth + 1);
      if (base) return *base + static_cast<std::int64_t>(*s.imm);
    }
    return std::nullopt;
  }
  [[nodiscard]] bool word(const Source& s) const noexcept {
    if (s.kind == Source::Kind::Entry) {
      const auto i = slot_of(s.from);
      return i && entry_[*i].word;
    }
    return from_request(s, word_field_, true);
  }
  [[nodiscard]] bool task(const Source& s) const noexcept {
    if (s.kind == Source::Kind::Argument) return task_ != 0 && s.from == task_;
    return from_request(s, task_field_, false);
  }
  // The register `reg` at a call: its source, or (for an argument) whether it holds the task.
  [[nodiscard]] static const Source* source_at(const CalleeCall& c, std::uint8_t reg) noexcept {
    for (const PassedSource& p : c.pass_src) {
      if (p.reg == reg) return &p.source;
    }
    return nullptr;
  }
  [[nodiscard]] bool task_at(const CalleeCall& c, std::uint8_t reg) const noexcept {
    for (const PassedArg& p : c.pass) {
      if (p.reg == reg && p.from == task_ && task_ != 0 && !p.first_word) return true;
    }
    const Source* s = source_at(c, reg);
    return s != nullptr && task(*s);
  }

 private:
  const EntryState& entry_;
  std::uint8_t task_;
  std::uint32_t task_field_;
  std::optional<std::uint32_t> manager_field_, word_field_;
};

bool used_as_base(const FieldRead& r) noexcept {
  return std::any_of(r.uses.begin(), r.uses.end(), [](const FieldUse& u) { return u.kind == UseKind::Base; });
}

// A load of 8 bytes at [base + disp], `base` described by `base_ok`.
template <class Pred>
bool loads(const Source& s, std::int64_t disp, Pred base_ok) {
  return s.kind == Source::Kind::Memory && !s.index && s.width == 8 && s.disp == disp && s.base_src.size() == 1 &&
         base_ok(s.base_src.front());
}

void list_of(const CalleeCode& now, const Roles& roles, MapShape& out) {
  for (const FieldRead& end : now.manager_reads) {
    if (end.direct || end.width != 8 || end.index) continue;
    const auto k_end = roles.manager_plus(end.base_src);
    if (!k_end) continue;
    for (const FieldUse& u : end.uses) {
      if (u.kind != UseKind::Changed || u.op != std::optional<InsnClass>(InsnClass::Sub)) continue;
      for (const FieldRead& begin : now.manager_reads) {
        if (!begin.direct || begin.offset != u.offset || begin.width != 8 || begin.index) continue;
        const auto k_begin = roles.manager_plus(begin.base_src);
        if (!k_begin) continue;
        out.list_begin = *k_begin + begin.disp;
        out.list_end = *k_end + end.disp;
        return;
      }
    }
  }
}

}  // namespace

MapShape map_shape(const CalleeCode& now, const EntryState& entry, const RequestShape& request, std::uint8_t task) {
  MapShape out;
  if (!now.read || now.instructions == 0) {
    out.failed = "no_now";
    return out;
  }
  const Roles roles(entry, request, task);
  list_of(now, roles, out);
  // The head, the root and the nil byte: three loads, each the next one's base.
  std::string_view failed = "head";
  bool found = false;
  for (const FieldRead& h : now.manager_reads) {
    if (found) break;
    if (h.direct || h.width != 8 || h.index || !used_as_base(h)) continue;
    const auto k = roles.manager_plus(h.base_src);
    if (!k) continue;
    const auto is_manager = [&](const Source& m) { return roles.manager_plus(m) == k; };
    const auto is_head = [&](const Source& s) { return loads(s, h.disp, is_manager); };
    if (failed == "head") failed = "root";
    for (const FieldRead& r : now.manager_reads) {
      if (found) break;
      if (r.direct || r.width != 8 || r.index || !used_as_base(r) || !is_head(r.base_src)) continue;
      if (failed == "root") failed = "nil";
      for (const FieldRead& n : now.manager_reads) {
        const bool compared_zero = std::any_of(n.uses.begin(), n.uses.end(), [](const FieldUse& u) {
          return u.kind == UseKind::Compared && u.imm == std::optional<std::uint64_t>(0);
        });
        if (!n.direct || n.width != 1 || n.index || !compared_zero || !loads(n.base_src, r.disp, is_head)) continue;
        out.head_disp = *k + h.disp;
        out.root_disp = r.disp;
        out.nil_disp = n.disp;
        found = true;
        break;
      }
    }
  }
  if (!found) {
    out.failed = failed;
    return out;
  }
  // The key: the memory operand of each compare with the word, all at one place and width.
  std::optional<std::pair<std::int64_t, std::uint8_t>> key;
  for (const FieldUse& u : now.word_uses) {
    if (u.kind != UseKind::ComparedOther || u.width == 0 || u.rip || u.index) continue;
    const std::pair<std::int64_t, std::uint8_t> k{u.disp, u.width};
    if (key && *key != k) {
      out.failed = "key";
      return out;
    }
    key = k;
  }
  if (!key) {
    out.failed = "key";
    return out;
  }
  out.key_disp = key->first;
  out.key_width = key->second;
  // The call a match makes: the word in r8 and the manager in r9; the value, an 8-byte load in rcx where the summary
  // knows it (where the paths into the call disagree on its base it may not), else 8 bytes past the key.
  for (const CalleeCall& c : now.calls) {
    if (c.form != CallForm::Direct || !c.reached || !c.target) continue;
    const Source* rcx = Roles::source_at(c, 1);
    const Source* r8 = Roles::source_at(c, 8);
    const Source* r9 = Roles::source_at(c, 9);
    if (r8 == nullptr || r9 == nullptr || !roles.word(*r8) ||
        roles.manager_plus(*r9) != std::optional<std::int64_t>(0)) {
      continue;
    }
    const bool loaded = rcx != nullptr && rcx->kind == Source::Kind::Memory && rcx->width == 8 && !rcx->index;
    if (rcx != nullptr && !loaded) continue;  // rcx known, and not a load: no value of a node's
    out.call = c.target;
    out.call_site = c.offset;
    out.value_disp = loaded ? rcx->disp : out.key_disp + 8;
    out.value_known = loaded;
    break;
  }
  if (!out.call) {
    out.failed = "call";
    return out;
  }
  // The children: the tree's own, beside the parent (MSVC's node: left, parent, right).
  if (out.root_disp != 8) {
    out.failed = "shape";
    return out;
  }
  out.left_disp = 0;
  out.right_disp = out.root_disp + 8;
  // The other branch's call: the last direct call with the task in rcx before the first call through its first word.
  std::optional<std::uint32_t> through;
  for (const CalleeCall& c : now.calls) {
    if ((c.form == CallForm::Memory || c.form == CallForm::TailMemory) && c.reached && c.vtable_src &&
        roles.task(*c.vtable_src)) {
      through = c.offset;
      break;
    }
  }
  std::optional<std::uint32_t> first, before;
  for (const CalleeCall& c : now.calls) {
    if (c.form != CallForm::Direct || !c.reached || !c.target || c.target == out.call || !roles.task_at(c, 1)) continue;
    if (!first) first = c.target;
    if (through && c.offset < *through) before = c.target;
  }
  out.other = before ? before : first;
  return out;
}

MapWalk walk_map(const BlockReader& read, const HeapTest& on_heap, std::uint64_t manager, const MapShape& s,
                 std::uint32_t word) {
  MapWalk out;
  MapRead& r = out.read;
  if (!s.failed.empty()) return out;
  const auto u64 = [&](std::uint64_t at) -> std::optional<std::uint64_t> {
    std::array<std::byte, 8> b{};
    if (at == 0 || !read(at, b)) return std::nullopt;
    std::uint64_t v = 0;
    std::memcpy(&v, b.data(), 8);
    return v;
  };
  const auto field = [](const std::vector<std::byte>& b, std::int64_t at, std::size_t width) {
    std::uint64_t v = 0;
    if (at >= 0 && static_cast<std::size_t>(at) + width <= b.size()) std::memcpy(&v, b.data() + at, width);
    return v;
  };
  const auto at = [&](std::uint64_t base, std::int64_t disp) { return base + static_cast<std::uint64_t>(disp); };
  if (manager == 0 || !on_heap(manager)) {
    r.failed = "manager";
    r.list_failed = "manager";
    return out;
  }

  // ---- The map ----
  [&] {
    const auto head = u64(at(manager, s.head_disp));
    if (!head) {
      r.failed = "manager";
      return;
    }
    r.read = true;
    out.count_word = u64(at(manager, s.head_disp + 8));
    if (*head == 0 || !on_heap(*head)) {
      r.failed = "head";
      return;
    }
    const std::int64_t span = std::max({s.nil_disp + 1, s.root_disp + 8, s.left_disp + 8, s.right_disp + 8,
                                        s.key_disp + s.key_width, s.value_disp + 8});
    if (span <= 0 || span > 0x400 || s.key_width == 0 || s.key_width > 8) {
      r.failed = "unread";
      return;
    }
    std::vector<std::byte> hb(static_cast<std::size_t>(span));
    if (!read(*head, hb)) {
      r.failed = "head";
      return;
    }
    if (field(hb, s.nil_disp, 1) == 0) {
      r.failed = "head_nil";
      return;
    }
    const std::uint64_t root = field(hb, s.root_disp, 8);
    struct Node {
      std::uint64_t at = 0, left = 0, right = 0, parent = 0, key = 0, value = 0;
    };
    std::set<std::uint64_t> seen;
    std::vector<Node> stack;
    std::vector<std::uint64_t> keys;
    std::uint64_t first = 0, last = 0, root_parent = 0;
    std::vector<std::byte> nb(static_cast<std::size_t>(span));
    const auto visit = [&](std::uint64_t a) -> std::optional<Node> {
      if (a == 0 || !on_heap(a)) {
        r.failed = "pointer";
        return std::nullopt;
      }
      if (!seen.insert(a).second) {
        r.failed = "cycle";
        return std::nullopt;
      }
      if (seen.size() > kMaxMapNodes) {
        r.failed = "too_many";
        return std::nullopt;
      }
      if (!read(a, nb)) {
        r.failed = "unread";
        return std::nullopt;
      }
      if (field(nb, s.nil_disp, 1) != 0) {
        r.failed = "node_nil";
        return std::nullopt;
      }
      return Node{a, field(nb, s.left_disp, 8), field(nb, s.right_disp, 8), field(nb, s.root_disp, 8),
                  field(nb, s.key_disp, s.key_width), field(nb, s.value_disp, 8)};
    };
    std::uint64_t cur = root;
    for (;;) {
      while (cur != *head) {
        const std::optional<Node> n = visit(cur);
        if (!n) return;
        if (cur == root) root_parent = n->parent;
        stack.push_back(*n);
        cur = n->left;
      }
      if (stack.empty()) break;
      const Node n = stack.back();
      stack.pop_back();
      if (keys.empty()) first = n.at;
      last = n.at;
      keys.push_back(n.key);
      out.values.push_back(n.value);
      cur = n.right;
    }
    r.nodes = static_cast<std::uint32_t>(keys.size());
    r.ordered = std::adjacent_find(keys.begin(), keys.end(), std::greater_equal<>()) == keys.end();
    if (!keys.empty()) {
      r.root_parent = root_parent == *head;
      r.head_ends = field(hb, s.left_disp, 8) == first && field(hb, s.right_disp, 8) == last;
    }
    std::map<std::uint32_t, std::uint32_t> highs;
    for (const std::uint64_t k : keys) {
      if (r.key_lows.size() < kMaxMapKeys) r.key_lows.push_back(static_cast<std::uint32_t>(k & 0xFFFF));
      ++highs[static_cast<std::uint32_t>((k >> 16) & 0xFFFF)];
    }
    for (const auto& [high, n] : highs) {
      if (r.key_highs.size() >= kMaxHighs) break;
      r.key_highs.emplace_back(high, n);
    }
    const bool present = std::find(keys.begin(), keys.end(), std::uint64_t{word}) != keys.end();
    r.word_present = present;
    if (!present) {
      for (const std::uint64_t k : keys) {
        if (k < word && (!r.below || k > *r.below)) r.below = k;
        if (k > word && (!r.above || k < *r.above)) r.above = k;
      }
    }
  }();
  if (!r.failed.empty()) out.values.clear();

  // ---- The list ----
  [&] {
    if (!s.list_begin || !s.list_end) {
      r.list_failed = "no_list";
      return;
    }
    const auto b = u64(at(manager, *s.list_begin));
    const auto e = u64(at(manager, *s.list_end));
    if (!b || !e) {
      r.list_failed = "manager";
      return;
    }
    if (*e < *b || (*e - *b) % 8 != 0) {
      r.list_failed = "order";
      return;
    }
    const std::uint64_t n = (*e - *b) / 8;
    if (n > kMaxListEntries) {
      r.list_failed = "too_many";
      return;
    }
    if (n != 0 && !on_heap(*b)) {
      r.list_failed = "pointer";
      return;
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(n) * 8);
    for (std::size_t done = 0; done < bytes.size();) {
      const std::size_t chunk = std::min<std::size_t>(bytes.size() - done, kMaxCopyBytes);
      if (!read(*b + done, std::span<std::byte>(bytes.data() + done, chunk))) {
        r.list_failed = "unread";
        return;
      }
      done += chunk;
    }
    r.list_count = n;
    out.list.resize(static_cast<std::size_t>(n));
    if (n != 0) std::memcpy(out.list.data(), bytes.data(), bytes.size());
  }();
  return out;
}

}  // namespace en::gal::probe

#endif
