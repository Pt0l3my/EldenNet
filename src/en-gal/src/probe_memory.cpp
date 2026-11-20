// The whole-memory search (probe.hpp's search_memory; EN_PROBE only; WHY§227, WHY§235): after R1's call, every committed
// and readable region of the process read through the crash-guarded copy for our task object's address, our fake
// vtable's and its slots'. Numbers only: the report keeps counts, kinds and offsets, never a byte that was read.
#include "en/gal/probe.hpp"

#if EN_PROBE

#include <algorithm>
#include <cstring>

namespace en::gal::probe {

namespace {

constexpr std::size_t kMaxHits = 8;  // of each side and kind
constexpr std::uint64_t kPage = 0x1000;
constexpr std::size_t kMaxModules = 16;  // images whose headers are kept for their hits' sections
constexpr std::uint64_t kMaxRegions = 1u << 22;  // regions walked at most (a process has thousands)
constexpr std::uint32_t kScnCode = 0x20, kScnExecute = 0x2000'0000, kScnRead = 0x4000'0000, kScnWrite = 0x8000'0000;

std::optional<std::uint32_t> le32(std::span<const std::byte> b, std::uint64_t at) noexcept {
  if (at > b.size() || b.size() - at < 4) return std::nullopt;
  std::uint32_t v = 0;
  std::memcpy(&v, b.data() + at, 4);
  return v;
}
std::optional<std::uint32_t> le16(std::span<const std::byte> b, std::uint64_t at) noexcept {
  if (at > b.size() || b.size() - at < 2) return std::nullopt;
  std::uint16_t v = 0;
  std::memcpy(&v, b.data() + at, 2);
  return v;
}

std::uint32_t distinct(const std::vector<std::uint8_t>& seen) noexcept {
  return static_cast<std::uint32_t>(std::count(seen.begin(), seen.end(), std::uint8_t{1}));
}

class Search {
 public:
  Search(SysLayer& sys, const MemorySearchSpec& spec) : sys_(sys), spec_(spec), buf_(kMaxCopyBytes) {
    out_.own_known = spec.own_known;
    skip_ = spec.skip;
    const std::byte* p = buf_.data();
    std::uint64_t at = 0;
    std::memcpy(&at, &p, sizeof at);  // the buffer's own address, as a number: it is never read
    skip_.push_back(AddressRange{at, at + buf_.size()});
    for (std::vector<MemoryHit>& list : lists_) list.reserve(kMaxHits);
    heads_.reserve(kMaxModules);
    end_ = spec.target + std::max<std::uint64_t>(spec.object_bytes, 1);
    for (std::size_t i = 0; i < spec.slots.size(); ++i) {
      if (spec.slots[i] != 0) slots_.emplace_back(spec.slots[i], static_cast<std::uint32_t>(i));
    }
    std::sort(slots_.begin(), slots_.end());
    game_slots_.assign(spec.slots.size(), 0);
    own_slots_.assign(spec.slots.size(), 0);
  }

  MemorySearch run(const RegionQuery& query) {
    const std::uint64_t start = sys_.now_us();
    std::uint64_t at = kLowestAddress;
    bool stop = false;
    for (std::uint64_t n = 0; at < kAddressEnd && spec_.target != 0 && n < kMaxRegions; ++n) {
      const std::optional<MemoryRegion> r = query(at);
      if (!r || r->size == 0 || r->base + r->size <= at) break;
      at = r->base + r->size;
      if (!r->committed) continue;
      ++out_.regions;
      out_.committed_bytes += r->size;
      if (!searchable(*r)) {
        out_.unread_bytes += r->size;
        continue;
      }
      if (stop) continue;  // past a cap: the regions are walked for the committed total, never read
      bool any = false;
      const std::uint64_t end = r->base + r->size;
      for (std::uint64_t p = r->base; p < end;) {
        if (out_.read_bytes >= spec_.max_bytes) {
          out_.stopped = SearchStop::Bytes;
        } else if (sys_.now_us() - start >= spec_.max_us) {
          out_.stopped = SearchStop::Time;
        }
        if (out_.stopped != SearchStop::Done) {
          stop = true;
          break;
        }
        const std::uint64_t len = std::min<std::uint64_t>(kMaxCopyBytes, end - p);
        any = chunk(p, len, *r) || any;
        p += len;
      }
      if (any) ++out_.read_regions;
    }
    out_.ms = (sys_.now_us() - start) / 1000;
    out_.slot_distinct = distinct(game_slots_);
    out_.own_slot_distinct = distinct(own_slots_);
    for (const std::vector<MemoryHit>& list : lists_) out_.hits.insert(out_.hits.end(), list.begin(), list.end());
    return std::move(out_);
  }

 private:
  enum class Got : std::uint8_t { Read, Fault, Refused };

  [[nodiscard]] bool kept_out(std::uint64_t at, std::uint64_t len) const noexcept {
    return std::any_of(skip_.begin(), skip_.end(), [&](const AddressRange& r) { return r.overlaps(at, len); });
  }

  // One chunk: whole when no stack or kept-out range overlaps it and it copies; else page by page.
  bool chunk(std::uint64_t at, std::uint64_t len, const MemoryRegion& r) {
    if (!kept_out(at, len) && !sys_.stacks().overlaps(at, len)) {
      const Got g = copy_scan(at, len, r);
      if (g != Got::Fault) return g == Got::Read;
    }
    bool any = false;
    for (std::uint64_t q = at; q < at + len; q += kPage) {
      const std::uint64_t n = std::min(kPage, at + len - q);
      if (kept_out(q, n)) {
        out_.stack_bytes += n;
        continue;
      }
      const Got g = copy_scan(q, n, r);
      if (g == Got::Fault) ++out_.faults;
      any = any || g == Got::Read;
    }
    return any;
  }

  Got copy_scan(std::uint64_t at, std::uint64_t len, const MemoryRegion& r) {
    if (const auto why = implausible(at, len, 8, sys_.stacks())) {
      (*why == FaultKind::Stack ? out_.stack_bytes : out_.unread_bytes) += len;
      return Got::Refused;
    }
    const std::span<std::byte> dst(buf_.data(), static_cast<std::size_t>(len));
    if (!sys_.copier().copy(at, dst).copied) return Got::Fault;
    out_.read_bytes += len;
    for (std::size_t o = 0; o + 8 <= dst.size(); o += 8) {
      std::uint64_t w = 0;
      std::memcpy(&w, dst.data() + o, 8);
      if (w >= spec_.target && w < end_) {
        hit(at + o, PointsTo::Object, w - spec_.target, std::nullopt, dst, o, r);
      } else if (w == spec_.vtable && w != 0) {
        hit(at + o, PointsTo::Vtable, 0, std::nullopt, dst, o, r);
      } else if (!slots_.empty() && w >= slots_.front().first && w <= slots_.back().first) {
        const auto it = std::lower_bound(slots_.begin(), slots_.end(), std::pair<std::uint64_t, std::uint32_t>{w, 0});
        if (it != slots_.end() && it->first == w) hit(at + o, PointsTo::Slot, 0, it->second, dst, o, r);
      }
    }
    return Got::Read;
  }

  [[nodiscard]] bool own(const MemoryRegion& r) const noexcept {
    if (r.allocation_base == 0) return false;
    if (spec_.own_base != 0 && r.allocation_base == spec_.own_base) return true;
    return std::find(spec_.own_bases.begin(), spec_.own_bases.end(), r.allocation_base) != spec_.own_bases.end();
  }

  void hit(std::uint64_t at, PointsTo what, std::uint64_t offset, std::optional<std::uint32_t> slot,
           std::span<const std::byte> d, std::size_t o, const MemoryRegion& r) {
    const bool mine = own(r);
    switch (what) {
      case PointsTo::Object:
        ++(offset == 0 ? (mine ? out_.own_found : out_.found) : (mine ? out_.own_inside : out_.inside));
        break;
      case PointsTo::Vtable:
        ++(mine ? out_.own_vtable_found : out_.vtable_found);
        break;
      case PointsTo::Slot:
        ++(mine ? out_.own_slot_found : out_.slot_found);
        if (slot) (mine ? own_slots_ : game_slots_)[*slot] = 1;
        break;
    }
    std::vector<MemoryHit>& list = lists_[(mine ? 3u : 0u) + static_cast<std::size_t>(what)];
    if (list.size() >= kMaxHits) return;
    MemoryHit h;
    h.own = mine;
    h.points_to = what;
    h.slot = slot;
    h.offset = offset;
    h.region = r.kind;
    h.protect = r.protect;
    if (r.kind == RegionKind::Image) {
      h.module = r.allocation_base == spec_.game_base ? ModuleKind::Game
                 : spec_.own_base != 0 && r.allocation_base == spec_.own_base ? ModuleKind::Ours
                                                                             : ModuleKind::Other;
      const std::uint64_t rva = at - r.allocation_base;
      h.section = section_of(r.allocation_base, rva);
      if (h.module != ModuleKind::Other && rva <= 0xFFFF'FFFFull) h.rva = static_cast<std::uint32_t>(rva);
    }
    const std::size_t lo = o >= 64 ? o - 64 : 0, hi = std::min(d.size(), o + 72);
    for (std::size_t k = lo; k + 4 <= hi; k += 4) {
      std::uint32_t v = 0;
      std::memcpy(&v, d.data() + k, 4);
      if (v == spec_.group_index) ++h.near_group;
    }
    list.push_back(h);
  }

  // An image's section kind at `rva`, from its first page, copied once per image (at most kMaxModules of them).
  SectionKind section_of(std::uint64_t base, std::uint64_t rva) {
    for (const Headers& h : heads_) {
      if (h.base == base) return h.read ? section_kind(h.bytes, rva) : SectionKind::None;
    }
    if (heads_.size() >= kMaxModules) return SectionKind::None;
    heads_.push_back(Headers{base, {}, false});
    Headers& h = heads_.back();
    h.read = !implausible(base, kPage, 8, sys_.stacks()) && sys_.copier().copy(base, h.bytes).copied;
    return h.read ? section_kind(h.bytes, rva) : SectionKind::None;
  }

  struct Headers {
    std::uint64_t base = 0;
    std::array<std::byte, kPage> bytes{};
    bool read = false;
  };

  SysLayer& sys_;
  const MemorySearchSpec& spec_;
  std::vector<std::byte> buf_;
  std::vector<AddressRange> skip_;
  std::uint64_t end_ = 0;
  std::array<std::vector<MemoryHit>, 6> lists_;  // the game's, then ours: each our object's, our vtable's, a slot's
  std::vector<std::pair<std::uint64_t, std::uint32_t>> slots_;  // (address, index), by address
  std::vector<std::uint8_t> game_slots_, own_slots_;            // which slots each side's words name
  std::vector<Headers> heads_;
  MemorySearch out_;
};

}  // namespace

bool searchable(const MemoryRegion& r) noexcept {
  if (!r.committed || (r.protect & (kPageGuard | kPageNoCache | kPageWriteCombine)) != 0) return false;
  switch (r.protect & 0xFF) {
    case kPageReadOnly: case kPageReadWrite: case kPageWriteCopy:
    case kPageExecuteRead: case kPageExecuteReadWrite: case kPageExecuteWriteCopy:
      return true;
    default:
      return false;  // no access, execute only, or nothing
  }
}

SectionKind section_kind_of(std::uint32_t ch) noexcept {
  if ((ch & (kScnExecute | kScnCode)) != 0) return SectionKind::Code;
  if ((ch & kScnWrite) != 0) return SectionKind::Data;
  if ((ch & kScnRead) != 0) return SectionKind::Rdata;
  return SectionKind::Other;
}

SectionKind section_kind(std::span<const std::byte> h, std::uint64_t rva) noexcept {
  if (h.size() < 0x40 || h[0] != std::byte{'M'} || h[1] != std::byte{'Z'}) return SectionKind::Other;
  const auto nt = le32(h, 0x3C);
  if (!nt || le32(h, *nt) != 0x0000'4550u) return SectionKind::Other;  // "PE\0\0"
  const auto count = le16(h, std::uint64_t{*nt} + 6);
  const auto optional = le16(h, std::uint64_t{*nt} + 20);
  if (!count || !optional) return SectionKind::Other;
  const std::uint64_t table = std::uint64_t{*nt} + 24 + *optional;
  std::optional<std::uint32_t> first;
  for (std::uint32_t i = 0; i < *count && i < 96; ++i) {
    const std::uint64_t s = table + 40ull * i;
    const auto vsize = le32(h, s + 8), va = le32(h, s + 12), raw = le32(h, s + 16), ch = le32(h, s + 36);
    if (!vsize || !va || !raw || !ch) break;
    first = first ? std::min(*first, *va) : *va;
    const std::uint64_t span = *vsize != 0 ? *vsize : *raw;
    if (rva < *va || rva - *va >= span) continue;
    return section_kind_of(*ch);
  }
  if (first && rva < *first) return SectionKind::Header;
  return SectionKind::Other;
}

MemorySearch search_memory(SysLayer& sys, const RegionQuery& query, const MemorySearchSpec& spec) {
  Search s(sys, spec);
  return s.run(query);
}

}  // namespace en::gal::probe

#endif
