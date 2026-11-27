// The sheet scan (lodestar.hpp's SheetFound; the twentieth read-only round).
#include "en/gal/lodestar_trial.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>

namespace en::gal::lodestar {

namespace {

constexpr std::size_t kFields = SheetScan::kSpan / 4;

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
std::uint16_t unit_at(std::span<const std::byte> bytes, std::size_t at) {
  std::uint16_t v = 0;
  std::memcpy(&v, bytes.data() + at, 2);
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

// A 16-bit unit that may be a letter of a text a player types, in whatever script: every unit from U+0020 up but the
// two that are no character (0xFFFE, 0xFFFF: what an unset 16-bit number reads as). A list of scripts would not do
// (Engine's review, and what it led to): two letters of a script it lacks make a number small enough to be told
// where both lie below U+0600, and the last letter of an odd-length text in any script it lacks, with the nothing
// after it, reads as a small number by itself. The price is numbers: one that stands right after a fraction, an id
// or any other field whose upper half is 32 or more goes with it, and so does a number of two million or more whose
// lower half is 32 or more.
bool letter(std::uint16_t u) { return u >= 0x20 && u < 0xFFFE; }

}  // namespace

struct SheetScan::Impl {
  enum class Step : std::uint8_t { Manager, Player, Piece };

  enum : std::uint8_t { kText = 1, kAddress = 2, kSeen = 4 };
  struct Object {
    std::uint64_t address = 0;      // at the last pass that read it
    std::uint32_t span = kSpan;     // what a pass asks for: cut where a piece could not be read
    std::uint32_t bytes = 0;        // the most a pass held: `before` holds each part as it was last read
    std::uint32_t passes = 0, moved = 0;
    std::vector<std::byte> before, now;
    std::vector<std::uint32_t> first;    // by field
    std::vector<std::uint16_t> changes;  // by field: passes in which it differed from the pass before
    std::vector<std::uint8_t> phases;    // by field: bit n, it changed in phase n
    std::vector<std::uint8_t> rose;      // by field: the changes that were a rise by exactly one (up to 255)
    std::vector<std::uint8_t> marks;     // by field: kText, kAddress, kSeen
  };
  struct Told {
    bool there = false;
    std::uint32_t bytes = 0, passes = 0, moved = 0, fields_total = 0, text_fields = 0, address_fields = 0,
                  restless = 0;
    std::array<SheetField, kListed> fields{};
    std::size_t field_count = 0;
    std::array<SheetText, kListedTexts> texts{};
    std::size_t text_count = 0;
    std::uint32_t rises_total = 0;
    std::array<SheetRise, kListedRises> rises{};
    std::size_t rise_count = 0;
  };
  struct Summary {
    std::uint32_t passes = 0, absent = 0, phase = 0;
    std::array<std::uint32_t, kPhases> phase_passes{};
    bool sheet_classed = false, sheet_is_first = false, sheet_is_second = false;
    std::array<Told, kObjects> objects{};
  };

  Options o;
  std::array<Object, kObjects> objects;
  std::array<std::uint32_t, 16> highs{};
  std::size_t high_count = 0;

  Step step = Step::Manager;
  std::uint64_t player_at = 0;
  std::array<std::uint64_t, kObjects> target{};
  std::size_t object_now = 0;
  std::uint32_t piece_from = 0;      // where in the object the next piece begins
  std::uint32_t piece_size = kPiece; // and how much of it is asked for
  std::uint32_t phase_now = 0;
  std::uint64_t next_due_us = 0;
  std::uint32_t passes = 0, absent = 0, read_now = 0;
  bool save_now = false;  // the save data's object was read in this pass
  std::array<std::uint32_t, kPhases> phase_passes{};
  bool sheet_classed = false, sheet_is_first = false, sheet_is_second = false;
  std::atomic<std::uint32_t> phase{0};
  mutable std::mutex mutex;
  Summary shared;

  explicit Impl(Options options) : o(options) {
    for (Object& ob : objects) {
      ob.before.resize(kSpan);
      ob.now.resize(kSpan);
      ob.first.resize(kFields);
      ob.changes.resize(kFields);
      ob.phases.resize(kFields);
      ob.rose.resize(kFields);
      ob.marks.resize(kFields);
    }
  }

  // Where the game's own objects were met so far, as the upper halves of their addresses (the survey's rule): an
  // eight-byte word is taken for an address only if it lies in the image or near one of them.
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
    // Two small numbers side by side read as one word are no address: a count with nothing before it, a level
    // beside an attribute. An address of memory in use has more in its lower half than a small number.
    if ((v & 0xFFFF'FFFFu) < 0x1000) return false;
    const auto high = static_cast<std::uint32_t>(v >> 32);
    for (std::size_t i = 0; i < high_count; ++i) {
      if (high + 4 >= highs[i] && high <= highs[i] + 4) return true;
    }
    return false;
  }

  // One read into `out`: 1 read, 2 the reader's time was up, 0 not readable.
  int read(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t address, std::span<std::byte> out) {
    if (!plausible(address)) return 0;
    if (reader.copy_into(gt, proto::Group::Session, address, out, 8).has_value()) return 1;
    return reader.spent() ? 2 : 0;
  }

  // The object's copy of this pass (`bytes` of it, from its start): compared with what was last read there, field
  // by field. A pass that read less than an earlier one (the object going as the tester quits) leaves the rest as
  // it was last read.
  void take(Object& ob, std::uint64_t address, std::uint32_t bytes) {
    const std::span<const std::byte> now(ob.now.data(), bytes);
    const std::span<const std::byte> before(ob.before.data(), ob.bytes);
    if (ob.passes != 0 && address != ob.address) ++ob.moved;
    // What reads as text: a run of two letters or more, as 16-bit units. Every field such a run touches is marked
    // for good, and nothing it held is ever told (a name lies in the sheet). A number that follows such units is
    // lost with them: better that than a letter told as a number.
    for (std::size_t u = 0; u + 2 <= bytes;) {
      std::size_t end = u;
      while (end + 2 <= bytes && letter(unit_at(now, end))) end += 2;
      if (end - u >= 4) {
        for (std::size_t f = u / 4; f <= (end - 1) / 4 && f < kFields; ++f) ob.marks[f] |= kText;
      }
      u = end == u ? u + 2 : end;
    }
    // What is an address: both halves of the word are marked.
    for (std::size_t w = 0; w + 8 <= bytes; w += 8) {
      if (is_address(word_at(now, w))) {
        ob.marks[w / 4] |= kAddress;
        ob.marks[w / 4 + 1] |= kAddress;
      }
    }
    for (std::size_t f = 0; f * 4 + 4 <= bytes; ++f) {
      const std::uint32_t v = field_at(now, f * 4);
      if ((ob.marks[f] & kSeen) == 0) {
        ob.marks[f] |= kSeen;
        ob.first[f] = v;
        continue;
      }
      const std::uint32_t was = field_at(before, f * 4);  // seen before, so it lies within what `before` holds
      if (was == v) continue;
      if (v == was + 1 && was != 0xFFFF'FFFFu && ob.rose[f] != 0xFF) ++ob.rose[f];
      if (ob.changes[f] != 0xFFFF) ++ob.changes[f];
      ob.phases[f] = static_cast<std::uint8_t>(ob.phases[f] | (1u << std::min(phase_now, 7u)));
    }
    std::memcpy(ob.before.data(), ob.now.data(), bytes);
    ob.address = address;
    ob.bytes = std::max(ob.bytes, bytes);
    ++ob.passes;
    ++read_now;
    if (&ob == &objects[0]) save_now = true;
  }

  void publish() {
    const std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;  // the other thread is reading the last one: the next pass publishes
    Summary& s = shared;
    s.passes = passes;
    s.absent = absent;
    s.phase = phase_now;
    s.phase_passes = phase_passes;
    s.sheet_classed = sheet_classed;
    s.sheet_is_first = sheet_is_first;
    s.sheet_is_second = sheet_is_second;
    for (std::size_t i = 0; i < kObjects; ++i) {
      const Object& ob = objects[i];
      Told& t = s.objects[i];
      t.there = ob.passes != 0;
      t.bytes = ob.bytes;
      t.passes = ob.passes;
      t.moved = ob.moved;
      t.fields_total = 0;
      t.text_fields = 0;
      t.address_fields = 0;
      t.restless = 0;
      t.field_count = 0;
      t.text_count = 0;
      t.rises_total = 0;
      t.rise_count = 0;
      if (ob.passes == 0) continue;
      const std::span<const std::byte> last(ob.before.data(), ob.bytes);  // each part as it was last read
      for (std::size_t f = 0; f < kFields; ++f) {
        if ((ob.marks[f] & kSeen) == 0) continue;
        // What rose by one once and did nothing else: its place and phase, whatever the field holds.
        if (ob.changes[f] == 1 && ob.rose[f] == 1) {
          ++t.rises_total;
          if (t.rise_count < kListedRises) {
            t.rises[t.rise_count++] = SheetRise{static_cast<std::uint32_t>(f * 4), ob.phases[f], ob.first[f] == 0};
          }
        }
        // An address first: its halves read as letters too, and are no text. Neither is ever told.
        if ((ob.marks[f] & kAddress) != 0) {
          ++t.address_fields;
          continue;
        }
        if ((ob.marks[f] & kText) != 0) {
          ++t.text_fields;
          // Where the text lies, run by run, and never what it is: a run of one field alone may be a number lost.
          if (f != 0 && (ob.marks[f - 1] & (kText | kAddress)) == kText && t.text_count != 0 &&
              t.texts[t.text_count - 1].offset + t.texts[t.text_count - 1].fields * 4 == f * 4) {
            ++t.texts[t.text_count - 1].fields;
          } else if (t.text_count < kListedTexts) {
            t.texts[t.text_count++] = SheetText{static_cast<std::uint32_t>(f * 4), 1};
          }
          continue;
        }
        const std::uint32_t now = field_at(last, f * 4);
        const auto told = [](std::uint32_t v) { return v >= 1 && v <= kMost; };
        if (!told(ob.first[f]) && !told(now)) continue;
        if (ob.changes[f] > kMaxChanges) {
          ++t.restless;
          continue;
        }
        ++t.fields_total;
        if (t.field_count >= kListed) continue;
        SheetField& row = t.fields[t.field_count++];
        row.offset = static_cast<std::uint32_t>(f * 4);
        row.first = ob.first[f];
        row.last = now;
        row.changes = ob.changes[f];
        row.phases = ob.phases[f];
      }
    }
  }
};

SheetScan::SheetScan(Options options) : impl_(std::make_unique<Impl>(options)) {}
SheetScan::~SheetScan() = default;

void SheetScan::mark(std::uint64_t) noexcept { impl_->phase.fetch_add(1, std::memory_order_release); }

bool SheetScan::due(std::uint64_t now_us) const noexcept {
  return impl_->step != Impl::Step::Manager || now_us >= impl_->next_due_us;
}

bool SheetScan::step(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t now_us) {
  Impl& m = *impl_;
  using Step = Impl::Step;
  const auto finish = [&] {
    if (m.read_now != 0) {
      ++m.phase_passes[m.phase_now];
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
        m.save_now = false;
        m.object_now = 0;
        m.piece_from = 0;
        m.piece_size = kPiece;
        m.player_at = 0;
        m.target = {};
        // The save data's object: its address is its static's, read from the image as loaded.
        m.target[0] = slot_value(m.o.image, m.o.save_slot);
        if (m.target[0] != 0) m.note(m.target[0]);
        const std::uint64_t manager = slot_value(m.o.image, m.o.manager_slot);
        m.step = Step::Player;
        if (manager == 0) {
          m.step = Step::Piece;
          break;
        }
        m.note(manager);
        std::array<std::byte, 8> raw{};
        const int got = m.read(gt, reader, manager + m.o.main_player_word, raw);
        if (got == 2) {
          m.step = Step::Manager;
          return false;
        }
        m.player_at = got == 1 && plausible(word_at(raw, 0)) ? word_at(raw, 0) : 0;
        if (got == 1) return false;
        break;
      }
      case Step::Player: {
        m.step = Step::Piece;
        if (m.player_at == 0 || m.o.sheet_at == 0) break;
        m.note(m.player_at);
        std::array<std::byte, 8> raw{};
        const int got = m.read(gt, reader, m.player_at + m.o.sheet_at, raw);
        if (got == 2) {
          m.step = Step::Player;
          return false;
        }
        m.target[1] = got == 1 && plausible(word_at(raw, 0)) ? word_at(raw, 0) : 0;
        if (m.target[1] != 0) m.note(m.target[1]);
        if (got == 1) return false;
        break;
      }
      case Step::Piece: {
        if (m.object_now >= kObjects) return finish();
        Impl::Object& ob = m.objects[m.object_now];
        const std::uint64_t at = m.target[m.object_now];
        const std::uint32_t from = m.piece_from;
        const auto next_object = [&] {
          ++m.object_now;
          m.piece_from = 0;
          m.piece_size = kPiece;
          // The sheet by the save data's own word, where the round gives that and no word of the main player's:
          // from the copy this pass made of the save data's object, and only from that.
          if (m.object_now == 1 && m.target[1] == 0 && m.o.sheet_at == 0 && m.o.save_sheet_word != 0 &&
              m.save_now && std::uint64_t{m.o.save_sheet_word} + 8 <= m.objects[0].bytes) {
            const std::span<const std::byte> held(m.objects[0].before.data(), m.objects[0].bytes);
            const std::uint64_t sheet = word_at(held, m.o.save_sheet_word);
            if (plausible(sheet)) {
              m.target[1] = sheet;
              m.note(sheet);
            }
          }
        };
        if (at == 0 || from >= ob.span) {
          if (at != 0 && from != 0) m.take(ob, at, std::min(from, ob.span));
          next_object();
          break;
        }
        const std::uint32_t size = std::min(m.piece_size, ob.span - from);
        const std::span<std::byte> out(ob.now.data() + from, size);
        const int got = m.read(gt, reader, at + from, out);
        if (got == 2) return false;
        if (got == 0) {
          // Not readable. An object never read yet is tried smaller, for memory may end sooner than the span: the
          // same place at half the size, down to 1 KiB, and the span is cut where that fails too. One that was
          // read before is going (the tester quits, or the world loads again): this pass keeps what it has, and
          // the span stays.
          if (ob.passes == 0 && size > 0x400) {
            m.piece_size = size / 2;
            return false;
          }
          if (ob.passes == 0 && from != 0) ob.span = from;
          if (from != 0) m.take(ob, at, from);
          next_object();
          return false;
        }
        // (A piece that had to be asked for smaller leaves the size as it is: what follows is asked for at that
        // size too, and the first failure at 1 KiB ends the object there.)
        if (m.object_now == 1 && from == 0) {
          // Whether the sheet is an object of a class (its first word lies in the image), and whether it is the
          // object one of the save data's two words leads to (the round's leads), by the copies already made.
          const std::uint64_t mine = word_at(out, 0);
          m.sheet_classed = mine >= m.o.base && mine - m.o.base < m.o.image_size;
          const Impl::Object& save = m.objects[0];
          const std::span<const std::byte> held(save.before.data(), save.bytes);
          if (m.o.save_first_word + 8 <= save.bytes) {
            m.sheet_is_first = word_at(held, m.o.save_first_word) == m.target[1];
          }
          if (m.o.save_second_word + 8 <= save.bytes) {
            m.sheet_is_second = word_at(held, m.o.save_second_word) == m.target[1];
          }
        }
        m.piece_from = from + size;
        return false;
      }
    }
  }
}

SheetFound SheetScan::found() const {
  const Impl& m = *impl_;
  SheetFound out;
  const std::lock_guard<std::mutex> lock(m.mutex);
  const Impl::Summary& s = m.shared;
  out.passes = s.passes;
  out.absent = s.absent;
  out.phase = s.phase;
  out.phase_passes.assign(s.phase_passes.begin(), s.phase_passes.end());
  out.sheet_classed = s.sheet_classed;
  out.sheet_is_first = s.sheet_is_first;
  out.sheet_is_second = s.sheet_is_second;
  for (std::size_t i = 0; i < kObjects; ++i) {
    const Impl::Told& t = s.objects[i];
    SheetObject ob;
    ob.which = static_cast<std::uint32_t>(i);
    ob.there = t.there;
    ob.bytes = t.bytes;
    ob.passes = t.passes;
    ob.moved = t.moved;
    ob.fields_total = t.fields_total;
    ob.text_fields = t.text_fields;
    ob.address_fields = t.address_fields;
    ob.restless = t.restless;
    ob.fields.assign(t.fields.begin(), t.fields.begin() + static_cast<std::ptrdiff_t>(t.field_count));
    ob.texts.assign(t.texts.begin(), t.texts.begin() + static_cast<std::ptrdiff_t>(t.text_count));
    ob.rises_total = t.rises_total;
    ob.rises.assign(t.rises.begin(), t.rises.begin() + static_cast<std::ptrdiff_t>(t.rise_count));
    out.objects.push_back(std::move(ob));
  }
  return out;
}

}  // namespace en::gal::lodestar
