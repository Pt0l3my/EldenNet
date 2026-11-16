#pragma once
// en-gal's image read once, for the derivation kit's many small reads by RVA (ARCH §6.2; O89): every section's bytes as
// the loader would map them (the raw data, then zeros to the section's virtual size), copied through a ByteSource:
// offline the executable file, in game the mapped image's guarded view. Reads answer nothing past a section's end or
// across two sections; nothing points into the source, and nothing here knows the game.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/pe.hpp"

namespace en::gal {

struct SectionCopy {
  std::size_t index = 0;  // into ImageInfo::sections
  std::uint32_t rva = 0;
  std::vector<std::byte> bytes;  // mapped_size() bytes
  std::uint64_t unreadable = 0;  // bytes the source could not give, left as zeros
};

class ImageBytes {
 public:
  // Copies each section of at most `max_section` bytes; a larger one is left out (and counted).
  ImageBytes(const ImageInfo& image, const ByteSource& source, std::uint64_t max_section = 1u << 30);

  [[nodiscard]] const ImageInfo& image() const noexcept { return image_; }
  [[nodiscard]] const std::vector<SectionCopy>& sections() const noexcept { return copies_; }
  [[nodiscard]] std::uint64_t left_out() const noexcept { return left_out_; }

  // The copy of the first section of that name, or of the section holding `rva`; null when there is none.
  [[nodiscard]] const SectionCopy* named(std::string_view name) const noexcept;
  [[nodiscard]] const SectionCopy* at(std::uint32_t rva) const noexcept;
  // The section's header for a copy.
  [[nodiscard]] const Section& header(const SectionCopy& copy) const noexcept;

  // `len` bytes at `rva` within one section, or an empty span.
  [[nodiscard]] std::span<const std::byte> view(std::uint32_t rva, std::uint32_t len) const noexcept;
  // From `rva` to the end of its section, at most `max` bytes.
  [[nodiscard]] std::span<const std::byte> tail(std::uint32_t rva, std::uint32_t max) const noexcept;
  [[nodiscard]] std::optional<std::uint8_t> u8(std::uint32_t rva) const noexcept;
  [[nodiscard]] std::optional<std::uint32_t> u32(std::uint32_t rva) const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> u64(std::uint32_t rva) const noexcept;

  // The RVA a virtual address names, when it lies within the image at its preferred base.
  [[nodiscard]] std::optional<std::uint32_t> rva_of(std::uint64_t va) const noexcept;
  // Whether `rva` lies in a section with these characteristic bits set (kSectionExecute, kSectionWrite, ...).
  [[nodiscard]] bool in_section_with(std::uint32_t rva, std::uint32_t bits) const noexcept;
  // The name of the section holding `rva`, or empty.
  [[nodiscard]] std::string_view section_name(std::uint32_t rva) const noexcept;

  // A NUL-terminated string at `rva`, within its section: printable ASCII of 1 to `max` bytes; or UTF-16LE whose units
  // are all printable ASCII, 1 to `max` of them, returned narrowed. Nothing otherwise.
  [[nodiscard]] std::optional<std::string> ascii(std::uint32_t rva, std::size_t max) const;
  [[nodiscard]] std::optional<std::string> utf16(std::uint32_t rva, std::size_t max) const;

 private:
  const ImageInfo& image_;
  std::vector<SectionCopy> copies_;  // in the image's section order, which is RVA order
  std::uint64_t left_out_ = 0;
};

// Whether `s` is an identifier: [A-Za-z_][A-Za-z0-9_]*.
[[nodiscard]] bool is_identifier_text(std::string_view s) noexcept;

}  // namespace en::gal
