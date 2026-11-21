// Discovery in game (discovery.hpp; ARCH §3.2, §6.2): the gate, the AOB items, the FD4 singletons and the RTTI sets over
// the mapped image, from gal/'s files. Portable: NativeSys shows it the game's image, FakeSys a test's.
#include "en/gal/discovery.hpp"

#include <algorithm>
#include <map>

#include "en/gal/fd4.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/recipes.hpp"
#include "en/gal/rtti.hpp"

namespace en::gal {

using proto::Expected;
using proto::Group;
using proto::Unexpected;
using roles::Cls;
using roles::Item;
using roles::ItemKind;

void VtableSet::assign(std::vector<std::uint64_t> vtables) {
  v_ = std::move(vtables);
  std::sort(v_.begin(), v_.end());
  v_.erase(std::unique(v_.begin(), v_.end()), v_.end());
}

bool VtableSet::contains(std::uint64_t vtable) const noexcept { return std::binary_search(v_.begin(), v_.end(), vtable); }

const char* to_string(ItemStatus s) noexcept {
  switch (s) {
    case ItemStatus::Resolved: return "resolved";
    case ItemStatus::Missing: return "missing";
    case ItemStatus::Ambiguous: return "ambiguous";
    case ItemStatus::Refused: return "refused";
  }
  return "missing";
}

Expected<Gated, GateFailure> gate_image(const SysLayer& sys, std::string_view builds_text) {
  auto image = read_image(sys.image(), ImageLayout::Memory);
  if (!image) return Unexpected{GateFailure{GateFailure::What::Image, describe(image.error())}};
  auto builds = parse_builds(builds_text);
  if (!builds) {
    const SchemaErrors& e = builds.error();
    return Unexpected{GateFailure{GateFailure::What::BuildsFile, e.empty() ? "builds.toml refused" : describe(e.front())}};
  }
  auto profile = gate(*image, *builds);
  if (!profile) return Unexpected{GateFailure{GateFailure::What::Gate, describe(profile.error())}};
  return Gated{std::move(*image), std::move(*profile)};
}

Discovery::Discovery(ImageInfo image, BuildProfile profile, std::unique_ptr<Layout> layout)
    : image_(std::move(image)), profile_(std::move(profile)), layout_(std::move(layout)) {
  bound_ = std::make_unique<roles::Bound>(*layout_);
}

std::optional<std::uint64_t> Discovery::address(Item item) const noexcept {
  const ItemResult& r = report_.items[roles::index(item)];
  if (r.status != ItemStatus::Resolved) return std::nullopt;
  return r.address;
}

proto::GroupSet Discovery::groups() const noexcept {
  proto::GroupSet out = proto::GroupSet::None;
  for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
    if (report_.groups[g] == GroupWhy::None) out = out | proto::bit_of(static_cast<Group>(g));
  }
  return out;
}

namespace {

constexpr std::array<Item, roles::kItemCount> kAllItems = {
    Item::RegisterTask, Item::ModuleHandle, Item::SaveData, Item::GameState, Item::FieldArea, Item::TaskHub,
    Item::Characters,   Item::Session,      Item::EventFlags, Item::Hud,     Item::Menus,     Item::Loading,
    Item::WorldClock,   Item::Camera,       Item::ParamStore, Item::Messages};

std::optional<Item> item_named(std::string_view label) {
  for (const Item i : kAllItems) {
    if (roles::info(i).label == label) return i;
  }
  return std::nullopt;
}

const char* why_of(HitProblem p) {
  switch (p) {
    case HitProblem::NoSection: return "its section is not in the image";
    case HitProblem::Count: return "its pattern matched other than once";
    case HitProblem::Unscanned: return "part of its section could not be read";
    case HitProblem::NoFunction: return "its hit lies in no function";
    case HitProblem::Fragment: return "its hit lies in a fragment of a function";
    case HitProblem::NearEntry: return "its hit lies before entry + anchor_min";
    case HitProblem::PastEnd: return "its hit runs past its function";
    case HitProblem::Length: return "its function's length is outside fn_len";
    case HitProblem::Prologue: return "its prologue differs";
    case HitProblem::Unreadable: return "bytes a check needs could not be read";
    case HitProblem::TargetSection: return "its target lies outside its section";
    case HitProblem::TargetAlignment: return "its target is not aligned";
    case HitProblem::TargetNotFunction: return "its target does not start a function";
    case HitProblem::Ambiguous: return "its hits disagree";
  }
  return "refused";
}

// A static's slot from the probe round: in a writable data section, aligned to 8.
bool slot_plausible(const ImageInfo& image, std::uint32_t rva) {
  const Section* s = image.section_at(rva);
  return s != nullptr && rva % 8 == 0 && (s->characteristics & kSectionWrite) != 0 &&
         (s->characteristics & kSectionExecute) == 0 && rva <= s->rva + s->mapped_size() - 8;
}

// The name a getter's answer points at: printable ASCII of 1 to 128 bytes within the image (ARCH §6.2 step 3).
std::optional<std::string> name_at(const ImageBytes& img, std::uint64_t base, std::uint64_t address) {
  if (address < base || address - base > 0xFFFF'FFFFull) return std::nullopt;
  return img.ascii(static_cast<std::uint32_t>(address - base), 128);
}

}  // namespace

Expected<Discovery, SchemaErrors> discover(SysLayer& sys, const Gated& gated, std::string_view patterns_text,
                                           std::string_view layout_text, const DiscoveryOptions& options) {
  const std::uint64_t t0 = sys.now_us();
  // The files first: a refusal is permanent.
  std::optional<RecipeSet> recipes;
  if (!options.recipes.empty()) {
    auto parsed = parse_recipes(options.recipes, {});
    if (!parsed) return Unexpected{std::move(parsed.error())};
    recipes = std::move(*parsed);
  }
  const RecipeSpec* accessor = recipes ? recipes->find(roles::kAccessorRecipeId) : nullptr;
  std::vector<std::string_view> required = {roles::info(Item::RegisterTask).label};
  if (accessor == nullptr) required.push_back(roles::kFd4IdiomId);  // the idiom, where no recipe replaces it
  for (const Item i : kAllItems) {
    const roles::ItemInfo& info = roles::info(i);
    if (info.kind != ItemKind::Static || !info.required) continue;
    const bool slotted = std::any_of(options.static_slots.begin(), options.static_slots.end(),
                                     [&](const auto& s) { return s.first == info.label; });
    if (!slotted) required.push_back(info.label);
  }
  auto patterns = parse_patterns(patterns_text, required);
  if (!patterns) return Unexpected{std::move(patterns.error())};
  const LayoutNeeds needs{{}, {}, {}, true};
  auto layout = load_layout(gated.profile, layout_text, needs, options.mode);
  if (!layout) return Unexpected{std::move(layout.error())};

  Discovery d(gated.image, gated.profile, std::make_unique<Layout>(std::move(*layout)));
  DiscoveryReport& r = d.report_;
  r.layout_has_leads = d.layout().has_leads();
  d.base_ = sys.module_base();
  const std::uint64_t base = d.base_;
  for (ItemResult& item : r.items) item = ItemResult{ItemStatus::Missing, 0, 0, 0, false, "no pattern or site found"};

  // ---- The AOB items: one pass over each section ----
  const ScanReport scan = scan_patterns(d.image(), sys.image(), *patterns);
  r.unscanned = scan.unscanned;
  for (const PatternResult& p : scan.results) {
    r.pattern_hits += p.count;
    const auto item = item_named(p.id);
    if (!item) continue;  // the idiom, or a pattern of no item of ours
    ItemResult& out = r.items[roles::index(*item)];
    const ItemKind kind = roles::info(*item).kind;
    if (!p.resolved()) {
      const HitProblem problem = p.problem.value_or(HitProblem::Count);
      out.status = problem == HitProblem::Count && p.count == 0 ? ItemStatus::Missing
                   : problem == HitProblem::Ambiguous            ? ItemStatus::Ambiguous
                                                                 : ItemStatus::Refused;
      out.why = why_of(problem);
      continue;
    }
    if (kind == ItemKind::Function && p.kind == PatternKind::Function) {
      out = ItemResult{ItemStatus::Resolved, base + p.entry, p.entry, 0, false, ""};
    } else if (kind == ItemKind::Static && p.kind == PatternKind::Static) {
      out = ItemResult{ItemStatus::Resolved, base + p.target, p.target, 0, false, ""};
    } else {
      out.status = ItemStatus::Refused;
      out.why = "its pattern is of another kind than the item";
    }
  }
  // The probe round's slots, where the item's pattern did not resolve it.
  for (const auto& [label, rva] : options.static_slots) {
    const auto item = item_named(label);
    if (!item || roles::info(*item).kind != ItemKind::Static) continue;
    ItemResult& out = r.items[roles::index(*item)];
    if (out.status == ItemStatus::Resolved) continue;
    if (slot_plausible(d.image(), rva)) {
      out = ItemResult{ItemStatus::Resolved, base + rva, rva, 0, true, ""};
    } else {
      out = ItemResult{ItemStatus::Refused, 0, rva, 0, true, "its slot is not 8-aligned in a writable data section"};
    }
  }

  // ---- The image, read once, for the FD4 finder and the RTTI walk ----
  const ImageBytes img(d.image(), sys.image());
  for (const SectionCopy& s : img.sections()) r.image_unreadable += s.unreadable;

  // ---- The FD4 singletons ----
  // By the accessor's shape where recipes.toml gives one (no byte of the game), else by patterns.toml's idiom.
  const PatternSpec* idiom_spec = accessor == nullptr ? patterns->find(roles::kFd4IdiomId) : nullptr;
  const std::optional<fd4::Idiom> idiom = idiom_spec != nullptr ? fd4::idiom_from(*idiom_spec) : std::nullopt;
  if (accessor != nullptr || idiom) {
    fd4::Config config;
    config.path_suffix = accessor != nullptr ? accessor->path_suffix : idiom_spec->path_suffix;
    if (accessor != nullptr) config.code = accessor->section;
    const bool name_layout = gated.profile.entry().dlrf_name_layout && d.layout().dlrf().has_value();
    if (name_layout) config.names = fd4::NameLayout{d.layout().dlrf()->name_ptr_off, fd4::NameEncoding::Ascii};
    const fd4::Result found =
        accessor != nullptr ? fd4::find(img, accessor->accessor(), config) : fd4::find(img, *idiom, config);
    r.fd4_hits = found.hits;
    struct Named {
      std::uint32_t static_slot = 0;
      std::uint32_t sites = 0;
      bool ambiguous = false;
    };
    std::map<std::string, Named, std::less<>> names;
    for (const fd4::Site& site : found.sites) {
      if (site.refused) continue;
      ++r.fd4_accepted;
      std::optional<std::string> name = site.name;
      if (!name_layout) {
        // The init-only fallback: the getter, called once for this candidate, under SEH (ARCH §6.2 step 3).
        ++r.getter_calls;
        const auto answer = sys.call_getter(base + site.getter, base + site.class_object);
        if (answer) name = name_at(img, base, *answer);
      }
      if (!name) continue;
      Named& n = names[*name];
      if (n.sites > 0 && n.static_slot != site.static_slot) n.ambiguous = true;
      if (n.sites == 0) n.static_slot = site.static_slot;
      ++n.sites;
    }
    r.fd4_names = names.size();
    for (const Item i : kAllItems) {
      if (roles::info(i).kind != ItemKind::Singleton) continue;
      ItemResult& out = r.items[roles::index(i)];
      const ClassLayout* cls = d.bound().cls(*roles::class_of(i));
      if (cls == nullptr || cls->singleton.empty()) {
        out.why = "its layout class names no singleton";
        continue;
      }
      const auto it = names.find(cls->singleton);
      if (it == names.end()) {
        out.why = "no accepted FD4 site gives its name";
      } else if (it->second.ambiguous) {
        out = ItemResult{ItemStatus::Ambiguous, 0, 0, it->second.sites, false, "its name has two statics"};
      } else {
        out = ItemResult{ItemStatus::Resolved, base + it->second.static_slot, it->second.static_slot, it->second.sites,
                         false, ""};
      }
    }
  } else {
    for (const Item i : kAllItems) {
      if (roles::info(i).kind == ItemKind::Singleton) r.items[roles::index(i)].why = "no accessor recipe and no fd4_idiom";
    }
  }

  // ---- The RTTI sets ----
  std::array<bool, roles::kClsCount> rtti_missing{};
  bool any_rtti = false;
  for (std::size_t c = 0; c < roles::kClsCount; ++c) {
    const ClassLayout* cls = d.bound().cls(static_cast<Cls>(c));
    any_rtti = any_rtti || (cls != nullptr && !cls->rtti.empty());
  }
  if (any_rtti) {
    const rtti::Walk walk = rtti::walk(img);
    r.rtti_locators = walk.locators.size();
    r.rtti_vtables = walk.vtables.size();
    for (std::size_t c = 0; c < roles::kClsCount; ++c) {
      const ClassLayout* cls = d.bound().cls(static_cast<Cls>(c));
      if (cls == nullptr || cls->rtti.empty()) continue;
      const rtti::Found td = rtti::find_type_descriptor(img, cls->rtti);
      if (td.count != 1) {
        rtti_missing[c] = true;
        ++r.rtti_missing;
        continue;
      }
      const rtti::Set set = rtti::derived_from(walk, td.rva, false);
      std::vector<std::uint64_t> vts;
      vts.reserve(set.primary.size());
      for (const std::uint32_t v : set.primary) vts.push_back(base + v);
      if (vts.empty()) {
        rtti_missing[c] = true;
        ++r.rtti_missing;
        continue;
      }
      d.sets_[c].assign(std::move(vts));
      ++r.rtti_classes;
    }
  }

  // ---- The groups ----
  const auto item_ok = [&](Item i) { return r.items[roles::index(i)].status == ItemStatus::Resolved; };
  for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
    const auto group = static_cast<Group>(g);
    r.groups[g] = GroupWhy::None;
    r.group_detail[g] = "";
    if (group == Group::Status) {
      r.groups[g] = GroupWhy::Off;
      r.group_detail[g] = "status is off until S8 verifies it (O40)";
      continue;
    }
    for (const Item i : roles::items_of(group)) {
      if (!item_ok(i)) {
        r.groups[g] = GroupWhy::NotDiscovered;
        r.group_detail[g] = roles::info(i).label.data();
        break;
      }
    }
    if (r.groups[g] != GroupWhy::None) continue;
    if (const auto gap = d.bound().gap(group)) {
      r.groups[g] = GroupWhy::NotDiscovered;
      r.group_detail[g] = gap->problem;
      continue;
    }
    // A class of the group whose RTTI the layout names but the image does not hold: its objects cannot be checked.
    for (std::size_t f = 0; f < roles::kFldCount; ++f) {
      const roles::FieldSpec& s = roles::spec(static_cast<roles::Fld>(f));
      if (s.use.boot || s.use.group != group) continue;
      if (rtti_missing[static_cast<std::size_t>(s.cls)]) {
        r.groups[g] = GroupWhy::NotDiscovered;
        r.group_detail[g] = "a class of the group names RTTI the image does not hold";
        break;
      }
    }
  }

  // ---- Ready to register? ----
  r.ready = true;
  for (const Item i : kAllItems) {
    if (roles::info(i).required && !item_ok(i)) {
      r.ready = false;
      r.failure = "a required item is missing";
    }
  }
  if (r.ready && (!d.layout().task() || d.layout().task()->partial || d.bound().boot_gap())) {
    r.ready = false;
    r.failure = "the layout's task, or the task hub's group table, is incomplete";
  }
  if (r.ready && rtti_missing[static_cast<std::size_t>(Cls::TaskHub)]) {
    r.ready = false;
    r.failure = "the task hub's RTTI is not in the image";
  }
  for (const Group g : {Group::Session, Group::World, Group::Player}) {
    if (r.ready && !d.has(g)) {
      r.ready = false;
      r.failure = "a structural group cannot run on this image and layout";
    }
  }
  r.pass_us = sys.now_us() - t0;
  return d;
}

Retried discover_with_retries(SysLayer& sys, const Gated& gated, std::string_view patterns_text,
                              std::string_view layout_text, const DiscoveryOptions& options,
                              const std::function<void(std::uint64_t)>& wait_until_ms) {
  Retried out{discover(sys, gated, patterns_text, layout_text, options), 1};
  for (std::size_t i = 1; i < kRetryAtMs.size(); ++i) {
    if (!out.last || out.last->ready()) break;
    wait_until_ms(kRetryAtMs[i]);
    out.last = discover(sys, gated, patterns_text, layout_text, options);
    ++out.passes;
  }
  return out;
}

}  // namespace en::gal
