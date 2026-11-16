// The caps on reading JSON (ARCH §10.2): 256 KB and 32 deep, checked before the parser sees the text, each just under
// and just over; brackets inside strings never count.
#include <doctest.h>

#include <string>

#include "en/proto/json.hpp"

using namespace en::proto;

namespace {

std::string nested(std::size_t depth, char open = '[', char close = ']') {
  return std::string(depth, open) + std::string(depth, close);
}

// A JSON string literal of exactly `size` bytes, quotes included.
std::string string_of_size(std::size_t size) { return "\"" + std::string(size - 2, 'x') + "\""; }

}  // namespace

TEST_CASE("depth: a scalar is at 0, each array or object one more, and brackets inside strings do not count") {
  CHECK(within_depth("17", 0));
  CHECK(within_depth("\"[[[[\"", 0));
  CHECK_FALSE(within_depth("[]", 0));
  CHECK(within_depth("[]", 1));
  CHECK(within_depth(R"({"a": [1, {"b": []}]})", 4));
  CHECK_FALSE(within_depth(R"({"a": [1, {"b": []}]})", 3));
  CHECK(within_depth(R"(["\"[[[[", "\\", "]]"])", 1));  // an escaped quote, then an escaped backslash
  CHECK(within_depth(R"({"a":[]}{"b":[]})", 2));         // siblings do not add up
  CHECK(within_depth("]]]][[", 2));                      // stray closers count nothing; the parser refuses them
}

TEST_CASE("depth: 32 levels parse, 33 are refused before the parser sees them") {
  CHECK(parse_json(nested(32)).ok());
  CHECK(parse_json(nested(32, '{', '}')).refusal == JsonRefusal::Malformed);  // {{ is no JSON, but deep enough to try
  const ParsedJson deep = parse_json(nested(33));
  CHECK(deep.refusal == JsonRefusal::TooDeep);
  CHECK(deep.value.is_discarded());

  std::string objects;
  for (int i = 0; i < 32; ++i) objects += "{\"k\":";
  objects += "0" + std::string(32, '}');
  CHECK(parse_json(objects).ok());
  CHECK(parse_json("[" + objects + "]").refusal == JsonRefusal::TooDeep);

  // Far deeper than the parser could follow without the check: refused at once.
  CHECK(parse_json(nested(100000)).refusal == JsonRefusal::TooDeep);
  CHECK(describe(JsonRefusal::TooDeep) == "nested deeper than 32");
}

TEST_CASE("size: 256 KB parse, a byte more is refused, whatever the text holds") {
  const std::string at_cap = string_of_size(kMaxJsonBytes);
  REQUIRE(at_cap.size() == 256u * 1024u);
  const ParsedJson fits = parse_json(at_cap);
  REQUIRE(fits.ok());
  CHECK(fits.value.get_ref<const std::string&>().size() == kMaxJsonBytes - 2);
  CHECK(parse_json(string_of_size(kMaxJsonBytes + 1)).refusal == JsonRefusal::TooLarge);
  CHECK(parse_json(std::string(kMaxJsonBytes + 1, '[')).refusal == JsonRefusal::TooLarge);  // size is checked first
  CHECK(describe(JsonRefusal::TooLarge) == "larger than 256 KB");
}

TEST_CASE("limits of the caller's own, and malformed text") {
  const JsonLimits small{16, 2};
  CHECK(parse_json("[[1]]", small).ok());
  CHECK(parse_json("[[[1]]]", small).refusal == JsonRefusal::TooDeep);
  CHECK(parse_json("\"0123456789abcdef\"", small).refusal == JsonRefusal::TooLarge);
  CHECK(describe(JsonRefusal::TooLarge, small) == "larger than 16 bytes");
  CHECK(describe(JsonRefusal::TooDeep, small) == "nested deeper than 2");
  for (const char* bad : {"", "{", "[1,]", "{\"a\" 1}", "nul", "\"\xff\"", "1 2", "// a comment\n1"}) {
    INFO(bad);
    const ParsedJson parsed = parse_json(bad);
    CHECK(parsed.refusal == JsonRefusal::Malformed);
    CHECK(parsed.value.is_discarded());
  }
  CHECK(describe(JsonRefusal::Malformed) == "not valid JSON");
  CHECK(describe(JsonRefusal::None).empty());
}

TEST_CASE("read_json refuses a text past the caps, or not JSON, with a clear message") {
  CHECK(read_json<int>("17") == 17);
  CHECK_THROWS_WITH_AS(read_json<int>(nested(33)), "the text is nested deeper than 32", ProtoError);
  CHECK_THROWS_WITH_AS(read_json<int>(string_of_size(kMaxJsonBytes + 1)), "the text is larger than 256 KB", ProtoError);
  CHECK_THROWS_WITH_AS(read_json<int>("{"), "the text is not valid JSON", ProtoError);
}
