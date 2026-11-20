// The hub's reachable memory across R1's call (probe.hpp's walk_hub, search_walk and diff_hub; EN_PROBE only;
// WHY§209, WHY§235): spike.5's breadth-first walk from the hub's instance, its blocks kept in the caller's heap, the
// search for our task over it, and two walks compared block by block. Numbers and kinds only: the report keeps counts,
// paths of offsets and the kinds of values, never a byte that was read beyond a small integer.
#include "en/gal/probe.hpp"

#if EN_PROBE

#include <algorithm>
#include <cstring>
#include <map>
#include <set>

namespace en::gal::probe {

namespace {

constexpr std::uint64_t kPage = 0x1000;
constexpr std::uint32_t kFirstSpan = 0x1000;  // the instance: a page
constexpr std::uint32_t kSpan = 0x400;        // each block it reaches: 1 KiB
constexpr std::size_t kMaxSearchHits = 8;
constexpr std::uint64_t kSmall = 65536;

std::uint64_t word_at(std::span<const std::byte> b, std::size_t at) noexcept {
  std::uint64_t w = 0;
  std::memcpy(&w, b.data() + at, 8);
  return w;
}

std::span<const std::byte> bytes_of(const HubWalk& walk, const HubBlock& b) noexcept {
  return std::span<const std::byte>(walk.bytes.data() + b.at, b.len);
}

struct PathKey {
  std::uint32_t depth = 0;
  std::array<std::uint32_t, 3> path{};
  friend auto operator<=>(const PathKey&, const PathKey&) = default;
};
PathKey key_of(const HubBlock& b) noexcept {
  PathKey k{b.depth, {}};
  for (std::uint32_t i = 0; i < b.depth && i < k.path.size(); ++i) k.path[i] = b.path[i];
  return k;
}

}  // namespace

HubWalk walk_hub(SysLayer& sys, const HubWalkSpec& spec) {
  HubWalk w;
  w.bytes.reserve(static_cast<std::size_t>(spec.max_bytes + kFirstSpan));  // a block past the cap at most: never grown
  struct Pending {
    std::uint64_t address;
    std::array<std::uint32_t, 3> path;
  };
  std::vector<Pending> level{Pending{spec.instance, {}}};
  std::set<std::uint64_t> seen{spec.instance};
  for (std::uint32_t depth = 0; depth <= spec.max_depth && !level.empty() && spec.instance != 0; ++depth) {
    std::vector<Pending> next;
    for (const Pending& n : level) {
      if (w.blocks.size() >= spec.max_blocks || w.read_bytes >= spec.max_bytes) {
        w.capped = true;
        break;
      }
      std::uint64_t len = depth == 0 ? kFirstSpan : kSpan;
      const std::uint64_t to_page = kPage - (n.address & (kPage - 1));
      if (len > to_page && to_page >= 0x40) len = to_page;  // one page at a time: a block never crosses into the next
      if (implausible(n.address, len, 8, sys.stacks())) {
        ++w.refused;
        continue;
      }
      HubBlock b;
      b.address = n.address;
      b.len = static_cast<std::uint32_t>(len);
      b.depth = depth;
      b.path = n.path;
      b.at = w.bytes.size();
      w.bytes.resize(static_cast<std::size_t>(b.at + len));
      const std::span<std::byte> dst(w.bytes.data() + b.at, static_cast<std::size_t>(len));
      if (!sys.copier().copy(n.address, dst).copied) {
        ++w.faults;
        w.bytes.resize(static_cast<std::size_t>(b.at));
        w.blocks.push_back(b);
        continue;
      }
      b.read = true;
      w.read_bytes += len;
      w.blocks.push_back(b);
      if (depth >= spec.max_depth) continue;
      for (std::size_t o = 0; o + 8 <= dst.size(); o += 8) {
        const std::uint64_t v = word_at(dst, o);
        const bool kept_out = v >= spec.keep.begin && v < spec.keep.end;
        if (!kept_out && (v & 7) == 0 && v >= kLowestAddress && v < kAddressEnd &&
            (v < spec.image_base || v >= spec.image_end) && seen.insert(v).second) {
          Pending m{v, n.path};
          m.path[depth] = static_cast<std::uint32_t>(o);
          next.push_back(m);
        }
      }
    }
    level = std::move(next);
  }
  return w;
}

HubSearch search_walk(const HubWalk& walk, std::uint64_t target, std::uint64_t end, std::uint32_t group_index) {
  HubSearch h;
  h.nodes = walk.blocks.size();
  h.read_bytes = walk.read_bytes;
  h.faults = walk.faults;
  h.refused_reads = walk.refused;
  if (target == 0) return h;
  for (const HubBlock& b : walk.blocks) {
    if (!b.read) continue;
    const std::span<const std::byte> d = bytes_of(walk, b);
    for (std::size_t o = 0; o + 8 <= d.size(); o += 8) {
      const std::uint64_t v = word_at(d, o);
      if (v == target) {
        ++h.found;
        if (h.hits.size() < kMaxSearchHits) {
          HubHit x;
          x.depth = b.depth;
          x.path.assign(b.path.begin(), b.path.begin() + b.depth);
          x.path.push_back(static_cast<std::uint32_t>(o));
          const std::size_t lo = o >= 64 ? o - 64 : 0, hi = std::min(d.size(), o + 72);
          for (std::size_t k = lo; k + 4 <= hi; k += 4) {
            std::uint32_t g = 0;
            std::memcpy(&g, d.data() + k, 4);
            if (g == group_index) ++x.near_group;
          }
          h.hits.push_back(std::move(x));
        }
      } else if (v > target && v < end) {
        ++h.inside;
      }
    }
  }
  return h;
}

namespace {

class Differ {
 public:
  Differ(const HubWalk& before, const HubWalk& after, const DiffSpec& spec)
      : before_(before), after_(after), spec_(spec) {
    for (std::uint32_t i = 0; i < before.blocks.size(); ++i) {
      const HubBlock& b = before.blocks[i];
      by_path_.emplace(key_of(b), i);
      by_address_.emplace(b.address, i);
    }
    if (spec.earlier != nullptr) {
      for (const ChangedWord& c : *spec.earlier) earlier_.emplace(std::pair{c.block, c.offset}, c.value);
    }
  }

  DiffResult run() {
    DiffResult r;
    HubDiff& d = r.diff;
    d.before_blocks = count_read(before_);
    d.after_blocks = count_read(after_);
    d.capped = before_.capped || after_.capped;
    d.kept_bytes = before_.bytes.size();
    std::set<std::uint64_t> after_addresses;
    struct Change {
      std::uint32_t after_block;
      std::uint32_t before_block;
      std::uint32_t offset;
    };
    std::vector<Change> changes;  // every changed word, within the cap, in the after-walk's order
    for (std::uint32_t j = 0; j < after_.blocks.size(); ++j) {
      const HubBlock& a = after_.blocks[j];
      after_addresses.insert(a.address);
      if (!a.read) continue;
      std::optional<std::uint32_t> bi;
      if (const auto p = by_path_.find(key_of(a)); p != by_path_.end()) {
        if (before_.blocks[p->second].address == a.address) {
          bi = p->second;
        } else {
          ++d.moved;
        }
      }
      if (!bi) {
        if (const auto q = by_address_.find(a.address); q != by_address_.end()) bi = q->second;
      }
      if (!bi) {
        ++d.new_blocks;
        d.new_bytes += a.len;
        continue;
      }
      const HubBlock& b = before_.blocks[*bi];
      if (!b.read) continue;
      ++d.compared;
      const std::span<const std::byte> old_bytes = bytes_of(before_, b), new_bytes = bytes_of(after_, a);
      const std::size_t n = std::min(old_bytes.size(), new_bytes.size());
      for (std::size_t o = 0; o + 8 <= n; o += 8) {
        const std::uint64_t was = word_at(old_bytes, o), now = word_at(new_bytes, o);
        if (was == now) continue;
        ++d.changed;
        if (changes.size() < kMaxChangedWords) {
          changes.push_back(Change{j, *bi, static_cast<std::uint32_t>(o)});
          r.changed.push_back(ChangedWord{*bi, static_cast<std::uint32_t>(o), now});
        }
      }
    }
    for (const HubBlock& b : before_.blocks) {
      if (b.read && !after_addresses.contains(b.address)) ++d.gone;
    }
    // The words described: in the walk's order, or with an earlier diff the steady ones first.
    std::vector<std::size_t> order;
    std::vector<std::uint8_t> steady(changes.size(), 0);
    if (spec_.earlier != nullptr) {
      std::uint64_t steady_words = 0;
      for (std::size_t i = 0; i < changes.size(); ++i) {
        const auto e = earlier_.find(std::pair{changes[i].before_block, changes[i].offset});
        steady[i] = e != earlier_.end() && e->second == r.changed[i].value ? 1 : 0;
        steady_words += steady[i];
      }
      d.steady_words = steady_words;
      for (std::size_t i = 0; i < changes.size() && order.size() < kMaxDiffWords; ++i) {
        if (steady[i] != 0) order.push_back(i);
      }
    }
    for (std::size_t i = 0; i < changes.size() && order.size() < kMaxDiffWords; ++i) {
      if (steady[i] == 0) order.push_back(i);
    }
    for (const std::size_t i : order) {
      const Change& c = changes[i];
      const HubBlock& a = after_.blocks[c.after_block];
      DiffWord w;
      w.depth = a.depth;
      w.path.assign(a.path.begin(), a.path.begin() + a.depth);
      w.path.push_back(c.offset);
      w.before = classify(word_at(bytes_of(before_, before_.blocks[c.before_block]), c.offset), false);
      w.after = classify(word_at(bytes_of(after_, a), c.offset), true);
      if (spec_.earlier != nullptr) w.steady = steady[i] != 0;
      d.words.push_back(std::move(w));
    }
    return r;
  }

 private:
  static std::uint64_t count_read(const HubWalk& w) noexcept {
    return static_cast<std::uint64_t>(
        std::count_if(w.blocks.begin(), w.blocks.end(), [](const HubBlock& b) { return b.read; }));
  }

  // A value's kind: zero, the group's index, a small integer, our task object, our vtable, our module, the game's image
  // (its section by the characteristics, its RVA), other private or mapped memory (the heap), or none of those.
  WordValue classify(std::uint64_t v, bool after) const {
    WordValue x;
    const auto into = [&](const AddressRange& r) { return r.begin != 0 && v >= r.begin && v < r.end; };
    if (v == 0) {
      x.kind = ValueKind::Zero;
    } else if (v == spec_.group_index) {
      x.kind = ValueKind::Group;
    } else if (v <= kSmall) {
      x.kind = ValueKind::Small;
      x.value = v;
    } else if (into(spec_.task)) {
      x.kind = ValueKind::Task;
      if (v != spec_.task.begin) x.offset = v - spec_.task.begin;
    } else if (into(spec_.vtable)) {
      x.kind = ValueKind::Vtable;
      if (v != spec_.vtable.begin) x.offset = v - spec_.vtable.begin;
    } else {
      const std::optional<MemoryRegion> r = spec_.query ? spec_.query(v) : std::nullopt;
      const bool in = r && r->committed && v >= r->base && v - r->base < r->size;
      if (in && r->kind == RegionKind::Image && r->allocation_base != 0 && r->allocation_base == spec_.own_base) {
        x.kind = ValueKind::Ours;
        if (v - r->allocation_base <= 0xFFFF'FFFFull) x.rva = static_cast<std::uint32_t>(v - r->allocation_base);
      } else if (in && r->kind == RegionKind::Image && r->allocation_base != 0 &&
                 r->allocation_base == spec_.game_base) {
        x.kind = ValueKind::Game;
        const std::uint64_t rva = v - r->allocation_base;
        if (rva <= 0xFFFF'FFFFull) x.rva = static_cast<std::uint32_t>(rva);
        x.section = SectionKind::Other;
        for (const SectionSpan& s : spec_.game_sections) {
          if (rva >= s.rva && rva - s.rva < s.size) x.section = s.kind;
        }
        if (!spec_.game_sections.empty() && rva < spec_.game_sections.front().rva) x.section = SectionKind::Header;
      } else if (in && (r->kind == RegionKind::Private || r->kind == RegionKind::Mapped)) {
        x.kind = ValueKind::Heap;
      } else {
        x.kind = ValueKind::Other;
      }
    }
    if (spec_.group_index != 0 && x.kind != ValueKind::Group &&
        ((v & 0xFFFF'FFFFull) == spec_.group_index || (v >> 32) == spec_.group_index)) {
      x.group_half = true;
    }
    if (after && (x.kind == ValueKind::Task || x.kind == ValueKind::Vtable || x.kind == ValueKind::Heap)) {
      x.fresh = !by_address_.contains(v);
    }
    return x;
  }

  const HubWalk& before_;
  const HubWalk& after_;
  const DiffSpec& spec_;
  std::map<PathKey, std::uint32_t> by_path_;
  std::map<std::uint64_t, std::uint32_t> by_address_;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint64_t> earlier_;
};

}  // namespace

DiffResult diff_hub(const HubWalk& before, const HubWalk& after, const DiffSpec& spec) {
  return Differ(before, after, spec).run();
}

}  // namespace en::gal::probe

#endif
