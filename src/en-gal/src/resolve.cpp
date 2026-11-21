// Lodestar's resolver: placement and witnesses, checked at every start (resolve.hpp; proposal 0005 §7.1, §9.1).
#include "en/gal/resolve.hpp"

#include <algorithm>
#include <array>
#include <optional>

#include "en/gal/x86.hpp"

namespace en::gal {

using proto::Expected;
using proto::Unexpected;

const char* to_string(ItemProblem problem) noexcept {
  switch (problem) {
    case ItemProblem::None: return "none";
    case ItemProblem::Unknown: return "unknown";
    case ItemProblem::Kind: return "kind";
    case ItemProblem::Section: return "section";
    case ItemProblem::Alignment: return "alignment";
    case ItemProblem::NotFunctionStart: return "not_function_start";
    case ItemProblem::Length: return "length";
    case ItemProblem::Extent: return "extent";
    case ItemProblem::WitnessOutsideCode: return "witness_outside_code";
    case ItemProblem::WitnessUnreadable: return "witness_unreadable";
    case ItemProblem::WitnessUndecodable: return "witness_undecodable";
    case ItemProblem::WitnessElsewhere: return "witness_elsewhere";
  }
  return "none";
}

ItemCheck check_of(ItemProblem problem) noexcept {
  switch (problem) {
    case ItemProblem::WitnessOutsideCode:
    case ItemProblem::WitnessUnreadable:
    case ItemProblem::WitnessUndecodable:
    case ItemProblem::WitnessElsewhere: return ItemCheck::Witness;
    default: return ItemCheck::Placement;
  }
}

namespace {

constexpr std::uint32_t kPointer = 8;

bool has(const Section& s, std::uint32_t bits) noexcept { return (s.characteristics & bits) == bits; }
bool lacks(const Section& s, std::uint32_t bits) noexcept { return (s.characteristics & bits) == 0; }

// Whether [rva, rva + len) lies within the section's mapped bytes.
bool within(const Section& s, std::uint32_t rva, std::uint64_t len) noexcept {
  return s.contains(rva) && len <= std::uint64_t{s.mapped_size()} - (rva - s.rva);
}

// Placement: the value lies where its kind must (proposal 0005 §7.1).
ItemProblem placement(const ImageInfo& image, const ItemEntry& item) noexcept {
  switch (item.kind) {
    case IdKind::Static: {
      const Section* s = image.section_at(item.rva);
      if (s == nullptr || !has(*s, kSectionRead | kSectionWrite) || !lacks(*s, kSectionExecute)) {
        return ItemProblem::Section;
      }
      return within(*s, item.rva, kPointer) ? ItemProblem::None : ItemProblem::Extent;
    }
    case IdKind::Function: {
      const Section* s = image.section_at(item.rva);
      if (s == nullptr || !has(*s, kSectionExecute)) return ItemProblem::Section;
      const RuntimeFunction* f = image.function_at(item.rva);
      if (f == nullptr || f->begin != item.rva) return ItemProblem::NotFunctionStart;
      return f->length() == item.length ? ItemProblem::None : ItemProblem::Length;
    }
    case IdKind::Table: {
      const Section* s = image.section_at(item.rva);
      if (s == nullptr || !has(*s, kSectionRead) || !lacks(*s, kSectionExecute)) return ItemProblem::Section;
      return within(*s, item.rva, std::uint64_t{item.stride} * item.count) ? ItemProblem::None : ItemProblem::Extent;
    }
    case IdKind::Class: {
      if (item.vtables.empty()) return ItemProblem::Section;
      for (const std::uint32_t v : item.vtables) {
        const Section* s = image.section_at(v);
        if (s == nullptr || !has(*s, kSectionRead) || !lacks(*s, kSectionWrite | kSectionExecute)) {
          return ItemProblem::Section;
        }
        if (v % kPointer != 0) return ItemProblem::Alignment;
        if (!within(*s, v, kPointer)) return ItemProblem::Extent;
      }
      return ItemProblem::None;
    }
  }
  return ItemProblem::Section;
}

// One witness: the instruction at `at` must refer to the item's place. Only the instruction's own bytes are read.
ItemProblem witness(const ImageInfo& image, const ByteSource& source, std::uint32_t at, const ItemEntry& item) noexcept {
  const Section* s = image.section_at(at);
  if (s == nullptr || !has(*s, kSectionExecute)) return ItemProblem::WitnessOutsideCode;
  std::array<std::byte, x86::kMaxLength> buffer{};
  const std::uint64_t room = std::uint64_t{s->mapped_size()} - (at - s->rva);  // at least 1: the section holds `at`
  const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(room, buffer.size()));
  const std::span<std::byte> bytes = std::span<std::byte>(buffer).first(n);
  if (!read_mapped(image, source, at, bytes)) return ItemProblem::WitnessUnreadable;
  const auto insn = x86::decode(bytes);
  if (!insn.has_value()) return ItemProblem::WitnessUndecodable;
  // With the instruction's own offset as its address, a target is an offset from the start of the image too.
  const std::optional<std::uint64_t> operand = x86::rip_target(*insn, bytes, at);
  if (operand && *operand == item.rva) return ItemProblem::None;
  if (item.kind == IdKind::Function && (insn->flow == x86::Flow::Call || insn->flow == x86::Flow::Jump)) {
    const std::optional<std::uint64_t> target = x86::branch_target(*insn, bytes, at);
    if (target && *target == item.rva) return ItemProblem::None;
  }
  return ItemProblem::WitnessElsewhere;
}

}  // namespace

ItemState check_item(const ImageInfo& image, const ByteSource& source, const ItemEntry& item) noexcept {
  ItemState state;
  state.id = item.id;
  state.problem = placement(image, item);
  if (!state.ok()) return state;
  for (std::size_t i = 0; i < item.witnesses.size() && i < kMaxWitnesses; ++i) {
    const ItemProblem problem = witness(image, source, item.witnesses[i], item);
    if (problem != ItemProblem::None) {
      state.problem = problem;
      state.witness = static_cast<std::uint8_t>(i);
      return state;
    }
    ++state.witnesses;
  }
  return state;
}

Library::Library(const Layout& layout, const ImageInfo& image, const ByteSource& source) {
  states_.reserve(layout.items().size());
  slots_.reserve(layout.items().size());
  for (const ItemEntry& item : layout.items()) {
    states_.push_back(check_item(image, source, item));
    slots_.push_back(Slot{item.kind, item.rva, item.vtables});
  }
}

std::size_t Library::index_of(std::uint32_t id) const noexcept {
  for (std::size_t i = 0; i < states_.size(); ++i) {
    if (states_[i].id == id) return i;
  }
  return states_.size();
}

const ItemState* Library::state(std::uint32_t id) const noexcept {
  const std::size_t i = index_of(id);
  return i < states_.size() ? &states_[i] : nullptr;
}

std::size_t Library::refused() const noexcept {
  return static_cast<std::size_t>(
      std::count_if(states_.begin(), states_.end(), [](const ItemState& s) { return !s.ok(); }));
}

Expected<std::uint32_t, ItemProblem> Library::place(std::uint32_t id, IdKind kind) const noexcept {
  const std::size_t i = index_of(id);
  if (i >= states_.size() || i >= slots_.size()) return Unexpected{ItemProblem::Unknown};
  if (slots_[i].kind != kind || kind == IdKind::Class) return Unexpected{ItemProblem::Kind};
  if (!states_[i].ok()) return Unexpected{states_[i].problem};
  return slots_[i].rva;
}

std::span<const std::uint32_t> Library::vtables(std::uint32_t id, ItemProblem* problem) const noexcept {
  const auto refuse = [&](ItemProblem why) {
    if (problem != nullptr) *problem = why;
    return std::span<const std::uint32_t>{};
  };
  const std::size_t i = index_of(id);
  if (i >= states_.size() || i >= slots_.size()) return refuse(ItemProblem::Unknown);
  if (slots_[i].kind != IdKind::Class) return refuse(ItemProblem::Kind);
  if (!states_[i].ok()) return refuse(states_[i].problem);
  if (problem != nullptr) *problem = ItemProblem::None;
  return slots_[i].vtables;
}

}  // namespace en::gal
