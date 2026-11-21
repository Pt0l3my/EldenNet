// A provider's base URL as a player types it (base_url.hpp).
#include "en/proto/base_url.hpp"

#include <algorithm>

namespace en::proto {

namespace {

bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
bool is_alpha(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool is_hex(char c) noexcept { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

char lower(char c) noexcept { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

std::string lowered(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = lower(c);
  return out;
}

bool starts_with_nocase(std::string_view s, std::string_view prefix) noexcept {
  if (s.size() < prefix.size()) return false;
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    if (lower(s[i]) != prefix[i]) return false;
  }
  return true;
}

// One label of a host name: 1 to 63 letters, digits and '-', neither first nor last a '-'.
bool label_ok(std::string_view label) noexcept {
  if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') return false;
  return std::all_of(label.begin(), label.end(), [](char c) { return is_alpha(c) || is_digit(c) || c == '-'; });
}

bool name_ok(std::string_view host) noexcept {
  if (host.empty() || host.size() > 253) return false;
  std::size_t at = 0;
  while (true) {
    const std::size_t dot = host.find('.', at);
    if (!label_ok(host.substr(at, dot == std::string_view::npos ? std::string_view::npos : dot - at))) return false;
    if (dot == std::string_view::npos) return true;
    at = dot + 1;
  }
}

// A dotted IPv4 address: four numbers of 0 to 255, each of one to three digits, into `out`.
bool dotted_v4(std::string_view host, int (&out)[4]) noexcept {
  int numbers = 0;
  std::size_t at = 0;
  while (at <= host.size()) {
    const std::size_t dot = host.find('.', at);
    const std::string_view part = host.substr(at, dot == std::string_view::npos ? std::string_view::npos : dot - at);
    if (numbers == 4 || part.empty() || part.size() > 3 || !std::all_of(part.begin(), part.end(), is_digit)) {
      return false;
    }
    int value = 0;
    for (const char c : part) value = value * 10 + (c - '0');
    if (value > 255) return false;
    out[numbers++] = value;
    if (dot == std::string_view::npos) break;
    at = dot + 1;
  }
  return numbers == 4;
}

bool loopback_v4(std::string_view host) noexcept {
  int n[4] = {};
  return dotted_v4(host, n) && n[0] == 127;
}

}  // namespace

bool is_private_host(std::string_view host) noexcept {
  int n[4] = {};
  if (dotted_v4(host, n)) {
    return n[0] == 10 || (n[0] == 172 && n[1] >= 16 && n[1] <= 31) || (n[0] == 192 && n[1] == 168);
  }
  // [fc00::/7]: the first group, one to four hex digits before ':', with its top seven bits 1111 110.
  if (host.size() < 4 || host.front() != '[' || host.back() != ']') return false;
  const std::string_view inside = host.substr(1, host.size() - 2);
  const std::size_t colon = inside.find(':');
  if (colon == 0 || colon == std::string_view::npos || colon > 4) return false;
  unsigned value = 0;
  for (const char c : inside.substr(0, colon)) {
    if (!is_hex(c)) return false;
    value = value * 16 + static_cast<unsigned>(is_digit(c) ? c - '0' : lower(c) - 'a' + 10);
  }
  return (value & 0xFE00u) == 0xFC00u;
}

UrlProblem url_extras(std::string_view url) noexcept {
  const std::size_t scheme = url.find("://");
  const std::string_view rest = scheme == std::string_view::npos ? url : url.substr(scheme + 3);
  if (rest.substr(0, rest.find_first_of("/?#")).find('@') != std::string_view::npos) return UrlProblem::Credentials;
  if (rest.find_first_of("?#") != std::string_view::npos) return UrlProblem::QueryOrFragment;
  return UrlProblem::None;
}

bool is_loopback_host(std::string_view host) noexcept {
  const std::string h = lowered(host);
  return h == "localhost" || h == "[::1]" || loopback_v4(h);
}

std::string host_of_url(std::string_view url) {
  const std::size_t scheme = url.find("://");
  std::string_view rest = scheme == std::string_view::npos ? url : url.substr(scheme + 3);
  rest = rest.substr(0, rest.find_first_of("/?#"));
  if (const std::size_t at = rest.rfind('@'); at != std::string_view::npos) rest = rest.substr(at + 1);
  if (!rest.empty() && rest.front() == '[') {
    const std::size_t close = rest.find(']');
    return lowered(rest.substr(0, close == std::string_view::npos ? rest.size() : close + 1));
  }
  return lowered(rest.substr(0, rest.find(':')));
}

Reach reach_of(std::string_view url) {
  if (starts_with_nocase(url, "https://")) return Reach::Encrypted;
  const std::string host = host_of_url(url);
  if (is_loopback_host(host)) return Reach::ThisPc;
  return is_private_host(host) ? Reach::HomeNetwork : Reach::Public;
}

bool plain_remote(std::string_view url) {
  const Reach r = reach_of(url);
  return r == Reach::HomeNetwork || r == Reach::Public;
}

BaseUrl check_base_url(std::string_view typed) {
  BaseUrl out;
  auto fail = [&](UrlProblem p) {
    out.problem = p;
    out.url.clear();
    return out;
  };
  while (!typed.empty() && (typed.front() == ' ' || typed.front() == '\t')) typed.remove_prefix(1);
  while (!typed.empty() && (typed.back() == ' ' || typed.back() == '\t')) typed.remove_suffix(1);
  while (!typed.empty() && typed.back() == '/') typed.remove_suffix(1);
  if (typed.empty()) return fail(UrlProblem::Empty);
  if (typed.size() > kBaseUrlMax) return fail(UrlProblem::TooLong);
  if (std::any_of(typed.begin(), typed.end(), [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u <= 0x20 || u >= 0x7F;
      })) {
    return fail(UrlProblem::Characters);
  }
  std::string_view rest;
  if (starts_with_nocase(typed, "https://")) {
    out.https = true;
    rest = typed.substr(8);
  } else if (starts_with_nocase(typed, "http://")) {
    rest = typed.substr(7);
  } else {
    return fail(UrlProblem::Scheme);
  }
  // The check the player's own file meets too: a name or password, then a query or fragment.
  if (const UrlProblem extras = url_extras(typed); extras != UrlProblem::None) return fail(extras);
  const std::size_t slash = rest.find('/');
  const std::string_view authority = rest.substr(0, slash);
  const std::string_view path = slash == std::string_view::npos ? std::string_view() : rest.substr(slash);

  // host[:port], or [v6]:port
  std::string_view host;
  std::string_view port;
  bool has_port = false;
  if (!authority.empty() && authority.front() == '[') {
    const std::size_t close = authority.find(']');
    if (close == std::string_view::npos) return fail(UrlProblem::Host);
    const std::string_view inside = authority.substr(1, close - 1);
    if (inside.empty() || inside.find(':') == std::string_view::npos ||
        !std::all_of(inside.begin(), inside.end(), [](char c) { return is_hex(c) || c == ':' || c == '.'; })) {
      return fail(UrlProblem::Host);
    }
    host = authority.substr(0, close + 1);
    const std::string_view after = authority.substr(close + 1);
    if (!after.empty()) {
      if (after.front() != ':') return fail(UrlProblem::Host);
      has_port = true;
      port = after.substr(1);
    }
  } else {
    const std::size_t colon = authority.find(':');
    host = authority.substr(0, colon);
    if (colon != std::string_view::npos) {
      has_port = true;
      port = authority.substr(colon + 1);
    }
    if (!name_ok(host)) return fail(UrlProblem::Host);
  }
  if (has_port) {
    if (port.empty() || port.size() > 5 || !std::all_of(port.begin(), port.end(), is_digit)) {
      return fail(UrlProblem::Port);
    }
    int value = 0;
    for (const char c : port) value = value * 10 + (c - '0');
    if (value < 1 || value > 65535) return fail(UrlProblem::Port);
  }
  for (std::size_t i = 0; i < path.size(); ++i) {
    const char c = path[i];
    if (c == '%') {
      if (i + 2 >= path.size() || !is_hex(path[i + 1]) || !is_hex(path[i + 2])) return fail(UrlProblem::Characters);
      i += 2;
      continue;
    }
    if (!(is_alpha(c) || is_digit(c) || c == '-' || c == '.' || c == '_' || c == '~' || c == '/')) {
      return fail(UrlProblem::Characters);
    }
  }
  out.loopback = is_loopback_host(host);
  if (!out.https && !out.loopback) return fail(UrlProblem::PlainRemote);
  out.url = std::string(out.https ? "https://" : "http://") + lowered(host);
  if (has_port) out.url += ":" + std::string(port);
  out.url += path;
  out.problem = UrlProblem::None;
  return out;
}

}  // namespace en::proto
