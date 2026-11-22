#pragma once
// The reader's trial of Lodestar's in-game round (step L5; proposal 0005 §6.2, §10; O101): the reader tried on the
// managers the round has just named. On the thread that will read in v1 (the frame service's own thread), about ten
// times a second, each manager's slot and its instance's first word are read through the guarded reader, and the slot
// read again: what was null, what still had its class, what had another, what faulted and what changed between the
// two reads are counted. It reads sixteen bytes a manager a reading, and writes nothing.
//
// It belongs to the part of en-gal that runs in the game (it holds a GuardedReader, whose shape depends on the probe
// option); its counts, TrialFound, are lodestar.hpp's, which the report writes.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "en/gal/guarded.hpp"
#include "en/gal/health.hpp"
#include "en/gal/lodestar.hpp"
#include "en/gal/sys.hpp"

namespace en::gal::lodestar {

static_assert(kTrialFaultKinds == kFaultKinds, "lodestar.hpp's count of the reader's fault kinds is out of date");

// A task body for the frame service's own thread. Managers are added by the thread that names them, at any time; the
// reader's thread reads those added so far. Nothing is ever removed.
class Trial final : public TaskBody {
 public:
  static constexpr std::size_t kMaxManagers = 32;

  // `base` is the image's address as loaded; a manager's slot and table of methods are offsets from it.
  Trial(SysLayer& sys, std::uint64_t base, std::uint64_t period_us = 100'000) noexcept;

  // Adds a manager the round named (status derived); false when it is there already, or the trial is full.
  bool add(const ManagerFound& manager);
  // The manager whose instance tells the stage: a reading made while it has one is a reading in the world (the
  // characters' manager exists only with a save loaded). 0, the default: no stage is told.
  void set_stage(std::uint32_t id) noexcept { stage_id_.store(id, std::memory_order_relaxed); }

  void run(const proto::GameThread& gt, const FrameCtx& frame) override;

  // The counts so far, from any thread.
  [[nodiscard]] TrialFound found() const;
  [[nodiscard]] std::uint64_t readings() const noexcept { return readings_.load(std::memory_order_relaxed); }

 private:
  struct Slot {
    std::uint32_t id = 0;
    std::string label;
    std::uint64_t slot = 0;    // absolute
    std::uint64_t vtable = 0;  // absolute
    std::atomic<std::uint64_t> null{0};
    std::atomic<std::uint64_t> null_runs{0};
    std::atomic<std::uint64_t> null_longest{0};
    std::atomic<std::uint64_t> null_world{0};
    std::uint64_t run = 0;  // the reader's thread only: the stretch of empty readings it is in
    std::atomic<std::uint64_t> same{0};
    std::atomic<std::uint64_t> other{0};
    std::atomic<std::uint64_t> fault{0};
    std::atomic<std::uint64_t> torn{0};
  };

  SysLayer& sys_;
  GuardedReader reader_;
  std::uint64_t base_;
  std::uint64_t period_us_;
  std::array<Slot, kMaxManagers> slots_;
  std::atomic<std::size_t> count_{0};  // slots published to the reader's thread
  std::atomic<std::uint64_t> steps_{0};
  std::atomic<std::uint64_t> readings_{0};
  std::atomic<std::uint64_t> world_readings_{0};
  std::atomic<std::uint64_t> stage_changes_{0};
  std::atomic<std::uint32_t> stage_id_{0};
  int last_stage_ = -1;  // the reader's thread only: 0 title, 1 world, -1 not yet read
  std::atomic<std::uint64_t> first_us_{0};
  std::atomic<std::uint64_t> last_us_{0};
  Histogram reading_us_;
  std::uint64_t next_us_ = 0;  // the reader's thread only
  bool bound_ = false;
};

// The clock search (step L6's first invariant of our own; proposal 0005 §6.1): which manager holds the time of day,
// by what its object holds. A manager with no type information cannot be named by its class (on 2.7.2.0 the world's
// clock has none: docs/ops/probes/2026-11-22-l5.md), so it is known by how its fields behave.
// Each candidate is one of the accessor's statics. About once a second, on the reader's thread, its slot is read and
// the first bytes of the object behind it copied, through the guarded reader. In those bytes, every 1-byte field and
// every 4-aligned 32-bit field is watched: one that has only ever held 0 to 59 and has only ever stood still or
// stepped forward by 1 to 5, round 60, behaves as minutes do; one that has only ever held 0 to 23 and stepped by one,
// round 24, as hours do. A hit is a minutes field that has stepped at least kMinTicks times; a field of the same size
// beside it that behaves as hours makes it a clock. Nothing is concluded here: the hits are counts for the review.
// It reads, at most, kSpan bytes of each candidate a second, and writes nothing. Everything is allocated when it is
// made; the reader's thread allocates nothing.
class ClockSearch {
 public:
  static constexpr std::uint32_t kSpan = 0x100;
  static constexpr std::size_t kMaxCandidates = 512;  // the accessor's statics and those its other shapes add
  static constexpr std::size_t kMaxHits = 32;
  static constexpr std::uint32_t kMinTicks = 10;
  static constexpr std::uint64_t kMinIntervalUs = 1'000'000;

  // `statics` are offsets of the candidates' slots; `base` and `size` the image's address as loaded and its size.
  ClockSearch(std::vector<std::uint32_t> statics, std::uint64_t base, std::uint64_t size);

  // Reads up to `copies` candidates at `now_us`; answers whether a pass over all of them ended on this call. A pass
  // is a sweep in which at least one candidate was read.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t copies, std::uint64_t now_us);

  // The counts and the hits as the last pass left them, from any thread.
  [[nodiscard]] ClockFound found() const;

 private:
  static constexpr std::size_t kFields = kSpan + kSpan / 4;  // every byte, then every 4-aligned 32-bit field
  struct Field {
    std::uint8_t minute_dead = 0;
    std::uint8_t hour_dead = 0;
    std::uint16_t ticks = 0;
    std::uint16_t wraps = 0;
    std::uint16_t hour_steps = 0;
  };
  struct Candidate {
    std::uint32_t rva = 0;
    std::uint32_t span = kSpan;      // shortened when the object is smaller than the span
    std::uint64_t instance = 0;      // the object the last copy was of; 0 before the first
    std::uint64_t last_us = 0;
    bool dead = false;               // its object cannot be copied at any span
    bool seen = false;               // `last` holds a copy of the same object
    std::array<std::byte, kSpan> last{};
    std::array<Field, kFields> fields{};
  };
  void compare(Candidate& c, std::span<const std::byte> now) noexcept;
  void publish() noexcept;

  std::uint64_t base_;
  std::uint64_t size_;
  std::vector<Candidate> candidates_;
  std::size_t next_ = 0;
  bool read_in_sweep_ = false;  // a sweep that read no candidate (each was read under a second ago) is no pass
  std::array<ClockHit, kMaxHits> hits_{};
  std::atomic<std::size_t> hit_count_{0};
  std::atomic<std::uint64_t> passes_{0};
  std::atomic<std::uint64_t> unreadable_{0};
};

// The member search (in game, read-only): a lead's manager that no slot of the image's data points at may be held
// by another object, as a member of it or behind a pointer in it. For each static given, its slot is read and the
// first bytes of the object behind it copied; every 8-aligned word of the copy after the first is looked at: one that
// is a table of methods of a lead's class says an object of that class lies there itself (`embedded`); one that could
// be a heap address is followed by one 8-byte read, and a lead's table of methods there says the member points at
// the lead's object. The hits are places for the review (a static and an offset into its object), never a name.
// A pass goes through every static once; the next starts kPassEveryUs later. It copies kSpan bytes of an object at
// most (less where the object's memory ends sooner) and reads 8 bytes behind each pointer. Everything is allocated
// when it is made; the reader's thread allocates nothing.
class MemberSearch {
 public:
  static constexpr std::uint32_t kSpan = 0x4000;  // guarded.hpp's longest copy
  static constexpr std::size_t kMaxStatics = 1024;
  static constexpr std::size_t kMaxHits = 32;
  static constexpr std::uint64_t kPassEveryUs = 30'000'000;

  // `statics`: offsets of the slots; `tables`: what to look for (Namer::lead_tables(), or a part of it); `base` and
  // `size`: the image's address as loaded and its size.
  MemberSearch(std::vector<std::uint32_t> statics, std::vector<Namer::LeadTable> tables, std::uint64_t base,
               std::uint64_t size);

  // Up to `reads` reads at `now_us` (an object's copy is one, a pointer followed is one); answers whether a pass ended
  // on this call.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t reads, std::uint64_t now_us);

  // The counts and the hits so far, from any thread.
  [[nodiscard]] MembersFound found() const;

 private:
  struct Table {
    std::uint64_t address = 0;  // the table of methods, absolute
    std::uint32_t lead = 0;     // which of leads_
  };
  struct Lead {
    std::uint32_t id = 0;
    std::string label;
  };
  struct Hit {
    std::uint32_t lead = 0;
    std::uint32_t rva = 0;
    std::uint32_t offset = 0;
    std::uint32_t vtable = 0;
    bool embedded = false;
  };
  [[nodiscard]] const Table* table_at(std::uint64_t word) const noexcept;
  void hit(const Table& t, std::uint32_t rva, std::uint32_t offset, bool embedded) noexcept;

  std::uint64_t base_;
  std::uint64_t size_;
  std::vector<std::uint32_t> statics_;
  std::vector<Table> tables_;  // by address
  std::vector<Lead> leads_;
  std::array<std::byte, kSpan> copy_{};
  std::uint32_t copied_ = 0;     // bytes of copy_ that hold the current static's object; 0: not copied yet
  std::uint64_t instance_ = 0;
  std::size_t next_ = 0;         // the current static
  std::uint32_t word_ = 8;       // the next offset of its copy to look at
  std::uint64_t pass_end_us_ = 0;
  bool resting_ = false;
  std::array<Hit, kMaxHits> hits_{};
  std::atomic<std::size_t> hit_count_{0};
  std::atomic<std::uint64_t> objects_{0};
  std::atomic<std::uint64_t> unreadable_{0};
  std::atomic<std::uint64_t> pointers_{0};
  std::atomic<std::uint64_t> passes_{0};
};

// The watch (step L6's first; in game, read-only): what do the objects the round has found hold, by how their first
// bytes behave? A way leads to an object: a static's slot, an offset added to the address it holds, and, for a
// pointer kept there, one more read. About once a second, on the reader's thread, each way is followed and the first
// kSpan bytes of its object copied (fewer where its memory ends sooner). From one copy to the next, for every 8-byte
// word: whether each half changed and whether it rose; and now and then what the word looks like (nothing, a small
// number, a table of methods, an address in the image, an address of live memory, something else). For a way marked
// `clock`, every run of 6 bits is also watched for a clock's minutes (never 60 or more; steps forward by 1 to 5,
// round 60) and every run of 5 bits for its hours, and every 32-bit field read as a fraction for a steady rise.
// A way may ask for a longer span (to kMaxSpan) and a slower pace, and many ways may be `wide`: every static's object
// watched only for a clock, of which found() lists those where something was found.
// What it keeps is counts; found() gives them from any thread. Ways are added by the naming thread at any time and
// never removed. Everything is allocated when a way is added; the reader's thread allocates nothing.
class Watch {
 public:
  static constexpr std::size_t kMaxObjects = 320;
  static constexpr std::uint32_t kSpan = 0x200;       // a way's span unless it asks for another
  static constexpr std::uint32_t kMaxSpan = 0x4000;   // the guarded reader's longest copy
  static constexpr std::uint64_t kEveryUs = 1'000'000;
  static constexpr std::size_t kMaxWideListed = 40;
  static constexpr std::uint64_t kKindEveryUs = 20'000'000;
  static constexpr std::size_t kMaxBitHits = 16;
  static constexpr std::size_t kMaxFloatHits = 8;
  static constexpr std::uint32_t kMinTicks = 5;

  struct Way {
    std::string label;
    std::uint32_t id = 0;
    std::uint32_t static_rva = 0;
    std::uint32_t offset = 0;
    bool deref = false;
    std::uint32_t expect = 0;  // the table of methods its first word should be, as an offset; 0: none expected
    bool clock = false;
    std::uint32_t span = kSpan;            // bytes watched: 16 to kMaxSpan, a multiple of 8
    std::uint64_t every_us = kEveryUs;     // how often it is followed
    bool wide = false;                     // one of many watched only for a clock: found() lists it only with a find
  };
  // Whether memory that may be read stands at an address, asked without reading it (VirtualQuery in the round).
  using Live = std::function<bool(std::uint64_t address)>;

  // `primary`: every class's primary table of methods, as offsets, ascending.
  Watch(std::uint64_t base, std::uint64_t size, std::vector<std::uint32_t> primary, Live live);
  ~Watch();
  Watch(const Watch&) = delete;
  Watch& operator=(const Watch&) = delete;

  // Adds a way; false when the watch is full or holds the same way already. From one thread at a time.
  bool add(const Way& way);
  [[nodiscard]] std::size_t ways() const noexcept { return count_.load(std::memory_order_acquire); }

  // Follows up to `objects` ways that are due at `now_us`, the plain ones before the wide ones; answers whether a pass
  // ended on this call (every plain way that was due has been followed).
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint32_t objects, std::uint64_t now_us);

  [[nodiscard]] WatchFound found() const;

 private:
  struct Object;
  [[nodiscard]] WordKind kind_of(std::uint64_t word) const;
  void compare(Object& o, std::span<const std::byte> now, std::uint64_t now_us) noexcept;

  std::uint64_t base_;
  std::uint64_t size_;
  std::vector<std::uint32_t> primary_;
  Live live_;
  std::array<std::unique_ptr<Object>, kMaxObjects> objects_;
  std::atomic<std::size_t> count_{0};
  std::size_t next_ = 0;       // the plain ways' turn
  std::size_t next_wide_ = 0;  // the wide ways' turn
  bool read_in_sweep_ = false;
  std::atomic<std::uint64_t> passes_{0};
};

}  // namespace en::gal::lodestar
