// libFuzzer target: the schemas of gal/'s files on arbitrary text (ARCH §6.3): patterns.toml (parse_patterns),
// builds.toml (parse_builds) and a layout (load_layout, for a build gated on an invented FakeImage). The first byte
// picks the file (its value modulo 3); the rest is the text. Besides any sanitizer report, a broken property is a
// finding:
// - a refusal lists 1 to kMaxSchemaErrors + 1 problems, each with where and what;
// - what patterns.toml accepts holds unique ids, own-derivation provenance and captures within their patterns;
// - what builds.toml accepts holds keys that are their version and region joined, layouts under layouts/ and a
//   SizeOfImage above 0;
// - what a layout accepts is the gated build's, with every field within its class's bound and every `to` and `elem`
//   naming a class of the file.
// toml++ is built without exceptions in en-gal; an exception that escapes is a finding too, so this entry has no
// ffi_guard.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "en/gal/builds.hpp"
#include "en/gal/layout.hpp"
#include "en/gal/patterns.hpp"
#include "fake_image.hpp"

namespace {

using namespace en::gal;

void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

void check_errors(const SchemaErrors& errors) {
  require(!errors.empty() && errors.size() <= kMaxSchemaErrors + 1, "a refusal lists its problems, within the cap");
  for (const SchemaError& e : errors) require(!e.where.empty() && !e.problem.empty(), "each with where and what");
}

// The build the layouts are read for: an invented image, gated once.
const BuildProfile& profile() {
  static const BuildProfile* gated = [] {
    test::FakeImageSpec spec;
    spec.sections.push_back(test::FakeSection{".text", test::kCode, std::vector<std::uint8_t>(0x100, 0), 0});
    spec.version = test::FakeVersion{};
    spec.version->file_version = {9, 8, 7, 0};
    const test::BuiltImage built = test::build(spec);
    const SpanSource source(built.file);
    const auto image = read_image(source, ImageLayout::File);
    const std::string text =
        "schema = 1\n[[build]]\nkey = \"9.8.7.0-ww\"\napp = \"1.0\"\nfile_version = \"9.8.7.0\"\n"
        "product_name = \"INVENTED GAME\"\nregion = \"ww\"\ntime_date_stamp = " +
        std::to_string(spec.time_date_stamp) + "\nsize_of_image = " + std::to_string(image->size_of_image) +
        "\nlayout = \"layouts/a.toml\"\nstatus = \"supported\"\n";
    const auto builds = parse_builds(text);
    auto p = gate(*image, *builds);
    require(p.has_value(), "the invented build passes the gate");
    return new BuildProfile(*p);
  }();
  return *gated;
}

constexpr FieldNeed kFields[] = {{"Holder", "mode", FieldType::I32}};
constexpr std::string_view kNames[] = {"Alpha", "Beta", "Gamma"};
constexpr EnumNeed kEnums[] = {{"mode", kNames}};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size < 1) return 0;
  // SAFETY: libFuzzer hands over `size` readable bytes, which live until this call returns; the view reads them as
  // the characters they are, and ends where they end.
  const std::string_view input(reinterpret_cast<const char*>(data), size);
  const std::string_view text = input.substr(1);
  switch (static_cast<std::uint8_t>(input[0]) % 3) {
    case 0: {
      const auto set = parse_patterns(text, {});
      if (!set) {
        check_errors(set.error());
        return 0;
      }
      for (std::size_t i = 0; i < set->patterns.size(); ++i) {
        const PatternSpec& p = set->patterns[i];
        require(set->find(p.id) == &set->patterns[i], "ids are unique");
        require(p.provenance.method == "own-derivation", "provenance is own-derivation");
        require(p.bytes.fixed() >= 4 && p.expect_min >= 1 && p.expect_min <= p.expect_max, "bytes and counts");
        if (p.kind == PatternKind::Static) {
          require(p.capture.at + 4 <= p.capture.insn_end && p.capture.insn_end <= p.bytes.size(), "capture within");
        }
        if (p.kind == PatternKind::Function) require(p.anchor_min >= 5, "an interior anchor");
      }
      return 0;
    }
    case 1: {
      const auto builds = parse_builds(text);
      if (!builds) {
        check_errors(builds.error());
        return 0;
      }
      for (const BuildEntry& e : builds->entries) {
        require(e.size_of_image > 0, "a probed SizeOfImage");
        require(e.layout.rfind("layouts/", 0) == 0, "a layout under layouts/");
        require(e.key.size() > 3 && e.key.substr(e.key.size() - 2) == to_string(e.region),
                "the key ends in the region");
      }
      return 0;
    }
    default: {
      const LayoutNeeds needs{kFields, kEnums, {}, false};
      const auto layout = load_layout(profile(), text, needs, LayoutMode::Dev);
      if (!layout) {
        check_errors(layout.error());
        return 0;
      }
      require(layout->build() == profile().entry().key, "the gated build's layout");
      for (const ClassLayout& c : layout->classes()) {
        const std::uint32_t bound = c.copy_size ? *c.copy_size : c.stride.value_or(0);
        for (const FieldLayout& f : c.fields) {
          require(std::uint64_t{f.off} + f.size <= bound, "a field within its class");
          if (!f.to.empty()) require(layout->find_class(f.to) != nullptr, "a pointer's class exists");
          if (!f.elem.empty()) require(layout->find_class(f.elem) != nullptr, "an array's class exists");
        }
      }
      return 0;
    }
  }
}
