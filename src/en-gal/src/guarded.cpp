// GuardedReader and ObjView (guarded.hpp; ARCH §6.5), the stack table (sys.hpp), and the group book, breakers and
// histogram (health.hpp; ARCH §6.7, C13).
#include <algorithm>
#include <bit>
#include <limits>

#include "en/gal/guarded.hpp"
#include "en/gal/health.hpp"

namespace en::gal {

using proto::Expected;
using proto::Group;
using proto::Unexpected;

// ---- The stack table ----

bool StackRanges::add(AddressRange r) noexcept {
  if (r.end <= r.begin || n_ >= kMax) return false;
  for (std::size_t i = 0; i < n_; ++i) {
    if (r_[i].begin == r.begin && r_[i].end == r.end) return true;
  }
  r_[n_++] = r;
  return true;
}

void StackRanges::remove(AddressRange r) noexcept {
  std::size_t kept = 0;
  for (std::size_t i = 0; i < n_; ++i) {
    if (!r_[i].overlaps(r.begin, r.end - r.begin)) r_[kept++] = r_[i];
  }
  n_ = kept;
}

bool StackRanges::overlaps(std::uint64_t at, std::uint64_t len) const noexcept {
  for (std::size_t i = 0; i < n_; ++i) {
    if (r_[i].overlaps(at, len)) return true;
  }
  return false;
}

const char* to_string(SysError e) noexcept {
  switch (e) {
    case SysError::Unsupported: return "unsupported";
    case SysError::Twice: return "twice";
    case SysError::BadShape: return "bad_shape";
    case SysError::Faulted: return "faulted";
    case SysError::NotInit: return "not_init";
    case SysError::Unreadable: return "unreadable";
  }
  return "unsupported";
}

// ---- The reader ----

const char* to_string(FaultKind k) noexcept {
  switch (k) {
    case FaultKind::Implausible: return "implausible";
    case FaultKind::Stack: return "stack";
    case FaultKind::Copy: return "copy";
    case FaultKind::Budget: return "budget";
    case FaultKind::WrongThread: return "wrong_thread";
    case FaultKind::Injected: return "injected";
  }
  return "implausible";
}

std::optional<FaultKind> implausible(std::uint64_t address, std::uint64_t len, std::uint32_t align,
                                     const StackRanges& stacks) noexcept {
  if (address == 0 || address < kLowestAddress) return FaultKind::Implausible;
  if (len > kMaxCopyBytes || address > kAddressEnd || len > kAddressEnd - address) return FaultKind::Implausible;
  if (align > 1 && address % align != 0) return FaultKind::Implausible;
  if (stacks.overlaps(address, len)) return FaultKind::Stack;
  return std::nullopt;
}

void GuardedReader::begin_tick() noexcept {
  copies_ = 0;
  bytes_ = 0;
  spent_ = false;
}

void GuardedReader::bind_thread() noexcept {
  thread_ = sys_.thread_id();
  bound_ = true;
}

ReadFault GuardedReader::fail(FaultKind kind, std::uint32_t code) noexcept {
  faults_[static_cast<std::size_t>(kind)].fetch_add(1, std::memory_order_relaxed);
  return ReadFault{kind, code};
}

Expected<void, ReadFault> GuardedReader::copy_into(const proto::GameThread& gt, Group group, std::uint64_t address,
                                                   std::span<std::byte> dst, std::uint32_t align) noexcept {
  (void)gt;  // the token is the proof that this runs on the game thread (R1); it carries nothing
#if !defined(NDEBUG) || EN_PROBE
  if (bound_ && sys_.thread_id() != thread_) return Unexpected{fail(FaultKind::WrongThread)};
#endif
  std::fill(dst.begin(), dst.end(), std::byte{0});
  if (spent_) return Unexpected{fail(FaultKind::Budget)};
  if (copies_ + 1 > budget_.copies || dst.size() > budget_.bytes - std::min<std::uint64_t>(bytes_, budget_.bytes)) {
    spent_ = true;
    return Unexpected{fail(FaultKind::Budget)};
  }
#if EN_PROBE
  // The probe's injected fault, counted as a read of its group.
  if (injection_.every != 0 && group == injection_.group) {
    std::uint32_t& n = reads_[static_cast<std::size_t>(group)];
    if (++n >= injection_.every) {
      n = 0;
      if (injection_.address == 0) return Unexpected{fail(FaultKind::Injected)};
      address = injection_.address;
    }
  }
#else
  (void)group;
#endif
  if (const auto why = implausible(address, dst.size(), align, sys_.stacks())) {
    // A range of a thread that has ended (its memory now the game's) is forgotten, and the read goes on (WHY§190).
    if (*why != FaultKind::Stack || !sys_.forget_stale_stacks(address, dst.size())) return Unexpected{fail(*why)};
    stale_stacks_.fetch_add(1, std::memory_order_relaxed);
  }
  ++copies_;
  bytes_ += static_cast<std::uint32_t>(dst.size());
  total_copies_.fetch_add(1, std::memory_order_relaxed);
  const CopyOutcome out = sys_.copier().copy(address, dst);
  if (!out.copied) return Unexpected{fail(FaultKind::Copy, out.code)};
  return {};
}

// ---- ObjView ----

std::optional<std::uint64_t> ObjView::raw(std::uint32_t off, std::uint32_t size) const noexcept {
  if (size == 0 || size > 8 || off > bytes_.size() || size > bytes_.size() - off) return std::nullopt;
  std::uint64_t v = 0;
  std::memcpy(&v, bytes_.data() + off, size);  // little-endian, as x64 is
  return v;
}

std::optional<std::int64_t> ObjView::integer(const FieldLayout* f) const noexcept {
  if (f == nullptr) return std::nullopt;
  switch (f->type) {
    case FieldType::U8:
    case FieldType::Bool8:
    case FieldType::U16:
    case FieldType::U32: {
      const auto v = raw(f->at(), f->size);
      if (!v) return std::nullopt;
      return static_cast<std::int64_t>(*v);
    }
    case FieldType::Bits: {
      if (f->width == 0 || f->width > 32 || f->shift > 63) return std::nullopt;
      const auto v = raw(f->at(), f->size);
      if (!v) return std::nullopt;
      return static_cast<std::int64_t>((*v >> f->shift) & ((std::uint64_t{1} << f->width) - 1));
    }
    case FieldType::I32: {
      const auto v = raw(f->at(), 4);
      if (!v) return std::nullopt;
      return static_cast<std::int64_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(*v)));
    }
    case FieldType::U64:
    case FieldType::Handle:
    case FieldType::Ptr: {
      const auto v = raw(f->at(), 8);
      if (!v || *v > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return std::nullopt;
      return static_cast<std::int64_t>(*v);
    }
    default: return std::nullopt;
  }
}

std::optional<std::uint64_t> ObjView::u64(const FieldLayout* f) const noexcept {
  if (f == nullptr) return std::nullopt;
  switch (f->type) {
    case FieldType::U64:
    case FieldType::Handle:
    case FieldType::Ptr:
    case FieldType::DlVector:
    case FieldType::DlMap: return raw(f->at(), 8);
    default: {
      const auto v = integer(f);
      if (!v || *v < 0) return std::nullopt;
      return static_cast<std::uint64_t>(*v);
    }
  }
}

std::optional<std::uint64_t> ObjView::second(const FieldLayout* f) const noexcept {
  if (f == nullptr || (f->type != FieldType::DlVector && f->type != FieldType::DlMap) || f->at() > 0xFFFF'FFF0) {
    return std::nullopt;
  }
  return raw(f->at() + 8, 8);
}

std::optional<bool> ObjView::bit(const FieldLayout* f) const noexcept {
  if (f == nullptr) return std::nullopt;
  if (f->type == FieldType::Bool8) {
    const auto v = raw(f->at(), 1);
    if (!v) return std::nullopt;
    return *v != 0;
  }
  if (f->type != FieldType::Bit) return std::nullopt;
  const auto v = raw(f->at(), 1);
  if (!v) return std::nullopt;
  return ((*v >> f->bit) & 1u) != 0;
}

std::optional<float> ObjView::f32(const FieldLayout* f, std::size_t i) const noexcept {
  if (f == nullptr) return std::nullopt;
  const std::size_t n = f->type == FieldType::F32 ? 1 : f->type == FieldType::Vec3f ? 3 : f->type == FieldType::Mat4f ? 16 : 0;
  if (i >= n) return std::nullopt;
  const auto v = raw(f->at() + static_cast<std::uint32_t>(4 * i), 4);
  if (!v) return std::nullopt;
  return std::bit_cast<float>(static_cast<std::uint32_t>(*v));
}

std::optional<std::size_t> ObjView::utf16(const FieldLayout* f, std::span<char16_t> out) const noexcept {
  if (f == nullptr || f->type != FieldType::Utf16) return std::nullopt;
  for (std::size_t i = 0; i < f->len; ++i) {
    const auto v = raw(f->at() + static_cast<std::uint32_t>(2 * i), 2);
    if (!v) return std::nullopt;
    if (*v == 0) return i;
    if (i >= out.size()) return std::nullopt;
    out[i] = static_cast<char16_t>(*v);
  }
  return std::nullopt;  // no NUL within the field
}

bool ObjView::in_range(const FieldLayout* f, std::int64_t v) noexcept {
  return f == nullptr || !f->range || (v >= f->range->first && v <= f->range->second);
}

// ---- Breakers, the group book and the histogram ----

bool Breaker::fault(std::uint64_t now_ms) noexcept {
  ++total_;
  if (tripped_) return false;
  std::uint32_t recent = 1;  // this one
  for (std::uint32_t i = 0; i < held_; ++i) {
    if (now_ms - last_[i] < kTripWindowMs) ++recent;
  }
  for (std::size_t i = last_.size() - 1; i > 0; --i) last_[i] = last_[i - 1];
  last_[0] = now_ms;
  held_ = std::min<std::uint32_t>(held_ + 1, static_cast<std::uint32_t>(last_.size()));
  if (recent >= kTripFaults) {
    tripped_ = true;
    return true;
  }
  return false;
}

const char* to_string(GroupWhy w) noexcept {
  switch (w) {
    case GroupWhy::None: return "none";
    case GroupWhy::NotDiscovered: return "not_discovered";
    case GroupWhy::PhaseA: return "phase_a";
    case GroupWhy::PhaseB: return "phase_b";
    case GroupWhy::Breaker: return "breaker";
    case GroupWhy::Off: return "off";
  }
  return "none";
}

void GroupBook::set_ok(Group g) noexcept {
  Entry& e = at(g);
  if (e.state == proto::GroupState::Untested) e.state = proto::GroupState::Ok;
}

bool GroupBook::trip(Group g, GroupWhy why) noexcept {
  Entry& e = at(g);
  if (e.state == proto::GroupState::Tripped) return false;
  e.state = proto::GroupState::Tripped;
  e.why = why;
  return true;
}

bool GroupBook::hard_fault(Group g, std::uint64_t now_ms) noexcept {
  Entry& e = at(g);
  if (e.faults < 255) ++e.faults;
  if (e.state == proto::GroupState::Tripped) return false;
  if (e.breaker.fault(now_ms)) return trip(g, GroupWhy::Breaker);
  return false;
}

void GroupBook::begin_tick() noexcept {
  for (Entry& e : e_) e.faults = 0;
}

void GroupBook::fill(proto::ReadHealth& out) const noexcept {
  for (std::size_t i = 0; i < proto::kGroupCount; ++i) {
    out.groups[i] = proto::GroupHealth{e_[i].state, e_[i].phase_b, e_[i].faults};
  }
}

std::size_t Histogram::bucket_of(std::uint64_t us) noexcept {
  if (us < 4) return static_cast<std::size_t>(us);
  const auto octave = static_cast<std::size_t>(std::bit_width(us) - 1);  // us in [2^octave, 2^(octave+1)), octave >= 2
  const auto sub = static_cast<std::size_t>((us >> (octave - 2)) & 3u);
  return std::min<std::size_t>(4 * (octave - 1) + sub, kBuckets - 1);
}

std::uint64_t Histogram::upper_of(std::size_t bucket) noexcept {
  if (bucket >= kBuckets - 1) return std::numeric_limits<std::uint64_t>::max();
  // The largest value whose bucket is `bucket`: found by the smallest one past it, less one.
  std::uint64_t lo = 0;
  std::uint64_t hi = std::uint64_t{1} << 40;
  while (lo < hi) {
    const std::uint64_t mid = lo + (hi - lo) / 2;
    if (bucket_of(mid) <= bucket) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo - 1;
}

void Histogram::record(std::uint64_t us) noexcept {
  std::atomic<std::uint32_t>& b = buckets_[bucket_of(us)];
  if (b.load(std::memory_order_relaxed) != std::numeric_limits<std::uint32_t>::max()) {
    b.fetch_add(1, std::memory_order_relaxed);
  }
  sum_.fetch_add(us, std::memory_order_relaxed);
  std::uint64_t m = max_.load(std::memory_order_relaxed);
  while (us > m && !max_.compare_exchange_weak(m, us, std::memory_order_relaxed)) {
  }
}

std::uint32_t Histogram::in_bucket(std::size_t bucket) const noexcept {
  return bucket < kBuckets ? buckets_[bucket].load(std::memory_order_relaxed) : 0;
}

std::uint64_t Histogram::count() const noexcept {
  std::uint64_t n = 0;
  for (const auto& b : buckets_) n += b.load(std::memory_order_relaxed);
  return n;
}

std::uint64_t Histogram::percentile_us(std::uint32_t permille) const noexcept {
  const std::uint64_t n = count();
  if (n == 0) return 0;
  const std::uint64_t want = (n * std::min<std::uint32_t>(permille, 1000) + 999) / 1000;
  std::uint64_t seen = 0;
  for (std::size_t i = 0; i < kBuckets; ++i) {
    seen += buckets_[i].load(std::memory_order_relaxed);
    if (seen >= want && seen > 0) return std::min(upper_of(i), max_us());
  }
  return max_us();
}

}  // namespace en::gal
