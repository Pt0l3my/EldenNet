// The reader's trial of Lodestar's in-game round (lodestar_trial.hpp).
#include "en/gal/lodestar_trial.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace en::gal::lodestar {

Trial::Trial(SysLayer& sys, std::uint64_t base, std::uint64_t period_us) noexcept
    : sys_(sys), reader_(sys), base_(base), period_us_(period_us) {}

bool Trial::add(const ManagerFound& manager) {
  const std::size_t n = count_.load(std::memory_order_acquire);
  if (manager.status != derive::Status::Derived || n >= kMaxManagers) return false;
  for (std::size_t i = 0; i < n; ++i) {
    if (slots_[i].id == manager.id) return false;
  }
  Slot& s = slots_[n];  // not yet the reader's: it sees a slot only once the count says so
  s.id = manager.id;
  s.label = manager.label;
  s.slot = base_ + manager.rva;
  s.vtable = base_ + manager.vtable;
  count_.store(n + 1, std::memory_order_release);
  return true;
}

void Trial::run(const proto::GameThread& gt, const FrameCtx& frame) {
  steps_.fetch_add(1, std::memory_order_relaxed);
  // A reading is due at the step nearest its time: half a step early rather than a whole step late, and the next is
  // counted from when this one was due, so that the rate holds whatever the step.
  const std::uint64_t now = frame.qpc_now_us;
  if (now + frame.dt_us / 2 < next_us_) return;
  next_us_ = std::max(next_us_ + period_us_, now + period_us_ / 2);
  if (!bound_) {
    bound_ = true;
    reader_.bind_thread();
  }
  reader_.begin_tick();
  const std::uint64_t t0 = sys_.now_us();
  const std::size_t n = count_.load(std::memory_order_acquire);
  const std::uint32_t stage_id = stage_id_.load(std::memory_order_relaxed);
  std::array<bool, kMaxManagers> empty{};
  int stage = -1;  // unknown until the stage's manager is read
  for (std::size_t i = 0; i < n; ++i) {
    Slot& s = slots_[i];
    const auto bump = [](std::atomic<std::uint64_t>& counter) { counter.fetch_add(1, std::memory_order_relaxed); };
    // The slot, the instance's first word, then the slot again: a slot that changed in between judges nothing.
    const auto first = reader_.copy<std::uint64_t>(gt, proto::Group::Session, s.slot);
    if (!first) {
      bump(s.fault);
      continue;
    }
    if (s.id == stage_id) stage = *first != 0 ? 1 : 0;
    if (*first == 0) {
      bump(s.null);
      empty[i] = true;
      if (s.run++ == 0) bump(s.null_runs);
      if (s.run > s.null_longest.load(std::memory_order_relaxed)) s.null_longest.store(s.run, std::memory_order_relaxed);
      continue;
    }
    s.run = 0;
    const auto word = reader_.copy<std::uint64_t>(gt, proto::Group::Session, *first);
    const auto again = reader_.copy<std::uint64_t>(gt, proto::Group::Session, s.slot);
    if (!again) {
      bump(s.fault);
    } else if (*again != *first) {
      bump(s.torn);
    } else if (!word) {
      bump(s.fault);
    } else {
      bump(*word == s.vtable ? s.same : s.other);
    }
  }
  if (stage == 1) {
    world_readings_.fetch_add(1, std::memory_order_relaxed);
    for (std::size_t i = 0; i < n; ++i) {
      if (empty[i]) slots_[i].null_world.fetch_add(1, std::memory_order_relaxed);
    }
  }
  if (stage != -1) {
    if (last_stage_ != -1 && stage != last_stage_) stage_changes_.fetch_add(1, std::memory_order_relaxed);
    last_stage_ = stage;
  }
  const std::uint64_t done = sys_.now_us();
  reading_us_.record(done - t0);
  if (readings_.fetch_add(1, std::memory_order_relaxed) == 0) first_us_.store(t0, std::memory_order_relaxed);
  last_us_.store(done, std::memory_order_relaxed);
}

TrialFound Trial::found() const {
  TrialFound out;
  out.steps = steps_.load(std::memory_order_relaxed);
  out.readings = readings_.load(std::memory_order_relaxed);
  out.world_readings = world_readings_.load(std::memory_order_relaxed);
  out.stage_changes = stage_changes_.load(std::memory_order_relaxed);
  const std::uint64_t first = first_us_.load(std::memory_order_relaxed);
  const std::uint64_t last = last_us_.load(std::memory_order_relaxed);
  out.elapsed_ms = last > first ? (last - first) / 1000 : 0;
  out.reading_p50_us = reading_us_.percentile_us(500);
  out.reading_p99_us = reading_us_.percentile_us(990);
  out.reading_max_us = reading_us_.max_us();
  out.copies = reader_.total_copies();
  for (std::size_t k = 0; k < kFaultKinds; ++k) out.faults[k] = reader_.faults(static_cast<FaultKind>(k));
  const std::size_t n = count_.load(std::memory_order_acquire);
  for (std::size_t i = 0; i < n; ++i) {
    const Slot& s = slots_[i];
    TrialManager m;
    m.id = s.id;
    m.label = s.label;
    m.null = s.null.load(std::memory_order_relaxed);
    m.null_runs = s.null_runs.load(std::memory_order_relaxed);
    m.null_longest = s.null_longest.load(std::memory_order_relaxed);
    m.null_world = s.null_world.load(std::memory_order_relaxed);
    m.same = s.same.load(std::memory_order_relaxed);
    m.other = s.other.load(std::memory_order_relaxed);
    m.fault = s.fault.load(std::memory_order_relaxed);
    m.torn = s.torn.load(std::memory_order_relaxed);
    out.managers.push_back(std::move(m));
  }
  std::sort(out.managers.begin(), out.managers.end(),
            [](const TrialManager& a, const TrialManager& b) { return a.id < b.id; });
  return out;
}

// ---- The clock search ----

ClockSearch::ClockSearch(std::vector<std::uint32_t> statics, std::uint64_t base, std::uint64_t size)
    : base_(base), size_(size) {
  if (statics.size() > kMaxCandidates) statics.resize(kMaxCandidates);
  candidates_.resize(statics.size());
  for (std::size_t i = 0; i < statics.size(); ++i) candidates_[i].rva = statics[i];
}

void ClockSearch::compare(Candidate& c, std::span<const std::byte> now) noexcept {
  const auto value = [](std::span<const std::byte> bytes, std::size_t field) -> std::uint32_t {
    if (field < kSpan) return static_cast<std::uint32_t>(bytes[field]);
    std::uint32_t v = 0;
    std::memcpy(&v, bytes.data() + (field - kSpan) * 4, 4);
    return v;
  };
  for (std::size_t f = 0; f < kFields; ++f) {
    const std::size_t end = f < kSpan ? f + 1 : (f - kSpan) * 4 + 4;
    if (end > now.size()) continue;  // past what this object's copy holds
    Field& field = c.fields[f];
    const std::uint32_t v = value(now, f);
    if (v >= 60) field.minute_dead = 1;
    if (v >= 24) field.hour_dead = 1;
    if (!c.seen) continue;
    const std::uint32_t was = value(c.last, f);
    if (field.minute_dead == 0 && was < 60) {
      const std::uint32_t d = (v + 60 - was) % 60;
      if (d >= 1 && d <= 5) {
        if (field.ticks < 0xFFFF) ++field.ticks;
        if (v < was && field.wraps < 0xFFFF) ++field.wraps;
      } else if (d != 0) {
        field.minute_dead = 1;  // a jump, or a step back: no clock's minutes
      }
    }
    if (field.hour_dead == 0 && was < 24) {
      const std::uint32_t d = (v + 24 - was) % 24;
      if (d == 1) {
        if (field.hour_steps < 0xFFFF) ++field.hour_steps;
      } else if (d != 0) {
        field.hour_dead = 1;
      }
    }
  }
}

bool ClockSearch::step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies,
                       std::uint64_t now_us) {
  bool ended = false;
  for (std::uint32_t done = 0; done < copies && !candidates_.empty(); ++done) {
    Candidate& c = candidates_[next_];
    if (!c.dead && (c.last_us == 0 || now_us >= c.last_us + kMinIntervalUs)) {
      c.last_us = now_us;
      read_in_sweep_ = true;
      const auto slot = reader.copy<std::uint64_t>(gt, proto::Group::Session, base_ + c.rva);
      const std::uint64_t instance = slot ? *slot : 0;
      // No object, or one that lies in the image (the clock is a heap object): nothing to compare this time.
      const bool outside = instance != 0 && (instance < base_ || instance - base_ >= size_);
      if (!outside) {
        c.seen = false;
      } else {
        if (instance != c.instance) c.seen = false;  // another object: its first copy is compared with nothing
        std::array<std::byte, kSpan> now{};
        bool copied = false;
        while (!copied) {
          copied = reader.copy_into(gt, proto::Group::Session, instance, std::span<std::byte>(now).first(c.span), 8)
                       .has_value();
          if (copied) break;
          if (reader.spent() || c.span <= 0x10) break;
          c.span = c.span > 0x40 ? 0x40 : c.span > 0x20 ? 0x20 : 0x10;  // a smaller object: its end is nearer
          c.seen = false;
        }
        if (!copied) {
          if (!reader.spent()) {
            c.dead = true;
            unreadable_.fetch_add(1, std::memory_order_relaxed);
          }
        } else {
          compare(c, std::span<const std::byte>(now).first(c.span));
          c.last = now;
          c.instance = instance;
          c.seen = true;
        }
      }
    }
    if (++next_ >= candidates_.size()) {
      next_ = 0;
      if (read_in_sweep_) {
        read_in_sweep_ = false;
        ended = true;
        publish();
        passes_.fetch_add(1, std::memory_order_release);
      }
      break;
    }
  }
  return ended;
}

void ClockSearch::publish() noexcept {
  std::array<ClockHit, kMaxHits> best{};
  std::size_t n = 0;
  for (const Candidate& c : candidates_) {
    for (std::size_t f = 0; f < kFields; ++f) {
      const Field& field = c.fields[f];
      if (field.minute_dead != 0 || field.ticks < kMinTicks) continue;
      ClockHit hit;
      hit.rva = c.rva;
      hit.width = f < kSpan ? 1 : 4;
      hit.offset = static_cast<std::uint32_t>(f < kSpan ? f : (f - kSpan) * 4);
      hit.ticks = field.ticks;
      hit.wraps = field.wraps;
      // The hours: a field of the same size right beside it, before or after, that behaved as hours do.
      const std::size_t first = f < kSpan ? 0 : kSpan;
      const std::size_t count = f < kSpan ? kSpan : kSpan / 4;
      for (const int step : {-1, 1}) {
        const std::size_t at = f - first;
        if ((step < 0 && at == 0) || (step > 0 && at + 1 >= count)) continue;
        const std::size_t g = f + static_cast<std::size_t>(step);
        const Field& hour = c.fields[g];
        if (hour.hour_dead != 0 || hour.hour_steps == 0 || hit.has_hour) continue;
        hit.has_hour = true;
        hit.hour_offset = static_cast<std::uint32_t>(g < kSpan ? g : (g - kSpan) * 4);
        hit.hour_steps = hour.hour_steps;
      }
      // Kept in order: a clock (with its hours) first, then the most ticks.
      const auto better = [](const ClockHit& a, const ClockHit& b) {
        return a.has_hour != b.has_hour ? a.has_hour : a.ticks > b.ticks;
      };
      std::size_t at = n < kMaxHits ? n : kMaxHits;
      while (at > 0 && better(hit, best[at - 1])) --at;
      if (at >= kMaxHits) continue;
      for (std::size_t i = (n < kMaxHits ? n : kMaxHits - 1); i > at; --i) best[i] = best[i - 1];
      best[at] = hit;
      if (n < kMaxHits) ++n;
    }
  }
  hit_count_.store(0, std::memory_order_release);
  hits_ = best;
  hit_count_.store(n, std::memory_order_release);
}

ClockFound ClockSearch::found() const {
  ClockFound out;
  out.candidates = candidates_.size();
  out.passes = passes_.load(std::memory_order_acquire);
  out.unreadable = unreadable_.load(std::memory_order_relaxed);
  const std::size_t n = hit_count_.load(std::memory_order_acquire);
  out.hits.assign(hits_.begin(), hits_.begin() + static_cast<std::ptrdiff_t>(n < kMaxHits ? n : kMaxHits));
  return out;
}

// ---- The member search ----

MemberSearch::MemberSearch(std::vector<std::uint32_t> statics, std::vector<Namer::LeadTable> tables, std::uint64_t base,
                           std::uint64_t size)
    : base_(base), size_(size), statics_(std::move(statics)) {
  if (statics_.size() > kMaxStatics) statics_.resize(kMaxStatics);
  for (Namer::LeadTable& t : tables) {
    std::uint32_t lead = 0;
    while (lead < leads_.size() && (leads_[lead].id != t.id || leads_[lead].label != t.label)) ++lead;
    if (lead == leads_.size()) leads_.push_back(Lead{t.id, std::move(t.label)});
    tables_.push_back(Table{base + t.vtable, lead});
  }
  std::sort(tables_.begin(), tables_.end(), [](const Table& a, const Table& b) { return a.address < b.address; });
}

const MemberSearch::Table* MemberSearch::table_at(std::uint64_t word) const noexcept {
  const auto it = std::lower_bound(tables_.begin(), tables_.end(), word,
                                   [](const Table& t, std::uint64_t w) { return t.address < w; });
  return it != tables_.end() && it->address == word ? &*it : nullptr;
}

void MemberSearch::hit(const Table& t, std::uint32_t rva, std::uint32_t offset, bool embedded) noexcept {
  const std::size_t n = hit_count_.load(std::memory_order_relaxed);
  for (std::size_t i = 0; i < n; ++i) {
    if (hits_[i].rva == rva && hits_[i].offset == offset && hits_[i].lead == t.lead) return;  // met in an earlier pass
  }
  if (n >= kMaxHits) return;
  hits_[n] = Hit{t.lead, rva, offset, static_cast<std::uint32_t>(t.address - base_), embedded};
  hit_count_.store(n + 1, std::memory_order_release);  // a reader sees a hit only once it is whole
}

bool MemberSearch::step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t reads,
                        std::uint64_t now_us) {
  if (statics_.empty() || tables_.empty()) return false;
  if (resting_) {
    if (now_us < pass_end_us_ + kPassEveryUs) return false;
    resting_ = false;
  }
  const auto in_image = [&](std::uint64_t v) { return v >= base_ && v - base_ < size_; };
  for (std::uint32_t done = 0; done < reads;) {
    const std::uint32_t rva = statics_[next_];
    if (copied_ == 0) {
      // The static's object: its slot, then its first bytes, at a shorter length where its memory ends sooner.
      ++done;
      const auto slot = reader.copy<std::uint64_t>(gt, proto::Group::Session, base_ + rva);
      if (!slot && reader.spent()) return false;
      instance_ = slot ? *slot : 0;
      std::uint32_t span = kSpan;
      bool copied = false;
      if (instance_ != 0) {
        while (!copied) {
          copied = reader.copy_into(gt, proto::Group::Session, instance_, std::span<std::byte>(copy_).first(span), 8)
                       .has_value();
          if (copied) break;
          if (reader.spent()) return false;  // the same static again at the next step
          if (span <= 0x10) break;
          span /= 2;
        }
        objects_.fetch_add(1, std::memory_order_relaxed);
        if (!copied) unreadable_.fetch_add(1, std::memory_order_relaxed);
      }
      copied_ = copied ? span : 0;
      word_ = 8;
      if (!copied) copied_ = 0;
    }
    // The words of the copy, from where the last step stopped.
    while (copied_ != 0 && word_ + 8 <= copied_ && done < reads) {
      std::uint64_t w = 0;
      std::memcpy(&w, copy_.data() + word_, 8);
      const std::uint32_t at = word_;
      word_ += 8;
      if (const Table* t = table_at(w)) {
        hit(*t, rva, at, true);
        continue;
      }
      if (w < kLowestAddress || w > kAddressEnd || w % 8 != 0 || in_image(w) || w == instance_) continue;
      ++done;
      pointers_.fetch_add(1, std::memory_order_relaxed);
      const auto first = reader.copy<std::uint64_t>(gt, proto::Group::Session, w);
      if (!first) {
        if (reader.spent()) {
          word_ = at;  // not judged: the same word again at the next step
          return false;
        }
        continue;
      }
      if (const Table* t = table_at(*first)) hit(*t, rva, at, false);
    }
    if (copied_ != 0 && word_ + 8 <= copied_) return false;  // this step's reads are spent inside the object
    // On to the next static; after the last, the pass has ended.
    copied_ = 0;
    if (++next_ >= statics_.size()) {
      next_ = 0;
      passes_.fetch_add(1, std::memory_order_release);
      pass_end_us_ = now_us;
      resting_ = true;
      return true;
    }
  }
  return false;
}

MembersFound MemberSearch::found() const {
  MembersFound out;
  out.objects = objects_.load(std::memory_order_relaxed);
  out.unreadable = unreadable_.load(std::memory_order_relaxed);
  out.pointers = pointers_.load(std::memory_order_relaxed);
  out.passes = passes_.load(std::memory_order_acquire);
  const std::size_t n = hit_count_.load(std::memory_order_acquire);
  for (std::size_t i = 0; i < n && i < kMaxHits; ++i) {
    const Hit& h = hits_[i];
    out.hits.push_back(MemberHit{leads_[h.lead].id, leads_[h.lead].label, h.rva, h.offset, h.embedded, h.vtable});
  }
  return out;
}

// ---- The watch ----

struct Watch::Object {
  static constexpr std::size_t kBits = 59;  // a run of 6 bits starts at bit 0 to 58 of a word
  struct Half {
    std::uint16_t changes = 0;
    std::uint16_t rises = 0;
    std::uint16_t float_rises = 0;
    float rose_by = 0.0f;  // the sum of its rises, read as a fraction
  };
  struct Run {
    std::uint8_t minute_dead = 0;
    std::uint8_t hour_dead = 0;
    std::uint16_t ticks = 0;
    std::uint16_t wraps = 0;
    std::uint16_t jumps = 0;
    std::uint16_t hour_steps = 0;
    std::uint16_t hour_jumps = 0;
  };
  Way way;
  std::uint32_t span = kSpan;  // what is copied: the way's, shortened where the object's memory ends sooner
  std::uint64_t address = 0;   // the object the last reading was of; 0: none
  std::uint64_t last_us = 0;
  std::uint64_t kind_us = 0;
  bool seen = false;           // `last` holds a copy of the same object
  bool kinds_seen = false;
  std::vector<std::byte> last;    // the way's span
  std::vector<std::byte> now;
  std::vector<Half> halves;       // two a word
  std::vector<std::uint8_t> kinds;
  std::vector<std::uint16_t> flips;
  std::vector<std::uint8_t> lived;  // the word was once not zero, or once changed
  std::vector<Run> runs;            // by word and bit, for a clock's way
  std::uint64_t elapsed_us = 0;     // between the readings compared
  std::atomic<std::uint64_t> readings{0};
  std::atomic<std::uint64_t> empty{0};
  std::atomic<std::uint64_t> unreadable{0};
  std::atomic<std::uint64_t> moved{0};
  std::atomic<std::uint64_t> vtable_same{0};
  std::atomic<std::uint64_t> same_address{0};
  std::atomic<std::uint32_t> vtable{0};
  std::atomic<std::uint32_t> watched{0};  // the span of the last reading
};

Watch::Watch(std::uint64_t base, std::uint64_t size, std::vector<std::uint32_t> primary, Live live)
    : base_(base), size_(size), primary_(std::move(primary)), live_(std::move(live)) {
  std::sort(primary_.begin(), primary_.end());
}

Watch::~Watch() = default;

bool Watch::add(const Way& way) {
  const std::size_t n = count_.load(std::memory_order_acquire);
  if (n >= kMaxObjects) return false;
  for (std::size_t i = 0; i < n; ++i) {
    const Way& w = objects_[i]->way;
    if (w.static_rva == way.static_rva && w.offset == way.offset && w.deref == way.deref && w.wide == way.wide) {
      return false;  // the same way; a wide way and a plain one to the same object may stand side by side
    }
  }
  auto o = std::make_unique<Object>();
  o->way = way;
  o->way.span = std::clamp<std::uint32_t>(way.span, 16, kMaxSpan) & ~7u;
  if (o->way.every_us == 0) o->way.every_us = kEveryUs;
  o->span = o->way.span;
  const std::size_t words = o->span / 8;
  o->last.assign(o->span, std::byte{0});
  o->now.assign(o->span, std::byte{0});
  o->halves.assign(words * 2, Object::Half{});
  o->kinds.assign(words, 0);
  o->flips.assign(words, 0);
  o->lived.assign(words, 0);
  if (way.clock) o->runs.assign(words * Object::kBits, Object::Run{});
  objects_[n] = std::move(o);  // not yet the reader's: it sees an object only once the count says so
  count_.store(n + 1, std::memory_order_release);
  return true;
}

WordKind Watch::kind_of(std::uint64_t word) const {
  if (word == 0) return WordKind::Zero;
  if (word < kLowestAddress) return WordKind::Small;
  if (word >= base_ && word - base_ < size_) {
    const auto rva = static_cast<std::uint32_t>(word - base_);
    return std::binary_search(primary_.begin(), primary_.end(), rva) ? WordKind::Vtable : WordKind::Image;
  }
  if (word <= kAddressEnd && word % 8 == 0 && live_ && live_(word)) return WordKind::Heap;
  return WordKind::Other;
}

void Watch::compare(Object& o, std::span<const std::byte> now, std::uint64_t now_us) noexcept {
  const std::size_t words = now.size() / 8;
  const auto count = [](std::uint16_t& n) {
    if (n < 0xFFFF) ++n;
  };
  if (o.seen) {
    o.elapsed_us += now_us - o.last_us;
    for (std::size_t w = 0; w < words; ++w) {
      std::uint64_t a = 0, b = 0;
      std::memcpy(&a, o.last.data() + w * 8, 8);
      std::memcpy(&b, now.data() + w * 8, 8);
      if (b != 0) o.lived[w] = 1;
      if (a == b) continue;  // nothing to count: what the word holds was judged when it last changed
      for (std::size_t h = 0; h < 2; ++h) {
        const auto x = static_cast<std::uint32_t>(a >> (h * 32));
        const auto y = static_cast<std::uint32_t>(b >> (h * 32));
        if (x == y) continue;
        Object::Half& half = o.halves[w * 2 + h];
        o.lived[w] = 1;
        count(half.changes);
        if (y > x) count(half.rises);
        if (!o.runs.empty()) {
          // Read as a fraction: a rise between two plain, positive numbers of an ordinary size.
          float f = 0.0f, g = 0.0f;
          std::memcpy(&f, &x, 4);
          std::memcpy(&g, &y, 4);
          if (f >= 1e-3f && g > f && g <= 1e7f) {
            count(half.float_rises);
            half.rose_by += g - f;
          }
        }
      }
      if (o.runs.empty()) continue;
      // Every run of 6 bits as a clock's minutes, and of 5 bits as its hours.
      for (std::size_t bit = 0; bit < Object::kBits; ++bit) {
        Object::Run& run = o.runs[w * Object::kBits + bit];
        const auto was = static_cast<std::uint32_t>((a >> bit) & 63);
        const auto is = static_cast<std::uint32_t>((b >> bit) & 63);
        if (is >= 60) run.minute_dead = 1;
        if (run.minute_dead == 0 && was < 60 && is != was) {
          const std::uint32_t d = (is + 60 - was) % 60;
          if (d >= 1 && d <= 5) {
            count(run.ticks);
            if (is < was) count(run.wraps);
          } else {
            count(run.jumps);
          }
        }
        const std::uint32_t hour_was = was & 31, hour_is = is & 31;
        if (hour_is >= 24) run.hour_dead = 1;
        if (run.hour_dead == 0 && hour_was < 24 && hour_is != hour_was) {
          if ((hour_is + 24 - hour_was) % 24 == 1) {
            count(run.hour_steps);
          } else {
            count(run.hour_jumps);
          }
        }
      }
    }
  } else {
    for (std::size_t w = 0; w < words; ++w) {
      std::uint64_t b = 0;
      std::memcpy(&b, now.data() + w * 8, 8);
      if (b != 0) o.lived[w] = 1;
      if (o.runs.empty()) continue;
      for (std::size_t bit = 0; bit < Object::kBits; ++bit) {
        Object::Run& run = o.runs[w * Object::kBits + bit];
        if (((b >> bit) & 63) >= 60) run.minute_dead = 1;
        if (((b >> bit) & 31) >= 24) run.hour_dead = 1;
      }
    }
  }
  // What each word looks like, now and then: a look asks about the memory behind an address, and reads nothing.
  if (!o.kinds_seen || now_us - o.kind_us >= kKindEveryUs) {
    for (std::size_t w = 0; w < words; ++w) {
      std::uint64_t b = 0;
      std::memcpy(&b, now.data() + w * 8, 8);
      const auto kind = static_cast<std::uint8_t>(kind_of(b));
      if (o.kinds_seen && kind != o.kinds[w]) count(o.flips[w]);
      o.kinds[w] = kind;
    }
    o.kinds_seen = true;
    o.kind_us = now_us;
  }
}

bool Watch::step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t objects, std::uint64_t now_us) {
  const std::size_t n = count_.load(std::memory_order_acquire);
  if (n == 0) return false;
  bool ended = false;
  const auto due = [&](const Object& o) { return o.last_us == 0 || now_us >= o.last_us + o.way.every_us; };
  for (std::uint32_t done = 0; done < objects; ++done) {
    // The next way that is due: the plain ones first, each in its turn; a wide one only when no plain one is due.
    std::size_t index = n;
    for (std::size_t k = 0; k < n && index == n; ++k) {
      const std::size_t i = (next_ + k) % n;
      if (!objects_[i]->way.wide && due(*objects_[i])) index = i;
    }
    if (index != n) {
      next_ = (index + 1) % n;
      read_in_sweep_ = true;
    } else {
      // Every plain way has been followed: a pass has ended, if anything was read in it.
      if (read_in_sweep_) {
        read_in_sweep_ = false;
        passes_.fetch_add(1, std::memory_order_release);
        ended = true;
      }
      for (std::size_t k = 0; k < n && index == n; ++k) {
        const std::size_t i = (next_wide_ + k) % n;
        if (objects_[i]->way.wide && due(*objects_[i])) index = i;
      }
      if (index == n) break;  // nothing is due
      next_wide_ = (index + 1) % n;
    }
    Object& o = *objects_[index];
    {
      // The way: the static's slot, the offset, and for a pointer kept there one more read.
      const auto slot = reader.copy<std::uint64_t>(gt, proto::Group::Session, base_ + o.way.static_rva);
      if (!slot && reader.spent()) return false;
      std::uint64_t at = slot && *slot != 0 ? *slot + o.way.offset : 0;
      bool broken = false;  // an address on the way could not be read
      if (at != 0 && o.way.deref) {
        const auto kept = reader.copy<std::uint64_t>(gt, proto::Group::Session, at);
        if (!kept && reader.spent()) return false;
        broken = !kept;
        at = kept ? *kept : 0;
      }
      std::vector<std::byte>& now = o.now;
      bool copied = false;
      if (at != 0) {
        while (!copied) {
          copied = reader.copy_into(gt, proto::Group::Session, at, std::span<std::byte>(now).first(o.span), 8)
                       .has_value();
          if (copied) break;
          if (reader.spent()) return false;  // the same way again at the next step
          if (o.span <= 0x10) break;
          o.span /= 2;  // a smaller object: its end is nearer
          o.seen = false;
        }
      }
      if (!copied) {
        (at == 0 && !broken ? o.empty : o.unreadable).fetch_add(1, std::memory_order_relaxed);
        o.seen = false;
        o.address = 0;
      } else {
        if (o.seen && at != o.address) {
          o.moved.fetch_add(1, std::memory_order_relaxed);
          o.seen = false;  // another object: its first copy is compared with nothing
        }
        o.readings.fetch_add(1, std::memory_order_relaxed);
        o.watched.store(o.span, std::memory_order_relaxed);
        std::uint64_t first = 0;
        std::memcpy(&first, now.data(), 8);
        const bool in_image = first >= base_ && first - base_ < size_;
        const std::uint32_t rva = in_image ? static_cast<std::uint32_t>(first - base_) : 0;
        o.vtable.store(rva != 0 && std::binary_search(primary_.begin(), primary_.end(), rva) ? rva : 0,
                       std::memory_order_relaxed);
        if (o.way.expect != 0 && rva == o.way.expect) o.vtable_same.fetch_add(1, std::memory_order_relaxed);
        // Is it the very object the first way of the same label led to at its last reading?
        for (std::size_t i = 0; i < index; ++i) {
          if (objects_[i]->way.label != o.way.label) continue;
          if (objects_[i]->address == at) o.same_address.fetch_add(1, std::memory_order_relaxed);
          break;
        }
        compare(o, std::span<const std::byte>(now).first(o.span), now_us);
        std::memcpy(o.last.data(), now.data(), o.span);
        o.address = at;
        o.seen = true;
      }
      o.last_us = now_us;
    }
  }
  return ended;
}

WatchFound Watch::found() const {
  WatchFound out;
  out.passes = passes_.load(std::memory_order_acquire);
  const std::size_t n = count_.load(std::memory_order_acquire);
  std::size_t wide_listed = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const Object& o = *objects_[i];
    WatchObject x;
    x.label = o.way.label;
    x.id = o.way.id;
    x.rva = o.way.static_rva;
    x.offset = o.way.offset;
    x.deref = o.way.deref;
    x.clock = o.way.clock;
    x.wide = o.way.wide;
    x.readings = o.readings.load(std::memory_order_relaxed);
    x.empty = o.empty.load(std::memory_order_relaxed);
    x.unreadable = o.unreadable.load(std::memory_order_relaxed);
    x.moved = o.moved.load(std::memory_order_relaxed);
    x.vtable_same = o.vtable_same.load(std::memory_order_relaxed);
    x.same_address = o.same_address.load(std::memory_order_relaxed);
    x.span = o.watched.load(std::memory_order_relaxed);
    x.vtable = o.vtable.load(std::memory_order_relaxed);
    const std::size_t words = x.span / 8;
    for (std::size_t w = 0; w < words && w < o.lived.size() && !o.way.wide; ++w) {
      if (o.lived[w] == 0) continue;  // never anything but zero, and never changed
      WatchWord word;
      word.offset = static_cast<std::uint32_t>(w * 8);
      word.kind = static_cast<WordKind>(o.kinds[w] <= 5 ? o.kinds[w] : 5);
      word.flips = o.flips[w];
      word.changes_low = o.halves[w * 2].changes;
      word.rises_low = o.halves[w * 2].rises;
      word.changes_high = o.halves[w * 2 + 1].changes;
      word.rises_high = o.halves[w * 2 + 1].rises;
      x.words.push_back(word);
    }
    if (!o.runs.empty()) {
      // The runs that behaved as minutes: forward steps well above jumps; one that went round before one that did
      // not, then the most steps.
      std::vector<BitHit> hits;
      for (std::size_t w = 0; w < words && w < o.lived.size(); ++w) {
        for (std::size_t bit = 0; bit < Object::kBits; ++bit) {
          const Object::Run& run = o.runs[w * Object::kBits + bit];
          if (run.minute_dead != 0 || run.ticks < kMinTicks || run.ticks <= 4u * run.jumps) continue;
          BitHit h;
          h.offset = static_cast<std::uint32_t>(w * 8);
          h.bit = static_cast<std::uint32_t>(bit);
          h.ticks = run.ticks;
          h.wraps = run.wraps;
          h.jumps = run.jumps;
          // Its hours: a run of 5 bits in the same word or the one beside it that stepped by one, about as often
          // as the minutes went round, and never otherwise.
          std::uint32_t best_gap = ~0u;
          for (std::size_t v = (w == 0 ? 0 : w - 1); v <= w + 1 && v < words && v < o.lived.size(); ++v) {
            for (std::size_t b = 0; b < Object::kBits; ++b) {
              const Object::Run& hour = o.runs[v * Object::kBits + b];
              if (hour.hour_dead != 0 || hour.hour_steps == 0 || hour.hour_jumps != 0) continue;
              const std::uint32_t gap = hour.hour_steps > run.wraps ? hour.hour_steps - run.wraps
                                                                    : run.wraps - hour.hour_steps;
              if (gap > 1 || gap >= best_gap) continue;
              best_gap = gap;
              h.has_hour = true;
              h.hour_offset = static_cast<std::uint32_t>(v * 8);
              h.hour_bit = static_cast<std::uint32_t>(b);
              h.hour_steps = hour.hour_steps;
            }
          }
          hits.push_back(h);
        }
      }
      std::stable_sort(hits.begin(), hits.end(), [](const BitHit& a, const BitHit& b) {
        if ((a.wraps != 0) != (b.wraps != 0)) return a.wraps != 0;
        if (a.has_hour != b.has_hour) return a.has_hour;
        return a.ticks - 4u * a.jumps > b.ticks - 4u * b.jumps;
      });
      if (hits.size() > kMaxBitHits) hits.resize(kMaxBitHits);
      x.bits = std::move(hits);
      // The fields that kept rising as fractions: in four readings of five at least.
      const std::uint64_t compared = x.readings > x.moved + 1 ? x.readings - x.moved - 1 : 0;
      std::vector<FloatHit> floats;
      for (std::size_t f = 0; f < words * 2 && f < o.halves.size(); ++f) {
        const Object::Half& half = o.halves[f];
        if (half.float_rises < 10 || std::uint64_t{half.float_rises} * 5 < compared * 4 || o.elapsed_us == 0) continue;
        FloatHit h;
        h.offset = static_cast<std::uint32_t>(f * 4);
        h.rises = half.float_rises;
        const double rate = static_cast<double>(half.rose_by) * 1e6 / static_cast<double>(o.elapsed_us) * 1000.0;
        h.rate_milli = rate >= 4.0e9 ? 0xFFFFFFFFu : static_cast<std::uint32_t>(rate);
        floats.push_back(h);
      }
      std::stable_sort(floats.begin(), floats.end(), [](const FloatHit& a, const FloatHit& b) { return a.rises > b.rises; });
      if (floats.size() > kMaxFloatHits) floats.resize(kMaxFloatHits);
      x.floats = std::move(floats);
    }
    if (o.way.wide) {
      // One of the many: listed only where a run of bits went round as minutes do with hours beside it, or a
      // fraction kept rising; and no more than kMaxWideListed of them.
      ++out.wide_ways;
      const bool clock_like = std::any_of(x.bits.begin(), x.bits.end(),
                                          [](const BitHit& h) { return h.wraps != 0 && h.has_hour; });
      if (!clock_like && x.floats.empty()) continue;
      if (wide_listed >= kMaxWideListed) continue;
      ++wide_listed;
      if (x.bits.size() > 4) x.bits.resize(4);
      if (x.floats.size() > 4) x.floats.resize(4);
    }
    out.objects.push_back(std::move(x));
  }
  return out;
}

}  // namespace en::gal::lodestar
