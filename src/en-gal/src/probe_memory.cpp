// The whole-memory search (probe.hpp's search_memory; EN_PROBE only; WHY§227): after R1's call, every committed and
// readable region of the process read through the crash-guarded copy for our task object's address. Numbers only: the
// report keeps counts, kinds and offsets, never a byte that was read.
#include "en/gal/probe.hpp"

#if EN_PROBE

#include <algorithm>
#include <cstring>

namespace en::gal::probe {

namespace {

constexpr std::size_t kMaxHits = 16;
constexpr std::uint64_t kPage = 0x1000;
constexpr std::size_t kMaxModules = 16;  // images whose headers are kept for their hits' sections
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

class Search {
 public:
  Search(SysLayer& sys, const MemorySearchSpec& spec) : sys_(sys), spec_(spec), buf_(kMaxCopyBytes) {
    out_.own_known = spec.own_known;
    skip_ = spec.skip;
    const std::byte* p = buf_.data();
    std::uint64_t at = 0;
    std::memcpy(&at, &p, sizeof at);  // the buffer's own address, as a number: it is never read
    skip_.push_back(AddressRange{at, at + buf_.size()});
    game_.reserve(kMaxHits);
    ours_.reserve(kMaxHits);
    heads_.reserve(kMaxModules);
    end_ = spec.target + std::max<std::uint64_t>(spec.object_bytes, 1);
  }

  MemorySearch run(const RegionQuery& query) {
    const std::uint64_t start = sys_.now_us();
    std::uint64_t at = kLowestAddress;
    bool stop = false;
    while (!stop && at < kAddressEnd && spec_.target != 0) {
      const std::optional<MemoryRegion> r = query(at);
      if (!r || r->size == 0 || r->base + r->size <= at) break;
      at = r->base + r->size;
      if (!r->committed) continue;
      ++out_.regions;
      if (!searchable(*r)) {
        out_.unread_bytes += r->size;
        continue;
      }
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
    out_.hits = std::move(game_);
    for (const MemoryHit& h : ours_) {
      if (out_.hits.size() < kMaxHits) out_.hits.push_back(h);
    }
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
      if (w >= spec_.target && w < end_) hit(at + o, w - spec_.target, dst, o, r);
    }
    return Got::Read;
  }

  [[nodiscard]] bool own(const MemoryRegion& r) const noexcept {
    if (r.allocation_base == 0) return false;
    if (spec_.own_base != 0 && r.allocation_base == spec_.own_base) return true;
    return std::find(spec_.own_bases.begin(), spec_.own_bases.end(), r.allocation_base) != spec_.own_bases.end();
  }

  void hit(std::uint64_t at, std::uint64_t offset, std::span<const std::byte> d, std::size_t o, const MemoryRegion& r) {
    const bool mine = own(r);
    if (offset == 0) {
      ++(mine ? out_.own_found : out_.found);
    } else {
      ++(mine ? out_.own_inside : out_.inside);
    }
    std::vector<MemoryHit>& list = mine ? ours_ : game_;
    if (list.size() >= kMaxHits) return;
    MemoryHit h;
    h.own = mine;
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
  std::vector<MemoryHit> game_, ours_;
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
    if ((*ch & (kScnExecute | kScnCode)) != 0) return SectionKind::Code;
    if ((*ch & kScnWrite) != 0) return SectionKind::Data;
    if ((*ch & kScnRead) != 0) return SectionKind::Rdata;
    return SectionKind::Other;
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
