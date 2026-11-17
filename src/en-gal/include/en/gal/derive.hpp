#pragma once
// en-gal's derivation (O89's offline round R0; ARCH §6.2, §6.3, §6.10): from an executable read as a file, candidates
// for what the game access layer needs, found by our own code from what the RTTI walk (rtti.hpp) and the FD4 finder
// (fd4.hpp) find, each pattern cut by our own decoder (x86.hpp) as an interior-anchored pattern of at most 64 bytes
// with its wildcards, checked unique in .text and checked again by part 2's scan (patterns.hpp) as patterns.toml would
// hold it; and the list of what the in-game round R1 must confirm.
//
// The leads say what to look for: names and shapes, never bytes (ARCH §6.3, "what a lead may carry"). They come from a
// leads file, under labels of their author's own (lower-case letters, digits and _), and only those labels reach the
// report, which is written through the numbers-only writer (report.hpp): no name, string, path or code read from the
// executable is ever written. Nothing here knows the game: every name comes from the leads.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/gal/fd4.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/report.hpp"
#include "en/gal/rtti.hpp"
#include "en/gal/schema.hpp"
#include "en/proto/expected.hpp"

namespace en::gal::derive {

// ---- The leads ----

struct NamedLead {
  std::string label;  // ours: [a-z][a-z0-9_]*
  std::string name;   // what to look for in the executable
  friend bool operator==(const NamedLead&, const NamedLead&) = default;
};

struct StaticLead {
  std::string label;
  std::string rtti;     // the class of the instance the static holds; empty with `module`
  bool module = false;  // the static holds the module's base
  friend bool operator==(const StaticLead&, const StaticLead&) = default;
};

struct Leads {
  std::string product_name;            // the version resource's ProductName the build should carry; empty: not checked
  std::string fd4_path_suffix;         // the path every FD4 site loads ends with this
  std::vector<NamedLead> singletons;   // label, the name its runtime class object gives
  std::string register_instance;       // the label of the singleton whose instance the registration takes
  std::string task_group;              // the name of the task group our task joins
  std::string task_base;               // the class every task of the game derives from (RTTI)
  std::vector<NamedLead> vtable_sets;  // label, a class whose derived classes' vtables make a set
  std::vector<StaticLead> statics;     // the AOB statics
};

// Reads a leads file (TOML, schema 1; a 64 KiB cap). `file` names it in the errors.
[[nodiscard]] proto::Expected<Leads, SchemaErrors> parse_leads(std::string_view text, std::string_view file = "leads");

// ---- What the derivation finds ----

enum class Status : std::uint8_t {
  Derived,    // found, and checked
  Ambiguous,  // found more than once where one was wanted
  Refused,    // found, but a check refused it
  Missing,    // not found
  NotUnique,  // no pattern of at most 64 bytes was unique
  NeedsSlot,  // a static whose slot only R1 can find
  Depends,    // not tried: what it rests on was not found
};

[[nodiscard]] const char* to_string(Status status) noexcept;

struct Cut {
  BytePattern bytes;
  std::uint32_t hit = 0;    // where it matches
  std::uint32_t entry = 0;  // its function's entry and length (functions and statics alike)
  std::uint32_t length = 0;
  std::uint64_t matches = 0;       // in .text
  std::optional<Capture> capture;  // a static's displacement
  std::uint32_t target = 0;        // and where it leads
  bool scan_resolved = false;      // part 2's scan resolves it as patterns.toml would hold it
};

struct Fingerprint {
  std::uint64_t image_base = 0;
  std::uint32_t entry_point = 0;
  std::uint32_t size_of_image = 0;
  std::uint32_t size_of_headers = 0;
  std::uint32_t section_alignment = 0;
  std::uint32_t file_alignment = 0;
  std::uint32_t time_date_stamp = 0;
  std::uint16_t characteristics = 0;
  std::uint16_t dll_characteristics = 0;
  std::uint16_t subsystem = 0;
  std::uint64_t functions = 0;
  std::vector<Section> sections;
  bool has_version = false;
  std::array<std::uint16_t, 4> file_version{};
  std::array<std::uint16_t, 4> product_version{};
  std::string_view region = "none";          // ww, jp, other, none
  std::string_view product_name = "absent";  // matches (once symbols are removed: builds.hpp), differs, absent,
                                             // not_given
  std::uint64_t product_name_characters = 0;  // in the version resource's ProductName, when compared
  std::uint64_t product_name_kept = 0;        // of them, those left once its symbols are removed
  std::optional<Translation> translation;
  std::uint16_t resource_language = 0;
};

struct SingletonResult {
  std::string label;
  Status status = Status::Missing;
  std::uint32_t static_slot = 0;
  std::uint32_t class_object = 0;
  std::uint32_t getter = 0;
  std::uint32_t sites = 0;
  std::optional<fd4::Refusal> refusal;
};

// Where a path's load sat, and how often (the FD4 diagnostics).
struct PathBucket {
  bool hoisted = false;        // before the site's head
  std::uint32_t at = 0;        // the address field's offset from the site's start (absent when hoisted)
  std::uint32_t insn_end = 0;  // the load's end, from the site's start (absent when hoisted)
  std::uint32_t before = 0;    // hoisted: from the load's start to the site's start
  std::uint32_t length = 0;    // the load's length
  std::uint32_t ordinal = 0;   // other references into the read-only section met before it
  std::uint8_t reg = 0;        // the register it writes, 0 (rax) to 15 (r15)
  std::uint32_t opcode = 0;    // its opcode: map * 256 + the opcode byte (0x8D lea, 0x8B mov, 0xB8 to 0xBF mov imm64)
  std::uint64_t count = 0;
};

// Why the site search did or did not find the idiom's head at a reference to a path (the FD4 diagnostics).
enum class SiteOutcome : std::uint8_t {
  Site,         // a site: the static's load, its test and a jnz whose jump passes the reference
  NoLanding,    // decoding from the function's entry does not land on the reference
  JumpShort,    // such a head before it, whose jump does not pass it
  CompareForm,  // no such head, but a compare of a RIP-relative quadword with zero before it
  LoadOnly,     // no such head, but a 64-bit load of a static of the data section before it
  Nothing,      // none of these within 24 instructions before it
};
inline constexpr std::size_t kSiteOutcomes = 6;

// One offset the name search tried in the class objects, as numbers only (R1 run 1 found 9 of 217 named, WHY§206): where
// the 64-bit value there points, and what text it reaches; and the text held in the object itself at that offset.
struct NameSlot {
  std::uint32_t offset = 0;
  std::uint64_t null_ptr = 0;    // zero
  std::uint64_t to_rdata = 0;    // into .rdata
  std::uint64_t to_data = 0;     // into .data
  std::uint64_t to_code = 0;     // into a code section
  std::uint64_t to_other = 0;    // into another section of the image
  std::uint64_t outside = 0;     // non-zero, outside the image (the heap, or not a pointer)
  std::uint64_t plain = 0;       // in the image: an identifier (ASCII or UTF-16)
  std::uint64_t qualified = 0;   // in the image: identifiers joined by ::
  std::uint64_t other_text = 0;  // in the image: other text of four characters or more
  std::uint64_t inline_text = 0;  // in the object itself: an identifier, plain or qualified, of four characters or more
};

struct Fd4Diagnostics {
  std::array<std::uint64_t, kSiteOutcomes> outcomes{};      // per reference to a path
  std::vector<std::pair<std::uint64_t, std::uint64_t>> heads;  // per head shape, largest first: sites, matched
  std::vector<PathBucket> site_paths;                       // the idiom's sites' path loads, as the references show
  std::vector<PathBucket> hit_paths;                        // the accepted hits' path loads, as the finder found them
  std::uint64_t first_reference_accepted = 0;               // R0's rule over the same hits
  std::array<std::uint64_t, 9> first_reference_refused{};
  std::uint64_t class_objects = 0;    // distinct, of the accepted hits
  std::uint64_t statics = 0;          // distinct statics of the accepted hits
  std::uint64_t getters = 0;          // distinct getters of the accepted hits
  std::uint64_t names_best = 0;       // the class objects the best name layout reads, even under half
  std::optional<fd4::NameLayout> best_names;
  std::uint64_t names_qualified = 0;  // of names_best, the qualified ones (identifiers joined by ::)
  std::vector<NameSlot> name_slots;   // every offset tried, in order
  std::uint64_t class_fixups = 0;     // class objects with a base relocation in their first 128 bytes
  std::uint64_t class_beyond_file = 0;  // class objects past the raw data the file holds (zeros once mapped)
};

struct Fd4Result {
  Status status = Status::Missing;
  std::uint64_t path_strings = 0;
  std::uint64_t path_references = 0;
  std::uint64_t sites = 0;       // references whose idiom's head was found before them
  std::uint64_t shapes = 0;      // distinct heads among them
  std::uint64_t sites_used = 0;  // the sites of the head the idiom was cut from
  std::optional<fd4::Idiom> idiom;
  std::uint64_t idiom_hits = 0;
  std::uint64_t sites_matched = 0;  // of sites_used, how many the idiom matches
  std::optional<fd4::NameLayout> names;
  std::uint64_t names_agree = 0;
  std::uint64_t names_of = 0;
  std::uint64_t accepted = 0;
  std::array<std::uint64_t, 9> refused{};  // by fd4::Refusal
  std::uint64_t unnamed = 0;
  std::uint64_t unlisted = 0;  // distinct names the leads do not list
  std::uint64_t unlisted_ambiguous = 0;
  bool scan_resolved = false;  // part 2's scan, with the idiom as patterns.toml would hold it
  std::uint64_t scan_hits = 0;
  std::uint64_t scan_rejected = 0;
  std::vector<SingletonResult> singletons;  // in the leads' order
  Fd4Diagnostics diagnostics;
};

struct VtableSetResult {
  std::string label;
  Status status = Status::Missing;
  std::uint32_t type_descriptor = 0;
  std::size_t classes = 0;
  std::vector<std::uint32_t> primary;
  std::vector<std::uint32_t> subobject;
};

struct ClassResult {
  Status status = Status::Missing;
  std::uint32_t type_descriptor = 0;
  std::uint32_t vtable = 0;  // the primary vtable
  std::uint32_t slots = 0;
  std::size_t vtables = 0;
};

struct TaskResult {
  Status status = Status::Missing;
  ClassResult base;
  std::vector<std::uint32_t> vtables;  // the derived classes' primary vtables
  std::uint32_t slots_min = 0;
  std::uint32_t slots_max = 0;
  std::vector<std::uint32_t> slot_targets;  // per slot, how many distinct functions the classes put there
  std::optional<std::uint32_t> run_slot;    // the one slot every class fills with a function of its own
};

// Registers by number: 1 rcx, 2 rdx, 8 r8, 9 r9.
struct Abi {
  std::optional<std::uint8_t> instance;
  std::optional<std::uint8_t> task;   // a 64-bit argument besides the instance
  std::optional<std::uint8_t> group;  // a 32-bit argument
  bool shaped = false;                // exactly one of each, and no other argument written
  friend bool operator==(const Abi&, const Abi&) = default;
};

struct RegisterCandidate {
  std::uint32_t callee = 0;
  std::uint32_t sites = 0;
  Abi abi;
  bool abi_agrees = true;              // every site gave the same
  bool function = false;               // the callee starts a function of the exception directory, not a fragment
  std::vector<std::uint32_t> callers;  // the call instructions, the first 16
};

struct RegisterResult {
  Status status = Status::Missing;
  std::uint32_t instance_static = 0;
  std::uint64_t loads = 0;                    // instructions loading the instance's static
  std::uint64_t calls = 0;                    // calls that pass it
  std::vector<RegisterCandidate> candidates;  // best first, at most 8
  std::optional<Cut> pattern;                 // cut in the first candidate
};

struct GroupCandidate {
  std::uint32_t index = 0;
  std::string_view source;  // pointer_table, code_reference
  std::uint32_t at = 0;     // the table's first name pointer (or its first entry, where code addresses it), or the
                            // referencing instruction
  std::uint32_t entries = 0;
  std::uint32_t stride = 0;                  // a table's: bytes from one entry's name pointer to the next's
  std::optional<std::uint32_t> name_offset;  // how far before an entry's name pointer code addresses the entry
  bool wide = false;                         // the names are UTF-16
  std::optional<std::uint32_t> stored_index; // a field of the entries holding position + a constant: its value in ours
};

struct GroupResult {
  Status status = Status::Missing;
  std::uint64_t strings = 0;
  std::uint64_t references = 0;  // instructions of .text addressing one of the strings
  std::uint64_t pointers = 0;    // 8-aligned pointers to one of them in initialised data
  std::vector<GroupCandidate> candidates;  // tables of two entries or more first, then code, then lone pointers
};

struct StaticResult {
  std::string label;
  bool module = false;
  Status status = Status::NeedsSlot;
  ClassResult rtti;  // the class's vtable, for R1's search
  std::optional<std::uint32_t> slot;
  std::uint64_t references = 0;
  std::optional<Cut> pattern;
  std::uint64_t same_name = 0;  // type descriptors of a class or struct of the class's own name, in any namespace
};

struct ConfirmItem {
  std::string_view item;
  std::string label;  // empty when the item takes none
  std::optional<std::uint64_t> value;
};

// One executable section, decoded through as the derivation decodes .text (the decoder's diagnostics).
struct SectionCensus {
  std::string name;  // read from the file: kept in memory, the report writes a fixed word for it
  std::uint32_t rva = 0;
  std::uint32_t virtual_size = 0;
  std::uint64_t instructions = 0;
  std::uint64_t undecodable = 0;      // bytes stepped over
  std::uint64_t runs = 0;             // runs of them
  std::uint64_t runs_after_stop = 0;  // runs right after an instruction that does not fall through (data after code)
  std::uint64_t runs_at_escape = 0;   // runs whose first byte is a VEX, EVEX or XOP escape (C4, C5, 62, 8F)
  std::uint64_t in_functions = 0;     // undecodable bytes inside functions of the exception directory
  std::uint64_t path_references = 0;  // instructions addressing one of the FD4 path strings
};

struct Stats {
  std::uint64_t instructions = 0;
  std::uint64_t undecodable = 0;
  std::uint64_t fixups = 0;
  std::uint64_t locators = 0;
  std::uint64_t vtables = 0;
  std::vector<SectionCensus> sections;  // every executable section
};

struct Result {
  std::uint64_t file_size = 0;
  Fingerprint fingerprint;
  Fd4Result fd4;
  std::vector<VtableSetResult> sets;
  TaskResult task;
  RegisterResult registration;
  GroupResult group;
  std::vector<StaticResult> statics;
  std::vector<ConfirmItem> confirm;
  Stats stats;
};

struct Options {
  std::uint64_t file_size = 0;
  std::vector<std::pair<std::string, std::uint32_t>> static_slots;  // R1's slots, by the statics' labels
};

// Derives everything it can from the image (a file's, ImageLayout::File). Never throws for what the image holds.
[[nodiscard]] Result derive(const ImageInfo& image, const ByteSource& source, const Leads& leads,
                            const Options& options = {});

// The task classes of a base class given by its type descriptor (the leads' `[task] base`, or a base R1 finds in game
// from the hub's own tasks): the classes deriving from it, their slots, and the run slot's candidate. `base.type_descriptor`
// is set; the rest of `base` is the caller's.
[[nodiscard]] TaskResult task_from_base(const rtti::Walk& walk, const ImageBytes& img, std::uint32_t base);

// Whether every fact the round can derive offline was derived.
[[nodiscard]] bool complete(const Result& result);

// derive.json: the result through the numbers-only writer, under the leads' labels.
[[nodiscard]] const report::Vocabulary& vocabulary() noexcept;
[[nodiscard]] proto::Expected<std::string, report::Problem> write_report(const Result& result);

}  // namespace en::gal::derive
