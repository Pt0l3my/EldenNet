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
};

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
};

// The in-game round's naming (O92), per distinct static of the accessor's accepted sites.
struct InstancesFound {
  std::uint64_t tried = 0;      // statics looked at
  std::uint64_t null = 0;       // the static holds no instance yet
  std::uint64_t unread = 0;     // the instance's first word could not be read, or the instance lies in the image
  std::uint64_t unclassed = 0;  // read, but no table of methods the image's type information knows
  std::uint64_t classed = 0;    // an instance whose class the image gives
  std::uint64_t leads = 0;      // managers the leads ask for
  std::uint64_t unlisted = 0;   // of those, the ones whose label the catalogue does not hold as a static
  std::uint64_t named = 0;
  std::uint64_t ambiguous = 0;
  std::uint32_t passes = 0;            // how many times the round named (merge counts them)
  std::vector<ManagerFound> managers;  // by number: every lead the catalogue holds, found or not
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

struct Found {
  ImageFound image;
  AccessorFound accessor;
  std::vector<ClassFound> classes;
  TableFound table;
  std::vector<LeadClasses> lead_classes;    // for each manager the leads ask for, offline and in game
  std::optional<InstancesFound> instances;  // the in-game round's; nothing offline
  std::optional<TrialFound> trial;          // the in-game round's
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
// one whose instance's class derives from such a class (`by_base`). Two statics that match the same way leave the lead
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
// the later pass's finding, with the pass that named it. `named` and `ambiguous` are counted again over the whole.
void merge(InstancesFound& held, const InstancesFound& later);

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
