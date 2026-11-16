// The guarded copy (reader.hpp; ARCH §6.5, DESIGN R4), under Windows or Wine, on pages of the test's own holding
// invented bytes. A copy from good memory arrives whole. A copy from a page reserved but not committed, from a guard
// page, from anywhere nothing is mapped or readable, and across the end of a committed region fails cleanly, with the
// destination zeroed, and the program goes on. Faults repeated many times, on one thread or several, leave nothing
// behind.
#include <doctest.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <thread>
#include <vector>

#include "en/gal/reader.hpp"

using en::gal::CopyOutcome;
using en::gal::seh_copy;

namespace {

constexpr std::byte kFill{0xA5};  // what a destination holds before a copy, so that a zeroed one shows

std::size_t page_size() {
  SYSTEM_INFO info{};
  GetSystemInfo(&info);
  return info.dwPageSize;
}

std::byte pattern(std::size_t i) { return static_cast<std::byte>((i * 7 + 3) & 0xFF); }

// Pages of the test's own: `reserved` pages reserved, of which the first `committed` are committed read-write and
// hold the pattern. Released at the end of the test.
// SAFETY: base_ is the test's own reservation of `reserved` pages; it writes only the pages it committed, and changes
// the protection of whole pages inside the reservation.
class Pages {
 public:
  Pages(std::size_t reserved, std::size_t committed) : page_(page_size()) {
    base_ = static_cast<std::byte*>(VirtualAlloc(nullptr, reserved * page_, MEM_RESERVE, PAGE_NOACCESS));
    REQUIRE(base_ != nullptr);
    if (committed > 0) {
      REQUIRE(VirtualAlloc(base_, committed * page_, MEM_COMMIT, PAGE_READWRITE) == base_);
      for (std::size_t i = 0; i < committed * page_; ++i) base_[i] = pattern(i);
    }
  }
  ~Pages() { VirtualFree(base_, 0, MEM_RELEASE); }
  Pages(const Pages&) = delete;
  Pages& operator=(const Pages&) = delete;

  [[nodiscard]] std::size_t page() const { return page_; }

  // SAFETY: a pointer of the test's own, turned into the address the copy takes; nothing is made from it here.
  [[nodiscard]] std::uint64_t address(std::size_t offset) const {
    return reinterpret_cast<std::uintptr_t>(base_) + offset;
  }

  void protect(std::size_t index, DWORD protection) {
    DWORD old = 0;
    REQUIRE(VirtualProtect(base_ + index * page_, page_, protection, &old) != 0);
  }

 private:
  std::size_t page_;
  std::byte* base_ = nullptr;
};

bool all_zero(const std::vector<std::byte>& v) {
  return std::all_of(v.begin(), v.end(), [](std::byte b) { return b == std::byte{0}; });
}

bool holds_pattern(const std::vector<std::byte>& v, std::size_t from) {
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (v[i] != pattern(from + i)) return false;
  }
  return true;
}

// A copy that must fail with `code`, leaving the destination zeroed.
bool fails_with(std::uint64_t address, std::vector<std::byte>& dst, std::uint32_t code) {
  std::fill(dst.begin(), dst.end(), kFill);
  const CopyOutcome out = seh_copy(address, dst);
  return !out.copied && out.code == code && all_zero(dst);
}

// A copy that must arrive whole, from offset `from` of a Pages.
bool arrives(const Pages& pages, std::size_t from, std::vector<std::byte>& dst) {
  std::fill(dst.begin(), dst.end(), kFill);
  const CopyOutcome out = seh_copy(pages.address(from), dst);
  return out.copied && out.code == 0 && holds_pattern(dst, from);
}

}  // namespace

TEST_CASE("a copy from good memory arrives whole") {
  Pages pages(2, 2);
  for (const std::size_t n : {std::size_t{1}, std::size_t{7}, std::size_t{64}, pages.page(), 2 * pages.page()}) {
    std::vector<std::byte> dst(n);
    CHECK(arrives(pages, 0, dst));
  }
  std::vector<std::byte> dst(64);
  CHECK(arrives(pages, pages.page() - 32, dst));  // across the edge between two committed pages
}

TEST_CASE("an empty destination copies nothing, wherever the address points") {
  std::vector<std::byte> none;
  const CopyOutcome out = seh_copy(0, none);
  CHECK(out.copied);
  CHECK(out.code == 0);
}

TEST_CASE("a copy from a page reserved but not committed fails cleanly") {
  Pages pages(1, 0);
  std::vector<std::byte> dst(16);
  CHECK(fails_with(pages.address(0), dst, EXCEPTION_ACCESS_VIOLATION));
  Pages good(1, 1);
  CHECK(arrives(good, 0, dst));  // and the program goes on
}

TEST_CASE("a copy from a guard page fails cleanly, and the guard is gone after") {
  Pages pages(1, 1);
  pages.protect(0, PAGE_READWRITE | PAGE_GUARD);
  std::vector<std::byte> dst(16);
  CHECK(fails_with(pages.address(0), dst, EXCEPTION_GUARD_PAGE));
  CHECK(arrives(pages, 0, dst));  // Windows lifted the guard as it raised the fault
}

TEST_CASE("a copy from where nothing is mapped or readable fails cleanly") {
  std::vector<std::byte> dst(16);
  CHECK(fails_with(0, dst, EXCEPTION_ACCESS_VIOLATION));                      // the null page
  CHECK(fails_with(0xFFFF'8000'0000'0000ULL, dst, EXCEPTION_ACCESS_VIOLATION));  // the kernel's half
  Pages no_access(1, 1);
  no_access.protect(0, PAGE_NOACCESS);
  CHECK(fails_with(no_access.address(0), dst, EXCEPTION_ACCESS_VIOLATION));
  std::uint64_t released = 0;
  {
    Pages gone(1, 1);
    released = gone.address(0);
  }
  CHECK(fails_with(released, dst, EXCEPTION_ACCESS_VIOLATION));
}

TEST_CASE("a copy across the end of a committed region fails, and keeps none of the bytes it could read") {
  Pages pages(2, 1);
  std::vector<std::byte> dst(64);
  CHECK(fails_with(pages.address(pages.page() - 32), dst, EXCEPTION_ACCESS_VIOLATION));
}

TEST_CASE("faults repeated many times leave nothing behind") {
  Pages pages(3, 2);  // pages 0 and 1 committed, page 2 only reserved
  std::vector<std::byte> dst(64);
  int wrong = 0;
  for (int i = 0; i < 2000; ++i) {
    pages.protect(1, PAGE_READWRITE | PAGE_GUARD);
    wrong += fails_with(pages.address(pages.page()), dst, EXCEPTION_GUARD_PAGE) ? 0 : 1;
    wrong += fails_with(pages.address(2 * pages.page()), dst, EXCEPTION_ACCESS_VIOLATION) ? 0 : 1;
    wrong += fails_with(pages.address(2 * pages.page() - 32), dst, EXCEPTION_ACCESS_VIOLATION) ? 0 : 1;
    wrong += arrives(pages, 16, dst) ? 0 : 1;  // no answer carries a fault of the copy before
  }
  CHECK(wrong == 0);
  CHECK_THROWS_AS(throw std::runtime_error("invented"), std::runtime_error);  // C++ exceptions work as before
}

TEST_CASE("faults on several threads at once leave nothing behind") {
  Pages pages(2, 1);
  std::atomic<int> wrong{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&pages, &wrong] {
      std::vector<std::byte> dst(48);
      for (int i = 0; i < 1000; ++i) {
        if (!fails_with(pages.address(pages.page() - 16), dst, EXCEPTION_ACCESS_VIOLATION)) ++wrong;
        if (!arrives(pages, 16, dst)) ++wrong;
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  CHECK(wrong.load() == 0);
}
