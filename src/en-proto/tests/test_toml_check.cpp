// The check of a TOML text before toml++ reads it (toml_check.hpp; WHY§122): each of toml++ 3.4.0's traps is refused
// before it can be reached, in gal/'s files and in the player's alike; a player's string may hold any letter, a gal/
// file's may not; and the files the engine's readers accept pass. Every text is invented.
#include <doctest.h>

#include <string>

#include "en/proto/toml_check.hpp"

using en::proto::TomlText;
using en::proto::unsafe_for_toml;

namespace {

bool refused(const std::string& text, TomlText whose) { return unsafe_for_toml(text, whose).has_value(); }

bool refused_both(const std::string& text) {
  return refused(text, TomlText::Gal) && refused(text, TomlText::Player);
}

}  // namespace

TEST_CASE("toml check: each of toml++'s traps is refused for gal/ and for the player alike") {
  // A table header whose first key does not start right after [ or [[.
  CHECK(refused_both("schema = 1\n[!]\n"));
  CHECK(refused_both("[=]\n"));
  CHECK(refused_both("  [ \x01]\n"));
  CHECK(refused_both("[[ \xC3\xA9 ]]\n"));
  // A character outside ASCII where whitespace may stand: after a key, after a value, inside a bare value.
  CHECK(refused_both("schema \xC3\x84 = 1\n"));
  CHECK(refused_both("x = 1 \xC3\xA9\n"));
  CHECK(refused_both("x = 0E1\"\xE2\x80\xA6\xC3\xA9\n"));
  // A } where an array wants a value, a date with a space and a digit, a time.
  CHECK(refused_both("x = [}]\n"));
  CHECK(refused_both("x = [1, }]\n"));
  CHECK(refused_both("x = [1,\n  # a comment\n  }]\n"));
  CHECK(refused_both("x = 1979-05-27 7,\n"));
  CHECK(refused_both("x = 5887-he-0a 6,\n"));
  CHECK(refused_both("x = 12:30\n"));
  // Multi-line strings, and a quote inside a bare word.
  CHECK(refused_both("x = \"\"\"a\nb\"\"\"\n"));
  CHECK(refused_both("x = '''a'''\n"));
  CHECK(refused_both("x = abc\"def\"\n"));
  // A word goes on, for toml++, through anything up to a terminator: a quote after any of it is refused, and so is a
  // control character (en_fuzz_config's finding: "= 2", control characters, then a quote and a letter outside ASCII).
  CHECK(refused_both("x = 2!'\xC3\xA9'\n"));
  CHECK(refused_both("x = 2\x03'\xEF\xB5\xBF'\n"));
  CHECK(refused_both("x = 2\x03\n"));
  CHECK(refused_both("# a comment \x7F\n"));
  CHECK(refused_both("x = \"a\x01b\"\n"));
  // toml++ scans a bare value through '=' and '[' too (en_fuzz_config's second finding: "4=[\"...\xC3\xA9").
  CHECK(refused_both("sbark_= 4=[\"[p?>>\xC3\xA9>>\n"));
  CHECK(refused_both("x = 4=[1]\n"));
  // After a value only a space, a comma, a closing bracket or brace, or a comment.
  CHECK(refused_both("x = \"a\" b\n"));
  CHECK(refused_both("x = \"a\"\"b\"\n"));
  CHECK(refused_both("t = { a = 1, }\n"));
  CHECK(refused_both("x = [1] 2\n"));
  // An escape of a character outside ASCII is never one.
  CHECK(refused_both("x = \"a\\\xC3\xA9\"\n"));
}

TEST_CASE("toml check: the line it names, and a fixed problem that quotes nothing") {
  const auto r = unsafe_for_toml("a = 1\nb = 2\n[!]\n", TomlText::Player);
  REQUIRE(r.has_value());
  CHECK(r->line == 3);
  CHECK(std::string(r->problem).find('!') == std::string::npos);
  const auto g = unsafe_for_toml("a = \"secret-\xC3\xA9\"\n", TomlText::Gal);
  REQUIRE(g.has_value());
  CHECK(std::string(g->problem).find("secret") == std::string::npos);
}

TEST_CASE("toml check: what each reader may hold") {
  const std::string plain =
      "\xEF\xBB\xBF# a comment with \xC3\xA9 in it\nschema = 1\n[[build]]\nkey = \"9.8.7.0-ww\"\nsize = 0x1000\n"
      "a.\"b\" = 'c'\nlist = [1, 2,\n  3]\ninline = { x = -1, y = +2.5, z = true }\n[ section ]\n[ \"quoted\" ]\n"
      "esc = \"\\u00A7 and \\\" and \\\\\"\n";
  CHECK_FALSE(refused(plain, TomlText::Gal));
  CHECK_FALSE(refused(plain, TomlText::Player));
  const std::string nested =
      "key=\"value\"\nk.\"q\".z = 1\n[[a.b]]\nx = [ [1, 2], [3], [] ]\ny = [\n  1, # one\n  2,\n]\n"
      "t = { a = { b = 1 }, c = [ { d = 'e' } ], f = {} }\n[ c . \"d\" ]\nn = -1.5e3\n";
  CHECK_FALSE(refused(nested, TomlText::Gal));
  CHECK_FALSE(refused(nested, TomlText::Player));
  // A player's string may hold any letter, where it truly opens a string; gal/'s writes \uXXXX.
  const std::string accented = "name = \"Caf\xC3\xA9 \xD0\x96\"\nother = '\xE2\x80\xA6'\n\"cl\xC3\xA9\" = 1\n";
  CHECK_FALSE(refused(accented, TomlText::Player));
  CHECK(refused(accented, TomlText::Gal));
  CHECK(refused("x = [\"a\", b\"\xC3\xA9\"]\n", TomlText::Player));
}
