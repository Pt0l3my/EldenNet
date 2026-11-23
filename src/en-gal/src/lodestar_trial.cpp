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
    // As a field that keeps rising (a clock's way only): how it is read, its rises at a pace, its pace, its jumps.
    std::uint8_t judged = 0;   // 0: not yet; 1: a fraction; 2: a whole number
    std::uint16_t steady = 0;
    std::uint16_t jumps = 0;
    float pace = 0.0f;         // a second, the running mean of its steady rises
    std::array<std::uint32_t, kJumpsKept> jump_s{};
    std::array<float, kJumpsKept> jump_from{};
    std::array<float, kJumpsKept> jump_to{};
  };
  struct Run {
    std::uint8_t minute_dead = 0;
    std::uint8_t hour_dead = 0;
    std::uint16_t ticks = 0;
    std::uint16_t wraps = 0;
    std::uint16_t jumps = 0;
    std::uint16_t hour_steps = 0;
    std::uint16_t hour_jumps = 0;
    std::uint32_t hour_seen = 0;                     // the hours it has held, a bit each
    std::array<std::uint16_t, kJumpsKept> jump_s{};  // an hour's first jumps: when, from what, to what
    std::array<std::uint8_t, kJumpsKept> jump_from{};
    std::array<std::uint8_t, kJumpsKept> jump_to{};
  };
  Way way;
  std::uint32_t span = kSpan;  // what is copied: the way's, shortened where the object's memory ends sooner
  std::uint64_t address = 0;   // the object the last reading was of; 0: none
  std::uint64_t last_us = 0;
  std::uint64_t kind_us = 0;
  bool seen = false;           // `last` holds a copy of the same object
  bool kinds_seen = false;
  bool changed = false;        // a word once differed from the reading before
  std::uint32_t compared = 0;  // readings compared with the one before
  std::uint64_t far_kind_us = 0;  // when the words past the first kSpan bytes were last looked at
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
    if (w.static_rva == way.static_rva && w.offset == way.offset && w.deref == way.deref && w.wide == way.wide &&
        w.deeper == way.deeper && (!w.deeper || w.via == way.via)) {
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
    if (o.compared < 0xFFFFFFFFu) ++o.compared;
    const double dt = static_cast<double>(now_us - o.last_us) / 1e6;
    const auto second = static_cast<std::uint32_t>((now_us - start_us_) / 1'000'000);
    for (std::size_t w = 0; w < words; ++w) {
      std::uint64_t a = 0, b = 0;
      std::memcpy(&a, o.last.data() + w * 8, 8);
      std::memcpy(&b, now.data() + w * 8, 8);
      if (b != 0) o.lived[w] = 1;
      if (a == b) continue;  // nothing to count: what the word holds was judged when it last changed
      o.changed = true;
      for (std::size_t h = 0; h < 2; ++h) {
        const auto x = static_cast<std::uint32_t>(a >> (h * 32));
        const auto y = static_cast<std::uint32_t>(b >> (h * 32));
        if (x == y) continue;
        Object::Half& half = o.halves[w * 2 + h];
        o.lived[w] = 1;
        count(half.changes);
        if (y > x) count(half.rises);
        if (!o.runs.empty() && dt > 0.0) {
          // As a field that keeps rising: read as a fraction where both readings are plain, positive numbers of an
          // ordinary size, as a whole number where both are below every such fraction's bits. A rise at about the
          // pace of the rises before is steady; one far above it, or a fall, is a jump once the field has risen
          // steadily a few times. Before that a fall starts the count again.
          float f = 0.0f, g = 0.0f;
          std::memcpy(&f, &x, 4);
          std::memcpy(&g, &y, 4);
          const bool plain_f = f >= 1e-3f && f <= 1e7f, plain_g = g >= 1e-3f && g <= 1e7f;
          const bool low_x = x < 0x3A000000u, low_y = y < 0x3A000000u;
          const std::uint8_t as = plain_f && plain_g ? 1 : low_x && low_y ? 2 : 0;
          if (half.steady == 0) half.judged = as;
          if (half.judged != 0) {
            // Once judged a fraction, any reading from nothing up to that size is one (a clock passes midnight);
            // anything else there reads as nothing.
            const bool fraction = half.judged == 1;
            const auto part = [](float v) { return v >= 0.0f && v <= 1e7f ? static_cast<double>(v) : 0.0; };
            const double from = fraction ? part(f) : static_cast<double>(x);
            const double to = fraction ? part(g) : static_cast<double>(y);
            const double rate = (to - from) / dt;
            const bool established = half.steady >= kMinSteady;
            if (to > from && !(established && rate > kPaceFactor * static_cast<double>(half.pace))) {
              count(half.steady);
              const double n = half.steady < 16 ? half.steady : 16;
              half.pace = static_cast<float>(static_cast<double>(half.pace) +
                                             (rate - static_cast<double>(half.pace)) / n);
            } else if (established) {
              if (half.jumps < kJumpsKept) {
                half.jump_s[half.jumps] = second;
                half.jump_from[half.jumps] = static_cast<float>(from);
                half.jump_to[half.jumps] = static_cast<float>(to);
              }
              count(half.jumps);
            } else {
              half.steady = 0;
              half.pace = 0.0f;
              half.judged = 0;
            }
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
        run.hour_seen |= (1u << hour_was) | (1u << hour_is);
        if (run.hour_dead == 0 && hour_was < 24 && hour_is != hour_was) {
          if ((hour_is + 24 - hour_was) % 24 == 1) {
            count(run.hour_steps);
          } else {
            if (run.hour_jumps < kJumpsKept) {
              run.jump_s[run.hour_jumps] = static_cast<std::uint16_t>(second < 0xFFFFu ? second : 0xFFFFu);
              run.jump_from[run.hour_jumps] = static_cast<std::uint8_t>(hour_was);
              run.jump_to[run.hour_jumps] = static_cast<std::uint8_t>(hour_is);
            }
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
  // A wide way's words are never written, so they are not looked at; a long way's words past the first kSpan bytes
  // are looked at seldom (a look asks Windows once for every word that may be an address).
  if (!o.way.wide && (!o.kinds_seen || now_us - o.kind_us >= kKindEveryUs)) {
    const bool far = !o.kinds_seen || now_us - o.far_kind_us >= 30 * kKindEveryUs;
    if (far) o.far_kind_us = now_us;
    const std::size_t looked = far ? words : std::min<std::size_t>(words, kSpan / 8);
    for (std::size_t w = 0; w < looked; ++w) {
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
  if (start_us_ == 0) start_us_ = now_us;
  // A wide way whose object has not changed in its first comparisons is followed more seldom: most statics' objects
  // never change, and the others want the reads.
  const auto due = [&](const Object& o) {
    const bool quiet = o.way.wide && !o.changed && o.compared >= kQuietAfter;
    return o.last_us == 0 || now_us >= o.last_us + o.way.every_us * (quiet ? kQuietSlower : 1);
  };
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
        if (at != 0 && o.way.deeper) {
          const auto next = reader.copy<std::uint64_t>(gt, proto::Group::Session, at + o.way.via);
          if (!next && reader.spent()) return false;
          broken = !next;
          at = next ? *next : 0;
        }
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
  std::vector<WatchObject> wide_found;
  for (std::size_t i = 0; i < n; ++i) {
    const Object& o = *objects_[i];
    WatchObject x;
    x.label = o.way.label;
    x.id = o.way.id;
    x.rva = o.way.static_rva;
    x.offset = o.way.offset;
    x.deref = o.way.deref;
    x.deeper = o.way.deeper;
    x.via = o.way.deeper ? o.way.via : 0;
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
    x.address = o.address;
    const std::size_t words = x.span / 8;
    for (std::size_t w = 0; w < words && w < o.lived.size() && !o.way.wide; ++w) {
      if (o.lived[w] == 0) continue;  // never anything but zero, and never changed
      // Past the first kSpan bytes only a word that changed, or looked like another kind of thing at another look,
      // is written: a long way's report must stay small.
      if (w >= kSpan / 8 && o.halves[w * 2].changes == 0 && o.halves[w * 2 + 1].changes == 0 && o.flips[w] == 0) {
        continue;
      }
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
          // (Two other changes are let be: the player may have let time pass. A run that held fewer hours than the
          // minutes' rounds call for is no hour: the minutes' own top bit goes up and down once a round.)
          std::uint32_t best_gap = ~0u;
          std::uint32_t best_jumps = ~0u;
          for (std::size_t v = (w == 0 ? 0 : w - 1); v <= w + 1 && v < words && v < o.lived.size(); ++v) {
            for (std::size_t b = 0; b < Object::kBits; ++b) {
              const Object::Run& hour = o.runs[v * Object::kBits + b];
              if (hour.hour_dead != 0 || hour.hour_steps == 0 || hour.hour_jumps > 2) continue;
              std::uint32_t held = 0;
              for (std::uint32_t seen = hour.hour_seen; seen != 0; seen &= seen - 1) ++held;
              if (held < std::min<std::uint32_t>(run.wraps + 1u, 3u)) continue;
              const std::uint32_t gap = hour.hour_steps > run.wraps ? hour.hour_steps - run.wraps
                                                                    : run.wraps - hour.hour_steps;
              if (gap > 1) continue;
              // The one that never changed otherwise first, then the nearest count.
              if (hour.hour_jumps > best_jumps || (hour.hour_jumps == best_jumps && gap >= best_gap)) continue;
              best_gap = gap;
              best_jumps = hour.hour_jumps;
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
      // The fields that kept rising at a pace, in two readings of three at least (a clock stands still in a menu and
      // at the title): those that jumped a few times
      // first (what a clock does when time is let pass), then fractions before whole numbers, then the most rises.
      const std::uint64_t compared = x.readings > x.moved + 1 ? x.readings - x.moved - 1 : 0;
      const auto few = [](std::uint32_t jumps) { return jumps >= 1 && jumps <= 8; };
      std::vector<RiseHit> rises;
      for (std::size_t f = 0; f < words * 2 && f < o.halves.size(); ++f) {
        const Object::Half& half = o.halves[f];
        if (half.judged == 0 || half.steady < 10 || std::uint64_t{half.steady} * 3 < compared * 2) continue;
        RiseHit h;
        h.offset = static_cast<std::uint32_t>(f * 4);
        h.whole = half.judged == 2;
        h.rises = half.steady;
        h.jumps = half.jumps;
        const double rate = static_cast<double>(half.pace) * 1e6;
        h.rate_micro = rate >= 1.8e19 ? ~std::uint64_t{0} : rate <= 0.0 ? 0 : static_cast<std::uint64_t>(rate);
        rises.push_back(h);
      }
      std::stable_sort(rises.begin(), rises.end(), [&](const RiseHit& a, const RiseHit& b) {
        if (few(a.jumps) != few(b.jumps)) return few(a.jumps);
        if (a.whole != b.whole) return !a.whole;
        return a.rises > b.rises;
      });
      if (rises.size() > kMaxRiseHits) rises.resize(kMaxRiseHits);
      x.rises = std::move(rises);
      // The runs of 5 bits that behaved as hours, whatever lies beside them: never 24 or more, four different hours
      // held at least (a bit that goes to and fro holds two), two steps by one at least, no more other changes than
      // steps, and four of those at most (a small counter that goes round gathers more).
      std::vector<HourHit> hours;
      for (std::size_t w = 0; w < words && w < o.lived.size(); ++w) {
        for (std::size_t bit = 0; bit < Object::kBits; ++bit) {
          const Object::Run& run = o.runs[w * Object::kBits + bit];
          if (run.hour_dead != 0 || run.hour_steps < 2 || run.hour_jumps > 4) continue;
          if (run.hour_steps < run.hour_jumps) continue;
          std::uint32_t held = 0;
          for (std::uint32_t seen = run.hour_seen; seen != 0; seen &= seen - 1) ++held;
          if (held < 4) continue;
          hours.push_back(HourHit{static_cast<std::uint32_t>(w * 8), static_cast<std::uint32_t>(bit), run.hour_steps,
                                  run.hour_jumps});
        }
      }
      std::stable_sort(hours.begin(), hours.end(), [](const HourHit& a, const HourHit& b) {
        if ((a.jumps != 0) != (b.jumps != 0)) return a.jumps != 0;
        return a.steps > b.steps;
      });
      if (hours.size() > kMaxHourHits) hours.resize(kMaxHourHits);
      x.hours = std::move(hours);
      // The jumps of what is listed, each field's or run's first kJumpsKept, in the order they happened.
      const auto milli = [](float v) {
        const double m = static_cast<double>(v) * 1000.0 + 0.5;
        return m <= 0.0 ? std::uint64_t{0} : m >= 1.8e19 ? ~std::uint64_t{0} : static_cast<std::uint64_t>(m);
      };
      for (const RiseHit& h : x.rises) {
        if (!few(h.jumps)) continue;  // a field that jumps all the time is no clock: its jumps are not written
        const Object::Half& half = o.halves[h.offset / 4];
        for (std::size_t j = 0; j < kJumpsKept && j < half.jumps; ++j) {
          JumpEvent e;
          e.offset = h.offset;
          e.as = h.whole ? JumpAs::Whole : JumpAs::Fraction;
          e.second = half.jump_s[j];
          e.from = h.whole ? static_cast<std::uint64_t>(half.jump_from[j]) : milli(half.jump_from[j]);
          e.to = h.whole ? static_cast<std::uint64_t>(half.jump_to[j]) : milli(half.jump_to[j]);
          x.jumps.push_back(e);
        }
      }
      for (const HourHit& h : x.hours) {
        const Object::Run& run = o.runs[h.offset / 8 * Object::kBits + h.bit];
        for (std::size_t j = 0; j < kJumpsKept && j < run.hour_jumps; ++j) {
          x.jumps.push_back(JumpEvent{h.offset, h.bit, JumpAs::Hour, run.jump_s[j], run.jump_from[j], run.jump_to[j]});
        }
      }
      std::stable_sort(x.jumps.begin(), x.jumps.end(),
                       [](const JumpEvent& a, const JumpEvent& b) { return a.second < b.second; });
    }
    if (o.way.wide) {
      // One of the many: listed only where a run of bits went round as minutes do, twice at least, with hours
      // beside it: a clock. (The seventh round also listed rises that jumped and hours that stood alone; they found
      // the clock once and are noise beside it now.)
      ++out.wide_ways;
      std::erase_if(x.bits, [](const BitHit& h) { return h.wraps < 2 || !h.has_hour; });
      if (x.bits.empty()) continue;
      if (x.bits.size() > 4) x.bits.resize(4);
      x.rises.clear();
      x.hours.clear();
      x.jumps.clear();
      wide_found.push_back(std::move(x));
      continue;
    }
    out.objects.push_back(std::move(x));
  }
  // Of the wide ones at most kMaxWideListed, in the order they were added.
  if (wide_found.size() > kMaxWideListed) wide_found.resize(kMaxWideListed);
  for (WatchObject& x : wide_found) out.objects.push_back(std::move(x));
  return out;
}

// ---- The sweep ----

struct Sweep::State {
  Target target;
  std::uint64_t address = 0;   // where the target stood at the last pass; 0: nowhere yet
  bool followed = false;       // the way has been followed in the pass under way
  bool read_any = false;       // a piece of it was read in the pass under way
  bool tail = false;           // the piece under way could not be read whole: read to the page instead
  std::uint64_t cursor = 0;    // bytes from the target's start to the next piece
  std::vector<std::byte> copy;      // the target's stretch, as last read
  std::vector<std::uint8_t> held;   // by word: the copy holds it
  std::vector<std::uint8_t> busy;   // by word: how often it differed, to 255
  struct Page {
    std::uint16_t words = 0;  // words of it that ever differed
    std::uint8_t kept = 0;
    std::array<SweepFlip, kPageEvents> flips{};  // its first flips in words not busy
  };
  std::vector<Page> pages;          // by page of the stretch
  // The latest flips in words not busy, with when they were seen: a mark that comes after them takes them.
  struct Seen {
    SweepFlip flip;
    std::uint64_t at_us = 0;
  };
  std::array<Seen, kRecent> recent{};
  std::size_t recent_count = 0;     // ever put; the ring's place is this modulo kRecent
  // By mark: the flips seen around it, and how many (written after the flip is whole).
  std::array<std::array<SweepFlip, kMarkEvents>, kMaxMarks> marked{};
  std::array<std::atomic<std::uint32_t>, kMaxMarks> marked_count{};
  std::atomic<bool> retired{false};
  std::atomic<std::uint64_t> bytes{0};
  std::atomic<std::uint64_t> readings{0};
  std::atomic<std::uint64_t> empty{0};
  std::atomic<std::uint64_t> unreadable{0};
  std::atomic<std::uint64_t> moved{0};
  std::atomic<std::uint64_t> changed_words{0};
  std::atomic<std::uint64_t> busy_words{0};
  std::atomic<std::uint64_t> single_flips{0};
  std::atomic<std::uint64_t> other_changes{0};
};

Sweep::Sweep(std::uint64_t base) : base_(base), piece_(kPiece) {}

Sweep::~Sweep() = default;

bool Sweep::add(const Target& target) {
  const std::size_t n = count_.load(std::memory_order_acquire);
  if (n >= kMaxTargets) return false;
  for (std::size_t i = 0; i < n; ++i) {
    const Target& t = targets_[i]->target;
    if (t.static_rva == target.static_rva && t.offset == target.offset && t.deeper == target.deeper &&
        (!t.deeper || t.via == target.via)) {
      return false;
    }
  }
  auto t = std::make_unique<State>();
  t->target = target;
  t->target.pieces = std::clamp<std::uint32_t>(target.pieces, 1, kMaxPieces);
  const std::size_t size = std::size_t{t->target.pieces} * kPiece;
  t->copy.assign(size, std::byte{0});
  t->held.assign(size / 8, 0);
  t->busy.assign(size / 8, 0);
  t->pages.assign(size / kPage, State::Page{});
  targets_[n] = std::move(t);  // not yet the reader's: it sees a target only once the count says so
  count_.store(n + 1, std::memory_order_release);
  return true;
}

void Sweep::compare(State& t, std::uint64_t at, std::span<const std::byte> now, std::uint64_t now_us) noexcept {
  // `at` and the length are multiples of 8. A word is compared only where the copy holds it already.
  const auto second = static_cast<std::uint32_t>((now_us - start_us_) / 1'000'000);
  for (std::size_t i = 0; i + 8 <= now.size(); i += 8) {
    const std::uint64_t where = at + i;
    const bool held = t.held[where / 8] != 0;
    t.held[where / 8] = 1;
    std::uint64_t a = 0, b = 0;
    std::memcpy(&a, t.copy.data() + where, 8);
    std::memcpy(&b, now.data() + i, 8);
    if (a == b) continue;
    std::memcpy(t.copy.data() + where, &b, 8);
    if (!held) continue;
    std::uint8_t& busy = t.busy[where / 8];
    State::Page& page = t.pages[where / kPage];
    if (busy == 0) {
      t.changed_words.fetch_add(1, std::memory_order_relaxed);
      if (page.words < 0xFFFF) ++page.words;
    }
    if (busy < 255) ++busy;
    if (busy == kBusy) t.busy_words.fetch_add(1, std::memory_order_relaxed);
    const std::uint64_t x = a ^ b;
    for (std::uint32_t k = 0; k < 8; ++k) {
      const auto bits = static_cast<std::uint8_t>(x >> (k * 8));
      if (bits == 0) continue;
      if ((bits & (bits - 1)) != 0) {
        t.other_changes.fetch_add(1, std::memory_order_relaxed);
        continue;
      }
      t.single_flips.fetch_add(1, std::memory_order_relaxed);
      if (busy >= kBusy) continue;
      std::uint32_t bit = 0;
      while ((bits >> bit) != 1) ++bit;
      const SweepFlip flip{static_cast<std::uint32_t>(where + k), bit, ((b >> (k * 8)) & bits) != 0, second};
      // Kept for a mark that comes after it, and under the latest mark it was seen around.
      t.recent[t.recent_count % kRecent] = State::Seen{flip, now_us};
      ++t.recent_count;
      for (std::size_t m = marks_taken_; m-- > 0;) {
        const std::uint64_t at = mark_us_[m].load(std::memory_order_relaxed);
        if (at > now_us + kMarkBeforeUs) continue;  // a later mark: not around this flip
        if (now_us <= at + kMarkAfterUs) {
          const std::uint32_t n = t.marked_count[m].load(std::memory_order_relaxed);
          if (n < kMarkEvents) {
            t.marked[m][n] = flip;
            t.marked_count[m].store(n + 1, std::memory_order_release);
          }
        }
        break;
      }
      if (page.kept >= kPageEvents) continue;
      page.flips[page.kept] = flip;
      ++page.kept;  // after the flip is whole: found() may be reading
    }
  }
}

void Sweep::finish(State& t, std::size_t n, bool& ended) {
  if (t.read_any) t.readings.fetch_add(1, std::memory_order_relaxed);
  t.followed = false;
  t.read_any = false;
  t.tail = false;
  t.cursor = 0;
  at_ = (at_ + 1) % n;
  if (at_ == 0) {
    passes_.fetch_add(1, std::memory_order_release);
    resting_ = true;
    ended = true;
  }
}

void Sweep::mark(std::uint64_t now_us) noexcept {
  const std::size_t n = mark_count_.load(std::memory_order_relaxed);
  if (n >= kMaxMarks) return;
  mark_us_[n].store(now_us, std::memory_order_relaxed);
  mark_count_.store(n + 1, std::memory_order_release);
}

bool Sweep::step(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t now_us) {
  const std::size_t n = count_.load(std::memory_order_acquire);
  if (n == 0) return false;
  if (start_us_ == 0) start_us_ = now_us;
  // A new mark: each target's latest flips seen shortly before it go under it, and the rest ends, so that the next
  // pass starts now.
  for (const std::size_t marks = mark_count_.load(std::memory_order_acquire); marks_taken_ < marks; ++marks_taken_) {
    const std::size_t m = marks_taken_;
    const std::uint64_t at = mark_us_[m].load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < n; ++i) {
      State& t = *targets_[i];
      const std::size_t kept = std::min<std::size_t>(t.recent_count, kRecent);
      for (std::size_t k = 0; k < kept; ++k) {
        const State::Seen& s = t.recent[(t.recent_count - kept + k) % kRecent];
        if (s.at_us + kMarkBeforeUs < at) continue;
        const std::uint32_t have = t.marked_count[m].load(std::memory_order_relaxed);
        if (have >= kMarkEvents) break;
        t.marked[m][have] = s.flip;
        t.marked_count[m].store(have + 1, std::memory_order_release);
      }
    }
    resting_ = false;
  }
  if (resting_) {
    if (now_us < pass_us_ + kEveryUs) return false;
    resting_ = false;
  }
  if (at_ >= n) at_ = 0;
  State& t = *targets_[at_];
  bool ended = false;
  if (!t.followed) {
    // A target of which too much changes all the time holds no flags: it is read no more.
    if (!t.retired.load(std::memory_order_relaxed) &&
        t.readings.load(std::memory_order_relaxed) >= kRetireAfter &&
        t.busy_words.load(std::memory_order_relaxed) > kRetireBusy) {
      t.retired.store(true, std::memory_order_relaxed);
    }
    if (t.retired.load(std::memory_order_relaxed)) {
      if (at_ == 0) pass_us_ = now_us;
      finish(t, n, ended);
      return ended;
    }
    if (at_ == 0) pass_us_ = now_us;
    // The way: the static's slot, the offset, the address kept there; and for a deeper target one step more.
    const auto slot = reader.copy<std::uint64_t>(gt, proto::Group::Session, base_ + t.target.static_rva);
    if (!slot && reader.spent()) return false;
    std::uint64_t at = 0;
    if (slot && *slot != 0) {
      const auto kept = reader.copy<std::uint64_t>(gt, proto::Group::Session, *slot + t.target.offset);
      if (!kept && reader.spent()) return false;
      at = kept ? *kept : 0;
      if (at != 0 && t.target.deeper) {
        const auto next = reader.copy<std::uint64_t>(gt, proto::Group::Session, at + t.target.via);
        if (!next && reader.spent()) return false;
        at = next ? *next : 0;
      }
    }
    if (at == 0 || at % 8 != 0 || at < kLowestAddress || at > kAddressEnd) {
      t.empty.fetch_add(1, std::memory_order_relaxed);
      finish(t, n, ended);
      return ended;
    }
    if (at != t.address) {
      if (t.address != 0) t.moved.fetch_add(1, std::memory_order_relaxed);
      std::fill(t.held.begin(), t.held.end(), std::uint8_t{0});  // another stretch: compared with nothing at first
      t.address = at;
    }
    t.followed = true;
    return false;
  }
  // One piece: kPiece bytes, or in the tail what stands before the next page's start, then a page at a time.
  const std::uint64_t size = std::uint64_t{t.target.pieces} * kPiece;
  const std::uint64_t from = t.address + t.cursor;
  std::uint64_t length = std::min<std::uint64_t>(kPiece, size - t.cursor);
  if (t.tail) {
    const std::uint64_t to_page = kPage - (from % kPage);
    length = std::min<std::uint64_t>(length, to_page);
  }
  const std::span<std::byte> into = std::span<std::byte>(piece_).first(static_cast<std::size_t>(length));
  if (!reader.copy_into(gt, proto::Group::Session, from, into, 8).has_value()) {
    if (reader.spent()) return false;  // the same piece again at the next step
    if (!t.tail && length > kPage - (from % kPage)) {
      t.tail = true;  // the memory may end inside the piece: what stands before its end is read to the page
      return false;
    }
    t.unreadable.fetch_add(1, std::memory_order_relaxed);
    finish(t, n, ended);
    return ended;
  }
  compare(t, t.cursor, into, now_us);
  t.read_any = true;
  t.cursor += length;
  if (t.cursor > t.bytes.load(std::memory_order_relaxed)) t.bytes.store(t.cursor, std::memory_order_relaxed);
  if (t.cursor >= size) finish(t, n, ended);
  return ended;
}

SweepFound Sweep::found() const {
  SweepFound out;
  out.passes = passes_.load(std::memory_order_acquire);
  for (std::size_t m = 0; m < std::min<std::size_t>(mark_count_.load(std::memory_order_acquire), kMaxMarks); ++m) {
    const std::uint64_t at = mark_us_[m].load(std::memory_order_relaxed);
    const bool after = start_us_ != 0 && at > start_us_;
    out.mark_seconds.push_back(after ? static_cast<std::uint32_t>((at - start_us_) / 1'000'000) : 0);
  }
  const std::size_t n = count_.load(std::memory_order_acquire);
  for (std::size_t i = 0; i < n; ++i) {
    const State& t = *targets_[i];
    SweepTarget x;
    x.label = t.target.label;
    x.id = t.target.id;
    x.rva = t.target.static_rva;
    x.offset = t.target.offset;
    x.deeper = t.target.deeper;
    x.via = t.target.deeper ? t.target.via : 0;
    x.bytes = t.bytes.load(std::memory_order_relaxed);
    x.readings = t.readings.load(std::memory_order_relaxed);
    x.empty = t.empty.load(std::memory_order_relaxed);
    x.unreadable = t.unreadable.load(std::memory_order_relaxed);
    x.moved = t.moved.load(std::memory_order_relaxed);
    x.changed_words = t.changed_words.load(std::memory_order_relaxed);
    x.busy_words = t.busy_words.load(std::memory_order_relaxed);
    x.single_flips = t.single_flips.load(std::memory_order_relaxed);
    x.other_changes = t.other_changes.load(std::memory_order_relaxed);
    // The flips in words that stayed quiet, of pages that stayed quiet: a flag flips once or twice in memory that
    // otherwise keeps still; a counter's bit flips all the time, and memory in use changes everywhere. The quietest
    // pages' flips first, then in the order they were seen.
    std::vector<std::pair<std::uint16_t, SweepFlip>> flips;
    for (const State::Page& page : t.pages) {
      if (page.words == 0) continue;
      ++x.changed_pages;
      if (page.words > kQuietPage) continue;
      ++x.quiet_pages;
      for (std::size_t e = 0; e < page.kept && e < kPageEvents; ++e) {
        const SweepFlip& f = page.flips[e];
        if (f.offset / 8 < t.busy.size() && t.busy[f.offset / 8] < kBusy) flips.emplace_back(page.words, f);
      }
    }
    std::stable_sort(flips.begin(), flips.end(), [](const auto& a, const auto& b) {
      return a.first != b.first ? a.first < b.first : a.second.second < b.second.second;
    });
    if (flips.size() > kMaxListed) flips.resize(kMaxListed);
    std::stable_sort(flips.begin(), flips.end(),
                     [](const auto& a, const auto& b) { return a.second.second < b.second.second; });
    for (const auto& f : flips) x.flips.push_back(f.second);
    // The flips around each mark, in words that stayed quiet. A target that was given up lists none: what flipped
    // there is noise (the eighth round's report was filled with it).
    x.retired = t.retired.load(std::memory_order_relaxed);
    x.address = t.address;
    if (x.retired) x.flips.clear();
    const std::size_t marks =
        x.retired ? 0 : std::min<std::size_t>(mark_count_.load(std::memory_order_acquire), kMaxMarks);
    for (std::size_t m = 0; m < marks; ++m) {
      const std::uint32_t kept = std::min<std::uint32_t>(t.marked_count[m].load(std::memory_order_acquire),
                                                         static_cast<std::uint32_t>(kMarkEvents));
      std::size_t listed = 0;
      for (std::uint32_t e = 0; e < kept && listed < kMarkListed; ++e) {
        const SweepFlip& f = t.marked[m][e];
        if (f.offset / 8 >= t.busy.size() || t.busy[f.offset / 8] >= kBusy) continue;
        x.mark_flips.push_back(MarkFlip{static_cast<std::uint32_t>(m + 1), f});
        ++listed;
      }
    }
    out.targets.push_back(std::move(x));
  }
  return out;
}

// ---- The tree's reader ----

TreeRead::TreeRead(std::uint64_t base, std::uint64_t size, std::uint32_t static_rva, std::uint32_t offset)
    : base_(base), size_(size), static_rva_(static_rva), offset_(offset) {
  records_.reserve(kMaxRecords);
  queue_.reserve(kMaxRecords * 3 + 1);
  table_.assign(std::size_t{1} << 18, 0);  // 262,144 places for at most 98,305 addresses
}

TreeRead::~TreeRead() = default;

bool TreeRead::address_like(std::uint64_t word) const noexcept {
  if (word < kLowestAddress || word > kAddressEnd || word % 8 != 0) return false;
  return !(word >= base_ && word - base_ < size_);  // the image's own addresses are no records
}

bool TreeRead::seen(std::uint64_t address) noexcept {
  const std::size_t mask = table_.size() - 1;
  std::size_t at = static_cast<std::size_t>((address >> 3) * 0x9E3779B97F4A7C15ull >> 40) & mask;
  for (std::size_t tries = 0; tries < table_.size(); ++tries) {
    if (table_[at] == address) return true;
    if (table_[at] == 0) {
      table_[at] = address;
      return false;
    }
    at = (at + 1) & mask;
  }
  return true;  // full: nothing more is taken
}

bool TreeRead::step(const proto::GameThread& gt, GuardedReader& reader) {
  if (done_.load(std::memory_order_relaxed)) return true;
  if (!rooted_) {
    // The way: the static's slot, the offset, the address kept there.
    const auto slot = reader.copy<std::uint64_t>(gt, proto::Group::Session, base_ + static_rva_);
    if (!slot && reader.spent()) return false;
    std::uint64_t root = 0;
    if (slot && *slot != 0) {
      const auto kept = reader.copy<std::uint64_t>(gt, proto::Group::Session, *slot + offset_);
      if (!kept && reader.spent()) return false;
      root = kept ? *kept : 0;
    }
    rooted_ = true;
    if (!address_like(root)) {
      done_.store(true, std::memory_order_release);
      return true;
    }
    (void)seen(root);
    queue_.push_back(root);
    return false;
  }
  if (next_ >= queue_.size()) {
    done_.store(true, std::memory_order_release);
    return true;
  }
  const std::uint64_t at = queue_[next_];
  std::array<std::byte, kRecord> bytes{};
  if (!reader.copy_into(gt, proto::Group::Session, at, bytes, 8).has_value()) {
    if (reader.spent()) return false;  // the same record again at the next step
    ++next_;
    unreadable_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  ++next_;
  Record r;
  r.address = at;
  std::array<std::uint64_t, kRecord / 8> words{};
  std::memcpy(words.data(), bytes.data(), kRecord);
  std::memcpy(r.fields.data(), bytes.data(), kRecord);
  // Its links' words now; the others' once the walk is done (settle()).
  for (std::size_t w = 0; w < 3; ++w) {
    if (address_like(words[w])) r.addresses = static_cast<std::uint8_t>(r.addresses | (1u << w));
  }
  // A record of the tree's shape: its first three words each an address or nothing, and one of them an address.
  bool shaped = (r.addresses & 7u) != 0;
  for (std::size_t w = 0; w < 3; ++w) shaped = shaped && (words[w] == 0 || address_like(words[w]));
  if (!shaped) {
    off_shape_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  if (records_.size() >= kMaxRecords) {
    capped_.store(true, std::memory_order_relaxed);
    done_.store(true, std::memory_order_release);
    return true;
  }
  records_.push_back(r);
  count_.store(static_cast<std::uint32_t>(records_.size()), std::memory_order_release);
  for (std::size_t w = 0; w < 3; ++w) {
    if (address_like(words[w]) && !seen(words[w])) queue_.push_back(words[w]);
  }
  return false;
}

void TreeRead::settle(const Watch::Live& live) {
  if (settled_ || !done_.load(std::memory_order_acquire)) return;
  settled_ = true;
  // The records' own addresses say where this process keeps such things.
  std::uint64_t lowest = ~std::uint64_t{0}, highest = 0;
  for (const Record& r : records_) {
    lowest = std::min(lowest, r.address);
    highest = std::max(highest, r.address);
  }
  constexpr std::uint64_t kNear = std::uint64_t{64} << 20;
  for (Record& r : records_) {
    for (std::size_t w = 3; w < kRecord / 8; ++w) {
      std::uint64_t v = 0;
      std::memcpy(&v, r.fields.data() + w * 2, 8);
      if (!address_like(v)) continue;
      const bool near = v + kNear >= lowest && v <= highest + kNear;
      if (near || (live && live(v))) r.addresses = static_cast<std::uint8_t>(r.addresses | (1u << w));
    }
  }
}

TreeFound TreeRead::found(std::span<const Flip> flips) const {
  TreeFound out;
  out.rva = static_rva_;
  out.offset = offset_;
  out.done = done_.load(std::memory_order_acquire);
  out.capped = capped_.load(std::memory_order_relaxed);
  out.records = count_.load(std::memory_order_acquire);
  out.off_shape = off_shape_.load(std::memory_order_relaxed);
  out.unreadable = unreadable_.load(std::memory_order_relaxed);
  if (!out.done || !settled_) return out;  // the records are the walker's until then, and uncounted until settled
  const std::size_t n = records_.size();
  // Field by field: what it held over the records.
  std::vector<std::uint32_t> held;
  held.reserve(n);
  for (std::size_t f = 0; f < kFields; ++f) {
    TreeField x;
    x.offset = static_cast<std::uint32_t>(f * 4);
    held.clear();
    for (const Record& r : records_) {
      if ((r.addresses >> (f / 2)) & 1u) {
        ++x.addresses;
        continue;
      }
      const std::uint32_t v = r.fields[f];
      if (v == 0) continue;
      ++x.nonzero;
      if (v % kBlock == 0) ++x.multiples;
      held.push_back(v);
    }
    if (!held.empty()) {
      std::sort(held.begin(), held.end());
      x.lowest = held.front();
      x.highest = held.back();
      const auto different = static_cast<std::size_t>(std::unique(held.begin(), held.end()) - held.begin());
      x.distinct = static_cast<std::uint32_t>(std::min<std::size_t>(different, 0xFFFF));
    }
    out.fields.push_back(x);
  }
  // The order: in a tree ordered by a key, a record's left child keeps a smaller key and its right child a greater
  // one. Both ways are tried (first word left and third right, and the reverse), record by record with each of its
  // two children; a record that keeps nothing in the field (the head) is passed over. A field that varies and holds
  // in nearly every pair (two may fail: the head's own links) is the key, and names the order.
  if (n >= 3) {
    std::vector<std::uint32_t> index(table_.size(), 0);  // by the seen table's place: the record's number + 1
    const std::size_t mask = table_.size() - 1;
    const auto record_at = [&](std::uint64_t address) -> std::size_t {
      std::size_t at = static_cast<std::size_t>((address >> 3) * 0x9E3779B97F4A7C15ull >> 40) & mask;
      for (std::size_t tries = 0; tries < table_.size(); ++tries) {
        if (table_[at] == address) return index[at] != 0 ? index[at] - 1 : n;
        if (table_[at] == 0) break;
        at = (at + 1) & mask;
      }
      return n;
    };
    for (std::size_t i = 0; i < n; ++i) {
      std::size_t at = static_cast<std::size_t>((records_[i].address >> 3) * 0x9E3779B97F4A7C15ull >> 40) & mask;
      for (std::size_t tries = 0; tries < table_.size(); ++tries) {
        if (table_[at] == records_[i].address) {
          index[at] = static_cast<std::uint32_t>(i + 1);
          break;
        }
        if (table_[at] == 0) break;
        at = (at + 1) & mask;
      }
    }
    // Each record's two children, by number (n: none).
    std::vector<std::array<std::uint32_t, 2>> children(n);
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t side = 0; side < 2; ++side) {
        std::uint64_t link = 0;
        std::memcpy(&link, records_[i].fields.data() + (side == 0 ? 0 : 4), 8);
        const std::size_t child = record_at(link);
        children[i][side] = static_cast<std::uint32_t>(child != i ? child : n);
      }
    }
    for (std::uint32_t order = 1; order <= 2 && out.order == 0; ++order) {
      for (std::size_t f = 6; f < kFields; ++f) {  // past the three links
        TreeField& x = out.fields[f];
        if (x.distinct < 3 || x.addresses != 0) continue;
        std::uint64_t held = 0, failed = 0;
        for (std::size_t i = 0; i < n; ++i) {
          const std::uint32_t mine = records_[i].fields[f];
          if (mine == 0) continue;
          for (std::size_t side = 0; side < 2; ++side) {
            const std::uint32_t child = children[i][side];
            if (child >= n || records_[child].fields[f] == 0) continue;
            const std::uint32_t theirs = records_[child].fields[f];
            const bool smaller_side = (side == 0) == (order == 1);  // the first word is the left one in order 1
            (smaller_side ? theirs < mine : theirs > mine) ? ++held : ++failed;
          }
        }
        if (held >= 3 && failed <= 2 && failed * 10 <= held) {
          x.rising = true;
          out.order = order;
        }
      }
    }
  }
  // The joins: for each flip, the records that name its block, by the block's start, its index or its address.
  for (const Flip& flip : flips) {
    if (out.joins.size() >= kMaxJoins) break;
    const std::uint32_t start = flip.flip - flip.flip % kBlock;
    const std::uint32_t index = flip.flip / kBlock;
    const std::uint64_t address = flip.address != 0 ? flip.address + start : 0;
    // By the block's start or its address first; by its index only where neither names it (a small index is met
    // by chance in a field that counts something else).
    std::size_t listed = 0;
    for (int by_index = 0; by_index < 2 && listed == 0; ++by_index) {
      for (const Record& r : records_) {
        if (listed >= kJoinsAFlip || out.joins.size() >= kMaxJoins) break;
        std::optional<std::pair<TreeMatch, std::uint32_t>> hit;
        for (std::size_t f = 6; f < kFields && !hit; ++f) {
          if ((r.addresses >> (f / 2)) & 1u) continue;
          const std::uint32_t v = r.fields[f];
          if (v == 0) continue;
          if (by_index == 0 && v == start) hit = std::make_pair(TreeMatch::Start, static_cast<std::uint32_t>(f * 4));
          if (by_index == 1 && v == index) hit = std::make_pair(TreeMatch::Index, static_cast<std::uint32_t>(f * 4));
        }
        for (std::size_t w = 3; w < kRecord / 8 && !hit && address != 0 && by_index == 0; ++w) {
          std::uint64_t v = 0;
          std::memcpy(&v, r.fields.data() + w * 2, 8);
          if (v == address) hit = std::make_pair(TreeMatch::Address, static_cast<std::uint32_t>(w * 8));
        }
        if (!hit) continue;
        TreeJoin j;
        j.mark = flip.mark;
        j.target = flip.target;
        j.flip = flip.flip;
        j.bit = flip.bit;
        j.match = hit->first;
        j.field = hit->second;
        for (std::size_t f = 0; f < kFields; ++f) {
          j.values.push_back(((r.addresses >> (f / 2)) & 1u) != 0 ? 0 : r.fields[f]);
        }
        out.joins.push_back(std::move(j));
        ++listed;
      }
    }
  }
  return out;
}

}  // namespace en::gal::lodestar
