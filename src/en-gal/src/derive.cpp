// en-gal's derivation (derive.hpp): the offline round R0's candidates, found from the executable file.
#include "en/gal/derive.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>

#include "bytes.hpp"
#include "code.hpp"
#include "cut.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/rtti.hpp"

namespace en::gal::derive {

const char* to_string(Status status) noexcept {
  switch (status) {
    case Status::Derived: return "derived";
    case Status::Ambiguous: return "ambiguous";
    case Status::Refused: return "refused";
    case Status::Missing: return "missing";
    case Status::NotUnique: return "not_unique";
    case Status::NeedsSlot: return "needs_slot";
    case Status::Depends: return "depends";
  }
  return "missing";
}

namespace {

constexpr std::uint32_t kInitialisedData = 0x00000040;  // IMAGE_SCN_CNT_INITIALIZED_DATA
constexpr std::string_view kText = ".text";
constexpr std::string_view kData = ".data";
constexpr std::string_view kRdata = ".rdata";

struct Context {
  const ImageInfo& image;
  const ByteSource& source;
  const Leads& leads;
  const Options& options;
  const ImageBytes& img;
  const SectionCopy* text;
  std::vector<Fixup> fixups;
  Result& result;
};

bool data_section(const ImageBytes& img, const SectionCopy& c) {
  const std::uint32_t ch = img.header(c).characteristics;
  return (ch & kSectionExecute) == 0 && (ch & kInitialisedData) != 0;
}

// ---- The fingerprint ----

void fingerprint(Context& cx) {
  Fingerprint& f = cx.result.fingerprint;
  const ImageInfo& im = cx.image;
  f.image_base = im.image_base;
  f.entry_point = im.entry_point;
  f.size_of_image = im.size_of_image;
  f.size_of_headers = im.size_of_headers;
  f.section_alignment = im.section_alignment;
  f.file_alignment = im.file_alignment;
  f.time_date_stamp = im.time_date_stamp;
  f.characteristics = im.characteristics;
  f.dll_characteristics = im.dll_characteristics;
  f.subsystem = im.subsystem;
  f.functions = im.functions.size();
  f.sections = im.sections;
  f.product_name = cx.leads.product_name.empty() ? "not_given" : "absent";
  if (!im.version) return;
  f.has_version = true;
  f.file_version = im.version->file_version;
  f.product_version = im.version->product_version;
  f.region = im.version->file_version[3] == 0 ? "ww" : im.version->file_version[3] == 1 ? "jp" : "other";
  f.translation = im.version->translation;
  f.resource_language = im.version->resource_language;
  if (!cx.leads.product_name.empty() && im.version->product_name) {
    f.product_name = *im.version->product_name == cx.leads.product_name ? "matches" : "differs";
  }
}

// ---- References ----

// Every instruction of .text whose RIP-relative operand points at one of `targets`, by target.
std::map<std::uint32_t, std::vector<code::Decoded>> references(Context& cx, const std::set<std::uint32_t>& targets,
                                                               std::size_t max_each, bool count_stats) {
  std::map<std::uint32_t, std::vector<code::Decoded>> out;
  if (cx.text == nullptr || targets.empty()) return out;
  code::Stats stats;
  code::sweep(
      cx.image, *cx.text,
      [&](const code::Decoded& d) {
        if (!d.insn.rip_relative) return;
        const std::optional<std::uint32_t> t = code::rip_target(cx.img, *cx.text, d);
        if (!t || targets.count(*t) == 0) return;
        std::vector<code::Decoded>& list = out[*t];
        if (list.size() < max_each) list.push_back(d);
      },
      stats);
  if (count_stats) {
    cx.result.stats.instructions = stats.instructions;
    cx.result.stats.undecodable = stats.undecodable;
  }
  return out;
}

// ---- The FD4 idiom ----

// The path strings ending with the suffix, in .rdata.
std::vector<std::uint32_t> path_strings(const ImageBytes& img, std::string_view suffix) {
  std::vector<std::uint32_t> out;
  const SectionCopy* rdata = img.named(kRdata);
  if (rdata == nullptr || suffix.empty()) return out;
  std::vector<std::byte> needle;
  for (const char c : suffix) needle.push_back(std::byte{static_cast<unsigned char>(c)});
  needle.push_back(std::byte{0});
  const std::vector<std::byte>& b = rdata->bytes;
  auto from = b.begin();
  while (true) {
    const auto hit = std::search(from, b.end(), needle.begin(), needle.end());
    if (hit == b.end()) break;
    from = hit + 1;
    auto start = hit;
    while (start != b.begin() && hit - start < 260) {
      const auto c = std::to_integer<std::uint8_t>(*(start - 1));
      if (c < 0x20 || c >= 0x7F) break;
      --start;
    }
    const auto rva = static_cast<std::uint32_t>(rdata->rva + (start - b.begin()));
    if (fd4::path_matches(img, rva, suffix) && std::find(out.begin(), out.end(), rva) == out.end()) out.push_back(rva);
  }
  return out;
}

bool is_static_load(const x86::Instruction& in) {
  return in.encoding == x86::Encoding::Legacy && in.map == 0 && in.opcode == 0x8B && in.wide && in.rip_relative;
}

bool is_self_test(const x86::Instruction& in, std::uint8_t reg) {
  return in.encoding == x86::Encoding::Legacy && in.map == 0 && in.opcode == 0x85 && in.wide && in.mod() == 3 &&
         in.reg() == reg && in.rm() == reg;
}

bool is_jnz(const x86::Instruction& in) {
  return in.encoding == x86::Encoding::Legacy &&
         ((in.map == 0 && in.opcode == 0x75) || (in.map == 1 && in.opcode == 0x85));
}

struct SiteSeq {
  std::vector<code::Decoded> insns;  // from the static's load to the log call after the path's load
  std::size_t path = 0;              // the path's load, in insns
};

// The idiom around one reference to a path: the closest `mov reg, [rip+static]; test reg, reg; jnz` before it whose
// jump passes the reference, then on to the call after it.
std::optional<SiteSeq> site_at(Context& cx, std::uint32_t ref) {
  const std::optional<std::vector<code::Decoded>> window = code::decode_to(cx.image, *cx.text, ref);
  if (!window || window->size() < 4) return std::nullopt;
  const std::size_t r = window->size() - 1;
  const std::size_t lowest = r > 24 ? r - 24 : 0;
  for (std::size_t i0 = r - 2; i0-- > lowest;) {  // i0 + 2, the jump, comes before r
    const code::Decoded& a = (*window)[i0];
    const code::Decoded& b = (*window)[i0 + 1];
    const code::Decoded& c = (*window)[i0 + 2];
    if (!is_static_load(a.insn) || !is_self_test(b.insn, a.insn.reg()) || !is_jnz(c.insn)) continue;
    const std::optional<std::uint32_t> skip = code::branch_target(cx.img, *cx.text, c);
    if (!skip || *skip < (*window)[r].end()) continue;
    SiteSeq s;
    s.insns.assign(window->begin() + static_cast<std::ptrdiff_t>(i0), window->end());
    s.path = r - i0;
    const std::vector<code::Decoded> after =
        code::decode_range(cx.image, *cx.text, (*window)[r].end(), (*window)[r].end() + x86::kMaxLength);
    if (!after.empty() && after.front().insn.flow == x86::Flow::Call) s.insns.push_back(after.front());
    return s;
  }
  return std::nullopt;
}

// An instruction's shape: its length, map and opcode.
std::uint32_t shape_of(const code::Decoded& d) {
  return (std::uint32_t{d.insn.length} << 16) | (std::uint32_t{d.insn.map} << 8) | d.insn.opcode;
}

// The pattern of one site's first `n` instructions.
BytePattern site_pattern(Context& cx, const SiteSeq& s, std::size_t n) {
  return cut::pattern_of(cx.img, *cx.text, cx.fixups, std::span<const code::Decoded>(s.insns).first(n));
}

// The sites' shared bytes: a byte is fixed where every site fixes it to the same value, and a displacement or an
// immediate that differs anywhere is wildcarded whole.
BytePattern merge(Context& cx, const std::vector<const SiteSeq*>& sites, std::size_t n) {
  BytePattern out = site_pattern(cx, *sites.front(), n);
  for (std::size_t k = 1; k < sites.size(); ++k) {
    const BytePattern p = site_pattern(cx, *sites[k], n);
    for (std::size_t i = 0; i < out.value.size() && i < p.value.size(); ++i) {
      if (out.mask[i] == 0 || p.mask[i] == 0 || out.value[i] != p.value[i]) {
        out.mask[i] = 0;
        out.value[i] = 0;
      }
    }
  }
  std::size_t base = 0;
  for (std::size_t j = 0; j < n; ++j) {
    const x86::Instruction& in = sites.front()->insns[j].insn;
    for (const cut::Field& f : cut::fields_of(in)) {
      bool varies = false;
      for (std::size_t i = base + f.at; i < base + f.at + f.size && i < out.mask.size(); ++i)
        varies = varies || out.mask[i] == 0;
      if (!varies) continue;
      for (std::size_t i = base + f.at; i < base + f.at + f.size && i < out.mask.size(); ++i) {
        out.mask[i] = 0;
        out.value[i] = 0;
      }
    }
    base += in.length;
  }
  return out;
}

std::uint32_t offset_in(const SiteSeq& s, std::size_t index) {
  std::uint32_t off = 0;
  for (std::size_t j = 0; j < index && j < s.insns.size(); ++j) off += s.insns[j].insn.length;
  return off;
}

// The name layout: the offset in the class objects that points at an identifier in .rdata for the most of them.
void derive_names(Context& cx, const std::vector<std::uint32_t>& class_objects) {
  Fd4Result& r = cx.result.fd4;
  r.names_of = class_objects.size();
  std::uint64_t best = 0;
  for (std::uint32_t off = 0; off < 0x80; off += 8) {
    for (const fd4::NameEncoding enc : {fd4::NameEncoding::Ascii, fd4::NameEncoding::Utf16}) {
      std::uint64_t n = 0;
      for (const std::uint32_t cls : class_objects) {
        const std::optional<std::uint64_t> va = cx.img.u64(cls + off);
        const std::optional<std::uint32_t> rva = va ? cx.img.rva_of(*va) : std::nullopt;
        if (!rva || cx.img.section_name(*rva) != kRdata) continue;
        const std::optional<std::string> name = fd4::class_name(cx.img, cls, fd4::NameLayout{off, enc});
        if (name && is_identifier_text(*name)) ++n;
      }
      if (n > best) {
        best = n;
        r.names = fd4::NameLayout{off, enc};
      }
    }
  }
  r.names_agree = best;
  if (best == 0 || 2 * best < class_objects.size()) r.names.reset();
}

void derive_fd4(Context& cx) {
  Fd4Result& r = cx.result.fd4;
  // Every singleton the leads name is reported, missing until found.
  for (const NamedLead& lead : cx.leads.singletons) {
    SingletonResult missing;
    missing.label = lead.label;
    r.singletons.push_back(std::move(missing));
  }
  if (cx.text == nullptr) return;
  const std::vector<std::uint32_t> paths = path_strings(cx.img, cx.leads.fd4_path_suffix);
  r.path_strings = paths.size();
  if (paths.empty()) return;
  const std::set<std::uint32_t> targets(paths.begin(), paths.end());
  std::vector<SiteSeq> sites;
  for (const auto& [_, refs] : references(cx, targets, 1u << 16, true)) {
    r.path_references += refs.size();
    for (const code::Decoded& ref : refs) {
      if (std::optional<SiteSeq> s = site_at(cx, ref.rva)) sites.push_back(std::move(*s));
    }
  }
  r.sites = sites.size();
  if (sites.empty()) return;

  // The sites by their heads (the first five instructions' shapes); the idiom is cut from the largest group.
  std::map<std::vector<std::uint32_t>, std::vector<const SiteSeq*>> heads;
  for (const SiteSeq& s : sites) {
    if (s.insns.size() < 5) continue;
    std::vector<std::uint32_t> key;
    for (std::size_t j = 0; j < 5; ++j) key.push_back(shape_of(s.insns[j]));
    heads[key].push_back(&s);
  }
  r.shapes = heads.size();
  const std::vector<const SiteSeq*>* group = nullptr;
  for (const auto& [_, members] : heads) {
    if (group == nullptr || members.size() > group->size()) group = &members;
  }
  if (group == nullptr) return;
  r.sites_used = group->size();

  // How far every site of the group shares its shape, within 64 bytes.
  std::size_t n = group->front()->insns.size();
  for (const SiteSeq* s : *group) {
    std::size_t k = 0;
    while (k < n && k < s->insns.size() && shape_of(s->insns[k]) == shape_of(group->front()->insns[k])) ++k;
    n = k;
  }
  while (n > 5 && offset_in(*group->front(), n) > report::kMaxPatternBytes) --n;
  if (offset_in(*group->front(), n) > report::kMaxPatternBytes) return;

  // The captures: the static's load, the class object's load after the jump, the getter's call, the path's load.
  const SiteSeq& first = *group->front();
  fd4::Idiom idiom;
  idiom.bytes = merge(cx, *group, n);
  idiom.static_slot = Capture{first.insns[0].insn.disp_at, first.insns[0].insn.length};
  std::optional<std::size_t> cls;
  std::optional<std::size_t> getter;
  for (std::size_t j = 3; j < n; ++j) {
    const code::Decoded& d = first.insns[j];
    if (!cls && d.insn.rip_relative) {
      const std::optional<std::uint32_t> t = code::rip_target(cx.img, *cx.text, d);
      if (t && cx.img.section_name(*t) == kData) cls = j;
    } else if (cls && !getter && d.insn.flow == x86::Flow::Call) {
      getter = j;
    }
  }
  if (!cls || !getter) return;
  idiom.class_object = Capture{offset_in(first, *cls) + first.insns[*cls].insn.disp_at, offset_in(first, *cls + 1)};
  idiom.getter = Capture{offset_in(first, *getter) + first.insns[*getter].insn.imm_at, offset_in(first, *getter + 1)};
  const bool same_path =
      std::all_of(group->begin(), group->end(), [&](const SiteSeq* s) { return s->path == first.path; });
  if (same_path && first.path < n) {
    idiom.path =
        Capture{offset_in(first, first.path) + first.insns[first.path].insn.disp_at, offset_in(first, first.path + 1)};
  }
  if (idiom.bytes.fixed() < 4) return;

  // The idiom over .text: every hit, and the sites it was cut from among them.
  const code::Matches hits = code::find_matches(*cx.text, idiom.bytes, 1u << 16);
  r.idiom_hits = hits.count;
  const std::set<std::uint32_t> hit_set(hits.hits.begin(), hits.hits.end());
  for (const SiteSeq* s : *group) r.sites_matched += hit_set.count(s->insns.front().rva);

  // The name layout, from the class objects of the hits that pass the placement checks.
  fd4::Config config;
  config.path_suffix = cx.leads.fd4_path_suffix;
  const fd4::Result unnamed = fd4::find(cx.img, idiom, config);
  std::vector<std::uint32_t> class_objects;
  for (const fd4::Site& s : unnamed.sites) {
    if (!s.refused && std::find(class_objects.begin(), class_objects.end(), s.class_object) == class_objects.end()) {
      class_objects.push_back(s.class_object);
    }
  }
  derive_names(cx, class_objects);
  config.names = r.names;
  const fd4::Result found = fd4::find(cx.img, idiom, config);
  for (const fd4::Site& s : found.sites) {
    if (s.refused) {
      const auto i = static_cast<std::size_t>(*s.refused);
      if (i < r.refused.size()) ++r.refused[i];
    } else {
      ++r.accepted;
    }
  }
  r.unnamed = found.unnamed;

  // The leads' singletons by their names; the others only counted.
  std::set<std::string> listed;
  r.singletons.clear();
  for (const NamedLead& lead : cx.leads.singletons) {
    listed.insert(lead.name);
    SingletonResult one;
    one.label = lead.label;
    const auto it = std::find_if(found.singletons.begin(), found.singletons.end(),
                                 [&](const fd4::Singleton& s) { return s.name == lead.name; });
    if (it != found.singletons.end()) {
      one.status = it->ambiguous ? Status::Ambiguous : Status::Derived;
      one.static_slot = it->static_slot;
      one.class_object = it->class_object;
      one.getter = it->getter;
      one.sites = it->sites;
    } else {
      const auto refused = std::find_if(found.sites.begin(), found.sites.end(),
                                        [&](const fd4::Site& s) { return s.refused && s.name == lead.name; });
      if (refused != found.sites.end()) {
        one.status = Status::Refused;
        one.refusal = refused->refused;
        one.static_slot = refused->static_slot;
        one.class_object = refused->class_object;
      }
    }
    r.singletons.push_back(std::move(one));
  }
  for (const fd4::Singleton& s : found.singletons) {
    if (listed.count(s.name) != 0) continue;
    ++r.unlisted;
    if (s.ambiguous) ++r.unlisted_ambiguous;
  }

  // The idiom as patterns.toml would hold it, through part 2's scan.
  PatternSet set;
  PatternSpec spec;
  spec.id = "fd4_idiom";
  spec.kind = PatternKind::Idiom;
  spec.section = std::string(kText);
  spec.bytes = idiom.bytes;
  spec.expect_min = 1;
  spec.expect_max = kMaxPatternHits;
  spec.captures.push_back(IdiomCapture{"static", idiom.static_slot, std::string(kData), 8, false});
  spec.captures.push_back(IdiomCapture{"class", idiom.class_object, std::string(kData), 1, false});
  spec.captures.push_back(IdiomCapture{"getter", idiom.getter, std::string(kText), 1, true});
  if (idiom.path) spec.captures.push_back(IdiomCapture{"path", *idiom.path, std::string(kRdata), 1, false});
  set.patterns.push_back(spec);
  const ScanReport scan = scan_patterns(cx.image, cx.source, set);
  if (!scan.results.empty()) {
    r.scan_resolved = scan.results.front().resolved();
    r.scan_hits = scan.results.front().idiom_hits.size();
    r.scan_rejected = scan.results.front().idiom_rejected;
  }
  r.idiom = std::move(idiom);

  // The idiom and the name layout are derived when every site of the group matches and part 2's scan resolves the
  // idiom; each singleton carries its own status.
  r.status = r.names && r.scan_resolved && r.sites_matched == r.sites_used ? Status::Derived : Status::Refused;
}

// ---- RTTI ----

ClassResult class_result(Context& cx, const rtti::Walk& walk, std::string_view cls) {
  ClassResult out;
  const rtti::Found td = rtti::find_type_descriptor(cx.img, cls);
  if (td.count == 0) return out;
  out.type_descriptor = td.rva;
  if (td.count > 1) {
    out.status = Status::Ambiguous;
    return out;
  }
  const std::vector<const rtti::Vtable*> vts = rtti::vtables_of(walk, td.rva);
  out.vtables = vts.size();
  for (const rtti::Vtable* v : vts) {
    if (walk.locators[v->locator].offset == 0) {
      out.vtable = v->rva;
      out.slots = v->slots;
      out.status = Status::Derived;
      break;
    }
  }
  return out;
}

void derive_rtti(Context& cx) {
  const rtti::Walk walk = rtti::walk(cx.img);
  cx.result.stats.locators = walk.locators.size();
  cx.result.stats.vtables = walk.vtables.size();

  for (const NamedLead& lead : cx.leads.vtable_sets) {
    VtableSetResult set;
    set.label = lead.label;
    const rtti::Found td = rtti::find_type_descriptor(cx.img, lead.name);
    set.type_descriptor = td.rva;
    if (td.count > 1) {
      set.status = Status::Ambiguous;
    } else if (td.count == 1) {
      const rtti::Set found = rtti::derived_from(walk, td.rva, false);
      set.primary = found.primary;
      set.subobject = found.subobject;
      set.classes = found.classes;
      set.status = set.primary.empty() ? Status::Missing : Status::Derived;
    }
    cx.result.sets.push_back(std::move(set));
  }

  TaskResult& task = cx.result.task;
  task.base = class_result(cx, walk, cx.leads.task_base);
  if (task.base.type_descriptor != 0 && task.base.status != Status::Ambiguous) {
    const rtti::Set derived = rtti::derived_from(walk, task.base.type_descriptor, true);
    task.vtables = derived.primary;
    std::vector<std::uint32_t> slots;
    for (const std::uint32_t vt : task.vtables) {
      const auto it = std::lower_bound(walk.vtables.begin(), walk.vtables.end(), vt,
                                       [](const rtti::Vtable& v, std::uint32_t r) { return v.rva < r; });
      if (it != walk.vtables.end() && it->rva == vt) slots.push_back(it->slots);
    }
    if (!slots.empty()) {
      task.slots_min = *std::min_element(slots.begin(), slots.end());
      task.slots_max = *std::max_element(slots.begin(), slots.end());
      for (std::uint32_t i = 0; i < task.slots_min; ++i) {
        std::set<std::uint64_t> targets;
        for (const std::uint32_t vt : task.vtables) targets.insert(cx.img.u64(vt + 8 * i).value_or(0));
        task.slot_targets.push_back(static_cast<std::uint32_t>(targets.size()));
      }
      // The run slot's candidate: the one slot every class fills with a function of its own.
      std::optional<std::uint32_t> own;
      std::uint32_t count = 0;
      for (std::uint32_t i = 0; i < task.slot_targets.size(); ++i) {
        if (task.vtables.size() >= 2 && task.slot_targets[i] == task.vtables.size()) {
          own = i;
          ++count;
        }
      }
      if (count == 1) task.run_slot = own;
    }
    task.status = task.vtables.empty() ? Status::Missing : Status::Derived;
  }

  for (const StaticLead& lead : cx.leads.statics) {
    StaticResult s;
    s.label = lead.label;
    s.module = lead.module;
    if (!lead.module) s.rtti = class_result(cx, walk, lead.rtti);
    cx.result.statics.push_back(std::move(s));
  }
}

// ---- The registration ----

enum class RegState : std::uint8_t { Unknown, Instance, Wide, Narrow };

constexpr std::array<std::uint8_t, 4> kArgs = {1, 2, 8, 9};  // rcx, rdx, r8, r9

// What one instruction does to the registers, as far as the derivation models it.
void simulate(Context& cx, const code::Decoded& d, std::uint32_t instance_static, std::array<RegState, 16>& st) {
  const x86::Instruction& in = d.insn;
  const auto set = [&](std::uint8_t reg, RegState s) {
    if (reg < st.size()) st[reg] = s;
  };
  const RegState written = in.wide ? RegState::Wide : RegState::Narrow;
  if (in.encoding != x86::Encoding::Legacy) {
    if (in.has_modrm) {
      set(in.reg(), RegState::Unknown);
      if (in.mod() == 3) set(in.rm(), RegState::Unknown);
    }
    return;
  }
  const bool reg_form = in.has_modrm && in.mod() == 3;
  if (in.map == 0) {
    const std::uint8_t op = in.opcode;
    if (op == 0x8B) {
      if (in.rip_relative && in.wide && code::rip_target(cx.img, *cx.text, d) == instance_static) {
        set(in.reg(), RegState::Instance);
      } else if (reg_form) {
        set(in.reg(), in.wide && st[in.rm()] == RegState::Instance ? RegState::Instance : written);
      } else {
        set(in.reg(), written);
      }
    } else if (op == 0x89) {
      if (reg_form) set(in.rm(), in.wide && st[in.reg()] == RegState::Instance ? RegState::Instance : written);
    } else if (op == 0x8D || op == 0x63) {
      set(in.reg(), op == 0x63 ? RegState::Wide : written);
    } else if (op >= 0xB8 && op <= 0xBF) {
      set(in.opcode_reg(), written);
    } else if (op >= 0xB0 && op <= 0xB7) {
      set(in.opcode_reg(), RegState::Unknown);
    } else if (op == 0xC7 && reg_form) {
      set(in.rm(), written);
    } else if ((op == 0x31 || op == 0x33 || op == 0x29 || op == 0x2B) && reg_form && in.reg() == in.rm()) {
      set(in.reg(), written);  // zeroing
    } else if (op == 0x87 && reg_form) {
      std::swap(st[in.reg()], st[in.rm()]);
    } else if (op >= 0x91 && op <= 0x97) {
      std::swap(st[0], st[in.opcode_reg()]);
    } else if (op >= 0x58 && op <= 0x5F) {
      set(in.opcode_reg(), RegState::Unknown);
    } else if (op == 0x99) {
      set(2, RegState::Unknown);
    } else if ((op == 0xF6 || op == 0xF7) && in.has_modrm && ((in.modrm >> 3) & 7) >= 4) {
      set(0, RegState::Unknown);
      set(2, RegState::Unknown);
    } else if ((op >= 0xA4 && op <= 0xAF) && in.rep != 0) {
      set(1, RegState::Unknown);
      set(6, RegState::Unknown);
      set(7, RegState::Unknown);
    } else if (op == 0x84 || op == 0x85 || (op >= 0x38 && op <= 0x3D) || op == 0xA8 || op == 0xA9 ||
               ((op >= 0x80 && op <= 0x83) && in.has_modrm && ((in.modrm >> 3) & 7) == 7) ||
               ((op == 0xF6 || op == 0xF7) && in.has_modrm && ((in.modrm >> 3) & 7) <= 1)) {
      // compares and tests: nothing written
    } else if (op >= 0x50 && op <= 0x57) {
      // push: nothing written
    } else if ((op >= 0x80 && op <= 0x83) || op == 0xC0 || op == 0xC1 || op == 0xC6 || (op >= 0xD0 && op <= 0xD3) ||
               op == 0xF6 || op == 0xF7 || op == 0xFE || op == 0xFF || op == 0x8F) {
      // groups: ModRM's reg is part of the opcode, and only a register r/m is written
      if (reg_form) set(in.rm(), RegState::Unknown);
    } else if (in.has_modrm) {
      set(in.reg(), RegState::Unknown);
      if (reg_form) set(in.rm(), RegState::Unknown);
    }
    return;
  }
  if (in.map == 1) {
    const std::uint8_t op = in.opcode;
    if (op == 0xB6 || op == 0xB7 || op == 0xBE || op == 0xBF) {
      set(in.reg(), written);
    } else if (op == 0x05 || op == 0xA2 || op == 0x31) {
      set(1, RegState::Unknown);
      set(2, RegState::Unknown);
    } else if (in.has_modrm) {
      set(in.reg(), RegState::Unknown);
      if (reg_form) set(in.rm(), RegState::Unknown);
    }
    return;
  }
  if (in.has_modrm) {
    set(in.reg(), RegState::Unknown);
    if (reg_form) set(in.rm(), RegState::Unknown);
  }
}

struct CallSite {
  std::uint32_t call = 0;
  std::uint32_t callee = 0;
  Abi abi;
};

std::optional<CallSite> call_after_load(Context& cx, const code::Decoded& load, std::uint32_t instance_static) {
  const std::optional<std::vector<code::Decoded>> before = code::decode_to(cx.image, *cx.text, load.rva);
  if (!before) return std::nullopt;
  std::vector<code::Decoded> insns = *before;
  insns.pop_back();
  const std::size_t load_at = insns.size();
  const std::vector<code::Decoded> after = code::decode_range(cx.image, *cx.text, load.rva, load.rva + 128);
  insns.insert(insns.end(), after.begin(), after.end());
  // The first call after the load, with no other change of flow between.
  std::optional<std::size_t> call;
  for (std::size_t i = load_at; i < insns.size() && i < load_at + 16; ++i) {
    const x86::Flow f = insns[i].insn.flow;
    if (f == x86::Flow::Call) {
      call = i;
      break;
    }
    if (f != x86::Flow::None) return std::nullopt;
  }
  if (!call) return std::nullopt;
  // The block the call ends: back to the last change of flow, at most 24 instructions.
  std::size_t start = *call;
  while (start > 0 && *call - start < 24 && insns[start - 1].insn.flow == x86::Flow::None) --start;
  std::array<RegState, 16> st{};
  for (std::size_t i = start; i < *call; ++i) simulate(cx, insns[i], instance_static, st);
  CallSite site;
  site.call = insns[*call].rva;
  const std::optional<std::uint32_t> callee = code::branch_target(cx.img, *cx.text, insns[*call]);
  if (!callee) return std::nullopt;
  site.callee = *callee;
  std::size_t instances = 0, wides = 0, narrows = 0;
  for (const std::uint8_t a : kArgs) {
    if (st[a] == RegState::Instance) {
      ++instances;
      site.abi.instance = a;
    } else if (st[a] == RegState::Wide) {
      ++wides;
      site.abi.task = a;
    } else if (st[a] == RegState::Narrow) {
      ++narrows;
      site.abi.group = a;
    }
  }
  if (instances != 1) return std::nullopt;
  if (wides != 1) site.abi.task.reset();
  if (narrows != 1) site.abi.group.reset();
  site.abi.shaped = wides == 1 && narrows == 1;
  return site;
}

std::optional<Cut> scan_checked_function(Context& cx, const cut::Cut& c, const RuntimeFunction& fn) {
  Cut out;
  out.bytes = c.bytes;
  out.hit = c.start;
  out.entry = fn.begin;
  out.length = fn.length();
  out.matches = c.matches;
  PatternSet set;
  PatternSpec spec;
  spec.id = "function";
  spec.kind = PatternKind::Function;
  spec.section = std::string(kText);
  spec.bytes = c.bytes;
  spec.anchor_min = 5;
  spec.fn_len_min = spec.fn_len_max = fn.length();
  set.patterns.push_back(spec);
  const ScanReport scan = scan_patterns(cx.image, cx.source, set);
  out.scan_resolved =
      !scan.results.empty() && scan.results.front().resolved() && scan.results.front().entry == fn.begin;
  return out;
}

void derive_register(Context& cx, const std::map<std::uint32_t, std::vector<code::Decoded>>& refs) {
  RegisterResult& r = cx.result.registration;
  const auto lead = std::find_if(cx.result.fd4.singletons.begin(), cx.result.fd4.singletons.end(),
                                 [&](const SingletonResult& s) { return s.label == cx.leads.register_instance; });
  if (lead == cx.result.fd4.singletons.end() || lead->status != Status::Derived) {
    r.status = Status::Depends;
    return;
  }
  r.instance_static = lead->static_slot;
  std::map<std::uint32_t, std::vector<CallSite>> by_callee;
  const auto it = refs.find(r.instance_static);
  if (it != refs.end()) {
    for (const code::Decoded& d : it->second) {
      if (!is_static_load(d.insn)) continue;
      ++r.loads;
      if (std::optional<CallSite> site = call_after_load(cx, d, r.instance_static)) {
        ++r.calls;
        by_callee[site->callee].push_back(*site);
      }
    }
  }
  for (const auto& [callee, sites] : by_callee) {
    RegisterCandidate c;
    c.callee = callee;
    c.sites = static_cast<std::uint32_t>(sites.size());
    c.abi = sites.front().abi;
    c.abi_agrees = std::all_of(sites.begin(), sites.end(), [&](const CallSite& s) { return s.abi == c.abi; });
    for (const CallSite& s : sites) {
      if (c.callers.size() < 16) c.callers.push_back(s.call);
    }
    const RuntimeFunction* fn = cx.image.function_at(callee);
    c.function =
        fn != nullptr && fn->begin == callee && is_fragment(cx.image, cx.source, *fn) == std::optional<bool>(false);
    r.candidates.push_back(std::move(c));
  }
  std::sort(r.candidates.begin(), r.candidates.end(), [](const RegisterCandidate& a, const RegisterCandidate& b) {
    const bool sa = a.abi.shaped && a.abi_agrees && a.function;
    const bool sb = b.abi.shaped && b.abi_agrees && b.function;
    if (sa != sb) return sa;
    if (a.sites != b.sites) return a.sites > b.sites;
    return a.callee < b.callee;
  });
  if (r.candidates.size() > 8) r.candidates.resize(8);
  if (r.candidates.empty()) {
    r.status = Status::Missing;
    return;
  }
  const RegisterCandidate& best = r.candidates.front();
  const RuntimeFunction* fn = cx.image.function_at(best.callee);
  if (!best.function || fn == nullptr) {
    r.status = Status::Refused;
    return;
  }
  const std::optional<cut::Cut> c = cut::cut(cx.img, *cx.text, cx.fixups, cut::Request{fn->begin, fn->end, 5, {}});
  if (!c) {
    r.status = Status::NotUnique;
    return;
  }
  r.pattern = scan_checked_function(cx, *c, *fn);
  r.status = best.abi.shaped && best.abi_agrees && r.pattern->scan_resolved ? Status::Derived : Status::Refused;
}

// ---- The task group's index ----

// Whole NUL-terminated strings equal to `s` in initialised data, as ASCII and as UTF-16.
std::vector<std::pair<std::uint32_t, bool>> whole_strings(const ImageBytes& img, std::string_view s) {
  std::vector<std::pair<std::uint32_t, bool>> out;
  for (const bool wide : {false, true}) {
    std::vector<std::byte> needle;
    for (const char c : s) {
      needle.push_back(std::byte{static_cast<unsigned char>(c)});
      if (wide) needle.push_back(std::byte{0});
    }
    needle.push_back(std::byte{0});
    if (wide) needle.push_back(std::byte{0});
    for (const SectionCopy& c : img.sections()) {
      if (!data_section(img, c)) continue;
      auto from = c.bytes.begin();
      while (true) {
        const auto hit = std::search(from, c.bytes.end(), needle.begin(), needle.end());
        if (hit == c.bytes.end()) break;
        from = hit + 1;
        const auto off = static_cast<std::size_t>(hit - c.bytes.begin());
        if (wide && (c.rva + off) % 2 != 0) continue;
        // A whole string: what comes before is not a character of it.
        const std::size_t unit = wide ? 2 : 1;
        if (off >= unit) {
          const auto prev = std::to_integer<std::uint8_t>(c.bytes[off - unit]);
          const bool prev_zero = !wide || std::to_integer<std::uint8_t>(c.bytes[off - 1]) == 0;
          if (prev >= 0x20 && prev < 0x7F && prev_zero) continue;
        }
        out.emplace_back(static_cast<std::uint32_t>(c.rva + off), wide);
      }
    }
  }
  return out;
}

bool names_identifier(const ImageBytes& img, std::uint64_t va, bool wide) {
  const std::optional<std::uint32_t> rva = img.rva_of(va);
  if (!rva) return false;
  const std::optional<std::string> s = wide ? img.utf16(*rva, 128) : img.ascii(*rva, 128);
  return s && s->size() >= 2 && is_identifier_text(*s);
}

// The places within [from, to) that code addresses RIP-relatively.
std::set<std::uint32_t> addressed_within(Context& cx, std::uint32_t from, std::uint32_t to) {
  std::set<std::uint32_t> out;
  if (cx.text == nullptr) return out;
  code::Stats stats;
  code::sweep(
      cx.image, *cx.text,
      [&](const code::Decoded& d) {
        if (!d.insn.rip_relative) return;
        const std::optional<std::uint32_t> t = code::rip_target(cx.img, *cx.text, d);
        if (t && *t >= from && *t < to) out.insert(*t);
      },
      stats);
  return out;
}

void derive_group(Context& cx, const std::map<std::uint32_t, std::vector<code::Decoded>>& refs,
                  const std::vector<std::pair<std::uint32_t, bool>>& strings) {
  GroupResult& g = cx.result.group;
  g.strings = strings.size();
  for (const auto& [rva, wide] : strings) {
    // Tables of pointers to names, one of them this one. Tables can lie end to end, so the table is the run of such
    // pointers cut where code addresses it: its index is its place after the last start the code addresses.
    const std::uint64_t va = cx.image.image_base + rva;
    for (const SectionCopy& c : cx.img.sections()) {
      if (!data_section(cx.img, c)) continue;
      for (std::uint64_t off = (8 - c.rva % 8) % 8; off + 8 <= c.bytes.size(); off += 8) {
        if (bytes::u64(c.bytes, off).value_or(0) != va) continue;
        std::uint64_t back = 0;
        while (off >= 8 * (back + 1) &&
               names_identifier(cx.img, bytes::u64(c.bytes, off - 8 * (back + 1)).value_or(0), wide) && back < 4096) {
          ++back;
        }
        std::uint64_t fwd = 0;
        while (off + 8 * (fwd + 2) <= c.bytes.size() &&
               names_identifier(cx.img, bytes::u64(c.bytes, off + 8 * (fwd + 1)).value_or(0), wide) && fwd < 4096) {
          ++fwd;
        }
        const auto run_start = static_cast<std::uint32_t>(c.rva + off - 8 * back);
        const auto entry = static_cast<std::uint32_t>(c.rva + off);
        const auto run_end = static_cast<std::uint32_t>(c.rva + off + 8 * (fwd + 1));
        std::uint32_t start = run_start;
        std::uint32_t end = run_end;
        for (const std::uint32_t at : addressed_within(cx, run_start, run_end)) {
          if ((at - run_start) % 8 != 0) continue;
          if (at <= entry) start = at;
          if (at > entry && at < end) end = at;
        }
        if (g.candidates.size() < 16) {
          g.candidates.push_back(GroupCandidate{(entry - start) / 8, "pointer_table", start, (end - start) / 8});
        }
      }
    }
    // Code that loads the name, with a small 32-bit constant set beside it.
    const auto it = refs.find(rva);
    if (it == refs.end()) continue;
    for (const code::Decoded& ref : it->second) {
      const std::optional<std::vector<code::Decoded>> before = code::decode_to(cx.image, *cx.text, ref.rva);
      std::vector<code::Decoded> near;
      if (before) near.assign(before->size() > 7 ? before->end() - 7 : before->begin(), before->end());
      const std::vector<code::Decoded> after = code::decode_range(cx.image, *cx.text, ref.end(), ref.end() + 48);
      for (std::size_t i = 0; i < after.size() && i < 6; ++i) near.push_back(after[i]);
      std::optional<std::uint32_t> best;
      std::int64_t best_distance = 0;
      for (const code::Decoded& d : near) {
        const x86::Instruction& in = d.insn;
        const bool mov_imm = in.encoding == x86::Encoding::Legacy && in.map == 0 && !in.wide &&
                             ((in.opcode >= 0xB8 && in.opcode <= 0xBF) || (in.opcode == 0xC7 && in.mod() == 3));
        if (!mov_imm) continue;
        const std::optional<std::int64_t> v = x86::immediate(in, code::bytes_of(*cx.text, d));
        if (!v || *v < 0 || *v >= 4096) continue;
        const std::int64_t distance = std::abs(std::int64_t{d.rva} - std::int64_t{ref.rva});
        if (!best || distance < best_distance) {
          best = static_cast<std::uint32_t>(*v);
          best_distance = distance;
        }
      }
      if (best && g.candidates.size() < 16) g.candidates.push_back(GroupCandidate{*best, "code_reference", ref.rva, 0});
    }
  }
  g.status = g.candidates.empty() ? Status::Missing : Status::Derived;
}

// ---- The statics ----

void derive_statics(Context& cx, const std::map<std::uint32_t, std::vector<code::Decoded>>& refs) {
  for (StaticResult& s : cx.result.statics) {
    const auto hint = std::find_if(cx.options.static_slots.begin(), cx.options.static_slots.end(),
                                   [&](const auto& h) { return h.first == s.label; });
    if (hint == cx.options.static_slots.end()) {
      s.status = Status::NeedsSlot;
      continue;
    }
    s.slot = hint->second;
    if (cx.img.section_name(hint->second) != kData || hint->second % 8 != 0) {
      s.status = Status::Refused;
      continue;
    }
    const auto it = refs.find(hint->second);
    if (it == refs.end() || it->second.empty()) {
      s.status = Status::Missing;
      continue;
    }
    s.references = it->second.size();
    // The references inside functions of the exception directory that are no fragments, at entry + 5 or later; the
    // 16 in the smallest functions are cut (each cut is one pass over .text).
    std::vector<std::pair<const code::Decoded*, const RuntimeFunction*>> usable;
    for (const code::Decoded& ref : it->second) {
      const RuntimeFunction* fn = cx.image.function_at(ref.rva);
      if (fn == nullptr || ref.rva < fn->begin + 5 ||
          is_fragment(cx.image, cx.source, *fn) != std::optional<bool>(false)) {
        continue;
      }
      usable.emplace_back(&ref, fn);
    }
    std::stable_sort(usable.begin(), usable.end(),
                     [](const auto& a, const auto& b) { return a.second->length() < b.second->length(); });
    if (usable.size() > 16) usable.resize(16);
    std::optional<cut::Cut> best;
    const RuntimeFunction* best_fn = nullptr;
    for (const auto& [ref_ptr, fn] : usable) {
      const code::Decoded& ref = *ref_ptr;
      std::optional<cut::Cut> c = cut::cut(cx.img, *cx.text, cx.fixups, cut::Request{fn->begin, fn->end, 5, ref.rva});
      if (!c || !c->capture) continue;
      // Of the references' patterns: one with a margin (20 bytes, 12 fixed) first, then the one in the smaller
      // function, whose code the next build is likelier to leave alone, then the shorter.
      const auto comfortable = [](const BytePattern& p) { return p.size() >= 20 && p.fixed() >= 12; };
      const bool better = !best || comfortable(c->bytes) != comfortable(best->bytes) ? !best || comfortable(c->bytes)
                          : fn->length() != best_fn->length()                        ? fn->length() < best_fn->length()
                                                              : c->bytes.size() < best->bytes.size();
      if (better) {
        best = std::move(c);
        best_fn = fn;
      }
    }
    if (!best || best_fn == nullptr) {
      s.status = Status::NotUnique;
      continue;
    }
    Cut out;
    out.bytes = best->bytes;
    out.hit = best->start;
    out.entry = best_fn->begin;
    out.length = best_fn->length();
    out.matches = best->matches;
    out.capture = best->capture;
    out.target = hint->second;
    PatternSet set;
    PatternSpec spec;
    spec.id = "static";
    spec.kind = PatternKind::Static;
    spec.section = std::string(kText);
    spec.bytes = best->bytes;
    spec.capture = *best->capture;
    spec.target_section = std::string(kData);
    set.patterns.push_back(spec);
    const ScanReport scan = scan_patterns(cx.image, cx.source, set);
    out.scan_resolved =
        !scan.results.empty() && scan.results.front().resolved() && scan.results.front().target == hint->second;
    s.status = out.scan_resolved ? Status::Derived : Status::Refused;
    s.pattern = std::move(out);
  }
}

// ---- What R1 must confirm ----

void confirm_list(Context& cx) {
  Result& r = cx.result;
  auto add = [&](std::string_view item, std::string label, std::optional<std::uint64_t> value) {
    r.confirm.push_back(ConfirmItem{item, std::move(label), value});
  };
  if (r.fd4.names) add("name_layout", "", r.fd4.names->offset);
  for (const SingletonResult& s : r.fd4.singletons) {
    if (s.status == Status::Derived) add("singleton_instance", s.label, s.static_slot);
  }
  for (const VtableSetResult& s : r.sets) {
    if (s.status == Status::Derived) add("vtable_set", s.label, s.primary.size());
  }
  if (!r.registration.candidates.empty()) {
    add("register_task", "", r.registration.candidates.front().callee);
    add("register_abi", "", std::nullopt);
  }
  if (!r.group.candidates.empty())
    add("task_group_index", "", r.group.candidates.front().index);
  else
    add("task_group_index", "", std::nullopt);
  add("task_run_slot", "", r.task.run_slot ? std::optional<std::uint64_t>(*r.task.run_slot) : std::nullopt);
  add("task_object_size", "", std::nullopt);
  for (const StaticResult& s : r.statics) {
    if (s.status == Status::NeedsSlot) {
      add(s.module ? "static_slot_module_base" : "static_slot_instance", s.label,
          s.module ? std::nullopt : std::optional<std::uint64_t>(s.rtti.vtable));
    } else if (s.pattern) {
      add("static_pattern_in_memory", s.label, s.pattern->hit);
    }
  }
  add("layout_fields", "", std::nullopt);
}

}  // namespace

Result derive(const ImageInfo& image, const ByteSource& source, const Leads& leads, const Options& options) {
  Result result;
  result.file_size = options.file_size;
  const ImageBytes img(image, source);
  Context cx{image, source, leads, options, img, img.named(kText), {}, result};
  if (auto fixups = read_relocations(image, source)) cx.fixups = std::move(*fixups);
  result.stats.fixups = cx.fixups.size();
  fingerprint(cx);
  derive_fd4(cx);
  derive_rtti(cx);

  // One sweep for every reference the rest needs: the registration's instance, the group's name, the statics' slots.
  std::set<std::uint32_t> targets;
  for (const SingletonResult& s : result.fd4.singletons) {
    if (s.label == leads.register_instance && s.status == Status::Derived) targets.insert(s.static_slot);
  }
  const std::vector<std::pair<std::uint32_t, bool>> strings =
      leads.task_group.empty() ? std::vector<std::pair<std::uint32_t, bool>>{} : whole_strings(img, leads.task_group);
  for (const auto& [rva, _] : strings) targets.insert(rva);
  for (const auto& [_, slot] : options.static_slots) targets.insert(slot);
  const auto refs = references(cx, targets, 4096, result.fd4.path_strings == 0);

  derive_register(cx, refs);
  derive_group(cx, refs, strings);
  derive_statics(cx, refs);
  confirm_list(cx);
  return result;
}

bool complete(const Result& r) {
  const Fingerprint& f = r.fingerprint;
  if (!f.has_version || f.region == "other" || f.product_name == "differs" || f.product_name == "absent") return false;
  if (r.fd4.status != Status::Derived) return false;
  for (const SingletonResult& s : r.fd4.singletons) {
    if (s.status != Status::Derived) return false;
  }
  for (const VtableSetResult& s : r.sets) {
    if (s.status != Status::Derived) return false;
  }
  if (r.task.status != Status::Derived || r.registration.status != Status::Derived ||
      r.group.status != Status::Derived) {
    return false;
  }
  for (const StaticResult& s : r.statics) {
    if (s.status != Status::Derived && s.status != Status::NeedsSlot) return false;
    if (!s.module && s.rtti.status != Status::Derived) return false;
  }
  return true;
}

}  // namespace en::gal::derive
