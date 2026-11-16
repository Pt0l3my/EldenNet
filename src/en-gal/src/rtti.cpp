// en-gal's RTTI walk (rtti.hpp).
#include "en/gal/rtti.hpp"

#include <algorithm>
#include <set>
#include <utility>

#include "bytes.hpp"

namespace en::gal::rtti {

namespace {

constexpr std::uint32_t kInitialisedData = 0x00000040;  // IMAGE_SCN_CNT_INITIALIZED_DATA
constexpr std::size_t kMaxDecoratedName = 4096;

bool data_section(const ImageBytes& img, const SectionCopy& c) {
  const std::uint32_t ch = img.header(c).characteristics;
  return (ch & kSectionExecute) == 0 && (ch & kInitialisedData) != 0;
}

// A type descriptor: a pointer into the image, then (after the spare pointer) a decorated name starting ".?A".
bool type_descriptor_ok(const ImageBytes& img, std::uint32_t td) {
  const std::optional<std::uint64_t> vft = img.u64(td);
  if (!vft || !img.rva_of(*vft)) return false;
  if (td > 0xFFFFFFFFu - 16) return false;
  const std::span<const std::byte> head = img.view(td + 16, 4);
  if (head.size() != 4 || std::to_integer<char>(head[0]) != '.' || std::to_integer<char>(head[1]) != '?' ||
      std::to_integer<char>(head[2]) != 'A') {
    return false;
  }
  return img.ascii(td + 16, kMaxDecoratedName).has_value();
}

// The class hierarchy descriptor's base class array, as type descriptors; nothing when any part does not hold.
std::optional<std::vector<std::uint32_t>> hierarchy_bases(const ImageBytes& img, std::uint32_t chd,
                                                          const Limits& limits) {
  const std::optional<std::uint32_t> signature = img.u32(chd);
  const std::optional<std::uint32_t> count = chd <= 0xFFFFFFFFu - 12 ? img.u32(chd + 8) : std::nullopt;
  const std::optional<std::uint32_t> array = chd <= 0xFFFFFFFFu - 12 ? img.u32(chd + 12) : std::nullopt;
  if (!signature || *signature != 0 || !count || *count == 0 || *count > limits.max_bases || !array)
    return std::nullopt;
  std::vector<std::uint32_t> bases;
  bases.reserve(*count);
  for (std::uint32_t i = 0; i < *count; ++i) {
    const std::optional<std::uint64_t> entry_at = bytes::add(*array, std::uint64_t{4} * i);
    if (!entry_at || *entry_at > 0xFFFFFFFFu) return std::nullopt;
    const std::optional<std::uint32_t> bcd = img.u32(static_cast<std::uint32_t>(*entry_at));
    if (!bcd) return std::nullopt;
    const std::optional<std::uint32_t> td = img.u32(*bcd);
    if (!td || !type_descriptor_ok(img, *td)) return std::nullopt;
    bases.push_back(*td);
  }
  return bases;
}

// How many pointers into executable sections follow one another from `rva`.
std::uint32_t count_slots(const ImageBytes& img, std::uint32_t rva, std::uint32_t max) {
  std::uint32_t n = 0;
  while (n < max) {
    const std::optional<std::uint64_t> at = bytes::add(rva, std::uint64_t{8} * n);
    if (!at || *at > 0xFFFFFFFFu) break;
    const std::optional<std::uint64_t> va = img.u64(static_cast<std::uint32_t>(*at));
    if (!va) break;
    const std::optional<std::uint32_t> target = img.rva_of(*va);
    if (!target || !img.in_section_with(*target, kSectionExecute)) break;
    ++n;
  }
  return n;
}

}  // namespace

Walk walk(const ImageBytes& img, const Limits& limits) {
  Walk w;
  // The locators: signature 1 and their own offset, in initialised data.
  for (const SectionCopy& c : img.sections()) {
    if (!data_section(img, c)) continue;
    const std::span<const std::byte> b(c.bytes);
    for (std::uint64_t off = (4 - c.rva % 4) % 4; off + 24 <= b.size(); off += 4) {
      if (bytes::u32(b, off).value_or(0) != 1) continue;
      const std::uint64_t rva = c.rva + off;
      if (bytes::u32(b, off + 20).value_or(0) != rva) continue;
      Locator l;
      l.rva = static_cast<std::uint32_t>(rva);
      l.offset = bytes::u32(b, off + 4).value_or(0);
      l.type_descriptor = bytes::u32(b, off + 12).value_or(0);
      l.hierarchy = bytes::u32(b, off + 16).value_or(0);
      std::optional<std::vector<std::uint32_t>> bases;
      if (type_descriptor_ok(img, l.type_descriptor)) bases = hierarchy_bases(img, l.hierarchy, limits);
      if (!bases) {
        ++w.rejected;
        continue;
      }
      l.bases = std::move(*bases);
      if (w.locators.size() >= limits.max_locators) break;
      w.locators.push_back(std::move(l));
    }
  }
  std::sort(w.locators.begin(), w.locators.end(), [](const Locator& a, const Locator& b) { return a.rva < b.rva; });

  // The vtables: one pointer after each pointer to a locator, with at least one slot.
  std::vector<std::pair<std::uint64_t, std::size_t>> by_va;
  by_va.reserve(w.locators.size());
  for (std::size_t i = 0; i < w.locators.size(); ++i) by_va.emplace_back(img.image().image_base + w.locators[i].rva, i);
  std::sort(by_va.begin(), by_va.end());
  if (by_va.empty()) return w;
  for (const SectionCopy& c : img.sections()) {
    if (!data_section(img, c)) continue;
    const std::span<const std::byte> b(c.bytes);
    for (std::uint64_t off = (8 - c.rva % 8) % 8; off + 16 <= b.size(); off += 8) {
      const std::uint64_t q = bytes::u64(b, off).value_or(0);
      if (q < by_va.front().first || q > by_va.back().first) continue;
      const auto it = std::lower_bound(by_va.begin(), by_va.end(), std::make_pair(q, std::size_t{0}));
      if (it == by_va.end() || it->first != q) continue;
      const auto vt = static_cast<std::uint32_t>(c.rva + off + 8);
      const std::uint32_t slots = count_slots(img, vt, limits.max_slots);
      if (slots == 0) continue;
      w.vtables.push_back(Vtable{vt, it->second, slots});
    }
  }
  std::sort(w.vtables.begin(), w.vtables.end(), [](const Vtable& a, const Vtable& b) { return a.rva < b.rva; });
  return w;
}

std::optional<std::string> decorate(std::string_view cls, bool structure) {
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  while (true) {
    const std::size_t sep = cls.find("::", start);
    const std::string_view part =
        cls.substr(start, sep == std::string_view::npos ? std::string_view::npos : sep - start);
    if (!is_identifier_text(part) || part.size() > 255) return std::nullopt;
    parts.push_back(part);
    if (sep == std::string_view::npos) break;
    start = sep + 2;
  }
  std::string out = structure ? ".?AU" : ".?AV";
  for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
    out += *it;
    out += '@';
  }
  out += '@';
  return out;
}

Found find_type_descriptor(const ImageBytes& img, std::string_view cls) {
  Found found;
  for (const bool structure : {false, true}) {
    const std::optional<std::string> name = decorate(cls, structure);
    if (!name) return found;
    std::vector<std::byte> needle;
    for (const char ch : *name) needle.push_back(std::byte{static_cast<unsigned char>(ch)});
    needle.push_back(std::byte{0});
    for (const SectionCopy& c : img.sections()) {
      if (!data_section(img, c)) continue;
      auto from = c.bytes.begin();
      while (true) {
        const auto hit = std::search(from, c.bytes.end(), needle.begin(), needle.end());
        if (hit == c.bytes.end()) break;
        const auto off = static_cast<std::uint64_t>(hit - c.bytes.begin());
        from = hit + 1;
        if (off < 16) continue;
        const auto td = static_cast<std::uint32_t>(c.rva + off - 16);
        const std::optional<std::uint64_t> vft = img.u64(td);
        const std::optional<std::uint64_t> spare = img.u64(td + 8);
        if (!vft || !img.rva_of(*vft) || !spare || *spare != 0) continue;
        if (found.count == 0) found.rva = td;
        ++found.count;
      }
    }
  }
  return found;
}

Set derived_from(const Walk& walk, std::uint32_t base, bool strict) {
  Set set;
  std::set<std::uint32_t> classes;
  for (const Vtable& v : walk.vtables) {
    if (v.locator >= walk.locators.size()) continue;
    const Locator& l = walk.locators[v.locator];
    if (std::find(l.bases.begin(), l.bases.end(), base) == l.bases.end()) continue;
    if (strict && l.type_descriptor == base) continue;
    (l.offset == 0 ? set.primary : set.subobject).push_back(v.rva);
    classes.insert(l.type_descriptor);
  }
  set.classes = classes.size();
  return set;
}

std::vector<const Vtable*> vtables_of(const Walk& walk, std::uint32_t type_descriptor) {
  std::vector<const Vtable*> out;
  for (const Vtable& v : walk.vtables) {
    if (v.locator < walk.locators.size() && walk.locators[v.locator].type_descriptor == type_descriptor) {
      out.push_back(&v);
    }
  }
  return out;
}

}  // namespace en::gal::rtti
