#pragma once
// FakeImage: a PE32+ image for x64 of the tests' own, built from invented bytes, in both of read_image's layouts: the
// file on disk and the image as the loader maps it. It holds whatever sections a test gives it, and can add a version
// resource (.rsrc) and an exception directory (.pdata, with its unwind information in .xdata). Nothing in it is any
// real program's: the names, numbers and bytes are made up for the tests.
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "en/gal/pe.hpp"

namespace en::gal::test {

inline constexpr std::uint32_t kNtOffset = 0x80;  // e_lfanew
inline constexpr std::uint32_t kFileHeaderAt = kNtOffset + 4;
inline constexpr std::uint32_t kOptionalAt = kNtOffset + 24;
inline constexpr std::uint32_t kOptionalSize = 240;  // PE32+ with 16 data directories
inline constexpr std::uint32_t kSectionTableAt = kOptionalAt + kOptionalSize;
inline constexpr std::uint32_t kHeadersSize = 0x400;
inline constexpr std::uint32_t kSectionAlign = 0x1000;
inline constexpr std::uint32_t kFileAlign = 0x200;

inline constexpr std::uint32_t kCode = kSectionCode | kSectionExecute | kSectionRead;
inline constexpr std::uint32_t kReadOnly = 0x00000040 | kSectionRead;  // initialised data, read
inline constexpr std::uint32_t kReadWrite = 0x00000040 | kSectionRead | kSectionWrite;

struct FakeSection {
  std::string name;
  std::uint32_t characteristics = kReadOnly;
  std::vector<std::uint8_t> bytes;
  std::uint32_t virtual_size = 0;  // 0: the bytes' size
};

struct FakeVersion {
  std::array<std::uint16_t, 4> file_version{9, 8, 7, 0};
  std::array<std::uint16_t, 4> product_version{9, 8, 0, 0};
  std::optional<std::u16string> product_name = u"INVENTED GAME";
  std::optional<Translation> translation = Translation{0x0409, 0x04B0};
  std::uint16_t language = 0x0409;
  std::string table_key;  // the StringTable's key; empty: the translation's, as hex
};

// A function of .text, by offsets into .text's bytes.
struct FakeFunction {
  std::uint32_t begin = 0;
  std::uint32_t end = 0;
  bool chained = false;   // its unwind information is chained to the function before it (UNW_FLAG_CHAININFO)
  bool indirect = false;  // its entry points at the entry before it instead (bit 0 of its unwind RVA)
};

struct FakeImageSpec {
  std::uint64_t image_base = 0x7FF7'0000'0000;
  std::uint32_t time_date_stamp = 0x1234'5678;
  std::vector<FakeSection> sections;    // in order; the first named ".text" holds the functions
  std::optional<FakeVersion> version;   // adds .rsrc after them
  std::vector<FakeFunction> functions;  // adds .xdata and .pdata after them
};

struct BuiltImage {
  std::vector<std::byte> file;
  std::vector<std::byte> memory;
  std::map<std::string, std::uint32_t> rva;  // each section's RVA, by name
  std::uint32_t size_of_image = 0;
  std::uint32_t pdata_rva = 0;
  std::uint32_t rsrc_rva = 0;
  std::uint32_t version_rva = 0;  // the version resource's data
};

BuiltImage build(const FakeImageSpec& spec);

// Little-endian writes into bytes the test holds, to break an image on purpose.
void put_u16(std::vector<std::byte>& bytes, std::size_t at, std::uint16_t v);
void put_u32(std::vector<std::byte>& bytes, std::size_t at, std::uint32_t v);

// A source over bytes that refuses every read touching [bad_from, bad_to): an unreadable range of a mapped image.
class HoleSource final : public ByteSource {
 public:
  HoleSource(const std::vector<std::byte>& bytes, std::uint64_t bad_from, std::uint64_t bad_to)
      : bytes_(bytes), from_(bad_from), to_(bad_to) {}
  [[nodiscard]] std::uint64_t size() const noexcept override { return bytes_.size(); }
  [[nodiscard]] bool read(std::uint64_t offset, std::span<std::byte> dst) const noexcept override;
  mutable std::uint64_t reads = 0;

 private:
  const std::vector<std::byte>& bytes_;
  std::uint64_t from_;
  std::uint64_t to_;
};

}  // namespace en::gal::test
