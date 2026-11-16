// The pattern grammar, the multi-pattern matcher and patterns.toml's schema (patterns.hpp). The matcher is checked
// against a plain search over the same invented bytes, fed whole and in chunks of many sizes, with and without gaps.
// Every pattern and id here is invented.
#include <doctest.h>

#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/patterns.hpp"

using namespace en::gal;

namespace {

BytePattern pattern(std::string_view text) {
  auto p = parse_pattern(text);
  REQUIRE(p.has_value());
  return *p;
}

GrammarError grammar_refusal(std::string_view text, GrammarLimits limits = {}) {
  const auto p = parse_pattern(text, limits);
  REQUIRE_FALSE(p.has_value());
  return p.error();
}

// Every start where `p` matches `bytes`, the plain way.
std::vector<std::uint64_t> naive(const BytePattern& p, const std::vector<std::byte>& bytes, std::uint64_t base) {
  std::vector<std::uint64_t> out;
  for (std::size_t i = 0; i + p.size() <= bytes.size(); ++i) {
    if (p.matches(std::span<const std::byte>(bytes).subspan(i))) out.push_back(base + i);
  }
  return out;
}

std::vector<std::byte> random_bytes(std::size_t n, std::uint32_t seed, int alphabet) {
  std::mt19937 rng(seed);
  std::vector<std::byte> out(n);
  for (std::byte& b : out) b = std::byte{static_cast<std::uint8_t>(0xA0 + rng() % static_cast<unsigned>(alphabet))};
  return out;
}

void plant(std::vector<std::byte>& bytes, std::size_t at, const BytePattern& p, std::uint8_t filler) {
  for (std::size_t i = 0; i < p.size(); ++i) {
    bytes[at + i] = std::byte{p.mask[i] == 0 ? filler : p.value[i]};
  }
}

// Feeds `bytes` (whose first byte is at `base`) to the matcher in chunks of `chunk`, overlapping as the scan does.
void feed_chunks(MultiMatcher& m, const std::vector<std::byte>& bytes, std::uint64_t base, std::size_t chunk) {
  m.begin(base);
  const std::size_t step = chunk - m.overlap();
  for (std::size_t pos = 0; pos < bytes.size(); pos += step) {
    const std::size_t len = std::min(chunk, bytes.size() - pos);
    const bool last = pos + len >= bytes.size();
    m.feed(base + pos, std::span<const std::byte>(bytes).subspan(pos, len), last);
    if (last) break;
  }
  m.finish(base + bytes.size());
}

constexpr std::string_view kProvenance = R"(provenance = { method = "own-derivation", leads = ["an invented lead"], )"
                                         R"(evidence = "docs/ops/probes/invented.json", date = "2026-11-16" })";

std::string function_entry(std::string_view id, std::string_view extra = "") {
  return "[[pattern]]\nid = \"" + std::string(id) +
         "\"\nkind = \"function\"\nsection = \".text\"\nbytes = \"5A 6B ?? 7C 8D 9E\"\nfn_len = [16, 4096]\n" +
         std::string(kProvenance) + "\n" + std::string(extra) + "\n";
}

SchemaErrors refusal(std::string_view text, std::span<const std::string_view> required = {}) {
  auto set = parse_patterns(text, required, "patterns.toml");
  REQUIRE_FALSE(set.has_value());
  return set.error();
}

bool has_error(const SchemaErrors& errors, std::string_view where, std::string_view problem) {
  for (const SchemaError& e : errors) {
    if (e.where == where && e.problem.find(problem) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("the grammar: hex pairs and ?? wildcards, whitespace between them ignored") {
  const BytePattern p = pattern("  6a 7b ??\t8c\n?? FD  ");
  CHECK(p.size() == 6);
  CHECK(p.fixed() == 4);
  CHECK(p.value == std::vector<std::uint8_t>{0x6A, 0x7B, 0x00, 0x8C, 0x00, 0xFD});
  CHECK(p.mask == std::vector<std::uint8_t>{0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF});
  CHECK(p.text() == "6A 7B ?? 8C ?? FD");
  CHECK(pattern(p.text()) == p);
  CHECK(pattern("6A7B??8C??FD") == p);  // pairs need no space between them
}

TEST_CASE("the grammar refuses what is not a pattern, saying where") {
  CHECK(grammar_refusal("") == GrammarError{GrammarProblem::Empty, 0});
  CHECK(grammar_refusal("   ") == GrammarError{GrammarProblem::Empty, 0});
  CHECK(grammar_refusal("6A 7G 8C 9D AE") == GrammarError{GrammarProblem::BadCharacter, 4});
  CHECK(grammar_refusal("6A 7B 8 9D AE") == GrammarError{GrammarProblem::HalfByte, 6});
  CHECK(grammar_refusal("6A 7B 8C 9D A") == GrammarError{GrammarProblem::HalfByte, 12});
  CHECK(grammar_refusal("6A ? 8C 9D AE") == GrammarError{GrammarProblem::HalfWildcard, 3});
  CHECK(grammar_refusal("6A 7? 8C 9D AE") == GrammarError{GrammarProblem::HalfWildcard, 4});
  CHECK(grammar_refusal("6A ?7 8C 9D AE") == GrammarError{GrammarProblem::HalfWildcard, 3});
  CHECK(grammar_refusal("6A ?? 8C ?? ??").problem == GrammarProblem::TooFewFixed);  // 2 fixed
  CHECK(grammar_refusal("0x6A 8C 9D AE").problem == GrammarProblem::BadCharacter);
  CHECK(grammar_refusal("6A 8C 9D AE BF", GrammarLimits{4, 4, true}).problem == GrammarProblem::TooLong);
  CHECK(grammar_refusal("6A ?? 9D", GrammarLimits{64, 1, false}) == GrammarError{GrammarProblem::Wildcard, 3});
  CHECK(describe(GrammarError{GrammarProblem::HalfByte, 6}) == "holds a hex digit without its pair (at character 6)");
}

TEST_CASE("a pattern matches with its wildcards, and never past the bytes it is given") {
  const BytePattern p = pattern("AB ?? CD EF 12");
  const std::vector<std::byte> hit = {std::byte{0xAB}, std::byte{0x00}, std::byte{0xCD}, std::byte{0xEF},
                                      std::byte{0x12}};
  const std::vector<std::byte> miss = {std::byte{0xAB}, std::byte{0x77}, std::byte{0xCE}, std::byte{0xEF},
                                       std::byte{0x12}};
  CHECK(p.matches(hit));
  CHECK_FALSE(p.matches(miss));
  CHECK_FALSE(p.matches(std::span<const std::byte>(hit).first(4)));
}

TEST_CASE("the matcher finds what a plain search finds, fed whole or in chunks of any size") {
  const std::vector<BytePattern> patterns = {
      pattern("A1 A2 ?? A3 A4"),                                   // anchored on a pair
      pattern("A5 ?? A6 ?? A7 ?? A8"),                             // no two fixed bytes in a row: anchored on one
      pattern("A1 A1 A1 A1"),                                      // overlaps itself
      pattern("?? ?? A9 AA AB AC ?? ??"),                          // wildcards at both ends
      pattern("B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA BB BC BD BE BF"),  // the longest
  };
  std::vector<std::byte> bytes = random_bytes(20000, 7, 12);
  for (std::size_t at = 0; at + 16 < bytes.size(); at += 997) plant(bytes, at, patterns[at % 5], 0xA1);
  plant(bytes, bytes.size() - 16, patterns[4], 0);  // at the very end
  plant(bytes, 0, patterns[1], 0x55);               // at the very start
  const std::uint64_t base = 0x40000;

  for (const std::size_t chunk : {std::size_t{16}, std::size_t{17}, std::size_t{31}, std::size_t{64}, std::size_t{1000},
                                  std::size_t{4096}, bytes.size()}) {
    MultiMatcher m(patterns, 100000);
    feed_chunks(m, bytes, base, chunk);
    CHECK(m.unscanned() == 0);
    for (std::size_t i = 0; i < patterns.size(); ++i) {
      const std::vector<std::uint64_t> expected = naive(patterns[i], bytes, base);
      const std::span<const std::uint64_t> found = m.hits(i);
      CHECK(m.count(i) == expected.size());
      CHECK(std::vector<std::uint64_t>(found.begin(), found.end()) == expected);
    }
  }
}

TEST_CASE("the matcher keeps at most max_hits positions a pattern, and counts on") {
  const std::vector<BytePattern> patterns = {pattern("C1 C2 C3 C4")};
  std::vector<std::byte> bytes(400, std::byte{0});
  for (std::size_t at = 0; at + 4 <= bytes.size(); at += 8) plant(bytes, at, patterns[0], 0);
  MultiMatcher m(patterns, 10);
  feed_chunks(m, bytes, 0, bytes.size());
  CHECK(m.count(0) == 50);
  CHECK(m.hits(0).size() == 10);
  CHECK(m.hits(0)[9] == 72);
  CHECK(m.count(7) == 0);  // no such pattern
  CHECK(m.hits(7).empty());
}

TEST_CASE("a gap between chunks is never examined, and is counted") {
  const std::vector<BytePattern> patterns = {pattern("D1 D2 D3 D4")};
  std::vector<std::byte> bytes(3000, std::byte{0});
  plant(bytes, 100, patterns[0], 0);
  plant(bytes, 1500, patterns[0], 0);  // in the gap
  plant(bytes, 2500, patterns[0], 0);
  MultiMatcher m(patterns);
  m.begin(0);
  m.feed(0, std::span<const std::byte>(bytes).first(1000), true);  // a run that ends: a gap follows
  m.feed(2000, std::span<const std::byte>(bytes).subspan(2000), true);
  m.finish(3000);
  CHECK(m.count(0) == 2);
  CHECK(m.unscanned() == 1000);
  const std::vector<std::uint64_t> expected = {100, 2500};
  CHECK(std::vector<std::uint64_t>(m.hits(0).begin(), m.hits(0).end()) == expected);
}

TEST_CASE("positions held back for the next chunk are counted if it never comes") {
  const std::vector<BytePattern> patterns = {pattern("E1 E2 E3 E4 E5 E6")};
  std::vector<std::byte> bytes(100, std::byte{0});
  MultiMatcher m(patterns);
  m.begin(0);
  m.feed(0, bytes, false);  // the last five positions wait for a chunk that does not come
  m.finish(100);
  CHECK(m.unscanned() == 5);
}

TEST_CASE("the matcher is fast enough for a section of tens of MiB") {
  // Not a benchmark: a guard against a quadratic slip. 8 MiB of invented bytes and six patterns.
  std::vector<std::byte> bytes = random_bytes(8u << 20, 3, 200);
  std::vector<BytePattern> patterns;
  for (int i = 0; i < 6; ++i) {
    BytePattern p = pattern("10 20 ?? ?? 30 40 ?? 50");
    p.value[0] = static_cast<std::uint8_t>(i);
    patterns.push_back(p);
    plant(bytes, 1000u + 1000000u * static_cast<std::size_t>(i), p, 0);
  }
  MultiMatcher m(patterns);
  feed_chunks(m, bytes, 0x1000, 1u << 20);
  for (std::size_t i = 0; i < patterns.size(); ++i) CHECK(m.count(i) == naive(patterns[i], bytes, 0x1000).size());
}

TEST_CASE("patterns.toml: functions, statics and idioms, with their defaults") {
  const std::string text =
      std::string("schema = 1\n") +
      function_entry("invented_entry",
                     "anchor_min = 9\nprologue = \"3C 4D\"\nbuilds = [\"9.8.7.0-ww\", \"9.8.7.1-jp\"]") +
      R"([[pattern]]
id = "InventedStatic"
kind = "static"
section = ".text"
bytes = "C7 D1 E2 ?? ?? ?? ?? F3 A4 B5"
expect_matches = 2
capture = { at = 3, kind = "rip_rel32", insn_end = 7 }
target_section = ".data"
rtti = "Invented::Holder"
)" + std::string(kProvenance) +
      R"(
[[pattern]]
id = "invented_idiom"
kind = "idiom"
section = ".text"
bytes = "D5 E6 F7 ?? ?? ?? ?? A8 B9 CA DB ?? EC FD 0E ?? ?? ?? ?? 1F ?? ?? ?? ??"
expect_matches = [3, 40]
captures = { slot = { at = 3, kind = "rip_rel32", insn_end = 7, section = ".data", align = 8 }, )"
      R"(getter = { at = 20, kind = "rip_rel32", insn_end = 24, section = ".text", function = true } }
)" + std::string(kProvenance) +
      "\n";
  constexpr std::string_view kRequired[] = {"invented_entry", "InventedStatic"};
  const auto set = parse_patterns(text, kRequired);
  REQUIRE(set.has_value());
  REQUIRE(set->patterns.size() == 3);
  const PatternSpec& fn = set->patterns[0];
  CHECK(fn.kind == PatternKind::Function);
  CHECK(fn.anchor_min == 9);
  CHECK(fn.fn_len_min == 16);
  CHECK(fn.fn_len_max == 4096);
  CHECK(fn.prologue == std::vector<std::uint8_t>{0x3C, 0x4D});
  CHECK(fn.expect_min == 1);
  CHECK(fn.expect_max == 1);
  CHECK(fn.builds.size() == 2);
  CHECK(fn.provenance.method == "own-derivation");
  CHECK(fn.provenance.leads == std::vector<std::string>{"an invented lead"});
  const PatternSpec* st = set->find("InventedStatic");
  REQUIRE(st != nullptr);
  CHECK(st->capture == Capture{3, 7});
  CHECK(st->target_section == ".data");
  CHECK(st->rtti == "Invented::Holder");
  CHECK(st->expect_min == 2);
  CHECK(st->expect_max == 2);
  const PatternSpec* idiom = set->find("invented_idiom");
  REQUIRE(idiom != nullptr);
  CHECK(idiom->expect_min == 3);
  CHECK(idiom->expect_max == 40);
  REQUIRE(idiom->captures.size() == 2);
  CHECK(idiom->captures[0].name == "getter");  // toml++ keeps a table's keys in order
  CHECK(idiom->captures[0].function);
  CHECK(idiom->captures[1].align == 8);
  CHECK(set->find("absent") == nullptr);

  // An idiom may expect as many hits as a large game's inlined accessors give, up to 65536; other kinds 4096.
  std::string many = text;
  many.replace(many.find("expect_matches = [3, 40]"), 24, "expect_matches = [1, 65536]");
  const auto wide = parse_patterns(many, kRequired);
  REQUIRE(wide.has_value());
  CHECK(wide->find("invented_idiom")->expect_max == kMaxIdiomHits);
  std::string too_many = text;
  too_many.replace(too_many.find("expect_matches = 2"), 18, "expect_matches = 4097");
  CHECK_FALSE(parse_patterns(too_many, kRequired).has_value());
}

TEST_CASE("patterns.toml refuses what its schema does not allow, every problem with its path") {
  CHECK(has_error(refusal("schema = 2\n"), "patterns.toml: schema", "must be between 1 and 1"));
  CHECK(has_error(refusal("schema = 1\n[[pattern\n"), "patterns.toml", "is not valid TOML: line 2"));
  constexpr std::string_view kNeeded[] = {"invented_needed"};
  CHECK(has_error(refusal("schema = 1\n", kNeeded), "patterns.toml", "lacks the required pattern \"invented_needed\""));
  const std::string two = "schema = 1\n" + function_entry("twice") + function_entry("twice");
  CHECK(has_error(refusal(two), "patterns.toml: pattern[1].id", "is not unique"));

  const auto one = [](std::string_view extra, std::string_view kind = "function",
                      std::string_view bytes = "5A 6B ?? 7C 8D 9E") {
    return "schema = 1\n[[pattern]]\nid = \"invented\"\nkind = \"" + std::string(kind) +
           "\"\nsection = \".text\"\nbytes = \"" + std::string(bytes) + "\"\n" + std::string(extra) + "\n";
  };
  const std::string prov = std::string(kProvenance) + "\n";
  CHECK(has_error(refusal(one("fn_len = [1, 2]\n" + prov + "colour = 1")), "patterns.toml: pattern[0]",
                  "has an unknown key \"colour\""));
  CHECK(has_error(refusal(one("fn_len = [1, 2]\n" + prov, "method")), "patterns.toml: pattern[0].kind",
                  "must be one of function"));
  CHECK(has_error(refusal(one("fn_len = [1, 2]\n" + prov, "function", "5A 6")), "patterns.toml: pattern[0].bytes",
                  "holds a hex digit without its pair (at character 3)"));
  CHECK(has_error(refusal(one("fn_len = [1, 2]\nanchor_min = 4\n" + prov)), "patterns.toml: pattern[0].anchor_min",
                  "must be between 5 and 4096"));
  CHECK(has_error(refusal(one("fn_len = [9, 2]\n" + prov)), "patterns.toml: pattern[0].fn_len",
                  "lowest above its highest"));
  CHECK(has_error(refusal(one(prov)), "patterns.toml: pattern[0].fn_len", "is missing"));
  CHECK(has_error(refusal(one("fn_len = [1, 2]\nprologue = \"3C ??\"\n" + prov)), "patterns.toml: pattern[0].prologue",
                  "holds a wildcard"));
  CHECK(has_error(refusal(one("fn_len = [1, 2]\nexpect_matches = 0\n" + prov)),
                  "patterns.toml: pattern[0].expect_matches", "must be between 1 and 4096"));
  CHECK(has_error(refusal(one("fn_len = [1, 2]\nbuilds = [\"9.8.7-ww\"]\n" + prov)),
                  "patterns.toml: pattern[0].builds[0]", "must be a build key"));
  CHECK(has_error(
      refusal(one(
          "fn_len = [1, 2]\nprovenance = { method = \"copied\", evidence = \"../x.json\", date = \"2026-13-01\" }")),
      "patterns.toml: pattern[0].provenance.method", "must be own-derivation"));
  CHECK(has_error(
      refusal(one(
          "fn_len = [1, 2]\nprovenance = { method = \"copied\", evidence = \"../x.json\", date = \"2026-13-01\" }")),
      "patterns.toml: pattern[0].provenance.evidence", "must be a relative path"));
  CHECK(has_error(refusal(one("fn_len = [1, 2]\nprovenance = { method = \"own-derivation\", evidence = \"a.json\", "
                              "date = \"2026-13-01\" }")),
                  "patterns.toml: pattern[0].provenance.date", "must be a date"));
  CHECK(has_error(refusal(one("fn_len = [1, 2]\n")), "patterns.toml: pattern[0].provenance", "is missing"));
  // A static's capture must lie within its pattern, and a static takes no function's fields.
  const std::string static_bytes = "6E 7F 8A ?? ?? ?? ?? 9B";
  CHECK(has_error(
      refusal(one("capture = { at = 5, kind = \"rip_rel32\", insn_end = 9 }\ntarget_section = \".data\"\n" + prov,
                  "static", static_bytes)),
      "patterns.toml: pattern[0].capture", "lies outside the pattern"));
  CHECK(
      has_error(refusal(one("capture = { at = 3, kind = \"rel8\", insn_end = 7 }\ntarget_section = \".data\"\n" + prov,
                            "static", static_bytes)),
                "patterns.toml: pattern[0].capture.kind", "must be rip_rel32"));
  CHECK(has_error(
      refusal(one(
          "capture = { at = 3, kind = \"rip_rel32\", insn_end = 7 }\ntarget_section = \".data\"\nfn_len = [1, 2]\n" +
              prov,
          "static", static_bytes)),
      "patterns.toml: pattern[0]", "has an unknown key \"fn_len\""));
  CHECK(has_error(
      refusal(
          one("capture = { at = 3, kind = \"rip_rel32\", insn_end = 7 }\ntarget_section = \".data\"\nrtti = \"a::\"\n" +
                  prov,
              "static", static_bytes)),
      "patterns.toml: pattern[0].rtti", "must be a class name"));
  CHECK(has_error(
      refusal(
          one("captures = { slot = { at = 3, kind = \"rip_rel32\", insn_end = 7, section = \".data\", align = 6 } }\n" +
                  prov,
              "idiom", static_bytes)),
      "patterns.toml: pattern[0].captures.slot.align", "must be a power of two"));
  CHECK(has_error(refusal(one("captures = {}\n" + prov, "idiom", static_bytes)), "patterns.toml: pattern[0].captures",
                  "must hold 1 to 8 captures"));
}

TEST_CASE("describe() gives where and what") {
  CHECK(describe(SchemaError{"patterns.toml: pattern[2].capture.at", "lies outside the pattern"}) ==
        "patterns.toml: pattern[2].capture.at: lies outside the pattern");
}
