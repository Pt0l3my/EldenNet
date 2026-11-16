#pragma once
// Internal to en-gal: reading code with the x86-64 decoder (x86.hpp) over an image read once (image_bytes.hpp), for the
// derivation kit and the FD4 finder (ARCH §6.2): instructions decoded one after another from a point where decoding can
// start (a function's entry in the exception directory, or the end of the function before), every instruction of a
// section, and how often patterns match in it.
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "en/gal/image_bytes.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/x86.hpp"

namespace en::gal::code {

struct Decoded {
  std::uint32_t rva = 0;
  x86::Instruction insn;

  [[nodiscard]] std::uint32_t end() const noexcept { return rva + insn.length; }
};

struct Stats {
  std::uint64_t instructions = 0;
  std::uint64_t undecodable = 0;  // bytes stepped over because nothing decoded there
};

// The instruction's bytes, as the copy holds them (empty if they do not lie in it).
[[nodiscard]] std::span<const std::byte> bytes_of(const SectionCopy& text, const Decoded& d) noexcept;

// The target of a RIP-relative operand, or of a relative branch, as an RVA within the image; nothing otherwise.
[[nodiscard]] std::optional<std::uint32_t> rip_target(const ImageBytes& img, const SectionCopy& text, const Decoded& d);
[[nodiscard]] std::optional<std::uint32_t> branch_target(const ImageBytes& img, const SectionCopy& text,
                                                         const Decoded& d);

// Where decoding can start for code at `rva`: the entry of its function in the exception directory, else the end of
// the function before it, else the start of the section.
[[nodiscard]] std::uint32_t sync_point(const ImageInfo& image, const SectionCopy& text, std::uint32_t rva) noexcept;

// The instructions of [begin, end), within the section, decoded one after another; a byte where nothing decodes is
// stepped over and counted. An instruction is never decoded across a function's entry: decoding starts again there.
[[nodiscard]] std::vector<Decoded> decode_range(const ImageInfo& image, const SectionCopy& text, std::uint32_t begin,
                                                std::uint32_t end, Stats* stats = nullptr);

// The instructions from the sync point of `rva` up to and including the one that starts at `rva`; nothing when
// decoding does not land on `rva`.
[[nodiscard]] std::optional<std::vector<Decoded>> decode_to(const ImageInfo& image, const SectionCopy& text,
                                                            std::uint32_t rva);

// Calls f(const Decoded&) for every instruction of the section, decoded as decode_range does.
template <class F>
void sweep(const ImageInfo& image, const SectionCopy& text, F&& f, Stats& stats);

// How often each pattern matches in the section.
[[nodiscard]] std::vector<std::uint64_t> count_matches(const SectionCopy& text, std::span<const BytePattern> patterns);

// The positions (RVAs) where one pattern matches, the first `max` of them, and how many there were.
struct Matches {
  std::vector<std::uint32_t> hits;
  std::uint64_t count = 0;
};
[[nodiscard]] Matches find_matches(const SectionCopy& text, const BytePattern& pattern, std::size_t max);

// ---- The template's body ----

namespace detail {
// One step of a sweep: the instruction at `at` (bounded by the next function's entry), or nothing.
std::optional<x86::Instruction> step(const SectionCopy& text, std::uint32_t at, std::uint32_t limit) noexcept;
}  // namespace detail

template <class F>
void sweep(const ImageInfo& image, const SectionCopy& text, F&& f, Stats& stats) {
  const std::uint32_t end = text.rva + static_cast<std::uint32_t>(text.bytes.size());
  auto next_fn = image.functions.begin();
  std::uint32_t at = text.rva;
  while (at < end) {
    while (next_fn != image.functions.end() && next_fn->begin <= at) ++next_fn;
    const std::uint32_t limit = next_fn != image.functions.end() && next_fn->begin < end ? next_fn->begin : end;
    const std::optional<x86::Instruction> insn = detail::step(text, at, limit);
    if (!insn) {
      ++stats.undecodable;
      ++at;
      continue;
    }
    ++stats.instructions;
    f(Decoded{at, *insn});
    at += insn->length;
  }
}

}  // namespace en::gal::code
