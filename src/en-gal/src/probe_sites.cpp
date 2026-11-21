// What the game's own call sites of the registration pass as the task (probe.hpp's task_argument; EN_PROBE only;
// WHY§289): the code of the function holding a call, decoded with en-gal's decoder from its first byte onto the call,
// read backwards from the call to the last write of the task's register along the straight line before it. Our own
// code over the architecture's documented encoding; nothing here knows the game.
#include "en/gal/probe.hpp"

#if EN_PROBE

#include <algorithm>

#include "en/gal/x86.hpp"

namespace en::gal::probe {

namespace {

using x86::Flow;
using x86::Instruction;

constexpr int kMaxCopies = 4;  // register-to-register moves followed back

struct Decoded {
  std::uint32_t at = 0;
  Instruction in;
};

constexpr bool volatile_reg(std::uint8_t r) noexcept { return r <= 2 || (r >= 8 && r <= 11); }

// The memory operand of an instruction with ModRM (mod != 3).
struct Mem {
  std::optional<std::uint8_t> base, index;
  bool rip = false;
  std::int64_t disp = 0;
};
Mem memory_of(const Instruction& in, std::span<const std::byte> bytes) noexcept {
  Mem m;
  m.disp = x86::displacement(in, bytes).value_or(0);
  if (in.rip_relative) {
    m.rip = true;
    return m;
  }
  if (in.has_sib) {
    const auto idx = static_cast<std::uint8_t>(((in.sib >> 3) & 7) | ((in.rex & 0x2) != 0 ? 8 : 0));
    if (idx != 4) m.index = idx;
    const auto low = static_cast<std::uint8_t>(in.sib & 7);
    if (!(low == 5 && in.mod() == 0)) m.base = static_cast<std::uint8_t>(low | ((in.rex & 0x1) != 0 ? 8 : 0));
  } else {
    m.base = in.rm();
  }
  return m;
}

// Whether an instruction may write `reg` (or a part of it): the forms the reading below names, and any other that
// might, so that nothing is taken for the register's value past it.
bool may_write(const Instruction& in, std::uint8_t reg) noexcept {
  const bool reg_form = in.has_modrm && in.mod() == 3;
  if (in.encoding != x86::Encoding::Legacy) return in.has_modrm && (in.reg() == reg || (reg_form && in.rm() == reg));
  const std::uint8_t op = in.opcode;
  const std::uint8_t sub = static_cast<std::uint8_t>((in.modrm >> 3) & 7);
  if (in.map == 0) {
    if (op < 0x40 && (op & 7) < 6) {  // the ALU's: r/m, r writes r/m (but cmp); r, r/m writes r (but cmp)
      if ((op >> 3) == 7) return false;
      return (op & 7) < 2 ? reg_form && in.rm() == reg : (op & 7) < 4 ? in.reg() == reg : reg == 0;
    }
    if (op >= 0x50 && op <= 0x57) return false;                       // push
    if (op >= 0x58 && op <= 0x5F) return in.opcode_reg() == reg;     // pop
    if (op >= 0xB0 && op <= 0xBF) return in.opcode_reg() == reg;                      // mov r, imm
    if (op >= 0x91 && op <= 0x97) return in.opcode_reg() == reg || reg == 0;         // xchg rax, r
    switch (op) {
      case 0x84: case 0x85: case 0xA8: case 0xA9: case 0x68: case 0x6A: case 0x90: return false;
      case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: return false;
      case 0x88: case 0x89: return reg_form && in.rm() == reg;
      case 0x8A: case 0x8B: case 0x8D: case 0x63: case 0x69: case 0x6B: return in.reg() == reg;
      case 0x86: case 0x87: return in.reg() == reg || (reg_form && in.rm() == reg);
      case 0x80: case 0x81: case 0x83: return sub != 7 && reg_form && in.rm() == reg;
      case 0xC6: case 0xC7: case 0xC0: case 0xC1: case 0xD0: case 0xD1: case 0xD2: case 0xD3: case 0x8F:
        return reg_form && in.rm() == reg;
      case 0xF6: case 0xF7: return sub <= 1 ? false : sub <= 3 ? reg_form && in.rm() == reg : reg == 0 || reg == 2;
      case 0xFE: case 0xFF: return sub <= 1 && reg_form && in.rm() == reg;
      case 0x98: return reg == 0;
      case 0x99: return reg == 2;
      default: break;
    }
    if (op >= 0xA4 && op <= 0xAF) return reg == 0 || reg == 1 || reg == 6 || reg == 7;  // string operations
    return in.has_modrm && (in.reg() == reg || (reg_form && in.rm() == reg));
  }
  if (in.map == 1) {
    if ((op >= 0x80 && op <= 0x8F) || op == 0x1F || op == 0x0D || op == 0x18) return false;  // jcc, nops, prefetches
    if (op == 0xA2 || op == 0x31 || op == 0x05) return reg <= 2;                             // cpuid, rdtsc, syscall
    return in.has_modrm && (in.reg() == reg || (reg_form && in.rm() == reg));
  }
  return in.has_modrm && (in.reg() == reg || (reg_form && in.rm() == reg));
}

// What a register holds before instruction `end`, read back along the straight line before it.
struct Found {
  ArgForm form = ArgForm::Other;
  std::optional<std::uint8_t> base;
  std::int64_t disp = 0;
  std::optional<std::uint32_t> target;
  bool rip = false;
  std::size_t writer = 0;  // the writing instruction's index (end: none)
};

Found trace(const std::vector<Decoded>& code, std::span<const std::byte> bytes, std::uint32_t rva, std::size_t end,
            std::uint8_t reg, int copies) {
  Found f;
  f.writer = end;
  for (std::size_t k = end; k-- > 0;) {
    const Instruction& in = code[k].in;
    const std::span<const std::byte> b = bytes.subspan(code[k].at, in.length);
    const std::uint64_t at = std::uint64_t{rva} + code[k].at;
    if (in.flow == Flow::Return || in.flow == Flow::Jump || in.flow == Flow::JumpIndirect) {
      return f;  // the line before it is not this path's: another path reaches what follows
    }
    if (in.flow == Flow::Call || in.flow == Flow::CallIndirect) {
      if (!volatile_reg(reg)) continue;
      f.writer = k;
      if (reg == 0) {
        f.form = ArgForm::Result;
        if (in.flow == Flow::Call) {
          if (const auto to = x86::branch_target(in, b, at); to && *to <= 0xFFFF'FFFFull) {
            f.target = static_cast<std::uint32_t>(*to);
          }
        }
      }
      return f;
    }
    if (!may_write(in, reg)) continue;
    f.writer = k;
    if (in.encoding != x86::Encoding::Legacy || in.map != 0) return f;
    const bool reg_form = in.has_modrm && in.mod() == 3;
    const std::uint8_t op = in.opcode;
    const auto copy_from = [&](std::uint8_t src) {
      return copies >= kMaxCopies || !in.wide ? f : trace(code, bytes, rva, k, src, copies + 1);
    };
    if (op == 0x8B && in.reg() == reg) {
      if (reg_form) return copy_from(in.rm());
      if (!in.wide) return f;
      const Mem m = memory_of(in, b);
      if (m.index) return f;
      f.form = ArgForm::Load;
      if (m.rip) {
        f.rip = true;
        if (const auto slot = x86::rip_target(in, b, at); slot && *slot <= 0xFFFF'FFFFull) {
          f.target = static_cast<std::uint32_t>(*slot);
        }
      } else {
        f.base = m.base;
        f.disp = m.disp;
      }
      return f;
    }
    if (op == 0x89 && reg_form && in.rm() == reg) return copy_from(in.reg());
    if (op == 0x8D && in.reg() == reg && in.wide) {
      const Mem m = memory_of(in, b);
      if (m.rip) {
        f.form = ArgForm::Static;
        f.rip = true;
        if (const auto slot = x86::rip_target(in, b, at); slot && *slot <= 0xFFFF'FFFFull) {
          f.target = static_cast<std::uint32_t>(*slot);
        }
      } else if (m.base && !m.index) {
        f.form = *m.base == 4 ? ArgForm::Stack : ArgForm::Member;
        f.base = m.base;
        f.disp = m.disp;
      }
      return f;
    }
    return f;
  }
  f.form = ArgForm::Argument;  // no write in the function before the call: what the function was entered with
  return f;
}

}  // namespace

SiteArg task_argument(std::span<const std::byte> code, std::uint32_t fn_rva, std::uint32_t call_rva,
                      std::uint8_t reg) {
  SiteArg out;
  out.site = call_rva;
  if (call_rva < fn_rva || call_rva - fn_rva >= code.size()) return out;
  const std::size_t want = call_rva - fn_rva;
  std::vector<Decoded> list;
  std::vector<std::uint64_t> targets;  // the branches' and jumps' targets, from the function's start
  std::size_t at = 0;
  while (at < want) {
    const auto in = x86::decode(code.subspan(at, std::min<std::size_t>(x86::kMaxLength, code.size() - at)));
    if (!in) return out;
    if (in->flow == Flow::Branch || in->flow == Flow::Jump) {
      const auto to = x86::branch_target(*in, code.subspan(at, in->length), std::uint64_t{fn_rva} + at);
      if (to && *to >= fn_rva) targets.push_back(*to - fn_rva);
    }
    list.push_back(Decoded{static_cast<std::uint32_t>(at), *in});
    at += in->length;
  }
  if (at != want) return out;
  const auto call = x86::decode(code.subspan(at, std::min<std::size_t>(x86::kMaxLength, code.size() - at)));
  if (!call || (call->flow != Flow::Call && call->flow != Flow::CallIndirect)) return out;
  out.decoded = true;
  const std::span<const std::byte> bytes = code.first(want + call->length);
  const Found f = trace(list, bytes, fn_rva, list.size(), reg, 0);
  out.form = f.form;
  out.base = f.base;
  out.disp = f.disp;
  out.target = f.target;
  out.rip = f.rip;
  if (f.writer < list.size()) {
    out.distance = static_cast<std::uint32_t>(list.size() - f.writer);
    const std::uint64_t from = list[f.writer].at;
    out.joined = std::any_of(targets.begin(), targets.end(), [&](std::uint64_t t) { return t > from && t <= want; });
  } else {
    out.distance = static_cast<std::uint32_t>(list.size());
  }
  // One level more: what the base holds, read back from its writer.
  if ((f.form == ArgForm::Member || f.form == ArgForm::Load) && f.base && f.writer < list.size()) {
    const Found b = trace(list, bytes, fn_rva, f.writer, *f.base, 0);
    out.base_form = *f.base == 4 ? ArgForm::Stack : b.form;
    if (*f.base != 4) {
      out.base_target = b.target;
      if (b.form == ArgForm::Member || b.form == ArgForm::Load) out.base_disp = b.disp;
    }
  }
  return out;
}

}  // namespace en::gal::probe

#endif
