// gal/ids.toml: Lodestar's catalogue of ids (ids.hpp; proposal 0005 §4).
#include "en/gal/ids.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "en/proto/snapshot.hpp"
#include "toml_reader.hpp"

namespace en::gal {

using proto::Expected;
using proto::Unexpected;
using toml_read::Errors;
using toml_read::Table;

const char* to_string(IdKind kind) noexcept {
  switch (kind) {
    case IdKind::Static: return "static";
    case IdKind::Function: return "function";
    case IdKind::Class: return "class";
    case IdKind::Table: return "table";
  }
  return "static";
}

bool is_id_label(std::string_view s) noexcept {
  if (s.empty() || s.size() > kMaxIdLabelBytes) return false;
  if (s.front() < 'a' || s.front() > 'z') return false;
  return std::all_of(s.begin(), s.end(),
                     [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

const IdEntry* Catalogue::find(std::uint32_t number) const noexcept {
  const auto it = std::lower_bound(entries_.begin(), entries_.end(), number,
                                   [](const IdEntry& e, std::uint32_t n) { return e.number < n; });
  return it != entries_.end() && it->number == number ? &*it : nullptr;
}

const IdEntry* Catalogue::find(std::string_view label, bool or_was) const noexcept {
  for (const IdEntry& e : entries_) {
    if (e.label == label || (or_was && !e.was.empty() && e.was == label)) return &e;
  }
  return nullptr;
}

namespace {

// What may need an id: the boot (readiness and the gate's own checks), a read group, or the task service.
constexpr std::array<std::string_view, 13> kNeeds = {"boot",  "session", "world",  "combat", "player",
                                                     "flags", "text",    "ui",     "place",  "camera",
                                                     "params", "status", "task_service"};
static_assert(kNeeds.size() == proto::kGroupCount + 2, "boot, proto::Group's names in its order, the task service");

constexpr std::array<std::pair<std::string_view, IdKind>, kIdKindCount> kKinds = {
    {{"static", IdKind::Static}, {"function", IdKind::Function}, {"class", IdKind::Class}, {"table", IdKind::Table}}};

bool printable_ascii(std::string_view s) noexcept {
  return std::all_of(s.begin(), s.end(), [](char c) { return c >= 0x20 && c <= 0x7E; });
}

// One [[id]]: { n, label, was?, kind, means, needs, required, status? }.
void read_id(const toml::table& t, const std::string& path, Errors& errors, IdEntry& out) {
  Table e(t, path, errors);
  std::optional<IdKind> kind;
  if (const auto text = e.text("kind", true)) {
    for (const auto& [name, value] : kKinds) {
      if (*text == name) kind = value;
    }
    if (!kind) errors.add(e.where("kind"), "must be one of static, function, class, table");
  }
  if (kind) out.kind = *kind;
  if (const auto n = e.integer("n", true, 1000, 4999)) {
    out.number = static_cast<std::uint32_t>(*n);
    if (kind && kind_of_number(out.number) != kind) {
      errors.add(e.where("n"), "lies outside its kind's block (static 1000, function 2000, class 3000, table 4000)");
    }
  }
  if (const auto label = e.text("label", true)) {
    if (!is_id_label(*label)) {
      errors.add(e.where("label"), "must be lower-case letters, digits and _, starting with a letter, 1 to 48 bytes");
    } else {
      out.label = *label;
    }
  }
  if (const auto was = e.text("was", false)) {
    if (!is_id_label(*was)) {
      errors.add(e.where("was"), "must be lower-case letters, digits and _, starting with a letter, 1 to 48 bytes");
    } else if (*was == out.label) {
      errors.add(e.where("was"), "is the label itself");
    } else {
      out.was = *was;
    }
  }
  if (const auto means = e.text("means", true)) {
    if (means->empty() || means->size() > kMaxIdMeansBytes || !printable_ascii(*means)) {
      errors.add(e.where("means"), "must be 1 to 400 bytes of printable ASCII");
    } else {
      out.means = *means;
    }
  }
  if (const auto needs = e.text("needs", true)) {
    if (std::find(kNeeds.begin(), kNeeds.end(), std::string_view(*needs)) == kNeeds.end()) {
      errors.add(e.where("needs"), "must be boot, a read group's name or task_service");
    } else {
      out.needs = *needs;
    }
  }
  if (const auto required = e.boolean("required", true)) out.required = *required;
  if (const auto status = e.text("status", false)) {
    if (*status == "active") {
      out.status = IdStatus::Active;
    } else if (*status == "retired") {
      out.status = IdStatus::Retired;
    } else {
      errors.add(e.where("status"), "must be one of active, retired");
    }
  }
  if (out.status == IdStatus::Retired && out.required) {
    errors.add(e.where("required"), "must be false for a retired id");
  }
  e.done();
}

}  // namespace

Expected<Catalogue, SchemaErrors> parse_ids(std::string_view text, std::string_view file) {
  Errors errors{std::string(file)};
  const std::optional<toml::table> doc = toml_read::parse(text, errors);
  if (!doc) return Unexpected{errors.take()};
  Table root(*doc, "", errors);
  (void)root.integer("schema", true, 1, 1);
  Catalogue catalogue;
  if (const toml::array* list = root.array("id", true)) {
    if (list->empty() || list->size() > kMaxIds) errors.add("id", "must list 1 to 4000 ids");
    std::uint32_t last = 0;
    for (std::size_t i = 0; i < list->size() && i < kMaxIds; ++i) {
      const std::string path = toml_read::indexed("id", i);
      const toml::node* node = list->get(i);
      const toml::table* t = node != nullptr ? node->as_table() : nullptr;
      if (t == nullptr) {
        errors.add(path, "must be a table");
        continue;
      }
      IdEntry entry;
      read_id(*t, path, errors, entry);
      if (entry.number != 0) {
        if (entry.number <= last) errors.add(path + ".n", "must be above the id before it: numbers ascend, none twice");
        last = std::max(last, entry.number);
      }
      for (const std::string* name : {&entry.label, &entry.was}) {
        if (name->empty()) continue;
        if (catalogue.find(*name, true) != nullptr) {
          errors.add(path + (name == &entry.label ? ".label" : ".was"), "is an earlier id's label, or its earlier label");
        }
      }
      catalogue.entries_.push_back(std::move(entry));
    }
  }
  root.done();
  if (!errors.empty()) return Unexpected{errors.take()};
  return catalogue;
}

}  // namespace en::gal
