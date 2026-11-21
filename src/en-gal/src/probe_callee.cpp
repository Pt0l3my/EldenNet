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
constexpr std::uint8_t kRax = 0, kRdx = 2, kRsp = 4;
constexpr std::size_t kMaxStackStores = 64;  // spike.11: stores to the stack frame kept, per summary

constexpr bool callee_saved(std::uint8_t r) noexcept { return r == 3 || r == 5 || r == 6 || r == 7 || r >= 12; }

std::optional<std::uint8_t> arg_index(std::uint8_t reg) noexcept {
  for (std::size_t i = 0; i < kArgs.size(); ++i) {
    if (kArgs[i] == reg) return static_cast<std::uint8_t>(i);
  }
  return std::nullopt;
}

// What a register holds, as far as the summary follows it: an argument's value, or the first word of the object an
// argument points at (its vtable, for a virtual call); since spike.8 also the value an inner summary was entered with in
// rcx to r9 (Entry), a value loaded from memory (Loaded, a source of the summary's table), or a call's result; since
// spike.9 a followed value (our task's, the group entry's, or one read from either) changed by an instruction; since
// spike.11 an address the code makes: a place in its stack frame, or a RIP-relative address.
struct Held {
  enum class Kind : std::uint8_t { None, Value, Deref, Entry, Loaded, Result, Modified, Stack, Address };
  Kind kind = Kind::None;
  std::uint8_t arg = 0;   // Value, Deref, Entry: into kArgs
  std::uint16_t id = 0;   // Loaded: the source's index; Result: the call's offset; Modified: the change's index;
                          // Stack: the displacement from rsp; Address: the address's index
  friend bool operator==(const Held&, const Held&) = default;
};

// A followed value changed by an instruction (spike.9): what it held before, the instruction's class and offset, and
// what it was changed by (an immediate, a register, or memory: neither).
struct Mod {
  Held origin;
  InsnClass op = InsnClass::Other;
  std::uint32_t at = 0;
  std::optional<std::uint64_t> imm;
  bool large = false;
  std::optional<std::uint8_t> by;
  std::uint8_t width = 8;
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
      case Held::Kind::Modified: return level(mods_[h.id].origin);
      default: return 0;
    }
  }
  // The change a modifying instruction makes (spike.9): the same change of the same value is the same, its offset the
  // first in address order; a change of a change of a change is not followed, nor past the table's size.
  std::optional<std::uint16_t> intern_mod(Mod m) {
    if (m.origin.kind == Held::Kind::Modified && mods_[m.origin.id].origin.kind == Held::Kind::Modified) {
      return std::nullopt;
    }
    for (std::size_t i = 0; i < mods_.size(); ++i) {
      Mod& o = mods_[i];
      if (o.origin == m.origin && o.op == m.op && o.imm == m.imm && o.large == m.large && o.by == m.by &&
          o.width == m.width) {
        o.at = std::min(o.at, m.at);
        return static_cast<std::uint16_t>(i);
      }
    }
    if (mods_.size() >= kMaxSources) return std::nullopt;
    mods_.push_back(m);
    return static_cast<std::uint16_t>(mods_.size() - 1);
  }
  [[nodiscard]] const Mod& mod(std::uint16_t id) const noexcept { return mods_[id]; }
  // A RIP-relative address a lea makes (spike.11), by its RVA; nothing past the table's size.
  std::optional<std::uint16_t> intern_address(std::uint32_t rva) {
    for (std::size_t i = 0; i < addresses_.size(); ++i) {
      if (addresses_[i] == rva) return static_cast<std::uint16_t>(i);
    }
    if (addresses_.size() >= kMaxSources) return std::nullopt;
    addresses_.push_back(rva);
    return static_cast<std::uint16_t>(addresses_.size() - 1);
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
      case Held::Kind::Modified: {
        const Mod& m = mods_[h.id];
        o.kind = Source::Kind::Modified;
        o.at = m.at;
        o.width = m.width;
        o.op = m.op;
        o.imm = m.imm;
        o.large = m.large;
        o.by = m.by;
        o.origin.push_back(describe(m.origin));
        return o;
      }
      case Held::Kind::Stack: o.kind = Source::Kind::Stack; o.disp = h.id; return o;
      case Held::Kind::Address: o.kind = Source::Kind::Address; o.target = addresses_[h.id]; return o;
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
  std::vector<Mod> mods_;
  std::vector<std::uint32_t> addresses_;
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

// Map 1 (0F) and VEX map 1 stores of a vector register to memory (movups, movlps, movhps, movaps, movntps, movdqa and
// movdqu, movq, movntdq), which read nothing at their address.
constexpr bool vector_store(std::uint8_t op) noexcept {
  return op == 0x11 || op == 0x13 || op == 0x17 || op == 0x29 || op == 0x2B || op == 0x7F || op == 0xD6 || op == 0xE7;
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

// What a value is, as the field reads follow it (spike.9): our task's (the task argument, a value loaded from it, or one
// of those changed), the group entry's (the same from the entry), since spike.10 the second singleton G's, or neither.
// Spike.11: the request G's method builds, and G's manager (the request's field of G's).
enum class Follow : std::uint8_t { None, Task, Entry, G, Request, Manager };
constexpr std::size_t kMaxWordUses = 16;

// Which of rcx to r9 hold what at the entry (spike.9 to spike.11): the group's entry, G, the entry's word, the request
// G's method builds, G's manager.
struct Holds {
  std::array<bool, 4> entry{}, g{}, word{}, request{}, manager{};
};

class Summariser {
 public:
  // `end`: the function's length, when the exception directory gives it, or with none the bytes decoded (a jump past it
  // is a tail jump). `task`: our task's argument, into kArgs; `holds`, what rcx to r9 hold. Spike.11: `layout`, the
  // request whose fields are followed; `parts`, the offsets [begin, end) of the parts past the primary (a jump into one
  // is no tail jump).
  Summariser(std::uint32_t rva, std::uint32_t end, CalleeCode& out, Sources& sources,
             std::optional<std::uint8_t> task = std::nullopt, const Holds& holds = {},
             const RequestShape* layout = nullptr, std::vector<std::pair<std::uint32_t, std::uint32_t>> parts = {})
      : rva_(rva), end_(end), out_(out), sources_(sources), task_(task), entry_(holds.entry), g_(holds.g),
        word_(holds.word), request_(holds.request), manager_(holds.manager), layout_(layout), parts_(std::move(parts)) {
    out_.args.resize(kArgs.size());
    for (std::size_t i = 0; i < kArgs.size(); ++i) out_.args[i].reg = kArgs[i];
    const auto any = [](const std::array<bool, 4>& a) { return std::find(a.begin(), a.end(), true) != a.end(); };
    following_ = task_.has_value() || any(entry_) || any(g_) || any(word_) || any(request_) || any(manager_);
    if (layout_ != nullptr) {
      task_field_ = layout_->task_field;
      for (const RequestField& f : layout_->fields) {
        if (f.role == FieldRole::Manager && f.width == 8) manager_fields_.push_back(f.offset);
        if (f.role == FieldRole::Word && !word_field_) word_field_ = f.offset;
      }
    }
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
    const Planned p = plan_read();
    effect();
    if (p.follow != Follow::None) add_read(p);
  }

  [[nodiscard]] bool called() const noexcept { return called_; }

 private:
  // ---- The field reads (spike.9) ----

  // What a value is, as the reads follow it. Spike.11: a value loaded from the request is the task where it is the
  // request's task field (8 bytes), G's manager where it is a manager field, else the request's (its vtable, say).
  [[nodiscard]] Follow follows(const Held& h, int depth = 0) const noexcept {
    if (depth > 8) return Follow::None;
    switch (h.kind) {
      case Held::Kind::Value:
      case Held::Kind::Deref:
        return task_ && h.arg == *task_ ? Follow::Task : Follow::None;
      case Held::Kind::Entry:
        return entry_[h.arg]     ? Follow::Entry
               : g_[h.arg]       ? Follow::G
               : request_[h.arg] ? Follow::Request
               : manager_[h.arg] ? Follow::Manager
                                 : Follow::None;
      case Held::Kind::Loaded: {
        const Src& s = sources_.at(h.id);
        if (s.rip || !s.base_reg) return Follow::None;
        const Follow b = follows(s.base, depth + 1);
        if (b == Follow::Request && !s.index_reg && s.width == 8 && s.disp >= 0) {
          const auto d = static_cast<std::uint64_t>(s.disp);
          if (task_ && layout_ != nullptr && d == task_field_) return Follow::Task;
          if (std::find(manager_fields_.begin(), manager_fields_.end(), d) != manager_fields_.end()) {
            return Follow::Manager;
          }
        }
        return b;
      }
      case Held::Kind::Modified:
        return follows(sources_.mod(h.id).origin, depth + 1);
      default:
        return Follow::None;
    }
  }

  // The instruction's memory read at a followed base, planned before its effect: what its base holds, and, where it
  // loads a register, which (the value's uses are followed), else how it uses the field in place.
  struct Planned {
    Follow follow = Follow::None;
    Mem mem;
    Held base;
    std::uint8_t width = 0;
    std::optional<std::uint8_t> dest;
    bool direct = false;
    FieldUse use;
  };
  [[nodiscard]] Planned plan_read() {
    Planned p;
    const Instruction& in = *in_;
    if (!following_ || !reached_ || !in.has_modrm || in.mod() == 3 || in.rip_relative) return p;
    const bool legacy = in.encoding == x86::Encoding::Legacy;
    const std::uint8_t op = in.opcode;
    const std::uint8_t sub = static_cast<std::uint8_t>((in.modrm >> 3) & 7);
    if (legacy && in.map == 0 && op == 0x8D) return p;  // lea reads no memory (its base's use is a change: modify)
    p.mem = memory_operand(in, bytes_);
    if (p.mem.index) use(regs_[*p.mem.index & 15], use_at(UseKind::Index));  // spike.10: a value as an index
    if (!p.mem.base) return p;
    p.base = regs_[*p.mem.base & 15];
    use(p.base, use_at(UseKind::Base));  // a value read before, the base of this one
    p.follow = follows(p.base);
    if (p.follow == Follow::None) return p;
    p.width = width_of(in, false);
    const auto direct = [&](UseKind k, std::uint8_t width) {
      p.direct = true;
      p.width = width;
      p.use = use_at(k);
    };
    if (legacy && in.map == 0) {
      switch (op) {
        case 0x8B: p.dest = in.reg(); return p;
        case 0x63: p.dest = in.reg(); p.width = 4; return p;
        case 0x8A: p.dest = byte_reg(in.reg(), in); p.width = 1; return p;
        case 0x88: case 0x89: case 0x8F: p.follow = Follow::None; return p;  // stores, not reads
        case 0xC6: case 0xC7:
          if (sub == 0) p.follow = Follow::None;
          else direct(UseKind::Other, width_of(in, op == 0xC6));
          return p;
        case 0x38: case 0x39: case 0x3A: case 0x3B: direct(UseKind::ComparedOther, width_of(in, (op & 1) == 0)); return p;
        case 0x80: case 0x81: case 0x83:
          if (sub == 7) {
            direct(UseKind::Compared, width_of(in, op == 0x80));
            immediate_use(p.use, p.width);
          } else {
            direct(UseKind::Other, width_of(in, op == 0x80));
          }
          return p;
        case 0x84: case 0x85: direct(UseKind::Tested, width_of(in, op == 0x84)); return p;
        case 0xF6: case 0xF7: direct(sub <= 1 ? UseKind::Tested : UseKind::Other, width_of(in, op == 0xF6)); return p;
        case 0xFF:
          direct(sub == 2 || sub == 3 || sub == 4 || sub == 5 ? UseKind::Called : UseKind::Other, sub >= 2 ? 8 : p.width);
          return p;
        default:
          direct(UseKind::Other, op < 0x40 && (op & 1) == 0 ? 1 : p.width);
          return p;
      }
    }
    if (legacy && in.map == 1) {
      if (op == 0xB6 || op == 0xBE || op == 0xB7 || op == 0xBF) {
        p.dest = in.reg();
        p.width = (op & 1) == 0 ? 1 : 2;
        return p;
      }
      if (vector_store(op) || op == 0xC3 || (op >= 0x90 && op <= 0x9F)) {  // a vector's store, movnti's or setcc's
        p.follow = Follow::None;
        return p;
      }
      if (op == 0xB1 || op == 0xC1) {  // cmpxchg, xadd: they read and write it
        direct(UseKind::Other, p.width);
        return p;
      }
      direct(UseKind::Other, 0);
      return p;
    }
    if (in.encoding == x86::Encoding::Vex && in.map == 1 && vector_store(op)) {
      p.follow = Follow::None;
      return p;
    }
    direct(UseKind::Other, 0);
    return p;
  }

  // The list of reads of a followed value, and its count.
  std::vector<FieldRead>& reads_list(Follow f) noexcept {
    switch (f) {
      case Follow::Task: return out_.task_reads;
      case Follow::Entry: return out_.entry_reads;
      case Follow::Request: return out_.request_reads;
      case Follow::Manager: return out_.manager_reads;
      default: return out_.g_reads;
    }
  }
  std::uint32_t& read_count(Follow f) noexcept {
    switch (f) {
      case Follow::Task: return out_.task_read_count;
      case Follow::Entry: return out_.entry_read_count;
      case Follow::Request: return out_.request_read_count;
      case Follow::Manager: return out_.manager_read_count;
      default: return out_.g_read_count;
    }
  }
  // A load or an in-place use of a followed field, kept in its list; a loaded value is mapped to it for its uses.
  void add_read(const Planned& p) {
    std::vector<FieldRead>& list = reads_list(p.follow);
    ++read_count(p.follow);
    Held value;
    if (p.dest) value = regs_[*p.dest & 15];
    if (list.size() >= kMaxFieldReads) return;
    FieldRead r;
    r.offset = at_;
    r.disp = p.mem.disp;
    r.width = p.width;
    r.base = static_cast<std::uint8_t>(*p.mem.base & 15);
    r.base_src = sources_.describe(p.base);
    r.index = p.mem.index;
    if (p.mem.index) r.scale = p.mem.scale;
    r.direct = p.direct;
    if (p.direct) {
      r.uses.push_back(p.use);
      r.use_count = 1;
    }
    list.push_back(std::move(r));
    const bool known = value.kind == Held::Kind::Loaded || value.kind == Held::Kind::Deref;
    if (known && std::none_of(reads_.begin(), reads_.end(), [&](const ReadOf& o) { return o.value == value; })) {
      reads_.push_back(ReadOf{value, p.follow, list.size() - 1});
    }
  }

  // The read whose value a register holds (or holds changed), if any.
  FieldRead* read_of(const Held& h, int depth = 0) {
    if (h.kind == Held::Kind::None || depth > 4) return nullptr;
    for (const ReadOf& o : reads_) {
      if (o.value == h) return &reads_list(o.which)[o.index];
    }
    if (h.kind == Held::Kind::Modified) return read_of(sources_.mod(h.id).origin, depth + 1);
    return nullptr;
  }
  [[nodiscard]] FieldUse use_at(UseKind k) const {
    FieldUse u;
    u.kind = k;
    u.offset = at_;
    return u;
  }
  // Spike.10: whether a register holds the entry's word (as G's method is entered with it in r8), or it changed;
  // spike.11: or it was loaded from the request's word field.
  [[nodiscard]] bool is_word(const Held& h, int depth = 0) const noexcept {
    if (depth > 4) return false;
    if (h.kind == Held::Kind::Entry) return word_[h.arg];
    if (h.kind == Held::Kind::Modified) return is_word(sources_.mod(h.id).origin, depth + 1);
    if (h.kind == Held::Kind::Loaded && word_field_) {
      const Src& s = sources_.at(h.id);
      return !s.rip && s.base_reg && !s.index_reg && s.width <= 4 && s.disp >= 0 &&
             static_cast<std::uint64_t>(s.disp) == *word_field_ && follows(s.base, depth + 1) == Follow::Request;
    }
    return false;
  }
  // A use of the value a register holds, kept with its read (spike.10: or as a use of the entry's word), in the
  // recording pass.
  void use(const Held& h, FieldUse u) {
    if (!recording_ || !reached_) return;
    if (is_word(h)) {
      ++out_.word_use_count;
      if (out_.word_uses.size() < kMaxWordUses) out_.word_uses.push_back(std::move(u));
      return;
    }
    FieldRead* r = read_of(h);
    if (r == nullptr) return;
    ++r->use_count;
    if (r->uses.size() < kMaxFieldUses) r->uses.push_back(std::move(u));
  }
  void immediate_use(FieldUse& u, std::uint8_t width) const {
    u.imm = small_immediate(*in_, bytes_, width);
    u.large = !u.imm && x86::immediate(*in_, bytes_).has_value();
  }
  // A store of the register `reg` to the instruction's memory operand: its value's use, and a task store where it holds
  // our task, a value read from it, or one of those changed.
  void stored(std::uint8_t reg, std::uint8_t width) {
    if (!recording_ || !reached_ || !following_) return;
    const Held h = regs_[reg & 15];
    const Mem m = memory_operand(*in_, bytes_);
    FieldUse u = use_at(UseKind::Stored);
    u.base = m.base;
    if (m.base) u.base_src = sources_.describe(regs_[*m.base & 15]);
    u.index = m.index;
    u.rip = m.rip;
    u.disp = m.disp;
    if (m.rip) {
      const auto slot = x86::rip_target(*in_, bytes_, std::uint64_t{rva_} + at_);
      u.disp = slot ? static_cast<std::int64_t>(*slot) : 0;
    }
    u.width = width;
    if (follows(h) == Follow::Task) {
      ++out_.task_store_count;
      if (out_.task_stores.size() < kMaxTaskStores) {
        TaskStore s;
        s.offset = at_;
        s.via = static_cast<std::uint8_t>(reg & 15);
        s.value = sources_.describe(h);
        s.base = u.base;
        s.base_src = u.base_src;
        s.index = u.index;
        s.disp = u.disp;
        s.rip = u.rip;
        s.width = width;
        out_.task_stores.push_back(std::move(s));
      }
    }
    use(h, std::move(u));
  }
  // The followed values in rcx to r9 at a call or a tail jump, passed.
  void passed_uses(std::optional<std::uint32_t> target) {
    for (const std::uint8_t r : kArgs) {
      FieldUse u = use_at(UseKind::Passed);
      u.reg = r;
      u.target = target;
      use(regs_[r], std::move(u));
    }
  }
  // dst changed by an instruction of class `op`: a followed value stays followed, as changed; anything else is cleared.
  void modify(std::uint8_t dst, InsnClass op, std::uint8_t width, std::optional<std::uint64_t> imm, bool large,
              std::optional<std::uint8_t> by) {
    change(regs_[dst & 15], dst, op, width, imm, large, by);
  }
  // dst := `h` changed (`h` the value dst held, or for a lea its base's).
  void change(const Held h, std::uint8_t dst, InsnClass op, std::uint8_t width, std::optional<std::uint64_t> imm,
              bool large, std::optional<std::uint8_t> by) {
    if (following_ && width >= 4 && (follows(h) != Follow::None || is_word(h))) {
      if (op != InsnClass::Lea) {  // a lea's base is used as one, which plan_read does not see (it reads nothing)
        FieldUse u = use_at(UseKind::Changed);
        u.op = op;
        use(h, std::move(u));
      }
      const auto id = sources_.intern_mod(Mod{h, op, at_, imm, large, by, width});
      if (id) {
        regs_[dst & 15] = Held{Held::Kind::Modified, 0, *id};
        return;
      }
    }
    kill(dst);
  }
  // lea reg, [base + index*scale + disp] (spike.9): from a followed base, a change of it by the displacement (or by the
  // index), and a use of it as a base; [base] alone, a copy; anything else clears reg.
  // Spike.11: lea reg, [rip + d] makes an address in the image, and lea reg, [rsp + d] a place in the stack frame (64-bit
  // only, a displacement from 0 to 65,535).
  void lea() {
    const Instruction& in = *in_;
    const Mem m = memory_operand(in, bytes_);
    const std::uint8_t dst = in.reg();
    if (m.rip) {
      const auto t = x86::rip_target(in, bytes_, std::uint64_t{rva_} + at_);
      const auto id = width_of(in, false) == 8 && t && *t <= 0xFFFF'FFFFull
                          ? sources_.intern_address(static_cast<std::uint32_t>(*t))
                          : std::nullopt;
      if (!id) return kill(dst);
      regs_[dst & 15] = Held{Held::Kind::Address, 0, *id};
      return;
    }
    if (!m.base) return kill(dst);
    if ((*m.base & 15) == kRsp && !m.index) {
      if (width_of(in, false) != 8 || m.disp < 0 || m.disp > 0xFFFF) return kill(dst);
      regs_[dst & 15] = Held{Held::Kind::Stack, 0, static_cast<std::uint16_t>(m.disp)};
      return;
    }
    const Held base = regs_[*m.base & 15];
    use(base, use_at(UseKind::Base));
    if (m.index) use(regs_[*m.index & 15], use_at(UseKind::Index));
    if (!following_ || (follows(base) == Follow::None && !is_word(base))) return kill(dst);
    const std::uint8_t width = width_of(in, false);
    if (!m.index && m.disp == 0) {
      if (width == 8) {
        regs_[dst & 15] = base;
      } else {
        kill(dst);
      }
      return;
    }
    const bool small = !m.index && m.disp >= 0 && static_cast<std::uint64_t>(m.disp) <= kImmBound;
    change(base, dst, InsnClass::Lea, width,
           small ? std::optional<std::uint64_t>(static_cast<std::uint64_t>(m.disp)) : std::nullopt,
           !m.index && !small, m.index ? std::optional<std::uint8_t>(*m.index & 15) : std::nullopt);
  }
  // The same by the instruction's immediate, read at `width`, or by a register.
  void modify_imm(std::uint8_t dst, InsnClass op, std::uint8_t width) {
    FieldUse u;
    immediate_use(u, width);
    modify(dst, op, width, u.imm, u.large, std::nullopt);
  }
  void modify_reg(std::uint8_t dst, InsnClass op, std::uint8_t width, std::uint8_t by) {
    if (op == InsnClass::Xor && (by & 15) == (dst & 15)) return kill(dst);  // xor r, r: zero
    if (op == InsnClass::And && (by & 15) == (dst & 15)) return;           // and r, r: unchanged
    if (op == InsnClass::Or && (by & 15) == (dst & 15)) return;
    modify(dst, op, width, std::nullopt, false, by & 15);
  }
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
    const bool narrow = h.kind == Held::Kind::Value || (h.kind == Held::Kind::Entry && word_[h.arg]) ||
                        (h.kind == Held::Kind::Loaded && sources_.at(h.id).width <= 4) ||
                        (h.kind == Held::Kind::Modified && sources_.mod(h.id).width <= 4);
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
      if (h.kind == Held::Kind::Entry || h.kind == Held::Kind::Loaded || h.kind == Held::Kind::Modified ||
          h.kind == Held::Kind::Stack || h.kind == Held::Kind::Address) {
        p.push_back(PassedSource{r, sources_.describe(h), false});
      }
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

  // Spike.11: a store to [rsp + d], kept for the request's shape (the first kMaxStackStores).
  void stack_store(std::uint8_t reg, std::uint8_t width) {
    if (!recording_ || !reached_ || out_.stack_stores.size() >= kMaxStackStores) return;
    const Mem m = memory_operand(*in_, bytes_);
    if (m.rip || !m.base || (*m.base & 15) != kRsp || m.index) return;
    out_.stack_stores.push_back(StackStore{at_, m.disp, width, sources_.describe(regs_[reg & 15])});
  }

  // ---- What is kept before the first call ----
  void store(std::uint8_t reg, std::uint8_t width) {
    stack_store(reg, width);
    stored(reg, width);
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
    FieldUse u = use_at(UseKind::Compared);
    immediate_use(u, width);
    use(regs_[reg & 15], std::move(u));
    const auto arg = value(reg);
    if (!arg || !keep()) return;
    auto& compares = out_.args[*arg].compares;
    if (compares.size() >= kMaxCompares) return;
    compares.push_back(ArgCompare{at_, static_cast<std::uint8_t>(reg & 15), small_immediate(*in_, bytes_, width)});
  }
  void compare_other(std::uint8_t reg) {
    use(regs_[reg & 15], use_at(UseKind::ComparedOther));
    if (const auto arg = value(reg); arg && keep()) ++out_.args[*arg].compared_other;
  }
  void tested(std::uint8_t reg) {
    use(regs_[reg & 15], use_at(UseKind::Tested));
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
      passed_uses(in.flow == Flow::Call ? direct_target() : std::nullopt);
      if (in.flow == Flow::CallIndirect && in.mod() == 3) use(regs_[in.rm()], use_at(UseKind::Called));
    }
    // The volatile registers are the callee's to change; the callee-saved ones keep what they held. rax holds its result.
    for (const std::uint8_t r : {0, 1, 2, 8, 9, 10, 11}) kill(static_cast<std::uint8_t>(r));
    regs_[kRax] = Held{Held::Kind::Result, 0, static_cast<std::uint16_t>(at_)};
  }

  // A direct call's or jump's target, as an RVA.
  [[nodiscard]] std::optional<std::uint32_t> direct_target() const {
    const auto to = x86::branch_target(*in_, bytes_, std::uint64_t{rva_} + at_);
    if (!to || *to > 0xFFFF'FFFFull) return std::nullopt;
    return static_cast<std::uint32_t>(*to);
  }

  // A direct jump out of the function: before its entry, or past its end where the exception directory gives one, or
  // (spike.9) with none, past the code decoded.
  void tail() {
    const auto to = x86::branch_target(*in_, bytes_, std::uint64_t{rva_} + at_);
    if (!to || *to > 0xFFFF'FFFFull) return;
    const bool out = (*to < rva_ || (end_ != 0 && *to >= std::uint64_t{rva_} + end_)) && !in_part(*to);
    if (!out) return;
    passed_uses(static_cast<std::uint32_t>(*to));
    if (out_.calls.size() >= kMaxCalls) return;
    CalleeCall c;
    c.offset = at_;
    c.form = CallForm::Tail;
    c.target = static_cast<std::uint32_t>(*to);
    c.reached = reached_;
    c.pass = passed();
    c.pass_src = passed_sources();
    out_.calls.push_back(std::move(c));
  }

  // Spike.11: whether an RVA lies in one of the parts past the primary.
  [[nodiscard]] bool in_part(std::uint64_t to) const noexcept {
    if (to < rva_) return false;
    const std::uint64_t off = to - rva_;
    return std::any_of(parts_.begin(), parts_.end(), [&](const auto& r) { return off >= r.first && off < r.second; });
  }

  // A jump through a register or memory (spike.8): it leaves the function, a tail call through a pointer.
  void tail_indirect() {
    const Instruction& in = *in_;
    passed_uses(std::nullopt);
    if (in.mod() == 3) use(regs_[in.rm()], use_at(UseKind::Called));
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
      // Spike.9: add, or, and, sub and xor keep a followed value followed, as changed (adc and sbb clear it).
      static constexpr std::array<InsnClass, 8> kChange = {InsnClass::Add, InsnClass::Or,  InsnClass::Other,
                                                           InsnClass::Other, InsnClass::And, InsnClass::Sub,
                                                           InsnClass::Xor, InsnClass::Cmp};
      const InsnClass change = byte_form ? InsnClass::Other : kChange[op >> 3];
      const std::uint8_t width = width_of(in, byte_form);
      switch (op & 7) {
        case 0: case 1:  // r/m, r
          if (cmp) {
            compare_other(r8(byte_form, in.reg()));
            if (reg_form) compare_other(r8(byte_form, in.rm()));
          } else if (reg_form) {
            if (change == InsnClass::Other) {
              kill(r8(byte_form, in.rm()));
            } else {
              modify_reg(in.rm(), change, width, in.reg());
            }
          }
          return true;
        case 2: case 3:  // r, r/m
          if (cmp) {
            compare_other(r8(byte_form, in.reg()));
            if (reg_form) compare_other(r8(byte_form, in.rm()));
          } else if (change == InsnClass::Other) {
            kill(r8(byte_form, in.reg()));
          } else if (reg_form) {
            modify_reg(in.reg(), change, width, in.rm());
          } else {
            modify(in.reg(), change, width, std::nullopt, false, std::nullopt);  // by memory
          }
          return true;
        default:  // al or eax, imm
          if (cmp) {
            compare_imm(kRax, width_of(in, (op & 7) == 4));
          } else if (change == InsnClass::Other) {
            kill(kRax);
          } else {
            modify_imm(kRax, change, width);
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
          regs_[in.reg()] = h.kind == Held::Kind::Value || is_word(h) ? h : Held{};
        } else {
          load(in.reg(), 4);
        }
        return true;
      case 0x69: case 0x6B:  // imul: writes reg
        kill(in.reg());
        return true;
      case 0x8D:  // lea: writes reg; spike.9: [followed + disp] or [followed + index*scale + disp] is a change of it
        lea();
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
          static constexpr std::array<InsnClass, 8> kGroup = {InsnClass::Add, InsnClass::Or,  InsnClass::Other,
                                                              InsnClass::Other, InsnClass::And, InsnClass::Sub,
                                                              InsnClass::Xor, InsnClass::Cmp};
          if (op == 0x80 || kGroup[sub] == InsnClass::Other) {
            kill(r8(op == 0x80, in.rm()));
          } else {
            modify_imm(in.rm(), kGroup[sub], width_of(in, false));
          }
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
          stored(r8(op == 0x86, in.reg()), width_of(in, op == 0x86));  // spike.9: it stores the register too
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
        if (!reg_form) return true;
        if ((op & 1) == 0 || sub < 4 || sub == 6) {  // a byte's, or a rotate: cleared
          kill(r8((op & 1) == 0, in.rm()));
        } else if (op == 0xC1) {
          modify_imm(in.rm(), InsnClass::Shift, width_of(in, false));
        } else if (op == 0xD1) {
          modify(in.rm(), InsnClass::Shift, width_of(in, false), 1, false, std::nullopt);
        } else {
          modify(in.rm(), InsnClass::Shift, width_of(in, false), std::nullopt, false, std::uint8_t{1});  // by cl
        }
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
        // Spike.9: a bit set, cleared or flipped in a followed value (a tag) changes it: class other, by the bit's number.
        if (sub > 4 && reg_form) {
          FieldUse bit;
          immediate_use(bit, 1);
          modify(in.rm(), InsnClass::Other, width_of(in, false), bit.imm, bit.large, std::nullopt);
        }
        return true;
      case 0xAB: case 0xB3: case 0xBB:  // bts, btr, btc r/m, r
        if (reg_form) modify(in.rm(), InsnClass::Other, width_of(in, false), std::nullopt, false, in.reg());
        return true;
      case 0xB1:  // cmpxchg
        if (!reg_form) stored(in.reg(), width_of(in, false));  // spike.9: it may store the register
        kill(kRax);
        if (reg_form) kill(in.rm());
        return true;
      case 0xC3:  // movnti m, r: a store
        if (!reg_form) stored(in.reg(), width_of(in, false));
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

  struct ReadOf {
    Held value;
    Follow which = Follow::Task;
    std::size_t index = 0;
  };

  std::uint32_t rva_;
  std::uint32_t end_;
  CalleeCode& out_;
  Sources& sources_;
  std::optional<std::uint8_t> task_;
  std::array<bool, 4> entry_{};
  std::array<bool, 4> g_{};
  std::array<bool, 4> word_{};
  std::array<bool, 4> request_{};
  std::array<bool, 4> manager_{};
  const RequestShape* layout_ = nullptr;
  std::uint64_t task_field_ = 0;
  std::vector<std::uint64_t> manager_fields_;
  std::optional<std::uint64_t> word_field_;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> parts_;
  bool following_ = false;
  std::vector<ReadOf> reads_;  // the loaded values of the reads, each with its read (the first that loaded it)
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
    if (const auto i = arg_index(p.reg)) e[*i] = EntryArg{p.from, p.first_word, false};
  }
  return e;
}

EntryState entry_at(const CalleeCall& call, const EntryState& parent) noexcept {
  EntryState e = entry_at(call);
  for (const PassedSource& p : call.pass_src) {
    const auto to = arg_index(p.reg);
    const auto from = p.source.kind == Source::Kind::Entry ? arg_index(p.source.from) : std::nullopt;
    if (!to || !from || e[*to].from != 0) continue;
    const EntryArg& was = parent[*from];
    e[*to].entry = e[*to].entry || was.entry;
    e[*to].g = e[*to].g || was.g;
    e[*to].word = e[*to].word || was.word;
    e[*to].zero = e[*to].zero || was.zero;
    e[*to].request = e[*to].request || was.request;  // spike.11
    e[*to].manager = e[*to].manager || was.manager;
  }
  return e;
}

EntryState method_entry_at(const CalleeCall& site, const EntryState& parent) noexcept {
  EntryState e = entry_at(site, parent);
  for (const PassedSource& p : site.pass_src) {
    const auto to = arg_index(p.reg);
    if (!to || e[*to].from != 0) continue;
    if (p.reg == 1 && p.source.kind == Source::Kind::Rip && p.source.width == 8) e[*to].g = true;
    if (p.source.kind == Source::Kind::Memory && p.source.base_src.size() == 1 &&
        p.source.base_src[0].kind == Source::Kind::Entry) {
      const auto from = arg_index(p.source.base_src[0].from);
      if (from && parent[*from].entry) e[*to].word = true;
    }
  }
  return e;
}

const CalleeCall* method_site(const CalleeCode& group, std::uint8_t task) noexcept {
  for (const CallForm form : {CallForm::Tail, CallForm::Direct}) {  // a tail jump first, as run 9's
    for (const CalleeCall& c : group.calls) {
      if (!c.reached || !c.target || c.form != form) continue;
      const bool passes_task = std::any_of(c.pass.begin(), c.pass.end(),
                                           [&](const PassedArg& p) { return p.from == task && !p.first_word; });
      const auto rcx =
          std::find_if(c.pass_src.begin(), c.pass_src.end(), [](const PassedSource& p) { return p.reg == 1; });
      if (passes_task && rcx != c.pass_src.end() && rcx->source.kind == Source::Kind::Rip && rcx->source.width == 8) {
        return &c;
      }
    }
  }
  return nullptr;
}

const CalleeCall* stub_jump(const CalleeCode& stub) noexcept {
  if (!stub.read || stub.bytes == 0 || stub.bytes > kMaxStubBytes || stub.call_count != 0 || stub.branch_count != 0 ||
      stub.first_ret || stub.calls.size() != 1 || stub.forms.empty()) {
    return nullptr;
  }
  const CalleeCall& j = stub.calls.front();
  const Form& last = stub.forms.back();
  if (j.form != CallForm::Tail || !j.reached || !j.target || last.offset != j.offset || last.cls != InsnClass::Jmp ||
      last.offset + last.length != stub.bytes) {
    return nullptr;
  }
  return &j;
}

EntryState through_stub(const CalleeCode& stub, const EntryState& stub_entry) noexcept {
  const CalleeCall* j = stub_jump(stub);
  if (j == nullptr) return {};
  EntryState e = entry_at(*j, stub_entry);
  std::array<bool, 4> zero{};
  for (const Form& f : stub.forms) {
    if (f.offset >= j->offset) break;
    if (f.operands.empty() || f.operands[0].form != OperandForm::Register || !f.operands[0].reg) continue;
    const auto i = arg_index(*f.operands[0].reg);
    if (!i) continue;
    const Operand& dst = f.operands[0];
    const bool self_xor = f.cls == InsnClass::Xor && f.operands.size() == 2 &&
                          f.operands[1].form == OperandForm::Register && f.operands[1].reg == dst.reg;
    const bool mov_zero = f.cls == InsnClass::Mov && f.operands.size() == 2 &&
                          f.operands[1].form == OperandForm::Immediate && f.operands[1].imm == std::optional<std::uint64_t>(0);
    switch (f.cls) {
      case InsnClass::Cmp: case InsnClass::Test: case InsnClass::Push: case InsnClass::Nop:
        break;
      default:
        zero[*i] = (self_xor || mov_zero) && dst.width >= 4;
        break;
    }
  }
  for (std::size_t i = 0; i < zero.size(); ++i) {
    if (zero[i]) e[i] = EntryArg{0, false, false, false, false, true};
  }
  return e;
}

EntryState group_entry_at(const CalleeCall& site) noexcept {
  EntryState e = entry_at(site);
  if (e[0].from == 0) e[0].entry = true;
  return e;
}

std::vector<ChainedEntry> chained_entries(const ImageBytes& img, std::span<const RuntimeFunction> functions) {
  constexpr unsigned kChainInfo = 0x4;  // UNW_FLAG_CHAININFO
  std::vector<ChainedEntry> out;
  for (const RuntimeFunction& f : functions) {
    std::uint32_t begin = f.begin;
    std::uint32_t unwind = f.unwind;
    std::optional<std::uint32_t> primary;
    bool chained = false;
    for (int link = 0; link < 16; ++link) {
      if ((unwind & 1u) != 0) {  // an entry that points at another entry: its RUNTIME_FUNCTION's RVA
        const auto b = img.u32(unwind & ~1u);
        const auto u = img.u32((unwind & ~1u) + 8);
        if (!b || !u) break;
        chained = true;
        begin = *b;
        unwind = *u;
        continue;
      }
      const auto flags = img.u8(unwind);
      const auto codes = img.u8(unwind + 2);
      if (!flags || !codes) break;
      if (((*flags >> 3) & kChainInfo) == 0) {
        primary = begin;
        break;
      }
      // The chained RUNTIME_FUNCTION follows the unwind codes, their count rounded up to an even number.
      const std::uint32_t at = unwind + 4 + 2 * ((std::uint32_t{*codes} + 1u) & ~1u);
      const auto b = img.u32(at);
      const auto u = img.u32(at + 8);
      if (!b || !u) break;
      chained = true;
      begin = *b;
      unwind = *u;
    }
    if (chained && primary && *primary != f.begin) out.push_back(ChainedEntry{f.begin, f.end, *primary});
  }
  std::sort(out.begin(), out.end(), [](const ChainedEntry& a, const ChainedEntry& b) {
    return a.primary != b.primary ? a.primary < b.primary : a.begin < b.begin;
  });
  return out;
}

namespace {

SectionKind section_at(std::span<const SectionSpan> sections, std::uint32_t rva) noexcept {
  for (const SectionSpan& sec : sections) {
    if (rva >= sec.rva && rva - sec.rva < sec.size) return sec.kind;
  }
  return SectionKind::None;
}

// The role of what a request's field holds, by the method's entry state (spike.11).
FieldRole role_of(const Source& v, const EntryState& entry, std::uint8_t task) noexcept {
  if (v.kind == Source::Kind::Argument && v.from == task) return FieldRole::Task;
  if (v.kind == Source::Kind::Entry) {
    if (const auto i = arg_index(v.from)) {
      if (entry[*i].word) return FieldRole::Word;
      if (entry[*i].zero) return FieldRole::Zero;
    }
  }
  if (v.kind == Source::Kind::Memory && v.base_src.size() == 1 && v.base_src[0].kind == Source::Kind::Entry &&
      !v.index) {
    if (const auto i = arg_index(v.base_src[0].from); i && entry[*i].g) return FieldRole::Manager;
  }
  return FieldRole::Other;
}

}  // namespace

std::optional<RequestShape> request_shape(const CalleeCode& method, const EntryState& entry, std::uint8_t task,
                                          std::span<const SectionSpan> sections) {
  const std::vector<StackStore>& st = method.stack_stores;
  for (const StackStore& head : st) {
    if (head.width != 8 || head.value.kind != Source::Kind::Address) continue;
    if (!sections.empty() && section_at(sections, head.value.target) != SectionKind::Rdata) continue;
    const auto has_task = std::any_of(st.begin(), st.end(), [&](const StackStore& x) {
      return x.disp == head.disp + 8 && x.width == 8 && x.value.kind == Source::Kind::Argument && x.value.from == task;
    });
    if (!has_task) continue;
    RequestShape r;
    r.vtable = head.value.target;
    r.disp = head.disp;
    r.site = head.offset;
    r.task_field = 8;
    for (const StackStore& x : st) {  // in address order: the first store at each place
      if (x.disp < head.disp || x.disp >= head.disp + 64) continue;
      const auto off = static_cast<std::uint32_t>(x.disp - head.disp);
      if (std::any_of(r.fields.begin(), r.fields.end(), [&](const RequestField& f) { return f.offset == off; })) {
        continue;
      }
      RequestField f;
      f.offset = off;
      f.width = x.width;
      f.role = off == 0 ? FieldRole::Vtable : role_of(x.value, entry, task);
      f.source = x.value;
      r.fields.push_back(std::move(f));
    }
    std::sort(r.fields.begin(), r.fields.end(),
              [](const RequestField& a, const RequestField& b) { return a.offset < b.offset; });
    for (const CalleeCall& c : method.calls) {
      if (c.form != CallForm::Direct || !c.reached || !c.target) continue;
      const auto rcx = std::find_if(c.pass_src.begin(), c.pass_src.end(), [](const PassedSource& p) { return p.reg == 1; });
      if (rcx != c.pass_src.end() && rcx->source.kind == Source::Kind::Stack && rcx->source.disp == head.disp) {
        r.now = c.target;
        break;
      }
    }
    return r;
  }
  return std::nullopt;
}

EntryState request_entry_at(const CalleeCall& call, const EntryState& parent, const RequestShape& request,
                            std::uint8_t task) noexcept {
  EntryState e = entry_at(call, parent);
  for (const PassedSource& p : call.pass_src) {
    const auto to = arg_index(p.reg);
    if (!to || e[*to].from != 0) continue;
    const Source& v = p.source;
    if (v.kind == Source::Kind::Stack && v.disp == request.disp) {
      e[*to].request = true;
      continue;
    }
    if (v.kind != Source::Kind::Memory || v.index || v.base_src.size() != 1 ||
        v.base_src[0].kind != Source::Kind::Entry || v.disp < 0) {
      continue;
    }
    const auto from = arg_index(v.base_src[0].from);
    if (!from || !parent[*from].request) continue;
    const auto off = static_cast<std::uint64_t>(v.disp);
    for (const RequestField& f : request.fields) {
      if (f.offset != off) continue;
      if (f.role == FieldRole::Task && v.width == 8 && task != 0) e[*to].from = task;
      if (f.role == FieldRole::Manager && v.width == 8) e[*to].manager = true;
      if (f.role == FieldRole::Word && v.width <= 4) e[*to].word = true;
    }
  }
  return e;
}

CalleeCode summarise_callee(std::span<const std::byte> code, std::uint32_t rva, std::uint32_t extent,
                            const EntryState& entry, std::uint8_t task, std::span<const std::uint32_t> starts) {
  SummaryOptions options;
  options.starts = starts;
  return summarise_callee(code, rva, extent, entry, task, options);
}

namespace {

// Spike.11: the exception directory's entry holding `rva`, by binary search over the sorted entries.
const RuntimeFunction* entry_holding(std::span<const RuntimeFunction> functions, std::uint64_t rva) noexcept {
  const auto it = std::upper_bound(functions.begin(), functions.end(), rva,
                                   [](std::uint64_t at, const RuntimeFunction& f) { return at < f.begin; });
  if (it == functions.begin()) return nullptr;
  const RuntimeFunction& f = *std::prev(it);
  return rva < f.end ? &f : nullptr;
}

// The parts past a function's primary entry (spike.11): its chained entries, then the unlisted code its conditional
// branches reach, each decoded linearly into `decoded`. `ranges` gets each decoded part's [begin, end) offsets.
class Parts {
 public:
  Parts(std::span<const std::byte> code, std::uint32_t rva, std::uint32_t primary_end, const SummaryOptions& options,
        std::vector<Decoded>& decoded, CalleeCode& out)
      : code_(code), rva_(rva), primary_(primary_end), options_(options), decoded_(decoded), out_(out) {}

  std::vector<std::pair<std::uint32_t, std::uint32_t>> run() {
    std::vector<CodePart> parts;
    const auto [lo, hi] = std::equal_range(options_.chained.begin(), options_.chained.end(), ChainedEntry{0, 0, rva_},
                                           [](const ChainedEntry& a, const ChainedEntry& b) { return a.primary < b.primary; });
    for (auto it = lo; it != hi && parts.size() + 1 < kMaxParts; ++it) {
      CodePart part{it->begin, 0, PartHow::Chained};
      if (it->begin > rva_ && it->end > it->begin && std::uint64_t{it->end} - rva_ <= code_.size() &&
          !inside(it->begin - rva_)) {
        const std::uint32_t from = it->begin - rva_;
        const std::uint32_t to = std::min<std::uint32_t>(it->end - rva_, from + kPartBound);
        part.bytes = linear(from, to, false);
      }
      parts.push_back(part);
    }
    while (parts.size() + 1 < kMaxParts) {
      const std::optional<std::uint32_t> from = next_branch_target();
      if (!from) break;
      const std::uint32_t to =
          static_cast<std::uint32_t>(std::min<std::uint64_t>(code_.size(), std::uint64_t{*from} + kPartBound));
      CodePart part{rva_ + *from, 0, PartHow::Unlisted};
      part.bytes = linear(*from, to, true);
      tried_.push_back(*from);
      if (part.bytes == 0) continue;
      parts.push_back(part);
    }
    if (!parts.empty()) {
      out_.parts.push_back(CodePart{rva_, primary_, PartHow::Primary});
      out_.parts.insert(out_.parts.end(), parts.begin(), parts.end());
    }
    return ranges_;
  }

 private:
  // Whether an offset lies in the primary or a decoded part.
  [[nodiscard]] bool inside(std::uint64_t off) const noexcept {
    if (off < primary_) return true;
    return std::any_of(ranges_.begin(), ranges_.end(), [&](const auto& r) { return off >= r.first && off < r.second; });
  }
  [[nodiscard]] bool starts_entry(std::uint64_t off) const noexcept {
    return std::binary_search(options_.starts.begin(), options_.starts.end(), static_cast<std::uint32_t>(rva_ + off));
  }
  // The lowest target of a conditional branch decoded so far past the primary, outside every part and every entry of
  // the directory, not tried before.
  [[nodiscard]] std::optional<std::uint32_t> next_branch_target() const {
    std::optional<std::uint32_t> best;
    for (const Decoded& d : decoded_) {
      if (d.insn.flow != Flow::Branch) continue;
      const auto to = x86::branch_target(d.insn, code_.subspan(d.at, d.insn.length), std::uint64_t{rva_} + d.at);
      if (!to || *to < rva_ || *to - rva_ >= code_.size()) continue;
      const auto off = static_cast<std::uint32_t>(*to - rva_);
      if (inside(off) || entry_holding(options_.functions, *to) != nullptr ||
          std::find(tried_.begin(), tried_.end(), off) != tried_.end()) {
        continue;
      }
      if (!best || off < *best) best = off;
    }
    return best;
  }
  // Decodes [from, to) linearly; an unlisted part ends at a return, a trap, a jump through memory or a register, a
  // direct jump not back into a part, another entry's start or a part's. Answers the bytes decoded.
  std::uint32_t linear(std::uint32_t from, std::uint32_t to, bool unlisted) {
    std::uint32_t at = from;
    while (at < to) {
      if (at != from && (inside(at) || (unlisted && starts_entry(at)))) break;
      const auto insn = x86::decode(code_.subspan(at, std::min<std::size_t>(x86::kMaxLength, to - at)));
      if (!insn) break;
      const std::uint32_t next = at + insn->length;
      if (std::any_of(ranges_.begin(), ranges_.end(), [&](const auto& r) { return at < r.first && next > r.first; })) {
        break;  // it would run into a part
      }
      decoded_.push_back(Decoded{at, *insn});
      at = next;
      if (!unlisted) continue;
      if (insn->flow == Flow::Return || insn->flow == Flow::Trap || insn->flow == Flow::JumpIndirect) break;
      if (insn->flow == Flow::Jump) {
        const auto t = x86::branch_target(*insn, code_.subspan(at - insn->length, insn->length),
                                          std::uint64_t{rva_} + at - insn->length);
        const bool back = t && *t >= rva_ && (inside(*t - rva_) || (*t - rva_ >= from && *t - rva_ < at));
        if (!back) break;
      }
    }
    if (at > from) ranges_.emplace_back(from, at);
    return at - from;
  }

  std::span<const std::byte> code_;
  std::uint32_t rva_;
  std::uint32_t primary_;
  const SummaryOptions& options_;
  std::vector<Decoded>& decoded_;
  CalleeCode& out_;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges_;
  std::vector<std::uint32_t> tried_;
};

}  // namespace

CalleeCode summarise_callee(std::span<const std::byte> code, std::uint32_t rva, std::uint32_t extent,
                            const EntryState& entry, std::uint8_t task, const SummaryOptions& options) {
  const std::span<const std::uint32_t> starts = options.starts;
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
      const bool to_function = insn->flow == Flow::Jump && to && *to > rva && *to <= 0xFFFF'FFFFull &&
                               std::binary_search(starts.begin(), starts.end(), static_cast<std::uint32_t>(*to));
      if (to && *to >= rva && !to_function) past = std::max<std::uint64_t>(past, *to - rva);
    }
    const bool leaves = insn->flow == Flow::Return || insn->flow == Flow::Jump || insn->flow == Flow::JumpIndirect;
    if (insn->flow == Flow::Return || (leaves && past < at)) {
      ended = true;
      break;
    }
  }
  if (extent != 0 && at >= extent) ended = true;
  out.cut = !ended && !out.undecodable && limit == kCalleeBound;
  // Spike.11: the parts past the extent, where the directory is known.
  std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
  std::size_t span = at;
  std::uint64_t decoded_bytes = at;
  if (extent != 0 && !options.functions.empty() && !decoded.empty()) {
    ranges = Parts(code, rva, static_cast<std::uint32_t>(at), options, decoded, out).run();
    for (const auto& r : ranges) {
      span = std::max<std::size_t>(span, r.second);
      decoded_bytes += r.second - r.first;
    }
    std::sort(decoded.begin(), decoded.end(), [](const Decoded& a, const Decoded& b) { return a.at < b.at; });
  }
  out.bytes = static_cast<std::uint32_t>(decoded_bytes);
  out.instructions = static_cast<std::uint32_t>(decoded.size());
  const std::span<const std::byte> bytes = code.first(span);
  const std::size_t max_forms = out.parts.empty() ? kMaxForms : 2 * kMaxForms;

  // What each instruction's registers hold along the flow, to a fixed point: a register only ever loses what it holds.
  Sources sources;
  Holds holds;
  for (std::size_t i = 0; i < kArgs.size(); ++i) {
    const bool none = entry[i].from == 0;
    holds.entry[i] = entry[i].entry && none;
    holds.g[i] = entry[i].g && none;
    holds.word[i] = entry[i].word && none;
    holds.request[i] = entry[i].request && none;
    holds.manager[i] = entry[i].manager && none;
  }
  Summariser s(rva, extent != 0 ? extent : static_cast<std::uint32_t>(at), out, sources, arg_index(task), holds,
               options.request, ranges);
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
    if (out.forms.size() < max_forms) out.forms.push_back(form_of(d, bytes, rva, states[i]));
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

// Whether a form writes rcx (or a part of it), as far as its class tells: an unknown class might.
bool writes_rcx(const Form& f) noexcept {
  switch (f.cls) {
    case InsnClass::Cmp: case InsnClass::Test: case InsnClass::Push: case InsnClass::Nop: return false;
    case InsnClass::Mov: case InsnClass::MovExt: case InsnClass::Lea: case InsnClass::Pop: case InsnClass::Xor:
    case InsnClass::Add: case InsnClass::Sub: case InsnClass::And: case InsnClass::Or: case InsnClass::Shift:
    case InsnClass::Imul:
      return !f.operands.empty() && f.operands[0].form == OperandForm::Register && f.operands[0].reg == 1;
    default: return true;
  }
}

// When the paths into the tail jump disagree on rcx (a singleton made and not stored where it is loaded from): the
// path past the null test of rcx loaded from S, where rcx is not null, reaches the jump in straight-line code that
// leaves rcx as it is. On that path the jump's rcx is the slot's value.
bool singleton_past_guard(const CalleeCode& wrapper, const CalleeCall& jump, Table& t) noexcept {
  for (const CalleeGuard& g : wrapper.guards) {
    if (g.op != GuardOp::Test || g.width != 8 || g.cond != 5 || g.operands.size() != 2) continue;  // jne
    const Operand& a = g.operands[0];
    const Operand& b = g.operands[1];
    if (a.form != OperandForm::Register || b.form != OperandForm::Register || a.reg != std::optional<std::uint8_t>(1) ||
        b.reg != a.reg || !a.source || g.to < 0 || g.to > jump.offset) {
      continue;
    }
    Table probe;
    if (!singleton_of(*a.source, probe)) continue;
    bool straight = false;
    for (const Form& f : wrapper.forms) {
      if (f.offset < g.to || f.offset > jump.offset) continue;
      if (f.offset == jump.offset) {
        straight = true;
        break;
      }
      if (f.unreached || writes_rcx(f) || f.cls == InsnClass::Jcc || f.cls == InsnClass::Jmp ||
          f.cls == InsnClass::Call || f.cls == InsnClass::Ret || f.cls == InsnClass::Int3) {
        break;
      }
    }
    if (!straight) continue;
    t.singleton_slot = probe.singleton_slot;
    t.hop = probe.hop;
    t.via_guard = true;
    return true;
  }
  return false;
}

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
  if ((rcx == jump->pass_src.end() || !singleton_of(rcx->source, t)) && !singleton_past_guard(wrapper, *jump, t)) {
    return failed("wrapper_rcx");
  }
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
    // Spike.9 (run 8's): a direct jump with the entry in rcx, to the group's own function.
    const auto rcx = std::find_if(c.pass_src.begin(), c.pass_src.end(), [](const PassedSource& p) { return p.reg == 1; });
    if (c.form == CallForm::Tail && c.reached && c.target && rcx != c.pass_src.end() &&
        same_source(rcx->source, *value)) {
      t.target = c.target;
      t.site = c.offset;
      break;
    }
  }
  t.matched = true;
  return t;
}

}  // namespace en::gal::probe

#endif
