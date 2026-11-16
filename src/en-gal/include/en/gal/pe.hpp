#pragma once
// en-gal's PE32+ reader (ARCH §6.1's executable image, §6.2): the headers, the section table, the version resource
// (VS_FIXEDFILEINFO's versions, the ProductName string and the first translation) and the exception directory's
// function table (.pdata), from an image as a file on disk or as the loader mapped it. It reads through a ByteSource
// and copies what it needs, so it never points into the image: in game the source is sys's image view, whose reads
// are guarded copies; offline it is the file's bytes. Everything read is checked against the image's own bounds, with
// checked arithmetic, and a malformed image is refused with the problem and where it was found. Nothing here knows
// the game: it reads any PE32+ image for x64.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/expected.hpp"

namespace en::gal {

// Where the bytes of an image come from. read() copies dst.size() bytes from `offset`, and answers false, leaving dst
// unspecified, when any of them cannot be read: past size(), or (in game) a page the guarded copy could not read.
class ByteSource {
 public:
  virtual ~ByteSource() = default;
  [[nodiscard]] virtual std::uint64_t size() const noexcept = 0;
  [[nodiscard]] virtual bool read(std::uint64_t offset, std::span<std::byte> dst) const noexcept = 0;

 protected:
  ByteSource() = default;
  ByteSource(const ByteSource&) = default;
  ByteSource& operator=(const ByteSource&) = default;
};

// Bytes the caller holds: a file read into memory, or a test's invented image.
class SpanSource final : public ByteSource {
 public:
  explicit SpanSource(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}
  [[nodiscard]] std::uint64_t size() const noexcept override { return bytes_.size(); }
  [[nodiscard]] bool read(std::uint64_t offset, std::span<std::byte> dst) const noexcept override;

 private:
  std::span<const std::byte> bytes_;
};

// The file on disk (sections at their raw offsets), or the image the loader mapped (sections at their RVAs).
enum class ImageLayout : std::uint8_t { File, Memory };

inline constexpr std::uint32_t kSectionCode = 0x00000020;     // IMAGE_SCN_CNT_CODE
inline constexpr std::uint32_t kSectionExecute = 0x20000000;  // IMAGE_SCN_MEM_EXECUTE
inline constexpr std::uint32_t kSectionRead = 0x40000000;     // IMAGE_SCN_MEM_READ
inline constexpr std::uint32_t kSectionWrite = 0x80000000;    // IMAGE_SCN_MEM_WRITE

struct Section {
  std::string name;  // the 8-byte name up to its first NUL
  std::uint32_t rva = 0;
  std::uint32_t virtual_size = 0;
  std::uint32_t raw_offset = 0;
  std::uint32_t raw_size = 0;
  std::uint32_t characteristics = 0;

  // The bytes it spans once mapped: its virtual size, or its raw size where the virtual size is 0.
  [[nodiscard]] std::uint32_t mapped_size() const noexcept { return virtual_size != 0 ? virtual_size : raw_size; }
  [[nodiscard]] bool contains(std::uint32_t rva_) const noexcept { return rva_ >= rva && rva_ - rva < mapped_size(); }

  friend bool operator==(const Section&, const Section&) = default;
};

// One entry of the exception directory: a function's code [begin, end) and its unwind information.
struct RuntimeFunction {
  std::uint32_t begin = 0;
  std::uint32_t end = 0;
  std::uint32_t unwind = 0;  // the UNWIND_INFO's RVA; bit 0 set marks an entry that points at another entry instead

  [[nodiscard]] std::uint32_t length() const noexcept { return end - begin; }
  friend constexpr bool operator==(const RuntimeFunction&, const RuntimeFunction&) = default;
};

struct Translation {
  std::uint16_t language = 0;
  std::uint16_t codepage = 0;
  friend constexpr bool operator==(const Translation&, const Translation&) = default;
};

// The version resource (RT_VERSION): its fixed information, ProductName and first translation.
struct VersionInfo {
  std::array<std::uint16_t, 4> file_version{};     // VS_FIXEDFILEINFO's FileVersion, most significant first
  std::array<std::uint16_t, 4> product_version{};  // its ProductVersion
  std::optional<std::string> product_name;         // UTF-8; empty when absent, or not valid UTF-16
  std::optional<Translation> translation;          // VarFileInfo's first
  std::uint16_t resource_language = 0;             // the language of the resource entry read
};

// Where a section's raw data lies in the file, as the loader reads it: SizeOfRawData bytes at PointerToRawData (none
// when either is 0), at most the section's mapped size, and only what lies before the end of the file. Raw ranges may
// be in any order, overlap, be shared or run past the end of the file; what the file does not hold maps as zeros.
struct FileSpan {
  std::uint64_t offset = 0;
  std::uint32_t size = 0;
  friend constexpr bool operator==(const FileSpan&, const FileSpan&) = default;
};

// One of the optional header's data directories.
struct DataDirectory {
  std::uint32_t rva = 0;
  std::uint32_t size = 0;
  friend constexpr bool operator==(const DataDirectory&, const DataDirectory&) = default;
};

inline constexpr std::size_t kDirBaseRelocation = 5;  // the index of the base relocation table

// What read_image found. The image itself is not kept: offset_of() says where an RVA's bytes lie in the source.
struct ImageInfo {
  ImageLayout layout = ImageLayout::File;
  std::uint64_t source_size = 0;
  std::uint64_t image_base = 0;
  std::uint32_t entry_point = 0;
  std::uint32_t size_of_image = 0;
  std::uint32_t size_of_headers = 0;
  std::uint32_t section_alignment = 0;
  std::uint32_t file_alignment = 0;
  std::uint32_t time_date_stamp = 0;
  std::uint16_t characteristics = 0;
  std::uint16_t dll_characteristics = 0;
  std::uint16_t subsystem = 0;
  std::vector<Section> sections;                // in RVA order (ascending, disjoint virtual ranges)
  std::optional<VersionInfo> version;           // absent when the image has no version resource
  std::vector<RuntimeFunction> functions;       // the exception directory, sorted and disjoint, without the entries of
                                                // no length; empty when it has none
  std::array<DataDirectory, 16> directories{};  // the data directories as the header gives them; zero past its count

  // The first section of that name, or null.
  [[nodiscard]] const Section* section(std::string_view name) const noexcept;
  // The section whose mapped bytes hold `rva`, or null.
  [[nodiscard]] const Section* section_at(std::uint32_t rva) const noexcept;
  // The bytes of a section the file holds (FileSpan); in Memory layout, nothing (size 0).
  [[nodiscard]] FileSpan file_span(const Section& section) const noexcept;
  // Whether [rva, rva + len) lies within the headers or one section's mapped bytes.
  [[nodiscard]] bool maps(std::uint32_t rva, std::uint32_t len) const noexcept;
  // Where [rva, rva + len) lies in the source, when it lies within the headers or one section and, in a file, within
  // the bytes the file holds of that section (file_span); else nothing. read_mapped() also reads what maps as zeros.
  [[nodiscard]] std::optional<std::uint64_t> offset_of(std::uint32_t rva, std::uint32_t len) const noexcept;
  // The exception directory's entry whose [begin, end) holds `rva`, or null.
  [[nodiscard]] const RuntimeFunction* function_at(std::uint32_t rva) const noexcept;
};

enum class PeProblem : std::uint8_t {
  Unreadable,             // the source could not give bytes the headers say are there
  NoDosHeader,            // no "MZ"
  BadNtHeaderOffset,      // e_lfanew outside the first 64 KiB, or the headers it points at past the end
  NoPeSignature,          // no "PE\0\0"
  NotAmd64,               // a machine other than x64
  NotPe32Plus,            // an optional header other than PE32+'s
  BadOptionalHeader,      // too small for its fields, or more than 16 data directories
  BadAlignment,           // section or file alignment not a power of two, or the file's above the section's
  BadSizes,               // SizeOfHeaders or SizeOfImage that cannot be, against each other or the source
  TooManySections,        // more than the limit (96, Windows' own)
  BadSectionTable,        // the section table past the headers
  BadSection,             // a section the loader would refuse: SectionFault says which, by which rule
  BadResourceDirectory,   // a resource directory, entry or data entry out of its bounds, or nested otherwise
  BadVersionResource,     // VS_VERSIONINFO malformed, or without its fixed information
  BadExceptionDirectory,  // a function table whose size is not whole entries, out of a section, unsorted or overlapping
                          // (an entry of no length is left out, not refused)
  TooManyFunctions,       // more function-table entries than the limit
  BadRelocations,         // a base relocation block out of its bounds, or an entry of a type x64 does not use
};

// The rules on sections, which are the loader's own: virtual ranges ascending, disjoint and aligned within the image,
// and raw ranges that do not wrap. Nothing else about raw ranges is a rule: they may be in any order, overlap, be
// shared, be empty or run past the end of the file.
enum class SectionRule : std::uint8_t {
  Unaligned,   // its virtual address is not a multiple of SectionAlignment (the bound)
  Overlaps,    // it starts below the end of the headers or of the section before it (the bound: that end)
  PastImage,   // its virtual range, aligned to SectionAlignment, ends past the image's end (the bound: SizeOfImage
               // rounded up to SectionAlignment, and to 4 KiB, as the loader rounds it)
  RawWraps,    // its raw data's end, PointerToRawData + SizeOfRawData, passes 4 GiB
};

// Which section broke which rule, with the section table's numbers for it (never its name).
struct SectionFault {
  SectionRule rule = SectionRule::Overlaps;
  std::uint32_t index = 0;  // in the section table
  std::uint64_t bound = 0;  // the number the rule compared against; 0 for RawWraps
  std::uint32_t rva = 0;
  std::uint32_t virtual_size = 0;
  std::uint32_t raw_offset = 0;
  std::uint32_t raw_size = 0;
  std::uint32_t characteristics = 0;

  friend constexpr bool operator==(const SectionFault&, const SectionFault&) = default;
};

struct PeError {
  PeProblem problem = PeProblem::Unreadable;
  std::uint64_t at = 0;                 // the offset or RVA where it was found
  std::optional<SectionFault> section;  // for BadSection

  constexpr PeError() noexcept = default;
  constexpr PeError(PeProblem problem_, std::uint64_t at_, std::optional<SectionFault> section_ = {}) noexcept
      : problem(problem_), at(at_), section(section_) {}

  friend constexpr bool operator==(const PeError&, const PeError&) = default;
};

// "the section table runs past the headers (at 0x1f8)"; a section's refusal also names the section by index, the rule
// and its numbers, in the words `--sections` uses: "a section is misplaced: section 3 starts below the end of section
// 2, 0x5000; va 0x4000 vsize 0x1200 raw 0x3400 rawsize 0x1200 flags 0x40000040 (at 0x1f8)". Numbers and fixed words
// only, at most 192 characters.
[[nodiscard]] std::string describe(const PeError& error);
[[nodiscard]] const char* to_string(PeProblem problem) noexcept;
// A section's numbers as describe() and `probe-exe --sections` give them: "va 0x4000 vsize 0x1200 raw 0x3400
// rawsize 0x1200 flags 0x40000040".
[[nodiscard]] std::string describe_numbers(std::uint32_t rva, std::uint32_t virtual_size, std::uint32_t raw_offset,
                                           std::uint32_t raw_size, std::uint32_t characteristics);

struct PeLimits {
  std::uint32_t max_sections = 96;              // Windows' loader refuses more
  std::uint32_t max_functions = 1u << 22;       // 48 MiB of table
  std::uint32_t max_version_bytes = 64u << 10;  // a version resource is a few KiB
  std::uint32_t max_resource_entries = 4096;    // per resource directory
  std::uint32_t max_product_name_units = 256;   // UTF-16 units read of ProductName
};

// Reads an image's headers, sections, version resource and function table. In File layout the source is the file; in
// Memory layout it is the mapped image, at least SizeOfImage long. Never throws for what the image holds.
[[nodiscard]] proto::Expected<ImageInfo, PeError> read_image(const ByteSource& source, ImageLayout layout,
                                                             const PeLimits& limits = {});

// The headers' sizes and the section table as the table gives them, read with only the checks a safe read needs (the
// DOS and NT headers, x64, PE32+, the optional header's size, the number of sections, the table within the source) and
// none of the rules on sizes or sections: for a diagnostic of an image read_image refuses (`probe-exe --sections`).
struct SectionTable {
  std::uint64_t source_size = 0;
  std::uint32_t size_of_image = 0;
  std::uint32_t size_of_headers = 0;
  std::uint32_t section_alignment = 0;
  std::uint32_t file_alignment = 0;
  std::vector<Section> sections;  // in the table's order
};
[[nodiscard]] proto::Expected<SectionTable, PeError> read_section_table(const ByteSource& source,
                                                                        const PeLimits& limits = {});

// Copies [rva, rva + dst.size()) of the image as the loader maps it, when it lies within the headers or one section's
// mapped bytes: in a file, the bytes the file holds of that section (ImageInfo::file_span) and zeros for the rest of
// it. False when the range lies elsewhere, or the source cannot give bytes it should hold.
[[nodiscard]] bool read_mapped(const ImageInfo& image, const ByteSource& source, std::uint32_t rva,
                               std::span<std::byte> dst) noexcept;

// One place the loader adjusts when it maps the image away from its preferred base: `size` bytes at `rva` (8 for
// IMAGE_REL_BASED_DIR64, 4 for HIGHLOW).
struct Fixup {
  std::uint32_t rva = 0;
  std::uint8_t size = 0;
  friend constexpr bool operator==(const Fixup&, const Fixup&) = default;
};

// The base relocation table's fixups, sorted by RVA; empty when the image has none. A block out of its bounds, or an
// entry of a type x64 images do not use, is refused as BadRelocations.
[[nodiscard]] proto::Expected<std::vector<Fixup>, PeError> read_relocations(const ImageInfo& image,
                                                                            const ByteSource& source,
                                                                            std::uint32_t max_fixups = 1u << 24);

// Whether the entry's code is a fragment of a larger function: an entry pointing at another one, or unwind information
// chained to another entry's (UNW_FLAG_CHAININFO). Nothing when its unwind information cannot be read.
[[nodiscard]] std::optional<bool> is_fragment(const ImageInfo& image, const ByteSource& source,
                                              const RuntimeFunction& function) noexcept;

}  // namespace en::gal
