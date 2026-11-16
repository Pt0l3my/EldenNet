// The WinHTTP transport's words for a failure (winhttp_failure, transport.hpp), built on every system: the tests check
// them on every build, the codes that Windows gives and Wine does not included. WinHTTP's codes and flags are written
// out here as winhttp.h numbers them, since other systems have no winhttp.h; on Windows the build checks each against
// it.
#include <cstdint>
#include <optional>
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

namespace en::llm {

namespace {

// WinHTTP's codes (ERROR_WINHTTP_...), and Windows' own for memory.
constexpr std::uint32_t kNotEnoughMemory = 8;
constexpr std::uint32_t kOutOfHandles = 12001;
constexpr std::uint32_t kTimeout = 12002;
constexpr std::uint32_t kInvalidUrl = 12005;
constexpr std::uint32_t kUnrecognizedScheme = 12006;
constexpr std::uint32_t kNameNotResolved = 12007;
constexpr std::uint32_t kOperationCancelled = 12017;
constexpr std::uint32_t kCannotConnect = 12029;
constexpr std::uint32_t kConnectionError = 12030;
constexpr std::uint32_t kSecureCertDateInvalid = 12037;
constexpr std::uint32_t kSecureCertCnInvalid = 12038;
constexpr std::uint32_t kClientAuthCertNeeded = 12044;
constexpr std::uint32_t kSecureInvalidCa = 12045;
constexpr std::uint32_t kSecureCertRevFailed = 12057;
constexpr std::uint32_t kInvalidServerResponse = 12152;
constexpr std::uint32_t kSecureChannelError = 12157;
constexpr std::uint32_t kBadAutoProxyScript = 12166;
constexpr std::uint32_t kUnableToDownloadScript = 12167;
constexpr std::uint32_t kSecureInvalidCert = 12169;
constexpr std::uint32_t kSecureCertRevoked = 12170;
constexpr std::uint32_t kSecureFailure = 12175;
constexpr std::uint32_t kUnhandledScriptType = 12176;
constexpr std::uint32_t kScriptExecutionError = 12177;
constexpr std::uint32_t kAutoProxyServiceError = 12178;
constexpr std::uint32_t kSecureCertWrongUsage = 12179;
constexpr std::uint32_t kAutodetectionFailed = 12180;
constexpr std::uint32_t kHeaderCountExceeded = 12181;
constexpr std::uint32_t kHeaderSizeOverflow = 12182;
constexpr std::uint32_t kChunkedHeaderSizeOverflow = 12183;
constexpr std::uint32_t kResponseDrainOverflow = 12184;
constexpr std::uint32_t kClientAuthCertNeededProxy = 12187;
constexpr std::uint32_t kSecureFailureProxy = 12188;

// What failed in TLS, as a SECURE_FAILURE notification says (WINHTTP_CALLBACK_STATUS_FLAG_...).
constexpr std::uint32_t kFlagCertRevFailed = 0x01;
constexpr std::uint32_t kFlagInvalidCert = 0x02;
constexpr std::uint32_t kFlagCertRevoked = 0x04;
constexpr std::uint32_t kFlagInvalidCa = 0x08;
constexpr std::uint32_t kFlagCertCnInvalid = 0x10;
constexpr std::uint32_t kFlagCertDateInvalid = 0x20;
constexpr std::uint32_t kFlagCertWrongUsage = 0x40;

#ifdef _WIN32
static_assert(kNotEnoughMemory == ERROR_NOT_ENOUGH_MEMORY);
static_assert(kOutOfHandles == ERROR_WINHTTP_OUT_OF_HANDLES);
static_assert(kTimeout == ERROR_WINHTTP_TIMEOUT);
static_assert(kInvalidUrl == ERROR_WINHTTP_INVALID_URL);
static_assert(kUnrecognizedScheme == ERROR_WINHTTP_UNRECOGNIZED_SCHEME);
static_assert(kNameNotResolved == ERROR_WINHTTP_NAME_NOT_RESOLVED);
static_assert(kOperationCancelled == ERROR_WINHTTP_OPERATION_CANCELLED);
static_assert(kCannotConnect == ERROR_WINHTTP_CANNOT_CONNECT);
static_assert(kConnectionError == ERROR_WINHTTP_CONNECTION_ERROR);
static_assert(kSecureCertDateInvalid == ERROR_WINHTTP_SECURE_CERT_DATE_INVALID);
static_assert(kSecureCertCnInvalid == ERROR_WINHTTP_SECURE_CERT_CN_INVALID);
static_assert(kClientAuthCertNeeded == ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED);
static_assert(kSecureInvalidCa == ERROR_WINHTTP_SECURE_INVALID_CA);
static_assert(kSecureCertRevFailed == ERROR_WINHTTP_SECURE_CERT_REV_FAILED);
static_assert(kInvalidServerResponse == ERROR_WINHTTP_INVALID_SERVER_RESPONSE);
static_assert(kSecureChannelError == ERROR_WINHTTP_SECURE_CHANNEL_ERROR);
static_assert(kBadAutoProxyScript == ERROR_WINHTTP_BAD_AUTO_PROXY_SCRIPT);
static_assert(kUnableToDownloadScript == ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT);
static_assert(kSecureInvalidCert == ERROR_WINHTTP_SECURE_INVALID_CERT);
static_assert(kSecureCertRevoked == ERROR_WINHTTP_SECURE_CERT_REVOKED);
static_assert(kSecureFailure == ERROR_WINHTTP_SECURE_FAILURE);
static_assert(kUnhandledScriptType == ERROR_WINHTTP_UNHANDLED_SCRIPT_TYPE);
static_assert(kScriptExecutionError == ERROR_WINHTTP_SCRIPT_EXECUTION_ERROR);
static_assert(kAutoProxyServiceError == ERROR_WINHTTP_AUTO_PROXY_SERVICE_ERROR);
static_assert(kSecureCertWrongUsage == ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE);
static_assert(kAutodetectionFailed == ERROR_WINHTTP_AUTODETECTION_FAILED);
static_assert(kHeaderCountExceeded == ERROR_WINHTTP_HEADER_COUNT_EXCEEDED);
static_assert(kHeaderSizeOverflow == ERROR_WINHTTP_HEADER_SIZE_OVERFLOW);
static_assert(kChunkedHeaderSizeOverflow == ERROR_WINHTTP_CHUNKED_ENCODING_HEADER_SIZE_OVERFLOW);
static_assert(kResponseDrainOverflow == ERROR_WINHTTP_RESPONSE_DRAIN_OVERFLOW);
static_assert(kClientAuthCertNeededProxy == ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED_PROXY);
static_assert(kSecureFailureProxy == ERROR_WINHTTP_SECURE_FAILURE_PROXY);
static_assert(kFlagCertRevFailed == WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED);
static_assert(kFlagInvalidCert == WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT);
static_assert(kFlagCertRevoked == WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED);
static_assert(kFlagInvalidCa == WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA);
static_assert(kFlagCertCnInvalid == WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID);
static_assert(kFlagCertDateInvalid == WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID);
static_assert(kFlagCertWrongUsage == WINHTTP_CALLBACK_STATUS_FLAG_CERT_WRONG_USAGE);
#endif

std::string tls_failure(std::uint32_t flags) {
  if (flags & kFlagCertCnInvalid) return "TLS failed: the certificate is for another name";
  if (flags & kFlagCertDateInvalid) return "TLS failed: the certificate has expired or is not valid yet";
  if (flags & kFlagInvalidCa) return "TLS failed: the certificate is not trusted";
  if (flags & kFlagCertRevoked) return "TLS failed: the certificate was revoked";
  if (flags & kFlagCertRevFailed) return "TLS failed: the certificate's revocation could not be checked";
  if (flags & kFlagCertWrongUsage) return "TLS failed: the certificate is not for a server";
  if (flags & kFlagInvalidCert) return "TLS failed: the certificate is not valid";
  return "TLS failed: the handshake did not complete";
}

// WinHTTP's TLS codes in their own words, in any phase; nothing for a code that is not about TLS.
std::optional<std::string> tls_code_words(std::uint32_t error, std::uint32_t secure_flags) {
  switch (error) {
    case kSecureFailure:
    case kSecureChannelError:
      return tls_failure(secure_flags);
    case kSecureCertCnInvalid:
      return tls_failure(kFlagCertCnInvalid);
    case kSecureCertDateInvalid:
      return tls_failure(kFlagCertDateInvalid);
    case kSecureInvalidCa:
      return tls_failure(kFlagInvalidCa);
    case kSecureCertRevoked:
      return tls_failure(kFlagCertRevoked);
    case kSecureCertRevFailed:
      return tls_failure(kFlagCertRevFailed);
    case kSecureCertWrongUsage:
      return tls_failure(kFlagCertWrongUsage);
    case kSecureInvalidCert:
      return tls_failure(kFlagInvalidCert);
    case kClientAuthCertNeeded:
      return "TLS failed: the server asks for a client certificate";
    case kSecureFailureProxy:
    case kClientAuthCertNeededProxy:
      return "TLS failed with the proxy";
    default:
      return std::nullopt;
  }
}

// The other codes, by class: never the address, a header or a key.
std::string code_words(std::uint32_t error) {
  switch (error) {
    case kCannotConnect:
      return "could not connect";
    case kNameNotResolved:
      return "the host name did not resolve";
    case kConnectionError:
      return "the connection was reset";
    case kInvalidServerResponse:
    case kHeaderCountExceeded:
    case kHeaderSizeOverflow:
    case kChunkedHeaderSizeOverflow:
    case kResponseDrainOverflow:
      return "the server's response was malformed";
    case kAutoProxyServiceError:
    case kAutodetectionFailed:
    case kBadAutoProxyScript:
    case kUnableToDownloadScript:
    case kScriptExecutionError:
    case kUnhandledScriptType:
      return "the system's proxy settings could not be used";
    case kInvalidUrl:
    case kUnrecognizedScheme:
      return "the address is not valid";
    case kOperationCancelled:
      return "cancelled";
    case kNotEnoughMemory:
    case kOutOfHandles:
      return "out of memory";
    default:
      return "network error (WinHTTP code " + std::to_string(error) + ")";
  }
}

}  // namespace

std::string winhttp_failure(std::uint32_t error, WinHttpPhase phase, bool https, std::uint32_t secure_flags) {
  if (std::optional<std::string> words = tls_code_words(error, secure_flags)) return *words;
  // The connection made and the request not yet going out: an https exchange is in its TLS handshake (SENDING_REQUEST
  // comes after it), so the handshake failed, whatever the code. The connection was made once CONNECTED_TO_SERVER
  // came, or if the code needs one, a reset or a malformed reply: Wine reports CONNECTED_TO_SERVER only after the
  // handshake, and Windows may too.
  const bool connection_made =
      phase == WinHttpPhase::Connected || error == kConnectionError || error == kInvalidServerResponse;
  const bool handshake = https && phase != WinHttpPhase::Sending && connection_made;
  if (error == kTimeout) {
    if (handshake) return "TLS failed: the handshake timed out";
    return phase == WinHttpPhase::Sending ? "timed out" : "timed out connecting";
  }
  if (handshake) return tls_failure(secure_flags);
  return code_words(error);
}

}  // namespace en::llm
