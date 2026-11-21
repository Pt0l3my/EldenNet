// libFuzzer target: the schemas of gal/'s files on arbitrary text (ARCH §6.3): patterns.toml (parse_patterns),
// builds.toml (parse_builds), a layout (load_layout, for a build gated on an invented FakeImage, its items checked
// against an invented catalogue) and ids.toml (parse_ids). The first byte picks the file (its value modulo 4); the rest
// is the text. Besides any sanitizer report, a broken property is a finding:
// - a refusal lists 1 to kMaxSchemaErrors + 1 problems, each with where and what;
// - what patterns.toml accepts holds unique ids, own-derivation provenance and captures within their patterns;
// - what builds.toml accepts holds keys that are their version and region joined, layouts under layouts/ and a
//   SizeOfImage above 0;
// - what a layout accepts is the gated build's, with every field within its class's bound and every `to` and `elem`
//   naming a class of the file, and every item an id of the catalogue under its number and kind, inside the image,
//   found by recipes of its kind;
// - what ids.toml accepts holds numbers that ascend, each in its kind's block, and labels of ours, none twice.
// toml++ is built without exceptions in en-gal; an exception that escapes is a finding too, so this entry has no
// ffi_guard.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "en/gal/builds.hpp"
#include "en/gal/ids.hpp"
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

// The catalogue the layouts' items are read against: invented ids, one of each kind and one retired.
const Catalogue& catalogue() {
  static const Catalogue* parsed = [] {
    std::string text = "schema = 1\n";
    const auto id = [&](int n, const char* label, const char* kind, const char* extra) {
      text += "[[id]]\nn = " + std::to_string(n) + "\nlabel = \"" + label + "\"\nkind = \"" + kind +
              "\"\nmeans = \"An invented thing.\"\nneeds = \"world\"\nrequired = false\n" + extra;
    };
    id(1001, "lantern", "static", "");
    id(1002, "ledger", "static", "status = \"retired\"\n");
    id(2001, "ring_bell", "function", "");
    id(3001, "walker", "class", "");
    id(4001, "rota", "table", "");
    auto c = parse_ids(text);
    require(c.has_value(), "the invented catalogue reads");
    return new Catalogue(*c);
  }();
  return *parsed;
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
  switch (static_cast<std::uint8_t>(input[0]) % 4) {
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
    case 3: {
      const auto ids = parse_ids(text);
      if (!ids) {
        check_errors(ids.error());
        return 0;
      }
      std::uint32_t last = 0;
      for (const IdEntry& e : ids->entries()) {
        require(e.number > last && kind_of_number(e.number) == e.kind, "numbers ascend, each in its kind's block");
        last = e.number;
        require(is_id_label(e.label) && (e.was.empty() || is_id_label(e.was)), "labels of ours");
        require(ids->find(e.label, true) == &e && ids->find(e.number) == &e, "no label or number twice");
        require(!e.means.empty() && !e.needs.empty(), "a meaning and a need");
        require(e.status == IdStatus::Active || !e.required, "a retired id is not required");
      }
      return 0;
    }
    default: {
      const LayoutNeeds needs{kFields, kEnums, {}, false, &catalogue()};
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
      const std::uint32_t image = profile().entry().size_of_image;
      require(layout->items().empty() || layout->schema() == 2, "items only in schema 2");
      require((layout->schema() == 2) == layout->provenance().has_value(), "schema 2 has its provenance");
      for (const ItemEntry& item : layout->items()) {
        const IdEntry* id = catalogue().find(item.label);
        require(id != nullptr && id->number == item.id && id->kind == item.kind && id->status == IdStatus::Active,
                "an item is an active id of the catalogue");
        require(layout->find_item(item.id) == &item, "no id twice");
        require(!item.found_by.empty(), "found by a recipe");
        if (item.kind == IdKind::Class) {
          require(!item.vtables.empty() && item.rva == 0 && item.witnesses.empty(), "a class has its method tables");
          for (const std::uint32_t v : item.vtables) require(v > 0 && v < image, "a method table inside the image");
        } else {
          require(item.rva > 0 && item.rva < image, "an offset inside the image");
          for (const std::uint32_t w : item.witnesses) require(w > 0 && w < image, "a witness inside the image");
        }
        if (item.kind == IdKind::Function) {
          require(item.length >= 1 && std::uint64_t{item.rva} + item.length <= image, "a function inside the image");
        } else {
          require(item.digest.empty() && !item.verdict.has_value(), "only a function has a digest or a verdict");
        }
        if (item.kind == IdKind::Table) {
          require(std::uint64_t{item.rva} + std::uint64_t{item.stride} * item.count <= image, "a table inside");
        }
        if (item.status == LayoutStatus::Confirmed) {
          require(item.passed(ItemCheck::Placement) && item.passed(ItemCheck::Agree), "confirmed by two sources");
        }
      }
      return 0;
    }
  }
}
