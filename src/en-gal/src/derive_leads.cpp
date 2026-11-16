// The derivation's leads file (derive.hpp): what to look for, by name and shape, under labels of its author's own.
//
//   schema = 1
//   product_name = "…"                 # optional: the ProductName the build should carry
//   [fd4]
//   path_suffix = "…"                  # the path every FD4 site loads ends with this
//   [singletons]                       # label = the name its runtime class object gives
//   [register]
//   instance = "<a singleton label>"   # the singleton whose instance the registration takes
//   [task]
//   group = "…"                        # the task group our task joins, by name
//   base = "Space::Class"              # the class every task of the game derives from
//   [vtable_sets]                      # label = a class whose derived classes' vtables make a set
//   [statics]                          # label = { rtti = "Space::Class" } or { module = true }
#include <algorithm>

#include "en/gal/derive.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/rtti.hpp"
#include "toml_reader.hpp"

namespace en::gal::derive {

using proto::Expected;
using proto::Unexpected;
using toml_read::Errors;
using toml_read::Table;

namespace {

constexpr std::size_t kMaxLeadsBytes = 64u << 10;
constexpr std::size_t kMaxSingletons = 64;
constexpr std::size_t kMaxSets = 16;
constexpr std::size_t kMaxStatics = 16;

bool printable(std::string_view s, std::size_t max) {
  return !s.empty() && s.size() <= max && std::all_of(s.begin(), s.end(), [](char c) { return c >= 0x20 && c < 0x7F; });
}

// A path's last part, or a file name: letters, digits, '_', '.', '-', '\\' and '/'.
bool is_suffix(std::string_view s) {
  return !s.empty() && s.size() <= 64 && std::all_of(s.begin(), s.end(), [](char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
           c == '-' || c == '\\' || c == '/';
  });
}

bool is_class(std::string_view s) { return rtti::decorate(s).has_value() && s.size() <= 128; }

// A table of label = "name" pairs.
std::vector<NamedLead> named_table(const toml::table& t, const std::string& path, Errors& errors, std::size_t max,
                                   bool classes) {
  std::vector<NamedLead> out;
  if (t.size() > max) errors.add(path, "holds more than " + std::to_string(max) + " entries");
  for (auto&& [key, node] : t) {
    const std::string label(key.str());
    const std::string at = path + "." + toml_read::shown(label);
    if (!report::is_label(label)) {
      errors.add(at, "is not a label: lower-case letters, digits and _, starting with a letter");
      continue;
    }
    const auto* value = node.as_string();
    if (value == nullptr) {
      errors.add(at, "must be a string");
      continue;
    }
    const std::string name(value->get());
    if (classes ? !is_class(name) : !(is_identifier_text(name) && name.size() <= 128)) {
      errors.add(at, classes ? "must be a class name such as Space::Class" : "must be a name of letters, digits and _");
      continue;
    }
    if (out.size() < max) out.push_back(NamedLead{label, name});
  }
  return out;
}

}  // namespace

Expected<Leads, SchemaErrors> parse_leads(std::string_view text, std::string_view file) {
  Errors errors{std::string(file)};
  if (text.size() > kMaxLeadsBytes) {
    errors.add("", "is larger than 64 KiB");
    return Unexpected{errors.take()};
  }
  const std::optional<toml::table> doc = toml_read::parse(text, errors);
  if (!doc) return Unexpected{errors.take()};
  Leads leads;
  Table root(*doc, "", errors);
  (void)root.integer("schema", true, 1, 1);
  if (const auto product = root.text("product_name", false)) {
    if (!printable(*product, 128)) errors.add("product_name", "must be printable ASCII of 1 to 128 characters");
    leads.product_name = *product;
  }
  if (const toml::table* fd4 = root.table("fd4", true)) {
    Table t(*fd4, "fd4", errors);
    if (const auto suffix = t.text("path_suffix", true)) {
      if (!is_suffix(*suffix)) errors.add(t.where("path_suffix"), "must be 1 to 64 letters, digits, _ . - \\ or /");
      leads.fd4_path_suffix = *suffix;
    }
    t.done();
  }
  if (const toml::table* singletons = root.table("singletons", true)) {
    leads.singletons = named_table(*singletons, "singletons", errors, kMaxSingletons, false);
    if (leads.singletons.empty()) errors.add("singletons", "must name at least one singleton");
  }
  if (const toml::table* reg = root.table("register", true)) {
    Table t(*reg, "register", errors);
    if (const auto instance = t.text("instance", true)) {
      const bool known = std::any_of(leads.singletons.begin(), leads.singletons.end(),
                                     [&](const NamedLead& s) { return s.label == *instance; });
      if (!known) errors.add(t.where("instance"), "must be the label of a singleton");
      leads.register_instance = *instance;
    }
    t.done();
  }
  if (const toml::table* task = root.table("task", true)) {
    Table t(*task, "task", errors);
    if (const auto group = t.text("group", true)) {
      if (!is_identifier_text(*group) || group->size() > 64)
        errors.add(t.where("group"), "must be a name of 1 to 64 letters, digits and _");
      leads.task_group = *group;
    }
    if (const auto base = t.text("base", true)) {
      if (!is_class(*base)) errors.add(t.where("base"), "must be a class name such as Space::Class");
      leads.task_base = *base;
    }
    t.done();
  }
  if (const toml::table* sets = root.table("vtable_sets", false)) {
    leads.vtable_sets = named_table(*sets, "vtable_sets", errors, kMaxSets, true);
  }
  if (const toml::table* statics = root.table("statics", false)) {
    if (statics->size() > kMaxStatics) errors.add("statics", "holds more than 16 entries");
    for (auto&& [key, node] : *statics) {
      const std::string label(key.str());
      const std::string at = "statics." + toml_read::shown(label);
      if (!report::is_label(label)) {
        errors.add(at, "is not a label: lower-case letters, digits and _, starting with a letter");
        continue;
      }
      const toml::table* entry = node.as_table();
      if (entry == nullptr) {
        errors.add(at, "must be a table: { rtti = \"Space::Class\" } or { module = true }");
        continue;
      }
      Table t(*entry, at, errors);
      StaticLead lead;
      lead.label = label;
      lead.module = t.boolean("module", false).value_or(false);
      if (const auto cls = t.text("rtti", false)) {
        if (!is_class(*cls)) errors.add(t.where("rtti"), "must be a class name such as Space::Class");
        lead.rtti = *cls;
      }
      t.done();
      if (lead.module == !lead.rtti.empty()) errors.add(at, "must give either rtti or module = true");
      if (leads.statics.size() < kMaxStatics) leads.statics.push_back(std::move(lead));
    }
  }
  root.done();
  // Labels are unique across the tables, so that each names one thing in the report.
  std::vector<std::string> labels;
  for (const NamedLead& s : leads.singletons) labels.push_back(s.label);
  for (const NamedLead& s : leads.vtable_sets) labels.push_back(s.label);
  for (const StaticLead& s : leads.statics) labels.push_back(s.label);
  std::sort(labels.begin(), labels.end());
  if (std::adjacent_find(labels.begin(), labels.end()) != labels.end()) errors.add("", "a label is used twice");
  if (!errors.empty()) return Unexpected{errors.take()};
  return leads;
}

}  // namespace en::gal::derive
