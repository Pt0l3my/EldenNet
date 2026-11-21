// The build's table (layout.hpp, schema 2; proposal 0005 §5 to §7): where each of Lodestar's ids lies in one build.
// An item agrees with the catalogue; its numbers lie inside the build's image; its recipes find its kind; a check it
// claims has what it checks; a confirmed item has passed the checks its kind needs; a release build refuses a lead;
// and no key of an item can carry a byte of the game. Every id, offset and digest here is invented.
#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/builds.hpp"
#include "en/gal/ids.hpp"
#include "en/gal/layout.hpp"
#include "fake_image.hpp"

using namespace en::gal;
using namespace en::gal::test;

namespace {

constexpr std::uint32_t kImageSize = 0x11000;

BuildEntry entry() {
  BuildEntry e;
  e.key = "9.8.7.0-ww";
  e.app = "4.5.6";
  e.file_version = {9, 8, 7, 0};
  e.product_name = "INVENTED GAME";
  e.size_of_image = kImageSize;
  e.time_date_stamp = 1;
  e.layout = "layouts/a.toml";
  return e;
}

Catalogue catalogue() {
  const auto id = [](std::uint32_t n, std::string_view label, std::string_view kind, std::string_view extra = "") {
    return "[[id]]\nn = " + std::to_string(n) + "\nlabel = \"" + std::string(label) + "\"\nkind = \"" +
           std::string(kind) + "\"\nmeans = \"An invented thing.\"\nneeds = \"world\"\nrequired = false\n" +
           std::string(extra) + "\n";
  };
  auto parsed = parse_ids("schema = 1\n" + id(1001, "lantern", "static") +
                          id(1002, "ledger", "static", "status = \"retired\"") + id(1003, "larder", "static") +
                          id(2001, "ring_bell", "function") + id(3001, "walker", "class") + id(4001, "rota", "table"));
  REQUIRE(parsed.has_value());
  return *parsed;
}

constexpr std::string_view kHead = R"(schema = 2
build = "9.8.7.0-ww"
derived_from = "probe"
probe_record = "docs/ops/probes/invented.md"
)";

constexpr std::string_view kProvenance = R"([provenance]
method = "own-derivation"
tool = "0.1.0-test.1"
tool_sha256 = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
round = "l3"
date = "2026-01-02"
leads = ["our own notes, section 4"]
)";

constexpr std::string_view kLantern = R"([item.lantern]
id = 1001
kind = "static"
rva = 0x8000
found_by = ["accessor", "instance_class"]
witnesses = [0x1010, 0x1200, 0x1040]
checks = ["placement", "agree", "witness", "class"]
status = "confirmed"
)";

constexpr std::string_view kBell = R"([item.ring_bell]
id = 2001
kind = "function"
rva = 0x2000
length = 0x5D
found_by = ["relation"]
witnesses = [0x1100]
digest = "0123456789abcdef0123456789abcdef"
verdict = "adds"
checks = ["placement", "agree", "witness", "digest", "verdict"]
status = "confirmed"
)";

constexpr std::string_view kWalker = R"([item.walker]
id = 3001
kind = "class"
vtables = [0x9000, 0x9100]
found_by = ["class"]
checks = ["placement", "agree", "class"]
status = "confirmed"
)";

constexpr std::string_view kRota = R"([item.rota]
id = 4001
kind = "table"
rva = 0xA000
stride = 32
count = 168
found_by = ["table"]
witnesses = [0x1300]
checks = ["placement", "agree", "witness"]
status = "confirmed"
)";

std::string table(std::initializer_list<std::string_view> items) {
  std::string text = std::string(kHead) + std::string(kProvenance);
  for (const std::string_view item : items) text += std::string(item);
  return text;
}

// `text` with its first `from` replaced by `to`.
std::string with(std::string_view text, std::string_view from, std::string_view to) {
  std::string out(text);
  const std::size_t at = out.find(from);
  REQUIRE(at != std::string::npos);
  out.replace(at, from.size(), to);
  return out;
}

SchemaErrors check(const std::string& text, LayoutMode mode = LayoutMode::Release, bool with_catalogue = true) {
  const Catalogue ids = catalogue();
  LayoutNeeds needs;
  needs.task = false;
  if (with_catalogue) needs.ids = &ids;
  return check_layout(entry(), text, needs, mode);
}

bool has_error(const SchemaErrors& errors, std::string_view where, std::string_view problem) {
  for (const SchemaError& e : errors) {
    if (e.where == where && e.problem.find(problem) != std::string::npos) return true;
  }
  return false;
}

// The one item `item`, in a table of its own, refused at `where` for `problem`.
bool refused(const std::string& item, std::string_view where, std::string_view problem,
             LayoutMode mode = LayoutMode::Release) {
  const SchemaErrors errors = check(std::string(kHead) + std::string(kProvenance) + item, mode);
  if (!has_error(errors, where, problem)) {
    for (const SchemaError& e : errors) MESSAGE(describe(e));
    return false;
  }
  return true;
}

}  // namespace

TEST_CASE("a build's table reads: an item of every kind, with its numbers, recipes, checks and status") {
  // A profile for a build whose image is large enough for the offsets below.
  FakeImageSpec spec;
  spec.sections.push_back(FakeSection{".text", kCode, std::vector<std::uint8_t>(0x10000, 0x00), 0});
  spec.version = FakeVersion{};
  spec.version->file_version = {9, 8, 7, 0};
  const BuiltImage built = build(spec);
  const SpanSource source(built.file);
  const auto image = read_image(source, ImageLayout::File);
  REQUIRE(image.has_value());
  REQUIRE(image->size_of_image >= kImageSize);
  const auto builds = parse_builds(
      "schema = 1\n[[build]]\nkey = \"9.8.7.0-ww\"\napp = \"4.5.6\"\nfile_version = \"9.8.7.0\"\n"
      "product_name = \"INVENTED GAME\"\nregion = \"ww\"\ntime_date_stamp = " +
      std::to_string(spec.time_date_stamp) + "\nsize_of_image = " + std::to_string(image->size_of_image) +
      "\nlayout = \"layouts/a.toml\"\nstatus = \"supported\"\n");
  REQUIRE(builds.has_value());
  const auto profile = gate(*image, *builds);
  REQUIRE(profile.has_value());

  const Catalogue ids = catalogue();
  LayoutNeeds needs;
  needs.task = false;
  needs.ids = &ids;
  const auto layout = load_layout(*profile, table({kLantern, kBell, kWalker, kRota}), needs, LayoutMode::Release);
  if (!layout.has_value()) {
    for (const SchemaError& e : layout.error()) MESSAGE(describe(e));
  }
  REQUIRE(layout.has_value());
  CHECK(layout->schema() == 2);
  CHECK_FALSE(layout->has_leads());
  REQUIRE(layout->items().size() == 4);

  REQUIRE(layout->provenance().has_value());
  CHECK(layout->provenance()->tool == "0.1.0-test.1");
  CHECK(layout->provenance()->round == "l3");
  CHECK(layout->provenance()->date == "2026-01-02");
  CHECK(layout->provenance()->leads == std::vector<std::string>{"our own notes, section 4"});
  CHECK(layout->probe_record() == "docs/ops/probes/invented.md");

  const ItemEntry* lantern = layout->find_item("lantern");
  REQUIRE(lantern != nullptr);
  CHECK(lantern == layout->find_item(1001));
  CHECK(lantern->kind == IdKind::Static);
  CHECK(lantern->rva == 0x8000);
  CHECK(lantern->found_by == std::vector<Recipe>{Recipe::Accessor, Recipe::InstanceClass});
  CHECK(lantern->witnesses == std::vector<std::uint32_t>{0x1010, 0x1200, 0x1040});
  CHECK(lantern->passed(ItemCheck::Witness));
  CHECK_FALSE(lantern->passed(ItemCheck::Digest));
  CHECK(lantern->status == LayoutStatus::Confirmed);

  const ItemEntry* bell = layout->find_item(2001);
  REQUIRE(bell != nullptr);
  CHECK(bell->kind == IdKind::Function);
  CHECK(bell->rva == 0x2000);
  CHECK(bell->length == 0x5D);
  CHECK(bell->digest == "0123456789abcdef0123456789abcdef");
  CHECK(bell->verdict == Verdict::Adds);

  const ItemEntry* walker = layout->find_item("walker");
  REQUIRE(walker != nullptr);
  CHECK(walker->vtables == std::vector<std::uint32_t>{0x9000, 0x9100});
  CHECK(walker->rva == 0);

  const ItemEntry* rota = layout->find_item("rota");
  REQUIRE(rota != nullptr);
  CHECK(rota->stride == 32);
  CHECK(rota->count == 168);
  CHECK(layout->find_item("nothing") == nullptr);
  CHECK(layout->find_item(1003) == nullptr);
}

TEST_CASE("the table is schema 2's: a schema 1 file holds neither items nor provenance, and schema 2 needs provenance") {
  const std::string one = with(table({kLantern}), "schema = 2", "schema = 1");
  const SchemaErrors errors = check(one);
  CHECK(has_error(errors, "layout.toml", "has an unknown key \"item\""));
  CHECK(has_error(errors, "layout.toml", "has an unknown key \"provenance\""));
  CHECK(has_error(check(std::string(kHead) + std::string(kLantern)), "layout.toml: provenance", "is missing"));
  // A schema 2 file with no item yet is a table all the same, and a schema 1 file reads as before.
  CHECK(check(table({})).empty());
  CHECK(check(with(std::string(kHead), "schema = 2", "schema = 1")).empty());
  CHECK_FALSE(check(with(table({}), "schema = 2", "schema = 3")).empty());
}

TEST_CASE("the table's provenance: our own derivation, the tool, the round and the date, in their forms") {
  const auto bad = [](std::string_view from, std::string_view to, std::string_view where, std::string_view problem) {
    const SchemaErrors errors = check(with(table({}), from, to));
    return has_error(errors, where, problem);
  };
  CHECK(bad("own-derivation", "copied", "layout.toml: provenance.method", "must be own-derivation"));
  CHECK(bad("0.1.0-test.1", "a tool", "layout.toml: provenance.tool", "version label"));
  CHECK(bad("00112233445566778899aabbccddeeff0011", "00112233445566778899AABBCCDDEEFF0011",
            "layout.toml: provenance.tool_sha256", "64 lower-case hex digits"));
  CHECK(bad("round = \"l3\"", "round = \"Round 3\"", "layout.toml: provenance.round", "label of ours"));
  CHECK(bad("2026-01-02", "2 January", "layout.toml: provenance.date", "YYYY-MM-DD"));
  CHECK(bad("leads = [\"our own notes, section 4\"]", "leads = [4]", "layout.toml: provenance.leads[0]", "string"));
  CHECK(bad("date = \"2026-01-02\"\n", "", "layout.toml: provenance.date", "is missing"));
  CHECK(bad("round = \"l3\"", "round = \"l3\"\npath = \"somewhere\"", "layout.toml: provenance", "unknown key \"path\""));
}

TEST_CASE("an item is an id of the catalogue, under its label, its number and its kind") {
  CHECK(has_error(check(table({kLantern}), LayoutMode::Release, false), "layout.toml: item",
                  "needs the catalogue"));
  CHECK(refused(with(kLantern, "[item.lantern]", "[item.lamp]"), "layout.toml: item.lamp",
                "is no label of the catalogue"));
  CHECK(refused(with(kLantern, "id = 1001", "id = 1003"), "layout.toml: item.lantern.id",
                "is not the catalogue's number"));
  CHECK(refused(with(with(kLantern, "[item.lantern]", "[item.ledger]"), "id = 1001", "id = 1002"),
                "layout.toml: item.ledger", "is a retired id"));
  // A number outside its kind's block never reaches the catalogue.
  CHECK(refused(with(kLantern, "id = 1001", "id = 2001"), "layout.toml: item.lantern.id",
                "lies outside its kind's block"));
  CHECK(refused(with(kLantern, "kind = \"static\"", "kind = \"gadget\""), "layout.toml: item.lantern.kind",
                "must be one of"));
  // The catalogue's kind: the walker's entry, written as a table under a table's number.
  CHECK(refused(with(with(with(kRota, "[item.rota]", "[item.walker]"), "id = 4001", "id = 3001"),
                     "kind = \"table\"", "kind = \"class\""),
                "layout.toml: item.walker.vtables", "is missing"));
  // Two items under one id: the second's label is not the catalogue's for it either.
  const SchemaErrors twice = check(table({kLantern, with(with(kLantern, "[item.lantern]", "[item.larder]"),
                                                         "status = \"confirmed\"", "status = \"confirmed\"")}));
  CHECK(has_error(twice, "layout.toml: item.larder.id", "is another item's id"));
  CHECK(has_error(twice, "layout.toml: item.larder.id", "is not the catalogue's number"));
  CHECK(refused(with(kLantern, "[item.lantern]", "[item.Lantern]"), "layout.toml: item",
                "not a table named with a label of ours"));
}

TEST_CASE("an item's numbers lie inside the build's image, each in its form") {
  CHECK(refused(with(kLantern, "rva = 0x8000", "rva = 0"), "layout.toml: item.lantern.rva", ""));
  CHECK(refused(with(kLantern, "rva = 0x8000", "rva = 0x11000"), "layout.toml: item.lantern.rva",
                "lies outside the build's image"));
  CHECK(refused(with(kLantern, "rva = 0x8000", "rva = 0x8004"), "layout.toml: item.lantern.rva",
                "must be a multiple of 8"));
  CHECK(refused(with(kLantern, "rva = 0x8000\n", ""), "layout.toml: item.lantern.rva", "is missing"));
  CHECK(refused(with(kLantern, "0x1200", "0x11000"), "layout.toml: item.lantern.witnesses[1]",
                "lies outside the build's image"));
  CHECK(refused(with(kLantern, "0x1200", "0x1010"), "layout.toml: item.lantern.witnesses[1]", "is listed twice"));
  CHECK(refused(with(kLantern, "[0x1010, 0x1200, 0x1040]", "[1, 2, 3, 4, 5, 6, 7, 8, 9]"),
                "layout.toml: item.lantern.witnesses", "has more than 8 offsets"));
  CHECK(refused(with(kLantern, "0x1200", "\"0x1200\""), "layout.toml: item.lantern.witnesses[1]",
                "must be a whole number"));
  // A function: its length, within the image.
  CHECK(refused(with(kBell, "length = 0x5D\n", ""), "layout.toml: item.ring_bell.length", "is missing"));
  CHECK(refused(with(kBell, "length = 0x5D", "length = 0"), "layout.toml: item.ring_bell.length", ""));
  CHECK(refused(with(kBell, "length = 0x5D", "length = 0xF001"), "layout.toml: item.ring_bell.length",
                "runs past the build's image"));
  CHECK(refused(with(kBell, "length = 0x5D", "length = 0x100001"), "layout.toml: item.ring_bell.length", ""));
  // A function need not be aligned as a static must.
  CHECK(check(table({with(kBell, "rva = 0x2000", "rva = 0x2003")})).empty());
  // A table: its entries' size and count, within the image.
  CHECK(refused(with(kRota, "stride = 32", "stride = 4"), "layout.toml: item.rota.stride", ""));
  CHECK(refused(with(kRota, "stride = 32", "stride = 65"), "layout.toml: item.rota.stride", ""));
  CHECK(refused(with(kRota, "count = 168", "count = 0"), "layout.toml: item.rota.count", ""));
  CHECK(refused(with(kRota, "count = 168", "count = 900"), "layout.toml: item.rota.count",
                "runs past the build's image"));
  // A class: its tables of methods, ascending, none twice, and no single offset.
  CHECK(refused(with(kWalker, "[0x9000, 0x9100]", "[0x9100, 0x9000]"), "layout.toml: item.walker.vtables[1]",
                "must be above the offset before it"));
  CHECK(refused(with(kWalker, "[0x9000, 0x9100]", "[0x9000, 0x9000]"), "layout.toml: item.walker.vtables[1]",
                "is listed twice"));
  CHECK(refused(with(kWalker, "[0x9000, 0x9100]", "[]"), "layout.toml: item.walker.vtables",
                "must list 1 to 64 offsets"));
  CHECK(refused(with(kWalker, "vtables = [0x9000, 0x9100]", "vtables = [0x9000]\nrva = 0x9000"),
                "layout.toml: item.walker", "unknown key \"rva\""));
  CHECK(refused(with(kWalker, "found_by", "witnesses = [0x1000]\nfound_by"), "layout.toml: item.walker",
                "unknown key \"witnesses\""));
}

TEST_CASE("an item's recipes find its kind, and its words come from the closed sets") {
  CHECK(refused(with(kLantern, "[\"accessor\", \"instance_class\"]", "[]"), "layout.toml: item.lantern.found_by",
                "must name at least one recipe"));
  CHECK(refused(with(kLantern, "found_by = [\"accessor\", \"instance_class\"]\n", ""),
                "layout.toml: item.lantern.found_by", "is missing"));
  CHECK(refused(with(kLantern, "\"instance_class\"", "\"guesswork\""), "layout.toml: item.lantern.found_by[1]",
                "must be one of accessor"));
  CHECK(refused(with(kLantern, "\"instance_class\"", "\"accessor\""), "layout.toml: item.lantern.found_by[1]",
                "is listed twice"));
  CHECK(refused(with(kLantern, "\"instance_class\"", "\"table\""), "layout.toml: item.lantern.found_by",
                "names table, which finds no static"));
  CHECK(refused(with(kBell, "[\"relation\"]", "[\"accessor\"]"), "layout.toml: item.ring_bell.found_by",
                "names accessor, which finds no function"));
  CHECK(refused(with(kWalker, "[\"class\"]", "[\"relation\"]"), "layout.toml: item.walker.found_by",
                "names relation, which finds no class"));
  CHECK(refused(with(kRota, "[\"table\"]", "[\"invariant\"]"), "layout.toml: item.rota.found_by",
                "names invariant, which finds no table"));
  // A relation finds a function or a static; an invariant a static.
  CHECK(check(table({with(kLantern, "[\"accessor\", \"instance_class\"]", "[\"relation\", \"invariant\"]")})).empty());
  CHECK(refused(with(kLantern, "\"class\"]", "\"luck\"]"), "layout.toml: item.lantern.checks[3]",
                "must be one of placement"));
  CHECK(refused(with(kBell, "verdict = \"adds\"", "verdict = \"probably adds\""),
                "layout.toml: item.ring_bell.verdict", "must be one of adds, removes, reads, unknown"));
  CHECK(refused(with(kBell, "0123456789abcdef0123456789abcdef", "0123456789ABCDEF0123456789ABCDEF"),
                "layout.toml: item.ring_bell.digest", "32 lower-case hex digits"));
  CHECK(refused(with(kBell, "0123456789abcdef0123456789abcdef", "0123"), "layout.toml: item.ring_bell.digest",
                "32 lower-case hex digits"));
  // Only a function has a verdict or a digest.
  CHECK(refused(with(kLantern, "found_by", "verdict = \"reads\"\nfound_by"), "layout.toml: item.lantern",
                "unknown key \"verdict\""));
  CHECK(refused(with(kRota, "found_by", "digest = \"0123456789abcdef0123456789abcdef\"\nfound_by"),
                "layout.toml: item.rota", "unknown key \"digest\""));
  CHECK(refused(with(kLantern, "status = \"confirmed\"", "status = \"sure\""), "layout.toml: item.lantern.status",
                "must be one of lead, confirmed"));
  for (const Verdict v : {Verdict::Adds, Verdict::Removes, Verdict::Reads, Verdict::Unknown}) {
    CHECK(check(table({with(kBell, "\"adds\"", std::string("\"") + to_string(v) + "\"")})).empty());
  }
}

TEST_CASE("a check an item claims has what it checks, and a confirmed item has passed its kind's checks") {
  CHECK(refused(with(kLantern, "witnesses = [0x1010, 0x1200, 0x1040]\n", ""), "layout.toml: item.lantern.checks",
                "names witness, but the entry has no witnesses"));
  CHECK(refused(with(kBell, "digest = \"0123456789abcdef0123456789abcdef\"\n", ""),
                "layout.toml: item.ring_bell.checks", "names digest, but the entry has no digest"));
  CHECK(refused(with(kBell, "verdict = \"adds\"\n", ""), "layout.toml: item.ring_bell.checks",
                "names verdict, but the entry has no verdict"));
  // One recipe alone never confirms an id: the agreement of two sources is a check of its own.
  CHECK(refused(with(kLantern, "\"placement\", \"agree\", \"witness\"", "\"placement\", \"witness\""),
                "layout.toml: item.lantern.status", "is confirmed without the check agree"));
  CHECK(refused(with(kLantern, "\"placement\", \"agree\"", "\"agree\""), "layout.toml: item.lantern.status",
                "is confirmed without the check placement"));
  CHECK(refused(with(kLantern, "\"agree\", \"witness\"", "\"agree\""), "layout.toml: item.lantern.status",
                "is confirmed without the check witness"));
  CHECK(refused(with(kBell, "\"witness\", \"digest\", \"verdict\"", "\"witness\", \"verdict\""),
                "layout.toml: item.ring_bell.status", "is confirmed without the check digest"));
  CHECK(refused(with(kWalker, "\"placement\", \"agree\", \"class\"", "\"placement\", \"agree\""),
                "layout.toml: item.walker.status", "is confirmed without the check class"));
  CHECK(refused(with(kRota, "\"agree\", \"witness\"", "\"agree\""), "layout.toml: item.rota.status",
                "is confirmed without the check witness"));
  // A function is confirmed as a place without a verdict: the verdict is what a call needs, not what reading needs.
  CHECK(check(table({with(with(kBell, "verdict = \"adds\"\n", ""), ", \"verdict\"]", "]")})).empty());
}

TEST_CASE("a lead item needs no check, a dev build reads it and says so, and a release build refuses it") {
  const std::string lead = "[item.lantern]\nid = 1001\nkind = \"static\"\nrva = 0x8000\nfound_by = [\"accessor\"]\n";
  CHECK(check(table({lead}), LayoutMode::Dev).empty());
  CHECK(check(table({lead + "status = \"lead\"\n"}), LayoutMode::Dev).empty());
  CHECK(has_error(check(table({lead})), "layout.toml: item.lantern", "is a lead, which a release build refuses"));
  // The function R1 took for the registration, as its table will hold it: a lead with its verdict recorded.
  const std::string removes = "[item.ring_bell]\nid = 2001\nkind = \"function\"\nrva = 0x2000\nlength = 93\n"
                              "found_by = [\"relation\"]\nverdict = \"removes\"\nchecks = [\"placement\", \"verdict\"]\n";
  CHECK(check(table({removes}), LayoutMode::Dev).empty());
}

TEST_CASE("no key of an item can carry a byte of the game: a pattern, bytes, text or a name is an unknown key") {
  for (const std::string_view key : {"bytes", "pattern", "code", "text", "name", "prologue", "forms"}) {
    CAPTURE(key);
    const std::string item = with(kBell, "found_by", std::string(key) + " = \"48 8b ?? ??\"\nfound_by");
    CHECK(refused(item, "layout.toml: item.ring_bell", "has an unknown key \"" + std::string(key) + "\""));
  }
}
