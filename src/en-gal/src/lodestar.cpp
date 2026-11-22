// Lodestar's finder and its numbers-only report (lodestar.hpp; proposal 0005 §6.2, §11).
#include "en/gal/lodestar.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>
#include <utility>

#include "en/gal/fd4.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/roles.hpp"

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
  }
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

bool complete(const Found& found) {
  const AccessorFound& a = found.accessor;
  if (!a.recipe || a.expect != Expect::Within || a.accepted == 0) return false;
  return !a.pattern || (a.only_shape == 0 && a.only_pattern == 0);
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
    W("expect", kExpect), U("statics_found"), U("class_objects"), AO("statics"), U("rva"), U("class_object"),
    AU("witnesses"), AU("getters"),
    // the comparison with the withdrawn pattern: counts only
    O("pattern"), B("cut"), U("both"), U("only_shape"), U("only_pattern"), AO("why"), U("step"),
    W("differs", kDiffers), AO("only_pattern_statics"),
    // classes and the groups' table
    AO("classes"), Key{"label", Type::Label}, W("status", kStatus), U("type_descriptor"), U("classes_derived"),
    AU("vtables"), O("table"), U("entries"), U("stride"), B("has_name_offset"), U("name_offset"), B("wide"), U("index"),
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
