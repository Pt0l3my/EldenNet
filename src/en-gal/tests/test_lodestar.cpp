// Lodestar's finder (lodestar.hpp; proposal 0005 §6.2, §11): on an executable file it counts the accessor's shape,
// gathers the statics its accepted sites load with the places that load them, compares the shape with the withdrawn
// pattern (cut in memory, never written), and reports the classes and the groups' table, all as numbers. Every
// witness it gives passes the resolver's own check. The programs and every name here are invented.
#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/builds.hpp"
#include "en/gal/derive.hpp"
#include "en/gal/lodestar.hpp"
#include "en/gal/pe.hpp"
#include "en/gal/recipes.hpp"
#include "en/gal/report.hpp"
#include "en/gal/resolve.hpp"
#include "fake_program.hpp"
#include "sample_program.hpp"

using namespace en::gal;
using namespace en::gal::test;

namespace {

constexpr std::string_view kRecipes = R"(schema = 1
[[recipe]]
id = "accessor"
kind = "accessor"
steps = ["load_slot", "test_loaded", "branch_set_short", "address_arg1", "call_relative"]
gives = { static = 0, class = 3, getter = 4 }
path_suffix = "Lone.h"
expect_matches = [1, 64]
provenance = { method = "own-derivation", evidence = "docs/ops/probes/invented.md", date = "2026-01-02" }
)";

struct Run {
  ImageInfo image;
  lodestar::Found found;
};

Run run(const Sample& sample, const char* leads_text, std::string_view recipes_text = kRecipes,
        const Builds* builds = nullptr) {
  const SpanSource source(sample.built.file);
  auto image = read_image(source, ImageLayout::File);
  REQUIRE(image.has_value());
  const auto leads = derive::parse_leads(leads_text);
  REQUIRE(leads.has_value());
  const auto recipes = parse_recipes(recipes_text, {});
  REQUIRE(recipes.has_value());
  Run r{*image, {}};
  r.found = lodestar::find(*image, source, sample.built.file.size(), *leads, *recipes, builds);
  return r;
}

}  // namespace

TEST_CASE("the finder counts the accessor's shape, and the withdrawn pattern accepts the same sites") {
  const Sample sample = build_sample();
  const Run r = run(sample, kSampleLeads);
  const lodestar::AccessorFound& a = r.found.accessor;
  CHECK(a.recipe);
  CHECK(a.hits == 8);
  CHECK(a.sites == 8);
  CHECK(a.accepted == 6);
  CHECK(a.expect == lodestar::Expect::Within);
  CHECK(a.refused[static_cast<std::size_t>(fd4::Refusal::StaticSection)] == 1);
  CHECK(a.refused[static_cast<std::size_t>(fd4::Refusal::PathText)] == 1);
  // The pattern is cut in memory, as R0 cut it, and accepts exactly the same sites.
  CHECK(a.pattern);
  CHECK(a.pattern_accepted == 6);
  CHECK(a.both == 6);
  CHECK(a.only_shape == 0);
  CHECK(a.only_pattern == 0);
  CHECK(a.pattern_statics == a.statics_found);
  CHECK(a.shared_class_objects == 0);
  CHECK(a.shared_statics == 0);
  CHECK(a.only_pattern_why.empty());
  CHECK(a.only_pattern_statics.empty());
  CHECK(lodestar::complete(r.found));
  // The variants: the pattern also hits a look-alike and refuses it; the shape never hits it.
  const Run v = run(build_variants(), kVariantLeads);
  CHECK(v.found.accessor.hits == 6);
  CHECK(v.found.accessor.accepted == 6);
  CHECK(v.found.accessor.pattern_hits == 7);
  CHECK(v.found.accessor.pattern_accepted == 6);
  CHECK(v.found.accessor.only_shape + v.found.accessor.only_pattern == 0);
  CHECK(lodestar::complete(v.found));
}

TEST_CASE("a site the pattern alone accepts is counted with why the shape leaves it out, and the static it loads") {
  // Three accessors that load a pointer, in three registers, and a fourth place with the same look that loads a
  // 32-bit value: the pattern, cut with the registers' bytes wildcarded, accepts all four; the shape only the three.
  using P = FakeProgram;
  Sample sample;
  P& p = sample.program;
  p.label("path", P::Rdata);
  p.ascii(P::Rdata, "invented\\core\\Lone.h");
  for (const char* n : {"a", "b", "c", "n"}) {
    p.align(P::Data, 8);
    p.label(std::string("st_") + n, P::Data);
    p.u64(P::Data, 0);
    p.label(std::string("cls_") + n, P::Data);
    p.zeros(P::Data, 8);
  }
  const auto fn = [&](const std::string& name, std::initializer_list<std::uint8_t> body) {
    p.align(P::Text, 16, 0xCC);
    p.label(name, P::Text);
    p.bytes(P::Text, body);
    p.label(name + "_end", P::Text);
    p.function(name, name + "_end");
  };
  fn("getter", {0x48, 0x89, 0xC8, 0xC3});
  fn("log", {0xC3});
  const auto accessor = [&](const std::string& n, std::vector<std::uint8_t> load, std::vector<std::uint8_t> test) {
    p.align(P::Text, 16, 0xCC);
    p.label("get_" + n, P::Text);
    p.bytes(P::Text, {0x48, 0x83, 0xEC, 0x28, 0x90});  // the function's own start, before the head
    p.bytes(P::Text, load);
    p.rel32(P::Text, "st_" + n);
    p.bytes(P::Text, test);
    p.bytes(P::Text, {0x75, 0x18, 0x48, 0x8D, 0x0D});  // skip the block when set; else the class object's address
    p.rel32(P::Text, "cls_" + n);
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "getter");
    p.bytes(P::Text, {0x48, 0x8D, 0x15});  // the path, loaded in the block the branch skips
    p.rel32(P::Text, "path");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "log");
    p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x28, 0xC3});
    p.label("get_" + n + "_end", P::Text);
    p.function("get_" + n, "get_" + n + "_end");
  };
  accessor("a", {0x48, 0x8B, 0x05}, {0x48, 0x85, 0xC0});
  accessor("b", {0x48, 0x8B, 0x3D}, {0x48, 0x85, 0xFF});
  accessor("c", {0x4C, 0x8B, 0x0D}, {0x4D, 0x85, 0xC9});
  accessor("n", {0x44, 0x8B, 0x05}, {0x45, 0x85, 0xC0});  // a 32-bit load and test: no pointer
  sample.built = p.build();

  const Run r = run(sample, kSampleLeadsClean);
  const lodestar::AccessorFound& a = r.found.accessor;
  CHECK(a.hits == 3);
  CHECK(a.accepted == 3);
  CHECK(a.statics_found == 3);
  REQUIRE(a.pattern);
  CHECK(a.pattern_accepted == 4);
  CHECK(a.pattern_statics == 4);
  CHECK(a.both == 3);
  CHECK(a.only_shape == 0);
  CHECK(a.only_pattern == 1);
  CHECK(a.shared_class_objects == 0);
  CHECK(a.shared_statics == 0);
  // A site the pattern alone accepts is reported with its reason, and does not count against the run.
  CHECK(lodestar::complete(r.found));
  REQUIRE(a.only_pattern_why.size() == 1);
  CHECK(a.only_pattern_why[0].step == 0);
  CHECK(a.only_pattern_why[0].differs == shape::Differs::Narrow);
  CHECK(a.only_pattern_why[0].count == 1);
  REQUIRE(a.only_pattern_statics.size() == 1);
  CHECK(a.only_pattern_statics[0].rva == p.rva("st_n"));
  CHECK(a.only_pattern_statics[0].class_object == p.rva("cls_n"));
  CHECK(a.only_pattern_statics[0].sites == 1);
  CHECK(a.only_pattern_statics[0].witnesses == std::vector<std::uint32_t>{p.rva("get_n") + 5});
  // The report carries them, and still lints clean.
  const auto text = lodestar::write_report(r.found);
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"differs\": \"narrow\"") != std::string::npos);
  CHECK(text->find("\"only_pattern_statics\"") != std::string::npos);
}

TEST_CASE("one class object for each static: a class object that goes with two statics is counted, and the run is to read") {
  // Two accessors pass the same class object with different statics: one of them took something else for the slot.
  using P = FakeProgram;
  Sample sample;
  P& p = sample.program;
  p.label("path", P::Rdata);
  p.ascii(P::Rdata, "invented\\core\\Lone.h");
  for (const char* n : {"a", "b"}) {
    p.align(P::Data, 8);
    p.label(std::string("st_") + n, P::Data);
    p.u64(P::Data, 0);
  }
  p.label("cls", P::Data);
  p.zeros(P::Data, 8);
  const auto fn = [&](const std::string& name, std::initializer_list<std::uint8_t> body) {
    p.align(P::Text, 16, 0xCC);
    p.label(name, P::Text);
    p.bytes(P::Text, body);
    p.label(name + "_end", P::Text);
    p.function(name, name + "_end");
  };
  fn("getter", {0x48, 0x89, 0xC8, 0xC3});
  fn("log", {0xC3});
  for (const char* n : {"a", "b"}) {
    p.align(P::Text, 16, 0xCC);
    p.label(std::string("get_") + n, P::Text);
    p.bytes(P::Text, {0x48, 0x83, 0xEC, 0x28, 0x90, 0x48, 0x8B, 0x05});
    p.rel32(P::Text, std::string("st_") + n);
    p.bytes(P::Text, {0x48, 0x85, 0xC0, 0x75, 0x18, 0x48, 0x8D, 0x0D});
    p.rel32(P::Text, "cls");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "getter");
    p.bytes(P::Text, {0x48, 0x8D, 0x15});
    p.rel32(P::Text, "path");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "log");
    p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x28, 0xC3});
    p.label(std::string("get_") + n + "_end", P::Text);
    p.function(std::string("get_") + n, std::string("get_") + n + "_end");
  }
  sample.built = p.build();
  const Run r = run(sample, kSampleLeadsClean);
  const lodestar::AccessorFound& a = r.found.accessor;
  CHECK(a.accepted == 2);
  CHECK(a.statics_found == 2);
  CHECK(a.class_objects == 1);
  CHECK(a.shared_class_objects == 1);
  CHECK(a.shared_statics == 0);
  CHECK_FALSE(lodestar::complete(r.found));
  const auto text = lodestar::write_report(r.found);
  REQUIRE(text.has_value());
  CHECK(text->find("\"shared_class_objects\": 1") != std::string::npos);
}

TEST_CASE("every static the finder gives is one the resolver re-finds from its witnesses") {
  const Sample sample = build_sample();
  const Run r = run(sample, kSampleLeads);
  const lodestar::AccessorFound& a = r.found.accessor;
  REQUIRE_FALSE(a.statics.empty());
  CHECK(a.statics.size() == a.statics_found);
  CHECK(std::is_sorted(a.statics.begin(), a.statics.end(),
                       [](const auto& x, const auto& y) { return x.rva < y.rva; }));
  const SpanSource source(sample.built.file);
  std::uint64_t sites = 0;
  for (const lodestar::StaticFound& s : a.statics) {
    CAPTURE(s.rva);
    CHECK(s.sites >= 1);
    CHECK(s.witnesses.size() == std::min<std::size_t>(s.sites, lodestar::kMaxSiteWitnesses));
    sites += s.sites;
    ItemEntry item;
    item.id = 1001;
    item.kind = IdKind::Static;
    item.rva = s.rva;
    item.witnesses = s.witnesses;
    const ItemState state = check_item(r.image, source, item);
    CHECK(state.ok());
    CHECK(state.witnesses == s.witnesses.size());
  }
  CHECK(sites == a.accepted);
  CHECK(a.class_objects >= 1);
  CHECK_FALSE(a.getters.empty());
  CHECK(std::is_sorted(a.getters.begin(), a.getters.end()));
}

TEST_CASE("the finder reports the leads' classes and the groups' table, and whether the build is known") {
  const Sample sample = build_sample();
  const Run r = run(sample, kSampleLeads);
  REQUIRE_FALSE(r.found.classes.empty());
  for (const lodestar::ClassFound& c : r.found.classes) {
    CAPTURE(c.label);
    CHECK(c.status == derive::Status::Derived);
    CHECK_FALSE(c.vtables.empty());
    CHECK(std::is_sorted(c.vtables.begin(), c.vtables.end()));
    // Each table of methods is placed as the resolver wants a class's.
    ItemEntry item;
    item.id = 3001;
    item.kind = IdKind::Class;
    item.vtables = c.vtables;
    const SpanSource source(sample.built.file);
    CHECK(check_item(r.image, source, item).ok());
  }
  CHECK(r.found.table.status == derive::Status::Derived);
  CHECK(r.found.table.entries >= 2);
  CHECK(r.found.table.stride >= 8);
  // Without builds.toml, or with one that does not hold this image, the build is not known.
  CHECK_FALSE(r.found.image.known_build);
  CHECK(r.found.image.gate_problem == GateProblem::UnknownBuild);
  CHECK(r.found.image.size_of_image == r.image.size_of_image);
  CHECK(r.found.image.file_size == sample.built.file.size());
  const auto builds = parse_builds(
      "schema = 1\n[[build]]\nkey = \"9.8.7.0-ww\"\napp = \"4.5.6\"\nfile_version = \"9.8.7.0\"\n"
      "product_name = \"INVENTED GAME\"\nregion = \"ww\"\ntime_date_stamp = " +
      std::to_string(r.image.time_date_stamp) + "\nsize_of_image = " + std::to_string(r.image.size_of_image) +
      "\nlayout = \"layouts/a.toml\"\nstatus = \"supported\"\n");
  REQUIRE(builds.has_value());
  const Run known = run(sample, kSampleLeads, kRecipes, &*builds);
  if (r.image.version && r.image.version->file_version == std::array<std::uint16_t, 4>{9, 8, 7, 0}) {
    CHECK(known.found.image.known_build);
  } else {
    CHECK_FALSE(known.found.image.known_build);
  }
}

TEST_CASE("the run is to read first when the count is outside its range, or there is no recipe") {
  const Sample sample = build_sample();
  std::string few(kRecipes);
  few.replace(few.find("[1, 64]"), 7, "[1, 4]");
  const Run above = run(sample, kSampleLeads, few);
  CHECK(above.found.accessor.expect == lodestar::Expect::Above);
  CHECK_FALSE(lodestar::complete(above.found));
  std::string many(kRecipes);
  many.replace(many.find("[1, 64]"), 7, "[20, 64]");
  CHECK(run(sample, kSampleLeads, many).found.accessor.expect == lodestar::Expect::Below);
  const Run none = run(sample, kSampleLeads, "schema = 1\n");
  CHECK_FALSE(none.found.accessor.recipe);
  CHECK(none.found.accessor.hits == 0);
  CHECK_FALSE(lodestar::complete(none.found));
  // The classes and the table do not need the recipe.
  CHECK_FALSE(none.found.classes.empty());
}

TEST_CASE("the report is numbers, words and labels: it lints clean, and its vocabulary has no key for bytes") {
  const Run r = run(build_sample(), kSampleLeads);
  const auto text = lodestar::write_report(r.found);
  if (!text.has_value()) MESSAGE(report::describe(text.error()));
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  const auto linted = report::lint(*text, known);
  if (!linted.has_value()) {
    for (const report::Problem& p : linted.error()) MESSAGE(report::describe(p));
  }
  CHECK(linted.has_value());
  CHECK(text->find("\"report\": \"lodestar\"") != std::string::npos);
  CHECK(text->find("\"schema\": 7") != std::string::npos);
  CHECK(text->size() < report::kMaxReportBytes);
  // Schema 2's promise: no key can carry a pattern (bytes) or a fraction; what is left is whole numbers, booleans,
  // fixed words, labels, and objects and arrays of those.
  const report::Vocabulary& v = lodestar::vocabulary();
  CHECK(v.kind == "lodestar");
  CHECK(v.schema == 7);
  for (const report::Key& k : v.keys) {
    CAPTURE(k.name);
    CHECK(k.type != report::Type::Pattern);
    CHECK(k.type != report::Type::Float);
    if (k.type == report::Type::Array) {
      CHECK(k.element != report::Type::Pattern);
      CHECK(k.element != report::Type::Float);
    }
  }
  // A report with a pattern slipped in is refused by the lint: the key does not exist.
  std::string tampered = *text;
  const std::size_t at = tampered.find("\"complete\"");
  REQUIRE(at != std::string::npos);
  tampered.insert(at, "\"bytes\": \"48 8B ?? ??\", ");
  CHECK_FALSE(report::lint(tampered, known).has_value());
}

TEST_CASE("the accessor's other shapes are counted beside it: what each reaches that the accessor's own sites do not") {
  // Two plain accessors (a, b); one with an instruction between its load and its test (g); one that compares the
  // slot where it lies (c); a second use of a's manager through the gap's shape; and a gap accessor whose class
  // object is b's (it took something else for the slot).
  using P = FakeProgram;
  Sample sample;
  P& p = sample.program;
  p.label("path", P::Rdata);
  p.ascii(P::Rdata, "invented\\core\\Lone.h");
  for (const char* n : {"a", "b", "g", "c", "x"}) {
    p.align(P::Data, 8);
    p.label(std::string("st_") + n, P::Data);
    p.u64(P::Data, 0);
    p.label(std::string("cls_") + n, P::Data);
    p.zeros(P::Data, 8);
  }
  const auto fn = [&](const std::string& name, std::initializer_list<std::uint8_t> body) {
    p.align(P::Text, 16, 0xCC);
    p.label(name, P::Text);
    p.bytes(P::Text, body);
    p.label(name + "_end", P::Text);
    p.function(name, name + "_end");
  };
  fn("getter", {0x48, 0x89, 0xC8, 0xC3});
  fn("log", {0xC3});
  // `head`: the bytes up to the slot's reference; `after`: what follows it up to the branch; `tail`: how many of
  // those bytes belong to the instruction that refers to the slot (an immediate).
  const auto accessor = [&](const std::string& name, const std::string& slot, const std::string& cls,
                            std::vector<std::uint8_t> head, std::vector<std::uint8_t> after, std::uint32_t tail = 0) {
    p.align(P::Text, 16, 0xCC);
    p.label("get_" + name, P::Text);
    p.bytes(P::Text, {0x48, 0x83, 0xEC, 0x28, 0x90});
    p.bytes(P::Text, head);
    p.rel32(P::Text, "st_" + slot, tail);
    p.bytes(P::Text, after);
    p.bytes(P::Text, {0x75, 0x18, 0x48, 0x8D, 0x0D});
    p.rel32(P::Text, "cls_" + cls);
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "getter");
    p.bytes(P::Text, {0x48, 0x8D, 0x15});
    p.rel32(P::Text, "path");
    p.bytes(P::Text, {0xE8});
    p.rel32(P::Text, "log");
    p.bytes(P::Text, {0x48, 0x83, 0xC4, 0x28, 0xC3});
    p.label("get_" + name + "_end", P::Text);
    p.function("get_" + name, "get_" + name + "_end");
  };
  accessor("a", "a", "a", {0x48, 0x8B, 0x05}, {0x48, 0x85, 0xC0});
  accessor("b", "b", "b", {0x48, 0x8B, 0x3D}, {0x48, 0x85, 0xFF});
  accessor("g", "g", "g", {0x48, 0x8B, 0x05}, {0x48, 0x89, 0xD1, 0x48, 0x85, 0xC0});   // a move between
  accessor("g2", "g", "g", {0x48, 0x8B, 0x05}, {0x90, 0x48, 0x85, 0xC0});              // the same manager again
  accessor("a2", "a", "a", {0x48, 0x8B, 0x05}, {0x90, 0x48, 0x85, 0xC0});              // a's, through the gap
  accessor("x", "x", "b", {0x48, 0x8B, 0x05}, {0x90, 0x48, 0x85, 0xC0});               // b's class object
  accessor("c", "c", "c", {0x48, 0x83, 0x3D}, {0x00}, 1);                              // compared where it lies
  sample.built = p.build();

  const std::string recipes = std::string(kRecipes) + R"(
[[recipe]]
id = "accessor_gap"
kind = "accessor"
steps = ["load_slot", "skip_one", "test_loaded", "branch_set_short", "address_arg1", "call_relative"]
gives = { static = 0, class = 4, getter = 5 }
path_suffix = "Lone.h"
provenance = { method = "own-derivation", evidence = "docs/ops/probes/invented.md", date = "2026-01-02" }

[[recipe]]
id = "accessor_compare"
kind = "accessor"
steps = ["compare_slot", "branch_set_short", "address_arg1", "call_relative"]
gives = { static = 0, class = 2, getter = 3 }
path_suffix = "Lone.h"
expect_matches = [2, 9]
provenance = { method = "own-derivation", evidence = "docs/ops/probes/invented.md", date = "2026-01-02" }
)";
  const Run r = run(sample, kSampleLeadsClean, recipes);
  const lodestar::AccessorFound& a = r.found.accessor;
  // The accessor's own numbers are what they were without the variants.
  CHECK(a.hits == 2);
  CHECK(a.accepted == 2);
  CHECK(a.statics_found == 2);
  CHECK(lodestar::complete(r.found));
  REQUIRE(a.variants.size() == 2);
  const lodestar::VariantFound& gap = a.variants[0];
  CHECK(gap.label == "accessor_gap");
  CHECK(gap.hits == 4);
  CHECK(gap.accepted == 4);
  CHECK(gap.expect == lodestar::Expect::Within);
  CHECK(gap.statics_found == 3);  // g's, a's and x's
  CHECK(gap.known == 1);          // a's, which the accessor's own site loads too
  CHECK(gap.agreeing == 1);       // with the same class object
  CHECK(gap.shared_class_objects == 1);  // x's class object is b's
  REQUIRE(gap.statics.size() == 2);
  CHECK(gap.statics[0].rva == p.rva("st_g"));
  CHECK(gap.statics[0].class_object == p.rva("cls_g"));
  CHECK(gap.statics[0].sites == 2);
  CHECK(gap.statics[0].witnesses == std::vector<std::uint32_t>{p.rva("get_g") + 5, p.rva("get_g2") + 5});
  CHECK(gap.statics[1].rva == p.rva("st_x"));
  const lodestar::VariantFound& compare = a.variants[1];
  CHECK(compare.label == "accessor_compare");
  CHECK(compare.hits == 1);
  CHECK(compare.expect == lodestar::Expect::Below);  // fewer places than its recipe says
  CHECK(compare.known == 0);
  REQUIRE(compare.statics.size() == 1);
  CHECK(compare.statics[0].rva == p.rva("st_c"));
  CHECK(compare.statics[0].class_object == p.rva("cls_c"));
  CHECK(compare.statics[0].witnesses == std::vector<std::uint32_t>{p.rva("get_c") + 5});
  // Every static the naming looks at: the accessor's own, then the variants' new ones.
  const std::vector<lodestar::StaticFound> all = lodestar::all_statics(a);
  REQUIRE(all.size() == 5);
  CHECK(all[0].rva == p.rva("st_a"));
  CHECK(all[2].rva == p.rva("st_g"));
  CHECK(all[4].rva == p.rva("st_c"));
  // Each new static's witness refers to it: the resolver's own check passes on a load and on a compare alike.
  const SpanSource source(sample.built.file);
  for (const lodestar::StaticFound& st : {gap.statics[0], compare.statics[0]}) {
    ItemEntry e;
    e.label = "characters";
    e.id = 1002;
    e.kind = IdKind::Static;
    e.rva = st.rva;
    e.found_by = {Recipe::Accessor};
    e.witnesses = st.witnesses;
    CHECK(check_item(r.image, source, e).ok());
  }
  // The report carries the variants as counts and places, and lints clean.
  const auto text = lodestar::write_report(r.found);
  REQUIRE(text.has_value());
  const report::Vocabulary* known[] = {&lodestar::vocabulary()};
  CHECK(report::lint(*text, known).has_value());
  CHECK(text->find("\"label\": \"accessor_gap\"") != std::string::npos);
  CHECK(text->find("\"agreeing\": 1") != std::string::npos);
}
