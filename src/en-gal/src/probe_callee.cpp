// The callee's summary (probe.hpp's summarise_callee; EN_PROBE only; WHY§227, WHY§235, WHY§240): what register_task's
// code does with its arguments, read with en-gal's decoder from the live image, following no call; since spike.7 what
// each register holds is known along the flow, mapped into every call, and each branch's flag test is read; since
// spike.8 the values loaded from memory are followed too (Source), jumps through memory or a register are tail call
// sites, and the instructions are given as forms. Our own code over the architecture's documented encoding; nothing
// here knows the game.
#include "en/gal/probe.hpp"

#if EN_PROBE

#include <algorithm>

#include "en/gal/x86.hpp"

namespace en::gal::probe {

namespace {

using x86::Flow;
using x86::Instruction;

constexpr std::size_t kMaxCalls = 32, kMaxBranches = 32, kMaxStores = 8, kMaxCompares = 8, kMaxGuards = 8;
constexpr std::size_t kMaxTransparent = 4;  // moves and loads passed over to find a branch's flag test
constexpr std::uint64_t kImmBound = 65536;
constexpr std::uint8_t kMaxLevels = 3;     // loads followed into a source
constexpr std::size_t kMaxSources = 1024;  // sources in one summary; past it a load holds nothing
constexpr std::array<std::uint8_t, 4> kArgs = {1, 2, 8, 9};  // rcx, rdx, r8, r9
constexpr std::uint8_t kRax = 0, kRdx = 2;

constexpr bool callee_saved(std::uint8_t r) noexcept { return r == 3 || r == 5 || r == 6 || r == 7 || r >= 12; }

std::optional<std::uint8_t> arg_index(std::uint8_t reg) noexcept {
  for (std::size_t i = 0; i < kArgs.size(); ++i) {
    if (kArgs[i] == reg) return static_cast<std::uint8_t>(i);
  }
  return std::nullopt;
}

// What a register holds, as far as the summary follows it: an argument's value, or the first word of the object an
// argument points at (its vtable, for a virtual call); since spike.8 also the value an inner summary was entered with in
// rcx to r9 (Entry), a value loaded from memory (Loaded, a source of the summary's table), or a call's result.
struct Held {
  enum class Kind : std::uint8_t { None, Value, Deref, Entry, Loaded, Result };
  Kind kind = Kind::None;
  std::uint8_t arg = 0;   // Value, Deref, Entry: into kArgs
  std::uint16_t id = 0;   // Loaded: the source's index; Result: the call's offset
  friend bool operator==(const Held&, const Held&) = default;
};
using Regs = std::array<Held, 16>;

[[nodiscard]] constexpr bool is_argument(const Held& h) noexcept {
  return h.kind == Held::Kind::Value || h.kind == Held::Kind::Deref;
}

// A load's source (spike.8): a RIP-relative slot, or [base + index*scale + disp] with what base and index held. Two
// loads make the same source when they read the same place by the same values: where a base or an index holds nothing
// known, the load's own offset tells it apart from every other.
struct Src {
  bool rip = false;
  std::uint32_t target = 0;  // rip: the slot's RVA
  std::optional<std::uint8_t> base_reg, index_reg;
  Held base, index;
  std::uint8_t scale = 1;
  std::int64_t disp = 0;
  std::uint8_t width = 8;
  std::uint32_t at = 0;      // the first load that made it, by offset
  std::uint8_t level = 1;
};

class Sources {
 public:
  [[nodiscard]] std::uint8_t level(const Held& h) const noexcept {
    switch (h.kind) {
      case Held::Kind::Deref: return 1;
      case Held::Kind::Loaded: return list_[h.id].level;
      default: return 0;
    }
  }
  // The source a load makes, or nothing past kMaxLevels loads deep or past the table's size.
  std::optional<std::uint16_t> intern(Src s) {
    s.level = s.rip ? 1 : static_cast<std::uint8_t>(1 + std::max(level(s.base), level(s.index)));
    if (s.level > kMaxLevels) return std::nullopt;
    const bool unknown = (s.base_reg && s.base.kind == Held::Kind::None) ||
                         (s.index_reg && s.index.kind == Held::Kind::None);
    for (std::size_t i = 0; i < list_.size(); ++i) {
      Src& o = list_[i];
      const bool same = o.rip == s.rip && o.width == s.width &&
                        (s.rip ? o.target == s.target
                               : o.base_reg.has_value() == s.base_reg.has_value() &&
                                     o.index_reg.has_value() == s.index_reg.has_value() && o.base == s.base &&
                                     o.index == s.index && o.scale == s.scale && o.disp == s.disp &&
                                     (!unknown || o.at == s.at));
      if (same) {
        o.at = std::min(o.at, s.at);
        return static_cast<std::uint16_t>(i);
      }
    }
    if (list_.size() >= kMaxSources) return std::nullopt;
    list_.push_back(s);
    return static_cast<std::uint16_t>(list_.size() - 1);
  }
  // What a register holding `h` holds, as the report describes it.
  [[nodiscard]] Source describe(const Held& h) const {
    Source o;
    switch (h.kind) {
      case Held::Kind::Value: o.kind = Source::Kind::Argument; o.from = kArgs[h.arg]; return o;
      case Held::Kind::Deref: o.kind = Source::Kind::FirstWord; o.from = kArgs[h.arg]; return o;
      case Held::Kind::Entry: o.kind = Source::Kind::Entry; o.from = kArgs[h.arg]; return o;
      case Held::Kind::Loaded: break;
      default: return o;
    }
    const Src& s = list_[h.id];
    o.at = s.at;
    o.width = s.width;
    if (s.rip) {
      o.kind = Source::Kind::Rip;
      o.target = s.target;
      return o;
    }
    o.kind = Source::Kind::Memory;
    o.base = s.base_reg;
    o.index = s.index_reg;
    o.scale = s.scale;
    o.disp = s.disp;
    if (s.base_reg) o.base_src.push_back(describe(s.base));
    if (s.index_reg) o.index_src.push_back(describe(s.index));
    return o;
  }
  [[nodiscard]] const Src& at(std::uint16_t id) const noexcept { return list_[id]; }

 private:
  std::vector<Src> list_;
};

// What the registers hold before an instruction, along the flow: `reached` once a known path from the entry leads there.
struct State {
  bool reached = false;
  Regs regs{};
};

// `in` met at `into`: a register keeps what it holds only where both paths agree. Answers whether `into` changed.
bool merge(State& into, const State& in) noexcept {
  if (!in.reached) return false;
  if (!into.reached) {
    into = in;
    return true;
  }
  bool changed = false;
  for (std::size_t r = 0; r < into.regs.size(); ++r) {
    if (into.regs[r].kind != Held::Kind::None && !(into.regs[r] == in.regs[r])) {
      into.regs[r] = Held{};
      changed = true;
    }
  }
  return changed;
}

// The memory operand of an instruction with ModRM (mod != 3).
struct Mem {
  std::optional<std::uint8_t> base;
  std::optional<std::uint8_t> index;
  std::uint8_t scale = 1;
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
    if (idx != 4) {
      m.index = idx;
      m.scale = static_cast<std::uint8_t>(1u << (in.sib >> 6));
    }
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

// The 64-bit register a byte register's number names: without REX, 4 to 7 are ah, ch, dh and bh, the second bytes of
// rax, rcx, rdx and rbx.
constexpr std::uint8_t byte_reg(std::uint8_t r, const Instruction& in) noexcept {
  return in.rex == 0 && r >= 4 && r <= 7 ? static_cast<std::uint8_t>(r - 4) : r;
}

// The first immediate as an operand of `width` bytes reads it (unsigned), kept only from 0 to 65,536.
std::optional<std::uint64_t> small_immediate(const Instruction& in, std::span<const std::byte> bytes,
                                             std::uint8_t width) noexcept {
  const auto imm = x86::immediate(in, bytes);
  if (!imm) return std::nullopt;
  auto v = static_cast<std::uint64_t>(*imm);
  if (width < 8) v &= (std::uint64_t{1} << (8u * width)) - 1;
  if (v > kImmBound) return std::nullopt;
  return v;
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

// An instruction that writes no flag, passed over to find the one that set a branch's: moves, loads of addresses and of
// immediates, pushes and pops, nops, conditional moves, and the vector moves.
bool flags_untouched(const Instruction& in) noexcept {
  if (in.encoding == x86::Encoding::Vex) {
    return in.map == 1 && (in.opcode == 0x10 || in.opcode == 0x11 || in.opcode == 0x28 || in.opcode == 0x29 ||
                           in.opcode == 0x6F || in.opcode == 0x7F);
  }
  if (in.encoding != x86::Encoding::Legacy) return false;
  const std::uint8_t op = in.opcode;
  if (in.map == 0) {
    const std::uint8_t sub = static_cast<std::uint8_t>((in.modrm >> 3) & 7);
    return (op >= 0x88 && op <= 0x8B) || op == 0x8D || op == 0x63 || (op >= 0x50 && op <= 0x5F) ||
           (op >= 0xB0 && op <= 0xBF) || ((op == 0xC6 || op == 0xC7) && sub == 0) || (op == 0x90 && in.rex == 0);
  }
  if (in.map == 1) {
    return op == 0xB6 || op == 0xB7 || op == 0xBE || op == 0xBF || op == 0x1F || (op >= 0x40 && op <= 0x4F) ||
           op == 0x10 || op == 0x11 || op == 0x28 || op == 0x29 || op == 0x6F || op == 0x7F || op == 0xD6;
  }
  return false;
}

class Summariser {
 public:
  // `end`: the function's length, when the exception directory gives it (a jump past it is a tail jump), else 0.
  Summariser(std::uint32_t rva, std::uint32_t end, CalleeCode& out, Sources& sources)
      : rva_(rva), end_(end), out_(out), sources_(sources) {
    out_.args.resize(kArgs.size());
    for (std::size_t i = 0; i < kArgs.size(); ++i) out_.args[i].reg = kArgs[i];
  }

  // One instruction's effect on `regs`, nothing recorded (the passes over the flow).
  void apply(Regs& regs, std::uint32_t at, const Instruction& in, std::span<const std::byte> bytes) {
    begin(at, in, bytes, false, true);
    regs_ = regs;
    effect();
    regs = regs_;
  }

  // The same with what the summary keeps recorded, in address order, from the registers the flow gave the instruction.
  void record(const State& s, std::uint32_t at, const Instruction& in, std::span<const std::byte> bytes) {
    begin(at, in, bytes, true, s.reached);
    regs_ = s.regs;
    if (in.flow == Flow::Branch && !called_) {
      ++out_.branch_count;
      if (out_.branches.size() < kMaxBranches) {
        const auto to = x86::branch_target(in, bytes, std::uint64_t{rva_} + at);
        out_.branches.push_back(
            CalleeBranch{at, to ? static_cast<std::int64_t>(*to) - static_cast<std::int64_t>(rva_) : 0});
      }
    }
    if (in.flow == Flow::Return && !out_.first_ret) out_.first_ret = at;
    effect();
  }

  [[nodiscard]] bool called() const noexcept { return called_; }

 private:
  void begin(std::uint32_t at, const Instruction& in, std::span<const std::byte> bytes, bool recording, bool reached) {
    at_ = at;
    in_ = &in;
    bytes_ = bytes;
    recording_ = recording;
    reached_ = reached;
  }

  void effect() {
    const Instruction& in = *in_;
    if (in.flow == Flow::Call || in.flow == Flow::CallIndirect) return call();
    if (in.flow == Flow::Jump) {
      if (recording_) tail();
      return;
    }
    if (in.flow == Flow::JumpIndirect) {  // spike.8: through memory or a register, out of the function
      if (recording_) tail_indirect();
      return;
    }
    if (!model()) {
      if (keep() && !out_.modelled_to) out_.modelled_to = at_;
      regs_.fill(Held{});  // nothing is guessed past it on this path
    }
  }

  // Whether what this instruction does is kept: in the recording pass, before the first call.
  [[nodiscard]] bool keep() const noexcept { return recording_ && !called_; }

  // ---- The registers ----
  [[nodiscard]] std::optional<std::uint8_t> value(std::uint8_t r) const noexcept {
    const Held& h = regs_[r & 15];
    if (h.kind != Held::Kind::Value) return std::nullopt;
    return h.arg;
  }
  void kill(std::uint8_t r) noexcept { regs_[r & 15] = Held{}; }
  // dst := src, `width` bytes: a full copy keeps what src holds, a 32-bit one only an argument's value or a value
  // loaded 32 bits wide or less.
  void copy(std::uint8_t dst, std::uint8_t src, std::uint8_t width) {
    Held h = regs_[src & 15];
    const bool narrow = h.kind == Held::Kind::Value ||
                        (h.kind == Held::Kind::Loaded && sources_.at(h.id).width <= 4);
    if (width < 4 || (width == 4 && !narrow)) h = Held{};
    regs_[dst & 15] = h;
    if (h.kind == Held::Kind::Value && keep() && callee_saved(dst & 15)) {
      auto& saved = out_.args[h.arg].saved;
      if (std::find(saved.begin(), saved.end(), dst & 15) == saved.end()) saved.push_back(dst & 15);
    }
  }
  [[nodiscard]] std::vector<PassedArg> passed() const {
    std::vector<PassedArg> p;
    for (const std::uint8_t r : kArgs) {
      const Held& h = regs_[r];
      if (is_argument(h)) p.push_back(PassedArg{r, kArgs[h.arg], h.kind == Held::Kind::Deref});
    }
    return p;
  }
  // rcx, rdx, r8 and r9 holding a known value that is no argument (spike.8).
  [[nodiscard]] std::vector<PassedSource> passed_sources() const {
    std::vector<PassedSource> p;
    for (const std::uint8_t r : kArgs) {
      const Held& h = regs_[r];
      if (h.kind == Held::Kind::Entry || h.kind == Held::Kind::Loaded) p.push_back(PassedSource{r, sources_.describe(h)});
    }
    return p;
  }
  // A call's or jump's base register: what it holds when that is no argument, and the object whose first word it holds
  // when that object's value is known (the argument's, as `vtable_of`; or a value of a source, as `vtable_src`).
  void base_of(CalleeCall& c, std::uint8_t base) const {
    const Held& h = regs_[base & 15];
    if (h.kind == Held::Kind::Deref) c.vtable_of = kArgs[h.arg];
    if (h.kind == Held::Kind::Loaded || h.kind == Held::Kind::Entry) c.base_src = sources_.describe(h);
    if (h.kind == Held::Kind::Loaded) {
      const Src& s = sources_.at(h.id);
      if (!s.rip && s.width == 8 && s.disp == 0 && s.base_reg && !s.index_reg && s.base.kind != Held::Kind::None) {
        c.vtable_src = sources_.describe(s.base);
      }
    }
  }

  // ---- What is kept before the first call ----
  void store(std::uint8_t reg, std::uint8_t width) {
    forward(reg, width);
    const Held& h = regs_[reg & 15];
    if (!is_argument(h) || !keep()) return;
    auto& stores = out_.args[h.arg].stores;
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
    s.first_word = h.kind == Held::Kind::Deref;
    stores.push_back(s);
  }
  // A 64-bit store to a RIP-relative slot of a register holding a call's result, or nothing known: it, and every register
  // holding the same result, now hold the slot's value (a lazily made singleton stored to its global, spike.8).
  void forward(std::uint8_t reg, std::uint8_t width) {
    const Held h = regs_[reg & 15];
    if (width != 8 || !in_->rip_relative || (h.kind != Held::Kind::None && h.kind != Held::Kind::Result)) return;
    const auto slot = x86::rip_target(*in_, bytes_, std::uint64_t{rva_} + at_);
    if (!slot || *slot > 0xFFFF'FFFFull) return;
    Src s;
    s.rip = true;
    s.target = static_cast<std::uint32_t>(*slot);
    s.at = at_;
    const auto id = sources_.intern(s);
    if (!id) return;
    const Held loaded{Held::Kind::Loaded, 0, *id};
    if (h.kind == Held::Kind::Result) {
      for (Held& r : regs_) {
        if (r == h) r = loaded;
      }
    }
    regs_[reg & 15] = loaded;
  }

  void compare_imm(std::uint8_t reg, std::uint8_t width) {
    const auto arg = value(reg);
    if (!arg || !keep()) return;
    auto& compares = out_.args[*arg].compares;
    if (compares.size() >= kMaxCompares) return;
    compares.push_back(ArgCompare{at_, static_cast<std::uint8_t>(reg & 15), small_immediate(*in_, bytes_, width)});
  }
  void compare_other(std::uint8_t reg) {
    if (const auto arg = value(reg); arg && keep()) ++out_.args[*arg].compared_other;
  }
  void tested(std::uint8_t reg) {
    if (const auto arg = value(reg); arg && keep()) ++out_.args[*arg].tested;
  }

  // ---- The calls, and the jumps out of the function ----
  void call() {
    const Instruction& in = *in_;
    if (recording_) {
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
            if (m.base) base_of(c, *m.base);
          }
        }
        if (c.form == CallForm::Register && c.base) base_of(c, *c.base);
        c.reached = reached_;
        c.pass = passed();
        c.pass_src = passed_sources();
        out_.calls.push_back(std::move(c));
      }
      called_ = true;
    }
    // The volatile registers are the callee's to change; the callee-saved ones keep what they held. rax holds its result.
    for (const std::uint8_t r : {0, 1, 2, 8, 9, 10, 11}) kill(static_cast<std::uint8_t>(r));
    regs_[kRax] = Held{Held::Kind::Result, 0, static_cast<std::uint16_t>(at_)};
  }

  // A direct jump out of the function: before its entry, or past its end where the exception directory gives one.
  void tail() {
    const auto to = x86::branch_target(*in_, bytes_, std::uint64_t{rva_} + at_);
    if (!to || *to > 0xFFFF'FFFFull) return;
    const bool out = *to < rva_ || (end_ != 0 && *to >= std::uint64_t{rva_} + end_);
    if (!out || out_.calls.size() >= kMaxCalls) return;
    CalleeCall c;
    c.offset = at_;
    c.form = CallForm::Tail;
    c.target = static_cast<std::uint32_t>(*to);
    c.reached = reached_;
    c.pass = passed();
    c.pass_src = passed_sources();
    out_.calls.push_back(std::move(c));
  }

  // A jump through a register or memory (spike.8): it leaves the function, a tail call through a pointer.
  void tail_indirect() {
    const Instruction& in = *in_;
    if (out_.calls.size() >= kMaxCalls) return;
    CalleeCall c;
    c.offset = at_;
    if (in.mod() == 3) {
      c.form = CallForm::TailRegister;
      c.base = in.rm();
      base_of(c, in.rm());
    } else {
      const Mem m = memory_operand(in, bytes_);
      c.form = CallForm::TailMemory;
      if (m.rip) {
        c.rip = true;
        const auto slot = x86::rip_target(in, bytes_, std::uint64_t{rva_} + at_);
        if (slot && *slot <= 0xFFFF'FFFFull) c.target = static_cast<std::uint32_t>(*slot);
      } else {
        c.base = m.base;
        c.index = m.index;
        c.disp = m.disp;
        if (m.base) base_of(c, *m.base);
      }
    }
    c.reached = reached_;
    c.pass = passed();
    c.pass_src = passed_sources();
    out_.calls.push_back(std::move(c));
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
    // A byte form's registers name ah, ch, dh and bh too.
    const auto r8 = [&](bool byte_form, std::uint8_t r) { return byte_form ? byte_reg(r, in) : r; };
    if (op < 0x40 && (op & 7) < 6) {  // add, or, adc, sbb, and, sub, xor, cmp
      const bool cmp = (op >> 3) == 7;
      const bool byte_form = (op & 1) == 0;
      switch (op & 7) {
        case 0: case 1:  // r/m, r
          if (cmp) {
            compare_other(r8(byte_form, in.reg()));
            if (reg_form) compare_other(r8(byte_form, in.rm()));
          } else if (reg_form) {
            kill(r8(byte_form, in.rm()));
          }
          return true;
        case 2: case 3:  // r, r/m
          if (cmp) {
            compare_other(r8(byte_form, in.reg()));
            if (reg_form) compare_other(r8(byte_form, in.rm()));
          } else {
            kill(r8(byte_form, in.reg()));
          }
          return true;
        default:  // al or eax, imm
          if (cmp) {
            compare_imm(kRax, width_of(in, (op & 7) == 4));
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
      case 0x63:  // movsxd: a 32-bit value's copy (an index), or a load
        if (reg_form) {
          const Held h = regs_[in.rm()];
          regs_[in.reg()] = h.kind == Held::Kind::Value ? h : Held{};
        } else {
          load(in.reg(), 4);
        }
        return true;
      case 0x69: case 0x6B: case 0x8D:  // imul, lea: write reg
        kill(in.reg());
        return true;
      case 0x8A:  // mov r8, r/m8
        kill(byte_reg(in.reg(), in));
        return true;
      case 0x68: case 0x6A: case 0xE9: case 0xEB: case 0xC2: case 0xC3: case 0xCC:  // push imm, jmp, ret, int3
        return true;
      case 0x80: case 0x81: case 0x83:  // group 1, imm
        if (sub == 7) {
          if (reg_form) compare_imm(r8(op == 0x80, in.rm()), width_of(in, op == 0x80));
        } else if (reg_form) {
          kill(r8(op == 0x80, in.rm()));
        }
        return true;
      case 0x84: case 0x85:  // test r/m, r
        tested(r8(op == 0x84, in.reg()));
        if (reg_form && r8(op == 0x84, in.rm()) != r8(op == 0x84, in.reg())) tested(r8(op == 0x84, in.rm()));
        return true;
      case 0x86: case 0x87:  // xchg
        if (reg_form && op == 0x87 && width_of(in, false) == 8) {
          std::swap(regs_[in.reg()], regs_[in.rm()]);
        } else if (reg_form) {
          kill(r8(op == 0x86, in.reg()));
          kill(r8(op == 0x86, in.rm()));
        } else {
          kill(r8(op == 0x86, in.reg()));
        }
        return true;
      case 0x88: case 0x89:  // mov r/m, r
        if (reg_form) {
          copy(r8(op == 0x88, in.rm()), r8(op == 0x88, in.reg()), width_of(in, op == 0x88));
        } else {
          store(r8(op == 0x88, in.reg()), width_of(in, op == 0x88));
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
        if (reg_form) kill(r8((op & 1) == 0, in.rm()));
        return true;
      case 0xC6: case 0xC7:  // mov r/m, imm
        if (sub != 0) return false;
        if (reg_form) kill(r8(op == 0xC6, in.rm()));
        return true;
      case 0xF6: case 0xF7:  // group 3
        if (sub <= 1) {
          if (reg_form) tested(r8(op == 0xF6, in.rm()));
        } else if (sub <= 3) {
          if (reg_form) kill(r8(op == 0xF6, in.rm()));
        } else {  // mul, imul, div, idiv: rdx:rax
          kill(kRax);
          kill(kRdx);
        }
        return true;
      case 0xFE:  // inc, dec r/m8
        if (sub > 1) return false;
        if (reg_form) kill(byte_reg(in.rm(), in));
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
    if (op >= 0xB0 && op <= 0xB7) {  // mov r8, imm8
      kill(byte_reg(in.opcode_reg(), in));
      return true;
    }
    if (op >= 0xB8 && op <= 0xBF) {  // mov r, imm
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
      if (reg_form) kill(byte_reg(in.rm(), in));
      return true;
    }
    switch (op) {
      case 0x0B: case 0x1F: case 0x18: case 0x0D: case 0xA3:  // ud2, nop, prefetches, bt r/m, r
        return true;
      case 0xAF:  // imul
        kill(in.reg());
        return true;
      case 0xB6: case 0xB7: case 0xBE: case 0xBF:  // movzx, movsx: a load of a byte or a word, or a register's
        if (reg_form) {
          kill(in.reg());
        } else {
          load(in.reg(), (op & 1) == 0 ? 1 : 2, true);
        }
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

  // reg := [mem]: the first word of an argument's object when the address is [argument + 0], 64-bit; since spike.8
  // otherwise the value of the load's source (a 64- or 32-bit load, or a byte's or word's extended), else nothing: a
  // 16- or 8-bit load writes part of the register, a segment's or an absolute address's is not followed.
  void load(std::uint8_t reg, std::uint8_t width, bool extended = false) {
    const Mem m = memory_operand(*in_, bytes_);
    Held h;
    if (width == 8 && m.base && !m.index && !m.rip && m.disp == 0) {
      if (const auto arg = value(*m.base)) h = Held{Held::Kind::Deref, *arg};
    }
    if (h.kind == Held::Kind::None && (width >= 4 || extended) && in_->segment != 0x64 && in_->segment != 0x65 &&
        !in_->absolute && (m.rip || m.base || m.index)) {
      Src s;
      s.width = width;
      s.at = at_;
      bool ok = true;
      if (m.rip) {
        s.rip = true;
        const auto slot = x86::rip_target(*in_, bytes_, std::uint64_t{rva_} + at_);
        ok = slot && *slot <= 0xFFFF'FFFFull;
        if (ok) s.target = static_cast<std::uint32_t>(*slot);
      } else {
        s.base_reg = m.base;
        s.index_reg = m.index;
        if (m.base) s.base = regs_[*m.base & 15];
        if (m.index) s.index = regs_[*m.index & 15];
        if (s.base.kind == Held::Kind::Result) s.base = Held{};  // a call's result is known by no other name
        if (s.index.kind == Held::Kind::Result) s.index = Held{};
        s.scale = m.scale;
        s.disp = m.disp;
      }
      if (ok) {
        if (const auto id = sources_.intern(s)) h = Held{Held::Kind::Loaded, 0, *id};
      }
    }
    regs_[reg & 15] = h;
  }

  std::uint32_t rva_;
  std::uint32_t end_;
  CalleeCode& out_;
  Sources& sources_;
  Regs regs_{};
  bool recording_ = false;
  bool reached_ = false;
  bool called_ = false;
  std::uint32_t at_ = 0;
  const Instruction* in_ = nullptr;
  std::span<const std::byte> bytes_;
};

struct Decoded {
  std::uint32_t at = 0;
  Instruction insn;
};

std::optional<std::size_t> index_of(const std::vector<Decoded>& code, std::uint64_t off) {
  const auto it = std::lower_bound(code.begin(), code.end(), off,
                                   [](const Decoded& d, std::uint64_t o) { return d.at < o; });
  if (it == code.end() || it->at != off) return std::nullopt;
  return static_cast<std::size_t>(it - code.begin());
}

// Where the flow goes after instruction `i`: the next one when it falls through, a relative branch's or jump's target
// within the decoded code. A return, a trap, a jump through a register or memory and a jump out of the code end it.
template <class Fn>
void successors(const std::vector<Decoded>& code, std::span<const std::byte> bytes, std::uint32_t rva, std::size_t i,
                Fn&& fn) {
  const Decoded& d = code[i];
  const auto next = [&] {
    if (i + 1 < code.size() && code[i + 1].at == d.at + d.insn.length) fn(i + 1);
  };
  const auto target = [&] {
    const auto to = x86::branch_target(d.insn, bytes.subspan(d.at), std::uint64_t{rva} + d.at);
    if (to && *to >= rva) {
      if (const auto j = index_of(code, *to - rva)) fn(*j);
    }
  };
  switch (d.insn.flow) {
    case Flow::None: case Flow::Call: case Flow::CallIndirect: next(); break;
    case Flow::Branch: target(); next(); break;
    case Flow::Jump: target(); break;
    case Flow::Return: case Flow::JumpIndirect: case Flow::Trap: break;
  }
}

// Whether a return is reached from the entry on a path through no call: conditional branches both ways, jumps within
// the decoded code followed, a jump out of it or through a register ending the path as a call would.
bool returns_before_call(const std::vector<Decoded>& code, std::span<const std::byte> bytes, std::uint32_t rva) {
  if (code.empty()) return false;
  std::vector<std::uint8_t> seen(code.size(), 0);
  std::vector<std::size_t> todo{0};
  while (!todo.empty()) {
    const std::size_t i = todo.back();
    todo.pop_back();
    if (seen[i] != 0) continue;
    seen[i] = 1;
    const Flow f = code[i].insn.flow;
    if (f == Flow::Return) return true;
    if (f == Flow::Call || f == Flow::CallIndirect) continue;
    successors(code, bytes, rva, i, [&](std::size_t j) { todo.push_back(j); });
  }
  return false;
}

// ---- A branch's flag test ----

// A register operand, with the argument it holds; with `sources` (a guard's), where its value came from when it holds
// none (spike.8).
Operand register_operand(std::uint8_t r, const Regs& regs, const Sources* sources = nullptr) {
  Operand o;
  o.form = OperandForm::Register;
  o.reg = r;
  const Held& h = regs[r & 15];
  if (is_argument(h)) {
    o.from = kArgs[h.arg];
    o.first_word = h.kind == Held::Kind::Deref;
  } else if (sources != nullptr) {
    o.source = sources->describe(h);
  }
  return o;
}

// A memory operand: its base and the argument it holds, its index and scale, its displacement; a RIP-relative one's
// slot. With `sources`, what the base holds when it is no argument.
Operand rm_operand(const Instruction& in, std::span<const std::byte> bytes, std::uint64_t at, bool byte_form,
                   const Regs& regs, const Sources* sources = nullptr) {
  if (in.mod() == 3) {
    Operand o = register_operand(byte_form ? byte_reg(in.rm(), in) : in.rm(), regs, sources);
    o.high = byte_form && in.rex == 0 && in.rm() >= 4 && in.rm() <= 7;
    return o;
  }
  const Mem m = memory_operand(in, bytes);
  Operand o;
  if (m.rip) {
    o.form = OperandForm::Rip;
    const auto slot = x86::rip_target(in, bytes, at);
    if (slot && *slot <= 0xFFFF'FFFFull) o.target = static_cast<std::uint32_t>(*slot);
    return o;
  }
  o.form = OperandForm::Memory;
  o.base = m.base;
  if (m.base && regs[*m.base].kind == Held::Kind::Value) o.base_arg = kArgs[regs[*m.base].arg];
  if (m.base && sources != nullptr && regs[*m.base].kind != Held::Kind::Value) {
    o.base_src = sources->describe(regs[*m.base]);
  }
  o.index = m.index;
  if (m.index) o.scale = m.scale;
  o.disp = m.disp;
  return o;
}

Operand immediate_operand(const Instruction& in, std::span<const std::byte> bytes, std::uint8_t width) {
  Operand o;
  o.form = OperandForm::Immediate;
  o.imm = small_immediate(in, bytes, width);
  return o;
}

// The compare or test at `s` (its registers as the flow gives them), its operands as the instruction names them, each
// register holding no argument with its source (spike.8).
void read_flag_test(CalleeGuard& g, const Instruction& in, std::span<const std::byte> bytes, std::uint64_t at,
                    const Regs& regs, const Sources& sources) {
  g.op = GuardOp::Other;
  if (in.encoding != x86::Encoding::Legacy || in.map != 0) return;
  const std::uint8_t op = in.opcode;
  const std::uint8_t sub = static_cast<std::uint8_t>((in.modrm >> 3) & 7);
  const auto reg_of = [&](bool byte_form) {
    Operand o = register_operand(byte_form ? byte_reg(in.reg(), in) : in.reg(), regs, &sources);
    o.high = byte_form && in.rex == 0 && in.reg() >= 4 && in.reg() <= 7;
    return o;
  };
  const auto rm_of = [&](bool byte_form) { return rm_operand(in, bytes, at, byte_form, regs, &sources); };
  const auto rax = [&] { return register_operand(kRax, regs, &sources); };
  if (op >= 0x38 && op <= 0x3D) {  // cmp
    g.op = GuardOp::Compare;
    const bool byte_form = (op & 1) == 0;
    g.width = width_of(in, byte_form);
    if (op <= 0x39) {
      g.operands = {rm_of(byte_form), reg_of(byte_form)};
    } else if (op <= 0x3B) {
      g.operands = {reg_of(byte_form), rm_of(byte_form)};
    } else {
      g.operands = {rax(), immediate_operand(in, bytes, g.width)};
    }
  } else if ((op == 0x80 || op == 0x81 || op == 0x83) && sub == 7) {
    g.op = GuardOp::Compare;
    g.width = width_of(in, op == 0x80);
    g.operands = {rm_of(op == 0x80), immediate_operand(in, bytes, g.width)};
  } else if (op == 0x84 || op == 0x85) {
    g.op = GuardOp::Test;
    g.width = width_of(in, op == 0x84);
    g.operands = {rm_of(op == 0x84), reg_of(op == 0x84)};
  } else if (op == 0xA8 || op == 0xA9) {
    g.op = GuardOp::Test;
    g.width = width_of(in, op == 0xA8);
    g.operands = {rax(), immediate_operand(in, bytes, g.width)};
  } else if ((op == 0xF6 || op == 0xF7) && sub <= 1) {
    g.op = GuardOp::Test;
    g.width = width_of(in, op == 0xF6);
    g.operands = {rm_of(op == 0xF6), immediate_operand(in, bytes, g.width)};
  }
}

CalleeGuard guard_of(const std::vector<Decoded>& code, const std::vector<State>& states, std::size_t i,
                     std::span<const std::byte> bytes, std::uint32_t rva, const Sources& sources) {
  const Decoded& d = code[i];
  CalleeGuard g;
  g.offset = d.at;
  const auto to = x86::branch_target(d.insn, bytes.subspan(d.at), std::uint64_t{rva} + d.at);
  g.to = to ? static_cast<std::int64_t>(*to) - static_cast<std::int64_t>(rva) : 0;
  const std::uint8_t op = d.insn.opcode;
  if (d.insn.encoding == x86::Encoding::Legacy &&
      ((d.insn.map == 0 && op >= 0x70 && op <= 0x7F) || (d.insn.map == 1 && op >= 0x80 && op <= 0x8F))) {
    g.cond = static_cast<std::uint8_t>(op & 0xF);
  }
  // The nearest instruction before it that writes flags, past at most a few that write none, in straight-line code.
  for (std::size_t k = i, passed = 0; k > 0 && passed <= kMaxTransparent; ++passed) {
    const Decoded& prev = code[k - 1];
    if (prev.at + prev.insn.length != code[k].at) break;
    --k;
    if (flags_untouched(prev.insn)) continue;
    g.op_at = prev.at;
    const Regs none{};
    read_flag_test(g, prev.insn, bytes.subspan(prev.at, prev.insn.length), std::uint64_t{rva} + prev.at,
                   states[k].reached ? states[k].regs : none, sources);
    break;
  }
  return g;
}

// ---- The forms (spike.8): each instruction's class and its operands' kinds, no bytes ----

Operand form_register(std::uint8_t r, std::uint8_t width, const Instruction& in, bool byte_form, const Regs& regs) {
  Operand o = register_operand(byte_form ? byte_reg(r, in) : r, regs);
  o.width = width;
  o.high = byte_form && in.rex == 0 && r >= 4 && r <= 7;
  return o;
}

// The r/m operand: a register, or memory of `width` bytes (0 for lea, which reads none).
Operand form_rm(const Instruction& in, std::span<const std::byte> bytes, std::uint64_t at, bool byte_form,
                std::uint8_t width, const Regs& regs) {
  if (in.mod() == 3) return form_register(in.rm(), width, in, byte_form, regs);
  Operand o = rm_operand(in, bytes, at, byte_form, regs);
  o.width = width;
  return o;
}

Operand form_immediate(const Instruction& in, std::span<const std::byte> bytes, std::uint8_t width) {
  Operand o = immediate_operand(in, bytes, width);
  o.large = !o.imm.has_value() && x86::immediate(in, bytes).has_value();
  return o;
}

Operand form_constant(std::uint64_t v) {
  Operand o;
  o.form = OperandForm::Immediate;
  o.imm = v;
  return o;
}

void fill_form(Form& f, const Instruction& in, std::span<const std::byte> b, std::uint64_t at, const Regs& regs);

Form form_of(const Decoded& d, std::span<const std::byte> code, std::uint32_t rva, const State& st) {
  using C = InsnClass;
  const Instruction& in = d.insn;
  const std::span<const std::byte> b = code.subspan(d.at, in.length);
  const std::uint64_t at = std::uint64_t{rva} + d.at;
  Form f;
  f.offset = d.at;
  f.length = in.length;
  f.unreached = !st.reached;
  const Regs none{};
  const Regs& regs = st.reached ? st.regs : none;
  if (in.relative) {
    if (const auto to = x86::branch_target(in, b, at)) {
      f.to = static_cast<std::int64_t>(*to) - static_cast<std::int64_t>(rva);
    }
  }
  if (in.encoding != x86::Encoding::Legacy) return f;
  fill_form(f, in, b, at, regs);
  // A register the instruction only writes holds nothing of interest before it: what it held is not shown.
  if ((f.cls == C::Mov || f.cls == C::MovExt || f.cls == C::Lea || f.cls == C::Pop) && !f.operands.empty() &&
      f.operands[0].form == OperandForm::Register) {
    f.operands[0].from.reset();
    f.operands[0].first_word = false;
  }
  return f;
}

void fill_form(Form& f, const Instruction& in, std::span<const std::byte> b, std::uint64_t at, const Regs& regs) {
  using C = InsnClass;
  const std::uint8_t op = in.opcode;
  const std::uint8_t sub = static_cast<std::uint8_t>((in.modrm >> 3) & 7);
  const std::uint8_t w = width_of(in, false);
  const auto reg = [&](bool byte_form) {
    return form_register(in.reg(), width_of(in, byte_form), in, byte_form, regs);
  };
  const auto rm = [&](bool byte_form, std::uint8_t width) { return form_rm(in, b, at, byte_form, width, regs); };
  const auto imm = [&](std::uint8_t width) { return form_immediate(in, b, width); };
  if (in.map == 1) {
    if (op == 0x1F) {
      f.cls = C::Nop;
    } else if (op >= 0x80 && op <= 0x8F) {
      f.cls = C::Jcc;
      f.cond = static_cast<std::uint8_t>(op & 0xF);
    } else if (op == 0xAF) {
      f.cls = C::Imul;
      f.operands = {reg(false), rm(false, w)};
    } else if (op == 0xB6 || op == 0xB7 || op == 0xBE || op == 0xBF) {
      f.cls = C::MovExt;
      const bool byte_src = (op & 1) == 0;
      f.operands = {reg(false), rm(byte_src, byte_src ? 1 : 2)};
    }
    return;
  }
  if (in.map != 0) return;
  if (op < 0x40 && (op & 7) < 6) {  // add, or, adc, sbb, and, sub, xor, cmp
    static constexpr std::array<C, 8> kArith = {C::Add, C::Or, C::Other, C::Other, C::And, C::Sub, C::Xor, C::Cmp};
    f.cls = kArith[op >> 3];
    const bool byte_form = (op & 1) == 0;
    const std::uint8_t width = width_of(in, byte_form);
    if ((op & 7) <= 1) {
      f.operands = {rm(byte_form, width), reg(byte_form)};
    } else if ((op & 7) <= 3) {
      f.operands = {reg(byte_form), rm(byte_form, width)};
    } else {
      f.operands = {form_register(kRax, width, in, byte_form, regs), imm(width)};
    }
    return;
  }
  if (op >= 0x50 && op <= 0x57) {
    f.cls = C::Push;
    f.operands = {form_register(in.opcode_reg(), 8, in, false, regs)};
  } else if (op >= 0x58 && op <= 0x5F) {
    f.cls = C::Pop;
    f.operands = {form_register(in.opcode_reg(), 8, in, false, regs)};
  } else if (op == 0x63) {
    f.cls = C::MovExt;
    f.operands = {reg(false), rm(false, 4)};
  } else if (op == 0x68 || op == 0x6A) {
    f.cls = C::Push;
    f.operands = {imm(8)};
  } else if (op == 0x69 || op == 0x6B) {
    f.cls = C::Imul;
    f.operands = {reg(false), rm(false, w), imm(w)};
  } else if (op >= 0x70 && op <= 0x7F) {
    f.cls = C::Jcc;
    f.cond = static_cast<std::uint8_t>(op & 0xF);
  } else if (op == 0x80 || op == 0x81 || op == 0x83) {
    static constexpr std::array<C, 8> kGroup = {C::Add, C::Or, C::Other, C::Other, C::And, C::Sub, C::Xor, C::Cmp};
    f.cls = kGroup[sub];
    const std::uint8_t width = width_of(in, op == 0x80);
    f.operands = {rm(op == 0x80, width), imm(width)};
  } else if (op == 0x84 || op == 0x85) {
    f.cls = C::Test;
    f.operands = {rm(op == 0x84, width_of(in, op == 0x84)), reg(op == 0x84)};
  } else if (op == 0x88 || op == 0x89) {
    f.cls = C::Mov;
    f.operands = {rm(op == 0x88, width_of(in, op == 0x88)), reg(op == 0x88)};
  } else if (op == 0x8A || op == 0x8B) {
    f.cls = C::Mov;
    f.operands = {reg(op == 0x8A), rm(op == 0x8A, width_of(in, op == 0x8A))};
  } else if (op == 0x8D) {
    f.cls = C::Lea;
    f.operands = {reg(false), rm(false, 0)};
  } else if (op == 0x8F && sub == 0) {
    f.cls = C::Pop;
    f.operands = {rm(false, 8)};
  } else if (op == 0x90 && (in.rex & 0x1) == 0) {
    f.cls = C::Nop;
  } else if (op == 0xA8 || op == 0xA9) {
    f.cls = C::Test;
    const std::uint8_t width = width_of(in, op == 0xA8);
    f.operands = {form_register(kRax, width, in, op == 0xA8, regs), imm(width)};
  } else if (op >= 0xB0 && op <= 0xB7) {
    f.cls = C::Mov;
    f.operands = {form_register(in.opcode_reg(), 1, in, true, regs), imm(1)};
  } else if (op >= 0xB8 && op <= 0xBF) {
    f.cls = C::Mov;
    f.operands = {form_register(in.opcode_reg(), w, in, false, regs), imm(w)};
  } else if (op == 0xC0 || op == 0xC1 || (op >= 0xD0 && op <= 0xD3)) {
    if (sub == 4 || sub == 5 || sub == 6 || sub == 7) {  // shl, shr, sal, sar
      f.cls = C::Shift;
      const bool byte_form = (op & 1) == 0;
      const std::uint8_t width = width_of(in, byte_form);
      Operand count = op <= 0xC1 ? imm(1) : op <= 0xD1 ? form_constant(1) : form_register(1, 1, in, true, regs);
      f.operands = {rm(byte_form, width), count};
    }
  } else if (op == 0xC2 || op == 0xC3) {
    f.cls = C::Ret;
    if (op == 0xC2) f.operands = {imm(2)};
  } else if ((op == 0xC6 || op == 0xC7) && sub == 0) {
    f.cls = C::Mov;
    const std::uint8_t width = width_of(in, op == 0xC6);
    f.operands = {rm(op == 0xC6, width), imm(width)};
  } else if (op == 0xCC) {
    f.cls = C::Int3;
  } else if (op == 0xE8) {
    f.cls = C::Call;
  } else if (op == 0xE9 || op == 0xEB) {
    f.cls = C::Jmp;
  } else if (op == 0xF6 || op == 0xF7) {
    const std::uint8_t width = width_of(in, op == 0xF6);
    if (sub <= 1) {
      f.cls = C::Test;
      f.operands = {rm(op == 0xF6, width), imm(width)};
    } else if (sub == 5) {
      f.cls = C::Imul;
      f.operands = {rm(op == 0xF6, width)};
    }
  } else if (op == 0xFF) {
    if (sub == 2 || sub == 3) {
      f.cls = C::Call;
      f.operands = {rm(false, 8)};
    } else if (sub == 4 || sub == 5) {
      f.cls = C::Jmp;
      f.operands = {rm(false, 8)};
    } else if (sub == 6) {
      f.cls = C::Push;
      f.operands = {rm(false, 8)};
    }
  }
}

}  // namespace

EntryState entry_at(const CalleeCall& call) noexcept {
  EntryState e{};
  for (const PassedArg& p : call.pass) {
    if (const auto i = arg_index(p.reg)) e[*i] = EntryArg{p.from, p.first_word};
  }
  return e;
}

CalleeCode summarise_callee(std::span<const std::byte> code, std::uint32_t rva, std::uint32_t extent,
                            const EntryState& entry) {
  CalleeCode out;
  out.read = !code.empty();
  out.extent = extent;
  const std::size_t want = extent != 0 ? std::min<std::size_t>(extent, kCalleeBound) : kCalleeBound;
  const std::size_t limit = std::min(want, code.size());
  std::vector<Decoded> decoded;
  std::size_t at = 0;
  bool ended = false;  // the function's end, or with no extent its first return
  std::uint64_t past = 0;  // with no extent: the furthest a branch or jump decoded so far goes
  while (at < limit) {
    const auto insn = x86::decode(code.subspan(at, std::min<std::size_t>(x86::kMaxLength, limit - at)));
    if (!insn) {
      // An instruction that runs past the 4 KiB bound is the bound's; anything else does not decode.
      const bool bound = insn.error() == x86::DecodeProblem::Truncated && limit == kCalleeBound &&
                         limit - at < x86::kMaxLength;
      if (!bound) out.undecodable = true;
      break;
    }
    decoded.push_back(Decoded{static_cast<std::uint32_t>(at), *insn});
    const std::size_t from = at;
    at += insn->length;
    if (extent != 0) continue;
    // No function's end known: up to its first return, or (spike.8) its first jump out of the flow, where no branch or
    // jump decoded so far goes past it (a tail jump's padding is the next function's, never its own).
    if (insn->flow == Flow::Branch || insn->flow == Flow::Jump) {
      const auto to = x86::branch_target(*insn, code.subspan(from, insn->length), std::uint64_t{rva} + from);
      if (to && *to >= rva) past = std::max<std::uint64_t>(past, *to - rva);
    }
    const bool leaves = insn->flow == Flow::Return || insn->flow == Flow::Jump || insn->flow == Flow::JumpIndirect;
    if (insn->flow == Flow::Return || (leaves && past < at)) {
      ended = true;
      break;
    }
  }
  if (extent != 0 && at >= extent) ended = true;
  out.cut = !ended && !out.undecodable && limit == kCalleeBound;
  out.bytes = static_cast<std::uint32_t>(at);
  out.instructions = static_cast<std::uint32_t>(decoded.size());
  const std::span<const std::byte> bytes = code.first(at);

  // What each instruction's registers hold along the flow, to a fixed point: a register only ever loses what it holds.
  Sources sources;
  Summariser s(rva, extent, out, sources);
  std::vector<State> states(decoded.size());
  if (!decoded.empty()) {
    states[0].reached = true;
    for (std::size_t i = 0; i < kArgs.size(); ++i) {
      if (const auto from = arg_index(entry[i].from)) {
        states[0].regs[kArgs[i]] = Held{entry[i].first_word ? Held::Kind::Deref : Held::Kind::Value, *from};
      } else {  // an inner summary's register that holds no argument: the value it was entered with (spike.8)
        states[0].regs[kArgs[i]] = Held{Held::Kind::Entry, static_cast<std::uint8_t>(i)};
      }
    }
    std::vector<std::size_t> todo{0};
    std::vector<std::uint8_t> queued(decoded.size(), 0);
    queued[0] = 1;
    while (!todo.empty()) {
      const std::size_t i = todo.back();
      todo.pop_back();
      queued[i] = 0;
      State after = states[i];
      const Decoded& d = decoded[i];
      s.apply(after.regs, d.at, d.insn, bytes.subspan(d.at, d.insn.length));
      successors(decoded, bytes, rva, i, [&](std::size_t j) {
        if (merge(states[j], after) && queued[j] == 0) {
          queued[j] = 1;
          todo.push_back(j);
        }
      });
    }
  }
  // What the summary keeps, in address order.
  for (std::size_t i = 0; i < decoded.size(); ++i) {
    const Decoded& d = decoded[i];
    if (d.insn.flow == Flow::Branch && !s.called() && out.guards.size() < kMaxGuards) {
      out.guards.push_back(guard_of(decoded, states, i, bytes, rva, sources));
    }
    if (out.forms.size() < kMaxForms) out.forms.push_back(form_of(d, bytes, rva, states[i]));
    State st = states[i];
    if (!st.reached) st.regs.fill(Held{});
    s.record(st, d.at, d.insn, bytes.subspan(d.at, d.insn.length));
  }
  out.returns_early = returns_before_call(decoded, bytes, rva);
  return out;
}

namespace {

bool same_list(const std::vector<Source>& a, const std::vector<Source>& b) noexcept {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!same_source(a[i], b[i])) return false;
  }
  return true;
}

SectionKind kind_at(std::span<const SectionSpan> sections, std::uint32_t rva) noexcept {
  if (!sections.empty() && rva < sections.front().rva) return SectionKind::Header;
  for (const SectionSpan& s : sections) {
    if (rva >= s.rva && rva - s.rva < s.size) return s.kind;
  }
  return SectionKind::Other;
}

void mark(std::vector<Operand>& operands, std::span<const SectionSpan> sections) noexcept {
  for (Operand& o : operands) {
    if (o.form == OperandForm::Rip && o.target) o.section = kind_at(sections, *o.target);
  }
}

// The value a wrapper's rcx holds at its tail jump, when it is run 7's: *S, or [*S + hop].
bool singleton_of(const Source& rcx, Table& t) noexcept {
  if (rcx.width != 8) return false;
  if (rcx.kind == Source::Kind::Rip) {
    t.singleton_slot = rcx.target;
    return true;
  }
  if (rcx.kind == Source::Kind::Memory && !rcx.index && rcx.base_src.size() == 1 &&
      rcx.base_src[0].kind == Source::Kind::Rip && rcx.base_src[0].width == 8) {
    t.singleton_slot = rcx.base_src[0].target;
    t.hop = rcx.disp;
    return true;
  }
  return false;
}

bool is_entry_rcx(const Source& s) noexcept { return s.kind == Source::Kind::Entry && s.from == 1; }

// The tail function's tested value, when it is run 7's: [entry rcx + group*8 + disp], or [[entry rcx + d0] + group*8 +
// disp].
bool entry_of(const Source& v, std::uint8_t group_reg, Table& t) noexcept {
  if (v.kind != Source::Kind::Memory || v.width != 8 || v.scale != 8 || v.index_src.size() != 1 ||
      v.index_src[0].kind != Source::Kind::Argument || v.index_src[0].from != group_reg || v.base_src.size() != 1) {
    return false;
  }
  const Source& b = v.base_src[0];
  if (is_entry_rcx(b)) {
    t.disp = v.disp;
    return true;
  }
  if (b.kind == Source::Kind::Memory && b.width == 8 && !b.index && b.base_src.size() == 1 &&
      is_entry_rcx(b.base_src[0])) {
    t.array = b.disp;
    t.disp = v.disp;
    return true;
  }
  return false;
}

}  // namespace

bool same_source(const Source& a, const Source& b) noexcept {
  return a.kind == b.kind && a.from == b.from && a.target == b.target && a.index.has_value() == b.index.has_value() &&
         a.base.has_value() == b.base.has_value() && a.scale == b.scale && a.disp == b.disp && a.width == b.width &&
         same_list(a.base_src, b.base_src) && same_list(a.index_src, b.index_src);
}

void mark_sections(CalleeCode& code, std::span<const SectionSpan> sections) noexcept {
  for (Form& f : code.forms) mark(f.operands, sections);
  for (CalleeGuard& g : code.guards) mark(g.operands, sections);
}

Table match_table(const CalleeCode& wrapper, std::span<const InnerCallee> inner, std::uint8_t group_reg) {
  Table t;
  const auto failed = [&](std::string_view word) {
    Table f;
    f.failed = word;
    return f;
  };
  const CalleeCall* jump = nullptr;
  const InnerCallee* tail = nullptr;
  for (const CalleeCall& c : wrapper.calls) {
    if (c.form != CallForm::Tail || !c.reached || !c.target) continue;
    for (const InnerCallee& i : inner) {
      if (i.tail && i.depth == 1 && i.site == c.offset && i.rva == *c.target) {
        jump = &c;
        tail = &i;
        break;
      }
    }
    if (jump != nullptr) break;
  }
  if (jump == nullptr) return failed("wrapper_tail");
  const auto rcx = std::find_if(jump->pass_src.begin(), jump->pass_src.end(),
                                [](const PassedSource& p) { return p.reg == 1; });
  if (rcx == jump->pass_src.end() || !singleton_of(rcx->source, t)) return failed("wrapper_rcx");
  t.rva = tail->rva;
  const CalleeCode& tc = tail->code;
  if (!tc.read || tc.instructions == 0) return failed("tail_summary");
  std::optional<std::uint64_t> bound;
  for (const CalleeGuard& g : tc.guards) {
    if (g.op == GuardOp::Compare && g.operands.size() == 2 && g.operands[0].form == OperandForm::Register &&
        g.operands[0].from == std::optional<std::uint8_t>(group_reg) && !g.operands[0].first_word &&
        g.operands[1].form == OperandForm::Immediate && g.operands[1].imm) {
      bound = g.operands[1].imm;
      break;
    }
  }
  if (!bound) return failed("tail_bound");
  t.bound = static_cast<std::uint32_t>(*bound);
  const Source* value = nullptr;
  for (const CalleeGuard& g : tc.guards) {
    if (g.op != GuardOp::Test || g.width != 8 || g.operands.size() != 2) continue;
    const Operand& a = g.operands[0];
    const Operand& b = g.operands[1];
    if (a.form == OperandForm::Register && b.form == OperandForm::Register && a.reg == b.reg && a.source &&
        entry_of(*a.source, group_reg, t)) {
      value = &*a.source;
      break;
    }
  }
  if (value == nullptr) return failed("tail_null_test");
  for (const CalleeCall& c : tc.calls) {
    if (c.form == CallForm::TailMemory && !c.rip && c.reached && c.vtable_src && same_source(*c.vtable_src, *value)) {
      t.slot_disp = c.disp;
      t.site = c.offset;
      break;
    }
  }
  t.matched = true;
  return t;
}

}  // namespace en::gal::probe

#endif
