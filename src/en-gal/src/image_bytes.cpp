// en-gal's image read once (image_bytes.hpp).
#include "en/gal/image_bytes.hpp"

#include <algorithm>

#include "bytes.hpp"

namespace en::gal {

namespace {

constexpr std::uint64_t kChunk = 1u << 20;
constexpr std::uint64_t kPage = 4096;

// Copies [offset, offset + dst.size()) of the source into dst, a page at a time where a whole chunk cannot be read;
// answers how many bytes could not be read (left as zeros).
std::uint64_t copy_from(const ByteSource& source, std::uint64_t offset, std::span<std::byte> dst) {
  std::uint64_t unreadable = 0;
  for (std::uint64_t pos = 0; pos < dst.size(); pos += kChunk) {
    const std::uint64_t len = std::min<std::uint64_t>(kChunk, dst.size() - pos);
    const std::span<std::byte> chunk = dst.subspan(static_cast<std::size_t>(pos), static_cast<std::size_t>(len));
    if (source.read(offset + pos, chunk)) continue;
    for (std::uint64_t page = 0; page < len;) {
      const std::uint64_t here = offset + pos + page;
      const std::uint64_t n = std::min<std::uint64_t>(kPage - here % kPage, len - page);
      const std::span<std::byte> part = chunk.subspan(static_cast<std::size_t>(page), static_cast<std::size_t>(n));
      if (!source.read(here, part)) {
        std::fill(part.begin(), part.end(), std::byte{0});
        unreadable += n;
      }
      page += n;
    }
  }
  return unreadable;
}

bool printable(std::uint32_t c) noexcept { return c >= 0x20 && c < 0x7F; }

}  // namespace

bool is_identifier_text(std::string_view s) noexcept {
  if (s.empty()) return false;
  const auto letter = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; };
  if (!letter(s.front())) return false;
  return std::all_of(s.begin(), s.end(), [&](char c) { return letter(c) || (c >= '0' && c <= '9'); });
}

ImageBytes::ImageBytes(const ImageInfo& image, const ByteSource& source, std::uint64_t max_section) : image_(image) {
  for (std::size_t i = 0; i < image.sections.size(); ++i) {
    const Section& s = image.sections[i];
    const std::uint64_t size = s.mapped_size();
    if (size > max_section) {
      ++left_out_;
      continue;
    }
    SectionCopy copy;
    copy.index = i;
    copy.rva = s.rva;
    copy.bytes.assign(static_cast<std::size_t>(size), std::byte{0});
    if (image.layout == ImageLayout::Memory) {
      copy.unreadable = copy_from(source, s.rva, copy.bytes);
    } else {
      // The raw data the file holds (file_span), which may be shorter than the mapped size or end before its raw size
      // (the rest is zeros once mapped), or longer (padding, not mapped).
      const FileSpan held = image.file_span(s);
      if (held.size != 0) {
        copy.unreadable =
            copy_from(source, held.offset, std::span<std::byte>(copy.bytes).first(static_cast<std::size_t>(held.size)));
      }
    }
    copies_.push_back(std::move(copy));
  }
}

const SectionCopy* ImageBytes::named(std::string_view name) const noexcept {
  for (const SectionCopy& c : copies_) {
    if (c.index < image_.sections.size() && image_.sections[c.index].name == name) return &c;
  }
  return nullptr;
}

const SectionCopy* ImageBytes::at(std::uint32_t rva) const noexcept {
  // Sections are in RVA order: the last whose start is at or below rva.
  const auto it = std::upper_bound(copies_.begin(), copies_.end(), rva,
                                   [](std::uint32_t r, const SectionCopy& c) { return r < c.rva; });
  if (it == copies_.begin()) return nullptr;
  const SectionCopy& c = *(it - 1);
  if (rva - c.rva >= c.bytes.size()) return nullptr;
  return &c;
}

const Section& ImageBytes::header(const SectionCopy& copy) const noexcept { return image_.sections[copy.index]; }

std::span<const std::byte> ImageBytes::view(std::uint32_t rva, std::uint32_t len) const noexcept {
  const SectionCopy* c = at(rva);
  if (c == nullptr) return {};
  const std::uint64_t off = rva - c->rva;
  if (!bytes::within(off, len, c->bytes.size())) return {};
  return std::span<const std::byte>(c->bytes).subspan(static_cast<std::size_t>(off), len);
}

std::span<const std::byte> ImageBytes::tail(std::uint32_t rva, std::uint32_t max) const noexcept {
  const SectionCopy* c = at(rva);
  if (c == nullptr) return {};
  const std::uint64_t off = rva - c->rva;
  const std::uint64_t n = std::min<std::uint64_t>(max, c->bytes.size() - off);
  return std::span<const std::byte>(c->bytes).subspan(static_cast<std::size_t>(off), static_cast<std::size_t>(n));
}

std::optional<std::uint8_t> ImageBytes::u8(std::uint32_t rva) const noexcept { return bytes::u8(view(rva, 1), 0); }
std::optional<std::uint32_t> ImageBytes::u32(std::uint32_t rva) const noexcept { return bytes::u32(view(rva, 4), 0); }
std::optional<std::uint64_t> ImageBytes::u64(std::uint32_t rva) const noexcept { return bytes::u64(view(rva, 8), 0); }

std::optional<std::uint32_t> ImageBytes::rva_of(std::uint64_t va) const noexcept {
  if (va < image_.image_base || va - image_.image_base >= image_.size_of_image) return std::nullopt;
  return static_cast<std::uint32_t>(va - image_.image_base);
}

bool ImageBytes::in_section_with(std::uint32_t rva, std::uint32_t bits) const noexcept {
  const SectionCopy* c = at(rva);
  return c != nullptr && (header(*c).characteristics & bits) == bits;
}

std::string_view ImageBytes::section_name(std::uint32_t rva) const noexcept {
  const SectionCopy* c = at(rva);
  return c == nullptr ? std::string_view() : std::string_view(header(*c).name);
}

std::optional<std::string> ImageBytes::ascii(std::uint32_t rva, std::size_t max) const {
  const std::span<const std::byte> t = tail(rva, static_cast<std::uint32_t>(std::min<std::size_t>(max + 1, 1u << 16)));
  std::string out;
  for (const std::byte b : t) {
    const auto c = std::to_integer<std::uint8_t>(b);
    if (c == 0) return out.empty() ? std::nullopt : std::optional<std::string>(out);
    if (!printable(c) || out.size() >= max) return std::nullopt;
    out += static_cast<char>(c);
  }
  return std::nullopt;
}

std::optional<std::string> ImageBytes::utf16(std::uint32_t rva, std::size_t max) const {
  if (rva % 2 != 0) return std::nullopt;
  const std::span<const std::byte> t =
      tail(rva, static_cast<std::uint32_t>(std::min<std::size_t>(2 * (max + 1), 1u << 17)));
  std::string out;
  for (std::size_t i = 0; i + 1 < t.size(); i += 2) {
    const std::uint32_t c = std::to_integer<std::uint32_t>(t[i]) | (std::to_integer<std::uint32_t>(t[i + 1]) << 8);
    if (c == 0) return out.empty() ? std::nullopt : std::optional<std::string>(out);
    if (!printable(c) || out.size() >= max) return std::nullopt;
    out += static_cast<char>(c);
  }
  return std::nullopt;
}

}  // namespace en::gal
