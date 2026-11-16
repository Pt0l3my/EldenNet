#pragma once
// gal/layouts/<build>.toml (ARCH §6.3, DESIGN §4.3): every offset and per-class copy size of one build, so that a patch
// that only moves fields is a data update. A Layout is read only for the build the gate matched (R7): load_layout()
// takes the BuildProfile, refuses a layout written for another build, and is the only maker of a Layout.
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
  std::string to;                                              // a pointer's class
  std::string enum_table;                                      // a whole-number field's enum table
  std::string elem;                                            // an array's element class
  std::uint32_t len = 0;                                       // an array's length, utf16[n]'s n
  std::uint32_t stride = 0;                                    // an array's stride
};

struct ClassLayout {
  std::string name;
  std::string rtti;                        // the class its objects' vtables must name; empty for none
  std::string singleton;                   // the name its FD4 singleton's runtime class gives (ARCH §6.2); empty for none
  std::optional<std::uint32_t> copy_size;  // the bytes of one guarded copy of an object
  std::optional<std::uint32_t> stride;     // the step between objects in an array or table
  LayoutStatus status = LayoutStatus::Lead;
  std::vector<FieldLayout> fields;

  [[nodiscard]] const FieldLayout* field(std::string_view name) const noexcept;
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
};

enum class LayoutMode : std::uint8_t { Release, Dev };

// Reads a layout for the build the gate matched. `file` names the file in the errors.
[[nodiscard]] proto::Expected<Layout, SchemaErrors> load_layout(const BuildProfile& profile, std::string_view text,
                                                                const LayoutNeeds& needs, LayoutMode mode,
                                                                std::string_view file = "layout.toml");

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

  [[nodiscard]] const ClassLayout* find_class(std::string_view name) const noexcept;
  [[nodiscard]] const EnumTable* find_enum(std::string_view name) const noexcept;
  [[nodiscard]] const ParamLayout* find_param(std::string_view name) const noexcept;
  // Whether any entry is still a lead (a dev build shows a banner then).
  [[nodiscard]] bool has_leads() const noexcept;
  // The largest copy size or stride of any class or row: the scratch buffers' size (ARCH §6.5).
  [[nodiscard]] std::uint32_t max_copy_size() const noexcept;

 private:
  Layout() = default;
  friend proto::Expected<Layout, SchemaErrors> load_layout(const BuildProfile& profile, std::string_view text,
                                                           const LayoutNeeds& needs, LayoutMode mode,
                                                           std::string_view file);

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
};

}  // namespace en::gal
