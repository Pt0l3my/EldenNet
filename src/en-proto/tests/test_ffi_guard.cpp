// ffi_guard (R3): the answer when nothing throws; on any exception the fallback, with only its own component tripped.
#include <doctest.h>

#include <array>
#include <stdexcept>
#include <string>

#include "en/proto/ffi_guard.hpp"

using namespace en::proto;

namespace {

constexpr std::array kAll = {Component::DllMain,      Component::Task,      Component::Present, Component::Overlay,
                             Component::LlmTransport, Component::LlmWorker, Component::Tools};

void clear_all() {
  for (const Component c : kAll) clear_trip(c);
}

int tripped_count() {
  int n = 0;
  for (const Component c : kAll) n += tripped(c) ? 1 : 0;
  return n;
}

struct NotAnException {
  int code = 0;
};

}  // namespace

TEST_CASE("ffi_guard returns the entry's answer, and trips nothing, when nothing throws") {
  clear_all();
  CHECK(ffi_guard(Component::Task, -1, [] { return 7; }) == 7);
  CHECK(ffi_guard(Component::LlmTransport, std::string("fallback"), [] { return std::string("answer"); }) == "answer");
  int calls = 0;
  ffi_guard(Component::Present, [&] { ++calls; });
  CHECK(calls == 1);
  CHECK(tripped_count() == 0);
}

TEST_CASE("on a thrown exception ffi_guard returns the fallback and trips only its component") {
  clear_all();
  const std::size_t written = ffi_guard(Component::LlmTransport, std::size_t{0}, []() -> std::size_t {
    throw std::runtime_error("an invented failure");
  });
  CHECK(written == 0);
  CHECK(tripped(Component::LlmTransport));
  CHECK(tripped_count() == 1);

  // Anything thrown is caught, not only standard exceptions, and the void form trips the same way.
  ffi_guard(Component::LlmWorker, [] { throw NotAnException{3}; });
  CHECK(tripped(Component::LlmWorker));
  CHECK(ffi_guard(Component::Overlay, false, []() -> bool { throw 42; }) == false);
  CHECK(tripped(Component::Overlay));
  CHECK(tripped_count() == 3);
  CHECK_FALSE(tripped(Component::Task));
  CHECK_FALSE(tripped(Component::DllMain));

  // A tripped component's next entry still runs: what a trip means is its owner's decision.
  CHECK(ffi_guard(Component::LlmTransport, std::size_t{0}, [] { return std::size_t{5}; }) == 5);
  CHECK(tripped(Component::LlmTransport));

  clear_trip(Component::LlmTransport);
  CHECK_FALSE(tripped(Component::LlmTransport));
  CHECK(tripped(Component::LlmWorker));
  clear_all();
}

TEST_CASE("ffi_guard is noexcept, so an entry that starts with it never lets an exception reach its caller") {
  auto entry = [] { return ffi_guard(Component::Tools, 0, []() -> int { throw std::logic_error("invented"); }); };
  static_assert(noexcept(ffi_guard(Component::Tools, 0, [] { return 1; })));
  static_assert(noexcept(ffi_guard(Component::Tools, [] {})));
  CHECK(entry() == 0);
  CHECK(tripped(Component::Tools));
  clear_all();
}

TEST_CASE("components have names, and room for more") {
  CHECK(std::string(to_string(Component::LlmWorker)) == "LlmWorker");
  CHECK(std::string(to_string(Component::DllMain)) == "DllMain");
  CHECK(kComponentSlots >= kAll.size());
  // A value past the named ones has a flag of its own until the room runs out; past the room, nothing happens.
  const auto unnamed = static_cast<Component>(kAll.size());
  clear_trip(unnamed);
  trip(unnamed);
  CHECK(tripped(unnamed));
  CHECK(tripped_count() == 0);
  clear_trip(unnamed);
  const auto outside = static_cast<Component>(kComponentSlots);
  trip(outside);
  CHECK_FALSE(tripped(outside));
}
