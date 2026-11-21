// Reading code with the decoder (code.hpp).
#include "code.hpp"

#include <algorithm>

namespace en::gal::code {

std::span<const std::byte> bytes_of(const SectionCopy& text, const Decoded& d) noexcept {
  if (d.rva < text.rva) return {};
  const std::uint64_t off = d.rva - text.rva;
  if (off > text.bytes.size() || d.insn.length > text.bytes.size() - off) return {};
  return std::span<const std::byte>(text.bytes).subspan(static_cast<std::size_t>(off), d.insn.length);
}

namespace {

std::optional<std::uint32_t> as_rva(const ImageBytes& img, std::optional<std::uint64_t> target) {
  if (!target || *target >= img.image().size_of_image) return std::nullopt;
  return static_cast<std::uint32_t>(*target);
}

}  // namespace

std::optional<std::uint32_t> rip_target(const ImageBytes& img, const SectionCopy& text, const Decoded& d) {
  return as_rva(img, x86::rip_target(d.insn, bytes_of(text, d), d.rva));
}

std::optional<std::uint32_t> branch_target(const ImageBytes& img, const SectionCopy& text, const Decoded& d) {
  return as_rva(img, x86::branch_target(d.insn, bytes_of(text, d), d.rva));
}

std::uint32_t sync_point(const ImageInfo& image, const SectionCopy& text, std::uint32_t rva) noexcept {
  const auto& fns = image.functions;
  // The last function whose entry is at or before rva.
  const auto it = std::upper_bound(fns.begin(), fns.end(), rva,
                                   [](std::uint32_t r, const RuntimeFunction& f) { return r < f.begin; });
  std::uint32_t from = text.rva;
  if (it != fns.begin()) {
    const RuntimeFunction& f = *(it - 1);
    from = rva < f.end ? f.begin : f.end;
  }
  return std::max(from, text.rva);
}

namespace detail {

std::optional<x86::Instruction> step(const SectionCopy& text, std::uint32_t at, std::uint32_t limit) noexcept {
  if (at < text.rva || limit <= at) return std::nullopt;
  const std::uint64_t off = at - text.rva;
  if (off >= text.bytes.size()) return std::nullopt;
  const std::uint64_t n = std::min<std::uint64_t>({x86::kMaxLength, limit - at, text.bytes.size() - off});
  const auto in = x86::decode(
      std::span<const std::byte>(text.bytes).subspan(static_cast<std::size_t>(off), static_cast<std::size_t>(n)));
  if (!in) return std::nullopt;
  return *in;
}

}  // namespace detail

std::vector<Decoded> decode_range(const ImageInfo& image, const SectionCopy& text, std::uint32_t begin,
                                  std::uint32_t end, Stats* stats) {
  std::vector<Decoded> out;
  const std::uint32_t section_end = text.rva + static_cast<std::uint32_t>(text.bytes.size());
  end = std::min(end, section_end);
  if (begin < text.rva) begin = text.rva;
  auto next_fn = std::upper_bound(image.functions.begin(), image.functions.end(), begin,
                                  [](std::uint32_t r, const RuntimeFunction& f) { return r < f.begin; });
  std::uint32_t at = begin;
  while (at < end) {
    while (next_fn != image.functions.end() && next_fn->begin <= at) ++next_fn;
    const std::uint32_t limit =
        next_fn != image.functions.end() && next_fn->begin < section_end ? next_fn->begin : section_end;
    const std::optional<x86::Instruction> insn = detail::step(text, at, limit);
    if (!insn) {
      if (stats != nullptr) ++stats->undecodable;
      ++at;
      continue;
    }
    if (stats != nullptr) ++stats->instructions;
    out.push_back(Decoded{at, *insn});
    at += insn->length;
  }
  return out;
}

std::optional<std::vector<Decoded>> decode_to(const ImageInfo& image, const SectionCopy& text, std::uint32_t rva) {
  const std::uint32_t from = sync_point(image, text, rva);
  std::vector<Decoded> out = decode_range(image, text, from, rva + 1);
  if (out.empty() || out.back().rva != rva) return std::nullopt;
  return out;
}

std::vector<std::uint64_t> count_matches(const SectionCopy& text, std::span<const BytePattern> patterns) {
  MultiMatcher matcher(patterns, 1);
  matcher.begin(text.rva);
  matcher.feed(text.rva, text.bytes, true);
  matcher.finish(text.rva + text.bytes.size());
  std::vector<std::uint64_t> counts(patterns.size(), 0);
  for (std::size_t i = 0; i < patterns.size(); ++i) counts[i] = matcher.count(i);
  return counts;
}

Matches find_matches(const SectionCopy& text, const BytePattern& pattern, std::size_t max) {
  const std::span<const BytePattern> one(&pattern, 1);
  MultiMatcher matcher(one, max);
  matcher.begin(text.rva);
  matcher.feed(text.rva, text.bytes, true);
  matcher.finish(text.rva + text.bytes.size());
  Matches out;
  out.count = matcher.count(0);
  for (const std::uint64_t h : matcher.hits(0)) out.hits.push_back(static_cast<std::uint32_t>(h));
  return out;
}

}  // namespace en::gal::code
