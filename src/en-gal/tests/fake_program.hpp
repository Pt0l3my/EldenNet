#pragma once
// FakeProgram: a FakeImage (fake_image.hpp) whose sections are written like an assembler's: bytes appended to .text,
// .rdata or .data, labels, and references between them resolved once every label is known (32-bit displacements
// relative to an instruction's end, 64-bit addresses at the image's preferred base, image-relative offsets). The
// sections sit at fixed RVAs, so references resolve before the image is built. Everything in it is invented for the
// tests: no byte, name or layout of any real program.
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "fake_image.hpp"

namespace en::gal::test {

class FakeProgram {
 public:
  enum Sec { Text = 0, Rdata = 1, Data = 2 };
  static constexpr std::uint32_t kRva[3] = {0x1000, 0x9000, 0x11000};  // each section's RVA
  static constexpr std::uint32_t kSize[3] = {0x8000, 0x8000, 0x8000};  // and the most it may hold

  explicit FakeProgram(std::uint64_t image_base = 0x1'4000'0000) : image_base_(image_base) {}

  std::uint64_t image_base() const { return image_base_; }
  std::uint32_t here(Sec s) const { return kRva[s] + static_cast<std::uint32_t>(bytes_[s].size()); }
  std::uint32_t size(Sec s) const { return static_cast<std::uint32_t>(bytes_[s].size()); }

  void bytes(Sec s, std::initializer_list<std::uint8_t> b) { bytes_[s].insert(bytes_[s].end(), b); }
  void bytes(Sec s, const std::vector<std::uint8_t>& b) { bytes_[s].insert(bytes_[s].end(), b.begin(), b.end()); }
  void zeros(Sec s, std::size_t n) { bytes_[s].insert(bytes_[s].end(), n, 0); }
  void align(Sec s, std::uint32_t a, std::uint8_t fill = 0) {
    while (bytes_[s].size() % a != 0) bytes_[s].push_back(fill);
  }
  void u32(Sec s, std::uint32_t v);
  void u64(Sec s, std::uint64_t v);
  void ascii(Sec s, const std::string& text);  // with its NUL
  void utf16(Sec s, const std::string& text);  // ASCII text as UTF-16LE, with its NUL

  // A label at the section's current end.
  void label(const std::string& name, Sec s);
  std::uint32_t rva(const std::string& name) const;

  // References, resolved by build(): a 32-bit displacement relative to the end of its instruction, which ends `after`
  // bytes past the displacement; a 64-bit address at the preferred base (a base relocation if `relocate`); a 32-bit
  // image-relative offset.
  void rel32(Sec s, const std::string& target, std::uint32_t after = 0);
  void va64(Sec s, const std::string& target, bool relocate = true);
  void rva32(Sec s, const std::string& target);

  // A function of the exception directory, between two labels of .text (chained: a fragment of the one before).
  void function(const std::string& begin, const std::string& end, bool chained = false);

  BuiltImage build(std::optional<FakeVersion> version = FakeVersion{});

 private:
  struct Ref {
    int kind;  // 0 rel32, 1 va64, 2 rva32
    Sec sec;
    std::uint32_t at;
    std::string target;
    std::uint32_t after;
    bool relocate;
  };
  std::uint64_t image_base_;
  std::vector<std::uint8_t> bytes_[3];
  std::map<std::string, std::uint32_t> labels_;
  std::vector<Ref> refs_;
  std::vector<std::pair<std::string, std::string>> fns_;
  std::vector<bool> chained_;
};

}  // namespace en::gal::test
