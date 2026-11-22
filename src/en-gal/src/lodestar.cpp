// Lodestar's finder and its numbers-only report (lodestar.hpp; proposal 0005 §6.2, §11).
#include "en/gal/lodestar.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>
#include <utility>

#include "en/gal/fd4.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/resolve.hpp"
#include "en/gal/roles.hpp"
#include "en/gal/rtti.hpp"

namespace en::gal::lodestar {

namespace {

constexpr std::size_t kMaxListed = 64;

std::set<std::uint32_t> accepted_hits(const fd4::Result& result) {
  std::set<std::uint32_t> out;
  for (const fd4::Site& s : result.sites) {
    if (!s.refused) out.insert(s.hit);
  }
  return out;
}

}  // namespace

Found find(const ImageInfo& image, const ByteSource& source, std::uint64_t file_size, const derive::Leads& leads,
           const RecipeSet& recipes, const Builds* builds) {
  Found out;
  // ---- The image ----
  out.image.has_version = image.version.has_value();
  if (image.version) out.image.file_version = image.version->file_version;
  out.image.time_date_stamp = image.time_date_stamp;
  out.image.size_of_image = image.size_of_image;
  out.image.file_size = file_size;
  out.image.sections = image.sections.size();
  out.image.functions = image.functions.size();
  if (builds != nullptr) {
    const auto gated = gate(image, *builds);
    out.image.known_build = gated.has_value();
    if (!gated) out.image.gate_problem = gated.error().problem;
  } else {
    out.image.gate_problem = GateProblem::UnknownBuild;
  }

  // ---- The derivation, as R0 ran it: the classes, the groups' table, and the pattern for the comparison ----
  derive::Options options;
  options.file_size = file_size;
  const derive::Result derived = derive::derive(image, source, leads, options);
  for (const derive::VtableSetResult& set : derived.sets) {
    ClassFound c;
    c.label = set.label;
    c.status = set.status;
    c.type_descriptor = set.type_descriptor;
    c.classes = set.classes;
    c.vtables = set.primary;
    std::sort(c.vtables.begin(), c.vtables.end());
    if (c.vtables.size() > kMaxListed) c.vtables.resize(kMaxListed);
    out.classes.push_back(std::move(c));
  }
  out.table.status = derived.group.status;
  if (!derived.group.candidates.empty()) {
    const derive::GroupCandidate& g = derived.group.candidates.front();
    out.table.rva = g.at;
    out.table.entries = g.entries;
    out.table.stride = g.stride;
    out.table.name_offset = g.name_offset;
    out.table.wide = g.wide;
    out.table.index = g.index;
  }

  // ---- The accessor's shape ----
  const RecipeSpec* recipe = recipes.find(roles::kAccessorRecipeId);
  if (recipe == nullptr) return out;
  AccessorFound& a = out.accessor;
  a.recipe = true;
  const ImageBytes img(image, source);
  fd4::Config config;
  config.code = recipe->section;
  config.path_suffix = recipe->path_suffix;
  const fd4::Result by_shape = fd4::find(img, recipe->accessor(), config);
  a.hits = by_shape.hits;
  a.sites = by_shape.sites.size();
  a.expect = a.hits < recipe->expect_min ? Expect::Below : a.hits > recipe->expect_max ? Expect::Above : Expect::Within;
  std::map<std::uint32_t, StaticFound> statics;
  std::set<std::uint32_t> class_objects;
  std::set<std::uint32_t> getters;
  std::map<std::uint32_t, std::set<std::uint32_t>> statics_of_class;  // by class object
  std::map<std::uint32_t, std::set<std::uint32_t>> classes_of_static;
  for (const fd4::Site& s : by_shape.sites) {
    if (s.refused) {
      const auto reason = static_cast<std::size_t>(*s.refused);
      if (reason < a.refused.size()) ++a.refused[reason];
      continue;
    }
    ++a.accepted;
    StaticFound& one = statics[s.static_slot];
    one.rva = s.static_slot;
    if (one.sites == 0) one.class_object = s.class_object;
    ++one.sites;
    if (one.witnesses.size() < kMaxSiteWitnesses) one.witnesses.push_back(s.hit);
    class_objects.insert(s.class_object);
    getters.insert(s.getter);
    statics_of_class[s.class_object].insert(s.static_slot);
    classes_of_static[s.static_slot].insert(s.class_object);
  }
  for (const auto& [object, slots] : statics_of_class) a.shared_class_objects += slots.size() > 1 ? 1 : 0;
  for (const auto& [slot, objects] : classes_of_static) a.shared_statics += objects.size() > 1 ? 1 : 0;
  a.statics_found = statics.size();
  a.class_objects = class_objects.size();
  for (const auto& [rva, one] : statics) {
    if (a.statics.size() >= kMaxStaticsReported) break;
    a.statics.push_back(one);
  }
  for (const std::uint32_t g : getters) {
    if (a.getters.size() >= kMaxListed) break;
    a.getters.push_back(g);
  }

  // ---- The withdrawn pattern, in memory only: do the two accept the same sites? ----
  if (derived.fd4.idiom) {
    a.pattern = true;
    fd4::Config old = config;
    old.code = ".text";  // the derivation cuts and counts it there
    const fd4::Result by_pattern = fd4::find(img, *derived.fd4.idiom, old);
    a.pattern_hits = by_pattern.hits;
    const std::set<std::uint32_t> p = accepted_hits(by_pattern);
    const std::set<std::uint32_t> s = accepted_hits(by_shape);
    a.pattern_accepted = p.size();
    for (const std::uint32_t hit : s) {
      if (p.count(hit) != 0) {
        ++a.both;
      } else {
        ++a.only_shape;
      }
    }
    a.only_pattern = p.size() - a.both;
    // The sites the pattern alone accepts: why the shape does not stand there, and the statics only they load.
    const SectionCopy* text = img.named(config.code);
    std::set<std::uint32_t> pattern_statics;
    std::map<std::pair<std::uint8_t, std::uint8_t>, std::uint64_t> why;
    std::map<std::uint32_t, StaticFound> extra;
    for (const fd4::Site& site : by_pattern.sites) {
      if (site.refused) continue;
      pattern_statics.insert(site.static_slot);
      if (s.count(site.hit) != 0 || text == nullptr) continue;
      const std::optional<shape::Mismatch> miss = shape::mismatch_at(img, *text, recipe->shape, site.hit);
      if (miss) ++why[{miss->step, static_cast<std::uint8_t>(miss->differs)}];
      if (statics.count(site.static_slot) != 0) continue;  // a static the shape's own sites load too
      StaticFound& one = extra[site.static_slot];
      one.rva = site.static_slot;
      if (one.sites == 0) one.class_object = site.class_object;
      ++one.sites;
      if (one.witnesses.size() < kMaxSiteWitnesses) one.witnesses.push_back(site.hit);
    }
    a.pattern_statics = pattern_statics.size();
    for (const auto& [key, count] : why) {
      a.only_pattern_why.push_back(Why{key.first, static_cast<shape::Differs>(key.second), count});
    }
    for (auto& [rva, one] : extra) {
      if (a.only_pattern_statics.size() >= kMaxListed) break;
      a.only_pattern_statics.push_back(std::move(one));
    }
  }
  return out;
}

InstancesFound name_instances(const ImageInfo& image, const ByteSource& live, const AccessorFound& accessor,
                              const derive::Leads& leads, const Catalogue& ids, const ReadWord& read_word) {
  InstancesFound out;
  const ImageBytes img(image, live);
  const rtti::Walk walk = rtti::walk(img);
  std::map<std::uint32_t, std::size_t> vtables;  // by offset: its place in the walk's
  for (std::size_t i = 0; i < walk.vtables.size(); ++i) vtables.emplace(walk.vtables[i].rva, i);
  // By static: its instance's class and table of methods. The names stay here: nothing below writes one.
  struct Classed {
    std::string cls;
    std::uint32_t vtable = 0;
  };
  std::map<std::uint32_t, Classed> classes;
  for (const StaticFound& s : accessor.statics) {
    ++out.tried;
    const std::optional<std::uint64_t> instance = img.u64(s.rva);
    if (!instance || *instance == 0) {
      ++out.null;
      continue;
    }
    // An instance is a heap object: one that lies in the image is no instance, and is not read as one.
    const std::optional<std::uint64_t> word =
        img.rva_of(*instance) || !read_word ? std::nullopt : read_word(*instance);
    if (!word) {
      ++out.unread;
      continue;
    }
    const std::optional<std::uint32_t> rva = img.rva_of(*word);
    const auto it = rva ? vtables.find(*rva) : vtables.end();
    if (it == vtables.end()) {
      ++out.unclassed;
      continue;
    }
    const rtti::Locator& locator = walk.locators[walk.vtables[it->second].locator];
    const std::optional<std::string> cls =
        locator.offset == 0 ? derive::innermost_class(img, locator.type_descriptor) : std::nullopt;
    if (!cls) {
      ++out.unclassed;
      continue;
    }
    ++out.classed;
    classes.emplace(s.rva, Classed{*cls, *rva});
  }
  for (const derive::NamedLead& lead : leads.singletons) {
    ++out.leads;
    const IdEntry* id = ids.find(lead.label, true);
    if (id == nullptr || id->kind != IdKind::Static || id->status != IdStatus::Active) {
      ++out.unlisted;
      continue;
    }
    ManagerFound m;
    m.label = id->label;
    m.id = id->number;
    std::uint32_t matches = 0;
    for (const auto& [slot, c] : classes) {
      const bool plain = c.cls == lead.name;
      if (!plain && c.cls != lead.name + "Imp") continue;
      if (matches++ != 0) continue;  // the first stands for an ambiguous one, as the derivation reports it
      m.rva = slot;
      m.vtable = c.vtable;
      m.imp = !plain;
    }
    if (matches != 0) {
      m.status = matches == 1 ? derive::Status::Derived : derive::Status::Ambiguous;
      for (const StaticFound& s : accessor.statics) {
        if (s.rva != m.rva) continue;
        m.sites = s.sites;
        m.witnesses = s.witnesses;
      }
    }
    out.named += m.status == derive::Status::Derived ? 1 : 0;
    out.ambiguous += m.status == derive::Status::Ambiguous ? 1 : 0;
    out.managers.push_back(std::move(m));
  }
  std::sort(out.managers.begin(), out.managers.end(),
            [](const ManagerFound& a, const ManagerFound& b) { return a.id < b.id; });
  return out;
}

void merge(InstancesFound& held, const InstancesFound& later) {
  const std::uint32_t pass = held.passes + 1;
  std::vector<ManagerFound> managers = std::move(held.managers);
  held = later;
  held.passes = pass;
  for (ManagerFound& m : held.managers) {
    const auto it = std::find_if(managers.begin(), managers.end(), [&](const ManagerFound& o) { return o.id == m.id; });
    if (it != managers.end() && it->status == derive::Status::Derived) {
      m = *it;  // named once, named for the round
    } else if (m.status == derive::Status::Derived) {
      m.pass = pass;
    }
  }
  held.named = 0;
  held.ambiguous = 0;
  for (const ManagerFound& m : held.managers) {
    held.named += m.status == derive::Status::Derived ? 1 : 0;
    held.ambiguous += m.status == derive::Status::Ambiguous ? 1 : 0;
  }
}

namespace {

std::string hex(std::uint32_t v) {
  static constexpr char kDigits[] = "0123456789ABCDEF";
  std::string out;
  do {
    out.insert(out.begin(), kDigits[v & 0xF]);
    v >>= 4;
  } while (v != 0);
  return "0x" + out;
}

std::string hex_list(const std::vector<std::uint32_t>& values) {
  std::string out = "[";
  for (std::size_t i = 0; i < values.size(); ++i) out += (i != 0 ? ", " : "") + hex(values[i]);
  return out + "]";
}

template <typename T>
std::string word_list(const std::vector<T>& words) {
  std::string out = "[";
  for (std::size_t i = 0; i < words.size(); ++i) out += std::string(i != 0 ? ", " : "") + "\"" + to_string(words[i]) + "\"";
  return out + "]";
}

std::string quoted_list(const std::vector<std::string>& texts) {
  std::string out = "[";
  for (std::size_t i = 0; i < texts.size(); ++i) out += std::string(i != 0 ? ", " : "") + "\"" + texts[i] + "\"";
  return out + "]";
}

}  // namespace

DraftTable draft_table(const Found& found, const Catalogue& ids, const TableProvenance& provenance,
                       const ImageInfo& image, const ByteSource& source) {
  // What was found, as entries; the catalogue's labels, numbers and kinds.
  std::vector<ItemEntry> entries;
  if (found.instances) {
    for (const ManagerFound& m : found.instances->managers) {
      if (m.status != derive::Status::Derived) continue;
      ItemEntry e;
      e.label = m.label;
      e.id = m.id;
      e.kind = IdKind::Static;
      e.rva = m.rva;
      e.found_by = {Recipe::Accessor, Recipe::InstanceClass};
      e.witnesses = m.witnesses;
      if (e.witnesses.size() > kMaxWitnesses) e.witnesses.resize(kMaxWitnesses);
      e.checks = {ItemCheck::Agree, ItemCheck::Class};  // two recipes give the slot; its instance has the class
      entries.push_back(std::move(e));
    }
  }
  for (const ClassFound& c : found.classes) {
    const IdEntry* id = ids.find(c.label, true);
    if (c.status != derive::Status::Derived || c.vtables.empty() || id == nullptr || id->kind != IdKind::Class ||
        id->status != IdStatus::Active) {
      continue;
    }
    ItemEntry e;
    e.label = id->label;
    e.id = id->number;
    e.kind = IdKind::Class;
    e.vtables = c.vtables;
    std::sort(e.vtables.begin(), e.vtables.end());
    if (e.vtables.size() > kMaxVtables) e.vtables.resize(kMaxVtables);
    e.found_by = {Recipe::Class};
    entries.push_back(std::move(e));
  }
  if (found.table.status == derive::Status::Derived && found.table.entries != 0) {
    for (const IdEntry& id : ids.entries()) {
      if (id.kind != IdKind::Table || id.status != IdStatus::Active) continue;
      ItemEntry e;
      e.label = id.label;
      e.id = id.number;
      e.kind = IdKind::Table;
      e.rva = found.table.rva;
      e.stride = found.table.stride;
      e.count = found.table.entries;
      e.found_by = {Recipe::Table};
      entries.push_back(std::move(e));
      break;  // the catalogue's first table is the task groups'
    }
  }
  std::sort(entries.begin(), entries.end(), [](const ItemEntry& a, const ItemEntry& b) { return a.id < b.id; });

  DraftTable out;
  out.text = "[provenance]\nmethod = \"own-derivation\"\ntool = \"" + provenance.tool + "\"\ntool_sha256 = \"" +
             provenance.tool_sha256 + "\"\nround = \"" + provenance.round + "\"\ndate = \"" + provenance.date + "\"\n";
  if (!provenance.leads.empty()) out.text += "leads = " + quoted_list(provenance.leads) + "\n";
  for (ItemEntry& e : entries) {
    // Each entry against the image, as a start will check it: first its placement alone, then each witness.
    ItemEntry bare = e;
    bare.witnesses.clear();
    if (!check_item(image, source, bare).ok()) {
      ++out.left_out;
      continue;
    }
    e.checks.insert(e.checks.begin(), ItemCheck::Placement);
    std::vector<std::uint32_t> standing;
    for (const std::uint32_t w : e.witnesses) {
      bare.witnesses = {w};
      if (check_item(image, source, bare).ok()) {
        standing.push_back(w);
      } else {
        ++out.witnesses_dropped;
      }
    }
    e.witnesses = std::move(standing);
    if (!e.witnesses.empty()) e.checks.push_back(ItemCheck::Witness);
    std::sort(e.checks.begin(), e.checks.end());
    out.text += "\n[item." + e.label + "]\nid = " + std::to_string(e.id) + "\nkind = \"" + to_string(e.kind) + "\"\n";
    if (e.kind == IdKind::Class) {
      out.text += "vtables = " + hex_list(e.vtables) + "\n";
    } else {
      out.text += "rva = " + hex(e.rva) + "\n";
    }
    if (e.kind == IdKind::Table) {
      out.text += "stride = " + std::to_string(e.stride) + "\ncount = " + std::to_string(e.count) + "\n";
    }
    out.text += "found_by = " + word_list(e.found_by) + "\n";
    if (!e.witnesses.empty()) out.text += "witnesses = " + hex_list(e.witnesses) + "\n";
    out.text += "checks = " + word_list(e.checks) + "\nstatus = \"lead\"\n";
    ++out.entries;
  }
  return out;
}

bool complete(const Found& found) {
  const AccessorFound& a = found.accessor;
  if (!a.recipe || a.expect != Expect::Within || a.accepted == 0) return false;
  if (a.shared_class_objects != 0 || a.shared_statics != 0) return false;
  return !a.pattern || a.only_shape == 0;
}

// ---- The report: schema 2, kind `lodestar`. No key takes a pattern, and there is no free string. ----

namespace {

using report::Key;
using report::Type;
using report::Writer;

constexpr std::array<std::string_view, 7> kStatus = {"derived",    "ambiguous",  "refused", "missing",
                                                     "not_unique", "needs_slot", "depends"};
constexpr std::array<std::string_view, 4> kGate = {"known", "no_version", "unknown_build", "fingerprint_differs"};
constexpr std::array<std::string_view, 3> kExpect = {"within", "below", "above"};
constexpr std::array<std::string_view, 4> kDiffers = {"narrow", "other_register", "other", "undecodable"};
constexpr std::array<std::string_view, 9> kRefusal = {"unreadable",    "static_section", "static_alignment",
                                                      "class_section", "getter_section", "getter_not_function",
                                                      "no_path",       "path_section",   "path_text"};

constexpr Key U(std::string_view name) { return Key{name, Type::Unsigned}; }
constexpr Key B(std::string_view name) { return Key{name, Type::Boolean}; }
constexpr Key O(std::string_view name) { return Key{name, Type::Object}; }
constexpr Key AO(std::string_view name) { return Key{name, Type::Array, Type::Object}; }
constexpr Key AU(std::string_view name) { return Key{name, Type::Array, Type::Unsigned}; }
template <std::size_t N>
constexpr Key W(std::string_view name, const std::array<std::string_view, N>& words) {
  return Key{name, Type::Word, Type::Object, words};
}

constexpr auto kKeys = std::to_array<Key>({
    B("complete"),
    // image
    O("image"), AU("file_version"), B("has_version"), U("time_date_stamp"), U("size_of_image"), U("file_size"),
    U("sections"), U("functions"), W("build", kGate),
    // accessor
    O("accessor"), B("recipe"), U("hits"), U("sites"), U("accepted"), AO("refused"), W("reason", kRefusal), U("count"),
    W("expect", kExpect), U("statics_found"), U("class_objects"), U("shared_class_objects"), U("shared_statics"), AO("statics"), U("rva"), U("class_object"),
    AU("witnesses"), AU("getters"),
    // the comparison with the withdrawn pattern: counts only
    O("pattern"), B("cut"), U("both"), U("only_shape"), U("only_pattern"), AO("why"), U("step"),
    W("differs", kDiffers), AO("only_pattern_statics"),
    // classes and the groups' table
    AO("classes"), Key{"label", Type::Label}, W("status", kStatus), U("type_descriptor"), U("classes_derived"),
    AU("vtables"), O("instances"), U("tried"), U("null"), U("unread"), U("unclassed"), U("classed"), U("leads"),
    U("unlisted"), U("named"), U("ambiguous"), U("passes"), AO("managers"), U("id"), U("vtable"), B("imp"), U("pass"),
    // The reader's trial (in game)
    O("trial"), U("steps"), U("readings"), U("elapsed_ms"), U("reading_p50_us"), U("reading_p99_us"),
    U("reading_max_us"), U("copies"), U("fault_implausible"), U("fault_stack"), U("fault_copy"), U("fault_budget"),
    U("fault_thread"), U("fault_injected"), U("same"), U("other"), U("fault"), U("torn"), O("table"), U("entries"), U("stride"), B("has_name_offset"), U("name_offset"), B("wide"), U("index"),
});

constexpr report::Vocabulary kVocabulary{"lodestar", 2, kKeys};

std::string_view status_word(derive::Status s) {
  const auto i = static_cast<std::size_t>(s);
  return i < kStatus.size() ? kStatus[i] : kStatus[3];
}

std::string_view gate_word(const ImageFound& image) {
  if (image.known_build) return kGate[0];
  switch (image.gate_problem.value_or(GateProblem::UnknownBuild)) {
    case GateProblem::NoVersion: return kGate[1];
    case GateProblem::UnknownBuild: return kGate[2];
    case GateProblem::FingerprintDiffers: return kGate[3];
  }
  return kGate[2];
}

void numbers(Writer& w, std::string_view key, const std::vector<std::uint32_t>& values) {
  w.begin_array(key);
  for (const std::uint32_t v : values) w.number(v);
  w.end_array();
}

}  // namespace

const report::Vocabulary& vocabulary() noexcept { return kVocabulary; }

proto::Expected<std::string, report::Problem> write_report(const Found& found) {
  Writer w(kVocabulary);
  w.boolean("complete", complete(found));

  w.begin_object("image");
  w.boolean("has_version", found.image.has_version);
  w.begin_array("file_version");
  for (const std::uint16_t v : found.image.file_version) w.number(v);
  w.end_array();
  w.number("time_date_stamp", found.image.time_date_stamp);
  w.number("size_of_image", found.image.size_of_image);
  w.number("file_size", found.image.file_size);
  w.number("sections", found.image.sections);
  w.number("functions", found.image.functions);
  w.word("build", gate_word(found.image));
  w.end_object();

  const AccessorFound& a = found.accessor;
  w.begin_object("accessor");
  w.boolean("recipe", a.recipe);
  w.number("hits", a.hits);
  w.number("sites", a.sites);
  w.number("accepted", a.accepted);
  w.begin_array("refused");
  for (std::size_t i = 0; i < a.refused.size() && i < kRefusal.size(); ++i) {
    if (a.refused[i] == 0) continue;
    w.begin_object();
    w.word("reason", kRefusal[i]);
    w.number("count", a.refused[i]);
    w.end_object();
  }
  w.end_array();
  w.word("expect", kExpect[static_cast<std::size_t>(a.expect) < kExpect.size() ? static_cast<std::size_t>(a.expect) : 0]);
  w.number("statics_found", a.statics_found);
  w.number("class_objects", a.class_objects);
  w.number("shared_class_objects", a.shared_class_objects);
  w.number("shared_statics", a.shared_statics);
  w.begin_array("statics");
  for (const StaticFound& s : a.statics) {
    w.begin_object();
    w.number("rva", s.rva);
    w.number("class_object", s.class_object);
    w.number("sites", s.sites);
    numbers(w, "witnesses", s.witnesses);
    w.end_object();
  }
  w.end_array();
  numbers(w, "getters", a.getters);
  w.begin_object("pattern");
  w.boolean("cut", a.pattern);
  w.number("hits", a.pattern_hits);
  w.number("accepted", a.pattern_accepted);
  w.number("both", a.both);
  w.number("only_shape", a.only_shape);
  w.number("only_pattern", a.only_pattern);
  w.number("statics_found", a.pattern_statics);
  w.begin_array("why");
  for (const Why& y : a.only_pattern_why) {
    w.begin_object();
    w.number("step", y.step);
    const auto kind = static_cast<std::size_t>(y.differs);
    w.word("differs", kDiffers[kind < kDiffers.size() ? kind : 2]);
    w.number("count", y.count);
    w.end_object();
  }
  w.end_array();
  w.begin_array("only_pattern_statics");
  for (const StaticFound& s : a.only_pattern_statics) {
    w.begin_object();
    w.number("rva", s.rva);
    w.number("class_object", s.class_object);
    w.number("sites", s.sites);
    numbers(w, "witnesses", s.witnesses);
    w.end_object();
  }
  w.end_array();
  w.end_object();
  w.end_object();

  w.begin_array("classes");
  for (const ClassFound& c : found.classes) {
    w.begin_object();
    w.label("label", c.label);
    w.word("status", status_word(c.status));
    w.number("type_descriptor", c.type_descriptor);
    w.number("classes_derived", c.classes);
    numbers(w, "vtables", c.vtables);
    w.end_object();
  }
  w.end_array();

  if (found.instances) {
    const InstancesFound& n = *found.instances;
    w.begin_object("instances");
    w.number("tried", n.tried);
    w.number("null", n.null);
    w.number("unread", n.unread);
    w.number("unclassed", n.unclassed);
    w.number("classed", n.classed);
    w.number("leads", n.leads);
    w.number("unlisted", n.unlisted);
    w.number("named", n.named);
    w.number("ambiguous", n.ambiguous);
    w.number("passes", n.passes);
    w.begin_array("managers");
    for (const ManagerFound& m : n.managers) {
      w.begin_object();
      w.number("id", m.id);
      w.label("label", m.label);
      w.word("status", status_word(m.status));
      w.number("rva", m.rva);
      w.number("vtable", m.vtable);
      w.number("sites", m.sites);
      w.boolean("imp", m.imp);
      w.number("pass", m.pass);
      numbers(w, "witnesses", m.witnesses);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.trial) {
    const TrialFound& t = *found.trial;
    static constexpr std::array<std::string_view, kTrialFaultKinds> kFaultKeys = {
        "fault_implausible", "fault_stack", "fault_copy", "fault_budget", "fault_thread", "fault_injected"};
    w.begin_object("trial");
    w.number("steps", t.steps);
    w.number("readings", t.readings);
    w.number("elapsed_ms", t.elapsed_ms);
    w.number("reading_p50_us", t.reading_p50_us);
    w.number("reading_p99_us", t.reading_p99_us);
    w.number("reading_max_us", t.reading_max_us);
    w.number("copies", t.copies);
    for (std::size_t k = 0; k < kTrialFaultKinds; ++k) w.number(kFaultKeys[k], t.faults[k]);
    w.begin_array("managers");
    for (const TrialManager& m : t.managers) {
      w.begin_object();
      w.number("id", m.id);
      w.label("label", m.label);
      w.number("null", m.null);
      w.number("same", m.same);
      w.number("other", m.other);
      w.number("fault", m.fault);
      w.number("torn", m.torn);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  w.begin_object("table");
  w.word("status", status_word(found.table.status));
  w.number("rva", found.table.rva);
  w.number("entries", found.table.entries);
  w.number("stride", found.table.stride);
  w.boolean("has_name_offset", found.table.name_offset.has_value());
  w.number("name_offset", found.table.name_offset.value_or(0));
  w.boolean("wide", found.table.wide);
  w.number("index", found.table.index);
  w.end_object();
  return w.finish();
}

}  // namespace en::gal::lodestar
