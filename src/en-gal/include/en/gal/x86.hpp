#pragma once
// en-gal's x86-64 instruction-length decoder (ARCH §6.2; the derivation round, O89): our own, written from the
// architecture's documented encoding (legacy and REX prefixes, the one-, two- and three-byte opcode maps, ModRM, SIB,
// displacements and immediates, and the VEX, EVEX and XOP prefixes), for 64-bit mode only. It finds where each
// instruction ends and where its displacement and immediates lie, which is all the derivation needs to cut patterns: it
// names no mnemonic and models no operand beyond the ModRM fields. It reads only the bytes it is given, at most 15 of
// them, never throws and allocates nothing. Nothing here knows the game.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "en/proto/expected.hpp"

namespace en::gal::x86 {

inline constexpr std::size_t kMaxLength = 15;  // the architecture's limit on one instruction

enum class Encoding : std::uint8_t { Legacy, Vex, Evex, Xop };

// What an instruction does to the flow of control, as far as its encoding says.
enum class Flow : std::uint8_t {
  None,          // falls through
  Call,          // a call with a relative target (E8)
  Jump,          // an unconditional jump with a relative target (E9, EB)
  Branch,        // a conditional jump with a relative target (Jcc, LOOP, JRCXZ, XBEGIN)
  CallIndirect,  // a call through a register or memory (FF /2, FF /3)
  JumpIndirect,  // a jump through a register or memory (FF /4, FF /5)
  Return,        // RET, RETF, IRET
  Trap,          // INT3, UD2, HLT: execution does not go on to the next instruction
};

struct Instruction {
  std::uint8_t length = 0;  // 1 to 15
  Encoding encoding = Encoding::Legacy;
  std::uint8_t map = 0;  // 0 one-byte, 1 0F, 2 0F38, 3 0F3A; VEX's and EVEX's mmm; XOP's 8, 9 or 10; 15 for 3DNow!
  std::uint8_t opcode = 0;
  std::uint8_t opcode_at = 0;
  std::uint8_t prefixes = 0;  // legacy and REX prefix bytes before the opcode, or before VEX, EVEX or XOP
  std::uint8_t rex = 0;       // the REX byte in effect (0x40 to 0x4F), or 0
  bool opsize = false;        // 66
  bool adsize = false;        // 67
  bool lock = false;          // F0
  std::uint8_t rep = 0;       // the last of F2 and F3, or 0
  std::uint8_t segment = 0;   // the last segment override (26, 2E, 36, 3E, 64, 65), or 0
  bool wide = false;          // REX.W, or VEX's, EVEX's or XOP's W

  bool has_modrm = false;
  std::uint8_t modrm_at = 0;
  std::uint8_t modrm = 0;
  bool has_sib = false;
  std::uint8_t sib = 0;

  std::uint8_t disp_at = 0;  // the displacement's offset in the instruction, and its size (0, 1, 4 or 8)
  std::uint8_t disp_size = 0;
  bool rip_relative = false;  // the displacement is relative to the next instruction (ModRM mod 00, r/m 101)
  bool absolute = false;      // the displacement is an absolute address (A0 to A3's moffs)

  std::uint8_t imm_at = 0;  // the first immediate (or the branch displacement), and its size (0, 1, 2, 4 or 8)
  std::uint8_t imm_size = 0;
  std::uint8_t imm2_at = 0;  // a second immediate: ENTER's, EXTRQ's and INSERTQ's
  std::uint8_t imm2_size = 0;
  bool relative = false;  // the first immediate is a branch displacement, relative to the next instruction

  Flow flow = Flow::None;

  [[nodiscard]] constexpr std::uint8_t mod() const noexcept { return static_cast<std::uint8_t>(modrm >> 6); }
  // ModRM's reg and r/m fields widened by REX.R and REX.B (VEX's and EVEX's inverted R and B are not applied).
  [[nodiscard]] constexpr std::uint8_t reg() const noexcept {
    return static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((rex & 0x4) != 0 ? 8 : 0));
  }
  [[nodiscard]] constexpr std::uint8_t rm() const noexcept {
    return static_cast<std::uint8_t>((modrm & 7) | ((rex & 0x1) != 0 ? 8 : 0));
  }
  // The register an opcode such as B8+r or 50+r names in its low three bits, widened by REX.B.
  [[nodiscard]] constexpr std::uint8_t opcode_reg() const noexcept {
    return static_cast<std::uint8_t>((opcode & 7) | ((rex & 0x1) != 0 ? 8 : 0));
  }
};

enum class DecodeProblem : std::uint8_t {
  Truncated,  // the bytes end before the instruction does
  TooLong,    // longer than 15 bytes
  Invalid,    // an opcode or form that 64-bit mode does not define
};

[[nodiscard]] const char* to_string(DecodeProblem problem) noexcept;

// Decodes the instruction at the start of `code`, which may hold more bytes after it.
[[nodiscard]] proto::Expected<Instruction, DecodeProblem> decode(std::span<const std::byte> code) noexcept;

// The signed value of the displacement, of the first immediate and of the second, as the instruction holds them (each
// sign-extended from its size; an immediate of 8 bytes as it is). Nothing when there is none, or `code` is too short.
[[nodiscard]] std::optional<std::int64_t> displacement(const Instruction& insn,
                                                       std::span<const std::byte> code) noexcept;
[[nodiscard]] std::optional<std::int64_t> immediate(const Instruction& insn, std::span<const std::byte> code) noexcept;
[[nodiscard]] std::optional<std::int64_t> immediate2(const Instruction& insn, std::span<const std::byte> code) noexcept;

// Where a RIP-relative operand points, and where a relative branch goes, for the instruction at address `at`; nothing
// for an instruction without one, or past the 64-bit range.
[[nodiscard]] std::optional<std::uint64_t> rip_target(const Instruction& insn, std::span<const std::byte> code,
                                                      std::uint64_t at) noexcept;
[[nodiscard]] std::optional<std::uint64_t> branch_target(const Instruction& insn, std::span<const std::byte> code,
                                                         std::uint64_t at) noexcept;

}  // namespace en::gal::x86
