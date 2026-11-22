#pragma once
// SHA-256 (FIPS 180-4), for the safety list's fingerprints (O107): our own, small, pure, in en-core, which may not link
// the launcher's copy (ARCH §2.3). Checked against NIST's published examples in the tests.
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace en::core::speech {

[[nodiscard]] std::array<std::uint8_t, 32> sha256(std::string_view data);
// Lower-case hex, 64 digits.
[[nodiscard]] std::string sha256_hex(std::string_view data);

}  // namespace en::core::speech
