#pragma once
// The HTTP seam. The client drives one Transport per request: it starts exchanges (the request, and a backup beside
// it), waits for their events and cancels the one it no longer needs. libcurl implements it where EN_WITH_CURL is
// set, the system's WinHTTP on Windows; tests use a scripted fake on a manual clock, and both real transports pass
// one conformance suite against a local server.
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "en/llm/secret.hpp"

namespace en::llm {

struct HttpHeader {
  std::string name;
  std::string value;
};

struct HttpRequest {
  std::string method = "POST";
  std::string url;
  std::vector<HttpHeader> headers;                         // never secret: safe to log
  std::vector<std::pair<std::string, Secret>> secret_headers;  // Authorization and the like: only the transport reads them
  std::string body;
  std::chrono::milliseconds connect_timeout{3000};         // ARCH §11.1 (impl)
};

enum class TransportEventKind {
  Head,    // the response's status and headers
  Data,    // a piece of the body, as it arrived
  Done,    // the body ended normally
  Failed,  // the exchange broke (no connection, a reset, TLS); `error` says how, without secrets
};

struct TransportEvent {
  int exchange = 0;
  TransportEventKind kind = TransportEventKind::Data;
  int status = 0;                   // Head
  std::vector<HttpHeader> headers;  // Head; names in lower case
  std::string data;                 // Data
  std::string error;                // Failed
};

// Every exchange ends with exactly one Done or Failed, unless it is cancelled first: its Head comes before its Data,
// and its Data in the order the body arrived. A failure that the transport sees before it sends anything (an address
// it cannot use, a malformed header) is a Failed event too, never an exception.
class Transport {
 public:
  virtual ~Transport() = default;
  // Sends a request; returns its exchange id (> 0). Its events arrive through wait().
  virtual int start(const HttpRequest& request) = 0;
  // Stops an exchange at once; no further events come for it.
  virtual void cancel(int exchange) = 0;
  // Waits at most `max_wait` for events of the running exchanges and appends them to `out`, returning as soon as
  // there are some.
  virtual void wait(std::chrono::milliseconds max_wait, std::vector<TransportEvent>& out) = 0;
};

class TransportFactory {
 public:
  virtual ~TransportFactory() = default;
  // One Transport per request; each is used by one thread at a time. Factories may share connections between them.
  virtual std::unique_ptr<Transport> open() = 0;
};

// The libcurl transport (EN_WITH_CURL builds): HTTP/1.1 keep-alive shared across requests, the system's TLS roots,
// a connect timeout from the request, and loopback never sent through a proxy. Null in other builds.
std::shared_ptr<TransportFactory> make_curl_transport_factory();

// The WinHTTP transport (Windows builds), with the same contract: the factory's sessions keep HTTP/1.1 connections
// alive across its transports; TLS 1.2 or 1.3 with the system's certificate checks; the request's connect timeout;
// the system's proxy for remote hosts and never one for loopback; no redirects, cookies or automatic logon. Null
// in other builds.
//
// It loads the system's own winhttp.dll by its full path in the system directory, once, and looks its functions up
// there; the program never imports it by name. A proxy winhttp.dll in the game's folder (some overlays and the EAC
// toggler ship one) would otherwise be bound in place of the system's and see every request, the key's header
// included. If that library cannot be loaded, or lacks a function the transport calls, the factory is null, as in a
// build without a transport.
std::shared_ptr<TransportFactory> make_winhttp_transport_factory();

// The winhttp.dll the WinHTTP transport uses, as a module handle (an HMODULE), loaded on first use and kept for the
// program's life; null in other builds or where it could not be loaded. For the self-test and the tests.
void* winhttp_module();

// The transport this build has: WinHTTP on Windows, libcurl elsewhere where EN_WITH_CURL is set, otherwise none
// (null).
std::shared_ptr<TransportFactory> make_default_transport_factory();

}  // namespace en::llm
