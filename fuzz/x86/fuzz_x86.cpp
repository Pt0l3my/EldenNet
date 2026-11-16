// libFuzzer target: en-gal's x86-64 length decoder (x86.hpp) on arbitrary bytes, decoded at every offset. Besides any
// sanitizer report (each decode gets an exact-size copy, so a read past the bytes given is one), a broken property is
// a finding:
// - an instruction decoded is 1 to 15 bytes, no longer than the bytes given; its prefixes, opcode, ModRM, SIB,
//   displacement and immediates lie within it, in that order, and the last of them ends it;
// - a RIP-relative operand has ModRM and a 32-bit displacement; a relative branch, an immediate of 1, 2 or 4 bytes;
// - what follows an instruction changes nothing: the same bytes with others after them decode the same;
// - every shorter run of an instruction's bytes is refused as truncated;
// - a refusal is truncated only where more bytes could help: with 15 bytes or more given, never.
// An exception that escapes is a finding too, so this entry has no ffi_guard (R3 is for the engine's entries).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>

#include "en/gal/x86.hpp"

namespace {

using namespace en::gal;

void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

bool same(const x86::Instruction& a, const x86::Instruction& b) {
  return a.length == b.length && a.encoding == b.encoding && a.map == b.map && a.opcode == b.opcode &&
         a.disp_at == b.disp_at && a.disp_size == b.disp_size && a.rip_relative == b.rip_relative &&
         a.imm_at == b.imm_at && a.imm_size == b.imm_size && a.imm2_at == b.imm2_at && a.imm2_size == b.imm2_size &&
         a.relative == b.relative && a.flow == b.flow && a.modrm == b.modrm && a.has_modrm == b.has_modrm;
}

void check_one(std::span<const std::byte> at) {
  // An exact-size copy on the heap, so that a read past it is AddressSanitizer's to see.
  const std::size_t n = at.size() < 15 ? at.size() : 15;
  std::vector<std::byte> exact(at.begin(), at.begin() + static_cast<std::ptrdiff_t>(n));
  const auto in = x86::decode(exact);
  if (!in) {
    require(in.error() != x86::DecodeProblem::Truncated || n < 15, "a refusal for want of bytes, with 15 given");
    return;
  }
  const x86::Instruction& i = *in;
  require(i.length >= 1 && i.length <= 15 && i.length <= n, "1 to 15 bytes, within those given");
  require(i.prefixes <= i.opcode_at && i.opcode_at < i.length, "the opcode after the prefixes, within");
  std::size_t end = i.opcode_at + 1u;
  if (i.has_modrm) {
    require(i.modrm_at >= end && i.modrm_at < i.length, "ModRM after the opcode");
    end = i.modrm_at + 1u + (i.has_sib ? 1u : 0u);
  }
  if (i.disp_size != 0) {
    require(i.disp_size == 1 || i.disp_size == 4 || i.disp_size == 8, "a displacement of 1, 4 or 8 bytes");
    require(i.disp_at == end, "the displacement right after ModRM and SIB (or the opcode, for moffs)");
    end = i.disp_at + std::size_t{i.disp_size};
  }
  if (i.imm_size != 0) {
    require(i.imm_at == end, "the immediate right after the displacement");
    end = i.imm_at + std::size_t{i.imm_size};
  }
  if (i.imm2_size != 0) {
    require(i.imm2_at == end, "the second immediate right after the first");
    end = i.imm2_at + std::size_t{i.imm2_size};
  }
  require(end == i.length, "the last part ends the instruction");
  if (i.rip_relative) require(i.has_modrm && i.disp_size == 4, "RIP-relative: ModRM and a 32-bit displacement");
  if (i.relative) require(i.imm_size == 1 || i.imm_size == 2 || i.imm_size == 4, "a branch of 1, 2 or 4 bytes");

  std::vector<std::byte> longer(exact.begin(), exact.begin() + i.length);
  longer.resize(i.length + 15, std::byte{0xC4});
  const auto again = x86::decode(longer);
  require(again.has_value() && same(*again, i), "what follows changes nothing");
  for (std::size_t k = 0; k < i.length; ++k) {
    std::vector<std::byte> cut(exact.begin(), exact.begin() + static_cast<std::ptrdiff_t>(k));
    const auto shorter = x86::decode(cut);
    require(!shorter.has_value() && shorter.error() == x86::DecodeProblem::Truncated, "a shorter run is truncated");
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  std::vector<std::byte> bytes(size);
  for (std::size_t i = 0; i < size; ++i) bytes[i] = std::byte{data[i]};
  const std::span<const std::byte> all(bytes);
  for (std::size_t off = 0; off < all.size() && off < 512; ++off) check_one(all.subspan(off));
  // And a sweep, as the derivation decodes a section: one instruction after another.
  std::size_t at = 0;
  while (at < all.size()) {
    const auto in = x86::decode(all.subspan(at));
    at += in ? in->length : 1;
  }
  return 0;
}
