// gal/layouts/<build>.toml (layout.hpp; ARCH §6.3; R7).
#include "en/gal/layout.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>

#include "toml_reader.hpp"

namespace en::gal {

using proto::Expected;
using proto::Unexpected;

const char* to_string(FieldType type) noexcept {
  switch (type) {
    case FieldType::U8: return "u8";
    case FieldType::U16: return "u16";
    case FieldType::U32: return "u32";
    case FieldType::U64: return "u64";
    case FieldType::I32: return "i32";
    case FieldType::F32: return "f32";
    case FieldType::Bool8: return "bool8";
    case FieldType::Bit: return "bit";
    case FieldType::Bits: return "bits";
    case FieldType::Ptr: return "ptr";
    case FieldType::Handle: return "handle";
    case FieldType::Vec3f: return "vec3f";
    case FieldType::Mat4f: return "mat4f";
    case FieldType::Utf16: return "utf16[n]";
    case FieldType::DlVector: return "dlvector";
    case FieldType::DlMap: return "dlmap";
    case FieldType::Array: return "array";
  }
  return "u8";
}

const char* to_string(ArgRegister r) noexcept {
  switch (r) {
    case ArgRegister::Rcx: return "rcx";
    case ArgRegister::Rdx: return "rdx";
    case ArgRegister::R8: return "r8";
    case ArgRegister::R9: return "r9";
  }
  return "rcx";
}

const char* to_string(Recipe r) noexcept {
  switch (r) {
    case Recipe::Accessor: return "accessor";
    case Recipe::Class: return "class";
    case Recipe::InstanceClass: return "instance_class";
    case Recipe::Table: return "table";
    case Recipe::Relation: return "relation";
    case Recipe::Invariant: return "invariant";
  }
  return "accessor";
}

const char* to_string(ItemCheck c) noexcept {
  switch (c) {
    case ItemCheck::Placement: return "placement";
    case ItemCheck::Agree: return "agree";
    case ItemCheck::Witness: return "witness";
    case ItemCheck::Digest: return "digest";
    case ItemCheck::Class: return "class";
    case ItemCheck::SelfTest: return "self_test";
    case ItemCheck::SecondSource: return "second_source";
    case ItemCheck::Verdict: return "verdict";
  }
  return "placement";
}

const char* to_string(Verdict v) noexcept {
  switch (v) {
    case Verdict::Adds: return "adds";
    case Verdict::Removes: return "removes";
    case Verdict::Reads: return "reads";
    case Verdict::Unknown: return "unknown";
  }
  return "unknown";
}

bool ItemEntry::passed(ItemCheck c) const noexcept { return std::find(checks.begin(), checks.end(), c) != checks.end(); }

const FieldLayout* ClassLayout::field(std::string_view name) const noexcept {
  for (const FieldLayout& f : fields) {
    if (f.name == name) return &f;
  }
  return nullptr;
}

std::optional<std::string_view> EnumTable::name_of(std::int64_t raw) const noexcept {
  const auto it =
      std::lower_bound(values.begin(), values.end(), raw,
                       [](const std::pair<std::int64_t, std::string>& v, std::int64_t r) { return v.first < r; });
  if (it == values.end() || it->first != raw) return std::nullopt;
  return std::string_view(it->second);
}

const ClassLayout* Layout::find_class(std::string_view name) const noexcept {
  for (const ClassLayout& c : classes_) {
    if (c.name == name) return &c;
  }
  return nullptr;
}

const EnumTable* Layout::find_enum(std::string_view name) const noexcept {
  for (const EnumTable& e : enums_) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

const ParamLayout* Layout::find_param(std::string_view name) const noexcept {
  for (const ParamLayout& p : params_) {
    if (p.name == name) return &p;
  }
  return nullptr;
}

const ItemEntry* Layout::find_item(std::string_view label) const noexcept {
  for (const ItemEntry& i : items_) {
    if (i.label == label) return &i;
  }
  return nullptr;
}

const ItemEntry* Layout::find_item(std::uint32_t id) const noexcept {
  for (const ItemEntry& i : items_) {
    if (i.id == id) return &i;
  }
  return nullptr;
}

bool Layout::has_leads() const noexcept {
  const auto lead = [](LayoutStatus s) { return s == LayoutStatus::Lead; };
  if (task_ && lead(task_->status)) return true;
  if (fmg_ && lead(fmg_->status)) return true;
  if (dlrf_ && lead(dlrf_->status)) return true;
  return std::any_of(classes_.begin(), classes_.end(), [&](const ClassLayout& c) { return lead(c.status); }) ||
         std::any_of(params_.begin(), params_.end(), [&](const ParamLayout& p) { return lead(p.status); }) ||
         std::any_of(items_.begin(), items_.end(), [&](const ItemEntry& i) { return lead(i.status); });
}

std::uint32_t Layout::max_copy_size() const noexcept {
  std::uint32_t most = 0;
  for (const ClassLayout& c : classes_) {
    most = std::max({most, c.copy_size.value_or(0) + c.far_bytes, c.stride.value_or(0) + c.far_bytes});
  }
  for (const ParamLayout& p : params_) most = std::max(most, p.row_size);
  return most;
}

namespace {

using toml_read::Errors;
using toml_read::Table;

constexpr std::array<std::string_view, 12> kGroupNames = {"session", "world", "combat", "player", "flags",  "text",
                                                          "ui",      "place", "camera", "params", "status", "boot"};
static_assert(kGroupNames.size() == proto::kGroupCount + 1, "proto::Group's names, in its order, then boot");

// A type's name, and its size where the name alone fixes it (0 for utf16[n], bits and array).
struct TypeName {
  std::string_view name;
  FieldType type;
  std::uint32_t size;
};
constexpr std::array<TypeName, 16> kTypes = {{{"u8", FieldType::U8, 1},
                                              {"u16", FieldType::U16, 2},
                                              {"u32", FieldType::U32, 4},
                                              {"u64", FieldType::U64, 8},
                                              {"i32", FieldType::I32, 4},
                                              {"f32", FieldType::F32, 4},
                                              {"bool8", FieldType::Bool8, 1},
                                              {"bit", FieldType::Bit, 1},
                                              {"bits", FieldType::Bits, 0},
                                              {"ptr", FieldType::Ptr, 8},
                                              {"handle", FieldType::Handle, 8},
                                              {"vec3f", FieldType::Vec3f, 12},
                                              {"mat4f", FieldType::Mat4f, 64},
                                              {"dlvector", FieldType::DlVector, 16},
                                              {"dlmap", FieldType::DlMap, 16},
                                              {"array", FieldType::Array, 0}}};

// The whole numbers a type can hold, for its range; nothing for a type that holds none.
std::optional<std::pair<std::int64_t, std::int64_t>> domain_of(FieldType type) {
  switch (type) {
    case FieldType::U8: return std::pair<std::int64_t, std::int64_t>{0, 0xFF};
    case FieldType::U16: return std::pair<std::int64_t, std::int64_t>{0, 0xFFFF};
    case FieldType::U32: return std::pair<std::int64_t, std::int64_t>{0, 0xFFFFFFFFLL};
    case FieldType::U64: return std::pair<std::int64_t, std::int64_t>{0, std::numeric_limits<std::int64_t>::max()};
    case FieldType::I32:
      return std::pair<std::int64_t, std::int64_t>{std::numeric_limits<std::int32_t>::min(),
                                                   std::numeric_limits<std::int32_t>::max()};
    case FieldType::Bool8: return std::pair<std::int64_t, std::int64_t>{0, 1};
    default: return std::nullopt;
  }
}

std::optional<LayoutStatus> read_status(Table& t, Errors& errors) {
  const auto status = t.text("status", false);
  if (!status) return LayoutStatus::Lead;  // an entry that does not say is a lead: a release build refuses it
  if (*status == "lead") return LayoutStatus::Lead;
  if (*status == "confirmed") return LayoutStatus::Confirmed;
  errors.add(t.where("status"), "must be one of lead, confirmed");
  return std::nullopt;
}

// One field: { off, ty, far?, group?, range?, bit?, word?, shift?, width?, to?, enum?, elem?, len?, stride? }. `param`
// fields take off and ty only. A far field's off may lie anywhere below kMaxFarOffset.
std::optional<FieldLayout> read_field(const toml::table& t, std::string name, const std::string& path, Errors& errors,
                                      bool param) {
  Table f(t, path, errors);
  FieldLayout out;
  out.name = std::move(name);
  bool ok = true;
  if (!param) {
    if (const toml::node* flag = f.node("far", false)) {
      if (flag->is_boolean()) {
        out.is_far = flag->value<bool>().value_or(false);
      } else {
        errors.add(f.where("far"), "must be true or false");
      }
    }
  }
  if (const auto off = f.integer("off", true, 0, (out.is_far ? kMaxFarOffset : kMaxCopySize) - 1)) {
    out.off = static_cast<std::uint32_t>(*off);
  } else {
    ok = false;
  }
  const auto ty = f.text("ty", true);
  if (!ty) {
    ok = false;
  } else if (ty->starts_with("utf16[") && ty->ends_with("]")) {
    const std::string_view digits = std::string_view(*ty).substr(6, ty->size() - 7);
    std::uint32_t n = 0;
    const auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), n);
    if (ec != std::errc{} || end != digits.data() + digits.size() || n == 0 || n > 1024) {
      errors.add(f.where("ty"), "must hold utf16[n] with n from 1 to 1024");
      ok = false;
    } else {
      out.type = FieldType::Utf16;
      out.len = n;
      out.size = 2 * n;
    }
  } else {
    const auto known = std::find_if(kTypes.begin(), kTypes.end(), [&](const TypeName& k) { return k.name == *ty; });
    if (known == kTypes.end()) {
      errors.add(f.where("ty"),
                 "must be one of u8, u16, u32, u64, i32, f32, bool8, bit, bits, ptr, handle, vec3f, mat4f, utf16[n], "
                 "dlvector, dlmap, array");
      ok = false;
    } else {
      out.type = known->type;
      out.size = known->size;
    }
  }
  if (param && ok && (out.type == FieldType::Array || out.type == FieldType::Bit || out.type == FieldType::Bits)) {
    errors.add(f.where("ty"), "is not a type a param row's field may have");
    ok = false;
  }

  if (!param) {
    if (const auto group = f.text("group", false)) {
      std::optional<std::size_t> index;
      for (std::size_t i = 0; i < kGroupNames.size(); ++i) {
        if (kGroupNames[i] == *group) index = i;
      }
      if (!index) {
        errors.add(f.where("group"),
                   "must be a read group: session, world, combat, player, flags, text, ui, place, "
                   "camera, params, status, or boot");
      } else if (*index == proto::kGroupCount) {
        out.boot = true;
      } else {
        out.group = static_cast<proto::Group>(*index);  // kGroupNames follows proto::Group's order, then boot
      }
    }
    // A run of bits: the word's size in bytes (1, 2, 4 or 8), the run's lowest bit and its length, all within the word.
    const bool bits = ok && out.type == FieldType::Bits;
    const auto word = f.integer("word", bits, 1, 8);
    const auto shift = f.integer("shift", bits, 0, 63);
    const auto width = f.integer("width", bits, 1, 32);
    if ((word || shift || width) && ok && !bits) errors.add(path, "has word, shift or width, which only bits fields take");
    std::optional<std::pair<std::int64_t, std::int64_t>> bits_domain;
    if (bits) {
      if (!word || !shift || !width) {
        ok = false;
      } else if (*word != 1 && *word != 2 && *word != 4 && *word != 8) {
        errors.add(f.where("word"), "must be 1, 2, 4 or 8");
        ok = false;
      } else if (*shift + *width > 8 * *word) {
        errors.add(f.where("width"), "runs past the word");
        ok = false;
      } else {
        out.size = static_cast<std::uint32_t>(*word);
        out.shift = static_cast<std::uint8_t>(*shift);
        out.width = static_cast<std::uint8_t>(*width);
        bits_domain = std::pair<std::int64_t, std::int64_t>{0, (std::int64_t{1} << *width) - 1};
      }
    }
    if (const toml::node* range = f.node("range", false)) {
      const auto domain = !ok ? std::nullopt : bits_domain ? bits_domain : domain_of(out.type);
      if (ok && !domain) {
        errors.add(f.where("range"), "applies only to whole-number fields");
      } else if (domain) {
        out.range = toml_read::range_of(*range, f.where("range"), errors, domain->first, domain->second);
      }
    }
    const auto bit = f.integer("bit", ok && out.type == FieldType::Bit, 0, 7);
    if (bit) {
      if (ok && out.type != FieldType::Bit) errors.add(f.where("bit"), "applies only to bit fields");
      out.bit = static_cast<std::uint8_t>(*bit);
    }
    if (const auto to = f.text("to", false)) {
      if (ok && out.type != FieldType::Ptr) errors.add(f.where("to"), "applies only to ptr fields");
      out.to = *to;
    }
    if (const auto table = f.text("enum", false)) {
      if (ok && !domain_of(out.type) && !bits_domain) errors.add(f.where("enum"), "applies only to whole-number fields");
      out.enum_table = *table;
    }
    const bool array = ok && out.type == FieldType::Array;
    const auto elem = f.text("elem", array);
    const auto len = f.integer("len", array, 1, 256);
    const auto stride = f.integer("stride", array, 1, kMaxCopySize);
    if ((elem || len || stride) && ok && !array) errors.add(path, "has elem, len or stride, which only arrays take");
    if (array) {
      if (elem) out.elem = *elem;
      if (len && stride) {
        out.len = static_cast<std::uint32_t>(*len);
        out.stride = static_cast<std::uint32_t>(*stride);
        out.size = out.len * out.stride;  // at most 256 × 0x4000
      } else {
        ok = false;
      }
    }
  }
  f.done();
  if (!ok) return std::nullopt;
  return out;
}

// The fields of a class or a param row, each within `bound` bytes.
std::vector<FieldLayout> read_fields(const toml::table& t, const std::string& path, Errors& errors,
                                     std::optional<std::uint32_t> bound, bool param) {
  std::vector<FieldLayout> fields;
  if (t.size() > 256) errors.add(path, "has more than 256 fields");
  for (auto&& [key, node] : t) {
    const std::string at = path + "." + std::string(key.str());
    if (!toml_read::is_identifier(key.str(), 64)) {
      errors.add(path, "has a field whose name is not 1 to 64 letters, digits and _");
      continue;
    }
    const toml::table* ft = node.as_table();
    if (ft == nullptr) {
      errors.add(at, "must be a table");
      continue;
    }
    std::optional<FieldLayout> field = read_field(*ft, std::string(key.str()), at, errors, param);
    if (!field) continue;
    if (field->is_far) {
      // Read by a copy of its own: a type of a few bytes, anywhere in the first MiB of its object.
      const bool small = field->type != FieldType::Utf16 && field->type != FieldType::Mat4f &&
                         field->type != FieldType::Array && field->type != FieldType::DlMap &&
                         field->size != 0 && field->size <= kMaxFarBytes;
      if (!small) {
        errors.add(at + ".far",
                   "applies only to the whole numbers, f32, bool8, bit, bits, ptr, handle, vec3f and dlvector");
        continue;
      }
      if (std::uint64_t{field->off} + field->size > kMaxFarOffset) {
        errors.add(at, "runs past the first MiB of its object");
      }
    } else if (bound && std::uint64_t{field->off} + field->size > *bound) {
      errors.add(at, "runs past the class's copy size");
    }
    fields.push_back(std::move(*field));
  }
  return fields;
}

void read_class(const toml::table& t, std::string name, const std::string& path, Errors& errors, ClassLayout& out) {
  Table c(t, path, errors);
  out.name = std::move(name);
  if (const auto rtti = c.text("rtti", false)) out.rtti = *rtti;
  if (const auto singleton = c.text("singleton", false)) {
    if (!toml_read::is_identifier(*singleton, 128)) {
      errors.add(c.where("singleton"), "must be 1 to 128 letters, digits and _");
    }
    out.singleton = *singleton;
  }
  if (const auto size = c.integer("copy_size", false, 0, kMaxCopySize))
    out.copy_size = static_cast<std::uint32_t>(*size);
  if (const auto stride = c.integer("stride", false, 1, kMaxCopySize)) out.stride = static_cast<std::uint32_t>(*stride);
  if (!c.has("copy_size") && !c.has("stride")) errors.add(path, "needs a copy_size or a stride");
  if (out.copy_size && out.stride && *out.copy_size > *out.stride) {
    errors.add(c.where("copy_size"), "is larger than the stride");
  }
  out.status = read_status(c, errors).value_or(LayoutStatus::Lead);
  const std::optional<std::uint32_t> bound = out.copy_size ? out.copy_size : out.stride;
  if (const toml::table* fields = c.table("fields", false)) {
    out.fields = read_fields(*fields, c.where("fields"), errors, bound, false);
  }
  // The far fields' places in the reader's buffer: past the copy, each at a multiple of eight. They need a class
  // that is copied by its size (not an element of a run), and a copy of nothing only where every field is far and
  // no class is checked (the check reads the object's first word).
  const bool any_far = std::any_of(out.fields.begin(), out.fields.end(), [](const FieldLayout& f) { return f.is_far; });
  const bool all_far = !out.fields.empty() &&
                       std::all_of(out.fields.begin(), out.fields.end(), [](const FieldLayout& f) { return f.is_far; });
  if (any_far && !out.copy_size) {
    errors.add(path, "has far fields, which need a copy_size (0 where every field is far)");
  }
  if (out.copy_size && *out.copy_size == 0) {
    if (!all_far) errors.add(c.where("copy_size"), "may be 0 only where every field is far");
    if (!out.rtti.empty()) {
      errors.add(c.where("copy_size"), "must hold the object's first 8 bytes where the class has an rtti");
    }
  }
  std::uint32_t next = (out.copy_size.value_or(0) + 7u) & ~7u;
  for (FieldLayout& f : out.fields) {
    if (!f.is_far) continue;
    f.far_at = next;
    next += (f.size + 7u) & ~7u;
  }
  out.far_bytes = next - out.copy_size.value_or(0);
  if (!any_far) out.far_bytes = 0;
  c.done();
}

void read_enum(const toml::table& t, std::string name, const std::string& path, Errors& errors, EnumTable& out) {
  Table e(t, path, errors);
  out.name = std::move(name);
  if (const toml::table* values = e.table("values", true)) {
    if (values->empty() || values->size() > 4096) errors.add(e.where("values"), "must map 1 to 4096 raw values");
    for (auto&& [key, node] : *values) {
      const std::string_view k = key.str();
      std::int64_t raw = 0;
      const auto [end, ec] = std::from_chars(k.data(), k.data() + k.size(), raw);
      if (ec != std::errc{} || end != k.data() + k.size()) {
        errors.add(e.where("values"), "has a key that is not a whole number");
        continue;
      }
      const auto* s = node.as_string();
      if (s == nullptr || !toml_read::is_identifier(s->get(), 64)) {
        errors.add(e.where("values") + "." + std::string(k), "must be an enum value's name");
        continue;
      }
      out.values.emplace_back(raw, s->get());
    }
    std::sort(out.values.begin(), out.values.end());
    for (std::size_t i = 1; i < out.values.size(); ++i) {
      if (out.values[i].first == out.values[i - 1].first) errors.add(e.where("values"), "maps one raw value twice");
    }
  }
  e.done();
}

void read_param(const toml::table& t, std::string name, const std::string& path, Errors& errors, ParamLayout& out) {
  Table p(t, path, errors);
  out.name = std::move(name);
  if (const auto slot = p.integer("slot", true, 0, 1023)) out.slot = static_cast<std::uint32_t>(*slot);
  const auto row = p.integer("row_size", true, 1, kMaxCopySize);
  if (row) out.row_size = static_cast<std::uint32_t>(*row);
  out.status = read_status(p, errors).value_or(LayoutStatus::Lead);
  if (const toml::table* fields = p.table("fields", true)) {
    out.fields = read_fields(*fields, p.where("fields"), errors,
                             row ? std::optional<std::uint32_t>(out.row_size) : std::nullopt, true);
  }
  p.done();
}

// [task]. A confirmed one holds every key. A lead may hold only its group (the derivation's offline round gives the
// group's index, and the in-game round R1 finds the rest): it is `partial` then, and discovery never registers with it.
TaskLayout read_task(const toml::table& t, Errors& errors) {
  Table k(t, "task", errors);
  TaskLayout out;
  const toml::node* status = t.get("status");
  const std::optional<std::string_view> stated = status != nullptr ? status->value<std::string_view>() : std::nullopt;
  const bool lead = !stated || *stated != "confirmed";
  const bool full = !lead || k.has("register_abi") || k.has("vtable_slots") || k.has("run_slot") || k.has("object_size");
  out.partial = !full;
  if (const toml::table* group = k.table("group", true)) {
    Table g(*group, k.where("group"), errors);
    if (const auto name = g.text("name", true)) {
      if (!toml_read::is_identifier(*name, 64)) errors.add(g.where("name"), "must be 1 to 64 letters, digits and _");
      out.group_name = *name;
    }
    if (const auto index = g.integer("index", true, 0, 4095)) out.group_index = static_cast<std::uint32_t>(*index);
    g.done();
  }
  if (const toml::table* abi = k.table("register_abi", full)) {
    Table a(*abi, k.where("register_abi"), errors);
    const auto reg = [&](std::string_view key, ArgRegister& to) {
      const auto name = a.text(key, true);
      if (!name) return false;
      for (const ArgRegister r : {ArgRegister::Rcx, ArgRegister::Rdx, ArgRegister::R8, ArgRegister::R9}) {
        if (*name == to_string(r)) {
          to = r;
          return true;
        }
      }
      errors.add(a.where(key), "must be one of rcx, rdx, r8, r9");
      return false;
    };
    const bool t = reg("task", out.register_abi.task);
    const bool i = reg("instance", out.register_abi.instance);
    const bool g = reg("group", out.register_abi.group);
    const RegisterAbi& r = out.register_abi;
    if (t && i && g && (r.task == r.instance || r.task == r.group || r.instance == r.group)) {
      errors.add(k.where("register_abi"), "must name three different registers");
    }
    a.done();
  }
  const auto slots = k.integer("vtable_slots", full, 1, 64);
  if (slots) out.vtable_slots = static_cast<std::uint32_t>(*slots);
  if (const auto run = k.integer("run_slot", full, 0, 63)) {
    out.run_slot = static_cast<std::uint32_t>(*run);
    if (slots && out.run_slot >= out.vtable_slots) errors.add(k.where("run_slot"), "must be below vtable_slots");
  }
  if (const auto size = k.integer("object_size", full, 0, kMaxCopySize))
    out.object_size = static_cast<std::uint32_t>(*size);
  out.status = read_status(k, errors).value_or(LayoutStatus::Lead);
  k.done();
  return out;
}

std::vector<std::uint32_t> read_ids(Table& t, std::string_view key, Errors& errors) {
  std::vector<std::uint32_t> out;
  const toml::array* ids = t.array(key, false);
  if (ids == nullptr) return out;
  if (ids->size() > 64) errors.add(t.where(key), "has more than 64 ids");
  for (std::size_t i = 0; i < ids->size() && i < 64; ++i) {
    if (const auto id =
            toml_read::integer_of(*ids->get(i), toml_read::indexed(t.where(key), i), errors, 0, 0xFFFFFFFFLL)) {
      out.push_back(static_cast<std::uint32_t>(*id));
    }
  }
  return out;
}

// ---- The build's table (schema 2) ----

template <class E, std::size_t N>
using Words = std::array<std::pair<std::string_view, E>, N>;

constexpr Words<Recipe, 6> kRecipes = {{{"accessor", Recipe::Accessor},
                                        {"class", Recipe::Class},
                                        {"instance_class", Recipe::InstanceClass},
                                        {"table", Recipe::Table},
                                        {"relation", Recipe::Relation},
                                        {"invariant", Recipe::Invariant}}};
constexpr Words<ItemCheck, 8> kChecks = {{{"placement", ItemCheck::Placement},
                                          {"agree", ItemCheck::Agree},
                                          {"witness", ItemCheck::Witness},
                                          {"digest", ItemCheck::Digest},
                                          {"class", ItemCheck::Class},
                                          {"self_test", ItemCheck::SelfTest},
                                          {"second_source", ItemCheck::SecondSource},
                                          {"verdict", ItemCheck::Verdict}}};
constexpr Words<Verdict, 4> kVerdicts = {
    {{"adds", Verdict::Adds}, {"removes", Verdict::Removes}, {"reads", Verdict::Reads}, {"unknown", Verdict::Unknown}}};
constexpr Words<IdKind, kIdKindCount> kItemKinds = {
    {{"static", IdKind::Static}, {"function", IdKind::Function}, {"class", IdKind::Class}, {"table", IdKind::Table}}};

template <class E, std::size_t N>
std::optional<E> word_of(const Words<E, N>& words, std::string_view text) noexcept {
  for (const auto& [name, value] : words) {
    if (name == text) return value;
  }
  return std::nullopt;
}

// Which kinds a recipe finds (proposal 0005 §6.1).
bool recipe_finds(Recipe r, IdKind kind) noexcept {
  switch (r) {
    case Recipe::Accessor:
    case Recipe::InstanceClass:
    case Recipe::Invariant: return kind == IdKind::Static;
    case Recipe::Class: return kind == IdKind::Class;
    case Recipe::Table: return kind == IdKind::Table;
    case Recipe::Relation: return kind == IdKind::Function || kind == IdKind::Static;
  }
  return false;
}

// A list of distinct words of one of the closed sets above, 0 to `max` of them.
template <class E, std::size_t N>
std::vector<E> read_words(Table& t, std::string_view key, bool required, const Words<E, N>& words, std::size_t max,
                          const char* problem, Errors& errors) {
  std::vector<E> out;
  const toml::array* list = t.array(key, required);
  if (list == nullptr) return out;
  if (list->size() > max) errors.add(t.where(key), "has more than " + std::to_string(max) + " items");
  for (std::size_t i = 0; i < list->size() && i < max; ++i) {
    const toml::node* node = list->get(i);
    const auto* text = node != nullptr ? node->as_string() : nullptr;
    const std::optional<E> value = text != nullptr ? word_of(words, text->get()) : std::nullopt;
    if (!value) {
      errors.add(toml_read::indexed(t.where(key), i), problem);
    } else if (std::find(out.begin(), out.end(), *value) != out.end()) {
      errors.add(toml_read::indexed(t.where(key), i), "is listed twice");
    } else {
      out.push_back(*value);
    }
  }
  return out;
}

// A list of offsets from the start of the image, each above 0 and below `image_size`, none twice; `ascending` wants
// them in order too.
std::vector<std::uint32_t> read_rvas(Table& t, std::string_view key, bool required, std::size_t max,
                                     std::uint32_t image_size, bool ascending, Errors& errors) {
  std::vector<std::uint32_t> out;
  const toml::array* list = t.array(key, required);
  if (list == nullptr) return out;
  if (list->size() > max) errors.add(t.where(key), "has more than " + std::to_string(max) + " offsets");
  for (std::size_t i = 0; i < list->size() && i < max; ++i) {
    const toml::node* node = list->get(i);
    const std::string path = toml_read::indexed(t.where(key), i);
    if (node == nullptr) continue;
    const auto rva = toml_read::integer_of(*node, path, errors, 1, 0xFFFFFFFFLL);
    if (!rva) continue;
    const auto value = static_cast<std::uint32_t>(*rva);
    if (value >= image_size) {
      errors.add(path, "lies outside the build's image");
    } else if (std::find(out.begin(), out.end(), value) != out.end()) {
      errors.add(path, "is listed twice");
    } else if (ascending && !out.empty() && value < out.back()) {
      errors.add(path, "must be above the offset before it");
    } else {
      out.push_back(value);
    }
  }
  return out;
}

bool lower_hex(std::string_view s, std::size_t digits) noexcept {
  return s.size() == digits &&
         std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

// One [item.<label>]: { id, kind, rva | vtables, length?, stride?, count?, found_by, witnesses?, digest?, verdict?,
// checks?, status? }. Numbers, our labels and fixed words only: no key of it can carry a byte of the game.
void read_item(const toml::table& t, std::string label, const std::string& path, std::uint32_t image_size,
               Errors& errors, ItemEntry& out) {
  Table i(t, path, errors);
  out.label = std::move(label);
  std::optional<IdKind> kind;
  if (const auto text = i.text("kind", true)) {
    kind = word_of(kItemKinds, *text);
    if (!kind) errors.add(i.where("kind"), "must be one of static, function, class, table");
  }
  if (kind) out.kind = *kind;
  if (const auto id = i.integer("id", true, 1000, 4999)) {
    out.id = static_cast<std::uint32_t>(*id);
    if (kind && kind_of_number(out.id) != kind) errors.add(i.where("id"), "lies outside its kind's block");
  }
  const bool is_class = kind == IdKind::Class;
  // Where it is: a class has its tables of methods, everything else one offset.
  if (kind && is_class) {
    out.vtables = read_rvas(i, "vtables", true, kMaxVtables, image_size, true, errors);
    if (i.has("vtables") && out.vtables.empty()) errors.add(i.where("vtables"), "must list 1 to 64 offsets");
  } else if (kind) {
    if (const auto rva = i.integer("rva", true, 1, 0xFFFFFFFFLL)) {
      out.rva = static_cast<std::uint32_t>(*rva);
      if (out.rva >= image_size) {
        errors.add(i.where("rva"), "lies outside the build's image");
      } else if (kind == IdKind::Static && out.rva % 8 != 0) {
        errors.add(i.where("rva"), "must be a multiple of 8 for a static");
      }
    }
  }
  if (kind == IdKind::Function) {
    if (const auto length = i.integer("length", true, 1, kMaxFunctionLength)) {
      out.length = static_cast<std::uint32_t>(*length);
      if (out.rva < image_size && out.length > image_size - out.rva) {
        errors.add(i.where("length"), "runs past the build's image");
      }
    }
    if (const auto digest = i.text("digest", false)) {
      if (!lower_hex(*digest, kDigestHexDigits)) {
        errors.add(i.where("digest"), "must be 32 lower-case hex digits");
      } else {
        out.digest = *digest;
      }
    }
    if (const auto verdict = i.text("verdict", false)) {
      out.verdict = word_of(kVerdicts, *verdict);
      if (!out.verdict) errors.add(i.where("verdict"), "must be one of adds, removes, reads, unknown");
    }
  }
  if (kind == IdKind::Table) {
    if (const auto stride = i.integer("stride", true, 8, 64)) out.stride = static_cast<std::uint32_t>(*stride);
    if (const auto count = i.integer("count", true, 1, 65536)) out.count = static_cast<std::uint32_t>(*count);
    if (out.rva < image_size && static_cast<std::uint64_t>(out.stride) * out.count > image_size - out.rva) {
      errors.add(i.where("count"), "runs past the build's image");
    }
  }
  out.found_by = read_words(i, "found_by", true, kRecipes, kRecipes.size(),
                            "must be one of accessor, class, instance_class, table, relation, invariant", errors);
  if (i.has("found_by") && out.found_by.empty()) errors.add(i.where("found_by"), "must name at least one recipe");
  for (const Recipe r : out.found_by) {
    if (kind && !recipe_finds(r, *kind)) {
      errors.add(i.where("found_by"), std::string("names ") + to_string(r) + ", which finds no " + to_string(*kind));
    }
  }
  if (kind && !is_class) {
    out.witnesses = read_rvas(i, "witnesses", false, kMaxWitnesses, image_size, false, errors);
  }
  out.checks = read_words(i, "checks", false, kChecks, kChecks.size(),
                          "must be one of placement, agree, witness, digest, class, self_test, second_source, verdict",
                          errors);
  if (const auto status = read_status(i, errors)) out.status = *status;
  i.done();

  // A check an entry claims must have what it checks; one recipe alone never confirms an id (proposal 0005 §7.1).
  if (out.passed(ItemCheck::Witness) && out.witnesses.empty()) {
    errors.add(path + ".checks", "names witness, but the entry has no witnesses");
  }
  if (out.passed(ItemCheck::Digest) && out.digest.empty()) {
    errors.add(path + ".checks", "names digest, but the entry has no digest");
  }
  if (out.passed(ItemCheck::Verdict) && !out.verdict) {
    errors.add(path + ".checks", "names verdict, but the entry has no verdict");
  }
  if (out.status == LayoutStatus::Confirmed && kind) {
    const auto lacks = [&](ItemCheck c) {
      if (!out.passed(c)) errors.add(path + ".status", std::string("is confirmed without the check ") + to_string(c));
    };
    lacks(ItemCheck::Placement);
    lacks(ItemCheck::Agree);
    if (is_class) {
      lacks(ItemCheck::Class);
    } else {
      lacks(ItemCheck::Witness);
    }
    if (kind == IdKind::Function) lacks(ItemCheck::Digest);
  }
}

// [provenance]: { method, tool, tool_sha256, round, date, leads? }.
TableProvenance read_provenance(const toml::table& t, Errors& errors) {
  Table p(t, "provenance", errors);
  TableProvenance out;
  if (const auto method = p.text("method", true)) {
    if (*method != "own-derivation") errors.add(p.where("method"), "must be own-derivation");  // O4, O48
  }
  const auto version_form = [](std::string_view s) {
    return !s.empty() && s.size() <= 48 && std::all_of(s.begin(), s.end(), [](char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
             c == '+' || c == '-';
    });
  };
  if (const auto tool = p.text("tool", true)) {
    if (!version_form(*tool)) {
      errors.add(p.where("tool"), "must be a version label of 1 to 48 letters, digits and . _ + -");
    } else {
      out.tool = *tool;
    }
  }
  if (const auto sha = p.text("tool_sha256", true)) {
    if (!lower_hex(*sha, 64)) {
      errors.add(p.where("tool_sha256"), "must be 64 lower-case hex digits");
    } else {
      out.tool_sha256 = *sha;
    }
  }
  if (const auto round = p.text("round", true)) {
    if (!is_id_label(*round)) {
      errors.add(p.where("round"), "must be a label of ours: lower-case letters, digits and _");
    } else {
      out.round = *round;
    }
  }
  if (const auto date = p.text("date", true)) {
    if (!toml_read::is_date(*date)) {
      errors.add(p.where("date"), "must be a date, YYYY-MM-DD");
    } else {
      out.date = *date;
    }
  }
  if (const toml::array* leads = p.array("leads", false)) {
    if (auto list = toml_read::strings_of(*leads, p.where("leads"), errors, 16, 64)) out.leads = std::move(*list);
  }
  p.done();
  return out;
}

// Each enum table total over the range of every field that uses it.
void check_enum_use(const std::vector<FieldLayout>& fields, const std::string& path,
                    const std::vector<EnumTable>& enums, Errors& errors) {
  for (const FieldLayout& f : fields) {
    if (f.enum_table.empty()) continue;
    const auto table =
        std::find_if(enums.begin(), enums.end(), [&](const EnumTable& e) { return e.name == f.enum_table; });
    if (table == enums.end()) {
      errors.add(path + "." + f.name + ".enum", "names no enum table of the file");
      continue;
    }
    if (!f.range) continue;
    if (f.range->second - f.range->first >= 4096) {
      errors.add(path + "." + f.name + ".range", "is wider than 4096 values, too wide for an enum");
      continue;
    }
    for (std::int64_t raw = f.range->first; raw <= f.range->second; ++raw) {
      if (!table->name_of(raw)) {
        errors.add(path + "." + f.name, "has a range its enum table does not cover");
        break;
      }
    }
  }
}

}  // namespace

Expected<Layout, SchemaErrors> load_layout(const BuildProfile& profile, std::string_view text, const LayoutNeeds& needs,
                                           LayoutMode mode, std::string_view file) {
  return Layout::read(profile.entry(), profile.key(), text, needs, mode, file);
}

SchemaErrors check_layout(const BuildEntry& entry, std::string_view text, const LayoutNeeds& needs, LayoutMode mode,
                          std::string_view file) {
  auto layout = Layout::read(entry, proto::BuildKey{}, text, needs, mode, file);
  return layout ? SchemaErrors{} : std::move(layout.error());
}

Expected<Layout, SchemaErrors> Layout::read(const BuildEntry& entry, proto::BuildKey build_key,
                                            std::string_view text, const LayoutNeeds& needs, LayoutMode mode,
                                            std::string_view file) {
  const std::string_view build_text = entry.key;
  Errors errors{std::string(file)};
  const std::optional<toml::table> doc = toml_read::parse(text, errors);
  if (!doc) return Unexpected{errors.take()};
  Table root(*doc, "", errors);
  Layout layout;
  layout.build_key_ = build_key;
  if (const auto schema = root.integer("schema", true, 1, 2)) layout.schema_ = static_cast<std::uint32_t>(*schema);
  if (const auto build = root.text("build", true)) {
    if (*build != build_text) errors.add("build", "is not the build the gate matched");  // R7
    layout.build_ = *build;
  }
  if (const auto from = root.text("derived_from", true)) {
    if (*from != "probe" && !toml_read::is_build_key(*from)) {
      errors.add("derived_from", "must be probe, or the key of the build it came from");
    }
    layout.derived_from_ = *from;
  }
  if (const auto record = root.text("probe_record", true)) {
    if (!toml_read::is_relative_path(*record)) errors.add("probe_record", "must be a relative path");
    layout.probe_record_ = *record;
  }
  if (const toml::table* task = root.table("task", needs.task)) layout.task_ = read_task(*task, errors);
  if (const toml::table* classes = root.table("class", false)) {
    if (classes->size() > 256) errors.add("class", "has more than 256 classes");
    for (auto&& [key, node] : *classes) {
      const std::string path = "class." + std::string(key.str());
      const toml::table* t = node.as_table();
      if (!toml_read::is_identifier(key.str(), 64) || t == nullptr) {
        errors.add("class", "has an entry that is not a table named with letters, digits and _");
        continue;
      }
      ClassLayout c;
      read_class(*t, std::string(key.str()), path, errors, c);
      layout.classes_.push_back(std::move(c));
    }
  }
  if (const toml::table* enums = root.table("enum", false)) {
    for (auto&& [key, node] : *enums) {
      const toml::table* t = node.as_table();
      if (!toml_read::is_identifier(key.str(), 64) || t == nullptr) {
        errors.add("enum", "has an entry that is not a table named with letters, digits and _");
        continue;
      }
      EnumTable e;
      read_enum(*t, std::string(key.str()), "enum." + std::string(key.str()), errors, e);
      layout.enums_.push_back(std::move(e));
    }
  }
  if (const toml::table* params = root.table("param", false)) {
    for (auto&& [key, node] : *params) {
      const toml::table* t = node.as_table();
      if (!toml_read::is_identifier(key.str(), 64) || t == nullptr) {
        errors.add("param", "has an entry that is not a table named with letters, digits and _");
        continue;
      }
      ParamLayout p;
      read_param(*t, std::string(key.str()), "param." + std::string(key.str()), errors, p);
      layout.params_.push_back(std::move(p));
    }
  }
  if (const toml::table* fmg = root.table("fmg", false)) {
    Table f(*fmg, "fmg", errors);
    FmgLayout out;
    if (const auto indexing = f.text("indexing", true)) {
      if (*indexing != "flat") errors.add(f.where("indexing"), "must be flat");
      out.indexing = *indexing;
    }
    if (const auto stride = f.integer("group_stride", true, 1, 256))
      out.group_stride = static_cast<std::uint32_t>(*stride);
    out.npc_names = read_ids(f, "npc_names", errors);
    out.place_names = read_ids(f, "place_names", errors);
    if (out.npc_names.size() > 8) errors.add(f.where("npc_names"), "has more than 8 tables");
    if (out.place_names.size() > 8) errors.add(f.where("place_names"), "has more than 8 tables");
    out.status = read_status(f, errors).value_or(LayoutStatus::Lead);
    f.done();
    layout.fmg_ = std::move(out);
  }
  if (const toml::table* dlrf = root.table("dlrf", false)) {
    Table d(*dlrf, "dlrf", errors);
    DlrfLayout out;
    if (const auto off = d.integer("name_ptr_off", true, 0, 0x1000))
      out.name_ptr_off = static_cast<std::uint32_t>(*off);
    out.status = read_status(d, errors).value_or(LayoutStatus::Lead);
    d.done();
    layout.dlrf_ = out;
  }
  if (const toml::table* selftest = root.table("selftest", false)) {
    Table s(*selftest, "selftest", errors);
    layout.selftest_.npc_name_ids = read_ids(s, "npc_name_ids", errors);
    layout.selftest_.place_name_ids = read_ids(s, "place_name_ids", errors);
    layout.selftest_.npc_param_ids = read_ids(s, "npc_param_ids", errors);
    s.done();
  }
  // The build's table, since schema 2 (proposal 0005 §5): in a schema 1 file both keys are unknown, and refused so.
  if (layout.schema_ >= 2) {
    if (const toml::table* provenance = root.table("provenance", true)) {
      layout.provenance_ = read_provenance(*provenance, errors);
    }
    if (const toml::table* items = root.table("item", false)) {
      if (items->size() > kMaxIds) errors.add("item", "has more than 4000 items");
      for (auto&& [key, node] : *items) {
        const toml::table* t = node.as_table();
        if (!is_id_label(key.str()) || t == nullptr) {
          errors.add("item", "has an entry that is not a table named with a label of ours");
          continue;
        }
        const std::string path = "item." + std::string(key.str());
        ItemEntry item;
        read_item(*t, std::string(key.str()), path, entry.size_of_image, errors, item);
        if (item.id != 0 && layout.find_item(item.id) != nullptr) errors.add(path + ".id", "is another item's id");
        layout.items_.push_back(std::move(item));
      }
    }
  }
  root.done();

  // The table against the catalogue: every item is an id of gal/ids.toml, under its label, number and kind.
  if (!layout.items_.empty() && needs.ids == nullptr) {
    errors.add("item", "needs the catalogue, gal/ids.toml, which was not given");
  }
  for (const ItemEntry& item : layout.items_) {
    if (needs.ids == nullptr) break;
    const std::string path = "item." + item.label;
    const IdEntry* id = needs.ids->find(item.label);
    if (id == nullptr) {
      errors.add(path, "is no label of the catalogue");
    } else if (id->number != item.id) {
      errors.add(path + ".id", "is not the catalogue's number for this label");
    } else if (id->kind != item.kind) {
      errors.add(path + ".kind", "is not the catalogue's kind for this id");
    } else if (id->status == IdStatus::Retired) {
      errors.add(path, "is a retired id");
    }
  }

  // Across the file: what fields point at, enum tables, what the groups need, and leads.
  for (const ClassLayout& c : layout.classes_) {
    const std::string path = "class." + c.name;
    for (const FieldLayout& f : c.fields) {
      if (!f.to.empty() && layout.find_class(f.to) == nullptr) {
        errors.add(path + ".fields." + f.name + ".to", "names no class of the file");
      }
      if (!f.elem.empty() && layout.find_class(f.elem) == nullptr) {
        errors.add(path + ".fields." + f.name + ".elem", "names no class of the file");
      }
    }
    check_enum_use(c.fields, path + ".fields", layout.enums_, errors);
  }
  for (const FieldNeed& need : needs.fields) {
    const ClassLayout* c = layout.find_class(need.cls);
    const FieldLayout* f = c != nullptr ? c->field(need.field) : nullptr;
    if (f == nullptr) {
      errors.add("", "lacks the field " + toml_read::shown(need.cls) + "." + toml_read::shown(need.field) +
                         " that a read group needs");
    } else if (f->type != need.type) {
      errors.add("class." + std::string(need.cls) + ".fields." + std::string(need.field),
                 std::string("must be of type ") + to_string(need.type));
    }
  }
  for (const EnumNeed& need : needs.enums) {
    const EnumTable* e = layout.find_enum(need.table);
    if (e == nullptr) {
      errors.add("", "lacks the enum table " + toml_read::shown(need.table) + " that a read group needs");
      continue;
    }
    for (const auto& [raw, name] : e->values) {
      if (std::find(need.names.begin(), need.names.end(), name) == need.names.end()) {
        errors.add("enum." + e->name + ".values", "maps to a name the snapshot's enum does not have");
        break;
      }
    }
  }
  for (const std::string_view param : needs.params) {
    if (layout.find_param(param) == nullptr) {
      errors.add("", "lacks the param " + toml_read::shown(param) + " that a read group needs");
    }
  }
  if (mode == LayoutMode::Release) {
    const auto refuse = [&](const std::string& path) { errors.add(path, "is a lead, which a release build refuses"); };
    if (layout.task_ && layout.task_->status == LayoutStatus::Lead) refuse("task");
    if (layout.fmg_ && layout.fmg_->status == LayoutStatus::Lead) refuse("fmg");
    if (layout.dlrf_ && layout.dlrf_->status == LayoutStatus::Lead) refuse("dlrf");
    for (const ClassLayout& c : layout.classes_) {
      if (c.status == LayoutStatus::Lead) refuse("class." + c.name);
    }
    for (const ParamLayout& p : layout.params_) {
      if (p.status == LayoutStatus::Lead) refuse("param." + p.name);
    }
    for (const ItemEntry& i : layout.items_) {
      if (i.status == LayoutStatus::Lead) refuse("item." + i.label);
    }
  }
  if (!errors.empty()) return Unexpected{errors.take()};
  return layout;
}

}  // namespace en::gal
