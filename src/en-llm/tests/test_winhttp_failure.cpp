// The WinHTTP transport's words for a failure (winhttp_failure), on every build: the codes that Windows itself may give
// and Wine does not, in each phase of an exchange. Once the connection is made and before the request goes out, an
// https exchange is in its TLS handshake, so its failure says TLS whatever the code; a timeout before the request goes
// out is one connecting.
#include <doctest.h>

#include <cstdint>
#include <string>

#include "en/llm/transport.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

using en::llm::winhttp_failure;
using en::llm::WinHttpPhase;

namespace {

// winhttp.h's numbers for the codes and flags these tests give (checked against it on Windows).
constexpr std::uint32_t kTimeout = 12002;
constexpr std::uint32_t kCannotConnect = 12029;
constexpr std::uint32_t kConnectionError = 12030;
constexpr std::uint32_t kSecureInvalidCa = 12045;
constexpr std::uint32_t kInvalidServerResponse = 12152;
constexpr std::uint32_t kSecureFailure = 12175;
constexpr std::uint32_t kFlagInvalidCa = 0x08;
constexpr std::uint32_t kFlagCertCnInvalid = 0x10;
#ifdef _WIN32
static_assert(kTimeout == ERROR_WINHTTP_TIMEOUT);
static_assert(kCannotConnect == ERROR_WINHTTP_CANNOT_CONNECT);
static_assert(kConnectionError == ERROR_WINHTTP_CONNECTION_ERROR);
static_assert(kSecureInvalidCa == ERROR_WINHTTP_SECURE_INVALID_CA);
static_assert(kInvalidServerResponse == ERROR_WINHTTP_INVALID_SERVER_RESPONSE);
static_assert(kSecureFailure == ERROR_WINHTTP_SECURE_FAILURE);
static_assert(kFlagInvalidCa == WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA);
static_assert(kFlagCertCnInvalid == WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID);
#endif

constexpr WinHttpPhase kConnecting = WinHttpPhase::Connecting;
constexpr WinHttpPhase kConnected = WinHttpPhase::Connected;
constexpr WinHttpPhase kSending = WinHttpPhase::Sending;

constexpr const char* kHandshake = "TLS failed: the handshake did not complete";

struct Case {
  std::uint32_t error;
  WinHttpPhase phase;
  bool https;
  std::uint32_t flags;
  const char* words;
};

const char* phase_name(WinHttpPhase phase) {
  switch (phase) {
    case WinHttpPhase::Connecting:
      return "connecting";
    case WinHttpPhase::Connected:
      return "connected";
    case WinHttpPhase::Sending:
      return "sending";
  }
  return "?";
}

void check(const Case& c) {
  INFO("code " << c.error << ", " << phase_name(c.phase) << (c.https ? ", https" : ", http") << ", flags " << c.flags);
  CHECK(winhttp_failure(c.error, c.phase, c.https, c.flags) == c.words);
}

}  // namespace

TEST_CASE("WinHTTP's words: after the connection was made and before its request goes out, https failed in TLS") {
  const Case cases[] = {
      // Whatever the code: Windows may report a server that does not speak TLS with a code that is not about TLS.
      {kConnectionError, kConnected, true, 0, kHandshake},
      {kInvalidServerResponse, kConnected, true, 0, kHandshake},
      {kSecureFailure, kConnected, true, 0, kHandshake},
      {kCannotConnect, kConnected, true, 0, kHandshake},
      {kTimeout, kConnected, true, 0, "TLS failed: the handshake timed out"},
      // A reset or a malformed reply needs a connection, so it was made, even before CONNECTED_TO_SERVER (which Wine
      // reports only after the handshake).
      {kConnectionError, kConnecting, true, 0, kHandshake},
      {kInvalidServerResponse, kConnecting, true, 0, kHandshake},
      // What a SECURE_FAILURE notification said, or a TLS code of its own, says more.
      {kConnectionError, kConnected, true, kFlagInvalidCa, "TLS failed: the certificate is not trusted"},
      {kSecureInvalidCa, kConnected, true, 0, "TLS failed: the certificate is not trusted"},
  };
  for (const Case& c : cases) check(c);
}

TEST_CASE("WinHTTP's words: an https exchange not yet connected, or sending its request, fails as its code says") {
  const Case cases[] = {
      {kSecureFailure, kConnecting, true, 0, kHandshake},
      {kCannotConnect, kConnecting, true, 0, "could not connect"},
      {kTimeout, kConnecting, true, 0, "timed out connecting"},
      // The request going out, after the handshake.
      {kConnectionError, kSending, true, 0, "the connection was reset"},
      {kInvalidServerResponse, kSending, true, 0, "the server's response was malformed"},
      {kSecureFailure, kSending, true, kFlagCertCnInvalid, "TLS failed: the certificate is for another name"},
      {kCannotConnect, kSending, true, 0, "could not connect"},
      {kTimeout, kSending, true, 0, "timed out"},
  };
  for (const Case& c : cases) check(c);
}

TEST_CASE("WinHTTP's words: plain http has no handshake, so only a TLS code says TLS, in any phase") {
  const Case cases[] = {
      {kConnectionError, kConnecting, false, 0, "the connection was reset"},
      {kConnectionError, kConnected, false, 0, "the connection was reset"},
      {kConnectionError, kSending, false, 0, "the connection was reset"},
      {kInvalidServerResponse, kConnecting, false, 0, "the server's response was malformed"},
      {kInvalidServerResponse, kConnected, false, 0, "the server's response was malformed"},
      {kInvalidServerResponse, kSending, false, 0, "the server's response was malformed"},
      {kSecureFailure, kConnecting, false, 0, kHandshake},
      {kSecureFailure, kConnected, false, 0, kHandshake},
      {kSecureFailure, kSending, false, 0, kHandshake},
      {kCannotConnect, kConnecting, false, 0, "could not connect"},
      {kCannotConnect, kConnected, false, 0, "could not connect"},
      {kCannotConnect, kSending, false, 0, "could not connect"},
      {kTimeout, kConnecting, false, 0, "timed out connecting"},
      {kTimeout, kConnected, false, 0, "timed out connecting"},
      {kTimeout, kSending, false, 0, "timed out"},
  };
  for (const Case& c : cases) check(c);
}
