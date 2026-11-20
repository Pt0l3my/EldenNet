// The callee's summary (probe.hpp's summarise_callee; EN_PROBE only; WHY§227): what register_task's code does with its
// arguments, read with en-gal's decoder from the live image, following no call. Our own code over the architecture's
// documented encoding; nothing here knows the game.
#include "en/gal/probe.hpp"

#if EN_PROBE

#include <algorithm>

#include "en/gal/x86.hpp"

namespace en::gal::probe {

namespace {

using x86::Flow;
using x86::Instruction;

constexpr std::size_t kMaxCalls = 32, kMaxBranches = 32, kMaxStores = 8, kMaxCompares = 8;
constexpr std::int64_t kImmBound = 65536;
constexpr std::array<std::uint8_t, 4> kArgs = {1, 2, 8, 9};  // rcx, rdx, r8, r9
constexpr std::uint8_t kRax = 0, kRdx = 2;

constexpr bool callee_saved(std::uint8_t r) noexcept { return r == 3 || r == 5 || r == 6 || r == 7 || r >= 12; }

// What a register holds, as far as the summary follows it: an argument's value, or the first word of the object an
// argument points at (its vtable, for a virtual call).
struct Held {
  enum class Kind : std::uint8_t { None, Value, Deref };
  Kind kind = Kind::None;
  std::uint8_t arg = 0;  // into kArgs
};

// The memory operand of an instruction with ModRM (mod != 3).
struct Mem {
  std::optional<std::uint8_t> base;
  std::optional<std::uint8_t> index;
  bool rip = false;
  std::int64_t disp = 0;
};

Mem memory_operand(const Instruction& in, std::span<const std::byte> bytes) noexcept {
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

// The operand size of an instruction whose byte form is `byte_form`.
std::uint8_t width_of(const Instruction& in, bool byte_form) noexcept {
  if (byte_form) return 1;
  if (in.wide) return 8;
  return in.opsize ? 2 : 4;
}

// Map 1 (0F) and VEX map 1 instructions that read or write vector registers only, and at most read a general one or
// a memory operand's base: moves, logic, arithmetic and compares of packed and scalar values, and emms or vzeroupper.
// None writes a general register (movmskps, pmovmskb, pextrw, movd to a register and the conversions to an integer
// are left out).
constexpr bool vector_only(std::uint8_t op) noexcept {
  return (op >= 0x10 && op <= 0x17) || op == 0x28 || op == 0x29 || op == 0x2A || op == 0x2B || op == 0x2E ||
         op == 0x2F || (op >= 0x51 && op <= 0x6F) || (op >= 0x70 && op <= 0x77) || op == 0x7F || op == 0xC2 ||
         op == 0xC6 || op == 0xD4 || op == 0xD6 || op == 0xDB || op == 0xDF || op == 0xE7 || op == 0xEB ||
         op == 0xEF || (op >= 0xFA && op <= 0xFE);
}

class Summariser {
 public:
  Summariser(std::uint32_t rva, CalleeCode& out) : rva_(rva), out_(out) {
    out_.args.resize(kArgs.size());
    for (std::size_t i = 0; i < kArgs.size(); ++i) {
      out_.args[i].reg = kArgs[i];
      regs_[kArgs[i]] = Held{Held::Kind::Value, static_cast<std::uint8_t>(i)};
    }
  }

  void step(std::uint32_t at, const Instruction& in, std::span<const std::byte> bytes) {
    at_ = at;
    in_ = &in;
    bytes_ = bytes;
    if (in.flow == Flow::Branch && !called_) {
      ++out_.branch_count;
      if (out_.branches.size() < kMaxBranches) {
        const auto to = x86::branch_target(in, bytes, std::uint64_t{rva_} + at);
        out_.branches.push_back(
            CalleeBranch{at, to ? static_cast<std::int64_t>(*to) - static_cast<std::int64_t>(rva_) : 0});
      }
    }
    if (in.flow == Flow::Return && !out_.first_ret) out_.first_ret = at;
    if (in.flow == Flow::Call || in.flow == Flow::CallIndirect) return call();
    if (!tracking_) return;
    if (!model()) {
      if (!called_ && !out_.modelled_to) out_.modelled_to = at;
      tracking_ = false;
      regs_.fill(Held{});
    }
  }

 private:
  // ---- The registers ----
  [[nodiscard]] std::optional<std::uint8_t> value(std::uint8_t r) const noexcept {
    const Held& h = regs_[r & 15];
    if (h.kind != Held::Kind::Value) return std::nullopt;
    return h.arg;
  }
  void kill(std::uint8_t r) noexcept { regs_[r & 15] = Held{}; }
  // dst := src, `width` bytes: a full copy keeps what src holds, a 32-bit one only an argument's value.
  void copy(std::uint8_t dst, std::uint8_t src, std::uint8_t width) {
    Held h = regs_[src & 15];
    if (width < 4 || (width == 4 && h.kind != Held::Kind::Value)) h = Held{};
    regs_[dst & 15] = h;
    if (h.kind == Held::Kind::Value && !called_ && callee_saved(dst & 15)) {
      auto& saved = out_.args[h.arg].saved;
      if (std::find(saved.begin(), saved.end(), dst & 15) == saved.end()) saved.push_back(dst & 15);
    }
  }

  // ---- What is kept before the first call ----
  void store(std::uint8_t reg, std::uint8_t width) {
    const auto arg = value(reg);
    if (!arg || called_) return;
    auto& stores = out_.args[*arg].stores;
    if (stores.size() >= kMaxStores) return;
    const Mem m = memory_operand(*in_, bytes_);
    ArgStore s;
    s.offset = at_;
    s.via = reg & 15;
    s.base = m.base;
    if (m.base) {
      if (const auto held = value(*m.base)) s.base_arg = kArgs[*held];
    }
    s.index = m.index;
    s.rip = m.rip;
    s.disp = m.disp;
    if (m.rip) {
      const auto slot = x86::rip_target(*in_, bytes_, std::uint64_t{rva_} + at_);
      s.disp = slot ? static_cast<std::int64_t>(*slot) : 0;
    }
    s.width = width;
    stores.push_back(s);
  }
  void compare_imm(std::uint8_t reg) {
    const auto arg = value(reg);
    if (!arg || called_) return;
    auto& compares = out_.args[*arg].compares;
    if (compares.size() >= kMaxCompares) return;
    const auto imm = x86::immediate(*in_, bytes_);
    ArgCompare c{at_, static_cast<std::uint8_t>(reg & 15), std::nullopt};
    if (imm && *imm >= 0 && *imm <= kImmBound) c.imm = static_cast<std::uint64_t>(*imm);
    compares.push_back(c);
  }
  void compare_other(std::uint8_t reg) {
    if (const auto arg = value(reg); arg && !called_) ++out_.args[*arg].compared_other;
  }
  void tested(std::uint8_t reg) {
    if (const auto arg = value(reg); arg && !called_) ++out_.args[*arg].tested;
  }

  // ---- The calls ----
  void call() {
    const Instruction& in = *in_;
    ++out_.call_count;
    if (!out_.first_call) out_.first_call = at_;
    if (out_.calls.size() < kMaxCalls) {
      CalleeCall c;
      c.offset = at_;
      if (in.flow == Flow::Call) {
        c.form = CallForm::Direct;
        const auto to = x86::branch_target(in, bytes_, std::uint64_t{rva_} + at_);
        if (to && *to <= 0xFFFF'FFFFull) c.target = static_cast<std::uint32_t>(*to);
      } else if (in.mod() == 3) {
        c.form = CallForm::Register;
        c.base = in.rm();
      } else {
        const Mem m = memory_operand(in, bytes_);
        if (m.rip) {
          c.form = CallForm::Rip;
          const auto slot = x86::rip_target(in, bytes_, std::uint64_t{rva_} + at_);
          if (slot && *slot <= 0xFFFF'FFFFull) c.target = static_cast<std::uint32_t>(*slot);
        } else {
          c.form = CallForm::Memory;
          c.base = m.base;
          c.index = m.index;
          c.disp = m.disp;
          if (m.base && tracking_ && regs_[*m.base].kind == Held::Kind::Deref) c.vtable_of = kArgs[regs_[*m.base].arg];
        }
      }
      out_.calls.push_back(c);
    }
    called_ = true;
    // The volatile registers are the callee's to change; the callee-saved ones keep what they held.
    for (const std::uint8_t r : {0, 1, 2, 8, 9, 10, 11}) kill(static_cast<std::uint8_t>(r));
  }

  // ---- One instruction's effect on the registers: false when it is not one the summary models ----
  bool model() {
    const Instruction& in = *in_;
    if (in.encoding != x86::Encoding::Legacy) return in.encoding == x86::Encoding::Vex && in.map == 1 && vector_only(in.opcode);
    if (in.map == 1) return model_0f();
    if (in.map != 0) return false;
    const std::uint8_t op = in.opcode;
    const bool reg_form = in.has_modrm && in.mod() == 3;
    const std::uint8_t sub = static_cast<std::uint8_t>((in.modrm >> 3) & 7);  // a group's operation
    if (op < 0x40 && (op & 7) < 6) {  // add, or, adc, sbb, and, sub, xor, cmp
      const bool cmp = (op >> 3) == 7;
      switch (op & 7) {
        case 0: case 1:  // r/m, r
          if (cmp) {
            compare_other(in.reg());
            if (reg_form) compare_other(in.rm());
          } else if (reg_form) {
            kill(in.rm());
          }
          return true;
        case 2: case 3:  // r, r/m
          if (cmp) {
            compare_other(in.reg());
            if (reg_form) compare_other(in.rm());
          } else {
            kill(in.reg());
          }
          return true;
        default:  // al or eax, imm
          if (cmp) {
            compare_imm(kRax);
          } else {
            kill(kRax);
          }
          return true;
      }
    }
    if (op >= 0x50 && op <= 0x57) return true;  // push
    if (op >= 0x58 && op <= 0x5F) {  // pop
      kill(in.opcode_reg());
      return true;
    }
    if (op >= 0x70 && op <= 0x7F) return true;  // Jcc
    switch (op) {
      case 0x63: case 0x69: case 0x6B: case 0x8A: case 0x8D:  // movsxd, imul, mov r8, lea: write reg
        kill(in.reg());
        return true;
      case 0x68: case 0x6A: case 0xE9: case 0xEB: case 0xC2: case 0xC3: case 0xCC:  // push imm, jmp, ret, int3
        return true;
      case 0x80: case 0x81: case 0x83:  // group 1, imm
        if (sub == 7) {
          if (reg_form) compare_imm(in.rm());
        } else if (reg_form) {
          kill(in.rm());
        }
        return true;
      case 0x84: case 0x85:  // test r/m, r
        tested(in.reg());
        if (reg_form && in.rm() != in.reg()) tested(in.rm());
        return true;
      case 0x86: case 0x87:  // xchg
        if (reg_form && op == 0x87 && width_of(in, false) == 8) {
          std::swap(regs_[in.reg()], regs_[in.rm()]);
        } else if (reg_form) {
          kill(in.reg());
          kill(in.rm());
        } else {
          kill(in.reg());
        }
        return true;
      case 0x88: case 0x89:  // mov r/m, r
        if (reg_form) {
          copy(in.rm(), in.reg(), width_of(in, op == 0x88));
        } else {
          store(in.reg(), width_of(in, op == 0x88));
        }
        return true;
      case 0x8B:  // mov r, r/m
        if (reg_form) {
          copy(in.reg(), in.rm(), width_of(in, false));
        } else {
          load(in.reg(), width_of(in, false));
        }
        return true;
      case 0x8F:  // pop r/m
        if (reg_form) kill(in.rm());
        return true;
      case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
        if (op == 0x90 && (in.rex & 0x1) == 0) return true;  // nop (and pause)
        if (in.wide) {
          std::swap(regs_[kRax], regs_[in.opcode_reg()]);  // xchg rax, r
        } else {
          kill(kRax);
          kill(in.opcode_reg());
        }
        return true;
      case 0x98: kill(kRax); return true;  // cdqe
      case 0x99: kill(kRdx); return true;  // cqo
      case 0xA8: case 0xA9: tested(kRax); return true;  // test al or eax, imm
      case 0xC0: case 0xC1: case 0xD0: case 0xD1: case 0xD2: case 0xD3:  // shifts and rotates
        if (reg_form) kill(in.rm());
        return true;
      case 0xC6: case 0xC7:  // mov r/m, imm
        if (sub != 0) return false;
        if (reg_form) kill(in.rm());
        return true;
      case 0xF6: case 0xF7:  // group 3
        if (sub <= 1) {
          if (reg_form) tested(in.rm());
        } else if (sub <= 3) {
          if (reg_form) kill(in.rm());
        } else {  // mul, imul, div, idiv: rdx:rax
          kill(kRax);
          kill(kRdx);
        }
        return true;
      case 0xFE:  // inc, dec r/m8
        if (sub > 1) return false;
        if (reg_form) kill(in.rm());
        return true;
      case 0xFF:  // inc, dec, (calls and jumps), push
        if (sub <= 1) {
          if (reg_form) kill(in.rm());
          return true;
        }
        return sub != 7;
      default:
        break;
    }
    if (op >= 0xB0 && op <= 0xBF) {  // mov r, imm
      kill(in.opcode_reg());
      return true;
    }
    return false;
  }

  bool model_0f() {
    const Instruction& in = *in_;
    const std::uint8_t op = in.opcode;
    const bool reg_form = in.has_modrm && in.mod() == 3;
    const std::uint8_t sub = static_cast<std::uint8_t>((in.modrm >> 3) & 7);
    if (vector_only(op)) return true;  // with or without 66, F2 or F3: movss, movsd, movdqu and the like
    if (op >= 0x80 && op <= 0x8F) return true;  // Jcc
    if (op >= 0x40 && op <= 0x4F) {  // cmovcc
      kill(in.reg());
      return true;
    }
    if (op >= 0x90 && op <= 0x9F) {  // setcc
      if (reg_form) kill(in.rm());
      return true;
    }
    switch (op) {
      case 0x0B: case 0x1F: case 0x18: case 0x0D: case 0xA3:  // ud2, nop, prefetches, bt r/m, r
        return true;
      case 0xAF: case 0xB6: case 0xB7: case 0xBE: case 0xBF:  // imul, movzx, movsx
        kill(in.reg());
        return true;
      case 0xBA:  // bt, bts, btr, btc r/m, imm
        if (sub < 4) return false;
        if (sub > 4 && reg_form) kill(in.rm());
        return true;
      case 0xB1:  // cmpxchg
        kill(kRax);
        if (reg_form) kill(in.rm());
        return true;
      case 0xC1:  // xadd
        kill(in.reg());
        if (reg_form) kill(in.rm());
        return true;
      default:
        return false;
    }
  }

  // reg := [mem]: the first word of an argument's object when the address is [argument + 0], 64-bit; else nothing.
  void load(std::uint8_t reg, std::uint8_t width) {
    const Mem m = memory_operand(*in_, bytes_);
    Held h;
    if (width == 8 && m.base && !m.index && !m.rip && m.disp == 0) {
      if (const auto arg = value(*m.base)) h = Held{Held::Kind::Deref, *arg};
    }
    regs_[reg & 15] = h;
  }

  std::uint32_t rva_;
  CalleeCode& out_;
  std::array<Held, 16> regs_{};
  bool called_ = false;
  bool tracking_ = true;
  std::uint32_t at_ = 0;
  const Instruction* in_ = nullptr;
  std::span<const std::byte> bytes_;
};

struct Decoded {
  std::uint32_t at = 0;
  Instruction insn;
};

// Whether a return is reached from the entry on a path through no call: conditional branches both ways, jumps within
// the decoded code followed, a jump out of it or through a register ending the path as a call would.
bool returns_before_call(const std::vector<Decoded>& code, std::span<const std::byte> bytes, std::uint32_t rva) {
  if (code.empty()) return false;
  const auto index_of = [&](std::uint64_t off) -> std::optional<std::size_t> {
    const auto it = std::lower_bound(code.begin(), code.end(), off,
                                     [](const Decoded& d, std::uint64_t o) { return d.at < o; });
    if (it == code.end() || it->at != off) return std::nullopt;
    return static_cast<std::size_t>(it - code.begin());
  };
  std::vector<std::uint8_t> seen(code.size(), 0);
  std::vector<std::size_t> todo{0};
  while (!todo.empty()) {
    const std::size_t i = todo.back();
    todo.pop_back();
    if (seen[i] != 0) continue;
    seen[i] = 1;
    const Decoded& d = code[i];
    const auto next = [&] {
      if (i + 1 < code.size() && code[i + 1].at == d.at + d.insn.length) todo.push_back(i + 1);
    };
    const auto target = [&] {
      const auto to = x86::branch_target(d.insn, bytes.subspan(d.at), std::uint64_t{rva} + d.at);
      if (to && *to >= rva) {
        if (const auto j = index_of(*to - rva)) todo.push_back(*j);
      }
    };
    switch (d.insn.flow) {
      case Flow::Return: return true;
      case Flow::Call: case Flow::CallIndirect: case Flow::JumpIndirect: case Flow::Trap: break;
      case Flow::Jump: target(); break;
      case Flow::Branch: target(); next(); break;
      case Flow::None: next(); break;
    }
  }
  return false;
}

}  // namespace

CalleeCode summarise_callee(std::span<const std::byte> code, std::uint32_t rva, std::uint32_t extent) {
  CalleeCode out;
  out.read = !code.empty();
  out.extent = extent;
  const std::size_t want = extent != 0 ? std::min<std::size_t>(extent, kCalleeBound) : kCalleeBound;
  const std::size_t limit = std::min(want, code.size());
  Summariser s(rva, out);
  std::vector<Decoded> decoded;
  std::size_t at = 0;
  bool ended = false;  // the function's end, or with no extent its first return
  while (at < limit) {
    const auto insn = x86::decode(code.subspan(at, std::min<std::size_t>(x86::kMaxLength, limit - at)));
    if (!insn) {
      // An instruction that runs past the 4 KiB bound is the bound's; anything else does not decode.
      const bool bound = insn.error() == x86::DecodeProblem::Truncated && limit == kCalleeBound &&
                         limit - at < x86::kMaxLength;
      if (!bound) out.undecodable = true;
      break;
    }
    const auto here = static_cast<std::uint32_t>(at);
    decoded.push_back(Decoded{here, *insn});
    ++out.instructions;
    s.step(here, *insn, code.subspan(at, insn->length));
    at += insn->length;
    if (extent == 0 && insn->flow == Flow::Return) {  // no function's end known: up to the first return
      ended = true;
      break;
    }
  }
  if (extent != 0 && at >= extent) ended = true;
  out.cut = !ended && !out.undecodable && limit == kCalleeBound;
  out.bytes = static_cast<std::uint32_t>(at);
  out.returns_early = returns_before_call(decoded, code.first(at), rva);
  return out;
}

}  // namespace en::gal::probe

#endif
