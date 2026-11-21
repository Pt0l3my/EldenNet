// The PE32+ reader (pe.hpp) on FakeImages of invented bytes: the same facts from the file and from the mapped image;
// the version resource, the sections and the function table; RVAs placed in the source; fragments; every way a
// malformed image is refused, with where, and for a section which rule, which section and its numbers; and the raw
// layouts the loader accepts (raw data out of order, overlapping, shared, empty or past the end of the file; sections
// of no size; virtual sizes far past the raw data; SizeOfImage short of the last section's alignment), each read as the
// loader maps it.
#include <doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "en/gal/image_bytes.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/pe.hpp"
#include "fake_image.hpp"

using namespace en::gal;
using namespace en::gal::test;

namespace {

FakeImageSpec sample_spec() {
  FakeImageSpec spec;
  std::vector<std::uint8_t> code(0x2400, 0xCC);
  for (std::size_t i = 0; i < code.size(); ++i) code[i] = static_cast<std::uint8_t>((i * 37 + 11) & 0xFF);
  spec.sections.push_back(FakeSection{".text", kCode, code, 0});
  spec.sections.push_back(FakeSection{".rdata", kReadOnly, std::vector<std::uint8_t>(0x300, 0x11), 0});
  spec.sections.push_back(FakeSection{".data", kReadWrite, std::vector<std::uint8_t>(0x200, 0x22), 0x1800});
  FakeVersion v;
  v.file_version = {3, 14, 15, 1};
  v.product_version = {3, 14, 0, 0};
  v.product_name = u"Invénted Ring ★";  // é and a star: two and three UTF-8 bytes
  spec.version = v;
  spec.functions = {{0x10, 0x80, false, false},    {0x80, 0x400, false, false},   {0x400, 0x500, true, false},
                    {0x600, 0x1000, false, false}, {0x1000, 0x1010, false, true}, {0x1100, 0x2000, false, false}};
  return spec;
}

ImageInfo read(const std::vector<std::byte>& bytes, ImageLayout layout) {
  const SpanSource source(bytes);
  auto image = read_image(source, layout);
  REQUIRE(image.has_value());
  return std::move(*image);
}

PeError refusal(const std::vector<std::byte>& bytes, ImageLayout layout = ImageLayout::File, PeLimits limits = {}) {
  const SpanSource source(bytes);
  const auto image = read_image(source, layout, limits);
  REQUIRE_FALSE(image.has_value());
  return image.error();
}

}  // namespace

TEST_CASE("an image reads the same from the file and from memory") {
  const BuiltImage built = build(sample_spec());
  const ImageInfo file = read(built.file, ImageLayout::File);
  const ImageInfo memory = read(built.memory, ImageLayout::Memory);
  CHECK(file.layout == ImageLayout::File);
  CHECK(memory.layout == ImageLayout::Memory);
  for (const ImageInfo* image : {&file, &memory}) {
    CHECK(image->image_base == 0x7FF7'0000'0000ULL);
    CHECK(image->time_date_stamp == 0x1234'5678u);
    CHECK(image->size_of_image == built.size_of_image);
    CHECK(image->size_of_headers == kHeadersSize);
    CHECK(image->section_alignment == kSectionAlign);
    CHECK(image->file_alignment == kFileAlign);
    CHECK(image->entry_point == built.rva.at(".text"));
    REQUIRE(image->sections.size() == 6);  // .text .rdata .data .rsrc .xdata .pdata
    CHECK(image->sections[0].name == ".text");
    CHECK(image->sections[2].mapped_size() == 0x1800);
    REQUIRE(image->version.has_value());
    CHECK(image->version->file_version == std::array<std::uint16_t, 4>{3, 14, 15, 1});
    CHECK(image->version->product_version == std::array<std::uint16_t, 4>{3, 14, 0, 0});
    REQUIRE(image->version->product_name.has_value());
    CHECK(*image->version->product_name == "Inv\xC3\xA9nted Ring \xE2\x98\x85");
    CHECK(image->version->translation == Translation{0x0409, 0x04B0});
    CHECK(image->version->resource_language == 0x0409);
    CHECK(image->functions.size() == 6);
  }
  CHECK(file.sections == memory.sections);
  CHECK(file.functions == memory.functions);
}

TEST_CASE("sections are found by name and by RVA, and an RVA's bytes are placed in either layout") {
  const BuiltImage built = build(sample_spec());
  const ImageInfo file = read(built.file, ImageLayout::File);
  const ImageInfo memory = read(built.memory, ImageLayout::Memory);
  const std::uint32_t text = built.rva.at(".text");
  REQUIRE(file.section(".data") != nullptr);
  CHECK(file.section(".nope") == nullptr);
  CHECK(file.section_at(text + 5) == file.section(".text"));
  CHECK(file.section_at(0x10) == nullptr);  // the headers belong to no section
  CHECK(memory.offset_of(text + 5, 4) == text + 5);
  CHECK(file.offset_of(text + 5, 4) == kHeadersSize + 5);  // .text's raw bytes start right after the headers
  CHECK(file.offset_of(0x3C, 4) == 0x3C);                  // the headers lie at the same place in both
  const std::uint32_t data = built.rva.at(".data");
  CHECK(memory.offset_of(data + 0x1000, 8).has_value());  // mapped, past the bytes the file holds
  CHECK_FALSE(file.offset_of(data + 0x1000, 8).has_value());
  CHECK_FALSE(memory.offset_of(data + 0x17FE, 4).has_value());  // runs past the section
}

TEST_CASE("the function table maps an RVA to the function that holds it") {
  const BuiltImage built = build(sample_spec());
  const ImageInfo image = read(built.memory, ImageLayout::Memory);
  const std::uint32_t text = built.rva.at(".text");
  REQUIRE(image.function_at(text + 0x80) != nullptr);
  CHECK(image.function_at(text + 0x80)->begin == text + 0x80);  // its first byte
  CHECK(image.function_at(text + 0x7F)->begin == text + 0x10);  // the last byte of the one before
  CHECK(image.function_at(text + 0x500) == nullptr);            // between two functions
  CHECK(image.function_at(text + 0x5) == nullptr);              // before the first
  CHECK(image.function_at(text + 0x2000) == nullptr);           // past the last
  CHECK(image.function_at(text + 0x1234)->length() == 0xF00);
}

TEST_CASE("a fragment is told by chained unwind information or by an entry that points at another") {
  const BuiltImage built = build(sample_spec());
  const SpanSource source(built.memory);
  const ImageInfo image = read(built.memory, ImageLayout::Memory);
  const std::uint32_t text = built.rva.at(".text");
  CHECK(is_fragment(image, source, *image.function_at(text + 0x100)) == false);
  CHECK(is_fragment(image, source, *image.function_at(text + 0x450)) == true);   // chained
  CHECK(is_fragment(image, source, *image.function_at(text + 0x1008)) == true);  // indirect
  const ImageInfo file = read(built.file, ImageLayout::File);
  const SpanSource file_source(built.file);
  CHECK(is_fragment(file, file_source, *file.function_at(text + 0x450)) == true);
}

TEST_CASE("an image without a version resource or a function table reads, without them") {
  FakeImageSpec spec = sample_spec();
  spec.version.reset();
  spec.functions.clear();
  const BuiltImage built = build(spec);
  const ImageInfo image = read(built.file, ImageLayout::File);
  CHECK_FALSE(image.version.has_value());
  CHECK(image.functions.empty());
  CHECK(image.sections.size() == 3);
}

TEST_CASE("a version resource without ProductName or a translation reads, without them") {
  FakeImageSpec spec = sample_spec();
  spec.version->product_name.reset();
  spec.version->translation.reset();
  const ImageInfo image = read(build(spec).file, ImageLayout::File);
  REQUIRE(image.version.has_value());
  CHECK_FALSE(image.version->product_name.has_value());
  CHECK_FALSE(image.version->translation.has_value());
  CHECK(image.version->file_version[1] == 14);
}

TEST_CASE("ProductName comes from the translation's string table, and invalid UTF-16 gives none") {
  FakeImageSpec spec = sample_spec();
  spec.version->table_key = "040904B0";  // what the translation names, in upper case
  CHECK(read(build(spec).file, ImageLayout::File).version->product_name.has_value());
  spec.version->product_name = std::u16string(u"Bad ") + char16_t{0xD800} + u"x";  // a lone high surrogate
  const ImageInfo image = read(build(spec).file, ImageLayout::File);
  REQUIRE(image.version.has_value());
  CHECK_FALSE(image.version->product_name.has_value());
}

TEST_CASE("a malformed image is refused with the problem and where it was found") {
  const BuiltImage built = build(sample_spec());
  const auto broken = [&](auto&& change) {
    std::vector<std::byte> bytes = built.file;
    change(bytes);
    return bytes;
  };
  CHECK(refusal(std::vector<std::byte>(40)).problem == PeProblem::NoDosHeader);
  CHECK(refusal(broken([](auto& b) { put_u16(b, 0, 0x4D5A); })) == PeError{PeProblem::NoDosHeader, 0});
  CHECK(refusal(broken([](auto& b) { put_u32(b, 0x3C, 0x20); })) == PeError{PeProblem::BadNtHeaderOffset, 0x3C});
  CHECK(refusal(broken([](auto& b) { put_u32(b, 0x3C, 0x20000); })).problem == PeProblem::BadNtHeaderOffset);
  CHECK(refusal(broken([](auto& b) { put_u32(b, kNtOffset, 0x00004551); })) ==
        PeError{PeProblem::NoPeSignature, kNtOffset});
  CHECK(refusal(broken([](auto& b) { put_u16(b, kFileHeaderAt, 0x014C); })).problem == PeProblem::NotAmd64);
  CHECK(refusal(broken([](auto& b) { put_u16(b, kOptionalAt, 0x010B); })).problem == PeProblem::NotPe32Plus);
  CHECK(refusal(broken([](auto& b) { put_u16(b, kFileHeaderAt + 16, 100); })).problem == PeProblem::BadOptionalHeader);
  CHECK(refusal(broken([](auto& b) { put_u32(b, kOptionalAt + 108, 17); })).problem == PeProblem::BadOptionalHeader);
  CHECK(refusal(broken([](auto& b) { put_u32(b, kOptionalAt + 32, 0x1001); })).problem == PeProblem::BadAlignment);
  CHECK(refusal(broken([](auto& b) { put_u32(b, kOptionalAt + 36, 0x2000); })).problem == PeProblem::BadAlignment);
  CHECK(refusal(broken([](auto& b) { put_u32(b, kOptionalAt + 56, 0); })).problem == PeProblem::BadSizes);
  CHECK(refusal(broken([](auto& b) { put_u32(b, kOptionalAt + 60, 0x100); })).problem == PeProblem::BadSectionTable);
  CHECK(refusal(broken([](auto& b) { put_u16(b, kFileHeaderAt + 2, 97); })).problem == PeProblem::TooManySections);
  // Sections: each of the loader's rules, with the section's index and numbers. .text is 0x2400 bytes at 0x1000, its
  // raw data at 0x400; .rdata 0x300 bytes at 0x4000, its raw data at 0x2800.
  const auto fault = [](SectionRule rule, std::uint32_t index, std::uint64_t bound, std::uint32_t rva,
                        std::uint32_t virtual_size, std::uint32_t raw_offset, std::uint32_t raw_size,
                        std::uint32_t characteristics) {
    SectionFault f;
    f.rule = rule;
    f.index = index;
    f.bound = bound;
    f.rva = rva;
    f.virtual_size = virtual_size;
    f.raw_offset = raw_offset;
    f.raw_size = raw_size;
    f.characteristics = characteristics;
    return PeError{PeProblem::BadSection, kSectionTableAt + 40u * index, f};
  };
  CHECK(refusal(broken([](auto& b) { put_u32(b, kSectionTableAt + 40 + 12, 0x1000); })) ==
        fault(SectionRule::Overlaps, 1, 0x4000, 0x1000, 0x300, 0x2800, 0x400, kReadOnly));
  CHECK(refusal(broken([](auto& b) { put_u32(b, kSectionTableAt + 12, 0); })) ==
        fault(SectionRule::Overlaps, 0, kHeadersSize, 0, 0x2400, 0x400, 0x2400, kCode));
  CHECK(refusal(broken([](auto& b) { put_u32(b, kSectionTableAt + 40 + 12, 0x4010); })) ==
        fault(SectionRule::Unaligned, 1, kSectionAlign, 0x4010, 0x300, 0x2800, 0x400, kReadOnly));
  CHECK(refusal(broken([&](auto& b) { put_u32(b, kSectionTableAt + 8, built.size_of_image); })) ==
        fault(SectionRule::PastImage, 0, built.size_of_image, 0x1000, built.size_of_image, 0x400, 0x2400, kCode));
  CHECK(refusal(broken([](auto& b) { put_u32(b, kSectionTableAt + 20, 0xFFFFF000u); })) ==
        fault(SectionRule::RawWraps, 0, 0, 0x1000, 0x2400, 0xFFFFF000u, 0x2400, kCode));
  // What the loader lets through: raw data far past the end of the file maps as zeros.
  const std::vector<std::byte> far = broken([](auto& b) { put_u32(b, kSectionTableAt + 40 + 20, 0x7FFF0000); });
  const ImageInfo far_image = read(far, ImageLayout::File);
  CHECK(far_image.file_span(far_image.sections[1]) == FileSpan{});
  std::vector<std::byte> rdata(0x300, std::byte{0x5A});
  CHECK(read_mapped(far_image, SpanSource(far), 0x4000, rdata));
  CHECK(std::all_of(rdata.begin(), rdata.end(), [](std::byte b) { return b == std::byte{0}; }));
  // A mapped image shorter than SizeOfImage cannot be the image the loader mapped.
  std::vector<std::byte> short_memory = built.memory;
  short_memory.resize(short_memory.size() - 0x1000);
  CHECK(refusal(short_memory, ImageLayout::Memory).problem == PeProblem::BadSizes);
}

TEST_CASE("a malformed resource directory or version resource is refused") {
  const BuiltImage built = build(sample_spec());
  std::vector<std::byte> bytes = built.memory;
  put_u32(bytes, built.rsrc_rva + 20, 0x80000000u | 0x7FFF0);  // the type entry's subdirectory, far past the end
  CHECK(refusal(bytes, ImageLayout::Memory).problem == PeProblem::BadResourceDirectory);
  bytes = built.memory;
  put_u32(bytes, kOptionalAt + 112 + 8 * 2 + 4, 0x7FFFFFFF);  // a resource directory larger than the image
  CHECK(refusal(bytes, ImageLayout::Memory) == PeError{PeProblem::BadResourceDirectory, built.rsrc_rva});
  bytes = built.memory;
  put_u16(bytes, built.rsrc_rva + 14, 0xFFFF);  // more entries than any directory may hold
  CHECK(refusal(bytes, ImageLayout::Memory).problem == PeProblem::BadResourceDirectory);
  bytes = built.memory;
  put_u32(bytes, built.version_rva + 40, 0xFEEF04BE);  // VS_FIXEDFILEINFO's signature
  CHECK(refusal(bytes, ImageLayout::Memory) == PeError{PeProblem::BadVersionResource, built.rsrc_rva});
  bytes = built.memory;
  put_u16(bytes, built.version_rva + 6, 'W');  // the root's key: "WS_VERSION_INFO"
  CHECK(refusal(bytes, ImageLayout::Memory).problem == PeProblem::BadVersionResource);
  bytes = built.memory;
  put_u16(bytes, built.version_rva, 0x4000);  // the root claims more bytes than the resource holds
  CHECK(refusal(bytes, ImageLayout::Memory).problem == PeProblem::BadVersionResource);
  bytes = built.memory;
  put_u32(bytes, built.rsrc_rva + 76, 0x20000);  // a data entry larger than a version resource may be
  CHECK(refusal(bytes, ImageLayout::Memory).problem == PeProblem::BadVersionResource);
}

TEST_CASE("a malformed function table is refused") {
  const BuiltImage built = build(sample_spec());
  const std::uint32_t pdata_dir = kOptionalAt + 112 + 8 * 3;
  std::vector<std::byte> bytes = built.memory;
  put_u32(bytes, pdata_dir + 4, 13);  // not whole entries
  CHECK(refusal(bytes, ImageLayout::Memory) == PeError{PeProblem::BadExceptionDirectory, built.pdata_rva});
  bytes = built.memory;
  put_u32(bytes, built.pdata_rva + 12, built.rva.at(".text"));  // the second entry starts before the first ends
  CHECK(refusal(bytes, ImageLayout::Memory) == PeError{PeProblem::BadExceptionDirectory, built.pdata_rva + 12});
  bytes = built.memory;
  put_u32(bytes, built.pdata_rva + 4, built.rva.at(".text"));  // ends before it begins
  CHECK(refusal(bytes, ImageLayout::Memory).problem == PeProblem::BadExceptionDirectory);
  bytes = built.memory;
  put_u32(bytes, built.pdata_rva + 8, 0xFFFFFFF0u);  // unwind information past the image
  CHECK(refusal(bytes, ImageLayout::Memory).problem == PeProblem::BadExceptionDirectory);
  PeLimits limits;
  limits.max_functions = 5;
  CHECK(refusal(built.memory, ImageLayout::Memory, limits).problem == PeProblem::TooManyFunctions);
}

TEST_CASE("an entry of no length in the function table is left out, as GNU toolchains emit them") {
  const BuiltImage built = build(sample_spec());
  const std::uint32_t text = built.rva.at(".text");
  std::vector<std::byte> bytes = built.memory;
  put_u32(bytes, built.pdata_rva + 2 * 12 + 4, text + 0x400);  // the third entry, [0x400, 0x500), now [0x400, 0x400)
  put_u32(bytes, built.pdata_rva + 3 * 12, text + 0x400);      // and the fourth starts where it starts
  const auto image = read_image(SpanSource(bytes), ImageLayout::Memory);
  REQUIRE(image.has_value());
  REQUIRE(image->functions.size() == 5);
  CHECK(image->functions[2] == RuntimeFunction{text + 0x400, text + 0x1000, image->functions[2].unwind});
  CHECK(image->function_at(text + 0x400) == &image->functions[2]);
}

TEST_CASE("bytes the source cannot give are refused as unreadable, and the reader keeps no pointer to them") {
  const BuiltImage built = build(sample_spec());
  const HoleSource headers_hole(built.memory, kOptionalAt + 8, kOptionalAt + 9);
  CHECK(read_image(headers_hole, ImageLayout::Memory).error() == PeError{PeProblem::Unreadable, kOptionalAt});
  const HoleSource pdata_hole(built.memory, built.pdata_rva + 30, built.pdata_rva + 31);
  CHECK(read_image(pdata_hole, ImageLayout::Memory).error().problem == PeProblem::Unreadable);
  const HoleSource code_hole(built.memory, built.rva.at(".text"), built.rva.at(".text") + 0x1000);
  CHECK(read_image(code_hole, ImageLayout::Memory).has_value());  // the code is not read to read the image
}

TEST_CASE("describe names the problem and where") {
  CHECK(describe(PeError{PeProblem::BadSectionTable, 0x1F8}) == "the section table runs past the headers (at 0x1f8)");
  CHECK(std::string(to_string(PeProblem::NotAmd64)) == "the machine is not x64");
}

TEST_CASE("a section's refusal names the rule, the section by index and its numbers, in at most 192 characters") {
  SectionFault f;
  f.index = 3;
  f.bound = 0x5000;
  f.rva = 0x4000;
  f.virtual_size = 0x1200;
  f.raw_offset = 0x3400;
  f.raw_size = 0x1200;
  f.characteristics = 0x40000040;
  const auto said = [&](SectionRule rule, std::uint32_t index, std::uint64_t bound) {
    f.rule = rule;
    f.index = index;
    f.bound = bound;
    return describe(PeError{PeProblem::BadSection, 0x1F8, f});
  };
  const std::string numbers = "va 0x4000 vsize 0x1200 raw 0x3400 rawsize 0x1200 flags 0x40000040 (at 0x1f8)";
  CHECK(said(SectionRule::Overlaps, 3, 0x5000) ==
        "a section is misplaced: section 3 starts below the end of section 2, 0x5000; " + numbers);
  CHECK(said(SectionRule::Overlaps, 0, 0x400) ==
        "a section is misplaced: section 0 starts below the end of the headers, 0x400; " + numbers);
  CHECK(said(SectionRule::Unaligned, 3, 0x1000) ==
        "a section is misplaced: section 3 is not aligned to the section alignment, 0x1000; " + numbers);
  CHECK(said(SectionRule::PastImage, 3, 0x9000) ==
        "a section is misplaced: section 3 ends past the end of the image, 0x9000; " + numbers);
  CHECK(said(SectionRule::RawWraps, 3, 0) == "a section is misplaced: section 3's raw data ends past 4 GiB; " + numbers);
  CHECK(describe_numbers(0x1000, 0x2400, 0x400, 0x2400, kCode) ==
        "va 0x1000 vsize 0x2400 raw 0x400 rawsize 0x2400 flags 0x60000020");
  // probe-exe's line, "probe-exe: the executable is not a PE32+ image for x64: " and this, stays within the 240
  // characters a line of DataPiler's log holds after "probe-exe: ", with every number at its widest.
  f.rva = f.virtual_size = f.raw_offset = f.raw_size = f.characteristics = 0xFFFFFFFFu;
  for (const SectionRule rule :
       {SectionRule::Unaligned, SectionRule::Overlaps, SectionRule::PastImage, SectionRule::RawWraps}) {
    f.rule = rule;
    f.index = PeLimits{}.max_sections - 1;
    f.bound = 0x100000000ull;
    const std::string text = describe(PeError{PeProblem::BadSection, 0x11EF0, f});
    CHECK(text.size() <= 192);
    CHECK(std::all_of(text.begin(), text.end(), [](char c) { return c >= 0x20 && c < 0x7F && c != '/' && c != '\\'; }));
  }
}

TEST_CASE("the section table reads as it stands, for an image read_image refuses") {
  const BuiltImage built = build(sample_spec());
  std::vector<std::byte> bytes = built.file;
  put_u32(bytes, kSectionTableAt + 40 + 12, 0x1000);  // .rdata over .text
  REQUIRE_FALSE(read_image(SpanSource(bytes), ImageLayout::File).has_value());
  const auto table = read_section_table(SpanSource(bytes));
  REQUIRE(table.has_value());
  CHECK(table->source_size == bytes.size());
  CHECK(table->size_of_image == built.size_of_image);
  CHECK(table->size_of_headers == kHeadersSize);
  CHECK(table->section_alignment == kSectionAlign);
  CHECK(table->file_alignment == kFileAlign);
  REQUIRE(table->sections.size() == 6);
  CHECK(table->sections[1].rva == 0x1000);
  CHECK(table->sections[1].virtual_size == 0x300);
  CHECK(table->sections[1].raw_offset == 0x2800);
  CHECK(table->sections[1].raw_size == 0x400);
  CHECK(table->sections[1].characteristics == kReadOnly);
  // Only what a safe read needs is checked: no PE signature, or a table past the file, is still refused.
  put_u32(bytes, kNtOffset, 0x00004551);
  CHECK(read_section_table(SpanSource(bytes)).error() == PeError{PeProblem::NoPeSignature, kNtOffset});
  std::vector<std::byte> cut(built.file.begin(), built.file.begin() + kSectionTableAt + 40);
  CHECK(read_section_table(SpanSource(cut)).error().problem == PeProblem::Unreadable);
}

namespace {

// Invented code for the layouts below: a planted run at 0x123 and another at 0x700, unique in the section.
constexpr std::uint8_t kPlanted[] = {0x5A, 0x6B, 0x7C, 0x8D, 0x9E, 0xAF};

FakeImageSpec layout_spec() {
  FakeImageSpec spec;
  std::vector<std::uint8_t> code(0x900);
  for (std::size_t i = 0; i < code.size(); ++i) code[i] = static_cast<std::uint8_t>(0x10 + (i % 0x40));
  std::copy(std::begin(kPlanted), std::end(kPlanted), code.begin() + 0x123);
  std::copy(std::begin(kPlanted), std::end(kPlanted), code.begin() + 0x700);
  spec.sections.push_back(FakeSection{".text", kCode, code, 0});
  std::vector<std::uint8_t> rdata(0x500);
  for (std::size_t i = 0; i < rdata.size(); ++i) rdata[i] = static_cast<std::uint8_t>((i * 13 + 7) & 0x3F);
  spec.sections.push_back(FakeSection{".rdata", kReadOnly, rdata, 0});
  spec.sections.push_back(FakeSection{".data", kReadWrite, std::vector<std::uint8_t>(0x300, 0x22), 0x2000});
  spec.version = FakeVersion{};
  spec.functions = {{0x10, 0x100, false, false}, {0x100, 0x400, false, false}, {0x400, 0x480, true, false}};
  return spec;
}

// The planted runs in .text, through the pattern scan.
PatternResult scan_planted(const ImageInfo& image, const ByteSource& source) {
  PatternSet set;
  PatternSpec spec;
  spec.id = "planted";
  spec.kind = PatternKind::Idiom;
  spec.section = ".text";
  spec.bytes = *parse_pattern("5A 6B 7C 8D 9E AF");
  spec.expect_min = 0;
  spec.expect_max = 8;
  set.patterns.push_back(spec);
  return scan_patterns(image, source, set).results.at(0);
}

// The file, read as the loader maps it, gives what the mapped image gives: the same sections, function table and
// version resource, every section's bytes (through read_mapped and ImageBytes, nothing counted unreadable), and the
// planted runs' hits that the file holds.
void check_as_loaded(const BuiltImage& built) {
  const SpanSource file_source(built.file);
  const SpanSource memory_source(built.memory);
  const ImageInfo file = read(built.file, ImageLayout::File);
  const ImageInfo memory = read(built.memory, ImageLayout::Memory);
  CHECK(file.sections == memory.sections);
  CHECK(file.functions == memory.functions);
  REQUIRE(file.version.has_value());
  REQUIRE(memory.version.has_value());
  CHECK(file.version->product_name == memory.version->product_name);
  CHECK(file.version->file_version == memory.version->file_version);
  const ImageBytes copies(file, file_source);
  for (const Section& s : file.sections) {
    CAPTURE(s.name);
    const SectionCopy* copy = copies.named(s.name);
    REQUIRE(copy != nullptr);
    CHECK(copy->unreadable == 0);
    const FileSpan held = file.file_span(s);
    if (s.mapped_size() == 0) {  // a section of no size: nothing to read, and nothing of it in the file
      CHECK(copy->bytes.empty());
      CHECK(held == FileSpan{});
      continue;
    }
    std::vector<std::byte> mapped(s.mapped_size());
    REQUIRE(read_mapped(file, file_source, s.rva, mapped));
    CHECK(std::equal(mapped.begin(), mapped.end(), built.memory.begin() + s.rva));
    CHECK(copy->bytes == mapped);
    CHECK(held.offset + held.size <= built.file.size());
    if (held.size != 0) CHECK(file.offset_of(s.rva, held.size) == held.offset);
    CHECK_FALSE(file.offset_of(s.rva, held.size + 1).has_value());  // past what the file holds: read_mapped's zeros
  }
  for (const RuntimeFunction& f : file.functions) {
    CHECK(is_fragment(file, file_source, f) == is_fragment(memory, memory_source, f));
  }
  const PatternResult in_file = scan_planted(file, file_source);
  const PatternResult in_memory = scan_planted(memory, memory_source);
  CHECK(in_file.resolved());
  CHECK(in_file.hits == in_memory.hits);
}

}  // namespace

TEST_CASE("a linker's layout reads as loaded, the planted runs found twice") {
  const BuiltImage built = build(layout_spec());
  check_as_loaded(built);
  const ImageInfo file = read(built.file, ImageLayout::File);
  const std::uint32_t text = built.rva.at(".text");
  CHECK(scan_planted(file, SpanSource(built.file)).hits == std::vector<std::uint32_t>{text + 0x123, text + 0x700});
}

TEST_CASE("raw data out of order reads as loaded") {
  FakeImageSpec spec = layout_spec();
  spec.raw = {FakeRaw{0, 0x10000, 0xA00}, FakeRaw{1, 0xF000, 0x600}};  // .text's raw data after .rdata's, both last
  const BuiltImage built = build(spec);
  check_as_loaded(built);
  const ImageInfo file = read(built.file, ImageLayout::File);
  CHECK(file.file_span(file.sections[0]) == FileSpan{0x10000, 0x900});  // at most its virtual size
  CHECK(file.file_span(file.sections[1]) == FileSpan{0xF000, 0x500});
}

TEST_CASE("overlapping and shared raw data reads as loaded, each section seeing the bytes the file holds") {
  FakeImageSpec spec = layout_spec();
  // .rdata's raw data starts inside .text's, and a second section shares .rdata's whole.
  spec.sections.insert(spec.sections.begin() + 2, FakeSection{".alias", kReadOnly, spec.sections[1].bytes, 0});
  spec.raw = {FakeRaw{0, 0x10000, 0xA00}, FakeRaw{1, 0x10800, 0x600}, FakeRaw{2, 0x10800, 0x600}};
  const BuiltImage built = build(spec);
  check_as_loaded(built);
  const ImageInfo file = read(built.file, ImageLayout::File);
  const SpanSource source(built.file);
  std::vector<std::byte> tail(0x100);
  std::vector<std::byte> head(0x100);
  REQUIRE(read_mapped(file, source, built.rva.at(".text") + 0x800, tail));
  REQUIRE(read_mapped(file, source, built.rva.at(".rdata"), head));
  CHECK(tail == head);  // .text's last bytes are .rdata's first, as the loader maps them
  std::vector<std::byte> alias(0x500);
  std::vector<std::byte> rdata(0x500);
  REQUIRE(read_mapped(file, source, built.rva.at(".alias"), alias));
  REQUIRE(read_mapped(file, source, built.rva.at(".rdata"), rdata));
  CHECK(alias == rdata);
}

TEST_CASE("empty raw data maps as zeros: SizeOfRawData 0 with a pointer, or PointerToRawData 0 with a size") {
  FakeImageSpec spec = layout_spec();
  spec.sections.push_back(FakeSection{".bss", kReadWrite, {}, 0x3000});
  spec.sections.push_back(FakeSection{".nofile", kReadOnly, std::vector<std::uint8_t>(0x100, 0x33), 0});
  spec.raw = {FakeRaw{3, kHeadersSize, 0}, FakeRaw{4, 0, 0x200}};
  const BuiltImage built = build(spec);
  check_as_loaded(built);
  const ImageInfo file = read(built.file, ImageLayout::File);
  for (const char* name : {".bss", ".nofile"}) {
    const Section* s = file.section(name);
    REQUIRE(s != nullptr);
    CHECK(file.file_span(*s) == FileSpan{});
    std::vector<std::byte> bytes(s->mapped_size(), std::byte{0x77});
    REQUIRE(read_mapped(file, SpanSource(built.file), s->rva, bytes));
    CHECK(std::all_of(bytes.begin(), bytes.end(), [](std::byte b) { return b == std::byte{0}; }));
  }
}

TEST_CASE("raw data past the end of the file reads as far as the file holds it, the rest as zeros") {
  FakeImageSpec spec = layout_spec();
  // .text's raw data runs 0x500 bytes into the file's last 0x500, .rdata's lies wholly past it.
  spec.raw = {FakeRaw{0, 0x20000, 0xA00}, FakeRaw{1, 0x7FFF0000, 0x600}};
  spec.file_size = 0x20500;
  const BuiltImage built = build(spec);
  REQUIRE(built.file.size() == 0x20500);
  check_as_loaded(built);
  const ImageInfo file = read(built.file, ImageLayout::File);
  const SpanSource source(built.file);
  CHECK(file.file_span(file.sections[0]) == FileSpan{0x20000, 0x500});
  CHECK(file.file_span(file.sections[1]) == FileSpan{});
  // The run planted at 0x700 lies past what the file holds: zeros, as loaded, and not found.
  const std::uint32_t text = built.rva.at(".text");
  CHECK(scan_planted(file, source).hits == std::vector<std::uint32_t>{text + 0x123});
  const ImageBytes copies(file, source);
  CHECK(copies.u32(text + 0x700) == 0u);
  CHECK(copies.u8(text + 0x123) == 0x5A);
}

TEST_CASE("a section of no size reads, and holds no RVA") {
  FakeImageSpec spec = layout_spec();
  spec.sections.insert(spec.sections.begin() + 1, FakeSection{".empty", kReadOnly, {}, 0});
  const BuiltImage built = build(spec);
  check_as_loaded(built);
  const ImageInfo file = read(built.file, ImageLayout::File);
  const Section* empty = file.section(".empty");
  REQUIRE(empty != nullptr);
  CHECK(empty->mapped_size() == 0);
  CHECK(file.section_at(empty->rva) == nullptr);
  CHECK_FALSE(file.maps(empty->rva, 1));
}

TEST_CASE("SizeOfRawData past the virtual size maps only the virtual size") {
  FakeImageSpec spec = layout_spec();
  spec.sections[1].virtual_size = 0x100;  // .rdata: 0x500 bytes in the file, 0x100 mapped
  const BuiltImage built = build(spec);
  check_as_loaded(built);
  const ImageInfo file = read(built.file, ImageLayout::File);
  CHECK(file.sections[1].raw_size == 0x600);
  CHECK(file.file_span(file.sections[1]).size == 0x100);
  CHECK_FALSE(file.maps(built.rva.at(".rdata") + 0x100, 1));
}

TEST_CASE("a virtual size far past the raw data reads, its tail as zeros") {
  FakeImageSpec spec = layout_spec();
  spec.sections.push_back(FakeSection{".huge", kReadWrite, std::vector<std::uint8_t>(0x200, 0x44), 0x40000000});
  spec.with_memory = false;  // a mapped image of a GiB is more than a test needs
  const BuiltImage built = build(spec);
  const ImageInfo file = read(built.file, ImageLayout::File);
  const SpanSource source(built.file);
  const Section* huge = file.section(".huge");
  REQUIRE(huge != nullptr);
  CHECK(huge->mapped_size() == 0x40000000u);
  CHECK(file.file_span(*huge).size == 0x200);
  std::array<std::byte, 8> edge{};
  REQUIRE(read_mapped(file, source, huge->rva + 0x1FC, edge));  // half held, half zeros
  CHECK(edge == std::array<std::byte, 8>{std::byte{0x44}, std::byte{0x44}, std::byte{0x44}, std::byte{0x44}, {}, {}, {},
                                         {}});
  std::array<std::byte, 8> last{std::byte{1}};
  REQUIRE(read_mapped(file, source, huge->rva + 0x40000000u - 8, last));
  CHECK(last == std::array<std::byte, 8>{});
  CHECK_FALSE(read_mapped(file, source, huge->rva + 0x40000000u - 4, edge));  // runs past the section
  const ImageBytes copies(file, source, 1u << 20);
  CHECK(copies.left_out() == 1);
  CHECK(copies.named(".text") != nullptr);
}

TEST_CASE("SizeOfImage short of the last section's alignment reads, as the loader rounds it; shorter is refused") {
  FakeImageSpec spec = layout_spec();
  const std::uint32_t aligned = build(spec).size_of_image;
  spec.size_of_image = aligned - 0x800;
  const BuiltImage built = build(spec);
  check_as_loaded(built);
  CHECK(read(built.file, ImageLayout::File).size_of_image == aligned - 0x800);
  spec.size_of_image = aligned - 0x1000;
  const PeError e = refusal(build(spec).file);
  REQUIRE(e.section.has_value());
  CHECK(e.section->rule == SectionRule::PastImage);
  CHECK(e.section->bound == aligned - 0x1000);
}
