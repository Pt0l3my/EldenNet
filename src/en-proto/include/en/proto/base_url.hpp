#pragma once
// A provider's base URL as a player types it on AI › Providers' form (PAGES §4.6.2; ARCH §11.1), checked before
// EldenNet writes it to providers.toml: portable, shared by EldenNet's space (its one entry point) and the DLL.
//
// The rules, each from ARCH §11 and the client as built (en-llm's parse_config):
// - the scheme is `https://`, or `http://` for a server on this PC alone (a loopback host: `localhost`, `127.x.x.x`,
//   `[::1]`), so that a key never crosses a network in clear text (parse_config refuses a cloud provider's http://);
// - the address holds no name or password (`user:pass@host`): a key is never written inline (ARCH §11.1's key_ref),
//   and the base URL is shown on AI › Providers (PV2);
// - a host of letters, digits, '-' and '.', or a bracketed IPv6 address, with an optional port of 1 to 65535;
// - a path of plain characters (letters, digits and `-._~/%`), with no query or fragment, since the client appends
//   its own paths (`/chat/completions`, `/models`);
// - at most 200 bytes (GraceUI's form field, D195); spaces round it and a trailing '/' are taken off.
// A server on another PC of the home network over http:// is refused here: whether to allow private addresses is the
// owner's to decide (the slice's notes), and parse_config's own rule for a player's file is unchanged.
#include <cstdint>
#include <string>
#include <string_view>

namespace en::proto {

enum class UrlProblem : std::uint8_t {
  None,
  Empty,
  TooLong,
  Scheme,           // neither https:// nor http://
  Credentials,      // a name or password before the host
  Host,             // no host, or one that is not a name or an address
  Port,             // a port that is not 1 to 65535
  Characters,       // a character a base URL's path does not hold
  QueryOrFragment,  // '?' or '#'
  PlainRemote,      // http:// to a host that is not this PC
};

struct BaseUrl {
  UrlProblem problem = UrlProblem::Empty;
  std::string url;        // when problem is None: as written to providers.toml (trimmed, no trailing '/')
  bool loopback = false;  // the host is this PC's
  bool https = false;
};

inline constexpr std::size_t kBaseUrlMax = 200;

[[nodiscard]] BaseUrl check_base_url(std::string_view typed);

// Whether a host (as a URL writes it, lower-cased, an IPv6 address in brackets) is this PC's: `localhost`, an address
// of 127.0.0.0/8 or `[::1]`.
[[nodiscard]] bool is_loopback_host(std::string_view host) noexcept;

// A URL's host as is_loopback_host takes it: after the scheme and any name or password, before the port and the path,
// lower-cased; an IPv6 address with its brackets.
[[nodiscard]] std::string host_of_url(std::string_view url);

// Whether a provider's base URL (as providers.toml has it) is http:// to a host that is not this PC's: a key must never
// go to it, since it would cross a network in clear text.
[[nodiscard]] bool plain_remote(std::string_view url);

}  // namespace en::proto
