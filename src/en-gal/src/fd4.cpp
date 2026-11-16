// en-gal's FD4 singleton finder (fd4.hpp).
#include "en/gal/fd4.hpp"

#include <algorithm>
#include <map>

#include "bytes.hpp"
#include "code.hpp"

namespace en::gal::fd4 {

const char* to_string(Refusal refusal) noexcept {
  switch (refusal) {
    case Refusal::Unreadable: return "a displacement could not be read";
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

// The path's load after a pattern that stops short of it: the first RIP-relative operand into the read-only section
// within the block the idiom's jump skips (from the pattern's end to the jump's target), or within `path_reach` bytes
// when the pattern holds no jump. Its target is the path, whether or not it ends with the suffix.
std::optional<std::uint32_t> find_path(const ImageBytes& img, const SectionCopy& text, std::uint32_t hit,
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
    std::optional<std::uint32_t> path;
    bool path_in_rdata = false;
    if (idiom.path) {
      path = target(img, hit, *idiom.path);
    } else {
      path = find_path(img, *text, hit, hit + static_cast<std::uint32_t>(idiom.bytes.size()), config);
    }
    path_in_rdata = path && in_named(img, *path, config.rdata);
    if (path) s.path = *path;
    const RuntimeFunction* fn = img.image().function_at(*gt);
    if (!in_named(img, *st, config.data)) {
      s.refused = Refusal::StaticSection;
    } else if (*st % 8 != 0) {
      s.refused = Refusal::StaticAlignment;
    } else if (!in_named(img, *cl, config.data)) {
      s.refused = Refusal::ClassSection;
    } else if (!in_named(img, *gt, config.code)) {
      s.refused = Refusal::GetterSection;
    } else if (fn == nullptr || fn->begin != *gt) {
      s.refused = Refusal::GetterNotFunction;
    } else if (!path) {
      s.refused = Refusal::NoPath;
    } else if (!path_in_rdata) {
      s.refused = Refusal::PathSection;
    } else if (!path_matches(img, *path, config.path_suffix)) {
      s.refused = Refusal::PathText;
    }
    if (config.names) s.name = class_name(img, *cl, *config.names);
    if (!s.refused) {
      if (!s.name) {
        ++result.unnamed;
      } else {
        auto [it, fresh] = by_name.try_emplace(*s.name);
        Singleton& one = it->second;
        if (fresh) {
          one.name = *s.name;
          one.static_slot = *st;
          one.class_object = *cl;
          one.getter = *gt;
        } else if (one.static_slot != *st) {
          one.ambiguous = true;
        }
        ++one.sites;
      }
    }
    result.sites.push_back(std::move(s));
  }
  for (auto& [_, one] : by_name) result.singletons.push_back(std::move(one));
  return result;
}

}  // namespace en::gal::fd4
