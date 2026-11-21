// en-gal's x86-64 instruction-length decoder (x86.hpp), from the architecture's documented encoding, 64-bit mode only.
//
// An instruction is: legacy prefixes (F0, F2, F3, 2E, 36, 3E, 26, 64, 65, 66, 67) and a REX prefix (40 to 4F, in
// effect only right before the opcode), or a VEX (C4, C5), EVEX (62) or XOP (8F with a map of 8 or more) prefix; then
// the opcode (one byte; 0F and a byte; 0F 38 or 0F 3A and a byte; or one byte after VEX, EVEX or XOP, whose map field
// names its table); then ModRM, SIB and a displacement where the opcode takes ModRM; then the immediates.
//
// Where 64-bit processors disagree, or the manuals leave a form undefined, this follows LLVM's disassembler, against
// which it is tested: a 66 prefix shortens JMP's, Jcc's and XBEGIN's displacement to 16 bits but not CALL's; a REX
// followed by another prefix is ignored (LLVM prints it alone); 0F 20 to 0F 23 take no SIB or displacement whatever
// ModRM's mod; 0F B8 is POPCNT with F3 and nothing without it; F1 (INT1, ICEBP) is refused, as LLVM 18 refuses it. It
// is more lenient than LLVM about opcodes a map leaves undefined, whose length it still gives from the map's shape.
// EVEX's map 4 (APX) and the VIA PadLock opcodes are refused: no compiler we build with emits them.
#include "en/gal/x86.hpp"

#include <array>

namespace en::gal::x86 {

using proto::Expected;
using proto::Unexpected;

const char* to_string(DecodeProblem problem) noexcept {
  switch (problem) {
    case DecodeProblem::Truncated: return "the bytes end before the instruction does";
    case DecodeProblem::TooLong: return "longer than 15 bytes";
    case DecodeProblem::Invalid: return "not an instruction of 64-bit mode";
  }
  return "not an instruction";
}

namespace {

// What an opcode of a legacy map takes after it.
enum : std::uint16_t {
  M = 0x001,    // ModRM (and SIB and a displacement as ModRM says)
  I8 = 0x002,   // an 8-bit immediate
  I16 = 0x004,  // a 16-bit immediate
  IZ = 0x008,   // a 16-bit immediate with a 66 prefix (and no REX.W), else 32-bit
  IV = 0x010,   // B8+r: a 64-bit immediate with REX.W, else as IZ
  J8 = 0x020,   // an 8-bit branch displacement
  JZ = 0x040,   // a 32-bit branch displacement (16-bit with 66, except CALL's)
  X = 0x080,    // not defined in 64-bit mode
  S = 0x100,    // handled before the table: a prefix, an escape, VEX, EVEX, XOP or moffs
};

// The one-byte map, sixteen opcodes a row.
// clang-format off
constexpr std::array<std::uint16_t, 256> kPrimary = {
    // 00
    M, M, M, M, I8, IZ, X, X, M, M, M, M, I8, IZ, X, S,
    // 10
    M, M, M, M, I8, IZ, X, X, M, M, M, M, I8, IZ, X, X,
    // 20
    M, M, M, M, I8, IZ, S, X, M, M, M, M, I8, IZ, S, X,
    // 30
    M, M, M, M, I8, IZ, S, X, M, M, M, M, I8, IZ, S, X,
    // 40: REX
    S, S, S, S, S, S, S, S, S, S, S, S, S, S, S, S,
    // 50
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 60: 62 is EVEX, 64 to 67 prefixes
    X, X, S, M, S, S, S, S, IZ, M | IZ, I8, M | I8, 0, 0, 0, 0,
    // 70
    J8, J8, J8, J8, J8, J8, J8, J8, J8, J8, J8, J8, J8, J8, J8, J8,
    // 80: 8F is POP r/m or XOP
    M | I8, M | IZ, X, M | I8, M, M, M, M, M, M, M, M, M, M, M, S,
    // 90
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, X, 0, 0, 0, 0, 0,
    // A0: A0 to A3 take moffs
    S, S, S, S, 0, 0, 0, 0, I8, IZ, 0, 0, 0, 0, 0, 0,
    // B0
    I8, I8, I8, I8, I8, I8, I8, I8, IV, IV, IV, IV, IV, IV, IV, IV,
    // C0: C4, C5 are VEX; C6, C7 checked below; C8 is ENTER
    M | I8, M | I8, I16, 0, S, S, M | I8, M | IZ, I16 | I8, 0, I16, 0, 0, I8, X, 0,
    // D0
    M, M, M, M, X, X, X, 0, M, M, M, M, M, M, M, M,
    // E0
    J8, J8, J8, J8, I8, I8, I8, I8, JZ, JZ, X, J8, 0, 0, 0, 0,
    // F0: F0, F2, F3 prefixes; F1 (INT1) refused, as LLVM does; F6, F7 checked below
    S, X, S, S, 0, 0, M, M, 0, 0, 0, 0, 0, 0, M, M,
};

// The two-byte map (0F xx).
constexpr std::array<std::uint16_t, 256> kSecondary = {
    // 00: 0F 0F is 3DNow!, checked below
    M, M, M, M, X, 0, 0, 0, 0, 0, X, 0, X, M, 0, S,
    // 10
    M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M,
    // 20
    M, M, M, M, X, X, X, X, M, M, M, M, M, M, M, M,
    // 30: 0F 38 and 0F 3A escape
    0, 0, 0, 0, 0, 0, X, 0, S, X, S, X, X, X, X, X,
    // 40
    M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M,
    // 50
    M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M,
    // 60
    M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M,
    // 70: 0F 78 checked below
    M | I8, M | I8, M | I8, M | I8, M, M, M, 0, M, M, X, X, M, M, M, M,
    // 80
    JZ, JZ, JZ, JZ, JZ, JZ, JZ, JZ, JZ, JZ, JZ, JZ, JZ, JZ, JZ, JZ,
    // 90
    M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M,
    // A0
    0, 0, 0, M, M | I8, M, X, X, 0, 0, 0, M, M | I8, M, M, M,
    // B0: 0F B8 checked below
    M, M, M, M, M, M, M, M, M, M, M | I8, M, M, M, M, M,
    // C0
    M, M, M | I8, M, M | I8, M | I8, M | I8, M, 0, 0, 0, 0, 0, 0, 0, 0,
    // D0
    M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M,
    // E0
    M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M,
    // F0
    M, M, M, M, M, M, M, M, M, M, M, M, M, M, M, M,
};
// clang-format on

// Map 1 opcodes that take an 8-bit immediate under VEX and EVEX as they do without.
constexpr bool map1_imm8(std::uint8_t op) noexcept {
  return (op >= 0x70 && op <= 0x73) || op == 0xC2 || op == 0xC4 || op == 0xC5 || op == 0xC6;
}

class Cursor {
 public:
  explicit Cursor(std::span<const std::byte> code) noexcept : code_(code) {}
  [[nodiscard]] std::size_t pos() const noexcept { return pos_; }
  // The byte `ahead` of the cursor, or nothing where the bytes given end or an instruction cannot reach.
  [[nodiscard]] std::optional<std::uint8_t> peek(std::size_t ahead = 0) const noexcept {
    const std::size_t at = pos_ + ahead;
    if (at >= code_.size() || at >= kMaxLength) return std::nullopt;
    return std::to_integer<std::uint8_t>(code_[at]);
  }
  // Why the byte `ahead` of the cursor is missing: the instruction would pass 15 bytes, or the bytes end first.
  [[nodiscard]] DecodeProblem missing(std::size_t ahead = 0) const noexcept {
    return pos_ + ahead >= kMaxLength ? DecodeProblem::TooLong : DecodeProblem::Truncated;
  }
  void skip(std::size_t n) noexcept { pos_ += n; }

 private:
  std::span<const std::byte> code_;
  std::size_t pos_ = 0;
};

constexpr bool legacy_prefix(std::uint8_t v) noexcept {
  return v == 0x66 || v == 0x67 || v == 0xF0 || v == 0xF2 || v == 0xF3 || v == 0x26 || v == 0x2E || v == 0x36 ||
         v == 0x3E || v == 0x64 || v == 0x65;
}

std::int64_t sign_extend(std::uint64_t raw, std::size_t size) noexcept {
  if (size == 0 || size >= 8) return static_cast<std::int64_t>(raw);
  const std::uint64_t sign = std::uint64_t{1} << (8 * size - 1);
  const std::uint64_t mask = (std::uint64_t{1} << (8 * size)) - 1;
  raw &= mask;
  return (raw & sign) != 0 ? static_cast<std::int64_t>(raw | ~mask) : static_cast<std::int64_t>(raw);
}

std::optional<std::int64_t> field(std::span<const std::byte> code, std::size_t at, std::size_t size) noexcept {
  if (size == 0 || size > 8 || at > code.size() || size > code.size() - at) return std::nullopt;
  std::uint64_t raw = 0;
  for (std::size_t i = 0; i < size; ++i) raw |= std::uint64_t{std::to_integer<std::uint8_t>(code[at + i])} << (8 * i);
  return sign_extend(raw, size);
}

}  // namespace

Expected<Instruction, DecodeProblem> decode(std::span<const std::byte> code) noexcept {
  Instruction in;
  Cursor c(code);

  // ---- Legacy prefixes and REX: a REX is in effect only right before the opcode ----
  std::optional<std::uint8_t> b = c.peek();
  while (true) {
    if (!b) return Unexpected{c.missing()};
    const std::uint8_t v = *b;
    if (v >= 0x40 && v <= 0x4F) {
      in.rex = v;  // the last REX counts
    } else if (legacy_prefix(v)) {
      in.rex = 0;  // a REX followed by another prefix is ignored
      if (v == 0x66) in.opsize = true;
      if (v == 0x67) in.adsize = true;
      if (v == 0xF0) in.lock = true;
      if (v == 0xF2 || v == 0xF3) in.rep = v;
      if (v == 0x26 || v == 0x2E || v == 0x36 || v == 0x3E || v == 0x64 || v == 0x65) in.segment = v;
    } else {
      break;
    }
    c.skip(1);
    ++in.prefixes;
    b = c.peek();
  }
  in.wide = (in.rex & 0x8) != 0;

  // ---- VEX, EVEX and XOP ----
  std::uint8_t op = *b;
  std::uint16_t flags = 0;
  bool legacy = true;
  if (op == 0xC4 || op == 0xC5 || op == 0x62 || (op == 0x8F && c.peek(1) && (*c.peek(1) & 0x1F) >= 8)) {
    if (in.rex != 0) return Unexpected{DecodeProblem::Invalid};  // a REX before VEX, EVEX or XOP is undefined
    legacy = false;
    std::uint8_t map = 0;
    std::size_t payload = 0;
    if (op == 0xC5) {
      const auto p0 = c.peek(1);
      if (!p0) return Unexpected{c.missing(1)};
      in.encoding = Encoding::Vex;
      map = 1;
      payload = 1;
    } else if (op == 0xC4) {
      const auto p0 = c.peek(1);
      const auto p1 = c.peek(2);
      if (!p0 || !p1) return Unexpected{c.missing(p0 ? 2 : 1)};
      in.encoding = Encoding::Vex;
      map = *p0 & 0x1F;
      in.wide = (*p1 & 0x80) != 0;
      if (map < 1 || map > 3) return Unexpected{DecodeProblem::Invalid};
      payload = 2;
    } else if (op == 0x62) {
      const auto p0 = c.peek(1);
      const auto p1 = c.peek(2);
      const auto p2 = c.peek(3);
      if (!p0 || !p1 || !p2) return Unexpected{c.missing(!p0 ? 1 : !p1 ? 2 : 3)};
      in.encoding = Encoding::Evex;
      map = *p0 & 0x07;
      in.wide = (*p1 & 0x80) != 0;
      if ((*p1 & 0x04) == 0) return Unexpected{DecodeProblem::Invalid};  // EVEX's fixed bit
      if (map != 1 && map != 2 && map != 3 && map != 5 && map != 6) return Unexpected{DecodeProblem::Invalid};
      payload = 3;
    } else {
      const auto p0 = c.peek(1);
      const auto p1 = c.peek(2);
      if (!p0 || !p1) return Unexpected{c.missing(p0 ? 2 : 1)};
      in.encoding = Encoding::Xop;
      map = *p0 & 0x1F;
      in.wide = (*p1 & 0x80) != 0;
      if (map != 8 && map != 9 && map != 10) return Unexpected{DecodeProblem::Invalid};
      payload = 2;
    }
    c.skip(1 + payload);
    const auto opcode = c.peek();
    if (!opcode) return Unexpected{c.missing()};
    in.map = map;
    in.opcode = *opcode;
    in.opcode_at = static_cast<std::uint8_t>(c.pos());
    c.skip(1);
    flags = M;
    if (in.encoding == Encoding::Vex && map == 1 && in.opcode == 0x77) flags = 0;  // VZEROUPPER, VZEROALL
    if (map == 3 || (map == 1 && map1_imm8(in.opcode)) || map == 8) flags |= I8;
    if (map == 10) flags |= IZ;
    if (in.encoding == Encoding::Xop) in.opsize = false;  // XOP's immediate is always 32-bit
  }

  // ---- The legacy opcode maps ----
  if (legacy) {
    in.opcode_at = static_cast<std::uint8_t>(c.pos());
    in.opcode = op;
    c.skip(1);
    if (op == 0x0F) {
      const auto second = c.peek();
      if (!second) return Unexpected{c.missing()};
      c.skip(1);
      if (*second == 0x38 || *second == 0x3A) {
        const auto third = c.peek();
        if (!third) return Unexpected{c.missing()};
        c.skip(1);
        in.map = *second == 0x38 ? 2 : 3;
        in.opcode = *third;
        in.opcode_at = static_cast<std::uint8_t>(c.pos() - 1);
        flags = *second == 0x38 ? M : M | I8;
      } else {
        in.map = 1;
        in.opcode = *second;
        in.opcode_at = static_cast<std::uint8_t>(c.pos() - 1);
        flags = kSecondary[*second];
        if (*second == 0x0F) {
          flags = M | I8;  // 3DNow!: ModRM, then the operation's own byte, read as an immediate
          in.map = 15;
        } else if (*second == 0x78) {
          if (in.opsize || in.rep == 0xF2) flags = M | I8;  // EXTRQ, INSERTQ: two 8-bit immediates (the second below)
        } else if (*second == 0xB8 && in.rep != 0xF3) {
          return Unexpected{DecodeProblem::Invalid};  // JMPE is not defined in 64-bit mode
        }
      }
    } else {
      in.map = 0;
      flags = kPrimary[op];
      if (op >= 0xA0 && op <= 0xA3) flags = 0;  // moffs, below
      if (op == 0x8F) flags = M;                // POP r/m (XOP was taken above)
    }
    if ((flags & X) != 0) return Unexpected{DecodeProblem::Invalid};
  }

  // ---- ModRM, SIB and the displacement ----
  if ((flags & M) != 0) {
    const auto modrm = c.peek();
    if (!modrm) return Unexpected{c.missing()};
    in.has_modrm = true;
    in.modrm_at = static_cast<std::uint8_t>(c.pos());
    in.modrm = *modrm;
    c.skip(1);
    const std::uint8_t mod = *modrm >> 6;
    const std::uint8_t rm = *modrm & 7;
    const bool control = legacy && in.map == 1 && in.opcode >= 0x20 && in.opcode <= 0x23;  // MOV CRn, DRn
    if (mod != 3 && !control) {
      if (rm == 4) {
        const auto sib = c.peek();
        if (!sib) return Unexpected{c.missing()};
        in.has_sib = true;
        in.sib = *sib;
        c.skip(1);
        if (mod == 0 && (*sib & 7) == 5) in.disp_size = 4;
      }
      if (mod == 0 && rm == 5) {
        in.disp_size = 4;
        in.rip_relative = true;
      } else if (mod == 1) {
        in.disp_size = 1;
      } else if (mod == 2) {
        in.disp_size = 4;
      }
      if (in.disp_size != 0) {
        in.disp_at = static_cast<std::uint8_t>(c.pos());
        c.skip(in.disp_size);
      }
    }
    // Groups whose ModRM chooses whether there is an immediate, or that leave some reg fields undefined.
    if (legacy && in.map == 0) {
      const std::uint8_t reg = (*modrm >> 3) & 7;
      if (op == 0xF6 && reg <= 1) flags |= I8;                                                  // TEST r/m8, imm8
      if (op == 0xF7 && reg <= 1) flags |= IZ;                                                  // TEST r/m, imm
      if (op == 0xC6 && reg != 0 && *modrm != 0xF8) return Unexpected{DecodeProblem::Invalid};  // only MOV, XABORT
      if (op == 0xC7) {
        if (*modrm == 0xF8) {
          flags = (flags & ~IZ) | JZ;  // XBEGIN rel
        } else if (reg != 0) {
          return Unexpected{DecodeProblem::Invalid};
        }
      }
      if (op == 0x8F && reg != 0) return Unexpected{DecodeProblem::Invalid};
      if (op == 0xFE && reg >= 2) return Unexpected{DecodeProblem::Invalid};  // only INC and DEC
      if (op == 0xFF && reg == 7) return Unexpected{DecodeProblem::Invalid};
    }
  } else if (legacy && in.map == 0 && op >= 0xA0 && op <= 0xA3) {
    in.disp_size = in.adsize ? 4 : 8;
    in.absolute = true;
    in.disp_at = static_cast<std::uint8_t>(c.pos());
    c.skip(in.disp_size);
  }

  // ---- The immediates ----
  const bool word = in.opsize && !in.wide;
  std::uint8_t size = 0;
  if ((flags & IV) != 0) {
    size = in.wide ? 8 : (in.opsize ? 2 : 4);
  } else if ((flags & IZ) != 0) {
    size = word ? 2 : 4;
  } else if ((flags & I16) != 0) {
    size = 2;
  } else if ((flags & I8) != 0) {
    size = 1;
  } else if ((flags & J8) != 0) {
    size = 1;
    in.relative = true;
  } else if ((flags & JZ) != 0) {
    const bool call = legacy && in.map == 0 && op == 0xE8;
    size = in.opsize && !call ? 2 : 4;
    in.relative = true;
  }
  if (size != 0) {
    in.imm_at = static_cast<std::uint8_t>(c.pos());
    in.imm_size = size;
    c.skip(size);
  }
  if (legacy && in.map == 0 && op == 0xC8) {  // ENTER imm16, imm8
    in.imm2_at = static_cast<std::uint8_t>(c.pos());
    in.imm2_size = 1;
    c.skip(1);
  }
  if (legacy && in.map == 1 && in.opcode == 0x78 && (flags & I8) != 0) {  // EXTRQ, INSERTQ imm8, imm8
    in.imm2_at = static_cast<std::uint8_t>(c.pos());
    in.imm2_size = 1;
    c.skip(1);
  }

  if (c.pos() > kMaxLength) return Unexpected{DecodeProblem::TooLong};
  if (c.pos() > code.size()) return Unexpected{DecodeProblem::Truncated};
  in.length = static_cast<std::uint8_t>(c.pos());

  // ---- Flow ----
  if (legacy && in.map == 0) {
    if (op == 0xE8) {
      in.flow = Flow::Call;
    } else if (op == 0xE9 || op == 0xEB) {
      in.flow = Flow::Jump;
    } else if ((op >= 0x70 && op <= 0x7F) || (op >= 0xE0 && op <= 0xE3) || (op == 0xC7 && in.modrm == 0xF8)) {
      in.flow = Flow::Branch;
    } else if (op == 0xC2 || op == 0xC3 || op == 0xCA || op == 0xCB || op == 0xCF) {
      in.flow = Flow::Return;
    } else if (op == 0xCC || op == 0xF4) {
      in.flow = Flow::Trap;
    } else if (op == 0xFF && in.has_modrm) {
      const std::uint8_t reg = (in.modrm >> 3) & 7;
      if (reg == 2 || reg == 3) in.flow = Flow::CallIndirect;
      if (reg == 4 || reg == 5) in.flow = Flow::JumpIndirect;
    }
  } else if (legacy && in.map == 1) {
    if (in.opcode >= 0x80 && in.opcode <= 0x8F) in.flow = Flow::Branch;
    if (in.opcode == 0x0B) in.flow = Flow::Trap;
  }
  return in;
}

std::optional<std::int64_t> displacement(const Instruction& insn, std::span<const std::byte> code) noexcept {
  return field(code, insn.disp_at, insn.disp_size);
}

std::optional<std::int64_t> immediate(const Instruction& insn, std::span<const std::byte> code) noexcept {
  return field(code, insn.imm_at, insn.imm_size);
}

std::optional<std::int64_t> immediate2(const Instruction& insn, std::span<const std::byte> code) noexcept {
  return field(code, insn.imm2_at, insn.imm2_size);
}

namespace {

std::optional<std::uint64_t> relative_to_next(std::uint64_t at, std::uint8_t length, std::int64_t disp) noexcept {
  const std::uint64_t next = at + length;
  if (next < at) return std::nullopt;
  if (disp >= 0) {
    const auto d = static_cast<std::uint64_t>(disp);
    if (d > ~std::uint64_t{0} - next) return std::nullopt;
    return next + d;
  }
  const std::uint64_t d = ~static_cast<std::uint64_t>(disp) + 1;
  if (d > next) return std::nullopt;
  return next - d;
}

}  // namespace

std::optional<std::uint64_t> rip_target(const Instruction& insn, std::span<const std::byte> code,
                                        std::uint64_t at) noexcept {
  if (!insn.rip_relative) return std::nullopt;
  const std::optional<std::int64_t> disp = displacement(insn, code);
  if (!disp) return std::nullopt;
  return relative_to_next(at, insn.length, *disp);
}

std::optional<std::uint64_t> branch_target(const Instruction& insn, std::span<const std::byte> code,
                                           std::uint64_t at) noexcept {
  if (!insn.relative) return std::nullopt;
  const std::optional<std::int64_t> rel = immediate(insn, code);
  if (!rel) return std::nullopt;
  return relative_to_next(at, insn.length, *rel);
}

}  // namespace en::gal::x86
