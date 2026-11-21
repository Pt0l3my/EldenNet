// Expected<T, E> and Unexpected<E> (expected.hpp): a value or an error, either kept whole through copies, moves and
// assignments between the two states; void's form; a value type whose constructor is private (R7's BuildProfile
// shape); no object leaked or destroyed twice. All data is invented.
#include <doctest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "en/proto/expected.hpp"

using en::proto::Expected;
using en::proto::Unexpected;

namespace {

enum class Why { Missing, Garbled };

// Counts its live objects, to find a leak or a double destruction.
struct Counted {
  static inline int live = 0;
  std::string text;
  explicit Counted(std::string t = {}) : text(std::move(t)) { ++live; }
  Counted(const Counted& o) : text(o.text) { ++live; }
  Counted(Counted&& o) noexcept : text(std::move(o.text)) { ++live; }
  Counted& operator=(const Counted&) = default;
  Counted& operator=(Counted&&) noexcept = default;
  ~Counted() { --live; }
};

// A value that only its factory makes, as R7's BuildProfile: Expected never needs its default constructor.
class Gated {
 public:
  static Expected<Gated, Why> make(int n) {
    if (n < 0) return Unexpected{Why::Garbled};
    return Gated(n);
  }
  int n() const { return n_; }

 private:
  explicit Gated(int n) : n_(n) {}
  int n_;
};

Expected<int, std::string> parse_digit(char c) {
  if (c < '0' || c > '9') return Unexpected{std::string("not a digit")};
  return c - '0';
}

Expected<void, Why> check(bool ok) {
  if (!ok) return Unexpected{Why::Missing};
  return {};
}

}  // namespace

TEST_CASE("a value or an error, as the function answered") {
  const auto seven = parse_digit('7');
  REQUIRE(seven.has_value());
  CHECK(static_cast<bool>(seven));
  CHECK(*seven == 7);
  const auto bad = parse_digit('x');
  REQUIRE_FALSE(bad.has_value());
  CHECK(bad.error() == "not a digit");
  CHECK(bad.value_or(-1) == -1);
  CHECK(seven.value_or(-1) == 7);

  const Expected<std::string, Why> word = std::string("lantern");
  CHECK(word->size() == 7);
  CHECK(Expected<int, Why>{} == Expected<int, Why>(0));  // a default value where T has one
}

TEST_CASE("void's form: success, or an error") {
  CHECK(check(true).has_value());
  const auto failed = check(false);
  REQUIRE_FALSE(failed);
  CHECK(failed.error() == Why::Missing);
  CHECK(check(true) == check(true));
  CHECK_FALSE(check(true) == check(false));
}

TEST_CASE("a value whose constructor is private comes only from its factory") {
  static_assert(!std::is_default_constructible_v<Gated>);
  static_assert(!std::is_default_constructible_v<Expected<Gated, Why>>);
  const auto made = Gated::make(4);
  REQUIRE(made);
  CHECK(made->n() == 4);
  const auto refused = Gated::make(-1);
  REQUIRE_FALSE(refused);
  CHECK(refused.error() == Why::Garbled);
}

TEST_CASE("copies, moves and assignments between the two states keep every object whole") {
  Counted::live = 0;
  {
    Expected<Counted, Counted> a = Counted("value");
    Expected<Counted, Counted> b = Unexpected{Counted("error")};
    CHECK(Counted::live == 2);
    Expected<Counted, Counted> c = a;  // copy of a value
    Expected<Counted, Counted> d = b;  // copy of an error
    CHECK(Counted::live == 4);
    c = d;  // a value becomes an error
    REQUIRE_FALSE(c);
    CHECK(c.error().text == "error");
    d = a;  // an error becomes a value
    REQUIRE(d);
    CHECK(d->text == "value");
    Expected<Counted, Counted> e = std::move(a);
    REQUIRE(e);
    CHECK(e->text == "value");
    e = std::move(b);
    REQUIRE_FALSE(e);
    CHECK(e.error().text == "error");
    CHECK(Counted::live == 5);  // a and b keep their moved-from objects
    Counted taken = *std::move(d);
    CHECK(taken.text == "value");
  }
  CHECK(Counted::live == 0);
}

TEST_CASE("trivially copyable when both sides are, and moved without throwing") {
  static_assert(std::is_trivially_copyable_v<Expected<int, Why>>);
  static_assert(std::is_nothrow_move_constructible_v<Expected<std::string, std::string>>);
  static_assert(std::is_nothrow_move_assignable_v<Expected<std::vector<int>, std::string>>);
  static_assert(!std::is_convertible_v<Why, Expected<int, Why>>, "an error converts only through Unexpected");
  const Expected<std::unique_ptr<int>, Why> owned = std::make_unique<int>(9);  // move-only values
  REQUIRE(owned);
  CHECK(**owned == 9);
}

TEST_CASE("Unexpected carries its error and compares by it") {
  const Unexpected why{Why::Garbled};
  CHECK(why.error() == Why::Garbled);
  CHECK(why == Unexpected{Why::Garbled});
  const Expected<int, Why> from_lvalue = why;
  CHECK(from_lvalue.error() == Why::Garbled);
  CHECK(Expected<int, Why>(Unexpected{Why::Missing}) != Expected<int, Why>(Unexpected{Why::Garbled}));
  CHECK(Expected<int, Why>(1) != Expected<int, Why>(Unexpected{Why::Missing}));
}
