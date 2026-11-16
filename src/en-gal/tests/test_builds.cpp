// gal/builds.toml and the version gate (builds.hpp; R7): the schema with every problem reported, and the gate's exact
// match over a FakeImage's fingerprint. Every build, version and name here is invented.
#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "en/gal/builds.hpp"
#include "fake_image.hpp"

using namespace en::gal;
using namespace en::gal::test;

namespace {

std::string entry(std::string_view key, std::string_view version, std::string_view region, std::uint32_t stamp,
                  std::uint32_t size, std::string_view extra = "") {
  return "[[build]]\nkey = \"" + std::string(key) + "\"\napp = \"4.5.6\"\nfile_version = \"" + std::string(version) +
         "\"\nproduct_name = \"INVENTED GAME\"\nregion = \"" + std::string(region) +
         "\"\ntime_date_stamp = " + std::to_string(stamp) + "\nsize_of_image = " + std::to_string(size) +
         "\nlayout = \"layouts/" + std::string(key) + ".toml\"\nstatus = \"supported\"\n" + std::string(extra) + "\n";
}

SchemaErrors refusal(std::string_view text) {
  auto builds = parse_builds(text);
  REQUIRE_FALSE(builds.has_value());
  return builds.error();
}

bool has_error(const SchemaErrors& errors, std::string_view where, std::string_view problem) {
  for (const SchemaError& e : errors) {
    if (e.where == where && e.problem.find(problem) != std::string::npos) return true;
  }
  return false;
}

ImageInfo image_of(const FakeVersion& version, std::uint32_t stamp) {
  FakeImageSpec spec;
  spec.sections.push_back(FakeSection{".text", kCode, std::vector<std::uint8_t>(0x100, 0x00), 0});
  spec.version = version;
  spec.time_date_stamp = stamp;
  const BuiltImage built = build(spec);
  const SpanSource source(built.file);
  auto image = read_image(source, ImageLayout::File);
  REQUIRE(image.has_value());
  return *image;
}

}  // namespace

TEST_CASE("builds.toml: every build with its fingerprint, its layout and its status") {
  const std::string text = "schema = 1\n" +
                           entry("9.8.7.0-ww", "9.8.7.0", "ww", 0x1234, 0x4000, "dlrf_name_layout = true") +
                           entry("9.8.7.1-jp", "9.8.7.1", "jp", 0x1235, 0x4000);
  const auto builds = parse_builds(text);
  REQUIRE(builds.has_value());
  REQUIRE(builds->entries.size() == 2);
  const BuildEntry& ww = builds->entries[0];
  CHECK(ww.key == "9.8.7.0-ww");
  CHECK(ww.app == "4.5.6");
  CHECK(ww.file_version == std::array<std::uint16_t, 4>{9, 8, 7, 0});
  CHECK(ww.product_name == "INVENTED GAME");
  CHECK(ww.region == Region::Ww);
  CHECK(ww.time_date_stamp == 0x1234);
  CHECK(ww.size_of_image == 0x4000);
  CHECK(ww.layout == "layouts/9.8.7.0-ww.toml");
  CHECK(ww.dlrf_name_layout);
  CHECK(ww.status == BuildStatus::Supported);
  CHECK(builds->entries[1].region == Region::Jp);
  CHECK_FALSE(builds->entries[1].dlrf_name_layout);
}

TEST_CASE("builds.toml refuses what its schema does not allow, every problem with its path") {
  CHECK(has_error(refusal("schema = 1\n"), "builds.toml: build", "is missing"));
  CHECK(has_error(refusal("schema = 1\nbuild = []\n"), "builds.toml: build", "must list 1 to 64 builds"));
  const auto one = [](std::string_view extra) {
    return "schema = 1\n" + entry("9.8.7.0-ww", "9.8.7.0", "ww", 1, 0x4000, extra);
  };
  CHECK(has_error(refusal(one("colour = 1")), "builds.toml: build[0]", "has an unknown key \"colour\""));
  CHECK(has_error(refusal("schema = 1\n" + entry("9.8.7.0-ww", "9.8.7.0", "eu", 1, 0x4000)),
                  "builds.toml: build[0].region", "must be one of ww, jp"));
  CHECK(has_error(refusal("schema = 1\n" + entry("9.8.7.0-jp", "9.8.7.0", "jp", 1, 0x4000)),
                  "builds.toml: build[0].region", "does not agree with file_version's fourth field"));
  CHECK(has_error(refusal("schema = 1\n" + entry("9.8.7.9-ww", "9.8.7.0", "ww", 1, 0x4000)),
                  "builds.toml: build[0].key", "must be file_version and region joined by -"));
  CHECK(has_error(refusal("schema = 1\n" + entry("9.8.7.0-ww", "9.8.70000.0", "ww", 1, 0x4000)),
                  "builds.toml: build[0].file_version", "must be four dot-separated numbers"));
  CHECK(has_error(refusal("schema = 1\n" + entry("9.8.7.0-ww", "9.8.7.0", "ww", 1, 0)),
                  "builds.toml: build[0].size_of_image",
                  "must be between 1 and 4294967295"));  // the placeholder, before the probe
  CHECK(has_error(refusal("schema = 1\n" + entry("9.8.7.0-ww", "9.8.7.0", "ww", 1, 0x4000) + "[[build]]\n"),
                  "builds.toml: build[1].key", "is missing"));
  std::string bad_layout = one("");
  bad_layout.replace(bad_layout.find("layouts/"), 8, "../");
  CHECK(has_error(refusal(bad_layout), "builds.toml: build[0].layout", "must be layouts/<name>.toml"));
  std::string bad_status = one("");
  bad_status.replace(bad_status.find("supported"), 9, "archived");
  CHECK(has_error(refusal(bad_status), "builds.toml: build[0].status", "must be one of supported, retired"));
  const std::string twice = "schema = 1\n" + entry("9.8.7.0-ww", "9.8.7.0", "ww", 1, 0x4000) +
                            entry("9.8.7.0-ww", "9.8.7.0", "ww", 1, 0x4000);
  CHECK(has_error(refusal(twice), "builds.toml: build[1].key", "is not unique"));
  CHECK(has_error(refusal(twice), "builds.toml: build[1]", "has the fingerprint of an earlier build"));
  CHECK(has_error(refusal("schema = 3\n" + entry("9.8.7.0-ww", "9.8.7.0", "ww", 1, 0x4000)), "builds.toml: schema",
                  "must be between 1 and 1"));
}

TEST_CASE("text that toml++ 3.4.0 cannot read safely is refused before it reads it") {
  // A table header that does not start with a key, and a character outside ASCII where whitespace may stand, are an
  // assertion in toml++'s debug build and undefined behaviour in its release one (toml_reader.hpp; found by
  // en_fuzz_galtoml, whose corpus keeps both inputs).
  const std::string_view header = "a line that starts with [ must open a table header with a key";
  const std::string_view ascii = "a character outside ASCII may stand only in a comment";
  CHECK(has_error(refusal("schema = 1\n[!]\n"), "builds.toml", "is refused before it is parsed: line 2: "));
  CHECK(has_error(refusal("schema = 1\n[!]\n"), "builds.toml", header));
  CHECK(has_error(refusal("\xEF\xBB\xBF[\x01]\n"), "builds.toml", "line 1: "));            // after a byte-order mark
  CHECK(has_error(refusal("schema = 1\n\t[[ \xC3\xA9 ]]\n"), "builds.toml", "line 2: "));  // not a bare key's character
  CHECK(has_error(refusal(std::string("[\0\0]\n", 5)), "builds.toml", header));
  CHECK(has_error(refusal("x = [\n[+1],\n]\n"), "builds.toml", header));  // valid TOML that gal/'s files never need
  CHECK(has_error(refusal("schema = 1\n[]\n"), "builds.toml", "is not valid TOML: line 2, column"));  // toml++'s own
  CHECK(has_error(refusal("schema \xC3\x84.A = 1\n"), "builds.toml", ascii));
  CHECK(has_error(refusal("schema = 1 \xE2\x80\x83\n"), "builds.toml", ascii));  // an em space after a value
  CHECK(has_error(refusal("schema = \"\"\"\nx\n\"\"\"\n"), "builds.toml", "multi-line strings are not used"));
  CHECK(has_error(refusal("schema = '''x'''\n"), "builds.toml", "multi-line strings are not used"));
  CHECK(has_error(refusal("schema = [}]\n"), "builds.toml", "a } stands where a value must"));
  const std::string_view dates = "a number with - or : after its first digit (a date or a time) is not used";
  CHECK(has_error(refusal("schema = 1979-05-27 7,\n"), "builds.toml", dates));  // the trap: a date, a space, one digit
  CHECK(has_error(refusal("schema = 07:32:00\n"), "builds.toml", dates));
  CHECK(has_error(refusal("schema = 5887-ab-0c\n"), "builds.toml", dates));
  CHECK(has_error(refusal("schema = -1\n"), "builds.toml: schema", "must be between 1 and 1"));  // a sign leads: read
  CHECK(has_error(refusal("schema = [1, # a comment\n  }]\n"), "builds.toml", "line 2: a } stands where a value must"));
  // toml++ scans a bare value through a quote, so outside ASCII is refused in what only looks like a string, and in a
  // string too: a string writes it as \uXXXX.
  CHECK(has_error(refusal("schema = a\"\xC3\xA9\"\n"), "builds.toml", ascii));
  CHECK(has_error(refusal("schema = \"\xC3\xA9\"\n"), "builds.toml", ascii));
  CHECK(has_error(refusal("schema = '\xC3\xA9'\n"), "builds.toml", ascii));
  // In a comment it is read as usual, and so are a quote and a # in a string, and an escape.
  std::string text =
      "# \xC3\xA9t\xC3\xA9 \"[!]\"\nschema = 1 # \xE2\x80\x83\n" + entry("9.8.7.0-ww", "9.8.7.0", "ww", 1, 0x4000);
  text.replace(text.find("INVENTED GAME"), 13, "INV\\u00C9NTED \\\" # GAME");
  const auto builds = parse_builds(text);
  REQUIRE(builds.has_value());
  CHECK(builds->entries[0].product_name == "INV\xC3\x89NTED \" # GAME");
}

TEST_CASE("the gate passes an exact fingerprint only, and names what differs") {
  FakeVersion version;
  version.file_version = {9, 8, 7, 0};
  version.product_name = u"INVENTED GAME";
  const ImageInfo image = image_of(version, 0x5EED);
  const std::string exact = "schema = 1\n" + entry("1.1.1.1-jp", "1.1.1.1", "jp", 7, 0x1000) +
                            entry("9.8.7.0-ww", "9.8.7.0", "ww", 0x5EED, image.size_of_image);
  const auto builds = parse_builds(exact);
  REQUIRE(builds.has_value());
  const auto profile = gate(image, *builds);
  REQUIRE(profile.has_value());
  CHECK(profile->key().index == 1);
  CHECK(profile->entry().key == "9.8.7.0-ww");

  const ImageInfo rebuilt = image_of(version, 0x5EEE);  // the same version, built again
  const auto differs = gate(rebuilt, *builds);
  REQUIRE_FALSE(differs.has_value());
  CHECK(differs.error() == GateError{GateProblem::FingerprintDiffers, kDiffersTimeDateStamp});
  CHECK(describe(differs.error()) == "build fingerprint differs (TimeDateStamp)");

  FakeVersion other = version;
  other.file_version = {9, 8, 7, 2};
  const auto unknown = gate(image_of(other, 0x5EED), *builds);
  REQUIRE_FALSE(unknown.has_value());
  CHECK(unknown.error().problem == GateProblem::UnknownBuild);
  CHECK(describe(unknown.error()) == "unknown build");

  FakeVersion renamed = version;
  renamed.product_name = u"ANOTHER INVENTED GAME";
  CHECK(gate(image_of(renamed, 0x5EED), *builds).error().problem == GateProblem::UnknownBuild);

  FakeVersion nameless = version;
  nameless.product_name.reset();
  CHECK(gate(image_of(nameless, 0x5EED), *builds).error().problem == GateProblem::NoVersion);
}

TEST_CASE("a BuildProfile comes only from the gate (R7)") {
  static_assert(!std::is_default_constructible_v<BuildProfile>);
  static_assert(!std::is_constructible_v<BuildProfile, BuildEntry, en::proto::BuildKey>);
  static_assert(std::is_copy_constructible_v<BuildProfile>);  // a copy of a gated profile is still gated
  CHECK(std::string(to_string(Region::Jp)) == "jp");
}

TEST_CASE("the fingerprint reads the version, the region rule and the image's sizes") {
  FakeVersion version;
  version.file_version = {9, 8, 7, 1};
  const ImageInfo image = image_of(version, 0xABCD);
  const auto fp = fingerprint(image);
  REQUIRE(fp.has_value());
  CHECK(fp->region == Region::Jp);
  CHECK(fp->product_name == "INVENTED GAME");
  CHECK(fp->time_date_stamp == 0xABCD);
  CHECK(fp->size_of_image == image.size_of_image);
  version.file_version = {9, 8, 7, 5};
  CHECK_FALSE(fingerprint(image_of(version, 1))->region.has_value());
}
