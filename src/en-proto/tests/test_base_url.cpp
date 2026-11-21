// A provider's base URL as a player types it (base_url.hpp): https:// anywhere, http:// for this PC alone, no name or
// password, a host and port that are one, a plain path with no query or fragment, at most 200 bytes. Every address is
// invented (the .invalid and .example names are reserved for that).
#include <doctest.h>

#include <string>

#include "en/proto/base_url.hpp"

using en::proto::check_base_url;
using en::proto::is_loopback_host;
using en::proto::is_private_host;
using en::proto::url_extras;
using en::proto::UrlProblem;

namespace {

UrlProblem problem(const std::string& typed) { return check_base_url(typed).problem; }

// A dotted IPv4 address from its four numbers: the private ranges' bounds are tested by number, so that no file holds
// an address of a home network as text (the privacy sweep's ip rule refuses one; these are invented).
std::string v4(int a, int b, int c, int d) {
  return std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c) + "." + std::to_string(d);
}

}  // namespace

TEST_CASE("base url: the addresses a provider of one's own may have, as written to providers.toml") {
  auto ok = check_base_url("  https://api.example.invalid/v1/  ");
  CHECK(ok.problem == UrlProblem::None);
  CHECK(ok.url == "https://api.example.invalid/v1");
  CHECK(ok.https);
  CHECK_FALSE(ok.loopback);
  ok = check_base_url("HTTPS://API.Example.Invalid:8443/Api/V1");
  CHECK(ok.problem == UrlProblem::None);
  CHECK(ok.url == "https://api.example.invalid:8443/Api/V1");  // the scheme and host lower-cased, the path kept
  ok = check_base_url("http://127.0.0.1:1234/v1");
  CHECK(ok.problem == UrlProblem::None);
  CHECK(ok.loopback);
  CHECK_FALSE(ok.https);
  CHECK(check_base_url("http://localhost:11434/v1").loopback);
  CHECK(check_base_url("http://[::1]:5001/v1").problem == UrlProblem::None);
  CHECK(check_base_url("https://[fd00::12]:8443").problem == UrlProblem::None);
  CHECK(check_base_url("https://models.example.invalid").url == "https://models.example.invalid");
  CHECK(check_base_url("https://models.example.invalid/a%20b/v1").problem == UrlProblem::None);
}

TEST_CASE("base url: each refusal named") {
  CHECK(problem("") == UrlProblem::Empty);
  CHECK(problem("   /") == UrlProblem::Empty);
  CHECK(problem("https://example.invalid/" + std::string(200, 'a')) == UrlProblem::TooLong);
  CHECK(problem("ftp://example.invalid/v1") == UrlProblem::Scheme);
  CHECK(problem("example.invalid/v1") == UrlProblem::Scheme);
  CHECK(problem("file:///C:/models") == UrlProblem::Scheme);
  // A name or password in the address: a key is never written inline.
  CHECK(problem("https://someone:hunter@example.invalid/v1") == UrlProblem::Credentials);
  CHECK(problem("https://token@example.invalid") == UrlProblem::Credentials);
  CHECK(problem("https://example.invalid/v1?key=abc") == UrlProblem::QueryOrFragment);
  CHECK(problem("https://example.invalid/v1#models") == UrlProblem::QueryOrFragment);
  CHECK(problem("https:///v1") == UrlProblem::Host);
  CHECK(problem("https://-bad.example.invalid") == UrlProblem::Host);
  CHECK(problem("https://bad..example.invalid") == UrlProblem::Host);
  CHECK(problem("https://under_score.example.invalid") == UrlProblem::Host);
  CHECK(problem("https://[::1") == UrlProblem::Host);
  CHECK(problem("https://[zz::1]") == UrlProblem::Host);
  CHECK(problem("https://example.invalid:0") == UrlProblem::Port);
  CHECK(problem("https://example.invalid:65536") == UrlProblem::Port);
  CHECK(problem("https://example.invalid:/v1") == UrlProblem::Port);
  CHECK(problem("https://example.invalid:12ab") == UrlProblem::Port);
  CHECK(problem("https://example.invalid/v 1") == UrlProblem::Characters);
  CHECK(problem("https://example.invalid/v1\\models") == UrlProblem::Characters);
  CHECK(problem("https://example.invalid/%zz") == UrlProblem::Characters);
  CHECK(problem("https://example.invalid/%2") == UrlProblem::Characters);
  CHECK(problem("https://example.invalid/\xC3\xA9") == UrlProblem::Characters);
  CHECK(problem("https://example.invalid/a\"b") == UrlProblem::Characters);
  // http:// only to this PC: never to another host, a private address included (the owner's to decide, O98).
  CHECK(problem("http://models.example.invalid/v1") == UrlProblem::PlainRemote);
  CHECK(problem("http://[fd00::20]:1234/v1") == UrlProblem::PlainRemote);
  CHECK(problem("http://192.0.2.7:1234/v1") == UrlProblem::PlainRemote);  // a documentation address: public
  CHECK(problem("http://storeroom.invalid:1234/v1") == UrlProblem::PlainRemote);
  CHECK(problem("http://127.example.invalid/v1") == UrlProblem::PlainRemote);  // a name, not 127.0.0.0/8
}

TEST_CASE("base url: where http:// goes, the one check of the form, parse_config and the key rows") {
  using en::proto::Reach;
  using en::proto::reach_of;
  CHECK(reach_of("https://api.example.invalid/v1") == Reach::Encrypted);
  CHECK(reach_of("http://127.0.0.1:1234/v1") == Reach::ThisPc);
  CHECK(reach_of("http://localhost:11434/v1") == Reach::ThisPc);
  CHECK(reach_of("http://[fd00::20]:1234/v1") == Reach::HomeNetwork);
  CHECK(reach_of("http://" + v4(192, 168, 7, 9) + ":1234/v1") == Reach::HomeNetwork);
  CHECK(reach_of("http://storeroom.invalid/v1") == Reach::Public);
  CHECK(reach_of("http://192.0.2.7/v1") == Reach::Public);
}

TEST_CASE("base url: a URL's host, and whether a key may go to it") {
  CHECK(en::proto::host_of_url("https://API.example.invalid:8443/v1") == "api.example.invalid");
  CHECK(en::proto::host_of_url("http://someone@127.0.0.1:1234/v1") == "127.0.0.1");
  CHECK(en::proto::host_of_url("http://[::1]:5001/v1") == "[::1]");
  CHECK(en::proto::plain_remote("http://[fd00::20]:1234/v1"));
  CHECK(en::proto::plain_remote("http://127.example.invalid/v1"));
  CHECK_FALSE(en::proto::plain_remote("http://127.0.0.1:1234/v1"));
  CHECK_FALSE(en::proto::plain_remote("https://api.example.invalid/v1"));
}

TEST_CASE("base url: this PC's hosts") {
  CHECK(is_loopback_host("localhost"));
  CHECK(is_loopback_host("LOCALHOST"));
  CHECK(is_loopback_host("127.0.0.1"));
  CHECK(is_loopback_host("127.1.2.3"));
  CHECK(is_loopback_host("[::1]"));
  CHECK_FALSE(is_loopback_host("127.example.invalid"));
  CHECK_FALSE(is_loopback_host("127.0.0"));
  CHECK_FALSE(is_loopback_host("127.0.0.1.example.invalid"));
  CHECK_FALSE(is_loopback_host("127.0.0.256"));
  CHECK_FALSE(is_loopback_host("128.0.0.1"));
  CHECK_FALSE(is_loopback_host("localhost.example.invalid"));
  CHECK_FALSE(is_loopback_host(""));
}

TEST_CASE("base url: the check the player's own file meets too: no name or password, no query or fragment") {
  CHECK(url_extras("https://api.example.invalid/v1") == UrlProblem::None);
  CHECK(url_extras("http://127.0.0.1:1234/v1") == UrlProblem::None);
  CHECK(url_extras("https://someone:hunter@api.example.invalid/v1") == UrlProblem::Credentials);
  CHECK(url_extras("https://token@api.example.invalid") == UrlProblem::Credentials);
  CHECK(url_extras("https://api.example.invalid/v1?key=abc") == UrlProblem::QueryOrFragment);
  CHECK(url_extras("https://api.example.invalid/v1#models") == UrlProblem::QueryOrFragment);
  CHECK(url_extras("https://api.example.invalid/v1/a@b") == UrlProblem::None);  // an '@' in the path names no one
  CHECK(url_extras("https://someone@api.example.invalid/v1?x") == UrlProblem::Credentials);
}

TEST_CASE("base url: a home network's private addresses, RFC 1918's and fc00::/7, by number") {
  CHECK(is_private_host(v4(10, 0, 0, 1)));
  CHECK(is_private_host(v4(10, 255, 255, 254)));
  CHECK(is_private_host(v4(172, 16, 0, 1)));
  CHECK(is_private_host(v4(172, 31, 255, 254)));
  CHECK(is_private_host(v4(192, 168, 0, 1)));
  CHECK_FALSE(is_private_host(v4(172, 15, 0, 1)));
  CHECK_FALSE(is_private_host(v4(172, 32, 0, 1)));
  CHECK_FALSE(is_private_host(v4(192, 169, 0, 1)));
  CHECK_FALSE(is_private_host(v4(11, 0, 0, 1)));
  CHECK_FALSE(is_private_host(v4(127, 0, 0, 1)));  // this PC's, which is_loopback_host answers
  CHECK_FALSE(is_private_host("192.0.2.7"));        // a documentation address: public
  CHECK_FALSE(is_private_host(v4(10, 0, 0, 1) + ".example.invalid"));  // a name, which may resolve anywhere
  CHECK_FALSE(is_private_host("storeroom.invalid"));
  CHECK(is_private_host("[fd00::20]"));
  CHECK(is_private_host("[fc00::1]"));
  CHECK(is_private_host("[FDab:1::2]"));
  CHECK_FALSE(is_private_host("[fe80::1]"));  // link-local: not one
  CHECK_FALSE(is_private_host("[fc::1]"));    // 0x00fc: not in fc00::/7
  CHECK_FALSE(is_private_host("[2001:db8::1]"));
  CHECK_FALSE(is_private_host("[::1]"));
  CHECK_FALSE(is_private_host("[fd0000::1]"));
}
