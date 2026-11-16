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
#include "en/gal/patterns.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/report.hpp"
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
  std::string_view product_name = "absent";  // matches, differs, absent, not_given
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
  std::uint32_t at = 0;     // the table's start, or the referencing instruction
  std::uint32_t entries = 0;
};

struct GroupResult {
  Status status = Status::Missing;
  std::uint64_t strings = 0;
  std::vector<GroupCandidate> candidates;
};

struct StaticResult {
  std::string label;
  bool module = false;
  Status status = Status::NeedsSlot;
  ClassResult rtti;  // the class's vtable, for R1's search
  std::optional<std::uint32_t> slot;
  std::uint64_t references = 0;
  std::optional<Cut> pattern;
};

struct ConfirmItem {
  std::string_view item;
  std::string label;  // empty when the item takes none
  std::optional<std::uint64_t> value;
};

struct Stats {
  std::uint64_t instructions = 0;
  std::uint64_t undecodable = 0;
  std::uint64_t fixups = 0;
  std::uint64_t locators = 0;
  std::uint64_t vtables = 0;
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

// Whether every fact the round can derive offline was derived.
[[nodiscard]] bool complete(const Result& result);

// derive.json: the result through the numbers-only writer, under the leads' labels.
[[nodiscard]] const report::Vocabulary& vocabulary() noexcept;
[[nodiscard]] proto::Expected<std::string, report::Problem> write_report(const Result& result);

}  // namespace en::gal::derive
