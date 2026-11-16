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

bool Layout::has_leads() const noexcept {
  const auto lead = [](LayoutStatus s) { return s == LayoutStatus::Lead; };
  if (task_ && lead(task_->status)) return true;
  if (fmg_ && lead(fmg_->status)) return true;
  if (dlrf_ && lead(dlrf_->status)) return true;
  return std::any_of(classes_.begin(), classes_.end(), [&](const ClassLayout& c) { return lead(c.status); }) ||
         std::any_of(params_.begin(), params_.end(), [&](const ParamLayout& p) { return lead(p.status); });
}

std::uint32_t Layout::max_copy_size() const noexcept {
  std::uint32_t most = 0;
  for (const ClassLayout& c : classes_) most = std::max({most, c.copy_size.value_or(0), c.stride.value_or(0)});
  for (const ParamLayout& p : params_) most = std::max(most, p.row_size);
  return most;
}

namespace {

using toml_read::Errors;
using toml_read::Table;

constexpr std::array<std::string_view, 12> kGroupNames = {"session", "world", "combat", "player", "flags",  "text",
                                                          "ui",      "place", "camera", "params", "status", "boot"};
static_assert(kGroupNames.size() == proto::kGroupCount + 1, "proto::Group's names, in its order, then boot");

// A type's name, and its size where the name alone fixes it (0 for utf16[n] and array).
struct TypeName {
  std::string_view name;
  FieldType type;
  std::uint32_t size;
};
constexpr std::array<TypeName, 15> kTypes = {{{"u8", FieldType::U8, 1},
                                              {"u16", FieldType::U16, 2},
                                              {"u32", FieldType::U32, 4},
                                              {"u64", FieldType::U64, 8},
                                              {"i32", FieldType::I32, 4},
                                              {"f32", FieldType::F32, 4},
                                              {"bool8", FieldType::Bool8, 1},
                                              {"bit", FieldType::Bit, 1},
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

// One field: { off, ty, group?, range?, bit?, to?, enum?, elem?, len?, stride? }. `param` fields take off and ty only.
std::optional<FieldLayout> read_field(const toml::table& t, std::string name, const std::string& path, Errors& errors,
                                      bool param) {
  Table f(t, path, errors);
  FieldLayout out;
  out.name = std::move(name);
  bool ok = true;
  if (const auto off = f.integer("off", true, 0, kMaxCopySize - 1)) {
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
                 "must be one of u8, u16, u32, u64, i32, f32, bool8, bit, ptr, handle, vec3f, mat4f, utf16[n], "
                 "dlvector, dlmap, array");
      ok = false;
    } else {
      out.type = known->type;
      out.size = known->size;
    }
  }
  if (param && ok && (out.type == FieldType::Array || out.type == FieldType::Bit)) {
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
    if (const toml::node* range = f.node("range", false)) {
      const auto domain = ok ? domain_of(out.type) : std::nullopt;
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
      if (ok && !domain_of(out.type)) errors.add(f.where("enum"), "applies only to whole-number fields");
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
    if (bound && std::uint64_t{field->off} + field->size > *bound) {
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
  if (const auto size = c.integer("copy_size", false, 1, kMaxCopySize))
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

TaskLayout read_task(const toml::table& t, Errors& errors) {
  Table k(t, "task", errors);
  TaskLayout out;
  if (const toml::table* group = k.table("group", true)) {
    Table g(*group, k.where("group"), errors);
    if (const auto name = g.text("name", true)) {
      if (!toml_read::is_identifier(*name, 64)) errors.add(g.where("name"), "must be 1 to 64 letters, digits and _");
      out.group_name = *name;
    }
    if (const auto index = g.integer("index", true, 0, 4095)) out.group_index = static_cast<std::uint32_t>(*index);
    g.done();
  }
  if (const toml::table* abi = k.table("register_abi", true)) {
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
  const auto slots = k.integer("vtable_slots", true, 1, 64);
  if (slots) out.vtable_slots = static_cast<std::uint32_t>(*slots);
  if (const auto run = k.integer("run_slot", true, 0, 63)) {
    out.run_slot = static_cast<std::uint32_t>(*run);
    if (slots && out.run_slot >= out.vtable_slots) errors.add(k.where("run_slot"), "must be below vtable_slots");
  }
  if (const auto size = k.integer("object_size", true, 0, kMaxCopySize))
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
  Errors errors{std::string(file)};
  const std::optional<toml::table> doc = toml_read::parse(text, errors);
  if (!doc) return Unexpected{errors.take()};
  Table root(*doc, "", errors);
  Layout layout;
  layout.build_key_ = profile.key();
  (void)root.integer("schema", true, 1, 1);
  if (const auto build = root.text("build", true)) {
    if (*build != profile.entry().key) errors.add("build", "is not the build the gate matched");  // R7
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
  root.done();

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
  }
  if (!errors.empty()) return Unexpected{errors.take()};
  return layout;
}

}  // namespace en::gal
