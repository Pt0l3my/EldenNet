// The scrubbed logger's lines (logline.hpp): numbers and words only, whatever a caller passes.
#include <doctest.h>

#include <ostream>
#include <string>

#include "en/shell/logline.hpp"

using namespace en::shell;

TEST_CASE("logline: words and numbers, and anything else redacted") {
  Line l("gate");
  l.word("result", "ok").num("ms", 12).sig("delta", -3).hex("rva", 0x1A2B).flag("probe", true);
  CHECK(l.text() == "gate result=ok ms=12 delta=-3 rva=0x1A2B probe=1");
  Line p("paths");
  p.word("dll", "C:\\Games\\EldenNet\\eldennet.dll").word("name", "Tess Wayfarer").word("file", "eldennet.dll");
  CHECK(p.text() == "paths dll=redacted name=redacted file=eldennet.dll");
  Line k("x y");
  k.num("a b", 1);
  CHECK(k.text() == "redacted redacted=1");
  CHECK_FALSE(is_word(std::string(kMaxWord + 1, 'a')));
  CHECK(is_word(std::string(kMaxWord, 'a')));
}

TEST_CASE("logline: a line cut at its capacity drops whole pairs") {
  Line l("long");
  for (int i = 0; i < 100; ++i) l.num("key_number", 1234567890);
  CHECK(l.cut());
  CHECK(l.text().size() <= kLineBytes);
  CHECK(l.text().back() == '0');
}
