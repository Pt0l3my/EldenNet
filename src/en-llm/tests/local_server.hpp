#pragma once
// A small HTTP/1.1 server for the transport tests, on POSIX sockets or Winsock, bound to 127.0.0.1 on a port the
// system picks. Every connection is served on a thread of its own, so exchanges can overlap; a handler answers each
// request and says whether the connection stays open for the next one (keep-alive).
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace en::llm::test {

struct SeenRequest {
  std::string head;    // the request line and the headers, without the blank line after them
  std::string body;    // as many bytes as Content-Length said
  std::string method;
  std::string target;
  int connection = 0;  // which accepted connection carried it: 1, 2, ...

  // A header's value (the name in any case), or "" when the request has none.
  std::string header(std::string_view name) const;
  bool has_header(std::string_view name) const;
};

// One accepted connection, as a handler sees it.
class Connection {
 public:
  virtual ~Connection() = default;
  // Sends all of `text`; false once the client has gone.
  virtual bool send(std::string_view text) = 0;
  // Waits at most `limit` for the client to close its end; true if it did.
  virtual bool closed_by_client(std::chrono::milliseconds limit) = 0;
};

class LocalServer {
 public:
  // Answers one request; returns true to keep the connection open for another.
  using Handler = std::function<bool(Connection&, const SeenRequest&)>;

  explicit LocalServer(Handler handler);
  ~LocalServer();  // waits for the handlers that are running: they must not wait for ever
  LocalServer(const LocalServer&) = delete;
  LocalServer& operator=(const LocalServer&) = delete;

  int port() const;
  std::vector<SeenRequest> seen() const;
  int connections() const;  // accepted so far

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Writes `text` to every connection as soon as it is accepted, reads nothing and closes it: what a client that
// expects another protocol (TLS) meets.
class RawServer {
 public:
  explicit RawServer(std::string text);
  ~RawServer();
  RawServer(const RawServer&) = delete;
  RawServer& operator=(const RawServer&) = delete;

  int port() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// A listening socket whose queue is full: a new connection is never answered, as Linux drops its first packet, so the
// client's connect timeout is what ends it. (Windows refuses it instead, but only after retrying for about 2 s.)
class FullBacklog {
 public:
  FullBacklog();
  ~FullBacklog();
  FullBacklog(const FullBacklog&) = delete;
  FullBacklog& operator=(const FullBacklog&) = delete;

  int port() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// A port on 127.0.0.1 that nothing listens on (bound, then closed again).
int unused_port();

// Whether this Windows program runs under Wine, whose WinHTTP is its own implementation (false elsewhere).
bool running_under_wine();

// Windows: the file a loaded module (an HMODULE) came from and the file of `file_name` in the system directory, both in
// UTF-8, and whether they are the same, compared without regard to case. Empty and false elsewhere.
struct ModuleOrigin {
  std::string loaded;
  std::string expected;
  bool same = false;
};
ModuleOrigin system_module_origin(void* module, std::string_view file_name);

// One piece of a chunked body, and the piece that ends it.
std::string chunk(std::string_view data);
inline constexpr std::string_view kLastChunk = "0\r\n\r\n";

// Where a test and a handler meet: wait_for() returns once notify() was called, or false when the time ran out.
class Signal {
 public:
  void notify();
  bool wait_for(std::chrono::milliseconds limit);
  bool notified() const;

 private:
  mutable std::mutex mutex_;
  std::condition_variable changed_;
  bool notified_ = false;
};

// Sets an environment variable for one scope, then puts back what was there.
class ScopedEnv {
 public:
  ScopedEnv(std::string name, const std::string& value);
  ~ScopedEnv();
  ScopedEnv(const ScopedEnv&) = delete;
  ScopedEnv& operator=(const ScopedEnv&) = delete;

 private:
  std::string name_;
  bool had_value_ = false;
  std::string old_value_;
};

}  // namespace en::llm::test
