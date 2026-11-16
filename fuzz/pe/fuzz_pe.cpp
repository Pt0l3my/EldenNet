// libFuzzer target: en-gal's PE32+ reader (pe.hpp) and the pattern scan over what it reads (scan_patterns), on
// arbitrary bytes. The first byte picks the layout (bit 0: the file or the mapped image), whether the source refuses
// one 4 KiB page (bit 1: as a guarded copy meets an unreadable page in game) and the scan's chunk size (bit 2: 4 KiB or
// 1 MiB); the rest is the image. Besides any sanitizer report, a broken property is a finding:
// - what read_image accepts is consistent: sections in RVA order, aligned, disjoint and within SizeOfImage as the
//   loader rounds it; what a file holds of each (file_span) within the file and within its raw and mapped sizes; every
//   section's first and last byte placed where the layout says and read as loaded (zeros past what a file holds); the
//   section table read alone the same; the function table sorted, disjoint and within the image, each entry found by
//   function_at; the product name valid UTF-8;
// - a refusal names a problem describe() can word, a section's with its fault and in at most 192 characters;
// - the scan's results keep to their pattern's rules: a resolved function's hits lie in its function, at least
//   anchor_min past its entry; a resolved static's target lies in its section, aligned to 8; an idiom's kept hits'
//   targets lie in their sections.
// An exception that escapes is a finding too, so this entry has no ffi_guard (R3 is for the engine's entries).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

#include "en/gal/patterns.hpp"
#include "en/gal/pe.hpp"
#include "en/proto/fixed.hpp"

namespace {

using namespace en::gal;

void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

// A source that refuses every read touching one page.
class Holed final : public ByteSource {
 public:
  Holed(std::span<const std::byte> bytes, std::uint64_t page) : bytes_(bytes), page_(page) {}
  std::uint64_t size() const noexcept override { return bytes_.size(); }
  bool read(std::uint64_t offset, std::span<std::byte> dst) const noexcept override {
    if (offset > bytes_.size() || dst.size() > bytes_.size() - offset) return false;
    if (offset < page_ + 4096 && offset + dst.size() > page_) return false;
    std::copy_n(bytes_.subspan(static_cast<std::size_t>(offset)).begin(), dst.size(), dst.begin());
    return true;
  }

 private:
  std::span<const std::byte> bytes_;
  std::uint64_t page_;
};

// Invented patterns, one of each kind, over .text.
const PatternSet& patterns() {
  static const PatternSet* set = [] {
    auto* s = new PatternSet;
    PatternSpec fn;
    fn.id = "fuzz_fn";
    fn.kind = PatternKind::Function;
    fn.section = ".text";
    fn.bytes = *parse_pattern("5A 6B ?? 7C 8D");
    fn.fn_len_min = 1;
    fn.fn_len_max = 0x7FFFFFFF;
    fn.expect_max = 8;
    s->patterns.push_back(fn);
    PatternSpec st;
    st.id = "fuzz_static";
    st.kind = PatternKind::Static;
    st.section = ".text";
    st.bytes = *parse_pattern("C7 D1 E2 ?? ?? ?? ?? F3");
    st.capture = Capture{3, 7};
    st.target_section = ".data";
    st.expect_max = 8;
    s->patterns.push_back(st);
    PatternSpec idiom;
    idiom.id = "fuzz_idiom";
    idiom.kind = PatternKind::Idiom;
    idiom.section = ".text";
    idiom.bytes = *parse_pattern("D5 E6 F7 ?? ?? ?? ?? A8 1F ?? ?? ?? ??");
    idiom.expect_max = kMaxPatternHits;
    idiom.captures = {IdiomCapture{"slot", Capture{3, 7}, ".data", 8, false},
                      IdiomCapture{"getter", Capture{9, 13}, ".text", 1, true}};
    s->patterns.push_back(idiom);
    return s;
  }();
  return *set;
}

void check_image(const ImageInfo& image, const ByteSource& source, bool holed) {
  const std::uint64_t page_or_alignment = std::max<std::uint64_t>(image.section_alignment, 4096);
  const std::uint64_t image_end = (std::uint64_t{image.size_of_image} + page_or_alignment - 1) / page_or_alignment *
                                  page_or_alignment;
  std::uint64_t last_end = 0;
  for (const Section& s : image.sections) {
    require(s.rva >= last_end && s.rva >= image.size_of_headers, "sections in RVA order and disjoint");
    require(s.rva % image.section_alignment == 0, "sections aligned");
    require(std::uint64_t{s.rva} + s.mapped_size() <= image_end, "sections within SizeOfImage, rounded");
    require(s.raw_size == 0 || std::uint64_t{s.raw_offset} + s.raw_size <= 0xFFFFFFFFu, "no raw range wraps");
    const FileSpan held = image.file_span(s);
    if (image.layout == ImageLayout::File) {
      require(held.offset + held.size <= image.source_size, "what a file holds of a section lies within the file");
      require(held.size <= s.raw_size && held.size <= s.mapped_size(), "and within its raw and mapped sizes");
    } else {
      require(held == FileSpan{}, "a mapped image holds no file span");
    }
    last_end = std::uint64_t{s.rva} + s.mapped_size();
    if (s.mapped_size() > 0) {
      const auto at = image.offset_of(s.rva, 1);
      if (image.layout == ImageLayout::Memory) require(at == s.rva, "a mapped section's bytes at its RVA");
      if (image.layout == ImageLayout::File) {
        require(held.size > 0 ? at == held.offset : !at.has_value(), "a file's at what it holds of the section");
      }
      for (const std::uint32_t rva : {s.rva, s.rva + s.mapped_size() - 1}) {
        std::array<std::byte, 1> b{std::byte{0x5A}};
        const bool read = read_mapped(image, source, rva, b);
        require(read || holed, "a section's bytes read as loaded");
        if (read && image.layout == ImageLayout::File && rva - s.rva >= held.size) {
          require(b[0] == std::byte{0}, "zeros past what a file holds");
        }
      }
    }
  }
  const auto table = read_section_table(source);
  require(table.has_value() && table->sections == image.sections && table->size_of_image == image.size_of_image,
          "the section table read alone is the image's");
  std::uint32_t previous_end = 0;
  for (std::size_t i = 0; i < image.functions.size(); ++i) {
    const RuntimeFunction& f = image.functions[i];
    require(f.begin < f.end && f.end <= image.size_of_image && f.begin >= previous_end, "the function table's order");
    previous_end = f.end;
    if (i < 64) {
      require(image.function_at(f.begin) == &image.functions[i], "function_at finds an entry's first byte");
      require(image.function_at(f.end - 1) == &image.functions[i], "and its last");
      (void)is_fragment(image, source, f);
    }
  }
  if (image.version && image.version->product_name) {
    require(en::proto::valid_utf8(*image.version->product_name), "the product name is UTF-8");
  }
}

void check_scan(const ImageInfo& image, const ScanReport& report) {
  const PatternSet& set = patterns();
  require(report.results.size() == set.patterns.size(), "a result for every pattern");
  for (std::size_t i = 0; i < set.patterns.size(); ++i) {
    const PatternSpec& spec = set.patterns[i];
    const PatternResult& r = report.results[i];
    require(r.hits.size() <= kMaxPatternHits && r.hits.size() <= r.count, "hits kept within the cap and the count");
    if (!r.resolved()) continue;
    require(r.count >= spec.expect_min && r.count <= spec.expect_max, "a resolved pattern matched as expected");
    if (spec.kind == PatternKind::Function) {
      for (const std::uint32_t hit : r.hits) {
        const RuntimeFunction* f = image.function_at(hit);
        require(f != nullptr && f->begin == r.entry && hit - f->begin >= spec.anchor_min, "a function's hit inside it");
      }
    } else if (spec.kind == PatternKind::Static) {
      const Section* data = image.section(spec.target_section);
      require(data != nullptr && data->contains(r.target) && r.target % 8 == 0, "a static's target in .data, aligned");
    } else {
      for (const IdiomHit& h : r.idiom_hits) {
        require(h.targets.size() == spec.captures.size(), "an idiom hit has every capture's target");
        for (std::size_t c = 0; c < h.targets.size(); ++c) {
          const Section* where = image.section(spec.captures[c].section);
          require(where != nullptr && where->contains(h.targets[c]), "an idiom capture's target in its section");
        }
      }
    }
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size < 1) return 0;
  const std::uint8_t mode = data[0];
  // SAFETY: libFuzzer hands over `size` readable bytes, which live until this call returns; the span reads them as
  // the bytes they are, and ends where they end.
  const std::span<const std::byte> image_bytes =
      std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size).subspan(1);
  const ImageLayout layout = (mode & 1) != 0 ? ImageLayout::Memory : ImageLayout::File;
  const SpanSource whole(image_bytes);
  const Holed holed(image_bytes, static_cast<std::uint64_t>((mode >> 3) & 0x1F) * 4096);
  const ByteSource& source = (mode & 2) != 0 ? static_cast<const ByteSource&>(holed) : whole;

  const auto image = read_image(source, layout);
  if (!image) {
    const PeError& e = image.error();
    const std::string text = describe(e);
    require(!text.empty(), "a refusal is worded");
    require(e.section.has_value() == (e.problem == PeProblem::BadSection), "a section's refusal has its fault");
    require(!e.section || text.size() <= 192, "a section's refusal in at most 192 characters");
    return 0;
  }
  check_image(*image, source, (mode & 2) != 0);
  const ScanReport report = scan_patterns(*image, source, patterns(), ScanOptions{(mode & 4) != 0 ? 4096u : 1u << 20});
  check_scan(*image, report);
  return 0;
}
