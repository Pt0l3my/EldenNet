// The probe option's searches (probe.hpp; EN_PROBE only).
#include "en/gal/probe.hpp"

#if EN_PROBE

#include <algorithm>
#include <cstring>

namespace en::gal::probe {

SlotSearch::SlotSearch(const SysLayer& sys, const Discovery& discovery, roles::Cls cls, std::size_t max_candidates)
    : set_(discovery.vtables(cls)), base_(discovery.module_base()) {
  const ImageInfo& image = discovery.image();
  const std::uint64_t image_end = base_ + image.size_of_image;
  std::vector<std::byte> chunk(1u << 16);
  for (const Section& s : image.sections) {
    if ((s.characteristics & kSectionWrite) == 0 || (s.characteristics & kSectionExecute) != 0) continue;
    for (std::uint32_t at = 0; at + 8 <= s.mapped_size() && slots_.size() < max_candidates;
         at += static_cast<std::uint32_t>(chunk.size())) {
      const std::uint32_t len = std::min<std::uint32_t>(static_cast<std::uint32_t>(chunk.size()), s.mapped_size() - at) & ~7u;
      const std::span<std::byte> dst(chunk.data(), len);
      if (!sys.image().read(std::uint64_t{s.rva} + at, dst)) {
        unreadable_ += len;
        continue;
      }
      for (std::uint32_t k = 0; k + 8 <= len && slots_.size() < max_candidates; k += 8) {
        std::uint64_t v = 0;
        std::memcpy(&v, dst.data() + k, 8);
        if (v < kLowestAddress || v > kAddressEnd || v % 8 != 0 || (v >= base_ && v < image_end)) continue;
        slots_.push_back(s.rva + at + k);
        values_.push_back(v);
      }
    }
  }
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
