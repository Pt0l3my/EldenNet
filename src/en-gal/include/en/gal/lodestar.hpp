#pragma once
// Lodestar's finder (proposal 0005 §6.2, §11; O100, O102): what `eldennet-engine lodestar find` runs on an executable
// with the game closed, and its report. It runs the recipes that need no running game and says, in numbers, what they
// found: the image's fingerprint and whether gal/builds.toml knows the build; the accessor's shape (its places, the
// sites accepted and refused, and every static they load, each with the places that load it); the classes the leads
// name, by their run-time type information, with their tables of methods; and the task groups' table.
//
// For as long as the shape is new, it also counts what the withdrawn pattern would have found: the derivation cuts
// that pattern from the executable in memory, as R0 did, and the finder compares the two sets of accepted sites. The
// pattern is never written: the report's vocabulary has no key that can hold a byte, a pattern, text, a name or an
// instruction in any form (report schema 2, kind `lodestar`). It holds whole numbers, booleans, fixed words and the
// leads' own labels.
//
// It reads the file only (O73, O75): nothing here runs the game, opens its memory or knows it.
//
// The in-game round (step L5; proposal 0005 §6.1's `instance_class`, O92) adds two things, both read-only:
// - name_instances: which static holds which manager. For each static the accessor's sites load, the instance it holds
//   as the game runs, that instance's first word read once through the caller's guarded copy, and that word's class by
//   the run-time type information of the image as loaded. A lead names the one static whose class has its name, or its
//   name with Imp. The classes' names stay in memory: the report carries the catalogue's labels and numbers.
// - draft_table: what the round found, as the entries of a build's table (layout.hpp, schema 2), each checked against
//   the image before it is written and each a lead until its review.
// - Trial (lodestar_trial.hpp, in the part that runs in the game): the reader tried on the managers just named. On the
//   thread that will read in v1 (the frame service's own
//   thread, O101), about ten times a second, each manager's slot and its instance's first word are read through the
//   guarded reader, and the slot read again: what was null, what still had its class, what had another, what faulted
//   and what changed between the two reads are counted. It reads sixteen bytes a manager and writes nothing.
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "en/gal/builds.hpp"
#include "en/gal/derive.hpp"
#include "en/gal/ids.hpp"
#include "en/gal/layout.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/recipes.hpp"
#include "en/gal/report.hpp"
#include "en/proto/expected.hpp"

namespace en::gal::lodestar {

inline constexpr std::size_t kMaxStaticsReported = 1024;  // statics written; the count says how many there were
inline constexpr std::size_t kMaxSiteWitnesses = 8;       // places kept per static (layout.hpp's kMaxWitnesses)

struct ImageFound {
  std::array<std::uint16_t, 4> file_version{};
  bool has_version = false;
  std::uint32_t time_date_stamp = 0;
  std::uint32_t size_of_image = 0;
  std::uint64_t file_size = 0;
  std::uint64_t sections = 0;
  std::uint64_t functions = 0;
  bool known_build = false;                // gal/builds.toml's gate passes
  std::optional<GateProblem> gate_problem; // else why not
};

// One static the accessor's accepted sites load: a manager's slot, not yet named (its name is the in-game round's).
struct StaticFound {
  std::uint32_t rva = 0;
  std::uint32_t class_object = 0;
  std::uint32_t sites = 0;
  std::vector<std::uint32_t> witnesses;  // the first places that load it, each the load itself
};

enum class Expect : std::uint8_t { Within, Below, Above };

// Why the shape does not stand at sites the pattern accepts: which step differs, how, and at how many sites.
struct Why {
  std::uint8_t step = 0;
  shape::Differs differs = shape::Differs::Other;
  std::uint64_t count = 0;
};

// Another shape of the accessor (a recipe of the same kind under another id: the compiler does not write every
// accessor the same way), counted beside the accessor's own: where it stands, and which statics it reaches that the
// accessor's own sites do not. Counts and places only.
struct VariantFound {
  std::string label;  // the recipe's id
  std::uint64_t hits = 0;
  std::uint64_t accepted = 0;
  std::array<std::uint64_t, 9> refused{};  // by fd4::Refusal
  Expect expect = Expect::Within;          // the hits against the recipe's expect_matches
  std::uint64_t statics_found = 0;         // distinct statics of its accepted sites
  std::uint64_t known = 0;                 // of those, statics the accessor's own sites load too
  std::uint64_t agreeing = 0;              // of the known, those it gives the same class object
  std::uint64_t shared_class_objects = 0;  // class objects its new statics share with another static, of any shape
  std::vector<StaticFound> statics;        // the statics only it reaches, by offset, the first 64
};

struct AccessorFound {
  bool recipe = false;  // recipes.toml holds an accessor
  std::uint64_t hits = 0;
  std::uint64_t sites = 0;     // the hits kept (all of them, up to the finder's cap)
  std::uint64_t accepted = 0;
  std::array<std::uint64_t, 9> refused{};  // by fd4::Refusal
  Expect expect = Expect::Within;          // the hits against the recipe's expect_matches
  std::uint64_t statics_found = 0;         // distinct statics of the accepted sites
  std::uint64_t class_objects = 0;
  // Each manager has one slot and one class object of its own. A class object that goes with two statics, or a
  // static with two class objects, means a site took something else for the slot: on 2.7.2.0 the withdrawn pattern
  // did so at 4 sites, where the test is of another register than the one just loaded
  // (docs/ops/probes/2026-11-22-l3.md).
  std::uint64_t shared_class_objects = 0;  // class objects the accepted sites pass with more than one static
  std::uint64_t shared_statics = 0;        // statics the accepted sites load with more than one class object
  std::vector<StaticFound> statics;        // by offset, the first kMaxStaticsReported
  std::vector<std::uint32_t> getters;      // distinct, by offset, the first 64
  // The withdrawn pattern, cut again in memory for the comparison and never written.
  bool pattern = false;
  std::uint64_t pattern_hits = 0;
  std::uint64_t pattern_accepted = 0;
  std::uint64_t both = 0;          // accepted sites the two share
  std::uint64_t only_shape = 0;    // accepted by the shape alone
  std::uint64_t only_pattern = 0;  // accepted by the pattern alone
  std::uint64_t pattern_statics = 0;              // distinct statics of the pattern's accepted sites
  std::vector<Why> only_pattern_why;              // for the sites the pattern alone accepts, by step and kind
  std::vector<StaticFound> only_pattern_statics;  // the statics only those sites load, each with those sites
  std::vector<VariantFound> variants;             // the other recipes of the accessor's kind, in the file's order
};

// Every static the naming looks at: the accessor's own, then each variant's new ones (a static two variants reach is
// taken once).
[[nodiscard]] std::vector<StaticFound> all_statics(const AccessorFound& accessor);

struct ClassFound {
  std::string label;  // the lead's
  derive::Status status = derive::Status::Missing;
  std::uint32_t type_descriptor = 0;
  std::uint64_t classes = 0;
  std::vector<std::uint32_t> vtables;  // primary, ascending, the first 64
};

struct TableFound {
  derive::Status status = derive::Status::Missing;
  std::uint32_t rva = 0;
  std::uint32_t entries = 0;
  std::uint32_t stride = 0;
  std::optional<std::uint32_t> name_offset;
  bool wide = false;
  std::uint32_t index = 0;  // the lead's group's place
};

// One manager the leads ask for, under the catalogue's label and number.
struct ManagerFound {
  std::string label;
  std::uint32_t id = 0;
  derive::Status status = derive::Status::Missing;  // derived, ambiguous (two statics of its class) or missing
  std::uint32_t rva = 0;     // its static
  std::uint32_t vtable = 0;  // its instance's table of methods
  std::uint32_t sites = 0;   // the accessor's sites that load the static
  bool imp = false;          // named by the lead's name with Imp
  bool by_base = false;      // named by a class that derives from the lead's class, not by the class itself
  bool by_related = false;   // named by a class whose name holds the lead's (the image has none of its name, with or
                             // without Imp, and none deriving from such): weaker than the others, and marked
  bool variant = false;      // its static is one only a variant of the accessor's shape reaches
  bool by_search = false;    // its static is none of the accessor's: found by the search of the data's slots
  std::uint32_t search_found = 0;  // slots that search found holding an object of its class
  // The search's own numbers, kept from its last run: how often it ran, how many slots it had to look at, how many it
  // looked at, how many pointed at memory it could not read, how many distinct objects the slots found hold (one
  // object behind several slots names the manager: the slots are copies of one address), and the slots themselves.
  std::uint32_t search_runs = 0;
  std::uint32_t search_candidates = 0;
  std::uint32_t search_checked = 0;
  std::uint32_t search_unreadable = 0;
  std::uint32_t search_objects = 0;
  std::vector<std::uint32_t> search_slots;  // by offset, the first 16
  // The code's references to each of those slots, in the same order (instructions with a RIP-relative operand that
  // is the slot): where the slots found hold one object, the one the code refers to most is the manager's, and its
  // references are its sites and witnesses.
  std::vector<std::uint32_t> search_refs;
  // Objects of its class that lie in the image's own data, not on the heap (the image's data holds one of its
  // tables of methods at an 8-aligned place): how many, and the first.
  std::uint32_t in_data = 0;
  std::uint32_t in_data_rva = 0;
  std::uint32_t pass = 0;    // the round's pass that named it (1 the first); 0 while unnamed
  std::vector<std::uint32_t> witnesses;  // the first of those sites, each the load itself
};

// What the image holds under one lead's name: counts, so that a reviewer can tell a manager that is not there from
// one that is there under another name, or with no type information at all. The name itself is never written.
struct LeadClasses {
  std::string label;            // the catalogue's label where it holds the lead, else the lead's own
  std::uint32_t id = 0;         // the catalogue's number; 0 where it does not hold the lead
  std::uint32_t named = 0;      // classes with tables of methods whose innermost name is the lead's
  std::uint32_t named_imp = 0;  // and those whose innermost name is the lead's with Imp
  std::uint32_t vtables = 0;    // primary tables of methods of those classes and of every class deriving from them
  std::uint32_t texts = 0;      // places in the image where a type's name of either form stands, in any namespace,
                                // with or without tables of methods
  std::uint32_t related = 0;    // classes with tables of methods whose innermost name holds the lead's name and is
                                // neither of the two above: a manager that goes by a longer name
};

// A lead the catalogue does not hold as a static (a lead on trial: it gets a number once a round has counted it):
// how many of the statics hold an object of its class at this pass, and the first.
struct OtherLead {
  std::string label;
  std::uint32_t held = 0;
  std::uint32_t rva = 0;
  std::uint32_t vtable = 0;
};

// What a static that only a variant of the accessor's shape reaches held at a pass.
enum class Holds : std::uint8_t { Nothing, Unread, Unclassed, Classed };
struct NewStatic {
  std::uint32_t rva = 0;
  Holds holds = Holds::Nothing;
  std::uint32_t vtable = 0;  // its object's table of methods, where the image's type information knows it
  bool lead = false;         // that class answers one of the leads
};

// The in-game round's naming (O92), per distinct static of the accessor's accepted sites.
struct InstancesFound {
  std::uint64_t tried = 0;      // statics looked at
  std::uint64_t null = 0;       // the static holds no instance yet
  std::uint64_t unread = 0;     // the instance's first word could not be read
  std::uint64_t in_image = 0;   // the instance lies inside the image (an object the game keeps in its own data): its
                                // first word is read through the image's view, and it is classed like any other
  std::uint64_t unclassed = 0;  // read, but no table of methods the image's type information knows
  std::uint64_t classed = 0;    // an instance whose class the image gives
  std::uint64_t leads = 0;      // managers the leads ask for
  std::uint64_t unlisted = 0;   // of those, the ones whose label the catalogue does not hold as a static
  std::uint64_t named = 0;
  std::uint64_t ambiguous = 0;
  std::uint32_t passes = 0;            // how many times the round named (merge counts them)
  std::vector<ManagerFound> managers;  // by number: every lead the catalogue holds, found or not
  std::vector<OtherLead> others;       // the leads the catalogue does not hold, in the leads' order
  std::vector<NewStatic> new_statics;  // what the statics only a variant reaches held at this pass
  std::vector<NewStatic> held;         // what every static held at this pass, in all_statics' order: a map from the
                                       // statics to the classes of their objects, as numbers
};

inline constexpr std::size_t kTrialFaultKinds = 6;  // guarded.hpp's kFaultKinds (checked where the trial is built)

// The reader's trial on one manager: what its readings saw.
struct TrialManager {
  std::uint32_t id = 0;
  std::string label;
  std::uint64_t null = 0;   // the slot held no instance
  std::uint64_t null_runs = 0;     // separate stretches of readings that found it empty
  std::uint64_t null_longest = 0;  // the longest of them, in readings
  std::uint64_t null_world = 0;    // of `null`, the readings made while the stage's manager had an instance
  std::uint64_t same = 0;   // the instance's first word was the table of methods it was named by
  std::uint64_t other = 0;  // another word there: an object of another class, or none
  std::uint64_t fault = 0;  // a read was refused or faulted (caught)
  std::uint64_t torn = 0;   // the slot changed between its two reads: the reading judged nothing
};

struct TrialFound {
  std::uint64_t steps = 0;       // times the reader's thread ran the trial
  std::uint64_t readings = 0;    // of those, the ones that read (one each period)
  std::uint64_t world_readings = 0;  // of those, the ones made while the stage's manager had an instance (the world)
  std::uint64_t stage_changes = 0;   // times the stage's manager came or went between two readings
  std::uint64_t elapsed_ms = 0;  // from the first reading to the last
  std::uint64_t reading_p50_us = 0;
  std::uint64_t reading_p99_us = 0;
  std::uint64_t reading_max_us = 0;
  std::uint64_t copies = 0;
  std::array<std::uint64_t, kTrialFaultKinds> faults{};  // the guarded reader's, by kind (guarded.hpp's FaultKind)
  std::vector<TrialManager> managers;                // by number
};

// A field that behaves as a clock's minutes do, in the object behind one of the accessor's statics (the clock
// search, lodestar_trial.hpp): where it is and how it behaved. Numbers only.
struct ClockHit {
  std::uint32_t rva = 0;         // the static
  std::uint32_t offset = 0;      // the field, from the object's start
  std::uint32_t width = 1;       // its size in bytes: 1 or 4
  std::uint32_t ticks = 0;       // readings at which it had stepped forward, by 1 to 5, round 60
  std::uint32_t wraps = 0;       // of those, the ones at which it went round
  bool has_hour = false;         // a field of the same size beside it stayed under 24 and stepped by one
  std::uint32_t hour_offset = 0;
  std::uint32_t hour_steps = 0;
};

struct ClockFound {
  std::uint64_t candidates = 0;
  std::uint64_t passes = 0;
  std::uint64_t unreadable = 0;  // candidates whose object could not be copied
  std::vector<ClockHit> hits;    // the most ticks first, at most 32
};

// A 32-bit field that rose by the clock's milliseconds, in the object behind a slot of the data (probe.hpp's
// RisingSearch: the play time rises in the world and stands still at the title and in menus). Numbers only.
struct RisingHit {
  std::uint32_t rva = 0;     // the slot
  std::uint32_t offset = 0;  // the field, from the object's start
  std::uint32_t rose = 0;    // passes it rose in by the time between them
  std::uint32_t still = 0;   // passes it held its value in
  std::uint32_t other = 0;   // passes it changed otherwise
};

struct RisingFound {
  std::uint64_t candidates = 0;
  std::uint64_t passes = 0;
  std::uint64_t unreadable = 0;
  std::uint64_t risen = 0;      // fields that rose in at least one pass
  std::vector<RisingHit> hits;  // of those, the ones that rose in the most passes, at most 32
};

// What the searches had to go on (in game): the data's 8-aligned slots whose value could be a heap address, sorted
// by what Windows says of the memory there when the round asks (VirtualQuery, which reads nothing), and what became
// of the reads the searches then asked for. Counts only.
struct SearchFound {
  std::uint64_t pointer_like = 0;  // slots whose value lies in user space, outside the image, 8-aligned
  std::uint64_t live = 0;          // of those, values where memory is committed and may be read: the candidates
  std::uint64_t not_committed = 0; // values where there is no memory (free or only reserved): not addresses
  std::uint64_t no_access = 0;     // values at a guard page or a page that may not be read
  std::uint64_t asked = 0;         // reads the searches asked for
  std::uint64_t refused_gone = 0;  // of those, refused because the memory was no longer committed
  std::uint64_t refused_access = 0;// refused because of a guard page or a page that may not be read
  std::uint64_t copy_failed = 0;   // asked, allowed, and Windows could not copy it all the same
  std::uint64_t steps = 0;         // the reader's steps that gave the searches time
  std::uint64_t steps_cut = 0;     // of those, the ones a search's share of the step ran out in
};

// An object of a lead's class found through another object (the member search, in game): a named or classed
// static's object holds, within its first bytes, either the lead's object itself or a pointer to it.
struct MemberHit {
  std::uint32_t id = 0;        // the lead's number; 0 for a lead the catalogue does not hold
  std::string label;
  std::uint32_t rva = 0;       // the static whose object holds it
  std::uint32_t offset = 0;    // where in that object
  bool embedded = false;       // the lead's object lies there itself; else a pointer to it does
  std::uint32_t vtable = 0;    // the table of methods found
};

struct MembersFound {
  std::uint64_t objects = 0;     // statics' objects gone through
  std::uint64_t unreadable = 0;  // of those, the ones that could not be copied
  std::uint64_t pointers = 0;    // pointers followed, one read each
  std::uint64_t passes = 0;
  std::vector<MemberHit> hits;   // at most 32
};

// The object scan (in game; probe.hpp's ObjectScan): the game's readable private memory gone through once for an
// object of an unnamed lead's class (a word that is one of its tables of methods), then, up to three times more, for
// what holds the address of what was found. A node is an object: of a lead's class (level 0), or one that holds the
// address of a node one level down, or encloses it. Places are offsets into objects and into the image; no address of
// the heap and nothing of what the memory held is written.
struct ScanNode {
  std::uint32_t level = 0;
  std::uint32_t id = 0;       // the lead's number; 0 for one the catalogue does not hold
  std::string label;          // the lead's
  std::uint32_t vtable = 0;   // the node's own class: its first word, as an offset in the image; 0 where the holder
                              // lies in no object the image's type information knows (an array, say)
  std::uint32_t parent = 0;   // level 1 and up: which node (by place in the list) it holds the address of
  std::uint32_t offset = 0;   // level 1 and up: where in the node that address lies
  bool embedded = false;      // level 1: no pointer to the parent was found, and the parent lies inside this object
  std::uint32_t holders = 0;  // words of the memory gone through that hold this node's address
  std::uint32_t slots = 0;    // slots of the image's data that hold this node's address
  std::uint32_t slot = 0;     // the first of them
  bool is_static = false;     // that slot is one of the accessor's statics, of any shape
};

struct ScanFound {
  std::uint64_t regions = 0;     // regions of private, writable memory gone through, over all sweeps
  std::uint64_t mib = 0;         // what was read, in MiB
  std::uint64_t unreadable = 0;  // pieces a read was refused for
  std::uint64_t sweeps = 0;      // sweeps finished
  std::uint64_t elapsed_ms = 0;
  bool done = false;
  bool own_known = false;        // our own memory was known and left out
  std::vector<ScanNode> nodes;   // at most 64
};

// What an 8-byte word of a watched object looked like (the watch, lodestar_trial.hpp): nothing, a small number, a
// table of methods the image knows, another address in the image, an address of live memory outside it, or
// something else.
enum class WordKind : std::uint8_t { Zero, Small, Vtable, Image, Heap, Other };

// One word of a watched object that was ever anything but zero, or ever changed: what it looked like, and how its
// two halves behaved from one reading to the next. Counts only; never what it held.
struct WatchWord {
  std::uint32_t offset = 0;
  WordKind kind = WordKind::Zero;  // at the last look
  std::uint32_t flips = 0;         // looks at which its kind was another than at the look before
  std::uint32_t changes_low = 0;   // readings at which its low half differed from the reading before
  std::uint32_t rises_low = 0;     // of those, the ones at which it was greater
  std::uint32_t changes_high = 0;
  std::uint32_t rises_high = 0;
};

// A run of bits of a watched object that behaved as a clock's minutes do: 6 bits that never held 60 or more and
// stepped forward by 1 to 5, round 60; with, where there is one, a run of 5 bits near it that behaved as hours.
struct BitHit {
  std::uint32_t offset = 0;  // the 8-byte word, from the object's start
  std::uint32_t bit = 0;     // the run's lowest bit in that word
  std::uint32_t ticks = 0;   // readings at which it had stepped forward
  std::uint32_t wraps = 0;   // of those, the ones at which it went round
  std::uint32_t jumps = 0;   // readings at which it changed otherwise
  bool has_hour = false;
  std::uint32_t hour_offset = 0;
  std::uint32_t hour_bit = 0;
  std::uint32_t hour_steps = 0;
};

// A 32-bit field of a watched object that kept rising at a steady pace, read as a fraction or as a whole number: a
// clock kept as seconds, hours or a part of the day. A jump is a rise far above its pace, or a fall: what a clock
// does when the player lets time pass.
struct RiseHit {
  std::uint32_t offset = 0;
  bool whole = false;            // read as a whole number; else as a fraction
  std::uint32_t rises = 0;       // readings at which it had risen at about its pace
  std::uint32_t jumps = 0;       // readings at which it had risen far above its pace, or fallen
  std::uint64_t rate_micro = 0;  // how much it rose a second of real time, times 1,000,000
};

// A run of 5 bits of a watched object that behaved as a clock's hours do, whatever lies beside it: never 24 or
// more, stepping by one (round 24), and seldom otherwise.
struct HourHit {
  std::uint32_t offset = 0;  // the 8-byte word, from the object's start
  std::uint32_t bit = 0;     // the run's lowest bit in that word
  std::uint32_t steps = 0;
  std::uint32_t jumps = 0;   // readings at which it changed otherwise
};

// One jump of a field listed as a rise or as hours: when, and from what to what.
enum class JumpAs : std::uint8_t { Fraction = 0, Whole = 1, Hour = 2 };
struct JumpEvent {
  std::uint32_t offset = 0;
  std::uint32_t bit = 0;     // an hour's run; else 0
  JumpAs as = JumpAs::Fraction;
  std::uint32_t second = 0;  // since the watch's first reading
  std::uint64_t from = 0;    // a fraction times 1,000; a whole number or an hour as it is
  std::uint64_t to = 0;
};

// One watched object: the way to it, how often it was there, and how its first bytes behaved.
struct WatchObject {
  std::string label;       // ours: a manager's, a lead's, or `play_time`
  std::uint32_t id = 0;    // the catalogue's number, where it has one
  std::uint32_t rva = 0;     // the static the way starts at
  std::uint32_t offset = 0;  // added to the address the static holds
  bool deref = false;        // the object is the one whose address is kept there; else it lies there itself
  bool deeper = false;       // one step more: the object whose address is kept `via` bytes into that one
  std::uint32_t via = 0;
  bool clock = false;        // its bits and fractions were watched for a clock
  bool wide = false;         // one of the many watched only for a clock: its words are not written
  bool words_left_out = false;  // its words were dropped to keep the report within the writer's limit
  std::uint64_t readings = 0;
  std::uint64_t empty = 0;        // the static, or the address on the way, held nothing
  std::uint64_t unreadable = 0;
  std::uint64_t moved = 0;        // readings at which it was another object than at the reading before
  std::uint64_t vtable_same = 0;  // readings at which its first word was the table of methods expected
  std::uint64_t same_address = 0; // readings at which it was the very object the first way of its label led to
  std::uint32_t span = 0;         // bytes watched
  std::uint32_t vtable = 0;       // its class at the last reading, where the image knows it
  std::uint64_t address = 0;      // where it stood at the last reading: for the round's own use, never written
  std::vector<WatchWord> words;
  std::vector<BitHit> bits;       // the best first, at most 16
  std::vector<RiseHit> rises;     // those that jumped a few times first, then fractions, then the most rises; at most 8
  std::vector<HourHit> hours;     // the most steps first, at most 6
  std::vector<JumpEvent> jumps;   // of the rises and hours listed, at most 4 each, in the order they happened
};

struct WatchFound {
  std::uint64_t passes = 0;
  std::uint64_t wide_ways = 0;        // objects watched only for a clock (every static's)
  std::vector<WatchObject> objects;   // the others, and of the wide ones those with something found, at most 40
};

// The sweep: longer stretches of memory, from the address a way leads to, copied piece by piece and compared with
// the copy before, for bits that flip once: a flag set by something the player does. Only a single bit's flip is
// kept as an event (where, which bit, which way, when); nothing of what the memory holds is written.
struct SweepFlip {
  std::uint32_t offset = 0;  // the byte, from the target's start
  std::uint32_t bit = 0;     // 0 to 7, the lowest first
  bool set = false;          // it went from 0 to 1
  std::uint32_t second = 0;  // since the sweep's first reading
};

// A flip seen around one of the tester's marks (a task confirmed as done): from 15 s before the mark to 60 s after.
struct MarkFlip {
  std::uint32_t mark = 0;  // which mark, from 1
  SweepFlip flip;
};

struct SweepTarget {
  std::string label;
  std::uint32_t id = 0;
  std::uint32_t rva = 0;      // the static the way starts at
  std::uint32_t offset = 0;   // the target is the memory whose address is kept there, from the static's object
  bool deeper = false;        // one step more: whose address is kept `via` bytes into that
  std::uint32_t via = 0;
  std::uint64_t bytes = 0;         // how far it could be read, at most
  std::uint64_t readings = 0;      // sweeps of it that read something
  std::uint64_t empty = 0;         // sweeps at which the way led nowhere
  std::uint64_t unreadable = 0;    // pieces that could not be read
  std::uint64_t moved = 0;         // sweeps at which it was at another address than before
  std::uint64_t changed_words = 0; // 8-byte words that ever differed from the copy before
  std::uint64_t busy_words = 0;    // of those, the ones that did so four times or more
  std::uint64_t changed_pages = 0; // pages of 4 KiB with a changed word
  std::uint64_t quiet_pages = 0;   // of those, the ones with eight changed words at most
  std::uint64_t single_flips = 0;  // bytes that differed by one bit
  std::uint64_t other_changes = 0; // bytes that differed by more
  // Single flips in words that are not busy, of quiet pages (each page's first four), the quietest pages' first; in
  // the order seen, at most 8.
  std::vector<SweepFlip> flips;
  // The flips around the tester's marks, in words that are not busy: at most 16 a mark, in the order seen.
  std::vector<MarkFlip> mark_flips;
  bool retired = false;  // too much of it changed all the time: it was read no more (and lists no flips)
  std::uint64_t address = 0;  // where it stood at the last pass: for the round's own use, never written
};

struct SweepFound {
  std::uint64_t passes = 0;
  std::vector<std::uint32_t> mark_seconds;  // when each mark was taken, since the sweep's first reading
  std::vector<SweepTarget> targets;
};

// The tree beside the flags (our notes' lead: a tree whose records name each group's block). The records reached
// from its root by their first three words (each an address or nothing) are read once, 64 bytes each, and counted
// field by field, as sixteen 32-bit numbers: which fields vary, which hold multiples of a block's 125 bytes. Then
// the joins: a record one of whose numbers is the start or the index of the block a marked flip lies in, or whose
// word is that block's address; such a record is written out as its sixteen numbers, addresses left out.
enum class TreeMatch : std::uint8_t { Start = 0, Index = 1, Address = 2 };
struct TreeField {
  std::uint32_t offset = 0;        // of the 32-bit field, from the record's start
  std::uint32_t nonzero = 0;       // records in which it held something
  std::uint32_t distinct = 0;      // how many different numbers, to 65,535
  std::uint32_t lowest = 0;        // of the numbers held (addresses aside)
  std::uint32_t highest = 0;
  std::uint32_t multiples = 0;     // records in which it held a multiple of 125 (nothing aside)
  std::uint32_t addresses = 0;     // records in which its word looked like an address
  bool rising = false;             // walked left to right, it never fell: a key the tree is ordered by
};
struct TreeJoin {
  std::uint32_t mark = 0;          // the mark the flip was seen around, from 1
  std::uint32_t target = 0;        // the swept stretch, by its offset in the store
  std::uint32_t flip = 0;          // the flipped byte, from the stretch's start
  std::uint32_t bit = 0;
  TreeMatch match = TreeMatch::Start;
  std::uint32_t field = 0;         // the matching field's (or word's) offset in the record
  std::vector<std::uint32_t> values;  // the record's sixteen numbers; a word that looked like an address reads 0
  bool reached = false;            // going down from the root by the key, as a reader does, arrived at this record
  std::uint32_t depth = 0;         // after so many records (0: the way was not tried, or it ended elsewhere)
};
struct TreeFound {
  std::uint32_t rva = 0;           // the static the way to the root starts at
  std::uint32_t offset = 0;        // the root is the record whose address is kept there, from the static's object
  bool done = false;               // the walk ended
  bool capped = false;             // it ended at the most records it takes
  std::uint32_t records = 0;       // records read and of the tree's shape
  std::uint32_t off_shape = 0;     // addresses followed that held something else
  std::uint32_t unreadable = 0;
  std::uint32_t order = 0;         // 0: no field rises in either order; 1: first word left, third right; 2: the reverse
  std::uint32_t key_field = 0;     // the key's offset in the record (the first field the tree is ordered by); 0: none
  std::vector<std::uint32_t> divisor_at;  // offsets in the store's first 256 bytes that hold 1,000 (the round's read)
  std::vector<std::uint32_t> block_at;    // and those that hold 125
  std::vector<TreeField> fields;   // the sixteen
  std::vector<TreeJoin> joins;     // one a flip, at most 24 a stretch and 48 in all
};

// The state search (the twelfth read-only round; WHY§460): a static found by what a field of its object holds in two
// states of the game the tester moves it between. Our notes' lead for the game state's object: its save slot reads
// -1 at the title and the slot's number once a save is loaded. Over the data's slots that point at live memory, the
// first bytes of each object are read in four passes (the title, the world, the title again, the world again); a
// 32-bit field that reads -1, a small number, -1 and a small number again is a hit: the same number where the same
// save was loaded twice, two numbers where two saves were (the thirteenth round: the stronger evidence).
struct StateHit {
  std::uint32_t rva = 0;     // the slot
  std::uint32_t offset = 0;  // the field, from the object's start
  std::uint32_t value = 0;   // what it held in the world at the second look (0 to 15)
  std::uint32_t again = 0;   // and at the fourth: the same where the same save was loaded, another save's number else
  std::uint32_t same_as = 0; // hits with one number here are one field of one object, seen through several slots
  std::uint32_t sites = 0;   // instructions of the code that refer to the slot (the round's count)
};
// One pass of the state search, as it went: a pass must be through before the tester moves the game on, or its later
// candidates were read in the next state.
struct StateLook {
  std::uint32_t took_ms = 0;  // from its first read to its last; 0 while it is not through
  bool through = false;
  bool overrun = false;       // the next pass was asked for before this one was through
};
struct StateFound {
  std::uint32_t candidates = 0;
  std::uint32_t passes = 0;         // passes finished, of four
  std::uint32_t asked = 0;          // passes asked for
  bool title_clean = true;          // no pass at the title was made while a save was loaded (the round's word)
  std::vector<std::uint32_t> alive; // candidates still read as the same object after each pass
  std::uint32_t fields_after_title = 0;  // 32-bit fields that read -1 at the first pass, over all candidates
  std::uint32_t fields_after_world = 0;  // of those, the ones that read a small number at the second
  std::uint32_t fields_after_again = 0;  // of those, the ones that read -1 again at the third
  std::vector<StateHit> hits;       // of those, the ones that read a small number again at the fourth; at most 64
  std::uint32_t slots_hit = 0;      // different slots among all hits
  std::uint32_t slots_differ = 0;   // different slots among the hits whose two numbers differ (two saves were loaded)
  std::uint32_t fields_differ = 0;  // different fields (object and offset) among those
  std::vector<StateLook> looks;     // one a pass asked for
  bool overrun = false;             // any pass was overrun: the hits are weaker for it, and no slot is named
};

// The character search (the thirteenth read-only round; WHY§460's second step: the pointers and lists by search and
// class). From the characters manager's object: the words that hold the address of an object of a character class
// (the main player's among them), the pairs of addresses that bound a run of elements holding characters (the list
// by distance), the addresses of memory that holds the main player's address (the player set), and what the main
// player's own object and the save data's point at. Offsets, sizes, counts and tables of methods only.
struct CharPointer {
  std::uint32_t offset = 0;  // in the manager's object
  bool player = false;       // the object is of the player's class; else of the character's
};
struct CharList {
  std::uint32_t offset = 0;      // of the first of the two addresses, in the manager's object
  std::uint32_t bytes = 0;       // between the two
  bool three = false;            // a third address follows, at or past the second (begin, end, capacity)
  std::uint32_t stride = 0;      // an element's size
  std::uint32_t chr_at = 0;      // where in an element the character's address lies
  std::uint32_t elements = 0;    // read (at most 16 KiB of the run)
  std::uint32_t characters = 0;  // of them, elements whose address is a character's
  bool has_player = false;       // the main player is one of them
  bool has_distance = false;     // a fraction in every element, never negative and never falling along the run
  std::uint32_t distance_at = 0;
  bool has_fraction = false;     // a fraction in every element, never negative, not the same in all (in order or not)
  std::uint32_t fraction_at = 0;
  bool expected = false;         // it lies where the build's layout puts a list (`characters.by_distance`)
};
struct CharSet {
  std::uint32_t offset = 0;  // in the manager's object: an address of memory that holds the main player's address
  std::uint32_t chr_at = 0;  // where behind it (within 256 bytes)
  std::uint32_t vtable = 0;  // that memory's table of methods; 0 for none
};
struct CharMember {
  std::uint32_t offset = 0;    // in the object: a word that holds the address of readable memory
  std::uint32_t vtable = 0;    // what lies there: an object's table of methods; 0 for none
  std::uint32_t pointers = 0;  // addresses among its first 32 words
  std::uint32_t classed = 0;   // of its first 16 words, addresses of objects with a table of methods (counted only
                               // where at least 8 of the 16 are addresses: a holder of parts)
};
struct CharLook {
  std::uint32_t mark = 0;           // how many of the tester's marks had come when it was asked for
  bool through = false;
  bool overrun = false;             // the next look was asked for before this one was through
  bool cut = false;                 // it reached its limit of reads
  std::uint32_t took_ms = 0;
  std::uint32_t reads = 0;
  std::uint32_t manager_bytes = 0;  // read of the manager's object and what follows it; 0: the slot was empty
  std::uint32_t pointer_words = 0;  // words of it that hold the address of readable memory
  std::uint32_t targets_read = 0;   // different addresses followed
  bool targets_cut = false;         // more than the search keeps
  std::uint32_t characters = 0;     // words of the manager that hold a character's address
  std::uint32_t players = 0;        // different objects of the player's class among them
  std::vector<CharPointer> pointers;  // the players' first; at most 48
  std::vector<CharList> lists;        // at most 16
  std::vector<CharSet> sets;          // at most 16
  std::uint32_t player_bytes = 0;     // read of the main player's object; 0: none
  std::uint32_t members_total = 0;
  std::vector<CharMember> members;    // of the main player's object; at most 96
  std::uint32_t save_bytes = 0;       // read of the save data's object; 0: none
  std::uint32_t save_members_total = 0;
  std::vector<CharMember> save_members;  // at most 48
};
struct CharFound {
  std::uint32_t rva = 0;    // the manager's slot
  std::uint32_t asked = 0;
  std::uint32_t passes = 0;
  bool overrun = false;
  std::vector<CharLook> looks;  // one a look that is through
};

// The pair search (the fourteenth read-only round): a field by two numbers it must read. The owner's save file gave
// the slots of the characters (one fact read offline), so the save's slot is sought as the field that reads the first
// loaded character's slot in the first world and the second's in the second, over the data's slots in use in the
// world, with no rule about the title and none about the object behind a slot staying the same.
// The character survey (in game; the eighteenth round): the characters the near list holds, and the main player,
// read for the first 2 KiB of their objects every two seconds, for a character's other fields. Behaviour only: where
// a number lies and how it stands among the characters and over the session; no value of any character is written.
struct SurveyField {             // a 32-bit field that keeps still in nearly every character
  std::uint32_t offset = 0;
  std::uint32_t seen = 0;        // characters seen at the last pass
  std::uint32_t changed_for = 0; // characters, of all followed, in which it ever changed
  std::uint32_t distinct = 0;    // different values among the characters seen
  std::uint32_t most = 0;        // the largest group of them that shares one value
  std::uint32_t zeros = 0;       // of them, zero
  std::uint32_t small = 0;       // of them, 1 to 255
  std::uint32_t addresses = 0;   // of them, half of a word that is an address
  std::uint32_t block_like = 0;  // of them, shaped as a map block's id (a low byte of 0, a top byte of 10 to 61)
  std::uint32_t id_like = 0;     // of them, 10,000,000 or more and under 2^31
  bool player_alone = false;     // the main player's value is no other character's
};
struct SurveyPair {              // two such fields: one is the other divided by 10,000
  std::uint32_t a = 0;           // the larger number's field
  std::uint32_t b = 0;           // the smaller's
  std::uint32_t holds = 0;       // characters for which a / 10,000 == b, with a at least 10,000
  std::uint32_t of = 0;          // characters seen, the main player aside
};
// One check of a place the round names (a candidate from an earlier round's survey), counted over the sightings of
// the pass's characters: how often it holds, for the main player and for the others apart. No value is kept.
struct SurveyCheck {
  enum Kind : std::uint32_t {
    kIs = 0,          // the 32-bit number at `a` is the number `b`
    kEqual = 1,       // the 32-bit numbers at `a` and at `b` are the same
    kWithPlayer = 2,  // the character's number at `a` is the main player's at `b` in the same pass (the others only)
    kBlock = 3,       // the number at `a` is shaped as a block's id
    kBitsEqual = 4,   // the bits `a` and `b` (the byte times 8, plus the bit) read the same
    kChanged = 5,     // the number at `a` is not what it was at the character's sighting before
    kKinds = 6,
  };
  std::uint32_t kind = kIs;
  std::uint32_t a = 0;
  std::uint32_t b = 0;
  std::uint32_t holds = 0;         // sightings of the others for which it holds
  std::uint32_t of = 0;            // of so many
  std::uint32_t player_holds = 0;  // the main player's sightings for which it holds
  std::uint32_t player_of = 0;     // of so many
};
struct SurveyDead {              // a bit that flipped as a character's health went to nothing
  std::uint32_t offset = 0;      // the byte
  std::uint32_t bit = 0;
  bool set = false;              // it went from 0 to 1 (else from 1 to 0)
  std::uint32_t agree_alive = 0; // sightings of a living character with the bit as it was before
  std::uint32_t agree_dead = 0;  // sightings of a dead one with the bit as it became
  std::uint32_t against = 0;     // sightings that say otherwise
};
struct SurveyFlip {              // a bit that flips while the tester looks around and not while they stand
  std::uint32_t offset = 0;
  std::uint32_t bit = 0;
  std::uint32_t look = 0;        // flips in the looking phase, over the characters
  std::uint32_t passes = 0;      // passes of that phase in which it flipped in some character
  std::uint32_t still = 0;       // and in the standing phase
  std::uint32_t flipped_in = 0;  // characters it flipped in (counted when another character than the last flips)
};
struct SurveyFound {
  std::uint32_t passes = 0;
  std::uint32_t absent = 0;         // passes with no main player or no list
  std::uint32_t tracked = 0;        // characters followed over the session (128 at most)
  std::uint32_t most_listed = 0;    // the most characters read in one pass
  std::uint32_t phase = 0;
  std::vector<std::uint32_t> phase_passes;
  std::uint32_t deaths = 0;         // characters whose health went from more than nothing to nothing
  std::uint32_t fields_total = 0;
  std::vector<SurveyField> fields;  // by offset; as the last pass told them that had four characters or more and
                                    // at least half as many as the fullest pass
  std::vector<std::uint32_t> handles;  // 8-byte fields that never changed, differ in every character seen and are
                                       // no address: at most 16
  std::vector<SurveyPair> pairs;    // at most 16, the best first
  std::uint32_t dead_total = 0;
  std::vector<SurveyDead> dead_bits;    // at most 32, the best first
  std::uint32_t screen_total = 0;
  std::vector<SurveyFlip> screen_bits;  // at most 32, the most flips first
  std::vector<SurveyCheck> checks;      // the round's own checks, in the order given (32 at most)
};

// The sheet scan (in game; the twentieth round, the owner's O121 and O122): the save data's object and the main
// player's sheet, read every two seconds, and every 32-bit number in them that may be a count or a level told with
// its place: as first read, as last read, in how many passes it changed and in which of the tester's phases. The
// owner chose the numbers written; they go from the owner to the reviewer and no further. What reads as text is
// never told (a name lies in the sheet): only where such a run lies and how long it is. Nor is an address. Text is
// two 16-bit units or more in a row from U+0020 up, in any script; a text of one single letter cannot be told from
// a small number by its shape, and the round's README says so.
struct SheetField {
  std::uint32_t offset = 0;
  std::uint32_t first = 0;    // as first read
  std::uint32_t last = 0;     // as last read
  std::uint32_t changes = 0;  // passes in which it was another number than in the pass before
  std::uint32_t phases = 0;   // bit n: it changed in the tester's phase n
};
struct SheetText {
  std::uint32_t offset = 0;   // where a run that reads as text begins
  std::uint32_t fields = 0;   // how many 32-bit fields it touches
};
struct SheetObject {
  std::uint32_t which = 0;           // 0: the save data's object; 1: the sheet
  bool there = false;                // it was read at least once
  std::uint32_t bytes = 0;           // the most a pass read of it
  std::uint32_t passes = 0;          // passes that read it
  std::uint32_t moved = 0;           // passes in which it lay at another address than before
  std::uint32_t fields_total = 0;    // numbers that would be listed
  std::uint32_t text_fields = 0;     // fields left out for being text
  std::uint32_t address_fields = 0;  // fields left out for being half of an address
  std::uint32_t restless = 0;        // numbers left out for changing in more than eight passes
  std::vector<SheetField> fields;    // by offset, 2,048 at most
  bool cut = false;                  // the round wrote fewer of them than it had, for the report's size
  std::vector<SheetText> texts;      // by offset, 32 at most
};
struct SheetFound {
  std::uint32_t passes = 0;
  std::uint32_t absent = 0;          // passes that read neither object
  std::uint32_t phase = 0;
  std::vector<std::uint32_t> phase_passes;
  bool sheet_classed = false;        // the sheet's first word lies in the image: an object of a class
  bool sheet_is_first = false;       // the sheet is the object the save data's first given word leads to
  bool sheet_is_second = false;      // or its second
  std::vector<SheetObject> objects;
};

// EldenNet's own reader on the world's leads (in game; ErBackend::world_alone, the seventeenth round): the game's
// state's slot, the main player with its health and position, and the near list, read through the build's layout
// once a second. Counts over the frames, and what the first frame after each of the tester's marks read. The
// health, its maximum and the save's slot are written as numbers, for the tester to hold against the screen (the
// owner agreed, 2026-11-24); the position never is: only whether it moved from one frame to the next.
struct WorldAt {
  std::uint32_t mark = 0;     // the tester's marks so far (0: before the first)
  bool there = false;         // the game's state's object was there
  bool loaded = false;        // its slot read 0 or more: a save is loaded
  std::uint32_t slot = 0;     // the slot, where loaded
  bool player = false;        // an object of the player's class behind the main player's word
  bool has_hp = false;        // the health and its maximum were read
  std::uint32_t hp = 0;
  std::uint32_t max_hp = 0;
  std::uint32_t near_all = 0; // the near list's elements
};
struct WorldRead {
  static constexpr std::size_t kSpans = 12;  // before the first mark, then after each of eleven
  std::uint32_t lacks = 0;          // the parts the layout does not bind (ErBackend::WorldAlone's bits); 0: none
  std::uint64_t calls = 0;          // frames asked for
  std::uint64_t state_frames = 0;   // in which the game's state's object was there
  std::uint64_t slot_frames = 0;    // in which a save's slot read 0 or more
  std::uint64_t player_frames = 0;  // in which the main player was there
  std::uint64_t hp_frames = 0;      // in which its health was read
  std::uint64_t hp_over = 0;        // of those, the health above its maximum, or the maximum 0
  std::uint64_t pos_frames = 0;     // in which its position was read
  std::uint64_t list_frames = 0;    // in which the near list's bounds made sense
  std::uint32_t near_most = 0;      // the longest list seen
  std::uint64_t near_read = 0;      // elements looked at, over the frames
  std::uint64_t near_chr = 0;       // of them, a character behind the address
  std::uint64_t near_distance = 0;  // of them, a plain fraction beside it
  std::uint64_t near_rising = 0;    // of them, not below the element's before it
  std::vector<std::uint32_t> pos_frames_by;  // by span (the marks so far): frames with a position after one with one
  std::vector<std::uint32_t> pos_moved_by;   // of those, the position more than a centimetre from the frame's before
  std::vector<WorldAt> at_marks;    // the first frame of each span: what was read just after the mark
  // A character's other fields through the layout (the nineteenth round), over the frames that read them: counts of
  // the near list's characters that are not the main player, the most a frame had, and the main player's own.
  std::uint32_t fields_bound = 0;      // which fields the layout binds (ErBackend::WorldAlone's bits)
  std::uint64_t fields_frames = 0;     // frames that read them
  std::uint64_t chr_self = 0;          // list elements that were the main player itself
  std::uint64_t chr_object = 0;        // the others' objects copied
  std::uint64_t chr_handle = 0;        // of them, a handle that is not nothing
  std::uint64_t chr_param = 0;         // a kind's number of an id's size
  std::uint64_t chr_kin = 0;           // the kind's number over 10,000 is the model's
  std::uint64_t chr_entity = 0;        // an entity's id that is not nothing
  std::uint64_t chr_dead = 0;          // the dead bit set
  std::uint64_t chr_on_screen = 0;     // the on-screen bit set
  std::uint64_t handles_shared = 0;    // frames in which two characters had one handle
  std::uint32_t objects_most = 0;      // the most a frame had: objects,
  std::uint32_t handles_most = 0;      //   different handles,
  std::uint32_t kinds_most = 0;        //   different kinds,
  std::uint32_t entities_most = 0;     //   different entity ids,
  std::uint32_t teams_most = 0;        //   different teams,
  std::uint32_t team_group_most = 0;   //   and characters of one team
  std::uint64_t self_frames = 0;       // frames with the main player's object copied for its fields
  std::uint64_t self_handle_frames = 0;      // of them: its handle not nothing
  std::uint64_t self_param_frames = 0;       // its kind's number not nothing
  std::uint64_t self_dead_frames = 0;        // its dead bit set
  std::uint64_t self_team_alone_frames = 0;  // its team no other's in the list
  std::vector<std::uint32_t> chr_object_by;     // by span: the others' objects copied,
  std::vector<std::uint32_t> chr_dead_by;       //   of them with the dead bit set,
  std::vector<std::uint32_t> chr_on_screen_by;  //   and with the on-screen bit set
  // The type and the block through the layout (the twentieth round), over the same frames.
  std::uint64_t chr_block = 0;             // the others with a block that reads as a block's id
  std::uint64_t chr_block_self = 0;        // of them, in the block the main player is in
  std::uint64_t chr_type_main = 0;         // the others of the frame's largest group of one type, summed
  std::uint32_t types_most = 0;            // the most different types a frame had
  std::uint64_t self_type_none_frames = 0; // frames in which the main player's type read nothing
  std::uint64_t self_block_frames = 0;     // frames in which its block read as one
  std::uint64_t zero_fields_frames = 0;    // frames with the fields in which its health read nothing
  std::uint64_t dead_at_zero = 0;          // of them, with its dead bit set
  std::vector<std::uint32_t> self_frames_by;         // by span: frames with the main player's fields,
  std::vector<std::uint32_t> self_dead_by;           //   of them with its dead bit set,
  std::vector<std::uint32_t> self_block_changes_by;  //   and in which its block was another than in the frame before
  // In every frame.
  std::vector<std::uint32_t> calls_by;     // by span: frames,
  std::vector<std::uint32_t> player_by;    //   with the main player there,
  std::vector<std::uint32_t> hp_zero_by;   //   and with its health reading nothing
  std::uint64_t clock_frames = 0;          // frames with a clock (an hour below 24, a minute below 60)
  std::uint64_t clock_steps = 0;           // of them: the minute one on from the frame's before (the hour with it)
  std::uint64_t clock_same = 0;            //   the same minute as before
  std::uint64_t clock_jumps = 0;           //   anything else
  std::uint64_t play_frames = 0;           // frames with a play time
  std::uint64_t play_rose = 0;             // of them: above the frame's before
  std::uint64_t play_fell = 0;             //   below it
};

struct PairHit {
  std::uint32_t rva = 0;     // the slot
  std::uint32_t offset = 0;  // the field, from the object's start
  std::uint32_t width = 4;   // 4: a 32-bit number read both; 1: one byte did (and the 32-bit number it lies in did not)
  std::uint32_t sites = 0;   // instructions of the code that refer to the slot (the round's count)
  bool lead = false;         // its object's class is the one the game state's lead names (the game's own type information)
};
struct PairFound {
  std::uint32_t first = 0;   // the number sought in the first world
  std::uint32_t second = 0;  // and in the second
  std::uint32_t candidates = 0;
  std::uint32_t asked = 0;
  std::uint32_t passes = 0;
  bool overrun = false;
  std::vector<StateLook> looks;
  std::uint32_t objects_first = 0;  // objects read at the first look
  std::uint32_t objects_second = 0; // of those with a field that read the first number, objects read at the second
  std::uint64_t words_first = 0;    // 32-bit fields that read the first number
  std::uint64_t bytes_first = 0;    // bytes that did
  std::uint32_t word_hits = 0;      // 32-bit fields that then read the second number
  std::uint32_t byte_hits = 0;      // bytes that did, outside those fields
  std::uint32_t leads = 0;          // tables of methods the image holds under the game state's lead's name
  std::vector<PairHit> hits;        // the 32-bit ones first; at most 64
};

// The parts watch (the fourteenth round; WHY§460's third step: the moving numbers by invariant, with the tester's
// tasks): the objects the holder of the main player's parts points at, read once a second, and each 32-bit field
// counted for how it changed in each phase of the round (the tester stands, walks, stands, is hit, heals). What a
// field held is never reported: only where it lies and how it behaved.
struct PartFound {
  std::uint32_t index = 0;   // which word of the holder
  std::uint32_t vtable = 0;  // the part's table of methods; 0 for none
  std::uint32_t bytes = 0;   // read of it a pass
};
struct PartMove {            // a field that changed while the tester walked and never while they stood
  std::uint32_t part = 0;
  std::uint32_t offset = 0;
  bool fraction = false;     // it reads as a plain fraction
  std::uint32_t span = 0;    // how far it ranged while the tester walked, as a fraction: 0 not known, 1 within 2 (a
                             // direction), 2 within 8 (an angle, a speed), 3 within 10,000 (a place), 4 further
  std::vector<std::uint32_t> changed;  // by phase: passes in which it differed from the pass before
};
struct PartFall {            // a number that fell when the tester was hit
  std::uint32_t part = 0;
  std::uint32_t offset = 0;
  std::uint32_t fell = 0;    // passes of the hit's phase in which it fell
  std::uint32_t rose = 0;    // passes of the heal's phase in which it rose
  bool has_max = false;      // a number beside it never changed and is at least as large: its maximum
  std::uint32_t max_at = 0;
};
struct PartsFound {
  std::uint32_t player_word = 0;  // where in the manager the main player's address stands (the character search's)
  std::uint32_t holder_at = 0;    // where in the player's object the holder's address stands
  std::uint32_t passes = 0;
  std::uint32_t absent = 0;       // passes at which there was no player or no holder
  std::uint32_t restarts = 0;     // times the player's object or the holder was another than before
  std::uint32_t phase = 0;        // the phase under way
  std::vector<std::uint32_t> phase_passes;  // counted passes by phase (the first seconds of a phase are not counted)
  std::vector<PartFound> parts;
  std::uint32_t moves_total = 0;
  std::vector<PartMove> moves;    // at most 64
  std::uint32_t falls_total = 0;
  std::vector<PartFall> falls;    // at most 32
};

// EldenNet's own reader on the flags (the eleventh read-only round; WHY§457): discovery through the build's layout
// as it stands, the backend built on it, and its flags group read alone on the round's reading thread; compared
// with what the round's own tree reader and sweep found. A flag is written by its number.
struct BackendFlag {
  std::uint32_t flag = 0;   // the flag's number
  bool expected = false;    // what the sweep saw the bit become (the known flag: set)
  bool known = false;       // this is the flag that is set in every playthrough, not one from a mark
  bool valid = false;       // the backend found the flag's group
  bool set = false;         // what the backend read
};
struct BackendFound {
  bool discovered = false;   // discovery read the build's files and made its pass
  bool flags_group = false;  // it found everything the flags group asks for
  bool ready = false;        // it found enough to start (not expected: only the flags are placed)
  std::uint32_t items = 0;   // items resolved, of those the design asks for
  std::uint32_t table_items = 0;
  std::uint32_t table_refused = 0;
  std::uint64_t calls = 0;   // times the flags group was read alone
  std::uint64_t reads = 0;   // of those, the ones that gave a frame
  bool known_read = false;   // the known flag was read at the last call
  bool known_set = false;
  bool tripped = false;      // the flags group's breaker tripped
  std::uint64_t reader_faults = 0;
  std::vector<BackendFlag> flags;  // the known flag first, then the marks' flags; at most 64
};

// A static found by what its object holds (proposal 0005 §6.1's `invariant`): the slot of the data behind which one
// field rose by the clock in nearly every pass of the rising search, far ahead of every other. The code's references
// to the slot are its sites and witnesses.
struct InvariantFound {
  std::string label;       // the catalogue's
  std::uint32_t id = 0;
  std::uint32_t rva = 0;     // the slot
  std::uint32_t offset = 0;  // the field, from the object's start
  std::uint32_t rose = 0;    // passes it rose in
  std::uint32_t passes = 0;  // of so many comparisons of the field from its first rise
  std::uint32_t sites = 0;   // instructions of the code that refer to the slot
  std::vector<std::uint32_t> witnesses;
};

struct Found {
  ImageFound image;
  AccessorFound accessor;
  std::vector<ClassFound> classes;
  TableFound table;
  std::vector<LeadClasses> lead_classes;    // for each manager the leads ask for, offline and in game
  std::optional<InstancesFound> instances;  // the in-game round's; nothing offline
  std::optional<TrialFound> trial;          // the in-game round's
  std::optional<ClockFound> clock;          // the in-game round's searches by what an object holds (step L6)
  std::optional<RisingFound> rising;
  std::optional<SearchFound> search;
  std::optional<MembersFound> members;
  std::optional<ScanFound> scan;
  std::optional<WatchFound> watch;
  std::optional<SweepFound> sweep;
  std::optional<TreeFound> tree;
  std::optional<BackendFound> backend;
  std::optional<StateFound> state;
  std::optional<CharFound> chars;
  std::optional<PairFound> pair;
  std::optional<PartsFound> parts;
  std::optional<WorldRead> world;
  std::optional<SurveyFound> survey;
  std::optional<SheetFound> sheet;
  std::vector<InvariantFound> invariants;
};

// Runs the offline recipes on an image read from its file. `builds` may be null (the build is then unknown). With
// `ids`, the catalogue, what the image holds under each lead's name is counted under the catalogue's labels
// (`lead_classes`); without it, under the leads' own.
[[nodiscard]] Found find(const ImageInfo& image, const ByteSource& source, std::uint64_t file_size,
                         const derive::Leads& leads, const RecipeSet& recipes, const Builds* builds,
                         const Catalogue* ids = nullptr);

// Whether the run found what the next step needs: the accessor's recipe, with hits within its range and at least one
// accepted site; one class object for each static and one static for each class object; and, where the withdrawn
// pattern could be cut, no site the shape alone accepts. Sites the pattern alone accepts do not count against it:
// they are reported with why the shape leaves them out (on 2.7.2.0, rightly: docs/ops/probes/2026-11-22-l3.md).
[[nodiscard]] bool complete(const Found& found);

// One guarded 8-byte read at an address outside the image; nothing when the read is refused or faults.
using ReadWord = std::function<std::optional<std::uint64_t>(std::uint64_t address)>;

// Names the managers by their instances (O92). Made once over the image (its type information does not change while
// the game runs), then asked as often as the round likes: a pass reads each static's slot, 8 bytes through `live`,
// and one word at each instance through `read_word`, and nothing else.
// A lead names the one static whose instance's class has its name or its name with Imp; where no static does, the
// one whose instance's class derives from such a class (`by_base`); and where the image has no class of its name at
// all, the one whose instance's class has a name that holds the lead's (`by_related`). Two statics that match the same way leave the lead
// ambiguous. A lead's label is the catalogue's label for the same thing, or the label it had before (`was`).
class Namer {
 public:
  Namer(const ImageInfo& image, const ByteSource& source, const derive::Leads& leads, const Catalogue& ids);
  ~Namer();
  Namer(const Namer&) = delete;
  Namer& operator=(const Namer&) = delete;

  // What the image holds under each lead's name. Offline too: it reads the image only.
  [[nodiscard]] const std::vector<LeadClasses>& lead_classes() const noexcept;
  [[nodiscard]] InstancesFound pass(const ByteSource& live, const AccessorFound& accessor,
                                    const ReadWord& read_word) const;
  // The tables of methods an instance of a lead's manager may carry (its class's and those of classes deriving from
  // it), as offsets, ascending: what a search for its instance looks for. Nothing for an id no lead asks for, or for a
  // lead with no class of its name.
  [[nodiscard]] std::vector<std::uint32_t> vtables(std::uint32_t id) const;
  // The same for every lead at once, the catalogue's or not: each table of methods with the lead it answers (the
  // lead's number, 0 where the catalogue does not hold it, and its label). What the member search looks for.
  struct LeadTable {
    std::uint32_t vtable = 0;
    std::uint32_t id = 0;
    std::string label;
  };
  [[nodiscard]] std::vector<LeadTable> lead_tables() const;
  // A lead of the leads' [statics] names the class of an object the accessor does not reach (the game's state): its
  // class's innermost name is compared, here as everywhere, and kept nowhere. Whether `vtable` is the primary table
  // of methods of a class of that name (1), of that name with Imp (2), or neither (0); and how many tables answer the
  // lead at all (0: the image holds no class of its name, or the leads hold no such lead).
  [[nodiscard]] int static_lead_class(std::string_view label, std::uint32_t vtable) const;
  [[nodiscard]] std::uint32_t static_lead_tables(std::string_view label) const;
  // Every class's primary table of methods the image's type information gives, as offsets, ascending: what an
  // object's first word is compared with to tell where an object starts.
  [[nodiscard]] std::vector<std::uint32_t> primary_tables() const;

 private:
  struct State;
  std::unique_ptr<State> state_;
};

// One pass of a Namer made for the call: the image is read whole each time. A round that names more than once keeps
// a Namer instead.
[[nodiscard]] InstancesFound name_instances(const ImageInfo& image, const ByteSource& live, const AccessorFound& accessor,
                                            const derive::Leads& leads, const Catalogue& ids, const ReadWord& read_word);

// Folds a later pass into what the round holds (a manager the game makes only once a save is loaded is null at the
// title): the counts become the later pass's, a manager once named stays as it was named, and one still unnamed takes
// the later pass's finding, with the pass that named it; what the searches found for a manager (set_by_search,
// set_in_data) is kept through every later pass. `named` and `ambiguous` are counted again over the whole.
// The counts, and what each static held (`held`, `new_statics`, `others`), are those of the fullest pass: the one
// that found the fewest statics empty, the latest of equals. The last pass of a session is made as the game closes,
// and says little of the game in play (the fourth round of 2026-11-22 reported it).
void merge(InstancesFound& held, const InstancesFound& later);

// A manager that no accessor's static holds, looked for by the search of the data's slots for an object of its class
// (probe.hpp's SlotSearch, read-only). The slots found name the manager when they hold one object between them: one
// slot, or several that are copies of one address (the first by offset is taken, and the others are reported). Two
// objects leave it ambiguous; none changes only the search's own numbers. A manager already named by a static of the
// accessor's keeps its name, and one the search named keeps the search's finding through later passes (merge).
struct SlotSearchResult {
  std::vector<std::uint32_t> slots;  // by offset
  std::uint32_t objects = 0;         // distinct objects behind them
  std::uint32_t vtable = 0;          // the first slot's object's table of methods
  std::uint32_t candidates = 0;
  std::uint32_t checked = 0;
  std::uint32_t unreadable = 0;
};
void set_by_search(InstancesFound& held, std::uint32_t id, const SlotSearchResult& result);

// The code's references to the slots a search found, in search_slots' order, and the first places that refer to each
// (at most kMaxSiteWitnesses a slot). Where the search named the manager (one object), its slot becomes the one the
// code refers to most (the first by offset among equals), with those references as its sites and witnesses.
void set_search_refs(InstancesFound& held, std::uint32_t id, const std::vector<std::uint32_t>& counts,
                     const std::vector<std::vector<std::uint32_t>>& places);

// For each of `rvas` (slots), the places in a section of the image's code whose instruction has a RIP-relative
// operand that is the slot: how many there are, and the first `max`, each the instruction's start, ascending. The
// image is read from `source` once. What the resolver checks a witness by (resolve.hpp) is the same reading of one
// instruction.
struct References {
  std::uint64_t count = 0;
  std::vector<std::uint32_t> places;
};
[[nodiscard]] std::vector<References> references(const ImageInfo& image, const ByteSource& source,
                                                 std::string_view section, std::span<const std::uint32_t> rvas,
                                                 std::size_t max);

// Objects of a manager's class in the image's own data: how many, and the first. Reported, never a name by itself.
void set_in_data(InstancesFound& held, std::uint32_t id, std::uint32_t count, std::uint32_t first);

// The round's findings as a build's table: `[provenance]` and one `[item.<label>]` for each manager named, each class
// the leads' sets gave and the groups' table, in the catalogue's order. Each entry is checked against the image first
// (resolve.hpp): one that fails its placement is left out and counted, a witness that no longer refers to its id is
// dropped, and `checks` lists only what passed. Every entry is a lead: the review confirms it.
struct DraftTable {
  std::string text;          // TOML, to follow a schema-2 layout's head
  std::uint32_t entries = 0;
  std::uint32_t left_out = 0;          // entries whose placement failed
  std::uint32_t witnesses_dropped = 0;
};
[[nodiscard]] DraftTable draft_table(const Found& found, const Catalogue& ids, const TableProvenance& provenance,
                                     const ImageInfo& image, const ByteSource& source);

[[nodiscard]] const report::Vocabulary& vocabulary() noexcept;  // kind `lodestar`, schema 2
[[nodiscard]] proto::Expected<std::string, report::Problem> write_report(const Found& found);

}  // namespace en::gal::lodestar
