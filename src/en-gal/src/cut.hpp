#pragma once
// Internal to en-gal: cutting patterns from decoded code (the derivation, derive.hpp). A pattern covers whole
// instructions; within them it wildcards what could change between builds or once the image is mapped:
// - every RIP-relative displacement and every absolute address (moffs);
// - every other 32-bit displacement: a large field or stack offset, which the next build may change;
// - every relative branch's displacement, unless its target lies within the pattern itself, where the pattern's own
//   bytes fix it;
// - every byte a base relocation covers (the loader rewrites them);
// - every 64-bit immediate, and every 16- or 32-bit immediate whose value does not fit in a signed byte (sizes, ids and
//   line numbers, which the next build may change).
// The rest is kept: opcodes, ModRM and SIB, 8-bit displacements (an object's first fields) and small immediates.
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "code.hpp"
#include "en/gal/image_bytes.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/pe.hpp"

namespace en::gal::cut {

// The wildcarded bytes of instructions [first, last] of `insns`, as one pattern spanning them.
[[nodiscard]] BytePattern pattern_of(const ImageBytes& img, const SectionCopy& text, std::span<const Fixup> fixups,
                                     std::span<const code::Decoded> insns);

// Which bytes of one instruction are fields (a displacement or an immediate) as [offset, size) pairs, in the
// instruction.
struct Field {
  std::uint8_t at = 0;
  std::uint8_t size = 0;
};
[[nodiscard]] std::vector<Field> fields_of(const x86::Instruction& insn);

struct Request {
  std::uint32_t fn_begin = 0;  // the function, of the exception directory, that the pattern lies inside
  std::uint32_t fn_end = 0;
  std::uint32_t anchor_min = 5;        // ARCH §6.2: at entry + 5 or later
  std::optional<std::uint32_t> cover;  // an instruction the pattern must hold (a static's reference)
};

struct Cut {
  BytePattern bytes;
  std::uint32_t start = 0;
  std::uint64_t matches = 0;
  std::optional<Capture> capture;  // the covered instruction's RIP-relative displacement, when it has one
};

// The best pattern of at most 64 bytes that matches once in the code section: of the unique ones, the shortest with 20
// bytes and 12 fixed if any (a margin for the next build), else the shortest; nothing when none is unique.
[[nodiscard]] std::optional<Cut> cut(const ImageBytes& img, const SectionCopy& text, std::span<const Fixup> fixups,
                                     const Request& request);

}  // namespace en::gal::cut
