// The probe option's searches over the process's regions (probe.hpp; EN_PROBE only): which regions a search reads,
// and the object scan. Numbers only: nothing keeps a byte that was read.
#include "en/gal/probe.hpp"

#if EN_PROBE

#include <algorithm>
#include <cstring>

namespace en::gal::probe {

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

// ---- The object scan ----

ObjectScan::ObjectScan(RegionQuery regions, std::vector<Target> targets, std::vector<std::uint32_t> primary,
                       std::uint64_t base, std::uint64_t size, std::vector<std::uint64_t> own)
    : regions_(std::move(regions)),
      targets_(std::move(targets)),
      primary_(std::move(primary)),
      base_(base),
      size_(size),
      own_(std::move(own)) {
  std::sort(targets_.begin(), targets_.end(), [](const Target& a, const Target& b) { return a.vtable < b.vtable; });
  std::sort(primary_.begin(), primary_.end());
  chunk_.assign(kChunk, std::byte{0});
  back_.assign(kBack, std::byte{0});
  if (targets_.empty() || !regions_) done_.store(true, std::memory_order_release);
}

bool ObjectScan::ours(std::uint64_t allocation_base) const noexcept {
  return std::find(own_.begin(), own_.end(), allocation_base) != own_.end();
}

bool ObjectScan::add(const Node& node) noexcept {
  const std::size_t n = node_count_.load(std::memory_order_relaxed);
  if (n >= kMaxNodes) return false;
  nodes_[n] = node;
  node_count_.store(n + 1, std::memory_order_release);  // a reader sees a node only once it is whole
  return true;
}

std::optional<ObjectScan::Owner> ObjectScan::owner_of(std::uint64_t at, std::uint64_t floor,
                                                      const proto::GameThread& gt, GuardedReader& reader) {
  const std::uint64_t from = std::max(floor, at > kBack ? at - kBack : 0) & ~std::uint64_t{7};
  if (from >= at) return std::nullopt;
  const std::size_t len = static_cast<std::size_t>(at - from) & ~std::size_t{7};
  if (len < 8) return std::nullopt;
  if (!reader.copy_into(gt, proto::Group::Session, from, std::span<std::byte>(back_.data(), len), 8)) {
    return std::nullopt;
  }
  for (std::size_t k = len; k >= 8; k -= 8) {
    std::uint64_t w = 0;
    std::memcpy(&w, back_.data() + k - 8, 8);
    const std::uint64_t d = w - base_;
    if (w < base_ || d >= size_) continue;
    if (std::binary_search(primary_.begin(), primary_.end(), static_cast<std::uint32_t>(d))) {
      return Owner{from + k - 8, static_cast<std::uint32_t>(d)};
    }
  }
  return std::nullopt;
}

void ObjectScan::look(std::span<const std::byte> chunk, std::uint64_t at, const proto::GameThread& gt,
                      GuardedReader& reader) {
  for (std::size_t k = 0; k + 8 <= chunk.size(); k += 8) {
    std::uint64_t w = 0;
    std::memcpy(&w, chunk.data() + k, 8);
    if (sweep_ == 0) {
      const std::uint64_t d = w - base_;
      if (w < base_ || d >= size_) continue;
      const auto it = std::lower_bound(targets_.begin(), targets_.end(), static_cast<std::uint32_t>(d),
                                       [](const Target& t, std::uint32_t v) { return t.vtable < v; });
      if (it == targets_.end() || it->vtable != d) continue;
      if (node_count_.load(std::memory_order_relaxed) >= kMaxObjects) continue;
      Node n;
      n.lead = it->lead;
      n.vtable = it->vtable;
      n.address = at + k;
      (void)add(n);
      continue;
    }
    if (w < want_lo_ || w > want_hi_) continue;
    for (std::size_t i = 0; i < want_count_; ++i) {
      Want& want = want_[i];
      if (w != want.address) continue;
      Node& parent = nodes_[want.node];
      if (parent.holders < 0xFFFFFFFFu) ++parent.holders;
      if (want.kept >= kMaxPerParent) break;
      const std::uint64_t holder = at + k;
      Node n;
      n.level = static_cast<std::uint8_t>(sweep_);
      n.parent = want.node;
      n.lead = parent.lead;
      n.address = holder;
      // The holder is no object's first word (it holds an address, not a table of methods): its object starts before.
      if (const std::optional<Owner> owner = owner_of(holder, region_base_, gt, reader)) {
        n.address = owner->address;
        n.vtable = owner->vtable;
        n.offset = static_cast<std::uint32_t>(holder - owner->address);
      }
      // One object that holds the address twice is one node.
      bool known = false;
      const std::size_t count = node_count_.load(std::memory_order_relaxed);
      for (std::size_t j = 0; j < count; ++j) {
        known = known || (nodes_[j].level == n.level && nodes_[j].parent == n.parent && nodes_[j].address == n.address);
      }
      if (!known && add(n)) ++want.kept;
      break;
    }
  }
}

void ObjectScan::end_sweep(const proto::GameThread& gt, GuardedReader& reader) {
  sweeps_.fetch_add(1, std::memory_order_acq_rel);
  const std::size_t count = node_count_.load(std::memory_order_relaxed);
  if (sweep_ == 1) {
    // A level-0 node that nothing holds: the object it lies inside, if one starts within kBack before it.
    for (std::size_t i = 0; i < count; ++i) {
      const Node found = nodes_[i];
      if (found.level != 0 || found.holders != 0) continue;
      const std::optional<MemoryRegion> r = regions_(found.address);
      if (!r || found.address < r->base) continue;
      const std::optional<Owner> owner = owner_of(found.address, r->base, gt, reader);
      if (!owner) continue;
      Node n;
      n.level = 1;
      n.embedded = true;
      n.parent = static_cast<std::uint16_t>(i);
      n.lead = found.lead;
      n.address = owner->address;
      n.vtable = owner->vtable;
      n.offset = static_cast<std::uint32_t>(found.address - owner->address);
      (void)add(n);
    }
  }
  // The next sweep looks for what holds the nodes of the level just found: objects only.
  want_count_ = 0;
  want_lo_ = ~std::uint64_t{0};
  want_hi_ = 0;
  const std::size_t now = node_count_.load(std::memory_order_relaxed);
  for (std::size_t i = 0; i < now && sweep_ < kLevels; ++i) {
    const Node& n = nodes_[i];
    if (n.level != sweep_ || (n.level != 0 && n.vtable == 0)) continue;
    want_[want_count_++] = Want{n.address, static_cast<std::uint16_t>(i), 0};
    want_lo_ = std::min(want_lo_, n.address);
    want_hi_ = std::max(want_hi_, n.address);
  }
  if (want_count_ == 0) {
    done_.store(true, std::memory_order_release);
    return;
  }
  ++sweep_;
  cursor_ = kLowestAddress;
  region_end_ = 0;
  in_region_ = false;
}

bool ObjectScan::step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t chunks) {
  if (done_.load(std::memory_order_relaxed)) return true;
  std::uint32_t asked = 0;
  for (std::uint32_t n = 0; n < chunks;) {
    if (cursor_ >= region_end_) {
      // The next region: one the scan may read, or one to pass over. A call asks for a bounded number.
      if (++asked > 256) return false;
      const std::optional<MemoryRegion> r = cursor_ < kAddressEnd ? regions_(cursor_) : std::nullopt;
      if (!r || r->size == 0 || r->base + r->size <= cursor_ || r->base >= kAddressEnd) {
        end_sweep(gt, reader);
        if (done_.load(std::memory_order_relaxed)) return true;
        continue;
      }
      const std::uint64_t end = r->base + r->size;
      const bool image = r->base < base_ + size_ && end > base_;
      const bool readable = searchable(*r) && r->kind == RegionKind::Private &&
                            (r->protect & 0xFFu) == kPageReadWrite && !image && !ours(r->allocation_base);
      if (!readable) {
        cursor_ = end;
        region_end_ = 0;
        in_region_ = false;
        continue;
      }
      // A region is counted once, though it is asked for again as the scan goes through it.
      if (r->base != region_base_ || !in_region_) regions_read_.fetch_add(1, std::memory_order_relaxed);
      in_region_ = true;
      cursor_ = std::max(cursor_, r->base);
      region_base_ = r->base;
      // What was asked holds for the next kAskEvery bytes; then the region is asked for again, where the scan stands.
      region_end_ = std::min<std::uint64_t>(end, cursor_ + kAskEvery);
    }
    const std::size_t len = static_cast<std::size_t>(std::min<std::uint64_t>(kChunk, region_end_ - cursor_)) &
                            ~std::size_t{7};
    if (len < 8) {
      cursor_ = region_end_;
      continue;
    }
    ++n;
    const std::span<std::byte> piece(chunk_.data(), len);
    if (!reader.copy_into(gt, proto::Group::Session, cursor_, piece, 8)) {
      if (reader.spent()) return false;  // the same piece again at the next step
      unreadable_.fetch_add(1, std::memory_order_relaxed);
      cursor_ += len;
      continue;
    }
    bytes_.fetch_add(len, std::memory_order_relaxed);
    look(piece, cursor_, gt, reader);
    cursor_ += len;
  }
  return false;
}

}  // namespace en::gal::probe

#endif
