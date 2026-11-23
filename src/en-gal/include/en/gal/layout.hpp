#pragma once
// gal/layouts/<build>.toml (ARCH §6.3, DESIGN §4.3): every offset and per-class copy size of one build, so that a patch
// that only moves fields is a data update. A Layout is read only for the build the gate matched (R7): load_layout()
// takes the BuildProfile, refuses a layout written for another build, and is the only maker of a Layout.
//
// Since schema 2 the file is also the build's table of Lodestar, EldenNet's own address library (proposal 0005 §5):
// [item.<label>] says where each id of gal/ids.toml lies in this build, how it was found and which checks it passed,
// and [provenance] where the table came from. A schema 1 file holds neither.
//
// Validation, at boot and in CI alike (O4): the schema, field types and offsets within their class (off + size ≤ the
// class's copy size, copy sizes ≤ 0x4000), every `to` and `elem` naming a class of the file, every field the read
// groups need present with its type, each enum table total over the range of the fields that use it and naming only
// the snapshot enum's values, and, in a release build, no entry still a `lead`. Nothing here knows the game: what the
// groups need comes from the caller (LayoutNeeds), and the numbers from the derivation round (O89).
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/gal/builds.hpp"
#include "en/gal/ids.hpp"
#include "en/gal/schema.hpp"
#include "en/proto/expected.hpp"
#include "en/proto/snapshot.hpp"

namespace en::gal {

enum class FieldType : std::uint8_t {
  U8,
  U16,
  U32,
  U64,
  I32,
  F32,
  Bool8,
  Bit,
  Bits,  // a run of bits in a little-endian word: { word, shift, width }
  Ptr,
  Handle,
  Vec3f,
  Mat4f,
  Utf16,
  DlVector,
  DlMap,
  Array
};

[[nodiscard]] const char* to_string(FieldType type) noexcept;  // as the file writes it: "u8", "utf16[n]", "array"

// An entry's standing: a lead from the research, or confirmed by the probe and the self-tests. A release build refuses
// any lead; a dev build accepts them, with a warning banner.
enum class LayoutStatus : std::uint8_t { Lead, Confirmed };

struct FieldLayout {
  std::string name;
  std::uint32_t off = 0;
  FieldType type = FieldType::U8;
  std::uint32_t size = 0;             // in bytes: the type's, utf16[n]'s 2n, an array's len × stride
  std::optional<proto::Group> group;  // the read group it belongs to
  bool boot = false;                  // the boot group's (read once, at registration)
  std::optional<std::pair<std::int64_t, std::int64_t>> range;  // a whole-number field's plausible values
  std::uint8_t bit = 0;                                        // a bit field's bit, 0 to 7
  std::uint8_t shift = 0;                                      // a bits field's lowest bit in its word
  std::uint8_t width = 0;                                      // a bits field's number of bits, 1 to 32
  std::string to;                                              // a pointer's class
  std::string enum_table;                                      // a whole-number field's enum table
  std::string elem;                                            // an array's element class
  std::uint32_t len = 0;                                       // an array's length, utf16[n]'s n
  std::uint32_t stride = 0;                                    // an array's stride
  // A far field (WHY§506): one that lies past its class's copy size, in a large object, and is read at the object's
  // address plus `off` by a copy of its own. `far_at` is where the reader keeps those bytes: past the class's copy,
  // in the object's buffer.
  bool is_far = false;  // (`far` itself is a word of the Windows headers)
  std::uint32_t far_at = 0;

  // Where the field's bytes lie in the reader's buffer of its object.
  [[nodiscard]] std::uint32_t at() const noexcept { return is_far ? far_at : off; }
};

inline constexpr std::uint32_t kMaxFarOffset = 0x100000;  // a far field's offset lies below this (1 MiB)
inline constexpr std::uint32_t kMaxFarBytes = 32;         // and its copy is no longer than this

struct ClassLayout {
  std::string name;
  std::string rtti;                        // the class its objects' vtables must name; empty for none
  std::string singleton;                   // the name its FD4 singleton's runtime class gives (ARCH §6.2); empty for none
  std::optional<std::uint32_t> copy_size;  // the bytes of one guarded copy of an object
  std::optional<std::uint32_t> stride;     // the step between objects in an array or table
  LayoutStatus status = LayoutStatus::Lead;
  std::vector<FieldLayout> fields;
  std::uint32_t far_bytes = 0;             // what its far fields take in the reader's buffer, past the copy

  [[nodiscard]] const FieldLayout* field(std::string_view name) const noexcept;
  // The reader's buffer for one object: the copy, then each far field's own bytes.
  [[nodiscard]] std::uint32_t buffer_size() const noexcept {
    return copy_size.value_or(stride.value_or(0)) + far_bytes;
  }
};

struct EnumTable {
  std::string name;
  std::vector<std::pair<std::int64_t, std::string>> values;  // raw value to the snapshot enum's name, by raw value

  // The name a raw value maps to, or nothing (an implausible value).
  [[nodiscard]] std::optional<std::string_view> name_of(std::int64_t raw) const noexcept;
};

struct ParamLayout {
  std::string name;
  std::uint32_t slot = 0;
  std::uint32_t row_size = 0;
  LayoutStatus status = LayoutStatus::Lead;
  std::vector<FieldLayout> fields;
};

// The registers of register_task's arguments (ARCH §6.4): Windows x64 passes the first four in rcx, rdx, r8 and r9.
enum class ArgRegister : std::uint8_t { Rcx, Rdx, R8, R9 };
[[nodiscard]] const char* to_string(ArgRegister r) noexcept;  // "rcx", "rdx", "r8", "r9"

// Which register carries each of register_task's three arguments: data, as the derivation found it (WHY§123), never a
// fixed order in code. `register_abi = { task = "rcx", instance = "rdx", group = "r8" }`, three different registers.
struct RegisterAbi {
  ArgRegister task = ArgRegister::Rcx;
  ArgRegister instance = ArgRegister::Rdx;
  ArgRegister group = ArgRegister::R8;
  friend constexpr bool operator==(const RegisterAbi&, const RegisterAbi&) = default;
};

struct TaskLayout {
  std::string group_name;  // checked against the game's table on the first tick
  std::uint32_t group_index = 0;
  RegisterAbi register_abi;  // the argument registers S0-A fixes
  std::uint32_t vtable_slots = 0;
  std::uint32_t run_slot = 0;
  std::uint32_t object_size = 0;
  LayoutStatus status = LayoutStatus::Lead;
  // A lead that holds only its group (register_abi, vtable_slots, run_slot and object_size all absent: R1's to find).
  // Discovery is never ready with it, and install_task refuses it.
  bool partial = false;
};

struct FmgLayout {
  std::string indexing;  // "flat"
  std::uint32_t group_stride = 0;
  std::vector<std::uint32_t> npc_names;    // the tables of NPC names, in the order they are searched (ARCH §6.9)
  std::vector<std::uint32_t> place_names;  // the tables of place names, likewise
  LayoutStatus status = LayoutStatus::Lead;
};

struct DlrfLayout {
  std::uint32_t name_ptr_off = 0;
  LayoutStatus status = LayoutStatus::Lead;
};

// Ids only, never text: known ids the text and params self-tests look up (ARCH §6.7).
struct SelfTestIds {
  std::vector<std::uint32_t> npc_name_ids;
  std::vector<std::uint32_t> place_name_ids;
  std::vector<std::uint32_t> npc_param_ids;
};

// ---- The build's table: where each of Lodestar's ids lies in this build (schema 2; proposal 0005 §5 to §7) ----

// How an id's value was found (proposal 0005 §6.1). A recipe finds one kind of thing: an accessor, an instance's class
// and an invariant find a static; a class recipe a class; a table recipe a table; a relation a function or a static.
enum class Recipe : std::uint8_t { Accessor, Class, InstanceClass, Table, Relation, Invariant };
[[nodiscard]] const char* to_string(Recipe r) noexcept;  // "accessor", "class", "instance_class", ...

// The checks an id passed in the reviewed round (proposal 0005 §7.1).
enum class ItemCheck : std::uint8_t { Placement, Agree, Witness, Digest, Class, SelfTest, SecondSource, Verdict };
[[nodiscard]] const char* to_string(ItemCheck c) noexcept;  // "placement", "agree", "witness", ...

// What a function does with what it is handed, as our own tool judged it (proposal 0005 §7.2). Unknown means no call.
enum class Verdict : std::uint8_t { Adds, Removes, Reads, Unknown };
[[nodiscard]] const char* to_string(Verdict v) noexcept;  // "adds", "removes", "reads", "unknown"

inline constexpr std::size_t kMaxWitnesses = 8;
inline constexpr std::size_t kMaxVtables = 64;
inline constexpr std::size_t kDigestHexDigits = 32;         // a 128-bit digest, as lower-case hex
inline constexpr std::uint32_t kMaxFunctionLength = 1u << 20;

// One id in one build: numbers, our labels and fixed words only, never a byte of the game.
struct ItemEntry {
  std::string label;  // the catalogue's
  std::uint32_t id = 0;
  IdKind kind = IdKind::Static;
  std::uint32_t rva = 0;                 // a static, a function, a table: its offset from the start of the image
  std::uint32_t length = 0;              // a function's length in bytes
  std::uint32_t stride = 0;              // a table's entry size
  std::uint32_t count = 0;               // a table's entries
  std::vector<std::uint32_t> vtables;    // a class: the offsets of its tables of methods, ascending
  std::vector<Recipe> found_by;          // the recipes that gave this value, independently
  std::vector<std::uint32_t> witnesses;  // places in the code that use it, re-checked at every start
  std::string digest;                    // a function: the digest of its code as reviewed; empty for none
  std::optional<Verdict> verdict;        // a function: what it does
  std::vector<ItemCheck> checks;
  LayoutStatus status = LayoutStatus::Lead;

  [[nodiscard]] bool passed(ItemCheck c) const noexcept;
};

// Where a build's table came from (proposal 0005 §5): the tool, the round and the leads, never a path or a name of
// anyone's. The reviewed record is the layout's probe_record.
struct TableProvenance {
  std::string tool;         // the tool's version label
  std::string tool_sha256;  // 64 lower-case hex digits
  std::string round;        // the round's label of ours: "r0c", "l3"
  std::string date;         // YYYY-MM-DD
  std::vector<std::string> leads;
};

inline constexpr std::uint32_t kMaxCopySize = 0x4000;  // ARCH §6.3, and GuardedReader's cap on one copy (§6.5)

class Layout;

// What the read groups need of a layout: the caller's list, so that this file names nothing of the game.
struct FieldNeed {
  std::string_view cls;
  std::string_view field;
  FieldType type = FieldType::U8;
};
struct EnumNeed {
  std::string_view table;
  std::span<const std::string_view> names;  // the snapshot enum's names the table may map to
};
struct LayoutNeeds {
  std::span<const FieldNeed> fields;
  std::span<const EnumNeed> enums;
  std::span<const std::string_view> params;
  bool task = true;  // the [task] table is required
  // The catalogue the table's items must agree with (label, number and kind; no retired id). A file with items is
  // refused without it.
  const Catalogue* ids = nullptr;
};

enum class LayoutMode : std::uint8_t { Release, Dev };

// Reads a layout for the build the gate matched. `file` names the file in the errors.
[[nodiscard]] proto::Expected<Layout, SchemaErrors> load_layout(const BuildProfile& profile, std::string_view text,
                                                                const LayoutNeeds& needs, LayoutMode mode,
                                                                std::string_view file = "layout.toml");

// The checks load_layout makes, on a layout written for the build `entry` describes, without the gate: what
// `eldennet-engine lint --gal` runs over gal/ off game. No Layout is made (R7 keeps that to load_layout and the gate's
// BuildProfile). Empty when the file passes.
[[nodiscard]] SchemaErrors check_layout(const BuildEntry& entry, std::string_view text, const LayoutNeeds& needs,
                                        LayoutMode mode, std::string_view file = "layout.toml");

class Layout {
 public:
  [[nodiscard]] const std::string& build() const noexcept { return build_; }
  [[nodiscard]] proto::BuildKey build_key() const noexcept { return build_key_; }
  [[nodiscard]] const std::string& derived_from() const noexcept { return derived_from_; }
  [[nodiscard]] const std::string& probe_record() const noexcept { return probe_record_; }
  [[nodiscard]] const std::optional<TaskLayout>& task() const noexcept { return task_; }
  [[nodiscard]] const std::vector<ClassLayout>& classes() const noexcept { return classes_; }
  [[nodiscard]] const std::vector<EnumTable>& enums() const noexcept { return enums_; }
  [[nodiscard]] const std::vector<ParamLayout>& params() const noexcept { return params_; }
  [[nodiscard]] const std::optional<FmgLayout>& fmg() const noexcept { return fmg_; }
  [[nodiscard]] const std::optional<DlrfLayout>& dlrf() const noexcept { return dlrf_; }
  [[nodiscard]] const SelfTestIds& selftest() const noexcept { return selftest_; }
  // Schema 2: the build's table and where it came from. A schema 1 file has neither.
  [[nodiscard]] std::uint32_t schema() const noexcept { return schema_; }
  [[nodiscard]] const std::vector<ItemEntry>& items() const noexcept { return items_; }
  [[nodiscard]] const std::optional<TableProvenance>& provenance() const noexcept { return provenance_; }
  [[nodiscard]] const ItemEntry* find_item(std::string_view label) const noexcept;
  [[nodiscard]] const ItemEntry* find_item(std::uint32_t id) const noexcept;

  [[nodiscard]] const ClassLayout* find_class(std::string_view name) const noexcept;
  [[nodiscard]] const EnumTable* find_enum(std::string_view name) const noexcept;
  [[nodiscard]] const ParamLayout* find_param(std::string_view name) const noexcept;
  // Whether any entry is still a lead (a dev build shows a banner then).
  [[nodiscard]] bool has_leads() const noexcept;
  // The largest copy size or stride of any class or row: the scratch buffers' size (ARCH §6.5).
  [[nodiscard]] std::uint32_t max_copy_size() const noexcept;

 private:
  Layout() = default;
  // The one reader, for the build whose key and index are given: load_layout's (from the gate's profile) and
  // check_layout's (from builds.toml's entry, keeping no Layout).
  static proto::Expected<Layout, SchemaErrors> read(const BuildEntry& entry, proto::BuildKey build_key,
                                                    std::string_view text, const LayoutNeeds& needs, LayoutMode mode,
                                                    std::string_view file);
  friend proto::Expected<Layout, SchemaErrors> load_layout(const BuildProfile& profile, std::string_view text,
                                                           const LayoutNeeds& needs, LayoutMode mode,
                                                           std::string_view file);
  friend SchemaErrors check_layout(const BuildEntry& entry, std::string_view text, const LayoutNeeds& needs,
                                   LayoutMode mode, std::string_view file);

  std::string build_;
  proto::BuildKey build_key_;
  std::string derived_from_;
  std::string probe_record_;
  std::optional<TaskLayout> task_;
  std::vector<ClassLayout> classes_;
  std::vector<EnumTable> enums_;
  std::vector<ParamLayout> params_;
  std::optional<FmgLayout> fmg_;
  std::optional<DlrfLayout> dlrf_;
  SelfTestIds selftest_;
  std::uint32_t schema_ = 1;
  std::vector<ItemEntry> items_;
  std::optional<TableProvenance> provenance_;
};

}  // namespace en::gal
