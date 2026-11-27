// Lodestar's finder and its numbers-only report (lodestar.hpp; proposal 0005 §6.2, §11).
#include "en/gal/lodestar.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string_view>
#include <utility>

#include "code.hpp"
#include "en/gal/fd4.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/resolve.hpp"
#include "en/gal/roles.hpp"
#include "en/gal/rtti.hpp"
#include "en/gal/x86.hpp"

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
           const RecipeSet& recipes, const Builds* builds, const Catalogue* ids) {
  Found out;
  // ---- What the image holds under each lead's name: counts only ----
  {
    const Catalogue none;
    out.lead_classes = Namer(image, source, leads, ids != nullptr ? *ids : none).lead_classes();
  }
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

  // ---- The accessor's other shapes: what each reaches that the accessor's own sites do not ----
  for (const RecipeSpec& other : recipes.recipes) {
    if (other.kind != RecipeKind::Accessor || other.id == roles::kAccessorRecipeId) continue;
    VariantFound v;
    v.label = other.id;
    fd4::Config c;
    c.code = other.section;
    c.path_suffix = other.path_suffix;
    const fd4::Result r = fd4::find(img, other.accessor(), c);
    v.hits = r.hits;
    v.expect = v.hits < other.expect_min ? Expect::Below : v.hits > other.expect_max ? Expect::Above : Expect::Within;
    std::map<std::uint32_t, StaticFound> reached;
    for (const fd4::Site& site : r.sites) {
      if (site.refused) {
        const auto reason = static_cast<std::size_t>(*site.refused);
        if (reason < v.refused.size()) ++v.refused[reason];
        continue;
      }
      ++v.accepted;
      StaticFound& one = reached[site.static_slot];
      one.rva = site.static_slot;
      if (one.sites == 0) one.class_object = site.class_object;
      ++one.sites;
      if (one.witnesses.size() < kMaxSiteWitnesses) one.witnesses.push_back(site.hit);
    }
    v.statics_found = reached.size();
    for (auto& [rva, one] : reached) {
      const auto known = statics.find(rva);
      if (known != statics.end()) {
        ++v.known;
        v.agreeing += known->second.class_object == one.class_object ? 1 : 0;
        continue;
      }
      // A new static's class object must be its own: one that another static has, of any shape, is counted.
      const auto shared = statics_of_class.find(one.class_object);
      const bool taken = shared != statics_of_class.end() && (shared->second.size() > 1 || *shared->second.begin() != rva);
      v.shared_class_objects += taken ? 1 : 0;
      statics_of_class[one.class_object].insert(rva);
      if (v.statics.size() < kMaxListed) v.statics.push_back(std::move(one));
    }
    a.variants.push_back(std::move(v));
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

std::vector<StaticFound> all_statics(const AccessorFound& accessor) {
  std::vector<StaticFound> out = accessor.statics;
  std::set<std::uint32_t> seen;
  for (const StaticFound& s : out) seen.insert(s.rva);
  for (const VariantFound& v : accessor.variants) {
    for (const StaticFound& s : v.statics) {
      if (seen.insert(s.rva).second) out.push_back(s);
    }
  }
  return out;
}

// ---- The namer ----

struct Namer::State {
  struct Lead {
    std::uint32_t id = 0;
    std::string label;
    bool listed = false;  // the catalogue holds it as an active static
  };
  enum Match : std::uint8_t { kExact, kImp, kBase, kRelated };
  struct Accepts {
    std::size_t lead = 0;
    Match match = kExact;
  };
  std::uint64_t base = 0;
  std::uint64_t size = 0;
  std::vector<Lead> leads;
  std::map<std::uint32_t, bool> vtables;                   // every table of methods of the walk: is it a class's primary one?
  std::multimap<std::uint32_t, Accepts> accepts;           // by table of methods: the leads its class answers
  // By a [statics] lead's label: the primary tables of methods of the classes of its name (1) or its name with Imp (2).
  std::map<std::string, std::map<std::uint32_t, int>, std::less<>> static_leads;
  std::vector<LeadClasses> classes;
};

namespace {

std::uint32_t count_text(const ImageBytes& img, const std::string& needle) {
  std::uint32_t n = 0;
  for (const SectionCopy& c : img.sections()) {
    const auto* begin = reinterpret_cast<const char*>(c.bytes.data());
    const std::string_view hay(begin, c.bytes.size());
    for (std::size_t at = hay.find(needle); at != std::string_view::npos; at = hay.find(needle, at + 1)) ++n;
  }
  return n;
}

}  // namespace

Namer::Namer(const ImageInfo& image, const ByteSource& source, const derive::Leads& leads, const Catalogue& ids)
    : state_(std::make_unique<State>()) {
  State& st = *state_;
  st.base = image.image_base;
  st.size = image.size_of_image;
  const ImageBytes img(image, source);
  const rtti::Walk walk = rtti::walk(img);
  // Each class of the walk by its innermost name, compared here and kept nowhere.
  std::map<std::uint32_t, std::string> name_of;  // by type descriptor
  const auto name = [&](std::uint32_t td) -> const std::string& {
    auto it = name_of.find(td);
    if (it == name_of.end()) it = name_of.emplace(td, derive::innermost_class(img, td).value_or(std::string())).first;
    return it->second;
  };
  for (const rtti::Vtable& v : walk.vtables) {
    const rtti::Locator& loc = walk.locators[v.locator];
    st.vtables[v.rva] = loc.offset == 0 && !name(loc.type_descriptor).empty();
  }
  for (const derive::NamedLead& lead : leads.singletons) {
    const IdEntry* id = ids.find(lead.label, true);
    State::Lead l;
    l.listed = id != nullptr && id->kind == IdKind::Static && id->status == IdStatus::Active;
    l.id = l.listed ? id->number : 0;
    l.label = l.listed ? id->label : lead.label;
    const std::size_t index = st.leads.size();
    st.leads.push_back(l);
    LeadClasses c;
    c.label = l.label;
    c.id = l.id;
    const std::string imp = lead.name + "Imp";
    std::set<std::uint32_t> exact_tds, imp_tds, related_tds;
    for (const rtti::Locator& loc : walk.locators) {
      const std::string& n = name(loc.type_descriptor);
      if (n == lead.name) {
        exact_tds.insert(loc.type_descriptor);
      } else if (n == imp) {
        imp_tds.insert(loc.type_descriptor);
      } else if (n.find(lead.name) != std::string::npos) {
        related_tds.insert(loc.type_descriptor);
      }
    }
    c.related = static_cast<std::uint32_t>(related_tds.size());
    c.named = static_cast<std::uint32_t>(exact_tds.size());
    c.named_imp = static_cast<std::uint32_t>(imp_tds.size());
    std::set<std::uint32_t> counted;
    for (const rtti::Vtable& v : walk.vtables) {
      const rtti::Locator& loc = walk.locators[v.locator];
      if (loc.offset != 0) continue;
      std::optional<State::Match> match;
      if (exact_tds.count(loc.type_descriptor) != 0) {
        match = State::kExact;
      } else if (imp_tds.count(loc.type_descriptor) != 0) {
        match = State::kImp;
      } else {
        for (const std::uint32_t b : loc.bases) {
          if (exact_tds.count(b) != 0 || imp_tds.count(b) != 0) match = State::kBase;
        }
      }
      // A class under a longer name answers only for a lead the image has no class of its own name for.
      if (!match && exact_tds.empty() && imp_tds.empty() && related_tds.count(loc.type_descriptor) != 0) {
        match = State::kRelated;
        st.accepts.emplace(v.rva, State::Accepts{index, *match});
        continue;  // not counted among the lead's own tables of methods
      }
      if (!match) continue;
      if (counted.insert(v.rva).second) ++c.vtables;
      st.accepts.emplace(v.rva, State::Accepts{index, *match});
    }
    for (const char* form : {".?AV", ".?AU"}) {
      c.texts += count_text(img, std::string(form) + lead.name + "@");
      c.texts += count_text(img, std::string(form) + imp + "@");
    }
    st.classes.push_back(std::move(c));
  }
  for (const derive::StaticLead& lead : leads.statics) {
    if (lead.rtti.empty()) continue;
    const std::size_t cut = lead.rtti.rfind("::");
    const std::string inner = cut == std::string::npos ? lead.rtti : lead.rtti.substr(cut + 2);
    const std::string imp = inner + "Imp";
    std::map<std::uint32_t, int>& tables = st.static_leads[lead.label];
    for (const rtti::Vtable& v : walk.vtables) {
      const rtti::Locator& loc = walk.locators[v.locator];
      if (loc.offset != 0) continue;
      const std::string& n = name(loc.type_descriptor);
      if (n == inner) {
        tables[v.rva] = 1;
      } else if (n == imp) {
        tables[v.rva] = 2;
      }
    }
  }
}

Namer::~Namer() = default;

int Namer::static_lead_class(std::string_view label, std::uint32_t vtable) const {
  const auto lead = state_->static_leads.find(label);
  if (lead == state_->static_leads.end()) return 0;
  const auto table = lead->second.find(vtable);
  return table == lead->second.end() ? 0 : table->second;
}

std::uint32_t Namer::static_lead_tables(std::string_view label) const {
  const auto lead = state_->static_leads.find(label);
  return lead == state_->static_leads.end() ? 0 : static_cast<std::uint32_t>(lead->second.size());
}

const std::vector<LeadClasses>& Namer::lead_classes() const noexcept { return state_->classes; }

InstancesFound Namer::pass(const ByteSource& live, const AccessorFound& accessor, const ReadWord& read_word) const {
  const State& st = *state_;
  InstancesFound out;
  const auto in_image = [&](std::uint64_t va) { return va >= st.base && va - st.base < st.size; };
  struct Hit {
    std::uint32_t slot = 0;
    std::uint32_t vtable = 0;
  };
  std::vector<std::array<std::vector<Hit>, 4>> hits(st.leads.size());  // by lead, by how it matched
  const std::vector<StaticFound> statics = all_statics(accessor);
  for (std::size_t n = 0; n < statics.size(); ++n) {
    const StaticFound& s = statics[n];
    // A static only a variant reaches: what it holds is said for itself, whatever it is.
    out.held.push_back(NewStatic{s.rva, Holds::Nothing, 0, false});
    NewStatic* fresh = &out.held.back();
    ++out.tried;
    std::array<std::byte, 8> raw{};
    std::uint64_t instance = 0;
    if (live.read(s.rva, raw)) std::memcpy(&instance, raw.data(), 8);
    if (instance == 0) {
      ++out.null;
      continue;
    }
    // An object the game keeps inside its own image is read through the image's view, as the slot was; one on the
    // heap through the caller's guarded read.
    std::optional<std::uint64_t> word;
    if (in_image(instance)) {
      ++out.in_image;
      std::array<std::byte, 8> first{};
      if (live.read(instance - st.base, first)) {
        std::uint64_t v = 0;
        std::memcpy(&v, first.data(), 8);
        word = v;
      }
    } else if (read_word) {
      word = read_word(instance);
    }
    if (!word) {
      ++out.unread;
      fresh->holds = Holds::Unread;
      continue;
    }
    const std::uint32_t rva = in_image(*word) ? static_cast<std::uint32_t>(*word - st.base) : 0;
    const auto known = rva != 0 ? st.vtables.find(rva) : st.vtables.end();
    if (known == st.vtables.end() || !known->second) {
      ++out.unclassed;
      fresh->holds = Holds::Unclassed;
      continue;
    }
    ++out.classed;
    const auto range = st.accepts.equal_range(rva);
    fresh->holds = Holds::Classed;
    fresh->vtable = rva;
    fresh->lead = range.first != range.second;
    for (auto it = range.first; it != range.second; ++it) {
      hits[it->second.lead][it->second.match].push_back(Hit{s.rva, rva});
    }
  }
  // The statics only a variant reaches, apart: the same findings, where a reviewer looks for them first.
  for (std::size_t n = accessor.statics.size(); n < out.held.size(); ++n) out.new_statics.push_back(out.held[n]);
  for (std::size_t i = 0; i < st.leads.size(); ++i) {
    ++out.leads;
    const State::Lead& lead = st.leads[i];
    // The class itself (by its name, or its name with Imp) before a class that only derives from it.
    std::vector<Hit> own = hits[i][State::kExact];
    own.insert(own.end(), hits[i][State::kImp].begin(), hits[i][State::kImp].end());
    const bool by_base = own.empty() && !hits[i][State::kBase].empty();
    const bool by_related = own.empty() && !by_base;
    const std::vector<Hit>& chosen = by_base ? hits[i][State::kBase] : by_related ? hits[i][State::kRelated] : own;
    const Hit first = chosen.empty() ? Hit{}
                                     : *std::min_element(chosen.begin(), chosen.end(),
                                                         [](const Hit& a, const Hit& b) { return a.slot < b.slot; });
    if (!lead.listed) {
      ++out.unlisted;
      out.others.push_back(OtherLead{lead.label, static_cast<std::uint32_t>(chosen.size()), first.slot, first.vtable});
      continue;
    }
    ManagerFound m;
    m.label = lead.label;
    m.id = lead.id;
    if (!chosen.empty()) {
      m.status = chosen.size() == 1 ? derive::Status::Derived : derive::Status::Ambiguous;
      m.rva = first.slot;
      m.vtable = first.vtable;
      m.by_base = by_base;
      m.by_related = by_related;
      m.imp = !by_base && !by_related && hits[i][State::kExact].empty();
      for (std::size_t k = 0; k < statics.size(); ++k) {
        if (statics[k].rva != m.rva) continue;
        m.sites = statics[k].sites;
        m.witnesses = statics[k].witnesses;
        m.variant = k >= accessor.statics.size();
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

std::vector<std::uint32_t> Namer::vtables(std::uint32_t id) const {
  std::vector<std::uint32_t> out;
  for (const auto& [rva, accepts] : state_->accepts) {
    if (state_->leads[accepts.lead].listed && state_->leads[accepts.lead].id == id) out.push_back(rva);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

std::vector<std::uint32_t> Namer::primary_tables() const {
  std::vector<std::uint32_t> out;
  for (const auto& [rva, primary] : state_->vtables) {
    if (primary) out.push_back(rva);
  }
  return out;  // the map's order: ascending
}

std::vector<Namer::LeadTable> Namer::lead_tables() const {
  std::vector<LeadTable> out;
  for (const auto& [rva, accepts] : state_->accepts) {
    const State::Lead& lead = state_->leads[accepts.lead];
    out.push_back(LeadTable{rva, lead.listed ? lead.id : 0, lead.label});
  }
  return out;
}

namespace {

void recount(InstancesFound& held) {
  held.named = 0;
  held.ambiguous = 0;
  for (const ManagerFound& m : held.managers) {
    held.named += m.status == derive::Status::Derived ? 1 : 0;
    held.ambiguous += m.status == derive::Status::Ambiguous ? 1 : 0;
  }
}

// What the searches found for a manager goes with it from pass to pass.
void keep_searches(ManagerFound& m, const ManagerFound& before) {
  m.search_found = before.search_found;
  m.search_runs = before.search_runs;
  m.search_candidates = before.search_candidates;
  m.search_checked = before.search_checked;
  m.search_unreadable = before.search_unreadable;
  m.search_objects = before.search_objects;
  m.search_slots = before.search_slots;
  m.search_refs = before.search_refs;
  m.in_data = before.in_data;
  m.in_data_rva = before.in_data_rva;
}

}  // namespace

void set_by_search(InstancesFound& held, std::uint32_t id, const SlotSearchResult& result) {
  for (ManagerFound& m : held.managers) {
    if (m.id != id) continue;
    ++m.search_runs;
    m.search_candidates = result.candidates;
    m.search_checked = result.checked;
    m.search_unreadable = result.unreadable;
    // A run that finds nothing after one that found something does not undo it: the earlier slots stay reported.
    if (result.slots.empty() && m.search_found != 0) continue;
    m.search_found = static_cast<std::uint32_t>(result.slots.size());
    m.search_objects = result.objects;
    m.search_slots = result.slots;
    std::sort(m.search_slots.begin(), m.search_slots.end());
    if (m.search_slots.size() > 16) m.search_slots.resize(16);
    if (result.slots.empty()) continue;
    if (m.status == derive::Status::Derived && !m.by_search) continue;  // a static of the accessor's holds it
    m.status = result.objects == 1 ? derive::Status::Derived : derive::Status::Ambiguous;
    m.rva = m.search_slots.front();
    m.vtable = result.vtable;
    m.by_search = true;
    m.variant = false;
    m.by_base = false;
    m.by_related = false;
    m.imp = false;
    m.sites = 0;
    m.witnesses.clear();
    m.pass = held.passes;
  }
  recount(held);
}

void set_search_refs(InstancesFound& held, std::uint32_t id, const std::vector<std::uint32_t>& counts,
                     const std::vector<std::vector<std::uint32_t>>& places) {
  for (ManagerFound& m : held.managers) {
    if (m.id != id || counts.size() != m.search_slots.size() || places.size() != counts.size()) continue;
    m.search_refs = counts;
    if (!m.by_search || m.status != derive::Status::Derived || counts.empty()) continue;
    const std::size_t best =
        static_cast<std::size_t>(std::max_element(counts.begin(), counts.end()) - counts.begin());  // the first of equals
    m.rva = m.search_slots[best];
    m.sites = counts[best];
    m.witnesses = places[best];
    if (m.witnesses.size() > kMaxSiteWitnesses) m.witnesses.resize(kMaxSiteWitnesses);
  }
}

std::vector<References> references(const ImageInfo& image, const ByteSource& source, std::string_view section,
                                   std::span<const std::uint32_t> rvas, std::size_t max) {
  std::vector<References> out(rvas.size());
  if (rvas.empty()) return out;
  const ImageBytes img(image, source);
  const SectionCopy* text = img.named(section);
  if (text == nullptr || text->bytes.size() < 8) return out;
  const std::size_t size = text->bytes.size();
  const auto limit = static_cast<std::uint32_t>(text->rva + size);
  std::vector<std::uint32_t> last(rvas.size(), 0);
  // Every place where four bytes, read as a displacement from the instruction's end, lead to a slot (the end is
  // right after them, or after an immediate of 1, 2 or 4 bytes); then the instruction that holds them, looked for a
  // few bytes back and read as the resolver reads a witness.
  for (std::size_t p = 0; p + 4 <= size; ++p) {
    std::int32_t disp = 0;
    std::memcpy(&disp, text->bytes.data() + p, 4);
    const std::int64_t reach = static_cast<std::int64_t>(text->rva) + static_cast<std::int64_t>(p) + 4 + disp;
    for (std::size_t k = 0; k < rvas.size(); ++k) {
      const std::int64_t gap = static_cast<std::int64_t>(rvas[k]) - reach;
      if (gap != 0 && gap != 1 && gap != 2 && gap != 4) continue;
      // The instruction that holds those four bytes. First by reading on from the start of the function they lie
      // in, which keeps to the instructions' own borders; where that does not arrive at them (data among the code),
      // by the longest reading a few bytes back (a reading that leaves a prefix off is an instruction too).
      const auto here = static_cast<std::uint32_t>(text->rva + p);
      std::optional<std::uint32_t> found;
      const auto refers = [&](std::uint32_t at) {
        const std::optional<x86::Instruction> insn = code::detail::step(*text, at, limit);
        if (!insn || !insn->rip_relative || insn->disp_size != 4 || at + insn->disp_at != here) return false;
        const std::optional<std::uint32_t> to = code::rip_target(img, *text, code::Decoded{at, *insn});
        return to && *to == rvas[k];
      };
      for (std::uint32_t at = code::sync_point(image, *text, here); at <= here;) {
        const std::optional<x86::Instruction> insn = code::detail::step(*text, at, limit);
        if (!insn || insn->length == 0) break;
        if (at + insn->length > here) {
          if (refers(at)) found = at;
          break;
        }
        at += insn->length;
      }
      for (std::uint32_t back = 4; !found && back >= 1 && back <= p; --back) {
        if (refers(here - back)) found = here - back;
      }
      if (!found || (out[k].count != 0 && *found == last[k])) continue;
      last[k] = *found;
      ++out[k].count;
      if (out[k].places.size() < max) out[k].places.push_back(*found);
    }
  }
  return out;
}

void set_in_data(InstancesFound& held, std::uint32_t id, std::uint32_t count, std::uint32_t first) {
  for (ManagerFound& m : held.managers) {
    if (m.id != id) continue;
    m.in_data = count;
    m.in_data_rva = count != 0 ? first : 0;
  }
}

InstancesFound name_instances(const ImageInfo& image, const ByteSource& live, const AccessorFound& accessor,
                              const derive::Leads& leads, const Catalogue& ids, const ReadWord& read_word) {
  return Namer(image, live, leads, ids).pass(live, accessor, read_word);
}

void merge(InstancesFound& held, const InstancesFound& later) {
  const std::uint32_t pass = held.passes + 1;
  std::vector<ManagerFound> managers = std::move(held.managers);
  // The counts and what each static held are the fullest pass's: a pass made as the game closes finds most statics
  // empty and is not what the round found.
  const bool fuller = held.passes == 0 || later.null <= held.null;
  InstancesFound before;
  if (!fuller) before = std::move(held);
  held = later;
  if (!fuller) {
    held.tried = before.tried;
    held.null = before.null;
    held.unread = before.unread;
    held.in_image = before.in_image;
    held.unclassed = before.unclassed;
    held.classed = before.classed;
    held.others = std::move(before.others);
    held.new_statics = std::move(before.new_statics);
    held.held = std::move(before.held);
  }
  held.passes = pass;
  for (ManagerFound& m : held.managers) {
    const auto it = std::find_if(managers.begin(), managers.end(), [&](const ManagerFound& o) { return o.id == m.id; });
    if (it == managers.end()) {
      if (m.status == derive::Status::Derived) m.pass = pass;
      continue;
    }
    if (it->status == derive::Status::Derived && it->by_search && m.status == derive::Status::Derived) {
      // A pass names what the search named first: the accessor's static with its sites is the stronger finding
      // (the search can run in the moment between a save loading and the next pass).
      m.pass = pass;
      keep_searches(m, *it);
    } else if (it->status == derive::Status::Derived) {
      m = *it;  // named once, named for the round
    } else if (m.status == derive::Status::Derived) {
      m.pass = pass;
      keep_searches(m, *it);
    } else if (it->by_search) {
      m = *it;  // the search found two objects of its class: that stands until a pass names it
    } else {
      keep_searches(m, *it);
    }
  }
  recount(held);
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
      // A static of the accessor's is given by two recipes that agree; one the search found, by its instance alone.
      e.found_by = m.by_search ? std::vector<Recipe>{Recipe::InstanceClass}
                               : std::vector<Recipe>{Recipe::Accessor, Recipe::InstanceClass};
      e.witnesses = m.witnesses;
      if (e.witnesses.size() > kMaxWitnesses) e.witnesses.resize(kMaxWitnesses);
      e.checks = m.by_search ? std::vector<ItemCheck>{ItemCheck::Class}
                             : std::vector<ItemCheck>{ItemCheck::Agree, ItemCheck::Class};
      entries.push_back(std::move(e));
    }
  }
  for (const InvariantFound& v : found.invariants) {
    const IdEntry* id = ids.find(v.label, true);
    if (id == nullptr || id->kind != IdKind::Static || id->status != IdStatus::Active || id->number != v.id) continue;
    ItemEntry e;
    e.label = id->label;
    e.id = id->number;
    e.kind = IdKind::Static;
    e.rva = v.rva;
    e.found_by = {Recipe::Invariant};
    e.witnesses = v.witnesses;
    if (e.witnesses.size() > kMaxWitnesses) e.witnesses.resize(kMaxWitnesses);
    entries.push_back(std::move(e));
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

// ---- The report: schema 18, kind `lodestar`. No key takes a pattern, and there is no free string. ----

namespace {

using report::Key;
using report::Type;
using report::Writer;

constexpr std::array<std::string_view, 7> kStatus = {"derived",    "ambiguous",  "refused", "missing",
                                                     "not_unique", "needs_slot", "depends"};
constexpr std::array<std::string_view, 4> kGate = {"known", "no_version", "unknown_build", "fingerprint_differs"};
constexpr std::array<std::string_view, 3> kExpect = {"within", "below", "above"};
constexpr std::array<std::string_view, 4> kDiffers = {"narrow", "other_register", "other", "undecodable"};
constexpr std::array<std::string_view, 4> kHolds = {"nothing", "unread", "unclassed", "classed"};
constexpr std::array<std::string_view, 6> kWordKinds = {"zero", "small", "vtable", "image", "heap", "other"};
constexpr std::array<std::string_view, 3> kJumpAs = {"fraction", "whole", "hour"};
constexpr std::array<std::string_view, 3> kTreeMatch = {"start", "index", "address"};
constexpr std::array<std::string_view, 2> kSheetOf = {"save_data", "sheet"};
constexpr std::array<std::string_view, 7> kCheck = {"is",         "equal",   "with_player", "block",
                                                    "bits_equal", "changed", "bit_at_zero"};
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
    AU("witnesses"), AU("getters"), AO("variants"), U("known"), U("agreeing"),
    // the comparison with the withdrawn pattern: counts only
    O("pattern"), B("cut"), U("both"), U("only_shape"), U("only_pattern"), AO("why"), U("step"),
    W("differs", kDiffers), AO("only_pattern_statics"),
    // classes and the groups' table
    AO("classes"), Key{"label", Type::Label}, W("status", kStatus), U("type_descriptor"), U("classes_derived"),
    AU("vtables"), O("instances"), U("tried"), U("null"), U("unread"), U("unclassed"), U("classed"), U("leads"),
    U("unlisted"), U("named"), U("ambiguous"), U("passes"), AO("managers"), U("id"), U("vtable"), B("imp"), U("pass"),
    B("by_base"), AO("lead_classes"), U("named_imp"), U("vtable_count"), U("texts"), U("in_image"), B("by_search"),
    U("search_found"), B("variant"), U("search_runs"), U("search_candidates"), U("search_checked"),
    U("search_unreadable"), U("search_objects"), AU("search_slots"), U("in_data"), U("in_data_rva"), U("related"),
    AO("others"), U("held"), B("by_related"), AU("search_refs"), AO("new_statics"), W("holds", kHolds), B("lead"),
    // The reader's trial (in game)
    O("trial"), U("steps"), U("readings"), U("elapsed_ms"), U("reading_p50_us"), U("reading_p99_us"),
    U("reading_max_us"), U("copies"), U("fault_implausible"), U("fault_stack"), U("fault_copy"), U("fault_budget"),
    U("fault_thread"), U("fault_injected"), U("same"), U("other"), U("fault"), U("torn"), U("world_readings"),
    U("stage_changes"), U("null_runs"), U("null_longest"), U("null_world"),
    // The searches by what an object holds (in game)
    O("clock"), U("candidates"), U("unreadable"), AO("clock_hits"), U("offset"), U("width"), U("ticks"), U("wraps"),
    B("has_hour"), U("hour_offset"), U("hour_steps"), O("rising"), AO("rising_hits"), U("rose"), U("still"),
    U("risen"), O("search"), U("pointer_like"), U("live"), U("not_committed"), U("no_access"), U("asked"),
    U("refused_gone"), U("refused_access"), U("copy_failed"), U("steps_cut"), O("members"), U("objects"),
    U("pointers"), AO("member_hits"), B("embedded"),
    // The object scan (in game)
    O("scan"), U("regions"), U("mib"), U("sweeps"), B("done"), B("own_known"), AO("nodes"), U("level"), U("parent"),
    U("holders"), U("slots"), U("slot"), B("is_static"), AO("held_statics"),
    // The watch (in game)
    O("watch"), AO("watched"), B("deref"), B("for_clock"), U("empty"), U("moved"), U("vtable_same"), U("same_address"),
    U("span"), AO("words"), W("kind", kWordKinds), U("flips"), U("changes_low"), U("rises_low"), U("changes_high"),
    U("rises_high"), AO("bit_hits"), U("bit"), U("jumps"), U("hour_bit"), AO("rise_hits"), U("rises"), B("whole"),
    U("rate_micro"), AO("hour_hits"), AO("jump_events"), W("as", kJumpAs), U("second"), U("from"), U("to"),
    B("deeper"), U("via"), B("words_left_out"), U("wide_ways"), AO("invariants"),
    // The sweep (in game)
    O("sweep"), AO("targets"), U("bytes"), U("changed_words"), U("busy_words"), U("changed_pages"),
    U("quiet_pages"), U("single_flips"),
    U("other_changes"), AO("flip_events"), B("set"), AU("mark_seconds"), AO("mark_flips"), U("mark"),
    B("retired"),
    // The tree beside the flags (in game)
    O("tree"), U("records"), U("off_shape"), B("capped"), U("order"), AO("fields"), U("nonzero"), U("distinct"),
    U("lowest"), U("highest"), U("multiples"), U("addresses"), B("ordered"), AO("joins"), U("target"), U("flip"),
    W("match", kTreeMatch), U("field"), AU("values"), B("reached"), U("depth"), U("key_field"),
    AU("divisor_at"), AU("block_at"),
    // EldenNet's own reader on the flags (in game)
    O("backend"), B("discovered"), B("flags_group"), B("ready"), U("items"), U("table_items"), U("table_refused"),
    U("calls"), U("reads"), B("known_read"), B("known_set"), B("tripped"), U("reader_faults"), AO("flags"),
    U("flag"), B("always_set"), B("expected"), B("valid"), B("agrees"),
    // The state search (in game)
    O("state"), B("title_clean"), AU("alive"), U("fields_after_title"), U("fields_after_world"),
    U("fields_after_again"), AO("state_hits"), U("value"), U("slots_hit"), AO("looks"), U("took_ms"), B("through"),
    B("overrun"), U("again"), U("same_as"), U("slots_differ"), U("fields_differ"),
    // The character search (in game)
    O("chars"), U("manager_bytes"), U("pointer_words"), U("targets_read"), B("targets_cut"), U("characters"),
    U("players"), AO("char_pointers"), B("player"), AO("lists"), B("three"), U("chr_at"), U("elements"),
    B("has_player"), B("has_distance"), U("distance_at"), AO("sets"), U("player_bytes"), U("members_total"),
    AO("player_members"), U("save_bytes"), U("save_members_total"), AO("save_members"), B("has_fraction"),
    U("fraction_at"),
    // The pair search (in game)
    O("pair"), U("first"), U("objects_first"), U("objects_second"), U("words_first"), U("bytes_first"),
    U("word_hits"), U("byte_hits"), AO("pair_hits"),
    // The parts watch (in game)
    O("parts"), U("player_word"), U("holder_at"), U("absent"), U("restarts"), U("phase"), AU("phase_passes"),
    AO("part_list"), U("part"), B("fraction"), AU("changed"), U("moves_total"), AO("moves"), U("falls_total"),
    AO("falls"), U("fell"), B("has_max"), U("max_at"),
    // EldenNet's own reader on the world's leads (in game). hp, max_hp and slot are numbers of the tester's own
    // character, written with the owner's word; no key holds a position.
    O("world"), U("lacks"), U("state_frames"), U("slot_frames"), U("player_frames"), U("hp_frames"), U("hp_over"),
    U("pos_frames"), U("list_frames"), U("near_most"), U("near_read"), U("near_chr"), U("near_distance"),
    U("near_rising"), AU("pos_frames_by"), AU("pos_moved_by"), AO("at_marks"), B("there"), B("loaded"), B("has_hp"),
    U("hp"), U("max_hp"), U("near"),
    // A character's other fields through the layout: counts only.
    U("fields_bound"), U("fields_frames"), U("chr_self"), U("chr_object"), U("chr_handle"), U("chr_param"),
    U("chr_kin"), U("chr_entity"), U("chr_dead"), U("chr_on_screen"), U("handles_shared"), U("objects_most"),
    U("handles_most"), U("kinds_most"), U("entities_most"), U("teams_most"), U("team_group_most"), U("self_frames"),
    U("self_handle_frames"), U("self_param_frames"), U("self_dead_frames"), U("self_team_alone_frames"),
    AU("chr_object_by"), AU("chr_dead_by"), AU("chr_on_screen_by"),
    U("chr_block"), U("chr_block_self"), U("chr_type_main"), U("types_most"), U("self_type_none_frames"),
    U("self_block_frames"), U("zero_fields_frames"), U("dead_at_zero"), AU("self_frames_by"), AU("self_dead_by"),
    AU("self_block_changes_by"), AU("calls_by"), AU("player_by"), AU("hp_zero_by"), U("clock_frames"),
    U("clock_steps"), U("clock_same"), U("clock_jumps"), U("play_frames"), U("play_rose"), U("play_fell"),
    U("sheet_frames"), U("level_frames"), U("level_changes"), U("name_frames"), U("name_changes"),
    // The sheet scan (in game). `first` and `last` under `numbers` are numbers of the tester's own character,
    // written with the owner's word (O122); no key holds a text, and a run that reads as one is told by its place.
    O("sheet"), B("sheet_classed"), B("sheet_is_first"), B("sheet_is_second"), AO("sheets"), W("sheet_of", kSheetOf),
    U("text_fields"), U("address_fields"), U("restless"), AO("numbers"), U("last"), U("changes"), U("phases"),
    AO("text_runs"), U("run_fields"), B("numbers_cut"), U("rose_once_total"), AO("rose_once"), B("from_nothing"), U("part_passes"), AO("matches"),
    U("anchor"), U("equal_passes"), U("changed_together"),
    // The character survey (in game): behaviour only, no value of any character.
    O("survey"), U("tracked"), U("most_listed"), U("deaths"), U("fields_total"), AO("survey_fields"), U("seen"),
    U("changed_for"), U("most"), U("zeros"), U("small"), U("block_like"), U("id_like"), B("player_alone"),
    AU("handles"), AO("pairs"), U("a"), U("b"), U("holds_for"), U("of"), U("dead_total"), AO("dead_bits"),
    U("agree_alive"), U("agree_dead"), U("against"), U("screen_total"), AO("screen_bits"), U("look"),
    U("flipped_in"), AO("checks"), W("check", kCheck), U("player_holds"), U("player_of"), U("sheet_same"),
    U("sheet_other"), U("sheet_none"),
    O("table"), U("entries"), U("stride"), B("has_name_offset"), U("name_offset"), B("wide"), U("index"),
});

constexpr report::Vocabulary kVocabulary{"lodestar", 22, kKeys, report::kMaxLargeReportBytes};

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

void statics(Writer& w, std::string_view key, const std::vector<StaticFound>& list) {
  w.begin_array(key);
  for (const StaticFound& s : list) {
    w.begin_object();
    w.number("rva", s.rva);
    w.number("class_object", s.class_object);
    w.number("sites", s.sites);
    numbers(w, "witnesses", s.witnesses);
    w.end_object();
  }
  w.end_array();
}

void refusals(Writer& w, const std::array<std::uint64_t, 9>& refused) {
  w.begin_array("refused");
  for (std::size_t i = 0; i < refused.size() && i < kRefusal.size(); ++i) {
    if (refused[i] == 0) continue;
    w.begin_object();
    w.word("reason", kRefusal[i]);
    w.number("count", refused[i]);
    w.end_object();
  }
  w.end_array();
}

std::string_view expect_word(Expect e) {
  const auto i = static_cast<std::size_t>(e);
  return kExpect[i < kExpect.size() ? i : 0];
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
  refusals(w, a.refused);
  w.word("expect", expect_word(a.expect));
  w.number("statics_found", a.statics_found);
  w.number("class_objects", a.class_objects);
  w.number("shared_class_objects", a.shared_class_objects);
  w.number("shared_statics", a.shared_statics);
  statics(w, "statics", a.statics);
  numbers(w, "getters", a.getters);
  w.begin_array("variants");
  for (const VariantFound& v : a.variants) {
    w.begin_object();
    w.label("label", v.label);
    w.number("hits", v.hits);
    w.number("accepted", v.accepted);
    refusals(w, v.refused);
    w.word("expect", expect_word(v.expect));
    w.number("statics_found", v.statics_found);
    w.number("known", v.known);
    w.number("agreeing", v.agreeing);
    w.number("shared_class_objects", v.shared_class_objects);
    statics(w, "statics", v.statics);
    w.end_object();
  }
  w.end_array();
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
  statics(w, "only_pattern_statics", a.only_pattern_statics);
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

  w.begin_array("lead_classes");
  for (const LeadClasses& c : found.lead_classes) {
    w.begin_object();
    w.number("id", c.id);
    w.label("label", c.label);
    w.number("named", c.named);
    w.number("named_imp", c.named_imp);
    w.number("vtable_count", c.vtables);
    w.number("texts", c.texts);
    w.number("related", c.related);
    w.end_object();
  }
  w.end_array();

  if (found.instances) {
    const InstancesFound& n = *found.instances;
    w.begin_object("instances");
    w.number("tried", n.tried);
    w.number("null", n.null);
    w.number("unread", n.unread);
    w.number("in_image", n.in_image);
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
      w.boolean("by_base", m.by_base);
      w.boolean("by_related", m.by_related);
      w.boolean("variant", m.variant);
      w.boolean("by_search", m.by_search);
      w.number("search_found", m.search_found);
      w.number("search_runs", m.search_runs);
      w.number("search_candidates", m.search_candidates);
      w.number("search_checked", m.search_checked);
      w.number("search_unreadable", m.search_unreadable);
      w.number("search_objects", m.search_objects);
      numbers(w, "search_slots", m.search_slots);
      numbers(w, "search_refs", m.search_refs);
      w.number("in_data", m.in_data);
      w.number("in_data_rva", m.in_data_rva);
      w.number("pass", m.pass);
      numbers(w, "witnesses", m.witnesses);
      w.end_object();
    }
    w.end_array();
    w.begin_array("others");
    for (const OtherLead& o : n.others) {
      w.begin_object();
      w.label("label", o.label);
      w.number("held", o.held);
      w.number("rva", o.rva);
      w.number("vtable", o.vtable);
      w.end_object();
    }
    w.end_array();
    for (const auto& [key, list] : {std::pair<std::string_view, const std::vector<NewStatic>*>{"new_statics", &n.new_statics},
                                    std::pair<std::string_view, const std::vector<NewStatic>*>{"held_statics", &n.held}}) {
      w.begin_array(key);
      for (const NewStatic& x : *list) {
        w.begin_object();
        w.number("rva", x.rva);
        const auto holds = static_cast<std::size_t>(x.holds);
        w.word("holds", kHolds[holds < kHolds.size() ? holds : 0]);
        w.number("vtable", x.vtable);
        w.boolean("lead", x.lead);
        w.end_object();
      }
      w.end_array();
    }
    w.end_object();
  }

  if (found.trial) {
    const TrialFound& t = *found.trial;
    static constexpr std::array<std::string_view, kTrialFaultKinds> kFaultKeys = {
        "fault_implausible", "fault_stack", "fault_copy", "fault_budget", "fault_thread", "fault_injected"};
    w.begin_object("trial");
    w.number("steps", t.steps);
    w.number("readings", t.readings);
    w.number("world_readings", t.world_readings);
    w.number("stage_changes", t.stage_changes);
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
      w.number("null_runs", m.null_runs);
      w.number("null_longest", m.null_longest);
      w.number("null_world", m.null_world);
      w.number("same", m.same);
      w.number("other", m.other);
      w.number("fault", m.fault);
      w.number("torn", m.torn);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.clock) {
    const ClockFound& c = *found.clock;
    w.begin_object("clock");
    w.number("candidates", c.candidates);
    w.number("passes", c.passes);
    w.number("unreadable", c.unreadable);
    w.begin_array("clock_hits");
    for (const ClockHit& h : c.hits) {
      w.begin_object();
      w.number("rva", h.rva);
      w.number("offset", h.offset);
      w.number("width", h.width);
      w.number("ticks", h.ticks);
      w.number("wraps", h.wraps);
      w.boolean("has_hour", h.has_hour);
      w.number("hour_offset", h.hour_offset);
      w.number("hour_steps", h.hour_steps);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }
  if (found.rising) {
    const RisingFound& r = *found.rising;
    w.begin_object("rising");
    w.number("candidates", r.candidates);
    w.number("passes", r.passes);
    w.number("unreadable", r.unreadable);
    w.number("risen", r.risen);
    w.begin_array("rising_hits");
    for (const RisingHit& h : r.hits) {
      w.begin_object();
      w.number("rva", h.rva);
      w.number("offset", h.offset);
      w.number("rose", h.rose);
      w.number("still", h.still);
      w.number("other", h.other);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.search) {
    const SearchFound& f = *found.search;
    w.begin_object("search");
    w.number("pointer_like", f.pointer_like);
    w.number("live", f.live);
    w.number("not_committed", f.not_committed);
    w.number("no_access", f.no_access);
    w.number("asked", f.asked);
    w.number("refused_gone", f.refused_gone);
    w.number("refused_access", f.refused_access);
    w.number("copy_failed", f.copy_failed);
    w.number("steps", f.steps);
    w.number("steps_cut", f.steps_cut);
    w.end_object();
  }
  if (found.members) {
    const MembersFound& f = *found.members;
    w.begin_object("members");
    w.number("objects", f.objects);
    w.number("unreadable", f.unreadable);
    w.number("pointers", f.pointers);
    w.number("passes", f.passes);
    w.begin_array("member_hits");
    for (const MemberHit& h : f.hits) {
      w.begin_object();
      w.number("id", h.id);
      w.label("label", h.label);
      w.number("rva", h.rva);
      w.number("offset", h.offset);
      w.boolean("embedded", h.embedded);
      w.number("vtable", h.vtable);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.scan) {
    const ScanFound& f = *found.scan;
    w.begin_object("scan");
    w.boolean("done", f.done);
    w.boolean("own_known", f.own_known);
    w.number("regions", f.regions);
    w.number("mib", f.mib);
    w.number("unreadable", f.unreadable);
    w.number("sweeps", f.sweeps);
    w.number("elapsed_ms", f.elapsed_ms);
    w.begin_array("nodes");
    for (const ScanNode& x : f.nodes) {
      w.begin_object();
      w.number("level", x.level);
      w.number("id", x.id);
      w.label("label", x.label);
      w.number("vtable", x.vtable);
      w.number("parent", x.parent);
      w.number("offset", x.offset);
      w.boolean("embedded", x.embedded);
      w.number("holders", x.holders);
      w.number("slots", x.slots);
      w.number("slot", x.slot);
      w.boolean("is_static", x.is_static);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.watch) {
    const WatchFound& f = *found.watch;
    w.begin_object("watch");
    w.number("passes", f.passes);
    w.number("wide_ways", f.wide_ways);
    w.begin_array("watched");
    for (const WatchObject& o : f.objects) {
      w.begin_object();
      w.label("label", o.label);
      w.number("id", o.id);
      w.number("rva", o.rva);
      w.number("offset", o.offset);
      w.boolean("deref", o.deref);
      w.boolean("deeper", o.deeper);
      w.number("via", o.via);
      w.boolean("for_clock", o.clock);
      w.boolean("wide", o.wide);
      w.boolean("words_left_out", o.words_left_out);
      w.number("readings", o.readings);
      w.number("empty", o.empty);
      w.number("unreadable", o.unreadable);
      w.number("moved", o.moved);
      w.number("vtable_same", o.vtable_same);
      w.number("same_address", o.same_address);
      w.number("span", o.span);
      w.number("vtable", o.vtable);
      w.begin_array("words");
      for (const WatchWord& x : o.words) {
        w.begin_object();
        w.number("offset", x.offset);
        const auto kind = static_cast<std::size_t>(x.kind);
        w.word("kind", kWordKinds[kind < kWordKinds.size() ? kind : 5]);
        // A count of nothing is left out: most words never change, and a long round's report must stay small.
        if (x.flips != 0) w.number("flips", x.flips);
        if (x.changes_low != 0) w.number("changes_low", x.changes_low);
        if (x.rises_low != 0) w.number("rises_low", x.rises_low);
        if (x.changes_high != 0) w.number("changes_high", x.changes_high);
        if (x.rises_high != 0) w.number("rises_high", x.rises_high);
        w.end_object();
      }
      w.end_array();
      w.begin_array("bit_hits");
      for (const BitHit& h : o.bits) {
        w.begin_object();
        w.number("offset", h.offset);
        w.number("bit", h.bit);
        w.number("ticks", h.ticks);
        w.number("wraps", h.wraps);
        w.number("jumps", h.jumps);
        w.boolean("has_hour", h.has_hour);
        w.number("hour_offset", h.hour_offset);
        w.number("hour_bit", h.hour_bit);
        w.number("hour_steps", h.hour_steps);
        w.end_object();
      }
      w.end_array();
      w.begin_array("rise_hits");
      for (const RiseHit& h : o.rises) {
        w.begin_object();
        w.number("offset", h.offset);
        w.boolean("whole", h.whole);
        w.number("rises", h.rises);
        w.number("jumps", h.jumps);
        w.number("rate_micro", h.rate_micro);
        w.end_object();
      }
      w.end_array();
      w.begin_array("hour_hits");
      for (const HourHit& h : o.hours) {
        w.begin_object();
        w.number("offset", h.offset);
        w.number("bit", h.bit);
        w.number("steps", h.steps);
        w.number("jumps", h.jumps);
        w.end_object();
      }
      w.end_array();
      w.begin_array("jump_events");
      for (const JumpEvent& e : o.jumps) {
        w.begin_object();
        w.number("offset", e.offset);
        w.number("bit", e.bit);
        const auto as = static_cast<std::size_t>(e.as);
        w.word("as", kJumpAs[as < kJumpAs.size() ? as : 0]);
        w.number("second", e.second);
        w.number("from", e.from);
        w.number("to", e.to);
        w.end_object();
      }
      w.end_array();
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.sweep) {
    const SweepFound& f = *found.sweep;
    w.begin_object("sweep");
    w.number("passes", f.passes);
    w.begin_array("mark_seconds");
    for (const std::uint32_t s : f.mark_seconds) w.number(s);
    w.end_array();
    w.begin_array("targets");
    for (const SweepTarget& t : f.targets) {
      w.begin_object();
      w.label("label", t.label);
      w.number("id", t.id);
      w.number("rva", t.rva);
      w.number("offset", t.offset);
      w.boolean("deeper", t.deeper);
      w.number("via", t.via);
      w.number("bytes", t.bytes);
      w.number("readings", t.readings);
      w.number("empty", t.empty);
      w.number("unreadable", t.unreadable);
      w.number("moved", t.moved);
      w.number("changed_words", t.changed_words);
      w.number("busy_words", t.busy_words);
      w.number("changed_pages", t.changed_pages);
      w.number("quiet_pages", t.quiet_pages);
      w.number("single_flips", t.single_flips);
      w.number("other_changes", t.other_changes);
      w.boolean("retired", t.retired);
      w.begin_array("mark_flips");
      for (const MarkFlip& e : t.mark_flips) {
        w.begin_object();
        w.number("mark", e.mark);
        w.number("offset", e.flip.offset);
        w.number("bit", e.flip.bit);
        w.boolean("set", e.flip.set);
        w.number("second", e.flip.second);
        w.end_object();
      }
      w.end_array();
      w.begin_array("flip_events");
      for (const SweepFlip& e : t.flips) {
        w.begin_object();
        w.number("offset", e.offset);
        w.number("bit", e.bit);
        w.boolean("set", e.set);
        w.number("second", e.second);
        w.end_object();
      }
      w.end_array();
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.tree) {
    const TreeFound& t = *found.tree;
    w.begin_object("tree");
    w.number("rva", t.rva);
    w.number("offset", t.offset);
    w.boolean("done", t.done);
    w.boolean("capped", t.capped);
    w.number("records", t.records);
    w.number("off_shape", t.off_shape);
    w.number("unreadable", t.unreadable);
    w.number("order", t.order);
    w.number("key_field", t.key_field);
    w.begin_array("divisor_at");
    for (const std::uint32_t v : t.divisor_at) w.number(v);
    w.end_array();
    w.begin_array("block_at");
    for (const std::uint32_t v : t.block_at) w.number(v);
    w.end_array();
    w.begin_array("fields");
    for (const TreeField& f : t.fields) {
      w.begin_object();
      w.number("offset", f.offset);
      w.number("nonzero", f.nonzero);
      w.number("distinct", f.distinct);
      w.number("lowest", f.lowest);
      w.number("highest", f.highest);
      w.number("multiples", f.multiples);
      w.number("addresses", f.addresses);
      w.boolean("ordered", f.rising);
      w.end_object();
    }
    w.end_array();
    w.begin_array("joins");
    for (const TreeJoin& j : t.joins) {
      w.begin_object();
      w.number("mark", j.mark);
      w.number("target", j.target);
      w.number("flip", j.flip);
      w.number("bit", j.bit);
      const auto match = static_cast<std::size_t>(j.match);
      w.word("match", kTreeMatch[match < kTreeMatch.size() ? match : 0]);
      w.number("field", j.field);
      w.begin_array("values");
      for (const std::uint32_t v : j.values) w.number(v);
      w.end_array();
      w.boolean("reached", j.reached);
      w.number("depth", j.depth);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.backend) {
    const BackendFound& b = *found.backend;
    w.begin_object("backend");
    w.boolean("discovered", b.discovered);
    w.boolean("flags_group", b.flags_group);
    w.boolean("ready", b.ready);
    w.number("items", b.items);
    w.number("table_items", b.table_items);
    w.number("table_refused", b.table_refused);
    w.number("calls", b.calls);
    w.number("reads", b.reads);
    w.boolean("known_read", b.known_read);
    w.boolean("known_set", b.known_set);
    w.boolean("tripped", b.tripped);
    w.number("reader_faults", b.reader_faults);
    w.begin_array("flags");
    for (const BackendFlag& f : b.flags) {
      w.begin_object();
      w.number("flag", f.flag);
      w.boolean("always_set", f.known);
      w.boolean("expected", f.expected);
      w.boolean("valid", f.valid);
      w.boolean("set", f.set);
      w.boolean("agrees", f.valid && f.set == f.expected);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.state) {
    const StateFound& f = *found.state;
    w.begin_object("state");
    w.number("candidates", f.candidates);
    w.number("asked", f.asked);
    w.number("passes", f.passes);
    w.boolean("title_clean", f.title_clean);
    w.boolean("overrun", f.overrun);
    w.begin_array("looks");
    for (const StateLook& l : f.looks) {
      w.begin_object();
      w.number("took_ms", l.took_ms);
      w.boolean("through", l.through);
      w.boolean("overrun", l.overrun);
      w.end_object();
    }
    w.end_array();
    w.begin_array("alive");
    for (const std::uint32_t v : f.alive) w.number(v);
    w.end_array();
    w.number("fields_after_title", f.fields_after_title);
    w.number("fields_after_world", f.fields_after_world);
    w.number("fields_after_again", f.fields_after_again);
    w.number("slots_hit", f.slots_hit);
    w.number("slots_differ", f.slots_differ);
    w.number("fields_differ", f.fields_differ);
    w.begin_array("state_hits");
    for (const StateHit& h : f.hits) {
      w.begin_object();
      w.number("rva", h.rva);
      w.number("offset", h.offset);
      w.number("value", h.value);
      w.number("again", h.again);
      w.number("same_as", h.same_as);
      w.number("sites", h.sites);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.chars) {
    const CharFound& f = *found.chars;
    const auto members = [&](std::string_view name, const std::vector<CharMember>& list) {
      w.begin_array(name);
      for (const CharMember& m : list) {
        w.begin_object();
        w.number("offset", m.offset);
        w.number("vtable", m.vtable);
        w.number("pointers", m.pointers);
        w.number("classed", m.classed);
        w.end_object();
      }
      w.end_array();
    };
    w.begin_object("chars");
    w.number("rva", f.rva);
    w.number("asked", f.asked);
    w.number("passes", f.passes);
    w.boolean("overrun", f.overrun);
    w.begin_array("looks");
    for (const CharLook& l : f.looks) {
      w.begin_object();
      w.number("mark", l.mark);
      w.boolean("through", l.through);
      w.boolean("overrun", l.overrun);
      w.boolean("cut", l.cut);
      w.number("took_ms", l.took_ms);
      w.number("reads", l.reads);
      w.number("manager_bytes", l.manager_bytes);
      w.number("pointer_words", l.pointer_words);
      w.number("targets_read", l.targets_read);
      w.boolean("targets_cut", l.targets_cut);
      w.number("characters", l.characters);
      w.number("players", l.players);
      w.begin_array("char_pointers");
      for (const CharPointer& c : l.pointers) {
        w.begin_object();
        w.number("offset", c.offset);
        w.boolean("player", c.player);
        w.end_object();
      }
      w.end_array();
      w.begin_array("lists");
      for (const CharList& c : l.lists) {
        w.begin_object();
        w.number("offset", c.offset);
        w.number("bytes", c.bytes);
        w.boolean("three", c.three);
        w.number("stride", c.stride);
        w.number("chr_at", c.chr_at);
        w.number("elements", c.elements);
        w.number("characters", c.characters);
        w.boolean("has_player", c.has_player);
        w.boolean("has_distance", c.has_distance);
        w.number("distance_at", c.distance_at);
        w.boolean("has_fraction", c.has_fraction);
        w.number("fraction_at", c.fraction_at);
        w.boolean("expected", c.expected);
        w.end_object();
      }
      w.end_array();
      w.begin_array("sets");
      for (const CharSet& c : l.sets) {
        w.begin_object();
        w.number("offset", c.offset);
        w.number("chr_at", c.chr_at);
        w.number("vtable", c.vtable);
        w.end_object();
      }
      w.end_array();
      w.number("player_bytes", l.player_bytes);
      w.number("members_total", l.members_total);
      members("player_members", l.members);
      w.number("save_bytes", l.save_bytes);
      w.number("save_members_total", l.save_members_total);
      members("save_members", l.save_members);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.pair) {
    const PairFound& f = *found.pair;
    w.begin_object("pair");
    w.number("first", f.first);
    w.number("second", f.second);
    w.number("candidates", f.candidates);
    w.number("asked", f.asked);
    w.number("passes", f.passes);
    w.boolean("overrun", f.overrun);
    w.begin_array("looks");
    for (const StateLook& l : f.looks) {
      w.begin_object();
      w.number("took_ms", l.took_ms);
      w.boolean("through", l.through);
      w.boolean("overrun", l.overrun);
      w.end_object();
    }
    w.end_array();
    w.number("objects_first", f.objects_first);
    w.number("objects_second", f.objects_second);
    w.number("words_first", f.words_first);
    w.number("bytes_first", f.bytes_first);
    w.number("word_hits", f.word_hits);
    w.number("byte_hits", f.byte_hits);
    w.number("leads", f.leads);
    w.begin_array("pair_hits");
    for (const PairHit& h : f.hits) {
      w.begin_object();
      w.number("rva", h.rva);
      w.number("offset", h.offset);
      w.number("width", h.width);
      w.number("sites", h.sites);
      w.boolean("lead", h.lead);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  if (found.parts) {
    const PartsFound& f = *found.parts;
    w.begin_object("parts");
    w.number("player_word", f.player_word);
    w.number("holder_at", f.holder_at);
    w.number("passes", f.passes);
    w.number("absent", f.absent);
    w.number("restarts", f.restarts);
    w.number("phase", f.phase);
    w.begin_array("phase_passes");
    for (const std::uint32_t v : f.phase_passes) w.number(v);
    w.end_array();
    w.begin_array("part_list");
    for (const PartFound& p : f.parts) {
      w.begin_object();
      w.number("index", p.index);
      w.number("vtable", p.vtable);
      w.number("bytes", p.bytes);
      w.end_object();
    }
    w.end_array();
    w.number("moves_total", f.moves_total);
    w.begin_array("moves");
    for (const PartMove& m : f.moves) {
      w.begin_object();
      w.number("part", m.part);
      w.number("offset", m.offset);
      w.boolean("fraction", m.fraction);
      w.number("span", m.span);
      w.begin_array("changed");
      for (const std::uint32_t v : m.changed) w.number(v);
      w.end_array();
      w.end_object();
    }
    w.end_array();
    w.number("falls_total", f.falls_total);
    w.begin_array("falls");
    for (const PartFall& p : f.falls) {
      w.begin_object();
      w.number("part", p.part);
      w.number("offset", p.offset);
      w.number("fell", p.fell);
      w.number("rose", p.rose);
      w.boolean("has_max", p.has_max);
      w.number("max_at", p.max_at);
      w.end_object();
    }
    w.end_array();
    w.end_object();
  }

  w.begin_array("invariants");
  for (const InvariantFound& v : found.invariants) {
    w.begin_object();
    w.number("id", v.id);
    w.label("label", v.label);
    w.number("rva", v.rva);
    w.number("offset", v.offset);
    w.number("rose", v.rose);
    w.number("passes", v.passes);
    w.number("sites", v.sites);
    numbers(w, "witnesses", v.witnesses);
    w.end_object();
  }
  w.end_array();

  if (found.survey) {
    const SurveyFound& f = *found.survey;
    w.begin_object("survey");
    w.number("passes", f.passes);
    w.number("absent", f.absent);
    w.number("tracked", f.tracked);
    w.number("most_listed", f.most_listed);
    w.number("phase", f.phase);
    w.begin_array("phase_passes");
    for (const std::uint32_t v : f.phase_passes) w.number(v);
    w.end_array();
    w.number("deaths", f.deaths);
    w.number("fields_total", f.fields_total);
    w.begin_array("survey_fields");
    for (const SurveyField& r : f.fields) {
      w.begin_object();
      w.number("offset", r.offset);
      w.number("seen", r.seen);
      w.number("changed_for", r.changed_for);
      w.number("distinct", r.distinct);
      w.number("most", r.most);
      w.number("zeros", r.zeros);
      w.number("small", r.small);
      w.number("addresses", r.addresses);
      w.number("block_like", r.block_like);
      w.number("id_like", r.id_like);
      w.boolean("player_alone", r.player_alone);
      w.end_object();
    }
    w.end_array();
    w.begin_array("handles");
    for (const std::uint32_t v : f.handles) w.number(v);
    w.end_array();
    w.begin_array("pairs");
    for (const SurveyPair& r : f.pairs) {
      w.begin_object();
      w.number("a", r.a);
      w.number("b", r.b);
      w.number("holds_for", r.holds);
      w.number("of", r.of);
      w.end_object();
    }
    w.end_array();
    w.number("dead_total", f.dead_total);
    w.begin_array("dead_bits");
    for (const SurveyDead& r : f.dead_bits) {
      w.begin_object();
      w.number("offset", r.offset);
      w.number("bit", r.bit);
      w.boolean("set", r.set);
      w.number("agree_alive", r.agree_alive);
      w.number("agree_dead", r.agree_dead);
      w.number("against", r.against);
      w.end_object();
    }
    w.end_array();
    w.number("screen_total", f.screen_total);
    w.begin_array("screen_bits");
    for (const SurveyFlip& r : f.screen_bits) {
      w.begin_object();
      w.number("offset", r.offset);
      w.number("bit", r.bit);
      w.number("look", r.look);
      w.number("passes", r.passes);
      w.number("still", r.still);
      w.number("flipped_in", r.flipped_in);
      w.end_object();
    }
    w.end_array();
    w.begin_array("checks");
    for (const SurveyCheck& r : f.checks) {
      w.begin_object();
      w.word("check", kCheck[std::min<std::size_t>(r.kind, kCheck.size() - 1)]);
      w.number("a", r.a);
      w.number("b", r.b);
      w.number("holds_for", r.holds);
      w.number("of", r.of);
      w.number("player_holds", r.player_holds);
      w.number("player_of", r.player_of);
      w.end_object();
    }
    w.end_array();
    w.number("sheet_same", f.sheet_same);
    w.number("sheet_other", f.sheet_other);
    w.number("sheet_none", f.sheet_none);
    w.end_object();
  }

  if (found.world) {
    const WorldRead& f = *found.world;
    w.begin_object("world");
    w.number("lacks", f.lacks);
    w.number("calls", f.calls);
    w.number("state_frames", f.state_frames);
    w.number("slot_frames", f.slot_frames);
    w.number("player_frames", f.player_frames);
    w.number("hp_frames", f.hp_frames);
    w.number("hp_over", f.hp_over);
    w.number("pos_frames", f.pos_frames);
    w.number("list_frames", f.list_frames);
    w.number("near_most", f.near_most);
    w.number("near_read", f.near_read);
    w.number("near_chr", f.near_chr);
    w.number("near_distance", f.near_distance);
    w.number("near_rising", f.near_rising);
    w.begin_array("pos_frames_by");
    for (const std::uint32_t v : f.pos_frames_by) w.number(v);
    w.end_array();
    w.begin_array("pos_moved_by");
    for (const std::uint32_t v : f.pos_moved_by) w.number(v);
    w.end_array();
    w.begin_array("at_marks");
    for (const WorldAt& a : f.at_marks) {
      w.begin_object();
      w.number("mark", a.mark);
      w.boolean("there", a.there);
      w.boolean("loaded", a.loaded);
      w.number("slot", a.slot);
      w.boolean("player", a.player);
      w.boolean("has_hp", a.has_hp);
      w.number("hp", a.hp);
      w.number("max_hp", a.max_hp);
      w.number("near", a.near_all);
      w.end_object();
    }
    w.end_array();
    w.number("fields_bound", f.fields_bound);
    w.number("fields_frames", f.fields_frames);
    w.number("chr_self", f.chr_self);
    w.number("chr_object", f.chr_object);
    w.number("chr_handle", f.chr_handle);
    w.number("chr_param", f.chr_param);
    w.number("chr_kin", f.chr_kin);
    w.number("chr_entity", f.chr_entity);
    w.number("chr_dead", f.chr_dead);
    w.number("chr_on_screen", f.chr_on_screen);
    w.number("handles_shared", f.handles_shared);
    w.number("objects_most", f.objects_most);
    w.number("handles_most", f.handles_most);
    w.number("kinds_most", f.kinds_most);
    w.number("entities_most", f.entities_most);
    w.number("teams_most", f.teams_most);
    w.number("team_group_most", f.team_group_most);
    w.number("self_frames", f.self_frames);
    w.number("self_handle_frames", f.self_handle_frames);
    w.number("self_param_frames", f.self_param_frames);
    w.number("self_dead_frames", f.self_dead_frames);
    w.number("self_team_alone_frames", f.self_team_alone_frames);
    numbers(w, "chr_object_by", f.chr_object_by);
    numbers(w, "chr_dead_by", f.chr_dead_by);
    numbers(w, "chr_on_screen_by", f.chr_on_screen_by);
    w.number("chr_block", f.chr_block);
    w.number("chr_block_self", f.chr_block_self);
    w.number("chr_type_main", f.chr_type_main);
    w.number("types_most", f.types_most);
    w.number("self_type_none_frames", f.self_type_none_frames);
    w.number("self_block_frames", f.self_block_frames);
    w.number("zero_fields_frames", f.zero_fields_frames);
    w.number("dead_at_zero", f.dead_at_zero);
    numbers(w, "self_frames_by", f.self_frames_by);
    numbers(w, "self_dead_by", f.self_dead_by);
    numbers(w, "self_block_changes_by", f.self_block_changes_by);
    numbers(w, "calls_by", f.calls_by);
    numbers(w, "player_by", f.player_by);
    numbers(w, "hp_zero_by", f.hp_zero_by);
    w.number("clock_frames", f.clock_frames);
    w.number("clock_steps", f.clock_steps);
    w.number("clock_same", f.clock_same);
    w.number("clock_jumps", f.clock_jumps);
    w.number("play_frames", f.play_frames);
    w.number("play_rose", f.play_rose);
    w.number("play_fell", f.play_fell);
    w.number("sheet_frames", f.sheet_frames);
    w.number("level_frames", f.level_frames);
    w.number("level_changes", f.level_changes);
    w.number("name_frames", f.name_frames);
    w.number("name_changes", f.name_changes);
    w.end_object();
  }

  if (found.sheet) {
    const SheetFound& f = *found.sheet;
    w.begin_object("sheet");
    w.number("passes", f.passes);
    w.number("absent", f.absent);
    w.number("phase", f.phase);
    numbers(w, "phase_passes", f.phase_passes);
    w.boolean("sheet_classed", f.sheet_classed);
    w.boolean("sheet_is_first", f.sheet_is_first);
    w.boolean("sheet_is_second", f.sheet_is_second);
    w.number("part_passes", f.part_passes);
    w.begin_array("matches");
    for (const SheetMatch& r : f.matches) {
      w.begin_object();
      w.number("anchor", r.anchor);
      w.number("offset", r.offset);
      w.number("equal_passes", r.equal);
      w.number("of", r.of);
      w.number("changed_together", r.together);
      w.end_object();
    }
    w.end_array();
    w.begin_array("sheets");
    for (const SheetObject& o : f.objects) {
      w.begin_object();
      w.word("sheet_of", kSheetOf[std::min<std::size_t>(o.which, kSheetOf.size() - 1)]);
      w.boolean("there", o.there);
      w.number("bytes", o.bytes);
      w.number("passes", o.passes);
      w.number("moved", o.moved);
      w.number("fields_total", o.fields_total);
      w.number("text_fields", o.text_fields);
      w.number("address_fields", o.address_fields);
      w.number("restless", o.restless);
      w.boolean("numbers_cut", o.cut);
      w.begin_array("numbers");
      for (const SheetField& r : o.fields) {
        w.begin_object();
        w.number("offset", r.offset);
        w.number("first", r.first);
        w.number("last", r.last);
        w.number("changes", r.changes);
        w.number("phases", r.phases);
        w.end_object();
      }
      w.end_array();
      w.begin_array("text_runs");
      for (const SheetText& r : o.texts) {
        w.begin_object();
        w.number("offset", r.offset);
        w.number("run_fields", r.fields);
        w.end_object();
      }
      w.end_array();
      w.number("rose_once_total", o.rises_total);
      w.begin_array("rose_once");
      for (const SheetRise& r : o.rises) {
        w.begin_object();
        w.number("offset", r.offset);
        w.number("phases", r.phases);
        w.boolean("from_nothing", r.from_nothing);
        w.end_object();
      }
      w.end_array();
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
