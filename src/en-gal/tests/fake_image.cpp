// FakeImage (fake_image.hpp): a PE32+ image of the tests' own, laid out as a linker would, with invented contents.
#include "fake_image.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace en::gal::test {

namespace {

std::uint32_t align(std::uint32_t v, std::uint32_t a) { return (v + a - 1) / a * a; }

void put(std::vector<std::uint8_t>& b, std::size_t at, std::uint64_t v, int n) {
  if (b.size() < at + static_cast<std::size_t>(n)) b.resize(at + static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) b[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
}

void pad4(std::vector<std::uint8_t>& b) {
  while (b.size() % 4 != 0) b.push_back(0);
}

// One version-resource block: header, key, value, children, each part aligned to 4 from the blob's start (`base` is
// where this block starts in the blob, so that its alignment is the blob's).
std::vector<std::uint8_t> block(std::size_t base, const std::u16string& key, std::uint16_t type,
                                const std::vector<std::uint8_t>& value, std::uint16_t value_length,
                                const std::vector<std::vector<std::uint8_t>>& children) {
  std::vector<std::uint8_t> b(6, 0);
  for (const char16_t c : key) put(b, b.size(), c, 2);
  put(b, b.size(), 0, 2);
  while ((base + b.size()) % 4 != 0) b.push_back(0);
  b.insert(b.end(), value.begin(), value.end());
  for (const auto& child : children) {
    while ((base + b.size()) % 4 != 0) b.push_back(0);
    b.insert(b.end(), child.begin(), child.end());
  }
  put(b, 0, b.size(), 2);
  put(b, 2, value_length, 2);
  put(b, 4, type, 2);
  return b;
}

std::vector<std::uint8_t> utf16_value(const std::u16string& s) {
  std::vector<std::uint8_t> v;
  for (const char16_t c : s) put(v, v.size(), c, 2);
  put(v, v.size(), 0, 2);
  return v;
}

std::u16string ascii16(const std::string& s) { return std::u16string(s.begin(), s.end()); }

std::vector<std::uint8_t> version_blob(const FakeVersion& v) {
  std::vector<std::uint8_t> fixed;
  put(fixed, 0, 0xFEEF04BD, 4);
  put(fixed, 4, 0x00010000, 4);
  put(fixed, 8, (std::uint32_t{v.file_version[0]} << 16) | v.file_version[1], 4);
  put(fixed, 12, (std::uint32_t{v.file_version[2]} << 16) | v.file_version[3], 4);
  put(fixed, 16, (std::uint32_t{v.product_version[0]} << 16) | v.product_version[1], 4);
  put(fixed, 20, (std::uint32_t{v.product_version[2]} << 16) | v.product_version[3], 4);
  put(fixed, 24, 0x3F, 4);
  put(fixed, 28, 0, 4);
  put(fixed, 32, 0x00040004, 4);
  put(fixed, 36, 1, 4);
  put(fixed, 40, 0, 4);
  put(fixed, 44, 0, 4);
  put(fixed, 48, 0, 4);

  // The children are built at offset 0 first to learn their sizes, then again where they will sit, so that their
  // padding is the blob's. Root header 6, key "VS_VERSION_INFO" 32, padding to 40, the fixed info 52: children at 92.
  const std::size_t children_at = 92;
  std::vector<std::vector<std::uint8_t>> root_children;
  std::size_t at = children_at;
  {
    std::string key = v.table_key;
    if (key.empty() && v.translation) {
      char buf[16];
      std::snprintf(buf, sizeof buf, "%04x%04x", v.translation->language, v.translation->codepage);
      key = buf;
    }
    if (key.empty()) key = "000004b0";
    // StringFileInfo { StringTable { String ProductName } }, built from the inside out at their final offsets.
    const std::size_t sfi_at = at;
    const std::size_t table_at = align(static_cast<std::uint32_t>(sfi_at + 6 + 2 * (14 + 1)), 4);
    const std::size_t string_at = align(static_cast<std::uint32_t>(table_at + 6 + 2 * (key.size() + 1)), 4);
    std::vector<std::vector<std::uint8_t>> strings;
    if (v.product_name) {
      const std::vector<std::uint8_t> value = utf16_value(*v.product_name);
      strings.push_back(
          block(string_at, u"ProductName", 1, value, static_cast<std::uint16_t>(v.product_name->size() + 1), {}));
    }
    const std::vector<std::uint8_t> table = block(table_at, ascii16(key), 1, {}, 0, strings);
    const std::vector<std::uint8_t> sfi = block(sfi_at, u"StringFileInfo", 1, {}, 0, {table});
    root_children.push_back(sfi);
    at = align(static_cast<std::uint32_t>(at + sfi.size()), 4);
  }
  if (v.translation) {
    const std::size_t vfi_at = at;
    const std::size_t var_at = align(static_cast<std::uint32_t>(vfi_at + 6 + 2 * (11 + 1)), 4);
    std::vector<std::uint8_t> value;
    put(value, 0, v.translation->language, 2);
    put(value, 2, v.translation->codepage, 2);
    const std::vector<std::uint8_t> var = block(var_at, u"Translation", 0, value, 4, {});
    root_children.push_back(block(vfi_at, u"VarFileInfo", 1, {}, 0, {var}));
  }
  return block(0, u"VS_VERSION_INFO", 0, fixed, 52, root_children);
}

}  // namespace

BuiltImage build(const FakeImageSpec& spec) {
  std::vector<FakeSection> sections = spec.sections;
  BuiltImage out;

  // RVAs first: the generated sections' contents need them.
  std::uint32_t rva = kSectionAlign;
  std::vector<std::uint32_t> rvas;
  const auto place = [&](std::uint32_t size) {
    rvas.push_back(rva);
    const std::uint32_t at = rva;
    rva = align(rva + std::max<std::uint32_t>(size, 1), kSectionAlign);
    return at;
  };
  for (const FakeSection& s : sections)
    place(s.virtual_size != 0 ? s.virtual_size : static_cast<std::uint32_t>(s.bytes.size()));
  const auto text =
      std::find_if(sections.begin(), sections.end(), [](const FakeSection& s) { return s.name == ".text"; });
  const std::uint32_t text_rva = text == sections.end() ? 0 : rvas[static_cast<std::size_t>(text - sections.begin())];

  std::vector<std::uint8_t> rsrc;
  if (spec.version) {
    const std::vector<std::uint8_t> blob = version_blob(*spec.version);
    const std::uint32_t rsrc_rva = rva;  // where the next section goes
    // root dir at 0, name dir at 24, language dir at 48, data entry at 72, data at 88
    put(rsrc, 12, 0, 2);
    put(rsrc, 14, 1, 2);
    put(rsrc, 16, 16, 4);  // RT_VERSION
    put(rsrc, 20, 0x80000000u | 24, 4);
    put(rsrc, 24 + 14, 1, 2);
    put(rsrc, 24 + 16, 1, 4);  // name id 1
    put(rsrc, 24 + 20, 0x80000000u | 48, 4);
    put(rsrc, 48 + 14, 1, 2);
    put(rsrc, 48 + 16, spec.version->language, 4);
    put(rsrc, 48 + 20, 72, 4);  // a data entry
    put(rsrc, 72, rsrc_rva + 88, 4);
    put(rsrc, 76, blob.size(), 4);
    rsrc.resize(88);
    rsrc.insert(rsrc.end(), blob.begin(), blob.end());
    sections.push_back(FakeSection{".rsrc", kReadOnly, rsrc, 0});
    out.rsrc_rva = place(static_cast<std::uint32_t>(rsrc.size()));
    out.version_rva = rsrc_rva + 88;
  }

  if (!spec.functions.empty()) {
    if (text == sections.end()) throw std::logic_error("functions need a .text");
    const std::uint32_t xdata_rva = rva;
    std::vector<std::uint8_t> xdata;
    std::vector<std::uint32_t> unwind(spec.functions.size());
    const std::uint32_t pdata_rva =
        align(xdata_rva + static_cast<std::uint32_t>(16 * spec.functions.size() + 16), kSectionAlign);
    for (std::size_t i = 0; i < spec.functions.size(); ++i) {
      const FakeFunction& f = spec.functions[i];
      pad4(xdata);
      unwind[i] = xdata_rva + static_cast<std::uint32_t>(xdata.size());
      if (f.chained && i > 0) {
        put(xdata, xdata.size(), 0x01 | (0x4 << 3), 1);  // version 1, UNW_FLAG_CHAININFO
        put(xdata, xdata.size(), 0, 3);                  // no prologue, no codes, no frame register
        const FakeFunction& parent = spec.functions[i - 1];
        put(xdata, xdata.size(), text_rva + parent.begin, 4);
        put(xdata, xdata.size(), text_rva + parent.end, 4);
        put(xdata, xdata.size(), unwind[i - 1], 4);
      } else {
        put(xdata, xdata.size(), 0x01, 1);
        put(xdata, xdata.size(), 0, 3);
      }
    }
    sections.push_back(FakeSection{".xdata", kReadOnly, xdata, 0});
    place(static_cast<std::uint32_t>(xdata.size()));
    std::vector<std::uint8_t> pdata;
    for (std::size_t i = 0; i < spec.functions.size(); ++i) {
      const FakeFunction& f = spec.functions[i];
      put(pdata, pdata.size(), text_rva + f.begin, 4);
      put(pdata, pdata.size(), text_rva + f.end, 4);
      const std::uint32_t entry_before = pdata_rva + static_cast<std::uint32_t>(12 * (i > 0 ? i - 1 : 0));
      put(pdata, pdata.size(), f.indirect && i > 0 ? entry_before | 1u : unwind[i], 4);
    }
    sections.push_back(FakeSection{".pdata", kReadOnly, pdata, 0});
    out.pdata_rva = place(static_cast<std::uint32_t>(pdata.size()));
    if (out.pdata_rva != pdata_rva) throw std::logic_error("the .pdata RVA came out other than planned");
  }
  std::vector<std::uint8_t> reloc;
  if (!spec.fixups.empty()) {
    // One block per page, each a page RVA, its size, and 16-bit entries (type 10, DIR64, and the offset in the page).
    std::vector<std::uint32_t> sorted = spec.fixups;
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t i = 0; i < sorted.size();) {
      const std::uint32_t page = sorted[i] & ~0xFFFu;
      const std::size_t block = reloc.size();
      put(reloc, block, page, 4);
      put(reloc, block + 4, 0, 4);
      while (i < sorted.size() && (sorted[i] & ~0xFFFu) == page) {
        put(reloc, reloc.size(), 0xA000u | (sorted[i] & 0xFFFu), 2);
        ++i;
      }
      if (reloc.size() % 4 != 0) put(reloc, reloc.size(), 0, 2);  // IMAGE_REL_BASED_ABSOLUTE: padding
      put(reloc, block + 4, reloc.size() - block, 4);
    }
    sections.push_back(FakeSection{".reloc", kReadOnly, reloc, 0});
    out.reloc_rva = place(static_cast<std::uint32_t>(reloc.size()));
  }
  out.size_of_image = spec.size_of_image.value_or(rva);

  // The headers.
  std::vector<std::uint8_t> headers(kHeadersSize, 0);
  put(headers, 0, 0x5A4D, 2);
  put(headers, 0x3C, kNtOffset, 4);
  put(headers, kNtOffset, 0x00004550, 4);
  put(headers, kFileHeaderAt, 0x8664, 2);
  put(headers, kFileHeaderAt + 2, sections.size(), 2);
  put(headers, kFileHeaderAt + 4, spec.time_date_stamp, 4);
  put(headers, kFileHeaderAt + 16, kOptionalSize, 2);
  put(headers, kFileHeaderAt + 18, 0x0022, 2);  // executable, large-address aware
  put(headers, kOptionalAt, 0x20B, 2);
  put(headers, kOptionalAt + 16, text_rva, 4);  // the entry point: .text's start
  put(headers, kOptionalAt + 24, spec.image_base, 8);
  put(headers, kOptionalAt + 32, kSectionAlign, 4);
  put(headers, kOptionalAt + 36, kFileAlign, 4);
  put(headers, kOptionalAt + 56, out.size_of_image, 4);
  put(headers, kOptionalAt + 60, kHeadersSize, 4);
  put(headers, kOptionalAt + 68, 3, 2);  // console
  put(headers, kOptionalAt + 70, 0x8160, 2);
  put(headers, kOptionalAt + 108, 16, 4);
  if (spec.version) {
    put(headers, kOptionalAt + 112 + 8 * 2, out.rsrc_rva, 4);
    put(headers, kOptionalAt + 112 + 8 * 2 + 4, rsrc.size(), 4);
  }
  if (!spec.functions.empty()) {
    put(headers, kOptionalAt + 112 + 8 * 3, out.pdata_rva, 4);
    put(headers, kOptionalAt + 112 + 8 * 3 + 4, 12 * spec.functions.size(), 4);
  }
  if (!spec.fixups.empty()) {
    put(headers, kOptionalAt + 112 + 8 * 5, out.reloc_rva, 4);
    put(headers, kOptionalAt + 112 + 8 * 5 + 4, reloc.size(), 4);
  }
  std::uint32_t raw = kHeadersSize;
  std::vector<std::uint32_t> raws;      // PointerToRawData as written
  std::vector<std::uint32_t> raw_sizes;  // SizeOfRawData as written
  std::vector<std::uint32_t> virtual_sizes;
  for (std::size_t i = 0; i < sections.size(); ++i) {
    const FakeSection& s = sections[i];
    const std::size_t h = kSectionTableAt + 40 * i;
    for (std::size_t c = 0; c < s.name.size() && c < 8; ++c) headers[h + c] = static_cast<std::uint8_t>(s.name[c]);
    const std::uint32_t size = static_cast<std::uint32_t>(s.bytes.size());
    std::uint32_t raw_size = align(size, kFileAlign);
    std::uint32_t raw_at = raw_size != 0 ? raw : 0;
    const auto by_hand =
        std::find_if(spec.raw.begin(), spec.raw.end(), [&](const FakeRaw& r) { return r.section == i; });
    if (by_hand != spec.raw.end()) {
      raw_size = by_hand->raw_size;
      raw_at = by_hand->raw_offset;
    } else {
      raw += raw_size;
    }
    virtual_sizes.push_back(s.virtual_size != 0 ? s.virtual_size : size);
    put(headers, h + 8, virtual_sizes.back(), 4);
    put(headers, h + 12, rvas[i], 4);
    put(headers, h + 16, raw_size, 4);
    put(headers, h + 20, raw_at, 4);
    put(headers, h + 36, s.characteristics, 4);
    raws.push_back(raw_at);
    raw_sizes.push_back(raw_size);
    out.rva[s.name] = rvas[i];
  }

  // The file: the headers, then each section's bytes where its PointerToRawData says.
  std::vector<std::uint8_t> file(raw, 0);
  std::copy(headers.begin(), headers.end(), file.begin());
  // Bytes at or past a given file size are not written at all, so that a raw range may lie far past the file's end.
  const std::size_t limit = spec.file_size.value_or(0xFFFFFFFFu);
  for (std::size_t i = 0; i < sections.size(); ++i) {
    const std::size_t end = std::min<std::size_t>(std::size_t{raws[i]} + sections[i].bytes.size(), limit);
    if (raws[i] == 0 || raws[i] >= end) continue;
    if (file.size() < end) file.resize(end, 0);
    std::copy_n(sections[i].bytes.begin(), end - raws[i], file.begin() + raws[i]);
  }
  if (spec.file_size) file.resize(*spec.file_size, 0);

  // The image as the loader maps the file, written plainly here: each section's raw data (none when SizeOfRawData or
  // PointerToRawData is 0), at most its virtual size, as far as the file holds it; zeros for the rest.
  std::vector<std::uint8_t> memory;
  if (spec.with_memory) {
    memory.assign(std::max<std::size_t>(rva, out.size_of_image), 0);
    std::copy(headers.begin(), headers.end(), memory.begin());
    for (std::size_t i = 0; i < sections.size(); ++i) {
      if (raws[i] == 0 || raw_sizes[i] == 0) continue;
      const std::size_t mapped = virtual_sizes[i] != 0 ? virtual_sizes[i] : raw_sizes[i];
      for (std::size_t k = 0; k < std::min<std::size_t>(raw_sizes[i], mapped); ++k) {
        if (std::size_t{raws[i]} + k >= file.size() || std::size_t{rvas[i]} + k >= memory.size()) break;
        memory[rvas[i] + k] = file[raws[i] + k];
      }
    }
  }
  out.file.resize(file.size());
  std::transform(file.begin(), file.end(), out.file.begin(), [](std::uint8_t b) { return std::byte{b}; });
  out.memory.resize(memory.size());
  std::transform(memory.begin(), memory.end(), out.memory.begin(), [](std::uint8_t b) { return std::byte{b}; });
  return out;
}

void put_u16(std::vector<std::byte>& bytes, std::size_t at, std::uint16_t v) {
  bytes.at(at) = std::byte{static_cast<std::uint8_t>(v)};
  bytes.at(at + 1) = std::byte{static_cast<std::uint8_t>(v >> 8)};
}

void put_u32(std::vector<std::byte>& bytes, std::size_t at, std::uint32_t v) {
  for (std::size_t i = 0; i < 4; ++i) bytes.at(at + i) = std::byte{static_cast<std::uint8_t>(v >> (8 * i))};
}

bool HoleSource::read(std::uint64_t offset, std::span<std::byte> dst) const noexcept {
  ++reads;
  if (offset > bytes_.size() || dst.size() > bytes_.size() - offset) return false;
  if (offset < to_ && offset + dst.size() > from_) return false;  // touches the hole
  std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(offset), dst.size(), dst.begin());
  return true;
}

}  // namespace en::gal::test
