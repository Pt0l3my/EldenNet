// Cutting patterns from decoded code (cut.hpp).
#include "cut.hpp"

#include <algorithm>

namespace en::gal::cut {

std::vector<Field> fields_of(const x86::Instruction& insn) {
  std::vector<Field> out;
  if (insn.disp_size != 0) out.push_back(Field{insn.disp_at, insn.disp_size});
  if (insn.imm_size != 0) out.push_back(Field{insn.imm_at, insn.imm_size});
  if (insn.imm2_size != 0) out.push_back(Field{insn.imm2_at, insn.imm2_size});
  return out;
}

namespace {

void wildcard(BytePattern& p, std::uint64_t at, std::uint64_t size) {
  for (std::uint64_t i = at; i < at + size && i < p.value.size(); ++i) {
    p.value[static_cast<std::size_t>(i)] = 0;
    p.mask[static_cast<std::size_t>(i)] = 0;
  }
}

}  // namespace

BytePattern pattern_of(const ImageBytes& img, const SectionCopy& text, std::span<const Fixup> fixups,
                       std::span<const code::Decoded> insns) {
  BytePattern p;
  if (insns.empty()) return p;
  const std::uint32_t start = insns.front().rva;
  const std::uint32_t end = insns.back().end();
  for (const code::Decoded& d : insns) {
    const std::span<const std::byte> b = code::bytes_of(text, d);
    const std::uint64_t base = p.value.size();
    for (const std::byte x : b) {
      p.value.push_back(std::to_integer<std::uint8_t>(x));
      p.mask.push_back(0xFF);
    }
    const x86::Instruction& in = d.insn;
    if (in.rip_relative || in.absolute || in.disp_size == 4) wildcard(p, base + in.disp_at, in.disp_size);
    if (in.relative) {
      const std::optional<std::uint32_t> t = code::branch_target(img, text, d);
      if (!t || *t < start || *t > end) wildcard(p, base + in.imm_at, in.imm_size);
    } else if (in.imm_size == 8) {
      wildcard(p, base + in.imm_at, in.imm_size);
    } else if (in.imm_size == 2 || in.imm_size == 4) {
      const std::optional<std::int64_t> v = x86::immediate(in, b);
      if (!v || *v < -128 || *v > 127) wildcard(p, base + in.imm_at, in.imm_size);
    }
  }
  // The bytes a base relocation covers, which the loader rewrites.
  const auto first = std::lower_bound(fixups.begin(), fixups.end(), start >= 8 ? start - 8 : 0,
                                      [](const Fixup& f, std::uint32_t r) { return f.rva < r; });
  for (auto it = first; it != fixups.end() && it->rva < end; ++it) {
    const std::uint64_t from = std::max<std::uint64_t>(it->rva, start);
    const std::uint64_t to = std::min<std::uint64_t>(std::uint64_t{it->rva} + it->size, end);
    if (from < to) wildcard(p, from - start, to - from);
  }
  return p;
}

std::optional<Cut> cut(const ImageBytes& img, const SectionCopy& text, std::span<const Fixup> fixups,
                       const Request& request) {
  constexpr std::size_t kMaxBytes = 64;
  constexpr std::size_t kStarts = 12;
  const std::vector<code::Decoded> insns = code::decode_range(img.image(), text, request.fn_begin, request.fn_end);
  std::optional<std::size_t> covered;
  if (request.cover) {
    for (std::size_t i = 0; i < insns.size(); ++i) {
      if (insns[i].rva == *request.cover) covered = i;
    }
    if (!covered) return std::nullopt;
  }
  struct Candidate {
    std::size_t first = 0;
    std::size_t last = 0;
    BytePattern bytes;
  };
  std::vector<Candidate> candidates;
  std::size_t starts = 0;
  for (std::size_t s = 0; s < insns.size(); ++s) {
    if (insns[s].rva < request.fn_begin + request.anchor_min) continue;
    if (covered && (s > *covered || insns[*covered].rva - insns[s].rva > 48)) continue;
    if (!covered && starts >= kStarts) break;
    ++starts;
    for (std::size_t e = s; e < insns.size(); ++e) {
      if (insns[e].end() - insns[s].rva > kMaxBytes || insns[e].end() > request.fn_end) break;
      if (covered && e < *covered) continue;
      BytePattern p = pattern_of(img, text, fixups, std::span<const code::Decoded>(insns).subspan(s, e - s + 1));
      if (p.size() >= 8 && p.fixed() >= 6) candidates.push_back(Candidate{s, e, std::move(p)});
    }
  }
  if (candidates.empty()) return std::nullopt;
  std::vector<BytePattern> patterns;
  patterns.reserve(candidates.size());
  for (const Candidate& c : candidates) patterns.push_back(c.bytes);
  const std::vector<std::uint64_t> counts = code::count_matches(text, patterns);

  std::optional<std::size_t> best;
  const auto comfortable = [](const BytePattern& p) { return p.size() >= 20 && p.fixed() >= 12; };
  const auto better = [&](std::size_t a, std::size_t b) {  // whether candidate a is better than b
    const Candidate& x = candidates[a];
    const Candidate& y = candidates[b];
    if (comfortable(x.bytes) != comfortable(y.bytes)) return comfortable(x.bytes);
    if (x.bytes.size() != y.bytes.size()) return x.bytes.size() < y.bytes.size();
    return covered ? x.first > y.first : x.first < y.first;
  };
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    if (counts[i] != 1) continue;
    if (!best || better(i, *best)) best = i;
  }
  if (!best) return std::nullopt;
  const Candidate& c = candidates[*best];
  Cut out;
  out.bytes = c.bytes;
  out.start = insns[c.first].rva;
  out.matches = 1;
  if (covered && insns[*covered].insn.rip_relative) {
    const std::uint32_t into = insns[*covered].rva - out.start;
    out.capture = Capture{into + insns[*covered].insn.disp_at, insns[*covered].end() - out.start};
  }
  return out;
}

}  // namespace en::gal::cut
