// The character survey (lodestar.hpp's SurveyFound; the eighteenth read-only round).
#include "en/gal/lodestar_trial.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>

namespace en::gal::lodestar {

namespace {

constexpr std::uint32_t kSmallest = 0x80;
constexpr std::size_t kFields = CharSurvey::kSpan / 4;
constexpr std::size_t kBits = std::size_t{CharSurvey::kSpan} * 8;
constexpr std::size_t kMaxSeen = CharSurvey::kMaxList + 1;  // the list's, and the main player
constexpr std::uint32_t kMaxStride = 0x40;
constexpr std::size_t kPairSide = 64;  // fields tried on each side of a pair

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

void bump(std::uint16_t& v) { v = v == 0xFFFF ? v : static_cast<std::uint16_t>(v + 1); }

}  // namespace

struct CharSurvey::Impl {
  enum class Step : std::uint8_t { Manager, Bounds, List, Chr, Holder, Vitals };

  struct Tracked {
    std::uint64_t address = 0;
    std::uint32_t span = kSpan;   // what the next copy asks for: shortened where the object's memory ends sooner
    std::uint32_t bytes = 0;      // what the last copy held
    std::uint32_t sightings = 0;
    std::uint32_t last_pass = 0;  // the pass it was last seen at (1 the first)
    bool player = false;
    bool has_hp = false;
    std::int32_t hp = 0;          // at the last sighting
    std::array<std::uint64_t, kFields / 64> changed{};  // by field: it differed between two sightings
  };
  struct DeadBit {
    std::uint16_t bit = 0;  // from the object's start
    bool set = false;
    bool live = true;       // still a candidate: a later death did not flip it otherwise
    std::uint32_t agree_alive = 0, agree_dead = 0, against = 0;
  };
  // What the last finished pass left, for the other thread (under the mutex).
  struct Summary {
    std::uint32_t passes = 0, absent = 0, tracked = 0, most_listed = 0, phase = 0, deaths = 0;
    std::array<std::uint32_t, kPhases> phase_passes{};
    std::uint32_t fields_total = 0;
    std::array<SurveyField, kListedFields> fields{};
    std::size_t field_count = 0;
    std::array<std::uint32_t, kListedHandles> handles{};
    std::size_t handle_count = 0;
    std::array<SurveyPair, kListedPairs> pairs{};
    std::size_t pair_count = 0;
    std::uint32_t dead_total = 0;
    std::array<SurveyDead, kListedBits> dead{};
    std::size_t dead_count = 0;
    std::uint32_t screen_total = 0;
    std::array<SurveyFlip, kListedBits> screen{};
    std::size_t screen_count = 0;
  };

  Options o;
  std::vector<Tracked> tracked;   // kMaxTracked, made once
  std::size_t tracked_count = 0;
  std::vector<std::byte> copies;  // each one's last copy
  std::vector<std::byte> cur;
  std::vector<std::byte> list;
  std::vector<std::uint16_t> flips_still, flips_look, flip_chars;  // by bit
  std::vector<std::uint16_t> look_passes, look_last;               // by bit: looking passes it flipped in, the last
  std::vector<std::uint8_t> flip_last;                             // by bit: the last character it flipped in, plus 1
  std::array<DeadBit, kMaxDeadBits> dead{};
  std::size_t dead_count = 0;
  bool dead_seeded = false;
  std::array<std::uint32_t, 16> highs{};
  std::size_t high_count = 0;

  Step step = Step::Manager;
  std::uint64_t manager_at = 0, player_at = 0, list_begin = 0, holder = 0, vitals = 0;
  std::uint32_t list_count = 0;
  std::array<std::uint64_t, kMaxSeen> targets{};
  std::size_t target_count = 0, target = 0, slot_now = 0;
  std::uint32_t bytes_now = 0;
  std::uint32_t phase_now = 0;
  std::uint64_t next_due_us = 0;
  std::uint32_t passes = 0, absent = 0, deaths = 0, most_listed = 0, read_now = 0;
  std::uint32_t told_most = 0;  // the most characters a pass had when the fields were told
  std::array<std::uint32_t, kPhases> phase_passes{};
  std::atomic<std::uint32_t> phase{0};
  mutable std::mutex mutex;
  Summary shared;

  explicit Impl(Options options)
      : o(std::move(options)),
        tracked(kMaxTracked),
        copies(kMaxTracked * std::size_t{kSpan}),
        cur(kSpan),
        list(kMaxList * std::size_t{kMaxStride}),
        flips_still(kBits),
        flips_look(kBits),
        flip_chars(kBits),
        look_passes(kBits),
        look_last(kBits),
        flip_last(kBits) {
    std::sort(o.character_vtables.begin(), o.character_vtables.end());
    o.stride = std::clamp<std::uint32_t>(o.stride, 8, kMaxStride);
  }

  // Where the game's own objects were met so far, as the upper halves of their addresses: an eight-byte word is
  // taken for an address only if it lies in the image or near one of them. (Two neighbouring numbers read as one
  // word often look like an address by their size alone: a kind's number beside its model's.)
  void note(std::uint64_t address) noexcept {
    const auto high = static_cast<std::uint32_t>(address >> 32);
    for (std::size_t i = 0; i < high_count; ++i) {
      if (highs[i] == high) return;
    }
    if (high_count < highs.size()) highs[high_count++] = high;
  }
  [[nodiscard]] bool is_address(std::uint64_t v) const noexcept {
    if (!plausible(v)) return false;
    if (v >= o.base && v - o.base < o.image_size) return true;
    const auto high = static_cast<std::uint32_t>(v >> 32);
    for (std::size_t i = 0; i < high_count; ++i) {
      if (high + 4 >= highs[i] && high <= highs[i] + 4) return true;
    }
    return false;
  }
  [[nodiscard]] bool is_character(std::uint64_t first) const noexcept {
    if (first < o.base || first - o.base >= o.image_size) return false;
    const auto rva = static_cast<std::uint32_t>(first - o.base);
    return std::binary_search(o.character_vtables.begin(), o.character_vtables.end(), rva);
  }
  [[nodiscard]] std::span<std::byte> copy_of(std::size_t slot) noexcept {
    return std::span<std::byte>(copies).subspan(slot * kSpan, kSpan);
  }
  // The character's place among those followed; kMaxTracked where there is no place left.
  [[nodiscard]] std::size_t slot_of(std::uint64_t address) noexcept {
    for (std::size_t i = 0; i < tracked_count; ++i) {
      if (tracked[i].address == address) return i;
    }
    if (tracked_count >= kMaxTracked) return kMaxTracked;
    tracked[tracked_count] = Tracked{};
    tracked[tracked_count].address = address;
    return tracked_count++;
  }

  // One read into `out`: 1 read, 2 the reader's time was up, 0 not readable.
  int read(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t address, std::span<std::byte> out) {
    if (!plausible(address)) return 0;
    if (reader.copy_into(gt, proto::Group::Session, address, out, 8).has_value()) return 1;
    return reader.spent() ? 2 : 0;
  }

  // The copy in `cur` is the character's at `slot`, with its health where it could be read.
  void take(std::size_t slot, std::uint32_t bytes, const std::int32_t* hp) {
    Tracked& t = tracked[slot];
    const std::span<const std::byte> now = std::span<const std::byte>(cur).first(bytes);
    const std::span<std::byte> before = copy_of(slot);
    const bool again = t.sightings != 0 && t.bytes != 0;
    const std::uint32_t both = again ? std::min(t.bytes, bytes) : 0;
    const bool died = again && t.has_hp && t.hp > 0 && hp != nullptr && *hp <= 0;
    if (died) {
      ++deaths;
      // The bits that flipped as it died and never flipped in any character before: the first death lists them, a
      // later one keeps those that flipped the same way again.
      if (!dead_seeded) {
        dead_seeded = true;
        for (std::size_t i = 0; i < both && dead_count < kMaxDeadBits; ++i) {
          const unsigned x = static_cast<unsigned>(before[i] ^ now[i]);
          for (unsigned b = 0; b < 8 && x != 0 && dead_count < kMaxDeadBits; ++b) {
            if ((x >> b & 1u) == 0 || flip_chars[i * 8 + b] != 0) continue;
            DeadBit d;
            d.bit = static_cast<std::uint16_t>(i * 8 + b);
            d.set = (static_cast<unsigned>(now[i]) >> b & 1u) != 0;
            dead[dead_count++] = d;
          }
        }
      } else {
        for (std::size_t k = 0; k < dead_count; ++k) {
          DeadBit& d = dead[k];
          const std::size_t i = d.bit / 8;
          if (!d.live || i >= both) continue;
          const bool was = (static_cast<unsigned>(before[i]) >> (d.bit % 8) & 1u) != 0;
          const bool is = (static_cast<unsigned>(now[i]) >> (d.bit % 8) & 1u) != 0;
          if (was == d.set || is != d.set) d.live = false;
        }
      }
    }
    if (again) {
      for (std::size_t f = 0; f * 4 + 4 <= both; ++f) {
        if (field_at(before, f * 4) != field_at(now, f * 4)) t.changed[f / 64] |= std::uint64_t{1} << (f % 64);
      }
      for (std::size_t i = 0; i < both; ++i) {
        const unsigned x = static_cast<unsigned>(before[i] ^ now[i]);
        for (unsigned b = 0; b < 8 && x != 0; ++b) {
          if ((x >> b & 1u) == 0) continue;
          const std::size_t bit = i * 8 + b;
          if (phase_now == o.still) bump(flips_still[bit]);
          if (phase_now == o.look) {
            bump(flips_look[bit]);
            const auto pass = static_cast<std::uint16_t>(std::min<std::uint32_t>(passes + 1, 0xFFFF));
            if (look_last[bit] != pass) {
              look_last[bit] = pass;
              bump(look_passes[bit]);
            }
          }
          if (flip_last[bit] != slot + 1) {
            flip_last[bit] = static_cast<std::uint8_t>(slot + 1);
            bump(flip_chars[bit]);
          }
        }
      }
    }
    // Each candidate against this sighting: a living character has the bit as it was, a dead one as it became.
    if (hp != nullptr) {
      for (std::size_t k = 0; k < dead_count; ++k) {
        DeadBit& d = dead[k];
        const std::size_t i = d.bit / 8;
        if (!d.live || i >= bytes) continue;
        const bool is = (static_cast<unsigned>(now[i]) >> (d.bit % 8) & 1u) != 0;
        if (*hp > 0) {
          ++(is != d.set ? d.agree_alive : d.against);
        } else {
          ++(is == d.set ? d.agree_dead : d.against);
        }
      }
    }
    std::memcpy(before.data(), now.data(), bytes);
    t.bytes = bytes;
    ++t.sightings;
    t.last_pass = passes + 1;
    t.player = t.address == player_at;
    t.has_hp = hp != nullptr;
    t.hp = hp != nullptr ? *hp : 0;
    ++read_now;
  }

  void publish() {
    const std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;  // the other thread is reading the last one: the next pass publishes
    Summary& s = shared;
    s.passes = passes;
    s.absent = absent;
    s.tracked = static_cast<std::uint32_t>(tracked_count);
    s.most_listed = most_listed;
    s.phase = phase_now;
    s.deaths = deaths;
    s.phase_passes = phase_passes;
    s.dead_total = 0;
    s.dead_count = 0;
    s.screen_total = 0;
    s.screen_count = 0;
    // The characters of this pass, the main player's place among them, and those seen twice or more.
    std::array<std::size_t, kMaxSeen> seen{};
    std::size_t n = 0, player = kMaxSeen;
    std::uint32_t followed = 0;
    for (std::size_t i = 0; i < tracked_count; ++i) {
      followed += tracked[i].sightings >= 2 ? 1 : 0;
      if (tracked[i].last_pass != passes || n >= kMaxSeen) continue;
      if (tracked[i].player) player = n;
      seen[n++] = i;
    }
    const auto value = [&](std::size_t k, std::size_t f, std::uint32_t& out) {
      if (tracked[seen[k]].bytes < f * 4 + 4) return false;
      out = field_at(copy_of(seen[k]), f * 4);
      return true;
    };
    std::array<std::uint16_t, kPairSide> large{}, little{};
    std::size_t large_count = 0, little_count = 0;
    // The fields, the handles and the pairs are told from the characters of this pass. A pass with few of them tells
    // little or nothing (the tester quit, or went back to the title, and the world is going or gone), and what a
    // fuller pass told is kept: the eighteenth round's first run lost all three to its last, empty passes. A pass
    // tells when it has four characters at least and half as many as the fullest pass so far.
    told_most = std::max(told_most, static_cast<std::uint32_t>(n));
    const bool told = n >= 4 && n * 2 >= told_most && followed != 0;
    if (told) {
      s.fields_total = 0;
      s.field_count = 0;
      s.handle_count = 0;
      s.pair_count = 0;
    }
    for (std::size_t f = 0; f < kFields && told; ++f) {
      std::uint32_t changed_for = 0;
      for (std::size_t i = 0; i < tracked_count; ++i) {
        changed_for += tracked[i].sightings >= 2 && (tracked[i].changed[f / 64] >> (f % 64) & 1) != 0 ? 1 : 0;
      }
      if (changed_for * 5 > followed) continue;  // it keeps still in four characters of five at least
      std::array<std::uint32_t, kMaxSeen> values{};
      SurveyField row;
      row.offset = static_cast<std::uint32_t>(f * 4);
      row.changed_for = changed_for;
      std::uint32_t players = 0, others_like_player = 0, ids = 0, smalls = 0;
      std::uint32_t mine = 0;
      const bool has_mine = player < n && value(player, f, mine);
      for (std::size_t k = 0; k < n; ++k) {
        std::uint32_t v = 0;
        if (!value(k, f, v)) continue;
        values[row.seen++] = v;
        row.zeros += v == 0 ? 1 : 0;
        row.small += v >= 1 && v <= 255 ? 1 : 0;
        row.id_like += v >= 10'000'000 && v < 0x8000'0000u ? 1 : 0;
        row.block_like += v != 0 && (v & 0xFF) == 0 && (v >> 24) >= 10 && (v >> 24) <= 61 ? 1 : 0;
        const std::size_t word = (f & ~std::size_t{1}) * 4;
        if (tracked[seen[k]].bytes >= word + 8 && is_address(word_at(copy_of(seen[k]), word))) ++row.addresses;
        if (k == player) {
          ++players;
        } else {
          others_like_player += has_mine && v == mine ? 1 : 0;
          ids += v >= 100'000 && v < 0x8000'0000u ? 1 : 0;
          smalls += v >= 1 && v <= 99'999 ? 1 : 0;
        }
      }
      if (row.seen < 2) continue;
      std::sort(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(row.seen));
      std::uint32_t run = 0;
      for (std::uint32_t k = 0; k < row.seen; ++k) {
        run = k != 0 && values[k] == values[k - 1] ? run + 1 : 1;
        row.distinct += run == 1 ? 1 : 0;
        row.most = std::max(row.most, run);
      }
      row.player_alone = has_mine && players == 1 && row.seen >= 2 && others_like_player == 0;
      const std::uint32_t others = row.seen - players;
      // A side of a pair: most of the others hold a number of the larger kind, or of the smaller.
      if (others >= 3 && ids * 2 >= others && large_count < kPairSide) {
        large[large_count++] = static_cast<std::uint16_t>(f);
      }
      if (others >= 3 && smalls * 2 >= others && little_count < kPairSide) {
        little[little_count++] = static_cast<std::uint16_t>(f);
      }
      // Listed: not nothing in all, not mostly addresses, and not the same in all, unless all are shaped as a
      // block's id (characters that stand together share their block).
      if (row.zeros == row.seen || row.addresses * 2 >= row.seen) continue;
      if (row.distinct == 1 && row.block_like != row.seen) continue;
      ++s.fields_total;
      if (s.field_count < kListedFields) s.fields[s.field_count++] = row;
    }
    // A handle: eight bytes that never changed, differ in every character seen, are never nothing, and are not all
    // addresses.
    for (std::size_t f = 0; f + 1 < kFields && told; f += 2) {
      bool changed = false;
      for (std::size_t i = 0; i < tracked_count && !changed; ++i) {
        changed = tracked[i].sightings >= 2 && (tracked[i].changed[f / 64] >> (f % 64) & 3) != 0;
      }
      if (changed) continue;
      std::array<std::uint64_t, kMaxSeen> values{};
      std::size_t count = 0;
      bool whole = true, odd = false;
      for (std::size_t k = 0; k < n && whole; ++k) {
        if (tracked[seen[k]].bytes < f * 4 + 8) continue;
        const std::uint64_t v = word_at(copy_of(seen[k]), f * 4);
        whole = v != 0;
        odd = odd || !is_address(v);
        values[count++] = v;
      }
      if (!whole || !odd || count < 4) continue;
      std::sort(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(count));
      if (std::adjacent_find(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(count)) !=
          values.begin() + static_cast<std::ptrdiff_t>(count)) {
        continue;
      }
      if (s.handle_count < kListedHandles) s.handles[s.handle_count++] = static_cast<std::uint32_t>(f * 4);
    }
    // Pairs: the one a number of the larger kind, the other that number divided by 10,000.
    for (std::size_t x = 0; x < large_count && told; ++x) {
      for (std::size_t y = 0; y < little_count; ++y) {
        if (large[x] == little[y]) continue;
        SurveyPair pair;
        pair.a = std::uint32_t{large[x]} * 4;
        pair.b = std::uint32_t{little[y]} * 4;
        for (std::size_t k = 0; k < n; ++k) {
          std::uint32_t a = 0, b = 0;
          if (k == player || !value(k, large[x], a) || !value(k, little[y], b)) continue;
          ++pair.of;
          pair.holds += a >= 10'000 && a / 10'000 == b ? 1 : 0;
        }
        if (pair.holds < 3 || pair.holds * 2 < pair.of) continue;
        // The best first: kept in order of how many it holds for.
        std::size_t at = s.pair_count;
        while (at > 0 && s.pairs[at - 1].holds < pair.holds) --at;
        if (at >= kListedPairs) continue;
        const std::size_t last = std::min<std::size_t>(s.pair_count, kListedPairs - 1);
        for (std::size_t m = last; m > at; --m) s.pairs[m] = s.pairs[m - 1];
        s.pairs[at] = pair;
        s.pair_count = std::min<std::size_t>(s.pair_count + 1, kListedPairs);
      }
    }
    // The death's bits: the fewest sightings against first, then the most for.
    for (std::size_t k = 0; k < dead_count; ++k) {
      const DeadBit& d = dead[k];
      if (!d.live) continue;
      ++s.dead_total;
      SurveyDead row;
      row.offset = d.bit / 8u;
      row.bit = d.bit % 8u;
      row.set = d.set;
      row.agree_alive = d.agree_alive;
      row.agree_dead = d.agree_dead;
      row.against = d.against;
      const auto better = [](const SurveyDead& p, const SurveyDead& q) {
        if (p.against != q.against) return p.against < q.against;
        return p.agree_alive + p.agree_dead > q.agree_alive + q.agree_dead;
      };
      std::size_t at = s.dead_count;
      while (at > 0 && better(row, s.dead[at - 1])) --at;
      if (at >= kListedBits) continue;
      const std::size_t last = std::min<std::size_t>(s.dead_count, kListedBits - 1);
      for (std::size_t m = last; m > at; --m) s.dead[m] = s.dead[m - 1];
      s.dead[at] = row;
      s.dead_count = std::min<std::size_t>(s.dead_count + 1, kListedBits);
    }
    // The screen's bits: flipped four times or more while the tester looked around, in two characters or more and in
    // three passes or more (a counter's bit flips in every character at one pass), and at most a quarter as often
    // while they stood. The most flips first.
    for (std::size_t bit = 0; bit < kBits; ++bit) {
      const std::uint32_t look = flips_look[bit], still = flips_still[bit];
      if (look < 4 || still * 4 > look || flip_chars[bit] < 2 || look_passes[bit] < 3) continue;
      ++s.screen_total;
      SurveyFlip row;
      row.offset = static_cast<std::uint32_t>(bit / 8);
      row.bit = static_cast<std::uint32_t>(bit % 8);
      row.look = look;
      row.passes = look_passes[bit];
      row.still = still;
      row.flipped_in = flip_chars[bit];
      std::size_t at = s.screen_count;
      while (at > 0 && s.screen[at - 1].look < row.look) --at;
      if (at >= kListedBits) continue;
      const std::size_t last = std::min<std::size_t>(s.screen_count, kListedBits - 1);
      for (std::size_t m = last; m > at; --m) s.screen[m] = s.screen[m - 1];
      s.screen[at] = row;
      s.screen_count = std::min<std::size_t>(s.screen_count + 1, kListedBits);
    }
  }
};

CharSurvey::CharSurvey(Options options) : impl_(std::make_unique<Impl>(std::move(options))) {}
CharSurvey::~CharSurvey() = default;

void CharSurvey::mark(std::uint64_t) noexcept { impl_->phase.fetch_add(1, std::memory_order_release); }

bool CharSurvey::due(std::uint64_t now_us) const noexcept {
  return impl_->step != Impl::Step::Manager || now_us >= impl_->next_due_us;
}

bool CharSurvey::step(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t now_us) {
  Impl& m = *impl_;
  using Step = Impl::Step;
  const auto finish = [&](bool present) {
    if (present) {
      ++m.phase_passes[m.phase_now];
      m.most_listed = std::max(m.most_listed, m.read_now);
    } else {
      ++m.absent;
    }
    ++m.passes;
    m.publish();
    m.next_due_us = now_us + kEveryUs;
    m.step = Step::Manager;
    return true;
  };
  for (;;) {  // until one read was made or the pass ends
    switch (m.step) {
      case Step::Manager: {
        m.phase_now = std::min(m.phase.load(std::memory_order_acquire), kPhases - 1);
        m.read_now = 0;
        m.target = 0;
        m.target_count = 0;
        m.list_count = 0;
        m.manager_at = slot_value(m.o.image, m.o.manager_slot);
        if (m.manager_at == 0) return finish(false);
        m.note(m.manager_at);
        std::array<std::byte, 8> raw{};
        const int got = m.read(gt, reader, m.manager_at + m.o.main_player_word, raw);
        if (got == 2) return false;
        m.player_at = got == 1 && plausible(word_at(raw, 0)) ? word_at(raw, 0) : 0;
        m.step = Step::Bounds;
        if (got == 1) return false;
        break;
      }
      case Step::Bounds: {
        std::array<std::byte, 16> raw{};
        const int got = m.read(gt, reader, m.manager_at + m.o.list_word, raw);
        if (got == 2) return false;
        const std::uint64_t begin = got == 1 ? word_at(raw, 0) : 0, end = got == 1 ? word_at(raw, 8) : 0;
        if (plausible(begin) && end >= begin && (end - begin) % m.o.stride == 0 && end - begin <= 0x10'0000) {
          m.list_begin = begin;
          m.note(begin);
          m.list_count = static_cast<std::uint32_t>(std::min<std::uint64_t>((end - begin) / m.o.stride, kMaxList));
        }
        if (m.player_at == 0 && m.list_count == 0) return finish(false);
        if (m.player_at != 0) m.targets[m.target_count++] = m.player_at;
        m.step = m.list_count != 0 ? Step::List : Step::Chr;
        if (got == 1) return false;
        break;
      }
      case Step::List: {
        const std::span<std::byte> out = std::span<std::byte>(m.list).first(std::size_t{m.list_count} * m.o.stride);
        const int got = m.read(gt, reader, m.list_begin, out);
        if (got == 2) return false;
        for (std::uint32_t k = 0; got == 1 && k < m.list_count && m.target_count < kMaxSeen; ++k) {
          if (std::size_t{m.o.chr_at} + 8 > m.o.stride) break;
          const std::uint64_t chr = word_at(out, std::size_t{k} * m.o.stride + m.o.chr_at);
          if (!plausible(chr) || chr == m.player_at) continue;
          if (std::find(m.targets.begin(), m.targets.begin() + static_cast<std::ptrdiff_t>(m.target_count), chr) !=
              m.targets.begin() + static_cast<std::ptrdiff_t>(m.target_count)) {
            continue;
          }
          m.targets[m.target_count++] = chr;
        }
        m.step = Step::Chr;
        if (got == 1) return false;
        break;
      }
      case Step::Chr: {
        if (m.target >= m.target_count) return finish(m.read_now != 0);
        m.slot_now = m.slot_of(m.targets[m.target]);
        if (m.slot_now >= kMaxTracked) {  // no place left to follow another
          ++m.target;
          break;
        }
        Impl::Tracked& t = m.tracked[m.slot_now];
        const std::span<std::byte> out = std::span<std::byte>(m.cur).first(t.span);
        const int got = m.read(gt, reader, t.address, out);
        if (got == 2) return false;
        if (got == 0) {
          if (t.span > kSmallest) {
            t.span /= 2;  // memory ends sooner than the span: a shorter read next
          } else {
            ++m.target;
          }
          return false;
        }
        m.bytes_now = t.span;
        if (!m.is_character(word_at(out, 0))) {  // no character's object (any more): not counted
          ++m.target;
          return false;
        }
        m.note(t.address);
        m.holder = m.o.parts_at != 0 && m.o.parts_at + 8 <= m.bytes_now ? word_at(out, m.o.parts_at) : 0;
        if (plausible(m.holder)) {
          m.step = Step::Holder;
        } else {
          m.take(m.slot_now, m.bytes_now, nullptr);
          ++m.target;
        }
        return false;
      }
      case Step::Holder: {
        std::array<std::byte, 8> raw{};
        const int got = m.read(gt, reader, m.holder + m.o.vitals_at, raw);
        if (got == 2) return false;
        m.vitals = got == 1 ? word_at(raw, 0) : 0;
        if (plausible(m.vitals)) {
          m.note(m.holder);
          m.note(m.vitals);
          m.step = Step::Vitals;
        } else {
          m.take(m.slot_now, m.bytes_now, nullptr);
          ++m.target;
          m.step = Step::Chr;
        }
        return false;
      }
      case Step::Vitals: {
        std::array<std::byte, 8> raw{};
        const int got = m.read(gt, reader, m.vitals + (m.o.hp_at & ~7u), raw);
        if (got == 2) return false;
        if (got == 1) {
          const auto hp = static_cast<std::int32_t>(field_at(raw, m.o.hp_at & 7u & ~3u));
          m.take(m.slot_now, m.bytes_now, &hp);
        } else {
          m.take(m.slot_now, m.bytes_now, nullptr);
        }
        ++m.target;
        m.step = Step::Chr;
        return false;
      }
    }
  }
}

SurveyFound CharSurvey::found() const {
  const Impl& m = *impl_;
  SurveyFound out;
  const std::lock_guard<std::mutex> lock(m.mutex);
  const Impl::Summary& s = m.shared;
  out.passes = s.passes;
  out.absent = s.absent;
  out.tracked = s.tracked;
  out.most_listed = s.most_listed;
  out.phase = s.phase;
  out.phase_passes.assign(s.phase_passes.begin(), s.phase_passes.end());
  out.deaths = s.deaths;
  out.fields_total = s.fields_total;
  out.fields.assign(s.fields.begin(), s.fields.begin() + static_cast<std::ptrdiff_t>(s.field_count));
  out.handles.assign(s.handles.begin(), s.handles.begin() + static_cast<std::ptrdiff_t>(s.handle_count));
  out.pairs.assign(s.pairs.begin(), s.pairs.begin() + static_cast<std::ptrdiff_t>(s.pair_count));
  out.dead_total = s.dead_total;
  out.dead_bits.assign(s.dead.begin(), s.dead.begin() + static_cast<std::ptrdiff_t>(s.dead_count));
  out.screen_total = s.screen_total;
  out.screen_bits.assign(s.screen.begin(), s.screen.begin() + static_cast<std::ptrdiff_t>(s.screen_count));
  return out;
}

}  // namespace en::gal::lodestar
