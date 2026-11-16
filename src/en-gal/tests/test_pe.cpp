// The PE32+ reader (pe.hpp) on FakeImages of invented bytes: the same facts from the file and from the mapped image;
// the version resource, the sections and the function table; RVAs placed in the source; fragments; and every way a
// malformed image is refused, with where.
#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

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
  // Sections: out of order, past the image, raw bytes past the file.
  CHECK(refusal(broken([](auto& b) { put_u32(b, kSectionTableAt + 40 + 12, 0x1000); })).problem ==
        PeProblem::BadSection);
  CHECK(refusal(broken([&](auto& b) { put_u32(b, kSectionTableAt + 8, built.size_of_image); })).problem ==
        PeProblem::BadSection);
  CHECK(refusal(broken([](auto& b) { put_u32(b, kSectionTableAt + 20, 0x7FFF0000); })) ==
        PeError{PeProblem::BadSection, kSectionTableAt});
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
