#pragma once
// A provider's base URL as a player types it on AI › Providers' form (PAGES §4.6.2; ARCH §11.1), checked before
// EldenNet writes it to providers.toml: portable, shared by EldenNet's space (its one entry point) and the DLL.
//
// The rules, each from ARCH §11 and the client as built (en-llm's parse_config):
// - the scheme is `https://`, or `http://` for a server on this PC alone (a loopback host: `localhost`, a dotted
//   127.0.0.0/8 address, `[::1]`), so that a key never crosses a network in clear text;
// - the address holds no name or password (`user:pass@host`): a key is never written inline (ARCH §11.1's key_ref),
//   and the base URL is shown on AI › Providers (PV2);
// - a host of letters, digits, '-' and '.', or a bracketed IPv6 address, with an optional port of 1 to 65535;
// - a path of plain characters (letters, digits and `-._~/%`), with no query or fragment, since the client appends
//   its own paths (`/chat/completions`, `/models`);
// - at most 200 bytes (GraceUI's form field, D195); spaces round it and a trailing '/' are taken off.
// A server on another PC of the home network over http:// is refused here: whether players may add one from the pages
// is the owner's to decide (O98). en-llm's parse_config, for the player's own providers.toml, takes the same checks of a
// name or password and a query or fragment (`url_extras`) and of where http:// goes (`reach_of`): never to a public host
// or a name, and to a private address only with no key.
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

// Where a base URL's requests go and how: encrypted (https://), or in clear text (http://) to this PC, to a literal
// private address of the home network (RFC 1918, fc00::/7), or to any other host (a public address or a name).
enum class Reach : std::uint8_t { Encrypted, ThisPc, HomeNetwork, Public };

struct BaseUrl {
  UrlProblem problem = UrlProblem::Empty;
  std::string url;        // when problem is None: as written to providers.toml (trimmed, no trailing '/')
  bool loopback = false;  // the host is this PC's
  bool https = false;
};

inline constexpr std::size_t kBaseUrlMax = 200;

[[nodiscard]] BaseUrl check_base_url(std::string_view typed);

// The part of check_base_url that the player's own providers.toml meets too: Credentials when the address holds a name
// or password before its host, QueryOrFragment when it holds '?' or '#', else None (a key is never written inline, ARCH
// §11.1, and the client appends its own paths). The form's check and parse_config share it.
[[nodiscard]] UrlProblem url_extras(std::string_view url) noexcept;

// Whether a host (as host_of_url gives it) is a private address of a home network: a dotted IPv4 address of RFC 1918's
// ranges (10/8, 172.16/12, 192.168/16), or a bracketed IPv6 address of fc00::/7. A name is not one: it
// may resolve anywhere.
[[nodiscard]] bool is_private_host(std::string_view host) noexcept;

// Whether a host (as a URL writes it, lower-cased, an IPv6 address in brackets) is this PC's: `localhost`, an address
// of 127.0.0.0/8 or `[::1]`.
[[nodiscard]] bool is_loopback_host(std::string_view host) noexcept;

// A URL's host as is_loopback_host takes it: after the scheme and any name or password, before the port and the path,
// lower-cased; an IPv6 address with its brackets.
[[nodiscard]] std::string host_of_url(std::string_view url);

// How a provider's base URL (as providers.toml has it) is reached: the form, parse_config and the pages' key rows share
// it. A key goes only where it is Encrypted or ThisPc.
[[nodiscard]] Reach reach_of(std::string_view url);

// Whether a provider's base URL is http:// to another computer (HomeNetwork or Public): a key must never go to it,
// since it would cross a network in clear text.
[[nodiscard]] bool plain_remote(std::string_view url);

}  // namespace en::proto
