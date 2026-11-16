#pragma once
// The HTTP seam. The client drives one Transport per request: it starts exchanges (the request, and a backup beside
// it), waits for their events and cancels the one it no longer needs. libcurl implements it where EN_WITH_CURL is
// set; tests use a scripted fake on a manual clock; a WinHTTP transport is later work and fits the same interface.
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

class Transport {
 public:
  virtual ~Transport() = default;
  // Sends a request; returns its exchange id (> 0). Its events arrive through wait().
  virtual int start(const HttpRequest& request) = 0;
  // Stops an exchange at once; no further events come for it.
  virtual void cancel(int exchange) = 0;
  // Waits at most `max_wait` for events of the running exchanges and appends them to `out`.
  virtual void wait(std::chrono::milliseconds max_wait, std::vector<TransportEvent>& out) = 0;
};

class TransportFactory {
 public:
  virtual ~TransportFactory() = default;
  // One Transport per request; each is used by one thread at a time. Factories may share connections between them.
  virtual std::unique_ptr<Transport> open() = 0;
};

// The libcurl transport (EN_WITH_CURL builds): HTTP/1.1 keep-alive shared across requests, the system's TLS roots,
// a connect timeout from the request, and loopback never sent through a proxy.
std::shared_ptr<TransportFactory> make_curl_transport_factory();

// The transport this build has: libcurl where EN_WITH_CURL is set, otherwise none yet (null) until WinHTTP lands.
std::shared_ptr<TransportFactory> make_default_transport_factory();

}  // namespace en::llm
