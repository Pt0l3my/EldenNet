// Shapes: recipes as runs of instruction forms, matched with our own decoder (shape.hpp; proposal 0005 §6.1; O102).
#include "en/gal/shape.hpp"

#include <algorithm>
#include <utility>

#include "code.hpp"
#include "en/gal/x86.hpp"

namespace en::gal::shape {

namespace {

constexpr std::array<std::pair<std::string_view, Form>, kFormCount> kWords = {{
    {"load_slot", Form::LoadSlot},
    {"test_loaded", Form::TestLoaded},
    {"branch_set_short", Form::BranchSetShort},
    {"branch_set_near", Form::BranchSetNear},
    {"address_arg1", Form::AddressArg1},
    {"call_relative", Form::CallRelative},
    {"skip_one", Form::SkipOne},
    {"compare_slot", Form::CompareSlot},
}};

// The architecture's encodings the forms name (its documented opcodes; no program's bytes).
constexpr std::uint8_t kOpLoad = 0x8B;         // move from register or memory to a register
constexpr std::uint8_t kOpTest = 0x85;         // test a register or memory with a register
constexpr std::uint8_t kOpAddress = 0x8D;      // the address of a memory operand into a register
constexpr std::uint8_t kOpBranchSet8 = 0x75;   // branch when the result was not zero, 8-bit distance
constexpr std::uint8_t kOpBranchSet32 = 0x85;  // the same in the two-byte map, 32-bit distance
constexpr std::uint8_t kOpCall = 0xE8;         // call, 32-bit distance
constexpr std::uint8_t kOpGroup1Imm8 = 0x83;   // an arithmetic group on register or memory with an 8-bit immediate
constexpr std::uint8_t kOpGroup1Imm32 = 0x81;  // the same with a 32-bit immediate
constexpr std::uint8_t kGroup1Compare = 7;     // the group's compare
constexpr std::uint8_t kArg1 = 1;              // the first argument's register in the Windows x64 convention (rcx)

// An instruction of the one-byte map with no prefix but REX.W: a plain 64-bit form.
bool plain_wide(const x86::Instruction& i, std::uint8_t opcode) noexcept {
  return i.encoding == x86::Encoding::Legacy && i.map == 0 && i.opcode == opcode && i.wide && i.rex != 0 &&
         i.prefixes == 1 && i.has_modrm;
}

bool no_prefix(const x86::Instruction& i) noexcept { return i.encoding == x86::Encoding::Legacy && i.prefixes == 0; }

// Whether the bytes at `at` could start the form at all: a cheap test before a decode, for the form find() starts
// with most often. True for a form it has no test for.
bool could_start(Form form, const SectionCopy& text, std::size_t at) noexcept {
  if (form != Form::LoadSlot && form != Form::CompareSlot) return true;
  if (at + 2 >= text.bytes.size()) return false;
  const auto b = [&](std::size_t k) { return std::to_integer<std::uint8_t>(text.bytes[at + k]); };
  // REX with W set, the load's opcode, and a ModRM that names a RIP-relative operand.
  if (form == Form::LoadSlot) return (b(0) & 0xF8) == 0x48 && b(1) == kOpLoad && (b(2) & 0xC7) == 0x05;
  // REX with W set, the group's opcode, and a ModRM that names the compare of a RIP-relative operand.
  return (b(0) & 0xF8) == 0x48 && (b(1) == kOpGroup1Imm8 || b(1) == kOpGroup1Imm32) &&
         b(2) == (0x05 | (kGroup1Compare << 3));
}

}  // namespace

const char* to_string(Form form) noexcept {
  for (const auto& [word, value] : kWords) {
    if (value == form) return word.data();
  }
  return "load_slot";
}

std::optional<Form> form_of(std::string_view word) noexcept {
  for (const auto& [name, value] : kWords) {
    if (name == word) return value;
  }
  return std::nullopt;
}

bool gives_place(Form form) noexcept {
  return form == Form::LoadSlot || form == Form::AddressArg1 || form == Form::CallRelative ||
         form == Form::CompareSlot;
}

bool Shape::valid() const noexcept {
  if (steps.empty() || steps.size() > kMaxSteps) return false;
  bool loaded = false;
  for (const Form f : steps) {
    if (f == Form::LoadSlot) loaded = true;
    if (f == Form::CompareSlot) loaded = false;          // a compare in memory leaves nothing in a register
    if (f == Form::TestLoaded && !loaded) return false;  // nothing to test yet
  }
  return true;
}

namespace {

// A load or a test of the one-byte map that is its form in all but its width: no 64-bit operand size.
bool plain_narrow(const x86::Instruction& i, std::uint8_t opcode) noexcept {
  return i.encoding == x86::Encoding::Legacy && i.map == 0 && i.opcode == opcode && !i.wide && i.has_modrm &&
         i.prefixes <= 1 && !i.lock && i.rep == 0 && i.segment == 0;
}

// The shape's steps at `rva`, one after another. On the first step that is not its form: `miss`, when given, says
// which and how, and the answer is nothing.
std::optional<Hit> walk(const ImageBytes& img, const SectionCopy& text, const Shape& shape, std::uint32_t rva,
                        Mismatch* miss) {
  const auto fail = [&](std::size_t step, Differs differs) -> std::optional<Hit> {
    if (miss != nullptr) *miss = Mismatch{static_cast<std::uint8_t>(step), differs};
    return std::nullopt;
  };
  const std::uint64_t section_end = std::uint64_t{text.rva} + text.bytes.size();
  if (rva < text.rva || section_end > 0xFFFFFFFFu) return fail(0, Differs::Undecodable);
  const auto limit = static_cast<std::uint32_t>(section_end);
  Hit hit;
  hit.rva = rva;
  std::uint32_t at = rva;
  std::optional<std::uint8_t> loaded;  // the register the last load wrote
  for (std::size_t k = 0; k < shape.steps.size() && k < kMaxSteps; ++k) {
    if (at >= limit) return fail(k, Differs::Undecodable);
    const std::optional<x86::Instruction> insn = code::detail::step(text, at, limit);
    if (!insn) return fail(k, Differs::Undecodable);
    const code::Decoded d{at, *insn};
    std::optional<std::uint32_t> place;
    switch (shape.steps[k]) {
      case Form::LoadSlot:
        if (!plain_wide(*insn, kOpLoad) || !insn->rip_relative || insn->disp_size != 4) {
          const bool narrow = plain_narrow(*insn, kOpLoad) && insn->rip_relative && insn->disp_size == 4;
          return fail(k, narrow ? Differs::Narrow : Differs::Other);
        }
        place = code::rip_target(img, text, d);
        if (!place) return fail(k, Differs::Other);
        loaded = insn->reg();
        break;
      case Form::TestLoaded:
        if (!loaded) return fail(k, Differs::Other);
        if (!plain_wide(*insn, kOpTest) || insn->mod() != 3 || insn->reg() != *loaded || insn->rm() != *loaded) {
          if (plain_wide(*insn, kOpTest) && insn->mod() == 3) return fail(k, Differs::OtherRegister);
          const bool narrow = plain_narrow(*insn, kOpTest) && insn->mod() == 3 && insn->reg() == insn->rm();
          return fail(k, narrow ? Differs::Narrow : Differs::Other);
        }
        break;
      case Form::BranchSetShort:
      case Form::BranchSetNear: {
        const bool near = shape.steps[k] == Form::BranchSetNear;
        if (!no_prefix(*insn) || insn->flow != x86::Flow::Branch || !insn->relative) return fail(k, Differs::Other);
        if (near ? (insn->map != 1 || insn->opcode != kOpBranchSet32 || insn->imm_size != 4)
                 : (insn->map != 0 || insn->opcode != kOpBranchSet8 || insn->imm_size != 1)) {
          return fail(k, Differs::Other);
        }
        const std::optional<std::uint32_t> to = code::branch_target(img, text, d);
        if (!to || *to <= d.end()) return fail(k, Differs::Other);  // forward, past the branch itself
        hit.branch_to = *to;
        break;
      }
      case Form::AddressArg1:
        if (!plain_wide(*insn, kOpAddress) || !insn->rip_relative || insn->disp_size != 4 || insn->reg() != kArg1) {
          return fail(k, Differs::Other);
        }
        place = code::rip_target(img, text, d);
        if (!place) return fail(k, Differs::Other);
        break;
      case Form::CallRelative:
        if (!no_prefix(*insn) || insn->flow != x86::Flow::Call || !insn->relative || insn->map != 0 ||
            insn->opcode != kOpCall || insn->imm_size != 4) {
          return fail(k, Differs::Other);
        }
        place = code::branch_target(img, text, d);
        if (!place) return fail(k, Differs::Other);
        break;
      case Form::CompareSlot: {
        const bool group = insn->encoding == x86::Encoding::Legacy && insn->map == 0 && insn->has_modrm &&
                           (insn->opcode == kOpGroup1Imm8 || insn->opcode == kOpGroup1Imm32) &&
                           ((insn->modrm >> 3) & 7) == kGroup1Compare && insn->rip_relative && insn->disp_size == 4;
        if (!group || insn->prefixes > 1 || insn->lock || insn->rep != 0 || insn->segment != 0) {
          return fail(k, Differs::Other);
        }
        if (!insn->wide) return fail(k, Differs::Narrow);
        // The immediate must be zero: a compare with anything else asks another question than "is it there".
        const std::size_t imm = static_cast<std::size_t>(at - text.rva) + insn->imm_at;
        if (insn->imm_size == 0 || imm + insn->imm_size > text.bytes.size()) return fail(k, Differs::Other);
        for (std::size_t b = 0; b < insn->imm_size; ++b) {
          if (text.bytes[imm + b] != std::byte{0}) return fail(k, Differs::Other);
        }
        place = code::rip_target(img, text, d);
        if (!place) return fail(k, Differs::Other);
        loaded.reset();  // nothing is in a register: a test_loaded after it has nothing to test
        break;
      }
      case Form::SkipOne: {
        // Any instruction that falls through. One that writes the remembered register by a load, an address or a
        // widening move would make the test that follows a test of something else: that is no accessor's.
        if (insn->flow != x86::Flow::None) return fail(k, Differs::Other);
        const bool one_byte_load = insn->map == 0 && (insn->opcode == kOpLoad || insn->opcode == kOpAddress);
        const bool widening = insn->map == 1 && (insn->opcode == 0xB6 || insn->opcode == 0xB7 ||
                                                 insn->opcode == 0xBE || insn->opcode == 0xBF);
        if (loaded && insn->has_modrm && (one_byte_load || widening) && insn->reg() == *loaded) {
          return fail(k, Differs::OtherRegister);
        }
        break;
      }
    }
    if (place) hit.gives[k] = *place;
    at = d.end();
  }
  hit.end = at;
  return hit;
}

}  // namespace

const char* to_string(Differs differs) noexcept {
  switch (differs) {
    case Differs::Narrow: return "narrow";
    case Differs::OtherRegister: return "other_register";
    case Differs::Other: return "other";
    case Differs::Undecodable: return "undecodable";
  }
  return "other";
}

std::optional<Hit> match_at(const ImageBytes& img, const SectionCopy& text, const Shape& shape, std::uint32_t rva) {
  if (!shape.valid()) return std::nullopt;
  return walk(img, text, shape, rva, nullptr);
}

std::optional<Mismatch> mismatch_at(const ImageBytes& img, const SectionCopy& text, const Shape& shape,
                                    std::uint32_t rva) {
  if (!shape.valid()) return std::nullopt;
  Mismatch miss;
  if (walk(img, text, shape, rva, &miss)) return std::nullopt;
  return miss;
}

Matches find(const ImageBytes& img, const SectionCopy& text, const Shape& shape, std::size_t max) {
  Matches out;
  if (!shape.valid()) return out;
  const Form first = shape.steps.front();
  for (std::size_t i = 0; i < text.bytes.size(); ++i) {
    if (!could_start(first, text, i)) continue;
    const std::uint64_t rva = std::uint64_t{text.rva} + i;
    if (rva > 0xFFFFFFFFu) break;
    const std::optional<Hit> hit = match_at(img, text, shape, static_cast<std::uint32_t>(rva));
    if (!hit) continue;
    ++out.count;
    if (out.hits.size() < max) out.hits.push_back(*hit);
  }
  return out;
}

}  // namespace en::gal::shape
