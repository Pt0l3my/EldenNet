// ARCH §6.2's rules over a FakeImage (scan_patterns, patterns.hpp): expect_matches, interior anchors, the function's
// length from .pdata and its prologue, fragments, RIP-relative statics in their section and aligned to 8, idioms whose
// captures land where they must, and unreadable pages. The code bytes, functions and patterns are invented.
#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/patterns.hpp"
#include "en/gal/pe.hpp"
#include "fake_image.hpp"

using namespace en::gal;
using namespace en::gal::test;

namespace {

constexpr std::uint32_t kText = 0x1000;  // the RVAs FakeImage gives these sections, checked in every scene
constexpr std::uint32_t kRdata = 0x4000;
constexpr std::uint32_t kData = 0x5000;

constexpr std::string_view kFn = "5A 6B ?? 7C 8D 9E";
constexpr std::string_view kStatic = "C7 D1 E2 ?? ?? ?? ?? F3";
constexpr std::string_view kIdiom = "D5 E6 F7 ?? ?? ?? ?? A8 1F ?? ?? ?? ??";

struct Scene {
  FakeImageSpec spec;

  Scene() {
    spec.sections.push_back(FakeSection{".text", kCode, std::vector<std::uint8_t>(0x3000, 0x00), 0});
    spec.sections.push_back(FakeSection{".rdata", kReadOnly, std::vector<std::uint8_t>(0x100, 0x00), 0});
    spec.sections.push_back(FakeSection{".data", kReadWrite, std::vector<std::uint8_t>(0x100, 0x00), 0x800});
    spec.version = FakeVersion{};
    spec.functions = {{0x000, 0x100, false, false},  {0x100, 0x300, false, false},   {0x300, 0x310, false, false},
                      {0x400, 0x500, true, false},   {0x600, 0x800, false, false},   {0x900, 0xC00, false, false},
                      {0xC00, 0x1000, false, false}, {0x1000, 0x2000, false, false}, {0x2000, 0x2800, false, false}};
  }

  std::vector<std::uint8_t>& text() { return spec.sections[0].bytes; }

  void plant(std::uint32_t at, std::string_view bytes) {
    const auto p = parse_pattern(bytes);
    REQUIRE(p.has_value());
    for (std::size_t i = 0; i < p->size(); ++i) {
      if (p->mask[i] != 0) text().at(at + i) = p->value[i];
    }
  }

  // The displacement at text offset `at`, for an instruction ending at text offset `end`, aimed at `target`.
  void aim(std::uint32_t at, std::uint32_t end, std::int64_t target) {
    const std::int64_t disp = target - (std::int64_t{kText} + end);
    for (std::size_t i = 0; i < 4; ++i)
      text().at(at + i) = static_cast<std::uint8_t>(static_cast<std::uint64_t>(disp) >> (8 * i));
  }

  BuiltImage build_checked() {
    BuiltImage built = build(spec);
    REQUIRE(built.rva.at(".text") == kText);
    REQUIRE(built.rva.at(".rdata") == kRdata);
    REQUIRE(built.rva.at(".data") == kData);
    return built;
  }
};

PatternSpec function_spec(std::string id, std::uint32_t lo = 0x100, std::uint32_t hi = 0x400) {
  PatternSpec s;
  s.id = std::move(id);
  s.kind = PatternKind::Function;
  s.section = ".text";
  s.bytes = *parse_pattern(kFn);
  s.fn_len_min = lo;
  s.fn_len_max = hi;
  return s;
}

PatternSpec static_spec(std::string id) {
  PatternSpec s;
  s.id = std::move(id);
  s.kind = PatternKind::Static;
  s.section = ".text";
  s.bytes = *parse_pattern(kStatic);
  s.capture = Capture{3, 7};
  s.target_section = ".data";
  return s;
}

PatternResult scan_one(const BuiltImage& built, const PatternSpec& spec, ImageLayout layout = ImageLayout::Memory,
                       std::uint32_t chunk = 1u << 20) {
  const std::vector<std::byte>& bytes = layout == ImageLayout::Memory ? built.memory : built.file;
  const SpanSource source(bytes);
  const auto image = read_image(source, layout);
  REQUIRE(image.has_value());
  PatternSet set;
  set.patterns.push_back(spec);
  const ScanReport report = scan_patterns(*image, source, set, ScanOptions{chunk});
  REQUIRE(report.results.size() == 1);
  return report.results[0];
}

}  // namespace

TEST_CASE("a function's pattern resolves to its function's entry and length, from the file and from memory") {
  Scene scene;
  scene.plant(0x120, kFn);
  const BuiltImage built = scene.build_checked();
  for (const ImageLayout layout : {ImageLayout::Memory, ImageLayout::File}) {
    const PatternResult r = scan_one(built, function_spec("invented_fn"), layout);
    CHECK(r.resolved());
    CHECK(r.count == 1);
    CHECK(r.hits == std::vector<std::uint32_t>{kText + 0x120});
    CHECK(r.entry == kText + 0x100);
    CHECK(r.length == 0x200);
  }
}

TEST_CASE("a function's hit must sit at entry + anchor_min or later, and end within its function") {
  Scene near;
  near.plant(0x102, kFn);  // two bytes into its function: where other mods' hooks sit
  PatternResult r = scan_one(near.build_checked(), function_spec("invented_fn"));
  CHECK(r.problem == HitProblem::NearEntry);
  CHECK(r.problem_at == kText + 0x102);
  Scene five;
  five.plant(0x105, kFn);  // exactly entry + 5
  CHECK(scan_one(five.build_checked(), function_spec("invented_fn")).resolved());
  Scene tail;
  tail.plant(0x30D, kFn);  // runs past its function's end at 0x310
  CHECK(scan_one(tail.build_checked(), function_spec("invented_fn", 1, 0x400)).problem == HitProblem::PastEnd);
}

TEST_CASE("a function's length must lie within fn_len, and its entry hold the prologue") {
  Scene small;
  small.plant(0x305, kFn);  // in a function of 0x10 bytes
  CHECK(scan_one(small.build_checked(), function_spec("invented_fn")).problem == HitProblem::Length);
  CHECK(scan_one(small.build_checked(), function_spec("invented_fn", 0x10, 0x10)).resolved());

  Scene with_prologue;
  with_prologue.plant(0x120, kFn);
  with_prologue.text()[0x100] = 0x3C;
  with_prologue.text()[0x101] = 0x4D;
  const BuiltImage built = with_prologue.build_checked();
  PatternSpec spec = function_spec("invented_fn");
  spec.prologue = {0x3C, 0x4D};
  CHECK(scan_one(built, spec).resolved());
  spec.prologue = {0x3C, 0x4E};
  CHECK(scan_one(built, spec).problem == HitProblem::Prologue);
}

TEST_CASE("a hit outside every function, or in a fragment of one, is refused") {
  Scene gap;
  gap.plant(0x850, kFn);  // no function holds 0x800 to 0x900
  CHECK(scan_one(gap.build_checked(), function_spec("invented_fn")).problem == HitProblem::NoFunction);
  Scene fragment;
  fragment.plant(0x420, kFn);  // in the chained fragment
  CHECK(scan_one(fragment.build_checked(), function_spec("invented_fn")).problem == HitProblem::Fragment);
}

TEST_CASE("a pattern must match exactly expect_matches times, and its hits agree") {
  Scene twice;
  twice.plant(0x120, kFn);
  twice.plant(0x620, kFn);
  const BuiltImage built = twice.build_checked();
  PatternResult r = scan_one(built, function_spec("invented_fn"));
  CHECK(r.problem == HitProblem::Count);
  CHECK(r.count == 2);
  PatternSpec two = function_spec("invented_fn");
  two.expect_min = two.expect_max = 2;
  CHECK(scan_one(built, two).problem == HitProblem::Ambiguous);  // two functions
  Scene same;
  same.plant(0x120, kFn);
  same.plant(0x180, kFn);
  r = scan_one(same.build_checked(), two);
  CHECK(r.resolved());
  CHECK(r.entry == kText + 0x100);
  Scene none;
  CHECK(scan_one(none.build_checked(), function_spec("invented_fn")).problem == HitProblem::Count);
}

TEST_CASE("a pattern whose section the image lacks resolves to nothing") {
  Scene scene;
  PatternSpec spec = function_spec("invented_fn");
  spec.section = ".nosuch";
  CHECK(scan_one(scene.build_checked(), spec).problem == HitProblem::NoSection);
}

TEST_CASE("a static's displacement must reach target_section, aligned to 8") {
  const auto with_target = [](std::int64_t target) {
    Scene scene;
    scene.plant(0x700, kStatic);
    scene.aim(0x703, 0x707, target);
    return scene.build_checked();
  };
  PatternResult r = scan_one(with_target(kData + 0x40), static_spec("invented_static"));
  CHECK(r.resolved());
  CHECK(r.target == kData + 0x40);
  r = scan_one(with_target(kData + 0x40), static_spec("invented_static"), ImageLayout::File);
  CHECK(r.target == kData + 0x40);
  CHECK(scan_one(with_target(kData + 0x7F8), static_spec("invented_static"))
            .resolved());  // mapped, past the file's bytes
  CHECK(scan_one(with_target(kData + 0x44), static_spec("invented_static")).problem == HitProblem::TargetAlignment);
  CHECK(scan_one(with_target(kRdata + 0x10), static_spec("invented_static")).problem == HitProblem::TargetSection);
  CHECK(scan_one(with_target(kData + 0x800), static_spec("invented_static")).problem == HitProblem::TargetSection);
  CHECK(scan_one(with_target(-0x2000), static_spec("invented_static")).problem == HitProblem::TargetSection);
}

TEST_CASE("a static's hits must agree on their target") {
  Scene scene;
  scene.plant(0x700, kStatic);
  scene.aim(0x703, 0x707, kData + 0x40);
  scene.plant(0xA00, kStatic);
  scene.aim(0xA03, 0xA07, kData + 0x40);
  PatternSpec two = static_spec("invented_static");
  two.expect_min = two.expect_max = 2;
  CHECK(scan_one(scene.build_checked(), two).resolved());
  scene.aim(0xA03, 0xA07, kData + 0x48);
  CHECK(scan_one(scene.build_checked(), two).problem == HitProblem::Ambiguous);
}

TEST_CASE("an idiom keeps the hits whose captures land where they must") {
  Scene scene;
  const std::uint32_t at[] = {0xC10, 0xC40, 0xC70, 0xCA0};
  for (const std::uint32_t a : at) scene.plant(a, kIdiom);
  scene.aim(0xC10 + 3, 0xC10 + 7, kData + 0x08);
  scene.aim(0xC10 + 9, 0xC10 + 13, kText + 0x600);  // the start of a function
  scene.aim(0xC40 + 3, 0xC40 + 7, kData + 0x10);
  scene.aim(0xC40 + 9, 0xC40 + 13, kText + 0x900);
  scene.aim(0xC70 + 3, 0xC70 + 7, kData + 0x18);
  scene.aim(0xC70 + 9, 0xC70 + 13, kText + 0x601);  // not a function's start
  scene.aim(0xCA0 + 3, 0xCA0 + 7, kData + 0x1C);    // not aligned
  scene.aim(0xCA0 + 9, 0xCA0 + 13, kText + 0x900);
  PatternSpec spec;
  spec.id = "invented_idiom";
  spec.kind = PatternKind::Idiom;
  spec.section = ".text";
  spec.bytes = *parse_pattern(kIdiom);
  spec.expect_min = 1;
  spec.expect_max = 10;
  spec.captures = {IdiomCapture{"slot", Capture{3, 7}, ".data", 8, false},
                   IdiomCapture{"getter", Capture{9, 13}, ".text", 1, true}};
  const PatternResult r = scan_one(scene.build_checked(), spec);
  CHECK(r.resolved());
  CHECK(r.count == 4);
  CHECK(r.idiom_rejected == 2);
  REQUIRE(r.idiom_hits.size() == 2);
  CHECK(r.idiom_hits[0].hit == kText + 0xC10);
  CHECK(r.idiom_hits[0].targets == std::vector<std::uint32_t>{kData + 0x08, kText + 0x600});
  CHECK(r.idiom_hits[1].targets == std::vector<std::uint32_t>{kData + 0x10, kText + 0x900});
  spec.expect_min = 5;
  CHECK(scan_one(scene.build_checked(), spec).problem == HitProblem::Count);
}

TEST_CASE("unreadable pages are skipped and counted; matches in the readable pages around them are found") {
  Scene scene;
  scene.plant(0x120, kFn);
  scene.plant(0x1200, kFn);  // in the unreadable pages
  scene.plant(0x2100, kFn);  // after them
  const BuiltImage built = scene.build_checked();
  const HoleSource source(built.memory, kText + 0x1000, kText + 0x2000);
  const auto image = read_image(source, ImageLayout::Memory);
  REQUIRE(image.has_value());
  PatternSet set;
  PatternSpec spec = function_spec("invented_fn", 0x100, 0x1000);
  spec.expect_min = 1;
  spec.expect_max = 3;
  set.patterns.push_back(spec);
  for (const std::uint32_t chunk : {std::uint32_t{4096}, std::uint32_t{1u << 20}}) {
    const ScanReport report = scan_patterns(*image, source, set, ScanOptions{chunk});
    CHECK(report.unscanned >= 0x1000);
    CHECK(report.results[0].count == 2);
    CHECK(report.results[0].hits == std::vector<std::uint32_t>{kText + 0x120, kText + 0x2100});
    CHECK(report.results[0].problem == HitProblem::Ambiguous);  // two functions: what the pages hid is unknown
  }
  set.patterns[0].expect_min = 3;
  const ScanReport too_few = scan_patterns(*image, source, set);
  CHECK(too_few.results[0].problem == HitProblem::Unscanned);
}

TEST_CASE("every section is scanned once, for every pattern in it, and the chunk size changes nothing") {
  Scene scene;
  scene.plant(0x120, kFn);
  scene.plant(0x700, kStatic);
  scene.aim(0x703, 0x707, kData + 0x40);
  const BuiltImage built = scene.build_checked();
  const SpanSource source(built.memory);
  const auto image = read_image(source, ImageLayout::Memory);
  REQUIRE(image.has_value());
  PatternSet set;
  set.patterns = {function_spec("invented_fn"), static_spec("invented_static")};
  std::vector<ScanReport> reports;
  for (const std::uint32_t chunk :
       {std::uint32_t{1}, std::uint32_t{4096}, std::uint32_t{5000}, std::uint32_t{1u << 20}}) {
    reports.push_back(scan_patterns(*image, source, set, ScanOptions{chunk}));
  }
  for (const ScanReport& report : reports) {
    REQUIRE(report.results.size() == 2);
    CHECK(report.results[0].resolved());
    CHECK(report.results[0].entry == kText + 0x100);
    CHECK(report.results[1].resolved());
    CHECK(report.results[1].target == kData + 0x40);
    CHECK(report.scanned == 0x3000);
    CHECK(report.unscanned == 0);
  }
  CHECK(std::string(to_string(HitProblem::NearEntry)) == "a hit starts before entry + anchor_min");
}

TEST_CASE("an idiom may have thousands of hits: every inlined accessor is one, and the scan keeps them all") {
  // 5117 hits of a short idiom (more than kMaxPatternHits), each in the functions' code; none lands where it must, so
  // all are rejected, but the pattern resolves: its count is within expect_matches and every hit was examined.
  Scene scene;
  scene.text().assign(0x3000, 0x00);
  constexpr std::string_view kShort = "D5 D5 D5 D5 ?? ?? ?? ??";  // matches at every offset of a run of D5
  for (std::uint32_t a = 0; a < 0x1400; ++a) scene.text()[a] = 0xD5;
  const std::uint32_t planted = 0x1400 - 3;  // the last four fixed bytes end the run
  REQUIRE(planted > kMaxPatternHits);
  PatternSpec spec;
  spec.id = "invented_idiom";
  spec.kind = PatternKind::Idiom;
  spec.section = ".text";
  spec.bytes = *parse_pattern(kShort);
  spec.expect_min = 1;
  spec.expect_max = kMaxIdiomHits;
  spec.captures = {IdiomCapture{"slot", Capture{1, 5}, ".data", 8, false}};
  const PatternResult r = scan_one(scene.build_checked(), spec);
  CHECK(r.resolved());
  CHECK(r.count == planted);
  CHECK(r.hits.size() == planted);
  CHECK(r.idiom_hits.size() + r.idiom_rejected == planted);
  // A static's hits are still kept to kMaxPatternHits beside it.
  PatternSet set;
  set.patterns.push_back(spec);
  PatternSpec st = spec;
  st.id = "invented_static";
  st.kind = PatternKind::Static;
  st.capture = Capture{1, 5};
  st.target_section = ".data";
  st.expect_max = kMaxPatternHits;
  set.patterns.push_back(st);
  const BuiltImage built = scene.build_checked();
  const SpanSource source(built.memory);
  const auto image = read_image(source, ImageLayout::Memory);
  REQUIRE(image.has_value());
  const ScanReport report = scan_patterns(*image, source, set);
  CHECK(report.results[0].hits.size() == planted);
  CHECK(report.results[1].hits.size() == kMaxPatternHits);
  CHECK(report.results[1].problem == HitProblem::Count);
}
