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
  CHECK(text->find("\"schema\": 2") != std::string::npos);
  CHECK(text->size() < report::kMaxReportBytes);
  // Schema 2's promise: no key can carry a pattern (bytes) or a fraction; what is left is whole numbers, booleans,
  // fixed words, labels, and objects and arrays of those.
  const report::Vocabulary& v = lodestar::vocabulary();
  CHECK(v.kind == "lodestar");
  CHECK(v.schema == 2);
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
