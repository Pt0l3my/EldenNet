// en-gal's FD4 singleton finder (fd4.hpp).
#include "en/gal/fd4.hpp"

#include <algorithm>
#include <map>

#include "bytes.hpp"
#include "code.hpp"

namespace en::gal::fd4 {

const char* to_string(Refusal refusal) noexcept {
  switch (refusal) {
    case Refusal::Unreadable: return "a displacement could not be read as the idiom's";
    case Refusal::StaticSection: return "the static is not in the data section";
    case Refusal::StaticAlignment: return "the static is not aligned to 8";
    case Refusal::ClassSection: return "the class object is not in the data section";
    case Refusal::GetterSection: return "the getter is not in the code section";
    case Refusal::GetterNotFunction: return "the getter does not start a function";
    case Refusal::NoPath: return "no path's load follows";
    case Refusal::PathSection: return "the path is not in the read-only data section";
    case Refusal::PathText: return "the path does not end with the suffix";
  }
  return "refused";
}

std::optional<Idiom> idiom_from(const PatternSpec& spec) {
  if (spec.kind != PatternKind::Idiom) return std::nullopt;
  Idiom idiom;
  idiom.bytes = spec.bytes;
  bool s = false, c = false, g = false;
  for (const IdiomCapture& cap : spec.captures) {
    if (cap.name == "static") {
      idiom.static_slot = cap.capture;
      s = true;
    } else if (cap.name == "class") {
      idiom.class_object = cap.capture;
      c = true;
    } else if (cap.name == "getter") {
      idiom.getter = cap.capture;
      g = true;
    } else if (cap.name == "path") {
      idiom.path = cap.capture;
    }
  }
  if (!s || !c || !g) return std::nullopt;
  return idiom;
}

std::optional<std::string> class_name(const ImageBytes& img, std::uint32_t class_object, const NameLayout& layout) {
  const std::optional<std::uint64_t> at = bytes::add(class_object, layout.offset);
  if (!at || *at > 0xFFFFFFFFu) return std::nullopt;
  const std::optional<std::uint64_t> va = img.u64(static_cast<std::uint32_t>(*at));
  if (!va) return std::nullopt;
  const std::optional<std::uint32_t> rva = img.rva_of(*va);
  if (!rva) return std::nullopt;
  return layout.encoding == NameEncoding::Ascii ? img.ascii(*rva, 128) : img.utf16(*rva, 128);
}

bool path_matches(const ImageBytes& img, std::uint32_t rva, std::string_view suffix) {
  const std::optional<std::string> path = img.ascii(rva, 260);
  return path && !suffix.empty() && path->size() >= suffix.size() &&
         path->compare(path->size() - suffix.size(), suffix.size(), suffix) == 0;
}

namespace {

// The target of the displacement at hit + capture.at, as an RVA in the image.
std::optional<std::uint32_t> target(const ImageBytes& img, std::uint32_t hit, const Capture& capture) {
  const std::optional<std::uint64_t> at = bytes::add(hit, capture.at);
  if (!at || *at > 0xFFFFFFFFu) return std::nullopt;
  const std::optional<std::uint32_t> raw = img.u32(static_cast<std::uint32_t>(*at));
  if (!raw) return std::nullopt;
  const std::int64_t disp = *raw >= 0x80000000u ? static_cast<std::int64_t>(*raw) - 0x100000000LL : *raw;
  const std::int64_t t = std::int64_t{hit} + capture.insn_end + disp;
  if (t < 0 || t >= std::int64_t{img.image().size_of_image}) return std::nullopt;
  return static_cast<std::uint32_t>(t);
}

bool in_named(const ImageBytes& img, std::uint32_t rva, std::string_view name) { return img.section_name(rva) == name; }

// R0's rule, kept for the derivation's comparison: the path's load after a pattern that stops short of it is the
// first RIP-relative operand into the read-only section within the block the idiom's jump skips (from the pattern's end
// to the jump's target), or within `path_reach` bytes when the pattern holds no jump, whatever its text.
std::optional<std::uint32_t> first_reference(const ImageBytes& img, const SectionCopy& text, std::uint32_t hit,
                                             std::uint32_t pattern_end, const Config& config) {
  std::uint32_t reach = pattern_end + config.path_reach;
  for (const code::Decoded& d : code::decode_range(img.image(), text, hit, pattern_end)) {
    if (d.insn.flow != x86::Flow::Branch) continue;
    const std::optional<std::uint32_t> t = code::branch_target(img, text, d);
    if (t && *t > pattern_end) reach = *t;
    break;
  }
  for (const code::Decoded& d : code::decode_range(img.image(), text, pattern_end, reach)) {
    if (d.end() > reach) break;
    const std::optional<std::uint32_t> t = code::rip_target(img, text, d);
    if (t && in_named(img, *t, config.rdata)) return t;
  }
  return std::nullopt;
}

// Whether the hit decodes as the idiom says: its instructions follow one another from the hit, the static's, the class
// object's and the path's displacements are RIP-relative operands of 32 bits, and the getter's is a relative call's,
// each ending where its capture says.
bool shaped(const ImageBytes& img, const SectionCopy& text, std::uint32_t hit, const Idiom& idiom) {
  const auto end = static_cast<std::uint32_t>(hit + idiom.bytes.size());
  const std::vector<code::Decoded> insns = code::decode_range(img.image(), text, hit, end);
  std::uint32_t at = hit;
  for (const code::Decoded& d : insns) {
    if (d.rva != at) return false;  // a byte that did not decode, or a function's entry inside the pattern
    at = d.end();
  }
  if (at < end) return false;
  const auto operand = [&](const Capture& c, bool call) {
    for (const code::Decoded& d : insns) {
      if (d.end() - hit != c.insn_end) continue;
      if (call) {
        return d.insn.flow == x86::Flow::Call && d.insn.relative && d.insn.imm_size == 4 &&
               d.rva - hit + d.insn.imm_at == c.at;
      }
      return d.insn.rip_relative && d.insn.disp_size == 4 && d.rva - hit + d.insn.disp_at == c.at;
    }
    return false;
  };
  return operand(idiom.static_slot, false) && operand(idiom.class_object, false) && operand(idiom.getter, true) &&
         (!idiom.path || operand(*idiom.path, false));
}

// What one instruction loads, if it loads an address: the place it names and how.
struct Load {
  std::uint32_t place = 0;
  PathForm form = PathForm::Address;
  std::uint8_t reg = 0;
};

std::optional<Load> load_of(const ImageBytes& img, const SectionCopy& text, const code::Decoded& d) {
  const x86::Instruction& in = d.insn;
  const bool legacy = in.encoding == x86::Encoding::Legacy && in.map == 0;
  if (in.rip_relative) {
    const std::optional<std::uint32_t> t = code::rip_target(img, text, d);
    if (!t) return std::nullopt;
    if (legacy && in.opcode == 0x8B && in.wide) {  // a pointer read from its slot
      const std::optional<std::uint64_t> va = img.u64(*t);
      const std::optional<std::uint32_t> place = va ? img.rva_of(*va) : std::nullopt;
      if (!place) return std::nullopt;
      return Load{*place, PathForm::Pointer, in.reg()};
    }
    return Load{*t, PathForm::Address, in.has_modrm ? in.reg() : std::uint8_t{0}};
  }
  if (legacy && in.opcode >= 0xB8 && in.opcode <= 0xBF && in.wide && in.imm_size == 8) {
    const std::optional<std::int64_t> v = x86::immediate(in, code::bytes_of(text, d));
    const std::optional<std::uint32_t> place = v ? img.rva_of(static_cast<std::uint64_t>(*v)) : std::nullopt;
    if (!place) return std::nullopt;
    return Load{*place, PathForm::Absolute, in.opcode_reg()};
  }
  return std::nullopt;
}

struct PathSearch {
  std::optional<PathLoad> load;           // the path, in the read-only section
  std::uint32_t path = 0;
  std::optional<std::uint32_t> first;     // the first other reference into the read-only section
  std::optional<std::uint32_t> elsewhere; // a string ending with the suffix, outside the read-only section
  std::uint32_t ordinal = 0;
};

// One stretch of instructions, in order; stops at the path.
void search(const ImageBytes& img, const SectionCopy& text, const std::vector<code::Decoded>& insns, std::uint32_t from,
            std::uint32_t to, bool hoisted, const Config& config, PathSearch& out) {
  for (const code::Decoded& d : insns) {
    if (d.rva < from) continue;
    if (d.end() > to) break;
    const std::optional<Load> l = load_of(img, text, d);
    if (!l) continue;
    const bool rdata = in_named(img, l->place, config.rdata);
    if (path_matches(img, l->place, config.path_suffix)) {
      if (rdata) {
        const std::uint8_t field = d.insn.rip_relative ? d.insn.disp_at : d.insn.imm_at;
        out.load =
            PathLoad{d.rva, d.insn.length, field, d.insn.map, d.insn.opcode, l->reg, l->form, out.ordinal, hoisted};
        out.path = l->place;
        return;
      }
      if (!out.elsewhere) out.elsewhere = l->place;
      continue;
    }
    if (rdata) {
      if (!out.first) out.first = l->place;
      ++out.ordinal;
    }
  }
}

// The path's load after a pattern that stops short of it: of the block the idiom's jump skips (from the jump to its
// target), or of `path_reach` bytes after the pattern when it holds no jump, the first load of a string that ends with
// the suffix; else of the `back_reach` bytes before the hit, within its function, the last such load.
PathSearch find_path(const ImageBytes& img, const SectionCopy& text, std::uint32_t hit, std::uint32_t pattern_end,
                     const Config& config) {
  PathSearch out;
  std::uint32_t from = pattern_end;
  std::uint32_t to = pattern_end + config.path_reach;
  for (const code::Decoded& d : code::decode_range(img.image(), text, hit, pattern_end)) {
    if (d.insn.flow != x86::Flow::Branch) continue;
    const std::optional<std::uint32_t> t = code::branch_target(img, text, d);
    if (t && *t > pattern_end) {
      from = d.end();
      to = *t;
    }
    break;
  }
  search(img, text, code::decode_range(img.image(), text, from, to), from, to, false, config, out);
  if (out.load) return out;
  // A load hoisted before the hit: decoded from its function's entry, so that it is read as the code runs.
  const RuntimeFunction* fn = img.image().function_at(hit);
  if (fn == nullptr || config.back_reach == 0) return out;
  const std::optional<std::vector<code::Decoded>> before = code::decode_to(img.image(), text, hit);
  if (!before) return out;
  const std::uint32_t back = hit - std::min(hit - fn->begin, config.back_reach);
  for (std::size_t i = before->size(); i-- > 0;) {  // the closest first
    const code::Decoded& d = (*before)[i];
    if (d.rva < back) break;
    if (d.rva >= hit) continue;
    PathSearch one;
    search(img, text, {d}, back, hit, true, config, one);
    if (one.load) {
      one.load->ordinal = 0;
      out.load = one.load;
      out.path = one.path;
      return out;
    }
  }
  return out;
}

}  // namespace

namespace {

// The rest of a site once its three places are known: its path (through the pattern's capture, by R0's rule, or by
// decoding on), its placement and its name.
void conclude(const ImageBytes& img, const SectionCopy& text, const Config& config, Site s, std::uint32_t pattern_end,
              const std::optional<Capture>& path_capture, bool first_reference_rule,
              std::map<std::string, Singleton>& by_name, Result& result) {
  const std::uint32_t hit = s.hit;
  const std::uint32_t st = s.static_slot;
  const std::uint32_t cl = s.class_object;
  const std::uint32_t gt = s.getter;
  std::optional<std::uint32_t> path;
  std::optional<Refusal> path_refusal;
  if (path_capture) {
    path = target(img, hit, *path_capture);
    if (!path) {
      path_refusal = Refusal::NoPath;
    } else if (!in_named(img, *path, config.rdata)) {
      path_refusal = Refusal::PathSection;
    } else if (!path_matches(img, *path, config.path_suffix)) {
      path_refusal = Refusal::PathText;
    }
  } else if (first_reference_rule) {
    path = first_reference(img, text, hit, pattern_end, config);
    if (!path) {
      path_refusal = Refusal::NoPath;
    } else if (!path_matches(img, *path, config.path_suffix)) {
      path_refusal = Refusal::PathText;
    }
  } else {
    PathSearch found = find_path(img, text, hit, pattern_end, config);
    if (found.load) {
      path = found.path;
      s.path_load = found.load;
    } else if (found.elsewhere) {
      path = found.elsewhere;
      path_refusal = Refusal::PathSection;
    } else if (found.first) {
      path = found.first;
      path_refusal = Refusal::PathText;
    } else {
      path_refusal = Refusal::NoPath;
    }
  }
  if (path) s.path = *path;
  const RuntimeFunction* fn = img.image().function_at(gt);
  if (!in_named(img, st, config.data)) {
    s.refused = Refusal::StaticSection;
  } else if (st % 8 != 0) {
    s.refused = Refusal::StaticAlignment;
  } else if (!in_named(img, cl, config.data)) {
    s.refused = Refusal::ClassSection;
  } else if (!in_named(img, gt, config.code)) {
    s.refused = Refusal::GetterSection;
  } else if (fn == nullptr || fn->begin != gt) {
    s.refused = Refusal::GetterNotFunction;
  } else if (path_refusal) {
    s.refused = path_refusal;
  }
  if (config.names) s.name = class_name(img, cl, *config.names);
  if (!s.refused) {
    if (!s.name) {
      ++result.unnamed;
    } else {
      auto [it, fresh] = by_name.try_emplace(*s.name);
      Singleton& one = it->second;
      if (fresh) {
        one.name = *s.name;
        one.static_slot = st;
        one.class_object = cl;
        one.getter = gt;
      } else if (one.static_slot != st) {
        one.ambiguous = true;
      }
      ++one.sites;
    }
  }
  result.sites.push_back(std::move(s));
}

}  // namespace

Result find(const ImageBytes& img, const Idiom& idiom, const Config& config) {
  Result result;
  const SectionCopy* text = img.named(config.code);
  if (text == nullptr) return result;
  const code::Matches matches = code::find_matches(*text, idiom.bytes, config.max_hits);
  result.hits = matches.count;
  std::map<std::string, Singleton> by_name;
  for (const std::uint32_t hit : matches.hits) {
    Site s;
    s.hit = hit;
    const auto st = target(img, hit, idiom.static_slot);
    const auto cl = target(img, hit, idiom.class_object);
    const auto gt = target(img, hit, idiom.getter);
    if (!st || !cl || !gt) {
      s.refused = Refusal::Unreadable;
      result.sites.push_back(std::move(s));
      continue;
    }
    s.static_slot = *st;
    s.class_object = *cl;
    s.getter = *gt;
    const auto pattern_end = static_cast<std::uint32_t>(hit + idiom.bytes.size());
    if (!config.first_reference && !shaped(img, *text, hit, idiom)) {
      s.refused = Refusal::Unreadable;
      result.sites.push_back(std::move(s));
      continue;
    }
    conclude(img, *text, config, std::move(s), pattern_end, idiom.path, config.first_reference, by_name, result);
  }
  for (auto& [_, one] : by_name) result.singletons.push_back(std::move(one));
  return result;
}

bool Accessor::valid() const noexcept {
  if (!shape.valid()) return false;
  const auto gives = [&](std::uint8_t step, shape::Form form) {
    return step < shape.steps.size() && shape.steps[step] == form;
  };
  return (gives(static_step, shape::Form::LoadSlot) || gives(static_step, shape::Form::CompareSlot)) &&
         gives(class_step, shape::Form::AddressArg1) &&
         gives(getter_step, shape::Form::CallRelative);
}

Result find(const ImageBytes& img, const Accessor& accessor, const Config& config) {
  Result result;
  const SectionCopy* text = img.named(config.code);
  if (text == nullptr || !accessor.valid()) return result;
  const shape::Matches matches = shape::find(img, *text, accessor.shape, config.max_hits);
  result.hits = matches.count;
  std::map<std::string, Singleton> by_name;
  for (const shape::Hit& hit : matches.hits) {
    Site s;
    s.hit = hit.rva;
    // A shape's hit is decoded already: its places are RIP-relative operands and a relative call by its forms.
    s.static_slot = hit.gives[accessor.static_step];
    s.class_object = hit.gives[accessor.class_step];
    s.getter = hit.gives[accessor.getter_step];
    conclude(img, *text, config, std::move(s), hit.end, std::nullopt, false, by_name, result);
  }
  for (auto& [_, one] : by_name) result.singletons.push_back(std::move(one));
  return result;
}

}  // namespace en::gal::fd4
