// The DLL's channels (rings.hpp).
#include "en/shell/rings.hpp"

#include <algorithm>
#include <cstring>

namespace en::shell {

SnapSlot::SnapSlot()
    : buf_{std::make_unique<proto::WorldSnapshot>(), std::make_unique<proto::WorldSnapshot>()} {}

proto::WorldSnapshot* SnapSlot::begin_write() noexcept {
  overwrote_ = false;
  for (int i = 0; i < 2; ++i) {
    std::uint8_t want = Empty;
    if (state_[i].compare_exchange_strong(want, Writing, std::memory_order_acq_rel)) {
      writing_ = i;
      return buf_[i].get();
    }
  }
  // No empty slot: overwrite a Ready one the reader has not taken (not the one it is reading), the older first.
  const int older = order_[0].load(std::memory_order_relaxed) <= order_[1].load(std::memory_order_relaxed) ? 0 : 1;
  for (const int i : {older, 1 - older}) {
    std::uint8_t want = Ready;
    if (state_[i].compare_exchange_strong(want, Writing, std::memory_order_acq_rel)) {
      writing_ = i;
      overwrote_ = true;
      overwritten_.fetch_add(1, std::memory_order_relaxed);
      return buf_[i].get();
    }
  }
  writing_ = -1;
  return nullptr;
}

void SnapSlot::end_write(bool publish) noexcept {
  if (writing_ < 0) return;
  const auto i = static_cast<std::size_t>(writing_);
  writing_ = -1;
  if (!publish) {
    state_[i].store(Empty, std::memory_order_release);
    return;
  }
  if (overwrote_) buf_[i]->gap = true;  // a snapshot the reader never saw was lost
  order_[i].store(++published_, std::memory_order_relaxed);
  state_[i].store(Ready, std::memory_order_release);
}

bool SnapSlot::take(proto::WorldSnapshot& out) noexcept {
  for (int attempt = 0; attempt < 2; ++attempt) {
    int newest = -1;
    std::uint64_t best = 0;
    for (int i = 0; i < 2; ++i) {
      if (state_[i].load(std::memory_order_acquire) == Ready && order_[i].load(std::memory_order_relaxed) >= best) {
        best = order_[i].load(std::memory_order_relaxed);
        newest = i;
      }
    }
    if (newest < 0) return false;
    std::uint8_t want = Ready;
    if (!state_[newest].compare_exchange_strong(want, Reading, std::memory_order_acq_rel)) continue;  // overwritten
    // The slot is ours now, and its order is what the writer published last into it: between the scan and the claim
    // the writer may have overwritten and published it again, newer than the other slot it left Ready.
    best = order_[newest].load(std::memory_order_relaxed);
    out = *buf_[newest];
    // An older Ready one is stale now: free it too, so the writer always finds room, and so that the next take never
    // answers a snapshot older than this one. It is claimed before its order is read, so that one the writer has
    // just published again (newer) is never freed unread.
    const int other = 1 - newest;
    std::uint8_t ready = Ready;
    if (state_[other].compare_exchange_strong(ready, Reading, std::memory_order_acq_rel)) {
      state_[other].store(order_[other].load(std::memory_order_relaxed) < best ? Empty : Ready,
                          std::memory_order_release);
    }
    state_[newest].store(Empty, std::memory_order_release);
    return true;
  }
  return false;
}

LogRing::LogRing() : cells_(std::make_unique<std::array<Cell, kSlots>>()) {
  for (std::size_t i = 0; i < kSlots; ++i) (*cells_)[i].seq.store(i, std::memory_order_relaxed);
}

bool LogRing::try_push(std::string_view text, std::uint64_t at_ms) noexcept {
  std::size_t pos = enqueue_.load(std::memory_order_relaxed);
  for (;;) {
    Cell& c = (*cells_)[pos & (kSlots - 1)];
    const std::size_t seq = c.seq.load(std::memory_order_acquire);
    const auto diff = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos);
    if (diff == 0) {
      if (enqueue_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
        const std::size_t n = std::min(text.size(), c.entry.text.size());
        std::memcpy(c.entry.text.data(), text.data(), n);
        c.entry.len = static_cast<std::uint16_t>(n);
        c.entry.at_ms = at_ms;
        c.seq.store(pos + 1, std::memory_order_release);
        return true;
      }
    } else if (diff < 0) {
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return false;
    } else {
      pos = enqueue_.load(std::memory_order_relaxed);
    }
  }
}

bool LogRing::try_pop(LogEntry& out) noexcept {
  Cell& c = (*cells_)[dequeue_ & (kSlots - 1)];
  const std::size_t seq = c.seq.load(std::memory_order_acquire);
  if (static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(dequeue_ + 1) < 0) return false;
  out = c.entry;
  c.seq.store(dequeue_ + kSlots, std::memory_order_release);
  ++dequeue_;
  return true;
}

}  // namespace en::shell
