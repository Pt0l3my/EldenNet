// The probe option's searches (probe.hpp; EN_PROBE only).
#include "en/gal/probe.hpp"

#if EN_PROBE

#include <algorithm>
#include <cstring>

namespace en::gal::probe {

namespace {

// Each writable data section's 8-aligned values, read through the image in 64 KiB pieces; `take(rva, value)` answers
// false to stop. Answers the bytes that could not be read.
template <class Take>
std::uint64_t data_values(const SysLayer& sys, const ImageInfo& image, Take take) {
  std::uint64_t unreadable = 0;
  std::vector<std::byte> chunk(1u << 16);
  for (const Section& s : image.sections) {
    if ((s.characteristics & kSectionWrite) == 0 || (s.characteristics & kSectionExecute) != 0) continue;
    for (std::uint32_t at = 0; at + 8 <= s.mapped_size(); at += static_cast<std::uint32_t>(chunk.size())) {
      const std::uint32_t len =
          std::min<std::uint32_t>(static_cast<std::uint32_t>(chunk.size()), s.mapped_size() - at) & ~7u;
      const std::span<std::byte> dst(chunk.data(), len);
      if (!sys.image().read(std::uint64_t{s.rva} + at, dst)) {
        unreadable += len;
        continue;
      }
      for (std::uint32_t k = 0; k + 8 <= len; k += 8) {
        std::uint64_t v = 0;
        std::memcpy(&v, dst.data() + k, 8);
        if (!take(s.rva + at + k, v)) return unreadable;
      }
    }
  }
  return unreadable;
}

}  // namespace

HeapSlots heap_slots(const SysLayer& sys, const ImageInfo& image, std::size_t max) {
  HeapSlots out;
  const std::uint64_t base = sys.module_base();
  const std::uint64_t image_end = base + image.size_of_image;
  out.unreadable = data_values(sys, image, [&](std::uint32_t rva, std::uint64_t v) {
    if (out.slots.size() >= max) return false;
    if (v < kLowestAddress || v > kAddressEnd || v % 8 != 0 || (v >= base && v < image_end)) return true;
    out.slots.push_back(rva);
    out.values.push_back(v);
    return true;
  });
  return out;
}

Holding slots_holding(const SysLayer& sys, const ImageInfo& image, std::uint64_t value, std::size_t max) {
  Holding out;
  (void)data_values(sys, image, [&](std::uint32_t rva, std::uint64_t v) {
    if (v == value) {
      ++out.count;
      if (out.slots.size() < max) out.slots.push_back(rva);
    }
    return true;
  });
  return out;
}

SlotSearch::SlotSearch(const SysLayer& sys, const Discovery& discovery, roles::Cls cls, std::size_t max_candidates)
    : SlotSearch(sys, discovery.image(), discovery.vtables(cls), max_candidates) {}

SlotSearch::SlotSearch(const SysLayer& sys, const ImageInfo& image, const VtableSet& set, std::size_t max_candidates)
    : set_(set) {
  HeapSlots found = heap_slots(sys, image, max_candidates);
  slots_ = std::move(found.slots);
  values_ = std::move(found.values);
  unreadable_.store(found.unreadable, std::memory_order_relaxed);
}

bool SlotSearch::step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies) {
  std::size_t next = next_.load(std::memory_order_relaxed);
  for (std::uint32_t n = 0; n < copies && next < slots_.size(); ++n, ++next) {
    // The group is only the reader's bookkeeping here: no breaker counts the probe's reads.
    const auto vt = reader.copy<std::uint64_t>(gt, proto::Group::Status, values_[next]);
    if (!vt) {
      if (reader.spent()) break;
      unreadable_.fetch_add(1, std::memory_order_relaxed);
      continue;
    }
    const std::size_t count = found_count_.load(std::memory_order_relaxed);
    if (set_.contains(*vt) && count < kMaxFound) {
      found_[count] = slots_[next];
      found_count_.store(count + 1, std::memory_order_release);
    }
  }
  next_.store(next, std::memory_order_release);
  return next >= slots_.size();
}

// ---- The rising-counter search ----

RisingSearch::RisingSearch(HeapSlots candidates, std::uint32_t span, std::size_t max_candidates)
    : span_(std::min<std::uint32_t>(std::max<std::uint32_t>(span, 8), kMaxCopyBytes) & ~3u) {
  const std::size_t n = std::min(candidates.slots.size(), max_candidates);
  slots_.assign(candidates.slots.begin(), candidates.slots.begin() + static_cast<std::ptrdiff_t>(n));
  values_.assign(candidates.values.begin(), candidates.values.begin() + static_cast<std::ptrdiff_t>(n));
  last_.assign(n * span_, std::byte{0});
  last_us_.assign(n, 0);
  dead_.assign(n, 0);
  scratch_.assign(span_, std::byte{0});
}

void RisingSearch::compare(std::size_t i, std::span<const std::byte> now, std::uint64_t now_us) noexcept {
  const std::span<std::byte> before(last_.data() + i * span_, span_);
  const std::uint64_t then_us = last_us_[i];
  if (then_us != 0 && now_us > then_us) {
    const std::uint64_t elapsed_ms = (now_us - then_us) / 1000;
    for (std::uint32_t off = 0; off + 4 <= span_; off += 4) {
      std::uint32_t a = 0, b = 0;
      std::memcpy(&a, before.data() + off, 4);
      std::memcpy(&b, now.data() + off, 4);
      if (a == b && a == 0) continue;
      const std::uint64_t delta = b >= a ? b - a : 0;
      const bool rose = b > a && elapsed_ms >= 500 && delta * 5 >= elapsed_ms * 4 && delta * 5 <= elapsed_ms * 6;
      // Only a field that has risen once is followed; afterwards each pass is counted for it.
      std::size_t count = hit_count_.load(std::memory_order_relaxed);
      Hit* hit = nullptr;
      for (std::size_t h = 0; h < count; ++h) {
        if (hits_[h].slot == slots_[i] && hits_[h].offset == off) hit = &hits_[h];
      }
      if (hit == nullptr) {
        if (!rose || count >= kMaxHits) continue;
        hits_[count] = Hit{slots_[i], off, 0, 0, 0};
        hit = &hits_[count];
        hit_count_.store(count + 1, std::memory_order_release);
      }
      if (rose) {
        ++hit->rose;
      } else if (a == b) {
        ++hit->still;
      } else {
        ++hit->other;
      }
    }
  }
  std::memcpy(before.data(), now.data(), span_);
  last_us_[i] = now_us;
}

bool RisingSearch::step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies,
                        std::uint64_t now_us) {
  if (slots_.empty()) return false;
  bool ended = false;
  std::uint32_t made = 0;
  // A candidate copied less than a second ago is passed over: two copies closer than that say nothing of a clock.
  for (std::size_t visits = 0; made < copies && visits < slots_.size(); ++visits) {
    const std::size_t i = next_;
    if (dead_[i] == 0 && (last_us_[i] == 0 || now_us - last_us_[i] >= kMinIntervalUs)) {
      const auto done = reader.copy_into(gt, proto::Group::Status, values_[i], scratch_, 8);
      if (!done && reader.spent()) break;
      ++made;
      if (done) {
        compare(i, scratch_, now_us);
      } else {
        dead_[i] = 1;  // a fault is not tried again: no stream of exceptions on the game thread
        unreadable_.fetch_add(1, std::memory_order_relaxed);
      }
    }
    if (++next_ >= slots_.size()) {
      next_ = 0;
      passes_.fetch_add(1, std::memory_order_acq_rel);
      ended = true;
    }
  }
  return ended;
}

// ---- The hub census ----

namespace {
constexpr std::size_t kSeenSlots = 1u << 15;  // a power of two, twice the queue's cap and more
}  // namespace

Census::Census(std::vector<std::uint64_t> vtables, std::uint64_t hub_static, std::uint64_t module_base,
               std::uint64_t image_size, std::vector<AddressRange> data)
    : vtables_(std::move(vtables)),
      data_(std::move(data)),
      hub_static_(hub_static),
      base_(module_base),
      image_end_(module_base + image_size) {
  std::sort(vtables_.begin(), vtables_.end());
  std::size_t total = 0;
  for (const std::size_t c : kCap) total += c;
  queue_.reserve(total);
  scratch_.assign(kSpan[0], std::byte{0});
  seen_.assign(kSeenSlots, 0);
}

bool Census::follow(std::uint64_t v) const noexcept {
  if (v < kLowestAddress || v > kAddressEnd || v % 8 != 0) return false;
  if (v < base_ || v >= image_end_) return true;
  return std::any_of(data_.begin(), data_.end(), [&](const AddressRange& r) { return v >= r.begin && v < r.end; });
}

void Census::found(std::uint64_t vtable) noexcept {
  objects_.fetch_add(1, std::memory_order_relaxed);
  const std::size_t count = found_count_.load(std::memory_order_relaxed);
  for (std::size_t i = 0; i < count; ++i) {
    if (found_[i] == vtable) {
      ++found_n_[i];
      return;
    }
  }
  if (count >= kMaxFound) return;
  found_[count] = vtable;
  found_n_[count] = 1;
  found_count_.store(count + 1, std::memory_order_release);
}

bool Census::step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies) {
  if (done()) return true;
  if (!started_) {
    // The hub's instance, from its static (R5: read again now, on the game thread).
    started_ = true;
    const auto inst = reader.copy<std::uint64_t>(gt, proto::Group::Status, hub_static_);
    copies_.fetch_add(1, std::memory_order_relaxed);
    if (!inst || !follow(*inst)) {
      faults_.fetch_add(1, std::memory_order_relaxed);
      done_.store(true, std::memory_order_release);
      return true;
    }
    queue_.push_back(Item{*inst, kSpan[0], 0});
    queued_[0] = 1;
  }
  for (std::uint32_t n = 0; n < copies && next_ < queue_.size(); ++n) {
    const Item item = queue_[next_];
    const std::span<std::byte> dst(scratch_.data(), item.span);
    const auto ok = reader.copy_into(gt, proto::Group::Status, item.address, dst, 8);
    if (!ok && reader.spent()) break;
    copies_.fetch_add(1, std::memory_order_relaxed);
    if (!ok) {
      if (item.span > 16) {
        queue_[next_].span = item.span / 2;  // the same object again, shorter
        continue;
      }
      ++next_;
      faults_.fetch_add(1, std::memory_order_relaxed);
      continue;
    }
    ++next_;
    std::uint64_t first = 0;
    std::memcpy(&first, dst.data(), 8);
    if (item.level > 0 && std::binary_search(vtables_.begin(), vtables_.end(), first)) found(first);
    const std::size_t child = item.level + 1u;
    if (child >= kSpan.size()) continue;
    for (std::size_t at = 0; at + 8 <= dst.size(); at += 8) {
      std::uint64_t v = 0;
      std::memcpy(&v, dst.data() + at, 8);
      if (!follow(v) || queued_[child] >= kCap[child] || queue_.size() >= queue_.capacity()) continue;
      // A small open-addressed set: an address is queued once.
      std::size_t h = static_cast<std::size_t>((v >> 3) * 0x9E3779B97F4A7C15ull) & (kSeenSlots - 1);
      bool seen = false;
      for (std::size_t probe = 0; probe < 16; ++probe, h = (h + 1) & (kSeenSlots - 1)) {
        if (seen_[h] == v) {
          seen = true;
          break;
        }
        if (seen_[h] == 0) {
          seen_[h] = v;
          break;
        }
      }
      if (seen) continue;
      queue_.push_back(Item{v, kSpan[child], static_cast<std::uint8_t>(child)});
      ++queued_[child];
    }
  }
  if (next_ >= queue_.size()) done_.store(true, std::memory_order_release);
  return done();
}

std::vector<std::uint32_t> vtable_offsets(std::span<const std::byte> obj, const VtableSet& set) {
  std::vector<std::uint32_t> out;
  for (std::size_t at = 0; at + 8 <= obj.size(); at += 8) {
    std::uint64_t v = 0;
    std::memcpy(&v, obj.data() + at, 8);
    if (set.contains(v)) out.push_back(static_cast<std::uint32_t>(at));
  }
  return out;
}

std::vector<std::uint32_t> meter_offsets(std::span<const std::byte> obj, bool max_first) {
  std::vector<std::uint32_t> out;
  for (std::size_t at = 0; at + 8 <= obj.size(); at += 4) {
    std::int32_t a = 0, b = 0;
    std::memcpy(&a, obj.data() + at, 4);
    std::memcpy(&b, obj.data() + at + 4, 4);
    const std::int32_t max = max_first ? a : b;
    const std::int32_t cur = max_first ? b : a;
    if (max > 0 && max <= 100000 && cur >= 0 && cur <= max) out.push_back(static_cast<std::uint32_t>(at));
  }
  return out;
}

std::vector<std::uint32_t> utf16_offsets(std::span<const std::byte> obj, std::u16string_view text) {
  std::vector<std::uint32_t> out;
  const std::size_t bytes = 2 * (text.size() + 1);
  if (text.empty()) return out;
  for (std::size_t at = 0; at + bytes <= obj.size(); at += 2) {
    bool match = true;
    for (std::size_t i = 0; i <= text.size() && match; ++i) {
      const char16_t want = i < text.size() ? text[i] : u'\0';
      const auto got = static_cast<char16_t>(static_cast<unsigned>(obj[at + 2 * i]) |
                                             (static_cast<unsigned>(obj[at + 2 * i + 1]) << 8));
      match = got == want;
    }
    if (match) out.push_back(static_cast<std::uint32_t>(at));
  }
  return out;
}

std::vector<std::uint32_t> u32_offsets(std::span<const std::byte> obj, std::uint32_t value) {
  std::vector<std::uint32_t> out;
  for (std::size_t at = 0; at + 4 <= obj.size(); at += 4) {
    std::uint32_t v = 0;
    std::memcpy(&v, obj.data() + at, 4);
    if (v == value) out.push_back(static_cast<std::uint32_t>(at));
  }
  return out;
}

}  // namespace en::gal::probe

#endif
