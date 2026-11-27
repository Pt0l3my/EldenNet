// en-store's tests: doctest's runner, or the crash child when the first argument asks for it (test_crash.cpp).
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <string_view>

#include "support.hpp"

int main(int argc, char** argv) {
  if (argc >= 2 && std::string_view(argv[1]) == "--crash-child") return en::store::test::crash_child(argc, argv);
  doctest::Context ctx(argc, argv);
  return ctx.run();
}
