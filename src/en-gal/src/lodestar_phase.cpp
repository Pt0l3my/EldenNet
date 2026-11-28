// The scan by phases (lodestar.hpp's PhaseFound; the twenty-second read-only round).
#include "en/gal/lodestar_trial.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <mutex>

namespace en::gal::lodestar {

namespace {

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

std::uint64_t slot_value(const ByteSource* image, std::uint32_t rva) {
  if (image == nullptr || rva == 0) return 0;
  std::array<std::byte, 8> raw{};
  if (!image->read(rva, raw)) return 0;
  const std::uint64_t at = word_at(raw, 0);
  return plausible(at) ? at : 0;
}

constexpr std::uint8_t kNoSlot = 0xFF;
constexpr std::uint32_t kLeastSpan = 0x40;

}  // namespace

struct PhaseScan::Impl {
  enum class Step : std::uint8_t { Begin, Holder, Parts, Target };

  struct Kept {
    Target t;
    std::uint32_t fields = 0;                 // by the span asked for
    std::uint32_t slots = 0;
    std::array<std::uint8_t, kPhases> slot_of{};
    std::array<std::uint8_t, kPhases> seen{};  // by slot: copies folded in (up to 255)
    std::uint32_t bytes = 0;                  // the least a copy held; 0: never copied
    std::uint32_t span_now = 0;               // what the next copy asks for
    std::uint32_t copies = 0;
    std::vector<std::uint32_t> always;        // fields * slots: the bits set in every copy of the phase
    std::vector<std::uint32_t> ever;          // the bits set in any
  };
  struct Told {
    std::uint32_t targets = 0, wholes = 0, bits = 0;
    std::array<PhaseHit, kListed> hits{};
    std::size_t hit_count = 0;
  };
  struct TargetTold {
    std::uint32_t bytes = 0, copies = 0, phases_seen = 0;
  };
  struct Summary {
    std::uint32_t passes = 0, absent = 0, phase = 0, copied = 0;
    std::array<Told, kMaxPatterns> patterns{};
    std::array<TargetTold, kMaxTargets> targets{};
  };

  Options o;
  std::vector<Kept> kept;
  std::vector<std::byte> buffer;

  Step step = Step::Begin;
  std::uint32_t phase_now = 0;
  std::uint64_t player_at = 0, holder_at = 0;
  std::array<std::uint64_t, kMaxParts> part_at{};
  bool player_seen = false;
  std::size_t target_now = 0;
  std::uint32_t read_now = 0;
  std::uint64_t next_due_us = 0;
  std::uint32_t passes = 0, absent = 0;
  std::atomic<std::uint32_t> marks{0};
  std::atomic<std::uint64_t> mark_us{0};
  mutable std::mutex mutex;
  Summary shared;
  Summary local;

  explicit Impl(Options options) : o(std::move(options)) {
    if (o.targets.size() > kMaxTargets) o.targets.resize(kMaxTargets);
    if (o.patterns.size() > kMaxPatterns) o.patterns.resize(kMaxPatterns);
    buffer.resize(kMaxSpan);
    kept.reserve(o.targets.size());
    for (const Target& t : o.targets) {
      Kept k;
      k.t = t;
      k.t.span = std::clamp<std::uint32_t>(t.span & ~3u, kLeastSpan, kMaxSpan);
      k.t.part = std::min<std::uint32_t>(t.part, kMaxParts - 1);
      k.fields = k.t.span / 4;
      k.span_now = k.t.span;
      k.slot_of.fill(kNoSlot);
      for (std::uint32_t p = 0; p < kPhases; ++p) {
        if ((t.phases >> p) & 1u) k.slot_of[p] = static_cast<std::uint8_t>(k.slots++);
      }
      k.always.assign(std::size_t{k.fields} * k.slots, 0);
      k.ever.assign(std::size_t{k.fields} * k.slots, 0);
      kept.push_back(std::move(k));
    }
  }

  // One read into `out`: 1 read, 2 the reader's time was up, 0 not readable.
  int read(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t address, std::span<std::byte> out) {
    if (!plausible(address)) return 0;
    if (reader.copy_into(gt, proto::Group::Session, address, out, 8).has_value()) return 1;
    return reader.spent() ? 2 : 0;
  }

  [[nodiscard]] std::uint64_t address_of(const Kept& k) const {
    switch (k.t.kind) {
      case kStatic: return slot_value(o.image, k.t.rva);
      case kPlayer: return player_at;
      default: return part_at[k.t.part];
    }
  }

  void fold(Kept& k, std::uint32_t bytes) {
    const std::uint8_t slot = k.slot_of[phase_now];
    const std::span<const std::byte> now(buffer.data(), bytes);
    const std::size_t fields = bytes / 4;
    std::uint32_t* always = k.always.data() + std::size_t{slot} * k.fields;
    std::uint32_t* ever = k.ever.data() + std::size_t{slot} * k.fields;
    if (k.seen[slot] == 0) {
      for (std::size_t f = 0; f < fields; ++f) always[f] = ever[f] = field_at(now, f * 4);
    } else {
      for (std::size_t f = 0; f < fields; ++f) {
        const std::uint32_t v = field_at(now, f * 4);
        always[f] &= v;
        ever[f] |= v;
      }
    }
    if (k.seen[slot] != 0xFF) ++k.seen[slot];
    k.bytes = k.bytes == 0 ? bytes : std::min(k.bytes, bytes);
    ++k.copies;
  }

  // Whether the pattern can be tried on the target: of its group, and copied in every phase the pattern names.
  [[nodiscard]] static bool tried(const Pattern& p, const Kept& k) {
    if (p.group != k.t.group || k.bytes == 0) return false;
    const std::uint32_t names = p.kind == kLevel ? (p.on | p.off) : p.on;
    if (names == 0 || (p.kind == kLevel && (p.on == 0 || p.off == 0))) return false;
    for (std::uint32_t ph = 0; ph < kPhases; ++ph) {
      if (((names >> ph) & 1u) == 0) continue;
      if (k.slot_of[ph] == kNoSlot || k.seen[k.slot_of[ph]] == 0) return false;
    }
    return true;
  }

  void try_patterns(Summary& s) const {
    for (std::size_t pi = 0; pi < o.patterns.size(); ++pi) {
      const Pattern& p = o.patterns[pi];
      Told& told = s.patterns[pi];
      told = Told{};
      for (std::size_t ti = 0; ti < kept.size(); ++ti) {
        const Kept& k = kept[ti];
        if (!tried(p, k)) continue;
        ++told.targets;
        const std::size_t fields = k.bytes / 4;
        const auto add = [&](const PhaseHit& h) {
          if (told.hit_count < kListed) told.hits[told.hit_count++] = h;
        };
        for (std::size_t f = 0; f < fields; ++f) {
          const auto always = [&](std::uint32_t ph) { return k.always[std::size_t{k.slot_of[ph]} * k.fields + f]; };
          const auto ever = [&](std::uint32_t ph) { return k.ever[std::size_t{k.slot_of[ph]} * k.fields + f]; };
          PhaseHit h;
          h.target = static_cast<std::uint32_t>(ti);
          h.offset = static_cast<std::uint32_t>(f * 4);
          if (p.kind == kCount) {
            // Still in each phase, and gone up between them by what the round says.
            bool fits = true, first = true, moved = false;
            std::uint32_t base = 0, base_count = 0;
            for (std::uint32_t ph = 0; ph < kPhases && fits; ++ph) {
              if (((p.on >> ph) & 1u) == 0) continue;
              if (always(ph) != ever(ph)) {
                fits = false;
              } else if (first) {
                first = false;
                base = always(ph);
                base_count = p.count[ph];
              } else {
                fits = always(ph) - base == std::uint32_t{p.count[ph]} - base_count;
                moved = moved || p.count[ph] != base_count;
              }
            }
            if (fits && moved) {
              h.whole = true;
              ++told.wholes;
              add(h);
            }
            continue;
          }
          // kLevel. `still`: the bits that kept still in every phase named; `a` and `b`: the off and the on values
          // where they are one value each (`same_*`: the bits on which the phases of that side agree).
          std::uint32_t still = ~0u, a = 0, b = 0, same_a = ~0u, same_b = ~0u;
          bool first_a = true, first_b = true;
          for (std::uint32_t ph = 0; ph < kPhases; ++ph) {
            const bool on = (p.on >> ph) & 1u, off = (p.off >> ph) & 1u;
            if (!on && !off) continue;
            const std::uint32_t v = always(ph);
            still &= ~(always(ph) ^ ever(ph));
            if (on) {
              if (first_b) b = v; else same_b &= ~(b ^ v);
              first_b = false;
            } else {
              if (first_a) a = v; else same_a &= ~(a ^ v);
              first_a = false;
            }
          }
          if (still == ~0u && same_a == ~0u && same_b == ~0u) {
            if (a == b) continue;
            h.whole = true;
            h.one_bit = std::has_single_bit(a ^ b);
            h.bit = h.one_bit ? static_cast<std::uint32_t>(std::countr_zero(a ^ b)) : 0;
            h.off_zero = a == 0;
            h.on_small = b >= 1 && b <= 255;
            ++told.wholes;
            add(h);
            continue;
          }
          // Bit by bit: a bit that kept still in every phase, one way in all the on phases and the other in all
          // the off ones, while other bits of the field did not.
          std::uint32_t fit = p.whole_only ? 0u : still & same_a & same_b & (a ^ b);
          while (fit != 0) {
            h.whole = false;
            h.bit = static_cast<std::uint32_t>(std::countr_zero(fit));
            fit &= fit - 1;
            ++told.bits;
            add(h);
          }
        }
      }
    }
  }

  void publish() {
    Summary& s = local;
    s.passes = passes;
    s.absent = absent;
    s.phase = phase_now;
    s.copied = 0;
    for (std::size_t i = 0; i < kept.size(); ++i) {
      const Kept& k = kept[i];
      TargetTold& t = s.targets[i];
      t.bytes = k.bytes;
      t.copies = k.copies;
      t.phases_seen = 0;
      for (std::uint32_t ph = 0; ph < kPhases; ++ph) {
        if (k.slot_of[ph] != kNoSlot && k.seen[k.slot_of[ph]] != 0) t.phases_seen |= 1u << ph;
      }
      s.copied += k.copies != 0 ? 1 : 0;
    }
    try_patterns(s);
    const std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (lock.owns_lock()) shared = s;  // a reader of the summary is copying the last one: the next pass publishes
  }
};

PhaseScan::PhaseScan(Options options) : impl_(std::make_unique<Impl>(std::move(options))) {}
PhaseScan::~PhaseScan() = default;

void PhaseScan::mark(std::uint64_t now_us) noexcept {
  impl_->mark_us.store(now_us, std::memory_order_release);
  impl_->marks.fetch_add(1, std::memory_order_release);
}

bool PhaseScan::due(std::uint64_t now_us) const noexcept {
  const Impl& m = *impl_;
  if (m.step != Impl::Step::Begin) return true;
  if (m.marks.load(std::memory_order_acquire) == 0 || now_us < m.next_due_us) return false;
  const std::uint64_t since = now_us - std::min(now_us, m.mark_us.load(std::memory_order_acquire));
  return since >= kWindowFromUs && since <= kWindowToUs;
}

bool PhaseScan::step(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t now_us) {
  Impl& m = *impl_;
  using Step = Impl::Step;
  const auto finish = [&] {
    if (!m.player_seen) ++m.absent;
    ++m.passes;
    m.publish();
    m.next_due_us = now_us + kEveryUs;
    m.step = Step::Begin;
    return true;
  };
  for (;;) {  // until one read was made or the pass ends
    switch (m.step) {
      case Step::Begin: {
        const std::uint32_t marks = m.marks.load(std::memory_order_acquire);
        if (marks == 0) return true;
        m.phase_now = std::min(marks - 1, kPhases - 1);
        m.player_at = 0;
        m.holder_at = 0;
        m.part_at = {};
        m.player_seen = false;
        m.read_now = 0;
        m.target_now = 0;
        m.step = Step::Target;
        const std::uint64_t manager = slot_value(m.o.image, m.o.manager_slot);
        if (manager == 0) break;
        std::array<std::byte, 8> raw{};
        const int got = m.read(gt, reader, manager + m.o.main_player_word, raw);
        if (got == 2) {
          m.step = Step::Begin;
          return false;
        }
        m.player_at = got == 1 && plausible(word_at(raw, 0)) ? word_at(raw, 0) : 0;
        m.player_seen = m.player_at != 0;
        if (m.player_at != 0 && m.o.parts_at != 0) m.step = Step::Holder;
        if (got == 1) return false;
        break;
      }
      case Step::Holder: {
        m.step = Step::Target;
        std::array<std::byte, 8> raw{};
        const int got = m.read(gt, reader, m.player_at + m.o.parts_at, raw);
        if (got == 2) {
          m.step = Step::Holder;
          return false;
        }
        m.holder_at = got == 1 && plausible(word_at(raw, 0)) ? word_at(raw, 0) : 0;
        if (m.holder_at != 0) m.step = Step::Parts;
        if (got == 1) return false;
        break;
      }
      case Step::Parts: {
        m.step = Step::Target;
        std::array<std::byte, kMaxParts * 8> raw{};
        const int got = m.read(gt, reader, m.holder_at, raw);
        if (got == 2) {
          m.step = Step::Parts;
          return false;
        }
        if (got == 1) {
          for (std::size_t i = 0; i < kMaxParts; ++i) {
            const std::uint64_t at = word_at(raw, i * 8);
            m.part_at[i] = plausible(at) ? at : 0;
          }
          return false;
        }
        break;
      }
      case Step::Target: {
        // The tester has moved on: what is left of this pass would be folded into the wrong phase.
        if (m.marks.load(std::memory_order_acquire) - 1 != m.phase_now && m.phase_now != kPhases - 1) return finish();
        if (m.target_now >= m.kept.size()) return finish();
        Impl::Kept& k = m.kept[m.target_now];
        const std::uint8_t slot = k.slot_of[m.phase_now];
        if (slot == kNoSlot || (k.t.once && k.seen[slot] != 0)) {
          ++m.target_now;
          break;
        }
        const std::uint64_t at = m.address_of(k);
        if (at == 0) {
          ++m.target_now;
          break;
        }
        const int got = m.read(gt, reader, at, std::span<std::byte>(m.buffer.data(), k.span_now));
        if (got == 2) return false;
        if (got == 1) {
          m.fold(k, k.span_now);
          ++m.read_now;
          ++m.target_now;
          return false;
        }
        // Not readable at this length: the object's memory ends sooner. Half as much at the next try, and the
        // target is passed over once nothing of use is left.
        if (k.span_now / 2 >= kLeastSpan && k.bytes == 0) {
          k.span_now = (k.span_now / 2) & ~3u;
        } else {
          ++m.target_now;
        }
        return false;
      }
    }
  }
}

PhaseFound PhaseScan::found() const {
  const Impl& m = *impl_;
  PhaseFound out;
  const std::lock_guard<std::mutex> lock(m.mutex);
  const Impl::Summary& s = m.shared;
  out.passes = s.passes;
  out.absent = s.absent;
  out.phase = s.phase;
  out.targets_total = static_cast<std::uint32_t>(m.kept.size());
  out.targets_copied = s.copied;
  // The targets told: the named ones that were copied (a static with a number, the player, its parts), and any
  // with a hit. A hit's `target` is its place in this list.
  std::vector<std::uint32_t> place(m.kept.size(), ~0u);
  const auto tell = [&](std::size_t i) {
    if (place[i] != ~0u) return place[i];
    const Impl::Kept& k = m.kept[i];
    PhaseTarget t;
    t.kind = k.t.kind;
    t.id = k.t.id;
    t.rva = k.t.rva;
    t.part = k.t.part;
    t.group = k.t.group;
    t.bytes = s.targets[i].bytes;
    t.copies = s.targets[i].copies;
    t.phases_seen = s.targets[i].phases_seen;
    place[i] = static_cast<std::uint32_t>(out.targets.size());
    out.targets.push_back(t);
    return place[i];
  };
  for (std::size_t i = 0; i < m.kept.size(); ++i) {
    const Impl::Kept& k = m.kept[i];
    if (s.targets[i].copies != 0 && (k.t.kind != kStatic || k.t.id != 0)) (void)tell(i);
  }
  for (std::size_t pi = 0; pi < m.o.patterns.size(); ++pi) {
    const Impl::Told& told = s.patterns[pi];
    PhasePatternFound p;
    p.pattern = static_cast<std::uint32_t>(pi);
    p.targets = told.targets;
    p.wholes = told.wholes;
    p.bits = told.bits;
    for (std::size_t h = 0; h < told.hit_count; ++h) {
      PhaseHit hit = told.hits[h];
      hit.target = tell(hit.target);
      p.hits.push_back(hit);
    }
    out.patterns.push_back(std::move(p));
  }
  return out;
}

}  // namespace en::gal::lodestar
