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
  static constexpr std::size_t kMaxWideListed = 24;
  static constexpr std::uint64_t kKindEveryUs = 20'000'000;
  static constexpr std::size_t kMaxBitHits = 8;
  static constexpr std::size_t kMaxRiseHits = 8;
  static constexpr std::size_t kMaxHourHits = 6;
  static constexpr std::size_t kJumpsKept = 3;         // a field's or a run's first jumps, kept with their times
  static constexpr std::uint32_t kMinTicks = 5;
  static constexpr std::uint32_t kMinSteady = 4;       // rises at a pace before a rise far above it is a jump
  static constexpr double kPaceFactor = 4.0;           // "far above": so many times the pace
  static constexpr std::uint32_t kQuietAfter = 4;      // comparisons with no change before a wide way is slowed
  static constexpr std::uint64_t kQuietSlower = 8;     // and by how much

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
    bool deeper = false;                   // one step more: the object whose address is kept `via` bytes into the
    std::uint32_t via = 0;                 // object the way led to (which must then be a `deref` way)
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
  std::uint64_t start_us_ = 0;  // the first step's time: a jump's second counts from it
  std::atomic<std::uint64_t> passes_{0};
};

// The sweep (lodestar.hpp's SweepFound): each target's memory copied piece by piece, a piece a call, and compared
// with the copy before. A pass goes through every target once; then the sweep rests until kEveryUs after the pass
// began. Memory in use all the time (a neighbour of what a way leads to) changes everywhere; flags change a bit here
// and there. So each page of a stretch keeps its own first flips, and only the quiet pages' flips are listed. A piece
// that cannot be read ends the target's stretch for that pass: what stands before the end of the memory there is
// read in smaller pieces, to the page.
class Sweep {
 public:
  static constexpr std::uint32_t kPiece = 0x4000;  // the guarded reader's longest copy
  static constexpr std::uint32_t kPage = 0x1000;
  static constexpr std::size_t kMaxTargets = 40;
  static constexpr std::uint32_t kMaxPieces = 128;  // 2 MiB a target (what the owner agreed to, 2026-11-22)
  static constexpr std::size_t kPageEvents = 4;     // flips kept a page of a target's stretch
  static constexpr std::uint16_t kQuietPage = 8;    // changed words to which a page is quiet: flags lie in quiet ones
  static constexpr std::size_t kMaxListed = 8;
  static constexpr std::size_t kMaxMarks = 16;      // the tester's marks it takes
  static constexpr std::size_t kMarkEvents = 32;    // flips kept a mark and target
  static constexpr std::size_t kMarkListed = 16;
  static constexpr std::size_t kRecent = 64;        // a target's latest flips, kept for a mark that comes after them
  static constexpr std::uint64_t kMarkBeforeUs = 15'000'000;  // a mark's flips: seen from so long before it
  static constexpr std::uint64_t kMarkAfterUs = 60'000'000;   // to so long after it
  static constexpr std::uint64_t kRetireBusy = 1024;  // busy words from which a target is read no more
  static constexpr std::uint64_t kRetireAfter = 3;    // and after so many readings
  static constexpr std::uint8_t kBusy = 4;          // changes of a word from which it is busy: no flag
  static constexpr std::uint64_t kEveryUs = 30'000'000;

  struct Target {
    std::string label;
    std::uint32_t id = 0;
    std::uint32_t static_rva = 0;
    std::uint32_t offset = 0;
    bool deeper = false;
    std::uint32_t via = 0;
    std::uint32_t pieces = 16;  // of kPiece bytes: 1 to kMaxPieces
  };

  explicit Sweep(std::uint64_t base);
  ~Sweep();
  Sweep(const Sweep&) = delete;
  Sweep& operator=(const Sweep&) = delete;

  // Adds a target; false when the sweep is full or holds the same way already. From one thread at a time.
  bool add(const Target& target);
  [[nodiscard]] std::size_t targets() const noexcept { return count_.load(std::memory_order_acquire); }

  // Follows one target's way, or reads one piece of it; answers whether a pass ended on this call. A call while the
  // sweep rests reads nothing.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t now_us);
  // The tester marked a task as done at `now_us`: flips seen around that moment are kept under the mark, and the
  // next pass starts at once. From any one thread; the stepping thread takes it up at its next step.
  void mark(std::uint64_t now_us) noexcept;
  [[nodiscard]] std::size_t marks() const noexcept { return mark_count_.load(std::memory_order_acquire); }

  // Whether a call at `now_us` would read: it has a target and is not resting. For the stepping thread only.
  [[nodiscard]] bool due(std::uint64_t now_us) const noexcept {
    return count_.load(std::memory_order_acquire) != 0 &&
           (!(resting_ && now_us < pass_us_ + kEveryUs) || marks_taken_ < mark_count_.load(std::memory_order_acquire));
  }

  [[nodiscard]] SweepFound found() const;

 private:
  struct State;
  void compare(State& t, std::uint64_t at, std::span<const std::byte> now, std::uint64_t now_us) noexcept;
  void finish(State& t, std::size_t n, bool& ended);

  std::uint64_t base_;
  std::array<std::unique_ptr<State>, kMaxTargets> targets_;
  std::atomic<std::size_t> count_{0};
  std::size_t at_ = 0;            // the target whose turn it is
  std::uint64_t start_us_ = 0;    // the first step's time
  std::uint64_t pass_us_ = 0;     // when the pass under way began
  bool resting_ = false;
  std::array<std::atomic<std::uint64_t>, kMaxMarks> mark_us_{};
  std::atomic<std::size_t> mark_count_{0};
  std::size_t marks_taken_ = 0;   // the marks the stepping thread has taken up
  std::vector<std::byte> piece_;  // one piece, as just read
  std::atomic<std::uint64_t> passes_{0};
};

// The tree's reader (lodestar.hpp's TreeFound): from the root, a record a call, breadth first by each record's first
// three words, each record read once. Everything it needs is allocated when it is made; a step allocates nothing.
// The walk is the stepping thread's; found() is for another thread and reads the records only once the walk is done.
class TreeRead {
 public:
  static constexpr std::size_t kMaxRecords = 32'768;
  static constexpr std::uint32_t kRecord = 64;
  static constexpr std::size_t kFields = kRecord / 4;
  static constexpr std::uint32_t kBlock = 125;       // a block's bytes, by our notes' lead
  static constexpr std::size_t kJoinsATarget = 24;  // a flip is joined to one record; a stretch to so many
  static constexpr std::size_t kMaxJoins = 48;

  // A flip to join the records with: seen around `mark`, `flip` bytes into the stretch that stood at `address` and
  // is kept `target` bytes into the store.
  struct Flip {
    std::uint32_t mark = 0;
    std::uint32_t target = 0;
    std::uint32_t flip = 0;
    std::uint32_t bit = 0;
    std::uint64_t address = 0;
  };

  // The image's place, for telling its addresses from numbers; the way to the root.
  TreeRead(std::uint64_t base, std::uint64_t size, std::uint32_t static_rva, std::uint32_t offset);
  ~TreeRead();
  TreeRead(const TreeRead&) = delete;
  TreeRead& operator=(const TreeRead&) = delete;

  // Follows the way to the root, or reads one record; answers whether the walk has ended.
  bool step(const proto::GameThread& gt, GuardedReader& reader);
  [[nodiscard]] bool done() const noexcept { return done_.load(std::memory_order_acquire); }

  // Once the walk is done, and before found() is to count anything: tells which of the records' words past their
  // links are addresses. A word that only looks like one (a key and a mode side by side do) is asked about with
  // `live` (whether memory that may be read stands there; nothing is read); with none, a word counts as an address
  // only within 64 MiB of the records' own. From the thread that calls found(); once.
  void settle(const Watch::Live& live);
  [[nodiscard]] bool settled() const noexcept { return settled_; }

  // `index_field`: where a record keeps its block's index, where a layout says so (its offset in the record): a
  // join by index then looks there alone. Without it, in any field but the key.
  [[nodiscard]] TreeFound found(std::span<const Flip> flips,
                                std::optional<std::uint32_t> index_field = std::nullopt) const;

 private:
  struct Record {
    std::uint64_t address = 0;
    std::array<std::uint32_t, kFields> fields{};
    std::uint8_t addresses = 0;  // by word: it looked like an address
  };
  [[nodiscard]] bool address_like(std::uint64_t word) const noexcept;
  bool seen(std::uint64_t address) noexcept;  // puts it among the seen; answers whether it was there

  std::uint64_t base_;
  std::uint64_t size_;
  std::uint32_t static_rva_;
  std::uint32_t offset_;
  bool rooted_ = false;
  std::vector<Record> records_;          // kMaxRecords reserved; filled in the order read
  std::vector<std::uint64_t> queue_;     // addresses to read; kMaxRecords * 3 + 1 reserved
  std::size_t next_ = 0;                 // the queue's place
  std::vector<std::uint64_t> table_;     // the seen addresses: open addressing, a power of two
  std::atomic<std::uint32_t> count_{0};  // records, as found() may read them
  std::atomic<std::uint32_t> off_shape_{0};
  std::atomic<std::uint32_t> unreadable_{0};
  std::atomic<bool> capped_{false};
  std::atomic<bool> done_{false};
  bool settled_ = false;
};

// The state search (lodestar.hpp's StateFound). Each candidate is a slot of the data; a pass reads the first bytes of
// the object each slot points at, a candidate a call. The passes are asked for one by one, from another thread (the
// round's, at the tester's marks); the stepping thread takes each up when the one before is done. Everything is
// allocated when the search is made.
class StateSearch {
 public:
  static constexpr std::uint32_t kSpan = 0x2000;       // bytes of each object: 8 KiB, shortened where it ends sooner
  static constexpr std::size_t kMaxCandidates = 8192;
  static constexpr std::size_t kKept = 16;             // fields kept a candidate after the second pass
  static constexpr std::size_t kMaxHits = 64;
  static constexpr std::uint32_t kPasses = 4;          // the title, the world, the title, the world (another save's,
                                                       // where the tester loads one: the field's two numbers differ)
  static constexpr std::uint32_t kSmall = 15;          // "a small number": 0 to 15

  // `image`: where given, a slot is read from the image as loaded (its data is always there) and only the object
  // through the guarded reader: one careful read a candidate, not two.
  StateSearch(std::uint64_t base, std::vector<std::uint32_t> slots, const ByteSource* image = nullptr);
  ~StateSearch();
  StateSearch(const StateSearch&) = delete;
  StateSearch& operator=(const StateSearch&) = delete;

  // Asks for the next pass. From one thread; at most kPasses times. A pass still under way (or not yet begun) when
  // the next is asked for is marked overrun: the tester has moved the game on under it.
  void ask() noexcept;
  [[nodiscard]] std::uint32_t asked() const noexcept { return asked_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint32_t passes() const noexcept { return done_.load(std::memory_order_acquire); }
  // Whether a call would read: a pass is asked for and not finished. For the stepping thread only.
  [[nodiscard]] bool due() const noexcept { return done_.load(std::memory_order_relaxed) < asked(); }

  // Reads one candidate of the pass under way; answers whether a pass ended on this call. `now_us`: the stepping
  // thread's clock, for the pass's time.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t now_us = 0);

  // The hits' `sites` are left 0: the caller counts the code's references.
  [[nodiscard]] StateFound found() const;

 private:
  struct Candidate;
  std::uint64_t base_;
  const ByteSource* image_;
  std::vector<Candidate> candidates_;
  std::vector<std::uint64_t> bits_;   // by candidate: kSpan / 4 bits, a field that read -1 at the first pass
  std::vector<std::byte> bytes_;      // one object's bytes, as just read
  std::size_t cursor_ = 0;
  std::atomic<std::uint32_t> asked_{0};
  std::atomic<std::uint32_t> done_{0};
  std::array<std::atomic<std::uint32_t>, kPasses> alive_{};
  std::array<std::atomic<std::uint32_t>, 3> fields_{};  // after the first, second and third pass
  std::array<std::atomic<std::uint32_t>, kPasses> took_ms_{};
  std::array<std::atomic<bool>, kPasses> overrun_{};
  std::uint64_t pass_start_us_ = 0;  // the stepping thread's
  bool pass_started_ = false;
};

// The character search (lodestar.hpp's CharFound): looks asked for as the state search's are, each a walk from the
// characters manager's slot. One careful read a call at most; nothing is allocated on the stepping thread (its
// tables and lists are made when the search is). A look reads: the manager's object and what follows it, up to
// kManagerSpan; the first kHead bytes behind each address found there (at most kMaxTargets different ones); up to
// kPiece bytes of each run two neighbouring addresses bound (at most kMaxLists) and the first bytes behind the
// addresses in it; kPlayerSpan of the main player's object and kSaveSpan of the save data's, with the first bytes
// behind their addresses; and never more than kMaxReads reads.
class CharSearch {
 public:
  static constexpr std::uint32_t kPiece = 0x4000;
  static constexpr std::uint32_t kManagerSpan = 0x40000;  // 256 KiB
  static constexpr std::uint32_t kHead = 0x100;
  static constexpr std::uint32_t kPlayerSpan = 0x4000;
  static constexpr std::uint32_t kSaveSpan = 0x1000;
  static constexpr std::uint32_t kListSpan = 0x10000;     // the most two addresses may lie apart to bound a run
  static constexpr std::size_t kMaxTargets = 8192;
  static constexpr std::size_t kMaxLists = 64;
  static constexpr std::size_t kMaxHolders = 24;
  static constexpr std::uint32_t kMaxReads = 12'000;
  static constexpr std::uint32_t kPasses = 4;
  static constexpr std::size_t kListedPointers = 48, kListedLists = 16, kListedSets = 16, kListedMembers = 160,
                               kListedSaveMembers = 48;

  struct Options {
    std::uint64_t base = 0;
    std::uint64_t image_size = 0;
    std::uint32_t manager_slot = 0;                 // the characters manager's static
    std::uint32_t save_slot = 0;                    // the save data's static; 0 for none
    std::vector<std::uint32_t> character_vtables;   // of the character's class and every class deriving from it
    std::vector<std::uint32_t> player_vtables;      // of the player's class and every class deriving from it
    const ByteSource* image = nullptr;              // the image as loaded: the slots are read from it
  };
  explicit CharSearch(Options options);
  ~CharSearch();
  CharSearch(const CharSearch&) = delete;
  CharSearch& operator=(const CharSearch&) = delete;

  // Asks for the next look; `mark`: how many of the tester's marks had come (the look's place in the round). From
  // one thread; a look still under way when the next is asked for is marked overrun.
  void ask(std::uint32_t mark = 0) noexcept;
  // The save data's static, once the caller knows it: looks begun after this read its object too.
  void set_save_slot(std::uint32_t rva) noexcept;
  [[nodiscard]] std::uint32_t asked() const noexcept { return asked_.load(std::memory_order_acquire); }
  [[nodiscard]] std::uint32_t passes() const noexcept { return done_.load(std::memory_order_acquire); }
  [[nodiscard]] bool due() const noexcept { return done_.load(std::memory_order_relaxed) < asked(); }

  // Works on the look under way until it has made one read or the look ends; answers whether it ended.
  bool step(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t now_us = 0);

  [[nodiscard]] CharFound found() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::atomic<std::uint32_t> asked_{0};
  std::atomic<std::uint32_t> done_{0};
  std::array<std::atomic<bool>, kPasses> overrun_{};
  std::array<std::atomic<std::uint32_t>, kPasses> mark_{};
  std::atomic<std::uint32_t> save_slot_{0};
};

}  // namespace en::gal::lodestar
