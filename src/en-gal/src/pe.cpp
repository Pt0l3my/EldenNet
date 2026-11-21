// en-gal's PE32+ reader (pe.hpp). Every structure is copied out of the source into a buffer of its own and read from
// there with bounds-checked little-endian reads (bytes.hpp); every offset the image gives is checked against the image
// before it is followed, and nothing is followed twice: the resource tree is read to its fixed depth of three, and the
// version resource's blocks within their parents.
#include "en/gal/pe.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>

#include "bytes.hpp"

namespace en::gal {

using proto::Expected;
using proto::Unexpected;

bool SpanSource::read(std::uint64_t offset, std::span<std::byte> dst) const noexcept {
  if (!bytes::within(offset, dst.size(), bytes_.size())) return false;
  const std::span<const std::byte> from = bytes_.subspan(static_cast<std::size_t>(offset), dst.size());
  std::copy(from.begin(), from.end(), dst.begin());
  return true;
}

const Section* ImageInfo::section(std::string_view name) const noexcept {
  for (const Section& s : sections) {
    if (s.name == name) return &s;
  }
  return nullptr;
}

const Section* ImageInfo::section_at(std::uint32_t rva) const noexcept {
  for (const Section& s : sections) {
    if (s.contains(rva)) return &s;
  }
  return nullptr;
}

FileSpan ImageInfo::file_span(const Section& s) const noexcept {
  // The loader ignores PointerToRawData when SizeOfRawData is 0, maps at most the virtual size of the raw data, and
  // reads nothing past the end of the file: the rest of the section is zeros. A PointerToRawData of 0 marks a section
  // without raw data, as the specification has it for uninitialised data.
  if (layout != ImageLayout::File || s.raw_size == 0 || s.raw_offset == 0 || s.raw_offset >= source_size) return {};
  const std::uint64_t held = std::min<std::uint64_t>({s.raw_size, s.mapped_size(), source_size - s.raw_offset});
  return FileSpan{s.raw_offset, static_cast<std::uint32_t>(held)};
}

bool ImageInfo::maps(std::uint32_t rva, std::uint32_t len) const noexcept {
  if (rva < size_of_headers) return bytes::within(rva, len, size_of_headers);
  const Section* s = section_at(rva);
  return s != nullptr && bytes::within(rva - s->rva, len, s->mapped_size());
}

std::optional<std::uint64_t> ImageInfo::offset_of(std::uint32_t rva, std::uint32_t len) const noexcept {
  if (rva < size_of_headers) {  // the headers lie at the same place in a file and in memory
    if (!bytes::within(rva, len, size_of_headers)) return std::nullopt;
    return rva;
  }
  const Section* s = section_at(rva);
  if (s == nullptr) return std::nullopt;
  const std::uint32_t into = rva - s->rva;
  if (layout == ImageLayout::Memory) {
    if (!bytes::within(into, len, s->mapped_size())) return std::nullopt;
    return rva;
  }
  const FileSpan held = file_span(*s);
  if (!bytes::within(into, len, held.size)) return std::nullopt;  // past what the file holds of it
  return held.offset + into;
}

bool read_mapped(const ImageInfo& image, const ByteSource& source, std::uint32_t rva,
                 std::span<std::byte> dst) noexcept {
  if (dst.size() > 0xFFFFFFFFu) return false;
  const auto len = static_cast<std::uint32_t>(dst.size());
  if (!image.maps(rva, len)) return false;
  // The headers lie at the same place in a file and in memory, and a mapped image holds every section at its RVA.
  if (image.layout == ImageLayout::Memory || rva < image.size_of_headers) return source.read(rva, dst);
  const Section* s = image.section_at(rva);
  if (s == nullptr) return false;
  const FileSpan held = image.file_span(*s);
  const std::uint64_t into = rva - s->rva;
  const std::uint64_t from_file = into < held.size ? std::min<std::uint64_t>(len, held.size - into) : 0;
  if (from_file != 0 && !source.read(held.offset + into, dst.first(static_cast<std::size_t>(from_file)))) return false;
  const std::span<std::byte> zeros = dst.subspan(static_cast<std::size_t>(from_file));
  std::fill(zeros.begin(), zeros.end(), std::byte{0});
  return true;
}

const RuntimeFunction* ImageInfo::function_at(std::uint32_t rva) const noexcept {
  auto it = std::upper_bound(functions.begin(), functions.end(), rva,
                             [](std::uint32_t v, const RuntimeFunction& f) { return v < f.begin; });
  if (it == functions.begin()) return nullptr;
  --it;
  return rva < it->end ? &*it : nullptr;
}

const char* to_string(PeProblem problem) noexcept {
  switch (problem) {
    case PeProblem::Unreadable: return "the source could not give the bytes the headers name";
    case PeProblem::NoDosHeader: return "there is no DOS header";
    case PeProblem::BadNtHeaderOffset: return "the NT headers' offset is out of bounds";
    case PeProblem::NoPeSignature: return "there is no PE signature";
    case PeProblem::NotAmd64: return "the machine is not x64";
    case PeProblem::NotPe32Plus: return "the optional header is not PE32+";
    case PeProblem::BadOptionalHeader: return "the optional header is malformed";
    case PeProblem::BadAlignment: return "the section or file alignment is not a power of two, or not in order";
    case PeProblem::BadSizes: return "SizeOfHeaders or SizeOfImage is impossible";
    case PeProblem::TooManySections: return "there are too many sections";
    case PeProblem::BadSectionTable: return "the section table runs past the headers";
    case PeProblem::BadSection: return "a section is misplaced";
    case PeProblem::BadResourceDirectory: return "the resource directory is malformed";
    case PeProblem::BadVersionResource: return "the version resource is malformed";
    case PeProblem::BadExceptionDirectory: return "the exception directory is malformed";
    case PeProblem::TooManyFunctions: return "the exception directory has too many entries";
    case PeProblem::BadRelocations: return "the base relocation table is malformed";
  }
  return "the image is malformed";
}

namespace {

std::string hex(std::uint64_t v) {
  std::array<char, 24> text{};
  std::snprintf(text.data(), text.size(), "0x%llx", static_cast<unsigned long long>(v));
  return std::string(text.data());
}

}  // namespace

std::string describe_numbers(std::uint32_t rva, std::uint32_t virtual_size, std::uint32_t raw_offset,
                             std::uint32_t raw_size, std::uint32_t characteristics) {
  return "va " + hex(rva) + " vsize " + hex(virtual_size) + " raw " + hex(raw_offset) + " rawsize " + hex(raw_size) +
         " flags " + hex(characteristics);
}

std::string describe(const PeError& error) {
  std::string out = to_string(error.problem);
  if (error.section) {
    const SectionFault& f = *error.section;
    out += ": section " + std::to_string(f.index);
    switch (f.rule) {
      case SectionRule::Unaligned: out += " is not aligned to the section alignment, " + hex(f.bound); break;
      case SectionRule::Overlaps:
        out += f.index == 0 ? std::string(" starts below the end of the headers, ")
                            : " starts below the end of section " + std::to_string(f.index - 1) + ", ";
        out += hex(f.bound);
        break;
      case SectionRule::PastImage: out += " ends past the end of the image, " + hex(f.bound); break;
      case SectionRule::RawWraps: out += "'s raw data ends past 4 GiB"; break;
    }
    out += "; " + describe_numbers(f.rva, f.virtual_size, f.raw_offset, f.raw_size, f.characteristics);
  }
  return out + " (at " + hex(error.at) + ")";
}

namespace {

constexpr std::uint32_t kMaxHeaderOffset = 0x10000;  // e_lfanew within the first 64 KiB
constexpr std::uint32_t kFileHeaderSize = 20;
constexpr std::uint32_t kOptionalFixedSize = 112;  // PE32+'s optional header before its data directories
constexpr std::uint32_t kMaxOptionalHeaderSize = 0x1000;
constexpr std::uint32_t kSectionHeaderSize = 40;
constexpr std::uint32_t kRuntimeFunctionSize = 12;
constexpr std::uint16_t kMachineAmd64 = 0x8664;
constexpr std::uint16_t kMagicPe32Plus = 0x20B;
constexpr std::uint32_t kDirResource = 2;
constexpr std::uint32_t kDirException = 3;
constexpr std::uint32_t kResourceTypeVersion = 16;  // RT_VERSION
constexpr std::uint32_t kFixedFileInfoSize = 52;
constexpr std::uint32_t kFixedFileInfoSignature = 0xFEEF04BD;

PeError at(PeProblem problem, std::uint64_t where) { return PeError{problem, where}; }

// Copies `len` bytes at `offset` of the source, or answers why not.
Expected<std::vector<std::byte>, PeError> fetch(const ByteSource& source, std::uint64_t offset, std::uint64_t len) {
  if (!bytes::within(offset, len, source.size())) return Unexpected{at(PeProblem::Unreadable, offset)};
  std::vector<std::byte> out(static_cast<std::size_t>(len));
  if (!source.read(offset, out)) return Unexpected{at(PeProblem::Unreadable, offset)};
  return out;
}

// Copies [rva, rva + len) of the image as the loader maps it, where it lies within the headers or one section.
Expected<std::vector<std::byte>, PeError> fetch_rva(const ImageInfo& image, const ByteSource& source, std::uint32_t rva,
                                                    std::uint32_t len, PeProblem otherwise) {
  if (!image.maps(rva, len)) return Unexpected{at(otherwise, rva)};
  std::vector<std::byte> out(len);
  if (!read_mapped(image, source, rva, out)) return Unexpected{at(PeProblem::Unreadable, rva)};
  return out;
}

// ---- Sections ----

std::string section_name(std::span<const std::byte> raw) {
  std::string name;
  for (const std::byte b : raw) {
    const auto c = std::to_integer<unsigned char>(b);
    if (c == 0) break;
    name += static_cast<char>(c);
  }
  return name;
}

// The loader's rules on sections (SectionRule), and nothing more: their virtual ranges ascending, disjoint, aligned
// and within the image, and no raw range that wraps. Raw ranges are otherwise free: in any order, overlapping, shared,
// empty, or past the end of the file (file_span reads what the file holds, and the rest maps as zeros). A section of
// no size at all is let through: nothing of it is ever read.
Expected<void, PeError> check_sections(const ImageInfo& image, std::uint64_t table_at) {
  // The loader maps whole pages and rounds SizeOfImage up to them; a section's span is rounded to SectionAlignment.
  constexpr std::uint32_t kPage = 0x1000;
  const std::uint64_t image_end =
      bytes::align_up(image.size_of_image, std::max(image.section_alignment, kPage)).value_or(0);
  std::uint64_t next_free = image.size_of_headers;  // the lowest RVA the next section may start at
  for (std::size_t i = 0; i < image.sections.size(); ++i) {
    const Section& s = image.sections[i];
    const auto refuse = [&](SectionRule rule, std::uint64_t bound) {
      SectionFault f;
      f.rule = rule;
      f.index = static_cast<std::uint32_t>(i);
      f.bound = bound;
      f.rva = s.rva;
      f.virtual_size = s.virtual_size;
      f.raw_offset = s.raw_offset;
      f.raw_size = s.raw_size;
      f.characteristics = s.characteristics;
      PeError e = at(PeProblem::BadSection, table_at + std::uint64_t{kSectionHeaderSize} * i);
      e.section = f;
      return Unexpected{e};
    };
    if (s.rva % image.section_alignment != 0) return refuse(SectionRule::Unaligned, image.section_alignment);
    if (s.rva < next_free) return refuse(SectionRule::Overlaps, next_free);
    // Both below 2^32 and the alignment a power of two of 32 bits: the sum cannot wrap 64 bits.
    const std::uint64_t end = std::uint64_t{s.rva} + bytes::align_up(s.mapped_size(), image.section_alignment).value_or(0);
    if (end > image_end) return refuse(SectionRule::PastImage, image_end);
    if (s.raw_size != 0 && std::uint64_t{s.raw_offset} + s.raw_size > 0xFFFFFFFFu) {
      return refuse(SectionRule::RawWraps, 0);
    }
    next_free = end;
  }
  return {};
}

// ---- Resources ----

struct ResourceEntry {
  bool named = false;
  std::uint32_t id = 0;      // when not named
  bool directory = false;    // a subdirectory, or a data entry
  std::uint32_t offset = 0;  // from the resource root
};

// The entries of the resource directory at `offset` from the root.
Expected<std::vector<ResourceEntry>, PeError> read_directory(const ImageInfo& image, const ByteSource& source,
                                                             std::uint32_t root, std::uint32_t root_size,
                                                             std::uint32_t offset, const PeLimits& limits) {
  const PeError bad = at(PeProblem::BadResourceDirectory, std::uint64_t{root} + offset);
  if (!bytes::within(offset, 16, root_size)) return Unexpected{bad};
  const auto header = fetch_rva(image, source, root + offset, 16, PeProblem::BadResourceDirectory);
  if (!header) return Unexpected{header.error()};
  const std::uint32_t named = bytes::u16(*header, 12).value_or(0);
  const std::uint32_t ids = bytes::u16(*header, 14).value_or(0);
  const std::uint32_t count = named + ids;
  if (count > limits.max_resource_entries ||
      !bytes::within(std::uint64_t{offset} + 16, std::uint64_t{count} * 8, root_size)) {
    return Unexpected{bad};
  }
  std::vector<ResourceEntry> entries;
  if (count == 0) return entries;
  const auto raw = fetch_rva(image, source, root + offset + 16, count * 8, PeProblem::BadResourceDirectory);
  if (!raw) return Unexpected{raw.error()};
  entries.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::uint32_t name = bytes::u32(*raw, std::uint64_t{i} * 8).value_or(0);
    const std::uint32_t data = bytes::u32(*raw, std::uint64_t{i} * 8 + 4).value_or(0);
    ResourceEntry e;
    e.named = (name & 0x80000000u) != 0;
    e.id = e.named ? 0 : (name & 0xFFFFu);
    e.directory = (data & 0x80000000u) != 0;
    e.offset = data & 0x7FFFFFFFu;
    entries.push_back(e);
  }
  return entries;
}

// The RT_VERSION resource's bytes and language, or nothing when the image has none.
Expected<std::optional<std::pair<std::vector<std::byte>, std::uint16_t>>, PeError> find_version(
    const ImageInfo& image, const ByteSource& source, std::uint32_t root, std::uint32_t root_size,
    const PeLimits& limits) {
  const auto types = read_directory(image, source, root, root_size, 0, limits);
  if (!types) return Unexpected{types.error()};
  const auto type = std::find_if(types->begin(), types->end(),
                                 [](const ResourceEntry& e) { return !e.named && e.id == kResourceTypeVersion; });
  if (type == types->end()) return std::nullopt;
  const PeError bad = at(PeProblem::BadResourceDirectory, root);
  if (!type->directory) return Unexpected{bad};
  const auto names = read_directory(image, source, root, root_size, type->offset, limits);
  if (!names) return Unexpected{names.error()};
  if (names->empty() || !names->front().directory) return Unexpected{bad};
  const auto languages = read_directory(image, source, root, root_size, names->front().offset, limits);
  if (!languages) return Unexpected{languages.error()};
  if (languages->empty() || languages->front().directory) return Unexpected{bad};
  const ResourceEntry& leaf = languages->front();
  if (!bytes::within(leaf.offset, 16, root_size)) return Unexpected{bad};
  const auto entry = fetch_rva(image, source, root + leaf.offset, 16, PeProblem::BadResourceDirectory);
  if (!entry) return Unexpected{entry.error()};
  const std::uint32_t data_rva = bytes::u32(*entry, 0).value_or(0);
  const std::uint32_t data_size = bytes::u32(*entry, 4).value_or(0);
  if (data_size == 0 || data_size > limits.max_version_bytes) {
    return Unexpected{at(PeProblem::BadVersionResource, data_rva)};
  }
  auto data = fetch_rva(image, source, data_rva, data_size, PeProblem::BadVersionResource);
  if (!data) return Unexpected{data.error()};
  const std::uint16_t language = leaf.named ? std::uint16_t{0} : static_cast<std::uint16_t>(leaf.id);
  return std::optional<std::pair<std::vector<std::byte>, std::uint16_t>>(std::in_place, std::move(*data), language);
}

// ---- The version resource's blocks (VS_VERSIONINFO, StringFileInfo, StringTable, String, VarFileInfo, Var) ----

struct Block {
  std::uint32_t end = 0;           // one past its last byte
  std::uint16_t value_length = 0;  // bytes for binary values, UTF-16 units for text
  std::uint16_t type = 0;          // 0 binary, 1 text
  std::string key;                 // ASCII; any other unit as '?'
  std::uint32_t value_start = 0;
  std::uint32_t value_end = 0;  // clamped to the block for text values
  std::uint32_t children = 0;   // where its first child would start
};

constexpr std::uint32_t align4(std::uint32_t v) noexcept { return (v + 3u) & ~3u; }

// The block at `start`, which must end by `limit`.
std::optional<Block> read_block(std::span<const std::byte> v, std::uint32_t start, std::uint32_t limit) {
  if (start > limit || limit - start < 8) return std::nullopt;
  const std::uint16_t length = bytes::u16(v, start).value_or(0);
  if (length < 8 || length > limit - start) return std::nullopt;
  Block b;
  b.end = start + length;
  b.value_length = bytes::u16(v, start + 2).value_or(0);
  b.type = bytes::u16(v, start + 4).value_or(0);
  if (b.type > 1) return std::nullopt;
  std::uint32_t k = start + 6;
  bool ended = false;
  while (k + 2 <= b.end) {
    const std::uint16_t unit = bytes::u16(v, k).value_or(0);
    k += 2;
    if (unit == 0) {
      ended = true;
      break;
    }
    if (b.key.size() >= 64) return std::nullopt;
    b.key += unit < 0x80 ? static_cast<char>(unit) : '?';
  }
  if (!ended) return std::nullopt;
  b.value_start = std::min(align4(k), b.end);
  const std::uint32_t value_bytes = b.type == 1 ? 2u * b.value_length : b.value_length;
  if (value_bytes > b.end - b.value_start) {
    if (b.type == 0) return std::nullopt;  // a binary value must fit; text values are clamped, as producers differ
    b.value_end = b.end;
  } else {
    b.value_end = b.value_start + value_bytes;
  }
  b.children = std::min(align4(b.value_end), b.end);
  return b;
}

// Calls `visit` with each child block of `parent`; false when one is malformed.
template <class F>
bool each_child(std::span<const std::byte> v, const Block& parent, F&& visit) {
  std::uint32_t c = parent.children;
  while (c < parent.end && parent.end - c >= 8) {
    const std::optional<Block> child = read_block(v, c, parent.end);
    if (!child) return false;
    if (!visit(*child)) return false;
    c = std::min(align4(child->end), parent.end);
  }
  return true;
}

// UTF-16LE units [from, to), up to the first NUL and at most `max_units`, as UTF-8; nothing if not valid UTF-16.
std::optional<std::string> utf16_text(std::span<const std::byte> v, std::uint32_t from, std::uint32_t to,
                                      std::uint32_t max_units) {
  std::string out;
  std::uint32_t units = 0;
  for (std::uint32_t k = from; k + 2 <= to && units < max_units; k += 2, ++units) {
    std::uint32_t cp = bytes::u16(v, k).value_or(0);
    if (cp == 0) break;
    if (cp >= 0xDC00 && cp <= 0xDFFF) return std::nullopt;  // a low surrogate first
    if (cp >= 0xD800 && cp <= 0xDBFF) {
      if (k + 4 > to || units + 1 >= max_units) return std::nullopt;
      const std::uint32_t low = bytes::u16(v, k + 2).value_or(0);
      if (low < 0xDC00 || low > 0xDFFF) return std::nullopt;
      cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
      k += 2;
      ++units;
    }
    if (cp < 0x80) {
      out += static_cast<char>(cp);
    } else if (cp < 0x800) {
      out += static_cast<char>(0xC0 | (cp >> 6));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      out += static_cast<char>(0xE0 | (cp >> 12));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      out += static_cast<char>(0xF0 | (cp >> 18));
      out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }
  return out;
}

bool same_key(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
    if (lower(a[i]) != lower(b[i])) return false;
  }
  return true;
}

std::string table_key(const Translation& t) {
  std::array<char, 16> key{};
  std::snprintf(key.data(), key.size(), "%04x%04x", static_cast<unsigned>(t.language),
                static_cast<unsigned>(t.codepage));
  return std::string(key.data());
}

std::array<std::uint16_t, 4> version_of(std::uint32_t ms, std::uint32_t ls) {
  return {static_cast<std::uint16_t>(ms >> 16), static_cast<std::uint16_t>(ms & 0xFFFF),
          static_cast<std::uint16_t>(ls >> 16), static_cast<std::uint16_t>(ls & 0xFFFF)};
}

Expected<VersionInfo, PeError> parse_version(std::span<const std::byte> v, std::uint32_t rva, std::uint16_t language,
                                             const PeLimits& limits) {
  const PeError bad = at(PeProblem::BadVersionResource, rva);
  const std::optional<Block> root = read_block(v, 0, static_cast<std::uint32_t>(v.size()));
  if (!root || root->key != "VS_VERSION_INFO" || root->type != 0 || root->value_length != kFixedFileInfoSize) {
    return Unexpected{bad};
  }
  const std::uint32_t f = root->value_start;
  if (bytes::u32(v, f).value_or(0) != kFixedFileInfoSignature) return Unexpected{bad};
  VersionInfo info;
  info.resource_language = language;
  info.file_version = version_of(bytes::u32(v, f + 8).value_or(0), bytes::u32(v, f + 12).value_or(0));
  info.product_version = version_of(bytes::u32(v, f + 16).value_or(0), bytes::u32(v, f + 20).value_or(0));

  std::vector<Block> tables;  // StringFileInfo's StringTables, in order
  const bool ok = each_child(v, *root, [&](const Block& child) {
    if (child.key == "StringFileInfo") {
      return each_child(v, child, [&](const Block& table) {
        tables.push_back(table);
        return tables.size() <= 64;
      });
    }
    if (child.key == "VarFileInfo") {
      return each_child(v, child, [&](const Block& var) {
        if (var.key == "Translation" && !info.translation && var.value_end - var.value_start >= 4) {
          info.translation =
              Translation{bytes::u16(v, var.value_start).value_or(0), bytes::u16(v, var.value_start + 2).value_or(0)};
        }
        return true;
      });
    }
    return true;
  });
  if (!ok) return Unexpected{bad};

  const Block* table = tables.empty() ? nullptr : &tables.front();
  if (info.translation) {
    const std::string wanted = table_key(*info.translation);
    for (const Block& t : tables) {
      if (same_key(t.key, wanted)) {
        table = &t;
        break;
      }
    }
  }
  if (table != nullptr) {
    const bool strings_ok = each_child(v, *table, [&](const Block& s) {
      if (s.key == "ProductName" && !info.product_name) {
        info.product_name = utf16_text(v, s.value_start, s.value_end, limits.max_product_name_units);
      }
      return true;
    });
    if (!strings_ok) return Unexpected{bad};
  }
  return info;
}

// ---- The exception directory ----

Expected<std::vector<RuntimeFunction>, PeError> read_functions(const ImageInfo& image, const ByteSource& source,
                                                               std::uint32_t rva, std::uint32_t size,
                                                               const PeLimits& limits) {
  const PeError bad = at(PeProblem::BadExceptionDirectory, rva);
  if (size % kRuntimeFunctionSize != 0) return Unexpected{bad};
  const std::uint32_t count = size / kRuntimeFunctionSize;
  if (count > limits.max_functions) return Unexpected{at(PeProblem::TooManyFunctions, rva)};
  if (!image.maps(rva, size)) return Unexpected{bad};
  std::vector<RuntimeFunction> functions;
  functions.reserve(count);
  constexpr std::uint32_t kChunkEntries = 5461;  // 65,532 bytes a copy
  std::uint32_t last_end = 0;
  for (std::uint32_t first = 0; first < count; first += kChunkEntries) {
    const std::uint32_t n = std::min(kChunkEntries, count - first);
    const auto raw = fetch_rva(image, source, rva + first * kRuntimeFunctionSize, n * kRuntimeFunctionSize,
                               PeProblem::BadExceptionDirectory);
    if (!raw) return Unexpected{raw.error()};
    for (std::uint32_t i = 0; i < n; ++i) {
      const std::uint64_t e = std::uint64_t{i} * kRuntimeFunctionSize;
      RuntimeFunction fn{bytes::u32(*raw, e).value_or(0), bytes::u32(*raw, e + 4).value_or(0),
                         bytes::u32(*raw, e + 8).value_or(0)};
      // An entry of no length covers no code and is never found by Windows' unwinder, whose lookup needs begin <= pc
      // < end; GNU toolchains emit them for empty functions, and the loader never reads the table. It is left out.
      if (fn.begin == fn.end) continue;
      const std::uint32_t unwind_rva = fn.unwind & ~1u;
      if (fn.begin > fn.end || fn.end > image.size_of_image || fn.begin < last_end ||
          unwind_rva >= image.size_of_image) {
        return Unexpected{at(PeProblem::BadExceptionDirectory, rva + (first + i) * kRuntimeFunctionSize)};
      }
      last_end = fn.end;
      functions.push_back(fn);
    }
  }
  return functions;
}

// ---- The headers and the section table ----

struct Headers {
  std::uint32_t opt_at = 0;
  std::uint64_t table_at = 0;
  std::uint32_t section_count = 0;
};

// The DOS header, the NT headers and the optional header, into `image`'s fields and data directories, with only the
// checks a safe read of them needs.
Expected<Headers, PeError> read_headers(const ByteSource& source, const PeLimits& limits, ImageInfo& image) {
  // The DOS header: "MZ", and where the NT headers start.
  if (image.source_size < 64) return Unexpected{at(PeProblem::NoDosHeader, 0)};
  const auto dos = fetch(source, 0, 64);
  if (!dos) return Unexpected{dos.error()};
  if (bytes::u16(*dos, 0).value_or(0) != 0x5A4D) return Unexpected{at(PeProblem::NoDosHeader, 0)};
  const std::uint32_t nt = bytes::u32(*dos, 0x3C).value_or(0);
  if (nt < 64 || nt > kMaxHeaderOffset || !bytes::within(nt, 4 + kFileHeaderSize, image.source_size)) {
    return Unexpected{at(PeProblem::BadNtHeaderOffset, 0x3C)};
  }

  // The signature and the file header.
  const auto head = fetch(source, nt, 4 + kFileHeaderSize);
  if (!head) return Unexpected{head.error()};
  if (bytes::u32(*head, 0).value_or(0) != 0x00004550) return Unexpected{at(PeProblem::NoPeSignature, nt)};
  if (bytes::u16(*head, 4).value_or(0) != kMachineAmd64) return Unexpected{at(PeProblem::NotAmd64, nt + 4)};
  Headers h;
  h.section_count = bytes::u16(*head, 6).value_or(0);
  image.time_date_stamp = bytes::u32(*head, 8).value_or(0);
  const std::uint32_t optional_size = bytes::u16(*head, 20).value_or(0);
  image.characteristics = bytes::u16(*head, 22).value_or(0);
  if (h.section_count > limits.max_sections) return Unexpected{at(PeProblem::TooManySections, nt + 6)};

  // The optional header.
  h.opt_at = nt + 4 + kFileHeaderSize;
  if (optional_size < kOptionalFixedSize || optional_size > kMaxOptionalHeaderSize) {
    return Unexpected{at(PeProblem::BadOptionalHeader, nt + 20)};
  }
  if (!bytes::within(h.opt_at, optional_size, image.source_size)) {
    return Unexpected{at(PeProblem::Unreadable, h.opt_at)};
  }
  const auto opt = fetch(source, h.opt_at, optional_size);
  if (!opt) return Unexpected{opt.error()};
  if (bytes::u16(*opt, 0).value_or(0) != kMagicPe32Plus) return Unexpected{at(PeProblem::NotPe32Plus, h.opt_at)};
  image.entry_point = bytes::u32(*opt, 16).value_or(0);
  image.image_base = bytes::u64(*opt, 24).value_or(0);
  image.section_alignment = bytes::u32(*opt, 32).value_or(0);
  image.file_alignment = bytes::u32(*opt, 36).value_or(0);
  image.size_of_image = bytes::u32(*opt, 56).value_or(0);
  image.size_of_headers = bytes::u32(*opt, 60).value_or(0);
  image.subsystem = bytes::u16(*opt, 68).value_or(0);
  image.dll_characteristics = bytes::u16(*opt, 70).value_or(0);
  const std::uint32_t directories = bytes::u32(*opt, 108).value_or(0);
  if (directories > 16 || kOptionalFixedSize + 8 * directories > optional_size) {
    return Unexpected{at(PeProblem::BadOptionalHeader, h.opt_at + 108)};
  }
  for (std::uint32_t i = 0; i < directories && i < image.directories.size(); ++i) {
    image.directories[i] = DataDirectory{bytes::u32(*opt, kOptionalFixedSize + 8 * i).value_or(0),
                                         bytes::u32(*opt, kOptionalFixedSize + 8 * i + 4).value_or(0)};
  }
  h.table_at = std::uint64_t{h.opt_at} + optional_size;
  return h;
}

// The section table's `count` entries at `table_at`, as they stand.
Expected<std::vector<Section>, PeError> read_table(const ByteSource& source, std::uint64_t table_at,
                                                   std::uint32_t count) {
  const auto table = fetch(source, table_at, std::uint64_t{kSectionHeaderSize} * count);
  if (!table) return Unexpected{table.error()};
  std::vector<Section> sections;
  sections.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::uint64_t s = std::uint64_t{i} * kSectionHeaderSize;
    Section section;
    section.name = section_name(std::span<const std::byte>(*table).subspan(static_cast<std::size_t>(s), 8));
    section.virtual_size = bytes::u32(*table, s + 8).value_or(0);
    section.rva = bytes::u32(*table, s + 12).value_or(0);
    section.raw_size = bytes::u32(*table, s + 16).value_or(0);
    section.raw_offset = bytes::u32(*table, s + 20).value_or(0);
    section.characteristics = bytes::u32(*table, s + 36).value_or(0);
    sections.push_back(std::move(section));
  }
  return sections;
}

}  // namespace

Expected<SectionTable, PeError> read_section_table(const ByteSource& source, const PeLimits& limits) {
  ImageInfo image;
  image.source_size = source.size();
  const auto headers = read_headers(source, limits, image);
  if (!headers) return Unexpected{headers.error()};
  auto sections = read_table(source, headers->table_at, headers->section_count);
  if (!sections) return Unexpected{sections.error()};
  SectionTable table;
  table.source_size = image.source_size;
  table.size_of_image = image.size_of_image;
  table.size_of_headers = image.size_of_headers;
  table.section_alignment = image.section_alignment;
  table.file_alignment = image.file_alignment;
  table.sections = std::move(*sections);
  return table;
}

Expected<ImageInfo, PeError> read_image(const ByteSource& source, ImageLayout layout, const PeLimits& limits) {
  ImageInfo image;
  image.layout = layout;
  image.source_size = source.size();
  const auto headers = read_headers(source, limits, image);
  if (!headers) return Unexpected{headers.error()};
  const std::uint64_t table_at = headers->table_at;
  const auto directory = [&](std::size_t index) {
    return std::pair{image.directories[index].rva, image.directories[index].size};
  };

  if (!bytes::is_power_of_two(image.section_alignment) || !bytes::is_power_of_two(image.file_alignment) ||
      image.file_alignment > image.section_alignment) {
    return Unexpected{at(PeProblem::BadAlignment, headers->opt_at + 32)};
  }
  const std::uint64_t table_end = table_at + std::uint64_t{kSectionHeaderSize} * headers->section_count;
  if (image.size_of_image == 0 || image.size_of_headers > image.size_of_image ||
      image.size_of_headers > image.source_size ||
      (layout == ImageLayout::Memory && image.size_of_image > image.source_size)) {
    return Unexpected{at(PeProblem::BadSizes, headers->opt_at + 56)};
  }
  if (table_end > image.size_of_headers) return Unexpected{at(PeProblem::BadSectionTable, table_at)};

  // The section table, and the loader's rules on it.
  auto sections = read_table(source, table_at, headers->section_count);
  if (!sections) return Unexpected{sections.error()};
  image.sections = std::move(*sections);
  if (const auto checked = check_sections(image, table_at); !checked) return Unexpected{checked.error()};

  // The version resource, where there is one.
  if (const auto [rsrc, rsrc_size] = directory(kDirResource); rsrc != 0 && rsrc_size != 0) {
    // Within the image, so that no offset from its root can pass 32 bits.
    if (!bytes::within(rsrc, rsrc_size, image.size_of_image) ||
        !image.maps(rsrc, std::min<std::uint32_t>(rsrc_size, 16))) {
      return Unexpected{at(PeProblem::BadResourceDirectory, rsrc)};
    }
    const auto found = find_version(image, source, rsrc, rsrc_size, limits);
    if (!found) return Unexpected{found.error()};
    if (*found) {
      const auto& [data, language] = **found;
      auto version = parse_version(data, rsrc, language, limits);
      if (!version) return Unexpected{version.error()};
      image.version = std::move(*version);
    }
  }

  // The exception directory, where there is one.
  if (const auto [pdata, pdata_size] = directory(kDirException); pdata != 0 && pdata_size != 0) {
    auto functions = read_functions(image, source, pdata, pdata_size, limits);
    if (!functions) return Unexpected{functions.error()};
    image.functions = std::move(*functions);
  }
  return image;
}

Expected<std::vector<Fixup>, PeError> read_relocations(const ImageInfo& image, const ByteSource& source,
                                                       std::uint32_t max_fixups) {
  const DataDirectory dir = image.directories[kDirBaseRelocation];
  std::vector<Fixup> fixups;
  if (dir.rva == 0 || dir.size == 0) return fixups;
  const PeError bad = at(PeProblem::BadRelocations, dir.rva);
  const auto table = fetch_rva(image, source, dir.rva, dir.size, PeProblem::BadRelocations);
  if (!table) return Unexpected{table.error()};
  // Blocks of {page RVA, block size, entries of 16 bits: a type in the top 4 bits and an offset in the page below}.
  std::uint64_t at_block = 0;
  while (at_block + 8 <= table->size()) {
    const std::uint32_t page = bytes::u32(*table, at_block).value_or(0);
    const std::uint32_t size = bytes::u32(*table, at_block + 4).value_or(0);
    if (size < 8 || size % 2 != 0 || !bytes::within(at_block, size, table->size())) return Unexpected{bad};
    for (std::uint64_t e = at_block + 8; e + 2 <= at_block + size; e += 2) {
      const std::uint16_t entry = bytes::u16(*table, e).value_or(0);
      const unsigned type = entry >> 12;
      const std::uint32_t offset = entry & 0x0FFFu;
      if (type == 0) continue;  // IMAGE_REL_BASED_ABSOLUTE: padding
      if (type != 10 && type != 3) return Unexpected{at(PeProblem::BadRelocations, dir.rva + e)};
      const std::uint8_t width = type == 10 ? 8 : 4;
      if (!bytes::within(std::uint64_t{page} + offset, width, image.size_of_image)) {
        return Unexpected{at(PeProblem::BadRelocations, dir.rva + e)};
      }
      if (fixups.size() >= max_fixups) return Unexpected{at(PeProblem::BadRelocations, dir.rva + e)};
      fixups.push_back(Fixup{page + offset, width});
    }
    at_block += size;
  }
  std::sort(fixups.begin(), fixups.end(), [](const Fixup& a, const Fixup& b) { return a.rva < b.rva; });
  return fixups;
}

std::optional<bool> is_fragment(const ImageInfo& image, const ByteSource& source,
                                const RuntimeFunction& function) noexcept {
  if ((function.unwind & 1u) != 0) return true;  // an entry that points at another entry
  if (!image.maps(function.unwind, 4)) return std::nullopt;
  std::array<std::byte, 1> first{};
  if (!read_mapped(image, source, function.unwind, first)) return std::nullopt;
  const unsigned flags = std::to_integer<unsigned>(first[0]) >> 3;
  constexpr unsigned kChainInfo = 0x4;  // UNW_FLAG_CHAININFO
  return (flags & kChainInfo) != 0;
}

}  // namespace en::gal
