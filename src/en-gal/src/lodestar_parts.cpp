// The pair search and the parts watch (lodestar.hpp's PairFound and PartsFound; the fourteenth read-only round).
#include "en/gal/lodestar_trial.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

namespace en::gal::lodestar {

namespace {

constexpr std::uint32_t kSmallest = 0x40;

std::uint64_t word_at(std::span<const std::byte> bytes, std::size_t at) {
  std::uint64_t v = 0;
  std::memcpy(&v, bytes.data() + at, 8);
  return v;
}
std::uint32_t field_at(std::span<const std::byte> bytes, std::size_t at) {
  std::uint32_t v = 0;
  std::memcpy(&v, bytes.data() + at, 4);
  return v;
}
bool plausible(std::uint64_t at) { return at % 8 == 0 && at >= kLowestAddress && at <= kAddressEnd; }

// A slot's value, from the image as loaded; 0 where it holds no address of memory.
std::uint64_t slot_value(const ByteSource* image, std::uint32_t rva) {
  if (image == nullptr || rva == 0) return 0;
  std::array<std::byte, 8> raw{};
  if (!image->read(rva, raw)) return 0;
  const std::uint64_t at = word_at(raw, 0);
  return plausible(at) ? at : 0;
}

}  // namespace

// ---- The pair search ----

struct PairSearch::Candidate {
  std::uint32_t slot = 0;
  std::uint32_t span = kSpan;  // what is copied: shortened where the object's memory ends sooner
  bool alive = true;           // something of it read the first number
};

namespace {
constexpr std::size_t kWordBits = PairSearch::kSpan / 4 / 64;  // u64s a candidate, one bit a 32-bit field
constexpr std::size_t kByteBits = PairSearch::kSpan / 64;      // one bit a byte
}  // namespace

PairSearch::PairSearch(std::vector<std::uint32_t> slots, std::uint32_t first, std::uint32_t second,
                       const ByteSource* image)
    : image_(image), first_(first), second_(second), buffer_(kSpan) {
  if (slots.size() > kMaxCandidates) slots.resize(kMaxCandidates);
  candidates_.resize(slots.size());
  for (std::size_t i = 0; i < slots.size(); ++i) candidates_[i].slot = slots[i];
  words_.assign(slots.size() * kWordBits, 0);
  bytes_.assign(slots.size() * kByteBits, 0);
}

PairSearch::~PairSearch() = default;

void PairSearch::ask() noexcept {
  const std::uint32_t n = asked_.load(std::memory_order_relaxed);
  if (n >= kPasses) return;
  if (n > 0 && done_.load(std::memory_order_acquire) < n) overrun_[n - 1].store(true, std::memory_order_relaxed);
  asked_.store(n + 1, std::memory_order_release);
}

bool PairSearch::step(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t now_us) {
  const std::uint32_t pass = done_.load(std::memory_order_relaxed);
  if (pass >= asked_.load(std::memory_order_acquire) || pass >= kPasses) return false;
  if (!pass_started_) {
    pass_started_ = true;
    pass_start_us_ = now_us;
  }
  for (;;) {  // until one read was made or the pass ends
    if (cursor_ >= candidates_.size()) {
      cursor_ = 0;
      const std::uint64_t took = now_us >= pass_start_us_ ? (now_us - pass_start_us_) / 1000 : 0;
      took_ms_[pass].store(static_cast<std::uint32_t>(std::min<std::uint64_t>(took, 0xFFFF'FFFFu)),
                           std::memory_order_relaxed);
      pass_started_ = false;
      done_.store(pass + 1, std::memory_order_release);
      return true;
    }
    Candidate& c = candidates_[cursor_];
    std::uint64_t* words = words_.data() + cursor_ * kWordBits;
    std::uint64_t* bytes = bytes_.data() + cursor_ * kByteBits;
    const auto drop = [&] {
      c.alive = false;
      std::fill(words, words + kWordBits, 0);
      std::fill(bytes, bytes + kByteBits, 0);
      ++cursor_;
    };
    if (!c.alive) {
      ++cursor_;
      continue;
    }
    const std::uint64_t at = slot_value(image_, c.slot);
    if (at == 0) {  // nothing behind the slot now
      drop();
      continue;
    }
    const std::span<std::byte> got = std::span<std::byte>(buffer_).first(c.span);
    if (!reader.copy_into(gt, proto::Group::Session, at, got, 8).has_value()) {
      if (reader.spent()) return false;  // the same candidate again at the next call
      if (c.span > kSmallest) {
        c.span /= 2;  // memory ends sooner than the span: a shorter read next
      } else {
        drop();
      }
      return false;
    }
    objects_[pass].fetch_add(1, std::memory_order_relaxed);
    const std::uint32_t want = pass == 0 ? first_ : second_;
    bool any = false;
    std::uint64_t word_count = 0, byte_count = 0;
    for (std::size_t f = 0; f < kSpan / 4; ++f) {
      const std::uint64_t bit = std::uint64_t{1} << (f % 64);
      const bool reads = f * 4 + 4 <= c.span && field_at(got, f * 4) == want;
      if (pass == 0 ? reads : (reads && (words[f / 64] & bit) != 0)) {
        words[f / 64] |= bit;
        any = true;
        ++word_count;
      } else {
        words[f / 64] &= ~bit;
      }
    }
    for (std::size_t i = 0; i < kSpan; ++i) {
      const std::uint64_t bit = std::uint64_t{1} << (i % 64);
      const bool reads = want <= 0xFF && i < c.span && static_cast<std::uint8_t>(got[i]) == want;
      if (pass == 0 ? reads : (reads && (bytes[i / 64] & bit) != 0)) {
        bytes[i / 64] |= bit;
        any = true;
        ++byte_count;
      } else {
        bytes[i / 64] &= ~bit;
      }
    }
    if (pass == 0) {
      words_first_.fetch_add(word_count, std::memory_order_relaxed);
      bytes_first_.fetch_add(byte_count, std::memory_order_relaxed);
    }
    c.alive = any;
    ++cursor_;
    return false;
  }
}

PairFound PairSearch::found() const {
  PairFound out;
  out.first = first_;
  out.second = second_;
  out.candidates = static_cast<std::uint32_t>(candidates_.size());
  out.asked = asked_.load(std::memory_order_acquire);
  out.passes = done_.load(std::memory_order_acquire);
  for (std::uint32_t p = 0; p < out.asked && p < kPasses; ++p) {
    StateLook look;
    look.through = p < out.passes;
    look.took_ms = look.through ? took_ms_[p].load(std::memory_order_relaxed) : 0;
    look.overrun = overrun_[p].load(std::memory_order_relaxed);
    out.overrun = out.overrun || look.overrun;
    out.looks.push_back(look);
  }
  out.objects_first = objects_[0].load(std::memory_order_relaxed);
  out.objects_second = objects_[1].load(std::memory_order_relaxed);
  out.words_first = words_first_.load(std::memory_order_relaxed);
  out.bytes_first = bytes_first_.load(std::memory_order_relaxed);
  if (out.passes < kPasses) return out;  // the candidates are the stepping thread's until the last pass is done
  std::vector<PairHit> narrow;
  for (std::size_t n = 0; n < candidates_.size(); ++n) {
    const Candidate& c = candidates_[n];
    if (!c.alive) continue;
    const std::uint64_t* words = words_.data() + n * kWordBits;
    const std::uint64_t* bytes = bytes_.data() + n * kByteBits;
    for (std::size_t f = 0; f < kSpan / 4; ++f) {
      if ((words[f / 64] >> (f % 64) & 1) == 0) continue;
      ++out.word_hits;
      if (out.hits.size() < kMaxHits) out.hits.push_back(PairHit{c.slot, static_cast<std::uint32_t>(f * 4), 4, 0});
    }
    for (std::size_t i = 0; i < kSpan; ++i) {
      if ((bytes[i / 64] >> (i % 64) & 1) == 0) continue;
      const std::size_t f = i / 4;
      if ((words[f / 64] >> (f % 64) & 1) != 0) continue;  // the 32-bit field it lies in is a hit already
      ++out.byte_hits;
      if (narrow.size() < kMaxHits) narrow.push_back(PairHit{c.slot, static_cast<std::uint32_t>(i), 1, 0});
    }
  }
  for (const PairHit& h : narrow) {
    if (out.hits.size() < kMaxHits) out.hits.push_back(h);
  }
  return out;
}

// ---- The parts watch ----

struct PartsWatch::Impl {
  enum class Step : std::uint8_t { Player, HolderPtr, Holder, Parts, End };
  static constexpr std::size_t kFields = kPartSpan / 4;

  struct MoveRow {
    std::uint32_t part = 0, offset = 0;
    bool fraction = false;
    std::array<std::uint8_t, kPhases> changed{};
  };
  struct FallRow {
    std::uint32_t part = 0, offset = 0, fell = 0, rose = 0, max_at = 0;
    bool has_max = false;
  };
  // What the last finished pass left, for the other thread (under the mutex).
  struct Summary {
    std::uint32_t passes = 0, absent = 0, restarts = 0, phase = 0;
    std::array<std::uint32_t, kPhases> phase_passes{};
    std::array<PartFound, kHolderWords> parts{};
    std::size_t part_count = 0;
    std::array<MoveRow, kMaxMoves> moves{};
    std::size_t move_count = 0;
    std::uint32_t moves_total = 0;
    std::array<FallRow, kMaxFalls> falls{};
    std::size_t fall_count = 0;
    std::uint32_t falls_total = 0;
  };

  Options o;
  std::vector<std::uint8_t> changed, fell, rose;  // by part, field and phase
  std::vector<std::byte> prev;                    // by part: what it held at the pass before
  std::vector<std::byte> cur;
  std::array<std::byte, kHolderWords * 8> holder{};
  std::array<std::uint64_t, kHolderWords> part_at{};
  std::array<std::uint32_t, kHolderWords> part_span{};   // the size the next read asks for
  std::array<std::uint32_t, kHolderWords> part_bytes{};  // read at the last pass
  std::array<std::uint32_t, kHolderWords> part_vtable{};
  std::array<bool, kHolderWords> has_prev{};
  std::uint64_t player = 0, holder_at = 0, player_new = 0, holder_new = 0;
  Step step = Step::Player;
  std::size_t part = 0;
  bool counted = false;
  std::uint32_t phase_now = 0;
  std::uint64_t next_due_us = 0;
  std::uint32_t passes = 0, absent = 0, restarts = 0;
  std::array<std::uint32_t, kPhases> phase_passes{};
  std::atomic<std::uint32_t> phase{0};
  std::atomic<std::uint64_t> phase_start_us{0};
  mutable std::mutex mutex;
  Summary shared;

  explicit Impl(Options options)
      : o(options),
        changed(kHolderWords * kFields * kPhases),
        fell(kHolderWords * kFields * kPhases),
        rose(kHolderWords * kFields * kPhases),
        prev(kHolderWords * kPartSpan),
        cur(kPartSpan) {
    part_span.fill(kPartSpan);
  }

  [[nodiscard]] bool pointer_like(std::uint64_t v) const noexcept {
    return plausible(v) && !(v >= o.base && v - o.base < o.image_size);
  }
  [[nodiscard]] std::size_t at(std::size_t p, std::size_t f, std::size_t ph) const noexcept {
    return (p * kFields + f) * kPhases + ph;
  }
  void forget_part(std::size_t p) {
    has_prev[p] = false;
    part_span[p] = kPartSpan;
    part_bytes[p] = 0;
    part_vtable[p] = 0;
    const std::size_t from = at(p, 0, 0), n = kFields * kPhases;
    std::fill_n(changed.begin() + static_cast<std::ptrdiff_t>(from), n, std::uint8_t{0});
    std::fill_n(fell.begin() + static_cast<std::ptrdiff_t>(from), n, std::uint8_t{0});
    std::fill_n(rose.begin() + static_cast<std::ptrdiff_t>(from), n, std::uint8_t{0});
  }

  // One 8-byte read: 1 read, 2 the reader's time was up, 0 not readable.
  int read_word(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t address, std::uint64_t& out) {
    std::array<std::byte, 8> raw{};
    if (!plausible(address)) return 0;
    if (reader.copy_into(gt, proto::Group::Session, address, raw, 8).has_value()) {
      out = word_at(raw, 0);
      return 1;
    }
    return reader.spent() ? 2 : 0;
  }

  void compare(std::size_t p, std::uint32_t size) {
    const std::span<const std::byte> now = std::span<const std::byte>(cur).first(size);
    const std::span<const std::byte> before = std::span<const std::byte>(prev).subspan(p * kPartSpan, size);
    const auto bump = [](std::uint8_t& v) { v = v == 0xFF ? v : static_cast<std::uint8_t>(v + 1); };
    for (std::size_t f = 0; f * 4 + 4 <= size; ++f) {
      const std::uint32_t a = field_at(before, f * 4), b = field_at(now, f * 4);
      if (a == b) continue;
      const std::size_t i = at(p, f, phase_now);
      bump(changed[i]);
      if (static_cast<std::int32_t>(b) < static_cast<std::int32_t>(a)) {
        bump(fell[i]);
      } else {
        bump(rose[i]);
      }
    }
  }

  void publish() {
    const std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;  // the other thread is reading the last one: the next pass publishes
    Summary& s = shared;
    s.passes = passes;
    s.absent = absent;
    s.restarts = restarts;
    s.phase = phase_now;
    s.phase_passes = phase_passes;
    s.part_count = 0;
    s.move_count = 0;
    s.moves_total = 0;
    s.fall_count = 0;
    s.falls_total = 0;
    const std::uint32_t walked = phase_passes[o.walk];
    const std::uint32_t stood = phase_passes[o.still_a] + phase_passes[o.still_b];
    for (std::size_t p = 0; p < kHolderWords; ++p) {
      if (part_bytes[p] == 0) continue;
      s.parts[s.part_count++] = PartFound{static_cast<std::uint32_t>(p), part_vtable[p], part_bytes[p]};
      if (!has_prev[p]) continue;
      const std::span<const std::byte> held = std::span<const std::byte>(prev).subspan(p * kPartSpan, part_bytes[p]);
      for (std::size_t f = 0; f * 4 + 4 <= part_bytes[p]; ++f) {
        const std::uint8_t* c = changed.data() + at(p, f, 0);
        // It moved in at least half of the walk's passes (three at least), and never while the tester stood.
        if (walked >= 3 && stood >= 3 && c[o.walk] >= 3 && std::uint32_t{c[o.walk]} * 2 >= walked &&
            c[o.still_a] == 0 && c[o.still_b] == 0) {
          ++s.moves_total;
          if (s.move_count < kMaxMoves) {
            MoveRow& row = s.moves[s.move_count++];
            row.part = static_cast<std::uint32_t>(p);
            row.offset = static_cast<std::uint32_t>(f * 4);
            float value = 0.0f;
            std::memcpy(&value, held.data() + f * 4, 4);
            row.fraction = std::isfinite(value) && (value == 0.0f || (std::fabs(value) > 1.0e-3f && std::fabs(value) < 1.0e7f));
            std::copy_n(c, kPhases, row.changed.begin());
          }
        }
        // It fell when the tester was hit, changed little else then, and never moved before.
        const std::uint8_t fallen = fell[at(p, f, o.hit)];
        if (fallen >= 1 && c[o.hit] <= 6 && c[o.still_a] == 0 && c[o.walk] == 0 && c[o.still_b] == 0) {
          ++s.falls_total;
          if (s.fall_count < kMaxFalls) {
            FallRow& row = s.falls[s.fall_count++];
            row.part = static_cast<std::uint32_t>(p);
            row.offset = static_cast<std::uint32_t>(f * 4);
            row.fell = fallen;
            row.rose = rose[at(p, f, o.heal)];
            row.has_max = false;
            row.max_at = 0;
            const auto value = static_cast<std::int32_t>(field_at(held, f * 4));
            // Its maximum: the nearest number within 16 bytes that never changed at all and is at least as large.
            for (std::size_t d = 1; d <= 4 && !row.has_max && value > 0; ++d) {
              for (const std::ptrdiff_t g : {static_cast<std::ptrdiff_t>(f) - static_cast<std::ptrdiff_t>(d),
                                             static_cast<std::ptrdiff_t>(f) + static_cast<std::ptrdiff_t>(d)}) {
                if (g < 0 || static_cast<std::size_t>(g) * 4 + 4 > part_bytes[p] || row.has_max) continue;
                const std::uint8_t* cg = changed.data() + at(p, static_cast<std::size_t>(g), 0);
                if (std::any_of(cg, cg + kPhases, [](std::uint8_t v) { return v != 0; })) continue;
                const auto most = static_cast<std::int32_t>(field_at(held, static_cast<std::size_t>(g) * 4));
                if (most >= value && most <= 10'000'000) {
                  row.has_max = true;
                  row.max_at = static_cast<std::uint32_t>(g * 4);
                }
              }
            }
          }
        }
      }
    }
  }
};

PartsWatch::PartsWatch(Options options) : impl_(std::make_unique<Impl>(options)) {}
PartsWatch::~PartsWatch() = default;

void PartsWatch::mark(std::uint64_t now_us) noexcept {
  impl_->phase_start_us.store(now_us, std::memory_order_relaxed);
  impl_->phase.fetch_add(1, std::memory_order_release);
}

bool PartsWatch::due(std::uint64_t now_us) const noexcept {
  return impl_->step != Impl::Step::Player || now_us >= impl_->next_due_us;
}

bool PartsWatch::step(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t now_us) {
  Impl& m = *impl_;
  using Step = Impl::Step;
  const auto finish = [&](bool present) {
    if (present) {
      if (m.counted) ++m.phase_passes[m.phase_now];
    } else {
      ++m.absent;
    }
    ++m.passes;
    m.publish();
    m.next_due_us = now_us + kEveryUs;
    m.step = Step::Player;
    return true;
  };
  for (;;) {  // until one read was made or the pass ends
    switch (m.step) {
      case Step::Player: {
        if (now_us < m.next_due_us) return false;
        m.phase_now = std::min(m.phase.load(std::memory_order_acquire), kPhases - 1);
        // The first seconds of a phase are the tester's change from one thing to the next, and are not counted;
        // but the hit and the heal come when they come, so their phases count from the start.
        m.counted = m.phase_now == m.o.hit || m.phase_now == m.o.heal ||
                    now_us - m.phase_start_us.load(std::memory_order_relaxed) >= kGraceUs;
        const std::uint64_t manager = slot_value(m.o.image, m.o.manager_slot);
        if (manager == 0) return finish(false);
        const int r = m.read_word(gt, reader, manager + m.o.player_word, m.player_new);
        if (r == 2) return false;
        if (r == 0 || !m.pointer_like(m.player_new)) return finish(false);
        m.step = Step::HolderPtr;
        return false;
      }
      case Step::HolderPtr: {
        const int r = m.read_word(gt, reader, m.player_new + m.o.holder_at, m.holder_new);
        if (r == 2) return false;
        if (r == 0 || !m.pointer_like(m.holder_new)) return finish(false);
        m.step = Step::Holder;
        return false;
      }
      case Step::Holder: {
        if (!reader.copy_into(gt, proto::Group::Session, m.holder_new, m.holder, 8).has_value()) {
          if (reader.spent()) return false;
          return finish(false);
        }
        if (m.player_new != m.player || m.holder_new != m.holder_at) {
          // Another player's object, or another holder: what was counted was the other's.
          if (m.player != 0) ++m.restarts;
          for (std::size_t p = 0; p < kHolderWords; ++p) {
            m.forget_part(p);
            m.part_at[p] = 0;
          }
          m.phase_passes.fill(0);
          m.player = m.player_new;
          m.holder_at = m.holder_new;
        }
        m.part = 0;
        m.step = Step::Parts;
        return false;
      }
      case Step::Parts: {
        if (m.part >= kHolderWords) {
          m.step = Step::End;
          break;
        }
        const std::size_t p = m.part;
        const std::uint64_t v = word_at(m.holder, p * 8);
        if (!m.pointer_like(v)) {
          if (m.part_at[p] != 0) m.forget_part(p);
          m.part_at[p] = 0;
          ++m.part;
          break;
        }
        if (v != m.part_at[p]) {  // another object behind this word than before
          m.forget_part(p);
          m.part_at[p] = v;
        }
        const std::uint32_t size = m.part_span[p];
        if (size == 0) {  // nothing of it could be read
          ++m.part;
          break;
        }
        if (!reader.copy_into(gt, proto::Group::Session, v, std::span<std::byte>(m.cur).first(size), 8).has_value()) {
          if (reader.spent()) return false;
          m.part_span[p] = size > kSmallest ? size / 2 : 0;  // memory ends sooner: a shorter read next
          m.has_prev[p] = false;
          return false;
        }
        const std::uint64_t first = word_at(m.cur, 0);
        m.part_vtable[p] =
            first >= m.o.base && first - m.o.base < m.o.image_size ? static_cast<std::uint32_t>(first - m.o.base) : 0;
        if (m.has_prev[p] && m.part_bytes[p] == size && m.counted) m.compare(p, size);
        std::memcpy(m.prev.data() + p * kPartSpan, m.cur.data(), size);
        m.part_bytes[p] = size;
        m.has_prev[p] = true;
        ++m.part;
        return false;
      }
      case Step::End: return finish(true);
    }
  }
}

PartsFound PartsWatch::found() const {
  const Impl& m = *impl_;
  PartsFound out;
  out.player_word = m.o.player_word;
  out.holder_at = m.o.holder_at;
  const std::lock_guard<std::mutex> lock(m.mutex);
  const Impl::Summary& s = m.shared;
  out.passes = s.passes;
  out.absent = s.absent;
  out.restarts = s.restarts;
  out.phase = s.phase;
  out.phase_passes.assign(s.phase_passes.begin(), s.phase_passes.end());
  out.parts.assign(s.parts.begin(), s.parts.begin() + static_cast<std::ptrdiff_t>(s.part_count));
  out.moves_total = s.moves_total;
  for (std::size_t i = 0; i < s.move_count; ++i) {
    const Impl::MoveRow& row = s.moves[i];
    PartMove move;
    move.part = row.part;
    move.offset = row.offset;
    move.fraction = row.fraction;
    move.changed.assign(row.changed.begin(), row.changed.end());
    out.moves.push_back(std::move(move));
  }
  out.falls_total = s.falls_total;
  for (std::size_t i = 0; i < s.fall_count; ++i) {
    const Impl::FallRow& row = s.falls[i];
    out.falls.push_back(PartFall{row.part, row.offset, row.fell, row.rose, row.has_max, row.max_at});
  }
  return out;
}

}  // namespace en::gal::lodestar
