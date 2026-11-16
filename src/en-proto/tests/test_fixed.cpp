// FixedVector, FixedString, the UTF-8 checks and the flag sets (fixed.hpp): fixed capacities that refuse rather than
// grow, slots past the end that hold nothing of what they held, strings that are always whole UTF-8, and flag sets
// that keep to their bits. All data is invented.
#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "en/proto/fixed.hpp"
#include "en/proto/snapshot.hpp"

using namespace en::proto;

namespace {

struct Pair {
  int a = 0;
  int b = 0;
  friend constexpr bool operator==(const Pair&, const Pair&) = default;
};

constexpr FixedVector<int, 4> make_three() {
  FixedVector<int, 4> v;
  (void)v.try_push_back(1);
  (void)v.try_push_back(2);
  (void)v.try_push_back(3);
  return v;
}

}  // namespace

TEST_CASE("a FixedVector holds up to its capacity and refuses past it") {
  FixedVector<Pair, 3> v;
  CHECK(v.empty());
  CHECK(v.capacity() == 3);
  CHECK(v.try_push_back(Pair{1, 2}));
  CHECK(v.try_push_back(Pair{3, 4}));
  CHECK(v.try_push_back(Pair{5, 6}));
  CHECK(v.full());
  CHECK_FALSE(v.try_push_back(Pair{7, 8}));
  CHECK(v.size() == 3);
  CHECK(v[2] == Pair{5, 6});
  CHECK(v.get(2) != nullptr);
  CHECK(v.get(3) == nullptr);
  int sum = 0;
  for (const Pair& p : v) sum += p.a + p.b;
  CHECK(sum == 21);
}

TEST_CASE("slots past the end hold T{}, so equal items make equal vectors") {
  FixedVector<int, 4> a;
  (void)a.try_push_back(9);
  (void)a.try_push_back(8);
  a.pop_back();
  FixedVector<int, 4> b;
  (void)b.try_push_back(9);
  CHECK(a == b);
  a.clear();
  CHECK(a.empty());
  a.pop_back();  // on an empty vector: nothing
  CHECK(a.size() == 0);
  CHECK(a == FixedVector<int, 4>{});
  CHECK_FALSE(a == b);
}

TEST_CASE("FixedVector works in constant expressions and is trivially copyable when its items are") {
  constexpr FixedVector<int, 4> three = make_three();
  static_assert(three.size() == 3 && three[1] == 2);
  static_assert(std::is_trivially_copyable_v<FixedVector<Pair, 8>>);
  CHECK(three.items().size() == 3);
}

TEST_CASE("valid_utf8 takes well-formed UTF-8 and nothing else") {
  CHECK(valid_utf8(""));
  CHECK(valid_utf8("plain ASCII"));
  CHECK(valid_utf8("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x8C\x99"));  // two, three and four bytes
  CHECK_FALSE(valid_utf8("\x80"));                                 // a stray continuation byte
  CHECK_FALSE(valid_utf8("\xC3"));                                 // cut at the end
  CHECK_FALSE(valid_utf8("\xC0\xAF"));                             // overlong
  CHECK_FALSE(valid_utf8("\xE0\x80\xAF"));                         // overlong, three bytes
  CHECK_FALSE(valid_utf8("\xED\xA0\x80"));                         // a surrogate
  CHECK_FALSE(valid_utf8("\xF4\x90\x80\x80"));                     // past U+10FFFF
  CHECK_FALSE(valid_utf8("\xFF"));
  CHECK(valid_utf8("\xF4\x8F\xBF\xBF"));  // U+10FFFF itself
}

TEST_CASE("a FixedString takes whole UTF-8 within its capacity, or refuses and keeps what it had") {
  FixedString<8> s;
  CHECK(s.try_assign("ember"));
  CHECK(s == "ember");
  CHECK(s.size() == 5);
  CHECK_FALSE(s.try_assign("ninebytes"));  // too long
  CHECK(s == "ember");
  CHECK_FALSE(s.try_assign(std::string_view("a\0b", 3)));  // a NUL
  CHECK_FALSE(s.try_assign("\xC3"));                       // not UTF-8
  CHECK(s == "ember");
  CHECK(FixedString<8>::from("exactly8").has_value());
  CHECK_FALSE(FixedString<8>::from("ninebytes").has_value());
  s.clear();
  CHECK(s.empty());
}

TEST_CASE("assign_prefix cuts at a whole character") {
  FixedString<5> s;
  CHECK(s.assign_prefix("ab\xE2\x82\xAC"));  // 2 + 3 bytes: fits exactly
  CHECK(s.view() == "ab\xE2\x82\xAC");
  CHECK(s.assign_prefix("abc\xE2\x82\xAC"));  // the euro sign would end at byte 6: cut before it
  CHECK(s.view() == "abc");
  CHECK(s.assign_prefix("\xF0\x9F\x8C\x99\xF0\x9F\x8C\x99"));  // two four-byte characters: one fits
  CHECK(s.size() == 4);
  CHECK_FALSE(s.assign_prefix("ok\xFF"));  // not UTF-8: nothing changes
  CHECK(s.size() == 4);
  CHECK(utf8_prefix("abc", 10) == 3);
}

TEST_CASE("FixedString compares by its text and is trivially copyable") {
  static_assert(std::is_trivially_copyable_v<FixedString<48>>);
  FixedString<16> a;
  FixedString<16> b;
  (void)a.try_assign("longer text");
  (void)a.try_assign("short");
  (void)b.try_assign("short");
  CHECK(a == b);  // what a held before leaves nothing behind
}

TEST_CASE("flag sets combine, and ~ keeps to the bits the set defines") {
  const StatusSet poisoned = bit_of(Status::Poison);
  StatusSet set = poisoned | bit_of(Status::Sleep);
  CHECK(has_all(set, poisoned));
  CHECK(any(set));
  CHECK_FALSE(has_all(set, bit_of(Status::Madness)));
  set &= ~poisoned;
  CHECK_FALSE(has_all(set, poisoned));
  CHECK(set == bit_of(Status::Sleep));
  set ^= bit_of(Status::Sleep);
  CHECK_FALSE(any(set));
  CHECK(bits_of(~StatusSet::None) == 0x7F);  // seven statuses, the eighth bit never set
  CHECK(bits_of(~GroupSet::None) == 0x7FF);  // eleven groups
  GroupSet groups = bit_of(Group::Session) | bit_of(Group::Status);
  groups |= bit_of(Group::World);
  CHECK(bits_of(groups) == ((1u << 0) | (1u << 1) | (1u << 10)));
}
